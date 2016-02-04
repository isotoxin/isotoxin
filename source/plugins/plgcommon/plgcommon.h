#pragma once

#pragma warning (disable:4091) // 'typedef ' : ignored on left of '' when no variable is declared
//-V::730

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

enum logging_flags_e
{
    LFLS_CLOSE = 1,
    LFLS_ESTBLSH = 2,
    LFLS_TIMEOUT = 4,
};

extern unsigned int g_logging_flags;
extern HINSTANCE g_module;


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

void LogMessage(const char *fn, const char *caption, const char *msg);
int LoggedMessageBox(const char *text, const char *caption, UINT type);

bool Warning(const char *s, ...);
void Error(const char *s, ...);
void Log(const char *s, ...);
void MaskLog(unsigned mask, const char *s, ...);
void LogToFile(const char *fn, const char *s, ...);

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

#define STRTYPE(TCHARACTER) pstr_t<TCHARACTER>
#define MAKESTRTYPE(TCHARACTER, s, l) STRTYPE(TCHARACTER)( sptr<TCHARACTER>((s),(l)) )
#include "plghost_interface.h"
#undef MAKESTRTYPE
#undef STRTYPE

#define SS2(x) #x
#define SS(x) SS2(x)

#define ISFLAG(f,mask) (((f)&(mask))!=0)
#define SETFLAG(f,mask) (f)|=(mask)
#define UNSETFLAG(f,mask) (f)&=~(mask)
#define SETUPFLAG(f,mask,val) if(val) {SETFLAG(f,mask);} else {UNSETFLAG(f,mask);}

#ifdef _DEBUG

struct delta_time_profiler_s
{
    double notfreq;
    LARGE_INTEGER prev;
    struct entry_s
    {
        int id;
        float deltams;
    };
    entry_s *entries = nullptr;
    int index = 0;
    int n = 0;
    delta_time_profiler_s(int n);
    ~delta_time_profiler_s();
    void operator()(int id);
};

#define DELTA_TIME_PROFILER(name, n) delta_time_profiler_s name(n)
#define DELTA_TIME_CHECKPOINT(name) name(__LINE__)
#else
#define DELTA_TIME_PROFILER(n, nn) 
#define DELTA_TIME_CHECKPOINT(n) 
#endif

#include "proto_interface.h"

wstr_c get_exe_full_name();

inline wstr_c fn_change_name_ext(const wstr_c &full, const wsptr &name, const wsptr &ext)
{
    ptrdiff_t i = full.find_last_pos_of(CONSTWSTR("/\\")) + 1;
    return wstr_c(wsptr(full.cstr(), i)).append(name).append_char('.').append(ext);
}

asptr utf8clamp( const asptr &utf8, int maxbytesize );


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
                int chunksz = *(int *)(d + ptrdiff_t(offset) + 1);
                offset += 1 + chunksz;
                current_chunk_data_size = chunksz - 4;
                return current_chunk_data_size;
            }
            int skipbytes = *(int *)(d + ptrdiff_t(offset) + 1);
            offset += 1 + skipbytes;
        }
        offset = store_offset;
        current_chunk_data_size = 0;
        return 0;
    }

    template<typename T> const T& __get() { int rptr = offset; offset += sizeof(T); ASSERT(offset <= sz); return *(T *)(d + ptrdiff_t(rptr)); }
    const void *get_data(int &rsz)
    {
        rsz = __get<int>();
        int rptr = offset;
        offset += rsz;
        if (ASSERT(offset <= sz))
            return d + ptrdiff_t(rptr);
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
    void add_data( fifo_stream_c &ofifo )
    {
        size_t av = ofifo.available();
        auto &b = buf[newdata];
        size_t offset = b.size();
        b.resize(offset + av);
        ofifo.read_data( b.data() + offset, av );
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
    size_t available()  const { return buf[readbuf].size() - readpos + buf[readbuf ^ 1].size(); }
};

struct ranges_s
{
    struct range_s
    {
        u64 offset0;
        u64 offset1;
        range_s(u64 _offset0, u64 _offset1):offset0(_offset0), offset1(_offset1) {}
    };
    std::vector<range_s> ranges;

    void add(u64 _offset0, u64 _offset1)
    {
        if (ranges.size() == 0)
        {
            ranges.emplace_back(_offset0, _offset1);
            return;
        }

        int cnt = ranges.size();
        for (int i = 0; i < cnt; ++i)
        {
            range_s r = ranges[i]; // copy

            if (_offset0 > r.offset1)
                continue;

            if (_offset1 < r.offset0)
            {
                ranges.insert(ranges.begin()+i, range_s(_offset0,_offset1));
                return;
            }
            if (_offset1 == r.offset0)
            {
                r.offset0 = _offset0;
                ranges.erase(ranges.begin()+i);
                add(r.offset0, r.offset1);
                return;
            }
            if (_offset0 == r.offset1)
            {
                r.offset1 = _offset1;
                ranges.erase(ranges.begin()+i);
                add(r.offset0, r.offset1);
                return;
            }

            return;
        }

        ranges.emplace_back(_offset0, _offset1);
    }

};

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

__inline unsigned __int64 my_ntohll(unsigned __int64 Value) //-V524
{
    const unsigned __int64 Retval = _MY_WS2_32_WINSOCK_SWAP_LONGLONG(Value);
    return Retval;
}

extern "C" { extern DWORD g_cpu_caps; extern int g_cpu_cores; }

namespace cpu_detect
{
    enum cpu_caps_e
    {
        CPU_MMX = 1,
        CPU_SSE = 2,
        CPU_SSE2 = 4,
        CPU_SSE3 = 8,
        CPU_SSSE3 = 16,
        CPU_SSE41 = 32,
        CPU_AVX = 64,
        CPU_AVX2 = 128,
    };

    __inline bool CCAPS(u32 mask) { return 0 != (g_cpu_caps & mask); }
}

void img_helper_i420_to_ARGB(const byte* src_y, int src_stride_y, const byte* src_u, int src_stride_u, const byte* src_v, int src_stride_v, byte* dst_argb, int dst_stride_argb, int width, int height);


