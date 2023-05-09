#pragma once

#include "CFileArchiveTape.h"

BOOL SetCOMForIF1(char* m_sComPort, int baud = 19200);
bool ReadBufferFromIF1(byte* bufDest, word bufLen);
bool WriteBufferToIF1(byte* bufDest, word bufLen);
bool SendFileToIF1(CFileSpectrumTape* fs, byte comIdx, dword baudWanted = 4800);
bool GetFileFromIF1(CFileSpectrumTape* f, byte comIdx, dword baudWanted = 9600);
BOOL CloseCOM();