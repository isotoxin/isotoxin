/*
    win32 system module
    (C) 2010-2016 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once


namespace ts
{

enum key_scan_e : unsigned char;

struct folder_spy_handler_s : public safe_object
{
    virtual ~folder_spy_handler_s() {}
    virtual void change_event( uint32 spyid) = 0;
};

struct process_handle_s
{
    static const int datasize = 8;
    uint8 data[ datasize ];

    process_handle_s() { memset( data, 0, sizeof( data ) ); }
    ~process_handle_s(); // platform specific
};

struct global_atom_s
{
    virtual ~global_atom_s() {};
};

enum sys_beep_e
{
    SBEEP_INFO,
    SBEEP_WARNING,
    SBEEP_ERROR,
    SBEEP_BADCLICK,
};

enum cursor_e
{
    CURSOR_ARROW,
    CURSOR_SIZEALL,
    CURSOR_SIZEWE,
    CURSOR_SIZENS,
    CURSOR_SIZENWSE,
    CURSOR_SIZENESW,
    CURSOR_IBEAM,
    CURSOR_HAND,
    CURSOR_CROSS,

    CURSOR_LAST,
};

enum detect_e
{
    DETECT_AUSTOSTART = 1,
    DETECT_READONLY = 2,
};

enum sysinf_e
{
    SINF_SCREENSAVER_RUNNING,
    SINF_LAST_INPUT,
};

class sys_master_c
{
protected:
    virtual void sys_loop() = 0; // called from app_loop
    virtual void shutdown() = 0; // called from app_loop
    void app_loop();

public:
    typedef fastdelegate::FastDelegate< bool () > _HANDLER;
    typedef fastdelegate::FastDelegate< void (mouse_event_e, const ivec2&, const ivec2& ) > _HANDLER_M;
    typedef fastdelegate::FastDelegate< bool ( wchar ) > _HANDLER_C;
    typedef fastdelegate::FastDelegate< bool ( int, bool, int ) > _HANDLER_K;
    typedef fastdelegate::FastDelegate< void () > _HANDLER_T;
    


    _HANDLER on_init;
    _HANDLER on_exit;
    _HANDLER on_loop;
    _HANDLER_M on_mouse;
    _HANDLER_C on_char;
    _HANDLER_K on_keyboard;

    wnd_c::sptr_t mainwindow;
    wnd_c::sptr_t activewindow;

    int sleep = 0;
    bool is_active = false;
    bool is_app_need_quit = false;
    bool is_shutdown = false;
    bool is_sys_loop; // loop from system modal window

    virtual void sys_idle() = 0;
    virtual void sys_exit( int iErrCode ) = 0;

    virtual wstr_c sys_language() = 0;
    virtual int sys_detect_autostart( wstr_c &cmdpar ) = 0;
    virtual void sys_autostart( const wsptr &appname, const wstr_c &exepath, const wsptr &cmdpar ) = 0;

    virtual void sys_start_thread( _HANDLER_T thr ) = 0;
    virtual global_atom_s *sys_global_atom( const wstr_c &n ) = 0;
    virtual bool sys_one_instance( const wstr_c &n, _HANDLER_T notify_cb ) = 0;

    virtual void sys_beep( sys_beep_e beep ) = 0;

    virtual wnd_c *get_focus() = 0;
    virtual wnd_c *get_capture() = 0; // return current mouse capture window
    virtual void release_capture() = 0;

    virtual void set_notification_icon_text( const wsptr& text ) = 0;

    virtual bool start_app( const wstr_c &exe, const wstr_c& prms, process_handle_s *process_handle, bool elevate ) = 0;

    virtual uint32 process_id() = 0;
    virtual bool open_process( uint32 processid, process_handle_s &phandle ) = 0;
    virtual bool wait_process( process_handle_s &phandle, int timeoutms ) = 0;

    virtual void hide_hardware_cursor() = 0;
    virtual void show_hardware_cursor() = 0;
    virtual void set_cursor( cursor_e ct ) = 0;

    virtual int get_system_info( sysinf_e ) = 0;

    virtual bool is_key_pressed( key_scan_e ) = 0;

    virtual uint32 folder_spy(const ts::wstr_c &path, folder_spy_handler_s *handler) = 0;
    virtual void folder_spy_stop(uint32 spyid) = 0;
};

#define MAX_PATH_LENGTH 4096

#ifdef _WIN32
#define MASTERCLASS_INTERNAL_STUFF_SIZE (ARCHBITS / 32 * 128)
class sys_master_win32_c : public sys_master_c
#define MASTER_CLASS sys_master_win32_c
#endif

#ifdef _NIX
#define MASTERCLASS_INTERNAL_STUFF_SIZE 64
class sys_master_nix_c : public sys_master_c
#define MASTER_CLASS sys_master_nix_c
#endif

{
protected:
    /*virtual*/ void sys_loop() override;
    /*virtual*/ void shutdown() override;
