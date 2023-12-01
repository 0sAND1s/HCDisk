#include "CFileTRD.h"
#include "stdio.h"

CFileTRD::CFileTRD(CFileArchive* fs): CFile()
{	
	if (fs != NULL)
		this->fs = fs;
	buffer = NULL;
	Length = 0;
}