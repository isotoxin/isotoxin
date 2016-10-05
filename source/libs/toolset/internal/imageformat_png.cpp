#include "toolset.h"
#include "platform.h"

#define PNG_USER_MEM_SUPPORTED
#define PNG_ZBUF_SIZE   65536

#include "pnglib/src/png.h"
#include "pnglib/src/pngstruct.h"
#include "pnglib/src/pnginfo.h"

#pragma comment(lib, "pnglib.lib")

namespace ts
{

struct read_png_data_internal_s
{
	png_structp     png_ptr;
	png_infop       info_ptr;
    buf_c          *buf;
	const void *    src;
    size_t          srcsize;
	size_t          offset;
    int             bytepp;
    int             shrinkx;

    bool    error;
    bool    warning;
};

static void PNGAPI _png_default_error(png_structp png_ptr, png_const_charp message)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)png_ptr->io_ptr;
    sdata->error = true;
}

static void PNGAPI _png_default_warning(png_structp png_ptr, png_const_charp message)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)png_ptr->io_ptr;
    sdata->warning = true;
}

static void PNGAPI _png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)png_ptr->io_ptr;

	if(sdata->srcsize - sdata->offset < length)
    {
        sdata->error = true;
        return;
    }
	memcpy(data,((uint8 *)(sdata->src))+sdata->offset,length);
	sdata->offset+=length;
}

static void PNGAPI _png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)(png_ptr->io_ptr);
    sdata->buf->append_buf(data,length);
}

static png_voidp _png_alloc(png_structp png_ptr, png_size_t length)
{
    return MM_ALLOC( length );
}

static void _png_free(png_structp png_ptr, png_voidp data)
{
    MM_FREE( data );
}

static bool pngdatareader(img_reader_s &r, void * buf, int pitch);

image_read_func img_reader_s::detect_png_format(const void *sourcebuf, aint sourcebufsize, const ivec2& limitsize )
{
    uint32 tag = *(uint32 *)sourcebuf;
    if (tag != 1196314761) return nullptr;

    if (!(sourcebufsize >= 8 && png_check_sig((png_byte*)sourcebuf, 8))) return nullptr;
    
    TS_STATIC_CHECK( sizeof(read_png_data_internal_s) <= sizeof(data), "check data size");

    read_png_data_internal_s &idata = ref_cast<read_png_data_internal_s>(data);

    memset(&idata, 0, sizeof(read_png_data_internal_s));

    uint sig_read = 0;

    idata.src = sourcebuf;
    idata.srcsize = sourcebufsize;

    if ((idata.png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
        &data,
        _png_default_error,
        _png_default_warning,
        nullptr,
        _png_alloc,
        _png_free


        )) == nullptr) goto eggog;

    if (idata.error) goto eggog;

    png_set_bgr(idata.png_ptr);
    png_set_scale_16(idata.png_ptr);
    png_set_palette_to_rgb(idata.png_ptr);
    png_set_gray_to_rgb(idata.png_ptr);

    if ((idata.info_ptr = png_create_info_struct(idata.png_ptr)) == nullptr) goto eggog;

    png_set_read_fn(idata.png_ptr, &data, _png_default_read_data);
    if (idata.error) goto eggog;

    png_set_sig_bytes(idata.png_ptr, sig_read);
    png_read_info(idata.png_ptr, idata.info_ptr);
    png_set_packing(idata.png_ptr);
    png_read_update_info(idata.png_ptr, idata.info_ptr);

    switch (idata.info_ptr->color_type)
    {
        case PNG_COLOR_TYPE_GRAY: bitpp = 8; break;
        case PNG_COLOR_TYPE_RGB: bitpp = 24; break;
        case PNG_COLOR_TYPE_RGB_ALPHA: bitpp = 32; break;
        default:
            goto eggog;
    }


    size.x = idata.info_ptr->width;
    size.y = idata.info_ptr->height;
    idata.bytepp = bitpp / 8;

    if ( limitsize >> 0 )
    {
        if ( idata.info_ptr->interlace_type != PNG_INTERLACE_NONE && ( size > limitsize ) )
            goto eggog; // just do not support interlaced super big images

        while ( size > limitsize )
        {
            size.x >>= 1;
            size.y >>= 1;
            ++idata.shrinkx;
        }
    }

    if (size >> 0)
        return pngdatareader;

