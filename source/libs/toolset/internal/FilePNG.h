#ifndef _INCLUDE_FILEPNG
#define _INCLUDE_FILEPNG

namespace ts
{

// format: 1-gray 2-rgb 3-rgba 4-palate
size_t FilePNG_ReadStart_Buf(const void * soubuf, int soubuflen, ivec2 &sz, uint8 &bitpp);
size_t FilePNG_Read(size_t id,void * buf,aint lenline);

// Возвращает полный размер файла. Если больше bufoutlen то нужно вызвать повторно. При ошибке 0
size_t FilePNG_Write(void * bufout,int bufoutlen,const void * buf,int ll,const ivec2 &sz,uint8 bytepp,int rgb_to_bgr);
}

#endif
