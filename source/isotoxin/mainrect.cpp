#include "isotoxin.h"

ts::uint32 desktop_rect_c::gm_handler( gmsg<GM_HEARTBEAT> & )
{
    // check monitor

    if ( 0 != ( checktick & 1 ) && !getprops().is_maximized() )
    {
        ts::irect cmrect = ts::wnd_get_max_size( rrect );
        if ( cmrect != mrect || getprops().screenrect() != rrect )
        {
            ts::ivec2 newc = rrect.center() - mrect.center() + cmrect.center();
            ts::irect newr = ts::irect::from_center_and_size( newc, rrect.size() );
            newr.movein( cmrect );
            MODIFY( *this ).pos( newr.lt ).size( newr.size() );
        }
    }
    ++checktick;

    on_tick();

    return 0;
}


ts::uint32 desktop_rect_c::gm_handler( gmsg<GM_UI_EVENT> &ue )
{
    if ( ue.evt == UE_THEMECHANGED )
    {
        name.clear();
        on_theme_changed();
    }
    if ( ue.evt == UE_ACTIVATE && ue.arid == getrid() )
        on_activate();
    return 0;
}

/*virtual*/ ts::wstr_c desktop_rect_c::get_name() const
{
    if ( name.is_empty() )
    {
        ts::wstr_c &n = const_cast<ts::wstr_c &>( name );
        n.set( APPNAME_CAPTION );
    }
    return name;
}


namespace
{

    class conv_host_c;
    template<> struct MAKE_ROOT<conv_host_c> : public _PROOT( conv_host_c )
    {
        ts::irect rect;
        mainrect_c *mr;
        contact_root_c *c;
        MAKE_ROOT( const ts::irect &rect, mainrect_c *mr, contact_root_c *c ) : _PROOT( conv_host_c )( ), rect( rect ), mr(mr), c(c) { init( RS_TASKBAR ); }
        ~MAKE_ROOT() { }
    };


    class conv_host_c : public desktop_rect_c
    {
        DUMMY( conv_host_c );

        ts::safe_ptr<mainrect_c> mr;
        ts::shared_ptr<contact_root_c> contact;

        void custom_close( const ts::str_c&p )
        {
            if ( p.equals( CONSTASTR( "0" ) ) )
                contact->b_hangup(RID(), nullptr);

            close_me( RID(), nullptr );
        }

        bool close_me_chk( RID, GUIPARAM )
        {
            if ( !contact->getkey().is_group() && g_app->find_avcontact_inprogress( contact ) != nullptr )
            {
                ts::str_c nn = contact->get_name();
                text_convert_from_bbcode( nn );

                dialog_msgbox_c::mb_warning( TTT( "There are call with $[br]What do you want to do with this call?", 482 ) / from_utf8( nn ) ).bcancel()
                    .bcustom( true, TTT( "Break", 483 ) )
                    .bok( TTT( "Dont't break", 484 ) )
                    .on_ok( DELEGATE( this, custom_close ), CONSTASTR( "1" ) ).on_custom( DELEGATE( this, custom_close ), CONSTASTR( "0" ) ).summon();
            } else
            {
                close_me(RID(), nullptr);
            }
            return true;
        }

        bool close_me( RID, GUIPARAM )
        {
            cfg().oncloseunreg( DELEGATE( this, onclosesave ) );
            onclosesave();
            TSDEL( this );
            return true;
        }
        bool min_me( RID, GUIPARAM )
        {
            g_app->iterate_avcontacts( [this]( av_contact_s & avc ) {
                if ( avc.state != av_contact_s::AV_INPROGRESS )
                    return;
                if ( contact == avc.c ) avc.set_inactive( true );
            } );

            MODIFY( getrid() ).minimize( true );
            return true;
        }

        /*virtual*/ void close_req() override
        {
            close_me( RID(), nullptr );
        }

        /*virtual*/ void on_activate() override
        {
            g_app->iterate_avcontacts( [this]( av_contact_s & avc ) {
                if ( avc.state != av_contact_s::AV_INPROGRESS )
                    return;
                if ( contact == avc.c ) avc.set_inactive( false );
            } );
        }

