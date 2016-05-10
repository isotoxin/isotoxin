#include "rectangles.h"

INLINE bool insidelt(int v, int a, int asz)
{
    return v >= a && v < (a + asz);
}

INLINE bool insiderb(int v, int a, int asz)
{
    return v < a && v >= (a - asz);
}


rectengine_c::rectengine_c()
{

}
rectengine_c::~rectengine_c() 
{
    ts::aint cnt = children.size();
    for(int i=0;i<cnt;++i)
    {
        if (rectengine_c *e = children.get(i))
            TSDEL(e);
    }

    if (gui) gui->delete_event(DELEGATE(this, cleanup_children)); // previous clear-loop can produce this event, so it should be deleted now

    if (guirect_c *r = rect_)
    {
        /* it's very very tricky
           do not repeat it at home

            rectengine should be deleted before guirect
            but
            guirect is owner of rectengine
            how it can be done?
        */

        if (&r->getengine() == this)
        {
            /*
                unique ptr to this rectengine must be cleared without call desctructor!
            */
            r->__spec_remove_engine();
        }
        TSDEL(r);
    }
}

void rectengine_c::cleanup_children_now()
{
    ts::aint oldchsz = children.size();
    for (ts::aint i = children.size() - 1; i >= 0; --i)
        if (children.get(i).expired()) children.remove_slow(i);
    if (oldchsz != children.size())
    {
        children_z_sorted.clear();
        evt_data_s d;
        d.values.count = children.size();
        getrect().sq_evt(SQ_CHILD_ARRAY_COUNT, getrid(), d);
    }
}

bool rectengine_c::cleanup_children(RID,GUIPARAM)
{
    cleanup_children_now();
    return true;
}

void rectengine_c::z_resort_children()
{
    if (children_z_sorted.size())
    {
        children_z_sorted.clear();
        gui->dirty_hover_data();
        redraw();
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, cleanup_children), nullptr);
    }

}

void rectengine_c::prepare_children_z_sorted()
{
    if (children_z_sorted.size() != children.size())
    {
        // start resorting
        children_z_sorted.clear();
        for(rectengine_c *e : children)
            if (e) children_z_sorted.add(e);
        children_z_sorted.invsort();
    }
}

bool rectengine_c::children_sort( fastdelegate::FastDelegate< bool (rectengine_c *, rectengine_c *) > swap_them )
{
    if (children.sort([&](rectengine_c *e1, rectengine_c *e2)->bool
    {
        if (e1 == e2) return false; // qsort requirement
        return !swap_them(e1, e2);
    }))
    {
        gui->dirty_hover_data();
        redraw();
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, cleanup_children), nullptr);
        return true;
    }
    return false;
}

void rectengine_c::child_move_to( ts::aint index, rectengine_c *e )
{
    ts::aint i = children.find(e);
    if (i != index)
    {
        children.set(i, nullptr);
        if (children.get(index))
            children.insert(index, e);
        else
            children.set(index, e);

        cleanup_children_now();
        gui->dirty_hover_data();
        redraw();
    }
}

ts::uint32 rectengine_c::detect_area(const ts::ivec2 &p)
{
    evt_data_s d;
    d.detectarea.area = 0;
    d.detectarea.pos = p;

    if (const theme_rect_s *themerect = getrect().themerect())
    {
        const rectprops_c &rps = getrect().getprops();
        ts::irect clr = themerect->hollowrect(rps);

        if (clr.inside(p))
        {
            if (rps.allow_resize())
            {
                if (insidelt(p.x, clr.lt.x, themerect->resizearea))
                    d.detectarea.area |= AREA_LEFT;

                if (insiderb(p.x, clr.rb.x, themerect->resizearea))
                    d.detectarea.area |= AREA_RITE;

                if (insidelt(p.y, clr.lt.y, themerect->resizearea))
                    d.detectarea.area |= AREA_TOP;

                if (insiderb(p.y, clr.rb.y, themerect->resizearea))
                    d.detectarea.area |= AREA_BOTTOM;
            }

            if (!d.detectarea.area)
                if (themerect->captionrect(rps.currentszrect(), rps.is_maximized()).inside(p))
                    d.detectarea.area |= rps.allow_move() ? AREA_CAPTION : AREA_CAPTION_NOMOVE;

        }

    }

    getrect().sq_evt(SQ_DETECT_AREA, getrid(), d);

    return d.detectarea.area;
}

void rectengine_c::mouse_lock()
{
    gui->mouse_lock( getrid() );
}
void rectengine_c::mouse_unlock()
{
    gui->mouse_lock( RID() );
}

void rectengine_c::trunc_children( ts::aint index)
{
    for (; index < children.size(); ++index)
    {
        rectengine_c *c = children.get(index);
        if (c) TSDEL(c);
    }
    cleanup_children_now();
    redraw();
}

void rectengine_c::add_child(rectengine_c *re, RID after)
{
    if (after)
    {
        ts::aint cnt = children.size();
        for(int i=0;i<cnt;++i)
        {
            rectengine_c *c = children.get(i);
            if (c && c->getrid() == after)
            {
                children.insert(i+1,re);
                break;
            }
        }

    } else
    {
        children.add(re);
    }
}

rectengine_c *rectengine_c::get_last_child()
{
    for( ts::aint i=children.size() - 1; i>=0; --i)
        if (rectengine_c *e = children.get(i))
            return e;
    return nullptr;
}

/*virtual*/ bool rectengine_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
	switch(qp)
	{
    case SQ_RECT_CHANGED: // dont pass through children
        if (guirect_c *r = rect())
        {
            ASSERT(r->getrid() == rid);
            r->sq_evt(qp, r->getrid(), data);
        }

        // notify children
        for (rectengine_c *c : children)
            if (c) c->sq_evt(SQ_PARENT_RECT_CHANGED, c->getrid(), data);

        return true;
	case SQ_MOUSE_MOVE:
		if (const mousetrack_data_s *mtd = gui->mtrack( getrid(), MTT_RESIZE | MTT_MOVE ))
		{
            manual_move_resize( true );

			evt_data_s d;
			d.rectchg.rect = mtd->rect;
			d.rectchg.area = mtd->area;
            d.rectchg.apply = true;

			ts::ivec2 delta = data.mouse.screenpos.get() - mtd->mpos;

            if (mtd->mtt & MTT_RESIZE)
            {
			    if ( mtd->area & AREA_TOP )
				    d.rectchg.rect.get().lt.y += delta.y;
			    if ( mtd->area & AREA_BOTTOM )
				    d.rectchg.rect.get().rb.y += delta.y;
			    if ( mtd->area & AREA_LEFT )
				    d.rectchg.rect.get().lt.x += delta.x;
			    if ( mtd->area & AREA_RITE )
				    d.rectchg.rect.get().rb.x += delta.x;
            } else if (mtd->mtt & MTT_MOVE)
            {
                d.rectchg.rect.get() += delta;
            }
			sq_evt(SQ_RECT_CHANGING, getrid(), d);

            manual_move_resize( false );

		} else if (gui->mtrack(getrid(), MTT_ANY))
            if (guirect_c *r = rect())
                r->sq_evt(SQ_MOUSE_MOVE_OP, r->getrid(), data);
		break;

	case SQ_MOUSE_LDOWN:
		{
            const hover_data_s &hd = gui->get_hoverdata(data.mouse.screenpos);
			if (hd.rid == getrid() && !getrect().getprops().is_maximized())
            {
                if (0 != (hd.area & AREA_RESIZE) && 0 == (hd.area & AREA_NORESIZE))
			    {
                    gui->set_focus(hd.rid);
				    sq_evt(SQ_RESIZE_START, getrid(), ts::make_dummy<evt_data_s>(true));
                    mousetrack_data_s & oo = gui->begin_mousetrack(rid, MTT_RESIZE);
				    oo.area = hd.area;
				    oo.rect = getrect().getprops().rect();
				    oo.mpos = data.mouse.screenpos;
				    return true;
			    }
			    if (0 != (hd.area & AREA_MOVE) && 0 == (hd.area & AREA_NOMOVE))
			    {
                    gui->set_focus(hd.rid);
				    sq_evt(SQ_MOVE_START, getrid(), ts::make_dummy<evt_data_s>(true));
                    mousetrack_data_s & oo = gui->begin_mousetrack(rid, MTT_MOVE);
				    oo.area = hd.area;
				    oo.rect = getrect().getprops().rect();
				    oo.mpos = data.mouse.screenpos;
				    return true;
			    }
            }
		}
		return false;
	case SQ_MOUSE_LUP:
		if (gui->end_mousetrack( getrid(), MTT_RESIZE ))
		{
			sq_evt(SQ_RESIZE_END, getrid(), ts::make_dummy<evt_data_s>(true));
			return true;
		} else if (gui->end_mousetrack( getrid(), MTT_MOVE ))
		{
			sq_evt(SQ_MOVE_END, getrid(), ts::make_dummy<evt_data_s>(true));
			return true;
		}
		return false;
    case SQ_MOUSE_RDOWN:
    case SQ_MOUSE_RUP:
    case SQ_MOUSE_MDOWN:
    case SQ_MOUSE_MUP:
    case SQ_MOUSE_L2CLICK:
        return false;
    case SQ_CHILD_CREATED:
        z_resort_children();
        break;
    case SQ_CHILD_DESTROYED:
        data.rect.index = (int)child_index( data.rect.id );
        if (data.rect.index>=0) children.get(data.rect.index) = nullptr;
        DEFERRED_UNIQUE_CALL(0,DELEGATE(this,cleanup_children),nullptr);
        break;
	}

    if (guirect_c *r = rect())
        if (ASSERT(r->getrid() == rid))
            return r->sq_evt(qp, rid, data);

    return false;
}

