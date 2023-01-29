#include "CFileArchiveTape.h"
#include "CFileSpectrum.h"
#include <algorithm>
#include <set>

#ifdef _MSC_VER
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK , __FILE__ , __LINE__ )	
#endif
#endif


/// <summary>
/// Will return blocks as higher level CFileSpectrumTape, based on the TAP/TZX tape blocks.
/// </summary>
/// <returns></returns>
bool CFileArchiveTape::Init()
{		
	bool res = true;
	if (theTap == nullptr)
		res = Open(Name);

	//res = res && theTap->Open(Name);
	
	if (res)
	{
		CFileSpectrumTape* tapeBlock = NULL;	

		//Cleanup from prev reading.
		if (TapeFiles.size() > 0)
		{
			for (auto block : TapeFiles)
				delete block;
			TapeFiles.clear();
		}		

		CurIdx = 0;
		tapeBlock = GetBlock();
		//Create unique names for blank named blocks, that's still related to the first block name.							
		if (tapeBlock != NULL)
		{
			tapeBlock->GetFileName(blankNameTemplate, true);
			byte idxOffset = std::min((byte)strlen(blankNameTemplate), (byte)(CTapeBlock::TAP_FILENAME_LEN - 1));
			sprintf(&blankNameTemplate[idxOffset], "%s", "%d");
		}		

		while (tapeBlock != NULL)
		{
			TapeFiles.push_back(tapeBlock);			
			
			tapeBlock = GetBlock();
		}					

		//Set unique names for blocks with identical names.
		set<string> names;
		CFileArchive::FileNameType fn;
		for (unsigned i = 0; i < TapeFiles.size(); i++)
		{
			TapeFiles[i]->GetFileName(fn);
			if (names.find(fn) == names.end())
			{
				names.insert(fn);
			}
			else
			{
				byte idxOffset = std::min((byte)strlen(fn), (byte)(CTapeBlock::TAP_FILENAME_LEN - 1));
				sprintf(&fn[idxOffset], "%d", i);
				TapeFiles[i]->SetFileName(fn);
			}
		}
	}

	return res;
}

bool CFileArchiveTape::Open(char* name, bool create)
{
	char nameUp[_MAX_PATH];
	strncpy(nameUp, name, sizeof(nameUp));
	strupr(nameUp);

	if (strstr(nameUp, ".TAP"))
		theTap = new CTapFile();
	else if (strstr(nameUp, ".TZX"))
		theTap = new CTZXFile();
	else
		return false;

	bool isOpen = theTap->Open(name, create ? CTapFile::TAP_OPEN_NEW : CTapFile::TAP_OPEN_EXISTING);
	//bool isInit = !create ? Init() : true;
	return isOpen/* && isInit*/;
}

bool CFileArchiveTape::Close()
{
	for (auto block : TapeFiles)
		delete block;
	TapeFiles.clear();

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
		fs = TapeFiles[CurIdx];
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

/// <summary>
/// Locates ONE next block that is a data block (not medatada).
/// </summary>
/// <returns></returns>
CFileSpectrumTape* CFileArchiveTape::GetBlock()
{
	CTapeBlock tbHdr, tbData;
	CFileSpectrumTape* fs = NULL;	
	
	do
	{
		bool blockOK = (CurIdx == 0 ? theTap->GetFirstBlock(&tbHdr) : theTap->GetNextBlock(&tbHdr));
			
		if (blockOK)
		{
			//Convert standard and turbo blocks only, the others are just pulses that we can't make sense of.
			if (tbHdr.m_BlkType == CTapeBlock::TAPE_BLK_STD || tbHdr.m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
			{
				if (tbHdr.IsBlockHeader())
				{
					if (theTap->GetNextBlock(&tbData))
						fs = TapeBlock2FileSpectrum(&tbHdr, &tbData);					
				}
				else
					fs = TapeBlock2FileSpectrum(NULL, &tbHdr);
			}
		}
		else
			fs = NULL;

		CurIdx++;
	} while (CurIdx < theTap->GetBlockCount() && tbHdr.m_BlkType == CTapeBlock::TAPE_BLK_METADATA);

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
			sprintf(fn, blankNameTemplate, CurIdx+1);
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
		sprintf(fn, blankNameTemplate, CurIdx+1);
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
		//Tape block destructor will free buffer.
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
	
	//Tape block destructor will free buffer.
	tbData.Data = new byte[fSpec->SpectrumLength];
	tbData.Length = fSpec->SpectrumLength;			
	
	return fSpec->GetData(tbData.Data);
}


bool CFileArchiveTape::AddFile(CFileSpectrumTape* fSpec)
{
	CTapeBlock tbHdr, tbData;
	bool res = true;

	res = FileSpectrum2TapeBlock(fSpec, tbHdr, tbData);	
	if (res)
		res = theTap->AddTapeBlock(tbHdr.Data, (word)tbHdr.Length, CTapeBlock::TAPE_BLOCK_FLAG_HEADER) &&
			theTap->AddTapeBlock(tbData.Data, (word)tbData.Length, CTapeBlock::TAPE_BLOCK_FLAG_DATA);	
		
	//Re-index tape after a block was appended.
	if (res)
		res = Init();

	return res;
}

bool CFileArchiveTape::ReadFile(CFile* file)
{
	//The file is already in memory.
	return true;
}

CFile* CFileArchiveTape::NewFile(const char* name, long len, byte* data)
{
	CFileSpectrumTape* fSpec = new CFileSpectrumTape();
	fSpec->SetFileName(name);
	if (data != NULL)
		fSpec->SetData(data, len);

	return dynamic_cast<CFile*>(fSpec);
}

bool CFileArchiveTape::WriteFile(CFile* file)
{
	return AddFile(dynamic_cast<CFileSpectrumTape*>(file));
}
