#include "nix_common.inl"
#include <spawn.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <string.h>
#include <stdlib.h>


TS_STATIC_CHECK( sizeof( master_internal_stuff_s ) <= MASTERCLASS_INTERNAL_STUFF_SIZE, "bad size" );


sys_master_nix_c::sys_master_nix_c()
{
    master_internal_stuff_s *istuff = (master_internal_stuff_s *)&internal_stuff;
    TSPLACENEW( istuff );
    XInitThreads();
}

sys_master_nix_c::~sys_master_nix_c()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    istuff.~master_internal_stuff_s();
}


extern "C" __attribute__((weak)) int main(int argc, char* argv[])
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

    //ts::wchar *cmdlb = GetCommandLineW();
    str_c cmdla( asptr((const char *)argv[0]) );

    for(int i=1;i<argc;++i)
    {
        int cl = cmdla.get_length()+1;
        cmdla.append_char(' ').append(argv[i]);

        if (cmdla.find_pos(cl,' ') > 0)
            cmdla.insert(cl,'\"').append_char('\"');
    }
    if (!app_preinit(from_utf8(cmdla.as_sptr()))) return 0;

    master().do_app_loop();

    return 0;
}



namespace
{
    struct thread_param_s
    {
        sys_master_c::_HANDLER_T tp;
        void run()
        {
            sys_master_c::_HANDLER_T tp1 = tp;
            TSDEL( this );
            tp1();
        }
    };
}

static void * thread_stub( void *p )
{
    ts::tmpalloc_c tmp;
    thread_param_s *tp = (thread_param_s *)p;
    tp->run();
    return nullptr;
}


/*virtual*/ void sys_master_nix_c::sys_start_thread( _HANDLER_T thr )
{
    thread_param_s * tp = TSNEW(thread_param_s);
    tp->tp = thr;

    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

    pthread_t tid;
    pthread_create(&tid, &threadAttr, thread_stub, tp);
    //pthread_attr_destroy(&threadAttr);
}

/*virtual*/ wstr_c sys_master_nix_c::sys_language()
{
    ts::wchar x[ 32 ] = { 'e', 'n', 0 };

    char *s = getenv("LANG");
    if (s != nullptr)
    {
        x[0] = s[0];
        x[1] = s[1];
    }

    return ts::wstr_c( ts::wsptr( x, 2 ) );
}

/*virtual*/ int sys_master_nix_c::sys_detect_autostart( ts::wstr_c &cmdpar )
{
    STOPSTUB("");

    return 0;

}
/*virtual*/ void sys_master_nix_c::sys_autostart( const ts::wsptr &appname, const ts::wstr_c &exepath, const ts::wsptr &cmdpar )
{
    STOPSTUB("");
}


/*virtual*/ global_atom_s *sys_master_nix_c::sys_global_atom( const ts::wstr_c &n )
{
    sem_t *sem = sem_open( to_utf8(n), O_CREAT|O_EXCL );
    if ( sem == SEM_FAILED )
        return nullptr;

    class my_global_atom_c : public global_atom_s
    {
        sem_t *sem;

        my_global_atom_c( const my_global_atom_c & ) UNUSED;
        my_global_atom_c( my_global_atom_c && ) UNUSED;
        my_global_atom_c &operator =( const my_global_atom_c & ) UNUSED;
        my_global_atom_c &operator =( my_global_atom_c && ) UNUSED;

    public:

        my_global_atom_c( sem_t *sem ) :sem(sem) {}
        ~my_global_atom_c()
        {
            sem_close(sem);
        }
    };

    return TSNEW( my_global_atom_c, sem );
}

/*virtual*/ bool sys_master_nix_c::sys_one_instance( const ts::wstr_c &n, _HANDLER_T notify_cb )
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;

#if 0
    istuff.popup_event = CreateEventW( nullptr, FALSE, FALSE, n );
    if ( !istuff.popup_event ) return true;

    if ( ERROR_ALREADY_EXISTS == GetLastError() )
    {
        // second instance
        SetEvent( istuff.popup_event );
        CloseHandle( istuff.popup_event );
        istuff.popup_event = nullptr;
        return false;
    }

    istuff.popup_notify = notify_cb;

    CloseHandle( CreateThread( nullptr, 0, multiinstanceblocker, nullptr, 0, nullptr ) );
#endif
    STOPSTUB("");
    return true;
}

