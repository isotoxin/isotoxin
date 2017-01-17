#pragma once

#define _ALLOW_RTCc_IN_STL
#include <zstrings/z_str_fake_std.h> // 1st include fake std::string


#define WIN32_LEAN_AND_MEAN
#define __STREAMS__
#include <windows.h>
#include <stdlib.h>


#include <dshow.h>
#include <ks.h>
#include <ksmedia.h>
#include <ksproxy.h>
#include <Amaudio.h>
#include <Dvdmedia.h>

#include <bdaiface.h>
#include <wmcodecdsp.h>
#include <mmreg.h>

#pragma warning (push)
#pragma warning (disable: 4995) //: 'gets': name was marked as #pragma deprecated
//-V::730

#include <deque>

#pragma warning (pop)

#define SLASSERT(c,...) (1,true)
#define SLERROR(c,...) __debugbreak()
#include "spinlock/spinlock.h"

#include "../dshowcapture.hpp"

#include "ComPtr.hpp"
#include "CoTaskMemPtr.hpp"

#include "dshow-base.hpp"
#include "dshow-media-type.hpp"
#include "dshow-enum.hpp"
#include "dshow-formats.hpp"

#include "output-filter.hpp"
#include "capture-filter.hpp"
#include "device.hpp"
#include "dshow-device-defs.hpp"
#include "dshow-device-defs.hpp"
#include "dshow-demux.hpp"
#include "avermedia-encode.h"
#include "encoder.hpp"

#include "log.hpp"


// allow global new replace
#include <new>
void* _cdecl operator new(size_t size);
void* _cdecl operator new(size_t size, const std::nothrow_t&);
void* _cdecl operator new[](size_t size);
void* _cdecl operator new[](size_t size, const std::nothrow_t&);
void _cdecl operator delete(void* p);
void _cdecl operator delete(void* p, const std::nothrow_t&);
void _cdecl operator delete[](void* p);
void _cdecl operator delete[](void* p, const std::nothrow_t&);
