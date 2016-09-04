#include "memspy.h"
#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <intrin.h>

// CONFIG!
//if you get a memory problem, change values of config here and recompile - it is ok 

#define MEMSPY_DISABLE 0
#define MEMSPY_CALL_STACK 1                 // store callstack for every allocation
#define MEMSPY_CALL_STACK_DEEP 4            // bigger values - slower (captain obvious)
#define MEMSPY_CALL_STACK_SKIP 3            // skip top of stack addresses
#define MEMSPY_SPY_SHOWCONTENT 10           // report
#define MEMSPY_SPY_SIZE 0                   // zero - spy every allocation, >0 - spy only allocations with given size
#define MEMSPY_SPY_LINE 0                   // zero - spy every line, >0 - spy only allocations with given line
#define MEMSPY_SPY_NUM 0
#define MEMSPY_MAX_FREE_UNALLOCATED     (1024*1024)   // in bytes - how many bytes spy keep unfree (useful to detect twice free)
#define MEMSPY_CORRUPT_CHECK_ZONE_BEGIN 32
#define MEMSPY_CORRUPT_CHECK_ZONE_END   32
#define MEMSPY_MEMLEAK_MESSAGEBOX       0
#define MEMSPY_MEMLEAK_DEBUGOUTPUT      1
#define MEMSPY_BREAK_ON_UNKNOWN_ALLOC   0

#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__) || defined(__GNUC__)
#undef MEMSPY_CALL_STACK
#define MEMSPY_CALL_STACK 0
#endif

#if MEMSPY_CALL_STACK
#pragma comment (lib, "dbghelp.lib")
#include <DbgHelp.h>
#endif

#pragma warning(disable:4127)


#define LIST_ADD(el,first,last,prev,next)       {if((last)!=nullptr) {(last)->next=el;} (el)->prev=(last); (el)->next=0;  last=(el); if(first==0) {first=(el);}}
#define LIST_DEL(el,first,last,prev,next) \
                {if((el)->prev!=0) (el)->prev->next=el->next;\
        if((el)->next!=0) (el)->next->prev=(el)->prev;\
        if((last)==(el)) last=(el)->prev;\
    if((first)==(el)) (first)=(el)->next;}


#pragma init_seg(lib)


namespace
{


#pragma intrinsic (_InterlockedCompareExchange)

volatile long memspylock = 0;
int numpool = 0;
int freesize = 0;

void spylock()
{
    long myv = GetCurrentThreadId();
    if (memspylock == myv)
        __debugbreak();
    for (;;)
    {
        long val = _InterlockedCompareExchange(&memspylock, myv, 0);
        if (val == 0)
            break;
        if (val == myv)
            __debugbreak();
        _mm_pause();
    }
    if (memspylock != myv)
        __debugbreak();
}

void spyunlock()
{
    long myv = GetCurrentThreadId();
    if (memspylock != myv)
        __debugbreak();
    long val = _InterlockedCompareExchange(&memspylock, 0, myv);
    if (val != myv)
        __debugbreak();
}

struct block_header_s
{
    const char * fn;
    unsigned int size;
    int line;
    int num;
    int typ;
    block_header_s *prev;
    block_header_s *next;
#if MEMSPY_CALL_STACK
    
    static HANDLE process;