/*virtual*/ void sys_master_nix_c::shutdown()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
#if 0
    if ( istuff.popup_event )
    {
        HANDLE h = istuff.popup_event;
        istuff.popup_event = nullptr;
        if ( h ) SetEvent( h );
    }
#endif
    STOPSTUB("");
}

static void looper()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

    istuff.looper = true;
    istuff.looper_staring = false;

    while ( !master().is_shutdown && !master().is_app_need_quit )
    {
        ts::Time ct = ts::Time::current();

        if ( master().is_sys_loop )
        {
            istuff.cnttick = 0;
            istuff.looper_allow_tick = true;
            istuff.lasttick = ct;
            Sleep( 100 );
            continue;
        }

        int d = ct - istuff.lasttick;
        int sleep = master().sleep;

        if ( master().mainwindow && ( d > 100 || istuff.looper_allow_tick ) )
        {
            Window w = wnd2x11( master().mainwindow );

            if ( d > 1000 )
                istuff.cnttick = 0;

            istuff.looper_allow_tick = false;
            if ( istuff.cnttick < 10 )
            {
                ++istuff.cnttick;
                istuff.lasttick = ct;

                XEvent event = {};
                //event.xclient.window = 0;
                event.xclient.type = ClientMessage,
                event.xclient.message_type = EVT_LOOPER_TICK,
                event.xclient.format = 8,

                //memcpy(&event.xclient.data.s[2], &data, sizeof(void*));

                XSendEvent(istuff.X11, w, False, 0, &event);
                XFlush(istuff.X11);

            }
        }
        else if ( !istuff.looper_allow_tick )
            sleep = 1;

        if ( sleep >= 0 )
            Sleep( sleep );
    }
    istuff.looper = false;
    return;
}

/*virtual*/ void sys_master_nix_c::sys_loop()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    istuff.prepare();

    if ( !istuff.looper && !istuff.looper_staring )
    {
        istuff.looper_staring = true;
        ts::master().sys_start_thread( looper );
    }

    XEvent e;
    while( !master().is_shutdown && !master().is_app_need_quit )
    {
        XNextEvent( istuff.X11, &e );

        switch( e.type )
        {
        case ClientMessage:
            if (e.xclient.message_type == EVT_LOOPER_TICK)
            {
                --istuff.cnttick;
                if ( istuff.cnttick > 0)
                {
                    //MSG msgm;
                    //while ( PeekMessage( &msgm, nullptr, WM_LOOPER_TICK, WM_LOOPER_TICK, PM_REMOVE ) )
                        --istuff.cnttick;
                }
                istuff.cnttick = 0;

                if ( master().on_loop )
                    master().on_loop();

                istuff.lasttick = Time::current();
                istuff.looper_allow_tick = true;

            }
            break;
        case ButtonPress: {
            ivec2 clp( e.xbutton.x, e.xbutton.y );
            ivec2 scrp(e.xbutton.x_root, e.xbutton.y_root);

            mouse_event_e me = MEVT_NOPE;
            switch(e.xbutton.button)
            {
            case Button1: me = MEVT_LDOWN; break;
            case Button2: me = MEVT_MDOWN; break;
            case Button3: me = MEVT_RDOWN; break;
            }

            if (me != MEVT_NOPE)
            {
                master().on_mouse( me, clp, scrp );
                if (wnd_c *wnd = (wnd_c *)istuff.x112wnd(e.xbutton.window))
                    wnd->get_callbacks()->evt_mouse( me, clp, scrp );
            }
            break;
        }

        case ButtonRelease: {
            ivec2 clp( e.xbutton.x, e.xbutton.y );
            ivec2 scrp(e.xbutton.x_root, e.xbutton.y_root);

            mouse_event_e me = MEVT_NOPE;
            switch(e.xbutton.button)
            {
            case Button1: me = MEVT_LUP; break;
            case Button2: me = MEVT_MUP; break;
            case Button3: me = MEVT_RUP; break;
            }

            if (me != MEVT_NOPE)
            {
                master().on_mouse( me, clp, scrp );
                if (wnd_c *wnd = (wnd_c *)istuff.x112wnd(e.xbutton.window))
                    wnd->get_callbacks()->evt_mouse( me, clp, scrp );
            }
            break;
        }
        case MotionNotify:
            {
                //Window cw = 0;
                //int x = 0, y = 0;
                //XTranslateCoordinates(istuff.X11, e.xbutton.window, istuff.root, e.xbutton.x, e.xbutton.y, &x, &y, &cw);
                ivec2 clp( e.xmotion.x, e.xmotion.y );
                ivec2 scrp(e.xmotion.x_root, e.xmotion.y_root);
                master().on_mouse(MEVT_MOVE, clp, scrp);
                if (wnd_c *wnd = (wnd_c *)istuff.x112wnd(e.xbutton.window))
                    wnd->get_callbacks()->evt_mouse( MEVT_MOVE, clp, scrp );
            }
            break;

        case Expose:
        case MapNotify:
            if (x11_wnd_c *wnd = istuff.x112wnd(e.xany.window))
                x11expose(wnd, e.type == MapNotify);

            XFlush( istuff.X11 );
            break;
        case ConfigureNotify: {
            if (x11_wnd_c *wnd = istuff.x112wnd(e.xconfigure.window))
                x11configure(wnd,e.xconfigure);


            } break;
        case KeyRelease:
            //if( XLookupKeysym( &e.xkey, 0 ) == XK_Escape )
            //    do_loop = false;
            break;

        }
    }

    DEBUG_BREAK();
}

