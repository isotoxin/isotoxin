#include "isotoxin.h"

#define _NTOS_
#undef ERROR

#ifdef _WIN32
#include "curl/include/curl/curl.h"
#endif
#ifdef _NIX
#include <curl/curl.h>
#endif // _NIX

//-V:dm:807

#define HGROUP_MEMBER ts::wsptr()

#define TEST_RECORD_LEN 5000

#define PREVIEW_HEIGHT 310
#define SETTINGS_VTAB_WIDTH 170
#define VTAB_OTS 5

#define PROTO_ICON_SIZE 32


#define DIALOG_ADVANCED_MINW 400



#define AM_TOOLS TTT("Tools", 432)
#define AM_MISC TTT("Misc", 435)
#define AM_DEBUG TTT("Debug", 395)


static menu_c list_proxy_types(int cur, MENUHANDLER mh, int av = -1)
{
    menu_c m;
    m.add(TTT("Direct connection",159), cur == 0 ? MIF_MARKED : 0, mh, CONSTASTR("0"));

    if (CF_PROXY_SUPPORT_HTTP & av)
        m.add( TTT("HTTP proxy",517), cur == 1 ? MIF_MARKED : 0, mh, CONSTASTR( "1" ) );
    if (CF_PROXY_SUPPORT_HTTPS & av)
        m.add(TTT("HTTPS proxy",160), cur == 2 ? MIF_MARKED : 0, mh, CONSTASTR("2"));
    if (CF_PROXY_SUPPORT_SOCKS4 & av)
        m.add(TTT("Socks 4 proxy",161), cur == 3 ? MIF_MARKED : 0, mh, CONSTASTR("3"));
    if (CF_PROXY_SUPPORT_SOCKS5 & av)
        m.add(TTT("Socks 5 proxy",162), cur == 4 ? MIF_MARKED : 0, mh, CONSTASTR("4"));

    return m;
}

static void check_proxy_addr(int connectiontype, RID editctl, ts::str_c &proxyaddr)
{
    gui_textfield_c &tf = HOLD(editctl).as<gui_textfield_c>();
    //if (tf.is_disabled()) return;
    tf.badvalue(connectiontype > 0 && !check_netaddr(proxyaddr));
}

static bool __kbd_chop(RID, GUIPARAM)
{
    return true;
}

// work with curl
void set_common_curl_options(CURL *curl);
size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata);
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
void set_proxy_curl(CURL *curl, int proxy_type, const ts::asptr &proxy_addr);
int curl_execute_download(CURL *curl, int id);

namespace
{
    class dialog_dictionaries_c;
    class dialog_advanced_c;

}

    template<> struct MAKE_ROOT<dialog_dictionaries_c> : public _PROOT(dialog_dictionaries_c)
    {
        dialog_settings_c *setts;
        ts::wstrings_c dar;
        MAKE_ROOT(bool mainparent, dialog_settings_c *setts, const ts::wstrings_c &dar) : _PROOT(dialog_dictionaries_c)(), setts(setts), dar(dar) { init( rect_sys_e( RS_NORMAL | (mainparent ? RS_MAINPARENT : 0) ) ); }
        ~MAKE_ROOT() {}
    };

    template<> struct MAKE_ROOT<dialog_advanced_c> : public _PROOT(dialog_advanced_c)
    {
        dialog_setup_network_c *setts;
        MAKE_ROOT(bool mainparent, dialog_setup_network_c *setts) : _PROOT(dialog_advanced_c)(), setts(setts) { init(rect_sys_e(RS_NORMAL | (mainparent ? RS_MAINPARENT : 0))); }
        ~MAKE_ROOT() {}
    };

namespace
{
    struct enum_video_devices_s : public ts::task_c
    {
        vsb_list_t video_devices;
        ts::safe_ptr<dialog_settings_c> dlg;

        enum_video_devices_s( dialog_settings_c *dlg ):dlg(dlg) {}

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            enum_video_capture_devices(video_devices, true);
            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && dlg)
                dlg->set_video_devices( std::move(video_devices) );

            ts::task_c::done(canceled);
        }

    };

    struct load_spelling_list_s : public ts::task_c
    {
        ts::safe_ptr<dialog_dictionaries_c> dlg;
        ts::astrings_c dicts;
        bool failed = false;

        int proxy_type = 0;
        ts::str_c proxy_addr;

        load_spelling_list_s(dialog_dictionaries_c *dlg) :dlg(dlg)
        {
            proxy_type = cfg().proxy();
            proxy_addr = cfg().proxy_addr();
        }

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            ts::buf_c d;
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                failed = true;
                return R_DONE;
            }

            int rslt = 0;
            rslt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
            rslt = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            rslt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            set_common_curl_options(curl);
            set_proxy_curl( curl, proxy_type, proxy_addr );

            rslt = curl_easy_setopt(curl, CURLOPT_URL, HOME_SITE "/spelling/dictindex.txt");
            rslt = curl_easy_perform(curl);
            if (CURLE_OK != rslt)
                failed = true;
            else
                for (ts::token<char> lines(d.cstr(), '\n'); lines; ++lines)
                {
                    ts::pstr_c ss = lines->get_trimmed();
                    if (ss.find_pos('=') != ss.get_length() - 33)
                    {
                        failed = true;
                        break;
                    }
                    dicts.add(ss);
                }

            curl_easy_cleanup(curl);

            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override;
    };
    struct download_dictionary_s : public ts::task_c
    {
        ts::safe_ptr<dialog_dictionaries_c> dlg;

        int id;
        int proxy_type = 0;
        ts::str_c proxy_addr;
        ts::str_c url;
        ts::wstr_c path;
        ts::wstr_c fname;

        bool failed = false;

        download_dictionary_s(dialog_dictionaries_c *dlg, const ts::str_c &utf8name, int id) :dlg(dlg), url( CONSTASTR( HOME_SITE "/spelling/"), utf8name ), id(id), fname( from_utf8( utf8name ), CONSTWSTR( ".zip" ) )
        {
            proxy_type = cfg().proxy();
            proxy_addr = cfg().proxy_addr();
            url.append(CONSTASTR(".zip"));
            path = ts::fn_join(ts::fn_get_path(cfg().get_path()), CONSTWSTR("spelling"));
        }

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            ts::buf_c d;
            CURL *curl = curl_easy_init();
            if (!curl)
            {
                failed = true;
                return R_DONE;
            }

            int rslt = 0;
            rslt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &d);
            rslt = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
            rslt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            set_common_curl_options(curl);
            set_proxy_curl(curl, proxy_type, proxy_addr);
            rslt = curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT);
            rslt = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
            rslt = curl_easy_setopt(curl, CURLOPT_URL, url.cstr());

            rslt = curl_execute_download(curl, id);
            if (CURLE_OK != rslt)
                failed = true;
            else
            {
                ts::make_path(path, 0);

                ts::wstr_c wfn(ts::fn_join(path, fname));
                d.save_to_file(wfn);
            }
            curl_easy_cleanup(curl);

            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override;
    };
    struct load_local_spelling_list_s : public ts::task_c
    {
        ts::safe_ptr<dialog_dictionaries_c> dlg;
        ts::wstrings_c fns;
        ts::blob_c aff, dic, zip;
        ts::wstrings_c dar;
        bool stopjob = false;

        struct dict_rec_s : public ts::movable_flag<true>
        {
            DUMMY(dict_rec_s);
            dict_rec_s() {};
            ts::wstr_c name;
            ts::wstr_c path;
            ts::uint8 hash[BLAKE2B_HASH_SIZE_SMALL];
        };

        ts::array_inplace_t< dict_rec_s, 16 > dicts;

        load_local_spelling_list_s(dialog_dictionaries_c *dlg);

        bool extract_zip(const ts::arc_file_s &z)
        {
            if (ts::pstr_c(z.fn).ends(CONSTASTR(".aff")))
                aff = z.get();
            else if (ts::pstr_c(z.fn).ends(CONSTASTR(".dic")))
                dic = z.get();
            return true;
        }

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            if (stopjob) return R_DONE;
            if ((aff.size() == 0 || dic.size() == 0) && zip.size() == 0) return R_RESULT_EXCLUSIVE;

            if (zip.size())
                ts::zip_open( zip.data(), zip.size(), DELEGATE(this, extract_zip) );

            BLAKE2B( dicts.last().hash, aff.data(), aff.size(), dic.data(), dic.size() );

            return fns.size() ? R_RESULT_EXCLUSIVE : R_DONE;
        }

        void try_load_zip(ts::wstr_c &fn)
        {
            fn.set_length(fn.get_length() - 3).append(CONSTWSTR("zip"));
            zip.load_from_file(fn);
            if (zip.size())
            {
                dict_rec_s &d = dicts.add();
                d.name = ts::fn_get_name(fn);
                d.path = fn;
            }
        }

        /*virtual*/ void result() override
        {
            for (; fns.size() > 0;)
            {
                zip.clear();
                ts::wstr_c fn(fns.get_last_remove(), CONSTWSTR("aff"));

                aff.load_from_file(fn);
                if (0 == aff.size())
                {
                    try_load_zip(fn);
                    if (zip.size()) break;
                    continue;
                }

                fn.set_length(fn.get_length() - 3).append(CONSTWSTR("dic"));
                dic.load_from_file(fn);
                if (dic.size() > 0)
                {
                    dict_rec_s &d = dicts.add();
                    d.name = ts::fn_get_name(fn);
                    d.path = fn;
                    break;
                }
                aff.clear();
                try_load_zip(fn);
                if (zip.size()) break;
            }
            stopjob = (aff.size() == 0 || dic.size() == 0) && zip.size() == 0;
        }

        /*virtual*/ void done(bool canceled) override;

    };

    class dialog_dictionaries_c : public gui_isodialog_c
    {
        typedef gui_isodialog_c super;
        friend struct load_local_spelling_list_s;
        ts::wstrings_c dar;
        ts::wstrings_c dicts;
        ts::safe_ptr<dialog_settings_c> setts;
        guirect_watch_c watchdog;
        process_animation_s pa;

        bool need2rewarn = false;

        GM_RECEIVER(dialog_dictionaries_c, ISOGM_DOWNLOADPROGRESS)
        {
            for (list_item_s &li : items)
            {
                if (li.id == (uint)p.id)
                {
                    li.downprocent = p.total ? (p.downloaded * 100 / p.total) : 0;

                    if (RID lst = find("lst"))
                    {
                        rectengine_c &lste = HOLD(lst).engine();
                        ts::aint cnt = lste.children_count();
                        for (int i = 0; i < cnt; ++i)
                        {
                            rectengine_c * lie = lste.get_child(i);
                            gui_listitem_c *gli = ts::ptr_cast<gui_listitem_c *>(&lie->getrect());
                            if (gli->getparam().as_uint() == li.id)
                            {
                                gli->set_text( li.guitext() );
                                break;
                            }
                        }
                    }

                    break;
                }
            }

            return 0;
        }

        struct list_item_s : public ts::movable_flag<true>
        {
            mutable ts::bmpcore_exbody_s img;

            uint id;
            int downprocent = 0;
            ts::wstr_c name;
            ts::wstr_c path;
            ts::uint8 hash_local[BLAKE2B_HASH_SIZE_SMALL] = {};
            ts::uint8 hash_remote[BLAKE2B_HASH_SIZE_SMALL] = {};

            bool local = false;
            bool remote = false;
            bool downloading = false;
            bool active = false;

            bool is_ood() const
            {
                return local && remote && !ts::blk_cmp( hash_local, hash_remote, BLAKE2B_HASH_SIZE_SMALL );
            }
            enum st
            {
                BAD_STATUS,
                LOCAL_UPTODATE_ACTIVE,
                LOCAL_UPTODATE_INACTIVE,
                LOCAL_OUTOFDATE_ACTIVE,
                LOCAL_OUTOFDATE_INACTIVE,
                REMOTE,
                DOWNLOADING,
            };

            mutable st imgstatus = BAD_STATUS;

            st get_status() const
            {
                if (downloading) return DOWNLOADING;
                if (local) return is_ood() ? (active ? LOCAL_OUTOFDATE_ACTIVE : LOCAL_OUTOFDATE_INACTIVE) : (active ? LOCAL_UPTODATE_ACTIVE : LOCAL_UPTODATE_INACTIVE);
                return remote ? REMOTE : BAD_STATUS;
            }
            ts::wstr_c guitext() const
            {
                ts::wstr_c t( name, CONSTWSTR("<br><l>") );

                switch (get_status())
                {
                case LOCAL_UPTODATE_ACTIVE:
                    if (remote)
                        t.append(TTT("used, up to date",412));
                    else
                        t.append(TTT("used",413));
                    break;
                case LOCAL_UPTODATE_INACTIVE:
                    if (remote)
                        t.append(TTT("not used, up to date",414));
                    else
                        t.append(TTT("not used",415));
                    break;
                case LOCAL_OUTOFDATE_ACTIVE:
                    t.append(TTT("used, update available",416));
                    break;
                case LOCAL_OUTOFDATE_INACTIVE:
                    t.append(TTT("not used, update available",417));
                    break;
                case REMOTE:
                    t.append( TTT("available for download",418) );
                    break;
                case DOWNLOADING:
                    t.append(TTT("downloading...",419));
                    if (downprocent > 0)
                        t.append_as_uint(downprocent).append_char('%');
                    break;
                }

                return t;
            }
            const ts::bmpcore_exbody_s &icon() const
            {
                st t = get_status();

                if (imgstatus == t)
                    return img;

                imgstatus = t;
                switch(t)
                {
                case BAD_STATUS:
                    img = ts::bmpcore_exbody_s();
                    break;
                case REMOTE:
                case DOWNLOADING:
                    if (const theme_image_s *i = gui->theme().get_image(CONSTASTR("dict_dn")))
                        img = i->extbody();
                    break;
                case LOCAL_UPTODATE_ACTIVE:
                case LOCAL_OUTOFDATE_ACTIVE:
                    if (const theme_image_s *i = gui->theme().get_image(CONSTASTR("dict_la")))
                        img = i->extbody();
                    break;
                case LOCAL_UPTODATE_INACTIVE:
                case LOCAL_OUTOFDATE_INACTIVE:
                    if (const theme_image_s *i = gui->theme().get_image(CONSTASTR("dict_li")))
                        img = i->extbody();
                    break;
                }

                return img;
            }
        };
        ts::array_inplace_t<list_item_s,16> items;

        bool loading_list = true;


    protected:

        void updrect_loadd(const void *rr, int r, const ts::ivec2 &p)
        {
            if (1000 == r)
            {
                rectengine_root_c *root = getroot();
                root->draw(p - root->get_current_draw_offset(), pa.bmp.extbody(), true);

            }
            else
                updrect_def(rr, r, p);
        }
        /*virtual*/ ts::UPDATE_RECTANGLE getrectupdate() override { return DELEGATE(this, updrect_loadd); }

        bool updanim(RID, GUIPARAM)
        {
            if (loading_list)
            {
                pa.render();
                DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, updanim), nullptr);
            }

            if (RID upd = find(CONSTASTR("upd")))
                HOLD(upd).engine().redraw();
            return true;
        }

        /*virtual*/ void created() override
        {
            if (watchdog)
                gui->exclusive_input(getrid());
            g_app->add_task( TSNEW(load_spelling_list_s, this) );
            g_app->add_task( TSNEW(load_local_spelling_list_s, this) );

            set_theme_rect(CONSTASTR("dictlst"), false);
            super::created();
            tabsel(CONSTASTR("1"));
        }
        /*virtual*/ void getbutton(bcreate_s &bcr) override
        {
            super::getbutton(bcr);
        }
        /*virtual*/ int additions(ts::irect & ) override
        {
            descmaker dm(this);
            dm << 1;
            dm().list(TTT("List of dictionaries (right click for options)",425), loc_text( loc_loading ), -350).setname(CONSTASTR("lst"));

            dm().vspace();
            ts::wstr_c updx(CONSTWSTR("<p=c><rect=1000,32,32><p=c>"), TTT("List of spelling dictionaries is being loaded", 409));
            dm().label(updx).setname(CONSTASTR("upd"));
            updanim(RID(), nullptr);
            return 0;
        }
        bool seldict(RID, GUIPARAM p)
        {
            dar.clear();
            int xx = as_int(p);
            for (int x = 1, i = 0; x >= 0; x <<= 1, ++i)
            {
                if (i >= dicts.size()) break;
                if (0 == (x & xx))
                    dar.add( dicts.get(i) );
            }
            dar.sort(true);

            return true;
        }

        /*virtual*/ void on_confirm() override
        {
            if (setts)
                setts->set_disabled_splchklist(dar, need2rewarn);
            else if (!watchdog)
            {
                prf().disabled_spellchk( dar.join('/') );
                g_app->resetup_spelling();
            }
            need2rewarn = false;
            super::on_confirm();
        }

        bool mustdie(RID, GUIPARAM)
        {
            TSDEL(this);
            return true;
        }

        void maction(const ts::str_c &prm)
        {
            uint id = prm.as_num_part(0, 1);
            ts::aint cnt = items.size();
            for (int i = 0; i < cnt;++i)
            {
                list_item_s &li = items.get(i);
                if (li.id == id)
                {
                    switch (prm.get_char(0))
                    {
                    case 'a':
                        li.active = true;
                        dar.find_remove_slow(li.name);
                        break;
                    case 'd':
                        li.active = false;
                        dar.add(li.name);
                        dar.kill_dups_and_sort(true);
                        break;
                    case 'x':
                        dar.find_remove_slow(li.name);
                        g_app->add_task(TSNEW(download_dictionary_s, this, to_utf8(li.name), id));
                        li.downloading = true;
                        li.downprocent = 0;
                        need2rewarn = true;
                        break;
                    case 'k':
                        if (!li.path.is_empty())
                        {
                            need2rewarn = true;
                            ts::kill_file(ts::fn_change_ext(li.path, CONSTWSTR("aff")));
                            ts::kill_file(ts::fn_change_ext(li.path, CONSTWSTR("dic")));
                            ts::kill_file(ts::fn_change_ext(li.path, CONSTWSTR("zip")));
                            dar.find_remove_slow(li.name);
                            if (!li.remote)
                                items.remove_slow(i);
                            else
                                li.local = false;
                            resort();
                        }
                        break;
                    }
                    break;
                }
            }
            update_list();
        }
        menu_c mnu( const ts::str_c &prm, bool on_2click )
        {
            menu_c m;

            uint id = prm.as_uint();
            for (list_item_s &li : items)
            {
                if (li.id == id)
                {
                    if (li.downloading)
                        return m;

                    if (on_2click)
                    {
                        if (li.local)
                        {
                            if (li.active)
                                maction(CONSTASTR("d") + prm);
                            else
                                maction(CONSTASTR("a") + prm);
                        } else if (li.remote)
                        {
                            maction(CONSTASTR("x") + prm);
                        }
                        return m;
                    }

                    if (li.local)
                    {
                        if (li.active)
                            m.add(ts::wstr_c(CONSTWSTR("<b>"), TTT("Deactivate",420)), 0, DELEGATE(this, maction), CONSTASTR("d") + prm);
                        else
                            m.add(ts::wstr_c(CONSTWSTR("<b>"), TTT("Activate",421)), 0, DELEGATE(this, maction), CONSTASTR("a") + prm);
                        if (li.is_ood())
                            m.add(TTT("Update",422), 0, DELEGATE(this, maction), CONSTASTR("x") + prm);
                    } else if (li.remote )
                    {
                        m.add(ts::wstr_c(CONSTWSTR("<b>"), TTT("Download and activate",423)), 0, DELEGATE(this, maction), CONSTASTR("x") + prm);
                    }

                    if (li.local)
                    {
                        m.add(TTT("Delete",424), 0, DELEGATE(this, maction), CONSTASTR("k") + prm);
                    }

                    break;
                }
            }

            return m;
        }

        bool updatelist(RID, GUIPARAM)
        {
            if (RID lst = find("lst"))
            {
                rectengine_c &lste = HOLD(lst).engine();
                ts::aint cnt = lste.children_count();
                int chi = 0;
                for (const list_item_s &li : items)
                {
                    if (chi < cnt)
                    {
                        rectengine_c * lie = lste.get_child(chi);
                        gui_listitem_c *gli = ts::ptr_cast<gui_listitem_c *>(&lie->getrect());
                        gli->setparam(ts::amake(li.id));
                        gli->set_text(li.guitext());
                        gli->set_icon(li.icon());
                        ++chi;
                        continue;
                    }

                    MAKE_CHILD<gui_listitem_c> l(lst, li.guitext(), ts::amake(li.id)); l << li.icon() << DELEGATE(this, mnu);
                    //l.get().set_gm( DELEGATE(this, mnu), true, true );
                }

                if (chi < cnt)
                    lste.trunc_children(chi);

                gui->repos_children(ts::ptr_cast<gui_vscrollgroup_c *>(&lste.getrect()));
                if (cnt)
                    ts::ptr_cast<gui_vscrollgroup_c *>(&lste.getrect())->keep_top_visible(true);
                else
                    ts::ptr_cast<gui_vscrollgroup_c *>(&lste.getrect())->scroll_to_begin();

            }
            return true;
        }

    public:
        dialog_dictionaries_c(MAKE_ROOT<dialog_dictionaries_c> &data) :gui_isodialog_c(data), watchdog(data.setts ? data.setts->getrid() : RID(), DELEGATE(this, mustdie), nullptr ), dar(data.dar)
        {
            deftitle = title_dictionaries;
            setts = data.setts;
        }
        ~dialog_dictionaries_c()
        {
            if (gui)
            {
                gui->delete_event(DELEGATE(this, updanim));
                gui->delete_event(DELEGATE(this, updatelist));

                if (need2rewarn)
                {
                    if (setts)
                        setts->set_need2rewarn();
                    else
                        g_app->resetup_spelling();
                }
            }
        }

        /*virtual*/ int unique_tag() override { return UD_DICTIONARIES; }

        /*virtual*/ ts::ivec2 get_min_size() const override { return ts::ivec2(400, 515); }
        /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
        {
            return super::sq_evt(qp, rid, data);
        }

        void downloaded(uint id)
        {
            for (list_item_s &li : items)
            {
                if (li.id == id)
                {
                    li.downloading = false;
                    break;
                }
            }

            g_app->add_task(TSNEW(load_local_spelling_list_s, this));
        }

        void resort()
        {
            items.sort([](const list_item_s &li1, const list_item_s &li2)->bool {

                if (li1.local && !li2.local)
                    return true;
                if (!li1.local && li2.local)
                    return false;
                return ts::wstr_c::compare(li1.name, li2.name) < 0;
            });
            update_list();
        }

        uint idpool = 1;
        void set_item(const ts::wstr_c &name, const ts::wstr_c &path, const ts::uint8 *hash, bool remote, bool act )
        {
            bool upd = false;

            auto setup = [&](list_item_s &li)
            {
                if (remote)
                {
                    memcpy(li.hash_remote, hash, BLAKE2B_HASH_SIZE_SMALL );
                    li.remote = true;
                } else
                {
                    memcpy(li.hash_local, hash, BLAKE2B_HASH_SIZE_SMALL );
                    li.local = true;
                    li.active = act;
                    li.path = path;
                }
            };

            for(list_item_s &li : items)
            {
                if (li.name.equals(name))
                {
                    auto old_status = li.get_status();
                    setup( li );
                    if (old_status == li.get_status())
                        return;

                    upd = true;
                    break;
                }
            }

            if (!upd)
            {
                list_item_s &li = items.add();
                li.id = idpool++;
                li.name = name;
                setup(li);
                upd = true;
            }
            if (upd)
                resort();
        }

        void update_list()
        {
            DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, updatelist), nullptr);
        }

        void set_list( const ts::astrings_c &lst, bool failed )
        {
            loading_list = false;

            if (!failed)
            {
                ts::uint8 hash[BLAKE2B_HASH_SIZE_SMALL];
                for (const ts::str_c &ln : lst)
                {
                    ts::token<char> p(ln, '=');
                    ts::wstr_c n = ts::from_utf8(*p);
                    ++p; p->hex2buf<16>( hash );
                    set_item(n, ts::wstr_c(), hash, true, false);
                }
            }

            if (failed)
            {
                set_label_text(CONSTASTR("upd"), CONSTWSTR("<p=c>") + colorize(TTT("Unable to load list of available dictionaries. Check your internet access.",303), get_default_text_color(0)));
            } else
            {
                set_label_text(CONSTASTR("upd"), CONSTWSTR("<p=c>") + (TTT("There are $ spelling dictionaries available for download",411) / ts::wmake(lst.size())));
            }

            gui->repos_children(this);
        }
    };
    /*virtual*/ void load_spelling_list_s::done(bool canceled)
    {
        if (!canceled && dlg)
            dlg->set_list(dicts, failed);
        ts::task_c::done(canceled);
    }
    load_local_spelling_list_s::load_local_spelling_list_s(dialog_dictionaries_c *dlg) :dlg(dlg)
    {
        dar = dlg->dar;
        g_app->get_local_spelling_files(fns);
        stopjob = fns.size() == 0;
    }

    /*virtual*/ void load_local_spelling_list_s::done(bool canceled)
    {
        if (!canceled && dlg)
        {
            for(const dict_rec_s& dr : dicts)
                dlg->set_item(dr.name, dr.path, dr.hash, false, dar.find(dr.name) < 0 );
            dlg->resort();
        }
        ts::task_c::done(canceled);
    }
    /*virtual*/ void download_dictionary_s::done(bool canceled)
    {
        if (!canceled)
        {
            if (dlg)
                dlg->downloaded(id);
            else
                g_app->resetup_spelling();
        }

        ts::task_c::done(canceled);
    }

    class dialog_advanced_c : public gui_isodialog_c
    {
        typedef gui_isodialog_c super;

        guirect_watch_c watchdog;
        ts::safe_ptr<dialog_setup_network_c> setts;

        struct setitem_s
        {
            ts::str_c name;
            ts::str_c value;
            setitem_s(const ts::str_c &name) :name(name) {}
            virtual ~setitem_s() {}
        };

        struct setitem_bool_s : public setitem_s
        {
            bool val;
            setitem_bool_s(bool v, const ts::str_c &name) :setitem_s(name), val(v) {}
            bool onchange(RID, GUIPARAM p)
            {
                val = p != nullptr;
                value.set(val ? CONSTASTR("1") : CONSTASTR("0"));
                return true;
            }
        };

        struct setitem_int_s : public setitem_s
        {
            int val;
            setitem_int_s(int v, const ts::str_c &name) :setitem_s(name), val(v) {}
            bool onchange(const ts::wstr_c &s, bool ch)
            {
                val = s.as_int();
                value.set_as_int(val);
                return true;
            }
        };

        struct setitem_file_s : public setitem_s
        {
            setitem_file_s(const ts::str_c &v, const ts::str_c &name) :setitem_s(name)
            {
                value = v;
            }

            bool onchange(RID srid, GUIPARAM p)
            {
                behav_s *b = (behav_s *)p;
                if (behav_s::EVT_ON_CLICK == b->e)
                {
                    value = b->param;
                }

                return true;
            }

            menu_c menu()
            {
                menu_c mm;

                ts::wstrings_c fns;
                ts::g_fileop->find(fns, CONSTWSTR("protocols/**/*.*"), true);
                fns.sort(true);

                mm.add(TTT("no file",397));
                mm.add_separator();

                for (const ts::wstr_c &f : fns)
                {
                    menu_c m;
                    ts::wstr_c fn, path;
                    int fni = f.find_last_pos_of(CONSTWSTR("/\\"));
                    ASSERT(fni >= 9);
                    if (fni > 10)
                    {
                        path.set(f.substr(10, fni));
                        path.replace_all('\\', '/');
                        fn = f.substr(fni + 1);
                        m = mm.add_path(path);
                    }
                    else
                        m = mm, fn.set(f.substr(fni + 1));
                    if (fni == 0) fn.cut(0, 1);

                    ts::wstr_c ffn(ts::fn_join(path, fn)); ffn.replace_all('\\', '/');
                    m.add(fn, 0, nullptr, ts::to_utf8(ffn));
                }

                return mm;
            }
        };

        struct setitem_enum_s : public setitem_s
        {
            dialog_advanced_c *dlg;
            ts::str_c cn;
            ts::astrings_c values;
            int val;
            setitem_enum_s(const ts::str_c &cn, dialog_advanced_c *dlg, ts::astrings_c&&values, const ts::str_c &v, const ts::str_c &name) :setitem_s(name), dlg(dlg), cn(cn), values(std::move(values)) 
            {
                val = static_cast<int>(this->values.find(v));
                if (val < 0)
                    val = 0;
            }
            void onchange(const ts::str_c &p)
            {
                val = p.as_int();
                value = values.get(val);
                dlg->set_combik_menu(cn, menu());
            }
            menu_c menu()
            {
                menu_c m;
                int ii = 0;
                for (const ts::str_c &vv : values)
                {
                    m.add( ts::from_utf8(vv), ii == val ? MIF_MARKED : 0, DELEGATE(this, onchange), ts::amake(ii) );
                    ++ii;
                }
                return m;
            }
        };

        ts::array_del_t<setitem_s, 4> setitems;

        /*virtual*/ void created() override
        {
            if (watchdog)
                gui->exclusive_input(getrid());

            set_theme_rect(CONSTASTR("advnet"), false);
            super::created();
            tabsel(CONSTASTR("1"));
        }
        /*virtual*/ void getbutton(bcreate_s &bcr) override
        {
            super::getbutton(bcr);
        }
        /*virtual*/ int additions(ts::irect &) override
        {
            descmaker dm(this);
            dm << 1;
            //dm().list(ts::wsptr(), ts::wsptr(), -350).setname(CONSTASTR("lst"));
            //dm().vspace();

            int ctln = 1;
            int prevtype = 0;
            ts::str_c props; props.set_as_int(DIALOG_ADVANCED_MINW - 100).append(CONSTASTR(",100"));

            for (ts::token<char> t(setts->get_advanced(), '/'); t; ++t)
            {
                ts::token<char> st(*t, ':');
                ts::str_c aname = *st;
                bool nameok = true;
                for (int i = 0; i < aname.get_length(); ++i)
                {
                    if (aname.get_char(i) != '_' && (aname.get_char(i) < 'a' || aname.get_char(i) > 'z'))
                    {
                        nameok = false;
                        continue;
                    }
                }
                if (!nameok)
                    continue;

                ts::str_c value = setts->adv_param(aname);

                ts::wstr_c name = ts::from_utf8(aname);
                ts::wstr_c hint = labels.get(CONSTWSTR("hint_") + name);
                name = labels.get(name,name);

                ++st;
                if (st->equals(CONSTASTR("bool")))
                {
                    if (prevtype == 1 || prevtype == 2 || prevtype == 3)
                        dm().vspace();

                    ++st;
                    if (value.is_empty()) value.set(*st);
                    setitem_bool_s *i = TSNEW(setitem_bool_s, value.as_int() != 0, aname);
                    setitems.add(i);

                    dm().checkb(ts::wsptr(), DELEGATE(i,onchange), i->val ? 1 : 0 ).setmenu(
                        menu_c().add(name, 0, MENUHANDLER(), CONSTASTR("1"))
                    ).sethint(hint);
                    prevtype = 0;
                }
                if (st->equals(CONSTASTR("int")))
                {
                    ++st;
                    if (value.is_empty()) value.set(*st);

                    setitem_int_s *i = TSNEW(setitem_int_s, value.as_int(), aname);
                    setitems.add(i);

                    dm().hgroup(ts::wsptr(), props, DIALOG_ADVANCED_MINW);
                    dm().label(name).sethint(hint);
                    dm().textfield(ts::wsptr(),ts::from_utf8(value), DELEGATE(i, onchange)).sethint(hint);

                    prevtype = 1;
                }
                if (st->begins(CONSTASTR("enum")))
                {
                    ts::ZSTRINGS_SIGNED index = 4;
                    ts::astrings_c values(st->substr(index, '(', ')').as_sptr(), ',');
                    ++st;
                    if (value.is_empty()) value = values.get(st->as_int());
                    ts::str_c cn(CONSTASTR("ctl")); cn.append_as_int(ctln++);
                    setitem_enum_s *i = TSNEW(setitem_enum_s, cn, this, std::move(values), value, aname);
                    setitems.add(i);

                    if (prevtype == 0 || prevtype == 2 || prevtype == 3)
                        dm().vspace();

                    dm().hgroup(ts::wsptr(), props, DIALOG_ADVANCED_MINW);
                    dm().label(name).sethint(hint);
                    dm().combik(ts::wsptr()).setmenu( i->menu() ).setname(cn).sethint(hint);

                    prevtype = 2;
                }
                if (st->begins(CONSTASTR("file")))
                {
                    setitem_file_s *i = TSNEW(setitem_file_s, value, aname);
                    setitems.add(i);

                    if (prevtype == 0 || prevtype == 1 || prevtype == 2 || prevtype == 3)
                        dm().vspace();

                    dm().hgroup(ts::wsptr());
                    dm().label(name).sethint(hint);
                    dm().selector(ts::wstr_c(), ts::from_utf8(value), DELEGATE(i, onchange)).setmenu(i->menu()).sethint(hint);

                    prevtype = 3;
                }
            }

            return 0;
        }

        /*virtual*/ void on_confirm() override
        {
            for (setitem_s *s : setitems)
            {
                if (!s->value.is_empty())
                    setts->adv_param(s->name, s->value);
            }

            super::on_confirm();
        }

        bool mustdie(RID, GUIPARAM)
        {
            TSDEL(this);
            return true;
        }

        ts::wstrmap_c labels;

    public:

        dialog_advanced_c(MAKE_ROOT<dialog_advanced_c> &data) :gui_isodialog_c(data), watchdog(data.setts->getrid(), DELEGATE(this, mustdie), nullptr)
        {
            deftitle = title_advanced_con_settings;
            setts = data.setts;

            ts::wstr_c labelsfn(CONSTWSTR("loc" NATIVE_SLASH_S "en.proto.$.lng")); labelsfn.replace_all(CONSTWSTR("$"), setts->prototag());
            ts::wstrings_c ss;
            ts::parse_text_file(labelsfn,ss,true);
            labels.parse(ss);

            ts::str_c lng = cfg().language();
            if (!lng.equals(CONSTASTR("en")))
            {
                labelsfn.replace(4, 2, ts::to_wstr(lng));
                ts::parse_text_file(labelsfn, ss, true);
                labels.parse(ss);
            }
        }
        ~dialog_advanced_c()
        {
            if (gui)
            {
            }
        }

        /*virtual*/ int unique_tag() override { return UD_ADVANCED_NETWORK_SETTINGS; }

        /*virtual*/ ts::ivec2 get_min_size() const override { return ts::ivec2(DIALOG_ADVANCED_MINW, 420); }
        /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
        {
            return super::sq_evt(qp, rid, data);
        }

    };
}

