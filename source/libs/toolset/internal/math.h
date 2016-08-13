//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
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

#ifndef f_VD2_SYSTEM_MATH_H
#define f_VD2_SYSTEM_MATH_H

#include <math.h>
#include "vdtypes.h"

// Rounding functions
//
// Round a double to an int or a long.  Behavior is not specified at
// int(y)+0.5, if x is NaN or Inf, or if x is out of range.

int VDRoundToInt(double x);
long VDRoundToLong(double x);
int64 VDRoundToInt64(double x);

inline sint32 VDRoundToIntFast(float x) {
	union {
		float f;
		sint32 i;
	} u = {x + 12582912.0f};		// 2^22+2^23

	return (sint32)u.i - 0x4B400000;
}

inline sint32 VDRoundToIntFastFullRange(double x) {
	union {
		double f;
		sint32 i[2];
	} u = {x + 6755399441055744.0f};		// 2^51+2^52

	return (sint32)u.i[0];
}

#if defined(MODE64) || !defined(_MSC_VER)
	inline sint32 VDFloorToInt(double x) {
		return (sint32)floor(x);
	}
#else
	#pragma warning(push)
	#pragma warning(disable: 4035)		// warning C4035: 'VDFloorToInt' : no return value
	inline sint32 VDFloorToInt(double x) {
		sint32 temp;

		__asm {
			fld x
			fist temp
			fild temp
			mov eax, temp
			fsub
			fstp temp
			cmp	temp, 80000001h
			adc eax, -1
		}
	}
	#pragma warning(pop)
#endif

#if defined(MODE64) || !defined(_MSC_VER)
	inline sint32 VDCeilToInt(double x) {
		return (sint32)ceil(x);
	}
#else
	#pragma warning(push)
	#pragma warning(disable: 4035)		// warning C4035: 'VDCeilToInt' : no return value
	inline sint32 VDCeilToInt(double x) {
		sint32 temp;

		__asm {
			fld x
			fist temp
			fild temp
			mov eax, temp
			fsubr
			fstp temp
			cmp	temp, 80000001h
			sbb eax, -1
		}
	}
	#pragma warning(pop)
#endif

#endif
