#include "isotoxin.h"

//-V:theme:807

application_c *g_app = nullptr;

#ifndef _FINAL
void dotests();
#endif

static bool __toggle_search_bar(RID, GUIPARAM)
{
    bool sbshow = prf().get_options().is(UIOPT_SHOW_SEARCH_BAR);
    prf().set_options( sbshow ? 0 : UIOPT_SHOW_SEARCH_BAR, UIOPT_SHOW_SEARCH_BAR );
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS).send();
    return true;
}

application_c::application_c(const ts::wchar * cmdl, bool minimize)
{
    F_NEWVERSION = false;
    F_UNREADICONFLASH = false;
    F_UNREADICON = false;
    F_NEEDFLASH = false;
    F_FLASHIP = false;
    F_SETNOTIFYICON = false;
    F_OFFLINE_ICON = true;
    F_ALLOW_AUTOUPDATE = false;

    autoupdate_next = now() + 10;
	g_app = this;
    cfg().load();
    if (cfg().is_loaded())
        summon_main_rect(minimize);

#ifndef _FINAL
    dotests();
#endif

    register_kbd_callback( __toggle_search_bar, HOTKEY_TOGGLE_SEARCH_BAR );
}


application_c::~application_c()
{
	g_app = nullptr;
}

ts::uint32 application_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SAVED>&t )
{
    if (t.tabi == pt_history)
        m_locked_recalc_unread.clear();
    return 0;
}

ts::uint32 application_c::gm_handler(gmsg<GM_UI_EVENT> & e)
{
    if (UE_MAXIMIZED == e.evt || UE_NORMALIZED == e.evt)
        picture_animated_c::allow_tick = true;
    else if (UE_MINIMIZED == e.evt)
        picture_animated_c::allow_tick = false;

    return 0;
}



DWORD application_c::handler_SEV_EXIT( const system_event_param_s & p )
{
    prf().shutdown_aps();
	TSDEL(this);
    return 0;
}

void application_c::load_locale( const SLANGID& lng )
{
    m_locale.clear();
    m_locale_lng.clear();
    m_locale_tag = lng;
    ts::wstrings_c fns;
    ts::wstr_c path(CONSTWSTR("loc/"));
    int cl = path.get_length();

    ts::g_fileop->find(fns, path.appendcvt(lng).append(CONSTWSTR(".*.lng")), false);
    fns.kill_dups();
    fns.sort(true);

    ts::wstrings_c ps;
    for(const ts::wstr_c &f : fns)
    {
        path.set_length(cl).append(f);
        ts::parse_text_file(path,ps,true);
        for (const ts::wstr_c &ls : ps)
        {
            ts::token<ts::wchar> t(ls,'=');
            ts::pwstr_c stag = *t;
            int tag = t->as_int(-1);
            ++t;
            ts::wstr_c l(*t); l.trim();
            if (tag > 0)
            {
                l.replace_all('<', '\1');
                l.replace_all('>', '\2');
                l.replace_all(CONSTWSTR("\1"), CONSTWSTR("<char=60>"));
                l.replace_all(CONSTWSTR("\2"), CONSTWSTR("<char=62>"));
                l.replace_all(CONSTWSTR("[br]"), CONSTWSTR("<br>"));
                l.replace_all(CONSTWSTR("[b]"), CONSTWSTR("<b>"));
                l.replace_all(CONSTWSTR("[/b]"), CONSTWSTR("</b>"));
                l.replace_all(CONSTWSTR("[l]"), CONSTWSTR("<l>"));
                l.replace_all(CONSTWSTR("[/l]"), CONSTWSTR("</l>"));
                l.replace_all(CONSTWSTR("[quote]"), CONSTWSTR("\""));
                l.replace_all(CONSTWSTR("[appname]"), CONSTWSTR(APPNAME));
                l.replace_all(CONSTWSTR(APPNAME), ts::wstr_c(CONSTWSTR("<color=0,50,0><b>")).append(CONSTWSTR(APPNAME)).append(CONSTWSTR("</b></color>")));

                int nbr = l.find_pos(CONSTWSTR("[nbr]"));
                if (nbr >= 0)
                {
                    int nbr2 = l.find_pos(nbr + 5, CONSTWSTR("[/nbr]"));
                    if (nbr2 > nbr)
                    {
                        ts::wstr_c nbsp = l.substr(nbr+5, nbr2);
                        nbsp.replace_all(CONSTWSTR(" "), CONSTWSTR("<nbsp>"));
                        l.replace(nbr, nbr2-nbr+6, nbsp);
                    }
                }

                m_locale[tag] = l;
            } else if (stag.get_length() == 2 && !l.is_empty())
            {
                m_locale_lng[SLANGID(to_str(stag))] = l;
            }
        }
    }
}

bool application_c::b_send_message(RID r, GUIPARAM param)
{
    gmsg<ISOGM_SEND_MESSAGE>().send().is(GMRBIT_ACCEPTED);
    return true;
}

bool application_c::flash_notification_icon(RID r, GUIPARAM param)
{
    F_UNREADICON = F_NEEDFLASH ? true : gmsg<ISOGM_SOMEUNREAD>().send().is(GMRBIT_ACCEPTED);;
    F_NEEDFLASH = false;
    F_FLASHIP = false;
    if (F_UNREADICON)
    {
        F_FLASHIP = true;
        F_UNREADICONFLASH = !F_UNREADICONFLASH;
        DEFERRED_UNIQUE_CALL(0.3, DELEGATE(this, flash_notification_icon), 0);
    }
    F_SETNOTIFYICON = true;
    for(RID r : m_flashredraw)
    {
        HOLD rr(r);
        if (rr) rr.engine().redraw();
    }
    m_flashredraw.clear();
    return true;
}

HICON application_c::app_icon(bool for_tray)
{
    if (!for_tray)
        return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(IDI_ICON)); 

    if (F_UNREADICON)
        return LoadIcon(g_sysconf.instance, F_UNREADICONFLASH ? MAKEINTRESOURCE(F_OFFLINE_ICON ? IDI_ICON_OFFLINE : IDI_ICON2) : MAKEINTRESOURCE(IDI_ICON_HOLLOW));

    return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(F_OFFLINE_ICON ? IDI_ICON_OFFLINE : IDI_ICON));
};

