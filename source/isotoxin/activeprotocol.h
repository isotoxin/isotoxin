#pragma once

#define NUMTICKS_COUNTDOWN_UNLOAD 6

struct proxy_settings_s
{
    int proxy_type = 0;
    ts::str_c proxy_addr;
    proxy_settings_s() {}
    proxy_settings_s(const proxy_settings_s &p) :proxy_type(p.proxy_type)
    {
        proxy_addr.setcopy(p.proxy_addr); // make copy to avoid race condition
    }
    void operator=(const proxy_settings_s &p)
    {
        proxy_type = p.proxy_type;
        proxy_addr.setcopy(p.proxy_addr); // make copy to avoid race condition
    }
    void operator=(proxy_settings_s &&p)
    {
        proxy_type = p.proxy_type;
        proxy_addr = std::move(p.proxy_addr);
        p.proxy_addr.clear();
    }
    bool operator!=(const proxy_settings_s &p) const
    {
        return proxy_type != p.proxy_type || !proxy_addr.equals(p.proxy_addr);
    }

};

struct configurable_s
{
    // db fields
    // loaded from db
    // no need to wait these fields from protocol
    ts::str_c login;
private:
    ts::str_c password;
public:

    // protocol fields
    // protocol will handle these fields itself
    proxy_settings_s proxy;
    int server_port = 0;

#define COPDEF( n, dv ) unsigned n : 1;
    CONN_OPTIONS
#undef COPDEF

    unsigned initialized : 1;

    bool operator != (const configurable_s &o) const
    {
        return initialized != o.initialized || proxy != o.proxy || server_port != o.server_port ||
            
#define COPDEF( n, dv ) n != o.n ||
        CONN_OPTIONS
#undef COPDEF

            login != o.login || password != o.password;
    }
    configurable_s()
    {
#define COPDEF( n, dv ) n = dv;
        CONN_OPTIONS
#undef COPDEF
            
        initialized = 0;
    }
    configurable_s(const configurable_s &c):initialized(0)
    {
        *this = c;
    }
    configurable_s &operator=(const configurable_s &c)
    {
        // always copy db fields
        login = c.login;
        password = c.password;

        if (!c.initialized) return *this;
        proxy = c.proxy;
        server_port = c.server_port;

#define COPDEF( n, dv ) n = c.n;
        CONN_OPTIONS
#undef COPDEF

        initialized = 1;
        return *this;
    }

    ts::str_c get_password_encoded() const { return password; }
    void set_password( const ts::asptr&p );
    void set_password_as_is( const ts::asptr&p ) { password = p; }
    bool get_password_decoded( ts::str_c& rslt ) const;

};

struct active_protocol_data_s
{
    ts::str_c tag;
    ts::str_c name; // utf8
    ts::str_c user_name; // utf8
    ts::str_c user_statusmsg; // utf8
    ts::blob_c config;
    ts::blob_c avatar;
    int sort_factor = 0;
    int options = O_AUTOCONNECT;

    configurable_s configurable;


    enum options_e
    {
        O_AUTOCONNECT   = SETBIT( 0 ),
        //O_SUSPENDED     = SETBIT(1),
        O_CONFIG_NATIVE = SETBIT( 2 ),
    };
};

struct sync_data_s
{
    active_protocol_data_s data;
    ts::astrings_c strings;
    ts::flags32_s flags;
    contact_online_state_e manual_cos = COS_ONLINE;
    cmd_result_e current_state;
    int cbits = 0;
    int reconnect_in = 0;

    const ts::str_c &getstr( info_string_e s ) const
    {
        if ( s < strings.size() )
            return strings.get( s );
        return ts::make_dummy<ts::str_c>(true);
    }
};

struct avatar_restrictions_s
{
    int maxsize = 16384;
    int minwh = 0;
    int maxwh = 0;
    ts::flags32_s options;

    static const ts::flags32_s::BITS O_ALLOW_ANIMATED_GIF = 1;
};

class folder_share_send_c;

