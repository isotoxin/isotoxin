#include "win32_common.inl"
class win32_wnd_c;

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, int alpha = -1 /* -1 means no alphablend used */ );
void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, const irect &r, int alpha = -1 /* -1 means no alphablend used */ );
HDC get_dbmp_dc( drawable_bitmap_c&dbmp );

namespace
{
    struct border_window_data_s
    {
        HWND hwnd = nullptr;
        safe_ptr<win32_wnd_c> owner;
        drawable_bitmap_c backbuffer;
        specialborder_s sb;
        irect rect;
        specborder_e bside;
        uint32 wid = 0xabcdc00c;

        static const wchar *classname_border() { return L"rectengineborder"; }

        border_window_data_s( win32_wnd_c* own, specborder_e bside, specialborder_s &bd ) :bside( bside ), owner( own ), sb(bd)
        {
        }

        ~border_window_data_s()
        {
            if ( hwnd ) DestroyWindow( hwnd );
        }

        void cvtrect( irect &sr ) const;
        irect getrect() const;

        void show( HDWP dwp, bool f )
        {
            if ( hwnd == nullptr ) return;
            uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
            flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
            flgs |= SWP_NOSIZE | SWP_NOMOVE;

            if ( f ) flgs |= SWP_SHOWWINDOW;
            else flgs |= SWP_HIDEWINDOW;

            DeferWindowPos( dwp, hwnd, nullptr, 0, 0, 0, 0, flgs );
        }

        void update( HDWP dwp, const irect &newr )
        {
            if ( hwnd == nullptr ) return;
            irect r = newr;
            cvtrect( r );
            if ( r != rect )
            {
                if ( r.size() == rect.size() )
                {
                    rect = r;
                    // just move, no need 2 redraw
                    visualize( false );

                    uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                    flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
                    flgs |= SWP_NOSIZE /*| SWP_NOMOVE*/;


                    DeferWindowPos( dwp, hwnd, nullptr, rect.lt.x, rect.lt.y, 0, 0, flgs );


                    visualize( false );
                }
                else
                {
                    rect = r;
                    visualize( true );

                    uint32 flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                    //if (update_frame) flgs |= SWP_FRAMECHANGED;
                    flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;



                    DeferWindowPos( dwp, hwnd, nullptr, rect.lt.x, rect.lt.y, rect.width(), r.height(), flgs );
                    visualize( false );
                }

            }
        }

        void create_window( HWND parent )
        {
            if ( hwnd ) return;

            rect = getrect();
            if ( !rect ) return;

            uint32 af = WS_POPUP;
            uint32 exf = WS_EX_LAYERED;
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            hwnd = CreateWindowExW( exf, classname_border(), L"", af, rect.lt.x, rect.lt.y, rect.width(), rect.height(), parent, 0, istuff.inst, this );
            visualize();
        }

        void recreate_back_buffer()
        {
            backbuffer.ajust( rect.size(), false );
        }

        void draw()
        {
            backbuffer.ajust( rect.size(), false );
            irect szr = rect.szrect();
            bmpcore_exbody_s bb = backbuffer.extbody();
            bmpcore_exbody_s src = sb.img.extbody();
            repdraw rdraw( bb, src, szr, 255 );

            rdraw.rbeg = &sb.s; rdraw.a_beg = false;
            rdraw.rrep = &sb.r; rdraw.a_rep = false;
            rdraw.rend = &sb.e; rdraw.a_end = false;

            switch ( bside )
            {
            case SPB_LEFT:
            case SPB_RIGHT:
                rdraw.draw_v( 0, 0, rect.height(), true );
                break;

            case SPB_TOP:
            case SPB_BOTTOM:
                rdraw.draw_h( 0, rect.width(), 0, true );
                break;
            }
        }

        void visualize( bool need_draw = true )
        {
            ShowWindow( hwnd, SW_SHOWNA );

            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptSrc; ptSrc.x = 0; ptSrc.y = 0; // start point of the copy from dcMemory to dcScreen

            if ( need_draw ) draw();

            UpdateLayeredWindow( hwnd, /*screen*/nullptr,
                (POINT *)&ref_cast<POINT>( rect.lt ),
                (SIZE *)&ref_cast<SIZE>( rect.size() ),
                get_dbmp_dc(backbuffer), &ptSrc, 0, &blendPixelFunction, ULW_ALPHA );

        }
    };


    struct creation_data_s
    {
        win32_wnd_c *wnd;
        ivec2 size;
        bool collapsed;
    };

}