/*virtual*/ void application_c::app_prepare_text_for_copy(ts::str_c &text)
{
    int rr = text.find_pos(CONSTASTR("<r>"));
    if (rr >= 0)
    {
        int rr2 = text.find_pos(rr+3, CONSTASTR("</r>"));
        if (rr2 > rr)
            text.cut(rr, rr2-rr+4);
    }

    text.replace_all(CONSTASTR("<char=60>"), CONSTASTR("\2"));
    text.replace_all(CONSTASTR("<char=62>"), CONSTASTR("\3"));
    text.replace_all(CONSTASTR("<br>"), CONSTASTR("\n"));
    text.replace_all(CONSTASTR("<p>"), CONSTASTR("\n"));

    text_convert_to_bbcode(text);
    text_close_bbcode(text);
    text_convert_char_tags(text);

    // unparse smiles
    auto t = CONSTASTR("<rect=");
    for (int i = text.find_pos(t); i >= 0; i = text.find_pos(i + 1, t))
    {
        int j = text.find_pos(i + t.l, '>');
        if (j < 0) break;
        int k = text.substr(i + t.l, j).find_pos(0, ',');
        if (k < 0) break;
        int emoi = text.substr(i + t.l, i + t.l + k).as_int(-1);
        if (const emoticon_s *e = emoti().get(emoi))
        {
            text.replace( i, j-i+1, e->def );
        } else
            break;
    }

    text_remove_tags(text);

    text.replace_all('\2', '<');
    text.replace_all('\3', '>');

    text.replace_all(CONSTASTR("\r\n"), CONSTASTR("\n"));
    text.replace_all(CONSTASTR("\n"), CONSTASTR("\r\n"));
}

/*virtual*/ ts::wsptr application_c::app_loclabel(loc_label_e ll)
{
    switch (ll)
    {
        case LL_CTXMENU_COPY: return TTT("Copy",92);
        case LL_CTXMENU_CUT: return TTT("Cut",93);
        case LL_CTXMENU_PASTE: return TTT("Paste",94);
        case LL_CTXMENU_DELETE: return TTT("Delete",95);
        case LL_CTXMENU_SELALL: return TTT("Select all",96);
        case LL_ABTT_CLOSE:
            if (cfg().collapse_beh() == 2)
            {
                return TTT("Minimize to notification area[br](Hold Ctrl key to close)",122);
            }
            return TTT("Close",3);
        case LL_ABTT_MAXIMIZE: return TTT("Expand",4);
        case LL_ABTT_NORMALIZE: return TTT("Normal size",5);
        case LL_ABTT_MINIMIZE:
            if (cfg().collapse_beh() == 1)
                return TTT("Minimize to notification area",123);
            return TTT("Minimize",6);
    }
    return __super::app_loclabel(ll);
}

/*virtual*/ void application_c::app_b_minimize(RID main)
{
    if (cfg().collapse_beh() == 1)
        MODIFY(main).micromize(true);
    else
        __super::app_b_minimize(main);
}
/*virtual*/ void application_c::app_b_close(RID main)
{
    if (GetKeyState(VK_CONTROL) >= 0 && cfg().collapse_beh() == 2)
        MODIFY(main).micromize(true);
    else
        __super::app_b_close(main);
}
/*virtual*/ void application_c::app_path_expand_env(ts::wstr_c &path)
{
    path_expand_env(path);
}

ts::static_setup<spinlock::syncvar<autoupdate_params_s>,1000> auparams;

void autoupdater();
static DWORD WINAPI autoupdater(LPVOID)
{
    UNSTABLE_CODE_PROLOG
    autoupdater();
    UNSTABLE_CODE_EPILOG
    return 0;
}

/*virtual*/ void application_c::app_5second_event()
{
    enum
    {
        OST_UNKNOWN,
        OST_OFFLINE,
        OST_ONLINE,
    } st = OST_UNKNOWN;
    
    prf().iterate_aps([&](const active_protocol_c &ap) {
        
        if ( 0 != (ap.get_features() & PF_OFFLINE_INDICATOR) )
        {
            bool onlflg = ap.is_current_online();
            st = onlflg ? OST_ONLINE : OST_OFFLINE;
        } else if (st == OST_UNKNOWN)
        {
            bool onlflg = ap.is_current_online();
            st = onlflg ? OST_ONLINE : OST_OFFLINE;
        }
    });

    F_OFFLINE_ICON = OST_ONLINE != st;
    F_SETNOTIFYICON = true; // once per 5 seconds do icon refresh

    if ( F_ALLOW_AUTOUPDATE && cfg().autoupdate() > 0 )
    {
        if (now() > autoupdate_next)
        {
            time_t nextupdate = F_OFFLINE_ICON ? SEC_PER_HOUR : SEC_PER_DAY; // do every-hour check, if offline (it seems no internet connection)

            autoupdate_next += nextupdate;
            if (autoupdate_next <= now() )
                autoupdate_next = now() + nextupdate;

            b_update_ver(RID(),as_param(AUB_DEFAULT));
        }
        if (!nonewversion())
        {
            if (!newversion() && new_version())
                gmsg<ISOGM_NEWVERSION>(cfg().autoupdate_newver()).send();
            nonewversion(true);
        }
    }

    for( auto &row : prf().get_table_unfinished_file_transfer() )
    {
        if (row.other.upload)
        {
            if (find_file_transfer(row.other.utag))
                continue;

            contact_c *sender = contacts().find( row.other.sender );
            if (!sender)
            {
                row.deleted();
                prf().changed();
                break;
            }
            contact_c *historian = contacts().find( row.other.historian );
            if (!historian)
            {
                row.deleted();
                prf().changed();
                break;
            }
            if (ts::is_file_exists(row.other.filename))
            {
                if (sender->get_state() != CS_ONLINE) break;
                g_app->register_file_transfer(historian->getkey(), sender->getkey(), row.other.utag, row.other.filename, 0);
                break;

            } else if (row.deleted())
            {
                prf().changed();
                break;
            }
        }
    }

    if (manual_cos == COS_ONLINE)
    {
        contact_online_state_e c = contacts().get_self().get_ostate();
        contact_online_state_e cnew = COS_ONLINE;

        if (prf().get_options().is(UIOPT_AWAYONSCRSAVER))
        {
            BOOL scrsvrun = FALSE;
            SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &scrsvrun, 0);
            if (scrsvrun) cnew = COS_AWAY;
        }

        int imins = prf().inactive_time();
        if (imins > 0)
        {
            LASTINPUTINFO lii = { (UINT)sizeof(LASTINPUTINFO) };
            GetLastInputInfo(&lii);
            int cimins = (GetTickCount() - lii.dwTime) / 60000;
            if (cimins >= imins)
                cnew = COS_AWAY;
        }

        if (c != cnew)
            set_status(cnew, false);
    }
}

void application_c::set_status(contact_online_state_e cos_, bool manual)
{
    if (manual)
        manual_cos = cos_;

    contacts().get_self().subiterate([&](contact_c *c) { //-V807
        c->set_ostate(cos_);
    });
    contacts().get_self().set_ostate(cos_);
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_ONLINESTATUS).send();

}

