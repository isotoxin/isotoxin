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

#pragma warning (disable:4456) // declaration of 'xxx' hides previous local declaration

/////////////////////////////////////////////////////////////////////
namespace ts
{
#ifndef MODE64
#define USE_ASM
#endif

#ifdef _MSC_VER
	#pragma warning(disable: 4799)		// function has no EMMS instruction
#endif

typedef unsigned char	Pixel8;
typedef long			PixCoord;
typedef	long			PixDim;
typedef	ptrdiff_t		PixOffset;

int VDRoundToInt(double x) 
{
	return (int)floor(x + 0.5);
}

/////////////////////////////////////////////////////////////////////

namespace {
//	void MakeCubic4Table(
//		int *table,			pointer to 256x4 int array
//		double A,			'A' value - determines characteristics
//		mmx_table);			generate interleaved table
//
//	Generates a table suitable for cubic 4-point interpolation.
//
//	Each 4-int entry is a set of four coefficients for a point
//	(n/256) past y[1].  They are in /16384 units.
//
//	A = -1.0 is the original VirtualDub bicubic filter, but it tends
//	to oversharpen video, especially on rotates.  Use A = -0.75
//	for a filter that resembles Photoshop's.


	void MakeCubic4Table(int *table, double A, bool mmx_table) {
	int i;

	for(i=0; i<256; i++) {
		double d = (double)i / 256.0;
		int y1, y2, y3, y4, ydiff;

		// Coefficients for all four pixels *must* add up to 1.0 for
		// consistent unity gain.
		//
		// Two good values for A are -1.0 (original VirtualDub bicubic filter)
		// and -0.75 (closely matches Photoshop).

		y1 = (int)floor(0.5 + (        +     A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
		y2 = (int)floor(0.5 + (+ 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
		y3 = (int)floor(0.5 + (        -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
		y4 = (int)floor(0.5 + (                  +           A*d*d -       A*d*d*d) * 16384.0);

		// Normalize y's so they add up to 16384.

		ydiff = (16384 - y1 - y2 - y3 - y4)/4;
		ASSERT(ydiff > -16 && ydiff < 16);

		y1 += ydiff;
		y2 += ydiff;
		y3 += ydiff;
		y4 += ydiff;

		if (mmx_table) {
			table[i*4 + 0] = table[i*4 + 1] = (y2<<16) | (y1 & 0xffff);
			table[i*4 + 2] = table[i*4 + 3] = (y4<<16) | (y3 & 0xffff);
		} else {
			table[i*4 + 0] = y1;
			table[i*4 + 1] = y2;
			table[i*4 + 2] = y3;
			table[i*4 + 3] = y4;
		}
	}
}

struct RotateRow {
	int leftzero, left, right, rightzero;
	int64 xaccum_left, yaccum_left;
};
}

struct VDRotate2FilterData
{
	int angle;
	int filtmode;

	// working variables

	int64	u_step;
	int64	v_step;

	RotateRow *rows;
	int *coeff_tbl;

	uint8* src;
	int src_h;
	int src_w;
	int src_pitch;	
    uint8* dst;
	int dst_h;
	int dst_w;
	int dst_pitch;	

	TSCOLOR	rgbColor;

	bool	fExpandBounds;
};

/////////////////////////////////////////////////////////////////////

#ifdef USE_ASM
extern "C" void __cdecl asm_rotate_point(
        uint32 *src,
        uint32 *dst,
		long width,
		long Ufrac,
		long Vfrac,
		long UVintstepV,
		long UVintstepnoV,
		long Ustep,
		long Vstep);

extern "C" void __cdecl asm_rotate_bilinear(
        uint32 *src,
        uint32 *dst,
		long width,
		long pitch,
		long Ufrac,
		long Vfrac,
		long UVintstepV,
		long UVintstepnoV,
		long Ustep,
		long Vstep);
#endif

/////////////////////////////////////////////////////////////////////

static uint32 bilinear_interp( uint32 c1, uint32 c2, uint32 c3, uint32 c4, unsigned long cox, unsigned long coy) {
	int co1, co2, co3, co4;

	co4 = (cox * coy) >> 8;
	co3 = coy - co4;
	co2 = cox - co4;
	co1 = 0x100 - coy - co2;

    uint32 cc1 = (c1 >> 8) & 0x00FF00FF;
    uint32 cc2 = (c2 >> 8) & 0x00FF00FF;
    uint32 cc3 = (c3 >> 8) & 0x00FF00FF;
    uint32 cc4 = (c4 >> 8) & 0x00FF00FF;

	return  ((((c1 & 0x00FF00FF)*co1 + (c2 & 0x00FF00FF)*co2 + (c3 & 0x00FF00FF)*co3 + (c4 & 0x00FF00FF)*co4)>>8)&0x00FF00FF)
			+ (((cc1 * co1) + (cc2 * co2) + (cc3 * co3) + (cc4 * co4)) & 0xFF00FF00);
}

#define ALPHA(x) ((signed long)((x)>>24)&255)
#define RED(x) ((signed long)((x)>>16)&255)
#define GRN(x) ((signed long)((x)>> 8)&255)
#define BLU(x) ((signed long)(x)&255)

static inline uint32 cc(const uint32 *yptr, const int *tbl) {
	const uint32 y1 = yptr[0];
	const uint32 y2 = yptr[1];
	const uint32 y3 = yptr[2];
	const uint32 y4 = yptr[3];
	long alpha, red, grn, blu;

	alpha = ALPHA(y1)*tbl[0] + ALPHA(y2)*tbl[1] + ALPHA(y3)*tbl[2] + ALPHA(y4)*tbl[3];
	red   = RED(y1)*tbl[0] + RED(y2)*tbl[1] + RED(y3)*tbl[2] + RED(y4)*tbl[3];
	grn   = GRN(y1)*tbl[0] + GRN(y2)*tbl[1] + GRN(y3)*tbl[2] + GRN(y4)*tbl[3];
	blu   = BLU(y1)*tbl[0] + BLU(y2)*tbl[1] + BLU(y3)*tbl[2] + BLU(y4)*tbl[3];

	if (alpha<0) alpha=0; else if (alpha>4194303) alpha=4194303;
	if (red<0) red=0; else if (red>4194303) red=4194303;
	if (grn<0) grn=0; else if (grn>4194303) grn=4194303;
	if (blu<0) blu=0; else if (blu>4194303) blu=4194303;

	return ((alpha<<10) & 0xFF000000) | ((red<<2) & 0x00FF0000) | ((grn>>6) & 0x0000FF00) | (blu>>14);
}

#undef ALPHA
#undef RED
#undef GRN
#undef BLU

#ifdef _M_IX86
static uint32 __declspec(naked) __cdecl cc_MMX(const uint32 *src, const int *table) {

	static const int64 x0000200000002000 = 0x0000200000002000i64;

	//	[esp + 4]	src
	//	[esp + 8]	table

	_asm {
		mov			ecx,[esp+4]
		mov			eax,[esp+8]

		movd		mm0,[ecx]
		pxor		mm7,mm7

		movd		mm1,[ecx+4]
		punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

		movd		mm2,[ecx+8]
		punpcklbw	mm1,mm7				;mm1 = [a2][r2][g2][b2]

		movd		mm3,[ecx+12]
		punpcklbw	mm2,mm7				;mm2 = [a3][r3][g3][b3]

		punpcklbw	mm3,mm7				;mm3 = [a4][r4][g4][b4]
		movq		mm4,mm0				;mm0 = [a1][r1][g1][b1]

		punpcklwd	mm0,mm1				;mm0 = [g2][g1][b2][b1]
		movq		mm5,mm2				;mm2 = [a3][r3][g3][b3]

		pmaddwd		mm0,[eax]
		punpcklwd	mm2,mm3				;mm2 = [g4][g3][b4][b3]

		pmaddwd		mm2,[eax+8]
		punpckhwd	mm4,mm1				;mm4 = [a2][a1][r2][r1]

		pmaddwd		mm4,[eax]
		punpckhwd	mm5,mm3				;mm5 = [a4][a3][b4][b3]

		pmaddwd		mm5,[eax+8]
		paddd		mm0,mm2				;mm0 = [ g ][ b ]

		paddd		mm0,x0000200000002000
		;idle V

		paddd		mm4,x0000200000002000
		psrad		mm0,14

		paddd		mm4,mm5				;mm4 = [ a ][ r ]

		psrad		mm4,14

		packssdw	mm0,mm4				;mm0 = [ a ][ r ][ g ][  b ]
		packuswb	mm0,mm0				;mm0 = [a][r][g][b][a][r][g][b]

		movd		eax,mm0
		ret
	}
}

static inline uint32 bicubic_interp_MMX(const uint32 *src, PixOffset pitch, unsigned long cox, unsigned long coy, const int *table) {
	uint32 x[4];

	cox >>= 24;
	coy >>= 24;

	src = (uint32 *)((char *)src - pitch - 4);

	x[0] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[1] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[2] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[3] = cc_MMX(src, table+cox*4);

	return cc_MMX(x, table + coy*4);
}
#endif

static inline uint32 bicubic_interp(const uint32 *src, PixOffset pitch, unsigned long cox, unsigned long coy, const int *table) {
	uint32 x[4];

	cox >>= 24;
	coy >>= 24;

	src = (uint32 *)((char *)src - pitch - 4);

	x[0] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[1] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[2] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[3] = cc(src, table+cox*4);

	return cc(x, table + coy*4);
}

static uint32 ColorRefToPixel32( TSCOLOR rgb) {
	return (uint32)(((rgb>>16)&0xff) | ((rgb<<16)&0xff0000) | (rgb&0xff00));
}

static int rotate2_run(VDRotate2FilterData *mfd ) {
	int64 xaccum, yaccum;
	uint32 *src, *dst, pixFill;
	PixDim w, h;
	const RotateRow *rr = mfd->rows;
	const int64 du = mfd->u_step;
	const int64 dv = mfd->v_step;

	unsigned long Ustep, Vstep;
	ptrdiff_t UVintstepV, UVintstepnoV;

	// initialize Abrash texmapping variables :)

	Ustep = (unsigned long)du;
	Vstep = (unsigned long)dv;

	UVintstepnoV = (long)(du>>32) + (long)(dv>>32)*(mfd->src_pitch>>2);
	UVintstepV = UVintstepnoV + (mfd->src_pitch>>2);

	dst = (uint32* )mfd->dst;
	pixFill = ColorRefToPixel32(mfd->rgbColor);

	h = mfd->dst_h;
	do {
		// texmap!

		w = rr->leftzero;
		if (w) do {
			*dst++ = pixFill;
		} while(--w);

		xaccum = rr->xaccum_left;
		yaccum = rr->yaccum_left;

		switch(mfd->filtmode) {
		case FILTMODE_POINT:
			w = mfd->dst_w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {

#ifdef USE_ASM
				asm_rotate_point(
					(uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch),
					dst,
					w,
					(unsigned long)xaccum,
					(unsigned long)yaccum,
					(long)UVintstepV,
                    (long)UVintstepnoV,
					Ustep,
					Vstep);

				dst += w;

#else
				do {
					*dst++ = *(uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);

					xaccum += du;
					yaccum += dv;
				} while(--w);
#endif
				break;
			}
			break;
		case FILTMODE_BILINEAR:
			w = rr->left;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + mfd->src_pitch);

					--px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + mfd->src_pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}

			w = mfd->dst_w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {
#ifdef USE_ASM
				asm_rotate_bilinear(
						(uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch),
						dst,
						w,
						mfd->src_pitch,
						(unsigned long)xaccum,
						(unsigned long)yaccum,
                    (long)UVintstepV,
                    (long)UVintstepnoV,
						Ustep,
						Vstep);

				xaccum += du*w;
				yaccum += dv*w;
				dst += w;
#else
				do {
					uint32 *src = (uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);
					uint32 c1, c2, c3, c4, cY;
					int co1, co2, co3, co4, cox, coy;

					c1 = *(uint32 *)((char *)src + 0);
					c2 = *(uint32 *)((char *)src + 4);
					c3 = *(uint32 *)((char *)src + 0 + mfd->src_pitch);
					c4 = *(uint32 *)((char *)src + 4 + mfd->src_pitch);

					cox = ((unsigned long)xaccum >> 24);
					coy = ((unsigned long)yaccum >> 24);

					co4 = (cox * coy) >> 8;
					co3 = coy - co4;
					co2 = cox - co4;
					co1 = 0x100 - coy - co2;

					uint32 cc1 = (c1 >> 8) & 0x00FF00FF;
					uint32 cc2 = (c2 >> 8) & 0x00FF00FF;
					uint32 cc3 = (c3 >> 8) & 0x00FF00FF;
					uint32 cc4 = (c4 >> 8) & 0x00FF00FF;

					cY =  ((((c1 & 0x00FF00FF)*co1 + (c2 & 0x00FF00FF)*co2 + (c3 & 0x00FF00FF)*co3 + (c4 & 0x00FF00FF)*co4)>>8)&0x00FF00FF);
					cY += ((cc1 * co1) + (cc2 * co2) + (cc3 * co3) + (cc4 * co4)) & 0xFF00FF00;
/*
					cY = ((((c1 & 0x00FF00FF)*co1 + (c2 & 0x00FF00FF)*co2 + (c3 & 0x00FF00FF)*co3 + (c4 & 0x00FF00FF)*co4)>>8)&0x00FF00FF)
					   + ((((c1 & 0x0000FF00)*co1 + (c2 & 0x0000FF00)*co2 + (c3 & 0x0000FF00)*co3 + (c4 & 0x0000FF00)*co4)&0x00FF0000)>>8);
*/
					*dst++ = cY;

					xaccum += du;
					yaccum += dv;
				} while(--w);
#endif
			}

			w = rr->right;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=mfd->src_w  || py>=mfd->src_h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=mfd->src_w  || py>=mfd->src_h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + mfd->src_pitch);

					--px;

					if (px<0 || py<0 || px>=mfd->src_w  || py>=mfd->src_h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + mfd->src_pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}
			break;

		case FILTMODE_BICUBIC:
			w = rr->left;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + mfd->src_pitch);

					--px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + mfd->src_pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}

			w = mfd->dst_w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {
				int64 xa = xaccum;
				int64 ya = yaccum;

				src = (uint32*)((char *)mfd->src + (int)(xa>>32)*4 + (int)(ya>>32)*mfd->src_pitch);

				xaccum += du * w;
				yaccum += dv * w;

#ifdef _M_IX86
				do {
					*dst++ = bicubic_interp_MMX(src, mfd->src_pitch, (unsigned long)xa, (unsigned long)ya, mfd->coeff_tbl);

					xa = (int64)(unsigned long)xa + Ustep;
					ya = (int64)(unsigned long)ya + Vstep;

					src += (xa>>32) + (ya>>32 ? UVintstepV : UVintstepnoV);

					xa = (unsigned long)xa;
					ya = (unsigned long)ya;
				} while(--w);
#else
                do {
                    *dst++ = bicubic_interp(src, mfd->src_pitch, (unsigned long)xa, (unsigned long)ya, mfd->coeff_tbl);

                    xa = (int64)(unsigned long)xa + Ustep;
                    ya = (int64)(unsigned long)ya + Vstep;

                    src += (xa>>32) + (ya>>32 ? UVintstepV : UVintstepnoV);

                    xa = (unsigned long)xa;
                    ya = (unsigned long)ya;
                } while(--w);
#endif

			}

			w = rr->right;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)mfd->src + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*mfd->src_pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + mfd->src_pitch);

					--px;

					if (px<0 || py<0 || px>=mfd->src_w || py>=mfd->src_h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + mfd->src_pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}
			break;
		}

