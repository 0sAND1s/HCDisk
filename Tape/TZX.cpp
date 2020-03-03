#include "TZX.h"
#include "memory.h"

const char CTZXFile::SIGNATURE[] = {'Z','X','T','a','p','e','!','\x1A'};
const char* CTZXFile::TZXArhBlkNames[] = 
{
	"Full title",
	"Publisher",
	"Author(s)",
	"Year",
	"Language",
	"Type",
	"Price",
	"Protection",		
	"Origin",
	"Comment(s)"
};

const char* CTZXFile::TZXBlkTypeNames[] = 
{
	"Standard speed data block",
	"Turbo speed data block",
	"Pure tone",
	"Sequence of pulses of various lengths",
	"Pure data block",
	"Direct recording block",
	"CSW recording block",
	"Generalized data block",
	"Pause (silence) or 'Stop the tape' command",
	"Group start",
	"Group end",
	"Jump to block",
	"Loop start",
	"Loop end",
	"Call sequence",
	"Return from sequence",
	"Select block",
	"Stop the tape if in 48K mode",
	"Set signal level"
	"Text description",
	"Message block",
	"Archive info",
	"Hardware type",
	"Custom info block",
	"Glue block"
};

CTZXFile::CTZXFile()
{
	m_bInGroup = m_bInLoop = false;	
	m_BlkLoopStrt = 0;
	m_CallSeq = NULL;
}

CTZXFile::~CTZXFile()
{

}

bool CTZXFile::Open(char* fileName, TapeOpenMode mode)
{
	TZXHeader hdr;

	this->fileName = fileName;
	
	tapFile = fopen(fileName, "rb");

	bool OK = false;

	if (mode == TAP_OPEN_EXISTING)
	{
		if (tapFile != NULL)
		{	
			fseek(tapFile, 0, SEEK_END);
			m_FileSz = ftell(tapFile);
			fseek(tapFile, 0, SEEK_SET);

			OK = (m_FileSz > 0) && fread(&hdr, sizeof(hdr), 1, tapFile) == 1 &&
				memcmp(hdr.Signature, SIGNATURE, sizeof(SIGNATURE)) == 0 &&
				hdr.VerMajor == VER_MAJOR &&
				IndexTape();			
		}
		else
			OK = false;
	}
	else if (mode == TAP_OPEN_NEW)
	{
		if (tapFile == NULL)
		{
			tapFile = fopen(fileName, "wb");
			if (tapFile != NULL)
			{
				memcpy(hdr.Signature, SIGNATURE, sizeof(SIGNATURE));
				hdr.VerMajor = VER_MAJOR;
				hdr.VerMinor = VER_MINOR;
				OK = fwrite(&hdr, sizeof(hdr), 1, tapFile) == 1;
			}
			else
				OK = false;
		}
	}
			
	return OK;
}

bool CTZXFile::Jump(short blkRelIdx)
{
	/*
	bool Res = true;
		
	Res = (fseek(tapFile, m_Idx[m_CurBlkIdx + blkRelIdx].offset, SEEK_SET) == 0);
	m_CurBlkIdx += blkRelIdx - 1;	

	return Res;
	*/

	bool Res = Seek((word)m_CurBlkIdx + blkRelIdx);
	m_CurBlkIdx--;
	return Res;
}

bool CTZXFile::GetFirstBlock(CTapeBlock* tb)
{
	m_CurBlkIdx = -1;
	fseek(tapFile, sizeof(TZXHeader), SEEK_SET);
	return GetNextBlock(tb);
}

