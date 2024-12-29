#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include "CFSTRD.h"
#include "CFileTRD.h"
#include <algorithm>

const CDiskBase::DiskDescType CFSTRDOS::TRDDiskTypes[] = 
{
	{ 80, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5 },
	{ 40, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_5_25 },
	{ 80, 1, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5 },
	{ 40, 1, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_5_25 }
};

CFSTRDOS::CFSTRDOS(CDiskBase* theDisk, char* name) : CFileSystem(theDisk, name)
{
	NAME_LENGHT = 8;
	EXT_LENGTH = 1;
	FSFeatures = (FileSystemFeature)(FSFT_DISK | FSFT_ZXSPECTRUM_FILES | FSFT_CASE_SENSITIVE_FILENAMES | FSFT_LABEL | FSFT_AUTORUN);

	FSParams.DirEntryCount = MAX_DIR_ENTRIES;
	FSParams.BlockSize = Disk->DiskDefinition.SectSize;
	FSParams.BlockCount = (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - 1) * Disk->DiskDefinition.SPT;
}

bool CFSTRDOS::Init()
{		
	if (Disk == NULL)
		return false;
	
	bool bFoundGeom = false;
	byte gIdx;
	for (gIdx = 0; gIdx < sizeof(TRDDiskTypes)/sizeof(TRDDiskTypes[0]) && !bFoundGeom; gIdx++)
		bFoundGeom = Disk->DiskDefinition.TrackCnt == TRDDiskTypes[gIdx].TrackCnt &&
			Disk->DiskDefinition.SideCnt == TRDDiskTypes[gIdx].SideCnt &&
			Disk->DiskDefinition.SPT == TRDDiskTypes[gIdx].SPT &&
			Disk->DiskDefinition.SectSize == TRDDiskTypes[gIdx].SectSize &&
			Disk->DiskDefinition.Density == TRDDiskTypes[gIdx].Density;

	if (!bFoundGeom)
		return false;	

	if (!Disk->ReadSectors((byte*)&diskDescTRD, 0, 0, DISK_INFO_SECTOR, 1))
		return false;
	
	if (diskDescTRD.TRDosID != TRDOS_ID)
		return false;

	if ((diskDescTRD.DiskType != DTI_TRK_80_S2 && diskDescTRD.DiskType != DTI_TRK_40_S2 && 
		diskDescTRD.DiskType != DTI_TRK_80_S1 && diskDescTRD.DiskType != DTI_TRK_40_S1) 
		|| (gIdx - 1) != (diskDescTRD.DiskType - DTI_TRK_80_S2))
		return false;	
	
	if (!ReadDirectory())
		return false;
	
	return true;
}

dword CFSTRDOS::GetDiskLeftSpace()
{
	if (Disk == NULL)
		return 0;
	
	return diskDescTRD.FreeSectCnt * Disk->DiskDefinition.SectSize;
}

dword CFSTRDOS::GetDiskMaxSpace()
{
	if (Disk == NULL)
		return 0;

	//1st track is reserved.
	return (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - 1) * Disk->DiskDefinition.SPT *
		Disk->DiskDefinition.SectSize;
}

