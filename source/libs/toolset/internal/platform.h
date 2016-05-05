#pragma once

#ifdef _WIN32

#include "../_win32/win32_inc.inl"

#ifndef _FINAL
#include <DbgHelp.h>
#endif

#endif

#ifdef __linux__
#include "../_nix/nix_inc.inl"
#endif