/*virtual*/ void application_c::app_loop_event()
{
    if (m_need_recalc_unread.count())
    {
        LOG( "m_need_recalc_unread" << m_need_recalc_unread.count() );

        contact_key_s ck = m_need_recalc_unread.get(0);
        if (m_locked_recalc_unread.find_index(ck) >= 0)
        {
            LOG("locked:" << ck);

            // locked. postpone
            m_need_recalc_unread.remove_slow(0);
            m_need_recalc_unread.add(ck);
        } else
        {
            LOG("recalc:" << ck);

            contact_c *c = contacts().find(ck);
            m_need_recalc_unread.remove_slow(0);
            if (c) F_NEEDFLASH |= c->recalc_unread();
        }
    }

    if (F_NEEDFLASH && !F_FLASHIP)
        flash_notification_icon();

    if (F_SETNOTIFYICON)
    {
        set_notification_icon();
        F_SETNOTIFYICON = false;
    }

    picture_animated_c::tick();
    m_tasks_executor.tick();
    resend_undelivered_messages();
}

/*virtual*/ void application_c::app_fix_sleep_value(int &sleep_ms)
{
    UNSTABLE_CODE_PROLOG

    if (s3::is_capturing())
    {
        struct x
        {
            static void datacaptureaccept(const void *data, int size, void * /*context*/)
            {
                g_app->handle_sound_capture(data, size);
            }
        };

        s3::capture_tick(x::datacaptureaccept, nullptr);
        sleep_ms = 1;
    }

    UNSTABLE_CODE_EPILOG
        
}

/*virtual*/ void application_c::app_notification_icon_action(naction_e act, RID iconowner)
{
    HOLD m(iconowner);

    if (act == NIA_L2CLICK)
    {
        if (m().getprops().is_collapsed())
            MODIFY(iconowner).decollapse();
        else
            MODIFY(iconowner).micromize(true);
    } else if (act == NIA_RCLICK)
    {
        struct handlers
        {
            static void m_exit(const ts::str_c&cks)
            {
                sys_exit(0);
            }
        };

        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            
            menu_c m;

            add_status_items(m);

            m.add_separator();
            m.add(TTT("Exit",117), 0, handlers::m_exit);
            gui_popup_menu_c::show(menu_anchor_s(true, menu_anchor_s::RELPOS_TYPE_3), m, true);
            g_app->set_notification_icon(); // just remove hint

        DEFERRED_EXECUTION_BLOCK_END(0)
    }
}

bool application_c::b_customize(RID r, GUIPARAM param)
{
    struct handlers
    {
        static void m_settings(const ts::str_c&)
        {
            SUMMON_DIALOG<dialog_settings_c>(UD_SETTINGS);
        }
        static bool m_newprofile_ok(const ts::wstr_c&prfn, const ts::str_c&)
        {
            ts::wstr_c pn = prfn;
            ts::wstr_c storpn(pn);
            profile_c::path_by_name(pn);
            if (ts::is_file_exists(pn))
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_ERROR,
                    TTT("Such profile already exists",49)
                    ));
                return false;
            }

            HANDLE f = CreateFileW( pn, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
            if (f == INVALID_HANDLE_VALUE)
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_ERROR,
                    TTT("Can't create profile ($)",50) / lasterror()
                    ));
                return true;
            }
            CloseHandle(f);
            

            ts::wstr_c profname = cfg().profile();
            if (profname.is_empty())
            {
                if (prf().load(pn))
                {
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                        DT_MSGBOX_INFO,
                        TTT("Profile [b]$[/b] has created and set as default.",48) / prfn
                        ));
                    cfg().profile(storpn);
                } else
                    profile_c::error_unique_profile(pn);
            } else
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_INFO,
                    TTT("Profile with name [b]$[/b] has created. You can switch to it using settings menu.",51) / prfn
                    ));
            }
            

            return true;
        }
        static void m_newprofile(const ts::str_c&)
        {
            ts::wstr_c defprofilename(CONSTWSTR("%USER%"));
            ts::parse_env(defprofilename);
            SUMMON_DIALOG<dialog_entertext_c>(UD_PROFILENAME, dialog_entertext_c::params(
                                                UD_PROFILENAME,
                                                TTT("[appname]: profile name",44),
                                                TTT("Enter profile name. It is profile file name. You can create any number of profiles and switch them any time. Detailed settings of current profile are available in settings dialog.",43),
                                                defprofilename,
                                                ts::str_c(),
                                                m_newprofile_ok,
                                                check_profile_name));
        }
        static void m_switchto(const ts::str_c& prfn)
        {
            ts::wstr_c oldprfn = cfg().profile();

            ts::wstr_c wpn(from_utf8(prfn));
            ts::wstr_c storwpn(wpn);
            profile_c::path_by_name(wpn);

            contacts().unload();

            if (prf().load(wpn))
                cfg().profile(storwpn);
            else
            {
                prf().load( oldprfn );
                profile_c::error_unique_profile( wpn );
            }

            contacts().update_meta();
            contacts().get_self().reselect(false);

        }
        static void m_about(const ts::str_c&)
        {
            SUMMON_DIALOG<dialog_about_c>(UD_ABOUT);
        }
    };

#ifndef _FINAL
    if (GetKeyState(VK_CONTROL)<0)
    {
        void summon_test_window();
        summon_test_window();
        return true;
    }
#endif


    menu_c m;
    menu_c &pm = m.add_sub( TTT("Profile",39) )
        .add( TTT("Create new",40), 0, handlers::m_newprofile )
        .add_separator();

    ts::wstr_c profname = cfg().profile();
    ts::wstrings_c prfs;
    ts::find_files(ts::fn_change_name_ext(cfg().get_path(), CONSTWSTR("*.profile")), prfs, 0xFFFFFFFF);
    for (const ts::wstr_c &fn : prfs)
    {
        ts::wstr_c wfn(fn);
        ts::wsptr ext = CONSTWSTR(".profile");
        if (ASSERT(wfn.ends(ext))) wfn.trunc_length( ext.l );
        ts::uint32 mif = 0;
        if (wfn == profname) mif = MIF_MARKED|MIF_DISABLED;
        pm.add(TTT("Switch to [b]$[/b]",41) / wfn, mif, handlers::m_switchto, ts::to_utf8(wfn));
    }


    m.add( TTT("Settings",42), 0, handlers::m_settings );
    m.add_separator();
    m.add( TTT("About",206), 0, handlers::m_about );
    gui_popup_menu_c::show(r.call_get_popup_menu_pos(), m);

    //SUMMON_DIALOG<dialog_settings_c>(L"dialog_settings");

    return true;
}

