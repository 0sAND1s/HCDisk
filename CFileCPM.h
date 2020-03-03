#ifndef _CFILECPM_H
#define _CFILECPM_H

#include "types.h"
#include "CFile.h"
#include <vector>

//disk file structure
class CFileCPM: public CFile
{
	friend class CFSCPM;	

public:
	static const byte MAX_FILE_NAME_LEN = 11;
	static const byte MAX_FILE_DIR_ENTRIES = 16;
	
	typedef std::vector<word> IdxType;
	typedef char FileNameType[MAX_FILE_NAME_LEN+2];

#pragma pack(1)		
	byte User;	
	word Idx;	
	
	CFileCPM();
	CFileCPM(CFSCPM* fs);
	CFileCPM(const CFileCPM& src);
	~CFileCPM();
	
	virtual CFileCPM operator= (const CFileCPM&);
	virtual int operator== (const CFileCPM&) const;
	virtual int operator!= (const CFileCPM& src) const {return !(*this == src);};
protected:			
	CFSCPM* fs;	
	bool isOpened;		
#pragma pack()
};


#endif//_CFILECPM_H