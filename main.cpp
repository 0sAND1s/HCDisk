//////////////////////////////////////////////////////////////////////////
//HCDisk2, (C) George Chirtoaca 2014
//////////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include <conio.h>

#include <list>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <exception>

#include <Windows.h>
#include <WinCon.h>


#include "dsk.h"
#include "edsk.h"
#include "cfscpm.h"
#include "DiskImgRaw.h"
#include "DiskWin32LowLevel.h"
#include "DiskWin32Native.h"
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
char path[MAX_PATH];

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
	"PHYSICAL DISK",
	"RAW - DISK IMAGE",
	"DSK - CPCEMU DISK IMAGE",
	"EDSK - CPCEMU DISK IMAGE",
	"TRD - TR-DOS DISK IMAGE",
	"SCL - TR-DOS DISK IMAGE",
	"CQM - Sydex COPYQM DISK IMAGE",
	"OPD - OPUS Discovery DISK IMAGE",
	"MGT - Miles Gordon Tech DISK IMAGE",
	"TAP - TAPE IMAGE",
	"TZX - TAPE IMAGE",
	"TD0 - Sydex Teledisk DISK IMAGE"
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


//Loader synthax for storage devices. Using '?' to signal the place of file/block name. Using NOT PI instead of 0, to avoid having to embed the 5 byte number value and it's shorter too.
typedef enum
{
	LDR_TAPE,
	LDR_MICRODRIVE,
	LDR_OPUS,
	LDR_HC_DISK,
	LDR_HC_COM,
	LDR_SPECTRUM_P3_DISK,
	LDR_MGT
} StorageLoaderType;

