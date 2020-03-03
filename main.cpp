//////////////////////////////////////////////////////////////////////////
//HCDisk2, (C) George Chirtoaca 2014
//////////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <conio.h>

#include <list>
#include <vector>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

#include <Windows.h>
#include <WinCon.h>

#include "dsk.h"
#include "edsk.h"
#include "cfscpm.h"
#include "DiskImgRaw.h"
#include "DiskWin32.h"
#include "DiskImgCQM.h"
#include "DiskImgTD0.h"
#include "CFilePlus3.h"
#include "CFileHC.h"
#include "CFileTRD.h"
#include "CFSTRD.h"
#include "CFSCPMHC.h"
#include "CFSCPMPlus3.h"
#include "CFSTRDSCL.h"
#include "CFSCobraDEVIL.h"
#include "CFSOpus.h"
#include "CFSMGT.h"
#include "FileConverters/BasicDecoder.h"
#include "FileConverters/scr2gif.c"
#include "FileConverters/dz80/dissz80.h"
#include "CFileArchive.h"
#include "CFileArchiveTape.h"
#include "Tape\Tape2Wave.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define new new( _NORMAL_BLOCK , __FILE__ , __LINE__ )	
#endif
#endif


using namespace std;

CDiskBase* theDisk = NULL;
CFileArchive* theFS = NULL;

typedef enum
{
	STOR_NONE,

	STOR_REAL,
	STOR_RAW,
	STOR_DSK,
	STOR_EDSK,
	STOR_TRD,	
	STOR_SCL,
	STOR_CQM,
	STOR_OPD,
	STOR_MGT,
	STOR_TAP,
	STOR_TZX,	
	STOR_TD0,

	STOR_LAST
} StorageType;

static char* StorageTypeNames[] = 
{
	"NONE",
	"REAL",
	"RAW",
	"DSK",
	"EDSK",
	"TRD",
	"SCL",
	"CQM",
	"OPD",
	"MGT",
	"TAP",
	"TZX",
	"TD0"
};

StorageType storType;

string GetHexPrint(byte * buff, dword len)
{
	string res;		

	char buf[80];
	byte bufIdx = 0;
	dword lenEven = (dword)ceil((float)len/16) * 16;	

	for (dword c = 0; c < lenEven; c++)
	{		
		if (c % 16 == 0)
			bufIdx += sprintf(&buf[bufIdx], "%04X: ", c);			

		if (c < len)
			bufIdx += sprintf(&buf[bufIdx], "%02X ",  buff[c]);
		else
			bufIdx += sprintf(&buf[bufIdx], "   ");

		if ((c+1) % 16 == 0)
		{
			bufIdx += sprintf(&buf[bufIdx], "| ");
			for (int a = 0; a < 16; a++)
			{
				byte chr = buff[c - 15 + a];
				if (chr < ' ' || chr > 128)
					chr = '.';
				bufIdx += sprintf(&buf[bufIdx], "%c", (c - 15 + a  < len ? chr : ' '));
			}
			
			bufIdx += sprintf(&buf[bufIdx], "\r\n");
			
			res += string(buf);			
			bufIdx = 0;
		}
		else if ((c+1) % 8 == 0)
			bufIdx += sprintf(&buf[bufIdx], "- ");
	}
	
	return res;
}

string GetExtension(string fileName)
{
	int dotIdx = fileName.find_last_of('.');
	string res = "";
	if (dotIdx != string::npos)
		res = fileName.substr(dotIdx + 1);
	if (res.length() <= 3)
		return res;
	else
		return "";
}


bool Confirm(char* msg)
{
	printf("%s Press [ENTER] to confirm, [ESC] to cancel.\n", msg);
	char c = getch();
	while (c != 27 && c != 13)
		c = getch();

	return c == 13;
}

typedef enum
{
	FS_CPM_GENERIC,
	FS_CPM_HC_BASIC,
	FS_CPM_PLUS3_BASIC,
	FS_TRDOS,
	FS_TRDOS_SCL,
	FS_COBRA_DEVIL,
	FS_OPUS_DISCOVERY,
	FS_MGT_PLUSD,
	//FS_FAT12
} FSType;

class FS 
{
public:
	char* Name;
	FSType fsType;	
	CDiskBase::DiskDescType diskDef;		
	CFileSystem::FSParamsType fsParams;
	word otherParams[5];
};

