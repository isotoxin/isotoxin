#include "stdafx.h"

#ifdef _MSC_VER
#pragma warning (disable:4127)
#endif

#ifdef _NIX
#include <win32emu/win32emu.h>
#define min(a,b) (((a)<(b))?(a):(b))
#endif // _NIX


#define IPCVER 2

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
    DATATYPE_NEW_BUFFER,
    DATATYPE_BUFFER_SENT,
    DATATYPE_CLEANUP_BUFFERS,
    DATATYPE_DATA_128k,
    DATATYPE_DATA_BIG,
};

struct handshake_ask_s
{
    int datasize = sizeof(handshake_ask_s);
    int datatype = DATATYPE_HANDSHAKE_ASK;
    int version = IPCVER;
    uint32_t pid = GetCurrentProcessId(); // processid
};

struct handshake_answer_s
{
    int datasize = sizeof(handshake_ask_s);
    int datatype = DATATYPE_HANDSHAKE_ANSWER;
    int version = IPCVER;
    uint32_t pid = GetCurrentProcessId(); // processid
};

struct new_xchg_buffer_s
{
    int datasize = sizeof(new_xchg_buffer_s);
    int datatype = DATATYPE_NEW_BUFFER;
    int allocated;
    char bufname[ MAX_PATH ];

    new_xchg_buffer_s() {}; //-V730
    new_xchg_buffer_s(int alc):allocated(alc) {} //-V730
};

template<datatype_e d> struct signal_s
{
    int datasize = sizeof(signal_s);
    int datatype = d;
};

struct xchg_buffer_header_s
{
    enum bstate_e
    {
        BST_FREE, // pid is 0
        BST_LOCKED, // pid is owner
        BST_SEND, // pid is sender
        BST_DEAD,
    };

    spinlock::long3264 sync;
    int allocated;
    int sendsize;
    uint32_t pid; // owner / sender
    uint32_t lastusetime;
    bstate_e st;

    void *getptr() const;

    bool isptr(const void *ptr) const
    {
        char *pstart = (char *)this;
        ptrdiff_t a = allocated; //-V101
        return ptr >= pstart && ptr < ((char *)getptr()+a);
    }

};

namespace
{
    enum padding_e
    {
        XCHG_BUF_HDR_SIZE = (sizeof(xchg_buffer_header_s) + XCHG_BUFFER_ADDITION_SPACE + 31) & (~31),
    };
}

inline void *xchg_buffer_header_s::getptr() const
{
    char *p = (char *)this;
    return p + XCHG_BUF_HDR_SIZE;
}


#define MAX_XCHG_BUFFERS 16

struct ipc_data_s
{
    spinlock::long3264 sync;
    HANDLE pipe_in; // server
    HANDLE pipe_out; // client
    processor_func *datahandler;
    idlejob_func *idlejobhandler;
    void *par_data;
    void *par_idlejob;


    struct exchange_buffer_s
    {
        HANDLE mapping;
        xchg_buffer_header_s *ptr;

        void reset()
        {
            UnmapViewOfFile(ptr);
            CloseHandle(mapping);
            mapping = nullptr;
            ptr = nullptr;
        }

    } xchg_buffers[MAX_XCHG_BUFFERS];
    int xchg_buffers_count;
    unsigned int xchg_buffer_tag;

    void remove_xchg_buffer(int index)
    {
#if defined _DEBUG && defined _WIN32
        if (!sync || index >= xchg_buffers_count)
            __debugbreak();
#endif // _DEBUG
        xchg_buffers[index].reset();
        memcpy( xchg_buffers + index, xchg_buffers + index + 1, (ptrdiff_t)(--xchg_buffers_count - index) * sizeof(xchg_buffers[0]) );
    }


    HANDLE watchdog[2];
    uint32_t other_pid;
    volatile bool quit_quit_quit;
    volatile bool watch_dog_works;
    volatile bool cleaup_buffers_signal;

    struct data_s
    {
        int datasize = sizeof(data_s);
        int datatype = DATATYPE_DATA_128k;
    };

