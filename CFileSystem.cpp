#include "CFileSystem.h"
#include <stdio.h>
#include <algorithm>


CFileSystem::CFileSystem(CDiskBase* disk, char* name): CFileArchive(name)
{	
	LastError = ERR_NONE;
	FSFeatures = FSFT_DISK;

	this->Disk = disk;

	if (name != NULL)
		strcpy(this->Name, name);
	
	NAME_LENGHT = 8;
	EXT_LENGTH = 3;

	memset(&FSParams, 0, sizeof(FSParams));
}

CFileSystem::~CFileSystem()
{
	/*
	if (Disk != NULL)
	{
		delete Disk;
		Disk = NULL;
	}
	*/
}

bool CFileSystem::ReadFile(CFile* f)
{
	bool res = (f != NULL);

	if (res)
		f->buffer = new byte[GetFileSize(f, true)];

	for (byte blkIdx = 0; blkIdx < f->FileBlocks.size() && res; blkIdx++)
		res = ReadBlock(f->buffer + blkIdx * this->FSParams.BlockSize, f->FileBlocks[blkIdx]);

	return res;
}

word CFileSystem::GetFreeBlockCount()
{
	return count(FS_BlockMap.begin(), FS_BlockMap.end(), false);
}

word CFileSystem::GetFreeDirEntriesCount()
{
	return count(FS_DirEntryMap.begin(), FS_DirEntryMap.end(), false);
}

dword CFileSystem::GetDiskLeftSpace()
{	
	return FSParams.BlockSize * GetFreeBlockCount();
}

dword CFileSystem::GetDiskMaxSpace()
{	
	return FSParams.BlockCount * FSParams.BlockSize;
}

dword CFileSystem::GetFileSize(CFile* file, bool onDisk)
{
	if (onDisk)
		return file->FileBlocks.size() * FSParams.BlockSize;
	else
		return file->GetLength();
}