; Copyright (C) 2016 Doubango Telecom <https://www.doubango.org>
;
; This file is part of Open Source ComputerVision (a.k.a CompV) project.
; Source code hosted at https://github.com/DoubangoTelecom/compv
; Website hosted at http://compv.org
;
; CompV is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; CompV is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with CompV.
;
%include "compv_common_x86.s"

COMPV_YASM_DEFAULT_REL

global sym(Convlt1_hz_float32_minpack4_Asm_X86_SSE2)

section .data

section .text

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This function requires sizeof(float) = 4byte = 32bits
; arg(0) -> const uint8_t* in_ptr
; arg(1) -> uint8_t* out_ptr
; arg(2) -> compv_scalar_t width
; arg(3) -> compv_scalar_t height
; arg(4) -> compv_scalar_t pad
; arg(5) -> const float* hkern_ptr
; arg(6) -> compv_scalar_t kern_size
; void Convlt1_hz_float32_minpack4_Asm_X86_SSE2(const uint8_t* in_ptr, uint8_t* out_ptr, compv_scalar_t width, compv_scalar_t height, compv_scalar_t pad, const float* hkern_ptr, compv_scalar_t kern_size)
sym(Convlt1_hz_float32_minpack4_Asm_X86_SSE2):
	push rbp
	mov rbp, rsp
	COMPV_YASM_SHADOW_ARGS_TO_STACK 7
	COMPV_YASM_SAVE_XMM 7 ;XMM[6-7]
	push rsi
	push rdi
	push rbx
	;; end prolog ;;

	%define COMPV_SIZE_OF_FLOAT 4 ; up to the caller to make sure sizeof(float)=4
	%define i_xmmSF0	rsp + 0
	%define i_xmmSF1	rsp + 16
	%define i_xmmSF2	rsp + 32
	%define i_xmmSF3	rsp + 48

	; align stack and alloc memory
	COMPV_YASM_ALIGN_STACK 16, rax
	sub rsp, 16*1 + 16*1 + 16*1 + 16*1
	; [rsp + 0] = xmmSF0
	; [rsp + 16] = xmmSF1
	; [rsp + 32] = xmmSF2
	; [rsp + 48] = xmmSF3

	; i = rdi
	; xor rdi, rdi

	; rcx = col

	; rbx = max

	; j = rsi
	xor rsi, rsi

	; xmm7 = xmmZero
	pxor xmm7, xmm7

	; arg(4) = pad += (width & 3)
	mov rdx, arg(2) ; width
	mov rax, arg(4) ; pad
	and rdx, 3
	add rax, rdx
	mov arg(4), rax
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	; for (j = 0; j < height; ++j)
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.LoopRows
		xor rdi, rdi ; i = 0
		mov rbx, arg(2) ; width
		sub rbx, 15 ; rbx = (width - 15)
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		; for (i = 0; i < width - 15; i += 16)
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		.LoopColumns16
			movaps [i_xmmSF0], xmm7
			movaps [i_xmmSF1], xmm7
			movaps [i_xmmSF2], xmm7
			movaps [i_xmmSF3], xmm7

			xor rcx, rcx ; col = 0
			mov rax, arg(0) ; in_ptr
			mov rdx, arg(5) ; hkern_ptr
			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			; for (col = 0; col < kern_size; ++col)
			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			.LoopColumns16Kern16
				movdqu xmm0, [rax + rcx] ; xmm0 = xmmI0
				movss xmm1, [rdx + rcx*COMPV_SIZE_OF_FLOAT]
				shufps xmm1, xmm1, 0x0 ; xmm1 = xmmCoeff

				movdqa xmm2, xmm0
				movdqa xmm3, xmm0
				punpcklbw xmm2, xmm7 ; xmm2 = xmmI1
				punpcklbw xmm3, xmm7 ; xmm3 = xmmI1

				movaps xmm4, [i_xmmSF0]
				punpcklwd xmm2, xmm7
				cvtdq2ps xmm2, xmm2
				mulps xmm2, xmm1 ; xmm2 = xmmF0
				addps xmm4, xmm2 ; xmm4 = xmmSF0
				movaps [i_xmmSF0], xmm4

				movaps xmm4, [i_xmmSF1]
				punpckhwd xmm3, xmm7
				cvtdq2ps xmm3, xmm3
				mulps xmm3, xmm1 ; xmm3 = xmmF0
				addps xmm4, xmm3 ; xmm4 = xmmSF1
				movaps [i_xmmSF1], xmm4

				movdqa xmm2, xmm0
				punpckhbw xmm0, xmm7 ; xmm0 = xmmI1
				punpckhbw xmm2, xmm7 ; xmm2 = xmmI1

				movaps xmm4, [i_xmmSF2]
				punpcklwd xmm0, xmm7
				cvtdq2ps xmm0, xmm0
				mulps xmm0, xmm1 ; xmm0 = xmmF0
				addps xmm4, xmm0 ; xmm4 = xmmSF0
				movaps [i_xmmSF2], xmm4

				movaps xmm4, [i_xmmSF3]
				punpckhwd xmm2, xmm7
				cvtdq2ps xmm2, xmm2
				mulps xmm2, xmm1 ; xmm2 = xmmF0
				addps xmm4, xmm2 ; xmm4 = xmmSF1
				movaps [i_xmmSF3], xmm4

			inc rcx
			cmp rcx, arg(6)
			jl .LoopColumns16Kern16		

			mov rax, arg(1) ; out_ptr
			mov rdx, arg(0) ; in_ptr
			movaps xmm0, [i_xmmSF0]
			movaps xmm1, [i_xmmSF1]
			movaps xmm2, [i_xmmSF2]
			movaps xmm3, [i_xmmSF3]
			cvtps2dq xmm0, xmm0
			cvtps2dq xmm1, xmm1
			cvtps2dq xmm2, xmm2
			cvtps2dq xmm3, xmm3
			packssdw xmm0, xmm1
			packssdw xmm2, xmm3
			packuswb xmm0, xmm2
			movdqu [rax], xmm0
			add rax, 16
			add rdx, 16
			mov arg(1), rax ; out_ptr += 16
			mov arg(0), rdx ; in_ptr += 16

		add rdi, 16 ; i += 16
		cmp rdi, rbx
		jl .LoopColumns16

		mov rbx, arg(2) ; width
		sub rbx, 3 ; rbx = (width - 3)
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		; for (; i < width - 3; i += 4)
		;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		.LoopColumns4
			xorps xmm4, xmm4 ; xmm4 = xmmSF0

			xor rcx, rcx ; col = 0
			mov rax, arg(0) ; in_ptr
			mov rdx, arg(5) ; hkern_ptr
			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			; for (col = 0; col < kern_size; ++col)
			;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
			.LoopColumns4Kern16
				movd xmm0, [rax + rcx] ; xmm0 = xmmI0
				movss xmm1, [rdx + rcx*COMPV_SIZE_OF_FLOAT]
				shufps xmm1, xmm1, 0x0 ; xmm1 = xmmCoeff

				punpcklbw xmm0, xmm7
				punpcklwd xmm0, xmm7
				cvtdq2ps xmm0, xmm0
				mulps xmm0, xmm1
				addps xmm4, xmm0

			inc rcx
			cmp rcx, arg(6)
			jl .LoopColumns4Kern16

			mov rax, arg(1) ; out_ptr

			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			movd rdx, xmm4
			mov [rax], dword edx
			
			mov rdx, arg(0) ; in_ptr
			add rax, 4
			add rdx, 4
			mov arg(1), rax ; out_ptr += 4
			mov arg(0), rdx ; in_ptr += 4

		add rdi, 4 ; i+= 4
		cmp rdi, rbx
		jl .LoopColumns4

		mov rax, arg(1) ; out_ptr
		mov rdx, arg(0) ; in_ptr
		add rax, arg(4)
		add rdx, arg(4)
		mov arg(1), rax ; out_ptr += pad
		mov arg(0), rdx ; in_ptr += pad

	inc rsi ; ++j
	cmp rsi, arg(3)
	jl .LoopRows

	; unalign stack and free memory
	add rsp, 16*1 + 16*1 + 16*1 + 16*1
	COMPV_YASM_UNALIGN_STACK

	%undef COMPV_SIZE_OF_FLOAT
	%undef i_xmmSF0
	%undef i_xmmSF1
	%undef i_xmmSF2
	%undef i_xmmSF3

	;; begin epilog ;;
	pop rbx
	pop rdi
	pop rsi
	COMPV_YASM_RESTORE_XMM
	COMPV_YASM_UNSHADOW_ARGS
	mov rsp, rbp
	pop rbp
	ret