//////////////////////////////////////////////////////////////////////////
//MGT +D File system driver. (C) Chirtoaca George 2014 
//Specs used: http://web.archive.org/web/20080514125600/http://www.ramsoft.bbk.org/tech/mgt_tech.txt
//////////////////////////////////////////////////////////////////////////

#pragma once


#include "types.h"
#include "DiskBase.h"
#include "CFileSystem.h"
#include "CFileSpectrum.h"
#include <vector>

class CFSMGT: public CFileSystem
{
	friend class CFileMGT;

public:
	CFSMGT(CDiskBase* disk, char* name = NULL) : CFileSystem(disk, name) 
	{
		FSFeatures = (FileSystemFeature)(FSFeatures | (FSFT_DISK | FSFT_ZXSPECTRUM_FILES | FSFT_CASE_SENSITIVE_FILENAMES));
	};
	~CFSMGT() 
	{
		if (Disk != NULL)
		{
			delete Disk;
			Disk = NULL;
		}
	};

	virtual bool Init();						
	virtual CFile* FindFirst(char* pattern);
	virtual CFile* FindNext();	
	virtual bool ReadBlock(byte* buf, word blkIdx, byte sectCnt = 0);	
	virtual bool OpenFile(CFile* file);
	virtual bool CloseFile(CFile* file);
	virtual bool ReadFile(CFile* f);	

protected:
	const static word BLOCK_SIZE = 512;
	const static word DIR_ENTRY_COUNT = 80;	
	const static byte DIR_TRACKS = 4;

#pragma pack(1)
	typedef struct  
	{
		byte TypeSpectrum;
		word Length;
		word StartAddr;
		union
		{
			word ProgLen;
			byte ArrVarName;
		};		
		word StartRun;		
	} MGTFileHdrSpectrum;

	typedef struct
	{
		byte BlockCount64K;
		byte Flag9;
		word LenLastBlock;
	} MGTFileHdrOpen;

	typedef struct  
	{
		byte TypeMGT;
		char Name[10];
		byte SectorCntHi;
		byte SectorCntLow;
		byte StartTrack;
		byte StartSector;
		byte AllocMap[196];
		union
		{
			MGTFileHdrSpectrum FileHdrSpectrum;
			MGTFileHdrOpen FileHdrOpen;
			byte unused[45];
		};		
	} MGTDirEntry;	
#pragma pack()
	
	vector<CFileMGT> MGTFiles;

	enum MGTFileType
	{
		MGT_FT_ERASED	= 0,

		//BASIC codes, +1.
		MGT_FT_BASIC	= 1,
		MGT_FT_NO_ARR	= 2,
		MGT_FT_STR_ARR	= 3,
		MGT_FT_CODE		= 4,

		MGT_FT_SNAP48	= 5,
		MGT_FT_MDRV		= 6,
		MGT_FT_SCR		= 7,
		MGT_FT_SPECIAL	= 8,
		MGT_FT_SNAP128	= 9,
		MGT_FT_OPEN		= 10,
		MGT_FT_EXE		= 11,
		MGT_FT_SUBDIR	= 12,
		MGT_FT_CREATE	= 13
	};

	byte FindIdx;
	char* FindPattern;

	virtual CFileMGT* CreateFileMGT(MGTDirEntry dir);	
};

class CFileMGT: public CFile, public CFileSpectrum
{
	friend class CFSMGT;
};