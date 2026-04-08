//TODO:
//-restore last 6 bytes of screen overwritten because of compressor gap
//-restore first part of screen where the restore code resides using the gap buffer; where to move the running code? in the stack?
//Restore code must be less than 257 bytes, since most games have the interrupt table buffer of 257 used as gap buffer.

#include <string>
#include <iostream>

#include "sna2tap.h"
#include "Snapshot/CSnapshotSNA.h"
#include "Compression/Compression.h"
#include "CFileArchiveTape.h"
#include "FileUtil.h"
#include "BASICUtil.h"
#include "FileConverters\sna2tapldr.h"

using namespace std;

SNA2Tap::SNA2Tap()
{

}

//Find a memory gap of identical values with minLenReq lenght
word SNA2Tap::FindMemGap(byte* mem, word memLen, const word minLenReq)
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

bool SNA2Tap::Convert(string nameSNA, string nameTAP)
{
#pragma pack(1)
	const byte stackEntries = 3;
	struct LoaderParams
	{
		word compressedLenTotal;
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
	if (!sna.Read(nameSNA.c_str()))
	{
		cout << "Couln't open '" << nameSNA << "' as 48K SNA snapshot." << endl;
		return false;
	}

	const byte ldrPrefix = 32;
	const word ldrSize = sizeof(sna2tapldr) - ldrPrefix;
	word gapAddr = 0;	
	byte gapValue = 0;
	word gapLen = ldrSize;
	word gapOffset = FindMemGap(sna.SNAMemory.SNAMainMem, sizeof(sna.SNAMemory.SNAMainMem), gapLen);//exclude first part that moves the code, but include the reserved stack
	if (gapOffset != 0xFFFF)
	{
		gapAddr = 16384 + sizeof(sna.SNAMemory.SNASCR) + gapOffset;
		gapValue = sna.SNAMemory.SNAMainMem[gapOffset];
		cout << "Found memory gap at address: " << gapAddr << " ($" << hex << gapAddr << ") with required lenght " << dec << gapLen << " and value $" << hex << (unsigned)gapValue << dec << "." << endl;
	}
	else
	{
		cout << "Couldn't find memory gap of lenght " << ldrSize << "." << endl;
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

	//Copy last 6 bytes of screen.
	const byte deltaScr = 6;
	memcpy(sna.SNAMemory.SNAMainMem + gapOffset, sna.SNAMemory.SNASCR + sizeof(sna.SNAMemory.SNASCR) - deltaScr, deltaScr);

	//Save screen without first part, which will be used for restore code.
	word lenOutScr = comp.Compress(sna.SNAMemory.SNASCR + gapLen, sizeof(sna.SNAMemory.SNASCR) - gapLen - deltaScr, &scrCompressed, compressBackwards, quickMode, &delta);
	cout << "Screen compressed " << (compressBackwards ? "backwards" : "forward") << " to " << lenOutScr << ", delta " << delta << "." << endl;

	//Copy first part of screen$ to main block gap, to restore later.
	memcpy(sna.SNAMemory.SNAMainMem + gapOffset+ deltaScr, sna.SNAMemory.SNASCR, gapLen);

	byte* mainCompressed = nullptr;
	word lenOutMain = comp.Compress(sna.SNAMemory.SNAMainMem, sizeof(sna.SNAMemory.SNAMainMem), &mainCompressed, compressBackwards, quickMode, &delta);
	cout << "Main block compressed " << (compressBackwards ? "backwards" : "forward") << " to " << lenOutMain << ", delta " << delta << "." << endl;
	
	//Copy from original buffer, as that one is read only.
	byte* Snap2TapLoader = new byte[sizeof(sna2tapldr)];
	memcpy(Snap2TapLoader, sna2tapldr, ldrSize);
	LoaderParams* ldrPrm = (LoaderParams*)&Snap2TapLoader[sizeof(sna2tapldr) - sizeof(LoaderParams)];
	ldrPrm->compressedLenTotal = lenOutMain + lenOutScr;
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
	fwrite(mainCompressed, 1, lenOutMain, fblob);
	fwrite(scrCompressed, 1, lenOutScr, fblob);
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