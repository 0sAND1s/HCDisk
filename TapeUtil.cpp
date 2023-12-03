#include <string>
#include <sstream>
#include <conio.h>

#include "TapeUtil.h"
#include "Tape/Tape2Wave.h"
#include "CFileSystem.h"
#include "GUIUtil.h"


using namespace std;

bool TapeUtil::ImportTape(CFileArchive* theFS, const char* tapSrcName, const char* pattern)
{
	bool writeOK = true;

	CFileArchiveTape tapeSrc((char*)tapSrcName);
	if (tapeSrc.Open((char*)tapSrcName, false) && tapeSrc.Init())
	{
		CFile* fSrc = tapeSrc.FindFirst((char*)pattern);

		while (fSrc != NULL && writeOK)
		{
			CFileSystem::FileNameType fn;
			fSrc->GetFileName(fn);
			CFile* fDest = theFS->NewFile(fn);

			if (fDest != NULL)
			{
				*dynamic_cast<CFileSpectrum*>(fDest) = *dynamic_cast<CFileSpectrumTape*>(fSrc);

				dword len = fSrc->GetLength();
				byte* buf = new byte[len];
				writeOK = fSrc->GetData(buf) && fDest->SetData(buf, len);
				//Ignore error if one of the files could not be written because it may exist already.
				theFS->WriteFile(fDest);
				delete[] buf;
			}
			else
			{
				writeOK = false;
			}

			delete fDest;
			fSrc = tapeSrc.FindNext();
		}


		tapeSrc.Close();
	}

	return writeOK;
}

void TapeUtil::DisplayTZXComments(CFileArchiveTape* theFS)
{
	CTapFile* theTap = (theFS)->theTap;
	CTapeBlock tb;
	bool bOK = theTap->GetFirstBlock(&tb);

	while (bOK)
	{
		if (tb.m_BlkType == CTapeBlock::TAPE_BLK_METADATA)
		{
			CTZXFile* tzx = (CTZXFile*)theTap;
			char msg[256];
			CTZXFile::TZXBlkArhiveInfo* blkArh;
			CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;

			switch (tzx->m_CurrBlkID)
			{
			case CTZXFile::BLK_ID_TXT_DESCR:
			case CTZXFile::BLK_ID_MSG:
				printf("Message/Description:\n");
				strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
				msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
				char* line;
				if ((line = strtok(msg, "\r")) != NULL)
				{
					do
					{
						printf("%s\n", line);
					} while ((line = strtok(NULL, "\r")) != nullptr);
				}
				break;
			case CTZXFile::BLK_ID_ARH_INF:
				blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb.Data;
				dword bufIdx = sizeof(word) + sizeof(byte);
				blkTxt = &blkArh->TxtItem;

				printf("Arhive Info:\n");
				for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
				{
					char* line;
					byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ?
						CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

					bufIdx += sizeof(byte) * 2;

					printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
					line = msg;
					strncpy(msg, (char*)&tb.Data[bufIdx], blkTxt->TxtLen);
					msg[blkTxt->TxtLen] = 0;

					if ((line = strtok(msg, "\r")) != NULL)
					{
						do
						{
							printf("%s%s\n", line == msg ? "" : "\t\t\t", line);
						} while (line = strtok(NULL, "\r"));
					}

					bufIdx += blkTxt->TxtLen;
					blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb.Data[bufIdx];
				}

				break;
			}
		}

		if (tb.Length > 0 && tb.Data != nullptr)
		{
			delete[] tb.Data;
			tb.Data = nullptr;
		}
		bOK = theTap->GetNextBlock(&tb);
	}
}