void application_c::summon_main_rect(bool minimize)
{
    load_locale(cfg().language());
    if (!load_theme(cfg().theme()))
    {
        MessageBoxW(nullptr, ts::wstr_c(TTT("Default GUI theme not found!",234)), L"error", MB_OK|MB_ICONERROR);
        sys_exit(1);
        return;
    }
    mediasystem().init();

    s3::DEVICE device = device_from_string( cfg().device_mic() );
    s3::set_capture_device( &device );

    ts::wstr_c profname = cfg().profile();
    auto checkprofilenotexist = [&]()->bool
    {
        if (profname.is_empty()) return true;
        ts::wstr_c pfn(profname);
        bool not_exist = !ts::is_file_exists(profile_c::path_by_name(pfn));
        if (not_exist) cfg().profile(CONSTWSTR("")), profname.clear();
        return not_exist;
    };
    if (checkprofilenotexist())
    {
        ts::wstr_c prfsearch(CONSTWSTR("*"));
        profile_c::path_by_name(prfsearch);
        ts::wstrings_c ss;
        ts::find_files(prfsearch, ss, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY);
        if (ss.size())
            profname = ts::fn_get_name(ss.get(0).as_sptr());
        else
        {
            //profname = CONSTASTR("%USER%");
            //ts::parse_env(profname);
            //if (profname.find_pos('%') >= 0) profname = CONSTASTR("profile");
        }
        cfg().profile(profname);
    }
    if (!profname.is_empty())
        if (!prf().load(profile_c::path_by_name(profname)))
        {
            profile_c::error_unique_profile(profname, true);
            sys_exit(10);
            return;
        }

    drawcollector dcoll;
    main = MAKE_ROOT<mainrect_c>(dcoll);
    ts::ivec2 sz = cfg().get<ts::ivec2>(CONSTASTR("main_rect_size"), ts::ivec2(800, 600));
    ts::irect mr( cfg().get<ts::ivec2>(CONSTASTR("main_rect_pos"), ts::wnd_get_center_pos(sz)), sz );
    mr.rb += mr.lt;

    ts::wnd_fix_rect(mr, sz.x, sz.y);

    if (minimize)
    {
        MODIFY(main)
            .size(mr.size())
            .pos(mr.lt)
            .allow_move_resize()
            .show()
            .micromize(true);
    } else
    {
        MODIFY(main)
            .size(mr.size())
            .pos(mr.lt)
            .allow_move_resize()
            .show();
    }


}

bool application_c::is_inactive(bool do_incoming_message_stuff)
{
    rectengine_root_c *root = HOLD(main)().getroot();
    if (!CHECK(root)) return false;
    bool inactive = false;
    for(;;)
    {
        if (root->getrect().getprops().is_collapsed())
            { inactive = true; break; }
        if (!root->is_foreground())
            { inactive = true; break; }
        break;
    }
    
    if (inactive && do_incoming_message_stuff)
    {
        root->flash();
    }
    return inactive;
}

bool application_c::b_update_ver(RID, GUIPARAM p)
{
    if (auto w = auparams().lock_write(true))
    {
        if (w().in_progress) return true;
        bool renotice = false;
        w().in_progress = true;
        w().downloaded = false;

        autoupdate_beh_e req = (autoupdate_beh_e)as_int(p);
        if (req == AUB_DOWNLOAD)
            renotice = true;
        if (req == AUB_DEFAULT)
            req = (autoupdate_beh_e)cfg().autoupdate();

        w().ver.setcopy(application_c::appver());
        w().path.setcopy(ts::fn_join(ts::fn_get_path(cfg().get_path()),CONSTWSTR("update\\")));
        w().proxy_addr.setcopy(cfg().autoupdate_proxy_addr());
        w().proxy_type = cfg().autoupdate_proxy();
        w().autoupdate = req;
        w.unlock();

        CloseHandle(CreateThread(nullptr, 0, autoupdater, this, 0, nullptr));

        if (renotice)
        {
            download_progress = ts::ivec2(0);
            gmsg<ISOGM_NOTICE>(&contacts().get_self(), nullptr, NOTICE_NEWVERSION, cfg().autoupdate_newver().as_sptr()).send();
        }
    }
    return true;
}

bool application_c::b_restart(RID, GUIPARAM)
{
    ts::wstr_c n = ts::get_exe_full_name();
    n.append(CONSTWSTR(" wait ")).append_as_uint( GetCurrentProcessId() );

    if (ts::start_app(n, nullptr))
    {
        prf().shutdown_aps();
        sys_exit(0);
    }
    
    return true;
}

bool application_c::b_install(RID, GUIPARAM)
{
    ts::wstr_c prm(CONSTWSTR("wait ")); prm.append_as_uint( GetCurrentProcessId() );

    SHELLEXECUTEINFOW shExInfo = { 0 };
    shExInfo.cbSize = sizeof(shExInfo);
    shExInfo.fMask = 0;
    shExInfo.hwnd = 0;
    shExInfo.lpVerb = L"runas";
    shExInfo.lpFile = ts::get_exe_full_name();
    shExInfo.lpParameters = prm;
    shExInfo.lpDirectory = 0;
    shExInfo.nShow = SW_NORMAL;
    shExInfo.hInstApp = 0;

    if (ShellExecuteExW(&shExInfo))
    {
        prf().shutdown_aps();
        sys_exit(0);
    }

    return true;
}


#ifdef _DEBUG
extern bool zero_version;
#endif

ts::str_c application_c::appver()
{
#ifdef _DEBUG
    if (zero_version) return ts::str_c(CONSTASTR("0.0.0"));
#endif // _DEBUG

    static ts::sstr_t<-32> fake_version;
    if (fake_version.is_empty())
    {
        ts::tmp_buf_c b;
        b.load_from_disk_file( ts::fn_change_name_ext(ts::get_exe_full_name(), CONSTWSTR("fake_version.txt")) );
        if (b.size())
            fake_version = b.cstr();
        else
            fake_version.set(CONSTASTR("-"));
    }
    if (fake_version.get_length() >= 5)
        return fake_version;

    struct verb
    {
        ts::sstr_t<-128> v;
        verb()  {}
        verb &operator/(int n) { v.append_as_int(n).append_char('.'); return *this; }
    } v;
    v /
#include "version.inl"
        ;
    return v.v.trunc_length();
}

void application_c::set_notification_icon()
{
    if (main)
    {
        rectengine_root_c *root = HOLD(main)().getroot();
        if (CHECK(root))
        {
            bool sysmenu = gmsg<GM_SYSMENU_PRESENT>().send().is(GMRBIT_ACCEPTED);
            root->notification_icon(sysmenu ? ts::wsptr() : CONSTWSTR(APPNAME));
        }
    }
}