/*virtual*/ bool rectengine_root_c::my_wnd_s::evt_specialborder( ts::specialborder_s bd[ 4 ] )
{
    const theme_rect_s *thr = owner()->getrect().themerect();
    if ( thr && thr->specialborder() && thr->maxcutborder != ts::irect( 0 ) )
    {
        const rectprops_c &rps = owner()->getrect().getprops();
        ts::irect sr = rps.screenrect();

        // SPB_LEFT:
        bd[ ts::SPB_LEFT ].img = thr->src;
        bd[ ts::SPB_LEFT ].s = thr->sis[ SI_LEFT_TOP ];
        bd[ ts::SPB_LEFT ].r = thr->sis[ SI_LEFT ];
        bd[ ts::SPB_LEFT ].e = thr->sis[ SI_LEFT_BOTTOM ];

        bd[ ts::SPB_LEFT ].s.rb.x = bd[ ts::SPB_LEFT ].s.lt.x + thr->maxcutborder.lt.x;
        bd[ ts::SPB_LEFT ].s.lt.y += thr->maxcutborder.lt.y;

        bd[ ts::SPB_LEFT ].r.rb.x = bd[ ts::SPB_LEFT ].r.lt.x + thr->maxcutborder.lt.x;
        bd[ ts::SPB_LEFT ].e.rb.x = bd[ ts::SPB_LEFT ].e.lt.x + thr->maxcutborder.lt.x;
        bd[ ts::SPB_LEFT ].e.rb.y -= thr->maxcutborder.rb.y;
        
        // SPB_TOP:
        bd[ ts::SPB_TOP ].img = thr->src;
        bd[ ts::SPB_TOP ].s = thr->sis[ SI_LEFT_TOP ];
        bd[ ts::SPB_TOP ].r = thr->sis[ SI_TOP ];
        bd[ ts::SPB_TOP ].e = thr->sis[ SI_RIGHT_TOP ];

        bd[ ts::SPB_TOP ].r.rb.y = bd[ ts::SPB_TOP ].r.lt.y + thr->maxcutborder.lt.y;
        bd[ ts::SPB_TOP ].s.rb.y = bd[ ts::SPB_TOP ].s.lt.y + thr->maxcutborder.lt.y;
        bd[ ts::SPB_TOP ].e.rb.y = bd[ ts::SPB_TOP ].e.lt.y + thr->maxcutborder.lt.y;

        // SPB_RIGHT:
        bd[ ts::SPB_RIGHT ].img = thr->src;
        bd[ ts::SPB_RIGHT ].s = thr->sis[ SI_RIGHT_TOP ];
        bd[ ts::SPB_RIGHT ].r = thr->sis[ SI_RIGHT ];
        bd[ ts::SPB_RIGHT ].e = thr->sis[ SI_RIGHT_BOTTOM ];
        
        bd[ ts::SPB_RIGHT ].r.lt.x = bd[ ts::SPB_RIGHT ].r.rb.x - thr->maxcutborder.rb.x;
        bd[ ts::SPB_RIGHT ].s.lt.x = bd[ ts::SPB_RIGHT ].s.rb.x - thr->maxcutborder.rb.x;
        bd[ ts::SPB_RIGHT ].s.lt.y += thr->maxcutborder.lt.y;

        bd[ ts::SPB_RIGHT ].e.lt.x = bd[ ts::SPB_RIGHT ].e.rb.x - thr->maxcutborder.rb.x;
        bd[ ts::SPB_RIGHT ].e.rb.y -= thr->maxcutborder.rb.y;

        // SPB_BOTTOM:
        bd[ ts::SPB_BOTTOM ].img = thr->src;
        bd[ ts::SPB_BOTTOM ].s = thr->sis[ SI_LEFT_BOTTOM ];
        bd[ ts::SPB_BOTTOM ].r = thr->sis[ SI_BOTTOM ];
        bd[ ts::SPB_BOTTOM ].e = thr->sis[ SI_RIGHT_BOTTOM ];

        bd[ ts::SPB_BOTTOM ].r.lt.y = bd[ ts::SPB_BOTTOM ].r.rb.y - thr->maxcutborder.rb.y;
        bd[ ts::SPB_BOTTOM ].s.lt.y = bd[ ts::SPB_BOTTOM ].s.rb.y - thr->maxcutborder.rb.y;
        bd[ ts::SPB_BOTTOM ].e.lt.y = bd[ ts::SPB_BOTTOM ].e.rb.y - thr->maxcutborder.rb.y;

        return true;
    }


    return false;
}

system_query_e me2sq( ts::mouse_event_e me )
{
    switch ( me )
    {
    case ts::MEVT_MOVE: return SQ_MOUSE_MOVE;
    case ts::MEVT_LDOWN: return SQ_MOUSE_LDOWN;
    case ts::MEVT_LUP: return SQ_MOUSE_LUP;
    case ts::MEVT_L2CLICK: return SQ_MOUSE_L2CLICK;
    case ts::MEVT_RDOWN: return SQ_MOUSE_RDOWN;
    case ts::MEVT_RUP: return SQ_MOUSE_RUP;
    case ts::MEVT_MDOWN: return SQ_MOUSE_MDOWN;
    case ts::MEVT_MUP: return SQ_MOUSE_MUP;
    }
    FORBIDDEN();
    return SQ_NOP;
}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_mouse( ts::mouse_event_e me, const ts::ivec2 &clpos /* relative to wnd */, const ts::ivec2 &scrpos )
{
    redraw_collector_s dcl;

    bool act = me == ts::MEVT_LDOWN;
    if ( gui->allow_input( owner()->getrid(), act ) )
    {
        evt_data_s d;
        d.mouse.screenpos = scrpos;
        owner()->sq_evt( me2sq(me), owner()->getrid(), d );
    }
    else if ( act )
    {
        //if (RID exl = gui->get_exclusive())
        //{
        //    ts::sys_beep(ts::SBEEP_BADCLICK);
        //}
    }
}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_mouse_out()
{
    gui->mouse_outside();
}

/*virtual*/ bool rectengine_root_c::my_wnd_s::evt_mouse_activate()
{
    if ( !gui->allow_input( owner()->getrid(), true ) )
    {
        if ( RID r = gui->get_exclusive() )
        {
            ts::master().sys_beep( ts::SBEEP_BADCLICK );
            if ( rectengine_root_c *root = HOLD( r )( ).getroot() )
                root->shake();
        }
        return false;
    }
    return true;

}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_notification_icon( ts::notification_icon_action_e a )
{
    gui->app_notification_icon_action( a, owner()->getrid() );
}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_refresh_pos( const ts::irect &scr, ts::disposition_e d )
{
    if ( owner()->is_dip() )
        return;

    if ( guirect_c *r = owner()->rect() )
    {
        if ( ts::D_RESTORE == d )
        {
            if ( !r->inmod() )
                MODIFY( *r ).decollapse();
        } else if ( ts::D_MIN == d )
        {
            if ( !r->inmod() )
                MODIFY( *r ).minimize( true );
        } else if ( ts::D_MAX == d )
        {
            if ( !r->inmod() )
                MODIFY( *r ).maximize( scr );

            drawcollector dch( owner() );
            owner()->redraw();

        } else if ( r->getprops().screenrect() != scr )
        {
            MODIFY( *r ).pos( scr.lt ).size( scr.size() );
        }
    }
}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_focus_changed( ts::wnd_c *w )
{
    if ( !w )
    {
        if ( owner()->getrid() >>= gui->get_focus() )
            gui->set_focus( RID() );
    } else
    {
        my_wnd_s *wndc = ts::ptr_cast<my_wnd_s *>( w->get_callbacks() );

        RID f = wndc->owner()->getrid();
        gui->set_focus( f );

        if (!gm_receiver_c::in_progress( GM_UI_EVENT ))
            gmsg<GM_UI_EVENT>( UE_ACTIVATE ).send();
    }
}

