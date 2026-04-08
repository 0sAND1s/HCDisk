	DEVICE ZXSPECTRUM48
	
Loader		EQU 16384
ScrDelta	EQU	6

	ORG	Loader - (StartFixed-Start)

Start:	
	di
	ld		sp, 0
	ld		hl, StartFixed-Start
	add		hl, bc
	ld		de, StartFixed
	ld		bc, End-StartFixed
	ldir

	push	hl				;HL = start of main compressed block
		ld		de, $5B00-1-ScrDelta
		ld		bc, (PackedLenTotal)
		add		hl, bc
		dec		hl
		call	Decompress ;DE = unpacked screen start
		jp		UnpackMain
	
StartFixed:	
Decompress:
	include "dzx0_standard_back.asm"	

UnpackMain:	
	;Move main block lower in RAM to avoid overwriting when unpacking.	
	pop		hl					;HL = start of main compressed block
	ld		bc, (PackedLenMain)
	ld		de, $5B00-ScrDelta			;Max delta of 6 should be enough.
	ld		sp, Stack			
	push	de
		ldir
		ex		de, hl
		dec		hl
		ld		de, $FFFF
		call	Decompress
	pop	de
	ld	hl, (GapAddr)
	ld	bc, ScrDelta
	ldir
	;ld	(GapAddr), hl					;Save start of screen for later.

;-----------------------------------------------------------------------------

; 0s&1s: Snapshot state restore implementation is based on the implementation below.
; Snapshot loader for the Spectrum +3
; Copyright 2005 John Elliott <jce@seasip.demon.co.uk>
	
	ld	a, (InstrDIEI)
	ld	(finalDIEI),a	;Stored interrupt state

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

	pop		af
	ld		sp, (REG_SP)
finalDIEI:
	nop				;Is set to EI or DI by stage 2 loader.

	;move relocatable code to stack and restore screen
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
	ld		b, ScrDelta
RestoreGapScr:
	ld		(hl), c
	inc		hl
	djnz	RestoreGapScr

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
	retn			;Retrieves snapshot PC.
FinalStageEnd:

PackedLenTotal	DW 0
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

STACK_ENTRIES EQU 3
	BLOCK STACK_ENTRIES * 2
Stack:

End: