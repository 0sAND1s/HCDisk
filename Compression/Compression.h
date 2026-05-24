#pragma once

#include "..\types.h"

class Compression
{
public:
	Compression();
	word Compress(byte* bufSrc, dword lenSrc, byte** bufDst, bool backwardsCompress, bool quickMode = false, int* delta = nullptr);
};

