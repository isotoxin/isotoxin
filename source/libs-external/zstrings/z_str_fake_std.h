#pragma once

#ifdef _WIN32
#include <crtdefs.h>
#endif
#ifdef _NIX
#include <stdlib.h>
#endif // _NIX

#include <malloc.h>
#include <memory.h>

// disable std::string (msvc)
#ifdef _WIN32
#define _STRING_
#define _XSTRING_
#define _CSTDIO_
#define _IOSFWD_
#endif

// disable std::string (g++)
#ifdef _NIX
#define _GLIBCXX_STRING
#define _CHAR_TRAITS_H
#define _LOCALE_CLASSES_H
#define _GLIBCXX_SYSTEM_ERROR
#define _GLIBCXX_IOSFWD
#define _LOCALE_FWD_H
#define _IOS_BASE_H
#define _GLIBXX_STREAMBUF
#define _STREAMBUF_ITERATOR_H
#define _LOCALE_FACETS_H
#define _BASIC_IOS_H
#define _OSTREAM_INSERT_H
#define _GLIBCXX_OSTREAM
#define _GLIBCXX_ISTREAM
#define _STREAM_ITERATOR_H
#define _GLIBCXX_STDEXCEPT
#endif


namespace std
{
    template<class _Elem> struct char_traits
    {
    };

}

#ifdef _WIN32
#include <xutility>
#endif
#ifdef _NIX
#include <type_traits>
#include <iterator>
#endif

#pragma push_macro("CONSTASTR")
#pragma push_macro("CONSTWSTR")
#pragma push_macro("CONSTSTR")
#pragma push_macro("ASPTR_MACRO")
#pragma push_macro("WSPTR_MACRO")
#pragma push_macro("WIDE2")


#pragma push_macro("ZSTRINGS_SYSCALL")
#pragma push_macro("ZSTRINGS_NULL")
#pragma push_macro("ZSTRINGS_CRC32")
#pragma push_macro("ZSTRINGS_FORCEINLINE")
#pragma push_macro("ZSTRINGS_UNREACHABLE")
#pragma push_macro("ZSTRINGS_NAMESPACE")
#pragma push_macro("ZSTRINGS_DEBUG")
#pragma push_macro("ZSTRINGS_ASSERT")
#pragma push_macro("ZSTRINGS_NUMCONVERSION_ERROR")
#pragma push_macro("ZSTRINGS_DEFAULT_STATIC_SIZE")
#pragma push_macro("ZSTRINGS_ALLOCATOR")
#pragma push_macro("_ASSERTE")

#undef _ASSERTE

#undef ZSTRINGS_SYSCALL
#undef ZSTRINGS_NULL
#undef ZSTRINGS_CRC32
#undef ZSTRINGS_FORCEINLINE
#undef ZSTRINGS_UNREACHABLE
#undef ZSTRINGS_NAMESPACE
#undef ZSTRINGS_DEBUG
#undef ZSTRINGS_ASSERT
#undef ZSTRINGS_NUMCONVERSION_ERROR
#undef ZSTRINGS_DEFAULT_STATIC_SIZE
#undef ZSTRINGS_ALLOCATOR


namespace std
{

#define ZSTRINGS_SYSCALL(func) str_wrap_##func
#define ZSTRINGS_NULL nullptr
#define ZSTRINGS_CRC32(p,s) 0
#if defined _MSC_VER
#define ZSTRINGS_FORCEINLINE __forceinline
#define ZSTRINGS_UNREACHABLE __assume(0)
#define ZSTRINGS_ASSERT(...) (1, true)
#elif defined __GNUC__
#define ZSTRINGS_FORCEINLINE inline
#define ZSTRINGS_UNREACHABLE __builtin_unreachable()
#define ZSTRINGS_ASSERT(...) (true)
#endif
#define ZSTRINGS_NAMESPACE
#define ZSTRINGS_DEBUG 0
#define ZSTRINGS_NUMCONVERSION_ERROR(def) //if (!def) ZSTRINGS_ASSERT(false, "num conversion error")

typedef char ZSTRINGS_ANSICHAR;
#ifdef _MSC_VER
typedef wchar_t ZSTRINGS_WIDECHAR;
#endif
#ifdef __GNUC__
typedef char16_t ZSTRINGS_WIDECHAR;
#endif // __GNUC__

typedef unsigned char ZSTRINGS_BYTE;
typedef size_t ZSTRINGS_UNSIGNED; // 32 or 64 bit
typedef long ZSTRINGS_SIGNED; // 32 bit enough
template<typename TCHARACTER> struct sptr;

int     str_wrap_text_ucs2_to_ansi( char *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from );
void    str_wrap_text_ansi_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from );
ZSTRINGS_SIGNED str_wrap_text_utf8_to_ucs2( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from );
ZSTRINGS_SIGNED str_wrap_text_ucs2_to_utf8( char *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_WIDECHAR> &from );
bool    str_wrap_text_iequalsw( const ZSTRINGS_WIDECHAR *s1, const ZSTRINGS_WIDECHAR *s2, ZSTRINGS_SIGNED len );
bool    str_wrap_text_iequalsa( const char *s1, const char *s2, ZSTRINGS_SIGNED len );
void    str_wrap_text_lowercase( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen );
void    str_wrap_text_lowercase( char *out, ZSTRINGS_SIGNED maxlen );
void    str_wrap_text_uppercase( ZSTRINGS_WIDECHAR *out, ZSTRINGS_SIGNED maxlen );
void    str_wrap_text_uppercase( char *out, ZSTRINGS_SIGNED maxlen );

