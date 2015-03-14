/*
    (C) 2010-2015 ROTKAERMOTA
*/
#pragma once

namespace zstrings_internal
{
	template<typename T> struct is_signed
	{
		static const bool value = (((T)-1) < 0);
	};

	template<typename T, bool sgn = is_signed<T>::value > struct invert
	{
		void operator()(T &) {__assume(0);}
	};
	template<typename T> struct invert<T, true>
	{
		void operator()(T &t) { t=-t;}
	};

	template<typename NUM, int shiftval> struct makemaxint
	{
		static const NUM value = (makemaxint<NUM, shiftval - 1>::value << 8) | 0xFF;
	};
	template<typename NUM> struct makemaxint<NUM, 0>
	{
		static const NUM value = 0x7F;
	};

	template<typename NUM> struct maximum
	{
		static const NUM value = is_signed<NUM>::value ? makemaxint<NUM, sizeof(NUM) - 1>::value : (NUM)(-1);
	};

	template<typename CHARTYPETO, typename CHARTYPEFROM> CHARTYPETO cvtchar( CHARTYPEFROM c )
	{
		return (CHARTYPETO)c;
	}

	template<typename TCHARACTER, typename NUMTYPE> bool CHARz_to_num(NUMTYPE &out, const TCHARACTER *s, ZSTRINGS_SIGNED slen)
	{
		return CHARz_to_int( out, s, slen );
	}
	template<typename TCHARACTER> bool CHARz_to_num(float &out, const TCHARACTER *s, ZSTRINGS_SIGNED slen)
	{
		double d;
		bool r = CHARz_to_double( d, s, slen );
		out = (float)d;
		return r;
	}
	template<typename TCHARACTER> bool CHARz_to_num(double &out, const TCHARACTER *s, ZSTRINGS_SIGNED slen)
	{
		return CHARz_to_double( out, s, slen );
	}
}

ZSTRINGS_FORCEINLINE ZSTRINGS_ANSICHAR base64(ZSTRINGS_SIGNED index)
{
    ZSTRINGS_ASSERT(index >= 0 && index < 64);
    static ZSTRINGS_ANSICHAR base64_encoding_table[64] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
    };
    return base64_encoding_table[index];
}



ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED wchar_as_hex(ZSTRINGS_WIDECHAR c)
{
    if (c>='0' && c<='9') return (c - '0');
    if (c>='A' && c<='F') return (c - 'A' + 10);
    if (c>='a' && c<='f') return (c - 'a' + 10);
    return 0;
}


template <class TCHARACTER> ZSTRINGS_FORCEINLINE bool    CHAR_is_letter(const TCHARACTER c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    return false;

}

template <class TCHARACTER> ZSTRINGS_FORCEINLINE bool    CHAR_is_digit(const TCHARACTER c)
{
    if (c >= '0' && c <= '9') return true;
    return false;
}

template <class THE_CHAR> ZSTRINGS_FORCEINLINE bool    CHAR_is_hollow(const THE_CHAR c)
{
	return (c == ' ' || c == 0x9 || c == 0x0d || c == 0x0a);
}


/////////////////////////
//block functions
/////////////////////////

__inline bool    blk_cmp(const void *d1, const void *d2, ZSTRINGS_UNSIGNED size)
{
    union
    {
        ZSTRINGS_UNSIGNED *db1;
        ZSTRINGS_BYTE *ds1;
    };
    union
    {
        ZSTRINGS_UNSIGNED *db2;
        ZSTRINGS_BYTE *ds2;
    };

    db1 = (ZSTRINGS_UNSIGNED *)d1;
    db2 = (ZSTRINGS_UNSIGNED *)d2;

    while (size >= sizeof(ZSTRINGS_UNSIGNED))
    {
        if (*db1 != *db2) return false;
        ++db1;
        ++db2;
        size -= sizeof(ZSTRINGS_UNSIGNED);
    }

    while (size > 0)
    {
        if (*ds1 != *ds2) return false;
        ++ds1;
        ++ds2;
        --size;
    }
    return true;
}

__inline void    blk_copy_fwd(void *tgt, const void *src, ZSTRINGS_UNSIGNED size)
{
    ZSTRINGS_BYTE    *btgt = (ZSTRINGS_BYTE *)tgt;
    const ZSTRINGS_BYTE    *bsrc = (const ZSTRINGS_BYTE *)src;
    while(size)
    {
        *btgt++ =  *bsrc++;
        size--;
    }
}

__inline void    blk_copy_back(void *tgt, const void *src, ZSTRINGS_UNSIGNED size)
{
    ZSTRINGS_BYTE    *btgt = ((ZSTRINGS_BYTE *)tgt) + size - 1;
    const ZSTRINGS_BYTE    *bsrc = ((const ZSTRINGS_BYTE *)src) + size - 1;
    while(size)
    {
        *btgt-- =  *bsrc--;
        size--;
    }
}


