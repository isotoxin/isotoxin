#include "toolset.h"
#include "internal/platform.h"
#include "qrencode.h"

//-V:bytepp:112
//-V:bitpp:112

namespace ts
{

    // YUV to RGB conversion constants.
    // Y contribution to R,G,B.  Scale and bias.
    // TODO(fbarchard): Consider moving constants into a common header.
#define YG 18997 /* round(1.164 * 64 * 256 * 256 / 257) */
#define YGB 1160 /* 1.164 * 64 * 16 - adjusted for even error distribution */

    // U and V contributions to R,G,B.
#define UB -128 /* -min(128, round(2.018 * 64)) */
#define UG 25 /* -round(-0.391 * 64) */
#define VG 52 /* -round(-0.813 * 64) */
#define VR -102 /* -round(1.596 * 64) */

    // Bias values to subtract 16 from Y and 128 from U and V.
#define BB (UB * 128            - YGB)
#define BG (UG * 128 + VG * 128 - YGB)
#define BR            (VR * 128 - YGB)

#define RGB_TO_YUV(t)                                                                       \
    ( (0.257*(float)(t>>16)) + (0.504*(float)(t>>8&0xff)) + (0.098*(float)(t&0xff)) + 16),  \
    (-(0.148*(float)(t>>16)) - (0.291*(float)(t>>8&0xff)) + (0.439*(float)(t&0xff)) + 128), \
    ( (0.439*(float)(t>>16)) - (0.368*(float)(t>>8&0xff)) - (0.071*(float)(t&0xff)) + 128)

void TSCALL img_helper_rgb2yuv(uint8 *dst, const imgdesc_s &src_info, const uint8 *sou, yuv_fmt_e yuvfmt)
{
    switch (yuvfmt)
    {
    case ts::YFORMAT_I420:
        {
            int sz = src_info.sz.x * src_info.sz.y;
            uint8 *U_plane = dst + sz;
            uint8 *V_plane = U_plane + sz / 4;
            img_helper_ARGB_to_i420(sou, src_info.pitch, dst, src_info.sz.x, U_plane, src_info.sz.x/2, V_plane, src_info.sz.x/2, src_info.sz.x, src_info.sz.y);
        }
        break;
    }
}

void TSCALL img_helper_yuv2rgb(uint8 *des, const imgdesc_s &des_info, const uint8 *sou, yuv_fmt_e yuvfmt)
{
    switch (yuvfmt)
    {
    case ts::YFORMAT_YUY2:
        {
            for(int y = 0; y<des_info.sz.y; ++y, des += des_info.pitch)
            {
                TSCOLOR *dst = (TSCOLOR *)des;
                TSCOLOR *dste = dst + des_info.sz.x;
                for(;dst < dste;sou += 4, dst += 2)
                {
                    uint32 YUYV = *(uint32 *)sou;

                    int32 Y0 = (uint32)(( YUYV & 0xff ) * 0x0101 * YG) >> 16;
                    int32 Y1 = (uint32)(( (YUYV>>16) & 0xff ) * 0x0101 * YG) >> 16;

                    uint8 U = (YUYV >> 8) & 0xff;
                    uint8 V = YUYV >> 24;

                    int prer = BR - (V * VR);
                    int preg = BG - (V * VG + U * UG);
                    int preb = BB - (U * UB);

                    dst[0] = ts::ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
                    dst[1] = ts::ARGB<int>((prer + Y1) >> 6, (preg + Y1) >> 6, (preb + Y1) >> 6);
                }
            }
        }
        break;
    case ts::YFORMAT_I420:
        {
            int sz = des_info.sz.x * des_info.sz.y;
            const uint8 *U_plane = sou + sz;
            const uint8 *V_plane = U_plane + sz / 4;
            img_helper_i420_to_ARGB( sou, des_info.sz.x, U_plane, des_info.sz.x/2, V_plane, des_info.sz.x/2, des, des_info.pitch, des_info.sz.x, des_info.sz.y );
        }
        break;
    case ts::YFORMAT_I420x2:
        {
            ts::ivec2 yuv_size = des_info.sz * 2;
            const uint8 *Y_plane = sou;
            int sz = yuv_size.x * yuv_size.y;
            const uint8 *U_plane = sou + sz;
            const uint8 *V_plane = U_plane + sz / 4;

            for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch, Y_plane += yuv_size.x)
            {
                TSCOLOR *dst = (TSCOLOR *)des;
                TSCOLOR *dste = dst + des_info.sz.x;
                for (; dst < dste; ++U_plane, ++V_plane, ++dst, Y_plane += 2)
                {

                    int Y = ((int)Y_plane[0] + Y_plane[1] + Y_plane[0 + yuv_size.x] + Y_plane[1 + yuv_size.x]) / 4;
                    int32 Y0 = (uint32)(Y * 0x0101 * YG) >> 16;

                    uint8 U = *U_plane;
                    uint8 V = *V_plane;

                    int prer = BR - (V * VR);
                    int preg = BG - (V * VG + U * UG);
                    int preb = BB - (U * UB);

                    *dst = ts::ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
                }
            }
        }
        break;
    }
}

void TSCALL img_helper_mulcolor(uint8 *des, const imgdesc_s &des_info, TSCOLOR color)
{
    int desnl = des_info.pitch - des_info.sz.x * 4;
    for (int y = 0; y < des_info.sz.y; ++y, des += desnl)
        for (int x = 0; x < des_info.sz.x; ++x, des += 4)
            *(TSCOLOR *)des = MULTIPLY( color, *(TSCOLOR *)des);
}

void TSCALL img_helper_colorclamp(uint8 *des, const imgdesc_s &des_info)
{
    int desnl = des_info.pitch - des_info.sz.x * 4;
    for (int y = 0; y < des_info.sz.y; ++y, des += desnl)
    {
        for (int x = 0; x < des_info.sz.x; ++x, des += 4)
        {
            TSCOLOR ocolor;
            TSCOLOR color = *(TSCOLOR *)des;
            uint8 alpha = ALPHA(color);
            if (alpha == 255)
            {
                continue;
            } else if (alpha == 0)
            {
                ocolor = 0;
            } else
            {
                ocolor = color;
                if ( RED( color ) > alpha )
                    ocolor = (ocolor & 0xff00ffff) | ( alpha << 16 );
                if ( GREEN( color ) > alpha )
                    ocolor = (ocolor & 0xffff00ff) | ( alpha << 8 );
                if ( BLUE( color ) > alpha )
                    ocolor = (ocolor & 0xffffff00) | alpha;
            }

            *(TSCOLOR *)des = ocolor;
        }
    }
}

bool TSCALL img_helper_premultiply(uint8 *des, const imgdesc_s &des_info)
{
    int desnl = des_info.pitch - des_info.sz.x * 4;
    bool is_alpha = false;
    for (int y = 0; y < des_info.sz.y; ++y, des += desnl)
    {
        for (int x = 0; x < des_info.sz.x; ++x, des += 4)
        {
            TSCOLOR ocolor;
            TSCOLOR color = *(TSCOLOR *)des;
            uint8 alpha = ALPHA(color);
            if (alpha == 255)
            {
                continue;
            } else if (alpha == 0)
            {
                ocolor = 0;
                is_alpha = true;
            } else
            {
                ocolor = PREMULTIPLY(color);
                is_alpha = true;
            }

            *(TSCOLOR *)des = ocolor;
        }
    }
    return is_alpha;
}

void TSCALL img_helper_fill(uint8 *des, const imgdesc_s &des_info, TSCOLOR color)
{
    aint desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
    aint desnp = des_info.bytepp();

    switch (des_info.bytepp())
    {
    case 1:
        for (int y = 0; y < des_info.sz.y; y++, des += desnl)
            for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                *(uint8 *)des = (uint8)color;
        break;
    case 2:
        for (int y = 0; y < des_info.sz.y; y++, des += desnl)
            for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                *(uint16 *)des = (uint16)color;
        break;
    case 3:
        for (int y = 0; y < des_info.sz.y; y++, des += desnl) {
            for (int x = 0; x < des_info.sz.x; x++, des += desnp) {
                *(uint16 *)des = (uint16)color;
                *(uint8 *)(des + 2) = (uint8)(color >> 16);
            }
        }
        break;
    case 4:
        for (int y = 0; y < des_info.sz.y; y++, des += desnl)
            for (int x = 0; x < des_info.sz.x; x++, des += desnp)
                *(uint32 *)des = color;
        break;
    }
}

uint8 ALIGN(256) multbl[256][256]; // Im very surprised, but multiplication table is faster then raw (a * b / 255)
uint8 ALIGN(256) divtbl[256][256];

static const ALIGN(16) uint16 min255[16] = { 255, 255, 255, 255, 255, 255, 255, 255 };

//alphablend constats
static const ALIGN(16) uint8 preparealphas[16] = { 6, 255, 6, 255, 6, 255, 6, 255, 14, 255, 14, 255, 14, 255, 14, 255 };
static const ALIGN(16) uint16 sub256[16] = { 256, 256, 256, 256, 256, 256, 256, 256 };

namespace sse_consts
{
ALIGN(16) uint8 preparetgtc_1[16] = { 255, 0, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7 };
ALIGN(16) uint8 preparetgtc_2[16] = { 255, 8, 255, 9, 255, 10, 255, 11, 255, 12, 255, 13, 255, 14, 255, 15 };
};


class setup_tbls
{
public:
    setup_tbls()
    {
        for (int a = 0; a < 256; ++a)
            for (int c = 0; c < 256; ++c)
            {
                int k = a * c / 255;
                multbl[ a ][ c ] = (uint8)k;
            }

        for ( int a = 0; a < 256; ++a )
            for ( int c = 0; c < 256; ++c )
            {
                int k = 0;

                if ( a == 255 )
                    k = c;
                if ( a == 0)
                {

                } else if ( c <= a )
                {
                    for ( int cc = 255; cc >= 0; --cc)
                    {
                        if ( multbl[a][cc] == c )
                        {
                            k = cc;
                            break;
                        }
                    }
                } else
                {
                    k = 255;
                }

                divtbl[ a ][ c ] = (uint8)k;
            }


    }
} setup_multbl_;

