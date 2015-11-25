#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <Mmsystem.h>
#include <stdio.h>

#define SLASSERT(c,...) (1,true)
#define SLERROR(c,...) __debugbreak()

#include "spinlock/spinlock.h"
#include "ipc.h"