bool choose_dicts_load(RID, GUIPARAM)
{
    SUMMON_DIALOG<dialog_dictionaries_c>(UD_DICTIONARIES, true, nullptr, ts::wstrings_c(prf().get_disabled_dicts(),'/'));
    return true;
}

namespace
{
    struct preset_s : public ts::movable_flag<true>
    {
        ts::wstr_c fn;
        ts::wstr_c name;
    };
}

dialog_settings_c::dialog_settings_c(initial_rect_data_s &data) :gui_isodialog_c(data), mic_test_rec_stop(ts::Time::undefined()), mic_level_refresh(ts::Time::past())
{
    deftitle = title_settings;
    shadow = gui->theme().get_rect(CONSTASTR("shadow"));

    gui->register_kbd_callback( __kbd_chop, HOTKEY_TOGGLE_SEARCH_BAR );
    gui->register_kbd_callback(__kbd_chop, HOTKEY_TOGGLE_TAGFILTER_BAR);

    s3::enum_sound_capture_devices(enum_capture_devices, this);
    s3::enum_sound_play_devices(enum_play_devices, this);
    s3::get_capture_device(&mic_device_stored);

    g_app->add_task( TSNEW( enum_video_devices_s, this ) );

    profile_selected = prf().is_loaded();

    fontsz_conv_text = 1000;
    fontsz_msg_edit = 1000;
    gui->theme().font_params(CONSTASTR("conv_text"), [&](const ts::font_params_s &fp) {
        fontsz_conv_text = ts::tmin(fontsz_conv_text, fp.size.x, fp.size.y);
    });
    gui->theme().font_params(CONSTASTR("msg_edit"), [&](const ts::font_params_s &fp) {
        fontsz_msg_edit = ts::tmin(fontsz_msg_edit, fp.size.x, fp.size.y);
    });

    ts::hashmap_t< ts::wstr_c, ts::abp_c > bps;
    ts::wstrings_c fns, ifns;
    ts::g_fileop->find(fns, CONSTWSTR("themes/*/struct.decl"), true);

    ts::tmp_array_inplace_t<preset_s, 4> tpresets;

    auto getbp = [&]( const ts::wstr_c &fn ) -> ts::abp_c &
    {
        if (auto *b = bps.find(fn))
            return b->value;
        ts::abp_c &bp = bps[ fn ];
        ts::g_fileop->load(fn, bp);
        return bp;
    };

    auto detect_presets = [&](ts::wstr_c thn)
    {
        tpresets.clear();

        ts::wstr_c path(CONSTWSTR("themes/"), thn, CONSTWSTR( "/*.decl" ) );
        ts::g_fileop->find(ifns, path, true);

        for(;;)
        {
            ts::abp_c &bp = getbp( path.set_length(7).append(thn).append(CONSTWSTR("/struct.decl")) );
            if (bp.is_empty()) break;
            ts::abp_c *par = bp.get(CONSTASTR("parent"));
            if (!par) break;
            thn = to_wstr( par->as_string() );
            path.set_length(7).append(thn).append(CONSTWSTR("/*.decl"));
            ts::g_fileop->find(ifns, path, true);
        }


        ifns.kill_dups_and_sort();
        for (const ts::wstr_c &f : ifns)
        {
            ts::wstr_c pn = ts::fn_get_name(f);
            if (pn.equals(CONSTWSTR("struct")))
                continue;

            for (const preset_s &p : tpresets)
            {
                if (p.fn.equals(pn))
                {
                    pn.clear();
                    break;
                }
            }

            if (pn.is_empty()) continue;

            ts::abp_c &pbp = getbp(f);
            if (pbp.is_empty()) continue;
            const ts::abp_c *conf = pbp.get(CONSTASTR("conf"));

            preset_s &p = tpresets.add();
            p.fn = pn;
            p.name = conf ? ts::from_utf8( conf->get_string(CONSTASTR("presetname"), ts::to_utf8(pn)) ) : pn;
        }
    };

    ts::wstr_c curthn(cfg().theme());
    if (curthn.find_pos('@') < 0)
    {
        curthn.append(CONSTWSTR("@defcolors"));
    }

    for( const ts::wstr_c &f : fns )
    {
        ts::wstr_c n = ts::fn_get_name_with_ext(ts::fn_get_path(f).trunc_length(1));
        for( const theme_info_s &thi : m_themes )
        {
            if (thi.folder.equals(n))
            {
                n.clear();
                break;
            }
        }

        detect_presets(n);

        if (!n.is_empty())
        {
            ts::abp_c &bp = getbp(f);
            if (bp.is_empty()) continue;
            const ts::abp_c *conf = bp.get(CONSTASTR("conf"));

            if (tpresets.size() == 0)
            {
                preset_s &p = tpresets.add();
                p.fn = CONSTWSTR("defcolors");
            }

            for( const preset_s &cpn : tpresets )
            {
                theme_info_s &thi = m_themes.add();
                thi.folder = n;
                thi.name.set_as_char('@').append(n);
                if (conf)
                    thi.name = to_wstr(conf->get_string(CONSTASTR("name"), ts::to_str(thi.name)));
                if (cpn.name.is_empty())
                {
                    thi.folder.append(CONSTWSTR("@defcolors"));

                } else
                {
                    thi.folder.append_char('@').append(cpn.fn);
                    thi.name.append(CONSTWSTR(" / ")).append(cpn.name);
                }
                thi.current = curthn.equals(thi.folder);
            }


        }
    }

    fns.clear();
    ts::g_fileop->find(fns, CONSTWSTR("sounds/**/*.*"), true);
    fns.sort(true);

    sounds_menu.add(TTT("no sound",301));
    sounds_menu.add_separator();

    for (const ts::wstr_c &f : fns)
    {
        menu_c m;
        ts::wstr_c fn, path;
        int fni = f.find_last_pos_of(CONSTWSTR("/\\"));
        ASSERT(fni >= 6);
        if (fni > 7)
        {
            path.set(f.substr(7, fni));
            path.replace_all('\\', '/');
            fn = f.substr(fni + 1);
            m = sounds_menu.add_path(path);
        } else
            m = sounds_menu, fn.set(f.substr(fni+1));
        if (fni == 0) fn.cut(0, 1);

        ts::wstr_c ffn(ts::fn_join(path,fn)); ffn.replace_all('\\', '/');
        if (ffn.ends_ignore_case(CONSTWSTR(".ogg")) || ffn.ends_ignore_case(CONSTWSTR(".wav")) || ffn.ends_ignore_case(CONSTWSTR(".flac")))
            m.add(fn,0,nullptr /* no handler provided here - used onclick selector handler */, ts::to_utf8( ffn ));
        else if (ffn.ends_ignore_case(CONSTWSTR(".decl")))
        {
            path = ts::fn_get_path(CONSTWSTR("/")+ffn); path.trim_left('/');
            ffn = ts::fn_join(ts::pwstr_c(CONSTWSTR("sounds")), ffn);
            sound_preset_s &spr = presets.add();
            spr.path = path;
            ts::g_fileop->load(ffn, spr.preset);
            spr.name = ts::from_utf8(spr.preset.get_string(CONSTASTR("presetname")));
            if (spr.name.is_empty()) spr.name = ffn;
        }
    }
}

