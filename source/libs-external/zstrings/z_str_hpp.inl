#pragma once

/*
    (C) 2010-2015 ROTKAERMOTA
*/

#include "z_str_funcs.h"

static_assert( sizeof(ZSTRINGS_ANSICHAR) == 1, "bad size of ZSTRINGS_ANSICHAR" );
static_assert( sizeof(ZSTRINGS_WIDECHAR) == 2, "bad size of ZSTRINGS_WIDECHAR" );

ZSTRINGS_SIGNED  text_utf8_to_ansi(ZSTRINGS_ANSICHAR *out, ZSTRINGS_SIGNED maxlen, const sptr<ZSTRINGS_ANSICHAR> &from);

#ifndef ZSTRINGS_ALLOCATOR
#pragma message ("malloc/free allocator used for strings")
class DEFAULT_STRALLOCATOR
{

public:
    void mf(void *ptr) { free( ptr ); }
    void *ma(ZSTRINGS_UNSIGNED sz) { return malloc(sz); }
    void *mra(void *ptr, ZSTRINGS_UNSIGNED sz) { return realloc( ptr, sz); }
};

#define ZSTRINGS_ALLOCATOR DEFAULT_STRALLOCATOR
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TCHARACTER> struct sptr;

template <typename TCHARACTER, class CORE > class str_t;
template<bool preparelen, class CORE1, class CORE2> void xset( str_t<ZSTRINGS_ANSICHAR,CORE1>&sto,  const str_t<ZSTRINGS_WIDECHAR,CORE2>&sfrom );
template<bool preparelen, class CORE1, class CORE2> void xset( str_t<ZSTRINGS_WIDECHAR,CORE1>&sto,  const str_t<ZSTRINGS_ANSICHAR,CORE2>&sfrom );
template<bool preparelen, typename TCHARACTER, class CORE1, class CORE2> void xset( str_t<TCHARACTER,CORE1>&sto,  const str_t<TCHARACTER,CORE2>&sfrom );

template<class CORE> void xappend(str_t<ZSTRINGS_ANSICHAR, CORE>&sto, const sptr<ZSTRINGS_ANSICHAR> &sfrom);
template<class CORE> void xappend(str_t<ZSTRINGS_WIDECHAR, CORE>&sto, const sptr<ZSTRINGS_WIDECHAR> &sfrom);
template<class CORE> void xappend( str_t<ZSTRINGS_ANSICHAR,CORE>&sto, const sptr<ZSTRINGS_WIDECHAR> &sfrom );
template<class CORE> void xappend( str_t<ZSTRINGS_WIDECHAR,CORE>&sto, const sptr<ZSTRINGS_ANSICHAR> &sfrom );

template<typename TCHARACTER> struct sptr
{
    const TCHARACTER *s;
    ZSTRINGS_SIGNED l;
    sptr() :s(ZSTRINGS_NULL), l(0) {}
    sptr(const TCHARACTER *s) :s(s), l(CHARz_len(s)) {}
    sptr(const TCHARACTER *s, ZSTRINGS_SIGNED l) :s(s), l(l) {}
    template<typename CORE> sptr(const str_t<TCHARACTER, CORE> &s) { *this = s.as_sptr(); }
    explicit operator bool() const {return l>0;}
    bool operator==(const sptr<TCHARACTER> &ss) const
    {
        if (ss.l != l) return false;
        return blk_cmp(s, ss.s, l * sizeof(TCHARACTER));
    }
    sptr skip(ZSTRINGS_SIGNED chars) const { ZSTRINGS_ASSERT(chars <= l); return sptr(s+chars,l-chars);}
    sptr trim(ZSTRINGS_SIGNED chars) const { ZSTRINGS_ASSERT(chars <= l); return sptr(s,l-chars);}
    sptr part(ZSTRINGS_SIGNED chars) const { ZSTRINGS_ASSERT(chars <= l); return sptr(s,chars);}
    void operator++()
    {
        ZSTRINGS_ASSERT(l > 0);
        ++s;
        --l;
    }

};

typedef sptr<ZSTRINGS_ANSICHAR> asptr;
typedef sptr<ZSTRINGS_WIDECHAR> wsptr;

namespace zstrings_internal
{
template<typename TCHARACTER> struct mod_nothing
{
    ZSTRINGS_SIGNED operator()(TCHARACTER * /*news*/, ZSTRINGS_SIGNED newl, TCHARACTER * /*olds*/, ZSTRINGS_SIGNED /*oldl*/) const
    {
        return newl;
    }
};
template<typename TCHARACTER> struct mod_preserve
{
    ZSTRINGS_SIGNED real_len;
    mod_preserve( ZSTRINGS_SIGNED real_len ):real_len(real_len) {}
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        if (news != olds)
        {
            if (newl <= oldl) // decrease len - copy content
                blk_UNIT_copy_fwd<TCHARACTER>(news, olds, newl);
            else
                blk_UNIT_copy_fwd<TCHARACTER>(news, olds, oldl);
        }
        return real_len;
    }
};

template<typename TCHARACTER> struct mod_expand_escape_codes
{
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_SIGNED len = 0;
        for (ZSTRINGS_SIGNED i = 0; i < oldl; ++i)
        {
            TCHARACTER    chr = *(olds + i);
            if (chr == '\\')
            {
                ZSTRINGS_SIGNED next = i + 1;
                if (next >= oldl) break;
                chr = *(olds + next);
                if (chr == 't')
                {
                    chr = '\t';
                }
                else if (chr == 'n')
                {
                    chr = '\n';
                }
                else if (chr == 'r')
                {
                    chr = '\r';
                }
                else if (chr == '\\')
                {
                    chr = '\\';
                }
                ++i;
            }
            *(news + len++) = chr;
        }
        return len;
    }
};
template<typename TCHARACTER> struct mod_copy
{
    const TCHARACTER *s;
    ZSTRINGS_SIGNED l;
    mod_copy(const TCHARACTER *s, ZSTRINGS_SIGNED l) :s(s), l(l) {}
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *, ZSTRINGS_SIGNED) const
    {
        ZSTRINGS_ASSERT(newl == l);
        blk_UNIT_copy_fwd<TCHARACTER>(news, s, l);
        return newl;
    }
};
template<typename TCHARACTER> struct mod_copychar
{
    TCHARACTER c;
    mod_copychar(TCHARACTER c) :c(c) {}
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *, ZSTRINGS_SIGNED) const
    {
        ZSTRINGS_ASSERT(newl == 1);
        news[0] = c;
        return newl;
    }
};

template<typename TCHARACTER> struct mod_cut
{
    ZSTRINGS_SIGNED idx, len;
    mod_cut(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED len) :idx(idx), len(len) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl - len));
        if (news != olds) blk_UNIT_copy_fwd<TCHARACTER>(news, olds, idx);
        blk_UNIT_copy_fwd<TCHARACTER>(news + idx, olds + idx + len, oldl - idx - len);
        return newl;
    }
};

template<typename TCHARACTER> struct mod_fill
{
    ZSTRINGS_SIGNED idx, sz;
    TCHARACTER c;
    mod_fill(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED sz, TCHARACTER c) :idx(idx), sz(sz), c(c) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        if (news != olds)
        {
            blk_UNIT_copy_fwd<TCHARACTER>(news, olds, idx);
            blk_UNIT_copy_fwd<TCHARACTER>(news+idx+sz, olds+idx+sz, oldl-idx-sz);
        }
        for(ZSTRINGS_SIGNED i=idx;i<(idx+sz);++i)
            news[i] = c;
        return newl;
    }
};

template<typename TCHARACTER> struct mod_replace
{
    ZSTRINGS_SIGNED idx, sz, l;
    const TCHARACTER *s;
    mod_replace(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED sz, const TCHARACTER *s, ZSTRINGS_SIGNED l) :idx(idx), sz(sz), s(s), l(l) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl + l - sz));

        if (news != olds) blk_UNIT_copy_fwd<TCHARACTER>(news, olds, idx);
        if (news != olds || sz >= l)
            blk_UNIT_copy_fwd<TCHARACTER>(news + idx + l, olds + idx + sz, oldl - idx - sz);
        else
            blk_UNIT_copy_back<TCHARACTER>(news + idx + l, olds + idx + sz, oldl - idx - sz);
        blk_UNIT_copy_fwd<TCHARACTER>(news + idx, s, l);
        return newl;
    }
};

template<typename TCHARACTER> struct mod_crop
{
    ZSTRINGS_SIGNED idx0, idx1;
    mod_crop(ZSTRINGS_SIGNED idx0, ZSTRINGS_SIGNED idx1) :idx0(idx0), idx1(idx1) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED /*oldl*/) const
    {
        ZSTRINGS_ASSERT(newl == (idx1 - idx0));
        if (news != olds || idx0 > 0) blk_UNIT_copy_fwd<TCHARACTER>(news, olds + idx0, idx1 - idx0);
        return newl;
    }
};

template<typename TCHARACTER> struct mod_format_left
{
    ZSTRINGS_SIGNED need_add, need_del;
    TCHARACTER c;
    mod_format_left(ZSTRINGS_SIGNED need_add, ZSTRINGS_SIGNED need_del, TCHARACTER c) :need_add(need_add), need_del(need_del), c(c) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl + need_add - need_del));
        ZSTRINGS_SIGNED _need_add(need_add);

        if (news != olds)
        {
            while (_need_add-- > 0) *news++ = c;
            blk_UNIT_copy_fwd<TCHARACTER>(news, olds + need_del, oldl - need_del);
            return;
        }

        if (need_del)
        {
            blk_UNIT_copy_fwd<TCHARACTER>(news, olds + need_del, oldl - need_del);
            return;
        }

        blk_UNIT_copy_back<TCHARACTER>(news + need_add, olds, oldl);
        while (_need_add-- > 0) *news++ = c;
        return newl;
    }
};

template<typename TCHARACTER> struct mod_format_right
{
    ZSTRINGS_SIGNED need_add, need_del;
    TCHARACTER c;
    mod_format_right(ZSTRINGS_SIGNED need_add, ZSTRINGS_SIGNED need_del, TCHARACTER c) :need_add(need_add), need_del(need_del), c(c) {}

    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl + need_add - need_del));

        if (news != olds) blk_UNIT_copy_fwd<TCHARACTER>(news, olds, oldl - need_del);

        if (need_add)
        {
            ZSTRINGS_SIGNED _need_add(need_add);
            news += oldl;
            while (_need_add-- > 0) *news++ = c;
        }
        return newl;
    }
};

template<typename TCHARACTER> struct mod_insert
{
    const TCHARACTER *s;
    ZSTRINGS_SIGNED idx, l;
    mod_insert(ZSTRINGS_SIGNED idx, const TCHARACTER *s, ZSTRINGS_SIGNED l) :idx(idx), s(s), l(l) {}
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl + l));
        if (news != olds && olds) blk_UNIT_copy_fwd<TCHARACTER>(news, olds, idx);
        if (news == olds) blk_UNIT_copy_back<TCHARACTER>(news+idx+l,olds+idx,(oldl-idx));
        blk_UNIT_copy_fwd<TCHARACTER>(news + idx, s, l);
        if (news != olds && olds) blk_UNIT_copy_fwd<TCHARACTER>(news+idx+l, olds+idx, oldl-idx);
        return newl;
    }
};

template<typename TCHARACTER> struct mod_insert_char
{
    ZSTRINGS_SIGNED idx;
    TCHARACTER c;
    mod_insert_char(ZSTRINGS_SIGNED idx, TCHARACTER c) :idx(idx), c(c) {}
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == (oldl + 1));
        if (news != olds && olds) blk_UNIT_copy_fwd<TCHARACTER>(news, olds, idx);
        if (news == olds) blk_UNIT_copy_back<TCHARACTER>(news+idx+1,olds+idx,(oldl-idx));
        news[idx] = c;
        if (news != olds && olds) blk_UNIT_copy_fwd<TCHARACTER>(news+idx+1, olds+idx, oldl-idx);
        return newl;
    }
};

template<typename TCHARACTER> struct mod_reverse
{
    ZSTRINGS_SIGNED operator()(TCHARACTER *news, ZSTRINGS_SIGNED newl, TCHARACTER *olds, ZSTRINGS_SIGNED oldl) const
    {
        ZSTRINGS_ASSERT(newl == oldl);

        if (news != olds)
        {
            for (ZSTRINGS_SIGNED i = 0; i < newl; ++i)
            {
                news[i] = olds[newl - i - 1];
            }
        }
        else
        {
            for (ZSTRINGS_SIGNED i = newl / 2 - 1; i >= 0; --i)
            {
                SWAP(news[i], news[newl - i - 1]);
            }
        }
        return newl;
    }
};

}

/////////////////////////////////////////////////////////////////////////////////////////////////
// copy-on-demand string core

