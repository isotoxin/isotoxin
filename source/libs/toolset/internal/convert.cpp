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

//-V:ANY:512

#include "toolset.h"
#include "fourcc.h"

#define WRITEWORD(p, v) *(uint32*)(p) = v
#define IS_ALIGNED(p, a) (!((uintptr_t)(p) & ((a) - 1)))

#define SIMD_ALIGNED(var) __declspec(align(16)) var

// Subsampled source needs to be increase by 1 of not even.
#define SS(width, shift) (((width) + (1 << (shift)) - 1) >> (shift))

// Any 3 planes to 1.
#define ANY31(NAMEANY, ANY_SIMD, UVSHIFT, DUVSHIFT, BPP, MASK)                 \
    void NAMEANY(const uint8* y_buf, const uint8* u_buf, const uint8* v_buf,   \
                 uint8* dst_ptr, int width) {                                  \
      SIMD_ALIGNED(uint8 temp[64 * 4]);                                        \
      memset(temp, 0, 64 * 3);  /* for YUY2 and msan */                        \
      int r = width & MASK;                                                    \
      int n = width & ~MASK;                                                   \
      if (n > 0) { ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, n); }                \
      memcpy(temp, y_buf + n, r);                                              \
      memcpy(temp + 64, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));               \
      memcpy(temp + 128, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));              \
      ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, MASK + 1);             \
      memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 192,                      \
             SS(r, DUVSHIFT) * BPP); }



// Any 1 to 2 with source stride (2 rows of source).  Outputs UV planes.
// 128 byte row allows for 32 avx ARGB pixels.
#define ANY12S(NAMEANY, ANY_SIMD, UVSHIFT, BPP, MASK)                          \
    void NAMEANY(const uint8* src_ptr, int src_stride_ptr,                     \
                 uint8* dst_u, uint8* dst_v, int width) {                      \
      SIMD_ALIGNED(uint8 temp[128 * 4]);                                       \
      memset(temp, 0, 128 * 2);  /* for msan */                                \
      int r = width & MASK;                                                    \
      int n = width & ~MASK;                                                   \
      if (n > 0) { ANY_SIMD(src_ptr, src_stride_ptr, dst_u, dst_v, n); }       \
      memcpy(temp, src_ptr  + (n >> UVSHIFT) * BPP, SS(r, UVSHIFT) * BPP);     \
      memcpy(temp + 128, src_ptr  + src_stride_ptr + (n >> UVSHIFT) * BPP,     \
             SS(r, UVSHIFT) * BPP);                                            \
      if ((width & 1) && BPP == 4) {  /* repeat last 4 bytes for subsampler */ \
        memcpy(temp + SS(r, UVSHIFT) * BPP,                                    \
               temp + SS(r, UVSHIFT) * BPP - BPP, 4);                          \
        memcpy(temp + 128 + SS(r, UVSHIFT) * BPP,                              \
               temp + 128 + SS(r, UVSHIFT) * BPP - BPP, 4);  }                 \
      ANY_SIMD(temp, 128, temp + 256, temp + 384, MASK + 1);                   \
      memcpy(dst_u + (n >> 1), temp + 256, SS(r, 1));                          \
      memcpy(dst_v + (n >> 1), temp + 384, SS(r, 1)); }

// Any 1 to 1.
#define ANY11(NAMEANY, ANY_SIMD, UVSHIFT, SBPP, BPP, MASK)                     \
    void NAMEANY(const uint8* src_ptr, uint8* dst_ptr, int width) {            \
      SIMD_ALIGNED(uint8 temp[128 * 2]);                                       \
      memset(temp, 0, 128);  /* for YUY2 and msan */                           \
      int r = width & MASK;                                                    \
      int n = width & ~MASK;                                                   \
      if (n > 0) { ANY_SIMD(src_ptr, dst_ptr, n); }                            \
      memcpy(temp, src_ptr + (n >> UVSHIFT) * SBPP, SS(r, UVSHIFT) * SBPP);    \
      ANY_SIMD(temp, temp + 128, MASK + 1);                                    \
      memcpy(dst_ptr + n * BPP, temp + 128, r * BPP); }



namespace ts
{
typedef __declspec(align(16)) uint8 uvec8[16];
typedef __declspec(align(16)) int8 vec8[16];
typedef __declspec(align(32)) int32 lvec32[8];


// Shuffle table for converting BGRA to ARGB.
static uvec8 kShuffleMaskBGRAToARGB = {
    3u, 2u, 1u, 0u, 7u, 6u, 5u, 4u, 11u, 10u, 9u, 8u, 15u, 14u, 13u, 12u
};

// Shuffle table for converting ABGR to ARGB.
static uvec8 kShuffleMaskABGRToARGB = {
    2u, 1u, 0u, 3u, 6u, 5u, 4u, 7u, 10u, 9u, 8u, 11u, 14u, 13u, 12u, 15u
};

// Shuffle table for converting RGBA to ARGB.
static uvec8 kShuffleMaskRGBAToARGB = {
    1u, 2u, 3u, 0u, 5u, 6u, 7u, 4u, 9u, 10u, 11u, 8u, 13u, 14u, 15u, 12u
};

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
void I422ToARGBRow_SSSE3( const uint8* y_buf, const uint8* u_buf, const uint8* v_buf, uint8* dst_argb, int width )
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i xmm5 = _mm_set1_epi8( -1 );
    const ptrdiff_t offset = (uint8*)v_buf - (uint8*)u_buf;

