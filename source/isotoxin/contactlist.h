#pragma once

//-V:getcontact():807

enum contact_item_role_e
{
    CIR_LISTITEM,
    CIR_ME,
    CIR_CONVERSATION_HEAD,
    CIR_METACREATE,
    CIR_DNDOBJ,
    CIR_SEPARATOR,
};

class gui_contact_item_c;
class gui_contact_separator_c;

template<> struct MAKE_CHILD<gui_contact_item_c> : public _PCHILD(gui_contact_item_c)
{
    contact_item_role_e role = CIR_LISTITEM;
    contact_root_c *contact;
    bool is_visible = true;
    MAKE_CHILD(RID parent_, contact_root_c *c):contact(c) { parent = parent_; }
    ~MAKE_CHILD();
    MAKE_CHILD &operator << (contact_item_role_e r) { role = r; return *this; }
};
template<> struct MAKE_ROOT<gui_contact_item_c> : public _PROOT(gui_contact_item_c)
{
    contact_root_c *contact;
    MAKE_ROOT(contact_root_c *c) : _PROOT(gui_contact_item_c)(), contact(c) { init( RS_INACTIVE ); }
    ~MAKE_ROOT() {}
};


class gui_conversation_header_c;
template<> struct MAKE_CHILD<gui_conversation_header_c> : public _PCHILD( gui_conversation_header_c )
{
    contact_root_c *contact;
    MAKE_CHILD( RID parent_, contact_root_c *c ) :contact( c ) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_clist_base_c : public gui_label_c
{
protected:
    contact_item_role_e role = CIR_LISTITEM;
public:
    gui_clist_base_c() {}
    gui_clist_base_c( initial_rect_data_s &data, contact_item_role_e role ) :gui_label_c( data ), role(role) {}

    contact_item_role_e getrole() const { return role; }
    virtual int proto_sort_factor() const = 0;

    gui_contact_item_c *as_item();
    const gui_contact_item_c *as_item() const;
    gui_contact_separator_c *as_separator();
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
        ts::ivec2 p = ts::ivec2(0);
        bool updated = false;
        bool dirty = true;
    };
};


class gui_contact_item_c : public gui_clist_base_c
{
    DUMMY(gui_contact_item_c);
    typedef gui_clist_base_c super;

    GM_RECEIVER( gui_contact_item_c, ISOGM_SELECT_CONTACT );
    
    ts::wstr_c typing_buf;
    bool stop_typing(RID, GUIPARAM);
    bool animate_typing(RID, GUIPARAM);

    ts::svec2 shiftstateicon;

    friend class contact_c;
    friend class contacts_c;

    bool redraw_now(RID, GUIPARAM);

    void draw();

    static const ts::flags32_s::BITS  F_LBDN = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS  F_DNDDRAW = FLAGS_FREEBITSTART_LABEL << 1;
    static const ts::flags32_s::BITS  F_SHOWTYPING = FLAGS_FREEBITSTART_LABEL << 2;

    static const ts::flags32_s::BITS  F_VIS_FILTER = FLAGS_FREEBITSTART_LABEL << 3;
    static const ts::flags32_s::BITS  F_VIS_GROUP = FLAGS_FREEBITSTART_LABEL << 4;

protected:
    static const ts::flags32_s::BITS  FLAGS_FREEBITSTART_CITM = FLAGS_FREEBITSTART_LABEL << 5;

    ts::shared_ptr<contact_root_c> contact;

    enum colors_e
    {
        COLOR_TEXT_SPECIAL,
        COLOR_TEXT_TYPING,
        COLOR_TEXT_FOUND,

        COLOR_PROTO_TEXT_ONLINE = 0,
        COLOR_PROTO_TEXT_OFFLINE,
    };

public:
    gui_contact_item_c(MAKE_ROOT<gui_contact_item_c> &data);
    gui_contact_item_c(MAKE_CHILD<gui_contact_item_c> &data);
    gui_contact_item_c( MAKE_CHILD<gui_conversation_header_c> &data );
    /*virtual*/ ~gui_contact_item_c();

    void vis_filter( bool f ) { ASSERT( CIR_ME != role ); flags.init( F_VIS_FILTER, f ); MODIFY( *this ).visible( flags.is( F_VIS_FILTER ) && flags.is( F_VIS_GROUP ) ); }
    void vis_group( bool f ) { ASSERT( CIR_ME != role ); flags.init( F_VIS_GROUP, f ); MODIFY( *this ).visible( flags.is( F_VIS_FILTER ) && flags.is( F_VIS_GROUP ) ); }

