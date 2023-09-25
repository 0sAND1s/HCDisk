//////////////////////////////////////////////////////////////////////////
//HCDisk2, (C) George Chirtoaca 2014 - 2022
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
#include "CFSHCBasic.h"
#include "CFSCPMPlus3.h"
#include "CFSTRDSCL.h"
#include "CFSCobraDEVIL.h"
#include "CFSOpus.h"
#include "CFSMGT.h"
#include "FileConverters/BasicDecoder.h"
#include "FileConverters/scr2gif.c"
#include "FileConverters/bas2tap.h"
#include "FileConverters/bas2tap.c"
#include "FileConverters/dz80/dissz80.h"
#include "FileConverters/Screen.h"
#include "CFileArchive.h"
#include "CFileArchiveTape.h"
#include "Tape\Tape2Wave.h"
#include "COMIF1.h"

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
	"A:/B: - PHYSICAL DISK (RW)",
	"RAW - DISK IMAGE (RW)",
	"DSK - CPCEMU DISK IMAGE (RW)",
	"EDSK - CPCEMU DISK IMAGE (RW)",
	"TRD - TR-DOS DISK IMAGE",
	"SCL - TR-DOS DISK IMAGE",
	"CQM - Sydex COPYQM DISK IMAGE",
	"OPD - OPUS Discovery DISK IMAGE",
	"MGT - Miles Gordon Tech DISK IMAGE",
	"TAP - TAPE IMAGE (RW)",
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

string GetNameWithoutExtension(string fileName)
{
	int dotIdx = fileName.find_last_of('.');
	string res = "";
	if (dotIdx != string::npos)
		res = fileName.substr(0, dotIdx);	
	return res;	
}


bool Confirm(char* msg)
{
	printf("%s Press [ENTER] to confirm, [ESC] to cancel.\n", msg);
	char c = getch();
	while (c != 27 && c != 13)
		c = getch();

	return c == 13;
}

bool ProgressCallbackRead(word item, word totalItems, bool haveError)
{
	if (!haveError)
		printf("\rReading track %d/%d.%c", item, totalItems, (item == totalItems ? '\n' : ' '));
	else
		printf("Error reading track %d/%d.\n", item, totalItems);
	return true;
}

bool ProgressCallbackWrite(word item, word totalItems, bool haveError)
{
	if (!haveError)
		printf("\rReading track %d/%d.%c", item, totalItems, (item == totalItems ? '\n' : ' '));
	else
		printf("Error writing track %d/%d.\n", item, totalItems);
	return true;
}