    bool send(const void *data, int datasize)
    {
        SIMPLELOCK(sync);

        uint32_t w = 0;
        if (cleaup_buffers_signal)
        {
            signal_s<DATATYPE_CLEANUP_BUFFERS> cdb;
            WriteFile(pipe_out, &cdb, sizeof(cdb), &w, nullptr);
            cleaup_buffers_signal = false;
        }

        data_s dd; dd.datasize += datasize;

        if (datasize > BIG_DATA_SIZE)
            dd.datatype = DATATYPE_DATA_BIG;

        WriteFile(pipe_out, &dd, sizeof(data_s), &w, nullptr);
        if (w != sizeof(data_s)) return false;

        WriteFile(pipe_out, data, datasize, &w, nullptr);
        if ((int)w != datasize) return false;

        return true;
    }

    template<typename S> bool send(const S&s)
    {
        SIMPLELOCK(sync);

        uint32_t w = 0;
        if (cleaup_buffers_signal)
        {
            signal_s<DATATYPE_CLEANUP_BUFFERS> cdb;
            WriteFile(pipe_out, &cdb, sizeof(cdb), &w, nullptr);
            cleaup_buffers_signal = false;
        }

        WriteFile(pipe_out, &s, sizeof(s), &w, nullptr);
        return w == sizeof(s);
    }

    void insert_buffer( exchange_buffer_s *newexchb )
    {
#if defined _DEBUG && defined _WIN32
        if (!sync || xchg_buffers_count >= MAX_XCHG_BUFFERS)
            __debugbreak(); // sync must be locked, xchg_buffers_count must be < MAX_XCHG_BUFFERS
#endif // _DEBUG

        for (int i = 0; i < xchg_buffers_count; ++i)
        {
            //allocated is constant and never changed, so it can be read without lock
            if ( xchg_buffers[i].ptr->allocated > newexchb->ptr->allocated )
            {
                // insert here
                memmove( xchg_buffers + i + 1, xchg_buffers + i, (ptrdiff_t)(xchg_buffers_count - i) * sizeof(xchg_buffers[0]) );
                xchg_buffers[i] = *newexchb;
                ++xchg_buffers_count;
                return;
            }
        }

        xchg_buffers[xchg_buffers_count++] = *newexchb;
    }

    void cleanup_buffers()
    {
#if defined _DEBUG && defined _WIN32
        if (!sync)
            __debugbreak(); // sync must be locked
#endif // _DEBUG

        //DWORD pid = GetCurrentProcessId();
        uint32_t timestump = GetTickCount();

        for (int i = xchg_buffers_count-1; i >= 0 ; --i)
        {
            exchange_buffer_s &b = xchg_buffers[i];
            spinlock::auto_simple_lock x(b.ptr->sync);
            if (b.ptr->st == xchg_buffer_header_s::BST_FREE)
            {
                if (int(timestump - b.ptr->lastusetime) > 5000)
                {
                    // 5 seconds unused...
                    // die die die
                    b.ptr->st = xchg_buffer_header_s::BST_DEAD;
                    cleaup_buffers_signal = true;
                    remove_i:
                    x.unlock();
                    remove_xchg_buffer(i);
                }
            } else if (XCHG_BUFFER_LOCK_TIMEOUT > 0 && b.ptr->st == xchg_buffer_header_s::BST_LOCKED)
            {
                #if defined _DEBUG && defined _WIN32
                if (int(timestump - b.ptr->lastusetime) > XCHG_BUFFER_LOCK_TIMEOUT)
                    __debugbreak();
                #endif

            } else if (b.ptr->st == xchg_buffer_header_s::BST_DEAD)
                goto remove_i;
        }
    }



#define OOPS do { quit_quit_quit = true; return false; } while(0,false)
//-V:OOPS:521

