//GC: Code adapted from https://github.com/jfdelnero/libhxcfe/blob/master/sources/loaders/teledisk_loader/teledisk_loader.c .


#include "DiskImgTD0.h"
#include "CRC.h"
#include "td0_lzss.h"

const char* CDiskImgTD0::TD0_SIG_NORMAL = "TD";    // Normal compression (RLE)
const char* CDiskImgTD0::TD0_SIG_ADVANCED = "td";    // Huffman compression also used for everything after TD0_HEADER

bool CDiskImgTD0::Open(char* imgName, DiskOpenMode openMode)
{
	LastError = ERR_OPEN;

	if (openMode != OPEN_MODE_EXISTING)
		return false;

	fileTD0 = fopen(imgName, "rb");	
	if (fileTD0 == NULL)
		return false;

	fseek(fileTD0, 0, SEEK_END);
	dword filesize = ftell(fileTD0);
	fseek(fileTD0, 0, SEEK_SET);
	imgBuff = new byte[filesize + 512];
	if (fread(imgBuff, 1, filesize, fileTD0) != filesize)
	{
		delete imgBuff;
		imgBuff = NULL;
		return false;
	}

	TDHeader* hdrImg = (TDHeader*)imgBuff;

	CRC16_Init(&CRC16_High, &CRC16_Low, (byte*)crctable, 0xA097, 0x0000);
	byte* ptr = (byte*)hdrImg;
	for(byte i=0; i<0xA; i++)
	{
		CRC16_Update(&CRC16_High,&CRC16_Low, ptr[i], (byte*)crctable);
	}

	if(((hdrImg->CRC[1]<<8) | hdrImg->CRC[0]) != ((CRC16_High << 8) | CRC16_Low))
		return false;
	
	if (memcmp(&hdrImg->TXT, TD0_SIG_NORMAL, sizeof(hdrImg->TXT)) == 0 )
		HasAdvancedCompression = false;
	else if (memcmp(&hdrImg->TXT, TD0_SIG_ADVANCED, sizeof(hdrImg->TXT)) == 0)
		HasAdvancedCompression = true;
	else
	{
		LastError = ERR_OPEN;
		return false;
	}	
		
	if (HasAdvancedCompression)
	{
		imgBuff = unpack(imgBuff, filesize);
	}	

	hdrImg = (TDHeader*)imgBuff;

	if (hdrImg->TDVer > 21 || hdrImg->TDVer < 10)
	{
		LastError = ERR_NOT_SUPPORTED;
		return false;
	}	

	byte tempdata[8*1024];
	dword imgBuffIdx = sizeof(TDHeader);
	if(hdrImg->TrkDens & 0x80)
	{
		byte* ptr;				
		TDComment* tdCmnt;

		CRC16_Init(&CRC16_High, &CRC16_Low, (byte*)crctable, 0xA097, 0x0000);
		
		tdCmnt = (TDComment*)&imgBuff[imgBuffIdx];
		imgBuffIdx += sizeof(TDComment);
		
		ptr = (byte*)tdCmnt;
		ptr += 2;
		for(word i=0; i < sizeof(TDComment) - 2; i++)
		{
			CRC16_Update(&CRC16_High, &CRC16_Low, ptr[i], (byte*)crctable);
		}

		memcpy(&tempdata, &imgBuff[imgBuffIdx], tdCmnt->Len);
		imgBuffIdx += tdCmnt->Len;

		ptr = (byte*)&tempdata;
		for(word i=0; i<tdCmnt->Len; i++)
		{
			CRC16_Update(&CRC16_High, &CRC16_Low, ptr[i],(byte*)crctable);
		}

		diskCmnt = new char[tdCmnt->Len + 80];
		sprintf(diskCmnt, "Creation date: %.2d/%.2d/%.4d %.2d:%.2d:%.2d. Comment: \"%s\"",tdCmnt->bDay, tdCmnt->bMon+1, tdCmnt->bYear+1900, tdCmnt->bHour, tdCmnt->bMin, tdCmnt->bSec, tempdata);					
	}

	TDTrackHeader* hdrTrk = (TDTrackHeader*)&imgBuff[imgBuffIdx];
	imgBuffIdx += sizeof(TDTrackHeader);		
	this->DiskDefinition.SideCnt = hdrImg->Surface;
	byte trackCnt = 0, sidenumber = 0, sectCnt = 0;

	while (hdrTrk->SecPerTrk != 0xFF)
	{
		if(hdrTrk->PhysCyl > trackCnt)
		{			
			trackCnt = hdrTrk->PhysCyl;
		}

		if(hdrTrk->PhysSide&0x7F)
		{
			sidenumber=1;
		}
		else
		{
			sidenumber=0;
		}

		TDTrack trk;
		trk.hdr = *hdrTrk;						

		CRC16_Init(&CRC16_High,&CRC16_Low,(byte*)crctable, 0xA097, 0x0000);
		ptr = (byte*)hdrTrk;
		for(byte idxSect = 0; idxSect < 0x03; idxSect++)
		{
			CRC16_Update(&CRC16_High, &CRC16_Low, ptr[idxSect],(byte*)crctable );
		}	
		
		if (CRC16_Low != hdrTrk->CRC)
		{
			return false;
		}
		
		TDSectorHeader* hdrSect = (TDSectorHeader*)&imgBuff[imgBuffIdx];			
		word sectSize = SectCode2SectSize(hdrSect->SLen);		
		if (sectCnt < trk.hdr.SecPerTrk)
			sectCnt = trk.hdr.SecPerTrk;
		trk.sectorsBuf = new byte[sectCnt * sectSize];
		memset(trk.sectorsBuf, DiskDefinition.Filler, sectCnt * sectSize);

		for (byte idxSect=0; idxSect < hdrTrk->SecPerTrk; idxSect++ )
		{
			hdrSect = (TDSectorHeader*)&imgBuff[imgBuffIdx];			
			imgBuffIdx += sizeof(TDSectorHeader);			
			word sectSize = SectCode2SectSize(hdrSect->SLen);											
			InterlaveTbl[idxSect] = hdrSect->SNum;			

			if ((hdrSect->Syndrome & 0x30) == 0 && (hdrSect->SLen & 0xf8) == 0)
			{				
				word* datalen = (word*)&imgBuff[imgBuffIdx];								
				//int expLen = RLEExpander(&imgBuff[imgBuffIdx], &trk.sectorsBuf[idxSect * sectSize], (*datalen));
				int expLen = RLEExpander(&imgBuff[imgBuffIdx], &trk.sectorsBuf[(InterlaveTbl[idxSect] - InterlaveTbl[0]) * sectSize], (*datalen));
				imgBuffIdx += (*datalen) + 2;
			}
			else
			{
				memset(&trk.sectorsBuf[(InterlaveTbl[idxSect] - InterlaveTbl[0]) * sectSize], 0xE5, sectSize);
			}
			
			trk.sectors.push_back(*hdrSect);							
		}

		//Add fake sectors in case of missing sectors in image.
		while (trk.sectors.size() < sectCnt)
		{
			trk.sectors.push_back(*hdrSect);
		}

		TDDisk[sidenumber].push_back(trk);		
		
		hdrTrk = (TDTrackHeader*)&imgBuff[imgBuffIdx];
		imgBuffIdx += sizeof(TDTrackHeader);
	}

	//Use track 2 for geometry reporting, since system COBRA CP/M disks have 1 sector/track for the first 2 tracks on each side.
	const byte trackToProbe = 2;
	this->DiskDefinition.SPT = (byte)TDDisk[0][trackToProbe].sectors.size();
	this->DiskDefinition.SectSize = SectCode2SectSize(TDDisk[0][trackToProbe].sectors[0].SLen);
	this->DiskDefinition.TrackCnt = trackCnt + 1;				

	//Source disk density (0 = 250K bps,  1 = 300K bps,  2 = 500K bps ; +128 = single-density FM)
	switch(hdrImg->Dens)
	{
	case 0:
		DiskDefinition.Density = DISK_DATARATE_DD_5_25;
		break;
	case 1:
		DiskDefinition.Density = DISK_DATARATE_DD_3_5;
		break;
	case 2:
		DiskDefinition.Density = DISK_DATARATE_HD;
		break;
	default:
		DiskDefinition.Density = DISK_DATARATE_DD_3_5;
		break;
	}

	return true;
}


