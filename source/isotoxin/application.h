#pragma once

#define IDI_ICON_APP IDI_ICON_ONLINE

#define BUTTON_FACE_PRELOADED( face ) (GET_BUTTON_FACE) ([]()->button_desc_s * { return g_app->buttons().face; } )

#define HOTKEY_TOGGLE_SEARCH_BAR SSK_F, gui_c::casw_ctrl
#define HOTKEY_TOGGLE_NEW_CONNECTION_BAR SSK_N, gui_c::casw_ctrl

struct preloaded_buttons_s
{
    ts::shared_ptr<button_desc_s> icon[ contact_gender_count ];
    ts::shared_ptr<button_desc_s> groupchat;
    ts::shared_ptr<button_desc_s> online, online2;
    ts::shared_ptr<button_desc_s> invite;
    ts::shared_ptr<button_desc_s> achtung;
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
    struct job_s
    {
        DUMMY(job_s);
        uint64 offset = 0xFFFFFFFFFFFFFFFFull;
        int sz = 0;
        job_s() {}
    };

    enum rslt_e
    {
        rslt_inprogress,
        rslt_idle,
        rslt_kill,
        rslt_ok,
    };

    struct sync_s
    {
        file_transfer_s *ftr = nullptr;
        job_s current_job;
        ts::array_inplace_t<job_s, 1> jobarray;
        rslt_e rslt = rslt_inprogress;
    };

    spinlock::syncvar<sync_s> sync;

    query_task_s(file_transfer_s *ftr);
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
        //uint64 offset = 0;
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
    //uint64 get_offset() const { return data.lock_read()().offset; }

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
    static const ts::flags32_s::BITS PEF_RECREATE_CTLS = PEF_FREEBITSTART << 0;
    static const ts::flags32_s::BITS PEF_UPDATE_BUTTONS_HEAD = PEF_FREEBITSTART << 1;
    static const ts::flags32_s::BITS PEF_UPDATE_BUTTONS_MSG = PEF_FREEBITSTART << 2;
    static const ts::flags32_s::BITS PEF_SHOW_HIDE_EDITOR = PEF_FREEBITSTART << 3;
    
    
    static const ts::flags32_s::BITS PEF_APP = (ts::flags32_s::BITS)(-1) & (~(PEF_FREEBITSTART-1));
    

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
    /*virtual*/ void app_path_expand_env(ts::wstr_c &path) override;
    /*virtual*/ void do_post_effect() override;
    /*virtual*/ void app_font_par(const ts::str_c&, ts::font_params_s&fprm) override;



    ///////////// application_c itself

    RID main;

    unsigned F_INITIALIZATION : 1;
    unsigned F_NEWVERSION : 1;
    unsigned F_NONEWVERSION : 1;
    unsigned F_UNREADICON : 1;
    unsigned F_NEED_BLINK_ICON : 1;
    unsigned F_BLINKING_FLAG : 1;
    unsigned F_BLINKING_ICON : 1;
    unsigned F_SETNOTIFYICON : 1; // once
    unsigned F_OFFLINE_ICON : 1;
    unsigned F_ALLOW_AUTOUPDATE : 1;
    unsigned F_PROTOSORTCHANGED : 1;
    unsigned F_READONLY_MODE : 1;
    unsigned F_READONLY_MODE_WARN : 1;


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

    struct blinking_reason_s
    {
        time_t last_update = now();
        contact_key_s historian;
        ts::Time nextblink = ts::Time::undefined();
        int unread_count = 0;
        ts::flags32_s flags;
        ts::tbuf_t<uint64> ftags_request, ftags_progress;

        static const ts::flags32_s::BITS F_BLINKING_FLAG = SETBIT(0);
        static const ts::flags32_s::BITS F_CONTACT_BLINKING = SETBIT(1);
        static const ts::flags32_s::BITS F_RINGTONE = SETBIT(2);
        static const ts::flags32_s::BITS F_INVITE_FRIEND = SETBIT(3);
        static const ts::flags32_s::BITS F_RECALC_UNREAD = SETBIT(4);
        static const ts::flags32_s::BITS F_NEW_VERSION = SETBIT(5);
        static const ts::flags32_s::BITS F_REDRAW = SETBIT(6);

        void do_recalc_unread_now();
        bool tick();

        bool get_blinking() const {return flags.is(F_BLINKING_FLAG);}

        bool is_blank() const
        {
            if (contacts().find(historian) == nullptr) return true;
            return unread_count == 0 && ftags_request.count() == 0 && ftags_progress.count() == 0 && (flags.__bits & ~(F_CONTACT_BLINKING|F_BLINKING_FLAG)) == 0;
        }
        bool notification_icon_need_blink() const
        {
            return flags.is(F_RINGTONE | F_INVITE_FRIEND) || unread_count > 0 || is_file_download_request();
        }
        bool contact_need_blink() const
        {
            return notification_icon_need_blink() || is_file_download_process();
        }

