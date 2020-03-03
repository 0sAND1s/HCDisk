//////////////////////////////////////////////////////////////////////////
//TRD File system driver. (C) Chirtoaca George
//Used specs: http://www.zx-modules.de/fileformats/trdformat.html and 
//http://web.archive.org/web/20080509075310/http://www.ramsoft.bbk.org/tech/tr-info.zip
//////////////////////////////////////////////////////////////////////////

#ifndef _CFSTRD_H_
#define _CFSTRD_H_

#include "types.h"
#include "DiskBase.h"
#include "CFileSpectrum.h"
#include "CFileSystem.h"
#include <vector>

class CFSTRDOS: public CFileSystem
{
	friend class CFileTRD;
public:		
	//The accepted TRDOS disk geometries.
	static const CDiskBase::DiskDescType TRDDiskTypes[];
	typedef enum
	{
		DTI_TRK_80_S2 = 0x16,
		DTI_TRK_40_S2 = 0x17,
		DTI_TRK_80_S1 = 0x18,
		DTI_TRK_40_S1 = 0x19
	} DiskTypeID;

	const static byte DEL_FLAG = 1;
	const static byte DISK_INFO_SECTOR = 9;
	const static byte MAX_DIR_ENTRIES = 128;
	const static byte TRDOS_ID = 0x10;

#pragma pack(1)
	struct DirEntryType
	{
		char FileName[8];
		char FileExt;
		
		union
		{
			word BasicProgVarLength;
			word ArrayUnused;
			word CodeStart;						
			word PrintExtent;			
		};		

		word Length;
		byte LenghtSect;
		//Not available for SCL.
		byte StartSect;
		byte StartTrack;
	};

	struct DiskDescTypeTRDOS
	{
		byte DirFullFlag;	//0 - signals end of root directory
		byte unused1[224];	//unused (filled by 0)
		byte NextFreeSect;	//number of first free sector on disk
		byte NextFreeSectTrack;//the number of track of first free sector on disk
		byte DiskType;		
		byte FileCnt;		//number of files on disk
		word FreeSectCnt;	//number of free sectors on disk
		byte TRDosID;		//TR-DOS ID byte (always 10h - 16 dec) 
		byte unused2[2];	
		byte Password[9];	//Disk password
		byte unused3[1];
		byte DelFilesCnt;	//number of deleted files on disk
		char DiskLabel[8];	//disk label (8 chars)
		byte unused4[3];	//end of disk info (filled by 0)
	};
#pragma pack()
	
	DiskDescTypeTRDOS diskDescTRD;


	CFSTRDOS(CDiskBase* theDisk, char* name = NULL);
	virtual ~CFSTRDOS();	

	virtual CFile* NewFile(char* name, long len = 0, byte* data = NULL);
	virtual CFile* FindFirst(char* pattern = "*");		
	virtual CFile* FindNext();	
	
	virtual dword GetDiskMaxSpace();
	virtual dword GetDiskLeftSpace();	
	
	virtual bool ReadFile(CFileTRD* file);
	virtual bool ReadFile(CFile* file) { return ReadFile((CFileTRD*)file); };
	virtual bool WriteFile(CFileTRD* file);
	virtual bool WriteFile(CFile* file) { return WriteFile((CFileTRD*)file); };

	virtual bool Format();
	virtual bool Delete(char* names);
	virtual bool Rename(CFile*, char* newName);		

	bool Init();	
	virtual dword GetFileSize(CFile* file, bool onDisk = false);
	virtual bool GetLabel(char* dest) 
	{
		memset(dest, 0, sizeof(diskDescTRD.DiskLabel) + 1);
		return strncpy(dest, diskDescTRD.DiskLabel, sizeof(diskDescTRD.DiskLabel)) != NULL; 
	};
	virtual bool OpenFile(CFile* file);
	virtual bool ReadBlock(byte* destBuf, word sectIdx, byte sectCnt = 0);
	
protected:		
	bool ReadDirectory();		
	//Converts a logical sector number to physical sector location.
	void LogicalToPhysicalSector(word logicalSect, byte* track, byte* side, byte* sectorID);	
	//Read the sectIdx'th file sector for file.	
	CFSTRDOS::DirEntryType TRD_Directory[MAX_DIR_ENTRIES];
	byte dirEntIdx;
	char* FindPattern;
	std::vector<CFileTRD> TRD_Files;
};

#endif//_CFSTRD_H_
//////////////////////////////////////////////////////////////////////////