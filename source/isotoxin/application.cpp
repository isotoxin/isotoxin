#include "isotoxin.h"

//-V:theme:807

application_c *g_app = nullptr;

#ifndef _FINAL
void dotests();
#endif

GM_PREPARE( ISOGM_COUNT );

static bool __toggle_search_bar(RID, GUIPARAM)
{
    bool sbshow = prf().get_options().is(UIOPT_SHOW_SEARCH_BAR);
    prf().set_options( sbshow ? 0 : UIOPT_SHOW_SEARCH_BAR, UIOPT_SHOW_SEARCH_BAR );
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS, UIOPT_SHOW_SEARCH_BAR).send();
    return true;
}

static bool __toggle_tagfilter_bar(RID, GUIPARAM)
{
    bool sbshow = prf().get_options().is(UIOPT_TAGFILETR_BAR);
    prf().set_options(sbshow ? 0 : UIOPT_TAGFILETR_BAR, UIOPT_TAGFILETR_BAR);
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS, UIOPT_TAGFILETR_BAR).send();
    return true;
}

static bool __toggle_newcon_bar(RID, GUIPARAM)
{
    bool sbshow = prf().get_options().is(UIOPT_SHOW_NEWCONN_BAR);
    prf().set_options(sbshow ? 0 : UIOPT_SHOW_NEWCONN_BAR, UIOPT_SHOW_NEWCONN_BAR);
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS, UIOPT_SHOW_NEWCONN_BAR).send();
    return true;
}

extern parsed_command_line_s g_commandline;

namespace
{
struct load_spellcheckers_s : public ts::task_c
{
    ts::array_del_t< Hunspell, 0 > spellcheckers;
    ts::wstrings_c fns;
    ts::wstr_c fn;
    ts::buf0_c aff, dic;

    load_spellcheckers_s():fn( CONSTWSTR("spellcheck/*.aff") )
    {
        ts::g_fileop->find(fns, fn, false);
    }

    /*virtual*/ int iterate(int pass) override
    {
        if ( fn.is_empty() ) return R_DONE;
        if (aff.size() == 0 || dic.size() == 0) return R_RESULT_EXCLUSIVE;

        hunspell_file_s aff_file_data(aff.data(), aff.size());
        hunspell_file_s dic_file_data(dic.data(), dic.size());

        Hunspell *hspl = TSNEW(Hunspell, aff_file_data, dic_file_data);
        spellcheckers.add(hspl);

        return fns.size() ? R_RESULT_EXCLUSIVE : R_DONE;
    }
    /*virtual*/ void result() override
    {
        if (fns.size() == 0)
        {
            fn.clear();
            return;
        }
        ts::wstr_c afffn = fns.get_last_remove();

        fn.set_length(11 /* length of "spellcheck/" */).append(afffn);
        aff.load_from_file(fn);
        if (0 == aff.size())
        {
            fn.clear();
            return;
        }
        fn.set_length(fn.get_length() - 3).append(CONSTWSTR("dic"));
        dic.load_from_file(fn);
        if (0 == dic.size())
            fn.clear();

    }

    /*virtual*/ void done(bool canceled) override
    {
        if (!canceled && spellcheckers.size())
            g_app->spellchecker.set_spellcheckers(std::move(spellcheckers));

        __super::done(canceled);
    }

};
struct check_word_task : public ts::task_c
{
    ts::astrings_c checkwords;
    ts::iweak_ptr<spellchecker_s> splchk;

    ts::str_c w;
    ts::astrings_c suggestions;
    bool is_valid = false;

    check_word_task()
    {
    }

    /*virtual*/ int iterate(int pass) override
    {
        auto lr = g_app->spellchecker.lock(this);
        if (application_c::splchk_c::LOCK_EMPTY == lr) return R_CANCEL;
        if ( application_c::splchk_c::LOCK_OK == lr )
        {
            w = checkwords.get_last_remove();
            is_valid = g_app->spellchecker.check_one(w, suggestions);
            if (g_app->spellchecker.unlock(this))
                return R_CANCEL;
            return checkwords.size() ? R_RESULT_EXCLUSIVE : R_DONE;
        }

        return 1;
    }

    /*virtual*/ void result() override
    {
        if (!splchk.expired())
            splchk->check_result( w, is_valid, std::move(suggestions) );
    }

    /*virtual*/ void done(bool canceled) override
    {
        if (!canceled && !w.is_empty())
            result();

        __super::done(canceled);
    }

};

}

bool application_c::splchk_c::check_one( const ts::str_c &w, ts::astrings_c &suggestions )
{
    ASSERT( busy );

    suggestions.clear();

    ts::tmp_pointers_t<Hunspell, 2> sugg;
    for( Hunspell *hspl : spellcheckers )
    {
        if (int csr = hspl->spell( w.cstr() ))
            return true; // good word
        sugg.add( hspl );
    }

    for (Hunspell *hspl : sugg)
    {
        char ** wlst;
        int cnt = hspl->suggest(&wlst, w.cstr());
        for(int i=0;i<cnt;++i)
            suggestions.add( ts::asptr(wlst[i]) );
        hspl->free_list(&wlst, cnt);
    }

    suggestions.kill_dups_and_sort();
    return false;
}

application_c::splchk_c::lock_rslt_e application_c::splchk_c::lock(void *prm)
{
    spinlock::auto_simple_lock l(sync);
    if ( busy ) return LOCK_BUSY;
    if ( EMPTY == state || unload_request ) return LOCK_EMPTY;
    if ( READY != state ) return LOCK_BUSY;

    busy = prm;
    return LOCK_OK;
}

bool application_c::splchk_c::unlock(void *prm)
{
    spinlock::auto_simple_lock l(sync);
    ASSERT( busy == prm );
    busy = nullptr;
    return unload_request;
}


void application_c::splchk_c::load()
{
    spinlock::auto_simple_lock l(sync);
    if (EMPTY == state)
    {
        state = LOADING;
        g_app->add_task(TSNEW(load_spellcheckers_s));
    }

}
void application_c::splchk_c::unload()
{
    spinlock::auto_simple_lock l(sync);
    if (nullptr != busy || LOADING == state)
    {
        unload_request = true;
        return;
    }
    if (READY == state)
    {
        spellcheckers.clear();
        state = EMPTY;
    }
}
void application_c::splchk_c::set_spellcheckers(ts::array_del_t< Hunspell, 0 > &&sa)
{
    spinlock::auto_simple_lock l(sync);

    if (nullptr != busy)
        return;

    if (unload_request)
    {
        spellcheckers.clear();
        unload_request = false;
        state = EMPTY;
        return;
    }
    
    spellcheckers = std::move( sa );
    state = spellcheckers.size() ? READY : EMPTY;
}

void application_c::splchk_c::check(ts::astrings_c &&checkwords, spellchecker_s *rsltrcvr)
{
    spinlock::auto_simple_lock l(sync);
    if (unload_request || spellcheckers.size() == 0 || LOADING == state)
    {
        rsltrcvr->undo_check(checkwords);
        return;
    }

    check_word_task *t = TSNEW(check_word_task);
    t->checkwords = std::move(checkwords);
    t->splchk = rsltrcvr;
    g_app->add_task(t);
}


