#include "hunspell_io.h"

#ifdef __GNUC__
#define __forceinline inline
#endif

__forceinline bool is_nl( char c, char breakchar )
{
    if ( breakchar ) return breakchar == c;
    return c == '\r' || c == '\n';
}

bool hunspell_file_s::gets( size_t&ptr, std::string &str, char breakchar )
{
    const char *src0 = (const char *)data + ptr;
    const char *src = src0;
    const char *send = (const char *)data + size;
    if (src == send)
    {
        str.clear();
        return false;
    }
    for( ;src < send; ++src )
    {
        if ( is_nl ( *src, breakchar ) )
        {
            str.assign( src0, src - src0 );

            ++src;
            for( ;src < send && is_nl(*src, breakchar); ++src );
            ptr = src - (const char *)data;
            return true;
        }
    }

    str.assign( src0, src - src0 );
    ptr = src - (const char *)data;
    
    return true;
}