static void overfill_row_sse( uint8 *dst_argb, int w, const uint16 *ctbl )
{
    for ( ; w > 0; w -= 8, dst_argb += 32 )
    {
        __m128i t4 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_1 );
        __m128i t5 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_2 );
        __m128i t6 = _mm_load_si128( ( const __m128i * )ctbl );
        __m128i t7 = _mm_load_si128( ( const __m128i * )( ctbl + 8 ) );

        __m128i t0 = _mm_lddqu_si128( ( const __m128i * )dst_argb );
        __m128i t2 = _mm_lddqu_si128( ( const __m128i * )(dst_argb+16) );

        _mm_storeu_si128( ( __m128i * )dst_argb, _mm_packus_epi16( _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t0, t4 ), t6 ), t7 ), _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t0, t5 ), t6 ), t7 ) ) );
        _mm_storeu_si128( ( __m128i * )(dst_argb + 16), _mm_packus_epi16( _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t2, t4 ), t6 ), t7 ), _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t2, t5 ), t6 ), t7 ) ) );
    }
}

void TSCALL img_helper_overfill(uint8 *des, const imgdesc_s &des_info, TSCOLOR color_pm)
{
    switch (des_info.bytepp())
    {
    case 3:
        {
            aint desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
            uint not_a = (255 - ALPHA(color_pm)) * 256 / 255;

            for (int y = 0; y < des_info.sz.y; y++, des += desnl)
            {
                for (int x = 0; x < des_info.sz.x; x++, des += 3)
                {

                    uint oiB = des[0] * not_a + BLUEx256(color_pm);
                    uint oiG = des[1] * not_a + GREENx256(color_pm);
                    uint oiR = des[2] * not_a + REDx256(color_pm);

                    *(uint16 *)des = CLAMP<uint8>(oiB >> 8) | (CLAMP<uint8>(oiG >> 8) << 8);
                    *(uint8 *)(des + 2) = (uint8)CLAMP<uint8>(oiR >> 8);

                }
            }
        }
        break;
    case 4:
        {
            auto overfill_row_no_clamp = [](uint8 *dst_argb, TSCOLOR c, int w)
            {
                /*

                // without use of table
                // it slower!

                uint not_a = 256 - ALPHA(c);
                for (int x = 0; x < w; ++x, dst_argb += 4)
                {
                    TSCOLOR cdst = *(TSCOLOR *)dst_argb;
                    *(TSCOLOR *)dst_argb = c + (((not_a * BLUEx256(cdst))>>16) |
                                            (((not_a * GREENx256(cdst))>>8) & 0xff00) |
                                            (((not_a * REDx256(cdst)))  & 0xff0000) |
                                            (((not_a * ALPHAx256(cdst))<<8) & 0xff000000));
                }
                */


                uint8 not_a = 255 - ALPHA(c);
                for (int x = 0; x < w; ++x, dst_argb += 4)
                {
                    TSCOLOR cdst = *(TSCOLOR *)dst_argb;

                    *(TSCOLOR *)dst_argb = c + ((multbl[not_a][cdst & 0xff]) |
                                (((uint)multbl[not_a][(cdst >> 8) & 0xff]) << 8) |
                                (((uint)multbl[not_a][(cdst >> 16) & 0xff]) << 16) |
                                (((uint)multbl[not_a][(cdst >> 24) & 0xff]) << 24));
                }

            };

            auto overfill_row = [](uint8 *dst_argb, TSCOLOR c, int w)
            {
                uint8 not_a = 255 - ALPHA(c);
                for (int x = 0; x < w; ++x, dst_argb += 4)
                {
                    TSCOLOR cdst = *(TSCOLOR *)dst_argb;

                    uint B = multbl[not_a][BLUE(cdst)] + BLUE(c);
                    uint G = multbl[not_a][GREEN(cdst)] + GREEN(c);
                    uint R = multbl[not_a][RED(cdst)] + RED(c);
                    uint A = multbl[not_a][ALPHA(cdst)] + ALPHA(c);

                    *(TSCOLOR *)dst_argb = CLAMP<uint8>(B) | (CLAMP<uint8>(G) << 8) | (CLAMP<uint8>(R) << 16) | (A << 24);
                }
            };

            if (CCAPS(CPU_SSSE3))
            {
                uint16 na = 256 - ALPHA(color_pm);

                ALIGN(16) uint16 xtabl[16] =
                {
                    na, na, na, na, na, na, na, na,  // 2 pixels x (1-A)
                    BLUE(color_pm), GREEN(color_pm), RED(color_pm), ALPHA(color_pm), BLUE(color_pm), GREEN(color_pm), RED(color_pm), ALPHA(color_pm) // add color
                };

                int w = des_info.sz.x & ~7;
                uint8 * dst_sse = des;

                if (PREMULTIPLIED(color_pm))
                {
                    if (ALPHA(color_pm))
                    {
                        if (w)
                            for (int y = 0; y < des_info.sz.y; ++y, dst_sse += des_info.pitch)
                                overfill_row_sse(dst_sse, w, xtabl);

                        if (int ost = (des_info.sz.x & 7))
                        {
                            des += w * 4;
                            for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch)
                                overfill_row_no_clamp(des, color_pm, ost);
                        }
                    }
                } else
                {
                    if (w)
                        for (int y = 0; y < des_info.sz.y; ++y, dst_sse += des_info.pitch)
                            overfill_row_sse(dst_sse, w, xtabl);

                    if (int ost = (des_info.sz.x & 7))
                    {
                        des += w * 4;
                        for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch)
                            overfill_row(des, color_pm, ost);
                    }
                }
            }
            else
            {
                if (PREMULTIPLIED(color_pm))
                {
                    if (ALPHA(color_pm))
                        for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch)
                            overfill_row_no_clamp(des, color_pm, des_info.sz.x);
                } else
                    for (int y = 0; y < des_info.sz.y; ++y, des += des_info.pitch)
                        overfill_row(des, color_pm, des_info.sz.x);
            }

        }
        break;
    }

}

