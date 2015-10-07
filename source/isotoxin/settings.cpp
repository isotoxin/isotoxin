#include "isotoxin.h"

//-V:dm:807

#define HGROUP_MEMBER ts::wsptr()

#define PROTO_ICON_SIZE 32

#define TEST_RECORD_LEN 5000

static menu_c list_proxy_types(int cur, MENUHANDLER mh, int av = -1)
{
    menu_c m;
    m.add(TTT("Direct connection",159), cur == 0 ? MIF_MARKED : 0, mh, CONSTASTR("0"));

    if (CF_PROXY_SUPPORT_HTTP & av)
        m.add(TTT("HTTP proxy",160), cur == 1 ? MIF_MARKED : 0, mh, CONSTASTR("1"));
    if (CF_PROXY_SUPPORT_SOCKS4 & av)
        m.add(TTT("Socks 4 proxy",161), cur == 2 ? MIF_MARKED : 0, mh, CONSTASTR("2"));
    if (CF_PROXY_SUPPORT_SOCKS5 & av)
        m.add(TTT("Socks 5 proxy",162), cur == 3 ? MIF_MARKED : 0, mh, CONSTASTR("3"));

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

dialog_settings_c::dialog_settings_c(initial_rect_data_s &data) :gui_isodialog_c(data), mic_test_rec_stop(ts::Time::undefined()), mic_level_refresh(ts::Time::past())
{
    gui->register_kbd_callback( __kbd_chop, HOTKEY_TOGGLE_SEARCH_BAR );

    s3::enum_sound_capture_devices(enum_capture_devices, this);
    s3::enum_sound_play_devices(enum_play_devices, this);
    media.init();
    s3::get_capture_device(&mic_device_stored);
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
                thi.name = to_wstr(conf->get_string(CONSTASTR("name"), ts::to_str(thi.name)));
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
            ts::wstr_c path = ts::fn_get_path(CONSTWSTR("/")+ffn); path.trim_left('/');
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

    gmsg<GM_CLOSE_DIALOG>( UD_PROTOSETUPSETTINGS ).send();

    if (gui)
    {
        gui->delete_event(DELEGATE(this, fileconfirm_handler));
        gui->delete_event(DELEGATE(this, msgopts_handler));
        gui->delete_event(DELEGATE(this, delete_used_network));

        gui->unregister_kbd_callback( __kbd_chop );
    }
}

int dialog_settings_c::detect_startopts()
{
    ts::str_c cmdline;
    int is_autostart = detect_autostart(cmdline);
    int asopts = 0;
    if (is_autostart & DETECT_AUSTOSTART) asopts |= 1;
    if (is_autostart & DETECT_READONLY) asopts |= 4;
    if (cmdline.equals(CONSTASTR("minimize"))) asopts |= 2;
    if (0 == (is_autostart & DETECT_AUSTOSTART))
        asopts |= 2; // by default enable minimize
    return asopts;
}

void dialog_settings_c::set_startopts()
{
    autostart( 0 != (1 & startopt) ? ts::get_exe_full_name() : ts::wstr_c(), 0 != (2 & startopt) ? CONSTWSTR("minimize") : ts::wsptr() );
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
    return TTT("[appname]: Settings",31);
}

/*virtual*/ void dialog_settings_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
}

void dialog_settings_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
    if (bcr.tag == 1)
    {
        bcr.btext = TTT("Save",61);
    }

}

bool dialog_settings_c::username_edit_handler( const ts::wstr_c &v )
{
    username = to_utf8(v);
    mod();
    return true;
}

bool dialog_settings_c::statusmsg_edit_handler( const ts::wstr_c &v )
{
    userstatusmsg = to_utf8(v);
    mod();
    return true;
}

bool dialog_settings_c::fileconfirm_handler(RID, GUIPARAM p)
{
    fileconfirm = as_int(p);

    ctlenable(CONSTASTR("confirmauto"), fileconfirm == 0);
    ctlenable(CONSTASTR("confirmmanual"), fileconfirm == 1);

    mod();
    return true;
}
bool dialog_settings_c::fileconfirm_auto_masks_handler(const ts::wstr_c &v)
{
    auto_download_masks = v;
    mod();
    return true;
}
bool dialog_settings_c::fileconfirm_manual_masks_handler(const ts::wstr_c &v)
{
    manual_download_masks = v;
    mod();
    return true;
}

bool dialog_settings_c::downloadfolder_edit_handler(const ts::wstr_c &v)
{
    downloadfolder = v;
    mod();
    return true;
}

bool dialog_settings_c::date_msg_tmpl_edit_handler(const ts::wstr_c &v)
{
    date_msg_tmpl = to_utf8(v);
    mod();
    return true;
}
bool dialog_settings_c::date_sep_tmpl_edit_handler(const ts::wstr_c &v)
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

    ctlenable(CONSTASTR("proxytype"), autoupdate > 0);
    ctlenable(CONSTASTR("proxyaddr"), autoupdate > 0 && autoupdate_proxy > 0);

    mod();
    return true;
}
void dialog_settings_c::autoupdate_proxy_handler(const ts::str_c& p)
{
    autoupdate_proxy = p.as_int();
    set_combik_menu(CONSTASTR("proxytype"), list_proxy_types(autoupdate_proxy, DELEGATE(this, autoupdate_proxy_handler)));
    if (RID r = find(CONSTASTR("proxyaddr")))
    {
        check_proxy_addr(autoupdate_proxy, r, autoupdate_proxy_addr);
        r.call_enable(autoupdate > 0 && autoupdate_proxy > 0);
    }
    mod();
}

bool dialog_settings_c::autoupdate_proxy_addr_handler( const ts::wstr_c & t )
{
    autoupdate_proxy_addr = to_str(t);
    if (RID r = find(CONSTASTR("proxyaddr")))
        check_proxy_addr(autoupdate_proxy, r, autoupdate_proxy_addr);
    mod();
    return true;
}

bool dialog_settings_c::check_update_now(RID, GUIPARAM)
{
    ctlenable(CONSTASTR("checkupdb"), false);

    checking_new_version = true;
    g_app->b_update_ver(RID(), as_param(AUB_ONLY_CHECK));
    return true;
}

ts::uint32 dialog_settings_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    ctlenable(CONSTASTR("checkupdb"), true);

    if (!checking_new_version) return 0;
    checking_new_version = false;

    if (nv.is_error())
    {
        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
            DT_MSGBOX_ERROR,
            TTT("No new versions detected. Connection failed.",303)
            ));

        return 0;
    }

    if (nv.ver.is_empty())
    {
        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
            DT_MSGBOX_INFO,
            TTT("No new versions detected.",194)
            ));
    
        return 0;
    }

    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
        DT_MSGBOX_INFO,
        TTT("New version detected: $",196) / ts::to_wstr(nv.ver.as_sptr())
        ));

    return 0;
}

bool dialog_settings_c::histopts_handler(RID, GUIPARAM p)
{
    int ip = as_int(p);
    if (ip < 0)
    {
        INITFLAG(hist_opts, 2, ip == -1);
        ctlenable(CONSTASTR("loadcount"), ip == -2);
    } else
        hist_opts = (ip & ~2) | (hist_opts & 2);

    mod();
    return true;
}


bool dialog_settings_c::load_history_count_handler(const ts::wstr_c &v)
{
    load_history_count = v.as_int();
    if (load_history_count < 10)
    {
        load_history_count = 10;
        set_edit_value(CONSTASTR("loadcount"), CONSTWSTR("10"));
    }
    mod();
    return true;
}


bool dialog_settings_c::commonopts_handler( RID, GUIPARAM p )
{
    common_opts = as_int(p);
    ctlenable(CONSTASTR("away_min"), 0 != (common_opts & 8));
    mod();
    return true;
}

bool dialog_settings_c::away_minutes_handler(const ts::wstr_c &v)
{
    set_away_on_timer_minutes_value = v.as_int();
    int o = set_away_on_timer_minutes_value;
    set_away_on_timer_minutes_value = ts::CLAMP(o, 1, 180);
    if (o != set_away_on_timer_minutes_value)
        set_edit_value(CONSTASTR("away_min"), ts::wmake(set_away_on_timer_minutes_value));

    mod();
    return true;
}

bool dialog_settings_c::gchatopts_handler(RID, GUIPARAM p)
{
    int newo = as_int(p);
    mute_mic_on_gchat_invite = 0 != (newo & 1);
    mute_speaker_on_gchat_invite = 0 != (newo & 2);

    mod();
    return true;
}

bool dialog_settings_c::msgopts_handler( RID, GUIPARAM p )
{
    ts::flags32_s::BITS newo = (ts::flags32_s::BITS)p;
    msgopts_changed |= newo ^ msgopts_current;
    msgopts_current = newo;

    ctlenable(CONSTASTR("date_msg_tmpl"), 0 != (msgopts_current & MSGOP_SHOW_DATE));
    ctlenable(CONSTASTR("date_sep_tmpl"), 0 != (msgopts_current & MSGOP_SHOW_DATE_SEPARATOR));

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
    for (value_watch_s *vw : watch_array)
        if (vw->changed())
        {
            ch = true;
            break;
        }

    ctlenable(CONSTASTR("dialog_button_1"), ch);
}

#define PREPARE(var, inits) var = inits; watch(var)

