#pragma once

namespace ts
{

class task_c
{
public:
    virtual ~task_c() {}

    static const int R_DONE = -1; // job finished
    static const int R_CANCEL = -2; // job canceled
    static const int R_RESULT = -3; // call result and iterate again


    virtual int iterate(int pass) { return R_DONE; }; // can be called from any thread
    virtual void done(bool cancleled) { TSDEL(this); } // called only from base thread. task should kill self
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
        bool worker_must = false;
        bool worker_started = false;
        bool worker_works = false;
        bool worker_should_stop = false;
    };

    spinlock::syncvar< sync_s > sync;

    static DWORD WINAPI worker_proc(LPVOID ap);
    void work();

    HANDLE evt = nullptr;
    DWORD base_thread_id;

    void check_worker();

public:
    task_executor_c();
    ~task_executor_c();

    void add( task_c *task ); // can be called from any thread
    void tick(); // can be called from any thread, but will do nothing, if called from non-base thread
};

} // namespace ts