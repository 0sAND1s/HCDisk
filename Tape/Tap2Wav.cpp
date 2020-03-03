#ifndef TAP2WAV
#define TAP2WAV

#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "TapeBlock.h"
#include "tap.h"
#include "TZX.h"
#include "Tape2Wave.h"


bool Confirm2(char* msg)
{
	char c;
	printf("%s [ENTER=YES,ESC=NO]", msg);
	do 
	{
		c = getch();
	} while(c != 13 && c != 27);

	printf(" %s\n", c == 13 ? "YES" : "NO");

	return c == 13;
}

void Tap2Wav(CTapFile* theTap)
{				
	CTape2Wav tape2wav;
	CTZXFile* tzx = dynamic_cast<CTZXFile*>(theTap);
	CTZXFile::TZXBlkArhiveInfo* blkArh;
	CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;
	dword bufIdx;

	if (theTap != NULL)
	{							
		printf("Block count is: %d.\n",  theTap->GetBlockCount());

		string outName = theTap->fileName;
		outName += ".wav";
		tape2wav.Open((char*)outName.c_str());

		CTapeBlock* tb = new CTapeBlock();
		char Name[CTapeBlock::TAP_FILENAME_LEN + 1];	

		for (word i = 0; i < theTap->GetBlockCount(); i++)
		{				
			if (theTap->GetBlock(i, tb) && tb->IsBlockHeader())
			{
				tb->GetName(Name);
				printf("%-7s: %s\t%d\t%02X\n", 
					tb->TapBlockTypeNames[((CTapeBlock::TapeBlockHeaderBasic*)tb->Data)->BlockType], 
					Name, tb->Length, tb->Flag);
			}
		}
		
		if (theTap->GetFirstBlock(tb))
			do
			{			
				printf("%02d: ", theTap->m_CurBlkIdx);					

				if (theTap->m_CurBlkStts != CTapFile::BLK_RD_STS_VALID)
				{
					printf("%s block, ID 0x%X, skipping.\n",
						CTapFile::TapeBlkReadStatusNames[theTap->m_CurBlkStts], (byte)tzx->m_CurrBlkID);						
				}
				else 
				{					
					if (tb->m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
					{				
						if (tb->m_BlkType == CTapeBlock::TAPE_BLK_PAUSE)
						{							
							if (tzx->m_CurrBlk.pauseLen > 0)
								tape2wav.PutSilence(tzx->m_CurrBlk.pauseLen);
							else
							{								
								if (!Confirm2("'Stop the tape' block encountered. Continue?"))
									break;
							}							
							tape2wav.SetPulseLevel(false);
						}
						else
						{
							if (tb->m_BlkType == CTapeBlock::TAPE_BLK_STD)
								printf("%sStd. Block\t", tzx->m_bInGroup ? "\t" : "");
							else if (tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
								printf("%sTrb. Block\t", tzx->m_bInGroup ? "\t" : "");
							else
								printf("%sRaw data\t", tzx->m_bInGroup ? "\t" : "");

							if ((tb->m_BlkType == CTapeBlock::TAPE_BLK_STD || tb->m_BlkType == CTapeBlock::TAPE_BLK_TURBO) &&
								tb->IsBlockHeader())
							{
								tb->GetName(Name);
								printf("%-7s: %s\t%d\t%02X\n", 
									tb->TapBlockTypeNames[((CTapeBlock::TapeBlockHeaderBasic*)tb->Data)->BlockType], 
									Name, tb->Length, tb->Flag);
							}
							else
								printf("Data   :\t\t%d\t%02X\n", tb->Length, tb->Flag);

							tape2wav.AddBlock(tb);
						}						
					}					
					else
					{
						char msg[256];

						switch (tzx->m_CurrBlkID)
						{
						case CTZXFile::BLK_ID_TXT_DESCR:
							strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
							msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
							printf("Message: '%s'\n", msg);
							break;
						case CTZXFile::BLK_ID_GRP_STRT:
							strncpy(msg, tzx->m_CurrBlk.blkGrpStrt.GrpName, tzx->m_CurrBlk.blkGrpStrt.Len);
							msg[tzx->m_CurrBlk.blkGrpStrt.Len] = 0;
							printf("Group: \"%s\"\n", msg);							
							break;
						case CTZXFile::BLK_ID_GRP_END:
							printf("Group end.\n");
							break;
						case CTZXFile::BLK_ID_JMP:
							printf("Jump to block: %d.\n", tzx->m_CurBlkIdx + tzx->m_CurrBlk.jmpType);							
							//if (Confirm(" Make jump?"))
							tzx->Jump(tzx->m_CurrBlk.jmpType);														
							break;						
						case CTZXFile::BLK_ID_ARH_INF:
							blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb->Data;
							bufIdx = sizeof(word) + sizeof(byte);
							blkTxt = &blkArh->TxtItem;

							printf("Arhive Info:\n");
							for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
							{													
								char* line;
								byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ? 
									CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

								bufIdx += sizeof(byte)*2;								

								printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
								line = msg;
								strncpy(msg, (char*)&tb->Data[bufIdx], blkTxt->TxtLen);
								msg[blkTxt->TxtLen] = 0;

								if ((line = strtok(msg, "\r")) != NULL)
								{
									do 
									{
										printf("%s%s\n", line == msg ? "" : "\t\t\t", line);									
									} while(line = strtok(NULL, "\r"));									
								}
								
								bufIdx += blkTxt->TxtLen;																															
								blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb->Data[bufIdx];
							}
							break;

						case CTZXFile::BLK_ID_STOP_48K:
							if (Confirm2("'Stop tape if 48K' encountered. Stop?"))
								goto ExitLoop;
							break;

						case CTZXFile::BLK_ID_LOOP_STRT:
							printf("Loop start: %d repeats.\n", tzx->m_LoopRptCnt);
							tzx->m_BlkLoopIdx = 0;
							break;

						case CTZXFile::BLK_ID_LOOP_END:
							printf("Loop end.\n");
							if (++tzx->m_BlkLoopIdx < tzx->m_LoopRptCnt)
							{
								tzx->Seek((word)tzx->m_BlkLoopStrt);															
								tzx->m_CurBlkIdx--;
							}
							break;							

						case CTZXFile::BLK_ID_CALL_SEQ:
							printf("Call sequence, %d calls.\n", tzx->m_CallSeq->CallCnt);														
							tzx->Jump(tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);
							break;

						case CTZXFile::BLK_ID_RET_SEQ:
							printf("Ret. from call to block ");
							if (tzx->m_CallSeqIdx < tzx->m_CallSeq->CallCnt)
							{
								printf("%d.\n", tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);
								tzx->Jump(tzx->m_CallSeq->CallItem[tzx->m_CallSeqIdx]);								
							}
							else
							{
								printf("%d.\n", tzx->m_CallSeqRetIdx);
								tzx->Seek(tzx->m_CallSeqRetIdx);
								tzx->m_CurBlkIdx--;
								delete [] tzx->m_CallSeq;
							}
							break;
						
						default:
							printf("Glue block.\n");
						}
					}
				}
			}
			while (theTap->GetNextBlock(tb));
ExitLoop:
		delete tb;			
		theTap->Close();
		tape2wav.Close();
	}		
}

#endif