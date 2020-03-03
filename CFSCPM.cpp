#include <memory.h>
#include <math.h>
#include <string.h>
#include <string>
#include "CFSCPM.h"
#include "CFileSystem.h"

#include <bitset>
#include <vector>
#include <algorithm>

using namespace std;

CFSCPM::CFSCPM(CDiskBase * disk, FSParamsType fsParams, CPMParams cpmParams, char* name) : CFileSystem(disk, name)
{
	NAME_LENGHT = 8;
	EXT_LENGTH = 3;
	FSFeatures = (FileSystemFeature)(FSFeatures | FSFT_FILE_ATTRIBUTES | FSFT_FOLDERS); //Simulate user region with folders.

	Disk = disk;		
	FSDesc = cpmParams;	
	FSParams = fsParams;

	//Get the number of alloc units used by the directory.	
	CPM_ReservedAllocUnits = FSParams.DirEntryCount*32/FSParams.BlockSize;			
	
	CPM_AUInExt = CPM_EXT_ALLOC*FSDesc.ExtentsInDirEntry/FSParams.BlockSize;	

	//Build the interleave table based on interleave factor.		
	byte SectOfs = 0;
	byte rotationIdx = 0;
	for (byte sectIdx = 0; sectIdx < Disk->DiskDefinition.SPT; sectIdx++)
	{									
		CPM_InterleaveTbl[sectIdx] = 1 + SectOfs;
		SectOfs += (FSDesc.Interleave + 1);
		if (SectOfs > Disk->DiskDefinition.SPT - 1)
		{
			rotationIdx = (rotationIdx + FSDesc.Interleave) % (FSDesc.Interleave + 1);
			SectOfs = rotationIdx;
		}
	}

	memset(&DirEntries, 0, sizeof(DirEntries));
}

CFSCPM::~CFSCPM()
{		
	for (CPMFileListType::iterator it = CPM_FileList.begin(); it != CPM_FileList.end(); it++)
		CloseFile((CFileCPM*)&*it);

	FS_BlockMap.clear();
	FS_DirEntryMap.clear();
	
	if (Disk != NULL)
	{
		delete Disk;
		Disk = NULL;
	}	
}


bool CFSCPM::LinearSect2HTS(word sectCnt, byte & head, byte & track, byte & sect)
{
	//side changes once for every full track read
	head = (sectCnt/Disk->DiskDefinition.SPT) % Disk->DiskDefinition.SideCnt;

	//track advances once for every full track read on both faces
	track = sectCnt/(Disk->DiskDefinition.SPT * Disk->DiskDefinition.SideCnt);

	//sector in track is a reminder,+1 cos it starts counting at 1
	sect = (sectCnt % Disk->DiskDefinition.SPT);

	return true;
}

bool CFSCPM::ReadBlock(byte* buf, word auIdx, byte maxSecCnt)
{
	bool bReadOK = true;
	word off = 0;
	byte SPAL = FSParams.BlockSize/Disk->DiskDefinition.SectSize;		
	byte Head, Track, Sect;

	//The allocation units can fill only part of a track.
	word TotalSectNo = (FSParams.BootTrackCount * Disk->DiskDefinition.SPT) + 		
		auIdx*SPAL;	

	if (maxSecCnt > 0 && maxSecCnt <= SPAL)
		SPAL = maxSecCnt;

	for (byte s = 0; s < SPAL && bReadOK; s++)
	{
		//Must read sectors in order of software interleave.
		LinearSect2HTS(TotalSectNo++, Head, Track, Sect);
		bReadOK = Disk->ReadSectors(buf + off, 
			Track, Head, 			 
			CPM_InterleaveTbl[Sect], 
			1);

		off += Disk->DiskDefinition.SectSize;
	}	

	return bReadOK;
}


bool CFSCPM::WriteBlock(byte* buf, word auIdx, byte maxSecCnt)
{
	bool bWriteOK = true;
	word off = 0;
	byte SPAL = FSParams.BlockSize/Disk->DiskDefinition.SectSize;		
	byte Head, Track, Sect;

	//The allocation units can fill only part of a track.
	word TotalSectNo = (FSParams.BootTrackCount * Disk->DiskDefinition.SPT) + 		
		auIdx*SPAL;	

	if (maxSecCnt > 0 && maxSecCnt <= SPAL)
		SPAL = maxSecCnt;

	for (byte s = 0; s < SPAL && bWriteOK; s++)
	{
		//Must write sectors in order of software interleave.
		LinearSect2HTS(TotalSectNo++, Head, Track, Sect);
		bWriteOK = Disk->WriteSectors(Track, Head, 			 
			CPM_InterleaveTbl[Sect], 1, buf + off);

		off += Disk->DiskDefinition.SectSize;
	}	

	return bWriteOK;
}

