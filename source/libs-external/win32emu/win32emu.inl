#define WIN32EMU
#ifdef WIN32EMU

// please, include this file into any cpp file of your project (executable or dynamic library, NOT static lib!)

#include <poll.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>

struct win32_emu_state_s
{
    w32e_consts lasterror = ERROR_SUCCESS;
};

__thread win32_emu_state_s w32state;

w32e_consts GetLastError()
{
    return w32state.lasterror;
}

HANDLE CreateNamedPipeA( const char *name, int dwOpenMode, int dwPipeMode, int nMaxInstances, int nOutBufferSize, int nInBufferSize, int nDefaultTimeOut, void *secur )
{
    w32state.lasterror = ERROR_SUCCESS;
    int fifo = mkfifo( name, 0777 ); // (dwOpenMode & PIPE_ACCESS_INBOUND) ? O_RDONLY : O_WRONLY
    if (fifo < 0)
    {
        if (errno == EEXIST)
            w32state.lasterror = ERROR_PIPE_BUSY;
        return INVALID_HANDLE_VALUE;
    }

    return new w32e_handle_pipe_s(name);
}

HANDLE CreateFileA(const char *name, uint32_t readwrite, int, void *, w32e_consts openmode, w32e_consts attr, void *)
{
    struct stat st;
    int e = stat(name,&st);
    if (e < 0)
        return INVALID_HANDLE_VALUE;

    if (S_ISFIFO(st.st_mode))
        return new w32e_handle_pipe_s(name);

    int mode = 0;
    if (readwrite & GENERIC_WRITE)
        mode |= O_WRONLY;
    if (0 != (readwrite & GENERIC_READ))
        mode |= O_RDONLY;
    int h = open(name, mode);
    if (h < 0)
        return INVALID_HANDLE_VALUE;

    //fcntl(h,F_SETFL,mode);

    close(h);
    __debugbreak();

    return INVALID_HANDLE_VALUE;
}

namespace
{
    struct thstart_s
    {
        THREADPROC *proc;
        void *par;

        thstart_s(THREADPROC *proc, void *par):proc(proc), par(par) {}

        static void *procx(void *par)
        {
            thstart_s p = *(thstart_s *)par;
            delete (thstart_s *)par;
            p.proc(p.par);
            return nullptr;
        }

    };

}

HANDLE CreateThread(void *, int, THREADPROC *proc, void *par, int, void *)
{
    pthread_t tid;
    thstart_s *tsp = new thstart_s(proc, par);

    pthread_attr_t threadAttr;
    pthread_attr_init(&threadAttr);
    pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

    int err = pthread_create(&tid, &threadAttr, thstart_s::procx, tsp);

    if (err == 0)
        return new w32e_handle_thread_s(tid);

    delete tsp;

    return nullptr;
}

unsigned int WaitForSingleObject( HANDLE evt, int timeout )
{
    if (w32e_handle_event_s *e = dynamic_cast<w32e_handle_event_s *>(evt))
    {
        if ( INFINITE != timeout )
        {
            struct timeval now;
            struct timespec timeouttime;
            gettimeofday(&now, 0);
            int secs = timeout / 1000;
            int msecs = timeout - (secs * 1000);
            timeouttime.tv_sec = now.tv_sec + secs;
            timeouttime.tv_nsec = now.tv_usec * 1000 + msecs * 1000000;
            if (timeouttime.tv_nsec >= 1000000000)
            {
                ++timeouttime.tv_sec;
                timeouttime.tv_nsec -= 1000000000;
            }


            int rv = 0;
            pthread_mutex_lock(&e->mutex);
            while (!e->signaled)
            {
                rv = pthread_cond_timedwait(&e->cond, &e->mutex, &timeouttime);
                if (ETIMEDOUT == rv)
                    break;
            }
            if (!e->manualreset)
                e->signaled = false;
            pthread_mutex_unlock(&e->mutex);

            if (ETIMEDOUT == rv)
                return WAIT_TIMEOUT;

            return WAIT_OBJECT_0;
        }

        pthread_mutex_lock(&e->mutex);
        while (!e->signaled)
            pthread_cond_wait(&e->cond, &e->mutex);
        if (!e->manualreset)
            e->signaled = false;
        pthread_mutex_unlock(&e->mutex);
        return WAIT_OBJECT_0;

    }
    return WAIT_FAILED;
}

void Sleep(int ms)
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


w32e_consts CompareStringW( w32e_consts l, w32e_consts o, const char16_t *s1, int len1, const char16_t *s2, int len2 )
{
    if ( len1 != len2 )
        return CSTR_NOTEQUAL;

    wchar_t *buf = (wchar_t *)alloca( (len1*2) * sizeof(wchar_t) );
    for(int i=0;i<len1;++i)
    {
        buf[i] = s1[i];
        buf[i+len1] = s2[i];
    }

    return 0 == wcsncasecmp( buf, buf+len1, len1 ) ? CSTR_EQUAL : CSTR_NOTEQUAL;
}

w32e_consts CompareStringA( w32e_consts l, w32e_consts o, const char *s1, int len1, const char *s2, int len2 )
{
    if ( len1 != len2 )
        return CSTR_NOTEQUAL;
    return 0 == strncasecmp( s1, s2, len1 ) ? CSTR_EQUAL : CSTR_NOTEQUAL;
}

void CharLowerBuffW( char16_t *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = towlower( *s1 );
}

void CharLowerBuffA( char *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = tolower( *s1 );
}
void CharUpperBuffW( char16_t *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = toupper( *s1 );
}

void CharUpperBuffA( char *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = towupper( *s1 );
}


#endif