/*virtual*/ void sys_master_nix_c::sys_idle()
{
#if 0
    MSG m;
    if ( PeekMessageW( &m, nullptr, 0U, 0U, PM_REMOVE ) )
    {
        TranslateMessage( &m );
        DispatchMessageW( &m );
    }
#endif
    STOPSTUB("");

    if ( master().on_loop )
        master().on_loop();

}
/*virtual*/ void sys_master_nix_c::sys_exit( int iErrCode )
{
#if 0
    if (master().mainwindow)
        master().mainwindow->get_callbacks()->evt_on_exit();
    is_app_need_quit = true;
    PostQuitMessage( iErrCode );
#endif
    STOPSTUB("");
}

/*virtual*/ wnd_c *sys_master_nix_c::get_focus()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    Window w = 0; int r;
    XGetInputFocus(istuff.X11, &w, &r);
    if (wnd_c *wnd = (wnd_c *)istuff.x112wnd(w))
        return wnd;

    return nullptr;
}

/*virtual*/ wnd_c *sys_master_nix_c::get_capture()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    if (istuff.grabbing)
    {
        Window w = 0; int r;
        XGetInputFocus(istuff.X11, &w, &r);
        if (wnd_c *wnd = (wnd_c *)istuff.x112wnd(w))
            if (wnd == istuff.grabbing.get())
                return wnd;
    }

    return nullptr;
}

/*virtual*/ void sys_master_nix_c::release_capture()
{

    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    XUngrabPointer(istuff.X11, CurrentTime);
    istuff.grabbing = nullptr;
}

/*virtual*/ void sys_master_nix_c::set_notification_icon_text( const ts::wsptr& text )
{
    if ( !mainwindow ) return;

    //STOPSTUB("");
    UNFINISHED(aaa);
}

namespace
{
    struct phi_s
    {
        pid_t pid;
    };
}

process_handle_s::~process_handle_s()
{
    //if ( ( ( phi_s* )this )->processhandle )
    //    CloseHandle( ( (phi_s*)this )->processhandle );
}


/*virtual*/ bool sys_master_nix_c::start_app( const wstr_c &exe, const wstrings_c& prmsa, process_handle_s *process_handle, bool elevate )
{
    astrings_c prms;
    for (const wstr_c& s : prmsa)
    {
        if (s.find_pos(' ') >= 0)
        {
            str_c &p = prms.add();
            p.append_char('\"');
            p.append(to_utf8(s));
            p.append_char('\"');
        } else
            prms.add(to_utf8(s));
    }

    str_c exe_a( to_utf8(exe) );

    int pcnt = prms.size();
    const char ** argv = (const char **)alloca( sizeof(char *) * (pcnt + 2) );
    argv[0] = exe_a.cstr();
    for(int i=0;i<pcnt;++i)
        argv[i+1] = prms.get(i).cstr();
    argv[pcnt+1] = nullptr;


    pid_t pid;
    int status = posix_spawn(&pid, exe_a.cstr(), nullptr, nullptr, (char * const*)argv, nullptr);

    if (status == 0)
    {
        if (process_handle)
            ((phi_s *)process_handle)->pid = pid;

        return true;
    }

    return false;
}

