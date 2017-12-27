// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#define SLASSERT(c,...) (1,true)
#define SLERROR(c,...) __debugbreak()

#include <stdint.h>
#include "spinlock/queue.h"


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "curl.lib")
#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "opus.lib")
#pragma comment(lib, "libvpx.lib")

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

#pragma warning (disable:4559)
#pragma warning (disable:4127)
#pragma warning (disable:4057)
#pragma warning (disable:4702)

#define MALLOC_ALIGNMENT ((size_t)16U)
#define USE_DL_PREFIX
#define USE_LOCKS 0

static spinlock::long3264 dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

extern "C"
{
#include "dlmalloc/dlmalloc.c"
}

#ifdef _WIN32
// disable VS2015 telemetry (Th you, kidding? Fuck spies.)
extern "C"
{
    void _cdecl __vcrt_initialize_telemetry_provider() {}
    void _cdecl __telemetry_main_invoke_trigger() {}
    void _cdecl __telemetry_main_return_trigger() {}
    void _cdecl __vcrt_uninitialize_telemetry_provider() {}
};
#endif