void TapeUtil::Tap2Wav(CTapFile * theTap, bool realTime)
{
	CTape2Wav tape2wav;
	CTZXFile* tzx = theTap->IsTZX() ? (CTZXFile*)theTap : nullptr;
	//CTZXFile::TZXBlkArhiveInfo* blkArh;
	//CTZXFile::TZXBlkArhiveInfo::TextItem* blkTxt;
	//dword bufIdx;

	if (theTap != NULL)
	{
		if (realTime)
			printf("Playing the tape. Press ESC to quit, SPACE to pause. ");
		printf("Block count: %d.\n", theTap->GetBlockCount());

		CTapeBlock tb;
		char Name[CTapeBlock::TAP_FILENAME_LEN + 1];
		stringstream wavName;
		if (!realTime)
		{
			wavName << theTap->fileName << ".wav";
			tape2wav.Open((char*)wavName.str().c_str());
		}

		bool blockRead = theTap->GetFirstBlock(&tb);
		while (blockRead)
		{
			printf("%02d: ", theTap->m_CurBlkIdx);

			if (theTap->m_CurBlkStts != CTapFile::BLK_RD_STS_VALID)
			{
				if (tzx != nullptr)
					printf("%s block, ID 0x%X, skipping.\n",
						CTapFile::TapeBlkReadStatusNames[theTap->m_CurBlkStts], (byte)tzx->m_CurrBlkID);
			}
			else
			{
				if (tb.m_BlkType != CTapeBlock::TAPE_BLK_METADATA)
				{
					if (tb.m_BlkType == CTapeBlock::TAPE_BLK_PAUSE)
					{
						if (tzx != nullptr && tzx->m_CurrBlk.pauseLen > 0)
							tape2wav.PutSilence(tzx->m_CurrBlk.pauseLen);
						else
						{
							if (!GUIUtil::Confirm("'Stop the tape' block encountered. Continue?"))
								break;
						}
						tape2wav.SetPulseLevel(false);
					}
					else
					{
						if (tb.m_BlkType == CTapeBlock::TAPE_BLK_STD)
							printf("%sStandard Block\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else if (tb.m_BlkType == CTapeBlock::TAPE_BLK_TURBO)
							printf("%sTurbo Block\t\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");
						else
							printf("%sRaw data\t\t", tzx != nullptr && tzx->m_bInGroup ? "\t" : "");

						if ((tb.m_BlkType == CTapeBlock::TAPE_BLK_STD || tb.m_BlkType == CTapeBlock::TAPE_BLK_TURBO) &&
							tb.IsBlockHeader())
						{
							tb.GetName(Name);
							printf("%-7s: %s\t%d\t%02X\n",
								tb.TapBlockTypeNames[((CTapeBlock::TapeBlockHeaderBasic*)tb.Data)->BlockType],
								Name, tb.Length, tb.Flag);
						}
						else
							printf("Data   :\t\t%d\t%02X\n", tb.Length, tb.Flag);

						//play along
						if (realTime)
						{
							wavName.str("");
							wavName << theTap->fileName << theTap->m_CurBlkIdx << ".wav";
							tape2wav.Open((char*)wavName.str().c_str());
							tape2wav.AddBlock(&tb);
							tape2wav.Close();
							dword lenMs = tape2wav.GetWrittenLenMS();
							dword timeIdx = 0;

							PlaySound((wavName.str().c_str()), NULL, SND_FILENAME | SND_ASYNC);

							while (!GetAsyncKeyState(VK_ESCAPE) && !GetAsyncKeyState(VK_SPACE) && timeIdx < lenMs)
							{
								Sleep(1000);
								timeIdx += 1000;
								printf("\rProgress: %3d %%, %3d/%3d seconds\t", (int)(((float)timeIdx / lenMs) * 100), timeIdx / 1000, lenMs / 1000);
							}

							if (timeIdx < lenMs)
							{
								char c = getch();
								if (c == 27)
								{
									PlaySound(NULL, NULL, 0);
									printf("Canceled\n");
									break;
								}
								else if (c == ' ')
								{
									printf("Paused. Press a key to continue.\n");
									getch();
								}
							}

							Sleep(500); //Need to sleep, to let PlaySound finish before starting a new block.
							remove(wavName.str().c_str());
						}
						else
							tape2wav.AddBlock(&tb);

					}
				}
				else if (tzx != nullptr) //Metadata block
				{
					char msg[256];

					switch (tzx->m_CurrBlkID)
					{
					case CTZXFile::BLK_ID_TXT_DESCR:
					case CTZXFile::BLK_ID_MSG:
						strncpy(msg, tzx->m_CurrBlk.blkMsg.Msg, tzx->m_CurrBlk.blkMsg.Len);
						msg[tzx->m_CurrBlk.blkMsg.Len] = 0;
						printf("Message/Description: '%s'\n", msg);
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
						/*
						case CTZXFile::BLK_ID_ARH_INF:
							blkArh = (CTZXFile::TZXBlkArhiveInfo*)tb.Data;
							bufIdx = sizeof(word) + sizeof(byte);
							blkTxt = &blkArh->TxtItem;

							printf("Arhive Info:\n");
							for (byte txtIdx = 0; txtIdx < blkArh->StrCnt; txtIdx++)
							{
								char* line;
								byte txtMsgIdx = (blkTxt->TxtID == CTZXFile::TXT_ID_COMMENTS ? CTZXFile::TXT_ID_ORIGIN + 1 : blkTxt->TxtID);

								bufIdx += sizeof(byte)*2;

								printf("\t%-10s:\t", CTZXFile::TZXArhBlkNames[txtMsgIdx]);
								line = msg;
								strncpy(msg, (char*)&tb.Data[bufIdx], blkTxt->TxtLen);
								msg[blkTxt->TxtLen] = 0;

								if ((line = strtok(msg, "\r")) != NULL)
								{
									do
									{
										printf("%s%s\n", line == msg ? "" : "\t\t\t", line);
									} while(line = strtok(NULL, "\r"));
								}

								bufIdx += blkTxt->TxtLen;
								blkTxt = (CTZXFile::TZXBlkArhiveInfo::TextItem*)&tb.Data[bufIdx];
							}
							break;
						*/
					case CTZXFile::BLK_ID_STOP_48K:
						if (GUIUtil::Confirm("'Stop tape if 48K' encountered. Stop?"))
							goto ExitLoop;
						break;

					case CTZXFile::BLK_ID_LOOP_STRT:
						printf("Loop start: %d repeats.\n", tzx->m_LoopRptCnt);
						tzx->m_BlkLoopIdx = 0;
						break;

					case CTZXFile::BLK_ID_LOOP_END:
						printf("Loop end %d.\n", tzx->m_BlkLoopIdx);
						if (++tzx->m_BlkLoopIdx < tzx->m_LoopRptCnt)
						{
							tzx->Seek((word)tzx->m_BlkLoopStrt);
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
							delete[] tzx->m_CallSeq;
						}
						break;

					default:
						printf("Block type %d skipped.\n", tzx->m_CurrBlkID);
					}
				}
			}

			if (tb.m_BlkType != CTapeBlock::TAPE_BLK_METADATA && tb.Length > 0 && tb.Data != nullptr)
			{
				delete[] tb.Data;
				tb.Data = nullptr;
			}

			blockRead = theTap->GetNextBlock(&tb);
		};
	ExitLoop:

		if (!realTime)
		{
			//theTap->Close();
			tape2wav.Close();
			dword sec = tape2wav.GetWrittenLenMS() / 1000;
			printf("Wrote %s with length is: %02d:%02d.\n", wavName.str().c_str(), sec / 60, sec % 60);
		}
	}
}
