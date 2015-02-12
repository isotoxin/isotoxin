#pragma once

namespace ts
{

template <typename TCHARACTER, class S_CORE = str_core_copy_on_demand_c<TCHARACTER> > class strings_t
{
    typedef str_t<TCHARACTER, S_CORE> strtype;
    typedef strings_t<TCHARACTER, S_CORE> arrtype;

    array_inplace_t<strtype, 8> m_vect;

public:

    strings_t() {};
    strings_t(const arrtype &othera):m_vect( othera.size() )
    {
        for (const strtype&s :othera)
            add( s );
    }

    strings_t(const void *buf, aint bufsize, bool allow_empty_lines)
    {
        const TCHARACTER * s = (TCHARACTER *)buf;
        int i0 = 0;
        int i1 = 0;
        for(;;)
        {
            if (s[i1] == 13 || s[i1] == 10)
            {
                if (i0 < i1)
                {
                    add( strtype( s + i0, i1 - i0 ) );
                } else if (allow_empty_lines && i0==i1 && s[i1] == 13)
                    add( strtype() );
                ++i1;
                i0 = i1;
                continue;
            }
            if (s[i1] == 0 || i1 >= bufsize)
            {
                if (i0 < i1)
                {
                    add( strtype( s + i0, i1 - i0 ) );
                } else if (allow_empty_lines && i0==i1)
                    add( strtype() );

                break;
            }
            ++i1;
        }
    }

    strings_t(const strtype &str, TCHARACTER separator = 32)
    {
        if (str.get_length() == 0)  return;
        int i = 0;
        for(;;)
        {
            int k = str.find_pos(i,separator);
            if (k < 0)
            {
                
                add(str.substr(i));
                break;
            }
            else
                add(str.substr(i,k));
            i = k + 1;
        }
    }
    strings_t(const sptr<TCHARACTER> &str, TCHARACTER separator)
    {
        if (str.l == 0)  return;
        int i = 0;
        for(;;)
        {
            int k = CHARz_findn(str.s+i,separator,str.l);
            if (k < 0)
            {
                add( str.skip(i) );
                break;
            }
            else
                add(sptr<TCHARACTER>(str.s + i, k));
            i += k + 1;
        }
    }
    strings_t(const strtype &str, const TCHARACTER *separator)
    {
        if (str.get_length() == 0)  return;

        int spll = CHARz_len(separator);

        int i = 0;
        for(;;)
        {
            int k = str.get_index(i,separator, spll);
            if (k < 0)
            {

                add(str.substr(i));
                break;
            }
            else
                add(str.substr(i,k));
            i = k + spll;
        }
    }
    strings_t(const strtype &str, const strtype &separator)
    {
        if (str.get_length() == 0)  return;

        int i = 0;
        for(;;)
        {
            int k = str.get_index(i,separator);
            if (k < 0)
            {

                add(str.substr(i));
                break;
            }
            else
                add(str.substr(i,k));
            i = k + separator.get_length();
        }
    }
    ~strings_t() {}

    strtype  split_by_multichar(const strtype &str, const TCHARACTER *separator)
    {
        strtype spls;
        clear();
        if (str.get_length() == 0)  return spls;
        int i = 0;
        for(;;)
        {
            int k = str.get_index_multi(i,separator);
            if (k < 0)
            {
                add(str.substr(i));
                break;
            }
            else
            {
                spls.append_char(str.get_char(k));
                add(str.substr(i,k));
            }
            i = k + 1;
        }
        return spls;
    }

    void qsplit(const sptr<TCHARACTER> &str)
    {
        clear();
        if (str.l == 0)  return;
        int i = 0;
        int bg = -1;
        bool quote = false;
        for(int l = str.l;l>0;++i,--l)
        {
            TCHARACTER ch = str.s[i];
            if (bg < 0)
            {
                if ( ch == ' ' ) continue;
                bg = i;
                quote = ch == '\"';

            } else
            {
                if ( quote )
                {
                    if ( ch == '\"' )
                    {
                        add( sptr<TCHARACTER>( str.s+bg+1,i-bg-1) );
                        bg = -1;
                    }
                } else
                {
                    if ( ch == ' ' )
                    {
                        add(sptr<TCHARACTER>( str.s + bg,i-bg));
                        bg = -1;
                    }
                }
            }
        }
        if (bg >= 0)
        {
            if (quote)
            {
                add( sptr<TCHARACTER>( str.s+bg+1,i-bg-1) );
            }
            else
            {
                add(sptr<TCHARACTER>(str.s + bg, i - bg));
            }
        }
    }

    void bsplit(const strtype &str, TCHARACTER b0, TCHARACTER b1, bool cutb = true )
    {
        clear();
        if (str.get_length() == 0)  return;
        int i = 0;
        int bg = -1;
        int bra = 0;
        for(;;++i)
        {
            TCHARACTER ch = str.get_char(i);
            if ( ch == 0 )
            {
                if ( bg >= 0 )
                {
                    if ( bra > 0 && cutb )
                    {
                        add(str.substr(bg+1,i));
                    } else
                    {
                        add(str.substr(bg,i));
                    }
                }
                break;
            }
            if (bg < 0)
            {
                if ( ch == ' ' ) continue;
                bg = i;
                if (ch == b0) ++bra;

            } else
            {
                if ( bra > 0 )
                {
                    if (ch == b0) ++bra; else
                    if ( ch == b1 )
                    {
                        --bra;
                        if (bra == 0)
                        {
                            if (cutb)
                            {
                                add(str.substr(bg+1,i));
                            } else
                            {
                                add(str.substr(bg,i+1));
                            }
                            bg = -1;
                        }
                    }
                } else
                {
                    if ( ch == ' ' )
                    {
                        add(str.substr(bg,i));
                        bg = -1;
                    }
                }
            }
        }
    }

    template<typename TCHARACTER2> void split(const sptr<TCHARACTER2> &_str, TCHARACTER2 separator)
    {
        clear();
        pstr_t<TCHARACTER2> str( _str );
        if (str.get_length() == 0)  return;
        int i = 0;
        for(;;)
        {
            int k = str.find_pos(i,separator);
            ASSERT(k < str.get_length());
            if (k < 0)
            {
                add(str.substr(i));
                break;
            }
            else
                add(str.substr(i,k));
            i = k + 1;
        }
    }

    template<typename TCHARACTER2> void split(const sptr<TCHARACTER2> &_str, const sptr<TCHARACTER2> &separator)
    {
        //str.trim();
        clear();
        pstr_t<TCHARACTER2> str( _str );

        if (str.get_length() == 0)  return;
        int i = 0;
        for(;;)
        {
            int k = str.get_index(i,separator);
            if (k < 0)
            {
                add(str.substr(i));
                break;
            }
            else
                add(str.substr(i,k));
            i = k + separator.l;
        }
    }

    strtype    join_multi( const TCHARACTER *multi, int begin = 0, int end = -1 ) const
    {
        strtype str;
        const int sz = (end < 0) ? size() : end;
        for (int k = begin; k<sz; ++k)
        {
            str.append( get(k) );
            if (k < (sz-1))
            {
                if (*multi == 0) return str;
                str.append_char( *multi );
                ++multi;
            }
        }
        return str;
    }


    strtype    join( TCHARACTER separator, int begin = 0, int end = -1 ) const
    {
        strtype str;
        const int sz = (end < 0) ? size() : end;
        for (int k = begin; k<sz; ++k)
        {
            str.append( get(k) );
            if (k < (sz-1)) str.append_char( separator );
        }
        return str;
    }

    strtype    join( const TCHARACTER *joincharz ) const
    {
        strtype str;
        const int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            str.append( get(k) );
            if (k < (sz-1)) str.append( joincharz );
        }
        return str;
    }

