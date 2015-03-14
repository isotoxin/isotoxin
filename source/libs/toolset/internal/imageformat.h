#pragma once

namespace ts
{

#define IMGFORMATS \
    FMT( png ) \
    FMT( jpg ) \
    FMT( dds ) \
    FMT( bmp ) \
    FMT( tga ) \


enum img_format_e
{
#define FMT(fn) if_##fn,
    IMGFORMATS
#undef FMT
    if_total
};

struct img_reader_s;
typedef fastdelegate::FastDelegate<bool (img_reader_s &, void *, int)> image_read_func;

struct img_reader_s
{
    uint8 data[1024]; // internal reader data
    ivec2 size;
    uint8 bitpp;

    image_read_func detect( const void *data, int datasize )
    {
#define FMT(fn) if (image_read_func f = detect_##fn##_format(data, datasize)) return f;
        IMGFORMATS
#undef FMT
        return nullptr;
    }
#define FMT(fn) image_read_func detect_##fn##_format(const void *data, int datasize);
    IMGFORMATS
#undef FMT

};


#define SAVE_IMAGE_OPT_JPG_QUALITY 127
#define SAVE_IMAGE_OPT_JPG_COLORSPACE_RGB ((SAVE_IMAGE_OPT_JPG_QUALITY+1) << 0)

#define DEFAULT_SAVE_OPT_png 0
#define DEFAULT_SAVE_OPT_jpg (80 | SAVE_IMAGE_OPT_JPG_COLORSPACE_RGB)
#define DEFAULT_SAVE_OPT_dds 0
#define DEFAULT_SAVE_OPT_bmp 0

#define DEFAULT_SAVE_OPT(fn) DEFAULT_SAVE_OPT_##fn

struct bmpcore_exbody_s;
#define FMT(fn) bool save_to_##fn##_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options);
IMGFORMATS
#undef FMT


} // namespace ts