    while ( width > 0 ) {
        xmm0 = _mm_cvtsi32_si128( *(uint32*)u_buf );
        xmm1 = _mm_cvtsi32_si128( *(uint32*)( u_buf + offset ) );
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
__declspec(naked) void I422ToARGBRow_SSSE3(const uint8* y_buf, const uint8* u_buf, const uint8* v_buf, uint8* dst_argb, int width)
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
__declspec(naked) void I422ToARGBRow_AVX2(const uint8* y_buf, const uint8* u_buf, const uint8* v_buf, uint8* dst_argb, int width)
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

#define USE_BRANCHLESS 1
#if USE_BRANCHLESS
static __inline int32 clamp0(int32 v) {
    return ((-(v) >> 31) & (v));
}

static __inline int32 clamp255(int32 v) {
    return (((255 - (v)) >> 31) | (v)) & 255;
}

static __inline uint32 Clamp(int32 val) {
    int v = clamp0(val);
    return (uint32)(clamp255(v));
}

static __inline uint32 Abs(int32 v) {
    int m = v >> 31;
    return (v + m) ^ m;
}
#else  // USE_BRANCHLESS
static __inline int32 clamp0(int32 v) {
    return (v < 0) ? 0 : v;
}

static __inline int32 clamp255(int32 v) {
    return (v > 255) ? 255 : v;
}

static __inline uint32 Clamp(int32 val) {
    int v = clamp0(val);
    return (uint32)(clamp255(v));
}

static __inline uint32 Abs(int32 v) {
    return (v < 0) ? -v : v;
}
#endif

#pragma warning (push)
#pragma warning (disable:4244) // conversion from 'ts::uint32' to 'ts::uint8', possible loss of data

// JPEG YUV to RGB reference
// *  R = Y                - V * -1.40200
// *  G = Y - U *  0.34414 - V *  0.71414
// *  B = Y - U * -1.77200

// Y contribution to R,G,B.  Scale and bias.
// TODO(fbarchard): Consider moving constants into a common header.
#define YGJ 16320 /* round(1.000 * 64 * 256 * 256 / 257) */
#define YGBJ 32  /* 64 / 2 */

// U and V contributions to R,G,B.
#define UBJ -113 /* round(-1.77200 * 64) */
#define UGJ 22 /* round(0.34414 * 64) */
#define VGJ 46 /* round(0.71414  * 64) */
#define VRJ -90 /* round(-1.40200 * 64) */

// Bias values to subtract 16 from Y and 128 from U and V.
#define BBJ (UBJ * 128 + YGBJ)
#define BGJ (UGJ * 128 + VGJ * 128 + YGBJ)
#define BRJ (VRJ * 128 + YGBJ)

// C reference code that mimics the YUV assembly.
static __inline void YuvJPixel(uint8 y, uint8 u, uint8 v,
                               uint8* b, uint8* g, uint8* r) {
    uint32 y1 = (uint32)(y * 0x0101 * YGJ) >> 16;
    *b = Clamp((int32)(-(u * UBJ) + y1 + BBJ) >> 6);
    *g = Clamp((int32)(-(v * VGJ + u * UGJ) + y1 + BGJ) >> 6);
    *r = Clamp((int32)(-(v * VRJ) + y1 + BRJ) >> 6);
}

#undef YGJ
#undef YGBJ
#undef UBJ
#undef UGJ
#undef VGJ
#undef VRJ
#undef BBJ
#undef BGJ
#undef BRJ

// BT.601 YUV to RGB reference
//  R = (Y - 16) * 1.164              - V * -1.596
//  G = (Y - 16) * 1.164 - U *  0.391 - V *  0.813
//  B = (Y - 16) * 1.164 - U * -2.018

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
#define BB (UB * 128 + YGB)
#define BG (UG * 128 + VG * 128 + YGB)
#define BR (VR * 128 + YGB)

// C reference code that mimics the YUV assembly.
static __inline void YuvPixel(uint8 y, uint8 u, uint8 v,
                              uint8* b, uint8* g, uint8* r) {
    uint32 y1 = (uint32)(y * 0x0101 * YG) >> 16;
    *b = Clamp((int32)(-(u * UB) + y1 + BB) >> 6);
    *g = Clamp((int32)(-(v * VG + u * UG) + y1 + BG) >> 6);
    *r = Clamp((int32)(-(v * VR) + y1 + BR) >> 6);
}

// C reference code that mimics the YUV assembly.
static __inline void YPixel(uint8 y, uint8* b, uint8* g, uint8* r) {
    uint32 y1 = (uint32)(y * 0x0101 * YG) >> 16;
    *b = Clamp((int32)(y1 + YGB) >> 6);
    *g = Clamp((int32)(y1 + YGB) >> 6);
    *r = Clamp((int32)(y1 + YGB) >> 6);
}

#pragma warning (pop)

static void CopyRow_C(const uint8* src, uint8* dst, int count)
{
    memcpy(dst, src, count);
}
// Copy a plane of data
void CopyPlane(const uint8* src_y, int src_stride_y, uint8* dst_y, int dst_stride_y, int width, int height)
{
    int y;
    void(*CopyRow)(const uint8* src, uint8* dst, int width) = CopyRow_C;
    // Coalesce rows.
    if (src_stride_y == width &&
        dst_stride_y == width) {
        width *= height;
        height = 1;
        src_stride_y = dst_stride_y = 0;
    }
    // Nothing to do.
    if (src_y == dst_y && src_stride_y == dst_stride_y) {
        return;
    }
#if defined(HAS_COPYROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        CopyRow = IS_ALIGNED(width, 32) ? CopyRow_SSE2 : CopyRow_Any_SSE2;
    }
#endif
#if defined(HAS_COPYROW_AVX)
    if (TestCpuFlag(kCpuHasAVX)) {
        CopyRow = IS_ALIGNED(width, 64) ? CopyRow_AVX : CopyRow_Any_AVX;
    }
#endif
#if defined(HAS_COPYROW_ERMS)
    if (TestCpuFlag(kCpuHasERMS)) {
        CopyRow = CopyRow_ERMS;
    }
#endif

    // Copy plane
    for (y = 0; y < height; ++y) {
        CopyRow(src_y, dst_y, width);
        src_y += src_stride_y;
        dst_y += dst_stride_y;
    }
}


#define ARGBToARGB ARGBCopy

// Copy ARGB with optional flipping
int ARGBCopy(const uint8* src_argb, int src_stride_argb, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    if (!src_argb || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_argb = src_argb + (height - 1) * src_stride_argb;
        src_stride_argb = -src_stride_argb;
    }

    CopyPlane(src_argb, src_stride_argb, dst_argb, dst_stride_argb,
              width * 4, height);
    return 0;
}

#pragma region row_c

static void RGB24ToARGBRow_C(const uint8* src_rgb24, uint8* dst_argb, int width) {
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_rgb24[0];
        uint8 g = src_rgb24[1];
        uint8 r = src_rgb24[2];
        dst_argb[0] = b;
        dst_argb[1] = g;
        dst_argb[2] = r;
        dst_argb[3] = 255u;
        dst_argb += 4;
        src_rgb24 += 3;
    }
}

