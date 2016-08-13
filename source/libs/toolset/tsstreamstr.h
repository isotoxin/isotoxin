#pragma once

#include <time.h>

TS_STATIC_CHECK( sizeof( time_t ) == sizeof( uint64 ), "32 bit time_t detected" );

#ifndef _CVTBUFSIZE
#define _CVTBUFSIZE (309 + 40) // # of digits in max. dp value + slop
#endif

namespace ts
{

template<int T> struct streamstr_hex
{
    const uint64 &t;
    streamstr_hex( const uint64 &t ):t(t) {}
};

struct streamstr_time
{
    const time_t &t;
    streamstr_time( const time_t &t ):t(t) {}
private:
    streamstr_time(const streamstr_time&) UNUSED;
    streamstr_time& operator=(streamstr_time &) UNUSED;
};

struct streamstr_text
{
    const char *t;
    int len;
    streamstr_text( const char *t, int len ):t(t), len(len) {}
};

struct endline
{
    endline(){}
};

template<typename STRTYPE> class streamstr
{
public:
    typedef STRTYPE buftype_t;
    typedef typename STRTYPE::TCHAR char_t;
private:
    streamstr(const streamstr &) UNUSED;
    streamstr& operator=(streamstr &) UNUSED;
    buftype_t &boof;

    char_t * boof_current(aint size)
    {
        boof.require_capacity( (int)size + boof.get_length() );

        ZSTRINGS_ASSERT(size + boof.get_length() <= boof.get_capacity(), "Bad arguments");
        if (size + boof.get_length() > boof.get_capacity()) return nullptr;
        return boof.str() + boof.get_length();
    }

    size_t boof_current_size() const
    {
        return boof.get_capacity() - boof.get_length();
    }

    void boof_update_len(const char_t *cur)
    {
        ZSTRINGS_ASSERT((ZSTRINGS_SIGNED)(cur - boof.cstr()) == boof.get_length(), "Bad arguments");
        boof.set_length( boof.get_length() + CHARz_len(cur) );
        ZSTRINGS_ASSERT(boof.get_length() <= boof.get_capacity(), "Corrupt!!!");
    }

    char_t * prepare_fill(aint chars = _CVTBUFSIZE)
    {
        begin();
        char_t *cur = boof_current(1);
        if (!cur) return nullptr;
        boof.require_capacity(boof.get_length() + chars);
        return boof_current(chars);
    }

public:
    const buftype_t& buffer()const { return(boof); }
    buftype_t& buffer() { return(boof); }

    void begin(char c = 0)
    {
        switch(c)
        {
        case '`':
        case '\'':
        case ')':
        case ']':
        case '}':
        case ':':
        case '/':
        case '\\':
        case '.':
        case '\r':
        case '\n':
            return;
        }
        addspaceifneed();
    }
    void raw_append(const char *s)
    {
        boof.append(s);
    }
    void raw_append(const char c)
    {
        boof.append_char(c);
    }
    /*
    uint lb_current_size() { return(boof.current_size()); }
    void lb_update_len(const char *cur) { boof.update_len(cur); }
    */

    template<typename NUM> void append_num( NUM n )
    {
        boof.template append_as_num<NUM>(n);
    }
    template<aint SIZE> void append_num_hex( uint64 n )
    {
        char *cur = boof_current( SIZE * 2 ); if (!cur) return;
        for (aint i=0;i<SIZE;++i)
        {
            set_hex( cur + (SIZE-i-1)*2, (n >> (i*8)) & 0xFF );
        }
        boof.set_length( boof.get_length() + SIZE * 2 );
    }

    static buftype_t & get_static_buffer()
    {
        static buftype_t buffer;

        buffer.clear();
        return buffer;
    }
private:
    void set_hex(char *cc, uint8 b)
    {
        static char table[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        cc[0] = table[b >> 4];
        cc[1] = table[b & 15];
    }

    void addspaceifneed()
    {
        if (boof.get_length() == 0) return;
        char c = boof[ boof.get_length() - 1 ];
        switch(c)
        {
        case '`':
        case '\'':
        case ':':
        case '[':
        case '(':
        case '<':
        case '>':
        case '.':
        case ' ':
        case '=':
        case '-':
        case '_':
        case '+':
        case '/':
        case '\\':
        case '\"':
        case '\n':
        case '\r':
        case '#':
            return;
        }
        boof.append_char(' ');
    }
public:
    explicit streamstr(buftype_t& buf):boof(buf) {}
    explicit streamstr(bool _YES_I_WANT_TO_USE_STATIC_BUFFER_):boof(get_static_buffer()) { ZSTRINGS_ASSERT(_YES_I_WANT_TO_USE_STATIC_BUFFER_); }
    ~streamstr() { }

    streamstr& operator<<(bool n) { begin(); boof.append(n ? CONSTASTR("TRUE") : CONSTASTR("FALSE")); return *this; }
    streamstr& operator<<(short n) { begin(); append_num<int>(n); return *this; }
    streamstr& operator<<(int n) { begin(); append_num<int>(n); return *this; }
    streamstr& operator<<(long n) { begin(); append_num<long,true>(n); return *this; }
    streamstr& operator<<(int64 n) { begin(); append_num<int64,true>(n); return *this; }

    streamstr& operator<<(uint8 n) { begin(); append_num<uint>(n); return *this; }
    streamstr& operator<<(unsigned short n) { begin(); append_num<uint32>(n); return *this; }
    streamstr& operator<<(uint n) { begin(); append_num<uint32>(n); return *this; }
    streamstr& operator<<(unsigned long n) { begin(); append_num<unsigned long>(n); return *this; }
    streamstr& operator<<(uint64 n) { begin(); append_num<uint64>(n); return *this; }

    streamstr& operator<<(float f) { if (char_t *cur = prepare_fill()) { _gcvt_s( cur, boof_current_size(), f, 16 ); boof_update_len(cur); } return *this; }
    streamstr& operator<<(double f) { if (char_t *cur = prepare_fill()) { _gcvt_s( cur, boof_current_size(), f, 16 ); boof_update_len(cur); } return *this; }

    streamstr& operator<<(const endline&) { boof.append(CONSTASTR("\r\n")); return *this; }
    streamstr& operator<<(char c) { begin(c); boof.append_char(c); return *this; }

    streamstr& operator<<(const char* s) { begin(s ? s[0] : 0); boof.append(s ? s : "(null)"); return *this; }
    //debuglog& operator<<(const std::string& s) { begin(s.c_str()[0]); boof.append(s.c_str(), (uint)s.length()); return *this; }
    streamstr& operator<<(const asptr& s) { begin(s.s[0]); boof.append(s); return *this; }
    template<typename TCHARACTER, typename CORE> streamstr& operator<<(const str_t<TCHARACTER, CORE>& s) { begin((char)(s.get_char(0) & 0xff)); boof.appendcvt(s); return *this; }

    streamstr& operator<<(const streamstr_text &tim)
    {
        begin();
        boof.append_char('[').append(tim.t, tim.len).append_char(']');
        return *this;
    }

    streamstr& operator<<(const streamstr_time &tim)
    {
        begin();
        tm today;
        #if defined _MSC_VER
        _localtime64_s(&today, &tim.t);
        #elif defined __GNUC__
        localtime_r(&tim.t, &today);
        #endif
        char_t *cur = prepare_fill(16); if (!cur) return *this;
        strftime(cur, boof_current_size(), "[%Y-%m-%d %H:%M:%S]", &today); boof_update_len(cur);
        return *this;
    }

    streamstr& operator<<(const void* v) { begin(); append_num_hex<sizeof(uint64)>((uint64)v); return *this; }

    template<int T> streamstr& operator<<(const streamstr_hex<T> &v) { begin(); append_num_hex<T>(v.t); return *this; }

    template<class T>
    streamstr& operator<<(const T& t) {

        begin(); boof.append("[");
        const uint8 * data = (const uint8 *)&t;
        size_t size = sizeof(T);
        for(size_t i=0;i<size;++i, ++data)
            append_num_hex<sizeof(uint8)>( *data );
        boof.append("]");
        return *this;
    }
};

#define MAKEASTR(strxxx) (ts::streamstr<ts::str_c>() << strxxx).buffer()
#define MAKEWSTR(strxxx) (ts::streamstr<ts::wstr_c>() << strxxx).buffer()
#define ASHEX(n) ts::streamstr_hex<sizeof(n)>(n)
#define ASTIME(n) ts::streamstr_time(n)
#define ASTEXT(n,l) ts::streamstr_text((n),(l))

} // namespace ts