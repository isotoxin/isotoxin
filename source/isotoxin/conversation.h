#pragma once

enum notice_e
{
    NOTICE_NETWORK,
    NOTICE_NEWVERSION,
    NOTICE_FRIEND_REQUEST_RECV,
    NOTICE_FRIEND_REQUEST_SEND_OR_REJECT,
    NOTICE_INCOMING_CALL,
    NOTICE_CALL_INPROGRESS,
    NOTICE_CALL,
    NOTICE_FILE,
    NOTICE_GROUP_CHAT,
    NOTICE_PREV_NEXT,
    
    NOTICE_WARN_NODICTS,

    // special
    NOTICE_KILL_CALL_INPROGRESS,
};

class gui_notice_c;
template<> struct gmsg<ISOGM_NOTICE> : public gmsgbase
{
    gmsg(contact_root_c *owner, contact_c *sender, notice_e nid, const ts::str_c& text) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid), text(text) {}
    gmsg(contact_root_c *owner, contact_c *sender, notice_e nid) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid) {}
    contact_root_c *owner;
    contact_c *sender;
    notice_e n;
    ts::str_c text; // utf8
    uint64 utag = 0; // file utag
    gui_notice_c *just_created = nullptr;
};

template<> struct gmsg<ISOGM_NOTICE_PRESENT> : public gmsgbase
{
    gmsg(contact_root_c *owner, contact_c *sender, notice_e nid) :gmsgbase(ISOGM_NOTICE_PRESENT), owner(owner), sender(sender), n(nid) {}
    contact_root_c *owner;
    contact_c *sender;
    notice_e n;
};

