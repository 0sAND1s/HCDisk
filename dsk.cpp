#include "dsk.h"
#include <string.h>

char CDSK::DSK_SIGNATURE[35] = "MV - CPCEMU Disk-File\r\nDisk-Info\r\n";
char CDSK::DSK_CREATOR_NAME[12] = "HC Disk 2.0";
char CDSK::DSK_TRACK_SIGNATURE[13] = "Track-Info\r\n";


CDSK::CDSK(): CDiskBase()
{
	dskFile = NULL;
	sigMinValidLen = (byte)strlen("MV - CPC");
	LastError = ERR_OK;	
	InterlaveTbl[0] = 0;
}

CDSK::CDSK(DiskDescType diskDesc): CDiskBase(diskDesc)
{
	CDSK();	
}


CDSK::~CDSK()
{
	if (dskFile != NULL)
		fclose(dskFile);
}

CDSK::CDSK(const CDSK& src): CDiskBase(src)
{	
}

bool CDSK::AddDiskHeader()
{
	DiskInfoBlockType dskInfo;

	fseek(dskFile, 0, SEEK_SET);	
	memcpy(dskInfo.signature, GetSignature(), sizeof(dskInfo.signature));
	memcpy(dskInfo.creatorName, DSK_CREATOR_NAME, sizeof(DSK_CREATOR_NAME));
	
	dskInfo.trackNo = DiskDefinition.TrackCnt;
	dskInfo.sideNo = DiskDefinition.SideCnt;
	//Track size includes the 0x100 bytes track header.	
	dskInfo.trackSize = 0x100 + DiskDefinition.SectSize * DiskDefinition.SPT;
	memset(dskInfo.trackSizeTbl, 0, sizeof(dskInfo.trackSizeTbl));

	if (fwrite(&dskInfo, sizeof(DiskInfoBlockType), 1, dskFile)  == 1)
		return true;
	else
	{
		LastError = ERR_WRITE;
		return false;
	}
}

bool CDSK::AddSectorHeaders(byte track, byte side)
{
	//Write sector headers.
	for (byte sect = 0; sect < DiskDefinition.SPT; sect++)
	{
		SectorInfoBlockType sectInfo = {track, side, InterlaveTbl[sect], CDiskBase::SectSize2SectCode(DiskDefinition.SectSize), 
			0, 0, {0, 0}};
		if (fwrite(&sectInfo, sizeof(sectInfo), 1, dskFile) != 1)
		{
			LastError = ERR_WRITE;
			return false;
		}
	}

	return true;
}

bool CDSK::CreateImage()
{
	AddDiskHeader();
	
	for (byte trk = 0; trk < DiskDefinition.TrackCnt; trk++)
	{
		for (byte side = 0; side < DiskDefinition.SideCnt; side++)
		{
			//Write track header.
			TrackInfoBlockType trkInfo = {"", {0, 0, 0}, trk, side, DATA_RATE_UNKNOWN, 0 , 
				SectSize2SectCode(DiskDefinition.SectSize), DiskDefinition.SPT, DiskDefinition.GapFmt, DiskDefinition.Filler};
			memcpy(trkInfo.signature, DSK_TRACK_SIGNATURE, sizeof(trkInfo.signature));
			if (fwrite(&trkInfo, sizeof(TrackInfoBlockType), 1, dskFile) != 1)
			{
				LastError = ERR_WRITE;
				return false;
			}

			if (!AddSectorHeaders(trk, side))			
				return false;			

			//Fill with 0's the rest of track header.
			for (byte idx = 0; idx < 0x100 - sizeof(TrackInfoBlockType) - sizeof(SectorInfoBlockType) * DiskDefinition.SPT; idx++)
			{
				if (fputc('\0', dskFile) == EOF)
				{
					LastError = ERR_WRITE;
					return false;
				}
			}


			//Fill with filler byte the sector data.
			for (int i = 0; i < DiskDefinition.SPT * DiskDefinition.SectSize; i++)
				if (fputc(DiskDefinition.Filler, dskFile) == EOF)
				{
					LastError = ERR_WRITE;
					return false;
				}
		}			
	}

	return true;
}


