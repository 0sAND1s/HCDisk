//Z80 snapshot loader. Specs https://worldofspectrum.net/zx-modules/fileformats/z80format.html .

#pragma once

#include "CSnapshotSNA.h"
#include <stdio.h>

class CSnapshotZ80 : public CSnapshotSNA
{
public:

#pragma pack(1)
	struct Z80MainHeaderType
	{
		byte A;
		byte F;
		word BC;
		word HL;
		word PC;	//value 0 indicates version 2 or later
		word SP;
		byte I;
		byte R;		// bit 7 is not significant

		//if the byte = 255, it has to be interpreted as being = 1
		struct Flags1Type
		{
			bool RBit7 : 1;
			byte BorderClr : 3;
			bool SamRomPagedIn : 1;
			bool IsDataCompressed : 1;
			byte NotUsed : 2;
		} Flags1;

		word DE;

		word BCp;
		word DEp;
		word HLp;		
		byte Ap;
		byte Fp;
		word IY;
		word IX;

		byte IFF1;	//interrupt flipflop, (0=di, <>0 =ei)
		byte IFF2;	//interrupt 2 flipflop (0=di, <>0 =ei)

		struct Flags2Type
		{
			byte IM : 2;	//Interrupt mode: 0, 1 or 2
			bool Issue2Emulation : 1;
			bool InterruptFreq : 1;
			byte VideoSync : 2;
			byte JoystickType : 2;

		} Flags2;
		
	} Z80MainHdr;


	struct Z80AditionalHeaderType
	{
		word PC;
		byte HWMode;
		byte States1;
		byte States2;
		struct FlagsType
		{
			bool RRegEmulation : 1;
			bool LDIREmulation : 1;
			byte Unused : 5;
			bool HWModifications : 1;
		} Flags;
		byte LastOutFFDDh;
		byte SoundChipRegisters[16];
	} Z80AditionalHdr;

	struct Z80AditionalHeaderV3Type
	{
		word LowTStateCounter;
		byte HighTStateCounter;
		byte Ignored;
		byte MGTPaged;
		byte MultifacePaged;
		byte First8KPaged;
		byte Second8KPaged;
		byte KeyboardMappings1[10];
		byte KeyboardMappings2[10];
		byte MGTType;
		byte DiscipleInhibator1;
		byte DiscipleInhibator2;
	} Z80AditionalHdrV3;

	//For v1 only: the compressed block is terminated by the end marker 00h, EDh, EDh, 00h.
	struct Z80DataBlockHeader
	{
		//length of datablock value 65535 means "uncompressed"
		//uncompressed block size=16384 bytes
		word Lenght;

		//48k:	
		//4 = (8000h..BFFFh),
		//5 = (C000h..FFFFh),
		//8 = (4000h..7FFFh)
		byte PageNo;
	};
#pragma pack()

	//Reads the Z80 format and converts the data in the SNA format.
	virtual bool Read(const char* path);
	virtual bool Write(const char* path);

protected:
	byte Z80SnapVersion = 0;
	//Will store the uncompressed data into the SNA memory bufer.
	bool UncompressZ80Data(byte* srcBuffer, const word srcLen, const byte pageNo = 0);
	bool Is48KZ80Snapshot();
	void Z802SNAHeader();
	bool ReadHeaders(FILE* fZ80);
	bool ProcessBlocks(FILE* fZ80);
};