class win32_wnd_c : public wnd_c
{
    friend HWND wnd2hwnd( const wnd_c * w );
    static const wchar_t *classname() { return L"REW013"; }
    static LRESULT CALLBACK wndhandler( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
    {
        safe_ptr<win32_wnd_c> wnd = hwnd2wnd( hwnd );

        auto mouse_evt = [&]( mouse_event_e e ) -> LRESULT
        {
            if ( wnd )
            {
                ivec2 clp = ivec2( GET_X_LPARAM( lparam ), GET_Y_LPARAM( lparam ) );
                ivec2 scrp(clp);
                ClientToScreen( hwnd, &ref_cast<POINT>( scrp ) );
                wnd->cbs->evt_mouse( e, clp, scrp );
            }

            return 0;
        };

        switch ( msg )
        {
        case WM_CREATE:
            {
                CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
                creation_data_s *d = (creation_data_s *)cs->lpCreateParams;
                d->wnd->hwnd = hwnd;
                d->wnd->recreate_back_buffer( d->size );
                if ( !d->collapsed ) d->wnd->recreate_border();
                SetWindowLongPtrW( hwnd, GWLP_USERDATA, PTR_TO_UNSIGNED( &d->wnd->wid ) ); //-V221
            }
            return 0;
        case WM_DESTROY:
            if ( win32_wnd_c *w = wnd )
            {
                if ( w->flags.is( F_NOTIFICATION_ICON ) )
                {
                    NOTIFYICONDATAW nd = { sizeof( NOTIFYICONDATAW ), 0 };
                    nd.hWnd = hwnd;
                    nd.uID = (int)w; //-V205
                    Shell_NotifyIconW( NIM_DELETE, &nd );
                    w->flags.clear( F_NOTIFICATION_ICON );
                }

                SetWindowLongPtrW( hwnd, GWLP_USERDATA, 0 );
                w->hwnd = nullptr;
                TSDEL( w );
            }
            return DefWindowProcW( hwnd, msg, wparam, lparam );
        case WM_USER + 7213:
            if ( wnd )
            {
                switch ( lparam )
                {
                    case WM_LBUTTONDBLCLK:
                        wnd->cbs->evt_notification_icon( NIA_L2CLICK );
                        break;
                    case WM_LBUTTONUP: {
                        wnd->cbs->evt_notification_icon( NIA_LCLICK );
                        break;
                    }
                    case WM_RBUTTONUP:
                    case WM_CONTEXTMENU: {
                        wnd->cbs->evt_notification_icon( NIA_RCLICK );
                        break;
                    }
                }
            }
            return 0;
        case WM_USER + 7214:
            {
                HICON oi = wnd->notifyicon;
                wnd->notifyicon = wnd->get_icon( true );

                NOTIFYICONDATAW *nd = (NOTIFYICONDATAW *)lparam;
                nd->uTimeout = 0;
                nd->hIcon = wnd->notifyicon;

                if ( wnd->flags.is( F_NOTIFICATION_ICON ) )
                {
                    nd->uFlags = NIF_ICON | NIF_TIP;
                    Shell_NotifyIconW( NIM_MODIFY, nd );

                } else {
                    nd->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                    Shell_NotifyIconW( NIM_ADD, nd );
                    nd->uVersion = NOTIFYICON_VERSION;
                    Shell_NotifyIconW( NIM_SETVERSION, nd );
                }
                wnd->flags.set( F_NOTIFICATION_ICON );
                if ( oi )
                {
                    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
                    istuff.release_icon(oi);
                }
            }
            return 0;
        case WM_PAINT:
            if ( wnd)
            {
                PAINTSTRUCT ps;
                wnd->dc = BeginPaint( hwnd, &ps );
                wnd->cbs->evt_draw();
                wnd->draw_back_buffer();
                EndPaint( hwnd, &ps );
                wnd->dc = nullptr;
                return 0;
            } else
            {
                PAINTSTRUCT ps;
                BeginPaint( hwnd, &ps );
                EndPaint( hwnd, &ps );
            }
            break;
        case WM_CAPTURECHANGED:
            if (wnd) wnd->cbs->evt_mouse_out();
            return 0;
        case WM_CANCELMODE:
            if ( hwnd == GetCapture() )
                ReleaseCapture();
            return 0;
        case WM_SETTINGCHANGE:
            if ( wnd && !wnd->flags.is( F_RECT_UPDATING ) )
            {
                irect cr = wnd->get_normal_rect();
                bool is_max = wnd->is_maximized();
                ASSERT( !is_max || wnd->maxmon >= 0 );
                wnd->cbs->evt_refresh_pos( is_max ? monitor_get_max_size(wnd->maxmon) : cr, is_max ? D_MAX : wnd->dp );
                wnd->kill_border();
                wnd->recreate_border();
            }
            return 0;

        case WM_SYSCOMMAND:
            switch ( GET_SC_WPARAM( wparam ) )
            {
            case SC_RESTORE:
                if ( wnd )
                {
                    wnd->cbs->evt_refresh_pos( wnd->normal_rect, D_RESTORE );
                    wnd->kill_border();
                    wnd->recreate_border();
                }
                break;
            }
            break;
        case WM_SIZE:
            //DMSG("wmsize wparam: " << wparam);
            if ( wparam == SIZE_MINIMIZED && wnd )
            {
                if ( wnd && master().is_active )
                {
                    master().is_active = false;
                    wnd->cbs->evt_focus_changed( nullptr );
                }

                // minimization by system - setup inner flags to avoid restore of window by refresh_frame checker
                wnd->cbs->evt_refresh_pos( wnd->normal_rect, D_MIN );

            } else if ( wparam == SIZE_RESTORED && wnd )
            {
                // restore by system - setup inner flags to avoid restore of window by refresh_frame checker
                wnd->cbs->evt_refresh_pos( wnd->normal_rect, D_RESTORE );
            }
            break;
        case WM_WINDOWPOSCHANGED:
            if ( wnd && !wnd->flags.is( F_RECT_UPDATING ) )
            {
                WINDOWPOS *wp = (WINDOWPOS *)lparam;

                if ( wp->flags & (SWP_NOMOVE|SWP_NOSIZE|0x8000|0x1000) )
                    break;

                irect cr = irect( wp->x, wp->y, wp->x + wp->cx, wp->y + wp->cy );
                bool is_max = wnd->maxmon >= 0;
                irect mr = is_max ? monitor_get_max_size(wnd->maxmon) : wnd_get_max_size( cr );
                wnd->cbs->evt_refresh_pos( is_max ? mr : cr, is_max ? D_MAX : wnd->dp );
            }
            break; // dont return - we need WM_SIZE to detect minimization
        case WM_ACTIVATEAPP:
            if ( wparam && wnd )
            {
                /*
                if (guirect_c *r = engine->rect())
                if (!r->inmod() && r->getprops().is_collapsed())
                MODIFY(*r).decollapse();
                */
            }
            break;
        case WM_MOVE:
            {
                //p.sz.cx = LOWORD( lParam );
                //p.sz.cy = HIWORD( lParam );
            }
            break;
        case WM_NCACTIVATE:
            if ( wnd )
            {

                if ( wparam == FALSE )
                {
                    if ( master().is_active )
                    {
                        master().is_active = false;
                        wnd->cbs->evt_focus_changed( nullptr );
                    }
                } else if ( !master().is_active )
                {
                    master().is_active = true;
                    //wnd->cbs->evt_focus_changed( wnd );
                }

            }
            return TRUE;
        case WM_KILLFOCUS:
            if ( wnd )
                wnd->cbs->evt_focus_changed( wparam ? hwnd2wnd((HWND)wparam) : nullptr );
            break;
        case WM_SETFOCUS:

            if ( !master().is_active )
            {
                master().is_active = true;
                wnd->cbs->evt_focus_changed( wnd );
            }
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

        case WM_MOUSEMOVE: return mouse_evt( MEVT_MOVE );
        case WM_LBUTTONDOWN: return mouse_evt( MEVT_LDOWN );
        case WM_LBUTTONUP: return mouse_evt( MEVT_LUP );
        case WM_RBUTTONDOWN: return mouse_evt( MEVT_RDOWN );
        case WM_RBUTTONUP: return mouse_evt( MEVT_RUP );
        case WM_MBUTTONDOWN: return mouse_evt( MEVT_MDOWN );
        case WM_MBUTTONUP: return mouse_evt( MEVT_MUP );
        case WM_LBUTTONDBLCLK: return mouse_evt( MEVT_L2CLICK );



        case WM_MOUSEWHEEL:
            return 0;
        case WM_MOUSEACTIVATE:
            if ( wnd && !wnd->cbs->evt_mouse_activate() )
                return MA_NOACTIVATEANDEAT;
            return MA_NOACTIVATE;

        case WM_DROPFILES:
            {
                HDROP drp = (HDROP)wparam;
                POINT p;
                if ( DragQueryPoint( drp, &p ) )
                {
                    wstr_c f;
                    for ( int i = 0;; ++i )
                    {
                        f.set_length( MAX_PATH );
                        int l = DragQueryFileW( drp, i, f.str(), MAX_PATH );
                        if ( l <= 0 ) break;
                        f.set_length( l );
                        if ( !wnd || !wnd->cbs->evt_on_file_drop( f, ref_cast<ivec2>( p ) ) )
                            break;
                    }
                }
                DragFinish( drp );
            }
            return 0;
        case WM_CLOSE:
            {
                // TODO : ask main window about close
                master().is_app_need_quit = true;
                PostQuitMessage(0);
            }
            break;

        case WM_ERASEBKGND:
            return 1;
        case WM_SYSCHAR:
            return 0;

        default:
            ;
        }

        return DefWindowProcW( hwnd, msg, wparam, lparam );

    }
    static LRESULT CALLBACK wndhandler_border( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
    {
        auto ddd = []( HWND hwnd ) -> border_window_data_s *
        {
            uint32 *wid = p_cast<uint32 *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
            if ( !wid ) return nullptr;
            if (ASSERT( *wid == 0xabcdc00c ))
                return (border_window_data_s *)( ( (uint8 *)wid ) - offsetof( border_window_data_s, wid ) );
            return nullptr;
        };


        switch ( msg )
        {
        case WM_CREATE:
        {
            CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
            border_window_data_s *d = (border_window_data_s *)cs->lpCreateParams;
            SetWindowLongPtrW( hwnd, GWLP_USERDATA, PTR_TO_UNSIGNED( &d->wid ) ); //-V221
            d->recreate_back_buffer();
        }
        return 0;
        case WM_DESTROY:
            if ( border_window_data_s *d = ddd( hwnd ) )
            {
                SetWindowLongPtrW( hwnd, GWLP_USERDATA, 0 );
                d->hwnd = nullptr;
            }
            return DefWindowProcW( hwnd, msg, wparam, lparam );
        case WM_PAINT:
            if ( border_window_data_s *d = ddd( hwnd ) )
            {
                PAINTSTRUCT ps;
                BeginPaint( hwnd, &ps );
                d->visualize();
                EndPaint( hwnd, &ps );
                return 0;
            }
            break;
        }
        return DefWindowProcW( hwnd, msg, wparam, lparam );
    }

    void recreate_border()
    {
        specialborder_s sbd[ 4 ];
        if ( !borderdata && cbs->evt_specialborder( sbd ) )
        {
            sbs.lt.x = sbd[ SPB_LEFT ].r.width();
            sbs.rb.x = sbd[ SPB_RIGHT ].r.width();
            sbs.lt.y = sbd[ SPB_TOP ].r.height();
            sbs.rb.y = sbd[ SPB_BOTTOM ].r.height();

            borderdata = MM_ALLOC( 4 * sizeof( border_window_data_s ) );
            border_window_data_s *d = (border_window_data_s *)borderdata;
            TSPLACENEW( d + 0, this, SPB_TOP, sbd[ SPB_TOP ] );
            TSPLACENEW( d + 1, this, SPB_RIGHT, sbd[ SPB_RIGHT ] );
            TSPLACENEW( d + 2, this, SPB_BOTTOM, sbd[ SPB_BOTTOM ] );
            TSPLACENEW( d + 3, this, SPB_LEFT, sbd[ SPB_LEFT ] );
        }

        if ( borderdata && hwnd )
        {
            border_window_data_s *d = (border_window_data_s *)borderdata;
            for ( int i = 0; i < 4; ++i )
                d[ i ].create_window( hwnd );
        }
    }
    void recreate_back_buffer( const ivec2 &sz, bool exact_size = false )
    {
        backbuffer.ajust( sz, exact_size );
    }

    void kill_window()
    {
        kill_border();

        if ( flags.is( F_NOTIFICATION_ICON ) )
        {
            NOTIFYICONDATAW nd = { sizeof( NOTIFYICONDATAW ), 0 };
            nd.hWnd = hwnd;
            nd.uID = ( int )this; //-V205
            Shell_NotifyIconW( NIM_DELETE, &nd );
            flags.clear( F_NOTIFICATION_ICON );
        }

        if ( HWND h = hwnd )
        {
            hwnd = nullptr;
            SetWindowLongPtrW( h, GWLP_USERDATA, 0 );
            DestroyWindow( h );
        }

    }
    void kill_border()
    {
        if ( borderdata )
        {
            border_window_data_s *d = (border_window_data_s *)borderdata;
            borderdata = nullptr;
            ( d + 0 )->~border_window_data_s();
            ( d + 1 )->~border_window_data_s();
            ( d + 2 )->~border_window_data_s();
            ( d + 3 )->~border_window_data_s();
            MM_FREE( d );
        }

    }

    void draw_back_buffer()
    {
        if ( dc )
            drawable_bitmap_draw( backbuffer, draw_target_s( dc ), 0, 0, get_actual_rect().szrect() ); // WM_PAINT - draw whole backbuffer
        else
        {
            irect redraw_rect = cbs->app_get_redraw_rect();
            if ( !redraw_rect )
                redraw_rect = get_actual_rect().szrect();

            //DMSG( "bb:" << redraw_rect );

            HDC tdc = GetDC( hwnd );
            drawable_bitmap_draw( backbuffer, draw_target_s( tdc ), redraw_rect.lt.x, redraw_rect.lt.y, redraw_rect );
            ReleaseDC( hwnd, tdc );
        }
    }

    HICON get_icon(bool for_tray)
    {
        bitmap_c bmp = cbs->app_get_icon( for_tray );
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        return istuff.get_icon( bmp );
    }

    static const flags32_s::BITS F_NOTIFICATION_ICON = FREEBITS << 0;
    static const flags32_s::BITS F_RECT_UPDATING = FREEBITS << 1;

    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HICON appicon = nullptr;
    HICON notifyicon = nullptr;
    void *borderdata = nullptr;
    drawable_bitmap_c backbuffer;
    irect normal_rect = irect(0);
    int maxmon = -1;
    //int evtstart = 0;

public:
    irect sbs = irect(0);
    uint32 wid = 0x0803c00c;

    win32_wnd_c( wnd_callbacks_s *cbs ):wnd_c(cbs)
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

        if ( !istuff.regclassref )
        {
            WNDCLASSW wc;
            wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
            wc.lpfnWndProc = wndhandler;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = istuff.inst;
            wc.hIcon = nullptr;
            wc.hCursor = nullptr; //LoadCursor( nullptr, IDC_ARROW );
            wc.hbrBackground = (HBRUSH)( 16 );
            wc.lpszMenuName = nullptr;
            wc.lpszClassName = classname();
            RegisterClassW( &wc );

            wc.lpfnWndProc = wndhandler_border;
            wc.lpszClassName = border_window_data_s::classname_border();
            RegisterClassW( &wc );

        }
        ++istuff.regclassref;

    }
    ~win32_wnd_c()
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

        kill_window();

        if ( appicon ) istuff.release_icon( appicon );
        if ( notifyicon ) istuff.release_icon( notifyicon );

        --istuff.regclassref;
        if ( istuff.regclassref <= 0 )
        {
            UnregisterClassW( classname(), istuff.inst );
            UnregisterClassW( border_window_data_s::classname_border(), istuff.inst );
        }

    }

