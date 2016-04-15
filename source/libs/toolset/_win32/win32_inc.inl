#pragma once

#undef STRICT
#define STRICT
#define _NTOS_
#pragma push_macro( "ERROR" )

#undef ERROR
#include <WinSock2.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <mmsystem.h>
#include <ShlDisp.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>

#pragma pop_macro( "ERROR" )