/*virtual*/ int dialog_settings_c::additions( ts::irect & border )
{
    PREPARE( force_change, 0 );

    if(profile_selected)
    {
        PREPARE( username, prf().username(); text_prepare_for_edit(username) );
        PREPARE( userstatusmsg, prf().userstatus(); text_prepare_for_edit(userstatusmsg) );

        PREPARE( ctl2send, (enter_key_options_s)prf().ctl_to_send() );

        PREPARE( msgopts_current, prf().get_options().__bits );
        msgopts_changed = 0;

        PREPARE(set_away_on_timer_minutes_value, prf().inactive_time());

        common_opts_orig = 0;
        if (0 != (msgopts_current & UIOPT_SHOW_SEARCH_BAR)) common_opts_orig |= 1;
        if (0 != (msgopts_current & UIOPT_PROTOICONS)) common_opts_orig |= 2;
        if (0 != (msgopts_current & UIOPT_AWAYONSCRSAVER)) common_opts_orig |= 4;
        if (set_away_on_timer_minutes_value > 0) common_opts_orig |= 8;
        PREPARE( common_opts, common_opts_orig );

        hist_opts_orig = 0;
        if (0 != (msgopts_current & MSGOP_KEEP_HISTORY)) hist_opts_orig |= 1;
        if (0 != (msgopts_current & MSGOP_LOAD_WHOLE_HISTORY)) hist_opts_orig |= 2;
        PREPARE(hist_opts, hist_opts_orig);

        PREPARE(load_history_count, prf().min_history());

        PREPARE( mute_mic_on_gchat_invite, 0 != (msgopts_current & GCHOPT_MUTE_MIC_ON_INVITE) );
        PREPARE( mute_speaker_on_gchat_invite, 0 != (msgopts_current & GCHOPT_MUTE_SPEAKER_ON_INVITE) );

        PREPARE( date_msg_tmpl, prf().date_msg_template() );
        PREPARE( date_sep_tmpl, prf().date_sep_template() );
        PREPARE( downloadfolder, prf().download_folder() );
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
    }

    PREPARE( startopt, detect_startopts() );

    PREPARE( curlang, cfg().language() );
    PREPARE( autoupdate, cfg().autoupdate() );
    oautoupdate = autoupdate;
    PREPARE( autoupdate_proxy, cfg().autoupdate_proxy() );
    PREPARE( autoupdate_proxy_addr, cfg().autoupdate_proxy_addr() );
    PREPARE( collapse_beh, cfg().collapse_beh() );
    PREPARE( talkdevice, cfg().device_talk() );
    PREPARE( signaldevice, cfg().device_signal() );
    PREPARE( micdevice, string_from_device(mic_device_stored) );

    PREPARE( cvtmic.volume, cfg().vol_mic() );
    PREPARE( talk_vol, cfg().vol_talk() );
    PREPARE( signal_vol, cfg().vol_signal() );

    PREPARE( dsp_flags, cfg().dsp_flags() );

#define SND(s) PREPARE( sndfn[snd_##s], cfg().snd_##s() ); PREPARE( sndvol[snd_##s], cfg().snd_vol_##s() );
    SOUNDS
#undef SND


    int textrectid = 0;

    menu_c m;
    if (profile_selected)
    {
        m.add_sub( TTT("Profile",1) )
            .add(TTT("General",32), 0, TABSELMI(MASK_PROFILE_COMMON) )
            .add(TTT("Chat",109), 0, TABSELMI(MASK_PROFILE_CHAT) )
            .add(TTT("Group chat",305), 0, TABSELMI(MASK_PROFILE_GCHAT) )
            .add(TTT("History",327), 0, TABSELMI(MASK_PROFILE_HISTORY) )
            .add(TTT("File receive",236), 0, TABSELMI(MASK_PROFILE_FILES) )
            .add(TTT("Networks",33), 0, TABSELMI(MASK_PROFILE_NETWORKS) );
    }

    m.add_sub(TTT("Application",34))
        .add(TTT("General",106), 0, TABSELMI(MASK_APPLICATION_COMMON))
        .add(TTT("System",35), 0, TABSELMI(MASK_APPLICATION_SYSTEM))
        .add(TTT("Audio",125), 0, TABSELMI(MASK_APPLICATION_SETSOUND))
        .add(TTT("Sounds",293), 0, TABSELMI(MASK_APPLICATION_SOUNDS));

    descmaker dm( descs );
    dm << MASK_APPLICATION_COMMON; //_________________________________________________________________________________________________//

    dm().page_header(TTT("General application settings",108));
    dm().vspace(10);
    dm().combik(TTT("Language",107)).setmenu( list_langs( curlang, DELEGATE(this, select_lang) ) ).setname( CONSTASTR("langs") );
    dm().vspace(10);
    dm().combik(TTT("GUI theme",233)).setmenu(list_themes()).setname(CONSTASTR("themes"));
    dm().vspace();
    dm().radio(TTT("Updates",155), DELEGATE(this, autoupdate_handler), autoupdate).setmenu(
        menu_c()
        .add(TTT("Do not check",156), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("Check and notify",157), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Check, download and update",158), 0, MENUHANDLER(), CONSTASTR("2"))
        );
    dm().hgroup(ts::wsptr());
    dm().combik(HGROUP_MEMBER).setmenu( list_proxy_types(autoupdate_proxy, DELEGATE(this, autoupdate_proxy_handler)) ).setname(CONSTASTR("proxytype"));
    dm().textfield(HGROUP_MEMBER, ts::to_wstr(autoupdate_proxy_addr), DELEGATE(this, autoupdate_proxy_addr_handler)).setname(CONSTASTR("proxyaddr"));
    dm().vspace();
    dm().button(HGROUP_MEMBER, TTT("Check for update",195), DELEGATE(this, check_update_now) ).height(35).setname(CONSTASTR("checkupdb"));

    dm << MASK_APPLICATION_SYSTEM; //_________________________________________________________________________________________________//

    dm().page_header( TTT("Some system settings of application",36) );
    dm().vspace(10);

    dm().checkb(TTT("Autostart", 309), DELEGATE(this, startopt_handler), startopt).setname(CONSTASTR("start")).setmenu(
        menu_c().add(loc_text(loc_autostart), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("Minimize on start with system", 311), 0, MENUHANDLER(), CONSTASTR("2"))
        );
    dm().vspace();

    ts::wstr_c profname = cfg().profile();
    if (!profname.is_empty()) profile_c::path_by_name(profname);

    dm().path( TTT("Current profile path",37), ts::to_wstr(profname) ).readonly(true);
    dm().vspace();
    dm().radio(TTT("Minimize to notification area",118), DELEGATE(this, collapse_beh_handler), collapse_beh).setmenu(
        menu_c()
        .add(TTT("Don't minimize to notification area",119), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("[quote]Minimize[quote] button $ minimizes to notification area",120) / CONSTWSTR("<img=bmin,-1>"), 0, MENUHANDLER(), CONSTASTR("1"))
        .add(TTT("[quote]Close[quote] button $ minimizes to notification area",121) / CONSTWSTR("<img=bclose,-1>"), 0, MENUHANDLER(), CONSTASTR("2"))
        );


    dm << MASK_APPLICATION_SETSOUND; //______________________________________________________________________________________________//
    dm().page_header(TTT("Audio settings",127));
    dm().vspace(10);
    dm().hgroup(TTT("Microphone",126));
    dm().combik(HGROUP_MEMBER).setmenu( list_capture_devices() ).setname( CONSTASTR("mic") );
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
    dm().hslider(L"", talk_vol, CONSTWSTR("0/0/1/1"), DELEGATE(this, talkvolset));
    dm().vspace(3);
    dm().checkb(ts::wsptr(), DELEGATE(this, dspf_handler), dsp_flags).setmenu(
        menu_c().add(TTT("Noise filter of incoming peer voice",291), 0, MENUHANDLER(), ts::amake<int>(DSP_SPEAKERS_NOISE))
                .add(TTT("Automatic gain of incoming peer voice",292), 0, MENUHANDLER(), ts::amake<int>(DSP_SPEAKERS_AGC)) );
    dm().vspace();

    dm().hgroup(TTT("Ringtone and other signals",129));
    dm().combik(HGROUP_MEMBER).setmenu( list_signal_devices() ).setname( CONSTASTR("signal") );
    dm().button(HGROUP_MEMBER, CONSTWSTR("face=play"), DELEGATE(this, test_signal_device) ).sethint(TTT("Test ringtone",132));
    dm().vspace();
    dm().hslider(L"", signal_vol, CONSTWSTR("0/0/1/1"), DELEGATE(this, signalvolset));
    dm().vspace(10);
    dm().label(L"").setname("soundhint");

    dm << MASK_APPLICATION_SOUNDS; //______________________________________________________________________________________________//
    dm().page_header(TTT("Sounds",294));
    dm().vspace(10);
    dm().list(TTT("Sounds list",295), L"", -270).setname(CONSTASTR("soundslist"));
    dm().vspace();
    dm().hgroup(TTT("Presets",298));
    dm().combik(HGROUP_MEMBER).setmenu(get_list_avaialble_sound_presets()).setname(CONSTASTR("availablepresets"));
    dm().button(HGROUP_MEMBER, TTT("Apply preset",299), DELEGATE(this, applysoundpreset)).width(250).height(25).setname(CONSTASTR("applypreset"));

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

    if (profile_selected)
    {
        dm << MASK_PROFILE_COMMON; //____________________________________________________________________________________________________//
        dm().page_header( TTT("General profile settings",38) );
        dm().vspace(10);
        dm().textfield( TTT("Name",52), from_utf8(username), DELEGATE(this,username_edit_handler) ).setname(CONSTASTR("uname")).focus(true);
        dm().vspace();
        dm().textfield(TTT("Status",68), from_utf8(userstatusmsg), DELEGATE(this, statusmsg_edit_handler)).setname(CONSTASTR("ustatus"));
        dm().vspace();
        dm().checkb(ts::wstr_c(), DELEGATE(this, commonopts_handler), common_opts).setmenu(
                menu_c().add(TTT("Show search bar ($)",276) / CONSTWSTR("Ctrl+F"), 0, MENUHANDLER(), CONSTASTR("1"))
                        .add(TTT("Protocol icons as contact state indicator",296), 0, MENUHANDLER(), CONSTASTR("2"))
            );

        dm().vspace();

        ts::wstr_c ctl;
        dm().textfield(ts::wstr_c(), ts::wmake(set_away_on_timer_minutes_value), DELEGATE(this, away_minutes_handler)).setname(CONSTASTR("away_min")).width(50).subctl(textrectid++, ctl);
        ts::wstr_c t_awaymin = TTT("Set [b]Away[/b] status when user has been inactive for $ minutes",322) / ctl;
        if (t_awaymin.find_pos(ctl) < 0) t_awaymin.append_char(' ').append(ctl);


        dm().checkb(ts::wstr_c(), DELEGATE(this, commonopts_handler), common_opts).setmenu(
            menu_c().add(TTT("Set [b]Away[/b] status on screen saver activation",323), 0, MENUHANDLER(), CONSTASTR("4"))
                    .add(t_awaymin, 0, MENUHANDLER(), CONSTASTR("8"))
            );

        dm << MASK_PROFILE_CHAT; //____________________________________________________________________________________________________//
        dm().page_header(TTT("Chat settings",110));
        dm().vspace(10);
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
        dm().combik(TTT("Emoticon set",269)).setmenu(emoti().get_list_smile_pack( smilepack, DELEGATE(this, smile_pack_selected) )).setname(CONSTASTR("avasmliepack"));
        dm().vspace();

        ctl.clear();
        dm().textfield( ts::wstr_c(), from_utf8(date_msg_tmpl), DELEGATE(this,date_msg_tmpl_edit_handler) ).setname(CONSTASTR("date_msg_tmpl")).width(100).subctl(textrectid++, ctl);
        ts::wstr_c t_showdate = TTT("Show message date (template: $)",171) / ctl;
        if (t_showdate.find_pos(ctl)<0) t_showdate.append_char(' ').append(ctl);

        ctl.clear();
        dm().textfield(ts::wstr_c(), from_utf8(date_sep_tmpl), DELEGATE(this, date_sep_tmpl_edit_handler)).setname(CONSTASTR("date_sep_tmpl")).width(140).subctl(textrectid++, ctl);
        ts::wstr_c t_showdatesep = TTT("Show separator $ between messages with different date",172) / ctl;
        if (t_showdatesep.find_pos(ctl)<0) t_showdate.append_char(' ').append(ctl);

        dm().checkb(TTT("Messages",170), DELEGATE(this, msgopts_handler), msgopts_current).setmenu(
                    menu_c().add(t_showdate, 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_DATE ))
                            .add(t_showdatesep, 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_DATE_SEPARATOR ))
                            .add(TTT("Show protocol name",173), 0, MENUHANDLER(), ts::amake<int>( MSGOP_SHOW_PROTOCOL_NAME ))
                    );

        dm().vspace();
        dm().checkb(TTT("Typing notification",272), DELEGATE(this, msgopts_handler), msgopts_current).setmenu(
            menu_c().add(TTT("Send typing notification",273), 0, MENUHANDLER(), ts::amake<int>(MSGOP_SEND_TYPING))
                    .add(TTT("Ignore typing notifications",274), 0, MENUHANDLER(), ts::amake<int>(MSGOP_IGNORE_OTHER_TYPING))
            );

        dm << MASK_PROFILE_GCHAT; //____________________________________________________________________________________________________//
        dm().page_header(TTT("Group chat settings",306));
        dm().vspace(10);

        int gchpts = 0;
        if (mute_mic_on_gchat_invite) gchpts |= 1;
        if (mute_speaker_on_gchat_invite) gchpts |= 2;
        dm().checkb(ts::wstr_c(), DELEGATE(this, gchatopts_handler), gchpts).setmenu(
            menu_c().add(TTT("Mute microphone on audio group chat invite",307), 0, MENUHANDLER(), CONSTASTR("1"))
                    .add(TTT("Mute speakers on audio group chat invite",308), 0, MENUHANDLER(), CONSTASTR("2"))
            );

        dm << MASK_PROFILE_HISTORY; //____________________________________________________________________________________________________//

        dm().page_header(TTT("Message log settings",328));
        dm().vspace(10);

        dm().checkb(ts::wstr_c(), DELEGATE(this, histopts_handler), hist_opts).setmenu(
            menu_c().add(TTT("Keep message history", 222), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        ctl.clear();
        dm().textfield(ts::wstr_c(), ts::wmake(load_history_count), DELEGATE(this, load_history_count_handler)).setname(CONSTASTR("loadcount")).width(50).subctl(textrectid++, ctl);
        ts::wstr_c t_loadcount = TTT("...load $ messages",330) / ctl;
        if (t_loadcount.find_pos(ctl) < 0) t_loadcount.append_char(' ').append(ctl);

        dm().vspace();
        dm().radio(TTT("On select contact...",331), DELEGATE(this, histopts_handler), (hist_opts & 2) ? -1 : -2).setmenu(
            menu_c()
            .add(TTT("...load whole history", 329), 0, MENUHANDLER(), CONSTASTR("-1"))
            .add(t_loadcount, 0, MENUHANDLER(), CONSTASTR("-2")) );


        dm << MASK_PROFILE_FILES; //____________________________________________________________________________________________________//
        dm().page_header(TTT("File receive settings",237));
        dm().vspace(10);
        dm().path(TTT("Default download folder",174), downloadfolder, DELEGATE(this, downloadfolder_edit_handler));
        dm().vspace();

        dm().radio(TTT("Confirmation",240), DELEGATE(this, fileconfirm_handler), fileconfirm).setmenu(
            menu_c().add(TTT("Confirmation required for any files",238), 0, MENUHANDLER(), CONSTASTR("0"))
                    .add(TTT("Files start downloading without confirmation",239), 0, MENUHANDLER(), CONSTASTR("1"))
            );

        dm().vspace();
        dm().textfield(TTT("These files start downloading without confirmation",241), auto_download_masks, DELEGATE(this, fileconfirm_auto_masks_handler)).setname(CONSTASTR("confirmauto"));
        dm().vspace();
        dm().textfield(TTT("These files require confirmation to start downloading",242), manual_download_masks, DELEGATE(this, fileconfirm_manual_masks_handler)).setname(CONSTASTR("confirmmanual"));


        dm << MASK_PROFILE_NETWORKS; //_________________________________________________________________________________________________//
        dm().page_header(TTT("[appname] supports simultaneous connections to multiple networks.",130));
        dm().vspace(10);
        dm().list(TTT("Active network connections",54), L"", -270).setname(CONSTASTR("protoactlist"));
        dm().vspace();
        dm().hgroup(TTT("Available networks",53));
        dm().combik(HGROUP_MEMBER).setmenu(get_list_avaialble_networks()).setname(CONSTASTR("availablenets"));
        dm().button(HGROUP_MEMBER, TTT("Add network connection",58), DELEGATE(this, addnetwork)).width(250).height(25).setname(CONSTASTR("addnet"));
    }

    gui_vtabsel_c &tab = MAKE_CHILD<gui_vtabsel_c>( getrid(), m );
    tab.leech( TSNEW(leech_dock_left_s, 170) );
    border = ts::irect(175,0,0,0);

    mod();

    return 1;
}

bool dialog_settings_c::startopt_handler( RID, GUIPARAM p )
{
    startopt = as_int(p);
   
    ctlenable( CONSTASTR("start2"), 0 != (startopt & 1) );

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

menu_c dialog_settings_c::get_list_avaialble_sound_presets()
{
    menu_c m;
    m.add(TTT("Choose preset",300), selected_preset < 0 ? MIF_MARKED : 0, DELEGATE(this, soundpresetselected), CONSTASTR("-1"));
    bool sep = false;
    int cnt = presets.size();
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

        sound_preset_s &prst = presets.get(selected_preset);
        for(int i=0;i<snd_count;++i)
            if (const ts::abp_c *rec = prst.preset.get(names[i]))
            {
                sndfn[i] = ts::fn_join( prst.path, ts::from_utf8(rec->as_string()) );
                if (RID ectl = find( ts::amake(CONSTASTR("sndfn"), i) ))
                    HOLD(ectl).as<gui_textedit_c>().set_text(sndfn[i]);
            }

        mod();

    }
    return true;
}


menu_c dialog_settings_c::get_sounds_menu()
{
    return sounds_menu;
}


const dialog_settings_c::protocols_s * dialog_settings_c::describe_network(ts::wstr_c&desc, const ts::str_c& name, const ts::str_c& tag, int id) const
{
    const protocols_s *p = find_protocol(tag);
    if (p == nullptr)
    {
        desc.replace_all(CONSTWSTR("{name}"), TTT("$ (Unknown protocol: $)",57) / from_utf8(name) / ts::to_wstr(tag));
        desc.replace_all(CONSTWSTR("{id}"), CONSTWSTR("?"));
        desc.replace_all(CONSTWSTR("{module}"), CONSTWSTR("?"));
    } else
    {
        ts::wstr_c pubid = to_wstr(contacts().find_pubid(id));
        if (pubid.is_empty()) pubid = TTT("not yet created or loaded",200); else pubid.insert(0, CONSTWSTR("<ee>"));

        desc.replace_all(CONSTWSTR("{name}"), from_utf8(name));
        desc.replace_all(CONSTWSTR("{id}"), pubid);
        desc.replace_all(CONSTWSTR("{module}"), from_utf8(p->description));
    }
    return p;
}

void dialog_settings_c::networks_tab_selected()
{
    set_list_emptymessage(CONSTASTR("protoactlist"), TTT("Empty list. Please add networks",278));
    if (RID lst = find(CONSTASTR("protoactlist")))
    {
        for (auto &row : table_active_protocol_underedit)
            add_active_proto(lst, row.id, row.other);
        available_network_selected(selected_available_network);
    }
}

namespace
{
    struct load_proto_list_s : public ts::task_c
    {
        ts::safe_ptr<dialog_settings_c> dlg;
        load_proto_list_s(dialog_settings_c *dlg) :dlg(dlg) {}
        ~load_proto_list_s() {}

        int result_x = 1;
        ts::array_inplace_t<dialog_settings_c::protocols_s, 0> available_prots;

        bool ipchandler(ipcr r)
        {
            if (r.d == nullptr)
            {
                // lost contact to plghost
                // just close settings
                if (dlg)
                    DEFERRED_UNIQUE_CALL(0, dlg->get_close_button_handler(), nullptr);

                result_x = R_CANCEL;
                return false;
            }
            else
            {
                switch (r.header().cmd)
                {
                    case HA_PROTOCOLS_LIST:
                    {
                        int n = r.get<int>();
                        while (--n >= 0)
                        {
                            ts::pstr_c d = r.getastr();
                            int p = d.find_pos(':');
                            dialog_settings_c::protocols_s &proto = available_prots.add();
                            proto.description = d.substr(p + 2);
                            proto.tag = d.substr(0, p);
                            proto.features = r.get<int>();
                            proto.connection_features = r.get<int>();

                            int icosz;
                            const void *icodata = r.get_data(icosz);
                            proto.icon.reset( prepare_proto_icon( proto.tag, icodata, icosz, PROTO_ICON_SIZE, IT_NORMAL ) );
                        }

                        result_x = R_DONE;
                    }
                    return false;
                }
            }

            return true;
        }


        /*virtual*/ int iterate(int pass) override
        {
            isotoxin_ipc_s ipcj(ts::str_c(CONSTASTR("isotoxin_settings_")).append_as_uint(GetCurrentThreadId()), DELEGATE(this, ipchandler) );
            ipcj.send(ipcw(AQ_GET_PROTOCOLS_LIST));
            ipcj.wait_loop(nullptr);
            ASSERT(result_x != 1);
            return result_x;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && dlg)
                dlg->protocols_loaded(available_prots);

            __super::done(canceled);
        }
    };
}

