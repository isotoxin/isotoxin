#pragma once

#define BUTTON_FACE_PRELOADED( face ) (GET_BUTTON_FACE) ([]()->button_desc_s * { return g_app->preloaded_stuff().face; } )
#define GET_FONT( face ) g_app->preloaded_stuff().face
#define GET_THEME_VALUE( name ) g_app->preloaded_stuff().name

#define HOTKEY_TOGGLE_SEARCH_BAR ts::SSK_F, ts::casw_ctrl
#define HOTKEY_TOGGLE_TAGFILTER_BAR ts::SSK_T, ts::casw_ctrl
#define HOTKEY_TOGGLE_NEW_CONNECTION_BAR ts::SSK_N, ts::casw_ctrl

//-V:preloaded_stuff():807
//-V:GET_FONT:807
//-V:GET_THEME_VALUE:807

struct preloaded_stuff_s
{
    const ts::font_desc_c *font_conv_name = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_text = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_time = &ts::g_default_text_font;
    const ts::font_desc_c *font_msg_edit = &ts::g_default_text_font;
    int contactheight = 55;
    int mecontactheight = 60;
    int minprotowidth = 100;
    int protoiconsize = 10;
    ts::ivec2 achtung_shift = ts::ivec2(0);

    ts::TSCOLOR common_bg_color = 0xffffffff;
    ts::TSCOLOR appname_color = ts::ARGB(0, 50, 0);
    ts::TSCOLOR found_mark_color = ts::ARGB(50, 50, 0);
    ts::TSCOLOR found_mark_bg_color = ts::ARGB(255, 100, 255);
    ts::TSCOLOR achtung_content_color = ts::ARGB(0, 0, 0);
    ts::TSCOLOR state_online_color = ts::ARGB(0, 255, 0);
    ts::TSCOLOR state_away_color = ts::ARGB(255, 255, 0);
    ts::TSCOLOR state_dnd_color = ts::ARGB(255, 0, 0);

    const theme_image_s* icon[contact_gender_count];
    const theme_image_s* conference = nullptr;
    const theme_image_s* nokeeph = nullptr;
    const theme_image_s* achtung_bg = nullptr;
    const theme_image_s* invite_send = nullptr;
    const theme_image_s* invite_recv = nullptr;
    const theme_image_s* invite_rej = nullptr;
    const theme_image_s* online[contact_online_state_check];
    const theme_image_s* offline = nullptr;
    const theme_image_s* online_some = nullptr;
    
    ts::shared_ptr<button_desc_s> callb;
    ts::shared_ptr<button_desc_s> callvb;
    ts::shared_ptr<button_desc_s> fileb;

    ts::shared_ptr<button_desc_s> editb;
    ts::shared_ptr<button_desc_s> confirmb;
    ts::shared_ptr<button_desc_s> cancelb;

    ts::shared_ptr<button_desc_s> exploreb;
    ts::shared_ptr<button_desc_s> pauseb;
    ts::shared_ptr<button_desc_s> unpauseb;
    ts::shared_ptr<button_desc_s> breakb;
    
    
    ts::shared_ptr<button_desc_s> smile;

    void update_fonts();
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
    enum
    {
        NUM_PATHS = 2,
    };

    ts::str_c ver;
    ts::wstr_c paths[NUM_PATHS];
    ts::astrmap_c dbgoptions;
    ts::str_c proxy_addr;
    int proxy_type = 0;
    autoupdate_beh_e autoupdate = AUB_NO;
    bool in_updater = false;
    bool in_progress = false;
    bool downloaded = false;
#ifndef MODE64
    bool disable64 = false;
#endif // MODE64

    void setup_paths();
    bool need_setup_paths() const
    {
        return paths[0].is_empty() || paths[1].is_empty();
    }

    bool in_downloading() const
    {
        return in_progress && autoupdate == AUB_DOWNLOAD;
    }
};

namespace ts
{
    template<> struct is_movable<s3::Format>
    {
        static const bool value = true;
    };
}