template <class UNIT>  ZSTRINGS_FORCEINLINE void    blk_UNIT_copy_fwd(UNIT *tgt, const UNIT *src, ZSTRINGS_UNSIGNED size) //same as blk_copy, but copying by sizeof(uint) ZSTRINGS_BYTEs
{
    UNIT    *uitgt = tgt;
    const UNIT    *uisrc = src;
    while(size)
    {
        *uitgt++ =  *uisrc++;
        size--;
    }
}

template <class UNIT>  ZSTRINGS_FORCEINLINE void    blk_UNIT_copy_back(UNIT *tgt, const UNIT *src, ZSTRINGS_UNSIGNED size) //same as blk_copy, but copying by sizeof(UNIT) ZSTRINGS_BYTEs
{
    UNIT    *uitgt = tgt + size - 1;
    const UNIT    *uisrc = src + size - 1;
    while(size)
    {
        *uitgt-- =  *uisrc--;
        size--;
    }
}

ZSTRINGS_FORCEINLINE void    blk_sizet_copy(void *tgt, const void *src, ZSTRINGS_UNSIGNED size) //same as blk_copy, but copying by sizeof(uint) ZSTRINGS_BYTEs
{
    ZSTRINGS_UNSIGNED    *uitgt = (ZSTRINGS_UNSIGNED *)tgt;
    const ZSTRINGS_UNSIGNED    *uisrc = (const ZSTRINGS_UNSIGNED *)src;

    if ((uisrc + size) <= uitgt) blk_UNIT_copy_fwd<ZSTRINGS_UNSIGNED>(uitgt,uisrc,size);
    else blk_UNIT_copy_back<ZSTRINGS_UNSIGNED>(uitgt,uisrc,size);
}

template <class U> ZSTRINGS_FORCEINLINE void    blk_fill(void *tgt, ZSTRINGS_UNSIGNED size, const U filler = U(0))
{
    U *btgt = (U *)tgt;
    while(size)
    {
        *btgt++ =  filler;
        size--;
    }
}

ZSTRINGS_FORCEINLINE bool blk_equal(const void *tgt, const void *src, ZSTRINGS_UNSIGNED size)
{
    //not optimized!!!

    const ZSTRINGS_BYTE    *btgt = (const ZSTRINGS_BYTE *)tgt;
    const ZSTRINGS_BYTE    *bsrc = (const ZSTRINGS_BYTE *)src;
    while(size)
    {
        if (*btgt++ !=  *bsrc++) return false;
        size--;
    }
    return true;
}

struct str_iwb_s
{
    void * buff;
};

__inline void blk_getstrinit(void *buff,str_iwb_s *sb)
{
    sb->buff = buff;
}

