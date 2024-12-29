//////////////////////////////////////////////////////////////////////////
//OPUS Discovery File system driver. (C) Chirtoaca George 2014 
//Specs used: http://www.worldofspectrum.org/opustools.html, Gert Kremer + reverse engineering
//////////////////////////////////////////////////////////////////////////

#ifndef _CFSOpus_
#define _CFSOpus_

#include "types.h"
#include "DiskBase.h"
#include "CFileSystem.h"
#include "CFileSpectrum.h"
#include <vector>

class CFileOpus;

class CFSOpus: public CFileSystem
{
	friend class CFileOpus;
public:
	CFSOpus(CDiskBase* disk, char* name = NULL);
	virtual ~CFSOpus() {};

	virtual bool Init();						
	virtual CFile* FindFirst(char* pattern);
	virtual CFile* FindNext();	
	virtual bool ReadBlock(byte* buf, word blkIdx, byte sectCnt = 0);	
	virtual bool OpenFile(CFile* file);
	virtual bool CloseFile(CFile* file);
	virtual bool ReadFile(CFile* f);
	virtual bool GetLabel(char* dest);
	virtual bool GetAutorunFilename(char** fileName) { *fileName = "run"; return true; }

protected:	
	//Block 1 is for disk description, blocks 1-7 are for catalog.
	const static word CATALOG_BLOCKS = 7;

#pragma pack(1)
	typedef struct
	{
		word bytes_in_last_block;
		word first_block;
		word last_block;
		char    name[10];
	} DirEntryOpus;

	typedef struct
	{
		byte Type;
		word Length;
		word StartAddr;		
		word StartLine;
	} OpusHeader;

	typedef struct  
	{
		word jmp;
		byte trackCnt;
		byte sectCnt;
		byte drvCfg:4;
		bool dblSide:1;
		bool singleDensity:1;
		byte sectSizeCode:2;
	} OpusBootSector;
#pragma pack()

	vector<DirEntryOpus> OpusDir;
	vector<CFileOpus> OpusFiles;
	byte findIdx;
	char* findPattern;
};

class CFileOpus: public CFileSpectrum, public CFile
{
	friend class CFSOpus;

public:
	~CFileOpus()
	{
		if (buffer != NULL && isOpened)
		{
			delete buffer;
			buffer = NULL;
		}
	}
};

#endif//CFSOpus