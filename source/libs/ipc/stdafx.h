#pragma once

#ifdef __linux__
#undef _NIX
#define _NIX
#endif // __linux__

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <Mmsystem.h>
#include <stdio.h>
#define SLASSERT(c,...) (1,true)
#define SLERROR(c,...) __debugbreak()

typedef DWORD uint32_t;

#endif
#ifdef _NIX

#include <stddef.h>
#include <string.h> // memcpy

#define SLASSERT(c,...) (true)
#define SLERROR(c,...) __builtin_trap()

#undef _alloca
#define _alloca alloca
#endif // _NIX

#include "spinlock/spinlock.h"
#include "ipc.h"