template<> struct MAKE_CHILD<gui_notice_c> : public _PCHILD(gui_notice_c)
{
    notice_e n;
    MAKE_CHILD(RID parent_, notice_e n) :n(n) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_notice_network_c;
template<> struct MAKE_CHILD<gui_notice_network_c> : public _PCHILD(gui_notice_network_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_notice_callinprogress_c;
template<> struct MAKE_CHILD<gui_notice_callinprogress_c> : public _PCHILD(gui_notice_callinprogress_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_notice_c : public gui_label_ex_c
{
    DUMMY(gui_notice_c);

    bool close_reject_notice( RID, GUIPARAM );

protected:
    ts::shared_ptr<contact_root_c> historian;
    ts::shared_ptr<contact_c> sender;
    notice_e notice;
    uint64 utag = 0;
    int addheight = 0;

    GM_RECEIVER(gui_notice_c, ISOGM_NOTICE);
    GM_RECEIVER(gui_notice_c, ISOGM_NOTICE_PRESENT);
    GM_RECEIVER(gui_notice_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_notice_c, ISOGM_FILE);
    GM_RECEIVER(gui_notice_c, ISOGM_DOWNLOADPROGRESS);
    GM_RECEIVER(gui_notice_c, ISOGM_NEWVERSION);
    GM_RECEIVER(gui_notice_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_notice_c, GM_UI_EVENT);

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS F_FREEBITSTART_NOTICE = FLAGS_FREEBITSTART_LABEL << 1;

    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    /*virtual*/ int get_height_by_width(int width) const override;

    void update_text(int dtimesec = 0);

    bool b_turn_off_spelling(RID, GUIPARAM);

public:
    gui_notice_c() {}
    gui_notice_c(MAKE_CHILD<gui_notice_c> &data);
    gui_notice_c(MAKE_CHILD<gui_notice_network_c> &data);
    gui_notice_c(MAKE_CHILD<gui_notice_callinprogress_c> &data);
    /*virtual*/ ~gui_notice_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    notice_e get_notice() const { return notice; }
    uint64 get_utag() const {return utag;}

    void setup(const ts::str_c &itext_utf8, contact_c *sender, uint64 utag);
    void setup(const ts::str_c &itext_utf8);
    void setup(contact_c *sender);
    bool setup_tail(RID r = RID(), GUIPARAM p = nullptr);

};

struct av_contact_s;
class gui_notice_callinprogress_c : public gui_notice_c
{
    friend struct common_videocall_stuff_s;
    friend class fullscreenvideo_c;

    GM_RECEIVER(gui_notice_callinprogress_c, ISOGM_VIDEO_TICK);
    GM_RECEIVER(gui_notice_callinprogress_c, ISOGM_CAMERA_TICK);
    GM_RECEIVER(gui_notice_callinprogress_c, ISOGM_PEER_STREAM_OPTIONS);
    GM_RECEIVER(gui_notice_callinprogress_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_notice_callinprogress_c, ISOGM_GRABDESKTOPEVENT );

    time_t showntime = 0;
    ts::safe_ptr< fullscreenvideo_c > fsvideo;
    ts::ivec2 last_video_size;
    common_videocall_stuff_s common;
    vsb_list_t video_devices;
    UNIQUE_PTR( vsb_c ) camera;

    ts::irect vrect_cache;

    void set_height(int addh);

    bool show_video_tick(RID, GUIPARAM p);
    bool wait_animation(RID, GUIPARAM p);
    bool b_extra(RID, GUIPARAM p);
    bool b_camera_switch(RID, GUIPARAM p);
    bool b_fs(RID, GUIPARAM p);

    void menu_video(const ts::str_c &p);

    void set_corresponding_height();
    void video_off();

    bool recalc_vsz(RID, GUIPARAM)
    {
        recalc_video_size(last_video_size);
        return true;
    }

    const ts::irect *vrect();

    static const ts::flags32_s::BITS F_WAITANIM = F_FREEBITSTART_NOTICE << 0;
    static const ts::flags32_s::BITS F_VIDEO_SHOW = F_FREEBITSTART_NOTICE << 1;
    
    void recalc_video_size(const ts::ivec2 &videosize);
    void acquire_display();

    bool is_cam_init_anim() const { return common.flags.is(common.F_CAMINITANIM); }
    void set_cam_init_anim()
    {
        common.flags.set(common.F_CAMINITANIM);
        if (fsvideo)
            fsvideo->set_cam_init_anim();
    }
    
    void clear_cam_init_anim()
    {
        common.flags.clear(common.F_CAMINITANIM);
        if (fsvideo)
            fsvideo->clear_cam_init_anim();
    }

    void calc_cam_display_rects();

    void on_fsvideo_die();

public:
    gui_notice_callinprogress_c(MAKE_CHILD<gui_notice_callinprogress_c> &data);
    /*virtual*/ ~gui_notice_callinprogress_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void setup(contact_c *collocutor);

    void setcam( vsb_c *cam ) { camera.reset(cam); }

    void set_video_devices(vsb_list_t &&_video_devices)
    {
        video_devices = std::move(_video_devices);
    }

    contact_c *collocutor() { return sender; }
    av_contact_s *get_avc();
    vsb_c *getcamera() { return camera.get();  }
};



namespace rbtn
{
    enum
    {
        EB_NAME,
        EB_STATUS,
        EB_NNAME,

        EB_MAX
    };
    struct ebutton_s
    {
        RID brid;
        ts::ivec2 p;
        bool updated = false;
    };
};


class gui_notice_network_c : public gui_notice_c
{
    GM_RECEIVER(gui_notice_network_c, ISOGM_PROFILE_TABLE_SAVED);
    GM_RECEIVER(gui_notice_network_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(gui_notice_network_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_notice_network_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_notice_network_c, ISOGM_CMD_RESULT);
    
    enum color_e
    {
        COLOR_ONLINE_STATUS = 0,
        COLOR_OFFLINE_STATUS = 1,
        COLOR_OVERAVATAR = 2,
        COLOR_DEFAULT_VALUE = 3,
        COLOR_ERROR_STATUS = 4,
    };
    
    ts::str_c pubid;
    int clicklink = -1;
    int flashing = 0;
    int networkid = 0;
    system_query_e clicka = SQ_NOP;
    cmd_result_e curstate = CR_OK;
    bool refresh = false;
    bool is_autoconnect = false;

    bool resetup(RID, GUIPARAM);
    bool flash_pereflash(RID, GUIPARAM);

    static const ts::flags32_s::BITS  F_OVERAVATAR  = F_FREEBITSTART_NOTICE << 0; // mouse cursor above avatar

    FREE_BIT_START_CHECK( F_FREEBITSTART_NOTICE, 1 );

    void moveup(const ts::str_c&);
    void movedn(const ts::str_c&);
    void show_link_submenu();
    void ctx_onlink_do(const ts::str_c &cc);

public:
    void flash();

    int sortfactor() const;

    gui_notice_network_c(MAKE_CHILD<gui_notice_network_c> &data):gui_notice_c(data) { notice = NOTICE_NETWORK; }
    /*virtual*/ ~gui_notice_network_c();

    /*virtual*/ void get_link_prolog(ts::wstr_c &r, int linknum) const override;
    /*virtual*/ void get_link_epilog(ts::wstr_c &r, int linknum) const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ int get_height_by_width(int width) const override;

    void setup(const ts::str_c &pubid); // network

};

class gui_noticelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_noticelist_c);
    GM_RECEIVER(gui_noticelist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_noticelist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_noticelist_c, ISOGM_NOTICE);
    GM_RECEIVER(gui_noticelist_c, GM_UI_EVENT);
    GM_RECEIVER(gui_noticelist_c, ISOGM_CHANGED_SETTINGS);

    ts::shared_ptr<contact_root_c> owner;

    void clear_list(bool hide = true);
    gui_notice_c &create_notice(notice_e n);
    bool resort_children(RID, GUIPARAM);
    void kill_notice( notice_e n );

public:
    gui_noticelist_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) {}
    /*virtual*/ ~gui_noticelist_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_NORMAL_MAX;}
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void children_repos() override;

    contact_c *get_owner() {return owner;}

    void refresh();
};