void TSCALL img_helper_copy(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(sou_info.sz == des_info.sz);

    if (des_info.bytepp() == sou_info.bytepp())
    {
        aint len = sou_info.sz.x * des_info.bytepp();
        if (len == des_info.pitch && des_info.pitch == sou_info.pitch)
        {
            memcpy(des, sou, len * sou_info.sz.y);
        }
        else
        {
            for (int y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
                memcpy(des, sou, len);
        }
    }
    else
    {
        uint32 alpha = (sou_info.bytepp() == 3) ? 0xFF000000 : 0;

        for (aint y = 0; y < sou_info.sz.y; ++y, des += des_info.pitch, sou += sou_info.pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (aint i = 0; i < (sou_info.sz.x - 1); ++i, sou1 += sou_info.bytepp(), des1 += des_info.bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            aint c = tmin(sou_info.bytepp(), des_info.bytepp());
            while (c--)
                *des1++ = *sou1++;

            if (des_info.bytepp() == 4 && c == 3)
                *des1 = 0xFF;
        }
    }


}

#ifdef MODE64
// Assume any cpu with amd64 arch supports sse2, so sse2 intrinsics is good idea
static void TSCALL asm_shrink2x( uint8 *dst, const uint8 *src, long width, long height, aint srcpitch, aint dstcorrectpitch )
{
    aint srccpitch = srcpitch * 2 - width * 8;
    for( aint y = 0; y<height; ++y, src += srccpitch, dst += dstcorrectpitch )
    {
        for ( aint x = 0; x < width; ++x, src += 8, dst += 4 )
        {
            __m128i zero = _mm_setzero_si128();
            __m128i pix = _mm_add_epi16( _mm_unpacklo_epi8( _mm_cvtsi64_si128( *(int64 *)src ), zero ), _mm_unpacklo_epi8( _mm_cvtsi64_si128( *(int64 *)( src + srcpitch ) ), zero ) );
            __m128 h2l = _mm_movehl_ps( ( __m128 & )zero, ( __m128 & )pix );
            *(TSCOLOR *)dst = _mm_cvtsi128_si32( _mm_packus_epi16( _mm_srli_epi16 ( _mm_add_epi16( ( const __m128i & )h2l, pix ), 2 ), zero ) );
        }
    }
}
#else
extern "C" void TSCALL asm_shrink2x(
		void *dst,
		const void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcpitch,
		unsigned long dstcorrectpitch);
#endif


static void shrink_row_sse2_8px( const uint8* src_argb, int src_stride_argb, uint8* dst_argb, int dst_width )
{
    for( ; dst_width > 0; dst_width -= 4, src_argb += 32, dst_argb += 16 )
    {
        __m128i zero = _mm_setzero_si128();
        __m128i t1 = _mm_shuffle_epi32( _mm_loadu_si128( (const __m128i *)src_argb ), 0xd8 );
        __m128i t2 = _mm_shuffle_epi32( _mm_loadu_si128( ( const __m128i * )(src_argb + src_stride_argb) ), 0xd8 );

        __m128i ttt1 = _mm_shuffle_epi32( _mm_loadu_si128( ( const __m128i * )(src_argb + 16) ), 0xd8 );
        __m128i ttt2 = _mm_shuffle_epi32( _mm_loadu_si128( ( const __m128i * )( src_argb + src_stride_argb + 16 ) ), 0xd8 );

        _mm_storeu_si128( ( __m128i * )dst_argb,
            _mm_packus_epi16( _mm_srli_epi16( _mm_adds_epi16( _mm_adds_epi16( _mm_unpacklo_epi8( t1, zero ), _mm_unpackhi_epi8( t1, zero ) ),
                _mm_adds_epi16( _mm_unpacklo_epi8( t2, zero ), _mm_unpackhi_epi8( t2, zero ) ) ), 2 ),
                    _mm_srli_epi16( _mm_adds_epi16( _mm_adds_epi16( _mm_unpacklo_epi8( ttt1, zero ), _mm_unpackhi_epi8( ttt1, zero ) ),
                        _mm_adds_epi16( _mm_unpacklo_epi8( ttt2, zero ), _mm_unpackhi_epi8( ttt2, zero ) ) ), 2 ) ));
    }
}

void TSCALL img_helper_shrink_2x(uint8 *des, const uint8 *sou, const imgdesc_s &des_info, const imgdesc_s &sou_info)
{
    ASSERT(des_info.sz == (sou_info.sz / 2));
    ivec2 newsz = sou_info.sz / 2;

    aint desnl = des_info.pitch - des_info.sz.x*des_info.bytepp();
    aint sounl = sou_info.pitch - sou_info.sz.x*sou_info.bytepp();

    switch (sou_info.bytepp())
    {
    case 1:
        for (int y = 0; y < sou_info.sz.y; y += 2)
        {
            for (int x = 0; x < sou_info.sz.x; x += 2, ++des)
            {
                aint b0 = *(sou + x + 0);

                b0 += *(sou + x + 1);

                b0 += *(sou + x + 0 + sou_info.pitch);

                b0 += *(sou + x + 1 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 / 4);
            }

            sou += sou_info.pitch * 2;
        }
        break;
    case 2:
        for (aint y = 0; y < newsz.y; y++, sou += sou_info.pitch + sounl, des += desnl)
        {
            for (aint x = 0; x < newsz.x; x++, des += 3, sou += 3 + 3)
            {

                aint b0 = *(sou + 0);
                aint b1 = *(sou + 1);
                aint b2 = *(sou + 2);

                b0 += *(sou + 3);
                b1 += *(sou + 4);
                b2 += *(sou + 5);

                b0 += *(sou + 0 + sou_info.pitch);
                b1 += *(sou + 1 + sou_info.pitch);
                b2 += *(sou + 2 + sou_info.pitch);

                b0 += *(sou + 3 + sou_info.pitch);
                b1 += *(sou + 4 + sou_info.pitch);
                b2 += *(sou + 5 + sou_info.pitch);

                *(des + 0) = (uint8)(b0 >> 2);
                *(des + 1) = (uint8)(b1 >> 2);
                *(des + 2) = (uint8)(b2 >> 2);
            }
        }
        break;
    case 4:

        if ( CCAPS( CPU_SSE2 ) )
        {
            ASSERT( des != sou || des_info.pitch == sou_info.pitch ); // shrink to source allowed only with same pitch == keep unprocessed pixels unchanged

            uint8 *dst_sse = des;
            const uint8 *src_sse = sou;

            int dw = des_info.sz.x & ~3;
            for ( int y = 0; y < des_info.sz.y; ++y, dst_sse += des_info.pitch, src_sse += sou_info.pitch * 2 )
                shrink_row_sse2_8px( src_sse, sou_info.pitch, dst_sse, dw );

            if ( int ost = ( des_info.sz.x & 3 ) )
                asm_shrink2x( des + dw * 4, sou + dw * 8, ost, des_info.sz.y, sou_info.pitch, des_info.pitch - ost * 4 );

        } else
            asm_shrink2x(des, sou, des_info.sz.x, des_info.sz.y, sou_info.pitch, (int)desnl);
    }




}

void TSCALL img_helper_copy_components(uint8* des, const uint8* sou, const imgdesc_s &des_info, const imgdesc_s &sou_info, int num_comps)
{
    aint dpitch = des_info.pitch;
    aint spitch = sou_info.pitch;
    aint dszy = des_info.sz.y;
    for (dpitch -= des_info.sz.x*des_info.bytepp(), spitch -= des_info.sz.x*sou_info.bytepp(); dszy > 0; --dszy, des += dpitch, sou += spitch)
        for (int x = des_info.sz.x; x > 0; --x, des += des_info.bytepp(), sou += sou_info.bytepp())
            for (int i = 0; i < num_comps; ++i)
                des[i] = sou[i];
}


void img_helper_merge_with_alpha(uint8 *dst, const uint8 *basesrc, const uint8 *src, const imgdesc_s &des_info, const imgdesc_s &base_info, const imgdesc_s &sou_info, int oalphao)
{
    ASSERT(sou_info.bytepp() == 4);

    if (des_info.bytepp() == 3)
    {
        for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
        {
            uint8 *des = dst;
            const uint8 *desbase = basesrc;
            const uint32 *sou = (uint32 *)src;
            aint bytepp = des_info.bytepp();
            aint byteppbase = base_info.bytepp();
            for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
            {
                uint32 color = *sou;
                uint8 alpha = uint8(color >> 24);
                uint32 ocolor = 0;
                if (alpha == 0)
                {
                    if (dst == basesrc)
                        continue;
                } else if (alpha == 255)
                {
                    ocolor = color;
                }
                else
                {
                    ocolor = uint32(*(uint16 *)desbase) | (uint32(*((uint8 *)desbase + 2)) << 16);

                    uint8 R = as_byte(color >> 16);
                    uint8 G = as_byte(color >> 8);
                    uint8 B = as_byte(color);

                    uint8 oR = as_byte(ocolor >> 16);
                    uint8 oG = as_byte(ocolor >> 8);
                    uint8 oB = as_byte(ocolor);


                    float A = float(double(alpha) * (1.0 / 255.0));
                    float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( float(C) * A + float(oC) * nA )

                    ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16);
#undef CCGOOD
                }

                *des = as_byte(ocolor);
                *(des + 1) = as_byte(ocolor >> 8);
                *(des + 2) = as_byte(ocolor >> 16);
            }
        }
    }
    else
    {

        if (oalphao < 0)
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint8 *desbase = basesrc;
                const uint32 *sou = (uint32 *)src;
                aint bytepp = des_info.bytepp();
                aint byteppbase = base_info.bytepp();
                for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
                {
                    uint32 ocolor = 0;
                    uint32 color = *sou;
                    uint8 alpha = uint8(color >> 24);
                    if (alpha == 0)
                    {
                        if (dst == basesrc)
                            continue;
                    } else if (alpha == 255)
                    {
                        ocolor = color;
                    } else
                    {

                        uint8 R = as_byte(color >> 16);
                        uint8 G = as_byte(color >> 8);
                        uint8 B = as_byte(color);

                        uint32 ocolor_cur = byteppbase == 4 ? *(uint32 *)desbase : (uint32(*(uint16 *)desbase) | (uint32(*(desbase + 2)) << 16) | 0xFF000000);
                        uint8 oalpha = as_byte(ocolor_cur >> 24);
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(alpha) / 255.0f;
                        float nA = 1.0f - A;

#define CCGOOD( C, oC ) CLAMP<uint8>( float(C) * A + float(oC) * nA )
                        uint oiA = (uint)ts::lround(oalpha + float(255 - oalpha) * A);
                        ocolor = CCGOOD(B, oB) | (CCGOOD(G, oG) << 8) | (CCGOOD(R, oR) << 16) | (CLAMP<uint8>(oiA) << 24);
#undef CCGOOD

                    }

                    *(uint32 *)des = ocolor;
                }
            }
        }
        else
        {
            for (int y = 0; y < des_info.sz.y; y++, dst += des_info.pitch, basesrc += base_info.pitch, src += sou_info.pitch)
            {
                uint8 *des = dst;
                const uint8 *desbase = basesrc;
                const uint32 *sou = (uint32 *)src;
                aint bytepp = des_info.bytepp();
                aint byteppbase = base_info.bytepp();
                for (int x = 0; x < des_info.sz.x; x++, des += bytepp, desbase += byteppbase, ++sou)
                {
                    uint32 ocolor = 0;
                    uint32 icolor = *sou;
                    uint8 ialpha = uint8(icolor >> 24);
                    if (ialpha == 0)
                    {
                        if (dst == basesrc)
                            continue;
                    } else if (ialpha == 255)
                    {
                        ocolor = icolor;
                    }
                    else
                    {

                        uint8 R = as_byte(icolor >> 16);
                        uint8 G = as_byte(icolor >> 8);
                        uint8 B = as_byte(icolor);

                        uint32 ocolor_cur = byteppbase == 4 ? *(uint32 *)desbase : (uint32(*(uint16 *)desbase) | (uint32(*(desbase + 2)) << 16));
                        uint8 oR = as_byte(ocolor_cur >> 16);
                        uint8 oG = as_byte(ocolor_cur >> 8);
                        uint8 oB = as_byte(ocolor_cur);

                        float A = float(ialpha) / 255.0f;
                        float nA = 1.0f - A;


                        int oiB = ts::lround(float(B) * A + float(oB) * nA);
                        int oiG = ts::lround(float(G) * A + float(oG) * nA);
                        int oiR = ts::lround(float(R) * A + float(oR) * nA);

                        int oiA = ts::lround(oalphao + float(255 - oalphao) * A);

                        ocolor = ((oiB > 255) ? 255 : oiB) | (((oiG > 255) ? 255 : oiG) << 8) | (((oiR > 255) ? 255 : oiR) << 16) | (((oiA > 255) ? 255 : oiA) << 24);

                    }

                    *(uint32 *)des = ocolor;
                }

            }
        }
    }
}

static void alphablend_row_sse_no_clamp_aa( uint8 *dst_argb, const uint8 *src_argb, int w, const uint16 * alpha_mul )
{
    for(;w > 0; w -= 4, dst_argb += 16, src_argb += 16)
    {
        __m128i pta1 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_1 );
        __m128i pta2 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_2 );
        __m128i t3 = _mm_load_si128( ( const __m128i * )alpha_mul );
        __m128i t4 = _mm_load_si128( ( const __m128i * )sub256 );
        __m128i prepa = _mm_load_si128( ( const __m128i * )preparealphas );

        __m128i zero = _mm_setzero_si128();
        __m128i t5 = _mm_lddqu_si128( ( const __m128i * )src_argb );
        __m128i tt5 = _mm_mulhi_epu16( _mm_unpacklo_epi8( t5, zero ), t3 );
        __m128i tt6 = _mm_mulhi_epu16( _mm_unpackhi_epi8( t5, zero ), t3 );
        __m128i t1 = _mm_lddqu_si128( ( const __m128i * )dst_argb );
        
        _mm_storeu_si128( ( __m128i * )dst_argb, _mm_packus_epi16( _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t1, pta1 ), _mm_sub_epi16( t4, _mm_shuffle_epi8( tt5, prepa ) ) ), tt5 ), _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t1, pta2 ), _mm_sub_epi16( t4, _mm_shuffle_epi8( tt6, prepa ) ) ), tt6 ) ) );
    }
}

static void alphablend_row_sse_no_clamp( uint8 *dst_argb, const uint8 *src_argb, int w )
{
    for ( ; w > 0; w -= 4, dst_argb += 16, src_argb += 16 )
    {
        __m128i pta1 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_1 );
        __m128i pta2 = _mm_load_si128( ( const __m128i * )sse_consts::preparetgtc_2 );
        __m128i t4 = _mm_load_si128( ( const __m128i * )sub256 );
        __m128i t3 = _mm_load_si128( ( const __m128i * )preparealphas );

        __m128i zero = _mm_setzero_si128();
        __m128i t5 = _mm_lddqu_si128( ( const __m128i * )src_argb );
        __m128i tt5 = _mm_unpacklo_epi8( t5, zero );
        __m128i tt6 = _mm_unpackhi_epi8( t5, zero );
        __m128i t1 = _mm_lddqu_si128( ( const __m128i * )dst_argb );

        _mm_storeu_si128( ( __m128i * )dst_argb, _mm_packus_epi16( _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t1, pta1 ), _mm_sub_epi16( t4, _mm_shuffle_epi8( tt5, t3 ) ) ), tt5 ), _mm_add_epi16( _mm_mulhi_epu16( _mm_shuffle_epi8( t1, pta2 ), _mm_sub_epi16( t4, _mm_shuffle_epi8( tt6, t3 ) ) ), tt6 ) ) );
    }
}

void TSCALL img_helper_alpha_blend_pm( uint8 *dst, int dst_pitch, const uint8 *sou, const imgdesc_s &src_info, uint8 alpha, bool guaranteed_premultiplied )
{
    auto alphablend_row_1 = [](uint8 *dst, const uint8 *sou, int width)
    {
        for(const uint8 *soue = sou + width * 4; sou < soue; dst += 4, sou += 4)
            *(TSCOLOR *)dst = ALPHABLEND_PM( *(TSCOLOR *)dst, *(TSCOLOR *)sou );
    };

    auto alphablend_row_1_noclamp = [](uint8 *dst, const uint8 *sou, int width)
    {
        for (const uint8 *soue = sou + width * 4; sou < soue; dst += 4, sou += 4)
            *(TSCOLOR *)dst = ALPHABLEND_PM_NO_CLAMP(*(TSCOLOR *)dst, *(TSCOLOR *)sou);
    };

    auto alphablend_row_2 = [](uint8 *dst, const uint8 *sou, int width, uint8 alpha)
    {
        for(const uint8 *soue = sou + width * 4; sou < soue; dst += 4, sou += 4)
            *(TSCOLOR *)dst = ALPHABLEND_PM(*(TSCOLOR *)dst, *(TSCOLOR *)sou, alpha);
    };

    if (CCAPS(CPU_SSSE3) && guaranteed_premultiplied)
    {
        int w = src_info.sz.x & ~3;
        uint8 * dst_sse = dst;
        const uint8 * src_sse = sou;

        if (alpha == 255)
        {
            if (w)
                for (int y = 0; y < src_info.sz.y; ++y, dst_sse += dst_pitch, src_sse += src_info.pitch)
                    alphablend_row_sse_no_clamp(dst_sse, src_sse, w);
            
            if (int ost = src_info.sz.x & 3)
            {
                dst += w * 4;
                sou += w * 4;
                for (int y = 0; y < src_info.sz.y; ++y, dst += dst_pitch, sou += src_info.pitch)
                    alphablend_row_1_noclamp(dst, sou, ost);
            }
        } else
        {
            ts::uint16 am = (uint16)(alpha+1) << 8; // valid due alpha < 255
            ALIGN(16) ts::uint16 amul[8] = { am, am, am, am, am, am, am, am };

            if (w)
                for (int y = 0; y < src_info.sz.y; ++y, dst_sse += dst_pitch, src_sse += src_info.pitch)
                    alphablend_row_sse_no_clamp_aa(dst_sse, src_sse, w, amul);


            if (int ost = src_info.sz.x & 3)
            {
                dst += w * 4;
                sou += w * 4;
                for (int y = 0; y < src_info.sz.y; ++y, dst += dst_pitch, sou += src_info.pitch)
                    alphablend_row_2(dst, sou, ost, alpha);
            }

        }

        return;
    }

    if (alpha == 255)
    { 
        if (guaranteed_premultiplied)
            for(int y=0;y<src_info.sz.y;++y, dst += dst_pitch, sou += src_info.pitch)
                alphablend_row_1_noclamp( dst, sou, src_info.sz.x );
        else
            for (int y = 0; y < src_info.sz.y; ++y, dst += dst_pitch, sou += src_info.pitch)
                alphablend_row_1(dst, sou, src_info.sz.x);

    } else
        for (int y = 0; y < src_info.sz.y; ++y, dst += dst_pitch, sou += src_info.pitch)
            alphablend_row_2(dst, sou, src_info.sz.x, alpha);
}

void    bmpcore_normal_s::before_modify(bitmap_c *me)
{
    if (m_core == nullptr || m_core->m_ref == 1) return;

    bitmap_c b( *me );

    const imgdesc_s &__inf = b.info();

    if (__inf.bitpp == 8) me->create_grayscale(__inf.sz);
    else if (__inf.bitpp == 15) me->create_15(__inf.sz);
    else if (__inf.bitpp == 16) me->create_16(__inf.sz);
    else if (__inf.bitpp == 24) me->create_RGB(__inf.sz);
    if (__inf.bitpp == 32) me->create_ARGB(__inf.sz);

    me->copy(ivec2(0), __inf.sz, b.extbody(), ivec2(0));
}

bool bmpcore_normal_s::operator==(const bmpcore_normal_s & bm) const
{
    if (m_core == bm.m_core) return true;
    if (m_core == nullptr || bm.m_core == nullptr) return false;

    if (m_core->m_info != bm.m_core->m_info) return false;
    aint ln = m_core->m_info.sz.x * m_core->m_info.bytepp();
    if ( m_core->m_info.pitch == ln && bm.m_core->m_info.pitch == ln )
    {
        int sz = m_core->m_info.pitch * m_core->m_info.sz.y;
        return 0 == memcmp(m_core + 1, bm.m_core + 1, sz);
    }
    
    const uint8 *d1 = (const uint8 *)m_core + 1;
    const uint8 *d2 = (const uint8 *)bm.m_core + 1;
    aint cnt = m_core->m_info.sz.y;
    for(aint i=0;i<cnt;++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_core->m_info.pitch;
        d2 += bm.m_core->m_info.pitch;
    }
    return true;
}

bool bmpcore_exbody_s::operator==(const bmpcore_exbody_s & bm) const
{
    if (m_body == bm.m_body) return m_info == bm.m_info;
    if (m_info != bm.m_info) return false;

    aint ln = m_info.sz.x * m_info.bytepp();
    if (m_info.pitch == ln && bm.m_info.pitch == ln)
    {
        int sz = m_info.pitch * m_info.sz.y;
        return 0 == memcmp(m_body, bm.m_body, sz);
    }

    const uint8 *d1 = (const uint8 *)m_body;
    const uint8 *d2 = (const uint8 *)bm.m_body;
    aint cnt = m_info.sz.y;
    for (aint i = 0; i < cnt; ++i)
    {
        if (0 != memcmp(d1, d2, ln))
            return false;
        d1 += m_info.pitch;
        d2 += bm.m_info.pitch;
    }
    return true;
}


template <typename CORE> bitmap_t<CORE>& bitmap_t<CORE>::operator =( const bmpcore_exbody_s &eb )
{
    const imgdesc_s& __inf = eb.info();
    ASSERT( __inf.bytepp() >= 3 );
    if ( info().sz != __inf.sz || info().bytepp() != 4 )
        create_ARGB( __inf.sz );
    copy( ts::ivec2( 0 ), __inf.sz, eb, ts::ivec2( 0 ) );
    return *this;
}

template<typename CORE> void bitmap_t<CORE>::convert_from_yuv( const ivec2 & pdes, const ivec2 & size, const uint8 *src, yuv_fmt_e fmt )
{
    ASSERT( info().sz >>= (pdes + size) );
    ASSERT( info().bytepp() == 4 );
    img_helper_yuv2rgb(body(pdes), info( irect(0, size) ), src, fmt);
}

template<typename CORE> void bitmap_t<CORE>::convert_to_yuv( const ivec2 & pdes, const ivec2 & size, buf_c &b, yuv_fmt_e fmt )
{
    ASSERT(info().sz >>= (pdes + size));
    ASSERT(info().bytepp() == 4);

    aint bsz = 0;
    switch (fmt)
    {
    case ts::YFORMAT_YUY2:
        break;
    case ts::YFORMAT_I420:
        bsz = size.x * size.y;
        bsz += bsz/2;
        break;
    case ts::YFORMAT_I420x2:
        break;
    }
    if (bsz == 0)
        return;
    b.set_size(bsz, false);

    img_helper_rgb2yuv(b.data(), info(irect(0, size)), body(pdes), fmt);
}

template<typename CORE> void bitmap_t<CORE>::shrink_2x_to(const ivec2 &lt_source, const ivec2 &sz_source, const bmpcore_exbody_s &eb_target) const
{
    const imgdesc_s &__inf = eb_target.info();
    if(info().bytepp()==2 || (sz_source /2) != __inf.sz || info().bytepp() != __inf.bytepp()) return;

    uint8 *dst = eb_target();
    const uint8 *src = body(lt_source);

    img_helper_shrink_2x(dst, src, __inf, info( irect(0, sz_source) ));
}


#if 0

template <typename CORE> void bitmap_t<CORE>::convert_32to16(bitmap_c &imgout) const
{
    if (info().bitpp != 32) return;

    int cnt = info().pitch * info().sz.y / info().bytepp();
    uint32 *src = (uint32 *)body();
    uint16  *dst;
    int newp = m_core->m_pitch >> 1;
    if (m_core->m_ref > 1)
    {
        int sz = m_core->m_size.x * 2 * m_core->m_size.y;
        m_core->ref_dec();
        create_normal( sz );
    }

    dst = (uint16 *)body();


    while (cnt-- > 0)
    {

        uint32 s = *(src);
        *dst = uint16( ((s >> 8) & 0xF800) |
                     ((s >> 5) & 0x07E0) |
                     ((s >> 3) & 0x001F) );

        ++src;
        ++dst;
    }
    m_core->m_bitPP = 16;
    m_core->m_bytePP = 2;
    m_core->m_pitch = newp;

}

template <typename CORE> void bitmap_t<CORE>::convert_32to24(bitmap_c &imgout) const
{
	if (info().bitpp != 32) return;

	int cnt = m_core->m_pitch * m_core->m_size.y / m_core->m_bytePP;
	uint32 *src = (uint32 *)body();
	int newp = m_core->m_pitch * 3 / 4;
	if (m_core->m_ref > 1)
	{
		int sz = m_core->m_size.x * 3 * m_core->m_size.y;
		m_core->ref_dec();
		create_normal( sz );
	}

	uint8 *dst = body();

	while (cnt--)
		*((uint32*)dst) = *src++, dst += 3;

	m_core->m_bitPP = 24;
	m_core->m_bytePP = 3;
	m_core->m_pitch = newp;
}

template<typename CORE> void bitmap_t<CORE>::crop(const ivec2 & left_up,const ivec2 & size)
{
    int nx = size.x;
    int ny = size.y;
    
    int op = info().pitch;
    int obytepp = info().bytepp();
    int obitpp = info().bitpp;

    const uint8 *sou = body() + info().pitch * left_up.y + obytepp * left_up.x;
    if (m_core->m_ref > 1)
    {
        int sz = nx * obytepp * ny;
        m_core->ref_dec();
        create_normal( sz );

        m_core->m_bitPP = obitpp;
        m_core->m_bytePP = obytepp;
    }
    uint8  *des = body();

    m_core->m_size.x = nx;
    m_core->m_size.y = ny;
    m_core->m_pitch = obytepp * nx;
    copy(des,sou,nx,ny,m_core->m_pitch,obytepp,op,obytepp);
}