application_c::application_c(const ts::wchar * cmdl)
{
    F_NEWVERSION = false;
    F_BLINKING_FLAG = false;
    F_UNREADICON = false;
    F_NEED_BLINK_ICON = false;
    F_BLINKING_ICON = false;
    F_SETNOTIFYICON = false;
    F_OFFLINE_ICON = true;
    F_ALLOW_AUTOUPDATE = false;
    F_READONLY_MODE = g_commandline.readonlymode;
    F_READONLY_MODE_WARN = g_commandline.readonlymode; // suppress warn
    F_MODAL_ENTER_PASSWORD = false;
    F_TYPING = false;
    F_CAPTURE_AUDIO_TASK = false;
    F_CAPTURING = false;
    F_SHOW_CONTACTS_IDS = false;

    autoupdate_next = now() + 10;
	g_app = this;
    cfg().load();
    if (cfg().is_loaded())
        load_profile_and_summon_main_rect(g_commandline.minimize);

#ifndef _FINAL
    dotests();
#endif

    register_kbd_callback( __toggle_search_bar, HOTKEY_TOGGLE_SEARCH_BAR );
    register_kbd_callback( __toggle_tagfilter_bar, HOTKEY_TOGGLE_TAGFILTER_BAR );
    register_kbd_callback( __toggle_newcon_bar, HOTKEY_TOGGLE_NEW_CONNECTION_BAR );
 
    register_capture_handler(this);
}


application_c::~application_c()
{
    m_avcontacts.clear(); // remove all av contacts before delete GUI
    unregister_capture_handler(this);
	g_app = nullptr;
}

ts::uint32 application_c::gm_handler(gmsg<ISOGM_EXPORT_PROTO_DATA>&d)
{
    if (!main) return 0;

    if (!d.buf.size())
    {
        ts::sys_beep(ts::SBEEP_ERROR);
        return 0;
    }

    const active_protocol_c *ap = prf().ap( d.protoid );
    if (!ap) return 0;

    ts::wstr_c fromdir;
    if (prf().is_loaded())
        fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());

    ts::wstr_c title(TTT("Export protocol data: $",393) / to_wstr(ap->get_desc()));

    ts::extension_s e[1];
    e[0].desc = CONSTWSTR("(*.*)");
    e[0].ext = CONSTWSTR("*.*");
    ts::extensions_s exts(e, 1);

    ts::wstr_c deffn(to_wstr(ap->get_tag()));
    if (deffn.equals(CONSTWSTR("tox")))  // not so good hardcode // TODO
    {
        fromdir = CONSTWSTR("%APPDATA%");
        ts::parse_env(fromdir);

        if (dir_present(ts::fn_join(fromdir, deffn)))
            fromdir = ts::fn_join(fromdir, deffn);
        ts::fix_path(fromdir, FNO_APPENDSLASH);
        deffn.set(CONSTWSTR("isotoxin_tox_save.tox"));
    }

    ts::wstr_c fn = HOLD(main)().getroot()->save_filename_dialog(fromdir, deffn, exts, title);
    if (!fn.is_empty())
        d.buf.save_to_file(fn);


    return 0;
}

void application_c::apply_debug_options()
{
    ts::astrmap_c d(cfg().debug());
    if (!prf().get_options().is(OPTOPT_POWER_USER))
        d.clear();

#if defined _DEBUG || defined _CRASH_HANDLER
    MINIDUMP_TYPE dump_type = (MINIDUMP_TYPE)(MiniDumpWithFullMemory /*| MiniDumpWithProcessThreadData*/ | MiniDumpWithDataSegs | MiniDumpWithHandleData /*| MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo*/);
    bool full_dump = d.get(CONSTASTR(DEBUG_OPT_FULL_DUMP)).as_int() != 0;
    if (!full_dump)
        dump_type = (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithHandleData);
    ts::exception_operator_c::set_dump_type(dump_type);
#endif

    F_SHOW_CONTACTS_IDS = d.get(CONSTASTR("contactids")).as_int() != 0;
}

ts::uint32 application_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass == 0)
    {
        if (CFG_DEBUG == ch.sp)
            apply_debug_options();

    }
    return 0;
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
        animation_c::allow_tick |= 1;
    else if (UE_MINIMIZED == e.evt)
        animation_c::allow_tick &= ~1;

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

    ts::wstr_c appnamecap = APPNAME_CAPTION;
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
                l.replace_all(CONSTWSTR("[i]"), CONSTWSTR("<i>"));
                l.replace_all(CONSTWSTR("[/i]"), CONSTWSTR("</i>"));
                l.replace_all(CONSTWSTR("[quote]"), CONSTWSTR("\""));
                l.replace_all(CONSTWSTR("[appname]"), CONSTWSTR(APPNAME));
                l.replace_all(CONSTWSTR(APPNAME), appnamecap);

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
    F_BLINKING_ICON = F_NEED_BLINK_ICON;
    F_UNREADICON = false;
    if (F_BLINKING_ICON)
    {
        F_UNREADICON = present_unread_blink_reason();
        F_BLINKING_FLAG = !F_BLINKING_FLAG;
        DEFERRED_UNIQUE_CALL(0.3, DELEGATE(this, flash_notification_icon), 0);
    }
    F_SETNOTIFYICON = true;
    return true;
}

HICON application_c::app_icon(bool for_tray)
{
    if (!for_tray)
        return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(IDI_ICON_APP)); 

    auto actual_icon_idi = [this]( bool with_message )->int
    {
        if (F_OFFLINE_ICON) return with_message ? IDI_ICON_OFFLINE_MSG : IDI_ICON_OFFLINE;
        switch (contacts().get_self().get_ostate())
        {
        case COS_AWAY:
            return with_message ? IDI_ICON_AWAY_MSG : IDI_ICON_AWAY;
        case COS_DND:
            return with_message ? IDI_ICON_DND_MSG : IDI_ICON_DND;
        }
        return with_message ? IDI_ICON_ONLINE_MSG : IDI_ICON_ONLINE;
    };

    if (F_UNREADICON)
        return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE( actual_icon_idi( F_BLINKING_FLAG ) ));

    if (F_BLINKING_ICON)
        return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE( F_BLINKING_FLAG ? actual_icon_idi( false ) : IDI_ICON_HOLLOW  ));

    return LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(actual_icon_idi(false)));
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
        case LL_CTXMENU_COPY: return loc_text(loc_copy);
        case LL_CTXMENU_CUT: return TTT("Cut",93);
        case LL_CTXMENU_PASTE: return TTT("Paste",94);
        case LL_CTXMENU_DELETE: return TTT("Delete",95);
        case LL_CTXMENU_SELALL: return TTT("Select all",96);
        case LL_ABTT_CLOSE:
            if (cfg().collapse_beh() == 2)
            {
                return TTT("Minimize to notification area[br](Hold Ctrl key to exit)",122);
            }
            return loc_text(loc_exit);
        case LL_ABTT_MAXIMIZE: return TTT("Expand",4);
        case LL_ABTT_NORMALIZE: return TTT("Normal size",5);
        case LL_ABTT_MINIMIZE:
            if (cfg().collapse_beh() == 1)
                return TTT("Minimize to notification area",123);
            return TTT("Minimize",6);
        case LL_ANY_FILES:
            return loc_text( loc_anyfiles );
    }
    return __super::app_loclabel(ll);
}

/*virtual*/ void application_c::app_b_minimize(RID mr)
{
    if (cfg().collapse_beh() == 1)
        MODIFY(mr).micromize(true);
    else
        __super::app_b_minimize(mr);
}
/*virtual*/ void application_c::app_b_close(RID mr)
{
    if (GetKeyState(VK_CONTROL) >= 0 && cfg().collapse_beh() == 2)
        MODIFY(mr).micromize(true);
    else
        __super::app_b_close(mr);
}
/*virtual*/ void application_c::app_path_expand_env(ts::wstr_c &path)
{
    path_expand_env(path, ts::wstr_c(CONSTWSTR("0")));
}