    bool is_vis_filter() const { return flags.is( F_VIS_FILTER ); }
    bool is_vis_group() const { return flags.is( F_VIS_GROUP ); }

    ts::wstr_c tt();

    void typing();

    virtual int contact_item_rite_margin() const;

    bool allow_drag() const;
    bool allow_drop() const;
    /*virtual*/ void update_dndobj(guirect_c *donor) override;
    /*virtual*/ guirect_c * summon_dndobj(const ts::ivec2 &deltapos) override;

    void target(bool tgt); // d'n'd target
    void on_drop(gui_contact_item_c *ondr);

    int sort_power() const
    {
        if (!contact)
            return -100;

        if ( contact->getkey().is_conference() )
        {
            if ( contact->get_state() == CS_OFFLINE )
                return -200;
            return contact->subonlinecount() + 3;
        }

        return 2;
    }

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    virtual bool setcontact(contact_root_c *c);
    contact_root_c &getcontact() {return SAFE_REF(contact.get());}
    const contact_root_c &getcontact() const {return SAFE_REF(contact.get());}
    const contact_root_c *getcontact_ptr() const { return contact.get(); }
    bool contacted() const {return contact != nullptr;}
    virtual void update_text();

    bool is_after(gui_contact_item_c &ci); // sort comparison
    /*virtual*/ int proto_sort_factor() const override;
    //bool same_prots(const gui_contact_item_c &itm) const;

    void redraw(float delay);
};

class gui_conversation_header_c : public gui_contact_item_c
{
    friend class gui_contact_item_c;

    static const ts::flags32_s::BITS  F_EDITNAME = FLAGS_FREEBITSTART_CITM << 0;
    static const ts::flags32_s::BITS  F_EDITSTATUS = FLAGS_FREEBITSTART_CITM << 1;
    static const ts::flags32_s::BITS  F_SKIPUPDATE = FLAGS_FREEBITSTART_CITM << 2;
    static const ts::flags32_s::BITS  F_DIRTY = FLAGS_FREEBITSTART_CITM << 3;

    void set_default_proto( const ts::str_c&ost );

    bool createvcallnow(RID, GUIPARAM);
    bool deletevcallnow(RID, GUIPARAM);
    bool avbinout(RID, GUIPARAM);
    bool av_call( RID, GUIPARAM );
    bool cancel_edit( RID r = RID(), GUIPARAM p = nullptr );
    bool apply_edit( RID r = RID(), GUIPARAM p = nullptr );

    bool edit0( RID, GUIPARAM );
    bool edit1( RID, GUIPARAM );
    void updrect( const void *, int r, const ts::ivec2 & );

    ts::wstr_c str;
    ts::ivec2 size = ts::ivec2( 0 );
    void hstuff_update();
    void hstuff_clear() { flags.set(F_DIRTY); str.clear(); }

    ts::ivec2 last_head_text_pos;
    rbtn::ebutton_s updr[ rbtn::EB_MAX ];
    RID editor;
    RID bconfirm;
    RID bcancel;
    ts::str_c curedit; // utf8
    ts::safe_ptr<gui_button_c> acall_button;
    ts::safe_ptr<gui_button_c> vcall_button;
    ts::safe_ptr<gui_button_c> b1;
    ts::safe_ptr<gui_button_c> b2;
    ts::safe_ptr<gui_button_c> b3;

    bool _edt( const ts::wstr_c & e, bool )
    {
        curedit = to_utf8( e );
        return true;
    }

    bool b_noti_switch( RID, GUIPARAM p );

public:

    gui_conversation_header_c( MAKE_CHILD<gui_conversation_header_c> &data ):gui_contact_item_c( data ) {}
    ~gui_conversation_header_c();

    bool update_buttons( RID r = RID(), GUIPARAM p = nullptr );

    void resetcontact()
    {
        if ( flags.is( F_SKIPUPDATE ) ) return;
        contact = nullptr;
        update_text();
    }

    /*virtual*/ int contact_item_rite_margin() const override;
    int prepare_protocols();

    void draw_online_state_text( draw_data_s &dd );
    void generate_protocols();
    void clearprotocols();
    /*virtual*/ bool setcontact( contact_root_c *c ) override;
    /*virtual*/ void update_text() override;

    void on_rite_click();

    bool edit_mode() const
    {
        return flags.is( F_EDITNAME | F_EDITSTATUS );
    }
};


