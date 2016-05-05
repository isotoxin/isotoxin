/*
    SSSE3/AVX2 code - copy-paste from libuv
*/

/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
Name: libyuv
URL: http://code.google.com/p/libyuv/
Version: 1456
License: BSD
*/

#include "stdafx.h"

//-V:ANY31:512

#pragma warning(disable:4100) //: 'width' : unreferenced formal parameter

#define IS_ALIGNED(p, a) (!((uintptr_t)(p) & ((a) - 1)))
#define SIMD_ALIGNED(var) __declspec(align(16)) var

// Subsampled source needs to be increase by 1 of not even.
#define SSS(width, shift) (((width) + (1 << (shift)) - 1) >> (shift))

// Any 3 planes to 1.
#define ANY31(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)              \
    void NAMEANY(const byte* y_buf, const byte* u_buf, const byte* v_buf,   \
                 byte* dst_ptr, int width) {                                \
      SIMD_ALIGNED(byte temp[64 * 4]);                                      \
      memset(temp, 0, 64 * 3);  /* for YUY2 and msan */                     \
      int r = width & MASK;                                                 \
      int n = width & ~MASK;                                                \
      if (n > 0) {                                                          \
        ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, n);                          \
                  }                                                         \
      memcpy(temp, y_buf + n, r);                                           \
      memcpy(temp + 64, u_buf + (n >> UVSHIFT), SSS(r, UVSHIFT));           \
      memcpy(temp + 128, v_buf + (n >> UVSHIFT), SSS(r, UVSHIFT));          \
      ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, MASK + 1);          \
      memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 192,                   \
             SSS(r, DUVSHIFT) * BPP);                                       \
                }


typedef signed char		sint8;
typedef signed short	sint16;
typedef sint8			int8;
typedef sint16			int16;

typedef __declspec(align(32)) int8 lvec8[32];
typedef __declspec(align(32)) int16 lvec16[16];

struct YuvConstants {
    lvec8 kUVToB;     // 0
    lvec8 kUVToG;     // 32
    lvec8 kUVToR;     // 64
    lvec16 kUVBiasB;  // 96
    lvec16 kUVBiasG;  // 128
    lvec16 kUVBiasR;  // 160
    lvec16 kYToRgb;   // 192
};

// Y contribution to R,G,B.  Scale and bias.
// TODO(fbarchard): Consider moving constants into a common header.
#define YG 18997 /* round(1.164 * 64 * 256 * 256 / 257) */
#define YGB -1160 /* 1.164 * 64 * -16 + 64 / 2 */

// U and V contributions to R,G,B.
#define UB -128 /* max(-128, round(-2.018 * 64)) */
#define UG 25 /* round(0.391 * 64) */
#define VG 52 /* round(0.813 * 64) */
#define VR -102 /* round(-1.596 * 64) */

// Bias values to subtract 16 from Y and 128 from U and V.
#define BB (UB * 128            + YGB)
#define BG (UG * 128 + VG * 128 + YGB)
#define BR            (VR * 128 + YGB)


// BT601 constants for YUV to RGB.
static YuvConstants SIMD_ALIGNED(kYuvConstants) = {
    { UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0,
    UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0, UB, 0 },
    { UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG,
    UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG },
    { 0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR,
    0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR, 0, VR },
    { BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB, BB },
    { BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG, BG },
    { BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR, BR },
    { YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG, YG }
};

