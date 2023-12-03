#pragma once
#include <string>
#include "CFileArchiveTape.h"

class TapeUtil
{
public:
	static bool ImportTape(CFileArchive* theFS, const char* tapSrcName, const char* pattern = "*");
	static void DisplayTZXComments(CFileArchiveTape* theFS);
	static void Tap2Wav(CTapFile* theTap, bool realTime = true);
};

