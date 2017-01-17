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
	.data
roundval		dq	0000200000002000h, 0000200000002000h


	.code

;-------------------------------------------------------------------------
;
;	long vdasm_resize_table_row_SSE2(
;		Pixel *out,			// rcx
;		Pixel *in,			// rdx
;		int *filter,		// r8
;		int filter_width,	// r9d
;		PixDim w,			// [rsp+40]
;		long accum,			// [rsp+48]
;		long frac);			// [rsp+56]
;
	public vdasm_resize_table_row_SSE2

vdasm_resize_table_row_SSE2:

	push rbx
	push rsi
	push rdi
	push rbp
	push r12
	push r13
	push r14
	push r15

	sub rsp, 32+8 ; +8 - align
	movdqa	oword ptr [rsp], xmm6
	movdqa	oword ptr [rsp+16], xmm7

parms equ rsp+64+32+8

	mov			r10d, dword ptr [parms+40]
	shl			r10, 2
	add			rcx, r10
	neg			r10
	shl			r9d, 2					;filter_width <<= 2

	movaps		xmm6, oword ptr [roundval]
	pxor		xmm5, xmm5
	mov			rsi, rdx
	shr			rsi, 2

	mov			edi, [parms+48]
	mov			eax, edi
	shl			edi, 16
	sar			rax, 16
	add			rsi, rax
	mov			ebp, [parms+56]
	movsxd		r11, ebp
	shl			ebp, 16
	sar			r11, 16

	;register map
	;
	;eax		temp coefficient pair counter
	;rbx		temp coefficient pointer
	;rcx		destination
	;rdx		temp source
	;rsi		source/4
	;edi		accumulator
	;ebp		fractional increment
	;r8			filter
	;r9			filter_width*4
	;r10		-width*4
	;r11		integral increment
	;r12
	;r13
	;r14
	;r15

	cmp			r9d, 16
	jz			accel_4coeff1
	cmp			r9d, 24
	jz			accel_6coeff1

	test		r9d, 8
	jz			pixelloop_even_pairs
	cmp			r9d, 8
	jnz			pixelloop_odd_pairs

pixelloop_single_pairs:
	mov			eax, edi
	shr			eax, 24
	imul		eax, r9d
	
	lea			rdx, [rsi*4]

	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm0, xmm5
	movq		xmm1, qword ptr [r8+rax]
	pshufd		xmm1, xmm1, 01000100b
	pmaddwd		xmm0, xmm1
	
	movdqa		xmm4, xmm6
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			edi, ebp
	adc			rsi, r11

	movd		dword ptr [rcx+r10], xmm4
	add			r10, 4
	jnz			pixelloop_single_pairs
	jmp			xit1

pixelloop_odd_pairs:
	movdqa		xmm4, xmm6

	mov			eax, edi
	shr			eax, 24
	imul		eax, r9d
	lea			rbx, [r8+rax]

	lea			rdx, [rsi*4]
	lea			rax, [r9-8]
coeffloop_odd_pairs:
	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	movd		xmm2, dword ptr [rdx+8]		;xmm2 = p2
	movd		xmm3, dword ptr [rdx+12]		;xmm3 = p3
	add			rdx, 16
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, qword ptr [rbx]
	movq		xmm3, qword ptr [rbx+8]
	add			rbx, 16
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0
	sub			eax, 16
	jnz			coeffloop_odd_pairs

	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm0, xmm5
	movq		xmm1, qword ptr [rbx]
	pshufd		xmm1, xmm1, 01000100b
	pmaddwd		xmm0, xmm1
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			edi, ebp
	adc			rsi, r11

	movd		dword ptr [rcx+r10], xmm4
	add			r10, 4
	jnz			pixelloop_odd_pairs
	jmp			xit1

pixelloop_even_pairs:
	movdqa		xmm4, xmm6

	mov			eax, edi
	shr			eax, 24
	imul		eax, r9d
	lea			rbx, [r8+rax]

	lea			rdx, [rsi*4]
	mov			eax, r9d
