#pragma once

// eah, simple threads emulation (just mutex), compatible with pthread lib

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>

#ifndef pthread_self
#define pthread_self GetCurrentThreadId
#endif

#pragma intrinsic (_InterlockedCompareExchange)
#endif

typedef long lock_t;

typedef struct
{
    lock_t lock;

} pthread_mutex_t;

typedef struct
{
    int dummy;

} pthread_mutexattr_t;

enum
{
    PTHREAD_MUTEX_RECURSIVE,
};

int inline pthread_mutexattr_init (pthread_mutexattr_t * attr) {return 0;}
int inline pthread_mutexattr_settype( pthread_mutexattr_t *attr, int kind ) { return 0; }
void inline pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {}

int inline pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t *attr)
{
    mutex->lock = 0;
    return 0;
}


int inline pthread_mutex_trylock(pthread_mutex_t * mutex)
{
    volatile lock_t *l = &mutex->lock;
    lock_t thread = 0xFFFFFF & pthread_self();
    lock_t val = *l;
    if ((val & 0xFFFFFF) == thread)
    {
        *l += 0x1000000; // simple increment due mutex locked by current thread
        return 0;
    }
    thread |= 0x1000000;

    lock_t tmp = 0;
    val = thread;
    val = _InterlockedCompareExchange(l, val, tmp);
    return (val == tmp) ? 0 : 1;
}

int inline pthread_mutex_lock (pthread_mutex_t * mutex)
{
    volatile lock_t *l = &mutex->lock;
    lock_t thread = 0xFFFFFF & pthread_self();
    lock_t val = *l;
    if ((val & 0xFFFFFF) == thread)
    {
        *l += 0x1000000; // simple increment due mutex locked by current thread
        return 0;
    }
    thread |= 0x1000000;

    for (;;)
    {
        lock_t tmp = 0;
        val = thread;
        val = _InterlockedCompareExchange(l, val, tmp);
        if (val == tmp)
            break;
        _mm_pause();
    }


    return 0;
}

int inline pthread_mutex_unlock (pthread_mutex_t * mutex)
{
    volatile lock_t *l = &mutex->lock;
    lock_t tmp = *l;

#ifdef _DEBUG
    lock_t thread = 0xFFFFFF & pthread_self();
    if ((*l & 0xFFFFFF) != thread)
        __debugbreak(); // fail fail fail
#endif // _DEBUG

    lock_t val = tmp - 0x1000000;
    if (!(val & 0xFF000000))
        val = 0;
    _InterlockedCompareExchange(l, val, tmp);

    return 0;
}

int inline pthread_mutex_destroy (pthread_mutex_t * mutex)
{
    return 0;
}