bool CTZXFile::GetNextBlock(CTapeBlock* tb)
{		
	m_CurrBlkID = (TZXBlockTypeID)fgetc(tapFile);	
	long l = ftell(tapFile);
	bool Res = true;
	word wTmp = 0, Idx = 0;
	dword dwTmp = 0;
	
	m_CurBlkStts = BLK_RD_STS_VALID;	
	m_CurBlkIdx++;

	switch (m_CurrBlkID)
	{
	case BLK_ID_STD:
		Res = fread(&m_CurrBlk.blkStd, sizeof(m_CurrBlk.blkStd), 1, tapFile) == 1;				
		tb->Flag = fgetc(tapFile);		
		tb->Data = new byte[m_CurrBlk.blkStd.Len - 2];
		Res = fread(tb->Data, 1, m_CurrBlk.blkStd.Len - 2, tapFile) == m_CurrBlk.blkStd.Len - 2;
		tb->Length = m_CurrBlk.blkStd.Len;
		tb->Checksum = fgetc(tapFile);
		if (tb->Flag < 128)
			tb->m_Timings = CTapeBlock::ROM_TIMINGS_HEAD;
		else
			tb->m_Timings = CTapeBlock::ROM_TIMINGS_DATA;
		tb->m_Timings.Pause = m_CurrBlk.blkStd.Pause;		
		tb->LastBitCnt = 8;
		tb->m_BlkType = CTapeBlock::TAPE_BLK_STD;
		break;

	case BLK_ID_TURBO:
		Res = fread(&m_CurrBlk.blkTrb, sizeof(m_CurrBlk.blkTrb), 1, tapFile) == 1;		

		tb->Flag = fgetc(tapFile);
		tb->Data = new byte[m_CurrBlk.blkTrb.Len1 - 2];
		Res = fread(tb->Data, 1, m_CurrBlk.blkTrb.Len1 - 2, tapFile) == m_CurrBlk.blkTrb.Len1 - 2;
		tb->Checksum = fgetc(tapFile);
		tb->Length = m_CurrBlk.blkTrb.Len1 + (0x10000 * m_CurrBlk.blkTrb.Len2);

		tb->m_Timings.Bit0 = m_CurrBlk.blkTrb.Zero;
		tb->m_Timings.Bit1 = m_CurrBlk.blkTrb.One;
		tb->m_Timings.Pause = m_CurrBlk.blkTrb.Pause;
		tb->m_Timings.Pilot = m_CurrBlk.blkTrb.Pilot;		
		tb->m_Timings.PilotPulseCnt = m_CurrBlk.blkTrb.PilotLen;		
		tb->m_Timings.Sync1 = m_CurrBlk.blkTrb.Sync1;
		tb->m_Timings.Sync2 = m_CurrBlk.blkTrb.Sync2;

		tb->LastBitCnt = m_CurrBlk.blkTrb.LastBitsCnt;

		tb->m_BlkType = CTapeBlock::TAPE_BLK_TURBO;
		break;

	case BLK_ID_TXT_DESCR:
		m_CurrBlk.blkMsg.Len = fgetc(tapFile);
		Res = fread(m_CurrBlk.blkMsg.Msg, 1, m_CurrBlk.blkMsg.Len, tapFile) == m_CurrBlk.blkMsg.Len;		
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		break;

	case BLK_ID_GRP_STRT:
		m_CurrBlk.blkGrpStrt.Len = fgetc(tapFile);
		Res = fread(m_CurrBlk.blkGrpStrt.GrpName, 1, m_CurrBlk.blkGrpStrt.Len, tapFile) == m_CurrBlk.blkGrpStrt.Len;		
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		m_bInGroup = true;
		break;

	case BLK_ID_GRP_END:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		m_bInGroup = false;
		Res = true;
		break;

	case BLK_ID_JMP:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		Res = fread(&wTmp, sizeof(word), 1, tapFile) == 1;		
		m_CurrBlk.jmpType = wTmp;
		break;

	case BLK_ID_PAUSE:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_PAUSE;
		Res = fread(&m_CurrBlk.pauseLen, sizeof(word), 1, tapFile) == 1;
		break;

	case BLK_ID_PURE_TONE:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_PURE_TONE;
		Res = fread(&m_CurrBlk.blkPureTone, sizeof(m_CurrBlk.blkPureTone), 1 , tapFile) == 1;
		tb->m_PureTone.pulseCnt = m_CurrBlk.blkPureTone.pulseCnt;
		tb->m_PureTone.pulseLen = m_CurrBlk.blkPureTone.pulseLen;
		break;

	case BLK_ID_PULSES:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_PULSE_SEQ;
		tb->m_PulseSeq.pulseCnt = fgetc(tapFile);
		Res = fread(tb->m_PulseSeq.pulseLen, sizeof(word), tb->m_PulseSeq.pulseCnt, tapFile) == 
			tb->m_PulseSeq.pulseCnt;		
		break;

	case BLK_ID_PURE_DATA:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_PURE_DATA;
		Res = fread(&m_CurrBlk.blkPureData, sizeof(m_CurrBlk.blkPureData), 1 , tapFile) == 1;		
		
		tb->m_Timings.Bit0 = m_CurrBlk.blkPureData.Bit0;
		tb->m_Timings.Bit1 = m_CurrBlk.blkPureData.Bit1;		
		tb->LastBitCnt = m_CurrBlk.blkPureData.LastBitsCnt;
		tb->m_Timings.Pause = m_CurrBlk.blkPureData.Pause;
		tb->Length = m_CurrBlk.blkPureData.Len1 + (0x10000 * m_CurrBlk.blkPureData.Len2);
		tb->Data = new byte[tb->Length];
		Res = Res && fread(tb->Data, 1, tb->Length, tapFile) == tb->Length;
		break;

	case BLK_ID_ARH_INF:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		Res = (fread(&m_CurrBlk.blkArhiveInfo.Len, sizeof(word), 1, tapFile) == 1);
		m_CurrBlk.blkArhiveInfo.StrCnt = fgetc(tapFile);
		tb->Data = new byte[m_CurrBlk.blkArhiveInfo.Len];
		memcpy(tb->Data, &m_CurrBlk.blkArhiveInfo, sizeof(word) + sizeof(byte));
		Idx += sizeof(word) + sizeof(byte);
		for (byte sIdx = 0; sIdx < m_CurrBlk.blkArhiveInfo.StrCnt; sIdx++)
		{
			fread(&m_CurrBlk.blkArhiveInfo.TxtItem, sizeof(TZXBlkArhiveInfo::TextItem), 1, tapFile);
			memcpy(&tb->Data[Idx], &m_CurrBlk.blkArhiveInfo.TxtItem, sizeof(TZXBlkArhiveInfo::TextItem));
			Idx += sizeof(TZXBlkArhiveInfo::TextItem);
			fread(&tb->Data[Idx], 1, m_CurrBlk.blkArhiveInfo.TxtItem.TxtLen, tapFile);
			Idx += m_CurrBlk.blkArhiveInfo.TxtItem.TxtLen;
		}
		break;
	
	case BLK_ID_STOP_48K:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		//Skip size (0).
		fseek(tapFile, sizeof(dword), SEEK_CUR);
		break;

	case BLK_ID_LOOP_STRT:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		Res = fread(&m_LoopRptCnt, sizeof(word), 1, tapFile) == 1;
		m_BlkLoopStrt = m_CurBlkIdx + 1;		
		break;

	case BLK_ID_LOOP_END:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;		
		break;

	case BLK_ID_DIRECT_REC:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_DIRECT_REC;
		Res = fread(&m_CurrBlk.blkDirRec, sizeof(m_CurrBlk.blkDirRec), 1, tapFile) == 1;
		tb->Length = m_CurrBlk.blkDirRec.Len1 + 0x10000 * m_CurrBlk.blkDirRec.Len2;
		tb->Data = new byte[tb->Length];
		Res = Res && fread(tb->Data, 1, tb->Length, tapFile) == tb->Length;		

		tb->m_Timings.Bit0 = tb->m_Timings.Bit1 = m_CurrBlk.blkDirRec.SampleLen;				
		tb->LastBitCnt = m_CurrBlk.blkDirRec.LastBitsCnt;
		tb->m_Timings.Pause = m_CurrBlk.blkDirRec.Pause;
		break;

	case BLK_ID_CALL_SEQ:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		Res = fread(&wTmp, sizeof(word), 1, tapFile) == 1;
		m_CallSeq = (TZXBlkCallSeq*)new byte[sizeof(word) + wTmp * sizeof(word)];		
		m_CallSeq->CallCnt = wTmp;
		for (word cIdx = 0; cIdx < m_CallSeq->CallCnt; cIdx++)
		{
			Res = Res && (fread(&wTmp, sizeof(word), 1, tapFile) == 1);
			m_CallSeq->CallItem[cIdx] = wTmp;
		}
		m_CallSeqIdx = 0;
		m_CallSeqRetIdx = (word)m_CurBlkIdx + 1;
		break;

	case BLK_ID_RET_SEQ:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;		
		m_CallSeqIdx++;		
		break;

	case BLK_ID_GLUE:
		tb->m_BlkType = CTapeBlock::TAPE_BLK_METADATA;
		fseek(tapFile, sizeof(TZXBlkGlue), SEEK_CUR);
		break;

	case BLK_ID_CUST_INFO:	
		Res = true;
		m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
		fseek(tapFile, 0x10, SEEK_CUR);
		fread(&dwTmp, sizeof(dwTmp), 1, tapFile);
		fseek(tapFile, dwTmp, SEEK_CUR);
		break;

	case BLK_ID_SELECT:
		Res = true;
		m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
		fread(&wTmp, sizeof(word), 1, tapFile);
		fseek(tapFile, wTmp, SEEK_CUR);
		break;

	case BLK_ID_MSG:
		Res = true;
		m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
		fgetc(tapFile);
		wTmp = fgetc(tapFile);
		fseek(tapFile, wTmp, SEEK_CUR);
		break;

	case BLK_ID_CSW:
	case BLK_ID_GENERALIZED:		
		Res = true;
		m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
		fread(&dwTmp, sizeof(dwTmp), 1, tapFile);
		fseek(tapFile, dwTmp, SEEK_CUR);
		break;
		
	default:
		Res = false;
		m_CurBlkStts = BLK_RD_STS_UNRECOGNIZED;
		//Try to follow extension rule.
		fread(&dwTmp, sizeof(dwTmp), 1, tapFile);
		fseek(tapFile, dwTmp, SEEK_CUR);
	}		

	return Res;
}

//This one will just skip trough the blocks, will not read their content, to make it faster.
bool CTZXFile::IndexTape()
{	
	bool Res = true;
	word wTmp = 0, Idx = 0;
	dword dwTmp = 0;
	long off = 0;	
	TapeBlkIdxType idxItm;	
	m_CurBlkIdx = -1;

	while (ftell(tapFile) < m_FileSz && Res)
	{
		m_CurrBlkID = (TZXBlockTypeID)fgetc(tapFile);	
		off = ftell(tapFile);
		m_CurBlkIdx++;
		m_CurBlkStts = BLK_RD_STS_VALID;	
	
		switch (m_CurrBlkID)
		{	
			case BLK_ID_STD:
				Res = fread(&m_CurrBlk.blkStd, sizeof(m_CurrBlk.blkStd), 1, tapFile) == 1 &&
					fseek(tapFile, m_CurrBlk.blkStd.Len, SEEK_CUR) == 0;										
				break;

			case BLK_ID_TURBO:
				Res = (fread(&m_CurrBlk.blkTrb, sizeof(m_CurrBlk.blkTrb), 1, tapFile) == 1) &&
					(fseek(tapFile, m_CurrBlk.blkTrb.Len1 + (0x10000 * m_CurrBlk.blkTrb.Len2), SEEK_CUR) == 0);								
				break;

			case BLK_ID_TXT_DESCR:
				m_CurrBlk.blkMsg.Len = fgetc(tapFile);
				Res = fseek(tapFile, m_CurrBlk.blkMsg.Len, SEEK_CUR) == 0;
				break;
						
			case BLK_ID_GRP_STRT:
				m_CurrBlk.blkGrpStrt.Len = fgetc(tapFile);
				Res = fseek(tapFile, m_CurrBlk.blkGrpStrt.Len, SEEK_CUR) == 0;
				break;

			case BLK_ID_GRP_END:				
				Res = true;
				break;
			
			case BLK_ID_JMP:				
				Res = fseek(tapFile, sizeof(word), SEEK_CUR) == 0;						
				break;			
			
			case BLK_ID_PAUSE:				
				Res = fseek(tapFile, sizeof(word), SEEK_CUR) == 0;
				break;
			
			case BLK_ID_PURE_TONE:
				Res = fseek(tapFile, sizeof(TZXBlkPureTone), SEEK_CUR) == 0;				
				break;

			case BLK_ID_PULSES:								
				Res = fseek(tapFile, fgetc(tapFile) * sizeof(word), SEEK_CUR) == 0;					
				break;

			
			case BLK_ID_PURE_DATA:				
				Res = (fread(&m_CurrBlk.blkPureData, sizeof(m_CurrBlk.blkPureData), 1 , tapFile) == 1) &&
					fseek(tapFile, m_CurrBlk.blkPureData.Len1 + (0x10000 * m_CurrBlk.blkPureData.Len2), SEEK_CUR) == 0;
				break;

			
			case BLK_ID_ARH_INF:			
				Res = (fread(&wTmp, sizeof(wTmp), 1, tapFile) == 1) &&
					fseek(tapFile, wTmp, SEEK_CUR) == 0;
				break;
			
			
			case BLK_ID_STOP_48K:			
				//Skip size (0).
				fseek(tapFile, sizeof(dword), SEEK_CUR);
				break;

			case BLK_ID_LOOP_STRT:				
				Res = fseek(tapFile, sizeof(word), SEEK_CUR) == 0;
				break;

			case BLK_ID_LOOP_END:
				Res = true;
				break;

			case BLK_ID_DIRECT_REC:				
				Res = (fread(&m_CurrBlk.blkDirRec, sizeof(m_CurrBlk.blkDirRec), 1, tapFile) == 1) &&
					fseek(tapFile, m_CurrBlk.blkDirRec.Len1 + 0x10000 * m_CurrBlk.blkDirRec.Len2, SEEK_CUR) == 0;
				
				break;

			case BLK_ID_CALL_SEQ:				
				Res = (fread(&wTmp, sizeof(word), 1, tapFile) == 1) &&
					(fseek(tapFile, wTmp * sizeof(word), SEEK_CUR) == 0);						
				break;

			case BLK_ID_RET_SEQ:
				Res = true;
				break;

			case BLK_ID_GLUE:				
				Res = fseek(tapFile, sizeof(TZXBlkGlue), SEEK_CUR) == 0;
				break;

			case BLK_ID_CUST_INFO:	
				Res = true;
				m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
				fseek(tapFile, 0x10, SEEK_CUR);
				fread(&dwTmp, 4, 1, tapFile);
				fseek(tapFile, dwTmp, SEEK_CUR);
				break;

			case BLK_ID_SELECT:
				Res = true;
				m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
				fread(&wTmp, sizeof(word), 1, tapFile);
				fseek(tapFile, wTmp, SEEK_CUR);
				break;

			case BLK_ID_MSG:
				Res = true;
				m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
				fgetc(tapFile);
				wTmp = fgetc(tapFile);
				fseek(tapFile, wTmp, SEEK_CUR);
				break;

			case BLK_ID_CSW:
			case BLK_ID_GENERALIZED:		
				Res = true;
				m_CurBlkStts = BLK_RD_STS_UNSUPORTED;
				fread(&dwTmp, sizeof(dwTmp), 1, tapFile);
				fseek(tapFile, dwTmp, SEEK_CUR);
				break;

			default:
				Res = true;
				m_CurBlkStts = BLK_RD_STS_UNRECOGNIZED;
				//Try to follow extension rule.
				fread(&dwTmp, sizeof(dwTmp), 1, tapFile);
				fseek(tapFile, dwTmp, SEEK_CUR);
		}

		idxItm.Offset = off;
		idxItm.RdSts = m_CurBlkStts;
		m_Idx.push_back(idxItm);
	}	

	return Res;	
}