DWORD application_c::handler_SEV_INIT(const system_event_param_s & p)
{
UNSTABLE_CODE_PROLOG
    set_notification_icon();
UNSTABLE_CODE_EPILOG
    return 0;
}

void application_c::handle_sound_capture(const void *data, int size)
{
    if (m_currentsc)
        m_currentsc->datahandler(data, size);
}
void application_c::register_capture_handler(sound_capture_handler_c *h)
{
    m_scaptures.insert(0,h);
}
void application_c::unregister_capture_handler(sound_capture_handler_c *h)
{
    bool cur = h == m_currentsc;
    m_scaptures.find_remove_slow(h);
    if (cur)
    {
        m_currentsc = nullptr;
        start_capture(nullptr);
    }
}
void application_c::start_capture(sound_capture_handler_c *h)
{
    struct checkcaptrue
    {
        application_c *app;
        checkcaptrue(application_c *app):app(app) {}
        ~checkcaptrue()
        {
            bool s3cap = s3::is_capturing();
            if (s3cap && !app->m_currentsc)
                s3::stop_capture();
            else if (!s3cap && app->m_currentsc) {
                int cntf;
                s3::Format *fmts = app->m_currentsc->formats(cntf);
                s3::start_capture(app->m_currentsc->getfmt(), fmts, cntf);
            } else if (s3cap && app->m_currentsc)
                s3::get_capture_format(app->m_currentsc->getfmt());
        }

    } chk(this);

    if (h == nullptr)
    {
        ASSERT( m_currentsc == nullptr );
        for(int i=m_scaptures.size()-1;i>=0;--i)
        {
            if (m_scaptures.get(i)->is_capture())
            {
                m_currentsc = m_scaptures.get(i);
                m_scaptures.remove_slow(i);
                m_scaptures.add(m_currentsc);
                break;
            }
        }
        return;
    }

    if (m_currentsc == h) return;
    if (m_currentsc)
    {
        m_scaptures.find_remove_slow(m_currentsc);
        m_scaptures.add(m_currentsc);
        m_currentsc = nullptr;
    }
    m_currentsc = h;
    m_scaptures.find_remove_slow(h);
    m_scaptures.add(h);
}

void application_c::stop_capture(sound_capture_handler_c *h)
{
    if (h != m_currentsc) return;
    m_currentsc = nullptr;
    start_capture(nullptr);
}

void application_c::capture_device_changed()
{
    if (s3::is_capturing() && m_currentsc)
    {
        sound_capture_handler_c *h = m_currentsc;
        stop_capture(h);
        start_capture(h);
    }        
}


void application_c::update_ringtone( contact_c *rt, bool play_stop_snd )
{
    if (rt->is_ringtone())
        m_ringing.set(rt);
    else
        m_ringing.find_remove_fast(rt);


    if (m_ringing.size())
    {
        gmsg<ISOGM_AV_COUNT> avc; avc.send();
        (avc.count == 0) ? 
            play_sound(snd_ringtone, true) :
            play_sound(snd_ringtone2, true);
    } else
    {
        stop_sound(snd_ringtone2);
        if (stop_sound(snd_ringtone) && play_stop_snd)
            play_sound(snd_call_cancel, false);
    }
}

bool application_c::present_file_transfer_by_historian(const contact_key_s &historian, bool accept_only_rquest)
{
    for (const file_transfer_s *ftr : m_files)
        if (ftr->historian == historian)
            if (accept_only_rquest) { if (ftr->file_handle() == nullptr) return true; }
            else { return true; }
    return false;
}

bool application_c::present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest)
{
    for (const file_transfer_s *ftr : m_files)
        if (ftr->sender == sender)
            if (accept_only_rquest) { if (ftr->file_handle() == nullptr) return true; }
            else { return true; }
    return false;
}

file_transfer_s *application_c::find_file_transfer_by_msgutag(uint64 utag)
{
    for (file_transfer_s *ftr : m_files)
        if (ftr->msgitem_utag == utag)
            return ftr;
    return nullptr;
}

file_transfer_s *application_c::find_file_transfer(uint64 utag)
{
    for(file_transfer_s *ftr : m_files)
        if (ftr->utag == utag)
            return ftr;
    return nullptr;
}

file_transfer_s * application_c::register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, const ts::wstr_c &filename, uint64 filesize )
{
    if (find_file_transfer(utag)) return nullptr;

    file_transfer_s *ftr = TSNEW( file_transfer_s );
    m_files.add( ftr );

    auto d = ftr->data.lock_write();

    ftr->historian = historian;
    ftr->sender = sender;
    ftr->filename = filename;
    ftr->filename_on_disk = filename;
    ftr->filesize = filesize;
    ftr->utag = utag;
    ts::fix_path(ftr->filename, FNO_NORMALIZE);

    auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.utag == utag; });

    if (filesize == 0)
    {
        // send
        ftr->upload = true;
        d().handle = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (d().handle == INVALID_HANDLE_VALUE)
        {
            d().handle = nullptr;
            m_files.remove_fast(m_files.size()-1);
            return nullptr;
        }
        GetFileSizeEx(d().handle, &ts::ref_cast<LARGE_INTEGER>(ftr->filesize) );

        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->send_file(sender.contactid, utag, ts::fn_get_name_with_ext(ftr->filename), ftr->filesize);

        d().bytes_per_sec = file_transfer_s::BPSSV_WAIT_FOR_ACCEPT;
        if (row == nullptr) ftr->upd_message_item(true);
    }

    if (row)
    {
        ASSERT( row->other.filesize == ftr->filesize && row->other.filename.equals(ftr->filename) );
        ftr->msgitem_utag = row->other.msgitem_utag;
        row->other = *ftr;
        ftr->upd_message_item(true);
    } else
    {
        auto &tft = prf().get_table_unfinished_file_transfer().getcreate(0);
        tft.other = *ftr;
    }

    prf().changed();

    return ftr;
}

void application_c::cancel_file_transfers( const contact_key_s &historian )
{
    for (int i = m_files.size()-1; i >= 0; --i)
    {
        file_transfer_s *ftr = m_files.get(i);
        if (ftr->historian == historian)
            m_files.remove_fast(i);
    }

    bool ch = false;
    for (auto &row : prf().get_table_unfinished_file_transfer())
    {
        if (row.other.historian == historian)
            ch |= row.deleted();
    }
    if (ch) prf().changed();

}

