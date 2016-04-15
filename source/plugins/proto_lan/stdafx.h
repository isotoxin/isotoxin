#pragma once

#define _ALLOW_RTCc_IN_STL
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <winsock2.h>
#include <Mmsystem.h>
#include "../plgcommon/plgcommon.h"

#pragma warning(disable : 4324)
#include "sodium.h"

#include "packetgen.h"
#include "engine.h"

