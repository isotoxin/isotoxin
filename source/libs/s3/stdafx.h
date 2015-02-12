#pragma once

#define NOMINMAX
#include <dsound.h>
#include <math.h>
#include <algorithm>
#define FLAC__NO_DLL
#include "../flac/include/flac/stream_decoder.h"
#include "../libvorbis/include/vorbis/vorbisfile.h"

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
