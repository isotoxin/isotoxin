#pragma once

namespace ts
{

#define IMGFORMATS \
    FMT( png ) \
    FMT( jpg ) \
    FMT( gif ) \
    FMT( dds ) \
    FMT( bmp ) \
    FMT( tga ) \


enum img_format_e
{
    if_none,
#define FMT(fn) if_##fn,
    IMGFORMATS
#undef FMT
    if_total
};

struct img_reader_s;
typedef fastdelegate::FastDelegate<bool (img_reader_s &, void *, int)> image_read_func;

class shrink_on_read_c
{
    ivec2 origsize;
    ivec2 shrinksize;
    int shrink_exp;
    int shrink_n;
public:
    shrink_on_read_c( const ivec2 &origsize, int shrink_exp ) :origsize( origsize ), shrink_exp( shrink_exp )
    {
        shrinksize.x = origsize.x >> shrink_exp;
        shrinksize.y = origsize.y >> shrink_exp;
        shrink_n = 1 << shrink_exp;
    }

    template<typename GETROW> void do_job( void * buf, int pitch, int components, const GETROW &grow )
    {
        uint8 * tbuf = (uint8 *)buf;
        if ( shrink_exp == 0 )
        {
            for ( int i = 0; i < origsize.y; ++i, tbuf += pitch )
                if ( grow( i, tbuf ) )
                    break;
        }
        else
        {
            tbuf_t<uint> shrinkrow;
            buf_c readrow;
            shrinkrow.set_count( shrinksize.x * components );
            readrow.set_size( origsize.x * components );

            int passe = shrink_n * components;
            int divsq = shrink_n * shrink_n;

            for ( int y = 0, ye = shrinksize.y * shrink_n; y < ye; )
            {
                memset( shrinkrow.data(), 0, shrinkrow.byte_size() );

                for ( int pass = 0; pass < shrink_n; ++pass )
                {
                    if ( grow( y, readrow.data() ) )
                        return;

                    const uint8 *src = readrow.data();
                    const uint8 *src_e = src + shrinksize.x * shrink_n * components;

                    for ( int rx = 0; src < src_e; rx += components )
                    {
                        ASSERT( rx < shrinksize.x * components );

                        for ( int passx = 0, c = 0; passx < passe; ++passx )
                        {
                            shrinkrow.begin()[ rx + c ] += *src;
                            ++c; if ( c >= components ) c = 0;
                            ++src;
                        }
                    }
                    ++y;
                }

                const uint *src = shrinkrow.begin();
                for ( uint8 *dst = tbuf, *dst_e = tbuf + shrinksize.x * components; dst < dst_e; ++dst, ++src )
                    *dst = as_byte( *src / divsq );

                tbuf += pitch;
            }

        }
    }
};

typedef fastdelegate::FastDelegate< bool ( int row, int rows ) > IMG_LOADING_PROGRESS;

struct img_reader_s
{
    uint8 data[1024]; // internal reader data

    IMG_LOADING_PROGRESS progress;

    ivec2 size;
    uint8 bitpp;

    image_read_func detect( const void *d, aint datasize, img_format_e &fmt, const ivec2 &limitsize )
    {
#define FMT(fn) if (image_read_func f = detect_##fn##_format(d, datasize, limitsize)) { fmt = if_##fn; return f; }
        IMGFORMATS
#undef FMT
        return nullptr;
    }
#define FMT(fn) image_read_func detect_##fn##_format(const void *data, aint datasize, const ivec2& limitsize);
    IMGFORMATS
#undef FMT

};


#define SAVE_IMAGE_OPT_JPG_QUALITY 127
#define SAVE_IMAGE_OPT_JPG_COLORSPACE_RGB ((SAVE_IMAGE_OPT_JPG_QUALITY+1) << 0)

#define DEFAULT_SAVE_OPT_png 0
#define DEFAULT_SAVE_OPT_jpg (80 | SAVE_IMAGE_OPT_JPG_COLORSPACE_RGB)
#define DEFAULT_SAVE_OPT_dds 0
#define DEFAULT_SAVE_OPT_bmp 0
#define DEFAULT_SAVE_OPT_tga 0
#define DEFAULT_SAVE_OPT_gif 0

#define DEFAULT_SAVE_OPT(fn) DEFAULT_SAVE_OPT_##fn

struct bmpcore_exbody_s;
#define FMT(fn) bool save_to_##fn##_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options);
IMGFORMATS
#undef FMT


template <typename R> void enum_supported_formats(R r)
{
#define FMT(fn) r(#fn);
    IMGFORMATS
#undef FMT
}

struct bmpcore_normal_s;
template<typename CORE> class bitmap_t;
typedef bitmap_t<bmpcore_normal_s> bitmap_c;

class animated_c // gif animated
{
    uint data[64];
public:
    animated_c();
    ~animated_c();

    bool load( const void *b, ts::aint bsize ); // returns true, if animated
    int firstframe( bitmap_c &bmp ); // returns ms time of frame. bmp should contain previous frame!
    int nextframe( const bmpcore_exbody_s &bmp );

    int numframes() const;

    size_t size() const;
};

} // namespace ts