    bool tick()
    {
        if (quit_quit_quit)
            return false;

        data_s d;

        uint32_t r;
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

        case DATATYPE_CLEANUP_BUFFERS:
            {
                SIMPLELOCK(sync);
                cleanup_buffers();
            }
            return true;

        case DATATYPE_NEW_BUFFER:
            {
                if (d.datasize != sizeof(new_xchg_buffer_s))
                    OOPS;

                new_xchg_buffer_s buf;
                ReadFile(pipe_in, ((char *)&buf) + sizeof(d), sizeof(new_xchg_buffer_s) - sizeof(d), &r, nullptr);

                ipc_data_s::exchange_buffer_s bb;
                bb.mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, XCHG_BUF_HDR_SIZE + buf.allocated, buf.bufname);
                if (!bb.mapping)
                    OOPS;
                bb.ptr = (xchg_buffer_header_s *)MapViewOfFile(bb.mapping, FILE_MAP_WRITE, 0, 0, (ptrdiff_t)(XCHG_BUF_HDR_SIZE + buf.allocated));
                if (!bb.ptr)
                {
                    CloseHandle(bb.mapping);
                    OOPS;
                }


                SIMPLELOCK(sync);
                cleanup_buffers();
                if (xchg_buffers_count < MAX_XCHG_BUFFERS)
                    insert_buffer(&bb);
                else
                    bb.reset();
            }
            return true;
        case DATATYPE_BUFFER_SENT:
            {
                uint32_t pid = GetCurrentProcessId();
                spinlock::auto_simple_lock l(sync);
                for (int i = 0; i < xchg_buffers_count;++i)
                {
                    exchange_buffer_s &b = xchg_buffers[i];
                    spinlock::auto_simple_lock x(b.ptr->sync);
                    if (b.ptr->st == xchg_buffer_header_s::BST_SEND && b.ptr->pid != pid)
                    {
                        b.ptr->st = xchg_buffer_header_s::BST_LOCKED;
                        b.ptr->pid = pid;
                        b.ptr->lastusetime = GetTickCount();

                        xchg_buffer_header_s *hdr = b.ptr;
                        x.unlock();
                        l.unlock();

                        if (IPCR_BREAK == datahandler(par_data, hdr->getptr(), -hdr->sendsize))
                            OOPS;
                        return true;
                    }
                }
#if defined _DEBUG && defined _WIN32
                __debugbreak();
#endif // _DEBUG
            }
            return true;
        case DATATYPE_DATA_128k:
            {
                int trdatasize = (d.datasize - (int)sizeof(data_s));
                if (trdatasize > BIG_DATA_SIZE)
                    OOPS;

                void *dd = _alloca((size_t)trdatasize);
                int rsz = trdatasize;
                for( char *t = (char *)dd; rsz > 0; )
                {
                    ReadFile(pipe_in, t, rsz, &r, nullptr);
                    rsz -= (int)r;
                    t += (ptrdiff_t)r;
                    if ( rsz < 0 )
                        OOPS;
                }

                if (IPCR_BREAK == datahandler(par_data, dd, trdatasize))
                    OOPS;
            }
            return true;
        case DATATYPE_DATA_BIG:
            {
                int trdatasize = (d.datasize - (int)sizeof(data_s));
                ipc::ipc_result_e rslt = datahandler(par_data, nullptr, trdatasize);
                if (IPCR_BREAK == rslt)
                    OOPS;
                void *dd = _alloca(BIG_DATA_SIZE);

                for (char *t = (char *)dd; trdatasize > 0;)
                {
                    ReadFile(pipe_in, t, min(trdatasize, BIG_DATA_SIZE), &r, nullptr);
                    trdatasize -= (int)r;

                    if (IPCR_SKIP != rslt)
                    {
                        rslt = datahandler(par_data, dd, r);
                        if (IPCR_BREAK == rslt)
                        OOPS;
                    }
                }

                if (IPCR_SKIP != rslt)
                    if (IPCR_BREAK == datahandler(par_data, nullptr, 0))
                        OOPS;

            }
            return true;
        }

        OOPS;
    }

};

}

#ifdef _WIN32
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
#endif

#ifdef _NIX
uint32_t watchdog(void *p)
{

    return 0;
}
#endif