template<> struct MAKE_CHILD<gui_contact_separator_c> : public _PCHILD( gui_contact_separator_c )
{
    ts::wstr_c text;
    const contact_root_c *cc = nullptr;
    bool is_visible = true;
    MAKE_CHILD( RID parent_, const ts::wstr_c &text, bool is_visible = true ) :text( text ), is_visible( is_visible ) { parent = parent_; }
    MAKE_CHILD( RID parent_, const contact_root_c *cc, bool is_visible = true ) :cc( cc ), is_visible( is_visible ) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_contact_separator_c : public gui_clist_base_c
{
    DUMMY( gui_contact_separator_c );
    typedef gui_clist_base_c super;

    static const ts::flags32_s::BITS  F_COLLAPSED = FLAGS_FREEBITSTART_LABEL << 0;

    ts::tbuf0_t<ts::uint16> prots;

    system_query_e clicka = SQ_NOP;

    bool on_collapse_or_expand( RID, GUIPARAM );

    void moveup( const ts::str_c& );
    void movedn( const ts::str_c& );

public:
    gui_contact_separator_c( MAKE_CHILD<gui_contact_separator_c> &data );
    /*virtual*/ ~gui_contact_separator_c();

    void set_prots_from_contact( const contact_root_c *cc );
    bool is_prots_same_as_contact( const contact_root_c *cc ) const;
    void update_text();

    ///*virtual*/ int get_height_by_width( int width ) const override;
    ///*virtual*/ ts::ivec2 get_min_size() const override;
    ///*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;

    /*virtual*/ int proto_sort_factor() const override;
    bool is_collapsed() const { return flags.is( F_COLLAPSED ); }
    void no_collapsed() { flags.clear( F_COLLAPSED ); }
};

INLINE gui_contact_item_c *gui_clist_base_c::as_item()
{
    return role != CIR_SEPARATOR ? ts::ptr_cast<gui_contact_item_c *>( this ) : nullptr;
}
INLINE const gui_contact_item_c *gui_clist_base_c::as_item() const
{
    return role != CIR_SEPARATOR ? ts::ptr_cast<const gui_contact_item_c *>( this ) : nullptr;
}
INLINE gui_contact_separator_c *gui_clist_base_c::as_separator()
{
    return role == CIR_SEPARATOR ? ts::ptr_cast<gui_contact_separator_c *>( this ) : nullptr;
}


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
    typedef gui_vscrollgroup_c super;

    GM_RECEIVER(gui_contactlist_c, ISOGM_PROFILE_TABLE_SL);
    GM_RECEIVER(gui_contactlist_c, ISOGM_PROTO_LOADED);
    GM_RECEIVER(gui_contactlist_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(gui_contactlist_c, ISOGM_V_UPDATE_CONTACT);
    GM_RECEIVER(gui_contactlist_c, ISOGM_DO_POSTEFFECT);
    GM_RECEIVER(gui_contactlist_c, ISOGM_TYPING);
    GM_RECEIVER(gui_contactlist_c, ISOGM_CALL_STOPED);
    GM_RECEIVER(gui_contactlist_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_contactlist_c, GM_DRAGNDROP);
    GM_RECEIVER(gui_contactlist_c, GM_UI_EVENT);


    static const ts::flags32_s::BITS F_NO_LEECH_CHILDREN = F_VSCROLLFREEBITSTART << 0;
    static const ts::flags32_s::BITS F_KEEP_SCROLL_POS = F_VSCROLLFREEBITSTART << 1; // will be set on first manual scroll

    int sort_tag = 0;
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

    /*virtual*/ void on_manual_scroll(manual_scroll_e ms) override;
    /*virtual*/ void children_repos_info(cri_s &info) const override;
    /*virtual*/ bool test_under_point( const guirect_c &r, const ts::ivec2& screenpos ) const override;

    bool refresh_list(RID, GUIPARAM);

public:
    gui_contactlist_c( MAKE_CHILD<gui_contactlist_c> &data) :gui_vscrollgroup_c(data) { defaultthrdraw = DTHRO_BASE; role = data.role; }
    /*virtual*/ ~gui_contactlist_c();

    void array_mode( ts::array_inplace_t<contact_key_s, 2> & arr );
    void refresh_array();
    void fix_sep_visibility();
    void fix_c_visibility();
    void clearlist();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ size_policy_e size_policy() const override {return SP_KEEP;}
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    rectengine_c *get_first_contact_item() {return getengine().children_count() > skipctl ? getengine().get_child(skipctl) : nullptr; }
    bool on_filter_deactivate(RID, GUIPARAM);
    void update_filter_pos();

};