/*virtual*/ void application_c::app_font_par(const ts::str_c&fn, ts::font_params_s&fprm)
{
    if (prf().is_loaded())
    {
        if (fn.begins(CONSTASTR("conv_text")))
        {
            float k = prf().fontscale_conv_text();
            fprm.size.x = ts::lround(k * fprm.size.x);
            fprm.size.y = ts::lround(k * fprm.size.y);
        }
    }
}

/*virtual*/ void application_c::do_post_effect()
{
    while( m_post_effect.is(PEF_APP) )
    {
        gmsg<ISOGM_DO_POSTEFFECT> x( m_post_effect.__bits );
        m_post_effect.clear(PEF_APP);
        x.send();
    }
    __super::do_post_effect();
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
    prf().check_aps();

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
                gmsg<ISOGM_NEWVERSION>(cfg().autoupdate_newver(), gmsg<ISOGM_NEWVERSION>::E_OK).send();
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

    if (prf().manual_cos() == COS_ONLINE)
    {
        contact_online_state_e c = contacts().get_self().get_ostate();
        if (!F_TYPING && c == COS_AWAY && prf().get_options().is(UIOPT_KEEPAWAY))
        {
            // keep away status
        } else
        {
            contact_online_state_e cnew = COS_ONLINE;

            if (prf().get_options().is(UIOPT_AWAYONSCRSAVER))
            {
                BOOL scrsvrun = FALSE;
                SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &scrsvrun, 0);
                if (scrsvrun)
                    cnew = COS_AWAY, F_TYPING = false;
            }

            int imins = prf().inactive_time();
            if (imins > 0)
            {
                LASTINPUTINFO lii = { (UINT)sizeof(LASTINPUTINFO) };
                GetLastInputInfo(&lii);
                int cimins = (GetTickCount() - lii.dwTime) / 60000;
                if (cimins >= imins)
                    cnew = COS_AWAY, F_TYPING = false;
            }

            if (c != cnew)
                set_status(cnew, false);
        }
    }

}

void application_c::set_status(contact_online_state_e cos_, bool manual)
{
    if (manual)
        prf().manual_cos(cos_);

    contacts().get_self().subiterate([&](contact_c *c) {
        c->set_ostate(cos_);
    });
    contacts().get_self().set_ostate(cos_);
    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_ONLINESTATUS).send();

}

application_c::blinking_reason_s &application_c::new_blink_reason(const contact_key_s &historian)
{
    for (blinking_reason_s &fr:m_blink_reasons)
    {
        if (fr.historian == historian)
            return fr;
    }

    bool recrctls = false;
    if (prf().get_options().is(UIOPT_TAGFILETR_BAR))
        if (contact_root_c *r = contacts().rfind(historian))
            if (!r->match_tags(prf().bitags()))
                recrctls = true;

    blinking_reason_s &fr = m_blink_reasons.add();
    fr.historian = historian;

    if (recrctls)
        recreate_ctls(true, false);

    return fr;
}


void application_c::update_blink_reason(const contact_key_s &historian_key)
{
    if (g_app->is_inactive(false))
        return;

    if (blinking_reason_s *flr = g_app->find_blink_reason(historian_key, true))
        flr->do_recalc_unread_now();
}

void application_c::blinking_reason_s::do_recalc_unread_now()
{
    if (contact_root_c *hi = contacts().rfind(historian))
    {
        if (flags.is(F_INVITE_FRIEND))
        {
            bool invite = false;
            hi->subiterate([&](contact_c *c) { if (c->get_state() == CS_INVITE_RECEIVE) invite = true; });
            if (!invite) friend_invite(false);
        }

        if (flags.is(F_RECALC_UNREAD) || (hi->gui_item && hi->gui_item->getprops().is_active()))
        {
            if (is_file_download_process() || is_file_download_request())
            {
                if (!g_app->present_file_transfer_by_historian(historian))
                    file_download_remove(0);
            }

            set_unread(hi->calc_unread());
            flags.clear(F_RECALC_UNREAD);
        }
    }
}

bool application_c::blinking_reason_s::tick()
{
    if (flags.is(F_RECALC_UNREAD))
        do_recalc_unread_now();

    if (!flags.is(F_CONTACT_BLINKING))
    {
        if (contact_need_blink())
        {
            flags.set(F_CONTACT_BLINKING);
            nextblink = ts::Time::current() + 300;
        } else
            flags.set(F_BLINKING_FLAG); // set it
    }

    if (flags.is(F_CONTACT_BLINKING))
    {
        if ((ts::Time::current() - nextblink) > 0)
        {
            nextblink = ts::Time::current() + 300;
            flags.invert(F_BLINKING_FLAG);
            flags.set(F_REDRAW);
        }
        if (!contact_need_blink())
            flags.clear(F_CONTACT_BLINKING);
    }

    if (flags.is(F_REDRAW))
    {
        if (contact_c *h = contacts().find(historian))
            h->redraw();
        flags.clear(F_REDRAW);
    }
    return is_blank();
}

/*virtual*/ void application_c::app_loop_event()
{
    F_NEED_BLINK_ICON = false;
    for(int i=m_blink_reasons.size()-1;i>=0;--i)
    {
        blinking_reason_s &br = m_blink_reasons.get(i);
        if (br.tick())
        {
            m_blink_reasons.remove_fast(i);
            continue;
        }
        F_NEED_BLINK_ICON |= br.notification_icon_need_blink();
    }

    if (F_NEED_BLINK_ICON && !F_BLINKING_ICON)
        flash_notification_icon();

    if (F_SETNOTIFYICON)
    {
        set_notification_icon();
        F_SETNOTIFYICON = false;
    }

    m_tasks_executor.tick();
    resend_undelivered_messages();

    if (F_PROTOSORTCHANGED)
    {
        F_PROTOSORTCHANGED = false;
        gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_ACTIVEPROTO_SORT).send();
    }

}

/*virtual*/ void application_c::app_fix_sleep_value(int &sleep_ms)
{
    UNSTABLE_CODE_PROLOG

    F_CAPTURING = s3::is_capturing();
    if (F_CAPTURING)
    {
        auto datacaptureaccept = [](const void *data, int size, void * /*context*/)
        {
            g_app->handle_sound_capture(data, size);
        };

        s3::capture_tick(datacaptureaccept, nullptr);
        sleep_ms = 1;
    }

    for( av_contact_s &avc : m_avcontacts )
    {
        if ( avc.is_camera_on() )
        {
            avc.camera_tick();
            sleep_ms = 1;
        }
    }

    UNSTABLE_CODE_EPILOG
        
}

void application_c::select_last_unread_contact()
{
    time_t latest = 0;
    contact_key_s historian;
    for (blinking_reason_s &br : m_blink_reasons)
    {
        if (br.notification_icon_need_blink())
        {
            if (br.last_update > latest)
            {
                latest = br.last_update;
                historian = br.historian;
            }
        }
    }
    if (latest)
    {
        if (contact_root_c *h = contacts().rfind(historian))
        {
            if (h->gui_item)
            {
                gui_contactlist_c &cl = HOLD(h->gui_item->getparent()).as<gui_contactlist_c>();
                cl.scroll_to_child(&h->gui_item->getengine(), false);
            }

            h->reselect();
        }
    } else
    {
        if (active_contact_item)
        {
            gui_contactlist_c &cl = HOLD(active_contact_item->getparent()).as<gui_contactlist_c>();
            cl.scroll_to_child(&active_contact_item->getengine(), false);
        } else if (gui_contact_item_c *active = contacts().get_self().gui_item)
        {
            gui_contactlist_c &cl = HOLD(active->getparent()).as<gui_contactlist_c>();
            cl.scroll_to_begin();
        }
    }
}

