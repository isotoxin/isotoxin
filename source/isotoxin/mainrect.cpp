#include "isotoxin.h"

mainrect_c::mainrect_c(initial_rect_data_s &data):gui_control_c(data)
{
}

mainrect_c::~mainrect_c() 
{
    cfg().onclosedie( DELEGATE(this, onclosesave) );
}

ts::uint32 mainrect_c::gm_handler( gmsg<ISOGM_APPRISE> & )
{
    if (getprops().is_collapsed())
        MODIFY(*this).decollapse();
    getengine().getroot()->set_system_focus(true);
    return 0;
}

/*virtual*/ ts::wstr_c mainrect_c::get_name() const
{
    return g_sysconf.name;
}

/*virtual*/ void mainrect_c::created()
{
    defaultthrdraw = DTHRO_BORDER | /*DTHRO_CENTER_HOLE |*/ DTHRO_CAPTION | DTHRO_CAPTION_TEXT;
    set_theme_rect(CONSTASTR("main"), false);
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
    
    gmsg<ISOGM_SELECT_CONTACT>(&contacts().get_self()).send(); // 1st selected item, yo

}

void mainrect_c::onclosesave()
{
    cfg().param(CONSTASTR("main_rect_pos"), ts::amake<ts::ivec2>(getprops().pos()));
    cfg().param(CONSTASTR("main_rect_size"), ts::amake<ts::ivec2>(getprops().size()));
}

/*virtual*/ bool mainrect_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_RECT_CHANGED)
    {
        cfg().onclosereg( DELEGATE(this, onclosesave) );
    }

    if (__super::sq_evt(qp, rid, data)) return true;

	//switch( qp )
	//{
	//case SQ_DRAW:
	//	if (const theme_rect_s *tr = themerect())
	//		draw( *tr );
	//	break;
	//}

    return false;
}
