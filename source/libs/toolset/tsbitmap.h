#pragma once

#include "internal/imageformat.h"

namespace ts
{

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
	FILTER_NONE				= 0,
	FILTER_BILINEAR			= 1,
	FILTER_BICUBIC			= 2,
	FILTER_TABLEBILINEAR	= 3,
	FILTER_TABLEBICUBIC075	= 4,
	FILTER_TABLEBICUBIC060	= 5,
	FILTER_TABLEBICUBIC100	= 6,
	FILTER_LANCZOS3			= 7
};

struct imgdesc_s
{
    ivec2 sz;
    uint16 pitch;
    uint8 bitpp;
    uint8 _dummy;
    imgdesc_s() {}
    imgdesc_s( const ivec2&sz, uint8 bitpp):sz(sz), pitch(as_word(((bitpp + 1) >> 3) * sz.x)), bitpp(bitpp) {}
    imgdesc_s( const ivec2&sz, uint8 bitpp, uint16 pitch ):sz(sz), pitch(pitch), bitpp(bitpp) { ASSERT(pitch>=(sz.x*bytepp())); }

    aint bytepp() const { return (bitpp + 1) >> 3; };
    bool operator==(const imgdesc_s&d) const { return sz == d.sz && /*pitch == d.pitch &&*/ bitpp == d.bitpp; }
    bool operator!=(const imgdesc_s&d) const { return sz != d.sz || /*pitch != d.pitch ||*/ bitpp != d.bitpp; }

    imgdesc_s &set_width(int w) { sz.x = w; return *this; }
    imgdesc_s &set_height(int h) { sz.y = h; return *this; }
    imgdesc_s &set_size(const ivec2 &szz) { sz = szz; return *this; }
};

void TSCALL img_helper_copy(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info);
void TSCALL img_helper_make_2x_smaller(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info);
void TSCALL img_helper_copy_components(uint8* des, const uint8* sou, const imgdesc_s &des_info, const imgdesc_s &sou_info, int num_comps );
void TSCALL img_helper_merge_with_alpha(uint8 *des, const uint8 *basesrc, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &base_info, const imgdesc_s &sou_info, int oalphao = -1);

struct bmpcore_normal_s;
template<typename CORE> class bitmap_t;
typedef bitmap_t<bmpcore_normal_s> bitmap_c;

struct bmpcore_normal_s
{
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


        void    ref_inc(void)
        {
            ++m_ref;
        }
        void    ref_dec(void)
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

    bmpcore_normal_s( const bmpcore_normal_s&oth ):m_core(oth.m_core)
    {
        m_core->ref_inc();
    }

    bmpcore_normal_s( aint sz )
    {
        m_core = core_s::build(sz);
    }
    ~bmpcore_normal_s()
    {
        m_core->ref_dec();
    }

    void before_modify(bitmap_c *me);

    void create( const imgdesc_s& info )
    {
        m_core = m_core->reuse( info.sz.x * info.sz.y * info.bytepp() );
        m_core->m_info = info;
    }
    void clear()
    {
        m_core->ref_dec();
        m_core = core_s::build(0);
    }

    const imgdesc_s &info() const {return m_core->m_info; }
    uint8 *operator()() const { return (uint8 *)(m_core+1);}

