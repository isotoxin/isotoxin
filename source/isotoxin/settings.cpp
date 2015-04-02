#include "isotoxin.h"

#define HGROUP_MEMBER ts::wsptr()

dialog_settings_c::dialog_settings_c(initial_rect_data_s &data) :gui_isodialog_c(data), ipcj( (ts::streamstr<ts::str_c>() << "isotoxin_settings_" << GetCurrentThreadId()).buffer(), prf().is_loaded() ? DELEGATE( this, ipchandler ) : nullptr )
{
    s3::enum_sound_capture_devices(enum_capture_devices, this);
    s3::enum_sound_play_devices(enum_play_devices, this);
    media.init();
    s3::get_capture_device(&mic_device_stored);
    micdevice = string_from_device(mic_device_stored);
    start_capture();
    profile_selected = prf().is_loaded();

    //ts::tbuf0_t<theme_info_s> m_themes;

    ts::wstrings_c fns;
    ts::g_fileop->find(fns, CONSTWSTR("themes/*/struct.decl"), true);

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
        if (!n.is_empty())
        {
            ts::abp_c bp;
            if (!ts::g_fileop->load(f, bp)) continue;
            const ts::abp_c *conf = bp.get(CONSTASTR("conf"));

            theme_info_s &thi = m_themes.add();
            thi.folder = n;
            thi.current = cfg().theme().equals(n);
            thi.name.set_as_char('@').append(n);
            if (conf)
                thi.name = conf->get_string(CONSTASTR("name"), ts::to_str(thi.name));
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

    gmsg<GM_CLOSE_DIALOG>( UD_NETWORKNAME ).send();
}

/*virtual*/ s3::Format *dialog_settings_c::formats(int &count)
{
    capturefmt.sampleRate = 48000;
    capturefmt.channels = 1;
    capturefmt.bitsPerSample = 16;
    count = 1;
    return &capturefmt;
};

/*virtual*/ ts::wstr_c dialog_settings_c::get_name() const
{
    return TTT("[appname]: Настройки",31);
}

/*virtual*/ void dialog_settings_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();

    ipcj.send( ipcw ( AQ_GET_PROTOCOLS_LIST ) );
}

void dialog_settings_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

bool dialog_settings_c::username_edit_handler( const ts::wstr_c &v )
{
    username = v;
    return true;
}

bool dialog_settings_c::statusmsg_edit_handler( const ts::wstr_c &v )
{
    userstatusmsg = v;
    return true;
}

bool dialog_settings_c::downloadfolder_edit_handler(const ts::wstr_c &v)
{
    downloadfolder = v;
    return true;
}

bool dialog_settings_c::date_msg_tmpl_edit_handler(const ts::wstr_c &v)
{
    date_msg_tmpl = v;
    return true;
}
bool dialog_settings_c::date_sep_tmpl_edit_handler(const ts::wstr_c &v)
{
    date_sep_tmpl = v;
    return true;
}


bool dialog_settings_c::ctl2send_handler( RID, GUIPARAM p )
{
    ctl2send = (int)p;
    return true;
}

bool dialog_settings_c::collapse_beh_handler(RID, GUIPARAM p)
{
    collapse_beh = (int)p;
    return true;
}

bool dialog_settings_c::autoupdate_handler( RID, GUIPARAM p)
{
    autoupdate = (int)p;
    if (RID r = find(CONSTASTR("proxytype")))
        r.call_enable(autoupdate > 0);
    if (RID r = find(CONSTASTR("proxyaddr")))
        r.call_enable(autoupdate > 0 && autoupdate_proxy > 0);
    return true;
}
void dialog_settings_c::autoupdate_proxy_handler(const ts::str_c& p)
{
    autoupdate_proxy = p.as_int();
    set_combik_menu(CONSTASTR("proxytype"), list_proxy_types(autoupdate_proxy, DELEGATE(this, autoupdate_proxy_handler)));
    if (RID r = find(CONSTASTR("proxyaddr")))
        r.call_enable(autoupdate > 0 && autoupdate_proxy > 0);
}

bool dialog_settings_c::autoupdate_proxy_addr_handler( const ts::wstr_c & t )
{
    autoupdate_proxy_addr = t;
    if (RID r = find(CONSTASTR("proxyaddr")))
        check_proxy_addr(r, autoupdate_proxy_addr);
    return true;
}

void dialog_settings_c::check_proxy_addr(RID editctl, ts::str_c &proxyaddr)
{
    gui_textfield_c &tf = HOLD(editctl).as<gui_textfield_c>();
    if (tf.is_disabled()) return;
    tf.badvalue(!check_netaddr(proxyaddr));
}