        /*virtual*/ ts::wstr_c get_name() const override
        {
            if ( name.is_empty() )
            {
                ts::wstr_c &n = const_cast<ts::wstr_c &>( name );
                ts::str_c nn = contact->get_name();
                text_convert_from_bbcode(nn);
                n.set( from_utf8( nn ) );

            }
            return name;
        }

        /*virtual*/ void created() override
        {
            defaultthrdraw = DTHRO_BORDER | /*DTHRO_CENTER_HOLE |*/ DTHRO_CAPTION | DTHRO_CAPTION_TEXT;
            set_theme_rect( CONSTASTR( "mainrect" ), false );
            __super::created();

            bcreate_s cbs[2];
            cbs[0].handler = DELEGATE( this, close_me_chk );
            cbs[0].tooltip = ( GET_TOOLTIP )[]()->ts::wstr_c { return ts::wstr_c(TTT("Close",481)); };

            cbs[1].handler = DELEGATE( this, min_me );
            cbs[1].tooltip = ( GET_TOOLTIP )[]()->ts::wstr_c { return loc_text( loc_minimize ); };

            gui->make_app_buttons( m_rid, 0xffffffff, cbs + 0, cbs + 1 );

            conv = &MAKE_CHILD<gui_conversation_c>( getrid() ).get();
            conv->leech( TSNEW( leech_fill_parent_s ) );

            on_tick();

            DEFERRED_EXECUTION_BLOCK_BEGIN( 0 )

                const conv_host_c *me = (const conv_host_c *)param;
                gmsg<ISOGM_INIT_CONVERSATION>( me->contact, 0, me->conv->getrid() ).send();
                g_app->update_buttons_msg();

            DEFERRED_EXECUTION_BLOCK_END( this )

        }

        void onclosesave()
        {
            ts::astrmap_c d( cfg().convs() );

            ts::str_c s;
            config_base_c::cvts<ts::irect>::cvt( true, s, getprops().screenrect() );
            d.set( contact->getkey().as_str() ) = s;
            cfg().convs( d.to_str() );
        }

        bool saverectpos( RID, GUIPARAM )
        {
            onclosesave();
            return true;
        }

        //GM_RECEIVER( conv_host_c, ISOGM_CHANGED_SETTINGS )
        //{

        //    return 0;
        //}

        ts::safe_ptr<gui_conversation_c> conv;
        ts::bitmap_c icon, desktopicon;

        struct icontag_s
        {
            union
            {
                struct
                {
                    unsigned id : 16;
                    unsigned cs : contact_state_bits;
                    unsigned cso : contact_online_state_bits;
                };
                int full;
            };
            icontag_s() { full = 0; }

            icon_type_e itype() const
            {
                if ( cs == CS_OFFLINE )
                    return IT_OFFLINE;
                switch ( cso )
                {
                case COS_ONLINE:
                    return IT_ONLINE;
                case COS_AWAY:
                    return IT_AWAY;
                case COS_DND:
                    return IT_DND;
                }
            }

        } icontag;

        TS_STATIC_CHECK( sizeof( icontag_s ) <= sizeof(int), "" );

        virtual void on_tick() override
        {
            icontag_s itag;
            bool emptyitag = true;

            auto csfix = []( contact_state_e s ) ->contact_state_e
            {
                return s == CS_ONLINE ? CS_ONLINE : CS_OFFLINE;
            };

            contact->subiterate( [&]( contact_c *c ) {
       
                if ( emptyitag )
                {
                    itag.cs = csfix( c->get_state() );
                    itag.cso = c->get_ostate();
                    itag.id = (ts::int16)c->getkey().protoid;
                    emptyitag = false;
                } else
                {
                    if ( c->get_state() == CS_ONLINE && itag.cs != CS_ONLINE )
                    {
                        itag.cs = CS_ONLINE;
                        itag.cso = c->get_ostate();
                        itag.id = ( ts::int16 )c->getkey().protoid;
                    } else if ( csfix(c->get_state()) == itag.cs )
                    {
                        if ( c->get_ostate() < itag.cso )
                        {
                            itag.cso = c->get_ostate();
                            itag.id = ( ts::int16 )c->getkey().protoid;
                        }
                    }
                }

            } );

            if ( itag.full != icontag.full || icon.info().sz.x == 0 )
            {
                // rebuild icon now
                if ( const theme_rect_s *tr = themerect() )
                {
                    if ( tr->captextadd.x >= 18 )
                    {
                        int sz = tr->captextadd.x - 4;
                        if ( active_protocol_c *ap = prf().ap( itag.id ) )
                        {
                            icon = ap->get_icon( sz, itag.itype() );
                            desktopicon = ap->get_icon( 32, itag.itype() );

                            ts::irect r( 0, 0, 100, 100 ); // just redraw left top 100px square of main rect. (we hope that app icon less then 100px)
                            getengine().redraw( &r );

                            getroot()->update_icon();
                        }
                    }
                }

                icontag.full = itag.full;
            }
        }