dialog_settings_c::~dialog_settings_c()
{
    if (mic_device_changed)
    {
        s3::set_capture_device( &mic_device_stored );
        g_app->capture_device_changed();
    }

    if (gui)
    {
        gui->delete_event(DELEGATE(this, fileconfirm_handler));
        gui->delete_event(DELEGATE(this, msgopts_handler));
        gui->delete_event(DELEGATE(this, ctl2send_handler));
        gui->delete_event(DELEGATE(this, histopts_handler));
        gui->delete_event(DELEGATE(this, delete_used_network));
        gui->delete_event(DELEGATE(this, drawcamerapanel));
        gui->delete_event(DELEGATE(this, encrypt_handler));
        gui->delete_event(DELEGATE(this, commonopts_handler));
        gui->delete_event(DELEGATE(this, password_not_entered_to_decrypt));
        gui->delete_event(DELEGATE(this, password_not_entered));
        gui->delete_event(DELEGATE(this, save_and_close));
        gui->delete_event(DELEGATE(this, addlistsound));
        gui->delete_event(DELEGATE(this, chat_options));


        gui->unregister_kbd_callback( __kbd_chop );

        if (need2rewarn)
            g_app->resetup_spelling();
    }
}

int dialog_settings_c::detect_startopts()
{
    ts::wstr_c cmdline;
    int is_autostart = ts::master().sys_detect_autostart(cmdline);
    int asopts = 0;
    if (is_autostart & ts::DETECT_AUSTOSTART) asopts |= 1;
    if (is_autostart & ts::DETECT_READONLY) asopts |= 4;
    if (cmdline.equals(CONSTWSTR("minimize"))) asopts |= 2;
    if (0 == (is_autostart & ts::DETECT_AUSTOSTART))
        asopts |= 2; // by default enable minimize
    return asopts;
}

void dialog_settings_c::set_startopts()
{
    ts::master().sys_autostart( CONSTWSTR("Isotoxin"), 0 != (1 & startopt) ? ts::get_exe_full_name() : ts::wstr_c(), 0 != (2 & startopt) ? CONSTWSTR("minimize") : ts::wsptr() );
}

/*virtual*/ const s3::Format *dialog_settings_c::formats(ts::aint &count)
{
    capturefmt.sampleRate = 48000;
    capturefmt.channels = 1;
    capturefmt.bitsPerSample = 16;
    count = 1;
    return &capturefmt;
};

/*virtual*/ ts::wstr_c dialog_settings_c::get_name() const
{
    MEMT( MEMT_GUI_COMMON );
    return super::get_name().append(CONSTWSTR(" / ")).append(gui_dialog_c::get_name());
}

/*virtual*/ ts::ivec2 dialog_settings_c::get_min_size() const
{
    return ts::ivec2( 850, 520 );
}


/*virtual*/ void dialog_settings_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    super::created();
}

void dialog_settings_c::getbutton(bcreate_s &bcr)
{
    super::getbutton(bcr);
    if (bcr.tag == 1)
    {
        bcr.btext = TTT("Save",61);
    }

}

bool dialog_settings_c::fileconfirm_handler(RID, GUIPARAM p)
{
    fileconfirm = as_int(p);

    ctlenable(CONSTASTR("confirmauto"), fileconfirm == 0);
    ctlenable(CONSTASTR("confirmmanual"), fileconfirm == 1);

    mod();
    return true;
}
bool dialog_settings_c::fileconfirm_auto_masks_handler(const ts::wstr_c &v, bool )
{
    auto_download_masks = v;
    mod();
    return true;
}
bool dialog_settings_c::fileconfirm_manual_masks_handler(const ts::wstr_c &v, bool )
{
    manual_download_masks = v;
    mod();
    return true;
}

bool dialog_settings_c::downloadfolder_edit_handler(const ts::wstr_c &v, bool )
{
    downloadfolder = v;
    mod();
    return true;
}

bool dialog_settings_c::downloadfolder_images_edit_handler(const ts::wstr_c &v, bool )
{
    downloadfolder_images = v;
    mod();
    return true;
}

bool dialog_settings_c::downloadfolder_fshare_edit_handler(const ts::wstr_c &v, bool)
{
    downloadfolder_fshare = v;
    mod();
    return true;
}

bool dialog_settings_c::tempfolder_sendimg_edit_handler( const ts::wstr_c &v, bool )
{
    tempfolder_sendimg = v;
    mod();
    return true;
}

bool dialog_settings_c::tempfolder_handlemsg_edit_handler( const ts::wstr_c &v, bool )
{
    tempfolder_handlemsg = v;
    mod();
    return true;
}

bool dialog_settings_c::date_msg_tmpl_edit_handler(const ts::wstr_c &v, bool )
{
    date_msg_tmpl = to_utf8(v);
    mod();
    return true;
}
bool dialog_settings_c::date_sep_tmpl_edit_handler(const ts::wstr_c &v, bool )
{
    date_sep_tmpl = to_utf8(v);
    mod();
    return true;
}


bool dialog_settings_c::ctl2send_handler( RID, GUIPARAM p )
{
    ctl2send = (enter_key_options_s)as_int(p);
    if (ctl2send == EKO_ENTER_NEW_LINE && double_enter)
        ctl2send = EKO_ENTER_NEW_LINE_DOUBLE_ENTER;

    ctlenable( CONSTASTR("dblenter1"), ctl2send != EKO_ENTER_SEND );

    mod();
    return true;
}
bool dialog_settings_c::ctl2send_handler_de(RID, GUIPARAM p)
{
    if (p)
        ctl2send = EKO_ENTER_NEW_LINE_DOUBLE_ENTER;
    else
        ctl2send = EKO_ENTER_NEW_LINE;

    mod();
    return true;
}


bool dialog_settings_c::collapse_beh_handler(RID, GUIPARAM p)
{
    collapse_beh = as_int(p);
    mod();
    return true;
}

bool dialog_settings_c::autoupdate_handler( RID, GUIPARAM p)
{
    autoupdate = as_int(p);

    mod();
    return true;
}

bool dialog_settings_c::useproxy_handler(RID, GUIPARAM p)
{
    int useproxy = as_int(p);

    COPYBITS(useproxyfor, useproxy, UPF_CURRENT_MASK);

    ctlenable(CONSTASTR("proxytype"), 0 != (useproxyfor & UPF_CURRENT_MASK));
    ctlenable(CONSTASTR("proxyaddr"), 0 != (useproxyfor & UPF_CURRENT_MASK) && proxy > 0);

    mod();
    return true;

}

void dialog_settings_c::proxy_handler(const ts::str_c& p)
{
    proxy = p.as_int();
    set_combik_menu(CONSTASTR("proxytype"), list_proxy_types(proxy, DELEGATE(this, proxy_handler)));
    if (RID r = find(CONSTASTR("proxyaddr")))
    {
        check_proxy_addr(proxy, r, proxy_addr);
        r.call_enable(0 != (useproxyfor & UPF_CURRENT_MASK) && proxy > 0);
    }
    mod();
}

bool dialog_settings_c::proxy_addr_handler( const ts::wstr_c & t, bool )
{
    proxy_addr = to_str(t);
    if (RID r = find(CONSTASTR("proxyaddr")))
        check_proxy_addr(proxy, r, proxy_addr);
    mod();
    return true;
}

bool dialog_settings_c::histopts_handler(RID, GUIPARAM p)
{
    int ip = as_int(p);

    ASSERT(MSGOP_LOAD_WHOLE_HISTORY == bgroups[BGROUP_HISTORY].masks[1]);

    int &hist_opts = bgroups[BGROUP_HISTORY].current;
    if (ip < 0)
    {
        INITFLAG(hist_opts, 2, ip == -1);
        ctlenable( CONSTASTR( "loadcount" ), ip == -2 );
        ctlenable( CONSTASTR( "loadcounta" ), ip == -2 );

    } else
        hist_opts = (ip & ~2) | (hist_opts & 2);

    bgroups[BGROUP_HISTORY].flush();

    mod();
    return true;
}


bool dialog_settings_c::load_history_count_handler(const ts::wstr_c &v, bool )
{
    load_history_count = v.as_int();
    if (load_history_count < 10)
    {
        load_history_count = 10;
        set_edit_value(CONSTASTR("loadcount"), ts::wstr_c(CONSTWSTR("10")));
    }
    mod();
    return true;
}

bool dialog_settings_c::load_history_count_a_handler( const ts::wstr_c &v, bool )
{
    load_history_count_addition = v.as_int();
    if ( load_history_count_addition < 10 )
    {
        load_history_count_addition = 10;
        set_edit_value(CONSTASTR("loadcounta"), ts::wstr_c(CONSTWSTR("10")));
    }
    mod();
    return true;
}

bool dialog_settings_c::password_entered(const ts::wstr_c &passwd, const ts::str_c &)
{
    gen_passwdhash(passwhash, passwd);
    encrypted_profile_password.clear().append_as_hex(passwhash, CC_HASH_SIZE);

    mod();

    auto oldrekey = rekey;
    rekey = was_encrypted_profile ? (is_changed(encrypted_profile_password) ? REKEY_REENCRYPT : REKEY_NOTHING_TO_DO) : REKEY_ENCRYPT;
    if (oldrekey != rekey)
        getengine().redraw();

    return true;
}

bool dialog_settings_c::password_not_entered( RID, GUIPARAM p )
{
    if (p)
    {
        auto oldrekey = rekey;
        rekey = was_encrypted_profile ? REKEY_REMOVECRYPT : REKEY_NOTHING_TO_DO;
        if (oldrekey != rekey)
            getengine().redraw();
        encrypted_profile_password.clear();
        set_check_value(CONSTASTR("encrypt1"), false);
        mod();
    } else
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, password_not_entered), as_param(1) );
    return true;
}

static ts::wsptr forgot()
{
    return TTT("Forgot password? Still not a problem. Just toggle [i]Encrypted profile[/i] checkbox and enter password again.", 382);
}

bool dialog_settings_c::re_password_entered(const ts::wstr_c &passwd, const ts::str_c &)
{
    ts::uint8 passwhash_re[32];
    gen_passwdhash(passwhash_re, passwd);
    ts::str_c re_password;
    re_password.append_as_hex(passwhash_re, CC_HASH_SIZE);
    if (!re_password.equals(encrypted_profile_password))
    {
        dialog_msgbox_c::mb_warning( ts::wstr_c(TTT("You must enter same password, that was entered on checkbox set.",380),CONSTWSTR("<br>"),forgot()) ).summon(false);
        return true;
    }

    prf().encrypt(passwhash_re);

    DEFERRED_UNIQUE_CALL( 0, DELEGATE( this, save_and_close ), nullptr );
    return true;
}
bool dialog_settings_c::re_password_not_entered(RID, GUIPARAM)
{
    dialog_msgbox_c::mb_info( forgot() ).summon(false);
    return true;
}

bool dialog_settings_c::password_entered_to_decrypt(const ts::wstr_c &passwd, const ts::str_c &)
{
    ts::uint8 _hash[32];
    gen_passwdhash(_hash, passwd);
    ts::str_c shash;
    shash.append_as_hex(_hash, CC_HASH_SIZE);
    ts::str_c chash = prf().get_keyhash_str();
    if ( shash.equals(chash) )
    {
        auto oldrekey = rekey;
        rekey = REKEY_REMOVECRYPT;
        if (oldrekey != rekey)
            getengine().redraw();
        encrypted_profile_password.clear();
        mod();
    } else
    {
        dialog_msgbox_c::mb_error(TTT("Incorrect password! You have to enter correct current password to remove encryption. Sorry.", 8)).summon( false );
        password_not_entered_to_decrypt(RID(), nullptr);
    }

    return true;
}

bool dialog_settings_c::password_not_entered_to_decrypt(RID, GUIPARAM p)
{
    if (p)
    {
        auto oldrekey = rekey;
        rekey = REKEY_NOTHING_TO_DO;
        if (oldrekey != rekey)
            getengine().redraw();
        lite_encset = true;
        set_check_value(CONSTASTR("encrypt1"), true);
        mod();
    } else
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, password_not_entered_to_decrypt), as_param(1));

    return true;
}

bool dialog_settings_c::encrypt_handler( RID, GUIPARAM pp )
{
    bool prevep = encrypted_profile;
    int p = as_int(pp);
    encrypted_profile = 0 != (p & 1);
    if (lite_encset)
    {
        lite_encset = false;
        return true;
    }

    if (!encrypted_profile)
    {
        TSDEL(epdlg.get());

        if (was_encrypted_profile)
        {
            RID epr = SUMMON_DIALOG<dialog_entertext_c>(UD_ENTERPASSWORD, false, dialog_entertext_c::params(
                UD_ENTERPASSWORD,
                gui_isodialog_c::title(title_enter_password),
                ts::wstr_c(TTT("Please enter current password to remove encryption",31)),
                ts::wstr_c(),
                ts::str_c(),
                DELEGATE(this, password_entered_to_decrypt),
                DELEGATE(this, password_not_entered_to_decrypt),
                check_always_ok,
                getrid()));
            if (epr)
                epdlg = &HOLD(epr).engine();
        } else
        {
            auto oldrekey = rekey;
            rekey = REKEY_NOTHING_TO_DO;
            if (oldrekey != rekey)
                getengine().redraw();

            encrypted_profile_password.clear();

        }

    } else if (!prevep)
    {
        TSDEL( epdlg.get() );
        RID epr = SUMMON_DIALOG<dialog_entertext_c>(UD_ENTERPASSWORD, false, dialog_entertext_c::params(
            UD_ENTERPASSWORD,
            gui_isodialog_c::title(title_enter_password),
            ts::wstr_c(TTT("ATTENTION! Your profile is about to be encrypted with password. The only one way to open password-encrypted profile - enter correct password. If you forget password, you will lose your profile!",383)),
            ts::wstr_c(),
            ts::str_c(),
            DELEGATE(this, password_entered),
            DELEGATE(this, password_not_entered),
            check_always_ok,
            getrid() ) );
        if (epr)
            epdlg = &HOLD(epr).engine();

    }

    mod();
    return true;
}

bool dialog_settings_c::commonopts_handler( RID, GUIPARAM p )
{
    bool is_away0 = 0 != (bgroups[BGROUP_COMMON2].current & 3);
    bgroups[BGROUP_COMMON2].handler(RID(), p);
    ctlenable(CONSTASTR("away_min"), 0 != (bgroups[BGROUP_COMMON2].current & 2) );
    ctlenable(CONSTASTR("awayflags4"), 0 != (bgroups[BGROUP_COMMON2].current & 3));
    bool is_away1 = 0 != (bgroups[BGROUP_COMMON2].current & 3);
    if (is_away0 && !is_away1 && 0 != (bgroups[BGROUP_COMMON2].current & 4))
        set_check_value(CONSTASTR("awayflags4"), false);

    set_away_on_timer_minutes_value = (bgroups[BGROUP_COMMON2].current & 2) ? set_away_on_timer_minutes_value_last : 0;
    mod();
    return true;
}

bool dialog_settings_c::away_minutes_handler(const ts::wstr_c &v, bool )
{
    set_away_on_timer_minutes_value = v.as_int();
    int o = set_away_on_timer_minutes_value;
    set_away_on_timer_minutes_value = ts::CLAMP(o, 1, 180);
    if (o != set_away_on_timer_minutes_value)
        set_edit_value(CONSTASTR("away_min"), ts::wmake(set_away_on_timer_minutes_value));
    set_away_on_timer_minutes_value_last = set_away_on_timer_minutes_value;
    mod();
    return true;
}

bool dialog_settings_c::notification_handler( RID, GUIPARAM p )
{
    bgroups[ BGROUP_INCOMING_NOTIFY ].handler( RID(), p );
    ctlenable( CONSTASTR( "dndur" ), 0 != ( bgroups[ BGROUP_INCOMING_NOTIFY ].current & 2 ) );

    mod();
    return true;

}

bool dialog_settings_c::desktop_notification_duration_handler( const ts::wstr_c &v, bool )
{
    desktop_notification_duration = v.as_int();
    int o = desktop_notification_duration;
    desktop_notification_duration = ts::CLAMP( o, 1, 60 );
    if ( o != desktop_notification_duration )
        set_edit_value( CONSTASTR( "dndur" ), ts::wmake( desktop_notification_duration ) );
    mod();
    return true;
}


void dialog_settings_c::bits_edit_s::flush()
{
    for (int i = 0; i < nmasks; ++i)
    {
        if (masks[i] == 0)
            continue;

        int localmask = 1 << i;
        bool newval = (localmask & current) != 0;
        INITFLAG(*source, masks[i], newval);
    }
}

bool dialog_settings_c::bits_edit_s::handler(RID, GUIPARAM p)
{
    current = as_int(p);

    flush();

    settings->mod();
    return true;
}

bool dialog_settings_c::msgopts_handler( RID, GUIPARAM p )
{
    bgroups[BGROUP_MSGOPTS].handler(RID(), p);

    ctlenable(CONSTASTR("date_msg_tmpl"), 0 != (msgopts_current & MSGOP_SHOW_DATE));
    ctlenable(CONSTASTR("date_sep_tmpl"), 0 != (msgopts_current & MSGOP_SHOW_DATE_SEPARATOR));

    ctlenable( CONSTASTR( "msgopts32" ), 0 != ( msgopts_current & MSGOP_SHOW_INLINE_IMG ) );

    return true;
}

ts::wstr_c dialog_settings_c::getactivedict()
{
    if (tt_next_check < ts::Time::current())
    {
        ts::wstrings_c alldicts;
        g_app->get_local_spelling_files(alldicts);
        alldicts.kill_dups_and_sort(true);
        int a = 0;
        ts::aint aa = alldicts.size();

        for( ts::wstr_c &n : alldicts)
        {
            n.trunc_length(4);
            if (disabled_spellchk.find(ts::fn_get_name(n)) < 0)
                ++a;
        }

        tt_cache = TTT("Active dictionaries: $ of $", 410) / ts::wmake(a) / ts::wmake(aa);
        tt_next_check = ts::Time::current() + 5000;
    }


    return tt_cache;
}

bool dialog_settings_c::chat_options(RID, GUIPARAM p)
{
    bgroups[BGROUP_CHAT].handler(RID(), p);

    if (RID b = find(CONSTASTR("seldict")))
    {
        ctlenable(CONSTASTR("seldict"), p != nullptr && !dialog_already_present(UD_DICTIONARIES));
        HOLD(b).as<gui_control_c>().tooltip( DELEGATE(this, getactivedict) );
    }

    return true;
}

bool dialog_settings_c::seldict(RID, GUIPARAM)
{
    SUMMON_DIALOG<dialog_dictionaries_c>(UD_DICTIONARIES, false, this, disabled_spellchk);
    return true;
}

void dialog_settings_c::set_disabled_splchklist(const ts::wstrings_c &s, bool need2rewarn_)
{
    disabled_spellchk = s;
    need2rewarn = need2rewarn_;
    mod();
}

bool dialog_settings_c::scale_font_conv_text(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    font_scale_conv_text = pp->value;

    int sz = ts::lround(font_scale_conv_text * fontsz_conv_text );

    pp->custom_value_text = TTT("Message font size: $",346)/ts::wmake(sz);
    if (sz != fontsz_conv_text)
        pp->custom_value_text.insert(0,CONSTWSTR("<b>")).append(CONSTWSTR("</b>"));

    mod();
    return true;
}

bool dialog_settings_c::scale_font_msg_edit(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    font_scale_msg_edit = pp->value;

    int sz = ts::lround(font_scale_msg_edit * fontsz_msg_edit);

    pp->custom_value_text = TTT("Message edit font size: $",429) / ts::wmake(sz);
    if (sz != fontsz_msg_edit)
        pp->custom_value_text.insert(0, CONSTWSTR("<b>")).append(CONSTWSTR("</b>"));

    mod();
    return true;
}

void dialog_settings_c::select_theme(const ts::str_c& prm)
{
    ts::wstr_c selfo( from_utf8(prm) );
    for( theme_info_s &ti : m_themes )
        ti.current = ti.folder.equals(selfo);
    set_combik_menu(CONSTASTR("themes"), list_themes());

    ++force_change; mod();
}

menu_c dialog_settings_c::list_themes()
{
    menu_c m;
    for( const theme_info_s &ti : m_themes )
        m.add( ti.name, ti.current ? MIF_MARKED : 0, DELEGATE(this, select_theme), to_utf8(ti.folder) );
    return m;
}

void dialog_settings_c::mod()
{
    bool ch = false;
    for (settings::value_watch_s *vw : watch_array)
        if (vw->changed())
        {
            ch = true;
            break;
        }

    ctlenable(CONSTASTR("dialog_button_1"), ch);
}

