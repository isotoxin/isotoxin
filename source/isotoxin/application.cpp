#include "isotoxin.h"

application_c *g_app = nullptr;

#ifndef _FINAL
void dotests();
#endif

application_c::application_c(const ts::wchar * cmdl)
{
    autoupdate_next = now() + 10;
	g_app = this;
    cfg().load();
    if (cfg().is_loaded())
        summon_main_rect();

#ifndef _FINAL
    dotests();
#endif

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

    ts::g_fileop->find(fns, path.append(lng).append(CONSTWSTR(".*.lng")));

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
                m_locale_lng[SLANGID(stag)] = l;
            }
        }
    }
}

bool application_c::b_send_message(RID r, GUIPARAM param)
{
    gmsg<ISOGM_SEND_MESSAGE>().send().is(GMRBIT_ACCEPTED);
    return true;
}

// isogui

HICON application_c::iso_gui_c::app_icon(bool for_tray)
{
    if (!for_tray)
        return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(IDI_ICON)); 
    bool unread = gmsg<ISOGM_SOMEUNREAD>().send().is(GMRBIT_ACCEPTED);
    return LoadIcon(g_sysconf.instance, unread ? MAKEINTRESOURCE(IDI_ICON2) : MAKEINTRESOURCE(IDI_ICON)); 
};

/*virtual*/ void application_c::iso_gui_c::app_prepare_text_for_copy(ts::wstr_c &text)
{
    int rr = text.find_pos(CONSTWSTR("<r>"));
    if (rr >= 0)
    {
        int rr2 = text.find_pos(rr+3, CONSTWSTR("</r>"));
        if (rr2 > rr)
            text.cut(rr, rr2-rr+4);
    }

    text.replace_all(CONSTWSTR("<char=60>"), CONSTWSTR("\2"));
    text.replace_all(CONSTWSTR("<char=62>"), CONSTWSTR("\3"));
    text.replace_all(CONSTWSTR("<br>"), CONSTWSTR("\n"));
    text.replace_all(CONSTWSTR("<p>"), CONSTWSTR("\n"));

    text_convert_to_bbcode(text);
    text_close_bbcode(text);
    text_convert_char_tags(text);

    // unparse img - smiles
    auto t = CONSTWSTR("<img=");
    for (int i = text.find_pos(t); i >= 0; i = text.find_pos(i + 1, t))
    {
        int j = text.find_pos(i + t.l, '>');
        if (j < 0) break;
        text.replace(i,j-i+1, ts::fn_get_name<ts::wchar>(text.substr(i + t.l, j)) );
    }

    text_remove_tags(text);

    text.replace_all('\2', '<');
    text.replace_all('\3', '>');

    text.replace_all(CONSTWSTR("\r\n"), CONSTWSTR("\n"));
    text.replace_all(CONSTWSTR("\n"), CONSTWSTR("\r\n"));
}

/*virtual*/ ts::wsptr application_c::iso_gui_c::app_loclabel(loc_label_e ll)
{
    switch (ll)
    {
        case LL_CTXMENU_COPY: return TTT("Копировать",92);
        case LL_CTXMENU_CUT: return TTT("Вырезать",93);
        case LL_CTXMENU_PASTE: return TTT("Вставить",94);
        case LL_CTXMENU_DELETE: return TTT("Удалить",95);
        case LL_CTXMENU_SELALL: return TTT("Выделить всё",96);
        case LL_ABTT_CLOSE:
            if (cfg().collapse_beh() == 2)
            {
                return TTT("Свернуть в область уведомлений[br](Чтобы закрыть, удерживайте Ctrl)",122);
            }
            return TTT("Закрыть", 3);
        case LL_ABTT_MAXIMIZE: return TTT("Во весь экран", 4);
        case LL_ABTT_NORMALIZE: return TTT("Нормальный размер", 5);
        case LL_ABTT_MINIMIZE:
            if (cfg().collapse_beh() == 1)
                return TTT("Свернуть в область уведомлений",123);
            return TTT("Свернуть", 6);
    }
    return __super::app_loclabel(ll);
}

