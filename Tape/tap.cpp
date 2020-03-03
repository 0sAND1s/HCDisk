/*
TAP file format from Z80 documentation

The .TAP files contain blocks of tape-saved data. All blocks start with two bytes specifying how many bytes will follow 
(not counting the two length bytes). Then raw tape data follows, including the flag and checksum bytes. The checksum is 
the bitwise XOR of all bytes including the flag byte. For example, when you execute the line SAVE "ROM" CODE 0,2 this will result:

          |------ Spectrum-generated data -------|       |---------|
  
    13 00 00 03 52 4f 4d 7x20 02 00 00 00 00 80 f1 04 00 ff f3 af a3
	
    ^^^^^...... first block is 19 bytes (17 bytes+flag+checksum)
	      ^^... flag byte (A reg, 00 for headers, ff for data blocks)
	         ^^ first byte of header, indicating a code block
	  
    file name ..^^^^^^^^^^^^^
	header info ..............^^^^^^^^^^^^^^^^^
	checksum of header .........................^^
	length of second block ........................^^^^^
	flag byte ............................................^^
	first two bytes of rom .................................^^^^^
	checksum (checkbittoggle would be a better name!).............^^
		
	Note that it is possible to join .TAP files by simply stringing them together, for example COPY /B FILE1.TAP + FILE2.TAP ALL.TAP
		  
	For completeness, I'll include the structure of a tape header. A header always consists of 17 bytes:

	  Byte    Length  Description
	  ---------------------------
	  0       1       Type (0,1,2 or 3)
	  1       10      Filename (padded with blanks)
	  11      2       Length of data block
	  13      2       Parameter 1
	  15      2       Parameter 2
			  
The type is 0,1,2 or 3 for a Program, Number array, Character array or Code file. A SCREEN$ file is regarded as a Code 
file with start address 16384 and length 6912 decimal. If the file is a Program file, parameter 1 holds the autostart 
line number (or a number >=32768 if no LINE parameter was given) and parameter 2 holds the start of the variable area 
relative to the start of the program. If it's a Code file, parameter 1 holds the start of the code block when saved, 
and parameter 2 holds 32768. For data files finally, the byte at position 14 decimal holds the variable name.
*/


#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "tap.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK , __FILE__ , __LINE__ )	
#endif
#endif


const char* CTapFile::TapeBlkReadStatusNames[] =
{
	"VALID",
	"DEPRECATED",
	"UNSUPORTED",
	"UNRECOGNIZED",
	"INVALID"
};

CTapFile::CTapFile()
{	
	tapFile = NULL;
}


CTapFile::~CTapFile()
{
	Close();
}



bool CTapFile::Open(char* fileName, TapeOpenMode mode)
{	
	this->fileName = fileName;
	CTapeBlock tb;	

	if (mode == TAP_OPEN_EXISTING)
	{
		tapFile = fopen(fileName, "rb+");
		return tapFile != NULL && IndexTape();
	}
	else if (mode == TAP_OPEN_NEW)
	{
		tapFile = fopen(fileName, "wb+");		
		return tapFile != NULL;		
	}
	
	return false;
}


bool CTapFile::Close()
{
	bool Result = true;

	if (tapFile)
		Result = fclose(tapFile) != EOF;

	m_Idx.clear();

	return Result;
}

bool CTapFile::GetNextBlock(CTapeBlock* tb)
{
	m_CurBlkIdx++;

	bool ReadOK = (fread(&tb->Length, sizeof(word), 1, tapFile) == 1) && 	
		(fread(&tb->Flag, sizeof(byte), 1, tapFile) == 1) &&
		((tb->Data = new byte[tb->Length - 2]) != NULL) &&
		(fread(tb->Data, 1, tb->Length - sizeof(tb->Flag) - sizeof(tb->Checksum), tapFile) == tb->Length - sizeof(tb->Flag) - sizeof(tb->Checksum)) &&
		(fread(&tb->Checksum, sizeof(tb->Checksum), 1, tapFile) == 1) &&
		(tb->Checksum == tb->GetBlockChecksum());

	if (tb->Flag < 128)
		tb->m_Timings = CTapeBlock::ROM_TIMINGS_HEAD;		
	else
		tb->m_Timings = CTapeBlock::ROM_TIMINGS_DATA;		

	tb->LastBitCnt = 8;

	tb->m_BlkType = CTapeBlock::TAPE_BLK_STD;
	m_CurBlkStts = BLK_RD_STS_VALID;

	return ReadOK;
}

bool CTapFile::GetFirstBlock(CTapeBlock* tb)
{		
	m_CurBlkIdx = -1;
	return (fseek(tapFile, 0, SEEK_SET) == 0) && GetNextBlock(tb);
}

bool CTapFile::GetBlock(word blkIdx, CTapeBlock* tb)
{	
	return blkIdx < GetBlockCount() && Seek(blkIdx) && (blkIdx == 0 ? GetFirstBlock(tb) : GetNextBlock(tb));
}

