#pragma once
#include "types.h"

class SNA2Tap
{
public:
	SNA2Tap();
	word FindMemGap(byte* mem, word memLen, const word minLenReq);
	bool Convert(std::string nameSNA, std::string nameTAP);	

protected:
};