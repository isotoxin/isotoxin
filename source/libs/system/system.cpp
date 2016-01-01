#include <ShlDisp.h>
#include <commctrl.h>
#include <windowsx.h>
#include "system.h"

#define WM_LOOPER_TICK (65534)
#define WM_RESTART (65535)

//
//#ifndef WM_UNICHAR
//#define WM_UNICHAR                      0x0109
//#endif

static bool g_allow_event_receivers = false;
static system_event_param_s g_par_def;

#define SS_FLAG(f,mask) (((f)&(mask))!=0)


#define SS_LIST_ADD(el,first,last,prev,next)       {if((last)!=nullptr) {(last)->next=el;} (el)->prev=(last); (el)->next=0;  last=(el); if(first==0) {first=(el);}}
#define SS_LIST_DEL(el,first,last,prev,next) \
    {if((el)->prev!=0) (el)->prev->next=el->next;\
    if((el)->next!=0) (el)->next->prev=(el)->prev;\
    if((last)==(el)) last=(el)->prev;\
    if((first)==(el)) (first)=(el)->next;}

namespace
{
struct evlst_s
{
    system_event_receiver_c *_ser_first;
    system_event_receiver_c *_ser_last;
    system_event_receiver_c *_curnext;
};
static evlst_s _evs[SEV_MAX] = {0};
}

system_event_receiver_c::system_event_receiver_c( system_event_e ev )
{
    evlst_s &l = _evs[ev];
    SS_LIST_ADD( this, l._ser_first, l._ser_last, _ser_prev, _ser_next );
}

void system_event_receiver_c::unsubscribe(system_event_e ev)
{
    evlst_s &l = _evs[ev];
    if (l._curnext == this) l._curnext = _ser_next;
    SS_LIST_DEL( this, l._ser_first, l._ser_last, _ser_prev, _ser_next );
}

DWORD system_event_receiver_c::notify_system_receivers( system_event_e ev, const system_event_param_s &par )
{
    if (!g_allow_event_receivers) return 0;

    if ( ev == SEV_IDLE || ev == SEV_LOOP )
    {
        DWORD r = notify_system_receivers( SEV_BEGIN_FRAME, par );
        if (0!=(r & SRBIT_ABORT)) return SRBIT_ABORT;
    }

    evlst_s &l = _evs[ev];
    if (l._curnext)
        return SRBIT_FAIL; // recursive ev call! NOT supported

    DWORD bits = 0;
    system_event_receiver_c *f = _evs[ev]._ser_first;
    for(;f;)
    {
        l._curnext = f->_ser_next;
        bits |= f->event_happens( par );
        f = l._curnext;
    }

    if ( ev == SEV_IDLE || ev == SEV_LOOP )
    {
        /*DWORD r = */ notify_system_receivers( SEV_END_FRAME, par );
        //if (0!=(r & SRBIT_ABORT)) return SRBIT_ABORT;
    }

    return bits;
}

//static HHOOK  g_hKeyboardHook = nullptr;
system_conf_s g_sysconf;

