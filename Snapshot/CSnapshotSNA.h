//Specs: https://sinclair.wiki.zxnet.co.uk/wiki/SNA_format
#pragma once

#include "..\types.h"
#include "..\FileConverters\Screen.h"

class CSnapshotSNA
{
public:
#pragma pack(1)
	struct SNAHeader
	{
		byte I;

		word HLp;
		word DEp;
		word BCp;
		word AFp;

		word HL;
		word DE;
		word BC;

		word IY;
		word IX;

		byte IFF2; //[Only bit 2 is defined: 1 for EI, 0 for DI]
		byte R;

		word AF;
		word SP;

		byte IM;	//Interrupt mode: 0, 1 or 2
		byte BorderClr;
	} SNAHdr;

	struct
	{				
		ScreenType SNASCR;
		byte SNANonSCR[48 * 1024 - sizeof(ScreenType)];		
	} SNAMemory;	
#pragma pack()

	bool CSnapshotSNA::Read(const char* path);
	bool CSnapshotSNA::Write(const char* path);

protected:
	bool IsOpened = false;
};

