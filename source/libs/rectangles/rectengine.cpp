#include "rectangles.h"
#include <windowsx.h>

INLINE bool insidelt(int v, int a, int asz)
{
    return v >= a && v < (a + asz);
}

INLINE bool insiderb(int v, int a, int asz)
{
    return v < a && v >= (a - asz);
}

namespace
{
struct border_window_data_s
{
    HWND hwnd = nullptr;
    guirect_c::sptr_t owner;
    ts::drawable_bitmap_c backbuffer;
    ts::irect rect;
    subimage_e si;

    static const ts::wchar *classname_border() { return L"rectengineborder"; }

    border_window_data_s( subimage_e si, guirect_c* own ):si(si), owner(own)
    {
    }

    ~border_window_data_s()
    {
        if (hwnd) DestroyWindow(hwnd);
    }

    void cvtrect( ts::irect &sr ) const
    {
        const theme_rect_s *thr = owner->themerect();
        switch (si)
        {
            case SI_LEFT:
                sr.rb.x = sr.lt.x;
                sr.lt.x -= thr->maxcutborder.lt.x;
                return;
            case SI_TOP:
                sr.lt.x -= thr->maxcutborder.lt.x;
                sr.rb.x += thr->maxcutborder.rb.x;
                sr.rb.y = sr.lt.y;
                sr.lt.y -= thr->maxcutborder.lt.y;
                return;
            case SI_RIGHT:
                sr.lt.x = sr.rb.x;
                sr.rb.x += thr->maxcutborder.rb.x;
                return;
            case SI_BOTTOM:
                sr.lt.x -= thr->maxcutborder.lt.x;
                sr.rb.x += thr->maxcutborder.rb.x;
                sr.lt.y = sr.rb.y;
                sr.rb.y += thr->maxcutborder.rb.y;
                return;
        }
        FORBIDDEN();
    }

    ts::irect getrect() const
    {
        const rectprops_c &rps = owner->getprops();
        ts::irect sr = rps.screenrect();
        cvtrect(sr);
        return sr;
    }

    void show(HDWP dwp, bool f)
    {
        if (hwnd == nullptr) return;
        ts::uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
        flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
        flgs |= SWP_NOSIZE | SWP_NOMOVE;

        if (f) flgs |= SWP_SHOWWINDOW;
        else flgs |= SWP_HIDEWINDOW;
    
        DeferWindowPos(dwp, hwnd, nullptr, 0, 0, 0, 0, flgs);
    }

    void update(HDWP dwp, const ts::irect &newr)
    {
        if (hwnd == nullptr) return;
        ts::irect r = newr;
        cvtrect(r);
        if (r != rect)
        {
            if (r.size() == rect.size())
            {
                rect = r;
                // just move, no need 2 redraw
                visualize(false);

                ts::uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
                flgs |= SWP_NOSIZE /*| SWP_NOMOVE*/;

                
                DeferWindowPos(dwp, hwnd, nullptr, rect.lt.x, rect.lt.y, 0, 0, flgs);


                visualize(false);
            } else
            {
                rect = r;
                visualize(true);

                ts::uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                //if (update_frame) flgs |= SWP_FRAMECHANGED;
                flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
                


                DeferWindowPos(dwp, hwnd, nullptr, rect.lt.x, rect.lt.y, rect.width(), r.height(), flgs);
                visualize(false);
            }

        }
    }

    void create_window(HWND parent)
    {
        if (hwnd) return;

        rect = getrect();
        if (!rect) return;

        ts::uint32 af = WS_POPUP | WS_VISIBLE;
        ts::uint32 exf = WS_EX_LAYERED;

        hwnd = CreateWindowExW(exf, classname_border(), L"", af, rect.lt.x, rect.lt.y, rect.width(), rect.height(), parent, 0, g_sysconf.instance, this);
        visualize();
    }

    void recreate_back_buffer()
    {
        backbuffer.ajust(rect.size(), false);
    }

    void draw();

    void visualize(bool need_draw = true)
    {
        BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        POINT ptSrc; ptSrc.x = 0; ptSrc.y = 0; // start point of the copy from dcMemory to dcScreen

        if (need_draw) draw();

        UpdateLayeredWindow(hwnd, /*screen*/nullptr,
                            (POINT *)&ts::ref_cast<POINT>(rect.lt),
                            (SIZE *)&ts::ref_cast<SIZE>(rect.size()),
                            backbuffer.DC(), &ptSrc, 0, &blendPixelFunction, ULW_ALPHA);

    }
};

}


rectengine_c::rectengine_c()
{

}
rectengine_c::~rectengine_c() 
{
    int cnt = children.size();
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
    for (ts::aint i = children_sorted.size() - 1; i >= 0; --i)
        if (children_sorted.get(i).expired()) children_sorted.remove_slow(i);
    if (oldchsz != children.size())
    {
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

void rectengine_c::resort_children()
{
    children_sorted.clear();
    if (children.sortcopy( children_sorted ))
    {
        gui->dirty_hover_data();
        redraw();
        DEFERRED_UNIQUE_CALL(0,DELEGATE(this,cleanup_children),nullptr);
    }
}

bool rectengine_c::children_sort( fastdelegate::FastDelegate< bool (rectengine_c *, rectengine_c *) > swap_them )
{
    auto srt = [&](rectengine_c *e1, rectengine_c *e2)->bool
    {
        if (e1 == e2) return false; // qsort requirement
        return !swap_them(e1,e2);
    };
    if (children.sort(srt))
    {
        gui->dirty_hover_data();
        redraw();
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, cleanup_children), nullptr);
        return true;
    }
    return false;
}

void rectengine_c::child_move_to( int index, rectengine_c *e )
{
    int i = children.find(e);
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

            if (!d.detectarea.area && rps.allow_move())
                if (themerect->captionrect(rps.szrect(), rps.is_maximized()).inside(p))
                    d.detectarea.area |= AREA_CAPTION;

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

void rectengine_c::trunc_children(int index)
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
        int cnt = children.size();
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
    for(int i=children.size() - 1; i>=0; --i)
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

		} else if (gui->mtrack(getrid(), MTT_ANY))
            if (guirect_c *r = rect())
                r->sq_evt(SQ_MOUSE_MOVE_OP, r->getrid(), data);
		break;

	case SQ_MOUSE_LDOWN:
		{
            const hover_data_s &hd = gui->get_hoverdata(data.mouse.screenpos);
			if (hd.rid == getrid() && !getrect().getprops().is_maximized())
            {   if (0 != (hd.area & AREA_RESIZE) && 0 == (hd.area & AREA_NORESIZE))
			    {
                    gui->set_focus(hd.rid);
				    sq_evt(SQ_RESIZE_START, getrid(), ts::make_dummy<evt_data_s>(true));
                    mousetrack_data_s & oo = gui->begin_mousetrack(rid, MTT_RESIZE);
				    oo.area = hd.area;
				    oo.rect = getrect().getprops().rect();
				    oo.mpos = data.mouse.screenpos;
				    return true;
			    }
			    if (0 != (hd.area & AREA_MOVE))
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
        resort_children();
        break;
    case SQ_CHILD_DESTROYED:
        data.rect.index = child_index( data.rect.id );
        if (data.rect.index>=0) children.get(data.rect.index) = nullptr;
        DEFERRED_UNIQUE_CALL(0,DELEGATE(this,cleanup_children),nullptr);
        break;
	}

    if (guirect_c *r = rect())
        if (ASSERT(r->getrid() == rid))
            return r->sq_evt(qp, rid, data);

    return false;
}


