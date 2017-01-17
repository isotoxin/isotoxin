#pragma once

#define WIN32EMU

#include <sys/cdefs.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <new>

enum w32e_bool
{
    FALSE,
    TRUE
};

enum w32e_consts
{
    WAIT_OBJECT_0 = 0,
    WAIT_TIMEOUT = 258,
    WAIT_FAILED = 0xFFFFFFFF,
    INFINITE = 0xFFFFFFFF,
    INVALID_HANDLE_VALUE = 0xFFFFFFFF,

    CSTR_NOTEQUAL = 0,
    CSTR_EQUAL = 1,
    LOCALE_USER_DEFAULT = 2,
    NORM_IGNORECASE = 3,

    PAGE_READWRITE = 4,
    FILE_MAP_WRITE = 2,

    ERROR_SUCCESS = 0,
    ERROR_PIPE_LISTENING = 536,
};

#ifdef __linux__

typedef union _LARGE_INTEGER {
    struct {
        uint32_t LowPart;
        int32_t HighPart;
    } DUMMYSTRUCTNAME;
    uint64_t QuadPart;
} LARGE_INTEGER;

static_assert( sizeof(void *) == (sizeof(int) * 2), "sorry, only 64 bit supported" );

struct w32e_handle_s
{
    virtual void close() = 0;
    virtual uint32_t read(void *b, uint32_t sz, uint32_t *rdb)
    {
        if (rdb) *rdb = 0;
        return 0;
    }

    virtual uint32_t write(const void *b, uint32_t sz, uint32_t *wrrn)
    {
        if (wrrn) *wrrn = 0;
        return 0;
    }

protected:
    ~w32e_handle_s() {} // no need to make it virtual

};

#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"

struct w32e_handle_event_s : public w32e_handle_s
{
    int h;
    bool manualreset;
    virtual void close() override
    {
        ::close( h );
        delete this;
    }
};

struct w32e_handle_shm_s : public w32e_handle_s
{
    int h;
    w32e_handle_shm_s(int h, const char *name, int l):h(h)
    {
        memcpy( this + 1, name, l );
    }
    virtual void close() override
    {
        ::close( h );
        shm_unlink((const char *)(this + 1));
        w32e_handle_shm_s *me = this;
        me->~w32e_handle_shm_s();
        free(me);
    }
};

struct w32e_handle_pipe_s : public w32e_handle_s
{
    virtual void close() override
    {

    }
    virtual uint32_t read(void *b, uint32_t sz, uint32_t *rdb) override
    {

        if (rdb) *rdb = 0;
        return 0;
    }

    virtual uint32_t write(const void *b, uint32_t sz, uint32_t *wrrn) override
    {
        if (wrrn) *wrrn = 0;
        return 0;
    }
};


typedef w32e_handle_s * HANDLE;

#if defined _SYS_EVENTFD_H
inline HANDLE CreateEvent(void *, w32e_bool bManualReset, w32e_bool bInitialState, void *)
{
    int h = eventfd( bInitialState ? 1 : 0, 0 );
    if (h < 0)
        return nullptr;

    w32e_handle_event_s *e = new w32e_handle_event_s;
    e->manualreset = bManualReset != FALSE;
    e->h = h;
    return e;
}
#endif

inline void SetEvent( HANDLE evt )
{
    if (w32e_handle_event_s *e = dynamic_cast<w32e_handle_event_s *>(evt))
    {
        int64_t inc1 = 1;
        write( e->h, &inc1, 8 );
    }
}

inline void CloseHandle( HANDLE h )
{
    h->close();
}

unsigned int WaitForSingleObject( HANDLE evt, int timeout );

inline void Sleep(int ms)
{
    if (0 == ms)
    {
        pthread_yield();
        return;
    }
    struct timespec req;
    req.tv_sec = ms >= 1000 ? (ms / 1000) : 0;
    req.tv_nsec = (ms - req.tv_sec * 1000) * 1000000;

    // Loop until we've slept long enough
    do
    {
        // Store remainder back on top of the original required time
        if( 0 != nanosleep( &req, &req ) )
        {
            /* If any error other than a signal interrupt occurs, return an error */
            if(errno != EINTR)
                return;
        }
        else
        {
            // nanosleep succeeded, so exit the loop
            break;
        }
    } while( req.tv_sec > 0 || req.tv_nsec > 0 );
}

