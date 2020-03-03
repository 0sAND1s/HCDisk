//////////////////////////////////////////////////////////////////////////
//SCL File system driver. (C) Chirtoaca George 2014 
//Specs used: http://zx-modules.de/fileformats/sclformat.html
//////////////////////////////////////////////////////////////////////////
#ifndef _CFSTRDSCL_H_
#define _CFSTRDSCL_H_

#include <stdio.h>
#include "CFSTRD.h"

class CFSTRDSCL: public CFileArchive
{
	friend class CFileTRD;

public:
	CFSTRDSCL(char* imgFile, char* name);
	~CFSTRDSCL();
	bool Init();	
	virtual CFile* FindFirst(char* pattern = "*");		
	virtual CFile* FindNext();	
	virtual bool ReadFile(CFile*);	
	virtual bool OpenFile(CFile* file);

protected:
	FILE* imgFile;
	char* imgFileName;
	std::vector<CFileTRD> TRD_Files;
	byte dirEntIdx;
	char* FindPattern;
	const static byte MAX_DIR_ENTRIES = 128;
	const static word SECT_SIZE = 256;
	CFSTRDOS::DirEntryType TRD_Directory[MAX_DIR_ENTRIES];	
	
	bool ReadBlock(CFileTRD* file, word sectIdx, byte* destBuf);
};

#endif