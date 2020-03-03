#include "Tape2Wave.h"

bool CTape2Wav::Open(char* wavName)
{	
	CurPulseLevel = false;	

	return wav.Open(wavName);
}

bool CTape2Wav::Close()
{
	wav.PutSilence(1);
	wav.SaveBuf(true);
	return wav.Close();
}

void CTape2Wav::PutByte(byte b, CTapeBlock::TapeTimgins t, byte bitCnt)
{
	dword sampWr = 0;

	for (byte bit = 0; bit < bitCnt; bit++)
	{
		wav.PutWaves(b&(1<<(7-bit)) ? T2HZ(t.Bit1) : T2HZ(t.Bit0), 1, CurPulseLevel);			
	}
}


bool CTape2Wav::AddBlock(CTapeBlock* tb)
{	
	CurPulseLevel = false;
	if (tb->m_BlkType == CTapeBlock::TAPE_BLK_STD || tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
	{
		//Pilot
		wav.PutWaves(T2HZ(tb->m_Timings.Pilot), tb->m_Timings.PilotPulseCnt/2, CurPulseLevel);	

		//Syncs
		wav.PutPulse(T2HZ(tb->m_Timings.Sync1), CurPulseLevel);
		CurPulseLevel ^= true;
		wav.PutPulse(T2HZ(tb->m_Timings.Sync2), CurPulseLevel);							
		CurPulseLevel ^= true;

		//Flag
		PutByte(tb->Flag, tb->m_Timings);

		//Main data
		word bIdx = 0;
		while (bIdx < tb->Length - 2)
		{
			PutByte(tb->Data[bIdx], tb->m_Timings);
			bIdx++;						
		}

		//Checksum
		PutByte(tb->Checksum, tb->m_Timings, tb->LastBitCnt);				
		
		if (tb->m_Timings.Pause > 0)
			wav.PutSilence(tb->m_Timings.Pause);		
	}
	else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PURE_TONE)
	{	
		for (int p = 0; p < tb->m_PureTone.pulseCnt/2; p++)
		{
			wav.PutPulse(T2HZ(tb->m_PureTone.pulseLen), CurPulseLevel);
			CurPulseLevel ^= true;
			wav.PutPulse(T2HZ(tb->m_PureTone.pulseLen), CurPulseLevel);							
			CurPulseLevel ^= true;
		}				
	}
	else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PULSE_SEQ)
	{	
		for (int p = 0; p < tb->m_PulseSeq.pulseCnt; p++)
		{
			wav.PutPulse(T2HZ(tb->m_PulseSeq.pulseLen[p]), CurPulseLevel);
			CurPulseLevel ^= true;			
		}				
	}
	else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PURE_DATA)
	{	
		dword bIdx = 0;
		while (bIdx < tb->Length - 1)
		{
			PutByte(tb->Data[bIdx], tb->m_Timings);
			bIdx++;						
		}

		PutByte(tb->Data[bIdx], tb->m_Timings, tb->LastBitCnt);

		if (tb->m_Timings.Pause > 0)
			wav.PutSilence(tb->m_Timings.Pause);		
	}
	else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_DIRECT_REC)
	{
		dword bIdx = 0;
		byte b = 0;
		while (bIdx < tb->Length - 1)
		{
			b = tb->Data[bIdx];
			
			for (byte bit = 0; bit < 8; bit++)
			{
				CurPulseLevel = (b&(1<<(7-bit))) > 0;
				wav.PutPulse(T2HZ(tb->m_Timings.Bit0), CurPulseLevel);			
			}
			bIdx++;						
		}

		//Last byte
		for (byte bit = 0; bit < tb->LastBitCnt; bit++)
		{
			CurPulseLevel = (b&(1<<(7-bit))) > 0;
			wav.PutPulse(T2HZ(tb->m_Timings.Bit0), CurPulseLevel);			
		}

		if (tb->m_Timings.Pause > 0)
			wav.PutSilence(tb->m_Timings.Pause);		
	}
	
	return true;
}

bool CTape2Wav::PutSilence(word ms)
{
	return wav.PutSilence(ms);
}