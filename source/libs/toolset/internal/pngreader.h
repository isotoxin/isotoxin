#pragma once

namespace ts
{

struct read_png_data_s
{
    char body[36];
};

bool png_decode_start(read_png_data_s&data, const void * soubuf, int soubuflen, ivec2 &sz, uint8 &bitpp);
bool png_decode(read_png_data_s&data, void * buf, aint lenline);

// returns png file size. call again, if returned size > bufoutlen. returns 0 if error
size_t png_write(void * bufout, size_t bufoutlen, const void * buf, int pitch, const ivec2 &sz, uint8 bytepp);
}
