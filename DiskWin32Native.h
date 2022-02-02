#pragma once

#include <Windows.h>

#include "fdrawcmd.h"
#include "types.h"
#include "DiskBase.h"

class CDiskWin32Native : public CDiskBase
{
private:	
	HANDLE hVol = INVALID_HANDLE_VALUE;
	typedef struct
	{
		DiskDescType dd;
		MEDIA_TYPE mt;
	} WinGeometryMedia;
	const static WinGeometryMedia WindowsSupportedGeometries[];
	MEDIA_TYPE Geometry2MediaType(DiskDescType dd);	

public:	
	CDiskWin32Native();
	CDiskWin32Native(DiskDescType dd);
	~CDiskWin32Native();	
	bool Open(char* drive, DiskOpenMode mode = OPEN_MODE_EXISTING);
	bool ReadSectors(byte* buf, byte track, byte head, byte sect, byte sectNO);
	bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte* buff);
	bool Seek(byte track);	
	bool GetDiskInfo(byte& trkCnt, byte& sideCnt, char* comment = NULL);
	bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectorsInfo[]);
	bool FormatTrack(byte track, byte side);

	static bool IsUSBVolume(char* drive);
	static bool IsFloppyDrive(char* path);
};