#include "toolset.h"
#include "internal/platform.h"

#ifdef _NIX
#include "_nix/nix_common.inl"
#endif // _NIX

namespace ts
{

void TSCALL sys_sleep( int ms )
{
#ifdef _WIN32
    Sleep( ms );
#endif
#ifdef _NIX
    usleep( ms * 1000 );
#endif
}



wstr_c  TSCALL monitor_get_description(int monitor)
{
#ifdef _WIN32
    struct mdata
    {
        int mi, mio;
        ts::wstr_c desc;
        static BOOL CALLBACK calcmc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
        {
            mdata *m = (mdata *)dwData;
            if (m->mi == 0)
            {
                MONITORINFOEXW minf;
                minf.cbSize = sizeof(MONITORINFOEXW);
                GetMonitorInfo(hMonitor, &minf);

                DEVMODEW dm; dm.dmSize = sizeof(DEVMODEW);
                EnumDisplaySettingsW(minf.szDevice, ENUM_CURRENT_SETTINGS, &dm);

                DISPLAY_DEVICEW dd; dd.cb = sizeof(DISPLAY_DEVICEW);
                m->desc.set_as_char('#').append_as_int( m->mio + 1 ).append(CONSTWSTR(" (")).append_as_int(dm.dmPelsWidth).append_char('x').append_as_int(dm.dmPelsHeight).append(CONSTWSTR(") "));
                for (int i = 0; 0 != EnumDisplayDevicesW(nullptr, i, &dd, EDD_GET_DEVICE_INTERFACE_NAME); ++i)
                {
                    if (CHARz_equal(minf.szDevice, dd.DeviceName))
                    {
                        m->desc.append(wsptr(dd.DeviceString));
                        m->desc.trim();
                        break;
                    }
                }


                return FALSE;
            }
            --m->mi;
            return TRUE;
        }
    } mm; mm.mi = monitor; mm.mio = monitor;

    EnumDisplayMonitors(nullptr, nullptr, mdata::calcmc, (LPARAM)&mm);
    return mm.desc;
#endif
#ifdef _NIX
    STOPSTUB("");
    //Display *X11 = XOpenDisplay(nullptr);
#endif
}

int TSCALL monitor_find_max_sz(const irect&fr, irect&maxr)
{
    int mc = monitor_count();
    int maxia = -1;
    int fm = -1;

    ts::ivec2 fc = fr.center();
    int dst = maximum<int>::value;
    int fmdst = -1;

    for (int i = 0; i < mc; ++i)
    {
        irect rr = monitor_get_max_size(i);
        int ia = fr.intersect_area(rr);
        if (ia > maxia)
        {
            fm = i;
            maxia = ia;
            maxr = rr;
        }
        if (fm < 0)
        {
            int d = (fc - rr.center()).sqlen();
            if (d < dst)
            {
                dst = d;
                fmdst = i;
                maxr = rr;
            }
        }
    }
    return fm < 0 ? fmdst : fm;
}


int     TSCALL monitor_find(const ivec2& pt)
{
#ifdef _WIN32
    struct mm
    {
        ts::ivec2 pt;
        int i = -1;
        int found = -1;

        static BOOL CALLBACK findmc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
        {
            mm * m = (mm *)dwData;
            ++m->i;
            if (ref_cast<irect>(*lprcMonitor).inside(m->pt))
            {
                m->found = m->i;
                return FALSE;
            }
            return TRUE;
        }
    } m; m.pt = pt;

    EnumDisplayMonitors(nullptr, nullptr, mm::findmc, (LPARAM)&m);
    return m.found;
#endif
#ifdef _NIX
    int c = monitor_count();
    for (int i = 0; i < c; ++i)
        if (monitor_get_max_size_fs(i).inside(pt))
            return i;
    return -1;
#endif
}

int     TSCALL monitor_count()
{
#ifdef _WIN32
    struct m
    {
        static BOOL CALLBACK calcmc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
        {
            int * cnt = (int *)dwData;
            *cnt = *cnt + 1;
            return TRUE;
        }
    };

    int cnt = 0;
    EnumDisplayMonitors(nullptr, nullptr, m::calcmc, (LPARAM)&cnt);
    return cnt;
#endif
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    return istuff.monitor_count();
#endif
}

irect   TSCALL monitor_get_max_size_fs(int monitor)
{
#ifdef _WIN32
    struct m
    {
        irect rr;
        int mi;
        static BOOL CALLBACK calcmrect_fs(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
        {
            m * mm = (m *)dwData;
            if (mm->mi == 0)
            {
                mm->rr = ref_cast<irect>(*lprcMonitor);
                return FALSE;
            }
            --mm->mi;
            return TRUE;
        }
    } mm; mm.mi = monitor;
    EnumDisplayMonitors(nullptr, nullptr, m::calcmrect_fs, (LPARAM)&mm);
    return mm.rr;
#endif
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    return istuff.monitor_fs(monitor);

#endif
}

irect fix_by_taskbar(irect r)
{
#ifdef _WIN32
    if ( HWND taskbar = FindWindowW( L"Shell_TrayWnd", nullptr ) )
    {
        ts::irect tbr;
        GetWindowRect( taskbar, &ref_cast<RECT>( tbr ) );
        tbr.intersect( r );
        if ( tbr && (tbr.width() <= 4 || tbr.height() <= 4) )
        {
            if ( r.width() != tbr.width() )
            {
                if ( r.rb.x > tbr.lt.x ) r.rb.x = tbr.lt.x;
                else if ( r.lt.x < tbr.rb.x ) r.lt.x = tbr.rb.x;

            }
            else if ( r.height() != tbr.height() )
            {
                if ( r.rb.y > tbr.lt.y ) r.rb.y = tbr.lt.y;
                else if ( r.lt.y < tbr.rb.y ) r.lt.y = tbr.rb.y;
            }
        }
    }
#endif
    return r;
}

irect    TSCALL monitor_get_max_size(int monitor)
{
#ifdef _WIN32
    struct m
    {
        irect rr;
        int mi;
        static BOOL CALLBACK calcmrect( HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData )
        {
            m * mm = (m *)dwData;
            if ( mm->mi == 0 )
            {
                MONITORINFO mi;
                mi.cbSize = sizeof( MONITORINFO );
                GetMonitorInfo( hMonitor, &mi );
                mm->rr = ref_cast<irect>( mi.rcWork );

                return FALSE;
            }
            --mm->mi;
            return TRUE;
        }
    } mm; mm.mi = monitor;
    EnumDisplayMonitors( nullptr, nullptr, m::calcmrect, (LPARAM)&mm );
    return fix_by_taskbar(mm.rr);
#endif
#ifdef _NIX
#endif
}

ivec2 TSCALL get_cursor_pos()
{
#ifdef _WIN32
    ts::ivec2 cp;
    GetCursorPos( &ts::ref_cast<POINT>( cp ) );
    return cp;
#endif // _WIN32
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
#endif
}


ivec2   TSCALL wnd_get_center_pos( const ts::ivec2& size )
{
    irect r = wnd_get_max_size( get_cursor_pos() );
    return r.center() - size/2;
}

void    TSCALL wnd_fix_rect(irect &r, int minw, int minh)
{
    int ww = r.width();
    int hh = r.height();
    if (ww < minw) ww = minw;
    if (hh < minh) hh = minh;

    int mc = monitor_count();

    ivec2 sdvig(65535, 65535);

    ts::irect rmin;
    rmin.lt.x = 1000000;
    rmin.rb.x = -1000000;
    rmin.lt.y = 1000000;
    rmin.rb.y = -1000000;

    for (int i = 0; i<mc; ++i)
    {
        irect mr = monitor_get_max_size_fs(i);
        if (mr.lt.x < rmin.lt.x) rmin.lt.x = mr.lt.x;
        if (mr.rb.x > rmin.rb.x ) rmin.rb.x = mr.rb.x;
        if (mr.lt.y < rmin.lt.y ) rmin.lt.y = mr.lt.y;
        if (mr.rb.y > rmin.rb.y ) rmin.rb.y = mr.rb.y;
    }

    int maxw = rmin.width();
    int maxh = rmin.height();
    if (ww > maxw) ww = maxw;
    if (hh > maxh) hh = maxh;

    for (int i = 0; i < mc; ++i)
    {
        irect mr = monitor_get_max_size(i);
        ivec2 csdvig(0, 0);
        if (r.lt.y < mr.lt.y) csdvig.y += (mr.lt.y - r.lt.y);
        if ((r.lt.y + 20) > mr.rb.y) csdvig.y -= (r.lt.y + 20) - mr.rb.y;

        if ((r.lt.x + ww) < (mr.lt.x + 100)) csdvig.x += ((mr.lt.x + 100) - (r.lt.x + ww));
        if ((r.lt.x) > (mr.rb.x - 50)) csdvig.x -= (r.lt.x - (mr.rb.x - 50));

        uint a = csdvig.sqlen();
        uint b = sdvig.sqlen();

        if (a < b)
        {
            sdvig = csdvig;
        }
    }

    r.lt.x += sdvig.x;
    r.lt.y += sdvig.y;
    r.rb.x = r.lt.x + ww;
    r.rb.y = r.lt.y + hh;

}

irect    TSCALL wnd_get_max_size_fs(const ts::ivec2 &pt)
{
#ifdef _WIN32
    MONITORINFO mi;
    if (HMONITOR m = MonitorFromPoint(ts::ref_cast<POINT>(pt), MONITOR_DEFAULTTONEAREST))
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else
    {
        //SystemParametersInfo(SPI_GETWORKAREA,0,&mi.rcMonitor,0);
        mi.rcMonitor.left = 0;
        mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    return ref_cast<irect>(mi.rcMonitor);
#endif // _WIN32
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    DEBUG_BREAK();
#endif
}


irect    TSCALL wnd_get_max_size(const ts::ivec2& pt)
{
#ifdef _WIN32
    MONITORINFO mi;
    if (HMONITOR m = MonitorFromPoint(ts::ref_cast<POINT>(pt), MONITOR_DEFAULTTONEAREST))
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return fix_by_taskbar(ref_cast<irect>(mi.rcWork));
#endif // _WIN32
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    return istuff.max_window_size(pt);
#endif
}


irect    TSCALL wnd_get_max_size(const irect &rfrom)
{
#ifdef _WIN32
    MONITORINFO mi;
    HMONITOR m = MonitorFromRect(&ref_cast<RECT>(rfrom), MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return fix_by_taskbar(ref_cast<irect>(mi.rcWork));
#endif // _WIN32
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    return istuff.max_window_size(rfrom);
#endif
}

irect   TSCALL wnd_get_max_size_fs(const irect &rfrom)
{
#ifdef _WIN32
    MONITORINFO mi;
    HMONITOR m = MonitorFromRect(&ref_cast<RECT>(rfrom), MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else
    {
        //SystemParametersInfo(SPI_GETWORKAREA,0,&mi.rcMonitor,0);
        mi.rcMonitor.left = 0;
        mi.rcMonitor.top = 0;
        mi.rcMonitor.right = GetSystemMetrics(SM_CXSCREEN);
        mi.rcMonitor.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    return ref_cast<irect>(mi.rcMonitor);
#endif // _WIN32
#ifdef _NIX
    master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
    DEBUG_BREAK();
#endif
}

void TSCALL set_clipboard_text(const wsptr &t)
{
#ifdef _WIN32
    OpenClipboard(nullptr);
    EmptyClipboard();

    aint len = t.l + 1;

    HANDLE text = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar));
    void *d = GlobalLock(text);
    memcpy(d, t.s, t.l * sizeof(wchar));
    *(((wchar *)d)+t.l) = 0;
    GlobalUnlock(text);

    SetClipboardData(CF_UNICODETEXT, text);
    CloseClipboard();
#endif
#ifdef _NIX
    STOPSTUB("set_clipboard_text");
    DEBUG_BREAK();
#endif
}

wstr_c TSCALL get_clipboard_text()
{
    wstr_c res;

#ifdef _WIN32
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(nullptr))
    {
        HGLOBAL hg = GetClipboardData(CF_UNICODETEXT);
        if (hg)
        {
            const wchar *p = (const wchar*)GlobalLock(hg);
            res.set(sptr<wchar>(p));
            res.replace_all(wsptr(L"\r\n"), wsptr(L"\n"));
            GlobalUnlock(hg);
        }
        CloseClipboard();
    }
#endif // _WIN32
#ifdef _NIX
    UNFINISHED( "get_clipboard_text" );
    DEBUG_BREAK();
#endif

    return res;
}

void TSCALL set_clipboard_bitmap(const bitmap_c &bmp)
{
#if 0
    OpenClipboard(nullptr);
    EmptyClipboard();

    int len = 0;

    HANDLE text = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar));
    /*void *d = */ GlobalLock(text);


    GlobalUnlock(text);

    SetClipboardData(CF_TIFF, text);
    CloseClipboard();
#endif
}

bitmap_c TSCALL get_clipboard_bitmap()
{
    bitmap_c res;

#ifdef _WIN32
    OpenClipboard(nullptr);

    if (IsClipboardFormatAvailable(CF_DIBV5))
    {
        if (HGLOBAL hg = GetClipboardData(CF_DIBV5))
        {
            uint sz = sz_cast<uint>(GlobalSize(hg));
            const BITMAPV5HEADER *p = (const BITMAPV5HEADER*)GlobalLock(hg);
            res.load_from_BMPHEADER((const BITMAPINFOHEADER *)p,sz);
            GlobalUnlock(hg);
        }
    } else if (IsClipboardFormatAvailable(CF_DIB))
    {
        if (HGLOBAL hg = GetClipboardData(CF_DIB))
        {
            uint sz = sz_cast<uint>(GlobalSize(hg));
            const BITMAPINFOHEADER *p = (const BITMAPINFOHEADER*)GlobalLock(hg);
            res.load_from_BMPHEADER((const BITMAPINFOHEADER *)p, sz);
            GlobalUnlock(hg);
        }
    }

    CloseClipboard();
#endif // _WIN32
#ifdef _NIX
    UNFINISHED( "get_clipboard_bitmap" );
    DEBUG_BREAK();
#endif

    return res;
}

void TSCALL open_link(const ts::wstr_c &lnk0)
{
    ts::wstr_c lnk( lnk0 );
    if (lnk.find_pos(CONSTWSTR("://")) < 0 && lnk.find_pos('@') >= 0)
        lnk.insert( 0, CONSTWSTR( "mailto:" ) );

#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", lnk, nullptr, nullptr, SW_SHOWNORMAL);
#endif // _WIN32
#ifdef _NIX
    DEBUG_BREAK();
#endif
}

#ifdef _WIN32
#include "_win32/win32_common.inl"
#endif // _WIN32

void TSCALL explore_path( const wsptr &path, bool path_only )
{
#ifdef _WIN32

    if ( path_only )
        ShellExecuteW(wnd2hwnd(master().mainwindow), L"explore", ts::tmp_wstr_c(path), nullptr, nullptr, SW_SHOWDEFAULT);
    else
        ShellExecuteW(wnd2hwnd(master().mainwindow), L"open", L"explorer", CONSTWSTR("/select,") + ts::fn_autoquote(ts::fn_get_name_with_ext(path)), ts::fn_get_path(ts::wstr_c(path)).cstr(), SW_SHOWDEFAULT);
#endif // _WIN32
#ifdef _NIX
    DEBUG_BREAK();
#endif
}


bool TSCALL is_admin_mode()
{
#ifdef _WIN32
    // http://www.codeproject.com/Articles/320748/Haephrati-Elevating-during-runtime

    BOOL fIsRunAsAdmin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    PSID pAdministratorsGroup = NULL;

    // Allocate and initialize a SID of the administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pAdministratorsGroup))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

    // Determine whether the SID of administrators group is enabled in
    // the primary access token of the process.
    if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
    {
        dwError = GetLastError();
        goto Cleanup;
    }

Cleanup:
    // Centralized cleanup for all allocated resources.
    if (pAdministratorsGroup)
    {
        FreeSid(pAdministratorsGroup);
        pAdministratorsGroup = NULL;
    }

    // Throw the error if something failed in the function.
    if (ERROR_SUCCESS != dwError)
    {
        return false;
    }
    return fIsRunAsAdmin != FALSE;
#endif // _WIN32
#ifdef _NIX
    DEBUG_BREAK();
#endif

}

str_c gen_machine_unique_string()
{
    str_c rs;
#ifdef _WIN32

    HKEY k;
    if ( RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ| KEY_WOW64_64KEY, &k ) == ERROR_SUCCESS )
    {
        ts::wchar buf[ 1024 ];
        DWORD sz = sizeof( buf );

        int rz = RegQueryValueExW( k, L"MachineGuid", nullptr, nullptr, (LPBYTE)buf, &sz );
        if ( rz == ERROR_SUCCESS )
        {
            ts::wsptr b( buf, sz / sizeof( ts::wchar ) - 1 );
            rs.set( to_str(b) );
        }
        RegCloseKey( k );
    }


    ts::uint32 volumesn = 0;
    GetVolumeInformationA( "c:\\", nullptr, 0, &volumesn, nullptr, nullptr, nullptr, 0 );

    rs.append_char( '-' ).append_as_uint( volumesn );

#endif

#ifdef _NIX
    DEBUG_BREAK();
#endif


    return rs;
}

}