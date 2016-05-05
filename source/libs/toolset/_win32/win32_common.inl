#pragma once

struct master_internal_stuff_s
{
    struct icon_cache_s
    {
        MOVABLE( true );
        HICON hicon = nullptr;
        uint32 crc = 0;
        int ref = 0;
        Time timeofzerpref = Time::undefined();
        icon_cache_s() {}
        ~icon_cache_s()
        {
            ASSERT( ref == 0 );
            DestroyIcon( hicon );
        }
        icon_cache_s( icon_cache_s&&ic )
        {
            hicon = ic.hicon;
            crc = ic.crc;
            ic.hicon = nullptr;
        }
        icon_cache_s &operator = ( icon_cache_s&& ic )
        {
            SWAP( hicon, ic.hicon );
            SWAP( crc, ic.crc );
        }

    private:
        icon_cache_s( const icon_cache_s& ) UNUSED;
        icon_cache_s &operator = ( const icon_cache_s& ) UNUSED;
    };

    array_inplace_t< icon_cache_s, 4 > icons;
    HICON get_icon( const bitmap_c &bmp );
    void release_icon( HICON icn );

    sys_master_c::_HANDLER_T popup_notify = nullptr;
    volatile HANDLE popup_event = nullptr;
    HWND looper_hwnd = nullptr;
    HINSTANCE inst = nullptr;
    DWORD lasttick = 0;
    int regclassref = 0;
    int sysmodal = 0;
    int cnttick = 0;
    bool looper = false;
    bool looper_staring = false;
    bool looper_allow_tick = true;
};

class win32_wnd_c;
win32_wnd_c * _cdecl hwnd2wnd( HWND hwnd );
HWND _cdecl wnd2hwnd( const wnd_c * );

struct draw_target_s
{
    const bmpcore_exbody_s *eb;
    HDC dc;
    explicit draw_target_s( const bmpcore_exbody_s &eb_ ) :eb( &eb_ ), dc( nullptr ) {}
    explicit draw_target_s( HDC dc ) :eb( nullptr ), dc( dc ) {}
};