int ipc_junction_s::start( const char *junction_name )
{
    char buf[ MAX_PATH ];
    static_assert( ipc_junction_s::internal_data_size >= sizeof(ipc_data_s), "update size of ipc_junction_s" );
    ipc_data_s &d = (ipc_data_s &)(*this);

    bool is_client = false;

    #ifdef _WIN32
    strcpy(buf, "\\\\.\\pipe\\_ipcp0_" __STR1__(IPCVER) "_");
    static const int nnn = 14;
    #endif
    #ifdef _NIX
    strcpy(buf, "/tmp/ipcp0_" __STR1__(IPCVER) "_");
    static const int nnn = 9;
    #endif
    strcat(buf, junction_name);

    d.pipe_in = CreateNamedPipeA( buf, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, BIG_DATA_SIZE, BIG_DATA_SIZE, 0, nullptr );
    d.pipe_out = nullptr;
    if (INVALID_HANDLE_VALUE == d.pipe_in)
    {
        d.pipe_in = nullptr;
        if (ERROR_PIPE_BUSY == GetLastError())
        {
            // looks like self is client
            is_client = true;

            d.pipe_out = CreateFileA(buf, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (INVALID_HANDLE_VALUE == d.pipe_out)
                goto finita;

            buf[nnn] = '1';

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
        buf[nnn] = '1';
        d.pipe_out = CreateNamedPipeA( buf, PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, BIG_DATA_SIZE, BIG_DATA_SIZE, 0, nullptr );
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
    d.cleaup_buffers_signal = false;
    d.xchg_buffers_count = 0;
    d.xchg_buffer_tag = 0;

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

void ipc_junction_s::stop_signal()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    d.quit_quit_quit = true;
    d.send(signal_s<DATATYPE_FIN_ASK>());
}

void ipc_junction_s::stop()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.pipe_in == nullptr && d.pipe_out == nullptr)
    {
        #if defined _DEBUG && defined _WIN32
        if (!stop_called || d.watch_dog_works)
            __debugbreak();
        #endif

        return; // already cleared
    }

    d.send(signal_s<DATATYPE_FIN_ASK>());

    {
        for(;;Sleep(1))
        {
            spinlock::auto_simple_lock l(d.sync);
            for (int i = d.xchg_buffers_count - 1; i >= 0; --i)
            {
                ipc_data_s::exchange_buffer_s &b = d.xchg_buffers[i];
                spinlock::auto_simple_lock x(b.ptr->sync);
                if (b.ptr->st == xchg_buffer_header_s::BST_LOCKED)
                    if (d.other_pid != b.ptr->pid)
                        continue; // we cant kill locked buffer due memory access violation

                b.ptr->st = xchg_buffer_header_s::BST_DEAD;
                x.unlock();
                d.remove_xchg_buffer(i);
            }
            if (0 == d.xchg_buffers_count) break;
            l.unlock();
        }
    }


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

void *ipc_junction_s::lock_buffer(int size)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return nullptr;

    DWORD pid = GetCurrentProcessId();
    DWORD timestump = GetTickCount();

    spinlock::auto_simple_lock l(d.sync);
    for( int i=0; i<d.xchg_buffers_count; )
    {
        ipc_data_s::exchange_buffer_s &b = d.xchg_buffers[i];
        spinlock::auto_simple_lock x(b.ptr->sync);
        if (b.ptr->st == xchg_buffer_header_s::BST_FREE)
        {
            if (b.ptr->allocated >= size)
            {
                b.ptr->st = xchg_buffer_header_s::BST_LOCKED;
                b.ptr->pid = pid;
                b.ptr->lastusetime = timestump;
                return b.ptr->getptr();
            }

            if ( int(timestump - b.ptr->lastusetime) > 5000 )
            {
                // 5 seconds unused...
                // die die die
                b.ptr->st = xchg_buffer_header_s::BST_DEAD;
                d.cleaup_buffers_signal = true;
            remove_dead_buffer:
                x.unlock();
                d.remove_xchg_buffer(i);
            }
        } else if (b.ptr->st == xchg_buffer_header_s::BST_DEAD)
            goto remove_dead_buffer;

        ++i;
    }
    if (d.xchg_buffers_count == MAX_XCHG_BUFFERS) return nullptr;
    l.unlock();

    new_xchg_buffer_s buf(size);
    sprintf_s(buf.bufname, sizeof(buf.bufname), "ipcb_" __STR1__(IPCVER) "_%u_%u", pid, d.xchg_buffer_tag++);

    ipc_data_s::exchange_buffer_s b;
    b.mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, XCHG_BUF_HDR_SIZE + size, buf.bufname);
    if (nullptr == b.mapping) return nullptr;
    b.ptr = (xchg_buffer_header_s *)MapViewOfFile(b.mapping, FILE_MAP_WRITE, 0, 0, XCHG_BUF_HDR_SIZE + size);
    if (!b.ptr)
    {
        CloseHandle(b.mapping);
        return nullptr;
    }
    b.ptr->st = xchg_buffer_header_s::BST_LOCKED;
    b.ptr->pid = pid;
    b.ptr->allocated = size;
    b.ptr->lastusetime = timestump;

    d.send(buf);

    SIMPLELOCK(d.sync);
    d.insert_buffer( &b );
    return b.ptr->getptr();
}

