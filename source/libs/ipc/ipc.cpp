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

enum datatype_e
{
    DATATYPE_HANDSHAKE_ASK,
    DATATYPE_HANDSHAKE_ANSWER,
    DATATYPE_IDLEJOB_ASK,
    DATATYPE_IDLEJOB_ANSWER,
    DATATYPE_FIN_ASK,
    DATATYPE_FIN_ANSWER,
    DATATYPE_DATA_128k,
    DATATYPE_DATA_BIG,
};

struct handshake_ask_s
{
    int datasize = sizeof(handshake_ask_s);
    int datatype = DATATYPE_HANDSHAKE_ASK;
    int version = IPCVER;
    DWORD pid = GetCurrentProcessId(); // processid
};

struct handshake_answer_s
{
    int datasize = sizeof(handshake_ask_s);
    int datatype = DATATYPE_HANDSHAKE_ANSWER;
    int version = IPCVER;
    DWORD pid = GetCurrentProcessId(); // processid
};

template<datatype_e d> struct signal_s
{
    int datasize = sizeof(signal_s);
    int datatype = d;
};

struct ipc_data_s
{
    long sync;
    HANDLE pipe_in; // server
    HANDLE pipe_out; // client
    processor_func *datahandler;
    idlejob_func *idlejobhandler;
    void *par_data;
    void *par_idlejob;
    HANDLE watchdog[2];
    DWORD other_pid;
    volatile bool quit_quit_quit;
    volatile bool watch_dog_works;

    struct data_s
    {
        int datasize = sizeof(data_s);
        int datatype = DATATYPE_DATA_128k;
    };

    bool send(const void *data, int datasize)
    {
        spinlock::auto_simple_lock l(sync);

        DWORD w = 0;

        data_s dd; dd.datasize += datasize;

        if (datasize > (65536 * 2))
            dd.datatype = DATATYPE_DATA_BIG;

        WriteFile(pipe_out, &dd, sizeof(data_s), &w, nullptr);
        if (w != sizeof(data_s)) return false;

        WriteFile(pipe_out, data, datasize, &w, nullptr);
        if ((int)w != datasize) return false;

        return true;
    }

    template<typename S> bool send(const S&s)
    {
        DWORD w = 0;
        WriteFile(pipe_out, &s, sizeof(s), &w, nullptr);
        return w == sizeof(s);
    }

#define OOPS do { quit_quit_quit = true; return false; } while(0,false)
//-V:OOPS:521

    bool tick()
    {
        if (quit_quit_quit)
            return false;

        data_s d;

        DWORD r;
        ReadFile(pipe_in, &d, sizeof(d), &r, nullptr);
        if (r != sizeof(d))
        {
            if (ERROR_PIPE_LISTENING == GetLastError())
                return true;

            OOPS;
        }

        switch(d.datatype)
        {
        case DATATYPE_HANDSHAKE_ASK:
            if (other_pid || d.datasize != sizeof(handshake_ask_s))
                OOPS;
            
            {
                handshake_ask_s hsh;

                ReadFile(pipe_in, ((char *)&hsh) + sizeof(data_s), sizeof(handshake_ask_s) - sizeof(data_s), &r, nullptr);
                if (r != sizeof(handshake_ask_s) - sizeof(data_s) || hsh.version != IPCVER)
                    OOPS;
                other_pid = hsh.pid;
            }

            send( handshake_answer_s() );
            return true;
        case DATATYPE_IDLEJOB_ASK:
            send(signal_s<DATATYPE_IDLEJOB_ANSWER>());
            return true;
        case DATATYPE_IDLEJOB_ANSWER:
            if (d.datasize != sizeof(data_s))
                OOPS;
            if (IPCR_BREAK == idlejobhandler(par_idlejob))
                OOPS;
            return true;
        case DATATYPE_FIN_ASK:
            send(signal_s<DATATYPE_FIN_ANSWER>());
            // no break here
        case DATATYPE_FIN_ANSWER:
        case DATATYPE_HANDSHAKE_ANSWER: // only client can receive this and only while connecting
            OOPS;

        case DATATYPE_DATA_128k:
            {
                int trdatasize = (d.datasize - sizeof(data_s));
                if (trdatasize > (65536 * 2))
                    OOPS;

                void *dd = _alloca(trdatasize);
                ReadFile(pipe_in, dd, trdatasize, &r, nullptr);
                if ((int)r != trdatasize)
                    OOPS;

                if (IPCR_BREAK == datahandler(par_data, dd, trdatasize))
                    OOPS;
            }

            return true;

        }

        OOPS;
    }

};

}

