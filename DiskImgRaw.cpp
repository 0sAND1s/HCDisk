#include "DiskImgRaw.h"


CDiskImgRaw::~CDiskImgRaw()
{
	if (fileImg != NULL)
		fclose(fileImg);
}

bool CDiskImgRaw::Open(char* imgName, DiskOpenMode openMode)
{
	fileImg = fopen(imgName, openMode == OPEN_MODE_EXISTING ? "rb+" : "wb+");
	if (fileImg != NULL)
	{
		LastError = ERR_OK;
		return true;
	}
	else
	{
		LastError = ERR_OPEN;
		return false;
	}
}

bool CDiskImgRaw::ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt)
{	
	bool res = Seek(track);	
	if (res)
		res = SeekSide(side);	
	if (res)
		fseek(fileImg, (sector - 1) * DiskDefinition.SectSize, SEEK_CUR);
	res = fread(buff, DiskDefinition.SectSize, sectCnt, fileImg) == sectCnt;		

	if (res)
	{
		LastError = ERR_OK;
		return true;
	}
	else
	{
		LastError = ERR_READ;
		return false;
	}
}

bool CDiskImgRaw::WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff)
{
	bool res = Seek(track);	
	if (res)
		res = SeekSide(side);	
	fseek(fileImg, (sector - 1) * DiskDefinition.SectSize, SEEK_CUR);
	res = fwrite(buff, DiskDefinition.SectSize, sectCnt, fileImg) == sectCnt;
	
	if (res)
	{
		LastError = ERR_OK;
		return true;	
	}
	else
	{
		LastError = ERR_WRITE;
		return false;
	}
}

bool CDiskImgRaw::FormatTrack(byte track, byte side)
{
	bool res = Seek(track);	
	if (res)
		res = SeekSide(side);	
	
	if (res)
	{
		for (word secIdx = 0; secIdx < DiskDefinition.SectSize * DiskDefinition.SPT && res; secIdx++)
			res = fputc(DiskDefinition.Filler, fileImg) != EOF;
	}
	
	if (res)
	{
		LastError = ERR_OK;
		return true;	
	}
	else
	{
		LastError = ERR_WRITE;
		return false;
	}
}

bool CDiskImgRaw::Seek(byte track)
{		
	bool res = fseek(fileImg, track * DiskDefinition.SideCnt * DiskDefinition.SPT * DiskDefinition.SectSize, SEEK_SET) == 0;
	
	if (res)
	{
		LastError = ERR_OK;
		return true;	
	}
	else
	{
		LastError = ERR_SEEK;
		return false;
	}	
}

bool CDiskImgRaw::SeekSide(byte side)
{	
	if (side > 0)
		return fseek(fileImg, (DiskDefinition.SideCnt - 1) * DiskDefinition.SPT * DiskDefinition.SectSize, SEEK_CUR) == 0;	
	else
		return true;
}