/*virtual*/ bool rectengine_root_c::my_wnd_s::evt_on_file_drop( const ts::wstr_c& fn, const ts::ivec2 &clp )
{
    if ( gmsg<GM_DROPFILES>( owner()->getrid(), fn, clp ).send().is( GMRBIT_ABORT ) )
        return false;

    return true;
}

/*virtual*/ void rectengine_root_c::my_wnd_s::evt_draw()
{
    if ( owner()->is_dip() )
        return;

    guirect_c &r = owner()->getrect();

    /*
    if ( force_redraw )
    {
        owner()->redraw(); // we need it! see redraw tag

        draw_data_s &dd = owner()->begin_draw();
        ASSERT( dd.offset == ts::ivec2( 0 ) );
        dd.size = r.getprops().currentsize();

        ASSERT( dd.cliprect.size() >> dd.size );
        evt_data_s d = evt_data_s::draw_s( owner()->drawtag );
        owner()->sq_evt( SQ_DRAW, owner()->getrid(), d );
        owner()->end_draw();

    } else
        */
    {
        evt_data_s d( evt_data_s::draw_s( owner()->drawtag ) );
        owner()->sq_evt( SQ_DRAW, r.getrid(), d );
    }

}


/*virtual*/ ts::bitmap_c rectengine_root_c::my_wnd_s::app_get_icon(bool for_tray)
{
    return gui->app_icon( for_tray );
}

/*virtual*/ ts::irect rectengine_root_c::my_wnd_s::app_get_redraw_rect()
{
    return owner()->redraw_rect;
}

void rectengine_root_c::my_wnd_s::kill()
{
    TSDEL( wnd );
}

rectengine_root_c::rectengine_root_c(bool sys)
{
    redraw_rect = ts::irect( maximum<int>::value, minimum<int>::value );
    flags.init( F_SYSTEM, sys );
    drawntag = drawtag - 1;

}

rectengine_root_c::~rectengine_root_c() 
{
    flags.set( F_DIP );
    //if (gui) gui->delete_event( DELEGATE(this, refresh_frame) );
    syswnd.kill();
}

/*virtual*/ bool rectengine_root_c::apply(rectprops_c &rpss, const rectprops_c &pss)
{
	if (!syswnd.wnd && (pss.is_visible() || pss.is_micromized()))
	{
        rpss.change_to(pss, this);

        struct shp_s : public ts::wnd_show_params_s
        {
            rectengine_root_c *engine;

            /*virtual*/ void apply( bool update_frame, bool force_apply_now ) override
            {

            }

        } shp;

        if ( !pss.is_micromized() )
            shp.visible = true;

        if ( !ts::master().mainwindow )
            shp.mainwindow = true;

        if ( pss.is_alphablend() )
            shp.layered = true;

        shp.acceptfiles = true;

        if ( pss.is_minimized() )
            shp.d = ts::D_MIN;

        if ( pss.is_micromized() )
            shp.d = ts::D_MICRO;

        if ( pss.is_maximized() && shp.d == ts::D_NORMAL )
            shp.d = ts::D_MAX;

        shp.parent = ts::master().mainwindow;
        if ( flags.is( F_SYSTEM ) )
        {
            ASSERT( !pss.is_micromized() );
            shp.parent = nullptr;
            shp.toolwindow = true;
        }

        {
            evt_data_s d;
            if ( getrect().sq_evt( SQ_GET_ROOT_PARENT, getrid(), d ) )
                shp.parent = ts::ptr_cast<rectengine_root_c *>( &HOLD( d.rect.id ).engine() )->syswnd.wnd;
        }


        //RID prev;
        //for( RID rr : gui->roots() )
        //{
        //    if (rr == getrid()) break;
        //    prev = rr;
        //}
        //if (prev) prnt = ts::ptr_cast<rectengine_root_c *>(&HOLD(prev).engine())->hwnd;


        ts::str_c name = to_utf8( getrect().get_name() );
        text_remove_tags( name );
        shp.name = from_utf8( name );
        shp.rect = rpss.screenrect(false);

        shp.focus = getrect().accept_focus();

        syswnd.wnd = ts::wnd_c::build( &syswnd );
        syswnd.wnd->show( &shp );

        if (pss.is_alphablend())
        {
            draw_data_s &dd = begin_draw();
            ASSERT( dd.offset == ts::ivec2(0) );
            dd.size = rpss.currentsize();
            ASSERT( dd.cliprect.size() >> dd.size );
            evt_data_s d = evt_data_s::draw_s(drawtag);
            sq_evt(SQ_DRAW, getrid(), d);
            end_draw();
        }

        if (!pss.is_micromized())
        {
            if ( getrect().accept_focus() )
                gui->set_focus(getrid()); // root rect stole focus

            if (gui->get_rootfocus() == getrid())
                syswnd.wnd->set_focus(false);
        }

	} else if (!pss.is_visible())
	{
		// window stil invisible => just change parameters
		rpss.change_to(pss, this);
        syswnd.kill();
	} else
	{
		// change exist window

        struct shp_s : public ts::wnd_show_params_s
        {
            rectprops_c &rpss;
            const rectprops_c &pss;
            rectengine_root_c *engine;

            /*virtual*/ void apply( bool update_frame, bool force_apply_now ) override
            {
                if ( force_apply_now || !pss.is_alphablend() )
                {
                    rpss.change_to( pss, engine );

                    if ( update_frame )
                    {
                        draw_data_s &dd = engine->begin_draw();
                        ASSERT( dd.offset == ts::ivec2( 0 ) );
                        dd.size = rpss.currentsize();
                        ASSERT( dd.cliprect.size() >> dd.size );
                        evt_data_s d = evt_data_s::draw_s( engine->drawtag );
                        engine->sq_evt( SQ_DRAW, engine->getrid(), d );
                        engine->end_draw();
                    }
                }
                else
                {
                    engine->begin_draw();
                    engine->end_draw();
                }

            };

            shp_s( rectprops_c &rpss, const rectprops_c &pss, rectengine_root_c *engine ):rpss(rpss), pss(pss), engine(engine) {}

        } shp( rpss, pss, this );

        shp.d = ts::D_NORMAL;

        if ( pss.is_maximized() )
            shp.d = ts::D_MAX;
        if ( pss.is_minimized() )
            shp.d = ts::D_MIN;
        if ( pss.is_micromized() )
            shp.d = ts::D_MICRO;

        shp.layered = pss.is_alphablend();
        shp.rect = pss.screenrect(false);
        shp.visible = true;
        ts::disposition_e odp = syswnd.wnd->disposition();
        syswnd.wnd->show( &shp );

        if ( odp != shp.d )

        switch ( shp.d )
        {
        case ts::D_MAX:
            gmsg<GM_UI_EVENT>(UE_MAXIMIZED).send();
            //refresh_frame();
            DMSG("maximized");
            break;
        case ts::D_NORMAL:
            gmsg<GM_UI_EVENT>(UE_NORMALIZED).send();
            //refresh_frame();
            DMSG("restored");
            break;
        case ts::D_MIN:
        case ts::D_MICRO:
            gmsg<GM_UI_EVENT>(UE_MINIMIZED).send();
            DMSG("minimized");
            break;
        }

	}
    return false;
}

/*virtual*/ void rectengine_root_c::redraw(const ts::irect *invalidate_rect)
{
    if(!flags.is(F_REDRAW_COLLECTOR))
    {
        DMSG("oops F_REDRAW_CHECKER");
    }
    redraw_rect.combine( invalidate_rect ? *invalidate_rect : getrect().getprops().currentszrect() );
    drawcollector dch( this );
    ++drawtag;
}

void rectengine_root_c::redraw_now()
{
    if (syswnd.wnd == nullptr || rect() == nullptr) return;

    ASSERT(drawdata.size() == 0);
    draw_data_s &dd = begin_draw();

    ASSERT(dd.offset == ts::ivec2(0));
    dd.size = getrect().getprops().currentsize();
    dd.cliprect = redraw_rect;
    redraw_rect = dd.cliprect.intersect( ts::irect(0, dd.size) );
    evt_data_s d = evt_data_s::draw_s(drawtag);
    sq_evt(SQ_DRAW, getrid(), d);
    end_draw();
    redraw_rect = ts::irect( maximum<int>::value, minimum<int>::value );
}