/*virtual*/ void application_c::iso_gui_c::app_b_minimize(RID main)
{
    if (cfg().collapse_beh() == 1)
        MODIFY(main).micromize(true);
    else
        __super::app_b_minimize(main);
}
/*virtual*/ void application_c::iso_gui_c::app_b_close(RID main)
{
    if (GetKeyState(VK_CONTROL) >= 0 && cfg().collapse_beh() == 2)
        MODIFY(main).micromize(true);
    else
        __super::app_b_close(main);
}
/*virtual*/ void application_c::iso_gui_c::app_path_expand_env(ts::wstr_c &path)
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

/*virtual*/ void application_c::iso_gui_c::app_5second_event()
{
    g_app->set_notification_icon();

    if ( cfg().autoupdate() > 0 )
    {
        if (now() > g_app->autoupdate_next)
        {
            g_app->autoupdate_next += SEC_PER_DAY;
            if (g_app->autoupdate_next <= now() )
                g_app->autoupdate_next = now() + SEC_PER_DAY;

            g_app->b_update_ver(RID(),(GUIPARAM)AUB_DEFAULT);
        }
        if (!g_app->nonewversion())
        {
            if (!g_app->newversion() && new_version())
                gmsg<ISOGM_NEWVERSION>(cfg().autoupdate_newver()).send();
            g_app->nonewversion(true);
        }
    }

}

/*virtual*/ void application_c::iso_gui_c::app_fix_sleep_value(int &sleep_ms)
{
    UNSTABLE_CODE_PROLOG

    ts::tbuf_t<contact_key_s> & unr = g_app->m_need_recalc_unread;
    while (unr.count())
    {
        ts::tbuf_t<contact_key_s> & unrl = g_app->m_locked_recalc_unread;

        contact_key_s ck = unr.get(0);
        if (unrl.find_index(ck) >= 0)
        {
            // locked. postpone
            unr.remove_slow(0);
            unr.add(ck);
            break;
        }
        contact_c *c = contacts().find(ck);
        unr.remove_slow(0);
        if (c) c->recalc_unread();
        break;
    }

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

/*virtual*/ void application_c::iso_gui_c::app_notification_icon_action(naction_e act, RID iconowner)
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

        menu_c m;
        m.add(TTT("Выход",117), 0, handlers::m_exit);
        gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(),0), m, true);

    }
}

bool application_c::iso_gui_c::b_customize(RID r, GUIPARAM param)
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
            profile_c::path_by_name(pn);
            if (ts::is_file_exists<ts::wchar>(pn))
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_ERROR),
                    TTT("Профиль с таким именем уже существует",49)
                    ));
                return false;
            }

            HANDLE f = CreateFileW( pn, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
            if (f == INVALID_HANDLE_VALUE)
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_ERROR),
                    TTT("Не удалось создать профиль ($)",50) / lasterror()
                    ));
                return true;
            }
            CloseHandle(f);
            

            ts::wstr_c profname = cfg().profile();
            if (profname.is_empty())
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_INFO),
                    TTT("Профиль с именем [b]$[/b] создан и установлен в качестве профиля по умолчанию.",48) / prfn
                    ));
                cfg().profile(pn);
                prf().load(pn);
            } else
            {
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_INFO),
                    TTT("Профиль с именем [b]$[/b] создан. Вы можете переключиться на него, используя меню настроек.",51) / prfn
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
                                                TTT("[appname]: имя профиля",44),
                                                TTT("Введите имя профия. Будет создан файл с таким именем. Вы можете создавать любое количество профилей и переключаться между ними в любой момент. Детальная настройка текущего профиля доступна в окне настроек.",43),
                                                defprofilename,
                                                ts::str_c(),
                                                m_newprofile_ok,
                                                check_profile_name));
        }
        static void m_switchto(const ts::str_c& prfn)
        {
            ts::wstr_c wpn; wpn.set_as_utf8(prfn);
            profile_c::path_by_name(wpn);
            cfg().profile(wpn);
            prf().load(wpn);

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
    menu_c &pm = m.add_sub( TTT("Профиль",39) )
        .add( TTT("Создать новый",40), 0, handlers::m_newprofile )
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
        pm.add(TTT("Переключиться на [b]$[/b]",41) / wfn, mif, handlers::m_switchto, ts::to_utf8(wfn));
    }


    m.add( TTT("Настройки",42), 0, handlers::m_settings );
    gui_popup_menu_c::show(r.call_get_popup_menu_pos(), m);

    //SUMMON_DIALOG<dialog_settings_c>(L"dialog_settings");

    return true;
}

