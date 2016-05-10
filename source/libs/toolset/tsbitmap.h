#pragma once

#include "internal/imageformat.h"

namespace ts
{

    INLINE uint8 RED( TSCOLOR c ) { return as_byte( c >> 16 ); }
    INLINE uint8 GREEN( TSCOLOR c ) { return as_byte( c >> 8 ); }
    INLINE uint8 BLUE( TSCOLOR c ) { return as_byte( c ); }
    INLINE uint8 ALPHA( TSCOLOR c ) { return as_byte( c >> 24 ); }

    INLINE uint16 REDx256( TSCOLOR c ) { return 0xff00 & ( c >> 8 ); }
    INLINE uint16 GREENx256( TSCOLOR c ) { return 0xff00 & ( c ); }
    INLINE uint16 BLUEx256( TSCOLOR c ) { return 0xff00 & ( c << 8 ); }
    INLINE uint16 ALPHAx256( TSCOLOR c ) { return 0xff00 & ( c >> 16 ); }

    template <typename CCC> INLINE TSCOLOR ARGB( CCC r, CCC g, CCC b, CCC a = 255 )
    {
        return CLAMP<uint8, CCC>( b ) | ( CLAMP<uint8, CCC>( g ) << 8 ) | ( CLAMP<uint8, CCC>( r ) << 16 ) | ( CLAMP<uint8, CCC>( a ) << 24 );
    }

    INLINE auint GRAYSCALE_C( TSCOLOR c )
    {
        return ts::lround( float( BLUE( c ) ) * 0.114f + float( GREEN( c ) ) * 0.587f + float( RED( c ) ) * 0.299 );
    }

    INLINE TSCOLOR GRAYSCALE( TSCOLOR c )
    {
        auint oi = GRAYSCALE_C( c );
        return ARGB<auint>( oi, oi, oi, ALPHA( c ) );
    }

    INLINE bool PREMULTIPLIED( TSCOLOR c )
    {
        uint8 a = ALPHA( c );
        return RED( c ) <= a && GREEN( c ) <= a && BLUE( c ) <= a;
    }

    INLINE TSCOLOR PREMULTIPLY( TSCOLOR c, float a )
    {
        auint oiB = ts::lround( float( BLUE( c ) ) * a );
        auint oiG = ts::lround( float( GREEN( c ) ) * a );
        auint oiR = ts::lround( float( RED( c ) ) * a );

        return ARGB<auint>( oiR, oiG, oiB, ALPHA( c ) );
    }

    INLINE TSCOLOR PREMULTIPLY( TSCOLOR c )
    {
        extern uint8 ALIGN(256) multbl[ 256 ][ 256 ];

        uint a = ALPHA( c );
        return multbl[ a ][ c & 0xff ] |
            ( (uint)multbl[ a ][ ( c >> 8 ) & 0xff ] << 8 ) |
            ( (uint)multbl[ a ][ ( c >> 16 ) & 0xff ] << 16 ) |
            ( c & 0xff000000 );

    }

    INLINE TSCOLOR PREMULTIPLY( TSCOLOR c, uint8 aa, double &not_a ) // premultiply with addition alpha and return not-alpha
    {
        double a = ( (double)( ALPHA( c ) * aa ) * ( 1.0 / 65025.0 ) );
        not_a = 1.0 - a;

        auint oiB = ts::lround( float( BLUE( c ) ) * a );
        auint oiG = ts::lround( float( GREEN( c ) ) * a );
        auint oiR = ts::lround( float( RED( c ) ) * a );
        auint oiA = ts::lround( a * 255.0 );

        return ARGB<auint>( oiR, oiG, oiB, oiA );
    }

    INLINE TSCOLOR UNMULTIPLY( TSCOLOR c )
    {
        extern uint8 ALIGN(256) divtbl[ 256 ][ 256 ];

        uint a = ALPHA( c );
        return divtbl[ a ][ c & 0xff ] |
            ( (uint)divtbl[ a ][ ( c >> 8 ) & 0xff ] << 8 ) |
            ( (uint)divtbl[ a ][ ( c >> 16 ) & 0xff ] << 16 ) |
            ( c & 0xff000000 );

    }

    INLINE TSCOLOR MULTIPLY( TSCOLOR c1, TSCOLOR c2 )
    {
        extern uint8 ALIGN(256) multbl[ 256 ][ 256 ];

        return multbl[ c1 & 0xff ][ c2 & 0xff ] |
            ( (uint)multbl[ ( c1 >> 8 ) & 0xff ][ ( c2 >> 8 ) & 0xff ] << 8 ) |
            ( (uint)multbl[ ( c1 >> 16 ) & 0xff ][ ( c2 >> 16 ) & 0xff ] << 16 ) |
            ( (uint)multbl[ ( c1 >> 24 ) & 0xff ][ ( c2 >> 24 ) & 0xff ] << 24 );

    }

