#include "DiskImgCQM.h"

//#define DRV_QM_DEBUG 1

#define QM_HEADER_SIZE 133
#define DSK_ERR_OK	 (0)	/* No error */
#define DSK_ERR_BADPTR	 (-1)	/* Bad pointer */
#define DSK_ERR_NOTME    (-5)	/* Driver rejects disc */
#define DSK_ERR_NOMEM  	 (-7)	/* Null return from malloc */

const unsigned long CDiskImgCQM::crc32r_table[256] = {
	0x00000000UL,0x77073096UL,0xEE0E612CUL,0x990951BAUL, 0x076DC419UL,0x706AF48FUL,0xE963A535UL,0x9E6495A3UL, 0x0EDB8832UL,0x79DCB8A4UL,0xE0D5E91EUL,0x97D2D988UL,
	0x09B64C2BUL,0x7EB17CBDUL,0xE7B82D07UL,0x90BF1D91UL, 0x1DB71064UL,0x6AB020F2UL,0xF3B97148UL,0x84BE41DEUL, 0x1ADAD47DUL,0x6DDDE4EBUL,0xF4D4B551UL,0x83D385C7UL,
	0x136C9856UL,0x646BA8C0UL,0xFD62F97AUL,0x8A65C9ECUL, 0x14015C4FUL,0x63066CD9UL,0xFA0F3D63UL,0x8D080DF5UL, 0x3B6E20C8UL,0x4C69105EUL,0xD56041E4UL,0xA2677172UL,
	0x3C03E4D1UL,0x4B04D447UL,0xD20D85FDUL,0xA50AB56BUL, 0x35B5A8FAUL,0x42B2986CUL,0xDBBBC9D6UL,0xACBCF940UL, 0x32D86CE3UL,0x45DF5C75UL,0xDCD60DCFUL,0xABD13D59UL,
	0x26D930ACUL,0x51DE003AUL,0xC8D75180UL,0xBFD06116UL, 0x21B4F4B5UL,0x56B3C423UL,0xCFBA9599UL,0xB8BDA50FUL, 0x2802B89EUL,0x5F058808UL,0xC60CD9B2UL,0xB10BE924UL,
	0x2F6F7C87UL,0x58684C11UL,0xC1611DABUL,0xB6662D3DUL, 0x76DC4190UL,0x01DB7106UL,0x98D220BCUL,0xEFD5102AUL, 0x71B18589UL,0x06B6B51FUL,0x9FBFE4A5UL,0xE8B8D433UL,
	0x7807C9A2UL,0x0F00F934UL,0x9609A88EUL,0xE10E9818UL, 0x7F6A0DBBUL,0x086D3D2DUL,0x91646C97UL,0xE6635C01UL, 0x6B6B51F4UL,0x1C6C6162UL,0x856530D8UL,0xF262004EUL,
	0x6C0695EDUL,0x1B01A57BUL,0x8208F4C1UL,0xF50FC457UL, 0x65B0D9C6UL,0x12B7E950UL,0x8BBEB8EAUL,0xFCB9887CUL, 0x62DD1DDFUL,0x15DA2D49UL,0x8CD37CF3UL,0xFBD44C65UL,
	0x4DB26158UL,0x3AB551CEUL,0xA3BC0074UL,0xD4BB30E2UL, 0x4ADFA541UL,0x3DD895D7UL,0xA4D1C46DUL,0xD3D6F4FBUL, 0x4369E96AUL,0x346ED9FCUL,0xAD678846UL,0xDA60B8D0UL,
	0x44042D73UL,0x33031DE5UL,0xAA0A4C5FUL,0xDD0D7CC9UL, 0x5005713CUL,0x270241AAUL,0xBE0B1010UL,0xC90C2086UL, 0x5768B525UL,0x206F85B3UL,0xB966D409UL,0xCE61E49FUL,
	0x5EDEF90EUL,0x29D9C998UL,0xB0D09822UL,0xC7D7A8B4UL, 0x59B33D17UL,0x2EB40D81UL,0xB7BD5C3BUL,0xC0BA6CADUL, 0xEDB88320UL,0x9ABFB3B6UL,0x03B6E20CUL,0x74B1D29AUL,
	0xEAD54739UL,0x9DD277AFUL,0x04DB2615UL,0x73DC1683UL, 0xE3630B12UL,0x94643B84UL,0x0D6D6A3EUL,0x7A6A5AA8UL, 0xE40ECF0BUL,0x9309FF9DUL,0x0A00AE27UL,0x7D079EB1UL,
	0xF00F9344UL,0x8708A3D2UL,0x1E01F268UL,0x6906C2FEUL, 0xF762575DUL,0x806567CBUL,0x196C3671UL,0x6E6B06E7UL, 0xFED41B76UL,0x89D32BE0UL,0x10DA7A5AUL,0x67DD4ACCUL,
	0xF9B9DF6FUL,0x8EBEEFF9UL,0x17B7BE43UL,0x60B08ED5UL, 0xD6D6A3E8UL,0xA1D1937EUL,0x38D8C2C4UL,0x4FDFF252UL, 0xD1BB67F1UL,0xA6BC5767UL,0x3FB506DDUL,0x48B2364BUL,
	0xD80D2BDAUL,0xAF0A1B4CUL,0x36034AF6UL,0x41047A60UL, 0xDF60EFC3UL,0xA867DF55UL,0x316E8EEFUL,0x4669BE79UL, 0xCB61B38CUL,0xBC66831AUL,0x256FD2A0UL,0x5268E236UL,
	0xCC0C7795UL,0xBB0B4703UL,0x220216B9UL,0x5505262FUL, 0xC5BA3BBEUL,0xB2BD0B28UL,0x2BB45A92UL,0x5CB36A04UL, 0xC2D7FFA7UL,0xB5D0CF31UL,0x2CD99E8BUL,0x5BDEAE1DUL,
	0x9B64C2B0UL,0xEC63F226UL,0x756AA39CUL,0x026D930AUL, 0x9C0906A9UL,0xEB0E363FUL,0x72076785UL,0x05005713UL, 0x95BF4A82UL,0xE2B87A14UL,0x7BB12BAEUL,0x0CB61B38UL,
	0x92D28E9BUL,0xE5D5BE0DUL,0x7CDCEFB7UL,0x0BDBDF21UL, 0x86D3D2D4UL,0xF1D4E242UL,0x68DDB3F8UL,0x1FDA836EUL, 0x81BE16CDUL,0xF6B9265BUL,0x6FB077E1UL,0x18B74777UL,
	0x88085AE6UL,0xFF0F6A70UL,0x66063BCAUL,0x11010B5CUL, 0x8F659EFFUL,0xF862AE69UL,0x616BFFD3UL,0x166CCF45UL, 0xA00AE278UL,0xD70DD2EEUL,0x4E048354UL,0x3903B3C2UL,
	0xA7672661UL,0xD06016F7UL,0x4969474DUL,0x3E6E77DBUL, 0xAED16A4AUL,0xD9D65ADCUL,0x40DF0B66UL,0x37D83BF0UL, 0xA9BCAE53UL,0xDEBB9EC5UL,0x47B2CF7FUL,0x30B5FFE9UL,
	0xBDBDF21CUL,0xCABAC28AUL,0x53B39330UL,0x24B4A3A6UL, 0xBAD03605UL,0xCDD70693UL,0x54DE5729UL,0x23D967BFUL, 0xB3667A2EUL,0xC4614AB8UL,0x5D681B02UL,0x2A6F2B94UL,
	0xB40BBE37UL,0xC30C8EA1UL,0x5A05DF1BUL,0x2D02EF8DUL,
};

