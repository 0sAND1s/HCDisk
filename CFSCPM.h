//////////////////////////////////////////////////////////////////////////
//CPM driver, (C) George Chirtoaca 2014
//Specs: http://www.seasip.demon.co.uk/Cpm/index.html#archive
//////////////////////////////////////////////////////////////////////////
#ifndef _CPMFS_H_
#define _CPMFS_H_

#include "types.h"
#include "DiskBase.h"
#include "CFileCPM.h"
#include "CFileSystem.h"

#include <bitset>
#include <vector>
#include <map>
#include <list>
#include <string>

using namespace std;

class CFileCPM;

class CFSCPM: public CFileSystem
{
	friend class CFileCPM;

public:
	static const byte DIR_ENTRY_BYTES_CNT = 16; //max no of allocation units a dir. entry can hold
	static const word MAX_FILE_NO = 256;
	static const byte CPM_REC_SIZE = 128;
	static const byte DEL_MARKER = 0xE5;
	static const byte EOF_MARKER = 0x1A;
	static const word CPM_EXT_ALLOC = 16384;	//space addressed by one extension; there can be more than 1 extension in 1 directory entry.

#pragma pack(1)
	//layout of directory entry
	typedef struct
	{
		byte UsrCode;	//0xE5 when file is deleted
		char FileName[CFileCPM::MAX_FILE_NAME_LEN]; //the file name
		byte ExtIdx;	//extent index
		byte ExtIdx2;	//extent index, high byte, formula: Entry number = ((32*ExtIdx2)+ExtIdx)/(exm+1)
		byte S1;		//unused space
		byte RecCnt;	//file size in 128 byte records multiple
		union
		{
			byte byteIdx[DIR_ENTRY_BYTES_CNT];
			word wordIdx[DIR_ENTRY_BYTES_CNT/2];			
		} AllocUnits;		//allocation units numbers
	}  DirectoryEntryType;
			
	//Disk directory buffer	
	DirectoryEntryType DirEntries[MAX_FILE_NO];				

	//CPM FS parameters
	struct CPMParams
	{		
		word ExtentsInDirEntry;	//Number of extents (16384 bytes) addressed by one directory entry.
		word Interleave;		//Software interleave factor. 0 = consecutive sector read, 1 = skip 1 sector (1,3,5..), etc.		
	};

	//CPM disk descriptor
	CPMParams FSDesc;
#pragma pack()

	typedef enum
	{
		FILE_OPEN_MODE_CREATE,
		FILE_OPEN_MODE_EXISTING
	} FileOpenMode;		


	CFSCPM(CDiskBase * disk, FSParamsType fsParams, CPMParams cpmParams, char* name = NULL);
	virtual CFSCPM::~CFSCPM();

	virtual bool Init() { return ReadDirectory(); }

	virtual CFile* NewFile(char* name, long len = 0, byte* data = NULL) 
	{ 
		CFileCPM* f = new CFileCPM(this);
		f->SetFileName(name);
		if (len > 0 && data != NULL)
			f->SetData(data, len);
		return f; 
	};
	virtual CFile* FindFirst(char* pattern = "*");		
	virtual CFile* FindNext();		
	virtual bool WriteFile(CFileCPM* file);
	virtual bool WriteFile(CFile* file) { return this->WriteFile((CFileCPM*)file);};
		
	virtual bool Delete(char* fnames);	
	virtual bool Rename(CFileCPM* f, char* newName);
	virtual bool Rename(CFile* f, char* newName) { return this->Rename((CFileCPM*)f, newName); }
	virtual bool Format();

	virtual dword GetFileSize(CFileCPM* file, bool onDisk = true);			
	virtual dword GetFileSize(CFile* file, bool onDisk) { return GetFileSize((CFileCPM*)file, onDisk); };			
	virtual FileAttributes GetAttributes(CFileCPM* file);
	virtual FileAttributes GetAttributes(CFile* file) { return GetAttributes((CFileCPM*)file); };	
	virtual bool SetAttributes(char* filespec, FileAttributes toSet, FileAttributes toClear);
	virtual bool GetFileFolder(CFile* file, char* folderName);
	virtual bool SetFileFolder(CFile* file, char* folderName);	
	virtual bool ReadBlock(byte* buf, word auIdx, byte maxSecCnt = 0);		
	virtual bool WriteBlock(byte* buf, word auIdx, byte maxSecCnt = 0);	

protected:							
	word CPM_FindIdx;				//Points to the next extension to check in FindFirst/FindNext cycle.
	byte CPM_ReservedAllocUnits;	//The number of reserved allocation units, for directory.
	byte CPM_InterleaveTbl[CDiskBase::MAX_SECT_PER_TRACK];
	char CPM_FindPattern[CFileCPM::MAX_FILE_NAME_LEN + 2];		
	//Allocation units per extension
	byte CPM_AUInExt;				
	typedef std::vector<CFileCPM> CPMFileListType;
	CPMFileListType CPM_FileList;	
	string CWD;

	//Translate a linear sector count to head/track/sector.
	bool LinearSect2HTS(word sectCnt, byte & head, byte & track, byte & sect);			
	word GetLastExtRecCnt(CFileCPM* file);	
	//Returns the next free alloc unit.
	word GetNextFreeAllocUnit();
	//Returns the allocation unit number from the specified directory entry.
	word GetAUNoFromExt(word extIdx, byte auPos);
	word GetNextFreeDirEntryIdx();		
	virtual bool ReadDirectory();	
	virtual bool WriteDirectory();
	virtual bool CreateFSName(CFile* file, char* fNameOut);
	static int FileDirEntSorter(word dirEnt1, word dirEnt2);
	virtual bool OpenFile(CFile* file);
	virtual bool CFSCPM::CloseFile(CFile* file);
};

#endif//_CPMFS_H_