    INLINE TSCOLOR ALPHABLEND( TSCOLOR target, TSCOLOR source, int constant_alpha = 255 ) // photoshop like Normal mode color blending
    {
        uint8 oA = ALPHA( target );

        if ( oA == 0 )
        {
            auint a = ts::lround( double( constant_alpha * ALPHA( source ) ) * ( 1.0 / 255.0 ) );
            return ( 0x00FFFFFF & source ) | ( CLAMP<uint8>( a ) << 24 );
        }

        uint8 oR = RED( target );
        uint8 oG = GREEN( target );
        uint8 oB = BLUE( target );

        uint8 R = RED( source );
        uint8 G = GREEN( source );
        uint8 B = BLUE( source );


        float A = float( double( constant_alpha * ALPHA( source ) ) * ( 1.0 / ( 255.0 * 255.0 ) ) );
        float nA = 1.0f - A;

        auint oiA = ts::lround( 255.0f * A + float( oA ) * nA );

        float k = 0;
        if ( oiA ) k = 1.0f - ( A * 255.0f / (float)oiA );

        auint oiB = ts::lround( float( B ) * A + float( oB ) * k );
        auint oiG = ts::lround( float( G ) * A + float( oG ) * k );
        auint oiR = ts::lround( float( R ) * A + float( oR ) * k );

        return ARGB<auint>( oiR, oiG, oiB, oiA );
    }

    INLINE TSCOLOR ALPHABLEND_PM_NO_CLAMP( TSCOLOR dst, TSCOLOR src ) // premultiplied alpha blend
    {
        extern uint8 ALIGN(256) multbl[ 256 ][ 256 ];

        uint8 not_a = 255 - ALPHA( src );

        return src + ( ( multbl[ not_a ][ dst & 0xff ] ) |
            ( ( (uint)multbl[ not_a ][ ( dst >> 8 ) & 0xff ] ) << 8 ) |
            ( ( (uint)multbl[ not_a ][ ( dst >> 16 ) & 0xff ] ) << 16 ) |
            ( ( (uint)multbl[ not_a ][ ( dst >> 24 ) & 0xff ] ) << 24 ) );
    }

    INLINE TSCOLOR ALPHABLEND_PM( TSCOLOR dst, TSCOLOR src ) // premultiplied alpha blend
    {
        if ( PREMULTIPLIED( src ) )
            return ALPHABLEND_PM_NO_CLAMP( dst, src );

        extern uint8 ALIGN(256) multbl[ 256 ][ 256 ];

        uint8 not_a = 255 - ALPHA( src );

        uint B = multbl[ not_a ][ BLUE( dst ) ] + BLUE( src );
        uint G = multbl[ not_a ][ GREEN( dst ) ] + GREEN( src );
        uint R = multbl[ not_a ][ RED( dst ) ] + RED( src );
        uint A = multbl[ not_a ][ ALPHA( dst ) ] + ALPHA( src );

        return CLAMP<uint8>( B ) | ( CLAMP<uint8>( G ) << 8 ) | ( CLAMP<uint8>( R ) << 16 ) | ( A << 24 );
    }

    INLINE TSCOLOR ALPHABLEND_PM( TSCOLOR dst, TSCOLOR src, uint8 calpha ) // premultiplied alpha blend with addition constant alpha
    {
        if ( calpha == 0 ) return dst;
        return ALPHABLEND_PM( dst, MULTIPLY( src, ARGB( calpha, calpha, calpha, calpha ) ) );
    }

#pragma pack(push, 1)
    struct BITMAPINFOHEADER
    {
        uint32     biSize;
        long       biWidth;
        long       biHeight;
        uint16     biPlanes;
        uint16     biBitCount;
        uint32     biCompression;
        uint32     biSizeImage;
        long       biXPelsPerMeter;
        long       biYPelsPerMeter;
        uint32     biClrUsed;
        uint32     biClrImportant;
    };
#pragma pack(pop)

enum image_area_type_e
{
    IMAGE_AREA_SOLID = 1,
    IMAGE_AREA_TRANSPARENT = 2,
    IMAGE_AREA_SEMITRANSPARENT = 4,