    template <int N> void adda( const strtype s[N] )
    {
        for (int i=0;i<N;++i)
        {
            add( s[i] );
        }
    }

    template<typename TCHARACTER2, typename CORE2> int     add(const str_t<TCHARACTER2, CORE2> &s)
    {
		int r = m_vect.size();
		m_vect.add().set(s);
		return r;
    };
    int     add(const sptr<TCHARACTER> &s, bool casedown = false)
    {
		strtype &ss = m_vect.add();
		ss.set(s);
        if ( casedown )
            ss.case_down();
        return m_vect.size() - 1;
    };
    template<typename TCHARACTER2, class CORE2> void    insert(uint i, const str_t<TCHARACTER2, CORE2> &s)
    {
        m_vect.insert(i) = s;
    }

	template<typename TCHARACTER2, class CORE2> void    set( int index, const str_t<TCHARACTER2, CORE2> &s )
	{
		m_vect.get(index) = s;
	}

    bool operator==( const arrtype &a ) const
    {
        int cnt = size();
        if (a.size() != size()) return false;
        for (int i=0;i<cnt;++i)
        {
            if (!a.get(i).equals(get(i))) return false;
        }
        return true;
    }
    bool operator!=( const arrtype &a ) const
    {
        int cnt = size();
        if (a.size() != size()) return true;
        for (int i=0;i<cnt;++i)
        {
            if (!a.get(i).equals(get(i))) return true;
        }
        return false;
    }