//block get next string
bool blk_getnextstr(ZSTRINGS_ANSICHAR *kuda,ZSTRINGS_UNSIGNED lim,str_iwb_s *sb);

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED CHARz_find(const TCHARACTER * const s, const TCHARACTER c)
{
    ZSTRINGS_SIGNED l = -1;
    TCHARACTER temp;
    do {l++; temp = *(s+l); } while (temp && (temp^c));
    return temp?l:-1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED CHARz_find_multi(const TCHARACTER * const s, const TCHARACTER *c)
{
    ZSTRINGS_SIGNED index = 0;
    TCHARACTER temp;
    while (temp = s[index])
    {
        if (CHARz_find(c,temp)>=0) return index;
        ++index;
    }
    return -1;
};
/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED CHARz_findn(const TCHARACTER * const s, const TCHARACTER c, ZSTRINGS_SIGNED slen)
{
    ZSTRINGS_SIGNED l = -1;
    TCHARACTER temp;
    do {++l; temp = *(s+l); } while ((temp^c) && l < slen);
    return (temp==c && l < slen)?l:-1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED CHARz_findr(const TCHARACTER * s, const TCHARACTER c)
{
    ZSTRINGS_SIGNED l = -1;
    ZSTRINGS_SIGNED i = -1;
    TCHARACTER temp;
    do
    {
        ++i;
        temp = *(s);
        if (temp==c) l=i;
    } while (temp);
    return l;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE ZSTRINGS_SIGNED CHARz_len(const TCHARACTER *s)
{
    ZSTRINGS_SIGNED l = -1;
    do {l++;} while (*(s+l));
    return l;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> __inline void    CHARz_copy(TCHARACTER *tgt, const TCHARACTER *src)
{
    do {*tgt++ = *src;} while (*src++);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> __inline void    CHARz_add_str(TCHARACTER *tgt, const TCHARACTER *src)
{
    TCHARACTER *rtgt = tgt + CHARz_len<TCHARACTER>(tgt);
    do {*rtgt++ = *src;} while (*src++);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE bool CHARz_equal(const TCHARACTER *src1, const TCHARACTER *src2)
{
    //not optimized!!!

    for(;;)
    {
        if ((*src1) !=  (*src2)) return false;
        if (*src1 ==  0) return true;
        src1++;
        src2++;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE bool CHARz_equal(const TCHARACTER *src1, const TCHARACTER *src2, ZSTRINGS_SIGNED len)
{
    //not optimized!!!

    for (;len > 0; --len)
    {
        if ((*src1) != (*src2)) return false;
        src1++;
        src2++;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
template <class TCHARACTER> ZSTRINGS_FORCEINLINE bool CHARz_begin(const TCHARACTER *src1, const TCHARACTER *src2)
{
    //not optimized!!!

    for(;;)
    {
        if (*src2 ==  0) return true;
        if ((*src1) !=  (*src2)) return false;
        src1++;
        src2++;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//convert string to integer
template <typename TCHARACTER> __inline bool CHARz_to_double(double &out1,const TCHARACTER * s, ZSTRINGS_SIGNED slen = -1)
{
	double zn = 0.0;
	if (slen < 0) slen = zstrings_internal::maximum<ZSTRINGS_SIGNED>::value;

	ZSTRINGS_SIGNED i = 0;
	bool negative = false;
	if (*s == '-') { negative = true; ++i; }

	TCHARACTER ch;
	for(;i<slen;++i)
	{
		ch = s[i];
		unsigned m = ch - TCHARACTER('0');
		if (m < 10)
			zn = zn*10.0 + (double)m;
		else if(ch==TCHARACTER('.'))
			break;
		else
		{
			out1 = negative ? -zn : zn;
			return ch == 0;
		}
	}

	if (i >= slen)
	{
		out1 = negative ? -zn : zn;
		return true;
	}

	i++;
	double tra = 10.0;
	for(;i<slen;++i)
	{
		ch = s[i];
		unsigned m = ch - TCHARACTER('0');
		if(m < 10)
		{
			zn = zn + ((double)m)/tra;
			tra *= 10.0;
		} else
		{
			out1 = negative ? -zn : zn;
			return ch == 0;
		}
	}

	out1 = negative ? -zn : zn;
	return true;
}

template <class TCHARACTER, typename I> __inline TCHARACTER * CHARz_make_str_unsigned(TCHARACTER *buf, ZSTRINGS_SIGNED &szbyte, I i)
{
	ZSTRINGS_SIGNED idx = (sizeof(I) * 3-1);
	buf[idx--] = 0;
	while (i >= 10)
	{
		I d = i / 10;
		buf[idx--] = (TCHARACTER)(i - ((d<<3)+(d<<1)) + 48);
		i = d;
	}
	buf[idx] = (TCHARACTER)((TCHARACTER)i + 48);
	szbyte = ((sizeof(I) * 3) - idx) * sizeof(TCHARACTER);
	return buf + idx;
}


#pragma warning ( push )
#pragma warning (disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#pragma warning (disable: 4127) // conditional expression is constant
/////////////////////////////////////////////////////////////////////////////////////////////////
//convert string to integer
template <class TCHARACTER, class NUMTYPE> inline bool CHARz_to_int(NUMTYPE &out,const TCHARACTER * const s, ZSTRINGS_SIGNED slen = -1)
{
    bool negative;
    ZSTRINGS_SIGNED i;
    NUMTYPE n2 = 0;
    NUMTYPE n8 = 0;
    NUMTYPE n10 = 0;
    NUMTYPE n16 = 0;
    TCHARACTER    c;


    if (zstrings_internal::is_signed<NUMTYPE>::value)
    {
        if (*s == '-') {negative = true; i = 1; } else {negative = false; i = 0;}
    } else 
    {
        i = 0;
        negative = false;
    }
    if (*(s + i) == '$')
    {
        i++;
        goto parse_hex;

    }
    if ( (*(s + i) == '0') && ((*(s + i + 1) | 32) == 'x') )
    {
        i+=2;
parse_hex:
        if (slen == i || (c = *(s + i)) == 0) return false;
        i++;
        do
        {
            ZSTRINGS_UNSIGNED m = c - 48;
            if (m>10) m = (c | 32) - 87;
            if (m>16) return false;
            n16 = (n16 << 4) + m;
        } while (slen != i && (c = *(s + i++)) != 0);
        out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n16:n16;
        return true;
    }
   
    if (slen == i || (c = *(s + i))==0) return false;
    i++;

    do
    {
        unsigned m = c - 48;
        if (m>1)
        {
            //non radix 2 loop

            if (slen == i || *(s + i) == 0)
            {
                TCHARACTER cc = (TCHARACTER)(c|32);
                if (cc == 'b')
                {
                    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n2:n2;
                    return true;
                } else
                if (cc == 'h')
                {
                    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n16:n16;
                    return true;
                } else
                if (cc == 'o')
                {
                    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n8:n8;
                    return true;
                }
            }
            do
            {
                unsigned m = c - 48;
                if (m>7)
                {
                    // non radix 8 loop
                    if (slen == i || *(s + i) == 0)
                    {
                        TCHARACTER cc = (TCHARACTER)(c|32);
                        if (cc == 'h')
                        {
                            out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n16:n16;
                            return true;
                        } else
                        if (cc == 'o')
                        {
                            out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n8:n8;
                            return true;
                        }
                    }
                    do
                    {
                        unsigned m = c - 48;
                        if (m>10)
                        {
                            // non radix 10 loop
                            if (slen == i || *(s + i) == 0)
                            {
                                TCHARACTER cc = (TCHARACTER)(c|32);
                                if (cc == 'h')
                                {
                                    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n16:n16;
                                    return true;
                                }
                            }
                            do
                            {
                                unsigned m = c - 48;
                                if (m>10) m = (c | 32) - 87;
                                if (m>16)
                                {
                                    if (slen == i || *(s + i) == 0)
                                    {
                                        TCHARACTER cc = (TCHARACTER)(c|32);
                                        if (cc == 'h')
                                        {
                                            out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n16:n16;
                                            return true;
                                        }
                                    }
                                    return false;
                                }
                                n16 = (n16 << 4) + m;
                            } while(slen != i && (c = *(s + i++))!=0);
                            return false;
                        }
                        n10 = (n10<<3) + (n10<<1) + m;
                        n16 = (n16 << 4) + m;

                    } while(slen != i && (c = *(s + i++))!=0);
                    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n10:n10;
                    return true;
                }
                n8 = (n8 << 3) + m;
                n10 = (n10<<3) + (n10<<1) + m;
                n16 = (n16 << 4) + m;
            } while(slen != i && (c = *(s + i++))!=0);
            out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n10:n10;
            return true;
        }
        n2 = (n2 << 1) + m;
        n8 = (n8 << 3) + m;
        n10 = (n10<<3) + (n10<<1) + m;
        n16 = (n16 << 4) + m;
    } while(slen != i && (c = *(s + i++))!=0);

    out = (negative&&(zstrings_internal::is_signed<NUMTYPE>::value))?-n10:n10;
    return true;
}
template <typename TCHARACTER, typename NUMTYPE> inline NUMTYPE CHARz_to_int_10(const TCHARACTER * const s, NUMTYPE def, ZSTRINGS_SIGNED slen = -1)
{
	NUMTYPE n10 = 0;
	TCHARACTER    c;
	bool negative = false;

	if (slen < 0) slen = zstrings_internal::maximum<ZSTRINGS_SIGNED>::value;

	ZSTRINGS_SIGNED i = 0;

	if ((c = *s)==0 || slen == 0) return def;
	if (zstrings_internal::is_signed<NUMTYPE>::value && c=='-')
	{
		negative = true;
		c = *(s + (++i));
		if (slen == 1) return def;
	}
	i++;

	for(;;)
	{
		unsigned m = c - 48;
		if (m>=10 || i > slen)
		{
			return (i == 1 || (negative && i==2)) ? def : ((zstrings_internal::is_signed<NUMTYPE>::value&&negative) ? -n10 : n10);
		}
		n10 = (n10<<3) + (n10<<1) + m;
		c = *(s + (i++));
	}
	//__assume(0);
}

#pragma warning ( pop )

inline unsigned char_as_hex(ZSTRINGS_SIGNED c)
{
	if (unsigned(c - '0') <= 9) return c - '0';
	if (unsigned(c - 'A') < 6) return c + (10 - 'A');
	if (ZSTRINGS_ASSERT(unsigned(c - 'a') < 6)) return c + (10 - 'a');
	return 0;
}
template <typename TCHARACTER> inline void hex_to_data(void *idata, const sptr<TCHARACTER> &hex)
{
	ZSTRINGS_ASSERT(hex.l % 2 == 0);
    uint8 *data = (uint8 *)idata;
	for (const TCHARACTER *h = hex.s, *end = hex.s + hex.l; h < end; h += 2)
		*(data++) = (uint8)((char_as_hex(h[0]) << 4) | char_as_hex(h[1]));
}

