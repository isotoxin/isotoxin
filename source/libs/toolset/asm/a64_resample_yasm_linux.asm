;	VirtualDub - Video processing and capture application
;	Graphics support library
;	Copyright (C) 1998-2004 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License	
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;
SECTION .rodata
roundval		dq	0000200000002000h, 0000200000002000h


SECTION .text

;-------------------------------------------------------------------------
;
;	long vdasm_resize_table_row_SSE2(
;		Pixel *out,			// rdi
;		Pixel *in,			// rsi
;		int *filter,		// rdx
;		int filter_width,	// ecx
;		PixDim w,			// r8d
;		long accum,			// r9d
;		long frac);			// [rsp+40]
;
	global vdasm_resize_table_row_SSE2

vdasm_resize_table_row_SSE2:

	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	sub rsp, 32+8 ; +8 - align
	movdqa	[rsp], xmm6
	movdqa	[rsp+16], xmm7

parms equ rsp+48+32+8

	shl			r8, 2
	add			rdi, r8
	neg			r8
	shl			ecx, 2					;filter_width <<= 2

	movaps		xmm6, [roundval]
	pxor		xmm5, xmm5
	mov			r10, rsi
	shr			r10, 2

	mov			eax, r9d
	shl			r9d, 16
	sar			rax, 16
	add			r10, rax
	mov			ebp, [parms+40]
	movsxd		r11, ebp
	shl			ebp, 16
	sar			r11, 16

	;register map
	;
	;eax		temp coefficient pair counter
	;rbx		temp coefficient pointer
	;rdi		destination
	;rsi		temp source
	;r10		source/4
	;r9d		accumulator
	;ebp		fractional increment
	;rdx		filter
	;rcx		filter_width*4
	;r8			-width*4
	;r11		integral increment
	;r12
	;r13
	;r14
	;r15

	cmp			ecx, 16
	jz			accel_4coeff1
	cmp			ecx, 24
	jz			accel_6coeff1

	test		ecx, 8
	jz			pixelloop_even_pairs
	cmp			ecx, 8
	jnz			pixelloop_odd_pairs

pixelloop_single_pairs:
	mov			eax, r9d
	shr			eax, 24
	imul		eax, ecx
	
	lea			rsi, [r10*4]

	movd		xmm0, [rsi]		;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm0, xmm5
	movq		xmm1, [rdx+rax]
	pshufd		xmm1, xmm1, 01000100b
	pmaddwd		xmm0, xmm1
	
	movdqa		xmm4, xmm6
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			r9d, ebp
	adc			r10, r11

	movd		[rdi+r8], xmm4
	add			r8, 4
	jnz			pixelloop_single_pairs
	jmp			xit1

pixelloop_odd_pairs:
	movdqa		xmm4, xmm6

	mov			eax, r9d
	shr			eax, 24
	imul		eax, ecx
	lea			rbx, [rdx+rax]

	lea			rsi, [r10*4]
	lea			rax, [rcx-8]
coeffloop_odd_pairs:
	movd		xmm0, [rsi]			;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	movd		xmm2, [rsi+8]		;xmm2 = p2
	movd		xmm3, [rsi+12]		;xmm3 = p3
	add			rsi, 16
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, [rbx]
	movq		xmm3, [rbx+8]
	add			rbx, 16
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0
	sub			eax, 16
	jnz			coeffloop_odd_pairs

	movd		xmm0, [rsi]		;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm0, xmm5
	movq		xmm1, [rbx]
	pshufd		xmm1, xmm1, 01000100b
	pmaddwd		xmm0, xmm1
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			r9d, ebp
	adc			r10, r11

	movd		[rdi+r8], xmm4
	add			r8, 4
	jnz			pixelloop_odd_pairs
	jmp			xit1

pixelloop_even_pairs:
	movdqa		xmm4, xmm6

	mov			eax, r9d
	shr			eax, 24
	imul		eax, ecx
	lea			rbx, [rdx+rax]

	lea			rsi, [r10*4]
	mov			eax, ecx
coeffloop_even_pairs:
	movd		xmm0, [rsi]			;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	movd		xmm2, [rsi+8]		;xmm2 = p2
	movd		xmm3, [rsi+12]		;xmm3 = p3
	add			rsi, 16
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, [rbx]
	movq		xmm3, [rbx+8]
	add			rbx, 16
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0
	sub			eax, 16
	jnz			coeffloop_even_pairs

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			r9d, ebp
	adc			r10, r11

	movd		[rdi+r8], xmm4
	add			r8, 4
	jnz			pixelloop_even_pairs

