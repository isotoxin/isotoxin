#pragma once
/*
  mainrect
*/

class desktop_shade_c;
template<> struct MAKE_ROOT<desktop_shade_c> : public _PROOT(desktop_shade_c)
{
    ts::irect r;
    MAKE_ROOT(const ts::irect &r) :_PROOT(desktop_shade_c)(), r(r) { init(RS_INACTIVE); }
    ~MAKE_ROOT();
};


class desktop_shade_c : public gui_control_c
{
    typedef gui_control_c super;

protected:
    /*virtual*/ void created() override;

public:
    desktop_shade_c(MAKE_ROOT<desktop_shade_c> &data);
    ~desktop_shade_c();

    /*virtual*/ ts::wstr_c get_name() const override { return ts::wstr_c(); }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    static desktop_shade_c &summon(const ts::irect &r);

};

class mainrect_c;
template<> struct MAKE_ROOT<mainrect_c> : public _PROOT(mainrect_c)
{
    ts::irect rect;
    MAKE_ROOT(const ts::irect &rect) : _PROOT(mainrect_c)(), rect(rect) { init(RS_TASKBAR); }
    ~MAKE_ROOT() {}
};

class desktop_rect_c : public gui_control_c
{
    GM_RECEIVER( desktop_rect_c, GM_HEARTBEAT );
    GM_RECEIVER( desktop_rect_c, GM_UI_EVENT );

    ts::safe_ptr<desktop_shade_c> shade;
    int checktick = 0;

protected:

    ts::wstr_c name;
    ts::irect rrect = ts::irect( 0 );
    ts::irect mrect = ts::irect( 0 );

    virtual void on_theme_changed() {}
    virtual void on_tick() {}
    virtual void on_activate() {}

    desktop_rect_c( initial_rect_data_s &data ):gui_control_c(data) {}
    desktop_rect_c() {}

public:

    /*virtual*/ ts::uint32 caption_buttons() const override { return SETBIT( CBT_MAXIMIZE ) | SETBIT( CBT_NORMALIZE ) | gui_control_c::caption_buttons(); }
    /*virtual*/ ts::wstr_c get_name() const override;

    virtual void close_req() {}
};

class mainrect_c : public desktop_rect_c
{
    DUMMY( mainrect_c );
    typedef desktop_rect_c super;

    /*virtual*/ void created() override;
    void onclosesave();
    bool saverectpos(RID,GUIPARAM);

    GM_RECEIVER( mainrect_c, ISOGM_CHANGED_SETTINGS );
    GM_RECEIVER( mainrect_c, ISOGM_APPRISE );

    ts::safe_ptr< gui_hgroup_c > hg;
    ts::safe_ptr<gui_conversation_c> mainconv;
    ts::array_safe_t< guirect_c, 4 > convs; // in split mode

    ts::bitmap_c icons; // (contact_online_state_check + 1) square images tiled vertically
    void rebuild_icons();

    /*virtual*/ void on_theme_changed() override { rebuild_icons(); }

public:
	mainrect_c(MAKE_ROOT<mainrect_c> &data);
	~mainrect_c();

    void apply_ui_mode( bool split_ui );
    RID create_new_conv( contact_root_c *c );
    RID find_conv_rid( const contact_key_s &histkey );

    /*virtual*/ ts::wstr_c get_name() const override;

        //sqhandler_i
	/*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

