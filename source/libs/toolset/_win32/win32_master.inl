#include "win32_common.inl"

#pragma comment(lib, "IPHLPAPI.lib") // GetAdaptersInfo
#pragma comment(lib, "Msimg32.lib") // AlphaBlend


TS_STATIC_CHECK( sizeof( master_internal_stuff_s ) <= MASTERCLASS_INTERNAL_STUFF_SIZE, "bad size" );

#define WM_LOOPER_TICK (WM_USER + 1111)

/*
static LRESULT CALLBACK looper_proc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    if ( WM_LOOPER_TICK == message )
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

        if ( master().on_loop )
            master().on_loop();

        istuff.lasttick = timeGetTime();
        istuff.looper_allow_tick = true;
        --istuff.cnttick;
        return 0;
    }

    return DefWindowProcW( hWnd, message, wParam, lParam );
}
*/

static DWORD WINAPI looper( void * )
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;

    istuff.looper = true;
    istuff.looper_staring = false;
    
    while ( !master().is_shutdown && !master().is_app_need_quit )
    {
        DWORD ct = timeGetTime();
        int d = (int)( ct - istuff.lasttick );

        int sleep = master().sleep;
        if ( master().is_sys_loop )
            sleep = 100;

        if ( master().is_sys_loop )
        {
            istuff.cnttick = 0;
            istuff.looper_allow_tick = true;
            istuff.lasttick = ct;
            Sleep( 100 );
            continue;
        }

        if ( master().mainwindow && ( d > 100 || istuff.looper_allow_tick ) )
        {
            DWORD mainthreadid = GetWindowThreadProcessId( wnd2hwnd( master().mainwindow ), nullptr );

            istuff.looper_allow_tick = false;
            if ( istuff.cnttick < 10 )
            {
                ++istuff.cnttick;
                istuff.lasttick = ct;
                PostThreadMessageW( mainthreadid, WM_LOOPER_TICK, 0, 0 );
            }
        }
        else if ( !istuff.looper_allow_tick )
            sleep = 1;

        if ( sleep >= 0 )
            Sleep( sleep );
    }
    istuff.looper = false;
    return 0;
}

} // namespace ts

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int )
{
    ts::master_internal_stuff_s &istuff = *(ts::master_internal_stuff_s *)&ts::master().internal_stuff;
    istuff.inst = hInstance;

    wchar_t *cmdlb = GetCommandLineW();
    if ( !ts::app_preinit( cmdlb ) ) return 0;

    ts::master().do_app_loop();
    CoUninitialize();
    return 0;
}

namespace ts {

sys_master_win32_c::sys_master_win32_c()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    istuff.master_internal_stuff_s::master_internal_stuff_s();
}

sys_master_win32_c::~sys_master_win32_c()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    istuff.~master_internal_stuff_s();
}

static HBITMAP gen_hbitmap( const bitmap_c& bmp )
{
    ASSERT( bmp.info().bytepp() == 4 );

    int w = bmp.info().sz.x;
    int h = bmp.info().sz.y;

    HWND hwnd = GetDesktopWindow();
    HDC display_dc = GetDC( hwnd );

    // Define the header
    BITMAPINFO bmi;
    memset( &bmi, 0, sizeof( bmi ) );
    bmi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = w * h * 4;

    // Create the pixmap
    uint8 *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection( display_dc, &bmi, DIB_RGB_COLORS, (void **)&pixels, 0, 0 );
    ReleaseDC( 0, display_dc );

    if ( !bitmap )
        return nullptr;

    if ( !pixels )
        return nullptr;

    // Copy over the data
    int bytes_per_line = w * 4;
    for ( int y = 0; y < h; ++y )
        memcpy( pixels + y * bytes_per_line, bmp.body( ivec2( 0, y ) ), bytes_per_line );

	uint32 *e = (uint32 *)pixels; e += w * h;
	for (uint32 *c = (uint32 *)pixels; c < e; ++c)
		if (*c) 
			return bitmap;

	*(uint32 *)pixels = 0x01808080; // stupid zero bitmap bug

	return bitmap;
}