/*virtual*/ bool sys_master_nix_c::wait_process( process_handle_s &phandle, int timeoutms )
{
    if (timeoutms == 0)
    {
        int status = 0;
        waitpid(((phi_s*)&phandle)->pid, &status, 0);
        return true;
    }

    int stopwaittime = timeGetTime() + timeoutms;
    for(;;)
    {
        if (getpgid(((phi_s*)&phandle)->pid) >= 0)
        {
            if ( (timeGetTime() - stopwaittime) > 0 )
                return false;

            Sleep(100);
            continue;
        }

        break;
    }

    return true;
}

/*virtual*/ bool sys_master_nix_c::open_process( uint32 processid, process_handle_s &phandle )
{
    pid_t pid = processid;

    if (getpgid(pid) >= 0)
    {
        ((phi_s*)&phandle)->pid = pid;
        return true;
    }

    return false;
}

/*virtual*/ uint32 sys_master_nix_c::process_id()
{
    return getpid();
}

/*virtual*/ void sys_master_nix_c::sys_beep( sys_beep_e beep )
{
    STOPSTUB("");
#if 0
    switch ( beep )
    {
    case ts::SBEEP_INFO:
        MessageBeep( MB_ICONINFORMATION );
        break;
    case ts::SBEEP_WARNING:
        MessageBeep( MB_ICONWARNING );
        break;
    case ts::SBEEP_ERROR:
        MessageBeep( MB_ICONERROR );
        break;
    case ts::SBEEP_BADCLICK:
        MessageBeep( 0xFFFFFFFF );
        break;
    }
#endif

}

/*virtual*/ void sys_master_nix_c::hide_hardware_cursor()
{
    STOPSTUB("");
}
/*virtual*/ void sys_master_nix_c::show_hardware_cursor()
{
    STOPSTUB("");
}
/*virtual*/ void sys_master_nix_c::set_cursor( cursor_e ct )
{
    UNFINISHED(set_cursor);
}

/*virtual*/ int sys_master_nix_c::get_system_info( sysinf_e si )
{
    UNFINISHED(get_system_info);

    switch ( si )
    {
    case SINF_SCREENSAVER_RUNNING:
        {
            //BOOL scrsvrun = FALSE;
            //SystemParametersInfoW( SPI_GETSCREENSAVERRUNNING, 0, &scrsvrun, 0 );
            //if ( scrsvrun ) return 1;
            return 0;
        }
    case SINF_LAST_INPUT:
        {
            //LASTINPUTINFO lii = { ( UINT )sizeof( LASTINPUTINFO ) };
            //GetLastInputInfo( &lii );
            //return ( GetTickCount() - lii.dwTime ) / 1000;
            return 0;
        }
    }

    return 0;
}

/*virtual*/ bool sys_master_nix_c::is_key_pressed( key_scan_e k )
{
	STOPSTUB("");

    int vk = -1;
    switch ( k )
    {
    case SSK_LCTRL:
        //vk = VK_LCONTROL;
        break;
    case SSK_RCTRL:
        //vk = VK_RCONTROL;
        break;
    case SSK_CTRL:
        //vk = VK_CONTROL;
        break;
    case SSK_SHIFT:
        //vk = VK_SHIFT;
        break;
    case SSK_LSHIFT:
        //vk = VK_LSHIFT;
        break;
    case SSK_RSHIFT:
        //vk = VK_RSHIFT;
        break;
    case SSK_ALT:
        //vk = VK_MENU;
        break;
    case SSK_LALT:
        //vk = VK_LMENU;
        break;
    case SSK_RALT:
        //vk = VK_RMENU;
        break;
    }

//    if (vk >= 0)
//        return ( GetAsyncKeyState( vk ) & 0x8000 ) == 0x8000;

    return false;
}


/*virtual*/ uint32 sys_master_nix_c::folder_spy(const ts::wstr_c &path, folder_spy_handler_s *handler)
{
//    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
//    master_internal_stuff_s::spystate_s::maintenance_s &m = istuff.fspystate.lock_write()().threads.add();
//    m.h = handler;
//    CloseHandle(CreateThread(NULL, 0, spy_folder, (LPVOID)path.cstr(), 0, &m.threadid));

//    for (;;)
//    {
//        Sleep(0);
//        if (m.started) break;
//    }

//    return m.threadid;
  return 0;
}

