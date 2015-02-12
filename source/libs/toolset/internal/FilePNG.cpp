#include "toolset.h"

#define PNG_USER_MEM_SUPPORTED
#define PNG_ZBUF_SIZE   65536

#include <windows.h>
#include "FilePNG.h"
#include "pnglib/src/png.h"

namespace ts
{

struct SPNGData
{
	png_structp     png_ptr;
	png_infop       info_ptr;
	png_colorp      palette;
	int             num_palette;

	const void *    SouBuf;
	size_t          SmeBuf;
	size_t          LenSouBuf;
	int             LenX,LenY;

    bool    error;
    bool    warning;

};

static void PNGAPI FilePNG_png_default_error(png_structp png_ptr, png_const_charp message)
{
	SPNGData * sdata=(SPNGData *)(png_ptr->io_ptr);
    sdata->error = true;
}

static void PNGAPI FilePNG_png_default_warning(png_structp png_ptr, png_const_charp message)
{
	SPNGData * sdata=(SPNGData *)(png_ptr->io_ptr);
    sdata->warning = true;
}

static void PNGAPI FilePNG_png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	SPNGData * sdata=(SPNGData *)(png_ptr->io_ptr);

	if(sdata->LenSouBuf - sdata->SmeBuf < length)
    {
        sdata->error = true;
        return;
    }
	memcpy(data,((uint8 *)(sdata->SouBuf))+sdata->SmeBuf,length);
	sdata->SmeBuf+=length;
}

static void PNGAPI FilePNG_png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	SPNGData * sdata=(SPNGData *)(png_ptr->io_ptr);

	if((sdata->SmeBuf+int(length))<sdata->LenSouBuf) {
		memcpy(((uint8 *)sdata->SouBuf)+sdata->SmeBuf,data,length);
	}
	sdata->SmeBuf+=length;
}

static png_voidp FilePNG_png_alloc(png_structp png_ptr, png_size_t length)
{
    return MM_ALLOC( length );
}

static void FilePNG_png_free(png_structp png_ptr, png_voidp data)
{
    MM_FREE( data );
}

// format: 1-gray 2-rgb 3-rgba 4-palate
size_t FilePNG_ReadStart_Buf(const void * soubuf,int soubuflen,ivec2 &sz, uint8 &bitpp)
{
	if (!(soubuflen >= 8 && png_check_sig((png_byte*)soubuf, 8))) return 0;

	SPNGData * data = (SPNGData *)MM_ALLOC( sizeof(SPNGData) );
	memset(data,0, sizeof(SPNGData));
    unsigned int sig_read = 0;

	data->SouBuf = soubuf;
	data->LenSouBuf = soubuflen;

	if((data->png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
												(void *)data,
												FilePNG_png_default_error,
												FilePNG_png_default_warning,
                                                NULL,
                                                FilePNG_png_alloc,
                                                FilePNG_png_free

                                                
                                                )) == NULL) goto eggog;

    if (data->error) goto eggog;

    png_set_bgr( data->png_ptr );

	if((data->info_ptr = png_create_info_struct(data->png_ptr)) == NULL) goto eggog;

	png_set_read_fn(data->png_ptr, (void *)data, FilePNG_png_default_read_data);
    if (data->error) goto eggog;

    png_set_sig_bytes(data->png_ptr, sig_read);
	png_read_info(data->png_ptr, data->info_ptr);
	//png_set_strip_16(data->png_ptr);
	png_set_packing(data->png_ptr);
	png_read_update_info(data->png_ptr, data->info_ptr);

//		uint32 coltype=png_get_color_type(data->png_ptr, data->info_ptr);
	uint32 coltype=data->info_ptr->color_type;
    if(coltype==PNG_COLOR_TYPE_GRAY) bitpp = 8;
	else if(coltype==PNG_COLOR_TYPE_RGB) bitpp=24;
	else if(coltype==PNG_COLOR_TYPE_RGB_ALPHA) bitpp=32;
	else if(coltype==PNG_COLOR_TYPE_PALETTE)
    {
        ERROR( "unsupported png format" );

		//*format=4;
		//png_get_PLTE(data->png_ptr,data->info_ptr, &(data->palette), &(data->num_palette));
		//*countcolor=data->num_palette;
	} else goto eggog;

	data->LenX=sz.x=data->info_ptr->width;
	data->LenY=sz.y=data->info_ptr->height;

    return (size_t)data;