/*virtual*/ draw_data_s & rectengine_root_c::begin_draw()
{
    draw_data_s &r = drawdata.duplast();
    r.engine = this;
    return r;
}
/*virtual*/ void rectengine_root_c::end_draw()
{
    if (ASSERT( drawdata.size() > 0))
	drawdata.truncate( drawdata.size() - 1 );
	if (drawdata.size() == 0)
    {
        const rectprops_c &rps = getrect().getprops();
        syswnd.wnd->flush_draw( rps.screenrect() );
    }
}

/*virtual*/ void rectengine_root_c::draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d )
{
    if (drawdata.size() == 0) return;
	const rectprops_c &rps = getrect().getprops();
	const draw_data_s &dd = drawdata.last();
    bool self_draw = dd.engine == this;
    bool use_alphablend = !self_draw; //!flags.is(F_SELF_DRAW);

    ts::bmpcore_exbody_s bb = syswnd.wnd->get_backbuffer();
    ts::bitmap_t<ts::bmpcore_exbody_s> backbuffer(bb);
    ts::bmpcore_exbody_s src = thr.src.extbody();
    ts::repdraw rdraw( bb, src, dd.cliprect, dd.alpha );
    ts::ivec2 rbpt = dd.offset + dd.size;

    ts::irect fillrect;

    auto filler = [&](ts::TSCOLOR c)
    {
        if (fillrect.intersect(dd.cliprect))
        {
            if (use_alphablend && ts::ALPHA(c) < 255)
                backbuffer.overfill(fillrect.lt, fillrect.size(), ts::PREMULTIPLY(c));
            else
                backbuffer.fill(fillrect.lt, fillrect.size(), c);
        }
    };

    auto fillc = [&]( const ts::irect &r, subimage_e si )
    {
        if (0 != ts::ALPHA(thr.siso[si].fillcolor))
        {
            fillrect.lt = r.lt + thr.sis[si].lt;
            fillrect.rb = r.rb - thr.sis[si].rb;
            filler(thr.siso[si].fillcolor);
        }
        else
        {
            rdraw.rrep = thr.sis + si; rdraw.a_rep = use_alphablend && thr.is_alphablend(si);

            if (!*rdraw.rrep)
            {
                si = SI_BASE;
                rdraw.rrep = thr.sis + si;
                rdraw.a_beg = use_alphablend && thr.is_alphablend(si);
            }

            rdraw.draw_c(r.lt.x, r.rb.x, r.lt.y, r.rb.y, thr.siso[si].tile);
        }
    };

    if (options & DTHRO_BASE)
    {
        fillc( ts::irect(dd.offset, rbpt), SI_BASE);

    } else if (0 != (options & DTHRO_BASE_HOLE) && d && ASSERT(thr.sis[SI_BASE] || 0 != ts::ALPHA(thr.siso[SI_BASE].fillcolor)))
    {
        if (0 != ts::ALPHA(thr.siso[SI_BASE].fillcolor))
        {
            // top
            fillrect.lt = dd.offset;
            fillrect.rb.x = rbpt.x;
            fillrect.rb.y = d->draw_thr.rect().lt.y + dd.offset.y;
            filler(thr.siso[SI_BASE].fillcolor);

            // left
            fillrect.lt.x = dd.offset.x;
            fillrect.lt.y = d->draw_thr.rect().lt.y + dd.offset.y;
            fillrect.rb = d->draw_thr.rect().lb() + dd.offset;
            filler(thr.siso[SI_BASE].fillcolor);

            // rite
            fillrect.lt = d->draw_thr.rect().rt() + dd.offset;
            fillrect.rb.x = rbpt.x;
            fillrect.rb.y = d->draw_thr.rect().rb.y + dd.offset.y;
            filler(thr.siso[SI_BASE].fillcolor);

            // bottom
            fillrect.lt.x = dd.offset.x;
            fillrect.lt.y = d->draw_thr.rect().rb.y + dd.offset.y;
            fillrect.rb = rbpt;
            filler(thr.siso[SI_BASE].fillcolor);

        } else
        {
            rdraw.rbeg = nullptr;
            rdraw.rrep = thr.sis + SI_BASE; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BASE);
            rdraw.rend = nullptr;

            int y = dd.offset.y;
            for (int ylmt = d->draw_thr.rect().lt.y + dd.offset.y; y < ylmt; y += rdraw.rrep->height())
                rdraw.draw_h(dd.offset.x, rbpt.x, y, thr.siso[SI_BASE].tile);

            int y2 = rbpt.y;
            for (int ylmt = d->draw_thr.rect().rb.y + dd.offset.y; y2 >= ylmt;)
                rdraw.draw_h(dd.offset.x, rbpt.x, y2 -= rdraw.rrep->height(), thr.siso[SI_BASE].tile);

            for (int x = dd.offset.x, xlmt = d->draw_thr.rect().lt.x + dd.offset.x; x < xlmt; x += rdraw.rrep->width())
                rdraw.draw_v(x, y, y2, thr.siso[SI_BASE].tile);

            for (int x2 = rbpt.x, xlmt = d->draw_thr.rect().rb.x + dd.offset.x; x2 >= xlmt;)
                rdraw.draw_v(x2 -= rdraw.rrep->width(), y, y2, thr.siso[SI_BASE].tile);
        }

    }

    if (options & (DTHRO_BORDER|DTHRO_BORDER_RECT|DTHRO_CENTER|DTHRO_CENTER_HOLE))
    {
        ASSERT( 0 == (options & (DTHRO_LEFT_CENTER|DTHRO_LT_T_RT|DTHRO_LB_B_RB)) );

        ts::irect trects[SI_count];
        const ts::irect *lt = thr.sis + SI_LEFT_TOP;
        const ts::irect *lb = thr.sis + SI_LEFT_BOTTOM;
        const ts::irect *rt = thr.sis + SI_RIGHT_TOP;
        const ts::irect *rb = thr.sis + SI_RIGHT_BOTTOM;

        const ts::irect *l = thr.sis + SI_LEFT;
        const ts::irect *t = thr.sis + SI_TOP;
        const ts::irect *r = thr.sis + SI_RIGHT;
        const ts::irect *b = thr.sis + SI_BOTTOM;

        if ((rps.is_maximized() || thr.fastborder()) && self_draw)
        {
            trects[SI_LEFT_TOP] = *lt; lt = trects + SI_LEFT_TOP; trects[SI_LEFT_TOP].lt += thr.maxcutborder.lt;
            trects[SI_LEFT_BOTTOM] = *lb; lb = trects + SI_LEFT_BOTTOM; trects[SI_LEFT_BOTTOM].lt.x += thr.maxcutborder.lt.x; trects[SI_LEFT_BOTTOM].rb.y -= thr.maxcutborder.rb.y;
            trects[SI_RIGHT_TOP] = *rt; rt = trects + SI_RIGHT_TOP; trects[SI_RIGHT_TOP].rb.x -= thr.maxcutborder.rb.x; trects[SI_RIGHT_TOP].lt.y += thr.maxcutborder.lt.y;
            trects[SI_RIGHT_BOTTOM] = *rb; rb = trects + SI_RIGHT_BOTTOM; trects[SI_RIGHT_BOTTOM].rb -= thr.maxcutborder.rb;

            trects[SI_LEFT] = *l; l = trects + SI_LEFT; trects[SI_LEFT].lt.x += thr.maxcutborder.lt.x;
            trects[SI_TOP] = *t; t = trects + SI_TOP; trects[SI_TOP].lt.y += thr.maxcutborder.lt.y;
            trects[SI_RIGHT] = *r; r = trects + SI_RIGHT; trects[SI_RIGHT].rb.x -= thr.maxcutborder.rb.x;
            trects[SI_BOTTOM] = *b; b = trects + SI_BOTTOM; trects[SI_BOTTOM].rb.y -= thr.maxcutborder.rb.y;

            use_alphablend = false; // in maximized mode no alphablend

        }
        if (0 != (options & DTHRO_CENTER) && 0 != ts::ALPHA(thr.siso[SI_CENTER].fillcolor))
        {
            // center color fill
            // do it before draw border due border can overdraw filled rect

            fillrect.lt = dd.offset + thr.sis[SI_CENTER].lt;
            fillrect.rb = rbpt - thr.sis[SI_CENTER].rb;
            filler( thr.siso[SI_CENTER].fillcolor );

            if (0 != ts::ALPHA(thr.siso[SI_CENTER].filloutcolor))
            {
                if ( thr.sis[SI_CENTER].lt.y > 0 )
                {
                    // top part
                    fillrect.lt = dd.offset;
                    fillrect.rb = rbpt + ts::ivec2(0, thr.sis[SI_CENTER].lt.y);
                    filler(thr.siso[SI_CENTER].filloutcolor);
                }
                if (thr.sis[SI_CENTER].rb.y > 0)
                {
                    // bottom part
                    fillrect.lt.x = dd.offset.x;
                    fillrect.lt.y = rbpt.y - thr.sis[SI_CENTER].rb.y;
                    fillrect.rb = rbpt;
                    filler(thr.siso[SI_CENTER].filloutcolor);
                }
                if (thr.sis[SI_CENTER].lt.x > 0)
                {
                    // left part
                    fillrect.lt = dd.offset + ts::ivec2(0, thr.sis[SI_CENTER].lt.y);
                    fillrect.rb.x = dd.offset.x + thr.sis[SI_CENTER].lt.x;
                    fillrect.rb.y = rbpt.y - thr.sis[SI_CENTER].lt.y;
                    filler(thr.siso[SI_CENTER].filloutcolor);
                }
                if (thr.sis[SI_CENTER].rb.x > 0)
                {
                    // rite part
                    fillrect.lt.x = rbpt.x - thr.sis[SI_CENTER].lt.x;
                    fillrect.lt.y = dd.offset.y + thr.sis[SI_CENTER].lt.y;
                    fillrect.rb.x = rbpt.x;
                    fillrect.rb.y = rbpt.y - thr.sis[SI_CENTER].lt.y;
                    filler(thr.siso[SI_CENTER].filloutcolor);
                }
            }
        }

        if (options & (DTHRO_BORDER|DTHRO_BORDER_RECT))
        {
            ts::irect brect;
            if ( options & DTHRO_BORDER_RECT )
                brect = d->draw_thr.rect, use_alphablend = true;
            else
                brect.lt = dd.offset, brect.rb = rbpt;

            // top
            enum class drawn_e
            {
                no,
                partial,
                full
            } lt_drawn = drawn_e::no;

            bool rt_drawn = false;
            if (*t)
            {
                int x1 = brect.rb.x;
                ts::irect lt_crop;

                if (lt->height() > t->height())
                {
                    lt_drawn = drawn_e::partial;
                    lt_crop = *lt; lt_crop.setheight( t->height() );
                    rdraw.rbeg = &lt_crop; //-V506
                    rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP);
                } else 
                {
                    lt_drawn = drawn_e::full;
                    rdraw.rbeg = lt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP);
                }

                rdraw.rrep = t; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_TOP);
                if (rt->height() == t->height())
                {
                    rdraw.rend = rt; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_TOP);
                    rt_drawn = true;
                } else
                {
                    rdraw.rend = nullptr;
                    x1 -= rt->width();
                }
                rdraw.draw_h(brect.lt.x, x1, brect.lt.y, thr.siso[SI_TOP].tile);
            }

            // bottom
            bool bottom_drawn;
            if (false != (bottom_drawn = *b))
            {
                rdraw.rbeg = lb; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_BOTTOM);
                rdraw.rrep = b; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BOTTOM);
                rdraw.rend = rb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_BOTTOM);
                rdraw.draw_h(brect.lt.x, brect.rb.x, brect.rb.y - b->height(), thr.siso[SI_BOTTOM].tile);
            }

            // left
            if (*l)
            {
                int y0 = brect.lt.y;
                int y1 = brect.rb.y;

                ts::irect lt_crop;
                switch(lt_drawn)
                {
                case drawn_e::no:
                case drawn_e::partial:
                    rdraw.rbeg = lt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP);
                    if (lt->width() > l->width())
                        lt_crop = *lt, lt_crop.setwidth(l->width()), rdraw.rbeg = &lt_crop;
                    break;
                case drawn_e::full:
                    rdraw.rbeg = nullptr; y0 += lt->height();
                    break;
                }
                rdraw.rrep = l; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_LEFT);
                if (bottom_drawn) { rdraw.rend = nullptr;  y1 -= lb->height(); } else { rdraw.rend = lb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_LEFT_BOTTOM); }
                rdraw.draw_v(brect.lt.x, y0, y1, thr.siso[SI_LEFT].tile);
            }

            // right
            if (*r)
            {
                int y0 = brect.lt.y;
                int y1 = brect.rb.y;

                if (rt_drawn) { rdraw.rbeg = nullptr;  y0 += rt->height(); } else { rdraw.rbeg = rt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_RIGHT_TOP); }
                rdraw.rrep = r; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_RIGHT);
                if (bottom_drawn) { rdraw.rend = nullptr; y1 -= rb->height(); } else { rdraw.rend = rb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_BOTTOM); }
                rdraw.draw_v(brect.rb.x - r->width(), y0, y1, thr.siso[SI_RIGHT].tile);
            }

        }
        if (0 != (options & DTHRO_CENTER))
        {
            fillc( ts::irect(dd.offset.x + l->width(), dd.offset.y + t->height(), rbpt.x - r->width(), rbpt.y - b->height()), SI_CENTER );

        } else if (0 != (options & DTHRO_CENTER_HOLE) && d)
        {
            // draw center with hole (faster)

            rdraw.rrep = thr.sis + SI_CENTER; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_CENTER);

            int y = dd.offset.y + t->height();
            for (int ylim = d->draw_thr.rect().lt.y + dd.offset.y; y < ylim; y += rdraw.rrep->height())
                rdraw.draw_h(dd.offset.x + l->width(), rbpt.x - r->width(), y, thr.siso[SI_CENTER].tile);

            int y2 = rbpt.y - b->height();
            for (int ylim = d->draw_thr.rect().rb.y + dd.offset.y; y2 >= ylim;)
                rdraw.draw_h(dd.offset.x + l->width(), rbpt.x - r->width(), y2 -= rdraw.rrep->height(), thr.siso[SI_CENTER].tile);

            for (int x = dd.offset.x + l->width(), xlim = d->draw_thr.rect().lt.x + dd.offset.x; x < xlim; x += rdraw.rrep->width())
                rdraw.draw_v(x, y, y2, thr.siso[SI_CENTER].tile);

            for (int x2 = rbpt.x - r->width(), xlim = d->draw_thr.rect().rb.x + dd.offset.x; x2 >= xlim;)
                rdraw.draw_v(x2 -= rdraw.rrep->width(), y, y2, thr.siso[SI_CENTER].tile);

        }
    } else if (options & DTHRO_LEFT_CENTER)
    {
        ASSERT( 0 == (options & (DTHRO_BORDER|DTHRO_CENTER|DTHRO_CENTER_HOLE|DTHRO_LT_T_RT|DTHRO_LB_B_RB)) );

        rdraw.rbeg = nullptr;
        rdraw.rrep = thr.sis + SI_LEFT; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_LEFT);
        rdraw.rend = nullptr;
        rdraw.draw_v(dd.offset.x, dd.offset.y, rbpt.y, thr.siso[SI_LEFT].tile);

        fillc( ts::irect(dd.offset.x + thr.sis[SI_LEFT].width(), dd.offset.y, rbpt.x, rbpt.y), SI_CENTER );

        if (options & DTHRO_BOTTOM)
        {
            rdraw.rbeg = nullptr;
            rdraw.rrep = thr.sis + SI_BOTTOM; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BOTTOM);
            rdraw.rend = nullptr;
            rdraw.draw_h(dd.offset.x + thr.sis[SI_LEFT].width(), rbpt.x, (dd.offset.y + rbpt.y) / 2, thr.siso[SI_BOTTOM].tile);
        }
        if (options & DTHRO_RIGHT)
        {
            render_image( bb, src, rbpt.x - thr.sis[SI_RIGHT].width(), (dd.offset.y + rbpt.y - thr.sis[SI_RIGHT].height())/2, thr.sis[SI_RIGHT], dd.cliprect, use_alphablend && thr.is_alphablend(SI_RIGHT) ? dd.alpha : -1 );
        }
    } else if (options & DTHRO_LT_T_RT)
    {
        ASSERT(0 == (options & (DTHRO_BORDER | DTHRO_CENTER | DTHRO_CENTER_HOLE | DTHRO_LEFT_CENTER)));

        rdraw.rbeg = thr.sis + SI_LEFT_TOP; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP);
        rdraw.rrep = thr.sis + SI_TOP; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_TOP);
        rdraw.rend = thr.sis + SI_RIGHT_TOP; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_TOP);
        rdraw.draw_h(dd.offset.x, rbpt.x, dd.offset.y, thr.siso[SI_TOP].tile);
    } else if (options & DTHRO_LB_B_RB)
    {
        ASSERT(0 == (options & (DTHRO_BORDER | DTHRO_CENTER | DTHRO_CENTER_HOLE | DTHRO_LEFT_CENTER)));

        rdraw.rbeg = thr.sis + SI_LEFT_BOTTOM; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_BOTTOM);
        rdraw.rrep = thr.sis + SI_BOTTOM; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BOTTOM);
        rdraw.rend = thr.sis + SI_RIGHT_BOTTOM; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_BOTTOM);

        if (d)
        {
            rdraw.draw_h(d->draw_thr.rect().lt.x, d->draw_thr.rect().rb.x, d->draw_thr.rect().lt.y, thr.siso[SI_BOTTOM].tile);
        } else
        {
            rdraw.draw_h(dd.offset.x, rbpt.x, dd.offset.y, thr.siso[SI_TOP].tile);
        }

    } else if (0 != (options & DTHRO_CENTER_ONLY) && d)
    {
        fillc( d->draw_thr.rect() + dd.offset, SI_CENTER );
    }

    if ((options & DTHRO_VSB) && d)
    {
        ts::irect sbr = d->draw_thr.sbrect() + dd.offset;
        rdraw.rrep = thr.sis + SI_SBREP;
        if (*rdraw.rrep)
        {
            rdraw.rbeg = thr.sis + SI_SBTOP;
            rdraw.rend = thr.sis + SI_SBBOT;

            rdraw.a_beg = thr.is_alphablend(SI_SBTOP);
            rdraw.a_rep = thr.is_alphablend(SI_SBREP);
            rdraw.a_end = thr.is_alphablend(SI_SBBOT);

            rdraw.draw_v(sbr.rb.x - rdraw.rrep->width(), sbr.lt.y, sbr.rb.y, true);
        }

        rdraw.rbeg = thr.sis + SI_SMTOP; rdraw.a_beg = thr.is_alphablend(SI_SMTOP);
        rdraw.rrep = thr.sis + SI_SMREP; rdraw.a_rep = thr.is_alphablend(SI_SMREP);
        rdraw.rend = thr.sis + SI_SMBOT; rdraw.a_end = thr.is_alphablend(SI_SMBOT);

        ts::irect sbhl[3];
        if ((options & DTHRO_SB_HL) && thr.activesbshift)
        {
            sbhl[0] = *rdraw.rbeg + thr.activesbshift; rdraw.rbeg = sbhl + 0;
            sbhl[1] = *rdraw.rrep + thr.activesbshift; rdraw.rrep = sbhl + 1;
            sbhl[2] = *rdraw.rend + thr.activesbshift; rdraw.rend = sbhl + 2;
        }

        d->draw_thr.sbrect().lt.x = sbr.rb.x - rdraw.rrep->width();
        d->draw_thr.sbrect().lt.y = sbr.lt.y + d->draw_thr.sbpos;
        d->draw_thr.sbrect().rb.x = sbr.rb.x;
        d->draw_thr.sbrect().rb.y = sbr.lt.y + d->draw_thr.sbpos + d->draw_thr.sbsize;

        rdraw.draw_v(d->draw_thr.sbrect().lt.x, d->draw_thr.sbrect().lt.y, d->draw_thr.sbrect().rb.y, true);

        d->draw_thr.sbrect() -= dd.offset;

    }

	// caption

	if (thr.capheight > 0 && (options & DTHRO_CAPTION) != 0)
	{
		ts::irect caprect = thr.captionrect( ts::irect( dd.offset, dd.offset + dd.size ), rps.is_maximized() );

        if ((options & (DTHRO_CENTER|DTHRO_CENTER_HOLE|DTHRO_BASE)) == 0)
        {
            fillc( caprect, SI_CENTER );
        }

        // bottom
        if (0 != ts::ALPHA(thr.siso[SI_CAPREP].fillcolor))
        {
            fillrect.lt = caprect.lt + thr.sis[SI_CAPREP].lt;
            fillrect.rb = caprect.rb - thr.sis[SI_CAPREP].rb;
            caprect = fillrect;
            filler(thr.siso[SI_CAPREP].fillcolor);
        } else
        {
            rdraw.rbeg = thr.sis + SI_CAPSTART; rdraw.a_beg = thr.is_alphablend(SI_CAPSTART);
            rdraw.rrep = thr.sis + SI_CAPREP; rdraw.a_beg = thr.is_alphablend(SI_CAPREP);
            rdraw.rend = thr.sis + SI_CAPEND; rdraw.a_beg = thr.is_alphablend(SI_CAPEND);
            rdraw.draw_h(caprect.lt.x, caprect.rb.x, caprect.lt.y + ((rps.is_maximized() || thr.fastborder()) ? thr.captop_max : thr.captop), thr.siso[SI_CAPREP].tile);
        }

		// caption text
        if ((options & DTHRO_CAPTION_TEXT) != 0)
        {
            ts::wstr_c dn;
            if (dd.engine) dn = dd.engine->getrect().get_name();

            text_draw_params_s tdp;
            tdp.font = thr.capfont;
            tdp.forecolor = &thr.deftextcolor;
            ts::flags32_s f; f.set(ts::TO_VCENTER);
            tdp.textoptions = &f;

            draw_data_s &ddd = begin_draw();
            ddd.offset = caprect.lt + thr.captextadd;
            ddd.size = caprect.size();

            draw(dn, tdp);
            end_draw();
        }
	}

	//draw_image( backbuffer.DC(), thr.src, w/2, h/2, ts::irect(64, 192, 128, 256), true );

	//if (alphablended && !rps.is_alphablend())
	//	MODIFY(getrect()).makealphablend();

}