void dialog_settings_c::protocols_loaded(ts::array_inplace_t<protocols_s, 0> &prots)
{
    available_prots = std::move(prots);
    set_combik_menu(CONSTASTR("availablenets"), get_list_avaialble_networks());
    if (is_networks_tab_selected) networks_tab_selected();
    proto_list_loaded = true;
}


/*virtual*/ void dialog_settings_c::tabselected(ts::uint32 mask)
{
    if (0 == (mask & MASK_APPLICATION_SETSOUND))
        stop_capture();

    is_networks_tab_selected = false;
    if ( mask & MASK_PROFILE_NETWORKS )
    {
        is_networks_tab_selected = true;
        if (proto_list_loaded)
        {
            networks_tab_selected();
        } else
        {
            ctlenable(CONSTASTR("addnet"), false);
            set_list_emptymessage(CONSTASTR("protoactlist"), TTT("Loading",277));
            ASSERT( prf().is_loaded() );
            g_app->add_task(TSNEW(load_proto_list_s, this));
            return;
        }
    }

    if (mask & MASK_PROFILE_CHAT)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, msgopts_handler), msgopts_current);
        int enter_key = ctl2send; if (enter_key == EKO_ENTER_NEW_LINE_DOUBLE_ENTER) enter_key = EKO_ENTER_NEW_LINE;
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, ctl2send_handler), enter_key);
    }
    if (mask & MASK_PROFILE_HISTORY)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, histopts_handler), hist_opts);
    }

    if (mask & MASK_PROFILE_FILES)
    {
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, fileconfirm_handler), fileconfirm);
    }

    if (mask & MASK_PROFILE_COMMON)
    {
        DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, commonopts_handler), common_opts);
    }

    if (mask & MASK_APPLICATION_COMMON)
    {
        select_lang(curlang);
        autoupdate_handler(RID(),as_param(autoupdate));
        autoupdate_proxy_handler(ts::amake(autoupdate_proxy));
        if (RID r = find(CONSTASTR("proxyaddr")))
        {
            gui_textfield_c &tf = HOLD(r).as<gui_textfield_c>();
            tf.set_text( to_wstr(autoupdate_proxy_addr), true );
            check_proxy_addr(autoupdate_proxy, r, autoupdate_proxy_addr);
        }
    }

    if (mask & MASK_APPLICATION_SYSTEM)
    {
        bool allow_edit_autostart = 0 == (startopt & 4);
        ctlenable( CONSTASTR("start1"), allow_edit_autostart );
        ctlenable( CONSTASTR("start2"), allow_edit_autostart && 0 != (startopt & 1) );
    }

    if (mask & MASK_APPLICATION_SETSOUND)
    {
        testrec.clear();
        set_slider_value( CONSTASTR("micvol"), cvtmic.volume );
        start_capture();
    }

    if (mask & MASK_APPLICATION_SOUNDS)
    {
        // #snd

        ts::wsptr sdescs[snd_count] = {
            TTT("Incoming message",1000),
            TTT("Incoming file",1001),
            TTT("File receiving started",1002),
            TTT("Ringtone",1003),
            TTT("Ringtone during call",1004),
            TTT("Call accept",1005),
            TTT("Call cancel",1006),
            TTT("Hang up",1007),
            TTT("Dialing",1008),
            TTT("New version",1009),
        };

        int sorted[snd_count] = {
            snd_incoming_message,
            snd_incoming_file,
            snd_start_recv_file,
            snd_ringtone,
            snd_ringtone2,
            snd_calltone,
            snd_call_accept,
            snd_call_cancel,
            snd_hangup,
            snd_new_version,
        };

        TS_STATIC_CHECK( ARRAY_SIZE(sdescs) == snd_count, "sz" );
        TS_STATIC_CHECK( ARRAY_SIZE(sorted) == snd_count, "sz" );

        ctlenable(CONSTASTR("applypreset"), selected_preset >= 0);

        if (RID lst = find(CONSTASTR("soundslist")))
        {
            gui_vscrollgroup_c &l = HOLD(lst).as<gui_vscrollgroup_c>();

            for(int sndi = 0; sndi < snd_count; ++sndi)
            {
                int ssndi = sorted[sndi];

                ts::wstr_c lit( CONSTWSTR("<b>") );
                lit.append(sdescs[ssndi]);
                lit.append(CONSTWSTR("</b><br=5><p=c>"));
                lit.append(sndselctl[ssndi]).append_char(' ');
                lit.append(sndvolctl[ssndi]);
                lit.append(sndplayctl[ssndi]);
                lit.append(CONSTWSTR("<br=-15> "));

                MAKE_CHILD<gui_listitem_c>(lst, lit, ts::amake(ssndi));
            }

            l.scroll_to_begin();
        }

    }
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
        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
            DT_MSGBOX_WARNING,
            TTT("Connection [b]$[/b] in use! All contacts of this connection will be deleted. History of these contacts will be deleted too. Are you still sure?",267) / from_utf8(row->other.name)
            ).on_ok(DELEGATE(this, on_delete_network_2), ts::amake<int>(as_int(param))).bcancel());
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
        r.other.tag = params.networktag;
        if (!params.importcfg.is_empty())
            r.other.config.load_from_disk_file( params.importcfg );
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
                        ts::wstr_c desc = make_proto_desc(MPD_NAME | MPD_MODULE | MPD_ID);
                        describe_network(desc, apd->name, apd->tag, params.protoid);
                        li->set_text(desc);
                        break;
                    }
                }
    }

    ++force_change; mod();

    return true;
}

