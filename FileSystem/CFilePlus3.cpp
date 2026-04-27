#include "CFilePlus3.h"

const char CFilePlus3::SIGNATURE[9] = {'P', 'L', 'U', 'S', '3', 'D', 'O', 'S', '\x1A'};

CFilePlus3::CFilePlus3(CFSCPM* fs): CFileCPM(fs)
{
	memset(&TheHeader, 0, sizeof(TheHeader));
	hasHeader = false;
	this->fs = fs;
}

CFilePlus3::CFilePlus3(const CFilePlus3& src): CFileCPM(src)
{
	this->fs = src.fs;
	this->FileBlocks = src.FileBlocks;
	this->FileDirEntries = src.FileDirEntries;
}

CFilePlus3::CFilePlus3(const CFileCPM& src): CFileCPM(src)
{	
	this->fs = ((CFilePlus3&)src).fs;
	this->FileBlocks = ((CFilePlus3&)src).FileBlocks;
	this->FileDirEntries = ((CFilePlus3&)src).FileDirEntries;
}



dword CFilePlus3::GetLength()
{
	if (this->SpectrumType != CFileSpectrum::SPECTRUM_UNTYPED)
		return SpectrumLength;
	else
		return this->Length;
}