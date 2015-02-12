/*
    win32 system module
    (C) 2010-2015 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#ifndef _WINDOWS_
#error please include <windows.h> before "system.h"
#endif

#include <stddef.h> // need "offsetof" macro

#define SS_JOINMACRO2(x,y) x##y
#define SS_JOINMACRO1(x,y) SS_JOINMACRO2(x,y)
#define SS_UNIQIDLINE(x) SS_JOINMACRO1(x,__LINE__)


#define SIMPLE_SYSTEM_EVENT_RECEIVER( parent, ev ) struct SS_UNIQIDLINE( ev##clazz ) : public system_event_receiver_c \
{ \
    SS_UNIQIDLINE( ev##clazz )():system_event_receiver_c(ev) {} \
    ~ SS_UNIQIDLINE( ev##clazz )() { unsubscribe(ev);} \
    virtual DWORD event_happens( const system_event_param_s & param ) \
    { \
        parent *p = (parent *)( ((::byte *)this) - offsetof( parent, SS_UNIQIDLINE( m_##ev ) ) ); \
        return p->handler_##ev( param ); \
    } \
} SS_UNIQIDLINE( m_##ev ); \
	DWORD handler_##ev( const system_event_param_s & p )


enum system_event_e
{
    SEV_STD_WINDOW,     // should system lib create standard app window? return SRBIT_ACCEPTED if yes
    SEV_BEFORE_INIT,    // called ones, after singletons init, main window created, before SEV_INIT
    SEV_INIT,           // called every restart
    SEV_SHUTDOWN,       // called every restart
    SEV_EXIT,           // called before singletons deinit

    SEV_WCHAR,
    SEV_ACTIVE_STATE,
    SEV_COMMAND,
    SEV_CLOSE,          // return SRBIT_ABORT to abort close
    SEV_CLOSE_ABORTED,
    SEV_SIZE_CHANGED,
    SEV_POS_CHANGED,
    SEV_MOUSE,          // самый низкий уровень - перехватить невозможно
    SEV_KEYBOARD,       // самый низкий уровень - перехватить невозможно
    SEV_MOUSE2,         // уровень приложения - вызывать самостоятельно из anymessage (sys_dispatch_input)
    SEV_KEYBOARD2,      // уровень приложения - вызывать самостоятельно из anymessage (sys_dispatch_input)
    SEV_SETCURSOR,
    SEV_LOOP,           // active loop
    SEV_IDLE,           // inactive loop
    SEV_PAINT,

    SEV_BEGIN_FRAME,    // before idle or loop event
    SEV_END_FRAME,      // after idle or loop event

    SEV_ANYMESSAGE,

    SEV_MAX,
    SEV_DWORD = 0x7FFFFFFF
};

struct system_event_param_s
{
    struct mouse_s
    {
        int     message;
        WPARAM  wp;
        POINT   pos; // in screen
    };

    struct keyboard_s
    {
        HWND    hwnd;
        int     scan;
        bool    down;
    };

    struct paint_s
    {
        HWND    hwnd;
        HDC     dc;
    };

    struct rv_s
    {
        int     rv;
        bool    rvpresent;
    };

    struct message_s
    {
        int msg;
        WPARAM wp;
        LPARAM lp;
        rv_s  *rv;
    };

    union
    {
        HWND        hwnd;
        mouse_s     mouse;
        keyboard_s  kbd;
        paint_s     paint;
        message_s   msg;
        SIZE        sz;
        WPARAM      cmd;
        wchar_t     c;
        bool        active;
    };
};

#define SRBIT_ABORT     (1<<0)
#define SRBIT_ACCEPTED  (1<<1)
#define SRBIT_FAIL      (1<<2)

class system_event_receiver_c
{
    system_event_receiver_c *_ser_next;
    system_event_receiver_c *_ser_prev;

public:

    static DWORD notify_system_receivers( system_event_e ev, const system_event_param_s &par );


    system_event_receiver_c( system_event_e ev );
    void unsubscribe( system_event_e ev );
    virtual DWORD event_happens( const system_event_param_s & param ) = 0;

};

struct system_conf_s
{
    wchar_t name[ 256 ];

    char    version[64];

    HINSTANCE instance;
    HWND    mainwindow;
    HWND    modalwindow;

    DWORD   dwStyle;
    DWORD   dwExStyle;

    bool    is_exit; // sys_exit
    bool    is_aborting;
    bool    is_active;
    bool    is_loop_in_background;
    bool    is_app_restart;
    bool    is_app_need_quit;
    bool    is_app_running;
    bool    is_in_render;
    bool    eat_win_keys;
    bool    purge_messages;
	bool	do_graphics;

    system_conf_s(void)
    {
        memset( this, 0, sizeof(system_conf_s) );
        dwStyle = WS_OVERLAPPEDWINDOW;
        dwExStyle = WS_EX_APPWINDOW;
        eat_win_keys = true;
    }
};

extern system_conf_s g_sysconf;

// app specific
bool _cdecl app_preinit(const wchar_t *cmdline);

// lib
void _cdecl sys_exit( int iErrCode );
void _cdecl sys_restart( void );
bool _cdecl sys_dispatch_input( const system_event_param_s & p );
LRESULT CALLBACK sys_def_main_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// keyboard

// VK

#define VK_LB_DOUBLE	1000
// scan codes
enum {
	SSK_LB          = 230,
	SSK_RB          = 231,
	SSK_MB          = 232,
    SSK_XB1         = 233,
    SSK_XB2         = 234,
	SSK_WHEELUP	    = 236,
	SSK_WHEELDN	    = 237,

    SSK_ALT	        = 238,
    SSK_CTRL        = 239,
    SSK_SHIFT       = 249,
    SSK_WIN         = 241,
    SSK_AENTER      = 242,  // left or right ENTER

    SSK_L2CLICK     = 243,

	SSK_ESC         = 1,
	SSK_F1          = 59,
	SSK_F2          = 60,
	SSK_F3          = 61,
	SSK_F4          = 62,
	SSK_F5          = 63,
	SSK_F6          = 64,
	SSK_F7          = 65,
	SSK_F8          = 66,
	SSK_F9          = 67,
	SSK_F10         = 68,
	SSK_F11         = 87,
	SSK_F12         = 88,

    SSK_PRINTSCREEN = 183,
	SSK_PAUSE       = 69,

	SSK_SLOCK       = 70,
	SSK_CLOCK       = 58,
	SSK_NLOCK       = 197,

	SSK_TILDA       = 41,
	SSK_RSLASH      = 53,
	SSK_LSLASH      = 43,
	SSK_DOT         = 52,
	SSK_COMMA       = 51,
	SSK_SEMICOLON   = 39,
	SSK_APOSTROF    = 40,
	SSK_LBRACKET    = 26,
	SSK_RBRACKET    = 27,
	SSK_MINUS       = 12,
	SSK_EQUAL       = 13,

	SSK_PADSLASH    = 181,
	SSK_PADSTAR     = 55,
	SSK_PADMINUS    = 74,
	SSK_PADPLUS     = 78,
	SSK_PADENTER    = 156,
	SSK_PADDEL      = 83,
	SSK_PAD0        = 82,
	SSK_PAD1        = 79,
	SSK_PAD2        = 80,
	SSK_PAD3        = 81,
	SSK_PAD4        = 75,
	SSK_PAD5        = 76,
	SSK_PAD6        = 77,
	SSK_PAD7        = 71,
	SSK_PAD8        = 72,
	SSK_PAD9        = 73,

	SSK_1           = 2,
	SSK_2           = 3,
	SSK_3           = 4,
	SSK_4           = 5,
	SSK_5           = 6,
	SSK_6           = 7,
	SSK_7           = 8,
	SSK_8           = 9,
	SSK_9           = 10,
	SSK_0           = 11,

	SSK_LEFT        = 203,
	SSK_RIGHT       = 205,
	SSK_UP          = 200,
	SSK_DOWN        = 208,

	SSK_BACKSPACE   = 14,
	SSK_TAB         = 15,
	SSK_ENTER       = 28,
	SSK_SPACE       = 57,

	SSK_INSERT      = 210,
	SSK_DELETE      = 211,
	SSK_HOME        = 199,
	SSK_END         = 207,
	SSK_PGUP        = 201,
	SSK_PGDN        = 209,

	SSK_LSHIFT      = 42,
	SSK_RSHIFT      = 54,
	SSK_LALT        = 56,
	SSK_RALT        = 184,
	SSK_LCTRL       = 29,
	SSK_RCTRL       = 157,

	SSK_LWIN        = 219,
	SSK_RWIN        = 220,
	SSK_MENU        = 221,

	SSK_Q           = 16,
	SSK_W           = 17,
	SSK_E           = 18,
	SSK_R           = 19,
	SSK_T           = 20,
	SSK_Y           = 21,
	SSK_U           = 22,
	SSK_I           = 23,
	SSK_O           = 24,
	SSK_P           = 25,

	SSK_A           = 30,
	SSK_S           = 31,
	SSK_D           = 32,
	SSK_F           = 33,
	SSK_G           = 34,
	SSK_H           = 35,
	SSK_J           = 36,
	SSK_K           = 37,
	SSK_L           = 38,

	SSK_Z           = 44,
	SSK_X           = 45,
	SSK_C           = 46,
	SSK_V           = 47,
	SSK_B           = 48,
	SSK_N           = 49,
	SSK_M           = 50,
};

#pragma warning (push)
#pragma warning (disable: 4035)
__forceinline byte __fastcall lp2key(DWORD lp)
{
#ifdef WIN32
    _asm
    {
        mov eax,lp
        shr eax,15
        shr ah,1
        shr ah,1
        rcr al,1
        //and eax,255
    };
#else
#error
#endif
}

__forceinline int __fastcall scan_f_to_index(int scan)
{
    switch (scan)
    {
    case SSK_F1: return 0;
    case SSK_F2: return 1;
    case SSK_F3: return 2;
    case SSK_F4: return 3;
    case SSK_F5: return 4;
    case SSK_F6: return 5;
    case SSK_F7: return 6;
    case SSK_F8: return 7;
    case SSK_F9: return 8;
    case SSK_F10: return 9;
    case SSK_F11: return 10;
    case SSK_F12: return 11;
    }
    __debugbreak(); // opa
}

__forceinline int __fastcall scan_num_to_index(int scan)
{
    switch (scan)
    {
    case SSK_0: return 0;
    case SSK_1: return 1;
    case SSK_2: return 2;
    case SSK_3: return 3;
    case SSK_4: return 4;
    case SSK_5: return 5;
    case SSK_6: return 6;
    case SSK_7: return 7;
    case SSK_8: return 8;
    case SSK_9: return 9;
    }
    __debugbreak(); // opa
}

__forceinline int __fastcall scan_pad_to_index(int scan)
{
    switch (scan)
    {
    case SSK_PAD0: return 0;
    case SSK_PAD1: return 1;
    case SSK_PAD2: return 2;
    case SSK_PAD3: return 3;
    case SSK_PAD4: return 4;
    case SSK_PAD5: return 5;
    case SSK_PAD6: return 6;
    case SSK_PAD7: return 7;
    case SSK_PAD8: return 8;
    case SSK_PAD9: return 9;
    }
    __debugbreak(); // opa
}

#pragma warning (pop)

#endif

