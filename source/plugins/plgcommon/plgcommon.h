#pragma once

#include <windows.h>
#include <malloc.h>
#include <locale.h>
#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <vector>
#include <map>
#include <unordered_map>

typedef unsigned long u32;
typedef unsigned char byte;

#define LIST_ADD(el,first,last,prev,next)       {if((last)!=nullptr) {(last)->next=el;} (el)->prev=(last); (el)->next=0;  last=(el); if(first==0) {first=(el);}}
#define LIST_DEL(el,first,last,prev,next) \
        {if((el)->prev!=0) (el)->prev->next=el->next;\
        if((el)->next!=0) (el)->next->prev=(el)->prev;\
        if((last)==(el)) last=(el)->prev;\
    if((first)==(el)) (first)=(el)->next;}

extern "C"
{
    void* dlmalloc(size_t);
    void  dlfree(void*);
    void* dlrealloc(void*, size_t);
    void* dlcalloc(size_t, size_t);
    size_t dlmalloc_usable_size(void*);
};

#ifdef _DEBUG_OPTIMIZED
#define SMART_DEBUG_BREAK (IsDebuggerPresent() ? __debugbreak(), false : false)
#else
#define SMART_DEBUG_BREAK __debugbreak()
#endif

#ifdef _FINAL
#define USELIB(ln) comment(lib, #ln ".lib")
#elif defined _DEBUG_OPTIMIZED
#define USELIB(ln) comment(lib, #ln "do.lib")
#else
#define USELIB(ln) comment(lib, #ln "d.lib")
#endif

__forceinline time_t now()
{
    time_t t;
    _time64(&t);
    return t;
}


bool AssertFailed(const char *file, int line, const char *s, ...);
inline bool AssertFailed(const char *file, int line) {return AssertFailed(file, line, "");}

void LogMessage(const char *caption, const char *msg);
int LoggedMessageBox(const char *text, const char *caption, UINT type);

bool Warning(const char *s, ...);
void Error(const char *s, ...);
void Log(const char *s, ...);

#define NOWARNING(n,...) __pragma(warning(push)) __pragma(warning(disable:n)) __VA_ARGS__ __pragma(warning(pop))

#ifdef _DEBUG
#define ASSERT(expr,...) NOWARNING(4800, ((expr) || (AssertFailed(__FILE__, __LINE__, __VA_ARGS__) ? SMART_DEBUG_BREAK, false : false)))
#define CHECK ASSERT
#define WARNING(...) (Warning(__VA_ARGS__) ? SMART_DEBUG_BREAK, false : false)
#else
#define ASSERT(expr,...) (1,true)
#define CHECK(expr,...) expr
#define WARNING(...) 
#endif

#ifdef ERROR
#undef ERROR
#endif
#define ERROR(...) Error(__VA_ARGS__)


#define SLASSERT ASSERT
#define SLERROR ERROR


#include "md5.h"
#include "spinlock/queue.h"
#include "strings.h"

#if defined _DEBUG || defined _CRASH_HANDLER
#include "excpn.h"
#define UNSTABLE_CODE_PROLOG __try {
#define UNSTABLE_CODE_EPILOG } __except(EXCEPTIONFILTER()){}
#else
#define UNSTABLE_CODE_PROLOG ;
#define UNSTABLE_CODE_EPILOG ;
#endif

template<typename T> inline sptr<T> _to_char_or_not_to_char(const char * sa, const wchar_t * sw, int len);
template<> inline asptr _to_char_or_not_to_char<char>(const char * sa, const wchar_t *, int len) { return asptr(sa, len); }
template<> inline wsptr _to_char_or_not_to_char<wchar_t>(const char *, const wchar_t * sw, int len) { return wsptr(sw, len); }
#define CONSTSTR( tc, s ) _to_char_or_not_to_char<tc>( s, JOINMACRO1(L,s), sizeof(s)-1 )
#define CONSTASTR( s ) asptr( s, sizeof(s)-1 )
#define CONSTWSTR( s ) wsptr( JOINMACRO1(L,s), sizeof(s)-1 )

#define STRTYPE(TCHARACTER) pstr_t<TCHARACTER>
#define MAKESTRTYPE(TCHARACTER, s, l) STRTYPE(TCHARACTER)( sptr<TCHARACTER>((s),(l)) )
#include "plghost_interface.h"
#undef MAKESTRTYPE
#undef STRTYPE

#include "proto_interface.h"


