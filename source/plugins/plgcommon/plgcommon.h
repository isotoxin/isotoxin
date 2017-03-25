#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif // _WIN32

#include "std_lite.h"

#ifdef _WIN32

#pragma warning (disable:4091) // 'typedef ' : ignored on left of '' when no variable is declared
//-V::730

#include <windows.h>
#include <Mmsystem.h>

#include <malloc.h>
#include <locale.h>
#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#endif // _WIN32

#if defined (_M_AMD64) || defined (_M_X64) || defined (WIN64) || defined(__LP64__)
#define MODE64
#define ARCHBITS 64
#define ASMFUNC(f) f##_64
#else
#define ARCHBITS 32
#define ASMFUNC(f) f
#endif

#if defined __linux__
#define _NIX
#endif

#ifdef _WIN32
#define WINDOWS_ONLY
#else
#define WINDOWS_ONLY __error
#endif

enum logging_flags_e
{
    LFLS_CLOSE = 1,
    LFLS_ESTBLSH = 2,
    LFLS_TIMEOUT = 4,
};

extern unsigned int g_logging_flags;
extern unsigned int g_telemetry_flags;
extern HINSTANCE g_module;

#define IS_TLM( t ) (0!=(g_telemetry_flags & ( 1 << t )))

typedef unsigned long u32;
typedef unsigned char byte;
typedef ptrdiff_t aint;
typedef long i32;

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

#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__)
#define LIBSUFFIX "64.lib"
#else
#define LIBSUFFIX ".lib"
#endif


#ifdef _FINAL
#define USELIB(ln) comment(lib, #ln LIBSUFFIX)
#elif defined _DEBUG_OPTIMIZED
#define USELIB(ln) comment(lib, #ln "do" LIBSUFFIX)
#else
#define USELIB(ln) comment(lib, #ln "d" LIBSUFFIX)
#endif

__forceinline time_t now()
{
    time_t t;
    _time64(&t);
    return t;
}

