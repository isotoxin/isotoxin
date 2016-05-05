#pragma once

namespace ts
{

uint32 TSCALL CRC32(const void * buf, aint len);
uint32 TSCALL CRC32_Buf(const void * buf, aint len, uint32 crc = 0xFFFFFFFF);
INLINE uint32 CRC32_End(uint32 crc) {return ~crc;}

} // namespace ts