//The hardware interleave is handled in the image decompression.
bool CDiskImgTD0::ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt)
{				
	word idxDstBuf = 0;	
	for (byte idxSect = sector - 1; idxSect < sector - 1 + sectCnt; idxSect++)
	{
		if (idxSect < TDDisk[side][track].sectors.size())
		{
			DiskSectType st = SectCode2SectSize(TDDisk[side][track].sectors[idxSect].SLen);
			word idxSectBuf = idxSect * (word)st;

			memcpy(&buff[idxDstBuf], &TDDisk[side][track].sectorsBuf[idxSectBuf], (word)st);
			idxSectBuf += (word)st;
			idxDstBuf += (word)st;
		}		
	}

	return true;
}

bool CDiskImgTD0::Seek(byte trackNo)
{
	return trackNo <= DiskDefinition.TrackCnt;
}

bool CDiskImgTD0::GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[])
{	
	if (side < TDDisk->size() && track < TDDisk[side].size())
	{
		sectorCnt = (byte)TDDisk[track][side].sectors.size();
		sectors = new SectDescType[sectorCnt];
		
		for (vector<TDSectorHeader>::iterator it = TDDisk[track][side].sectors.begin(); it != TDDisk[track][side].sectors.end(); it++)
		{
			SectDescType sdt;
			sdt.head = it->Side;
			sdt.sectID = it->SNum;
			sdt.sectSizeCode = it->SLen;
			sdt.track = it->Cyl;

			sectors[it - TDDisk[track][side].sectors.begin()] = sdt;
		}
	}

	return true;
}

