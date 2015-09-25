#pragma once

#define BUTTON_FACE_PRELOADED( face ) (GET_BUTTON_FACE) ([]()->button_desc_s * { return g_app->buttons().face; } )

#define HOTKEY_TOGGLE_SEARCH_BAR SSK_F, gui_c::casw_ctrl

struct preloaded_buttons_s
{
    ts::shared_ptr<button_desc_s> icon[ contact_gender_count ];
    ts::shared_ptr<button_desc_s> groupchat;
    ts::shared_ptr<button_desc_s> online, online2;
    ts::shared_ptr<button_desc_s> invite;
    ts::shared_ptr<button_desc_s> unread;
    ts::shared_ptr<button_desc_s> callb;
    ts::shared_ptr<button_desc_s> fileb;

    ts::shared_ptr<button_desc_s> editb;
    ts::shared_ptr<button_desc_s> confirmb;
    ts::shared_ptr<button_desc_s> cancelb;

    ts::shared_ptr<button_desc_s> exploreb;
    ts::shared_ptr<button_desc_s> pauseb;
    ts::shared_ptr<button_desc_s> unpauseb;
    ts::shared_ptr<button_desc_s> breakb;
    
    ts::shared_ptr<button_desc_s> nokeeph;
    
    ts::shared_ptr<button_desc_s> smile;

    void reload();
};

enum autoupdate_beh_e
{
    AUB_NO,
    AUB_ONLY_CHECK,
    AUB_DOWNLOAD,

    AUB_DEFAULT = AUB_NO,
};

struct autoupdate_params_s
{
    ts::str_c ver;
    ts::wstr_c path;
    ts::str_c proxy_addr;
    int proxy_type = 0;
    autoupdate_beh_e autoupdate = AUB_NO;
    bool in_updater = false;
    bool in_progress = false;
    bool downloaded = false;
};

struct file_transfer_s;
struct query_task_s : public ts::task_c
{
    file_transfer_s *ftr;

    struct job_s
    {
        DUMMY(job_s);
        uint64 offset;
        int sz;
        job_s() {}
    };

    struct sync_s
    {
        job_s current_job;
        ts::array_inplace_t<job_s, 1> jobarray;
    };

    spinlock::syncvar<sync_s> sync;

    volatile enum 
    {
        rslt_inprogress,
        rslt_kill,
        rslt_ok,

    } rslt = rslt_inprogress;

    query_task_s(file_transfer_s *ftr):ftr(ftr) {}
    ~query_task_s();
    /*virtual*/ int iterate(int pass) override;
    /*virtual*/ void done(bool canceled) override;
    /*virtual*/ void result() override;
};

struct file_transfer_s : public unfinished_file_transfer_s
{
    MOVABLE(true);

    static const int BPSSV_WAIT_FOR_ACCEPT = -3;
    static const int BPSSV_PAUSED_BY_REMOTE = -2;
    static const int BPSSV_PAUSED_BY_ME = -1;
    static const int BPSSV_ALLOW_CALC = 0;


    query_task_s *query_task = nullptr;

    struct data_s
    {
        uint64 offset = 0;
        uint64 progrez = 0;
        HANDLE handle = nullptr;
        double notfreq = 1.0;
        LARGE_INTEGER prevt;
        int bytes_per_sec = BPSSV_ALLOW_CALC;
        float upduitime = 0;

        struct range_s
        {
            uint64 offset0;
            uint64 offset1;
        };
        ts::tbuf0_t<range_s> transfered;
        void tr(uint64 _offset0, uint64 _offset1);
        uint64 trsz() const
        {
            uint64 sz = 0;
            for (const range_s &r : transfered)
                sz += r.offset1 - r.offset0;
            return sz;
        }

        float deltatime(bool updateprevt, int addseconds = 0)
        {
            LARGE_INTEGER cur;
            QueryPerformanceCounter(&cur);
            float dt = (float)((double)(cur.QuadPart - prevt.QuadPart) * notfreq);
            if (updateprevt)
            {
                prevt = cur;
                if (addseconds)
                    prevt.QuadPart += (int64)((double)addseconds / notfreq);
            }
            return dt;
        }

    };

    spinlock::syncvar<data_s> data;

