#include "toolset.h"

#define PNG_USER_MEM_SUPPORTED
#define PNG_ZBUF_SIZE   65536

#include <windows.h>
#include "pngreader.h"
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

	const void *    src;
    size_t          srcsize;
	size_t          offset;
	int             sizex,sizey;
    int             bytepp;

    bool    error;
    bool    warning;
};

static_assert(sizeof(read_png_data_s) == sizeof(read_png_data_internal_s),"!");

static void PNGAPI FilePNG_png_default_error(png_structp png_ptr, png_const_charp message)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)png_ptr->io_ptr;
    sdata->error = true;
}

static void PNGAPI FilePNG_png_default_warning(png_structp png_ptr, png_const_charp message)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)png_ptr->io_ptr;
    sdata->warning = true;
}

static void PNGAPI FilePNG_png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
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

static void PNGAPI FilePNG_png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	read_png_data_internal_s * sdata=(read_png_data_internal_s *)(png_ptr->io_ptr);

	if((sdata->offset+int(length))<sdata->srcsize) {
		memcpy(((uint8 *)sdata->src)+sdata->offset,data,length);
	}
	sdata->offset+=length;
}

static png_voidp FilePNG_png_alloc(png_structp png_ptr, png_size_t length)
{
    return MM_ALLOC( length );
}

static void FilePNG_png_free(png_structp png_ptr, png_voidp data)
{
    MM_FREE( data );
}

bool png_decode_start(read_png_data_s&idata, const void * soubuf,int soubuflen,ivec2 &sz, uint8 &bitpp)
{
	if (!(soubuflen >= 8 && png_check_sig((png_byte*)soubuf, 8))) return false;
    read_png_data_internal_s &data = ref_cast<read_png_data_internal_s>(idata);
    memset(&data,0,sizeof(read_png_data_internal_s));

    unsigned int sig_read = 0;

	data.src = soubuf;
	data.srcsize = soubuflen;

	if((data.png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
												&data,
												FilePNG_png_default_error,
												FilePNG_png_default_warning,
                                                nullptr,
                                                FilePNG_png_alloc,
                                                FilePNG_png_free

                                                
                                                )) == nullptr) goto eggog;

    if (data.error) goto eggog;

    png_set_bgr( data.png_ptr );
    png_set_scale_16(data.png_ptr);
    png_set_palette_to_rgb(data.png_ptr);

	if((data.info_ptr = png_create_info_struct(data.png_ptr)) == nullptr) goto eggog;

	png_set_read_fn(data.png_ptr, &data, FilePNG_png_default_read_data);
    if (data.error) goto eggog;

    png_set_sig_bytes(data.png_ptr, sig_read);
	png_read_info(data.png_ptr, data.info_ptr);
	png_set_packing(data.png_ptr);
	png_read_update_info(data.png_ptr, data.info_ptr);

	switch(data.info_ptr->color_type)
    {
        case PNG_COLOR_TYPE_GRAY: bitpp = 8; break;
        case PNG_COLOR_TYPE_RGB: bitpp = 24; break;
        case PNG_COLOR_TYPE_RGB_ALPHA: bitpp = 32; break;
        default:
            goto eggog;
    }


	data.sizex=sz.x=data.info_ptr->width;
	data.sizey=sz.y=data.info_ptr->height;
    data.bytepp = bitpp / 8;

    return true;
eggog:
	if(data.png_ptr!=nullptr) png_destroy_read_struct(&(data.png_ptr),&(data.info_ptr), (png_infopp)nullptr);
	return false;
}

bool png_decode(read_png_data_s&idata, void * buf, aint pitch)
{
	read_png_data_internal_s &data = ref_cast<read_png_data_internal_s>(idata);
	png_bytep * row_pointers=nullptr;

    aint rolinesize = png_get_rowbytes(data.png_ptr, data.info_ptr);
    if (rolinesize > pitch) goto eggog;

	row_pointers=(png_bytep *)_alloca( data.sizey*sizeof(png_bytep) );
    uint8 * tbuf = (uint8 *)buf;
	for(int i=0;i<data.sizey;++i,tbuf+=pitch)
        row_pointers[i]=tbuf;

	png_read_image(data.png_ptr, row_pointers);
	png_read_end(data.png_ptr, data.info_ptr);

    if(data.png_ptr!=nullptr) png_destroy_read_struct(&(data.png_ptr),&(data.info_ptr), (png_infopp)nullptr);
    return true;

eggog:
	//if(row_pointers!=nullptr) MM_FREE(slmemcat::bitmap, row_pointers);
	if(data.png_ptr!=nullptr) png_destroy_read_struct(&(data.png_ptr),&(data.info_ptr), (png_infopp)nullptr);
	return false;
}

size_t png_write(void * bufout,size_t bufoutlen,const void * buf,int pitch, const ivec2 &sz,uint8 bytepp)
{
	png_structp png_ptr=nullptr;
	png_infop info_ptr=nullptr;
	read_png_data_internal_s data;
	uint8 **rows=nullptr;
	int i;

    data.error = false;

	png_ptr = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        (void *)&data,
        FilePNG_png_default_error,
        FilePNG_png_default_warning,
        nullptr,
        FilePNG_png_alloc,
        FilePNG_png_free
        );
    if (data.error) goto eggog;
	if (png_ptr == nullptr) goto eggog;

    png_set_bgr( png_ptr );

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) goto eggog;

	data.src=bufout;
	data.srcsize=bufoutlen;
	data.offset=0;

	png_set_write_fn(png_ptr, (void *)&data, FilePNG_png_default_write_data,nullptr);

	int type;
	if(bytepp==1) type=PNG_COLOR_TYPE_GRAY;
	else if(bytepp==2) type=PNG_COLOR_TYPE_GRAY_ALPHA;
	else if(bytepp==3) type=PNG_COLOR_TYPE_RGB;
	else if(bytepp==4) type=PNG_COLOR_TYPE_RGB_ALPHA;
	else goto eggog;

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_IHDR(png_ptr, info_ptr, sz.x, sz.y, 8, type,PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	rows=(uint8 * *)MM_ALLOC(sz.y*sizeof(uint8 *));
	for (i = 0; i < int(sz.y); i++) rows[i] = ((uint8 *)buf) + i*pitch;

	png_write_image(png_ptr, rows);

	png_write_end(png_ptr, info_ptr);

	if(png_ptr)
    {
        png_destroy_info_struct( png_ptr, &info_ptr );
        png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
    }
	if(rows!=nullptr) MM_FREE(rows);
    return data.offset;

eggog:
	if(png_ptr)
    {
        png_destroy_info_struct( png_ptr, &info_ptr );
        png_destroy_write_struct(&png_ptr, (png_infopp)nullptr);
    }
	if(rows!=nullptr) MM_FREE(rows);
	return 0;
}

} // namespace ts