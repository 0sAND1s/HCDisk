//////////////////////////////////////////////////////////////////////////
//Wrapper class for tape file archive: TAP or TZX
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
		FSFeatures = (FileSystemFeature)(FSFT_TAPE | FSFT_ZXSPECTRUM_FILES | FSFT_CASE_SENSITIVE_FILENAMES);
		NAME_LENGHT = 10;
		EXT_LENGTH = 0;
		theTap = NULL;
	};

	virtual ~CFileArchiveTape()
	{
		Close();
	}
		
	virtual bool Init();
	virtual CFile* NewFile(const char* name, long len = 0, byte* data = NULL);
	virtual bool WriteFile(CFile*);
	virtual bool Open(char* name, bool create = false);
	virtual CFile* FindFirst(char* pattern);
	virtual CFile* FindNext();		
	virtual bool AddFile(CFileSpectrumTape* fSpec);
	virtual bool Close();
	virtual bool ReadFile(CFile* file);		
	bool HasStandardBlocksOnly() { return theTap->HasStandardBlocksOnly(); }
	bool IsTZX() { return theTap->IsTZX(); };

	CTapFile* theTap;
protected:	
	word CurIdx;	
	char FindPattern[MAX_FILE_NAME_LEN];	
	CFileSpectrumTape* GetBlock();
	CFileSpectrumTape* TapeBlock2FileSpectrum(CTapeBlock* tbHdr, CTapeBlock* tbData);
	bool FileSpectrum2TapeBlock(CFileSpectrumTape* fSpec, CTapeBlock& tbHdr, CTapeBlock& tbData);	
	vector<CFileSpectrumTape*> TapeFiles;
	FileNameType blankNameTemplate = "noname%02d";	
};

class CFileSpectrumTape: public CFileSpectrum, public CFile
{
public:	
	virtual bool GetFileName(char* dest, bool trim = true)
	{
		bool res = CFile::GetFileName(dest, trim);
		//Trim according to tape block lenght.
		dest[CTapeBlock::TAP_FILENAME_LEN] = 0;
		return res;
	}
	
	CFileSpectrumTape(): CFile(), CFileSpectrum()
	{
	}

	CFileSpectrumTape(const CFileSpectrum& src1, const CFile& src2): CFileSpectrum(src1), CFile(src2)
	{
	}

	CFileSpectrumTape(const CFileSpectrumTape& src)
	{
		*(CFile*)this = src;
		*(CFileSpectrum*)this = src;
	}	

	virtual ~CFileSpectrumTape()
	{		
		if (this->buffer != NULL)
		{
			delete[] buffer;
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