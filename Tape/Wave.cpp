#include "Wave.h"
#include <math.h>

CWave::CWave()
{
	theBuf = NULL;
	SamplRest = 0.0;	
}

CWave::~CWave()
{
	if (theBuf)
		delete theBuf;
}

bool CWave::Open(char* fileName,  BitsPerSampleType bps, SampleRateType samp, ChannelsType chn)
{
	formatinfo.cbSize = 0;
	formatinfo.wFormatTag = WAVE_FORMAT_PCM;
	formatinfo.nChannels = chn; // 1 for mono 2 and 2 for stereo
	formatinfo.wBitsPerSample = bps; // can be 8 or 16
	formatinfo.nSamplesPerSec = samp; // u can set nSamplesPerSec as 16000, 24000.
	formatinfo.nBlockAlign = formatinfo.nChannels * (formatinfo.wBitsPerSample / 8);
	formatinfo.nAvgBytesPerSec = formatinfo.nSamplesPerSec * formatinfo.nBlockAlign;

	m_Chn = chn;
	m_SampRate = samp;
	m_BPS = bps;

	/*
	Basic steps you need to follow while writing wave file using mmio
	1) mmioOpen
	2) mmioCreateChunk – 'RIFF' & 'WAVE'
	3) mmioCreateChunk – 'fmt '
	4) mmioWrite – WAVEFORMATEX
	5) mmioAscend – out of 'fmt ' – writes fmt length
	6) mmioCreateChunk 'data'
	7) mmioWrite – audio data
	8) mmioAscend – out of 'data' – writes data length
	9) mmioAscend -out of 'RIFF' – write 'RIFF' length
	10) mmioClose
	*/

	// creating a file using mmio, C:\\Recording.wav already exists	
	file = mmioOpen(fileName, NULL, MMIO_CREATE | MMIO_READWRITE );
	if (file == NULL)
		return false;

	memset(&mmckinfo, 0, sizeof(mmckinfo));
	memset(&mmckinfoSubchunk, 0, sizeof(mmckinfoSubchunk));
	memset(&mmckinfoData, 0, sizeof(mmckinfoData));

	//step 1 create riff chunk
	mmckinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if(mmioCreateChunk( file, &mmckinfo, MMIO_CREATERIFF) != MMSYSERR_NOERROR)
		return false;

	//step 2 create fmt chunk
	//creating fmt chunk also includes writing formatex to this chunk
	mmckinfoSubchunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mmckinfoSubchunk.cksize = sizeof(WAVEFORMATEX) ;

	if(mmioCreateChunk( file, &mmckinfoSubchunk, 0) != MMSYSERR_NOERROR)
		return false;

	if(mmioWrite( file, (char*)&formatinfo, sizeof(formatinfo)) == -1 )
		return false;

	if(mmioAscend( file, &mmckinfoSubchunk, 0) != MMSYSERR_NOERROR )
		return false;

	//step 3 creating data chunk
	//creating this chunk includes writing actual voice data
	mmckinfoData.ckid = mmioFOURCC('d','a','t','a');
	mmckinfoData.cksize = formatinfo.nAvgBytesPerSec;
	if(mmioCreateChunk( file, &mmckinfoData, 0) != MMSYSERR_NOERROR)
		return false;

	BUF_SIZE = 64 * 1024;//(samp * (bps/8) * BUF_LEN_SEC);
	theBuf = new byte[BUF_SIZE];
	bufIdx = 0;

	return true;
}

bool CWave::Close()
{
	bool res = true;	
	
	res = mmioAscend( file, &mmckinfoData, 0) == MMSYSERR_NOERROR;	
	if (res)
		writenLen = mmckinfoData.cksize/formatinfo.nAvgBytesPerSec * 1000;
	res = res && mmioAscend( file,  &mmckinfo, 0) == MMSYSERR_NOERROR; 	
	res = res && mmioClose(file, 0) == MMSYSERR_NOERROR;

	delete [] theBuf;
	theBuf = NULL;

	return true;	
}

bool CWave::SaveBuf(bool flush)
{	
	if (bufIdx > 0 && (flush || (bufIdx > (BUF_SIZE * .90))))
	{
		if (mmioWrite(file, (const char*)theBuf, bufIdx) == -1 )	
			return false;		
		bufIdx = 0;
	}	

	return true;
}


