#pragma once

#define SLASSERT ASSERTO
#define SLERROR ERROR
#include "spinlock/spinlock.h"
#include "spinlock/queue.h"

namespace ts
{

#pragma intrinsic (_InterlockedCompareExchange)

INLINE void simple_lock(volatile long &lock)
{
    long myv = spinlock::pthread_self();
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
    long myv = spinlock::pthread_self();
    if (lock != myv)
        __debugbreak();
    long val = _InterlockedCompareExchange(&lock, 0, myv);
    if (val != myv)
        __debugbreak();
}


} // namespace ts

