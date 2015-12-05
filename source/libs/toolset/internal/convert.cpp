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
      if (n > 0) {                                                             \
        ANY_SIMD(y_buf, u_buf, v_buf, dst_ptr, n);                             \
            }                                                                        \
      memcpy(temp, y_buf + n, r);                                              \
      memcpy(temp + 64, u_buf + (n >> UVSHIFT), SS(r, UVSHIFT));               \
      memcpy(temp + 128, v_buf + (n >> UVSHIFT), SS(r, UVSHIFT));              \
      ANY_SIMD(temp, temp + 64, temp + 128, temp + 192, MASK + 1);             \
      memcpy(dst_ptr + (n >> DUVSHIFT) * BPP, temp + 192,                      \
             SS(r, DUVSHIFT) * BPP);                                           \
        }


namespace ts
{
typedef __declspec(align(16)) uint8 uvec8[16];


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


ANY31(I422ToARGBRow_Any_SSSE3, I422ToARGBRow_SSSE3, 1, 0, 4, 7)
ANY31(I422ToARGBRow_Any_AVX2, I422ToARGBRow_AVX2, 1, 0, 4, 15)




struct FourCCAliasEntry {
    uint32 alias;
    uint32 canonical;
};

static const struct FourCCAliasEntry kFourCCAliases[] = {
    { FOURCC_IYUV, FOURCC_I420 },
    { FOURCC_YU16, FOURCC_I422 },
    { FOURCC_YU24, FOURCC_I444 },
    { FOURCC_YUYV, FOURCC_YUY2 },
    { FOURCC_YUVS, FOURCC_YUY2 },  // kCMPixelFormat_422YpCbCr8_yuvs
    { FOURCC_HDYC, FOURCC_UYVY },
    { FOURCC_2VUY, FOURCC_UYVY },  // kCMPixelFormat_422YpCbCr8
    { FOURCC_JPEG, FOURCC_MJPG },  // Note: JPEG has DHT while MJPG does not.
    { FOURCC_DMB1, FOURCC_MJPG },
    { FOURCC_BA81, FOURCC_BGGR },  // deprecated.
    { FOURCC_RGB3, FOURCC_RAW },
    { FOURCC_BGR3, FOURCC_24BG },
    { FOURCC_CM32, FOURCC_BGRA },  // kCMPixelFormat_32ARGB
    { FOURCC_CM24, FOURCC_RAW },  // kCMPixelFormat_24RGB
    { FOURCC_L555, FOURCC_RGBO },  // kCMPixelFormat_16LE555
    { FOURCC_L565, FOURCC_RGBP },  // kCMPixelFormat_16LE565
    { FOURCC_5551, FOURCC_RGBO },  // kCMPixelFormat_16LE5551
};

uint32 CanonicalFourCC(uint32 fourcc) {
    int i;
    for (i = 0; i < ARRAY_SIZE(kFourCCAliases); ++i) {
        if (kFourCCAliases[i].alias == fourcc) {
            return kFourCCAliases[i].canonical;
        }
    }
    // Not an alias, so return it as-is.
    return fourcc;
}

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

static void I411ToARGBRow_C(const uint8* src_y, const uint8* src_u, const uint8* src_v, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 3; x += 4) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_y[1], src_u[0], src_v[0],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        YuvPixel(src_y[2], src_u[0], src_v[0],
                 rgb_buf + 8, rgb_buf + 9, rgb_buf + 10);
        rgb_buf[11] = 255;
        YuvPixel(src_y[3], src_u[0], src_v[0],
                 rgb_buf + 12, rgb_buf + 13, rgb_buf + 14);
        rgb_buf[15] = 255;
        src_y += 4;
        src_u += 1;
        src_v += 1;
        rgb_buf += 16;  // Advance 4 pixels.
    }
    if (width & 2) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_y[1], src_u[0], src_v[0],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_y += 2;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

static void I444ToARGBRow_C(const uint8* src_y, const uint8* src_u, const uint8* src_v, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        src_y += 1;
        src_u += 1;
        src_v += 1;
        rgb_buf += 4;  // Advance 1 pixel.
    }
}

#pragma region row_c

