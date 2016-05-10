	.586
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

    end