// WINAPI RECT ENGINE

int rectengine_root_c::regclassref = 0;

LRESULT CALLBACK rectengine_root_c::wndhandler_border(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
#if defined _DEBUG || defined _CRASH_HANDLER
{
    UNSTABLE_CODE_PROLOG
    return wndhandler_border_dojob(hwnd, msg, wparam, lparam);
    UNSTABLE_CODE_EPILOG
    return 0;
}

LRESULT CALLBACK rectengine_root_c::wndhandler_border_dojob(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
#endif
{
    switch (msg)
    {
        case WM_CREATE:
            {
                CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, PTR_TO_UNSIGNED(cs->lpCreateParams));
                border_window_data_s *d = (border_window_data_s *)cs->lpCreateParams;
                d->recreate_back_buffer();
            }
            return 0;
        case WM_DESTROY:
            if (border_window_data_s *d = ts::p_cast<border_window_data_s *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) ))
            {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                d->hwnd = nullptr;
            }
            return sys_def_main_window_proc(hwnd, msg, wparam, lparam);
        case WM_PAINT:
            if (border_window_data_s *d = ts::p_cast<border_window_data_s *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) ))
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                d->visualize();
                EndPaint(hwnd,&ps);
                return 0;
            }
            break;
    }
    return sys_def_main_window_proc(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK rectengine_root_c::wndhandler(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
#if defined _DEBUG || defined _CRASH_HANDLER
{
    UNSTABLE_CODE_PROLOG
    return wndhandler_dojob(hwnd, msg, wparam, lparam);
    UNSTABLE_CODE_EPILOG
    return 0;
}

LRESULT CALLBACK rectengine_root_c::wndhandler_dojob(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
#endif
{

    struct _redraw {
        rectengine_root_c *engine;

        _redraw(rectengine_root_c *engine_) :engine(engine_) {}

        explicit operator bool() {return engine != nullptr;};
        rectengine_root_c * operator ->() {return engine;};

    } engine(hwnd2engine(hwnd));

    redraw_collector_s dch;

	switch(msg)
	{
		case WM_CREATE:
			{
				CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, PTR_TO_UNSIGNED(cs->lpCreateParams) );
				rectengine_root_c *e = (rectengine_root_c *)cs->lpCreateParams;
                e->hwnd = hwnd;
				e->recreate_back_buffer( e->getrect().getprops().currentsize() );
			}
			return 0;
		case WM_DESTROY:
			if (rectengine_root_c *e = engine.engine)
            {
                if (e->flags.is(F_NOTIFY_ICON))
                {
                    NOTIFYICONDATAW nd = {sizeof(NOTIFYICONDATAW), 0};
                    nd.hWnd = hwnd;
                    nd.uID = (int)e;
                    Shell_NotifyIconW(NIM_DELETE, &nd);
                    e->flags.clear(F_NOTIFY_ICON);
                }

                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0 );
                e->hwnd = nullptr;
                TSDEL(e);
                if (hwnd == ts::g_main_window) ts::g_main_window = nullptr;
            }
			return sys_def_main_window_proc(hwnd,msg,wparam,lparam);
        case WM_USER + 7213:
            if (engine)
            {
                switch (lparam) 
                {
                    case WM_LBUTTONDBLCLK:
                        gui->app_notification_icon_action(NIA_L2CLICK, engine->getrid());
                        break;
                    case WM_LBUTTONUP: {
                        gui->app_notification_icon_action(NIA_LCLICK, engine->getrid());
                        break;
                    }
                    case WM_RBUTTONUP:
                    case WM_CONTEXTMENU: {
                        gui->app_notification_icon_action(NIA_RCLICK, engine->getrid());
                        break;
                    }
                }
            }
            return 0;
        case WM_USER + 7214:
            {
                NOTIFYICONDATAW *nd = (NOTIFYICONDATAW *)lparam;
                nd->uTimeout = 0;
                nd->hIcon = gui->app_icon(true);

                if (engine->flags.is(F_NOTIFY_ICON))
                {
                    nd->uFlags = NIF_ICON;
                    Shell_NotifyIconW(NIM_MODIFY, nd);

                } else {
                    nd->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                    Shell_NotifyIconW(NIM_ADD, nd);
                    nd->uVersion = NOTIFYICON_VERSION;
                    Shell_NotifyIconW(NIM_SETVERSION, nd);
                }
                engine->flags.set(F_NOTIFY_ICON);
            }
            return 0;
		case WM_PAINT:
			if (engine && !engine->is_dip())
			{
				PAINTSTRUCT ps;
				engine->dc = BeginPaint(hwnd,&ps);
                guirect_c &r = engine->getrect();
				//const rectprops_c &rps = r.getprops();
                evt_data_s d( evt_data_s::draw_s(engine->drawtag) );
				//if (!rps.is_alphablend())
				//{
    //                engine->sq_evt(SQ_DRAW, r.getrid(), d);
				//	EndPaint(hwnd,&ps);

				//} else
				//{
					engine->sq_evt(SQ_DRAW, r.getrid(), d );
					EndPaint(hwnd,&ps);
				//}
                engine->dc = nullptr;
				return 0;
			} else
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd,&ps);
                EndPaint(hwnd,&ps);
            }
			break;
        case WM_CAPTURECHANGED:
            //DMSG("lost capture" << hwnd);
            gui->mouse_outside();
            return 0;
        case WM_CANCELMODE:
            if (hwnd == GetCapture())
                ReleaseCapture();
            return 0;
        case WM_SETTINGCHANGE:
            if (engine)
                if (guirect_c *r = engine->rect())
                    if (r->getprops().is_maximized())
                    {
                        MODIFY(*r).maximize(false);
                        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
                            MODIFY(RID::from_ptr(param)).maximize(true);
                        DEFERRED_EXECUTION_BLOCK_END(r->getrid().to_ptr());
                    }
            return 0;

        case WM_SYSCOMMAND:
            switch (GET_SC_WPARAM(wparam))
            {
            case SC_RESTORE:
                if (engine)
                    if (guirect_c *r = engine->rect())
                        if (!r->inmod())
                            MODIFY(*r).decollapse();
                break;
            }
            break;
        case WM_SIZE:
            //DMSG("wmsize wparam: " << wparam);
            if (wparam == SIZE_MINIMIZED && engine)
            {
                // minimization by system - setup inner flags to avoid restore of window by refresh_frame checker
                if (guirect_c *r = engine->rect())
                    if (!r->inmod())
                        MODIFY(*r).minimize(true);
            } else if (wparam == SIZE_RESTORED && engine)
            {
                // restore by system - setup inner flags to avoid restore of window by refresh_frame checker
                if (guirect_c *r = engine->rect())
                    if (!r->inmod())
                        MODIFY(*r).decollapse();
            }
            break;
        case WM_WINDOWPOSCHANGED:
            if (engine && !engine->is_dip())
            {
                WINDOWPOS *wp = (WINDOWPOS *)lparam;
                DMSG("wpch flags: " << wp->flags);
                const guirect_c &r = engine->getrect();
                ts::ivec2 p = r.getprops().screenpos();
                ts::ivec2 s = r.getprops().size();
                if (p.x != wp->x || p.y != wp->y || s.x != wp->cx || s.y != wp->cy)
                    engine->refresh_frame();
            }
            break; // dont return - we need WM_SIZE to detect minimization
        case WM_ACTIVATEAPP:
            if (wparam && engine)
            {
                /*
                if (guirect_c *r = engine->rect())
                    if (!r->inmod() && r->getprops().is_collapsed())
                        MODIFY(*r).decollapse();
                        */
            }
            break;
        case WM_NCACTIVATE:
            if (wparam == FALSE && engine)
                if (engine->getrid() >>= gui->get_focus())
                    gui->set_focus(RID());
            break;
        case WM_KILLFOCUS:
            if (engine)
                if (engine->getrid() >>= gui->get_focus())
                    gui->set_focus(RID());
            break;
        //case WM_NCPAINT:
        //    return 0;
        //case WM_NCCALCSIZE:
        //    if (wparam)
        //    {
        //        return 0;
        //    } else
        //    {
        //        return 0;
        //    }

#define MOUSE_HANDLER(sq) if (engine && gui->allow_input(engine->getrid())) { evt_data_s d; \
				d.mouse.screenpos = ts::ivec2( GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) ); \
				ClientToScreen(hwnd, &ts::ref_cast<POINT>(d.mouse.screenpos) ); \
				engine->sq_evt( sq, engine->getrid(), d ); } return 0

		case WM_MOUSEMOVE: MOUSE_HANDLER(SQ_MOUSE_MOVE);
		case WM_LBUTTONDOWN: MOUSE_HANDLER(SQ_MOUSE_LDOWN);
		case WM_LBUTTONUP: MOUSE_HANDLER(SQ_MOUSE_LUP);
		case WM_RBUTTONDOWN: MOUSE_HANDLER(SQ_MOUSE_RDOWN);
		case WM_RBUTTONUP: MOUSE_HANDLER(SQ_MOUSE_RUP);
		case WM_MBUTTONDOWN: MOUSE_HANDLER(SQ_MOUSE_MDOWN);
		case WM_MBUTTONUP: MOUSE_HANDLER(SQ_MOUSE_MUP);
        case WM_LBUTTONDBLCLK: MOUSE_HANDLER(SQ_MOUSE_L2CLICK);

        case WM_MOUSEWHEEL:
            return 0;
        case WM_MOUSEACTIVATE:
            if (!gui->allow_input(engine->getrid()))
                return MA_NOACTIVATEANDEAT;
            return MA_NOACTIVATE;
        case WM_DROPFILES:
            {
                HDROP drp = (HDROP)wparam;
                POINT p;
                if (DragQueryPoint(drp, &p))
                {
                    ts::wstr_c f;
                    for(int i = 0;;++i)
                    {
                        f.set_length(MAX_PATH);
                        int l = DragQueryFileW(drp,i,f.str(), MAX_PATH);
                        if (l<=0) break;
                        f.set_length(l);
                        if (gmsg<GM_DROPFILES>(engine ? engine->getrid() : RID(), f, ts::ref_cast<ts::ivec2>(p)).send().is(GMRBIT_ABORT))
                            break;
                    }
                }
                DragFinish(drp);
            }
            return 0;
	}

	return sys_def_main_window_proc(hwnd,msg,wparam,lparam);
}