/*virtual*/ void application_c::app_notification_icon_action(naction_e act, RID iconowner)
{
    HOLD m(iconowner);

    if (act == NIA_L2CLICK)
    {
        if (m().getprops().is_collapsed())
        {
            if (F_MODAL_ENTER_PASSWORD)
            {
                TSNEW(gmsg<ISOGM_APPRISE>)->send_to_main_thread();

            } else
            {
                MODIFY(iconowner).decollapse();
                select_last_unread_contact();
            }
        } else
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
            if (prf().is_loaded())
            {
                add_status_items(m);
                m.add_separator();
            }
            m.add(loc_text(loc_exit), 0, handlers::m_exit);
            gui_popup_menu_c::show(menu_anchor_s(true, menu_anchor_s::RELPOS_TYPE_SYS), m, true);
            g_app->set_notification_icon(); // just remove hint

        DEFERRED_EXECUTION_BLOCK_END(0)
    }
}

namespace
{
    struct profile_loader_s
    {
        ts::uint8 k[CC_SALT_SIZE + CC_HASH_SIZE];
        ts::wstr_c wpn; // full filename
        ts::wstr_c storwpn; // just name (for config)
        ts::wstr_c prevname; // prev profile name

        enum stage_e
        {
            STAGE_CHECK_DB,
            STAGE_LOAD_NO_PASSWORD,
            STAGE_ENTER_PASSWORD,
            STAGE_CLEANUP_UI,
            STAGE_LOAD_WITH_PASSWORD,
        } st = STAGE_CHECK_DB;
        db_check_e chk = DBC_IO_ERROR;
        bool modal;
        bool decollapse = false;

        profile_loader_s(bool modal):modal(modal)
        {
        }

        static bool load( const ts::wstr_c&name, bool modal ); // return true if encrypted

        void tick_it()
        {
            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, tick), nullptr);
        }
        bool tick(RID, GUIPARAM)
        {
            switch (st)
            {
            case STAGE_CHECK_DB:
                {
                    if (DBC_IO_ERROR == chk)
                    {
                        profile_c::mb_error_load_profile(wpn, PLR_CONNECT_FAILED);
                        die();
                        return true;
                    }
                    if (DBC_NOT_DB == chk)
                    {
                        dialog_msgbox_c::mb_error(TTT("File [b]$[/b] is not profile", 389) / wpn).summon();
                        die();
                        return true;
                    }
                    if (DBC_DB_ENCRTPTED == chk)
                    {
                        st = STAGE_ENTER_PASSWORD;
                        tick_it();
                        break;
                    }
                    st = STAGE_LOAD_NO_PASSWORD;
                }
                // no break
            case STAGE_LOAD_NO_PASSWORD:
            case STAGE_LOAD_WITH_PASSWORD:

                contacts().unload();
                {
                    auto rslt = prf().xload(wpn, STAGE_LOAD_WITH_PASSWORD == st ? k : nullptr);
                    if (PLR_OK == rslt)
                    {
                        cfg().profile(storwpn);

                    } else if (!prevname.is_empty())
                    {
                        ts::wstr_c badpf = wpn;
                        wpn = prevname;
                        prevname.clear();
                        storwpn = wpn;
                        profile_c::path_by_name(wpn);
                        chk = check_db(wpn, k);
                        st = chk == DBC_DB ? STAGE_CHECK_DB : STAGE_CLEANUP_UI;
                        profile_c::mb_error_load_profile(badpf, rslt, modal);
                        tick_it();
                        break;
                    } else
                    {
                        profile_c::mb_error_load_profile(wpn, rslt, modal && rslt != PLR_CORRUPT_OR_ENCRYPTED);
                    }
                }

                // no break here
            case STAGE_CLEANUP_UI:

                contacts().update_meta();
                contacts().get_self().reselect();
                g_app->recreate_ctls(true, true);

                if (decollapse)
                    TSNEW(gmsg<ISOGM_APPRISE>)->send_to_main_thread();

                die();
                return true;

            case STAGE_ENTER_PASSWORD:

                g_app->F_MODAL_ENTER_PASSWORD = modal;
                SUMMON_DIALOG<dialog_entertext_c>(UD_ENTERPASSWORD, dialog_entertext_c::params(
                    UD_ENTERPASSWORD,
                    gui_isodialog_c::title(title_enter_password),
                    TTT("Profile [b]$[/b] is encrypted.[br]You have to enter password to load encrypted profile.",390) / storwpn,
                    ts::wstr_c(),
                    ts::str_c(),
                    DELEGATE(this, password_entered),
                    DELEGATE(this, password_not_entered)));

                break;
            }

            return true;
        }

        bool password_entered(const ts::wstr_c &passwd, const ts::str_c &)
        {
            st = STAGE_LOAD_WITH_PASSWORD;
            gen_passwdhash( k + CC_SALT_SIZE, passwd );
            tick_it();
            decollapse = !g_commandline.minimize;
            g_app->F_MODAL_ENTER_PASSWORD = false;
            return true;
        }

        bool password_not_entered(RID, GUIPARAM)
        {
            st = STAGE_CLEANUP_UI;
            tick_it();
            decollapse = !g_commandline.minimize;
            g_app->F_MODAL_ENTER_PASSWORD = false;
            return true;
        }


        ~profile_loader_s()
        {
            if (gui)
                gui->delete_event(DELEGATE(this, tick));
        }
        void die();
    };
    static UNIQUE_PTR(profile_loader_s) ploader;

    bool profile_loader_s::load(const ts::wstr_c&name, bool modal)
    {
        ploader.reset( TSNEW(profile_loader_s, modal) );
        ploader->prevname = cfg().profile();
        ploader->wpn = name;
        ploader->storwpn = name;
        if (ploader->storwpn.equals(ploader->prevname))
            ploader->prevname.clear();
        profile_c::path_by_name(ploader->wpn);
        ploader->chk = check_db(ploader->wpn, ploader->k);
        ploader->tick_it();
        return ploader->chk == DBC_DB_ENCRTPTED;
    }

    void profile_loader_s::die()
    {
        ploader.reset();
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
                dialog_msgbox_c::mb_error( TTT("Such profile already exists",49) ).summon();
                return false;
            }

            HANDLE f = CreateFileW( pn, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
            if (f == INVALID_HANDLE_VALUE)
            {
                dialog_msgbox_c::mb_error( TTT("Can't create profile ($)",50) / lasterror() ).summon();
                return true;
            }
            CloseHandle(f);
            

            ts::wstr_c profname = cfg().profile();
            if (profname.is_empty())
            {
                auto rslt = prf().xload(pn, nullptr);
                if (PLR_OK == rslt)
                {
                    dialog_msgbox_c::mb_info( TTT("Profile [b]$[/b] has created and set as default.",48) / prfn ).summon();
                    cfg().profile(storpn);
                } else
                    profile_c::mb_error_load_profile(pn, rslt);
            } else
            {
                dialog_msgbox_c::mb_info( TTT("Profile with name [b]$[/b] has created. You can switch to it using settings menu.",51) / prfn ).summon();
            }
            

            return true;
        }
        static void m_newprofile(const ts::str_c&)
        {
            ts::wstr_c defprofilename(CONSTWSTR("%USER%"));
            ts::parse_env(defprofilename);
            SUMMON_DIALOG<dialog_entertext_c>(UD_PROFILENAME, dialog_entertext_c::params(
                                                UD_PROFILENAME,
                                                gui_isodialog_c::title(title_profile_name),
                                                TTT("Enter profile name. It is profile file name. You can create any number of profiles and switch them any time. Detailed settings of current profile are available in settings dialog.",43),
                                                defprofilename,
                                                ts::str_c(),
                                                m_newprofile_ok,
                                                nullptr,
                                                check_profile_name));
        }

        static void m_switchto(const ts::str_c& prfn)
        {
            profile_loader_s::load( from_utf8(prfn), false );
        }
        static void m_about(const ts::str_c&)
        {
            SUMMON_DIALOG<dialog_about_c>(UD_ABOUT);
        }
        static void m_exit(const ts::str_c&)
        {
            sys_exit(0);
        }
    };