FS diskTypes[] =
{
	//Name, Type,
	//Geometry
	//Block size, block count, catalog capacity, boot track count
	//Extra params. For CPM: extensions in catalog entry, software interleave factor.
	//Some values are informative, or for disk detection. Most FS classes recalculate FS params dynamically based on disk geometry.
	{ 
		"HC BASIC 5.25\"", FS_CPM_HC_BASIC, 
		{40, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 
		{2048, 160, 128, 0},
		{1, 1}
	},

	{
		"HC BASIC 3.5\"", FS_CPM_HC_BASIC,
		{80, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 320, 128, 0}, 
		{1, 1}
	},

	{
		"CP/M 2.2 5.25\"", FS_CPM_GENERIC,
		{40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 175, 64, 2}, 
		{2, 1}
	},

	{
		"CP/M 2.2 3.5\"", FS_CPM_GENERIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 351, 128, 4}, 
		{1, 1}
	},

	{
		"Spectrum +3 BASIC 3\" 180K", FS_CPM_PLUS3_BASIC,
		{40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 175, 64, 1}, 
		{1, 0}
	},

	{
		"Spectrum +3 3\" BASIC 203K", FS_CPM_PLUS3_BASIC,
		{42, 1, 10, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 6}, 		
		{1024, 205, 64, 1}, 
		{1, 2}
	},

	{
		"Spectrum +3 BASIC 3\" 720K", FS_CPM_PLUS3_BASIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 357, 256, 1}, 
		{1, 0}
	},

	{
		"Spectrum +3 BASIC PCW", FS_CPM_PLUS3_BASIC,
		{40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 175, 64, 1}, 
		{1, 0}
	},

	{
		"Spectrum +3 CP/M SSDD", FS_CPM_GENERIC,
		{40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 175, 64, 1}, 
		{1, 0}
	},

	{
		"Spectrum +3 CP/M DSDD", FS_CPM_GENERIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 357, 256, 1}, 
		{1, 0}
	},	

	{
		"TRDOS DS 3.5\"", FS_TRDOS,
		{ 80, 2, 16, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{256, (80 * 2 - 1) * 16, 128, 1}
		//{}
	},

	{
		"TRDOS DS 5.25\"", FS_TRDOS,
		{ 40, 2, 16, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_5_25, 0},
		{256, (40 * 2 - 1) * 16, 128, 1}
		//{}
	},

	{
		"TRDOS SS 3.5\"", FS_TRDOS,
		{ 80, 1, 16, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{256, (80 * 1 - 1) * 16, 128, 1}
		//{}
	},

	{
		"TRDOS SS 5.25\"", FS_TRDOS,
		{ 40, 1, 16, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_5_25, 0},
		{ 256, (40 * 1 - 1) * 16, 128, 1 }
		//{}
	},

	{
		"Opus Discovery 40T SS", FS_OPUS_DISCOVERY, 
		{ 40, 1, 18, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{256, 720, 112}
	},

	{
		"Opus Discovery 40T DS", FS_OPUS_DISCOVERY, 
		{ 40, 2, 18, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{256, 1440, 112}
	},

	{
		"Opus Discovery 80T SS", FS_OPUS_DISCOVERY, 
		{ 80, 1, 18, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{256, 2880, 112}
	},

	{
		"Opus Discovery 80T DS", FS_OPUS_DISCOVERY, 
		{ 80, 2, 18, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{256, 5760, 112}
	},

	{
		"MGT +D", FS_MGT_PLUSD,
		{ 80, 2, 10, CDiskBase::SECT_SZ_512, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0},
		{ 512, 3125, 80}
	},
	
	{
		//HardSkew = 2.
		"CoBra Devil", FS_COBRA_DEVIL,
		{80, 2, 18, CDiskBase::SECT_SZ_256, 0xE5, 20, CDiskBase::DISK_DATARATE_DD_3_5, 2}, 		
		{9216, 77, 108}
		//{}
	},	
	
	{
		"Electronica CIP-04", FS_CPM_PLUS3_BASIC,
		{80, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 355, 64, 1}, 
		{1, 0}
	},

/*
	{
		"FAT12", FS_FAT12,
		{80, 2, 18, CDiskBase::SECT_SZ_512, 0xF6, 0x1B, CDiskBase::DISK_DATARATE_HD, 0}, 		
		{1024, 355, 64, 1} 		
	},
*/
};
/*
{
	//40 tracks x 2 heads x 16 sect/track x 256 B/sect = 327680 B
	//2048 B / alloc. unit => 160 alloc. units
	//0 tracks for boot = 0 alloc units => 160 alloc. units free
	//2 alloc unit for dir. => 4096/32 = 128 dir. entries max.	
	{"HC BASIC 5.25", {40, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{2048, 160, 128, 0, 1, 1}},

	//80 tracks x 2 heads x 16 sect/track x 256 B/sect = 655360 B
	//2048 B / alloc. unit => 320 alloc. units
	//0 tracks for boot = 0 alloc units => 320 alloc. units free
	//2 alloc unit for dir. => 4096/32 = 128 dir. entries max.	
	{"HC BASIC 3.5", {80, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{2048, 320, 128, 0, 1, 1}},

	//40 tracks x 2 heads x 9 sect/track x 512 B/sect = 368640 B
	//2048 B/alloc. unit
	//2 tracks for boot => 359.424B allocatable/2048B = 175.5 alloc. units
	//1 alloc unit for dir. => 2048/32 = 64 dir. entries max.	
	{"HC CP/M 5.25", {40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{2048, 175, 64, 2, 2, 1}},

	//80 tracks x 2 heads x 9 sect/track x 512 B/sect = 737280 B
	//2048 B / alloc. unit => 360 alloc. units
	//4 tracks for boot = 9 alloc units => 351 alloc. units free
	//2 alloc unit for dir. => 4096/32 = 128 dir. entries max.	
	{"HC CP/M 3.5", {80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{2048, 351, 128, 4, 1, 1}},

	//40 tracks x 1 heads x 9 sect/track x 512 B/sect = 184.320 B
	//1024 B/alloc. unit
	//1 tracks for boot => 179.712 allocatable/2048B = 175.5 alloc. units
	//1 alloc unit for dir. => 2048/32 = 64 dir. entries max.	
	{"Spectrum +3 SSDD", {40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{1024, 175, 64, 1, 1, 0}},

	//732.672 B
	//357,75 AU	
	{"Spectrum +3 DSDD", {80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{2048, 357, 256, 1, 1, 0}},

	//209.920 B
	//205 AU
	{"Spectrum HiForm 203", {42, 1, 10, CDiskBase::SECT_SZ_512, 0xE5, 0x58, CDiskBase::DISK_DATARATE_DD_3_5}, 		
		{1024, 205, 64, 1, 1, 2}}
};
*/

long fsize(char* fname)
{
	long fs;

	FILE* f = fopen(fname, "rb");
	if (f != NULL)
	{
		fseek(f, 0, SEEK_END);
		fs = ftell(f);
	}
	else
		fs = 0;

	return fs;	
}

bool ShowKnownFS(int argc, char* argv[])
{
	puts("Idx\tName\t\t\t|Geometry\t|Bl.Sz.\t|Bl.Cnt\t|Dir.\t|Boot");
	puts("--------------------------------------------------------------------");
	for (byte fsIdx = 0; fsIdx < sizeof(diskTypes)/sizeof(diskTypes[0]); fsIdx++)
	{
		printf("%2d. %-20s\t|%dx%dx%dx%d\t", fsIdx+1, diskTypes[fsIdx].Name, 			
			diskTypes[fsIdx].diskDef.TrackCnt, diskTypes[fsIdx].diskDef.SideCnt, 
			diskTypes[fsIdx].diskDef.SPT, diskTypes[fsIdx].diskDef.SectSize);
		printf("|%d\t|%d\t|%d\t|%d\n", diskTypes[fsIdx].fsParams.BlockSize, diskTypes[fsIdx].fsParams.BlockCount, 
			diskTypes[fsIdx].fsParams.DirEntryCount, diskTypes[fsIdx].fsParams.BootTrackCount);
	}
	printf("Known data sources: %s", StorageTypeNames[STOR_NONE + 1]);
	for (byte dsIdx = STOR_NONE + 2; dsIdx < STOR_LAST; dsIdx++)
		printf(", %s", StorageTypeNames[dsIdx]);
	printf("\n");

	return true;
}

int FileSorterByName(CFile* f1, CFile* f2)
{
	CFileSystem::FileNameType fname1, fname2;
	f1->GetFileName(fname1);
	f2->GetFileName(fname2);
	return string(fname1) < string(fname2);
}

int FileSorterBySize(CFile* f1, CFile* f2)
{	
	return f1->GetLength() < f2->GetLength();
}

int FileSorterByType(CFile* f1, CFile* f2)
{
	return dynamic_cast<CFileSpectrum*>(f1)->SpectrumType < dynamic_cast<CFileSpectrum*>(f2)->SpectrumType;
}

void DisplayTZXComments()
{
	CTapFile* theTap = ((CFileArchiveTape*)theFS)->theTap;
	CTapeBlock* tb = new CTapeBlock();
	bool bOK = true;
	for (word i = 0; i < theTap->GetBlockCount() && bOK; i++)
	{				
		if (bOK = theTap->GetBlock(i, tb) && tb->m_BlkType == CTapeBlock::TAPE_BLK_METADATA)
		{		
			CTZXFile* tzx = (CTZXFile*)theTap;
			char msg[256];
			CTZXFile::TZXBlkArhiveInfo* blkArh;
			CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;

			switch (tzx->m_CurrBlkID)
			{					
			case CTZXFile::BLK_ID_TXT_DESCR:
				strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
				msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
				printf("Message: '%s'\n", msg);
				break;
			case CTZXFile::BLK_ID_ARH_INF:
				blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb->Data;
				dword bufIdx = sizeof(word) + sizeof(byte);
				blkTxt = &blkArh->TxtItem;

				printf("Arhive Info:\n");
				for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
				{													
					char* line;
					byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ? 
						CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

					bufIdx += sizeof(byte)*2;								

					printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
					line = msg;
					strncpy(msg, (char*)&tb->Data[bufIdx], blkTxt->TxtLen);
					msg[blkTxt->TxtLen] = 0;

					if ((line = strtok(msg, "\r")) != NULL)
					{
						do 
						{
							printf("%s%s\n", line == msg ? "" : "\t\t\t", line);									
						} while(line = strtok(NULL, "\r"));									
					}

					bufIdx += blkTxt->TxtLen;																															
					blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb->Data[bufIdx];
				}
				break;
			}
		}			
	}				

	delete tb;
}

bool Cat(int argc, char* argv[])
{	
	if (theFS == NULL)
		return false;

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	bool HasAttr = (theFS->GetFSFeatures() & CFileSystem::FSFT_FILE_ATTRIBUTES) > 0;	
	bool HasFolders = (theFS->GetFSFeatures() & CFileSystem::FSFT_FOLDERS) > 0;	
	bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;

	enum SortType
	{
		ST_NONE,
		ST_NAME,
		ST_SIZE,
		ST_TYPE
	};

	string wildCard = "*";
	bool bExtendedCat = true;
	byte sortType = ST_NONE;
	for (byte ai = 0; ai < argc; ai++)
	{
		string a = (char*)argv[ai];
		if (a == "-sn")
			sortType = ST_NAME;
		else if (a == "-ss")
			sortType = ST_SIZE;
		else if (a == "-st")
			sortType = ST_TYPE;
		else if (a == "-ne")
			bExtendedCat = false;
		else
			wildCard = a;
	}	
	if (sortType != ST_NONE)
		bExtendedCat = true;
			
	CFileSystem::FileNameType fName = "";	
	char fExt[4] = "";	


	//For tzx files, display comments and archive info.
	if (!IsDisk)			
		DisplayTZXComments();

	printf("\nIDX");
	if (HasFolders)
		printf("\tFolder");
	printf("\tName\t\tSize(KB)");
	if (HasAttr)
		printf("\tAttr");
	if (IsBasic && bExtendedCat)
		printf("\tType\tStart\tBasLen\n");
	else
		printf("\n");
	printf("------------------------------------------------------------------------------\n");	
	
	list<CFile*> lstAllFiles;
	CFile* f = theFS->FindFirst((char*)wildCard.c_str());
	while (f != NULL)
	{
		lstAllFiles.push_back(f);
		if (bExtendedCat)
			theFS->OpenFile(f); //to get the type, but may be slower.
		f = theFS->FindNext();
	}


	if (sortType != ST_NONE)
	{
		if (sortType == ST_NAME)
			lstAllFiles.sort(FileSorterByName);
		else if (sortType == ST_SIZE)
			lstAllFiles.sort(FileSorterBySize);
		else if (sortType == ST_TYPE && IsBasic)
			lstAllFiles.sort(FileSorterByType);
	}	

	int fileIdx = 1;	
	list<CFile*>::iterator it = lstAllFiles.begin();	

	while (it != lstAllFiles.end())
	{					
		f = *it;
		
		CFileSystem::FileAttributes fa = theFS->GetAttributes(f);										
		f->GetFileName(fName, fExt);

		printf("%3d", fileIdx++);
		if (HasFolders)
		{
			char folderName[CFileSystem::MAX_FILE_NAME_LEN];
			if (theFS->GetFileFolder(f, folderName))
				printf("\t%s", folderName);
		}

		printf("\t%-8s%c%-3s\t%5.2f", 				
			fName, (strlen(fExt) > 0 ? '.' : ' '), (strlen(fExt) > 0 ? fExt : ""),				
			(float)theFS->GetFileSize(f, true)/1024);		

		if (HasAttr)
			printf("\t\t%c%c%c", 
			(((byte)fa & (byte)CFileSystem::ATTR_READ_ONLY) > 0 ? 'R' : '-'),
			(((byte)fa & (byte)CFileSystem::ATTR_SYSTEM) > 0 ? 'S' : '-'),
			(((byte)fa & (byte)CFileSystem::ATTR_ARCHIVE) > 0 ? 'A' : '-'));
		else
			printf("\t");


		if (IsBasic && bExtendedCat)
		{									
			CFileSpectrum* s = dynamic_cast<CFileSpectrum*>(f);
			if (s != NULL)
			{
				char start[6] = "", len[6] = "";
				CFileSpectrum::SpectrumFileType st = s->GetType();								

				if (st == CFileSpectrum::SPECTRUM_PROGRAM && s->SpectrumStart != (word)-1)
					itoa(s->SpectrumStart, start, 10);											
				else if (st == CFileSpectrum::SPECTRUM_BYTES)
					itoa(s->SpectrumStart, start, 10);
				else if (st == CFileSpectrum::SPECTRUM_CHAR_ARRAY || st == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
					sprintf(start, "%c", s->GetArrVarName());
				
				if (st != CFileSpectrum::SPECTRUM_UNTYPED)
					itoa(s->SpectrumLength, len, 10);
									
				printf("\t%s\t%5s\t%5s", 
					CFileSpectrum::SPECTRUM_FILE_TYPE_NAMES[st], start, len);										
			}				
		}
		
		theFS->CloseFile(f);		
		//delete f;
		printf("\n");		
			
				
		it++;
	};
	
	if (IsDisk)
	{
		CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
		printf("Space free/filled/total\t: %03d/%03d/%03d KB\n",		
			fs->GetDiskLeftSpace()/1024,
			fs->GetDiskMaxSpace()/1024 - fs->GetDiskLeftSpace()/1024,
			fs->GetDiskMaxSpace()/1024);	
	}	

	return true;
}


bool GetFile(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;	

	bool asText = (argc >= 2 && strcmp((char*)argv[1], "-t") == 0);
	CFile* f = theFS->FindFirst((char*)argv[0]);	
	CFileSystem::FileNameType fn;
	bool res = false;
		
	while (f != NULL)
	{			
		f->GetFileName(fn);
		if (!theFS->OpenFile(f))
			printf("Couldn't open file '%s'!\n", fn);
		else
		{						
			FILE* pcFile = fopen(fn, "wb");
			if (pcFile != NULL)
			{							
				if (theFS->ReadFile(f))
				{
					dword len = f->GetLength();					
					byte wrap = 0;
					if (strstr(fn, ".TAS"))
						wrap = 64;
					byte* buf1 = new byte[(dword)(len * 1.33)];
					if (asText)
						len = f->GetDataAsText(buf1, wrap);
					else
						f->GetData(buf1);				
					fwrite(buf1, 1, len, pcFile);
					fclose(pcFile);
					delete buf1;							
					printf("Wrote file %s\n", fn);		
					res = true;
				}				
				else
					printf("Could not read file %s\n", fn);				
			}												

			theFS->CloseFile(f);
		}
		
		f = theFS->FindNext();
	}

	return res;
}

string DecodeBASIC(byte* buf, word progLen, word varLen = 0)
{
	Basic::BasicDecoder BasDec;		
	word lineSize = 0;
	word bufPos = 0;
	stringstream res;

	do
	{
		lineSize = BasDec.GetLine(buf + bufPos, progLen - varLen - bufPos);		
		bufPos+= lineSize;			
		BasDec.DecodeLine(res);
	}
	while (lineSize > 0 && bufPos < progLen - varLen);

	if (varLen > 0)	
		BasDec.DecodeVariables(&buf[bufPos], varLen, res);	

	return res.str();
}

string Disassemble(byte* buf, word len, word addr = 0)
{
	DISZ80	*d;			/* Pointer to the Disassembly structure */
	int		err;		/* line count */		
	string res;

	d = (DISZ80 *)malloc(sizeof(DISZ80));	
	if (d != NULL)
	{
		memset(d, 0, sizeof(DISZ80));
		dZ80_SetDefaultOptions(d);		
		d->cpuType = DCPU_Z80;		
		d->flags |= (DISFLAG_SINGLE | DISFLAG_LABELLED | DISFLAG_ADDRDUMP | DISFLAG_OPCODEDUMP | DISFLAG_UPPER);		

		//Fix address.
		byte* buf1 = new byte[64 * 1024];
		memcpy(&buf1[addr], buf, len);		
		d->mem0Start = buf1;
		d->start = d->end = addr;
		err = dZ80_Disassemble(d);

		stringstream s;
		s.unsetf(ios::skipws);
		s.setf(ios::uppercase);
		while (err == DERR_NONE && len > 0) 		
		{							
			s << setw(4) << hex << setfill('0') << addr << "H\t" << d->disBuf << "\t" << d->commentBuf << "\r\n";												
			
			addr += (word)d->bytesProcessed;		
			//Instruction/buffer boundary mismatch.
			if (d->bytesProcessed < len)
				len -= (word)d->bytesProcessed;
			else
				len = 0;
			d->start = d->end = addr;

			err = dZ80_Disassemble(d);						
		}

		res = s.str();
		delete buf1;
	}

	free(d);	
	return res;
}

void GetWindowSize( int& lines, int& columns )
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(
		GetStdHandle( STD_OUTPUT_HANDLE ),
		&csbi
		))
	{
		lines   = csbi.srWindow.Bottom -csbi.srWindow.Top  +1;
		columns = csbi.srWindow.Right  -csbi.srWindow.Left +1;
	}
	else 
		lines = columns = 0;
}

void PrintIntense(char* str)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hCMD = GetStdHandle( STD_OUTPUT_HANDLE );

	if (GetConsoleScreenBufferInfo(hCMD,&csbi))
	{		
		SetConsoleTextAttribute(hCMD, csbi.wAttributes | FOREGROUND_INTENSITY);

		WriteConsole(hCMD, str, strlen(str), NULL, NULL);
		
		SetConsoleTextAttribute(hCMD, csbi.wAttributes);
	}
}

void TextViewer(string str)
{		
	int winLines, winColumns;
	GetWindowSize(winLines, winColumns);

	const word lnCnt = winLines - 2;
	long off1 = 0, off2 = 0;
	long lnIdx = 0;
	long len = str.length();	

		
	while (off1 < len && off2 != string::npos)
	{
		do
		{
			off2 = str.find("\r\n", off1);	
			if (off1 != off2)
				cout << str.substr(off1, off2 - off1) << endl;
			else
				cout << endl;

			off1 = off2+2;		
			lnIdx++;
		}
		while (off2 != string::npos && lnIdx < lnCnt && off1 < len);

		if (off1 < len && off2 != string::npos)
		{			
			char buf[80];
			sprintf(buf, "Press a key to continue, ESC to cancel. Progress: %d%%\n", (int)(((float)off1/len)*100));			
			PrintIntense(buf);
			char key = getch();
			if (key == 27)
				break;
			else			
				lnIdx = 0;							
		}		
	}
}

bool TypeFile(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	bool asHex = argc >= 2 && (strcmp(argv[1], "-h") == 0);	
	bool asText = argc >= 2 && (strcmp(argv[1], "-t") == 0);
	char* fname = argv[0];
	CFile* f = theFS->FindFirst(fname);		
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;				

	if (f != NULL && theFS->OpenFile(f) && theFS->ReadFile(f))		
	{		
		CFileSystem::FileNameType fn;
		f->GetFileName(fn);		
		CFileArchive::TrimEnd(fn);
		dword len = f->GetLength();
		byte* buf1 = new byte[theFS->GetFileSize(f, true)];
		
		if (!asHex && IsBasic)
		{
			bool isProgram = false, isSCR = false, isBytes = false;		
			CFileSpectrum* s = dynamic_cast<CFileSpectrum*>(f);				
			isProgram = s->GetType() == CFileSpectrum::SPECTRUM_PROGRAM;
			isBytes = s->GetType() == CFileSpectrum::SPECTRUM_BYTES;
			isSCR = isBytes && s->SpectrumLength == 6912;			

			if (isProgram)
			{
				f->GetData(buf1);
				TextViewer(DecodeBASIC(buf1, s->SpectrumLength, s->SpectrumVarLength));
			}
			else if (isSCR)
			{
				f->GetData(buf1);				
				char* fntmp = _tempnam(NULL, fn);				
				strcat(fntmp, ".gif");
				ConvertSCR2GIF(buf1, fntmp);												
				system(fntmp);
				free(fntmp);	
				remove(fntmp);
			}
			else if (isBytes)
			{							
				if (strstr(fn, ".TAS"))
					asText = true;
				else
				{
					f->GetData(buf1);												
					TextViewer(Disassemble(buf1, (word)len, s->SpectrumStart));
				}				
			}
			else
				asText = true;
		}			
		else
			asText = true;	

		if (asHex)
		{						
			f->GetData(buf1);			
			TextViewer(GetHexPrint(buf1, len));
		}				
		else if (asText)
		{			
			byte wrap = 80;
			if (IsBasic /*&& strstr(fn, ".TAS")*/)			
				wrap = 64;			
			
			//Adjust buffer length for line wrap.
			dword lenToSet = len + (len/wrap) * 2;		
			byte* buf2 = new byte[lenToSet];

			dword txtLen = f->GetDataAsText(buf2, wrap);
			if (txtLen > 0)
				buf2[txtLen - 1] = '\0';						
			TextViewer((char*)buf2);
			delete buf2;
		}

		delete buf1;		
		theFS->CloseFile(f);
	}					
	else	
		return false;
	

	return true;
}

//Will return a list of indexes for matching geometries for the specified geometry.
vector<byte> GetMatchingGeometriesByGeometry(CDiskBase::DiskDescType x)
{
	vector<byte> res;
	
	for (byte fsIdx = 0; fsIdx < sizeof(diskTypes)/sizeof(diskTypes[0]); fsIdx++)
	{
		CDiskBase::DiskDescType dd = diskTypes[fsIdx].diskDef;
		if (dd.TrackCnt == x.TrackCnt && dd.SideCnt == x.SideCnt && dd.SPT == x.SPT && dd.SectSize == x.SectSize)
			res.push_back(fsIdx);
	}

	return res;
}

//Will return a list of indexes for matching geometries by raw disk image size.
vector<byte> GetMatchingGeometriesByImgSize(dword imgSize)
{
	vector<byte> res;

	for (byte fsIdx = 0; fsIdx < sizeof(diskTypes)/sizeof(diskTypes[0]); fsIdx++)
	{
		CDiskBase::DiskDescType dd = diskTypes[fsIdx].diskDef;
		if (//diskTypes[fsIdx].fsType != FS_TRDOS && diskTypes[fsIdx].fsType != FS_TRDOS_SCL &&
			(dd.TrackCnt * dd.SideCnt * dd.SPT * dd.SectSize) == imgSize)
			res.push_back(fsIdx);
	}

	return res;
}

vector<byte> GetMatchingGeometriesByType(FSType fsType)
{
	vector<byte> res;

	for (byte fsIdx = 0; fsIdx < sizeof(diskTypes)/sizeof(diskTypes[0]); fsIdx++)
	{
		if (diskTypes[fsIdx].fsType == fsType)
			res.push_back(fsIdx);
	}

	return res;
}

bool Close(int argc, char* argv[])
{
	if (theFS != NULL)
	{				
		delete theFS;
		theFS = NULL;					
	}

	/*
	if (theDisk != NULL)
	{
		delete theDisk;
		theDisk = NULL;
	}
	*/
	
	return true;
}



CDiskBase* OpenDisk(char* path, StorageType& srcType, vector<byte>& foundGeom)
{	
	srcType = STOR_NONE;
	word fsIdx = 0;	
	FS theDiskDesc;
	theFS = NULL;

	string fileExt = GetExtension(path);

	if (stricmp(path, "A:") == 0)
	{
		puts("Auto detecting disk geometry and file system...");
		CDiskWin32* diskWin32 = new CDiskWin32();
		diskWin32->Open(path);
		byte sectCnt = 0;
		CDiskWin32::SectDescType si[CDiskBase::MAX_SECT_PER_TRACK];
		byte tracks, heads;

		if (diskWin32->GetTrackInfo(0, 0, sectCnt, si))
		{
			diskWin32->DiskDefinition.SectSize = CDiskBase::SectCode2SectSize(si->sectSizeCode);
			diskWin32->GetDiskInfo(tracks, heads);
			fsIdx = 0;			
		}

		CDiskBase::DiskDescType dd;
		dd.TrackCnt = tracks;
		dd.SideCnt = heads;
		dd.SPT = sectCnt;
		dd.SectSize = CDiskBase::SectCode2SectSize(si->sectSizeCode);
		foundGeom = GetMatchingGeometriesByGeometry(dd);

		//delete diskWin32;			
		theDisk = diskWin32;

		if (foundGeom.size() > 0)
			srcType = STOR_REAL;
	}	
	else if (fileExt == "DSK") //Detect image type.
	{			
		theDisk = new CDSK();
		if (!theDisk->Open(path, CDSK::OPEN_MODE_EXISTING))
		{						
			delete theDisk;
			theDisk = new CEDSK();
			if (theDisk->Open(path, CDSK::OPEN_MODE_EXISTING))			
				srcType = STOR_EDSK;
			else
			{
				delete theDisk;
				theDisk = NULL;
			}
		}
		else
			srcType = STOR_DSK;

		if (srcType == STOR_DSK || srcType == STOR_EDSK)		
			foundGeom = GetMatchingGeometriesByGeometry(theDisk->DiskDefinition);		
	}	
	else if (fileExt == "TRD")
	{
		foundGeom = GetMatchingGeometriesByType(FS_TRDOS);		
		byte foundFSIdx = 0;
		for (byte trdGeomIdx = 0; trdGeomIdx < foundGeom.size(); trdGeomIdx++)
		{						
			theDiskDesc = diskTypes[foundGeom[trdGeomIdx]];

			theDisk = new CDiskImgRaw(theDiskDesc.diskDef);
			if (((CDiskImgRaw*)theDisk)->Open(path))
			{
				theFS = new CFSTRDOS(theDisk, theDiskDesc.Name);				
				if (theFS->Init())
				{
					srcType = STOR_TRD;
					foundFSIdx = foundGeom[trdGeomIdx];
					break;				
				}			
				else
				{
					//delete theDisk;
					//theDisk = NULL;
					delete theFS;					
					theFS = NULL;
				}
			}	
			else
			{
				//delete theDisk;
				//theDisk = NULL;
				delete theFS;				
				theFS = NULL;
			}			
		}

		if (srcType == STOR_TRD)
		{
			foundGeom.clear();
			foundGeom.push_back(foundFSIdx);
		}		
		
	}
	else if (fileExt == "SCL")
	{		
		theFS = new CFSTRDSCL(path, StorageTypeNames[STOR_SCL]);				
		if (theFS->Init())
		{
			srcType = STOR_SCL;						
		}						
	}
	else if (fileExt == "CQM")
	{
		theDisk = new CDiskImgCQM();
		if (theDisk->Open(path, CDSK::OPEN_MODE_EXISTING))
		{
			foundGeom = GetMatchingGeometriesByGeometry(theDisk->DiskDefinition);		
			if (foundGeom.size() > 0)
				srcType = STOR_CQM;
		}
	}	
	else if (fileExt == "OPD" || fileExt == "OPU")
	{
		foundGeom = GetMatchingGeometriesByType(FS_OPUS_DISCOVERY);		
		byte foundFSIdx = 0;
		for (byte opdGeomIdx = 0; opdGeomIdx < foundGeom.size(); opdGeomIdx++)
		{						
			theDiskDesc = diskTypes[foundGeom[opdGeomIdx]];

			theDisk = new CDiskImgRaw(theDiskDesc.diskDef);
			if (((CDiskImgRaw*)theDisk)->Open(path))
			{
				theFS = new CFSOpus(theDisk, theDiskDesc.Name);				
				if (theFS->Init())
				{
					srcType = STOR_OPD;
					foundFSIdx = foundGeom[opdGeomIdx];
					break;				
				}			
				else
				{
					//delete theDisk;
					//theDisk = NULL;
					delete theFS;					
					theFS = NULL;
				}
			}	
			else
			{
				//delete theDisk;
				//theDisk = NULL;
				delete theFS;				
				theFS = NULL;
			}						
		}

		if (srcType == STOR_OPD)
		{
			foundGeom.clear();
			foundGeom.push_back(foundFSIdx);
		}		
	}
	else if (fileExt == "TAP")
	{
		theFS = new CFileArchiveTape(path);
		if (theFS->Init())
			srcType = STOR_TAP;
	}
	else if (fileExt == "TZX")
	{
		theFS = new CFileArchiveTape(path);
		if (theFS->Init())
			srcType = STOR_TZX;
	}
	else if (fileExt == "TD0")
	{
		theDisk = new CDiskImgTD0();
		if (theDisk->Open(path, CDSK::OPEN_MODE_EXISTING))			
			srcType = STOR_TD0;
					
		foundGeom = GetMatchingGeometriesByGeometry(theDisk->DiskDefinition);	
	}

	if (srcType == STOR_NONE) //Assume raw disk image.
	{
		dword imgSz = fsize(path);

		//detect fs by image size			
		foundGeom = GetMatchingGeometriesByImgSize(imgSz);				
		if (foundGeom.size() > 0)		
			srcType = STOR_RAW;
	}

	return theDisk;
}

void CheckTRD(char* path, vector<byte>& foundGeom)
{
	//Exclude TRD from list if it cannot be opened, because it has a signature, and can be tested.									
	CFileArchive* theFS = NULL;			
	CDiskBase* theDisk = NULL;

	vector<byte>::iterator it = foundGeom.begin();
	while (it != foundGeom.end())
	{
		FS theDiskDesc = diskTypes[*it];				

		if (theDiskDesc.fsType == FS_TRDOS)
		{
			if (theDisk == NULL)
			{
				theDisk = new CDiskImgRaw(theDiskDesc.diskDef);						
				if (theDisk->Open(path, CDiskBase::OPEN_MODE_EXISTING))				
					theFS = new CFSTRDOS(theDisk, theDiskDesc.Name);				
			}
			else
				theFS = new CFSTRDOS(theDisk, theDiskDesc.Name);				
			
			if (!theFS->Init())										
			{
				it = foundGeom.erase(it);					
				delete theDisk;
			}
			else
				it++;								
		}				
		else
			it++;
		
		delete theFS;		
	}
}

byte AskGeometry(vector<byte> foundGeom)
{
	bool bFoundSel = false, bCancel = false;
	byte fsIdx = 0xFF;

	for (byte fsMatchIdx = 0; fsMatchIdx < foundGeom.size(); fsMatchIdx++)				
		printf("%d. %s\n", fsMatchIdx+1, diskTypes[foundGeom[fsMatchIdx]].Name);

	do
	{
		printf("Select FS (empty for cancel): ");																					
		char buf[10];
		if (gets(buf) && strlen(buf) > 0 && (fsIdx = atoi(buf)) > 0)
			bFoundSel = (fsIdx - 1 < (word)foundGeom.size());
		else
			bCancel = true;

		if (bFoundSel && !bCancel)
			fsIdx = foundGeom[fsIdx-1];

	}while (!bFoundSel && !bCancel);

	return fsIdx;
}

bool Open(int argc, char* argv[])
{
	char* path = argv[0];	
	//CDiskBase* theDisk = NULL;
	FS theDiskDesc;
	word fsIdx = 0;	
	vector<byte> foundGeom;
	byte selGeom = 0;
	if (argc >= 3 && strcmp(argv[1], "-t") == 0)
		selGeom = atoi(argv[2]);

	/*
	if (theFS != NULL)
	{
		delete theFS;
		theFS = NULL;
	}
	
	
	if (theDisk != NULL)
	{
		delete theDisk;
		theDisk = NULL;
	}
	*/
	Close(0, NULL);

	strupr(path);	
	theDisk = OpenDisk(path, storType, foundGeom);	
	
	if (storType == STOR_NONE)
	{
		printf("%s is not a recognized disk/file system.\n", path);			
		theFS = NULL;
	}
	else
	{				
		char* dskFmt = StorageTypeNames[(int)storType];
		
		printf("Storage format is: %s. ", dskFmt);				

		bool bFoundSel = false, bCancel = false;		
		if (storType != STOR_TAP && storType != STOR_TZX && storType != STOR_SCL)
		{
			if (storType != STOR_REAL)
				CheckTRD(path, foundGeom);
			
			if (foundGeom.size() > 1 && selGeom == 0)
			{			
				printf("Select file system.\n");
				fsIdx = AskGeometry(foundGeom);				
				if (fsIdx >= 0)
					bFoundSel = true;
			}
			else if (foundGeom.size() == 1)
			{
				bFoundSel = true;
				fsIdx = foundGeom[0];					
			}
			else if (selGeom > 0)
			{
				//diskTypes
				if (selGeom < foundGeom.size())
					fsIdx = foundGeom[selGeom - 1];
				else
					fsIdx = selGeom - 1;

				bFoundSel = true;
			}
			else
				return NULL;
		}
		
		if (bFoundSel && storType != STOR_TAP && storType != STOR_TZX && storType != STOR_SCL)
		{
			theDiskDesc = diskTypes[fsIdx];
			bool isOpen = true;
			printf("File system is: %s.\n", theDiskDesc.Name);
			
			//These must be processed here, after the user choses the geometry/fs, as the geometry is not known for RAW images.
			if (storType == STOR_RAW)
			{
				/*
				if (theDisk != NULL)
					delete theDisk;
				*/

				theDisk = new CDiskImgRaw(theDiskDesc.diskDef);
				isOpen = ((CDiskImgRaw*)theDisk)->Open(path);				
			}			
			else if (storType == STOR_REAL)
			{				
				if (theDisk != NULL)
					delete theDisk;

				theDisk = new CDiskWin32(theDiskDesc.diskDef);
				isOpen = ((CDiskWin32*)theDisk)->Open(path);
			}			

			if (isOpen && theFS == NULL && theDisk != NULL)
			{
				if (theDiskDesc.fsType == FS_CPM_GENERIC)		
					theFS = new CFSCPM(theDisk, theDiskDesc.fsParams, *(CFSCPM::CPMParams*)&theDiskDesc.otherParams, theDiskDesc.Name);								
				else if (theDiskDesc.fsType == FS_CPM_HC_BASIC)		
					theFS = new CFSCPMHC(theDisk, theDiskDesc.fsParams, *(CFSCPM::CPMParams*)&theDiskDesc.otherParams, theDiskDesc.Name);								
				else if (theDiskDesc.fsType == FS_CPM_PLUS3_BASIC)
					theFS = new CFSCPMPlus3(theDisk, theDiskDesc.fsParams, *(CFSCPM::CPMParams*)&theDiskDesc.otherParams, theDiskDesc.Name);
				else if (theDiskDesc.fsType == FS_COBRA_DEVIL)
					theFS = new CFSCobraDEVIL(theDisk, theDiskDesc.Name);			
				else if (theDiskDesc.fsType == FS_TRDOS)
					theFS = new CFSTRDOS(theDisk, theDiskDesc.Name);
				else if (theDiskDesc.fsType == FS_OPUS_DISCOVERY)
					theFS = new CFSOpus(theDisk, theDiskDesc.Name);
				else if (theDiskDesc.fsType == FS_MGT_PLUSD)
					theFS = new CFSMGT(theDisk, theDiskDesc.Name);
				/*
				else if (theDiskDesc.fsType == FS_FAT12)
				{
					
				}
				*/
			}
						
			if (theFS == NULL || !theFS->Init())
			{
				Close(0, NULL);			
				puts("File system initialization failed.");
			}
		}		
		else
			printf("\n");
	}

	if (theFS != NULL)		
	{

		bool HasLabel = (theFS->GetFSFeatures() & (CFileSystem::FSFT_LABEL | CFileSystem::FSFT_DISK)) > 0;
		if (HasLabel)
		{
			char label[CFileSystem::MAX_FILE_NAME_LEN];
			if (((CFileSystem*)theFS)->GetLabel(label))
				printf("Disk label is: %s\n", label);
		}
		return true;
	}
	else
		return false;
}

word log2(word x)
{
	word log2 = 0;

	while ((x = x >> 1) > 0)
		log2++;

	return log2;
}

bool Stat(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	word feat = theFS->GetFSFeatures();

	printf("Storage: %s, File system: %s\n", StorageTypeNames[storType], theFS->Name);
	if (feat & CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
		printf("Disk geometry: Tracks: %d, Sides: %d, Sectors: %d, Sector Size: %d\n", 
			fs->Disk->DiskDefinition.TrackCnt, fs->Disk->DiskDefinition.SideCnt, 
			fs->Disk->DiskDefinition.SPT, fs->Disk->DiskDefinition.SectSize);
		printf("Block size: %.02f K, Blocks free/max: %d/%d, Space free/max KB: %d/%d\n", 
			(float)fs->GetBlockSize()/1024, fs->GetFreeBlockCount(), fs->GetMaxBlockCount(), 
			fs->GetDiskLeftSpace()/1024, fs->GetDiskMaxSpace()/1024);
		printf("Directory entries free/max: %d/%d\n", fs->GetFreeDirEntriesCount(), fs->GetMaxDirEntriesCount());
		if (fs->Disk->diskCmnt != NULL)
			printf("%s", fs->Disk->diskCmnt);

	}

	
	printf("File system features: ");
	vector<string> vFt;
	if (feat & CFileSystem::FSFT_ZXSPECTRUM_FILES)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_ZXSPECTRUM_FILES)]);
	if (feat & CFileSystem::FSFT_LABEL)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_LABEL)]);
	if (feat & CFileSystem::FSFT_FILE_ATTRIBUTES)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_FILE_ATTRIBUTES)]);
	if (feat & CFileSystem::FSFT_FOLDERS)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_FOLDERS)]);
	if (feat & CFileSystem::FSFT_TIMESTAMPS)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_TIMESTAMPS)]);
	if (feat & CFileSystem::FSFT_CASE_SENSITIVE_FILENAMES)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_CASE_SENSITIVE_FILENAMES)]);
	for (byte ftIdx = 0; ftIdx < vFt.size(); ftIdx++)
		printf("%s, ", vFt[ftIdx].c_str());		
	
	printf("\n");

	return true;
}