#ifdef MODE64
void I422ToARGBRow_SSSE3( const byte* y_buf, const byte* u_buf, const byte* v_buf, byte* dst_argb, int width )
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i xmm5 = _mm_set1_epi8( -1 );
    const ptrdiff_t offset = (byte*)v_buf - (byte*)u_buf;

    while ( width > 0 ) {
        xmm0 = _mm_cvtsi32_si128( *(u32*)u_buf );
        xmm1 = _mm_cvtsi32_si128( *(u32*)( u_buf + offset ) );
        xmm0 = _mm_unpacklo_epi8( xmm0, xmm1 );
        xmm0 = _mm_unpacklo_epi16( xmm0, xmm0 );
        xmm1 = _mm_loadu_si128( &xmm0 );
        xmm2 = _mm_loadu_si128( &xmm0 );
        xmm0 = _mm_maddubs_epi16( xmm0, *( __m128i* )kYuvConstants.kUVToB );
        xmm1 = _mm_maddubs_epi16( xmm1, *( __m128i* )kYuvConstants.kUVToG );
        xmm2 = _mm_maddubs_epi16( xmm2, *( __m128i* )kYuvConstants.kUVToR );
        xmm0 = _mm_sub_epi16( *( __m128i* )kYuvConstants.kUVBiasB, xmm0 );
        xmm1 = _mm_sub_epi16( *( __m128i* )kYuvConstants.kUVBiasG, xmm1 );
        xmm2 = _mm_sub_epi16( *( __m128i* )kYuvConstants.kUVBiasR, xmm2 );
        xmm3 = _mm_loadl_epi64( ( __m128i* )y_buf );
        xmm3 = _mm_unpacklo_epi8( xmm3, xmm3 );
        xmm3 = _mm_mulhi_epu16( xmm3, *( __m128i* )kYuvConstants.kYToRgb );
        xmm0 = _mm_adds_epi16( xmm0, xmm3 );
        xmm1 = _mm_adds_epi16( xmm1, xmm3 );
        xmm2 = _mm_adds_epi16( xmm2, xmm3 );
        xmm0 = _mm_srai_epi16( xmm0, 6 );
        xmm1 = _mm_srai_epi16( xmm1, 6 );
        xmm2 = _mm_srai_epi16( xmm2, 6 );
        xmm0 = _mm_packus_epi16( xmm0, xmm0 );
        xmm1 = _mm_packus_epi16( xmm1, xmm1 );
        xmm2 = _mm_packus_epi16( xmm2, xmm2 );
        xmm0 = _mm_unpacklo_epi8( xmm0, xmm1 );
        xmm2 = _mm_unpacklo_epi8( xmm2, xmm5 );
        xmm1 = _mm_loadu_si128( &xmm0 );
        xmm0 = _mm_unpacklo_epi16( xmm0, xmm2 );
        xmm1 = _mm_unpackhi_epi16( xmm1, xmm2 );

        _mm_storeu_si128( ( __m128i * )dst_argb, xmm0 );
        _mm_storeu_si128( ( __m128i * )( dst_argb + 16 ), xmm1 );

        y_buf += 8;
        u_buf += 4;
        dst_argb += 32;
        width -= 8;
    }
}

#else

// Read 4 UV from 422, upsample to 8 UV.
#define READYUV422 __asm {                                                     \
    __asm movd       xmm0, [esi]          /* U */                              \
    __asm movd       xmm1, [esi + edi]    /* V */                              \
    __asm lea        esi,  [esi + 4]                                           \
    __asm punpcklbw  xmm0, xmm1           /* UV */                             \
    __asm punpcklwd  xmm0, xmm0           /* UVUV (upsample) */                \
  }

// Convert 8 pixels: 8 UV and 8 Y.
#define YUVTORGB(YuvConstants) __asm {                                         \
    /* Step 1: Find 4 UV contributions to 8 R,G,B values */                    \
    __asm movdqa     xmm1, xmm0                                                \
    __asm movdqa     xmm2, xmm0                                                \
    __asm movdqa     xmm3, xmm0                                                \
    __asm movdqa     xmm0, YuvConstants.kUVBiasB /* unbias back to signed */   \
    __asm pmaddubsw  xmm1, YuvConstants.kUVToB   /* scale B UV */              \
    __asm psubw      xmm0, xmm1                                                \
    __asm movdqa     xmm1, YuvConstants.kUVBiasG                               \
    __asm pmaddubsw  xmm2, YuvConstants.kUVToG   /* scale G UV */              \
    __asm psubw      xmm1, xmm2                                                \
    __asm movdqa     xmm2, YuvConstants.kUVBiasR                               \
    __asm pmaddubsw  xmm3, YuvConstants.kUVToR   /* scale R UV */              \
    __asm psubw      xmm2, xmm3                                                \
    /* Step 2: Find Y contribution to 8 R,G,B values */                        \
    __asm movq       xmm3, qword ptr [eax]                        /* NOLINT */ \
    __asm lea        eax, [eax + 8]                                            \
    __asm punpcklbw  xmm3, xmm3                                                \
    __asm pmulhuw    xmm3, YuvConstants.kYToRgb                                \
    __asm paddsw     xmm0, xmm3           /* B += Y */                         \
    __asm paddsw     xmm1, xmm3           /* G += Y */                         \
    __asm paddsw     xmm2, xmm3           /* R += Y */                         \
    __asm psraw      xmm0, 6                                                   \
    __asm psraw      xmm1, 6                                                   \
    __asm psraw      xmm2, 6                                                   \
    __asm packuswb   xmm0, xmm0           /* B */                              \
    __asm packuswb   xmm1, xmm1           /* G */                              \
    __asm packuswb   xmm2, xmm2           /* R */                              \
      }

