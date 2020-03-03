#ifndef CDISKTD0_H
#define CDISKTD0_H

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <vector>
#include "DiskBase.h"

using namespace std;

class CDiskImgTD0: public CDiskBase
{
public:
	CDiskImgTD0()
	{		
		imgBuff = NULL;
		diskCmnt = NULL;
		fileTD0 = NULL;
		DiskDefinition.Filler = 0xE5;
	}

	virtual bool Open(char* imgName, DiskOpenMode openMode = OPEN_MODE_EXISTING);
	virtual bool ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt);	
	virtual bool Seek(byte trackNo);		
	virtual bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[]);	
	virtual bool GetDiskInfo(byte& trkCnt, byte& sideCnt, char* comment = NULL);
	
	virtual bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff)
	{
		LastError = ERR_NOT_SUPPORTED;
		return false;
	}
	virtual bool FormatTrack(byte track, byte side)
	{
		LastError = ERR_NOT_SUPPORTED;
		return false;
	}

	~CDiskImgTD0()
	{
		if (imgBuff != NULL)
			delete imgBuff;

		if (fileTD0 != NULL)
			fclose(fileTD0);

		if (diskCmnt != NULL)
			delete diskCmnt;

		for (word idxTrk = 0; idxTrk < TDDisk[0].size(); idxTrk++)
		{
			if (TDDisk[0][idxTrk].sectorsBuf != NULL)
				delete TDDisk[0][idxTrk].sectorsBuf;
			
			if (TDDisk[1][idxTrk].sectorsBuf != NULL)
				delete TDDisk[1][idxTrk].sectorsBuf;
		}		
	}

#pragma pack(1)
	typedef struct
	{
		byte	TXT[2];
		byte	SeqVal;		// Volume sequence (zero for the first)
		byte	ChkSig;		// Check signature for multi-volume sets (all must match)
		byte	TDVer;		// Teledisk version used to create the file (11 = v1.1)
		byte	Dens;		// Source disk density (0 = 250K bps,  1 = 300K bps,  2 = 500K bps ; +128 = single-density FM)
		byte	DrvType;	// Source drive type (1 = 360K, 2 = 1.2M, 3 = 720K, 4 = 1.44M)
		byte	TrkDens;	// 0 = source matches media density, 1 = double density, 2 = quad density)
		byte	DosMode;	// Non-zero if disk was analysed according to DOS allocation
		byte	Surface;	// Disk sides stored in the image
		byte	CRC[2];		// 16-bit CRC for this header
	} TDHeader;

	// Optional comment block, present if bit 7 is set in bTrackDensity above
	typedef struct
	{
		byte CRC[2];					// 16-bit CRC covering the comment block
		word Len;					// Comment block length
		byte  bYear, bMon, bDay;		// Date of disk creation
		byte  bHour, bMin, bSec;		// Time of disk creation
		//  BYTE    abData[];				// Comment data, in null-terminated blocks
	} TDComment;

	typedef struct
	{
		byte SecPerTrk;			// Number of sectors in track
		byte PhysCyl;			// Physical track we read from
		byte PhysSide;			// Physical side we read from
		byte CRC;				// Low 8-bits of track header CRC
	} TDTrackHeader;

	typedef struct
	{
		byte Cyl;				// Track number in ID field
		byte Side;				// Side number in ID field
		byte SNum;				// Sector number in ID field
		byte SLen;				// Sector size indicator:  (128 << bSize) gives the real size
		byte Syndrome;			// Flags detailing special sector conditions
		byte CRC;				// Low 8-bits of sector header CRC
	} TDSectorHeader;

#pragma pack()

private:	
	static const char* TD0_SIG_NORMAL;    // Normal compression (RLE)
	static const char* TD0_SIG_ADVANCED;    // Huffman compression also used for everything after TD0_HEADER

	FILE* fileTD0;
	unsigned char crctable[32];
	unsigned char CRC16_High, CRC16_Low;
	byte* imgBuff;	
	bool HasAdvancedCompression;
	
	typedef struct
	{
		TDTrackHeader hdr;
		byte* sectorsBuf;
		vector<TDSectorHeader> sectors;
	} TDTrack;
	
	typedef vector<TDTrack> TDSide;
	TDSide TDDisk[2];

	int CDiskImgTD0::RLEExpander(unsigned char *src,unsigned char *dst,int blocklen);
};

#endif//CDISKTD0_H