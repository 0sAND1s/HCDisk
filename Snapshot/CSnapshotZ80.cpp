#include "CSnapshotZ80.h"
#include <stdio.h>
#include "..\FileUtil.h"
#include <algorithm>

using namespace std;

bool CSnapshotZ80::Read(const char* path)
{
	string ext = FileUtil::GetExtension(string(path));

	IsOpened = false;

	if (strnicmp(ext.c_str(), "Z80", 3) != 0)
		return false;	

	FILE* fZ80 = fopen(path, "rb");
	if (fZ80 == nullptr)
		return false;

	bool readOK = ReadHeaders(fZ80) && Is48KZ80Snapshot() && ProcessBlocks(fZ80);
	
	fclose(fZ80);

	if (readOK)
		Z802SNAHeader();

	IsOpened = readOK;	

	return readOK;
}

bool CSnapshotZ80::UncompressZ80Data(byte* srcBuffer, const word srcLen, const byte pageNo)
{
	const word expectedDestLen = (Z80SnapVersion == 1 ? 48 * 1024 : 16 * 1024);
	const byte marker = 0xED;
	word srcIdx = 0;
	dword dstIdx = 0;
	if (Z80SnapVersion > 1)
	{
		switch (pageNo)
		{
		case 4:
			dstIdx = 0x8000 - 0x4000;
			break;
		case 5:
			dstIdx = 0xC000 - 0x4000;
			break;
		case 8:
			dstIdx = 0x4000 - 0x4000;
			break;
		}
	}
	const dword dstIdxInit = dstIdx;

	byte* destBuf = SNAMemory.AllMem;
	byte counter, value;
	while (srcIdx < srcLen)
	{
		if (srcBuffer[srcIdx] != marker)
		{
			destBuf[dstIdx++] = srcBuffer[srcIdx++];
		}
		else if (srcIdx + 1 < srcLen && srcBuffer[srcIdx + 1] != marker)
		{
			destBuf[dstIdx++] = srcBuffer[srcIdx++];
		}
		else if (srcIdx + 3 < srcLen)
		{
			counter = srcBuffer[srcIdx + 2];
			value = srcBuffer[srcIdx + 3];
			srcIdx += 4;

			while (counter > 0)
			{
				destBuf[dstIdx] = value;
				counter--;
				dstIdx++;
			}
		}
	}
	
	return (dstIdx - dstIdxInit) == expectedDestLen;
}

bool CSnapshotZ80::Is48KZ80Snapshot()
{
	//V1 snapshot format supports only the 48K format.
	if (Z80SnapVersion == 1)
	{
		return true;
	}
	else if (Z80SnapVersion == 2)
	{
		return Z80AditionalHdr.Flags.HWModifications == false && 
			(Z80AditionalHdr.HWMode == 0 || Z80AditionalHdr.HWMode == 1);
	}
	else if (Z80SnapVersion == 3)
	{
		return Z80AditionalHdr.Flags.HWModifications == false &&
			(Z80AditionalHdr.HWMode == 0 || Z80AditionalHdr.HWMode == 1 || Z80AditionalHdr.HWMode == 3);
	}
	
	return false;
}

bool CSnapshotZ80::Write(const char* path)
{
	FILE* fZ80 = fopen(path, "wb");
	if (fZ80 == nullptr)
		return false;

	//Write a v1 Z80 file, uncompressed, for testing purposes.
	Z80MainHdr.Flags1.IsDataCompressed = false;
	if (Z80MainHdr.PC == 0)
		Z80MainHdr.PC = Z80AditionalHdr.PC;

	bool res = fwrite(&Z80MainHdr, sizeof(Z80MainHdr), 1, fZ80) == 1;
	if (res)
		res = fwrite(SNAMemory.AllMem, sizeof(SNAMemory.AllMem), 1, fZ80) == 1;

	fclose(fZ80);

	return res;
}