// Store 8 ARGB values.
#define STOREARGB __asm {                                                      \
    /* Step 3: Weave into ARGB */                                              \
    __asm punpcklbw  xmm0, xmm1           /* BG */                             \
    __asm punpcklbw  xmm2, xmm5           /* RA */                             \
    __asm movdqa     xmm1, xmm0                                                \
    __asm punpcklwd  xmm0, xmm2           /* BGRA first 4 pixels */            \
    __asm punpckhwd  xmm1, xmm2           /* BGRA next 4 pixels */             \
    __asm movdqu     0[edx], xmm0                                              \
    __asm movdqu     16[edx], xmm1                                             \
    __asm lea        edx,  [edx + 32]                                          \
  }

// 8 pixels.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) void I422ToARGBRow_SSSE3(const byte* y_buf, const byte* u_buf, const byte* v_buf, byte* dst_argb, int width)
{
    __asm {
        push       esi
        push       edi
        mov        eax, [esp + 8 + 4]   // Y
        mov        esi, [esp + 8 + 8]   // U
        mov        edi, [esp + 8 + 12]  // V
        mov        edx, [esp + 8 + 16]  // argb
        mov        ecx, [esp + 8 + 20]  // width
        sub        edi, esi
        pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha

    convertloop :
        READYUV422
        YUVTORGB(kYuvConstants)
        STOREARGB

        sub        ecx, 8
        jg         convertloop

        pop        edi
        pop        esi
        ret
    }
}

// Read 8 UV from 422, upsample to 16 UV.
#define READYUV422_AVX2 __asm {                                                \
    __asm vmovq      xmm0, qword ptr [esi]        /* U */         /* NOLINT */ \
    __asm vmovq      xmm1, qword ptr [esi + edi]  /* V */         /* NOLINT */ \
    __asm lea        esi,  [esi + 8]                                           \
    __asm vpunpcklbw ymm0, ymm0, ymm1             /* UV */                     \
    __asm vpermq     ymm0, ymm0, 0xd8                                          \
    __asm vpunpcklwd ymm0, ymm0, ymm0             /* UVUV (upsample) */        \
  }

// Convert 16 pixels: 16 UV and 16 Y.
#define YUVTORGB_AVX2(YuvConstants) __asm {                                    \
    /* Step 1: Find 8 UV contributions to 16 R,G,B values */                   \
    __asm vpmaddubsw ymm2, ymm0, YuvConstants.kUVToR        /* scale R UV */   \
    __asm vpmaddubsw ymm1, ymm0, YuvConstants.kUVToG        /* scale G UV */   \
    __asm vpmaddubsw ymm0, ymm0, YuvConstants.kUVToB        /* scale B UV */   \
    __asm vmovdqu    ymm3, YuvConstants.kUVBiasR                               \
    __asm vpsubw     ymm2, ymm3, ymm2                                          \
    __asm vmovdqu    ymm3, YuvConstants.kUVBiasG                               \
    __asm vpsubw     ymm1, ymm3, ymm1                                          \
    __asm vmovdqu    ymm3, YuvConstants.kUVBiasB                               \
    __asm vpsubw     ymm0, ymm3, ymm0                                          \
    /* Step 2: Find Y contribution to 16 R,G,B values */                       \
    __asm vmovdqu    xmm3, [eax]                  /* NOLINT */                 \
    __asm lea        eax, [eax + 16]                                           \
    __asm vpermq     ymm3, ymm3, 0xd8                                          \
    __asm vpunpcklbw ymm3, ymm3, ymm3                                          \
    __asm vpmulhuw   ymm3, ymm3, YuvConstants.kYToRgb                          \
    __asm vpaddsw    ymm0, ymm0, ymm3           /* B += Y */                   \
    __asm vpaddsw    ymm1, ymm1, ymm3           /* G += Y */                   \
    __asm vpaddsw    ymm2, ymm2, ymm3           /* R += Y */                   \
    __asm vpsraw     ymm0, ymm0, 6                                             \
    __asm vpsraw     ymm1, ymm1, 6                                             \
    __asm vpsraw     ymm2, ymm2, 6                                             \
    __asm vpackuswb  ymm0, ymm0, ymm0           /* B */                        \
    __asm vpackuswb  ymm1, ymm1, ymm1           /* G */                        \
    __asm vpackuswb  ymm2, ymm2, ymm2           /* R */                        \
  }