class gui_message_item_c;
template<> struct MAKE_CHILD<gui_message_item_c> : public _PCHILD(gui_message_item_c)
{
    contact_c *author;
    contact_root_c *historian;
    ts::str_c skin;
    message_type_app_e mt;
    MAKE_CHILD(RID parent_, contact_root_c *historian, contact_c *author, const ts::str_c &skin, message_type_app_e mt):historian(historian), author(author), skin(skin), mt(mt) { parent = parent_; }
    ~MAKE_CHILD();
};

class image_loader_c;

class gui_message_item_c : public gui_label_ex_c
{
    DUMMY(gui_message_item_c);
    message_type_app_e mt;
    int height = 0;
    ts::uint16 addheight = 0;
    ts::uint16 timestrwidth = 0;

    static const int BTN_EXPLORE = 0;
    static const int BTN_BREAK = 1;
    static const int BTN_PAUSE = 2;
    static const int BTN_UNPAUSE = 3;
    static const int RECT_IMAGE = 1000;
    static const int TYPING_SPACERECT = 1001;
    static const int CTL_PROGRESSBAR = 1002;

    struct addition_data_s
    {
        virtual ~addition_data_s() {}
    };
    struct addition_file_data_s : public addition_data_s
    {
        struct btn_s
        {
            MOVABLE( true );
            RID rid;
            int r = -1;
            bool used = false;
        };
        ts::array_inplace_t<btn_s, 1>  btns;
#if defined(MODE64)
        ts::uninitialized<80 /* sizeof(image_loader_c) */ > imgloader;
#else
        ts::uninitialized<44 /* sizeof(image_loader_c) */ > imgloader;
#endif
        ts::shared_ptr<theme_rect_s> shadow;

        ts::safe_ptr<gui_hslider_c> pbar;
        ts::wstr_c rectt, pbtext;
        float progress = 0;
        int rwidth = 0, clw = -1;
        ts::ivec2 pbsize = ts::ivec2(0);
        const ts::wstr_c &getrectt(gui_message_item_c *mi);

        /*virtual*/ ~addition_file_data_s();
    };

    struct addition_prevnext_s : public addition_data_s
    {
        uint64 prev;
        uint64 next;
        ts::aint offset;
    };


    UNIQUE_PTR( addition_data_s ) addition;

    bool imgloader() const
    {
        if (subtype != ST_RECV_FILE && subtype != ST_SEND_FILE)
            return false;
        if (addition_file_data_s *afd = ts::ptr_cast<addition_file_data_s *>(addition.get()))
            return afd->imgloader;
        return false;
    }

    image_loader_c &imgloader_get(addition_file_data_s **ftb);

    addition_file_data_s &addition_file_data()
    {
        ASSERT(ST_RECV_FILE == subtype || ST_SEND_FILE == subtype);

        if (!addition)
            addition.reset(TSNEW(addition_file_data_s));
        return *ts::ptr_cast<addition_file_data_s *>(addition.get());
    }

