#ifndef _CWIN32DISK_H_
#define _CWIN32DISK_H_

#include <Windows.h>

#include "fdrawcmd.h"
#include "types.h"
#include "DiskBase.h"

class CDiskWin32: public CDiskBase
{
private:		
	bool CmdRead (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE count_, BYTE size_, PVOID pv_);
	bool CmdWrite (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE count_, BYTE size_, PVOID pv_);
	bool CmdVerify (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE end_, BYTE size_);
	bool CmdFormat (HANDLE h_, PFD_FORMAT_PARAMS pfp_, ULONG ulSize_);
	bool SetDataRate (HANDLE h_, BYTE bDataRate_);
	bool ResetDrive();
	HANDLE OpenDrive(int drive);

public:
	CDiskWin32(CDiskBase::DiskDescType diskDesc);
	CDiskWin32();
	~CDiskWin32();
	static DWORD GetDriverVersion();
	bool Open(char* drive, DiskOpenMode mode = OPEN_MODE_EXISTING);
	bool ReadSectors(byte *buf,byte track,byte head,byte sect,byte sectNO);
	bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff);
	bool Seek(byte track);
	bool FormatTrack(byte head, byte track);
	bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectorsInfo[]);
};

#endif//_CWIN32DISK_H_