#include "z_str_hpp.inl"

#define _ASSERTE(...) (1, true)

    template<typename TCH> struct string_t : public str_t<TCH>
    {
        typedef str_t<TCH> super;

        const TCH *c_str() const { return super::cstr(); }
        size_t length() const { return super::get_length(); }
        size_t size() const { return super::get_length(); }
        TCH *data() { return super::str(); }
        const TCH *data() const { return super::cstr(); }
        string_t &assign( const TCH *s, size_t len ) { super::set( s, static_cast<int>(len) ); return *this; }
        string_t &assign( const TCH *s ) { super::set( s, CHARz_len( s ) ); return *this; }
        string_t &assign( const string_t& s ) { super::set( s ); return *this; }
        bool empty() const { return super::is_empty(); }
        string_t &resize( size_t sz ) { super::set_length( static_cast<int>(sz) ); return *this; }

        string_t &reserve(size_t sz)
        {
            if (sz > length())
            {
                size_t oldsz = length();
                resize(sz);
                resize(oldsz);
            }
            return *this;
        }

        string_t &erase( size_t i, size_t n = npos )
        {
            if (i > static_cast<size_t>(super::get_length()))
                return *this;
            if (n + i > static_cast<size_t>(super::get_length()))
                n = static_cast<size_t>(super::get_length()) - i;
            super::cut( static_cast<int>(i), static_cast<int>(n) );
            return *this;
        }

        string_t& replace( size_t i, size_t sz, const string_t& s )
        {
            if (sz == npos)
            {
                super::set_length( static_cast<int>(i) ).append( s );
            } else
            {
                super::replace( static_cast<int>(i), static_cast<int>(sz), s.as_sptr() );
            }
            return *this;
        }

        size_t find(const TCH* s) const
        {
            int r = super::find_pos(sptr<TCH>(s));
            if (r < 0) return npos;
            return r;
        }

        size_t find( const string_t& s ) const
        {
            int r = super::find_pos( s.as_sptr() );
            if (r < 0) return npos;
            return r;
        }
        size_t find( const string_t& s, size_t from ) const
        {
            int r = super::find_pos( static_cast<int>(from), s.as_sptr() );
            if (r < 0) return npos;
            return r;
        }

        size_t find( TCH c ) const
        {
            int r = super::find_pos( c );
            if (r < 0) return npos;
            return r;
        }
        size_t find( TCH c, size_t from ) const
        {
            int r = super::find_pos( static_cast<int>(from), c );
            if (r < 0) return npos;
            return r;
        }

        size_t find_first_not_of( const TCH *cc, size_t i ) const
        {
            int r = super::template find_pos_not_of<TCH>( static_cast<int>(i), sptr<TCH>( cc ) );
            if (r < 0) return npos;
            return r;
        }

        string_t substr( size_t i ) const
        {
            return string_t( super::substr( static_cast<int>(i) ).as_sptr() );
        }
        string_t substr( size_t i, size_t len ) const
        {
            if (len >= (super::get_length() - i))
                return string_t( super::substr( static_cast<int>(i) ).as_sptr() );
            return string_t( super::substr( static_cast<int>(i), static_cast<int>(i + len) ).as_sptr() );
        }

        template<typename TT> struct iterator_t
        {
            typedef std::random_access_iterator_tag iterator_category;
            typedef ptrdiff_t difference_type;
            typedef TT * pointer;
            typedef TT& reference;

            TT *p;

            bool operator!=( const iterator_t&o ) const { return p != o.p; }
            bool operator<( const iterator_t&o ) const { return p < o.p; }
            iterator_t &operator--()
            {
                --p;
                return *this;
            }
        };

        struct const_iterator : public iterator_t<const TCH>
        {
            typedef iterator_t<const TCH> super;
            typedef TCH value_type;

            explicit const_iterator( const TCH *p_ )
            {
                super::p = p_;
            }
            const_iterator( iterator_t<char> i )
            {
                super::p = i.p;
            }

            const_iterator &operator=( const_iterator i )
            {
                super::p = i.p;
                return *this;
            }
            const_iterator &operator=( iterator_t<char> i )
            {
                super::p = i.p;
                return *this;
            }

            const_iterator operator+( size_t d ) const
            {
                return const_iterator( super::p + d );
            }

            ptrdiff_t operator-( const_iterator d ) const
            {
                return super::p - d.p;
            }

            const_iterator &operator++()
            {
                ++super::p;
                return *this;
            }


            bool operator!=( const const_iterator&itr )
            {
                return super::p != itr.p;
            }
            bool operator==(const const_iterator&itr)
            {
                return super::p == itr.p;
            }

            TCH operator*() const { return *super::p; }
        };
        struct iterator : public iterator_t<TCH>
        {
			typedef iterator_t<TCH> super;
            typedef TCH value_type;

            iterator &operator=( iterator i )
            {
                super::p = i.p;
                return *this;
            }

            iterator operator+( size_t d ) const
            {
                iterator x;
                x.p = super::p + d;
                return x;
            }
            iterator operator-( size_t d ) const
            {
                iterator x;
                x.p = super::p - d;
                return x;
            }
            iterator &operator++()
            {
                ++super::p;
                return *this;
            }

            iterator operator++( int )
            {
                iterator x;
                x.p = super::p;
                ++super::p;
                return x;
            }

            TCH & operator*() { return *super::p; }

            bool operator==( const iterator&itr )
            {
                return super::p == itr.p;
            }
        };

        struct reverse_iterator
        {
            typedef std::random_access_iterator_tag iterator_category;
            typedef ptrdiff_t difference_type;
            typedef TCH value_type;
            typedef TCH * pointer;
            typedef TCH& reference;

            TCH *p;

            reverse_iterator &operator++()
            {
                --p;
                return *this;
            }
            reverse_iterator operator-( size_t d ) const
            {
                reverse_iterator x;
                x.p = p + d;
                return x;
            }
            reverse_iterator operator+( size_t d ) const
            {
                reverse_iterator x;
                x.p = p - d;
                return x;
            }


            TCH & operator*() { return *(p - 1); }
            bool operator!=( const reverse_iterator&o ) const { return p != o.p; }
        };

        template<typename TT> string_t &erase( iterator_t<TT> it )
        {
            super::cut( static_cast<int>(it.p - super::cstr()), 1 );
            return *this;
        }

        template<typename TT> string_t &erase( iterator_t<TT> first, iterator_t<TT> last )
        {
            super::cut( static_cast<int>(first.p - super::cstr()), static_cast<int>(last.p - first.p) );
            return *this;
        }

        template<typename TT> string_t &assign( iterator_t<TT> f, iterator_t<TT> l ) { set( sptr<TCH>( f.p, static_cast<int>(l.p - f.p) ) ); return *this; }

        string_t(size_t n, bool set0len) : str_t<TCH>(static_cast<int>(n), set0len) {}
        string_t(size_t n, TCH c) { super::fill(static_cast<int>(n), c); }
        string_t(const sptr<TCH>& s1, const sptr<TCH> &s2) : str_t<TCH>(s1, s2) {}
        string_t(const sptr<TCH>& s) : str_t<TCH>(s) {}
        string_t(const pstr_t<TCH>& s) : str_t<TCH>(s.as_sptr()) {}
        string_t(const string_t &s) : str_t<TCH>((const super &)s) {}
        string_t( const TCH *s ) : str_t<TCH>( sptr<TCH>( s ) ) {}
        string_t( const TCH *s, size_t l ) : str_t<TCH>( sptr<TCH>( s, static_cast<int>(l) ) ) {}
        template<typename TT> string_t( iterator_t<TT> f, iterator_t<TT> l ) : str_t<TCH>( sptr<TCH>( f.p, static_cast<int>(l.p - f.p) ) ) {}
        string_t() {}

        operator const char *() const { return super::cstr(); }

        int compare( const TCH *s ) const
        {
            return super::compare( super::as_sptr(), sptr<TCH>( s ) );
        }
        int compare( size_t pos, size_t len, const TCH *s, size_t n ) const
        {
            return super::compare( super::substr( static_cast<int>(pos), static_cast<int>(pos + len) ), sptr<TCH>( s, static_cast<int>(n) ) );
        }
        int compare(size_t pos, size_t len, const TCH *s) const
        {
            return super::compare(super::substr(static_cast<int>(pos), static_cast<int>(pos + len)), sptr<TCH>(s));
        }
        int compare( size_t pos, size_t len, const string_t &s, size_t subpos, size_t sublen ) const
        {
            return super::compare( super::substr( static_cast<int>(pos), static_cast<int>(pos + len) ), s.super::substr( static_cast<int>(subpos), static_cast<int>(subpos + sublen) ) );
        }

        static const size_t npos = static_cast<size_t>(-1);

        typedef size_t size_type;

        const_iterator begin() const { const_iterator i( super::cstr() ); return i; }
        iterator begin() { iterator i; i.p = super::str(); return i; }
        const_iterator end() const { const_iterator i( super::cstr() + super::get_length() ); return i; }
        iterator end() { iterator i; i.p = super::str() + super::get_length(); return i; }

        reverse_iterator rbegin() { reverse_iterator i; i.p = super::str() + super::get_length(); return i; }
        reverse_iterator rend() { reverse_iterator i; i.p = super::str(); return i; }

        string_t &append(const sptr<TCH> &s)
        {
            super::append(s);
            return *this;
        }
        string_t &append(const string_t&s)
        {
            super::append(s.as_sptr());
            return *this;
        }
        string_t &append(const TCH *s)
        {
            super::append(sptr<TCH>(s));
            return *this;
        }
        string_t &append( const TCH *s, size_t l )
        {
            super::append( sptr<TCH>( s, static_cast<int>(l) ) );
            return *this;
        }
        string_t &append(const string_t&s, size_t subpos, size_t sublen)
        {
            if (sublen >= (s.super::get_length() - subpos))
                super::append(s.super::substr(static_cast<int>(subpos)));
            else
                super::append(s.super::substr(static_cast<int>(subpos), static_cast<int>(subpos + sublen)));
            return *this;
        }
        string_t &append( const_iterator i1, const_iterator i2 )
        {
            super::append( sptr<TCH>( i1.p, static_cast<int>(i2.p - i1.p) ) );
            return *this;
        }

        string_t &insert( iterator it, const TCH *p1, const TCH *p2 )
        {
            if (it == end())
                return append( p1, static_cast<int>(p2 - p1) );

            super::insert( static_cast<int>(it.p - super::cstr()), sptr<TCH>( p1, static_cast<int>(p2 - p1) ) );

            return *this;
        }

        string_t &insert( iterator it, TCH c )
        {
            super::insert( static_cast<int>(it.p - super::cstr()), c );
            return *this;
        }

        string_t &insert( size_t pos, size_t n, TCH c )
        {
            super::insert( static_cast<int>(pos), c, static_cast<int>(n) );
            return *this;
        }
        string_t &insert( size_t pos, const string_t&s )
        {
            super::insert( static_cast<int>(pos), s.as_sptr() );
            return *this;
        }

        string_t &operator+=(TCH c)
        {
            super::append_char(c);
            return *this;
        }

        string_t &operator+=(const string_t &s)
        {
            super::append(s.as_sptr());
            return *this;
        }

        string_t &operator+=(const TCH *s)
        {
            super::append( sptr<TCH>(s) );
            return *this;
        }

        bool operator==( const TCH *s ) const
        {
            return blk_equal( s, super::cstr(), super::get_length() * sizeof( TCH ) + sizeof( TCH ) );
        }
        bool operator==( const string_t&s ) const
        {
            return super::equals( s );
        }

        bool operator!=( const TCH *s ) const
        {
            return !blk_equal( s, super::cstr(), super::get_length() * sizeof( TCH ) + sizeof( TCH ) );
        }
        bool operator!=(const string_t&s) const
        {
            return !super::equals(s);
        }

        TCH &operator[](ptrdiff_t i )
        {
            return super::str()[i];
        }
        TCH operator[](ptrdiff_t i) const
        {
            return super::get_char(static_cast<int>(i));
        }

        string_t& push_back( TCH c )
        {
            super::append_char( c );
            return *this;
        }

        string_t& operator=(const sptr<TCH> &s)
        {
            super::set(s);
            return *this;
        }
        string_t& operator=(const TCH *s)
        {
            super::set(sptr<TCH>(s));
            return *this;
        }
        string_t& operator=(const string_t &s)
        {
            super::set( static_cast<const super &>(s) );
            return *this;
        }

    };

    typedef string_t<ZSTRINGS_ANSICHAR> string;
    typedef string_t<ZSTRINGS_WIDECHAR> wstring;

    template<typename TCH> ZSTRINGS_FORCEINLINE string operator+(const TCH *s1, const string_t<TCH>&s2)
    {
        return string_t<TCH>(sptr<TCH>(s1), s2.as_sptr());
    }
    template<typename TCH> ZSTRINGS_FORCEINLINE string operator+(const string_t<TCH>&s1, const TCH *s2)
    {
        return string_t<TCH>(s1.as_sptr(), sptr<TCH>(s2));
    }
    template<typename TCH> ZSTRINGS_FORCEINLINE string operator+( const string_t<TCH>&s1, const string_t<TCH>&s2 )
    {
        return string_t<TCH>( s1.as_sptr(), s2.as_sptr() );
    }
    template<typename TCH> ZSTRINGS_FORCEINLINE string operator+(const string_t<TCH>&s1, TCH s2)
    {
        return string_t<TCH>(s1.as_sptr(), sptr<TCH>( &s2, 1 ));
    }
    template<typename TCH> ZSTRINGS_FORCEINLINE string operator+(TCH s1, const string_t<TCH>&s2)
    {
        return string_t<TCH>(sptr<TCH>(&s1, 1), s2.as_sptr());
    }

    ZSTRINGS_FORCEINLINE ptrdiff_t distance( string::iterator i1, string::iterator i2 )
    {
        return i2.p - i1.p;
    }
    ZSTRINGS_FORCEINLINE ptrdiff_t distance( string::reverse_iterator i1, string::reverse_iterator i2 )
    {
        return i1.p - i2.p;
    }

    ZSTRINGS_FORCEINLINE void copy( const char *f, const char *l, string::iterator itr )
    {
        blk_copy_fwd( itr.p, f, (l - f) * sizeof(char) );
    }

    struct ostringstream
    {
        string s;

        ostringstream &operator <<( unsigned short v )
        {
            s.append_as_num( v );
            return *this;
        }
        const string &str() const { return s; }
    };

    template<typename T1, typename T2> class basic_ostream
    {
    };

    typedef ZSTRINGS_WIDECHAR widechar;
} // namespace std