xit1:
	movdqa	xmm6, [rsp]
	movdqa	xmm7, [rsp+16]
	add rsp, 32+8

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx

	ret


accel_4coeff1:
pixelloop_4coeff1:
	pxor		xmm5, xmm5
	movdqa		xmm4, xmm6

	mov			eax, 0ff000000h
	lea			rsi, [r10*4]
	and			eax, r9d
	shr			eax, 20
	lea			rbx, [rdx+rax]

	movd		xmm0, [rsi]			;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	movd		xmm2, [rsi+8]		;xmm2 = p2
	movd		xmm3, [rsi+12]		;xmm3 = p3
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, [rbx]
	movq		xmm3, [rbx+8]
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			r9d, ebp
	adc			r10, r11

	movd		[rdi+r8], xmm4
	add			r8, 4
	jnz			pixelloop_4coeff1
	jmp			xit1

accel_6coeff1:
pixelloop_6coeff1:
	pxor		xmm5, xmm5
	movdqa		xmm4, xmm6

	lea			rsi, [r10*4]
	mov			eax, r9d
	shr			eax, 24
	lea			rax, [rax+rax*2]
	lea			rbx, [rdx+rax*8]

	movd		xmm0, [rsi]			;xmm0 = p0
	movd		xmm1, [rsi+4]		;xmm1 = p1
	movd		xmm2, [rsi+8]		;xmm2 = p2
	movd		xmm3, [rsi+12]		;xmm3 = p3
	movd		xmm8, [rsi+16]		;xmm6 = p4
	movd		xmm9, [rsi+20]		;xmm7 = p5
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm8, xmm9
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	punpcklbw	xmm8, xmm5
	movq		xmm1, [rbx]
	movq		xmm3, [rbx+8]
	movq		xmm9, [rbx+16]
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pshufd		xmm9, xmm9, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	pmaddwd		xmm8, xmm9
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0
	paddd		xmm4, xmm8

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			r9d, ebp
	adc			r10, r11

	movd		[rdi+r8], xmm4
	add			r8, 4
	jnz			pixelloop_6coeff1
	jmp			xit1



;--------------------------------------------------------------------------
;
;	vdasm_resize_table_col_SSE2(
;		uint32 *dst,				// rdi
;		const uint32 *const *srcs,	// rsi
;		int *filter,				// rdx
;		int filter_width,			// ecx
;		PixDim w,					// r8d
;		);
;
	global	vdasm_resize_table_col_SSE2
vdasm_resize_table_col_SSE2:

	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	sub rsp, 32+8
	movdqa	[rsp], xmm6
	movdqa	[rsp+16], xmm7


	;parms equ rsp+48+32+8

	pxor		xmm5, xmm5
	movdqa		xmm4, [roundval]
	xor			rbx, rbx					;rbx = source offset

	cmp			ecx, 4
	jz			accel_4coeff2
	cmp			ecx, 6
	jz			accel_6coeff2

	shr			ecx, 1						;ecx = filter pair count

pixelloop:
	mov			rax, rsi					;rax = row pointer table
	mov			r9, rdx						;r9 = filter
	mov			r11d, ecx					;r11d = filter width counter
	movdqa		xmm2, xmm4
coeffloop:
	mov			r10, [rax]

	movd		xmm0, [r10+rbx]

	mov			r10, [rax+8]
	add			rax, 16

	movd		xmm1, [r10+rbx]
	punpcklbw	xmm0, xmm1

	punpcklbw	xmm0, xmm5

	movq		xmm1, [r9]
	pshufd		xmm1, xmm1, 01000100b

	pmaddwd		xmm0, xmm1

	paddd		xmm2, xmm0

	add			r9,8

	sub			r11d,1
	jne			coeffloop

	psrad		xmm2,14
	packssdw	xmm2,xmm2
	add			rbx,4
	packuswb	xmm2,xmm2

	movd		[rdi],xmm2
	add			rdi,4
	sub			r8d,1
	jne			pixelloop

xit2:
	movdqa	xmm6, [rsp]
	movdqa	xmm7, [rsp+16]
	add rsp, 32+8

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx


	ret

accel_4coeff2:
	mov			r12, [rsi]
	mov			r13, [rsi+8]
	mov			r14, [rsi+16]
	mov			r15, [rsi+24]
	movq		xmm8, [rdx]
	punpcklqdq	xmm8, xmm8
	movq		xmm9, [rdx+8]
	punpcklqdq	xmm9, xmm9

	sub			r8d, 1
	jc			oddpixel_4coeff