static uint32 calc_hash( const bitmap_c &b )
{
    union
    {
        uint32 crc;
        uint8 crc4[ 4 ];
    } crc;

    crc.crc = 0;
    int t = 0;

    auto updcrc = [&]( const uint8 *p, aint sz )
    {
        for ( int i = 0; i < sz; ++i, ++p )
        {
            uint8 b = *p;
            crc.crc += b;
            crc.crc4[ t & 3 ] ^= ( ( b + t ) & 255 );
            ++t;
        }
    };

    updcrc( (const uint8 *)&b.info(), sizeof( imgdesc_s ) );


    aint bytes_per_line = b.info().sz.x * b.info().bytepp();
    int h = b.info().sz.y;
    for ( int y = 0; y < h; ++y )
        updcrc( b.body( ivec2( 0, y ) ), bytes_per_line );

    return crc.crc;
}

void master_internal_stuff_s::release_icon( HICON icn )
{
    for ( icon_cache_s &ic : icons )
        if ( ic.hicon == icn )
        {
            ASSERT( ic.ref > 0 );
            --ic.ref;
            if ( ic.ref == 0 ) ic.timeofzerpref = Time::current();
            break;
        }
}

HICON master_internal_stuff_s::get_icon( const bitmap_c &bmp )
{
    uint32 crc = calc_hash( bmp );

    for ( icon_cache_s &ic : icons )
    {
        if ( ic.crc == crc )
        {
            ++ic.ref;
            return ic.hicon;
        }
    }

    if (icons.size() > 31)
    {
        // do some cleanup
        Time curt = Time::current();
        aint cnt = icons.size();

        int di = -1;
        int maxd = 0;

        for ( int i = 0; i < cnt;++i)
        {
            icon_cache_s &ic = icons.get(i);
            if (ic.ref == 0)
            {
                int td = curt - ic.timeofzerpref;
                if ( td > maxd )
                {
                    di = i;
                    maxd = td;
                }
            }
        }
        if ( di >= 0 )
            icons.remove_fast( di );
    }

    icon_cache_s &icnew = icons.add();
    icnew.ref = 1;
    icnew.crc = crc;

    int w = bmp.info().sz.x;
    int h = bmp.info().sz.y;
    int bpl = ( ( w + 15 ) / 16 ) * 2;                        // bpl, 16 bit alignment
    uint8 *bits = (uint8 *)_alloca( bpl*h );

    memset( bits, 0, bpl * h );

    ICONINFO ii;
    ii.fIcon = true;
    ii.hbmMask = CreateBitmap( w, h, 1, 1, bits );
    ii.hbmColor = gen_hbitmap( bmp );
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    HICON hIcon = CreateIconIndirect( &ii );

    DeleteObject( ii.hbmColor );
    DeleteObject( ii.hbmMask );

    icnew.hicon = hIcon;

    return hIcon;

}

mouse_event_e wm2me( int msg )
{
    switch ( msg )
    {
    case WM_MOUSEMOVE: return MEVT_MOVE;
    case WM_LBUTTONDOWN: return MEVT_LDOWN;
    case WM_LBUTTONUP: return MEVT_LUP;
    case WM_MBUTTONDOWN: return MEVT_MDOWN;
    case WM_MBUTTONUP: return MEVT_MUP;
    case WM_RBUTTONDOWN: return MEVT_RDOWN;
    case WM_RBUTTONUP: return MEVT_RUP;
    //case WM_XBUTTONUP: return MEVT_MOVE;
    //case WM_XBUTTONDOWN: return MEVT_MOVE;
    //case WM_MOUSEWHEEL: return MEVT_MOVE;
    case WM_LBUTTONDBLCLK: return MEVT_L2CLICK;
    }

    return MEVT_NOPE;
}

namespace
{
    struct thread_param_s
    {
        sys_master_c::_HANDLER_T tp;
        void run()
        {
            UNSTABLE_CODE_PROLOG
                sys_master_c::_HANDLER_T tp1 = tp;
                TSDEL( this );
                tp1();
            UNSTABLE_CODE_EPILOG
        }
    };
}

static DWORD WINAPI thread_stub( void *p )
{
    ts::tmpalloc_c tmp;
    thread_param_s *tp = (thread_param_s *)p;
    tp->run();
    return 0;
}

/*virtual*/ void sys_master_win32_c::sys_start_thread( _HANDLER_T thr )
{
    thread_param_s * tp = TSNEW( thread_param_s );
    tp->tp = thr;

    CloseHandle( CreateThread( nullptr, 0, thread_stub, tp, 0, nullptr ) );
}

/*virtual*/ wstr_c sys_master_win32_c::sys_language()
{
    ts::wchar x[ 32 ];
    GetLocaleInfoW( NULL, LOCALE_SISO639LANGNAME, x, 32 );
    return ts::wstr_c( ts::wsptr( x, 2 ) );
}