uint64 random64();

class application_c;
extern application_c *g_app;

class application_c : public gui_c, public sound_capture_handler_c
{
    typedef gui_c super;
    bool b_customize(RID r, GUIPARAM param);

    ts::tbuf_t<s3::Format> avformats;
    /*virtual*/ bool datahandler(const void *data, int size) override;
    /*virtual*/ const s3::Format *formats( ts::aint &count) override;

public:
    static const ts::flags32_s::BITS PEF_RECREATE_CTLS_CL = PEF_FREEBITSTART << 0;
    static const ts::flags32_s::BITS PEF_RECREATE_CTLS_MSG = PEF_FREEBITSTART << 1;
    static const ts::flags32_s::BITS PEF_UPDATE_BUTTONS_HEAD = PEF_FREEBITSTART << 2;
    static const ts::flags32_s::BITS PEF_UPDATE_BUTTONS_MSG = PEF_FREEBITSTART << 3;
    static const ts::flags32_s::BITS PEF_SHOW_HIDE_EDITOR = PEF_FREEBITSTART << 4;
    static const ts::flags32_s::BITS PEF_UPDATE_PROTOS_HEAD = PEF_FREEBITSTART << 5;
    
    
    static const ts::flags32_s::BITS PEF_APP = (ts::flags32_s::BITS)(-1) & (~(PEF_FREEBITSTART-1));
    
    ts::bitmap_c build_icon( int sz, ts::TSCOLOR colorblack = 0xff000000 );

    /*virtual*/ ts::bitmap_c app_icon(bool for_tray) override;
    /*virtual*/ void app_setup_custom_button(bcreate_s & bcr) override
    {
        if (bcr.tag == CBT_APPCUSTOM)
        {
            bcr.face = BUTTON_FACE(customize);
            bcr.handler = DELEGATE(this, b_customize);
            bcr.tooltip = TOOLTIP(TTT("Settings",2));
        }
    }
    /*virtual*/ bool app_custom_button_state(int tag, int &shiftleft) override
    {
        if (tag == CBT_APPCUSTOM)
            shiftleft += 5;

        return true;
    }

    /*virtual*/ void app_prepare_text_for_copy(ts::str_c &text_utf8) override;

    /*virtual*/ ts::wstr_c app_loclabel(loc_label_e ll) override;

    /*virtual*/ void app_notification_icon_action( ts::notification_icon_action_e act, RID iconowner) override;
    /*virtual*/ void app_fix_sleep_value(int &sleep_ms) override;
    /*virtual*/ void app_5second_event() override;
    /*virtual*/ void app_loop_event() override;
    /*virtual*/ void app_b_minimize(RID main) override;
    /*virtual*/ void app_b_close(RID main) override;
    /*virtual*/ void app_path_expand_env(ts::wstr_c &path) override;
    /*virtual*/ void do_post_effect() override;
    /*virtual*/ void app_font_par(const ts::str_c&, ts::font_params_s&fprm) override;
    /*virtual*/ guirect_c * app_create_shade(const ts::irect &r) override;

    ///////////// application_c itself

    ts::safe_ptr<gui_contactlist_c> contactlist;
    RID main;


    struct uap : public ts::movable_flag<true>
    {
        ts::buf0_c usedids;
        ts::uint16 apid;
    };

    spinlock::syncvar< ts::array_inplace_t< uap, 0 > > uaps; // no need to sync

    void getuap(ts::uint16 apid, ipcw & w);
    void setuap(ts::uint16 apid, ts::aint id);

    void apply_ui_mode( bool split_ui );