    public:
        conv_host_c( MAKE_ROOT<conv_host_c> &data ):desktop_rect_c(data), mr(data.mr), contact(data.c)
        {
            rrect = data.rect;
            mrect = cfg().get<ts::irect>( CONSTASTR( "main_rect_monitor" ), ts::wnd_get_max_size( data.rect ) );
        }
        ~conv_host_c()
        {
            cfg().oncloseunreg( DELEGATE( this, onclosesave ) );
            if ( gui )
            {
                gui->delete_event( DELEGATE( this, saverectpos ) );
                gui->delete_event( DELEGATE( this, close_me ) );
            }
        }

        const contact_root_c *getconctact() const { return contact; }

        /*virtual*/ ts::bitmap_c get_icon( bool for_tray ) override
        {
            if ( for_tray || desktopicon.info().sz.x == 0 )
                return guirect_c::get_icon( for_tray );

            return desktopicon;
        }

        //sqhandler_i
        /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override
        {
            if ( qp == SQ_RECT_CHANGED )
            {
                cfg().onclosereg( DELEGATE( this, onclosesave ) );
                if ( data.changed.manual )
                {
                    rrect = getprops().screenrect();
                    mrect = ts::wnd_get_max_size( rrect );
                    DEFERRED_UNIQUE_CALL( 1.0, DELEGATE( this, saverectpos ), nullptr );
                }
            }

            if ( rid != getrid() )
            {
                if ( qp == SQ_FOCUS_CHANGED && rid != conv->getrid() && data.changed.focus )
                {
                    conv->set_focus();
                    return true;
                }
                return false;
            }

            if ( __super::sq_evt( qp, rid, data ) ) return true;

            switch ( qp )
            {
            case SQ_DRAW:
                if ( const theme_rect_s *tr = themerect() )
                    if ( icon.info().sz.x > 0 )
                    {
                        ts::irect cr = tr->captionrect( getprops().currentszrect(), getprops().is_maximized() );
                        ts::irect ir;
                        ir.lt.x = 0;
                        ir.rb.x = icon.info().sz.x;
                        ir.lt.y = 0;
                        ir.rb.y = ir.lt.y + ir.rb.x;
                        cr.lt.y += tr->captextadd.y;

                        getengine().begin_draw();
                        getengine().draw( cr.lt, icon.extbody( ir ), true );
                        getengine().end_draw();
                    }
                break;
            case SQ_CLOSE:
                {
                    DEFERRED_UNIQUE_CALL( 0, DELEGATE( this, close_me_chk ), nullptr );
                }
                return true;
            }

            return false;
        }
    };

}







mainrect_c::mainrect_c(MAKE_ROOT<mainrect_c> &data):desktop_rect_c(data)
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
    if (g_app->F_MODAL_ENTER_PASSWORD) return 0;

    if (getprops().is_collapsed())
        MODIFY(*this).decollapse();
    if (getroot()) getroot()->set_system_focus(true);
    return 0;
}

ts::uint32 mainrect_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS> &ch)
{
    if (ch.pass == 0)
    {
        if (PP_ONLINESTATUS == ch.sp)
        {
            ts::irect r(0,0,100,100); // just redraw left top 100px square of main rect. (we hope that app icon less then 100px)
            getengine().redraw( &r );
        }
    }

    return 0;
}

