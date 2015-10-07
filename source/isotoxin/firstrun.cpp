#include "isotoxin.h"

dialog_firstrun_c::dialog_firstrun_c(initial_rect_data_s &data) :gui_isodialog_c(data)
{
    deflng = detect_language();
    g_app->load_locale(deflng);
}

dialog_firstrun_c::~dialog_firstrun_c()
{
    gui->delete_event(DELEGATE(this, refresh_current_page));
}

/*virtual*/ ts::wstr_c dialog_firstrun_c::get_name() const
{
    return TTT("[appname]: First run",8);
}

void dialog_firstrun_c::set_defaults()
{
    copyto = path_by_choice(PCH_PROGRAMFILES);
    profilename = CONSTWSTR("%USER%");
    parse_env(profilename);
    if (profilename.find_pos('%') >= 0) profilename = CONSTWSTR("profile");
    choice0 = PCH_PROGRAMFILES;
    choice1 = PCH_APPDATA;
    is_autostart = true;
}


ts::wstr_c dialog_firstrun_c::gen_info() const
{
    auto summary = [this]()->ts::wstr_c
    {
        ts::TSCOLOR pathcolor = ts::ARGB(0,0,100);

        ts::wstr_c profnameinfo;;
        if (!profilename.is_empty())
            profnameinfo = enquote(colorize<ts::wchar>(profilename, pathcolor));
        else
            profnameinfo = TTT("do not create now",320);

        ts::wstr_c t(CONSTWSTR("<l>"));
        t.append( TTT("Install to: $",313) / enquote(colorize<ts::wchar>(path_by_choice(PCH_INSTALLPATH), pathcolor)) ).append(CONSTWSTR("<br>"));
        t.append(TTT("Autostart: $", 314) / (colorize<ts::wchar>(loc_text(is_autostart ? loc_yes : loc_no), pathcolor))).append(CONSTWSTR("<br>"));
        t.append( TTT("Config path: $",317) / enquote(colorize<ts::wchar>(path_by_choice(choice1), pathcolor)) ).append(CONSTWSTR("<br>"));
        t.append( TTT("Profile name: $",318) / profnameinfo ).append(CONSTWSTR("<br>"));

        return t;
    };

    auto oops = [this](const ts::wstr_c &ot)->ts::wstr_c
    {
        if (ts::check_write_access(path_by_choice(PCH_INSTALLPATH))) return ot;
        ts::wstr_c t;
        if (!ot.is_empty()) t.append(ot).append(CONSTWSTR("<br>"));
        t.append(CONSTWSTR("<b><p=c>"));
        t.append(maketag_color<ts::wchar>(ts::ARGB(10,10,0)));
        t.append( TTT("Install path protected; permissions elevation will be performed",319) );

        return t;
    };

    switch (page)
    {
    case 0:
        if (!i_am_noob)
            break;
        // no break here
    case 3:
        return oops( summary() );
    }

    return ts::wstr_c();
}

void dialog_firstrun_c::updinfo()
{
    if (infolabel)
    {
        gui_label_c &ctl = HOLD(infolabel).as<gui_label_c>();
        ctl.set_text(gen_info());
    }

}

ts::wstr_c dialog_firstrun_c::path_by_choice(path_choice_e choice) const
{
    switch (choice)
    {
    case PCH_PROGRAMFILES: return ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%PROGRAMS%\\Isotoxin\\")), FNO_FULLPATH | FNO_PARSENENV);
    case PCH_HERE: return ts::fn_fix_path(ts::wstr_c(CONSTWSTR("")), FNO_FULLPATH);
    case PCH_APPDATA: return ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%APPDATA%\\Isotoxin\\")), FNO_FULLPATH | FNO_PARSENENV);
    case PCH_CUSTOM:
        {
            if (ASSERT(selpath))
                return HOLD(selpath).as<gui_textfield_c>().get_text();
            return ts::wstr_c();
        }
    case PCH_INSTALLPATH: return copyto.is_empty() ? path_by_choice(choice0) : copyto;
    }

    return ts::wstr_c();
}