namespace
{
    enum enm_reg_e
    {
        ER_CONTINUE = 0,
        ER_BREAK = 1,
        ER_DELETE_AND_CONTINUE = 2,
    };
}

template< typename ENM > void enum_reg( const ts::wsptr &regpath_, ENM e )
{
    HKEY okey = HKEY_CURRENT_USER;

    ts::wsptr regpath = regpath_;

    if ( regpath.l > 5 )
    {
        if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKCR\\" ) ) ) { okey = HKEY_CLASSES_ROOT; regpath = regpath_.skip( 5 ); }
        else if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKCU\\" ) ) ) { okey = HKEY_CURRENT_USER; regpath = regpath_.skip( 5 ); }
        else if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKLM\\" ) ) ) { okey = HKEY_LOCAL_MACHINE; regpath = regpath_.skip( 5 ); }
    }

    HKEY k;
    if ( RegOpenKeyExW( okey, ts::tmp_wstr_c( regpath ), 0, KEY_READ, &k ) != ERROR_SUCCESS ) return;

    ts::wstrings_c to_delete;

    DWORD t, len = 1024;
    ts::wchar buf[ 1024 ];
    for ( int index = 0; ERROR_SUCCESS == RegEnumValueW( k, index, buf, &len, nullptr, &t, nullptr, nullptr ); ++index, len = 1024 )
        if ( REG_SZ == t )
        {
            ts::tmp_wstr_c kn( ts::wsptr( buf, len ) );
            DWORD lt = REG_BINARY;
            DWORD sz = 1024;
            int rz = RegQueryValueExW( k, kn, 0, &lt, (LPBYTE)buf, &sz );
            if ( rz != ERROR_SUCCESS ) continue;

            ts::wsptr b( buf, sz / sizeof( ts::wchar ) - 1 );

            enm_reg_e r = e( b );
            if ( r == ER_DELETE_AND_CONTINUE )
            {
                to_delete.add( kn );
                continue;
            }
            if ( r == ER_CONTINUE ) continue;

            break;
        }


    RegCloseKey( k );

    if ( to_delete.size() )
    {
        if ( RegOpenKeyExW( okey, ts::tmp_wstr_c( regpath ), 0, KEY_READ | KEY_WRITE, &k ) != ERROR_SUCCESS ) return;

        for ( const ts::wstr_c &kns : to_delete )
            RegDeleteValueW( k, kns );

        RegCloseKey( k );
    }

}

static bool check_reg_writte_access( const ts::wsptr &regpath_ )
{
    HKEY okey = HKEY_CURRENT_USER;

    ts::wsptr regpath = regpath_;

    if ( regpath.l > 5 )
    {
        if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKCR\\" ) ) ) { okey = HKEY_CLASSES_ROOT; regpath = regpath_.skip( 5 ); }
        else if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKCU\\" ) ) ) { okey = HKEY_CURRENT_USER; regpath = regpath_.skip( 5 ); }
        else if ( ts::pwstr_c( regpath ).begins( CONSTWSTR( "HKLM\\" ) ) ) { okey = HKEY_LOCAL_MACHINE; regpath = regpath_.skip( 5 ); }
    }

    //RegGetKeySecurity() - too complex to use; also it requres Advapi32.lib
    //now way! just try open key for write

    HKEY k;
    if ( RegOpenKeyExW( okey, ts::tmp_wstr_c( regpath ), 0, KEY_WRITE, &k ) != ERROR_SUCCESS ) return false;
    RegCloseKey( k );
    return true;

}