bool CDiskImgCQM::Open(char* imgName, DiskOpenMode openMode)
{
	if (openMode != OPEN_MODE_EXISTING)
		return false;

	if (qmOpen(imgName) != 0)
		return false;

	if (qm_self->qm_h_density == -1)
		DiskDefinition.Density = CDiskBase::DISK_DATARATE_HD;
	else
		DiskDefinition.Density = CDiskBase::DISK_DATARATE_DD_3_5;

	DiskDefinition.SideCnt = qm_self->qm_h_nbr_heads;
	DiskDefinition.SectSize = (CDiskBase::DiskSectType)qm_self->qm_h_sector_size;
	DiskDefinition.SPT = qm_self->qm_h_nbr_sec_per_track;
	DiskDefinition.TrackCnt = qm_self->qm_h_total_tracks;
	DiskDefinition.Filler = 0xE5;

	return true;
}

bool CDiskImgCQM::ReadSectors(byte * buff, byte track, byte side, byte sector, byte sectCnt)
{
	if (track * side <= qm_self->qm_h_total_tracks * qm_self->qm_h_nbr_heads && 
		sector + sectCnt - 1 <= qm_self->qm_h_nbr_sec_per_track)
	{
		word sectSize = qm_self->qm_h_sector_size;
		dword trkSize = sectSize * qm_self->qm_h_nbr_sec_per_track;
		dword off = (track * qm_self->qm_h_nbr_heads + side) * trkSize + (sector - 1) * sectSize;
		memcpy(buff, qm_self->qm_image + off, sectCnt * sectSize);

		return true;
	}
	else	
		return false;
}

