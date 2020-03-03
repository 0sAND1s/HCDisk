/***************************************************************************
 *                                                                         *
 *    This program has been derived from LIBDSK 1.2.1                      *
 *    LIBDSK: General floppy and diskimage access library                  *
 *    Copyright (C) 2001-2,2005  John Elliott <jce@seasip.demon.co.uk>     *
 *                                                                         *
 *    This library is free software; you can redistribute it and/or        *
 *    modify it under the terms of the GNU Library General Public          *
 *    License as published by the Free Software Foundation; either         *
 *    version 2 of the License, or (at your option) any later version.     *
 *                                                                         *
 *    This library is distributed in the hope that it will be useful,      *
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    *
 *    Library General Public License for more details.                     *
 *                                                                         *
 *    You should have received a copy of the GNU Library General Public    *
 *    License along with this library; if not, write to the Free           *
 *    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,      *
 *    MA 02111-1307, USA                                                   *
 *                                                                         *
 ***************************************************************************/

/*
 *  This driver is for CopyQM images
 *  Written by Per Ola Ingvarsson.
 *  Thanks to Roger Plant for the expandqm program which has helped a lot.
 *  Currently it is read only
 */

//Modified by George Chirtoaca for HCDisk 2.0.

#ifndef CDISKCQM_H
#define CDISKCQM_H

#include <stdio.h>
#include <stdlib.h>
#include "DiskBase.h"

class CDiskImgCQM: public CDiskBase
{
public:
	CDiskImgCQM()
	{
		qm_self = &qm;
	}

	virtual bool Open(char* imgName, DiskOpenMode openMode = OPEN_MODE_EXISTING);
	virtual bool ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt);	
	virtual bool Seek(byte trackNo);		
	virtual bool GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[]);	
	virtual bool GetDiskInfo(byte& trkCnt, byte& sideCnt);
	
	virtual bool WriteSectors(byte track, byte side, byte sector, byte sectCnt, byte * buff)
	{
		LastError = ERR_NOT_SUPPORTED;
		return false;
	}
	virtual bool FormatTrack(byte track, byte side)
	{
		LastError = ERR_NOT_SUPPORTED;
		return false;
	}

private:
	static const unsigned long crc32r_table[256];
	typedef struct
	{
		void*         qm_super;
		char*         qm_filename;
		int           qm_h_sector_size;
		/* Number of total sectors. Not valid if blind */
		int           qm_h_nbr_sectors;
		int           qm_h_nbr_sec_per_track;
		int           qm_h_nbr_heads;
		int           qm_h_comment_len;
		/* Density - 1 means HD, 2 means QD */
		int           qm_h_density;
		/* Blind transfer or not. */
		int           qm_h_blind;
		int           qm_h_used_tracks;
		int           qm_h_total_tracks;
		/* Interleave */
		int           qm_h_interleave;
		/* Skew. Negative number for skew between sides */
		int           qm_h_skew;
		/* Sector number base. */
		signed char   qm_h_secbase;
		/* The crc read from the header */
		unsigned long qm_h_crc;
		/* The crc calculated while the image is read */
		unsigned long qm_calc_crc;
		unsigned int  qm_image_offset;
		unsigned char* qm_image;
		/* Fake sector for READ ID command */
		unsigned int  qm_sector;
	} QM_DSK_DRIVER;

	QM_DSK_DRIVER qm;
	QM_DSK_DRIVER* qm_self;

	int get_i16( char* buf, int pos );
	unsigned int get_u16( char* buf, int pos );
	unsigned long get_u32( char* buf, int pos );
	void drv_qm_update_crc( unsigned long* crc, unsigned char byte );
	int drv_qm_load_image( QM_DSK_DRIVER* qm_self, FILE* fp );
	int drv_qm_load_header( QM_DSK_DRIVER* qm_self, char* header );
	int drv_qm_get_read_ptr(QM_DSK_DRIVER *qm_self, unsigned char **buf_ptr, int cylinder, int head, int sector);
	void drv_qm_dump_sector_hex(unsigned char* buf,int sz);
	int qmOpen(char* imgName);
};

#endif//CDISKCQM_H