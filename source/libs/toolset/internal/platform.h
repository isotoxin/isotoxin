#pragma once

#ifdef _WIN32

#include "../_win32/win32_inc.inl"
#include <stdint.h>

#ifndef _FINAL
#include <DbgHelp.h>
#endif

#endif

#ifdef _NIX
#include "../_nix/nix_inc.inl"
#include "win32emu/win32emu.h"
#endif

