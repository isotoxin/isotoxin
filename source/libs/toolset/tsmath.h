#pragma once

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef MODE64
#include <emmintrin.h>
#else
#include <xmmintrin.h>
#endif

namespace ts
{
    template < typename T1, typename T2 > struct minmax2
    {
        static typename biggest<T1, T2>::type calcmax( const T1 & v1, const T2 & v2 )
        {
            return (v1 > v2) ? static_cast< typename biggest<T1, T2>::type >(v1) : static_cast< typename biggest<T1, T2>::type >(v2);
        }
        static typename biggest<T1, T2>::type calcmin( const T1 & v1, const T2 & v2 )
        {
            return (v1 < v2) ? static_cast<typename biggest<T1, T2>::type>(v1) : static_cast<typename biggest<T1, T2>::type>(v2);
        }
    };

    template<> struct minmax2<float, float>
    {
        static float calcmax( float x, float y )
        {
            float r;
            _mm_store_ss( &r, _mm_max_ss( _mm_load_ss( &x ), _mm_load_ss( &y ) ) );
            return r;
        }
        static float calcmin( float x, float y )
        {
            float r;
            _mm_store_ss( &r, _mm_min_ss( _mm_load_ss( &x ), _mm_load_ss( &y ) ) );
            return r;
        }
    };

    /// Max, 2 arguments
    template < typename T1, typename T2 > INLINE typename biggest<T1, T2>::type tmax( const T1 & v1, const T2 & v2 )
    {
        return minmax2<T1, T2>::calcmax( v1, v2 );
    }

    /// Min, 2 arguments
    template < typename T1, typename T2 > INLINE typename biggest<T1, T2>::type tmin( const T1 & v1, const T2 & v2 )
    {
        return minmax2<T1, T2>::calcmin( v1, v2 );
    }


	INLINE const float fastsqrt(const float x)
	{
		//	return x * inversesqrt(max(x, limits<float>::min()));//3x times faster than _mm_sqrt_ss with accuracy of 22 bits (but compiler optimize this not so well, and sqrtss works faster)
		float r;
		_mm_store_ss(&r, _mm_sqrt_ss(_mm_load_ss(&x)));
		return r;
	}

    template < typename T1, typename T2, typename T3 >
    INLINE T1 CLAMP(const T1 & a, const T2 & vmin, const T3 & vmax)
    {
        return (T1)(((a) > (vmax)) ? (vmax) : (((a) < (vmin)) ? (vmin) : (a)));
    }

	struct f_rect_s
	{
		float x,y,width,height;
	};

#undef M_PI
#define M_PI        3.14159265358979323846
#define M_PI_MUL(x) float((x)*M_PI)

#define TO_RAD_K (M_PI/180.0)
#define TO_GRAD_K (180.0/M_PI)

#define GRAD2RAD(a) (float((a) * TO_RAD_K))
#define RAD2GRAD(a) (float((a) * TO_GRAD_K))

#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

	INLINE double    Pi()                    { return M_PI;          }
	INLINE double    TwoPi()                 { return M_PI * 2.0f;   }

    INLINE long int lround( float x )
    {
        return _mm_cvtss_si32( _mm_load_ss( &x ) ); //assume that current rounding mode is always correct (i.e. round to nearest)
    }

    INLINE long int lround( double x )
    {
#ifdef MODE64
        return _mm_cvtsd_si32( _mm_load_sd( &x ) );
#else
        #if defined _MSC_VER
        _asm
        {
            fld x
            push eax
            fistp dword ptr[ esp ]
            pop eax
        }
        #elif defined __GNUC__
        return ::lround( x );
        #endif
#endif
    }


    INLINE void sincos(const float angle, float & vsin, float & vcos)
    {
        float cis, sis;

#if defined (_MSC_VER) && !defined(MODE64)
        _asm
        {
            fld angle
            fsincos
            fstp cis
            fstp sis
        }
#elif defined (_MSC_VER) && defined(MODE64)
        sis = sinf(angle);
        cis = cosf( angle );
#elif defined __GNUC__
        ::sincosf( angle, &cis, &sis );
#endif
        vsin = sis;
        vcos = cis;
    }

    INLINE void sincos(const float angle, double & vsin, double & vcos)
    {
        double cis, sis;

#if defined (_MSC_VER) && !defined(MODE64)
        _asm
        {
            fld angle
            fsincos
            fstp cis
            fstp sis
        }
#elif defined (_MSC_VER) && defined(MODE64)
        sis = sin( angle );
        cis = cos( angle );
#elif defined __GNUC__
        ::sincos( angle, &cis, &sis );
#endif


        vsin = sis;
        vcos = cis;
    }