template <typename TCHARACTER, typename ALLOCATOR = ZSTRINGS_ALLOCATOR> class str_core_copy_on_demand_c : public ALLOCATOR
{
	struct str_core_s
	{
		void    ref_inc() {m_ref++;};
		void    ref_dec(ALLOCATOR *a)
		{
			ZSTRINGS_SIGNED nrf = m_ref-1;
			if (nrf==0)
			{
				a->mf( this );
			} else
				m_ref = nrf;
		}
		void        ref_dec_only()
		{
			m_ref--;
		};
		bool  is_multi_ref() const {return (m_ref>1);}

		TCHARACTER *str() {return (TCHARACTER *)(this + 1);}
		const TCHARACTER *str() const {return (TCHARACTER *)(this + 1);}

		static str_core_s  *build(ALLOCATOR *a, ZSTRINGS_SIGNED size)
		{
			ZSTRINGS_SIGNED a_size = zstrings_internal::_calc_alloc_size<TCHARACTER>( size + 1 );
			ZSTRINGS_SIGNED mem_size = a_size*sizeof(TCHARACTER);

			str_core_s *core = (str_core_s *)a->ma( sizeof(str_core_s) + mem_size );

			core->m_n_size=size;
			core->m_a_size=a_size;
			core->m_ref=1;

			core->str()[size] = 0;
			return core;
		};
		str_core_s *  newsize(ALLOCATOR *a, ZSTRINGS_SIGNED size)
		{
			if (size>=m_a_size)
			{
				ZSTRINGS_SIGNED a_size = zstrings_internal::_calc_alloc_size<TCHARACTER>( size+ 1 );
				ZSTRINGS_SIGNED mem_size = a_size*sizeof(TCHARACTER);
				str_core_s *nd = (str_core_s *)a->mra(this,mem_size + sizeof(str_core_s));
				nd->m_a_size = a_size;
				return nd;

			} else
			{
				return this;
			}
		}


		ZSTRINGS_SIGNED     m_n_size;
		ZSTRINGS_SIGNED     m_a_size;
	private:
		ZSTRINGS_SIGNED     m_ref;

	};

	str_core_s *core;

public:
    static const bool cstr_allow = true;
    static const bool readonly = false;


	str_core_copy_on_demand_c():core(ZSTRINGS_NULL) {}
	str_core_copy_on_demand_c(ZSTRINGS_SIGNED size) { core = str_core_s::build(this, size); }
	str_core_copy_on_demand_c(const str_core_copy_on_demand_c &ocore):core( ocore.core ) { if (core) core->ref_inc(); }
    str_core_copy_on_demand_c(const sptr<TCHARACTER> &sp) { core = str_core_s::build(this, sp.l); blk_UNIT_copy_fwd<TCHARACTER>((*this)(),sp.s,sp.l);}

    ~str_core_copy_on_demand_c()
    {
        if (core) core->ref_dec( this );
    }

    void operator=(const sptr<TCHARACTER> &sp)
    {
        if (sp.l == 0)
		{
			clear();
            return;
		}

		change(sp.l, zstrings_internal::mod_copy<TCHARACTER>(sp.s,sp.l));
    }

    template<bool> void set(const str_core_copy_on_demand_c &ocore);
    template<> void set<false>(const str_core_copy_on_demand_c &ocore)
    {
        ZSTRINGS_SIGNED l = ocore.len();
        if (l == 0)
        {
            if (core) 
            {
                core->ref_dec(this);
                core = ZSTRINGS_NULL;
            }
            return;
        }
        if (!core)
            core = str_core_s::build(this, l);
        else
        {
            core = core->newsize(this, l);
            core->m_n_size = l;
            core->str()[l] = 0;
        }
        blk_UNIT_copy_fwd<TCHARACTER>(core->str(), ocore(), l);
    }
    template<> void set<true>(const str_core_copy_on_demand_c &ocore)
    {
        if (core == ocore.core) return;
        if (core) core->ref_dec(this);
        core = ocore.core;
        if (core) core->ref_inc();
    }

	void operator=(const str_core_copy_on_demand_c &ocore)
	{
        set< zstrings_internal::is_struct_empty<ALLOCATOR>::value >( ocore );
	}
	bool operator==(const str_core_copy_on_demand_c &ocore) const
	{
		return core == ocore.core;
	}

	TCHARACTER * operator()() {return core ? core->str() : ZSTRINGS_NULL;}
	const TCHARACTER * operator()() const {return core ? core->str() : ZSTRINGS_NULL;}
	ZSTRINGS_SIGNED len() const {return core?core->m_n_size:0;}
	ZSTRINGS_SIGNED cap() const {return core?core->m_a_size:0;}

	void clear() // set zero len
	{
		if (core == ZSTRINGS_NULL) return;
		if (core->is_multi_ref())
		{
			core->ref_dec_only();
			core = ZSTRINGS_NULL;
			return;
		}

		core->m_n_size = 0;
		core->str()[0] = 0;
	}

	template<typename MODER> void change( ZSTRINGS_SIGNED newlen, const MODER &moder )
	{
		if (core == ZSTRINGS_NULL)
		{
			core = str_core_s::build(this, newlen);
			ZSTRINGS_SIGNED fixlen = moder(core->str(), newlen, ZSTRINGS_NULL, 0);
			if (fixlen < newlen)
			{
				core->m_n_size = fixlen;
				core->str()[fixlen] = 0;
			}
			return;
		}
		if (core->is_multi_ref())
		{
			str_core_s *oldcore = core;
			core = str_core_s::build(this, newlen);
			ZSTRINGS_SIGNED fixlen = moder(core->str(), newlen, oldcore->str(), oldcore->m_n_size);
			oldcore->ref_dec_only();
			if (fixlen < newlen)
			{
				core->m_n_size = fixlen;
				core->str()[fixlen] = 0;
			}
			return;

		}
		core = core->newsize( this, newlen );
		ZSTRINGS_SIGNED fixlen = moder( core->str(), newlen, core->str(), core->m_n_size );
		core->m_n_size = fixlen;
		core->str()[fixlen] = 0;
	}

};

// static string core

template <typename TCHARACTER, ZSTRINGS_SIGNED _limit> class str_core_static_c
{
	#pragma warning(push)
	#pragma warning(disable:4308) // warning C4308: negative integral constant converted to unsigned type
	static const ZSTRINGS_SIGNED _cap = (_limit >= 0) ? _limit : ((-_limit-sizeof(ZSTRINGS_SIGNED))/sizeof(TCHARACTER));
	#pragma warning(pop)
	TCHARACTER _str[_cap];
	ZSTRINGS_SIGNED _len;

	static ZSTRINGS_SIGNED goodlen( ZSTRINGS_SIGNED l ) { ZSTRINGS_ASSERT(l < _cap); return l < _cap ? l : (_cap-1); }

public:
    static const bool cstr_allow = true;
    static const bool readonly = false;


    str_core_static_c():_len(0) { _str[0] = 0; }
	str_core_static_c(ZSTRINGS_SIGNED size):_len( goodlen(size) ) { _str[_len] = 0; }
	str_core_static_c(const str_core_static_c &ocore):_len(ocore._len) { blk_UNIT_copy_fwd<TCHARACTER>(_str, ocore._str, ocore._len + 1); }
    str_core_static_c(const sptr<TCHARACTER> &sp):_len( goodlen(sp.l) ) { blk_UNIT_copy_fwd<TCHARACTER>(_str, sp.s, _len); _str[_len] = 0; }

    void operator=(const sptr<TCHARACTER> &sp)
    {
		_len = goodlen(sp.l);
		blk_UNIT_copy_fwd<TCHARACTER>(_str, sp.s, _len);
        _str[_len] = 0;
    }
	void operator=(const str_core_static_c &ocore)
	{
		_len = ocore._len;
		blk_UNIT_copy_fwd<TCHARACTER>(_str, ocore._str, ocore._len + 1);
	}
	bool operator==(const str_core_static_c &ocore) const
	{
		return false; // static cores never equal each other
	}

	const TCHARACTER * operator()() const {return _str;}
	TCHARACTER * operator()() {return _str;}
	ZSTRINGS_SIGNED len() const {return _len;}
	ZSTRINGS_SIGNED cap() const {return _cap;}

	void clear() // set zero len
	{
		_len = 0;
		_str[0] = 0;
	}

	template<typename MODER> void change( ZSTRINGS_SIGNED newlen, const MODER &moder )
	{
		newlen = goodlen(newlen);
		ZSTRINGS_SIGNED fixlen = moder( _str, newlen, _str, _len );
		_len = fixlen;
		_str[fixlen] = 0;
	}

};

template <typename TCHARACTER> class str_core_part_c
{
    sptr<TCHARACTER> buf;

public:
    static const bool cstr_allow = false;
    static const bool readonly = true;

    str_core_part_c() {}
    str_core_part_c(ZSTRINGS_SIGNED size) { ZSTRINGS_ASSERT(false, "str_core_part_c cannot be created"); }
    str_core_part_c(const str_core_part_c &ocore) { buf = ocore.buf; }
    str_core_part_c(const sptr<TCHARACTER> &sp):buf( sp ) {}

    void operator=(const sptr<TCHARACTER> &sp)
    {
        buf = sp;
    }
    void operator=(const str_core_part_c &ocore)
    {
        buf = ocore.buf;
    }
    bool operator==(const str_core_part_c &ocore) const
    {
        return buf.s == ocore.buf.s && buf.l == ocore.buf.l;
    }

    const TCHARACTER * operator()() const { return buf.s; }
    TCHARACTER * operator()() { ZSTRINGS_ASSERT(false, "str_core_part_c is read only"); return ZSTRINGS_NULL; } //-V659
    ZSTRINGS_SIGNED len() const { return buf.l; }
    ZSTRINGS_SIGNED cap() const { return 0; }

    void clear() // set zero len
    {
        ZSTRINGS_ASSERT(false, "str_core_part_c is read only");
    }

    template<typename MODER> void change(ZSTRINGS_SIGNED newlen, const MODER &moder)
    {
        ZSTRINGS_ASSERT(false, "str_core_part_c is read only");
    }

};