void CWave::PutPulse(float freq, bool lvlHigh)
{
	//This has to be interpolated.
	float SampPerPulse = (float)formatinfo.nSamplesPerSec/freq/2;	
	word iSampPerPulse = (word)SampPerPulse;
	SamplRest += SampPerPulse - iSampPerPulse;
	word p;

	if (m_BPS == BPS_8)
	{		
		for (p = 0; p < iSampPerPulse;  p++)		
		{
			theBuf[bufIdx] = (lvlHigh ? AMP_HI_8BIT : AMP_LOW_8BIT);			
			bufIdx++;
			SaveBuf();
		}

		if ((word)SamplRest > 1)
		{
			theBuf[bufIdx] = (lvlHigh ? AMP_HI_8BIT : AMP_LOW_8BIT);
			SamplRest -= 1.0;
			iSampPerPulse++;						
			bufIdx++;
			SaveBuf();
		}
	}
	else
	{
		for (p = 0; p < iSampPerPulse;  p++)		
		{
			theBuf[bufIdx] = (lvlHigh ? AMP_HI_16BIT%0x100 : AMP_LOW_16BIT%0x100);			
			theBuf[bufIdx + 1] = (lvlHigh ? AMP_HI_16BIT/0x100 : AMP_LOW_16BIT/0x100);			
			bufIdx += 2;
			SaveBuf();
		}

		if ((word)SamplRest > 1)
		{
			theBuf[bufIdx] = (lvlHigh ? AMP_HI_16BIT%0x100 : AMP_LOW_16BIT%0x100);			
			theBuf[bufIdx + 1] = (lvlHigh ? AMP_HI_16BIT/0x100 : AMP_LOW_16BIT/0x100);
			SamplRest -= 1.0;
			iSampPerPulse++;			
			bufIdx += 2;
			SaveBuf();
		}
	}
				
	SaveBuf();
}


void CWave::PutWaves(float freq, word waveCnt, bool& startLevel)
{	
	for (unsigned long s = 0; s < waveCnt; s++)
	{
		PutPulse(freq, startLevel);		
		startLevel ^= true;
		PutPulse(freq, startLevel);		
		startLevel ^= true;
	}		
}

bool CWave::PutSilence(word ms)
{
	byte SilanceSample8Bit = AMP_LOW_8BIT;
	short SilanceSample16Bit = AMP_LOW_16BIT;
	dword iSampCnt = (word)((float)m_SampRate/1000)*ms;

	if (m_BPS == BPS_8)
	{		
		for (dword p = 0; p < iSampCnt;  p++)		
		{
			theBuf[bufIdx] = SilanceSample8Bit;	
			bufIdx++;
			SaveBuf();
		}
	}
	else
	{
		for (dword p = 0; p < iSampCnt;  p++)		
		{
			theBuf[bufIdx] = SilanceSample16Bit % 0x100;
			theBuf[bufIdx + 1] = SilanceSample16Bit / 0x100;			
			bufIdx += 2;
			SaveBuf();
		}
	}	

	//bufIdx += iSampCnt * m_BPS/8;
	return SaveBuf();
}


/*
int Record()
{
WAVEINCAPS lpwaveincaps;
MMRESULT sounddeviceinfo;

UINT count = waveInGetNumDevs();

printf("%d\n",count);

sounddeviceinfo = waveInGetDevCaps(0, &lpwaveincaps, sizeof(WAVEINCAPS));
if (sounddeviceinfo != MMSYSERR_NOERROR)
{
printf("Error in getting info about Sound Device\n");
return 0;
}
//printf("%d",sounddeviceinfo);

MMRESULT soundfileopen;	

soundfileopen = waveInOpen( &hwi, WAVE_MAPPER, &formatinfo, 0, 0, 0);
if (soundfileopen != MMSYSERR_NOERROR)
{
if (soundfileopen == MMSYSERR_ALLOCATED)
{
printf("Error in opening audio device, 1\n");
return 1;
}
if (soundfileopen == MMSYSERR_BADDEVICEID)
{
printf("Error in opening audio device, 2\n");
return 2;
}
if (soundfileopen == MMSYSERR_NODRIVER)
{
printf("Error in opening audio device, 3 \n");
return 3;
}
if (soundfileopen == MMSYSERR_NOMEM)
{
printf("Error in opening audio device, 4\n");
return 4;
}
if (soundfileopen == WAVERR_BADFORMAT)
{
printf("Error in opening audio device, 5 \n");
return 5;
}
}

// Preaparing header
MMRESULT waveinheader;
waveinheader = waveInPrepareHeader( hwi, &buffer, sizeof(WAVEHDR));
if (waveinheader == MMSYSERR_NOERROR)
{		
waveInAddBuffer( hwi, &buffer, sizeof(buffer));

printf("\nRecording Data...\n");

// start recording
waveInStart( hwi);

//keep recording for 100milliseconds unless flag is set
while ( (buffer.dwFlags & WHDR_DONE) != WHDR_DONE)
{
Sleep(100);
//printf("dwFlags %d\n", buffer.dwFlags);
}

waveInStop( hwi);	
waveInUnprepareHeader(hwi, &buffer, sizeof(buffer));
waveInClose(hwi);

printf("Recording Completed, number of bytes recorded are %d\n", buffer.dwBytesRecorded );
}	
else
{
if (waveinheader == MMSYSERR_INVALHANDLE)
{
printf("Specified device handle is invalid.\n");
return 6;
}

if (waveinheader == MMSYSERR_NODRIVER)
{
printf("No device driver is present.\n");
return 7;
}

if (waveinheader == MMSYSERR_NOMEM)
{
printf("Unable to allocate or lock memory.\n");
return 8;
}	
}	

return 0;
}
*/