bool CDiskImgCQM::Seek(byte trackNo)
{
	return trackNo <= DiskDefinition.TrackCnt;
}

bool CDiskImgCQM::GetTrackInfo(byte track, byte side, byte& sectorCnt, SectDescType sectors[])
{
	sectorCnt = qm_self->qm_h_nbr_sec_per_track;
	for (byte sectIdx = 0; sectIdx < sectorCnt; sectIdx++)
	{
		sectors[sectIdx].head = side;
		sectors[sectIdx].track = track;
		sectors[sectIdx].sectSizeCode = CDiskBase::SectSize2SectCode((CDiskBase::DiskSectType)qm_self->qm_h_sector_size);
		sectors[sectIdx].sectID = sectIdx + qm_self->qm_h_secbase + 1;
	}

	return true;
}

bool CDiskImgCQM::GetDiskInfo(byte& trkCnt, byte& sideCnt)
{
	sideCnt = qm_self->qm_h_nbr_heads;
	trkCnt = qm_self->qm_h_total_tracks;

	return true;
}

int CDiskImgCQM::get_i16( char* buf, int pos )
{
    unsigned char low_byte;
    unsigned char high_byte;
    unsigned int outInt;
    
    low_byte = buf[pos++];
    high_byte = buf[pos];
    
    /* Signed, eh. Lets see. */
    outInt = 0;
    if ( (signed char)high_byte < 0 ) {
        /* Set all to ones except for the lower 16 */
        /* Should work if sizeof(int) >= 16 */
        outInt = (-1) << 16;
    }

    outInt |= (high_byte << 8 ) | low_byte;
    
    return outInt;
}

unsigned int CDiskImgCQM::get_u16( char* buf, int pos )
{
    return ((unsigned int)get_i16( buf, pos )) & 0xffff;
}

unsigned long CDiskImgCQM::get_u32( char* buf, int pos )
{
    int i;
    unsigned long ret_val = 0;
    for ( i = 3; i >= 0; --i ) {
        ret_val <<= 8;
        ret_val |= ( (unsigned long)buf[pos+i] & 0xff );
    }
    return ret_val;
}

void CDiskImgCQM::drv_qm_update_crc( unsigned long* crc, unsigned char byte )
{
    /* Note that there is a bug in the CopyQM CRC calculation  */
    /* When indexing in this table, they shift the crc ^ data  */
    /* 2 bits up to address longwords, but they do that in an  */
    /* eight bit register, so that the top 2 bits are lost,    */
    /* thus the anding with 0x3f                               */
    *crc = crc32r_table[(byte ^ (unsigned char)*crc) & 0x3f ] ^ (*crc >> 8);
}

