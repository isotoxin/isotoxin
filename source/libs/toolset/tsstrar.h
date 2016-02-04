#pragma once

namespace ts
{


template <typename TCHARACTER, class S_CORE = str_core_copy_on_demand_c<TCHARACTER> > class strings_t : public array_inplace_t<str_t<TCHARACTER, S_CORE>, 8>
{
    typedef str_t<TCHARACTER, S_CORE> strtype;
    typedef strings_t<TCHARACTER, S_CORE> arrtype;

public:

    strings_t() {};
    strings_t(const arrtype &othera):array_inplace_t<str_t<TCHARACTER, S_CORE>, 8>( othera.size() )
    {
        for (const strtype&s :othera)
            add( s );
    }
    strings_t(arrtype &&othera) :array_inplace_t<str_t<TCHARACTER, S_CORE>, 8>(std::move(othera))
    {
    }

    strings_t(const void *buf, aint bufsize, bool allow_empty_lines)
    {
        const TCHARACTER * s = (TCHARACTER *)buf;
        aint i0 = 0;
        aint i1 = 0;
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

    strings_t(const strtype &str, TCHARACTER separator)
    {
        if (str.get_length() == 0)  return;
        aint i = 0;
        for(;;)
        {
            aint k = str.find_pos(i,separator);
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
        aint i = 0;
        for(;;)
        {
            aint k = CHARz_findn(str.s+i,separator,str.l-i);
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
    strings_t(const strtype &str, const sptr<TCHARACTER> &separator)
    {
        if (str.get_length() == 0)  return;

        aint i = 0;
        for(;;)
        {
            aint k = str.find_pos(i, separator);
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
    strings_t(const strtype &str, const strtype &separator)
    {
        if (str.get_length() == 0)  return;

        aint i = 0;
        for(;;)
        {
            aint k = str.get_index(i,separator);
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

    strtype  split_by_multichar(const strtype &str, const sptr<TCHARACTER> &separator)
    {
        strtype spls;
        clear();
        if (str.get_length() == 0)  return spls;
        aint i = 0;
        for(;;)
        {
            aint k = str.find_pos_of(i,separator);
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
        aint i = 0;
        aint bg = -1;
        bool quote = false;
        for(aint l = str.l;l>0;++i,--l)
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
        aint i = 0;
        aint bg = -1;
        aint bra = 0;
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
        aint i = 0;
        for(;;)
        {
            aint k = str.find_pos(i,separator);
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
        aint i = 0;
        for(;;)
        {
            aint k = str.get_index(i,separator);
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

    strtype    join_multi( const TCHARACTER *multi, aint begin = 0, aint end = -1 ) const
    {
        strtype str;
        const aint sz = (end < 0) ? size() : end;
        for (aint k = begin; k<sz; ++k)
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


    strtype    join( TCHARACTER separator, aint begin = 0, aint end = -1 ) const
    {
        strtype str;
        const aint sz = (end < 0) ? size() : end;
        for (aint k = begin; k<sz; ++k)
        {
            str.append( get(k) );
            if (k < (sz-1)) str.append_char( separator );
        }
        return str;
    }

    strtype    join( const sptr<TCHARACTER> &joincharz ) const
    {
        strtype str;
        aint sz = size();
        for (aint k = 0; k<sz; ++k)
        {
            str.append( get(k) );
            if (k < (sz-1)) str.append( joincharz );
        }
        return str;
    }

    template <int N> void adda( const strtype s[N] )
    {
        for (aint i=0;i<N;++i)
            add( s[i] );
    }

    template<typename CORE2> aint add(const str_t<TCHARACTER, CORE2> &s)
    {
        aint r = size();
        __super::add().set(s);
        return r;
    };

    void add(const arrtype &a)
    {
        for (const strtype &s : a)
            add(s);
    };

    template<typename TCHARACTER2, typename CORE2> aint add(const str_t<TCHARACTER2, CORE2> &s)
    {
		aint r = size();
		__super::add().setcvt(s);
		return r;
    };
    aint add(const sptr<TCHARACTER> &s, bool casedown = false)
    {
		strtype &ss = __super::add();
		ss.set(s);
        if ( casedown )
            ss.case_down();
        return size() - 1;
    };

    template<class CORE2> void    insert(aint i, const str_t<CORE2> &s)
    {
        __super::insert(i).set(s);
    }

    template<typename TCHARACTER2, class CORE2> void    insert( aint i, const str_t<TCHARACTER2, CORE2> &s)
    {
        __super::insert(i).setcvt(s);
    }
    template<class CORE2> void    set(aint index, const str_t<CORE2> &s)
    {
        get(index).set(s);
    }

	template<typename TCHARACTER2, class CORE2> void    set( aint index, const str_t<TCHARACTER2, CORE2> &s )
	{
		get(index).setcvt(s);
	}

    bool operator==( const arrtype &a ) const
    {
        aint cnt = size();
        if (a.size() != cnt) return false;
        for (aint i=0;i<cnt;++i)
            if (!a.get(i).equals(get(i))) return false;
        return true;
    }
    bool operator!=( const arrtype &a ) const
    {
        aint cnt = size();
        if (a.size() != cnt) return true;
        for (aint i=0;i<cnt;++i)
            if (!a.get(i).equals(get(i))) return true;
        return false;
    }

    strtype & operator[] (aint index)
    {
        while (index >= size())
			add();

        return get( index );
    }

    
    aint find_sorted_descending(const TCHARACTER *text) const
    {
        aint index;
        if (__super::find_sorted( index, text )) return index;
        return -1;
    }
    aint find_sorted_descending(const strtype &text) const
    {
        aint index;
        if (__super::find_sorted( index, text )) return index;
        return -1;
    }

    aint find(const sptr<TCHARACTER> &text) const
    {
        aint sz = size();
        for (aint k = 0; k<sz; ++k)
            if (get(k).equals(text))
                return k;
        return -1;
    }
    template<typename CORE2> aint find(const str_t<TCHARACTER, CORE2> &text) const { return find(text.as_spart()); }

    aint find_ignore_case(const sptr<TCHARACTER> &text) const
    {
        aint sz = size();
        for (aint k = 0; k < sz; ++k)
            if (get(k).equals_ignore_case(text))
                return k;
        return -1;
    }

    template<typename CORE2> int     find_ignore_case(const str_t<TCHARACTER, CORE2> &text) const { return find_ignore_case(text.as_spart()); }

    bool    present_any_of(const arrtype &a) const
    {
        aint cnt = a.size();
        for (aint i=0;i<cnt;++i)
        {
            if (find(a.get(i)) >= 0) return true;
        }
        return false;
    }

    void    copyin(aint i, const arrtype &a)
    {
        aint sz = a.size();
        expand(i,sz);
        for (aint k = 0; k<sz; ++k)
        {
            set(i+k, a.get(k));
        }
    }

    void    remove_fast_by(const arrtype &a)
    {
        aint cnt = size();
        for (aint i=0;i<cnt;)
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
    

    void    moveup(aint i)    
    { 
        if ((i > 0) && (i < size()))
        {
            move_up_unsafe(i);
        }
    }
    void    movedown(aint i)  
    { 
        if (i < size()-1)
        {
            move_down_unsafe(i); 
        }
    }

    void    case_down()
    {
        for(strtype &s : *this)
            s.case_down();
    }

    void    case_up()
    {
        for (strtype &s : *this)
            s.case_up();
    }

    void    trim()
    {
        for (strtype &s : *this)
            s.trim();
    }
    void    trim(char c)
    {
        for (strtype &s : *this)
            s.trim(c);
    }

    void    remove_slow(const sptr<TCHARACTER> &s)
    {
        aint i = find( s );
        if (i>=0) delete_slow(i);
    }
    void    remove_slow(aint idx)
    {
        __super::remove_slow(idx);
    }

    void    remove_fast(aint i)
    {
        __super::remove_fast(i);
    }

    bool remove_fast(const sptr<TCHARACTER> &s)
    {
        aint i = find( s );
        if (i>=0)
        {
            __super::remove_fast(i);
            return true;
        }
        return false;
    }

    template <aint N> void geta(strtype ar[N], aint i) const
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
        for (aint k=0;k<N;++k)
        {
            ar[k] = (*this)[i+k];
        }
    }

    // pool funtions

    const TCHARACTER *get_string_pointer(const sptr<TCHARACTER> &of) // dangerous! do not store pointer
    {
        aint index = find(of);
        if (index < 0)
        {
            add(of);
            return get(size() - 1).cstr();
        }
        return get(index).cstr();
    }
    template<typename CORE2> const TCHARACTER *get_string_pointer(const str_t<TCHARACTER, CORE2> &of) {return get_string_pointer(of.as_spart());}

    aint get_string_index(const sptr<TCHARACTER> &of)
    {
        aint index = find(of);
        if (index < 0)
        {
            add(of);
            return size() - 1;
        }
        return index;
    }

    template<typename CORE2> int get_string_index(const str_t<TCHARACTER, CORE2> &of) {return get_string_index(of.as_spart());}

    void kill_dups()
    {
        for(aint i = size() - 1; i > 0; --i)
            for(aint j = i - 1; j >= 0; --j)
                if (get(j).equals(get(i)))
                {
                    remove_fast(i);
                    break;
                }
    }

    void kill_dups_and_sort(bool ascending = true)
    {
        sort(ascending);

        for (aint i = size() - 2; i >= 0; --i)
            if (get(i).equals(get(i + 1)))
                remove_slow(i);
    }

    void kill_empty_fast()
    {
        aint i = 0;
        for(;i<size();)
        {
            if (get(i).is_empty())
            {
                remove_fast(i);
                continue;
            }
            ++i;
        }
    }

    void kill_empty_slow(aint startindex = 0)
    {
        aint i = startindex;
        for(;i<size();)
        {
            if (get(i).is_empty())
            {
                remove_slow(i);
                continue;
            }
            ++i;
        }
    }

    void clear()
    {
        __super::clear();
    }

    void clear(int index)
    {
        __super::get(index).clear();
    }

    void sort( bool ascending )
    {
        if (ascending)
        {
            __super::sort([](const strtype &u1, const strtype &u2)->bool
            {
                return 0 > strtype::compare(u1, u2);
            });
        } else
        {
            __super::sort([](const strtype &u1, const strtype &u2)->bool
            {
                return 0 < strtype::compare(u1, u2);
            });
        }
    }

	template <typename T> void sort(const T &sorter)
	{
		__super::sort(sorter);
	}

    signed char compare( const arrtype &othera ) const
    {
        aint cnt = tmin( size(), othera.size() );
        for (aint i=0;i<cnt;++i)
        {
            signed char c = strtype::compare( get(i), othera.get(i) );
            if (c != 0) return c;
        }
        aint c =( size() - othera.size() );
        if (c < 0) return -1;
        if (c > 0) return 1;
        return 0;
    }

    void merge_and_sort( const arrtype &othera )
    {
        aint cnt = othera.size();
        for (aint i = 0; i < cnt; ++i)
            add( othera.get(i) );

        sort(true);

        for (aint i=size()-2;i>=0;--i)
            if ( get(i).equals(get(i+1)) )
                remove_slow(i);
    }

	arrtype & operator = ( const arrtype &othera )
	{
        aint cc = tmin( size(), othera.size() );
        for (aint i=0;i<cc;++i)
            get(i).set( othera.get(i) );

        if ( cc == othera.size() )
        {
            truncate( cc );
        } else
        {

            aint cnt = othera.size();
            for (aint i = cc; i < cnt; ++i)
            {
                add( othera.get(i) );
            }
        }
        return *this;
	}

};

template <typename S> struct strmap_pair_s
{
    S k,v;
    typedef typename S::TCHAR TCHARACTER;
    int operator()( const sptr<TCHARACTER> &_k ) const
    {
        return S::compare( _k, k );
    }
    int operator()( const S &_k ) const
    {
        return S::compare(_k, k);
    }
    bool operator==( const strmap_pair_s&m ) const
    {
        return k.equals(m.k) && v.equals(m.v);
    }
    bool operator!=(const strmap_pair_s&m) const
    {
        return !k.equals(m.k) || !v.equals(m.v);
    }
};

template <typename S> class strmap_t : public array_inplace_t < strmap_pair_s< S >, 8 >
{
    typedef typename S::TCHAR TCHARACTER;
    typedef array_inplace_t < strmap_pair_s< S >, 8 > TSUPER;
public:
    strmap_t() {}
    explicit strmap_t( const sptr<TCHARACTER>&froms, TCHARACTER eq = '=' ) { parse( froms, eq ); }
    strmap_t( const strmap_t&m ):array_c(m.size())
    {
        *this = m;
    }
    strmap_t(strmap_t&&m)
    {
        *this = std::move( m );
    }
    strmap_t &operator=( const strmap_t&m )
    {
        aint msz = tmin( size(), m.size() );
        for(aint i=0;i<msz;++i)
            __super::get(i) = ((TSUPER &)m).get(i);
        if (m.size() > msz)
        {
            aint cnt = m.size();
            for (; msz < cnt; ++msz)
                add() = ((TSUPER &)m).get(msz);
        } else if (size() > msz)
            truncate(msz);
        return *this;
    }
    strmap_t &operator=(strmap_t&&m)
    {
        __super::operator=(std::move(m));
        return *this;
    }

    void parse( const sptr<TCHARACTER>&froms, TCHARACTER eq = '=' )
    {
        ts::token<TCHARACTER> lns(froms, '\n');
        for (; lns; ++lns)
        {
            auto s = lns->get_trimmed();
            aint eqi = s.find_pos(eq);
            if (eqi > 0)
                set( s.substr(0,eqi) ) = s.substr(eqi+1);
        }
    }
    bool unset(const sptr<TCHARACTER>&k)
    {
        int index;
        if (find_sorted(index, k))
        {
            remove_slow(index);
            return true;
        }
        return false;
    }
    S& set(const sptr<TCHARACTER>&k)
    {
        int index;
        if (find_sorted(index, k))
            return __super::get(index).v;

        auto &i = insert(index);
        i.k = k;
        return i.v;
    }
    S get( const sptr<TCHARACTER>&k, const S&def = S() )
    {
        if (const S *s = find(k))
            return *s;
        return def;
    }

    const S *find(const sptr<TCHARACTER>&k) const
    {
        int index;
        if (find_sorted(index, k))
            return &__super::get(index).v;
        return nullptr;
    }
    S *find(const sptr<TCHARACTER>&k)
    {
        int index;
        if (find_sorted(index, k))
            return &__super::get(index).v;
        return nullptr;
    }
    S to_str( TCHARACTER eq = '=' )
    {
        S s;
        for( const auto& i : *this )
            s.append(i.k).append_char(eq).append(i.v).append_char('\n');
        return s.trunc_length();
    }
    bool operator==( const strmap_t&m ) const
    {
        aint cnt = size();
        if (cnt != m.size()) return false;
        for(aint i=0;i<cnt;++i)
            if ( __super::get(i) != ((TSUPER &)m).get(i) ) return false;
        return true;
    }
    bool operator!=( const strmap_t&m ) const
    {
        return !(*this == m);
    }
    
};

typedef strmap_t<str_c> astrmap_c;
typedef strmap_t<wstr_c> wstrmap_c;


typedef strings_t<char> astrings_c;
typedef strings_t<wchar> wstrings_c;

} // namespace ts