bool CDiskImgTD0::GetDiskInfo(byte& trkCnt, byte& sideCnt, char* comment)
{
	if (diskCmnt != NULL)
		comment = diskCmnt;

	sideCnt = (byte)TDDisk->size();
	trkCnt = (byte)TDDisk[0].size();

	return true;
}


int CDiskImgTD0::RLEExpander(byte *src, byte *dst, int blocklen)
{
	byte *s1,*s2,d1,d2;
	byte type;
	unsigned short len,len2,rlen;

	unsigned int uBlock;
	unsigned int uCount;

	s2=dst;

	len=(src[1]<<8)|src[0];
	type=src[2];
	len--;
	src=src+3;

	switch ( type )
	{
	case 0:
		{
			memcpy(dst,src,len);
			rlen=len;
			break;
		}

	case 1:
		{
			len=(src[1]<<8) | src[0];
			rlen=len<<1;
			d1=src[2];
			d2=src[3];
			while (len--)
			{
				*dst++=d1;
				*dst++=d2;
			}
			src=src+4;
			break;
		}

	case 2:
		{
			rlen=0;
			len2=len;
			s1=&src[0];
			s2=dst;
			do
			{
				if( !s1[0])
				{
					len2--;

					len=s1[1];
					s1=s1+2;
					len2--;

					memcpy(s2,s1,len);
					rlen=rlen+len;
					s2=s2+len;
					s1=s1+len;

					len2=len2-len;
				}
				else
				{
					uBlock = 1<<*s1;
					s1++;
					len2--;

					uCount = *s1;
					s1++;
					len2--;

					while(uCount)
					{
						memcpy(s2, s1, uBlock);
						rlen=rlen+uBlock;
						s2=s2+uBlock;
						uCount--;
					}

					s1=s1 + uBlock;
					len2=len2-uBlock;
				}

			}while(len2);
		}

	default:
		{
			rlen=-1;
		}
	}

	return rlen;
}