// Store 16 ARGB values.
#define STOREARGB_AVX2 __asm {                                                 \
    /* Step 3: Weave into ARGB */                                              \
    __asm vpunpcklbw ymm0, ymm0, ymm1           /* BG */                       \
    __asm vpermq     ymm0, ymm0, 0xd8                                          \
    __asm vpunpcklbw ymm2, ymm2, ymm5           /* RA */                       \
    __asm vpermq     ymm2, ymm2, 0xd8                                          \
    __asm vpunpcklwd ymm1, ymm0, ymm2           /* BGRA first 8 pixels */      \
    __asm vpunpckhwd ymm0, ymm0, ymm2           /* BGRA next 8 pixels */       \
    __asm vmovdqu    0[edx], ymm1                                              \
    __asm vmovdqu    32[edx], ymm0                                             \
    __asm lea        edx,  [edx + 64]                                          \
  }


// 16 pixels
// 8 UV values upsampled to 16 UV, mixed with 16 Y producing 16 ARGB (64 bytes).
__declspec(naked) void I422ToARGBRow_AVX2(const byte* y_buf, const byte* u_buf, const byte* v_buf, byte* dst_argb, int width)
{
    __asm {
        push       esi
        push       edi
        mov        eax, [esp + 8 + 4]   // Y
        mov        esi, [esp + 8 + 8]   // U
        mov        edi, [esp + 8 + 12]  // V
        mov        edx, [esp + 8 + 16]  // argb
        mov        ecx, [esp + 8 + 20]  // width
        sub        edi, esi
        vpcmpeqb   ymm5, ymm5, ymm5     // generate 0xffffffffffffffff for alpha

    convertloop :
        READYUV422_AVX2
        YUVTORGB_AVX2(kYuvConstants)
        STOREARGB_AVX2

        sub        ecx, 16
        jg         convertloop

        pop        edi
        pop        esi
        vzeroupper
        ret
    }
}


ANY31(I422ToARGBRow_Any_AVX2, I422ToARGBRow_AVX2, 1, 0, 4, 15)

#endif

/*
void I422ToARGBRow_Any_SSSE3(const byte* y_buf, const byte* u_buf, const byte* v_buf, byte* dst_ptr, int width)
{
    SIMD_ALIGNED(byte temp[64 * 4]);
    memset(temp, 0, 64 * 3);  // for YUY2 and msan
    int r = width & 7;
    int n = width & ~7;
    if (n > 0)
        I422ToARGBRow_SSSE3(y_buf, u_buf, v_buf, dst_ptr, n);

    memcpy(temp, y_buf + n, r);
    memcpy(temp + 64, u_buf + (n >> 1), SS(r, 1));
    memcpy(temp + 128, v_buf + (n >> 1), SS(r, 1));
    I422ToARGBRow_SSSE3(temp, temp + 64, temp + 128, temp + 192, 8);
    memcpy(dst_ptr + n * 4, temp + 192, r * 4);
}
*/


namespace internal
{
    __forceinline long int lround(double x)
    {
#ifdef _M_AMD64
        return _mm_cvtsd_si32(_mm_load_sd(&x));
#else
        _asm
        {
            fld x
                push eax
                fistp dword ptr[esp]
                pop eax
        }
#endif
    }

    __forceinline long int lround(float x)
    {
        return _mm_cvtss_si32(_mm_load_ss(&x));//assume that current rounding mode is always correct (i.e. round to nearest)
    }

    typedef ptrdiff_t aint; // auto int (32/64 bit)

    template<typename T> struct is_signed
    {
        static const bool value = (((T)-1) < 0);
    };
    template<> struct is_signed < float >
    {
        static const bool value = true;
    };
    template<> struct is_signed < double >
    {
        static const bool value = true;
    };

    template<typename IT> byte __inline clamp2byte(IT n)
    {
        return n < 0 ? 0 : (n > 255 ? 255 : (byte)n);
    }

    template<typename IT> byte __inline clamp2byte_u(IT n)
    {
        return n > 255 ? 255 : (byte)n;
    }

    template<typename RT, typename IT, bool issigned> struct clamper;

    template<> struct clamper < byte, float, true >
    {
        static byte dojob(float b)
        {
            return clamp2byte<aint>(internal::lround(b));
        }
    };

    template<> struct clamper < byte, double, true >
    {
        static byte dojob(double b)
        {
            return clamp2byte<aint>(internal::lround(b));
        }
    };

    template<typename IT> struct clamper < byte, IT, true >
    {
        static byte dojob(IT n)
        {
            return clamp2byte<IT>(n);
        }
    };

    template<typename IT> struct clamper < byte, IT, false >
    {
        static byte dojob(IT n)
        {
            return clamp2byte_u<IT>(n);
        }
    };