    bool operator==(const bmpcore_normal_s &ocore) const;
    bmpcore_normal_s &operator=(const bmpcore_normal_s &ocore)
    {
        if (m_core != ocore.m_core)
        {
            m_core->ref_dec();
            m_core = ocore.m_core;
            m_core->ref_inc();
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
    const uint8 *m_body;
    imgdesc_s m_info;

    bmpcore_exbody_s(const bmpcore_exbody_s & oth):m_body(oth.m_body), m_info(oth.m_info) {}
    bmpcore_exbody_s(const uint8 *body, const imgdesc_s &info):m_body(body), m_info(info)
    {
    }
    bmpcore_exbody_s():m_body(nullptr) {}
    bmpcore_exbody_s(aint) {}

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
};
template<typename CORE> class bitmap_t
{

public:
    typedef const uint8 *FMATRIX[3][3];

    class CF_BLUR
    {
    public:
        bool begin( const bitmap_t & tgt, const bitmap_t & src )
        {
            return src.info().bitpp == 32 && tgt.info().bitpp == 32;
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
    bitmap_t():core(0) {}
    bitmap_t(const bitmap_t &bmp):core(bmp.core) {}
    ~bitmap_t() {}

	void clear(void)                { core.clear(); }
    const imgdesc_s &info() const   { return core.info(); }
    imgdesc_s info(const ts::ivec2 &offset) const 
    {
        if (offset == ts::ivec2(0))
            return core.info();
        imgdesc_s inf( core.info() );
        inf.sz -= offset;
        return inf;
    }
    uint8* body() const             { return core(); }
    uint8* body(const ivec2& pos) const { return core() + pos.x * core.info().bytepp() + pos.y * core.info().pitch; }

    bmpcore_exbody_s extbody() const { return bmpcore_exbody_s( body(), info() ); }

    void operator =(const bitmap_t &bmp)
    {
        core = bmp.core;
    }
    void operator =(bitmap_t &&bmp)
    {
        core = std::move( bmp.core );
    }

    bool equals(const bitmap_t & bm) const { return core == bm.core; }

    void convert_24to32(bitmap_c &imgout) const;
    /*
    void convert_32to16(bitmap_c &imgout) const;
    void convert_32to24(bitmap_c &imgout) const;
    */

	bitmap_t &create(const ivec2 &sz, uint8 bitpp) { core.create(imgdesc_s(sz, bitpp)); return *this; }
    bitmap_t &create_grayscale(const ivec2 &sz) { core.create(imgdesc_s(sz, 8)); return *this; }
	bitmap_t &create_16(const ivec2 &sz) { core.create(imgdesc_s(sz, 16)); return *this; }
	bitmap_t &create_15(const ivec2 &sz) { core.create(imgdesc_s(sz, 15)); return *this; }
	bitmap_t &create_RGB(const ivec2 &sz)  { core.create(imgdesc_s(sz, 24)); return *this; }
	bitmap_t &create_RGBA(const ivec2 &sz)  { core.create(imgdesc_s(sz, 32)); return *this; }

    void copy_components(bitmap_c &imageout, int num_comps, int dst_first_comp, int src_first_comp) const;//копирует только часть компонент (R|G|B|A) из bmsou

	void copy(const ivec2 & pdes,const ivec2 & size, const bmpcore_exbody_s & bmsou, const ivec2 & spsou);
    void tile(const ivec2 & pdes,const ivec2 & desize, const bmpcore_exbody_s & bmsou,const ivec2 & spsou, const ivec2 & szsou);
	void fill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color);
    void fill(TSCOLOR color);
    void fill_alpha(const ivec2 & pdes,const ivec2 & size, uint8 a);
    void fill_alpha(uint8 a);

    bool has_alpha() const;
    bool has_alpha(const ivec2 & pdes,const ivec2 & size) const;

    void overfill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color); // draws rectangle with premultiplied color

    void premultiply();
    void detect_alpha_channel( const bitmap_t & bmsou ); // me - black background, bmsou - white background

    template <typename FILTER, typename CORE2> void copy_with_filter(bitmap_t<CORE2>& outimage, FILTER &f) const
    {
        if (!ASSERT(outimage.info().sz == info().sz)) return;
        if (!f.begin( outimage, *this )) return;
        outimage.before_modify();

        uint8 * des = outimage.body();
        const uint8 * sou = body();

        FMATRIX mat;

        for(int y=0;y<info().sz.y;y++,des+=outimage.info().pitch,sou+=info().pitch)
        {
            bool up = y == 0;
            bool down = y == info().sz.y - 1;

            uint8 * des1 = des;
            const uint8 * sou1 = sou;
            for (int x = 0; x<info().sz.x; ++x, sou1 += info().bytepp(), des1 += outimage.info().bytepp())
            {
                bool left = x == 0;
                bool rite = x == info().sz.x - 1;

                mat[0][0] = (left || up) ? nullptr : (sou1 - info().pitch - info().bytepp());
                mat[1][0] = (up) ? nullptr : (sou1 - info().pitch);
                mat[2][0] = (rite || up) ? nullptr : (sou1 - info().pitch + info().bytepp());

                mat[0][1] = (left) ? nullptr : (sou1 - info().bytepp());
                mat[1][1] = sou1;
                mat[2][1] = (rite) ? nullptr : (sou1 + info().bytepp());

                mat[0][2] = (left || down) ? nullptr : (sou1 + info().pitch - info().bytepp());
                mat[1][2] = (down) ? nullptr : (sou1 + info().pitch);
                mat[2][2] = (rite || down) ? nullptr : (sou1 + info().pitch + info().bytepp());

                f.point( des1, mat );
            }
        }

    }

    template <typename FILTER> void apply_filter(const ts::ivec2 &pdes, const ts::ivec2 &size, FILTER &f)
    {
        if (pdes.x < 0 || pdes.y < 0) return;
        if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

        before_modify();

        uint8 * bu = body() + info().bytepp()*pdes.x + info().pitch*pdes.y;
        int desnl = info().pitch - size.x*info().bytepp();
        int desnp = info().bytepp();

        FMATRIX mat;

        bool blef = pdes.x == 0;
        bool btop = pdes.y == 0;

        bool brit = info().sz.x == (size.x + pdes.x);
        bool bbot = info().sz.y == (size.y + pdes.y);

		for(int y=0;y<size.y;y++,bu+=desnl)
        {
            bool up = btop && y == 0;
            bool down = bbot && y == info().sz.y - 1;

            for (int x = 0; x < size.x; x++, bu += desnp)
            {
                bool left = blef && x == 0;
                bool rite = brit && x == info().sz.x - 1;

                mat[0][0] = (left || up) ? nullptr : (bu - info().pitch - info().bytepp());
                mat[1][0] = (up) ? nullptr : (bu - info().pitch);
                mat[2][0] = (rite || up) ? nullptr : (bu - info().pitch + info().bytepp());

                mat[0][1] = (left) ? nullptr : (bu - info().bytepp());
                mat[1][1] = bu;
                mat[2][1] = (rite) ? nullptr : (bu + info().bytepp());

                mat[0][2] = (left || down) ? nullptr : (bu + info().pitch - info().bytepp());
                mat[1][2] = (down) ? nullptr : (bu + info().pitch);
                mat[2][2] = (rite || down) ? nullptr : (bu + info().pitch + info().bytepp());

                f.point(bu, mat);
            }
        }

    }

    void make_grayscale(void);

    void sharpen(bitmap_c &outimage, int lv) const; // lv [0..64]

    bool resize(bitmap_c& outimage, const ivec2 & newsize, resize_filter_e filt_mode = FILTER_NONE) const;
    /*
	bool resize(bitmap_c& outimage, float scale=2.f, resize_filter_e filt_mode=FILTER_NONE) const;
	bool rotate(bitmap_c& outimage, float angle_rad, rot_filter_e filt_mode=FILTMODE_POINT, bool expand_dst=true) const;
	
	void rotate_90(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou,const bitmap_t & bmsou,const ivec2 & spsou);
	void rotate_180(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou,const bitmap_t & bmsou,const ivec2 & spsou);
	void rotate_270(bitmap_c& outimage, const ivec2 & pdes,const ivec2 & sizesou,const bitmap_t & bmsou,const ivec2 & spsou);
    void make_larger(bitmap_c& outimage, int factor) const;
    void make_2x_smaller(bitmap_c& outimage, const ivec2 &lt, const ivec2 &sz) const;


    void crop(const ivec2 & lt, const ivec2 &sz);
    void crop_to_square(void);
    */

    uint32 get_area_type(const ivec2 & p,const ivec2 & size) const;

	void flip_x(const ivec2 & pdes,const ivec2 & size, const bitmap_t & bmsou,const ivec2 & spsou);
	void flip_y(const ivec2 & pdes,const ivec2 & size, const bitmap_t & bmsou,const ivec2 & spsou);
	void flip_y(const ivec2 & pdes,const ivec2 & size);
    void flip_y(void);

    void alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img );
    void alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img, const bmpcore_exbody_s & base ); // this - target, result size - base size

