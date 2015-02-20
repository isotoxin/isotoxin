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
};

class gui_notice_c;
template<> struct gmsg<ISOGM_NOTICE> : public gmsgbase
{
    gmsg(contact_c *owner, contact_c *sender, notice_e nid, const ts::wstr_c& text) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid), text(text) {}
    gmsg(contact_c *owner, contact_c *sender, notice_e nid) :gmsgbase(ISOGM_NOTICE), owner(owner), sender(sender), n(nid) {}
    contact_c *owner;
    contact_c *sender;
    notice_e n;
    ts::wstr_c text;
    uint64 utag = 0; // file utag
    gui_notice_c *just_created = nullptr;
};

template<> struct MAKE_CHILD<gui_notice_c> : public _PCHILD(gui_notice_c)
{
    notice_e n;
    MAKE_CHILD(RID parent_, notice_e n) :n(n) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_notice_c : public gui_label_c
{
    DUMMY(gui_notice_c);
    notice_e notice;
    uint64 utag = 0;
    int height = 0;
    int addheight = 0;
    int flashing = 0;
    int networkid = 0; // only for NOTICE_NETWORK

    GM_RECEIVER(gui_notice_c, ISOGM_NOTICE);
    GM_RECEIVER(gui_notice_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_notice_c, ISOGM_FILE);
    GM_RECEIVER(gui_notice_c, ISOGM_PROFILE_TABLE_SAVED);

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE = FLAGS_FREEBITSTART_LABEL << 0;

    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    /*virtual*/ int get_height_by_width(int width) const override;

    void update_text(contact_c *sender);

    void setup_tail();

    bool flash_pereflash(RID, GUIPARAM);

public:

    gui_notice_c(MAKE_CHILD<gui_notice_c> &data);
    /*virtual*/ ~gui_notice_c();

    void flash();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    notice_e get_notice() const { return notice; }
    uint64 get_utag() const {return utag;}

    void setup(const ts::wstr_c &name, const ts::str_c &pubid); // network
    void setup(const ts::wstr_c &itext, contact_c *sender, uint64 utag);
    void setup(const ts::wstr_c &itext);
    void setup(contact_c *sender);
};

class gui_noticelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_noticelist_c);
    GM_RECEIVER(gui_noticelist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_noticelist_c, ISOGM_UPDATE_CONTACT_V);
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

class gui_message_item_c : public gui_label_c
{
    DUMMY(gui_message_item_c);
    message_type_app_e mt;
    int height = 0;
    int addheight = 0;
    struct record
    {
        DUMMY(record);
        record() {}

        uint64 utag;
        time_t time;
        //int index = 0;
        ts::wstr_c text;
        ts::TSCOLOR undelivered = 0;
        void append( ts::wstr_c &t, const ts::wsptr &pret, const ts::wsptr &postt );
        bool operator()( const record&r ) const
        {
            return time < r.time;
        }
    };
    ts::array_inplace_t<record, 2> records;
    void rebuild_text()
    {
        ts::wstr_c pret, postt, newtext;
        prepare_str_prefix(pret, postt);
        for(record &r : records)
            r.append(newtext,pret,postt);
        textrect.set_text_only(newtext, false);
    }

    ts::shared_ptr<contact_c> historian;
    ts::shared_ptr<contact_c> author;
    mutable ts::wstr_c protodesc;

    static const ts::flags32_s::BITS F_DIRTY_HEIGHT_CACHE = FLAGS_FREEBITSTART_LABEL << 0;

    static const int m_left = 10;
    static const int m_top = 3;
    
    mutable ts::ivec2 height_cache[2]; // size of array -> num of cached widths
    mutable int next_cache_write_index = 0;
    
    void ctx_menu_copy(const ts::str_c &)
    {
        gmsg<GM_COPY_HOTKEY>().send();
    }

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