int CDiskImgCQM::drv_qm_load_image( QM_DSK_DRIVER* qm_self, FILE* fp )
{
    unsigned char* image = 0;
    int errcond = DSK_ERR_OK;
    /* Write position in the memory image */
    size_t curwritepos = 0;
    /* FIXME: Use the used tracks instead of the total tracks to detect */
    /*        that there is the correct amount of data in the image     */
    size_t image_size = (size_t)qm_self->qm_h_nbr_sec_per_track *
        (size_t)qm_self->qm_h_nbr_heads *
        (size_t)qm_self->qm_h_total_tracks * (size_t)qm_self->qm_h_sector_size;
    /* Set the position after the header and comment */
    int res = fseek( fp, QM_HEADER_SIZE + qm_self->qm_h_comment_len, SEEK_SET );
    if ( res < 0 ) return DSK_ERR_NOTME;

    /* Alloc memory for the image */
    qm_self->qm_image = (unsigned char*)malloc( image_size );
    if ( ! qm_self->qm_image ) return DSK_ERR_NOMEM;
    image = qm_self->qm_image;
    /* Start reading */
    /* Note that it seems like each track starts a new block */
    while ( curwritepos < image_size ) {
        /* Read the length */
        char lengthBuf[2];
        res = fread( lengthBuf, 2, 1, fp );
        if ( res != 1 ) {
            if ( feof( fp ) ) {
                /* End of file - fill with f6 - do not update CRC for these */
                memset( image + curwritepos, 0xf6, image_size - curwritepos );
                curwritepos += image_size - curwritepos;
            } else {
                return DSK_ERR_NOTME;
            }
        } else {
            int length = get_i16( lengthBuf, 0 );
            if ( length < 0 ) {            
                /* Negative number - next byte is repeated (-length) times */
                int c = fgetc( fp );
                if ( c == EOF ) return DSK_ERR_NOTME;
                /* Copy the byte into memory and update the offset */
                memset( image + curwritepos, c, -length );                
                curwritepos -= length;
                /* Update CRC */
                for( ; length != 0; ++length ) {
                    drv_qm_update_crc( &qm_self->qm_calc_crc, (unsigned char)c );
                }
            } else {
                if ( length != 0 ) {
                    /* Positive number - length different characters */
                    res = fread( image + curwritepos, length, 1, fp );
                    /* Update CRC (and write pos) */
                    while ( length-- ) {
                        drv_qm_update_crc( &qm_self->qm_calc_crc,
                                           image[curwritepos++] );
                    }
                    if ( res != 1 ) return DSK_ERR_NOTME;
                }
            }
        }
    }
#ifdef DRV_QM_DEBUG    
    fprintf( stderr, "drv_qm_load_image - crc from header = 0x%08lx, "
             "calc = 0x%08lx\n", qm_self->qm_h_crc, qm_self->qm_calc_crc );
#endif
    /* Compare the CRCs */
    /* The CRC is zero on old images so it cannot be checked then */
//GC: It seems this is always wrong for the images I have, so I skip it.
/*
    if ( qm_self->qm_h_crc ) {
        if ( qm_self->qm_h_crc != qm_self->qm_calc_crc ) {
            return DSK_ERR_NOTME;
        }
    }
*/

#ifdef DRV_QM_DEBUG
    /* Write image to file for testing */
/*
    if(1) {
       FILE* tmpFile = fopen("image.dd", "wb" );
       fwrite( image, image_size, 1, tmpFile );
       fclose( tmpFile );
       fprintf(stderr,"INFO: wrote image file image.dd - size %d\n",image_size);
    }
*/
#endif
    
    return errcond;
}

