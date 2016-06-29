/*
    toolset
    (C) 2014-2016 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

#define _ALLOW_RTCc_IN_STL

#ifdef _WIN32
#define WINDOWS_ONLY
#else
#define WINDOWS_ONLY __error
#endif

#pragma pack (push, 1)

//-V::730 // disable 730 check globally - too unusable
//-V::UPAR:614

#if defined(_MSC_VER)
#pragma warning (disable:4100) //  unreferenced formal parameter
//#pragma warning (disable:4668) // 'MACRONAME' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning (disable:4640) // 'T' : construction of local static object is not thread-safe
#pragma warning (disable:4062) // enumerator 'xxx' in switch of enum 'yyy' is not handled
#pragma warning (disable:4127) // condition expression is constant
#pragma warning (disable:4201) // nameless struct/union
#pragma warning (disable:4091) // 'typedef ' : ignored on left of '' when no variable is declared

#pragma warning (push)
#pragma warning (disable:4820)
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wreorder"
#pragma GCC diagnostic ignored "-Wswitch"
#endif

#include <new>
#include <type_traits>
#include <memory>
#include <float.h>
#include "fastdelegate/FastDelegate.h"
#if defined _MSC_VER
#pragma warning (pop)
#endif

#define DELEGATE(a,method) fastdelegate::MakeDelegate((a), &ts::clean_type<decltype(a)>::type::method)

#ifndef TS_SKIP_FREETYPE
#pragma pack(push,1)
#include <ft2build.h>
#include FT_FREETYPE_H
#pragma pack(pop)
#endif

#if defined _MSC_VER
#define ALIGN(n) __declspec( align( n ) )
#define THREADLOCAL __declspec(thread)
#define TSCALL __cdecl
#define INLINE __forceinline
#define UNREACHABLE() __assume(0)

#define DLMALLOC_USED 1
#if defined _FINAL || defined _DEBUG_OPTIMIZED
#ifndef DLMALLOC_USED
#define DLMALLOC_USED 1
#endif
#endif

#elif defined __GNUC__
#define ALIGN(n) __attribute__((aligned(n)))
#define THREADLOCAL __thread
#define UNREACHABLE() __builtin_unreachable()
#define TSCALL // __attribute__((__cdecl__))
#define INLINE inline //__attribute__((always_inline))
#undef DLMALLOC_USED

#endif

#if defined (_M_AMD64) || defined (_M_X64) || defined (WIN64) || defined(__LP64__)
#define MODE64
#define ARCHBITS 64
#else
#define ARCHBITS 32
#endif

#if defined __linux__
#define _NIX
#endif

#define UPAR(p) { (p) = (p); }

#if DLMALLOC_USED
extern "C"
{
	void* dlmalloc(size_t);
	void  dlfree(void*);
	void* dlrealloc(void*, size_t);
	void* dlcalloc(size_t, size_t);
	size_t dlmalloc_usable_size(void*);
};

#define MEMSPY_SYS_ALLOC(sz) dlmalloc(sz)
#define MEMSPY_SYS_RESIZE(ptr,sz) dlrealloc(ptr,sz)
#define MEMSPY_SYS_FREE(ptr) dlfree(ptr)
#define MEMSPY_SYS_SIZE(ptr) dlmalloc_usable_size(ptr)
#else
// memspy uses crt malloc/free by default
#endif

#ifdef _FINAL
#define MM_ALLOC(sz) dlmalloc(sz)
#define MM_RESIZE(ptr,sz) dlrealloc(ptr,sz)
#define MM_FREE(ptr) dlfree(ptr)
#define MM_SIZE(ptr) dlmalloc_usable_size(ptr)
#else
#define MM_ALLOC(sz) mspy_malloc(__FILE__, __LINE__, sz)
#define MM_RESIZE(ptr,sz) mspy_realloc(__FILE__, __LINE__, ptr,sz)
#define MM_FREE(ptr) mspy_free(ptr)
#define MM_SIZE(ptr) mspy_size(ptr)
#endif

#include "memspy/memspy.h"

namespace ts
{
    template<class _Ty> struct clean_type { typedef _Ty type; };
    template<class _Ty> struct clean_type < _Ty& > { /*remove reference*/ typedef _Ty type; };
    template<class _Ty> struct clean_type < const _Ty& > { /*remove reference*/ typedef _Ty type; };
    template<class _Ty> struct clean_type < _Ty&& > { /* remove rvalue reference */ typedef _Ty type; };
    template<class _Ty> struct clean_type < _Ty* > { /*remove pointer*/ typedef _Ty type; };
    template<class _Ty> struct clean_type < const _Ty* > { /*remove pointer*/ typedef _Ty type; };
}

// I know, know... TSNEW is ugly. May be later I'll use global new and delete

template<typename T> struct TSNEWDEL
{
    template<class... _Valty> static T* __tsnew( _Valty&&... _Val )
        { T * t = (T *)MM_ALLOC(sizeof(T)); new(t)T(std::forward<_Valty>(_Val)...); return t; }

    template<class... _Valty> static void __tsplacenew(T* t, _Valty&&... _Val)
        { new(t)T(std::forward<_Valty>(_Val)...); }

    static void __tsdel(T *ptr) { if (!ptr) return; ptr->~T(); MM_FREE(ptr); }
};

#define DECLARE_DYNAMIC_BEGIN( cn ) friend struct TSNEWDEL<cn>; private:
#define DECLARE_DYNAMIC_END( inheritance ) inheritance:

#define TSPLACENEW(p, ...) TSNEWDEL< typename ts::clean_type<decltype(p) >::type >::__tsplacenew((p), ##__VA_ARGS__)
#define TSNEW(T, ...) TSNEWDEL<T>::__tsnew(__VA_ARGS__)
#define TSDEL(p) TSNEWDEL<typename ts::clean_type<decltype(p)>::type>::__tsdel(p)
#define TSDELC(p) do { TSDEL(p); p = nullptr; } while ((1, false))

#define PLEASE_USE_TSDEL =delete
#define UNUSED delete
#define delete PLEASE_USE_TSDEL
#define new PLEASE_USE_TSNEW

// final config
#ifdef _FINAL
#endif

// do config
#ifdef _DEBUG_OPTIMIZED
#define __DMSG__
#endif

// debug config
#if defined _DEBUG && !defined _DEBUG_OPTIMIZED && !defined _FINAL
#define __DMSG__
#endif

template <typename T, size_t N> char( &__ARRAYSIZEHELPER( T( &array )[ N ] ) )[ N ];
#define ARRAY_SIZE( a ) (sizeof(__ARRAYSIZEHELPER(a)))
#define ARRAY_WRAPPER( a ) ts::array_wrapper_c<const std::remove_reference<decltype(a[0])>::type>(a, ARRAY_SIZE(a))


#if defined _MSC_VER
#define NOWARNING(n,...) __pragma(warning(push)) __pragma(warning(disable:n)) __VA_ARGS__ __pragma(warning(pop))
#define DEBUG_BREAK() __debugbreak()
#define UNFINISHED(...) __pragma(message(__LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__))
#define STUB(...) __pragma(message(__LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__)); WARNING( __LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__ )
#define STOPSTUB(...) __pragma(message(__LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__)); ERROR( __LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__ )
#define NOP() ASSERT((1,true))

#define UNSAFE_BLOCK_BEGIN __try {
#define UNSAFE_BLOCK_END } __except ( -1 ) {}

#elif defined __GNUC__
#define NOWARNING(n,...) __VA_ARGS__
#define DEBUG_BREAK() __builtin_trap()
#define DO_PRAGMA(x) _Pragma (#x)
#define UNFINISHED(s) DO_PRAGMA(message (__LOC__ "unfinished function: " __FUNCTION__ ": " s))
#define STUB(...) _Pragma(message (__LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__); WARNING( __LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__ ))
#define STOPSTUB(...) _Pragma(message (__LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__); ERROR( __LOC__ "unfinished function: " __FUNCTION__ ": " __VA_ARGS__ ))
#define NOP() ASSERT((1,true))

#define UNSAFE_BLOCK_BEGIN try {
#define UNSAFE_BLOCK_END } catch ( ... ) {}

#endif


#ifndef _FINAL
#define DEBUGCODE(...) __VA_ARGS__
#define FINALCODE(...) 
#else
#define DEBUGCODE(...)
#define FINALCODE(...) __VA_ARGS__
#endif

//#define TS_STATIC_CHECK(expr, msg) typedef char UNIQID(static_check) [(expr) ? 1 : -1]
#define TS_STATIC_CHECK(expr, msg) static_assert((expr), msg)

//#define SIMPLE_STR_HASH4( s ) MAKEFOURCC( (s)[0], (s)[1], (s)[2], (s)[3] )
//#define SIMPLE_STR_HASH( s ) SIMPLE_STR_HASH4(s) ^ SIMPLE_STR_HASH4( s + sizeof(s) - 4 )

template<typename T> struct is_signed
{
	static const bool value = (((T)-1) < 0);
};
template<> struct is_signed<float>
{
    static const bool value = true;
};
template<> struct is_signed < double >
{
    static const bool value = true;
};


template<typename NUM, int shiftval> struct makemaxint
{
	static const NUM value = (makemaxint<NUM, shiftval - 1>::value << 8) | 0xFF;
};
template<typename NUM> struct makemaxint < NUM, 0 >
{
	static const NUM value = 0x7F;
};
template<typename NUM> struct maximum
{
	static const NUM value = is_signed<NUM>::value ? makemaxint<NUM, sizeof(NUM) - 1>::value : (NUM)(-1);
};
template<typename NUM> struct minimum
{
    static const NUM value = is_signed<NUM>::value ? (-maximum<NUM>::value-1) : 0;
};

#define SEC_PER_MIN     60U
#define SEC_PER_HOUR    (SEC_PER_MIN * 60U)
#define SEC_PER_DAY     (SEC_PER_HOUR * 24U)
#define SEC_PER_MONTH   (SEC_PER_DAY * 31U)
#define SEC_PER_YEAR    (SEC_PER_DAY * 365U)

/// Swap two varibales
template<class T> INLINE void SWAP(T& first, T& second) 
{
    T temp = std::move(first);
    first = std::move(second);
    second = std::move(temp);
}

#define BINSWAPDWORD( x ) (*(DWORD *)(&(x)))
#define BINSWAP(first, second) { BINSWAPDWORD(first) ^= BINSWAPDWORD(second); BINSWAPDWORD(second) ^= BINSWAPDWORD(first); BINSWAPDWORD(first) ^= BINSWAPDWORD(second); }
#ifndef LIST_ADD
#define LIST_ADD(el,first,last,prev,next)       {if((last)!=nullptr) {(last)->next=el;} (el)->prev=(last); (el)->next=0;  last=(el); if(first==0) {first=(el);}}
#define LIST_DEL(el,first,last,prev,next) \
    {if((el)->prev!=0) (el)->prev->next=el->next;\
        if((el)->next!=0) (el)->next->prev=(el)->prev;\
        if((last)==(el)) last=(el)->prev;\
    if((first)==(el)) (first)=(el)->next;}

