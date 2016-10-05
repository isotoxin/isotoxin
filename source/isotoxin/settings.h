#pragma once

struct dialog_protosetup_params_s;
typedef fastdelegate::FastDelegate<bool (dialog_protosetup_params_s &params)> CONFIRM_PROTOSETUP;

struct dialog_protosetup_params_s
{
    RID watch;

    // new
    ts::iweak_ptr<available_protocols_s> avprotos;
    ts::str_c networktag; // if empty - choose network
    tableview_active_protocol_s *protocols = nullptr;
    CONFIRM_PROTOSETUP confirm;

    // edit
    protocol_description_s proto_desc; // set only for edit new connections
    int protoid = 0;

    ts::str_c uname; // utf8
    ts::str_c ustatus; // utf8
    ts::str_c networkname; // utf8
    ts::wstr_c importcfg; // filename

    configurable_s configurable;
    bool connect_at_startup = true;

    static ts::str_c setup_name( const ts::asptr &tag, ts::aint n );

    dialog_protosetup_params_s() {}

    // new
    dialog_protosetup_params_s(available_protocols_s *avprotos, tableview_active_protocol_s *protocols, CONFIRM_PROTOSETUP confirm) :
        avprotos(avprotos), protocols(protocols), confirm(confirm) {}

    // edit exist
    dialog_protosetup_params_s(CONFIRM_PROTOSETUP confirm) : confirm(confirm) {}
    dialog_protosetup_params_s(int protoid);

    //dialog_protosetup_params_s(int protoid, const ts::str_c &uname,
    //                           const ts::str_c &ustatus,
    //                           const ts::str_c &networkname
    //                           ) : protoid(protoid), uname(uname), ustatus(ustatus), networkname(networkname) {}

    void to( active_protocol_data_s &d ) const;
};

class dialog_setup_network_c;
template<> struct MAKE_ROOT<dialog_setup_network_c> : public _PROOT(dialog_setup_network_c)
{
    dialog_protosetup_params_s prms;
    MAKE_ROOT(const dialog_protosetup_params_s &prms) :_PROOT(dialog_setup_network_c)(), prms(prms) { init( rect_sys_e(RS_NORMAL|RS_MAINPARENT) ); }
    ~MAKE_ROOT() {}
};


class dialog_setup_network_c : public gui_isodialog_c
{
    guirect_watch_c watch;
    dialog_protosetup_params_s params;
    int addh = 0;
    bool predie = false;
    bool password_changed = false;
    bool password_ok = false;

    bool uname_edit(const ts::wstr_c &t, bool );
    bool ustatus_edit(const ts::wstr_c &t, bool );
    bool netname_edit(const ts::wstr_c &t, bool );
    bool login_edit( const ts::wstr_c &t, bool );
    bool password_edit( const ts::wstr_c &t, bool );

    bool network_importfile(const ts::wstr_c & t, bool );
    bool network_serverport(const ts::wstr_c & t, bool );
    bool network_ipv6(RID, GUIPARAM p);
    bool network_udp(RID, GUIPARAM p);
    bool network_connect(RID, GUIPARAM p);
    void set_proxy_type_handler(const ts::str_c&);
    bool set_proxy_addr_handler(const ts::wstr_c & t, bool );

    bool lost_contact(RID, GUIPARAM p);

    void available_network_selected(const ts::str_c&);
    menu_c get_list_avaialble_networks();

protected:
    /*virtual*/ int unique_tag() override { return predie ? UD_NOT_UNIQUE : (params.watch ? UD_PROTOSETUPSETTINGS : UD_PROTOSETUP); }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
public:
    dialog_setup_network_c(MAKE_ROOT<dialog_setup_network_c> &data);
    ~dialog_setup_network_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;

    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void tabselected(ts::uint32 mask) override;
};






class dialog_settings_c : public gui_isodialog_c, public sound_capture_handler_c
{
    NUMGEN_START( ctlm, 0 );
    enum ctlmask
    {
        MASK_PROFILE_COMMON         = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_CLIST          = SETBIT( NUMGEN_NEXT( ctlm ) ),
        MASK_PROFILE_NOTIFICATIONS  = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_CHAT           = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_GCHAT          = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_MSGSNHIST      = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_FILES          = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_NETWORKS       = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_SYSTEM     = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_COMMON     = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_SETSOUND   = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_SOUNDS     = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_VIDEO      = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_ADVANCED_VIDEOCALLS    = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_ADVANCED_TOOLS         = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_ADVANCED_MISC          = SETBIT( NUMGEN_NEXT( ctlm ) ),