bool CFSTRDOS::ReadDirectory()
{
	DirEntryType dirEnt;
	bool bDirFinished = false;
	CFileTRD file(this);
	//FileNameType fn;

	TRD_Files.clear();
	FS_DirEntryMap.clear();
	FS_BlockMap.clear();

	Disk->ReadSectors((byte*)&TRD_Directory, 0, 0, 1, 
		MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType)/Disk->DiskDefinition.SectSize);	

	for (word dirIdx = 0; dirIdx < MAX_DIR_ENTRIES; dirIdx++)
		FS_DirEntryMap.push_back(false);

	for (word auIdx = 0; auIdx < FSParams.BlockCount; auIdx++)
		FS_BlockMap.push_back(false);

	for (dirEntIdx = 0; dirEntIdx < MAX_DIR_ENTRIES && !bDirFinished; dirEntIdx++)		
	{
		dirEnt = *(DirEntryType*)&TRD_Directory[dirEntIdx];		 
		bDirFinished = dirEnt.FileName[0] == END_OF_DIR_MARKER;		
	
		if (!bDirFinished && dirEnt.FileName[0] != DEL_FLAG)		
		{
			file.FileDirEntry = dirEntIdx;					
			file.Length = dirEnt.Length;
			file.StartSector = dirEnt.StartSect;
			file.StartTrack = dirEnt.StartTrack;
			file.SectorCnt = dirEnt.LenghtSect;	

			FileNameType fn{};
			strncpy(fn, dirEnt.FileName, sizeof(dirEnt.FileName));
			TrimEnd(fn);
			sprintf(&fn[strlen(fn)], ".%c", dirEnt.FileExt);
			CreateFileName(fn, &file);
			
			
			FS_DirEntryMap[dirEntIdx] = true;
			for (word secIdx = 0; secIdx <= dirEnt.LenghtSect; secIdx++)
			{
				word logicalStartSect = file.StartTrack * Disk->DiskDefinition.SPT + file.StartSector;
				FS_BlockMap[logicalStartSect + secIdx] = true;
			}
			
			if (file.Extension[0] != 'C' && file.Extension[0] != 'B' && file.Extension[0] != 'D' &&
				file.Extension[0] != '#')
			{				 
				int c1 = (dirEnt.CodeStart & 0xFF);
				int c2 = dirEnt.CodeStart >> 8;

				if (isprint(c1) && isprint(c2))
				{
					if (c1 != ' ')
					{
						file.Extension[1] = c1;
						if (c2 != ' ')
							file.Extension[2] = c2;
						strcat(file.FileName, &file.Extension[1]);
					}					
				}
			}


			TRD_Files.push_back(file);											
		}
	}			
	
	return true;
}

bool CFSTRDOS::Format()
{
	bool res = true;
	res = CFileSystem::Format();

	if (res)
	{
		memset(&diskDescTRD, 0, sizeof(diskDescTRD));
		diskDescTRD.TRDosID = TRDOS_ID;		
		diskDescTRD.NextFreeSectTrack = 1;
		strcpy(diskDescTRD.DiskLabel, "HCDisk20");
		memset(diskDescTRD.Password, ' ', sizeof(diskDescTRD.Password));

		bool bFoundGeom = false;
		byte gIdx;
		for (gIdx = 0; gIdx < sizeof(TRDDiskTypes)/sizeof(TRDDiskTypes[0]) && !bFoundGeom; gIdx++)
			bFoundGeom = Disk->DiskDefinition.TrackCnt == TRDDiskTypes[gIdx].TrackCnt &&
				Disk->DiskDefinition.SideCnt == TRDDiskTypes[gIdx].SideCnt &&
				Disk->DiskDefinition.SPT == TRDDiskTypes[gIdx].SPT &&
				Disk->DiskDefinition.SectSize == TRDDiskTypes[gIdx].SectSize &&
				Disk->DiskDefinition.Density == TRDDiskTypes[gIdx].Density;

		if (bFoundGeom)
			diskDescTRD.DiskType = DTI_TRK_80_S2 + gIdx - 1;

		diskDescTRD.FreeSectCnt = Disk->DiskDefinition.SPT * (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - 1);			

		res = bFoundGeom;
	}
	
	if (res)
		res = Disk->WriteSectors(0, 0, DISK_INFO_SECTOR, 1, (byte*)&diskDescTRD);

	if (res)
	{
		memset(&TRD_Directory, 0, sizeof(TRD_Directory));
		res = Disk->WriteSectors(0, 0, 1, 
			MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType)/Disk->DiskDefinition.SectSize, 
			(byte*)&TRD_Directory);	
	}

	return res;
}