		w = rr->rightzero;
		if (w) do {
			*dst++ = pixFill;
		} while(--w);

//		dst = (uint32 *)((char *)dst + mfd->dst.modulo);

		++rr;

	} while(--h);

#ifdef _M_IX86
	__asm emms
#endif

	return 0;
}

static void rotate2_param(VDRotate2FilterData *mfd) 
{
	int destw, desth;

	if (mfd->fExpandBounds) {
		double ang = mfd->angle * (3.14159265358979323846 / 2147483648.0);
		double xcos = cos(ang);
		double xsin = sin(ang);
		int destw1, destw2, desth1, desth2;

		// Because the rectangle is symmetric, we only
		// need to rotate two corners and pick the farthest.

		destw1 = VDRoundToInt(fabs(mfd->src_w * xcos - mfd->src_h * xsin));
		desth1 = VDRoundToInt(fabs(mfd->src_h * xcos + mfd->src_w * xsin));
		destw2 = VDRoundToInt(fabs(mfd->src_w * xcos + mfd->src_h * xsin));
		desth2 = VDRoundToInt(fabs(mfd->src_h * xcos - mfd->src_w * xsin));

		destw = tmax(destw1, destw2);
		desth = tmax(desth1, desth2);
	} else {
		destw = mfd->src_w;
		desth = mfd->src_h;
	}

	mfd->dst_w = destw;
	mfd->dst_h = desth;
}
///////////////////////////////////////////

