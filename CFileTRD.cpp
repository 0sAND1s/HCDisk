#include "CFileTRD.h"
#include "stdio.h"

CFileTRD::CFileTRD(CFileArchive* fs): CFile()
{	
	if (fs != NULL)
		this->fs = fs;
	buffer = NULL;
	Length = 0;
}



word CFileTRD::GetBASStartLine()
{	
	word sectSz = 256;		
	//byte* buf = new byte[sectSz];
	//((CFileSystem*)fs)->ReadBlock(buf, SpectrumLength/sectSz);
	byte offset = (byte)SpectrumLength % sectSz;	
	this->buffer[sectSz * (SpectrumLength/sectSz) + offset];
	word lineNo = -1;
	if (*(word*)&this->buffer[offset] == 0xAA80 && (*(word*)&this->buffer[offset+2] & 0x8000) == 0)
		lineNo = *(word*)&this->buffer[offset+2];
	//delete buf;

	return lineNo;
}
