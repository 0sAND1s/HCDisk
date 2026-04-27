#include <cstdio>
#include <cmath>
#include <memory.h>

#include "CFileCPM.h"
#include "CFSCPM.h"

#include <algorithm>

CFileCPM::CFileCPM(CFSCPM* fs)
{	
	this->fs = fs;	
	this->User = 0;
}

CFileCPM::CFileCPM()
{
	this->fs = NULL;	
	this->User = 0;
}

CFileCPM::CFileCPM(const CFileCPM& src)
{
	this->fs = src.fs;

	strcpy(this->FileName, src.FileName);	
	strcpy(this->Name, src.Name);	
	strcpy(this->Extension, src.Extension);	

	this->FileBlocks = src.FileBlocks;
	this->FileDirEntries = src.FileDirEntries;
	this->User = src.User;	
}

CFileCPM::~CFileCPM()
{	
}

CFileCPM CFileCPM::operator= (const CFileCPM& src)
{
	this->fs = src.fs;
	
	strcpy(this->FileName, src.FileName);
	strcpy(this->Name, src.Name);	
	strcpy(this->Extension, src.Extension);	

	this->FileBlocks = src.FileBlocks;
	this->FileDirEntries = src.FileDirEntries;
	this->User = src.User;	
		
	return *this;
}

int CFileCPM::operator== (const CFileCPM& src) const
{
	return string(this->FileName) == string(src.FileName) && src.User == this->User;
}