bool CDSK::Open(char * dskName, DiskOpenMode mode)
{
	strcpy(this->dskName, dskName);
	dskFile = fopen(dskName, (mode == OPEN_MODE_EXISTING ? "rb+" : "w+b"));	

	if (dskFile)
	{
		if (mode == OPEN_MODE_EXISTING)
		{			
			if ((fread(&diskInfoBlk, sizeof(diskInfoBlk), 1, dskFile) == 1) &&				
				(memcmp(GetSignature(), diskInfoBlk.signature, sigMinValidLen) == 0) &&
				Seek(0))
			{
				DiskDefinition.SideCnt = diskInfoBlk.sideNo;
				DiskDefinition.TrackCnt = diskInfoBlk.trackNo;				
				DiskDefinition.SPT = currTrack.sectorCount;
				DiskDefinition.SectSize = CDiskBase::SectCode2SectSize(currSectorInfo[0].sectorSizeCode);
				DiskDefinition.Filler = currTrack.fillerByte;
				DiskDefinition.GapFmt = currTrack.gap3;

				//Take interleave from firs track.
				for (byte secIdx = 0; secIdx < currTrack.sectorCount; secIdx++)
					InterlaveTbl[secIdx] = currSectorInfo[secIdx].sectorID;
				
				if (currTrack.dataRate == DATA_RATE_HIGH)
					DiskDefinition.Density = DISK_DATARATE_HD;
				else if (currTrack.dataRate == DATA_RATE_HIGH)
					DiskDefinition.Density = DISK_DATARATE_ED;
				else if (DiskDefinition.TrackCnt > 42)
					 DiskDefinition.Density= DISK_DATARATE_DD_3_5;
				else
					DiskDefinition.Density = DISK_DATARATE_DD_5_25;
				
				return true;
			}
			else
			{
				LastError = ERR_READ;
				return false;
			}
		}
		else
		{
			if (!CreateImage())
				return false;

			if (fseek(dskFile, 0, SEEK_SET) == 0 &&
				fread(&diskInfoBlk, sizeof(diskInfoBlk), 1, dskFile) == 1)
				return true;
			else
			{
				LastError = ERR_READ;
				return false;
			}
		}				
	}
	else
	{
		LastError = ERR_READ;
		return false;
	}
}