        MASK_ADVANCED_DEBUG = SETBIT(NUMGEN_NEXT(ctlm)),
    };

    ts::shared_ptr<theme_rect_s> shadow;

    struct value_watch_s
    {
        const void *underedit;
        virtual ~value_watch_s() {}
        virtual bool changed() const { return true; }
    };

    template<typename T> struct value_watch_t : public value_watch_s
    {
        T original;
        value_watch_t(const T&t):original(t) { underedit = &t; }
        virtual bool changed() const { return original != (*(const T *)underedit); }
    };

    template<> struct value_watch_t<float> : value_watch_s
    {
        float original;
        value_watch_t(const float&t) :original(t) { underedit = &t; }
        virtual bool changed() const { return ts::tabs( original - (*(const float *)underedit) ) > 0.0001f; }
    };

    int force_change = 0;
    ts::array_del_t<value_watch_s, 32> watch_array;

    template <typename T> void watch(const T&t)
    {
        watch_array.add( TSNEW( value_watch_t<T>, t ) );
    }

    template <typename T> bool is_changed(const T&t) const
    {
        for( const value_watch_s *w : watch_array )
            if (w->underedit == &t)
                return w->changed();
        return false;
    }

    fmt_converter_s cvtmic;
    /*virtual*/ const s3::Format *formats( ts::aint &count) override;

    bool signalvolset(RID, GUIPARAM);
    bool talkvolset(RID, GUIPARAM);
    bool micvolset(RID, GUIPARAM);
    bool test_mic(RID, GUIPARAM);

    int tools_bits = 0;
    bool advt_handler(RID, GUIPARAM);

    ts::astrmap_c video_codecs;
    bool disable_video_ex = false;
    int encoding_quality = 0;
    int video_bitrate = 0;
    bool encoding_quality_set(RID, GUIPARAM);
    bool advv_handler(RID, GUIPARAM);
    
    void videocodecs_tab_selected();
    menu_c codecctxmenu(const ts::str_c& param, bool activation);
    void codecselected(const ts::str_c& prm);
    bool set_bitrate(const ts::wstr_c &, bool );

    ts::buf_c testrec;
    s3::Format recfmt;
    float current_mic_level = -1;
    float talk_vol = 1.0f;
    float signal_vol = 1.0f;

    bool mute_all_sounds = false;
    bool mute_handler( RID, GUIPARAM );

    int dsp_flags = 0;
    bool dspf_handler( RID, GUIPARAM );

    int misc_flags = 0;
    int misc_flags_store = 0;
    bool miscf_handler( RID, GUIPARAM );

    int pmisc_flags = 0;
    bool pmiscf_handler( RID, GUIPARAM );

    s3::DEVICE mic_device_stored;
    bool mic_device_changed = false;
    bool mic_test_rec = false;

    ts::Time mic_test_rec_stop, mic_level_refresh;

    SLANGID curlang;

    ts::str_c date_msg_tmpl;
    ts::str_c date_sep_tmpl;

    struct sound_preset_s
    {
        MOVABLE( true );
        ts::wstr_c path;
        ts::wstr_c name;
        ts::abp_c preset;
    };
    ts::array_inplace_t<sound_preset_s, 1> presets;
    int selected_preset = -1;

    menu_c sounds_menu;
    menu_c get_sounds_menu();
    bool sndselhandler( RID, GUIPARAM );
    bool sndplayhandler( RID, GUIPARAM );
    bool sndvolhandler( RID, GUIPARAM );
    
    menu_c get_list_avaialble_sound_presets();
    bool applysoundpreset( RID, GUIPARAM );
    void soundpresetselected(const ts::str_c&);
    bool addlistsound( RID, GUIPARAM );
    
    
    ts::wstr_c sndfn[snd_count];
    float sndvol[snd_count];
    ts::wstr_c sndselctl[snd_count];
    ts::wstr_c sndvolctl[snd_count];
    ts::wstr_c sndplayctl[snd_count];

    int startopt = 0;
    bool startopt_handler( RID, GUIPARAM );
    int detect_startopts();
    void set_startopts();

    int backupopt = 0;
    ts::wstr_c backuppath;
    bool backupopt_handler( RID, GUIPARAM );
    bool backuppath_handler( const ts::wstr_c &, bool );
    
    
    ts::uint8 passwhash[32];
    ts::safe_ptr<rectengine_c> epdlg;
    enum rekey_e
    {
        REKEY_NOTHING_TO_DO,
        REKEY_ENCRYPT,
        REKEY_REENCRYPT,
        REKEY_REMOVECRYPT,
    } rekey = REKEY_NOTHING_TO_DO;
    bool was_encrypted_profile = false;
    bool encrypted_profile = false;
    bool lite_encset = false;
    ts::str_c encrypted_profile_password;
    int enc_val() const { return encrypted_profile_password.is_empty() ? 0 : 1; }