/*virtual*/ void sys_master_nix_c::folder_spy_stop(uint32 spyid)
{
/*
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    auto s = istuff.fspystate.lock_write();
    s().stop(spyid);
    s.unlock();

    for (;;)
    {
        Sleep(0);

        {
            auto sr = istuff.fspystate.lock_read();

            bool wait_again = false;
            int cnt = sr().threads.size();
            for (int i = 0; i < cnt; ++i)
            {
                const auto & _m = sr().threads.get(i);
                if (spyid != 0 && spyid != _m.threadid)
                    continue;
                if (!_m.finished)
                    wait_again = true;
            }

            if (wait_again) continue;
        }

        break;
    }

    if (spyid == 0)
    {
        auto ss = istuff.fspystate.lock_write();
        ss().threads.clear();
        ss.unlock();
    }
*/

}


struct x11button : movable_flag<true>
{
    int x, y;
    int width, height;
    int textx, texty;
    str_c text;
    smbr_e r;
    int clicked = 0;
    bool mouseover = false;

    void draw( int fg, int bg, Display* dpy, Window w, GC gc )
    {
        if( mouseover )
        {
            XFillRectangle( dpy, w, gc, clicked+x, clicked+y, width, height );
            XSetForeground( dpy, gc, bg );
            XSetBackground( dpy, gc, fg );
        }
        else
        {
            XSetForeground( dpy, gc, fg );
            XSetBackground( dpy, gc, bg );
            XDrawRectangle( dpy, w, gc, x, y, width, height );
        }

        XDrawString( dpy, w, gc, clicked+textx, clicked+texty, text.cstr(), text.get_length() );
        XSetForeground( dpy, gc, fg );
        XSetBackground( dpy, gc, bg );
    }


    bool is_point_inside( int px, int py )
    {
        return px>=x && px<(x+width) && py>=y && py<(y+height);
    }

};


