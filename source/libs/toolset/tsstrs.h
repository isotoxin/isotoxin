#pragma once

namespace ts
{
bool AssertFailed(const char *file, int line, const char *s, ...);
INLINE bool AssertFailed(const char *file, int line);
}

#define ZSTRINGS_SYSCALL(func) str_wrap_##func
#define ZSTRINGS_NULL nullptr
#define ZSTRINGS_CRC32(p,s) 0
#define ZSTRINGS_FORCEINLINE INLINE
#define ZSTRINGS_NAMESPACE ts
#define ZSTRINGS_UNREACHABLE UNREACHABLE()
#ifndef _FINAL
#define ZSTRINGS_DEBUG 1
#if defined _MSC_VER
#define ZSTRINGS_ASSERT(expr, ...) NOWARNING(4800, ((expr) || (ts::AssertFailed(__FILE__, __LINE__, __VA_ARGS__) ? (DEBUG_BREAK(), false) : false)))
#elif defined __GNUC__
inline bool f_assert(const char *f, int line, bool expr) { if (expr) return true; if (ts::AssertFailed(f, line)) DEBUG_BREAK(); return false; }
#define ZSTRINGS_ASSERT(expr, ...) f_assert(__FILE__, __LINE__, expr)
#endif
#else
#define ZSTRINGS_DEBUG 0
#define ZSTRINGS_ASSERT(...) (1, true)
#endif
#define ZSTRINGS_NUMCONVERSION_ERROR(def) //if (!def) ZSTRINGS_ASSERT(false, "num conversion error")
#define ZSTRINGS_DEFAULT_STATIC_SIZE 1024 // 1024 bytes - default static string in-memory size
//#define ZSTRINGS_VEC3(t) vec_t<t,3>

#define ZSTRINGS_ALLOCATOR TS_DEFAULT_ALLOCATOR

namespace ts
{


typedef char ZSTRINGS_ANSICHAR;
#if defined _MSC_VER
typedef wchar_t ZSTRINGS_WIDECHAR;
#elif defined __GNUC__
typedef char16_t ZSTRINGS_WIDECHAR;
#endif

typedef unsigned char ZSTRINGS_BYTE;
typedef size_t ZSTRINGS_UNSIGNED; // 32 or 64 bit
typedef long ZSTRINGS_SIGNED; // 32 bit enough

template<typename TCHARACTER> struct sptr;

int     str_wrap_text_ucs2_to_ansi(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from);
void    str_wrap_text_ansi_to_ucs2(ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from);
ZSTRINGS_SIGNED str_wrap_text_utf8_to_ucs2(ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from);
ZSTRINGS_SIGNED str_wrap_text_ucs2_to_utf8(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from);
bool    str_wrap_text_iequalsw(const ZSTRINGS_WIDECHAR *s1, const ZSTRINGS_WIDECHAR *s2, ZSTRINGS_SIGNED len);
bool    str_wrap_text_iequalsa(const ZSTRINGS_ANSICHAR *s1, const ZSTRINGS_ANSICHAR *s2, ZSTRINGS_SIGNED len);
void    str_wrap_text_lowercase(ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_lowercase(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_uppercase(ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_uppercase(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen);

#include "zstrings/z_str_hpp.inl"

//typedef str_c tmp_str_c;
//typedef wstr_c tmp_wstr_c;

}


