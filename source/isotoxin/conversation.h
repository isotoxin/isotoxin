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
};

class gui_notice_c;
template<> struct gmsg<ISOGM_NOTICE> : public gmsgbase
{
    gmsg(contact_c *owner, contact_c *sender, notice_e nid, const ts::str_c& text) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid), text(text) {}
    gmsg(contact_c *owner, contact_c *sender, notice_e nid) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid) {}
    contact_c *owner;
    contact_c *sender;
    notice_e n;
    ts::str_c text; // utf8
    uint64 utag = 0; // file utag
    gui_notice_c *just_created = nullptr;
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

class gui_notice_c : public gui_label_c
{
    DUMMY(gui_notice_c);

protected:
    notice_e notice;
    uint64 utag = 0;
    int height = 0;
    int addheight = 0;

    GM_RECEIVER(gui_notice_c, ISOGM_NOTICE);
    GM_RECEIVER(gui_notice_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_notice_c, ISOGM_FILE);
    GM_RECEIVER(gui_notice_c, ISOGM_DOWNLOADPROGRESS);
    GM_RECEIVER(gui_notice_c, ISOGM_V_UPDATE_CONTACT);

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE = FLAGS_FREEBITSTART_LABEL << 0;

    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    /*virtual*/ int get_height_by_width(int width) const override;

    void update_text(contact_c *sender);

public:
    gui_notice_c() {}
    gui_notice_c(MAKE_CHILD<gui_notice_c> &data);
    gui_notice_c(MAKE_CHILD<gui_notice_network_c> &data);
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
    GM_RECEIVER(gui_notice_network_c, ISOGM_CHANGED_PROFILEPARAM);
    GM_RECEIVER(gui_notice_network_c, GM_HEARTBEAT);
    
    ts::str_c pubid;
    int flashing = 0;
    int networkid = 0;
    int left_margin = 0;

    bool resetup(RID, GUIPARAM);
    bool flash_pereflash(RID, GUIPARAM);

    static const ts::flags32_s::BITS  F_OVERAVATAR = FLAGS_FREEBITSTART_LABEL << 0; // mouse cursor above avatar

public:
    void flash();

    gui_notice_network_c(MAKE_CHILD<gui_notice_network_c> &data):gui_notice_c(data) {}
    /*virtual*/ ~gui_notice_network_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ int gui_notice_network_c::get_height_by_width(int width) const override;

    void setup(const ts::str_c &pubid); // network

};

class gui_noticelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_noticelist_c);
    GM_RECEIVER(gui_noticelist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_noticelist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_noticelist_c, ISOGM_NOTICE);
    GM_RECEIVER(gui_noticelist_c, GM_UI_EVENT);

    ts::shared_ptr<contact_c> owner;

    void clear_list(bool hide = true);
    gui_notice_c &create_notice(notice_e n);

public:
    gui_noticelist_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) {}
    /*virtual*/ ~gui_noticelist_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_NORMAL_MAX;}
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    contact_c *get_owner() {return owner;}

    void refresh();
};


