//////////////////////////////////////////////////////////////////////////
//Base class for file system classes
//Author: George Chirtoaca, 2014
//////////////////////////////////////////////////////////////////////////
#ifndef CFILESYSTEM_H
#define CFILESYSTEM_H

#include "types.h"
#include "DiskBase.h"
#include "CFile.h"
#include "CFileArchive.h"
#include <string.h>
#include <list>
#include <vector>

using namespace std;

class CFileSystem: public CFileArchive
{
	friend class CFile;
public:		
	

	CFileSystem(CDiskBase* disk, char* name = NULL);		
	virtual ~CFileSystem();	

	char Name[32];
	CDiskBase* Disk;

	virtual bool Init() = 0;					
	virtual CFile* NewFile(char* name, long len = 0, byte* data = NULL) { return NULL; };
	virtual CFile* FindFirst(char* pattern) = 0;
	virtual CFile* FindNext() = 0;
	virtual dword GetFileSize(CFile* file, bool onDisk = false);			
			
	virtual bool ReadBlock(byte* buf, word blkIdx, byte sectCnt = 0) = 0;
	virtual bool WriteBlock(word blkIdx, byte* buf) { LastError = ERR_NOT_SUPPORTED; return false; };

	virtual bool ReadFile(CFile*);
	virtual bool WriteFile(CFile*) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool Delete(char* fnames) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool Rename(CFile*, char* newName) { LastError = ERR_NOT_SUPPORTED; return false; };		
	virtual bool GetFileFolder(CFile* file, char* folderName) { LastError = ERR_NOT_SUPPORTED; return false; }	
	virtual bool SetFileFolder(CFile* file, char* folderName) { LastError = ERR_NOT_SUPPORTED; return false; }		

	virtual bool OpenFile(CFile* file) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool CloseFile(CFile* file) { LastError = ERR_NOT_SUPPORTED; return false; };

	//Generic disk space info.
	virtual word GetBlockSize() { return FSParams.BlockSize; }	
	virtual word GetFreeBlockCount();
	virtual word GetMaxBlockCount() { return this->FSParams.BlockCount; }
	virtual word GetFreeDirEntriesCount();	
	virtual word GetMaxDirEntriesCount() { return this->FSParams.DirEntryCount; }	
	virtual dword GetDiskMaxSpace();
	virtual dword GetDiskLeftSpace();		

	//Disk specific.		
	virtual bool Format() { return Disk->FormatDisk(); };
	virtual bool GetLabel(char* dest) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool SetLabel(char* src) { LastError = ERR_NOT_SUPPORTED; return false; };

	virtual ErrorType GetLastError(char* errMsg)
	{
		CFileArchive::GetLastError(errMsg);

		if (LastError == ERR_PHYSICAL)			
		{
			char errDisk[80];
			Disk->GetLastError(errDisk);
			strcat(errMsg, " ");
			strcat(errMsg, errDisk);
		}

		return LastError;
	}

	struct FSParamsType
	{
		//Allocation unit size, usually multiple of 1K.
		word BlockSize;
		//The max. no. of blocks.
		word BlockCount;	
		//The max. no. of directory entries.
		word DirEntryCount;
		//The number of reserved boot tracks.
		byte BootTrackCount;
	} FSParams;	


protected:						
	//Allocation unit map: free/occupied.
	vector<bool> FS_BlockMap;
	//Directory entries map: free/occupied.
	vector<bool> FS_DirEntryMap;
};


#endif//CFILESYSTEM_H