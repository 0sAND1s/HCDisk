#ifndef _CFILEHC_H_
#define _CFILEHC_H_

#include "types.h"
#include "CFSCPM.h"
#include "CFileCPM.h"
#include "CFileSpectrum.h"
#include "CFileIF1.h"
#include <memory>

class CFileHC: public CFileCPM, public CFileSpectrum, public CFileIF1
{
	friend class CFSCPMHC;
public:
	CFileHC(CFSCPM* fs);
	CFileHC(const CFileHC&);
	CFileHC(const CFileCPM&);		
	virtual dword GetLength();	
};

#endif