bool dialog_settings_c::check_update_now(RID, GUIPARAM)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(false);

    checking_new_version = true;
    g_app->b_update_ver(RID(), (GUIPARAM)AUB_ONLY_CHECK);
    return true;
}

ts::uint32 dialog_settings_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(true);

    if (!checking_new_version) return 0;
    checking_new_version = false;
    if (nv.ver.is_empty())
    {
        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
            gui_isodialog_c::title(DT_MSGBOX_INFO),
            TTT("Новых версий не обнаружено.",194)
            ));
    
        return 0;
    }

    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
        gui_isodialog_c::title(DT_MSGBOX_INFO),
        TTT("Обнаружена новая версия: $",196) / ts::to_wstr(nv.ver.as_sptr())
        ));

    return 0;
}

menu_c dialog_settings_c::list_proxy_types(int cur, MENUHANDLER mh, int av)
{
    menu_c m;
    m.add(TTT("Прямое соединение", 159), cur == 0 ? MIF_MARKED : 0, mh, CONSTASTR("0"));

    if (PROXY_SUPPORT_HTTP & av)
        m.add(TTT("HTTP прокси", 160), cur == 1 ? MIF_MARKED : 0, mh, CONSTASTR("1"));
    if (PROXY_SUPPORT_SOCKS4 & av)
        m.add(TTT("Socks 4 прокси", 161), cur == 2 ? MIF_MARKED : 0, mh, CONSTASTR("2"));
    if (PROXY_SUPPORT_SOCKS5 & av)
        m.add(TTT("Socks 5 прокси", 162), cur == 3 ? MIF_MARKED : 0, mh, CONSTASTR("3"));

    return m;
}

bool dialog_settings_c::msgopts_handler( RID, GUIPARAM p )
{
    ts::flags32_s::BITS newo = (ts::flags32_s::BITS)p;
    msgopts_changed |= newo ^ msgopts_current;
    msgopts_current = newo;

    if (RID r = find(CONSTASTR("date_msg_tmpl")))
        r.call_enable(0 != (msgopts_current & MSGOP_SHOW_DATE));

    if (RID r = find(CONSTASTR("date_sep_tmpl")))
        r.call_enable(0 != (msgopts_current & MSGOP_SHOW_DATE_SEPARATOR));

    return true;
}

void dialog_settings_c::select_theme(const ts::str_c& prm)
{
    ts::wstr_c selfo; selfo.set_as_utf8(prm);
    for( theme_info_s &ti : m_themes )
        ti.current = ti.folder.equals(selfo);
    set_combik_menu(CONSTASTR("themes"), list_themes());
}

menu_c dialog_settings_c::list_themes()
{
    menu_c m;
    for( const theme_info_s &ti : m_themes )
        m.add( ti.name, ti.current ? MIF_MARKED : 0, DELEGATE(this, select_theme), to_utf8(ti.folder) );
    return m;
}


/*virtual*/ int dialog_settings_c::additions( ts::irect & border )
{
    if(profile_selected)
    {
        username = prf().username(); text_prepare_for_edit(username);
        userstatusmsg = prf().userstatus(); text_prepare_for_edit(userstatusmsg);
        ctl2send = prf().ctl_to_send();
        msgopts_current = prf().get_msg_options().__bits;
        msgopts_changed = 0;
        date_msg_tmpl = prf().date_msg_template();
        date_sep_tmpl = prf().date_sep_template();
        downloadfolder = prf().download_folder();

        table_active_protocol = &prf().get_table_active_protocol();
        table_active_protocol_underedit = *table_active_protocol;

        for (auto &row : table_active_protocol_underedit)
            if (active_protocol_c *ap = prf().ap(row.id))
            {
                row.other.proxy = ap->get_proxy_settings();
                if (row.other.proxy.proxy_addr.is_empty()) row.other.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);
            }

    }
    curlang = cfg().language();
    autoupdate = cfg().autoupdate();
    oautoupdate = autoupdate;
    autoupdate_proxy = cfg().autoupdate_proxy();
    autoupdate_proxy_addr = cfg().autoupdate_proxy_addr();
    collapse_beh = cfg().collapse_beh();
    talkdevice = cfg().device_talk();
    signaldevice = cfg().device_signal();

    int textrectid = 0;

    menu_c m;
    if (profile_selected)
    {
        m.add_sub( TTT("Профиль",1) )
            .add(TTT("Основные",32), 0, TABSELMI(MASK_PROFILE_COMMON) )
            .add(TTT("Чат",109), 0, TABSELMI(MASK_PROFILE_CHAT) )
            .add(TTT("Сети",33), 0, TABSELMI(MASK_PROFILE_NETWORKS) );
    }

    m.add_sub(TTT("Приложение", 34))
        .add(TTT("Основные",106), 0, TABSELMI(MASK_APPLICATION_COMMON))
        .add(TTT("Система", 35), 0, TABSELMI(MASK_APPLICATION_SYSTEM))
        .add(TTT("Звук",125), 0, TABSELMI(MASK_APPLICATION_SETSOUND));

    descmaker dm( descs );
    dm << MASK_APPLICATION_COMMON; //_________________________________________________________________________________________________//

    dm().page_header(TTT("Основные настройки приложения",108));
    dm().vspace(10);
    dm().combik(TTT("Язык",107)).setmenu( list_langs( curlang, DELEGATE(this, select_lang) ) ).setname( CONSTASTR("langs") );
    dm().vspace(10);
    dm().combik(TTT("Тема интерфейса",233)).setmenu(list_themes()).setname(CONSTASTR("themes"));
    dm().vspace();
    dm().radio(TTT("Обновления", 155), DELEGATE(this, autoupdate_handler), autoupdate).setmenu(
        menu_c()
        .add(TTT("Не проверять", 156), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("Только уведомлять", 157), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Скачивать и обновлять", 158), 0, MENUHANDLER(), CONSTASTR("2"))
        );
    dm().hgroup(ts::wsptr());
    dm().combik(HGROUP_MEMBER).setmenu( list_proxy_types(autoupdate_proxy, DELEGATE(this, autoupdate_proxy_handler)) ).setname(CONSTASTR("proxytype"));
    dm().textfield(HGROUP_MEMBER, ts::to_wstr(autoupdate_proxy_addr), DELEGATE(this, autoupdate_proxy_addr_handler)).setname(CONSTASTR("proxyaddr"));
    dm().vspace();
    dm().button(HGROUP_MEMBER, TTT("Проверить наличие обновления",195), DELEGATE(this, check_update_now) ).height(35).setname(CONSTASTR("checkupdb"));

    dm << MASK_APPLICATION_SYSTEM; //_________________________________________________________________________________________________//

    dm().page_header( TTT("Некоторые системные настройки приложения",36) );
    dm().vspace(10);
    //dm().combik(TTT("Текущий профиль")).setmenu( list_profiles() );
    
    ts::wstr_c profname = cfg().profile();
    if (!profname.is_empty()) profile_c::path_by_name(profname);

    dm().path( TTT("Путь к текущему профилю",37), ts::to_wstr(profname) ).readonly(true);
    dm().vspace();
    dm().radio(TTT("Сворачивание в область уведомлений",118), DELEGATE(this, collapse_beh_handler), collapse_beh).setmenu(
        menu_c()
        .add(TTT("Не сворачивать в область уведомлений",119), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("Кнопка минимизации $ сворачивает в область уведомлений",120) / CONSTWSTR("<img=bmin,-1>"), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Кнопка закрытия $ сворачивает в область уведомлений",121) / CONSTWSTR("<img=bclose,-1>"), 0, MENUHANDLER(), CONSTASTR("2"))
        );

    dm << MASK_APPLICATION_SETSOUND; //______________________________________________________________________________________________//
    dm().page_header(TTT("Настройка звука",127));
    dm().vspace(10);
    dm().combik(TTT("Микрофон",126)).setmenu( list_capture_devices() ).setname( CONSTASTR("mic") );
    dm().vspace();
    dm().hgroup(TTT("Динамики",128));
    dm().combik(HGROUP_MEMBER).setmenu( list_talk_devices() ).setname( CONSTASTR("talk") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=play"), DELEGATE(this, test_talk_device) ).sethint(TTT("Проверка динамиков",131));
    dm().vspace();
    dm().hgroup(TTT("Звонок",129));
    dm().combik(HGROUP_MEMBER).setmenu( list_signal_devices() ).setname( CONSTASTR("signal") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=play"), DELEGATE(this, test_signal_device) ).sethint(TTT("Проверка сигнала",132));
    dm().vspace();

    if (profile_selected)
    {
        dm << MASK_PROFILE_COMMON; //____________________________________________________________________________________________________//
        dm().page_header( TTT("Основные настройки профиля",38) );
        dm().vspace(10);
        dm().textfield( TTT("Имя",52), username, DELEGATE(this,username_edit_handler) ).setname(CONSTASTR("uname")).focus(true);
        dm().vspace();
        dm().textfield(TTT("Статус",68), userstatusmsg, DELEGATE(this, statusmsg_edit_handler)).setname(CONSTASTR("ustatus"));
        dm().vspace();
        dm().path(TTT("Папка для файлов",174), downloadfolder, DELEGATE(this, downloadfolder_edit_handler));

        dm << MASK_PROFILE_CHAT; //____________________________________________________________________________________________________//
        dm().page_header(TTT("Настройки чата",110));
        dm().vspace(10);
        dm().radio(TTT("Enter и Ctrl+Enter",111), DELEGATE(this, ctl2send_handler), ctl2send).setmenu(
            menu_c().add(TTT("Enter - отправить сообщение, Ctrl+Enter - перенос строки",112), 0, MENUHANDLER(), CONSTASTR("0"))
                    .add(TTT("Enter - перенос строки, Ctrl+Enter - отправить сообщение",113), 0, MENUHANDLER(), CONSTASTR("1"))
                    .add(TTT("Enter - перенос строки, двойной Enter - отправить сообщение",152), 0, MENUHANDLER(), CONSTASTR("2"))
            );
        dm().vspace();

        ts::wstr_c ctl;
        dm().textfield( ts::wstr_c(), date_msg_tmpl, DELEGATE(this,date_msg_tmpl_edit_handler) ).setname(CONSTASTR("date_msg_tmpl")).width(100).subctl(textrectid++, ctl);
        ts::wstr_c t_showdate = TTT("Показывать дату (шаблон: $)",171) / ctl;
        if (t_showdate.find_pos(ctl)<0) t_showdate.append_char(' ').append(ctl);

        ctl.clear();
        dm().textfield(ts::wstr_c(), date_sep_tmpl, DELEGATE(this, date_sep_tmpl_edit_handler)).setname(CONSTASTR("date_sep_tmpl")).width(140).subctl(textrectid++, ctl);
        ts::wstr_c t_showdatesep = TTT("Показывать разделитель $ между сообщениями с различной датой",172) / ctl;
        if (t_showdatesep.find_pos(ctl)<0) t_showdate.append_char(' ').append(ctl);

        dm().checkb(TTT("Сообщения",170), DELEGATE(this, msgopts_handler), msgopts_current).setmenu(
                    menu_c().add(t_showdate, 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_DATE ))
                            .add(t_showdatesep, 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_DATE_SEPARATOR ))
                            .add(TTT("Показывать название протокола",173), 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_PROTOCOL_NAME ))
                            .add(TTT("Сохранять историю сообщений",222), 0, MENUHANDLER(), ts::amake<int>( MSGOP_KEEP_HISTORY ))
                    );

        dm << MASK_PROFILE_NETWORKS; //_________________________________________________________________________________________________//
        dm().page_header(TTT("Настройки сетей",130));
        dm().vspace(10);
        dm().list(TTT("Доступные сети (правый клик - опции)",53), 5).setname(CONSTASTR("protolist"));
        dm().vspace();
        dm().list(TTT("Активные сети (правый клик - опции)",54), 5).setname(CONSTASTR("protoactlist"));
        dm().vspace();
    /*
        dm().checkb(L"Тестовый чекбоксег", GUIPARAMHANDLER(), 3).setmenu(
              menu_c().add(L"Медвед",0,MENUHANDLER(), CONSTASTR("1"))
                      .add(L"Превед",0,MENUHANDLER(), CONSTASTR("2"))
                      .add(L"Велосипед",0,MENUHANDLER(), CONSTASTR("4"))
            );
            */
    }

    gui_vtabsel_c &tab = MAKE_CHILD<gui_vtabsel_c>( getrid(), m );
    tab.leech( TSNEW(leech_dock_left_s, 150) );
    border = ts::irect(155,0,0,0);

    return 1;
}

ts::wstr_c dialog_settings_c::describe_network(const ts::wstr_c& name, const ts::str_c& tag) const
{
    const protocols_s *p = find_protocol(tag);
    if (p == nullptr)
    {
        return TTT("$ (Неизвестный протокол: $)", 57) / name / ts::to_wstr(tag);
    }
    else
        return ts::wstr_c(name).append(CONSTWSTR(" (")).append(p->description).append_char(')');
}

void dialog_settings_c::add_suspended_proto( RID lst, int id, const active_protocol_data_s &apdata )
{
    ts::wstr_c desc(TTT("Отключено: $",200) / describe_network(apdata.name, apdata.tag));
    MAKE_CHILD<gui_listitem_c>(lst, desc, ts::str_c(CONSTASTR("3/")).append(apdata.tag).append_char('/').append_as_int(id)) << DELEGATE(this, getcontextmenu);
}

/*virtual*/ void dialog_settings_c::tabselected(ts::uint32 mask)
{
    network_props = -1;

    if ( mask & MASK_PROFILE_NETWORKS )
    {
        if (RID lst = find(CONSTASTR("protolist")))
        {
            for( const protocols_s&proto : available_prots )
            {
                MAKE_CHILD<gui_listitem_c>(lst, proto.description, ts::str_c(CONSTASTR("1/")).append(proto.tag)) << DELEGATE( this, getcontextmenu );
            }

            for (auto &row : table_active_protocol_underedit)
            {
                if (0 == (row.other.options & active_protocol_data_s::O_SUSPENDED))
                    continue;

                add_suspended_proto( lst, row.id, row.other );
            }
        }

        if (RID lst = find(CONSTASTR("protoactlist")))
        {
            for (auto &row : table_active_protocol_underedit)
            {
                if (0 != (row.other.options & active_protocol_data_s::O_SUSPENDED))
                    continue;

                add_active_proto(lst, row.id, row.other);
            }
        }

        num_ctls_in_network_tab = getengine().children_count();

    }

    if (mask & MASK_PROFILE_CHAT)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this,msgopts_handler), (GUIPARAM)msgopts_current);
    }

    if (mask & MASK_APPLICATION_COMMON)
    {
        select_lang(curlang);
        autoupdate_handler(RID(),(GUIPARAM)autoupdate);
        autoupdate_proxy_handler(ts::amake(autoupdate_proxy));
        if (RID r = find(CONSTASTR("proxyaddr")))
        {
            gui_textfield_c &tf = HOLD(r).as<gui_textfield_c>();
            tf.set_text( autoupdate_proxy_addr, true );
            check_proxy_addr(r, autoupdate_proxy_addr);
        }
    }
}

bool dialog_settings_c::network_1( const ts::wstr_c & nnn )
{
    auto row = table_active_protocol_underedit.find<false>(network_props);
    if (!row) return true;

    if (RID lst = find(CONSTASTR("protoactlist")))
    {
        for( rectengine_c *itm : HOLD(lst).engine() )
        {
            if (itm)
            {
                gui_listitem_c * li = ts::ptr_cast<gui_listitem_c *>( &itm->getrect() );
                ts::token<char> t( li->getparam(), '/');
                ++t;
                ++t;
                if (t->as_int() == network_props && ASSERT(li->get_text().begins(row->other.name)))
                {
                    ts::wstr_c n = li->get_text();
                    n.replace(0, row->other.name.get_length(), nnn);
                    li->set_text(n);
                }
            }
        }
    }

    row->other.name = nnn;
    row->changed();

    return true;
}

bool dialog_settings_c::network_2( RID, GUIPARAM p )
{
    auto row = table_active_protocol_underedit.find<false>(network_props);
    if (!row) return true;
    row->changed();
    
    INITFLAG( row->other.options, active_protocol_data_s::O_AUTOCONNECT, p != nullptr );

    return true;
}

void dialog_settings_c::set_proxy_type_handler(const ts::str_c& p)
{
    int psel = p.as_int();
    auto row = table_active_protocol_underedit.find<false>(network_props);
    if (!row) return;
    if (psel == 0) row->other.proxy.proxy_type = 0;
    else if (psel == 1) row->other.proxy.proxy_type = PROXY_SUPPORT_HTTP;
    else if (psel == 2) row->other.proxy.proxy_type = PROXY_SUPPORT_SOCKS4;
    else if (psel == 3) row->other.proxy.proxy_type = PROXY_SUPPORT_SOCKS5;

    if (const protocols_s *pr = find_protocol(row->other.tag))
        set_combik_menu(CONSTASTR("protoproxytype"),list_proxy_types(psel, DELEGATE(this, set_proxy_type_handler), pr->proxy_support));

    if (RID r = find(CONSTASTR("protoproxyaddr")))
        r.call_enable(psel > 0);
}

bool dialog_settings_c::set_proxy_addr_handler(const ts::wstr_c & t)
{
    auto row = table_active_protocol_underedit.find<false>(network_props);
    row->other.proxy.proxy_addr = t;

    if (RID r = find(CONSTASTR("protoproxyaddr")))
        check_proxy_addr(r, row->other.proxy.proxy_addr);
    return true;
}


