#pragma once

#define BUTTON_FACE_PRELOADED( face ) (GET_BUTTON_FACE) ([]()->button_desc_s * { return g_app->buttons().face; } )

struct preloaded_buttons_s
{
    ts::shared_ptr<button_desc_s> icon[ contact_gender_count ];
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

struct file_transfer_s
{
    MOVABLE(true);

    contact_key_s historian;
    contact_key_s sender;
    uint64 utag = 0;
    uint64 offset = 0;
    uint64 progrez = 0;
    uint64 filesize = 0;
    uint64 msgitem_utag = 0;
    HANDLE handle = nullptr;
    ts::Time trtime = ts::Time::past();
    int bytes_per_sec = 0;
    ts::wstr_c filename;
    bool send = false;

    ~file_transfer_s();

    int progress(int &bytes_per_sec) const;
    void upd_message_item();

    void prepare_fn( const ts::wstr_c &path_with_fn, bool overwrite );
    void kill( file_control_e fctl = FIC_BREAK );
    void save( uint64 offset, const ts::buf0_c&data );
    void query( uint64 offset, int sz );
    void pause_by_remote( bool p );
    void pause_by_me( bool p );
    bool is_active() const { return bytes_per_sec == -3 || (ts::Time::current() - trtime) < 60000; /* last activity in 60 sec */ }
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
            bcr.tooltip = TOOLTIP(TTT("Настройки", 2));
        }
    }
    /*virtual*/ bool app_custom_button_state(int tag, int &shiftleft) override
    {
        if (tag == ABT_APPCUSTOM)
            shiftleft += 5;

        return true;
    }

    /*virtual*/ void app_prepare_text_for_copy(ts::wstr_c &text) override;

    /*virtual*/ ts::wsptr app_loclabel(loc_label_e ll);

    /*virtual*/ void app_notification_icon_action(naction_e act, RID iconowner) override;
    /*virtual*/ void app_fix_sleep_value(int &sleep_ms) override;
    /*virtual*/ void app_5second_event() override;
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

    SIMPLE_SYSTEM_EVENT_RECEIVER (application_c, SEV_EXIT);
    SIMPLE_SYSTEM_EVENT_RECEIVER (application_c, SEV_INIT);

    GM_RECEIVER( application_c, ISOGM_PROFILE_TABLE_SAVED );

    ts::pointers_t<contact_c,0> m_ringing;
    mediasystem_c m_mediasystem;

    ts::hashmap_t<int, ts::wstr_c> m_locale;
    ts::hashmap_t<SLANGID, ts::wstr_c> m_locale_lng;
    SLANGID m_locale_tag;

    preloaded_buttons_s m_buttons;

    ts::array_inplace_t<file_transfer_s, 2> m_files;

    ts::tbuf_t<contact_key_s> m_need_recalc_unread;
    ts::tbuf_t<contact_key_s> m_locked_recalc_unread;
    
    sound_capture_handler_c *m_currentsc = nullptr;
    ts::pointers_t<sound_capture_handler_c, 0> m_scaptures;

    ts::tbuf_t<RID> m_flashredraw;

public:
    bool b_send_message(RID r, GUIPARAM param);
    bool flash_notification_icon(RID r = RID(), GUIPARAM param = nullptr);
    bool flashiconflag() const {return F_UNREADICONFLASH;};
    bool flashingicon() const {return F_UNREADICON;};
    void flashredraw(RID r) { m_flashredraw.set(r); }
public:


    const ts::font_desc_c *font_conv_name = &ts::g_default_text_font;
    const ts::font_desc_c *font_conv_text = &ts::g_default_text_font;

    time_t autoupdate_next;

	application_c( const ts::wchar * cmdl );
	~application_c();

    static ts::str_c get_downloaded_ver();
    bool b_update_ver(RID, GUIPARAM);
    bool b_restart(RID, GUIPARAM);
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

    void summon_main_rect();
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

    void update_ringtone( contact_c *rt, bool play_stop_snd = true );


    template<typename R> void enum_file_transfers_by_historian( const contact_key_s &historian, R r )
    {
        for (file_transfer_s &ftr : m_files)
            if (ftr.historian == historian)
                r(ftr);
    }
    bool present_file_transfer_by_historian(const contact_key_s &historian, bool accept_only_rquest);
    bool present_file_transfer_by_sender(const contact_key_s &sender, bool accept_only_rquest);
    file_transfer_s *find_file_transfer(uint64 utag);
    file_transfer_s *find_file_transfer_by_msgutag(uint64 utag);
    bool register_file_transfer( const contact_key_s &historian, const contact_key_s &sender, uint64 utag, const ts::wstr_c &filename, uint64 filesize );
    void unregister_file_transfer(uint64 utag);
};

extern application_c *g_app;

INLINE bool contact_c::achtung() const
{
    if (is_ringtone() || is_filein()) return true;
    if (key.is_self()) return g_app->newversion();
    return false;
}
