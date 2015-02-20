#pragma once

class dialog_settings_c : public gui_isodialog_c, public sound_capture_handler_c
{
    GM_RECEIVER(dialog_settings_c, ISOGM_NEWVERSION);

    NUMGEN_START( ctlm, 0 );
    enum ctlmask
    {
        MASK_PROFILE_COMMON         = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_CHAT           = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_PROFILE_NETWORKS       = SETBIT( NUMGEN_NEXT(ctlm) ),
        MASK_APPLICATION_SYSTEM     = SETBIT(NUMGEN_NEXT(ctlm)),
        MASK_APPLICATION_COMMON     = SETBIT(NUMGEN_NEXT(ctlm)),
        MASK_APPLICATION_SETSOUND   = SETBIT(NUMGEN_NEXT(ctlm)),
    };

    fmt_converter_s cvter;
    /*virtual*/ s3::Format *formats(int &count) override;

    isotoxin_ipc_s ipcj;

    s3::DEVICE mic_device_stored;
    bool mic_device_changed = false;

    ts::wstr_c username;
    ts::wstr_c userstatusmsg;
    SLANGID curlang;

    ts::wstr_c downloadfolder;
    ts::wstr_c date_msg_tmpl;
    ts::wstr_c date_sep_tmpl;

    bool username_edit_handler( const ts::wstr_c & );
    bool statusmsg_edit_handler( const ts::wstr_c & );

    struct protocols_s
    {
        ts::str_c  tag; // lan, tox
        ts::wstr_c description;
        int proxy_support;
    };

    ts::array_inplace_t<protocols_s,0> available_prots;
    const protocols_s *find_protocol(const ts::str_c& tag) const
    {
        for(const protocols_s& p : available_prots)
            if (p.tag == tag)
                return &p;
        return nullptr;
    }
    tableview_active_protocol_s * table_active_protocol = nullptr;
    tableview_active_protocol_s table_active_protocol_underedit;

    void add_suspended_proto( RID lst, int id, const active_protocol_data_s &apdata );
    void add_active_proto( RID lst, int id, const active_protocol_data_s &apdata );

    void set_proxy_type_handler(const ts::str_c&);
    bool set_proxy_addr_handler(const ts::wstr_c & t);

    bool profile_selected = false;
    bool checking_new_version = false;

    int msgopts = 0;
    int ctl2send = 1;
    int collapse_beh = 2;
    int oautoupdate = 0;
    int autoupdate = 2;
    int autoupdate_proxy = 0;
    int num_ctls_in_network_tab = 0;
    int network_props = 0;
    ts::str_c autoupdate_proxy_addr;
    bool downloadfolder_edit_handler(const ts::wstr_c &v);
    bool date_msg_tmpl_edit_handler(const ts::wstr_c &v);
    bool date_sep_tmpl_edit_handler(const ts::wstr_c &v);
    void create_network_props_ctls( int id );
    bool network_1( const ts::wstr_c & t );
    bool network_2( RID, GUIPARAM );
    bool ctl2send_handler( RID, GUIPARAM );
    bool collapse_beh_handler( RID, GUIPARAM );
    bool autoupdate_handler( RID, GUIPARAM );
    void autoupdate_proxy_handler( const ts::str_c& );
    bool autoupdate_proxy_addr_handler( const ts::wstr_c & t );
    menu_c list_proxy_types(int cur, MENUHANDLER mh, int av = -1);
    void check_proxy_addr(RID editctl, ts::str_c &proxyaddr);
    bool check_update_now(RID, GUIPARAM);

    ts::wstr_c describe_network(const ts::wstr_c& name, const ts::str_c& tag) const;

    bool msgopts_handler( RID, GUIPARAM );

    bool ipchandler( ipcr );
    bool activateprotocol( const ts::wstr_c& name, const ts::str_c& param );
    void contextmenuhandler( const ts::str_c& prm );
    menu_c getcontextmenu( const ts::str_c& param, bool activation );

    void select_lang( const ts::str_c& prm );

    struct sound_device_s
    {
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

    static BOOL __stdcall enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext);
    void enum_capture_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule);
    void select_capture_device( const ts::str_c& prm );
    menu_c list_capture_devices();

    static BOOL __stdcall enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext);
    void enum_play_devices(s3::DEVICE *device, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule);
    menu_c list_talk_devices();
    menu_c list_signal_devices();

    void select_talk_device(const ts::str_c& prm);
    void select_signal_device(const ts::str_c& prm);

protected:
    /*virtual*/ int unique_tag() { return UD_SETTINGS; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions( ts::irect & border ) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void tabselected(ts::uint32 mask) override;
public:
    dialog_settings_c(initial_rect_data_s &data);
    ~dialog_settings_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override { return ts::ivec2(810, 500); }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