    DWORD callstack[MEMSPY_CALL_STACK_DEEP];
    void fill_callstack(DWORD *p)
    {
        memset(callstack, 0, sizeof(callstack));

        __try {
            for (int i = 0; i<MEMSPY_CALL_STACK_SKIP; ++i)
                p = (DWORD*)p[0];

            for (int i = 0; i < sizeof(callstack)/sizeof(callstack[0]); i++, p = (DWORD*)p[0])
                callstack[i] = p[1];
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
#endif

    block_header_s *setup( const char *fn_, int line_, int typ, unsigned int sz );
    static block_header_s *ma( const char *fn, int line, int typ, size_t sz );

    static block_header_s *mr(const char *fn, int line, int typ, void *p, size_t sz);
    static void mf(void *p);
    static size_t ms(void *p)
    {
        block_header_s *me = (block_header_s *)((char *)p-sizeof(block_header_s));
        return me->size;
    }

    void *usable_space()
    {
        return this + 1;
    }

    void check_corrupt()
    {
    }

    int getinfo( char *buf, int bufsz )
    {
        if (bufsz < 2) return -1;
        char b[4096], content[512];
        auto bufcontent = [&]( const void *buf, int bufsz )-> const char *
        {

            unsigned char *d = (unsigned char *)buf;
            int show_bytes = bufsz; if (show_bytes > MEMSPY_SPY_SHOWCONTENT) show_bytes = MEMSPY_SPY_SHOWCONTENT;
            int ii = 0;
            for(;show_bytes > 0; --show_bytes)
            {
                ii += sprintf_s(content + ii, sizeof(content)-1 - ii, "%02x", (int)*d);
                ++d;
            }

            return content;
        };

#if MEMSPY_CALL_STACK
        char callstacks[2048]; callstacks[0] = 0;
        int csi = 0;
        for (int i = 0; i < sizeof(callstack) / sizeof(callstack[0]); i++)
        {
            // resolve file and line for given address

            IMAGEHLP_LINE Line = { 0 };
            Line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

            DWORD LineDisplacement = 0;
            if (!SymGetLineFromAddr(process, callstack[i] - 1, &LineDisplacement, &Line)) break;
            if (Line.FileName)
            {
                csi += sprintf_s(callstacks+csi, sizeof(callstacks)-csi-1, "%s(%i) : stack frame %i\r\n", Line.FileName, Line.LineNumber, i);
            }
        }
        int n = 1+sprintf_s(b,sizeof(b),"%s(%i): leak size: %i, num: %i, content: %s\r\n%s", fn, line, size, num, bufcontent(usable_space(), size), callstacks);
#else
        int n = 1+sprintf_s(b,sizeof(b),"%s(%i): leak size: %i, num: %i, content: %s\r\n", fn, line, size, num, bufcontent(usable_space(), size));
#endif
        if (n > bufsz) n = bufsz;
        b[n-1] = 0;
        memcpy(buf,b,n);

        return n;
    }
};

#if MEMSPY_CALL_STACK
HANDLE block_header_s::process;
#endif

enum
{
    preblock_size_x = MEMSPY_CORRUPT_CHECK_ZONE_BEGIN + sizeof(block_header_s),
    preblock_size = ((preblock_size_x + 15) & (~15)) - sizeof(block_header_s),
};


static block_header_s *first = nullptr;
static block_header_s *last = nullptr;
static block_header_s *first_free = nullptr;
static block_header_s *last_free = nullptr;

block_header_s *block_header_s::setup(const char *fn_, int line_, int typ_, unsigned int sz)
{
    fn = fn_;
    line = line_;
    size = sz;
    typ = typ_;
#if MEMSPY_BREAK_ON_UNKNOWN_ALLOC
    if ( typ == 0 )
        __debugbreak();
#endif

    spylock();
#if MEMSPY_SPY_SIZE && MEMSPY_SPY_LINE
    if (MEMSPY_SPY_SIZE == sz && MEMSPY_SPY_LINE == line_) num = numpool++;
    else num = -1;
#elif MEMSPY_SPY_SIZE
    if (MEMSPY_SPY_SIZE == sz) num = numpool++;
    else num = -1;
#elif MEMSPY_SPY_LINE
    if (MEMSPY_SPY_LINE == line_) num = numpool++;
    else num = -1;
#else
    num = numpool++;
#endif
    LIST_ADD(this, first, last, prev, next);
    spyunlock();

#if MEMSPY_SPY_NUM
    if (MEMSPY_SPY_NUM == num)
        __debugbreak();
#endif

#if MEMSPY_CALL_STACK
    // fastest way to collect callstack addresses.
    DWORD *p;
    _asm mov[p], ebp
    fill_callstack(p);
#endif


    return this;
}

block_header_s *block_header_s::ma(const char *fn, int line, int typ, size_t sz)
{
    size_t real_alloc_size = sz + sizeof(block_header_s) + preblock_size + MEMSPY_CORRUPT_CHECK_ZONE_END;
    char *p = (char *)MEMSPY_SYS_ALLOC(real_alloc_size);
    block_header_s *self = (block_header_s *)(p + preblock_size);
    if (preblock_size)
        memset(p, 0xBB, preblock_size);
#if MEMSPY_CORRUPT_CHECK_ZONE_END
    memset(p + preblock_size + sizeof(block_header_s) + sz, 0xEE, MEMSPY_CORRUPT_CHECK_ZONE_END);
#endif
    return self->setup(fn, line, typ, (unsigned int)sz);
}

block_header_s *block_header_s::mr(const char *fn, int line, int typ, void *p, size_t sz)
{
    if (p == nullptr) return ma(fn,line,typ,sz);

    block_header_s *me = (block_header_s *)((char *)p-sizeof(block_header_s));
    spylock();
    LIST_DEL( me, first, last, prev, next );
    spyunlock();
    me->check_corrupt();

    size_t real_alloc_size = sz + sizeof(block_header_s) + preblock_size + MEMSPY_CORRUPT_CHECK_ZONE_END;
    void *real_p = ((char *)p)-sizeof(block_header_s)-preblock_size;
    char *new_p = (char *)MEMSPY_SYS_RESIZE(real_p,real_alloc_size);
    if (new_p != real_p)
    {
        me = (block_header_s *)(new_p + preblock_size);
        if (preblock_size)
            memset(new_p, 0xBB, preblock_size);
    }
#if MEMSPY_CORRUPT_CHECK_ZONE_END
    memset(new_p + preblock_size + sizeof(block_header_s) + sz, 0xEE, MEMSPY_CORRUPT_CHECK_ZONE_END);
#endif
    return me->setup(fn, line, typ, (unsigned int)sz);

}

void block_header_s::mf(void *p)
{
    if (p == nullptr) return;

    block_header_s *me = (block_header_s *)((char *)p - sizeof(block_header_s));
    void *real_p = ((char *)me) - preblock_size;
    spylock();

    for (block_header_s *b = first_free; b; b = b->next)
    {
        if (b == me)
            __debugbreak(); // delete again
    }

    LIST_DEL(me, first, last, prev, next);
#if MEMSPY_MAX_FREE_UNALLOCATED
    real_p = nullptr;
    LIST_ADD(me, first_free, last_free, prev, next);
    freesize += me->size;
    while( freesize > MEMSPY_MAX_FREE_UNALLOCATED && first_free )
    {
        block_header_s *x = first_free;
        freesize -= x->size;
        LIST_DEL(x, first_free, last_free, prev, next);
        void *real_x = ((char *)x) - preblock_size;
#ifndef _DEBUG
        size_t my_size = x->size + sizeof(block_header_s) + preblock_size + MEMSPY_CORRUPT_CHECK_ZONE_END;
        memset(real_x, 0xAA, my_size);
#endif
        MEMSPY_SYS_FREE(real_x);
    }
#endif
    spyunlock();
    me->check_corrupt();

    if (real_p)
    {
        void *real_p1 = ((char *)p)-sizeof(block_header_s)-preblock_size;
#ifndef _DEBUG
        size_t my_size = me->size + sizeof(block_header_s) + preblock_size + MEMSPY_CORRUPT_CHECK_ZONE_END;
        memset(real_p1, 0xAA, my_size);
#endif
        MEMSPY_SYS_FREE(real_p1);
    }
}

}

void reset_allocnum()
{
    numpool = 0;
}


void *mspy_malloc(const char *fn, int line, int typ, size_t sz)
{
#if MEMSPY_DISABLE
    return MEMSPY_SYS_ALLOC(sz);
#else
    return block_header_s::ma(fn, line, typ, sz)->usable_space();
#endif
}

void *mspy_realloc(const char *fn, int line, int typ, void *p, size_t sz)
{
#if MEMSPY_DISABLE
    return MEMSPY_SYS_RESIZE(p, sz);
#else
    return block_header_s::mr(fn, line, typ, p, sz)->usable_space();
#endif
}

void mspy_free(void *p)
{
#if MEMSPY_DISABLE
    MEMSPY_SYS_FREE(p);
#else
    block_header_s::mf(p);
#endif
}

size_t mspy_size(void *p)
{
#if MEMSPY_DISABLE
    return MEMSPY_SYS_SIZE(p);
#else
    return block_header_s::ms(p);
#endif
}

bool mspy_getallocated_info( memcb *cb, void *prm )
{
    int curl = 0;
    spylock();
    if ( first == nullptr )
    {
        spyunlock();
        return false;
    }

    int i = 0;

    for(;;)
    {
        int n = 0;
        for ( block_header_s *b = first; b; b = b->next )
        {
            if ( i == n )
            {
                int typ = b->typ;
                int num = b->num;
                size_t sz = b->size;
                spyunlock();
                cb( typ, num, sz, prm );
                spylock();
                ++i;
                n = -1;
                break;
            }
            ++n;
        }
        if (i>=n && n >= 0)
            break;
    }

    spyunlock();

    return true;

}

bool mspy_getallocated_info( char *buf, int bufsz )
{
    int curl = 0;
    spylock();
    if (first == nullptr)
    {
        spyunlock();
        return false;
    }

    const char *t = "   -================[MEMORY LEAKS]================-\r\n";
    int tl = (unsigned int)strlen(t);

    if (tl < bufsz)
    {
        memcpy(buf, t, tl + 1);
        buf += tl;
        bufsz -= tl;
    }

#if MEMSPY_CALL_STACK
    SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
    block_header_s::process = GetCurrentProcess();
    SymInitialize(block_header_s::process, NULL, TRUE);
#endif

    for( block_header_s *b = first; b; b=b->next )
    {
        int cs = b->getinfo(buf+curl,bufsz-curl);
        if (cs < 0) break;
        curl += cs;
    }

    spyunlock();

    return true;
}

struct memleak_fin_s
{
    ~memleak_fin_s()
    {
        char buf[2048];
#if MEMSPY_MEMLEAK_MESSAGEBOX
        memset(buf, 0, sizeof(buf));
        if (mspy_getallocated_info(buf, 2047))
        {
            MessageBoxA(nullptr, buf, "memory leak detected!!!", MB_OK|MB_ICONEXCLAMATION);
        }
#endif
#if MEMSPY_MEMLEAK_DEBUGOUTPUT
        spylock();
        if (first == nullptr)
        {
            spyunlock();
            return;
        }

#if MEMSPY_CALL_STACK
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
        block_header_s::process = GetCurrentProcess();
        SymInitialize(block_header_s::process, NULL, TRUE);
#endif
        OutputDebugStringA("   -================[MEMORY LEAKS]================-\r\n");

        for (block_header_s *b = first; b; b = b->next)
        {
            if (b->getinfo(buf, 2048) < 0) break;
            OutputDebugStringA(buf);
        }

        spyunlock();
#endif
        for (;first_free; )
        {
            block_header_s *x = first_free;
            LIST_DEL(x, first_free, last_free, prev, next);
            void *real_x = ((char *)x) - preblock_size;
            MEMSPY_SYS_FREE(real_x);
        }
    }

} mlmb;