    INLINE void sincos_r(const float angle, const float r, float & vsin, float & vcos)
    {
        float cis, sis;

#if defined (_MSC_VER) && !defined(MODE64)
        _asm
        {
            fld angle
            fsincos
            fmul r
            fstp cis
            fmul r
            fstp sis
        }
        vsin = sis;
        vcos = cis;
#elif defined (_MSC_VER) && defined(MODE64)
        UPAR( cis );
        UPAR( sis );
        vsin = sinf( angle ) * r;
        vcos = cosf( angle ) * r;
#elif defined __GNUC__
        ::sincosf( angle, &cis, &sis );
        vsin = sis * r;
        vcos = cis * r;
#endif

    }

#include "tsvec.h"

#if defined _MSC_VER
#pragma warning ( push )
#pragma warning (disable: 4201) // nonstandard extension used : nameless struct/union
#endif
	class mat22
	{
	public:

		union
		{
			struct
			{
				float e11;
				float e12;
				float e21;
				float e22;
			};

			float e[2][2];
		};

		mat22(float _e11, float _e12, float _e21, float _e22) : e11(_e11), e12(_e12), e21(_e21), e22(_e22) {}
		mat22()	{}

		mat22(float angle)
		{
			float c = cos(angle);
			float s = sin(angle);

			e11 = c; e12 = s;
			e21 =-s; e22 = c;
		}

		float  operator()(aint i, aint j) const { return e[i][j]; }
		float& operator()(aint i, aint j)       { return e[i][j]; }


		const vec2& operator[](aint i) const
		{
			return reinterpret_cast<const vec2&>(e[i][0]);
		}

		vec2& operator[](aint i)
		{
			return reinterpret_cast<vec2&>(e[i][0]);
		}

		static mat22 Identity()
		{
			static const mat22 T(1.0f, 0.0f, 0.0f, 1.0f);

			return T;
		}

		static mat22 Zer0()
		{
			static const mat22 T(0.0f, 0.0f, 0.0f, 0.0f);

			return T;
		}


		mat22 Tranpose() const
		{
			mat22 T;

			T.e11 = e11;
			T.e21 = e12;
			T.e12 = e21;
			T.e22 = e22;

			return T;
		}

		mat22 operator * (const mat22& M) const
		{
			mat22 T;

			T.e11 = e11 * M.e11 + e12 * M.e21;
			T.e21 = e21 * M.e11 + e22 * M.e21;
			T.e12 = e11 * M.e12 + e12 * M.e22;
			T.e22 = e21 * M.e12 + e22 * M.e22;

			return T;
		}

		mat22 operator ^ (const mat22& M) const
		{
			mat22 T;

			T.e11 = e11 * M.e11 + e12 * M.e12;
			T.e21 = e21 * M.e11 + e22 * M.e12;
			T.e12 = e11 * M.e21 + e12 * M.e22;
			T.e22 = e21 * M.e21 + e22 * M.e22;

			return T;
		}

		inline mat22 operator * ( float s ) const
		{
			mat22 T;

			T.e11 = e11 * s;
			T.e21 = e21 * s;
			T.e12 = e12 * s;
			T.e22 = e22 * s;

			return T;
		}

	};

#if defined _MSC_VER
#pragma warning ( pop )
#endif

    INLINE vec2 operator * (const vec2 &P, const mat22& M)
    {
        vec2 T;
        T.x = P.x * M.e11 + P.y * M.e12;
        T.y = P.x * M.e21 + P.y * M.e22;
        return T;
    }

    INLINE vec2 operator ^ (const vec2 &P, const mat22& M)
    {
        vec2 T;
        T.x = P.x * M.e11 + P.y * M.e21;
        T.y = P.x * M.e12 + P.y * M.e22;
        return T;
    }

    INLINE bool point_rite(const vec2 &s, const vec2 &e, const vec2 &p)
    {
        return (bool) (((e - s).cross(p - s)) <= 0.0f);
    }

	struct irect //-V690
	{
        ivec2 lt;
		ivec2 rb;

		irect() {}
		explicit irect(decltype(ivec2::x) s) :lt(s), rb(s) {}
		irect(const irect &ir):lt(ir.lt), rb(ir.rb) {}
		template<typename T1, typename T2> irect(const vec_t<T1,2> &lt_, const vec_t<T2,2> &rb_):lt(lt_.x,lt_.y), rb(rb_.x, rb_.y) {}
		irect(decltype(ivec2::x) x1, decltype(ivec2::y) y1, decltype(ivec2::x) x2, decltype(ivec2::y) y2) :lt(x1, y1), rb(x2, y2) {}
        irect(decltype(ivec2::x) v1, decltype(ivec2::x) v2) :lt(v1), rb(v2) {}
        template<typename T> irect(decltype(ivec2::x) lt_, const vec_t<T,2> &rb_) :lt(lt_, lt_), rb(rb_) {}