void application_c::unregister_file_transfer(uint64 utag, bool disconnected)
{
    if (!disconnected)
        if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.utag == utag; }))
            if (row->deleted())
                prf().changed();

    int cnt = m_files.size();
    for (int i=0;i<cnt;++i)
    {
        file_transfer_s *ftr = m_files.get(i);
        if (ftr->utag == utag)
        {
            m_files.remove_fast(i);
            return;
        }
    }
}

ts::uint32 application_c::gm_handler(gmsg<ISOGM_DELIVERED>&d)
{
    int cntx = m_undelivered.size();
    for (int j = 0; j < cntx; ++j)
    {
        send_queue_s *q = m_undelivered.get(j);
        
        int cnt = q->queue.size();
        for (int i = 0; i < cnt; ++i)
        {
            if (q->queue.get(i).utag == d.utag)
            {
                q->queue.remove_slow(i);

                if (q->queue.size() == 0)
                    m_undelivered.remove_fast(j);
                else
                    resend_undelivered_messages(q->receiver); // now send other undelivered messages

                return 0;
            }
        }
    }

    WARNING("m_undelivered fail");
    return 0;
}

void application_c::resend_undelivered_messages( const contact_key_s& rcv )
{
    for (send_queue_s *q : m_undelivered)
    {
        if (q->receiver == rcv || rcv.is_empty())
        {
            while ( !rcv.is_empty() || (ts::Time::current() - q->last_try_send_time) > 5000 /* try 2 resend every 5 seconds */ )
            {
                q->last_try_send_time = ts::Time::current();
                contact_c *receiver = contacts().find( q->receiver );

                if (receiver == nullptr)
                {
                    q->queue.clear();
                    break;
                }

                if (receiver->is_meta())
                    receiver = receiver->subget_for_send(); // get default subcontact for message target

                if (receiver == nullptr)
                {
                    q->queue.clear();
                    break;
                }

                gmsg<ISOGM_MESSAGE> msg(&contacts().get_self(), receiver, MTA_UNDELIVERED_MESSAGE);

                const post_s& post = q->queue.get(0);
                msg.post.time = post.time;
                msg.post.utag = post.utag;
                msg.post.message_utf8 = post.message_utf8;
                msg.resend = true;
                msg.send();

                break; //-V612 // yeah. unconditional break
            }

            if (!rcv.is_empty())
                break;
        }
    }
}

void application_c::undelivered_message( const post_s &p )
{
    contact_c *c = contacts().find(p.receiver);
    if (!c) return;

    for( const send_queue_s *q : m_undelivered )
        for( const post_s &pp : q->queue )
            if (pp.utag == p.utag)
                return;

    contact_key_s rcv = p.receiver;

    if (!c->is_meta())
    {
        c = c->getmeta();
        if (c)
            rcv = c->getkey();
    }

    for( send_queue_s *q : m_undelivered )
        if (q->receiver == rcv)
        {
            int cnt = q->queue.size();
            for(int i=0;i<cnt;++i)
            {
                const post_s &qp = q->queue.get(i);
                if (qp.time > p.time)
                {
                    post_s &insp = q->queue.insert(i);
                    insp = p;
                    insp.receiver = rcv;
                    rcv = contact_key_s();
                    break;
                }
            }
            if (!rcv.is_empty())
            {
                post_s &insp = q->queue.add();
                insp = p;
                insp.receiver = rcv;
            }
            return;
        }

    send_queue_s *q = TSNEW( send_queue_s );
    m_undelivered.add( q );
    q->receiver = rcv;
    post_s &insp = q->queue.add();
    insp = p;
    insp.receiver = rcv;

}

bool application_c::load_theme( const ts::wsptr&thn )
{
    if (!__super::load_theme(thn))
    {
        if (!ts::pwstr_c(thn).equals(CONSTWSTR("def")))
        {
            cfg().theme( CONSTWSTR("def") );
            return load_theme( CONSTWSTR("def") );
        }
        return false;
    }
    m_buttons.reload();

    font_conv_name = &get_font( CONSTASTR("conv_name") );
    font_conv_text = &get_font( CONSTASTR("conv_text") );
    font_conv_time = &get_font( CONSTASTR("conv_time") );
    contactheight= theme().conf().get_string(CONSTASTR("contactheight")).as_int(55);
    mecontactheight = theme().conf().get_string(CONSTASTR("mecontactheight")).as_int(60);
    protowidth = theme().conf().get_string(CONSTASTR("protowidth")).as_int(100);
    protoiconsize = theme().conf().get_string(CONSTASTR("protoiconsize")).as_int(10);

    emoti().reload();

    return true;
}

void preloaded_buttons_s::reload()
{
    const theme_c &th = gui->theme();

    icon[CSEX_UNKNOWN] = th.get_button(CONSTASTR("nosex"));
    icon[CSEX_MALE] = th.get_button(CONSTASTR("male"));
    icon[CSEX_FEMALE] = th.get_button(CONSTASTR("female"));
    groupchat = th.get_button(CONSTASTR("groupchat"));
            
    online = th.get_button(CONSTASTR("online"));
    online2 = th.get_button(CONSTASTR("online2"));
    invite = th.get_button(CONSTASTR("invite"));
    unread = th.get_button(CONSTASTR("unread"));
    callb = th.get_button(CONSTASTR("call"));
    fileb = th.get_button(CONSTASTR("file"));

    editb = th.get_button(CONSTASTR("edit"));
    confirmb = th.get_button(CONSTASTR("confirmb"));
    cancelb = th.get_button(CONSTASTR("cancelb"));

    breakb = th.get_button(CONSTASTR("break"));
    pauseb = th.get_button(CONSTASTR("pause"));
    unpauseb = th.get_button(CONSTASTR("unpause"));
    exploreb = th.get_button(CONSTASTR("explore"));

    nokeeph = th.get_button(CONSTASTR("nokeeph"));
    smile = th.get_button(CONSTASTR("smile"));
}

file_transfer_s::file_transfer_s()
{
    auto d = data.lock_write();

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    d().notfreq = 1.0 / (double)freq.QuadPart;
    QueryPerformanceCounter(&d().prevt);
}

file_transfer_s::~file_transfer_s()
{
    if (query_task)
    {
        while (query_task->rslt == query_task_s::rslt_inprogress) // oops. query task in progress (other thread job). wait...
            Sleep(1);

        query_task->ftr = nullptr;
    }

    if (HANDLE handle = file_handle())
        CloseHandle(handle);
}

bool file_transfer_s::confirm_required() const
{
    if ( prf().fileconfirm() == 0 )
    {
        // required, except...
        return !file_mask_match( filename, prf().auto_confirm_masks() );
    } else if (prf().fileconfirm() == 1)
    {
        // not required, except...
        return file_mask_match(filename, prf().manual_confirm_masks());
    }
    return true;
}