coeffloop_even_pairs:
	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	movd		xmm2, dword ptr [rdx+8]		;xmm2 = p2
	movd		xmm3, dword ptr [rdx+12]		;xmm3 = p3
	add			rdx, 16
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, qword ptr [rbx]
	movq		xmm3, qword ptr [rbx+8]
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

	add			edi, ebp
	adc			rsi, r11

	movd		dword ptr [rcx+r10], xmm4
	add			r10, 4
	jnz			pixelloop_even_pairs

xit1:
	movdqa	xmm6, oword ptr [rsp]
	movdqa	xmm7, oword ptr [rsp+16]
	add rsp, 32+8

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rdi
	pop rsi
	pop rbx

	ret


accel_4coeff1:
pixelloop_4coeff1:
	pxor		xmm5, xmm5
	movdqa		xmm4, xmm6

	mov			eax, 0ff000000h
	lea			rdx, [rsi*4]
	and			eax, edi
	shr			eax, 20
	lea			rbx, [r8+rax]

	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	movd		xmm2, dword ptr [rdx+8]		;xmm2 = p2
	movd		xmm3, dword ptr [rdx+12]		;xmm3 = p3
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	movq		xmm1, qword ptr [rbx]
	movq		xmm3, qword ptr [rbx+8]
	pshufd		xmm1, xmm1, 01000100b
	pshufd		xmm3, xmm3, 01000100b
	pmaddwd		xmm0, xmm1
	pmaddwd		xmm2, xmm3
	paddd		xmm0, xmm2
	paddd		xmm4, xmm0

	psrad		xmm4, 14
	packssdw	xmm4, xmm4
	packuswb	xmm4, xmm4

	add			edi, ebp
	adc			rsi, r11

	movd		dword ptr [rcx+r10], xmm4
	add			r10, 4
	jnz			pixelloop_4coeff1
	jmp			xit1

accel_6coeff1:
pixelloop_6coeff1:
	pxor		xmm5, xmm5
	movdqa		xmm4, xmm6

	lea			rdx, [rsi*4]
	mov			eax, edi
	shr			eax, 24
	lea			rax, [rax+rax*2]
	lea			rbx, [r8+rax*8]

	movd		xmm0, dword ptr [rdx]			;xmm0 = p0
	movd		xmm1, dword ptr [rdx+4]		;xmm1 = p1
	movd		xmm2, dword ptr [rdx+8]		;xmm2 = p2
	movd		xmm3, dword ptr [rdx+12]		;xmm3 = p3
	movd		xmm8, dword ptr [rdx+16]		;xmm6 = p4
	movd		xmm9, dword ptr [rdx+20]		;xmm7 = p5
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm8, xmm9
	punpcklbw	xmm0, xmm5
	punpcklbw	xmm2, xmm5
	punpcklbw	xmm8, xmm5
	movq		xmm1, qword ptr [rbx]
	movq		xmm3, qword ptr [rbx+8]
	movq		xmm9, qword ptr [rbx+16]
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

	add			edi, ebp
	adc			rsi, r11

	movd		dword ptr [rcx+r10], xmm4
	add			r10, 4
	jnz			pixelloop_6coeff1
	jmp			xit1



;--------------------------------------------------------------------------
;
;	vdasm_resize_table_col_SSE2(
;		uint32 *dst,				// rcx
;		const uint32 *const *srcs,	// rdx
;		int *filter,		// r8
;		int filter_width,	// r9d
;		PixDim w,			// [rsp+40] -> r10d
;		);
;
	public	vdasm_resize_table_col_SSE2
vdasm_resize_table_col_SSE2:

	push rbx
	push rsi
	push rdi
	push rbp
	push r12
	push r13
	push r14
	push r15

	sub rsp, 32+8
	movdqa	oword ptr [rsp], xmm6
	movdqa	oword ptr [rsp+16], xmm7


	parms equ rsp+64+32+8

	mov			r10d, [parms+40]			;r10d = w

	pxor		xmm5, xmm5
	movdqa		xmm4, oword ptr [roundval]
	xor			rbx, rbx					;rbx = source offset

	cmp			r9d, 4
	jz			accel_4coeff2
	cmp			r9d, 6
	jz			accel_6coeff2

	shr			r9d, 1						;r9d = filter pair count