/*virtual*/ int sys_master_win32_c::sys_detect_autostart( ts::wstr_c &cmdpar )
{
    ts::wstrings_c lks;
    ts::find_files( ts::fn_join( ts::fn_fix_path( ts::wstr_c( CONSTWSTR( "%STARTUP%" ) ), FNO_FULLPATH | FNO_PARSENENV ), CONSTWSTR( "*.lnk" ) ), lks, ATTR_ANY, ATTR_DIR, true );
    ts::find_files( ts::fn_join( ts::fn_fix_path( ts::wstr_c( CONSTWSTR( "%COMMONSTARTUP%" ) ), FNO_FULLPATH | FNO_PARSENENV ), CONSTWSTR( "*.lnk" ) ), lks, ATTR_ANY, ATTR_DIR, true );

    ts::wstr_c exepath = ts::get_exe_full_name();
    ts::fix_path( exepath, FNO_NORMALIZE );

    ts::tmp_buf_c lnk;
    for ( const ts::wstr_c &lnkf : lks )
    {
        lnk.load_from_disk_file( lnkf );
        ts::lnk_s lnkreader( lnk.data(), lnk.size() );
        if ( lnkreader.read() )
        {
            ts::fix_path( lnkreader.local_path, FNO_NORMALIZE );
            if ( exepath.equals_ignore_case( lnkreader.local_path ) )
            {
                cmdpar = lnkreader.command_line_arguments;

                if ( ts::check_write_access( ts::fn_get_path( lnkf ) ) )
                    return DETECT_AUSTOSTART;

                return DETECT_AUSTOSTART | DETECT_READONLY;
            }
        }
    }

    ts::wsptr regpaths2check[] =
    {
        CONSTWSTR( "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" ),
        CONSTWSTR( "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run" ),
        CONSTWSTR( "HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run" ),
    };

    ts::wstrings_c cmd;
    for ( const ts::wsptr & path : regpaths2check )
    {
        int detect_stuff = 0;

        enum_reg( path, [&]( const ts::wsptr& regvalue )->enm_reg_e
        {
            cmd.qsplit( regvalue );

            if ( cmd.size() )
                ts::fix_path( cmd.get( 0 ), FNO_NORMALIZE );

            if ( cmd.size() && exepath.equals_ignore_case( cmd.get( 0 ) ) )
            {
                detect_stuff = DETECT_AUSTOSTART;
                if ( !check_reg_writte_access( path ) )
                    detect_stuff |= DETECT_READONLY;

                if ( cmd.size() == 2 )
                    cmdpar = cmd.get( 1 );

                return ER_BREAK;
            }

            return ER_CONTINUE;
        } );

        if ( detect_stuff ) return detect_stuff;
    }

    return 0;

}
/*virtual*/ void sys_master_win32_c::sys_autostart( const ts::wsptr &appname, const ts::wstr_c &exepath, const ts::wsptr &cmdpar )
{
    ts::wstr_c my_exe = exepath;
    if ( my_exe.is_empty() )
        my_exe = ts::get_exe_full_name();
    ts::fix_path( my_exe, FNO_NORMALIZE );

    // 1st of all - delete lnk file

    ts::wstrings_c lks;
    ts::find_files( ts::fn_join( ts::fn_fix_path( ts::wstr_c( CONSTWSTR( "%STARTUP%" ) ), FNO_FULLPATH | FNO_PARSENENV ), CONSTWSTR( "*.lnk" ) ), lks, ATTR_ANY, ATTR_DIR, true );
    ts::find_files( ts::fn_join( ts::fn_fix_path( ts::wstr_c( CONSTWSTR( "%COMMONSTARTUP%" ) ), FNO_FULLPATH | FNO_PARSENENV ), CONSTWSTR( "*.lnk" ) ), lks, ATTR_ANY, ATTR_DIR, true );

    ts::tmp_buf_c lnk;
    for ( const ts::wstr_c &lnkf : lks )
    {
        lnk.load_from_disk_file( lnkf );
        ts::lnk_s lnkreader( lnk.data(), lnk.size() );
        if ( lnkreader.read() )
            if ( my_exe.equals_ignore_case( lnkreader.local_path ) )
                ts::kill_file( lnkf );
    }

    ts::wsptr regpaths2check[] =
    {
        CONSTWSTR( "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" ),
        CONSTWSTR( "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run" ),
        CONSTWSTR( "HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run" ),
    };

    // delete all reg records

    ts::wstrings_c cmd;
    for ( const ts::wsptr & path : regpaths2check )
    {
        enum_reg( path, [&]( const ts::wsptr& regvalue )->enm_reg_e
        {
            cmd.qsplit( regvalue );
            if ( cmd.size() )
                ts::fix_path( cmd.get( 0 ), FNO_NORMALIZE );

            if ( cmd.size() && my_exe.equals_ignore_case( cmd.get( 0 ) ) )
                return ER_DELETE_AND_CONTINUE;

            return ER_CONTINUE;
        } );
    }

    if ( !exepath.is_empty() )
    {
        HKEY k;
        if ( RegOpenKeyExW( HKEY_CURRENT_USER, ts::tmp_wstr_c( regpaths2check[ 0 ].skip( 5 ) ), 0, KEY_WRITE, &k ) != ERROR_SUCCESS ) return;

        ts::wstr_c path( CONSTWSTR( "\"" ) ); path.append( exepath );
        if ( cmdpar.l ) path.append( CONSTWSTR( "\" " ) ).append( to_wstr( cmdpar ) );
        else path.append_char( '\"' );

        RegSetValueEx( k, ts::tmp_wstr_c(appname), 0, REG_SZ, (const BYTE *)path.cstr(), (DWORD)(( path.get_length() + 1 ) * sizeof( ts::wchar )) );
        RegCloseKey( k );
    }
}