#define LIST_INSERT_AFTER(after,el,first,last,prev,next)    \
{                                                           \
    if(after==nullptr)                                         \
    {                                                       \
        last->next = el;                                    \
        el->prev = last;                                    \
        el->next = nullptr;                                    \
        last = el;                                          \
    } else {                                                \
        el->prev = after;                                   \
        el->next = after->next;                             \
        if (after->next) after->next->prev = el; else last = el; \
        after->next = el;                                   \
    }                                                       \
}

#define LIST_INSERT_BEFORE(before,el,first,last,prev,next)  \
{                                                           \
    if(before==nullptr)                                        \
    {                                                       \
        first->prev = el;                                   \
        el->next = first;                                   \
        el->prev = nullptr;                                    \
        first = el;                                         \
    }  else {                                               \
        el->prev = before->prev;                            \
        el->next = before;                                  \
        if (before->prev) before->prev->next = el; else first = el; \
        before->prev = el;                                  \
    }                                                       \
}

#define LIST_DEL_CLEAR(el,first,last,prev,next) \
    {if(el->prev!=nullptr) el->prev->next=el->next;\
	if(el->next!=nullptr) el->next->prev=el->prev;\
	if(last==el) last=el->prev;\
	if(first==el) first=el->next;\
	el->prev=nullptr;\
    el->next=nullptr;}

#endif

//#define offsetof( _struct, _member ) ((size_t)(&((_struct *)0)->_member))

#define SAFECALL( ptr, func ) if (ptr) (ptr)->func

#ifndef LOCATION_DEFINED

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)

#define __STR3W__(x) L ## x
#define __STR2W__(x) __STR3W__( #x )
#define __STR1W__(x) __STR2W__(x)

#define __LOC__ __FILE__ "(" __STR1__(__LINE__)") : "
#define LOCATION_DEFINED
#endif

#define JOINMACRO2(x,y) x##y
#define JOINMACRO1(x,y) JOINMACRO2(x,y)

#define __W_DATE__ JOINMACRO1( L, __DATE__ )

#define UNIQID(x) JOINMACRO1(x,__COUNTER__)
#define UNIQIDLINE(x) JOINMACRO1(x,__LINE__)

typedef unsigned int uint;
typedef unsigned long ulong;

#if defined(_MSC_VER)
typedef signed __int64		int64;
typedef unsigned __int64	uint64;
#elif defined(__GNUC__)
typedef signed long long	int64;
typedef unsigned long long	uint64;
#endif

namespace ts // some ts types
{
    #if defined _MSC_VER
    typedef wchar_t wchar;
    #elif defined __GNUC__
    typedef char16_t wchar;
    #endif
    typedef unsigned long dword;
    typedef unsigned long uint32;
    typedef unsigned char uint8;
    typedef unsigned short uint16;
    typedef unsigned short word;

    typedef signed char int8;
    typedef signed short int16;
    typedef signed long int32;

    typedef uint32 TSCOLOR;

    typedef size_t auint; // auto uint (32/64 bit)
    typedef ptrdiff_t aint; // auto int (32/64 bit)

    class smart_int
    {
        int value;
    public:
        smart_int( int v = 0 ):value(v) {}
        smart_int & operator=( int v ) { value = v; return *this; }
        operator int() const { return value; }

        smart_int& operator++()
        {
            ++value;
            return *this;
        }

        smart_int operator++( int )
        {
            smart_int tmp = *this;
            ++value;
            return tmp;
        }
        smart_int& operator--()
        {
            --value;
            return *this;
        }

        smart_int operator--( int )
        {
            smart_int tmp = *this;
            --value;
            return tmp;
        }

        smart_int& operator +=( int v )
        {
            value += v;
            return *this;
        }
        smart_int& operator -=( int v )
        {
            value -= v;
            return *this;
        }

#ifdef MODE64
        smart_int( aint v ) :value( (int)v ) {}
        smart_int & operator=( aint v ) { value = (int)v; return *this; }
#endif // MODE64
    };

    struct TS_DEFAULT_ALLOCATOR
    {
        INLINE void * TSCALL  ma(auint sz)
        {
            return MM_ALLOC(sz);
        }
        INLINE void   TSCALL  mf(void *ptr)
        {
            MM_FREE(ptr);
        }
        INLINE void * TSCALL  mra(void *ptr, auint sz)
        {
            return MM_RESIZE(ptr, sz);
        }
    };

} // namespace ts

#include "tsstrs.h" // "strings lib" is most independent
#include "tsdebug.h"

namespace ts
{

#if defined _MSC_VER
#pragma warning (push)
#pragma warning (disable:4311) //: 'type cast' : pointer truncation from 'const void *' to 'DWORD'
#pragma warning (disable:4267) //: 'argument' : conversion from 'size_t' to 'DWORD', possible loss of data
#pragma warning (disable:4312) //: 'type cast' : conversion from 'DWORD' to 'void *' of greater size
#endif
template <typename T> INLINE T p_cast( const void * p ) {return (T)p;}
template <typename P, typename T> INLINE P p_cast( T p ) {return (P)p;}
template <typename T> INLINE T sz_cast( size_t p ) {return (T)p;}
#if defined _MSC_VER
#pragma warning (pop)
#endif

namespace internals
{
    template<typename T, bool ispod> struct is_movable__
    {
        static const bool xvalue = true;
    };
    template<typename T> struct is_movable__ < T, false >
    {
        static const bool xvalue = T::__movable;
    };

