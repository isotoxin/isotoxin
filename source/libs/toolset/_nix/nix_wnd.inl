#include "nix_common.inl"
class x11_wnd_c;

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, int alpha = -1 /* -1 means no alphablend used */ );
void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, const irect &r, int alpha = -1 /* -1 means no alphablend used */ );
//HDC get_dbmp_dc( drawable_bitmap_c&dbmp );

class x11_wnd_c : public wnd_c
{
    friend Window wnd2x11( const wnd_c * w );
    friend void x11expose(x11_wnd_c *, bool mapnotify);
    friend void x11configure(x11_wnd_c *, const XConfigureEvent &ce);

    void recreate_back_buffer( const ivec2 &sz, bool exact_size = false )
    {
        backbuffer.ajust( sz, exact_size );
    }

    void kill_window()
    {
/*
        if ( flags.is( F_NOTIFICATION_ICON ) )
        {
            NOTIFYICONDATAW nd = { sizeof( NOTIFYICONDATAW ), 0 };
            nd.hWnd = hwnd;
            nd.uID = ( int )( size_t )this; //-V205
            Shell_NotifyIconW( NIM_DELETE, &nd );
            flags.clear( F_NOTIFICATION_ICON );
        }

        if ( HWND h = hwnd )
        {
            hwnd = nullptr;
            SetWindowLongPtrW( h, GWLP_USERDATA, 0 );
            DestroyWindow( h );
        }
*/
    }

    void draw_back_buffer()
    {
        if (backbuffer.body() == nullptr)
            return;

        if ( false /*redraw by event*/ )
        {
            drawable_bitmap_draw( backbuffer, draw_target_s( x11w ), 0, 0, get_actual_rect().szrect() ); // EXPOSE - draw whole backbuffer
        } else
        {
            irect redraw_rect = cbs->app_get_redraw_rect();
            if ( !redraw_rect )
                redraw_rect = get_actual_rect().szrect();

            //DMSG( "bb:" << redraw_rect );

            drawable_bitmap_draw( backbuffer, draw_target_s( x11w ), redraw_rect.lt.x, redraw_rect.lt.y, redraw_rect );

        }
    }


//    static const flags32_s::BITS F_NOTIFICATION_ICON = FREEBITS << 0;
    static const flags32_s::BITS F_RECT_UPDATING = FREEBITS << 1;
    static const flags32_s::BITS F_MAX_CHANGE = FREEBITS << 2;

    array_safe_t< x11_wnd_c, 1 > children;

    Window x11w = 0;

    bool mapped = false;
/*
    HDC dc = nullptr;
    HICON appicon = nullptr;
    HICON notifyicon = nullptr;
    void *borderdata = nullptr;
*/
    drawable_bitmap_c backbuffer;
    irect normal_rect = irect(0);
    int maxmon = -1;

    int l_alpha = 255; // 0..255

    void add_child( x11_wnd_c *c )
    {
        for ( aint i = children.size() - 1; i >= 0; --i )
        {
            if ( children.get( i ).expired() )
                children.remove_fast( i );
            else if ( c == children.get( i ) )
                return;
        }
        children.add( c );
    }

public:
    uint32 wid = 0x0803c00c;
    irect sbs = irect(0);

    x11_wnd_c( wnd_callbacks_s *cbs ):wnd_c(cbs)
    {
        //master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    }
    ~x11_wnd_c()
    {
        kill_window();

        //if ( appicon ) istuff.release_icon( appicon );
        //if ( notifyicon ) istuff.release_icon( notifyicon );

        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        istuff.remove_window(this);
    }

    /*virtual*/ bool is_collapsed() const override
    {
        UNFINISHED(is_collapsed);
		return false;
        //return IsIconic( hwnd ) != 0;
    }

    /*virtual*/ bool is_maximized() const override
    {
        UNFINISHED(is_maximized);
		return false;
/*
        RECT r;
        GetWindowRect( hwnd, &r );
        bool is_max = ref_cast<irect>( r ) == wnd_get_max_size( ref_cast<irect>( r ) );
        ASSERT(!is_max || maxmon >= 0);
        return is_max;
*/
    }

    /*virtual*/ bool is_foreground() const override
    {
        STOPSTUB();
		return false;
    }

    irect get_actual_rect() const
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        XWindowAttributes wa;
        XGetWindowAttributes(istuff.X11, x11w, &wa);
		return irect(wa.x,wa.y,wa.x+wa.width,wa.y+wa.height);
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
        //Bool XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return);
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        Window rw = 0, cw = 0;
        int rx, ry, wx, wy;
        unsigned int m;
        Bool z = XQueryPointer(istuff.X11,x11w,&rw,&cw,&rx,&ry,&wx,&wy,&m);

        return cw == x11w;
    }

