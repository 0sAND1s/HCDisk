;HL = column ordered SCREEN$ address
ScrDraw:		
		ld		de, 16384
		ld		bc, 6144	
FillScrLoop:	
		ldi
		dec		de

;Return the next char line down
ScrNext:
		;next line in char cell
		inc d
		ld a,d
		and 7
		jr nz, ScrNextEnd

		;next cell below
		ld a,e
		add a,32
		ld e,a
		jr nc, ScrNextPart

		;did we finish a column?
		ld a,$58
		cp d
		jr nz, ScrNextEnd

		inc e		;move to next column
		ld a,d		;must return to the first line
		sub 16		;return 2 parts above
		ld d,a

ScrNextPart:
		;next part
		ld a,d		;return 1 part above
		sub 8
		ld d,a		
ScrNextEnd:
		
		ld		a, b
		or		c	
		jr		nz, FillScrLoop	
		
		ld		de, 16384 + 6144
		ld		bc, 768
		ldir		
		ret