bool dialog_settings_c::addnetwork(RID, GUIPARAM)
{
    const protocols_s *p = find_protocol(selected_available_network);
    if (!p) return true;
    int n = table_active_protocol_underedit.rows.size() + 1;
    again:
    for (auto &row : table_active_protocol_underedit)
    {
        if (row.other.name.extract_numbers().as_int() == n)
        {
            ++n;
            goto again;
        }
    }
    ts::wstr_c name = TTT("Connection $",63) / ts::to_wstr(selected_available_network).append_char(' ').append(ts::wmake(n));

    dialog_protosetup_params_s prms(selected_available_network, to_utf8(name), p->features, p->connection_features, DELEGATE(this,addeditnethandler));
    prms.configurable.udp_enable = true;
    prms.configurable.server_port = 0;
    prms.configurable.initialized = true;
    prms.connect_at_startup = true;
    SUMMON_DIALOG<dialog_setup_network_c>(UD_PROTOSETUPSETTINGS, prms);

    return true;
}

void dialog_settings_c::available_network_selected(const ts::str_c& ntag)
{
    selected_available_network = ntag;
    ctlenable(CONSTASTR("addnet"), !selected_available_network.is_empty());
    set_combik_menu(CONSTASTR("availablenets"), get_list_avaialble_networks());
}

