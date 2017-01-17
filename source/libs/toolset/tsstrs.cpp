#include "toolset.h"
#include "internal/platform.h"
#include "tsstrs.h"

namespace ts
{
#include "zstrings/z_str_cpp.inl"

#ifdef _NIX
struct inconv_stuff_s
{
    iconv_t ucs2_to_ansi = (iconv_t)-1;
    iconv_t ansi_to_ucs2 = (iconv_t)-1;
    iconv_t utf8_to_ucs2 = (iconv_t)-1;
    iconv_t ucs2_to_utf8 = (iconv_t)-1;
    iconv_t utf8_to_ansi = (iconv_t)-1;
    iconv_t ansi_to_utf8 = (iconv_t)-1;

    ~inconv_stuff_s()
    {
        if ( ucs2_to_ansi != (iconv_t)-1 )
            iconv_close( ucs2_to_ansi );
        if ( ansi_to_ucs2 != (iconv_t)-1 )
            iconv_close( ansi_to_ucs2 );
        if ( utf8_to_ucs2 != (iconv_t)-1 )
            iconv_close( utf8_to_ucs2 );
        if ( ucs2_to_utf8 != (iconv_t)-1 )
            iconv_close( ucs2_to_utf8 );
        if ( utf8_to_ansi != (iconv_t)-1 )
            iconv_close( utf8_to_ucs2 );
        if ( ansi_to_utf8 != (iconv_t)-1 )
            iconv_close( ucs2_to_utf8 );

    }

} iconvs;

ZSTRINGS_SIGNED    str_wrap_text_ucs2_to_ansi( char *out, ZSTRINGS_SIGNED maxlen, const wsptr &from )
{
    if ( iconvs.ucs2_to_ansi <= (iconv_t)-1 )
        iconvs.ucs2_to_ansi = iconv_open( "cp1251//IGNORE", "UCS-2" );
    if ( iconvs.ucs2_to_ansi != (iconv_t)-1 )
    {
        size_t isz = from.l * sizeof(wchar);
        size_t osz = maxlen;
        char *f = (char *)from.s;
        iconv( iconvs.ucs2_to_ansi, &f, &isz, &out, &osz );
        if ( isz == 0 )
            return maxlen - osz;
    }
    return 0;
}

ZSTRINGS_SIGNED    str_wrap_text_ansi_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const asptr &from )
{
    if ( iconvs.ucs2_to_ansi <= (iconv_t)-1 )
        iconvs.ucs2_to_ansi = iconv_open( "UCS-2//IGNORE", "cp1251" );
    if ( iconvs.ucs2_to_ansi != (iconv_t)-1 )
    {
        size_t isz = from.l;
        size_t osz = maxlen * sizeof(ZSTRINGS_WIDECHAR);
        char *f = (char *)from.s;
        char *o = (char *)out;
        iconv( iconvs.ucs2_to_ansi, &f, &isz, &o, &osz );
        if ( isz == 0 )
            return maxlen - osz/sizeof(ZSTRINGS_WIDECHAR);
    }
    return 0;
}

ZSTRINGS_SIGNED    str_wrap_text_utf8_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const asptr &from )
{
    if ( iconvs.utf8_to_ucs2 <= (iconv_t)-1 )
        iconvs.utf8_to_ucs2 = iconv_open( "UCS-2//IGNORE", "UTF-8" );
    if ( iconvs.utf8_to_ucs2 != (iconv_t)-1 )
    {
        size_t isz = from.l;
        size_t osz = maxlen * sizeof(ZSTRINGS_WIDECHAR);
        char *f = (char *)from.s;
        char *o = (char *)out;
        iconv( iconvs.utf8_to_ucs2, &f, &isz, &o, &osz );
        if ( isz == 0 )
            return maxlen - osz/sizeof(ZSTRINGS_WIDECHAR);
    }
    return 0;
}

ZSTRINGS_SIGNED    str_wrap_text_ucs2_to_utf8( char *out, ZSTRINGS_SIGNED maxlen, const wsptr &from )
{
    if ( iconvs.ucs2_to_utf8 <= (iconv_t)-1 )
        iconvs.ucs2_to_utf8 = iconv_open( "UTF-8//IGNORE", "UCS-2" );
    if ( iconvs.ucs2_to_utf8 != (iconv_t)-1 )
    {
        size_t isz = from.l * sizeof(ZSTRINGS_WIDECHAR);
        size_t osz = maxlen;
        char *f = (char *)from.s;
        iconv( iconvs.ucs2_to_utf8, &f, &isz, &out, &osz );
        if ( isz == 0 )
            return maxlen - osz;
    }
    return 0;
}

/*
int    str_wrap_text_ansi_to_utf8( char *out, ZSTRINGS_SIGNED maxlen, const asptr &from )
{
    if ( iconvs.ansi_to_utf8 <= (iconv_t)-1 )
        iconvs.ansi_to_utf8 = iconv_open( "UTF-8//IGNORE", "cp1251" );
    if ( iconvs.ansi_to_utf8 != (iconv_t)-1 )
    {
        size_t isz = from.l;
        size_t osz = maxlen;
        char *f = (char *)from.s;
        size_t r = iconv( iconvs.ansi_to_utf8, &f, &isz, &out, &osz );
        if ( isz == 0 )
            return maxlen - osz;
    }
    return 0;
}
*/

