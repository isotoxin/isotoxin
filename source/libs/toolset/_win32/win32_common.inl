#pragma once

#define WM_NOTIFICATION_ICON_CALLBACK (WM_USER + 7213)
#define WM_NOTIFICATION_ICON_EVENT (WM_USER + 7214)

struct master_internal_stuff_s
{
    struct icon_cache_s : public movable_flag<true>
    {
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
            return *this;
        }

    private:
        icon_cache_s( const icon_cache_s& ) UNUSED;
        icon_cache_s &operator = ( const icon_cache_s& ) UNUSED;
    };

    struct spystate_s
    {
        struct maintenance_s : public movable_flag<true>
        {
            safe_ptr<folder_spy_handler_s> h;
            DWORD threadid = 0;
            bool started = false;
            bool finished = false;
            bool stop = false;
            bool energized = false;
        };

        ts::array_inplace_t< maintenance_s, 0 > threads;

        void stop(DWORD threadid);
        const maintenance_s *find(DWORD threadid) const;
        maintenance_s *find(DWORD threadid);
    };

    spinlock::syncvar< spystate_s > fspystate;


    array_safe_t< wnd_c, 2 > activewnds;
    bool actwnd( wnd_c *w, bool a );
    bool isactwnd( wnd_c *w );

    array_inplace_t< icon_cache_s, 4 > icons;
    HICON get_icon( const bitmap_c &bmp );
    void release_icon( HICON icn );

    HWND move_hwnd = nullptr;
    ts::irect move_rect = ts::irect(0);

    sys_master_c::_HANDLER_T popup_notify = nullptr;
    volatile HANDLE popup_event = nullptr;
    //HWND looper_hwnd = nullptr;
    HINSTANCE inst = nullptr;
    UINT_PTR timerid = 0;
    DWORD lasttick = 0;
    int regclassref = 0;
    int sysmodal = 0;
    int cnttick = 0;
    bool looper = false;
    bool looper_staring = false;
    bool looper_allow_tick = true;
    bool folder_spy_evt = false;
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

#pragma warning (push)
#pragma warning (disable: 4035)
__forceinline byte __fastcall lp2key( LPARAM lp )
{
    // essssssssxxxxxxxxxxxxxxxx
    //  llllllllhhhhhhhhllllllll

#ifdef MODE64
    uint64 t = lp;
    t >>= 16;
    t = ( t & 0x7f ) | ( ( t >> 1 ) & 0x80 );

    return as_byte( t & 0xff );
#else
    _asm
    {
        mov eax, lp
        shr eax, 15
        shr ah, 1
        shr ah, 1
        rcr al, 1
        //and eax,255
    };
#endif
}
#pragma warning (pop)
