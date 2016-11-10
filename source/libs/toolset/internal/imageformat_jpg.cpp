#include "toolset.h"
#include "platform.h"

#pragma pack (push)
#pragma pack (16)
extern "C" {
#define XMD_H
#undef FAR
#include "jpeg/src/jpeglib.h"
#include "jpeg/src/jerror.h"
#undef XMD_H
}
#pragma pack (pop)

#pragma comment(lib, "jpeg.lib")

namespace ts
{


namespace
{
    struct jpgsavebuf : public jpeg_destination_mgr
    {
        static const int BLOCK_SIZE = 4096 / sizeof(JOCTET);

        buf_c &buf;
        jpgsavebuf(buf_c &buf) :buf(buf)
        {
            buf.clear();

            init_destination = tsjpg_init_destination;
            empty_output_buffer = tsjpg_empty_output_buffer;
            term_destination = tsjpg_term_destination;

        }
        jpgsavebuf(const jpgsavebuf&) UNUSED;
        jpgsavebuf(jpgsavebuf &&) UNUSED;
        jpgsavebuf &operator=(const jpgsavebuf &) UNUSED;

        static void tsjpg_init_destination(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->next_output_byte = (JOCTET *)me->buf.expand(BLOCK_SIZE);
            me->free_in_buffer = BLOCK_SIZE;
        }

        static boolean tsjpg_empty_output_buffer(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->next_output_byte = (JOCTET *)me->buf.expand(BLOCK_SIZE);
            me->free_in_buffer = BLOCK_SIZE;
            return true;
        }

        static void tsjpg_term_destination(j_compress_ptr cinfo)
        {
            jpgsavebuf *me = (jpgsavebuf *)cinfo->dest;
            me->buf.set_size(me->buf.size() - me->free_in_buffer);
        }

    };

    struct jpgreadbuf : public jpeg_source_mgr
    {
        static const int BLOCK_SIZE = 4096 / sizeof(JOCTET);

        const JOCTET * buf;
        int buflen;
        uint curp;
        JOCTET fakeend[2];
        bool fail;
        jpgreadbuf(const void * buf, int buflen) :buf((const JOCTET *)buf), buflen(buflen)
        {
            curp = 0;
            fail = false;
            init_source = tsjpg_init_source;
            fill_input_buffer = tsjpg_fill_input_buffer;
            skip_input_data = tsjpg_skip_input_data;
            resync_to_restart = jpeg_resync_to_restart; /* use default method */
            term_source = tsjpg_term_source;
            bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
            next_input_byte = nullptr; /* until buffer loaded */
            fakeend[0] = (JOCTET)0xFF;
            fakeend[1] = (JOCTET)JPEG_EOI;

        }
        jpgreadbuf(jpgreadbuf &&) UNUSED;
        jpgreadbuf(const jpgreadbuf&) UNUSED;
        jpgreadbuf &operator=(const jpgreadbuf &) UNUSED;


        static void tsjpg_init_source(j_decompress_ptr cinfo)
        {
            jpgreadbuf *me = (jpgreadbuf *)cinfo->src;
            me->curp = 0;
        }

        static boolean tsjpg_fill_input_buffer(j_decompress_ptr cinfo)
        {
            jpgreadbuf *src = (jpgreadbuf *)cinfo->src;

            if ((int)src->curp >= src->buflen)
            {
                src->next_input_byte = src->fakeend;
                src->bytes_in_buffer = 2;
                return TRUE;
            }
            else
            {
                src->next_input_byte = src->buf;
                src->bytes_in_buffer = src->buflen;
                src->curp = (uint)src->bytes_in_buffer;
                return TRUE;
            }
            //return TRUE;
        }

        static void tsjpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
        {
            jpgreadbuf *src = (jpgreadbuf *)cinfo->src;
            src->next_input_byte += num_bytes;
            src->bytes_in_buffer -= num_bytes;
            if (src->bytes_in_buffer <= 0)
            {
                src->next_input_byte = src->fakeend;
                src->bytes_in_buffer = 2;
            }
            //DEBUG_BREAK();
        }
        static void tsjpg_term_source(j_decompress_ptr cinfo)
        {
            /* no work necessary here */
        }

    };

    static void __error_exit(j_common_ptr cinfo)
    {
        jpgreadbuf *src = (jpgreadbuf *)cinfo->client_data;
        src->fail = true;
    }

    struct jpgread_internal_data_s
    {
        jpeg_decompress_struct cinfo;
        jpeg_error_mgr jerr;
        jpgreadbuf tmp;
        int row_stride;
        int shrinkx;