bool CFSCPM::WriteDirectory()
{
	bool bWriteOK = true;
	for (word auIdx = 0; auIdx < CPM_ReservedAllocUnits && bWriteOK; auIdx++)	
		bWriteOK = WriteBlock(((byte*)&DirEntries) + FSParams.BlockSize*auIdx, auIdx);			

	return bWriteOK;
}


bool CFSCPM::ReadDirectory()
{			
	bool bReadOK = true;
	word Idx = 0;
	memset(&DirEntries, Disk->DiskDefinition.Filler, sizeof(DirEntries));	

	for (word auIdx = 0; auIdx < CPM_ReservedAllocUnits && bReadOK; auIdx++)	
		bReadOK = ReadBlock(((byte*)&DirEntries) + FSParams.BlockSize*auIdx, auIdx);			
	
	if (bReadOK)
	{
		//Init alloc map with reserved AUs.
		FS_BlockMap = vector<bool>(FSParams.BlockCount);		
		for (byte idx = 0; idx < CPM_ReservedAllocUnits; idx++)
			FS_BlockMap[idx] = true;

		FS_DirEntryMap = vector<bool>(FSParams.DirEntryCount);		
		CPM_FileList = CPMFileListType();		
		for (word entIdx = 0; entIdx < FSParams.DirEntryCount; entIdx++)
		{
			if (DirEntries[entIdx].UsrCode != Disk->DiskDefinition.Filler)		
			{								
				CFileCPM f = CFileCPM(this);				
				f.User = DirEntries[entIdx].UsrCode;
				CreateFileName(DirEntries[entIdx].FileName, &f);				
				
				CPMFileListType::iterator fit = find(CPM_FileList.begin(), CPM_FileList.end(), f);
				if (fit != CPM_FileList.end())
				{
					f = *fit;
				}
				else //New file.
				{
					f.FileBlocks = vector<word>();
					f.User = DirEntries[entIdx].UsrCode;
					f.Idx = Idx++;	
					f.fs = this;
				}

				//File directory entries.
				//Must sort dir entries by index, because they may lay on disk in random order.
				//Rares Atodiresei says BDOS re-scans the dir from the beginning for each FindNext().				
				vector<word>::iterator it = f.FileDirEntries.begin();
				bool mustInsert = false;
				while (it != f.FileDirEntries.end() && !mustInsert)
				{					
					if (DirEntries[*it].ExtIdx > DirEntries[entIdx].ExtIdx)													
						mustInsert = true;						
					else
						it++;									
				}
				
				if (!mustInsert)
					f.FileDirEntries.push_back(entIdx);
				else
					f.FileDirEntries.insert(it, entIdx);								
				
				if (fit != CPM_FileList.end())
				{
					//Also move file in the file list where the file dir entry with index 0 is found.
					if (!mustInsert)
						*fit = f;
					else
					{
						CPM_FileList.erase(fit);
						CPM_FileList.push_back(f);	
					}
				}
				else				
					CPM_FileList.push_back(f);									

				FS_DirEntryMap[entIdx] = true;
			}
			else
			{
				FS_DirEntryMap[entIdx] = false;
			}
		}

		//File blocks.
		CPMFileListType::iterator fIt = CPM_FileList.begin();
		while (fIt != CPM_FileList.end())
		{						
			vector<word>::iterator fDirEntIdx = fIt->FileDirEntries.begin();
			while (fDirEntIdx != fIt->FileDirEntries.end())
			{
				byte auIdx = 0;
				word auNo = GetAUNoFromExt(*fDirEntIdx, auIdx);
				while (auNo > 0 && auNo < (word)FS_BlockMap.size() && auIdx < CPM_AUInExt)
				{	
					fIt->FileBlocks.push_back(auNo);
					FS_BlockMap[auNo] = true;
					auIdx++;
					auNo = GetAUNoFromExt(*fDirEntIdx, auIdx);
				}
				fDirEntIdx++;
			}			
			fIt++;
		}		
	}		
	
	return bReadOK;
}