        static irect from_center_and_size( const ivec2&c, const ivec2&sz )
        {
            irect r;
            r.lt = c - sz/2;
            r.rb = r.lt + sz;
            return r;
        }
        static irect from_lt_and_size(const ivec2&lt, const ivec2&sz)
        {
            irect r;
            r.lt = lt;
            r.rb = r.lt + sz;
            return r;
        }

		auto width() const -> decltype(rb.x - lt.x) {return rb.x - lt.x;}
		auto height() const -> decltype(rb.y - lt.y) {return rb.y - lt.y;};
		irect &setheight(aint h) {rb.y = (int)(lt.y + h); return *this;}
		irect &setwidth(aint w) {rb.x = (int)(lt.x + w); return *this;}

		ivec2 size() const {return ivec2( width(), height() );}

        ivec2 rt() const {return ivec2( rb.x, lt.y );}
        ivec2 lb() const {return ivec2( lt.x, rb.y );}

        ivec2 center() const { return ivec2((lt.x + rb.x) / 2, (lt.y + rb.y) / 2); }
        auto hcenter() const -> decltype(rb.x - lt.x) { return (lt.x + rb.x) / 2; }
        auto vcenter() const -> decltype(rb.x - lt.x) { return (lt.y + rb.y) / 2; }

        ivec2 prc( const ivec2 &c, bool clamp ) const
        {
            ivec2 s = size();
            ivec2 r( s.x ? (10000 * (c.x - lt.x) / s.x) : 0, s.y ? (10000 * (c.y - lt.y) / s.y) : 0 );
            return clamp ? ivec2( CLAMP(r.x, 0, 10000), CLAMP(r.y, 0, 10000) ) : r;
        }

        ivec2 byprc(const ivec2 &pprc) const
        {
            return ivec2( lt.x + width() * pprc.x / 10000, lt.y + height() * pprc.y / 10000 );
        }

        irect &movein(const irect &r)
        {
            if (lt.x < r.lt.x)
            {
                int addx = r.lt.x - lt.x;
                lt.x += addx;
                rb.x += addx;
            }
            if (lt.y < r.lt.y)
            {
                int addy = r.lt.y - lt.y;
                lt.y += addy;
                rb.y += addy;
            }

            if (rb.x > r.rb.x)
            {
                int subx = rb.x - r.rb.x;
                lt.x -= subx;
                rb.x -= subx;
                if (lt.x < r.lt.x) lt.x = r.lt.x;
            }

            if (rb.y > r.rb.y)
            {
                int suby = rb.y - r.rb.y;
                lt.y -= suby;
                rb.y -= suby;
                if (lt.y < r.lt.y) lt.y = r.lt.y;
            }

            return *this;
        }

        int area() const { return width() * height(); }
        operator bool () const {return rb.x > lt.x && rb.y > lt.y; }

		bool inside(const ivec2 &p) const
		{
			return p.x >= lt.x && p.x < rb.x && p.y >= lt.y && p.y < rb.y;
		}

		irect & make_empty()
		{
			lt.x = maximum< decltype(lt.x) >::value;
			lt.y = maximum< decltype(lt.y) >::value;
			rb.x = minimum< decltype(rb.x) >::value;
			rb.y = minimum< decltype(rb.y) >::value;
            return *this;
		}

        irect & normalize()
        {
            if ( lt.x > rb.x ) SWAP( lt.x, rb.x );
            if ( lt.y > rb.y ) SWAP( lt.y, rb.y );
            return *this;
        }

        bool intersected(const irect &i) const;
        int intersect_area(const irect &i) const;
		irect & intersect(const irect &i);
		irect & combine(const irect &i);

        ts::irect szrect() const { return ts::irect(ts::ivec2(0), size()); } // just size

        bool operator == (const irect &r) const {return lt == r.lt && rb == r.rb;}
        bool operator != (const irect &r) const {return lt != r.lt || rb != r.rb;}

        irect & operator +=(const ivec2& p)
        {
            lt += p;
            rb += p;
            return *this;
        }
        irect & operator -=(const ivec2& p)
        {
            lt -= p;
            rb -= p;
            return *this;
        }
	};

    namespace internals { template <> struct movable<irect> : public movable_customized_yes {}; }

    INLINE irect operator+(const irect &r, const ivec2 &p) {return irect( r.lt + p, r.rb + p );}
    INLINE irect operator-(const irect &r, const ivec2 &p) {return irect( r.lt - p, r.rb - p );}

    template<typename STRTYPE> INLINE streamstr<STRTYPE> & operator<<(streamstr<STRTYPE> &dl, const irect &r)
    {
        dl.raw_append("rec[");
        dl << r.lt << "," << r.rb << "=" << r.size();
        return dl << "]";
    }


