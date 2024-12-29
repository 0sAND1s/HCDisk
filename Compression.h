#pragma once

#include "types.h"

class Compression
{
public:
	struct Z80Unpacker
	{
		word UnpackerAddress;
		byte UnpackerCode[];
	} Unpacker;

	virtual dword Compress(byte* bufSrc, dword lenSrc, byte* bufDst) = 0;
	virtual dword UnCompress(byte& bufSrc, dword lenSrc, byte* bufDst) = 0;
};