void CSnapshotZ80::Z802SNAHeader()
{
	SNAHdr.I = Z80MainHdr.I;
	SNAHdr.HLp = Z80MainHdr.HLp;
	SNAHdr.DEp = Z80MainHdr.DEp;
	SNAHdr.BCp = Z80MainHdr.BCp;
	SNAHdr.AFp = Z80MainHdr.Fp | (Z80MainHdr.Ap << 8);
	SNAHdr.HL = Z80MainHdr.HL;
	SNAHdr.DE = Z80MainHdr.DE;
	SNAHdr.BC = Z80MainHdr.BC;
	SNAHdr.IY = Z80MainHdr.IY;
	SNAHdr.IX = Z80MainHdr.IX;
	SNAHdr.IFF2 = (Z80MainHdr.IFF1 > 0 || Z80MainHdr.IFF2 > 0 ? 0x04 : 0);
	SNAHdr.R = Z80MainHdr.R | (Z80MainHdr.Flags1.RBit7 << 7);
	SNAHdr.AF = Z80MainHdr.F | (Z80MainHdr.A << 8);	
	SNAHdr.SP = Z80MainHdr.SP;
	
	//Store PC into SNA stack.
	word PC = Z80MainHdr.PC > 0 ? Z80MainHdr.PC : Z80AditionalHdr.PC;
	SNAHdr.SP -= 2;
	*(word*)&SNAMemory.AllMem[SNAHdr.SP - 0x4000] = PC;

	SNAHdr.IM = Z80MainHdr.Flags2.IM;
	SNAHdr.BorderClr = Z80MainHdr.Flags1.BorderClr;
}

bool CSnapshotZ80::ProcessBlocks(FILE* fZ80)
{
	bool resRead = true, resUnpack = true;

	//Read RAM data.
	if (Z80SnapVersion == 1)
	{
		dword fPos = ftell(fZ80);
		fseek(fZ80, 0, SEEK_END);
		dword fSize = ftell(fZ80);
		fseek(fZ80, fPos, SEEK_SET);

		//v1 snapshot has a single data block.
		const dword srcLen = fSize - sizeof(Z80MainHdr);
		byte* bufSrc = new byte[srcLen];
		resRead = fread(bufSrc, srcLen, 1, fZ80) == 1;

		if (resRead)
		{
			if (Z80MainHdr.Flags1.IsDataCompressed)
			{
				//Ignore end marker 00, ED, ED, 00.
				resUnpack = UncompressZ80Data(bufSrc, (word)srcLen - 4);
			}
			else
			{
				memcpy(&SNAMemory, bufSrc, srcLen);
			}
		}

		delete[] bufSrc;
	}
	else //Z80 v2 or v3
	{
		const byte blockCount = 3;	//48K has 3x16K blocks.
		byte blockIdx = 0;

		while (blockIdx < blockCount && resRead && resUnpack)
		{
			Z80DataBlockHeader blockHdr;
			resRead = fread(&blockHdr, sizeof(blockHdr), 1, fZ80) == 1;
			if (resRead)
			{
				if (blockHdr.Lenght == 0xFFFF)
					blockHdr.Lenght = 16384;

				byte* bufCompressed = new byte[blockHdr.Lenght];
				resRead = fread(bufCompressed, blockHdr.Lenght, 1, fZ80) == 1;
				if (resRead)
				{
					resUnpack = UncompressZ80Data(bufCompressed, blockHdr.Lenght, blockHdr.PageNo);
				}
				delete[] bufCompressed;
			}		

			blockIdx++;
		}
	}

	return resRead && resUnpack;
}

bool CSnapshotZ80::ReadHeaders(FILE* fZ80)
{
	if (fread((byte*)&Z80MainHdr, sizeof(Z80MainHdr), 1, fZ80) != 1)
	{
		return false;
	}

	//Fix Flags1 in case it's 0xff.
	if (*((byte*)&Z80MainHdr.Flags1) == 0xff)
		*((byte*)&Z80MainHdr.Flags1) = 1;

	//Determine Z80 version format based on additional header lenght
	word hdr2Len = 0;
	if (fread(&hdr2Len, sizeof(hdr2Len), 1, fZ80) != 1)
	{
		return false;
	}

	Z80SnapVersion = 1;

	if (hdr2Len == 23)
	{
		Z80SnapVersion = 2;
		if (fread(&Z80AditionalHdr, sizeof(Z80AditionalHdr), 1, fZ80) != 1)
		{
			return false;
		}
	}
	else if (hdr2Len == 54 || hdr2Len == 55)
	{
		Z80SnapVersion = 3;
		if (fread(&Z80AditionalHdr, sizeof(Z80AditionalHdr), 1, fZ80) != 1)
		{
			return false;
		}

		if (fread(&Z80AditionalHdrV3, sizeof(Z80AditionalHdrV3), 1, fZ80) != 1)
		{
			return false;
		}

		//Read last output value for port 0x1FFDh if hdr2Len is 55.
		if (hdr2Len == 55)
		{
			byte tmp;
			fread(&tmp, 1, 1, fZ80);
		}
	}

	//Rollback 2 bytes for v1.
	if (Z80SnapVersion == 1)
		fseek(fZ80, -1 * sizeof(hdr2Len), SEEK_CUR);

	return true;
}