bool dialog_firstrun_c::handler_2( RID, GUIPARAM p)
{
    is_autostart = p != nullptr;
    updinfo();
    return true;
}

bool dialog_firstrun_c::handler_1( RID, GUIPARAM p )
{
    choice1 = (path_choice_e)as_int(p);
    updinfo();
    return true;
}

bool dialog_firstrun_c::handler_0( RID, GUIPARAM p )
{
    path_choice_e prevchoice = choice0;
    choice0 = (path_choice_e)as_int(p);

    if (ASSERT(selpath))
    {
        gui_textfield_c &tf = HOLD(selpath).as<gui_textfield_c>();

        if (choice0 == 2)
        {
            copyto = pathselected;
            if (copyto.is_empty())
            {
                if (tf.get_text().is_empty())
                {
                    tf.set_text(path_by_choice(prevchoice));
                }
            } else
            {
                tf.set_text(copyto);
            }
            MODIFY(tf).visible(true);
        } else
        {
            MODIFY(tf).visible(false);
        }
    }

    copyto = path_by_choice(choice0);
    updinfo();
    return true;
}

bool dialog_firstrun_c::path_check_0( const ts::wstr_c & t )
{
    copyto = t;
    pathselected = t;
    updinfo();
    return true;
}
bool dialog_firstrun_c::path_check_1( const ts::wstr_c & t )
{
    if (!check_profile_name(t)) return false;
    profilename = t;
    updinfo();
    return true;
}

bool dialog_firstrun_c::refresh_current_page( RID, GUIPARAM )
{
    int ppage = page;
    page = -1;
    go2page(ppage);
    return true;
}

bool dialog_firstrun_c::noob_or_father( RID, GUIPARAM par )
{
    i_am_noob = par != nullptr;
    //update_buttons();
    DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, refresh_current_page), nullptr );

    if (i_am_noob)
        set_defaults();

    return true;
}

bool dialog_firstrun_c::prev_page(RID, GUIPARAM)
{
    ASSERT( page > 0 );
    go2page(page-1);
    return true;
}
bool dialog_firstrun_c::next_page(RID, GUIPARAM)
{
    go2page(page+1);
    return true;
}

void dialog_firstrun_c::select_lang(const ts::str_c& prm)
{
    deflng = prm;
    g_app->load_locale(deflng);
    //set_combik_menu(CONSTASTR("langs"), list_langs(deflng, DELEGATE(this, select_lang)));
    DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, refresh_current_page), nullptr );
}