void file_transfer_s::auto_confirm()
{
    ts::wstr_c downf = prf().download_folder();
    path_expand_env(downf);
    ts::make_path(downf, 0);
    prepare_fn(ts::fn_join(downf, filename), false);

    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_control(utag, FIC_ACCEPT);
}


void file_transfer_s::prepare_fn( const ts::wstr_c &path_with_fn, bool overwrite )
{
    accepted = true;
    filename_on_disk = path_with_fn;
    if (!overwrite)
    {
        ts::wstr_c origname = ts::fn_get_name(filename_on_disk);
        int n = 1;
        while (ts::is_file_exists(filename_on_disk) || ts::is_file_exists(filename_on_disk + CONSTWSTR(".!rcv")))
            filename_on_disk = ts::fn_change_name(filename_on_disk, ts::wstr_c(origname).append_char('(').append_as_int(n++).append_char(')'));
    }
    filename_on_disk.append(CONSTWSTR(".!rcv"));
    data.lock_write()().deltatime(true);
    upd_message_item(true);

    if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.utag == utag; }))
    {
        row->other.msgitem_utag = msgitem_utag;
        row->other.filename_on_disk = filename_on_disk;
        row->changed();
        prf().changed();
    }
}

int file_transfer_s::progress(int &bps) const
{
    auto rdata = data.lock_read();
    bps = rdata().bytes_per_sec;
    return (int)(rdata().progrez * 100 / filesize);
}

void file_transfer_s::pause_by_remote( bool p )
{
    if (p)
    {
        data.lock_write()().bytes_per_sec = BPSSV_PAUSED_BY_REMOTE;
        upd_message_item(true);
    } else
    {
        auto wdata = data.lock_write();
        wdata().deltatime(true);
        wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
    }
}

void file_transfer_s::pause_by_me(bool p)
{
    active_protocol_c *ap = prf().ap(sender.protoid);
    if (!ap) return;

    if (p)
    {
        ap->file_control(utag, FIC_PAUSE);
        data.lock_write()().bytes_per_sec = BPSSV_PAUSED_BY_ME;
        upd_message_item(true);
    }
    else
    {
        ap->file_control(utag, FIC_UNPAUSE);
        auto wdata = data.lock_write();
        wdata().deltatime(true);
        wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
    }
}

void file_transfer_s::kill( file_control_e fctl )
{
    //DMSG("kill " << utag << fctl << filename_on_disk);

    if (contact_c *h = contacts().find(historian))
        if (h->gui_item)
            h->gui_item->getengine().redraw();

    if (!upload && !accepted && (fctl == FIC_NONE || fctl == FIC_DISCONNECT))
    {
        // kill without accept - just do nothing
        g_app->unregister_file_transfer(utag, false);
        return;
    }

    if (fctl != FIC_NONE)
    {
        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->file_control(utag, fctl);
    }

    HANDLE handle = file_handle();
    if (handle && (!upload || fctl != FIC_DONE)) // close before update message item
    {
        LARGE_INTEGER fsz = {0};
        if (!upload)
        {
            GetFileSizeEx(handle,&fsz);
            if (fctl == FIC_DONE && (uint64)fsz.QuadPart != filesize)
                return;
        }
        CloseHandle(handle);
        data.lock_write()().handle = nullptr;
        if (filename_on_disk.ends(CONSTWSTR(".!rcv")))
            filename_on_disk.trunc_length(5);
        if ( (uint64)fsz.QuadPart != filesize || upload)
        {
            post_s p;
            p.sender = sender;
            filename_on_disk.insert(0, fctl != FIC_DISCONNECT ? '*' : '?');
            p.message_utf8 = to_utf8(filename_on_disk);
            p.utag = msgitem_utag;
            prf().change_history_item(historian, p, HITM_MESSAGE);
            if (contact_c * h = contacts().find(historian)) h->iterate_history([this](post_s &p)->bool {
                if (p.utag == msgitem_utag)
                {
                    p.message_utf8 = to_utf8(filename_on_disk);
                    return true;
                }
                return false;
            });
        
            if (!upload && fctl != FIC_DISCONNECT) 
                DeleteFileW(ts::wstr_c(filename_on_disk.as_sptr().skip(1)).append(CONSTWSTR(".!rcv")));
        } else if (!upload && fctl == FIC_DONE)
            MoveFileW(filename_on_disk + CONSTWSTR(".!rcv"), filename_on_disk);
    }
    data.lock_write()().deltatime(true, -60);
    if (fctl != FIC_REJECT) upd_message_item(true);
    if (fctl == FIC_DONE)
    {
        post_s p;
        p.sender = sender;
        p.message_utf8 = to_utf8(filename_on_disk);
        p.utag = msgitem_utag;
        prf().change_history_item(historian, p, HITM_MESSAGE);
        if (contact_c * h = contacts().find(historian)) h->iterate_history([this](post_s &p)->bool {
            if (p.utag == msgitem_utag)
            {
                p.message_utf8 = to_utf8(filename_on_disk);
                return true;
            }
            return false;
        });

    }
    g_app->unregister_file_transfer(utag, fctl == FIC_DISCONNECT);
}

void file_transfer_s::data_s::tr( uint64 _offset0, uint64 _offset1 )
{
    if ( transfered.count() == 0 )
    {
        range_s &r = transfered.add();
        r.offset0 = _offset0;
        r.offset1 = _offset1;
        return;
    }

    int cnt = transfered.count();
    for(int i=0; i<cnt; ++i)
    {
        range_s r = transfered.get(i);
        
        if (_offset0 > r.offset1)
            continue;

        if (_offset1 < r.offset0)
        {
            range_s &rr = transfered.insert(i);
            rr.offset0 = _offset0;
            rr.offset1 = _offset1;
            return;
        }
        if (_offset1 == r.offset0)
        {
            r.offset0 = _offset0;
            transfered.remove_slow(i);
            return tr(r.offset0, r.offset1);
        }
        if (_offset0 == r.offset1)
        {
            r.offset1 = _offset1;
            transfered.remove_slow(i);
            return tr(r.offset0, r.offset1);
        }

        return;
    }

    range_s &rr = transfered.add();
    rr.offset0 = _offset0;
    rr.offset1 = _offset1;
}

query_task_s::~query_task_s()
{
    if (ftr)
        ftr->query_task = nullptr;
}


