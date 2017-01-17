#pragma once

//-V:da_seed:112
//-V:seed:112

namespace ts
{

INLINE uint hash_func( uint data )
{
    uint key = data;

    key += ~(key << 16);
    key ^=  (key >>  5);
    key +=  (key <<  3);
    key ^=  (key >> 13);
    key += ~(key <<  9);
    key ^=  (key >> 17);

    return key;
}

/////////////// rand

template <class RNDGEN> double cRND(RNDGEN & rndgen, double from, double to)
{
    return ((double)rndgen.get_next()*(1.0/(RNDGEN::MAXRND))*((double((to)-(from))))+(from));
}

template <class RNDGEN>  float cRND(RNDGEN & rndgen, float from, float to)
{
    return float((double)rndgen.get_next()*(1.0/(RNDGEN::MAXRND))*((double((to)-(from))))+(from));
}

template <class RNDGEN>  int cRND(RNDGEN & rndgen, int from, int to) // [..)
{
    return (int)rndgen.get_next( tabs(to-from) ) + tmin(from,to);
}

template <class RNDGEN>  float cFRND(RNDGEN & rndgen, float x) { return cRND(rndgen, 0.0f,x); }
template <class RNDGEN>  float cFSRND(RNDGEN & rndgen, float x) { return cFRND(rndgen,2.0f*x)-x; }

extern uint da_seed;

// GCC/EMX
INLINE uint emx_rand() {
    da_seed = da_seed*69069 + 5;
    return (da_seed>>0x10)&0x7FFF;
}

// Watcom C/C++
INLINE uint wc_rand() {
    da_seed = da_seed * 0x41C64E6Du + 0x3039;
    return (da_seed>>0x10)&0x7FFF;
}

// Borland C++ for OS/2
INLINE uint bc2_rand() {
    da_seed = da_seed * 0x15A4E35u + 1;
    return (da_seed>>0x10)&0x7FFF;
}
INLINE uint bc2_lrand() {
    da_seed = da_seed * 0x15A4E35u + 1;
    return da_seed&0x7FFFFFFF;
}

// Microsoft Visual C++
INLINE uint ms_rand() {
    da_seed = 0x343FD * da_seed + 0x269EC3;
    return (da_seed>>0x10)&0x7FFF;
}

class random_modnar_c
{
	uint32 m_seed;
public:
    random_modnar_c( uint32 seed ):m_seed(seed) {}
    random_modnar_c();

	static const uint32 MAXRND = 0xFFFFFFFF; //-V112

    void init( uint32 seed )
    {
        m_seed = seed;
    }

    void mutate_seed( uint32 in_seed )
    {
        mutate( false, get_current() + hash_func( in_seed ) );
    }

    void mutate( bool use_timer = true, uint32 in_seed = 0 );

	uint32 get_current() const
	{
		return hash_func(m_seed);
	}
	uint32 get_current(uint64 n) const
	{
		return uint32(((uint64)get_current() * n) >> 32u); //-V112
	}
	uint32 get_next(uint64 n = MAXRND+1ull)
	{
		m_seed = 1664525L * m_seed + 1013904223L;
		return get_current(n);
	}

    uint32 operator()( uint64 n = MAXRND+1ull ) {return get_next(n);}


};


#ifdef DETERMINATE
#define my_rnd() (RAND_MAX/2)
#else
#define my_rnd ts::ms_rand
#endif

#ifdef RAND_MAX

namespace rnd_internals
{
    template<typename NTYPE> struct rndgen_s
    {
        static NTYPE rnd(NTYPE from, NTYPE to)
        {
            return ((NTYPE)my_rnd())*((to)-(from)) / (RAND_MAX)+(from);
        }
    };
    template<> struct rndgen_s<float>
    {
        static float rnd(float from, float to)
        {
            return float((double)my_rnd()*(1.0 / (RAND_MAX))*((double((to)-(from)))) + (from));
        }
    };
    template<> struct rndgen_s<double>
    {
        static double rnd(double from, double to)
        {
            return ((double)my_rnd()*(1.0 / (RAND_MAX))*((double((to)-(from)))) + (from));
        }
    };
}

template<typename NTYPE> NTYPE rnd(NTYPE from, NTYPE to)
{
    return rnd_internals::rndgen_s<NTYPE>::rnd( from, to );
}
#endif

#define FRND(x)    ((float)(ts::rnd<float>(0.0f,float(x))))
#define FSRND(x)   (FRND(2.0f*(x))-float(x))
#define IRND(n)    (ts::lround(ts::rnd<double>(0.0,double(n)-0.55)))

}
