#pragma once

template <class T, int N> struct vec_t;
template <class T, int N> struct vecbase_t;
template <class T> struct vecbase_t < T, 2 >
{
	union { T x, r, s, r0; };
	union { T y, g, t, r1; };
	typedef T TYPE;//for external use only

    template <class T> INLINE const T cross(const vecbase_t<T, 2> &v1) const //=dot(perp(v0), v1)
    {
        const vecbase_t<T, 2> &v0 = *this;
        return v0.x*v1.y - v0.y*v1.x;
    }

    INLINE double angle(const vecbase_t<T, 2>& v) const
    {
        double cdot = x * v.x + y * v.y;
        double ccross = cross(v);
        // angle between segments
        return atan2(ccross, cdot);
    }

    INLINE vec_t<T, 2>& rotate(float fAngle);
    INLINE vec_t<T, 2>& rotate(const vec_t<T, 2>& vCentre, float fAngle);
    INLINE vec_t<T, 2>& clamp(const vec_t<T, 2>& _min_, const vec_t<T, 2>& _max_);
    INLINE vec_t<T, 2> perpl() const;
    INLINE vec_t<T, 2> perpr() const;

    INLINE T delta() const { return r1-r0; }

    INLINE vec_t<T, 2> operator&(const vec_t<T, 2> &b) const;

};
template <class T> struct vecbase_t<T, 3> : vecbase_t < T, 2 >
{
	union { T z, b, p, r2; };
	vec_t<T, 2> &xy() { return (vec_t<T, 2>&)x; }
	vec_t<T, 2> &rg() { return (vec_t<T, 2>&)x; }
	vec_t<T, 2> &st() { return (vec_t<T, 2>&)x; }
	vec_t<T, 2> &yz() { return (vec_t<T, 2>&)y; }
	vec_t<T, 2> &gb() { return (vec_t<T, 2>&)y; }
	vec_t<T, 2> &tp() { return (vec_t<T, 2>&)y; }
	const vec_t<T, 2> &xy() const { return (vec_t<T, 2>&)x; }
	const vec_t<T, 2> &rg() const { return (vec_t<T, 2>&)x; }
	const vec_t<T, 2> &st() const { return (vec_t<T, 2>&)x; }
	const vec_t<T, 2> &yz() const { return (vec_t<T, 2>&)y; }
	const vec_t<T, 2> &gb() const { return (vec_t<T, 2>&)y; }
	const vec_t<T, 2> &tp() const { return (vec_t<T, 2>&)y; }

    template <class T> INLINE const vec_t<T, 3> cross(const vec_t<T, 3> &v1) const
    {
        const vec_t<T, 3> &v0 = *this;
        return vec_t<T, 3>(v0.y*v1.z - v0.z*v1.y, v0.z*v1.x - v0.x*v1.z, v0.x*v1.y - v0.y*v1.x);
    }

};
template <class T> struct vecbase_t<T, 4> : vecbase_t < T, 3 >
{
	union { T w, a, q, r3; };
	vec_t<T, 2> &zw() { return (vec_t<T, 2>&)z; }
	vec_t<T, 2> &ba() { return (vec_t<T, 2>&)z; }
	vec_t<T, 2> &pq() { return (vec_t<T, 2>&)z; }
	const vec_t<T, 2> &zw() const { return (vec_t<T, 2>&)z; }
	const vec_t<T, 2> &ba() const { return (vec_t<T, 2>&)z; }
	const vec_t<T, 2> &pq() const { return (vec_t<T, 2>&)z; }
	vec_t<T, 3> &xyz() { return (vec_t<T, 3>&)x; }
	vec_t<T, 3> &rgb() { return (vec_t<T, 3>&)x; }
	vec_t<T, 3> &stp() { return (vec_t<T, 3>&)x; }
	vec_t<T, 3> &yzw() { return (vec_t<T, 3>&)y; }
	vec_t<T, 3> &gba() { return (vec_t<T, 3>&)y; }
	vec_t<T, 3> &tpq() { return (vec_t<T, 3>&)y; }
	const vec_t<T, 3> &xyz() const { return (vec_t<T, 3>&)x; }
	const vec_t<T, 3> &rgb() const { return (vec_t<T, 3>&)x; }
	const vec_t<T, 3> &stp() const { return (vec_t<T, 3>&)x; }
	const vec_t<T, 3> &yzw() const { return (vec_t<T, 3>&)y; }
	const vec_t<T, 3> &gba() const { return (vec_t<T, 3>&)y; }
	const vec_t<T, 3> &tpq() const { return (vec_t<T, 3>&)y; }
};