static void ARGB1555ToARGBRow_C(const uint8* src_argb1555, uint8* dst_argb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_argb1555[0] & 0x1f;
        uint8 g = (src_argb1555[0] >> 5) | ((src_argb1555[1] & 0x03) << 3);
        uint8 r = (src_argb1555[1] & 0x7c) >> 2;
        uint8 a = src_argb1555[1] >> 7;
        dst_argb[0] = (b << 3) | (b >> 2);
        dst_argb[1] = (g << 3) | (g >> 2);
        dst_argb[2] = (r << 3) | (r >> 2);
        dst_argb[3] = -a;
        dst_argb += 4;
        src_argb1555 += 2;
    }
}

/*
static void ARGBToRGB24Row_C(const uint8* src_argb, uint8* dst_rgb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_argb[0];
        uint8 g = src_argb[1];
        uint8 r = src_argb[2];
        dst_rgb[0] = b;
        dst_rgb[1] = g;
        dst_rgb[2] = r;
        dst_rgb += 3;
        src_argb += 4;
    }
}

static void ARGBToRAWRow_C(const uint8* src_argb, uint8* dst_rgb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_argb[0];
        uint8 g = src_argb[1];
        uint8 r = src_argb[2];
        dst_rgb[0] = r;
        dst_rgb[1] = g;
        dst_rgb[2] = b;
        dst_rgb += 3;
        src_argb += 4;
    }
}

static void ARGBToRGB565Row_C(const uint8* src_argb, uint8* dst_rgb, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        uint8 b0 = src_argb[0] >> 3;
        uint8 g0 = src_argb[1] >> 2;
        uint8 r0 = src_argb[2] >> 3;
        uint8 b1 = src_argb[4] >> 3;
        uint8 g1 = src_argb[5] >> 2;
        uint8 r1 = src_argb[6] >> 3;
        WRITEWORD(dst_rgb, b0 | (g0 << 5) | (r0 << 11) |
                  (b1 << 16) | (g1 << 21) | (r1 << 27));
        dst_rgb += 4;
        src_argb += 8;
    }
    if (width & 1) {
        uint8 b0 = src_argb[0] >> 3;
        uint8 g0 = src_argb[1] >> 2;
        uint8 r0 = src_argb[2] >> 3;
        *(uint16*)(dst_rgb) = b0 | (g0 << 5) | (r0 << 11);
    }
}
*/

