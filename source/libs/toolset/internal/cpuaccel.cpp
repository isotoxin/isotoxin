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

#include <wtypes.h>
#include <winnt.h>
#include "cpuaccel.h"

static long g_lCPUExtensionsEnabled;
static long g_lCPUExtensionsAvailable;

extern "C" {
	bool FPU_enabled, MMX_enabled, ISSE_enabled, SSE2_enabled;
};


#ifdef _M_AMD64

	long CPUCheckForExtensions() {
		long flags = CPUF_SUPPORTS_FPU;

		if (IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE))
			flags |= CPUF_SUPPORTS_MMX;

		if (IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE))
			flags |= CPUF_SUPPORTS_SSE | CPUF_SUPPORTS_INTEGER_SSE;

		if (IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
			flags |= CPUF_SUPPORTS_SSE2;

		if (IsProcessorFeaturePresent(PF_3DNOW_INSTRUCTIONS_AVAILABLE))
			flags |= CPUF_SUPPORTS_3DNOW;

		return flags;
	}

#else

	// This is ridiculous.

	static long CPUCheckForSSESupport() {
		__try {
	//		__asm andps xmm0,xmm0

			__asm _emit 0x0f
			__asm _emit 0x54
			__asm _emit 0xc0

		} __except(EXCEPTION_EXECUTE_HANDLER) {
			if (_exception_code() == STATUS_ILLEGAL_INSTRUCTION)
				g_lCPUExtensionsAvailable &= ~(CPUF_SUPPORTS_SSE|CPUF_SUPPORTS_SSE2);
		}

		return g_lCPUExtensionsAvailable;
	}

	long __declspec(naked) CPUCheckForExtensions() {
		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			xor		ebp,ebp			;cpu flags - if we don't have CPUID, we probably
									;won't want to try FPU optimizations.

			;check for CPUID.

			pushfd					;flags -> EAX
			pop		eax
			or		eax,00200000h	;set the ID bit
			push	eax				;EAX -> flags
			popfd
			pushfd					;flags -> EAX
			pop		eax
			and		eax,00200000h	;ID bit set?
			jz		done			;nope...

			;CPUID exists, check for features register.

			mov		ebp,00000003h
			xor		eax,eax
			cpuid
			or		eax,eax
			jz		done			;no features register?!?

			;features register exists, look for MMX, SSE, SSE2.

			mov		eax,1
			cpuid
			mov		ebx,edx
			and		ebx,00800000h	;MMX is bit 23
			shr		ebx,21
			or		ebp,ebx			;set bit 2 if MMX exists

			mov		ebx,edx
			and		edx,02000000h	;SSE is bit 25
			shr		edx,25
			neg		edx
			and		edx,00000018h	;set bits 3 and 4 if SSE exists
			or		ebp,edx

			and		ebx,04000000h	;SSE2 is bit 26
			shr		ebx,21
			and		ebx,00000020h	;set bit 5
			or		ebp,ebx

			;check for vendor feature register (K6/Athlon).

			mov		eax,80000000h
			cpuid
			mov		ecx,80000001h
			cmp		eax,ecx
			jb		done

			;vendor feature register exists, look for 3DNow! and Athlon extensions

			mov		eax,ecx
			cpuid

			mov		eax,edx
			and		edx,80000000h	;3DNow! is bit 31
			shr		edx,25
			or		ebp,edx			;set bit 6

			mov		edx,eax
			and		eax,40000000h	;3DNow!2 is bit 30
			shr		eax,23
			or		ebp,eax			;set bit 7

			and		edx,00400000h	;AMD MMX extensions (integer SSE) is bit 22
			shr		edx,19
			or		ebp,edx

	done:
			mov		eax,ebp
			mov		g_lCPUExtensionsAvailable, ebp

			;Full SSE and SSE-2 require OS support for the xmm* registers.

			test	eax,00000030h
			jz		nocheck
			call	CPUCheckForSSESupport
	nocheck:
			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
		}
	}

#endif

long CPUEnableExtensions(long lEnableFlags) {
	g_lCPUExtensionsEnabled = lEnableFlags;

	MMX_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_MMX);
	FPU_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_FPU);
	ISSE_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_INTEGER_SSE);
	SSE2_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_SSE2);

	return g_lCPUExtensionsEnabled;
}

long CPUGetAvailableExtensions() {
	return g_lCPUExtensionsAvailable;
}

long CPUGetEnabledExtensions() {
	return g_lCPUExtensionsEnabled;
}

void VDCPUCleanupExtensions() {
#ifndef _M_AMD64
	if (ISSE_enabled)
		__asm sfence
	if (MMX_enabled)
		__asm emms
#else
	_mm_sfence();
#endif
}