bool CopyDisk(int argc, char* argv[])
{	
	if (theFS->GetFSFeatures() && CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
		CDiskBase* src = fs->Disk;
		char* dstName = argv[0];
		if ((void*)src == NULL || (void*)dstName == NULL || strlen((char*)dstName) == 0)
			return false;

		CDiskBase* dst;
		strupr((char*)dstName);

		if (strstr((char*)dstName, "A:"))	
			dst = new CDiskWin32(((CDiskBase*)src)->DiskDefinition);			
		else if (strstr((char*)dstName, ".DSK"))
			dst = new CEDSK((CEDSK&)*(CDiskBase*)src);		
		else
			dst = new CDiskImgRaw(((CDiskBase*)src)->DiskDefinition);	

		dst->Open((char*)dstName, CDiskBase::OPEN_MODE_CREATE);
		bool res = ((CDiskBase*)src)->CopyTo(dst, false);		
		delete dst;
		
		return res;
	}	
	else	
		return false;
}

bool PutFile(int argc, char* argv[])
{
	char* name = (char*)argv[0];
	char* nameDest = name;
	char* folder = "0";
	byte argIdx = 1;	

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	bool HasFolders = (theFS->GetFSFeatures() & CFileSystem::FSFT_FOLDERS) > 0;
		
	bool res = true;		
	
	if (res)
	{
		while (argIdx < argc)
		{
			if (strcmp(argv[argIdx], "-n") == 0 && argc >= argIdx)
			{				
				nameDest = argv[argIdx + 1];
				argIdx++;
			}

			if (HasFolders && strcmp(argv[argIdx], "-d") == 0 && argc >= argIdx)
			{
				folder = argv[argIdx + 1];
				argIdx++;
			}

			argIdx++;
		}	

		CFile* f = theFS->NewFile(nameDest);
		theFS->SetFileFolder(f, folder);									
		//res = f->SetFileName(nameDest);		
		
		//char name2[80];
		//f->GetFileName(name2);
		dword fsz = fsize(name);	
		FILE* fpc = fopen(name, "rb");
		if (res && fpc != NULL && f != NULL)
		{
			byte* buf = new byte[fsz];				
			fread(buf, 1, fsz, fpc);
			fclose(fpc);
			f->SetData(buf, fsz);

			if (IsBasic)
			{
				CFileSpectrum* s = dynamic_cast<CFileSpectrum*>(f);

				s->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
				s->SpectrumStart = 0xFFFF;
				s->SpectrumVarLength = s->SpectrumArrayVarName = 0;
				s->SpectrumLength = (word)fsz;				
				
				argIdx = 0;
				while (argIdx < argc)
				{					
					if (strcmp(argv[argIdx], "-s") == 0)
					{
						s->SpectrumStart = atoi(argv[argIdx+1]);
						argIdx++;
					}
					else if (strcmp(argv[argIdx], "-t") == 0)
					{
						string stStr = argv[argIdx+1];
						CFileSpectrum::SpectrumFileType st = CFileSpectrum::SPECTRUM_UNTYPED;

						if (stStr == "p")
						{
							st = CFileSpectrum::SPECTRUM_PROGRAM;
							s->SpectrumVarLength = Basic::BasicDecoder::GetVarSize(buf, (word)fsz);							
						}
						else if (stStr == "b")
							st = CFileSpectrum::SPECTRUM_BYTES;
						else if (stStr == "c")
							st = CFileSpectrum::SPECTRUM_CHAR_ARRAY;
						else if (stStr == "n")
							st = CFileSpectrum::SPECTRUM_NUMBER_ARRAY;
						s->SpectrumType = st;
						argIdx++;
					}

					argIdx++;
				}				
			}
			
			res = theFS->WriteFile(f);
			delete buf;
		}	
		else		
			res = false;					

		delete f;
	}

	return res;
}