/*virtual*/ int query_task_s::iterate(int pass)
{
    job_s cj = sync.lock_read()().current_job;

    if (ftr->get_offset() != cj.offset)
    {
        auto wdata = ftr->data.lock_write();
        LARGE_INTEGER li;
        li.QuadPart = cj.offset;
        SetFilePointer(wdata().handle, li.LowPart, &li.HighPart, FILE_BEGIN);
        wdata().offset = cj.offset;
    }
    int sz = (int)ts::tmin<int64>(cj.sz, (int64)(ftr->filesize - cj.offset));
    ts::tmp_buf_c b(sz, true);
    DWORD r;
    if (!ReadFile(ftr->file_handle(), b.data(), sz, &r, nullptr))
    {
        rslt = rslt_kill;
        return R_DONE;
    }

    if (active_protocol_c *ap = prf().ap(ftr->sender.protoid))
        ap->file_portion(ftr->utag, cj.offset, b.data(), cj.sz);

    if (cj.sz)
    {
        auto wdata = ftr->data.lock_write();

        if (wdata().bytes_per_sec >= file_transfer_s::BPSSV_ALLOW_CALC)
        {
            wdata().tr(cj.offset, cj.offset + cj.sz);
            wdata().upduitime += wdata().deltatime(true);

            if (wdata().upduitime > 0.3f)
            {
                wdata().upduitime -= 0.3f;
                wdata().bytes_per_sec = lround((float)wdata().trsz() / 0.3f);
                wdata().transfered.clear();
                ftr->update_item = true;
            }
        }

        wdata().offset += cj.sz;
        wdata().progrez = wdata().offset + cj.sz;
    }


    auto d = sync.lock_write();
    if (d().jobarray.size())
    {
        d().current_job = d().jobarray.get_remove_slow();
        rslt = rslt_inprogress;
        return R_RESULT;
    }

    rslt = rslt_ok;
    return R_DONE;
}
/*virtual*/ void query_task_s::done(bool canceled)
{
    if (canceled || ftr == nullptr)
    {
        __super::done(canceled);
        return;
    }

    if (rslt == rslt_kill)
    {
        ftr->kill(); //-V595
        ASSERT(ftr == nullptr);
        __super::done(canceled);
        return;
    }

    ASSERT( rslt == rslt_ok );

    ftr->upd_message_item(false);

    __super::done(canceled);
}

/*virtual*/ void query_task_s::result()
{
    if (ftr) ftr->upd_message_item(false);
}

void file_transfer_s::query( uint64 offset_, int sz )
{
    if (query_task)
    {
        auto d = query_task->sync.lock_write();
        auto &job = d().jobarray.add();
        job.offset = offset_;
        job.sz = sz;
        return;
    }
        

    if (file_handle())
    {
        query_task = TSNEW( query_task_s, this );
        auto d = query_task->sync.lock_write();
        d().current_job.offset = offset_;
        d().current_job.sz = sz;
        d.unlock();
        g_app->add_task(query_task);
    }
}

void file_transfer_s::upload_accepted()
{
    auto wdata = data.lock_write();
    ASSERT( wdata().bytes_per_sec == BPSSV_WAIT_FOR_ACCEPT );
        wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
}

void file_transfer_s::resume()
{
    ASSERT(file_handle() == nullptr);

    HANDLE h = CreateFileW(filename_on_disk, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        kill();
        return;
    }
    auto wdata = data.lock_write();
    wdata().handle = h;

    LARGE_INTEGER fsz = { 0 };
    GetFileSizeEx(wdata().handle, &fsz);
    wdata().offset = fsz.QuadPart > 1024 ? fsz.QuadPart - 1024 : 0;
    wdata().progrez = wdata().offset;
    fsz.QuadPart = wdata().offset;
    SetFilePointer(wdata().handle, fsz.LowPart, &fsz.HighPart, FILE_BEGIN);

    accepted = true;

    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_resume( utag, wdata().offset );

}

void file_transfer_s::save(uint64 offset_, const ts::buf0_c&bdata)
{
    if (!accepted) return;

    if (file_handle() == nullptr)
    {
        play_sound( snd_start_recv_file, false );

        HANDLE h = CreateFileW(filename_on_disk, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            kill();
            return;
        }
        data.lock_write()().handle = h;
        if (contact_c *c = contacts().find(historian))
            if (c->gui_item)
                c->gui_item->getengine().redraw();
    }
    if (offset_ + bdata.size() > filesize)
    {
        kill();
        return;
    }

    auto wdata = data.lock_write();

    if ( wdata().offset != offset_ )
    {
        LARGE_INTEGER li;
        li.QuadPart = offset_;
        SetFilePointer(wdata().handle, li.LowPart, &li.HighPart, FILE_BEGIN);
        wdata().offset = offset_;
    }

    DWORD w;
    WriteFile(wdata().handle, bdata.data(), bdata.size(), &w, nullptr);
    if ((ts::aint)w != bdata.size())
    {
        kill();
        return;
    }

    wdata().offset += bdata.size();
    wdata().progrez += bdata.size();

    if (bdata.size())
    {
        if (wdata().bytes_per_sec >= BPSSV_ALLOW_CALC)
        {
            wdata().tr( offset_, offset_+ bdata.size() );
            wdata().upduitime += wdata().deltatime(true);

            if (wdata().upduitime > 0.3f)
            {
                wdata().upduitime -= 0.3f;
                wdata().bytes_per_sec = lround((float)wdata().trsz() / 0.3f);
                wdata().transfered.clear();
                wdata.unlock();
                upd_message_item(true);
            }
        }
    }

}

void file_transfer_s::upd_message_item(bool force)
{
    if (!force && !update_item) return;
    update_item = false;
    //DMSG("upditem " << utag << filename_on_disk);

    if (msgitem_utag)
    {
        post_s p;
        p.type = upload ? MTA_SEND_FILE : MTA_RECV_FILE;
        p.message_utf8 = to_utf8(filename_on_disk);
        if (p.message_utf8.ends(CONSTASTR(".!rcv")))
            p.message_utf8.trunc_length(5);

        p.utag = msgitem_utag;
        p.sender = sender;
        p.receiver = contacts().get_self().getkey();
        gmsg<ISOGM_SUMMON_POST>(p, true).send();
    } else if (contact_c *c = contacts().find(sender))
    {
        gmsg<ISOGM_MESSAGE> msg(c, &contacts().get_self(), upload ? MTA_SEND_FILE : MTA_RECV_FILE);
        msg.create_time = now();
        msg.post.message_utf8 = to_utf8(filename_on_disk);
        if (msg.post.message_utf8.ends(CONSTASTR(".!rcv")))
            msg.post.message_utf8.trunc_length(5);
        msg.post.utag = prf().uniq_history_item_tag();
        msgitem_utag = msg.post.utag;
        msg.send();
    }
}