class gui_message_item_c;
template<> struct MAKE_CHILD<gui_message_item_c> : public _PCHILD(gui_message_item_c)
{
    contact_c *author;
    contact_c *historian;
    ts::str_c skin;
    message_type_app_e mt;
    MAKE_CHILD(RID parent_, contact_c *historian, contact_c *author, const ts::str_c &skin, message_type_app_e mt):historian(historian), author(author), skin(skin), mt(mt) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_message_item_c : public gui_label_ex_c
{
    DUMMY(gui_message_item_c);
    message_type_app_e mt;
    int height = 0;
    ts::uint16 addheight = 0;
    ts::uint16 timestrwidth = 0;

    struct record
    {
        DUMMY(record);
        record() {}

        uint64 utag;
        time_t time;
        ts::str_c text; // utf8
        ts::TSCOLOR undelivered = 0;
        ts::uint16 append( ts::wstr_c &t, ts::wstr_c &pret, const ts::wsptr &postt = ts::wsptr() );
        bool operator()( const record&r ) const
        {
            return time < r.time;
        }
    };
    ts::array_inplace_t<record, 2> records;
    void rebuild_text();

    ts::shared_ptr<contact_c> historian;
    ts::shared_ptr<contact_c> author;
    mutable ts::str_c protodesc;
    ts::wstr_c timestr;

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE   = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS F_NO_AUTHOR            = FLAGS_FREEBITSTART_LABEL << 1;

    static const int m_left = 10;
    static const int m_top = 3;
    
    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    
    void ctx_menu_copy(const ts::str_c &)
    {
        gmsg<GM_COPY_HOTKEY>().send();
    }
    void ctx_menu_golink(const ts::str_c &);
    void ctx_menu_copylink(const ts::str_c &);
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
    void updrect(void *, int r, const ts::ivec2 &p);
    void updrect_emoticons(void *, int r, const ts::ivec2 &p);
    
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

public:

    enum
    {
        ST_JUST_TEXT,
        ST_RECV_FILE,
        ST_SEND_FILE,
        ST_CONVERSATION,
    } subtype = ST_JUST_TEXT;

    void dirty_height_cache() { flags.set(F_DIRTY_HEIGHT_CACHE); }

    gui_message_item_c(MAKE_CHILD<gui_message_item_c> &data) :gui_label_ex_c(data), mt(data.mt), author(data.author), historian(data.historian) { set_theme_rect( CONSTASTR("message.") + data.skin, false );}
    /*virtual*/ ~gui_message_item_c();

    /*virtual*/ int get_height_by_width(int width) const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    ts::wstr_c hdr() const;
    contact_c * get_author() const {return author;}
    message_type_app_e get_mt() const {return mt;}

    time_t get_first_post_time() const {return records.size() ? records.get(0).time : 0;}
    time_t get_last_post_time() const {return records.size() ? records.last().time : 0;}

    void update_text();
    void set_no_author( bool f = true ) { bool ona = flags.is(F_NO_AUTHOR); flags.init(F_NO_AUTHOR, f); if (ona != f) flags.set(F_DIRTY_HEIGHT_CACHE); }

    void append_text( const post_s &post, bool resize_now = true );
    bool delivered(uint64 utag);
    bool with_utag(uint64 utag) const;
    bool remove_utag(uint64 utag);
    
    void init_date_separator( const tm &tmtm );
    void init_request( const ts::str_c &message_utf8 );
    void init_load( int n_load );
};

class gui_messagelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_messagelist_c);
    GM_RECEIVER(gui_messagelist_c, ISOGM_MESSAGE);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SUMMON_POST);
    GM_RECEIVER(gui_messagelist_c, ISOGM_DELIVERED);
    GM_RECEIVER(gui_messagelist_c, GM_DROPFILES);
    
    SIMPLE_SYSTEM_EVENT_RECEIVER(gui_messagelist_c, SEV_ACTIVE_STATE);

    //static const ts::flags32_s::BITS F_OVER_NOKEEPH = F_VSCROLLFREEBITSTART << 0;

    tm last_post_time;
    ts::shared_ptr<contact_c> historian;

    void clear_list();
    gui_message_item_c &get_message_item(message_type_app_e mt, contact_c *author, const ts::str_c &skin, time_t post_time = 0, uint64 replace_post = 0);

public:
    gui_messagelist_c(initial_rect_data_s &data);
    /*virtual*/ ~gui_messagelist_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};

class gui_message_editor_c : public gui_textedit_c
{
    DUMMY(gui_message_editor_c);

    GM_RECEIVER(gui_message_editor_c, ISOGM_SEND_MESSAGE);
    GM_RECEIVER(gui_message_editor_c, ISOGM_MESSAGE);
    GM_RECEIVER(gui_message_editor_c, ISOGM_SELECT_CONTACT);
    
    ts::shared_ptr<contact_c> historian;
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

public:
    gui_message_editor_c(initial_rect_data_s &data) :gui_textedit_c(data) {}
    /*virtual*/ ~gui_message_editor_c();
    /*virtual*/ void created() override;
    contact_c *get_historian() { return historian; }
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

    bool send_file(RID, GUIPARAM);

    void update_buttons();
};


class gui_conversation_c;
template<> struct MAKE_CHILD<gui_conversation_c> : public _PCHILD(gui_conversation_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_conversation_c : public gui_vgroup_c, public sound_capture_handler_c
{
    DUMMY(gui_conversation_c);

    GM_RECEIVER(gui_conversation_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_conversation_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_conversation_c, ISOGM_CHANGED_PROFILEPARAM);
    GM_RECEIVER(gui_conversation_c, ISOGM_AV);
    GM_RECEIVER(gui_conversation_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_conversation_c, ISOGM_PROFILE_TABLE_SAVED);
    GM_RECEIVER(gui_conversation_c, ISOGM_UPDATE_BUTTONS);
    
    ts::tbuf_t<s3::Format> avformats;

    ts::safe_ptr<gui_contact_item_c> caption;
    ts::safe_ptr<gui_messagelist_c> msglist;
    ts::safe_ptr<gui_noticelist_c> noticelist;
    ts::safe_ptr<gui_message_area_c> messagearea;

    static const ts::flags32_s::BITS F_ALWAYS_SHOW_EDITOR = F_HGROUP_FREEBITSTART << 0;

    RID message_editor;

    /*virtual*/ void datahandler(const void *data, int size) override;
    /*virtual*/ s3::Format *formats(int &count) override;

    bool hide_show_messageeditor(RID r = RID(), GUIPARAM p = nullptr);

public:
    gui_conversation_c(initial_rect_data_s &data) :gui_vgroup_c(data) { /*defaultthrdraw = DTHRO_BASE;*/ }
    /*virtual*/ ~gui_conversation_c();

    RID get_msglist() const { return msglist->getrid(); }
    void always_show_editor(bool f = true) { flags.init(F_ALWAYS_SHOW_EDITOR, f); hide_show_messageeditor(); }

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    contact_c *get_other() { ASSERT(!caption.expired()); return caption->contacted() ? &caption->getcontact() : nullptr; }
};
