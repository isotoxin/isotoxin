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
    {
        // \r\n not found... may be file was converted to \n format?
        // so
        ts::token<char> vv(verss, '\n');
        ts::str_c winverss;
        for(;vv;++vv)
            winverss.append( vv->get_trimmed() ).append(CONSTASTR("\r\n"));
        winverss.trunc_length(2); // remove last \r\n
        return winverss.get_length() == verss.l ? 0 : ver_ok(winverss.as_sptr());
    }
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

auto getss = [&](const ts::asptr &latest, const ts::asptr&t) ->ts::asptr
{
    ts::token<char> ver( latest, '\n' );
    for (;ver; ++ver)
    {
        auto s = ver->get_trimmed();
        if (s.begins(t) && s.get_char(t.l) == '=')
        {
            return s.substr(t.l + 1).get_trimmed().as_sptr();
        }
    }
    return ts::asptr();
};

bool md5ok(ts::buf_c &b, const ts::asptr &latest)
{
    ts::str_c md5s = getss(latest, CONSTASTR("md5"));
    if (md5s.get_length() != 32) return false;
    if (ts::pstr_c(getss(latest, CONSTASTR("size"))).as_int() != b.size()) return false;
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
        ts::asptr latest(pak->cstr().s, signi);
        ts::wstr_c wurl(from_utf8( getss(latest,CONSTASTR("url")) ));
        pak->load_from_disk_file(ts::fn_join(auparams().lock_read()().path, ts::fn_get_name_with_ext(wurl)));
        if (md5ok(*pak,latest))
            return getss(latest,CONSTASTR("ver"));
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
    ts::tmpalloc_c tmpalloc;

    if (auparams().lock_read()().in_updater)
        return;

    ts::str_c address("https://github.com/Rotkaermota/Isotoxin/wiki/latest");

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

    // extract info from github wiki
    int begin_wiki = ts::pstr_c(d.cstr()).find_pos(CONSTASTR("[begin]"));
    if ( begin_wiki > 0 )
    {
        int end_wiki = ts::pstr_c(d.cstr()).find_pos(begin_wiki + 7,CONSTASTR("[end]"));
        if (end_wiki > 0)
        {
            ts::str_c preop( ts::pstr_c(d.cstr()).substr(begin_wiki + 7, end_wiki) );
            preop.replace_all(CONSTASTR("\r"), CONSTASTR(""));
            preop.replace_all(CONSTASTR("\n"), CONSTASTR(""));
            preop.replace_all(CONSTASTR("<br><code>"), CONSTASTR(""));
            preop.replace_all(CONSTASTR("</code><br>"), CONSTASTR("\n"));
            preop.replace_all(CONSTASTR("</code>"), CONSTASTR("\n"));
            ts::str_c newver;
            for(ts::token<char> t(preop, '\n');t;++t)
                newver.append(*t).append(CONSTASTR("\r\n"));
            if (newver.get_length())
            {
                d.set_size(newver.get_length() - 2); // -2 - do not copy last \r\n
                memcpy(d.data(), newver.cstr(), newver.get_length() - 2);
            }
        }
    }

    int signi = ver_ok( d.cstr() );
    if (!signi) 
    {
        curl.newver.clear();
        curl.send_newver = true;
        return;
    }

    ts::str_c latests( d.cstr().s, signi );

    r = auparams().lock_read();
    if (!new_version( r().ver, getss(latests, CONSTASTR("ver")) ))
    {
        curl.newver.clear();
        curl.send_newver = true;
        return;
    }

    bool downloaded = false;
    ts::str_c dver = get_downloaded_ver();
    ts::str_c aver = getss(latests, CONSTASTR("ver"));
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

    ts::str_c aurl( getss(latests, CONSTASTR("url")) );
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

    if (!md5ok(d, latests))
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