    struct record
    {
        DUMMY(record);
        record() {}

        uint64 utag = 0;
        time_t rcv_time = 0;
        time_t cr_time = 0;
        ts::str_c text; // utf8
        ts::TSCOLOR undelivered = 0;
        ts::uint16 append( ts::wstr_c &t, ts::wstr_c &pret, const ts::wsptr &postt = ts::wsptr() );
        bool operator()( const record&r ) const
        {
            return rcv_time < r.rcv_time;
        }
    } zero_rec;

#if JOIN_MESSAGES
    ts::array_inplace_t<record, 2> other_records;
#endif
    void rebuild_text();
    void mark_found();

    ts::shared_ptr<contact_root_c> historian;
    ts::shared_ptr<contact_c> author;
    mutable ts::str_c protodesc;
    ts::wstr_c timestr;

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE   = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS F_NO_AUTHOR            = FLAGS_FREEBITSTART_LABEL << 1;
    static const ts::flags32_s::BITS F_OVERIMAGE            = FLAGS_FREEBITSTART_LABEL << 2; // mouse cursor above image
    static const ts::flags32_s::BITS F_OVERIMAGE_LBDN       = FLAGS_FREEBITSTART_LABEL << 3;
    static const ts::flags32_s::BITS F_FOUND_ITEM           = FLAGS_FREEBITSTART_LABEL << 4;
    static const ts::flags32_s::BITS F_PASSIVE              = FLAGS_FREEBITSTART_LABEL << 5;

    static const int m_left = 10; // recta width and this value should be same
    static const int m_top = 3;
    
    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    
    void ctx_menu_copy(const ts::str_c &)
    {
        gui->simulate_kbd( ts::SSK_C, ts::casw_ctrl);
    }
    void ctx_menu_golink(const ts::str_c &);
    void ctx_menu_copylink(const ts::str_c &);
    void ctx_menu_qrcode(const ts::str_c &);
    void ctx_menu_copymessage(const ts::str_c &);
    void ctx_menu_delmessage(const ts::str_c &);
    
    

    bool try_select_link(RID r = RID(), GUIPARAM p = nullptr);

    void prepare_str_prefix( ts::wstr_c &pret, ts::wstr_c &postt )
    {
        pret.set(CONSTWSTR("<p><r> <color=#"));
        ts::TSCOLOR c = get_default_text_color(2);
        pret.append_as_hex(ts::RED(c))
            .append_as_hex(ts::GREEN(c))
            .append_as_hex(ts::BLUE(c))
            .append_as_hex(ts::ALPHA(c))
            .append(CONSTWSTR(">"));
        postt.set(CONSTWSTR("</color></r>"));
    }

    bool message_prefix(ts::wstr_c &newtext, time_t posttime);
    void message_postfix(ts::wstr_c &newtext);

    ts::wstr_c prepare_button_rect(int r, const ts::ivec2 &sz);
    void kill_button( rectengine_c *beng, int r );
    void repl_button( int r_from, int r_to );
    void updrect(const void *, int r, const ts::ivec2 &p);
    void updrect_emoticons(const void *, int r, const ts::ivec2 &p);
    
    /*virtual*/ bool custom_tag_parser(ts::wstr_c& r, const ts::wsptr &tv) const override;

    bool b_explore(RID, GUIPARAM);
    bool b_break(RID, GUIPARAM);
    bool b_pause_unpause(RID, GUIPARAM);

    bool some_selected() const
    {
        return gui->selcore().owner == (const gui_label_c *)this && gui->selcore().some_selected();
    }

    ts::pwstr_c get_message_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const;
    ts::ivec2 get_message_pos_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const;
    ts::ivec2 extract_message(int chari, uint64 &mutag) const;

    virtual void get_link_prolog(ts::wstr_c & r, int linknum) const;
    virtual void get_link_epilog(ts::wstr_c & r, int linknum) const;

    bool kill_self(RID, GUIPARAM);
    bool animate_typing(RID, GUIPARAM);


public:

    enum
    {
        ST_JUST_TEXT,
        ST_RECV_FILE,
        ST_SEND_FILE,
        ST_TYPING,
        ST_CONVERSATION,
    } subtype = ST_JUST_TEXT;

