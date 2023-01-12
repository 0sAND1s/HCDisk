#include "CFSOpus.h"
#include "CFileSystem.h"
#include "DiskBase.h"


CFSOpus::CFSOpus(CDiskBase* disk, char* name /* = NULL */): CFileSystem(disk, name)
{
	NAME_LENGHT = 10;
	EXT_LENGTH = 0;
	FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_DISK | FSFT_ZXSPECTRUM_FILES | FSFT_LABEL | FSFT_CASE_SENSITIVE_FILENAMES);	
}

bool CFSOpus::Init()
{
	bool res = true;	

	//Read disk configuration from boot block.	
	byte* bufBoot = new byte[1024];
	res = Disk->ReadSectors(bufBoot, 0, 0, 1, 1);
	OpusBootSector* bootRec = (OpusBootSector*)bufBoot;		

	if (res)
		res = bootRec->jmp == 0x0518; //jump code, use it as signature.

	if (res)
	{		
		res = Disk->DiskDefinition.TrackCnt == bootRec->trackCnt &&
			Disk->DiskDefinition.SPT == bootRec->sectCnt &&
			Disk->DiskDefinition.SideCnt == bootRec->dblSide+1 &&
			Disk->SectSize2SectCode(Disk->DiskDefinition.SectSize) == bootRec->sectSizeCode;		
	}	
	delete bufBoot;	
	
	byte* buf = NULL;
	if (res)
	{
		FSParams.BlockSize = Disk->DiskDefinition.SectSize;
		FSParams.BlockCount = Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SPT * Disk->DiskDefinition.SideCnt;
		FSParams.BootTrackCount = 0;
		FSParams.DirEntryCount = (Disk->DiskDefinition.SectSize * CATALOG_BLOCKS)/sizeof(DirEntryOpus);

		//Initialize dir. entry map.	
		FS_DirEntryMap = vector<bool>(FSParams.DirEntryCount, false);
		FS_BlockMap = vector<bool>(FSParams.BlockCount, false);

		//First block is reserved for disk description.		
		buf = new byte[sizeof(DirEntryOpus) * FSParams.DirEntryCount];
		res = Disk->ReadSectors(buf, 0, 0, 2, CATALOG_BLOCKS);
	}
	
	if (res)
	{
		OpusDir.clear();
		OpusFiles.clear();

		for (word fIdx = 0; fIdx < FSParams.DirEntryCount; fIdx++)
		{			
			DirEntryOpus* dirEnt = (DirEntryOpus*)(buf + fIdx * sizeof(DirEntryOpus));
			if (dirEnt->name[0] != 0xE5 && dirEnt->name[0] != 0x00 && dirEnt->first_block <= dirEnt->last_block && dirEnt->last_block < FSParams.BlockCount)
			{
				OpusDir.push_back(*dirEnt);
				FS_DirEntryMap[fIdx] = true;				

				for (word blk = dirEnt->first_block; blk <= dirEnt->last_block; blk++)
					FS_BlockMap[blk] = true;

				CFileOpus* f = new CFileOpus();								
				FileNameType fn{};
				strncpy(fn, dirEnt->name, sizeof(dirEnt->name));
				CreateFileName(fn, f);	

				f->Length = (dirEnt->last_block - dirEnt->first_block + 1) * FSParams.BlockSize + dirEnt->bytes_in_last_block;			

				for (word blk = dirEnt->first_block; blk <= dirEnt->last_block; blk++)
					f->FileBlocks.push_back(blk);			

				f->FileDirEntries.push_back(findIdx);

				OpusFiles.push_back(*f);

				delete f;
			}
		}		
	}

	delete buf;

	return res;
}

CFile* CFSOpus::FindFirst(char* pattern)
{
	//First dir. entry is for label.
	findIdx = 1;
	findPattern = pattern;
	LastError = CFileArchive::ERR_NONE;

	return FindNext();
}

CFile* CFSOpus::FindNext()
{
	bool found = false;	
	CFileOpus* f = NULL;
	vector<CFileOpus>::iterator i = OpusFiles.begin() + findIdx;
	while (i != OpusFiles.end() && !found)
	{
		FileNameType fn;
		i->GetFileName(fn);
		found = WildCmp(findPattern, fn);		

		if (found)
			f = &*i;
		
		i++;
		findIdx++;
	}	

	if (f == NULL)
		LastError = CFileArchive::ERR_FILE_NOT_FOUND;

	return f;
}

bool CFSOpus::ReadBlock(byte* buf, word blkIdx, byte sectCnt)
{
	//Blocks start at 1, not 0.
	blkIdx++;
	
	if (sectCnt == 0)
		sectCnt = FSParams.BlockSize/Disk->DiskDefinition.SectSize;

	return Disk->ReadSectors(buf, 
		(blkIdx / Disk->DiskDefinition.SPT) % Disk->DiskDefinition.TrackCnt, 
		blkIdx / (Disk->DiskDefinition.SPT * Disk->DiskDefinition.TrackCnt), 
		(blkIdx % Disk->DiskDefinition.SPT) + 1, sectCnt);
}

bool CFSOpus::OpenFile(CFile* file)
{
	CFileOpus* f = (CFileOpus*)file;

	byte* buf = new byte[FSParams.BlockSize];
	bool res = f->FileBlocks.size() > 0;

	if (res)
		res = ReadBlock(buf, f->FileBlocks[0], 1);			

	if (res)
	{
		OpusHeader* h = (OpusHeader*)buf;
		f->SpectrumType = (CFileSpectrum::SpectrumFileType)h->Type;
		f->SpectrumType = f->GetType();
		f->SpectrumLength = h->Length;	
		f->SpectrumStart = h->StartAddr;
		f->SpectrumVarLength = 0;
		f->SpectrumArrayVarName = 0;
	}	

	delete buf;

	f->isOpened = res;
	return res;
}

bool CFSOpus::CloseFile(CFile* file)
{
	CFileOpus* f = (CFileOpus*)file;
	if (f->isOpened)
	{
		f->isOpened = false;
		if (f->buffer != NULL)
		{
			delete f->buffer;
			f->buffer = NULL;
		}
		return true;
	}
	else
		return false;
}

bool CFSOpus::ReadFile(CFile* file)
{
	CFileOpus* f = (CFileOpus*)file;
	bool res = true;
	if (f->isOpened)
	{
		res = CFileSystem::ReadFile(file);
		if (res)
		{
			if (GetFileSize(f) > 0)
			{
				if (f->GetType() != CFileSpectrum::SPECTRUM_UNTYPED)
				{
					memcpy(f->buffer, f->buffer + sizeof(OpusHeader), f->SpectrumLength);
					f->Length = f->SpectrumLength;
				}
			}
		}
	}
	else
		res = false;

	return res;
}

bool CFSOpus::GetLabel(char* dest)
{
	if (OpusDir.size() > 0 && OpusDir[0].first_block == 0)
	{
		memcpy(dest, OpusDir[0].name, sizeof(OpusDir[0].name));
		dest[sizeof(OpusDir[0].name)] = '\0';
		return true;
	}
	else
		return false;
}