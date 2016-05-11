#pragma once
#include <crtdefs.h>

#define ZSTRINGS_SYSCALL(func) str_wrap_##func
#define ZSTRINGS_NULL nullptr
#define ZSTRINGS_CRC32(p,s) 0
#if defined _MSC_VER
#define ZSTRINGS_FORCEINLINE __forceinline
#define ZSTRINGS_UNREACHABLE __assume(0)
#elif defined __GNUC__
ZSTRINGS_FORCEINLINE inline
#define ZSTRINGS_UNREACHABLE __builtin_unreachable()
#endif
#define ZSTRINGS_NAMESPACE
#ifndef _FINAL
#define ZSTRINGS_DEBUG 1
#define ZSTRINGS_ASSERT ASSERT
#else
#define ZSTRINGS_DEBUG 0
#define ZSTRINGS_ASSERT(...) (1, true)
#endif
#define ZSTRINGS_NUMCONVERSION_ERROR(def) //if (!def) ZSTRINGS_ASSERT(false, "num conversion error")
#define ZSTRINGS_DEFAULT_STATIC_SIZE 1024 // 1024 bytes - default static string in-memory size
//#define ZSTRINGS_VEC3(t) vec_t<t,3>

#define ZSTRINGS_ALLOCATOR STR_ALLOCATOR

typedef char ZSTRINGS_ANSICHAR;
typedef wchar_t ZSTRINGS_WIDECHAR;

typedef unsigned char ZSTRINGS_BYTE;
typedef size_t ZSTRINGS_UNSIGNED; // 32 or 64 bit
typedef long ZSTRINGS_SIGNED; // 32 bit enough

template<typename TCHARACTER> struct sptr;

struct STR_ALLOCATOR
{
    void mf(void *ptr) { dlfree(ptr); }
    void *ma(ZSTRINGS_UNSIGNED sz) { return dlmalloc(sz); }
    void *mra(void *ptr, ZSTRINGS_UNSIGNED sz) { return dlrealloc(ptr, sz); }
};

int     str_wrap_text_ucs2_to_ansi(char *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from);
void    str_wrap_text_ansi_to_ucs2(wchar_t *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from);
ZSTRINGS_SIGNED str_wrap_text_utf8_to_ucs2(wchar_t *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from);
ZSTRINGS_SIGNED str_wrap_text_ucs2_to_utf8(char *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from);
bool    str_wrap_text_iequalsw(const wchar_t *s1, const wchar_t *s2, ZSTRINGS_SIGNED len);
bool    str_wrap_text_iequalsa(const char *s1, const char *s2, ZSTRINGS_SIGNED len);
void    str_wrap_text_lowercase(wchar_t *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_lowercase(char *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_uppercase(wchar_t *out, ZSTRINGS_SIGNED maxlen);
void    str_wrap_text_uppercase(char *out, ZSTRINGS_SIGNED maxlen);


#include "zstrings/z_str_hpp.inl"

typedef str_c tmp_str_c;
typedef wstr_c tmp_wstr_c;