static LRESULT CALLBACK app_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if ( g_sysconf.is_aborting )
        return DefWindowProc ( hWnd, message, wParam, lParam );

    {
        system_event_param_s p;
        system_event_param_s::rv_s rv;
        rv.rvpresent = false;
        p.msg.msg = message;
        p.msg.wp = wParam;
        p.msg.lp = lParam;
        p.msg.rv = &rv;

        system_event_receiver_c::notify_system_receivers( SEV_ANYMESSAGE, p );

        if (rv.rvpresent)
            return rv.rv;
    }


    static bool isCloseProcessed = false;
    // process message
    switch ( message )
    {
        // if application is active
    case WM_ACTIVATEAPP:
        //{
        //    bool bActivate = (wParam != 0);
        //    if ( g_sysconf.is_active != bActivate )
        //    {
        //        g_sysconf.is_active = bActivate;

        //        system_event_param_s p; p.active = bActivate;
        //        system_event_receiver_c::notify_system_receivers( SEV_ACTIVE_STATE, p );
        //    }
        //}
        break;
    case WM_NCACTIVATE:
        {
            g_sysconf.is_active = wParam != 0;
            system_event_param_s p; p.active = g_sysconf.is_active;
            system_event_receiver_c::notify_system_receivers(SEV_ACTIVE_STATE, p);
        }
        return TRUE;
    case WM_ACTIVATE:
        //if ( LOWORD(wParam) != WA_INACTIVE && g_sysconf.is_active == false )
        //{
        //    g_sysconf.is_active = true;
        //    system_event_param_s p; p.active = true;
        //    system_event_receiver_c::notify_system_receivers( SEV_ACTIVE_STATE, p );
        //}
        break;

    //case WM_UNICHAR:
        //{
        //    system_event_param_s p;
        //    char ac = (char)wParam;
        //    MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,&ac,1,(LPWSTR)&p.c,1);
        //    system_event_receiver_c::notify_system_receivers( SEV_WCHAR, p );
        //}

        //return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_COMMAND:
        {
            system_event_param_s p; p.cmd = wParam;
            system_event_receiver_c::notify_system_receivers( SEV_COMMAND, p );
        }
        break;

        // trying to close main window
    case WM_RESTART:
        {
            g_sysconf.is_app_restart = true;
            g_sysconf.is_app_need_quit = true;
            return 0;
        }
    case WM_CLOSE:
        {
            DWORD b = system_event_receiver_c::notify_system_receivers( SEV_CLOSE, g_par_def );
            if (0 != (b & SRBIT_ABORT))
            {
                system_event_receiver_c::notify_system_receivers( SEV_CLOSE_ABORTED, g_par_def );
                return 0;
            }
            system_event_receiver_c::notify_system_receivers( SEV_SHUTDOWN, g_par_def );
            isCloseProcessed = true;
            //DestroyWindow ( hWnd );
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

        // destroy window called
    case WM_DESTROY:
        if (hWnd != g_sysconf.mainwindow) break;
        g_sysconf.mainwindow = nullptr;
        g_sysconf.is_app_need_quit = isCloseProcessed;
        break;

    //case WM_GETMINMAXINFO:
    //    {
    //        MINMAXINFO * pMinMax = (MINMAXINFO *)lParam;
    //        pMinMax->ptMinTrackSize.x = 128;
    //        pMinMax->ptMinTrackSize.y = 128;
    //    }
    //    break;

    case WM_SIZE:
        if ( wParam == SIZE_MINIMIZED )
        {
            if ( g_sysconf.is_active )
            {
                g_sysconf.is_active = false;
                system_event_param_s p; p.active = false;
                system_event_receiver_c::notify_system_receivers( SEV_ACTIVE_STATE, p );
            }
        } else
        {
            system_event_param_s p;
            p.sz.cx = LOWORD( lParam );
            p.sz.cy = HIWORD( lParam );
            system_event_receiver_c::notify_system_receivers( SEV_SIZE_CHANGED, p );
        }
        break;
    case WM_MOVE:
        {
            system_event_param_s p;
            p.sz.cx = LOWORD( lParam );
            p.sz.cy = HIWORD( lParam );
            system_event_receiver_c::notify_system_receivers( SEV_POS_CHANGED, p );
        }
        break;
    case WM_SETCURSOR:
        {
            system_event_param_s p;
            p.hwnd = (HWND)wParam;
            if (SRBIT_ABORT & system_event_receiver_c::notify_system_receivers( SEV_SETCURSOR, p ))
            {
                return TRUE;
            }
        }
    case WM_SETFOCUS:
        if ( !g_sysconf.is_active )
        {
            g_sysconf.is_active = true;
            system_event_param_s p; p.active = true;
            system_event_receiver_c::notify_system_receivers( SEV_ACTIVE_STATE, p );
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_SYSCOMMAND:
        //if (SC_CLOSE == wParam)
        //{
        //    __debugbreak();
        //}
        //if (wParam == SC_KEYMENU) __debugbreak();
        
//        if ( wParam == SC_KEYMENU && lParam == VK_RETURN )
//        {
//#ifndef BM_FINAL
//            /// alt + enter keys pressed
//            g_bAppWindowed = !g_bAppWindowed;
//            g_FullscreenSwitchCallbacker.Call(g_bAppWindowed);
//            if ( g_bNoResize )
//                AppDisableWndResize ();
//#endif
//            break;
//        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_PAINT:
        {
            if ( g_sysconf.is_app_running && g_sysconf.is_active && !g_sysconf.is_in_render )
            {
                g_sysconf.is_in_render = true;

                PAINTSTRUCT ps;
                system_event_param_s p;
                p.paint.hwnd = hWnd;
                p.paint.dc = BeginPaint( hWnd, &ps );

                system_event_receiver_c::notify_system_receivers( SEV_PAINT, p );

                EndPaint( hWnd, &ps );

                g_sysconf.is_in_render = false;
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }



    case WM_ERASEBKGND:
        return 1;
    case WM_SYSCHAR:
        return 0;


    //case WM_SETCURSOR:
    //    if ( g_fnCursorCallback )
    //        return g_fnCursorCallback(hWnd, wParam, lParam);
    //    else 
    //        return DefWindowProc(hWnd, message, wParam, lParam);

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK sys_def_main_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return app_wndproc(hWnd, message, wParam, lParam);
}

static LRESULT CALLBACK looper_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (WM_LOOPER_TICK == message)
    {
        if (!g_sysconf.is_in_render)
            system_event_receiver_c::notify_system_receivers(g_sysconf.is_active ? SEV_LOOP : SEV_IDLE, g_par_def);
        return 0;
    }

    return DefWindowProcW(hWnd,message,wParam,lParam);
}

static ATOM register_window_class()
{
    // only once per application
    static bool bRegistred = false;
    if ( bRegistred )
        return 0;
    bRegistred = true;

    WNDCLASSEXW wcex;

    // register class

    wcex.cbSize 		= sizeof(wcex); 
    wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc	= (WNDPROC) app_wndproc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_sysconf.instance;
    wcex.hIcon			= LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor		= nullptr;//LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH) GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName	= nullptr; 
    wcex.lpszClassName	= L"mainwindowclass";
    wcex.hIconSm		= nullptr;

    return RegisterClassExW(&wcex);
}


static BOOL system_init()
{
    RECT rect;
    rect.left	= 0;
    rect.top	= 0;
    rect.right	= 640;
    rect.bottom	= 480;

    // create window
    g_sysconf.mainwindow = CreateWindowExW( // unicode window
        g_sysconf.dwExStyle,
        L"mainwindowclass", 
        g_sysconf.name, 
        g_sysconf.dwStyle,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr, //g_hWndParent, 
        nullptr, 
        g_sysconf.instance,	// hInstance
        nullptr);

    return g_sysconf.mainwindow != nullptr;
}

static HWND looper_hwnd = nullptr;

static DWORD WINAPI looper(void *)
{
    g_sysconf.looper = true;
    while(g_sysconf.is_app_running && !g_sysconf.is_app_need_quit)
    {
        if ( g_sysconf.is_active || g_sysconf.is_loop_in_background )
            if (looper_hwnd)
                PostMessageW( looper_hwnd, WM_LOOPER_TICK, 0, 0 );
        if (g_sysconf.sleep >= 0) Sleep(g_sysconf.sleep);
    }
    g_sysconf.looper = false;
    return 0;
}

static void system_loop()
{
    g_sysconf.mainthread = GetCurrentThreadId();
    g_sysconf.is_app_running = true;

    if (!g_sysconf.looper)
        CloseHandle(CreateThread(nullptr, 0, looper, nullptr, 0, nullptr));

    MSG msg;
    while (!g_sysconf.is_app_need_quit && GetMessageW(&msg, nullptr, 0U, 0U))
    {
        if (msg.message == WM_LOOPER_TICK)
        {
            MSG msgm;
            if (PeekMessage( &msgm, nullptr, min(WM_KEYFIRST, WM_MOUSEFIRST), max(WM_KEYLAST, WM_MOUSELAST), PM_REMOVE))
                msg = msgm;
        }

        bool downkey = false;
        bool dispatch = true;

        if (looper_hwnd == nullptr)
        {
            WNDCLASSEXW wcex;

            wcex.cbSize = sizeof(wcex);
            wcex.style = 0;
            wcex.lpfnWndProc = (WNDPROC)looper_proc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = g_sysconf.instance;
            wcex.hIcon = nullptr;
            wcex.hCursor = nullptr;
            wcex.hbrBackground = nullptr;
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = L"aoi202309fasdo234";
            wcex.hIconSm = nullptr;

            RegisterClassExW(&wcex);

            looper_hwnd = CreateWindowW(L"aoi202309fasdo234", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, g_sysconf.instance, nullptr);
        }

        int xmsg = msg.message;
        switch( xmsg )
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
            case WM_MOUSEWHEEL:
            case WM_LBUTTONDBLCLK:
                {
do_key:
                    system_event_param_s p;
                    p.mouse.message = xmsg;
                    p.mouse.wp = msg.wParam;
                    p.mouse.pos.x = GET_X_LPARAM( msg.lParam );
                    p.mouse.pos.y = GET_Y_LPARAM( msg.lParam );
                    if (xmsg != WM_MOUSEWHEEL) ClientToScreen(msg.hwnd, &p.mouse.pos);
                    system_event_receiver_c::notify_system_receivers( SEV_MOUSE, p );
                    break;
                }

            case WM_SYSCHAR:
            case WM_CHAR:
                if ((msg.lParam&(1 << 29)) == 0 && GetKeyState(VK_CONTROL) < 0); else
                {
                    // wow! unicode! ... hm... WM_WCHAR?
                    system_event_param_s p; p.c = (wchar_t)msg.wParam;
                    DWORD res = system_event_receiver_c::notify_system_receivers(SEV_WCHAR, p);
                    if (0 != (res & SRBIT_ACCEPTED))
                    {
                        dispatch = false;
                    }
                }
                break;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                downkey = true;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                TranslateMessage(&msg);
                {
                    if ( msg.wParam == VK_SNAPSHOT )
                    {
                        // very special button - only release event can be handled

                        system_event_param_s p;
                        p.kbd.hwnd = msg.hwnd;
                        p.kbd.scan = lp2key(msg.lParam); // 183
                        p.kbd.down = true;

                        DWORD res = system_event_receiver_c::notify_system_receivers( SEV_KEYBOARD, p );
                        if ( 0 != (res & SRBIT_ACCEPTED))
                        {
                            dispatch = false;
                        }

                        p.kbd.down = false;
                        res = system_event_receiver_c::notify_system_receivers( SEV_KEYBOARD, p );
                        if ( 0 != (res & SRBIT_ACCEPTED))
                        {
                            dispatch = false;
                        }

                    } else if ( msg.wParam == VK_MENU )
                    {
                        //dispatch = false;
                        goto kbddo;
                    } else if ( downkey && msg.wParam == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)==0x8000 )
                    {
                        PostMessageW( g_sysconf.mainwindow, WM_CLOSE, 0, 0 );
                        dispatch = false;

#if 0
                    } else if ((msg.wParam == 0x44 || msg.wParam == 0x4D) && ((GetAsyncKeyState(VK_LWIN) & 0x8000)==0x8000 || (GetAsyncKeyState(VK_RWIN) & 0x8000)==0x8000))
                    {

                        ShowWindow( g_sysconf.mainwindow, SW_MINIMIZE );

                        IShellDispatch4 *pShell;

                        HRESULT hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
                            IID_IShellDispatch, (void**)&pShell);

                        if(SUCCEEDED(hr))
                        {
                            pShell->ToggleDesktop();
                            pShell->Release();
                        }

                        //CoUninitialize();
                        dispatch = false;
#endif

                    } else
                    {
kbddo:
                        system_event_param_s p;
                        p.kbd.hwnd = msg.hwnd;
                        p.kbd.scan = lp2key(msg.lParam);
                        p.kbd.down = downkey;

                        if ( 0 != (system_event_receiver_c::notify_system_receivers( SEV_KEYBOARD, p ) & SRBIT_ACCEPTED))
                        {
                            dispatch = false;
                        }

                    }
                    break;
                }

        }

        if (dispatch) DispatchMessageW(&msg);
    }

    if (looper_hwnd)
        DestroyWindow(looper_hwnd);

    g_sysconf.is_app_running = false;
}