/*virtual*/ global_atom_s *sys_master_win32_c::sys_global_atom( const ts::wstr_c &n )
{
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
}

static DWORD WINAPI multiinstanceblocker( LPVOID )
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    if ( !istuff.popup_event ) return 0;
    HANDLE h = istuff.popup_event;
    for ( ;;)
    {
        WaitForSingleObject( h, INFINITE );
        if ( istuff.popup_event == nullptr ) break;
        if ( istuff.popup_notify )
            istuff.popup_notify();
    }
    CloseHandle( h );
    return 0;
}


/*virtual*/ bool sys_master_win32_c::sys_one_instance( const ts::wstr_c &n, _HANDLER_T notify_cb )
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;

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
    return true;
}

/*virtual*/ void sys_master_win32_c::shutdown()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;
    if ( istuff.popup_event )
    {
        HANDLE h = istuff.popup_event; 
        istuff.popup_event = nullptr;
        if ( h ) SetEvent( h );
    }
}

/*virtual*/ void sys_master_win32_c::sys_loop()
{
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&internal_stuff;

    istuff.lasttick = timeGetTime();

    if ( !istuff.looper && !istuff.looper_staring )
    {
        istuff.looper_staring = true;
        CloseHandle( CreateThread( nullptr, 0, looper, nullptr, 0, nullptr ) );
    }

    MSG msg;
    while ( !master().is_app_need_quit && GetMessageW( &msg, nullptr, 0U, 0U ) )
    {
        if ( msg.message == WM_LOOPER_TICK )
        {
            --istuff.cnttick;
            if ( istuff.cnttick > 0)
            {
                MSG msgm;
                while ( PeekMessage( &msgm, nullptr, WM_LOOPER_TICK, WM_LOOPER_TICK, PM_REMOVE ) )
                    --istuff.cnttick;
            }

            if ( master().on_loop )
                master().on_loop();

            istuff.lasttick = timeGetTime();
            istuff.looper_allow_tick = true;
            continue;
        }

        bool downkey = false;
        bool dispatch = true;

        //if ( istuff.looper_hwnd == nullptr )
        //{
        //    WNDCLASSEXW wcex;

        //    wcex.cbSize = sizeof( wcex );
        //    wcex.style = 0;
        //    wcex.lpfnWndProc = (WNDPROC)looper_proc;
        //    wcex.cbClsExtra = 0;
        //    wcex.cbWndExtra = 0;
        //    wcex.hInstance = istuff.inst;
        //    wcex.hIcon = nullptr;
        //    wcex.hCursor = nullptr;
        //    wcex.hbrBackground = nullptr;
        //    wcex.lpszMenuName = nullptr;
        //    wcex.lpszClassName = L"aoi202309fasdo234";
        //    wcex.hIconSm = nullptr;

        //    RegisterClassExW( &wcex );

        //    istuff.looper_hwnd = CreateWindowW( L"aoi202309fasdo234", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, istuff.inst, nullptr );
        //}

        int xmsg = msg.message;
        switch ( xmsg )
        {
            //case WM_NCMOUSEMOVE: xmsg = WM_MOUSEMOVE; goto do_key;
            //case WM_NCLBUTTONDOWN: xmsg = WM_LBUTTONDOWN; goto do_key;
            //case WM_NCLBUTTONUP: xmsg = WM_LBUTTONUP; goto do_key;
        case WM_NCMBUTTONDOWN: xmsg = WM_MBUTTONDOWN; goto do_key;
        case WM_NCMBUTTONUP: xmsg = WM_MBUTTONUP; goto do_key;
            //case WM_NCRBUTTONDOWN: xmsg = WM_RBUTTONDOWN; goto do_key;
            //case WM_NCRBUTTONUP: xmsg = WM_RBUTTONUP; goto do_key;
        case WM_NCXBUTTONUP: xmsg = WM_XBUTTONUP; goto do_key;
        case WM_NCXBUTTONDOWN: xmsg = WM_XBUTTONDOWN;

            // mouse callbacker implementation
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_XBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        {
        do_key:

            if (master().on_mouse)
            {
                ts::ivec2 p;
                p.x = GET_X_LPARAM( msg.lParam );
                p.y = GET_Y_LPARAM( msg.lParam );
                ts::ivec2 scrp(p);
                ClientToScreen( msg.hwnd, &ts::ref_cast<POINT>( scrp ) );

                master().on_mouse( wm2me( xmsg ), p, scrp );
            }

            break;
        }
        case WM_MOUSEWHEEL:
            if ( master().on_mouse )
            {
                ts::ivec2 scrp;
                scrp.x = GET_X_LPARAM( msg.lParam );
                scrp.y = GET_Y_LPARAM( msg.lParam );
                ts::ivec2 p( scrp );
                ScreenToClient( msg.hwnd, &ts::ref_cast<POINT>( p ) );


                int rot = GET_WHEEL_DELTA_WPARAM( msg.wParam );

                int rot2 = ts::lround( (float)rot / (float)WHEEL_DELTA );
                if ( rot2 == 0 ) rot2 = ts::isign( rot );
                if ( rot2 > 10 ) rot2 = 10;
                if ( rot2 < -10 ) rot2 = -10;

                for ( ; rot2 > 0; --rot2 )
                    master().on_mouse( MEVT_WHEELUP, p, scrp );

                for ( ; rot2 < 0; ++rot2 )
                    master().on_mouse( MEVT_WHEELDN, p, scrp );

            }
            break;

        case WM_SYSCHAR:
        case WM_CHAR:
            if ( ( msg.lParam&( 1 << 29 ) ) == 0 && GetKeyState( VK_CONTROL ) < 0 ); else
            {
                // wow! unicode! ... hm... WM_WCHAR?

                if ( master().on_char )
                    if (master().on_char( (wchar_t)msg.wParam ))
                        dispatch = false;
            }
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            downkey = true;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            //++istuff.kbdh;
            TranslateMessage( &msg );
            //--istuff.kbdh;
            if ( master().on_keyboard )
            {
                if ( msg.wParam == VK_SNAPSHOT )
                {
                    // very special button - only release event can be handled

                    int scan = lp2key( msg.lParam );
                    if (master().on_keyboard( scan, true, 0 ))
                        dispatch = false;
                    if (master().on_keyboard( scan, false, 0 ))
                        dispatch = false;

                }
                else if ( msg.wParam == VK_MENU )
                {
                    //dispatch = false;
                    goto kbddo;
                }
                else if ( downkey && msg.wParam == VK_F4 && ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) == 0x8000 )
                {
                    sys_exit(0);
                    dispatch = false;

#if 0
                }
                else if ( ( msg.wParam == 0x44 || msg.wParam == 0x4D ) && ( ( GetAsyncKeyState( VK_LWIN ) & 0x8000 ) == 0x8000 || ( GetAsyncKeyState( VK_RWIN ) & 0x8000 ) == 0x8000 ) )
                {

                    ShowWindow( g_sysconf.mainwindow, SW_MINIMIZE );

                    IShellDispatch4 *pShell;

                    HRESULT hr = CoCreateInstance( CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IShellDispatch, (void**)&pShell );

                    if ( SUCCEEDED( hr ) )
                    {
                        pShell->ToggleDesktop();
                        pShell->Release();
                    }

                    //CoUninitialize();
                    dispatch = false;
#endif

                }
                else
                {
                kbddo:

                    int casw = 0;
                    if (downkey)
                    {
                        if ( GetKeyState( VK_CONTROL ) < 0 ) casw |= casw_ctrl;
                        if ( GetKeyState( VK_MENU ) < 0 ) casw |= casw_alt;
                        if ( GetKeyState( VK_SHIFT ) < 0 ) casw |= casw_shift;
                        if ( GetKeyState( VK_LWIN ) < 0 || GetKeyState( VK_RWIN ) < 0 ) casw |= casw_win;
                    }

                    if ( master().on_keyboard( lp2key( msg.lParam ), downkey, casw ) )
                        dispatch = false;
                }
                break;
            }

        }

        if ( dispatch )
        {
            //++istuff.kbdh;
            DispatchMessageW( &msg );
            //--istuff.kbdh;
        }

    }

    //if ( istuff.looper_hwnd )
    //{
    //    DestroyWindow( istuff.looper_hwnd );
    //    istuff.looper_hwnd = nullptr;
    //}

}