    void dirty_height_cache() { flags.set(F_DIRTY_HEIGHT_CACHE); }

    gui_message_item_c(MAKE_CHILD<gui_message_item_c> &data) :gui_label_ex_c(data), mt(data.mt), author(data.author), historian(data.historian) { set_theme_rect( CONSTASTR("message.") + data.skin, false );}
    /*virtual*/ ~gui_message_item_c();

    int calc_height_by_width(int width);
    /*virtual*/ int get_height_by_width(int width) const override
    {
        return const_cast<gui_message_item_c *>(this)->calc_height_by_width(width);
    }
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void set_passive() { flags.set(F_PASSIVE); };

    uint64 get_prev_found() const
    {
        ASSERT( flags.is(F_FOUND_ITEM) && addition );
        if (addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>( addition.get() ))
            return pn->prev;
        return 0;
    }
    uint64 get_next_found() const
    {
        ASSERT(flags.is(F_FOUND_ITEM) && addition);
        if (addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()))
            return pn->next;
        return 0;
    }
    /*virtual*/ void update_offset( ts::aint offset) override
    {
        if (flags.is(F_FOUND_ITEM))
            if (addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()))
                pn->offset = offset;
    }
    ts::aint get_offset() const
    {
        ASSERT(flags.is(F_FOUND_ITEM) && addition);
        if (const addition_prevnext_s *pn = ts::ptr_cast<const addition_prevnext_s *>(addition.get()))
            return pn->offset;
        return 0;
    }

    bool found_item() const { return flags.is(F_FOUND_ITEM); }

    ts::wstr_c hdr() const;
    contact_c * get_author() const {return author;}
    message_type_app_e get_mt() const {return mt;}

    time_t get_first_post_recv_time() const {return zero_rec.rcv_time;}
    time_t get_first_post_cr_time() const {return zero_rec.cr_time;}
#if JOIN_MESSAGES
    time_t get_last_post_time() const {return other_records.size() ? other_records.last().time : 0;}
#endif

    void update_text(int for_width = 0);
    void set_no_author( bool f = true ) { bool ona = flags.is(F_NO_AUTHOR); flags.init(F_NO_AUTHOR, f); if (ona != f) flags.set(F_DIRTY_HEIGHT_CACHE); }

    void setup_found_item( uint64 prev, uint64 next );
    void append_text( const post_s &post, bool resize_now = true, bool force_new = false );
    bool delivered(uint64 utag);
    bool with_utag(uint64 utag) const;
    bool remove_utag(uint64 utag);
    uint64 zero_utag() const {return zero_rec.utag;}

    bool is_history_load_button() const {return MTA_SPECIAL == mt;}
    bool is_date_separator() const {return MTA_DATE_SEPARATOR == mt;}
    void init_date_separator( const tm &tmtm );
    void init_request( const ts::str_c &message_utf8 );
    void init_load( ts::aint n_load );

    void refresh_typing();
};