#define PREPARE(var, inits) var = inits; watch(var)
#define font_size_slider( fontname ) float minscale##fontname = fontsz_##fontname ? (8.0f / fontsz_##fontname) : ts::tmin(0.9f, font_scale_##fontname); \
                                    float maxscale##fontname = fontsz_##fontname ? (32.0f / fontsz_##fontname) : ts::tmax(1.1f, font_scale_##fontname); \
                                    ts::wstr_c initstr##fontname(CONSTWSTR("0/")); \
                                    initstr##fontname.append_as_float(minscale##fontname).append(CONSTWSTR("/0.5/1/1/")).append_as_float(maxscale##fontname); \
                                    dm().hslider(ts::wstr_c(), font_scale_##fontname, initstr##fontname, DELEGATE(this, scale_font_##fontname))




/*virtual*/ int dialog_settings_c::additions( ts::irect & edges )
{
    PREPARE( force_change, 0 );

    if(profile_selected)
    {
        rekey = REKEY_NOTHING_TO_DO;
        was_encrypted_profile = prf().is_encrypted();
        PREPARE( encrypted_profile, was_encrypted_profile);
        PREPARE( encrypted_profile_password, prf().get_keyhash_str() );

        PREPARE( ctl2send, (enter_key_options_s)prf().ctl_to_send() );

        PREPARE( msgopts_current, prf_options().__bits );
        msgopts_original = msgopts_current;

        for (bits_edit_s &b : bgroups)
            b.settings = this, b.source = &msgopts_current;

        PREPARE( desktop_notification_duration, prf().dmn_duration() );

        PREPARE(set_away_on_timer_minutes_value, prf().inactive_time());
        set_away_on_timer_minutes_value_last = set_away_on_timer_minutes_value;

        bgroups[BGROUP_COMMON1].add(UIOPT_SHOW_NEWCONN_BAR);

        bgroups[BGROUP_CLIST].add(UIOPT_SHOW_SEARCH_BAR);
        bgroups[BGROUP_CLIST].add(UIOPT_TAGFILETR_BAR);
        bgroups[BGROUP_CLIST].add(UIOPT_PROTOICONS);
        bgroups[BGROUP_CLIST].add(UIOPT_GEN_IDENTICONS);
        bgroups[BGROUP_CLIST].add(CLOPT_GROUP_CONTACTS_PROTO);
        bgroups[BGROUP_CLIST].add(CLOPT_SMALL_ITEMS);

        bgroups[BGROUP_COMMON2].add(UIOPT_AWAYONSCRSAVER);
        bgroups[BGROUP_COMMON2].add(0, set_away_on_timer_minutes_value > 0);
        bgroups[BGROUP_COMMON2].add(UIOPT_KEEPAWAY);

        bgroups[BGROUP_COMMON3].add(OPTOPT_POWER_USER);

        bgroups[BGROUP_CONFERENCE].add(COPT_MUTE_MIC_ON_INVITE);
        bgroups[BGROUP_CONFERENCE].add(COPT_MUTE_SPEAKER_ON_INVITE);
        bgroups[BGROUP_CONFERENCE].add(COPT_MUTE_MINIMIZED);

        bgroups[BGROUP_CHAT].add(MSGOP_SPELL_CHECK);

        bgroups[BGROUP_MSGOPTS].add(MSGOP_SHOW_DATE);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_SHOW_DATE_SEPARATOR);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_SHOW_PROTOCOL_NAME);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_REPLACE_SHORT_SMILEYS);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_SHOW_INLINE_IMG);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_MAXIMIZE_INLINE_IMG);
        bgroups[BGROUP_MSGOPTS].add(MSGOP_PROCESS_MARKDOWN);


        bgroups[BGROUP_TYPING].add(MSGOP_SEND_TYPING);

        bgroups[BGROUP_TYPING_NOTIFY].add(UIOPT_SHOW_TYPING_CONTACT);
        bgroups[BGROUP_TYPING_NOTIFY].add(UIOPT_SHOW_TYPING_MSGLIST);

        bgroups[BGROUP_CALL_NOTIFY].add(UIOPT_SHOW_INCOMING_CALL_BAR);

        bgroups[ BGROUP_INCOMING_NOTIFY ].add( UIOPT_INTRUSIVE_BEHAVIOUR );
        bgroups[ BGROUP_INCOMING_NOTIFY ].add( UIOPT_SHOW_INCOMING_MSG_PNL );

        bgroups[ BGROUP_SOUNDS_NOTIFY ].add( SNDOPT_MUTE_ON_AWAY );
        bgroups[ BGROUP_SOUNDS_NOTIFY ].add( SNDOPT_MUTE_ON_DND );

        bgroups[BGROUP_HISTORY].add(MSGOP_KEEP_HISTORY);
        bgroups[BGROUP_HISTORY].add(MSGOP_LOAD_WHOLE_HISTORY);

        PREPARE(load_history_count, prf().min_history());
        PREPARE( load_history_count_addition, prf().add_history() );

        PREPARE(font_scale_conv_text, prf().fontscale_conv_text() );
        PREPARE(font_scale_msg_edit, prf().fontscale_msg_edit());

        PREPARE(date_msg_tmpl, prf().date_msg_template());
        PREPARE(date_sep_tmpl, prf().date_sep_template());
        PREPARE(downloadfolder, prf().download_folder());
        PREPARE(downloadfolder_images, prf().download_folder_images());
        PREPARE(downloadfolder_fshare, prf().download_folder_fshare());
        PREPARE( fileconfirm, prf().fileconfirm() );
        PREPARE( auto_download_masks, prf().auto_confirm_masks() );
        PREPARE( manual_download_masks, prf().manual_confirm_masks() );

        table_active_protocol = &prf().get_table_active_protocol();
        table_active_protocol_underedit = *table_active_protocol;

        for (auto &row : table_active_protocol_underedit)
            if (active_protocol_c *ap = prf().ap(row.id))
            {
                row.other.configurable = ap->get_configurable();
                if (row.other.configurable.proxy.proxy_addr.is_empty()) row.other.configurable.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);
            }

        PREPARE( smilepack,  prf().emoticons_pack() );

        PREPARE(disabled_spellchk, ts::wstrings_c( prf().get_disabled_dicts(), '/' ));

        PREPARE( pmisc_flags, prf().misc_flags() );
    }

    PREPARE( tempfolder_sendimg, cfg().temp_folder_sendimg() );
    PREPARE( tempfolder_handlemsg, cfg().temp_folder_handlemsg() );

    PREPARE(tools_bits, cfg().allow_tools());

    PREPARE( startopt, detect_startopts() );

    PREPARE(misc_flags, cfg().misc_flags());

    int bo = 0;
    bo = ( MISCF_DONT_BACKUP_PROFILE & misc_flags) ? 0 : 1;
    bo |= (MISCF_DONT_LIM_BACKUP_DAYS & misc_flags) ? 0 : 2;
    bo |= (MISCF_DONT_LIM_BACKUP_SIZE & misc_flags) ? 0 : 4;
    bo |= (MISCF_DONT_LIM_BACKUP_COUNT & misc_flags) ? 0 : 8;

    PREPARE( backupopt, bo );
    PREPARE( backuppath, cfg().folder_backup() );
    PREPARE(backup_clean_days, cfg().lim_backup_days());
    PREPARE(backup_clean_size, cfg().lim_backup_size());
    PREPARE(backup_clean_count, cfg().lim_backup_count());


    PREPARE( curlang, cfg().language() );
    PREPARE( autoupdate, cfg().autoupdate() );
    oautoupdate = autoupdate;
    PREPARE( proxy, cfg().proxy() );
    PREPARE( proxy_addr, cfg().proxy_addr() );

    if (profile_selected)
    {
        PREPARE(useproxyfor, prf().useproxyfor());
    }

    PREPARE( collapse_beh, cfg().collapse_beh() );
    PREPARE( talkdevice, cfg().device_talk() );
    PREPARE( signaldevice, cfg().device_signal() );
    PREPARE( micdevice, string_from_device(mic_device_stored) );
    auto getcam = []()->ts::wstrmap_c
    {
        ts::wstrmap_c c( cfg().device_camera() );
        if ( nullptr == c.find(CONSTWSTR("id")) )
            c.set( CONSTWSTR("id") ) = CONSTWSTR("desktop");
        return c;
    };
    PREPARE( camera, std::move(getcam()) );

    PREPARE( cvtmic.volume, cfg().vol_mic() );
    PREPARE( talk_vol, cfg().vol_talk() );
    PREPARE( signal_vol, cfg().vol_signal() );

    PREPARE( dsp_flags, cfg().dsp_flags() );

    PREPARE( mute_all_sounds, cfg().sounds_flags() != 0 );

#define SND(s) PREPARE( sndfn[snd_##s], cfg().snd_##s() ); PREPARE( sndvol[snd_##s], cfg().snd_vol_##s() );
    SOUNDS