template<typename CORE> void bitmap_t<CORE>::crop_to_square()
{
    if (info().sz.x > info().sz.y)
    {
        int d = info().sz.x - info().sz.y;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(d0,0), ivec2(info().sz.x-d0-d1,info().sz.y) );

    } else if (info().sz.x < info().sz.y)
    {
        int d = info().sz.y - info().sz.x;
        int d0 = d/2;
        int d1 = d-d0;
        crop( ivec2(0, d0), ivec2(info().sz.x,info().sz.y-d0-d1) );
    }
}

template<typename CORE> void bitmap_t<CORE>::make_larger(int factor, bitmap_t<CORE> &out) const
{
	ivec2 newsz = size() * factor;

    if(out.info().sz.x != newx || out.info().sz.y != newy || out.info().bytepp() != info().bytepp() || out.m_core->m_ref > 1)
    {
        if(info().bytepp()==1) out.create_grayscale(newx,newy);
        else if(info().bytepp()==2) out.create_16(newx,newy);
        else if(info().bytepp()==3) out.create_RGB(newx,newy);
		else if(info().bytepp()==4) out.create_RGBA(newx,newy);
	}

	uint8 * des = out.body();;
	const uint8 * sou = body();

    int add = info().bytepp()*factor;
    int addl = info().pitch*factor;

    if(info().bytepp()==1)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                memset(des,*((uint8 *)sou + x), factor);
		    }

            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==2)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 2);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint16 *)des) + k) = w;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint16 w = *(uint16 *)(sou + x * 3);
                uint8 b = *(uint8 *)(sou + x * 3 + 2);
                for (int k = 0; k<factor; ++k)
                {
                    *((uint16 *)(des + k*3)) = w;
                    *((uint8 *)(des + k*3 + 2)) = b;
                }
		    }			
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
	    }
    } else
    if(info().bytepp()==4)
    {
        for(int y=0;y<info().sz.y;++y)
        {
		    for(int x=0;x<info().sz.x;++x,des+=add)
            {
                uint32 dw = *(uint32 *)(sou + x * 4);
                for (int k = 0; k<factor; ++k)
                {
                    *(((uint32 *)des) + k) = dw;
                }
            }
            for (int k = 0; k<(factor-1); ++k)
            {
                memcpy(des,des - addl, addl);
                des += addl;
            }

            sou += info().pitch;
        }
    }
}

#endif

template<typename CORE> irect bitmap_t<CORE>::calc_visible_rect() const
{
    ts::ivec2 sz = info().sz;
    if (info().bytepp() != 4)
        return irect( 0, sz );

    irect r;

    const uint8 * ptr = body() + 3;
    int addy = info().pitch - sz.x * 4;

    // search top
    for (int j = 0; j<sz.y; ++j, ptr += addy)
    {
        for (int i = 0; i<sz.x; ++i, ptr += 4)
        {
            if (*ptr)
            {
                r.lt.x = r.rb.x = i;
                r.lt.y = j;
                goto brk;
            }
        }
    }
    return irect(0);
brk:
    // search bottom
    ptr = body() + 3 + info().pitch * (sz.y - 1);
    addy -= info().pitch * 2;
    for (int j = sz.y-1; j > r.lt.y; --j, ptr += addy )
    {
        for (int i = 0; i < sz.x; ++i, ptr += 4)
        {
            if (*ptr)
            {
                if (i < r.lt.x) r.lt.x = i;
                if (i > r.rb.x) r.rb.x = i;
                r.rb.y = j+1;
                goto brk2;
            }
        }
    }
brk2:
    // search left
    for (int i = 0; i < r.lt.x; ++i)
    {
        ptr = body() + 3 + i * 4 + r.lt.y * info().pitch;

        for (int j = r.lt.y; j < r.rb.y; ++j, ptr += info().pitch)
        {
            if (*ptr)
            {
                r.lt.x = i;
                goto brk3;
            }
        }
    }
brk3:
    // search left
    for (int i = sz.x - 1; i > r.rb.x; --i)
    {
        ptr = body() + 3 + i * 4 + r.lt.y * info().pitch;

        for (int j = r.lt.y; j < r.rb.y; ++j, ptr += info().pitch)
        {
            if (*ptr)
            {
                r.rb.x = i+1;
                goto brk4;
            }
        }
    }
brk4:

    return r;
}

template<typename CORE> uint32 bitmap_t<CORE>::get_area_type(const irect &r) const
{
    if (info().bitpp != 32) return IMAGE_AREA_SOLID;

    ASSERT( r.lt.x >= 0 && r.lt.y >= 0 );

	const uint8 * src = body(r.lt) + 3;

    uint32 at = 0;

    int ww = tmin(r.width(), info().sz.x - r.lt.x);
    int hh = tmin(r.height(), info().sz.y - r.lt.y);

    for (int y = 0; y<hh; ++y, src += info().pitch)
    {
        for (int x = 0; x<ww; ++x)
        {
            uint8 ap = src[ x * 4 ];

            bool t_empty_found = (ap == 0x00);
            bool t_solid_found = (ap == 0xFF);

            if ( !t_empty_found && !t_solid_found  ) at |=IMAGE_AREA_SEMITRANSPARENT;
            if (t_empty_found) at |= IMAGE_AREA_TRANSPARENT;
            if (t_solid_found) at |= IMAGE_AREA_SOLID;

            if (at == IMAGE_AREA_ALLTYPE) return at;
        }
    }
    return at;
}

template<typename CORE> void bitmap_t<CORE>::tile(const ivec2 & pdes,const ivec2 & desize, const bmpcore_exbody_s & bmsou,const ivec2 & spsou, const ivec2 & szsou)
{
    copy(pdes, szsou, bmsou, spsou);

    // filling by x

    int cx = szsou.x;
    while (cx < desize.x)
    {
        int w = tmin((desize.x-cx), cx);
        copy( ivec2(pdes.x + cx, pdes.y), ivec2(w,szsou.y), extbody(), pdes);
        cx += w;
    }

    // filling by y

    int cy = szsou.y;
    while (cy < desize.y)
    {
        int h = tmin((desize.y-cy), cy);
        copy( ivec2(pdes.x, pdes.y + cy), ivec2(desize.x,h), extbody(), pdes);
        cy += h;
    }
}

template<typename CORE> void bitmap_t<CORE>::copy_components(bitmap_c &imageout, int num_comps, int dst_first_comp, int src_first_comp) const
{
	ASSERT(imageout.info().sz == info().sz && dst_first_comp+num_comps <= imageout.info().bytepp() && src_first_comp+num_comps <= info().bytepp());
	imageout.before_modify();
	img_helper_copy_components(imageout.body() + dst_first_comp, body() + src_first_comp, imageout.info(), info(), num_comps);
}

