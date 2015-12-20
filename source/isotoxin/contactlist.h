#pragma once

enum contact_item_role_e
{
    CIR_LISTITEM,
    CIR_ME,
    CIR_CONVERSATION_HEAD,
    CIR_METACREATE,
    CIR_DNDOBJ,
};

class gui_contact_item_c;
template<> struct MAKE_CHILD<gui_contact_item_c> : public _PCHILD(gui_contact_item_c)
{
    contact_item_role_e role = CIR_LISTITEM;
    contact_c *contact;
    MAKE_CHILD(RID parent_, contact_c *c):contact(c) { parent = parent_; }
    ~MAKE_CHILD();
    MAKE_CHILD &operator << (contact_item_role_e r) { role = r; return *this; }
};
template<> struct MAKE_ROOT<gui_contact_item_c> : public _PROOT(gui_contact_item_c)
{
    contact_c *contact;
    MAKE_ROOT(contact_c *c) : _PROOT(gui_contact_item_c)(), contact(c) { init(false); }
    ~MAKE_ROOT() {}
};

class gui_contact_item_c : public gui_label_c
{
    enum colors_e
    {
        COLOR_TEXT_SPECIAL,
        COLOR_TEXT_TYPING,
        COLOR_TEXT_FOUND,

        COLOR_PROTO_TEXT_ONLINE = 1,
        COLOR_PROTO_TEXT_OFFLINE,
    };

    DUMMY(gui_contact_item_c);
    ts::shared_ptr<contact_c> contact;
    contact_item_role_e role = CIR_LISTITEM;
    GM_RECEIVER( gui_contact_item_c, ISOGM_SELECT_CONTACT );
    
    ts::wstr_c typing_buf;
    bool stop_typing(RID, GUIPARAM);
    bool animate_typing(RID, GUIPARAM);

    ts::svec2 shiftstateicon;

    struct protocols_s
    {
        ts::safe_ptr<gui_contact_item_c> owner;
        ts::wstr_c str;
        ts::ivec2 size = ts::ivec2(0);
        bool dirty = true;
        protocols_s(gui_contact_item_c *itm):owner(itm) {}
        void update();
        void clear() { dirty = true; str.clear(); }
    };

    const protocols_s *protocols() const
    {
        ASSERT( CIR_CONVERSATION_HEAD == role );
        return (const protocols_s *)get_customdata();
    }
    protocols_s *protocols( bool create )
    {
        ASSERT(CIR_CONVERSATION_HEAD == role);
        if (!get_customdata() && create)
            set_customdata_obj<protocols_s>(this);
        return (protocols_s *)get_customdata();
    }

    static const ts::flags32_s::BITS  F_PROTOHIT    = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS  F_NOPROTOHIT  = FLAGS_FREEBITSTART_LABEL << 1;
    static const ts::flags32_s::BITS  F_EDITNAME    = FLAGS_FREEBITSTART_LABEL << 2;
    static const ts::flags32_s::BITS  F_EDITSTATUS  = FLAGS_FREEBITSTART_LABEL << 3;
    static const ts::flags32_s::BITS  F_SKIPUPDATE  = FLAGS_FREEBITSTART_LABEL << 4;
    static const ts::flags32_s::BITS  F_LBDN        = FLAGS_FREEBITSTART_LABEL << 5;
    static const ts::flags32_s::BITS  F_DNDDRAW     = FLAGS_FREEBITSTART_LABEL << 6;
    static const ts::flags32_s::BITS  F_CALLBUTTON  = FLAGS_FREEBITSTART_LABEL << 7;
    static const ts::flags32_s::BITS  F_SHOWTYPING  = FLAGS_FREEBITSTART_LABEL << 8;

    friend class contact_c;
    friend class contacts_c;

    void set_default_proto(const ts::str_c&ost);

    bool redraw_now(RID, GUIPARAM);
    bool audio_call(RID, GUIPARAM);

    bool edit0(RID, GUIPARAM);
    bool edit1(RID, GUIPARAM);
    void updrect(const void *, int r, const ts::ivec2 &);

    void generate_protocols();
    void draw_online_state_text(draw_data_s &dd);

public:
    gui_contact_item_c(MAKE_ROOT<gui_contact_item_c> &data);
    gui_contact_item_c(MAKE_CHILD<gui_contact_item_c> &data);
    /*virtual*/ ~gui_contact_item_c();

    ts::wstr_c tt();

    void typing();

    int contact_item_rite_margin();

