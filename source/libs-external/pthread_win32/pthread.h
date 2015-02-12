#pragma once

// eah, simple win32 threads, compartible with pthread lib

#include <windows.h>

#define pthread_self GetCurrentThreadId

#pragma intrinsic (_InterlockedCompareExchange64)
typedef __int64 lock_t;

typedef struct
{
    lock_t lock;

} pthread_mutex_t;

#define pthread_w32_lock_write_mask  0x0000FF0000000000ll
#define pthread_w32_lock_thread_mask 0x000000FFFFFFFFFFll
#define pthread_w32_lock_write_val   0x0000010000000000ll

typedef struct
{
    int __dummy;

} pthread_mutexattr_t;

enum
{
    PTHREAD_MUTEX_RECURSIVE,
};

int _inline pthread_mutexattr_init (pthread_mutexattr_t * attr) {return 0;}
int _inline pthread_mutexattr_settype( pthread_mutexattr_t *attr, int kind ) { return 0; }
void _inline pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {}

int _inline pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t *attr)
{
    mutex->lock = 0;
    return 0;
}

int _inline pthread_mutex_lock (pthread_mutex_t * mutex)
{
    volatile lock_t *l = &mutex->lock;
    lock_t thread = GetCurrentThreadId();
    lock_t val = *l;
    if ((val & pthread_w32_lock_thread_mask) == thread)
    {
        *l += pthread_w32_lock_write_val; // simple increment due mutex locked by current thread
        return 0;
    }
    thread |= pthread_w32_lock_write_val;

    for (;;)
    {
        lock_t tmp = 0;
        val = thread;
        val = _InterlockedCompareExchange64(l, val, tmp);
        if (val == tmp)
            break;
        _mm_pause();
    }


    return 0;
}

int _inline pthread_mutex_unlock (pthread_mutex_t * mutex)
{
    volatile lock_t *l = &mutex->lock;
    lock_t tmp = *l;
    lock_t val = tmp - pthread_w32_lock_write_val;
    if (!(val & pthread_w32_lock_write_mask))
        val = 0;
    _InterlockedCompareExchange64(l, val, tmp);

    return 0;
}

int _inline pthread_mutex_destroy (pthread_mutex_t * mutex)
{
    return 0;
}