/*virtual*/ ts::wstr_c mainrect_c::get_name() const
{
    if ( name.is_empty() )
    {
        ts::wstr_c &n = const_cast<ts::wstr_c &>( name );
        n.set( APPNAME_CAPTION );

#ifdef _DEBUG
        n.append_char(' ');
        n.append( ts::to_wstr( application_c::appver() ) );
        n.append( CONSTWSTR(" - CRC:") );
        ts::buf_c b;
        b.load_from_disk_file( ts::get_exe_full_name() );
        long sz;
        wchar_t bx[ 32 ];
        wchar_t * t = ts::CHARz_make_str_unsigned<ts::wchar, uint>( bx, sz, b.crc() );
        n.append( ts::wsptr(t) );
#endif // _DEBUG
        
    }
    return name;
}

/*virtual*/ void mainrect_c::created()
{
    defaultthrdraw = DTHRO_BORDER | /*DTHRO_CENTER_HOLE |*/ DTHRO_CAPTION | DTHRO_CAPTION_TEXT;
    set_theme_rect(CONSTASTR("mainrect"), false);
    __super::created();
    gui->make_app_buttons(m_rid);

    apply_ui_mode( g_app->F_SPLIT_UI );

    g_app->F_ALLOW_AUTOUPDATE = !g_app->F_READONLY_MODE;

    rebuild_icons();
}

RID mainrect_c::find_conv_rid( const contact_key_s &histkey )
{
    for ( guirect_c *r : convs )
        if ( r )
        {
            conv_host_c *h = ts::ptr_cast<conv_host_c *>( r );
            if ( histkey == h->getconctact()->getkey() )
                return r->getrid();
        }
    return RID();
}

RID mainrect_c::create_new_conv( contact_root_c *c )
{
    for( guirect_c *r : convs )
        if (r)
        {
            conv_host_c *h = ts::ptr_cast<conv_host_c *>( r );
            if ( c == h->getconctact() )
            {
                g_app->bring2front( c );
                return r->getrid();
            }
        }

    ts::astrmap_c d( cfg().convs() );

    ts::str_c coords = d.get( c->getkey().as_str() );
    if ( coords.is_empty() )
        coords = cfg().get( CONSTASTR( "main_defconv" ), "0,0,300,300" );

    ts::irect defconvc = ts::parserect( coords, ts::irect( 0, 0, 300, 300 ) );

    for ( int i = convs.size() - 1; i >= 0; --i )
        if ( convs.get( i ).expired() )
            convs.remove_fast( i );


    redraw_collector_s dch;
    MAKE_ROOT<conv_host_c> mch( defconvc, this, c );
    conv_host_c &ch = mch;
    convs.add( &ch );

    ts::ivec2 sz = MIN_CONV_SIZE;

    ts::wnd_fix_rect( defconvc, sz.x, sz.y );

    MODIFY( ch )
        .size( defconvc.size() )
        .pos( defconvc.lt )
        .allow_move_resize()
        .show()
        .maximize( false );

    return ch.getrid();
}