    HANDLE file_handle() const { return data.lock_read()().handle; }
    uint64 get_offset() const { return data.lock_read()().offset; }

    bool accepted = false; // prepare_fn called - file receive accepted // used only for receive
    bool update_item = false;

    file_transfer_s();
    ~file_transfer_s();

    void auto_confirm();

    int progress(int &bytes_per_sec) const;
    void upd_message_item(bool force);


    void upload_accepted();
    void resume();
    void prepare_fn( const ts::wstr_c &path_with_fn, bool overwrite );
    void kill( file_control_e fctl = FIC_BREAK );
    void save( uint64 offset, const ts::buf0_c&data );
    void query( uint64 offset, int sz );
    void pause_by_remote( bool p );
    void pause_by_me( bool p );
    bool is_active() const 
    {
        auto rdata = data.lock_read();
        return rdata().bytes_per_sec == BPSSV_WAIT_FOR_ACCEPT || (const_cast<data_s *>(&rdata())->deltatime(false)) < 60; /* last activity in 60 sec */
    }
    bool confirm_required() const;
};

class application_c : public gui_c
{
    bool b_customize(RID r, GUIPARAM param);
public:
    /*virtual*/ HICON app_icon(bool for_tray) override;
    /*virtual*/ void app_setup_custom_button(bcreate_s & bcr) override
    {
        if (bcr.tag == ABT_APPCUSTOM)
        {
            bcr.face = BUTTON_FACE(customize);
            bcr.handler = DELEGATE(this, b_customize);
            bcr.tooltip = TOOLTIP(TTT("Settings",2));
        }
    }
    /*virtual*/ bool app_custom_button_state(int tag, int &shiftleft) override
    {
        if (tag == ABT_APPCUSTOM)
            shiftleft += 5;

        return true;
    }

    /*virtual*/ void app_prepare_text_for_copy(ts::str_c &text_utf8) override;

    /*virtual*/ ts::wsptr app_loclabel(loc_label_e ll);

    /*virtual*/ void app_notification_icon_action(naction_e act, RID iconowner) override;
    /*virtual*/ void app_fix_sleep_value(int &sleep_ms) override;
    /*virtual*/ void app_5second_event() override;
    /*virtual*/ void app_loop_event() override;
    /*virtual*/ void app_b_minimize(RID main) override;
    /*virtual*/ void app_b_close(RID main) override;
    /*virtual*/ void app_path_expand_env(ts::wstr_c &path);
    /*virtual*/ void app_active_state(bool is_active);

    ///////////// application_c itself

    RID main;

    unsigned F_INITIALIZATION : 1;
    unsigned F_NEWVERSION : 1;
    unsigned F_NONEWVERSION : 1;
    unsigned F_UNREADICONFLASH : 1;
    unsigned F_UNREADICON : 1;
    unsigned F_NEEDFLASH : 1;
    unsigned F_FLASHIP : 1;
    unsigned F_SETNOTIFYICON : 1; // once
    unsigned F_OFFLINE_ICON : 1;
    unsigned F_ALLOW_AUTOUPDATE : 1;


    SIMPLE_SYSTEM_EVENT_RECEIVER (application_c, SEV_EXIT);
    SIMPLE_SYSTEM_EVENT_RECEIVER (application_c, SEV_INIT);

    GM_RECEIVER( application_c, ISOGM_PROFILE_TABLE_SAVED );
    GM_RECEIVER( application_c, GM_UI_EVENT );
    GM_RECEIVER( application_c, ISOGM_DELIVERED );

    ts::pointers_t<contact_c,0> m_ringing;
    mediasystem_c m_mediasystem;
    ts::task_executor_c m_tasks_executor;

    ts::hashmap_t<int, ts::wstr_c> m_locale;
    ts::hashmap_t<SLANGID, ts::wstr_c> m_locale_lng;
    SLANGID m_locale_tag;

    preloaded_buttons_s m_buttons;

    ts::array_del_t<file_transfer_s, 2> m_files;

    ts::tbuf_t<contact_key_s> m_need_recalc_unread;
    ts::tbuf_t<contact_key_s> m_locked_recalc_unread;
    
    sound_capture_handler_c *m_currentsc = nullptr;
    ts::pointers_t<sound_capture_handler_c, 0> m_scaptures;