int CDiskImgCQM::drv_qm_load_header( QM_DSK_DRIVER* qm_self, char* header ) 
{
    int i;
    
    /* Check the header checksum */
    unsigned char chksum = 0;
    for ( i = 0; i < QM_HEADER_SIZE; ++i ) {
        chksum += (unsigned char)(header[i]);
    }
    if ( chksum != 0 ) {
#ifdef DRV_QM_DEBUG
        fprintf( stderr, "qm: header checksum error\n");
#endif
        return DSK_ERR_NOTME;
    } else {
#ifdef DRV_QM_DEBUG
        fprintf( stderr, "qm: header checksum is ok!\n");
#endif
    }
    if ( header[0] != 'C' || header[1] != 'Q' ) {
#ifdef DRV_QM_DEBUG
        fprintf( stderr, "qm: First two chars are %c%c\n", header[0],
                 header[1] );
#endif
        return DSK_ERR_NOTME;
    }
    /* I'm guessing sector size is at 3. Expandqm thinks 7 */
    qm_self->qm_h_sector_size = get_u16( header, 0x03 );
    /* Number of sectors 0x0B-0x0C, strange number for non-blind, often 116 */
    qm_self->qm_h_nbr_sectors = get_u16( header, 0x0b );
    /* Number of sectors per track */
    qm_self->qm_h_nbr_sec_per_track = get_u16( header, 0x10 );
    /* Number of heads */
    qm_self->qm_h_nbr_heads = get_u16( header, 0x12 );
    /* Blind or not */
    qm_self->qm_h_blind = header[0x58];
    /* Density - 0 is DD, 1 means HD */
    qm_self->qm_h_density = header[0x59];    
    /* Number of used tracks */
    qm_self->qm_h_used_tracks = header[0x5a];
    /* Number of total tracks */
    qm_self->qm_h_total_tracks = header[0x5b];
    /* CRC 0x5c - 0x5f */
    qm_self->qm_h_crc = get_u32( header, 0x5c );
    /* Length of comment */
    qm_self->qm_h_comment_len = get_u16( header, 0x6f );
    /* 0x71 is first sector number - 1 */
    qm_self->qm_h_secbase = (signed char)(header[0x71]);
    /* 0x74 is interleave, I think. Normally 1, but 0 for old copyqm */
    qm_self->qm_h_interleave = header[0x74];
    /* 0x75 is skew. Normally 0. Negative number for alternating sides */
    qm_self->qm_h_skew = header[0x75];

#ifdef DRV_QM_DEBUG
    fprintf( stderr, "qm: sector size is %d\n", qm_self->qm_h_sector_size);
    fprintf( stderr, "qm: nbr sectors %d\n",
             qm_self->qm_h_nbr_sectors );
    fprintf( stderr, "qm: nbr sectors/track %d\n",
             qm_self->qm_h_nbr_sec_per_track);
    fprintf( stderr, "qm: nbr heads %d\n",
             qm_self->qm_h_nbr_heads );
    fprintf( stderr, "qm: secbase %d\n", qm_self->qm_h_secbase );
    fprintf( stderr, "qm: density %d\n", qm_self->qm_h_density );
    fprintf( stderr, "qm: used tracks %d\n", qm_self->qm_h_used_tracks );
    fprintf( stderr, "qm: CRC 0x%08lx\n", qm_self->qm_h_crc );
    fprintf( stderr, "qm: interleave %d\n", qm_self->qm_h_interleave );
    fprintf( stderr, "qm: skew %d\n", qm_self->qm_h_skew );
    fprintf( stderr, "qm: description \"%s\"\n" , header + 0x1c );
#endif

    /* Fix the interleave value for old versions of CopyQM */
    if ( qm_self->qm_h_interleave == 0 ) {
        qm_self->qm_h_interleave = 1;
    }
    
    return DSK_ERR_OK;
}

int CDiskImgCQM::drv_qm_get_read_ptr(QM_DSK_DRIVER *qm_self, unsigned char **buf_ptr, int cylinder, int head, int sector)
{
    long offset;
    if (!buf_ptr || !qm_self)
        return DSK_ERR_BADPTR;

    offset = (cylinder * qm_self->qm_h_nbr_heads) + head;        /* Drive track */
    offset *= qm_self->qm_h_nbr_sec_per_track;
    offset += (sector - ((qm_self->qm_h_secbase + 1) & 0xFF));
    offset *=  qm_self->qm_h_sector_size;

    *buf_ptr = qm_self->qm_image + offset;

    return DSK_ERR_OK;
}

void CDiskImgCQM::drv_qm_dump_sector_hex(unsigned char* buf,int sz)
{
    int m,n;

    for (n = 0; n < sz; n = ((n / 16)+1)*16)
    {
	int base = (n/16)*16;
	int c = 0;

	printf("%04x: ", n);
	for (m = 0; m < (n%16); m++)
	{
		putchar(' ');
		putchar(' ');
		if ((c & 3) == 3) putchar(' ');
		c++;
	}
	for (m = (n % 16); m < 16; m++)
	{
		printf("%02x", buf[base+m]);
		if ((c & 3) == 3) putchar(' ');
		c++;
		if ((n + m + 1) >= sz) break;
	}
	for (; c < 16; c++)
	{
		putchar(' ');
		putchar(' ');
		if ((c & 3) == 3) putchar(' ');
	}

    	printf(" *");
    	for (m = 0; m < (n%16); m++) putchar(' ');
		
    	for (m = (n % 16); m < 16; m++)
    	{
		if (buf[base+m] >= ' ' && buf[base+m] < 127) 
			putchar(buf[base+m]);
		else    putchar('.');
		if ((n + m + 1) >= sz) break;
   	}
   	putchar('*');
   	putchar('\n');
    }
}