// Use first 4 shuffler values to reorder ARGB channels.
static void ARGBShuffleRow_C(const uint8* src_argb, uint8* dst_argb, const uint8* shuffler, int pix)
{
    int index0 = shuffler[0];
    int index1 = shuffler[1];
    int index2 = shuffler[2];
    int index3 = shuffler[3];
    // Shuffle a row of ARGB.
    int x;
    for (x = 0; x < pix; ++x) {
        // To support in-place conversion.
        uint8 b = src_argb[index0];
        uint8 g = src_argb[index1];
        uint8 r = src_argb[index2];
        uint8 a = src_argb[index3];
        dst_argb[0] = b;
        dst_argb[1] = g;
        dst_argb[2] = r;
        dst_argb[3] = a;
        src_argb += 4;
        dst_argb += 4;
    }
}

#pragma endregion row_c


int RGB24ToARGB(const uint8* src_rgb24, int src_stride_rgb24, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*RGB24ToARGBRow)(const uint8* src_rgb, uint8* dst_argb, int pix) =
        RGB24ToARGBRow_C;
    if (!src_rgb24 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_rgb24 = src_rgb24 + (height - 1) * src_stride_rgb24;
        src_stride_rgb24 = -src_stride_rgb24;
    }
    // Coalesce rows.
    if (src_stride_rgb24 == width * 3 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_rgb24 = dst_stride_argb = 0;
    }
#if defined(HAS_RGB24TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        RGB24ToARGBRow = RGB24ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 16)) {
            RGB24ToARGBRow = RGB24ToARGBRow_SSSE3;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        RGB24ToARGBRow(src_rgb24, dst_argb, width);
        src_rgb24 += src_stride_rgb24;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

// Shuffle ARGB channel order.  e.g. BGRA to ARGB.
int ARGBShuffle(const uint8* src_bgra, int src_stride_bgra, uint8* dst_argb, int dst_stride_argb, const uint8* shuffler, int width, int height)
{
    int y;
    void(*ARGBShuffleRow)(const uint8* src_bgra, uint8* dst_argb,
                          const uint8* shuffler, int pix) = ARGBShuffleRow_C;
    if (!src_bgra || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_bgra = src_bgra + (height - 1) * src_stride_bgra;
        src_stride_bgra = -src_stride_bgra;
    }
    // Coalesce rows.
    if (src_stride_bgra == width * 4 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_bgra = dst_stride_argb = 0;
    }
#if defined(HAS_ARGBSHUFFLEROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        ARGBShuffleRow = ARGBShuffleRow_Any_SSE2;
        if (IS_ALIGNED(width, 4)) {
            ARGBShuffleRow = ARGBShuffleRow_SSE2;
        }
    }
#endif
#if defined(HAS_ARGBSHUFFLEROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        ARGBShuffleRow = ARGBShuffleRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            ARGBShuffleRow = ARGBShuffleRow_SSSE3;
        }
    }
#endif
#if defined(HAS_ARGBSHUFFLEROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        ARGBShuffleRow = ARGBShuffleRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            ARGBShuffleRow = ARGBShuffleRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        ARGBShuffleRow(src_bgra, dst_argb, shuffler, width);
        src_bgra += src_stride_bgra;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

#if 0
// Convert BGRA to ARGB.
static int BGRAToARGB(const uint8* src_bgra, int src_stride_bgra, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_bgra, src_stride_bgra,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskBGRAToARGB),
                       width, height);
}

// Convert ARGB to BGRA (same as BGRAToARGB).
static int ARGBToBGRA(const uint8* src_bgra, int src_stride_bgra, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_bgra, src_stride_bgra,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskBGRAToARGB),
                       width, height);
}

// Convert ABGR to ARGB.
static int ABGRToARGB(const uint8* src_abgr, int src_stride_abgr, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_abgr, src_stride_abgr,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskABGRToARGB),
                       width, height);
}

// Convert ARGB to ABGR to (same as ABGRToARGB).
static int ARGBToABGR(const uint8* src_abgr, int src_stride_abgr, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_abgr, src_stride_abgr,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskABGRToARGB),
                       width, height);
}