    bool password_entered_to_decrypt(const ts::wstr_c &passwd, const ts::str_c &);
    bool password_not_entered_to_decrypt(RID, GUIPARAM);

    bool password_entered(const ts::wstr_c &passwd, const ts::str_c &);
    bool password_not_entered( RID, GUIPARAM );

    bool re_password_entered(const ts::wstr_c &passwd, const ts::str_c &);
    bool re_password_not_entered(RID, GUIPARAM);

private:

    struct avprots_s : public available_protocols_s
    {
        /*virtual*/ void done(bool fail) override;
    } available_prots;

    tableview_active_protocol_s * table_active_protocol = nullptr;
    tableview_active_protocol_s table_active_protocol_underedit;

    void add_active_proto( RID lst, int id, const active_protocol_data_s &apdata );

    bool is_video_tab_selected = false;
    bool is_networks_tab_selected = false;
    bool is_video_codecs_tab_selected = false;
    bool proto_list_loaded = false;

    bool profile_selected = false;


    enum bits_group_e
    {
        BGROUP_COMMON1,
        BGROUP_COMMON2,
        BGROUP_COMMON3,
        BGROUP_CLIST,
        BGROUP_CHAT,
        BGROUP_GCHAT,
        BGROUP_MSGOPTS,
        BGROUP_TYPING,
        BGROUP_TYPING_NOTIFY,
        BGROUP_HISTORY,
        BGROUP_CALL_NOTIFY,
        BGROUP_INCOMING_NOTIFY,
        BGROUP_SOUNDS_NOTIFY,

        BGROUP_count
    };

    struct bits_edit_s
    {
        dialog_settings_c *settings = nullptr;
        ts::flags32_s::BITS *source = nullptr;
        int current = 0;

        ts::flags32_s::BITS masks[8];
        int nmasks = 0;
        void add( ts::flags32_s::BITS m, bool value = false)
        {
            ASSERT(nmasks < ARRAY_SIZE(masks)-1);
            int curvm = 1 << nmasks;
            if (m)
            {
                bool srcval = (*source & m) != 0;
                INITFLAG(current, curvm, srcval);
            } else
            {
                INITFLAG(current, curvm, value);
            }
            masks[nmasks++] = m;
        }
        void flush();
        bool handler(RID, GUIPARAM p);

    } bgroups[BGROUP_count];

    
    int load_history_count = 0;
    int load_history_count_addition = 0;
    int set_away_on_timer_minutes_value_last = 0;
    int set_away_on_timer_minutes_value = 0;

    int desktop_notification_duration = 5;

    ts::flags32_s::BITS msgopts_current = 0;
    ts::flags32_s::BITS msgopts_original = 0;

    ts::wstr_c tempfolder_sendimg;
    ts::wstr_c tempfolder_handlemsg;

    ts::wstr_c auto_download_masks;
    ts::wstr_c manual_download_masks;
    ts::wstr_c downloadfolder;
    ts::wstr_c downloadfolder_images;
    int fileconfirm = 0;

    int fontsz_conv_text = 1000, fontsz_msg_edit = 1000;
    float font_scale_conv_text = 1.0f, font_scale_msg_edit = 1.0f;
    bool scale_font_conv_text(RID, GUIPARAM);
    bool scale_font_msg_edit(RID, GUIPARAM);

    enter_key_options_s ctl2send = EKO_ENTER_NEW_LINE;
    int double_enter = 0;

    int collapse_beh = 2;
    int oautoupdate = 0;
    int autoupdate = 2;

    int proxy = 0;
    ts::str_c proxy_addr;
    int useproxyfor = 0xffff;

    bool need2rewarn = false;
    ts::wstrings_c disabled_spellchk;
    ts::wstr_c tt_cache;
    ts::Time tt_next_check = ts::Time::past();

    void on_delete_network(const ts::str_c&);
    bool delete_used_network(RID, GUIPARAM);
    void on_delete_network_2(const ts::str_c&);
    bool addeditnethandler(dialog_protosetup_params_s &params);
    bool addnetwork(RID, GUIPARAM);

    bool tempfolder_sendimg_edit_handler( const ts::wstr_c &v, bool );
    bool tempfolder_handlemsg_edit_handler( const ts::wstr_c &v, bool );