static void YUY2ToARGBRow_C(const uint8* src_yuy2, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_yuy2[0], src_yuy2[1], src_yuy2[3],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_yuy2[2], src_yuy2[1], src_yuy2[3],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_yuy2 += 4;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_yuy2[0], src_yuy2[1], src_yuy2[3],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

static void UYVYToARGBRow_C(const uint8* src_uyvy, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_uyvy[1], src_uyvy[0], src_uyvy[2],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_uyvy[3], src_uyvy[0], src_uyvy[2],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_uyvy += 4;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_uyvy[1], src_uyvy[0], src_uyvy[2],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

static void J422ToARGBRow_C(const uint8* src_y, const uint8* src_u, const uint8* src_v, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvJPixel(src_y[0], src_u[0], src_v[0],
                  rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvJPixel(src_y[1], src_u[0], src_v[0],
                  rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_y += 2;
        src_u += 1;
        src_v += 1;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvJPixel(src_y[0], src_u[0], src_v[0],
                  rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

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

static void RAWToARGBRow_C(const uint8* src_raw, uint8* dst_argb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 r = src_raw[0];
        uint8 g = src_raw[1];
        uint8 b = src_raw[2];
        dst_argb[0] = b;
        dst_argb[1] = g;
        dst_argb[2] = r;
        dst_argb[3] = 255u;
        dst_argb += 4;
        src_raw += 3;
    }
}

static void RGB565ToARGBRow_C(const uint8* src_rgb565, uint8* dst_argb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_rgb565[0] & 0x1f;
        uint8 g = (src_rgb565[0] >> 5) | ((src_rgb565[1] & 0x07) << 3);
        uint8 r = src_rgb565[1] >> 3;
        dst_argb[0] = (b << 3) | (b >> 2);
        dst_argb[1] = (g << 2) | (g >> 4);
        dst_argb[2] = (r << 3) | (r >> 2);
        dst_argb[3] = 255u;
        dst_argb += 4;
        src_rgb565 += 2;
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

static void ARGB4444ToARGBRow_C(const uint8* src_argb4444, uint8* dst_argb, int width)
{
    int x;
    for (x = 0; x < width; ++x) {
        uint8 b = src_argb4444[0] & 0x0f;
        uint8 g = src_argb4444[0] >> 4;
        uint8 r = src_argb4444[1] & 0x0f;
        uint8 a = src_argb4444[1] >> 4;
        dst_argb[0] = (b << 4) | b;
        dst_argb[1] = (g << 4) | g;
        dst_argb[2] = (r << 4) | r;
        dst_argb[3] = (a << 4) | a;
        dst_argb += 4;
        src_argb4444 += 2;
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

static void I400ToARGBRow_C(const uint8* src_y, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YPixel(src_y[0], rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YPixel(src_y[1], rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_y += 2;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YPixel(src_y[0], rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

static void NV12ToARGBRow_C(const uint8* src_y, const uint8* src_uv, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], src_uv[0], src_uv[1],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_y[1], src_uv[0], src_uv[1],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_y += 2;
        src_uv += 2;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_uv[0], src_uv[1],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

static void NV21ToARGBRow_C(const uint8* src_y, const uint8* src_vu, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], src_vu[1], src_vu[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;

        YuvPixel(src_y[1], src_vu[1], src_vu[0],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;

        src_y += 2;
        src_vu += 2;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_vu[1], src_vu[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

/*
static void NV12ToRGB565Row_C(const uint8* src_y, const uint8* src_uv, uint8* dst_rgb565, int width)
{
    uint8 b0;
    uint8 g0;
    uint8 r0;
    uint8 b1;
    uint8 g1;
    uint8 r1;
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], src_uv[0], src_uv[1], &b0, &g0, &r0);
        YuvPixel(src_y[1], src_uv[0], src_uv[1], &b1, &g1, &r1);
        b0 = b0 >> 3;
        g0 = g0 >> 2;
        r0 = r0 >> 3;
        b1 = b1 >> 3;
        g1 = g1 >> 2;
        r1 = r1 >> 3;
        *(uint32*)(dst_rgb565) = b0 | (g0 << 5) | (r0 << 11) |
            (b1 << 16) | (g1 << 21) | (r1 << 27);
        src_y += 2;
        src_uv += 2;
        dst_rgb565 += 4;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_uv[0], src_uv[1], &b0, &g0, &r0);
        b0 = b0 >> 3;
        g0 = g0 >> 2;
        r0 = r0 >> 3;
        *(uint16*)(dst_rgb565) = b0 | (g0 << 5) | (r0 << 11);
    }
}

static void NV21ToRGB565Row_C(const uint8* src_y, const uint8* vsrc_u, uint8* dst_rgb565, int width)
{
    uint8 b0;
    uint8 g0;
    uint8 r0;
    uint8 b1;
    uint8 g1;
    uint8 r1;
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], vsrc_u[1], vsrc_u[0], &b0, &g0, &r0);
        YuvPixel(src_y[1], vsrc_u[1], vsrc_u[0], &b1, &g1, &r1);
        b0 = b0 >> 3;
        g0 = g0 >> 2;
        r0 = r0 >> 3;
        b1 = b1 >> 3;
        g1 = g1 >> 2;
        r1 = r1 >> 3;
        *(uint32*)(dst_rgb565) = b0 | (g0 << 5) | (r0 << 11) |
            (b1 << 16) | (g1 << 21) | (r1 << 27);
        src_y += 2;
        vsrc_u += 2;
        dst_rgb565 += 4;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], vsrc_u[1], vsrc_u[0], &b0, &g0, &r0);
        b0 = b0 >> 3;
        g0 = g0 >> 2;
        r0 = r0 >> 3;
        *(uint16*)(dst_rgb565) = b0 | (g0 << 5) | (r0 << 11);
    }
}
*/

// Also used for 420
static void I422ToARGBRow_C(const uint8* src_y, const uint8* src_u, const uint8* src_v, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
        YuvPixel(src_y[1], src_u[0], src_v[0],
                 rgb_buf + 4, rgb_buf + 5, rgb_buf + 6);
        rgb_buf[7] = 255;
        src_y += 2;
        src_u += 1;
        src_v += 1;
        rgb_buf += 8;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        rgb_buf[3] = 255;
    }
}

/*
static void I422ToRGB24Row_C(const uint8* src_y, const uint8* src_u, const uint8* src_v, uint8* rgb_buf, int width)
{
    int x;
    for (x = 0; x < width - 1; x += 2) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
        YuvPixel(src_y[1], src_u[0], src_v[0],
                 rgb_buf + 3, rgb_buf + 4, rgb_buf + 5);
        src_y += 2;
        src_u += 1;
        src_v += 1;
        rgb_buf += 6;  // Advance 2 pixels.
    }
    if (width & 1) {
        YuvPixel(src_y[0], src_u[0], src_v[0],
                 rgb_buf + 0, rgb_buf + 1, rgb_buf + 2);
    }
}
*/

#pragma endregion row_c


static int YUY2ToARGB(const uint8* src_yuy2, int src_stride_yuy2, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*YUY2ToARGBRow)(const uint8* src_yuy2, uint8* dst_argb, int pix) =
        YUY2ToARGBRow_C;
    if (!src_yuy2 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_yuy2 = src_yuy2 + (height - 1) * src_stride_yuy2;
        src_stride_yuy2 = -src_stride_yuy2;
    }
    // Coalesce rows.
    if (src_stride_yuy2 == width * 2 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_yuy2 = dst_stride_argb = 0;
    }
#if defined(HAS_YUY2TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        YUY2ToARGBRow = YUY2ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 16)) {
            YUY2ToARGBRow = YUY2ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_YUY2TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        YUY2ToARGBRow = YUY2ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 32)) {
            YUY2ToARGBRow = YUY2ToARGBRow_AVX2;
        }
    }
#endif
    for (y = 0; y < height; ++y) {
        YUY2ToARGBRow(src_yuy2, dst_argb, width);
        src_yuy2 += src_stride_yuy2;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

// Convert UYVY to ARGB.
int UYVYToARGB(const uint8* src_uyvy, int src_stride_uyvy, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*UYVYToARGBRow)(const uint8* src_uyvy, uint8* dst_argb, int pix) =
        UYVYToARGBRow_C;
    if (!src_uyvy || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_uyvy = src_uyvy + (height - 1) * src_stride_uyvy;
        src_stride_uyvy = -src_stride_uyvy;
    }
    // Coalesce rows.
    if (src_stride_uyvy == width * 2 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_uyvy = dst_stride_argb = 0;
    }
#if defined(HAS_UYVYTOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        UYVYToARGBRow = UYVYToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 16)) {
            UYVYToARGBRow = UYVYToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_UYVYTOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        UYVYToARGBRow = UYVYToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 32)) {
            UYVYToARGBRow = UYVYToARGBRow_AVX2;
        }
    }
#endif
    for (y = 0; y < height; ++y) {
        UYVYToARGBRow(src_uyvy, dst_argb, width);
        src_uyvy += src_stride_uyvy;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

// Convert J420 to ARGB.
int J420ToARGB(const uint8* src_y, int src_stride_y,
const uint8* src_u, int src_stride_u,
const uint8* src_v, int src_stride_v,
uint8* dst_argb, int dst_stride_argb,
int width, int height) {
    int y;
    void(*J422ToARGBRow)(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) = J422ToARGBRow_C;
    if (!src_y || !src_u || !src_v || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
#if defined(HAS_J422TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        J422ToARGBRow = J422ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            J422ToARGBRow = J422ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_J422TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        J422ToARGBRow = J422ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            J422ToARGBRow = J422ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        J422ToARGBRow(src_y, src_u, src_v, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        if (y & 1) {
            src_u += src_stride_u;
            src_v += src_stride_v;
        }
    }
    return 0;
}

// Convert J422 to ARGB.
int J422ToARGB(const uint8* src_y, int src_stride_y,
const uint8* src_u, int src_stride_u,
const uint8* src_v, int src_stride_v,
uint8* dst_argb, int dst_stride_argb,
int width, int height) {
    int y;
    void(*J422ToARGBRow)(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) = J422ToARGBRow_C;
    if (!src_y || !src_u || !src_v ||
        !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
    // Coalesce rows.
    if (src_stride_y == width &&
        src_stride_u * 2 == width &&
        src_stride_v * 2 == width &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_y = src_stride_u = src_stride_v = dst_stride_argb = 0;
    }
#if defined(HAS_J422TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        J422ToARGBRow = J422ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            J422ToARGBRow = J422ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_J422TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        J422ToARGBRow = J422ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            J422ToARGBRow = J422ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        J422ToARGBRow(src_y, src_u, src_v, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        src_u += src_stride_u;
        src_v += src_stride_v;
    }
    return 0;
}



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


int RAWToARGB(const uint8* src_raw, int src_stride_raw,
              uint8* dst_argb, int dst_stride_argb,
              int width, int height) {
    int y;
    void(*RAWToARGBRow)(const uint8* src_rgb, uint8* dst_argb, int pix) =
        RAWToARGBRow_C;
    if (!src_raw || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_raw = src_raw + (height - 1) * src_stride_raw;
        src_stride_raw = -src_stride_raw;
    }
    // Coalesce rows.
    if (src_stride_raw == width * 3 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_raw = dst_stride_argb = 0;
    }
#if defined(HAS_RAWTOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        RAWToARGBRow = RAWToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 16)) {
            RAWToARGBRow = RAWToARGBRow_SSSE3;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        RAWToARGBRow(src_raw, dst_argb, width);
        src_raw += src_stride_raw;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

// Convert RGB565 to ARGB.
static int RGB565ToARGB(const uint8* src_rgb565, int src_stride_rgb565, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*RGB565ToARGBRow)(const uint8* src_rgb565, uint8* dst_argb, int pix) =
        RGB565ToARGBRow_C;
    if (!src_rgb565 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_rgb565 = src_rgb565 + (height - 1) * src_stride_rgb565;
        src_stride_rgb565 = -src_stride_rgb565;
    }
    // Coalesce rows.
    if (src_stride_rgb565 == width * 2 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_rgb565 = dst_stride_argb = 0;
    }
#if defined(HAS_RGB565TOARGBROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        RGB565ToARGBRow = RGB565ToARGBRow_Any_SSE2;
        if (IS_ALIGNED(width, 8)) {
            RGB565ToARGBRow = RGB565ToARGBRow_SSE2;
        }
    }
#endif
#if defined(HAS_RGB565TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        RGB565ToARGBRow = RGB565ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            RGB565ToARGBRow = RGB565ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        RGB565ToARGBRow(src_rgb565, dst_argb, width);
        src_rgb565 += src_stride_rgb565;
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

// Convert BGRA to ARGB.
static int BGRAToARGB(const uint8* src_bgra, int src_stride_bgra, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_bgra, src_stride_bgra,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskBGRAToARGB),
                       width, height);
}

/*
// Convert ARGB to BGRA (same as BGRAToARGB).
static int ARGBToBGRA(const uint8* src_bgra, int src_stride_bgra, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_bgra, src_stride_bgra,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskBGRAToARGB),
                       width, height);
}
*/

// Convert ABGR to ARGB.
static int ABGRToARGB(const uint8* src_abgr, int src_stride_abgr, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_abgr, src_stride_abgr,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskABGRToARGB),
                       width, height);
}

/*
// Convert ARGB to ABGR to (same as ABGRToARGB).
static int ARGBToABGR(const uint8* src_abgr, int src_stride_abgr, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_abgr, src_stride_abgr,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskABGRToARGB),
                       width, height);
}
*/


// Convert RGBA to ARGB.
static int RGBAToARGB(const uint8* src_rgba, int src_stride_rgba, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    return ARGBShuffle(src_rgba, src_stride_rgba,
                       dst_argb, dst_stride_argb,
                       (const uint8*)(&kShuffleMaskRGBAToARGB),
                       width, height);
}

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


// Convert ARGB4444 to ARGB.
static int ARGB4444ToARGB(const uint8* src_argb4444, int src_stride_argb4444, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*ARGB4444ToARGBRow)(const uint8* src_argb4444, uint8* dst_argb,
                             int pix) = ARGB4444ToARGBRow_C;
    if (!src_argb4444 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        src_argb4444 = src_argb4444 + (height - 1) * src_stride_argb4444;
        src_stride_argb4444 = -src_stride_argb4444;
    }
    // Coalesce rows.
    if (src_stride_argb4444 == width * 2 &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_argb4444 = dst_stride_argb = 0;
    }
#if defined(HAS_ARGB4444TOARGBROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        ARGB4444ToARGBRow = ARGB4444ToARGBRow_Any_SSE2;
        if (IS_ALIGNED(width, 8)) {
            ARGB4444ToARGBRow = ARGB4444ToARGBRow_SSE2;
        }
    }
#endif
#if defined(HAS_ARGB4444TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        ARGB4444ToARGBRow = ARGB4444ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            ARGB4444ToARGBRow = ARGB4444ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        ARGB4444ToARGBRow(src_argb4444, dst_argb, width);
        src_argb4444 += src_stride_argb4444;
        dst_argb += dst_stride_argb;
    }
    return 0;
}

// Convert I400 to ARGB.
int I400ToARGB(const uint8* src_y, int src_stride_y, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*I400ToARGBRow)(const uint8* y_buf,
                         uint8* rgb_buf,
                         int width) = I400ToARGBRow_C;
    if (!src_y || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
    // Coalesce rows.
    if (src_stride_y == width &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_y = dst_stride_argb = 0;
    }
#if defined(HAS_I400TOARGBROW_SSE2)
    if (TestCpuFlag(kCpuHasSSE2)) {
        I400ToARGBRow = I400ToARGBRow_Any_SSE2;
        if (IS_ALIGNED(width, 8)) {
            I400ToARGBRow = I400ToARGBRow_SSE2;
        }
    }
#endif
#if defined(HAS_I400TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        I400ToARGBRow = I400ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            I400ToARGBRow = I400ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        I400ToARGBRow(src_y, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
    }
    return 0;
}

// Convert NV12 to ARGB.
int NV12ToARGB(const uint8* src_y, int src_stride_y, const uint8* src_uv, int src_stride_uv, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*NV12ToARGBRow)(const uint8* y_buf,
                         const uint8* uv_buf,
                         uint8* rgb_buf,
                         int width) = NV12ToARGBRow_C;
    if (!src_y || !src_uv || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
#if defined(HAS_NV12TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        NV12ToARGBRow = NV12ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            NV12ToARGBRow = NV12ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_NV12TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        NV12ToARGBRow = NV12ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            NV12ToARGBRow = NV12ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        NV12ToARGBRow(src_y, src_uv, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        if (y & 1) {
            src_uv += src_stride_uv;
        }
    }
    return 0;
}

