#pragma once

namespace ts
{
#ifndef _FINAL
#define MVINIT( n ) n( #n )
#else
#define MVINIT( n ) n()
#endif

#define MULTIVAR_WRITELOCK_OPTIMIZE 1

class interlocked_c
{
    LONG m_value;

    static __forceinline int imax(int x, int y)   { return (x>y)?x:y; }
    static __forceinline int imin(int x, int y)   { return (x<y)?x:y; }

public:
    interlocked_c()
    {
        InterlockedExchange( &m_value, 0 );
    }

    LONG addlim( LONG v, LONG lim )
    {
        return InterlockedExchangeAdd( &m_value, imin(v,imax(0,lim - get())) );
    }
    LONG add( LONG v )
    {
        return InterlockedExchangeAdd( &m_value, v );
    }
    LONG get()
    {
        return InterlockedExchangeAdd( &m_value, 0 );
    }
    LONG set(LONG v)
    {
        return InterlockedExchange( &m_value, v );
    }
};

/**
*  Multi thread protection for your variable
*/
template <typename VARTYPE> class cs_lock_t
{
    CRITICAL_SECTION mutable    m_csect;
    VARTYPE                     m_var;
#ifndef _FINAL
    sstr_t<32>                m_debugtag;
#endif
    LONG mutable        m_onwrite;
    LONG mutable        m_onread;
    DWORD mutable       m_lockwthreadid;
    bool mutable        m_writer_waits;

    class read_c
    {
        const VARTYPE & var;
        const cs_lock_t *host;
        friend class cs_lock_t;
        bool locked;
        read_c(const VARTYPE & _var, const cs_lock_t *_host) : var(_var), host(_host), locked(true)
        {
        }
        read_c & operator = (const read_c &r) UNUSED;
        read_c(const read_c &r) UNUSED;
    public:
        read_c(read_c &&r) : var(r.var), host(r.host), locked(r.locked)
        {
            r.locked = false;
        }
        ~read_c()
        {
            if (locked)
                host->unlock_read();
        }
        read_c & operator = (read_c &&r)
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
            ASSERT( locked );
            host->unlock_read();
            locked = false;
        }


        const VARTYPE &operator()()
        {
            ASSERT(locked);
            return var;
        }
    };

    class write_c
    {
        friend class cs_lock_t;
        VARTYPE * var;
        const cs_lock_t *host;
        bool locked;
        write_c(VARTYPE * _var, const cs_lock_t *_host) : var(_var), host(_host), locked(true)
        {
        }
        write_c & operator = (const write_c &r) UNUSED;
        write_c(const write_c &r) UNUSED;
    public:
        write_c(): var(nullptr), host(nullptr), locked(false) {}
        write_c(const write_c &&r) : var(r.var), host(r.host), locked(r.locked)
        {
            r.locked = false;
        }
        write_c & operator = ( write_c &&r )
        {
            if (locked)
                host->unlock_write();
            var = r.var;
            host = r.host;
            locked = true;
            r.locked = false;
            return *this;
        }
        ~write_c()
        {
            if (locked)
                host->unlock_write();
        }
        bool is_locked() const {return locked;}
        void unlock()
        {
            ASSERT(locked);
            host->unlock_write();
            locked = false;
        }

        VARTYPE &operator()()
        {
            ASSERT(locked);
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
        --m_onwrite;
        ASSERT( m_onwrite >= 0 );
        if ( m_onwrite == 0 && m_onread == 0 ) m_lockwthreadid = 0;
        LeaveCriticalSection( &m_csect );
    }
    void unlock_read() const
    {
        EnterCriticalSection( &m_csect );
        --m_onread;
        ASSERT( m_onread >= 0 );
        if ( m_onwrite == 0 && m_onread == 0 ) m_lockwthreadid = 0;
        LeaveCriticalSection( &m_csect );
    }

public:

    typedef read_c READER;
    typedef write_c WRITER;

public:


    /**
    * Constructor. Custom initialization for protected variable
    */
    cs_lock_t(const VARTYPE &v): m_var(v)
    {
        InitializeCriticalSection( &m_csect );
        m_onread = 0;
        m_onwrite = 0;
        m_writer_waits = false;
        m_lockwthreadid = 0;
    }
    /**
    * Constructor. Default initialization for protected variable
    */
#ifndef _FINAL
    cs_lock_t(const char *dbgtag): m_var(), m_debugtag(dbgtag)
    {
#else
    cs_lock_t(): m_var()
    {
#endif
        InitializeCriticalSection( &m_csect );
        m_onread = 0;
        m_writer_waits = false;
        m_lockwthreadid = 0;
        m_onwrite = 0;
    }
    /**
    * Destructor
    */
    ~cs_lock_t()
    {
        DeleteCriticalSection( &m_csect );
    }

#ifndef _FINAL
    const char *dbgtag() const {return m_debugtag;}
#endif

    /** 
    * Lock variable for read. Other threads can also lock this variable for read.
    * Current thread will wait for lock_read if there are some other thread waits for lock_write
    */

    bool is_locked_write() const
    {
        EnterCriticalSection( &m_csect );
        if ( m_onwrite > 0 || m_writer_waits )
        {
            if ( (HANDLE)m_lockwthreadid == m_csect.OwningThread )
            {
                LeaveCriticalSection( &m_csect );
                return false; // not locked for current thread anyway
            }
            LeaveCriticalSection( &m_csect );
            return true;
        }

        LeaveCriticalSection( &m_csect );
        return false;
    }


    /** 
    * Lock variable for read. Other threads can also lock this variable for read.
    * Current thread will wait for lock_read if there are some other thread waits for lock_write
    */

    read_c lock_read() const
    {
        for (;;)
        {
            EnterCriticalSection( &m_csect );

            if ( m_writer_waits && (HANDLE)m_lockwthreadid != m_csect.OwningThread )
            {
                // если пейсатель ждет - это свято
                // но только если текущий лок в другом потоке. Если в этом, то не ждем, т.к. может и дэдлок случится
                LeaveCriticalSection( &m_csect );
                Sleep(0);
                continue;
            }
            if ( m_onwrite > 0 )
            {
                // ну раз мы тут, то мы полюбас в том же потоке, что и пейсатель
                ASSERT ( (HANDLE)m_lockwthreadid == m_csect.OwningThread );

                // а значит можно читать
                ++m_onread;
                LeaveCriticalSection( &m_csect );
                return read_c( m_var, this );
            }
            break;
        } // for(;;)
        if ( m_onread == 0 )
        {
            // если все чтецы одного потока, то m_lockwthreadid != 0
            m_lockwthreadid = (DWORD)m_csect.OwningThread;
        } else
        {
            if ((HANDLE)m_lockwthreadid != m_csect.OwningThread)
            {
                // но если хоть один чтец пришел из других краев, то
                m_lockwthreadid = 0;
                // дабы не вводить пейсателей в заблуждение
            }
        }
        ++m_onread;
        LeaveCriticalSection( &m_csect );
        return read_c( m_var, this );
    }

    /**
    *  Lock variable for write. no any other thread can lock for read or write.
    */

    write_c lock_write(bool trylock = false)
    {
        if (trylock)
        {
            if (FALSE==TryEnterCriticalSection( &m_csect ))
                return write_c();

        } else
            EnterCriticalSection( &m_csect );
        if ( m_onwrite > 0 )
        {
            ++m_onwrite;
            ASSERT( (HANDLE)m_lockwthreadid == m_csect.OwningThread );
            //LeaveCriticalSection( &m_csect ); // если локаем на запись - секцию не освобождаем
            return write_c( &m_var, this );
        }

        for (;;)
        {
            if ( m_onread > 0 )
            {
                // кто-то читает
                if ( (HANDLE)m_lockwthreadid == m_csect.OwningThread )
                {
                    // все читатели в том-же потоке. значит легко лочим на запись
                    ++m_onwrite;
                    //LeaveCriticalSection( &m_csect ); // если локаем на запись - секцию не освобождаем
                    return write_c( &m_var, this );
                }

                // упс. есть читател(ь)(и) в другом потоке
                // ждемс. до первой звезды

                m_writer_waits = true;
                LeaveCriticalSection( &m_csect );
                Sleep(0);
                EnterCriticalSection( &m_csect );
                continue;
            }
            // читателей больше нет
            ++m_onwrite;
            m_writer_waits = false;
            m_lockwthreadid = (DWORD)m_csect.OwningThread;
            //LeaveCriticalSection( &m_csect ); // если локаем на запись - секцию не освобождаем
            return write_c( &m_var, this );
        }

        //__assume(0);
    }
};

class critical_section_c
{
    CRITICAL_SECTION mutable    m_csect;
public:
    /**
    * Constructor. Custom initialization for protected variable
    */
    critical_section_c()
    {
        InitializeCriticalSection( &m_csect );
    }
    /**
    * Destructor
    */
    ~critical_section_c()
    {
        ASSERT( m_csect.LockCount == -1 && m_csect.RecursionCount == 0 );
        DeleteCriticalSection( &m_csect );
    }

    void lock() const
    {
        EnterCriticalSection( &m_csect );
    }

    void unlock()
    {
        LeaveCriticalSection( &m_csect );
    }
};

class cs_block_c
{
    critical_section_c *cs;
public:
    cs_block_c( critical_section_c *_cs ):cs(_cs) { cs->lock(); }
    ~cs_block_c() {if (cs) cs->unlock();}

    void unlock()
    {
        if (cs) {cs->unlock(); cs = nullptr;}
    }
};

class cs_shell_c
{
    CRITICAL_SECTION _cs;
    bool _flag;
public:
    cs_shell_c( void ) { _flag = false; InitializeCriticalSection(&_cs); }
    ~cs_shell_c() {DeleteCriticalSection(&_cs);}

    bool flag()
    {
        lock();
        bool f = _flag;
        unlock(f);
        return f;
    }

    void lock() {EnterCriticalSection(&_cs);}
    void unlock(bool f) {_flag = f; LeaveCriticalSection(&_cs);}
    void unlock_or(bool f) {_flag |= f; LeaveCriticalSection(&_cs);}
};

#pragma intrinsic (_InterlockedCompareExchange)

INLINE void simple_lock(volatile long &lock)
{
    long myv = GetCurrentThreadId();
    if (lock == myv)
        __debugbreak();
    for (;;)
    {
        long val = _InterlockedCompareExchange(&lock, myv, 0);
        if (val == 0)
            break;
        if (val == myv)
            __debugbreak();
        _mm_pause();
    }
    if (lock != myv)
        __debugbreak();
}

INLINE void simple_unlock(volatile long &lock)
{
    long myv = GetCurrentThreadId();
    if (lock != myv)
        __debugbreak();
    long val = _InterlockedCompareExchange(&lock, 0, myv);
    if (val != myv)
        __debugbreak();
}


} // namespace ts