    IMAGE_AREA_ALLTYPE = IMAGE_AREA_SOLID|IMAGE_AREA_TRANSPARENT|IMAGE_AREA_SEMITRANSPARENT,
};

enum rot_filter_e
{
	FILTMODE_POINT		= 0,
	FILTMODE_BILINEAR,
	FILTMODE_BICUBIC
};

enum resize_filter_e
{
	FILTER_NONE,
	FILTER_BILINEAR,
	FILTER_BICUBIC,
	FILTER_TABLEBILINEAR,
	FILTER_TABLEBICUBIC075,
	FILTER_TABLEBICUBIC060,
	FILTER_TABLEBICUBIC100,
	FILTER_LANCZOS3,
    FILTER_BOX_LANCZOS3,
};

enum yuv_fmt_e
{
    YFORMAT_YUY2, // MEDIASUBTYPE_YUY2 // MAKEFOURCC('Y', 'U', 'Y', '2'):
    YFORMAT_I420, // 3 planes yuy image from libvpx
    YFORMAT_I420x2, // same as YFORMAT_I420, but dimension of Y plane is /2 (shrink during conversion)
};

struct imgdesc_s
{
    ivec2 sz;
    int16 pitch; // pitch is signed and can be < 0
    uint8 bitpp;
    uint8 _dummy = 0;
    imgdesc_s() {}
    imgdesc_s( const imgdesc_s &inf, const ivec2&sz ) :sz(sz), pitch(inf.pitch), bitpp(inf.bitpp) {}
    imgdesc_s( const ivec2&sz, uint8 bitpp):sz(sz), pitch(as_word(((bitpp + 1) >> 3) * sz.x)), bitpp(bitpp) {}
    imgdesc_s( const ivec2&sz, uint8 bitpp, int16 pitch ):sz(sz), pitch(pitch), bitpp(bitpp) { ASSERT(tabs(pitch)>=(sz.x*bytepp())); }

    aint bytepp() const { return (bitpp + 1) >> 3; };
    bool operator==(const imgdesc_s&d) const { return sz == d.sz && /*pitch == d.pitch &&*/ bitpp == d.bitpp; }
    bool operator!=(const imgdesc_s&d) const { return sz != d.sz || /*pitch != d.pitch ||*/ bitpp != d.bitpp; }

    imgdesc_s &set_width(int w) { sz.x = w; return *this; }
    imgdesc_s &set_height(int h) { sz.y = h; return *this; }
    imgdesc_s &set_size(const ivec2 &szz) { sz = szz; return *this; }

    imgdesc_s chsize( const ivec2 &szz ) const {return imgdesc_s(szz, bitpp, pitch); }
};

template<> INLINE imgdesc_s &make_dummy<imgdesc_s>(bool quiet) { static imgdesc_s t(ts::ivec2(0),0,0); DUMMY_USED_WARNING(quiet); return t; }

void TSCALL img_helper_colorclamp(uint8 *des, const imgdesc_s &des_info); // fix components to match premultiplied
bool TSCALL img_helper_premultiply(uint8 *des, const imgdesc_s &des_info);
void TSCALL img_helper_mulcolor(uint8 *des, const imgdesc_s &des_info, TSCOLOR color);
void TSCALL img_helper_fill(uint8 *des, const imgdesc_s &des_info, TSCOLOR color);
void TSCALL img_helper_overfill(uint8 *des, const imgdesc_s &des_info, TSCOLOR color_pm);
void TSCALL img_helper_copy(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info);
void TSCALL img_helper_shrink_2x(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info);
void TSCALL img_helper_copy_components(uint8* des, const uint8* sou, const imgdesc_s &des_info, const imgdesc_s &sou_info, int num_comps );
void TSCALL img_helper_merge_with_alpha(uint8 *des, const uint8 *basesrc, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &base_info, const imgdesc_s &sou_info, int oalphao = -1);
void TSCALL img_helper_yuv2rgb(uint8 *des, const imgdesc_s &des_info, const uint8 *sou, yuv_fmt_e yuvfmt);
void TSCALL img_helper_rgb2yuv(uint8 *dst, const imgdesc_s &src_info, const uint8 *sou, yuv_fmt_e yuvfmt);
void TSCALL img_helper_alpha_blend_pm( uint8 *dst, int dst_pitch, const uint8 *sou, const imgdesc_s &src_info, uint8 alpha, bool guaranteed_premultiplied = true ); // only 32 bpp target and source
bool TSCALL img_helper_resize( const bmpcore_exbody_s &extbody, const uint8 *sou, const imgdesc_s &souinfo, resize_filter_e filt_mode );

// see convert.cpp
void TSCALL img_helper_i420_to_ARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height);
void TSCALL img_helper_ARGB_to_i420(const uint8* src_argb, int src_stride_argb, uint8* dst_y, int dst_stride_y, uint8* dst_u, int dst_stride_u, uint8* dst_v, int dst_stride_v, int width, int height);

struct bmpcore_normal_s;
template<typename CORE> class bitmap_t;
typedef bitmap_t<bmpcore_normal_s> bitmap_c;

struct bmpcore_normal_s
{
    MOVABLE( true );

    struct core_s
    {
        int         m_ref;
        imgdesc_s    m_info;

        core_s(aint sz)
        {
            m_ref = 1;
            m_info.sz.x = 0;
            m_info.sz.y = 0;
            m_info.pitch = 0;
            m_info.bitpp = 0;

            memset(this + 1, 0, sz);
        }


        void    ref_inc()
        {
            ++m_ref;
        }
        void    ref_dec()
        {
            if (m_ref == 1)
            {
                destroy();
            }
            else
                --m_ref;
        }