	struct doubleRect {
		double x0,x1,x2,x3,y0,y1,y2,y3;
	};

	INLINE void getUVfromRect(float & tx/*in - out*/, float & ty/*in - out*/, const doubleRect & rect/*out*/)
	{
		double B11 = (rect.x2 - rect.x3 - rect.x1 + rect.x0);
		double B12 = -(rect.y2 - rect.y3 - rect.y1 + rect.y0);
		double B13 = (rect.x3 - rect.x0) * (rect.y0 - rect.y1) - (rect.x0 - rect.x1) * (rect.y3 - rect.y0);
		double C11 = (rect.x3 - rect.x0);
		double C12 =  -(rect.y3 - rect.y0);
		//double B13 = C11 * (rect.y0 - rect.y1) + C12 * (rect.x0 - rect.x1);

		double B21 = (rect.x3 - rect.x0 - rect.x2 + rect.x1);
		double B22 = - (rect.y3 - rect.y0 - rect.y2 + rect.y1);
		double B23 = (rect.x0 - rect.x1) * (rect.y1 - rect.y2) - (rect.x1 - rect.x2) * (rect.y0 - rect.y1);
		double C21 = (rect.x0 - rect.x1);
		double C22 = - (rect.y0 - rect.y1);

		float xx = tx;
		float yy = ty;

		double dxu = xx - rect.x0;
		double dyu = yy - rect.y0;
		double dxv = xx - rect.x1;
		double dyv = yy - rect.y1;

		double B = B11 * dyu + B12 * dxu + B13;
		double C = C11 * dyu + C12 * dxu;

		tx = float (-C/B);

		B = B21 * dyv + B22 * dxv + B23;
		//B = B21 * dyv - B12 * dxv + B23;
		//B = - B11 * dyv - B12 * dxv + B23;
		C = C21 * dyv + C22 * dxv;

		ty = float (-C/B);
	}

    template<> inline str_c amake(irect r) { return str_c().set_as_int(r.lt.x).append_char(',').append_as_int(r.lt.y).append_char(',').append_as_int(r.rb.x).append_char(',').append_as_int(r.rb.y); }

typedef float   mat44[4][4];

namespace internal
{
    template<typename IT> uint8 INLINE clamp2byte(IT n)
    {
        return n < 0 ? 0 : (n > 255 ? 255 : (uint8)n);
    }

    template<typename IT> uint8 INLINE clamp2byte_u(IT n)
    {
        return n > 255 ? 255 : (uint8)n;
    }

    template<typename RT, typename IT, bool issigned> struct clamper;

    template<> struct clamper < uint8, float, true >
    {
        static uint8 dojob(float b)
        {
            return clamp2byte<aint>( ts::lround(b) );
        }
    };

    template<> struct clamper < uint8, double, true >
    {
        static uint8 dojob(double b)
        {
            return clamp2byte<aint>( ts::lround(b));
        }
    };

    template<typename IT> struct clamper < uint8, IT, true >
    {
        static uint8 dojob(IT n)
        {
            return clamp2byte<IT>(n);
        }
    };

    template<typename IT> struct clamper < uint8, IT, false >
    {
        static uint8 dojob(IT n)
        {
            return clamp2byte_u<IT>(n);
        }
    };