CFile* CFSCPM::FindFirst(char* pattern)
{
	//ReadDirectory();	
	CPM_FindIdx = 0;
	
	//If we don't have wild cards, must convert the name.
	/*
	if (strstr(pattern, "*") == 0 && strstr(pattern, "?") == 0)
	{
		FileNameType fName = "";
		CreateFileName(pattern, fName, true);		
	}
	*/
	
	CWD = "";
	char* fld = strrchr(pattern, '\\');
	if (fld != NULL)
	{
		CWD = string(pattern).substr(0, fld - pattern);
		strcpy(CPM_FindPattern, string(pattern).substr(fld - pattern + 1).c_str());
	}
	else
		strcpy(CPM_FindPattern, pattern);	
		
	return FindNext();
}


CFile* CFSCPM::FindNext()
{	
	bool bNameMatch = false, bDirMatch = false;		
	CFileCPM* file = NULL;
	
	for (word fnIdx = CPM_FindIdx; fnIdx < CPM_FileList.size() && !(bNameMatch && bDirMatch); fnIdx++)
	{		
		bNameMatch = WildCmp(CPM_FindPattern, CPM_FileList[fnIdx].FileName);		
		if (CWD != "")
		{
			string dir;
			dir += CPM_FileList[fnIdx].User + '0';
			bDirMatch = (CWD == dir);
		}
		else
			bDirMatch = true;

		if (bNameMatch && bDirMatch)
			file = &CPM_FileList[fnIdx];
		CPM_FindIdx++;	
	}
	
	if (file == NULL)
		LastError = CFileSystem::ERR_FILE_NOT_FOUND;
	return file;
}

dword CFSCPM::GetFileSize(CFileCPM* file, bool onDisk)
{
	dword fs = 0;

	if (onDisk)
		fs = file->FileBlocks.size() * FSParams.BlockSize;
	else
		fs = ((file->FileDirEntries.size() - 1) * CPM_EXT_ALLOC * this->FSDesc.ExtentsInDirEntry) + 
			(GetLastExtRecCnt(file) * CPM_REC_SIZE);

	return fs;
}

bool CFSCPM::WriteFile(CFileCPM* file)
{
	LastError = ERR_NONE;	

	if (FindFirst(file->FileName) != NULL)
	{
		LastError = ERR_FILE_EXISTS;
	}
	else
	{
		word reqAUCnt = (word)ceil((float)file->Length/FSParams.BlockSize);
		word freeAUCnt = GetFreeBlockCount();
		word reqDirEnt = (word)ceil((float)reqAUCnt/CPM_AUInExt);		

		if (reqAUCnt > freeAUCnt)
			LastError = ERR_NO_SPACE_LEFT;
		else if (GetFreeDirEntriesCount() < reqDirEnt)
			LastError = ERR_NO_CATALOG_LEFT;
		else
		{
			//Write file blocks.		
			vector<word> fileAUs;
			word auIdx = 0, auNo = GetNextFreeAllocUnit();							
			
			//Set EOF for the remaining file slack.
			byte* diskBuf = new byte[reqAUCnt * FSParams.BlockSize];
			memcpy(diskBuf, file->buffer, file->Length);

			if (file->Length % FSParams.BlockSize > 0)
			{
				dword bufOff = (reqAUCnt - 1) * FSParams.BlockSize + file->Length % FSParams.BlockSize;
				word eofLen = FSParams.BlockSize - file->Length % FSParams.BlockSize - 1;
				memset(diskBuf + bufOff, EOF_MARKER, eofLen);
			}

			bool writeOK = true;
			while (auIdx < reqAUCnt && auNo > 0 && writeOK)
			{				
				writeOK = WriteBlock(diskBuf + (FSParams.BlockSize*auIdx), auNo);
				fileAUs.push_back(auNo);
				FS_BlockMap[auNo] = true;
				auNo = GetNextFreeAllocUnit();
				auIdx++;
			}

			delete diskBuf;

			//Write file catalog entries
			if (writeOK && fileAUs.size() > 0)
			{
				LastError = CFileSystem::ERR_NONE;

				for (byte dirEntIdx = 0; dirEntIdx < reqDirEnt && LastError == ERR_NONE; dirEntIdx++)
				{
					word freeDirEnt = GetNextFreeDirEntryIdx();
					DirectoryEntryType di;
					
					//Construct the directory entry.
					memset(&di, 0, sizeof(di));									
					memset(&di, ' ', sizeof(di.FileName));									
					CreateFSName(file, di.FileName);
					di.UsrCode = file->User;
					
					if (dirEntIdx < reqDirEnt - 1)
						di.RecCnt = 0x80;
					else
						di.RecCnt = (byte)ceil((double)(file->Length%CPM_EXT_ALLOC)/CPM_REC_SIZE);
					
					for (byte auIdx = dirEntIdx * CPM_AUInExt, auInExt = 0; auInExt < CPM_AUInExt && auIdx < reqAUCnt; auIdx++, auInExt++)
					{
						if (CPM_AUInExt == 8)
							di.AllocUnits.wordIdx[auInExt] = (word)fileAUs[auIdx];
						else if (CPM_AUInExt == 16)
							di.AllocUnits.byteIdx[auInExt] = (byte)fileAUs[auIdx];
					}

					//TBD: fix using ExtIdx2.
					di.ExtIdx = dirEntIdx;

					DirEntries[freeDirEnt] = di;
					FS_DirEntryMap[freeDirEnt] = true;				
				}
			
				if (!WriteDirectory())
					LastError = ERR_PHYSICAL;
				else
					ReadDirectory();
			}			
		}
	}	

	return LastError == ERR_NONE;
}