class active_protocol_c : public ts::safe_object
{
    GM_RECEIVER(active_protocol_c, ISOGM_PROFILE_TABLE_SL);
    GM_RECEIVER(active_protocol_c, ISOGM_MESSAGE);
    GM_RECEIVER(active_protocol_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(active_protocol_c, GM_HEARTBEAT);
    GM_RECEIVER(active_protocol_c, ISOGM_CMD_RESULT);
    GM_RECEIVER(active_protocol_c, GM_UI_EVENT);

    int id;
    int priority = 0;
    int indicator = 0;
    int features = 0;
    int conn_features = 0;

    int countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;

    avatar_restrictions_s arest;
    s3::Format audio_fmt;
    isotoxin_ipc_s * volatile ipcp = nullptr;
    spinlock::syncvar< sync_data_s > syncdata;
    ts::Time lastconfig;
    ts::Time typingtime = ts::Time::past();
    contact_id_s typingsendcontact;

    time_t last_backup_time = 0;

    spinlock::long3264 lbsync = 0;
    ts::pointers_t<data_header_s,0> locked_bufs;

    NUMGEN_START(fff, 0);
#define DECLAREBIT( fn ) static const ts::flags32_s::BITS fn = SETBIT(NUMGEN_NEXT(fff))

    DECLAREBIT(F_DIP);
    DECLAREBIT(F_WORKER);
    DECLAREBIT(F_WORKER_STOP);
    DECLAREBIT(F_WORKER_STOPED);
    DECLAREBIT(F_CONFIG_OK);
    DECLAREBIT(F_CONFIG_FAIL);
    DECLAREBIT(F_SAVE_REQUEST);
    DECLAREBIT(F_CONFIG_UPDATED);
    DECLAREBIT(F_CFGSAVE_CHECKER);
    DECLAREBIT(F_CONFIGURABLE_RCVD);
    DECLAREBIT(F_ONLINE_SWITCH);
    DECLAREBIT(F_SET_PROTO_OK);
    DECLAREBIT(F_CURRENT_ONLINE);
    DECLAREBIT(F_NEED_SEND_CONFIGURABLE); // wait for restore plugin process and send config
    DECLAREBIT(F_CLEAR_ICON_CACHE);

#undef DECLAREBIT

    struct tlm_statistic_s
    {
        uint64 uid = 0;
        uint64 accum = 0;
        uint64 accumps = 0; // per second
        uint64 accumcur = 0;
        ts::Time last_update = ts::Time::past();
        int updatecnt = 0;

        tlm_statistic_s * next = nullptr;

        ~tlm_statistic_s()
        {
            TSDEL( next );
        }
        void newdata( const tlm_data_s *d, bool full = true );
    };

    tlm_statistic_s tlms[ TLM_COUNT ];

#ifdef _DEBUG
    uint64 amsmonotonic = 0;
    uint64 vmsmonotonic = 0;
#endif

    struct icon_s
    {
        const ts::bitmap_c *bmp;
        icon_type_e icot;
    };

    ts::array_inplace_t<icon_s, 1> icons_cache; // FREE MEMORY

    bool cmdhandler(ipcr r);
    bool tick();
    void worker();
    bool check_die(RID, GUIPARAM);
    bool check_save(RID, GUIPARAM);
    void save_config( const ts::blob_c &cfg, bool native_config );
    void run();

    void push_debug_settings();

    void setup_audio_fmt( ts::str_c& s );
    void setup_avatar_restrictions( ts::str_c& s );

    ts::blob_c fit_ava( const ts::blob_c&ava ) const;

public:
    active_protocol_c(int id, const active_protocol_data_s &pd);
    ~active_protocol_c();

    void once_per_5sec_tick();

    void unlock_video_frame( incoming_video_frame_s *f );

    ts::str_c get_actual_username() const;

    const ts::str_c &get_infostr(info_string_e s) const {return syncdata.lock_read()().getstr(s);};
    const ts::str_c &get_name() const {return syncdata.lock_read()().data.name;};
    const ts::str_c &get_tag() const { return syncdata.lock_read()().data.tag; };
    int get_features() const {return features; }
    int get_conn_features() const { return conn_features; }
    int get_priority() const {return priority; }
    int get_indicator_lv() const { return indicator; }
    protocol_description_s proto_desc() const
    {
        protocol_description_s d;
        d.connection_features = conn_features;
        d.features = features;
        d.strings = syncdata.lock_read()().strings;
        return d;
    }

    const ts::str_c &get_uname() const {return syncdata.lock_read()().data.user_name;};
    const ts::str_c &get_ustatusmsg() const {return syncdata.lock_read()().data.user_statusmsg;};

    configurable_s get_configurable() const { return syncdata.lock_read()().data.configurable; };
    void set_configurable( const configurable_s &c, bool force_send = false );
    void send_configurable(const configurable_s &c);

    const s3::Format& defaudio() const {return audio_fmt;}

    const ts::bitmap_c &get_icon(int sz, icon_type_e icot);
    void clear_icon_cache() { syncdata.lock_write()().flags.set(F_CLEAR_ICON_CACHE); }

    bool set_current_online(bool oflg)
    {
        auto w = syncdata.lock_write();
        bool ofv = w().flags.is( F_CURRENT_ONLINE );
        w().flags.init( F_CURRENT_ONLINE, oflg );
        return ofv != oflg;
    }
    bool is_current_online() const { return syncdata.lock_read()().flags.is(F_CURRENT_ONLINE); }
    cmd_result_e get_current_state(int &reconnect_in) const
    {
        auto r = syncdata.lock_read();
        reconnect_in = r().reconnect_in;
        return r().current_state;
    }
    int get_connection_bits() const { return syncdata.lock_read()().cbits; }

    int sort_factor() const { return syncdata.lock_read()().data.sort_factor; }
    void set_sortfactor(int sf);

    ts::blob_c gen_system_user_avatar();
    void set_avatar(contact_c *); // self avatar to self contact
    void set_avatar( const ts::blob_c &ava ); // avatar for this protocol
    void set_ostate(contact_online_state_e _cos);

    void save_config(bool wait);
    void stop_and_die(bool wait_worker_end = false);
    int getid() const {return id;}
    bool is_new() const { return id < 0 && !is_dip(); }
    bool is_dip() const { return syncdata.lock_read()().flags.is(F_DIP); }

    bool is_autoconnect() const { return 0 != (syncdata.lock_read()().data.options & active_protocol_data_s::O_AUTOCONNECT); }
    void set_autoconnect( bool v );

    void del_message( uint64 utag );

    void signal(contact_id_s cid, signal_e s);
    void signal(signal_e s);

    void join_conference(contact_id_s gid, contact_id_s cid);
    void rename_conference(contact_id_s gid, const ts::str_c &confaname);
    void create_conference( const ts::str_c &confaname, const ts::str_c &o );
    void del_conference( const ts::str_c &confa_id );
    void enter_conference( const ts::str_c &confa_id );
    void leave_conference(contact_id_s gid, bool keep_leave );
    void resend_request(contact_id_s cid, const ts::str_c &msg_utf8 );
    void add_contact( const ts::str_c& pub_id, const ts::str_c &msg_utf8 );
    void add_contact( const ts::str_c& pub_id ); // without authorization
    void del_contact(const contact_key_s &ck);
    void send_proto_data(contact_id_s cid, const ts::blob_c &pdata);

    void refresh_details( const contact_key_s &ck );

    void apply_encoding_settings(); // should be called before enabling video or during video call (to change current settings)
    void send_video_frame(contact_id_s cid, const ts::bmpcore_exbody_s &eb, uint64 timestamp );
    void send_audio(contact_id_s cid, const void *data, int size, uint64 timestamp );
    void call(contact_id_s cid, int seconds, bool videocall);
    void set_stream_options(contact_id_s cid, int so, const ts::ivec2 &vr); // tell to proto/other peer about recommended video resolution (if I see video in 320x240, why you send 640x480?)

    void file_accept( uint64 utag, uint64 offset);
    void file_control(uint64 utag, file_control_e fctl);
    void send_file(contact_id_s cid, uint64 utag, const ts::wstr_c &filename, uint64 filesize);
    bool file_portion(uint64 utag, uint64 offset, const void *data, ts::aint sz);

    void send_folder_share_toc(contact_id_s cid, int ver, const folder_share_send_c &fsh);
    void send_folder_share_ctl(uint64 utag, folder_share_control_e ctl);
    void query_folder_share_file(uint64 utag, const ts::asptr &filedn, const ts::asptr &fakefn);

    void typing( const contact_key_s &ck );

    void reset_data();
    void change_data( const ts::blob_c &b, bool is_native );


    void draw_telemtry( rectengine_c&e, const uint64& uid, const ts::irect& r, ts::uint32 tlmmask );
};