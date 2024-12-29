#include "CFSCobraDEVIL.h"
#include "CFileSystem.h"

#include <vector>
#include <algorithm>


CFSCobraDEVIL::CFSCobraDEVIL(CDiskBase* disk, char* name): CFileSystem(disk, name)
{
	NAME_LENGHT = 10;
	EXT_LENGTH = 0;

	FSParams.BlockSize = 9 * 1024;
	//The max. no. of blocks is limited by disk geometry. Max. block count is 77 = ((80 x 2 - 6) * 2 * 256) / 9K.
	FSParams.BlockCount = ((Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - RESERVED_TRACKS) *
		Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize)/FSParams.BlockSize;
	//A directory entry has one sector and addresses one block. Max. dir. entries is 108 = 6 tracks x 18 SPT. 
	FSParams.DirEntryCount = (RESERVED_TRACKS * disk->DiskDefinition.SPT);
	
	FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_ZXSPECTRUM_FILES | FSFT_CASE_SENSITIVE_FILENAMES | FSFT_FILE_ATTRIBUTES);
}

bool CFSCobraDEVIL::Init()
{
	bool res = true;
	byte* dirBuf = new byte[RESERVED_TRACKS * Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize];
	for (byte trkIdx = 0; trkIdx < RESERVED_TRACKS && res; trkIdx++)
		res = Disk->ReadTrack(dirBuf + (Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize) * trkIdx, 
			trkIdx/2, trkIdx % 2);

	//Initialize dir. entry map.
	for (word dirEntIdx = 0; dirEntIdx < FSParams.DirEntryCount; dirEntIdx++)
		FS_DirEntryMap.push_back(false);

	if (res)
	{		
		DirEntryType* dirEnt;
		word dirIdx = 0; 
		while (dirIdx < FSParams.BlockCount)		
		{
			dirEnt = (DirEntryType*)(dirBuf + (dirIdx * sizeof(DirEntryType)));
			if (dirEnt->Flag != FLAG_DELETE)
			{	
				DEVIL_Dir.push_back(*dirEnt);
				FS_BlockMap.push_back(true);
				FS_DirEntryMap[dirIdx] = true;

				CFileDevil* f = new CFileDevil();
				FileNameType fn{};
				strncpy(fn, dirEnt->Name, sizeof(dirEnt->Name));
				CreateFileName(fn, f);				
				
				if (dirEnt->Flag == FLAG_EXTENT)	
				{	
					delete f;
					f = &DEVIL_FileList[DEVIL_FileList.size() - 1];

					//Check if this directory entry refers to the main block or an headerless block, to report proper length on disk.
					bool isBlockForMainFile = dirEnt->hdrlBlockLen[0] == 0 || 
						//This checks if this extent belongs to the first file: if block length is bigger than the current file size (main file dir entries not yet finished).
						dirEnt->hdrlBlockLen[0] > f->FileBlocks.size() * FSParams.BlockSize;

					if (isBlockForMainFile)
					{
						f->FileDirEntries.push_back(dirIdx);
						f->FileBlocks.push_back(dirIdx);
					}						
					else //map headerless blocks too.
					{							
						dword hdrLessIdx = f->FileDirEntries.size();

						//For each headerless file.
						while (dirIdx < FSParams.BlockCount && dirEnt->Flag == FLAG_EXTENT)
						{
							dword hdrlessLen = dirEnt->hdrlBlockLen[hdrLessIdx];
							dword fileBlockCount = (dword)ceil((float)hdrlessLen / FSParams.BlockSize);
							dword fileBlockIdx = 0;
							
							CFileDevil* fh = new CFileDevil();
							fh->Length = hdrlessLen;
							fh->fs = this;

							FileNameType fn{};
							snprintf(fn, NAME_LENGHT, "%s", dirEnt->Name);
							char blockIdxStr[2];
							itoa(hdrLessIdx, blockIdxStr, 10);
							strcat(fn, blockIdxStr);
							CreateFileName(fn, fh);							
							
							fh->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
							fh->SpectrumLength = (word)hdrlessLen;

							//For each block from the headerless file.
							while (fileBlockIdx < fileBlockCount && dirEnt->Flag == FLAG_EXTENT)
							{								
								fh->FileBlocks.push_back(dirIdx);																
								fh->FileDirEntries.push_back(dirIdx);								

								fileBlockIdx++;

								DEVIL_Dir.push_back(*dirEnt);
								FS_BlockMap.push_back(true);
								FS_DirEntryMap[dirIdx] = true;

								dirIdx++;
								dirEnt = (DirEntryType*)(dirBuf + (dirIdx * sizeof(DirEntryType)));	
							}		

							//Add protection for when the declared lenght is bigger than actual length (incomplete file)
							if (fh->Length > fh->FileBlocks.size() * FSParams.BlockSize)
								fh->Length = fh->FileBlocks.size() * FSParams.BlockSize;

							DEVIL_FileList.push_back(*fh);
							delete fh;
							
							hdrLessIdx++;
						}

						//Go back one dir entry.
						dirIdx--;
					}					
				}
				else
				{
					f->Length = dirEnt->Length;
					f->FileDirEntries.push_back(dirIdx);					
					f->FileBlocks.push_back(dirIdx);
					f->fs = this;

					f->SpectrumType = (dirEnt->Flag < CFileSpectrum::SPECTRUM_UNTYPED ? (CFileSpectrum::SpectrumFileType)dirEnt->Flag : CFileSpectrum::SPECTRUM_UNTYPED);
					f->SpectrumLength = dirEnt->Length;					
					f->SpectrumStart = dirEnt->Param1;
					if (f->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
						f->SpectrumVarLength = dirEnt->Length - dirEnt->Param2;
					else
						f->SpectrumVarLength = 0;					

					DEVIL_FileList.push_back(*f);

					delete f;					
				}									
			}
			else
			{
				FS_BlockMap.push_back(false);
				FS_DirEntryMap[dirIdx] = false;
			}

			dirIdx++;
		}

		
	}

	delete[] dirBuf;
	return res;
}

CFile* CFSCobraDEVIL::NewFile(const char* name, long len, byte* data)
{
	CFileDevil* f = new CFileDevil(this);	
	f->SetFileName(name);
	f->SetData(data, len);
	return f;
}

CFile* CFSCobraDEVIL::FindFirst(char* pattern)
{
	findIdx = 0;
	strcpy(DEVIL_FindPattern, pattern);

	return FindNext();
}

CFile* CFSCobraDEVIL::FindNext()
{	
	CFileDevil* f = NULL;
	bool bFound = false;		

	while (findIdx < DEVIL_FileList.size() && !bFound)
	{					
		bFound = WildCmp(DEVIL_FindPattern, DEVIL_FileList[findIdx].Name);

		if (bFound)		
			f = &DEVIL_FileList[findIdx];		

		findIdx++;
	}

	if (f == NULL)
		LastError = CFileArchive::ERR_FILE_NOT_FOUND;

	return f;
}

dword CFSCobraDEVIL::GetFileSize(CFile* file, bool onDisk)
{
	CFileDevil* fDevil = (CFileDevil*)file;
	dword fsizeOnDisk = fDevil->FileDirEntries.size() * FSParams.BlockSize;
	dword fsizeLogical = file->GetLength();

	if (onDisk)
		return fsizeOnDisk;
	else if (fsizeLogical <= fsizeOnDisk)  //Some files may report bigger size than what is actually allocated on disk.
		return fsizeLogical;
	else
		return fsizeOnDisk;
}

bool CFSCobraDEVIL::ReadBlock(byte* buf, word blkIdx, byte sectCnt)
{
	byte tracksPerBloc = FSParams.BlockSize/(Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize);
	
	for (byte trackIdx = 0; trackIdx < tracksPerBloc; trackIdx++)
	{
		byte absTrack = RESERVED_TRACKS + (blkIdx * tracksPerBloc) + trackIdx;
		byte track = absTrack / Disk->DiskDefinition.SideCnt;
		byte side = absTrack % Disk->DiskDefinition.SideCnt;

		Disk->ReadTrack(buf + (FSParams.BlockSize/tracksPerBloc) * trackIdx, track, side);
	}		
	
	return true;
}


bool CFSCobraDEVIL::ReadFile(CFile* file)
{
	CFileDevil* f = (CFileDevil*)file;
	bool res = true;	

	if (f->buffer == NULL)
		f->buffer = new byte[GetFileSize(f, true)];

	for (byte blkIdx = 0; blkIdx < f->FileBlocks.size() && res; blkIdx++)
	{		
		res = ReadBlock(f->buffer + (blkIdx * FSParams.BlockSize), f->FileBlocks[blkIdx]);
	}

	return res;
}


dword CFSCobraDEVIL::GetDiskMaxSpace() 
{ 
	return (Disk->DiskDefinition.TrackCnt * Disk->DiskDefinition.SideCnt - RESERVED_TRACKS) *
		Disk->DiskDefinition.SPT * Disk->DiskDefinition.SectSize; 
};

dword CFSCobraDEVIL::GetDiskLeftSpace() 
{ 
	return GetDiskMaxSpace() - DEVIL_Dir.size() * FSParams.BlockSize; 
};	

CFileSystem::FileAttributes CFSCobraDEVIL::GetAttributes(CFile* file)
{
	auto f = (CFileDevil*)file;
	byte res = (byte)FileAttributes::ATTR_NONE;
	if (f->FileDirEntries.size() > 0)
	{
		word dirEnt = f->FileDirEntries[0];
		if (dirEnt < DEVIL_Dir.size())
		{
			byte attrByte = this->DEVIL_Dir[f->FileDirEntries[0]].Attributes;

			if ((attrByte & 1) == 1)
				res |= (int)FileAttributes::ATTR_SYSTEM;
			if ((attrByte & 2) == 2)
				res |= (int)FileAttributes::ATTR_READ_ONLY;
		}		
	}	

	return (FileAttributes)res;
}

//////////////////////////////////////////////////////////////////////////

CFileDevil::CFileDevil()
{

}

CFileDevil::CFileDevil(CFSCobraDEVIL* fs)
{ 
	this->fs = dynamic_cast<CFSCobraDEVIL*>(fs); 
}

CFileDevil::~CFileDevil()
{	
}

CFileDevil::CFileDevil(const CFileDevil& src): CFile(src)
{	
	fs = src.fs;
	FileDirEntries = src.FileDirEntries;
	FileBlocks = src.FileBlocks;

	SpectrumType = src.SpectrumType;
	SpectrumLength = src.SpectrumLength;
	SpectrumStart = src.SpectrumStart;
	SpectrumVarLength = src.SpectrumVarLength;
}

CFileDevil& CFileDevil::operator= (const CFileDevil& src)
{
	CFile::operator =(src);

	fs = src.fs;
	FileDirEntries = src.FileDirEntries;
	FileBlocks = src.FileBlocks;
	SpectrumType = src.SpectrumType;
	SpectrumLength = src.SpectrumLength;	
	SpectrumStart = src.SpectrumStart;
	SpectrumVarLength = src.SpectrumVarLength;

	return *this;
}
