//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "toolset.h"

namespace ts
{

#define USE_ASM


extern "C" void _cdecl asm_sharpen_run(
		void *dst,
		const void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		long a_mult,
		long b_mult);

extern "C" void _cdecl asm_sharpen_run_MMX(
		void *dst,
		const void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride,
		long a_mult,
		long b_mult);

#define C_TOPOK		(1)
#define C_BOTTOMOK	(2)
#define C_LEFTOK	(4)
#define C_RIGHTOK	(8)

void inline conv_add(long& rt, long& gt, long& bt, unsigned long dv, long m) {
	bt += m*(0xFF & (dv));
	gt += m*(0xFF & (dv>>8));
	rt += m*(0xFF & (dv>>16));
}

void inline conv_add2(long& rt, long& gt, long& bt, unsigned long dv) {
	bt += 0xFF & (dv);
	gt += 0xFF & (dv>>8);
	rt += 0xFF & (dv>>16);
}

static unsigned long __fastcall do_conv(const unsigned long *data, long *m, long sflags, long pit) {
	long rt=0, gt=0, bt=0;

	if (sflags & C_TOPOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[        -1]);
		else							conv_add2(rt, gt, bt, data[         0]);
										conv_add2(rt, gt, bt, data[         0]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[        +1]);
		else							conv_add2(rt, gt, bt, data[         0]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}
	if (sflags & (C_LEFTOK))			conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & (C_RIGHTOK))			conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	else								conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	if (sflags & C_BOTTOMOK) {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>1)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
										conv_add2(rt, gt, bt, data[(pit>>1)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>1)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	} else {
		if (sflags & (C_LEFTOK))		conv_add2(rt, gt, bt, data[(pit>>2)-1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
										conv_add2(rt, gt, bt, data[(pit>>2)  ]);
		if (sflags & (C_RIGHTOK))		conv_add2(rt, gt, bt, data[(pit>>2)+1]);
		else							conv_add2(rt, gt, bt, data[(pit>>2)  ]);
	}

	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ],m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}

#ifndef USE_ASM
static inline unsigned long do_conv2(unsigned long *data, long *m, long pit) {
	long rt=0, gt=0, bt=0;

	conv_add2(rt, gt, bt, data[        -1]);
	conv_add2(rt, gt, bt, data[         0]);
	conv_add2(rt, gt, bt, data[        +1]);
	conv_add2(rt, gt, bt, data[(pit>>2)-1]);
	conv_add2(rt, gt, bt, data[(pit>>2)+1]);
	conv_add2(rt, gt, bt, data[(pit>>1)-1]);
	conv_add2(rt, gt, bt, data[(pit>>1)  ]);
	conv_add2(rt, gt, bt, data[(pit>>1)+1]);
	rt = rt*m[0]+m[9];
	gt = gt*m[0]+m[9];
	bt = bt*m[0]+m[9];

	conv_add(rt, gt, bt, data[(pit>>2)  ], m[4]);

	rt>>=8;	if (rt<0) rt=0; else if (rt>255) rt=255;
	gt>>=8;	if (gt<0) gt=0; else if (gt>255) gt=255;
	bt>>=8;	if (bt<0) bt=0; else if (bt>255) bt=255;

	return (unsigned long)((rt<<16) | (gt<<8) | (bt));
}
#endif

enum MMXSTATE
{
    MMX_UNKNOWN,
    MMX_PRESENT,
    MMX_ABSENT

};

#define _CPU_FEATURE_MMX 0x0001 
#define _CPU_FEATURE_SSE 0x0002 
#define _CPU_FEATURE_SSE2 0x0004 
//#define _CPU_FEATURE_3DNOW 0x0008 


bool IsCPUID(void) 
{ 
    __try
    { 
        _asm
        { 
            xor eax, eax 
            cpuid 
        } 
    } 
    __except (EXCEPTION_EXECUTE_HANDLER)
    { 
        return false; 
    } 
    return true; 
} 

int _os_support(int feature) 
{ 
    __try
    { 
        switch (feature)
        { 
        case _CPU_FEATURE_SSE: 
        __asm
        { 
            xorps xmm0, xmm0 // executing SSE instruction 
        } 
        break; 
        case _CPU_FEATURE_SSE2: 
        __asm
        { 
            xorpd xmm0, xmm0 // executing SSE2 instruction 
        } 
        //break; 
        //case _CPU_FEATURE_3DNOW: 
        //__asm
        //{ 
        //    pfrcp mm0, mm0 // executing 3DNow! instruction 
        //    emms 
        //} 
        //break; 
        case _CPU_FEATURE_MMX: 
        __asm
        { 
            pxor mm0, mm0 // executing MMX instruction 
            emms 
        } 
        break; 
        } 
    } 
    __except (EXCEPTION_EXECUTE_HANDLER)
    { 
        if (_exception_code() == STATUS_ILLEGAL_INSTRUCTION)
        { 
            return 0; 
        } 
        return 0; 
    } 
    return 1; 
} 


void TSCALL sharpen_run(bitmap_c &obm, const uint8 *sou, const imgdesc_s &souinfo, int lv)
{
    ASSERT(obm.info().sz == souinfo.sz && obm.info().bytepp() == 4);

    unsigned long w,h;
	const uint32 *src = (const uint32 *)sou;
    uint32 *dst = (uint32 *)obm.body();
	auint pitch = souinfo.pitch;

    static MMXSTATE mmx = MMX_UNKNOWN;

    if (mmx == MMX_UNKNOWN)
    {
        mmx = _os_support(_CPU_FEATURE_MMX) ? MMX_PRESENT:MMX_ABSENT;
    }

//    ConvoluteFilterData fd;

	long m[9];

    for(int i=0; i<9; i++)
        if (i==4) m[4] = 256+8*lv; else m[i]=-lv;

	src -= pitch>>2;

	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_RIGHTOK, pitch);
	w = souinfo.sz.x-2;
	do { *dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_BOTTOMOK | C_LEFTOK, pitch);

#ifdef USE_ASM
    if (mmx == MMX_PRESENT)
    {
        _asm emms
	    asm_sharpen_run_MMX(
			    dst+1,
			    src+1,
			    souinfo.sz.x-2,
			    souinfo.sz.y-2,
			    souinfo.pitch,
			    obm.info().pitch,
			    m[0],
			    m[4]);
        _asm finit
    } else
    {
	    asm_sharpen_run(
			    dst+1,
			    src+1,
			    souinfo.sz.x-2,
			    souinfo.sz.y-2,
			    souinfo.pitch,
			    obm.info().pitch,
			    m[0],
			    m[4]);
    }
#endif

	h = souinfo.sz.y-2;
	do {
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_RIGHTOK, pitch);
#ifdef USE_ASM
		src += souinfo.sz.x-2;
		dst += souinfo.sz.x-2;
#else
		w = souinfo.sz.x-2;
		do { *dst++ = do_conv2(src++, m, pitch); } while(--w);
#endif
		*dst++ = do_conv(src++, m, C_TOPOK | C_BOTTOMOK | C_LEFTOK, pitch);

		//src += fa->src.modulo>>2;
		//dst += fa->dst.modulo>>2;
	} while(--h);

	*dst++ = do_conv(src++, m, C_TOPOK | C_RIGHTOK, pitch);
	w = souinfo.sz.x-2;
	do { *dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK | C_RIGHTOK, pitch); } while(--w);
	*dst++ = do_conv(src++, m, C_TOPOK | C_LEFTOK, pitch);

}


} // namespace ts