//Returns the length of the data in the last file's allocation unit, based on record count and sector size.
word CFSCPM::GetLastExtRecCnt(CFileCPM* file)
{
	CFSCPM::DirectoryEntryType lastDirEnt = DirEntries[file->FileDirEntries[file->FileDirEntries.size() - 1]];	
	return lastDirEnt.RecCnt;
}

//Performs physical disk format, but also adds boot and directory information, if any.
bool CFSCPM::Format()
{	
	return this->Disk->FormatDisk();
}

bool CFSCPM::Delete(char* fnames)
{
	bool res = true;
	CFileCPM* f = dynamic_cast<CFileCPM*>(FindFirst(fnames));
	
	while (f != NULL)
	{		
		for (word entIdx = 0; entIdx < f->FileDirEntries.size(); entIdx++)
			this->DirEntries[f->FileDirEntries[entIdx]].UsrCode = Disk->DiskDefinition.Filler;		

		f = dynamic_cast<CFileCPM*>(FindNext());
	}

	res = WriteDirectory();	
	if (res)
		res = ReadDirectory();			

	return res;
}

bool CFSCPM::Rename(CFileCPM* f, char* newName)
{		
	bool res = true;

	if (FindFirst(newName) != NULL)
	{
		LastError = ERR_FILE_EXISTS;
		res = false;
	}
		
	if (res)
		res = f->SetFileName(newName);	

	if (res)
		for (byte fDirEnt = 0; fDirEnt < f->FileDirEntries.size() && res; fDirEnt++)
			res = CreateFSName(f, DirEntries[f->FileDirEntries[fDirEnt]].FileName);	

	if (res)
		res = WriteDirectory() && ReadDirectory();

	return res;
}

word CFSCPM::GetNextFreeAllocUnit()
{
	word auRes = 0;
	vector<bool>::const_iterator i = find(FS_BlockMap.begin(), FS_BlockMap.end(), false);
	if (i != FS_BlockMap.end())
		auRes = (word)(i - FS_BlockMap.begin());	

	return auRes;
}


word CFSCPM::GetAUNoFromExt(word extIdx, byte auPos)
{
	word auNo = 0;

	if (CPM_AUInExt == 16)
		auNo = DirEntries[extIdx].AllocUnits.byteIdx[auPos];
	else if (CPM_AUInExt == 8)
		auNo = DirEntries[extIdx].AllocUnits.wordIdx[auPos];

	return auNo;
}


word CFSCPM::GetNextFreeDirEntryIdx()
{
	word dirEntRes = -1;
	vector<bool>::const_iterator i = find(FS_DirEntryMap.begin(), FS_DirEntryMap.end(), false);
	if (i != FS_DirEntryMap.end())
		dirEntRes = (word)(i - FS_DirEntryMap.begin());	

	return dirEntRes;	
}