	void swap_byte(const ivec2 & pos,const ivec2 & size,int n1,int n2);
    void swap_byte(void *target) const;

    /*
	void merge_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou);
	void merge_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_c & bmsou,const ivec2 & spsou); // PM - premultiplied alpha

    void merge_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t * data_src,const ivec2 & data_src_point, const bitmap_t * alpha_src, const ivec2 & alpha_src_point);
    void merge_grayscale_with_alpha_PM(const ivec2 & pdes,const ivec2 & size, const bitmap_t * data_src,const ivec2 & data_src_point, const bitmap_t * alpha_src, const ivec2 & alpha_src_point);
    void modulate_grayscale_with_alpha(const ivec2 & pdes,const ivec2 & size, const bitmap_t * alpha_src, const ivec2 & alpha_src_point);

    */

    uint32 ARGB(float x, float y) const // get interpolated ARGB for specified coordinate (0.0 - 1.0)
                                    // 0.0,0.0 - center of left-top pixel
                                    // 1.0,1.0 - center of right-bottom pixel
    {
        float xf = x * (info().sz.x - 1) + 0.5f;
        int x0 = TruncFloat(xf);
        int x1 = x0 + 1;
        float yf = y * (info().sz.y - 1) + 0.5f;
        int y0 = TruncFloat(yf);
        int y1 = y0 + 1;

        TSCOLOR cl = LERPCOLOR(ARGBPixel(x0, y0), ARGBPixel(x0, y1), yf - y0);
        TSCOLOR cr = LERPCOLOR(ARGBPixel(x1, y0), ARGBPixel(x1, y1), yf - y0);
        return LERPCOLOR(cl, cr, xf - x0);
    }

