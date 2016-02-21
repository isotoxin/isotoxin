#include "hunspell_io.h"

__inline void    blk_copy_fwd(void *tgt, const void *src, size_t size)
{
    unsigned char *btgt = (unsigned char *)tgt;
    const unsigned char *bsrc = (const unsigned char *)src;
    while (size)
    {
        *btgt++ = *bsrc++;
        size--;
    }
}

__inline int CHARz_nlen(const char *s, int n)
{
    int l = -1;
    do { l++; } while (*(s + l) && l < n);
    return l;
}


size_t hunspell_file_s::read( void *buffer, size_t s )
{
    size_t csz = size - ptr;
    if (csz > s) csz = s;
    blk_copy_fwd(buffer, data + ptr, csz);
    ptr += csz;
    return csz;
}

__forceinline bool is_nl( char c )
{
    return c == '\r' || c == '\n';
}

char *hunspell_file_s::gets( char *str, int n )
{
    char *tgt = str;
    const char *src = (const char *)data + ptr;
    const char *send = (const char *)data + size;
    if (src == send)
    {
        *str = 0;
        return nullptr;
    }
    for( ;n > 1 && src < send; ++tgt, ++src, --n )
    {
        char c = *src;
        if (c == '\n' || c == '\r')
        {
            *tgt = 0;
            ++src;
            for( ;src < send && is_nl(*src); ++src );
            ptr = src - (const char *)data;
            return str;
        }
        *tgt = c;
    }
    *tgt = 0;
    ptr = src - (const char *)data;
    
    return str;
}