    template <typename T> struct movable_predefined
    {
        static const bool value = std::is_pod<T>::value || std::is_pointer<T>::value;
    };
    template <typename T, typename D> struct movable_predefined< std::unique_ptr< T, D > >
    {
        static const bool value = true;
    };
    template <typename T, typename D> struct movable_predefined< str_t< T, D > >
    {
        static const bool value = true;
    };

    template <typename T> struct movable_predefined< fastdelegate::FastDelegate< T > >
    {
        static const bool value = true;
    };

    template<typename T1, typename T2, bool t1_bigger_t2> struct biggest__;
    template<typename T1, typename T2> struct biggest__<T1, T2, true>
    {
        typedef T1 type;
    };
    template<typename T1, typename T2> struct biggest__<T1, T2, false>
    {
        typedef T2 type;
    };

    INLINE void set( int &a, int b ) { a = b; }
    INLINE void set( int16 &a, int16 b ) { a = b; }
    INLINE void set( uint16 &a, uint16 b ) { a = b; }
    INLINE void set( float &a, float b ) { a = b; }

    INLINE void set( int16 &a, int b ) { a = (int16)b; }
#ifdef MODE64
    INLINE void set( int &a, aint b ) { a = (int)b; }
#endif // MODE64
}

template<typename T> struct is_movable
{
    static const bool value = internals::is_movable__<T, internals::movable_predefined<T>::value >::xvalue;
};

#define MOVABLE(b) template<typename TTT, bool ZZZ> friend struct ts::internals::is_movable__; static const bool __movable = (b)

template<typename T1, typename T2> struct biggest
{
    typedef typename internals::biggest__<T1, T2, sizeof( T1 ) >= sizeof( T2 )>::type type;
};


template<class _Ty1, class _Ty2> struct pair_s
{	// store a pair of values
    typedef pair_s<_Ty1, _Ty2> me_type;
    typedef _Ty1 first_type;
    typedef _Ty2 second_type;

    MOVABLE( is_movable<_Ty1>::value && is_movable<_Ty2>::value );

    pair_s() : first(_Ty1()), second(_Ty2())
    {	// construct from defaults
    }

    pair_s(const _Ty1& _Val1, const _Ty2& _Val2) : first(_Val1), second(_Val2)
    {	// construct from specified values
    }

    template<class _Other1, class _Other2> pair_s(const pair_s<_Other1, _Other2>& _Right) : first(_Right.first), second(_Right.second)
    {	// construct from compatible pair
    }

    template<class _Other1, class _Other2> bool operator==( const pair_s<_Other1, _Other2>& _Right ) const
    {
        return first == _Right.first && second == _Right.second;
    }

    _Ty1 first;	// the first stored value
    _Ty2 second;	// the second stored value
};

#define PTR_TO_UNSIGNED( p ) ((size_t)p)

namespace staticnumgen
{
    template< int n > struct staticnumgen_s
    {
        static const int N = n;
    };
}

#define NUMGEN_START( ngn, stv ) struct numgen_##ngn##_s : public ts::staticnumgen::staticnumgen_s<stv + __COUNTER__+1> {}
#define NUMGEN_NEXT( ngn ) (__COUNTER__-numgen_##ngn##_s::N)
#define NUMGEN_PREV( ngn ) (numgen_##ngn##_s::N - __COUNTER__)

INLINE uint8 as_byte(int aa) {return uint8(aa & 0xFF);}
INLINE uint8 as_byte( uint aa ) { return uint8( aa & 0xFF ); }
INLINE uint8 as_byte( uint32 aa) {return uint8(aa & 0xFF);}
INLINE uint8 as_byte( uint64 aa ) { return uint8( aa & 0xFF ); }

INLINE word as_word(int aa) {return word(aa & 0xFFFF);}
INLINE word as_word(uint aa) {return word(aa & 0xFFFF);}
INLINE word as_word(uint32 aa) {return word(aa & 0xFFFF);}
INLINE word as_word(uint64 aa) {return word(aa & 0xFFFF);}
//INLINE word as_word(dword aa) {return word(aa & 0xFFFF);}

INLINE dword as_dword(uint64 aa) {return dword(aa & 0xFFFFFFFF);} //-V112


template <typename T, aint factor = sizeof(T)>
class aligned
{
    char    store_[sizeof(T) + factor -1];

public:
    INLINE aligned() {
    }

    INLINE aligned(T src) {
        *(&(*this)) = src;
    }

    INLINE T* operator &() const {
        return (T*)addr();
    }

    INLINE operator T& () const {
        return *(&(*this));
    }