bool ProgressCallbackFormat(word item, word totalItems, bool haveError)
{
	if (!haveError)
		printf("\rFormatting track %d/%d.%c", item, totalItems, (item == totalItems ? '\n' : ' '));
	else
		printf("Error formatting track %d/%d.\n", item, totalItems);
	return true;
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
	LDR_IF1_COM,
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
//IF1 COM:		LOAD *"b";
const char LDR_IF1_COM_STR[] = { '*', '"', 'b', '"', 0 };
//Spectrum +3:	LOAD "A:NAME"
const char LDR_SPECTRUM_P3_DISK_STR[] = { '"', '?', '"', 0 };
//MGT +D:		LOAD d1"NAME"; Microdrive synthax is also supported.
const char LDR_MGT_STR[] = { 'd', '1', '"', '?', '"', 0 };

const char* STORAGE_LOADER_EXPR[] =
{
	LDR_TAPE_STR,
	LDR_MICRODRIVE_STR,
	LDR_OPUS_STR,
	LDR_HC_DISK_STR,
	LDR_IF1_COM_STR,
	LDR_SPECTRUM_P3_DISK_STR,
	LDR_MGT_STR
};

const char* LDR_TAG[] = { "TAPE", "MICRODRIVE", "OPUS", "HCDISK", "IF1COM", "PLUS3", "MGT"};

class FileSystemDescription 
{
public:
	char* Name;
	FSType fsType;	
	CDiskBase::DiskDescType diskDef;		
	CFileSystem::FSParamsType fsParams;
	word otherParams[5];
	bool writeSupport;
};
FileSystemDescription theDiskDesc;

const FileSystemDescription DISK_TYPES[] =
{
	//Name, Type,
	//Geometry, filler byte, gap3, density, hardware interleave factor
	//Block size, block count, catalog capacity, boot track count
	//Extra params. For CPM: extensions in catalog entry, software interleave factor.
	//Some values are informative, or for disk detection. Most FS classes recalculate FS params dynamically based on disk geometry.	
	{ 
		"HC BASIC 5.25\"", FS_CPM_HC_BASIC, 
		{40, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_5_25, 0}, 
		{2048, 160, 128, 0},
		{1, 1},
		true
	},

	{
		"HC BASIC 3.5\"", FS_CPM_HC_BASIC,
		{80, 2, 16, CDiskBase::SECT_SZ_256, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 320, 128, 0}, 
		{1, 1},
		true
	},

	{
		"GENERIC CP/M 2.2 5.25\"", FS_CPM_GENERIC,
		{40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_5_25, 0},
		{2048, 175, 64, 2}, 
		{2, 1},
		true
	},

	{
		"GENERIC CP/M 2.2 3.5\"", FS_CPM_GENERIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 351, 128, 4}, 
		{1, 1},
		true
	},

	{
		"Spectrum +3 BASIC 180K", FS_CPM_PLUS3_BASIC,
		{40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 175, 64, 1}, 
		{1, 0},
		true
	},

	{
		"Spectrum +3 BASIC 203K", FS_CPM_PLUS3_BASIC,
		{42, 1, 10, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 6}, 		
		{1024, 205, 64, 1}, 
		{1, 2},
		true
	},

	{
		"Spectrum +3 BASIC 720K", FS_CPM_PLUS3_BASIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 357, 256, 1}, 
		{1, 0},
		true
	},

	{
		"Spectrum +3 BASIC PCW", FS_CPM_PLUS3_BASIC,
		{40, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 175, 64, 1}, 
		{1, 0},
		true
	},

	{
		"Spectrum +3 CP/M SSDD", FS_CPM_GENERIC,
		{40, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 175, 64, 1}, 
		{1, 0},
		true
	},

	{
		"Spectrum +3 CP/M DSDD", FS_CPM_GENERIC,
		{80, 2, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{2048, 357, 256, 1}, 
		{1, 0},
		true
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
		{ 40, 2, 18, CDiskBase::SECT_SZ_256, 0x00, 0x1B, CDiskBase::DISK_DATARATE_DD_5_25, 0},
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
		//HardSkew = 1.
		"CoBra Devil", FS_COBRA_DEVIL,
		{80, 2, 18, CDiskBase::SECT_SZ_256, 0xE5, 20, CDiskBase::DISK_DATARATE_DD_3_5, 1}, 		
		{9216, 77, 108}
		//{}
	},	
	
	{
		"Electronica CIP-04", FS_CPM_PLUS3_BASIC,
		{80, 1, 9, CDiskBase::SECT_SZ_512, 0xE5, 0x1B, CDiskBase::DISK_DATARATE_DD_3_5, 0}, 		
		{1024, 355, 64, 1}, 
		{1, 0},
		true
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

long fsize(const char* fname)
{
	long fs;

	FILE* f = fopen(fname, "rb");
	if (f != NULL)
	{
		fseek(f, 0, SEEK_END);
		fs = ftell(f);
		fclose(f);
	}
	else
		fs = 0;	

	return fs;	
}

bool ShowKnownFS(int argc, char* argv[])
{
	puts("Idx\tName\t\t\t|Geometry\t|Bl.Sz.\t|Bl.Cnt\t|Dir\t|Boot\t|Writable");
	puts("-----------------------------------------------------------------------------------------");
	for (byte fsIdx = 0; fsIdx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); fsIdx++)
	{
		printf("%2d. %-20s\t|%dx%dx%dx%d\t", fsIdx+1, DISK_TYPES[fsIdx].Name, 			
			DISK_TYPES[fsIdx].diskDef.TrackCnt, DISK_TYPES[fsIdx].diskDef.SideCnt, 
			DISK_TYPES[fsIdx].diskDef.SPT, DISK_TYPES[fsIdx].diskDef.SectSize);
		printf("|%d\t|%d\t|%d\t|%d\t|%s\n", DISK_TYPES[fsIdx].fsParams.BlockSize, DISK_TYPES[fsIdx].fsParams.BlockCount, 
			DISK_TYPES[fsIdx].fsParams.DirEntryCount, DISK_TYPES[fsIdx].fsParams.BootTrackCount, DISK_TYPES[fsIdx].writeSupport ? "yes" : "no");
	}
	printf("Known containers: \n");
	for (byte dsIdx = STOR_NONE + 1; dsIdx < STOR_LAST; dsIdx++)
		printf("- %s\n", StorageTypeNames[dsIdx]);
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
	CTapeBlock tb;
	bool bOK = theTap->GetFirstBlock(&tb);

	while (bOK)
	{				
		if (tb.m_BlkType == CTapeBlock::TAPE_BLK_METADATA)
		{		
			CTZXFile* tzx = (CTZXFile*)theTap;
			char msg[256];
			CTZXFile::TZXBlkArhiveInfo* blkArh;
			CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;

			switch (tzx->m_CurrBlkID)
			{					
			case CTZXFile::BLK_ID_TXT_DESCR:
			case CTZXFile::BLK_ID_MSG:
				printf("Message/Description:\n");
				strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
				msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
				char* line;
				if ((line = strtok(msg, "\r")) != NULL)
				{
					do
					{
						printf("%s\n", line);
					} while ((line = strtok(NULL, "\r")) != nullptr);
				}				
				break;
			case CTZXFile::BLK_ID_ARH_INF:
				blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb.Data;
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
					strncpy(msg, (char*)&tb.Data[bufIdx], blkTxt->TxtLen);
					msg[blkTxt->TxtLen] = 0;

					if ((line = strtok(msg, "\r")) != NULL)
					{
						do 
						{
							printf("%s%s\n", line == msg ? "" : "\t\t\t", line);									
						} while(line = strtok(NULL, "\r"));									
					}

					bufIdx += blkTxt->TxtLen;																															
					blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb.Data[bufIdx];
				}
				
				break;
			}
		}			

		if (tb.Length > 0 && tb.Data != nullptr)
		{
			delete[] tb.Data;
			tb.Data = nullptr;
		}
		bOK = theTap->GetNextBlock(&tb);
	}					
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
		printf("\tType\tStart\tBasLen\tVarLen\n");
	else
		printf("\n");
	printf("--------------------------------------------------------------------------------------\n");	
	
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
				char start[6] = "", len[7] = "", varLen[7] = "";
				CFileSpectrum::SpectrumFileType st = s->GetType();								

				if (st == CFileSpectrum::SPECTRUM_PROGRAM && s->SpectrumStart != (word)-1)
				{
					itoa(s->SpectrumStart, start, 10);
					itoa(s->SpectrumVarLength, varLen, 10);
				}
				else if (st == CFileSpectrum::SPECTRUM_BYTES)
					itoa(s->SpectrumStart, start, 10);
				else if (st == CFileSpectrum::SPECTRUM_CHAR_ARRAY || st == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
					sprintf(start, "%c", s->GetArrVarName());
				
				if (st != CFileSpectrum::SPECTRUM_UNTYPED)
					itoa(s->SpectrumLength, len, 10);
				else
					itoa(theFS->GetFileSize(f, false), len, 10);

				printf("\t%s\t%5s\t%5s\t%5s", 
					CFileSpectrum::SPECTRUM_FILE_TYPE_NAMES[st], start, len, varLen);
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

	char* fileName = argv[0];
	bool asText = false;
	string pcName;

	byte argIdx = 1;
	while (argIdx < argc)
	{
		string arg = argv[argIdx];
		if (arg == "-t")
		{
			asText = true;
		}
		else if (arg == "-n" && argc > argIdx + 1)
		{
			pcName = argv[argIdx + 1];
			argIdx++;
		}

		argIdx++;
	}


	CFile* f = theFS->FindFirst(fileName);
	CFileSystem::FileNameType fn;
	bool res = false;
		
	while (f != NULL)
	{			
		f->GetFileName(fn);
		if (!theFS->OpenFile(f))
			printf("Couldn't open file '%s'!\n", fn);
		else
		{						
			if (pcName.length() == 0)
				pcName = fn;

			FILE* pcFile = fopen(pcName.c_str(), "wb");
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
					delete[] buf1;							
					printf("Wrote file '%s'.\n", pcName.c_str());		
					res = true;
				}				
				else
					printf("Could not read file '%s'.\n", fn);				
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
	bool useHex = false;

	d = (DISZ80 *)malloc(sizeof(DISZ80));	
	if (d != NULL)
	{
		memset(d, 0, sizeof(DISZ80));
		dZ80_SetDefaultOptions(d);		

		d->cpuType = DCPU_Z80;		
		d->flags |= (DISFLAG_SINGLE | DISFLAG_ADDRDUMP | DISFLAG_OPCODEDUMP | DISFLAG_UPPER | DISFLAG_ANYREF | DISFLAG_RELCOMMENT | DISFLAG_VERBOSE);
		if (useHex)
			d->layoutRadix = DRADIX_HEX;

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
			if (useHex)
				s << hex;

			s << setw(4) << setfill('0') << addr << "\t" << setfill(' ') << setw(8) << d->hexDisBuf << "\t" << d->disBuf << "\t" << d->commentBuf << "\r\n";
			
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
		delete[] buf1;
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
		
		if (!asHex && !asDisasm && !asText && IsBasic)
		{
			bool isProgram = false, isSCR = false, isBytes = false;
			CFileSpectrum* fileSpectrum = dynamic_cast<CFileSpectrum*>(f);
			isProgram = fileSpectrum->GetType() == CFileSpectrum::SPECTRUM_PROGRAM;
			isBytes = fileSpectrum->GetType() == CFileSpectrum::SPECTRUM_BYTES;
			isSCR = fileSpectrum->SpectrumLength == 6912;

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
			byte wrap = IsBasic ? 64 : 80;

			//Adjust buffer length for line wrap.
			dword lenToSet = len + (len / wrap) * 2;
			byte* buf2 = new byte[lenToSet];

			dword txtLen = f->GetDataAsText(buf2, wrap);
			if (txtLen > 0)
				buf2[txtLen - 1] = '\0';
			TextViewer((char*)buf2);
			delete buf2;
		}

		delete[] buf1;		
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
			disk = new CDSK(*dd);
		else
			disk = new CDSK();
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

				delete theDisk;
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
	bool isTRDValid = false;

	vector<byte>::iterator it = foundGeom.begin();
	while (it != foundGeom.end())
	{
		FileSystemDescription theDiskDesc = DISK_TYPES[*it];				

		if (theDiskDesc.fsType == FS_TRDOS)
		{			
			theDisk = new CDiskImgRaw(theDiskDesc.diskDef);						
			if (theDisk->Open(path, CDiskBase::OPEN_MODE_EXISTING))				
				theFS = new CFSTRDOS(theDisk, theDiskDesc.Name);				
			
			
			if (theFS != nullptr && !theFS->Init())
			{
				it = foundGeom.erase(it);		
				isTRDValid = false;
			}
			else
			{
				it++;
				isTRDValid = true;
			}
		}				
		else
			it++;		

		//theFS destructor dealocates theDisk.
		if (theFS != nullptr)
		{
			delete theFS;
			theFS = nullptr;
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
	word fsIdx = 0;	
	vector<byte> foundGeom;
	byte selGeom = 0;
	if (argc >= 3 && strcmp(argv[1], "-t") == 0)
		selGeom = atoi(argv[2]);	

	Close(0, NULL);

	strupr(path);	
	theDisk = OpenDisk(path, storType, foundGeom);		
	
	if (storType == STOR_NONE)
	{
		printf("The specified source '%s' doesn't hold a recognized disk/file system.\n", path);			
		theFS = NULL;
	}
	else
	{				
		char* dskFmt = StorageTypeNames[(int)storType];
		
		printf("Container\t: %s. \n", dskFmt);				

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
				if (selGeom <= foundGeom.size())
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
			printf("File system\t: %s.\n", theDiskDesc.Name);
			
			//These must be processed here, after the user choses the geometry/fs, as the geometry is not known for RAW images.
			if (storType == STOR_RAW)
			{				
				//if (theDisk != NULL)
				//	delete theDisk;			

				theDisk = new CDiskImgRaw(theDiskDesc.diskDef);
				isOpen = ((CDiskImgRaw*)theDisk)->Open(path);				
			}			
			else //Override disk geometry with the one specified, in case of > 80 track disks for example.
			{								
				if (theDisk != NULL)
					theDisk->DiskDefinition = theDiskDesc.diskDef;
				else
					isOpen = false;							
			}			

			if (isOpen && theFS == NULL && theDisk != NULL)
			{
				if (theDiskDesc.fsType == FS_CPM_GENERIC)		
					theFS = new CFSCPM(theDisk, theDiskDesc.fsParams, *(CFSCPM::CPMParams*)&theDiskDesc.otherParams, theDiskDesc.Name);								
				else if (theDiskDesc.fsType == FS_CPM_HC_BASIC)		
					theFS = new CFSHCBasic(theDisk, theDiskDesc.fsParams, *(CFSCPM::CPMParams*)&theDiskDesc.otherParams, theDiskDesc.Name);								
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

	return true;
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

	printf("Container\t\t: %s\n", StorageTypeNames[storType]);
	printf("File system\t\t: %s\n", theFS->Name);
	printf("Path\t\t\t: %s\n", path);

	if (feat & CFileSystem::FSFT_DISK)
	{
		CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
		printf("Disk geometry\t\t: %dT x %dH x %dS x %dB/S\n",
			fs->Disk->DiskDefinition.TrackCnt, fs->Disk->DiskDefinition.SideCnt,
			fs->Disk->DiskDefinition.SPT, fs->Disk->DiskDefinition.SectSize);
		printf("Hard/soft interleave\t: %d/%d\n", fs->Disk->DiskDefinition.HWInterleave, theDiskDesc.otherParams[0]);
		printf("Raw Size\t\t: %d KB\n", (fs->Disk->DiskDefinition.TrackCnt * fs->Disk->DiskDefinition.SideCnt * fs->Disk->DiskDefinition.SPT * fs->Disk->DiskDefinition.SectSize)/1024);
		printf("Block size\t\t: %.02f KB\n", (float)fs->GetBlockSize()/1024);
		printf("Blocks free/max\t\t: %d/%d \n", fs->GetFreeBlockCount(), fs->GetMaxBlockCount());
		printf("Disk free/max KB\t: %d/%d = %02.2f%% free\n", fs->GetDiskLeftSpace() / 1024, fs->GetDiskMaxSpace() / 1024, 
			((float)fs->GetDiskLeftSpace() / fs->GetDiskMaxSpace()) * 100);
		printf("Catalog free/max\t: %d/%d = %02.2f%% free\n", fs->GetFreeDirEntriesCount(), fs->GetMaxDirEntriesCount(), 
			((float)fs->GetFreeDirEntriesCount()/fs->GetMaxDirEntriesCount())*100);

		if (fs->Disk->diskCmnt != NULL)
			printf("%s", fs->Disk->diskCmnt);

		if (theFS->GetFSFeatures() & (CFileSystem::FSFT_LABEL | CFileSystem::FSFT_DISK))		
		{
			char label[CFileSystem::MAX_FILE_NAME_LEN];
			if (((CFileSystem*)theFS)->GetLabel(label))
				printf("Disk label\t\t: '%s'\n", label);
		}

		char* runFile = nullptr;
		if (feat & CFileSystem::FSFT_AUTORUN && fs->GetAutorunFilename(&runFile))
			printf("Autorun filename\t: '%s'\n", runFile);
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
	if (feat & CFileSystem::FSFT_TAPE)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_TAPE)]);
	if (feat & CFileSystem::FSFT_AUTORUN)
		vFt.push_back(CFileSystem::FSFeatureNames[log2(CFileSystem::FSFT_AUTORUN)]);

	for (byte ftIdx = 0; ftIdx < vFt.size(); ftIdx++)
	{
		printf("%s, ", vFt[ftIdx].c_str());		
	}

	printf("\n");

	byte nameLen, extLen;
	theFS->GetFileNameLen(&nameLen, &extLen);
	printf("File name structure\t: %d.%d (name.extension)\n", nameLen, extLen);	
	

	return true;
}

bool CopyDisk(int argc, char* argv[])
{
	bool format = (argc >= 2 && stricmp(argv[1], "-f") == 0);
	bool formatWithoutConfirmation = (argc >= 3 && stricmp(argv[2], "-y") == 0);
	if (theFS != nullptr && (format ? formatWithoutConfirmation || Confirm("This copy operation will overwrite data on destination drive/image, if it exists already. Are you sure?") : true))
	{		
		if (theFS->GetFSFeatures() && CFileSystem::FSFT_DISK)
		{
			CFileSystem* fs = dynamic_cast<CFileSystem*>(theFS);
			CDiskBase* src = fs->Disk;
			char* dstName = argv[0];
			if ((void*)src == NULL || (void*)dstName == NULL || strlen((char*)dstName) == 0)
				return false;

			strupr((char*)dstName);

			src->SetProgressCallback(ProgressCallbackRead);
			CDiskBase* dst = InitDisk(dstName, &src->DiskDefinition);					
			bool res = dst->Open((char*)dstName, (format ? CDiskBase::OPEN_MODE_CREATE : CDiskBase::OPEN_MODE_EXISTING));

			if (res)
			{
				dst->SetProgressCallback(ProgressCallbackWrite);
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
			else
			{
				puts("The destionation disk could not be opened.");
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


bool CopyDiskFromCOM(char* remoteName, CFSCPM* fsSrc)
{
	bool res = SetCOMForIF1(remoteName, 19200);
	if (!res)
	{
		cout << "Could not open port " << remoteName << endl;
		return res;
	}
	word blockCnt = 0, blockIdx = 0, blockIdxIdx = 0;
	int blocksLeft = 0;
	vector<byte> bufCom(theDiskDesc.fsParams.BlockSize);

	res = ReadBufferFromIF1((byte*)&blockCnt, sizeof(word));

	if (blockCnt > fsSrc->FSParams.BlockCount || blockCnt == 0)
	{
		cout << "The received block count is not valid: " << blockCnt << "." << endl;
		return false;
	}

	vector<word> blIdx(blockCnt);
	if (res)
	{
		//Read block indexes.
		res = ReadBufferFromIF1((byte*)blIdx.data(), blockCnt * sizeof(word));

		blocksLeft = blockCnt;
	}

	while (res && blocksLeft > 0)
	{
		res = ReadBufferFromIF1(bufCom.data(), (word)bufCom.size());
		blockIdx = blIdx[blockIdxIdx++];

		printf("%cCopying block %03d, %03d blocks left.", 13, blockIdx, blocksLeft);

		if (kbhit())
		{
			cout << "Transfer canceled." << endl;
			break;
		}

		if (blockIdx < fsSrc->FSParams.BlockCount)
			res = fsSrc->WriteBlock(bufCom.data(), blockIdx);
		else
			cout << "Invalid block number " << blockIdx << "." << endl;

		if (!res)
			cout << "Error writing block " << blockIdx << "." << endl;

		blocksLeft--;
	}

	res = CloseCOM() && fsSrc->Init();

	return res;
}


bool CopyDiskToCOM(CFSCPM* fsSrc, char* remoteName)
{	
	bool res = SetCOMForIF1(remoteName, 19200);
	if (!res)
	{
		cout << "Could not open port " << remoteName << endl;
		return res;
	}

	const word blkCntTotal = fsSrc->GetMaxBlockCount();
	word blkCntUsed = blkCntTotal - fsSrc->GetFreeBlockCount();
	word blkIdx = 0, blkIdxIdx = 0;	
	vector<word> blkUsedIdx(blkCntUsed);

	while (blkIdx < blkCntTotal)
	{
		if (!fsSrc->IsBlockFree(blkIdx))
		{
			blkUsedIdx[blkIdxIdx++] = blkIdx;
		}

		blkIdx++;
	}

	res = WriteBufferToIF1((byte*)&blkCntUsed, sizeof(word)) && 
		WriteBufferToIF1((byte*)blkUsedIdx.data(), blkCntUsed * sizeof(word));

	vector<byte> blockBuf(fsSrc->FSParams.BlockSize);
	int blocksLeft = blkCntUsed;
	blkIdxIdx = 0;
	while (res && blocksLeft > 0)
	{
		blkIdx = blkUsedIdx[blkIdxIdx++];
		printf("%cCopying block %03d, %03d blocks left.", 13, blkIdx, blocksLeft--);
		res = fsSrc->ReadBlock(blockBuf.data(), blkIdx) && WriteBufferToIF1(blockBuf.data(), blockBuf.size());
	}

	CloseCOM();

	return res;
}

bool CopyFSDisk(CFSCPM* fsSrc, CFSCPM* fsDst)
{
	bool res = true;

	CDiskBase::DiskDescType ddSrc, ddDst;
	res = fsSrc->Disk->DetectDiskGeometry(ddSrc) && fsDst->Disk->DetectDiskGeometry(ddDst);

	bool sameGeometry = 
		ddSrc.TrackCnt == ddDst.TrackCnt &&
		ddSrc.SideCnt == ddDst.SideCnt &&
		ddSrc.SPT == ddDst.SPT &&
		ddSrc.SectSize == ddDst.SectSize;

	if (!sameGeometry)
	{
		res = false;
		cout << "The destination disk has a different format than the source disk. Format it first." << endl;
		return res;
	}

	//Copy the used blocks only, ignore the free blocks, for efficiency.		
	const word blkCntTotal = fsSrc->GetMaxBlockCount();
	word blkCntUsed = blkCntTotal - fsSrc->GetFreeBlockCount();
	word blkIdx = 0;
	vector<byte> buf(fsSrc->GetBlockSize());
	while (res && blkIdx < blkCntTotal)
	{
		if (!fsSrc->IsBlockFree(blkIdx))
		{
			printf("%cCopying block %03d, %03d blocks left.", 13, blkIdx, blkCntUsed);

			res = fsSrc->ReadBlock(buf.data(), blkIdx) && fsDst->WriteBlock(buf.data(), blkIdx);

			blkCntUsed--;
		}

		blkIdx++;
	}

	return res;
}


//Copy file system blocks from one disk to another or from/to specified COM port. 
//Currently only supported for CP/M, since it's the only FS with write support.
bool CopyFS(int argc, char* argv[])
{
	bool res = true;
	enum TransferType
	{
		TO_COM,
		FROM_COM,
		TO_DISK,
		FROM_DISK
	} transType;

	char* remoteName = argv[1];
	string direction = argv[0];
	if (direction != "to" && direction != "from")
	{
		cout << "Direction must be specified as 'to' or 'from'." << endl;
		return false;
	}

	bool isCOM = CFileArchive::StrWildCmp(remoteName, "COM?") || CFileArchive::StrWildCmp(remoteName, "COM??");
	
	if (isCOM)
	{
		transType = (direction == "to" ? TO_COM : FROM_COM);
	}
	else
	{
		transType = (direction == "to" ? TO_DISK : FROM_DISK);
	}
	
	if (theFS == nullptr || ((theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) != CFileSystem::FSFT_DISK) ||
		!(theDiskDesc.fsType == FS_CPM_GENERIC || theDiskDesc.fsType == FS_CPM_HC_BASIC || theDiskDesc.fsType == FS_CPM_PLUS3_BASIC))
	{		
		cout << "Current disk is not a CP/M based file system." << endl;		
		return false;
	}

	CFSCPM* fsCurrent = dynamic_cast<CFSCPM*>(theFS);	
	if ((void*)fsCurrent == NULL || (void*)remoteName == NULL || strlen((char*)remoteName) == 0)
		return false;
	strupr((char*)remoteName);			
	
	if (transType == FROM_COM)
	{
		res = CopyDiskFromCOM(remoteName, fsCurrent);
	}	
	else if (transType == TO_COM)
	{
		res = CopyDiskToCOM(fsCurrent, remoteName);
	}
	else if (transType == TO_DISK || transType == FROM_DISK)
	{
		CFSCPM* fsRemote = nullptr;
		CDiskBase* dskRemote = InitDisk(remoteName, &fsCurrent->Disk->DiskDefinition);
		res = dskRemote->Open((char*)remoteName, CDiskBase::OPEN_MODE_EXISTING);		
		if (res)
		{
			fsRemote = new CFSCPM(dskRemote, fsCurrent->FSParams, fsCurrent->FSDesc);
			res = fsRemote->Init();
		}
		if (!res)
		{
			cout << "Could not open " << remoteName << "." << endl;
			delete fsRemote;
			return res;
		}		

		if (transType == TO_DISK)
		{
			res = CopyFSDisk(fsCurrent, fsRemote);
		}
		else if (transType == FROM_DISK)
		{
			res = CopyFSDisk(fsRemote, fsCurrent);
			fsCurrent->Init();
		}

		delete fsRemote;
	}


	printf("\n");	


	return res;
}

/*
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
*/

bool PutFile(int argc, char* argv[])
{
	if (theFS == nullptr)
		return false;

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

	CFile* fileNew = theFS->NewFile(nameDest.c_str());			
	theFS->SetFileFolder(fileNew, folder);										
	dword fsz = fsize(name);	
	FILE* fpc = fopen(name, "rb");
	if (res && fpc != NULL && fileNew != NULL)
	{		
		byte* buf = new byte[fsz];				
		fread(buf, 1, fsz, fpc);
		fclose(fpc);
		fileNew->SetData(buf, fsz);		
		
		if (IsBasic)
		{
			CFileSpectrum* s = dynamic_cast<CFileSpectrum*>(fileNew);

			s->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
			s->SpectrumStart = 0;
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

					if (stStr == "p")
					{
						s->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
						s->SpectrumVarLength = Basic::BasicDecoder::GetVarSize(buf, (word)fsz);							
					}
					else if (stStr == "b")
						s->SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
					else if (stStr == "c")
						s->SpectrumType = CFileSpectrum::SPECTRUM_CHAR_ARRAY;
					else if (stStr == "n")
						s->SpectrumType = CFileSpectrum::SPECTRUM_NUMBER_ARRAY;
					
					argIdx++;
				}

				argIdx++;
			}				
		}		
			
		res = theFS->WriteFile(fileNew);		
		delete[] buf;
	}	
	else		
		res = false;			

	delete fileNew;
	

	return res;
}

bool DeleteFiles(int argc, char* argv[])
{
	char* fspec = (char*)argv[0];
	byte fCnt = 0;
	bool withoutConfirmation = (argc >= 2 && stricmp(argv[1], "-y") == 0);

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
		if (withoutConfirmation || Confirm(msg))
			return theFS->Delete(fspec);
		else
			return false;
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
	//CTZXFile::TZXBlkArhiveInfo* blkArh;
	//CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;
	//dword bufIdx;

	if (theTap != NULL)
	{	
		if (realTime)
			printf("Playing the tape. Press ESC to quit, SPACE to pause. ");
		printf("Block count: %d.\n",  theTap->GetBlockCount());
		
		CTapeBlock tb;
		char Name[CTapeBlock::TAP_FILENAME_LEN + 1];	
		stringstream wavName;		
		if (!realTime)
		{
			wavName << theTap->fileName << ".wav";
			tape2wav.Open((char*)wavName.str().c_str());
		}		

		bool blockRead = theTap->GetFirstBlock(&tb);		
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
				if (tb.m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
				{				
					if (tb.m_BlkType == CTapeBlock::TAPE_BLK_PAUSE)
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
						if (tb.m_BlkType == CTapeBlock::TAPE_BLK_STD)
							printf("%sStandard Block\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else if (tb.m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
							printf("%sTurbo Block\t\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else
							printf("%sRaw data\t\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");

						if ((tb.m_BlkType == CTapeBlock::TAPE_BLK_STD || tb.m_BlkType == CTapeBlock::TAPE_BLK_TURBO) &&
							tb.IsBlockHeader())
						{
							tb.GetName(Name);
							printf("%-7s: %s\t%d\t%02X\n", 
								tb.TapBlockTypeNames[((CTapeBlock::TapeBlockHeaderBasic*)tb.Data)->BlockType], 
								Name, tb.Length, tb.Flag);
						}
						else
							printf("Data   :\t\t%d\t%02X\n", tb.Length, tb.Flag);

						//play along
						if (realTime)
						{		
							wavName.str("");
							wavName << theTap->fileName << theTap->m_CurBlkIdx << ".wav";
							tape2wav.Open((char*)wavName.str().c_str());
							tape2wav.AddBlock(&tb);
							tape2wav.Close();
							dword lenMs = tape2wav.GetWrittenLenMS();
							dword timeIdx = 0;

							PlaySound((wavName.str().c_str()), NULL, SND_FILENAME | SND_ASYNC);

							while (!GetAsyncKeyState(VK_ESCAPE) && !GetAsyncKeyState(VK_SPACE) && timeIdx < lenMs)
							{
								Sleep(1000);
								timeIdx += 1000;
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

							Sleep(500); //Need to sleep, to let PlaySound finish before starting a new block.
							remove(wavName.str().c_str());
						}							
						else
							tape2wav.AddBlock(&tb);

					}						
				}					
				else if (tzx != nullptr) //Metadata block
				{
					char msg[256];

					switch (tzx->m_CurrBlkID)
					{
					case CTZXFile::BLK_ID_TXT_DESCR:
					case CTZXFile::BLK_ID_MSG:
						strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
						msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
						printf("Message/Description: '%s'\n", msg);
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
					/*
					case CTZXFile::BLK_ID_ARH_INF:
						blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb.Data;
						bufIdx = sizeof(word) + sizeof(byte);
						blkTxt = &blkArh->TxtItem;

						printf("Arhive Info:\n");
						for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
						{													
							char* line;
							byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ? CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

							bufIdx += sizeof(byte)*2;								

							printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
							line = msg;
							strncpy(msg, (char*)&tb.Data[bufIdx], blkTxt->TxtLen);
							msg[blkTxt->TxtLen] = 0;

							if ((line = strtok(msg, "\r")) != NULL)
							{
								do 
								{
									printf("%s%s\n", line == msg ? "" : "\t\t\t", line);									
								} while(line = strtok(NULL, "\r"));									
							}

							bufIdx += blkTxt->TxtLen;																															
							blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb.Data[bufIdx];
						}
						break;
					*/
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
						printf("Block type %d skipped.\n", tzx->m_CurrBlkID);
					}					
				}
			}
			
			if (tb.m_BlkType != CTapeBlock::TAPE_BLK_METADATA && tb.Length > 0 && tb.Data != nullptr)
			{
				delete[] tb.Data;
				tb.Data = nullptr;
			}

			blockRead = theTap->GetNextBlock(&tb);
		};
ExitLoop:		

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
	if (theFS == NULL)
		return false;
	
	//bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	if (!IsBasic)
	{
		cout << "Can export to tape only a Spectrum file collection." << endl;
		return false;
	}


	char* tmpName = "temp.tap";
	string outName = argv[0];
	transform(outName.begin(), outName.end(), outName.begin(), (int (*)(int))::toupper);
	string ext = GetExtension(outName);			
	if (ext != "TAP")
		outName += ".TAP";
	CFileArchiveTape tapeDest(tmpName);
	tapeDest.Open(tmpName, true);
	
	char* fspec = "*";
	if (argc >= 2)
		fspec = (char*)argv[1];

	bool convertLoader = false;
	if (argc >= 3 && stricmp(argv[2], "-convldr") == 0)
		convertLoader = true;

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
												
			auto loadedBlocks = GetLoadedBlocksInProgram(buf, (word)fst->GetLength(), fst->SpectrumVarLength);	
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

								
	if (convertLoader)
	{
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
	else
		rename(tmpName, outName.c_str());	


	return true;
}

bool ImportTape(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	bool IsDisk = (theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) > 0;
	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	bool IsHCDisk = (strstr(theFS->Name, "HC BASIC") != nullptr);
	bool IsPlus3 = (strstr(theFS->Name, "Spectrum +3") != nullptr);
	char* tapSrcName = argv[0];
	char* tapTempName = nullptr;

	StorageLoaderType ldrType = LDR_TAPE;
	if (IsHCDisk)
		ldrType = LDR_HC_DISK;
	else if (IsPlus3)
		ldrType = LDR_SPECTRUM_P3_DISK;

	bool convertLoader = false;
	if (argc >= 3 && stricmp(argv[2], "-convldr") == 0)
		convertLoader = true;

	//Conversion for BASIC loaders
	if (convertLoader)
	{
		tapTempName = "temp.tap";
		CFileArchiveTape tapeSrc(tapSrcName);
		CFileArchiveTape tapeDst(tapTempName);
		if (tapeSrc.Open(tapSrcName, false) && tapeSrc.Init() &&
			tapeDst.Open(tapTempName, true))
		{			
			//Convert loader or set tape syntax, but with proper file names.
			ConvertBASICLoaderForDevice(&tapeSrc, &tapeDst, ldrType);
			tapeSrc.Close();
			tapeDst.Close();

			tapSrcName = tapTempName;
		}		
	}

	bool writeOK = true;
	if (IsBasic)
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


bool PutFilesIF1COM(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	if (!IsBasic)
	{
		puts("Can send only BASIC files to IF1 trough COM port.");
		return false;
	}	

	char* fspec = (char*)argv[0];		

	char* comName = "COM1";
	if (argc >= 2)
		comName = argv[1];	

	dword baud = 4800; //HC put transfer is reliable at max 4800 baud.
	if (argc >= 3)
		baud = atoi(argv[2]);

	printf("Type on the Spectrum: FORMAT \"b\";%u and\nLOAD *\"b\"  - to load to RAM or\nMOVE \"b\" TO \"d\";1;\"filename\" - to save to disk.\n", baud);

	//First export selected blocks to tape.
	bool res = true;
	char* expArgs[2] = { "exported.tap", fspec };
	res = Export2Tape(2, expArgs);
	if (!res)
	{
		puts("Could not export selected blocks to tape.");
		remove("exported.tap");
		return res;
	}

	//Convert exported tape to IF1 synthax.
	CFileArchiveTape tapExp("exported.tap");
	CFileArchiveTape tapIF1("if1.tap");
	res = tapExp.Open("exported.tap") && tapExp.Init() && tapIF1.Open("if1.tap", true) && ConvertBASICLoaderForDevice(&tapExp, &tapIF1, LDR_IF1_COM);
	tapIF1.Close();		
	tapExp.Close();
	remove("exported.tap");
	if (!res)
	{
		puts("Could not convert tape to IF1 format.");
		remove("if1.tap");
		return res;
	}
	
	
	//Send IF1 tape to COM port.
	CFileArchiveTape tap("if1.tap");
	res = tap.Open("if1.tap") && tap.Init();
	CFile* f = tap.FindFirst("*");
	while (f != NULL && res)
	{
		CFileSpectrumTape* fst = new CFileSpectrumTape(*dynamic_cast<CFileSpectrum*>(f),
			*dynamic_cast<CFile*>(f));

		CFileArchive::FileNameType fn;
		fst->GetFileName(fn);
		printf("Sending block '%s: %s' using baud %u. Press any key to cancel.\n", CFileSpectrum::SPECTRUM_FILE_TYPE_NAMES[fst->GetType()], fn, baud);
		res = SendFileToIF1(fst, comName, baud);		
		puts("");

		//Sleep a bit after each block, to let the Spectrum resume the loading. Otherwise, it breaks the current load.
		Sleep(500);
			
		f = tap.FindNext();
		delete fst;
	}
	tap.Close();
	remove("if1.tap");	
	CloseCOM();

	return res;
}


bool GetFileIF1COM(int argc, char* argv[])
{
	if (theFS == NULL)
		return false;

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) == CFileSystem::FSFT_ZXSPECTRUM_FILES;
		//(theFS->GetFSFeatures() & CFileSystem::FSFT_DISK) == CFileSystem::CFileSystem::FSFT_DISK;
	if (!IsBasic)
	{
		puts("Can write BASIC files only to a BASIC file system.");
		return false;
	}

	CFileArchive::FileNameType fn;
	strcpy(fn, argv[0]);

	char* comName = "COM1";
	if (argc >= 2)
		comName = argv[1];	

	dword baud = 9600; //HC get transfer is reliable at max 9600 baud.
	if (argc >= 3)
		baud = atoi(argv[2]);

	printf("Type on the Spectrum: FORMAT \"b\";%u and\nSAVE *\"b\" - to load to RAM, or\nMOVE \"d\";1;\"filename\" TO \"b\" - to save to disk.\n", baud);
			
	CFileSpectrumTape* fst = new CFileSpectrumTape();	
	fst->SetFileName(fn);
	bool res = GetFileFromIF1(fst, comName, baud);
	if (res)
	{
		CFile* fDest = theFS->NewFile(fn);
		*dynamic_cast<CFileSpectrum*>(fDest) = *dynamic_cast<CFileSpectrumTape*>(fst);
		*dynamic_cast<CFile*>(fDest) = *dynamic_cast<CFile*>(fst);
		res = theFS->WriteFile(fDest);
		delete fDest;
		delete fst;
	}		
	else
	{
		puts("Failed to receive file.");
	}

	return res;
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

			delete[] trkBuf;			
		}
		else
		{
			printf("There are no boot tracks on the current disk.\n");
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
			printf("There are no boot tracks on the current disk.\n");
			res = false;
		}

		delete[] trkBuf;			
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

	bool formatWithoutConfirmation = false;	

	auto ext = GetExtension(path);
	if (ext == "TAP")
	{
		ofstream tapFile(path);
		tapFile.close();
		formatWithoutConfirmation = (argc >= 2 && stricmp(argv[3], "-y") == 0);
		return true;
	}
	else
	{ 
		formatWithoutConfirmation = (argc >= 4 && stricmp(argv[3], "-y") == 0);
	}
	
	if (!formatWithoutConfirmation && !Confirm("The format operation will erase existing data. Are you sure?"))
		return false;

	CDiskBase::DiskDescType dd;

	if (selGeom == 0xFF)
	{
		vector<byte> geometries;
		for (byte idx = 0; idx < sizeof(DISK_TYPES)/sizeof(DISK_TYPES[0]); idx++)			
			geometries.push_back(idx);
		selGeom = AskGeometry(geometries);
		res = (selGeom >= 0 && selGeom != 0xFF);
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
			fs = new CFSHCBasic(disk, diskDesc.fsParams, *(CFSCPM::CPMParams*)&diskDesc.otherParams, diskDesc.Name);								
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
	{		
		disk->SetProgressCallback(ProgressCallbackFormat);
		res = fs->Format();
	}

	if (fs != nullptr)
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

bool Bin2REM(string blobFileName, word addrForMove, string programName)
{	
	const long blobSize = fsize(blobFileName.c_str());
	if (blobSize > 48 * 1024)
	{
		puts("Blob size is too big.");
		return false;
	}

	FILE* fBlob = fopen(blobFileName.c_str(), "rb");
	if (fBlob == NULL)
	{
		printf("Could not open blob file %s.", blobFileName.c_str());
		return false;
	}

	byte basLdrREM[] = {
		Basic::BASICKeywordsIDType::BK_RANDOMIZE,
		Basic::BASICKeywordsIDType::BK_USR,
		Basic::BASICKeywordsIDType::BK_VAL,
		'"', '3', '1', '+',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '5', '+', '2', '5', '6', '*',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '6',
		'"', ':', Basic::BASICKeywordsIDType::BK_REM
	};
	word basLdrSize = sizeof(basLdrREM);

	byte asmLdr[] = {
		0x21, 14, 0,										//ld hl, lenght of ASM loader, the blob follows it
		0x09,												//add hl, bc	;BC holds the address called from RANDOMIZE USR.
		0x11, (byte)(addrForMove % 256), (byte)(addrForMove / 256),			//ld de, start addr
		0xD5,												//push de		;Jump to start of blob. 
		0x01, (byte)(blobSize % 256), (byte)(blobSize / 256),	//ld bc, blob size
		0xED, 0xB0,											//ldir
		0xC9												//ret		
	};
	word asmLdrSize = (addrForMove > 0 ? sizeof(asmLdr) : 0);
	
	const word ldrSize = basLdrSize + asmLdrSize;
	byte* blobBuf = new byte[ldrSize + blobSize];

	memcpy(blobBuf, basLdrREM, basLdrSize);
	//Copy LDIR loader in front of the blob.
	if (addrForMove > 0)
		memcpy(blobBuf + basLdrSize, asmLdr, asmLdrSize);

	fread(&blobBuf[ldrSize], 1, blobSize, fBlob);
	fclose(fBlob);

	//Create BASIC line from buffer.	
	const word progLineNo = 0;
	Basic::BasicLine bl(blobBuf, ldrSize + (word)blobSize, progLineNo);

	//Create header for Spectrum file.
	CFile* f = theFS->NewFile(programName.c_str());
	CFileSpectrum* fs = dynamic_cast<CFileSpectrum*>(f);
	fs->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
	fs->SpectrumStart = bl.lineNumber;
	fs->SpectrumVarLength = 0;
	fs->SpectrumArrayVarName = 0;
	fs->SpectrumLength = bl.lineSize;

	//Write BASIC line to buffer.
	Basic::BasicDecoder bd;
	byte* lastBuf = new byte[bl.lineSize];
	bd.PutBasicLineToLineBuffer(bl, lastBuf);	
	f->SetData(lastBuf, bl.lineSize);
	delete[] lastBuf;
	delete[] blobBuf;

	bool res = theFS->WriteFile(f);
	delete f;

	return true;
}


bool Bin2Var(string blobFileName, word addrForMove, string programName)
{
	const long blobSize = fsize(blobFileName.c_str());
	if (blobSize > 48 * 1024)
	{
		puts("Blob size is too big.");
		return false;
	}

	FILE* fBlob = fopen(blobFileName.c_str(), "rb");
	if (fBlob == NULL)
	{
		printf("Could not open blob file %s.", blobFileName.c_str());
		return false;
	}

	byte asmLdr[] = {
		0x21, 14, 0,										//ld hl, lenght of ASM loader, the blob follows it
		0x09,												//add hl, bc	;BC holds the address called from RANDOMIZE USR.
		0x11, (byte)(addrForMove % 256), (byte)(addrForMove / 256),			//ld de, start addr
		0xD5,												//push de		;Jump to start of blob. 
		0x01, (byte)(blobSize % 256), (byte)(blobSize / 256),	//ld bc, blob size
		0xED, 0xB0,											//ldir
		0xC9												//ret		
	};
	word asmLdrSize = (addrForMove > 0 ? sizeof(asmLdr) : 0);

	byte varHdr[] = { ((Basic::BASVarType::BV_STRING << 5) | ('x' - 'a' + 1)), (byte)((blobSize + asmLdrSize) % 256), (byte)((blobSize + asmLdrSize) / 256) };
	const word varHdrSize = sizeof(varHdr);

	byte basLdrVar[] = {
		Basic::BASICKeywordsIDType::BK_RANDOMIZE,
		Basic::BASICKeywordsIDType::BK_USR,
		Basic::BASICKeywordsIDType::BK_VAL, '"',
		//Address of blob is variable area + 3 bytes for variable header
		(varHdrSize + '0'), '+',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '2', '7', '+', '2', '5', '6', '*',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '2', '8', '"',
		0x0D
	};
	word basLdrSize = sizeof(basLdrVar);

	const word ldrSize = basLdrSize + asmLdrSize;
	const word varLen = varHdrSize + asmLdrSize + (word)blobSize;
	byte* blobBuf = new byte[ldrSize + varHdrSize + (word)blobSize];

	//Copy basic loader.
	memcpy(blobBuf, basLdrVar, basLdrSize);
	//Copy variables header.
	memcpy(blobBuf + basLdrSize, varHdr, varHdrSize);

	if (addrForMove > 0)	
		//Copy LDIR loader in front of the blob.
		memcpy(blobBuf + basLdrSize + varHdrSize, asmLdr, asmLdrSize);	

	fread(&blobBuf[ldrSize + varHdrSize], 1, blobSize, fBlob);
	fclose(fBlob);

	//Create BASIC line from buffer.	
	const word progLineNo = 0;
	Basic::BasicLine bl(blobBuf, basLdrSize, progLineNo);	

	//Create header for Spectrum file.
	CFile* f = theFS->NewFile(programName.c_str());
	CFileSpectrum* fs = dynamic_cast<CFileSpectrum*>(f);
	fs->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
	fs->SpectrumStart = bl.lineNumber;
	fs->SpectrumVarLength = varLen;
	fs->SpectrumArrayVarName = 0;
	fs->SpectrumLength = bl.lineSize + varLen;

	//Write BASIC line to buffer.
	Basic::BasicDecoder bd;
	byte* lastBuf = new byte[bl.lineSize + varLen];
	bd.PutBasicLineToLineBuffer(bl, lastBuf);	
	memcpy(&lastBuf[bl.lineSize], &blobBuf[basLdrSize], varLen);
	f->SetData(lastBuf, bl.lineSize + varLen);
	delete[] lastBuf;
	delete[] blobBuf;

	bool res = theFS->WriteFile(f);
	delete f;

	return true;
}


//Convers a Z80 binary into a BASIC block with a REM line OR in a variable (not visible in program).
//Before executing the Z80 binary, it moves it to the specified address, if the address is specified.
bool Bin2BAS(int argc, char* argv[])
{
	if (theFS == nullptr)
		return false;

	bool IsBasic = (theFS->GetFSFeatures() & CFileSystem::FSFT_ZXSPECTRUM_FILES) > 0;
	if (!IsBasic)
	{
		puts("The file system doesn't hold BASIC files.");
		return false;
	}

	string blobName, programName;
	const bool blobToVar = (string(argv[0]) == "var");
	word addr = 0;
	
	if (argc >= 2)
		blobName = argv[1];
	if (argc >= 3)
		programName = argv[2];
	if (argc >= 4)
		addr = atoi(argv[3]);
	
	if (programName.length() == 0)
		programName =  blobName;	
	
	bool res;

	if (blobToVar)
	{
		res = Bin2Var(blobName, addr, programName);
	}
	else
	{
		res = Bin2REM(blobName, addr, programName);
	}											
		
	return res;	
}


//Automatic BASIC loader conversion for different storage devices (HC disk, IF1 serial, Spectrum disk, Opus, etc).
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
				//Convert file name to match destination FS naming rules.
				//dst->CreateFileName(fileName, fileToCheck);
				//fileToCheck->GetFileName(fileName);
				// 
				//For +3 the name in the BASIC loader must contain dot if lenght is > 8.
								
				char name[sizeof(CFileArchive::FileNameType)];
				char ext[4];															
				fileToCheck->GetFileName(fileName);
				CFile fTmp;
				theFS->CreateFileName(fileName, &fTmp);
				fTmp.GetFileName(name, ext);
				if (strlen(ext) > 0)
					sprintf(fileName, "%s.%s", name, ext);				
				else
					sprintf(fileName, "%s", name);
				
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
			} while (bufPosIn < srcFileSpec->SpectrumLength - srcFileSpec->SpectrumVarLength);

			//Copy variables too.
			if (srcFileSpec->SpectrumVarLength > 0)
			{
				memcpy(bufDst + bufPosOut, bufSrc + bufPosIn, srcFileSpec->SpectrumVarLength);
				bufPosOut += srcFileSpec->SpectrumVarLength;
			}

			CFileArchive::FileNameType fileName;
			srcFileSpec->GetFileName(fileName);
			CFile* file = new CFile(fileName, bufPosOut, bufDst);
			CFileSpectrumTape* dstFile = new CFileSpectrumTape(*srcFileSpec, *file);
			dstFile->SpectrumLength = bufPosOut;
			dst->AddFile(dstFile);

/*
#ifdef _DEBUG
			TextViewer(DecodeBASICProgramToText(bufSrc, bufPosIn));
			TextViewer(DecodeBASICProgramToText(bufDst, bufPosOut));
#endif
*/
			delete file;	
			delete dstFile;

			if (bufSrc != nullptr)
				delete [] bufSrc;
			if (bufSrc != nullptr)
				delete [] bufDst;

		}		
		else //Copy rest of blocks that are not program.
		{			
			dst->AddFile(srcFileSpec);		
		}
		
		srcFile = src->FindNext();
	}

	return true;	
}


bool DiskView(int argc, char* argv[])
{
	if (theDisk == NULL)
		return false;
	
	byte trackIdx = (argc >= 0 ? atoi(argv[0]) : 0);
	byte headIdx = (argc >= 1 ? atoi(argv[1]) : 0);
	byte sectIdx = (argc >= 2 ? atoi(argv[2]) : 0);

	byte trackCnt, headCnt, sectCnt;
	word sectSize;
	CDiskBase::SectDescType sectors[CDiskBase::MAX_SECT_PER_TRACK];

	bool res = theDisk->GetTrackInfo(trackIdx, headIdx, sectCnt, sectors);
	res = res && theDisk->GetDiskInfo(trackCnt, headCnt);		
	
	if (res)
	{
		sectSize = theDisk->SectCode2SectSize(sectors[sectIdx].sectSizeCode);
		printf("T x H x S: %d x %d x %d, size: %d, ID: %d\n", trackIdx, headIdx, sectIdx, sectSize, sectors[sectIdx].sectID);
		auto secBuf = vector<byte>(CDiskBase::SECT_SZ_4096);
		res = theDisk->ReadSectors(secBuf.data(), trackIdx, headIdx, sectIdx + 1, 1);
		TextViewer(GetHexPrint(secBuf.data(), sectSize));		
	}		

	return res;
}

bool BasImport(int argc, char* argv[])
{
	string name = GetNameWithoutExtension(argv[0]);
	if (argc >= 2)
		name = argv[1];

	word autorunLine = 0;
	if (argc >= 3)
		autorunLine = atoi(argv[2]);
		

	bool res = bas2tap(argv[0], name.c_str(), autorunLine) == 0;	
	
	//Add binary as variable.
	byte* varBuf = nullptr;
	word varBufLen = 0;
	if (res && argc >= 4)
	{
		auto varFileName = argv[3];
		auto varLen = fsize(varFileName);
		const byte varHdr[] = { ((Basic::BASVarType::BV_STRING << 5) | ('x' - 'a' + 1)), (byte)(varLen % 256), (byte)(varLen / 256) };

		auto varFile = fopen(varFileName, "rb");
		varBufLen = sizeof(varHdr) + varLen;
		varBuf = new byte[varBufLen];
		memcpy(varBuf, varHdr, sizeof(varHdr));
		fread(varBuf + sizeof(varHdr), 1, varLen, varFile);
		fclose(varFile);		
	}

	char* tapToImport = "temp.tap";
	if (res && varBuf != nullptr)
	{
		//Open temporary file created by BasImp.
		CFileArchiveTape* tapeOld = new CFileArchiveTape(tapToImport);
		tapeOld->Open(tapToImport);
		tapeOld->Init();
		
		//Get program block blob and copy it into memory.
		auto oldProg = (CFileSpectrumTape*)tapeOld->FindFirst("*");
		word progLen = (word)oldProg->GetLength();
		byte* newProgBuf = new byte[progLen + varBufLen];
		oldProg->GetData(newProgBuf);
		memcpy(newProgBuf + progLen, varBuf, varBufLen);
		delete[] varBuf;

		//Create new program block and copy old program + new variables.
		CFileSpectrumTape* newProg = new CFileSpectrumTape(*oldProg);
		newProg->SetData(newProgBuf, progLen + varBufLen);	
		newProg->SpectrumVarLength = varBufLen;
		newProg->SpectrumLength = progLen + varBufLen;
		delete[] newProgBuf;		
		//delete oldProg;
		delete tapeOld;		

		//Create new program tape.
		CFileArchiveTape* tapeNew = new CFileArchiveTape("newProg.tap");
		tapeNew->Open("newProg.tap", true);
		tapeNew->AddFile(newProg);
		tapeNew->Close();
		delete newProg;
		delete tapeNew;

		tapToImport = "newProg.tap";
		remove("temp.tap");
	}

	const char* argsImport[] = {tapToImport, name.c_str()};
	if (res)
	{
		res = ImportTape(1, (char**)argsImport);		
		remove(tapToImport);
	}
	return res;
}


bool ScreenProcess(int argc, char* argv[])
{
	string scrOper = argv[0];
	string operArg = argv[1];
	string nameIn = argv[2];
	string nameOut = argv[3];

	if (fsize(nameIn.c_str()) != sizeof(ScreenType))
	{
		cout << "File '" << nameIn << "' is not a valid SCREEN$ file." << endl;
		return false;
	}
	ScreenType scrInBuf;
	FILE* scrIn = fopen(nameIn.c_str(), "rb");
	fread(&scrInBuf, 1, sizeof(ScreenType), scrIn);
	fclose(scrIn);

	byte bufOut[sizeof(ScreenType)];

	if (scrOper == "order")
	{
		ScrOrderType ordType = ORD_NONE;
		if (operArg == ScrOrdNames[ORD_COLUMN])
			ordType = ORD_COLUMN;
		else if (operArg == ScrOrdNames[ORD_CELL])
			ordType = ORD_CELL;
		else
		{
			cout << "SCREEN$ ordering must be " << ScrOrdNames[ORD_COLUMN] << " or " << ScrOrdNames[ORD_CELL] << endl;
			return false;
		}
		
		switch (ordType)
		{
		case ORD_COLUMN:
			ScrLiniarizeByColumn(&scrInBuf, bufOut);
			break;
		case ORD_CELL:
			ScrLiniarizeByCell(&scrInBuf, bufOut);
			break;
		}
		
		FILE* scrOut = fopen(nameOut.c_str(), "wb");
		fwrite(bufOut, 1, sizeof(ScreenType), scrOut);
		fclose(scrOut);
		cout << "Created '" << nameOut << "' ordered by " << ScrOrdNames[ordType] << "." << endl;
	}
	else if (scrOper == "blank")
	{
		word l1 = 5, c1 = 1, l2 = 22, c2 = 31;
		char x;
		stringstream ss(operArg);
		ss >> l1 >> x >> c1 >> x >> l2 >> x >> c2;		

		ScrBlankRectangle(&scrInBuf, (byte)l1, (byte)c1, (byte)l2, (byte)c2);
		FILE* scrOut = fopen(nameOut.c_str(), "wb");
		fwrite(&scrInBuf, 1, sizeof(ScreenType), scrOut);
		fclose(scrOut);
		printf("Created '%s' with blanked rectange %ux%ux%ux%u\n", nameOut.c_str(), l1, c1, l2, c2);
	}
			

	return true;
}


bool BinCut(int argc, char* argv[])
{
	char* fnameIn = argv[0];
	char* fnameOut = argv[1];
	char* offsetStr = argv[2];
	char* lenStr = (argc >= 3 ? argv[3] : "0");

	long offset = atoi(offsetStr);
	long fsizeIn = fsize(fnameIn);
	long lenBlock = atoi(lenStr);

	if (lenBlock == 0)
		lenBlock = fsizeIn - offset;

	if (offset + lenBlock > fsizeIn)
	{
		cout << offset << " + " << lenBlock << " do not fit the file " << fnameIn << " lenght of " << fsizeIn << "." << endl;
		return false;
	}

	FILE* fin = fopen(fnameIn, "rb");
	FILE* fout = fopen(fnameOut, "wb");
	if (fin == nullptr || fout == nullptr)
	{
		cout << "Couln't open input or output file." << endl;
		return false;
	}
	
	fseek(fin, offset, SEEK_SET);
	long leftBytes = lenBlock;
	while (leftBytes > 0)
	{
		fputc(fgetc(fin), fout);
		leftBytes--;
	}

	fclose(fin);
	fclose(fout);

	cout << "Created file " << fnameOut << " with lenght " << lenBlock << "." << endl;

	return true;
}

bool BinPatch(int argc, char* argv[])
{
	char* fnameIn = argv[0];		
	char* fnamePatch = argv[1];
	char* offsetStr = argv[2];

	long offset = atoi(offsetStr);
	long fsizeIn = fsize(fnameIn);
	long fsizePatch = fsize(fnamePatch);
	
	
	FILE* fToPatch = fopen(fnameIn, "r+b");
	FILE* fThePatch = fopen(fnamePatch, "rb");
	if (fToPatch == nullptr || fThePatch == nullptr)
	{
		cout << "Couln't open input or output file." << endl;
		return false;
	}

	fseek(fToPatch, offset, SEEK_SET);
	long leftBytes = fsizePatch;
	while (leftBytes > 0)
	{
		fputc(fgetc(fThePatch), fToPatch);
		leftBytes--;
	}

	fclose(fToPatch);
	fclose(fThePatch);

	cout << "Applied patch file " << fnamePatch << " with lenght " << fsizePatch << " to file " << fnameIn << " at offset " << offset << "." << endl;

	return true;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////

static const Command theCommands[] = 
{
	{{"help", "?"}, "Command list, this message",
		{
			{{"command"}, false, "Show help only for the specified command."}
		},
		PrintHelp},
	{{"fsinfo"}, "Display the known file systems", {{}},
		ShowKnownFS},
	{{"stat"}, "Display the current file system parameters", {{}}, Stat},
	{{"open"}, "Open disk or disk image",
		{
			{"drive|image", true, "The disk/image to open"},
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
		{"-t", false, "Copy as text"},
		{"-n newName", false, "Set the PC file name, default: original name"}
		},
		GetFile},
	{{"type", "cat"}, "Display file",
		{{"file spec.", true, "* or *.com or readme.txt, etc"},
		{"-h|-t|-d", false, "display as hex|text|asm"}},
		TypeFile},
	{{"copydisk"}, "Copy current disk to another disk or image, sector by sector",
		{{"destination", true, "destination disk/image"},
		 {"-f", false, "format destination while copying"},
		 {"-y", false, "format without confirmation"}
		}, CopyDisk},
	{{"copyfs"}, "Copy only used blocks from current file system to another disk (same FS type, CP/M only)",
		{
			{"direction", true, "'to'/'from'"},
			{"remote", true, "source/destination disk image (i.e. 1.dsk) or COM port (i.e. COM1)"}
		}, CopyFS},
	{{"put"}, "Copy PC file to file system",
		{{"source file", true, "the file to copy"},
		{"-n newname", false, "name for destination file"},
		{"-d <destination folder>", false, "file folder"},
		{"-s start, -t p|b|c|n file type", false, "Spectrum file attributes"}},
		PutFile},
	{{"del", "rm"}, "Delete file(s)",
		{{"file spec.", true, "the file(s) to delete"},
		{"-y", false, "delete without confirmation"}}, DeleteFiles},
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
		{"file mask", false, "the file name mask"},
		{"-convldr", false, "convert BASIC loader synthax, file names"}
		},
		Export2Tape},
	{{"tapimp"}, "Imports the TAP file to disk",
		{{".tap name", true, "the TAP file name"},
		{"file mask", false, "the file name mask"},
		{"-convldr", false, "convert BASIC loader synthax, file names"}
		},
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
	{{"bin2bas"}, "Put binary to BASIC block, in a REM statement or variable",
		{
			{"type", true, "type of conversion: rem or var"},
			{"file", true, "blob to add"},
			{"name of block", false, "name of BASIC block, default blob file name"},
			{"address of execution", false, "address to copy the block to before execution, default 0 - no moving blob to an address"},
		},
		Bin2BAS},
	{{"convldr"}, "Converts a BASIC loader to work with another storage device",
		{{".tap name", true, "destination TAP file name"},
		{"loader type", true, "type of loader: TAPE, MICRODRIVE, OPUS, HCDISK, IF1COM, PLUS3, MGT"}
		},
		ConvertBASICLoader},
	{{"putif1"}, "Send a file or collection to IF1 trough the COM port",
		{
		{"file name/mask", true, "file mask to select files for sending"},
		{"COMx", false, "COMx port to use, default COM1"},
		{"baud rate", false, "baud rate for COM, default is 4800"}
		},
		PutFilesIF1COM},
	{{"getif1"}, "Get a single file from IF1 trough the COM port",
		{{"file name", true, "file name for the received file"},
		{"COMx", false, "COMx port to use, default COM1"},
		{"baud rate", false, "baud rate for COM, default is 9600"}
		},
		GetFileIF1COM},
	{{"diskview"}, "View disk sectors",
		{{"track", false, "track index"},
		{"head", false, "head index"},
		{"sector", false, "sector index (not ID)"}
		},
		DiskView},
	{{"basimp"}, "Import a BASIC program from a text file",
		{{"BASIC file", true, "BASIC program file to compile"},
		{"file name", false, "Program file name (default: file name)"},
		{"autorun line", false, "Autorun line number (default: 0)"},
		{"variables", false, "Variable buffer to add"}
		},
		BasImport},
	{ {"screen"}, "SCREEN$ block processing functions",
		{
			{"operation", true, "order, blank"},
			{"argument", true, "order: column/cell; blank: line1xcol1xline2xcol2"},
			{"input file", true, "SCREEN$ file on PC read"},
			{"output file", true, "SCREEN$ file on PC write"}
		},
		ScreenProcess},
	{ {"bincut"}, "File section cut, starting at offset, with size length",
		{
			{"input file", true, "input file name"},
			{"output file", true, "output file name"},
			{"offset", true, "0 based start offset"},		
			{"lenght", false, "length of block, default: file len - offset"}
		},
		BinCut },
	{ {"binpatch"}, "Patches a file, with content of another file, at set offset",
		{
			{"input file", true, "input file name"},
			{"patch file", true, "patch content file"},
			{"offset", true, "0 based start offset in input file"},			
		},
		BinPatch },
	{{"exit", "quit"}, "Exit program", 
	{{}}},	
};


void PrintHelpCmd(Command theCommand)
{
	for (auto theAlias : theCommand.cmd)
	{
		if (theAlias != nullptr)
			printf("%s ", theAlias);
	}
	printf(" - %s\n", theCommand.desc);

	for (auto theParam : theCommand.Params)
	{
		if (theParam.param != NULL)
		{
			if (theParam.mandatory)
				printf("- <%s>: %s\n", theParam.param, theParam.desc);
			else
				printf("- [%s]: %s\n", theParam.param, theParam.desc);
		}
	}
}


bool PrintHelp(int argc, char* argv[])
{			
	string cmdToShow = argc >= 1 ? argv[0] : "";
	bool listAllCommands = cmdToShow.length() == 0;

	if (listAllCommands)
	{
		printf("This program is designed to manipulate ZX Spectrum computer related file systems on a modern PC.\n");

		for (auto theCommand : theCommands)
		{
			PrintHelpCmd(theCommand);
		}
	}	
	else
	{
		bool foundCmd = false;
		for (auto theCommand : theCommands)
		{
			for (auto theAlias : theCommand.cmd)
			{
				foundCmd = foundCmd || (theAlias != nullptr && theAlias == cmdToShow);
			}

			if (foundCmd)
			{
				PrintHelpCmd(theCommand);

				return true;
			}							
		}

		if (!foundCmd)
			cout << "Command '" << cmdToShow << "' does not exist." << endl;
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
		printf("Command not understood. Type ? for help.\n");

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
	printf("HCDisk 2.0 by George Chirtoaca, compiled on %s %s.\n", __DATE__, __TIME__);	

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
						{
							strncpy(params[prmIdx], &word[1], prmEnd - &word[1]);
							do
							{
								word = strtok(NULL, sep);
							} while (word != NULL && word <= prmEnd);
						}
						prmIdx++;
					}
					else
					{
						strcpy(params[prmIdx++], word);
						word = strtok(NULL, sep);
					}
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

