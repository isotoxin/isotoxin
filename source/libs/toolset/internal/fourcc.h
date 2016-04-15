#pragma once

#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) ( \
    (static_cast<uint32>(a)) | (static_cast<uint32>(b) << 8) | \
    (static_cast<uint32>(c) << 16) | (static_cast<uint32>(d) << 24))
#endif

#ifndef MAKEWORD
#define MAKEWORD(a, b)      ((uint16)(((uint8)(((uint32)(a)) & 0xff)) | ((uint16)((uint8)(((uint32)(b)) & 0xff))) << 8))
#endif

namespace ts
{

// FourCC codes grouped according to implementation efficiency.
// Primary formats should convert in 1 efficient step.
// Secondary formats are converted in 2 steps.
// Auxilliary formats call primary converters.
enum FourCC {
    // 9 Primary YUV formats: 5 planar, 2 biplanar, 2 packed.
    FOURCC_I420 = MAKEFOURCC('I', '4', '2', '0'),
    FOURCC_I422 = MAKEFOURCC('I', '4', '2', '2'),
    FOURCC_I444 = MAKEFOURCC('I', '4', '4', '4'),
    FOURCC_I411 = MAKEFOURCC('I', '4', '1', '1'),
    FOURCC_I400 = MAKEFOURCC('I', '4', '0', '0'),
    FOURCC_NV21 = MAKEFOURCC('N', 'V', '2', '1'),
    FOURCC_NV12 = MAKEFOURCC('N', 'V', '1', '2'),
    FOURCC_YUY2 = MAKEFOURCC('Y', 'U', 'Y', '2'),
    FOURCC_UYVY = MAKEFOURCC('U', 'Y', 'V', 'Y'),

    // 2 Secondary YUV formats: row biplanar.
    FOURCC_M420 = MAKEFOURCC('M', '4', '2', '0'),
    FOURCC_Q420 = MAKEFOURCC('Q', '4', '2', '0'), // deprecated.

    // 9 Primary RGB formats: 4 32 bpp, 2 24 bpp, 3 16 bpp.
    FOURCC_ARGB = MAKEFOURCC('A', 'R', 'G', 'B'),
    FOURCC_BGRA = MAKEFOURCC('B', 'G', 'R', 'A'),
    FOURCC_ABGR = MAKEFOURCC('A', 'B', 'G', 'R'),
    FOURCC_24BG = MAKEFOURCC('2', '4', 'B', 'G'),
    FOURCC_RAW  = MAKEFOURCC('r', 'a', 'w', ' '),
    FOURCC_RGBA = MAKEFOURCC('R', 'G', 'B', 'A'),
    FOURCC_RGBP = MAKEFOURCC('R', 'G', 'B', 'P'),  // rgb565 LE.
    FOURCC_RGBO = MAKEFOURCC('R', 'G', 'B', 'O'),  // argb1555 LE.
    FOURCC_R444 = MAKEFOURCC('R', '4', '4', '4'),  // argb4444 LE.

    // 4 Secondary RGB formats: 4 Bayer Patterns. deprecated.
    FOURCC_RGGB = MAKEFOURCC('R', 'G', 'G', 'B'),
    FOURCC_BGGR = MAKEFOURCC('B', 'G', 'G', 'R'),
    FOURCC_GRBG = MAKEFOURCC('G', 'R', 'B', 'G'),
    FOURCC_GBRG = MAKEFOURCC('G', 'B', 'R', 'G'),

    // 1 Primary Compressed YUV format.
    FOURCC_MJPG = MAKEFOURCC('M', 'J', 'P', 'G'),

    // 5 Auxiliary YUV variations: 3 with U and V planes are swapped, 1 Alias.
    FOURCC_YV12 = MAKEFOURCC('Y', 'V', '1', '2'),
    FOURCC_YV16 = MAKEFOURCC('Y', 'V', '1', '6'),
    FOURCC_YV24 = MAKEFOURCC('Y', 'V', '2', '4'),
    FOURCC_YU12 = MAKEFOURCC('Y', 'U', '1', '2'),  // Linux version of I420.
    FOURCC_J420 = MAKEFOURCC('J', '4', '2', '0'),
    FOURCC_J400 = MAKEFOURCC('J', '4', '0', '0'),

