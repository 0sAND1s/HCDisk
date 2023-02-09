//////////////////////////////////////////////////////////////////////////
//Purpose	: ICTI CoBra computer DEVIL file system driver
//Author	: George Chirtoaca, 2014
//Specs		: None, reverse engineered.
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
	bool ReadBlock(byte* buf, word blkIdx, byte sectCnt = 0);
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