int CDiskImgCQM::qmOpen(char* imgName) 
{
    FILE* fp;
    char header[QM_HEADER_SIZE];
    size_t res;
    int errcond = 0;

	/*
    if(argc<2) {
       fprintf(stderr,"ERROR: missing file argument - abort\n");
       fprintf(stderr,"USAGE: %s <filename>\n",argv[0]);
       fprintf(stderr,"INFO:  %s - derived from LibDsk-1.2.1 - GNU Library General Public License applies\n",argv[0]);
       fprintf(stderr,"INFO:  %s - read CopyQM image, reports drive statistics and extracts binary disk image - \"image.dd\"\n\n",argv[0]);
       return 0;
    }
	*/

    /* Zero some stuff */
    qm_self->qm_image = 0;
    qm_self->qm_filename = 0;
    
    /* Open file. Read only for now */
    fp = fopen(imgName, "rb");
    if ( ! fp ) {
        fprintf(stderr,"ERROR: cannot open file %s - abort\n",imgName);
        return 0;
    }

    /* Load the header */
    res = fread( header, QM_HEADER_SIZE, 1, fp );
    if ( res != 1 ) {
	   fprintf(stderr,"ERROR while reading header - abort\n");
         return 0;
    }
    
    /* Load the header */
    errcond = drv_qm_load_header( qm_self, header );

    /* If there's a comment, allocate a temporary buffer for it and load it. */
    if (errcond == DSK_ERR_OK && qm_self->qm_h_comment_len)
    {
        char *comment_buf = (char*)malloc(1 + qm_self->qm_h_comment_len);
        /* If malloc fails, ignore it - comments aren't essential */
        if (comment_buf)
        {
            int res = fseek( fp, QM_HEADER_SIZE, SEEK_SET );
            if ( res < 0 ) errcond = DSK_ERR_NOTME;
            else 
            {
                res = fread(comment_buf, 1, qm_self->qm_h_comment_len, fp);
                if ( res < qm_self->qm_h_comment_len) {
                     fprintf(stderr,"ERROR: comment length exceeded image - abort\n");
                     return DSK_ERR_NOTME;
                } else {
                    comment_buf[qm_self->qm_h_comment_len] = 0;
                    /*dsk_set_comment(&qm_self->qm_super, comment_buf);  In case we want to store the comment */
                    fprintf(stderr,"qm: Disk Comment: %s\n",comment_buf);
                    free(comment_buf);
                }
            }
        }
    }

    errcond = drv_qm_load_image( qm_self, fp );
    if ( errcond != DSK_ERR_OK ) 
    {
         fprintf( stderr, "drv_qm_load_image returned %d\n", (int)errcond );
    }

	/*
    unsigned char* rd_buf;
    int rd_cyl, rd_head, rd_sec, rd_track;
    rd_cyl=rd_head=rd_track=0;
    rd_sec = 1;

    do {
         drv_qm_get_read_ptr( qm_self, &rd_buf, rd_cyl, rd_head, rd_sec);
         printf("Track=%d Sector=%d\n",rd_track,rd_sec);
         printf("drv_qm_read(cylinder=%d,head=%d,sector=%d)\n",rd_cyl,rd_head,rd_sec);
         drv_qm_dump_sector_hex( rd_buf, qm_self->qm_h_sector_size);

         rd_sec++;
         if(rd_sec > qm_self->qm_h_nbr_sec_per_track) {
            rd_sec=1;
            rd_track++;

            rd_cyl = (rd_track / qm_self->qm_h_nbr_heads);
            rd_head = (rd_track % qm_self->qm_h_nbr_heads);
         }
   } while((rd_cyl < qm_self->qm_h_total_tracks) && (rd_head < qm_self->qm_h_nbr_heads) && (rd_sec < (1 + qm_self->qm_h_nbr_sec_per_track)));
    */

    /* Close the file */
	/*
    if ( fp ) {
        fclose( fp );
    }
	*/
    return errcond;
}
