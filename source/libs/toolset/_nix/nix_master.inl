#include "nix_common.inl"

TS_STATIC_CHECK( sizeof( master_internal_stuff_s ) <= MASTERCLASS_INTERNAL_STUFF_SIZE, "bad size" );


sys_master_nix_c::sys_master_nix_c()
{
    master_internal_stuff_s *istuff = (master_internal_stuff_s *)&internal_stuff;
    TSPLACENEW( istuff );
}

sys_master_nix_c::~sys_master_nix_c()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    istuff.~master_internal_stuff_s();
}

/*virtual*/ void sys_master_nix_c::sys_start_thread( _HANDLER_T thr )
{
    //thread_param_s * tp = TSNEW_T( MEMT_MASTER, thread_param_s );
    //tp->tp = thr;

    //CloseHandle( CreateThread( nullptr, 0, thread_stub, tp, 0, nullptr ) );
    STOPSTUB("");
}

/*virtual*/ wstr_c sys_master_nix_c::sys_language()
{
    STOPSTUB("");
    ts::wchar x[ 32 ] = { 'e', 'n', 0 };
    //GetLocaleInfoW( NULL, LOCALE_SISO639LANGNAME, x, 32 );
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
#if 0
    HANDLE mutex = CreateMutexW( nullptr, FALSE, n );
    if ( !mutex )
        return nullptr;

    if ( ERROR_ALREADY_EXISTS == GetLastError() )
    {
        CloseHandle( mutex );
        return nullptr;
    }

    class my_global_atom_c : public global_atom_s
    {
        HANDLE mutex;

        my_global_atom_c( const my_global_atom_c & ) UNUSED;
        my_global_atom_c( my_global_atom_c && ) UNUSED;
        my_global_atom_c &operator =( const my_global_atom_c & ) UNUSED;
        my_global_atom_c &operator =( my_global_atom_c && ) UNUSED;

    public:

        my_global_atom_c( HANDLE mutex ) :mutex( mutex ) {}
        ~my_global_atom_c()
        {
            CloseHandle( mutex );
        }
    };

    return TSNEW( my_global_atom_c, mutex );
#endif

    STOPSTUB("");
	return nullptr;
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

/*virtual*/ void sys_master_nix_c::sys_loop()
{
    STOPSTUB("");
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

    if ( master().on_loop )
        master().on_loop();
#endif
    STOPSTUB("");

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
#if 0
    HWND fhwnd = GetFocus();
    if ( !fhwnd ) return nullptr;
    return (wnd_c *)hwnd2wnd( fhwnd );
#endif
    STOPSTUB("");

return nullptr;
}

/*virtual*/ wnd_c *sys_master_nix_c::get_capture()
{
#if 0
    HWND chwnd = GetCapture();
    if ( !chwnd ) return nullptr;
    return (wnd_c *)hwnd2wnd(chwnd);
#endif
    STOPSTUB("");

return nullptr;
}

/*virtual*/ void sys_master_nix_c::release_capture()
{
    STOPSTUB("");
}

/*virtual*/ void sys_master_nix_c::set_notification_icon_text( const ts::wsptr& text )
{
    if ( !mainwindow ) return;

    STOPSTUB("");
}

process_handle_s::~process_handle_s()
{
    //if ( ( ( phi_s* )this )->processhandle )
    //    CloseHandle( ( (phi_s*)this )->processhandle );
}


/*virtual*/ bool sys_master_nix_c::start_app( const wstr_c &exe, const wstrings_c& prms, process_handle_s *process_handle, bool elevate )
{
    STOPSTUB("");
    return false;
}

/*virtual*/ bool sys_master_nix_c::wait_process( process_handle_s &phandle, int timeoutms )
{
    STOPSTUB("");
#if 0
    if ( ( (phi_s*)&phandle )->processhandle )
    {
        DWORD r = WaitForSingleObject( ( (phi_s*)&phandle )->processhandle, timeoutms > 0 ? timeoutms : INFINITE );
        if ( r == WAIT_TIMEOUT ) return false;
    }
#endif
    return true;
}

/*virtual*/ bool sys_master_nix_c::open_process( uint32 processid, process_handle_s &phandle )
{
    STOPSTUB("");
#if 0
    HANDLE h = OpenProcess( SYNCHRONIZE, FALSE, processid );
    if ( h )
    {
        if ( ( (phi_s*)&phandle )->processhandle )
            CloseHandle( ( (phi_s*)&phandle )->processhandle );

        ( (phi_s*)&phandle )->processhandle = h;
        return true;
    }
#endif

    return false;
}

/*virtual*/ uint32 sys_master_nix_c::process_id()
{
    STOPSTUB("");
    //return GetCurrentProcessId();
return 0;
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
    STOPSTUB("");
}

/*virtual*/ int sys_master_nix_c::get_system_info( sysinf_e si )
{
#if 0
    switch ( si )
    {
    case SINF_SCREENSAVER_RUNNING:
        {
            BOOL scrsvrun = FALSE;
            SystemParametersInfoW( SPI_GETSCREENSAVERRUNNING, 0, &scrsvrun, 0 );
            if ( scrsvrun ) return 1;
            return 0;
        }
    case SINF_LAST_INPUT:
        {
            LASTINPUTINFO lii = { ( UINT )sizeof( LASTINPUTINFO ) };
            GetLastInputInfo( &lii );
            return ( GetTickCount() - lii.dwTime ) / 1000;
        }
    }
#endif
	STOPSTUB("");

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