    INLINE T& operator= (T src) {
        return *(&(*this)) = src;
    }

protected:
    INLINE char* addr() const {
        return (char*)(
            ((size_t)&store_ + factor -1) - 
            ((size_t)&store_ + factor -1) % factor);
    }
};

/*
// no need in C++11

template<typename T, T def = (T)0> class initialized
{
    T val;
public:
    initialized():val(def) {}
    initialized( const T & iv ):val(iv) {}

    const T& operator->() const {return val;}
    T& operator->() {return val;}
    operator T&() {return val;}
    operator const T&() const {return val;}
    initialized &operator=( const T & iv ) { val=iv; return *this;}
};
*/

template<class T, class... _Valty> inline void renew(T &obj, _Valty&&... _Val)
{
    obj.~T();
    TSNEWDEL<T>::__tsplacenew(&obj, std::forward<_Valty>(_Val)...);
}

template<typename T> class make_pod // This hack is usable for hide constructors or copy operators of some pod type. Never use it for non true pod types...or... sure, you know what you do
{
    uint8 data[sizeof(T)];
public:
    operator T&() { return *(T *)data; }
    T& cget() { memset(data, 0, sizeof(data)); return *(T *)data; }
    T& get() { return *(T *)data; }
    const T& get() const { return *(T *)data; }
    T *operator&() { return (T *)data; }
    const T *operator&() const { return (T *)data; }
    T& operator()() { return *(T *)data; }
    const T& operator()() const { return *(T *)data; }

    T &operator=(const T&t) { get() = t; return get(); }
};

template<typename T> class only_destructor : public make_pod<T> // very special hack - object without calling constructor, but destructor.
{
public:
    ~only_destructor()
    {
        make_pod<T>::get().~T();
    }
};


template<aint sz> class uninitialized
{
    uint8 data[sz];
    bool initialized = false;
public:
    template<typename T, class... _Valty> T &initialize(_Valty&&... _Val)
    {
        TS_STATIC_CHECK( sizeof(T) == sz, "please provide correct size" );
        ASSERT(!initialized);
        initialized = true;
        T &t = get<T>();
        TSPLACENEW(&t, std::forward<_Valty>(_Val)...);
        return t;
    }
    template<typename T> void destroy()
    {
        if (initialized)
        {
            get<T>().~T();
            initialized = false;
        }
    }
    ~uninitialized()
    {
        ASSERT(!initialized);
    }
    operator bool() const {return initialized;}
    template<typename T> T& get() { ASSERT(initialized); TS_STATIC_CHECK( sizeof(T) == sz, "please provide correct size" ); return *(T *)data; }
};

struct sobase
{
	static sobase *first;
    static bool initialized;
    static bool initializing;
	sobase *next;
	aint priority;
	virtual void destruct() = 0;
    virtual void init() = 0;
	sobase(aint priority) : priority(priority)
	{
		sobase **f = &first;
        for(;*f && priority > (*f)->priority; f = &(*f)->next);
        next = *f;
		*f = this;
	}
    static void init_all()
    {
        if (initializing) return;
        ASSERT(!initialized);
        initializing = true;
        for(sobase *obj = first; obj; obj = obj->next) obj->init();
        initializing = false;
        initialized = true;
    }
    static void destruct_all()
    {
        ASSERT(initialized);
        // back order
        for(;first;)
        {
            // just iterate list to find last item. stupid, but its ok: slow deinitialization is not big evil
            sobase **prev = &first;
            for (sobase *obj = first; obj; obj = obj->next)
            {
                if (obj->next == nullptr) // last
                {
                    obj->destruct();
                    *prev = nullptr;
                } else
                    prev = &obj->next;
            }
        }
        initialized = false;
    }
};

template <typename T, aint _priority=10000 /*  Lower numbers indicate a higher priority */> class static_setup : public sobase
{
#ifdef _DEBUG
    T *objptr; // debug purpose
#endif
    make_pod<T> obj;
    bool me_initialized = false;
	/*virtual*/ void destruct() override
	{
        if (me_initialized)
		    obj.get().~T();
	}
    /*virtual*/ void init() override
    {
        if (me_initialized) return;
        TSPLACENEW( &obj.get() );
        me_initialized = true;
#ifdef _DEBUG
        objptr = &obj.get();
#endif
    }
public:
	static_setup() : sobase(_priority) { if (initialized || initializing) init(); }
    ~static_setup() { if (sobase::initialized) sobase::destruct_all(); }
    T &operator()() { if (!sobase::initialized) sobase::init_all(); ASSERT(me_initialized); return obj.get(); }
    const T &operator()() const { if (!sobase::initialized) sobase::init_all(); ASSERT(me_initialized); return obj.get(); }
};

// file access

struct buf_wrapper_s
{
    virtual void *alloc(aint size) = 0;
};

class tsfileop_c;
extern tsfileop_c *g_fileop;

INLINE void _ts_construct(uint *_Ptr, uint _Val)
{
	*_Ptr = _Val;
}

template <class CC> INLINE   void _ts_construct(CC *_Ptr, const CC &_Val)
{
	TSPLACENEW(_Ptr, _Val );
}

#define MAX_TEMP_ARRAYS 4

struct tmpbuf_s
{
    void *data; auint size;
    bool used = false;
    bool extra = false;
    tmpbuf_s( bool extra = false ) : data(nullptr), size(0), extra(extra) {}
    ~tmpbuf_s() { if (data) MM_FREE(data); }
    void *resize(auint sz)
    {
        if (sz > size)
        {
            data = MM_RESIZE(data, sz);
            size = sz;
        }
        return data;
    }
};

class tmpalloc_c
{
    static THREADLOCAL tmpalloc_c *core;

    tmpbuf_s bufs[MAX_TEMP_ARRAYS];
    uint32 threadid;

public:
    tmpalloc_c();
    ~tmpalloc_c();
    static tmpbuf_s *get();
    static void unget( tmpbuf_s * b );
};

struct TMP_ALLOCATOR
{
    tmpbuf_s *curbuf;