        core_s * reuse(aint bodysz)
        {
            if (m_ref == 1)
            {
                return (core_s *)MM_RESIZE(this, bodysz + sizeof(core_s));
            }
            else
            {
                --m_ref;
                return core_s::build(bodysz);
            }
        }
        void destroy()
        {
            this->~core_s();
            MM_FREE(this);
        }
        static  core_s *  build(aint sz = 0)
        {
            core_s *n = (core_s *)MM_ALLOC(sizeof(core_s) + sz);
            TSPLACENEW(n, sz);
            return n;
        }
    } *m_core;
    TS_STATIC_CHECK(sizeof(core_s) == 16, "sizeof(core_s) must be 16");

    explicit bmpcore_normal_s( const bmpcore_normal_s&oth ):m_core(oth.m_core)
    {
        if (m_core) m_core->ref_inc();
    }
    explicit bmpcore_normal_s(bmpcore_normal_s &&oth) :m_core(oth.m_core)
    {
        oth.m_core = nullptr;
    }

    explicit bmpcore_normal_s( aint sz = 0 )
    {
        m_core = sz ? core_s::build(sz) : nullptr;
    }
    ~bmpcore_normal_s()
    {
        if (m_core) m_core->ref_dec();
    }

    void before_modify(bitmap_c *me);

    void create( const imgdesc_s& info )
    {
        aint sz = info.sz.x * info.sz.y * info.bytepp();
        m_core = m_core ? m_core->reuse( sz ) : core_s::build(sz);
        m_core->m_info = info;
    }
    void clear()
    {
        if (m_core)
        {
            m_core->ref_dec();
            m_core = nullptr;
        }
    }

    const imgdesc_s &info() const {return m_core ? m_core->m_info : make_dummy<imgdesc_s>(true); }
    uint8 *operator()() const { return (uint8 *)(m_core+1);}

    bool operator==(const bmpcore_normal_s &ocore) const;
    bmpcore_normal_s &operator=(const bmpcore_normal_s &ocore)
    {
        if (m_core != ocore.m_core)
        {
            if (m_core) m_core->ref_dec();
            m_core = ocore.m_core;
            if (m_core) m_core->ref_inc();
        }
        return *this;
    }
    bmpcore_normal_s &operator=(bmpcore_normal_s &&ocore)
    {
        SWAP(m_core, ocore.m_core);
        return *this;
    }
};


struct bmpcore_exbody_s
{
    MOVABLE( true );
    DUMMY(bmpcore_exbody_s);
    const uint8 *m_body;
    imgdesc_s m_info;

    bmpcore_exbody_s(const bmpcore_exbody_s & oth):m_body(oth.m_body), m_info(oth.m_info) {}
    bmpcore_exbody_s(const uint8 *body, const imgdesc_s &info):m_body(body), m_info(info)
    {
    }
    bmpcore_exbody_s():m_body(nullptr) {}
    explicit bmpcore_exbody_s(aint) {} //-V730

    bmpcore_exbody_s(bmpcore_exbody_s &&ocore)
    {
        m_body = ocore.m_body;
        m_info = ocore.m_info;
        ocore.m_body = nullptr;
    }


    void before_modify(bitmap_t<bmpcore_exbody_s> *me) {}
    void clear()
    {
        ERROR( "Extbody images cannot be cleared" );
    }
    void create(const imgdesc_s &)
    {
        ERROR( "Extbody images cannot be created" );
    }

    const imgdesc_s &info() const { return m_info; }
    uint8 *operator()() const { return const_cast<uint8 *>(m_body); }
    uint8 *operator()(const ivec2& pos) const { return (*this)() + pos.x * info().bytepp() + pos.y * info().pitch; }

    bool operator==(const bmpcore_exbody_s &ocore) const;
    bmpcore_exbody_s &operator=(const bmpcore_exbody_s &ocore)
    {
        m_body = ocore.m_body;
        m_info = ocore.m_info;
        return *this;
    }
    bmpcore_exbody_s &operator=(bmpcore_exbody_s &&ocore)
    {
        m_body = ocore.m_body;
        m_info = ocore.m_info;
        return *this;
    }