/*
LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if (nCode < 0 || nCode != HC_ACTION )  // do not process message
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam);

    bool bEatKeystroke = false;
    KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
    switch (wParam)
    {
    case WM_KEYDOWN:
    case WM_KEYUP:
        {
            if ( g_sysconf.eat_win_keys && g_sysconf.is_active )
            {
                bEatKeystroke = (p->vkCode == VK_LWIN)
                    || (p->vkCode == VK_RWIN);
            }
            break;
        }
    }

    if( bEatKeystroke )
        return 1;
    else
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam );
}
*/

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    g_allow_event_receivers = true;

    wchar_t *cmdlb = GetCommandLineW();


    if ( !app_preinit(cmdlb) ) return 0;

    // Initialization
#ifdef _FINAL
    //g_hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(nullptr), 0 );
#endif

    g_sysconf.instance = hInstance;

    DWORD beforeinit = system_event_receiver_c::notify_system_receivers(SEV_BEFORE_INIT, g_par_def);
    if (0 != (SRBIT_ABORT & beforeinit))
    {
        return 0;
    }
    if (0 != (SRBIT_ACCEPTED & beforeinit))
    {
        // Windows startup
        register_window_class();
        system_init();

        // Common controls.
        InitCommonControls();

        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    while ( 0 == (SRBIT_ABORT & system_event_receiver_c::notify_system_receivers( SEV_INIT, g_par_def )) )
    {
        system_loop();

        if (g_sysconf.is_app_restart)
        {
            system_event_receiver_c::notify_system_receivers( SEV_SHUTDOWN, g_par_def );
            g_sysconf.is_app_need_quit = false;
            g_sysconf.is_app_restart = false;
            continue;
        }
        break;
    }

    CoUninitialize();

    // Windows shutdown
    if ( g_sysconf.mainwindow )
        sys_exit( 0 );

    g_sysconf.is_active = false;
    g_sysconf.is_aborting = true;
    g_sysconf.mainwindow = nullptr;

    // Cleanup before shutdown
#ifdef _FINAL
    //UnhookWindowsHookEx( g_hKeyboardHook );
#endif

    system_event_receiver_c::notify_system_receivers( SEV_EXIT, g_par_def );
    return 0;
}

void _cdecl sys_idle()
{
    MSG m;
    if (PeekMessageW(&m, nullptr, 0U, 0U, PM_REMOVE))
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    system_event_receiver_c::notify_system_receivers( SEV_IDLE, g_par_def );
}

void _cdecl sys_exit ( int code )
{
    g_sysconf.is_exit = true;
    if (g_sysconf.mainwindow)
        ::PostMessage ( g_sysconf.mainwindow, WM_CLOSE, 0, 0 );
    else
        PostQuitMessage( code );
}


void _cdecl sys_restart()
{
    PostMessageW( g_sysconf.mainwindow, WM_RESTART, 0, 0 );
}