#ifndef _FINAL
    if (GetKeyState(VK_SHIFT)<0)
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
        if (wfn == profname)
        {
            mif = MIF_MARKED;
            if (prf().is_loaded()) mif |= MIF_DISABLED;
        }
        pm.add(TTT("Switch to [b]$[/b]",41) / wfn, mif, handlers::m_switchto, ts::to_utf8(wfn));
    }


    m.add( TTT("Settings",42), 0, handlers::m_settings );
    m.add_separator();
    m.add( TTT("About",356), 0, handlers::m_about );
    m.add_separator();
    m.add(loc_text(loc_exit), 0, handlers::m_exit);
    gui_popup_menu_c::show(r.call_get_popup_menu_pos(), m);

    return true;
}

void application_c::load_profile_and_summon_main_rect(bool minimize)
{
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
        cfg().profile(profname);
    }
    if (!profname.is_empty())
        minimize |= profile_loader_s::load( profname, true );

    if (F_READONLY_MODE)
    {
        profile_c::mb_warning_readonly( minimize );
    } else
    {
        summon_main_rect(minimize);
    }

}

void application_c::summon_main_rect(bool minimize)
{
    ts::ivec2 sz = cfg().get<ts::ivec2>(CONSTASTR("main_rect_size"), ts::ivec2(800, 600));
    ts::irect mr = ts::irect::from_lt_and_size(cfg().get<ts::ivec2>(CONSTASTR("main_rect_pos"), ts::wnd_get_center_pos(sz)), sz);

    redraw_collector_s dch;
    main = MAKE_ROOT<mainrect_c>( mr );

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
        w().aurl = ts::astrmap_c( cfg().debug() ).get( CONSTASTR("local_upd_url") );
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
    if (elevate())
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
#endif // _DEBUG

    struct verb
    {
        ts::sstr_t<-128> v;
        verb()  {}
        verb &operator/(int n) { v.append_as_int(n).append_char('.'); return *this; }
    } v;
    v / //-V609
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

namespace
{
    struct hardware_sound_capture_switch_s : public ts::task_c
    {
        ts::tbuf0_t<s3::Format> fmts;
        hardware_sound_capture_switch_s(const s3::Format *_fmts, int cnt):fmts(_fmts, cnt) {}
        hardware_sound_capture_switch_s() {}

        /*virtual*/ int iterate(int pass) override
        {
            if (fmts.count())
            {
                // start
                s3::Format fmtw;
                s3::start_capture(fmtw, fmts.begin(), fmts.count());
            } else
            {
                s3::stop_capture();
            }

            return R_DONE;
        }


        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && g_app)
            {
                g_app->F_CAPTURE_AUDIO_TASK = false;
                
                DEFERRED_EXECUTION_BLOCK_BEGIN(0)
                    g_app->check_capture();
                DEFERRED_EXECUTION_BLOCK_END(0)
            }

            __super::done(canceled);
        }

    };
}

void application_c::check_capture()
{
    if (F_CAPTURE_AUDIO_TASK)
        return;

    F_CAPTURING = s3::is_capturing();

    if (F_CAPTURING && !m_currentsc)
    {
        F_CAPTURE_AUDIO_TASK = true;
        add_task(TSNEW(hardware_sound_capture_switch_s));

    }  else if (!F_CAPTURING && m_currentsc)
    {
        F_CAPTURE_AUDIO_TASK = true;
        int cntf;
        const s3::Format *fmts = m_currentsc->formats(cntf);
        add_task(TSNEW(hardware_sound_capture_switch_s, fmts, cntf));

    } else if (F_CAPTURING && m_currentsc)
        s3::get_capture_format(m_currentsc->getfmt());

}