// vecs

#define EACH_COMPONENT(i) for (aint i = 0; i < N; ++i)

template <typename T, aint N> struct vec_t : public vecbase_t<T,N>
{
private:
	static int __sqrt(int n) { return ts::lround(fastsqrt((float)n)); }
	static float __sqrt(float n) { return fastsqrt(n); }

    template <class T> static INLINE const T __dot(const vec_t<T, 2> &v0, const vec_t<T, 2> &v1) { return v0.x*v1.x + v0.y*v1.y; }
    template <class T> static INLINE const T __dot(const vec_t<T, 3> &v0, const vec_t<T, 3> &v1) { return v0.x*v1.x + v0.y*v1.y + v0.z*v1.z; }
    template <class T> static INLINE const T __dot(const vec_t<T, 4> &v0, const vec_t<T, 4> &v1) { return v0.x*v1.x + v0.y*v1.y + v0.z*v1.z + v0.w*v1.w; }

public:

    INLINE vec_t() {};
    template <class TT> INLINE explicit vec_t(const vec_t<TT, N> &v) { EACH_COMPONENT(i) (&x)[i] = (T)v[i]; }
    //copy constructor
    INLINE vec_t(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] = v[i]; }
    //1 arg
    INLINE explicit vec_t(const T a) { EACH_COMPONENT(i) (&x)[i] = a; }
    template <int n> INLINE explicit vec_t(const vec_t<T, n> &v)
    {
        typedef char staticCheck[n - N];
        EACH_COMPONENT(i) (&x)[i] = v[i];
    }
    //vec2
    INLINE vec_t(const T x_, const T y_) { x = x_; y = y_; TS_STATIC_CHECK(N == 2, "vec2 expected"); }
    //vec3
    INLINE vec_t(const vec_t<T, 2> &xy, const T z_)   { x = xy.x; y = xy.y; z = z_;   TS_STATIC_CHECK(N == 3, "vec3 expected"); }
    INLINE vec_t(const T x_, const vec_t<T, 2> &yz)   { x = x_;   y = yz.x; z = yz.y; TS_STATIC_CHECK(N == 3, "vec3 expected"); } //-V537
    INLINE vec_t(const T x_, const T y_, const T z_) { x = x_;   y = y_;   z = z_;    TS_STATIC_CHECK(N == 3, "vec3 expected"); }
    //vec4
    //2 arg
    INLINE vec_t(const vec_t<T, 3> &xyz, const T w_)         { x = xyz.x; y = xyz.y; z = xyz.z; w = w_; }
    INLINE vec_t(const T x_, const vec_t<T, 3> &yzw)         { x = x_;    y = yzw.x; z = yzw.y; w = yzw.z; } //-V537
    INLINE vec_t(const vec_t<T, 2> &xy, const vec_t<T, 2> &zw) { x = xy.x;  y = xy.y;  z = zw.x;  w = zw.y; }
    //3 arg
    INLINE vec_t(const vec_t<T, 2> &xy, const T z_, const T w_) { x = xy.x; y = xy.y; z = z_;   w = w_; }
    INLINE vec_t(const T x_, const vec_t<T, 2> &yz, const T w_) { x = x_;   y = yz.x; z = yz.y; w = w_; } //-V537
    INLINE vec_t(const T x_, const T y_, const vec_t<T, 2> &zw) { x = x_;   y = y_;   z = zw.x; w = zw.y; }
    //4 arg
    INLINE vec_t(const T x_, const T y_, const T z_, const T w_) { x = x_; y = y_; z = z_; w = w_; }


    //operators
    INLINE T &operator [] (aint i) { return (&x)[i]; }
    INLINE const T &operator [] (aint i) const { return (&x)[i]; }

    //vector ~ a
    INLINE const vec_t operator+(const T a) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] + a; return __r; }
    INLINE const vec_t operator-(const T a) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] - a; return __r; }
    INLINE const vec_t operator*(const T a) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] * a; return __r; }
    INLINE const vec_t operator/(const T a) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] / a; return __r; }

    //a ~ vector
    INLINE friend const vec_t operator+(const T a, const vec_t &v) { vec_t __r; EACH_COMPONENT(i) __r[i] = a + v[i]; return __r; }
    INLINE friend const vec_t operator-(const T a, const vec_t &v) { vec_t __r; EACH_COMPONENT(i) __r[i] = a - v[i]; return __r; }
    INLINE friend const vec_t operator*(const T a, const vec_t &v) { vec_t __r; EACH_COMPONENT(i) __r[i] = a * v[i]; return __r; }
    INLINE friend const vec_t operator/(const T a, const vec_t &v) { vec_t __r; EACH_COMPONENT(i) __r[i] = a / v[i]; return __r; }

    //bools
    INLINE bool operator >(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] > v[i]) return true; return false; } // if any
    INLINE bool operator <(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] < v[i]) return true; return false; } // if any
    INLINE bool operator ==(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] != v[i]) return false; return true; }
    INLINE bool operator !=(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] != v[i]) return true; return false; }

    INLINE bool operator >>=(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] < v[i]) return false; return true; } // if all


    INLINE bool operator >>(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] <= v[i]) return false; return true; } // if all
    INLINE bool operator <<(const vec_t& v) const { EACH_COMPONENT(i) if ((&x)[i] >= v[i]) return false; return true; } // if all

    INLINE bool operator >(const T a) const { EACH_COMPONENT(i) if ((&x)[i] > a) return true; return false; } // if any
    INLINE bool operator >=(const T a) const { EACH_COMPONENT(i) if ((&x)[i] >= a) return true; return false; } // if any
    INLINE bool operator <(const T a) const { EACH_COMPONENT(i) if ((&x)[i] < a) return true; return false; } // if any
    INLINE bool operator <=(const T a) const { EACH_COMPONENT(i) if ((&x)[i] <= a) return true; return false; } // if any

    INLINE bool operator >>(const T a) const { EACH_COMPONENT(i) if ((&x)[i] <= a) return false; return true; } // if all
    INLINE bool operator <<(const T a) const { EACH_COMPONENT(i) if ((&x)[i] >= a) return false; return true; } // if all


    //vector ~ vector
    INLINE const vec_t operator+(const vec_t &v) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] + v[i]; return __r; }
    INLINE const vec_t operator-(const vec_t &v) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] - v[i]; return __r; }
    INLINE const vec_t operator*(const vec_t &v) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] * v[i]; return __r; }
    INLINE const vec_t operator/(const vec_t &v) const { vec_t __r; EACH_COMPONENT(i) __r[i] = (&x)[i] / v[i]; return __r; }

    //vector ~= a
    INLINE const vec_t &operator+=(const T a) { EACH_COMPONENT(i) (&x)[i] += a; return *this; }
    INLINE const vec_t &operator-=(const T a) { EACH_COMPONENT(i) (&x)[i] -= a; return *this; }
    INLINE const vec_t &operator*=(const T a) { EACH_COMPONENT(i) (&x)[i] *= a; return *this; }
    INLINE const vec_t &operator/=(const T a) { EACH_COMPONENT(i) (&x)[i] /= a; return *this; }

    //vector ~= vector
    INLINE const vec_t &operator+=(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] += v[i]; return *this; }
    INLINE const vec_t &operator-=(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] -= v[i]; return *this; }
    INLINE const vec_t &operator*=(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] *= v[i]; return *this; }
    INLINE const vec_t &operator/=(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] /= v[i]; return *this; }
    INLINE const vec_t &operator =(const vec_t &v) { EACH_COMPONENT(i) (&x)[i] = v[i]; return *this; }

    //-vector
    INLINE const vec_t operator-() const { vec_t re; EACH_COMPONENT(i) re[i] = -(&x)[i]; return re; }

    //++vector and --vector
    INLINE const vec_t operator++() { EACH_COMPONENT(i) ++(&x)[i]; return *this; }
    INLINE const vec_t operator--() { EACH_COMPONENT(i) --(&x)[i]; return *this; }

    //vector++ and vector--
    INLINE const vec_t operator++(int notused) { vec_t t(*this); EACH_COMPONENT(i) ++(&x)[i]; return t; }
    INLINE const vec_t operator--(int notused) { vec_t t(*this); EACH_COMPONENT(i) --(&x)[i]; return t; }

	/// dot product
    INLINE T	dot(const vec_t & v) const { return __dot(*this, v); }

	INLINE T len() const
	{
		return __sqrt(dot(*this));
	}
	INLINE T sqlen() const
	{
        return dot(*this);
	}
    INLINE operator bool() const
    {
        EACH_COMPONENT(i)
            if ((&x)[i]) return true;
        return false;
    }
};

