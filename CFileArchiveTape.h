//////////////////////////////////////////////////////////////////////////
//Base class for tape file archive
//Author: George Chirtoaca, 2014
//////////////////////////////////////////////////////////////////////////

#ifndef CFILE_ARCHIVE_TAPE_H
#define CFILE_ARCHIVE_TAPE_H

#include "types.h"
#include "CFileArchive.h"
#include "CFileSpectrum.h"
#include "Tape\tap.h"
#include "Tape\TZX.h"

class CFileSpectrumTape;

class CFileArchiveTape: public CFileArchive
{
	friend class CFile;

public:
	CFileArchiveTape(char* name): CFileArchive(name) 
	{
		FSFeatures = (FileSystemFeature)(FSFT_ZXSPECTRUM_FILES | FSFT_CASE_SENSITIVE_FILENAMES);
		NAME_LENGHT = 10;
		EXT_LENGTH = 0;
	};

	virtual ~CFileArchiveTape()
	{
		Close();
	}

	virtual bool Init();
	virtual bool Open(char* name, bool create = false);
	virtual CFile* FindFirst(char* pattern);
	virtual CFile* FindNext();		
	virtual bool AddFile(CFileSpectrumTape* fSpec);
	virtual bool Close();
	virtual bool ReadFile(CFile* file);	

	CTapFile* theTap;
protected:	
	word CurIdx;	
	char FindPattern[MAX_FILE_NAME_LEN];
	CFileSpectrumTape* GetBlock();
	CFileSpectrumTape* TapeBlock2FileSpectrum(CTapeBlock* tbHdr, CTapeBlock* tbData);
	bool FileSpectrum2TapeBlock(CFileSpectrumTape* fSpec, CTapeBlock& tbHdr, CTapeBlock& tbData);	
	vector<CFileSpectrumTape> TapeFiles;
};

class CFileSpectrumTape: public CFileSpectrum, public CFile
{
public:
	virtual bool SetFileName(char* src)
	{
		return CFileArchive::TrimEnd(src) && CFile::SetFileName(src);
	}
	
	CFileSpectrumTape()
	{
	}

	CFileSpectrumTape(const CFileSpectrum& src1, const CFile& src2): CFileSpectrum(src1), CFile(src2)
	{
	}

	~CFileSpectrumTape()
	{
		if (this->buffer != NULL)
		{
			delete buffer;
			buffer = NULL;
		}
	}

	virtual dword GetLength()
	{
		return SpectrumLength;
	}
	
	virtual bool Read() {return true; };	
};
#endif//CFILE_ARCHIVE_TAPE_H