bool DeleteFiles(int argc, char* argv[])
{
	char* fspec = (char*)argv[0];
	byte fCnt = 0;
	CFile* f = theFS->FindFirst(fspec);
	while (f != NULL)
	{
		fCnt++;
		f = theFS->FindNext();
	}

	char msg[32];
	sprintf(msg, "Delete %d files?", fCnt);
	if (Confirm(msg))
		return theFS->Delete(fspec);	
	else
		return false;
}

bool ExecSysCmd(int argc, char* argv[])
{
	string s((char*)argv[0]);	
	for (byte argIdx = 1; argIdx < argc; argIdx++)
	{
		s += " ";
		s += (char*)argv[argIdx];
	}

	return system(s.c_str()) == 0;
}

typedef bool (*CommandDelegate)(int argc, char* argv[]);		

bool RenameFile(int argc, char* argv[])
{
	bool res = argc == 2;

	CFile* f = theFS->FindFirst(argv[0]);
	res = res && f != NULL;

	if (res)
		res = theFS->Rename(f, argv[1]);

	return res;
}

struct Command
{
	char* cmd[3];
	char* desc;

	struct CommandParam 
	{
		char* param;
		bool mandatory;
		char* desc;
	} Params[4];			

	CommandDelegate cmdDlg;
};