DWORD WINAPI watchdog(LPVOID p)
{
    ipc_data_s &d = *(ipc_data_s *)p;
    d.watch_dog_works = true;

    for(;!d.quit_quit_quit;)
    {
        if (d.other_pid)
        {
            d.watchdog[0] = OpenProcess(SYNCHRONIZE,FALSE, d.other_pid);
            if (!d.watchdog[0])
            {
                d.quit_quit_quit = true;
                d.send(signal_s<DATATYPE_FIN_ASK>());
                break;
            }
            HANDLE processhandler = d.watchdog[0];
            DWORD rtn = WaitForMultipleObjects(2, d.watchdog, FALSE, INFINITE);
            CloseHandle(processhandler);
            d.watchdog[0] = nullptr;

            if (rtn - WAIT_OBJECT_0 == 0)
            {
                d.quit_quit_quit = true; // terminate partner process - is always emergency_state
                d.send(signal_s<DATATYPE_FIN_ASK>());
            }
            break;
        }

        if (d.other_pid)
        {
            HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, d.other_pid);
            if (h)
            {
                DWORD info = GetProcessVersion(d.other_pid);
                if (info == 0) { CloseHandle(h); h = nullptr; }
            }
            if (!h) d.send(signal_s<DATATYPE_FIN_ASK>());
            else CloseHandle(h);
        }
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

    bool is_client = false;

    strcpy(buf, "\\\\.\\pipe\\_ipcp0_" __STR1__(IPCVER) "_");
    strcat(buf, junction_name);

    d.pipe_in = CreateNamedPipeA( buf, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536*2, 65536*2, 0, nullptr );
    d.pipe_out = nullptr;
    if (INVALID_HANDLE_VALUE == d.pipe_in)
    {
        d.pipe_in = nullptr;
        if (ERROR_PIPE_BUSY == GetLastError())
        {
            is_client = true;
            // looks like self is client

            d.pipe_out = CreateFileA(buf, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (INVALID_HANDLE_VALUE == d.pipe_out)
                goto finita;

            buf[14] = '1';

            d.pipe_in = CreateFileA( buf, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
            if (INVALID_HANDLE_VALUE == d.pipe_in)
            {
                CloseHandle(d.pipe_out);
                goto finita;
            }

        } else
        {
            finita:
            memset(buffer, 0, sizeof(buffer));
            stop_called = true;
            return -1;
        }
    } else
    {
        buf[14] = '1';
        d.pipe_out = CreateNamedPipeA( buf, PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536*2, 65536*2, 0, nullptr );
        if (INVALID_HANDLE_VALUE == d.pipe_out)
        {
            CloseHandle(d.pipe_in);
            goto finita;
        }
    }

    d.sync = 0;
    d.other_pid = 0;
    d.datahandler = nullptr;
    d.idlejobhandler = nullptr;
    d.watchdog[0] = nullptr;
    d.watchdog[1] = CreateEvent(nullptr,TRUE,FALSE,nullptr);
    d.quit_quit_quit = false;
    d.watch_dog_works = false;

    if (is_client)
    {
        if ( !d.send( handshake_ask_s() ))
        {
        byebye:
            if (d.pipe_in) CloseHandle(d.pipe_in);
            if (d.pipe_out) CloseHandle(d.pipe_out);
            CloseHandle(d.watchdog[1]);
            goto finita;
        }

        DWORD r;
        handshake_answer_s hsh;
        for(;;)
        {
            ReadFile(d.pipe_in, &hsh, sizeof(hsh), &r, nullptr);
            if (r != sizeof(hsh))
            {
                if (ERROR_PIPE_LISTENING == GetLastError())
                {
                    Sleep(0);
                    continue;
                }
                goto byebye;
            }
            break;
        }

        if (hsh.datasize != sizeof(hsh) || hsh.datatype != DATATYPE_HANDSHAKE_ANSWER || hsh.version != IPCVER)
            goto byebye;

        d.other_pid = hsh.pid;

    }

    CloseHandle(CreateThread(nullptr, 0, watchdog, this, 0, nullptr));
    stop_called = false;
    for( ; !d.watch_dog_works; ) Sleep(1);
    return is_client ? 1 : 0;
}

void ipc_junction_s::stop()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.pipe_in == nullptr && d.pipe_out == nullptr)
    {
        if (!stop_called || d.watch_dog_works)
            __debugbreak();

        return; // already cleared
    }

    d.send(signal_s<DATATYPE_FIN_ASK>());

    SetEvent(d.watchdog[1]);
    CloseHandle(d.watchdog[1]);

    d.quit_quit_quit = true;
    for( ; d.watch_dog_works; ) Sleep(1);
    
    if (d.pipe_in) CloseHandle(d.pipe_in);
    if (d.pipe_out) CloseHandle(d.pipe_out);

    memset(buffer, 0, sizeof(buffer));
    stop_called = true;
}

void ipc_junction_s::idlejob()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (!d.idlejobhandler || d.quit_quit_quit) return;
    d.send( signal_s<DATATYPE_IDLEJOB_ASK>() );
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

    bool working = d.tick();
    while (working)
        working = d.tick();
}

bool ipc_junction_s::send( const void *data, int datasize )
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return false;

    return d.send( data, datasize );
}

bool ipc_junction_s::wait_partner(int ms)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    DWORD time = timeGetTime();
    for(;!d.quit_quit_quit && int(timeGetTime()-time)<ms;Sleep(1))
    {
        if (!d.tick())
            return false;
        if (d.other_pid) return true;
    }

    return false;
}

} // namespace ipc

