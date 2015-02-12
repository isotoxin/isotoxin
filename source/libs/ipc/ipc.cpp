#include "stdafx.h"

#define IPCVER 1

#ifndef __STR1__
#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#endif


namespace ipc
{
namespace
{

enum data_type_e
{
    IPCDT_CUSTOM = 77182
};

struct ipc_member_s
{
    HWND handler = nullptr;
    DWORD pid = 0;
    DWORD tid = 0;
};

struct ipc_sync_s
{
    ipc_member_s members[2];

    ipc_sync_s()
    {
        memset(members,0,sizeof(members));
    }
};

typedef spinlock::syncvar<ipc_sync_s> ipc_sync_struct_s;

struct ipc_data_s
{
    HANDLE mapfile;
    processor_func *datahandler;
    idlejob_func *idlejobhandler;
    void *par_data;
    void *par_idlejob;
    ipc_sync_struct_s *sync; // shared data. I know - process sync via spinlock - is bad idea. Believe me - I know what I do
    int member;
    HANDLE watchdog[2];
    volatile bool quit_quit_quit;
    volatile bool emergency_state;
    volatile bool watch_dog_works;
};

static LRESULT CALLBACK app_wndproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NULL:
        break;
    case WM_CREATE:
        {
            CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (size_t)cs->lpCreateParams);
        }
        return 0;
    case WM_COPYDATA:
        {
            ipc_data_s *data = (ipc_data_s *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lParam;
            switch (cds->dwData)
            {
            case IPCDT_CUSTOM:
                {
                    ipc_result_e r = data->datahandler(data->par_data, cds->lpData, cds->cbData);
                    if (r == IPCR_BREAK) { PostQuitMessage(0); data->quit_quit_quit = true; }
                }
            }

        }
        return TRUE;
    case WM_USER + 17532:
        {
            // idlejob
            ipc_data_s *data = (ipc_data_s *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (IPCR_BREAK == data->idlejobhandler(data->par_idlejob)) { PostQuitMessage(0); data->quit_quit_quit = true; }
        }
        return 0;
    case WM_USER + 7532:
        {
            ipc_data_s *data = (ipc_data_s *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (data->emergency_state || !data->sync->lock_read()().members[data->member ^ 1].handler)
                if (IPCR_BREAK == data->datahandler(data->par_data, nullptr, 0))
                    { PostQuitMessage(0); data->quit_quit_quit = true; }
        }
        return 0;
    }

    return DefWindowProcA(hwnd,message,wParam,lParam);
}

static void reg_handler_class()
{
    // only once per application
    static bool bRegistred = false;
    if (bRegistred) return;
    bRegistred = true;

    // register class
    WNDCLASSEXA wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = 0;
    wcex.lpfnWndProc = (WNDPROC)app_wndproc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.hIcon = nullptr;
    wcex.hCursor = nullptr;
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = "0LLRgdC10Lwg0LTQvtCx0YDQsA";
    wcex.hIconSm = nullptr;

    RegisterClassExA(&wcex);
}


}

DWORD WINAPI watchdog(LPVOID p)
{
    ipc_data_s &d = *(ipc_data_s *)p;
    d.watch_dog_works = true;

    for(;!d.quit_quit_quit;)
    {
        auto r = d.sync->lock_read();
        int partner_index = d.member ^ 1;
        auto & partner = r().members[partner_index];
        if (partner.handler && partner.pid)
        {
            HWND handler = r().members[d.member].handler;
            d.watchdog[0] = OpenProcess(SYNCHRONIZE,FALSE, partner.pid);
            r.unlock();
            if (!d.watchdog[0])
            {
                d.emergency_state = true;
                PostMessageA(handler, WM_USER + 7532, 0, 0);
                break;
            }
            HANDLE processhandler = d.watchdog[0];
            DWORD r = WaitForMultipleObjects(2, d.watchdog, FALSE, INFINITE);
            CloseHandle(processhandler);

            if (r - WAIT_OBJECT_0 == 0)
            {
                d.emergency_state = true; // terminate partner process - is always emergency_state
                PostMessageA(handler, WM_USER + 7532, 0, 0);
            }
            
            break;
        }

        if (partner.pid)
        {
            HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, partner.pid);
            if (h)
            {
                DWORD info = GetProcessVersion(partner.pid);
                if (info == 0) { CloseHandle(h); h = nullptr; }
            }
            if (!h) PostMessageA(r().members[d.member].handler, WM_USER + 7532, 0, 0);
            else CloseHandle(h);
        }
        r.unlock();
        Sleep(1);
    }

    d.watch_dog_works = false;
    return 0;
}