    /*virtual*/ void special_border(bool on) override
    {
    }

    /*virtual*/ void make_hole( const ts::irect &holerect ) override
    {
        STOPSTUB();
/*
        if (holerect)
        {
            RECT rr;
            GetWindowRect( hwnd, &rr );

            HRGN r0 = CreateRectRgn( 0, 0, 0, 0 );
            HRGN r1 = CreateRectRgn(0,0,rr.right-rr.left, rr.bottom-rr.top);
            HRGN r2 = CreateRectRgn( holerect.lt.x, holerect.lt.y, holerect.rb.x, holerect.rb.y );
            CombineRgn(r0, r1, r2, RGN_XOR);
            DeleteObject( r1 );
            DeleteObject( r2 );

            SetWindowRgn( hwnd, r0, TRUE );
        } else
        {
            SetWindowRgn( hwnd, nullptr, TRUE );
        }
*/
    }

    void opacity( float v )
    {
        if (v >= 1.0f)
            return;

        STOPSTUB();
        //l_alpha = CLAMP<uint8>( ts::lround( v * 255.0f ) );
        //apply_layered_attributes();
    }

    /*virtual*/ void update_icon()
    {
        STOPSTUB();
/*
        LONG exf = GetWindowLongW( hwnd, GWL_EXSTYLE );

        if ( 0 != ( exf & WS_EX_APPWINDOW ) )
        {
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

            HICON oi = appicon;
            appicon = get_icon( false );
            SendMessageW( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)appicon );
            SendMessageW( hwnd, WM_SETICON, ICON_BIG, (LPARAM)appicon );
            if ( oi ) istuff.release_icon( oi );
        }
  */
    }

    /*virtual*/ void vshow( wnd_show_params_s *shp ) override
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

