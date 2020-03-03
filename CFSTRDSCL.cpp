#include "CFSTRDSCL.h"
#include "CFileTRD.h"
#include "DiskImgRaw.h"

CFSTRDSCL::CFSTRDSCL(char* imgFile, char* name)	
{
	this->imgFileName = imgFile;
	strcpy(this->Name, name);
	FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_ZXSPECTRUM_FILES & ~FSFT_LABEL);
}

CFSTRDSCL::~CFSTRDSCL()
{
	if (imgFile != NULL)
		fclose(imgFile);
}

bool CFSTRDSCL::Init()
{
	bool res = true;

	this->imgFile = fopen(this->imgFileName, "rb+");
	res = imgFile != NULL;
	
	TRD_Files.clear();

	//Check signature.
	if (res)
	{
		char signature[8];
		res = (fread(&signature, sizeof(signature), 1, imgFile) == 1);
		if (res)
			res = memcmp(signature, "SINCLAIR", sizeof(signature)) == 0;
	}

	
	//Read file catalog.
	if (res)
	{
		byte fileCnt = fgetc(imgFile);
		bool bDirFinished = false;
		word filledSectCnt = 0;		

		for (byte fIdx = 0; fIdx < fileCnt && !bDirFinished; fIdx++)
		{			
			res = fread(&TRD_Directory[fIdx], sizeof(CFSTRDOS::DirEntryType) - 2, 1, imgFile) == 1;
			CFSTRDOS::DirEntryType* dirEnt = &TRD_Directory[fIdx];						
			bDirFinished = dirEnt->FileName[0] == 0;

			if (res && !bDirFinished && dirEnt->FileName[0] != CFSTRDOS::DEL_FLAG)
			{				 
				CFileTRD file(this);
				file.FileDirEntry = fIdx;				
				file.Length = dirEnt->LenghtSect * SECT_SIZE;//DiskDefinition.SectSize;				
				file.SectorCnt = dirEnt->LenghtSect;	
				file.StartSector = 0;	//Not available for SCL.
				file.StartTrack = 0;	//Not available for SCL.
				CreateFileName(dirEnt->FileName, &file);

				TRD_Files.push_back(file);							
			}
		}		
	}

	return res;
}

CFile* CFSTRDSCL::FindFirst(char* pattern)
{	
	dirEntIdx = 0;
	FindPattern = pattern;

	return FindNext();
}

CFile* CFSTRDSCL::FindNext()
{
	bool bFound = false;
	CFileTRD* file = NULL;

	while (dirEntIdx < TRD_Files.size() && !bFound)
	{							
		bFound = WildCmp(FindPattern, TRD_Files[dirEntIdx].FileName);		
		if (bFound)
			file = &TRD_Files[dirEntIdx];
		dirEntIdx++;
	}

	if (file == NULL)
		LastError = CFileSystem::ERR_FILE_NOT_FOUND;

	return file;
}

bool CFSTRDSCL::OpenFile(CFile* file)
{		
	CFileTRD* f = (CFileTRD*)file;
	f->buffer = new byte[SECT_SIZE];
	ReadBlock(f, f->StartSector, f->buffer);
	switch (f->Extension[0])
	{
	case 'B':	
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;		
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].BasicProgVarLength;			
		f->CFileSpectrum::SpectrumStart = f->GetBASStartLine();
		f->CFileSpectrum::SpectrumVarLength = TRD_Directory[f->FileDirEntry].BasicProgVarLength - TRD_Directory[f->FileDirEntry].Length;
		break;
	case 'C':
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].Length;
		f->CFileSpectrum::SpectrumStart = TRD_Directory[f->FileDirEntry].CodeStart;		
		break;
	case 'D':
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_NUMBER_ARRAY;
		f->CFileSpectrum::SpectrumStart = 0;
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].Length;

		break;
	case '#':
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_NUMBER_ARRAY;
		f->CFileSpectrum::SpectrumStart = TRD_Directory[f->FileDirEntry].PrintExtent;
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].Length;		
		break;
	default:						
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_BYTES;					
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].Length;
		f->CFileSpectrum::SpectrumStart = TRD_Directory[f->FileDirEntry].CodeStart;		
		break;		
	}

	f->Length = GetFileSize(f, true);
	delete f->buffer;
	f->buffer = NULL;

	return true;
}

bool CFSTRDSCL::ReadFile(CFile* file)
{	
	CFileTRD* f = (CFileTRD*)file;
	word sectSz = SECT_SIZE;
	word len = f->SectorCnt * sectSz;		
	f->buffer = new byte[len];
	bool res = true;

	for (word sectIdx = 0; sectIdx < f->SectorCnt && res; sectIdx++)
	{
		word blockIdx = f->StartTrack * 16 + f->StartSector + sectIdx;
		res = ReadBlock(f, blockIdx, f->buffer + (sectIdx * sectSz));
	}
	return res;
}


bool CFSTRDSCL::ReadBlock(CFileTRD* file, word sectIdx, byte* destBuf)
{	
	word logicalStartSect = sectIdx;
	for (byte fileIdx = 0; fileIdx < file->FileDirEntry; fileIdx++)
		logicalStartSect += TRD_Directory[fileIdx].LenghtSect;

	word imgOffset = 8 + 1 + TRD_Files.size() * (sizeof(CFSTRDOS::DirEntryType) - 2) + (logicalStartSect * SECT_SIZE);
	bool res = fseek(imgFile, imgOffset, SEEK_SET) == 0;

	if (res)
		res = fread(destBuf, SECT_SIZE, 1, imgFile) == 1;

	return res;
}