template<class T> INLINE vec_t<T, 2>& vecbase_t<T, 2>::rotate(float fAngle)
{
    double s, c;
    sincos(fAngle, s, c);

    float tx = x;
    x = x * c - y * s; //-V537
    y = tx * s + y * c;
    return *this;
}

template<class T> INLINE vec_t<T, 2>& vecbase_t<T, 2>::rotate(const vec_t<T, 2>& vCentre, float fAngle)
{
    vec_t<T, 2> D = *this - vCentre;
    D.rotate(fAngle);
    *this = vCentre + D;
    return *this;
}

template<class T> INLINE vec_t<T, 2>& vecbase_t<T, 2>::clamp(const vec_t<T, 2>& _min_, const vec_t<T, 2>& _max_)
{
    x = (x < _min_.x) ? _min_.x : ((x > _max_.x) ? _max_.x : x);
    y = (y < _min_.y) ? _min_.y : ((y > _max_.y) ? _max_.y : y);
    return *this;
}

template<class T> INLINE vec_t<T, 2> vecbase_t<T, 2>::perpl() const   { return vec2(-y, x); }
template<class T> INLINE vec_t<T, 2> vecbase_t<T, 2>::perpr() const   { return vec2(y, -x); }
template<class T> INLINE vec_t<T, 2> vecbase_t<T, 2>::operator&(const vec_t<T, 2> &b) const
{
   return vec_t<T, 2>( tmax(r0, b.r0), tmin(r1, b.r1) );
}



typedef vec_t<float, 2> vec2;
typedef vec_t<int, 2> ivec2;
typedef vec_t<short, 2> svec2;

typedef vec_t<float,3> vec3;
typedef vec_t<int,3> ivec3;

typedef vec_t<float, 4> vec4;
typedef vec_t<int, 4> ivec4;


template<> INLINE ivec2 &make_dummy<ivec2>(bool quiet) { static ivec2 t(0); DUMMY_USED_WARNING(quiet); return t; }


INLINE vec2 make_direction(float angle)   { float s, c; sincos(angle, s, c); return vec2(c, s); }

template<> inline str_c amake(ivec2 v) { return str_c().set_as_int(v.x).append_char(',').append_as_int(v.y); }
