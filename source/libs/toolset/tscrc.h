#pragma once

namespace ts
{

uint TSCALL CRC32(const void * buf, uint len);
uint TSCALL CRC32_Buf(const void * buf, int len, uint crc = 0xFFFFFFFF);
FORCEINLINE uint CRC32_End(uint crc) {return ~crc;}

} // namespace ts
