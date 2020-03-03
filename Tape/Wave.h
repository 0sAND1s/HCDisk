//////////////////////////////////////////////////////////////////////////
//Wav file writer, (C) George Chirtoaca, 2014
//////////////////////////////////////////////////////////////////////////

#ifndef _WAVE_H_
#define _WAVE_H_

#include <Windows.h>
#include <Mmsystem.h>
#include <Mmreg.h.>
#include "types.h"

//Includes functionality for reading & writing WAV files.
class CWave
{
public:
	typedef enum
	{
		BPS_8 = 8,
		BPS_16 = 16
	} BitsPerSampleType;

	typedef enum
	{
		CHANNELS_MONO = 1,
		CHANNELS_STEREO = 2
	} ChannelsType;

	typedef enum
	{
		SAMPLES_11025 = 11025,
		SAMPLES_22050 = 22050,
		SAMPLES_44100 = 44100,
		SAMPLES_48000 = 48000,
	} SampleRateType;	

	CWave();
	//Open for write.
	bool Open(char* fileName, BitsPerSampleType bps = BPS_8, SampleRateType samp = SAMPLES_48000, ChannelsType chn = CHANNELS_MONO);
	bool Close();
	void PutPulse(float freq, bool high);
	void PutWaves(float freq, word waveCnt, bool& startLevel);
	bool PutSilence(word ms);
	bool SaveBuf(bool flush = false);
	dword GetWrittenLenMS() { return writenLen; }
	~CWave();

protected:
	WAVEFORMATEX formatinfo;
	WAVEHDR buffer;
	HWAVEIN hwi;
	HMMIO file;
	MMCKINFO mmckinfo;
	MMCKINFO mmckinfoSubchunk;
	MMCKINFO mmckinfoData;

	SampleRateType m_SampRate;
	BitsPerSampleType m_BPS;
	ChannelsType m_Chn;

	//buffer length
	static const byte BUF_LEN_SEC = 5;
	//double buffering
	static const byte BUF_ITEMS = 2;
	//buffer size
	dword BUF_SIZE;

	dword bufIdx;
	byte* theBuf;
	float SamplRest;

	static const word VOLUME = 80;
	static const byte AMP_HI_8BIT = 0xFF * VOLUME/100;
	static const byte AMP_LOW_8BIT = (0xFF - AMP_HI_8BIT);
	static const word AMP_HI_16BIT = 32767 * VOLUME/100;
	static const word AMP_LOW_16BIT = -32768 * VOLUME/100;			

	dword writenLen;
};


#endif//_WAVE_H_