// Convert RGBA to ARGB.
static int RGBAToARGB(const uint8* src_rgba, int src_stride_rgba, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_rgba, src_stride_rgba,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskRGBAToARGB),
                       width, height);
}

#endif


// Convert ARGB1555 to ARGB.
int ARGB1555ToARGB(const uint8* src_argb1555, int src_stride_argb1555, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*ARGB1555ToARGBRow)(const uint8* src_argb1555, uint8* dst_argb,
                             int pix) = ARGB1555ToARGBRow_C;
    if (!src_argb1555 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_argb1555 = src_argb1555 + (height - 1) * src_stride_argb1555;
        src_stride_argb1555 = -src_stride_argb1555;
    }
    // Coalesce rows.
    if (src_stride_argb1555 == width * 2 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_argb1555 = dst_stride_argb = 0;
    }
#if defined(HAS_ARGB1555TOARGBROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        ARGB1555ToARGBRow = ARGB1555ToARGBRow_Any_SSE2;
        if (IS_ALIGNED(width, 8)) {
            ARGB1555ToARGBRow = ARGB1555ToARGBRow_SSE2;
        }
    }
#endif
#if defined(HAS_ARGB1555TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        ARGB1555ToARGBRow = ARGB1555ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            ARGB1555ToARGBRow = ARGB1555ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        ARGB1555ToARGBRow(src_argb1555, dst_argb, width);
        src_argb1555 += src_stride_argb1555;
        dst_argb += dst_stride_argb;
    }
    return 0;
}



// Convert I420 to ARGB.
void img_helper_i420_to_ARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height)
{

    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }

    typedef void ROWFUNC(const uint8* y_buf, const uint8* u_buf, const uint8* v_buf, uint8* rgb_buf, int width);


    auto loopf = [&](ROWFUNC rowfunc, int w)
    {
        uint8* dst_argb1 = dst_argb;
        const uint8* src_y1 = src_y;
        const uint8* src_u1 = src_u;
        const uint8* src_v1 = src_v;

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
    if (CCAPS(CPU_AVX2))
    {
        if (IS_ALIGNED(width, 16))
            loopf( I422ToARGBRow_AVX2, width );
        else
            loopf( I422ToARGBRow_Any_AVX2, width );
        return;
    }
#endif // MODE64

    //if (CCAPS(CPU_SSSE3))
    //{
    //    if (IS_ALIGNED(width, 8))
    //        loopf(I422ToARGBRow_SSSE3, width);
    //    else
    //        loopf(I422ToARGBRow_Any_SSSE3, width);
    //    return;
    //}

    if (CCAPS(CPU_SSSE3))
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
    int addu = src_stride_u - width/2;
    int addv = src_stride_v - width/2;

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

            uint8 U = *src_u;
            uint8 V = *src_v;

            int prer = BR - (V * VR);
            int preg = BG - (V * VG + U * UG);
            int preb = BB - (U * UB);

            dst0[0] = ts::ARGB<int>((prer + Y0) >> 6, (preg + Y0) >> 6, (preb + Y0) >> 6);
            dst0[1] = ts::ARGB<int>((prer + Y1) >> 6, (preg + Y1) >> 6, (preb + Y1) >> 6);

            dst1[0] = ts::ARGB<int>((prer + Y2) >> 6, (preg + Y2) >> 6, (preb + Y2) >> 6);
            dst1[1] = ts::ARGB<int>((prer + Y3) >> 6, (preg + Y3) >> 6, (preb + Y3) >> 6);

        }
    }
}

static const uvec8 kAddUV128 = {
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
    128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u
};

static const vec8 kARGBToU = {
    112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0
};

static const vec8 kARGBToV = {
    -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0,
};

// Constants for ARGB.
static const vec8 kARGBToY = {
    13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0
};

static const uvec8 kAddY16 = {
    16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u
};

