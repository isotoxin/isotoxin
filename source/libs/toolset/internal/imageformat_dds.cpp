#include "toolset.h"
#include "fourcc.h"

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

#ifdef _WIN32
#pragma pack(push, 1)
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
#pragma pack(pop)
#endif

#pragma warning (push)
#pragma warning (disable:4201) //: nonstandard extension used : nameless struct/union)
struct Color8888
{
    union
    {
        struct
        {
            uint8 b;
            uint8 g;
            uint8 r;
            uint8 a;
        };
        uint32 col;
    };

};
#pragma warning (pop)

struct Color565
{
    unsigned nBlue  : 5;        // order of names changes
    unsigned nGreen : 6;        //  byte order of output to 32 bit
    unsigned nRed   : 5;
};

struct dds_decompressor_s
{
    const DDSHEAD * dds;
};

struct dds_decompressor_uncompressed_32_s : public dds_decompressor_s
{
    dds_decompressor_uncompressed_32_s():dds_decompressor_s() {}
    static bool decompress( img_reader_s &r, void * buf, int pitch );
};

struct dds_decompressor_dxt1_s : public dds_decompressor_s
{
    dds_decompressor_dxt1_s():dds_decompressor_s() {}
    static bool decompress( img_reader_s &r, void * buf, int pitch );
};

struct dds_decompressor_dxt3_s : public dds_decompressor_s
{
    dds_decompressor_dxt3_s() :dds_decompressor_s() {}
    static bool decompress( img_reader_s &r, void * buf, int pitch);
};

struct dds_decompressor_dxt5_s : public dds_decompressor_s
{
    dds_decompressor_dxt5_s() :dds_decompressor_s() {}
    static bool decompress( img_reader_s &r, void * buf, int pitch);
};