bool PrintHelp(int argc, char* argv[]);

void Tap2Wav(CTapFile* theTap, bool realTime = true)
{				
	CTape2Wav tape2wav;
	CTZXFile* tzx = (CTZXFile*)theTap;
	CTZXFile::TZXBlkArhiveInfo* blkArh;
	CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;
	dword bufIdx;

	if (theTap != NULL)
	{	
		if (realTime)
			printf("Playing the tape. Press ESC to quit, SPACE to pause. ");
		printf("Block count: %d.\n",  theTap->GetBlockCount());
		
		CTapeBlock* tb = new CTapeBlock();
		char Name[CTapeBlock::TAP_FILENAME_LEN + 1];	
		stringstream wavName;		
		if (!realTime)
		{
			wavName << theTap->fileName << ".wav";
			tape2wav.Open((char*)wavName.str().c_str());
		}		

		if (theTap->GetFirstBlock(tb))
			do
			{			
				printf("%02d: ", theTap->m_CurBlkIdx);					

				if (theTap->m_CurBlkStts != CTapFile::BLK_RD_STS_VALID)
				{
					printf("%s block, ID 0x%X, skipping.\n",
						CTapFile::TapeBlkReadStatusNames[theTap->m_CurBlkStts], (byte)tzx->m_CurrBlkID);						
				}
				else 
				{					
					if (tb->m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
					{				
						if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PAUSE)
						{							
							if (tzx->m_CurrBlk.pauseLen > 0)
								tape2wav.PutSilence(tzx->m_CurrBlk.pauseLen);
							else
							{								
								if (!Confirm("'Stop the tape' block encountered. Continue?"))
									break;
							}							
							tape2wav.SetPulseLevel(false);
						}
						else
						{
							if (tb->m_BlkType == CTapeBlock::TAPE_BLK_STD)
								printf("%sStd. Block\t", tzx->m_bInGroup ? "\t" : "");
							else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
								printf("%sTrb. Block\t", tzx->m_bInGroup ? "\t" : "");
							else
								printf("%sRaw data\t", tzx->m_bInGroup ? "\t" : "");

							if ((tb->m_BlkType == CTapeBlock::TAPE_BLK_STD || tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO) &&
								tb->IsBlockHeader())
							{
								tb->GetName(Name);
								printf("%-7s: %s\t%d\t%02X\n", 
									tb->TapBlockTypeNames[((CTapeBlock::TapeBlockHeaderBasic*)tb->Data)->BlockType], 
									Name, tb->Length, tb->Flag);
							}
							else
								printf("Data   :\t\t%d\t%02X\n", tb->Length, tb->Flag);

							//play along
							if (realTime)
							{		
								wavName.str("");
								wavName << theTap->fileName << theTap->m_CurBlkIdx << ".wav";
								tape2wav.Open((char*)wavName.str().c_str());
								tape2wav.AddBlock(tb);
								tape2wav.Close();
								dword lenMs = tape2wav.GetWrittenLenMS();
								dword timeIdx = 0;

								PlaySound((wavName.str().c_str()), NULL, SND_FILENAME | SND_ASYNC);

								while (!GetAsyncKeyState(VK_ESCAPE) && !GetAsyncKeyState(VK_SPACE) && timeIdx < lenMs)
								{
									Sleep(500);
									timeIdx += 500;
									printf("\rProgress: %d %%\t", (int)(((float)timeIdx/lenMs)*100));
								}

								if (timeIdx < lenMs)
								{
									char c = getch();
									if (c == 27)
									{
										PlaySound(NULL, NULL, 0);
										printf("Canceled\n");
										break;
									}
									else if (c == ' ')
									{
										printf("Paused. Press a key to continue.\n");
										getch();
									}
								}							

								remove(wavName.str().c_str());
							}							
							else
								tape2wav.AddBlock(tb);

						}						
					}					
					else
					{
						char msg[256];

						switch (tzx->m_CurrBlkID)
						{
						case CTZXFile::BLK_ID_TXT_DESCR:
							strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
							msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
							printf("Message: '%s'\n", msg);
							break;
						case CTZXFile::BLK_ID_GRP_STRT:
							strncpy(msg, tzx->m_CurrBlk.blkGrpStrt.GrpName, tzx->m_CurrBlk.blkGrpStrt.Len);
							msg[tzx->m_CurrBlk.blkGrpStrt.Len] = 0;
							printf("Group: \"%s\"\n", msg);							
							break;
						case CTZXFile::BLK_ID_GRP_END:
							printf("Group end.\n");
							break;
						case CTZXFile::BLK_ID_JMP:
							printf("Jump to block: %d.\n", tzx->m_CurBlkIdx + tzx->m_CurrBlk.jmpType);							
							//if (Confirm(" Make jump?"))
							tzx->Jump(tzx->m_CurrBlk.jmpType);														
							break;						
						case CTZXFile::BLK_ID_ARH_INF:
							blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb->Data;
							bufIdx = sizeof(word) + sizeof(byte);
							blkTxt = &blkArh->TxtItem;

							printf("Arhive Info:\n");
							for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
							{													
								char* line;
								byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ? 
									CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

								bufIdx += sizeof(byte)*2;								

								printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
								line = msg;
								strncpy(msg, (char*)&tb->Data[bufIdx], blkTxt->TxtLen);
								msg[blkTxt->TxtLen] = 0;

								if ((line = strtok(msg, "\r")) != NULL)
								{
									do 
									{
										printf("%s%s\n", line == msg ? "" : "\t\t\t", line);									
									} while(line = strtok(NULL, "\r"));									
								}

								bufIdx += blkTxt->TxtLen;																															
								blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb->Data[bufIdx];
							}
							break;

						case CTZXFile::BLK_ID_STOP_48K:
							if (Confirm("'Stop tape if 48K' encountered. Stop?"))
								goto ExitLoop;
							break;

						case CTZXFile::BLK_ID_LOOP_STRT:
							printf("Loop start: %d repeats.\n", tzx->m_LoopRptCnt);
							tzx->m_BlkLoopIdx = 0;
							break;

						case CTZXFile::BLK_ID_LOOP_END:
							printf("Loop end.\n");
							if (++tzx->m_BlkLoopIdx < tzx->m_LoopRptCnt)
							{
								tzx->Seek((word)tzx->m_BlkLoopStrt);															
								tzx->m_CurBlkIdx--;
							}
							break;							

						case CTZXFile::BLK_ID_CALL_SEQ:
							printf("Call sequence, %d calls.\n", tzx->m_CallSeq->CallCnt);														
							tzx->Jump(tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);
							break;

						case CTZXFile::BLK_ID_RET_SEQ:
							printf("Ret. from call to block ");
							if (tzx->m_CallSeqIdx < tzx->m_CallSeq->CallCnt)
							{
								printf("%d.\n", tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);
								tzx->Jump(tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);								
							}
							else
							{
								printf("%d.\n", tzx->m_CallSeqRetIdx);
								tzx->Seek(tzx->m_CallSeqRetIdx);
								tzx->m_CurBlkIdx--;
								delete [] tzx->m_CallSeq;
							}
							break;

						default:
							printf("Glue block.\n");
						}
					}
				}

				delete tb->Data;
				tb->Data = NULL;

			}
			while (theTap->GetNextBlock(tb));
ExitLoop:
			delete tb;			

			if (!realTime)				
			{
				//theTap->Close();
				tape2wav.Close();
				dword sec = tape2wav.GetWrittenLenMS()/1000;
				printf("The length is: %02d:%02d.\n", sec/60, sec%60);
			}
	}		
}

