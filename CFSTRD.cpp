#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include "CFSTRD.h"
#include "CFileTRD.h"

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
	FSFeatures = (FileSystemFeature)(FSFT_DISK | FSFT_ZXSPECTRUM_FILES | FSFT_LABEL);	

	FSParams.DirEntryCount = MAX_DIR_ENTRIES;
	FSParams.BlockSize = Disk->DiskDefinition.SectSize;
	FSParams.BlockCount = (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - 1) * Disk->DiskDefinition.SPT;
}

CFSTRDOS::~CFSTRDOS()
{	
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

	Disk->ReadSectors((byte*)&TRD_Directory, 0, 0, 0, 
		MAX_DIR_ENTRIES * (byte)sizeof(DirEntryType)/Disk->DiskDefinition.SectSize);	

	for (word dirIdx = 0; dirIdx < MAX_DIR_ENTRIES; dirIdx++)
		FS_DirEntryMap.push_back(false);

	for (word auIdx = 0; auIdx < FSParams.BlockCount; auIdx++)
		FS_BlockMap.push_back(false);

	for (dirEntIdx = 0; dirEntIdx < MAX_DIR_ENTRIES && !bDirFinished; dirEntIdx++)		
	{
		dirEnt = *(DirEntryType*)&TRD_Directory[dirEntIdx];		 
		bDirFinished = dirEnt.FileName[0] == 0;		
	
		if (!bDirFinished && dirEnt.FileName[0] != DEL_FLAG)		
		{
			file.FileDirEntry = dirEntIdx;					
			file.Length = dirEnt.Length;
			file.StartSector = dirEnt.StartSect;
			file.StartTrack = dirEnt.StartTrack;
			file.SectorCnt = dirEnt.LenghtSect;	
			CreateFileName(dirEnt.FileName, &file);			
			
			FS_DirEntryMap[dirEntIdx] = true;
			for (word secIdx = 0; secIdx <= dirEnt.LenghtSect; secIdx++)
			{
				word logicalStartSect = file.StartTrack * Disk->DiskDefinition.SPT + file.StartSector;
				FS_BlockMap[logicalStartSect + secIdx] = true;
			}
			
			if (file.Extension[0] != 'P' && file.Extension[0] != 'B' && file.Extension[0] != 'D' &&
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
	LastError = ERR_NOT_SUPPORTED;
	return false;//TBD
}

bool CFSTRDOS::Rename(CFile* file, char* newName)
{
	LastError = ERR_NOT_SUPPORTED;
	return false;//TBD
}

bool CFSTRDOS::ReadBlock(byte* destBuf, word sectIdx, byte sectCnt)
{	
	byte trk, side, sector;
	
	LogicalToPhysicalSector(sectIdx, &trk, &side, &sector);	
	return Disk->ReadSectors(destBuf, trk, side, sector, 1);
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
	LastError = ERR_NOT_SUPPORTED;
	return false;
}

CFile* CFSTRDOS::NewFile(char* name, long len, byte* data)
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

bool CFSTRDOS::OpenFile(CFile* file)
{		
	CFileTRD* f = (CFileTRD*)file;
	f->buffer = new byte[FSParams.BlockSize];
	ReadBlock(f->buffer, f->StartSector);
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