public:

    uint8 internal_stuff[ MASTERCLASS_INTERNAL_STUFF_SIZE ];

    MASTER_CLASS();
    ~MASTER_CLASS();

    void do_app_loop() { app_loop(); }

    /*virtual*/ void sys_idle() override;
    /*virtual*/ void sys_exit( int iErrCode ) override;

    /*virtual*/ wstr_c sys_language() override;
    /*virtual*/ int sys_detect_autostart( wstr_c &cmdpar ) override;
    /*virtual*/ void sys_autostart( const wsptr &appname, const wstr_c &exepath, const wsptr &cmdpar ) override;

    /*virtual*/ void sys_start_thread( _HANDLER_T thr ) override;
    /*virtual*/ global_atom_s *sys_global_atom( const wstr_c &n ) override;
    /*virtual*/ bool sys_one_instance( const wstr_c &n, _HANDLER_T notify_cb ) override;

    /*virtual*/ void sys_beep( sys_beep_e beep ) override;

    /*virtual*/ wnd_c *get_focus() override;
    /*virtual*/ wnd_c *get_capture() override;
    /*virtual*/ void release_capture() override;

    /*virtual*/ void set_notification_icon_text( const wsptr& text ) override;

    /*virtual*/ bool start_app( const wstr_c &exe, const wstr_c& prms, process_handle_s *process_handle, bool elevate ) override;
    /*virtual*/ uint32 process_id() override;
    /*virtual*/ bool open_process( uint32 processid, process_handle_s &phandle ) override;
    /*virtual*/ bool wait_process( process_handle_s &phandle, int timeoutms ) override;

    /*virtual*/ void hide_hardware_cursor() override;
    /*virtual*/ void show_hardware_cursor() override;
    /*virtual*/ void set_cursor( cursor_e ct ) override;

    /*virtual*/ int get_system_info( sysinf_e ) override;
    /*virtual*/ bool is_key_pressed( key_scan_e ) override;

    /*virtual*/ uint32 folder_spy(const ts::wstr_c &path, folder_spy_handler_s *handler) override;
    /*virtual*/ void folder_spy_stop(uint32 spyid) override;
};

extern static_setup<MASTER_CLASS, 0> master;

// app specific
bool TSCALL app_preinit(const ts::wchar *cmdline);

// keyboard
enum casw_e : unsigned int
{
    casw_ctrl = 1U << 31,
    casw_alt = 1U << 30,
    casw_shift = 1U << 29,
    casw_win = 1U << 28,
};

// scan codes
enum key_scan_e : unsigned char
{
	SSK_LB          = 230, // fake scan code - never generated by system
	SSK_RB          = 231, // fake scan code - never generated by system

    SSK_CTRL        = 232, // fake scan code
    SSK_SHIFT       = 233, // fake scan code
    SSK_ALT         = 234, // fake scan code

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
	SSK_APOSTROPHE  = 40,
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


} // namespace ts
