/*
    spinlock module
    (C) 2010-2016 BMV, ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)

*/
#pragma once
#if defined(_MSC_VER)
#include <intrin.h>
#pragma warning(push)
#pragma warning(disable:4189) // 'val': local variable is initialized but not referenced
#endif
#if defined(__GNUC__)
#include <x86intrin.h>
#endif

#ifndef JOINMACRO1
#define JOINMACRO2(x,y) x##y
#define JOINMACRO1(x,y) JOINMACRO2(x,y)
#endif

#ifndef UNIQIDLINE
#define UNIQIDLINE(x) JOINMACRO1(x,__LINE__)
#endif

#ifndef __STR1__
#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#endif

#ifndef __LOC__
#define __LOC__ __FILE__ "(" __STR1__(__LINE__) ") : "
#endif

namespace spinlock
{
#if defined _WIN32
typedef __int64 int64;
#elif defined __linux__
typedef long long int64;
#else
#error "Sorry, can't compile: unknown target system"
#endif

#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__)
typedef int64 long3264;
#else
typedef long long3264;
#endif

#if defined(_MSC_VER)
// =========================================== Visual Studio stuff begin ======================================
#define SLINLINE __forceinline
#ifndef SLASSERT
__pragma( message( "SLASSERT undefined" ) )
#define SLASSERT(c) (1,true)
#endif

#ifndef SLERROR
__pragma( message( "SLERROR undefined" ) )
#define SLERROR(s, ...) __debugbreak()
#endif
// =========================================== Visual Studio stuff end ======================================
#elif defined(__GNUC__)
// =========================================== GCC stuff begin ======================================
#define SLINLINE inline
#ifndef SLASSERT
#pragma message "SLASSERT undefined"
#define SLASSERT(c) (1,true)
#endif

#ifndef SLERROR
#pragma message "SLERROR undefined"
#define SLERROR(s, ...) __builtin_trap()
#endif
// =========================================== GCC stuff end ======================================
#endif


#ifdef _WIN32
#ifdef _WINDOWS_
#define INLINE_PTHREAD_SELF
SLINLINE unsigned long pthread_self() { return ::GetCurrentThreadId(); }
#else
unsigned long pthread_self();
#endif

#pragma intrinsic (_InterlockedExchangeAdd, _InterlockedCompareExchange64)
#define InterlockedExchangeAdd _InterlockedExchangeAdd

#ifdef _WIN64
#	pragma intrinsic (_InterlockedExchangeAdd64, _InterlockedCompareExchange128)
#	define InterlockedExchangeAdd64 _InterlockedExchangeAdd64
#endif

#endif

#if defined(_M_AMD64)
#define SLxInterlockedCompareExchange _InterlockedCompareExchange64
#define SLxInterlockedCompareExchange32 _InterlockedCompareExchange
#define SLxInterlockedCompareExchange2 _InterlockedCompareExchange128
#define SLxInterlockedCompareExchange64 _InterlockedCompareExchange64
#define SLxInterlockedAdd _InterlockedExchangeAdd64
#define SLxInterlockedAdd64 _InterlockedExchangeAdd64
#define SLxInterlockedAnd64 _InterlockedAnd64
#define SLxInterlockedIncrement _InterlockedIncrement64
#define SLxInterlockedDecrement _InterlockedDecrement64
#elif defined (_M_IX86)
#define SLxInterlockedCompareExchange _InterlockedCompareExchange
#define SLxInterlockedCompareExchange32 _InterlockedCompareExchange
#define SLxInterlockedCompareExchange2 _InterlockedCompareExchange64
#define SLxInterlockedCompareExchange64 _InterlockedCompareExchange64
#define SLxInterlockedAdd InterlockedExchangeAdd
#define SLxInterlockedAdd64 SLlInterlockedExchangeAdd64
#define SLxInterlockedAnd64 SLlInterlockedAnd64
#define SLxInterlockedIncrement _InterlockedIncrement
#define SLxInterlockedDecrement _InterlockedDecrement

inline int64 SLlInterlockedExchangeAdd64(volatile int64* lock, int64 adding)
{
	int64 old=*lock;
	for (;;)
	{
		int64 tmp=SLxInterlockedCompareExchange64(lock, old + adding, old);
		if (tmp==old)
			return old;
		old=tmp;
	}
}

inline int64 SLlInterlockedAnd64(volatile int64* lock, int64 mask)
{
	int64 old=*lock;
	for (;;)
	{
        int64 tmp=SLxInterlockedCompareExchange64(lock, old & mask, old);
        if (tmp==old)
			return old;
        old=tmp;
	}
}

inline int64 SLlInterlockedExchange64(volatile int64* lock, int64 newValue)
{
    int64 old=*lock;
    for (;;)
    {
        int64 tmp=SLxInterlockedCompareExchange64(lock, newValue, old);
        if (tmp==old)
			return old;
        old=tmp;
    }
}

#elif defined(__linux__)

inline void _mm_pause() { usleep( 0 ); }

#define SLxInterlockedCompareExchange(a,b,c)     __sync_val_compare_and_swap(a,c,b)
#define SLxInterlockedCompareExchange32(a,b,c)   __sync_val_compare_and_swap(a,c,b)
#define SLxInterlockedCompareExchange2(a,b,c)    __sync_val_compare_and_swap(a,c,b)
#define SLxInterlockedCompareExchange64(a,b,c)   __sync_val_compare_and_swap(a,c,b)
#define SLxInterlockedAdd  __sync_fetch_and_add
#define SLxInterlockedAdd64 __sync_fetch_and_add
#define SLxInterlockedAnd64 __sync_fetch_and_and
#define SLxInterlockedIncrement(a) __sync_fetch_and_add(a,1)
#define SLxInterlockedDecrement(a) __sync_fetch_and_sub(a,1)

inline int64 SLlInterlockedExchangeAdd64(volatile int64* lock, int64 adding)
{
    return __sync_fetch_and_add(lock, adding);
}

inline int64 SLlInterlockedAnd64(volatile int64* lock, int64 mask)
{
    return __sync_fetch_and_and(lock, mask);
}

inline int64 SLlInterlockedExchange64(volatile int64* lock, int64 newValue)
{
    return __sync_lock_test_and_set(lock, newValue);
}

SLINLINE unsigned long pthread_self() { return ::pthread_self(); }

#else
#error "Sorry, can't compile: unknown target system"
#endif

//////////////////////////////////////////////////////////////////////////
// reenterable spinlock
// many readers, writer wait and block new readers
// ACHTUNG! ACHTUNG! [read] then [write] lock in same thread leads to deadlock!!!

#define RWLOCKVALUE			spinlock::int64
#define RWLOCK              RWLOCKVALUE volatile
#define LOCK_READ_MASK      0xFFFF000000000000
#define LOCK_WRITE_MASK     0x0000FF0000000000
#define LOCK_THREAD_MASK    0x000000FFFFFFFFFF
#define LOCK_WRITE_VAL      0x0000010000000000
#define LOCK_READ_VAL       0x0001000000000000

#define CHECK_WRITE_LOCK(lock) if (!(lock & LOCK_WRITE_MASK)) SLERROR("No WRITE LOCK: %llu", lock)
#define CHECK_READ_LOCK(lock) SLASSERT(lock & LOCK_READ_MASK, "No READ LOCK: %llu", lock)
#define CHECK_LOCK(lock) if (!(lock & LOCK_READ_MASK) && !(lock & LOCK_WRITE_MASK)) SLERROR("No LOCK: %llu", lock)

#define DEADLOCK_COUNTER 3000000000LU // 30 seconds
#define DEBUG_DEADLOCK
#if defined(DEBUG_DEADLOCK) && defined(_WIN32)
#define DEADLOCKCHECK_INIT() spinlock::int64 deadlockcounter=0;
#define DEADLOCKCHECK(counter) if (deadlockcounter++>DEADLOCK_COUNTER){ SLERROR("Deadlock: %llu", counter); }
#else
#define DEADLOCKCHECK_INIT()
#define DEADLOCKCHECK(counter)
#endif


SLINLINE void lock_write(RWLOCK &lock)
{
	RWLOCKVALUE thread = pthread_self();
	RWLOCKVALUE val = lock;
	if ((val & LOCK_THREAD_MASK) == thread) // second lock same thread
	{
		SLxInterlockedAdd64(&lock, LOCK_WRITE_VAL);
		return;
	}
	thread |= LOCK_WRITE_VAL;

	DEADLOCKCHECK_INIT();
	for(;;) // initial lock
	{
		RWLOCKVALUE tmp = val & LOCK_READ_MASK;
		val = tmp | thread;
		val = SLxInterlockedCompareExchange64(&lock, val, tmp);
		if (val == tmp) // no more writers - set current thread to block new readers
			break;
		DEADLOCKCHECK(lock);
		_mm_pause();
	}

	for (;;) // if there are readers from current thread - deadlock
	{
		if (!(lock & LOCK_READ_MASK)) // all readers gone
			return;
		DEADLOCKCHECK(lock);
		_mm_pause();
	}
}

SLINLINE void lock_read(RWLOCK &lock)
{
	RWLOCKVALUE thread = pthread_self();
	RWLOCKVALUE val = lock;
	if ((val & LOCK_THREAD_MASK) == thread)
	{
		SLxInterlockedAdd64(&lock, LOCK_READ_VAL);
		return;
	}

	DEADLOCKCHECK_INIT();
	for (;;) // Initial lock
	{
		RWLOCKVALUE tmp = val & LOCK_READ_MASK;
		val = tmp + LOCK_READ_VAL;
		val = SLxInterlockedCompareExchange64(&lock, val, tmp);
		if (val == tmp) // only readers
			break;
		DEADLOCKCHECK(lock);
		_mm_pause();
	}
}

SLINLINE bool try_lock_read( RWLOCK &lock )
{
    RWLOCKVALUE thread = pthread_self();
    RWLOCKVALUE val = lock;
    if ( ( val & LOCK_THREAD_MASK ) == thread )
    {
        SLxInterlockedAdd64( &lock, LOCK_READ_VAL );
        return true;
    }

    DEADLOCKCHECK_INIT();

    RWLOCKVALUE tmp = val & LOCK_READ_MASK;
    val = tmp + LOCK_READ_VAL;
    val = SLxInterlockedCompareExchange64( &lock, val, tmp );
    return val == tmp; // only readers
}


SLINLINE bool try_lock_write(RWLOCK &lock)
{
	RWLOCKVALUE thread = pthread_self();
	RWLOCKVALUE val = lock;
	if ((val & LOCK_THREAD_MASK)==thread) // second lock
	{
		SLxInterlockedAdd64(&lock, LOCK_WRITE_VAL);
		return true;
	}
	thread |= LOCK_WRITE_VAL;
	val = SLxInterlockedCompareExchange64(&lock, thread, 0);
	return !val;
}

SLINLINE void unlock_write(RWLOCK &lock)
{
	RWLOCKVALUE tmp = lock;

	RWLOCKVALUE thread=pthread_self();
    if((tmp & LOCK_THREAD_MASK)!=thread)
        SLERROR("DEAD LOCK: %llu", lock);

	CHECK_WRITE_LOCK(tmp);

	RWLOCKVALUE val = tmp - LOCK_WRITE_VAL;
	if(!(val & LOCK_WRITE_MASK)) // last lock - reset thread
		val &= LOCK_READ_MASK;
	SLxInterlockedCompareExchange64(&lock, val, tmp);
}

SLINLINE void unlock_read(RWLOCK &lock)
{
	CHECK_READ_LOCK(lock);
	SLxInterlockedAdd64(&lock, ~(LOCK_READ_VAL-1));
}

struct auto_lock_write
{
    RWLOCK *lock;
    auto_lock_write( RWLOCK& _lock ) : lock(&_lock)
    {
		lock_write(*lock);
    }
    ~auto_lock_write()
    {
		if(lock)
			unlock_write(*lock);
    }
};

struct auto_lock_read
{
    RWLOCK *lock;
    auto_lock_read( RWLOCK& _lock ) : lock(&_lock)
    {
        lock_read(*lock);
    }
    ~auto_lock_read()
    {
        if(lock)
			unlock_read(*lock);
    }
};

#define LOCK4WRITE( ll ) spinlock::auto_lock_write UNIQIDLINE(__l4w)( ll )
#define LOCK4READ( ll ) spinlock::auto_lock_read UNIQIDLINE(__l4r)( ll )

inline void simple_lock(volatile long3264 &lock)
{
    long3264 myv = pthread_self();
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (lock == myv)
        __debugbreak();
    #endif
    for (;;)
    {
        long3264 val = SLxInterlockedCompareExchange(&lock, myv, 0);
        if (val == 0)
            break;
        #if defined(_MSC_VER) && !defined(_FINAL)
        if (val == myv)
            __debugbreak();
        #endif
        _mm_pause();
    }
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (lock != myv)
        __debugbreak();
    #endif
}

inline bool try_simple_lock(volatile long3264 &lock)
{
    long3264 myv = pthread_self();
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (lock == myv)
        __debugbreak();
    #endif
    long3264 val = SLxInterlockedCompareExchange(&lock, myv, 0);
    if (val) return false;
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (lock != myv)
        __debugbreak();
    #endif
    return true;
}


inline void simple_unlock(volatile long3264 &lock)
{
    long3264 myv = pthread_self();
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (lock != myv)
        __debugbreak();
    #endif
    long3264 val = SLxInterlockedCompareExchange(&lock, 0, myv);
    #if defined(_MSC_VER) && !defined(_FINAL)
    if (val != myv)
        __debugbreak();
    #endif
}


struct auto_simple_lock
{
    long3264 *lockvar;
    auto_simple_lock( long3264& _lock, bool) : lockvar(&_lock)
    {
        if (!try_simple_lock(*lockvar))
            lockvar = nullptr;
    }
    auto_simple_lock( long3264& _lock) : lockvar(&_lock)
    {
        simple_lock(*lockvar);
    }
    ~auto_simple_lock()
    {
        if (lockvar)
            simple_unlock(*lockvar);
    }
    bool is_locked() const { return lockvar != nullptr; }
    void lock( long3264& _lock)
    {
        if (lockvar) simple_unlock(*lockvar);
        lockvar = &_lock;
        simple_lock(*lockvar);
    }
    void unlock()
    {
        if (lockvar)
        {
            simple_unlock(*lockvar);
            lockvar = nullptr;
        }
    }
};

#define SIMPLELOCK( ll ) spinlock::auto_simple_lock UNIQIDLINE(__slock)( ll )

inline void increment( volatile long3264 &lock )
{
    SLxInterlockedIncrement( &lock );
}

inline void decrement( volatile long3264 &lock )
{
    SLxInterlockedDecrement( &lock );
}

//////////////////////////////////////////////////////////////////////////

template <typename VARTYPE> class syncvar
{
    RWLOCK mutable  m_lock;
    VARTYPE         m_var;

    class read_c
    {
        const VARTYPE *var;
        const syncvar *host;
        bool locked;
        friend class syncvar;
        read_c( const VARTYPE & _var, const syncvar *_host ): var(&_var), host(_host), locked(true)
        {
        }
        read_c & operator = (const read_c &);
        read_c(const read_c &);
    public:
        read_c( read_c &&r ): var(r.var), host(r.host), locked(r.locked)
        {
            r.locked = false;
        }
        ~read_c()
        {
            if (locked)
            {
                host->unlock_read();
            }
        }
        read_c & operator = ( read_c &&r )
        {
            if (locked)
            {
                host->unlock_read();
            }
            var = r.var;
            host = r.host;
            locked = r.locked;
            r.locked = false;
            return *this;
        }
        void unlock()
        {
            SLASSERT ( locked );
            host->unlock_read();
            locked = false;
        }

        const VARTYPE &operator()()
        {
            SLASSERT( locked );
            return *var;
        }
    };

    class write_c
    {
        VARTYPE * var;
        const syncvar *host;
        bool locked;
        friend class syncvar;
        write_c( VARTYPE * _var, const syncvar *_host ): var(_var), host(_host), locked(true)
        {
        }
        write_c & operator = (const write_c &r);
        write_c(const write_c &r);
    public:
        write_c(): var(nullptr), host(nullptr), locked(false) {}
        write_c( write_c &&r ): var(r.var), host(r.host), locked(r.locked)
        {
            r.locked = false;
        }
        write_c & operator = ( write_c &&r )
        {
            if (locked)
            {
                host->unlock_write();
            }
            var = r.var;
            host = r.host;
            locked = r.locked;
            r.locked = false;
            return *this;
        }
        ~write_c()
        {
            if (locked)
                host->unlock_write();
        }
        bool is_locked() const {return locked;}
        operator bool() const { return is_locked(); }
        void unlock()
        {
            SLASSERT( locked );
            host->unlock_write();
            locked = false;
        }


        VARTYPE &operator()()
        {
            SLASSERT( locked );
            return *var;
        }
    };


    friend class read_c;
    friend class write_c;

private:

    /**
    * Unlock variable (other threads can lock it)
    */
    void unlock_write() const
    {
        spinlock::unlock_write(m_lock);
    }
    void unlock_read() const
    {
        spinlock::unlock_read(m_lock);
    }

public:

    typedef read_c READER;
    typedef write_c WRITER;

public:


    /**
    * Constructor. Custom initialization for protected variable
    */
    syncvar(const VARTYPE &v): m_lock(0), m_var(v)
    {
    }
    /**
    * Constructor. Default initialization for protected variable
    */
    syncvar():m_lock(0)
    {
    }
    /**
    * Destructor
    */
    ~syncvar()
    {
    }

    /**
    * Sync variable for read. Other threads can also lock this variable for read.
    * Current thread will wait for lock_read if there are some other thread waits for lock_write
    */

    read_c lock_read() const
    {
        spinlock::lock_read(m_lock);
        return read_c( m_var, this );
    }

    /**
    *  Lock variable for write. no any other thread can lock for read or write.
    */
    write_c lock_write(bool trylock = false)
    {
        if (trylock)
        {
            if (try_lock_write(m_lock))
            {
                return write_c( &m_var, this );
            }
            return write_c();
        }
        spinlock::lock_write(m_lock);
        return write_c( &m_var, this );
    }

    bool locked() const {return m_lock != 0;}
};


#undef DEADLOCKCHECK_INIT
#undef DEADLOCKCHECK

template<typename T> SLINLINE bool CAS2(volatile T& dest, T& compare, const T& value)
{
#ifdef _M_AMD64
    return(SLxInterlockedCompareExchange2((int64*)&dest, ((int64*)&value)[1], ((int64*)&value)[0], ((int64*)&compare)) ? true : false);
#else
    int64 old = SLxInterlockedCompareExchange2((int64*)&dest, *((int64*)&value), *((int64*)&compare));
    if (*((int64*)&compare) == old)	return true;
    *((int64*)&compare) = old;
    return false;
#endif
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace spinlock