smbr_e TSCALL sys_master_nix_c::sys_mb( const wchar *caption, const wchar *wtext, smb_e options )
{
    //master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;

    const char* wmDeleteWindow = "WM_DELETE_WINDOW";
    int height = 0, X, Y, W=0, H;
    size_t i, lines = 0;
    char *atom;
    array_inplace_t<x11button,0> buttons;
    XFontStruct* font;
    XSizeHints hints;
    XEvent e;

    auto addb = [&]( const asptr &btext, smbr_e r )
    {
        x11button &b = buttons.add();
        b.r = r;

        /* Compute the shape of the OK button */
        XCharStruct overall;
        int direction, ascent, descent;
        XTextExtents( font, btext.s, btext.l, &direction, &ascent, &descent, &overall );

        b.width = overall.width + 30;
        b.height = ascent + descent + 5;
        //b.x = W/2 - b.width/2;
        b.y = H   - height - 15;
        //b.textx = b.x + 15;
        b.texty = b.y + b.height - 3;
        b.text.set(btext);

        int allbw = 0;
        for(x11button &t : buttons)
            allbw += t.width + 10;

        int bx = W-allbw - 10;
        for(x11button &t : buttons)
            t.x = bx, bx += t.width + 10, t.textx = t.x + 15;

    };

    /* Open a display */
    Display* dpy;
    if( !(dpy = XOpenDisplay( 0 )) )
        return SMBR_UNKNOWN;

    /* Get us a white and black color */
    int backcol = 0x00aaaaaa;//BlackPixel( dpy, DefaultScreen(dpy) );
    int textcol = 0;//WhitePixel( dpy, DefaultScreen(dpy) );

    /* Create a window with the specified title */
    Window w = XCreateSimpleWindow( dpy, DefaultRootWindow(dpy), 0, 0, 100, 100,
                             0, backcol, backcol );

    XSelectInput( dpy, w, ExposureMask | StructureNotifyMask |
                          KeyReleaseMask | PointerMotionMask |
                          ButtonPressMask | ButtonReleaseMask );

    XMapWindow( dpy, w );
    XStoreName( dpy, w, to_utf8(caption).cstr() );

    Atom wmDelete = XInternAtom( dpy, wmDeleteWindow, True );
    XSetWMProtocols( dpy, w, &wmDelete, 1 );

    /* Create a graphics context for the window */
    GC gc = XCreateGC( dpy, w, 0, 0 );

    XSetForeground( dpy, gc, textcol );
    XSetBackground( dpy, gc, backcol );

    /* Compute the printed width and height of the text */
    if( !(font = XQueryFont( dpy, XGContextFromGC(gc) )) )
    {
        XFreeGC( dpy, gc );
        XDestroyWindow( dpy, w );
        XCloseDisplay( dpy );
        return SMBR_UNKNOWN;
    }

    str_c text( to_utf8(wtext) );

    for(token<char> tkn(text,'\n');tkn;++tkn, ++lines)
    {
        XCharStruct overall;
        int direction, ascent, descent;

        XTextExtents( font, tkn->as_sptr().s, tkn->get_length(), &direction, &ascent, &descent, &overall );
        W = overall.width>W ? overall.width : W;
        height = (ascent+descent)>height ? (ascent+descent) : height;

    }

    /* Compute the shape of the window and adjust the window accordingly */
    W += 20;
    H = lines*height + height + 40;
    X = DisplayWidth ( dpy, DefaultScreen(dpy) )/2 - W/2;
    Y = DisplayHeight( dpy, DefaultScreen(dpy) )/2 - H/2;

    XMoveResizeWindow( dpy, w, X, Y, W, H );

    switch ( options )
    {
    case SMB_OK_ERROR:
        //f |= MB_ICONERROR;
        // no break here
    case SMB_OK:
        //f |= MB_OK;
        addb(CONSTASTR("OK"), SMBR_OK);
        break;
    case SMB_OKCANCEL:
        //f |= MB_OKCANCEL;
        addb(CONSTASTR("OK"), SMBR_OK);
        addb(CONSTASTR("Cancel"), SMBR_CANCEL);
        break;
    case SMB_YESNOCANCEL:
        addb(CONSTASTR("Yes"), SMBR_YES);
        addb(CONSTASTR("No"), SMBR_NO);
        addb(CONSTASTR("Cancel"), SMBR_CANCEL);
        break;
    case SMB_YESNO_ERROR:
        //f |= MB_ICONERROR;
        // no break here
    case SMB_YESNO:
        //f |= MB_YESNO;
        addb(CONSTASTR("Yes"), SMBR_YES);
        addb(CONSTASTR("No"), SMBR_NO);

        break;
    default:
        break;
    }



    XFreeFontInfo( NULL, font, 1 ); /* We don't need that anymore */

    /* Make the window non resizeable */
    XUnmapWindow( dpy, w );

    hints.flags      = PSize | PMinSize | PMaxSize;
    hints.min_width  = hints.max_width  = hints.base_width  = W;
    hints.min_height = hints.max_height = hints.base_height = H;

    XSetWMNormalHints( dpy, w, &hints );
    XMapRaised( dpy, w );
    XFlush( dpy );

    /* Event loop */
    smbr_e r = SMBR_UNKNOWN;
    bool do_loop = true;
    while( do_loop )
    {
        XNextEvent( dpy, &e );
        for(x11button &b : buttons)
            b.clicked = 0;

        if( e.type == MotionNotify )
        {
            for(x11button &b : buttons)
            {
                if( b.is_point_inside( e.xmotion.x, e.xmotion.y ) )
                {
                    if( !b.mouseover )
                        e.type = Expose;

                    b.mouseover = true;
                }
                else
                {
                    if( b.mouseover )
                        e.type = Expose;

                    b.mouseover = false;
                    b.clicked = 0;
                }
            }

        }

        switch( e.type )
        {
        case ButtonPress:
        case ButtonRelease:
            if( e.xbutton.button!=Button1 )
                break;

            for(x11button &b : buttons)
            {
                if( b.mouseover )
                {
                    b.clicked = e.type==ButtonPress ? 1 : 0;

                    if( !b.clicked )
                    {
                        do_loop = false;
                        r = b.r;
                        break;
                    }
                }
                else
                {
                    b.clicked = 0;
                }
            }


        case Expose:
        case MapNotify:
            XClearWindow( dpy, w );

            /* Draw text lines */
            i=0;
            for(token<char> tkn(text,'\n');tkn;++tkn, ++lines, i+=height)
                XDrawString( dpy, w, gc, 10, 10+height+i, tkn->as_sptr().s, tkn->get_length() );

            for(x11button &b : buttons)
                b.draw(textcol, backcol, dpy, w, gc );

            XFlush( dpy );
            break;

        case KeyRelease:
            if( XLookupKeysym( &e.xkey, 0 ) == XK_Escape )
                do_loop = false;
            break;

        case ClientMessage:
            atom = XGetAtomName( dpy, e.xclient.message_type );

            if( *atom == *wmDeleteWindow )
                do_loop = false;

            XFree( atom );
            break;
        }
    }

    XFreeGC( dpy, gc );
    XDestroyWindow( dpy, w );
    XCloseDisplay( dpy );


    return r;
}
