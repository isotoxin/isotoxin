#include "toolset.h"
#include "FileDDS.h"
#include <ddraw.h>

namespace ts
{

// Defines

//Those 4 were added on 20040516 to make
//the written dds files more standard compliant
#define DDS_CAPS                0x00000001l
#define DDS_HEIGHT              0x00000002l
#define DDS_WIDTH               0x00000004l
#define DDS_PIXELFORMAT         0x00001000l

#define DDS_ALPHAPIXELS         0x00000001l
#define DDS_ALPHA               0x00000002l
#define DDS_FOURCC              0x00000004l
#define DDS_PITCH               0x00000008l
#define DDS_COMPLEX             0x00000008l
#define DDS_TEXTURE             0x00001000l
#define DDS_MIPMAPCOUNT         0x00020000l
#define DDS_LINEARSIZE          0x00080000l
#define DDS_VOLUME              0x00200000l
#define DDS_MIPMAP              0x00400000l
#define DDS_DEPTH               0x00800000l

#define DDS_CUBEMAP             0x00000200L
#define DDS_CUBEMAP_POSITIVEX   0x00000400L
#define DDS_CUBEMAP_NEGATIVEX   0x00000800L
#define DDS_CUBEMAP_POSITIVEY   0x00001000L
#define DDS_CUBEMAP_NEGATIVEY   0x00002000L
#define DDS_CUBEMAP_POSITIVEZ   0x00004000L
#define DDS_CUBEMAP_NEGATIVEZ   0x00008000L


#define IL_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
            ((int)(byte)(ch0) | ((int)(byte)(ch1) << 8) |   \
            ((int)(byte)(ch2) << 16) | ((int)(byte)(ch3) << 24 ))


#ifdef _WIN32
    #pragma pack(push, dds_struct, 1)
#endif
struct DDSHEAD
{
    uint  Signature;

    uint  Size1;              // size of the structure (minus MagicNum)
    uint  Flags1;             // determines what fields are valid
    uint  Height;             // height of surface to be created
    uint  Width;              // width of input surface
    uint  LinearSize;         // Formless late-allocated optimized surface size
    uint  Depth;              // Depth if a volume texture
    uint  MipMapCount;        // number of mip-map levels requested
    uint  AlphaBitDepth;      // depth of alpha buffer requested

    uint  NotUsed[10];

    uint  Size2;              // size of structure
    uint  Flags2;             // pixel format flags
    uint  FourCC;             // (FOURCC code)
    uint  RGBBitCount;        // how many bits per pixel
    uint  RBitMask;           // mask for red bit
    uint  GBitMask;           // mask for green bits
    uint  BBitMask;           // mask for blue bits
    uint  RGBAlphaBitMask;    // mask for alpha channel
    
    uint  ddsCaps1, ddsCaps2, ddsCaps3, ddsCaps4; // direct draw surface capabilities
    uint  TextureStage;
};
#ifdef _WIN32
    #pragma pack(pop, dds_struct)
#endif

#pragma warning (push)
#pragma warning (disable:4201) //: nonstandard extension used : nameless struct/union)
typedef struct Color8888
{
    union
    {
        struct
        {
            byte b;
            byte g;
            byte r;
            byte a;
        };
        uint32 col;
    };

} Color8888;
#pragma warning (pop)

typedef struct Color565
{
    unsigned nBlue  : 5;        // order of names changes
    unsigned nGreen : 6;        //  byte order of output to 32 bit
    unsigned nRed   : 5;
} Color565;



struct dds_decompressor_s
{
    const DDSHEAD * dds;
    size_t          blocksize;

    dds_decompressor_s( uint32 _blocksize ):blocksize( _blocksize ) {};

    virtual size_t decompress( void * buf, uint32 pitch ) = 0;

private:
    dds_decompressor_s(void) {}
};

struct dds_decompressor_uncompressed_32_s : public dds_decompressor_s
{

    dds_decompressor_uncompressed_32_s( uint32 _blocksize ):dds_decompressor_s(_blocksize) {}