static int rotate2_start(VDRotate2FilterData *mfd) {

	// Compute step parameters.

	double ang = mfd->angle * (3.14159265358979323846 / 2147483648.0);
	double ustep = cos(ang);
	double vstep = -sin(ang);
	int64 du, dv;

	mfd->u_step = du = (int64)floor(ustep*4294967296.0 + 0.5);
	mfd->v_step = dv = (int64)floor(vstep*4294967296.0 + 0.5);

    mfd->rows = (RotateRow *)MM_ALLOC(sizeof(RotateRow)* mfd->dst_h);
    if (mfd->rows == NULL) return 1;

	// It's time for Mr.Bonehead!!

	int x0, x1, x2, x3, y;
	int64 xaccum, yaccum;
	int64 xaccum_low, yaccum_low, xaccum_high, yaccum_high, xaccum_base, yaccum_base;
	int64 xaccum_low2 = 0, yaccum_low2 = 0, xaccum_high2 = 0, yaccum_high2 = 0;
	RotateRow *rr = mfd->rows;

	// Compute allowable source bounds.

	xaccum_base = xaccum_low = -0x80000000LL*mfd->src_w;
	yaccum_base = yaccum_low = -0x80000000LL*mfd->src_h;
	xaccum_high = 0x80000000LL*mfd->src_w;
	yaccum_high = 0x80000000LL*mfd->src_h;

	// Compute accumulators for top-left destination position.

	xaccum = ( dv*(mfd->dst_h-1) - du*(mfd->dst_w-1))/2;
	yaccum = (-du*(mfd->dst_h-1) - dv*(mfd->dst_w-1))/2;

	// Compute 'marginal' bounds that require partial clipping.

	switch(mfd->filtmode) {
	case FILTMODE_POINT:
		xaccum_low2 = xaccum_low;
		yaccum_low2 = yaccum_low;
		xaccum_high2 = xaccum_high;
		yaccum_high2 = yaccum_high;
		break;
	case FILTMODE_BILINEAR:
		xaccum_low2 = xaccum_low;
		yaccum_low2 = yaccum_low;
		xaccum_high2 = xaccum_high - 0x100000000LL;
		yaccum_high2 = yaccum_high - 0x100000000LL;
		xaccum_low -= 0x100000000LL;
		yaccum_low -= 0x100000000LL;

		xaccum -= 0x80000000LL;
		yaccum -= 0x80000000LL;
		break;

	case FILTMODE_BICUBIC:
		xaccum_low2  = xaccum_low  + 0x100000000LL;
		yaccum_low2  = yaccum_low  + 0x100000000LL;
		xaccum_high2 = xaccum_high - 0x200000000LL;
		yaccum_high2 = yaccum_high - 0x200000000LL;
		xaccum_low -= 0x100000000LL;
		yaccum_low -= 0x100000000LL;

		xaccum -= 0x80000000LL;
		yaccum -= 0x80000000LL;
		break;
	}

	for(y=0; y<mfd->dst_h; y++) {
		int64 xa, ya;

		xa = xaccum;
		ya = yaccum;

		for(x0=0; x0<mfd->dst_w; x0++) {
			if (xa >= xaccum_low && ya >= yaccum_low && xa < xaccum_high && ya < yaccum_high)
				break;

			xa += du;
			ya += dv;
		}

		rr->xaccum_left = xa - xaccum_base;
		rr->yaccum_left = ya - yaccum_base;

		for(x1=x0; x1<mfd->dst_w; x1++) {
			if (xa >= xaccum_low2 && ya >= yaccum_low2 && xa < xaccum_high2 && ya < yaccum_high2)
				break;

			xa += du;
			ya += dv;
		}

		for(x2=x1; x2<mfd->dst_w; x2++) {
			if (xa < xaccum_low2 || ya < yaccum_low2 || xa >= xaccum_high2 || ya >= yaccum_high2)
				break;

			xa += du;
			ya += dv;
		}

		for(x3=x2; x3<mfd->dst_w; x3++) {
			if (xa < xaccum_low || ya < yaccum_low || xa >= xaccum_high || ya >= yaccum_high)
				break;

			xa += du;
			ya += dv;
		}

		rr->leftzero = x0;
		rr->left = x1-x0;
		rr->right = x3-x2;
		rr->rightzero = mfd->dst_w - x3;
		++rr;

		xaccum -= dv;
		yaccum += du;
	}

	// Fill out cubic interpolation coeff. table

	if (mfd->filtmode == FILTMODE_BICUBIC)
    {
        mfd->coeff_tbl = (int *)MM_ALLOC(sizeof(int)*256*4);
        if (mfd->coeff_tbl==NULL) return 1;

		MakeCubic4Table(mfd->coeff_tbl, -0.75, true);
		
	}

	return 0;
}