static void ARGBToUVRow_SSSE3( const uint8* src_argb, int src_stride_argb, uint8* dst_u, uint8* dst_v, int width )
{
    size_t delta = dst_v - dst_u;
    for ( ; width > 0; width -= 16, src_argb += 64, dst_u += 8 )
    {
        __m128i t5 = _mm_load_si128( ( const __m128i * )kAddUV128 );
        __m128i t6 = _mm_load_si128( ( const __m128i * )kARGBToV );
        __m128i t7 = _mm_load_si128( ( const __m128i * )kARGBToU );

        __m128i t0 = _mm_avg_epu8( _mm_lddqu_si128( ( const __m128i * )src_argb ), _mm_lddqu_si128( ( const __m128i * )( src_argb + src_stride_argb ) ) );
        __m128i t1 = _mm_avg_epu8( _mm_lddqu_si128( ( const __m128i * )(src_argb + 16) ), _mm_lddqu_si128( ( const __m128i * )( src_argb + src_stride_argb + 16 ) ) );
        __m128i t2 = _mm_avg_epu8( _mm_lddqu_si128( ( const __m128i * )( src_argb + 32 ) ), _mm_lddqu_si128( ( const __m128i * )( src_argb + src_stride_argb + 32 ) ) );
        __m128i t3 = _mm_avg_epu8( _mm_lddqu_si128( ( const __m128i * )( src_argb + 48 ) ), _mm_lddqu_si128( ( const __m128i * )( src_argb + src_stride_argb + 48 ) ) );

        __m128 x0 = _mm_shuffle_ps( ( const __m128 & )t0, ( const __m128 & )t1, 0x88 );
        __m128 x1 = _mm_shuffle_ps( ( const __m128 & )t0, ( const __m128 & )t1, 0xdd );
        __m128i tt0 = _mm_avg_epu8( ( const __m128i & )x0, ( const __m128i & )x1 );

        __m128 y0 = _mm_shuffle_ps( ( const __m128 & )t2, ( const __m128 & )t3, 0x88 );
        __m128 y1 = _mm_shuffle_ps( ( const __m128 & )t2, ( const __m128 & )t3, 0xdd );
        __m128i tt2 = _mm_avg_epu8( ( const __m128i & )y0, ( const __m128i & )y1 );

        __m128i uv = _mm_add_epi8( _mm_packs_epi16( _mm_srai_epi16( _mm_hadd_epi16( _mm_maddubs_epi16( tt0, t7 ), _mm_maddubs_epi16( tt2, t7 ) ), 8 ),
            _mm_srai_epi16( _mm_hadd_epi16( _mm_maddubs_epi16( tt0, t6 ), _mm_maddubs_epi16( tt2, t6 ) ), 8 ) ), t5 );

        _mm_storel_pi( ( __m64* )dst_u, ( const __m128 & )uv );
        _mm_storeh_pi( ( __m64* )( dst_u + delta ), ( const __m128 & )uv );
    }
}

static void ARGBToYRow_SSSE3( const uint8* src_argb, uint8* dst_y, int pix )
{
    for ( ; pix > 0; pix -= 16, src_argb += 64, dst_y += 16 )
    {
        __m128i t4 = _mm_load_si128( ( const __m128i * )kARGBToY );
        __m128i t5 = _mm_load_si128( ( const __m128i * )kAddY16 );

        _mm_storeu_si128( ( __m128i * )dst_y,
            _mm_add_epi8( _mm_packus_epi16( _mm_srli_epi16(
                _mm_hadd_epi16( _mm_maddubs_epi16( _mm_lddqu_si128( ( const __m128i * )src_argb ), t4 ), _mm_maddubs_epi16( _mm_lddqu_si128( ( const __m128i * )( src_argb + 16 ) ), t4 ) ), 7 ), _mm_srli_epi16(
                    _mm_hadd_epi16( _mm_maddubs_epi16( _mm_lddqu_si128( ( const __m128i * )( src_argb + 32 ) ), t4 ), _mm_maddubs_epi16( _mm_lddqu_si128( ( const __m128i * )( src_argb + 48 ) ), t4 ) ), 7 ) ), t5 ) );
    }
}

ANY12S(ARGBToUVRow_Any_SSSE3, ARGBToUVRow_SSSE3, 0, 4, 15)
ANY11(ARGBToYRow_Any_SSSE3, ARGBToYRow_SSSE3, 0, 4, 1, 15)

#ifndef MODE64

// vpermd for vphaddw + vpackuswb vpermd.
static const lvec32 kPermdARGBToY_AVX = {
    0, 4, 1, 5, 2, 6, 3, 7
};

// vpshufb for vphaddw + vpackuswb packed to shorts.
static const lvec8 kShufARGBToUV_AVX = {
    0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
    0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15
};


// Convert 32 ARGB pixels (128 bytes) to 32 Y values.
__declspec(naked) void ARGBToYRow_AVX2(const uint8* src_argb, uint8* dst_y, int pix)
{
    __asm {
        mov        eax, [esp + 4]   /* src_argb */
        mov        edx, [esp + 8]   /* dst_y */
        mov        ecx, [esp + 12]  /* pix */
        vbroadcastf128 ymm4, kARGBToY
        vbroadcastf128 ymm5, kAddY16
        vmovdqu    ymm6, kPermdARGBToY_AVX

    convertloop :
        vmovdqu    ymm0, [eax]
        vmovdqu    ymm1, [eax + 32]
        vmovdqu    ymm2, [eax + 64]
        vmovdqu    ymm3, [eax + 96]
        vpmaddubsw ymm0, ymm0, ymm4
        vpmaddubsw ymm1, ymm1, ymm4
        vpmaddubsw ymm2, ymm2, ymm4
        vpmaddubsw ymm3, ymm3, ymm4
        lea        eax, [eax + 128]
        vphaddw    ymm0, ymm0, ymm1  // mutates.
        vphaddw    ymm2, ymm2, ymm3
        vpsrlw     ymm0, ymm0, 7
        vpsrlw     ymm2, ymm2, 7
        vpackuswb  ymm0, ymm0, ymm2  // mutates.
        vpermd     ymm0, ymm6, ymm0  // For vphaddw + vpackuswb mutation.
        vpaddb     ymm0, ymm0, ymm5  // add 16 for Y
        vmovdqu[edx], ymm0
        lea        edx, [edx + 32]
        sub        ecx, 32
        jg         convertloop
        vzeroupper
        ret
    }
}