    /*virtual*/ bool is_collapsed() const override
    {
        return IsIconic( hwnd ) != 0;
    }
    
    /*virtual*/ bool is_maximized() const override
    {
        RECT r;
        GetWindowRect( hwnd, &r );
        bool is_max = ref_cast<irect>( r ) == wnd_get_max_size( ref_cast<irect>( r ) );
        ASSERT(!is_max || maxmon >= 0);
        return is_max;
    }

    /*virtual*/ bool is_foreground() const override
    {
        return GetForegroundWindow() == hwnd;
    }

    irect get_actual_rect() const
    {
        RECT r;
        GetWindowRect( hwnd, &r );
        return ref_cast<irect>( r );
    }

    irect get_normal_rect()
    {
        if ( is_maximized() || is_collapsed() )
            return normal_rect;

        irect ar = get_actual_rect();
        normal_rect = ar;
        return ar;
    }

    /*virtual*/ bool is_hover( const ivec2& screenpos ) const override
    {
        return hwnd == WindowFromPoint( (POINT &)screenpos );
    }

    static int get_max_sz( const irect&fr, irect&r )
    {
        int mc = monitor_count();
        int maxia = -1;
        int fm = -1;

        ts::ivec2 fc = fr.center();
        int dst = maximum<int>::value;
        int fmdst = -1;

        for( int i=0;i<mc; ++i )
        {
            irect rr = monitor_get_max_size(i);
            int ia = fr.intersect_area(rr);
            if (ia > maxia )
            {
                fm = i;
                maxia = ia;
                r = rr;
            }
            if (fm < 0)
            {
                int d = ( fc - rr.center() ).sqlen();
                if (d < dst)
                {
                    dst = d;
                    fmdst = i;
                    r = rr;
                }
            }
        }

        return fm < 0 ? fmdst : fm;
    }