void ipc_junction_s::unlock_send_buffer(const void *ptr, int sendsize)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return;

    spinlock::auto_simple_lock l(d.sync);
    for (int i = 0; i < d.xchg_buffers_count;++i)
    {
        ipc_data_s::exchange_buffer_s &b = d.xchg_buffers[i];
        spinlock::auto_simple_lock x(b.ptr->sync);

        if (b.ptr->isptr( ptr ))
        {
#ifdef _DEBUG
            if (xchg_buffer_header_s::BST_LOCKED != b.ptr->st || b.ptr->pid != GetCurrentProcessId())
                __debugbreak();
#endif // _DEBUG

            if (sendsize > 0 && sendsize <= b.ptr->allocated)
            {
                b.ptr->st = xchg_buffer_header_s::BST_SEND;
                b.ptr->lastusetime = GetTickCount();
                b.ptr->sendsize = sendsize;
                x.unlock();
                l.unlock();
                d.send(signal_s<DATATYPE_BUFFER_SENT>());
            } else
            {
                b.ptr->st = xchg_buffer_header_s::BST_FREE;
            }
            return;
        }
    }
#ifdef _DEBUG
    __debugbreak();
#endif // _DEBUG
}

void ipc_junction_s::unlock_buffer(const void *ptr)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return;
    SIMPLELOCK(d.sync);
    for (int i = 0; i < d.xchg_buffers_count;++i)
    {
        ipc_data_s::exchange_buffer_s &b = d.xchg_buffers[i];
        spinlock::auto_simple_lock x(b.ptr->sync);

        if (b.ptr->isptr( ptr ))
        {
            if (xchg_buffer_header_s::BST_DEAD == b.ptr->st)
            {
                d.cleaup_buffers_signal = true;
                x.unlock();
                d.remove_xchg_buffer(i);
                return;
            }

#ifdef _DEBUG
            if (xchg_buffer_header_s::BST_LOCKED != b.ptr->st || b.ptr->pid != GetCurrentProcessId())
                __debugbreak();
#endif // _DEBUG

            b.ptr->st = xchg_buffer_header_s::BST_FREE;
            return;
        }
    }

#ifdef _DEBUG
    __debugbreak();
#endif // _DEBUG

}

bool ipc_junction_s::send( const void *data, int datasize )
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return false;

    return d.send( data, datasize );
}

void ipc_junction_s::cleanup_buffers()
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    if (d.quit_quit_quit) return;

    SIMPLELOCK(d.sync);

    if (d.cleaup_buffers_signal)
    {
        // DATATYPE_CLEANUP_BUFFERS must be called directly (not using send)
        signal_s<DATATYPE_CLEANUP_BUFFERS> cdb;
        DWORD  w;
        WriteFile(d.pipe_out, &cdb, sizeof(cdb), &w, nullptr);
        d.cleaup_buffers_signal = false;
    }

    d.cleanup_buffers();
}

bool ipc_junction_s::wait_partner(int ms)
{
    ipc_data_s &d = (ipc_data_s &)(*this);
    DWORD time = GetTickCount();
    for(;!d.quit_quit_quit && int(GetTickCount()-time)<ms;Sleep(1))
    {
        if (!d.tick())
            return false;
        if (d.other_pid) return true;
    }

    return false;
}

} // namespace ipc