bool CTapFile::IndexTape()
{	
	long fileOffset = 0;
	word blkLen = 0;
	bool Res = true;
	
	m_Idx.clear();
	
	while (Res)
	{
		TapeBlkIdxType idxType = {fileOffset, BLK_RD_STS_VALID};		
		Res = (fread(&blkLen, sizeof(blkLen), 1, tapFile) == 1) && (fseek(tapFile, blkLen, SEEK_CUR) == 0);
		if (Res)
		{
			m_Idx.push_back(idxType);
			fileOffset = ftell(tapFile);
		}
	} 		

	return m_Idx.size() > 0;
}

dword CTapFile::GetBlockCount()
{
	return (word)m_Idx.size();
}

CTapeBlock CTapFile::operator[](dword blkIdx)
{
	CTapeBlock tb;

	if (blkIdx < m_Idx.size())
	{
		fseek(tapFile, m_Idx[blkIdx].Offset, SEEK_SET);
		GetNextBlock(&tb);
	}
	else
	{		
		tb.m_BlkType = CTapeBlock::TAPE_BLK_INVALID;		
	}

	return tb;
}

bool CTapFile::Seek(word iBlkIdx)
{
	if (iBlkIdx < m_Idx.size())
	{
		m_CurBlkIdx = iBlkIdx;
		return fseek(tapFile, m_Idx[m_CurBlkIdx].Offset, SEEK_SET) == 0;
	}	
	else
		return false;
}

/*
CTapFile::TapeBlock CTapFile::FindTapeHeaderBlock(char* blockName)
{
	bool FoundBlock = false;
	CTapeBlock TB;

	while (!FoundBlock && GetNextBlock(TB))
	{		
		if (IsBlockHeader(TB))
		{
			TapeHeader* TH = (TapeHeader*)TB.Data;			
			if (strncmp(TH->FileName, blockName, TAP_FILENAME_LEN) == 0)
				FoundBlock = true;
		}
	}

	return TB;
}

bool CTapFile::FindTapeBlock(unsigned long blkIdx, TapeBlock & tb)
{
	unsigned long IdxCnt = 0;
	bool FoundBlock = false;

	while ((FoundBlock = GetNextBlock(tb)) && IdxCnt++ < blkIdx);

	return (FoundBlock && IdxCnt-1 == blkIdx);
}
*/

bool CTapFile::AddTapeBlock(void* data, word dataLen, byte flag)
{
	CTapeBlock TB;

	TB.Length = dataLen + 2;
	TB.Flag = flag;
	TB.Data = new byte[dataLen];
	memcpy(TB.Data, data, dataLen);
	TB.Checksum = TB.GetBlockChecksum();

	fseek(tapFile, 0, SEEK_END);

	bool res = fwrite(&TB.Length, 1, sizeof(word), tapFile) == sizeof(word) &&
		fwrite(&flag, 1, sizeof(TB.Flag), tapFile) == sizeof(TB.Flag) &&
		fwrite(TB.Data, 1, dataLen, tapFile) == dataLen &&
		fwrite(&TB.Checksum, 1, sizeof(TB.Checksum), tapFile) == sizeof(TB.Checksum);
	
	return res;
}


void CTapFile::AddTapeHeaderBlock(CTapeBlock::TapeBlockHeaderBasicType blType, char* name, word dataLen, word param1, word param2)
{
	CTapeBlock::TapeBlockHeaderBasic TH;

	TH.BlockType = blType;
	memset(TH.FileName, ' ', CTapeBlock::TAP_FILENAME_LEN);
	memcpy(TH.FileName, name, strlen(name));
	TH.Length = dataLen;

	TH.Param1 = param1;
	TH.Param2 = param2;

	AddTapeBlock(&TH, sizeof(TH), CTapeBlock::TAPE_BLOCK_FLAG_HEADER);
}


void CTapFile::AddTapeProgramHeader( char* name, word dataLen, word startLine, word varLen)
{
	AddTapeHeaderBlock(CTapeBlock::TAPE_BLK_BAS_HDR_PROGRAM, name, dataLen, startLine, dataLen-varLen);
}


void CTapFile::AddTapeCodeHeader( char* name, word dataLen, word startAddr)
{
	AddTapeHeaderBlock(CTapeBlock::TAPE_BLK_BAS_HDR_CODE, name, dataLen, startAddr, CTapeBlock::TAPE_UNUSED_PARAM_VAL);
}


void CTapFile::AddTapeArrayHeader( char* name, word dataLen, CTapeBlock::TapeBlockHeaderBasicType blType, char varName)
{
	//The variable name must be uppercase and is held in the MSB part of the first parameter word.	
	AddTapeHeaderBlock(blType, name, dataLen, toupper(varName) << 8,  CTapeBlock::TAPE_UNUSED_PARAM_VAL);
}
