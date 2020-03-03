#ifndef _TAPE2WAV_H_
#define _TAPE2WAV_H_

#include "TapeBlock.h"
#include "Wave.h"

class CTape2Wav
{
public:
	static const dword Z80TSTATES	= 3500000;
	static float T2HZ(word TCount) {return ((float)Z80TSTATES/TCount/2);};

	bool Open(char* wavName);
	bool AddBlock(CTapeBlock* tb);
	bool Close();	
	bool PutSilence(word ms);
	void SetPulseLevel(bool pulseLvl) {CurPulseLevel = pulseLvl;};
	dword GetWrittenLenMS() 
	{ 		
		return wav.GetWrittenLenMS(); 
	}

protected:
	CWave wav;
	bool CurPulseLevel;

	void PutByte(byte b, CTapeBlock::TapeTimgins t, byte bitCnt = 8);
};

#endif//_TAPE2WAV_H_