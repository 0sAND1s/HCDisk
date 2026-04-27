#pragma once
#include "..\types.h"

class SNAP2TAP
{
public:
	SNAP2TAP();
	word FindMemGap(byte* mem, word memLen, const word minLenReq);
	bool Convert(std::string nameSNA, std::string nameTAP);	

protected:
};