#pragma once

#define MAKEMASK( numbits, shiftleft ) (((1U << (numbits))-1)<<(shiftleft))

#define SETBIT(x) ((1U)<<(x))
#define SETFLAG(f,mask) (f)|=(mask)
#define RESETFLAG(f,mask) (f)&=~(mask)
#define INVERTFLAG(f,mask) (f)^=(mask)
#define FLAG(f,mask) (((f)&(mask))!=0)
#define INITFLAG(f,mask,val) if(val) {SETFLAG(f,mask);} else {RESETFLAG(f,mask);}

#define DECLARE_FLAG(flags_var, fn, fvalue) void set_##fn## (void)  { /* lint -e19 */ flags_var.set(fvalue); }; \
    bool is_##fn## (void) const    { return flags_var.is(fvalue);    }; \
    void clear_##fn## (void)       { flags_var.clear(fvalue);        }; \
    void invert_##fn## (void)      { flags_var.invert(fvalue);       }; \
    void init_##fn## (bool bValue) { flags_var.init(fvalue, bValue); /* lint -e19 */ }

namespace ts
{

template <typename T, T FMASK = (T)(-1)> struct flags_t
{
	typedef T BITS;
    typedef flags_t<T, (T)(-1)> UNMASKED;
    T   __bits;

    flags_t()
    {
        __bits = 0;
    }
    flags_t(T from)
    {
        __bits = (from & FMASK);
    }

    void setup(T val) { __bits = val & FMASK; }
    void set(T mask) { SETFLAG(__bits, mask & FMASK); };
    void clear(T mask) { RESETFLAG(__bits, mask & FMASK); };
    bool clearr(T mask) { bool r = is(mask); if (r) RESETFLAG(__bits, mask & FMASK); return r; };
    void clear() { __bits &= ~FMASK; };
    void invert(T mask) { INVERTFLAG(__bits, mask & FMASK); };
    bool init(T mask, bool value) { INITFLAG(__bits, mask & FMASK, value); return value; };
    bool is(T mask) const { return FLAG(__bits, mask & FMASK); };
    flags_t & operator = (const flags_t &from)
    {
        __bits = (__bits & ~FMASK) | (from.__bits & FMASK);
        return *this;
    };

    flags_t & operator = (const T &dwfrom)
    {
        __bits = (__bits & ~FMASK) | (dwfrom & FMASK);
        return *this;
    };

    operator T() const {return (__bits & FMASK);};

    UNMASKED& unmasked() { return *(UNMASKED *)&__bits; }
    const UNMASKED& unmasked() const { return *(UNMASKED *)&__bits; }
};

typedef flags_t<uint32> flags32_s;
typedef flags_t<uint64> flags64_s;

template<class FLAGS, decltype(FLAGS::BITS) v, bool v1, bool v2> struct auto_change_flags
{
    FLAGS &f;
    auto_change_flags(FLAGS &f):f(f) { f.init(v,v1); }
    ~auto_change_flags() { f.init(v,v2); }
private:
    auto_change_flags(const auto_change_flags&) UNUSED;
    auto_change_flags operator=(const auto_change_flags &) UNUSED;
};

#define AUTOCLEAR(f,v) ts::auto_change_flags< decltype(f), v, true, false > __autoclear(f);
#define AUTOSET(f,v) ts::auto_change_flags< decltype(f), v, false, true > __autoset(f);


} // namespace ts