rectengine_root_c::rectengine_root_c(bool sys):hwnd(nullptr), dc(nullptr)/*, alphablendfor(nullptr)*/
{
    redraw_rect = ts::irect( maximum<int>::value, minimum<int>::value );
    flags.init( F_SYSTEM, sys );
    drawntag = drawtag - 1;
	if (!regclassref)
	{
		WNDCLASSW wc;
		wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc   = wndhandler;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = g_sysconf.instance;
		wc.hIcon         = nullptr;
		wc.hCursor       = nullptr; //LoadCursor( nullptr, IDC_ARROW );
		wc.hbrBackground = (HBRUSH)(16);
		wc.lpszMenuName  = nullptr;
		wc.lpszClassName = classname();
		RegisterClassW(&wc);

        wc.lpfnWndProc = wndhandler_border;
        wc.lpszClassName = border_window_data_s::classname_border();
        RegisterClassW(&wc);

	}
	++regclassref;

}

rectengine_root_c::~rectengine_root_c() 
{
    if (flags.is(F_SYSTEM) && flags.is(F_NOTIFY_ICON))
    {
        NOTIFYICONDATAW nd = {sizeof(NOTIFYICONDATAW), 0};
        nd.hWnd = hwnd;
        nd.uID = (int)this; //V-205
        Shell_NotifyIconW(NIM_SETFOCUS, &nd);
    }

    flags.set(F_DIP);
    if (gui) gui->delete_event( DELEGATE(this, refresh_frame) );
    kill_window();

	--regclassref;
	if (regclassref <= 0)
		UnregisterClassW( classname(), g_sysconf.instance );
}

void rectengine_root_c::recreate_back_buffer(const ts::ivec2 &sz, bool exact_size)
{
	backbuffer.ajust(sz, exact_size);

    const theme_rect_s *thr = getrect().themerect();
    if (thr && thr->specialborder() && borderdata == nullptr)
    {
        borderdata = MM_ALLOC(4 * sizeof(border_window_data_s));
        border_window_data_s *d = (border_window_data_s *)borderdata;
        TSPLACENEW(d + 0, SI_TOP, &getrect());
        TSPLACENEW(d + 1, SI_RIGHT, &getrect());
        TSPLACENEW(d + 2, SI_BOTTOM, &getrect());
        TSPLACENEW(d + 3, SI_LEFT, &getrect());
    }

    if (borderdata && hwnd)
    {
        border_window_data_s *d = (border_window_data_s *)borderdata;
        for(int i=0;i<4;++i)
            d[i].create_window(hwnd);
    }
}

bool rectengine_root_c::refresh_frame(RID, GUIPARAM p)
{
    if (p)
    {
        if (hwnd && !getrect().getprops().is_collapsed())
        {
            RECT r;
            GetWindowRect(hwnd, &r);
            if ( ts::ref_cast<ts::irect>(r) != getrect().getprops().screenrect() )
            {
                MODIFY(getrect()).visible(false);
                MODIFY(getrect()).visible(true);
            }
        }

    } else
    {
        DEFERRED_UNIQUE_CALL( 1.0, DELEGATE(this, refresh_frame), (GUIPARAM)1 );
    }
    return true;
}

void rectengine_root_c::kill_window()
{
    if (borderdata)
    {
        border_window_data_s *d = (border_window_data_s *)borderdata;
        borderdata = nullptr;
        (d + 0)->~border_window_data_s();
        (d + 1)->~border_window_data_s();
        (d + 2)->~border_window_data_s();
        (d + 3)->~border_window_data_s();
        MM_FREE(d);
    }

    if (HWND h = hwnd)
    {
        if (flags.is(F_NOTIFY_ICON))
        {
            NOTIFYICONDATAW nd = {sizeof(NOTIFYICONDATAW), 0};
            nd.hWnd = hwnd;
            nd.uID = (int)this; //V-205
            Shell_NotifyIconW(NIM_DELETE, &nd);
            flags.clear(F_NOTIFY_ICON);
        }

        hwnd = nullptr;
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        DestroyWindow(h);
    }
}