    template<typename RT, typename IT> struct clamper< RT, IT, false>
    {
        static RT dojob( IT b )
        {
            return b > maximum<RT>::value ? maximum<RT>::value : (RT)b;
        }
    };
    template<typename RT, typename IT> struct clamper < RT, IT, true >
    {
        static RT dojob(IT b)
        {
            return b < minimum<RT>::value ? minimum<RT>::value : (b > maximum<RT>::value ? maximum<RT>::value : (RT)b);
        }
    };

};

template < typename RT, typename IT > INLINE RT CLAMP(IT b)
{
    return internal::clamper<RT, IT, is_signed<IT>::value>::dojob(b);
}

template < typename T1, typename T2, typename T3 >
INLINE bool INSIDE ( const T1 & a, const T2 & vmin, const T3 & vmax )
{
    return (((a) >= (vmin)) && ((a) <= (vmax)));
}

template < typename T1, typename T2, typename T3, typename T4 >
INLINE bool NOTWITHIN ( const T1 & start, const T2 & end, const T3 & vmin, const T4 & vmax )
{
    return ((((start) < (vmin)) && ((end) < (vmin))) || (((start) > (vmax)) && ((end) > (vmax))));
}

template < typename T1, typename T2, typename T3, typename T4 >
INLINE T1 BETWEEN ( const T1 & a, const T2 & b, const T3 & dist, const T4 & pos )
{
    return ((a) + ((pos) * ((b) - (a))) / (dist));
}

template < typename T1 >
INLINE int SIGN ( const T1 & a )
{
    return (((a) == T1(0)) ? 0 : (((a) < T1(0)) ? -1 : 1));
}

INLINE int ROLLING(int v, int vmax)
{
    return  (vmax + (v % vmax)) % vmax;
}

INLINE float ROLL_FLOAT(float v, float vmax)
{
    return fmod((vmax + fmod(v, vmax)), vmax);
}

INLINE bool check_aspect_ratio(int width,int height, int x_ones, int y_ones)
{
    bool ost = 0 == ((width % x_ones) + (height % y_ones));
    bool divs = (width / x_ones) == (height / y_ones);
    return ( ost && divs );
}

ivec2 TSCALL calc_best_coverage( int width,int height, int x_ones, int y_ones );

/*
INLINE int CeilPowerOfTwo(int x)
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    ++x;
    return x;
}
*/

INLINE bool IsPowerOfTwo(const int x)    { return (x & (x - 1)) == 0 && x > 0; }

template<typename T> T LERP(const T&a, const T&b, float t)
{
    t = CLAMP(t, 0.0f, 1.0f);
    return a * (1.0f - t) + t * b;
}


#define ISWAP(a,b) {a^=b^=a^=b;}
#define LERPFLOAT(a, b, t) ( static_cast<float>(a) * (1.0f  - (t) ) + (t) * static_cast<float>(b) )
#define LERPDOUBLE(a, b, t) ( double(a) * (1.0  - double(t) ) + double(t) * double(b) )
#define DOUBLESCALEADD(a, b, s0, s1) (a * s0 + b * s1)
#define HAS_ALL_BITS(code, bits) (((code) & (bits)) == (bits))
#define BIN__N(x) (x) | x>>3 | x>>6 | x>>9
#define BIN__B(x) (x) & 0xf | (x)>>12 & 0xf0
#define BIN8(v) (BIN__B(BIN__N(0x##v)))

#define BIN16(x1,x2) \
    ((BIN8(x1)<<8)+BIN8(x2))

#define BIN24(x1,x2,x3) \
    ((BIN8(x1)<<16)+(BIN8(x2)<<8)+BIN8(x3))

#define BIN32(x1,x2,x3,x4) \
    ((BIN8(x1)<<24)+(BIN8(x2)<<16)+(BIN8(x3)<<8)+BIN8(x4))

#define BIN64(x1,x2,x3,x4,x5,x6,x7,x8) \
    ((__int64(BIN32(x1,x2,x3,x4)) << 32) + __int64(BIN32(x5,x6,x7,x8)))

template<class T> T SQUARE(const T& value) { return value * value; }

double erf(double x);

#define IEEE_FLT_MANTISSA_BITS	23
#define IEEE_FLT_EXPONENT_BITS	8
#define IEEE_FLT_EXPONENT_BIAS	127
#define IEEE_FLT_SIGN_BIT		31
INLINE int ilog2(float f)
{
    return (((*reinterpret_cast<int *>(&f)) >> IEEE_FLT_MANTISSA_BITS) & ((1 << IEEE_FLT_EXPONENT_BITS) - 1)) - IEEE_FLT_EXPONENT_BIAS;
}

// NVidia stuff
#define FP_ONE_BITS 0x3F800000
// r = 1/p
#define FP_INV(r,p)                                                          \
{                                                                            \
    int _i = 2 * FP_ONE_BITS - *(int *)&(p);                                 \
    r = *(float *)&_i;                                                       \
    r = r * (2.0f - (p) * r);                                                \
}

INLINE unsigned char FP_NORM_TO_BYTE2(float p)
{
  float fpTmp = p + 1.0f;
  return (uint8)(((*(unsigned long*)&fpTmp) >> 15) & 0xFF);
}

INLINE unsigned char FP_NORM_TO_BYTE3(float p)
{
  float ftmp = p + 12582912.0f;
  return (uint8)((*(unsigned long *)&ftmp) & 0xFF);
}

INLINE auint DetermineGreaterPowerOfTwo( auint val )
{
    auint num = 1;
    while (val > num)
        num <<= 1;

    return num;
}

INLINE int float_into_int( float x )
{
    return *(int *)&x;
}

INLINE float int_into_float( int x )
{
    return *(float *)&x;
}

INLINE aint TruncFloat( float x )
{
    return aint(x);
}
INLINE aint TruncDouble( double x )
{
    return aint(x);
}

#if defined _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4035)
#endif

INLINE aint RoundToInt(float x)
{
    //union {
    //    float f;
    //    int i;
    //} u = {x + 12582912.0f};		// 2^22+2^23

    //return (int)u.i - 0x4B400000;

    return _mm_cvtss_si32(_mm_load_ss(&x));
}

