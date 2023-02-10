//Virtual base class for disk device/image

#ifndef _DISK_BASE_
#define _DISK_BASE_

#include "types.h"
#include <malloc.h>
#include <math.h>
#include <string.h>

#define DEF_RETRY_CNT 5

class CDiskBase
{
public:
	static const byte MAX_SECT_PER_TRACK = 32;		//Maximum sector count on a track
	typedef bool (ProgressCallbackType)(word item, word totalItems);
	
	typedef enum
	{
		ERR_OK,
		ERR_NOT_SUPPORTED,

		ERR_OPEN,
		ERR_READ,
		ERR_WRITE,
		ERR_SEEK,

		ERR_PARAM,		

		ERR_ITEM_CNT
	} ErrorType;	

	typedef enum
	{		
		DISK_DATARATE_HD,         //high
		DISK_DATARATE_DD_5_25,//double for 5.25         
		DISK_DATARATE_DD_3_5,   //double for 3.5   
		DISK_DATARATE_ED         //extra density
	} DiskDensType;

	typedef enum
	{
		SECT_SZ_128 = 128,
		SECT_SZ_256 = 256,
		SECT_SZ_512 = 512,
		SECT_SZ_1024 = 1024,		
		SECT_SZ_2048 = 2048,
		SECT_SZ_4096 = 4096,
	} DiskSectType;

	typedef enum
	{
		OPEN_MODE_EXISTING,
		OPEN_MODE_CREATE
	} DiskOpenMode;

	typedef struct
	{
		byte track;
		byte head;
		byte sectID;
		byte sectSizeCode;		
	} SectDescType;

	struct DiskDescType
	{
		byte TrackCnt;
		byte SideCnt;
		byte SPT;
		DiskSectType SectSize;
		byte Filler;
		byte GapFmt;		
		DiskDensType Density;				
		byte HWInterleave;
	};

	CDiskBase();
	CDiskBase(byte tracks, byte spt, byte sides = 2, DiskSectType st = SECT_SZ_512, DiskDensType dens = DISK_DATARATE_DD_3_5, 
		byte filler = 0xE5, byte gap = 0x16);	
	CDiskBase(DiskDescType dd);	
	CDiskBase(const CDiskBase& src);
	virtual ~CDiskBase();

	//128 = 0; 256 = 1; 512 = 2; 1024 = 3, etc.
	static DiskSectType SectCode2SectSize(byte sectCode) { return (DiskSectType)(128 * (1 << sectCode)); }
	static byte SectSize2SectCode(DiskSectType sectSize) 
	{ 
		word lg2 = 0, x = sectSize/128;
		while ((x = x >> 1) > 0)
			lg2++;
		return (byte)lg2; 
	}

	virtual bool ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt)=0;
	virtual bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff)=0;
	virtual bool FormatTrack(byte track, byte side)=0;
	virtual bool FormatDisk();
	virtual bool Seek(byte trackNo)=0;		
	virtual bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[])=0;
	virtual bool Open(char* src, DiskOpenMode)=0;

	virtual bool ReadTrack(byte* buff, byte track, byte head);
	virtual bool WriteTrack(byte track, byte head, byte* buff);	
	bool CopyTo(CDiskBase* dest,  bool formatDst = false);
	virtual bool GetDiskInfo(byte& trkCnt, byte& sideCnt, char* comment = NULL);
	bool DetectDiskGeometry(DiskDescType& dd);
	bool CreateInterleaveTable();
	byte CalculateInterleaveFactor();
	void SetContinueOnError(bool doIgnore) { continueOnError = doIgnore;  }
	void SetProgressCallback(ProgressCallbackType* callback) { progCallback = callback; }

	ErrorType GetLastError(char* errMsg) 
	{ 
		if (errMsg != NULL)
			strcpy(errMsg, ERROR_TYPE_MSG[LastError]);
		return LastError; 
	}

	DiskDescType DiskDefinition;
	char* diskCmnt;

protected:		
	byte m_Skew;			//Sector IDs for hardware interleave
	byte InterlaveTbl[MAX_SECT_PER_TRACK];
	static char* ERROR_TYPE_MSG[];
	ErrorType LastError;	
	bool continueOnError = true;
	ProgressCallbackType* progCallback = nullptr;
};


#endif//_DISK_BASE_