void application_c::summon_main_rect()
{
    load_locale(cfg().language());
    load_theme(cfg().theme());
    mediasystem().init();
    ts::str_c mic = cfg().device_mic();
    s3::DEVICE device = s3::DEFAULT_DEVICE;
    if (mic.get_length() / 2 == sizeof(device))
        for (int i = 0; i < sizeof(device); ++i)
            ((ts::uint8 *)&device)[i] = mic.as_byte_hex(i * 2);
    s3::set_capture_device( &device );

    ts::wstr_c profname = cfg().profile();
    if (profname.is_empty())
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
        prf().load(profile_c::path_by_name(profname));

    drawchecker dch;
    main = MAKE_ROOT<mainrect_c>(dch);
    ts::ivec2 sz = cfg().get<ts::ivec2>(CONSTASTR("main_rect_size"), ts::ivec2(800, 600));
    MODIFY(main)
        .size(sz)
        .pos(cfg().get<ts::ivec2>(CONSTASTR("main_rect_pos"), ts::wnd_get_center_pos(sz)))
        .allow_move_resize()
        .show();

}

bool application_c::is_inactive(bool do_incoming_message_stuff)
{
    rectengine_root_c *root = HOLD(main).engine().getroot();
    if (!ASSERT(root)) return false;
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

        autoupdate_beh_e req = (autoupdate_beh_e)(int)p;
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
            gmsg<ISOGM_NOTICE>(&contacts().get_self(), nullptr, NOTICE_NEWVERSION, ts::to_wstr(cfg().autoupdate_newver().as_sptr())).send();
    }
    return true;
}

bool application_c::b_restart(RID, GUIPARAM)
{
    ts::wstr_c n = ts::get_exe_full_name();
    n.append(CONSTWSTR(" wait ")).append_as_uint( GetCurrentProcessId() );

    prf().shutdown_aps();

    if (ts::start_app(n, nullptr))
        sys_exit(0);
    
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
        rectengine_root_c *root = HOLD(main).engine().getroot();
        if (ASSERT(root))
        {
            root->notification_icon(CONSTWSTR(APPNAME));
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
        s3::start_capture( m_currentsc->getfmt() );
}


void application_c::update_ringtone( contact_c *rt, bool play_stop_snd )
{
    if (rt->is_ringtone())
        m_ringing.set(rt);
    else
        m_ringing.find_remove_fast(rt);


    if (m_ringing.size())
    {
        play_sound(snd_ringtone, true);
    }
    else
    {
        if (stop_sound(snd_ringtone) && play_stop_snd)
            play_sound(snd_call_cancel, false);
    }
}

bool application_c::present_file_transfer_by_historian(const contact_key_s &historian, bool accept_only_rquest)
{
    for (file_transfer_s &ftr : m_files)
        if (ftr.historian == historian)
            if (accept_only_rquest) { if (ftr.handle == nullptr) return true; }
            else { return true; }
    return false;
}

bool application_c::present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest)
{
    for (file_transfer_s &ftr : m_files)
        if (ftr.sender == sender)
            if (accept_only_rquest) { if (ftr.handle == nullptr) return true; }
            else { return true; }
    return false;
}

file_transfer_s *application_c::find_file_transfer_by_msgutag(uint64 utag)
{
    for (file_transfer_s &ftr : m_files)
        if (ftr.msgitem_utag == utag)
            return &ftr;
    return nullptr;
}

file_transfer_s *application_c::find_file_transfer(uint64 utag)
{
    for(file_transfer_s &ftr : m_files)
        if (ftr.utag == utag)
            return &ftr;
    return nullptr;
}

