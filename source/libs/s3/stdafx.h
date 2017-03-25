#pragma once

#define _ALLOW_RTCc_IN_STL

#define NOMINMAX
#include <math.h>
#include <algorithm>
#define FLAC__NO_DLL
#include "../libflac/include/flac/stream_decoder.h"
#include "../libvorbis/include/vorbis/vorbisfile.h"

#ifdef _WIN32
#include <dsound.h> // need to be included before spinlock
#endif // _WIN32

#define SLASSERT(...) (0, true)
#define SLERROR(...) (0, true)
#include "spinlock/spinlock.h"

namespace s3
{
    typedef unsigned int		uint32;
}

#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) ( \
    (static_cast<uint32>(a)) | (static_cast<uint32>(b) << 8) | \
    (static_cast<uint32>(c) << 16) | (static_cast<uint32>(d) << 24))
#endif

//Для возможности глобальной перегрузки оператора new в использующих либу проектах
#include <new>
void* _cdecl operator new(size_t size);
void* _cdecl operator new(size_t size, const std::nothrow_t&);
void* _cdecl operator new[](size_t size);
void* _cdecl operator new[](size_t size, const std::nothrow_t&);
void _cdecl operator delete(void* p);
void _cdecl operator delete(void* p, const std::nothrow_t&);
void _cdecl operator delete[](void* p);
void _cdecl operator delete[](void* p, const std::nothrow_t&);