    volatile bool F_INDICATOR_CHANGED = false; // whole byte due it modified from non-main thread
    volatile bool F_AUDIO_MUTED = false; // whole byte due it modified from non-main thread

#define APPFLAGS \
    APPF( NEWVERSION, false ) \
    APPF( NONEWVERSION, false ) \
    APPF( UNREADICON, false ) \
    APPF( NEED_BLINK_ICON, false ) \
    APPF( BLINKING_FLAG, false ) \
    APPF( BLINKING_ICON, false ) \
    APPF( SETNOTIFYICON, false ) \
    APPF( OFFLINE_ICON, true ) \
    APPF( ALLOW_AUTOUPDATE, false ) \
    APPF( PROTOSORTCHANGED, true ) \
    APPF( READONLY_MODE, g_commandline().readonlymode ) \
    APPF( READONLY_MODE_WARN, g_commandline().readonlymode ) \
    APPF( MODAL_ENTER_PASSWORD, false ) \
    APPF( TYPING, false ) \
    APPF( CAPTURE_AUDIO_TASK, false ) \
    APPF( CAPTURING, false ) \
    APPF( SHOW_CONTACTS_IDS, false ) \
    APPF( SHOW_SPELLING_WARN, false ) \
    APPF( MAINRECTSUMMON, false ) \
    APPF( SPLIT_UI, false ) \
    APPF( BACKUP_PROFILE, false ) \

    enum {
#define APPF(n,v) FLAG_##n,
        APPFLAGS
#undef APPF
        FLAG_LAST,
    };

    TS_STATIC_CHECK(FLAG_LAST <= 31, "too many flags");

#define APPF(n,v) static const ts::flags32_s::BITS FLAGBIT_##n = F_FREEBITSTART << FLAG_##n;
    APPFLAGS
#undef APPF

#define APPF(n,v) bool F_##n() const {return m_flags.is(FLAGBIT_##n);} void F_##n(bool vv) { m_flags.init(FLAGBIT_##n, vv); }
    APPFLAGS
#undef APPF


    bool on_init();
    bool on_exit();
    bool on_loop();
    void on_mouse( ts::mouse_event_e me, const ts::ivec2 &clpos /* relative to wnd */, const ts::ivec2 &scrpos );
    bool on_char( ts::wchar c );
    bool on_keyboard( int scan, bool dn, int casw );

    bool handle_keyboard( int scan, bool dn, int casw );

    GM_RECEIVER( application_c, ISOGM_PROFILE_TABLE_SL );
    GM_RECEIVER( application_c, ISOGM_CHANGED_SETTINGS );
    GM_RECEIVER( application_c, GM_UI_EVENT );
    GM_RECEIVER( application_c, ISOGM_DELIVERED );
    GM_RECEIVER( application_c, ISOGM_EXPORT_PROTO_DATA );
    GM_RECEIVER( application_c, ISOGM_GRABDESKTOPEVENT );
    
    av_contacts_c m_avcontacts;
    mediasystem_c m_mediasystem;
    ts::task_executor_c m_tasks_executor;

    ts::hashmap_t<int, ts::wstr_c> m_locale;
    ts::hashmap_t<SLANGID, ts::wstr_c> m_locale_lng;
    SLANGID m_locale_tag;

    preloaded_stuff_s m_preloaded_stuff;

    ts::array_del_t<file_transfer_s, 2> m_files;
    ts::array_del_t<folder_share_c, 4> m_foldershares;

    struct blinking_reason_s : public ts::movable_flag<true>
    {
        time_t last_update = ts::now();
        contact_key_s historian;
        ts::Time nextblink = ts::Time::undefined();
        ts::Time acblinking_stop = ts::Time::undefined();
        int unread_count = 0;
        ts::flags32_s flags;
        ts::tbuf_t<uint64> ftags_request, ftags_progress, fshares;

        static const ts::flags32_s::BITS F_BLINKING_FLAG = SETBIT(0);
        static const ts::flags32_s::BITS F_CONTACT_BLINKING = SETBIT(1);
        static const ts::flags32_s::BITS F_REDRAW = SETBIT(2);