inline int timeGetTime()
{
    struct timespec monotime;
#if defined(__linux__) && defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &monotime);
#else
    clock_gettime(CLOCK_MONOTONIC, &monotime);
#endif
    uint64_t time = 1000ULL * monotime.tv_sec + (monotime.tv_nsec / 1000000ULL);
    return time & 0xffffffff;
}

inline int GetTickCount()
{
    return timeGetTime();
}

inline void QueryPerformanceCounter(LARGE_INTEGER *li)
{
    struct timespec monotime;
#if defined(__linux__) && defined(CLOCK_MONOTONIC_RAW)
    clock_gettime(CLOCK_MONOTONIC_RAW, &monotime);
#else
    clock_gettime(CLOCK_MONOTONIC, &monotime);
#endif
    li->QuadPart = 1000000000ULL * monotime.tv_sec + monotime.tv_nsec;
}

inline void QueryPerformanceFrequency(LARGE_INTEGER *li)
{
    li->QuadPart = 1000000000ULL;
}

inline uint32_t GetCurrentProcessId()
{
    return getpid();
}

w32e_consts CompareStringW( w32e_consts l, w32e_consts o, const char16_t *s1, int len1, const char16_t *s2, int len2 );
w32e_consts CompareStringA( w32e_consts l, w32e_consts o, const char *s1, int len1, const char *s2, int len2 );
void CharLowerBuffW( char16_t *s1, int len );
void CharLowerBuffA( char *s1, int len );
void CharUpperBuffW( char16_t *s1, int len );
void CharUpperBuffA( char *s1, int len );

inline w32e_consts GetLastError()
{

    return ERROR_SUCCESS;
}

// mmf

inline off_t w32e_shm_size(off_t sz)
{
    off_t psz = sysconf(_SC_PAGESIZE);
    sz += 16 + psz - 1;
    return sz & ~(psz-1);
}

inline HANDLE CreateFileMappingA(w32e_consts, int, int ofl, int, off_t sz, const char *name)
{
    if (PAGE_READWRITE != ofl)
        return nullptr;

    int h = shm_open(name, O_CREAT|O_RDWR, 0666);
    if (h < 0)
        return nullptr;

    ftruncate( h, w32e_shm_size(sz) );
    int nl = strlen(name) + 1;
    w32e_handle_shm_s *shm = (w32e_handle_shm_s *)malloc( nl + sizeof(w32e_handle_shm_s) );
    new(shm) w32e_handle_shm_s(h, name, nl);

    return shm;
}

inline void *MapViewOfFile(HANDLE mapping, int flags, int, int, off_t sz)
{
    if (FILE_MAP_WRITE != flags)
        return nullptr;

    if (w32e_handle_shm_s *shm = dynamic_cast<w32e_handle_shm_s *>(mapping))
    {
        off_t rsz = w32e_shm_size(sz);
        uint8_t *ptr = (uint8_t *)mmap(nullptr, rsz, PROT_READ|PROT_WRITE, MAP_SHARED, shm->h, 0 );
        *(off_t *)ptr = rsz;
        return ptr + 16;
    }

    return nullptr;
}


inline void UnmapViewOfFile(void *ptr)
{
    uint8_t *bptr = (uint8_t *)ptr;
    bptr = bptr - 16;
    munmap( bptr, *(off_t *)bptr );
}

// file
inline uint32_t ReadFile(HANDLE h, void *b, uint32_t sz, uint32_t *rdb, void * ovrlpd)
{
    return h->read(b,sz,rdb);
}

inline uint32_t WriteFile(HANDLE h, const void *b, uint32_t sz, uint32_t *wrrn, void * ovrlpd)
{
    return h->write(b,sz,wrrn);
}


#define MAX_PATH 256

#define WSAStartup(a,b) (0)
#define WSACleanup() ;
#define WSAGetLastError() (errno)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

#define closesocket close


typedef int SOCKET;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr SOCKADDR;
struct WSADATA {};



#endif