    uint32 ARGBPixel(int x, int y) const // get ARGB color of specified pixel
    {

        uint32 c;
        c = *(uint32 *)(body() + (y * info().pitch + x * info().bytepp()));
        uint32 mask = 0xFF;
        if (info().bytepp() == 2) mask = 0x0000FFFF;
        else
        if (info().bytepp() == 3) mask = 0x00FFFFFF;
        else
        if (info().bytepp() == 4) mask = 0xFFFFFFFF;
        return c & mask;
    }

	void ARGBPixel(int x, int y, TSCOLOR color) // set ARGB color of specified pixel
    {
        before_modify();
		*(TSCOLOR *)(body() + (y * info().pitch + x * info().bytepp())) = color;
    }
	void ARGBPixel(int x, int y, TSCOLOR color, int alpha) // alpha blend ARGB color of specified pixel (like photoshop Normal mode)
    {
        before_modify();
		TSCOLOR * c = (TSCOLOR *)(body() + (y * info().pitch + x * info().bytepp()));
        *c = ALPHABLEND( *c, color, alpha );
    }

    irect calc_visible_rect() const;

    uint32 hash(void) const
    {
        uint32 crc = 0xFFFFFFFF;
        const uint8 *s = body();
        for(int y=0;y<info().sz.y;++y, s+=info().pitch)
            crc = CRC32_Buf(s, info().pitch, crc);
        return CRC32_End(crc);
    }

    void load_from_HWND(HWND hwnd);
    bool load_from_BMPHEADER(const BITMAPINFOHEADER * iH, int buflen);

	img_format_e load_from_file(const void * buf, int buflen);
	img_format_e load_from_file(const buf_c & buf);
	img_format_e load_from_file(const wsptr &filename);

#define FMT(fn) bool save_as_##fn(buf_c &buf, int options = DEFAULT_SAVE_OPT(fn)) { return save_to_##fn##_format(buf, extbody(), options); }
    IMGFORMATS
#undef FMT

#define FMT(fn) bool save_as_##fn(const wsptr &filename, int options = DEFAULT_SAVE_OPT(fn)) { buf_c b; if (save_to_##fn##_format(b, extbody(), options)) { b.save_to_file(filename); return true;} return false; }
    IMGFORMATS
#undef FMT


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

    HBITMAP m_memBitmap;		// Windows-кий bitmap
    HDC     m_memDC;		    // Context windows-кого bitmap-а

public:

    void clear()
    {
        if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
        if (m_memBitmap) { DeleteObject(m_memBitmap); m_memBitmap = nullptr; }
        core.m_body = nullptr;
        memset( &core.m_info, 0, sizeof(imgdesc_s) );

    }

    bool	ajust(const ivec2 &sz, bool exact_size)
    {
        ivec2 newbbsz((sz.x + 15) & (~15), (sz.y + 15) & (~15));
        ivec2 bbsz = info().sz;

        if ((exact_size && newbbsz != bbsz) || sz > bbsz)
        {
            create(newbbsz);
            return true;
        }
        return false;
    }

    bool    is_alphablend(const irect &r) const;
    bool    create_from_bitmap(const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy = false, bool premultiply = false, bool detect_alpha_pixels = false);
    bool    create_from_bitmap(const bitmap_c &bmp, bool flipy = false, bool premultiply = false, bool detect_alpha_pixels = false);
    void    save_to_bitmap(bitmap_c &bmp, bool save16as32 = false);
    void    save_to_bitmap(bitmap_c &bmp, const ivec2 & pos_from_dc);
    void    create(const ivec2 &sz);

    HBITMAP bitmap(void)	const  { return m_memBitmap; }
    HDC     DC(void)	const  { return m_memDC; }

    drawable_bitmap_c() :m_memBitmap(nullptr), m_memDC(nullptr), image_extbody_c(nullptr, imgdesc_s(ivec2(0), 0)) {}
    explicit drawable_bitmap_c(const ivec2 &sz):m_memBitmap(nullptr), m_memDC(nullptr), image_extbody_c(nullptr, imgdesc_s(ivec2(0), 0)) { create(sz); }

    ~drawable_bitmap_c(void) { clear(); }

    template <class W> void work(W & w)
    {
        if (body() == nullptr) return;
        BITMAP bmpInfo;
        GetObject(m_memBitmap, sizeof(BITMAP), &bmpInfo);

        for (int j = 0; j < bmpInfo.bmHeight; ++j)
        {
            uint32 *x = (uint32 *)((BYTE *)body() + j * bmpInfo.bmWidthBytes);
            for (int i = 0; i < bmpInfo.bmWidth; ++i)
            {
                x[i] = w(i, j, x[i]);
            }
        }
    }

    void draw(HDC dc, int xx, int yy, int alpha = -1 /* -1 means no alphablend used */) const;
    void draw(HDC dc, int xx, int yy, const irect &r, int alpha = -1 /* -1 means no alphablend used */ ) const;
};


} // namespace ts