bool CFSTRDOS::Delete(char* names)
{
	byte delFilesCnt = 0, allFilesCnt = 0;
	CFileTRD* file = (CFileTRD*)FindFirst(names);
	while (file != nullptr)
	{
		DirEntryType* dirEnt = (DirEntryType*)&TRD_Directory[file->FileDirEntry];
		dirEnt->FileName[0] = DEL_FLAG;
		
		file = (CFileTRD*)FindNext();		
	}

	bool bDirFinished = false;	
	for (word dirIdx = 0; dirIdx < MAX_DIR_ENTRIES && !bDirFinished; dirIdx++)
	{
		DirEntryType* dirEnt = (DirEntryType*)&TRD_Directory[dirIdx];
		bDirFinished = (dirEnt->FileName[0] == END_OF_DIR_MARKER);

		if (!bDirFinished)
		{
			if (dirEnt->FileName[0] == DEL_FLAG)
				delFilesCnt++;
			
			allFilesCnt++;
		}		
	}

	bool res = true; 
	//res = Disk->ReadSectors((byte*)&diskDescTRD, 0, 0, DISK_INFO_SECTOR, 1);	
	diskDescTRD.DelFilesCnt = delFilesCnt;
	diskDescTRD.FileCnt = allFilesCnt;
	res = Disk->WriteSectors(0, 0, DISK_INFO_SECTOR, 1, (byte*)&diskDescTRD);
	if (!res)
		return res;
		
	res = Disk->WriteSectors(0, 0, 1,
		MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType) / Disk->DiskDefinition.SectSize,
		(byte*)&TRD_Directory);	
	if (!res)
		return res;

	res = ReadDirectory();

	return res;
}

bool CFSTRDOS::Rename(CFile* file, char* newName)
{
	bool res = true;	
	if (file == nullptr || newName == nullptr)
		return false;

	CFileTRD* fTRD = (CFileTRD*)file;
	DirEntryType* dirEnt = (DirEntryType*)&TRD_Directory[fTRD->FileDirEntry];

	res = this->CreateFileName(newName, fTRD);
	if (!res)
		return res;

	memset(dirEnt->FileName, ' ', sizeof(dirEnt->FileName));
	memcpy(dirEnt->FileName, fTRD->Name, min(strlen(fTRD->Name), sizeof(dirEnt->FileName)));
	dirEnt->FileExt = fTRD->Extension[0];
	if (dirEnt->FileExt != 'C' && dirEnt->FileExt != 'B' && dirEnt->FileExt != 'D' && dirEnt->FileExt != '#' &&
		isprint(fTRD->Extension[1]) && isprint(fTRD->Extension[2]))
	{
		dirEnt->CodeStart = ((word)fTRD->Extension[1] << 8) | fTRD->Extension[2];
	}

	res = Disk->WriteSectors(0, 0, 1,
		MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType) / Disk->DiskDefinition.SectSize,
		(byte*)&TRD_Directory);
	if (!res)
		return res;

	res = ReadDirectory();

	return res;
}

bool CFSTRDOS::ReadBlock(byte* destBuf, word sectIdx, byte sectCnt)
{	
	byte trk, side, sector;
	
	LogicalToPhysicalSector(sectIdx, &trk, &side, &sector);	
	return Disk->ReadSectors(destBuf, trk, side, sector, 1);
}

bool CFSTRDOS::WriteBlock(byte* srcBuf, word sectIdx, byte sectCnt)
{
	byte trk, side, sector;

	LogicalToPhysicalSector(sectIdx, &trk, &side, &sector);
	return Disk->WriteSectors(trk, side, sector, 1, srcBuf);
}

bool CFSTRDOS::ReadFile(CFileTRD* file)
{	
	word sectSz = Disk->DiskDefinition.SectSize;
	word len = file->SectorCnt * sectSz;		
	file->buffer = new byte[len];
	bool res = true;

	for (word sectIdx = 0; sectIdx < file->SectorCnt && res; sectIdx++)
	{
		word blockIdx = file->StartTrack * Disk->DiskDefinition.SPT + file->StartSector + sectIdx;
		res = ReadBlock(file->buffer + (sectIdx * sectSz), blockIdx);
	}
	return res;
}


