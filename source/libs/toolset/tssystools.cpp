#include "toolset.h"
#include "internal/platform.h"

namespace ts
{


wstr_c  TSCALL monitor_get_description(int monitor)
{
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
}

int     TSCALL monitor_count()
{
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
}

irect   TSCALL monitor_get_max_size_fs(int monitor)
{
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
}

irect    TSCALL monitor_get_max_size(int monitor)
{
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
    return mm.rr;
}

ivec2   TSCALL wnd_get_center_pos( const ts::ivec2& size )
{
    ivec2 cp;
    GetCursorPos(&ref_cast<POINT>(cp));
    irect r = wnd_get_max_size(cp);
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

    RECT min;
    min.left = 1000000;
    min.right = -1000000;
    min.top = 1000000;
    min.bottom = -1000000;
    
    for (int i = 0; i<mc; ++i)
    {
        irect mr = monitor_get_max_size_fs(i);
        if (mr.lt.x < min.left) min.left = mr.lt.x;
        if (mr.rb.x > min.right) min.right = mr.rb.x;
        if (mr.lt.y < min.top) min.top = mr.lt.y;
        if (mr.rb.y > min.bottom) min.bottom = mr.rb.y;
    }

    int maxw = min.right - min.left;
    int maxh = min.bottom - min.top;
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
}


irect    TSCALL wnd_get_max_size(const ts::ivec2& pt)
{
    MONITORINFO mi;
    if (HMONITOR m = MonitorFromPoint(ts::ref_cast<POINT>(pt), MONITOR_DEFAULTTONEAREST))
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return ref_cast<irect>(mi.rcWork);
}


irect    TSCALL wnd_get_max_size(const irect &rfrom)
{
    MONITORINFO mi;
    HMONITOR m = MonitorFromRect(&ref_cast<RECT>(rfrom), MONITOR_DEFAULTTONEAREST);
    if (m)
    {
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(m, &mi);
    }
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &mi.rcWork, 0);

    return ref_cast<irect>(mi.rcWork);
}

irect   TSCALL wnd_get_max_size_fs(const irect &rfrom)
{
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
#if __linux__
    UNFINISHED("set_clipboard_text");
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
#if __linux__
    UNFINISHED( "get_clipboard_text" );
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
#if __linux__
    UNFINISHED( "get_clipboard_bitmap" );
#endif

    return res;
}

void TSCALL open_link(const ts::wstr_c &lnk)
{
    ShellExecuteW(nullptr, L"open", lnk, nullptr, nullptr, SW_SHOWNORMAL);
}

void TSCALL explore_path( const wsptr &path, bool path_only )
{
    if ( path_only )
        ShellExecuteW( nullptr, L"explore", ts::tmp_wstr_c( path ), nullptr, nullptr, SW_SHOWDEFAULT );
    else
        ShellExecuteW( nullptr, L"open", L"explorer", CONSTWSTR( "/select," ) + ts::fn_autoquote( ts::fn_get_name_with_ext( path ) ), ts::fn_get_path( path ), SW_SHOWDEFAULT );
}


bool TSCALL is_admin_mode()
{
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
}

str_c gen_machine_unique_string()
{
    str_c rs;
#ifdef _WIN32

    HKEY k;
    if ( RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &k ) == ERROR_SUCCESS )
    {
        DWORD lt = REG_SZ;
        DWORD sz = 1024;
        ts::wchar buf[ 1024 ];
        int rz = RegQueryValueExW( k, L"MachineGuid", 0, &lt, (LPBYTE)buf, &sz );
        if ( rz == ERROR_SUCCESS )
        {
            ts::wsptr b( buf, sz / sizeof( ts::wchar ) - 1 );
            rs.set( to_str(b) );
        }
    }

    RegCloseKey( k );

    ts::uint32 volumesn = 0;
    GetVolumeInformationA( "c:\\", nullptr, 0, &volumesn, nullptr, nullptr, nullptr, 0 );

    rs.append_char( '-' ).append_as_uint( volumesn );

#endif

    return rs;
}

}