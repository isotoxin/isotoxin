#include <malloc.h>

ZSTRINGS_SIGNED  text_utf8_to_ansi(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from)
{
    ZSTRINGS_WIDECHAR *buffer = (ZSTRINGS_WIDECHAR *)_alloca( (maxlen + 1) * sizeof(ZSTRINGS_WIDECHAR) );
    ZSTRINGS_SIGNED l = ZSTRINGS_SYSCALL(text_utf8_to_ucs2)( buffer, maxlen, from );
    return ZSTRINGS_SYSCALL(text_ucs2_to_ansi)( out, maxlen, wsptr(buffer, l) );
}

///////////////////////////////////////////////////////////////////////////////


bool blk_getnextstr(ZSTRINGS_ANSICHAR *kuda,ZSTRINGS_UNSIGNED lim, str_iwb_s *sb)
{
    ZSTRINGS_BYTE *src = (ZSTRINGS_BYTE *)sb->buff;
    if (!lim) { return true; }
    if (lim==1) { *kuda = 0; return true; }
    for(;;)
    {
        if (!(*src)) { *kuda = 0; return false; }
        if (*src == 10) break;
        if (*src == 13) break;

        *kuda++ = (ZSTRINGS_ANSICHAR)*src++;
        lim--;
        if (lim == 1) { *kuda = 0; return true; }

    };
    *kuda = 0;
    while ((*src == 10)||(*src == 13)) src++;
    sb->buff = src;
    return true;
}