menu_c dialog_settings_c::get_list_avaialble_networks()
{
    menu_c m;
    m.add(TTT("Choose network",201), selected_available_network.is_empty() ? MIF_MARKED : 0, DELEGATE( this,  available_network_selected ));
    bool sep = false;
    for (const protocols_s&proto : available_prots)
    {
        if (!sep) m.add_separator();
        sep = true;
        m.add( from_utf8(proto.description), selected_available_network.equals(proto.tag) ? MIF_MARKED : 0, DELEGATE( this,  available_network_selected ), proto.tag);
    }
    return m;
}


void dialog_settings_c::add_active_proto( RID lst, int id, const active_protocol_data_s &apdata )
{
    ts::wstr_c desc = make_proto_desc( MPD_NAME | MPD_MODULE | MPD_ID );
    const protocols_s *p = describe_network(desc, apdata.name, apdata.tag, id);

    ts::str_c par(CONSTASTR("2/")); par.append(apdata.tag).append_char('/').append_as_int(id);
    MAKE_CHILD<gui_listitem_c>(lst, desc, par) << DELEGATE(this, getcontextmenu) << (const ts::drawable_bitmap_c *)(p ? p->icon.get() : nullptr);
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
                SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_WARNING,
                    TTT("Connection [b]$[/b] will be deleted![br]Are you sure?",266) / from_utf8(row->other.name)
                    ).on_ok(DELEGATE(this, on_delete_network), *t).bcancel());
            }
        }

    } else if (*t == CONSTASTR("props"))
    {
        ++t;

        auto *row = table_active_protocol_underedit.find<true>(t->as_int());
        if (CHECK(row))
        {
            const protocols_s *p = find_protocol(row->other.tag);
            if (CHECK(p))
            {
                dialog_protosetup_params_s prms(row->other.tag, row->other.name, p->features, p->connection_features, DELEGATE(this, addeditnethandler));
                prms.uname = row->other.user_name;
                prms.ustatus = row->other.user_statusmsg;
                prms.protoid = row->id;
                prms.configurable = row->other.configurable;
                prms.connect_at_startup = 0 != (row->other.options & active_protocol_data_s::O_AUTOCONNECT);
                SUMMON_DIALOG<dialog_setup_network_c>(UD_PROTOSETUPSETTINGS, prms);
            }
        }

    }
    else if (*t == CONSTASTR("copy"))
    {
        ++t;
        ts::set_clipboard_text(ts::to_wstr(*t));
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
            contextmenuhandler( ts::str_c(CONSTASTR("props/")).append(*t) );
        else 
        {
            m.add(ts::wstr_c(CONSTWSTR("<b>")).append(TTT("Properties",60)), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("props/")).append(*t));
            m.add(TTT("Delete",59), 0, DELEGATE(this, contextmenuhandler), ts::str_c(CONSTASTR("del/")).append(*t));
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
    {
        g_app->load_locale(curlang);
        gmsg<ISOGM_CHANGED_SETTINGS>(0, CFG_LANGUAGE, curlang).send();
    }

    if (profile_selected)
    {
        bool ch1 = prf().username(username);
        bool ch2 = prf().userstatus(userstatusmsg);
        if (ch1) gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_USERNAME, username).send();
        if (ch2) gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_USERSTATUSMSG, userstatusmsg).send();
        prf().ctl_to_send(ctl2send);
        ch1 = prf().date_msg_template(date_msg_tmpl);
        ch1 |= prf().date_sep_template(date_sep_tmpl);