/*virtual*/ bool rectengine_root_c::apply(rectprops_c &rpss, const rectprops_c &pss)
{
	if (!hwnd && pss.is_visible())
	{
        rpss.change_to(pss, this);

		// create visible window
		ts::uint32 af = WS_POPUP|WS_VISIBLE|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
		ts::uint32 exf = WS_EX_ACCEPTFILES;;
        
        if ( !g_sysconf.mainwindow )
            exf |= WS_EX_APPWINDOW;
		
		if (pss.is_alphablend())
			exf |= WS_EX_LAYERED;

        //if (pss.is_maximized())
        //    af |= WS_MAXIMIZE;

        if (pss.is_minimized())
            af |= WS_MINIMIZE;

        HWND prnt = g_sysconf.mainwindow;
        if (flags.is(F_SYSTEM))
        {
            af = WS_POPUP | WS_VISIBLE;
            exf = WS_EX_TOPMOST|WS_EX_TOOLWINDOW;
            //prnt = WindowFromPoint(ts::ref_cast<POINT>(gui->get_cursor_pos()));
            //prnt = FindWindowW(L"Shell_TrayWnd", NULL);
            prnt = nullptr;
        }

        //RID prev;
        //for( RID rr : gui->roots() )
        //{
        //    if (rr == getrid()) break;
        //    prev = rr;
        //}
        //if (prev) prnt = ts::ptr_cast<rectengine_root_c *>(&HOLD(prev).engine())->hwnd;

        ts::str_c name( to_utf8(getrect().get_name()) );
        text_remove_tags(name);

        ts::irect sr = rpss.screenrect();
		hwnd = CreateWindowExW(exf, classname(), from_utf8(name), af, sr.lt.x,sr.lt.y,sr.width(),sr.height(),prnt,0,g_sysconf.instance,this);
        if ( g_sysconf.mainwindow == nullptr )
        {
            g_sysconf.mainwindow = hwnd;
            ts::g_main_window = hwnd;
        }

        if (flags.is(F_SYSTEM))
        {
            //SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            //SetForegroundWindow(hwnd);
        } else
            SetClassLong(hwnd, GCL_HICON, (LONG)gui->app_icon(false));

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

        if ( getrid() >> gui->get_focus() ) ; else
            gui->set_focus(getrid(), getrect().steal_active_focus_if_root()); // root rect stole focus

        if ( gui->get_active_focus() == getrid() )
            SetFocus(hwnd);

	} else if (!pss.is_visible())
	{
		// window stil invisible => just change parameters
		rpss.change_to(pss, this);
        kill_window();
	} else
	{
		// change exist window

		bool update_frame = false;
        bool was_micro = rpss.is_micromized();

		if ( !pss.is_collapsed() && pss.is_alphablend() )
		{ 
            LONG oldstate = GetWindowLongW(hwnd,GWL_EXSTYLE);
            LONG newstate = oldstate | WS_EX_LAYERED;
            if (oldstate != newstate)
            {
                SetWindowLongW(hwnd, GWL_EXSTYLE, newstate);
                update_frame = true;
            }
		}
		else if ( pss.is_collapsed() || !pss.is_alphablend() )
		{
            // due some reasons we should remove WS_EX_LAYERED when rect in minimized state

            LONG oldstate = GetWindowLongW(hwnd,GWL_EXSTYLE);
            LONG newstate = oldstate & (~WS_EX_LAYERED);

            bool changed = oldstate != newstate;
            if (changed && pss.is_collapsed())
                ShowWindow(hwnd,SW_HIDE); // это делаем, чтобы не мелькал черный квадрат непосредственно перед свертыванием окна

			SetWindowLongW(hwnd,GWL_EXSTYLE, newstate );
			update_frame |= changed;
		}
		bool rebuf = false;
        bool reminimization = pss.is_collapsed() != rpss.is_collapsed();
        bool remaximization = pss.is_maximized() != rpss.is_maximized();
        // не меняем координаты окна, если изменяется MAX-MIN статус. иначе SW_MAXIMIZE будет странно работать
        bool rectchanged = !remaximization && !reminimization && !pss.is_maximized() && !pss.is_collapsed() && pss.screenrect() != rpss.screenrect();

        ts::uint32 flgs = 0;

		if (update_frame || rectchanged)
		{
			flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
			if (update_frame) flgs |= SWP_FRAMECHANGED;
			if (pss.is_alphablend()) flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
            if (!rectchanged) flgs |= SWP_NOSIZE | SWP_NOMOVE;
            rebuf = true;
		}
        if (remaximization || reminimization)
            rebuf = true;

        if (rebuf)
        {
            update_frame = pss.is_alphablend(); // force draw for alphablend mode
            recreate_back_buffer(pss.currentsize());
        }

        if (pss.is_alphablend()) // in alphablend mode we have to draw rect before resize
        {
            rpss.change_to(pss, this);

            if (update_frame)
            {
                redraw(); // we need it! see redraw tag
                draw_data_s &dd = begin_draw();
                ASSERT(dd.offset == ts::ivec2(0));
                dd.size = rpss.currentsize();
                ASSERT(dd.cliprect.size() >> dd.size);
                evt_data_s d = evt_data_s::draw_s(drawtag);
                sq_evt(SQ_DRAW, getrid(), d);
                end_draw();
            }
        }

        if (flgs)
        {
            if (borderdata)
            {
                border_window_data_s *d = (border_window_data_s *)borderdata;
                int n = 1; 
                if (d[0].hwnd) ++n;
                if (d[1].hwnd) ++n;
                if (d[2].hwnd) ++n;
                if (d[3].hwnd) ++n;

                HDWP dwp = BeginDeferWindowPos(n);

                ts::irect sr = pss.screenrect();
                d[0].update(dwp, sr);
                d[1].update(dwp, sr);
                d[2].update(dwp, sr);
                d[3].update(dwp, sr);

                DeferWindowPos(dwp, hwnd, nullptr, pss.pos().x, pss.pos().y, pss.size().x, pss.size().y, flgs);

                //DMSG( "mwp " << pss.pos() );

                EndDeferWindowPos(dwp);

            } else
                SetWindowPos(hwnd, nullptr, pss.pos().x, pss.pos().y, pss.size().x, pss.size().y, flgs);
        }

        int scmd = 0;
        if (remaximization || reminimization)
        {
            if (borderdata && remaximization)
            {
                border_window_data_s *d = (border_window_data_s *)borderdata;
                int n = 0;
                if (d[0].hwnd) ++n;
                if (d[1].hwnd) ++n;
                if (d[2].hwnd) ++n;
                if (d[3].hwnd) ++n;

                HDWP dwp = BeginDeferWindowPos(n);

                d[0].show(dwp, !pss.is_maximized());
                d[1].show(dwp, !pss.is_maximized());
                d[2].show(dwp, !pss.is_maximized());
                d[3].show(dwp, !pss.is_maximized());
                EndDeferWindowPos(dwp);
            }

            if (GetCapture() == hwnd) ReleaseCapture();
            scmd = pss.is_collapsed() ? SW_MINIMIZE : (pss.is_maximized() ? SW_MAXIMIZE : SW_RESTORE);
            bool curmin = is_collapsed();
            bool curmax = is_maximized();
            if (curmin != pss.is_collapsed() || curmax != pss.is_maximized())
            {
                ts::irect sr = pss.screenrect();
                switch (scmd)
                {
                case SW_MAXIMIZE:
                    MoveWindow(hwnd,sr.lt.x, sr.lt.y, sr.width(), sr.height(), !pss.is_alphablend());
                    if (was_micro)
                    {
                        ShowWindow(hwnd, SW_SHOWNA);
                        ShowWindow(hwnd, SW_RESTORE);
                    }
                    break;
                case SW_RESTORE:
                    MoveWindow(hwnd,sr.lt.x, sr.lt.y, sr.width(), sr.height(), !pss.is_alphablend());
                    if (was_micro) ShowWindow(hwnd, SW_SHOWNA);
                    // no break here
                case SW_MINIMIZE:
                    ShowWindow(hwnd, scmd);
                    if (pss.is_micromized()) ShowWindow(hwnd, SW_HIDE);
                    break;
                }
                    
            }
        }

        if (!pss.is_alphablend())
        {
            rpss.change_to(pss, this);

            if (update_frame)
            {
                draw_data_s &dd = begin_draw();
                ASSERT(dd.offset == ts::ivec2(0));
                dd.size = rpss.currentsize();
                ASSERT(dd.cliprect.size() >> dd.size);
                evt_data_s d = evt_data_s::draw_s(drawtag);
                sq_evt(SQ_DRAW, getrid(), d);
                end_draw();
            }
        } else
        {
            begin_draw();
            end_draw();
        }

        switch (scmd)
        {
        case SW_MAXIMIZE:
            gmsg<GM_UI_EVENT>(UE_MAXIMIZED).send();
            refresh_frame();
            DMSG("maximized");
            break;
        case SW_RESTORE:
            gmsg<GM_UI_EVENT>(UE_NORMALIZED).send();
            refresh_frame();
            DMSG("restored");
            break;
        case SW_MINIMIZE:
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

    //if (hwnd && flags.is(F_SYSTEM))
    //{
    //    SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    //}

}

void rectengine_root_c::redraw_now()
{
    if (hwnd == nullptr) return;

    ASSERT(drawdata.size() == 0 && hwnd);
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
        if (rps.is_alphablend())
        {
            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptSrc; ptSrc.x = 0; ptSrc.y = 0; // start point of the copy from dcMemory to dcScreen

            //HDC screen = GetDC(NULL);
            UpdateLayeredWindow(hwnd, /*screen*/nullptr,
                                (POINT *)&ts::ref_cast<POINT>(rps.screenpos()),
                                (SIZE *)&ts::ref_cast<SIZE>(rps.currentsize()),
                                backbuffer.DC(), &ptSrc, 0, &blendPixelFunction, ULW_ALPHA);
            //ReleaseDC( NULL, screen );

        } else
            draw_back_buffer();
    }
}

void rectengine_root_c::draw_back_buffer()
{
    if (dc)
        backbuffer.draw(dc, 0, 0, getrect().getprops().currentszrect()); // WM_PAINT - draw whole backbuffer
    else
    {
        if (!redraw_rect)
            redraw_rect = ts::irect(0, getrect().getprops().currentszrect());

        //DMSG( "bb:" << redraw_rect );

        HDC tdc = GetDC(hwnd);
        backbuffer.draw(tdc, redraw_rect.lt.x, redraw_rect.lt.y, redraw_rect);
        ReleaseDC(hwnd, tdc);
    }
}

static void draw_image( HDC dc, const ts::drawable_bitmap_c &image, ts::aint x, ts::aint y, const ts::irect &imgrect, const ts::irect &cliprect, int alpha )
{
    if (image.DC() == nullptr) return;
    ts::irect imgrectnew = imgrect.szrect();
    imgrectnew += ts::ivec2(x,y);
    imgrectnew.intersect(cliprect);
    
    if (!imgrectnew) return;

    imgrectnew -= ts::ivec2(x,y);
    x += imgrectnew.lt.x; y += imgrectnew.lt.y;
    imgrectnew += imgrect.lt;

    image.draw(dc, x, y, imgrectnew, alpha);
}

struct repdraw //-V690
{
    HDC dc;
    const ts::drawable_bitmap_c &image;
    const ts::irect &cliprect;
    const ts::irect *rbeg;
    const ts::irect *rrep;
    const ts::irect *rend;
    int alpha;
    bool a_beg, a_rep, a_end;
    void operator=(const repdraw &) UNUSED;
    repdraw(HDC dc, const ts::drawable_bitmap_c &image, const ts::irect &cliprect, int alpha):dc(dc), image(image), cliprect(cliprect), alpha(alpha) {}

    void draw_h( ts::aint x1, ts::aint x2, ts::aint y, bool tile )
    {
        if (rbeg) draw_image( dc, image, x1, y, *rbeg, cliprect, a_beg ? alpha : -1);
    
        if (tile)
        {
            int dx = rrep->width();
            if (CHECK(dx))
            {
                int a = a_rep ? alpha : -1;

                int sx0 = x1 + (rbeg ? rbeg->width() : 0);
                int sx1 = x2 - (rend ? rend->width() : 0);
                int z = sx0 + dx;
                for (; z <= sx1; z += dx)
                    draw_image(dc, image, z - dx, y, *rrep, cliprect, a);
                z -= dx;
                if (z < sx1)
                    draw_image(dc, image, z, y, ts::irect(*rrep).setwidth(sx1 - z), cliprect, a);
            }
        } else
        {
            ASSERT(false, "stretch");
        }
        if (rend) draw_image( dc, image, x2 - rend->width(), y, *rend, cliprect, a_end ? alpha : -1);
    }
    void draw_v(ts::aint x, ts::aint y1, ts::aint y2, bool tile)
    {
        if (rbeg) draw_image(dc, image, x, y1, *rbeg, cliprect, a_beg ? alpha : -1);

        if (tile)
        {
            int dy = rrep->height();
            if (CHECK(dy))
            {
                int a = a_rep ? alpha : -1;

                int sy0 = y1 + (rbeg ? rbeg->height() : 0);
                int sy1 = y2 - (rend ? rend->height() : 0);
                int z = sy0 + dy;
                for (; z <= sy1; z += dy)
                    draw_image(dc, image, x, z - dy, *rrep, cliprect, a);
                z -= dy;
                if (z < sy1)
                    draw_image(dc, image, x, z, ts::irect(*rrep).setheight(sy1 - z), cliprect, a);
            }
        }
        else
        {
            ASSERT(false, "stretch");
        }

        if (rend) draw_image(dc, image, x, y2 - rend->height(), *rend, cliprect, a_end ? alpha : -1 );
    }

    void draw_c(ts::aint x1, ts::aint x2, ts::aint y1, ts::aint y2, bool tile)
    {
        if (tile)
        {
            int a = a_rep ? alpha : -1;

            int dx = rrep->width();
            int dy = rrep->height();
            if (dx == 0 || dy == 0) return;
            int sx0 = x1;
            int sx1 = x2 - dx;
            int sy0 = y1;
            int sy1 = y2 - dy;
            int x;
            ts::irect rr; bool rr_initialized = false;
            int y = sy0;
            for (; y < sy1; y += dy)
            {
                for (x = sx0; x < sx1; x += dx)
                    draw_image(dc, image, x, y, *rrep, cliprect, a);

                if (!rr_initialized)
                {
                    if (x < (sx1 + dx))
                    {
                        rr = *rrep;
                        rr.setwidth(sx1 + dx - x);
                        rr_initialized = true;
                    }
                }

                if (rr_initialized)
                    draw_image(dc, image, x, y, rr, cliprect, a);
            }
            if (y < (sy1 + dy))
            {
                ts::irect rrb = *rrep;
                rrb.setheight(sy1 + dy - y);
                for (x = sx0; x < sx1; x += dx)
                    draw_image(dc, image, x, y, rrb, cliprect, a);
                if (rr_initialized)
                    draw_image(dc, image, x, y, rrb.setwidth(rr.width()), cliprect, a);
                else
                    draw_image(dc, image, x, y, rrb.setwidth(sx1 + dx - x), cliprect, a);
            }
        } else
        {
            ASSERT(false, "stretch");
        }
    }
};

void border_window_data_s::draw()
{
    backbuffer.ajust(rect.size(), false);
    const theme_rect_s *thr = owner->themerect();
    ts::irect szr = rect.szrect();
    repdraw rdraw( backbuffer.DC(), thr->src, szr, 255 );

    ts::irect prev, rep, last;

    switch (si)
    {
        case SI_LEFT:
            if (thr->sis[SI_LEFT])
            {
                rep = thr->sis[SI_LEFT]; rep.rb.x = rep.lt.x + thr->maxcutborder.lt.x;
                prev = thr->sis[SI_LEFT_TOP];
                prev.rb.x = prev.lt.x + thr->maxcutborder.lt.x;
                prev.lt.y += thr->maxcutborder.lt.y;

                last = thr->sis[SI_LEFT_BOTTOM];
                last.rb.x = last.lt.x + thr->maxcutborder.lt.x;
                last.rb.y -= thr->maxcutborder.rb.y;

                rdraw.rbeg = &prev; rdraw.a_beg = false;
                rdraw.rrep = &rep; rdraw.a_rep = false;
                rdraw.rend = &last; rdraw.a_end = false;
                rdraw.draw_v(0, 0, rect.height(), thr->siso[SI_LEFT].tile);
            }

            break;
        case SI_TOP:
            if (thr->sis[SI_TOP])
            {
                rep = thr->sis[SI_TOP];  rep.rb.y = rep.lt.y + thr->maxcutborder.lt.y;
                prev = thr->sis[SI_LEFT_TOP]; prev.rb.y = prev.lt.y + thr->maxcutborder.lt.y;
                last = thr->sis[SI_RIGHT_TOP]; last.rb.y = last.lt.y + thr->maxcutborder.lt.y;

                rdraw.rbeg = &prev; rdraw.a_beg = false;
                rdraw.rrep = &rep; rdraw.a_rep = false;
                rdraw.rend = &last; rdraw.a_end = false;
                rdraw.draw_h(0, rect.width(), 0, thr->siso[SI_TOP].tile);
            }

            break;
        case SI_RIGHT:

            rep = thr->sis[SI_RIGHT]; rep.lt.x = rep.rb.x - thr->maxcutborder.rb.x;
            prev = thr->sis[SI_RIGHT_TOP];
            prev.lt.x = prev.rb.x - thr->maxcutborder.rb.x;
            prev.lt.y += thr->maxcutborder.lt.y;

            last = thr->sis[SI_RIGHT_BOTTOM];
            last.lt.x = last.rb.x - thr->maxcutborder.rb.x;
            last.rb.y -= thr->maxcutborder.rb.y;


            rdraw.rbeg = &prev; rdraw.a_beg = false;
            rdraw.rrep = &rep; rdraw.a_rep = false;
            rdraw.rend = &last; rdraw.a_end = false;
            rdraw.draw_v(0, 0, rect.height(), thr->siso[SI_RIGHT].tile);

            break;
        case SI_BOTTOM:

            rep = thr->sis[SI_BOTTOM]; rep.lt.y = rep.rb.y - thr->maxcutborder.rb.y;
            prev = thr->sis[SI_LEFT_BOTTOM]; prev.lt.y = prev.rb.y - thr->maxcutborder.rb.y;
            last = thr->sis[SI_RIGHT_BOTTOM]; last.lt.y = last.rb.y - thr->maxcutborder.rb.y;

            rdraw.rbeg = &prev; rdraw.a_beg = false;
            rdraw.rrep = &rep; rdraw.a_rep = false;
            rdraw.rend = &last; rdraw.a_end = false;
            rdraw.draw_h(0, rect.width(), 0, thr->siso[SI_BOTTOM].tile);

            break;
    }

}


/*virtual*/ void rectengine_root_c::draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d )
{
    if (drawdata.size() == 0) return;
	const rectprops_c &rps = getrect().getprops();
	const draw_data_s &dd = drawdata.last();
    bool self_draw = dd.engine == this;
    bool use_alphablend = !self_draw; //!flags.is(F_SELF_DRAW);

    repdraw rdraw( backbuffer.DC(), thr.src, dd.cliprect, dd.alpha );
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

    if (options & DTHRO_BASE)
    {
        if (0 != ts::ALPHA(thr.siso[SI_BASE].fillcolor))
        {
            fillrect.lt = dd.offset + thr.sis[SI_BASE].lt;
            fillrect.rb = rbpt - thr.sis[SI_BASE].rb;
            filler(thr.siso[SI_BASE].fillcolor);

        } else
        {
            rdraw.rbeg = nullptr;
            rdraw.rrep = thr.sis + SI_BASE; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BASE);
            rdraw.rend = nullptr;
            rdraw.draw_c(dd.offset.x, rbpt.x, dd.offset.y, rbpt.y, thr.siso[SI_BASE].tile);
        }
    } else if (0 != (options & DTHRO_BASE_HOLE) && d && ASSERT(thr.sis[SI_BASE]))
    {
        rdraw.rbeg = nullptr;
        rdraw.rrep = thr.sis + SI_BASE; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BASE);
        rdraw.rend = nullptr;

        int y = dd.offset.y;
        for (int ylmt = d->draw_thr.rect().lt.y + dd.offset.y; y < ylmt; y += rdraw.rrep->height()) //-V807
            rdraw.draw_h(dd.offset.x, rbpt.x, y, thr.siso[SI_BASE].tile);

        int y2 = rbpt.y;
        for (int ylmt = d->draw_thr.rect().rb.y + dd.offset.y; y2 >= ylmt;)
            rdraw.draw_h(dd.offset.x, rbpt.x, y2 -= rdraw.rrep->height(), thr.siso[SI_BASE].tile);

        for (int x = dd.offset.x, xlmt = d->draw_thr.rect().lt.x + dd.offset.x; x < xlmt; x += rdraw.rrep->width())
            rdraw.draw_v(x, y, y2, thr.siso[SI_BASE].tile);

        for (int x2 = rbpt.x, xlmt = d->draw_thr.rect().rb.x + dd.offset.x; x2 >= xlmt;)
            rdraw.draw_v(x2 -= rdraw.rrep->width(), y, y2, thr.siso[SI_BASE].tile);
    }

    if (options & (DTHRO_BORDER|DTHRO_CENTER|DTHRO_CENTER_HOLE))
    {
        ASSERT( 0 == (options & (DTHRO_LEFT_CENTER)) );

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

        if (options & DTHRO_BORDER)
        {
            // top
            bool top_drawn;
            if (false != (top_drawn = *t))
            {
                rdraw.rbeg = lt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP);
                rdraw.rrep = t; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_TOP);
                rdraw.rend = rt; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_TOP);
                rdraw.draw_h(dd.offset.x, rbpt.x, dd.offset.y, thr.siso[SI_TOP].tile);
            }

            // bottom
            bool bottom_drawn;
            if (false != (bottom_drawn = *b))
            {
                rdraw.rbeg = lb; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_BOTTOM);
                rdraw.rrep = b; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BOTTOM);
                rdraw.rend = rb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_BOTTOM);
                rdraw.draw_h(dd.offset.x, rbpt.x, rbpt.y - b->height(), thr.siso[SI_BOTTOM].tile);
            }

            // left
            if (*l)
            {
                int y0 = dd.offset.y;
                int y1 = rbpt.y;

                if (top_drawn) { rdraw.rbeg = nullptr; y0 += lt->height(); } else { rdraw.rbeg = lt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_LEFT_TOP); }
                rdraw.rrep = l; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_LEFT);
                if (bottom_drawn) { rdraw.rend = nullptr;  y1 -= lb->height(); } else { rdraw.rend = lb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_LEFT_BOTTOM); }
                rdraw.draw_v(dd.offset.x, y0, y1, thr.siso[SI_LEFT].tile);
            }

            // right
            if (*r)
            {
                int y0 = dd.offset.y;
                int y1 = rbpt.y;

                if (top_drawn) { rdraw.rbeg = nullptr;  y0 += rt->height(); } else { rdraw.rbeg = rt; rdraw.a_beg = use_alphablend && thr.is_alphablend(SI_RIGHT_TOP); }
                rdraw.rrep = r; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_RIGHT);
                if (bottom_drawn) { rdraw.rend = nullptr; y1 -= rb->height(); } else { rdraw.rend = rb; rdraw.a_end = use_alphablend && thr.is_alphablend(SI_RIGHT_BOTTOM); }
                rdraw.draw_v(rbpt.x - r->width(), y0, y1, thr.siso[SI_RIGHT].tile);
            }

        }
        if (0 != (options & DTHRO_CENTER))
        {
            if (0 == ts::ALPHA(thr.siso[SI_CENTER].fillcolor))
            {
                rdraw.rbeg = nullptr;
                rdraw.rrep = thr.sis + SI_CENTER; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_CENTER);
                rdraw.rend = nullptr;
                rdraw.draw_c(dd.offset.x + l->width(), rbpt.x - r->width(), dd.offset.y + t->height(), rbpt.y - b->height(), thr.siso[SI_CENTER].tile);
            }
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
        ASSERT( 0 == (options & (DTHRO_BORDER|DTHRO_CENTER|DTHRO_CENTER_HOLE)) );

        rdraw.rbeg = nullptr;
        rdraw.rrep = thr.sis + SI_LEFT; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_LEFT);
        rdraw.rend = nullptr;
        rdraw.draw_v(dd.offset.x, dd.offset.y, rbpt.y, thr.siso[SI_LEFT].tile);

        if (0 != ts::ALPHA(thr.siso[SI_CENTER].fillcolor))
        {
            fillrect.lt.x = dd.offset.x + thr.sis[SI_LEFT].width();
            fillrect.lt.y = dd.offset.y;
            fillrect.rb = rbpt - thr.sis[SI_CENTER].rb;
            fillrect.lt += thr.sis[SI_CENTER].lt;
            filler(thr.siso[SI_CENTER].fillcolor);
        } else
        {
            rdraw.rrep = thr.sis + SI_CENTER; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_CENTER);
            rdraw.draw_c(dd.offset.x + thr.sis[SI_LEFT].width(), rbpt.x, dd.offset.y, rbpt.y, thr.siso[SI_CENTER].tile);
        }

        if (options & DTHRO_BOTTOM)
        {
            rdraw.rbeg = nullptr;
            rdraw.rrep = thr.sis + SI_BOTTOM; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_BOTTOM);
            rdraw.rend = nullptr;
            rdraw.draw_h(dd.offset.x + thr.sis[SI_LEFT].width(), rbpt.x, (dd.offset.y + rbpt.y) / 2, thr.siso[SI_BOTTOM].tile);
        }
        if (options & DTHRO_RIGHT)
        {
            draw_image( backbuffer.DC(), thr.src, rbpt.x - thr.sis[SI_RIGHT].width(), (dd.offset.y + rbpt.y - thr.sis[SI_RIGHT].height())/2, thr.sis[SI_RIGHT], dd.cliprect, use_alphablend && thr.is_alphablend(SI_RIGHT) ? dd.alpha : -1 );
        }
    } else if (0 != (options & DTHRO_CENTER_ONLY) && d)
    {
        rdraw.rrep = thr.sis + SI_CENTER; rdraw.a_rep = use_alphablend && thr.is_alphablend(SI_CENTER);
        rdraw.draw_c(dd.offset.x + d->draw_thr.rect().lt.x, dd.offset.x + d->draw_thr.rect().rb.x, 
                     dd.offset.y + d->draw_thr.rect().lt.y, dd.offset.y + d->draw_thr.rect().rb.y, thr.siso[SI_CENTER].tile);

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
            subimage_e si = SI_CENTER;
            rdraw.rbeg = nullptr;
            rdraw.rrep = thr.sis + si; rdraw.a_beg = use_alphablend && thr.is_alphablend(si);
            if (! *rdraw.rrep)
            {
                si = SI_BASE;
                rdraw.rrep = thr.sis + si;
                rdraw.a_beg = use_alphablend && thr.is_alphablend(si);
            }
            rdraw.rend = nullptr;
            rdraw.draw_c(caprect.lt.x, caprect.rb.x, caprect.lt.y, caprect.rb.y, thr.siso[si].tile);
        }

        // bottom
        rdraw.rbeg = thr.sis+SI_CAPSTART; rdraw.a_beg = thr.is_alphablend(SI_CAPSTART);
        rdraw.rrep = thr.sis+SI_CAPREP; rdraw.a_beg = thr.is_alphablend(SI_CAPREP);
        rdraw.rend = thr.sis+SI_CAPEND; rdraw.a_beg = thr.is_alphablend(SI_CAPEND);
        rdraw.draw_h(caprect.lt.x, caprect.rb.x, caprect.lt.y + ((rps.is_maximized() || thr.fastborder()) ? thr.captop_max : thr.captop), thr.siso[SI_CAPREP].tile);

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
            ddd.offset = caprect.lt;
            ddd.offset.x += thr.captexttab;
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
        updr.param = (dd.engine ? dd.engine->getrid() : getrid()).to_ptr();
        tr.parse_and_render_texture(&updr, nullptr);
    } else
        tr.parse_and_render_texture(nullptr, nullptr);

    draw_image( backbuffer.DC(), tr.get_texture(), dd.offset.x, dd.offset.y, ts::irect( ts::ivec2(0), dd.size ), dd.cliprect, dd.alpha );
    if (tdp.sz)
        *tdp.sz = gui->tr().lastdrawsize;
    //if (tdp.glyphs)
        //tr.use_external_glyphs( nullptr );
}