bool PlayTape(int argc, char* argv[])
{
	if (theFS != NULL)
	{
		bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
		if (!IsDisk)
		{
			bool playToWav = argc > 0 && stricmp((char*)argv[0], "-w") == 0;
			Tap2Wav(((CFileArchiveTape*)theFS)->theTap, !playToWav);
		}
	}	

	return theFS != NULL;
}

bool Export2Tape(int argc, char* argv[])
{
	if (theFS != NULL)
	{
		//bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
		bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
		if (IsBasic)
		{		
			string fname = argv[0];
			transform(fname.begin(), fname.end(), fname.begin(), (int (*)(int))::toupper);
			string ext = GetExtension(fname);			
			if (ext != "TAP")
				fname += ".TAP";
			CFileArchiveTape tape((char*)fname.c_str());
			if (tape.Open((char*)fname.c_str(), true))
			{
				char* fspec = "*";
				if (argc >= 2)
					fspec = (char*)argv[1];
				
				CFile* f = theFS->FindFirst(fspec);				
				while (f != NULL && theFS->OpenFile(f) && theFS->ReadFile(f))
				{											
					CFileSpectrumTape* fst = new CFileSpectrumTape(*dynamic_cast<CFileSpectrum*>(f), 
						*dynamic_cast<CFile*>(f));
					tape.AddFile(fst);
					f = theFS->FindNext();
					delete fst;
				}				

				tape.Close();
			}
		}
		else
		{
			cout << "Cannot export a non-Spectrum file collection to tape." << endl;			
			return false;
		}
	}

	return true;
}