/*virtual*/ void rectengine_root_c::draw(const ts::wstr_c & text, const text_draw_params_s&tdp)
{
    const draw_data_s &dd = drawdata.last();
    if (dd.size <= 0) return;
    ts::text_rect_c &tr = gui->tr();
    //tr.use_external_glyphs( tdp.glyphs );
    tr.set_text_only(text, true);
    tr.set_size( dd.size );
    tr.set_font( tdp.font );
    tr.set_options(tdp.textoptions ? *tdp.textoptions : 0);
    tr.set_def_color(tdp.forecolor ? *tdp.forecolor : ts::ARGB(0,0,0));

    if ( tdp.rectupdate )
    {
        ts::rectangle_update_s updr;
        updr.updrect = tdp.rectupdate;
        updr.offset = dd.offset;
        updr.param = (dd.engine ? dd.engine->getrid() : getrid()).to_param();
        tr.parse_and_render_texture(&updr, nullptr);
    } else
        tr.parse_and_render_texture(nullptr, nullptr);

    ts::bmpcore_exbody_s bb = syswnd.wnd->get_backbuffer();
    ts::bmpcore_exbody_s src = tr.get_texture().extbody();

#ifdef _DEBUG
    if (tdp.textoptions && tdp.textoptions->is(ts::TO_SAVETOFILE))
    {
        tr.get_texture().save_as_png(L"text.png");
    }
#endif // _DEBUG

    render_image( bb, src, dd.offset.x, dd.offset.y, ts::irect( ts::ivec2(0), dd.size ), dd.cliprect, dd.alpha );
    if (tdp.sz)
        *tdp.sz = gui->tr().lastdrawsize;
    //if (tdp.glyphs)
        //tr.use_external_glyphs( nullptr );
}