/*virtual*/ void sys_master_win32_c::sys_idle()
{
    MSG m;
    if ( PeekMessageW( &m, nullptr, 0U, 0U, PM_REMOVE ) )
    {
        TranslateMessage( &m );
        DispatchMessageW( &m );
    }

    if ( master().on_loop )
        master().on_loop();

}
/*virtual*/ void sys_master_win32_c::sys_exit( int iErrCode )
{
    if ( mainwindow )
        mainwindow->try_close();
    else
    {
        is_app_need_quit = true;
        PostQuitMessage( iErrCode );
    }
}

/*virtual*/ void sys_master_win32_c::sys_sleep( int ms )
{
    Sleep( ms );
}

/*virtual*/ wnd_c *sys_master_win32_c::get_focus()
{
    HWND fhwnd = GetFocus();
    if ( !fhwnd ) return nullptr;
    return (wnd_c *)hwnd2wnd( fhwnd );

}

/*virtual*/ wnd_c *sys_master_win32_c::get_capture()
{
    HWND chwnd = GetCapture();
    if ( !chwnd ) return nullptr;
    return (wnd_c *)hwnd2wnd(chwnd);
}

/*virtual*/ void sys_master_win32_c::release_capture()
{
    ReleaseCapture();
}

/*virtual*/ void sys_master_win32_c::set_notification_icon_text( const ts::wsptr& text )
{
    if ( !mainwindow ) return;

    static NOTIFYICONDATAW nd = { sizeof( NOTIFYICONDATAW ), 0 };
    nd.hWnd = wnd2hwnd( mainwindow );
    nd.uID = ( int )(size_t)mainwindow.get(); //-V205
    nd.uCallbackMessage = WM_USER + 7213;
    //nd.hIcon = gui->app_icon(true);

    size_t copylen = ts::tmin( sizeof( nd.szTip ) - sizeof( ts::wchar ), text.l * sizeof( ts::wchar ) );
    memcpy( nd.szTip, text.s, copylen );
    nd.szTip[ copylen / sizeof( ts::wchar ) ] = 0;

    PostMessageW( nd.hWnd, WM_USER + 7214, 0, (LPARAM)&nd );
}

