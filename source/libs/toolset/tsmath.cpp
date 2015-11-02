#include "toolset.h"
#include <math.h>
#include <float.h>

namespace ts 
{
	float ts_math::sincostable::table[SIN_TABLE_SIZE];
	ts_math::sincostable __sincostable;


ivec2 TSCALL calc_best_coverage( int width,int height, int x_ones, int y_ones )
{
    ivec2 p1;


    for (int h = height; h >= y_ones; --h)
    {
        if ( 0 == ((h * x_ones) % y_ones) )
        {
            int neww =(h * x_ones) / y_ones;
            if (neww > width) continue;
            p1.x = neww;
            p1.y = h;
            break;
        }
    }
    
    return p1;
}

double erf(double x)
{
    double y,z,xnum,xden,del,result;
    int i;
    const double xbreak = 0.46875;
    const double pi=3.14159265358979;


    y = fabs(x);

    //  evaluate  erf  for  |x| <= 0.46875
    if (y<xbreak)
    {
        double a[] = {3.16112374387056560e00, 1.13864154151050156e02, 3.77485237685302021e02,
                      3.20937758913846947e03, 1.85777706184603153e-1};
        double b[] = {2.36012909523441209e01, 2.44024637934444173e02,  1.28261652607737228e03,
                      2.84423683343917062e03};

        z = y * y;
        xnum = a[4] * z;
        xden = z;
        for (i=0; i<3; i++)
        {
            xnum = (xnum + a[i]) * z;
            xden = (xden + b[i]) * z;
        }
        result= x * (xnum + a[3]) / (xden + b[3]);
    }
    //  evaluate  erfc  for 0.46875 <= |x| <= 4.0
    else if ((y > xbreak) && (y <= 4.))
    {
        double c[] = {5.64188496988670089e-1, 8.88314979438837594e00, 6.61191906371416295e01,
                      2.98635138197400131e02, 8.81952221241769090e02, 1.71204761263407058e03,
                      2.05107837782607147e03, 1.23033935479799725e03, 2.15311535474403846e-8};
        double d[] = {1.57449261107098347e01, 1.17693950891312499e02, 5.37181101862009858e02,
                      1.62138957456669019e03, 3.29079923573345963e03, 4.36261909014324716e03,
                      3.43936767414372164e03, 1.23033935480374942e03};

        xnum = c[8] * y;
        xden = y;
        for (i=0; i<7; i++)
        {
            xnum = (xnum + c[i]) * y;
            xden = (xden + d[i]) * y;
        }
        result = (xnum + c[7]) / (xden + d[7]);
        z = ((int)(y*16))/16.0;
        del = (y-z)*(y+z);
        result *= exp(-z*z-del);
    }
    //   evaluate  erfc  for |x| > 4.0
    else
    {
        double p[] = {3.05326634961232344e-1, 3.60344899949804439e-1, 1.25781726111229246e-1,
                      1.60837851487422766e-2, 6.58749161529837803e-4, 1.63153871373020978e-2};
        double q[] = {2.56852019228982242e00, 1.87295284992346047e00, 5.27905102951428412e-1,
                      6.05183413124413191e-2, 2.33520497626869185e-3};
        z = 1 / (y * y);
        xnum = p[5] * z;
        xden = z;
        for (i=0; i<4; i++)
        {
            xnum = (xnum + p[i]) * z;
            xden = (xden + q[i]) * z;
        }
        result = z * (xnum + p[4]) / (xden + q[4]);
        result = (1/sqrt(pi) -  result) / y;
        z = ((int)(y*16))/16.0;
        del = (y-z) * (y+z);
        result *= exp(-z*z-del);
    }

    //   fix up for negative argument, erf, etc.
    if (x > xbreak)
        result = 1 - result;
    if (x < -xbreak)
        result = result - 1;

    return result;
}


template<typename TCHARACTER>  str_t<TCHARACTER>  time_float_c::initstr() const
{
    const key_s *keys = get_keys();

    str_t<TCHARACTER> r;
    int cnt = get_keys_count();
    for (int i=0;i<cnt;++i)
        r.append_as_float(keys[i].time).append_char('/').append_as_float(keys[i].val).append_char('/');
    r.trunc_length();
    return r;
}

template str_c  time_float_c::initstr() const;
template wstr_c  time_float_c::initstr() const;

template<typename TCHARACTER>  void time_float_c::init( const sptr<TCHARACTER> &val )
{
    strings_t<TCHARACTER> sa(val, '/');
    ASSERT( (sa.size() & 1) == 0 && sa.size() >= 4 );

    key_s *keys = get_keys( sa.size() / 2 );

    int cnt = sa.size();
    for (int i=0;i<cnt;i+=2,++keys)
    {
        keys->time = sa.get(i).as_float();
        keys->val = sa.get(i+1).as_float();
    }
}

template  void time_float_c::init( const asptr &val );
template  void time_float_c::init( const wsptr &val );

float time_float_c::find_t(float v) const
{
    const key_s *keys = get_keys();
    int cnt = get_keys_count() - 1;
    for(int i=0;i<cnt;++i)
    {
        const key_s &k0 = *(keys + i);
        const key_s &k1 = *(keys + i + 1);

        if ( (v >= k0.val && v <= k1.val) || (v <= k0.val && v >= k1.val) )
        {
            float dval = k1.val - k0.val;
            if (dval == 0) return k0.time; //-V550
            return (v - k0.val) * (k1.time - k0.time) / dval + k0.time;
        }
        
    }
    return keys->time;
}

void time_float_c::set_low(float low, bool correct)
{
    if (correct)
    {
        float k = m_high-low;
        if (k != 0) //-V550
        {
            k = 1.0f / k;
            for (int i=0;i<get_keys_count();++i)
            {
                float x = get_keys()[i].val;
                x = CLAMP( (x * (m_high - m_low) + m_low - low) * k, low, m_high );
                const_cast<key_s *>(get_keys())[i].val = x;
            }
        }
    }

    m_low = low;
}
void time_float_c::set_high(float hi, bool correct)
{
    if (correct)
    {
        float k = hi-m_low;
        if (k != 0) //-V550
        {
            k = 1.0f / k;
            for (int i=0;i<get_keys_count();++i)
            {
                float x = get_keys()[i].val;
                x = CLAMP( (x * (m_high - m_low) ) * k, m_low, hi );
                const_cast<key_s *>(get_keys())[i].val = x;
            }
        }
    }
    m_high = hi;
}




wstr_c  time_float_fixed_c::initstr() const
{
    const key_s *keys = get_keys();

    wstr_c r;
    for (int i=0;i<get_keys_count();++i)
    {
        r.append_as_float(keys[i].time).append_char('/').append_as_float(keys[i].val).append_char('/');
    }
    r.trunc_length();
    return r;
}

void time_float_fixed_c::init( const wsptr &val )
{
    wstrings_c sa(val, '/');
    ASSERT( (sa.size() & 1) == 0 && sa.size() >= 4 );

    key_s *keys = get_keys( sa.size() / 2 );

    int cnt = sa.size();
    for (int i=0;i<cnt;i+=2,++keys)
    {
        keys->time = sa.get(i).as_float();
        keys->val = sa.get(i+1).as_float();
    }
}


void    time_float_fixed_c::init()
{
    key_s *keys = get_keys( 2 );
    keys[0].time = 0;
    keys[0].val = 1;
    keys[1].time = 1;
    keys[1].val = 1;
}


wstr_c  time_color_c::initstr() const
{
    const key_s *keys = get_keys();

    wstr_c r;
    for (int i=0;i<get_keys_count();++i)
    {
        r.append_as_float(keys[i].time).append_char('/')
            .append_as_float(keys[i].val.x).append_char('/')
            .append_as_float(keys[i].val.y).append_char('/')
            .append_as_float(keys[i].val.z).append_char('/');
    }
    r.trunc_length();
    return r;
}

void time_color_c::init( const wsptr &val )
{
    wstrings_c sa(val, '/');
    ASSERT( (sa.size() & 3) == 0 && sa.size() >= 8 );

    key_s *keys = get_keys( sa.size() / 4 );

    int cnt = sa.size();
    for (int i=0;i<cnt;i+=4,++keys)
    {
        keys->time = sa.get(i).as_float();
        keys->val.x = sa.get(i+1).as_float();
        keys->val.y = sa.get(i+2).as_float();
        keys->val.z = sa.get(i+3).as_float();
    }
}



//#pragma optimize ( "tp", on )

vec3 TSCALL RGB_to_HSL(const vec3& rgb)
{
    float var_Min = tmin(rgb.r, rgb.g, rgb.b);   // Min. value of RGB
    float var_Max = tmax(rgb.r, rgb.g, rgb.b);   // Max. value of RGB
    float del_Max = var_Max - var_Min;          // Delta RGB value

    float L = (var_Max + var_Min) / 2.0f;

    if (del_Max == 0.0f) //-V550
    {
        // This is a gray, no chroma...
        return vec3(0.0f, 0.0f, L); // HSL results = 0 ? 1
    } else {                                    // Chromatic data...
        float S = 0.0f;

        if (L < 0.5f) S = del_Max / (var_Max + var_Min);
        else          S = del_Max / (2.0f - var_Max - var_Min);

        float del_R = (((var_Max - rgb.r) / 6.0f) + (del_Max / 2.0f)) / del_Max;
        float del_G = (((var_Max - rgb.g) / 6.0f) + (del_Max / 2.0f)) / del_Max;
        float del_B = (((var_Max - rgb.b) / 6.0f) + (del_Max / 2.0f)) / del_Max;

        float H = 0.0f;

        if (rgb.r == var_Max) H = del_B - del_G;
        else if (rgb.g == var_Max) H = (1.0f / 3.0f) + del_R - del_B;
        else if (rgb.b == var_Max) H = (2.0f / 3.0f) + del_G - del_R;

        if (H < 0.0f) H += 1.0f;
        if (H > 1.0f) H -= 1.0f;
        return vec3(H, S, L);
    }
}

float HUE_to_RGB(const float v1, const float v2, float vH)  // Function HUE_to_RGB
{
    if (vH < 0.0f) vH += 1.0f;
    if (vH > 1.0f) vH -= 1.0f;
    if ((6.0f * vH) < 1.0f) return v1 + (v2 - v1) * 6.0f * vH;
    if ((2.0f * vH) < 1.0f) return v2;
    if ((3.0f * vH) < 2.0f) return v1 + (v2 - v1) * ((2.0f / 3.0f) - vH) * 6.0f;
    return v1;
}

vec3 TSCALL HSL_to_RGB(const vec3& c)
{
    float H = c.r;
    float S = c.g;
    float L = c.b;

    
    if (S == 0) //-V550 // HSL values = 0 ? 1
    {
        return vec3(L, L, L); // RGB results = 0 ? 255
    } else { 
        float var_2 = 0.0f;
        if (L < 0.5) var_2 = L * (1.0f + S);
        else         var_2 = (L + S) - (S * L);

        float var_1 = 2.0f * L - var_2;
        return vec3(HUE_to_RGB(var_1, var_2, H + (1.0f / 3.0f)),
            HUE_to_RGB(var_1, var_2, H),
            HUE_to_RGB(var_1, var_2, H - (1.0f / 3.0f))
            );
    } 
}

vec3 TSCALL RGB_to_HSV(const vec3& rgb)
{
    //float var_Min = tmin(rgb.r, rgb.g, rgb.b);   // Min. value of RGB
    //float var_Max = tmax(rgb.r, rgb.g, rgb.b);   // Max. value of RGB
    //float del_Max = var_Max - var_Min;          // Delta RGB value

    ////float L = (var_Max + var_Min) / 2.0f;

    //if (del_Max == 0.0f)                        // This is a gray, no chroma...
    //{
    //    return vec3(0.0f, 0.0f, var_Max);
    //} else {                                    // Chromatic data...
    //    float S = (var_Max == 0.0) ? 0.0f : (var_Min / var_Max);
    //    float H = 0;

    //    if (var_Max == rgb.r)
    //    {
    //        H = (rgb.g - rgb.b) / (6.0f * del_Max);
    //        if ( rgb.g < rgb.b ) H += 1.0f;
    //    } else if (var_Max == rgb.g)
    //    {
    //        H = (rgb.b - rgb.r) / (6.0f * del_Max) + 1.0f / 3.0f;
    //    }if (var_Max == rgb.b)
    //    {
    //        H = (rgb.r - rgb.g) / (6.0f * del_Max) + 2.0f / 3.0f;
    //    }

    //    return vec3(H, S, var_Max);
    //}

    double rd, gd, bd, h, s, v, dmax, dmin, del, rc, gc, bc;

    rd = rgb.r;
    gd = rgb.g;
    bd = rgb.b;

    /* compute maximum of rd,gd,bd */
    if (rd >= gd)
        dmax = tmax (rd, bd);
    else
        dmax = tmax (gd, bd);

    /* compute minimum of rd,gd,bd */
    if (rd <= gd)
        dmin = tmin (rd, bd);
    else
        dmin = tmin (gd, bd);

    del = dmax - dmin;
    v = dmax;
    if (dmax != 0.0) //-V550
        s = del / dmax;
    else
        s = 0.0;

    h = 0;
    if (s != 0.0) //-V550
    {
        rc = (dmax - rd) / del;
        gc = (dmax - gd) / del;
        bc = (dmax - bd) / del;

        if (rd == dmax) //-V550
            h = bc - gc;
        else if (gd == dmax) //-V550
            h = 2 + rc - bc;
        else if (bd == dmax) //-V550
            h = 4 + gc - rc;
        if (h < 0)
            h += 6;
    }
    //hsv[0] = h;
    //hsv[1] = s;
    //hsv[2] = v;

    return vec3( (float)(h / 6.0), (float)s, (float)v );

}
vec3 TSCALL HSV_to_RGB(const vec3& c)
{
    double h = c.r * 6, s = c.g, v = c.b;
    int    j;
    double rd, gd, bd;
    double f, p, q, t;

    while (h >= 6.0)
        h = h - 6.0;
    while (h <  0.0)
        h = h + 6.0;
    j = (int) floor (h);
    f = h - j;
    p = v * (1 - s);
    q = v * (1 - (s * f));
    t = v * (1 - (s * (1 - f)));

    switch (j)
    {
    case 0:
        rd = v;  gd = t;  bd = p;  break;
    case 1:
        rd = q;  gd = v;  bd = p;  break;
    case 2:
        rd = p;  gd = v;  bd = t;  break;
    case 3:
        rd = p;  gd = q;  bd = v;  break;
    case 4:
        rd = t;  gd = p;  bd = v;  break;
    case 5:
        rd = v;  gd = p;  bd = q;  break;
    default:
        rd = v;  gd = t;  bd = p;  break;
    }

    return vec3( (float)rd, (float)gd, (float)bd );
    //rgb[0] = rd;
    //rgb[1] = gd;
    //rgb[2] = bd;
}



//#pragma optimize( "", on ) 






} // namespace ts
