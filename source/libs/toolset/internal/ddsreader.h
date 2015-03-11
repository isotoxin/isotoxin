#pragma once
namespace ts
{

void * dds_start(const void * soubuf,aint soubuflen,ivec2 &sz, uint8 &bitpp);
bool dds_read(void * decoder,void * buf,aint lenline);

}