    // 14 Auxiliary aliases.  CanonicalFourCC() maps these to canonical fourcc.
    FOURCC_IYUV = MAKEFOURCC('I', 'Y', 'U', 'V'),  // Alias for I420.
    FOURCC_YU16 = MAKEFOURCC('Y', 'U', '1', '6'),  // Alias for I422.
    FOURCC_YU24 = MAKEFOURCC('Y', 'U', '2', '4'),  // Alias for I444.
    FOURCC_YUYV = MAKEFOURCC('Y', 'U', 'Y', 'V'),  // Alias for YUY2.
    FOURCC_YUVS = MAKEFOURCC('y', 'u', 'v', 's'),  // Alias for YUY2 on Mac.
    FOURCC_HDYC = MAKEFOURCC('H', 'D', 'Y', 'C'),  // Alias for UYVY.
    FOURCC_2VUY = MAKEFOURCC('2', 'v', 'u', 'y'),  // Alias for UYVY on Mac.
    FOURCC_JPEG = MAKEFOURCC('J', 'P', 'E', 'G'),  // Alias for MJPG.
    FOURCC_DMB1 = MAKEFOURCC('d', 'm', 'b', '1'),  // Alias for MJPG on Mac.
    FOURCC_BA81 = MAKEFOURCC('B', 'A', '8', '1'),  // Alias for BGGR.
    FOURCC_RGB3 = MAKEFOURCC('R', 'G', 'B', '3'),  // Alias for RAW.
    FOURCC_BGR3 = MAKEFOURCC('B', 'G', 'R', '3'),  // Alias for 24BG.
    FOURCC_CM32 = MAKEFOURCC(0, 0, 0, 32),  // Alias for BGRA kCMPixelFormat_32ARGB
    FOURCC_CM24 = MAKEFOURCC(0, 0, 0, 24),  // Alias for RAW kCMPixelFormat_24RGB
    FOURCC_L555 = MAKEFOURCC('L', '5', '5', '5'),  // Alias for RGBO.
    FOURCC_L565 = MAKEFOURCC('L', '5', '6', '5'),  // Alias for RGBP.
    FOURCC_5551 = MAKEFOURCC('5', '5', '5', '1'),  // Alias for RGBO.

    // 1 Auxiliary compressed YUV format set aside for capturer.
    FOURCC_H264 = MAKEFOURCC('H', '2', '6', '4'),

    // Match any fourcc.
    FOURCC_ANY = -1,
};

enum FourCCBpp {
    // Canonical fourcc codes used in our code.
    FOURCC_BPP_I420 = 12,
    FOURCC_BPP_I422 = 16,
    FOURCC_BPP_I444 = 24,
    FOURCC_BPP_I411 = 12,
    FOURCC_BPP_I400 = 8,
    FOURCC_BPP_NV21 = 12,
    FOURCC_BPP_NV12 = 12,
    FOURCC_BPP_YUY2 = 16,
    FOURCC_BPP_UYVY = 16,
    FOURCC_BPP_M420 = 12,
    FOURCC_BPP_Q420 = 12,
    FOURCC_BPP_ARGB = 32,
    FOURCC_BPP_BGRA = 32,
    FOURCC_BPP_ABGR = 32,
    FOURCC_BPP_RGBA = 32,
    FOURCC_BPP_24BG = 24,
    FOURCC_BPP_RAW = 24,
    FOURCC_BPP_RGBP = 16,
    FOURCC_BPP_RGBO = 16,
    FOURCC_BPP_R444 = 16,
    FOURCC_BPP_RGGB = 8,
    FOURCC_BPP_BGGR = 8,
    FOURCC_BPP_GRBG = 8,
    FOURCC_BPP_GBRG = 8,
    FOURCC_BPP_YV12 = 12,
    FOURCC_BPP_YV16 = 16,
    FOURCC_BPP_YV24 = 24,
    FOURCC_BPP_YU12 = 12,
    FOURCC_BPP_J420 = 12,
    FOURCC_BPP_J400 = 8,
    FOURCC_BPP_MJPG = 0,  // 0 means unknown.
    FOURCC_BPP_H264 = 0,
    FOURCC_BPP_IYUV = 12,
    FOURCC_BPP_YU16 = 16,
    FOURCC_BPP_YU24 = 24,
    FOURCC_BPP_YUYV = 16,
    FOURCC_BPP_YUVS = 16,
    FOURCC_BPP_HDYC = 16,
    FOURCC_BPP_2VUY = 16,
    FOURCC_BPP_JPEG = 1,
    FOURCC_BPP_DMB1 = 1,
    FOURCC_BPP_BA81 = 8,
    FOURCC_BPP_RGB3 = 24,
    FOURCC_BPP_BGR3 = 24,
    FOURCC_BPP_CM32 = 32,
    FOURCC_BPP_CM24 = 24,

    // Match any fourcc.
    FOURCC_BPP_ANY = 0,  // 0 means unknown.
};

} // namespace ts