__declspec(naked) void ARGBToUVRow_AVX2(const uint8* src_argb0, int src_stride_argb, uint8* dst_u, uint8* dst_v, int width)
{
    __asm {
        push       esi
        push       edi
        mov        eax, [esp + 8 + 4]   // src_argb
        mov        esi, [esp + 8 + 8]   // src_stride_argb
        mov        edx, [esp + 8 + 12]  // dst_u
        mov        edi, [esp + 8 + 16]  // dst_v
        mov        ecx, [esp + 8 + 20]  // pix
        vbroadcastf128 ymm5, kAddUV128
        vbroadcastf128 ymm6, kARGBToV
        vbroadcastf128 ymm7, kARGBToU
        sub        edi, edx             // stride from u to v

    convertloop :
        /* step 1 - subsample 32x2 argb pixels to 16x1 */
        vmovdqu    ymm0, [eax]
        vmovdqu    ymm1, [eax + 32]
        vmovdqu    ymm2, [eax + 64]
        vmovdqu    ymm3, [eax + 96]
        vpavgb     ymm0, ymm0, [eax + esi]
        vpavgb     ymm1, ymm1, [eax + esi + 32]
        vpavgb     ymm2, ymm2, [eax + esi + 64]
        vpavgb     ymm3, ymm3, [eax + esi + 96]
        lea        eax, [eax + 128]
        vshufps    ymm4, ymm0, ymm1, 0x88
        vshufps    ymm0, ymm0, ymm1, 0xdd
        vpavgb     ymm0, ymm0, ymm4  // mutated by vshufps
        vshufps    ymm4, ymm2, ymm3, 0x88
        vshufps    ymm2, ymm2, ymm3, 0xdd
        vpavgb     ymm2, ymm2, ymm4  // mutated by vshufps

        // step 2 - convert to U and V
        // from here down is very similar to Y code except
        // instead of 32 different pixels, its 16 pixels of U and 16 of V
        vpmaddubsw ymm1, ymm0, ymm7  // U
        vpmaddubsw ymm3, ymm2, ymm7
        vpmaddubsw ymm0, ymm0, ymm6  // V
        vpmaddubsw ymm2, ymm2, ymm6
        vphaddw    ymm1, ymm1, ymm3  // mutates
        vphaddw    ymm0, ymm0, ymm2
        vpsraw     ymm1, ymm1, 8
        vpsraw     ymm0, ymm0, 8
        vpacksswb  ymm0, ymm1, ymm0  // mutates
        vpermq     ymm0, ymm0, 0xd8  // For vpacksswb
        vpshufb    ymm0, ymm0, kShufARGBToUV_AVX  // For vshufps + vphaddw
        vpaddb     ymm0, ymm0, ymm5  // -> unsigned

        // step 3 - store 16 U and 16 V values
        vextractf128[edx], ymm0, 0 // U
        vextractf128[edx + edi], ymm0, 1 // V
        lea        edx, [edx + 16]
        sub        ecx, 32
        jg         convertloop

        pop        edi
        pop        esi
        vzeroupper
        ret
    }
}

ANY11(ARGBToYRow_Any_AVX2, ARGBToYRow_AVX2, 0, 4, 1, 31)
ANY12S(ARGBToUVRow_Any_AVX2, ARGBToUVRow_AVX2, 0, 4, 31)

#endif

static INLINE uint8 RGB_Y(TSCOLOR c)
{
    //return (uint8)( ts::lround( (0.257*(float)RED(c)) + (0.504*(float)GREEN(c)) + (0.098*(float)BLUE(c)) + 16) );
    return (uint8)((RED(c) * 16843 + GREEN(c) * 33030 + BLUE(c) * 6423 + 1048576) >> 16);
}

static INLINE uint8 RGB_U(TSCOLOR c)
{
    //return (uint8)( ts::lround(-(0.148*(float)RED(c)) - (0.291*(float)GREEN(c)) + (0.439*(float)BLUE(c)) + 128) );
    return (uint8)((8388608 - RED(c) * 9699 - GREEN(c) * 19071 + BLUE(c) * 28770) >> 16);
}