bool ImportTape(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	if (IsDisk && IsBasic)
	{
		CFileArchiveTape tape(argv[0]);
		if (tape.Open(argv[0], false) && tape.Init())
		{
			char* pattern = "*";
			if (argc >= 2)
				pattern = argv[1];

			CFile* fSrc = tape.FindFirst(pattern);
			while (fSrc != NULL)
			{
				CFileSystem::FileNameType fn;				
				fSrc->GetFileName(fn);
				CFile* fDest = theFS->NewFile(fn);

				if (fDest != NULL)
				{
					*dynamic_cast<CFileSpectrum*>(fDest) = *dynamic_cast<CFileSpectrum*>(fSrc);

					dword len = fSrc->GetLength();
					byte* buf = new byte[len];
					fSrc->GetData(buf);
					fDest->SetData(buf, len);

					theFS->WriteFile(fDest);
					delete buf;

					fSrc = tape.FindNext();
				}				
				else
					return false;
			}
		}
		else
			return false;
	}
	else
	{
		cout << "Can only import tape to a Spectrum disk." << endl;
		return false;
	}

	return true;
}

bool SaveBoot(int argc, char* argv[])
{
	bool res = true;

	if (argc == 0)
		res = false;
	
	char* fName = argv[0];

	if (res && theFS != NULL && theFS->FSFeatures & CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = (CFileSystem*)theFS;
		byte bootTrkCnt = fs->FSParams.BootTrackCount;
		res = bootTrkCnt > 0;

		if (res)
		{
			word trkSz = fs->Disk->DiskDefinition.SPT * fs->Disk->DiskDefinition.SectSize;
			byte* trkBuf = new byte[trkSz * bootTrkCnt];
			for (byte trkIdx = 0; trkIdx < bootTrkCnt && res; trkIdx++)
				res = fs->Disk->ReadTrack(trkBuf + trkIdx * trkSz, trkIdx/fs->Disk->DiskDefinition.SideCnt, trkIdx % fs->Disk->DiskDefinition.SideCnt);

			if (res)
			{
				FILE* fout = fopen(fName, "wb");
				fwrite(trkBuf, trkSz, bootTrkCnt, fout);
				fclose(fout);
			}

			delete trkBuf;			
		}
		else
		{
			printf("Numarul de piste pentru boot este 0.\n");
			res = false;
		}
	}
	else
		res = false;

	return res;
}

bool LoadBoot(int argc, char* argv[])
{
	bool res = true;

	if (argc == 0)
		res = true;

	char* fName = argv[0];

	if (res && theFS != NULL && theFS->FSFeatures & CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = (CFileSystem*)theFS;
		byte bootTrkCnt = fs->FSParams.BootTrackCount;
		res = bootTrkCnt > 0;

		word trkSz = fs->Disk->DiskDefinition.SPT * fs->Disk->DiskDefinition.SectSize;
		byte* trkBuf = new byte[trkSz * bootTrkCnt];

		FILE* fout = NULL;
		if (res)
		{
			fout = fopen(fName, "rb");
			res = (fout != NULL);			
		}		
		
		if (res)
		{
			res = fread(trkBuf, trkSz, bootTrkCnt, fout) > 0;
			fclose(fout);			
		}
		else
			printf("Couldn't open file %s.", fName);

		if (res)
		{			
			for (byte trkIdx = 0; trkIdx < bootTrkCnt && res; trkIdx++)
				res = fs->Disk->WriteTrack(trkIdx/fs->Disk->DiskDefinition.SideCnt, trkIdx % fs->Disk->DiskDefinition.SideCnt, trkBuf + trkIdx * trkSz);			
		}
		else
		{
			printf("Boot track count is 0.\n");
			res = false;
		}

		delete trkBuf;			
	}
	else
		res = false;

	return res;
}

bool FormatDisk(int argc, char* argv[])
{
	bool res = true;

	if (argc == 0)
		res = false;

	char* path = argv[0];
	strupr(path);

	byte selGeom = 0xFF;
	if (argc >= 3 && strcmp(argv[1], "-t") == 0)
		selGeom = atoi(argv[2])-1;

	CDiskBase::DiskDescType dd;

	if (selGeom == 0xFF)
	{
		vector<byte> geometries;
		for (byte idx = 0; idx < sizeof(diskTypes)/sizeof(diskTypes[0]); idx++)			
			geometries.push_back(idx);
		selGeom = AskGeometry(geometries);
		res = (selGeom > 0 && selGeom != 0xFF);
	}	
	
	CDiskBase* disk;
	if (res)
	{
		dd = diskTypes[selGeom].diskDef;

		if (strstr((char*)path, "A:"))	
			disk = new CDiskWin32(dd);			
		else if (strstr((char*)path, ".DSK"))
			disk = new CEDSK(dd);		
		else
			disk = new CDiskImgRaw(dd);	
	}	

	if (res)
		res = (disk != NULL);

	if (res)
		res = disk->Open((char*)path, CDiskBase::OPEN_MODE_CREATE);

	CFileSystem* fs = NULL;

	if (res)		
	{
		FS diskDesc = diskTypes[selGeom];		

		if (diskDesc.fsType == FS_CPM_GENERIC)		
			fs = new CFSCPM(disk, diskDesc.fsParams, *(CFSCPM::CPMParams*)&diskDesc.otherParams, diskDesc.Name);								
		else if (diskDesc.fsType == FS_CPM_HC_BASIC)		
			fs = new CFSCPMHC(disk, diskDesc.fsParams, *(CFSCPM::CPMParams*)&diskDesc.otherParams, diskDesc.Name);								
		else if (diskDesc.fsType == FS_CPM_PLUS3_BASIC)
			fs = new CFSCPMPlus3(disk, diskDesc.fsParams, *(CFSCPM::CPMParams*)&diskDesc.otherParams, diskDesc.Name);
		else if (diskDesc.fsType == FS_COBRA_DEVIL)
			fs = new CFSCobraDEVIL(disk, diskDesc.Name);			
		else if (diskDesc.fsType == FS_TRDOS)
			fs = new CFSTRDOS(disk, diskDesc.Name);
		else if (diskDesc.fsType == FS_OPUS_DISCOVERY)
			fs = new CFSOpus(disk, diskDesc.Name);
		else if (diskDesc.fsType == FS_MGT_PLUSD)
			fs = new CFSMGT(disk, diskDesc.Name);
		else
			res = false;
	}

	if (res)
		res = fs->Format();	

	if (res)
		delete fs;

	return res;
}

bool ChangeAttributes(int argc, char* argv[])
{
	bool res = argc == 2;		
	char* faStr = argv[1];		
	CFileArchiveTape::FileAttributes faSet = CFileSystem::ATTR_HIDDEN, faClear = CFileSystem::ATTR_HIDDEN;

	while (res && strlen(faStr) > 0)
	{
		if (*faStr != '+' && *faStr != '-')
		{
			res = false;
			break;
		}

		bool toSet = *faStr == '+';
		faStr++;
		switch (*faStr)
		{
		case 'a':
			if (toSet)
				faSet = (CFileSystem::FileAttributes)((byte)faSet | (byte)CFileSystem::ATTR_ARCHIVE);
			else
				faClear = (CFileSystem::FileAttributes)((byte)faClear | (byte)CFileSystem::ATTR_ARCHIVE);
			break;
		case 'r':
			if (toSet)
				faSet = (CFileSystem::FileAttributes)((byte)faSet | (byte)CFileSystem::ATTR_READ_ONLY);
			else
				faClear = (CFileSystem::FileAttributes)((byte)faClear | (byte)CFileSystem::ATTR_READ_ONLY);
			break;
		case 's':
			if (toSet)
				faSet = (CFileSystem::FileAttributes)((byte)faSet | (byte)CFileSystem::ATTR_SYSTEM);
			else
				faClear = (CFileSystem::FileAttributes)((byte)faClear | (byte)CFileSystem::ATTR_SYSTEM);
			break;
		default:
			res = false;			
		}
		faStr++;
	}

	if (res)
	{		
		res = theFS->SetAttributes(argv[0], faSet, faClear);
	}
	
	return res;
}