template<typename CORE> void bitmap_t<CORE>::copy(const ivec2 &pdes, const ivec2 &size, const bmpcore_exbody_s &bmsou, const ivec2 &spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	const uint8 * sou=bmsou()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;

    if (bmsou.info().bytepp() == info().bytepp())
    {
        aint len = size.x * info().bytepp();
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            memcpy(des, sou, len);
	    }
    } else
        // TODO : fix this brunch for non 3 or 4 bytePP images
    {
        uint32 alpha = bmsou.info().bytepp()==3?0xFF000000:0;
	    for(int y=0;y<size.y;y++,des+=info().pitch,sou+=bmsou.info().pitch)
        {
            uint8 *des1 = des;
            const uint8 *sou1 = sou;
            for (int i = 0; i<(size.x-1); ++i, sou1 += bmsou.info().bytepp(), des1 += info().bytepp())
            {
                uint32 color = *((uint32 *)sou1) | alpha;
                *((uint32 *)des1) = color;
            }
            aint c = tmin(bmsou.info().bytepp(), info().bytepp());
            while (c--)
            {
                *des1++ = *sou1++;
            }
            if (alpha) *des1 = 0xFF;
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    before_modify();

    if ( info().pitch == 4 * info().sz.x )
    {
        uint8 * o = body() + 3;
        int cnt = info().sz.x * info().sz.y;
        for (; cnt > 0; --cnt, o += 4)
            *o = a;
    } else
        fill_alpha(ts::ivec2(0), info().sz, a);
}

template<typename CORE> void bitmap_t<CORE>::fill_alpha(const ivec2 & pdes, const ivec2 & size, uint8 a)
{
    ASSERT( info().bytepp() == 4 );
    if (pdes.x < 0 || pdes.y < 0) return;
    if ((pdes.x + size.x) > info().sz.x || (pdes.y + size.y) > info().sz.y) return;

    before_modify();

    uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y + 3;
    aint desnl = info().pitch - size.x*info().bytepp();
    aint desnp = info().bytepp();

    for (int y = 0; y < size.y; y++, des += desnl) {
        for (int x = 0; x < size.x; x++, des += desnp) {
            *(uint8 *)des = a;
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::unmultiply()
{
    if ( info().bytepp() != 4 ) return;
    before_modify();

    uint8 * des = body();
    aint desnl = info().pitch - info().sz.x*info().bytepp();
    aint desnp = info().bytepp();

    for ( int y = 0; y < info().sz.y; ++y, des += desnl ) {
        for ( int x = 0; x < info().sz.x; ++x, des += desnp ) {
            *(TSCOLOR *)des = UNMULTIPLY( *(TSCOLOR *)des );
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::invcolor()
{
    if ( info().bytepp() != 4 ) return;
    before_modify();

    uint8 * des = body();
    aint desnl = info().pitch - info().sz.x*info().bytepp();
    aint desnp = info().bytepp();

    for ( int y = 0; y < info().sz.y; ++y, des += desnl ) {
        for ( int x = 0; x < info().sz.x; ++x, des += desnp ) {
            *(TSCOLOR *)des = 0x00ffffff ^ ( *(TSCOLOR *)des );
        }
    }
}

template<typename CORE> void bitmap_t<CORE>::detect_alpha_channel( const bitmap_t<CORE> & bmsou )
{
    class ADFILTERFILTER
    {
    public:

        bool begin( const bitmap_t<CORE> & tgt, const bitmap_t<CORE> & src )
        {
            return src.info().bitpp == 32 && tgt.info().bitpp == 32;
        }

        void point( uint8 * me, const FMATRIX &m )
        {
            // me = black
            // from = white

            const uint8 * from = m[1][1];

            if ( me[0] == from[0] && me[1] == from[1] && me[2] == from[2] )
            {
                me[3] = 0xFF;
            } else if ( me[0] == 0 && me[1] == 0 && me[2] == 0 && from[0] == 255 && from[1] == 255 && from[2] == 255 )
            {
                me[3] = 0x0;

            } else 
            {
                int a0 = 255 - from[0] + me[0];
                int a1 = 255 - from[1] + me[1];
                int a2 = 255 - from[2] + me[2];

                int a = tmax( a0, a1, a2 );

                me[0] = as_byte( me[0] * 255 / a );
                me[1] = as_byte( me[1] * 255 / a );
                me[2] = as_byte( me[2] * 255 / a );
                me[3] = as_byte( a );

            }

        }
    };
    
    ADFILTERFILTER xx;
    bmsou.copy_with_filter(*this,xx);
}

template<typename CORE> void bitmap_t<CORE>::fill(TSCOLOR color)
{
    before_modify();
    img_helper_fill( body(), info(), color );
}

template<typename CORE> void bitmap_t<CORE>::fill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
	if(pdes.x<0 || pdes.y<0 || (pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x+sz.x)>info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y+sz.y)>info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) fill(pos,sz,color);
        return;
    }
    if (!(size >> 0)) return;

    before_modify();
    const imgdesc_s &__inf = info();
    img_helper_fill( body()+ __inf.bytepp()*pdes.x+ __inf.pitch*pdes.y, imgdesc_s( __inf, size), color );
}

template<typename CORE> void bitmap_t<CORE>::overfill(const ivec2 & pdes,const ivec2 & size, TSCOLOR color)
{
    if (pdes.x<0 || pdes.y<0 || (pdes.x + size.x)>info().sz.x || (pdes.y + size.y)>info().sz.y)
    {
        ivec2 pos(pdes);
        ivec2 sz(size);
        if (pos.x < 0) { sz.x += pos.x; pos.x = 0; }
        if (pos.y < 0) { sz.y += pos.y; pos.y = 0; }
        if ((pos.x + sz.x) > info().sz.x) sz.x = info().sz.x - pos.x;
        if ((pos.y + sz.y) > info().sz.y) sz.y = info().sz.y - pos.y;
        if (sz >> 0) overfill(pos, sz, color);
        return;
    }
    if (!ASSERT(size >> 0)) return;

    before_modify();

    // formula is: (1-src.alpha) * dst + src.color
    // dont check 0 alpha due it can be (1 * desc + src) op

    uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y;

    img_helper_overfill(des, info( irect(0, size) ), color);
}

template<typename CORE> bool bitmap_t<CORE>::premultiply()
{
    if(info().bytepp()!=4) return false;
    before_modify();
    return img_helper_premultiply( body(), info() );
}

template<typename CORE> bool bitmap_t<CORE>::premultiply( const irect &rect )
{
    if (info().bytepp() != 4) return false;
    before_modify();
    return img_helper_premultiply( body(rect.lt), info(rect) );
}

template<typename CORE> void bitmap_t<CORE>::colorclamp()
{
    if (info().bytepp() != 4) return;
    before_modify();
    img_helper_colorclamp(body(), info());
}


template<typename CORE> void bitmap_t<CORE>::mulcolor(const ivec2 & pdes, const ivec2 & size, TSCOLOR color)
{
    if (info().bytepp() != 4) return;
    before_modify();
    uint8 * des = body() + info().bytepp()*pdes.x + info().pitch*pdes.y;
    img_helper_mulcolor(des, info(irect(0, size)), color);
}

template<typename CORE> void bitmap_t<CORE>::flip_x(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify(); //TODO: multicore optimizations

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	aint desnl=info().pitch-size.x*info().bytepp();
	aint desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	aint soull=bmsou.info().pitch;
	aint sounp=bmsou.info().bytepp();

	sou=sou+(size.x-1)*sounp;

	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+soull+(size.x)*sounp) {
		for(int x=0;x<size.x;x++,des+=desnp,sou-=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y()
{
    before_modify(); //TODO: multicore optimizations

    uint8 *l1 = body();
    uint8 *l2 = body() + info().pitch * (info().sz.y-1);
    
    while (l1<l2)
    {
        int cnt = info().pitch;
        uint8 *p0 = l1;
        uint8 *p1 = l2;
        while (cnt >= 4)
        {
            uint32 temp = *(uint32 *)p0;
            *(uint32 *)p0 = *(uint32 *)p1;
            *(uint32 *)p1 = temp;
            p0 += 4;
            p1 += 4;
            cnt -= 4;
        }
        while (cnt > 0)
        {
            uint8 temp = *p0;
            *p0 = *p1;
            *p1 = temp;
            ++p0;
            ++p1;
            --cnt;
        }

        l1+=info().pitch;
        l2-=info().pitch;
    }

}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size, const bitmap_t<CORE> & bmsou,const ivec2 & spsou)
{
	if(spsou.x<0 || spsou.y<0) return;
	if((spsou.x+size.x)>bmsou.info().sz.x || (spsou.y+size.y)>bmsou.info().sz.y) return;

	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

	if(bmsou.info().bytepp()!=info().bytepp()) return;
	if(bmsou.info().bitpp!=info().bitpp) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	aint desnl=info().pitch-size.x*info().bytepp();
	aint desnp=info().bytepp();

	const uint8 * sou=bmsou.body()+bmsou.info().bytepp()*spsou.x+bmsou.info().pitch*spsou.y;
	aint sounl=bmsou.info().pitch-size.x*bmsou.info().bytepp();
	aint soull=bmsou.info().pitch;
	aint sounp=bmsou.info().bytepp();

	sou=sou+(size.y-1)*soull;



	for(int y=0;y<size.y;y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1: *(uint8 *)des=*(uint8 *)sou; break;
				case 2: *(uint16 *)des=*(uint16 *)sou; break;
				case 3: *(uint16 *)des=*(uint16 *)sou; *(uint8 *)(des+2)=*(uint8 *)(sou+2); break;
				case 4: *(uint32 *)des=*(uint32 *)sou;break;
			}
		}			
	}
}

template<typename CORE> void bitmap_t<CORE>::flip_y(const ivec2 & pdes,const ivec2 & size)
{
	if(pdes.x<0 || pdes.y<0) return;
	if((pdes.x+size.x)>info().sz.x || (pdes.y+size.y)>info().sz.y) return;

    before_modify();

	uint8 * des=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	aint desnl=info().pitch-size.x*info().bytepp();
	aint desnp=info().bytepp();

	const uint8 * sou=body()+info().bytepp()*pdes.x+info().pitch*pdes.y;
	aint sounl=info().pitch-size.x*info().bytepp();
	aint soull=info().pitch;
	aint sounp=info().bytepp();

	sou=sou+(size.y-1)*soull;

	for(int y=0;y<(size.y >> 1);y++,des+=desnl,sou=sou+sounl-soull*2) {
		for(int x=0;x<size.x;x++,des+=desnp,sou+=sounp) {
			switch(info().bytepp()) {
				case 1:  {
					uint8 t=*(uint8 *)des;
					*(uint8 *)des=*(uint8 *)sou; 
					*(uint8 *)sou=t; 
					} break;
				case 2: {
                    uint16 t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=t;
					} break;
				case 3:  {
                    uint16 t=*(uint16 *)des;
					*(uint16 *)des=*(uint16 *)sou; 
					*(uint16 *)sou=t;

					t=*(uint8 *)(des+2);
					*(uint8 *)(des+2)=*(uint8 *)(sou+2); 
					*(uint8 *)(sou+2)=t & 0xff;
					} break;
				case 4: {
                    uint32 t=*(uint32 *)des;
					*(uint32 *)des=*(uint32 *)sou;
					*(uint32 *)sou=t;
					} break;
			}
		}			
	}

}

template<typename CORE> void bitmap_t<CORE>::alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img )
{
    if ( info().bytepp() != 4 )
        alpha_blend(p,img,extbody());
    else
    {
        // do photoshop normal mode mixing
        before_modify();

        const uint8 * s = img();

        ts::ivec2 px = p;
        ts::ivec2 isz = img.info().sz;

        if (px.x < 0) { s -= px.x * sizeof(TSCOLOR); isz.x += px.x; px.x = 0; };
        if (px.y < 0) { s -= px.y * img.info().pitch; isz.y += px.y; px.y = 0; };

        uint8 * t = body() + info().bytepp() * px.x + info().pitch * px.y;
        int srcwidth = tmin( isz.x, info().sz.x - px.x );
        int srcheight = tmin( isz.y, info().sz.y - px.y );

        for( int y=0; y<srcheight; ++y, t += info().pitch, s += img.info().pitch )
        {
            TSCOLOR *tgt = (TSCOLOR *)t;
            const TSCOLOR *src = (const TSCOLOR *)s;
            for(int x = 0; x<srcwidth; ++x, ++tgt, ++src)
                *tgt = ALPHABLEND( *tgt, *src );
        }

    }
}

template<typename CORE> void bitmap_t<CORE>::alpha_blend( const ivec2 &p, const bmpcore_exbody_s & img, const bmpcore_exbody_s & base )
{
    ASSERT( img.info().bytepp() == 4 );
    if (info().sz != base.info().sz)
        create_ARGB( base.info().sz );
    else
        before_modify();

    irect baserect( 0, base.info().sz - p );
    baserect.intersect( irect( 0, img.info().sz ) );

    ts::ivec2 imgwh( tmin(img.info().sz, info().sz - p) );
    if (base() != body())
    {
        // we have to copy base part outside of img rectange
        if (p.y > 0) // top
            img_helper_copy( body(), base(), imgdesc_s(info()).set_height(p.y), imgdesc_s(base.info()).set_height(p.y) );

        if (imgwh.y > 0)
        {

            if (p.x > 0) // left
                img_helper_copy(body() + p.y * info().pitch,
                                base() + p.y * base.info().pitch,
                                imgdesc_s(info()).set_width(p.x).set_height(imgwh.y),
                                imgdesc_s(base.info()).set_width(p.x).set_height(imgwh.y));

            int r_space = info().sz.x - p.x - img.info().sz.x;
            if (r_space > 0) // rite
                img_helper_copy(body() + p.y * info().pitch + (info().sz.x-r_space) * info().bytepp(),
                                base() + p.y * base.info().pitch + (info().sz.x-r_space) * base.info().bytepp(),
                                imgdesc_s(info()).set_width(r_space).set_height(imgwh.y),
                                imgdesc_s(base.info()).set_width(r_space).set_height(imgwh.y));
        }
        int b_space = info().sz.y - p.y - img.info().sz.y;
        if (b_space > 0) // bottom
            img_helper_copy(body() + (info().sz.y - b_space) * info().pitch, base() + (info().sz.y - b_space) * base.info().pitch, imgdesc_s(info()).set_height(b_space), imgdesc_s(base.info()).set_height(b_space));
    }

    // alphablend itself
    img_helper_merge_with_alpha(body(p), base() + p.x * base.info().bytepp() + p.y * base.info().pitch, img(), imgdesc_s(info()).set_size(imgwh), imgdesc_s(base.info()).set_size(imgwh), imgdesc_s(img.info()).set_size(imgwh));

}

template<typename CORE> void bitmap_t<CORE>::swap_byte(const ivec2 & pos,const ivec2 & size,int n1,int n2)
{
	if(n1==n2) return;
	if(n1<0 || n1>=info().bytepp()) return;
	if(n2<0 || n2>=info().bytepp()) return;
	if(info().bytepp()<=1) return;

    before_modify();

	uint8 * buf=body()+info().bytepp()*pos.x+info().pitch*pos.y;
	aint bufnl=info().pitch-size.x*info().bytepp();
	aint bufnp=info().bytepp();

	for(int y=0;y<size.y;y++,buf+=bufnl) {
		for(int x=0;x<size.x;x++,buf+=bufnp) {
            uint8 zn=*(buf+n1);
            *(buf+n1)=*(buf+n2);
            *(buf+n2)=zn;
		}
	}
}

void TSCALL sharpen_run(bitmap_c &obm, const uint8 *sou, const imgdesc_s &souinfo, int lv);
template<typename CORE> void bitmap_t<CORE>::sharpen(bitmap_c& outimage, int lv) const
{
    outimage.create_ARGB(info().sz);
    sharpen_run(outimage, body(), info(), lv);
}

bool resize3(const bmpcore_exbody_s &extbody, const uint8 *sou, const imgdesc_s &souinfo, resize_filter_e filt_mode);
bool img_helper_resize(const bmpcore_exbody_s &extbody, const uint8 *sou, const imgdesc_s &souinfo, resize_filter_e filt_mode)
{
    if ( filt_mode == FILTER_BOX_LANCZOS3 )
    {
        ASSERT( extbody.info().sz >>= ts::ivec2(16) );

        ivec2 sz2 = souinfo.sz / 2;
        if ( sz2 >>= extbody.info().sz )
        {
            if ( sz2 == extbody.info().sz )
            {
                img_helper_shrink_2x(extbody(), sou, extbody.info(), souinfo);
                return true;
            }

            ts::int16 stride = ((sz2.x * 4) + 15) & ~15;
            uint8 * tmp = (uint8 *)MM_ALLOC( stride * sz2.y );

            imgdesc_s desinf(sz2, 32, stride);
            img_helper_shrink_2x( tmp, sou, desinf, souinfo );


            imgdesc_s souinf = desinf;
            sz2 = sz2/2;
            for(;sz2 >>= extbody.info().sz;)
            {
                if (sz2 == extbody.info().sz)
                {
                    img_helper_shrink_2x(extbody(), tmp, extbody.info(), souinf);
                    MM_FREE(tmp);
                    return true;
                }

                desinf = imgdesc_s(sz2, 32, stride);
                img_helper_shrink_2x( tmp, tmp, desinf, souinf );
                souinf = desinf;
                sz2 = sz2 / 2;
            }


            bool r = resize3( extbody, tmp, souinf, FILTER_LANCZOS3 );
            MM_FREE(tmp);
            return r;
        }
        filt_mode = FILTER_LANCZOS3;
    }

    return resize3( extbody, sou, souinfo, filt_mode );
}
template<typename CORE> bool bitmap_t<CORE>::resize_to(bitmap_c& obm, const ivec2 & newsize, resize_filter_e filt_mode) const
{
    obm.create_ARGB(newsize);
    return img_helper_resize(obm.extbody(), body(), info(), filt_mode);
}

template<typename CORE> bool bitmap_t<CORE>::resize_to(const bmpcore_exbody_s &extbody_, resize_filter_e filt_mode) const
{
    return img_helper_resize(extbody_, body(), info(), filt_mode);
}

template<typename CORE> bool bitmap_t<CORE>::resize_from(const bmpcore_exbody_s &extbody_, resize_filter_e filt_mode) const
{
    return img_helper_resize(extbody(), extbody_(), extbody_.info(), filt_mode);
}

template<typename CORE> void bitmap_t<CORE>::make_grayscale()
{
    if (info().bytepp() != 4 || info().bitpp != 32) return;
    before_modify();

    uint8 * des=body();
    aint desnl=info().pitch-info().sz.x*info().bytepp();
    aint desnp=info().bytepp();

    if(info().bytepp()==3)
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                uint32 ocolor  = (X) | (X<<8) | (X<<16);


                *(uint16 *)des=(uint16)ocolor;
                *(uint8 *)(des+2)=(uint8)(ocolor>>16);
            }			
        }
    } else
    {
        for(int y=0;y<info().sz.y;y++,des+=desnl) {
            for(int x=0;x<info().sz.x;x++,des+=desnp)
            {
                uint32 color = *(uint32 *)des;

                int  A = as_byte( color>>24 );
                int  R = as_byte( color>>16 );
                int  G = as_byte( color>>8 );
                int  B = as_byte( color );

                // 0.3 0.5 0.2
                int X = (R * 350 + G * 450 + B * 200) / 1000;

                *(uint32 *)des=(X) | (X<<8) | (X<<16) | (A<<24);
            }			
        }
    }
}