    void draw(const bmpcore_exbody_s &eb, aint xx, aint yy, int alpha = -1 /* -1 means no alphablend used */) const;
    void draw(const bmpcore_exbody_s &eb, aint xx, aint yy, const irect &r, int alpha = -1 /* -1 means no alphablend used */) const;

};
template<typename CORE> class bitmap_t
{
    MOVABLE( is_movable<CORE>::value );
    DUMMY( bitmap_t );

public:
    typedef const uint8 *FMATRIX[3][3];

    class CF_BLUR
    {
    public:
        bool begin( const bitmap_t & tgt, const bitmap_t & src )
        {
            return src.info().bitpp == 32 && tgt.info().bitpp == 32; //-V112
        }

        void point( ts::uint8 * me, const FMATRIX &m )
        {
            int r = 0;
            int g = 0;
            int b = 0;
            int c = 0;
            for (int i=0;i<3;++i)
            {
                for (int j=0;j<3;++j)
                {
                    const ts::uint8 *df = m[i][j];
                    if (df)
                    {
                        ++c;
                        r += df[0];
                        g += df[1];
                        b += df[2];
                    }
                }
            }
            me[0] = as_byte( r / c );
            me[1] = as_byte( g / c );
            me[2] = as_byte( b / c );

        }
    };
/*
    template <unsigned long color = 0xFF000000, unsigned char alpha = 0> struct CF_ALPHA_EDGE
    {
        bool begin( const bitmap_c & tgt, const bitmap_c & src )
        {
            return src.bitPP() == 32 && tgt.bitPP() == 32;
        }

        void point( uint8 *des, const FMATRIX &src )
        {
            if (alpha >= *(src[1][1] + 3))
            {
                //zero alpha found
                if (
                    (src[0][0] && alpha < *(src[0][0]+3)) ||
                    (src[1][0] && alpha < *(src[1][0]+3)) ||
                    (src[2][0] && alpha < *(src[2][0]+3)) ||
                    (src[0][1] && alpha < *(src[0][1]+3)) ||
                    (src[2][1] && alpha < *(src[2][1]+3)) ||
                    //(src[0][0] && alpha < *(src[0][0]+3)) ||
                    (src[1][1] && alpha < *(src[1][1]+3)) ||
                    (src[2][2] && alpha < *(src[2][2]+3)) )
                {
                    *(uint32 *)des = color;
                    return;
                }
            }
            *(uint32 *)des = *(uint32 *)src[1][1];

        }
    };
*/

protected:

    CORE core;

    template<typename T1, typename T2> bitmap_t(const T1&t1, const T2&t2):core(t1,t2) {}

public:

    void before_modify()
    {
        core.before_modify(this);
    }

public:
    bitmap_t() {}
    explicit bitmap_t(const CORE &extcore):core(extcore) {}
    bitmap_t(const bitmap_t &bmp):core(bmp.core) {}
    bitmap_t(bitmap_t &&bmp):core(std::move(bmp.core)) {}
    ~bitmap_t() {}

	void clear()                    { core.clear(); }
    const imgdesc_s &info() const   { return core.info(); }
    imgdesc_s info(const ts::ivec2 &offset) const 
    {
        if (offset == ts::ivec2(0))
            return core.info();
        imgdesc_s inf( core.info() );
        inf.sz -= offset;
        return inf;
    }
    imgdesc_s info(const ts::irect &r) const
    {
        imgdesc_s inf(core.info());
        inf.sz = r.size();
        return inf;
    }

    operator const CORE&() const { return core; }

    uint8* body() const             { return core(); }
    uint8* body(const ivec2& pos) const { return core() + pos.x * core.info().bytepp() + pos.y * core.info().pitch; }

    bmpcore_exbody_s extbody() const { return bmpcore_exbody_s( body(), info() ); }
    bmpcore_exbody_s extbody( const ts::irect &r ) const
    {
        return bmpcore_exbody_s( body(r.lt), info(r) );
    }

    operator bool() const {return info().sz >> 0;}

    void operator =(const bitmap_t &bmp)
    {
        core = bmp.core;
    }
    void operator =(bitmap_t &&bmp)
    {
        core = std::move( bmp.core );
    }
    bitmap_t& operator =( const bmpcore_exbody_s &eb );

    bool equals(const bitmap_t & bm) const { return core == bm.core; }

    void convert_from_yuv( const ivec2 & pdes, const ivec2 & size, const uint8 *src, yuv_fmt_e fmt ); // src pitch must be equal to size.x * byte_per_pixrl of current yuv format
    void convert_to_yuv( const ivec2 & pdes, const ivec2 & size, buf_c &b, yuv_fmt_e fmt );

    /*
    void convert_32to16(bitmap_c &imgout) const;
    void convert_32to24(bitmap_c &imgout) const;
    */

	bitmap_t &create(const ivec2 &sz, uint8 bitpp) { core.create(imgdesc_s(sz, bitpp)); return *this; }
    bitmap_t &create_grayscale(const ivec2 &sz) { core.create(imgdesc_s(sz, 8)); return *this; }
	bitmap_t &create_16(const ivec2 &sz) { core.create(imgdesc_s(sz, 16)); return *this; }
	bitmap_t &create_15(const ivec2 &sz) { core.create(imgdesc_s(sz, 15)); return *this; }
	bitmap_t &create_RGB(const ivec2 &sz)  { core.create(imgdesc_s(sz, 24)); return *this; }
	bitmap_t &create_ARGB(const ivec2 &sz)  { core.create(imgdesc_s(sz, 32)); return *this; } //-V112

    bool	ajust_ARGB(const ivec2 &sz, bool exact_size)
    {
        ivec2 newbbsz((sz.x + 15) & (~15), (sz.y + 15) & (~15));
        ivec2 bbsz = info().sz;

        if ((exact_size && newbbsz != bbsz) || sz > bbsz)
        {
            if (newbbsz.x < bbsz.x) newbbsz.x = bbsz.x;
            if (newbbsz.y < bbsz.y) newbbsz.y = bbsz.y;
            create_ARGB(newbbsz);
            return true;
        }
        return false;
    }

    void gen_qrcore( int margins, int dotsize, const asptr& utf8string, int bitpp = 32, TSCOLOR fg_color = ARGB<uint8>(0,0,0), TSCOLOR bg_color = ARGB<uint8>(255,255,255) );

    void copy_components(bitmap_c &imageout, int num_comps, int dst_first_comp, int src_first_comp) const;

	void copy(const ivec2 & pdes,const ivec2 & size, const bmpcore_exbody_s & bmsou, const ivec2 & spsou);
    void tile(const ivec2 & pdes,const ivec2 & desize, const bmpcore_exbody_s & bmsou,const ivec2 & spsou, const ivec2 & szsou);
	void fill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color);
    void fill(TSCOLOR color);
    void fill_alpha(const ivec2 & pdes,const ivec2 & size, uint8 a);
    void fill_alpha(uint8 a);

    void overfill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color); // draws rectangle with premultiplied color

    bool premultiply();
    bool premultiply( const irect &rect );
    void colorclamp();
    void unmultiply(); // restore color from premultiplied

    void invcolor();
    void mulcolor(const ivec2 & pdes, const ivec2 & size, TSCOLOR color);

    void detect_alpha_channel( const bitmap_t & bmsou ); // me - black background, bmsou - white background

    void render_cursor( const ivec2&pos, buf_c &cache ); // render current cursor to bitmap; PLATFORM SPECIFIC!!!

    template <typename FILTER, typename CORE2> void copy_with_filter(bitmap_t<CORE2>& outimage, FILTER &f) const
    {
        if (!ASSERT(outimage.info().sz == info().sz)) return;
        if (!f.begin( outimage, *this )) return;
        outimage.before_modify();

        uint8 * des = outimage.body();
        const uint8 * sou = body();

        FMATRIX mat;

        const imgdesc_s& __inf = info();
        const imgdesc_s& __inf_o = outimage.info();

        int szy = __inf.sz.y;
        for(int y=0;y<szy;y++,des+= __inf_o.pitch,sou+=info().pitch)
        {
            bool up = y == 0;
            bool down = y == info().sz.y - 1;

            uint8 * des1 = des;
            const uint8 * sou1 = sou;
            for (int x = 0; x<info().sz.x; ++x, sou1 += __inf.bytepp(), des1 += __inf_o.bytepp())
            {
                bool left = x == 0;
                bool rite = x == info().sz.x - 1;

                mat[0][0] = (left || up) ? nullptr : (sou1 - __inf.pitch - __inf.bytepp());
                mat[1][0] = (up) ? nullptr : (sou1 - info().pitch);
                mat[2][0] = (rite || up) ? nullptr : (sou1 - __inf.pitch + __inf.bytepp());

                mat[0][1] = (left) ? nullptr : (sou1 - __inf.bytepp());
                mat[1][1] = sou1;
                mat[2][1] = (rite) ? nullptr : (sou1 + __inf.bytepp());

                mat[0][2] = (left || down) ? nullptr : (sou1 + __inf.pitch - __inf.bytepp());
                mat[1][2] = (down) ? nullptr : (sou1 + info().pitch);
                mat[2][2] = (rite || down) ? nullptr : (sou1 + __inf.pitch + __inf.bytepp());

                f.point( des1, mat );
            }
        }

    }

    template <typename FILTER> void apply_filter(const ts::ivec2 &pdes, const ts::ivec2 &size, const FILTER &f)
    {
        if (pdes.x < 0 || pdes.y < 0) return;
        if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

        before_modify();

        const imgdesc_s& __inf = info();

        uint8 * bu = body() + __inf.bytepp()*pdes.x + __inf.pitch*pdes.y;
        aint desnl = __inf.pitch - size.x*info().bytepp();
        aint desnp = __inf.bytepp();

        FMATRIX mat;

        bool blef = pdes.x == 0;
        bool btop = pdes.y == 0;

        bool brit = __inf.sz.x == (size.x + pdes.x);
        bool bbot = __inf.sz.y == (size.y + pdes.y);

		for(aint y=0;y<size.y;y++,bu+=desnl)
        {
            bool up = btop && y == 0;
            bool down = bbot && y == info().sz.y - 1;

            for (int x = 0; x < size.x; x++, bu += desnp)
            {
                bool left = blef && x == 0;
                bool rite = brit && x == info().sz.x - 1;

                mat[0][0] = (left || up) ? nullptr : (bu - __inf.pitch - __inf.bytepp());
                mat[1][0] = (up) ? nullptr : (bu - info().pitch);
                mat[2][0] = (rite || up) ? nullptr : (bu - __inf.pitch + __inf.bytepp());

                mat[0][1] = (left) ? nullptr : (bu - __inf.bytepp());
                mat[1][1] = bu;
                mat[2][1] = (rite) ? nullptr : (bu + __inf.bytepp());

                mat[0][2] = (left || down) ? nullptr : (bu + __inf.pitch - __inf.bytepp());
                mat[1][2] = (down) ? nullptr : (bu + info().pitch);
                mat[2][2] = (rite || down) ? nullptr : (bu + __inf.pitch + __inf.bytepp());

                f(bu, mat);
            }
        }
    }

    template <typename FILTER> void process_pixels(const ts::ivec2 &pdes, const ts::ivec2 &size, const FILTER &f)
    {
        if (pdes.x < 0 || pdes.y < 0) return;
        if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

        before_modify();

        uint8 * bu = body() + info().bytepp()*pdes.x + info().pitch*pdes.y;
        aint desnl = info().pitch - size.x*info().bytepp();
        aint desnp = info().bytepp();

        for (aint y = 0; y < size.y; ++y, bu += desnl)
            for (int x = 0; x < size.x; ++x, bu += desnp)
                f( *(TSCOLOR *)bu );
    }


    void make_grayscale();

    void sharpen(bitmap_c &outimage, int lv) const; // lv [0..64]

    bool resize_to(bitmap_c& outimage, const ivec2 & newsize, resize_filter_e filt_mode = FILTER_NONE) const;
    bool resize_to(const bmpcore_exbody_s &eb, resize_filter_e filt_mode = FILTER_NONE) const;
    bool resize_from(const bmpcore_exbody_s &eb, resize_filter_e filt_mode = FILTER_NONE) const;
    void shrink_2x_to(const ivec2 &lt_source, const ivec2 &sz_source, const bmpcore_exbody_s &eb_target) const;

	void flip_x(const ivec2 & pdes,const ivec2 & size, const bitmap_t & bmsou,const ivec2 & spsou);
	void flip_y(const ivec2 & pdes,const ivec2 & size, const bitmap_t & bmsou,const ivec2 & spsou);
	void flip_y(const ivec2 & pdes,const ivec2 & size);
    void flip_y();

    void alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img );
    void alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img, const bmpcore_exbody_s & base ); // this - target, result size - base size

	void swap_byte(const ivec2 & pos,const ivec2 & size,int n1,int n2);

    TSCOLOR get_argb_lerp(float x, float y) const // get interpolated ARGB for specified coordinate (0.0 - 1.0)
                                    // 0.0,0.0 - center of left-top pixel
                                    // 1.0,1.0 - center of right-bottom pixel
    {
        float xf = x * (info().sz.x - 1) + 0.5f;
        aint x0 = TruncFloat(xf);
        aint x1 = x0 + 1;
        float yf = y * (info().sz.y - 1) + 0.5f;
        aint y0 = TruncFloat(yf);
        aint y1 = y0 + 1;

        TSCOLOR cl = LERPCOLOR( get_argb(x0, y0), get_argb(x0, y1), yf - y0);
        TSCOLOR cr = LERPCOLOR( get_argb(x1, y0), get_argb(x1, y1), yf - y0);
        return LERPCOLOR(cl, cr, xf - x0);
    }

    TSCOLOR get_argb(aint x, aint y) const // get ARGB color of specified pixel
    {

        uint32 c;
        c = *(uint32 *)(body() + (y * info().pitch + x * info().bytepp()));
        uint32 mask = 0xFF;
        if (info().bytepp() == 2) mask = 0x0000FFFF;
        else
        if (info().bytepp() == 3) mask = 0x00FFFFFF;
        else
        if (info().bytepp() == 4) mask = 0xFFFFFFFF; //-V112
        return c & mask;
    }

	void set_argb(aint x, aint y, TSCOLOR color) // set ARGB color of specified pixel
    {
        before_modify();
		*(TSCOLOR *)(body() + (y * info().pitch + x * info().bytepp())) = color;
    }
	void set_argb(aint x, aint y, TSCOLOR color, int alpha) // alpha blend ARGB color of specified pixel (like photoshop Normal mode)
    {
        before_modify();
		TSCOLOR * c = (TSCOLOR *)(body() + (y * info().pitch + x * info().bytepp()));
        *c = ALPHABLEND( *c, color, alpha );
    }

    irect calc_visible_rect() const;

    uint32  get_area_type(const irect& r) const;
    uint32  get_area_type() const { return get_area_type(irect(0, info().sz)); }
    bool    is_alpha_blend(const irect& r) const
    {
        return 0 != (get_area_type(r) & (ts::IMAGE_AREA_TRANSPARENT|ts::IMAGE_AREA_SEMITRANSPARENT));
    }
    bool    is_alpha_blend() const
    {
        return 0 != (get_area_type() & (ts::IMAGE_AREA_TRANSPARENT | ts::IMAGE_AREA_SEMITRANSPARENT));
    }
    
    uint32 hash() const
    {
        uint32 crc = 0xFFFFFFFF; //-V112
        const uint8 *s = body();
        for(int y=0;y<info().sz.y;++y, s+=info().pitch)
            crc = CRC32_Buf(s, info().pitch, crc);
        return CRC32_End(crc);
    }

    bool load_from_BMPHEADER(const BITMAPINFOHEADER * iH, int buflen);

	img_format_e load_from_file(const void * buf, aint buflen);
	img_format_e load_from_file(const buf_c & buf)
    {
        return load_from_file(buf.data(), buf.size());
    }
    img_format_e load_from_file(const blob_c & buf)
    {
        return load_from_file(buf.data(), buf.size());
    }

	img_format_e load_from_file(const wsptr &filename);