eggog:
    if (idata.png_ptr != nullptr) png_destroy_read_struct(&(idata.png_ptr), &(idata.info_ptr), (png_infopp)nullptr);
    return nullptr;
}

static bool pngdatareader(img_reader_s &r, void * buf, int pitch)
{
	read_png_data_internal_s &data = ref_cast<read_png_data_internal_s>(r.data);

    ASSERT( pitch >= r.size.x * r.bitpp/8 );

    bool instant_stop = false;

    if ( data.info_ptr->interlace_type != PNG_INTERLACE_NONE )
    {
        aint rolinesize = png_get_rowbytes( data.png_ptr, data.info_ptr ) >> data.shrinkx;
        if ( rolinesize <= pitch )
        {
            png_bytep * row_pointers = nullptr;
            uint8 * tbuf = (uint8 *)buf;
            row_pointers = (png_bytep *)_alloca( r.size.y * sizeof( png_bytep ) );
            for ( int i = 0; i < r.size.y; ++i, tbuf += pitch )
                row_pointers[ i ] = tbuf;
            png_read_image( data.png_ptr, row_pointers );
            png_read_end( data.png_ptr, data.info_ptr );

            if ( data.png_ptr != nullptr ) png_destroy_read_struct( &( data.png_ptr ), &( data.info_ptr ), ( png_infopp )nullptr );
            return true;
        }

        if ( r.progress )
            r.progress( data.info_ptr->height, data.info_ptr->height );

        if ( data.png_ptr != nullptr ) png_destroy_read_struct( &( data.png_ptr ), &( data.info_ptr ), ( png_infopp )nullptr );
        return true;

    } else
    {
        shrink_on_read_c sor( ivec2( (int)data.info_ptr->width, (int)data.info_ptr->height ), data.shrinkx );
        sor.do_job( buf, pitch, data.bytepp, [&]( int y, uint8 *row ) {

            if ( r.progress && r.progress( y, data.info_ptr->height ) )
            {
                instant_stop = true;
                return true;
            }

            png_read_row( data.png_ptr, row, nullptr );
            return false;
        } );
        png_read_end( data.png_ptr, data.info_ptr );
    }

    if ( r.progress )
        r.progress( data.info_ptr->height, data.info_ptr->height );

    if ( data.png_ptr != nullptr ) png_destroy_read_struct( &( data.png_ptr ), &( data.info_ptr ), ( png_infopp )nullptr );
    return !instant_stop;
}

bool save_to_png_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options)
{
    buf.clear();
	read_png_data_internal_s data;

    data.error = false;

    png_infop info_ptr = nullptr;
    png_structp png_ptr = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        (void *)&data,
        _png_default_error,
        _png_default_warning,
        nullptr,
        _png_alloc,
        _png_free
        );
    if (data.error) goto eggog;
	if (png_ptr == nullptr) goto eggog;

    png_set_bgr( png_ptr );

    info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) goto eggog;

	data.buf = &buf;

	png_set_write_fn(png_ptr, &data, _png_default_write_data,nullptr);

	int type;
    switch (bmp.info().bytepp())
    {
    case 1: type = PNG_COLOR_TYPE_GRAY; break;
    case 3: type = PNG_COLOR_TYPE_RGB; break;
    case 4: type = PNG_COLOR_TYPE_RGB_ALPHA; break;
    default:
    eggog:
        buf.clear();
        if ( png_ptr )
        {
            png_destroy_info_struct( png_ptr, &info_ptr );
            png_destroy_write_struct( &png_ptr, ( png_infopp )nullptr );
        }
        return false;
    }

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_IHDR(png_ptr, info_ptr, bmp.info().sz.x, bmp.info().sz.y, 8, type,PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

    uint8 **rows = (uint8 **)_alloca(bmp.info().sz.y*sizeof(uint8 *));
    uint8 *ibody = bmp();
	for (int i = 0, y = bmp.info().sz.y; i < y; ++i, ibody+=bmp.info().pitch)
        rows[i] = ibody;

	png_write_image(png_ptr, rows);

	png_write_end(png_ptr, info_ptr);

	if(png_ptr)
    {
        png_destroy_info_struct( png_ptr, &info_ptr );
        png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
    }
    return true;
}

} // namespace ts