//Tape:			LOAD "NAME"
const char LDR_TAPE_STR[] = {'"', '?', '"', 0};
//Microdrive:	LOAD *"m";0;"NAME"
const char LDR_MICRODRIVE_STR[] = { '*', '"', 'm', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//Opus:			LOAD *"m";0;"NAME"
const char LDR_OPUS_STR[] = { '*', '"', 'm', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//HC disk:		LOAD *"d";0;"NAME"
const char LDR_HC_DISK_STR[] = { '*', '"', 'd', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//HC COM:		LOAD *"b";"NAME"
const char LDR_HC_COM_STR[] = { '*', '"', 'b', '"', ';', '"', '?', '"', 0 };
//Spectrum +3:	LOAD "A:NAME"
const char LDR_SPECTRUM_P3_DISK_STR[] = { '"', 'A', ':', '?', '"', 0 };
//MGT +D:		LOAD d1"NAME"
const char LDR_MGT_STR[] = { 'd', '1', '"', '?', '"', 0 };

const char* STORAGE_LOADER_EXPR[] =
{
	LDR_TAPE_STR,
	LDR_MICRODRIVE_STR,
	LDR_OPUS_STR,
	LDR_HC_DISK_STR,
	LDR_HC_COM_STR,
	LDR_SPECTRUM_P3_DISK_STR,
	LDR_MGT_STR
};

const char* LDR_TAG[] = { "TAPE", "MICRODRIVE", "OPUS", "HCDISK", "HCCOM", "PLUS3", "MGT"};

class FileSystemDescription 
{
public:
	char* Name;
	FSType fsType;	
	CDiskBase::DiskDescType diskDef;		
	CFileSystem::FSParamsType fsParams;
	word otherParams[5];
};

const FileSystemDescription DISK_TYPES[] =
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
		"GENERIC CP/M 2.2 5.25\"", FS_CPM_GENERIC,
		{40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 175, 64, 2}, 
		{2, 1}
	},

	{
		"GENERIC CP/M 2.2 3.5\"", FS_CPM_GENERIC,
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

bool ConvertBASICLoaderForDevice(CFileArchive* src, CFileArchiveTape* dst, StorageLoaderType ldrType);

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
	for (byte fsIdx = 0; fsIdx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); fsIdx++)
	{
		printf("%2d. %-20s\t|%dx%dx%dx%d\t", fsIdx+1, DISK_TYPES[fsIdx].Name, 			
			DISK_TYPES[fsIdx].diskDef.TrackCnt, DISK_TYPES[fsIdx].diskDef.SideCnt, 
			DISK_TYPES[fsIdx].diskDef.SPT, DISK_TYPES[fsIdx].diskDef.SectSize);
		printf("|%d\t|%d\t|%d\t|%d\n", DISK_TYPES[fsIdx].fsParams.BlockSize, DISK_TYPES[fsIdx].fsParams.BlockCount, 
			DISK_TYPES[fsIdx].fsParams.DirEntryCount, DISK_TYPES[fsIdx].fsParams.BootTrackCount);
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
	bool IsTape = (theFS->GetFSFeatures() & CFileSystem::FSFT_TAPE) > 0;

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
	bool includeDeleted = false;

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
		else if (a == "-del")
			includeDeleted = true;
		else
			wildCard = a;
	}	
	if (sortType != ST_NONE)
		bExtendedCat = true;
			
	CFileSystem::FileNameType fName = "";	
	char fExt[4] = "";	


	//For tzx files, display comments and archive info.
	if (IsTape)			
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
	CFile* f = theFS->FindFirst((char*)wildCard.c_str(), includeDeleted);
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

list<string> GetLoadedBlocksInProgram(byte* buf, word progLen, word varLen = 0)
{
	Basic::BasicDecoder BasDec;
	word bufPos = 0;	
	Basic::BasicLine bl;
	list<string> blocksLoaded;

	do
	{
		bl = BasDec.GetBasicLineFromLineBuffer(buf + bufPos, progLen - varLen - bufPos);
		//Some loaders have extra data after the BASIC program that is not valid BASIC nor variables.
		if (bl.lineSize > 0)
		{			
			auto loadsInLine = BasDec.GetLoadingBlocks(bl.lineBufBasic.data(), (word)bl.lineBufBasic.size());
			for (auto load : loadsInLine)
				blocksLoaded.push_back(load.second.second);
		}

		bufPos += bl.lineSize;
	} while (bl.lineSize > 0 && bufPos < progLen - varLen);

	return blocksLoaded;
}

string DecodeBASICProgramToText(byte* buf, word progLen, word varLen = 0)
{
	stringstream res;
	Basic::BasicDecoder BasDec;
	Basic::BasicLine bl;
	word bufPos = 0;	
	list<string> blocksLoaded;

	do
	{
		bl = BasDec.GetBasicLineFromLineBuffer(buf + bufPos, progLen - varLen - bufPos);
		//Some loaders have extra data after the BASIC program that is not valid BASIC nor variables.
		if (bl.lineSize > 0)
		{
			BasDec.DecodeBasicLineToText(bl, res);

			auto loadsInLine = BasDec.GetLoadingBlocks(bl.lineBufBasic.data(), (word)bl.lineBufBasic.size());
			for (auto load : loadsInLine)
				blocksLoaded.push_back(load.second.second);
		}

		bufPos += bl.lineSize;
	} while (bl.lineSize > 0 && bufPos < progLen - varLen);
	
	if (varLen > 0)	
		BasDec.DecodeVariables(&buf[progLen-varLen], varLen, res);

	//Display block names for blocks loaded by this BASIC block.	
	if (blocksLoaded.size() > 0)
		res << "Loading blocks: ";
	auto blIt = blocksLoaded.begin();	
	while (blIt != blocksLoaded.end())
	{
		res << "\"" << *blIt << "\" ";
		blIt++;
	}
	res << endl;
	
	
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
		d->flags |= (DISFLAG_SINGLE | DISFLAG_ADDRDUMP | DISFLAG_OPCODEDUMP | DISFLAG_UPPER | DISFLAG_ANYREF | DISFLAG_RELCOMMENT | DISFLAG_VERBOSE);

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
	bool asDisasm = argc >= 2 && (strcmp(argv[1], "-d") == 0);
	char* fname = argv[0];
	CFile* f = theFS->FindFirst(fname);		
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;				

	if (f != NULL && theFS->OpenFile(f) && theFS->ReadFile(f))		
	{		
		CFileSystem::FileNameType fn;
		f->GetFileName(fn);		
		CFileArchive::TrimEnd(fn);
		dword len = f->GetLength();
		const dword fsize = theFS->GetFileSize(f, true);
		byte* buf1 = new byte[fsize];		
		
		if (!asHex && !asDisasm && IsBasic)
		{
			bool isProgram = false, isSCR = false, isBytes = false;		
			CFileSpectrum* fileSpectrum = dynamic_cast<CFileSpectrum*>(f);				
			isProgram = fileSpectrum->GetType() == CFileSpectrum::SPECTRUM_PROGRAM;
			isBytes = fileSpectrum->GetType() == CFileSpectrum::SPECTRUM_BYTES;
			isSCR = isBytes && fileSpectrum->SpectrumLength == 6912;			

			if (isProgram)
			{
				f->GetData(buf1);
				TextViewer(DecodeBASICProgramToText(buf1, (word)f->GetLength(), fileSpectrum->SpectrumVarLength));												
			}
			else if (isSCR)
			{
				f->GetData(buf1);				
				char* fntmp = _tempnam(NULL, fn);				
				char gifName[MAX_PATH], gifNameQuoted[MAX_PATH];
				sprintf(gifName, "%s.%s", fntmp, "gif");				
				free(fntmp);
				ConvertSCR2GIF(buf1, gifName);			
				sprintf(gifNameQuoted, "\"%s\"", gifName);
				system(gifNameQuoted);				
				remove(gifName);
			}
			else if (isBytes)
			{							
				asDisasm = true;				
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
		else if (asDisasm)
		{			
			f->GetData(buf1);
			word addr = 0;
			if (IsBasic)
			{
				CFileSpectrum* s = dynamic_cast<CFileSpectrum*>(f);
				addr = s->SpectrumStart;
			}
			TextViewer(Disassemble(buf1, (word)len, addr));			
		}
		else if (asText)
		{
			byte wrap = 80;

			//Adjust buffer length for line wrap.
			dword lenToSet = len + (len / wrap) * 2;
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
	
	for (byte fsIdx = 0; fsIdx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); fsIdx++)
	{
		CDiskBase::DiskDescType dd = DISK_TYPES[fsIdx].diskDef;
		if (dd.TrackCnt == x.TrackCnt && dd.SideCnt == x.SideCnt && dd.SPT == x.SPT && dd.SectSize == x.SectSize)
			res.push_back(fsIdx);
	}

	return res;
}

//Will return a list of indexes for matching geometries by raw disk image size.
vector<byte> GetMatchingGeometriesByImgSize(dword imgSize)
{
	vector<byte> res;

	for (byte fsIdx = 0; fsIdx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); fsIdx++)
	{
		CDiskBase::DiskDescType dd = DISK_TYPES[fsIdx].diskDef;
		if (//diskTypes[fsIdx].fsType != FS_TRDOS && diskTypes[fsIdx].fsType != FS_TRDOS_SCL &&
			(dd.TrackCnt * dd.SideCnt * dd.SPT * dd.SectSize) == imgSize)
			res.push_back(fsIdx);
	}

	return res;
}

vector<byte> GetMatchingGeometriesByType(FSType fsType)
{
	vector<byte> res;

	for (byte fsIdx = 0; fsIdx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); fsIdx++)
	{
		if (DISK_TYPES[fsIdx].fsType == fsType)
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

void PrintLastErrMsg()
{
	char buf[MAX_PATH];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	printf("Windows reports an error: %s", buf);
}

CDiskBase* InitDisk(char* path, CDiskBase::DiskDescType* dd = NULL)
{
	CDiskBase* disk = NULL;

	//Windows assigned floppy drives to volume A: or B:, including the USB floppy drives.
	//USB floppy drive letter cannot be changed in Disk Management, since it's not detected as a disk drive, but a volume only.
	//In case a controller-connected floppy drive exists on drive A:, the USB unit is assigned letter B:.	
	if ((stricmp((char*)path, "A:") == 0 || stricmp((char*)path, "B:") == 0))
	{				
		//Verify if the specified path points to a floppy drive, not hard disk.
		if (CDiskWin32Native::IsFloppyDrive(path))
		{			
			//Verify if the specified path is a USB floppy drive, or the low level driver is not installed, use the default Windows drivers then.
			if (CDiskWin32Native::IsUSBVolume(path) || CDiskWin32LowLevel::GetDriverVersion() == 0)
			{
				puts("Using the Windows native drivers for accessing the floppy drive.");

				if (dd != NULL)
					disk = new CDiskWin32Native(*dd);
				else
					disk = new CDiskWin32Native();
			}
			//If not an USB floppy drive, then it must be a floppy controller drive. Use the low level driver for it, if installed.
			else if (CDiskWin32LowLevel::GetDriverVersion() > 0)
			{
				puts("Using the FDRAWCMD.sys low level driver for accessing the floppy drive.");

				if (dd != NULL)
					disk = new CDiskWin32LowLevel(*dd);
				else
					disk = new CDiskWin32LowLevel();
			}			
		}			
		else
		{
			//Drives A: or B: can be assigned to other disks.
			printf("Drive %s is not a floppy drive.\n", path);
		}		
	}
	else if (strstr((char*)path, ".DSK"))
	{		
		if (dd != NULL)
			disk = new CEDSK(*dd);
		else
			disk = new CEDSK();
	}
	else
	{
		disk = new CDiskImgRaw(*dd);
	}


	return disk;
}


CDiskBase* OpenDisk(char* path, StorageType& srcType, vector<byte>& foundGeom)
{	
	srcType = STOR_NONE;
	word fsIdx = 0;	
	FileSystemDescription theDiskDesc;
	theFS = NULL;

	string fileExt = GetExtension(path);

	if (stricmp(path, "A:") == 0 || stricmp(path, "B:") == 0)
	{
		puts("Auto detecting disk geometry and file system...");
		byte tracks = 0, heads = 0, sectCnt = 0;		
		CDiskBase* diskWin32 = InitDisk(path, NULL);

		if (diskWin32 != nullptr)
		{
			if (diskWin32->Open(path, CDiskBase::OPEN_MODE_EXISTING) && 
				diskWin32->DetectDiskGeometry(diskWin32->DiskDefinition))
			{									
				fsIdx = 0;				
				foundGeom = GetMatchingGeometriesByGeometry(diskWin32->DiskDefinition);
			}
			else
			{
				delete diskWin32;
				diskWin32 = NULL;
				PrintLastErrMsg();
			}

			//delete diskWin32;			
			theDisk = diskWin32;
		}		

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
			theDiskDesc = DISK_TYPES[foundGeom[trdGeomIdx]];

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
			theDiskDesc = DISK_TYPES[foundGeom[opdGeomIdx]];

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
		FileSystemDescription theDiskDesc = DISK_TYPES[*it];				

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
			}
			else
				it++;								
		}				
		else
			it++;
		
		if (theDisk != NULL)
		{
			delete theDisk;
			theDisk = NULL;
		}

		if (theFS != NULL)
		{
			delete theFS;
			theFS = NULL;
		}
	}
}

byte AskGeometry(vector<byte> foundGeom)
{
	bool bFoundSel = false, bCancel = false;
	byte fsIdx = 0xFF;

	for (byte fsMatchIdx = 0; fsMatchIdx < foundGeom.size(); fsMatchIdx++)				
		printf("%d. %s\n", fsMatchIdx+1, DISK_TYPES[foundGeom[fsMatchIdx]].Name);

	do
	{
		printf("Select FS (empty for cancel): ");																					
		char buf[10];
		if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n' && (fsIdx = atoi(buf)) > 0)
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
	strcpy(path, argv[0]);	
	//CDiskBase* theDisk = NULL;
	FileSystemDescription theDiskDesc;
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
		printf("The specified source '%s' doesn't hold a recognized disk/file system.\n", path);			
		//theFS = NULL;
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
				if (fsIdx >= 0 && fsIdx != 0xFF)
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
			theDiskDesc = DISK_TYPES[fsIdx];
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
				//if (theDisk != NULL)
				//	delete theDisk;
				if (theDisk != NULL)
					theDisk->DiskDefinition = theDiskDesc.diskDef;
				else
					isOpen = false;
				
				//theDisk = new CDiskWin32Native(theDiskDesc.diskDef);
				//isOpen = ((CDiskWin32Native*)theDisk)->Open(path);
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

word log2(CFileSystem::FileSystemFeature y)
{
	word log2 = 0;
	unsigned long x = (unsigned long)y;

	while ((x = x >> 1) > 0)
		log2++;

	return log2;
}

bool Stat(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	word feat = theFS->GetFSFeatures();

	printf("Storage\t\t\t: %s\n", StorageTypeNames[storType]);
	printf("File system\t\t: %s\n", theFS->Name);
	printf("Path\t\t\t: %s\n", path);

	if (feat & CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
		printf("Disk geometry\t\t: %dT x %dH x %dS x %dB/S\n",
			fs->Disk->DiskDefinition.TrackCnt, fs->Disk->DiskDefinition.SideCnt,
			fs->Disk->DiskDefinition.SPT, fs->Disk->DiskDefinition.SectSize);
		printf("Raw Size\t\t: %d KB\n", (fs->Disk->DiskDefinition.TrackCnt * fs->Disk->DiskDefinition.SideCnt * fs->Disk->DiskDefinition.SPT * fs->Disk->DiskDefinition.SectSize)/1024);
		printf("Block size\t\t: %.02f KB\n", (float)fs->GetBlockSize()/1024);
		printf("Blocks free/max\t\t: %d/%d \n", fs->GetFreeBlockCount(), fs->GetMaxBlockCount());
		printf("Disk free/max KB\t: %d/%d = %02.2f%% free\n", fs->GetDiskLeftSpace() / 1024, fs->GetDiskMaxSpace() / 1024, 
			((float)fs->GetDiskLeftSpace() / fs->GetDiskMaxSpace()) * 100);
		printf("Catalog free/max\t: %d/%d = %02.2f%% free\n", fs->GetFreeDirEntriesCount(), fs->GetMaxDirEntriesCount(), 
			((float)fs->GetFreeDirEntriesCount()/fs->GetMaxDirEntriesCount())*100);
		if (fs->Disk->diskCmnt != NULL)
			printf("%s", fs->Disk->diskCmnt);
	}

	
	printf("File system features\t: ");
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
	bool format = argc >= 2 && stricmp(argv[1], "-f") == 0;
	if (theFS != nullptr/* && Confirm("This copy operation will overwrite data on destination drive/image, if it exists already. Are you sure?")*/)
	{		
		if (theFS->GetFSFeatures() && CFileSystem::FSFT_DISK)
		{
			CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
			CDiskBase* src = fs->Disk;
			char* dstName = argv[0];
			if ((void*)src == NULL || (void*)dstName == NULL || strlen((char*)dstName) == 0)
				return false;

			strupr((char*)dstName);

			CDiskBase* dst = InitDisk(dstName, &src->DiskDefinition);		
			bool res = dst->Open((char*)dstName, CDiskBase::OPEN_MODE_CREATE);

			if (res)
			{
				if (!format)
				{
					//Make sure that the read disk has the same format as intended. If not, it must be formatted first.
					bool sameGeometry = true;
					CDiskBase::DiskDescType dd;
					sameGeometry = dst->DetectDiskGeometry(dd) &&
						dd.TrackCnt == src->DiskDefinition.TrackCnt &&
						dd.SideCnt == src->DiskDefinition.SideCnt &&
						dd.SPT == src->DiskDefinition.SPT &&
						dd.SectSize == src->DiskDefinition.SectSize;

					if (!sameGeometry)
					{
						res = false;
						puts("The destination disk has a different format than the source disk. Format it first.");
					}
					else
						res = res && ((CDiskBase*)src)->CopyTo(dst, format);
				}
				else
					res = res && ((CDiskBase*)src)->CopyTo(dst, format);
			}										

			delete dst;

			return res;
		}
		else
			return false;
	}
	else
		return false;
}

vector<string> ListFilesInPath(string path)
{
	auto files = vector<string>();
	HANDLE dir;
	WIN32_FIND_DATA file_data;

	dir = FindFirstFile(path.c_str(), &file_data);
	while (dir != INVALID_HANDLE_VALUE)
	{
		if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			files.push_back(file_data.cFileName);

		FindNextFile(dir, &file_data);
	}

	return files;
}

void PutFile2(int argc, char* argv[])
{
	char* path = (char*)argv[0];
	auto fileNames = ListFilesInPath(path);

	for(auto filePath: fileNames)
	{
		//Strip path from file name.
		string nameDest(filePath);
		size_t lastSlash = nameDest.find_last_of('\\');
		if (lastSlash != string::npos)
			nameDest = nameDest.substr(lastSlash + 1, CFSCPM::MAX_FILE_NAME_LEN + 1);
	}	
}

bool PutFile(int argc, char* argv[])
{
	char* name = (char*)argv[0];	
	
	//Strip path from file name.
	string nameDest(name);
	size_t lastSlash = nameDest.find_last_of('\\');
	if (lastSlash != string::npos)
		nameDest = nameDest.substr(lastSlash + 1, CFSCPM::MAX_FILE_NAME_LEN + 1);

	
	char* folder = "0";
	byte argIdx = 1;	

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	bool HasFolders = (theFS->GetFSFeatures() & CFileSystem::FSFT_FOLDERS) > 0;
		
	bool res = true;		
		
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

	CFile* f = theFS->NewFile((char*)nameDest.c_str());
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

	if (fCnt > 0)
	{
		char msg[32];
		sprintf(msg, "Delete %d files?", fCnt);
		//if (Confirm(msg))
			return theFS->Delete(fspec);
		//else
			//return false;
	}
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
	CTZXFile* tzx = theTap->IsTZX() ? (CTZXFile*)theTap : nullptr;
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

		bool blockRead = theTap->GetFirstBlock(tb);		
		while (blockRead)
		{			
			printf("%02d: ", theTap->m_CurBlkIdx);					

			if (theTap->m_CurBlkStts != CTapFile::BLK_RD_STS_VALID)
			{
				if (tzx != nullptr)
					printf("%s block, ID 0x%X, skipping.\n",
						CTapFile::TapeBlkReadStatusNames[theTap->m_CurBlkStts], (byte)tzx->m_CurrBlkID);						
			}
			else 
			{					
				if (tb->m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
				{				
					if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PAUSE)
					{							
						if (tzx != nullptr && tzx->m_CurrBlk.pauseLen > 0)
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
							printf("%sStd. Block\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
							printf("%sTrb. Block\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else
							printf("%sRaw data\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");

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
								printf("\rProgress: %3d %%, %3d/%3d seconds\t", (int)(((float)timeIdx/lenMs)*100), timeIdx/1000, lenMs/1000);
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
				else if (tzx != nullptr)
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
						printf("Loop end %d.\n", tzx->m_BlkLoopIdx);
						if (++tzx->m_BlkLoopIdx < tzx->m_LoopRptCnt)
						{
							tzx->Seek((word)tzx->m_BlkLoopStrt);																						
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

			delete [] tb->Data;
			tb->Data = NULL;

			blockRead = theTap->GetNextBlock(tb);
		};
ExitLoop:
		delete tb;			

		if (!realTime)				
		{
			//theTap->Close();
			tape2wav.Close();
			dword sec = tape2wav.GetWrittenLenMS()/1000;
			printf("Wrote %s with length is: %02d:%02d.\n", wavName.str().c_str(), sec/60, sec%60);
		}
	}		
}

bool PlayTape(int argc, char* argv[])
{
	bool IsTape = false;
	if (theFS != NULL)
	{
		IsTape = (theFS->GetFSFeatures() & CFileSystem::FSFT_TAPE) > 0;
		if (IsTape)
		{
			bool playToWav = argc > 0 && stricmp((char*)argv[0], "-w") == 0;
			if (!playToWav && !((CFileArchiveTape*)theFS)->HasStandardBlocksOnly())
			{
				playToWav = true;
				puts("The tape image has non-standard blocks, playing to wave file instead of real time.");
			}
			Tap2Wav(((CFileArchiveTape*)theFS)->theTap, !playToWav);
		}
		else
		{
			puts("The current container is not TAP/TZX.");
		}
	}	

	return theFS != NULL && IsTape;
}

bool ConvertBASICLoader(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;
	
	StorageLoaderType ldrType = LDR_TAPE;
	char* ldrTypeStr = argv[1];
	for (int ldrIdx = 0; ldrIdx < sizeof(STORAGE_LOADER_EXPR) / sizeof(STORAGE_LOADER_EXPR[0]); ldrIdx++)
	{
		if (strcmp(ldrTypeStr, LDR_TAG[ldrIdx]) == 0)
		{
			ldrType = (StorageLoaderType)ldrIdx;
			break;
		}
	}	
		
	bool IsBasicTape = (theFS->GetFSFeatures() & (CFileSystem::FSFT_ZXSPECTRUM_FILES | CFileSystem::FSFT_TAPE)) == 
		(CFileSystem::FSFT_ZXSPECTRUM_FILES | CFileSystem::FSFT_TAPE);
	if (!IsBasicTape)
	{
		puts("The source must be a tape file. Export from disk to tape, convert tape loader, then import the new tape to disk.");
		return false;
	}
	
	string fname = argv[0];
	transform(fname.begin(), fname.end(), fname.begin(), (int (*)(int))::toupper);
	string ext = GetExtension(fname);
	if (ext != "TAP")
		fname += ".TAP";

	CFileArchiveTape tapeDst((char*)fname.c_str());
	tapeDst.Open((char*)fname.c_str(), true);		
	bool res = ConvertBASICLoaderForDevice(theFS, &tapeDst, ldrType);
	tapeDst.Close();		
	

	return res;
}


bool Export2Tape(int argc, char* argv[])
{
	if (theFS != NULL)
	{
		//bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
		bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
		if (IsBasic)
		{					
			char* tmpName = "temp.tap";
			string outName = argv[0];
			transform(outName.begin(), outName.end(), outName.begin(), (int (*)(int))::toupper);
			string ext = GetExtension(outName);			
			if (ext != "TAP")
				outName += ".TAP";
			CFileArchiveTape tapeDest(tmpName);
			if (tapeDest.Open(tmpName, true))
			{
				char* fspec = "*";
				if (argc >= 2)
					fspec = (char*)argv[1];

				auto exportedBlocks = list<string>();				
				CFile* f = theFS->FindFirst(fspec);
				while (f != NULL && theFS->OpenFile(f) && theFS->ReadFile(f))
				{
					CFileSpectrumTape* fst = new CFileSpectrumTape(*dynamic_cast<CFileSpectrum*>(f),
						*dynamic_cast<CFile*>(f));

					CFileArchive::FileNameType fn;
					fst->GetFileName(fn);					
					if (find(exportedBlocks.begin(), exportedBlocks.end(), fn) == exportedBlocks.end())
						exportedBlocks.push_back(fn);					

					//Also add blocks loaded by a basic block if not already exported by name.
					if (fst->GetType() == CFileSpectrum::SPECTRUM_PROGRAM)
					{						
						byte* buf = new byte[fst->GetLength()];
						fst->GetData(buf);
												
						auto loadedBlocks = GetLoadedBlocksInProgram(buf, fst->GetLength(), fst->SpectrumVarLength);	
						for (auto loadedName : loadedBlocks)
						{
							if (find(exportedBlocks.begin(), exportedBlocks.end(), loadedName) == exportedBlocks.end())
							{
								//Trim loaded block name.
								CFileArchive::FileNameType fn;
								strcpy(fn, loadedName.c_str());
								CFileArchive::TrimEnd(fn);
								exportedBlocks.push_back(fn);
							}
						}
												
						delete[] buf;												
					}
										

					f = theFS->FindNext();
					delete fst;
				}

				for (auto name : exportedBlocks)
				{
					CFile* f = theFS->FindFirst((char *)name.c_str());
					if (f != NULL)
					{
						theFS->OpenFile(f);
						theFS->ReadFile(f);
						CFileSpectrumTape* fst = new CFileSpectrumTape(*dynamic_cast<CFileSpectrum*>(f),
							*dynamic_cast<CFile*>(f));
						tapeDest.AddFile(fst);
						delete fst;
					}					
				}				
				tapeDest.Close();

								
				//Update the BASIC loader to match tape LOAD synthax.								
				CFileArchiveTape tapeDest2((char*)outName.c_str());
				tapeDest2.Open((char*)outName.c_str(), true);
				
				tapeDest.Open(tmpName);				
				tapeDest.Init();

				ConvertBASICLoaderForDevice(&tapeDest, &tapeDest2, LDR_TAPE);

				tapeDest.Close();
				tapeDest2.Close();
				remove(tmpName);				
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
	bool IsHCDisk = (strcmp(theFS->Name, "HC BASIC 3.5\"") == 0) || (strcmp(theFS->Name, "HC BASIC 5.25\"") == 0);	
	char* tapSrcName = argv[0];
	char* tapTempName = nullptr;

	//Conversion for HC BASIC loaders
	if (IsHCDisk)
	{
		tapTempName = "temp.tap";
		CFileArchiveTape tapeSrc(tapSrcName);
		CFileArchiveTape tapeHC(tapTempName);
		if (tapeSrc.Open(tapSrcName, false) && tapeSrc.Init() &&
			tapeHC.Open(tapTempName, true))
		{			
			ConvertBASICLoaderForDevice(&tapeSrc, &tapeHC, LDR_HC_DISK);
			tapeSrc.Close();
			tapeHC.Close();

			tapSrcName = tapTempName;
		}		
	}

	bool writeOK = true;
	if (IsDisk && IsBasic)
	{		
		CFileArchiveTape tapeSrc(tapSrcName);
		if (tapeSrc.Open(tapSrcName, false) && tapeSrc.Init())
		{
			char* pattern = "*";
			if (argc >= 2)
				pattern = argv[1];

			CFile* fSrc = tapeSrc.FindFirst(pattern);			
			
			while (fSrc != NULL && writeOK)
			{			
				CFileSystem::FileNameType fn;				
				fSrc->GetFileName(fn);
				CFile* fDest = theFS->NewFile(fn);
				
				if (fDest != NULL)
				{
					*dynamic_cast<CFileSpectrum*>(fDest) = *dynamic_cast<CFileSpectrumTape*>(fSrc);
				
					dword len = fSrc->GetLength();
					byte* buf = new byte[len];										
					writeOK = fSrc->GetData(buf) && fDest->SetData(buf, len) && theFS->WriteFile(fDest);
					delete [] buf;										
				}
				else
				{
					writeOK = false;
				}
			
				delete fDest;				
				fSrc = tapeSrc.FindNext();
			}

						
			tapeSrc.Close();

			if (tapTempName != nullptr)
				remove(tapTempName);
		}
		else
			return false;
	}
	else
	{
		cout << "Can only import tape to a Spectrum disk." << endl;
		return false;
	}

	return writeOK;
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
		for (byte idx = 0; idx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); idx++)			
			geometries.push_back(idx);
		selGeom = AskGeometry(geometries);
		res = (selGeom > 0 && selGeom != 0xFF);
	}	
	
	CDiskBase* disk;
	if (res)
	{
		dd = DISK_TYPES[selGeom].diskDef;

		disk = InitDisk(path, &dd);
	}	

	if (res)
		res = (disk != NULL);

	if (res)
		res = disk->Open((char*)path, CDiskBase::OPEN_MODE_CREATE);

	CFileSystem* fs = NULL;

	if (res)		
	{
		FileSystemDescription diskDesc = DISK_TYPES[selGeom];		

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

//Convers a Z80 binary into a BASIC block with a REM line.
//Before executing the Z80 binary, it moves it to the specified address.
bool Bin2REM(int argc, char* argv[])
{
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	if (!IsBasic)
	{
		puts("The file system doesn't hold BASIC files.");
		return false;
	}

	char *blobName = nullptr, *setName = nullptr;
	word addr = 0;

	if (argc >= 1)
		blobName = argv[0];
	if (argc >= 2)
		addr = atoi(argv[1]);
	if (argc >= 3)
		setName = argv[2];
	
	if (setName == nullptr)
		setName = blobName;

	long blobSize = fsize(blobName);
	if (blobSize > 48 * 1024)
	{
		puts("Blob size is too big.");
		return false;
	}

	FILE* fBlob = fopen(blobName, "rb");
	if (fBlob == NULL)
	{
		printf("Could not open blob file %s.", blobName);
		return false;
	}
	
	byte basLDR[] = { 
		Basic::BASICKeywordsIDType::BK_RANDOMIZE, 
		Basic::BASICKeywordsIDType::BK_USR,
		Basic::BASICKeywordsIDType::BK_VAL,
		'"', '3', '1', '+',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '5', '+', '2', '5', '6', '*',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '6',
		'"', ':', Basic::BASICKeywordsIDType::BK_REM,
		0x21, 14, 0,										//ld hl, lenght of ASM loader, the blob follows it
		0x09,												//add hl, bc	;BC holds the address called from RANDOMIZE USR.
		0x11, (byte)(addr%256), (byte)(addr/256),			//ld de, start addr
		0xD5,												//push de		;Jump to start of blob. 
		0x01, (byte)(blobSize%256), (byte)(blobSize/256),	//ld bc, blob size
		0xED, 0xB0,											//ldir
		0xC9												//ret
	};
					
	//Create blob with BASIC loader + actual blob.
	byte* blobBuf = new byte[blobSize + sizeof(basLDR)];
	memcpy(blobBuf, basLDR, sizeof(basLDR));
	fread(&blobBuf[sizeof(basLDR)], 1, blobSize, fBlob);
	fclose(fBlob);
			
	//Create BASIC line from buffer.
	const word progLineNo = 0;	
	Basic::BasicLine bl(blobBuf, (word)blobSize + sizeof(basLDR), progLineNo);
	delete [] blobBuf;
				
	//Create header for Spectrum file.
	CFile* f = theFS->NewFile(setName);
	CFileSpectrum* fs = dynamic_cast<CFileSpectrum*>(f);
	fs->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
	fs->SpectrumStart = bl.lineNumber;
	fs->SpectrumVarLength = fs->SpectrumArrayVarName = 0;
	fs->SpectrumLength = bl.lineSize;			
	
	//Write BASIC line to buffer.
	Basic::BasicDecoder bd;
	byte* lastBuf = new byte[bl.lineSize];
	bd.PutBasicLineToLineBuffer(bl, lastBuf);	
	f->SetData(lastBuf, bl.lineSize);
	delete [] lastBuf;

	//Set file type to something that is both CFile and CFileSpectrum, to be accepted on Spectrum disks and tape images.	
	bool res = theFS->WriteFile(f);		
	delete f;	
	return res;	
}


//Automatic BASIC loader conversion for different storage devices (HC disk, HC serial, Spectrum disk, Opus, etc).
//Must first check that the tape is convertible:
//- it has the number of blocks with header (typed files) equal to the number of blocks present in the BASIC loader to be loaded
//- multilevel games have more blocks loaded by machine code (not present in the BASIC loader), so those games cannot be converted
//- if headerless blocks (untyped blocks) are present in the tape, it means machine code routine is loading blocks, so those games cannot be converted.
//If the BASIC loader is addressing blocks using empty strings, we must find the actual block names following the BASIC loader and use those in the loader, as empty names are not allowed on disk, even if are allowed on tapes.
//If the block names contain unprintable chars or are empty strings, we must convert names to printable chars and use those names in the BASIC loader, making sure names are unique, as duplicate names are not allowed on disk.	
bool ConvertBASICLoaderForDevice(CFileArchive* src, CFileArchiveTape* dst, StorageLoaderType ldrType)
{			
	if ((src->GetFSFeatures() & CFileArchive::FSFT_ZXSPECTRUM_FILES) == 0 ||
		(dst->GetFSFeatures() & CFileArchive::FSFT_ZXSPECTRUM_FILES) == 0)
	{
		puts("Can only convert loader for ZX Spectrum files.");
		return false;
	}

	bool hasHeaderless = false;
	word blockCnt = 0;
	Basic::BasicDecoder bd;
	CFileSpectrumTape* fileToCheck = (CFileSpectrumTape*)src->FindFirst("*");
	list<string> loadedBlocks;
	vector<string> actualBlockNames;

	while (fileToCheck != nullptr && !hasHeaderless)
	{
		if (fileToCheck->GetType() == CFileSpectrum::SPECTRUM_UNTYPED)
			hasHeaderless = true;
		else
		{
			if (fileToCheck->GetType() == CFileSpectrum::SPECTRUM_PROGRAM)
			{
				dword fileLen = fileToCheck->GetLength();
				byte* fileData = new byte[fileLen];
				fileToCheck->GetData(fileData);
				loadedBlocks = GetLoadedBlocksInProgram(fileData, fileToCheck->SpectrumLength, fileToCheck->SpectrumVarLength);
				delete[] fileData;				
			}
			else
			{
				CFileArchive::FileNameType fileName;
				fileToCheck->GetFileName(fileName);
				actualBlockNames.push_back(fileName);
			}					

			blockCnt++;
			fileToCheck = (CFileSpectrumTape*)src->FindNext();
		}				
	}

	if (hasHeaderless)
	{
		puts("The tape has headerless (untyped) blocks, it cannot be converted.\n");
		return false;
	}

	if ((word)loadedBlocks.size() < blockCnt - 1)
	{
		printf("The BASIC loader should load %d blocks, but is loading %d blocks.\n", blockCnt-1, loadedBlocks.size());
		return false;
	}
	
	Basic::BasicLine blSrc, blDst;	
	CFile* srcFile = src->FindFirst("*");
	while (srcFile != nullptr)
	{		
		CFileSpectrumTape* srcFileSpec = (CFileSpectrumTape*)srcFile;
		if (srcFileSpec->GetType() == CFileSpectrum::SPECTRUM_PROGRAM)
		{	
			word bufPosIn = 0, bufPosOut = 0;
			dword srcLen = srcFile->GetLength();
			byte* bufSrc = new byte[srcLen];
			byte* bufDst = new byte[srcLen + 100];

			srcFile->GetData(bufSrc);
			
			byte nameIdx = 0;
			do
			{
				blSrc = bd.GetBasicLineFromLineBuffer(bufSrc + bufPosIn, srcFileSpec->SpectrumLength - srcFileSpec->SpectrumVarLength - bufPosIn);
				if (blSrc.lineSize == 0)
					break;				
								
				//Some BASIC loaders have more LOAD commands than the actual blocks.
				if (nameIdx < actualBlockNames.size())
				{
					bd.ConvertLoader(&blSrc, &blDst, STORAGE_LOADER_EXPR[ldrType], &actualBlockNames, &nameIdx);
					bd.PutBasicLineToLineBuffer(blDst, bufDst + bufPosOut);

					bufPosIn += blSrc.lineSize;
					bufPosOut += blDst.lineSize;
				}
				else
				{
					memcpy(bufDst + bufPosOut, bufSrc + bufPosIn, blSrc.lineSize);
					bufPosIn += blSrc.lineSize;
					bufPosOut += blSrc.lineSize;
				}				
			} while (bufPosIn < srcFileSpec->SpectrumLength);

			CFileArchive::FileNameType fileName;
			srcFileSpec->GetFileName(fileName);
			CFile* file = new CFile(fileName, bufPosOut, bufDst);
			CFileSpectrumTape* dstFile = new CFileSpectrumTape(*srcFileSpec, *file);
			dstFile->SpectrumLength = bufPosOut;
			dst->AddFile(dstFile);

#ifdef _DEBUG
			TextViewer(DecodeBASICProgramToText(bufSrc, bufPosIn));
			TextViewer(DecodeBASICProgramToText(bufDst, bufPosOut));
#endif

			delete file;	
			delete dstFile;

			if (bufSrc != nullptr)
				delete bufSrc;
			if (bufSrc != nullptr)
				delete bufDst;

		}		
		else //Copy rest of blocks that are not program.
		{			
			dst->AddFile(srcFileSpec);		
		}
		
		srcFile = src->FindNext();
	}

	return true;	
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
		{"-ne", false, "Don't show extended info, faster"}, 
		{"-del", false, "List deleted files"}},
		Cat},	
	{{"get"}, "Copy file(s) to PC", 
		{{"[\"]filespec[\"]", true, "* or *.com or readme.txt, \"1 2\", etc"}, 
		{"-t", false, "Copy as text"}}, 
		GetFile},	
	{{"type", "cat"}, "Display file", 
		{{"file spec.", true, "* or *.com or readme.txt, etc"}, 
		{"-h|-t|-d", false, "display as hex|text|asm"}}, 
		TypeFile},	
	{{"copydisk"}, "Copy current disk to another disk or image", 
		{{"destination", true, "destination disk/image"},
		 {"-f", false, "format destination while copying"}}, 
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
	{{"bin2rem"}, "Transform binary to BASIC block",
		{{"file", true, "blob to add"},
		 {"address of execution", true, "address to copy the block to before execution"},
		 {"name of block", false, "name of BASIC block"}
		},
		Bin2REM},
	{{"convldr"}, "Converts a BASIC loader to work with another storage device",
		{{".tap name", true, "destination TAP file name"},
		{"loader type", true, "type of loader: TAPE, MICRODRIVE, OPUS, HCDISK, HCCOM, PLUS3, MGT"}
		},
		ConvertBASICLoader},
	{{"exit", "quit"}, "Exit program", 
	{{}}},	
};


bool PrintHelp(int argc, char* argv[])
{		
	printf("This program is designed to manipulate ZX Spectrum computer related file systems on a modern PC.\n");
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
								theCommands[cmdIdx].cmd[0], theCommands[cmdIdx].Params[prmIdx].param);
						}

						if (!paramMissing)
						{
							byte pCnt = 0;
							for (byte pIdx = 0; pIdx < 10; pIdx++)
								if (pParams[pIdx] != NULL && strlen(pParams[pIdx]) > 0)
									pCnt++;
							if (!theCommands[cmdIdx].cmdDlg(pCnt, pParams))
							{																		
								printf("Operation failed.\n");
								/*
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
								*/
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
	char cmd[MAX_PATH] = "";
	char bufc[128];	
	memset(bufc, 0, sizeof(bufc));
	bool exitReq = false;

	//setlocale(LC_ALL, "ro-RO");
	printf("HCDisk 2.0 by George Chirtoaca, compiled on %s %s.\nType ? for help.\n", __DATE__, __TIME__);	

	try
	{
		const char* sep = " \n";
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
				fgets(bufc, MAX_PATH, stdin);
			} while (bufc[0] == '\n');

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
	catch (std::exception ex)
	{
		cout << ex.what();

		return -1;
	}			
}