#define FMT(fn) bool save_as_##fn(buf_c &buf, int options = DEFAULT_SAVE_OPT(fn)) const { return save_to_##fn##_format(buf, extbody(), options); }
    IMGFORMATS
#undef FMT

#define FMT(fn) bool save_as_##fn(const wsptr &filename, int options = DEFAULT_SAVE_OPT(fn)) const { buf_c b; if (save_to_##fn##_format(b, extbody(), options)) { b.save_to_file(filename); return true;} return false; }
    IMGFORMATS
#undef FMT


    void draw(const bmpcore_exbody_s &eb, aint xx, aint yy, int alpha = -1 /* -1 means no alphablend used */) const
    {
        extbody().draw(eb,xx,yy,alpha);
    }
    void draw(const bmpcore_exbody_s &eb, aint xx, aint yy, const irect &r, int alpha = -1 /* -1 means no alphablend used */) const
    {
        extbody().draw(eb,xx,yy,r,alpha);
    }

};

class image_extbody_c : public bitmap_t < bmpcore_exbody_s >
{
public:
    image_extbody_c(): bitmap_t(nullptr, imgdesc_s(ivec2(0), 0)) {}
    image_extbody_c( const uint8 *imgbody, const imgdesc_s &info ): bitmap_t(imgbody, info) {}
};