        if ( x11w )
        {
            AUTOCLEAR( flags, F_RECT_UPDATING );

            wnd_show_params_s def;
            if ( !shp ) shp = &def;

            bool update_frame = false;
            disposition_e odp = dp;

            if ( !shp->collapsed() && (shp->layered || shp->opacity < 1.0f) )
            {
                /*
                LONG oldstate = GetWindowLongW( hwnd, GWL_EXSTYLE );
                LONG newstate = oldstate | WS_EX_LAYERED;
                if ( oldstate != newstate )
                {
                    SetWindowLongW( hwnd, GWL_EXSTYLE, newstate );
                    update_frame = true;
                }
                */
            }
            else if ( shp->collapsed() || !shp->layered )
            {
                // MINIMIZE HERE
                //XWindowAttributes wa;
                //XGetWindowAttributes(istuff.X11, x11w, &wa);


                //update_frame |= changed;
            }

            int m = 0;
            irect fssz(shp->maxrect);
            if (fssz.area() == 0)
                m = monitor_find_max_sz(ref_cast<irect>(shp->rect), fssz);
            else
                m = monitor_find(fssz.center());

            bool rebuf = false;
            bool reminimization = shp->collapsed() != ( odp == D_MIN || odp == D_MICRO );
            bool remaximization = shp->d != odp && (shp->d == D_MAX || odp == D_MAX);
            bool rectchanged = !remaximization && !reminimization && shp->d != D_MAX && !shp->collapsed() && shp->rect != get_normal_rect();
            if ( flags.is( F_MAX_CHANGE ) )
                remaximization = true, rectchanged = true;

            //uint32 flgs = 0;

            if ( update_frame || rectchanged )
            {
                //flgs = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW;
                //if ( update_frame ) flgs |= SWP_FRAMECHANGED;
                //if ( shp->layered ) flgs |= SWP_NOCOPYBITS | SWP_NOREDRAW;
                //if ( !rectchanged ) flgs |= SWP_NOSIZE | SWP_NOMOVE;
                rebuf = true;
            }
            if ( remaximization || reminimization )
                rebuf = true;

            if ( rebuf )
            {
                update_frame = shp->layered; // force draw for alphablend mode
                recreate_back_buffer( shp->d == D_MAX ? fssz.size() : shp->rect.size() );
            }

            if ( shp->layered ) // in alphablend mode we have to draw rect before resize
                shp->apply( update_frame, true );

            if ( rectchanged )
            {
                XMoveResizeWindow(istuff.X11, x11w, shp->rect.lt.x, shp->rect.lt.y, shp->rect.width(), shp->rect.height() );
                normal_rect = shp->rect;
            }
            int scmd = 0;
            if ( remaximization || reminimization )
            {
                enum
                {
                    SW_MINIMIZE,
                    SW_MAXIMIZE,
                    SW_RESTORE
                };

                //if ( GetCapture() == hwnd ) ReleaseCapture();
                scmd = shp->collapsed() ? SW_MINIMIZE : ( shp->d == D_MAX ? SW_MAXIMIZE : SW_RESTORE );
                bool curmin = is_collapsed();
                bool curmax = maxmon >= 0 && is_maximized();
                if ( curmin != shp->collapsed() || curmax != ( shp->d == D_MAX ) )
                {
                    // always reset maximize state
                    //if (0 != (WS_MAXIMIZE & GetWindowLongW( hwnd, GWL_STYLE )) )
                    //    ShowWindow( hwnd, SW_RESTORE );

                    irect sr = shp->rect;
                    switch ( scmd )
                    {
                    case SW_MAXIMIZE:
                        sr = fssz;
                        maxmon = m;
                        //MoveWindow( hwnd, sr.lt.x, sr.lt.y, sr.width(), sr.height(), !shp->layered );
                        if ( odp == D_MICRO )
                        {
                            //ShowWindow( hwnd, SW_SHOWNA );
                            //ShowWindow( hwnd, SW_RESTORE );
                            set_focus( reminimization );
                        }

                        break;
                    case SW_RESTORE:
                        maxmon = -1;
                        //MoveWindow(hwnd, sr.lt.x, sr.lt.y, sr.width(), sr.height(), !shp->layered);
                        //if ( odp == D_MICRO )
                        //    ShowWindow( hwnd, SW_SHOWNA );

                        istuff.move_x11w = x11w;
                        istuff.move_rect = sr;

                        // no break here
                    case SW_MINIMIZE:
                        maxmon = -1;
                        //ShowWindow( hwnd, scmd );
                        if ( shp->collapsed() )
                        {
                            //if ( shp->d == D_MICRO )
                            //    ShowWindow( hwnd, SW_HIDE );
                        }
                        else
                        {
                            set_focus( reminimization );
                        }
                        break;
                    }

                }
            }

            opacity( shp->opacity );

            shp->apply( update_frame, false );
            dp = shp->d;
            return;
        }

        // create visible window

        /*
        uint32 af = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        uint32 exf = (shp && shp->acceptfiles) ? WS_EX_ACCEPTFILES : 0;

        if ( shp && shp->taskbar )
        {
            af &= ~WS_POPUP;
            exf |= WS_EX_APPWINDOW | WS_OVERLAPPED;
        }

        if ( shp && (shp->layered || shp->opacity < 1.0f) )
            exf |= WS_EX_LAYERED;

        if ( shp && shp->collapsed() )
            af |= WS_MINIMIZE;

        if ( shp && shp->toolwindow )
        {
            af = WS_POPUP | WS_VISIBLE;
            exf = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        }
        */

