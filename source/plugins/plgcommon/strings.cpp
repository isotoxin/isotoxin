#include "stdafx.h"

#if 0
#include "zstrings/z_str_cpp.inl"
#endif

namespace std
{

//int    str_wrap_text_ucs2_to_ansi(char *out, long maxlen, const wsptr &from)
//{
//	if ( (maxlen==0) || (from.l== 0) ) return 0;
//	int l = WideCharToMultiByte(CP_ACP,WC_COMPOSITECHECK|WC_DEFAULTCHAR, from.s, from.l, out, maxlen, nullptr, nullptr);
//	out[l] = 0;
//	return l;
//}
void   str_wrap_text_ansi_to_ucs2( widechar *out, long maxlen, const asptr &from)
{
	if ( (maxlen==0) || (from.l== 0) ) return;
	int res = MultiByteToWideChar(CP_ACP,0,from.s, from.l,out, maxlen);
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
}
bool   str_wrap_text_iequalsa(const char *s1, const char *s2, long len)
{
    return CSTR_EQUAL == CompareStringA(LOCALE_USER_DEFAULT, NORM_IGNORECASE, s1, len, s2, len);
}
long    str_wrap_text_ucs2_to_utf8(char *out, long maxlen, const wsptr &from)
{
    long l = WideCharToMultiByte(CP_UTF8, 0, from.s, from.l, out, maxlen - 1, nullptr, nullptr);
    out[l] = 0;
    return l;
}
}

#if 0
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
bool   str_wrap_text_iequalsw(const ZSTRINGS_WIDECHAR *s1, const ZSTRINGS_WIDECHAR *s2, ZSTRINGS_SIGNED len)
{
	return CSTR_EQUAL == CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE,s1, len,s2, len);
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

#endif