/*virtual*/ void rectengine_root_c::draw( const ts::irect & rect, ts::TSCOLOR color, bool clip )
{
    ts::bitmap_t<ts::bmpcore_exbody_s> backbuffer( syswnd.wnd->get_backbuffer() );

    const draw_data_s &dd = drawdata.last();
    if (clip)
    {
        ts::irect dr = rect + dd.offset;
        if (dr.intersect(dd.cliprect))
        {
            if (ts::ALPHA(color) < 255)
                backbuffer.overfill(dr.lt, dr.size(), color);
            else
                backbuffer.fill(dr.lt, dr.size(), color);
        }
    } else
    {
        if (ts::ALPHA(color) < 255)
            backbuffer.overfill(rect.lt + dd.offset, rect.size(), color);
        else
            backbuffer.fill(rect.lt + dd.offset, rect.size(), color);
    }
}

/*virtual*/ void rectengine_root_c::draw( const ts::ivec2 & p, const ts::bmpcore_exbody_s &bmp, bool alphablend)
{
    const draw_data_s &dd = drawdata.last();
    render_image( syswnd.wnd->get_backbuffer(), bmp, dd.offset.x + p.x, dd.offset.y + p.y, dd.cliprect, alphablend ? dd.alpha : -1 );
}

void rectengine_root_c::simulate_mousemove()
{
    if (gui->allow_input(getrid()))
    {
        evt_data_s d;
		d.mouse.screenpos = gui->get_cursor_pos();
		sq_evt( SQ_MOUSE_MOVE, getrid(), d );
    }
}