static int rotate2_end(VDRotate2FilterData *mfd)
{
    if (mfd->rows) MM_FREE(mfd->rows); mfd->rows = NULL;
	if (mfd->coeff_tbl) MM_FREE(mfd->coeff_tbl); mfd->coeff_tbl = NULL;
	return 0;
}

bool rotate2(ts::bitmap_c &obm, ts::bitmap_c &ibm, int angle, rot_filter_e filt_mode=FILTMODE_POINT, bool expand_dst=true)
{
	VDRotate2FilterData mfd;

	mfd.angle =angle;
	mfd.src   =ibm.body();
	mfd.src_h =ibm.info().sz.y;
	mfd.src_w =ibm.info().sz.x;
	mfd.src_pitch =ibm.info().pitch;
	mfd.fExpandBounds =expand_dst;
	mfd.filtmode =filt_mode;

	rotate2_param(&mfd);

	switch( ibm.info().bytepp() )
	{
		case 4: obm.create_ARGB(ivec2(mfd.dst_w,mfd.dst_h)); break;
		default: return false;
	}

	mfd.dst =obm.body();
	mfd.dst_pitch =obm.info().pitch;

	rotate2_start(&mfd);
	rotate2_run(&mfd);
	rotate2_end(&mfd);

	return true;
}


} // namespace ts