/*virtual*/ void rectengine_root_c::draw( const ts::irect & rect, ts::TSCOLOR color, bool clip )
{
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

/*virtual*/ void rectengine_root_c::draw( const ts::ivec2 & p, const ts::drawable_bitmap_c &bmp, const ts::irect& bmprect, bool alphablend)
{
    const draw_data_s &dd = drawdata.last();
    draw_image( backbuffer.DC(), bmp, dd.offset.x + p.x, dd.offset.y + p.y, bmprect, dd.cliprect, alphablend ? dd.alpha : -1 );
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
            // вызывается во время ресайза и мува мышкой ( data.newsize.apply == true )
            // а также при необходимости расчитать допустимые размеры ( data.newsize.apply == false )

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
            HWND h = GetCapture();
            rid = getrid();
			const hover_data_s &hd = gui->get_hoverdata(data.mouse.screenpos);
            if (hd.rid && !h)
            {
                if (rectengine_root_c *root = HOLD(hd.rid)().getroot())
                {
                    h = root->hwnd;
                    SetCapture(h);
                }
                //DMSG("capture" << h);
            }
            if (!hd.rid && h)
            {
                ReleaseCapture();
            }
            RID prevmouserid = hd.minside;
            if (hd.rid != prevmouserid)
            {
                rectengine_root_c *root = HOLD(hd.rid)().getroot();
                if (CHECK( root != nullptr ) && hd.rid && root->getrid() != rid)
                {
                    h = root->hwnd;
                    SetCapture(h);
                    //DMSG("capture" << h);
                }

                if (prevmouserid)
                {
                    if (prevmouserid == rid && !hd.rid)
                    {
                        ASSERT(h == hwnd);
                        ReleaseCapture();
                    }
                    gui->mouse_outside();
                    if (prevmouserid == rid && !hd.rid) return true; // out of root rect? end of all
                }
                if (hd.rid == rid && h != hwnd)
                {
                    h = hwnd;
                    SetCapture(hwnd);
                    //DMSG("capture" << hwnd);
                }
                if (hd.rid) gui->mouse_inside(hd.rid);
            }

            if (hd.rid == rid && h != hwnd)
            {
                h = hwnd;
                SetCapture(hwnd);
                //DMSG("capture" << hwnd);
            }

            if (hd.rid == rid)
            {
                if (guirect_c *r = rect())
                {
                    r->sq_evt(SQ_MOUSE_MOVE, rid, data);
                    if (r->getprops().is_maximized())
                    {
                        data.mouse.allowchangecursor = false;
                        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                    }
                }
            } else if (hd.rid)
            {
                HOLD r(hd.rid);
                r.engine().sq_evt(SQ_MOUSE_MOVE, hd.rid, data);
            }
            if (data.mouse.allowchangecursor)
            {
                auto getcs = [=]()-> const ts::wchar *
                {
                    static const ts::wchar *cursors[] = {
                        IDC_ARROW,
                        IDC_SIZEWE,
                        IDC_SIZEWE,
                        nullptr,
                        IDC_SIZENS,
                        IDC_SIZENWSE,
                        IDC_SIZENESW,
                        nullptr,
                        IDC_SIZENS,
                        IDC_SIZENESW,
                        IDC_SIZENWSE,
                    };
                    if ((hd.area & AREA_RESIZE) && (hd.area & AREA_RESIZE) < LENGTH(cursors)) return cursors[hd.area & AREA_RESIZE];
                    if (hd.area & AREA_CAPTION) return IDC_SIZEALL;
                    if (hd.area & AREA_EDITTEXT) return IDC_IBEAM;
                    if (hd.area & AREA_HAND) return IDC_HAND;
                    

                    return IDC_ARROW;
                };

                SetCursor(LoadCursorW(nullptr, getcs()));
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
                    return r->sq_evt(qp, rid, data);
            } else
                if (hd.rid)
                    return HOLD(hd.rid).engine().sq_evt(qp, hd.rid, data);
        }
        return false;
    case SQ_DRAW:
        if (drawntag == data.draw().drawtag)
        {
            if (drawdata.size() == 0)
                draw_back_buffer();
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

            for( rectengine_c *c : children_sorted )
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
    if (hwnd && hwnd != GetFocus())
    {
        SetFocus(hwnd);
    }
    if (bring_to_front)
        SetForegroundWindow(hwnd);
}

void rectengine_root_c::flash()
{
    FlashWindow(hwnd, 0);
}


void rectengine_root_c::notification_icon( const ts::wsptr& text )
{
    static NOTIFYICONDATAW nd = { sizeof(NOTIFYICONDATAW), 0 };
    nd.hWnd = hwnd;
    nd.uID = (int)this; //V-205
    nd.uCallbackMessage = WM_USER + 7213;
    //nd.hIcon = gui->app_icon(true);

    size_t copylen = ts::tmin(sizeof(nd.szTip) - sizeof(ts::wchar), text.l * sizeof(ts::wchar));
    memcpy(nd.szTip, text.s, copylen);
    nd.szTip[copylen / sizeof(ts::wchar)] = 0;

    PostMessageW( hwnd, WM_USER + 7214, 0, (LPARAM)&nd );
}

bool rectengine_root_c::update_foreground()
{
    HWND prev = nullptr;
    for( RID rr : gui->roots() )
    {
        HWND cur = HOLD(rr)().getroot()->hwnd;
        SetWindowPos(cur, HWND_NOTOPMOST /*prev*/, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (prev)
        {
        } else
        {
            SetForegroundWindow(cur);
        }
        prev = cur;
    }
    SetFocus(prev);
    RID oldfocus = gui->get_focus();
    gmsg<GM_ROOT_FOCUS>(getrid()).send();
    return oldfocus != gui->get_focus();
}

#ifdef _WIN32
namespace ts {
    extern HWND g_main_window;
};
#endif

ts::wstr_c   rectengine_root_c::load_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title)
{
    /*
        извините за этот небольшой win32 хак, но
        если системному окну выбора папки не прописать в качестве владельца текущий диалог,
        то системное окно начнет вести себя не вполне корректно
    */
    HWND ow = ts::g_main_window;
    ts::g_main_window = hwnd;

    ++sysmodal;
    ts::wstr_c r = ts::get_load_filename_dialog(iroot, name, filt, defext, title);
    --sysmodal;
    ts::g_main_window = ow;

    return r;
}
bool     rectengine_root_c::load_filename_dialog(ts::wstrings_c &files, const ts::wsptr &iroot, const ts::wsptr& name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title)
{
    HWND ow = ts::g_main_window;
    ts::g_main_window = hwnd;

    ++sysmodal;
    bool r = ts::get_load_filename_dialog(files, iroot, name, filt, defext, title);
    --sysmodal;
    ts::g_main_window = ow;

    return r;
}

ts::wstr_c  rectengine_root_c::save_directory_dialog(const ts::wsptr &root, const ts::wsptr &title, const ts::wsptr &selectpath, bool nonewfolder)
{
    HWND ow = ts::g_main_window;
    ts::g_main_window = hwnd;

    ++sysmodal;
    ts::wstr_c r = ts::get_save_directory_dialog(root, title, selectpath, nonewfolder);
    --sysmodal;
    ts::g_main_window = ow;

    return r;
}
ts::wstr_c  rectengine_root_c::save_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title)
{
    HWND ow = ts::g_main_window;
    ts::g_main_window = hwnd;

    ++sysmodal;
    ts::wstr_c r = ts::get_save_filename_dialog(iroot, name, filt, defext, title);
    --sysmodal;
    ts::g_main_window = ow;

    return r;
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

                for (rectengine_c *c : children_sorted)
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
        HOLD(getrect().getparent()).engine().resort_children();
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

/*virtual*/ void rectengine_child_c::draw( const ts::ivec2 & p, const ts::drawable_bitmap_c &bmp, const ts::irect& bmprect, bool alphablend)
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(p, bmp, bmprect, alphablend);
}

/*virtual*/ void rectengine_child_c::draw( const ts::irect & rect, ts::TSCOLOR color, bool clip)
{
    if (rectengine_root_c *root = getrect().getroot())
        root->draw(rect, color, clip);
}