// Convert NV21 to ARGB.
static int NV21ToARGB(const uint8* src_y, int src_stride_y, const uint8* src_uv, int src_stride_uv, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*NV21ToARGBRow)(const uint8* y_buf,
                         const uint8* uv_buf,
                         uint8* rgb_buf,
                         int width) = NV21ToARGBRow_C;
    if (!src_y || !src_uv || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
#if defined(HAS_NV21TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        NV21ToARGBRow = NV21ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            NV21ToARGBRow = NV21ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_NV21TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        NV21ToARGBRow = NV21ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            NV21ToARGBRow = NV21ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        NV21ToARGBRow(src_y, src_uv, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        if (y & 1) {
            src_uv += src_stride_uv;
        }
    }
    return 0;
}

// Convert M420 to ARGB.
static int M420ToARGB(const uint8* src_m420, int src_stride_m420, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*NV12ToARGBRow)(const uint8* y_buf,
                         const uint8* uv_buf,
                         uint8* rgb_buf,
                         int width) = NV12ToARGBRow_C;
    if (!src_m420 || !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
#if defined(HAS_NV12TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        NV12ToARGBRow = NV12ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            NV12ToARGBRow = NV12ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_NV12TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        NV12ToARGBRow = NV12ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            NV12ToARGBRow = NV12ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height - 1; y += 2) {
        NV12ToARGBRow(src_m420, src_m420 + src_stride_m420 * 2, dst_argb, width);
        NV12ToARGBRow(src_m420 + src_stride_m420, src_m420 + src_stride_m420 * 2,
                      dst_argb + dst_stride_argb, width);
        dst_argb += dst_stride_argb * 2;
        src_m420 += src_stride_m420 * 3;
    }
    if (height & 1) {
        NV12ToARGBRow(src_m420, src_m420 + src_stride_m420 * 2, dst_argb, width);
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


    auto loopf = [&](ROWFUNC rowfunc)
    {
        for (int y = 0; y < height; ++y)
        {
            rowfunc(src_y, src_u, src_v, dst_argb, width);
            dst_argb += dst_stride_argb;
            src_y += src_stride_y;
            if (y & 1) {
                src_u += src_stride_u;
                src_v += src_stride_v;
            }
        }
    };

    if (CCAPS(CPU_AVX2))
    {
        if (IS_ALIGNED(width, 16))
            loopf( I422ToARGBRow_AVX2 );
        else
            loopf( I422ToARGBRow_Any_AVX2 );
        return;
    }

    if (CCAPS(CPU_SSSE3))
    {
        if (IS_ALIGNED(width, 16))
            loopf(I422ToARGBRow_SSSE3);
        else
            loopf(I422ToARGBRow_Any_SSSE3);
        return;
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


// Convert I422 to ARGB.
static int I422ToARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*I422ToARGBRow)(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) = I422ToARGBRow_C;
    if (!src_y || !src_u || !src_v ||
        !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
    // Coalesce rows.
    if (src_stride_y == width &&
        src_stride_u * 2 == width &&
        src_stride_v * 2 == width &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_y = src_stride_u = src_stride_v = dst_stride_argb = 0;
    }
#if defined(HAS_I422TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        I422ToARGBRow = I422ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            I422ToARGBRow = I422ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_I422TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        I422ToARGBRow = I422ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            I422ToARGBRow = I422ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        I422ToARGBRow(src_y, src_u, src_v, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        src_u += src_stride_u;
        src_v += src_stride_v;
    }
    return 0;
}