        Window prnt = (shp && shp->parent) ? ptr_cast<x11_wnd_c *>( shp->parent )->x11w : DefaultRootWindow(istuff.X11);
        normal_rect = irect( 100, 100, 600, 400 );
        if ( shp ) normal_rect = shp->rect;

        ts::irect wrect = normal_rect;
        if (shp && shp->d == D_MAX)
        {
            wrect = shp->maxrect;
            if (wrect.area() == 0)
                maxmon = monitor_find_max_sz(ref_cast<irect>(shp->rect), wrect);
            else
                maxmon = monitor_find(wrect.center());
        }

        //creation_data_s cd;
        //cd.wnd = this;
        //cd.size = wrect.size();
        //cd.collapsed = ( af & WS_MINIMIZE ) != 0;

        //shp ? shp->name.cstr() : nullptr

        XSetWindowAttributes attrib = {};
        attrib.background_pixel = WhitePixel(istuff.X11, istuff.screen),
        attrib.border_pixel = BlackPixel(istuff.X11, istuff.screen),
        attrib.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
                    PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | PropertyChangeMask;

        mapped = false;
        x11w = XCreateWindow( istuff.X11, prnt, wrect.lt.x, wrect.lt.y, wrect.width(), wrect.height(), 1, CopyFromParent, InputOutput, CopyFromParent, 0, &attrib );

        /*
        Atom window_type = XInternAtom(istuff.X11, "_NET_WM_WINDOW_TYPE", False);
        int value = XInternAtom(istuff.X11, "_NET_WM_WINDOW_TYPE_SPLASH", False);
        XChangeProperty(istuff.X11, x11w, window_type, XA_ATOM, 32, PropModeReplace, (unsigned char *) &value, 1 );
        */

        istuff.add_window( x11w, this );

        recreate_back_buffer(wrect.size(), true);

        XSelectInput( istuff.X11, x11w, attrib.event_mask );


        if ( x11_wnd_c *p = istuff.x112wnd( prnt ) )
            p->add_child( this );

        if ( !shp || shp->visible )
        {
            //ShowWindow( hwnd, (!shp || shp->focus) ? SW_SHOW : SW_SHOWNA );
            XMapWindow( istuff.X11, x11w );
            XStoreName( istuff.X11, x11w, to_utf8(shp->name).cstr() );
            XMoveWindow(istuff.X11, x11w, wrect.lt.x, wrect.lt.y);

        }

        if ( shp ) dp = shp->d;

        /*
        if ( 0 != ( exf & WS_EX_APPWINDOW ) )
        {
            HICON oi = appicon;
            appicon = get_icon( false );
            SendMessageW( hwnd, WM_SETICON, ICON_SMALL, (LPARAM)appicon );
            SendMessageW( hwnd, WM_SETICON, ICON_BIG, (LPARAM)appicon );
            if ( oi ) istuff.release_icon( oi );
        }
        */

        DMSG( "create x11w: " << x11w );