void dialog_firstrun_c::go2page(int page_)
{
    infolabel = RID();

    if (page != page_)
    {
        page = page_;
        reset();
    }
    switch (page_)
    {
    case 0:
        
        // CHOOSE YOUR STYLE

        label(ts::wstr_c(L"<p=c>")
            .append(TTT("This is first run of [appname]. You have to select language. Also You can choose default initialization or you can manually set up some settings. Please select.",24))
            .append(L"<color=#808080><hr=5,1,1>"));

        vspace(25);

        label( ts::wstr_c(CONSTWSTR("<l>")).append(TTT("Language",114)).append(CONSTWSTR("</l>")) );
        combik( list_langs(deflng, DELEGATE(this, select_lang)) );

        vspace(5);
        {
            radio_item_s items[] =
            {
                radio_item_s(TTT("Default initialization",25), as_param(1), CONSTASTR("radio00")),
                radio_item_s(TTT("Manual setup",26), as_param(0))
            };

            label( ts::wstr_c(CONSTWSTR("<l>")).append(TTT("Choice",115)).append(CONSTWSTR("</l>")) );
            radio(ARRAY_WRAPPER(items), DELEGATE(this, noob_or_father), as_param((i_am_noob && !developing) ? 1 : 0));

            if (developing) ctlenable( CONSTASTR("radio00"), false );
        }

        vspace(20);
        infolabel = label( gen_info() );

        break;
    case 1:

        // SELECT INSTALL PATH

        label(ts::wstr_c(L"<p=c>")
            .append(TTT("[appname] can copy self to work folder or can continue work in current folder",11))
            .append(L"<color=#808080><hr=5,1,1>"));

        vspace(25);

        {
            radio_item_s items[] =
            {
                radio_item_s(TTT("Copy to[br][l]$[/l]",16) / enquote(path_by_choice(PCH_PROGRAMFILES)), as_param(PCH_PROGRAMFILES), CONSTASTR("radio01")),
                radio_item_s(TTT("[appname] already in right place[br]([l]leave it here: $[/l])",15) / enquote(path_by_choice(PCH_HERE)), as_param(PCH_HERE)),
                radio_item_s(TTT("Select another folder...",17), as_param(PCH_CUSTOM), CONSTASTR("radio02"))
            };

            if (developing)
                choice0 = PCH_HERE;

            radio(ARRAY_WRAPPER(items), DELEGATE(this, handler_0), as_param(choice0));

            if (developing)
            {
                ctlenable( CONSTASTR("radio01"), false );
                ctlenable( CONSTASTR("radio02"), false );
            }

            selpath = textfield(CONSTWSTR(""), MAX_PATH, TFR_PATH_SELECTOR, DELEGATE(this, path_check_0));
            handler_0(RID(), as_param(choice0));
        }

        vspace(10);
        {
            check_item_s items[] =
            {
                check_item_s(loc_text(loc_autostart), 1),
            };
            check(ARRAY_WRAPPER(items), DELEGATE(this, handler_2), is_autostart ? 1 : 0);
        }

        vspace(20);
        infolabel = label( gen_info() );

        break;
    case 2:

        // SELECT SETTINGS PATH

        label(ts::wstr_c(L"<p=c>")
            .append(TTT("You can select place for settings and profiles. Default choice recommended.",14))
            .append(L"<color=#808080><hr=5,1,1>"));
        vspace(25);
        {
            ts::wstr_c samepath = path_by_choice(PCH_INSTALLPATH);
            radio_item_s items[] =
            {
                radio_item_s(TTT("By default[br][l]$[/l]",20) / enquote(path_by_choice(PCH_APPDATA)), as_param(PCH_APPDATA), CONSTASTR("radio03")),
                radio_item_s(TTT("Same folder of [appname][br][l]$[/l]",19) / enquote(samepath), as_param(PCH_INSTALLPATH), CONSTASTR("radio04")),
            };

            bool oops = !ts::check_write_access(samepath);

            if (developing)
                choice1 = PCH_INSTALLPATH;
            else if (oops)
                choice1 = PCH_APPDATA;

            radio(ARRAY_WRAPPER(items), DELEGATE(this, handler_1), as_param(choice1));

            if (developing)
                ctlenable( CONSTASTR("radio03"), false );
            else if (oops)
                ctlenable( CONSTASTR("radio04"), false );
        }

        vspace(20);
        infolabel = label(gen_info());

        break;
    case 3:

        // SELECT PROFILE NAME
        label(ts::wstr_c(L"<p=c>")
            .append(TTT("Last step - define profile name. You can leave this field empty and create profile later",18))
            .append(L"<color=#808080><hr=5,1,1>"));
        vspace(25);

        profile = textfield(profilename, ts::tmax(0, MAX_PATH - copyto.get_length() - 1), TFR_TEXT_FILED, DELEGATE(this, path_check_1));

        vspace(20);
        infolabel = label(gen_info());

    }

    gui->repos_children(this);

}

/*virtual*/ void dialog_firstrun_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();

    set_defaults();

    go2page(0);
}

/*virtual*/ void dialog_firstrun_c::on_close()
{
    sys_exit(0);
}

