#include "isotoxin.h"
#include "curl/include/curl/curl.h"
#pragma warning (disable:4324)
#include "libsodium/src/libsodium/include/sodium.h"

extern ts::static_setup<spinlock::syncvar<autoupdate_params_s>,1000> auparams;

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    return size * nitems;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ts::buf_c *resultad = (ts::buf_c *)userdata;
    resultad->append_buf(ptr, size * nmemb);
    return size * nmemb;
}

int ver_ok( ts::asptr verss )
{
    ts::pstr_c ss(verss);
    int signi = ss.find_pos(CONSTASTR("\r\nsign="));
    if (signi < 0)
        return 0;
    if ((ss.get_length() - signi - 7) != crypto_sign_BYTES * 2)
        return 0;

    byte sig[crypto_sign_BYTES];
    byte pk[crypto_sign_PUBLICKEYBYTES] = {
#include "signpk.inl"
    };
    ss.hex2buf<crypto_sign_BYTES>(sig, signi + 7);

    if  (0 == crypto_sign_verify_detached(sig, (const byte *)verss.s, signi, pk))
        return signi;

    return 0;
}

bool md5ok(ts::buf_c &b, const ts::abp_c &ver)
{
    ts::str_c md5s = ver.get_string(CONSTASTR("md5"));
    if (md5s.get_length() != 32) return false;
    if (ver.get_int(CONSTASTR("size")) != b.size()) return false;
    ts::md5_c md5;
    md5.update(b.data(), b.size()); md5.done();
    for (int i = 0; i < 16; ++i)
        if (md5.result()[i] != md5s.as_byte_hex(i * 2))
            return false;
    return true;
}

bool find_config(ts::wstr_c &path);
ts::str_c get_downloaded_ver( ts::buf_c *pak = nullptr )
{
    if ( auparams().lock_read()().path.is_empty() )
    {
        auto w = auparams().lock_write();
        ts::wstr_c cfgpath = cfg().get_path();
        if (cfgpath.is_empty()) find_config(cfgpath);
        if (cfgpath.is_empty()) return ts::str_c();
        w().path.setcopy(ts::fn_join(ts::fn_get_path(cfgpath), CONSTWSTR("update\\")));
    }

    ts::buf_c bbb;
    if (pak == nullptr) pak = &bbb;
    pak->load_from_disk_file(ts::fn_join( auparams().lock_read()().path, CONSTWSTR("latest.txt") ));
    if (pak->size() > 0)
    {
        int signi = ver_ok(pak->cstr());
        if (!signi) return ts::str_c();
        ts::abp_c ver; ver.load(ts::asptr(pak->cstr().s, signi));
        ts::wstr_c wurl(from_utf8( ver.get_string(CONSTASTR("url")) ));
        pak->load_from_disk_file(ts::fn_join(auparams().lock_read()().path, ts::fn_get_name_with_ext(wurl)));
        if (md5ok(*pak,ver))
            return ver.get_string(CONSTASTR("ver"));
    }
    return ts::str_c();
}

namespace
{
    struct myprogress_s
    {
        double lastruntime = 0;
        CURL *curl;
        myprogress_s(CURL *c):curl(c) {}
    };
    static int xferinfo(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
    {
        myprogress_s *myp = (myprogress_s *)p;
        CURL *curl = myp->curl;
        double curtime = 0;
 
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curtime);
 
        if((curtime - myp->lastruntime) >= 1)
        {
            // every 1000 ms
            myp->lastruntime = curtime;
            TSNEW(gmsg<ISOGM_DOWNLOADPROGRESS>, (int)dlnow, (int)dltotal)->send_to_main_thread();
        }

        return 0;
    }
}