#undef SND

    auto getdebug = []()->ts::astrmap_c
    {
        ts::astrmap_c d(cfg().debug());
        return d;
    };
    PREPARE(debug, std::move(getdebug()));

    int textrectid = 0;

    menu_c m;
    if (profile_selected)
    {
        m.add_sub( TTT("Profile",1) )
            .add(TTT("General",32), 0, TABSELMI(MASK_PROFILE_COMMON) )
            .add( TTT("Contact list",476), 0, TABSELMI( MASK_PROFILE_CLIST ) )
            .add(TTT("Notifications",401), 0, TABSELMI(MASK_PROFILE_NOTIFICATIONS))
            .add(TTT("Chat",109), 0, TABSELMI(MASK_PROFILE_CHAT) )
            .add(TTT("Conference",305), 0, TABSELMI(MASK_PROFILE_CONFERENCE) )
            .add(TTT("Messages & History",327), 0, TABSELMI(MASK_PROFILE_MSGSNHIST) )
            .add(TTT("File receive",236), 0, TABSELMI(MASK_PROFILE_FILES) )
            .add(loc_text(loc_networks), 0, TABSELMI(MASK_PROFILE_NETWORKS) );
    }

    m.add_sub(TTT("Application",34))
        .add(TTT("General",106), 0, TABSELMI(MASK_APPLICATION_COMMON))
        .add(TTT("System",35), 0, TABSELMI(MASK_APPLICATION_SYSTEM))
        .add(TTT("Audio",125), 0, TABSELMI(MASK_APPLICATION_SETSOUND))
        .add(TTT("Sounds",293), 0, TABSELMI(MASK_APPLICATION_SOUNDS))
        .add(TTT("Video",347), 0, TABSELMI(MASK_APPLICATION_VIDEO));

    if ( profile_selected && prf_options().is(OPTOPT_POWER_USER) )
    m.add_sub(TTT("Advanced",394))
        .add(AM_TOOLS, 0, TABSELMI(MASK_ADVANCED_TOOLS))
        .add(AM_MISC, 0, TABSELMI(MASK_ADVANCED_MISC))
        .add(AM_DEBUG, 0, TABSELMI(MASK_ADVANCED_DEBUG));

    descmaker dm(this);
    dm << MASK_APPLICATION_COMMON; //_________________________________________________________________________________________________//

    dm().page_caption(TTT("General application settings",108));
    dm().combik(LOC_LANGUAGE).setmenu( list_langs( curlang, DELEGATE(this, select_lang) ) ).setname( CONSTASTR("langs") );
    dm().vspace(10);
    dm().combik(TTT("GUI theme",233)).setmenu(list_themes()).setname(CONSTASTR("themes"));
    dm().vspace();

    dm().vspace();
    dm().radio(TTT("Updates",155), DELEGATE(this, autoupdate_handler), autoupdate).setmenu(
        menu_c()
        .add(TTT("Do not check",156), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("Check and notify",157), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Check, download and update",158), 0, MENUHANDLER(), CONSTASTR("2"))
        );

    dm().vspace();
    if (profile_selected)
    {
        dm().checkb(TTT("Proxy settings", 194), DELEGATE(this, useproxy_handler), useproxyfor).setmenu(
            menu_c().add(TTT("Use proxy for check and download updates", 195), 0, MENUHANDLER(), ts::amake<int>(USE_PROXY_FOR_AUTOUPDATES))
            .add(TTT("Use proxy for download spelling dictionaries", 196), 0, MENUHANDLER(), ts::amake<int>(USE_PROXY_FOR_LOAD_SPELLING_DICTS)));
    }
    dm().hgroup(ts::wsptr());
    dm().combik(HGROUP_MEMBER).setmenu(list_proxy_types(proxy, DELEGATE(this, proxy_handler))).setname(CONSTASTR("proxytype"));
    dm().textfield(HGROUP_MEMBER, ts::to_wstr(proxy_addr), DELEGATE(this, proxy_addr_handler)).setname(CONSTASTR("proxyaddr"));

    dm << MASK_APPLICATION_SYSTEM; //_________________________________________________________________________________________________//

    dm().page_caption( TTT("Some system settings of application",36) );

    dm().checkb(TTT("Autostart", 309), DELEGATE(this, startopt_handler), startopt).setname(CONSTASTR("start")).setmenu(
        menu_c().add(loc_text(loc_autostart), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Minimize on start with system", 311), 0, MENUHANDLER(), CONSTASTR("2"))
        );
    dm().vspace();

    ts::wstr_c profname = cfg().profile();
    if (!profname.is_empty()) profile_c::path_by_name(profname);

    dm().path( TTT("Current profile path",37), ts::to_wstr(profname) ).readonly(true);

    dm().vspace();
    dm().checkb( TTT("Profile backup",495), DELEGATE( this, backupopt_handler ), backupopt ).setmenu(
        menu_c().add( TTT("Create backup on start",496), 0, MENUHANDLER(), CONSTASTR( "1" ) )
    );
    dm().path( TTT("Backup path",497), backuppath, DELEGATE( this, backuppath_handler ) );

    ts::wstr_c blm1, blm2, blm3;
    dm().textfield(ts::wstr_c(), ts::wmake(backup_clean_days), DELEGATE(this, backup_clean_days_handler)).setname(CONSTASTR("backup_clean_days")).width(50).subctl(textrectid++, blm1);
    dm().textfield(ts::wstr_c(), ts::wmake(backup_clean_size), DELEGATE(this, backup_clean_size_handler)).setname(CONSTASTR("backup_clean_size")).width(50).subctl(textrectid++, blm2);
    dm().textfield(ts::wstr_c(), ts::wmake(backup_clean_count), DELEGATE(this, backup_clean_count_handler)).setname(CONSTASTR("backup_clean_count")).width(50).subctl(textrectid++, blm3);

    dm().checkb(ts::wstr_c(), DELEGATE(this, backupopt_handler), backupopt).setmenu(
        menu_c().add(TTT("Delete backup files older than $ days",526) / blm1, 0, MENUHANDLER(), CONSTASTR("2"))
                .add(TTT("Limit size of backup files by $ Mb",527) / blm2, 0, MENUHANDLER(), CONSTASTR("4"))
                .add(TTT("Keep only $ backup files",528) / blm3, 0, MENUHANDLER(), CONSTASTR("8"))
    ).setname(CONSTASTR("awayflags"));


    dm().vspace();
    dm().radio(TTT("Minimize to notification area",118), DELEGATE(this, collapse_beh_handler), collapse_beh).setmenu(
        menu_c()
        .add(TTT("Don't minimize to notification area",119), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("[quote]Minimize[quote] button $ minimizes to notification area",120) / CONSTWSTR("<img=bmin,-1>"), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("[quote]Close[quote] button $ minimizes to notification area",121) / CONSTWSTR("<img=bclose,-1>"), 0, MENUHANDLER(), CONSTASTR("2"))
        );


    dm << MASK_APPLICATION_SETSOUND; //______________________________________________________________________________________________//
    dm().page_caption(TTT("Audio settings",127));
    dm().hgroup(TTT("Microphone",126));
    dm().combik(HGROUP_MEMBER).setmenu( list_audio_capture_devices() ).setname( CONSTASTR("mic") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=rec"), DELEGATE(this, test_mic) ).sethint(TTT("Record test 5 seconds",280)).setname( CONSTASTR("micrecb") );
    dm().vspace();
    dm().hslider(ts::wsptr(), cvtmic.volume, CONSTWSTR("0/0/0.5/1/1/5"), DELEGATE(this, micvolset)).setname(CONSTASTR("micvol")).setmenu(
            menu_c().add(CONSTWSTR("= 50%"), 0, nullptr, CONSTASTR("0.5"))
                    .add(CONSTWSTR("= 100%"), 0, nullptr, CONSTASTR("1"))
                    .add(CONSTWSTR("= 200%"), 0, nullptr, CONSTASTR("2"))
                    .add(CONSTWSTR("= 300%"), 0, nullptr, CONSTASTR("3"))
        );
    dm().vspace(3);
    dm().checkb(ts::wsptr(), DELEGATE(this, dspf_handler), dsp_flags).setmenu(
            menu_c().add(TTT("Noise filter of microphone",289), 0, MENUHANDLER(), ts::amake<int>( DSP_MIC_NOISE ))
                    .add(TTT("Automatic gain of microphone",290), 0, MENUHANDLER(), ts::amake<int>( DSP_MIC_AGC ))   );

    dm().vspace();
    dm().hgroup(TTT("Speakers",128));
    dm().combik(HGROUP_MEMBER).setmenu( list_talk_devices() ).setname( CONSTASTR("talk") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=play"), DELEGATE(this, test_talk_device) ).sethint(TTT("Test speakers",131));
    dm().vspace();
    dm().hslider(ts::wsptr(), talk_vol, CONSTWSTR("0/0/1/1"), DELEGATE(this, talkvolset));
    dm().vspace(3);
    dm().checkb(ts::wsptr(), DELEGATE(this, dspf_handler), dsp_flags).setmenu(
        menu_c().add(TTT("Noise filter of incoming peer voice",291), 0, MENUHANDLER(), ts::amake<int>(DSP_SPEAKERS_NOISE))
                .add(TTT("Automatic gain of incoming peer voice",292), 0, MENUHANDLER(), ts::amake<int>(DSP_SPEAKERS_AGC)) );
    dm().vspace();

    dm().hgroup(TTT("Ringtone and other signals",129));
    dm().combik(HGROUP_MEMBER).setmenu( list_signal_devices() ).setname( CONSTASTR("signal") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=play"), DELEGATE(this, test_signal_device) ).sethint(TTT("Test ringtone",132));
    dm().vspace();
    dm().hslider(ts::wsptr(), signal_vol, CONSTWSTR("0/0/1/1"), DELEGATE(this, signalvolset));
    dm().vspace(10);
    dm().label(ts::wsptr()).setname( CONSTASTR( "soundhint" ) );

    dm << MASK_APPLICATION_SOUNDS; //______________________________________________________________________________________________//
    dm().page_caption(TTT("Sounds",294));
    dm().list(TTT("Sounds list",295), ts::wsptr(), -300).setname(CONSTASTR("soundslist"));
    dm().vspace();
    dm().hgroup(TTT("Presets",298));
    dm().combik(HGROUP_MEMBER).setmenu(get_list_avaialble_sound_presets()).setname(CONSTASTR("availablepresets"));
    dm().button(HGROUP_MEMBER, TTT("Apply preset",299), DELEGATE(this, applysoundpreset)).width(250).height(25).setname(CONSTASTR("applypreset"));
    dm().vspace();
    dm().checkb( ts::wsptr(), DELEGATE( this, mute_handler ), mute_all_sounds ? 1 : 0 ).setmenu(
        menu_c().add( TTT("Mute all sounds",475), 0, MENUHANDLER(), CONSTASTR("1") ) );

    ts::ivec2 bsz(20);
    if (const button_desc_s *bdesc = gui->theme().get_button(CONSTASTR("play")))
        bsz = bdesc->size;

    int slh = bsz.y;
    if (const theme_rect_s *th = gui->theme().get_rect(CONSTASTR("hslider")))
        slh = th->sis[SI_TOP].height();

    ts::sstr_t<-32> isnd;
    int www = 295;
    for(int sndi = 0; sndi < snd_count; ++sndi)
    {
        isnd.set_as_int(sndi);
        dm().selector(ts::wstr_c(), sndfn[sndi], DELEGATE(this, sndselhandler)).setmenu(get_sounds_menu()).setname(CONSTASTR("sndfn") + isnd).width(www).subctl(textrectid++, sndselctl[sndi]);
        dm().button(ts::wstr_c(), CONSTWSTR("face=play"), DELEGATE(this, sndplayhandler)).setname(CONSTASTR("sndpl") + isnd).width(bsz.x).height(bsz.y).subctl(textrectid++, sndplayctl[sndi]);
        dm().hslider(ts::wstr_c(), sndvol[sndi], CONSTWSTR("0/0/1/1"), DELEGATE(this, sndvolhandler)).setname(CONSTASTR("sndvl") + isnd).width(www).height(slh).subctl(textrectid++, sndvolctl[sndi]);
    }

    dm << MASK_APPLICATION_VIDEO; //______________________________________________________________________________________________//
    dm().page_caption(TTT("Video settings",348));
    dm().vspace(15);
    dm().combik(TTT("Default video source",349)).setmenu(list_video_capture_devices()).setname(CONSTASTR("camera"));
    dm().combik(ts::wsptr()).setmenu(list_video_capture_resolutions()).setname(CONSTASTR("camerares"));
    dm().vspace();
    dm().panel(PREVIEW_HEIGHT, DELEGATE(this, drawcamerapanel)).setname(CONSTASTR("preview"));

    if (profile_selected)
    {
        dm << MASK_PROFILE_COMMON; //____________________________________________________________________________________________________//
        dm().page_caption( TTT("General profile settings",38) );

        dm().checkb(ts::wstr_c(), DELEGATE(this, encrypt_handler), enc_val()).setname(CONSTASTR("encrypt")).setmenu(
            menu_c().add( TTT( "Encrypted profile", 52 ), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        dm().vspace();
        dm().checkb(ts::wstr_c(), DELEGATE(bgroups+BGROUP_COMMON1, handler), bgroups[BGROUP_COMMON1].current).setmenu(
            menu_c().add(TTT("Show [i]join network[/i] button ($)",344)/ CONSTWSTR("Ctrl+N"), 0, MENUHANDLER(), CONSTASTR("1"))
        );


        dm().vspace();

        ts::wstr_c ctl;
        dm().textfield(ts::wstr_c(), ts::wmake(set_away_on_timer_minutes_value), DELEGATE(this, away_minutes_handler)).setname(CONSTASTR("away_min")).width(50).subctl(textrectid++, ctl);
        ts::wstr_c t_awaymin = TTT("Set [b]Away[/b] status when user has been inactive for $ minutes",322) / ctl;
        if (t_awaymin.find_pos(ctl) < 0) t_awaymin.append_char(' ').append(ctl);


        dm().checkb(ts::wstr_c(), DELEGATE(this, commonopts_handler), bgroups[BGROUP_COMMON2].current).setmenu(
            menu_c().add(TTT("Set [b]Away[/b] status on screen saver activation",323), 0, MENUHANDLER(), CONSTASTR("1"))
                    .add(t_awaymin, 0, MENUHANDLER(), CONSTASTR("2"))
                    .add(TTT("Keep [b]Away[/b] status until user typing",363), 0, MENUHANDLER(), CONSTASTR("4"))
            ).setname(CONSTASTR("awayflags"));

        dm().vspace(10);
        dm().checkb(ts::wstr_c(), DELEGATE(bgroups+BGROUP_COMMON3, handler), bgroups[BGROUP_COMMON3].current).setmenu(
            menu_c().add(TTT("I'm power user. Please, show me advanced settings.",396), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        dm << MASK_PROFILE_CLIST; //____________________________________________________________________________________________________//
        dm().page_caption( TTT("Contact list settings",477) );

        dm().checkb( ts::wstr_c(), DELEGATE( bgroups + BGROUP_CLIST, handler ), bgroups[ BGROUP_CLIST ].current ).setmenu(
            menu_c().add( TTT( "Show search bar ($)", 341 ) / CONSTWSTR( "Ctrl+F" ), 0, MENUHANDLER(), CONSTASTR( "1" ) )
            .add( TTT( "Show tags filter bar ($)", 65 ) / CONSTWSTR( "Ctrl+T" ), 0, MENUHANDLER(), CONSTASTR( "2" ) )
            .add( TTT( "Protocol icons as contact state indicator", 296 ), 0, MENUHANDLER(), CONSTASTR( "4" ) )
            .add( TTT( "Generate identicons for contacts without avatar", 469 ), 0, MENUHANDLER(), CONSTASTR( "8" ) )
            .add( TTT("Group contacts by protocol",478), 0, MENUHANDLER(), CONSTASTR( "16" ) )
            .add(TTT("Compact list items",405), 0, MENUHANDLER(), CONSTASTR("32"))
        );

        dm << MASK_PROFILE_NOTIFICATIONS; //____________________________________________________________________________________________________//
        dm().page_caption(TTT("Notifications settings",402));
        dm().checkb(TTT("Typing notification", 272), DELEGATE(bgroups + BGROUP_TYPING_NOTIFY, handler), bgroups[BGROUP_TYPING_NOTIFY].current).setmenu(
            menu_c().add(TTT("Show typing notifications in contacts list", 274), 0, MENUHANDLER(), CONSTASTR("1"))
            .add(TTT("Show typing notifications in messages", 275), 0, MENUHANDLER(), CONSTASTR("2"))
            );

        dm().vspace();
        dm().checkb(ts::wstr_c(), DELEGATE(bgroups + BGROUP_CALL_NOTIFY, handler), bgroups[BGROUP_CALL_NOTIFY].current).setmenu(
            menu_c().add(TTT("Allow insistent notification of incoming call",403), 0, MENUHANDLER(), CONSTASTR("1"))
            );



        dm().vspace();

        ctl.clear();
        dm().textfield( ts::wstr_c(), ts::wmake( desktop_notification_duration ), DELEGATE( this, desktop_notification_duration_handler ) ).setname( CONSTASTR( "dndur" ) ).width( 50 ).subctl( textrectid++, ctl );

        dm().checkb( ts::wstr_c(), DELEGATE( this, notification_handler ), bgroups[ BGROUP_INCOMING_NOTIFY ].current ).setmenu(
            menu_c().add( TTT("Allow intrusive behaviour on incoming message (bring window to foreground)",459), 0, MENUHANDLER(), CONSTASTR( "1" ) )
                    .add( TTT("Show desktop notification of incoming message during $ seconds",434) / ctl, 0, MENUHANDLER(), CONSTASTR( "2" ) )
            );

        dm().checkb( ts::wstr_c(), DELEGATE( bgroups + BGROUP_SOUNDS_NOTIFY, handler), bgroups[ BGROUP_SOUNDS_NOTIFY ].current ).setmenu(
            menu_c().add( TTT("Mute all sound notifications if [b]Away[/b] status",467), 0, MENUHANDLER(), CONSTASTR( "1" ) )
            .add( TTT("Mute all sound notifications if [b]Busy[/b] status",468), 0, MENUHANDLER(), CONSTASTR( "2" ) )
        );

        dm << MASK_PROFILE_CHAT; //____________________________________________________________________________________________________//
        dm().page_caption(TTT("Chat settings",110));
        int enter_key = ctl2send; if (enter_key == EKO_ENTER_NEW_LINE_DOUBLE_ENTER) enter_key = EKO_ENTER_NEW_LINE;
        dm().radio(TTT("Enter key",111), DELEGATE(this, ctl2send_handler), enter_key).setmenu(
            menu_c().add(TTT("Enter - send message, Shift+Enter or Ctrl+Enter - new line",112), 0, MENUHANDLER(), ts::amake<int>(EKO_ENTER_SEND))
                    .add(TTT("Enter - new line, Shift+Enter or Ctrl+Enter - send message",113), 0, MENUHANDLER(), ts::amake<int>(EKO_ENTER_NEW_LINE))
            );
        double_enter = ctl2send == EKO_ENTER_NEW_LINE_DOUBLE_ENTER ? 1 : 0;
        dm().checkb(ts::wstr_c(), DELEGATE(this, ctl2send_handler_de), double_enter).setmenu(
            menu_c().add(TTT("Double Enter - send message", 152), 0, MENUHANDLER(), CONSTASTR("1"))
            ).setname(CONSTASTR("dblenter"));

        dm().vspace();
        dm().checkb(ts::wstr_c(), DELEGATE(bgroups+BGROUP_TYPING, handler), bgroups[BGROUP_TYPING].current).setmenu(
            menu_c().add(TTT("Send typing notification",273), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        ts::wstr_c seldictctl;
        dm().button(ts::wstr_c(), TTT("Dictionaries...",408), DELEGATE(this, seldict)).setname(CONSTASTR("seldict")).width(150).height(25).subctl(textrectid++, seldictctl);
        ts::wstr_c espt(TTT("Enable spell checker", 400), CONSTWSTR(" "));
        espt.append(seldictctl);

        dm().checkb(ts::wstr_c(), DELEGATE(this, chat_options), bgroups[BGROUP_CHAT].current).setmenu(
            menu_c().add(espt, 0, MENUHANDLER(), CONSTASTR("1"))
            );

        dm().vspace();
        font_size_slider(msg_edit);

        dm << MASK_PROFILE_CONFERENCE; //____________________________________________________________________________________________________//
        dm().page_caption(TTT("Conference settings",306));

        dm().checkb(ts::wstr_c(), DELEGATE(bgroups+BGROUP_CONFERENCE, handler), bgroups[BGROUP_CONFERENCE].current).setmenu(
            menu_c().add(TTT("Mute microphone on audio conference invite",307), 0, MENUHANDLER(), CONSTASTR("1"))
                    .add(TTT("Mute speakers on audio conference invite",308), 0, MENUHANDLER(), CONSTASTR("2"))
                    .add(TTT("Mute speakers when minimzed",529), 0, MENUHANDLER(), CONSTASTR("4"))
            );

        dm << MASK_PROFILE_MSGSNHIST; //____________________________________________________________________________________________________//

        dm().page_caption(TTT("Messages & history settings",328));

        dm().combik(TTT("Emoticon set", 269)).setmenu(emoti().get_list_smile_pack(smilepack, DELEGATE(this, smile_pack_selected))).setname(CONSTASTR("avasmliepack"));
        dm().vspace();

        ctl.clear();
        dm().textfield(ts::wstr_c(), from_utf8(date_msg_tmpl), DELEGATE(this, date_msg_tmpl_edit_handler)).setname(CONSTASTR("date_msg_tmpl")).width(100).subctl(textrectid++, ctl);
        ts::wstr_c t_showdate = TTT("Show message date (template: $)", 171) / ctl;
        if (t_showdate.find_pos(ctl) < 0) t_showdate.append_char(' ').append(ctl);

        ctl.clear();
        dm().textfield(ts::wstr_c(), from_utf8(date_sep_tmpl), DELEGATE(this, date_sep_tmpl_edit_handler)).setname(CONSTASTR("date_sep_tmpl")).width(140).subctl(textrectid++, ctl);
        ts::wstr_c t_showdatesep = TTT("Show separator $ between messages with different date", 172) / ctl;
        if (t_showdatesep.find_pos(ctl) < 0) t_showdate.append_char(' ').append(ctl);

        dm().checkb(TTT("Messages", 170), DELEGATE(this, msgopts_handler), bgroups[BGROUP_MSGOPTS].current).setmenu(
            menu_c().add(t_showdate, 0, MENUHANDLER(), CONSTASTR("1"))
            .add(t_showdatesep, 0, MENUHANDLER(), CONSTASTR("2"))
            .add(TTT("Show protocol name", 173), 0, MENUHANDLER(), CONSTASTR("4"))
            .add(TTT("Replace short smileys to emoticons",373), 0, MENUHANDLER(), CONSTASTR("8"))
            .add(TTT("Process Markdown syntax",404), 0, MENUHANDLER(), CONSTASTR("64"))
            .add(TTT("Show inline images",487), 0, MENUHANDLER(), CONSTASTR( "16" ))
            .add(TTT("Maximize inline images",391), 0, MENUHANDLER(), CONSTASTR("32"))
            ).setname(CONSTASTR("msgopts"));


        font_size_slider(conv_text);

        dm().vspace();

        dm().checkb(ts::wstr_c(), DELEGATE(this, histopts_handler), bgroups[BGROUP_HISTORY].current).setmenu(
            menu_c().add(TTT("Keep message history", 232), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        ctl.clear();
        dm().textfield(ts::wstr_c(), ts::wmake(load_history_count), DELEGATE(this, load_history_count_handler)).setname(CONSTASTR("loadcount")).width(50).subctl(textrectid++, ctl);
        ts::wstr_c ctl2;
        dm().textfield( ts::wstr_c(), ts::wmake( load_history_count_addition ), DELEGATE( this, load_history_count_a_handler ) ).setname( CONSTASTR( "loadcounta" ) ).width( 50 ).subctl( textrectid++, ctl2 );
        ts::wstr_c t_loadcount = TTT("...load $ messages (also load $ messages by button)",330) / ctl / ctl2;
        if (t_loadcount.find_pos(ctl) < 0) t_loadcount.append_char(' ').append(ctl);

        dm().vspace();
        dm().radio(TTT("On select contact...",331), DELEGATE(this, histopts_handler), (bgroups[BGROUP_HISTORY].current & 2) ? -1 : -2).setmenu(
            menu_c()
            .add(TTT("...load whole history", 329), 0, MENUHANDLER(), CONSTASTR("-1"))
            .add(t_loadcount, 0, MENUHANDLER(), CONSTASTR("-2")) );


        dm << MASK_PROFILE_FILES; //____________________________________________________________________________________________________//
        dm().page_caption(TTT("File receive settings",237));
        dm().path(TTT("Default download folder",174), downloadfolder, DELEGATE(this, downloadfolder_edit_handler));
        dm().vspace();
        dm().path(TTT("Default download folder for images",372), downloadfolder_images, DELEGATE(this, downloadfolder_images_edit_handler));
        dm().vspace();

        dm().radio(TTT("Confirmation",240), DELEGATE(this, fileconfirm_handler), fileconfirm).setmenu(
            menu_c().add(TTT("Confirmation required for any files",238), 0, MENUHANDLER(), CONSTASTR("0"))
                    .add(TTT("Files start downloading without confirmation",239), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        dm().vspace();
        dm().textfield(TTT("These files start downloading without confirmation",241), auto_download_masks, DELEGATE(this, fileconfirm_auto_masks_handler)).setname(CONSTASTR("confirmauto"));
        dm().vspace();
        dm().textfield(TTT("These files require confirmation to start downloading",242), manual_download_masks, DELEGATE(this, fileconfirm_manual_masks_handler)).setname(CONSTASTR("confirmmanual"));

        dm().vspace();
        dm().path(TTT("Default folder for shares",540), downloadfolder_fshare, DELEGATE(this, downloadfolder_fshare_edit_handler));
        dm().vspace();


        dm << MASK_PROFILE_NETWORKS; //_________________________________________________________________________________________________//
        dm().page_caption(loc_text(loc_networks));
        dm().page_header(TTT("[appname] supports simultaneous connections to multiple networks.",130));
        dm().vspace(10);
        dm().list(TTT("Active network connections",54), ts::wsptr(), -270).setname(CONSTASTR("protoactlist"));
        dm().vspace();
        dm().button(HGROUP_MEMBER, TTT("Add new network connection",58), DELEGATE(this, addnetwork)).height(35).setname(CONSTASTR("addnet"));
    }


    dm << MASK_ADVANCED_DEBUG;

    dm().page_caption(AM_DEBUG);

    dm().vspace();
    int dopts = 0;
    dopts |= debug.get(CONSTASTR(DEBUG_OPT_FULL_DUMP)).as_int() ? 1 : 0;
    dopts |= debug.get(CONSTASTR(DEBUG_OPT_LOGGING)).as_int() ? 2 : 0;
    dopts |= debug.get(CONSTASTR("contactids")).as_int() ? 4 : 0;

    dm().checkb(ts::wstr_c(), DELEGATE(this, debug_handler), dopts).setmenu(
        menu_c().add(CONSTWSTR("Create full memory dump on crash"), 0, MENUHANDLER(), CONSTASTR("1"))
                .add(CONSTWSTR("Enable logging"), 0, MENUHANDLER(), CONSTASTR("2"))
                //.add( CONSTWSTR("Enable telemetry"), 0, MENUHANDLER(), CONSTASTR( "8" ))
                .add(CONSTWSTR("Show contacts id's"), 0, MENUHANDLER(), CONSTASTR("4"))
        );

    dm().vspace();
    dm().textfield(CONSTWSTR("Addition updates URL"), to_wstr(debug.get(CONSTASTR("local_upd_url"))), DELEGATE(this, debug_local_upd_url));

    dopts = 0;
    dopts |= debug.get(CONSTASTR("ignproxy")).as_int() ? 1 : 0;
    dopts |= debug.get(CONSTASTR("onlythisurl")).as_int() ? 2 : 0;

    dm().checkb(ts::wstr_c(), DELEGATE(this, debug_handler2), dopts).setmenu(
        menu_c().add(CONSTWSTR("Ignory proxy settings for this url"), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(CONSTWSTR("Use only this url"), 0, MENUHANDLER(), CONSTASTR("2"))
        );

    dm << MASK_ADVANCED_TOOLS;
    dm().page_caption(AM_TOOLS);

    dm().checkb(ts::wstr_c(), DELEGATE(this, advt_handler), tools_bits).setmenu(
        menu_c().add(TTT("Color editor",433), 0, MENUHANDLER(), CONSTASTR("1"))
            .add( TTT("Command line generator",489), 0, MENUHANDLER(), CONSTASTR( "2" ) )
        );

    dm << MASK_ADVANCED_MISC;
    dm().page_caption(AM_MISC);
    dm().path( TTT("Temp folder for sending images",441), tempfolder_sendimg, DELEGATE( this, tempfolder_sendimg_edit_handler ) );
    dm().vspace();
    dm().path( TTT("Temp folder for message handler",442), tempfolder_handlemsg, DELEGATE( this, tempfolder_handlemsg_edit_handler ) );
    dm().vspace();

    dm().checkb( ts::wstr_c(), DELEGATE( this, miscf_handler ), misc_flags ).setmenu(
        menu_c().add( TTT( "Disable autoupdate to 64 bit version", 452 ), 0, MENUHANDLER(), ts::amake<int>( MISCF_DISABLE64 ) )
        .add( TTT("Disable window border",458), 0, MENUHANDLER(), ts::amake<int>( MISCF_DISABLEBORDER ) )
    );

    dm().checkb( ts::wstr_c(), DELEGATE( this, pmiscf_handler ), pmisc_flags ).setmenu(
        menu_c().add( TTT("Save avatar images",498), 0, MENUHANDLER(), ts::amake<int>( PMISCF_SAVEAVATARS ) )
    );

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    gui_vtabsel_c &tab = MAKE_CHILD<gui_vtabsel_c>( getrid(), m );
    tab.leech( TSNEW(leech_dock_left_s, SETTINGS_VTAB_WIDTH) );
    edges = ts::irect(SETTINGS_VTAB_WIDTH + VTAB_OTS,0,0,0);

    mod();

    return 1;
}

bool dialog_settings_c::debug_local_upd_url(const ts::wstr_c &v, bool )
{
    if (v.is_empty())
        debug.unset(CONSTASTR("local_upd_url"));
    else
        debug.set( CONSTASTR("local_upd_url") ) = ts::to_str(v);

    mod();
    return true;
}

bool dialog_settings_c::debug_handler(RID, GUIPARAM p)
{
    int opts = as_int(p);

    if (0 == (opts & 1))
        debug.unset(CONSTASTR(DEBUG_OPT_FULL_DUMP));
    else
        debug.set(CONSTASTR(DEBUG_OPT_FULL_DUMP)) = CONSTASTR("1");

    if (0 == (opts & 2))
        debug.unset(CONSTASTR(DEBUG_OPT_LOGGING));
    else
        debug.set(CONSTASTR(DEBUG_OPT_LOGGING)) = CONSTASTR("-1");

    if (0 == (opts & 4))
        debug.unset(CONSTASTR("contactids"));
    else
        debug.set(CONSTASTR("contactids")) = CONSTASTR("1");

    mod();
    return true;
}

bool dialog_settings_c::debug_handler2(RID, GUIPARAM p)
{
    int opts = as_int(p);

    if (0 == (opts & 1))
        debug.unset(CONSTASTR("ignproxy"));
    else
        debug.set(CONSTASTR("ignproxy")) = CONSTASTR("1");

    if (0 == (opts & 2))
        debug.unset(CONSTASTR("onlythisurl"));
    else
        debug.set(CONSTASTR("onlythisurl")) = CONSTASTR("1");

    mod();
    return true;
}

bool dialog_settings_c::miscf_handler( RID, GUIPARAM p )
{
    misc_flags = as_int( p );
    mod();
    return true;
}

bool dialog_settings_c::pmiscf_handler( RID, GUIPARAM p )
{
    pmisc_flags = as_int( p );
    mod();
    return true;
}


bool dialog_settings_c::startopt_handler( RID, GUIPARAM p )
{
    startopt = as_int(p);

    ctlenable( CONSTASTR("start2"), 0 != (startopt & 1) );

    mod();
    return true;
}

bool dialog_settings_c::backupopt_handler( RID, GUIPARAM p )
{
    backupopt = as_int( p );

    ctlenable(CONSTASTR("backup_clean_days"), 0 != (backupopt & 2));
    ctlenable(CONSTASTR("backup_clean_size"), 0 != (backupopt & 4));
    ctlenable(CONSTASTR("backup_clean_count"), 0 != (backupopt & 8));

    mod();
    return true;
}

bool dialog_settings_c::backuppath_handler( const ts::wstr_c &p, bool )
{
    backuppath = p;
    mod();
    return true;
}

bool dialog_settings_c::backup_clean_days_handler(const ts::wstr_c &n, bool)
{
    backup_clean_days = n.as_int();

    if (backup_clean_days < 0)
    {
        backup_clean_days = 0;
        set_edit_value(CONSTASTR("backup_clean_days"), ts::wmake(backup_clean_days));
    }
    mod();
    return true;
}
bool dialog_settings_c::backup_clean_size_handler(const ts::wstr_c &n, bool)
{
    backup_clean_size = n.as_int();

    if (backup_clean_size < 0)
    {
        backup_clean_size = 0;
        set_edit_value(CONSTASTR("backup_clean_days"), ts::wmake(backup_clean_size));
    }
    mod();
    return true;
}
bool dialog_settings_c::backup_clean_count_handler(const ts::wstr_c &n, bool)
{
    backup_clean_count = n.as_int();
    if (backup_clean_count < 0)
    {
        backup_clean_count = 0;
        set_edit_value(CONSTASTR("backup_clean_days"), ts::wmake(backup_clean_count));
    }
    mod();
    return true;
}


bool dialog_settings_c::advt_handler(RID, GUIPARAM p)
{
    tools_bits = as_int(p);
    mod();
    return true;
}


bool dialog_settings_c::sndvolhandler( RID srid, GUIPARAM p )
{
    ts::str_c sname = find(srid);
    if (sname.get_length() > 5)
    {
        sound_e snd = (sound_e)sname.substr(5).as_int();
        gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
        sndvol[snd] = pp->value;
        pp->custom_value_text.set(CONSTWSTR("<l>")).append(TTT("Volume: $",297) / ts::roundstr<ts::wstr_c, float>(pp->value * 100.0f, 1).append_char('%')).append(CONSTWSTR("</l>"));

        mod();
    }

    return true;
}

bool dialog_settings_c::sndplayhandler(RID brid, GUIPARAM p)
{
    ts::str_c bname = find(brid);
    if (bname.get_length() > 5)
    {
        sound_e snd = (sound_e)bname.substr(5).as_int();
        if (ASSERT(snd >= 0 && snd < snd_count))
            if (ts::blob_c buf = ts::g_fileop->load(ts::fn_join(ts::pwstr_c(CONSTWSTR("sounds")),sndfn[snd])))
                media.play(buf, sndvol[snd]);
    }

    return true;
}

bool dialog_settings_c::sndselhandler( RID srid, GUIPARAM p )
{
    behav_s *b = (behav_s *)p;
    if (behav_s::EVT_ON_CLICK == b->e)
    {
        ts::str_c fnctl = find( srid );
        ASSERT(!fnctl.is_empty());
        sound_e snd = (sound_e)fnctl.substr(5).as_int();
        if (ASSERT(snd >= 0 && snd < snd_count))
        {
            sndfn[snd] = ts::from_utf8(b->param);
            mod();
        }
    }


    return true;
}

bool dialog_settings_c::mute_handler( RID, GUIPARAM p )
{
    mute_all_sounds = p != nullptr;
    mod();
    return true;
}


menu_c dialog_settings_c::get_list_avaialble_sound_presets()
{
    menu_c m;
    m.add(TTT("Choose preset",300), selected_preset < 0 ? MIF_MARKED : 0, DELEGATE(this, soundpresetselected), CONSTASTR("-1"));
    bool sep = false;
    ts::aint cnt = presets.size();
    for(int i=0;i<cnt;++i)
    {
        const sound_preset_s &preset = presets.get(i);

        if (!sep) m.add_separator();
        sep = true;
        m.add(preset.name, i == selected_preset ? MIF_MARKED : 0, DELEGATE(this, soundpresetselected), ts::amake(i) );
    }
    return m;
}

void dialog_settings_c::soundpresetselected(const ts::str_c &p)
{
    selected_preset = p.as_int();
    ctlenable(CONSTASTR("applypreset"), selected_preset >= 0);
    set_combik_menu(CONSTASTR("availablepresets"), get_list_avaialble_sound_presets());

}

bool dialog_settings_c::applysoundpreset(RID, GUIPARAM)
{
    if (selected_preset >= 0 && selected_preset < presets.size())
    {
        ts::asptr names[snd_count] = {
#define SND(s) CONSTASTR(#s),
            SOUNDS
#undef SND
        };

        const sound_preset_s &prst = presets.get(selected_preset);

        float volk = (float)prst.preset.get_int( CONSTASTR("setvolume"), 100 ) / 100.0f;

        for(int i=0;i<snd_count;++i)
            if (const ts::abp_c *rec = prst.preset.get(names[i]))
            {
                sndfn[i] = ts::fn_join( prst.path, ts::from_utf8(rec->as_string()) );
                sndvol[i] = volk;

                set_edit_value(ts::amake(CONSTASTR("sndfn"), i), sndfn[i]);
                set_slider_value(ts::amake(CONSTASTR("sndvl"), i), volk);
            }

        mod();

    }
    return true;
}


menu_c dialog_settings_c::get_sounds_menu()
{
    return sounds_menu;
}


const protocol_description_s * dialog_settings_c::describe_network(ts::wstr_c&desc, const ts::str_c& name, const ts::str_c& tag, int id) const
{
    ts::wstr_c idname( CONSTWSTR( "ID" ) );
    const protocol_description_s *p = available_prots.find(tag);
    if (p == nullptr)
    {
        desc.replace_all(CONSTWSTR("{name}"), TTT("$ (Unknown protocol: $)",57) / from_utf8(name) / ts::to_wstr(tag));
        desc.replace_all(CONSTWSTR("{id}"), CONSTWSTR("?"));
        desc.replace_all(CONSTWSTR("{module}"), CONSTWSTR("?"));
    } else
    {
        ts::wstr_c pubid;
        if (id > 0)
            pubid = to_wstr(contacts().find_pubid(id));
        if (pubid.is_empty()) pubid = TTT("not yet created or loaded",200); else pubid.insert(0, CONSTWSTR("<ee>"));

        desc.replace_all(CONSTWSTR("{name}"), from_utf8(name));
        desc.replace_all(CONSTWSTR("{id}"), pubid);
        desc.replace_all(CONSTWSTR("{module}"), from_utf8(p->getstr( IS_PROTO_DESCRIPTION ) ));
        idname = p->idname();
    }
    desc.replace_all( CONSTWSTR( "<id>" ), idname );
    return p;
}

void dialog_settings_c::networks_tab_selected()
{
    set_list_emptymessage(CONSTASTR("protoactlist"), ts::wstr_c(TTT("Empty list. Please add networks",278)));
    if (RID lst = find(CONSTASTR("protoactlist")))
    {
        for (auto &row : table_active_protocol_underedit)
            add_active_proto(lst, row.id, row.other);
    }
    ctlenable(CONSTASTR("addnet"), true);
}

/*virtual*/ void dialog_settings_c::avprots_s::done(bool fail)
{
    dialog_settings_c *dlg = (dialog_settings_c *)(((char *)this) - offsetof( dialog_settings_c, available_prots ));
    if (fail)
    {
        DEFERRED_UNIQUE_CALL(0, dlg->get_close_button_handler(), nullptr);
    } else
    {
        if (dlg->is_networks_tab_selected) dlg->networks_tab_selected();
        dlg->proto_list_loaded = true;
    }
}


/*virtual*/ void dialog_settings_c::tabselected(ts::uint32 mask)
{
    if (0 == (mask & MASK_APPLICATION_SETSOUND))
        stop_capture();

    is_networks_tab_selected = false;
    is_video_tab_selected = false;

    if ( mask & MASK_PROFILE_NETWORKS )
    {
        is_networks_tab_selected = true;
        if (proto_list_loaded)
        {
            networks_tab_selected();
        } else
        {
            ctlenable(CONSTASTR("addnet"), false);
            set_list_emptymessage(CONSTASTR("protoactlist"), ts::wstr_c(loc_text(loc_loading)));
            ASSERT( prf().is_loaded() );
            available_prots.load();
        }
    }

    if (mask & MASK_PROFILE_CHAT)
    {
        int enter_key = ctl2send; if (enter_key == EKO_ENTER_NEW_LINE_DOUBLE_ENTER) enter_key = EKO_ENTER_NEW_LINE;
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, ctl2send_handler), enter_key);
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, chat_options), bgroups[BGROUP_CHAT].current);
    }
    if (mask & MASK_PROFILE_MSGSNHIST)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, msgopts_handler), bgroups[ BGROUP_MSGOPTS ].current );
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, histopts_handler), (bgroups[BGROUP_HISTORY].current & 2) ? -1 : -2);
    }

    if (mask & MASK_PROFILE_FILES)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, fileconfirm_handler), fileconfirm);
    }

    if (mask & MASK_PROFILE_COMMON)
    {
        DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, commonopts_handler), bgroups[BGROUP_COMMON2].current);
        ctlenable(CONSTASTR("awayflags4"), 0 != (bgroups[BGROUP_COMMON2].current & 3));
        if (g_app->F_READONLY_MODE())
            ctlenable( CONSTASTR("encrypt1"), false );
        else
            DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, encrypt_handler), as_param(enc_val()));
    }

    if (mask & MASK_PROFILE_NOTIFICATIONS)
    {
        DEFERRED_UNIQUE_CALL( 0.1, DELEGATE( this, notification_handler ), bgroups[ BGROUP_INCOMING_NOTIFY ].current );
    }

    if (mask & MASK_APPLICATION_COMMON)
    {
        select_lang(curlang);

        useproxy_handler(RID(), as_param(useproxyfor));
        proxy_handler(ts::amake(proxy));
        if (RID r = find(CONSTASTR("proxyaddr")))
        {
            gui_textfield_c &tf = HOLD(r).as<gui_textfield_c>();
            tf.set_text( to_wstr(proxy_addr), true );
            check_proxy_addr(proxy, r, proxy_addr);
        }
    }

    if (mask & MASK_APPLICATION_SYSTEM)
    {
        bool allow_edit_autostart = 0 == (startopt & 4);
        ctlenable( CONSTASTR("start1"), allow_edit_autostart );
        ctlenable( CONSTASTR("start2"), allow_edit_autostart && 0 != (startopt & 1) );

        DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, backupopt_handler), backupopt);
    }

    if (mask & MASK_APPLICATION_SETSOUND)
    {
        testrec.clear();
        set_slider_value( CONSTASTR("micvol"), cvtmic.volume );
        start_capture();
    }

    if (mask & MASK_APPLICATION_VIDEO)
    {
        is_video_tab_selected = true;
        ts::wstr_c cid = camera.set(CONSTWSTR("id"));
        ctlenable(CONSTASTR("camera"), video_devices.size() != 0 );
        ctlenable(CONSTASTR("camerares"), video_devices.size() != 0 && !cid.equals(CONSTWSTR("off")));
    }

    if (mask & MASK_APPLICATION_SOUNDS)
    {
        ctlenable(CONSTASTR("applypreset"), selected_preset >= 0);
        DEFERRED_UNIQUE_CALL( 0.01, DELEGATE(this, addlistsound), as_param( snd_count -1 ) );
    }

    setup_video_device();
}

