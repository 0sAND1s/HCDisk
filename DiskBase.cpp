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

CDiskBase::CDiskBase(DiskDescType dd): CDiskBase()
{ 					
	this->DiskDefinition = dd; 
	diskCmnt = NULL;
	memset(InterlaveTbl, 0, sizeof(InterlaveTbl));
	CreateInterleaveTable();
}	

CDiskBase::CDiskBase()
{	
	diskCmnt = NULL;
	this->DiskDefinition.Filler = 0xE5;
	this->DiskDefinition.GapFmt = 0x16;
	this->DiskDefinition.HWInterleave = 0;
	m_Skew = 0;
	//Initialize using consecutive sector IDs = no interleave.	
	CreateInterleaveTable();
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
		
	/*
	for (byte sectIdx = 1; sectIdx < DiskDefinition.SPT; sectIdx++)
	{									
		InterlaveTbl[sectIdx] = 
			((InterlaveTbl[sectIdx - 1] + DiskDefinition.HWInterleave) % DiskDefinition.SPT) + 1;		
	}
	*/
	//Interleave factor tells us how many sectors to skip: 1 = skip 1 sector = 1, x, 2, y, 3, etc.	
	int secIdx = 0;	
	int secID = 1;	
	int trkOffset = 0;
	for (int secCnt = 1; secCnt <= DiskDefinition.SPT; secCnt++)
	{		
		InterlaveTbl[trkOffset + secIdx] = secID;
		secIdx += (DiskDefinition.HWInterleave + 1);
		secIdx = secIdx % DiskDefinition.SPT;
		secID++;		
		//Restart on track index.
		trkOffset = (secCnt / (DiskDefinition.SPT / (DiskDefinition.HWInterleave + 1)));
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
			if (progCallback != nullptr)
				resOK = progCallback(track, DiskDefinition.TrackCnt-1, !resOK);

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
	
	//Get geometry from track 2, skip tracks 0,1, as these might be non-standard.
	if (ReadSectors(buf, 2, 0, 1, 1))
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
			if (progCallback != nullptr)
				progCallback(track, DiskDefinition.TrackCnt-1, !res);
		}

	return res;
}

byte CDiskBase::CalculateInterleaveFactor()
{		
	byte secIdx = 1;
	//Search sector index with first ID +1;
	while (secIdx < DiskDefinition.SPT && InterlaveTbl[secIdx] - InterlaveTbl[0] != 1)
		secIdx++;

	return secIdx - 1;
}