#ifndef _CFILETRD_H_
#define _CFILETRD_H_

#include "CFile.h"
#include "types.h"
#include "CFSTRD.h"

class CFileTRD: public CFile, public CFileSpectrum
{	
	friend class CFSTRDOS;
	friend class CFSTRDSCL;

public:	
	CFileTRD(CFileArchive* fs);		
	virtual ~CFileTRD(){};

protected:	
	word FileDirEntry;
	byte StartTrack;
	byte StartSector;	
	byte SectorCnt;
	CFileArchive* fs;

	word GetBASStartLine();		
};

#endif//_CFILETRD_H_