class drawable_bitmap_c : public image_extbody_c
{
    DUMMY(drawable_bitmap_c);

#ifdef _WIN32
    static const int datasize = 16;
#elif defined __linux__
    static const int datasize = 16;
#else
    unknown
#endif

public:

    uint8 data[ datasize ];

    void clear();

    bool	ajust( const ivec2 &sz, bool exact_size );

    bool    create_from_bitmap(const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy = false, bool premultiply = false, bool detect_alpha_pixels = false);
    bool    create_from_bitmap(const bitmap_c &bmp, bool flipy = false, bool premultiply = false, bool detect_alpha_pixels = false);
    void    save_to_bitmap(bitmap_c &bmp, bool save16as32 = false);
    void    save_to_bitmap(bitmap_c &bmp, const ivec2 & pos_from_dc);
    void    create(const ivec2 &sz, int monitor = -1);


    drawable_bitmap_c(const drawable_bitmap_c &) UNUSED;
    drawable_bitmap_c( drawable_bitmap_c && ob );

    drawable_bitmap_c() : image_extbody_c(nullptr, imgdesc_s(ivec2(0), 0)) {}
    explicit drawable_bitmap_c(const ivec2 &sz) : image_extbody_c(nullptr, imgdesc_s(ivec2(0), 0)) { create(sz); }
    explicit drawable_bitmap_c(const bitmap_c &bmp, bool flipy = false, bool premultiply = false) { create_from_bitmap(bmp, flipy, premultiply, false); }