    /*virtual*/ void vshow( wnd_show_params_s *shp ) override
    {
        if ( hwnd )
        {
            AUTOCLEAR( flags, F_RECT_UPDATING );

            wnd_show_params_s def;
            if ( !shp ) shp = &def;

            bool update_frame = false;
            disposition_e odp = dp;

            if ( !shp->collapsed() && shp->layered )
            {
                LONG oldstate = GetWindowLongW( hwnd, GWL_EXSTYLE );
                LONG newstate = oldstate | WS_EX_LAYERED;
                if ( oldstate != newstate )
                {
                    SetWindowLongW( hwnd, GWL_EXSTYLE, newstate );
                    update_frame = true;
                }
            }
            else if ( shp->collapsed() || !shp->layered )
            {
                // due some reasons we should remove WS_EX_LAYERED when rect in minimized state

                LONG oldstate = GetWindowLongW( hwnd, GWL_EXSTYLE );
                LONG newstate = oldstate & ( ~WS_EX_LAYERED );

                bool changed = oldstate != newstate;
                if ( changed && shp->collapsed() )
                    ShowWindow( hwnd, SW_HIDE ); // это делаем, чтобы не мелькал черный квадрат непосредственно перед свертыванием окна

                SetWindowLongW( hwnd, GWL_EXSTYLE, newstate );
                update_frame |= changed;
            }

            irect fssz;
            int m = get_max_sz( ref_cast<irect>( shp->rect ), fssz );

            bool rebuf = false;
            bool reminimization = shp->collapsed() != ( odp == D_MIN || odp == D_MICRO );
            bool remaximization = shp->d != odp && (shp->d == D_MAX || odp == D_MAX);
            // не меняем координаты окна, если изменяется MAX-MIN статус. иначе SW_MAXIMIZE будет странно работать
            
            bool rectchanged = !remaximization && !reminimization && shp->d != D_MAX && !shp->collapsed() && shp->rect != get_normal_rect();

            uint32 flgs = 0;

            if ( update_frame || rectchanged )
            {
                flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                if ( update_frame ) flgs |= SWP_FRAMECHANGED;
                if ( shp->layered ) flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
                if ( !rectchanged ) flgs |= SWP_NOSIZE | SWP_NOMOVE;
                rebuf = true;
            }
            if ( remaximization || reminimization )
                rebuf = true;

            if ( rebuf )
            {
                update_frame = shp->layered; // force draw for alphablend mode
                recreate_back_buffer( shp->d == D_MAX ? fssz.size() : shp->rect.size() );
                if ( !shp->collapsed() && shp->visible )
                    recreate_border();
            }

            if ( shp->layered ) // in alphablend mode we have to draw rect before resize
            {
                shp->apply( update_frame, true );
            }

            if ( flgs )
            {
                if ( borderdata )
                {
                    border_window_data_s *d = (border_window_data_s *)borderdata;
                    int n = 1;
                    if ( d[ 0 ].hwnd ) ++n;
                    if ( d[ 1 ].hwnd ) ++n;
                    if ( d[ 2 ].hwnd ) ++n;
                    if ( d[ 3 ].hwnd ) ++n;

                    HDWP dwp = BeginDeferWindowPos( n );

                    irect sr = shp->rect;
                    d[ 0 ].update( dwp, sr );
                    d[ 1 ].update( dwp, sr );
                    d[ 2 ].update( dwp, sr );
                    d[ 3 ].update( dwp, sr );

                    DeferWindowPos( dwp, hwnd, nullptr, shp->rect.lt.x, shp->rect.lt.y, shp->rect.width(), shp->rect.height(), flgs );

                    //DMSG( "mwp " << pss.pos() );

                    EndDeferWindowPos( dwp );

                }
                else
                    SetWindowPos( hwnd, nullptr, shp->rect.lt.x, shp->rect.lt.y, shp->rect.width(), shp->rect.height(), flgs );

                normal_rect = shp->rect;
            }
            bool cr_brd = false;
            int scmd = 0;
            if ( remaximization || reminimization )
            {
                if ( borderdata && remaximization )
                {
                    border_window_data_s *d = (border_window_data_s *)borderdata;
                    int n = 0;
                    if ( d[ 0 ].hwnd ) ++n;
                    if ( d[ 1 ].hwnd ) ++n;
                    if ( d[ 2 ].hwnd ) ++n;
                    if ( d[ 3 ].hwnd ) ++n;

                    HDWP dwp = BeginDeferWindowPos( n );

                    d[ 0 ].show( dwp, shp->d != D_MAX );
                    d[ 1 ].show( dwp, shp->d != D_MAX );
                    d[ 2 ].show( dwp, shp->d != D_MAX );
                    d[ 3 ].show( dwp, shp->d != D_MAX );
                    EndDeferWindowPos( dwp );
                }

                if ( GetCapture() == hwnd ) ReleaseCapture();
                scmd = shp->collapsed() ? SW_MINIMIZE : ( shp->d == D_MAX ? SW_MAXIMIZE : SW_RESTORE );
                bool curmin = is_collapsed();
                bool curmax = is_maximized();
                if ( curmin != shp->collapsed() || curmax != ( shp->d == D_MAX ) )
                {
                    irect sr = shp->rect;
                    switch ( scmd )
                    {
                    case SW_MAXIMIZE:
                        sr = fssz;
                        maxmon = m;
                        MoveWindow( hwnd, sr.lt.x, sr.lt.y, sr.width(), sr.height(), !shp->layered );
                        if ( odp == D_MICRO )
                        {
                            ShowWindow( hwnd, SW_SHOWNA );
                            ShowWindow( hwnd, SW_RESTORE );
                            set_focus( reminimization );
                        }
                        break;
                    case SW_RESTORE:
                        maxmon = -1;
                        MoveWindow( hwnd, sr.lt.x, sr.lt.y, sr.width(), sr.height(), !shp->layered );
                        if ( odp == D_MICRO ) ShowWindow( hwnd, SW_SHOWNA );
                        // no break here
                    case SW_MINIMIZE:
                        maxmon = -1;
                        ShowWindow( hwnd, scmd );
                        if ( shp->collapsed() )
                        {
                            if ( shp->d == D_MICRO )
                                ShowWindow( hwnd, SW_HIDE );
                            kill_border();
                        }
                        else
                        {
                            kill_border();
                            cr_brd = true;
                            set_focus( reminimization );
                        }
                        break;
                    }

                }
            }


            if ( cr_brd )
                recreate_border(); // rect pos is ok now

            shp->apply( update_frame, false );
            dp = shp->d;
            return;
        }


        // create visible window

        uint32 af = WS_POPUP | /*WS_VISIBLE|*/ WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        uint32 exf = (shp && shp->acceptfiles) ? WS_EX_ACCEPTFILES : 0;

        if ( shp && shp->mainwindow )
            exf |= WS_EX_APPWINDOW;

        if ( shp && shp->layered )
            exf |= WS_EX_LAYERED;

        if ( shp && shp->collapsed() )
            af |= WS_MINIMIZE;

        if ( shp && shp->toolwindow )
        {
            af = WS_POPUP | WS_VISIBLE;
            exf = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        }

        HWND prnt = (shp && shp->parent) ? ptr_cast<win32_wnd_c *>( shp->parent )->hwnd : nullptr;
        normal_rect = irect( 100, 100, 600, 400 );
        if ( shp ) normal_rect = shp->rect;

        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        creation_data_s cd;
        cd.wnd = this;
        cd.size = normal_rect.size();
        cd.collapsed = ( af & WS_MINIMIZE ) != 0;

        hwnd = CreateWindowExW( exf, classname(), shp ? shp->name.cstr() : nullptr, af, normal_rect.lt.x, normal_rect.lt.y, normal_rect.width(), normal_rect.height(), prnt, 0, istuff.inst, &cd );

        if ( !shp || shp->visible )
        {
            ShowWindow( hwnd, (!shp || shp->focus) ? SW_SHOW : SW_SHOWNA );
        }

        if ( shp ) dp = shp->d;

        if ( 0 == ( exf & WS_EX_TOOLWINDOW ) )
        {
            HICON oi = appicon;
            appicon = get_icon( false );
            SetClassLongPtrW( hwnd, GCL_HICON, (size_t)appicon );
            if ( oi ) istuff.release_icon( oi );
        }

        DMSG( "create hwnd: " << hwnd );

        if ( shp )
            shp->apply( true, false );

        //if ( exf & WS_EX_ACCEPTFILES )
        //    DragAcceptFiles( hwnd, TRUE );
    }