pixelloop_4coeff2:
	movq		xmm0, [r12+rbx]
	movq		xmm1, [r13+rbx]
	movq		xmm2, [r14+rbx]
	movq		xmm3, [r15+rbx]

	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3

	movdqa		xmm1, xmm0
	movdqa		xmm3, xmm2

	punpcklbw	xmm0, xmm5
	punpckhbw	xmm1, xmm5
	punpcklbw	xmm2, xmm5
	punpckhbw	xmm3, xmm5

	pmaddwd		xmm0, xmm8
	pmaddwd		xmm1, xmm8
	pmaddwd		xmm2, xmm9
	pmaddwd		xmm3, xmm9

	paddd		xmm0, xmm4
	paddd		xmm1, xmm4
	paddd		xmm0, xmm2
	paddd		xmm1, xmm3

	psrad		xmm0, 14
	psrad		xmm1, 14
	packssdw	xmm0, xmm1
	packuswb	xmm0, xmm0

	movq		[rdi], xmm0
	add			rdi, 8
	add			rbx, 8
	sub			r8d, 2
	ja			pixelloop_4coeff2
	jnz			xit2
oddpixel_4coeff:
	movd		xmm0, [r12+rbx]
	movd		xmm1, [r13+rbx]
	movd		xmm2, [r14+rbx]
	movd		xmm3, [r15+rbx]

	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5

	pmaddwd		xmm0, xmm8
	pmaddwd		xmm2, xmm9

	paddd		xmm0, xmm4
	paddd		xmm0, xmm2

	psrad		xmm0, 14
	packssdw	xmm0, xmm0
	packuswb	xmm0, xmm0

	movd		[rdi], xmm0

	jmp			xit2

accel_6coeff2:
	mov			r12, [rsi]
	mov			r13, [rsi+8]
	mov			r14, [rsi+16]
	mov			r15, [rsi+24]
	mov			r10, [rsi+32]
	mov			rsi, [rsi+40]
	movq		xmm10, [rdx]
	punpcklqdq	xmm10, xmm10
	movq		xmm11, [rdx+8]
	punpcklqdq	xmm11, xmm11
	movq		xmm12, [rdx+16]
	punpcklqdq	xmm12, xmm12

	sub			r8d, 1
	jc			oddpixel_6coeff
pixelloop_6coeff2:
	movq		xmm0, [r12+rbx]
	movq		xmm1, [r13+rbx]
	movq		xmm2, [r14+rbx]
	movq		xmm3, [r15+rbx]
	movq		xmm8, [r10+rbx]
	movq		xmm9, [rsi+rbx]

	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm8, xmm9

	movdqa		xmm1, xmm0
	movdqa		xmm3, xmm2
	movdqa		xmm9, xmm8

	punpcklbw	xmm0, xmm5
	punpckhbw	xmm1, xmm5
	punpcklbw	xmm2, xmm5
	punpckhbw	xmm3, xmm5
	punpcklbw	xmm8, xmm5
	punpckhbw	xmm9, xmm5

	pmaddwd		xmm0, xmm10
	pmaddwd		xmm1, xmm10
	pmaddwd		xmm2, xmm11
	pmaddwd		xmm3, xmm11
	pmaddwd		xmm8, xmm12
	pmaddwd		xmm9, xmm12

	paddd		xmm0, xmm4
	paddd		xmm1, xmm4
	paddd		xmm2, xmm8
	paddd		xmm3, xmm9
	paddd		xmm0, xmm2
	paddd		xmm1, xmm3

	psrad		xmm0, 14
	psrad		xmm1, 14
	packssdw	xmm0, xmm1
	packuswb	xmm0, xmm0

	movq		[rdi], xmm0
	add			rdi, 8
	add			rbx, 8
	sub			r8d, 2
	ja			pixelloop_6coeff2
	jnz			xit2
oddpixel_6coeff:
	movd		xmm0, [r12+rbx]
	movd		xmm1, [r13+rbx]
	movd		xmm2, [r14+rbx]
	movd		xmm3, [r15+rbx]
	movd		xmm8, [r10+rbx]
	movd		xmm9, [rsi+rbx]

	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm8, xmm9
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	punpcklbw	xmm8, xmm5

	pmaddwd		xmm0, xmm10
	pmaddwd		xmm2, xmm11
	pmaddwd		xmm8, xmm12

	paddd		xmm0, xmm4
	paddd		xmm2, xmm8
	paddd		xmm0, xmm2

	psrad		xmm0, 14
	packssdw	xmm0, xmm0
	packuswb	xmm0, xmm0

	movd		[rdi], xmm0

	jmp			xit2

	end