// Convert I411 to ARGB.
static int I411ToARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height) {
    int y;
    void(*I411ToARGBRow)(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) = I411ToARGBRow_C;
    if (!src_y || !src_u || !src_v ||
        !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
    // Coalesce rows.
    if (src_stride_y == width &&
        src_stride_u * 4 == width &&
        src_stride_v * 4 == width &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_y = src_stride_u = src_stride_v = dst_stride_argb = 0;
    }
#if defined(HAS_I411TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        I411ToARGBRow = I411ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            I411ToARGBRow = I411ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_I411TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        I411ToARGBRow = I411ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            I411ToARGBRow = I411ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        I411ToARGBRow(src_y, src_u, src_v, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        src_u += src_stride_u;
        src_v += src_stride_v;
    }
    return 0;
}

// Convert I444 to ARGB.
int I444ToARGB(const uint8* src_y, int src_stride_y, const uint8* src_u, int src_stride_u, const uint8* src_v, int src_stride_v, uint8* dst_argb, int dst_stride_argb, int width, int height)
{
    int y;
    void(*I444ToARGBRow)(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgb_buf,
                         int width) = I444ToARGBRow_C;
    if (!src_y || !src_u || !src_v ||
        !dst_argb ||
        width <= 0 || height == 0) {
        return -1;
    }
    // Negative height means invert the image.
    if (height < 0) {
        height = -height;
        dst_argb = dst_argb + (height - 1) * dst_stride_argb;
        dst_stride_argb = -dst_stride_argb;
    }
    // Coalesce rows.
    if (src_stride_y == width &&
        src_stride_u == width &&
        src_stride_v == width &&
        dst_stride_argb == width * 4) {
        width *= height;
        height = 1;
        src_stride_y = src_stride_u = src_stride_v = dst_stride_argb = 0;
    }
#if defined(HAS_I444TOARGBROW_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3)) {
        I444ToARGBRow = I444ToARGBRow_Any_SSSE3;
        if (IS_ALIGNED(width, 8)) {
            I444ToARGBRow = I444ToARGBRow_SSSE3;
        }
    }
