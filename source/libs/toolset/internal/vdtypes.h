//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2007 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_SYSTEM_VDTYPES_H
#define f_VD2_SYSTEM_VDTYPES_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <algorithm>
#include <stdio.h>
#include <stdarg.h>
#include <new>

#ifndef NULL
#define NULL 0
#endif

///////////////////////////////////////////////////////////////////////////
//
//	compiler detection
//
///////////////////////////////////////////////////////////////////////////

#ifndef VD_COMPILER_DETECTED
	#define VD_COMPILER_DETECTED

	#if defined(_MSC_VER)
		#define VD_COMPILER_MSVC	_MSC_VER

		#if _MSC_VER >= 1400
			#define VD_COMPILER_MSVC_VC8		1
			#define VD_COMPILER_MSVC_VC8_OR_LATER 1

			#if _MSC_FULL_VER == 140040310
				#define VD_COMPILER_MSVC_VC8_PSDK 1
			#elif _MSC_FULL_VER == 14002207
				#define VD_COMPILER_MSVC_VC8_DDK 1
			#endif

		#elif _MSC_VER >= 1310
			#define VD_COMPILER_MSVC_VC71	1
		#elif _MSC_VER >= 1300
			#define VD_COMPILER_MSVC_VC7		1
		#elif _MSC_VER >= 1200
			#define VD_COMPILER_MSVC_VC6		1
		#endif
	#elif defined(__GNUC__)
		#define VD_COMPILER_GCC
		#if defined(__MINGW32__) || defined(__MINGW64__)
			#define VD_COMPILER_GCC_MINGW
		#endif
	#endif
#endif

#ifndef VD_CPU_DETECTED
	#define VD_CPU_DETECTED

	#if defined(_M_AMD64)
		#define VD_CPU_AMD64	1
	#elif defined(_M_IX86) || defined(__i386__)
		#define VD_CPU_X86		1
	#elif defined(_M_ARM)
		#define VD_CPU_ARM
	#endif
#endif

///////////////////////////////////////////////////////////////////////////
//
//	types
//
///////////////////////////////////////////////////////////////////////////

#ifndef VD_STANDARD_TYPES_DECLARED
	#if defined(_MSC_VER)
		typedef signed __int64		sint64;
		typedef unsigned __int64	uint64;
	#elif defined(__GNUC__)
		typedef signed long long	sint64;
		typedef unsigned long long	uint64;
	#endif
	typedef signed int			sint32;
	typedef unsigned int		uint32;
	typedef signed short		sint16;
	typedef unsigned short		uint16;
	typedef signed char			sint8;
	typedef unsigned char		uint8;

	typedef sint64				int64;
	typedef sint32				int32;
	typedef sint16				int16;
	typedef sint8				int8;

	#ifdef _M_AMD64
		typedef sint64 sintptr;
		typedef uint64 uintptr;
	#else
		#if _MSC_VER >= 1310
			typedef __w64 sint32 sintptr;
			typedef __w64 uint32 uintptr;
		#else
			typedef sint32 sintptr;
			typedef uint32 uintptr;
		#endif
	#endif
#endif

#if defined(_MSC_VER)
	#define VD64(x) x##i64
#elif defined(__GNUC__)
	#define VD64(x) x##ll
#else
	#error Please add an entry for your compiler for 64-bit constant literals.
#endif

	
#define VDAPIENTRY			__cdecl

#endif