void dialog_settings_c::create_network_props_ctls( int id )
{
    getengine().trunc_children( num_ctls_in_network_tab );
    if (id == 0)
    {
        network_props = 0;
        return;
    }
    

    auto row = table_active_protocol_underedit.find<false>(id);
    if (!row) return;

    //if (network_props != id)
    network_props = id;


    vspace(5);
    label( CONSTWSTR("<l>") + ts::wstr_c(TTT("Имя сети",70)) + CONSTWSTR("</l>") );
    textfield(row->other.name, MAX_PATH, TFR_TEXT_FILED, DELEGATE(this, network_1));
    vspace(5);

    check_item_s chis[] = 
    {
        check_item_s( TTT("Соединение с сетью при запуске",204), 1 )
    };

    check( ARRAY_WRAPPER(chis), DELEGATE(this, network_2), 0 != (row->other.options & active_protocol_data_s::O_AUTOCONNECT) ? 1 : 0 );

    if (const protocols_s *p = find_protocol(row->other.tag))
        if (p->proxy_support)
        {
            int pt = 0;
            if (row->other.proxy.proxy_type & PROXY_SUPPORT_HTTP) pt = 1;
            if (row->other.proxy.proxy_type & PROXY_SUPPORT_SOCKS4) pt = 2;
            if (row->other.proxy.proxy_type & PROXY_SUPPORT_SOCKS5) pt = 3;
            ts::wstr_c pa = row->other.proxy.proxy_addr;

            vspace(5);
            RID hg = hgroup(TTT("Настройки соединения",169));
            RID ptcombik = combik( list_proxy_types(pt, DELEGATE(this, set_proxy_type_handler), p->proxy_support), hg );
            RID paedit = textfield(pa, MAX_PATH, TFR_TEXT_FILED, DELEGATE(this, set_proxy_addr_handler), nullptr, 0, hg);
            setctlname(CONSTASTR("protoproxytype"), HOLD(ptcombik)());
            setctlname( CONSTASTR("protoproxyaddr"), HOLD(paedit)());

            paedit.call_enable(pt != 0);

            check_proxy_addr(paedit, row->other.proxy.proxy_addr);
        }
}

void dialog_settings_c::add_active_proto( RID lst, int id, const active_protocol_data_s &apdata )
{
    ts::wstr_c desc = describe_network(apdata.name, apdata.tag);
    ts::str_c par(CONSTASTR("2/")); par.append(apdata.tag).append_char('/').append_as_int(id);
    MAKE_CHILD<gui_listitem_c>(lst, desc, par) << DELEGATE(this, getcontextmenu);
}

bool dialog_settings_c::activateprotocol( const ts::wstr_c& name, const ts::str_c& tag )
{
    auto &r = table_active_protocol_underedit.getcreate(0);
    r.other.name = name;
    r.other.tag = tag;
    r.other.config = import;
    if (RID lst = find(CONSTASTR("protoactlist")))
        add_active_proto(lst, r.id, r.other);
    return true;
}