bool application_c::register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, const ts::wstr_c &filename, uint64 filesize )
{
    if (find_file_transfer(utag)) return false;

    file_transfer_s &ftr = m_files.add();
    ftr.historian = historian;
    ftr.sender = sender;
    ftr.filename = filename;
    ftr.filesize = filesize;
    ftr.utag = utag;

    if (filesize == 0)
    {
        // send
        ftr.send = true;
        ftr.handle = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (ftr.handle == INVALID_HANDLE_VALUE)
        {
            ftr.handle = nullptr;
            m_files.remove_fast(m_files.size()-1);
            return false;
        }
        GetFileSizeEx(ftr.handle, &ts::ref_cast<LARGE_INTEGER>(ftr.filesize) );

        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->send_file(sender.contactid, utag, ts::fn_get_name_with_ext(ftr.filename), ftr.filesize);

        ftr.bytes_per_sec = -3; // wait 4 accept
        ftr.upd_message_item();
    }

    return true;
}

void application_c::unregister_file_transfer(uint64 utag)
{
    int cnt = m_files.size();
    for (int i=0;i<cnt;++i)
    {
        file_transfer_s &ftr = m_files.get(i);
        if (ftr.utag == utag)
        {
            m_files.remove_fast(i);
            return;
        }
    }

}


void application_c::load_theme( const ts::wsptr&thn )
{
    m_gui.load_theme(thn);
    m_buttons.reload();

    font_conv_name = &m_gui.get_font( CONSTASTR("conv_name") );
    font_conv_text = &m_gui.get_font( CONSTASTR("conv_text") );

}

void preloaded_buttons_s::reload()
{
    icon[CSEX_UNKNOWN] = gui->theme().get_button(CONSTASTR("nosex"));
    icon[CSEX_MALE] = gui->theme().get_button(CONSTASTR("male"));
    icon[CSEX_FEMALE] = gui->theme().get_button(CONSTASTR("female"));
            
    online = gui->theme().get_button(CONSTASTR("online"));
    online2 = gui->theme().get_button(CONSTASTR("online2"));
    invite = gui->theme().get_button(CONSTASTR("invite"));
    unread = gui->theme().get_button(CONSTASTR("unread"));
    callb = gui->theme().get_button(CONSTASTR("call"));
    fileb = gui->theme().get_button(CONSTASTR("file"));

    editb = gui->theme().get_button(CONSTASTR("edit"));
    confirmb = gui->theme().get_button(CONSTASTR("confirmb"));
    cancelb = gui->theme().get_button(CONSTASTR("cancelb"));

    breakb = gui->theme().get_button(CONSTASTR("break"));
    pauseb = gui->theme().get_button(CONSTASTR("pause"));
    unpauseb = gui->theme().get_button(CONSTASTR("unpause"));
    exploreb = gui->theme().get_button(CONSTASTR("explore"));
}

file_transfer_s::~file_transfer_s()
{
    if (handle)
        CloseHandle(handle);
}

void file_transfer_s::prepare_fn( const ts::wstr_c &path_with_fn, bool overwrite )
{
    filename = path_with_fn;
    if (!overwrite)
    {
        ts::wstr_c origname = ts::fn_get_name<ts::wchar>(filename);
        int n = 1;
        while (ts::is_file_exists<ts::wchar>(filename))
            filename = ts::fn_change_name<ts::wchar>(filename, ts::wstr_c(origname).append_char('(').append_as_int(n++).append_char(')'));
    }

    trtime = ts::Time::current();
    upd_message_item();
}

int file_transfer_s::progress(int &bps) const
{
    bps = bytes_per_sec;
    return (int)(progrez * 100 / filesize);
}

void file_transfer_s::pause_by_remote( bool p )
{
    if (p)
    {
        bytes_per_sec = -2;
        upd_message_item();
    } else
    {
        trtime = ts::Time::current();
        bytes_per_sec = 0;
    }
}

void file_transfer_s::pause_by_me(bool p)
{
    active_protocol_c *ap = prf().ap(sender.protoid);
    if (!ap) return;

    if (p)
    {
        ap->file_control(utag, FIC_PAUSE);
        bytes_per_sec = -1;
        upd_message_item();
    }
    else
    {
        ap->file_control(utag, FIC_UNPAUSE);
        trtime = ts::Time::current();
        bytes_per_sec = 0;
    }
}