bool rectengine_root_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    bool focuschanged = false;
	switch( qp )
	{
	case SQ_RESIZE_START:
	case SQ_MOVE_START:
		//SetCapture(hwnd);
		break;
	case SQ_RESIZE_END:
	case SQ_MOVE_END:
		//ReleaseCapture();
		break;
	case SQ_RECT_CHANGING:
        {
            // called on resize or move by mouse ( data.newsize.apply == true )
            // also called when it is need to calc size limits ( data.newsize.apply == false )

			evt_data_s data2;
			data2.rectchg.rect = data.rectchg.rect;
			data2.rectchg.area = data.rectchg.area;
            data2.rectchg.apply = false;

			ts::ivec2 minsz = getrect().get_min_size();
            ts::ivec2 maxsz = getrect().get_max_size();
			fixrect(data2.rectchg.rect, minsz, maxsz, data.rectchg.area);
			getrect().sq_evt(qp, getrid(), data2);
            for (rectengine_c *c : children)
                if (c) c->sq_evt(SQ_PARENT_RECT_CHANGING, c->getrid(), data2);

            fixrect(data2.rectchg.rect, minsz, maxsz, data.rectchg.area);
			
            if (data.rectchg.apply)
			    MODIFY(getrect()).size(data2.rectchg.rect.get().size()).pos(data2.rectchg.rect.get().lt);
            
            data.rectchg.rect = data2.rectchg.rect;
		}
		break;
	case SQ_MOUSE_MOVE:
		if (!gui->mtrack(getrid(), MTT_ANY))
		{
            gui->check_hintzone(data.mouse.screenpos);
            data.mouse.allowchangecursor = true;
            ts::wnd_c *h = ts::master().get_capture();
            rid = getrid();
			const hover_data_s &hd = gui->get_hoverdata(data.mouse.screenpos);
            if (hd.rid && !h)
            {
                if (rectengine_root_c *root = HOLD(hd.rid)().getroot())
                {
                    h = root->syswnd.wnd;
                    if (h) h->set_capture();
                }
                //DMSG("capture" << h);
            }
            if (!hd.rid && h)
            {
                ts::master().release_capture();
            }
            RID prevmouserid = hd.minside;
            if (hd.rid != prevmouserid)
            {
                rectengine_root_c *root = HOLD(hd.rid)().getroot();
                if (CHECK( root != nullptr ) && hd.rid && root->getrid() != rid)
                {
                    h = root->syswnd.wnd;
                    if ( h ) h->set_capture();
                    //DMSG("capture" << h);
                }

                if (prevmouserid)
                {
                    if (prevmouserid == rid && !hd.rid)
                    {
                        ASSERT(h == syswnd.wnd);
                        ts::master().release_capture();
                    }
                    gui->mouse_outside();
                    if (prevmouserid == rid && !hd.rid) return true; // out of root rect? end of all
                }
                if (hd.rid == rid && h != syswnd.wnd)
                {
                    h = syswnd.wnd;
                    if ( h ) h->set_capture();
                    //DMSG("capture" << hwnd);
                }
                if (hd.rid) gui->mouse_inside(hd.rid);
            }

            if (hd.rid == rid && h != syswnd.wnd )
            {
                h = syswnd.wnd;
                if ( h ) h->set_capture();
                //DMSG("capture" << hwnd);
            }

            bool noresize_nomove = false;
            if (hd.rid == rid)
            {
                if (guirect_c *r = rect())
                {
                    r->sq_evt(SQ_MOUSE_MOVE, rid, data);
                    if (r->getprops().is_maximized())
                    {
                        noresize_nomove = true;
                        ts::master().set_cursor(ts::CURSOR_ARROW);
                    }
                }
            } else if (hd.rid)
            {
                HOLD r(hd.rid);
                r.engine().sq_evt(SQ_MOUSE_MOVE, hd.rid, data);
            }
            if (data.mouse.allowchangecursor)
            {
                auto getcs = [&]()-> ts::cursor_e
                {
                    static ts::cursor_e cursors[] = {
                        ts::CURSOR_ARROW,
                        ts::CURSOR_SIZEWE,
                        ts::CURSOR_SIZEWE,
                    ts::CURSOR_LAST,
                        ts::CURSOR_SIZENS,
                        ts::CURSOR_SIZENWSE,
                        ts::CURSOR_SIZENESW,
                    ts::CURSOR_LAST,
                        ts::CURSOR_SIZENS,
                        ts::CURSOR_SIZENESW,
                        ts::CURSOR_SIZENWSE,
                    };
                    if ((hd.area & AREA_RESIZE) && (hd.area & AREA_RESIZE) < ARRAY_SIZE(cursors))
                    {
                        if (noresize_nomove && 0 == (hd.area & AREA_FORCECURSOR)) return ts::CURSOR_LAST;
                        return cursors[hd.area & AREA_RESIZE];
                    }
                    if (hd.area & (AREA_MOVE))
                    {
                        if (noresize_nomove && 0 == (hd.area & AREA_FORCECURSOR)) return ts::CURSOR_LAST;
                        return ts::CURSOR_SIZEALL;
                    }
                    if (hd.area & AREA_EDITTEXT) return ts::CURSOR_IBEAM;
                    if (hd.area & AREA_HAND) return ts::CURSOR_HAND;
                    

                    return ts::CURSOR_ARROW;
                };
                ts::cursor_e c = getcs();
                if (c != ts::CURSOR_LAST )
                    ts::master().set_cursor(c);
            }
            return false;
		}
        break;
    case SQ_MOUSE_IN:
    case SQ_MOUSE_OUT:
        if (guirect_c *r = rect()) r->sq_evt(qp, rid, data);
        return false;
    case SQ_MOUSE_LDOWN:
    case SQ_MOUSE_RDOWN:
    case SQ_MOUSE_MDOWN:
        focuschanged = update_foreground();
    case SQ_MOUSE_LUP:
    case SQ_MOUSE_RUP:
    case SQ_MOUSE_MUP:
    case SQ_MOUSE_WHEELUP:
    case SQ_MOUSE_WHEELDOWN:
    case SQ_MOUSE_L2CLICK:

        if (__super::sq_evt(qp, rid, data)) return true;
        {
            const hover_data_s &hd = gui->get_hoverdata(data.mouse.screenpos);
            ts::safe_ptr<rectengine_root_c> me(this);
            if (!focuschanged && SQ_MOUSE_LUP != qp) gui->set_focus(hd.rid);
            if (!me) return false;
            if (hd.rid == getrid())
            {
                if (guirect_c *r = rect())
                    if (r->sq_evt(qp, rid, data))
                        return true;
                if (SQ_MOUSE_L2CLICK == qp && (hd.area & (AREA_CAPTION|AREA_CAPTION_NOMOVE)))
                {
                    if (guirect_c *r = rect())
                    {
                        if ( 0 != (r->caption_buttons() & SETBIT(CBT_MAXIMIZE)) )
                        {
                            if (r->getprops().is_maximized())
                                MODIFY(*r).maximize(false);
                            else if (!r->getprops().is_collapsed())
                                MODIFY(*r).maximize(true);
                        }
                    }
                }
                return false;
            } else
                if (hd.rid)
                    return HOLD(hd.rid).engine().sq_evt(qp, hd.rid, data);
        }
        return false;
    case SQ_DRAW:
        if (drawntag == data.draw().drawtag)
        {
            if (drawdata.size() == 0)
                syswnd.wnd->flush_draw(getrect().getprops().screenrect());
            return true;
        }
        drawntag = data.draw().drawtag;
        if (data.draw().fake_draw) return true;
        if (guirect_c *r = rect())
        {
            ASSERT(r->getrid() == rid);

            if (!r->getprops().is_visible()) return true;

            draw_data_s  &dd = begin_draw();
            dd.offset = ts::ivec2(0);
            dd.size = r->getprops().currentsize();
            r->sq_evt(qp, r->getrid(), data);

            prepare_children_z_sorted();
            for( rectengine_c *c : children_z_sorted )
                if (c) c->sq_evt(qp, c->getrid(), data);

            end_draw();
            return true;
        }
        break;
    case SQ_ZINDEX_CHANGED:
        gui->resort_roots();
        break;
	}
	return __super::sq_evt(qp, rid, data);
}

