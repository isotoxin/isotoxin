#pragma once

#define _ALLOW_RTCc_IN_STL
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "../plgcommon/plgcommon.h"

#ifdef _WIN32
#include <windows.h>
#include <Mmsystem.h>
#include <Windns.h>
#endif

#include "../plgcommon/proto_interface.h"