    strtype & operator[] (uint index)
    {
        while (index >= size())
			m_vect.add();

        return m_vect.get( index );
    }

    
    int     find_sorted_descending(const TCHARACTER *text) const
    {
        int index;
        if (m_vect.find_sorted( index, text )) return index;
        return -1;
    }
    int     find_sorted_descending(const strtype &text) const
    {
        int index;
        if (m_vect.find_sorted( index, text )) return index;
        return -1;
    }

    int     find(const sptr<TCHARACTER> &text) const
    {
        int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            if (get(k).equals(text)) return k;
        }
        return -1;
    }
    template<typename CORE2> int     find(const str_t<TCHARACTER, CORE2> &text) const { return find(text.as_spart()); }

    int     find_ignore_case(const sptr<TCHARACTER> &text) const
    {
        int sz = size();
        for (int k = 0; k < sz; ++k)
        {
            if (get(k).equals_ignore_case(text)) return k;
        }
        return -1;
    }

    template<typename CORE2> int     find_ignore_case(const str_t<TCHARACTER, CORE2> &text) const { return find_ignore_case(text.as_spart()); }

    bool    present_any_of(const arrtype &a) const
    {
        int cnt = a.size();
        for (int i=0;i<cnt;++i)
        {
            if (find(a.get(i)) >= 0) return true;
        }
        return false;
    }

    void    copyin(aint i, const arrtype &a)
    {
        aint sz = a.size();
        m_vect.expand(i,sz);
        for (int k = 0; k<sz; ++k)
        {
            m_vect.set(i+k, a.get(k));
        }
    }

    void    remove_fast_by(const arrtype &a)
    {
        int cnt = size();
        for (int i=0;i<cnt;)
        {
            if (a.find(get(i)) >= 0)
            {
                remove_fast(i);
                --cnt;
                continue;
            }
            ++i;
        }
    }
    

    void    moveup(const uint i)    
    { 
        if ((i > 0) && (i < m_vect.size()))
        {
            m_vect.move_up_unsafe(i);
        }
    }
    void    movedown(const uint i)  
    { 
        if ((int)i < (int)m_vect.size()-1)
        {
            m_vect.move_down_unsafe(i); 
        }
    }

    void    swap_unsafe(const uint i, const uint j)  
    {
        m_vect.swap_unsafe(i,j);
    }

    
    void    case_down(void)
    {
        int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            m_vect.get(k)->case_down();
        }
    }

    void    case_up(void)
    {
        int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            m_vect.get(k)->case_up();
        }
    }

    void    trim(void)
    {
        int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            m_vect.get(k).trim();
        }
    }
    void    trim(char c)
    {
        int sz = size();
        for (int k = 0; k<sz; ++k)
        {
            m_vect.get(k)->trim(c);
        }
    }

    void    remove_slow(aint i)
    {
        m_vect.remove_slow(i);
    }
    void    remove_slow(aint i, aint count)
    {
        m_vect.remove_some(i, count);
    }
    void    remove_slow(const sptr<TCHARACTER> &s)
    {
        int i = find( s );
        if (i>=0) m_vect.delete_slow(i);
    }

    void    remove_fast(aint i)
    {
        m_vect.remove_fast(i);
    }

    bool remove_fast(const sptr<TCHARACTER> &s)
    {
        int i = find( s );
        if (i>=0)
        {
            m_vect.remove_fast(i);
            return true;
        }
        return false;
    }

    void truncate(aint i)
    {
        m_vect.truncate(i);
    }

    int    size(void) const {return m_vect.size();};

    template <int N> void geta(strtype ar[N], uint i) const
    {
#ifndef _FINAL
        if (i > (size()-N))
        {
#pragma warning (push)
#pragma warning (disable : 4127)
            ERROR( tmp_str_c("geta: out of range of string array <").append(join('|')).append(">").cstr() );
#pragma warning (pop)

        }
#endif
        for (int k=0;k<N;++k)
        {
            ar[k] = m_vect[i+k];
        }
    }

    const strtype & get(int i) const
    {
        if (ASSERT(i >= 0 && i < size(), "get: out of range of string array <" << join('|') << ">"))
        {
            return m_vect[i];
        }
        return make_dummy<strtype>();
    }

    strtype & get(int i)
    {
#ifndef _FINAL
        if (i >= size())
        {
#pragma warning (push)
#pragma warning (disable : 4127)
            ERROR( tmp_str_c("get: out of range of string array <").append(join('|')).append(">").cstr() );
#pragma warning (pop)
        }
#endif
        return m_vect[i];
    }

    // pool funtions

    const TCHARACTER *get_string_pointer(const sptr<TCHARACTER> &of) // dangerous! do not store pointer
    {
        int index = find(of);
        if (index < 0)
        {
            add(of);
            return get(size() - 1).cstr();
        }
        return get(index).cstr();
    }
    template<typename CORE2> const TCHARACTER *get_string_pointer(const str_t<TCHARACTER, CORE2> &of) {return get_string_pointer(of.as_spart());}

    int get_string_index(const sptr<TCHARACTER> &of)
    {
        int index = find(of);
        if (index < 0)
        {
            add(of);
            return size() - 1;
        }
        return index;
    }

    template<typename CORE2> int get_string_index(const str_t<TCHARACTER, CORE2> &of) {return get_string_index(of.as_spart());}

    void kill_empty_fast(void)
    {
        int i = 0;
        for(;i<m_vect.size();)
        {
            if (m_vect.get(i).is_empty())
            {
                m_vect.remove_fast(i);
                continue;
            }
            ++i;
        }
    }

    void kill_empty_slow(int startindex = 0)
    {
        int i = startindex;
        for(;i<m_vect.size();)
        {
            if (m_vect.get(i).is_empty())
            {
                m_vect.remove_slow(i);
                continue;
            }
            ++i;
        }
    }

    void clear(void)
    {
        m_vect.truncate(0);
    }
    void clear(int index)
    {
        m_vect.get(index)->clear();
    }

    void sort( bool ascending )
    {
        if (ascending)
        {
            m_vect.sort([](const strtype &u1, const strtype &u2)->bool
            {
                return 0 > strtype::compare(u1, u2);
            });
        } else
        {
            m_vect.sort([](const strtype &u1, const strtype &u2)->bool
            {
                return 0 < strtype::compare(u1, u2);
            });
        }
    }

	template <typename T> void sort(T &sorter)
	{
		m_vect.sort(sorter);
	}

    void absorb( arrtype &othera )
    {
        m_vect.absorb( othera.m_vect );
    }

    int compare( const arrtype &othera ) const
    {
        int cnt = imin( size(), othera.size() );
        for (int i=0;i<cnt;++i)
        {
            int c = strtype::compare( get(i), othera.get(i) );
            if (c != 0) return c;
        }
        return isign( (int)size() - (int)othera.size() );
    }

    void merge_and_sort( const arrtype &othera )
    {
        int cnt = (int)othera.size();
        for (int i = 0; i < cnt; ++i)
        {
            add( othera.get(i) );
        }
        sort(true);
        for (int i=size()-2;i>=0;--i)
        {
            if ( get(i).equals(get(i+1)) )
            {
                remove_slow(i);
            }
        }
    }

	arrtype & operator = ( const arrtype &othera )
	{
        int cc = imin( (int)size(), (int)othera.size() );
        for (int i=0;i<cc;++i)
        {
            m_vect.get(i)->set( othera.get(i) );
        }
        if ( cc == (int)othera.size() )
        {
            m_vect.truncate( cc );
        } else
        {

            int cnt = (int)othera.size();
            for (int i = cc; i < cnt; ++i)
            {
                add( othera.get(i) );
            }
        }
        return *this;
	}

    const strtype& last() const
    {
        return m_vect.last();
    }

    // begin() end() semantics

    strtype * begin() { return m_vect.begin(); }
    const strtype *begin() const { return m_vect.begin(); }

    strtype *end() { return m_vect.end(); }
    const strtype *end() const { return m_vect.end(); }

};

typedef strings_t<char> astrings_c;
typedef strings_t<wchar> wstrings_c;

} // namespace ts