bool CDSK::Seek(byte trackNo)
{
	if (fseek(dskFile, sizeof(DiskInfoBlockType), SEEK_SET) == 0 &&	
		fseek(dskFile, 0x100 + diskInfoBlk.trackSize * trackNo * DiskDefinition.SideCnt, SEEK_SET) == 0 &&
		fread(&currTrack, sizeof(TrackInfoBlockType), 1, dskFile) == 1 &&
		memcmp(currTrack.signature, DSK_TRACK_SIGNATURE, sizeof(currTrack.signature)) == 0 &&
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


//Only called if the side is > 0.
bool CDSK::SeekSide(byte side)
{
	//Seek back to the track header.
	if (fseek(dskFile, side * diskInfoBlk.trackSize - 0x100, SEEK_CUR) == 0 &&
		fread(&currTrack, sizeof(TrackInfoBlockType), 1, dskFile) == 1 &&
		memcmp(currTrack.signature, DSK_TRACK_SIGNATURE, sizeof(currTrack.signature)) == 0 &&
		fread(&currSectorInfo, sizeof(SectorInfoBlockType), currTrack.sectorCount, dskFile) == currTrack.sectorCount &&
		//Seek to the beginning of track data.
		fseek(dskFile,  0x100 - sizeof(TrackInfoBlockType) - sizeof(SectorInfoBlockType) * currTrack.sectorCount, SEEK_CUR) == 0)
		return true;
	else
	{
		LastError = ERR_SEEK;
		return false;
	}
}


//This is required because some images don't have a valid currTrack.sectorSize and also, the track may have mixed sized sectors.
bool CDSK::SeekSector(byte sector)
{
	byte s = 1;	
	while (s < sector)
	{
		if (fseek(dskFile, CDiskBase::SectCode2SectSize((currSectorInfo[s-1].sectorSizeCode)), SEEK_CUR)  != 0 )
		{
			LastError = ERR_SEEK;
			return false;
		}
		s++;
	}

	return true;
}


bool CDSK::ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt)
{	
	if (sector > currTrack.sectorCount || (sectCnt - sector) > currTrack.sectorCount)
	{
		LastError = ERR_PARAM;
		return false;
	}

	if (!Seek(track))
		return false;

	if (side > 0 && !SeekSide(side))		
		return false;

	if (!SeekSector(sector))
		return false;
	
	static word cnt = 0;
	cnt++;

	word off = 0;
	for (byte s = 0; s < sectCnt; s++)
	{	
		word sectSize = CDiskBase::SectCode2SectSize(currSectorInfo[(sector - 1) + s].sectorSizeCode);		
		if (fread(buff + off, sectSize, 1, dskFile) != 1)
		{
			LastError = ERR_READ;
			return false;
		}
		off += sectSize;		
	}	

	return true;
}


bool CDSK::WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff)
{
	if (sector > currTrack.sectorCount || (sectCnt - sector) > currTrack.sectorCount)
	{
		LastError = ERR_PARAM;
		return false;
	}

	if (!Seek(track))
		return false;

	if (side > 0 && !SeekSide(side))		
		return false;

	if (!SeekSector(sector))
		return false;
	
	word off = 0;
	for (byte s = 0; s < sectCnt; s++)
	{		
		word sectSize = CDiskBase::SectCode2SectSize(currSectorInfo[(sector - 1) + s].sectorSizeCode);		
		if (fwrite(buff + off, sectSize, 1, dskFile) != 1)
		{
			LastError = ERR_WRITE;
			return false;
		}
		off += sectSize;
	}	

	return true;
}


bool CDSK::FormatTrack(byte track, byte side)
{
	if (!Seek(track))
		return false;

	if (side > 0 && !SeekSide(side))		
		return false;

	for (int s = 0; s < currTrack.sectorCount; s++)
		for (int b = 0; b < CDiskBase::SectCode2SectSize(currSectorInfo[s].sectorSizeCode); b++)
			if (fputc(currTrack.fillerByte, dskFile) == EOF)
			{
				LastError = ERR_WRITE;
				return false;
			}

	return true;
}


//This would return info for unusual formated tracks.
bool CDSK::GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectorsInfo[])
{
	if (track > DiskDefinition.TrackCnt || side > DiskDefinition.SideCnt)
	{
		LastError = ERR_PARAM;
		return false;
	}

	if (!Seek(track))
	{
		LastError = ERR_SEEK;
		return false;
	}

	if (side > 0 && !SeekSide(side))
	{
		LastError = ERR_SEEK;
		return false;
	}		

	sectorCnt = currTrack.sectorCount;

	for (int s = 0; s < sectorCnt; s++)
	{
		sectorsInfo[s].sectID = currSectorInfo[s].sectorID;
		sectorsInfo[s].sectSizeCode = currSectorInfo[s].sectorSizeCode;
		sectorsInfo[s].head = currSectorInfo[s].side;
		sectorsInfo[s].track = currSectorInfo[s].track;
	}

	return true;
}

bool CDSK::GetDiskInfo(byte & trackCount, byte & sideCount)
{
	if (Seek(40))
		trackCount = 40;
	else
		return false;

	if (Seek(80))
		trackCount = 80;

	if (SeekSide(1))
		sideCount = 2;
	else
		sideCount = 1;

	return true;
}