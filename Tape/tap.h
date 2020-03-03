//////////////////////////////////////////////////////////////////////////
//TAP image file parser, (C) George Chirtoaca 2014
//Specs used: http://www.zx-modules.de/fileformats/tapformat.html
//////////////////////////////////////////////////////////////////////////

#ifndef _TAP_H_
#define _TAP_H_

#include <stdio.h>
#include "types.h"
#include "TapeBlock.h"
#include <map>
#include <vector>

using namespace std;

class CTapFile
{
public:	
#pragma pack(1)		
	typedef enum
	{
		TAP_OPEN_EXISTING,
		TAP_OPEN_NEW
	} TapeOpenMode;

	//Edit status of block.
	typedef enum
	{
		BLK_STATUS_NORMAL,
		BLK_STATUS_DELETED,
		BLK_STATUS_INSERTED		
	} TapeBlkEditStatus;			

	//Read status of block.
	typedef enum
	{
		BLK_RD_STS_VALID,
		BLK_RD_STS_DEPRECATED,
		BLK_RD_STS_UNSUPORTED,
		BLK_RD_STS_UNRECOGNIZED,
		BLK_RD_STS_INVALID
	} TapeBlkReadStatus;

	typedef struct
	{
		dword Offset;		
		TapeBlkReadStatus RdSts;
	} TapeBlkIdxType;
#pragma pack()

	static const char* TapeBlkReadStatusNames[];
	
	dword m_CurBlkIdx;
	TapeBlkReadStatus m_CurBlkStts;
	long m_FileSz;

	char * fileName;
protected:
	
	FILE * tapFile;			
	vector<TapeBlkIdxType> m_Idx;

public:
	CTapFile();	
	~CTapFile();

	virtual bool Open(char * fileName, TapeOpenMode mode = TAP_OPEN_EXISTING);
	virtual bool Close();
	
	virtual bool GetFirstBlock(CTapeBlock* tb);
	virtual bool GetNextBlock(CTapeBlock* tb);
	virtual bool GetBlock(word blkIdx, CTapeBlock* tb);

	virtual bool IndexTape();
	dword GetBlockCount();
	//ABSOLUTE block seek from beginning.
	virtual bool Seek(word iBlkIdx);


	CTapeBlock operator[](dword blkIdx);

	/*
	TapeBlock FindTapeHeaderBlock(char* blockName);
	bool FindTapeBlock(unsigned long idx, TapeBlock & tb);
	*/

	virtual bool AddTapeBlock(void* data, word dataLen, byte flag);
	virtual void AddTapeHeaderBlock(CTapeBlock::TapeBlockHeaderBasicType blType, char* name, word dataLen, word param1, word param2);	
	virtual void AddTapeProgramHeader(char* name, word dataLen, word startLine, word varLen);
	virtual void AddTapeCodeHeader(char* name, word dataLen, word startAddr);
	virtual void AddTapeArrayHeader(char* name, word dataLen, CTapeBlock::TapeBlockHeaderBasicType blType, char varName);	
};

#endif// _TAP_H_