    bool allow_drag() const;
    bool allow_drop() const;
    /*virtual*/ void update_dndobj(guirect_c *donor) override;
    /*virtual*/ guirect_c * summon_dndobj(const ts::ivec2 &deltapos) override;

    void target(bool tgt); // d'n'd target
    void on_drop(gui_contact_item_c *ondr);

    bool is_protohit() const {return flags.is(F_PROTOHIT);}
    bool is_noprotohit() const {return flags.is(F_NOPROTOHIT);}
    int sort_power() const
    {
        if (contact && contact->getkey().is_group())
            return contact->subonlinecount() + 2;

        if (!is_protohit())
        {
            if (contact)
            {
                auto state = contact->get_meta_state();
                if (CS_INVITE_RECEIVE == state || CS_INVITE_SEND == state) return 2;
            }

            return -100;
        }
        if (is_noprotohit()) return 1;
        return 2;
    }

    void clearprotocols();
    void protohit();
    bool update_buttons( RID r = RID(), GUIPARAM p = nullptr );
    bool cancel_edit( RID r = RID(), GUIPARAM p = nullptr);
    bool apply_edit( RID r = RID(), GUIPARAM p = nullptr);

    contact_item_role_e getrole() const {return role;}

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void resetcontact()
    {
        if (flags.is(F_SKIPUPDATE)) return;
        contact = nullptr; update_text();
    }
    void setcontact(contact_c *c);
    contact_c &getcontact() {return SAFE_REF(contact.get());}
    const contact_c &getcontact() const {return SAFE_REF(contact.get());}
    bool contacted() const {return contact != nullptr;}
    void update_text();

    bool is_after(gui_contact_item_c &ci); // sort comparsion


    void redraw(float delay);
};

enum contact_list_role_e
{
    CLR_MAIN_LIST,
    CLR_NEW_METACONTACT_LIST,
};

class gui_contactlist_c;
template<> struct MAKE_CHILD<gui_contactlist_c> : public _PCHILD(gui_contactlist_c)
{
    contact_list_role_e role = CLR_MAIN_LIST;
    MAKE_CHILD(RID parent_, contact_list_role_e role = CLR_MAIN_LIST) :role(role) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_contactlist_c : public gui_vscrollgroup_c
{
    DUMMY(gui_contactlist_c);

    GM_RECEIVER(gui_contactlist_c, ISOGM_PROFILE_TABLE_SAVED);
    GM_RECEIVER(gui_contactlist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_contactlist_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(gui_contactlist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_contactlist_c, ISOGM_DO_POSTEFFECT);
    GM_RECEIVER(gui_contactlist_c, ISOGM_TYPING);
    GM_RECEIVER(gui_contactlist_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_contactlist_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_contactlist_c, GM_DRAGNDROP);
    GM_RECEIVER(gui_contactlist_c, GM_UI_EVENT)
    {
        if (UE_THEMECHANGED == p.evt) recreate_ctls();
        return 0;
    }


    static const ts::flags32_s::BITS F_NO_LEECH_CHILDREN = F_VSCROLLFREEBITSTART << 0;

    uint64 sort_tag = 0;
    contact_list_role_e role = CLR_MAIN_LIST;

    int skip_top_pixels = 70;
    int skip_bottom_pixels = 70;
    int skipctl = 0;

    ts::safe_ptr<gui_filterbar_c> filter;
    ts::safe_ptr<gui_button_c> addcbtn; // add contact button
    ts::safe_ptr<gui_button_c> addgbtn; // add group button
    ts::safe_ptr<gui_contact_item_c> self;
    ts::safe_ptr<gui_contact_item_c> dndtarget;

    ts::array_inplace_t<contact_key_s, 2> * arr = nullptr;

    void recreate_ctls(bool focus_filter = false);
    /*virtual*/ bool i_leeched( guirect_c &to ) override;
    bool filter_proc(system_query_e qp, evt_data_s &data);

    /*virtual*/ void children_repos_info(cri_s &info) const override;
public:
    gui_contactlist_c( MAKE_CHILD<gui_contactlist_c> &data) :gui_vscrollgroup_c(data) { defaultthrdraw = DTHRO_BASE; role = data.role; }
    /*virtual*/ ~gui_contactlist_c();

    void array_mode( ts::array_inplace_t<contact_key_s, 2> & arr );
    void refresh_array();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_KEEP;}
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    rectengine_c *get_first_contact_item() {return getengine().children_count() > skipctl ? getengine().get_child(skipctl) : nullptr; }
    bool on_filter_deactivate(RID, GUIPARAM);
    void update_filter_pos();

};