__forceinline int time_ms()
{
    return (int)timeGetTime();
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

template <typename T, size_t N> char( &__ARRAYSIZEHELPER( T( &array )[ N ] ) )[ N ];
#define ARRAY_SIZE( a ) (sizeof(__ARRAYSIZEHELPER(a)))

#include "spinlock/queue.h"
#include "strings.h"

#if defined _DEBUG || defined _CRASH_HANDLER
#include "excpn.h"
#ifdef _MSC_VER
#define UNSTABLE_CODE_PROLOG __try {
#define UNSTABLE_CODE_EPILOG } __except(EXCEPTIONFILTER()){}
#define LENDIAN(x) (x)
#endif // _MSC_VER
#ifdef __GNUC__
#define UNSTABLE_CODE_PROLOG try {
#define UNSTABLE_CODE_EPILOG } catch(...){}
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LENDIAN(x) (x)
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LENDIAN(x) not_yet_implemented
#endif
#endif // __GNUC__
#else
#define UNSTABLE_CODE_PROLOG ;
#define UNSTABLE_CODE_EPILOG ;
#endif

#define STRTYPE(TCHARACTER) std::pstr_t<TCHARACTER>
typedef std::widechar WIDECHAR;
#define MAKESTRTYPE(TCHARACTER, s, l) STRTYPE(TCHARACTER)( std::sptr<TCHARACTER>((s),(l)) )
#include "plghost_interface.h"
#undef LENDIAN
#undef MAKESTRTYPE
#undef STRTYPE

#define SS2(x) #x
#define SS(x) SS2(x)

#define ISFLAG(f,mask) (((f)&(mask))!=0)
#define SETFLAG(f,mask) (f)|=(mask)
#define UNSETFLAG(f,mask) (f)&=~(mask)
#define SETUPFLAG(f,mask,val) if(val) {SETFLAG(f,mask);} else {UNSETFLAG(f,mask);}

#define SETBIT(x) (static_cast<size_t>(1)<<(x))

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

std::wstr_c get_exe_full_name();

inline std::wstr_c fn_change_name_ext(const std::wstr_c &full, const std::wsptr &name, const std::wsptr &ext)
{
    int i = full.find_last_pos_of(STD_WSTR("/\\")) + 1;
    return std::wstr_c(std::wsptr(full.cstr(), i)).append(name).append_char('.').append(ext);
}

std::asptr utf8clamp( const std::asptr &utf8, int maxbytesize );


template <class T> class shared_ptr
{
    T *object;

    void unconnect()
    {
        if (object) T::dec_ref(object);
    }

    void connect(T *p)
    {
        object = p;
        if (object) object->add_ref();
    }

public:
    shared_ptr() :object(nullptr) {}
    shared_ptr(T *p) { connect(p); }
    shared_ptr(const shared_ptr &p) { connect(p.object); }
    shared_ptr(shared_ptr &&p) :object(p.object) { p.object = nullptr; }
    shared_ptr operator=(shared_ptr &&p) { SWAP(object, p.object); return *this; }

    shared_ptr &operator=(T *p)
    {
        if (p) p->add_ref();
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
    int ref_count;

    shared_object(const shared_object &);
    void operator=(const shared_object &);

public:
    shared_object() : ref_count(0) {}

    bool is_multi_ref() const { return ref_count > 1; }
    void add_ref() { ref_count++; }
    template <class T> static void dec_ref(T *object)
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

    int operator()(byte chunkid, bool seek = true)
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
            } else
                if (!seek)
                    return 0;

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
    i32 get_i32() const { return current_chunk_data_size >= sizeof(i32) ? (*(i32 *)chunkdata()) : 0; }
    u32 get_u32() const { return current_chunk_data_size >= sizeof(u32) ? (*(u32 *)chunkdata()) : 0; }
    u64 get_u64() const { return current_chunk_data_size >= sizeof(u64) ? (*(u64 *)chunkdata()) : 0; }
    std::asptr get_astr() const
    {
        if (current_chunk_data_size < 2) return std::asptr();
        int slen = *(unsigned short *)chunkdata();
        if ((current_chunk_data_size - 2) < slen) return std::asptr();
        return std::asptr((const char *)chunkdata() + 2, slen);
    }
};

struct savebuffer : public byte_buffer
{
    template<typename T> T &add()
    {
        size_t offset = size();
        resize(offset + sizeof(T));
        return *(T *)(data() + offset);
    }
    void add(const void *d, aint sz)
    {
        size_t offset = size();
        resize(offset + sz);
        memcpy(data() + offset, d, sz);
    }
    void add(const std::asptr& s)
    {
        add<unsigned short>() = (unsigned short)s.l;
        add(s.s, s.l);
    }

};

class bitset_s
{
    size_t * data = nullptr;
    size_t size = 0; // in size_t elements which is 32 or 64 bits

    enum : size_t
    {
        BITS_PER_ELEMENT = sizeof(size_t) * 8,
        MASK = BITS_PER_ELEMENT - 1,
    };

    static size_t maskmake(size_t shift)
    {
#if LITTLEENDIAN
        return static_cast<size_t>(1) << shift;
#endif
#if BIGENDIAN
        size_t x = static_cast<size_t>(1) << shift;
        return _MY_WS2_32_WINSOCK_SWAP_LONGLONG( x );
#endif
    }

    void sure_fit_bit(aint index)
    {
        size_t newsz = (index + BITS_PER_ELEMENT) / BITS_PER_ELEMENT;
        if (newsz > size)
        {
            size_t *ndata = (size_t *)realloc(data, newsz * sizeof(size_t));
            if (!ndata) return;
            memset(ndata + size, 0, (newsz - size) * sizeof(size_t));
            size = newsz;
            data = ndata;
        }
    }

public:

    ~bitset_s()
    {
        free( data );
    }

    void or_bits(const uint8_t *block, int blocksize)
    {
        sure_fit_bit( blocksize * 8 - 1 );

        size_t * tgt = data;
        for (; blocksize >= sizeof(size_t); blocksize -= sizeof(size_t), block += sizeof(size_t), ++tgt)
            *tgt |= *(size_t *)block;

        if (blocksize)
        {
            uint8_t *tgt8 = (uint8_t *)tgt;
            for (; blocksize > 0; --blocksize, ++block, ++tgt8)
                *tgt8 |= *block;
        }
    }

    bool get_bit(aint index) const
    {
        if (index >= aint(size * BITS_PER_ELEMENT)) return false;
        return 0 != (data[index / BITS_PER_ELEMENT] & maskmake((index & MASK)));
    }

    void set_bit(aint index, bool b)
    {
        sure_fit_bit(index);
        if (b) data[index / BITS_PER_ELEMENT] |= maskmake(index & MASK);
        else data[index / BITS_PER_ELEMENT] &= ~maskmake(index & MASK);
    }

    aint find_first_0() const
    {
        for( size_t i = 0; i<size; ++i )
        {
            size_t el = data[i];
            if (el != static_cast<size_t>(-1))
            {
                for (int k = 0; k < BITS_PER_ELEMENT; ++k)
                {
                    if (0 == (el & maskmake(k)))
                        return (i * BITS_PER_ELEMENT) | k;
                }
            }
        }
        return size * BITS_PER_ELEMENT;
    }

};


extern "C" {
    int crypto_generichash( unsigned char *out, size_t outlen,
        const unsigned char *in, unsigned long long inlen,
        const unsigned char *key, size_t keylen );
};

template <int hashsize> struct blake2b
{
    byte hash[hashsize];
    explicit blake2b( const byte_buffer &buf )
    {
        crypto_generichash(hash, hashsize, buf.data(), buf.size(), nullptr, 0);
    }
    blake2b( const void *buf, size_t len )
    {
        crypto_generichash( hash, hashsize, (const unsigned char *)buf, len, nullptr, 0 );
    }
};

struct bytes
{
    const void *data = nullptr;
    aint datasize = 0;
    bytes() {}
    bytes(const void *data, aint datasize) :data(data), datasize(datasize) {}
};

template<typename T> struct defnext
{
    static T * getnext(T * el)
    {
        return el->next;
    }
};

template<typename T> struct nonext
{
    static T * getnext(T *)
    {
        return nullptr;
    }
};

template<typename T, typename N = defnext<T> > struct serlist
{
    T *first;
    serlist(T *first) :first(first) {}
    T *operator() (T *el) const
    {
        return N::getnext( el );
    }
};

template<typename T> struct servec
{
    const std::vector<T> &vec;
    servec( const std::vector<T> &vec ) :vec( vec ) {}
};

struct chunk
{
    savebuffer &b;
    size_t sizeoffset;
    chunk &operator=(const chunk&) = delete;
    chunk(savebuffer &b) :b(b), sizeoffset(static_cast<size_t>(-1))
    {
    }
    chunk(savebuffer &b, byte chunkid) :b(b)
    {
        b.add<byte>() = chunkid;
        sizeoffset = b.size();
        b.add<int>();
    }
    ~chunk()
    {
        if (sizeoffset < b.size() - sizeof(i32))
            *(i32 *)(b.data() + sizeoffset) = (i32)(b.size() - sizeoffset);
    }

    void *alloc(aint sz)
    {
        b.add<i32>() = (i32)sz;
        size_t offset = b.size();
        b.resize( offset + sz );
        return b.data() + offset;
    }

    void operator <<(contact_id_s v) { b.add<contact_id_s>() = v; }
    void operator <<(byte v) { b.add<byte>() = v; }
    void operator <<( i32 v) { b.add<i32>() = v; }
    void operator <<(u64 v) { b.add<u64>() = v; }
    void operator <<(const std::str_c& str) { b.add(str.as_sptr()); }
    void operator <<(const bytes&byts) { b.add<i32>() = static_cast<i32>(byts.datasize); b.add(byts.data, byts.datasize); }
    template<typename T, typename N> void operator <<(const serlist<T,N>& sl)
    {
        size_t numoffset = b.size();
        b.add<i32>();
        int n = 0;
        for (T *el = sl.first; el; el = sl(el))
        {
            size_t preoffset = b.size();
            *this << *el;
            if (b.size() > preoffset)
                ++n;
        }
        *(i32 *)(b.data() + numoffset) = n;
    }
    template<typename T> void operator <<( const servec<T>& sv )
    {
        size_t numoffset = b.size();
        b.add<i32>();
        int n = 0;
        for ( const T&t : sv.vec )
        {
            size_t preoffset = b.size();
            *this << t;
            if ( b.size() > preoffset )
                ++n;
        }
        *(i32 *)( b.data() + numoffset ) = n;
    }
};


class fifo_stream_c
{
    byte_buffer buf[2];
    int readpos = 0;
    
    unsigned readbuf : 1;
    unsigned newdata : 1;

public:

    fifo_stream_c()
    {
        readbuf = 0;
        newdata = 0;
    }

    void add_data(const void *d, aint s)
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

    void get_data(aint offset, byte *dest, aint size); // copy data and leave it in buffer
    aint read_data(byte *dest, aint size); // dest can be null
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

/*
struct ranges_s
{
    struct range_s
    {
        u64 offset0;
        u64 offset1;
        range_s(u64 _offset0, u64 _offset1):offset0(_offset0), offset1(_offset1) {}
    };
    std::vector<range_s, std::pcmna> ranges;

    void add(u64 _offset0, u64 _offset1)
    {
        if (ranges.size() == 0)
        {
            ranges.emplace_back(_offset0, _offset1);
            return;
        }

        aint cnt = ranges.size();
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
*/

struct config_accessor_s
{
    std::asptr params;
    const byte *native_data = nullptr;
    const byte *protocol_data = nullptr;
    int native_data_len = 0;
    int protocol_data_len = 0;

    config_accessor_s( const void * d, int sz )
    {
        const byte *dd = (const byte *)d;
        u32 flags = *(u32 *)d;

        dd += sizeof( u32 );
        sz -= sizeof( u32 );

        if ( CFL_PARAMS & flags )
        {
            params.s = (const char *)dd + sizeof( i32 );
            params.l = *(i32 *)dd;
            dd += sizeof( i32 ) + params.l;
            sz -= sizeof( i32 ) + params.l;
        }
        if ( CFL_NATIVE_DATA & flags )
        {
            native_data = sz ? dd : nullptr;
            native_data_len = sz;
        } else
        {
            protocol_data = sz ? dd : nullptr;
            protocol_data_len = sz;
        }
    }
};

template<typename COPP, auto_conn_options_e co> struct handle_cop_s
{
    static bool h(const std::asptr&n, const std::asptr&nn, const std::asptr&v, const COPP& c)
    {
        if (std::pstr_c(n).equals(nn))
        {
            c( co, std::pstr_c(v).as_uint() != 0 );
            return true;
        }
        return false;
    }
};

template<typename COPP> struct handle_cop_s<COPP, auto_co_count>
{
    static bool h(const std::asptr&, const std::asptr&, const std::asptr&, const COPP&) { return false; }
};

template<size_t cops, typename COPP> void handle_cop(const std::asptr& n, const std::asptr& v, const COPP &copp)
{
#define COPDEF( nnn, dv ) if (handle_cop_s<COPP, ISFLAG(cops, SETBIT(auto_co_##nnn) ) ? auto_co_##nnn : auto_co_count>::h(n, STD_ASTR(#nnn), v, copp)) return;
    CONN_OPTIONS
#undef COPDEF
}

template<auto_conn_options_e co> struct copname{};
#define COPDEF( nnn, dv ) template <> struct copname<auto_co_##nnn> { static std::asptr name() { return STD_ASTR(#nnn); } };
CONN_OPTIONS
#undef COPDEF


template<typename EL > int findIndex(const std::vector<EL> &vec, const EL & el)
{
    size_t cnt = vec.size();
    for (size_t index = 0; index < cnt; ++index)
    {
        if (vec[index] == el) return (int)index;
    }
    return -1;
}

template<typename EL > bool isPresent(const std::vector<EL> &vec, const EL & el)
{
    for (std::vector<EL>::const_iterator it = vec.cbegin(); it != vec.cend(); ++it)
    {
        if (*it == el) return true;
    }
    return false;
}

template<typename EL > bool addIfNotPresent(std::vector<EL> &vec, const EL & el) // true - added
{
    for (std::vector<EL>::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el) return false;
    }
    vec.push_back(el);
    return true;
}

template< typename EL > bool removeIfPresent(std::vector<EL> &vec, const EL & el) // true - found and removed
{
    for (std::vector<EL>::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el)
        {
            vec.erase(it);
            return true;
        }
    }
    return false;
}

template< typename EL > bool removeIfPresentFast(std::vector<EL> &vec, const EL & el) // true - found and removed
{
    for (std::vector<EL>::iterator it = vec.begin(); it != vec.end(); ++it)
    {
        if (*it == el)
        {
            *it = std::move(vec.back());
            vec.resize(vec.size() - 1);
            return true;
        }
    }
    return false;
}

template< typename EL > void removeFast(std::vector<EL> &vec, size_t index) // true - found and removed
{
    std::vector<EL>::iterator it = vec.begin() + index;
    *it = std::move(vec.back());
    vec.resize(vec.size() - 1);
}

template<typename checker> u64 random64(const checker &ch)
{
    u64 v;
    do
    {
        randombytes_buf(&v, sizeof(v));
    } while (v == 0 || ch(v));
    return v;
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


