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
    return TTT("[appname]: Первый запуск",8);
}

ts::wstr_c dialog_firstrun_c::path_by_choice(path_choice_e choice)
{
    switch (choice)
    {
    case PCH_PROGRAMFILES: return ts::get_full_path(ts::wstr_c(CONSTWSTR("%PROGRAMS%\\Isotoxin\\")), false, true);
    case PCH_HERE: return ts::get_full_path(ts::wstr_c(CONSTWSTR("")));
    case PCH_APPDATA: return ts::get_full_path(ts::wstr_c(CONSTWSTR("%APPDATA%\\isotoxin\\")), false, true);
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

bool dialog_firstrun_c::handler_1( RID, GUIPARAM p )
{
    choice1 = (path_choice_e)(int)p;
    return true;
}

bool dialog_firstrun_c::handler_0( RID, GUIPARAM p )
{
    path_choice_e prevchoice = choice0;
    choice0 = (path_choice_e)(int)p;

    if (ASSERT(selpath))
    {
        gui_textfield_c &tf = HOLD(selpath).as<gui_textfield_c>();

        if (choice0 == 2)
        {
            if (copyto.is_empty())
            {
                if (tf.get_text().is_empty())
                    tf.set_text(path_by_choice(prevchoice));
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

    return true;
}

bool dialog_firstrun_c::path_check_0( const ts::wstr_c & t )
{
    copyto = t;
    return true;
}
bool dialog_firstrun_c::path_check_1( const ts::wstr_c & t )
{
    if (!check_profile_name(t)) return false;
    profilename = t;
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
    DELAY_CALL_R( 0, DELEGATE(this, refresh_current_page), nullptr );
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
    DELAY_CALL_R( 0, DELEGATE(this, refresh_current_page), nullptr );
}

void dialog_firstrun_c::go2page(int page_)
{
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
            .append(TTT("[appname] впервые запущен на этом компьютере. При первом запуске необходимо выбрать язык. Также можно настроить некоторые параметры или оставить всё по умолчанию. Что вы выберете?",24))
            .append(L"<color=#808080><hr=5,1,1>"));

        vspace(25);

        label( ts::wstr_c(CONSTWSTR("<l>")).append(TTT("Язык",114)).append(CONSTWSTR("</l>")) );
        combik( list_langs(deflng, DELEGATE(this, select_lang)) );

        vspace(5);
        {
            radio_item_s items[] =
            {
                radio_item_s(TTT("Всё по умолчанию",25), (GUIPARAM)true, CONSTASTR("radio00")),
                radio_item_s(TTT("Буду настраивать",26), (GUIPARAM)false)
            };

            label( ts::wstr_c(CONSTWSTR("<l>")).append(TTT("Выбор",115)).append(CONSTWSTR("</l>")) );
            radio(ARRAY_WRAPPER(items), DELEGATE(this, noob_or_father), (GUIPARAM)(i_am_noob && !developing));

            if (developing) ctlenable( CONSTASTR("radio00"), false );
        }

        break;
    case 1:

        // SELECT INSTALL PATH

        label(ts::wstr_c(L"<p=c>")
            .append(TTT("[appname] может скопировать себя в рабочую папку или продолжить работу в текущей папке", 11))
            .append(L"<color=#808080><hr=5,1,1>"));

        vspace(25);

        {
            radio_item_s items[] =
            {
                radio_item_s(TTT("Скопировать в[br][l][quote]$[quote][/l]", 16) / path_by_choice(PCH_PROGRAMFILES), (GUIPARAM)(int)PCH_PROGRAMFILES, CONSTASTR("radio01")),
                radio_item_s(TTT("[appname] уже там где нужно[br]([l]оставить тут: [quote]$[quote][/l])", 15) / path_by_choice(PCH_HERE), (GUIPARAM)(int)PCH_HERE),
                radio_item_s(TTT("Выбрать другую папку...", 17), (GUIPARAM)(int)PCH_CUSTOM, CONSTASTR("radio02"))
            };

            if (developing)
                choice0 = PCH_HERE;

            radio(ARRAY_WRAPPER(items), DELEGATE(this, handler_0), (GUIPARAM)(int)choice0);

            if (developing)
            {
                ctlenable( CONSTASTR("radio01"), false );
                ctlenable( CONSTASTR("radio02"), false );
            }

            selpath = textfield(CONSTWSTR(""), MAX_PATH, TFR_PATH_SELECTOR, DELEGATE(this, path_check_0));
            handler_0(RID(), (GUIPARAM)(int)choice0);
        }
        break;
    case 2:

        // SELECT SETTINGS PATH

        label(ts::wstr_c(L"<p=c>")
            .append(TTT("Вы можете выбрать место хранения настроек и профилей. Обычно выбор по умолчанию является оптимальным.", 14))
            .append(L"<color=#808080><hr=5,1,1>"));
        vspace(25);
        {
            radio_item_s items[] =
            {
                radio_item_s(TTT("По умолчанию[br][l][quote]$[quote][/l]", 20) / path_by_choice(PCH_APPDATA), (GUIPARAM)(int)PCH_APPDATA, CONSTASTR("radio03")),
                radio_item_s(TTT("Там же где [appname][br][l][quote]$[quote][/l]", 19) / path_by_choice(PCH_INSTALLPATH), (GUIPARAM)(int)PCH_INSTALLPATH),
            };

            if (developing)
                choice1 = PCH_INSTALLPATH;

            radio(ARRAY_WRAPPER(items), DELEGATE(this, handler_1), (GUIPARAM)(int)choice1);

            if (developing)
                ctlenable( CONSTASTR("radio03"), false );
        }
        break;
    case 3:

        // SELECT PROFILE NAME
        label(ts::wstr_c(L"<p=c>")
            .append(TTT("Последний шаг - выбор имени файла профиля. Вы можете оставить это поле пустым и создать профиль позднее", 18))
            .append(L"<color=#808080><hr=5,1,1>"));
        vspace(25);

        profile = textfield(profilename, ts::tmax(0, MAX_PATH - copyto.get_length() - 1), TFR_TEXT_FILED, DELEGATE(this, path_check_1));
    }

    children_repos();

}

/*virtual*/ void dialog_firstrun_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    
    profilename = CONSTWSTR("%USER%");
    parse_env(profilename);
    if (profilename.find_pos('%') >= 0) profilename = CONSTWSTR("profile");

    go2page(0);
}

void dialog_firstrun_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
    if (bcr.tag == 0)
    {
        bcr.tooltip = TOOLTIP( TTT("Ничего не делать. Выйти.",12) );
        bcr.btext = TTT("Выход",22);
    }
    if (bcr.tag == 1)
    {
        if (page == 0)
        {
            if (i_am_noob && !developing)
            {
                bcr.tooltip = TOOLTIP( TTT("Настроить всё по умолчанию и продолжить работу",28) );
            make_pusk:
                bcr.btext = TTT("Пуск",27);
                bcr.handler = DELEGATE(this, start);
            } else
            {
                make_dalee:
                bcr.tooltip = TOOLTIP( TTT("Перейти к следующей странице",13) );
                bcr.btext = TTT("Далее", 23);
                bcr.handler = DELEGATE(this, next_page);
            }
        } else
        {
            if ( page == 3 )
            {
                bcr.tooltip = TOOLTIP( TTT("Начать работу",116) );
                goto make_pusk;
            }
            goto make_dalee;
        }
    }
    if (bcr.tag == 2)
    {
        if (page > 0)
        {
            bcr.face = CONSTASTR("button");
            bcr.tooltip = TOOLTIP( TTT("Перейти к предыдущей странице",29) );
            bcr.btext = TTT("Назад",30);
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
    WINDOWS_ONLY curd.case_down().replace_all('/', '\\').trim_right('\\');
    WINDOWS_ONLY copyto.case_down().replace_all('/', '\\').trim_right('\\');
    if (curd != copyto)
    {

        // TODO : copy
    }
    ts::wstr_c config_fn = ts::fn_join(copyto, CONSTWSTR("config.db"));

    ts::sqlitedb_c * db = ts::sqlitedb_c::connect( config_fn );
    if (ASSERT(db))
        config_c::prepare_conf_table(db);
    cfg().load( true );

    if (!profilename.is_empty())
    {
        ts::wstr_c n = profilename;
        cfg().profile(n);
        ts::sqlitedb_c::connect(ts::fn_change_name_ext<ts::wchar>(config_fn, n.append(CONSTWSTR(".profile"))));
    }

    cfg().language( deflng );

    if (exit) 
    {
        on_close();
    } else 
    {
        on_confirm();
        g_app->summon_main_rect();
        g_app->set_notification_icon();
    }

    return true;
}
