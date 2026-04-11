	DEVICE ZXSPECTRUM48
	
Loader			EQU 16384
ScrDelta		EQU	5
StackEntries	EQU 5

	ORG	Loader - (StartFixed-Start)

Start:	
	;Clear screen to hide our code.
	push	bc
		ld		hl, 16384+6144
		ld		d, h
		ld		e, l
		inc		de
		xor		a
		ld		(hl), a
		ld		bc, 767
		ldir
		out		($fe), a
	pop		bc
	push	bc
		push	bc
			xor		a
			call	$1601				
		pop		bc		
		ld		hl, Msg - Start
		add		hl, bc	
		ld		b, MsgLen
PrintMsgLoop:	
		ld		a, (hl)
		rst		$10
		inc		hl
		halt
		djnz	PrintMsgLoop			
	pop		bc		
	jr		Msg + MsgLen
Msg	defb	13, 16, 0, 17, 2, " -=SNAP2TAP/HCDisk/0s&1s/2026=- "	
MsgLen equ	$ - Msg

	di
	ld		sp, Stack

	;Copy fixed code to start of screen.
	ld		hl, StartFixed-Start
	add		hl, bc				;BC = randomize usr address from BASIC
	ld		de, StartFixed
	ld		bc, End-StartFixed
	ldir

	;HL = start of screen compressed block
	ld		bc, (PackedLenScr)
	ldir
	dec		de
	push	de			;Save end of compressed screen
		jp		Unpack
	
StartFixed:	
Decompress:
	include "dzx0_standard_back.asm"	

Unpack:	
		;Move main block lower in RAM to avoid overwriting when unpacking.	
		;HL = start of main compressed block
		ld		bc, (PackedLenMain)
		ld		de, $5B00		;Start main block after screen area, to avoid visible artefacts during decompression.
		ldir
		ex		de, hl
		dec		hl
		ld		de, $FFFF
		call	Decompress

	;Unpack screen
	pop		hl
	ld		de, $5B00 + 5		;Include main block delta.
	call	Decompress

;-----------------------------------------------------------------------------

; 0s&1s: Snapshot state restore implementation is based on the implementation specified below.
; Snapshot loader for the Spectrum +3
; Copyright 2005 John Elliott <jce@seasip.demon.co.uk>
	

;restore registers
; Load the registers from the SNA header. Do this by pointing SP at HL' and
; popping them off.

	ld	sp, REG_HL1
	pop	hl
	pop	de
	pop	bc
	pop	af
	ex	af,af'
	exx			;Alternates done
	pop	hl
	pop	de
	pop	bc		
	pop	iy
	pop	ix
	pop	af		;IFF and R ignore.
	
	ld	a, (REG_BDR)
	out	($FE), a

	ld	a, (InstrIM)
	ld	(SetIM), a
SetIM EQU $+1	
	im	0

	ld	a,(REG_I)	;Retrieve I and R
	ld	i,a
	ld	a,(REG_R)
	ld	r,a	

	ld	a, (InstrDIEI)
	ld	(finalDIEI),a	;Stored interrupt state

	pop		af
	ld		sp, (REG_SP)

;---------------------------------------------------------------------------

	;Move final stage code to game stack and restore screen part used by code.
	push	hl
	push	de
	push	bc
	push	af

	ld		hl, (REG_SP)
	ld		bc, FinalStageStart - FinalStageEnd - 5*2
	add		hl, bc
	ex		de, hl
	ld		hl, FinalStageStart
	push	de
	ld		bc, FinalStageEnd - FinalStageStart
	ldir
	ret

FinalStageStart:
	ld		hl, (GapAddr)
	ld		a, (GapValue)
	ld		c, a
	ld		de, 16384
	ld		a, (GapLen)
	ld		b, a

RestoreGap:
	ld		a, (hl)
	ld		(de), a
	ld		(hl), c
	inc		hl
	inc		de
	djnz	RestoreGap

	pop		af
	pop		bc
	pop		de
	pop		hl

finalDIEI:
	nop				;Is set to EI or DI by stage 2 loader.
	retn			;Retrieves snapshot PC.
FinalStageEnd:

;---------------------------------------------------------------------

PackedLenScr	DW 0
PackedLenMain	DW 0
GapAddr			DW 0
GapLen			DB 0
GapValue		DB 0
InstrIM			DB 0
InstrDIEI		DB 0

REG_I		DB 0
REG_HL1		DW 0
REG_DE1		DW 0
REG_BC1		DW 0
REG_AF1		DW 0
REG_HL		DW 0
REG_DE		DW 0
REG_BC		DW 0
REG_IY		DW 0
REG_IX		DW 0
REG_IFF		DB 0
REG_R		DB 0
REG_AF		DW 0
REG_SP		DW 0
REG_IM		DB 0
REG_BDR		DB 0

	BLOCK StackEntries * 2
Stack:

End: