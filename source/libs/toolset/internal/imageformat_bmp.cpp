#include "toolset.h"
#include "fourcc.h"

namespace ts
{
struct bmpread_s
{
    BITMAPINFOHEADER *iH;
    int ibuflen;
};

static bool bmpdatareader(img_reader_s &r, void * buf, int pitch)
{
    bmpread_s & br = ref_cast<bmpread_s>(r.data);

    const uint8 *pixels = ((const uint8*)br.iH) + br.iH->biSize;
    const uint8 *epixels = ((const uint8*)br.iH) + br.ibuflen;
    uint8 *tdata = (uint8 *)buf + (r.size.y - 1)*pitch;

    int bmppitch = r.size.x * br.iH->biBitCount / 8;
    bmppitch = (bmppitch + 3) & ~3;

    for (int y = r.size.y; y > 0; tdata -= pitch, pixels += bmppitch, --y)
        if (CHECK((pixels + pitch) <= epixels))
            memcpy(tdata, pixels, pitch);

    return true;

}

namespace
{
#pragma pack(push, 1)
    struct BITMAPFILEHEADER
    {
        uint16  bfType;
        uint32  bfSize;
        uint16  bfReserved1;
        uint16  bfReserved2;
        uint32  bfOffBits;
    };
    struct Header
    {
        BITMAPFILEHEADER fH;
        BITMAPINFOHEADER iH;
    };
#pragma pack(pop)
}

image_read_func img_reader_s::detect_bmp_format(const void *sourcebuf, aint sourcebufsize)
{
    Header *header = (Header*)sourcebuf;

    if (header->fH.bfType != MAKEWORD('B', 'M')) return nullptr;
    if (header->iH.biCompression != 0) return nullptr;
    if (header->iH.biBitCount != 24 && header->iH.biBitCount != 32) return nullptr;

    bmpread_s & br = ref_cast<bmpread_s>(data);

    br.iH = &header->iH;
    br.ibuflen = (int)(sourcebufsize - sizeof(BITMAPFILEHEADER));

    size.x = header->iH.biWidth;
    size.y = header->iH.biHeight;
    bitpp = (uint8)header->iH.biBitCount;

    return bmpdatareader;
}


bool save_to_bmp_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options)
{
    buf.clear();

    BITMAPFILEHEADER bmFileHeader = {};
    BITMAPINFOHEADER bmInfoHeader = {};

    if (bmp.info().bytepp() != 1 && bmp.info().bytepp() != 3 && bmp.info().bytepp() != 4) return false;
    if ((bmp.info().bitpp >> 3) != bmp.info().bytepp()) return false;

    int lPitch = (int)(((bmp.info().bytepp()*bmp.info().sz.x - 1) / 4 + 1) * 4);

    bmFileHeader.bfType = 0x4D42;
    bmFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + lPitch*bmp.info().sz.y;
    bmFileHeader.bfOffBits = bmFileHeader.bfSize - lPitch*bmp.info().sz.y;

    buf.append_buf(&bmFileHeader, sizeof(BITMAPFILEHEADER));

    bmInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmInfoHeader.biWidth = bmp.info().sz.x;
    bmInfoHeader.biHeight = bmp.info().sz.y;
    bmInfoHeader.biPlanes = 1;
    bmInfoHeader.biBitCount = (uint16)bmp.info().bitpp;
    bmInfoHeader.biCompression = 0;
    bmInfoHeader.biSizeImage = lPitch*bmp.info().sz.y;
    buf.append_buf(&bmInfoHeader, sizeof(BITMAPINFOHEADER));

    uint8 * sou = bmp() + (bmp.info().sz.y - 1)*bmp.info().pitch;

    aint len = bmp.info().bytepp()*bmp.info().sz.x;
    if (bmp.info().bytepp() == 3)
    {
        for (int y = 0; y < bmp.info().sz.y; y++, sou -= bmp.info().pitch)
        {
            uint8 *sb = sou;
            uint8 *db = (uint8 *)buf.expand(len);
            uint8 *de = db + len;

            for (; db < de; db += 3, sb += 3)
            {
                *(db + 0) = *(sb + 0);
                *(db + 1) = *(sb + 1);
                *(db + 2) = *(sb + 2);
            }

            if (len < lPitch)
            {
                aint sz = lPitch - len;
                memset(buf.expand(sz), 0, sz);
            }
        }
    }
    else if (bmp.info().bytepp() == 4)
    {
        for (int y = 0; y < bmp.info().sz.y; y++, sou -= bmp.info().pitch)
        {
            uint8 *sb = sou;
            uint8 *db = (uint8 *)buf.expand(len);
            uint8 *de = db + len;

            for (; db < de; db += 4, sb += 4)
            {
                *(db + 0) = *(sb + 0);
                *(db + 1) = *(sb + 1);
                *(db + 2) = *(sb + 2);
                *(db + 3) = *(sb + 3);
            }

            if (len < lPitch)
            {
                aint sz = lPitch - len;
                memset(buf.expand(sz), 0, sz);
            }
        }

    }
    else
    {
        for (int y = 0; y < bmp.info().sz.y; y++, sou -= bmp.info().pitch)
        {
            buf.append_buf(sou, len);
            if (len < lPitch)
            {
                aint sz = lPitch - len;
                memset(buf.expand(sz), 0, sz);
            }
        }
    }

    return true;
}



} // namespace ts