#define UPSETOPT(b,m) if (is_changed(b)) { msgopts_changed |= m; INITFLAG(msgopts_current, m, b); }
        UPSETOPT(mute_mic_on_gchat_invite, GCHOPT_MUTE_MIC_ON_INVITE);
        UPSETOPT(mute_speaker_on_gchat_invite, GCHOPT_MUTE_SPEAKER_ON_INVITE);
#undef UPSETOPT

#define UPSETOPT(bm,m) if ((common_opts_orig ^ common_opts) & bm) { msgopts_changed |= m; INITFLAG(msgopts_current, m, 0 != (common_opts & bm)); }
        UPSETOPT(1, UIOPT_SHOW_SEARCH_BAR);
        UPSETOPT(2, UIOPT_PROTOICONS);
        UPSETOPT(4, UIOPT_AWAYONSCRSAVER);
#undef UPSETOPT

#define UPSETOPT(bm,m) if ((hist_opts_orig ^ hist_opts) & bm) { msgopts_changed |= m; INITFLAG(msgopts_current, m, 0 != (hist_opts & bm)); }
        UPSETOPT(1, MSGOP_KEEP_HISTORY);
        UPSETOPT(2, MSGOP_LOAD_WHOLE_HISTORY);
#undef UPSETOPT

        prf().min_history(load_history_count);
        prf().inactive_time( (common_opts & 8) ? set_away_on_timer_minutes_value : 0 );

        if (prf().set_options( msgopts_current, msgopts_changed ) || ch1)
            gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_PROFILEOPTIONS).send();

        prf().download_folder(downloadfolder);
        prf().auto_confirm_masks( auto_download_masks );
        prf().manual_confirm_masks( manual_download_masks );
        prf().fileconfirm( fileconfirm );
        if (prf().emoticons_pack(smilepack))
        {
            emoti().reload();
            gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_EMOJISET).send();
        }
    }

    if (is_changed(startopt))
        set_startopts();

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
        g_app->capture_device_changed();

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
    mod();
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

        media.play_voice((uint64)-1,recfmt,testrec.data(),testrec.size(), talk_vol, dspf);
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
    media.init( talkdevice, signaldevice );

    set_combik_menu(CONSTASTR("talk"), list_talk_devices());
    mod();
}
void dialog_settings_c::select_signal_device(const ts::str_c& prm)
{
    signaldevice = prm;
    media.init(talkdevice, signaldevice);

    set_combik_menu(CONSTASTR("signal"), list_signal_devices());
    mod();
}

static float find_max(const s3::Format&fmt, const void *idata, int isize)
{
    if (fmt.bitsPerSample == 8)
    {
        int m = 0;
        for (int i = 0; i < isize; ++i)
        {
            ts::uint8 sample8 = ((ts::uint8 *)idata)[i];
            int t = ts::tabs((int)sample8 - 128);
            if (t > m) m = t;
        }
        return (float)m * (float)(1.0/128.0);
    }
    ASSERT(fmt.bitsPerSample == 16);
    int samples = isize / 2;

    int m = 0;
    for (int i = 0; i < samples; ++i)
    {
        int samplex = ((ts::int16 *)idata)[i];
        int t = ts::tabs(samplex);
        if (t > m) m = t;
    }

    return (float)m * (float)(1.0/32767.0);
}