void dialog_settings_c::contextmenuhandler( const ts::str_c& param )
{
    ts::token<char> t(param, '/');
    if (*t == CONSTASTR("add") || *t == CONSTASTR("imp"))
    {
        import.clear();

        if (*t == CONSTASTR("imp"))
        {
            ++t;
            ts::wstr_c title = TTT("Импорт конфигурации для протокола $",55) / ts::wstr_c(*t);
            ts::wstr_c iroot( CONSTWSTR("%APPDATA%") );
            ts::parse_env(iroot);

            if (dir_present( ts::fn_join(iroot, ts::wstr_c(*t)) ))
            {
                iroot = ts::fn_join(iroot, ts::wstr_c(*t));
            }

            ++sysmodal;
            ts::wstr_c fn = ts::get_load_filename_dialog(iroot, CONSTWSTR(""), CONSTWSTR(""), nullptr, title);
            --sysmodal;

            if (fn.is_empty())
                return;

            import.load_from_text_file(fn);

        } else
        {
            ++t;
        }

        SUMMON_DIALOG<dialog_entertext_c>(UD_NETWORKNAME, dialog_entertext_c::params(
            UD_NETWORKNAME,
            TTT("[appname]: имя сети",61),
            TTT("Введите имя сети. Поскольку возможно одновременное использование нескольких сетей одного типа, вам будет удобнее отличать эти сети друг от друга по именам, которые вы сами им дадите.",62),
            TTT("Сеть $",63) / ts::wmake( table_active_protocol_underedit.rows.size() + 1 ),
            *t,
            DELEGATE(this, activateprotocol),
            check_always_ok));

    } else if (*t == CONSTASTR("on"))
    {
        // activate suspended proto
        ++t;
        int id = t->as_int();
        auto *row = table_active_protocol_underedit.find<false>(id);
        if (ASSERT(row))
        {
            RESETFLAG( row->other.options, active_protocol_data_s::O_SUSPENDED );
            row->changed();

            if (RID lst = find(CONSTASTR("protolist")))
                lst.call_kill_child(ts::str_c(CONSTASTR("3/")).append(row->other.tag).append_char('/').append_as_int(id));

            if (RID lst = find(CONSTASTR("protoactlist")))
                add_active_proto(lst, row->id, row->other);
        }
        

    } else if (*t == CONSTASTR("del"))
    {
        ++t;
        int id = t->as_int();
        if (network_props == id) create_network_props_ctls(0);

        ts::str_c tag;
        if (contacts().present_protoid( id ))
        {
            auto *row = table_active_protocol_underedit.find<false>( id );
            if (ASSERT(row))
            {
                row->other.options |= active_protocol_data_s::O_SUSPENDED;
                row->changed();
                tag = row->other.tag;
            }

            if (RID lst = find(CONSTASTR("protolist")))
                add_suspended_proto(lst, id, row->other);

        } else
        {
            tag = table_active_protocol_underedit.del(id).other.tag;
        }

        if (RID lst = find(CONSTASTR("protoactlist")))
            lst.call_kill_child( tag.insert(0,CONSTASTR("2/")).append_char('/').append_as_int(id) );

    } else if (*t == CONSTASTR("props"))
    {
        ++t;
        create_network_props_ctls( t->as_int() );
    }
    else if (*t == CONSTASTR("copy"))
    {
        ++t;
        ts::set_clipboard_text(ts::wstr_c(*t));
    }
}

menu_c dialog_settings_c::getcontextmenu( const ts::str_c& param, bool activation )
{
    menu_c m;
    ts::token<char> t(param, '/');
    if (*t == CONSTASTR("1"))
    {
        if (gmsg<GM_DIALOG_PRESENT>( UD_NETWORKNAME ).send().is(GMRBIT_ACCEPTED))
            return m;

        ++t;
        if (activation)
            contextmenuhandler( ts::str_c(CONSTASTR("add/")).append(*t) );
        else
        {
            m.add(TTT("Добавить к списку активных", 58), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("add/")).append(*t));
            if (!t->equals(CONSTASTR("lan"))) // temporary disable lan import
                m.add(TTT("Импортировать конфигурацию из файла",56), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("imp/")).append(*t));
        }

    } else if (*t == CONSTASTR("2"))
    {
        ++t;
        ++t;
        if (activation)
            contextmenuhandler( ts::str_c(CONSTASTR("del/")).append(*t) );
        else 
        {
            m.add(TTT("Деактивировать", 59), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("del/")).append(*t));
            m.add(TTT("Свойства", 60), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("props/")).append(*t));
        }

    } else if (*t == CONSTASTR("3"))
    {
        ++t;
        ++t;
        if (activation)
            contextmenuhandler(ts::str_c(CONSTASTR("on/")).append(*t) );
        else
            m.add(TTT("Активировать",201),0,DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("on/")).append(*t) );
    }

    return m;
}

void dialog_settings_c::select_lang( const ts::str_c& prm )
{
    curlang = prm;
    set_combik_menu(CONSTASTR("langs"), list_langs(curlang,DELEGATE(this,select_lang)));
}

/*virtual*/ bool dialog_settings_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

