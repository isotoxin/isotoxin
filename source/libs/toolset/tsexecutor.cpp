#include "toolset.h"

namespace ts
{

    static const int f_executing = 1;
    static const int f_finished = 2;
    static const int f_canceled = 4;
    static const int f_result = 8;

task_executor_c::task_executor_c()
{
    base_thread_id = GetCurrentThreadId();
    evt = CreateEvent(nullptr,FALSE,FALSE,nullptr);
}

task_executor_c::~task_executor_c()
{
    task_c *t;

    // now cancel all tasks
    while (executing.try_pop(t))
    {
        t->resetflag( f_executing );
        t->done(true);
    }

    for (;;)
    {
        auto w = sync.lock_write();
        if (w().worker_works || w().worker_started)
        {
            w().worker_should_stop = true;
            SetEvent(evt);
            Sleep(1);
        }
        else
            break;
    }

    // cancel all tasks again
    while (executing.try_pop(t))
    {
        t->resetflag(f_executing);
        t->done(true);
    }

    while (results.try_pop(t))
    {
        t->resetflag(f_result);
        t->result();
    }

    while (finished.try_pop(t))
    {
        t->resetflag(f_finished);
        t->done(false);
    }

    while (canceled.try_pop(t))
    {
        t->resetflag(f_canceled);
        t->done(true);
    }

    CloseHandle(evt);
}

DWORD WINAPI task_executor_c::worker_proc(LPVOID ap)
{
    tmpalloc_c tmp;
    ((task_executor_c *)ap)->work();
    return 0;
}

void task_executor_c::work()
{
    auto w = sync.lock_write();
    w().worker_started = false;
    w().worker_works = true;
    w.unlock();

    for (;!sync.lock_read()().worker_should_stop;)
    {
        bool timeout = WAIT_TIMEOUT == WaitForSingleObject(evt, 5000);

        task_c *t;
        while (executing.try_pop(t))
        {
            t->resetflag( f_executing );

            timeout = false;
            int r = t->call_iterate();

            if (r == task_c::R_DONE)
            {
                t->setflag( f_finished );
                finished.push(t);

            } else if (r == task_c::R_CANCEL)
            {
                t->setflag( f_canceled );
                canceled.push(t);

            } else if (r == task_c::R_RESULT)
            {
                t->setflag( f_executing );

                if (0 == (t->queueflags & f_result))
                {
                    t->setflag(f_result);
                    results.push(t); // only one result
                }

                executing.push(t);
            } else
            {
                t->setflag( f_executing );
                executing.push(t); // next iteration
            }
        }

        if (timeout)
            break;
        
    }

    sync.lock_write()().worker_works = false;
}

void task_executor_c::check_worker()
{
    auto w = sync.lock_write();

    if (w().worker_must && !w().worker_works && !w().worker_started)
    {
        w().worker_started = true;
        CloseHandle(CreateThread(nullptr, 0, worker_proc, this, 0, nullptr));
    }
    w().worker_must = false;
}

void task_executor_c::add(task_c *task)
{
    auto w = sync.lock_write();
    if (w().worker_should_stop)
    {
        w.unlock();
        task->done(true);
        return;
    }
    w().worker_must = true;
    w.unlock();

    task->setflag(f_executing);
    executing.push(task);
    SetEvent(evt);

    check_worker();
    tick();
}

void task_executor_c::tick()
{
    task_c *t;

    if ( GetCurrentThreadId() == base_thread_id )
    {
        while (results.try_pop(t))
        {
            t->resetflag(f_result);
            t->result();
        }

        while (finished.try_pop(t))
        {
            t->resetflag(f_finished);
            t->done(false);
        }

        while (canceled.try_pop(t))
        {
            t->resetflag(f_canceled);
            t->done(true);
        }
    }
}


} // namespace ts