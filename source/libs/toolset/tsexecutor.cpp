#include "toolset.h"
#include "internal/platform.h"

namespace ts
{

    static const int f_executing = 1;
    static const int f_finished = 2;
    static const int f_canceled = 4;
    static const int f_result = 8;
    static const int f_exec_after_result = 16;
    static const int f_sleeping = 32;

task_executor_c::task_executor_c()
{
    base_thread_id = spinlock::pthread_self();
    evt = CreateEvent(nullptr,FALSE,FALSE,nullptr);
    maximum_workers = g_cpu_cores - 1;
    if (maximum_workers < 1) maximum_workers = 1;
    if (maximum_workers == 1 && g_cpu_cores == 2) maximum_workers = 2;
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
        w().tasks = 0;
        if (w().workers || w().worker_started)
        {
            w().worker_should_stop = true;
            SetEvent(evt);
            w.unlock();
            Sleep(1);
        } else
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
    while ( sleeping.try_pop( t ) )
    {
        t->resetflag( f_sleeping );
        t->done( true );
    }

    CloseHandle(evt);
}

void task_executor_c::work()
{
    auto w = sync.lock_write();
    w().worker_started = false;
    ++w().workers;
    w.unlock();

    for (;!sync.lock_read()().worker_should_stop;)
    {
        MEMT( MEMT_EXECUTOR );

        bool timeout = WAIT_TIMEOUT == WaitForSingleObject(evt, 5000);

        task_c *t;
        while (executing.try_pop(t))
        {
            t->resetflag( f_executing );

            timeout = false;
            int r = t->call_iterate();

            if ( sync.lock_read()( ).worker_should_stop && r != task_c::R_DONE )
                r = task_c::R_CANCEL;

            if (r > 0)
            {
                t->setflag( f_sleeping );
                sleeping.push(t);
            } else if (r == task_c::R_DONE)
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

                if (!t->is_flag(f_result))
                {
                    t->setflag(f_result);
                    results.push(t); // only one result
                }

                executing.push(t);
            } else if (r == task_c::R_RESULT_EXCLUSIVE)
            {
                if (!t->is_flag(f_result))
                {
                    t->setflag(f_exec_after_result | f_result);
                    results.push(t); // only one result
                } else
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

    --sync.lock_write()().workers;
}

void task_executor_c::check_worker()
{
    auto w = sync.lock_write();

    if (w().worker_must && w().workers < tmin(maximum_workers,w().tasks) && !w().worker_started)
    {
        w().worker_started = true;
        master().sys_start_thread( DELEGATE( this, work ) );
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
    ++w().tasks;
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

    int finished_tasks = 0;

    if ( spinlock::pthread_self() == base_thread_id )
    {
        while (results.try_pop(t))
        {
            t->resetflag(f_result);
            t->result();
            if (t->is_flag(f_exec_after_result))
            {
                t->resetflag( f_exec_after_result );
                t->setflag( f_executing );
                executing.push(t); // next iteration
                SetEvent(evt);
            }
        }

        while (finished.try_pop(t))
        {
            t->resetflag(f_finished);
            t->done(false);
            ++finished_tasks;
        }

        while (canceled.try_pop(t))
        {
            t->resetflag(f_canceled);
            t->done(true);
            ++finished_tasks;
        }

        tmp_pointers_t<task_c, 1> sltasks;
        int curtime = timeGetTime();
        while ( sleeping.try_pop( t ) )
        {
            t->resetflag( f_sleeping );
            if (curtime >= t->__wake_up_time)
            {
                t->setflag( f_executing );
                executing.push( t ); // next iteration
                SetEvent( evt );
            } else
            {
                sltasks.add( t );
            }
        }
        for( task_c *x : sltasks )
        {
            x->setflag( f_sleeping );
            sleeping.push(x);
        }
        if ( sync.lock_read()().reexec() )
        {
            while ( executing.try_pop( t ) )
            {
                t->resetflag( f_executing );
                add( t );
                break;
            }
        }
    }

    sync.lock_write()().tasks -= finished_tasks;
}

void task_c::setup_wakeup( int t )
{
    __wake_up_time = timeGetTime() + t;
}


} // namespace ts