bool dialog_settings_c::addlistsound( RID, GUIPARAM p )
{
    // #snd

    ts::wsptr sdescs[snd_count] = {
        TTT("Incoming message", 1000),
        TTT("Incoming message (current conversation)", 1010),
        TTT("Incoming file", 1001),
        TTT("Incoming folder share", 1014), // max
        TTT("File receiving started", 1002),
        TTT("File received", 1011),
        TTT("Contact online", 1012),
        TTT("Contact offline", 1013),
        TTT("Ringtone", 1003),
        TTT("Ringtone during call", 1004),
        TTT("Call accept", 1005),
        TTT("Call cancel", 1006),
        TTT("Hang up", 1007),
        TTT("Dialing", 1008),
        TTT("New version", 1009),
    };

    int sorted[snd_count] = {
        snd_incoming_message,
        snd_incoming_message2,
        snd_incoming_file,
        snd_folder_share,
        snd_start_recv_file,
        snd_file_received,
        snd_friend_online,
        snd_friend_offline,
        snd_ringtone,
        snd_ringtone2,
        snd_calltone,
        snd_call_accept,
        snd_call_cancel,
        snd_hangup,
        snd_new_version,
    };

    TS_STATIC_CHECK(ARRAY_SIZE(sdescs) == snd_count, "sz");
    TS_STATIC_CHECK(ARRAY_SIZE(sorted) == snd_count, "sz");

    if (RID lst = find(CONSTASTR("soundslist")))
    {
        int sndi = as_int(p);

        gui_vscrollgroup_c &l = HOLD(lst).as<gui_vscrollgroup_c>();

        int ssndi = sorted[sndi];

        ts::wstr_c lit(CONSTWSTR("<b>"));
        lit.append(sdescs[ssndi]);
        lit.append(CONSTWSTR("</b><br=5><p=c>"));
        lit.append(sndselctl[ssndi]).append_char(' ');
        lit.append(sndvolctl[ssndi]);
        lit.append(sndplayctl[ssndi]);
        lit.append(CONSTWSTR("<br=-15> "));

        gui_listitem_c &li = MAKE_CHILD<gui_listitem_c>(lst, lit, ts::amake(ssndi));
        l.getengine().child_move_top(&li.getengine());

        set_edit_value( ts::amake(CONSTASTR("sndfn"), ssndi), sndfn[ssndi] );
        set_slider_value( ts::amake(CONSTASTR("sndvl"), ssndi), sndvol[ssndi] );

        l.scroll_to_begin();
        if (sndi > 0)
            DEFERRED_UNIQUE_CALL( 0.01, DELEGATE(this, addlistsound), as_param( sndi - 1 ) );
    }

    return true;
}

void dialog_settings_c::on_delete_network_2(const ts::str_c&prm)
{
    int id = prm.as_int();
    auto *row = table_active_protocol_underedit.find<true>(id);
    if (ASSERT(row))
    {
        ts::str_c tag(row->other.tag);
        row->deleted();

        if (RID lst = find(CONSTASTR("protoactlist")))
            lst.call_kill_child(tag.insert(0, CONSTASTR("2/")).append_char('/').append_as_int(id));

        ++force_change; mod();
    }
}

bool dialog_settings_c::delete_used_network(RID, GUIPARAM param)
{
    auto *row = table_active_protocol_underedit.find<false>(as_int(param));
    if (ASSERT(row))
    {
        dialog_msgbox_c::mb_warning( TTT("Connection [b]$[/b] in use! All contacts of this connection will be deleted. History of these contacts will be deleted too. Are you still sure?",267) / from_utf8(row->other.name) )
            .on_ok(DELEGATE(this, on_delete_network_2), ts::amake<int>(as_int(param)))
            .bcancel()
            .summon( false );
    }
    return true;
}

void dialog_settings_c::on_delete_network(const ts::str_c& prm)
{
    int id = prm.as_int();
    ts::str_c tag;
    if (contacts().present_protoid( id ))
    {
        DEFERRED_UNIQUE_CALL(0.7, DELEGATE(this, delete_used_network), id);

    } else
    {
        auto *row = table_active_protocol_underedit.find<true>(id);
        if (ASSERT(row))
        {
            tag = row->other.tag;
            row->deleted();

            if (RID lst = find(CONSTASTR("protoactlist")))
                lst.call_kill_child(tag.insert(0, CONSTASTR("2/")).append_char('/').append_as_int(id));

            ++force_change; mod();
        }
    }

}

bool dialog_settings_c::addeditnethandler(dialog_protosetup_params_s &params)
{
    active_protocol_data_s *apd = nullptr;
    int id = 0;
    if ( params.protoid == 0 )
    {
        auto &r = table_active_protocol_underedit.getcreate(0);
        id = r.id;
        params.to(r.other);
        apd = &r.other;
    } else
    {
        auto *row = table_active_protocol_underedit.find<true>( params.protoid );
        apd = &row->other;
        id = params.protoid;
        row->changed();
    }

    apd->user_name = params.uname;
    apd->user_statusmsg = params.ustatus;
    apd->tag = params.networktag;
    apd->name = params.networkname;
    INITFLAG(apd->options, active_protocol_data_s::O_AUTOCONNECT, params.connect_at_startup);
    apd->configurable = params.configurable;

    if (RID lst = find(CONSTASTR("protoactlist")))
    {
        if (params.protoid == 0)
        {
            // new - create list item
            add_active_proto(lst, id, *apd);
        } else
            // find list item and update
            for( rectengine_c *itm : HOLD(lst).engine() )
                if (itm)
                {
                    gui_listitem_c * li = ts::ptr_cast<gui_listitem_c *>(&itm->getrect());
                    ts::token<char> t(li->getparam(), '/');
                    ++t;
                    ++t;
                    if (t->as_int() == params.protoid)
                    {
                        ts::wstr_c desc = make_proto_desc(CONSTWSTR("<id>"), MPD_NAME | MPD_MODULE | MPD_ID);
                        describe_network(desc, apd->name, apd->tag, params.protoid);
                        li->set_text(desc);
                        break;
                    }
                }
    }

    ++force_change; mod();

    return true;
}

ts::str_c dialog_protosetup_params_s::setup_name( const ts::asptr &tag, ts::aint n )
{
    ts::wstr_c name( TTT("Connection $", 63) / ts::to_wstr(tag), CONSTWSTR(" "), ts::wmake(n));
    return to_utf8(name);
}

bool dialog_settings_c::addnetwork(RID, GUIPARAM)
{
    dialog_protosetup_params_s prms( &available_prots, &table_active_protocol_underedit, DELEGATE(this,addeditnethandler));
    prms.configurable.set_defaults();
    prms.configurable.initialized = true;
    prms.connect_at_startup = true;
    prms.watch = getrid();
    SUMMON_DIALOG<dialog_setup_network_c>(UD_PROTOSETUPSETTINGS, false, prms);

    return true;
}

void dialog_settings_c::add_active_proto( RID lst, int id, const active_protocol_data_s &apdata )
{
    ts::wstr_c desc = make_proto_desc( CONSTWSTR("<id>"), MPD_NAME | MPD_MODULE | MPD_ID );
    const protocol_description_s *p = describe_network(desc, apdata.name, apdata.tag, id);

    ts::str_c par( CONSTASTR( "2/" ), apdata.tag, CONSTASTR( "/" ) ); par .append_as_int( id );

    const ts::bitmap_c *icon = p ? &prepare_proto_icon(apdata.tag, p->getstr( IS_PROTO_ICON ).as_sptr(), PROTO_ICON_SIZE, IT_NORMAL) : nullptr;

    MAKE_CHILD<gui_listitem_c>(lst, desc, par) << DELEGATE(this, getcontextmenu) << (const ts::bitmap_c *)icon;
}

void dialog_settings_c::contextmenuhandler( const ts::str_c& param )
{
    ts::token<char> t(param, '/');
    if (*t == CONSTASTR("del"))
    {
        ++t;
        int id = t->as_int();

        if (id < 0)
        {
            on_delete_network( *t );
        } else
        {
            auto *row = table_active_protocol_underedit.find<false>(id);
            if (ASSERT(row))
            {
                dialog_msgbox_c::mb_warning( TTT("Connection [b]$[/b] will be deleted![br]Are you sure?",266) / from_utf8(row->other.name) )
                    .on_ok(DELEGATE(this, on_delete_network), *t)
                    .bcancel()
                    .summon( false );
            }
        }

    } else if (*t == CONSTASTR("props"))
    {
        ++t;

        auto *row = table_active_protocol_underedit.find<true>(t->as_int());
        if (CHECK(row))
        {
            const protocol_description_s *p = available_prots.find(row->other.tag);
            if (CHECK(p))
            {
                dialog_protosetup_params_s prms(DELEGATE(this, addeditnethandler));
                prms.proto_desc = *p;
                prms.networktag = row->other.tag;
                prms.networkname = row->other.name;
                prms.uname = row->other.user_name;
                prms.ustatus = row->other.user_statusmsg;
                prms.protoid = row->id;
                prms.configurable = row->other.configurable;
                prms.connect_at_startup = 0 != (row->other.options & active_protocol_data_s::O_AUTOCONNECT);
                SUMMON_DIALOG<dialog_setup_network_c>(UD_PROTOSETUPSETTINGS, false, prms);
            }
        }

    }
}

menu_c dialog_settings_c::getcontextmenu( const ts::str_c& param, bool activation )
{
    menu_c m;
    ts::token<char> t(param, '/');
    if (*t == CONSTASTR("2"))
    {
        ++t;
        ++t;
        if (activation)
            contextmenuhandler( ts::str_c(CONSTASTR("props/"),*t) );
        else
        {
            m.add(ts::wstr_c(CONSTWSTR("<b>"), TTT("Properties",60)), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("props/"),*t));
            m.add(TTT("Delete",59), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("del/"),*t));
        }
    }

    return m;
}

