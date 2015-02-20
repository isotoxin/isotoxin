#pragma once

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

struct active_protocol_data_s
{
    ts::str_c tag;
    ts::wstr_c name;
    ts::wstr_c user_name;
    ts::wstr_c user_statusmsg;
    ts::blob_c config;
    int options = O_AUTOCONNECT;
    proxy_settings_s proxy;

    enum options_e
    {
        O_AUTOCONNECT   = SETBIT(0),
        O_SUSPENDED     = SETBIT(1),
    };

};

struct sync_data_s
{
    active_protocol_data_s data;
    ts::wstr_c description;
    ts::flags32_s flags;
};

class active_protocol_c : public ts::safe_object
{
    GM_RECEIVER( active_protocol_c, ISOGM_PROFILE_TABLE_SAVED );
    GM_RECEIVER( active_protocol_c, ISOGM_MESSAGE);
    GM_RECEIVER( active_protocol_c, ISOGM_CHANGED_PROFILEPARAM);
    GM_RECEIVER( active_protocol_c, GM_HEARTBEAT);
    GM_RECEIVER( active_protocol_c, ISOGM_CMD_RESULT);
    
    int id;
    int priority = 0;
    int features = 0;
    /*int proxy_support = 0;*/
    int max_friend_request_bytes = 0;
    s3::Format audio_fmt;
    isotoxin_ipc_s *ipcp = nullptr;
    spinlock::syncvar< sync_data_s > syncdata;
    ts::Time lastconfig;

    fmt_converter_s cvt;

    static const ts::flags32_s::BITS F_DIP                  = SETBIT(0);
    static const ts::flags32_s::BITS F_WORKER               = SETBIT(1);
    static const ts::flags32_s::BITS F_CONFIG_OK            = SETBIT(2);
    static const ts::flags32_s::BITS F_CONFIG_FAIL          = SETBIT(3);
    static const ts::flags32_s::BITS F_SAVE_REQUEST         = SETBIT(4);
    static const ts::flags32_s::BITS F_DIRTY_PROXY_SETTINGS = SETBIT(5);
    static const ts::flags32_s::BITS F_PROXY_SETTINGS_RCVD  = SETBIT(6);
    

    bool cmdhandler(ipcr r);
    bool tick();
    static DWORD WINAPI worker(LPVOID);
    void worker();
#if defined _DEBUG || defined _CRASH_HANDLER
    void worker_check();
#endif
    bool check_die(RID, GUIPARAM);
    bool check_save(RID, GUIPARAM);
    void run();
public:
    active_protocol_c(int id, const active_protocol_data_s &pd);
    ~active_protocol_c();

    const ts::wstr_c &get_desc() const {return syncdata.lock_read()().description;};
    const ts::wstr_c &get_name() const {return syncdata.lock_read()().data.name;};
    int get_features() const {return features; }
    int get_priority() const {return priority; }
    
    proxy_settings_s get_proxy_settings() const { return syncdata.lock_read()().data.proxy; };
    void set_proxy_settings( const proxy_settings_s &ps );

    const s3::Format& defaudio() const {return audio_fmt;}

    void save_config(bool wait);
    void stop_and_die(bool wait_worker_end = false);
    int getid() const {return id;}
    bool is_new() const { return id < 0 && !is_dip(); }
    bool is_dip() const { return syncdata.lock_read()().flags.is(F_DIP); }

    void resend_request( int cid, const ts::wstr_c &msg );
    void add_contact( const ts::str_c& pub_id, const ts::wstr_c &msg );
    void del_contact(int cid);
    void accept(int cid);
    void reject(int cid);

    void accept_call(int cid);
    void send_audio(int cid, const s3::Format &fmt, const void *data, int size);
    void call(int cid, int seconds);
    void stop_call(int cid, stop_call_e sc);


    void file_control(uint64 utag, file_control_e fctl);
    void send_file(int cid, uint64 utag, const ts::wstr_c &filename, uint64 filesize);
    void file_portion(uint64 utag, uint64 offset, const void *data, int sz);
};