        void ringtone(bool f = true)
        {
            if (flags.is(F_RINGTONE) != f)
            {
                flags.init(F_RINGTONE, f);
                flags.set(F_REDRAW);
            }
        }
        void friend_invite(bool f = true)
        {
            if (flags.is(F_INVITE_FRIEND) != f)
            {
                flags.init(F_INVITE_FRIEND, f);
                flags.set(F_REDRAW);
            }
        }
        bool is_file_download() const { return is_file_download_request() || is_file_download_process(); }
        bool is_file_download_request() const { return ftags_request.count() > 0; }
        bool is_file_download_process() const { return ftags_progress.count() > 0; }
        void file_download_request_add( uint64 ftag )
        {
            bool dirty = ftags_progress.find_remove_fast(ftag);
            int oldc = ftags_request.count();
            ftags_request.set(ftag);
            if (dirty || oldc != ftags_request.count())
                flags.set(F_REDRAW);
        }
        void file_download_progress_add( uint64 ftag )
        {
            bool dirty = ftags_request.find_remove_fast(ftag);
            int oldc = ftags_progress.count();
            ftags_progress.set(ftag);
            if (dirty || oldc != ftags_progress.count())
                flags.set(F_REDRAW);
        }
        void file_download_remove( uint64 ftag )
        {
            bool was_f = is_file_download();
            if (!ftag)
                ftags_request.clear(), ftags_progress.clear();
            else
                ftags_request.find_remove_fast(ftag), ftags_progress.find_remove_fast(ftag);
            if (was_f)
                flags.set(F_REDRAW);
        }
        void new_version(bool f = true)
        {
            if (flags.is(F_NEW_VERSION) != f)
            {
                flags.init(F_NEW_VERSION, f);
                flags.set(F_REDRAW);
            }
        }

        void recalc_unread()
        {
            flags.set(F_RECALC_UNREAD);
        }

        void up_unread()
        {
            last_update = now();
            ++unread_count;
            flags.set(F_REDRAW);
            g_app->F_SETNOTIFYICON = true;
        }

        void set_unread(int unread)
        {
            if (unread != unread_count)
            {
                flags.set(F_REDRAW);
                unread_count = unread;
                g_app->F_SETNOTIFYICON = true;
            }
        }

    };

    ts::array_inplace_t<blinking_reason_s,2> m_blink_reasons;
    ts::tbuf_t<contact_key_s> m_locked_recalc_unread;
    
    sound_capture_handler_c *m_currentsc = nullptr;
    ts::pointers_t<sound_capture_handler_c, 0> m_scaptures;

    struct send_queue_s
    {
        ts::Time last_try_send_time = ts::Time::current();
        contact_key_s receiver; // metacontact
        ts::array_inplace_t<post_s, 1> queue; // sorted by time
    };

    ts::array_del_t<send_queue_s, 1> m_undelivered;

    void update_fonts();

public:
    bool b_send_message(RID r, GUIPARAM param);
    bool flash_notification_icon(RID r = RID(), GUIPARAM param = nullptr);
    bool flashingicon() const {return F_UNREADICON;};
public:

    ts::safe_ptr<gui_contact_item_c> active_contact_item;

    const ts::font_desc_c *font_conv_name = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_text = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_time = &ts::g_default_text_font;
    int contactheight = 55;
    int mecontactheight = 60;
    int minprotowidth = 100;
    int protoiconsize = 10;

    ts::TSCOLOR found_mark_color = ts::ARGB( 50, 50, 0 );
    ts::TSCOLOR found_mark_bg_color = ts::ARGB( 255, 100, 255 );


    time_t autoupdate_next;
    ts::ivec2 download_progress = ts::ivec2(0);

    found_stuff_s *found_items = nullptr;

	application_c( const ts::wchar * cmdl );
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

    void reload_fonts();
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
    void load_profile_and_summon_main_rect(bool minimize);
    preloaded_buttons_s &buttons() {return m_buttons;}

    void lock_recalc_unread( const contact_key_s &ck ) { m_locked_recalc_unread.set(ck); };
    blinking_reason_s &new_blink_reason(const contact_key_s &historian);
    void update_blink_reason(const contact_key_s &historian);
    blinking_reason_s *find_blink_reason(const contact_key_s &historian, bool skip_locked)
    { 
        for (blinking_reason_s &br : m_blink_reasons)
            if (br.historian == historian)
            {
                if (skip_locked && m_locked_recalc_unread.find_index(historian) >= 0)
                    return nullptr;
                return &br;
            }
        return nullptr;
    }
    bool present_unread_blink_reason() const
    {
        for (const blinking_reason_s &br : m_blink_reasons)
            if (br.unread_count > 0)
                return true;
        return false;
    }
    void select_last_unread_contact();


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
    bool present_file_transfer_by_historian(const contact_key_s &historian);
    bool present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest);
    file_transfer_s *find_file_transfer(uint64 utag);
    file_transfer_s *find_file_transfer_by_msgutag(uint64 utag);
    file_transfer_s *register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, const ts::wstr_c &filename, uint64 filesize );
    void unregister_file_transfer(uint64 utag,bool disconnected);
    void cancel_file_transfers( const contact_key_s &historian ); // by historian

    void resend_undelivered_messages( const contact_key_s& rcv = contact_key_s() );
    void undelivered_message( const post_s &p );
    void kill_undelivered( uint64 utag );

    void set_status(contact_online_state_e cos_, bool manual);

    void recreate_ctls() { m_post_effect.set(PEF_RECREATE_CTLS); };
    void update_buttons_head() { m_post_effect.set(PEF_UPDATE_BUTTONS_HEAD); };
    void update_buttons_msg() { m_post_effect.set(PEF_UPDATE_BUTTONS_MSG); };
    void hide_show_messageeditor() { m_post_effect.set(PEF_SHOW_HIDE_EDITOR); };

};

extern application_c *g_app;

