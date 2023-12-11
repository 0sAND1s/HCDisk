//////////////////////////////////////////////////////////////////////////
//Purpose	: ICTI CoBra computer DEVIL file system driver
//Author	: George Chirtoaca, 2014
//Specs		: Reverse engineered, later confirmed with https://www.cobrasov.com/CoBra%20Project/devil.html .
//The block size is 9 KB. Max. block count is 77 = ((80 x 2 - 6) * 2 * 256) / 9K.
//Each directory entry has 1 sector and can address 1 or more blocks. Max. dir. entries is 108 = 6 directory-reserved tracks x 18 SPT.
//For headerless blocks from tape, the dir entry contains 2-byte size of each block. Identical dir entries are created for each headerless block, for each 9K file segment.
//To determine which identical dir entry maps to which file, for the headerless blocks, the block size from the header can be used. If block size is 0, the dir entry refers to the a regular file or regular file extent.
//If the block size from header is > 0, the dir entry refers to the first file (if dir entry size < first block length) or to a headerless file (each block size determines which dir entry).
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "CFileSystem.h"
#include "CFileSpectrum.h"
#include <vector>

class CFileDevil;

class CFSCobraDEVIL: public CFileSystem
{
	friend class CFileDevil;
public:

#pragma pack (1)
	static const byte FLAG_EXTENT = 0xE4;
	static const byte FLAG_DELETE = 0xE5;

	struct DirEntryType
	{
		byte Flag;
		char Name[10];
		word Length;
		word Param1;
		word Param2;
		byte Attributes;
		byte unused[2];
		word hdrlBlockLen[118];		
	};
#pragma pack ()

	CFSCobraDEVIL(CDiskBase* disk, char* name);
	virtual ~CFSCobraDEVIL() {} ;

	bool Init();	
	dword GetDiskMaxSpace();
	dword GetDiskLeftSpace();

	virtual dword GetFileSize(CFile* file, bool onDisk = false);
	CFile* NewFile(const char* name, long len = 0, byte* data = NULL);
	CFile* FindFirst(char* pattern);
	CFile* FindNext();
	virtual bool OpenFile(CFile*) { return true; };
	virtual bool CloseFile(CFile*) { return true; };
	virtual bool ReadBlock(byte* buf, word blkIdx, byte sectCnt = 0);
	virtual bool ReadFile(CFile* file);
	virtual FileAttributes GetAttributes(CFile* file);

	//bool ReadFile(CFile*);	
	bool IsCharValidForFilename(char c)
	{
		return c >= ' ' && c < 128 && strchr("\"*?\\/", c) == NULL;
	}

protected:
	static const word RESERVED_TRACKS = 6;
	static const word SPT = 18;	

	byte findIdx;
	std::vector<DirEntryType> DEVIL_Dir;
	std::vector<CFileDevil> DEVIL_FileList;
	char DEVIL_FindPattern[CFileSystem::MAX_FILE_NAME_LEN];
};

//////////////////////////////////////////////////////////////////////////

class CFileDevil: public CFile, public CFileSpectrum
{
	friend class CFSCobraDEVIL;
public:		

	CFileDevil();
	CFileDevil(const CFileDevil&);
	CFileDevil(CFSCobraDEVIL* fs);
	virtual ~CFileDevil();
	virtual CFileDevil& operator= (const CFileDevil&);		
protected:	
	CFSCobraDEVIL* fs;
};