 INLINE int RoundToIntFullRange(double x)
 {
     union {
         double f;
         int i[2];
     } u = {x + 6755399441055744.0f};		// 2^51+2^52

     return (int)u.i[0];
 }

#if 0
INLINE aint Float2Int( float x )
{
    //return (int)( x + ( (x >= 0) ? 0.5f : (-0.5f)) );
    //return floor( x + 0.5f );
    return RoundToInt( x );
}


INLINE aint Double2Int( double x )
{
    _asm
    {
        fld x
        push eax
        fistp dword ptr [esp]
        pop eax
    }
    //return (int)( x + ( (x >= 0) ? 0.5 : (-0.5)) );
    //return floor( x + 0.5f );

    //return RoundToIntFullRange( x );
 }
#endif

//Fast floor and ceil ("Fast Rounding of Floating Point Numbers in C/C++ on Wintel Platform" - http://ldesoras.free.fr/doc/articles/rounding_en.pdf)
//valid input: |x| < 2^30, wrong results: lfloor(-1e-8) = lceil(1e-8) = 0
INLINE long int lfloor(float x) //http://www.masm32.com/board/index.php?topic=9515.0
{
	x = x + x - 0.5f;
	return _mm_cvtss_si32(_mm_load_ss(&x)) >> 1;
}
INLINE long int lceil(float x) //http://www.masm32.com/board/index.php?topic=9514.0
{
	x = -0.5f - (x + x);
	return -(_mm_cvtss_si32(_mm_load_ss(&x)) >> 1);
}


INLINE TSCOLOR LERPCOLOR(TSCOLOR c0, TSCOLOR c1, float t)
{
	TSCOLOR c = 0;

	c |= 0xFF & ts::lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t);

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & ts::lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 8;

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & ts::lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 16;

    c0 >>= 8; c1 >>= 8;
	c |= (0xFF & ts::lround((0xFF & c0) + (int(0xFF & c1) - int(0xFF & c0)) * t)) << 24;
    return c;
}

vec3 TSCALL RGB_to_HSL(const vec3& rgb);
vec3 TSCALL HSL_to_RGB(const vec3& c);

vec3 TSCALL RGB_to_HSV(const vec3& rgb);
vec3 TSCALL HSV_to_RGB(const vec3& c);

#if defined _MSC_VER
#pragma warning (pop)
#endif

#define SIN_TABLE_SIZE  256
#define FTOIBIAS        12582912.f

namespace ts_math
{
	struct sincostable
	{
		static float table[SIN_TABLE_SIZE];

		sincostable()
		{
			// init sincostable
			for( int i = 0; i < SIN_TABLE_SIZE; i++ )
			{
				table[i] = (float)sin(i * 2.0 * M_PI / SIN_TABLE_SIZE);
			}
		}

	};
}
INLINE float TableCos( float theta )
{
    union
    {
        int i;
        float f;
    } ftmp;

    // ideally, the following should compile down to: theta * constant + constant, changing any of these constants from defines sometimes fubars this.
    ftmp.f = theta * ( float )( SIN_TABLE_SIZE / ( 2.0f * M_PI ) ) + ( FTOIBIAS + ( SIN_TABLE_SIZE / 4 ) ); //-V112
    return ts_math::sincostable::table[ ftmp.i & ( SIN_TABLE_SIZE - 1 ) ];
}

INLINE float TableSin( float theta )
{
    union
    {
        int i;
        float f;
    } ftmp;

    // ideally, the following should compile down to: theta * constant + constant
    ftmp.f = theta * ( float )( SIN_TABLE_SIZE / ( 2.0f * M_PI ) ) + FTOIBIAS;
    return ts_math::sincostable::table[ ftmp.i & ( SIN_TABLE_SIZE - 1 ) ];
}


template <class NUM> INLINE NUM angle_norm(NUM a) // normalize angle. returns -pi .. +pi
{
    //return fmod(a, NUM(2.0*M_PI));
    while(a>M_PI) a -= NUM(2.0*M_PI);
    while(a<=-M_PI) a += NUM(2.0*M_PI);
    return a;
}

template <class NUM> INLINE NUM angle_delta(NUM from, NUM to) // returns -pi .. +pi
{
    while (from < 0.0) from += NUM(2.0*M_PI);
    while (to < 0.0) to += NUM(2.0*M_PI);

    NUM r=to-from;

    if(from < M_PI)
    {
        if(r > M_PI) r -= NUM(2.0*M_PI);
    } else
    {
        if(r < -M_PI) r += NUM(2.0*M_PI);
    }

    return r;
}

INLINE bool point_line_side( const vec2 &p0, const vec2 &p1, const vec2 &p )
{
    vec2 dp = p1-p0;
    vec2 ddp = p-p0;
    return (ddp.x * dp.y - ddp.y * dp.x) >= 0;
}

INLINE double dsqr(double value)
{
    return value * value;
};