void mainrect_c::apply_ui_mode( bool split_ui )
{
    if ( split_ui )
    {
        int neww = getprops().size().x;
        if (g_app->contactlist)
        {
            neww = g_app->contactlist->getprops().size().x;
            if ( const theme_rect_s *tr = themerect() )
                neww += tr->clborder_x();
        }

        ts::irect defconvc( 0 );
        if ( mainconv )
        {
            defconvc = mainconv->getprops().screenrect();
            if ( const theme_rect_s *tr = themerect() )
            {
                ts::ivec2 s = tr->size_by_clientsize( defconvc.size(), false );
                defconvc = ts::irect::from_center_and_size( defconvc.center(), s );
                defconvc.lt.y = getprops().screenrect().lt.y;
                defconvc.rb.y = getprops().screenrect().rb.y;
                defconvc.lt.x += ts::tmax( tr->maxcutborder.lt.x, tr->maxcutborder.rb.x );
                defconvc.rb.x += ts::tmax( tr->maxcutborder.lt.x, tr->maxcutborder.rb.x );
            }

            cfg().param( CONSTASTR( "main_defconv" ), ts::amake<ts::irect>( defconvc ) );

            if ( contact_root_c *r = mainconv->get_selected_contact() )
                if ( !r->getkey().is_self() )
                    create_new_conv( r );
        }

        TSDEL( hg );
        g_app->contactlist = &MAKE_CHILD<gui_contactlist_c>( getrid() ).get();
        g_app->contactlist->leech( TSNEW( leech_fill_parent_s ) );
        contacts().update_meta();

        if (neww)
        {
            MODIFY( *this ).sizew( neww );
            rrect = getprops().screenrect();
        }

        onclosesave();

    } else
    {
        for ( guirect_c *ch : convs )
            if ( ch )
                TSDEL( ch );

        if ( getprops().is_collapsed() )
            MODIFY( *this ).decollapse();

        bool restore_size = false;
        int clw = 0;
        if ( g_app->contactlist )
        {
            clw = g_app->contactlist->getprops().size().x;
            TSDEL( g_app->contactlist ), restore_size = true;
            if ( const theme_rect_s *tr = themerect() )
                clw += tr->clborder_x();
        }

        auto uiroot = []( RID p )-> gui_hgroup_c *
        {
            gui_hgroup_c &g = MAKE_VISIBLE_CHILD<gui_hgroup_c>( p );
            g.allow_move_splitter( true );
            g.leech( TSNEW( leech_fill_parent_s ) );
            g.leech( TSNEW( leech_save_proportions_s, CONSTASTR( "main_splitter" ), CONSTASTR( "7060,12940" ) ) );
            return &g;
        };

        hg = uiroot( m_rid );
        g_app->contactlist = &MAKE_CHILD<gui_contactlist_c>( hg->getrid() ).get();
        mainconv = &MAKE_CHILD<gui_conversation_c>( hg->getrid() ).get();

        if ( restore_size )
        {
            ts::irect cr = cfg().get<ts::irect>( CONSTASTR( "main_defconv" ), ts::irect(0,0,300,300) );
            MODIFY( *this ).sizew( clw + cr.width() );
            rrect = getprops().screenrect();
            onclosesave();
        }
        hg->getrid().call_restore_signal();
        gmsg<ISOGM_SELECT_CONTACT>( &contacts().get_self(), 0 ).send(); // 1st selected item, yo

    }

    getengine().redraw();
}

void mainrect_c::rebuild_icons()
{
    if (const theme_rect_s *tr = themerect())
    {
        if (tr->captextadd.x >= 18)
        {
            int sz = tr->captextadd.x - 2;
            icons = g_app->build_icon(sz);
        }
    }
}

void mainrect_c::onclosesave()
{
    cfg().param(CONSTASTR("main_rect_pos"), ts::amake<ts::ivec2>(rrect.lt));
    cfg().param(CONSTASTR("main_rect_size"), ts::amake<ts::ivec2>(rrect.size()));
    cfg().param(CONSTASTR("main_rect_monitor"), ts::amake<ts::irect>(mrect));
    cfg().param( CONSTASTR( "main_rect_fs" ), getprops().is_maximized() ? "1" : "0" );
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

    if ( rid != getrid() )
    {
        return false;
    }

    if (__super::sq_evt(qp, rid, data)) return true;

	switch( qp )
	{
	case SQ_DRAW:
        if (const theme_rect_s *tr = themerect())
		if (icons.info().sz.x > 0)
        {
            ts::irect cr = tr->captionrect( getprops().currentszrect(), getprops().is_maximized() );
            int st = g_app->F_OFFLINE_ICON ? contact_online_state_check : contacts().get_self().get_ostate();
            ts::irect ir;
            ir.lt.x = 0;
            ir.rb.x = icons.info().sz.x;
            ir.lt.y = st * ir.rb.x;
            ir.rb.y = ir.lt.y + ir.rb.x;
            cr.lt.y += tr->captextadd.y;

            getengine().begin_draw();
            getengine().draw( cr.lt, icons.extbody(ir), true );
            getengine().end_draw();
        }
		break;
	}

    return false;
}