    TMP_ALLOCATOR():curbuf( tmpalloc_c::get() )
    {
    }
    ~TMP_ALLOCATOR()
    {
        tmpalloc_c::unget( curbuf );
    }

    INLINE void * TSCALL  ma(auint sz)
    {
        return curbuf->resize(sz);
    }
    INLINE void   TSCALL  mf(void *ptr)
    {
    }
    INLINE void * TSCALL  mra(void *, auint sz)
    {
        return curbuf->resize(sz);
    }
};

typedef str_t<ZSTRINGS_ANSICHAR, str_core_copy_on_demand_c<ZSTRINGS_ANSICHAR, TMP_ALLOCATOR> > tmp_str_c;
typedef str_t<ZSTRINGS_WIDECHAR, str_core_copy_on_demand_c<ZSTRINGS_WIDECHAR, TMP_ALLOCATOR> > tmp_wstr_c;


template<size_t sz> struct sztype;
template<> struct sztype < 1 > { typedef uint8 type; };
template<> struct sztype < 2 > { typedef uint16 type; };
template<> struct sztype < 3 > { typedef uint32 type; };
template<> struct sztype < 4 > { typedef uint32 type; };
template<> struct sztype < 5 > { typedef uint64 type; };
template<> struct sztype < 6 > { typedef uint64 type; };
template<> struct sztype < 7 > { typedef uint64 type; };
template<> struct sztype < 8 > { typedef uint64 type; };

template<size_t sz> struct enough
{
    enum
    {
        sz2sz = (sz <= 256u) ? 1 :
        ((sz <= 65536u) ? 2 :
        ((sz <= (65536ull * 65536ull)) ? 4 : 8))
    };
    typedef typename sztype<sz2sz>::type type;
};

template<aint bsize, aint count> class struct_pool_t
{
    uint8 m_buffer[count * bsize];
    aint  m_current_count = 0;
    aint  m_free_count = 0;
    typedef typename enough<count>::type fbindex;
    fbindex m_free_buffer[count];

public:

    struct_pool_t() {}

    void *    alloc()
    {
        void * ptr = try_alloc();
        if (ptr != nullptr) return ptr;
        return MM_ALLOC(bsize);
    }

    void *    try_alloc()
    {
        if (m_free_count > 0)
        {
            aint disp = bsize * m_free_buffer[--m_free_count];
            return (void*)(m_buffer + disp);
        }
        if (m_current_count < count)
        {
            aint disp = (bsize * m_current_count++);
            return (void*)(m_buffer + disp);
        }
        return nullptr;
    }

    void    dealloc(void * c)
    {
        aint disp = ((uint8 *)c) - m_buffer;

        if ((c >= &m_buffer) && disp < (count * bsize))
        {
            fbindex fi = (fbindex)(disp/bsize);
            if (ASSERT( (disp == bsize * fi) && m_free_count < count ))
                m_free_buffer[m_free_count++] = fi;
        }
        else
        {
            MM_FREE(c);
        }
    }
    bool     yours(void *c) const
    {
        aint disp = ((uint8 *)c) - m_buffer;
        return (c >= &m_buffer) && disp < (count * bsize);
    }
};

#define DUMMY_USED_WARNING(b_quiet) if (!b_quiet) WARNING("Dummy used")

template<typename T, bool isptr> struct dummy_maker;
template<typename T> struct dummy_maker < T, true >
{
	static T &get(bool quiet) { static T t = nullptr; DUMMY_USED_WARNING(quiet); return t; }
};
template<typename T> struct dummy_maker < T, false >
{
    static T &get(bool quiet) { DUMMY_USED_WARNING(quiet); return T::get_dummy(); }
};

template<typename T> T &make_dummy(bool quiet = false)
{
	//if (std::is_pointer<T>::value) { static T t = nullptr; return t; }
	//else { return T::get_dummy(); }
	return dummy_maker<T, std::is_pointer<T>::value >::get(quiet);
}
template<> INLINE int &make_dummy<int>(bool quiet) { static int t = 0; DUMMY_USED_WARNING(quiet); return t; }
template<> INLINE uint &make_dummy<uint>(bool quiet) { static uint t = 0; DUMMY_USED_WARNING(quiet); return t; }
template<> INLINE TSCOLOR &make_dummy<TSCOLOR>(bool quiet) { static TSCOLOR t = 0; DUMMY_USED_WARNING(quiet); return t; }
template<> INLINE str_c &make_dummy<str_c>(bool quiet) { static str_c t; DUMMY_USED_WARNING(quiet); return t; }
template<> INLINE wstr_c &make_dummy<wstr_c>(bool quiet) { static wstr_c t; DUMMY_USED_WARNING(quiet); return t; }


template<typename Tout, typename Tin> Tout &ref_cast(Tin & t)
{
    TS_STATIC_CHECK(sizeof(Tout) <= sizeof(Tin), "ref cast fail");
    return (Tout &)t;
}
template<typename Tout, typename Tin> const Tout &ref_cast(const Tin & t) //-V659
{
    TS_STATIC_CHECK(sizeof(Tout) <= sizeof(Tin), "ref cast fail");
    return *(const Tout *)&t;
}

template<typename Tout, typename Tin> const Tout &ref_cast(const Tin & t1, const Tin & t2)
{
    TS_STATIC_CHECK(sizeof(Tout) <= (sizeof(Tin)*2), "ref cast fail");
    ASSERT( ((uint8 *)&t1)+sizeof(Tin) == (uint8 *)&t2 );
    return *(const Tout *)&t1;
}

#ifndef _FINAL
template <typename T> INLINE T * BREAK_ON_NULL(T * ptr, const char *file, int line) { if (ptr == nullptr) { ERROR( "nullptr pointer conversion: %s:%i",file,line ); } return ptr; }
#define NOT_NULL( x ) BREAK_ON_NULL(x, __FILE__, __LINE__)
//#define PTRCVT_N( to_type, from_ptr ) ((from_ptr) ? BREAK_ON_NULL<to_type>( dynamic_cast< to_type * >(from_ptr), __FILE__, __LINE__ ) : nullptr )
//#define PTRCVT( to_type, from_ptr ) BREAK_ON_NULL<to_type>( dynamic_cast< to_type * >(from_ptr), __FILE__, __LINE__ )
//#define PTRCVT_D( to_type, from_ptr ) BREAK_ON_NULL<to_type>( dynamic_cast< to_type * >(from_ptr), __FILE__, __LINE__ )

template<typename PTRT, typename TF> INLINE PTRT ptr_cast(TF *p) { if (!p) return nullptr; return NOT_NULL( dynamic_cast<PTRT>(p) ); }

#else
#define NOT_NULL( x ) x
//#define PTRCVT_N( to_type, from_ptr ) static_cast< to_type * >(from_ptr)
//#define PTRCVT( to_type, from_ptr ) static_cast< to_type * >(from_ptr)
//#define PTRCVT_D( to_type, from_ptr ) dynamic_cast< to_type * >(from_ptr)

template<typename PTRT, typename TF> INLINE PTRT ptr_cast(TF *p) { return static_cast<PTRT>(p); }

#endif

template <class T> class array_wrapper_c //array wrapper, which can be initialized by any of the array types
{
	T *data_;
	aint length_;

public:
	array_wrapper_c(T *data, aint size) : data_(data), length_(size) {}

	aint   length() const { return length_; }
	aint     size() const { return length_; }

	bool empty() const { return length_ == 0; }

	const T &operator[](aint i) const
	{
		if (ASSERT(unsigned(i) < unsigned(length_))) return data_[i];
		return make_dummy<T>();
	}

	T &operator[](aint i)
	{
		if (ASSERT(unsigned(i) < unsigned(length_))) return data_[i];
		return make_dummy<T>();
	}

    array_wrapper_c subarray(aint index) const
    {
        ASSERT( index >= 0 && index <= length_ );
        return array_wrapper_c( data_ + index, length_ - index );
    }
    array_wrapper_c subarray(aint index0, aint index1) const
    {
        ASSERT( index0 <= length_ && index1 <= length_ && index0 < index1 );
        return array_wrapper_c(data_ + index0, index1 - index0);
    }
    array_wrapper_c subarray_safe(aint index) const
    {
        if (index < 0) index = 0;
        if (index > length_) return array_wrapper_c(data_ + length_, 0);
        return array_wrapper_c(data_ + index, length_ - index);
    }
    array_wrapper_c subarray_safe(aint index0, aint index1) const
    {
        if (index0 < 0) index0 = 0;
        if (index0 > length_ || index0 > index1) return array_wrapper_c(data_ + length_, 0);
        if (index1 > length_) index1 = length_;
        return array_wrapper_c(data_ + index0, index1 - index0);
    }

    bool operator==(const array_wrapper_c &o)
    {
        if (length_ != o.length_) return false;
        const T *a1 = begin();
        const T *a2 = o.begin();
        for(const T *e = end(); a1 < e; ++a1, ++a2)
            if (!(*a1 == *a2))
                return false;
        return true;
    }

    T * begin() { return data_ + 0; }
    const T *begin() const { return data_ + 0; }

    T *end() { return data_ + length_; }
    const T *end() const { return data_ + length_; }

};

struct dummyparam { dummyparam() {}; char unused; }; //-V730
template <typename T> struct dummy
{
    T t;
    dummy() :t(dummyparam()) {}
};

#define DUMMY(cn) cn(const ts::dummyparam &) {}; friend struct ts::dummy<cn>; friend struct ts::dummy_maker<cn, false>; static cn & get_dummy() { static ts::dummy<cn> d; return d.t; }

} // namespace ts