    ~drawable_bitmap_c() { clear(); }

    drawable_bitmap_c &operator=( drawable_bitmap_c && ob );
    drawable_bitmap_c &operator=(const drawable_bitmap_c & ) UNUSED;

    void grab_screen( const ts::irect &screenr, const ts::ivec2& p_in_bitmap );
};

void render_image( const bmpcore_exbody_s &tgt, const bmpcore_exbody_s &image, aint x, aint y, const irect &cliprect, int alpha );
void render_image( const bmpcore_exbody_s &tgt, const bmpcore_exbody_s &image, aint x, aint y, const irect &imgrect, const irect &cliprect, int alpha );

struct repdraw //-V690
{
    const ts::bmpcore_exbody_s &tgt;
    const ts::bmpcore_exbody_s &image;
    const ts::irect &cliprect;
    const ts::irect *rbeg;
    const ts::irect *rrep;
    const ts::irect *rend;
    int alpha;
    bool a_beg, a_rep, a_end;
    void operator=( const repdraw & ) UNUSED;
    repdraw( const ts::bmpcore_exbody_s &tgt, const ts::bmpcore_exbody_s &image, const ts::irect &cliprect, int alpha ) :tgt( tgt ), image( image ), cliprect( cliprect ), alpha( alpha ) {}

    void draw_h( ts::aint x1, ts::aint x2, ts::aint y, bool tile );
    void draw_v( ts::aint x, ts::aint y1, ts::aint y2, bool tile );
    void draw_c( ts::aint x1, ts::aint x2, ts::aint y1, ts::aint y2, bool tile );
};



} // namespace ts
