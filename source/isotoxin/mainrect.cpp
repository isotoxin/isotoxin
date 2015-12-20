#include "isotoxin.h"

mainrect_c::mainrect_c(MAKE_ROOT<mainrect_c> &data):gui_control_c(data)
{
    rrect = data.rect;
    mrect = cfg().get<ts::irect>( CONSTASTR("main_rect_monitor"), ts::wnd_get_max_size(data.rect) );
}

mainrect_c::~mainrect_c() 
{
    if (gui) gui->delete_event( DELEGATE(this,saverectpos) );
    cfg().onclosedie( DELEGATE(this, onclosesave) );
}

ts::uint32 mainrect_c::gm_handler( gmsg<ISOGM_APPRISE> & )
{
    if (getprops().is_collapsed())
        MODIFY(*this).decollapse();
    if (getroot()) getroot()->set_system_focus(true);
    return 0;
}

/*virtual*/ ts::wstr_c mainrect_c::get_name() const
{
    if (name.is_empty())
        const_cast<ts::wstr_c &>(name).set(ts::wsptr(g_sysconf.name)).replace_all(CONSTWSTR(APPNAME), APPNAME_CAPTION);
    return name;
}

/*virtual*/ void mainrect_c::created()
{
    defaultthrdraw = DTHRO_BORDER | /*DTHRO_CENTER_HOLE |*/ DTHRO_CAPTION | DTHRO_CAPTION_TEXT;
    set_theme_rect(CONSTASTR("mainrect"), false);
    __super::created();
    gui->make_app_buttons( m_rid );

    auto uiroot = [](RID p)->RID
    {
        gui_hgroup_c &g = MAKE_VISIBLE_CHILD<gui_hgroup_c>(p);
        g.allow_move_splitter(true);
        g.leech(TSNEW(leech_fill_parent_s));
        g.leech(TSNEW(leech_save_proportions_s, CONSTASTR("main_splitter"), CONSTASTR("7060,12940")));
        return g.getrid();
    };

    RID hg = uiroot(m_rid);
    RID cl = MAKE_CHILD<gui_contactlist_c>( hg );
    RID chat = MAKE_CHILD<gui_conversation_c>( hg );
    hg.call_restore_signal();
    
    gmsg<ISOGM_SELECT_CONTACT>(&contacts().get_self(), 0).send(); // 1st selected item, yo

    g_app->F_ALLOW_AUTOUPDATE = !g_app->F_READONLY_MODE;


    if (const theme_rect_s *tr = themerect())
    {
        if (tr->captextadd.x >= 18)
        {
            int sz = tr->captextadd.x - 2;
            ts::bitmap_c icon, rsz;
            if (icon.load_from_file(CONSTWSTR("icon.png")))
            {
                rsz.create_ARGB(ts::ivec2(sz));
                icon.resize_to( rsz.extbody(), ts::FILTER_BOX_LANCZOS3 );
                rsz.premultiply();
                icons[0] = rsz;
            }

            if (icon.load_from_file(CONSTWSTR("icon-offline.png")))
            {
                rsz.create_ARGB(ts::ivec2(sz));
                icon.resize_to(rsz.extbody(), ts::FILTER_BOX_LANCZOS3);
                rsz.premultiply();
                icons[1] = rsz;
            }
        }
    }
}

ts::uint32 mainrect_c::gm_handler(gmsg<GM_HEARTBEAT> &)
{
    // check monitor

    if (0 != (checktick & 1) && !getprops().is_maximized())
    {
        ts::irect cmrect = ts::wnd_get_max_size(rrect);
        if (cmrect != mrect || getprops().screenrect() != rrect)
        {
            ts::ivec2 newc = rrect.center() - mrect.center() + cmrect.center();
            ts::irect newr = ts::irect::from_center_and_size(newc, rrect.size());
            newr.movein(cmrect);
            MODIFY(*this).pos(newr.lt).size(newr.size());
        }
    }
    ++checktick;

    return 0;
}

void mainrect_c::onclosesave()
{
    cfg().param(CONSTASTR("main_rect_pos"), ts::amake<ts::ivec2>(rrect.lt));
    cfg().param(CONSTASTR("main_rect_size"), ts::amake<ts::ivec2>(rrect.size()));
    cfg().param(CONSTASTR("main_rect_monitor"), ts::amake<ts::irect>(mrect));
}

bool mainrect_c::saverectpos(RID,GUIPARAM)
{
    onclosesave();
    return true;
}

/*virtual*/ bool mainrect_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_RECT_CHANGED)
    {
        cfg().onclosereg( DELEGATE(this, onclosesave) );
        if (data.changed.manual)
        {
            rrect = getprops().screenrect();
            mrect = ts::wnd_get_max_size(rrect);
            DEFERRED_UNIQUE_CALL( 1.0, DELEGATE(this,saverectpos), nullptr );
        }
    }

    if (__super::sq_evt(qp, rid, data)) return true;

	switch( qp )
	{
	case SQ_DRAW:
        if (const theme_rect_s *tr = themerect())
		if (icons[0].info().sz.x > 0 && icons[1].info().sz.x)
        {
            ts::irect cr = tr->captionrect( getprops().currentszrect(), getprops().is_maximized() );

            ts::bitmap_c &icon = icons[ g_app->F_OFFLINE_ICON ? 1 : 0 ];
            //cr.lt.y += (cr.height() - icon.info().sz.y)/2;
            cr.lt.y += tr->captextadd.y;

            getengine().begin_draw();
            getengine().draw( cr.lt, icon.extbody(), ts::irect(0, icon.info().sz), true );
            getengine().end_draw();
        }
		break;
	}

    return false;
}
