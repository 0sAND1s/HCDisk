//////////////////////////////////////////////////////////////////////////
//Purpose	: HC BASIC file system driver
//Author	: George Chirtoaca, 2014
//Specs		: reverse engineering
//////////////////////////////////////////////////////////////////////////

#ifndef _CFSCPMHC_H_
#define _CFSCPMHC_H_

#include "CFSCPM.h"
#include "CFileHC.h"

class CFSHCBasic: public CFSCPM
{
public:
	CFSHCBasic(CDiskBase* disk, FSParamsType fsParams, CPMParams params, char* name): CFSCPM(disk, fsParams, params,  name)
	{
		EXT_LENGTH = 0;
		NAME_LENGHT = 11;
		//Folders flag is not supported by HC BASIC (files are ignored if on user area > 0). But we still show them, to benefit the undelete feature.
		FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_ZXSPECTRUM_FILES | FSFT_FILE_ATTRIBUTES | FSFT_CASE_SENSITIVE_FILENAMES | FSFT_AUTORUN);
		//FSFeatures = (FileSystemFeature)(FSFeatures & ~FSFT_FOLDERS);
	}	

	virtual CFile* FindNext();	
	virtual CFile* NewFile(const char* name, long len = 0, byte* data = NULL);	
	//Adds the HC file header before writing to disk.	
	virtual bool WriteFile(CFile* file);
	virtual bool ReadFile(CFile* file);
	virtual bool IsCharValidForFilename(char c);
	//virtual bool CreateFSName(CFile* file, char* fNameOut);
	virtual bool OpenFile(CFile* file);	
	virtual bool GetAutorunFilename(char** fileName) { *fileName = "run"; return true; }

protected:
	vector<CFileHC> HCFileList;
	virtual bool ReadDirectory();
};

#endif