bool CFSTRDOS::WriteFile(CFileTRD* file)
{
	bool res = true;

	//Set program start line on the last 4 bytes.	
	if (file->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
	{
		file->Length += 4;
		byte* buf = new byte[file->Length];
		memcpy(buf, file->buffer, file->Length - 4);
		delete[] file->buffer;
		file->buffer = buf;

		*(word*)&file->buffer[file->Length - 4] = PROGRAM_LINE_MARKER;
		*(word*)&file->buffer[file->Length - 2] = file->SpectrumStart;		
	}

	const byte fileSectorCount = (byte)ceil((float)file->Length / Disk->DiskDefinition.SectSize);

	//Allocate the last partial sector, using a clean buffer.	
	if (file->Length % Disk->DiskDefinition.SectSize != 0)
	{		
		word unusedLen = (word)(fileSectorCount * Disk->DiskDefinition.SectSize - file->Length);
		file->Length = fileSectorCount * Disk->DiskDefinition.SectSize;		
		byte* buf = new byte[file->Length];
		memcpy(buf, file->buffer, file->Length - unusedLen);
		delete[] file->buffer;
		file->buffer = buf;
		memset(&file->buffer[file->Length - unusedLen], 0, unusedLen);		
	}			
	
	const word freeSectCount = GetFreeBlockCount();
	if (freeSectCount < fileSectorCount)
	{
		LastError = ERR_NO_SPACE_LEFT;
		return false;
	}
	
	if (GetFreeDirEntriesCount() < 1)
	{
		LastError = ERR_NO_CATALOG_LEFT;
		return false;
	}	

	//Search next free dir entry.
	DirEntryType* dirEnt = nullptr;
	bool bDirFinished = false;
	for (dirEntIdx = 0; dirEntIdx < MAX_DIR_ENTRIES && !bDirFinished; dirEntIdx++)
	{
		dirEnt = (DirEntryType*)&TRD_Directory[dirEntIdx];
		bDirFinished = dirEnt->FileName[0] == END_OF_DIR_MARKER;
	}

	//Set spectrum parameters.
	switch (file->SpectrumType)
	{
	case CFileSpectrum::SPECTRUM_PROGRAM:
		dirEnt->ProgramLength = file->CFileSpectrum::SpectrumLength;
		//Length is variables offset for Programs.
		dirEnt->Length = file->CFileSpectrum::SpectrumLength - file->SpectrumVarLength;
		dirEnt->FileExt = 'B';
		break;
	case CFileSpectrum::SPECTRUM_BYTES:
		dirEnt->Length = file->CFileSpectrum::SpectrumLength;
		dirEnt->CodeStart = file->CFileSpectrum::SpectrumStart;
		dirEnt->FileExt = 'C';
		break;
	case CFileSpectrum::SPECTRUM_CHAR_ARRAY:
		dirEnt->Length = file->CFileSpectrum::SpectrumLength;
		dirEnt->FileExt = 'D';
		break;
	case CFileSpectrum::SPECTRUM_NUMBER_ARRAY:
		dirEnt->PrintExtent = file->CFileSpectrum::SpectrumStart;
		dirEnt->Length = file->CFileSpectrum::SpectrumLength;
		dirEnt->FileExt = '#';
		break;
	default:
		dirEnt->Length = file->CFileSpectrum::SpectrumLength;
		dirEnt->CodeStart = file->CFileSpectrum::SpectrumStart;
		break;
	}

	//Set file name.	
	memset(dirEnt->FileName, ' ', sizeof(dirEnt->FileName));
	memcpy(dirEnt->FileName, file->FileName, min(strlen(file->FileName), sizeof(dirEnt->FileName)));
	if (dirEnt->FileExt != 'C' && dirEnt->FileExt != 'B' && dirEnt->FileExt != 'D' && dirEnt->FileExt != '#' &&
		isprint(file->Extension[1]) && isprint(file->Extension[2]))
	{
		dirEnt->CodeStart = ((word)file->Extension[1] << 8) | file->Extension[2];
	}

	//Check if file exists already.
	CFile* existingFile = FindFirst(file->FileName);
	if (existingFile != nullptr)
	{
		LastError = ERR_FILE_EXISTS;
		return false;
	}

	//Write file data.
	byte startTrack = diskDescTRD.NextFreeSectTrack;
	byte startSector = diskDescTRD.NextFreeSect;					
	for (word sectIdx = 0; sectIdx < fileSectorCount && res; sectIdx++)
	{
		word blockIdx = startTrack * Disk->DiskDefinition.SPT + startSector + sectIdx;			
		res = WriteBlock(file->buffer + (sectIdx * Disk->DiskDefinition.SectSize), blockIdx);
	}	

	//Update next free position in disk description.			
	diskDescTRD.NextFreeSectTrack = diskDescTRD.NextFreeSectTrack + (fileSectorCount / Disk->DiskDefinition.SPT) + 
		//When advancing the track, must also consider an aditional track, when filling the previous track.
		(diskDescTRD.NextFreeSect + fileSectorCount % Disk->DiskDefinition.SPT)/Disk->DiskDefinition.SPT;
	diskDescTRD.NextFreeSect = (diskDescTRD.NextFreeSect + fileSectorCount) % Disk->DiskDefinition.SPT;
	diskDescTRD.FreeSectCnt -= fileSectorCount;	
	diskDescTRD.FileCnt++;

	//Fill directory entry for the new file.	
	dirEnt->LenghtSect = fileSectorCount;
	dirEnt->StartSect = startSector;
	dirEnt->StartTrack = startTrack;		
	

	//Update disk description.
	res = Disk->WriteSectors(0, 0, DISK_INFO_SECTOR, 1, (byte*)&diskDescTRD);
	if (!res)
		return res;

	//Update disk catalog.
	res = Disk->WriteSectors(0, 0, 1,
		MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType) / Disk->DiskDefinition.SectSize,
		(byte*)&TRD_Directory);
	if (!res)
		return res;

	//Re-read catalog.
	res = ReadDirectory();
	
	return res;
}