/////////////////////////////////////////////////////////////////////////////////////////////////
#pragma warning ( push )
#pragma warning (disable: 4127)
template <typename TCHARACTER, class CORE = str_core_copy_on_demand_c<TCHARACTER> > class str_t
{
	CORE core;

    typedef str_t<TCHARACTER, str_core_part_c<TCHARACTER> > strpart;

    const TCHARACTER *_cstr() const
    {
        static const ZSTRINGS_WIDECHAR dummy = 0;
        const TCHARACTER * r = core();
        if (r == ZSTRINGS_NULL) return (TCHARACTER *)&dummy;
        return r;
    };

public:

    typedef TCHARACTER char_t;
    typedef TCHARACTER TCHAR;
    typedef CORE TCORE;


    str_t() {}
    explicit str_t(ZSTRINGS_SIGNED size, bool set0len):core( size ) { if (set0len) set_length(0); }

    explicit str_t(TCHARACTER c) :core(1)
    {
        core()[0] = c;
    };

    str_t(const TCHARACTER * const s):core( CHARz_len<TCHARACTER>(s) )
    {
		blk_UNIT_copy_fwd<TCHARACTER>(core(),s,core.len());
    };

    str_t(const sptr<TCHARACTER> &s1, const sptr<TCHARACTER> &s2) :core(s1.l + s2.l)
    {
        blk_UNIT_copy_fwd<TCHARACTER>(core(), s1.s, s1.l);
        blk_UNIT_copy_fwd<TCHARACTER>(core() + s1.l, s2.s, s2.l);
    };

    str_t(const TCHARACTER * const s, const ZSTRINGS_SIGNED l):core( l )
    {
        blk_UNIT_copy_fwd<TCHARACTER>(core(),s,l);
    };

    str_t(const sptr<TCHARACTER> &s) :core(s)
    {
    };

    str_t(const str_t &s):core(s.core) {}

    /*
        convert on create is evil: it can lead to invisible conversion
	    template<typename TCHARACTER2, class CORE2> str_t(const str_t<TCHARACTER2, CORE2> &s):core( s.get_length() ) { xset<false>( *this, s ); }
    */

    template<class CORE2> str_t(const str_t<TCHARACTER, CORE2> &s):core( s.get_length() ) { xset<false>( *this, s ); }

    ~str_t() {}

    template<class CORE2> bool begins_ignore_case(str_t<TCHARACTER, CORE2> & s) const
    {
        return begins_ignore_case(s.as_sptr());
    }

    bool begins_ignore_case(const TCHARACTER *s) const
    {
        return begins_ignore_case(sptr<TCHARACTER>(s));
    }
    bool begins_ignore_case(const sptr<TCHARACTER> &s) const
    {
        if (core.len() == 0) return s.l == 0;
        return s.l <= get_length() && CHARz_equal_ignore_case(s.s, core(), s.l);
    }

    template<class CORE2> bool begins(str_t<TCHARACTER, CORE2> & s) const
    {
        if (core.len() == 0) return s.is_empty();
        sptr<TCHARACTER> sp = s.as_sptr();
        return blk_cmp(core(), sp.s, sp.l * sizeof(TCHARACTER));
    }

    bool begins(const sptr<TCHARACTER> &s) const
    {
        if (core.len() == 0) return s.l == 0;
        return blk_cmp(core(), s.s, s.l * sizeof(TCHARACTER));
    }

    bool begins(const TCHARACTER *s) const
    {
        if (core.len() == 0) return s[0] == 0;
        return blk_cmp(core(),s,CHARz_len(s) * sizeof(TCHARACTER));
    }

    template<class CORE2> bool ends_ignore_case(str_t<TCHARACTER, CORE2> & s) const { return ends_ignore_case(s.as_sptr()); }
    bool ends_ignore_case(const TCHARACTER *s) const { return ends_ignore_case(sptr<TCHARACTER>(s));}
    bool ends_ignore_case(const sptr<TCHARACTER> &s) const
    {
        if (core.len() == 0) return s.l == 0;
        return s.l <= get_length() && CHARz_equal_ignore_case(s.s, core() + get_length() - s.l, s.l);
    }

    bool ends(const TCHARACTER *s) const
    {
        if (core.len() == 0) return s[0] == 0;
        ZSTRINGS_SIGNED s_len = CHARz_len(s);

        return (s_len > core.len())
            ? false
            : blk_cmp(core() + core.len() - s_len, s, s_len * sizeof(TCHARACTER));
    }

    bool ends(const sptr<TCHARACTER> &s) const
    {
        if (core.len() == 0) return s.l == 0;

        return (s.l > core.len())
            ? false
            : blk_cmp(core() + core.len() - s.l, s.s, s.l * sizeof(TCHARACTER));
    }

    template<class CORE2> bool ends(str_t<TCHARACTER, CORE2> & s) const { return ends( s.as_sptr() );}

    bool beginof(const TCHARACTER *s) const 
    {
        if (core.len() == 0) return s[0] == 0;
        return blk_cmp(core(),s, get_length() * sizeof(TCHARACTER));
    }

    bool contain_chars(const sptr<TCHARACTER> &s) const
    {
        for(TCHARACTER c : *this)
            if (CHARz_findn(s.s,c,s.l) < 0)
                return false;
        return true;
    }

    template<class CORE2> bool contain_token( str_t<TCHARACTER, CORE2> & s, TCHARACTER td )
    {
        ZSTRINGS_SIGNED seek = 0;
        for (;;)
        {
            ZSTRINGS_SIGNED newseek = find_pos(seek,s);
            if ( newseek < 0 ) return false;
            if ( (newseek != 0 && get_char(newseek - 1) != td) ||
                (newseek != ZSTRINGS_SIGNED(get_length() - s.get_length()) && get_char( newseek + s.get_length() ) != td )
                )
            {
                seek = newseek + s.get_length();
                continue;
            }

            return true;
        }
    }


    str_t<TCHARACTER, CORE> & case_down(ZSTRINGS_SIGNED offset = 0)
    {
        if (core.len() == 0) return *this;
		core.change( core.len(), zstrings_internal::mod_preserve<TCHARACTER>( core.len() ) );
        ZSTRINGS_SYSCALL(text_lowercase)(str() + offset, get_length() - offset);
        return *this;
    }

    str_t<TCHARACTER, CORE> & case_up(ZSTRINGS_SIGNED offset = 0)
    {
        if (core.len() == 0) return *this;
        core.change( core.len(), zstrings_internal::mod_preserve<TCHARACTER>( core.len() ) );

        ZSTRINGS_SYSCALL(text_uppercase)(str() + offset, get_length() - offset);
        return *this;
    }

    str_t<TCHARACTER, CORE> & reverse()
    {
        ZSTRINGS_SIGNED cnt = get_length();
        if (cnt <= 1) return *this;
		core.change(cnt, mod_reverse());
        return *this;
    }

    str_t<TCHARACTER, CORE> extract_tail( TCHARACTER separator, bool cut_original = true )
    {
        str_t<TCHARACTER, CORE> str;
        ZSTRINGS_SIGNED i = find_pos(separator);
        if (i >= 0)
        {
            str.set( core() + i + 1, get_length() - i - 1 );
            if (cut_original) set_length(i);
        }
        return str;
    }

    str_t<TCHARACTER, CORE> extract_numbers() const
    {
        str_t<TCHARACTER, CORE> str;
        if (core.len() == 0) return str;
        for (ZSTRINGS_UNSIGNED i = 0, cnt = get_length(); i < cnt; ++i)
        {
            if (CHAR_is_digit( get_char(i) )) str.append_char(get_char(i));
        }
        return str;
    }

    str_t<TCHARACTER, CORE> extract_non_numbers() const
    {
        str_t<TCHARACTER, CORE> str;
        if (core.len() == 0) return str;
        for (ZSTRINGS_UNSIGNED i = 0, cnt = get_length(); i < cnt; ++i)
        {
            if (!CHAR_is_digit( get_char(i) )) str.append_char(get_char(i));
        }
        return str;
    }

    
    template<typename CORE2> void extract_non_chars( str_t<TCHARACTER, CORE2> &str, const wsptr& nonchars ) const
    {
        static_assert( !CORE2::readonly, "readonly str" );
        str.clear();
        if (core.len() == 0) return;
        for (ZSTRINGS_UNSIGNED i = 0, cnt = get_length(); i < cnt; ++i)
            if (CHARz_findn(nonchars.s, get_char(i), nonchars.l) < 0)
                str.append_char(get_char(i));
    }

    str_t extract(ZSTRINGS_SIGNED idx, const sptr<TCHARACTER> &s1, const sptr<TCHARACTER> &s2) const
    {
        if (core.len() == 0) return str_t<TCHARACTER, CORE>();
        ZSTRINGS_SIGNED idx1 = find_pos(idx, s1);
        if (idx1 < 0) return str_t<TCHARACTER, CORE>();
        ZSTRINGS_SIGNED idx2 = find_pos(idx1+1, s2);
        if (idx2 < 0) return str_t<TCHARACTER, CORE>();
        return str_t(core() + idx1 + s1.l, idx2 - idx1 - s1.l);
    }

    bool find_inds(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED &idx1, ZSTRINGS_SIGNED &idx2, const TCHARACTER *s1, const TCHARACTER *s2) const
    {
        if (core.len() == 0) return false;
        idx1 = find_pos(idx, s1);
        if (idx1 < 0) return false;
        idx2 = find_pos(idx1+1, s2);
        if (idx2 < 0) return false;
        return true;
    }
    bool find_inds(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED &idx1, ZSTRINGS_SIGNED &idx2, TCHARACTER c1, TCHARACTER c2) const
    {
        if (core.len() == 0) return false;
        idx1 = find_pos(idx, c1);
        if (idx1 < 0) return false;
        idx2 = find_pos(idx1+1, c2);
        if (idx2 < 0) return false;
        return true;
    }

#ifdef ZSTRINGS_VEC3
	template<typename VECCTYPE, ZSTRINGS_SIGNED VECLEN> ZSTRINGS_VEC3(VECCTYPE) as_vec() const
	{
		ZSTRINGS_VEC3(VECCTYPE) r(0);
		ZSTRINGS_SIGNED index = 0;
		for (token t(*this, TCHARACTER(',')); t && index < VECLEN; ++t, ++index)
			r[index] = t->as_num<VECCTYPE>();
		return r;
	}
	ZSTRINGS_VEC3(float) as_fvec3() const {return as_vec<float,3>(); }
	ZSTRINGS_VEC3(int) as_ivec3() const { return as_vec<int, 3>(); }
#endif

    template <ZSTRINGS_UNSIGNED N> void hex2buf( ZSTRINGS_BYTE *out, ZSTRINGS_SIGNED index = 0 ) const
    {
        ZSTRINGS_SIGNED n = (get_length() - index) / 2; 
        if (n > N) n = N;
        for (ZSTRINGS_SIGNED i = 0; i < n; ++i)
            out[i] = as_byte_hex(index + i * 2);
        for (ZSTRINGS_SIGNED i = N; i < n; ++i)
            out[i] = 0;
    }

    ZSTRINGS_BYTE as_byte_hex( ZSTRINGS_SIGNED char_index ) const
    {
        ZSTRINGS_SIGNED v = (wchar_as_hex( get_char(char_index) ) << 4) | wchar_as_hex( get_char(char_index + 1) );
        return (ZSTRINGS_BYTE)v;
    }

	template<typename NUMTYPE> NUMTYPE as_num(NUMTYPE def = 0, ZSTRINGS_SIGNED skip_chars = 0) const
	{
		if (core.len() == 0) return def;
		NUMTYPE r = 0;
		if ( !CHARz_to_int<TCHARACTER, NUMTYPE>(r, core() + skip_chars, core.len() - skip_chars) )
		{
			ZSTRINGS_NUMCONVERSION_ERROR(def);
			return def;
		}
		return r;
	}
	template<> float as_num<float>(float def, ZSTRINGS_SIGNED skip_chars) const { return as_float(def,skip_chars); }
	template<> double as_num<double>(double def, ZSTRINGS_SIGNED skip_chars) const { return as_double(def, skip_chars); }

    int as_int(int def = 0, ZSTRINGS_SIGNED skip_chars = 0) const { return as_num<int>(def, skip_chars); }
	unsigned int as_uint(unsigned int def = 0, ZSTRINGS_SIGNED skip_chars = 0) const { return as_num<unsigned int>(def, skip_chars); }


    template<typename NUMTYPE> NUMTYPE as_num_part(NUMTYPE def = 0, ZSTRINGS_SIGNED skip_chars = 0) const
    {
        if (core.len() == 0) return def;
        return CHARz_to_int_10<TCHARACTER, NUMTYPE>( core() + skip_chars, def, core.len() - skip_chars );
    }

    bool is_float() const
    {
        if ( get_char(0) != '-' && get_char(0) != '.' && !CHAR_is_digit( get_char(0) ) ) return false;
        bool dot_yes = false;
        ZSTRINGS_SIGNED cnt = get_length();
        for (ZSTRINGS_SIGNED i=0;i<cnt;++i)
        {
            if ( get_char(i) == '.' )
            {
                if (dot_yes) return false;
                dot_yes = true;
                continue;
            }
            if ( get_char(i) == '-' )
            {
                if (i != 0) return false;
                continue;
            }
            if (!CHAR_is_digit(get_char(i))) return false;
        }

        return true;
    }

    float as_float() const
    {
        if (core.len() == 0) return 0.0f;
        double out = 0;
        if ( !CHARz_to_double(out, core(), get_length()) )
        {
			ZSTRINGS_NUMCONVERSION_ERROR(0);
        }
        return (float)out;
    }

    float as_float(float def, ZSTRINGS_SIGNED skip_chars = 0) const
    {
        if (core.len() == 0) return def;
        double out = 0;
		if (!CHARz_to_double(out, core() + skip_chars, get_length() - skip_chars))
        {
            return def;
        }
        return (float)out;
    }

    double as_double() const
    {
		if (core.len() == 0) return 0.0;
		double out = 0;
		if ( !CHARz_to_double(out, core(), get_length()) )
		{
			ZSTRINGS_NUMCONVERSION_ERROR(0);
		}
		return out;
    }
	double as_double(double def, ZSTRINGS_SIGNED skip_chars = 0) const
	{
		if (core.len() == 0) return 0.0;
		double out = 0;
		if (!CHARz_to_double(out, core() + skip_chars, get_length() - skip_chars))
		{
			return def;
		}
		return out;
	}

    sptr<TCHARACTER> as_sptr() const
    {
        return sptr<TCHARACTER>( _cstr(), get_length() );
    }
    sptr<TCHARACTER> as_sptr(ZSTRINGS_SIGNED othlen) const
    {
        ZSTRINGS_ASSERT(othlen <= get_length());
        return sptr<TCHARACTER>(_cstr(), othlen);
    }

    TCHARACTER *str() const
    {
        ZSTRINGS_ASSERT( CORE::cstr_allow, "pointer to string not allowed" );
        return const_cast<TCHARACTER *>( core() );
    };
	const TCHARACTER *cstr() const
    {
        ZSTRINGS_ASSERT( CORE::cstr_allow, "const pointer to string not allowed" );
        return _cstr();
    };
    operator const TCHARACTER *() const { return cstr();};

    bool operator < (const sptr<TCHARACTER> &s) const
    {
        return compare( as_sptr(), s ) < 0;
    }
    bool operator > (const sptr<TCHARACTER> &s) const
    {
        return compare( as_sptr(), s ) > 0;
    }

    bool operator <= (const sptr<TCHARACTER> &s) const
    {
        return compare( as_sptr(), s ) <= 0;
    }
    bool operator >= (const sptr<TCHARACTER> &s) const
    {
        return compare( as_sptr(), s ) >= 0;
    }


    str_t & clear() // set zero len
    {
		core.clear();
        return *this;
    }

    bool  is_empty() const { return (get_length() == 0); }
    ZSTRINGS_SIGNED get_length() const {return core.len();};
    ZSTRINGS_SIGNED get_capacity() const {return core.cap();};
    void require_capacity(ZSTRINGS_SIGNED capacity)
    {
        if (core.cap() < capacity)
        {
            core.change(capacity, zstrings_internal::mod_preserve<TCHARACTER>(get_length()));
        }
    }

    str_t &  trunc_length(const ZSTRINGS_SIGNED chars = 1)
    {
        return set_length( get_length() - zstrings_internal::zmin(chars, get_length()) );
    }
    str_t &  trunc_char(const TCHARACTER c)
    {
        if (get_length() > 0 && get_last_char() == c)
            trunc_length();
        return *this;
    }

    str_t &  set_length(ZSTRINGS_SIGNED len, bool preserve_content = true) // undefined content for increasing buffer
    {
		if (preserve_content)
			core.change(len, zstrings_internal::mod_preserve<TCHARACTER>(len));
		else
			core.change(len, zstrings_internal::mod_nothing<TCHARACTER>());
        return *this;
    }

    str_t &  set_length() // calc len
    {
        ZSTRINGS_SIGNED newlen = CHARz_len(core());
        if (ZSTRINGS_ASSERT(newlen <= get_length()))
            set_length( newlen );
        return *this;
    }

    str_t &  append_unicode_as_utf8(long n) // encode utf8 form int32 code
    {
        static_assert( sizeof(TCHARACTER) == 1, "wide is bad" );

        if (n <= 0x7f)
            return append_char(n & 0x7f);
        if (n <= 0x7FF)
            return append_char(0xc0 | ((n >> 6) & 0x1f)).append_char(0x80 | (n & 0x3f));
        if (n <= 0xFFFF)
            return append_char(0xe0 | ((n >> 12) & 0xf)).append_char(0x80 | ((n >> 6) & 0x3f)).append_char(0x80 | (n & 0x3f));
        if (n <= 0x1FFFFF)
            return append_char(0xf0 | ((n >> 18) & 0x7)).append_char(0x80 | ((n >> 12) & 0x3f)).append_char(0x80 | ((n >> 6) & 0x3f)).append_char(0x80 | (n & 0x3f));
        if (n <= 0x3FFFFFF)
            return append_char(0xf8 | ((n >> 24) & 0x3)).append_char(0x80 | ((n >> 18) & 0x3f)).append_char(0x80 | ((n >> 12) & 0x3f)).append_char(0x80 | ((n >> 6) & 0x3f)).append_char(0x80 | (n & 0x3f));

        return append_char(0xfc | ((n >> 30) & 0x1)).append_char(0x80 | ((n >> 24) & 0x3f)).append_char(0x80 | ((n >> 18) & 0x3f)).append_char(0x80 | ((n >> 12) & 0x3f)).append_char(0x80 | ((n >> 6) & 0x3f)).append_char(0x80 | (n & 0x3f));
    }

    str_t &  append_chars( ZSTRINGS_SIGNED n, TCHARACTER filler )
    {
        ZSTRINGS_SIGNED l = get_length(), nl = l + n;
        set_length( nl );
        for (ZSTRINGS_SIGNED i=l;i<nl;++i)
            core()[i] = filler;
        return *this;
    }
    str_t &  align(ZSTRINGS_SIGNED reqlen, TCHARACTER filler)
    {
        ZSTRINGS_SIGNED l = get_length();
        if (l < reqlen)
        {
            set_length(reqlen);
            for (ZSTRINGS_SIGNED i=l;i<reqlen;++i)
                core()[i] = filler;
        }
        return *this;
    }

    str_t &  fill( ZSTRINGS_SIGNED count, TCHARACTER filler )
    {
        set_length(count, false);
        for (ZSTRINGS_SIGNED i=0;i<count;++i)
            core()[i] = filler;
        return *this;
    }
    str_t &  fill(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED sz, TCHARACTER filler)
    {
        if (ZSTRINGS_ASSERT( idx + sz <= core.len() ))
        {
            core.change(get_length(), zstrings_internal::mod_fill<TCHARACTER>(idx, sz, filler));
        }
        return *this;
    }

    ZSTRINGS_SIGNED     find_last_pos(TCHARACTER c) const
    {
        ZSTRINGS_SIGNED l = get_length();
        TCHARACTER temp;
        while (l>0)
        {
            --l;
            temp = *(core()+l);
            if (temp == c) return l;
        }
        return -1;
    };

    ZSTRINGS_SIGNED     find_last_pos(const sptr<TCHARACTER> &s) const
    {
        ZSTRINGS_SIGNED idx = get_length() - s.l;
        if (s.l == 0) return idx;
        for(;idx >= 0;)
        {
            if (core()[idx] == s.s[0])
            {
                if (blk_cmp(core() + idx, s.s, s.l * sizeof(TCHARACTER))) return idx;
            }
            --idx;
        }
        return -1;
    };

    ZSTRINGS_SIGNED     find_last_pos_of(const sptr<TCHARACTER> &cc) const
    {
        ZSTRINGS_SIGNED l = get_length();
        TCHARACTER temp;
        while (l>0)
        {
            --l;
            temp = *(core()+l);
            if (CHARz_findn(cc.s,temp,cc.l)>=0) return l;
        }
        return -1;
    };

    template<typename TCHARACTER2> ZSTRINGS_SIGNED     find_pos_of(ZSTRINGS_SIGNED index, const sptr<TCHARACTER2> &cc) const
    {
        ZSTRINGS_SIGNED l = get_length();
        TCHARACTER2 temp;
        while (index<l)
        {
            temp = (TCHARACTER2)*(core()+index);
            if (CHARz_findn<TCHARACTER2>(cc.s,temp,cc.l)>=0) return index;
            ++index;
        }
        return -1;
    };

    ZSTRINGS_SIGNED find_word_begin(ZSTRINGS_SIGNED i, const TCHARACTER *c) const
    {
        if (i < 0) return -1;
        while( CHARz_find( c, get_char(i) ) >= 0 && get_char(i) != 0 ) ++i;
        if ( i == get_length() ) return -1;
        return i;
    }
    ZSTRINGS_SIGNED find_word_end(ZSTRINGS_SIGNED i, const TCHARACTER *c) const
    {
        if (i < 0) return -1;
        while( CHARz_find( c, get_char(i) ) >= 0 && get_char(i) != 0 ) ++i;
        while( CHARz_find( c, get_char(i) ) < 0 && get_char(i) != 0 ) ++i;
        return i;
    }

    ZSTRINGS_SIGNED     find_pos_t(ZSTRINGS_SIGNED idx, const sptr<TCHARACTER> &subs, TCHARACTER c = '?') const // find substring. c is any char // str_t("abc5abc").find_pos_t(0,"c?a",'?') == 2
    {
        if (core.len() == 0) return ((idx == 0) && (subs.l == 0)) ? 0 : -1;

        sptr<TCHARACTER> s = subs;
        for(;s && s.s[0] == c; ++s) ;
        ZSTRINGS_SIGNED skip = subs.l - s.l;
        idx += skip;
        for (;;)
        {
            ZSTRINGS_SIGNED left = core.len() - idx;
            if (left < s.l) return -1;
            ZSTRINGS_SIGNED temp = CHARz_findn(core() + idx, *s.s, left);
            if (temp < 0) return -1;
            idx += temp;
            if ((idx + s.l) > get_length()) return -1;

            bool gotcha = true;
            for(ZSTRINGS_SIGNED i=0;i<s.l;++i)
            {
                if (s.s[i] == c) continue;
                if (*(core() + idx + i) != s.s[i]) { gotcha = false; break; }
            }

            if (gotcha) return idx - skip;
            ++idx;
        }
        //return -1;
    };

    ZSTRINGS_SIGNED     find_pos(TCHARACTER c) const {return (core.len() == 0) ? -1 : CHARz_findn(core(),c, core.len());};
    ZSTRINGS_SIGNED     find_pos(ZSTRINGS_SIGNED idx, TCHARACTER c) const 
    {
        if (core.len() == 0 || idx >= core.len()) return -1;
        ZSTRINGS_SIGNED r = CHARz_findn(core()+idx,c, core.len()-idx);
        return (r < 0)?-1:(idx+r);
    };
    ZSTRINGS_SIGNED     find_pos(ZSTRINGS_SIGNED idx, const sptr<TCHARACTER> &s) const
    {
        if (core.len() == 0) return ( (idx == 0) && (s.l == 0) ) ? 0 : -1;
        if (idx + s.l > core.len()) return -1;
        if (s.l == 0 && idx == core.len()) return idx;
        for(;;)
        {
            ZSTRINGS_SIGNED left = core.len() - idx;
            if (left < s.l) return -1;
            ZSTRINGS_SIGNED temp = CHARz_findn(core() + idx, *s.s, left);
            if (temp < 0) return -1;
            idx += temp;
            if ((idx + s.l) > get_length()) return -1;
            if (blk_cmp(core() + idx, s.s, s.l * sizeof(TCHARACTER))) return idx;
            ++idx;
        }
        //return -1;
    };
    ZSTRINGS_SIGNED     find_pos(const sptr<TCHARACTER> &s) const
    {
        return find_pos( 0, s );
    }
    template<class CORE2> ZSTRINGS_SIGNED     find_pos(const str_t<TCHARACTER, CORE2> &s) const
    {
        return find_pos( 0, s.as_sptr() );
    }
    template<class CORE2> ZSTRINGS_SIGNED     find_pos(const ZSTRINGS_SIGNED idx, const str_t<TCHARACTER, CORE2> &s) const {return find_pos(idx, s.as_sptr());}

    ZSTRINGS_SIGNED skip( ZSTRINGS_SIGNED from, TCHARACTER ch = 32 ) const
    {
        if (ZSTRINGS_ASSERT(from >= 0 && from < core.len()))
            while( get_char(from) == ch ) ++from;
        return from;
    }

    template<typename ALG> ZSTRINGS_SIGNED skip(ZSTRINGS_SIGNED from, ALG alg) const
    {
        if (ZSTRINGS_ASSERT(from >= 0 && from < core.len()))
        {
            ZSTRINGS_SIGNED l = get_length();
            while (from < l && alg(get_char(from))) ++from;
        }
        return from;
    }

    TCHARACTER   get_char(ZSTRINGS_SIGNED idx) const
    {
        if (core.len() == 0) return 0;
        if (ZSTRINGS_ASSERT(idx >= 0 && idx < core.len()))
            return core()[idx];
        return 0;
    }


    TCHARACTER   get_last_char() const
    {
        if (core.len() == 0) return 0;
        return core()[ get_length() - 1 ];
    }


    str_t &      set_char(ZSTRINGS_SIGNED idx, const TCHARACTER c)
    {
		if (c != get_char(idx))
		{
			core.change( get_length(), zstrings_internal::mod_preserve<TCHARACTER>(get_length()) );
			core()[idx] = c;
		}
		return *this;
    }

    TCHARACTER &operator[](ZSTRINGS_SIGNED idx)
    {
        return str()[idx];
    }
    TCHARACTER operator[](ZSTRINGS_SIGNED idx) const //-V659
    {
        return get_char(idx);
    }

    ZSTRINGS_SIGNED count_chars( TCHARACTER c ) const
    {
        ZSTRINGS_SIGNED cnt = 0;
        const TCHARACTER *s = _cstr();
        for (;*s != 0; ++ s)
        {
            if (*s == c) ++cnt;
        }
        return cnt;
    }

	str_t & crop(ZSTRINGS_SIGNED idx0, ZSTRINGS_SIGNED idx1)
	{
		ZSTRINGS_SIGNED len = get_length();
		if (idx0 > idx1) return *this;
		if (idx0 < 0) idx0 = 0;
		if (idx1 > len) idx1 = len;
		if (idx0 > 0 || idx1 < len) core.change( idx1 - idx0, mod_crop(idx0, idx1) );
		return *this;

	}

    strpart get_trimmed() const
    {
        ZSTRINGS_SIGNED tlen = get_length();
        ZSTRINGS_SIGNED i0 = 0, i1 = tlen - 1;
        for( ;i0 < tlen && CHAR_is_hollow( get_char(i0)); ++i0 );
        for( ;i1 >=0 && CHAR_is_hollow( get_char(i1)); --i1 );
        return substr(i0,i1+1);
    }

    str_t & trim()						// removes 0x20,0x9,0x0d,0x0a
    {
        ZSTRINGS_SIGNED tlen = get_length();
        const TCHARACTER * first = _cstr();
        const TCHARACTER * beg = first;
        const TCHARACTER * end = beg + tlen - 1;
        while ( CHAR_is_hollow( *beg ) ) ++beg;
        while ( end >= beg && CHAR_is_hollow( *end ) ) --end;
        ZSTRINGS_SIGNED cutpos_rite = (ZSTRINGS_SIGNED)(end - first + 1);
        ZSTRINGS_SIGNED cutpos_left = (ZSTRINGS_SIGNED)(beg - first);
		if (cutpos_left > 0 || cutpos_rite < tlen) core.change( cutpos_rite - cutpos_left, zstrings_internal::mod_crop<TCHARACTER>(cutpos_left, cutpos_rite) );
		return *this;
    }

    str_t &  trim_left(TCHARACTER c = ' ')
    {
		ZSTRINGS_SIGNED tlen = get_length();
        if (tlen == 0) return *this;
        const TCHARACTER *s = core();
        for (;;) { if (*s != c) break; else s++;}
		ZSTRINGS_SIGNED cutchars = ZSTRINGS_SIGNED(s - core());
		if (cutchars > 0) core.change( tlen - cutchars, zstrings_internal::mod_crop<TCHARACTER>(cutchars, tlen) );
		return *this;
    }

    str_t &  trim_right(TCHARACTER c = ' ')
    {
		ZSTRINGS_SIGNED tlen = get_length();
		if (tlen == 0) return *this;
        TCHARACTER *s = core() + tlen - 1;
        while (s >= core()) { if (*s != c) break; else s--;}
		ZSTRINGS_SIGNED cutchars = ZSTRINGS_SIGNED((core() + tlen - 1) - s);
		if (cutchars > 0) core.change( tlen - cutchars, zstrings_internal::mod_crop<TCHARACTER>(0, tlen-cutchars) );
		return *this;
    }

    str_t &  trim_right(const sptr<TCHARACTER> &c)
    {
        ZSTRINGS_SIGNED tlen = get_length();
        if (tlen == 0) return *this;
        TCHARACTER *s = core() + tlen - 1;
        while (s >= core()) { if (CHARz_findn(c.s, *s, c.l) < 0) break; else s--; }
        ZSTRINGS_SIGNED cutchars = ZSTRINGS_SIGNED((core() + tlen - 1) - s);
        if (cutchars > 0) core.change(tlen - cutchars, zstrings_internal::mod_crop<TCHARACTER>(0, tlen - cutchars));
        return *this;
    }

    str_t &    expand_escapes()
    {
        if (core.len() == 0) return *this;
		core.change( core.len(), zstrings_internal::mod_expand_escape_codes<TCHARACTER>() );
        return *this;
    }

	str_t &  format_left(ZSTRINGS_SIGNED nn, TCHARACTER c = ' ') // nn - desirable string length // use '0' for numbers
	{
		ZSTRINGS_SIGNED len = get_length();
		if (len == 0) return fill(nn, c);

		ZSTRINGS_SIGNED need_add = nn - len;
		ZSTRINGS_SIGNED need_del = 0;
		if (need_add < 0)
		{
			TCHARACTER *s = core();
			TCHARACTER *e = core() + len;
			while ((s<e) && (need_add<0) && (*s == c)) {++s; ++need_add; ++need_del;}
		}
		if (need_add <= 0 && need_del == 0) return *this;
		if (need_add < 0) need_add = 0;
		core.change( len + need_add - need_del, zstrings_internal::mod_format_left<TCHARACTER>( need_add, need_del, c ) );
		return *this;
	}

	str_t &  adapt_left(ZSTRINGS_SIGNED n, TCHARACTER c = ' ') // n - desirable number of chars c at left of string
	{
		ZSTRINGS_SIGNED len = get_length();
		if (len == 0) return fill(n, c);

		TCHARACTER *s = core();
		TCHARACTER *e = core() + len;
		while ((s<e) && (n>0) && (*s == c)) {s++; n--;}
		ZSTRINGS_SIGNED need_del = 0;
		while ((s<e) && (*s == c)) {s++; need_del++;}
		if ((n|need_del) == 0) return *this;
		core.change( len + n - need_del, zstrings_internal::mod_format_left<TCHARACTER>( n, need_del, c ) );

		return *this;
	}


    str_t &  adapt_right(ZSTRINGS_SIGNED n, TCHARACTER c = ' ') // n - desirable number of chars c at right of string
    {
		ZSTRINGS_SIGNED len = get_length();
		if (len == 0) return fill(n, c);

        TCHARACTER *s = core() + len - 1;
        while ((s>=core()) && (n>0) && (*s == c)) {s--; n--;}
        ZSTRINGS_SIGNED need_del = 0;
        while ((s>=core()) && (*s == c)) {s--; need_del++;}
        if ((n|need_del) == 0) return *this;
		core.change( len + n - need_del, zstrings_internal::mod_format_right<TCHARACTER>( n, need_del, c ) );
        return *this;
    }

    template<class CORE2> str_t<TCHARACTER, CORE> &  insert(const ZSTRINGS_SIGNED idx, const str_t<TCHARACTER, CORE2> &s) { return insert(idx,s.as_sptr()); };
    str_t &  insert(const ZSTRINGS_SIGNED idx, const sptr<TCHARACTER> &s)
    {
		ZSTRINGS_SIGNED len = get_length();
		ZSTRINGS_ASSERT( idx <= len );
		core.change( s.l + len, zstrings_internal::mod_insert<TCHARACTER>(idx, s.s, s.l) );
        return *this;
    }

    str_t &  insert(const ZSTRINGS_SIGNED idx, TCHARACTER c)
    {
		ZSTRINGS_SIGNED len = get_length();

        if (len == 0)
        {
			ZSTRINGS_ASSERT( idx == 0 );
			return set_as_char(c);
        }

		ZSTRINGS_ASSERT( idx <= len );
		core.change( 1 + len, zstrings_internal::mod_insert_char<TCHARACTER>(idx, c) );
		return *this;
    }

    template<class CORE1, class CORE2> str_t<TCHARACTER, CORE> &  replace_all(ZSTRINGS_SIGNED sme, const str_t<TCHARACTER, CORE1> &substr, const str_t<TCHARACTER, CORE2> &strreplace)
    {
        ZSTRINGS_SIGNED tlen = get_length() - sme;
        if(tlen<1 || tlen<substr.get_length()) return *this;

        for(;;)
        {
            if((sme=find_pos(sme,substr))<0) break;
            replace( sme, substr.get_length(), strreplace );
            sme += strreplace.get_length();
        }
        return *this;
    }

    template<class CORE1, class CORE2> str_t<TCHARACTER, CORE> &  replace_all(const str_t<TCHARACTER, CORE1> &substr, const str_t<TCHARACTER, CORE2> &strreplace)
    {
		return replace_all(substr.as_sptr(), strreplace.as_sptr());
    }

	str_t<TCHARACTER, CORE> &  replace_all(const sptr<TCHARACTER> &substr, const sptr<TCHARACTER> &strreplace)
	{
		ZSTRINGS_SIGNED tlen = get_length();
		if(tlen<1 || tlen<substr.l) return *this;

		ZSTRINGS_SIGNED sme=0;
		for(;;)
		{
			if((sme=find_pos(sme, substr))<0) break;
			replace( sme, substr.l, strreplace );
			sme += strreplace.l;
		}
		return *this;
	}

    str_t &  replace_all(ZSTRINGS_SIGNED sme, TCHARACTER c1, TCHARACTER c2)
    {
		ZSTRINGS_SIGNED len = get_length();
		if (len == 0) return *this;
		bool changed = false;
        for (ZSTRINGS_SIGNED i=sme;i<len;++i)
        {
            if (core()[i] == c1)
            {
				if (!changed)
				{
					core.change(len, zstrings_internal::mod_preserve<TCHARACTER>(len));
					changed = true;
				}
                str()[i] = c2;
            }
        }
        return *this;
    };

    str_t &  replace_all(TCHARACTER c1, TCHARACTER c2)
    {
		ZSTRINGS_SIGNED len = get_length();
        if (len == 0) return *this;
		bool changed = false;

        for (ZSTRINGS_SIGNED i=0;i<len;++i)
        {
            if (core()[i] == c1)
            {
                if (!changed)
                {
					core.change(len, zstrings_internal::mod_preserve<TCHARACTER>(len));
					changed = true;
                }
                str()[i] = c2;
            }
        }
        return *this;
    };
    str_t &  replace(const ZSTRINGS_SIGNED idx, const ZSTRINGS_SIGNED sz, const TCHARACTER * const s)
    {
        return replace(idx,sz,sptr<TCHARACTER>(s));
    };
    template<typename CORE2> str_t &  replace(ZSTRINGS_SIGNED idx, const ZSTRINGS_SIGNED sz, const str_t<TCHARACTER, CORE2> &s)
    {
        return replace(idx,sz,s.as_sptr());
    };
    str_t &  replace(ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED sz, const sptr<TCHARACTER> &s)
    {
		ZSTRINGS_SIGNED len = core.len();
        
        ZSTRINGS_ASSERT(len > 0 || (sz == 0 && idx == 0));
        ZSTRINGS_ASSERT( idx + sz <= len );

		ZSTRINGS_SIGNED n = len + s.l - sz;
		core.change(n, zstrings_internal::mod_replace<TCHARACTER>(idx, sz, s.s, s.l));
        return *this;
    }

    str_t<TCHARACTER, CORE> &  cut(const ZSTRINGS_SIGNED idx, ZSTRINGS_SIGNED sz = 1)
    {
		ZSTRINGS_SIGNED len = get_length();
		if (idx >= len) return *this;
		if ((idx + sz) > len) sz = len - idx;
		core.change( len - sz, zstrings_internal::mod_cut<TCHARACTER>(idx, sz) );
        return *this;

    }

    str_t<TCHARACTER, CORE> &  cut(const TCHARACTER *charset)
    {
        for (ZSTRINGS_SIGNED i=get_length()-1;i>=0;--i)
        {
            if ( CHARz_find(charset, get_char(i)) >= 0 ) cut( i, 1 );
        }
        return *this;
    }

    template<class CORE2> str_t<TCHARACTER, CORE> &  prefixate(const str_t<TCHARACTER, CORE2> &s)
    {
        // "abcde".prefixate("abczz") => "abc"

        ZSTRINGS_SIGNED cnt = get_length();
        for (ZSTRINGS_SIGNED i=0;i<cnt;++i)
        {
            if ( s.get_char(i) != get_char(i) )
            {
                return set_length( i );
            }
        }
        return *this;
    }


    strpart substr(ZSTRINGS_SIGNED idx0, ZSTRINGS_SIGNED idx1) const /// returns substring from idx0 to idx1
    {
        if (core.len() == 0) return strpart();
        if (idx1 > get_length()) idx1 = get_length();
        if (idx0 < 0) idx0 = 0;
        if (idx0 > idx1) return strpart();
        return strpart(sptr<TCHARACTER>(core() + idx0, idx1 - idx0));
    }
    strpart substr(const ZSTRINGS_SIGNED idx) const /// returns substring from idx to end
    {
        if (core.len() == 0) return strpart();
        if (idx > get_length()) return strpart();
        return strpart(sptr<TCHARACTER>(core() + idx, get_length() - idx));
    }
    strpart substr(ZSTRINGS_SIGNED &idx, TCHARACTER obr, TCHARACTER cbr) const /// find substring between obr and cbr characters
    {
        if (core.len() == 0) return strpart();
        ZSTRINGS_SIGNED bri = 0;
        ZSTRINGS_SIGNED store = 0;
        for(ZSTRINGS_SIGNED len = get_length();idx < len;)
        {
            TCHARACTER c = get_char(idx);
            ++idx;
            if (c == cbr)
            {
                --bri;
                if (bri <= 0)
                {
                    return strpart(sptr<TCHARACTER>(core() + store, idx-store-1));
                }
            }
            if (c == obr)
            {
                store = idx;
                ++bri;
            }
        }

        return strpart();
    }

    template<class CORE2> bool substr( str_t<TCHARACTER, CORE2> &out, ZSTRINGS_SIGNED index, TCHARACTER separator ) /// get token substring
    {
        if (get_length() == 0) return false;

        ZSTRINGS_SIGNED cindex = 0;
        ZSTRINGS_SIGNED i = 0;
        for(;;)
        {
            ZSTRINGS_SIGNED k = find_pos(i,separator);
            if (k < 0)
            {
                if (cindex == index)
                {
                    out = substr<CORE2>(i);
                    return true;
                }
                break;
            }
            else
            {
                if (cindex == index)
                {
                    out = substr<CORE2>(i,k);
                    return true;
                }
                ++cindex;

            }
            i = k + 1;
        }

        return false;
    }

	template<class CORE2> void substr(str_t<TCHARACTER, CORE2> & str, ZSTRINGS_SIGNED index, const TCHARACTER * separators_chars) const
	{
		ZSTRINGS_SIGNED sme=token_offset(index,separators_chars);
		ZSTRINGS_SIGNED len=token_len(sme,separators_chars);
		str.set(_cstr()+sme,len);
	}
	strpart substr(ZSTRINGS_SIGNED index,const TCHARACTER * separators_chars) const
	{
		ZSTRINGS_SIGNED sme=token_offset(np,separators_chars);
		return strpart( sptr<TCHARACTER>(_cstr()+sme, token_len(sme,separators_chars)) );
	}


    template<typename NUMTYPE1, typename NUMTYPE2, typename SEPCHARTYPE> void split(NUMTYPE1 &beg, NUMTYPE2 &rem, const SEPCHARTYPE * separators_chars, NUMTYPE1 defbeg = 0, NUMTYPE2 defrem = 0) const
    {
        const TCHARACTER *c = _cstr();

        for(;*c; ++c)
        {
            if (CHARz_find<SEPCHARTYPE>(separators_chars, zstrings_internal::cvtchar<SEPCHARTYPE, TCHARACTER>(*c)) >= 0)
            {
                ZSTRINGS_SIGNED i = ZSTRINGS_SIGNED(c - _cstr());
                if (!zstrings_internal::CHARz_to_num<TCHARACTER>( beg, _cstr(), i ))
                {
                    beg = defbeg;
                }

                if (!zstrings_internal::CHARz_to_num<TCHARACTER>( rem, c + 1, core.len() - i - 1 ))
                {
                    rem = defrem;
                }
                return;
            }
        }

        if (!zstrings_internal::CHARz_to_num<TCHARACTER>( beg, _cstr(), core.len() ))
        {
            beg = defbeg;
        }
        rem = defrem;
    }


    template<class CORE2, class CORE3, typename SEPCHARTYPE> void split(str_t<TCHARACTER, CORE2> &beg, str_t<TCHARACTER, CORE3> &rem, const SEPCHARTYPE * separators_chars) const
    {

        const TCHARACTER *c = _cstr();
        const TCHARACTER *first = c;

        for(;*c; ++c)
        {
			if (CHARz_find<SEPCHARTYPE>(separators_chars, zstrings_internal::cvtchar<SEPCHARTYPE, TCHARACTER>(*c)) >= 0)
			{
                ZSTRINGS_SIGNED beglen = ZSTRINGS_SIGNED(c - first);
                beg.set( sptr(first, beglen) );
                rem.set( sptr(c + 1, get_length() - beglen - 1) );
                return;
            }
        }

        beg = *this;
        rem.clear();
    }


    template<class CORE2, class CORE3> void split(str_t<TCHARACTER, CORE2> &beg, str_t<TCHARACTER, CORE3> &rem, TCHARACTER separator) const
    {
        const TCHARACTER *c = _cstr();
        const TCHARACTER *first = c;
        for(;*c; ++c)
        {
            if (*c == separator) 
            {
                ZSTRINGS_SIGNED beglen = ZSTRINGS_SIGNED(c - first);
                beg.set(sptr<TCHARACTER>(first, beglen));
                rem.set(sptr<TCHARACTER>(c + 1, get_length() - beglen - 1));
                return;
            }
        }

        beg = *this;
        rem.clear();
    }

    bool equals_ignore_case(const sptr<TCHARACTER> &s) const
    {
        if (s.l != get_length()) return false;
        if (get_length() == 0) return true;
        return CHARz_equal_ignore_case(core(), s.s, s.l);
    }
    bool equals_ignore_case(const str_t<TCHARACTER, CORE> &s) const
    {
        if ( core == s.core ) return true;
        return equals_ignore_case(s.as_sptr());
    }
    template<class CORE2> bool equals_ignore_case(const str_t<TCHARACTER, CORE2> &s) const
    {
        return equals_ignore_case(s.as_sptr());
    }

    //spart
    bool equals(const sptr<TCHARACTER> &s) const
    {
        if (s.l != get_length()) return false;
        if (get_length() == 0) return true;
        return blk_cmp(core(), s.s, s.l * sizeof(TCHARACTER));
    }
    bool equals(const sptr<TCHARACTER> &s1, const sptr<TCHARACTER> &s2) const // compare with concatenation of s1 and s2
    {
        if ((s1.l+s2.l) != get_length()) return false;
        if (get_length() == 0) return true;
        return blk_cmp(core(), s1.s, s1.l * sizeof(TCHARACTER)) && blk_cmp(core() + s1.l, s2.s, s2.l * sizeof(TCHARACTER));
    }

    //same core
    bool equals(const str_t<TCHARACTER, CORE> &s) const
    {
        if ( core == s.core ) return true;
        return equals(s.as_sptr());
    }

    //other core
    template<class CORE2> bool equals(const str_t<TCHARACTER, CORE2> &s) const
    {
        return equals(s.as_sptr());
    }

    bool    operator == (const sptr<TCHARACTER> &s) const {return equals(s);}
    template<class CORE2> bool    operator == (const str_t<TCHARACTER, CORE2> &s) const {return equals(s);}

    bool    operator != (const sptr<TCHARACTER> &s) const {return !equals(s);}
    template<class CORE2> bool    operator != (const str_t<TCHARACTER, CORE2> &s) const {return !equals(s);}

    str_t & append_as_float(float n, ZSTRINGS_SIGNED zpz = 8)
    {
        return append_as_double((double)n,zpz);
    }
    str_t & append_as_double(double n, ZSTRINGS_SIGNED zpz = 8)
    {
        str_t<ZSTRINGS_ANSICHAR, str_core_static_c<ZSTRINGS_ANSICHAR, 64> > tstr(63,false);
        ZSTRINGS_SIGNED dec,sign;
        ZSTRINGS_SIGNED count=0;
        _fcvt_s(tstr.str(), 63, n, (int)zpz, &dec,&sign);
        if (sign) append_char(TCHARACTER('-'));
        tstr.set_length();

        if(dec<0)
        {
            append_char(TCHARACTER('0'));
            if(tstr.get_length()>0)
            {
                append_char(TCHARACTER('.'));
                append(TCHARACTER('0'),-dec);
                for(ZSTRINGS_SIGNED i=tstr.get_length()-1;i>=0;--i)
                    if(tstr.get_char(i)=='0') ++count; else break;
                if(tstr.get_length()>count)
                {
                    for(ZSTRINGS_SIGNED yu=0, ccc = tstr.get_length()-count;yu<ccc;yu++) append_char(TCHARACTER(tstr.get_char(yu)));
                }
            }
        } else
        {
            if(dec>0)
            {
                for(ZSTRINGS_SIGNED yu=0;yu<dec;++yu) append_char(TCHARACTER(tstr.get_char(yu)));
            } else { append_char(TCHARACTER('0')); }

            for(ZSTRINGS_SIGNED i=tstr.get_length()-1;i>=dec;--i) if(tstr.get_char(i)=='0') ++count; else break;
            if(dec<ZSTRINGS_SIGNED(tstr.get_length()-count)) 
            {
                append_char(TCHARACTER('.'));
                for(ZSTRINGS_SIGNED yu=0, ccc=tstr.get_length()-count-dec;yu<ccc;yu++)
                    append_char(TCHARACTER(tstr.get_char(dec+yu)));
            }
        }
        return *this;
    }

    template<typename NUMTYPE> str_t & append_as_num(NUMTYPE n)
    {
        TCHARACTER buf[sizeof(NUMTYPE) * 3];

        if (zstrings_internal::is_signed<NUMTYPE>::value && n<0)
        {
            append_char('-');
			zstrings_internal::invert<NUMTYPE>()(n);
        }

        ZSTRINGS_SIGNED szbyte;
        TCHARACTER *tcalced = CHARz_make_str_unsigned( buf, szbyte, n );
        return append(sptr<TCHARACTER>(tcalced,szbyte/sizeof(TCHARACTER)-1));
    }

    template<> str_t & append_as_num<float>(float n) { return append_as_float(n); }
    template<> str_t & append_as_num<double>(double n) { return append_as_double(n); }

    str_t & append_as_int(int n) { return append_as_num<int>(n); }
    str_t & append_as_uint(unsigned int n) { return append_as_num<unsigned int>(n); }

    str_t & append(TCHARACTER sep, sptr<TCHARACTER> s)
    {
        if (s.l == 0) return *this;
        ZSTRINGS_SIGNED prevlen = get_length();
        ZSTRINGS_SIGNED prevlen_x = prevlen;
        ZSTRINGS_SIGNED newlen = prevlen + s.l;
        if (prevlen)
        {
            if (get_last_char() != sep && s.s[0] != sep)
            {
                ++prevlen_x;
                ++newlen;
            } else if ( get_last_char() == sep && s.s[0] == sep )
            {
                --newlen;
                s = s.skip(1);
            }
        } else if (s.s[0] == sep )
        {
            --newlen;
            s = s.skip(1);
        }
        core.change(newlen, zstrings_internal::mod_preserve<TCHARACTER>(newlen));
        blk_UNIT_copy_fwd<TCHARACTER>(core() + prevlen_x, s.s, s.l);
        if (prevlen_x > prevlen) core()[prevlen] = sep;
        return *this;
    }

    str_t & append(const sptr<TCHARACTER> &s)
    {
        if (s.l == 0) return *this;
        ZSTRINGS_SIGNED prevlen = get_length();
        core.change(prevlen + s.l, zstrings_internal::mod_preserve<TCHARACTER>(prevlen + s.l));
        blk_UNIT_copy_fwd<TCHARACTER>(core() + prevlen, s.s, s.l);
        return *this;
    }

    str_t & append(TCHARACTER sep, const str_t &s) {return this->append(sep, s.as_sptr());};
    str_t & append(const str_t &s) {return this->append(s.as_sptr());};

    template<class CORE2> str_t & append(const str_t<TCHARACTER, CORE2> &s) {return this->append(s.as_sptr());};
	template<typename TCHARACTER2, class CORE2> str_t & appendcvt(const str_t<TCHARACTER2, CORE2> &s) { xappend(*this, s.as_sptr()); return *this; };
	
	str_t & append(const TCHARACTER * const s) {return this->append(sptr<TCHARACTER>(s));};
	template<typename TCHARACTER2> str_t & appendcvt(const TCHARACTER2 * const s) { xappend(*this, sptr<TCHARACTER2>(s)); return *this; };

    str_t & append_char(TCHARACTER c)
    {
		ZSTRINGS_SIGNED prevlen = get_length();
		core.change(prevlen + 1,zstrings_internal::mod_preserve<TCHARACTER>(prevlen + 1));
		core()[prevlen] = c;
		return *this;
    }

    str_t & append(const TCHARACTER c, ZSTRINGS_SIGNED count)
    {
		if (count == 0) return *this;

		ZSTRINGS_SIGNED prevlen = get_length();
		core.change(prevlen + count,zstrings_internal::mod_preserve<TCHARACTER>(prevlen + count));
		for (TCHARACTER *fi = core() + prevlen;count > 0;--count)
			*(fi++) = c;

		return *this;
    }

    /*
        convert on append is evil: it can lead to invisible conversion
	    template<typename THCARACTER2, typename CORE2> str_t & operator += (const str_t<THCARACTER2, CORE2> &s) {return append<THCARACTER2, CORE2>(s);};
    */

    str_t & operator += (const TCHARACTER * const s) {return this->append(sptr<TCHARACTER>(s));};

    str_t & append_as_hex(ZSTRINGS_UNSIGNED b)
    {
        return append_as_hex( ZSTRINGS_BYTE( b >> 24 ) ).
            append_as_hex( ZSTRINGS_BYTE( 0xFF & ( b >> 16 ) ) ).
            append_as_hex( ZSTRINGS_BYTE( 0xFF & ( b >> 8 ) ) ).
            append_as_hex( ZSTRINGS_BYTE( 0xFF & ( b >> 0 ) ) );
    }

    str_t & append_as_hex(ZSTRINGS_BYTE b)
    {
        static TCHARACTER table[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        return append_char(table[b >> 4]).append_char(table[b & 15]);
    }
    str_t & append_as_hex(const void *ib, ZSTRINGS_SIGNED len)
    {
        static TCHARACTER table[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
        const ZSTRINGS_BYTE *b = (const ZSTRINGS_BYTE *)ib;
        ZSTRINGS_SIGNED prevlen = get_length();
        core.change(prevlen + len * 2, zstrings_internal::mod_preserve<TCHARACTER>(prevlen + len * 2));
        TCHARACTER *to = core() + prevlen;
        for(ZSTRINGS_SIGNED i=0;i<len;++i, ++to, ++b)
        {
            *to = table[(*b) >> 4];
            ++to; *to = table[(*b) & 15];
        }
        return *this;
    }
    str_t & append_as_bin(ZSTRINGS_BYTE b)
    {
        for (ZSTRINGS_SIGNED i=7;i>=0;--i)
        {
            append_char( (b & (1<<i)) != 0 ? '1' : '0' );
        }

        return *this;
    }

    str_t & set_as_float(float n)
    {
        clear();
        return append_as_float(n);
    }

    str_t & set_as_double(double n)
    {
        clear();
        return append_as_double(n);
    }
#pragma warning(push)
#pragma warning(disable:4702) // unreachable code
    template<typename NUMTYPE> str_t & set_as_num(NUMTYPE n)
    {
        TCHARACTER buf[sizeof(NUMTYPE) * 3];

        if (zstrings_internal::is_signed<NUMTYPE>::value && n<0)
        {
            set_as_char('-');
            zstrings_internal::invert<NUMTYPE>()(n);

            ZSTRINGS_SIGNED szbyte;
            TCHARACTER *tcalced = CHARz_make_str_unsigned( buf, szbyte, n );
            return append(sptr<TCHARACTER>(tcalced,szbyte/sizeof(TCHARACTER)-1));
        }

        ZSTRINGS_SIGNED szbyte;
        TCHARACTER *tcalced = CHARz_make_str_unsigned( buf, szbyte, n );
        return set(tcalced,szbyte/sizeof(TCHARACTER)-1);
    }
#pragma warning(pop)
	template<> str_t & set_as_num<float>(float n) { return set_as_float(n); }
	template<> str_t & set_as_num<double>(double n) { return set_as_double(n); }

	str_t & set_as_int(int n) {return set_as_num<int>(n);}
	str_t & set_as_uint(unsigned int n) {return set_as_num<unsigned int>(n);}

    str_t & set_as_char(TCHARACTER c)
    {
		core.change(1, zstrings_internal::mod_copychar<TCHARACTER>(c));
        return *this;
    }

    str_t<TCHARACTER, CORE> &  set_as_utf8( const sptr<ZSTRINGS_ANSICHAR> &utf8 );

    str_t & copy(const sptr<TCHARACTER> &s)
    {
        ZSTRINGS_SIGNED len;
        for (len=0;len<s.l;++len)
        {
            if (s.s[len] == 0) break;
        }
        return set( s, len );
    }

    template<class CORE2> str_t & setcopy(const str_t<TCHARACTER, CORE2> &s)
    {
        sptr<TCHARACTER> sp = s.as_sptr();
        set( sp.s, sp.l );
        return *this;
    }

    str_t & set(const TCHARACTER * const s, ZSTRINGS_SIGNED len) // copy
    {
		if (len == 0)
		{
			core.clear();
			return *this;
		}

		core.change(len, zstrings_internal::mod_copy<TCHARACTER>(s,len));
        return *this;
    }

    str_t & set(const sptr<TCHARACTER> &s) // part core string hint
    {
        core = s;
        return *this;
    }

    str_t & set(const str_t &s)
    {
		core = s.core;
        return *this;
    }

    template<class CORE2> str_t & set(const str_t<TCHARACTER, CORE2> &s)
    {
        xset<true>(*this, s);
        return *this;
    }

	template<typename TCHARACTER2, class CORE2> str_t & setcvt(const str_t<TCHARACTER2, CORE2> &s)
	{
		xset<true>( *this, s );
		return *this;
	}

    str_t & encode_base64( const void * data, ZSTRINGS_UNSIGNED size )
    {
        const ZSTRINGS_BYTE *b = (const ZSTRINGS_BYTE *)data;

        ZSTRINGS_UNSIGNED blockcount = size/3;
        ZSTRINGS_UNSIGNED padding = size - (blockcount * 3);
        if (padding) { padding = 3-padding; ++blockcount; }

        for (ZSTRINGS_UNSIGNED x=0;x<blockcount;x++)
        {
            ZSTRINGS_UNSIGNED indx = x * 3;

            ZSTRINGS_BYTE b1 = indx < size ? b[indx] : 0;
            ++indx;
            ZSTRINGS_BYTE b2 = indx < size ? b[indx] : 0;
            ++indx;
            ZSTRINGS_BYTE b3 = indx < size ? b[indx] : 0;

            ZSTRINGS_BYTE temp, temp1, temp2, temp3, temp4;
            temp1=(ZSTRINGS_BYTE)((b1 & 252)>>2);//first


            temp=(ZSTRINGS_BYTE)((b1 & 3)<<4);
            temp2=(ZSTRINGS_BYTE)((b2 & 240)>>4);
            temp2+=temp; //second


            temp=(ZSTRINGS_BYTE)((b2 & 15)<<2);
            temp3=(ZSTRINGS_BYTE)((b3 & 192)>>6);
            temp3+=temp; //third


            temp4=(ZSTRINGS_BYTE)(b3 & 63); //fourth

            append_char( base64(temp1) );
            append_char( base64(temp2) );
            append_char( base64(temp3) );
            append_char( base64(temp4) );

        }

        switch (padding)
        {
        case 2:set_char(get_length()-2,'=');
        case 1:set_char(get_length()-1,'=');
        }

        return *this;

    }

    void decode_base64( void *data, ZSTRINGS_SIGNED datasize, ZSTRINGS_SIGNED from = 0, ZSTRINGS_SIGNED ilen = -1 ) const
    {
        ZSTRINGS_BYTE *d = (ZSTRINGS_BYTE *)data;
        sptr<TCHARACTER> s;
        s.s = core() + from;
        s.l = ilen < 0 ? (get_length() - from) : ilen;
        static const ZSTRINGS_ANSICHAR cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";
        ZSTRINGS_BYTE inb[4]; //-V112
        for (;s && datasize;)
        {
            ZSTRINGS_SIGNED len, i;
            for (len = 0, i = 0; i < sizeof(inb)/sizeof(ZSTRINGS_BYTE) && (s); ++i) 
            {
                ZSTRINGS_BYTE v = 0;
                while ((s) && v == 0)
                {
                    v = (ZSTRINGS_BYTE)s.s[0]; ++s;
                    v = (ZSTRINGS_BYTE)((v < 43 || v > 122) ? 0 : cd64[v - 43]);
                    if (v)
                    {
                        v = (ZSTRINGS_BYTE)((v == '$') ? 0 : v - 61);
                    }
                }
                if (s)
                {
                    len++;
                    if (v)
                    {
                        inb[i] = (ZSTRINGS_BYTE)(v - 1);
                    }
                }
                else {
                    inb[i] = 0;
                }
            }
            if (len)
            {
                if (datasize) { *d = (ZSTRINGS_BYTE)((inb[0] << 2 | inb[1] >> 4)&0xFF); ++d; --datasize; }
                if (datasize) { *d = (ZSTRINGS_BYTE)((inb[1] << 4 | inb[2] >> 2)&0xFF); ++d; --datasize; }
                if (datasize) { *d = (ZSTRINGS_BYTE)((((inb[2] << 6) & 0xc0) | inb[3])&0xFF); ++d; --datasize; }
            }
        }
    }

    str_t & encode_pointer( const void * p ) // encode format: 'A' + 4 bit
    {
        const ZSTRINGS_BYTE * s = (const ZSTRINGS_BYTE *)&p;
        for( const ZSTRINGS_BYTE * e = s + sizeof(p); s<e; ++s )
            append_char( 'A' + (*s & 15) ).append_char( 'A' + (*s >> 4) );
        return *this;
    }

    const void * decode_pointer( ZSTRINGS_SIGNED from = 0 ) const
    {
        if ( from + (ZSTRINGS_SIGNED)(sizeof(void *)*2) > get_length() )
            return nullptr;
        const void *ptr;
        ZSTRINGS_BYTE * s = (ZSTRINGS_BYTE *)&ptr;
        for( ZSTRINGS_SIGNED i = 0; i < (2*sizeof(void *)); i+=2, ++s )
        {
            ZSTRINGS_BYTE c0 = get_char(from + i) - 'A';
            ZSTRINGS_BYTE c1 = get_char(from + i + 1) - 'A';
            *s = c0 | (c1 << 4);
        }
        return ptr;
    }


    str_t & operator = (const str_t &s) { return set(s); }
    
    /* 
        convert on assign is evil: it can lead to invisible conversion
        template<typename TCHARACTER2, class CORE2> str_t<TCHARACTER, CORE> & operator = (const str_t<TCHARACTER2, CORE2> &s) { return set(s); }
    */

    template<class CORE2> str_t<TCHARACTER, CORE> & operator = (const str_t<TCHARACTER, CORE2> &s) { return set(s); }
    str_t & operator = (const TCHARACTER * const s)   { *this = sptr<TCHARACTER>(s); return *this; } // copy always, fail on part strings
    str_t & operator = (const sptr<TCHARACTER> &s) { set(s.s, s.l); return *this; } // copy always, fail on part strings

    template<class CORE2> ZSTRINGS_SIGNED   operator()( const str_t<TCHARACTER,CORE2> & n ) const { return compare(*this,n); }
    ZSTRINGS_SIGNED							operator()( const TCHARACTER * n ) const { return compare(n, CHARz_len(n), _cstr(), get_length()); }

    static signed char compare(const sptr<TCHARACTER> &s1, const sptr<TCHARACTER> &s2)
    {
        return compare(s1.s,s1.l,s2.s,s2.l);
    }
    static signed char compare(const TCHARACTER * s1, ZSTRINGS_SIGNED s1len, const TCHARACTER * s2, ZSTRINGS_SIGNED s2len) // "A","B"=-1  "A","A"=0  "B","A"=1
    {
        if(!s1len && !s2len) return 0;
        if(!s1len) return -1;
        if(!s2len) return 1;
        if (s1 == s2 && s1len == s2len) return 0;

        ZSTRINGS_SIGNED len=s1len;
        if(len>s2len) len=s2len;

        for(ZSTRINGS_SIGNED i=0;i<len;++i)
        {
            if(s1[i]<s2[i]) return -1;
            if(s1[i]>s2[i]) return 1;
        }
        if (s1len < s2len) return -1;
        if (s1len > s2len) return 1;
        return 0;
    }

    static signed char icompare(const sptr<TCHARACTER> &s1, const sptr<TCHARACTER> &s2)
    {
        return icompare(s1.s,s1.l,s1.s,s1.l);
    }
    static signed char icompare(const TCHARACTER * s1,ZSTRINGS_SIGNED s1len,const TCHARACTER * s2,ZSTRINGS_SIGNED s2len) // "A","B"=-1  "A","A"=0  "B","A"=1
    {
        if(!s1len && !s2len) return 0;
        if(!s1len) return -1;
        if(!s2len) return 1;

        ZSTRINGS_SIGNED len=s1len;
        if(len>s2len) len=s2len;

        TCHARACTER cc[2];

        for(ZSTRINGS_SIGNED i=0;i<len;i++)
        {
            cc[0] = s1[i];
            cc[1] = s2[i];

            ZSTRINGS_SYSCALL(text_lowercase)(cc,2);

            if (cc[0] == cc[1]) continue;

            return (cc[0]<cc[1]) ? -1 : 1;
        }
        if(s1len<s2len) return -1;
        if(s1len>s2len) return 1;
        return 0;
    }

    template<class CORE1, class CORE2> static signed char compare(const str_t<TCHARACTER, CORE1>& s1, const str_t<TCHARACTER, CORE2>& s2) { return compare(s1.as_sptr(), s2.as_sptr());}
    template<class CORE1, class CORE2> static signed char icompare(const str_t<TCHARACTER, CORE1>& s1,const str_t<TCHARACTER, CORE2>& s2) { return icompare(s1.as_sptr(), s2.as_sptr());}

    ZSTRINGS_SIGNED token_count(const TCHARACTER * separators_chars) const
    {
        ZSTRINGS_SIGNED len=get_length(); 
        if(len<1) return 0;

        ZSTRINGS_SIGNED count = 1;
        if(!separators_chars[0]) return 0; // no separatos - no tokens :)

        const TCHARACTER * s = core();
        for(ZSTRINGS_SIGNED i=0;i<len;i++)
            if(CHARz_find(separators_chars, s[i])>=0) count++;
        return count;
    }
    ZSTRINGS_SIGNED token_offset(ZSTRINGS_SIGNED tokenindex, const TCHARACTER * separators_chars) const
    {
        ZSTRINGS_SIGNED len=get_length();
        ZSTRINGS_ASSERT(len>0 && separators_chars[0] || tokenindex>=0);
        ZSTRINGS_SIGNED tokenoffset=0;
        ZSTRINGS_SIGNED currenttoken=0;

        const TCHARACTER * s = core();

        if(tokenindex>0)
        {
            ZSTRINGS_SIGNED i=0;
            for(;i<len;++i)
            {
                if(CHARz_find(separators_chars, s[i])>=0)
                {
                    currenttoken++;
                    tokenoffset = i+1;
                    if(currenttoken==tokenindex) break;
                }
            }
            ZSTRINGS_ASSERT(i<len);
        }
        return tokenoffset;
    }

    ZSTRINGS_SIGNED token_len(ZSTRINGS_SIGNED tokenoffset, const TCHARACTER * separators_chars) const
    {
        ZSTRINGS_SIGNED tlen=get_length(); 
        if(tlen<1 || separators_chars[0]==0 || tokenoffset>tlen) return -1;

        const TCHARACTER * s = core();

		ZSTRINGS_SIGNED i;
        for(i=tokenoffset;i<tlen;i++)
        {
            if(CHARz_find(separators_chars, s[i])>=0) break;
        }
        return i-tokenoffset;
    }

    ZSTRINGS_UNSIGNED crc() const { return get_length() ? ZSTRINGS_CRC32(core(), get_length() * sizeof(TCHARACTER)) : 0;}

    TCHARACTER * begin() { return core(); }
    const TCHARACTER *begin() const { return core(); }

    TCHARACTER *end() { return core() + core.len(); }
    const TCHARACTER *end() const { return core() + core.len(); }

};

template<> inline str_t<ZSTRINGS_ANSICHAR> &  str_t<ZSTRINGS_ANSICHAR>::set_as_utf8( const sptr<ZSTRINGS_ANSICHAR> &utf8 )
{
    set_length( utf8.l );
    set_length(text_utf8_to_ansi(str(), get_length(), utf8));
    return *this;
}

template<> inline str_t<ZSTRINGS_WIDECHAR> &  str_t<ZSTRINGS_WIDECHAR>::set_as_utf8( const sptr<ZSTRINGS_ANSICHAR> &utf8 )
{
    set_length( utf8.l );
    set_length(ZSTRINGS_SYSCALL(text_utf8_to_ucs2)(str(), get_length(), utf8));
    return *this;
}

template <typename TCHARACTER, class CORE1, class CORE2> inline str_t<TCHARACTER, CORE1> operator + (const str_t<TCHARACTER, CORE1> &s1, const str_t<TCHARACTER, CORE2> &s2)
{
    return str_t<TCHARACTER, CORE1>(s1.as_sptr(),s2.as_sptr());
}

template <typename TCHARACTER1, typename TCHARACTER2, class CORE1, class CORE2> inline str_t<ZSTRINGS_WIDECHAR> operator + (const str_t<TCHARACTER1, CORE1> &s1, const str_t<TCHARACTER2, CORE2> &s2)
{
    return str_t<ZSTRINGS_WIDECHAR>( s1.get_length() + s2.get_length(), false ).set(s1).append(s2);
}

template <typename TCHARACTER, class CORE>  inline str_t<TCHARACTER, CORE> operator + (const str_t<TCHARACTER, CORE> &s1, const sptr<TCHARACTER> &s2)
{
    return str_t<TCHARACTER, CORE>(s1.as_sptr(), s2);
}

template <typename TCHARACTER, class CORE> inline str_t<TCHARACTER, CORE> operator + (const sptr<TCHARACTER> &s1, const str_t<TCHARACTER, CORE> &s2)
{
    return str_t<TCHARACTER, CORE>(s1, s2.as_sptr());
}

template <typename TCHARACTER, class CORE> ZSTRINGS_FORCEINLINE bool    operator == (const sptr<TCHARACTER> &s1, const str_t<TCHARACTER, CORE> &s2)
{
    return s2.equals(s1);
}

template <typename TCHARACTER, class CORE> ZSTRINGS_FORCEINLINE bool    operator == (const str_t<TCHARACTER, CORE> &s1, const sptr<TCHARACTER> &s2)
{
    return s1.equals(s2);
}

template <typename TCHARACTER, class CORE1, class CORE2> ZSTRINGS_FORCEINLINE bool    operator == (const str_t<TCHARACTER, CORE1> &s1, const str_t<TCHARACTER, CORE2> &s2)
{
    return s1.as_sptr() == s2.as_sptr();
}


template<bool preparelen, class CORE1, class CORE2> void xset( str_t<ZSTRINGS_ANSICHAR,CORE1>&sto,  const str_t<ZSTRINGS_WIDECHAR,CORE2>&sfrom )
{
	ZSTRINGS_SIGNED len = sfrom.get_length();
    if (len == 0 && sto.get_length() == 0) return;
	if (preparelen)	sto.set_length(len);
	if (sto.get_capacity() <= len) len = sto.get_capacity() - 1;
	if (len > 0) ZSTRINGS_SYSCALL(text_ucs2_to_ansi)(sto.str(), len+1, sfrom.as_sptr(len));
}
template<bool preparelen, class CORE1, class CORE2> void xset( str_t<ZSTRINGS_WIDECHAR,CORE1>&sto,  const str_t<ZSTRINGS_ANSICHAR,CORE2>&sfrom )
{
	ZSTRINGS_SIGNED len = sfrom.get_length();
    if (len == 0 && sto.get_length() == 0) return;
	if (preparelen) sto.set_length(len);
	if (sto.get_capacity() <= len) len = sto.get_capacity() - 1;
	if (len > 0) ZSTRINGS_SYSCALL(text_ansi_to_ucs2)(sto.str(), len+1, sfrom.as_sptr(len));
}
template<bool preparelen, typename TCHARACTER, class CORE1, class CORE2> void xset( str_t<TCHARACTER,CORE1>&sto,  const str_t<TCHARACTER,CORE2>&sfrom )
{
	static_assert( !std::is_same<CORE1, CORE2>::value, "CORE1 == CORE2" );
	sto.set( sfrom.as_sptr() );
}

template<class CORE> void xappend( str_t<ZSTRINGS_ANSICHAR,CORE>&sto,  const sptr<ZSTRINGS_WIDECHAR> &sfrom )
{
    ZSTRINGS_SIGNED len = sfrom.l;
    if (len == 0) return;
	ZSTRINGS_SIGNED tlen = sto.get_length();
	sto.set_length( tlen + len, true );
	if (sto.get_capacity() <= (tlen + len)) len = sto.get_capacity() - 1 - tlen;
	if (len > 0) ZSTRINGS_SYSCALL(text_ucs2_to_ansi)(sto.str() + tlen, len+1, wsptr(sfrom.s, len));

}
template<class CORE> void xappend( str_t<ZSTRINGS_WIDECHAR,CORE>&sto,  const sptr<ZSTRINGS_ANSICHAR> &sfrom )
{
    ZSTRINGS_SIGNED len = sfrom.l;
    if (len == 0) return;
	ZSTRINGS_SIGNED tlen = sto.get_length();
	sto.set_length( tlen + len, true );
	if (sto.get_capacity() <= (tlen + len)) len = sto.get_capacity() - 1 - tlen;
	if (len > 0) ZSTRINGS_SYSCALL(text_ansi_to_ucs2)(sto.str() + tlen, len+1, sptr<ZSTRINGS_ANSICHAR>(sfrom.s, len));
}
template<class CORE> void xappend( str_t<ZSTRINGS_WIDECHAR,CORE>&sto,  const sptr<ZSTRINGS_WIDECHAR> &sfrom )
{
    sto.append(sfrom);
}
template<class CORE> void xappend(str_t<ZSTRINGS_ANSICHAR, CORE>&sto, const sptr<ZSTRINGS_ANSICHAR> &sfrom)
{
    sto.append(sfrom);
}


typedef str_t<ZSTRINGS_ANSICHAR> str_c;
typedef str_t<ZSTRINGS_WIDECHAR> wstr_c;

template<ZSTRINGS_SIGNED __size> using sstr_t = str_t<ZSTRINGS_ANSICHAR, str_core_static_c<ZSTRINGS_ANSICHAR, __size> >;
template<ZSTRINGS_SIGNED __size> using swstr_t = str_t<ZSTRINGS_WIDECHAR, str_core_static_c<ZSTRINGS_WIDECHAR, __size> >;
template<typename TCHARACTER, ZSTRINGS_SIGNED __size> using sxstr_t = str_t<TCHARACTER, str_core_static_c<TCHARACTER, __size> >;


#ifndef ZSTRINGS_DEFAULT_STATIC_SIZE
#define ZSTRINGS_DEFAULT_STATIC_SIZE 1024
#endif

typedef sstr_t< -ZSTRINGS_DEFAULT_STATIC_SIZE > sstr_c;
typedef swstr_t< -ZSTRINGS_DEFAULT_STATIC_SIZE > swstr_c;

typedef str_t< ZSTRINGS_ANSICHAR, str_core_part_c<ZSTRINGS_ANSICHAR> > pstr_c;
typedef str_t< ZSTRINGS_WIDECHAR, str_core_part_c<ZSTRINGS_WIDECHAR> > pwstr_c;
template<typename TCHARACTER> using pstr_t = str_t<TCHARACTER, str_core_part_c<TCHARACTER> >;

template<typename TCHARACTER> using strp_c = str_t < TCHARACTER, str_core_part_c<TCHARACTER> > ;


ZSTRINGS_FORCEINLINE const str_c &to_str(const str_c &s)
{
    return s;
}

ZSTRINGS_FORCEINLINE str_c to_str(const asptr &s)
{
    return str_c(s);
}

ZSTRINGS_FORCEINLINE const ZSTRINGS_ANSICHAR *to_str(const ZSTRINGS_ANSICHAR *s)
{
    ZSTRINGS_ASSERT(false, "Please, dont use [const char *to_str(const char *)]");
    return s;
}

ZSTRINGS_FORCEINLINE str_c to_str(const wsptr &s)
{
    str_c   sout(s.l,false);
    ZSTRINGS_SYSCALL(text_ucs2_to_ansi)(sout.str(), s.l+1, s);
    return sout;
}

ZSTRINGS_FORCEINLINE str_c to_utf8(const wsptr &s)
{
    str_c   sout(s.l*3,false); // hint: char at utf8 can be 6 bytes length, but ucs2 maximum code is 0xffff encoding to utf8 has 3 bytes len
    ZSTRINGS_SIGNED nl = ZSTRINGS_SYSCALL(text_ucs2_to_utf8)(sout.str(), s.l*3+1, s);
    sout.set_length(nl);
    return sout;
}

ZSTRINGS_FORCEINLINE wstr_c from_utf8(const asptr &s)
{
    wstr_c   sout(s.l, false);
    sout.set_as_utf8(s);
    return sout;
}

ZSTRINGS_FORCEINLINE wstr_c to_wstr(const wsptr &s)
{
    return wstr_c(s);
}

ZSTRINGS_FORCEINLINE const wstr_c &to_wstr(const wstr_c &s)
{
    return s;
}

ZSTRINGS_FORCEINLINE const ZSTRINGS_WIDECHAR *to_wstr(const ZSTRINGS_WIDECHAR *s)
{
    ZSTRINGS_ASSERT(false, "Please, dont use [const wchar *to_str(const wchar *)]");
    return s;
}

ZSTRINGS_FORCEINLINE wstr_c to_wstr(const asptr &s)
{
    wstr_c   sout(s.l,false);
    ZSTRINGS_SYSCALL(text_ansi_to_ucs2)(sout.str(), s.l+1, s);
    return sout;
}

ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED skip_utf8_char(const asptr &utf8, ZSTRINGS_SIGNED i = 0)
{
    ZSTRINGS_BYTE x = (ZSTRINGS_BYTE)utf8.s[i];
    if (x == 0) return i;
    if (x <= 0x7f) return i + 1;
    if ((x & (~0x1f)) == 0xc0) return i + 2;
    if ((x & (~0xf)) == 0xe0) return i + 3;
    if ((x & (~0x7)) == 0xf0) return i + 4;
    if ((x & (~0x3)) == 0xf8) return i + 5;
    return i + 6;
}



inline bool CHARz_equal_ignore_case(const ZSTRINGS_WIDECHAR *src1, const ZSTRINGS_WIDECHAR *src2, ZSTRINGS_SIGNED len)
{
    return ZSTRINGS_SYSCALL(text_iequalsw)(src1,src2,len);
}
inline bool CHARz_equal_ignore_case(const ZSTRINGS_ANSICHAR *src1, const ZSTRINGS_ANSICHAR *src2, ZSTRINGS_SIGNED len)
{
    return ZSTRINGS_SYSCALL(text_iequalsa)(src1,src2,len);
}


template<typename NUMTYPE> wstr_c wmake( NUMTYPE u ) {return wstr_c().set_as_num<NUMTYPE>(u);}
template<typename NUMTYPE> wstr_c wmake(const wsptr&prevt, NUMTYPE u) { return wstr_c(prevt).append_as_num<NUMTYPE>(u); }
template<typename NUMTYPE> str_c amake( NUMTYPE u ) {return str_c().set_as_num<NUMTYPE>(u);}
template<typename NUMTYPE> str_c amake( const asptr&prevt, NUMTYPE u ) {return str_c(prevt).append_as_num<NUMTYPE>(u);}

template<typename STRT, typename NUMT> STRT roundstr( NUMT x, ZSTRINGS_SIGNED zp, bool format = true )
{
	STRT t;
	t.set_as_num<NUMT>(x);
	ZSTRINGS_SIGNED index = t.find_pos('.');
	if (index < 0)
	{
		if (format)
		{
			t.append_char('.');
			t.adapt_right( zp, '0' );
		}
	} else
	{
		ZSTRINGS_SIGNED ost = t.get_length() - index - 1;
		if (ost > zp) t.set_length( index + zp + 1 );
		if (zp == 0)
		{
			t.trunc_length();
		}
	}

	return t;

}

template<typename TCHARACTER> class token // tokenizer, for (token t(str); t; t++) if (*t=="...") ...
{
	sptr<TCHARACTER> str;
	str_t<TCHARACTER, str_core_part_c<TCHARACTER> > tkn;
	TCHARACTER separator;
    bool eos;

public:

    typedef decltype(tkn) tokentype;

	template<typename CORE> token(const str_t<TCHARACTER,CORE> &str, TCHARACTER separator = TCHARACTER(',')) : str(str.as_sptr()), separator(separator), eos(false) {++(*this);}
    token(const sptr<TCHARACTER> &str, TCHARACTER separator = TCHARACTER(',')) : str(str), separator(separator), eos(false) {++(*this);}
    token() : eos(true) {}

    TCHARACTER sep() const {return separator;}

	operator bool() const {return !eos;}

    const sptr<TCHARACTER>& tail() const {return str;}

	const str_t<TCHARACTER, str_core_part_c<TCHARACTER> > &operator* () const {return  tkn;}
	const str_t<TCHARACTER, str_core_part_c<TCHARACTER> > *operator->() const {return &tkn;}

    token & begin() {return *this;}
    token end() {return token();}
    bool operator!=(const token &t) { return eos != t.eos; }

	void operator++()
	{
        if (!str)
        {
            eos = true;
            tkn.set(sptr<TCHARACTER>(str.s, 0));
            return;
        }
		const TCHARACTER *begin = str.s;
		for (; str; ++str)
			if (*str.s == separator)
			{
				tkn.set(sptr<TCHARACTER>(begin, str.s - begin));
				++str;
				return;
			}
		tkn.set(sptr<TCHARACTER>(begin, str.s - begin));
	}
	void operator++(int) {++(*this);}
};

#ifndef Z_STR_JOINMACRO1
#define Z_STR_JOINMACRO2(x,y) x##y
#define Z_STR_JOINMACRO1(x,y) JOINMACRO2(x,y)
#endif

template<typename T> ZSTRINGS_FORCEINLINE sptr<T> _to_char_or_not_to_char(const ZSTRINGS_ANSICHAR * sa, const ZSTRINGS_WIDECHAR * sw, int len);
template<> ZSTRINGS_FORCEINLINE asptr _to_char_or_not_to_char<char>(const ZSTRINGS_ANSICHAR * sa, const ZSTRINGS_WIDECHAR *, ZSTRINGS_SIGNED len) { return asptr(sa, len); }
template<> ZSTRINGS_FORCEINLINE wsptr _to_char_or_not_to_char<wchar_t>(const ZSTRINGS_ANSICHAR *, const ZSTRINGS_WIDECHAR * sw, ZSTRINGS_SIGNED len) { return wsptr(sw, len); }
#define CONSTSTR( tc, s ) AUTOSPTR_MACRO<tc>( s, Z_STR_JOINMACRO1(L,s), sizeof(s)-1 )
#define CONSTASTR( s ) ASPTR_MACRO( s, sizeof(s)-1 )
#define CONSTWSTR( s ) WSPTR_MACRO( Z_STR_JOINMACRO1(L,s), sizeof(s)-1 )

#define NAMESPACE_MACRO( a, b, c ) a##b##c
#define ASPTR_MACRO NAMESPACE_MACRO(ZSTRINGS_NAMESPACE,::,asptr)
#define WSPTR_MACRO NAMESPACE_MACRO(ZSTRINGS_NAMESPACE,::,wsptr)
#define AUTOSPTR_MACRO NAMESPACE_MACRO(ZSTRINGS_NAMESPACE,::,_to_char_or_not_to_char)