INLINE auint isqr(aint x)
{
	return (auint)(x * x);
}

template<typename T> INLINE T isign(T x)
{
    typedef typename sztype<sizeof(T)>::type unsignedT;
	const unsigned bshift = (sizeof(T) * 8 - 1); //-V103
	return (T)((x >> bshift) | (unsignedT(-x) >> bshift));
}

INLINE int slround(int v1, int v2)
{
    int v3 = v1 / v2;
    if (v3 * v2 == v1)
    {
        return v1;
    }
    return v3 * v2 + v2;
}

#define NEG_FLOAT(x) (*((ts::uint32 *)&(x))) ^= 0x80000000;
#define MAKE_ABS_FLOAT(x) (*((ts::uint32 *)&(x))) &= 0x7FFFFFFF;
#define MAKE_ABS_DOUBLE(x) (*(((ts::uint32 *)&(x)) + 1)) &= 0x7FFFFFFF;
#define COPY_SIGN_FLOAT(to, from) { (*((ts::uint32 *)&(to))) = ((*((ts::uint32 *)&(to)))&0x7FFFFFFF) | ((*((ts::uint32 *)&(from))) & 0x80000000); }
#define SET_SIGN_FLOAT(to, sign) {*((ts::uint32 *)&(to)) = ((*((ts::uint32 *)&(to)))&0x7FFFFFFF) | ((ts::uint32(sign)) << 31);}
#define GET_SIGN_FLOAT(x) (((*((ts::uint32 *)&(x))) & 0x80000000) != 0)

template < typename T1 > INLINE T1 tabs(const T1 &x)
{
    return x >= 0 ? x : (-x);
}


template < typename T1, typename T2 > INLINE T1 absmin(const T1 & v1, const T2 & v2)
{
    return (tabs(v1) < tabs(v2)) ? v1 : v2;
}
template < typename T1, typename T2 > INLINE T1 absmax(const T1 & v1, const T2 & v2)
{
    return (tabs(v1) > tabs(v2)) ? v1 : v2;
}

/// Min, 3 arguments
template < typename T1, typename T2, typename T3 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3 )
{
    return tmin ( tmin ( v1, v2 ), v3 );
}

/// Min, 4 arguments
template < typename T1, typename T2, typename T3, typename T4 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ) );
}

/// Min, 5 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5 > INLINE T1 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ), v5 );
}

/// Min, 6 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 > INLINE T2 tmin ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5, const T6 & v6 )
{
    return tmin ( tmin ( v1, v2 ), tmin ( v3, v4 ), tmin ( v5, v6 ) );
}

template <typename T, char N> INLINE T tmin(const vec_t<T, N> &v)
{
    T r = v[0];
    for (aint i = 1; i < N; ++i) r = tmin(r, v[i]);
    return r;
}
template <typename T, char N> INLINE vec_t<T, N> tmin(const vec_t<T, N> &x, const vec_t<T, N> &y)
{
    vec_t<T, N> r;
    EACH_COMPONENT(i) r[i] = tmin(x[i], y[i]);
    return r;
}

/// Max, 3 arguments
template < typename T1, typename T2, typename T3 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3 )
{
    return tmax ( tmax (v1, v2), v3 );
}

/// Max, 4 arguments
template < typename T1, typename T2, typename T3, typename T4 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ) );
}

/// Max, 5 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ), v5 );
}

/// Max, 6 arguments
template < typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 > INLINE T1 tmax ( const T1 & v1, const T2 & v2, const T3 & v3, const T4 & v4, const T5 & v5, const T6 & v6 )
{
    return tmax ( tmax ( v1, v2 ), tmax ( v3, v4 ), tmax ( v5, v6 ) );
}

template <typename T, char N> INLINE T tmax(const vec_t<T, N> &v)
{
    T r = v[0];
    for (aint i = 1; i < N; ++i) r = tmax(r, v[i]);
    return r;
}

template <typename T, char N> INLINE const vec_t<T, N> tmax(const vec_t<T, N> &x, const vec_t<T, N> &y)
{
    vec_t<T, N> r;
    EACH_COMPONENT(i) r[i] = tmax(x[i], y[i]);
    return r;
}

INLINE bool irect::intersected(const irect &i) const
{
    if (lt.x >= i.rb.x) return false;
    if (lt.y >= i.rb.y) return false;
    if (rb.x <= i.lt.x) return false;
    if (rb.y <= i.lt.y) return false;
    return true;
}

INLINE int irect::intersect_area(const irect &i) const
{
    if (lt.x >= i.rb.x) return 0;
    if (lt.y >= i.rb.y) return 0;
    if (rb.x <= i.lt.x) return 0;
    if (rb.y <= i.lt.y) return 0;

    return irect( tmax(lt.x, i.lt.x), tmax(lt.y, i.lt.y), tmin(rb.x, i.rb.x), tmin(rb.y, i.rb.y) ).area();
}

