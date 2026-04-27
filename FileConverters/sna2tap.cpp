//Restore code must be less than 257 bytes, since most games have the interrupt table buffer of 257 used as gap buffer. Reducing to 255 bytes, to keep the lenght on a single byte and siplify code.

#include <string>
#include <iostream>

#include "sna2tap.h"
#include "../Snapshot/CSnapshotSNA.h"
#include "../Snapshot/CSnapshotZ80.h"
#include "../Compression/Compression.h"
#include "../FileSystem/CFileArchiveTape.h"
#include "../Utils/FileUtil.h"
#include "../Utils/BASICUtil.h"
#pragma warning(push)
#pragma warning(disable: 4838)
#pragma warning(disable: 4309)
#include "sna2tapldr.h"
#pragma warning(pop)

using namespace std;

SNAP2TAP::SNAP2TAP()
{

}

//Find a memory gap of identical values with minLenReq lenght
word SNAP2TAP::FindMemGap(byte* mem, word memLen, const word minLenReq)
{
	word idx = 1;
	word gapCnt = 1;
	do
	{
		while (mem[idx] == mem[idx - 1] && ++idx < memLen && ++gapCnt < minLenReq)
			;

		if (gapCnt >= minLenReq)
		{
			break;
		}
		else
		{
			gapCnt = 1;
		}

	} while (idx++ < memLen);

	return gapCnt > 1 ? idx - gapCnt : 0xFFFF;
}


bool SNAP2TAP::Convert(string nameSnap, string nameTAP)
{
#pragma pack(1)
	const byte stackEntries = 5;
	struct LoaderParams
	{
		word compressedLenScr;
		word compressedLenMain;
		word gapAddr;
		byte gapLen;
		byte gapValue;
		byte instrIM;
		byte instrDIEI;
		CSnapshotSNA::SNAHeader SNAHdr;
		byte stack[stackEntries * 2];
	};
#pragma pack()

	CSnapshotSNA sna;
	string ext = FileUtil::GetExtension(nameSnap);		

	if (stricmp(ext.c_str(), "SNA") == 0)
	{
		if (!sna.Read(nameSnap.c_str()))
		{
			cout << "Couln't open '" << nameSnap << "' as 48K SNA snapshot." << endl;
			return false;
		}
	}
	else if (stricmp(ext.c_str(), "Z80") == 0)
	{
		CSnapshotZ80 z80;
		if (!z80.Read(nameSnap.c_str()))
		{
			cout << "Couln't open '" << nameSnap << "' as 48K Z80 snapshot." << endl;
			return false;
		}

		sna = z80;
	}
	else
	{
		return false;
	}
	

	const byte ldrPrefix = 103; //Loader has a routine that moves the code to fixed address and it's not needed anymore.
	const byte scrDelta = 5; //Exclude x bytes of screen for screen compression delta.
	const byte mainDelta = 6;
	const word ldrSize = sizeof(sna2tapldr) - ldrPrefix;
	const word gapLen = ldrSize + scrDelta; //Include screen delta in gap, as uncompressed data.
	word gapAddr = 0;
	byte gapValue = 0;

	//Exclude first part that moves the code, but include the reserved stack.
	word gapOffset = FindMemGap(sna.SNAMemory.SNAMainMem, sizeof(sna.SNAMemory.SNAMainMem), gapLen);
	if (gapOffset != 0xFFFF)
	{
		gapAddr = 16384 + sizeof(sna.SNAMemory.SNASCR) + gapOffset;
		gapValue = sna.SNAMemory.SNAMainMem[gapOffset];
		cout << "Found memory gap at address: " << gapAddr << " ($" << hex << gapAddr << ") with required lenght " << dec << gapLen << " and value $" << hex << (unsigned)gapValue << dec << "." << endl;
	}
	else
	{
		cout << "Couldn't find memory gap of lenght " << gapLen << "." << endl;
		return false;
	}

	bool quickMode = false;
#ifdef _DEBUG
	quickMode = true;
#endif


	//Compress screen and main block.
	Compression comp;
	byte* scrCompressed = nullptr;
	int delta = 0;
	const bool compressBackwards = true;

	//Compress screen without first part, which will be used for code. Include screen compression delta and main block delta.
	word lenOutScr = comp.Compress(sna.SNAMemory.SNASCR + gapLen, sizeof(sna.SNAMemory.SNASCR) - gapLen + mainDelta, &scrCompressed, compressBackwards, quickMode, &delta);
	cout << "Screen compressed to " << lenOutScr << ", delta " << delta << "." << endl;

	//Copy first part of screen$ to main block gap, to restore later.
	memcpy(sna.SNAMemory.SNAMainMem + gapOffset, sna.SNAMemory.SNASCR, gapLen);

	byte* mainCompressed = nullptr;
	word lenOutMain = comp.Compress(sna.SNAMemory.SNAMainMem + mainDelta, sizeof(sna.SNAMemory.SNAMainMem) - mainDelta, &mainCompressed, compressBackwards, quickMode, &delta);
	cout << "Main block compressed to " << lenOutMain << ", delta " << delta << "." << endl;
	
	//Copy the loader AND prefix code from original buffer, as the definition of byte array is read only.
	byte* Snap2TapLoader = new byte[sizeof(sna2tapldr)];
	memcpy(Snap2TapLoader, sna2tapldr, sizeof(sna2tapldr));

	LoaderParams* ldrPrm = (LoaderParams*)&Snap2TapLoader[sizeof(sna2tapldr) - sizeof(LoaderParams)];
	ldrPrm->compressedLenScr = lenOutScr;
	ldrPrm->compressedLenMain = lenOutMain;
	ldrPrm->gapAddr = gapAddr;
	ldrPrm->gapLen = (byte)gapLen;
	ldrPrm->gapValue = gapValue;
	ldrPrm->instrDIEI = ((sna.SNAHdr.IFF2 & 4) == 4 ? 0xFB /*EI*/ : 0xF3 /*DI*/);
	switch (sna.SNAHdr.IM)
	{
	case 0:
		ldrPrm->instrIM = 0x46; //IM0
		break;
	case 1:
		ldrPrm->instrIM = 0x56; //IM1
		break;
	case 2:
		ldrPrm->instrIM = 0x5E; //IM2
	}
	ldrPrm->SNAHdr = sna.SNAHdr;


	//Create final blob with loader+main block+screen.
	const char* blobName = "snap2tap.bin";
	FILE* fblob = fopen(blobName, "wb");
	fwrite(Snap2TapLoader, 1, sizeof(sna2tapldr), fblob);
	fwrite(scrCompressed, 1, lenOutScr, fblob);
	fwrite(mainCompressed, 1, lenOutMain, fblob);
	fclose(fblob);
	free(scrCompressed);
	free(mainCompressed);
	delete[] Snap2TapLoader;
	
	//Write into TAP.
	CFileArchiveTape tape((char*)nameTAP.c_str());
	bool tapeOpened = tape.Open((char*)nameTAP.c_str(), true);
	bool bin2basRes = BASICUtil::Bin2Var(&tape, blobName, 0, FileUtil::GetNameWithoutExtension(nameTAP));
	remove(blobName);

	return lenOutScr > 0 && lenOutMain > 0 && tapeOpened && bin2basRes;
}