#define FORWARD_DECLARE_STRING(c, s) template <typename TCHARACTER, typename ALC> class str_core_copy_on_demand_c; template <typename TCHARACTER, class CORE > class str_t; typedef str_t<c, str_core_copy_on_demand_c<c, ZSTRINGS_ALLOCATOR>> s;

#include "tsflags.h"
#include "tscrc.h"
#include "tsmath.h"
#include "tsbuf.h"
#include "tshash_md5.h"
#include "tspointers.h"
#include "tsarray.h"
#include "tsrnd.h"
#include "tsstrar.h"
#include "tshashmap.h"
#include "tsbitmap.h"
#include "tspackcol.h"
#include "tsfilesystem.h"
#include "tsbp.h"
#include "tssystools.h"
#include "tsmlock.h"
#include "tstime.h"
#include "tsparse.h"
#include "tssqlite.h"
#include "tszip.h"
#include "tsexecutor.h"
#include "tswnd.h"
#include "tssys.h"

#pragma pack (pop)

extern "C" { extern ts::uint32 g_cpu_caps; extern int g_cpu_cores; }

namespace ts
{

enum cpu_caps_e
{
    CPU_MMX     = SETBIT(0),
    CPU_SSE     = SETBIT(1),
    CPU_SSE2    = SETBIT(2),
    CPU_SSE3    = SETBIT(3),
    CPU_SSSE3   = SETBIT(4),
    CPU_SSE41   = SETBIT(5),
    CPU_AVX     = SETBIT(6),
    CPU_AVX2    = SETBIT(7),
};

static_assert( CPU_MMX == 1, "check a_resize.asm" );

INLINE bool CCAPS( uint32 mask ) { return 0 != (g_cpu_caps & mask); }

class tsfileop_c
{
    static void killop();
public:

    tsfileop_c() {}
    virtual ~tsfileop_c() {};

    virtual bool read(const wsptr &fn, buf_wrapper_s &b) = 0;
    virtual bool size(const wsptr &fn, size_t &sz) = 0;

    virtual void find(wstrings_c & files, const wsptr &fnmask, bool full_paths) = 0;

    template<typename T> static void setup()
    {
        tsfileop_c *nfop = TSNEW(T, &g_fileop);
        killop();
        g_fileop = nfop;
    }

    blob_c load(const wstrings_c &paths, const wsptr &fn);
    blob_c load(const wsptr &fn);
    bool load(const wsptr &fn, buf_wrapper_s &b, size_t reservebefore = 0, size_t reserveafter = 0);
    bool load(const wsptr &fn, abp_c &bp);
};

/*
    windows lnk file reader
*/
struct lnk_s
{
    lnk_s() {}
    lnk_s(const uint8 *data, aint datasize):data(data), datasize(datasize) {}

    // setup
    const uint8 *data = nullptr;
    aint datasize = 0;

    // call read
    bool read();

    // now, you can read values
    wstr_c local_path;
    wstr_c relative_path;
    wstr_c working_directory;
    wstr_c command_line_arguments;
};

	template<class _Ty> struct ts_delete
	{	// default deleter for unique_ptr
		typedef ts_delete<_Ty> _Myt;

		ts_delete()
		{	// default construct
		}

		template<class _Ty2> ts_delete(const ts_delete<_Ty2>&)
		{	// construct from another default_delete
		}

		void operator()(_Ty *_Ptr) const
		{	// delete a pointer
			if (0 < sizeof (_Ty))	// won't compile for incomplete type
				TSDEL(_Ptr);
		}
	};
#define UNIQUE_PTR(ty) std::unique_ptr<ty, ts::ts_delete<ty> >

    template<typename T> const char *shorttypename();
    template<> INLINE const char *shorttypename<short>() {return "s";};
    template<> INLINE const char *shorttypename<int>() {return "i";};
    template<> INLINE const char *shorttypename<float>() {return "f";};
    template<> INLINE const char *shorttypename<double>() {return "d";};

    template<typename STRTYPE, typename VT, aint VSZ> INLINE streamstr<STRTYPE> & operator<<( streamstr<STRTYPE> &dl, const vec_t<VT,VSZ> &v )
    {
        (dl << shorttypename<VT>()).raw_append("vec[");
        for(aint i=0;i<VSZ;++i)
        {
            dl << v[i];
            if (i < VSZ-1) dl << ",";
        }
        return dl << "]";
    }
}

#if defined _DEBUG || defined _CRASH_HANDLER
#ifdef _WIN32
struct _EXCEPTION_POINTERS;
long __stdcall crash_exception_filter( _EXCEPTION_POINTERS* pExp );
extern "C" { void * __cdecl _exception_info( void ); }
#define UNSTABLE_CODE_PROLOG __try {
//#define UNSTABLE_CODE_EPILOG } __except(crash_exception_filter(GetExceptionInformation())){}
#define UNSTABLE_CODE_EPILOG } __except(crash_exception_filter((_EXCEPTION_POINTERS *)_exception_info())){}
#endif
#else
#define UNSTABLE_CODE_PROLOG ;
#define UNSTABLE_CODE_EPILOG ;
#endif

#include "tstext.h"
#include "tsjson.h"

// out of ts namespace functions

template<typename T> T& SAFE_REF(T * t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> const T& SAFE_REF(const T * t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> T& SAFE_REF(ts::safe_ptr<T> &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> const T& SAFE_REF(const ts::safe_ptr<T> &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> T& SAFE_REF(ts::shared_ptr<T> &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> const T& SAFE_REF(const ts::shared_ptr<T> &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T1, typename T2> T2& SAFE_REF(ts::iweak_ptr<T1, T2> &t) { return (t) ? (*(t)) : ts::make_dummy<T2>(); }
template<typename T1, typename T2> const T2& SAFE_REF(const ts::iweak_ptr<T1, T2> &t) { return (t) ? (*(t)) : ts::make_dummy<T2>(); }
template<typename T> T& SAFE_REF(std::unique_ptr<T, ts::ts_delete<T> > &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }
template<typename T> const T& SAFE_REF(const std::unique_ptr<T, ts::ts_delete<T> > &t) { return (t) ? (*(t)) : ts::make_dummy<T>(); }



/*
little help:

is_signed<T>::value
maximum<T>::value
make_pod<T> - remove constructor from pod type. don't use for non-po types!
make_dummy<T>() - returns dummy instance of object. See DUMMY(T)
initialized<T,def> - use inside structures to create default initialized members
aligned<T> - aligned var

SAFE_REF( ptr ) - returns valid reference to object even ptr is nullptr

*/