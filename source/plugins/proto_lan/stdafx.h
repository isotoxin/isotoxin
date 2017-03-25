#pragma once

#define _ALLOW_RTCc_IN_STL
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include "../plgcommon/plgcommon.h"

#ifdef _WIN32
#include <winsock2.h>
#include <Mmsystem.h>
#endif

#ifdef _MSC_VER
#pragma  warning(disable:4505)
#endif

#pragma warning(disable : 4324)
#include "sodium.h"

#include "packetgen.h"
#include "engine.h"