CFSCPM::FileAttributes CFSCPM::GetAttributes(CFileCPM* file)
{
	FileAttributes fa = ATTR_NONE;

	if ((DirEntries[file->FileDirEntries[0]].FileName[8] & 0x80) == 0x80)	
		*(byte*)&fa |= (byte)ATTR_READ_ONLY;
	if ((DirEntries[file->FileDirEntries[0]].FileName[9] & 0x80) == 0x80)	
		*(byte*)&fa |= (byte)ATTR_SYSTEM;
	if ((DirEntries[file->FileDirEntries[0]].FileName[10] & 0x80) == 0x80)	
		*(byte*)&fa |= (byte)ATTR_ARCHIVE;

	return fa;
}

bool CFSCPM::SetAttributes(char* filespec, FileAttributes toSet, FileAttributes toClear)
{	
	bool res = true;

	if (filespec == NULL)
	{
		res = false;
		LastError = ERR_BAD_PARAM;
	}

	CFileCPM* file = res ? (CFileCPM*)this->FindFirst(filespec) : NULL;	

	while (res && file != NULL)
	{		
		FileAttributes faOrig = this->GetAttributes(file);

		for (byte feIdx = 0; feIdx < file->FileDirEntries.size(); feIdx++)
		{
			if ((toSet & (byte)ATTR_READ_ONLY) == (byte)ATTR_READ_ONLY)
				DirEntries[file->FileDirEntries[feIdx]].FileName[8] |= 0x80;			
			if ((toClear & (byte)ATTR_READ_ONLY) == (byte)ATTR_READ_ONLY)
				DirEntries[file->FileDirEntries[feIdx]].FileName[8] &= 0xFF ^ 0x80;

			if ((toSet & (byte)ATTR_SYSTEM) == (byte)ATTR_SYSTEM)	
				DirEntries[file->FileDirEntries[feIdx]].FileName[9] |= 0x80;
			if ((toClear & (byte)ATTR_SYSTEM) == (byte)ATTR_SYSTEM)	
				DirEntries[file->FileDirEntries[feIdx]].FileName[9] &= 0xFF ^ 0x80;

			if ((toSet & (byte)ATTR_ARCHIVE) == (byte)ATTR_ARCHIVE)	
				DirEntries[file->FileDirEntries[feIdx]].FileName[10] |= 0x80;		
			if ((toClear & (byte)ATTR_ARCHIVE) == (byte)ATTR_ARCHIVE)	
				DirEntries[file->FileDirEntries[feIdx]].FileName[10] &= 0xFF ^ 0x80;
		}

		file = (CFileCPM*)this->FindNext();
	}	

	if (res)
		res = this->WriteDirectory();

	return res;
}

bool CFSCPM::GetFileFolder(CFile* file, char* folderName)
{
	if (file != NULL && folderName != NULL)			
		sprintf(folderName, "%c", '0' + ((CFileCPM*)file)->User);	
	
	return file != NULL && folderName != NULL;
}

bool CFSCPM::SetFileFolder(CFile* file, char* folderName)
{
	if (file != NULL && folderName != NULL && strlen(folderName) == 1 &&
		isdigit(folderName[0]))
		((CFileCPM*)file)->User = folderName[0] - '0';

	return file != NULL && folderName != NULL;
}

bool CFSCPM::CreateFSName(CFile* file, char* fNameOut)
{
	bool res = CFileSystem::CreateFSName(file, fNameOut);	
	//CPM is by default not case sensitive, and uses uppercase file names.
	if ((GetFSFeatures() & CFileSystem::FSFT_CASE_SENSITIVE_FILENAMES) == 0)
		strupr(fNameOut);
	return res;
}

bool CFSCPM::OpenFile(CFile* file)
{
	CFileCPM* f = (CFileCPM*)file;
	
	CFSCPM::CPMFileListType::iterator fit = find(CPM_FileList.begin(), CPM_FileList.end(), *f);
	if (fit != CPM_FileList.end())
	{		
		f->isOpened = true;		
		f->Length = GetFileSize(f);
	}
	else	
		f->isOpened = false;

	return f->isOpened;
}

bool CFSCPM::CloseFile(CFile* file)
{
	CFileCPM* f = (CFileCPM*)file;
	if (f->isOpened)
	{
		f->isOpened = false;	
		f->Length = 0;		
		if (f->buffer != NULL)
		{
			delete f->buffer;
			f->buffer = NULL;
		}

		//delete f;
		return true;
	}
	else
		return false;
}