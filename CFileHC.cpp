#include "CFileHC.h"

CFileHC::CFileHC(CFSCPM* fs): CFileCPM(fs)
{	
}

CFileHC::CFileHC(const CFileHC& src): CFileCPM(src)
{
}

CFileHC::CFileHC(const CFileCPM& src): CFileCPM(src)
{
}


dword CFileHC::GetLength()
{
	if (GetType() == SPECTRUM_UNTYPED)
		return this->Length;	
	else
		return this->SpectrumLength;
}
