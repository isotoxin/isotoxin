#ifndef _INCLUDE_FILEDDS
#define _INCLUDE_FILEDDS

namespace ts
{

size_t FileDDS_ReadStart_Buf(const void * soubuf,aint soubuflen,ivec2 &sz, uint8 &bitpp);
size_t FileDDS_Read(size_t id,void * buf,aint lenline);

}

#endif