    template<typename RT, typename IT> struct clamper < RT, IT, false >
    {
        static RT dojob(IT b)
        {
            return b > maximum<RT>::value ? maximum<RT>::value : (RT)b;
        }
    };
    template<typename RT, typename IT> struct clamper < RT, IT, true >
    {
        static RT dojob(IT b)
        {
            return b < minimum<RT>::value ? minimum<RT>::value : (b > maximum<RT>::value ? maximum<RT>::value : (RT)b);
        }
    };

};

template < typename RT, typename IT > __inline RT CLAMP(IT b)
{
    return internal::clamper<RT, IT, internal::is_signed<IT>::value>::dojob(b);
}

template <typename CCC> __inline u32 ARGB(CCC r, CCC g, CCC b, CCC a = 255)
{
    return CLAMP<byte, CCC>(b) | (CLAMP<byte, CCC>(g) << 8) | (CLAMP<byte, CCC>(r) << 16) | (CLAMP<byte, CCC>(a) << 24);
}



// Convert I420 to ARGB.
void img_helper_i420_to_ARGB(const byte* src_y, int src_stride_y, const byte* src_u, int src_stride_u, const byte* src_v, int src_stride_v, byte* dst_argb, int dst_stride_argb, int width, int height)
{

    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }

    typedef void ROWFUNC(const byte* y_buf, const byte* u_buf, const byte* v_buf, byte* rgb_buf, int width);

    auto loopf = [&](ROWFUNC rowfunc, int w)
    {
        byte* dst_argb1 = dst_argb;
        const byte* src_y1 = src_y;
        const byte* src_u1 = src_u;
        const byte* src_v1 = src_v;

        for (int y = 0; y < height; ++y)
        {
            rowfunc(src_y1, src_u1, src_v1, dst_argb1, w);
            dst_argb1 += dst_stride_argb;
            src_y1 += src_stride_y;
            if (y & 1) {
                src_u1 += src_stride_u;
                src_v1 += src_stride_v;
            }
        }
    };

#ifndef MODE64
    if (cpu_detect::CCAPS(cpu_detect::CPU_AVX2))
    {
        if (IS_ALIGNED(width, 16))
            loopf(I422ToARGBRow_AVX2, width);
        else
            loopf(I422ToARGBRow_Any_AVX2, width);
        return;
    }
#endif // MODE64

    //if (cpu_detect::CCAPS(cpu_detect::CPU_SSSE3))
    //{
    //    if (IS_ALIGNED(width, 8))
    //        loopf(I422ToARGBRow_SSSE3, width);
    //    else
    //        loopf(I422ToARGBRow_Any_SSSE3, width);
    //    return;
    //}

    if (cpu_detect::CCAPS(cpu_detect::CPU_SSSE3))
    {
        int ost = width & 7;
        int w = width & ~7;
        if (w) loopf(I422ToARGBRow_SSSE3, w);
        if (!ost) return;

        width = ost;
        dst_argb += w * 4;
        src_y += w;
        src_u += w/2;
        src_v += w/2;
    }

    int addy = src_stride_y - width;
    int addu = src_stride_u - width / 2;
    int addv = src_stride_v - width / 2;

    typedef u32 TSCOLOR;

    for (int y = 0; y < height; y += 2, dst_argb += dst_stride_argb * 2, src_y += src_stride_y + addy, src_u += addu, src_v += addv)
    {
        TSCOLOR *dst0 = (TSCOLOR *)dst_argb;
        TSCOLOR *dst1 = (TSCOLOR *)(dst_argb + dst_stride_argb);
        TSCOLOR *dste = dst0 + width;
        for (; dst0 < dste; ++src_u, ++src_v, dst0 += 2, dst1 += 2, src_y += 2)
        {

            int Y0 = (int)(src_y[0] * 0x0101 * YG) >> 16;
            int Y1 = (int)(src_y[1] * 0x0101 * YG) >> 16;
            int Y2 = (int)(src_y[0 + src_stride_y] * 0x0101 * YG) >> 16;
            int Y3 = (int)(src_y[1 + src_stride_y] * 0x0101 * YG) >> 16;

            byte U = *src_u;
            byte V = *src_v;

            int prer = BR - (V * VR);
            int preg = BG - (V * VG + U * UG);
            int preb = BB - (U * UB);

            dst0[0] = ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
            dst0[1] = ARGB<int>((prer + Y1) >> 6, (preg + Y1) >> 6, (preb + Y1) >> 6);

            dst1[0] = ARGB<int>((prer + Y2) >> 6, (preg + Y2) >> 6, (preb + Y2) >> 6);
            dst1[1] = ARGB<int>((prer + Y3) >> 6, (preg + Y3) >> 6, (preb + Y3) >> 6);

        }
    }
}