eggog:
	if(data->png_ptr!=NULL) png_destroy_read_struct(&(data->png_ptr),&(data->info_ptr), (png_infopp)NULL);
	MM_FREE( data );
	return 0;
}

size_t FilePNG_Read(size_t id,void * buf,aint lenline)
{
	SPNGData * data=(SPNGData *)id;
	png_bytep * row_pointers=NULL;
	uint8 * tbuf;
	int i;

	//if(arraycolor!=NULL && data->num_palette>0)
 //   {
	//	for(i=0;i<data->num_palette;i++)
 //       {
	//		arraycolor[i]=(uint32(data->palette[i].red)) | ((uint32(data->palette[i].green))<<8) | ((uint32(data->palette[i].blue))<<16);
	//	}
	//}

    aint rolinesize = png_get_rowbytes(data->png_ptr, data->info_ptr);
    if (rolinesize > lenline) goto eggog;

	row_pointers=(png_bytep *)_alloca( (data->LenY)*sizeof(png_bytep) );
	for(i=0,tbuf=(uint8 *)buf;i<data->LenY;i++,tbuf+=lenline) row_pointers[i]=tbuf;

	png_read_image(data->png_ptr, row_pointers);
	png_read_end(data->png_ptr, data->info_ptr);

    //if(row_pointers!=NULL) MM_FREE(slmemcat::bitmap, row_pointers);
    if(data->png_ptr!=NULL) png_destroy_read_struct(&(data->png_ptr),&(data->info_ptr), (png_infopp)NULL);
    MM_FREE( data );
    return 1;

eggog:
	//if(row_pointers!=NULL) MM_FREE(slmemcat::bitmap, row_pointers);
	if(data->png_ptr!=NULL) png_destroy_read_struct(&(data->png_ptr),&(data->info_ptr), (png_infopp)NULL);
	MM_FREE( data );
	return 0;
}

size_t FilePNG_Write(void * bufout,int bufoutlen,const void * buf,int ll,const ivec2 &sz,uint8 bytepp,int rgb_to_bgr)
{
	png_structp png_ptr=NULL;
	png_infop info_ptr=NULL;
	SPNGData data;
	uint8 * * rows=NULL;
	int i;

    data.error = false;

	png_ptr = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        (void *)&data,
        FilePNG_png_default_error,
        FilePNG_png_default_warning,
        NULL,
        FilePNG_png_alloc,
        FilePNG_png_free
        );
    if (data.error) goto eggog;
	if (png_ptr == NULL) goto eggog;

    png_set_bgr( png_ptr );

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) goto eggog;

	data.SouBuf=bufout;
	data.LenSouBuf=bufoutlen;
	data.SmeBuf=0;

	png_set_write_fn(png_ptr, (void *)&data, FilePNG_png_default_write_data,NULL);

	int type;
	if(bytepp==1) type=PNG_COLOR_TYPE_GRAY;
	else if(bytepp==2) type=PNG_COLOR_TYPE_GRAY_ALPHA;
	else if(bytepp==3) type=PNG_COLOR_TYPE_RGB;
	else if(bytepp==4) type=PNG_COLOR_TYPE_RGB_ALPHA;
	else throw "Error";

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	png_set_IHDR(png_ptr, info_ptr, sz.x, sz.y, 8, type,PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	if(rgb_to_bgr) png_set_bgr(png_ptr);

	rows=(uint8 * *)MM_ALLOC(sz.y*sizeof(uint8 *));
	for (i = 0; i < int(sz.y); i++) rows[i] = ((uint8 *)buf) + i*ll;

	png_write_image(png_ptr, rows);

	png_write_end(png_ptr, info_ptr);

	if(png_ptr!=NULL)
    {
        png_destroy_info_struct( png_ptr, &info_ptr );
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    }
	if(rows!=NULL) MM_FREE(rows);
    return data.SmeBuf;

eggog:
	if(png_ptr!=NULL)
    {
        png_destroy_info_struct( png_ptr, &info_ptr );
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    }
	if(rows!=NULL) MM_FREE(rows);
	return 0;
}

} // namespace ts