image_read_func img_reader_s::detect_dds_format(const void *sourcebuf, int sourcebufsize)
{
    const DDSHEAD *dds = (const DDSHEAD *)sourcebuf;
    if (dds->Signature != 0x20534444) return nullptr;

    if (dds->Flags2 & DDS_FOURCC)
    {
        switch (dds->FourCC)
        {
            case MAKEFOURCC('D','X','T','1'):
                ((dds_decompressor_dxt1_s *)&data)->dds = dds;
                size.x = dds->Width;
                size.y = dds->Height;
                bitpp = 32;
                return dds_decompressor_dxt1_s::decompress;

            //case IL_MAKEFOURCC('D','X','T','2'):
            //    break;

            case MAKEFOURCC('D','X','T','3'):
                ((dds_decompressor_dxt3_s *)&data)->dds = dds;
                size.x = dds->Width;
                size.y = dds->Height;
                bitpp = 32;
                return dds_decompressor_dxt3_s::decompress;

            //case IL_MAKEFOURCC('D','X','T','4'):
            //    break;

            case MAKEFOURCC('D','X','T','5'):
                ((dds_decompressor_dxt5_s *)&data)->dds = dds;
                size.x = dds->Width;
                size.y = dds->Height;
                bitpp = 32;
                return dds_decompressor_dxt5_s::decompress;

            default:
                return nullptr;
        }
    } else
    {
        if (dds->RGBBitCount == 32)
        {
            ((dds_decompressor_uncompressed_32_s *)&data)->dds = dds;
            size.x = dds->Width;
            size.y = dds->Height;
            bitpp = 32;
            return dds_decompressor_uncompressed_32_s::decompress;
        } else {
        }
    }

    return nullptr;


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





bool dds_decompressor_uncompressed_32_s::decompress( img_reader_s &reader, void * buf, int pitch )
{
    dds_decompressor_uncompressed_32_s &me = ref_cast<dds_decompressor_uncompressed_32_s>(reader.data);

    uint ReadI, RedL, RedR, GreenL, GreenR, BlueL, BlueR, AlphaL, AlphaR;
    const uint32 *Temp;
    uint32        *out = (uint32 *)buf;

    GetBitsFromMask(me.dds->RBitMask, &RedL, &RedR);
    GetBitsFromMask(me.dds->GBitMask, &GreenL, &GreenR);
    GetBitsFromMask(me.dds->BBitMask, &BlueL, &BlueR);
    GetBitsFromMask(me.dds->RGBAlphaBitMask, &AlphaL, &AlphaR);
    Temp = (const uint32 *)(me.dds + 1);

    //int sz = dds->Width * dds->Height * 4;

    //int temp_w = dds->Width;
    for (uint y = 0; y < me.dds->Height; ++y)
    {
        uint32 * next_out = (uint32 *)(((uint8 *)out) + pitch);
        for (uint x = 0; x < me.dds->Width; ++x, ++out, ++Temp)
        {
            ReadI = *Temp;

            uint32 r = ((ReadI & me.dds->RBitMask) >> RedR) << RedL;
            uint32 g = ((ReadI & me.dds->GBitMask) >> GreenR) << GreenL;
            uint32 b = ((ReadI & me.dds->BBitMask) >> BlueR) << BlueL;
            uint32 a = ((ReadI & me.dds->RGBAlphaBitMask) >> AlphaR) << AlphaL;

            *out = (a << 24) | (r << 16) | (g << 8) | (b << 0);
        }
        out = next_out;
    }

    return 1;

}

bool dds_decompressor_dxt1_s::decompress( img_reader_s&r, void * buf, int pitch )
{
    dds_decompressor_dxt1_s &me = ref_cast<dds_decompressor_dxt1_s>(r.data);

    int         Select;
    Color565    *color_0, *color_1;
    Color8888   colours[4], *col;
    uint        bitmask;

    const uint8 *Temp = (const uint8 *)(me.dds + 1);

    //for (z = 0; z < Depth; z++) { // mip maps ?
    {
        int w = (int)me.dds->Width;
        int h = (int)me.dds->Height;
        for (int y = 0; y < h; y += 4) {
            for (int x = 0; x < w; x += 4) {

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

                uint8 *out = (uint8 *)buf;

                for (int j = 0, k = 0; j < 4; j++) {
                    for (int i = 0; i < 4; i++, k++) {

                        Select = (bitmask & (0x03 << k*2)) >> k*2;
                        col = &colours[Select];

                        if (((x + i) < w) && ((y + j) < h))
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

/* -----------------------------------------------------------------------------

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the 
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to 
	permit persons to whom the Software is furnished to do so, subject to 
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	
   -------------------------------------------------------------------------- */

typedef uint8 u8;

static int Unpack565(u8 const* packed, u8* colour)
{
    // build the packed value
    int value = (int)packed[0] | ((int)packed[1] << 8);

    // get the components in the stored range
    u8 red = (u8)((value >> 11) & 0x1f);
    u8 green = (u8)((value >> 5) & 0x3f);
    u8 blue = (u8)(value & 0x1f);

    // scale up to 8 bits
    colour[0] = (red << 3) | (red >> 2);
    colour[1] = (green << 2) | (green >> 4);
    colour[2] = (blue << 3) | (blue >> 2);
    colour[3] = 255;

    // return the value
    return value;
}

static void DecompressColour(u8* rgba, void const* block /*, bool isDxt1*/)
{
    // get the block bytes
    u8 const* bytes = reinterpret_cast<u8 const*>(block);

    // unpack the endpoints
    u8 codes[16];
    /*int a =*/ Unpack565(bytes, codes);
    /*int b =*/ Unpack565(bytes + 2, codes + 4);

    // generate the midpoints
    for (int i = 0; i < 3; ++i)
    {
        int c = codes[i];
        int d = codes[4 + i];

        /* if (a <= b)
        {
            codes[8 + i] = (u8)((c + d) / 2);
            codes[12 + i] = 0;
        }
        else */
        {
            codes[8 + i] = (u8)((2 * c + d) / 3);
            codes[12 + i] = (u8)((c + 2 * d) / 3);
        }
    }

    // fill in alpha for the intermediate values
    codes[8 + 3] = 255;
    codes[12 + 3] = /*(isDxt1 && a <= b) ? 0 :*/ 255;

    // unpack the indices
    u8 indices[16];
    for (int i = 0; i < 4; ++i)
    {
        u8* ind = indices + 4 * i;
        u8 packed = bytes[4 + i];

        ind[0] = packed & 0x3;
        ind[1] = (packed >> 2) & 0x3;
        ind[2] = (packed >> 4) & 0x3;
        ind[3] = (packed >> 6) & 0x3;
    }

    // store out the colours
    for (int i = 0; i < 16; ++i)
    {
        u8 offset = 4 * indices[i];
        for (int j = 0; j < 4; ++j)
            rgba[4 * i + j] = codes[offset + j];
    }
}

void DecompressAlphaDxt3(u8* rgba, void const* block)
{
    u8 const* bytes = reinterpret_cast<u8 const*>(block);

    // unpack the alpha values pairwise
    for (int i = 0; i < 8; ++i)
    {
        // quantise down to 4 bits
        u8 quant = bytes[i];

        // unpack the values
        u8 lo = quant & 0x0f;
        u8 hi = quant & 0xf0;

        // convert back up to bytes
        rgba[8 * i + 3] = lo | (lo << 4);
        rgba[8 * i + 7] = hi | (hi >> 4);
    }
}

void DecompressAlphaDxt5(u8* rgba, void const* block)
{
    // get the two alpha values
    u8 const* bytes = reinterpret_cast<u8 const*>(block);
    int alpha0 = bytes[0];
    int alpha1 = bytes[1];

    // compare the values to build the codebook
    u8 codes[8];
    codes[0] = (u8)alpha0;
    codes[1] = (u8)alpha1;
    if (alpha0 <= alpha1)
    {
        // use 5-alpha codebook
        for (int i = 1; i < 5; ++i)
            codes[1 + i] = (u8)(((5 - i)*alpha0 + i*alpha1) / 5);
        codes[6] = 0;
        codes[7] = 255;
    }
    else
    {
        // use 7-alpha codebook
        for (int i = 1; i < 7; ++i)
            codes[1 + i] = (u8)(((7 - i)*alpha0 + i*alpha1) / 7);
    }

    // decode the indices
    u8 indices[16];
    u8 const* src = bytes + 2;
    u8* dest = indices;
    for (int i = 0; i < 2; ++i)
    {
        // grab 3 bytes
        int value = 0;
        for (int j = 0; j < 3; ++j)
        {
            int b = *src++;
            value |= (b << (8 * j));
        }

        // unpack 8 3-bit values from it
        for (int j = 0; j < 8; ++j)
        {
            int index = (value >> (3 * j)) & 0x7;
            *dest++ = (u8)index;
        }
    }

    // write out the indexed codebook values
    for (int i = 0; i < 16; ++i)
        rgba[4 * i + 3] = codes[indices[i]];
}

bool dds_decompressor_dxt3_s::decompress(img_reader_s &r, void * buf, int pitch)
{
    dds_decompressor_dxt3_s &me = ref_cast<dds_decompressor_dxt3_s>(r.data);

    const uint8 *blocks = (const uint8 *)(me.dds + 1);
    uint8 *rgba = (uint8 *)buf;

    u8 const* sourceBlock = reinterpret_cast<u8 const*>(blocks);

    int w = (int)me.dds->Width;
    int h = (int)me.dds->Height;
    for (int y = 0; y < h; y += 4) {
        for (int x = 0; x < w; x += 4) {

            // decompress the block
            u8 targetRgba[4 * 16];

            // decompress colour
            DecompressColour(targetRgba, sourceBlock + 8);
            DecompressAlphaDxt3(targetRgba, sourceBlock);

            // write the decompressed pixels to the correct image locations
            u8 const* sourcePixel = targetRgba;
            for (int py = 0; py < 4; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    // get the target location
                    int sx = x + px;
                    int sy = y + py;
                    if (sx < w && sy < h)
                    {
                        u8* targetPixel = rgba + 4 * sx + pitch * sy;

                        // copy the rgba value
                        for (int i = 0; i < 4; ++i)
                            *targetPixel++ = *sourcePixel++;
                    }
                    else
                    {
                        // skip this pixel as its outside the image
                        sourcePixel += 4;
                    }
                }
            }

            // advance
            sourceBlock += 16;

        }
    }

    return true;

}

bool dds_decompressor_dxt5_s::decompress(img_reader_s &r, void * buf, int pitch)
{
    dds_decompressor_dxt5_s &me = ref_cast<dds_decompressor_dxt5_s>(r.data);

    const uint8 *blocks = (const uint8 *)(me.dds + 1);
    uint8 *rgba = (uint8 *)buf;

    u8 const* sourceBlock = reinterpret_cast<u8 const*>(blocks);

    int w = (int)me.dds->Width;
    int h = (int)me.dds->Height;
    for (int y = 0; y < h; y += 4) {
        for (int x = 0; x < w; x += 4) {

            // decompress the block
            u8 targetRgba[4 * 16];

            // decompress colour
            DecompressColour(targetRgba, sourceBlock+8 /*, (flags & kDxt1) != 0*/);

            // decompress alpha separately if necessary
            DecompressAlphaDxt5(targetRgba, sourceBlock);

            // write the decompressed pixels to the correct image locations
            u8 const* sourcePixel = targetRgba;
            for (int py = 0; py < 4; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    // get the target location
                    int sx = x + px;
                    int sy = y + py;
                    if (sx < w && sy < h)
                    {
                        u8* targetPixel = rgba + 4 * sx + pitch * sy;

                        // copy the rgba value
                        for (int i = 0; i < 4; ++i)
                            *targetPixel++ = *sourcePixel++;
                    }
                    else
                    {
                        // skip this pixel as its outside the image
                        sourcePixel += 4;
                    }
                }
            }

            // advance
            sourceBlock += 16;

       }
    }

    return true;
}

bool save_to_dds_format(buf_c &buf, const bmpcore_exbody_s &bmp, int options)
{
    buf.clear();
    return false;
}

/*

void save_to_dds_format(buf_c & buf, int compression, int additional_squish_flags)
{
    if (info().bytepp() != 3 && info().bytepp() != 4)
    {
        DEBUG_BREAK(); //ERROR(L"Unsupported bitpp to convert");
    }

    int sz = info().sz.x * info().sz.y * info().bytepp();
	int squishc = squish::kDxt5;
	if (compression)
	{
		switch (compression)
		{
		default:
			sux("wrong DDS compression bitpp, using DXT1");
		case D3DFMT_DXT1:
			squishc = squish::kDxt1;
			break;
		case D3DFMT_DXT3:
			squishc = squish::kDxt3;
			break;
		case D3DFMT_DXT5:
			squishc = squish::kDxt5;
			break;
		}
		sz = squish::GetStorageRequirements(info().sz.x, info().sz.y, squishc);
	}

    buf.clear();
    uint32 *dds = (uint32 *)buf.expand(sz + sizeof(DDSURFACEDESC2) + sizeof(uint32));

    *dds = 0x20534444; // "DDS "

    DDSURFACEDESC2 * ddsp = (DDSURFACEDESC2 *)(dds + 1);
    memset(ddsp, 0, sizeof(DDSURFACEDESC2));
    
    ddsp->dwSize = sizeof(DDSURFACEDESC2);
    ddsp->dwWidth = info().sz.x;
    ddsp->dwHeight = info().sz.y;
    ddsp->dwFlags = DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_LINEARSIZE;

    ddsp->dwLinearSize = sz;
    ddsp->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	ddsp->ddpfPixelFormat.dwFlags = (compression == 0 ? DDPF_RGB : DDPF_FOURCC) | ((info().bytepp() == 4)?DDPF_ALPHAPIXELS:0);
    ddsp->ddpfPixelFormat.dwRGBBitCount = info().bitpp;
    ddsp->ddpfPixelFormat.dwRBitMask = 0x00FF0000;
    ddsp->ddpfPixelFormat.dwGBitMask = 0x0000FF00;
    ddsp->ddpfPixelFormat.dwBBitMask = 0x000000FF;
    ddsp->ddpfPixelFormat.dwRGBAlphaBitMask = 0xFF000000;
	ddsp->ddpfPixelFormat.dwFourCC = compression;

    ddsp->ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    if (info().sz.x < 4)
    {
		sux("!-!");
        int cnt = info().sz.x * info().sz.y;
        BYTE *des = (BYTE *)(ddsp + 1);
        const uint8 *sou = body();
        if (info().bytepp() == 3)
        {
            while (cnt-- > 0)
            {
                //*des = *(sou + 2);
                //*(des+1) = *(sou + 1);
                //*(des+2) = *(sou + 0);
                *des = *(sou + 0);
                *(des+1) = *(sou + 1);
                *(des+2) = *(sou + 2);
                *(des+3) = 0xFF;

                sou += 3;
                des += 4;
            }
        } else
        {
            while (cnt-- > 0)
            {
                //*des = *(sou + 2);
                //*(des+1) = *(sou + 1);
                //*(des+2) = *(sou + 0);
                //*(des+3) = *(sou + 3);
                *des = *(sou + 0);
                *(des+1) = *(sou + 1);
                *(des+2) = *(sou + 2);
                *(des+3) = *(sou + 3);

                sou += 4;
                des += 4;
            }

        }

    } else
    {
		if (compression == 0)
			memcpy( ddsp + 1, body(), info().sz.x * info().sz.y * info().bytepp() );
		else
		{
			squish::u8 *buf = (squish::u8*)MM_ALLOC(slmemcat::squish_buff, info().sz.x * info().sz.y * info().bytepp());
			swap_byte(buf);
			squish::CompressImage(buf, info().sz.x, info().sz.y, ddsp + 1, squishc | additional_squish_flags);
			MM_FREE(slmemcat::squish_buff, buf);
		}
        //swap_byte( ddsp + 1 );

    }
    //memcpy(des,sou,sz);
}
#pragma warning (pop)
*/

}