#pragma pop_macro("CONSTASTR")
#pragma pop_macro("CONSTWSTR")
#pragma pop_macro("CONSTSTR")
#pragma pop_macro("ASPTR_MACRO")
#pragma pop_macro("WSPTR_MACRO")
#pragma pop_macro("WIDE2")

#define STD_ASPTR_MACRO(x,y) std::asptr(x,y)

#if defined _MSC_VER
#define STD_WSPTR_MACRO(x,y) std::wsptr(L##x,y)
#define STD_S( tc, s ) std::_to_char_or_not_to_char<tc>::get( s, L##s, sizeof(s)-1 )
#define STD_WIDE2(s) L##s
#elif defined __GNUC__
#define STD_WSPTR_MACRO(x,y) std::wsptr(u##x,y)
#define STD_S( tc, s ) std::_to_char_or_not_to_char<tc>::get( s, u##s, sizeof(s)-1 )
#define STD_WIDE2(s) u##s
#endif

#define STD_ASTR( s ) STD_ASPTR_MACRO( s, sizeof(s)-1 )
#define STD_WSTR( s ) STD_WSPTR_MACRO( s, sizeof(s)-1 )

#pragma pop_macro("ZSTRINGS_SYSCALL")
#pragma pop_macro("ZSTRINGS_NULL")
#pragma pop_macro("ZSTRINGS_CRC32")
#pragma pop_macro("ZSTRINGS_FORCEINLINE")
#pragma pop_macro("ZSTRINGS_UNREACHABLE")
#pragma pop_macro("ZSTRINGS_NAMESPACE")
#pragma pop_macro("ZSTRINGS_DEBUG")
#pragma pop_macro("ZSTRINGS_ASSERT")
#pragma pop_macro("ZSTRINGS_NUMCONVERSION_ERROR")
#pragma pop_macro("ZSTRINGS_DEFAULT_STATIC_SIZE")
#pragma pop_macro("ZSTRINGS_ALLOCATOR")
#pragma pop_macro("_ASSERTE")

#ifndef _ASSERTE
#define _ASSERTE(...) ;
#endif