/*virtual*/ void dialog_settings_c::datahandler( const void *data, int size )
{
    bool mic_level_detected = false;
    if (mic_test_rec)
    {
        struct ss_s
        {
            ts::buf_c *buf;
            float current_level;
            void addb(const s3::Format&f, const void *data, int size)
            {
                float m = find_max(f, data, size);
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
            ts::wstr_c t(CONSTWSTR("<p=c><b>"));
            t.append( maketag_color<ts::wchar>( ts::ARGB(155,0,0) ) );
            t.append( TTT("Recording test sound...$",282) / prc );

            set_label_text(CONSTASTR("soundhint"), t);
        }
    }

    if (!mic_level_detected)
        current_mic_level = ts::tmax( current_mic_level, ts::CLAMP( find_max(capturefmt, data, size) * cvtmic.volume, 0, 1 ) );

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









// proto setup

dialog_setup_network_c::dialog_setup_network_c(MAKE_ROOT<dialog_setup_network_c> &data) :gui_isodialog_c(data), params(data.prms)
{
    if (params.protoid && !params.confirm)
        if (active_protocol_c *ap = prf().ap(params.protoid))
        {
            params.uname = ap->get_uname();
            params.ustatus = ap->get_ustatusmsg();
            params.networkname = ap->get_name();
        }
}

dialog_setup_network_c::~dialog_setup_network_c()
{
    //gui->delete_event(DELEGATE(this, refresh_current_page));
}


/*virtual*/ void dialog_setup_network_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
}

void dialog_setup_network_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

/*virtual*/ int dialog_setup_network_c::additions(ts::irect & border)
{

    descmaker dm(descs);
    dm << 1;

    ts::wstr_c hdr(CONSTWSTR("<l>"));

    bool newnet = true;
    if (params.protoid)
        if (active_protocol_c *ap = prf().ap(params.protoid))
            hdr.append(from_utf8(ap->get_desc())), newnet = false;
    if (newnet)
        hdr.append(TTT("new network connection will be created: $",62) / ts::to_wstr(params.networktag));
    hdr.append(CONSTWSTR("</l>"));

    dm().page_header(hdr);


    dm().vspace(15);
    dm().textfield(TTT("Network connection name",70), from_utf8(params.networkname), DELEGATE(this, netname_edit)).focus(true);
    dm().vspace();
    dm().textfield(TTT("Your name on this network",261), from_utf8(params.uname), DELEGATE(this, uname_edit));
    dm().vspace();
    dm().textfield(TTT("Your status on this network",262), from_utf8(params.ustatus), DELEGATE(this, ustatus_edit));

    dm().vspace();

    if ( params.confirm )
    {
        dm().checkb(ts::wstr_c(), DELEGATE(this, network_connect), params.connect_at_startup ? 1 : 0).setmenu(
            menu_c().add(TTT("Connect at startup",204), 0, MENUHANDLER(), CONSTASTR("1"))  );

        addh += 22;
    }

    if (0 != (params.conn_features & CF_UDP_OPTION))
    {
        ASSERT(params.configurable.initialized);
        addh += 22;
        dm().checkb(ts::wstr_c(), DELEGATE(this, network_udp), params.configurable.udp_enable ? 1 : 0).setmenu(
            menu_c().add(TTT("Allow UDP",264), 0, MENUHANDLER(), CONSTASTR("1")));
    }

    if (0 != (params.conn_features & CF_SERVER_OPTION))
    {
        ASSERT(params.configurable.initialized);
        dm().vspace(5);
        addh += 45;
        dm().textfield(TTT("Server port. If 0, the server is disabled.",265), ts::wmake<int>(params.configurable.server_port), DELEGATE(this, network_serverport));
    }

    if (0 != (params.conn_features & CF_PROXY_MASK))
    {
        ASSERT(params.configurable.initialized);
        addh += 45;

        int pt = 0;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_HTTP) pt = 1;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_SOCKS4) pt = 2;
        if (params.configurable.proxy.proxy_type & CF_PROXY_SUPPORT_SOCKS5) pt = 3;
        ts::wstr_c pa = to_wstr(params.configurable.proxy.proxy_addr);

        dm().vspace(5);

        dm().hgroup(TTT("Connection settings",169));
        dm().combik(HGROUP_MEMBER).setmenu(list_proxy_types(pt, DELEGATE(this, set_proxy_type_handler), params.conn_features & CF_PROXY_MASK)).setname(CONSTASTR("protoproxytype"));
        dm().textfield(HGROUP_MEMBER, ts::to_wstr(params.configurable.proxy.proxy_addr), DELEGATE(this, set_proxy_addr_handler)).setname(CONSTASTR("protoproxyaddr"));
    }

    if (params.protoid == 0 && 0 != (params.features & PF_IMPORT))
    {
        dm().vspace(5);
        addh += 45;

        ts::wstr_c iroot(CONSTWSTR("%APPDATA%"));
        ts::parse_env(iroot);

        if (dir_present(ts::fn_join(iroot, ts::to_wstr(params.networktag))))
            iroot = ts::fn_join(iroot, ts::to_wstr(params.networktag));
        ts::fix_path(iroot, FNO_APPENDSLASH);

        dm().file(TTT("Import configuration from file",56), iroot, ts::wstr_c(), DELEGATE(this, network_importfile));
    }

    return 0;
}

bool dialog_setup_network_c::uname_edit(const ts::wstr_c &t)
{
    params.uname = to_utf8(t);
    return true;
}

bool dialog_setup_network_c::ustatus_edit(const ts::wstr_c &t)
{
    params.ustatus = to_utf8(t);
    return true;
}

bool dialog_setup_network_c::netname_edit(const ts::wstr_c &t)
{
    params.networkname = to_utf8(t);
    return true;
}

/*virtual*/ bool dialog_setup_network_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

    __super::on_confirm();
}

/*virtual*/ ts::wstr_c dialog_setup_network_c::get_name() const
{
    return TTT("[appname]: Connection properties",263);
}
/*virtual*/ ts::ivec2 dialog_setup_network_c::get_min_size() const
{
    return ts::ivec2(400, 300 + addh);
}

bool dialog_setup_network_c::network_importfile(const ts::wstr_c & t)
{
    params.importcfg = t;
    return true;
}

bool dialog_setup_network_c::network_serverport(const ts::wstr_c & t)
{
    params.configurable.initialized = true;
    params.configurable.server_port = t.as_int();
    if (params.configurable.server_port < 0 || params.configurable.server_port > 65535)
        params.configurable.server_port = 0;
    return true;
}

bool dialog_setup_network_c::network_udp(RID, GUIPARAM p)
{
    params.configurable.initialized = true;
    params.configurable.udp_enable = p != nullptr;
    return true;
}

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
    else if (psel == 2) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_SOCKS4;
    else if (psel == 3) params.configurable.proxy.proxy_type = CF_PROXY_SUPPORT_SOCKS5;

    set_combik_menu(CONSTASTR("protoproxytype"), list_proxy_types(psel, DELEGATE(this, set_proxy_type_handler), params.conn_features & CF_PROXY_MASK));
    if (RID r = find(CONSTASTR("protoproxyaddr")))
    {
        check_proxy_addr(params.configurable.proxy.proxy_type, r, params.configurable.proxy.proxy_addr);
        r.call_enable(psel > 0);
    }
}

bool dialog_setup_network_c::set_proxy_addr_handler(const ts::wstr_c & t)
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
}






dialog_protosetup_params_s::dialog_protosetup_params_s(int protoid) : protoid(protoid)
{
    if (active_protocol_c *ap = prf().ap(protoid))
    {
        features = ap->get_features();
        conn_features = ap->get_conn_features();
        configurable = ap->get_configurable();
        if (configurable.proxy.proxy_addr.is_empty())
            configurable.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);
    }
}