void dialog_firstrun_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
    if (bcr.tag == 0)
    {
        bcr.tooltip = TOOLTIP( TTT("Do nothing. Exit.",12) );
        bcr.btext = TTT("Quit",22);
    }
    if (bcr.tag == 1)
    {
        if (page == 0)
        {
            if (i_am_noob && !developing)
            {
                bcr.tooltip = TOOLTIP( TTT("Initialize by default and run application",28) );
            make_pusk:

                start_button_text = TTT("Start",27);
                if (!ts::check_write_access(path_by_choice( PCH_INSTALLPATH )))
                    start_button_text.insert( 0, CONSTWSTR("<img=uac>") );

                bcr.btext = start_button_text;
                bcr.handler = DELEGATE(this, start);
            } else
            {
                make_dalee:
                bcr.tooltip = TOOLTIP( TTT("Next page",13) );
                bcr.btext = TTT("Next",23);
                bcr.handler = DELEGATE(this, next_page);
            }
        } else
        {
            if ( page == 3 )
            {
                bcr.tooltip = TOOLTIP( TTT("Run!",116) );
                goto make_pusk;
            }
            goto make_dalee;
        }
    }
    if (bcr.tag == 2)
    {
        if (page > 0)
        {
            bcr.face = BUTTON_FACE(button);
            bcr.tooltip = TOOLTIP( TTT("Previous page",29) );
            bcr.btext = TTT("Back",30);
            bcr.handler = DELEGATE(this, prev_page);
        }
    }
}

/*virtual*/ bool dialog_firstrun_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

bool dialog_firstrun_c::start( RID, GUIPARAM )
{
    bool exit = false;
    ts::wstr_c curd = path_by_choice( PCH_HERE );
    copyto = path_by_choice( PCH_INSTALLPATH );
    fix_path(curd, FNO_SIMPLIFY | FNO_TRIMLASTSLASH);
    fix_path(copyto, FNO_SIMPLIFY_NOLOWERCASE | FNO_TRIMLASTSLASH);

    bool run_another_place = false;
    ts::wstr_c exepath = ts::get_exe_full_name();
    if (curd != fn_fix_path(copyto, FNO_SIMPLIFY | FNO_TRIMLASTSLASH))
    {
        install_to( copyto, true );
        exepath = ts::fn_join(copyto, ts::fn_get_name_with_ext(ts::get_exe_full_name()));
        if ( !ts::is_file_exists(exepath) )
        {
            SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                DT_MSGBOX_ERROR,
                TTT("Sorry, copy to $ failed.",312) / enquote(copyto)
                ));
            return true;
        }
        exit = true;
        run_another_place = true;
    }

    make_path( path_by_choice(choice1), 0 );
    ts::wstr_c config_fn = ts::fn_join(path_by_choice(choice1), CONSTWSTR("config.db"));

    if (ts::sqlitedb_c * db = ts::sqlitedb_c::connect( config_fn, g_app->F_READONLY_MODE ))
    {
        config_c::prepare_conf_table(db);
        db->close();
    }

    cfg().load( config_fn );

    if (!profilename.is_empty())
    {
        ts::wstr_c n = profilename;
        cfg().profile(n);
        ts::sqlitedb_c::connect(ts::fn_change_name_ext(config_fn, n.append(CONSTWSTR(".profile"))), g_app->F_READONLY_MODE);
    }

    cfg().language( deflng );
    if (is_autostart)
        autostart(exepath, CONSTWSTR("minimize"));

    if (exit) 
    {
        cfg().close();

        if (run_another_place)
        {
            ts::wstr_c another_exe = ts::fn_join(copyto, ts::fn_get_name_with_ext(ts::get_exe_full_name()));
            another_exe.append(CONSTWSTR(" wait ")).append_as_uint(GetCurrentProcessId());
            ts::start_app(another_exe, nullptr);
        }

        on_close();
    } else 
    {
        on_confirm();
        g_app->load_profile_and_summon_main_rect(false);
        g_app->set_notification_icon();
    }

    return true;
}
