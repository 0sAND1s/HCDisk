#include <stdio.h>
#include <memory.h>
#include "DiskBase.h"

char* CDiskBase::ERROR_TYPE_MSG[] = 
{
	"",
	"Not supported",
	"Open",
	"Read",
	"Write",
	"Seek",
	"Invalid parameter"
};

CDiskBase::CDiskBase(DiskDescType dd)
{ 					
	this->DiskDefinition = dd; 
	diskCmnt = NULL;
	CreateInterleaveTable();
}	

CDiskBase::CDiskBase()
{
	//Initialize using consecutive sector IDs = no interleave.	
	CreateInterleaveTable();
	diskCmnt = NULL;
}

CDiskBase::CDiskBase(byte tracks, byte spt, byte sides, DiskSectType st, DiskDensType dens, byte filler, byte gap)
{
	this->DiskDefinition.TrackCnt = tracks;
	this->DiskDefinition.SPT = spt;
	this->DiskDefinition.SideCnt = sides;	
	this->DiskDefinition.SectSize = st;
	this->DiskDefinition.Density = dens;
	this->DiskDefinition.Filler = filler;
	this->DiskDefinition.GapFmt = gap;		
	this->DiskDefinition.HWInterleave = 0;

	diskCmnt = NULL;
}

CDiskBase::CDiskBase(const CDiskBase& src)
{
	memcpy(InterlaveTbl, src.InterlaveTbl, sizeof(InterlaveTbl));
	DiskDefinition = src.DiskDefinition;
	diskCmnt = NULL;
}

CDiskBase::~CDiskBase()
{	
}

bool CDiskBase::CreateInterleaveTable()
{
	if (DiskDefinition.HWInterleave > DiskDefinition.SPT|| DiskDefinition.SPT > MAX_SECT_PER_TRACK)
		return false;
	
	InterlaveTbl[0] = 1;
	for (byte sectIdx = 1; sectIdx < DiskDefinition.SPT; sectIdx++)
	{									
		InterlaveTbl[sectIdx] = 
			((InterlaveTbl[sectIdx - 1] + DiskDefinition.HWInterleave) % DiskDefinition.SPT) + 1;		
	}

	return true;
}

bool CDiskBase::ReadTrack(byte* buff, byte track, byte head)
{					
	return ReadSectors(buff, track, head, 1, DiskDefinition.SPT);
}

bool CDiskBase::WriteTrack(byte track, byte head, byte* buff)
{
	return WriteSectors(track, head, 1, DiskDefinition.SPT, buff);
}

bool CDiskBase::CopyTo(CDiskBase* dest, bool formatDst)
{
	bool resOK = true;
	byte* trkBuf = (byte*)malloc(DiskDefinition.SPT * DiskDefinition.SectSize + 512 * 18);

	for (byte track = 0; track < DiskDefinition.TrackCnt && resOK; track++)
		for (byte head = 0; head < DiskDefinition.SideCnt && resOK; head++)
		{
			resOK = ReadTrack(trkBuf, track, head);
			if (resOK && formatDst)
				resOK = dest->FormatTrack(track, head);
			if (resOK)
				resOK = dest->WriteTrack(track, head, trkBuf);
		}

	free(trkBuf);

	return resOK;
}

bool CDiskBase::GetDiskInfo(byte & trackCount, byte & sideCount, char* comment)
{
	byte buf[1024];

	if (Seek(79))
	{
		trackCount = 80;
	}
	else if (Seek(39))
	{
		trackCount = 40;		
	}	
	else
		return false;
	

	if (ReadSectors(buf, 0, 1, 1, 1))
		sideCount = 2;
	else
		sideCount = 1;

	return true;
}

bool CDiskBase::DetectDiskGeometry(DiskDescType& dd)
{	
	SectDescType sd[MAX_SECT_PER_TRACK];
	bool res = GetTrackInfo(0, 0, dd.SPT, sd);	

	if (res)
	{		
		dd.SectSize = SectCode2SectSize(sd[0].sectSizeCode);		
		//For this call to succeed, it must have the sector size filled in.
		res = GetDiskInfo(dd.TrackCnt, dd.SideCnt, nullptr);
	}			

	return res;		
}

bool CDiskBase::FormatDisk()
{
	bool res = true;

	for (byte track = 0; track < DiskDefinition.TrackCnt && res; track++)
		for (byte head = 0; head < DiskDefinition.SideCnt && res; head++)
		{
			res = this->FormatTrack(track, head);		
		}

	return res;
}