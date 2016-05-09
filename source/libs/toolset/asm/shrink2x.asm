
IFNDEF MODE64

	.586
;	.mmx
	.model	flat

	.code

	public	_asm_shrink2x

;asm_shrink2x(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcpitch
;	[esp+24] ulong dstcorrectedpitch);

_asm_shrink2x:

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx ; +24

    mov edi, [esp+ 4+24] ; dst
    mov esi, [esp+ 8+24] ; src
    mov ebx, [esp+20+24] ; src pitch
    mov eax, [esp+16+24] ; height
loopy :
    push eax ; +28
    mov eax, [esp+12+28] ; width
loopx :
    push eax ; +32

    mov ebp, [esi]
    mov ecx, ebp
    and ebp, 00FF00FFh
    and ecx, 0FF00FF00h

    mov eax, [esi + 4]
    mov edx, eax
    and eax, 00FF00FFh
    and edx, 0FF00FF00h
    add ebp, eax
    add ecx, edx

    mov eax, [esi + ebx]
    adc ecx, 0
    mov edx, eax
    and eax, 00FF00FFh
    and edx, 0FF00FF00h
    add ebp, eax
    add ecx, edx

    mov eax, [esi + ebx + 4]
    adc ecx, 0
    mov edx, eax
    and eax, 00FF00FFh
    and edx, 0FF00FF00h
    add eax, ebp
    add edx, ecx
    adc edx, 0

    ror eax, 2
    ror edx, 2
    and eax, 00FF00FFh
    and edx, 0FF00FF00h
    or  eax, edx

    mov[edi], eax

    add esi, 8
    add edi, 4

    pop eax ; +28
    dec eax
    jnz loopx

    mov eax, [esp+12+28] ; width
    lea esi, [esi+ebx*2]
    shl eax,3 ; mul by 8 (4 bytes per pixel and 2x size)
    add edi, [esp+24+28] ; dstcorrectedpitch
    sub esi, eax

    pop eax
    dec eax
    jnz loopy

	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

    ret

ENDIF

IFDEF MODE64

	.code

	public	asm_shrink2x

asm_shrink2x:

	push	r12
	push	r13
	push	r14
	push	r15
	push	rdi
	push	rsi
	push	rbx

reg_src equ rdx
reg_dst equ rcx
reg_srcpitch equ r10
reg_srccpitch equ r15
reg_dstcpitch equ r11
reg_width equ r8
reg_height equ r9

reg_mask1 equ r13
reg_mask2 equ r14

reg_loop_x equ r12

reg_temp1 equ rax
reg_temp1d equ eax
reg_temp2 equ rbx
reg_temp3 equ rsi
reg_temp4 equ rdi

	mov reg_mask1, 00FF00FF00FF00FFh
	mov reg_mask2, 0FF00FF00FF00FF00h

    movsxd reg_srcpitch, dword ptr [rsp+96] ; srcpitch
    movsxd reg_dstcpitch, dword ptr [rsp+104] ; dstcorrpitch

	lea reg_srccpitch,[reg_width * 8]
	neg reg_srccpitch
	lea reg_srccpitch,[reg_srccpitch + reg_srcpitch * 2]

loopy :
    mov reg_loop_x, reg_width
loopx :

    mov reg_temp1, [reg_src]
    mov reg_temp2, reg_temp1

    and reg_temp1, reg_mask1 ;   0FF00FF00FF00FFh
    and reg_temp2, reg_mask2 ; 0FF00FF00FF00FF00h

    mov reg_temp3, [reg_src + reg_srcpitch]
    mov reg_temp4, reg_temp3
    and reg_temp3, reg_mask1
    and reg_temp4, reg_mask2
    add reg_temp3, reg_temp1
    add reg_temp4, reg_temp2 ; need carry flag

	mov reg_temp1, reg_temp3
	mov reg_temp2, reg_temp4

	rcr reg_temp4, 1
	shr reg_temp3, 32
	shr reg_temp4, 31

	lea reg_temp1, [reg_temp1 + reg_temp3] ;add reg_temp1, reg_temp3
	lea reg_temp2, [reg_temp2 + reg_temp4] ;add reg_temp2, reg_temp4

	shr reg_temp1, 2
	shr reg_temp2, 2

	and reg_temp1, reg_mask1
    and reg_temp2, reg_mask2
    
	lea reg_temp1, [ reg_temp1 + reg_temp2 ] ;or  reg_temp1, reg_temp2

    mov [reg_dst], reg_temp1d

    lea reg_src, [reg_src + 8]
    lea reg_dst, [reg_dst + 4]

    dec reg_loop_x
    jnz loopx

    lea reg_src, [reg_src+reg_srccpitch]
    lea reg_dst, [reg_dst+reg_dstcpitch]

    dec reg_height
    jnz loopy

	pop	rbx
	pop	rsi
	pop	rdi
	pop	r15
	pop	r14
	pop	r13
	pop	r12

    ret

ENDIF

    end