CFile* CFSTRDOS::NewFile(const char* name, long len, byte* data)
{
	CFileTRD* f = new CFileTRD(this);
	f->SetFileName(name);
	f->SetData(data, len);
	return f;
}

CFile* CFSTRDOS::FindFirst(char* pattern)
{	
	dirEntIdx = 0;
	FindPattern = pattern;
	
	return FindNext();
}

CFile* CFSTRDOS::FindNext()
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

dword CFSTRDOS::GetFileSize(CFile* file, bool onDisk)
{
	//return file->GetLength();
	return TRD_Directory[((CFileTRD*)file)->FileDirEntry].LenghtSect * Disk->DiskDefinition.SectSize;
}

void CFSTRDOS::LogicalToPhysicalSector(word logicalSect, byte* track, byte* side, byte* sectorID)
{
	*track = logicalSect / (Disk->DiskDefinition.SPT * Disk->DiskDefinition.SideCnt);
	*side = (logicalSect / Disk->DiskDefinition.SPT) % Disk->DiskDefinition.SideCnt;
	*sectorID = (logicalSect % Disk->DiskDefinition.SPT) + 1;
}

void CFSTRDOS::ReadProgramStartLine(CFileTRD* file)
{
	word line = -1;

	if (file->buffer != nullptr)
	{
		delete[] file->buffer;
		file->buffer = nullptr;
	}

	file->buffer = new byte[FSParams.BlockSize];
	//Read last block from file to get program start line.
	ReadBlock(file->buffer, file->StartTrack * Disk->DiskDefinition.SPT + file->StartSector + file->SectorCnt - 1);
	word lineNo = -1;
	word* lineNoPtr = (word*)&file->buffer[file->SpectrumLength % Disk->DiskDefinition.SectSize];
	if (*lineNoPtr == PROGRAM_LINE_MARKER && (*(lineNoPtr + 1) & 0x8000) == 0)
		lineNo = *(lineNoPtr + 1);
	file->CFileSpectrum::SpectrumStart = lineNo;
	delete[] file->buffer;
	file->buffer = nullptr;	
}

bool CFSTRDOS::OpenFile(CFile* file)
{		
	CFileTRD* f = (CFileTRD*)file;	
	word lineNo = -1;
	word* lineNoPtr = nullptr;
	switch (f->Extension[0])
	{
	case 'B':	
		f->CFileSpectrum::SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;		
		f->CFileSpectrum::SpectrumLength = TRD_Directory[f->FileDirEntry].ProgramLength;					
		//Length is actually variables offset in program block.
		f->CFileSpectrum::SpectrumVarLength = TRD_Directory[f->FileDirEntry].ProgramLength - TRD_Directory[f->FileDirEntry].Length;
		ReadProgramStartLine(f);
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

	return true;
}
