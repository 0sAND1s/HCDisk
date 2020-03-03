#include "CFileArchiveTape.h"
#include "CFileSpectrum.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK , __FILE__ , __LINE__ )	
#endif
#endif


bool CFileArchiveTape::Init()
{	
	if (strstr(Name, ".TAP"))
		theTap = new CTapFile();	
	else if (strstr(Name, ".TZX"))
		theTap = new CTZXFile();	

	bool res = theTap->Open(Name);
	
	if (res)
	{
		CFileSpectrumTape* fs = NULL;	
		TapeFiles.clear();

		CurIdx = 0;
		fs = GetBlock();
		while (fs != NULL)
		{
			TapeFiles.push_back(*fs);
			delete fs;
			fs = GetBlock();
		}	
		
	}

	return res;
}

bool CFileArchiveTape::Open(char* name, bool create)
{
	strupr(name);		
	theTap = new CTapFile();					
	return theTap->Open(name, create ? CTapFile::TAP_OPEN_NEW : CTapFile::TAP_OPEN_EXISTING);	
}

bool CFileArchiveTape::Close()
{
	if (theTap != NULL)
	{
		theTap->Close();
		delete theTap;
		theTap = NULL;

		return true;
	}
	else
		return false;
}

CFile* CFileArchiveTape::FindFirst(char* pattern)
{
	//theTap->Seek(0);
	strcpy(FindPattern, pattern);
	CurIdx = 0;	

	return FindNext();
}

CFile* CFileArchiveTape::FindNext()
{	
	CFileSpectrumTape* fs = NULL;	
	FileNameType fn;
	bool bNameMatch = false;			

	while (CurIdx < TapeFiles.size() && !bNameMatch)
	{	
		fs = &TapeFiles[CurIdx];
		bNameMatch = (fs->GetFileName((char*)&fn) && TrimEnd((char*)&fn) && WildCmp(FindPattern, fn));
		CurIdx++;
	}	
	
	
	if (!bNameMatch)
	{
		LastError = CFileArchive::ERR_FILE_NOT_FOUND;
		fs = NULL;
	}

	return (CFile*)fs;
}

CFileSpectrumTape* CFileArchiveTape::GetBlock()
{
	CTapeBlock tbHdr, tbData;
	CFileSpectrumTape* fs = NULL;

	if (CurIdx < theTap->GetBlockCount())
		do
		{
			if (CurIdx == 0 ? theTap->GetFirstBlock(&tbHdr) : theTap->GetNextBlock(&tbHdr))
			{
				if (tbHdr.m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
					if(tbHdr.IsBlockHeader())
					{
						if (theTap->GetNextBlock(&tbData))
							fs = TapeBlock2FileSpectrum(&tbHdr, &tbData);									
					}		
					else
						fs = TapeBlock2FileSpectrum(NULL, &tbHdr);										
			}
			else
				fs = NULL;

			CurIdx++;
		}	
		while (CurIdx < theTap->GetBlockCount() && tbHdr.m_BlkType == CTapeBlock::TAPE_BLK_METADATA);

	return fs;
}

CFileSpectrumTape* CFileArchiveTape::TapeBlock2FileSpectrum(CTapeBlock* tbHdr, CTapeBlock* tbData)
{
	CFileSpectrumTape* fs = new CFileSpectrumTape();
	FileNameType fn;

	if (tbHdr != NULL)
	{				
		CTapeBlock::TapeBlockHeaderBasic* th = (CTapeBlock::TapeBlockHeaderBasic*)tbHdr->Data;

		memset(fn, 0, sizeof(fn));
		tbHdr->GetName((char*)&fn);
		TrimEnd((char*)&fn);
		if (strlen((char*)&fn) == 0)
			sprintf(fn, "!noname%02d!", CurIdx+1);
		fs->SetFileName(fn);		
		fs->SpectrumType = (CFileSpectrum::SpectrumFileType)th->BlockType;
		fs->SpectrumLength = th->Length;		
		fs->SpectrumStart = th->Param1;
		if (fs->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
			fs->SpectrumVarLength = fs->SpectrumLength - th->Param2;
		else if (fs->SpectrumType == CFileSpectrum::SPECTRUM_CHAR_ARRAY || 
			fs->SpectrumType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
			fs->SpectrumArrayVarName = th->Param2 & 0x00FF;
	}	
	else
	{
		fs->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
		sprintf(fn, "!nohead%02d!", CurIdx+1);
		fs->SetFileName(fn);		
	}

	if (tbData != NULL)		
	{
		fs->SetData(tbData->Data, tbData->Length-2);
		fs->SpectrumLength = (word)tbData->Length - 2;
	}

	return fs;
}

bool CFileArchiveTape::FileSpectrum2TapeBlock(CFileSpectrumTape* fSpec, CTapeBlock& tbHdr, CTapeBlock& tbData)
{	
	FileNameType fn;

	if (fSpec->GetType() != CFileSpectrum::SPECTRUM_UNTYPED)
	{
		//tbHdr = new CTapeBlock();		
		tbHdr.Data = (byte*)new CTapeBlock::TapeBlockHeaderBasic();
		CTapeBlock::TapeBlockHeaderBasic* th = (CTapeBlock::TapeBlockHeaderBasic*)tbHdr.Data;
		tbHdr.Length = sizeof(CTapeBlock::TapeBlockHeaderBasic);		
		fSpec->GetFileName(fn);
		tbHdr.SetName(fn);
		th->BlockType = fSpec->GetType();
		th->Length = fSpec->SpectrumLength;
		th->Param1 = fSpec->SpectrumStart;
		if (th->BlockType == CFileSpectrum::SPECTRUM_PROGRAM)
			th->Param2 = fSpec->SpectrumLength - fSpec->SpectrumVarLength;
		else if (th->BlockType == CFileSpectrum::SPECTRUM_CHAR_ARRAY || 
			th->BlockType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
			th->Param2 = fSpec->SpectrumArrayVarName;
		else if (th->BlockType == CFileSpectrum::SPECTRUM_BYTES)
			th->Param2 = 32768;		
	}
	
	tbData.Data = new byte[fSpec->SpectrumLength];
	tbData.Length = fSpec->SpectrumLength;		
	fSpec->GetData(tbData.Data);
	
	return true;
}


bool CFileArchiveTape::AddFile(CFileSpectrumTape* fSpec)
{
	CTapeBlock tbHdr, tbData;

	FileSpectrum2TapeBlock(fSpec, tbHdr, tbData);	
	theTap->AddTapeBlock(tbHdr.Data, (word)tbHdr.Length, CTapeBlock::TAPE_BLOCK_FLAG_HEADER);
	theTap->AddTapeBlock(tbData.Data, (word)tbData.Length, CTapeBlock::TAPE_BLOCK_FLAG_DATA);	
	return true;
}

bool CFileArchiveTape::ReadFile(CFile* file)
{
	//The file is already in memory.
	return true;
}