    ts::tbuf_t<RID> m_flashredraw;

    struct send_queue_s
    {
        ts::Time last_try_send_time = ts::Time::current();
        contact_key_s receiver; // metacontact
        ts::array_inplace_t<post_s, 1> queue; // sorted by time
    };

    ts::array_del_t<send_queue_s, 1> m_undelivered;

    contact_online_state_e manual_cos = COS_ONLINE;

public:
    bool b_send_message(RID r, GUIPARAM param);
    bool flash_notification_icon(RID r = RID(), GUIPARAM param = nullptr);
    bool flashiconflag() const {return F_UNREADICONFLASH;};
    bool flashingicon() const {return F_UNREADICON;};
    void flashredraw(RID r) { m_flashredraw.set(r); }
public:

    ts::safe_ptr<gui_contact_item_c> active_contact_item;

    const ts::font_desc_c *font_conv_name = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_text = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_time = &ts::g_default_text_font;
    int contactheight = 55;
    int mecontactheight = 60;
    int protowidth = 100;
    int protoiconsize = 10;

    time_t autoupdate_next;
    ts::ivec2 download_progress = ts::ivec2(0);

	application_c( const ts::wchar * cmdl, bool minimize );
	~application_c();

    static ts::str_c get_downloaded_ver();
    bool b_update_ver(RID, GUIPARAM);
    bool b_restart(RID, GUIPARAM);
    bool b_install(RID, GUIPARAM);
    
    void newversion(bool nv) { F_NEWVERSION = nv; };
    void nonewversion(bool nv) { F_NONEWVERSION = nv; };
    bool newversion() const {return F_NEWVERSION;}
    bool nonewversion() const {return F_NONEWVERSION;}

    static ts::str_c appver();
    void set_notification_icon();
    bool is_inactive(bool do_incoming_message_stuff);

    bool load_theme( const ts::wsptr&thn );

    const SLANGID &current_lang() const {return m_locale_tag;};
    void load_locale( const SLANGID& lng );
    const ts::wstr_c &label(int id) const
    {
        const ts::wstr_c *l = m_locale.get(id);
        if (l) return *l;
        return ts::make_dummy<ts::wstr_c>(true);
    }

    void summon_main_rect(bool minimize);
    preloaded_buttons_s &buttons() {return m_buttons;}

    void lock_recalc_unread( const contact_key_s &ck ) { m_locked_recalc_unread.set(ck); };
    void need_recalc_unread( const contact_key_s &ck ) { m_need_recalc_unread.set(ck); };

    void handle_sound_capture( const void *data, int size );
    void register_capture_handler( sound_capture_handler_c *h );
    void unregister_capture_handler( sound_capture_handler_c *h );
    void start_capture(sound_capture_handler_c *h);
    void stop_capture(sound_capture_handler_c *h);
    void capture_device_changed();

    mediasystem_c &mediasystem() {return m_mediasystem;};

    void add_task( ts::task_c *t ) { m_tasks_executor.add(t); }

    void update_ringtone( contact_c *rt, bool play_stop_snd = true );


    template<typename R> void enum_file_transfers_by_historian( const contact_key_s &historian, R r )
    {
        for (file_transfer_s *ftr : m_files)
            if (ftr->historian == historian)
                r(*ftr);
    }
    bool present_file_transfer_by_historian(const contact_key_s &historian, bool accept_only_rquest);
    bool present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest);
    file_transfer_s *find_file_transfer(uint64 utag);
    file_transfer_s *find_file_transfer_by_msgutag(uint64 utag);
    file_transfer_s *register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, const ts::wstr_c &filename, uint64 filesize );
    void unregister_file_transfer(uint64 utag,bool disconnected);
    void cancel_file_transfers( const contact_key_s &historian ); // by historian

    void resend_undelivered_messages( const contact_key_s& rcv = contact_key_s() );
    void undelivered_message( const post_s &p );

    void set_status(contact_online_state_e cos_, bool manual);
};

extern application_c *g_app;

INLINE bool contact_c::achtung() const
{
    if (is_ringtone() || is_filein()) return true;
    if (key.is_self()) return g_app->newversion();
    return false;
}