    /*virtual*/ void try_close() override
    {
        ::PostMessageW ( hwnd, WM_CLOSE, 0, 0 );
    }

    /*virtual*/ void set_focus( bool bring_to_front ) override
    {
        if ( hwnd && hwnd != GetFocus() )
        {
            DMSG( "focus: " << hwnd );
            SetFocus( hwnd );
        }
        if ( bring_to_front )
        {
            SetForegroundWindow( hwnd );
        }
    }

    /*virtual*/ void flash() override
    {
        FlashWindow( hwnd, 0 );
    }

    /*virtual*/ void flush_draw( const irect &r ) override
    {
        if ( flags.is(F_LAYERED) )
        {
            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptSrc; ptSrc.x = 0; ptSrc.y = 0; // start point of the copy from dcMemory to dcScreen

                                                   //HDC screen = GetDC(NULL);
            UpdateLayeredWindow( hwnd, /*screen*/nullptr,
                (POINT *)&ref_cast<POINT>( r.lt ),
                (SIZE *)&ref_cast<SIZE>( r.size() ),
                get_dbmp_dc( backbuffer ), &ptSrc, 0, &blendPixelFunction, ULW_ALPHA );
            //ReleaseDC( NULL, screen );

        }
        else
            draw_back_buffer();

    }

