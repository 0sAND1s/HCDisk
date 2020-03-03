//////////////////////////////////////////////////////////////////////////
//Purpose	: HC BASIC file system driver
//Author	: George Chirtoaca, 2014
//Specs		: reverse engineering
//////////////////////////////////////////////////////////////////////////

#ifndef _CFSCPMHC_H_
#define _CFSCPMHC_H_

#include "CFSCPM.h"
#include "CFileHC.h"

class CFSCPMHC: public CFSCPM
{
public:
	CFSCPMHC(CDiskBase* disk, FSParamsType fsParams, CPMParams params, char* name): CFSCPM(disk, fsParams, params,  name)
	{
		EXT_LENGTH = 0;
		NAME_LENGHT = 11;
		FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_ZXSPECTRUM_FILES | FSFT_FILE_ATTRIBUTES | FSFT_CASE_SENSITIVE_FILENAMES);
	}	

	~CFSCPMHC()
	{
		/*
		if (Disk != NULL)
		{
			delete Disk;
			Disk = NULL;
		}
		*/
	}

	virtual CFile* FindNext();	
	virtual CFile* NewFile(char* name, long len = 0, byte* data = NULL);	
	//Adds the HC file header before writing to disk.	
	virtual bool WriteFile(CFile* file);
	virtual bool ReadFile(CFile* file);
	virtual bool IsCharValidForFilename(char c);
	//virtual bool CreateFSName(CFile* file, char* fNameOut);
	virtual bool OpenFile(CFile* file);

protected:
	vector<CFileHC> HCFileList;
	virtual bool ReadDirectory();
};

#endif