class gui_messagelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_messagelist_c);
    
    GM_RECEIVER(gui_messagelist_c, ISOGM_SUMMON_NOPROFILE_UI);
    GM_RECEIVER(gui_messagelist_c, ISOGM_DO_POSTEFFECT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_MESSAGE);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SUMMON_POST);
    GM_RECEIVER(gui_messagelist_c, ISOGM_DELIVERED);
    GM_RECEIVER(gui_messagelist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_messagelist_c, ISOGM_TYPING);
    GM_RECEIVER(gui_messagelist_c, GM_DROPFILES);
    GM_RECEIVER(gui_messagelist_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_REFRESH_SEARCH_RESULT );
    GM_RECEIVER(gui_messagelist_c, ISOGM_CHANGED_SETTINGS );
    GM_RECEIVER(gui_messagelist_c, GM_UI_EVENT );
    
    static const ts::flags32_s::BITS F_SEARCH_RESULTS_HERE = F_VSCROLLFREEBITSTART << 0;
    static const ts::flags32_s::BITS F_EMPTY_MODE          = F_VSCROLLFREEBITSTART << 1;
    static const ts::flags32_s::BITS F_IN_REPOS            = F_VSCROLLFREEBITSTART << 2;
    static const ts::flags32_s::BITS F_SCROLLING_TO_TGT    = F_VSCROLLFREEBITSTART << 3;

    struct filler_s
    {
        uint64 scrollto = 0;
        gui_messagelist_c *owner;
        rectengine_c *scroll_to = nullptr;
        const found_item_s *found_item;
        ts::aint fillindex_up = -1;
        ts::aint fillindex_down = -1;
        ts::aint fillindex_down_end = -1;
        ts::aint load_n = 0;
        int numpertick = 20;
        int options = 0;
        bool dont_scroll = false;
        filler_s(gui_messagelist_c *owner, ts::aint n, ts::aint loadn):owner(owner), found_item(nullptr), fillindex_up(n-1), fillindex_down(0), fillindex_down_end(0), options(RSEL_INSERT_NEW), load_n(loadn) { tick(); }
        filler_s(gui_messagelist_c *owner, const found_item_s *found_item, uint64 scrollto, int options, ts::aint loadn);
        ~filler_s();
        bool tick(RID r = RID(), GUIPARAM p = nullptr);

        void die()
        {
            owner->filler.reset();
        }

    };
    UNIQUE_PTR( filler_s ) filler;


    time_t last_seen_post_time = 0;
    tm last_post_time;
    ts::shared_ptr<contact_root_c> historian;
    ts::array_safe_t< gui_message_item_c, 1 > typing;

    void clear_list(bool empty_mode);
    gui_message_item_c &insert_message_item(message_type_app_e mt, contact_c *author, const ts::str_c &skin, time_t post_time = 0);
    gui_message_item_c &get_message_item(message_type_app_e mt, contact_c *author, const ts::str_c &skin, time_t post_time = 0, uint64 replace_post = 0);
    bool insert_date_separator( ts::aint index, tm &prev_post_time, time_t next_post_time );

    void create_empty_mode_stuff();
    void repos_empty_mode_stuff();

    ts::safe_ptr<gui_button_c> btn_prev;
    ts::safe_ptr<gui_button_c> btn_next;
    ts::smart_int target_offset;
    int prevdelta = 0;

    /*virtual*/ void on_manual_scroll() override;

    bool font_size_up(RID, GUIPARAM);
    bool font_size_down(RID, GUIPARAM);

    bool scroll_do(RID, GUIPARAM);
    void scroll( ts::aint shift);
    bool pageup(RID, GUIPARAM);
    bool pagedown(RID, GUIPARAM);
    bool totop(RID, GUIPARAM);
    bool tobottom(RID, GUIPARAM);
    bool lineup(RID, GUIPARAM);
    bool linedown(RID, GUIPARAM);

public:
    gui_messagelist_c(initial_rect_data_s &data);
    /*virtual*/ ~gui_messagelist_c();


    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    /*virtual*/ void children_repos_info(cri_s &info) const override;
    /*virtual*/ void children_repos() override;

    void find_prev_next(uint64 *prevutag, uint64 *nextutag);
    void goto_item(uint64 utag);
    bool b_prev(RID, GUIPARAM);
    bool b_next(RID, GUIPARAM);
    void set_prev_next_buttons(gui_button_c &_prev, gui_button_c &_next)
    {
        btn_prev = &_prev;
        btn_next = &_next;
    }

    void update_last_seen_message_time(time_t t)
    {
        if (t >= last_seen_post_time)
            last_seen_post_time = t + 1; // refresh time of items we see
    }
};


struct spellchecker_s
{
    struct chk_word_s
    {
        MOVABLE( false );
        ts::str_c utf8;
        ts::wstr_c badword; // empty, if valid
        ts::astrings_c suggestions;
        bool checked = false;
        bool check_started = false;
        bool present = true;
    };

    ts::array_inplace_t<chk_word_s, 64> words;
    ts::buf0_c badwords;

    void check_text(const ts::wsptr &t, int caret);
    void undo_check(const ts::astrings_c &words);
    void check_result(const ts::str_c &w, bool is_valid, ts::astrings_c &&suggestions);
    bool update_bad_words(RID r = RID(), GUIPARAM p = nullptr);

    ~spellchecker_s();

    DECLARE_EYELET(spellchecker_s);
};

