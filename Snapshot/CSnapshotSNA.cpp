#include "CSnapshotSNA.h"
#include <stdio.h>
#include "..\FileUtil.h"
#include <algorithm>

using namespace std;

bool CSnapshotSNA::Read(const char* path)
{
	string ext = FileUtil::GetExtension(string(path));
	transform(ext.begin(), ext.end(), ext.begin(), toupper);

	if (ext != "SNA")
		return false;	
	
	long fSize = FileUtil::fsize(path);
	if (fSize != sizeof(SNAHeader) + sizeof(SNAMemory))
		return false;

	FILE* fSNA = fopen(path, "rb");
	if (fSNA == nullptr)
		return false;

	if (fread(&SNAHdr, sizeof(SNAHdr), 1, fSNA) != 1)
	{
		fclose(fSNA);
		return false;
	}

	if (fread(&SNAMemory, sizeof(SNAMemory), 1, fSNA) != 1)
	{
		fclose(fSNA);
		return false;
	}

	fclose(fSNA);
	IsOpened = true;	

	return true;
}


bool CSnapshotSNA::Write(const char* path)
{
	FILE* fSNA = fopen(path, "wb");
	if (fSNA == nullptr)
		return false;

	if (fwrite(&SNAHdr, sizeof(SNAHdr), 1, fSNA) != 1)
	{
		fclose(fSNA);
		return false;
	}

	if (fwrite(&SNAMemory, sizeof(SNAMemory), 1, fSNA) != 1)
	{
		fclose(fSNA);
		return false;
	}

	fclose(fSNA);

	return true;
}