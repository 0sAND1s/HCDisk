//////////////////////////////////////////////////////////////////////////
//+3 BASIC driver, (C) George Chirtoaca 2014
//Specs: http://www.worldofspectrum.org/ZXSpectrum128+3Manual/chapter8pt27.html
//////////////////////////////////////////////////////////////////////////

#ifndef _CFS_CPM_PLUS3_H_
#define _CFS_CPM_PLUS3_H_

#include "CFSCPM.h"
#include "CFilePlus3.h"

class CFSBASICPlus3: public CFSCPM
{
	friend class CFileCPMPlus3;

public:
	CFSBASICPlus3(CDiskBase* disk, FSParamsType fsParams, CPMParams params, char* name): CFSCPM(disk, fsParams, params,  name)
	{		
		NAME_LENGHT = 8;
		EXT_LENGTH = 3;
		FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_ZXSPECTRUM_FILES | FSFT_FILE_ATTRIBUTES | FSFT_FOLDERS | FSFT_AUTORUN);
	}	

	virtual ~CFSBASICPlus3() {};

	virtual bool ReadDirectory();
	virtual CFile* FindNext();
	virtual CFile* NewFile(const char* name, long len = 0, byte* data = NULL);
	virtual bool ReadFile(CFile* file);
	virtual bool WriteFile(CFile* file);
	virtual bool OpenFile(CFile* file);
	virtual bool GetAutorunFilename(char** fileName) { *fileName = "DISK"; return true; }

protected:
	vector<CFilePlus3> Plus3FileList;
};

#endif