    /*virtual*/ void set_capture() override
    {
        SetCapture( hwnd );
    }

    /*virtual*/ bmpcore_exbody_s get_backbuffer() override
    {
        return backbuffer.extbody();
    }


    struct modal_use_s
    {
        modal_use_s()
        {
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            ++istuff.sysmodal;
            master().is_sys_loop = istuff.sysmodal > 0;
        }
        ~modal_use_s()
        {
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            --istuff.sysmodal;
            master().is_sys_loop = istuff.sysmodal > 0;
        }
    };

    /*virtual*/ wstr_c get_load_filename_dialog( const wsptr &iroot, const wsptr &name, extensions_s &exts, const wchar *title ) override
    {
        modal_use_s muse;

        wchar cdp[ MAX_PATH ];
        GetCurrentDirectoryW( MAX_PATH, cdp );

        OPENFILENAMEW o;
        memset( &o, 0, sizeof( OPENFILENAMEW ) );

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        o.lStructSize = sizeof( o );
        o.hwndOwner = hwnd;
        o.hInstance = GetModuleHandle( nullptr );

        wstr_c filter;
        if ( exts.exts.size() )
        {
            for ( const extension_s &e : exts.exts )
                filter.append( e.desc ).append_char( '/' ).append( e.ext ).append_char( '/' );
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wstr_c buffer;
        buffer.set_length( 2048 );
        buffer.set( name );

        o.lpstrTitle = title;
        o.lpstrFile = buffer.str();
        o.nMaxFile = 2048;
        o.lpstrDefExt = exts.defext();

        o.lpstrFilter = filter;
        o.lpstrInitialDir = root;
        o.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

        if ( !GetOpenFileNameW( &o ) )
        {
            SetCurrentDirectoryW( cdp );
            return wstr_c();
        }


        SetCurrentDirectoryW( cdp );
        buffer.set_length( CHARz_len( buffer.cstr() ) );
        return buffer;

    }
    /*virtual*/ bool get_load_filename_dialog( wstrings_c &files, const wsptr &iroot, const wsptr& name, ts::extensions_s & exts, const wchar *title ) override
    {
        modal_use_s muse;

        wchar cdp[ MAX_PATH ];
        GetCurrentDirectoryW( MAX_PATH, cdp );

        OPENFILENAMEW o;
        memset( &o, 0, sizeof( OPENFILENAMEW ) );

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        o.lStructSize = sizeof( o );
        o.hwndOwner = hwnd;
        o.hInstance = GetModuleHandle( nullptr );

        wstr_c filter;
        if ( exts.exts.size() )
        {
            for ( const extension_s &e : exts.exts )
                filter.append( e.desc ).append_char( '/' ).append( e.ext ).append_char( '/' );
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wstr_c buffer( 16384, true );
        buffer.set( name );

        o.lpstrTitle = title;
        o.lpstrFile = buffer.str();
        o.nMaxFile = 16384;
        o.lpstrDefExt = exts.defext();

        o.lpstrFilter = filter.cstr();
        o.lpstrInitialDir = root.cstr();
        o.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;
        if ( !GetOpenFileNameW( &o ) )
        {
            SetCurrentDirectoryW( cdp );
            return false;
        }

        SetCurrentDirectoryW( cdp );

        files.clear();


        const wchar *zz = buffer.cstr() + o.nFileOffset;

        if ( o.nFileOffset < CHARz_len( buffer.cstr() ) )
        {
            files.add( buffer.cstr() );
            zz += CHARz_len( zz ) + 1;
        }

        for ( ; *zz; zz += CHARz_len( zz ) + 1 )
        {
            files.add( wstr_c( buffer.cstr() ).append_char( NATIVE_SLASH ).append( zz ) );
        }

        return true;
    }

    static int CALLBACK BrowseCallbackProc( HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
    {
        switch ( uMsg )
        {
        case BFFM_INITIALIZED:
            if ( lpData ) SendMessageW( hWnd, BFFM_SETSELECTION, TRUE, lpData );
            break;
        }
        return 0;
    }



    /*virtual*/ wstr_c  get_save_directory_dialog( const wsptr &root, const wsptr &title, const wsptr &selectpath, bool nonewfolder ) override
    {
        wstr_c buffer;
        buffer.clear();

        LPMALLOC pMalloc;
        if ( ::SHGetMalloc( &pMalloc ) == NOERROR )
        {
            LPITEMIDLIST idl = root.s ? ILCreateFromPathW( wstr_c( root ) ) : nullptr;

            BROWSEINFOW bi;
            ZeroMemory( &bi, sizeof( bi ) );

            wstr_c title_( title );
            wstr_c selectpath_( selectpath );

            bi.hwndOwner = hwnd;
            bi.pidlRoot = idl;
            bi.lpszTitle = title_;
            bi.lpfn = BrowseCallbackProc;
            bi.lParam = (LPARAM)selectpath_.cstr();
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | ( nonewfolder ? BIF_NONEWFOLDERBUTTON : 0 );
            LPITEMIDLIST pidl = ::SHBrowseForFolderW( &bi );
            if ( pidl != nullptr ) {
                buffer.set_length( 2048 - 16 );
                if ( ::SHGetPathFromIDListW( pidl, buffer.str() ) )
                {
                    buffer.set_length( CHARz_len( buffer.cstr() ) );
                }
                else
                {
                    buffer.clear();
                }
                pMalloc->Free( pidl );
            }
            ILFree( idl );
            pMalloc->Release();
        }

        return buffer;
    }

    /*virtual*/ wstr_c get_save_filename_dialog( const wsptr &iroot, const wsptr &name, extensions_s &exts, const wchar *title ) override
    {
        ts::wstr_c filter;
        if ( exts.exts.size() )
        {
            for ( const extension_s &e : exts.exts )
                filter.append( e.desc ).append_char( '/' ).append( e.ext ).append_char( '/' );
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wchar cdp[ MAX_PATH + 16 ];
        GetCurrentDirectoryW( MAX_PATH + 15, cdp );

        OPENFILENAMEW o;
        memset( &o, 0, sizeof( OPENFILENAMEW ) );

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        o.lStructSize = sizeof( o );
        o.hwndOwner = hwnd;
        o.hInstance = GetModuleHandle( nullptr );

        wstr_c buffer( MAX_PATH + 16, true );
        buffer.set( name );

        o.lpstrTitle = title;
        o.lpstrFile = buffer.str();
        o.nMaxFile = MAX_PATH;
        o.lpstrDefExt = exts.defext();

        o.lpstrFilter = filter.is_empty() ? nullptr : filter.cstr();
        o.lpstrInitialDir = root;
        o.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_OVERWRITEPROMPT;

        if ( !GetSaveFileNameW( &o ) )
        {
            SetCurrentDirectoryW( cdp );
            return wstr_c();
        }

        exts.index = o.nFilterIndex - 1;
        SetCurrentDirectoryW( cdp );
        buffer.set_length( CHARz_len( buffer.cstr() ) );

        return buffer;
    }
};

void border_window_data_s::cvtrect( irect &sr ) const
{
    switch ( bside )
    {
    case SPB_LEFT:
        sr.rb.x = sr.lt.x;
        sr.lt.x -= sb.r.width();
        return;
    case SPB_TOP:
        sr.lt.x -= owner->sbs.lt.x;
        sr.rb.x += owner->sbs.rb.x;
        sr.rb.y = sr.lt.y;
        sr.lt.y -= sb.r.height();
        return;
    case SPB_RIGHT:
        sr.lt.x = sr.rb.x;
        sr.rb.x += sb.r.width();
        return;
    case SPB_BOTTOM:
        sr.lt.x -= owner->sbs.lt.x;
        sr.rb.x += owner->sbs.rb.x;
        sr.lt.y = sr.rb.y;
        sr.rb.y += sb.r.height();
        return;
    }
}


irect border_window_data_s::getrect() const
{
    irect sr = owner->get_actual_rect();
    cvtrect( sr );
    return sr;
}

HWND _cdecl wnd2hwnd( const wnd_c * w )
{
    if ( !w ) return nullptr;
    return ( (const win32_wnd_c *)w )->hwnd;
}

win32_wnd_c * _cdecl hwnd2wnd( HWND hwnd )
{
    uint32 *wid = ts::p_cast<uint32 *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
    if ( wid && *wid == 0x0803c00c )
        return (win32_wnd_c *)(( (uint8 *)wid ) - offsetof( win32_wnd_c, wid ));

    return nullptr;
}