class gui_message_editor_c : public gui_textedit_c, public spellchecker_s
{
    DUMMY(gui_message_editor_c);

    GM_RECEIVER(gui_message_editor_c, ISOGM_SEND_MESSAGE);
    GM_RECEIVER(gui_message_editor_c, ISOGM_MESSAGE);
    GM_RECEIVER(gui_message_editor_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_message_editor_c, GM_UI_EVENT);
    GM_RECEIVER(gui_message_editor_c, ISOGM_CHANGED_SETTINGS);
    
    
    ts::shared_ptr<contact_root_c> historian;
    struct editstate_s
    {
        ts::wstr_c text;
        int cp;
    };
    ts::hashmap_t<contact_key_s, editstate_s> messages;

    ts::safe_ptr<leech_dock_bottom_right_s> smile_pos_corrector;
    ts::ivec2 rb;

    bool show_smile_selector(RID, GUIPARAM);
    bool clear_text(RID, GUIPARAM);
    bool on_enter_press_func(RID, GUIPARAM);
    /*virtual*/ void cb_scrollbar_width(int w) override;

    const ts::buf0_c *bad_words_handler();
    void suggestions(menu_c &m, ts::aint index);
    void suggestions_apply(const ts::str_c &prm);

    /*virtual*/ void paste_( int cp) override;
    /*virtual*/ void new_text( int caret_char_pos ) override;

public:
    gui_message_editor_c(initial_rect_data_s &data) :gui_textedit_c(data) {}
    /*virtual*/ ~gui_message_editor_c();
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    contact_root_c *get_historian() { return historian; }
};

class gui_message_area_c : public gui_group_c
{
    DUMMY(gui_message_area_c);

    static const ts::flags32_s::BITS F_INITIALIZED = FLAGS_FREEBITSTART << 0;
    
    bool change_text_handler(const ts::wstr_c &);

protected:
    /*virtual*/ void children_repos() override;
public:

    ts::safe_ptr<gui_message_editor_c> message_editor;
    ts::safe_ptr<gui_button_c> send_button;
    ts::safe_ptr<gui_button_c> file_button;

    gui_message_area_c(initial_rect_data_s &data) :gui_group_c(data) {}
    /*virtual*/ ~gui_message_area_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_KEEP;}
    /*virtual*/ void created() override;

    void send_file_item(const ts::str_c& prm);
    bool send_file(RID, GUIPARAM);

    void update_buttons();
};


class gui_conversation_c;
template<> struct MAKE_CHILD<gui_conversation_c> : public _PCHILD(gui_conversation_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_conversation_c : public gui_vgroup_c
{
    DUMMY(gui_conversation_c);

    GM_RECEIVER(gui_conversation_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_conversation_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_conversation_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(gui_conversation_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_conversation_c, ISOGM_PROFILE_TABLE_SAVED);
    GM_RECEIVER(gui_conversation_c, ISOGM_DO_POSTEFFECT);
    GM_RECEIVER(gui_conversation_c, GM_DRAGNDROP );


    ts::safe_ptr<gui_contact_item_c> caption;
    ts::safe_ptr<gui_messagelist_c> msglist;
    ts::safe_ptr<gui_noticelist_c> noticelist;
    ts::safe_ptr<gui_message_area_c> messagearea;

    static const ts::flags32_s::BITS F_ALWAYS_SHOW_EDITOR   = F_HGROUP_FREEBITSTART << 0;
    static const ts::flags32_s::BITS F_DNDTARGET            = F_HGROUP_FREEBITSTART << 1;

    RID message_editor;

    bool hide_show_messageeditor(RID r = RID(), GUIPARAM p = nullptr);

public:
    gui_conversation_c(initial_rect_data_s &data) :gui_vgroup_c(data) { /*defaultthrdraw = DTHRO_BASE;*/ }
    /*virtual*/ ~gui_conversation_c();

    const gui_messagelist_c &get_msglist() const {return *msglist;}
    gui_messagelist_c &get_msglist() { return *msglist; }
    void always_show_editor(bool f = true) { flags.init(F_ALWAYS_SHOW_EDITOR, f); hide_show_messageeditor(); }

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    contact_root_c *get_selected_contact() { ASSERT(!caption.expired()); return caption->contacted() ? &caption->getcontact() : nullptr; }
};
