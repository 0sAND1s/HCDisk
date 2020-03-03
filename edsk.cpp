#include "dsk.h"
#include "edsk.h"
#include "types.h"
#include <string.h>

char CEDSK::EDSK_SIGNATURE[35] = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";
char CEDSK::DSK_CREATOR_NAME[12] = "HC Disk 2.0";

CEDSK::CEDSK(): CDSK()
{
	sigMinValidLen = (byte)strlen("EXTENDED");
}

CEDSK::CEDSK(DiskDescType dskDesc): CDSK(dskDesc)
{	
	CEDSK();
	DiskDefinition = dskDesc;	
}


bool CEDSK::AddDiskHeader()
{
	DiskInfoBlockType dskInfo;
	memset(&dskInfo, 0, sizeof(dskInfo));

	fseek(dskFile, 0, SEEK_SET);	
	strcpy(dskInfo.signature, GetSignature());
	memcpy(dskInfo.creatorName, DSK_CREATOR_NAME, sizeof(DSK_CREATOR_NAME));
	dskInfo.trackNo = DiskDefinition.TrackCnt;
	dskInfo.sideNo = DiskDefinition.SideCnt;	

	for (int i = 0; i < DiskDefinition.TrackCnt * DiskDefinition.SideCnt; i++)
		//Track size includes the 0x100 bytes track header.
		dskInfo.trackSizeTbl[i] = (0x100 + DiskDefinition.SectSize * DiskDefinition.SPT)/0x100;

	if (fwrite(&dskInfo, sizeof(DiskInfoBlockType), 1, dskFile)  == 1)
		return true;
	else
	{
		LastError = ERR_WRITE;
		return false;
	}
}


bool CEDSK::Seek(byte track)
{
	unsigned trkOff = 0;
	for (int i = 0; i < track * diskInfoBlk.sideNo; i++)
		trkOff += (diskInfoBlk.trackSizeTbl[i] * 0x100);

	if (fseek(dskFile, sizeof(DiskInfoBlockType) + trkOff, SEEK_SET) == 0 &&	
		fread(&currTrack, sizeof(TrackInfoBlockType), 1, dskFile) == 1 &&
		strcmp(currTrack.signature, DSK_TRACK_SIGNATURE) == 0 &&
		fread(&currSectorInfo, sizeof(SectorInfoBlockType), currTrack.sectorCount, dskFile) == currTrack.sectorCount &&			
		//Seek to start of sector data
		fseek(dskFile,  0x100 - sizeof(TrackInfoBlockType) - sizeof(SectorInfoBlockType) * currTrack.sectorCount, SEEK_CUR) == 0)
		return true;
	else
	{
		LastError = ERR_SEEK;
		return false;
	}
}


bool CEDSK::AddSectorHeaders(byte track, byte side)
{
	for (byte sect = 0; sect < DiskDefinition.SPT; sect++)
	{
		SectorInfoBlockType sectInfo = {track, side, InterlaveTbl[sect], SectSize2SectCode(DiskDefinition.SectSize), 0, 0, {0, 0}};
		*(short*)sectInfo.sectorSize = DiskDefinition.SectSize;
		if (fwrite(&sectInfo, sizeof(sectInfo), 1, dskFile) != 1)
		{
			LastError = ERR_WRITE;
			return false;
		}
	}

	return true;
}

bool CEDSK::SeekSide(byte side)
{			
	if (fseek(dskFile, diskInfoBlk.trackSizeTbl[DiskDefinition.SideCnt * currTrack.trackNo + side] * 0x100 - 0x100, SEEK_CUR) == 0 &&
		fread(&currTrack, sizeof(TrackInfoBlockType), 1, dskFile) == 1 &&
		strcmp(currTrack.signature, DSK_TRACK_SIGNATURE) == 0 &&
		fread(&currSectorInfo, sizeof(SectorInfoBlockType), currTrack.sectorCount, dskFile) == currTrack.sectorCount &&
		fseek(dskFile,  0x100 - sizeof(TrackInfoBlockType) - sizeof(SectorInfoBlockType) * currTrack.sectorCount, SEEK_CUR) == 0)
		return true;
	else
	{
		LastError = ERR_SEEK;
		return false;
	}
}


