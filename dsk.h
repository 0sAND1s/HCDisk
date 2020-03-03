#ifndef _DSK_H_
#define _DSK_H_

#include "types.h"
#include "DiskBase.h"
#include <stdio.h>

#define MAX_SECTOR_CNT	32		//Maximum sector count on a track
#define SIGNATURE_LEN	34

class CDSK: public CDiskBase
{
public:			
	typedef enum
	{
		DATA_RATE_UNKNOWN = 0,
		DATA_RATE_SINGLE_DOUBLE = 1,
		DATA_RATE_HIGH = 2,
		DATA_RATE_EXTENDED = 3,
	} DSK_DATA_RATE;

#pragma pack(1)
	typedef struct
	{
		char signature[34];
		char creatorName[14];
		byte trackNo;
		byte sideNo;
		word trackSize;
		byte trackSizeTbl[204];	//EDSK feature.
	} DiskInfoBlockType;


	typedef struct  
	{
		char signature[13];
		char unused1[3];
		byte trackNo;
		byte sideNo;
		byte dataRate;	//Extension
		byte recMode;	//Extension
		byte sectorSizeCode;
		byte sectorCount;
		byte gap3;
		byte fillerByte;
	} TrackInfoBlockType;


	typedef struct
	{
		byte track;
		byte side;
		byte sectorID;
		byte sectorSizeCode;
		byte ST1;
		byte ST2;
		byte sectorSize[2];
	} SectorInfoBlockType;


	typedef struct
	{
		byte track;
		byte head;
		byte sectorID;
		byte sectorSizeCode;		
	} TrackDescriptorType;
#pragma pack()

protected:
	char dskName[255];
	FILE * dskFile;	
	//char signature[SIGNATURE_LEN];
	byte sigMinValidLen;	
	static char *ErrMsg[ERR_ITEM_CNT];	

	virtual bool SeekSide(byte side);
	virtual bool SeekSector(byte sector);
	virtual bool CreateImage();
	virtual bool AddDiskHeader();
	virtual bool AddSectorHeaders(byte track, byte side);
	virtual char* GetSignature() {return DSK_SIGNATURE;};

	static char DSK_SIGNATURE[];
	static char DSK_CREATOR_NAME[];
	static char DSK_TRACK_SIGNATURE[];

	DiskInfoBlockType diskInfoBlk;
	TrackInfoBlockType currTrack;
	SectorInfoBlockType currSectorInfo[MAX_SECTOR_CNT];	

public:	
	CDSK();
	CDSK(DiskDescType diskDesk);
	CDSK(const CDSK& src);
	virtual ~CDSK();
	
	virtual bool Open(char * dskName, DiskOpenMode mode = OPEN_MODE_EXISTING);
	virtual bool Seek(byte trackNo);
	virtual bool ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt);
	virtual bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff);
	virtual bool FormatTrack(byte track, byte side);
	void SetInterleave(byte sectIDs[]);
	virtual bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[]);
	bool GetDiskInfo(byte & trackCount, byte & sideCount);
};


#endif//_DSK_H_