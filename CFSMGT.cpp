//////////////////////////////////////////////////////////////////////////
//MGT file system parser. George Chirtoaca 2014.
//Specifications used: http://web.archive.org/web/20080514125600/http://www.ramsoft.bbk.org/tech/mgt_tech.txt .
//////////////////////////////////////////////////////////////////////////

#include "CFSMGT.h"


bool CFSMGT::Init()
{
	word dirSize = DIR_ENTRY_COUNT * sizeof(MGTDirEntry);
	byte* dirBuf = new byte[dirSize];
	memset(dirBuf, 0, dirSize);	
	word trkSize = Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize;
	byte dirTracks = dirSize / trkSize;	
	bool res = true;

	FSParams.BlockSize = BLOCK_SIZE;
	FSParams.BlockCount = ((Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - dirTracks) * 
		Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize )/BLOCK_SIZE;
	FSParams.BootTrackCount = 0;
	FSParams.DirEntryCount = DIR_ENTRY_COUNT;

	FS_DirEntryMap = vector<bool>(FSParams.DirEntryCount, false);
	FS_BlockMap = vector<bool>(FSParams.BlockCount, false);	

	for (byte trk = 0; trk < dirTracks && res; trk++)
		res = Disk->ReadTrack(dirBuf + trk * trkSize, trk, 0);
	
	for (byte dirEntIdx = 0; dirEntIdx < DIR_ENTRY_COUNT; dirEntIdx++)
	{
		MGTDirEntry* dirEnt = (MGTDirEntry*)(dirBuf + dirEntIdx * sizeof(MGTDirEntry));				
		if (dirEnt->TypeMGT > MGT_FT_ERASED)
		{
			CFileMGT* f = CreateFileMGT(*dirEnt);
			if (f != NULL)
			{				
				f->FileDirEntries.push_back(dirEntIdx);
				word sectCnt = dirEnt->SectorCntLow + 0x100 * dirEnt->SectorCntHi;
				word sectIdx = 0;
				byte trackStart = (dirEnt->StartTrack - dirTracks) % 80;
				byte sideStart = ((trackStart & 0x80) > 0 ? 1 : 0);				
				word blockIdx = trackStart * Disk->DiskDefinition.SPT + sideStart * Disk->DiskDefinition.TrackCnt + 
					dirEnt->StartSector - 1;

				//While block map is not finished, and the file sector count is not reached...
				while (blockIdx < sizeof(dirEnt->AllocMap) * 8 && sectIdx < sectCnt)
				{
					//If the sector is marked as used for this file, assign it to the file.
					if (dirEnt->AllocMap[blockIdx/8] & (1 << (blockIdx % 8)))
					{
						f->FileBlocks.push_back(blockIdx);
						FS_BlockMap[blockIdx] = true;	
						sectIdx++;
					}

					blockIdx++;
				}					
				
				MGTFiles.push_back(*f);

				delete f;
			}		

			FS_DirEntryMap[dirEntIdx] = true;
		}		
	}
	
	delete dirBuf;
	return res;
}