        // reasons
        static const ts::flags32_s::BITS F_RINGTONE = SETBIT(3);
        static const ts::flags32_s::BITS F_INVITE_FRIEND = SETBIT(4);
        static const ts::flags32_s::BITS F_RECALC_UNREAD = SETBIT(5);
        static const ts::flags32_s::BITS F_NEW_VERSION = SETBIT(6);
        static const ts::flags32_s::BITS F_CONFERENCE_AUDIO = SETBIT(7);

        void do_recalc_unread_now();
        bool tick();

        bool get_blinking() const {return flags.is(F_BLINKING_FLAG);}

        bool is_blank() const
        {
            if (contacts().find(historian) == nullptr) return true;
            return unread_count == 0 && ftags_request.count() == 0 && ftags_progress.count() == 0 && fshares.count() == 0 && (flags.__bits & ~(F_CONTACT_BLINKING|F_BLINKING_FLAG|F_REDRAW)) == 0;
        }
        bool notification_icon_need_blink() const
        {
            return flags.is(F_RINGTONE | F_INVITE_FRIEND | F_CONFERENCE_AUDIO) || unread_count > 0 || is_file_download_request();
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

        bool is_folder_share_announce() const { return fshares.count() > 0; }
        void folder_share(uint64 utag)
        {
            ts::aint oldc = fshares.count();
            ftags_request.set(utag);
            if (oldc != fshares.count())
                flags.set(F_REDRAW);
        }

        bool is_file_download() const { return is_file_download_request() || is_file_download_process(); }
        bool is_file_download_request() const { return ftags_request.count() > 0; }
        bool is_file_download_process() const { return ftags_progress.count() > 0; }
        void file_download_request_add( uint64 ftag )
        {
            bool dirty = ftags_progress.find_remove_fast(ftag);
            ts::aint oldc = ftags_request.count();
            ftags_request.set(ftag);
            if (dirty || oldc != ftags_request.count())
                flags.set(F_REDRAW);
        }
        void file_download_progress_add( uint64 ftag )
        {
            bool dirty = ftags_request.find_remove_fast(ftag);
            ts::aint oldc = ftags_progress.count();
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
        void folder_share_remove(uint64 utag)
        {
            bool was_f = is_folder_share_announce();
            if (!utag)
                fshares.clear();
            else
                fshares.find_remove_fast(utag);
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

        void conference_audio(bool f = true)
        {
            if (flags.is(F_CONFERENCE_AUDIO) != f)
            {
                flags.init(F_CONFERENCE_AUDIO, f);
                flags.set(F_REDRAW);
            }
            if (f)
                acblinking_stop = ts::Time::current() + 5000;
        }

        void recalc_unread()
        {
            flags.set(F_RECALC_UNREAD);
        }

        void up_unread()
        {
            last_update = ts::now();
            ++unread_count;
            flags.set(F_REDRAW);
            flags.clear( F_RECALC_UNREAD );
            g_app->F_SETNOTIFYICON(true);
        }

        void set_unread(int unread)
        {
            if (unread != unread_count)
            {
                flags.set(F_REDRAW);
                unread_count = unread;
                g_app->F_SETNOTIFYICON(true);
            }
        }

    };

    enum icon_e
    {
        ICON_APP,
        ICON_HOLLOW,
        ICON_OFFLINE,
        ICON_OFFLINE_MSG1,
        ICON_OFFLINE_MSG2,
        ICON_ONLINE,
        ICON_ONLINE_MSG1,
        ICON_ONLINE_MSG2,
        ICON_AWAY,
        ICON_AWAY_MSG1,
        ICON_AWAY_MSG2,
        ICON_DND,
        ICON_DND_MSG1,
        ICON_DND_MSG2,

        icons_count
    };

    ts::bitmap_c icons[ icons_count ];
    int icon_num = -1;

    ts::array_inplace_t<blinking_reason_s,2> m_blink_reasons;
    ts::tbuf_t<contact_key_s> m_locked_recalc_unread;
    
    ts::Time last_capture_accepted = ts::Time::current();
    sound_capture_handler_c *m_currentsc = nullptr;
    ts::pointers_t<sound_capture_handler_c, 0> m_scaptures;

    struct send_queue_s
    {
        ts::Time last_try_send_time = ts::Time::current();
        contact_key_s receiver; // metacontact
        ts::array_inplace_t<post_s, 1> queue; // sorted by time
    };

    ts::array_del_t<send_queue_s, 1> m_undelivered;

    struct reselect_data_s
    {
        contact_key_s hkey;
        ts::Time eventtime = ts::Time::current();
        int options = 0;
    } reselect_data;

    ts::hashmap_t< ts::str_c, avatar_s > m_identicons;

public:
    bool b_send_message(RID r, GUIPARAM param);
    bool flash_notification_icon(RID r = RID(), GUIPARAM param = nullptr);
    bool flashingicon() const {return F_UNREADICON();}
    bool update_state();
public:

    ts::wstr_c folder_share_sel_path;
    ts::wstr_c folder_share_sel_path2;
    ts::wstr_c file_save_as_path;

    ts::shared_ptr<ts::dynamic_allocator_s> global_allocator;

    ts::safe_ptr<gui_contact_item_c> active_contact_item;
    vsb_displays_pool_c video_displays;

    time_t autoupdate_next;
    ts::ivec2 download_progress = ts::ivec2(0);

    found_stuff_s *found_items = nullptr;

    class splchk_c
    {
        mutable spinlock::long3264 sync = 0;
        enum
        {
            EMPTY,
            LOADING,
            READY,
        } state = EMPTY;
        void *busy = nullptr;

        enum
        {
            AU_NOTHING,
            AU_RELOAD,
            AU_UNLOAD,
            AU_DIE,
        } after_unlock = AU_NOTHING;

        ts::array_del_t< Hunspell, 0 > spellcheckers;
    public:

        enum lock_rslt_e
        {
            LOCK_OK,
            LOCK_BUSY,
            LOCK_EMPTY,
            LOCK_DIE,
        };

        bool is_enabled() const
        {
            SIMPLELOCK(sync);
            return state != EMPTY && AU_UNLOAD != after_unlock && AU_DIE != after_unlock;
        }

        void load();
        void unload();
        void spell_check_work_done();
        void set_spellcheckers(ts::array_del_t< Hunspell, 0 > &&sa);
        void check(ts::astrings_c &&checkwords, spellchecker_s *rsltrcvr);
        lock_rslt_e lock( void *prm );
        bool unlock( void *prm );
        bool check_one( const ts::str_c &w, ts::astrings_c &suggestions );

        bool is_locked( bool set_dip )
        {
            SIMPLELOCK(sync);
            if (set_dip) after_unlock = AU_DIE;
            return busy != nullptr;
        }

    } spellchecker;

    void get_local_spelling_files(ts::wstrings_c &names);
    void resetup_spelling();

	application_c( const ts::wchar * cmdl );
	~application_c();

    const avatar_s * gen_identicon_avatar( const ts::str_c &pubid );

    void apply_debug_options();

    static ts::str_c get_downloaded_ver();
    bool b_update_ver(RID, GUIPARAM);
    bool b_restart(RID, GUIPARAM);
    bool b_install(RID, GUIPARAM);
    
    static ts::str_c appver();
    static int appbuild();
    void set_notification_icon();
    bool is_inactive(bool do_incoming_message_stuff, const contact_key_s &ck);

    void reload_fonts();
    bool load_theme( const ts::wsptr&thn, bool summon_ch_signal = true);
    ts::bitmap_c &prepareimageplace(const ts::wsptr &name)
    {
        return get_theme().prepareimageplace(name);
    }
    void clearimageplace(const ts::wsptr &name);

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
    void recheck_no_profile();
    preloaded_stuff_s &preloaded_stuff() {return m_preloaded_stuff;}

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
    int count_unread_blink_reason() const
    {
        int cnt = 0;
        for ( const blinking_reason_s &br : m_blink_reasons )
            if ( br.unread_count > 0 )
                cnt += br.unread_count;
        return cnt;
    }
    bool reselect_p( RID, GUIPARAM );
    void reselect( contact_root_c *historian, int options, double delay = 0.0 );
    void bring2front( contact_root_c *historian );

    void handle_sound_capture( const void *data, int size );
    void register_capture_handler( sound_capture_handler_c *h );
    void unregister_capture_handler( sound_capture_handler_c *h );
    void start_capture(sound_capture_handler_c *h);
    void stop_capture(sound_capture_handler_c *h);
    void capture_device_changed();
    void check_capture();
    bool capturing() const
    {
        if (F_CAPTURE_AUDIO_TASK()) return false;
        return F_CAPTURING();
    }

    mediasystem_c &mediasystem() {return m_mediasystem;};
    av_contacts_c &avcontacts() { return m_avcontacts; };

    folder_share_c * add_folder_share(const contact_key_s &k, const ts::str_c &name, folder_share_s::fstype_e t, uint64 utag);
    folder_share_c * add_folder_share(const contact_key_s &k, const ts::str_c &name, folder_share_s::fstype_e t, uint64 utag, const ts::wstr_c &path);
    void remove_folder_share(folder_share_c *sfc);
    folder_share_c *find_folder_share_by_utag(uint64 utag);
    bool folder_share_recv_announce_present() const;

    void add_task( ts::task_c *t ) { m_tasks_executor.add(t); }
    ts::uint32 base_tid() const { return  m_tasks_executor.base_tid(); }

    void update_ringtone( contact_root_c *rt, contact_c *sub, bool play_stop_snd = true );
    av_contact_s * update_av( contact_root_c *avmc, contact_c *sub, bool activate, bool camera = false );

    template<typename R> void enum_file_transfers_by_historian( const contact_key_s &historian, R r )
    {
        for (file_transfer_s *ftr : m_files)
            if (ftr->historian == historian)
                r(*ftr);
    }
    bool present_file_transfer_by_historian(const contact_key_s &historian);
    bool present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest);
    file_transfer_s *find_file_transfer( uint64 utag );
    file_transfer_s *find_file_transfer_by_iutag(uint64 i_utag);
    file_transfer_s *find_file_transfer_by_msgutag(uint64 utag);
    file_transfer_s *find_file_transfer_by_fshutag(uint64 utag);
    file_transfer_s *register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, ts::wstr_c filename, uint64 filesize );
    file_transfer_s *register_file_hidden_send(const contact_key_s &historian, const contact_key_s &sender, ts::wstr_c filename, ts::str_c fakename); // not saved to db as unfinished
    void unregister_file_transfer(uint64 utag,bool disconnected);
    void cancel_file_transfers( const contact_key_s &historian ); // by historian

    bool present_undelivered_messages( const contact_key_s& rcv, uint64 except_utag ) const;
    void reset_undelivered_resend_cooldown( const contact_key_s& rcv );
    void resend_undelivered_messages( const contact_key_s& rcv = contact_key_s() );
    void undelivered_message( const contact_key_s &historian_key, const post_s &p );
    void kill_undelivered( uint64 utag );

    void set_status(contact_online_state_e cos_, bool manual);

    void recreate_ctls(bool cl, bool m)
    {
        if (cl) m_post_effect.set(PEF_RECREATE_CTLS_CL); 
        if (m) m_post_effect.set(PEF_RECREATE_CTLS_MSG);
    };

    void update_buttons_head() { m_post_effect.set(PEF_UPDATE_BUTTONS_HEAD); };
    void update_buttons_msg() { m_post_effect.set(PEF_UPDATE_BUTTONS_MSG); };
    void hide_show_messageeditor() { m_post_effect.set(PEF_SHOW_HIDE_EDITOR); };
    void update_protos_head() { m_post_effect.set(PEF_UPDATE_PROTOS_HEAD); };

};