void application_c::start_capture(sound_capture_handler_c *h)
{
    struct checkcaptrue
    {
        application_c *app;
        checkcaptrue(application_c *app):app(app) {}
        ~checkcaptrue()
        {
            app->check_capture();
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

int application_c::get_avinprogresscount() const
{
    int cnt = 0;
    for (const av_contact_s &avc : m_avcontacts)
        if (av_contact_s::AV_INPROGRESS == avc.state)
            ++cnt;
    return cnt;
}

int application_c::get_avringcount() const
{
    int cnt = 0;
    for (const av_contact_s &avc : m_avcontacts)
        if (av_contact_s::AV_RINGING == avc.state)
            ++cnt;
    return cnt;
}

av_contact_s * application_c::find_avcontact_inprogress( contact_root_c *c )
{
    for (av_contact_s &avc : m_avcontacts)
        if (avc.c == c && av_contact_s::AV_INPROGRESS == avc.state)
            return &avc;
    return nullptr;
}

av_contact_s & application_c::get_avcontact( contact_root_c *c, av_contact_s::state_e st )
{
    for( av_contact_s &avc : m_avcontacts)
        if (avc.c == c)
        {
            if (av_contact_s::AV_NONE != st)
                avc.state = st;
            return avc;
        }
    av_contact_s &avc = m_avcontacts.addnew(c, st);
    avc.send_so(); // update stream options now
    return avc;
}

void application_c::del_avcontact(contact_root_c *c)
{
    for(int i=m_avcontacts.size()-1;i>=0;--i)
    {
        av_contact_s &avc = m_avcontacts.get(i);
        if (avc.c == c)
            m_avcontacts.remove_fast(i);
    }
}

void application_c::stop_all_av()
{
    gmsg<ISOGM_NOTICE>(nullptr, nullptr, NOTICE_KILL_CALL_INPROGRESS).send();

    while (m_avcontacts.size())
    {
        av_contact_s &avc = m_avcontacts.get(0);
        avc.c->stop_av();
    }

}

void application_c::update_ringtone( contact_root_c *rt, bool play_stop_snd )
{
    int avcount = get_avringcount();
    if (rt->is_ringtone())
        get_avcontact(rt, av_contact_s::AV_RINGING);
    else
        del_avcontact(rt);


    if (0 == avcount && get_avringcount())
    {
        (get_avinprogresscount() == 0) ? 
            play_sound(snd_ringtone, true) :
            play_sound(snd_ringtone2, true);

        if (prf().get_options().is(UIOPT_SHOW_INCOMING_CALL_BAR))
        {
            contact_c *ccc = nullptr;
            rt->subiterate([&](contact_c *c) { if (c->is_ringtone()) ccc = c; });
            if (ccc) {
                MAKE_ROOT<incoming_call_panel_c> xxx(ccc);
            }
        }

    } else if (avcount && 0 == get_avringcount())
    {
        stop_sound(snd_ringtone2);
        if (stop_sound(snd_ringtone) && play_stop_snd)
            play_sound(snd_call_cancel, false);
    }
}

av_contact_s * application_c::update_av( contact_root_c *avmc, bool activate, bool camera )
{
    ASSERT(avmc->is_meta() || avmc->getkey().is_group());

    av_contact_s *r = nullptr;

    int was_avip = get_avinprogresscount();
    
    if (activate)
    {
        av_contact_s &avc = get_avcontact(avmc, av_contact_s::AV_INPROGRESS);
        avc.camera(camera);

        if (!avmc->getkey().is_group())
            avmc->subiterate([&](contact_c *c) {
                if (c->is_av())
                    gmsg<ISOGM_NOTICE>(avmc, c, NOTICE_CALL_INPROGRESS).send();
            });
        r = &avc;

    } else
        del_avcontact(avmc);


    if (0 == was_avip && get_avinprogresscount())
        static_cast<sound_capture_handler_c*>(this)->start_capture();
    else if (0 == get_avinprogresscount() && was_avip)
        static_cast<sound_capture_handler_c*>(this)->stop_capture();

    if (activate)
        for (av_contact_s &avc : m_avcontacts)
            if (av_contact_s::AV_INPROGRESS == avc.state)
                avc.set_inactive(avc.c != avmc);


    if (active_contact_item && active_contact_item->contacted())
        if (avmc == &active_contact_item->getcontact())
            update_buttons_head(); // it updates some stuff

    return r;
}

/*virtual*/ void application_c::datahandler(const void *data, int size)
{
    contact_key_s current_receiver;

    for (const av_contact_s &avc : m_avcontacts)
    {
        if (av_contact_s::AV_INPROGRESS != avc.state)
            continue;

        if (avc.is_mic_off())
            continue;;

        if (avc.c->getkey().is_group())
        {
            current_receiver = avc.c->getkey();
            break;
        }

        avc.c->subiterate([&](contact_c *sc) {
            if (sc->is_av())
                current_receiver = sc->getkey();
        });

        if (!current_receiver.is_empty())
            break;;
    }

    if (!current_receiver.is_empty()) // only one contact receives sound at one time
        if (active_protocol_c *ap = prf().ap(current_receiver.protoid))
            ap->send_audio(current_receiver.contactid, capturefmt, data, size);
}

/*virtual*/ const s3::Format *application_c::formats(int &count)
{
    avformats.clear();

    for (const av_contact_s &avc : m_avcontacts)
    {
        if (av_contact_s::AV_INPROGRESS != avc.state)
            continue;

        if (avc.c->getkey().is_group())
        {
            if (active_protocol_c *ap = prf().ap(avc.c->getkey().protoid))
                avformats.set(ap->defaudio());
        }
        else avc.c->subiterate([this](contact_c *sc)
        {
            if (sc->is_av())
                if (active_protocol_c *ap = prf().ap(sc->getkey().protoid))
                    avformats.set(ap->defaudio());
        });

    }

    count = avformats.count();
    return count ? avformats.begin() : nullptr;
}


bool application_c::present_file_transfer_by_historian(const contact_key_s &historian)
{
    for (const file_transfer_s *ftr : m_files)
        if (ftr->historian == historian)
            return true;
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
    for (int qi=0;qi<m_undelivered.size();)
    {
        send_queue_s *q = m_undelivered.get(qi);

        if (0 == q->queue.size())
        {
            m_undelivered.remove_fast(qi);
            continue;
        }

        if ((q->receiver == rcv || rcv.is_empty()))
        {
            while ( !rcv.is_empty() || (ts::Time::current() - q->last_try_send_time) > 60000 /* try 2 resend every 1 minute */ )
            {
                q->last_try_send_time = ts::Time::current();
                contact_root_c *receiver = contacts().rfind( q->receiver );

                if (receiver == nullptr)
                {
                    q->queue.clear();
                    break;
                }

                contact_c *tgt = receiver->subget_for_send(); // get default subcontact for message target

                if (tgt == nullptr)
                {
                    q->queue.clear();
                    break;
                }

                gmsg<ISOGM_MESSAGE> msg(&contacts().get_self(), tgt, MTA_UNDELIVERED_MESSAGE);

                const post_s& post = q->queue.get(0);
                msg.post.recv_time = post.recv_time;
                msg.post.cr_time = post.cr_time;
                msg.post.utag = post.utag;
                msg.post.message_utf8 = post.message_utf8;
                msg.resend = true;
                msg.send();

                break; //-V612 // yeah. unconditional break
            }

            if (!rcv.is_empty())
                break;
        }
        ++qi;
    }
}

void application_c::kill_undelivered( uint64 utag )
{
    for (send_queue_s *q : m_undelivered)
    {
        int cnt = q->queue.size();
        for (int i = 0; i < cnt; ++i)
        {
            const post_s &qp = q->queue.get(i);
            if (qp.utag == utag)
            {
                q->queue.get_remove_slow(i);
                return;
            }
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
                if (qp.recv_time > p.recv_time)
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

void application_c::reload_fonts()
{
    __super::reload_fonts();
    preloaded_stuff().update_fonts();
}

bool application_c::load_theme( const ts::wsptr&thn, bool summon_ch_signal)
{
    if (!__super::load_theme(thn, false))
    {
        if (!ts::pwstr_c(thn).equals(CONSTWSTR("def")))
        {
            cfg().theme( CONSTWSTR("def") );
            return load_theme( CONSTWSTR("def") );
        }
        return false;
    }
    m_preloaded_stuff.reload();

    deftextcolor = ts::parsecolor<char>(theme().conf().get_string(CONSTASTR("deftextcolor")), ts::ARGB(0, 0, 0));
    errtextcolor = ts::parsecolor<char>(theme().conf().get_string(CONSTASTR("errtextcolor")), ts::ARGB(255, 0, 0));
    selection_color = ts::parsecolor<char>( theme().conf().get_string(CONSTASTR("selection_color")), ts::ARGB(255, 255, 0) );
    selection_bg_color = ts::parsecolor<char>( theme().conf().get_string(CONSTASTR("selection_bg_color")), ts::ARGB(100, 100, 255) );
    selection_bg_color_blink = ts::parsecolor<char>( theme().conf().get_string(CONSTASTR("selection_bg_color_blink")), ts::ARGB(0, 0, 155) );

    emoti().reload();

    load_locale(cfg().language());

    if (summon_ch_signal)
        gmsg<GM_UI_EVENT>(UE_THEMECHANGED).send();

    return true;
}

void preloaded_stuff_s::update_fonts()
{
    font_conv_name = &gui->get_font(CONSTASTR("conv_name"));
    font_conv_text = &gui->get_font(CONSTASTR("conv_text"));
    font_conv_time = &gui->get_font(CONSTASTR("conv_time"));
}

void preloaded_stuff_s::reload()
{
    update_fonts();

    const theme_c &th = gui->theme();

    contactheight = th.conf().get_string(CONSTASTR("contactheight")).as_int(55);
    mecontactheight = th.conf().get_string(CONSTASTR("mecontactheight")).as_int(60);
    minprotowidth = th.conf().get_string(CONSTASTR("minprotowidth")).as_int(100);
    protoiconsize = th.conf().get_string(CONSTASTR("protoiconsize")).as_int(10);
    common_bg_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("common_bg_color")), 0xffffffff);
    appname_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("appnamecolor")), ts::ARGB(0, 50, 0));
    found_mark_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("found_mark_color")), ts::ARGB(50, 50, 0));
    found_mark_bg_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("found_mark_bg_color")), ts::ARGB(255, 100, 255));
    achtung_content_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("achtung_content_color")), ts::ARGB(0, 0, 0));
    state_online_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("state_online_color")), ts::ARGB(0, 255, 0));
    state_away_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("state_away_color")), ts::ARGB(255, 255, 0));
    state_dnd_color = ts::parsecolor<char>(th.conf().get_string(CONSTASTR("state_dnd_color")), ts::ARGB(255, 0, 0));

    achtung_shift = ts::parsevec2(th.conf().get_string(CONSTASTR("achtung_shift")), ts::ivec2(0));

    icon[CSEX_UNKNOWN] = th.get_image(CONSTASTR("nosex"));
    icon[CSEX_MALE] = th.get_image(CONSTASTR("male"));
    icon[CSEX_FEMALE] = th.get_image(CONSTASTR("female"));

    groupchat = th.get_image(CONSTASTR("groupchat"));
    nokeeph = th.get_image(CONSTASTR("nokeeph"));
    achtung_bg = th.get_image(CONSTASTR("achtung_bg"));
    invite_send = th.get_image(CONSTASTR("invite_send"));
    invite_recv = th.get_image(CONSTASTR("invite_recv"));
    invite_rej = th.get_image(CONSTASTR("invite_rej"));
    online[COS_ONLINE] = th.get_image(CONSTASTR("online0"));
    online[COS_AWAY] = th.get_image(CONSTASTR("online1"));
    online[COS_DND] = th.get_image(CONSTASTR("online2"));
    offline = th.get_image(CONSTASTR("offline"));
    online_some = th.get_image(CONSTASTR("online_some"));

    callb = th.get_button(CONSTASTR("call"));
    fileb = th.get_button(CONSTASTR("file"));

    editb = th.get_button(CONSTASTR("edit"));
    confirmb = th.get_button(CONSTASTR("confirmb"));
    cancelb = th.get_button(CONSTASTR("cancelb"));

    breakb = th.get_button(CONSTASTR("break"));
    pauseb = th.get_button(CONSTASTR("pause"));
    unpauseb = th.get_button(CONSTASTR("unpause"));
    exploreb = th.get_button(CONSTASTR("explore"));

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
        auto w = query_task->sync.lock_write();
        w().rslt = query_task_s::rslt_kill;
        w().ftr = nullptr;
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