        jpgread_internal_data_s(const void *sourcebuf, aint sourcebufsize):tmp(sourcebuf, (int)sourcebufsize)
        {
            cinfo.err = jpeg_std_error(&jerr);
            jerr.error_exit = __error_exit;

            jpeg_create_decompress(&cinfo);

            cinfo.src = &tmp;
            cinfo.client_data = &tmp;
            cinfo.out_color_space = JCS_RGB;

        }
        ~jpgread_internal_data_s()
        {
            jpeg_finish_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
        }
    };
}


static bool jpgdatareader(img_reader_s &r, void * buf, int pitch)
{
    jpgread_internal_data_s &d = ref_cast<jpgread_internal_data_s>( r.data );

    JSAMPARRAY aTempBuffer = (*d.cinfo.mem->alloc_sarray)
        ((j_common_ptr)&d.cinfo, JPOOL_IMAGE, d.row_stride, 1);

    shrink_on_read_c sor( ivec2( (int)d.cinfo.output_width, (int)d.cinfo.output_height ), d.shrinkx );

    sor.do_job( buf, pitch, 3, [&]( int y, uint8 *row ) {

        if ( r.progress && r.progress( y, d.cinfo.output_height ) )
        {
            d.tmp.fail = true;
            return true;
        }

        jpeg_read_scanlines( &d.cinfo, aTempBuffer, 1 );
        if ( d.tmp.fail ) return true;

        uint8* src = aTempBuffer[ 0 ];

        for ( int x = d.cinfo.output_width; x > 0; x--, src += d.cinfo.output_components )
        {
            *row++ = src[ 2 ];
            *row++ = src[ 1 ];
            *row++ = src[ 0 ];
        }
        return false;

    } );

    if ( r.progress )
        r.progress( d.cinfo.output_height, d.cinfo.output_height );

    bool ok = !d.tmp.fail;
    d.~jpgread_internal_data_s();
    return ok;
}

image_read_func img_reader_s::detect_jpg_format(const void *sourcebuf, aint sourcebufsize, const ivec2& limitsize )
{
    uint32 tag = *(uint32 *)sourcebuf;
    if ((tag & 0xFFFFFF) != 16767231)
        return nullptr;

    jpgread_internal_data_s &d = ref_cast<jpgread_internal_data_s>( data );

    TSPLACENEW(&d, sourcebuf, sourcebufsize);

    if (JPEG_SUSPENDED == jpeg_read_header(&d.cinfo, true))
    {
        d.~jpgread_internal_data_s();
        return nullptr;
    }
    if (d.tmp.fail)
    {
        d.~jpgread_internal_data_s();
        return nullptr;
    }
    jpeg_start_decompress(&d.cinfo);
    if (d.tmp.fail)
    {
        d.~jpgread_internal_data_s();
        return nullptr;
    }

    d.row_stride = d.cinfo.output_width * d.cinfo.output_components;

    size.x = d.cinfo.output_width;
    size.y = d.cinfo.output_height;
    bitpp = 24;

    d.shrinkx = 0;

    if ( limitsize >> 0 )
    {
        while ( size > limitsize )
        {
            size.x >>= 1;
            size.y >>= 1;
            ++d.shrinkx;
        }
    }

    if ( size >> 0 );
    else
    {
        d.~jpgread_internal_data_s();
        return nullptr;
    }

    return jpgdatareader;
}


bool save_to_jpg_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options)
{
    buf.clear();
    if (bmp.info().bytepp() < 3) return false;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    if (options & SAVE_IMAGE_OPT_JPG_COLORSPACE_RGB) jpeg_set_colorspace(&cinfo, JCS_RGB);
    cinfo.image_width = bmp.info().sz.x;
    cinfo.image_height = bmp.info().sz.y;
    cinfo.input_components = 3;
    cinfo.optimize_coding = 1;
    jpeg_set_quality(&cinfo, (options & SAVE_IMAGE_OPT_JPG_QUALITY), TRUE);

    jpgsavebuf mgr(buf);

    if (cinfo.dest == nullptr)
        cinfo.dest = &mgr;

    jpeg_start_compress(&cinfo, true);

    aint row_stride = bmp.info().sz.x * 3, bpp = bmp.info().bytepp();

    unsigned char* aTempBuffer = (unsigned char*)_alloca(row_stride);

    for (int y = 0; y < bmp.info().sz.y; y++){
        uint8* src = bmp() + bmp.info().pitch*y;
        uint8* dest = aTempBuffer;

        for (int x = bmp.info().sz.x; x > 0; x--, src += bpp){
            *dest++ = src[2];
            *dest++ = src[1];
            *dest++ = src[0];
        }

        jpeg_write_scanlines(&cinfo, &aTempBuffer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return true;
}


} //namespace ts