    virtual size_t decompress( void * buf, uint32 pitch );
};

struct dds_decompressor_dxt1_s : public dds_decompressor_s
{

    dds_decompressor_dxt1_s( uint32 _blocksize ):dds_decompressor_s(_blocksize) {}

    virtual size_t decompress( void * buf, uint32 pitch );
};

size_t FileDDS_ReadStart_Buf(const void * soubuf,aint soubuflen,ivec2 &sz, uint8 &bitpp)
{
    const DDSHEAD *dds = (const DDSHEAD *)soubuf;
    if (dds->Signature != 0x20534444) return 0;

    dds_decompressor_s *dec = nullptr;

    if (dds->Flags2 & DDS_FOURCC)
    {
        int BlockSize = ((dds->Width + 3)/4) * ((dds->Height + 3)/4) * ((dds->Depth + 3)/4);
        switch (dds->FourCC)
        {
            case IL_MAKEFOURCC('D','X','T','1'):
                dec = TSNEW( dds_decompressor_dxt1_s, BlockSize * 8);
                bitpp = 32;
                break;

            case IL_MAKEFOURCC('D','X','T','2'):
                //dec = SLNEW dds_decompressor_dxt2_s(BlockSize * 16);
                bitpp = 32;
                break;

            case IL_MAKEFOURCC('D','X','T','3'):
                //dec = SLNEW dds_decompressor_dxt3_s(BlockSize * 16);
                bitpp = 32;
                break;

            case IL_MAKEFOURCC('D','X','T','4'):
                //dec = SLNEW dds_decompressor_dxt4_s(BlockSize * 16);
                bitpp = 32;
                break;

            case IL_MAKEFOURCC('D','X','T','5'):
                //dec = SLNEW dds_decompressor_dxt5_s(BlockSize * 16);
                bitpp = 32;
                break;

            default:
                return 0;
                break;
        }
    } else
    {
        int BlockSize = (dds->Width * dds->Height * dds->Depth * (dds->RGBBitCount >> 3));
        if (dds->RGBBitCount == 32)
        {
            dec = TSNEW( dds_decompressor_uncompressed_32_s, BlockSize );
            bitpp = 32;
        } else {
            // dec = TSNEW( dds_decompressor_uncompressed_24_s, BlockSize );
            //*format = bitmap_c::BMF_32;
        }
    }

    dec->dds = dds;

    sz.x = dds->Width;
    sz.y = dds->Height;

    return (size_t)dec;


}

size_t FileDDS_Read(size_t id,void * buf,aint pitch)
{
    dds_decompressor_s *dec = (dds_decompressor_s *)id;
    size_t ret = dec->decompress( buf, pitch );
    TSDEL(dec);
    return ret;
}



static void GetBitsFromMask(uint Mask, uint *ShiftLeft, uint *ShiftRight)
{
    uint Temp, i;

    if (Mask == 0) {
        *ShiftLeft = *ShiftRight = 0;
        return;
    }

    Temp = Mask;
    for (i = 0; i < 32; i++, Temp >>= 1) {
        if (Temp & 1)
            break;
    }
    *ShiftRight = i;

    // Temp is preserved, so use it again:
    for (i = 0; i < 8; i++, Temp >>= 1) {
        if (!(Temp & 1))
            break;
    }
    *ShiftLeft = 8 - i;

    return;
}





size_t dds_decompressor_uncompressed_32_s::decompress( void * buf, uint32 pitch )
{
    uint ReadI, RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
    const uint32 *Temp;
    uint32        *out = (uint32 *)buf;

    GetBitsFromMask(dds->RBitMask, &RedL, &RedR);
    GetBitsFromMask(dds->GBitMask, &GreenL, &GreenR);
    GetBitsFromMask(dds->BBitMask, &BlueL, &BlueR);
    GetBitsFromMask(dds->RGBAlphaBitMask, &AlphaL, &AlphaR);
    Temp = (const uint32 *)(dds + 1);

    //int sz = dds->Width * dds->Height * 4;

    //int temp_w = dds->Width;
    for (uint y = 0; y < dds->Height; ++y)
    {
        uint32 * next_out = (uint32 *)(((byte *)out) + pitch);
        for (uint x = 0; x < dds->Width; ++x, ++out, ++Temp)
        {
            ReadI = *Temp;

            uint32 r = ((ReadI & dds->RBitMask) >> RedR) << RedL;
            uint32 g = ((ReadI & dds->GBitMask) >> GreenR) << GreenL;
            uint32 b = ((ReadI & dds->BBitMask) >> BlueR) << BlueL;
            uint32 a = ((ReadI & dds->RGBAlphaBitMask) >> AlphaR) << AlphaL;

            *out = (a << 24) | (r << 16) | (g << 8) | (b << 0);
        }
        out = next_out;
    }

    return 1;

}

size_t dds_decompressor_dxt1_s::decompress( void * buf, uint32 pitch )
{
    int         /*z,*/ k, Select;
    uint        i, j, x, y;
    Color565    *color_0, *color_1;
    Color8888   colours[4], *col;
    uint        bitmask;

    const byte     *Temp = (const byte *)(dds + 1);

    //for (z = 0; z < Depth; z++) { // mip maps ?
    {
        for (y = 0; y < dds->Height; y += 4) {
            for (x = 0; x < dds->Width; x += 4) {

                color_0 = ((Color565*)Temp);
                color_1 = ((Color565*)(Temp+2));
                bitmask = ((uint*)Temp)[1];
                Temp += 8;

                colours[0].r = as_byte( color_0->nRed << 3 );
                colours[0].g = as_byte( color_0->nGreen << 2 );
                colours[0].b = as_byte( color_0->nBlue << 3 );
                colours[0].a = 0xFF;

                colours[1].r = as_byte( color_1->nRed << 3 );
                colours[1].g = as_byte( color_1->nGreen << 2 );
                colours[1].b = as_byte( color_1->nBlue << 3 );
                colours[1].a = 0xFF;


                if (*((word*)color_0) > *((word*)color_1))
                {
                    // Four-color block: derive the other two colors.    
                    // 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
                    // These 2-bit codes correspond to the 2-bit fields 
                    // stored in the 64-bit block.
                    colours[2].b = (2 * colours[0].b + colours[1].b + 1) / 3;
                    colours[2].g = (2 * colours[0].g + colours[1].g + 1) / 3;
                    colours[2].r = (2 * colours[0].r + colours[1].r + 1) / 3;
                    colours[2].a = 0xFF;

                    colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
                    colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
                    colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
                    colours[3].a = 0xFF;
                }    
                else { 
                    // Three-color block: derive the other color.
                    // 00 = color_0,  01 = color_1,  10 = color_2,
                    // 11 = transparent.
                    // These 2-bit codes correspond to the 2-bit fields 
                    // stored in the 64-bit block. 
                    colours[2].b = (colours[0].b + colours[1].b) / 2;
                    colours[2].g = (colours[0].g + colours[1].g) / 2;
                    colours[2].r = (colours[0].r + colours[1].r) / 2;
                    colours[2].a = 0xFF;

                    colours[3].b = (colours[0].b + 2 * colours[1].b + 1) / 3;
                    colours[3].g = (colours[0].g + 2 * colours[1].g + 1) / 3;
                    colours[3].r = (colours[0].r + 2 * colours[1].r + 1) / 3;
                    colours[3].a = 0x00;
                }

                byte        *out = (byte *)buf;

                for (j = 0, k = 0; j < 4; j++) {
                    for (i = 0; i < 4; i++, k++) {

                        Select = (bitmask & (0x03 << k*2)) >> k*2;
                        col = &colours[Select];

                        if (((x + i) < dds->Width) && ((y + j) < dds->Height))
                        {
                            //Offset = z * Image->SizeOfPlane + (y + j) * Image->Bps + (x + i) * Image->Bpp;
                            int Offset = (y + j) * pitch + (x + i) * 4;

                            *(uint32 *)(out + Offset) = col->col;
                        }
                    }
                }
            }
        }
    }

    return 1;

}





}