int    str_wrap_text_utf8_to_ansi( char *out, ZSTRINGS_SIGNED maxlen, const asptr &from )
{
    if ( iconvs.utf8_to_ansi <= (iconv_t)-1 )
        iconvs.utf8_to_ansi = iconv_open( "cp1251//IGNORE", "UTF-8" );
    if ( iconvs.utf8_to_ansi != (iconv_t)-1 )
    {
        size_t isz = from.l;
        size_t osz = maxlen;
        char *f = (char *)from.s;
        iconv( iconvs.utf8_to_ansi, &f, &isz, &out, &osz );
        if ( isz == 0 )
            return maxlen - osz;
    }
    return 0;
}


#endif

#ifdef _WIN32
ZSTRINGS_SIGNED    str_wrap_text_ucs2_to_ansi(char *out, ZSTRINGS_SIGNED maxlen, const wsptr &from)
{
	if ( (maxlen==0) || (from.l== 0) ) return 0;
	int l = WideCharToMultiByte(CP_ACP,WC_COMPOSITECHECK|WC_DEFAULTCHAR, from.s, from.l, out, maxlen, nullptr, nullptr);
	out[l] = 0;
	return l;

}
ZSTRINGS_SIGNED   str_wrap_text_ansi_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const asptr &from)
{
	if ( (maxlen==0) || (from.l== 0) ) return 0;
	int res = MultiByteToWideChar(CP_ACP,0,from.s,from.l,out,maxlen);
	if (res == 0)
	{
		//DWORD err = GetLastError();
		//const char *err_txt = "unknown error";
		//if (err == ERROR_INSUFFICIENT_BUFFER) err_txt = "ERROR_INSUFFICIENT_BUFFER";
		//else if (err == ERROR_INVALID_FLAGS) err_txt = "ERROR_INVALID_FLAGS";
		//else if (err == ERROR_INVALID_PARAMETER) err_txt = "ERROR_INVALID_PARAMETER";
		//else if (err == ERROR_NO_UNICODE_TRANSLATION) err_txt = "ERROR_NO_UNICODE_TRANSLATION";

		//printf(err_txt);
	} else
	{
		out[ res ] = 0;
	}
    return res;
}
ZSTRINGS_SIGNED    str_wrap_text_utf8_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const asptr &from)
{
	if ( (maxlen==0) || (from.l== 0) ) return 0;
	int res = MultiByteToWideChar(CP_UTF8,0,from.s, from.l,out, maxlen);
	if (res == 0)
	{
		//DWORD err = GetLastError();
		//const char *err_txt = "unknown error";
		//if (err == ERROR_INSUFFICIENT_BUFFER) err_txt = "ERROR_INSUFFICIENT_BUFFER";
		//else if (err == ERROR_INVALID_FLAGS) err_txt = "ERROR_INVALID_FLAGS";
		//else if (err == ERROR_INVALID_PARAMETER) err_txt = "ERROR_INVALID_PARAMETER";
		//else if (err == ERROR_NO_UNICODE_TRANSLATION) err_txt = "ERROR_NO_UNICODE_TRANSLATION";

		//printf(err_txt);
	} else
	{
		out[ res ] = 0;
	}
	return res;
}
ZSTRINGS_SIGNED    str_wrap_text_ucs2_to_utf8(char *out, ZSTRINGS_SIGNED maxlen, const wsptr &from)
{
	ZSTRINGS_SIGNED l = WideCharToMultiByte(CP_UTF8,0, from.s, from.l, out , (maxlen - 1),nullptr,nullptr);
	out[ l ] = 0;
	return l;
}
#endif // _WIN32


bool   str_wrap_text_iequalsw(const ZSTRINGS_WIDECHAR *s1, const ZSTRINGS_WIDECHAR *s2, ZSTRINGS_SIGNED len)
{
	return CSTR_EQUAL == CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE,s1, len,s2, len);
}
bool   str_wrap_text_iequalsa(const char *s1, const char *s2, ZSTRINGS_SIGNED len)
{
	return CSTR_EQUAL == CompareStringA(LOCALE_USER_DEFAULT,NORM_IGNORECASE,s1, len,s2, len);
}

void  str_wrap_text_lowercase( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen)
{
	CharLowerBuffW(out, maxlen);
}
void  str_wrap_text_lowercase(char *out, ZSTRINGS_SIGNED maxlen)
{
	CharLowerBuffA(out, maxlen);
}
void  str_wrap_text_uppercase( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen)
{
	CharUpperBuffW(out, maxlen);
}
void  str_wrap_text_uppercase(char *out, ZSTRINGS_SIGNED maxlen)
{
	CharUpperBuffA(out, maxlen);
}


}
