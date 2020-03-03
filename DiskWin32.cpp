#include <windows.h>
#include "DiskWin32.h"

DWORD dwRet;
HANDLE h = INVALID_HANDLE_VALUE;
#define TRACK_SKEW 0

dword CDiskWin32::GetDriverVersion ()
{
	dword dwVersion = 0;
	HANDLE h = CreateFile("\\\\.\\fdrawcmd", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (h != INVALID_HANDLE_VALUE)
	{
		DeviceIoControl(h, IOCTL_FDRAWCMD_GET_VERSION, NULL, 0, &dwVersion, sizeof(dwVersion), &dwRet, NULL);
		CloseHandle(h);
	}

	return dwVersion;
}


bool CDiskWin32::CmdRead (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE count_, BYTE size_, PVOID pv_)
{
	FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, head_, cyl_,head_, start_,size_,start_+count_, 0x0a,0xff };
	return !!DeviceIoControl(h_, IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp), pv_, count_*(128<<rwp.size), &dwRet, NULL);
}


bool CDiskWin32::CmdWrite (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE count_, BYTE size_, PVOID pv_)
{
	FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, head_, cyl_,head_, start_,size_,start_+count_, 0x0a,0xff };
	return !!DeviceIoControl(h_, IOCTL_FDCMD_WRITE_DATA, &rwp, sizeof(rwp), pv_, count_*(128<<rwp.size), &dwRet, NULL);
}


bool CDiskWin32::CmdVerify (HANDLE h_, BYTE cyl_, BYTE head_, BYTE start_, BYTE end_, BYTE size_)
{
	FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, head_, cyl_,head_, start_,size_,end_, 0x0a,0xff };
	return !!DeviceIoControl(h_, IOCTL_FDCMD_VERIFY, &rwp, sizeof(rwp), NULL, 0, &dwRet, NULL);
}


bool CDiskWin32::CmdFormat (HANDLE h_, PFD_FORMAT_PARAMS pfp_, ULONG ulSize_)
{
	return !!DeviceIoControl(h_, IOCTL_FDCMD_FORMAT_TRACK, pfp_, ulSize_, NULL, 0, &dwRet, NULL);
}


bool CDiskWin32::SetDataRate (HANDLE h_, BYTE bDataRate_)
{
	return !!DeviceIoControl(h_, IOCTL_FD_SET_DATA_RATE, &bDataRate_, sizeof(bDataRate_), NULL, 0, &dwRet, NULL);
}

bool CDiskWin32::Seek(byte track)
{
	return !!DeviceIoControl(h, IOCTL_FDCMD_SEEK, &track, sizeof(track), NULL, 0, &dwRet, NULL);
}


CDiskWin32::CDiskWin32(CDiskBase::DiskDescType diskDesc): CDiskBase(diskDesc)
{
	this->DiskDefinition = diskDesc;
}

CDiskWin32::CDiskWin32(): CDiskBase()
{

}

CDiskWin32::~CDiskWin32()
{
	ResetDrive() && CloseHandle(h);
	h = INVALID_HANDLE_VALUE;
}


bool CDiskWin32::ReadSectors(byte *buf,byte track,byte head,byte sect,byte sectNO)
{
	bool OK = false;
	byte RetryCnt = DEF_RETRY_CNT;
	if (Seek(track))
		while (RetryCnt-- && !(OK = CmdRead(h, track, head, sect, sectNO, CDiskBase::SectSize2SectCode(DiskDefinition.SectSize), buf)));

	return OK;
}

bool CDiskWin32::WriteSectors(byte track, byte side, byte sect, byte sectCnt, byte * buf)
{
	bool OK = false;
	byte RetryCnt = DEF_RETRY_CNT;

	if (Seek(track))
		while (RetryCnt-- && !(OK = CmdWrite(h, track, side, sect, sectCnt, CDiskBase::SectSize2SectCode(DiskDefinition.SectSize), buf)));

	return OK;
}

bool CDiskWin32::Open(char* drive, DiskOpenMode mode)
{	
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);

	dword dwVersion = GetDriverVersion();

	if (!dwVersion)
		return false;	
	else if (HIWORD(dwVersion) != HIWORD(FDRAWCMD_VERSION))	
		return false;	
	else
	{	
		h = OpenDrive('A' - toupper(drive[0]));		
		return ResetDrive();
	}	
}

HANDLE CDiskWin32::OpenDrive(int nDrive)
{
	char szDevice[32];
	wsprintf(szDevice, "\\\\.\\fdraw%u", nDrive);

	h = CreateFile(szDevice, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	return h;
}

bool CDiskWin32::ResetDrive()
{	
	return !!DeviceIoControl(h, IOCTL_FD_RESET, NULL, 0, NULL, 0, &dwRet, NULL);
}

bool CDiskWin32::FormatTrack(byte track, byte side)
{
	if (!Seek(track))
		return false;

	BYTE abFormat[sizeof(FD_FORMAT_PARAMS) + sizeof(FD_ID_HEADER)*MAX_SECT_PER_TRACK];

	PFD_FORMAT_PARAMS pfp = (PFD_FORMAT_PARAMS)abFormat;
	pfp->flags = FD_OPTION_MFM;
	pfp->phead = side;
	pfp->size = CDiskBase::SectSize2SectCode(DiskDefinition.SectSize);
	pfp->sectors = DiskDefinition.SPT;
	pfp->gap = DiskDefinition.GapFmt;
	pfp->fill = DiskDefinition.Filler;

	PFD_ID_HEADER ph = pfp->Header;

	for (BYTE s = 1 ; s <= pfp->sectors ; s++, ph++)
	{
		ph->cyl = track;
		ph->head = side;
		//ph->sector = ((s + track*(pfp->sectors - TRACK_SKEW)) % pfp->sectors);
		ph->sector = s;
		ph->size = pfp->size;
	}

	return CmdFormat(h, pfp, (ULONG)((PBYTE)ph - abFormat));	
}

bool CDiskWin32::GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectorsInfo[])
{
	dword dwRet;
	bool OK = false;

	// Allow space for 32 headers
	byte buf[1+4*MAX_SECT_PER_TRACK];
	PFD_SCAN_RESULT psr = (PFD_SCAN_RESULT)buf;

	// set up scan parameters
	FD_SCAN_PARAMS sp;
	sp.flags = FD_OPTION_MFM;   
	sp.head = side;

	Seek(track);

	// seek and scan track
	for (int dr = DISK_DATARATE_HD; dr <= DISK_DATARATE_ED && !OK; dr++)
	{
		SetDataRate(h, (byte)dr);
		if (!DeviceIoControl(h, IOCTL_FD_SCAN_TRACK, &sp, sizeof(sp), psr, sizeof(buf), &dwRet, NULL))
		{
			sectorCnt = 0;
			OK = false;
		}
		else
		{
			sectorCnt = psr->count;
			OK = sectorCnt > 0;

			for (int s = 0; s < sectorCnt; s++)
			{
				sectorsInfo[s].sectID = psr->Header[s].sector;
				sectorsInfo[s].sectSizeCode = psr->Header[s].size;
				sectorsInfo[s].head = psr->Header[s].head;
				sectorsInfo[s].track = psr->Header[s].cyl;
			}		
		}		
	}   

	return OK;
}