void rectengine_root_c::set_system_focus(bool bring_to_front)
{
    if ( syswnd.wnd )
        syswnd.wnd->set_focus( bring_to_front );
}

void rectengine_root_c::flash()
{
    if ( syswnd.wnd )
        syswnd.wnd->flash();
}

bool rectengine_root_c::shakeme(RID, GUIPARAM)
{
    if (shaker)
    {
        ts::ivec2 p = shaker->p;
        if (shaker->countdown > 0)
        {
            (shaker->countdown & 1) ?
                p.x += shaker->countdown :
                p.x -= shaker->countdown;
            --shaker->countdown;
            DEFERRED_UNIQUE_CALL( 0.03, DELEGATE(this, shakeme), nullptr );
        } else
            shaker.reset();
        MODIFY( getrect() ).pos(p);
    }
    return true;
}

void rectengine_root_c::tab_focus( RID r, bool fwd )
{
    decltype( afocus ) sfocus( afocus );
    
    for ( ts::aint i = sfocus.size() - 1; i >= 0; --i )
        if ( sfocus.get( i ).expired() )
            sfocus.remove_fast(i);

    if ( sfocus.size() < 2 ) return;

    sfocus.sort( []( const guirect_c *r1, const guirect_c *r2 ) {

        if ( r1 == r2 ) return false;
        if ( r1->getprops().pos().y == r2->getprops().pos().y )
            return r1->getprops().pos().x < r2->getprops().pos().x;
        return r1->getprops().pos().y < r2->getprops().pos().y;
    } );

    ts::aint sel = -1;
    for ( ts::aint i = sfocus.size() - 1; i >= 0; --i )
    {
        if ( sfocus.get( i )->getrid() == r )
        {
            sel = i;
            break;
        }
    }

    if (sel >= 0)
    {
        if ( fwd )
        {
            ++sel;
            if ( sel >= sfocus.size() ) sel = 0;
        } else
        {
            --sel;
            if ( sel < 0 ) sel = sfocus.size() - 1;
        }

        gui->set_focus( sfocus.get(sel)->getrid() );
    }
}

void rectengine_root_c::register_afocus( guirect_c *r, bool reg )
{
    ASSERT( getrect().accept_focus() );

    bool refocus = false;
    if ( r->getrid() == gui->get_focus() )
        refocus = true;

    for( ts::aint i=afocus.size()-1;i>=0;--i)
        if ( !afocus.get( i ) || afocus.get( i ) == r )
            afocus.remove_slow( i );
    if ( reg ) afocus.add(r);
    if ( refocus )
        gui->set_focus( afocus.size() ? afocus.get( afocus.size()-1)->getrid() : RID() );
}

RID rectengine_root_c::active_focus( RID sub )
{
    while ( afocus.size() > 0 && afocus.get( afocus.size() - 1 ).expired() )
        afocus.remove_slow( afocus.size() - 1 );

    if ( afocus.size() == 0 ) return getrid();

    if ( !sub ) return afocus.get( afocus.size()-1 )->getrid();

    ts::aint fi = afocus.find( &HOLD( sub )( ) );
    if (fi >= 0)
    {
        afocus.remove_slow(fi);
        afocus.add( &HOLD( sub )( ) );
        return sub;
    }
    return active_focus(RID());
}

void rectengine_root_c::shake()
{
    shaker_s *sh = TSNEW( shaker_s );
    sh->p = getrect().getprops().pos();
    shaker.reset(sh);
    DEFERRED_UNIQUE_CALL( 0.03, DELEGATE(this, shakeme), nullptr );
}

bool rectengine_root_c::update_foreground()
{
    ts::wnd_c * prev = nullptr;
    for( RID rr : gui->roots() )
    {
        ts::wnd_c *cur = HOLD(rr)().getroot()->syswnd.wnd;
        if (prev)
        {
            //SetWindowPos(cur, prev, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else
        {
            //SetWindowPos(cur, HWND_NOTOPMOST /*prev*/, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            //SetForegroundWindow(cur);
        }
        prev = cur;
    }
    if (prev) prev->set_focus( false );

    RID oldfocus = gui->get_focus();
    gui->set_focus( active_focus(RID()) );
    return oldfocus != gui->get_focus();
}

ts::wstr_c   rectengine_root_c::load_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, ts::extensions_s & exts, const ts::wchar *title)
{
    return syswnd.wnd->get_load_filename_dialog(iroot, name, exts, title);
}
bool     rectengine_root_c::load_filename_dialog(ts::wstrings_c &files, const ts::wsptr &iroot, const ts::wsptr& name, ts::extensions_s & exts, const ts::wchar *title)
{
    return syswnd.wnd->get_load_filename_dialog(files, iroot, name, exts, title);
}
ts::wstr_c  rectengine_root_c::save_directory_dialog(const ts::wsptr &root, const ts::wsptr &title, const ts::wsptr &selectpath, bool nonewfolder)
{
    return syswnd.wnd->get_save_directory_dialog(root, title, selectpath, nonewfolder);
}
ts::wstr_c  rectengine_root_c::save_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, ts::extensions_s & exts, const ts::wchar *title)
{
    return syswnd.wnd->get_save_filename_dialog(iroot, name, exts, title);
}



// PARENT RECT ENGINE

rectengine_child_c::rectengine_child_c(guirect_c *parent, RID after):parent(parent)
{
    parent->getengine().add_child(this, after);
    drawntag = parent->getroot()->current_drawtag() - 1;
}
rectengine_child_c::~rectengine_child_c() 
{

}

/*virtual*/ draw_data_s & rectengine_child_c::begin_draw() 
{
    draw_data_s &d = getrect().getroot()->begin_draw();
    d.engine = this;
    return d;
}

/*virtual*/ bool rectengine_child_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    switch (qp)
    {
    case SQ_DRAW:
        if (drawntag == data.draw().drawtag) return true;
        drawntag = data.draw().drawtag;
        if (data.draw().fake_draw) return true;
        if (guirect_c *r = rect())
        {
            ASSERT( r->getrid() == rid );

            if (!r->getprops().is_visible()) return true;

            draw_data_s  &dd = begin_draw();
            dd.offset = r->local_to_root( ts::ivec2(0) );
            dd.size = r->getprops().size();
            
            if (dd.cliprect.intersected(ts::irect(dd.offset,dd.offset+dd.size)))
            {
                r->sq_evt(qp, r->getrid(), data);

                prepare_children_z_sorted();
                for (rectengine_c *c : children_z_sorted)
                    if (c) c->sq_evt(qp, c->getrid(), data);
            }

            end_draw();
            return true;
        }
        break;
    case SQ_MOUSE_MOVE:
        if (gui->mtrack(getrid(), MTT_ANY)) break; // move op - do default action
    case SQ_MOUSE_IN:
    case SQ_MOUSE_OUT:
    case SQ_MOUSE_LDOWN:
    case SQ_MOUSE_LUP:
    case SQ_MOUSE_MDOWN:
    case SQ_MOUSE_MUP:
    case SQ_MOUSE_RDOWN:
    case SQ_MOUSE_RUP:
    case SQ_MOUSE_WHEELDOWN:
    case SQ_MOUSE_WHEELUP:
    case SQ_MOUSE_L2CLICK:
        if (guirect_c *r = rect()) r->sq_evt(qp, rid, data);
        return false;
        break;
    case SQ_ZINDEX_CHANGED:
        HOLD(getrect().getparent()).engine().z_resort_children();
        break;
    default:
        break;
    }
    return __super::sq_evt(qp, rid, data);
}

/*virtual*/ void rectengine_child_c::redraw(const ts::irect *invalidate_rect)
{
    if (rectengine_root_c *root = getrect().getroot())
    {
        ts::irect r = getrect().local_to_root( invalidate_rect ? *invalidate_rect : getrect().getprops().currentszrect() );
        root->redraw( &r );
    }
}

/*virtual*/ void rectengine_child_c::draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d )
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(thr, options, d);
}

/*virtual*/ void rectengine_child_c::draw(const ts::wstr_c & text, const text_draw_params_s &tdp)
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(text, tdp);
}

/*virtual*/ void rectengine_child_c::draw( const ts::ivec2 & p, const ts::bmpcore_exbody_s &bmp, bool alphablend)
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(p, bmp, alphablend);
}

/*virtual*/ void rectengine_child_c::draw( const ts::irect & rect, ts::TSCOLOR color, bool clip)
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(rect, color, clip);
}