        if ( shp )
        {
            opacity( shp->opacity );
            shp->apply( true, false );
        }
    }

    /*virtual*/ void try_close() override
    {
        //::PostMessageW ( hwnd, WM_CLOSE, 0, 0 );
    }

    /*virtual*/ void set_focus( bool bring_to_front ) override
    {
/*
        if ( hwnd && hwnd != GetFocus() )
        {
            DMSG( "focus: " << hwnd );
            SetFocus( hwnd );
        }
        if ( bring_to_front )
        {
            SetForegroundWindow( hwnd );
        }
*/
    }

    /*virtual*/ void flash() override
    {
  //      FlashWindow( hwnd, 0 );
    }

    /*virtual*/ void flush_draw( const irect &r ) override
    {
/*
        if ( flags.is(F_LAYERED) )
        {
            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT ptSrc; ptSrc.x = 0; ptSrc.y = 0; // start point of the copy from dcMemory to dcScreen

                                                   //HDC screen = GetDC(NULL);
            UpdateLayeredWindow( hwnd, nullptr,
                (POINT *)&ref_cast<POINT>( r.lt ),
                (SIZE *)&ref_cast<SIZE>( r.size() ),
                get_dbmp_dc( backbuffer ), &ptSrc, 0, &blendPixelFunction, ULW_ALPHA );
            //ReleaseDC( NULL, screen );

        }
        else
*/
            draw_back_buffer();

    }

    /*virtual*/ void set_capture() override
    {
  //      SetCapture( hwnd );
    }

    /*virtual*/ bmpcore_exbody_s get_backbuffer() override
    {
        return backbuffer.extbody();
    }


    struct modal_use_s
    {
        static void ticker()
        {
            if ( master().on_loop )
                master().on_loop();
        }

        modal_use_s()
        {
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            //++istuff.sysmodal;
            //master().is_sys_loop = istuff.sysmodal > 0;
            //if ( istuff.sysmodal == 1)
            //    istuff.timerid = SetTimer( nullptr, 0, 1, TimerProc );
        }
        ~modal_use_s()
        {
            master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            //--istuff.sysmodal;
            //master().is_sys_loop = istuff.sysmodal > 0;
            //if ( istuff.sysmodal == 0 )
            //    KillTimer( nullptr, istuff.timerid );
        }
    };

    /*virtual*/ wstr_c get_load_filename_dialog( const wsptr &iroot, const wsptr &name, filefilters_s &ff, const wchar *title ) override
    {
        modal_use_s muse;

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        wstr_c filter;
        if ( ff.filters.size() )
        {
            for ( const filefilter_s &e : ff.filters)
                filter.append(e.desc).append_char('/').append(e.wildcard).append_char('/');
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wstr_c buffer;
        buffer.set_length( 2048 );
        buffer.set( name );


        buffer.set_length( CHARz_len( buffer.cstr() ) );
        return buffer;

    }
    /*virtual*/ bool get_load_filename_dialog( wstrings_c &files, const wsptr &iroot, const wsptr& name, ts::filefilters_s & ff, const wchar *title ) override
    {
        modal_use_s muse;

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        wstr_c filter;
        if ( ff.filters.size() )
        {
            for ( const filefilter_s &e : ff.filters )
                filter.append(e.desc).append_char('/').append(e.wildcard).append_char('/');
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wstr_c buffer( 16384, true );
        buffer.set( name );

        files.clear();


        /*
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
        */

        return true;
    }

    /*virtual*/ wstr_c  get_save_directory_dialog( const wsptr &root, const wsptr &title, const wsptr &selectpath, bool nonewfolder ) override
    {
        wstr_c buffer;
        buffer.clear();


        return buffer;
    }

    /*virtual*/ wstr_c get_save_filename_dialog( const wsptr &iroot, const wsptr &name, filefilters_s &ff, const wchar *title ) override
    {
        ts::wstr_c filter;
        if ( ff.filters.size() )
        {
            for ( const filefilter_s &e : ff.filters )
                filter.append(e.desc).append_char('/').append(e.wildcard).append_char('/');
            filter.append_char( '/' );
            filter.replace_all( '/', 0 );
        }

        wstr_c root( iroot );
        fix_path( root, FNO_FULLPATH );

        wstr_c buffer( MAX_PATH_LENGTH + 16, true );
        buffer.set( name );


        return buffer;
    }
};

void x11expose(x11_wnd_c * wnd, bool mapnotify)
{
    wnd->mapped |= mapnotify;
    wnd->cbs->evt_draw();
    wnd->draw_back_buffer();
}

Window wnd2x11( const wnd_c * w )
{
    if (!w)
        return (Window)0;

    return ((x11_wnd_c *)w)->x11w;
}

void x11configure(x11_wnd_c *w, const XConfigureEvent &ce)
{
    if (!w->mapped)
        return;

    w->normal_rect.lt.x = ce.x;
    w->normal_rect.lt.y = ce.y;
    w->normal_rect.rb.x = ce.x + ce.width;
    w->normal_rect.rb.y = ce.y + ce.height;
    w->cbs->evt_refresh_pos( w->normal_rect, D_NORMAL );
}



