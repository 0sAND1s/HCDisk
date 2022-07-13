//Raw disk image support

#ifndef _DISK_IMG_RAW
#define _DISK_IMG_RAW

#include <stdio.h>
#include "DiskBase.h"

class CDiskImgRaw: public CDiskBase
{
public:	
	CDiskImgRaw(DiskDescType diskDesc): CDiskBase(diskDesc)
	{	
		this->DiskDefinition = diskDesc;		
	}

	CDiskImgRaw(const CDiskImgRaw& src)
	{
		this->DiskDefinition = src.DiskDefinition;
	}

	~CDiskImgRaw();
		
	virtual bool Open(char* imgName, DiskOpenMode openMode = OPEN_MODE_EXISTING);
	virtual bool ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt);
	virtual bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff);
	virtual bool FormatTrack(byte track, byte side);
	virtual bool Seek(byte trackNo);	
	virtual bool SeekSide(byte side);
	virtual bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[]) 
	{
		sectorCnt = DiskDefinition.SPT;
		for (int secIdx = 0; secIdx < sectorCnt; secIdx++)
		{
			sectors[secIdx].head = side;
			sectors[secIdx].sectID = secIdx + 1;
			sectors[secIdx].track = track;
			sectors[secIdx].sectSizeCode = SectSize2SectCode(DiskDefinition.SectSize);
		}
		return true;
	}

protected:	
	FILE* fileImg;
};
#endif//_DISK_IMG_RAW