CFileMGT* CFSMGT::CreateFileMGT(MGTDirEntry dir)
{	
	CFileMGT* f = new CFileMGT();
	f->SetFileName(dir.Name);
	f->Length = dir.FileHdrSpectrum.Length;		

	if (dir.TypeMGT - 1 < CFileSpectrum::SPECTRUM_UNTYPED)
	{
		f->SpectrumType = (CFileSpectrum::SpectrumFileType)(dir.FileHdrSpectrum.TypeSpectrum);
		f->SpectrumType = f->GetType();
		f->SpectrumVarLength = 0;

		if (f->SpectrumType != CFileSpectrum::SPECTRUM_UNTYPED)
		{
			f->SpectrumLength = dir.FileHdrSpectrum.Length;
			if (f->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)				
			{
				f->SpectrumStart = dir.FileHdrSpectrum.StartRun;
				f->SpectrumVarLength = f->SpectrumLength - dir.FileHdrSpectrum.ProgLen;
			}
			else if (f->SpectrumType == CFileSpectrum::SPECTRUM_CHAR_ARRAY || 
				f->SpectrumType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
				f->SpectrumArrayVarName = dir.FileHdrSpectrum.ArrVarName;
			else
				f->SpectrumStart = dir.FileHdrSpectrum.StartAddr;
		}		
	}		
	else
	{
		f->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;

		if (dir.TypeMGT == MGT_FT_SCR)
		{
			f->SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
			f->SpectrumLength = 6912;
			f->SpectrumStart = 16384;
		}
		else if (dir.TypeMGT == MGT_FT_OPEN)
		{
			f->Length = (dir.FileHdrOpen.BlockCount64K - 1) * 64 * 1024 + dir.FileHdrOpen.LenLastBlock;
		}
		else if (dir.TypeMGT == MGT_FT_EXE || dir.TypeMGT == MGT_FT_CREATE)
		{
			f->SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
			f->SpectrumLength = dir.FileHdrSpectrum.Length;
			f->SpectrumStart = dir.FileHdrSpectrum.StartAddr;
		}
		else
		{
			f->SpectrumLength = f->SpectrumStart = 0;
			f->Length = (dir.SectorCntLow + 0x100 * dir.SectorCntHi) * BLOCK_SIZE;
		}
	}

	return f;
}

CFile* CFSMGT::FindFirst(char* pattern)
{
	FindIdx = 0;
	FindPattern = pattern;
	return FindNext();
}

CFile* CFSMGT::FindNext()
{
	CFileMGT* f = NULL;	
	bool found = false;

	while (FindIdx < MGTFiles.size() && !found)
	{
		f = &MGTFiles[FindIdx];
		FileNameType fn;
		f->GetFileName(fn);
		found = WildCmp(FindPattern, fn);
		FindIdx++;
	}

	if (found)			
		return f;
	else
	{
		LastError = ERR_FILE_NOT_FOUND;
		return NULL;
	}
}

bool CFSMGT::ReadBlock(byte* buf, word blkIdx, byte sectCnt)
{	
	byte side = blkIdx / (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SPT);
	byte track = (DIR_TRACKS + blkIdx / Disk->DiskDefinition.SPT) % Disk->DiskDefinition.TrackCnt;	
	byte sector = blkIdx % Disk->DiskDefinition.SPT + 1;
	return Disk->ReadSectors(buf, track, side, sector, sectCnt == 0 ? 1 : sectCnt);
}

bool CFSMGT::OpenFile(CFile* file)
{
	CFileMGT* f = (CFileMGT*)file;
	byte* buf = new byte[FSParams.BlockSize];
	bool res = f->FileBlocks.size() > 0;

	if (res)
		res = ReadBlock(buf, f->FileBlocks[0]);

	//Check if the header from the directory is the same as the header from file.
	if (res && f->SpectrumType != CFileSpectrum::SPECTRUM_UNTYPED)
		res = ((MGTFileHdrSpectrum*)buf)->TypeSpectrum == f->SpectrumType;

	delete buf;

	return res;
}

bool CFSMGT::CloseFile(CFile* file)
{
	CFileMGT* f = (CFileMGT*)file;
	if (f->buffer != NULL)
	{

		delete f->buffer;
		f->buffer = NULL;
		return NULL;
	}
	else
		return false;
}

bool CFSMGT::ReadFile(CFile* file)
{
	CFileMGT* f = (CFileMGT*)file;
	bool res = true;
	
	f->buffer = new byte[GetFileSize(f, true)];

	for (byte blkIdx = 0; blkIdx < f->FileBlocks.size() && res; blkIdx++)
		res = ReadBlock(f->buffer + blkIdx * (this->FSParams.BlockSize - 2), f->FileBlocks[blkIdx]);

	//Skip the file header.
	if (f->SpectrumType != CFileSpectrum::SPECTRUM_UNTYPED)
		memmove(f->buffer, f->buffer + sizeof(MGTFileHdrSpectrum), f->GetLength());

	return res;
}