static INLINE uint8 RGB_V(TSCOLOR c)
{
    //return (uint8)( 0xff & ts::lround((0.439*(float)RED(c)) - (0.368*(float)GREEN(c)) - (0.071*(float)BLUE(c)) + 128) );
    return (uint8)((RED(c) * 28770 - GREEN(c) * 24117 - BLUE(c) * 4653 + 8388608) >> 16);
}

#ifdef MODE64
// see shrink2x.asm
extern "C" uint32 coloravg( uint32 c1, uint32 c2, uint32 c3, uint32 c4 );
#else

TSCOLOR INLINE coloravg(TSCOLOR c1, TSCOLOR c2, TSCOLOR c3, TSCOLOR c4)
{
    //return ARGB<uint8>((RED(c1) + RED(c2) + RED(c3) + RED(c4)) / 4, (GREEN(c1) + GREEN(c2) + GREEN(c3) + GREEN(c4)) / 4, (BLUE(c1) + BLUE(c2) + BLUE(c3) + BLUE(c4)) / 4);
    _asm
    {
        mov eax, c1
        mov ecx, c2
        mov ebx, eax
        mov edx, ecx
        and eax, 0x00FF00FF
        and ebx, 0x0000FF00

        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        mov ecx, c3
        mov edx, ecx
        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        mov ecx, c4
        mov edx, ecx
        and ecx, 0x00FF00FF
        and edx, 0x0000FF00
        add eax, ecx
        add ebx, edx

        shr eax, 2
        shr ebx, 2
        and eax, 0x00FF00FF
        and ebx, 0x0000FF00
        or  eax, ebx

    }
}
#endif

// Convert ARGB to I420.
void img_helper_ARGB_to_i420(const uint8* src_argb, int src_stride_argb, uint8* dst_y, int dst_stride_y, uint8* dst_u, int dst_stride_u, uint8* dst_v, int dst_stride_v, int width, int height)
{


    typedef void ROWFUNC_Y(const uint8* src_argb, uint8* dst_y, int width);
    typedef void ROWFUNC_UV(const uint8* src_argb0, int src_stride_argb, uint8* dst_u, uint8* dst_v, int width);

    auto loopf = [&](ROWFUNC_Y rowfunc_y, ROWFUNC_UV rowfunc_uv)
    {
        int my = (height-1);
        for (int y = 0; y < my; y += 2)
        {
            rowfunc_uv(src_argb, src_stride_argb, dst_u, dst_v, width);
            rowfunc_y(src_argb, dst_y, width);
            rowfunc_y(src_argb + src_stride_argb, dst_y + dst_stride_y, width);
            src_argb += src_stride_argb * 2;
            dst_y += dst_stride_y * 2;
            dst_u += dst_stride_u;
            dst_v += dst_stride_v;
        }
        if (height & 1) {
            rowfunc_uv(src_argb, 0, dst_u, dst_v, width);
            rowfunc_y(src_argb, dst_y, width);
        }
    };

    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_argb = src_argb + (height - 1) * src_stride_argb;
        src_stride_argb = -src_stride_argb;
    }

#ifndef MODE64
    if (CCAPS(CPU_AVX2))
    {
        if (IS_ALIGNED(width, 32))
            loopf(ARGBToYRow_AVX2, ARGBToUVRow_AVX2);
        else
            loopf(ARGBToYRow_Any_AVX2, ARGBToUVRow_Any_AVX2);
        return;
    }
#endif

    if (CCAPS(CPU_SSSE3))
    {
        if (IS_ALIGNED(width, 16))
            loopf(ARGBToYRow_SSSE3, ARGBToUVRow_SSSE3);
        else
            loopf(ARGBToYRow_Any_SSSE3, ARGBToUVRow_Any_SSSE3);
        return;
    }

    int addy = dst_stride_y*2 - width;
    int my = height-1;
    int mx = width-1;
    for (int y = 0; y < my; y += 2, dst_y += addy, src_argb += src_stride_argb * 2)
    {
        const TSCOLOR *clr0 = (const TSCOLOR *)src_argb;
        const TSCOLOR *clr1 = (const TSCOLOR *)(src_argb + src_stride_argb);

        for (int x = 0; x < mx; x += 2, dst_y += 2, clr0 += 2, clr1 += 2, ++dst_u, ++dst_v)
        {
            TSCOLOR c1 = clr0[0];
            TSCOLOR c2 = clr0[1];
            TSCOLOR c3 = clr1[0];
            TSCOLOR c4 = clr1[1];

            dst_y[0] = RGB_Y(c1);
            dst_y[1] = RGB_Y(c2);
            dst_y[0 + dst_stride_y] = RGB_Y(c3);
            dst_y[1 + dst_stride_y] = RGB_Y(c4);

            TSCOLOR cc = coloravg(c1, c2, c3, c4);

            *dst_u = RGB_U(cc);
            *dst_v = RGB_V(cc);
        }
    }

}




} // namespace ts