    void prepare_message_time(ts::wstr_c &newtext, time_t posttime);
    ts::wstr_c prepare_button_rect(int r, const ts::ivec2 &sz);
    void kill_button( rectengine_c *beng, int r );
    void updrect(void *, int r, const ts::ivec2 &p);
    bool b_explore(RID, GUIPARAM);
    bool b_break(RID, GUIPARAM);
    bool b_pause(RID, GUIPARAM);
    bool b_unpause(RID, GUIPARAM);

public:

    enum
    {
        ST_JUST_TEXT,
        ST_RECV_FILE,
        ST_SEND_FILE,
        ST_CONVERSATION,
    } subtype = ST_JUST_TEXT;

    void dirty_height_cache() { flags.set(F_DIRTY_HEIGHT_CACHE); }

    gui_message_item_c(MAKE_CHILD<gui_message_item_c> &data) :gui_label_c(data), mt(data.mt), author(data.author), historian(data.historian) { set_theme_rect( CONSTASTR("message.") + data.skin, false );}
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

    void append_text( const post_s &post, bool resize_now = true );
    bool delivered(uint64 utag);
    bool with_utag(uint64 utag) const;
    bool remove_utag(uint64 utag);
    
    void init_date_separator( const tm &tmtm );
    void init_request( const ts::wstr_c &message );
    void init_load( int n_load );
};

class gui_messagelist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_messagelist_c);
    GM_RECEIVER(gui_messagelist_c, ISOGM_MESSAGE);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_messagelist_c, ISOGM_UPDATE_CONTACT_V);
    GM_RECEIVER(gui_messagelist_c, ISOGM_SUMMON_POST);
    GM_RECEIVER(gui_messagelist_c, ISOGM_DELIVERED);
    GM_RECEIVER(gui_messagelist_c, GM_DROPFILES);
    
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

    bool on_enter_press_func(RID, GUIPARAM);
public:
    gui_message_editor_c(initial_rect_data_s &data) :gui_textedit_c(data) {}
    /*virtual*/ ~gui_message_editor_c();
    /*virtual*/ void created() override;
};

class gui_message_area_c : public gui_group_c
{
    DUMMY(gui_message_area_c);

    static const ts::flags32_s::BITS F_INITIALIZED = FLAGS_FREEBITSTART << 0;

protected:
    /*virtual*/ void children_repos() override;
public:

    ts::safe_ptr<gui_message_editor_c> message_editor;
    ts::safe_ptr<gui_button_c> send_button;

    gui_message_area_c(initial_rect_data_s &data) :gui_group_c(data) {}
    /*virtual*/ ~gui_message_area_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_KEEP;}
    /*virtual*/ void created() override;
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

    GM_RECEIVER(gui_conversation_c, ISOGM_UPDATE_CONTACT_V);
    GM_RECEIVER(gui_conversation_c, ISOGM_SELECT_CONTACT);
    GM_RECEIVER(gui_conversation_c, ISOGM_CHANGED_PROFILEPARAM);
    GM_RECEIVER(gui_conversation_c, ISOGM_AV);
    GM_RECEIVER(gui_conversation_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_conversation_c, ISOGM_PROFILE_TABLE_SAVED);
    
    
    ts::tbuf_t<s3::Format> avformats;

    ts::safe_ptr<gui_contact_item_c> caption;
    ts::safe_ptr<gui_messagelist_c> msglist;
    ts::safe_ptr<gui_noticelist_c> noticelist;
    ts::safe_ptr<gui_message_area_c> messagearea;

    RID message_editor;

    /*virtual*/ void datahandler(const void *data, int size) override;
    /*virtual*/ s3::Format *formats(int &count) override;

    bool hide_show_messageeditor(RID r = RID(), GUIPARAM p = nullptr);

public:
    gui_conversation_c(initial_rect_data_s &data) :gui_vgroup_c(data) { /*defaultthrdraw = DTHRO_BASE;*/ }
    /*virtual*/ ~gui_conversation_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    contact_c *get_other() { ASSERT(!caption.expired()); return caption->contacted() ? &caption->getcontact() : nullptr; }
};
