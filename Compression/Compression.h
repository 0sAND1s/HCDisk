#pragma once

#include "..\types.h"

class Compression
{
public:
	/*
	const struct Z80Unpacker
	{
		word UnpackerAddress;
		byte* UnpackerCode;
	} Unpacker;
	*/

	Compression();
	word Compress(byte* bufSrc, dword lenSrc, byte** bufDst, bool backwardsCompress, bool quickMode = false, int* delta = nullptr);
};