static const Command theCommands[] = 
{
	{{"help", "?"}, "Command list, this message", {{}}, 
		PrintHelp},
	{{"fsinfo"}, "Display the known file systems", {{}}, 
		ShowKnownFS},
	{{"stat"}, "Display the current file system parameters", {{}}, Stat},
	{{"open"}, "Open disk or disk image", 
		{{"drive|image", true, "The disk/image to open"},
		{"-t", false, "The number of file system type to use"}}, 
		Open},
	{{"close"}, "Close disk or disk image", 
		{{}}, 
		Close},
	{{"ls", "dir"}, "List directory", 
		{{"<folder><\\>file spec.", false, "filespec: *.com or 1\\*, etc"}, 
		{"-sn|-ss|-st", false, "Sort by name|size|type"},		
		{"-ne", false, "Don't show extended info, faster"}}, 
		Cat},	
	{{"get"}, "Copy file(s) to PC", 
		{{"[\"]filespec[\"]", true, "* or *.com or readme.txt, \"1 2\", etc"}, 
		{"-t", false, "Copy as text"}}, 
		GetFile},	
	{{"type", "cat"}, "Display file", 
		{{"file spec.", true, "* or *.com or readme.txt, etc"}, 
		{"-h|-t", false, "display as hex or as text"}}, 
		TypeFile},	
	{{"copydisk"}, "Copy current disk to another disk or image", 
		{{"destination", true, "destination disk/image"}}, 
		CopyDisk},
	{{"put"}, "Copy PC file to file system", 
		{{"source file", true, "the file to copy"}, 
		{"-n newname", false, "name for destination file"},
		{"-d <destination folder>", false, "file folder"},
		{"-s start, -t p|b|c|n file type", false, "Spectrum file attributes"}}, 
		PutFile},
	{{"del", "rm"}, "Delete file(s)", 
		{{"file spec.", true, "the file(s) to delete"}}, 
		DeleteFiles},			
	{{"ren"}, "Rename file", 
		{{"file name", true, "the file to rename"}, {"new name", true, "new file name"}}, 
		RenameFile},			
	{{"!"}, "Execute DOS command after '!'", 
		{"DOS command", true, "! dir, ! mkdir, etc"}, 
		ExecSysCmd},
	{{"tapplay"}, "Play the tape into a wav file", 
		{{"-w", false, "play to a .wav file instead of realtime"}}, 
		PlayTape},
	{{"tapexp"}, "Exports the files to a tape image", 
		{{".tap name", true, "the TAP file name"}, 
		{"file mask", false, "the file name mask"}}, 
		Export2Tape},
	{{"tapimp"}, "Imports the TAP file to disk", 
		{{".tap name", true, "the TAP file name"}, 
		{"file mask", false, "the file name mask"}}, 
		ImportTape},
	{{"saveboot"}, "Save boot tracks to file", {"file name", true, "output file"}, SaveBoot},
	{{"loadboot"}, "Load boot tracks from file", {"file name", true, "input file"}, LoadBoot},
	{{"format"}, "Format disk", 
		{{"disk/image", true, "disk/image"}, 
		{"-t", false, "Format number to pre-select (from fsinfo command)"}},
		FormatDisk},
	{{"attrib", "chattr"}, "Change file(s) attributes", 
		{{"file spec.", true, "file(s) to update"}, 
		{"+/-ars", true, "set/remove attribute(s) (a)rhive, (r)eadonly, (s)ystem"}}, 
		ChangeAttributes},
	{{"exit", "quit"}, "Exit program", 
	{{}}},
};


bool PrintHelp(int argc, char* argv[])
{		
	printf("This program is designed to manipulate older file systems on a modern PC,\nespecially, but not limited to, the ones related to Sinclair Spectrum computers\n");
	for (byte cmdIdx = 0; cmdIdx < (sizeof(theCommands)/sizeof(theCommands[0])); cmdIdx++)
	{		
		for (byte cmdAlias = 0; cmdAlias < (sizeof(theCommands[0].cmd)/sizeof(theCommands[0].cmd[0])); cmdAlias++)
			if (theCommands[cmdIdx].cmd[cmdAlias] != NULL)
				printf("%s ", theCommands[cmdIdx].cmd[cmdAlias]);
		printf(" - %s\n", theCommands[cmdIdx].desc);
		
		for (byte paramIdx = 0; paramIdx < sizeof(theCommands[0].Params)/sizeof(theCommands[0].Params[0]); paramIdx++)
			if (theCommands[cmdIdx].Params[paramIdx].param != NULL)
			{							
				if (theCommands[cmdIdx].Params[paramIdx].mandatory)
					printf("- <%s>: %s\n",
					theCommands[cmdIdx].Params[paramIdx].param, theCommands[cmdIdx].Params[paramIdx].desc);
				else
					printf("- [%s]: %s\n",
					theCommands[cmdIdx].Params[paramIdx].param, theCommands[cmdIdx].Params[paramIdx].desc);
			}		
	}

	return true;
}


bool ExecCommand(char* cmd, char params[10][MAX_PATH])
{
	bool foundCmd = false;
	bool exitReq = false;	
	char* pParams[10] = {params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]};	

	for (byte cmdIdx = 0; cmdIdx < sizeof(theCommands)/sizeof(theCommands[0]) && !foundCmd; cmdIdx++)
	{
		for (byte cmdAlias = 0; cmdAlias < (sizeof(theCommands[0].cmd)/sizeof(theCommands[0].cmd[0])) && !foundCmd; cmdAlias++)
		{
			foundCmd = theCommands[cmdIdx].cmd[cmdAlias] != NULL && stricmp(cmd, theCommands[cmdIdx].cmd[cmdAlias]) == 0;
			if (foundCmd)
			{
				exitReq = stricmp(theCommands[cmdIdx].cmd[0], "exit") == 0;
				if (!exitReq && theCommands[cmdIdx].cmdDlg != NULL)
				{
					bool paramMissing = false;

					for (byte prmIdx = 0; prmIdx < sizeof(theCommands[cmdIdx].Params)/sizeof(theCommands[cmdIdx].Params[0]); prmIdx++)
						if (theCommands[cmdIdx].Params[prmIdx].param != NULL && theCommands[cmdIdx].Params[prmIdx].mandatory &&								
							((void*)params[prmIdx] == NULL || strlen(params[prmIdx]) == 0))							
						{
							paramMissing = true;
							printf("The command '%s' requires a value for the parameter '%s'.\n", 
								theCommands[cmdIdx].cmd[0], theCommands[cmdIdx].Params[0]);
						}

						if (!paramMissing)
						{
							byte pCnt = 0;
							for (byte pIdx = 0; pIdx < 10; pIdx++)
								if (pParams[pIdx] != NULL && strlen(pParams[pIdx]) > 0)
									pCnt++;
							if (!theCommands[cmdIdx].cmdDlg(pCnt, pParams))
							{																		
								printf("Failed.");
								if (theFS != NULL)
								{
									char errMsg[80];
									CFileSystem::ErrorType err = theFS->GetLastError(errMsg);
									if (err != CFileSystem::ERR_NONE && strlen(errMsg) > 0)
										printf(" Error message: '%s'.\n", errMsg);
									else if (err != CFileSystem::ERR_NONE)
										printf(" Error code: '%d'.\n", err);
									else
										printf("\n");
								}
								else
									printf("\n");
							}								
						}
				}						
			}
		}
	}

	if (!foundCmd)
		printf("Command not understood.\n");

	return !exitReq;
}

int main(int argc, char * argv[])
{		
#ifdef _MSC_VER	
	#ifdef _DEBUG
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );	
		#define new new( _NORMAL_BLOCK , __FILE__ , __LINE__ )	
	#endif
#endif

	char params[10][MAX_PATH];	
	char cmd[32] = "";	
	char bufc[128];	
	memset(bufc, 0, sizeof(bufc));
	bool exitReq = false;

	printf("HCDisk 2.0, (C) George Chirtoaca, compiled on %s %s.\nType ? for help.\n", __DATE__, __TIME__);	
		
	const char* sep = " ";
	//Parse commands at command line.
	byte argIdx = 1, argIdxCmd = 0;
	if (argc > 1)
	{		
		while (argIdx < argc)
		{	
			strcpy(cmd, argv[argIdx]);
			memset(params, 0, sizeof(params));
			argIdx++;

			while (argIdx < argc && strcmp(argv[argIdx], ":") != 0)	
			{
				strcpy(params[argIdxCmd], argv[argIdx]);			
				argIdx++;
				argIdxCmd++;
			}

			if (!ExecCommand(cmd, params))
				exit(0);			
			argIdx++;
			argIdxCmd = 0;
		}		
	}

	//Parse interactive commands.
	bool cancelReq = false;
	do
	{
		//Read command.
		do
		{
			printf("%s>", theFS != NULL ? theFS->Name : "");
			gets(bufc);
		} while (strlen(bufc) == 0);

		char* word = strtok(bufc, sep);

		//Parse command line.
		while (word != NULL)
		{
			strcpy(cmd, word);
			memset(params, 0, sizeof(params));
			byte prmIdx = 0;

			//Parse parameters.
			word = strtok(NULL, sep);
			bool isCmdSep = (word != NULL && strcmp(word, ":") == 0);
			while (prmIdx < 10 && word != NULL && !isCmdSep)
			{			
				if (word[0] == '"')
				{		
					//Reset first space.
					word[strlen(word)] = ' ';
					char* prmEnd = strchr(&word[1], '"');
					if (prmEnd != NULL)
						strncpy(params[prmIdx], &word[1], prmEnd - &word[1]);
					prmIdx++;
				}
				else			
					strcpy(params[prmIdx++], word);			

				word = strtok(NULL, sep);
			}

			cancelReq = !ExecCommand(cmd, params);				

			if (isCmdSep)
				word = strtok(NULL, sep);
		}		
	} while (!cancelReq);			
	
	int lastErr = theFS != NULL ? theFS->GetLastError() : 0;
	Close(0, NULL);

	return lastErr;
}