pixelloop:
	mov			rax, rdx					;rax = row pointer table
	mov			rdi, r8						;rdi = filter
	mov			r11d, r9d					;r11d = filter width counter
	movdqa		xmm2, xmm4
coeffloop:
	mov			rsi, [rax]

	movd		xmm0, dword ptr [rsi+rbx]

	mov			rsi, [rax+8]
	add			rax, 16

	movd		xmm1, dword ptr [rsi+rbx]
	punpcklbw	xmm0, xmm1

	punpcklbw	xmm0, xmm5

	movq		xmm1, qword ptr [rdi]
	pshufd		xmm1, xmm1, 01000100b

	pmaddwd		xmm0, xmm1

	paddd		xmm2, xmm0

	add			rdi,8

	sub			r11d,1
	jne			coeffloop

	psrad		xmm2,14
	packssdw	xmm2,xmm2
	add			rbx,4
	packuswb	xmm2,xmm2

	movd		dword ptr [rcx],xmm2
	add			rcx,4
	sub			r10d,1
	jne			pixelloop

xit2:
	movdqa	xmm6, oword ptr [rsp]
	movdqa	xmm7, oword ptr [rsp+16]
	add rsp, 32+8

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rdi
	pop rsi
	pop rbx


	ret

accel_4coeff2:
	mov			r12, [rdx]
	mov			r13, [rdx+8]
	mov			r14, [rdx+16]
	mov			r15, [rdx+24]
	movq		xmm8, qword ptr [r8]
	punpcklqdq	xmm8, xmm8
	movq		xmm9, qword ptr [r8+8]
	punpcklqdq	xmm9, xmm9

	sub			r10d, 1
	jc			oddpixel_4coeff
pixelloop_4coeff2:
	movq		xmm0, qword ptr [r12+rbx]
	movq		xmm1, qword ptr [r13+rbx]
	movq		xmm2, qword ptr [r14+rbx]
	movq		xmm3, qword ptr [r15+rbx]

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

	movq		qword ptr [rcx], xmm0
	add			rcx, 8
	add			rbx, 8
	sub			r10d, 2
	ja			pixelloop_4coeff2
	jnz			xit2
oddpixel_4coeff:
	movd		xmm0, dword ptr [r12+rbx]
	movd		xmm1, dword ptr [r13+rbx]
	movd		xmm2, dword ptr [r14+rbx]
	movd		xmm3, dword ptr [r15+rbx]

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

	movd		dword ptr [rcx], xmm0

	jmp			xit2

accel_6coeff2:
	mov			r12, [rdx]
	mov			r13, [rdx+8]
	mov			r14, [rdx+16]
	mov			r15, [rdx+24]
	mov			rsi, [rdx+32]
	mov			rdx, [rdx+40]
	movq		xmm10, qword ptr [r8]
	punpcklqdq	xmm10, xmm10
	movq		xmm11, qword ptr [r8+8]
	punpcklqdq	xmm11, xmm11
	movq		xmm12, qword ptr [r8+16]
	punpcklqdq	xmm12, xmm12

	sub			r10d, 1
	jc			oddpixel_6coeff
pixelloop_6coeff2:
	movq		xmm0, qword ptr [r12+rbx]
	movq		xmm1, qword ptr [r13+rbx]
	movq		xmm2, qword ptr [r14+rbx]
	movq		xmm3, qword ptr [r15+rbx]
	movq		xmm8, qword ptr [rsi+rbx]
	movq		xmm9, qword ptr [rdx+rbx]

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

	movq		qword ptr [rcx], xmm0
	add			rcx, 8
	add			rbx, 8
	sub			r10d, 2
	ja			pixelloop_6coeff2
	jnz			xit2
oddpixel_6coeff:
	movd		xmm0, dword ptr [r12+rbx]
	movd		xmm1, dword ptr [r13+rbx]
	movd		xmm2, dword ptr [r14+rbx]
	movd		xmm3, dword ptr [r15+rbx]
	movd		xmm8, dword ptr [rsi+rbx]
	movd		xmm9, dword ptr [rdx+rbx]

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

	movd		dword ptr [rcx], xmm0

	jmp			xit2

	end
