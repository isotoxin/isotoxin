/*
  Just include this file in your project to use dlmalloc anywhere
  also, don't forget to activate "Ignore Specific Default Libraries" "libcmt" in linker options (Configuration Properties->Linker->Input)
  to obtain libcmt_nomem.lib see makelib\vs2013\readme.txt

  bonus: spin lock instead of CriticalSection for crt syncronizations

example (in any cpp file of your project):

#ifdef _USE_DLMALLOC_ANYWHERE
#include "crtfunc.h"
#endif

*/
#pragma comment(lib, "libcmt_nomem.lib")
#pragma warning (push)
#pragma warning (disable:4565) //redefinition; the symbol was previously declared with __declspec(restrict)

namespace
{
    namespace spinlock_crt_nomem
    {
        const int TOTAL_CRT_LOCKS = 36; // for VS 2013 total number of crt locks is 36
        typedef __int64 lock_t;

        enum : lock_t
        {
            lock_write_mask  = 0x0000FF0000000000ull,
            lock_thread_mask = 0x000000FFFFFFFFFFull,
            lock_write_val   = 0x0000010000000000ull,
        };


        static lock_t crt_locks[ TOTAL_CRT_LOCKS ];

        #pragma intrinsic (_InterlockedCompareExchange64)
    
        inline lock_t SpecialInterlockedExchangeAdd64(volatile lock_t* lock, lock_t adding)
        {
            lock_t old = *lock;
            for (;;)
            {
                lock_t tmp = _InterlockedCompareExchange64(lock, old + adding, old);
                if (tmp == old)
                    return old;
                old = tmp;
            }
        }

    }
}

extern "C" {

    void* dlmalloc(size_t);
    void  dlfree(void*);
    void* dlrealloc(void*, size_t);
    void* dlcalloc(size_t, size_t);
    void* dlcalloc(size_t, size_t);
    size_t dlmalloc_usable_size(void*);

    int __cdecl _heap_init(int /*mtflag*/)
    {
        return 1;
    }
    void __cdecl _heap_term()
    {
    }

    void __cdecl _initp_heap_handler(void * /*enull*/)
    {
    }

    int __cdecl _mtinitlocknum( int locknum )
    {
        if (locknum >= spinlock_crt_nomem::TOTAL_CRT_LOCKS)
        {
            MessageBox(0, L"_mtinitlocknum check fail", 0, 0);
            exit(0);
        }

        return TRUE;
    }

    int __cdecl _mtinitlocks()
    {
        memset( spinlock_crt_nomem::crt_locks, 0, sizeof(spinlock_crt_nomem::crt_locks) );
        return TRUE;
    }
    void __cdecl _mtdeletelocks()
    {
    }
    void __cdecl _lock( int locknum )
    {
        if (locknum >= spinlock_crt_nomem::TOTAL_CRT_LOCKS)
        {
            MessageBox(0, L"_lock check fail", 0, 0);
            exit(0);
        }
        volatile spinlock_crt_nomem::lock_t & l = *(volatile spinlock_crt_nomem::lock_t *)(spinlock_crt_nomem::crt_locks+locknum);
        spinlock_crt_nomem::lock_t thread = GetCurrentThreadId();
        spinlock_crt_nomem::lock_t val = l;
        if ((val & spinlock_crt_nomem::lock_thread_mask) == thread)
        {
            spinlock_crt_nomem::SpecialInterlockedExchangeAdd64(&l, spinlock_crt_nomem::lock_write_val);
            return;
        }
        thread |= spinlock_crt_nomem::lock_write_val;

        for (;;)
        {
            spinlock_crt_nomem::lock_t tmp = 0;
            val = thread;
            val = _InterlockedCompareExchange64(&l, val, tmp);
            if (val == tmp)
                break;
            _mm_pause();
        }

    }
    void __cdecl _unlock( int locknum )
    {
        if (locknum >= spinlock_crt_nomem::TOTAL_CRT_LOCKS)
        {
            MessageBox(0, L"_unlock check fail", 0, 0);
            exit(0);
        }

        volatile spinlock_crt_nomem::lock_t & l = *(volatile spinlock_crt_nomem::lock_t *)(spinlock_crt_nomem::crt_locks + locknum);
        spinlock_crt_nomem::lock_t tmp = l;
        spinlock_crt_nomem::lock_t val = tmp - spinlock_crt_nomem::lock_write_val;
        if (!(val & spinlock_crt_nomem::lock_write_mask))
            val = 0;
        _InterlockedCompareExchange64(&l, val, tmp);

    }


#undef malloc
void * __cdecl malloc (size_t nSize)
{
    return dlmalloc(nSize);
}
void * __cdecl malloc_crt(size_t nSize)
{
    return dlmalloc(nSize);
}
#undef _malloc_crt
void * __cdecl _malloc_crt(size_t nSize)
{
    return dlmalloc(nSize);
}

#undef calloc
void * __cdecl calloc(size_t count, size_t size)
{
    return dlcalloc(count, size);
}

#undef _calloc_crt
void * __cdecl _calloc_crt(size_t count, size_t size)
{
    return dlcalloc(count, size);
}

void * __cdecl calloc_crt(size_t count, size_t size)
{
    return dlcalloc(count, size);
}

#undef _msize
size_t __cdecl _msize(void * pUserData)
{
    return dlmalloc_usable_size(pUserData);
}

#undef realloc
void * __cdecl realloc(void *p, size_t nSize)
{
    return dlrealloc(p, nSize);
}

#undef _realloc_crt
void * __cdecl _realloc_crt(void *p, size_t nSize)
{
    return dlrealloc(p, nSize);
}

#undef _recalloc_crt
void * __cdecl _recalloc_crt(void * memblock, size_t count, size_t size)
{
    void * retp = nullptr;
    size_t  size_orig = 0, old_size = 0;

    size_orig = size * count;
    if (memblock != nullptr)
        old_size = dlmalloc_usable_size(memblock);
    retp = dlrealloc(memblock, size_orig);
    if (retp != nullptr && old_size < size_orig)
    {
        memset((char*)retp + old_size, 0, size_orig - old_size);
    }
    return retp;
}

#undef free
void __cdecl free(void * pUserData)
{
    dlfree(pUserData);
}

} // extern "C"

#undef delete
void __cdecl operator delete(void *p)
{
    dlfree(p);
}

#undef new
void * __cdecl operator new(size_t sz)
{
    return dlmalloc(sz);
}
void _cdecl operator delete[](void* p)
{
    dlfree(p);
}

#pragma warning (pop)