INLINE irect & irect::intersect(const irect &i)
{
	if (lt.x >= i.rb.x) return make_empty();
	if (lt.y >= i.rb.y) return make_empty();
	if (rb.x <= i.lt.x) return make_empty();
	if (rb.y <= i.lt.y) return make_empty();

	lt.x = tmax(lt.x, i.lt.x);
	lt.y = tmax(lt.y, i.lt.y);

	rb.x = tmin(rb.x, i.rb.x);
	rb.y = tmin(rb.y, i.rb.y);

	return *this;
}

INLINE irect & irect::combine(const irect &i)
{
	if (i.lt.x < lt.x) lt.x = i.lt.x;
	if (i.lt.y < lt.y) lt.y = i.lt.y;
	if (i.rb.x > rb.x) rb.x = i.rb.x;
	if (i.rb.y > rb.y) rb.y = i.rb.y;

	return *this;
}



template <typename V = float> class time_value_c
{
public:
    struct key_s
    {
        V       val;
        float   time;
    };
private:
    key_s *m_keys;
    aint m_keys_count;

    void destroy()
    {
        for (aint i=0;i<m_keys_count;++i)
        {
            m_keys[i].~key_s();
        }
    }

public:
    time_value_c( const time_value_c &v ):m_keys(nullptr), m_keys_count(0)
    {
        memcpy( get_keys( v.get_keys_count() ), v.get_keys(), v.get_keys_count() * sizeof(key_s) );
    }
    time_value_c():m_keys(nullptr), m_keys_count(0) {}
    ~time_value_c()
    {
        destroy();
        if ( m_keys ) MM_FREE( m_keys );
    }

    time_value_c & operator=( const time_value_c &v )
    {
        memcpy( get_keys( v.get_keys_count() ), v.get_keys(), v.get_keys_count() * sizeof(key_s) );
        return *this;
    }


    aint get_keys_count() const {return m_keys_count;};
    const key_s *get_keys() const {return m_keys;}
    key_s *get_keys( aint count ) // discard all!!!
    {
        ASSERT( count >= 2 );
        destroy();
        if ( count != m_keys_count )
        {
            m_keys = (key_s *)MM_RESIZE(m_keys, sizeof(key_s) * count );
            m_keys_count = count;
        }
        return m_keys;
    }

    V get_linear( float t ) const
    {
        ASSERT (m_keys_count >= 2);
        const key_s *s = m_keys + 1;
        const key_s *e = m_keys + m_keys_count;

        if (t < m_keys[0].time) return m_keys[0].val;

        for (;s < e;)
        {
            const key_s *f = s - 1;
            if (f->time <= t && t <= s->time)
            {
                float dt = (s->time - f->time);
                if (dt <= 0) return s->val;
                return ( s->val - f->val ) * ((t - f->time) / dt) + f->val;
            }
            ++s;
        }

        return m_keys[m_keys_count-1].val;
    }

    bool  is_empty() const {return m_keys_count == 0;}

};

class time_float_c : public time_value_c<float>
{
    float m_low;
    float m_high;

public:
    time_float_c():m_low(0), m_high(1) {}
    ~time_float_c() {};

    float get_low() const {return m_low;}
    float get_high() const {return m_high;}
    void set_low(float low, bool correct = false);
    void set_high(float hi, bool correct = false);

    float get_linear( float t ) const
    {
        return time_value_c<float>::get_linear(t) * (m_high-m_low) + m_low;
    }
    float get_linear( float t, float amp /* recommended range [0..1] */ ) const
    {
        return time_value_c<float>::get_linear(t) * amp * (m_high-m_low) + m_low;
    }

    float find_t(float v) const; // find across raw values (ignore m_low and m_high)

    template<typename TCHARACTER> void    init( const sptr<TCHARACTER> &val ); // example (x/y pairs): 0/0/0.7/1/1/10
    template<typename TCHARACTER> str_t<TCHARACTER>  initstr() const;

};

class time_float_fixed_c : public time_value_c<float>
{
public:
    time_float_fixed_c() {}
    ~time_float_fixed_c() {};

    float get_linear( float t ) const
    {
        return time_value_c<float>::get_linear(t);
    }
    float get_linear( float t, float amp /* recommended range [0..1] */ ) const
    {
        return time_value_c<float>::get_linear(t) * amp;
    }

    void    init();
    void    init( const wsptr &val );
    wstr_c  initstr() const;

};

class time_color_c : public time_value_c< vec3 >
{

public:
    time_color_c() {}
    ~time_color_c() {};

    void    init( const wsptr &val );
    wstr_c  initstr() const;

};

} // namespace ts

