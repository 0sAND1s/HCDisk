//////////////////////////////////////////////////////////////////////////
//Base class for file archive (file collections)
//Author: George Chirtoaca, 2014
//////////////////////////////////////////////////////////////////////////

#ifndef CFILE_ARCHIVE_H
#define CFILE_ARCHIVE_H

#include "types.h"
#include "CFile.h"

//Class for files group, that is not a disk: tape image, zip archives, etc.
class CFileArchive
{
	friend class CFile;
public:
	static const byte MAX_FILE_NAME_LEN = 11;	
	//accomodate . before extension and ending 0 (8.3\0).
	typedef char FileNameType[MAX_FILE_NAME_LEN+2];			

	enum ErrorType
	{
		ERR_NONE,
		ERR_NOT_SUPPORTED,
		ERR_BAD_PARAM,

		ERR_PHYSICAL,			//Physical error, detailed by CDiskBase.GetLastError().

		ERR_NO_CATALOG_LEFT,
		ERR_NO_SPACE_LEFT,
		ERR_FILE_EXISTS,
		ERR_FILE_NOT_FOUND,
		ERR_FILE_READ_ONLY
	};		

	enum FileAttributes
	{
		ATTR_NONE		= 0,

		ATTR_ARCHIVE	= 1,
		ATTR_READ_ONLY	= 2,
		ATTR_HIDDEN		= 4,
		ATTR_SYSTEM		= 8,		

		ATTR_DIRECTORY	= 12
	};

	const static char* FSFeatureNames[];

	enum FileSystemFeature
	{
		FSFT_NONE				= 0,
		FSFT_DISK				= 1,
		FSFT_ZXSPECTRUM_FILES	= 2,	//FS contains ZX Spectrum files (Program, Bytes, etc).
		FSFT_LABEL				= 4,	//FS supports the label feature
		FSFT_FILE_ATTRIBUTES	= 8,	//FS supports file attributes
		FSFT_FOLDERS			= 16,	//FS supports folders
		FSFT_TIMESTAMPS			= 32,	//FS supports file timestamps (CPM 3.0, FAT, etc)
		FSFT_CASE_SENSITIVE_FILENAMES = 64,
		FSFT_TAPE				= 128,
		FSFT_AUTORUN			= 256
	} FSFeatures;	


	char Name[260];

	CFileArchive(char* name = NULL);	
	virtual ~CFileArchive();		

	virtual bool Init() = 0;					
	virtual CFile* NewFile(const char* name, long len = 0, byte* data = NULL) { LastError = ERR_NONE; return NULL; };
	virtual CFile* FindFirst(char* pattern) = 0;
	virtual CFile* FindFirst(char* pattern, bool includeDeleted) { return FindFirst(pattern); };
	virtual CFile* FindNext() = 0;
	virtual dword GetFileSize(CFile* file, bool onDisk = false);			
	virtual FileSystemFeature GetFSFeatures() { return FSFeatures; };
	
	virtual FileAttributes GetAttributes(CFile*) { return ATTR_NONE; };					
	virtual bool SetAttributes(char* filespec, FileAttributes toSet, FileAttributes toClear) { LastError = ERR_NOT_SUPPORTED; return false; };

	virtual bool OpenFile(CFile*) { LastError = ERR_NONE; return true; };
	virtual bool CloseFile(CFile*) { LastError = ERR_NONE; return true; };
	virtual bool ReadFile(CFile*) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool WriteFile(CFile*) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool Delete(char* fnames) { LastError = ERR_NOT_SUPPORTED; return false; };
	virtual bool Rename(CFile*, char* newName) { LastError = ERR_NOT_SUPPORTED; return false; };	
	virtual bool GetFileFolder(CFile* file, char* folderName) { LastError = ERR_NOT_SUPPORTED; return false; }	
	virtual bool SetFileFolder(CFile* file, char* folderName) { LastError = ERR_NOT_SUPPORTED; return false; }
	
	virtual bool IsCharValidForFilename(char c);
	virtual bool CreateFileName(const char* fNameIn, CFile* file);
	virtual bool CreateFSName(CFile* file, char* fNameOut);

	virtual bool GetAutorunFilename(char** runFileName) { LastError = ERR_NOT_SUPPORTED; return false; }

	virtual ErrorType GetLastError(char* errMsg = NULL)
	{		
		if (errMsg != NULL)		
			strcpy(errMsg, ERROR_TYPE_MSG[LastError]);					
		return LastError;		
	}

	static bool TrimEnd(char* src)
	{
		int len = strlen(src);
		while (len > 0 && src[len - 1] == ' ')
		{
			src[len - 1] = '\0';
			len = strlen(src);
		}

		return src != NULL;
	}

	void GetFileNameLen(byte* nameLen, byte* extLen) 
	{
		if (nameLen != nullptr)
			*nameLen = NAME_LENGHT;

		if (extLen != nullptr)
			*extLen = EXT_LENGTH;
	}

	static bool StrWildCmp(const char* string, const char* wild);

protected:
	ErrorType LastError;		
	const static char* ERROR_TYPE_MSG[];		
	byte NAME_LENGHT, EXT_LENGTH;		

	//Compares 2 strings using wildcards.
	bool WildCmp(char * mask, const FileNameType fileName);			
};

#endif