    bool fileconfirm_handler(RID, GUIPARAM);
    bool fileconfirm_auto_masks_handler(const ts::wstr_c &v, bool );
    bool fileconfirm_manual_masks_handler(const ts::wstr_c &v, bool );
    bool downloadfolder_edit_handler(const ts::wstr_c &v, bool );
    bool downloadfolder_images_edit_handler(const ts::wstr_c &v, bool );
    bool date_msg_tmpl_edit_handler(const ts::wstr_c &v, bool );
    bool date_sep_tmpl_edit_handler(const ts::wstr_c &v, bool);
    bool ctl2send_handler( RID, GUIPARAM );
    bool ctl2send_handler_de( RID, GUIPARAM );
    bool collapse_beh_handler( RID, GUIPARAM );
    bool autoupdate_handler( RID, GUIPARAM );
    void proxy_handler( const ts::str_c& );
    bool proxy_addr_handler( const ts::wstr_c & t, bool );
    bool useproxy_handler(RID, GUIPARAM);
    
    const protocol_description_s * describe_network(ts::wstr_c&desc, const ts::str_c& name, const ts::str_c& tag, int id) const;

    bool encrypt_handler( RID, GUIPARAM );
    bool msgopts_handler( RID, GUIPARAM );
    bool commonopts_handler( RID, GUIPARAM );
    bool histopts_handler( RID, GUIPARAM );
    bool away_minutes_handler(const ts::wstr_c &v, bool );
    bool load_history_count_handler(const ts::wstr_c &v, bool );
    bool load_history_count_a_handler( const ts::wstr_c &v, bool );

    bool notification_handler( RID, GUIPARAM );
    bool desktop_notification_duration_handler( const ts::wstr_c &v, bool );

    bool seldict(RID, GUIPARAM);
    bool chat_options(RID, GUIPARAM);
    ts::wstr_c getactivedict();

    bool ipchandler( ipcr );
    void contextmenuhandler( const ts::str_c& prm );
    menu_c getcontextmenu( const ts::str_c& param, bool activation );

    void select_lang( const ts::str_c& prm );

    ts::wstr_c smilepack;
    void smile_pack_selected(const ts::str_c&);

    struct theme_info_s
    {
        MOVABLE( true );
        ts::wstr_c folder;
        ts::wstr_c name;
        bool current;
    };

    ts::array_inplace_t<theme_info_s, 0> m_themes;
    void select_theme( const ts::str_c& prm );
    menu_c list_themes();

    struct sound_device_s
    {
        MOVABLE( true );
        s3::DEVICE deviceid;
        ts::wstr_c desc;
    };
    ts::array_inplace_t<sound_device_s, 0> record_devices;
    ts::array_inplace_t<sound_device_s, 0> play_devices;
    mediasystem_c media;

    /*virtual*/ void datahandler( const void *data, int size ) override;

    ts::str_c micdevice;
    ts::str_c talkdevice;
    ts::str_c signaldevice;

    bool test_talk_device( RID, GUIPARAM );
    bool test_signal_device( RID, GUIPARAM );

    static int __stdcall enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext);
    void enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule);
    void select_audio_capture_device( const ts::str_c& prm );
    menu_c list_audio_capture_devices();

    static int __stdcall enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext);
    void enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule);
    menu_c list_talk_devices();
    menu_c list_signal_devices();

    void select_talk_device(const ts::str_c& prm);
    void select_signal_device(const ts::str_c& prm);


    bool drawcamerapanel(RID, GUIPARAM);
    void setup_video_device();
    void select_video_capture_device( const ts::str_c& prm );
    void select_video_capture_res(const ts::str_c& prm);
    menu_c list_video_capture_devices();
    menu_c list_video_capture_resolutions();
    ts::wstr_c fix_camera_res(const ts::wstr_c &r );
    vsb_list_t video_devices;

    ts::wstrmap_c camera;
    UNIQUE_PTR( vsb_c ) video_device;
    process_animation_s initializing_animation;

    void mod();
    void networks_tab_selected();

    bool save_and_close(RID r = RID(), GUIPARAM prm = nullptr);

    ts::astrmap_c debug;
    bool debug_handler(RID, GUIPARAM p);
    bool debug_handler2(RID, GUIPARAM p);
    bool debug_local_upd_url(const ts::wstr_c &, bool );

protected:
    /*virtual*/ int unique_tag() override { return UD_SETTINGS; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions( ts::irect & border ) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void tabselected(ts::uint32 mask) override;
public:
    dialog_settings_c(initial_rect_data_s &data);
    ~dialog_settings_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;


    void set_video_devices( vsb_list_t &&video_devices );
    void set_disabled_splchklist( const ts::wstrings_c &s, bool need2rewarn );
    void set_need2rewarn() { need2rewarn = true; }
};