#endif
#if defined(HAS_I444TOARGBROW_AVX2)
    if (TestCpuFlag(kCpuHasAVX2)) {
        I444ToARGBRow = I444ToARGBRow_Any_AVX2;
        if (IS_ALIGNED(width, 16)) {
            I444ToARGBRow = I444ToARGBRow_AVX2;
        }
    }
#endif

    for (y = 0; y < height; ++y) {
        I444ToARGBRow(src_y, src_u, src_v, dst_argb, width);
        dst_argb += dst_stride_argb;
        src_y += src_stride_y;
        src_u += src_stride_u;
        src_v += src_stride_v;
    }
    return 0;
}








// Convert camera sample to ARGB with cropping, rotation and vertical flip.
// "src_size" is needed to parse MJPG.
// "dst_stride_argb" number of bytes in a row of the dst_argb plane.
//   Normally this would be the same as dst_width, with recommended alignment
//   to 16 bytes for better efficiency.
//   If rotation of 90 or 270 is used, stride is affected. The caller should
//   allocate the I420 buffer according to rotation.
// "dst_stride_u" number of bytes in a row of the dst_u plane.
//   Normally this would be the same as (dst_width + 1) / 2, with
//   recommended alignment to 16 bytes for better efficiency.
//   If rotation of 90 or 270 is used, stride is affected.
// "crop_x" and "crop_y" are starting position for cropping.
//   To center, crop_x = (src_width - dst_width) / 2
//              crop_y = (src_height - dst_height) / 2
// "src_width" / "src_height" is size of src_frame in pixels.
//   "src_height" can be negative indicating a vertically flipped image source.
// "crop_width" / "crop_height" is the size to crop the src to.
//    Must be less than or equal to src_width/src_height
//    Cropping parameters are pre-rotation.
// "rotation" can be 0, 90, 180 or 270.
// "format" is a fourcc. ie 'I420', 'YUY2'
// Returns 0 for successful; -1 for invalid parameter. Non-zero for failure.
int ConvertToARGB(const uint8* sample, size_t sample_size,
                  uint8* crop_argb, int argb_stride,
                  int crop_x, int crop_y,
                  int src_width, int src_height, int crop_width, int crop_height,
                  uint32 fourcc )
{
    uint32 format = CanonicalFourCC(fourcc);
    int aligned_src_width = (src_width + 1) & ~1;
    const uint8* src;
    const uint8* src_uv;
    int abs_src_height = (src_height < 0) ? -src_height : src_height;
    int inv_crop_height = (crop_height < 0) ? -crop_height : crop_height;
    int r = 0;

    // One pass rotation is available for some formats. For the rest, convert
    // to I420 (with optional vertical flipping) into a temporary I420 buffer,
    // and then rotate the I420 to the final destination buffer.
    // For in-place conversion, if destination crop_argb is same as source sample,
    // also enable temporary buffer.
    //bool need_buf = (rotation && format != FOURCC_ARGB) || crop_argb == sample;
    //uint8* tmp_argb = crop_argb;
    //int tmp_argb_stride = argb_stride;
    //uint8* rotate_buffer = NULL;
    //int abs_crop_height = (crop_height < 0) ? -crop_height : crop_height;

    if (crop_argb == NULL || sample == NULL ||
        src_width <= 0 || crop_width <= 0 ||
        src_height == 0 || crop_height == 0) {
        return -1;
    }
    if (src_height < 0) {
        inv_crop_height = -inv_crop_height;
    }

    switch (format) {
        // Single plane formats
        case FOURCC_YUY2:
            src = sample + (aligned_src_width * crop_y + crop_x) * 2;
            r = YUY2ToARGB(src, aligned_src_width * 2,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_UYVY:
            src = sample + (aligned_src_width * crop_y + crop_x) * 2;
            r = UYVYToARGB(src, aligned_src_width * 2,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_24BG:
            src = sample + (src_width * crop_y + crop_x) * 3;
            r = RGB24ToARGB(src, src_width * 3,
                            crop_argb, argb_stride,
                            crop_width, inv_crop_height);
            break;
        case FOURCC_RAW:
            src = sample + (src_width * crop_y + crop_x) * 3;
            r = RAWToARGB(src, src_width * 3,
                          crop_argb, argb_stride,
                          crop_width, inv_crop_height);
            break;
        case FOURCC_ARGB:
            src = sample + (src_width * crop_y + crop_x) * 4;
            r = ARGBToARGB(src, src_width * 4,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_BGRA:
            src = sample + (src_width * crop_y + crop_x) * 4;
            r = BGRAToARGB(src, src_width * 4,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_ABGR:
            src = sample + (src_width * crop_y + crop_x) * 4;
            r = ABGRToARGB(src, src_width * 4,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_RGBA:
            src = sample + (src_width * crop_y + crop_x) * 4;
            r = RGBAToARGB(src, src_width * 4,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_RGBP:
            src = sample + (src_width * crop_y + crop_x) * 2;
            r = RGB565ToARGB(src, src_width * 2,
                             crop_argb, argb_stride,
                             crop_width, inv_crop_height);
            break;
        case FOURCC_RGBO:
            src = sample + (src_width * crop_y + crop_x) * 2;
            r = ARGB1555ToARGB(src, src_width * 2,
                               crop_argb, argb_stride,
                               crop_width, inv_crop_height);
            break;
        case FOURCC_R444:
            src = sample + (src_width * crop_y + crop_x) * 2;
            r = ARGB4444ToARGB(src, src_width * 2,
                               crop_argb, argb_stride,
                               crop_width, inv_crop_height);
            break;
        case FOURCC_I400:
            src = sample + src_width * crop_y + crop_x;
            r = I400ToARGB(src, src_width,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;

            // Biplanar formats
        case FOURCC_NV12:
            src = sample + (src_width * crop_y + crop_x);
            src_uv = sample + aligned_src_width * (src_height + crop_y / 2) + crop_x;
            r = NV12ToARGB(src, src_width,
                           src_uv, aligned_src_width,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_NV21:
            src = sample + (src_width * crop_y + crop_x);
            src_uv = sample + aligned_src_width * (src_height + crop_y / 2) + crop_x;
            // Call NV12 but with u and v parameters swapped.
            r = NV21ToARGB(src, src_width,
                           src_uv, aligned_src_width,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        case FOURCC_M420:
            src = sample + (src_width * crop_y) * 12 / 8 + crop_x;
            r = M420ToARGB(src, src_width,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
            // Triplanar formats
        case FOURCC_I420:
        case FOURCC_YU12:
        case FOURCC_YV12: {
            const uint8* src_y = sample + (src_width * crop_y + crop_x);
            const uint8* src_u;
            const uint8* src_v;
            int halfwidth = (src_width + 1) / 2;
            int halfheight = (abs_src_height + 1) / 2;
            if (format == FOURCC_YV12) {
                src_v = sample + src_width * abs_src_height +
                    (halfwidth * crop_y + crop_x) / 2;
                src_u = sample + src_width * abs_src_height +
                    halfwidth * (halfheight + crop_y / 2) + crop_x / 2;
            }
            else {
                src_u = sample + src_width * abs_src_height +
                    (halfwidth * crop_y + crop_x) / 2;
                src_v = sample + src_width * abs_src_height +
                    halfwidth * (halfheight + crop_y / 2) + crop_x / 2;
            }
            r = 0;
                img_helper_i420_to_ARGB(src_y, src_width,
                           src_u, halfwidth,
                           src_v, halfwidth,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        }

        case FOURCC_J420: {
            const uint8* src_y = sample + (src_width * crop_y + crop_x);
            const uint8* src_u;
            const uint8* src_v;
            int halfwidth = (src_width + 1) / 2;
            int halfheight = (abs_src_height + 1) / 2;
            src_u = sample + src_width * abs_src_height +
                (halfwidth * crop_y + crop_x) / 2;
            src_v = sample + src_width * abs_src_height +
                halfwidth * (halfheight + crop_y / 2) + crop_x / 2;
            r = J420ToARGB(src_y, src_width,
                           src_u, halfwidth,
                           src_v, halfwidth,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        }

        case FOURCC_I422:
        case FOURCC_YV16: {
            const uint8* src_y = sample + src_width * crop_y + crop_x;
            const uint8* src_u;
            const uint8* src_v;
            int halfwidth = (src_width + 1) / 2;
            if (format == FOURCC_YV16) {
                src_v = sample + src_width * abs_src_height +
                    halfwidth * crop_y + crop_x / 2;
                src_u = sample + src_width * abs_src_height +
                    halfwidth * (abs_src_height + crop_y) + crop_x / 2;
            }
            else {
                src_u = sample + src_width * abs_src_height +
                    halfwidth * crop_y + crop_x / 2;
                src_v = sample + src_width * abs_src_height +
                    halfwidth * (abs_src_height + crop_y) + crop_x / 2;
            }
            r = I422ToARGB(src_y, src_width,
                           src_u, halfwidth,
                           src_v, halfwidth,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        }
        case FOURCC_I444:
        case FOURCC_YV24: {
            const uint8* src_y = sample + src_width * crop_y + crop_x;
            const uint8* src_u;
            const uint8* src_v;
            if (format == FOURCC_YV24) {
                src_v = sample + src_width * (abs_src_height + crop_y) + crop_x;
                src_u = sample + src_width * (abs_src_height * 2 + crop_y) + crop_x;
            }
            else {
                src_u = sample + src_width * (abs_src_height + crop_y) + crop_x;
                src_v = sample + src_width * (abs_src_height * 2 + crop_y) + crop_x;
            }
            r = I444ToARGB(src_y, src_width,
                           src_u, src_width,
                           src_v, src_width,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        }
        case FOURCC_I411: {
            int quarterwidth = (src_width + 3) / 4;
            const uint8* src_y = sample + src_width * crop_y + crop_x;
            const uint8* src_u = sample + src_width * abs_src_height +
                quarterwidth * crop_y + crop_x / 4;
            const uint8* src_v = sample + src_width * abs_src_height +
                quarterwidth * (abs_src_height + crop_y) + crop_x / 4;
            r = I411ToARGB(src_y, src_width,
                           src_u, quarterwidth,
                           src_v, quarterwidth,
                           crop_argb, argb_stride,
                           crop_width, inv_crop_height);
            break;
        }
#ifdef HAVE_JPEG
        case FOURCC_MJPG:
            r = MJPGToARGB(sample, sample_size,
                           crop_argb, argb_stride,
                           src_width, abs_src_height, crop_width, inv_crop_height);
            break;
#endif
        default:
            r = -1;  // unknown fourcc - return failure code.
    }

    return r;
}

} // namespace ts