bool file_transfer_s::auto_confirm()
{
    bool image = image_loader_c::is_image_fn(to_utf8(filename));
    ts::wstr_c downf = image ? prf().download_folder_images() : prf().download_folder();
    path_expand_env(downf, ts::wmake<uint>(historian.contactid));
    ts::make_path(downf, 0);
    if (!ts::dir_present(downf))
        return false;

    prepare_fn(ts::fn_join(downf, filename), false);

    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_control(utag, FIC_ACCEPT);

    return true;
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
    int prc = (int)(rdata().progrez * 100 / filesize);
    if (prc > 100) prc = 100;
    return prc;
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

    if (!upload && !accepted && (fctl == FIC_NONE || fctl == FIC_DISCONNECT))
    {
        // kill without accept - just do nothing
        g_app->new_blink_reason(historian).file_download_remove(utag);
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
            if (contact_root_c * h = contacts().rfind(historian)) h->iterate_history([this](post_s &p)->bool {
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
        {
            MoveFileW(filename_on_disk + CONSTWSTR(".!rcv"), filename_on_disk);
            play_sound( snd_file_received, false );
        }
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
        if (contact_root_c * h = contacts().rfind(historian)) h->iterate_history([this](post_s &p)->bool {
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
            tr(r.offset0, r.offset1);
            return;
        }
        if (_offset0 == r.offset1)
        {
            r.offset1 = _offset1;
            transfered.remove_slow(i);
            tr(r.offset0, r.offset1);
            return;
        }

        return;
    }

    range_s &rr = transfered.add();
    rr.offset0 = _offset0;
    rr.offset1 = _offset1;
}

query_task_s::query_task_s(file_transfer_s *ftr)
{
    sync.lock_write()().ftr = ftr;
}

query_task_s::~query_task_s()
{
    auto w = sync.lock_write();
    if (w().ftr)
        w().ftr->query_task = nullptr;
}


/*virtual*/ int query_task_s::iterate(int pass)
{
    auto rr = sync.lock_read();
    if (rslt_kill == rr().rslt || !rr().ftr)
        return R_CANCEL;

    if (rslt_idle == rr().rslt)
    {
        rr.unlock();
        Sleep(0);
        return pass + 1;
    }

    job_s cj = rr().current_job;
    rr.unlock();

    if( cj.sz == 0 || cj.offset == 0xFFFFFFFFFFFFFFFFull )
    {
        auto d = sync.lock_write();
        if (d().jobarray.size())
        {
            d().current_job = d().jobarray.get_remove_slow();
            d().rslt = rslt_inprogress;
            cj = d().current_job;
        } else
        {
            d().rslt = rslt_idle;
            return pass + 1;
        }
    }

    rr = sync.lock_read();
    if (!rr().ftr)
        return R_CANCEL;
    HANDLE handler = rr().ftr->file_handle(); //-V807
    int protoid = rr().ftr->sender.protoid;
    uint64 utag = rr().ftr->utag;

    // always set file pointer
    LARGE_INTEGER li;
    li.QuadPart = cj.offset;
    SetFilePointer(handler, li.LowPart, &li.HighPart, FILE_BEGIN);

    int sz = (int)ts::tmin<int64>(cj.sz, (int64)(rr().ftr->filesize - cj.offset));
    ts::tmp_buf_c b(sz, true);

    rr.unlock();

    DWORD r;
    if (!ReadFile(handler, b.data(), sz, &r, nullptr))
    {
        sync.lock_write()().rslt = rslt_kill;
        return R_DONE;
    }

    if (active_protocol_c *ap = prf().ap(protoid))
        ap->file_portion(utag, cj.offset, b.data(), sz);

    if (cj.sz)
    {
        auto wftr = sync.lock_write();
        if (!wftr().ftr)
            return R_CANCEL;

        auto wdata = wftr().ftr->data.lock_write();

        if (wdata().bytes_per_sec >= file_transfer_s::BPSSV_ALLOW_CALC)
        {
            wdata().tr(cj.offset, cj.offset + cj.sz);
            wdata().upduitime += wdata().deltatime(true);

            if (wdata().upduitime > 0.3f)
            {
                wdata().upduitime -= 0.3f;
                wdata().bytes_per_sec = ts::lround((float)wdata().trsz() / 0.3f);
                wdata().transfered.clear();
                wftr().ftr->update_item = true;
            }
        }

        //wdata().offset += cj.sz;
        wdata().progrez += cj.sz;
    }

    auto d = sync.lock_write();

    d().current_job.offset = 0xFFFFFFFFFFFFFFFFull;
    d().current_job.sz = 0;

    if (d().jobarray.size())
    {
        d().current_job = d().jobarray.get_remove_slow();
        d().rslt = rslt_inprogress;
        return R_RESULT;
    }

    if (rslt_kill == d().rslt)
        return R_CANCEL;

    d().rslt = rslt_idle;
    return pass + 1;

}
/*virtual*/ void query_task_s::done(bool canceled)
{
    auto w = sync.lock_write();
    file_transfer_s *ftr = w().ftr;

    if (canceled || ftr == nullptr)
    {
        __super::done(canceled);
        return;
    }

    if (w().rslt == rslt_kill)
    {
        ftr->kill();
        __super::done(canceled);
        return;
    }

    ASSERT( w().rslt == rslt_ok );

    ftr->upd_message_item(false);

    __super::done(canceled);
}

/*virtual*/ void query_task_s::result()
{
    file_transfer_s *ftr = sync.lock_write()().ftr;
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
        ASSERT( d().rslt == query_task_s::rslt_idle || d().rslt == query_task_s::rslt_inprogress );
        d().rslt = query_task_s::rslt_inprogress;
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
    uint64 offset = fsz.QuadPart > 1024 ? fsz.QuadPart - 1024 : 0;
    wdata().progrez = offset;
    fsz.QuadPart = offset;
    SetFilePointer(wdata().handle, fsz.LowPart, &fsz.HighPart, FILE_BEGIN);

    accepted = true;

    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_resume( utag, offset );

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
            c->redraw();
    }
    if (offset_ + bdata.size() > filesize)
    {
        kill();
        return;
    }

    auto wdata = data.lock_write();

    LARGE_INTEGER li;
    li.QuadPart = offset_;
    SetFilePointer(wdata().handle, li.LowPart, &li.HighPart, FILE_BEGIN);

    DWORD w;
    WriteFile(wdata().handle, bdata.data(), bdata.size(), &w, nullptr);
    if ((ts::aint)w != bdata.size())
    {
        kill();
        return;
    }

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
                wdata().bytes_per_sec = ts::lround((float)wdata().trsz() / 0.3f);
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

        if (!upload)
            g_app->new_blink_reason(historian).file_download_progress_add(utag);

    } else if (contact_c *c = contacts().find(sender))
    {
        gmsg<ISOGM_MESSAGE> msg(c, &contacts().get_self(), upload ? MTA_SEND_FILE : MTA_RECV_FILE);
        msg.post.cr_time = now();
        msg.post.message_utf8 = to_utf8(filename_on_disk);
        if (msg.post.message_utf8.ends(CONSTASTR(".!rcv")))
            msg.post.message_utf8.trunc_length(5);
        msg.post.utag = prf().uniq_history_item_tag();
        msgitem_utag = msg.post.utag;
        msg.send();
    }
}

av_contact_s::av_contact_s(contact_root_c *c, state_e st) :c(c), state(st)
{
    inactive = false;
    dirty_cam_size = true;
    if (st == AV_INPROGRESS)
        options_handler.reset( TSNEW( OPTIONS_HANDLER, DELEGATE(this, ohandler) ) );
    starttime = now();
}

bool av_contact_s::ohandler( gmsg<ISOGM_PEER_STREAM_OPTIONS> &rso )
{
    remote_so = rso.so;
    remote_sosz = rso.videosize;
    dirty_cam_size = true;
    return false;
}

void av_contact_s::update_btn_face_camera(gui_button_c &btn)
{
    is_camera_on() ?
        btn.set_face_getter(BUTTON_FACE(on_camera)) :
    btn.set_face_getter(BUTTON_FACE(off_camera));
}

bool av_contact_s::b_mic_switch(RID, GUIPARAM p)
{
    mic_switch();
    is_mic_off() ?
        ((gui_button_c *)p)->set_face_getter(BUTTON_FACE(unmute_mic)) :
        ((gui_button_c *)p)->set_face_getter(BUTTON_FACE(mute_mic));
    return true;
}

bool av_contact_s::b_speaker_switch(RID, GUIPARAM p)
{
    speaker_switch();
    is_speaker_off() ?
        ((gui_button_c *)p)->set_face_getter(BUTTON_FACE(unmute_speaker)) :
        ((gui_button_c *)p)->set_face_getter(BUTTON_FACE(mute_speaker));
    return true;
}

void av_contact_s::mic_off()
{
    int oso = cur_so();
    RESETFLAG(so, SO_SENDING_AUDIO);
    if (cur_so() != oso)
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::camera_switch()
{
    int oso = cur_so();
    INVERTFLAG(so, SO_SENDING_VIDEO);
    if (cur_so() != oso)
    {
        if (!is_camera_on())
            vsb.reset();

        send_so();
        c->redraw();
    }
}

void av_contact_s::mic_switch()
{
    int oso = cur_so();
    INVERTFLAG( so, SO_SENDING_AUDIO );
    if (cur_so() != oso)
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::update_speaker()
{
    if (c->getkey().is_group())
    {
        g_app->mediasystem().voice_mute([this](uint64 id)->bool {

            contact_key_s &ck = ts::ref_cast<contact_key_s>(id);
            return (c->getkey().protoid | (c->getkey().contactid << 16)) == ck.protoid;

        }, is_speaker_off());
    }
    else
        c->subiterate([this](contact_c *cc) {
        if (cc->is_av())
            g_app->mediasystem().voice_mute(ts::ref_cast<uint64>(cc->getkey()), is_speaker_off());
    });

}

void av_contact_s::speaker_switch()
{
    int oso = cur_so();
    INVERTFLAG(so, SO_RECEIVING_AUDIO);

    if (cur_so() != oso)
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::camera( bool on )
{
    int oso = cur_so();
    INITFLAG( so, SO_SENDING_VIDEO, on );

    if (cur_so() != oso)
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_recv_video(bool allow_recv)
{
    int oso = cur_so();
    INITFLAG( so, SO_RECEIVING_VIDEO, allow_recv );

    if (cur_so() != oso)
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_inactive(bool inactive_)
{
    int oso = cur_so();
    inactive = inactive_;

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_so_audio(bool inactive_, bool enable_mic, bool enable_speaker)
{
    int oso = cur_so();
    inactive = inactive_;

    INITFLAG( so, SO_SENDING_AUDIO, enable_mic );
    INITFLAG( so, SO_RECEIVING_AUDIO, enable_speaker );

    if (cur_so() != oso)
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_video_res(const ts::ivec2 &vsz)
{
    if (ts::tabs(sosz.x-vsz.x) > 64 || ts::tabs(sosz.y - vsz.y) > 64)
    {
        sosz = vsz;
        send_so();
    }
}

void av_contact_s::send_so()
{
    c->subiterate([this](contact_c *cc) {
        if (cc->is_av())
        {
            if (active_protocol_c *ap = prf().ap(cc->getkey().protoid))
                ap->set_stream_options(cc->getkey().contactid, cur_so(), sosz);
        }
    });
}

vsb_c *av_contact_s::createcam()
{
    if (currentvsb.id.is_empty())
        return vsb_c::build();
    return vsb_c::build(currentvsb);
}

void av_contact_s::camera_tick()
{
    if (!vsb)
    {
        vsb.reset( createcam() );
        vsb->set_bmp_ready_handler( DELEGATE(this, on_frame_ready) );
    }

    ts::ivec2 dsz = vsb->get_video_size();
    if (dirty_cam_size || dsz != prev_video_size)
    {
        prev_video_size = dsz;
        if (remote_sosz.x == 0 || (remote_sosz >>= dsz))
        {
            vsb->set_desired_size(dsz);
        }
        else
        {
            dsz = vsb->fit_to_size(remote_sosz);
        }
        dirty_cam_size = false;
    }

    if (vsb->updated())
    {
        gmsg<ISOGM_CAMERA_TICK>(c).send();

        ap4video = nullptr;
        videocid = 0;
        c->subiterate([&](contact_c *cc) {
            if (nullptr == ap4video && cc->is_av())
            {
                videocid = cc->getkey().contactid;
                ap4video = prf().ap(cc->getkey().protoid);
            }
        });
    }

}

void av_contact_s::on_frame_ready( const ts::bmpcore_exbody_s &ebm )
{
    if (0 == (remote_so & SO_RECEIVING_VIDEO))
        return;

    if (ap4video)
        ap4video->send_video_frame(videocid, ebm);
}
