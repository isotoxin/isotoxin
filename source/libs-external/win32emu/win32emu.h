#pragma once

#define WIN32EMU

enum w32e_bool
{
    FALSE,
    TRUE
};

enum w32e_ht
{
    HT_NULL,
    HT_EVENT
};

enum w32e_consts
{
    WAIT_OBJECT_0 = 0,
    WAIT_TIMEOUT = 258,
    WAIT_FAILED = 0xFFFFFFFF,
    INFINITE = 0xFFFFFFFF,
    
    CSTR_NOTEQUAL = 0,
    CSTR_EQUAL = 1,
    LOCALE_USER_DEFAULT = 2,
    NORM_IGNORECASE = 3
};

#ifdef __linux__

typedef union _LARGE_INTEGER {
    struct {
        uint32_t LowPart;
        int32_t HighPart;
    } DUMMYSTRUCTNAME;
    uint64_t QuadPart;
} LARGE_INTEGER;

//typedef uint32_t DWORD;

static_assert( sizeof(void *) == (sizeof(int) * 2), "sorry, only 64 bit supported" );

typedef union
{
    void *evt;
    struct
    {
        int h;
        unsigned char ht;
        bool manualreset;
    };
} evt_t;

static_assert( sizeof(evt_t) == sizeof(void *), "size" );

inline void *CreateEvent(void *, w32e_bool bManualReset, w32e_bool bInitialState, void *)
{
    evt_t t;
    t.h = eventfd( bInitialState ? 1 : 0, 0 );
    if (t.h == -1)
        return nullptr;
    t.ht = HT_EVENT;
    t.manualreset = bManualReset != FALSE;
    return t.evt;
}

inline void SetEvent( void *evt )
{
    evt_t t;
    t.evt = evt;
    if (t.ht == HT_EVENT)
    {
        int64 inc1 = 1;
        write( t.h, &inc1, 8 );
    }
}

inline void CloseHandle( void *evt )
{
    evt_t t;
    t.evt = evt;
    switch (t.ht)
    {
    case HT_EVENT:
        close( t.h );
        break;
    }
}

unsigned int WaitForSingleObject( void *evt, int timeout );

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
    uint64 time = 1000ULL * monotime.tv_sec + (monotime.tv_nsec / 1000000ULL);
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

w32e_consts CompareStringW( w32e_consts l, w32e_consts o, const wchar_t *s1, int len1, const wchar_t *s2, int len2 );
w32e_consts CompareStringA( w32e_consts l, w32e_consts o, const char *s1, int len1, const char *s2, int len2 );
void CharLowerBuffW( wchar_t *s1, int len );
void CharLowerBuffA( char *s1, int len );
void CharUpperBuffW( wchar_t *s1, int len );
void CharUpperBuffA( char *s1, int len );

#endif