int ipc_junction_s::start( const char *junction_name )
{
    char buf[MAX_PATH];
    static_assert( sizeof(ipc_junction_s) >= sizeof(ipc_data_s), "update size of ipc_junction_s" );
    ipc_data_s &d = (ipc_data_s &)(*this);


    strcpy(buf, "_ipcf_" __STR1__(IPCVER) "_");
    strcat(buf, junction_name);
    d.mapfile = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, sizeof(ipc_sync_struct_s), buf);
    d.member = (GetLastError() == ERROR_ALREADY_EXISTS) ? 1 : 0;
    d.sync = (ipc_sync_struct_s *)MapViewOfFile(d.mapfile, FILE_MAP_WRITE, 0, 0, sizeof(ipc_sync_struct_s));
    if (d.member == 0) d.sync->syncvar<ipc_sync_s>::syncvar();
    d.datahandler = nullptr;
    d.idlejobhandler = nullptr;
    d.watchdog[0] = nullptr;
    d.watchdog[1] = CreateEvent(nullptr,TRUE,FALSE,nullptr);
    d.emergency_state = false;
    d.quit_quit_quit = false;
    d.watch_dog_works = false;

    if (d.member == 1) Sleep(100); // sure 0 member initialized sync block

    if ( d.sync->lock_read()().members[d.member].handler )
    {
        UnmapViewOfFile(d.sync);
        CloseHandle(d.mapfile);
        CloseHandle(d.watchdog[1]);
        memset(this, 0, sizeof(ipc_junction_s));
        return -1;
    }

    reg_handler_class();

    HWND handler = CreateWindowA("0LLRgdC10Lwg0LTQvtCx0YDQsA", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), this);
    if (!handler)
    {
        UnmapViewOfFile(d.sync);
        CloseHandle(d.mapfile);
        CloseHandle(d.watchdog[1]);
        memset(this, 0, sizeof(ipc_junction_s));
        return -1;
    }

    auto w = d.sync->lock_write();
    w().members[d.member].tid = GetCurrentThreadId(); // debug purpose
    w().members[d.member].handler = handler;
    w().members[d.member].pid = GetCurrentProcessId();

    bool wait_0_member = false;
    if (d.member == 1)
    {
        if ( w().members[d.member ^ 1].handler == nullptr )
        {
            // oops, no 0 member
            wait_0_member = true;
        }
    }
    w.unlock();

    if (wait_0_member)
    {
        for(int i=0;i<50;++i)
        {
            Sleep(100);
            auto r = d.sync->lock_read();
            if ( r().members[d.member ^ 1].handler != nullptr )
            {
                wait_0_member = false;
                break;
            }
        }
        if (wait_0_member)
        {
            auto w = d.sync->lock_write();
            w().members[d.member].handler = nullptr;
            w().members[d.member].pid = 0;
            w.unlock();

            UnmapViewOfFile(d.sync);
            CloseHandle(d.mapfile);
            CloseHandle(d.watchdog[1]);
            DestroyWindow(handler);
            memset(this, 0, sizeof(ipc_junction_s));
            return -1;
        }
    }

    CloseHandle(CreateThread(nullptr, 0, watchdog, this, 0, nullptr));
    for( ; !d.watch_dog_works; ) Sleep(1);
    return d.member;
}

void ipc_junction_s::stop()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.sync == nullptr) return; // already cleared

    SetEvent(d.watchdog[1]);
    CloseHandle(d.watchdog[1]);

    d.quit_quit_quit = true;
    for( ; d.watch_dog_works; ) Sleep(1);
    
    auto w = d.sync->lock_write(d.emergency_state);
    HWND handler = w().members[d.member].handler;
    HWND partner = w().members[d.member ^ 1].handler;
    w().members[d.member].handler = nullptr;
    w().members[d.member].pid = 0;
    w.unlock();

    PostMessageA(partner, WM_USER + 7532, 0, 0);
    DestroyWindow(handler);

    UnmapViewOfFile(d.sync);
    CloseHandle(d.mapfile);

    memset(this, 0, sizeof(ipc_junction_s));
}

void ipc_junction_s::idlejob()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (!d.idlejobhandler || d.emergency_state) return;

    HWND me = d.sync->lock_read()().members[d.member].handler;
    PostMessageA( me, WM_USER + 17532, 0, 0 );

}

void ipc_junction_s::set_data_callback( processor_func *f, void *par )
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    d.datahandler = f;
    d.par_data = par;
}

void ipc_junction_s::wait( processor_func *df, void *par_data, idlejob_func *ij, void *par_ij )
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    d.datahandler = df;
    d.par_data = par_data;

    d.idlejobhandler = ij;
    d.par_idlejob = par_ij;
    BOOL bRet;

    MSG msg;
    while (!d.quit_quit_quit && (bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) // WM_QUIT is good, but some times bad
    {
        if (bRet == -1) ; else
        {
            //TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

bool ipc_junction_s::send( const void *data, int datasize )
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.emergency_state || d.quit_quit_quit) return false;

    auto r = d.sync->lock_read();

    HWND me = r().members[d.member].handler;
    HWND receiver = r().members[d.member ^ 1].handler;

    r.unlock();

    COPYDATASTRUCT cds;
    cds.dwData = IPCDT_CUSTOM;
    cds.cbData = datasize;
    cds.lpData = (void *)data;
    HRESULT rslt = SendMessageTimeoutA( receiver, WM_COPYDATA, (WPARAM)me, (LPARAM)&cds, SMTO_NORMAL, 1000, nullptr );
    return rslt != 0;
}

bool ipc_junction_s::wait_partner(int ms)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    DWORD time = timeGetTime();
    for(;!d.emergency_state && int(timeGetTime()-time)<ms;Sleep(1))
        if (d.sync->lock_read()().members[d.member ^ 1].handler) return true;
    return false;
}

} // namespace ipc