namespace
{
    struct phi_s
    {
        HANDLE processhandle;
    };
    TS_STATIC_CHECK( sizeof( phi_s ) <= sizeof( process_handle_s ), "bad data size" );

}

process_handle_s::~process_handle_s()
{
    if ( ( ( phi_s* )this )->processhandle )
        CloseHandle( ( (phi_s*)this )->processhandle );
}

/*virtual*/ bool sys_master_win32_c::start_app( const wstr_c &exe, const wstr_c& prms, process_handle_s *process_handle, bool elevate )
{
    if ( process_handle )
    {
        if ( ( (phi_s*)process_handle )->processhandle )
        {
            CloseHandle( ( (phi_s*)process_handle )->processhandle );
            ( (phi_s*)process_handle )->processhandle = nullptr;
        }
    }

    if (!elevate)
    {
        STARTUPINFOW si = { sizeof( si ) };
        PROCESS_INFORMATION pi = { 0 };
        wstr_c cmd;
        tmp_wstr_c prm( exe );

        bool batch = exe.ends_ignore_case( CONSTWSTR( ".cmd" ) ) || exe.ends_ignore_case( CONSTWSTR( ".bat" ) );

        if ( !batch && !prm.ends_ignore_case( CONSTWSTR( ".exe" ) ) )
            prm.append( CONSTWSTR( ".exe" ) );

        if ( prm.find_pos( ' ' ) >= 0 )
            prm.insert( 0, '\"' ).append_char( '\"' );

        if ( batch )
        {
            cmd = CONSTWSTR( "%SYSTEM%\\cmd.exe" );
            parse_env( cmd );
            prm.insert( 0, CONSTWSTR( "/c " ) );
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        prm.append_char( ' ' ).append( prms );
        wstr_c path = fn_get_path( fn_fix_path ( exe, FNO_NORMALIZE | FNO_FULLPATH | FNO_REMOVECRAP ) );
        if ( CreateProcessW( cmd.is_empty() ? nullptr : cmd.cstr(), prm.str(), nullptr, nullptr, FALSE, 0, nullptr, path, &si, &pi ) )
        {
            if ( process_handle )
                ( (phi_s*)process_handle )->processhandle = pi.hProcess;
            return true;

        } else
        {
            return false;
        }
    } else
    {
        wstr_c path = fn_get_path( fn_fix_path ( exe, FNO_NORMALIZE | FNO_FULLPATH | FNO_REMOVECRAP ) );

        SHELLEXECUTEINFOW shExInfo = { 0 };
        shExInfo.cbSize = sizeof( shExInfo );
        shExInfo.fMask = process_handle ? SEE_MASK_NOCLOSEPROCESS : 0;
        shExInfo.hwnd = 0;
        shExInfo.lpVerb = L"runas";
        shExInfo.lpFile = exe;
        shExInfo.lpParameters = prms;
        shExInfo.lpDirectory = path;
        shExInfo.nShow = SW_NORMAL;
        shExInfo.hInstApp = 0;

        if ( ShellExecuteExW( &shExInfo ) )
        {
            if ( process_handle )
                ( (phi_s*)process_handle )->processhandle = shExInfo.hProcess;

            return true;
        }
        return false;
    }
}

/*virtual*/ bool sys_master_win32_c::wait_process( process_handle_s &phandle, int timeoutms )
{
    if ( ( (phi_s*)&phandle )->processhandle )
    {
        DWORD r = WaitForSingleObject( ( (phi_s*)&phandle )->processhandle, timeoutms > 0 ? timeoutms : INFINITE );
        if ( r == WAIT_TIMEOUT ) return false;
    }
    return true;
}

/*virtual*/ bool sys_master_win32_c::open_process( uint32 processid, process_handle_s &phandle )
{
    HANDLE h = OpenProcess( SYNCHRONIZE, FALSE, processid );
    if ( h )
    {
        if ( ( (phi_s*)&phandle )->processhandle )
            CloseHandle( ( (phi_s*)&phandle )->processhandle );

        ( (phi_s*)&phandle )->processhandle = h;
        return true;
    }

    return false;
}

/*virtual*/ uint32 sys_master_win32_c::process_id()
{
    return GetCurrentProcessId();
}

/*virtual*/ void sys_master_win32_c::sys_beep( sys_beep_e beep )
{
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

}

/*virtual*/ void sys_master_win32_c::hide_hardware_cursor()
{
    while ( ShowCursor( false ) >= 0 )
    {
    }

}
/*virtual*/ void sys_master_win32_c::show_hardware_cursor()
{
    while ( ShowCursor( true ) < 0 )
    {
    }
}
/*virtual*/ void sys_master_win32_c::set_cursor( cursor_e ct )
{
    static const ts::wchar *cursors[] = {
        IDC_ARROW,
        IDC_SIZEALL,
        IDC_SIZEWE,
        IDC_SIZENS,
        IDC_SIZENWSE,
        IDC_SIZENESW,
        IDC_IBEAM,
        IDC_HAND,
        IDC_CROSS,
    };

    if ( ct < 0 || ct >= CURSOR_LAST )
        ct = CURSOR_ARROW;
        
    SetCursor( LoadCursorW( nullptr, cursors[ct] ) );
}

/*virtual*/ ivec2 sys_master_win32_c::get_cursor_pos()
{
    ts::ivec2 cp;
    GetCursorPos( &ts::ref_cast<POINT>( cp ) );
    return cp;
}

/*virtual*/ int sys_master_win32_c::get_system_info( sysinf_e si )
{
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

    return 0;
}

/*virtual*/ bool sys_master_win32_c::is_key_pressed( key_scan_e k )
{
    int vk = -1;
    switch ( k )
    {
    case SSK_LCTRL:
        vk = VK_LCONTROL;
        break;
    case SSK_RCTRL:
        vk = VK_RCONTROL;
        break;
    case SSK_CTRL:
        vk = VK_CONTROL;
        break;
    case SSK_SHIFT:
        vk = VK_SHIFT;
        break;
    case SSK_LSHIFT:
        vk = VK_LSHIFT;
        break;
    case SSK_RSHIFT:
        vk = VK_RSHIFT;
        break;
    case SSK_ALT:
        vk = VK_MENU;
        break;
    case SSK_LALT:
        vk = VK_LMENU;
        break;
    case SSK_RALT:
        vk = VK_RMENU;
        break;
    }

    if (vk >= 0)
        return ( GetAsyncKeyState( vk ) & 0x8000 ) == 0x8000;

    return false;
}