void dialog_settings_c::smile_pack_selected(const ts::str_c&smp)
{
    smilepack = from_utf8(smp);
    set_combik_menu(CONSTASTR("avasmliepack"), emoti().get_list_smile_pack(smilepack,DELEGATE(this,smile_pack_selected)));
    mod();
}

void dialog_settings_c::select_lang( const ts::str_c& prm )
{
    curlang = prm;
    set_combik_menu(CONSTASTR("langs"), list_langs(curlang,DELEGATE(this,select_lang)));
    mod();
}

/*virtual*/ bool dialog_settings_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    MEMT( MEMT_GUI_COMMON );
    switch (qp)
    {
    case SQ_DRAW:
        if (rid == getrid())
        {
            super::sq_evt(qp, rid, data);
            if ( rekey )
                if (RID b = find(CONSTASTR("dialog_button_1")))
                {
                    ts::wstr_c infot;
                    switch (rekey) //-V719
                    {
                    case dialog_settings_c::REKEY_ENCRYPT:
                        infot = TTT("Profile will be encrypted after [b]Save[/b] button pressed",384);
                        break;
                    case dialog_settings_c::REKEY_REENCRYPT:
                        infot = TTT("Profile will be re-encrypted after [b]Save[/b] button pressed",385);
                        break;
                    case dialog_settings_c::REKEY_REMOVECRYPT:
                        infot = TTT("Encryption will be removed after [b]Save[/b] button pressed",386);
                        break;
                    }

                    ts::wstr_c infostr( CONSTWSTR("<p=r><font=default><l>"), maketag_color<ts::wchar>( gui->imptextcolor ) );
                    infostr.append( infot ).append( CONSTWSTR(" <img=right,-1>") );

                    cri_s inf;
                    children_repos_info(inf);

                    ts::irect br = HOLD(b)().getprops().rect();
                    br.rb.x = br.lt.x - VTAB_OTS;
                    br.lt.x = inf.area.lt.x;

                    draw_data_s &dd = getengine().begin_draw();
                    dd.offset = br.lt;
                    dd.size = br.size();
                    text_draw_params_s tdp;
                    ts::flags32_s f(ts::TO_END_ELLIPSIS | ts::TO_VCENTER);
                    tdp.textoptions = &f;
                    getengine().draw(infostr, tdp);
                    getengine().end_draw();

                }
            return true;
        }
        return false;
    }

    if (super::sq_evt(qp, rid, data)) return true;

    return false;
}

/*virtual*/ void dialog_settings_c::on_confirm()
{
    if (!g_app->F_READONLY_MODE())
    {
        if (REKEY_ENCRYPT == rekey || REKEY_REENCRYPT == rekey)
        {
            // reenter password

            RID epr = SUMMON_DIALOG<dialog_entertext_c>(UD_ENTERPASSWORD, false, dialog_entertext_c::params(
                UD_ENTERPASSWORD,
                gui_isodialog_c::title(title_reenter_password),
                ts::wstr_c(TTT("Please re-enter password to confirm encrypt",387)),
                ts::wstr_c(),
                ts::str_c(),
                DELEGATE(this, re_password_entered),
                DELEGATE(this, re_password_not_entered),
                check_always_ok,
                getrid()));

            return;
        }

        if (REKEY_REMOVECRYPT == rekey)
        {
            // remove encryption
            prf().encrypt(nullptr);
        }
    }


    save_and_close();
}

bool dialog_settings_c::save_and_close(RID, GUIPARAM)
{
    mic_device_changed = false; // to avoid restore in destructor

    if (cfg().language(curlang))
    {
        g_app->load_locale(curlang);
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_LANGUAGE, curlang).send();
    }
    bool fontchanged = false;
    ts::flags64_s::BITS msgopts_changed = 0;
    if (profile_selected)
    {
        prf().ctl_to_send(ctl2send);
        bool ch1 = prf().date_msg_template(date_msg_tmpl);
        ch1 |= prf().date_sep_template(date_sep_tmpl);

        prf().min_history(load_history_count);
        prf().add_history( load_history_count_addition );
        prf().inactive_time( (bgroups[BGROUP_COMMON2].current & 2) ? set_away_on_timer_minutes_value : 0 );
        prf().dmn_duration( desktop_notification_duration );

        msgopts_changed = msgopts_current ^ msgopts_original;

        if (prf().set_options( msgopts_current, msgopts_changed ) || ch1)
            gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS, msgopts_changed).send();

        prf().download_folder(downloadfolder);
        prf().download_folder_images(downloadfolder_images);
        prf().download_folder_fshare(downloadfolder_fshare);
        prf().auto_confirm_masks( auto_download_masks );
        prf().manual_confirm_masks( manual_download_masks );
        prf().fileconfirm( fileconfirm );
        if (prf().emoticons_pack(smilepack))
        {
            emoti().reload();
            gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_EMOJISET).send();
        }
        fontchanged = prf().fontscale_conv_text(font_scale_conv_text) || prf().fontscale_msg_edit(font_scale_msg_edit);

        prf().useproxyfor( useproxyfor );
        prf().misc_flags( pmisc_flags );
    }

    cfg().temp_folder_sendimg( tempfolder_sendimg );
    cfg().temp_folder_handlemsg( tempfolder_handlemsg );

    cfg().folder_backup( backuppath );
    cfg().lim_backup_days(backup_clean_days);
    cfg().lim_backup_size(backup_clean_size);
    cfg().lim_backup_count(backup_clean_count);

    if (is_changed(startopt))
        set_startopts();

    if (proxy > 0 && !check_netaddr(proxy_addr))
        proxy_addr = CONSTASTR(DEFAULT_PROXY);

    cfg().allow_tools(tools_bits);

    cfg().autoupdate(autoupdate);
    cfg().proxy(proxy);
    cfg().proxy_addr(proxy_addr);
    if (oautoupdate != autoupdate && autoupdate > 0)
        g_app->autoupdate_next = ts::now() + 10;


    cfg().collapse_beh(collapse_beh);
    bool sndch = cfg().device_talk(talkdevice);
    sndch |= cfg().device_signal(signaldevice);
    if (sndch) g_app->mediasystem().deinit();

    s3::DEVICE micd = device_from_string(micdevice);
    s3::set_capture_device(&micd);
    stop_capture();
    if (cfg().device_mic(micdevice))
        g_app->capture_device_changed();

    cfg().device_camera(camera.to_str());

    if (is_changed(cvtmic.volume))
    {
        cfg().vol_mic(cvtmic.volume);
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_MICVOLUME).send();
    }

    if (is_changed(talk_vol))
    {
        cfg().vol_talk(talk_vol);
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_TALKVOLUME).send();
    }

    if (is_changed(signal_vol))
        cfg().vol_signal(signal_vol);

    if (cfg().dsp_flags(dsp_flags))
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_DSPFLAGS).send();

    cfg().sounds_flags( mute_all_sounds ? 1 : 0 );
        //gmsg<ISOGM_CHANGED_SETTINGS>( 0, CFG_DSPFLAGS ).send();

    misc_flags = ( misc_flags & 7 ) | ( misc_flags_store & ~7 );
    INITFLAG(misc_flags, MISCF_DONT_BACKUP_PROFILE, 0 == (backupopt & 1));
    INITFLAG(misc_flags, MISCF_DONT_LIM_BACKUP_DAYS, 0 == (backupopt & 2));
    INITFLAG(misc_flags, MISCF_DONT_LIM_BACKUP_SIZE, 0 == (backupopt & 4));
    INITFLAG(misc_flags, MISCF_DONT_LIM_BACKUP_COUNT, 0 == (backupopt & 8));

    cfg().misc_flags( misc_flags );
    gui->enable_special_border( ( misc_flags & MISCF_DISABLEBORDER ) == 0 );


#define SND(s) cfg().snd_##s(sndfn[snd_##s]); cfg().snd_vol_##s(sndvol[snd_##s]);
    SOUNDS
#undef SND


    for (const theme_info_s& thi : m_themes)
    {
        if (thi.current)
        {
            if (thi.folder != cfg().theme())
            {
                cfg().theme(thi.folder);
                g_app->load_theme(thi.folder);
                fontchanged = false;
            }
            break;
        }
    }

    if (cfg().debug(debug.to_str()) || 0!=(msgopts_changed & OPTOPT_POWER_USER))
    {
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_DEBUG).send();
    }

    if (fontchanged)
        g_app->reload_fonts();

    if (profile_selected)
    {
        prf().get_table_active_protocol() = std::move(table_active_protocol_underedit);
        prf().changed(true); // save now, due its OK pressed: user can wait
        g_app->resetup_spelling();
        need2rewarn = false;
    }

    super::on_confirm();
    return true;
}


bool S3CALL dialog_settings_c::enum_capture_devices(s3::DEVICE *device, const s3::wchar *lpcstrDescription, void *lpContext)
{
    ((dialog_settings_c *)lpContext)->enum_capture_devices(device, lpcstrDescription);
    return true;
}
void dialog_settings_c::enum_capture_devices(s3::DEVICE *device, const ts::wchar *lpcstrDescription)
{
    sound_device_s &sd = record_devices.add();
    sd.deviceid = device ? *device : s3::DEFAULT_DEVICE;
    sd.desc.set( ts::wsptr(lpcstrDescription) );
}

void dialog_settings_c::select_audio_capture_device(const ts::str_c& prm)
{
    micdevice = prm;
    s3::DEVICE device = device_from_string(prm);
    s3::set_capture_device(&device);
    set_combik_menu(CONSTASTR("mic"), list_audio_capture_devices());
    s3::start_capture( capturefmt );
    mic_device_changed = true;
    mod();
}
menu_c dialog_settings_c::list_audio_capture_devices()
{
    s3::DEVICE device;
    s3::get_capture_device(&device);

    menu_c m;
    for (const sound_device_s &sd : record_devices)
    {
        ts::uint32 f = 0;
        if (device == sd.deviceid) f = MIF_MARKED;
        m.add( sd.desc, f, DELEGATE(this, select_audio_capture_device), string_from_device(sd.deviceid) );
    }

    return m;
}

bool S3CALL dialog_settings_c::enum_play_devices(s3::DEVICE *device, const s3::wchar *lpcstrDescription, void *lpContext)
{
    ((dialog_settings_c *)lpContext)->enum_play_devices(device, lpcstrDescription);
    return true;
}
void dialog_settings_c::enum_play_devices(s3::DEVICE *device, const ts::wchar *lpcstrDescription)
{
    sound_device_s &sd = play_devices.add();
    sd.deviceid = device ? *device : s3::DEFAULT_DEVICE;
    sd.desc.set(ts::wsptr(lpcstrDescription));
}
menu_c dialog_settings_c::list_talk_devices()
{
    s3::DEVICE curdevice(device_from_string(talkdevice));

    menu_c m;
    for (const sound_device_s &sd : play_devices)
    {
        ts::uint32 f = 0;
        if (curdevice == sd.deviceid) f = MIF_MARKED;
        m.add(sd.desc, f, DELEGATE(this, select_talk_device), string_from_device(sd.deviceid));
    }
    return m;
}
menu_c dialog_settings_c::list_signal_devices()
{
    s3::DEVICE curdevice(device_from_string(signaldevice));

    menu_c m;
    for (const sound_device_s &sd : play_devices)
    {
        ts::uint32 f = 0;
        if (curdevice == sd.deviceid) f = MIF_MARKED;
        ts::wstr_c desc(sd.desc);
        ts::str_c par;
        if (sd.deviceid == s3::DEFAULT_DEVICE)
        {
            desc = TTT("Use speakers",133);
        } else
        {
            par = string_from_device(sd.deviceid);
        }
        m.add(desc, f, DELEGATE(this, select_signal_device), par);
    }
    return m;
}

bool dialog_settings_c::test_talk_device(RID, GUIPARAM)
{
    if (testrec.size())
    {
        int dspf = 0;
        if (0 != (dsp_flags & DSP_SPEAKERS_NOISE)) dspf |= fmt_converter_s::FO_NOISE_REDUCTION;
        if (0 != (dsp_flags & DSP_SPEAKERS_AGC)) dspf |= fmt_converter_s::FO_GAINER;

        media.play_voice((uint64)-1,recfmt,testrec.data(),testrec.size(), talk_vol, dspf, recfmt.bytesToMSec(testrec.size()));
    } else
        media.test_talk( talk_vol );
    return true;
}
bool dialog_settings_c::test_signal_device(RID, GUIPARAM)
{
    media.test_signal( signal_vol );
    return true;
}

void dialog_settings_c::select_talk_device(const ts::str_c& prm)
{
    talkdevice = prm;
    media.init( talkdevice, signaldevice, play_event_s() );

    set_combik_menu(CONSTASTR("talk"), list_talk_devices());
    mod();
}
void dialog_settings_c::select_signal_device(const ts::str_c& prm)
{
    signaldevice = prm;
    media.init(talkdevice, signaldevice, play_event_s() );

    set_combik_menu(CONSTASTR("signal"), list_signal_devices());
    mod();
}

/*virtual*/ bool dialog_settings_c::datahandler( const void *data, int size )
{
    bool mic_level_detected = false;
    if (mic_test_rec)
    {
        struct ss_s
        {
            ts::buf_c *buf;
            float current_level;
            void addb(const s3::Format&f, const void *data, ts::aint size)
            {
                float m = find_max_sample(f, data, size);
                if (m > current_level) current_level = m;
                buf->append_buf(data, size);
            }
        } ss { &testrec, current_mic_level };

        cvtmic.ofmt = recfmt;
        cvtmic.acceptor = DELEGATE(&ss, addb);
        cvtmic.cvt( capturefmt, data, size );

        if (ss.current_level > current_mic_level)
            current_mic_level = ss.current_level;

        mic_level_detected = true;

        if ((ts::Time::current() - mic_test_rec_stop) > 0)
        {
            mic_test_rec = false;
            ctlenable(CONSTASTR("mic"), true);
            ctlenable(CONSTASTR("micrecb"), true);

            ts::wstr_c t(CONSTWSTR("<p=c><b>"));
            t.append( TTT("Test record now available via test speakers button",281) );
            set_label_text(CONSTASTR("soundhint"), t);

        } else
        {
            int iprc = ts::lround((float)(mic_test_rec_stop - ts::Time::current()) * (100.0f / (float)TEST_RECORD_LEN));
            if (iprc < 0) iprc = 0;
            if (iprc > 100) iprc = 100;
            ts::wstr_c prc; prc.set_as_int(100-iprc).append_char('%');
            ts::wstr_c t(CONSTWSTR("<p=c><b><color=155,0,0>"));
            t.append( TTT("Recording test sound...$",282) / prc );

            set_label_text(CONSTASTR("soundhint"), t);
        }
    }

    if (!mic_level_detected)
        current_mic_level = ts::tmax( current_mic_level, ts::CLAMP( find_max_sample(capturefmt, data, size) * cvtmic.volume, 0, 1 ) );

    if ((ts::Time::current() - mic_level_refresh) > 0)
    {
        set_pb_pos(CONSTASTR("micvol"), current_mic_level);
        mic_level_refresh = ts::Time::current() + 100; // 10 fps ought to be enough for anybody
    } else
        current_mic_level *= 0.8; // fade out

    /*

    static bool x = false;
    static s3::Format f;
    if (x)
        media.play_voice(11111, f, data, size);
    else {
        x = true;
        f.channels = 1;
        f.sampleRate = 48000;
        f.bitsPerSample = 16;
        cvter.cvt(capturefmt, data, size, f, DELEGATE(this, datahandler));
        x = false;
    }
    */

    return true;
}

bool dialog_settings_c::micvolset(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    cvtmic.volume = pp->value;

    pp->custom_value_text.set(CONSTWSTR("<l>")).append( TTT("Microphone volume: $",279) / ts::roundstr<ts::wstr_c, float>(pp->value * 100.0f, 1).append_char('%') ).append(CONSTWSTR("</l>"));

    mod();
    return true;
}

bool dialog_settings_c::signalvolset(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    signal_vol = pp->value;
    pp->custom_value_text.set(CONSTWSTR("<l>")).append( TTT("Signal volume: $",283) / ts::roundstr<ts::wstr_c, float>(pp->value * 100.0f, 1).append_char('%') ).append(CONSTWSTR("</l>"));
    mod();
    return true;
}

bool dialog_settings_c::talkvolset(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    talk_vol = pp->value;
    pp->custom_value_text.set(CONSTWSTR("<l>")).append(TTT("Speakers volume: $",284) / ts::roundstr<ts::wstr_c, float>(pp->value * 100.0f, 1).append_char('%')).append(CONSTWSTR("</l>"));
    media.voice_volume((uint64)-1,talk_vol);
    mod();
    return true;
}


bool dialog_settings_c::test_mic(RID, GUIPARAM)
{
    testrec.clear();
    mic_test_rec = true;
    mic_test_rec_stop = ts::Time::current() + TEST_RECORD_LEN;
    recfmt = capturefmt;

    ctlenable(CONSTASTR("mic"), false);
    ctlenable(CONSTASTR("micrecb"), false);

    return true;
}


bool dialog_settings_c::dspf_handler( RID, GUIPARAM p )
{
    int old_voice_dsp = dsp_flags & (DSP_SPEAKERS_NOISE | DSP_SPEAKERS_AGC);
    dsp_flags = as_int(p);

    cvtmic.filter_options.init(fmt_converter_s::FO_NOISE_REDUCTION, FLAG(dsp_flags, DSP_MIC_NOISE));
    cvtmic.filter_options.init(fmt_converter_s::FO_GAINER, FLAG(dsp_flags, DSP_MIC_AGC));

    int dspf = dsp_flags & (DSP_SPEAKERS_NOISE | DSP_SPEAKERS_AGC);;
    if (dspf != old_voice_dsp)
        media.free_voice_channel((uint64)-1);

    mod();
    return true;
}

ts::wstr_c dialog_settings_c::fix_camera_res(const ts::wstr_c &tr )
{
    ts::ivec2 res(0);
    if (tr.get_length()) tr.split(res.x, res.y, ",");

    ts::wstr_c cid = camera.set(CONSTWSTR("id"));
    for (const vsb_descriptor_s &vd : video_devices)
    {
        if (vd.id == cid)
        {
            if (res == ts::ivec2(0) && vd.resolutions.count())
                res = vd.resolutions.get(0);

            for (const ts::ivec2 &r : vd.resolutions)
            {
                if (r == res) return ts::wmake(r.x).append_char(',').append_as_uint(r.y);
            }
            if (vd.resolutions.count())
            {
                res = vd.resolutions.get(0);
                return ts::wmake(res.x).append_char(',').append_as_uint(res.y);
            }

            break;
        }
    }
    return ts::wstr_c();
}

void dialog_settings_c::select_video_capture_device( const ts::str_c& prm )
{
    camera.set( CONSTWSTR("id") ) = from_utf8(prm);
    camera.set(CONSTWSTR("res")) = fix_camera_res(camera.get(CONSTWSTR("res")));
    set_combik_menu(CONSTASTR("camera"), list_video_capture_devices());
    set_combik_menu(CONSTASTR("camerares"), list_video_capture_resolutions());
    ctlenable(CONSTASTR("camerares"), !prm.equals(CONSTASTR("off")));
    mod();

    setup_video_device();
}

void dialog_settings_c::select_video_capture_res(const ts::str_c& prm)
{
    camera.set(CONSTWSTR("res")) = fix_camera_res(to_wstr(prm));
    set_combik_menu(CONSTASTR("camerares"), list_video_capture_resolutions());
    mod();

    setup_video_device();
}

menu_c dialog_settings_c::list_video_capture_devices()
{
    menu_c m;

    if (video_devices.size() == 0)
    {
        m.add(loc_text(loc_loading));
        return m;
    }

    ts::wstr_c cid = camera.set(CONSTWSTR("id"));

    m.add(TTT("Disable video capture",114), cid.equals(CONSTWSTR("off")) ? MIF_MARKED : 0, DELEGATE(this, select_video_capture_device), CONSTASTR("off"));
    m.add_separator();

    for (const vsb_descriptor_s &vd : video_devices)
    {
        ts::uint32 f = 0;
        if (vd.id == cid) f = MIF_MARKED;
        m.add(vd.desc, f, DELEGATE(this, select_video_capture_device), to_utf8(vd.id));
    }

    return m;
}

menu_c dialog_settings_c::list_video_capture_resolutions()
{
    menu_c m;
    ts::wstr_c cid = camera.set(CONSTWSTR("id"));
    ts::str_c res = to_str(fix_camera_res(camera.set(CONSTWSTR("res"))));
    for (const vsb_descriptor_s &vd : video_devices)
    {
        if (vd.id == cid)
        {
            for( const ts::ivec2 &r : vd.resolutions )
            {
                ts::uint32 f = 0;
                ts::str_c tres; tres.append_as_uint(r.x).append_char(',').append_as_uint(r.y);
                if (tres == res) f = MIF_MARKED;
                m.add(ts::wmake(r.x).append(CONSTWSTR(" x ")).append_as_uint(r.y), f, DELEGATE(this, select_video_capture_res), tres);
            }
        }
    }
    if (m.is_empty())
        m.add( TTT("Default resolution",430), MIF_MARKED, DELEGATE(this, select_video_capture_res), CONSTASTR(""));
    return m;
}

