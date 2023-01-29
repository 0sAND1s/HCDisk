#pragma once

#include "CFileArchiveTape.h"

bool SendFileToIF1(CFileSpectrumTape* fs, byte comIdx, dword baudWanted = 4800);
bool GetFileFromIF1(CFileSpectrumTape* f, byte comIdx, dword baudWanted = 9600);