template<typename CORE> bool bitmap_t<CORE>::load_from_BMPHEADER(const BITMAPINFOHEADER * iH, int buflen)
{
    clear();

	ivec2 sz( iH->biWidth, iH->biHeight );

	switch (iH->biBitCount)
	{
	case 24: create_RGB (sz); break;
	case 32: create_ARGB(sz); break;
	default: return false;
	}

	const uint8 *pixels = ((const uint8*)iH) + iH->biSize;
    const uint8 *epixels = ((const uint8*)iH) + buflen;
    uint8 *tdata = body()+(sz.y-1)*info().pitch;

	int bmppitch = (info().pitch + 3) & ~3;

	for (int y=sz.y; y>0; tdata-=info().pitch, pixels+=bmppitch, y--)
        if ( CHECK((pixels + info().pitch) <= epixels) )
		    memcpy(tdata, pixels, info().pitch);

	return true;
}

template<typename CORE> img_format_e bitmap_t<CORE>::load_from_file( const void * buf, aint buflen, const ivec2 &limitsize, IMG_LOADING_PROGRESS progress )
{
	if (buflen < 4) return if_none;

    UNSAFE_BLOCK_BEGIN

        img_reader_s reader;
        img_format_e fmt = if_none;
        if (image_read_func r = reader.detect(buf,buflen,fmt,limitsize))
        {
            reader.progress = progress;
            switch (reader.bitpp)
            {
            case 8:
                create_grayscale(reader.size);
                if (r(reader,body(),info().pitch)) return fmt;
                break;
            case 15:
                create_15(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 16:
                create_16(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 24:
                create_RGB(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            case 32:
                create_ARGB(reader.size);
                if (r(reader, body(), info().pitch)) return fmt;
                break;
            }
        }

    UNSAFE_BLOCK_END

    clear();
    return if_none;
}
template<typename CORE> img_format_e bitmap_t<CORE>::load_from_file(const wsptr &filename)
{
	if (blob_c b = g_fileop->load(filename))
		return load_from_file( b.data(), b.size() );
	return if_none;
}

#if 0
template<typename CORE> bool bitmap_t<CORE>::load_from_HBITMAP(HBITMAP hbmp)
{
    BITMAP bm;
    if (0 == GetObject(hbmp, sizeof(BITMAP), &bm)) return false;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[256];
    } b;

    HDC dc = GetDC(0);
    memset(&b, 0, sizeof(BITMAPV4HEADER));
    b.bmi.bmiHeader.bV4Size = sizeof(BITMAPINFOHEADER);
    b.bmi.bmiHeader.bV4Width = bm.bmWidth;
    b.bmi.bmiHeader.bV4Height = -bm.bmHeight;
    b.bmi.bmiHeader.bV4Planes = 1;

    if (1 == bm.bmBitsPixel)
    {
        b.bmi.bmiHeader.bV4BitCount = 1;
        b.bmi.bmiHeader.bV4SizeImage = bm.bmWidthBytes * bm.bmHeight;
        byte *data = (byte *)_alloca(bm.bmWidthBytes * bm.bmHeight);
        bool ok = GetDIBits(dc, hbmp, 0, bm.bmHeight, data, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS) != 0;
        if (ok)
        {
            create_grayscale(ivec2(bm.bmWidth, bm.bmHeight));

            byte *my_body = body();
            for (int y = 0; y < bm.bmHeight; ++y, data += bm.bmWidthBytes, my_body += info().pitch)
            {
                const byte *d2 = (const byte *)data;
                byte *bb = my_body;
                for (int x = 0; x < bm.bmWidth; x += 8, ++d2)
                {
                    ts::uint8 bits = *d2;
                    for( int m = 128, xx = x; m>0; m >>=1, ++xx, ++bb )
                        *bb = (bits & m) ? 255 : 0;
                }
            }

        }

        ReleaseDC(0, dc);
        return ok;
    }
    return true;
}

template<typename CORE> void bitmap_t<CORE>::load_from_HWND(HWND hwnd)
{
    RECT r;
    GetClientRect( hwnd, &r );
    ivec2 sz(r.right-r.left, r.bottom-r.top);

    int lPitch=((4*sz.x-1)/4+1)*4;

    BITMAPINFO bi;
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz.x;
    bi.bmiHeader.biHeight = -sz.y;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = 0;
    bi.bmiHeader.biSizeImage = lPitch * sz.y;
    bi.bmiHeader.biXPelsPerMeter = 0;
    bi.bmiHeader.biYPelsPerMeter = 0;
    bi.bmiHeader.biClrImportant = 0;
    bi.bmiHeader.biClrUsed = 0;

    HDC dc = GetDC( hwnd );
    
    uint8 *bits;
    HDC memDC = CreateCompatibleDC(dc);
    HBITMAP memBM = CreateDIBSection(dc,&bi,DIB_RGB_COLORS,(void **)&bits,0,0);
    ReleaseDC(hwnd, dc);

    if (memBM == nullptr)
    {
        return;
    }

    HBITMAP tempBM = (HBITMAP)SelectObject(memDC,memBM);

    BitBlt(memDC,0,0,info().sz.x,info().sz.y,dc,0,0,SRCCOPY);

    if (info().sz != sz || info().bytepp() != 4) create_ARGB( sz );

    img_helper_copy( body(), bits, info(), imgdesc_s( sz, 32, (uint16)lPitch ) );

    SelectObject(memDC,tempBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}
#endif

template<typename CORE> void bitmap_t<CORE>::gen_qrcore( int margins, int dotsize, const asptr& utf8string, int bitpp, TSCOLOR fg_color, TSCOLOR bg_color )
{
    QRcode *code = nullptr;

    int micro = 0;
    int eightbit = 0;
    int casesensitive = 1;
    int version = 0;
    QRecLevel level = QR_ECLEVEL_L;
    QRencodeMode hint = QR_MODE_8;

    if (micro)
    {
        if (eightbit)
        {
            //code = QRcode_encodeDataMQR(utf8string.l, utf8string.s, version, level);
        }
        else 
        {
            tmp_str_c s( utf8string );
            code = QRcode_encodeStringMQR(s.cstr(), version, level, hint, casesensitive);
        }
    } else
    {
        if (eightbit)
        {
            //code = QRcode_encodeData(length, intext, version, level);
        }
        else
        {
            tmp_str_c s( utf8string );
            code = QRcode_encodeString(s.cstr(), version, level, hint, casesensitive);
        }
    }

    if (code)
    {
        int sz = code->width * dotsize + margins * 2;
        core.create(imgdesc_s(ts::ivec2(sz), (uint8)bitpp));

        fill(ivec2(0), ivec2(sz, margins), bg_color);
        fill(ivec2(0,sz-margins), ivec2(sz, margins), bg_color);
        fill(ivec2(0,margins), ivec2(margins, sz-margins*2), bg_color);
        fill(ivec2(sz-margins,margins), ivec2(margins, sz-margins*2), bg_color);

        unsigned char *p = code->data;

        uint8 *tgt = body( ivec2(margins) );
        aint addtgtbyqr = dotsize * info().bytepp();
        aint addtgtbyy = dotsize * info().pitch - addtgtbyqr * code->width;

        for(int y = 0; y<code->width; ++y, tgt += addtgtbyy )
        {
            unsigned char *row = (p + (y*code->width));
            for(int x = 0; x<code->width; ++x, tgt += addtgtbyqr )
            {
                TSCOLOR c = (*(row + x) & 0x1) ? fg_color : bg_color;

                if (bitpp == 32)
                {
                    img_helper_fill( tgt, info(irect(0,0,dotsize,dotsize)), c );

                } else
                {
                    ERROR("Sorry, supported only 32 bits per pixel"); // TODO
                }
            }
        }
        QRcode_free(code);
    }
}


bool    drawable_bitmap_c::create_from_bitmap(const bitmap_c &bmp, bool flipy, bool premultiply, bool detect_alpha_pixels)
{
    return create_from_bitmap(bmp, ivec2(0, 0), bmp.info().sz, flipy, premultiply, detect_alpha_pixels);
}


void bmpcore_exbody_s::draw(const bmpcore_exbody_s &eb, aint xx, aint yy, int alpha) const
{
    const imgdesc_s& __inf = eb.info();

    ASSERT(xx + info().sz.x <= __inf.sz.x);
    ASSERT(yy + info().sz.y <= __inf.sz.y);

    if (alpha > 0)
        img_helper_alpha_blend_pm(eb(ivec2(xx, yy)), __inf.pitch, (*this)(), info(), (uint8)alpha);
    else
        img_helper_copy(eb(ivec2(xx, yy)), (*this)(), __inf.chsize(info().sz), info());

}

void bmpcore_exbody_s::draw(const bmpcore_exbody_s &eb, aint xx, aint yy, const irect &r, int alpha) const
{
    const uint8 *src = (*this)(r.lt);
    const imgdesc_s& __inf = eb.info();
    
    ts::ivec2 sz = r.size();
    if (xx + sz.x > __inf.sz.x)
        sz.x = (int)( __inf.sz.x - xx);
    if (yy + sz.y > __inf.sz.y)
        sz.x = (int)( __inf.sz.y - yy);

    imgdesc_s sinf(sz, 32, info().pitch);

    if (alpha > 0)
        img_helper_alpha_blend_pm(eb(ivec2(xx, yy)), __inf.pitch, src, sinf, (uint8)alpha);
    else
        img_helper_copy(eb(ivec2(xx, yy)), src, __inf.chsize(sinf.sz), sinf);

}


template class bitmap_t<bmpcore_normal_s>;
template class bitmap_t<bmpcore_exbody_s>;


bool	drawable_bitmap_c::ajust( const ivec2 &sz, bool exact_size )
{
    ivec2 newbbsz( ( sz.x + 15 ) & ( ~15 ), ( sz.y + 15 ) & ( ~15 ) );
    ivec2 bbsz = info().sz;

    if ( ( exact_size && newbbsz != bbsz ) || sz > bbsz )
    {
        if ( newbbsz.x < bbsz.x ) newbbsz.x = bbsz.x;
        if ( newbbsz.y < bbsz.y ) newbbsz.y = bbsz.y;
        create( newbbsz );
        return true;
    }
    return false;
}

void render_image( const bmpcore_exbody_s &tgt, const bmpcore_exbody_s &image, aint x, aint y, const irect &cliprect, int alpha )
{
    irect imgrectnew( 0, image.info().sz );
    imgrectnew += ivec2( x, y );
    imgrectnew.intersect( cliprect );
    imgrectnew.intersect( ts::irect( 0, tgt.info().sz ) );
    if ( !imgrectnew ) return;

    imgrectnew -= ivec2( x, y );
    x += imgrectnew.lt.x; y += imgrectnew.lt.y;

    image.draw( tgt, x, y, imgrectnew, alpha );
}

void render_image( const bmpcore_exbody_s &tgt, const bmpcore_exbody_s &image, aint x, aint y, const irect &imgrect, const irect &cliprect, int alpha )
{
    irect imgrectnew = imgrect.szrect();
    imgrectnew += ivec2( x, y );
    imgrectnew.intersect( cliprect );
    imgrectnew.intersect( ts::irect(0, tgt.info().sz) );
    if ( !imgrectnew ) return;

    imgrectnew -= ivec2( x, y );
    x += imgrectnew.lt.x; y += imgrectnew.lt.y;
    imgrectnew += imgrect.lt;

    image.draw( tgt, x, y, imgrectnew, alpha );
}

void repdraw::draw_h( ts::aint x1, ts::aint x2, ts::aint y, bool tile )
{
    if ( rbeg ) render_image( tgt, image, x1, y, *rbeg, cliprect, a_beg ? alpha : -1 );

    if ( tile )
    {
        int dx = rrep->width();
        if ( CHECK( dx ) )
        {
            int a = a_rep ? alpha : -1;

            aint sx0 = x1 + ( rbeg ? rbeg->width() : 0 );
            aint sx1 = x2 - ( rend ? rend->width() : 0 );
            aint z = sx0 + dx;
            for ( ; z <= sx1; z += dx )
                render_image( tgt, image, z - dx, y, *rrep, cliprect, a );
            z -= dx;
            if ( z < sx1 )
                render_image( tgt, image, z, y, ts::irect( *rrep ).setwidth( sx1 - z ), cliprect, a );
        }
    }
    else
    {
        ASSERT( false, "stretch" );
    }
    if ( rend ) render_image( tgt, image, x2 - rend->width(), y, *rend, cliprect, a_end ? alpha : -1 );
}
void repdraw::draw_v( ts::aint x, ts::aint y1, ts::aint y2, bool tile )
{
    if ( rbeg ) render_image( tgt, image, x, y1, *rbeg, cliprect, a_beg ? alpha : -1 );

    if ( tile )
    {
        int dy = rrep->height();
        if ( CHECK( dy ) )
        {
            int a = a_rep ? alpha : -1;

            aint sy0 = y1 + ( rbeg ? rbeg->height() : 0 );
            aint sy1 = y2 - ( rend ? rend->height() : 0 );
            aint z = sy0 + dy;
            for ( ; z <= sy1; z += dy )
                render_image( tgt, image, x, z - dy, *rrep, cliprect, a );
            z -= dy;
            if ( z < sy1 )
                render_image( tgt, image, x, z, ts::irect( *rrep ).setheight( sy1 - z ), cliprect, a );
        }
    }
    else
    {
        ASSERT( false, "stretch" );
    }

    if ( rend ) render_image( tgt, image, x, y2 - rend->height(), *rend, cliprect, a_end ? alpha : -1 );
}

void repdraw::draw_c( ts::aint x1, ts::aint x2, ts::aint y1, ts::aint y2, bool tile )
{
    if ( tile )
    {
        int a = a_rep ? alpha : -1;

        int dx = rrep->width();
        int dy = rrep->height();
        if ( dx == 0 || dy == 0 ) return;
        aint sx0 = x1;
        aint sx1 = x2 - dx;
        aint sy0 = y1;
        aint sy1 = y2 - dy;
        aint x;
        ts::irect rr; bool rr_initialized = false;
        aint y = sy0;
        for ( ; y < sy1; y += dy )
        {
            for ( x = sx0; x < sx1; x += dx )
                render_image( tgt, image, x, y, *rrep, cliprect, a );

            if ( !rr_initialized )
            {
                if ( x < ( sx1 + dx ) )
                {
                    rr = *rrep;
                    rr.setwidth( sx1 + dx - x );
                    rr_initialized = true;
                }
            }

            if ( rr_initialized )
                render_image( tgt, image, x, y, rr, cliprect, a );
        }
        if ( y < ( sy1 + dy ) )
        {
            ts::irect rrb = *rrep;
            rrb.setheight( sy1 + dy - y );
            for ( x = sx0; x < sx1; x += dx )
                render_image( tgt, image, x, y, rrb, cliprect, a );
            if ( rr_initialized )
                render_image( tgt, image, x, y, rrb.setwidth( rr.width() ), cliprect, a );
            else
                render_image( tgt, image, x, y, rrb.setwidth( sx1 + dx - x ), cliprect, a );
        }
    }
    else
    {
        ASSERT( false, "stretch" );
    }
}


#ifdef _WIN32
#include "_win32/win32_bitmap.inl"
#endif

#ifdef _NIX
#include "_nix/nix_bitmap.inl"
#endif

} // namespace ts