void autoupdater()
{
    if (auparams().lock_read()().in_updater)
        return;

    ts::str_c address("http://95.215.46.114/latest.txt");
    //ts::str_c address("http://2ip.ru");

    struct curl_s
    {
        CURL *curl;
        ts::str_c newver;
        bool send_newver = false;
        curl_s()
        {
            curl = curl_easy_init();
            auto w = auparams().lock_write();
            w().in_progress = true;
            w().in_updater = true;
        }
        ~curl_s()
        {
            if (curl) curl_easy_cleanup(curl);
            auto w = auparams().lock_write();
            w().in_progress = false;
            w().in_updater = false;
            w.unlock();

            if (send_newver)
                TSNEW(gmsg<ISOGM_NEWVERSION>, newver)->send_to_main_thread();
        }
        operator CURL *() {return curl;}
    } curl;

    if (!curl) return;
    ts::buf_c d;
    int rslt = 0;
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
    rslt = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    rslt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

#ifdef _DEBUG
    auparams().lock_write()().proxy_addr = CONSTASTR("srv:9050");
    auparams().lock_write()().proxy_type = 3;
#endif

    auto r = auparams().lock_read();
    if (r().proxy_type > 0)
    {
        
        ts::token<char> t(r().proxy_addr, ':');
        ts::str_c proxya = *t;
        ++t;
        ts::str_c proxyp = *t;

        int pt = 0;
        if (r().proxy_type == 1) pt = CURLPROXY_HTTP;
        else if (r().proxy_type == 2) pt = CURLPROXY_SOCKS4;
        else if (r().proxy_type == 3) pt = CURLPROXY_SOCKS5_HOSTNAME;

        rslt = curl_easy_setopt(curl, CURLOPT_PROXY, proxya.cstr());
        rslt = curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxyp.as_int());
        rslt = curl_easy_setopt(curl, CURLOPT_PROXYTYPE, pt);
    }
    r.unlock();

    rslt = curl_easy_setopt(curl, CURLOPT_URL, address.cstr());
    rslt = curl_easy_perform(curl);

    int signi = ver_ok( d.cstr() );
    if (!signi) 
    {
        curl.newver.clear();
        curl.send_newver = true;
        return;
    }

    ts::abp_c ver; ver.load( ts::asptr(d.cstr().s, signi) );

    r = auparams().lock_read();
    if (!new_version( r().ver, ver.get_string(CONSTASTR("ver")) ))
    {
        curl.newver.clear();
        curl.send_newver = true;
        return;
    }

    bool downloaded = false;
    ts::str_c dver = get_downloaded_ver();
    ts::str_c aver = ver.get_string(CONSTASTR("ver"));
    if (dver == aver)
        downloaded = true;

    if (downloaded || r().autoupdate == AUB_ONLY_CHECK)
    {
        r.unlock();
        if (downloaded)
        {
            auparams().lock_write()().downloaded = true;
        }

        // just notify
        curl.newver = aver;
        curl.send_newver = true;
        return;
    }
    r.unlock();

    ts::str_c aurl( ver.get_string(CONSTASTR("url")) );
    ts::wstr_c pakname = ts::fn_get_name_with_ext(from_utf8(aurl));
    if (aurl.get_char(0) == '/')
    {
        address.set_length(address.find_pos(7, '/')).append(aurl).trim();
    } else
    {
        address = aurl;
    }

    ts::buf_c latest(d);
    d.clear();
    rslt = curl_easy_setopt(curl, CURLOPT_URL, address.cstr());
    myprogress_s progress(curl);
    rslt = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
    rslt = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    rslt = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    
    rslt = curl_easy_perform(curl);

    TSNEW(gmsg<ISOGM_DOWNLOADPROGRESS>, (int)d.size(), (int)d.size())->send_to_main_thread();

    if (!md5ok(d, ver))
    {
        curl.newver.clear();
        curl.send_newver = true;
        return;
    }

    ts::make_path( auparams().lock_read()().path, 0 );
    latest.save_to_file( ts::fn_join( auparams().lock_read()().path, CONSTWSTR("latest.txt") ) );
    d.save_to_file( ts::fn_join( auparams().lock_read()().path, pakname ) );

    auparams().lock_write()().downloaded = true;
    curl.newver = aver;
    curl.send_newver = true;
}

namespace {
struct updater
{
    bool updfail = false;
    time_t amfn = now();
    ts::wstrings_c moved;

    bool process_pak_file(const ts::arc_file_s &f)
    {
        ts::wstr_c wfn(ts::to_wstr(f.fn));
        ts::wstr_c ff(auparams().lock_read()().path); ff.append_as_num<time_t>(amfn).append_char(NATIVE_SLASH);
        ts::make_path(ff, 0);
        if (MoveFileW(wfn, ts::fn_join(ff, wfn)))
        {
            moved.add(wfn);
        }
        else if (ts::is_file_exists(wfn))
        {
            updfail = true;
            // oops
            for (const ts::wstr_c &mf : moved)
            {
                DeleteFileW(mf);
                MoveFileW(ts::fn_join(ff, mf), mf);
            }
        }
        f.get().save_to_file(wfn);
        return true;
    }
};
}

bool check_autoupdate()
{
    ts::buf_c pak;
    ts::str_c dver = get_downloaded_ver(&pak);
    if (dver.is_empty()) return true;
    if (application_c::appver() == dver)
    {
        ts::wstr_c ff(auparams().lock_read()().path);
        if (dir_present(ff))
            del_dir(ff);

    } else if (new_version(application_c::appver(), dver))
    {
        updater u;

        ts::zip_open(pak.data(), pak.size(), DELEGATE(&u,process_pak_file));

        if (u.updfail)
        {
            return true; // continue run
        }
    
        ts::start_app(CONSTWSTR("isotoxin.exe"), nullptr);

        return false;
    }

    return true;
}