template <class T> class shared_ptr
{
    T *object;

    void unconnect()
    {
        if (object) T::decRef(object);
    }

    void connect(T *p)
    {
        object = p;
        if (object) object->addRef();
    }

public:
    shared_ptr() :object(nullptr) {}
    shared_ptr(T *p) { connect(p); }
    shared_ptr(const shared_ptr &p) { connect(p.object); }
    shared_ptr(shared_ptr &&p) :object(p.object) { p.object = nullptr; }
    shared_ptr operator=(shared_ptr &&p) { SWAP(object, p.object); return *this; }

    shared_ptr &operator=(T *p)
    {
        if (p) p->addRef();
        unconnect();
        object = p;
        return *this;
    }
    shared_ptr &operator=(const shared_ptr &p)
    {
        return *this = p.object;
    }

    ~shared_ptr() { unconnect(); }

    void swap(shared_ptr &p) { SWAP(*this, p); }

    operator T *() const  { return object; }
    T *operator->() const { return object; }

    T *get() { return object; }
    const T *get() const { return object; }
private:
};

class shared_object
{
    int refCount;

    shared_object(const shared_object &);
    void operator=(const shared_object &);

public:
    shared_object() : refCount(0) {}

    bool isMultiRef() const { return refCount > 1; }
    void addRef() { refCount++; }
    template <class T> static void decRef(T *object)
    {
        ASSERT(object->refCount > 0);
        if (--object->refCount == 0) delete object;
    }
};


struct loader
{
    const byte *d;
    int sz;
    int offset = 0;
    int current_chunk = -1;
    int current_chunk_data_size = 0;
    loader(const void *dd, int sz) :d((const byte *)dd), sz(sz) {}

    int operator()(byte chunkid)
    {
        int store_offset = offset;
        for (; offset < sz;)
        {
            if (d[offset] == chunkid)
            {
                current_chunk = offset + 5;
                int chunksz = *(int *)(d + offset + 1);
                offset += 1 + chunksz;
                current_chunk_data_size = chunksz - 4;
                return current_chunk_data_size;
            }
            int skipbytes = *(int *)(d + offset + 1);
            offset += 1 + skipbytes;
        }
        offset = store_offset;
        current_chunk_data_size = 0;
        return 0;
    }

    template<typename T> const T& __get() { int rptr = offset; offset += sizeof(T); ASSERT(offset <= sz); return *(T *)(d + rptr); }
    const void *get_data(int &rsz)
    {
        rsz = __get<int>();
        int rptr = offset;
        offset += rsz;
        if (ASSERT(offset <= sz))
            return d + rptr;
        return nullptr;
    }
    int read_list_size() { return __get<int>(); }

    const byte *chunkdata() const { return d + current_chunk; }
    int get_byte() const { return current_chunk_data_size >= 1 ? (*(byte *)chunkdata()) : 0; }
    int get_int() const { return current_chunk_data_size >= sizeof(int) ? (*(int *)chunkdata()) : 0; }
    u64 get_u64() const { return current_chunk_data_size >= sizeof(u64) ? (*(u64 *)chunkdata()) : 0; }
    asptr get_astr() const
    {
        if (current_chunk_data_size < 2) return asptr();
        int slen = *(unsigned short *)chunkdata();
        if ((current_chunk_data_size - 2) < slen) return asptr();
        return asptr((const char *)chunkdata() + 2, slen);
    }
};

struct savebuffer : public std::vector < char >
{
    template<typename T> T &add()
    {
        size_t offset = size();
        resize(offset + sizeof(T));
        return *(T *)(data() + offset);
    }
    void add(const void *d, int sz)
    {
        size_t offset = size();
        resize(offset + sz);
        memcpy(data() + offset, d, sz);
    }
    void add(const asptr& s)
    {
        add<unsigned short>() = (unsigned short)s.l;
        add(s.s, s.l);
    }

};
struct bytes
{
    const void *data;
    int datasize;
    bytes(const void *data, int datasize) :data(data), datasize(datasize) {}
};
template<typename T> struct serlist
{
    T *first;
    serlist(T *first) :first(first) {}
};
struct chunk
{
    savebuffer &b;
    size_t sizeoffset;
    chunk &operator=(const chunk&) = delete;
    chunk(savebuffer &b, byte chunkid) :b(b)
    {
        b.add<byte>() = chunkid;
        sizeoffset = b.size();
        b.add<int>();
    }
    ~chunk()
    {
        *(int *)(b.data() + sizeoffset) = b.size() - sizeoffset;
    }

    void *alloc(int sz)
    {
        b.add<int>() = sz;
        size_t offset = b.size();
        b.resize( offset + sz );
        return b.data() + offset;
    }

    void operator <<(byte v) { b.add<byte>() = v; }
    void operator <<(int v) { b.add<int>() = v; }
    void operator <<(u64 v) { b.add<u64>() = v; }
    void operator <<(const str_c& str) { b.add(str.as_sptr()); }
    void operator <<(const bytes&byts) { b.add<int>() = byts.datasize; b.add(byts.data, byts.datasize); }
    template<typename T> void operator <<(const serlist<T>& sl)
    {
        size_t numoffset = b.size();
        b.add<int>();
        int n = 0;
        for (T *el = sl.first; el; el = el->next)
        {
            size_t preoffset = b.size();
            *this << *el;
            if (b.size() > preoffset)
                ++n;
        }
        *(int *)(b.data() + numoffset) = n;
    }
};


class fifo_stream_c
{
    std::vector<byte> buf[2];
    int readpos = 0;
    
    unsigned readbuf : 1;
    unsigned newdata : 1;

public:

    fifo_stream_c()
    {
        readbuf = 0;
        newdata = 0;
    }

    void add_data(const void *d, int s)
    {
        auto &b = buf[newdata];

        size_t offset = b.size();
        b.resize(offset + s);
        memcpy(b.data() + offset, d, s);

    }
    void get_data(int offset, byte *dest, int size); // copy data and leave it in buffer
    int read_data(byte *dest, int size); // dest can be null
    void clear()
    {
        buf[0].clear();
        buf[1].clear();
        readbuf = 0;
        newdata = 0;
        readpos = 0;
    }
    int available()  const { return buf[readbuf].size() - readpos + buf[readbuf ^ 1].size(); }
};

#if 0
class critical_section_c
{
    CRITICAL_SECTION mutable    m_csect;
public:
    /**
    * Constructor. Custom initialization for protected variable
    */
    critical_section_c(void)
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

    void lock(void) const
    {
        EnterCriticalSection( &m_csect );
    }

    void unlock(void)
    {
        LeaveCriticalSection( &m_csect );
    }
};

class cs_block_c
{
    critical_section_c *cs;
public:
    cs_block_c(critical_section_c *_cs) :cs(_cs) { cs->lock(); }
    ~cs_block_c() { if (cs) cs->unlock(); }

    void unlock(void)
    {
        if (cs) { cs->unlock(); cs = nullptr; }
    }
};
#endif

template< typename VEC, typename EL > int findIndex(const VEC &vec, const EL & el)
{
    size_t cnt = vec.size();
    for (size_t index = 0; index < cnt; ++index)
    {
        if (vec[index] == el) return (int)index;
    }
    return -1;
}

template< typename VEC, typename EL > bool isPresent(const VEC &vec, const EL & el)
{
    for (VEC::const_iterator it = vec.cbegin(); it != vec.cend(); ++it)
    {
        if (*it == el) return true;
    }
    return false;
}

template< typename VEC, typename EL > bool addIfNotPresent(VEC &vec, const EL & el) // true - added
{
    for (VEC::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el) return false;
    }
    vec.push_back(el);
    return true;
}

template< typename VEC, typename EL > bool removeIfPresent(VEC &vec, const EL & el) // true - found and removed
{
    for (VEC::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el)
        {
            vec.erase(it);
            return true;
        }
    }
    return false;
}

template< typename VEC, typename EL > bool removeIfPresentFast(VEC &vec, const EL & el) // true - found and removed
{
    for (VEC::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el)
        {
            *it = vec.back();
            vec.resize(vec.size() - 1);
            return true;
        }
    }
    return false;
}

#define _MY_WS2_32_WINSOCK_SWAP_LONGLONG(l)            \
            ( ( ((l) >> 56) & 0x00000000000000FFLL ) |       \
              ( ((l) >> 40) & 0x000000000000FF00LL ) |       \
              ( ((l) >> 24) & 0x0000000000FF0000LL ) |       \
              ( ((l) >>  8) & 0x00000000FF000000LL ) |       \
              ( ((l) <<  8) & 0x000000FF00000000LL ) |       \
              ( ((l) << 24) & 0x0000FF0000000000LL ) |       \
              ( ((l) << 40) & 0x00FF000000000000LL ) |       \
              ( ((l) << 56) & 0xFF00000000000000LL ) )


__inline unsigned __int64 my_htonll(unsigned __int64 Value)
{
    const unsigned __int64 Retval = _MY_WS2_32_WINSOCK_SWAP_LONGLONG(Value);
    return Retval;
}

__inline unsigned __int64 my_ntohll(unsigned __int64 Value)
{
    const unsigned __int64 Retval = _MY_WS2_32_WINSOCK_SWAP_LONGLONG(Value);
    return Retval;
}