void file_transfer_s::kill( file_control_e fctl )
{
    if (fctl != FIC_NONE)
    {
        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->file_control(utag, fctl);
    }

    if (handle && (!send || fctl != FIC_DONE)) // close before update message item
    {
        LARGE_INTEGER fsz = {0};
        if (!send) GetFileSizeEx(handle,&fsz);
        CloseHandle(handle);
        handle = nullptr;
        if ( (uint64)fsz.QuadPart != filesize || send)
        {
            post_s p;
            p.sender = sender;
            filename.insert(0, '*');
            p.message = filename;
            p.utag = msgitem_utag;
            prf().change_history_item(historian, p, HITM_MESSAGE);
            if (contact_c * h = contacts().find(historian)) h->iterate_history([this](post_s &p)->bool {
                if (p.utag == msgitem_utag)
                {
                    p.message = filename;
                    return true;
                }
                return false;
            });
        
            if (!send) 
                DeleteFileW(filename.cstr() + 1);
        }
    }
    trtime = ts::Time::past();
    if (fctl != FIC_REJECT) upd_message_item();
    g_app->unregister_file_transfer(utag);
}

void file_transfer_s::query( uint64 offset_, int sz )
{
    if (handle)
    {
        if (offset != offset_)
        {
            LARGE_INTEGER li;
            li.QuadPart = offset_;
            SetFilePointer(handle, li.LowPart, &li.HighPart, FILE_BEGIN);
            offset = offset_;
        }
        sz = ts::tmin<int>(sz, (int)(filesize - offset));
        ts::tmp_buf_c b(sz,true);
        DWORD r;
        if (!ReadFile(handle, b.data(), sz, &r, nullptr))
        {
            kill();
            return;
        }

        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->file_portion(utag, offset_, b.data(), sz);

        if (sz)
        {
            if (bytes_per_sec < 0)
            {
                trtime = ts::Time::current();
                bytes_per_sec = 0;
            } else
            {
                ts::Time t = ts::Time::current();
                int mspersize = t - trtime;
                trtime = t;
                if (mspersize > 0)
                {
                    bytes_per_sec = sz * 1000 / mspersize;
                }
            }
            upd_message_item();
        }

        offset += sz;
        progrez += sz;

    }
}

void file_transfer_s::save(uint64 offset_, const ts::buf0_c&data)
{
    if (handle == nullptr)
    {
        handle = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            handle = nullptr;
            kill();
            return;
        }
        if (contact_c *c = contacts().find(historian))
            if (c->gui_item)
                c->gui_item->getengine().redraw();
    }
    if (offset_ + data.size() > filesize)
    {
        kill();
        return;
    }
    if ( offset != offset_ )
    {
        LARGE_INTEGER li;
        li.QuadPart = offset_;
        SetFilePointer(handle, li.LowPart, &li.HighPart, FILE_BEGIN);
        offset = offset_;
    }

    DWORD w;
    WriteFile(handle, data.data(), data.size(), &w, nullptr);
    if ((ts::aint)w != data.size())
    {
        kill();
        return;
    }

    offset += data.size();
    progrez += data.size();

    if (data.size())
    {
        if (bytes_per_sec >= 0)
        {
            ts::Time t = ts::Time::current();
            int mspersize = t - trtime;
            trtime = t;
            if (mspersize > 0)
            {
                bytes_per_sec = data.size() * 1000 / mspersize;
            }
        }
        upd_message_item();
    }

}

void file_transfer_s::upd_message_item()
{
    if (msgitem_utag)
    {
        post_s p;
        p.type = send ? MTA_SEND_FILE : MTA_RECV_FILE;
        p.message = filename;
        p.utag = msgitem_utag;
        p.sender = sender;
        p.receiver = contacts().get_self().getkey();
        gmsg<ISOGM_SUMMON_POST>(p, true).send();
    } else if (contact_c *c = contacts().find(sender))
    {
        gmsg<ISOGM_MESSAGE> msg(c, &contacts().get_self(), send ? MTA_SEND_FILE : MTA_RECV_FILE);
        msg.create_time = now();
        msg.post.message = filename;
        msg.post.utag = prf().uniq_history_item_tag();
        msgitem_utag = msg.post.utag;
        msg.send();
    }
}