void dialog_settings_c::set_video_devices( vsb_list_t &&_video_devices )
{
    video_devices = std::move(_video_devices);

    if (is_video_tab_selected)
        ctlenable(CONSTASTR("camera"), video_devices.size() != 0);

    ts::wstr_c cid = camera.set( CONSTWSTR("id") );
    bool camok = cid.equals(CONSTWSTR("off"));
    if (!camok)
    for (const vsb_descriptor_s &d : video_devices)
    {
        if (d.id == cid)
        {
            camok = true;
            break;
        }
    }

    if (!camok && video_devices.size() > 0)
    {
        camera.set( CONSTWSTR("id") ) = video_devices.get(0).id;
        mod();
    }

    set_combik_menu(CONSTASTR("camera"), list_video_capture_devices());
    set_combik_menu(CONSTASTR("camerares"), list_video_capture_resolutions());
    ctlenable(CONSTASTR("camerares"), !cid.equals(CONSTWSTR("off")));
    setup_video_device();

}

void dialog_settings_c::setup_video_device()
{
    video_device.reset();
    if (!is_video_tab_selected)
        return;

    if (video_devices.size() == 0)
        return;

    struct end_s
    {
        dialog_settings_c *dlg;
        end_s(dialog_settings_c *dlg):dlg(dlg) {}
        ~end_s()
        {
            if (dlg->video_device)
                DEFERRED_UNIQUE_CALL(0, DELEGATE(dlg, drawcamerapanel), nullptr);
        }

    } end(this);

    ts::wstr_c cid = camera.set( CONSTWSTR("id") );
    if (cid.equals(CONSTWSTR("off")))
    {
        getengine().redraw();
        return;
    }
    ts::wstrmap_c camdesc( camera );
    if (cid.begins(CONSTWSTR("desktop"))) camdesc.set( CONSTWSTR("res") ).clear();
    const vsb_descriptor_s *dd = nullptr;
    for (const vsb_descriptor_s &d : video_devices)
    {
        if (d.id == cid)
        {
            video_device.reset( vsb_c::build(d, camdesc) );
            return;
        }
        if (d.id.equals(CONSTWSTR("desktop")))
            dd = &d;
    }
    if (dd)
    {
        initializing_animation.restart();
        video_device.reset(vsb_c::build(*dd, camdesc));
    }
}

bool dialog_settings_c::drawcamerapanel(RID, GUIPARAM p)
{
    if (p == nullptr)
    {
        if (is_video_tab_selected && video_device)
        {
            if (video_device->updated())
            {
                if (RID prid = find(CONSTASTR("preview")))
                {
                    ts::ivec2 dsz = video_device->get_desired_size();
                    if (dsz == ts::ivec2(0) && HOLD(prid))
                    {
                        ts::ivec2 sz( width_for_children(), PREVIEW_HEIGHT);
                        ts::ivec2 shadow_size(0);
                        if (shadow)
                        {
                            shadow_size = ts::ivec2(shadow->clborder_x(), shadow->clborder_y());
                            sz -= shadow_size;
                        }
                        sz = video_device->fit_to_size(sz);

                        sz += shadow_size;
                        gui_panel_c &pnl = HOLD(prid).as<gui_panel_c>();
                        pnl.set_min_size(sz);
                        pnl.set_max_size(sz);
                        MODIFY(prid).size(sz);
                        gui->repos_children(this);
                    }
                    HOLD(prid).engine().redraw();
                }
            } else if (video_device->still_initializing())
            {
                if (RID prid = find(CONSTASTR("preview")))
                    HOLD(prid).engine().redraw();
            }

            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, drawcamerapanel), nullptr);
        }
        return true;
    }

    rectengine_c *e = (rectengine_c *)p;

    //e->draw( ts::irect(0,0,100,100), ts::ARGB(255,0,0) );
    if (video_device)
    {
        if (video_device->still_initializing())
        {
            initializing_animation.render();
            ts::wstr_c ainfo( loc_text(loc_initialization) );
            if (video_device->is_busy()) ainfo.append(CONSTWSTR("<br>")).append(loc_text(loc_camerabusy) );
            draw_initialization(e, initializing_animation.bmp, e->getrect().getprops().szrect(), get_default_text_color(), ainfo );

        } else if (ts::drawable_bitmap_c *b = video_device->lockbuf(nullptr))
        {
            ts::ivec2 dsz = video_device->get_desired_size();
            if (dsz >>= b->info().sz)
            {
                e->begin_draw();
                ts::ivec2 sz = e->getrect().getprops().size();
                ts::ivec2 pos = (sz - dsz) / 2;
                if (shadow)
                    e->draw(*shadow, DTHRO_BORDER);

                e->draw(pos, b->extbody(), false);
                e->end_draw();
            }

            video_device->unlock(b);
        }
    }

    return true;
}



// proto setup

dialog_setup_network_c::dialog_setup_network_c(MAKE_ROOT<dialog_setup_network_c> &data) :gui_isodialog_c(data), params(data.prms), watch( data.prms.watch, DELEGATE(this, lost_contact), nullptr )
{
    if (params.protoid && !params.confirm)
        if (active_protocol_c *ap = prf().ap(params.protoid))
        {
            params.uname = ap->get_uname();
            params.ustatus = ap->get_ustatusmsg();
            params.networkname = ap->get_name();
        }

    if (params.avprotos && !params.networktag.is_empty() && params.protocols)
    {
        // generate name

        if (const protocol_description_s *p = params.avprotos->find(params.networktag))
        {
            ts::aint n = params.protocols->rows.size() + 1;
        again:
            for (auto &row : *params.protocols)
            {
                if (row.other.name.extract_numbers().as_int() == n)
                {
                    ++n;
                    goto again;
                }
            }

            params.networkname = dialog_protosetup_params_s::setup_name(params.networktag, n);
        }
    }
}

dialog_setup_network_c::~dialog_setup_network_c()
{
    //gui->delete_event(DELEGATE(this, refresh_current_page));
}

bool dialog_setup_network_c::lost_contact(RID, GUIPARAM p)
{
    TSDEL(this);
    return true;
}

/*virtual*/ void dialog_setup_network_c::created()
{
    set_theme_rect(CONSTASTR("setupnetw"), false);
    super::created();
    tabsel(CONSTASTR("1"));
}

bool dialog_setup_network_c::b_advanced(RID, GUIPARAM)
{
    SUMMON_DIALOG<dialog_advanced_c>(UD_ADVANCED_NETWORK_SETTINGS, false, this);
    return true;
}

ts::str_c dialog_setup_network_c::adv_param(const ts::asptr &pn) const
{
    return params.configurable.advanced.get(pn);
}

void dialog_setup_network_c::adv_param(const ts::asptr &pn, const ts::str_c &pv)
{
    params.configurable.advanced.set(pn) = pv;
}

ts::wstr_c dialog_setup_network_c::prototag() const
{
    if (!params.networktag.is_empty())
        return ts::to_wstr(params.networktag);

    if (params.protoid != 0)
    {
        if (active_protocol_c *ap = prf().ap(params.protoid))
            return ts::to_wstr(ap->get_tag());
    }

    return ts::wstr_c();
}

ts::str_c dialog_setup_network_c::get_advanced()
{
    if (!params.networktag.is_empty())
    {
        if (params.avprotos)
        {
            for (const protocol_description_s&proto : *params.avprotos)
            {
                if (params.networktag.equals(proto.getstr(IS_PROTO_TAG)))
                    return proto.getstr(IS_ADVANCED_SETTINGS);
            }
        }
    }

    if (params.protoid != 0)
    {
        if (active_protocol_c *ap = prf().ap(params.protoid))
            return ap->get_infostr(IS_ADVANCED_SETTINGS);
    }

    return ts::str_c();
}


void dialog_setup_network_c::getbutton(bcreate_s &bcr)
{
    super::getbutton(bcr);

    if (!get_advanced().is_empty())
    {
        if (bcr.tag == 2)
        {
            bcr.face = BUTTON_FACE(button);
            bcr.btext = TTT("Advanced", 265);
            bcr.handler = DELEGATE(this, b_advanced);
        }
    }
}

void dialog_setup_network_c::available_network_selected(const ts::str_c&tag)
{
    params.networktag = tag;
    if (!tag.is_empty())
        if (const protocol_description_s *p = params.avprotos->find(params.networktag))
            params.proto_desc = *p;

    predie = true;
    SUMMON_DIALOG<dialog_setup_network_c>(UD_PROTOSETUPSETTINGS, false, params);
    TSDEL( this );
}

menu_c dialog_setup_network_c::get_list_avaialble_networks()
{
    menu_c m;

    if ( params.avprotos )
    {
        m.add(TTT("(not selected)", 201), params.networktag.is_empty() ? MIF_MARKED : 0, DELEGATE(this, available_network_selected));
        bool sep = false;
        for (const protocol_description_s&proto : *params.avprotos)
        {
            if (!sep) m.add_separator();
            sep = true;
            m.add(from_utf8(proto.getstr( IS_PROTO_DESCRIPTION ) ), params.networktag.equals(proto.getstr( IS_PROTO_TAG ) ) ? MIF_MARKED : 0, DELEGATE(this, available_network_selected), proto.getstr( IS_PROTO_TAG ) );
        }
    }

    return m;
}



/*virtual*/ int dialog_setup_network_c::additions(ts::irect &)
{
    addh = 0;
    descmaker dm(this);
    dm << 1;

    ts::wstr_c hdr(CONSTWSTR("<l>"));

    bool newnet = params.avprotos != nullptr;
    bool addheader = true;
    if (params.protoid < 0)
    {
        hdr.append(from_utf8(params.proto_desc.getstr(IS_PROTO_DESCRIPTION_WITH_TAG)));
        newnet = false;

    } else if (params.protoid)
        if (active_protocol_c *ap = prf().ap(params.protoid))
            hdr.append(from_utf8(ap->get_infostr(IS_PROTO_DESCRIPTION_WITH_TAG))), newnet = false;

    if (newnet)
    {
        addheader = false;
    }
    if (addheader)
    {
        hdr.append(CONSTWSTR("</l>"));
        dm().page_caption(hdr);
    }

    if (params.avprotos)
    {
        if (!addheader)
            dm().vspace(15);

        dm().combik(TTT("Select network", 62)).setmenu(get_list_avaialble_networks()).setname(CONSTASTR("availablenets"));
        if (params.networktag.is_empty())
        {
            ctlenable(CONSTASTR("dialog_button_1"), false);
            return 0;
        }
        addh += 30;
    }

    dm().vspace(15);
    dm().textfield(TTT("Network connection name",70), from_utf8(params.networkname), DELEGATE(this, netname_edit)).focus(true);
    dm().vspace();
    dm().textfield(TTT("Your name on this network",261), from_utf8(params.uname), DELEGATE(this, uname_edit));
    dm().vspace();
    dm().textfield(TTT("Your status on this network",262), from_utf8(params.ustatus), DELEGATE(this, ustatus_edit));
    dm().vspace();
    addh += 100;

    if ( 0 != ( params.proto_desc.features & PF_NEW_REQUIRES_LOGIN ) )
    {
        dm().textfield( params.proto_desc.idname(), from_utf8( params.configurable.login ), DELEGATE( this, login_edit ) ).setname( CONSTASTR( "loginedit" ) );
        dm().vspace();

        ts::str_c pwd;
        password_ok = params.configurable.get_password_decoded(pwd);

        dm().textfield( TTT("Password",103), from_utf8( password_changed ? pwd : ts::str_c() ), DELEGATE( this, password_edit ) ).passwd(true).setname( CONSTASTR( "passedit" ) );
        dm().vspace();
        addh += 100;
        ctlenable( CONSTASTR( "dialog_button_1" ), !params.configurable.login.is_empty() );
    }

    if ( params.confirm )
    {
        dm().checkb(ts::wstr_c(), DELEGATE(this, network_connect), params.connect_at_startup ? 1 : 0).setmenu(
            menu_c().add(TTT("Connect at startup",204), 0, MENUHANDLER(), CONSTASTR("1"))  );

        addh += 30;
    }

    ts::wsptr opnames[auto_co_count] =
    {
        TTT("Allow IPv6",361),
        TTT("Allow UDP",264),
        TTT("Allow only encrypted connection",523),
        TTT("Allow only trusted connection",524),
    };

    TS_STATIC_CHECK(auto_co_count == 4, "Please describe new options");

#define COPDEF( nnn, dv ) if (0 != (params.proto_desc.connection_features & CF_##nnn)) \
    { \
        ASSERT(params.configurable.initialized); \
        addh += 30; \
        dm().checkb(ts::wstr_c(), DELEGATE(this, network_##nnn), params.configurable.nnn ? 1 : 0).setmenu( \
        menu_c().add(opnames[auto_co_##nnn], 0, MENUHANDLER(), CONSTASTR("1"))); \
    }
    CONN_OPTIONS
#undef COPDEF

    if (0 != (params.proto_desc.connection_features & CF_PROXY_MASK))
    {
        ASSERT(params.configurable.initialized);
        addh += 55;

        int pt = 0;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_HTTP) pt = 1;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_HTTPS) pt = 2;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_SOCKS4) pt = 3;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_SOCKS5) pt = 4;
        ts::wstr_c pa = to_wstr(params.configurable.proxy.proxy_addr);

        dm().vspace(5);

        dm().hgroup(TTT("Connection settings",169));
        dm().combik(HGROUP_MEMBER).setmenu(list_proxy_types(pt, DELEGATE(this, set_proxy_type_handler), params.proto_desc.connection_features & CF_PROXY_MASK)).setname(CONSTASTR("protoproxytype"));
        dm().textfield(HGROUP_MEMBER, ts::to_wstr(params.configurable.proxy.proxy_addr), DELEGATE(this, set_proxy_addr_handler)).setname(CONSTASTR("protoproxyaddr"));
    }

    if (params.protoid == 0 && 0 != (params.proto_desc.features & PF_IMPORT))
    {
        dm().vspace(5);
        addh += 45;

        ts::wstr_c iroot( ts::from_utf8( params.proto_desc.getstr( IS_IMPORT_PATH ) ) );
        ts::parse_env(iroot);
        ts::fix_path(iroot, FNO_APPENDSLASH);

        dm().file(loc_text(loc_import_from_file), iroot, params.importcfg, DELEGATE(this, network_importfile));
    }

    return 0;
}

bool dialog_setup_network_c::uname_edit(const ts::wstr_c &t, bool )
{
    params.uname = to_utf8(t);
    return true;
}

bool dialog_setup_network_c::ustatus_edit(const ts::wstr_c &t, bool )
{
    params.ustatus = to_utf8(t);
    return true;
}

bool dialog_setup_network_c::netname_edit(const ts::wstr_c &t, bool )
{
    params.networkname = to_utf8(t);
    return true;
}

bool dialog_setup_network_c::login_edit( const ts::wstr_c &t, bool )
{
    params.configurable.login = to_utf8( t );
    ctlenable( CONSTASTR("dialog_button_1"), !params.configurable.login.is_empty() );

    if ( RID r = find( CONSTASTR( "loginedit" ) ) )
    {
        gui_textfield_c &tf = HOLD( r ).as<gui_textfield_c>();
        tf.badvalue( params.configurable.login.is_empty() );
    }

    return true;
}

bool dialog_setup_network_c::password_edit( const ts::wstr_c &t, bool )
{
    params.configurable.set_password( to_utf8( t ) );
    password_changed = true;
    password_ok = true;

    if ( RID r = find( CONSTASTR( "passedit" ) ) )
    {
        gui_textfield_c &tf = HOLD( r ).as<gui_textfield_c>();
        tf.badvalue( false );
    }

    return true;
}

/*virtual*/ bool dialog_setup_network_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (super::sq_evt(qp, rid, data)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

/*virtual*/ void dialog_setup_network_c::on_confirm()
{
    if (params.confirm)
    {
        if (!params.confirm(params))
            return;
    } else if (params.protoid)
    {
        if (active_protocol_c *ap = prf().ap(params.protoid))
        {
            if (!params.uname.equals(ap->get_uname())) gmsg<ISOGM_CHANGED_SETTINGS>(params.protoid, PP_USERNAME, params.uname).send();
            if (!params.ustatus.equals(ap->get_ustatusmsg())) gmsg<ISOGM_CHANGED_SETTINGS>(params.protoid, PP_USERSTATUSMSG, params.ustatus).send();
            if (!params.networkname.equals(ap->get_name())) gmsg<ISOGM_CHANGED_SETTINGS>(params.protoid, PP_NETWORKNAME, params.networkname).send();
            if (params.configurable.initialized)
                ap->set_configurable(params.configurable);
        }
    }

    super::on_confirm();
}

/*virtual*/ ts::wstr_c dialog_setup_network_c::get_name() const
{
    ts::wstr_c n( title(params.avprotos ? title_new_network : title_connection_properties) );
    ts::wstr_c nn = gui_dialog_c::get_name();
    if (!nn.is_empty())
        n.append(CONSTWSTR(" (")).append(nn).append_char(')');
    return n;

}
/*virtual*/ ts::ivec2 dialog_setup_network_c::get_min_size() const
{
    return ts::ivec2(400, 200 + addh);
}

bool dialog_setup_network_c::network_importfile(const ts::wstr_c & t, bool )
{
    params.importcfg = t;
    return true;
}

#define COPDEF( nnn, dv ) bool dialog_setup_network_c::network_##nnn(RID, GUIPARAM p) \
{ \
    params.configurable.initialized = true;  params.configurable.nnn = p != nullptr; return true; \
}
CONN_OPTIONS
#undef COPDEF

bool dialog_setup_network_c::network_connect(RID, GUIPARAM p)
{
    params.connect_at_startup = p != nullptr;
    return true;
}

void dialog_setup_network_c::set_proxy_type_handler(const ts::str_c& p)
{
    params.configurable.initialized = true;

    int psel = p.as_int();
    if (psel == 0) params.configurable.proxy.proxy_type = 0;
    else if (psel == 1) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_HTTP;
    else if (psel == 2) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_HTTPS;
    else if (psel == 3) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_SOCKS4;
    else if (psel == 4) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_SOCKS5;

    set_combik_menu(CONSTASTR("protoproxytype"), list_proxy_types(psel, DELEGATE(this, set_proxy_type_handler), params.proto_desc.connection_features & CF_PROXY_MASK));
    if (RID r = find(CONSTASTR("protoproxyaddr")))
    {
        check_proxy_addr(params.configurable.proxy.proxy_type, r, params.configurable.proxy.proxy_addr);
        r.call_enable(psel > 0);
    }
}

bool dialog_setup_network_c::set_proxy_addr_handler(const ts::wstr_c & t, bool )
{
    params.configurable.initialized = true;
    params.configurable.proxy.proxy_addr = to_str(t);

    if (RID r = find(CONSTASTR("protoproxyaddr")))
        check_proxy_addr(params.configurable.proxy.proxy_type, r, params.configurable.proxy.proxy_addr);
    return true;
}

/*virtual*/ void dialog_setup_network_c::tabselected(ts::uint32 mask)
{
    if (RID r = find(CONSTASTR("protoproxyaddr")))
    {
        check_proxy_addr(params.configurable.proxy.proxy_type, r, params.configurable.proxy.proxy_addr);
        r.call_enable(params.configurable.proxy.proxy_type != 0);
    }
    if (RID r = find(CONSTASTR("loginedit")))
    {
        gui_textfield_c &tf = HOLD( r ).as<gui_textfield_c>();
        tf.badvalue( params.configurable.login.is_empty() );
    }
    if (RID r = find(CONSTASTR("passedit")))
    {
        gui_textfield_c &tf = HOLD( r ).as<gui_textfield_c>();
        tf.badvalue( !password_ok );
        if ( !password_ok )
            tf.set_placeholder( TOOLTIP( TTT("Enter password again",473) ), get_default_text_color( 0 ) );
        else if ( !password_changed && !params.configurable.get_password_encoded().is_empty() )
            tf.set_placeholder( TOOLTIP( TTT("Unchanged",474) ), get_default_text_color( 0 ) );
    }
}


dialog_protosetup_params_s::dialog_protosetup_params_s(int protoid) : protoid(protoid)
{
    if (active_protocol_c *ap = prf().ap(protoid))
    {
        proto_desc = ap->proto_desc();
        configurable = ap->get_configurable();
        if (configurable.proxy.proxy_addr.is_empty())
            configurable.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);
    }
}

void dialog_protosetup_params_s::to( active_protocol_data_s &d ) const
{
    d.tag = networktag;
    RESETFLAG( d.options, active_protocol_data_s::O_CONFIG_NATIVE );
    if ( !importcfg.is_empty() )
        if ( d.config.load_from_disk_file( importcfg ) )
            d.options |= active_protocol_data_s::O_CONFIG_NATIVE;
}


