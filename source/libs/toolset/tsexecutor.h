#pragma once

namespace ts
{

class task_c
{
    friend class task_executor_c;
    spinlock::syncvar<int> queueflags;
    int __pass = 0;
    void setflag( int flagmask )
    {
        auto w = queueflags.lock_write();
        if ( 0 != (w() & flagmask) )
            DEBUG_BREAK();
        w() |= flagmask;
    }
    void resetflag(int flagmask)
    {
        auto w = queueflags.lock_write();
        if (0 == (w() & flagmask))
            DEBUG_BREAK();
        RESETFLAG( w(), flagmask );
    }
    bool is_flag(int mask) const
    {
        return 0 != (queueflags.lock_read()() & mask);
    }
public:
    task_c() { queueflags.lock_write()() = 0; }
    virtual ~task_c() {}

    static const int R_DONE = -1; // job finished
    static const int R_CANCEL = -2; // job canceled
    static const int R_RESULT = -3; // call result and iterate again (simultaneously)
    static const int R_RESULT_EXCLUSIVE = -4; // call result in base thread and iterate strongly after result

    int call_iterate()
    {
        __pass = iterate( __pass );
        return __pass;
    }

    virtual int iterate(int pass) { return R_DONE; }; // can be called from any thread
    virtual void done(bool canceled) { TSDEL(this); } // called only from base thread. task should kill self
    virtual void result() {} // called only from base thread

};

class task_executor_c
{
    struct slallocator
    {
        static void *ma(size_t sz) { return MM_ALLOC(sz); }
        static void mf(void *ptr) { MM_FREE(ptr); }
    };

    spinlock::spinlock_queue_s<task_c *, slallocator> executing;
    spinlock::spinlock_queue_s<task_c *, slallocator> finished;
    spinlock::spinlock_queue_s<task_c *, slallocator> canceled;
    spinlock::spinlock_queue_s<task_c *, slallocator> results;

    struct sync_s
    {
        int tasks = 0;
        int workers = 0;
        bool worker_must = false;
        bool worker_started = false;
        bool worker_should_stop = false;
    };

    spinlock::syncvar< sync_s > sync;

#ifdef _WIN32
    static unsigned long __stdcall worker_proc(void *ap);
    void *evt = nullptr;
#endif // _WIN32

    void work();

    uint32 base_thread_id;
    int maximum_workers = 1;

    void check_worker();

public:
    task_executor_c();
    ~task_executor_c();

    void add( task_c *task ); // can be called from any thread
    void tick(); // can be called from any thread, but will do nothing, if called from non-base thread
};

} // namespace ts