/*virtual*/ void dialog_settings_c::on_confirm()
{
    mic_device_changed = false; // to avoid restore in destructor

    if (cfg().language(curlang))
        g_app->load_locale(curlang);

    if (profile_selected)
    {
        bool ch1 = prf().username(username);
        bool ch2 = prf().userstatus(userstatusmsg);
        if (ch1) gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_USERNAME, username).send();
        if (ch2) gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_USERSTATUSMSG, userstatusmsg).send();
        prf().ctl_to_send(ctl2send);
        ch1 = prf().date_msg_template(date_msg_tmpl);
        ch1 |= prf().date_sep_template(date_sep_tmpl);
        if (prf().set_msg_options( msgopts_current, msgopts_changed ) || ch1)
            gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_MSGOPTIONS).send();

        prf().download_folder(downloadfolder);

    }

    if (autoupdate_proxy > 0 && !check_netaddr(autoupdate_proxy_addr))
        autoupdate_proxy_addr = CONSTASTR(DEFAULT_PROXY);

    cfg().autoupdate(autoupdate);
    cfg().autoupdate_proxy(autoupdate_proxy);
    cfg().autoupdate_proxy_addr(autoupdate_proxy_addr);
    if (oautoupdate != autoupdate && autoupdate > 0)
        g_app->autoupdate_next = now() + 10;


    cfg().collapse_beh(collapse_beh);
    bool sndch = cfg().device_talk(talkdevice);
    sndch |= cfg().device_signal(signaldevice);
    if (sndch) g_app->mediasystem().init();

    s3::DEVICE micd = device_from_string(micdevice);
    s3::set_capture_device(&micd);
    stop_capture();
    if (cfg().device_mic(micdevice))
    {
        g_app->capture_device_changed();
        gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_MICDEVICE).send();
    }

    for (const theme_info_s& thi : m_themes)
    {
        if (thi.current)
        {
            if (thi.folder != cfg().theme())
            {
                cfg().theme(thi.folder);
                g_app->load_theme(thi.folder);
            }
            break;
        }
    }

    if (profile_selected)
    {
        prf().get_table_active_protocol() = std::move(table_active_protocol_underedit);
        prf().changed(true); // save now, due its OK pressed: user can wait
    }

    __super::on_confirm();
}

bool dialog_settings_c::ipchandler( ipcr r )
{
    if (r.d == nullptr)
    {
        // lost contact to plghost
        DEFERRED_UNIQUE_CALL( 0, get_close_button_handler(), nullptr );
    } else
    {
        switch (r.header().cmd)
        {
        case HA_PROTOCOLS_LIST:
            {
                int n = r.get<int>();
                while(--n >= 0)
                {
                    ts::tmp_str_c d = r.getastr();
                    int p = d.find_pos(':');
                    protocols_s &proto = available_prots.add();
                    proto.description = d.substr(p + 2);
                    proto.tag = d.substr(0, p);
                    proto.proxy_support = r.get<int>();
                }
            }
            break;
        }
    }

    return true;
}


BOOL __stdcall dialog_settings_c::enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext)
{
    ((dialog_settings_c *)lpContext)->enum_capture_devices(device, lpcstrDescription, lpcstrModule);
    return TRUE;
}
void dialog_settings_c::enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t * /*lpcstrModule*/)
{
    sound_device_s &sd = record_devices.add();
    sd.deviceid = device ? *device : s3::DEFAULT_DEVICE;
    sd.desc.set( ts::wsptr(lpcstrDescription) );
}

void dialog_settings_c::select_capture_device(const ts::str_c& prm)
{
    micdevice = prm;
    s3::DEVICE device = device_from_string(prm);
    s3::set_capture_device(&device);
    set_combik_menu(CONSTASTR("mic"), list_capture_devices());
    s3::start_capture( capturefmt );
    mic_device_changed = true;
}
menu_c dialog_settings_c::list_capture_devices()
{
    s3::DEVICE device;
    s3::get_capture_device(&device);

    menu_c m;
    for (const sound_device_s &sd : record_devices)
    {
        ts::uint32 f = 0;
        if (device == sd.deviceid) f = MIF_MARKED;
        m.add( sd.desc, f, DELEGATE(this, select_capture_device), string_from_device(sd.deviceid) );
    }

    return m;
}

BOOL __stdcall dialog_settings_c::enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext)
{
    ((dialog_settings_c *)lpContext)->enum_play_devices(device, lpcstrDescription, lpcstrModule);
    return TRUE;

}
void dialog_settings_c::enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule)
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
            desc = TTT("Использовать динамики", 133);
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
    media.test_talk();
    return true;
}
bool dialog_settings_c::test_signal_device(RID, GUIPARAM)
{
    media.test_signal();
    return true;
}

void dialog_settings_c::select_talk_device(const ts::str_c& prm)
{
    talkdevice = prm;
    media.init( talkdevice, signaldevice );

    set_combik_menu(CONSTASTR("talk"), list_talk_devices());
}
void dialog_settings_c::select_signal_device(const ts::str_c& prm)
{
    signaldevice = prm;
    media.init(talkdevice, signaldevice);

    set_combik_menu(CONSTASTR("signal"), list_signal_devices());
}

/*virtual*/ void dialog_settings_c::datahandler( const void *data, int size )
{
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
}
