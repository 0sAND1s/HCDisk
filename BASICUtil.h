#pragma once

#include "types.h"
#include "CFileArchive.h"
#include "CFileArchiveTape.h"
#include "FileConverters/BasicDecoder.h"

#include <string>
#include <vector>
using namespace std;

class BASICUtil
{
public:
	//Loader synthax for storage devices. Using '?' to signal the place of file/block name. Using NOT PI instead of 0, to avoid having to embed the 5 byte number value and it's shorter too.
	typedef enum
	{
		LDR_TAPE,
		LDR_MICRODRIVE,
		LDR_OPUS,
		LDR_HC_DISK,
		LDR_IF1_COM,
		LDR_SPECTRUM_P3_DISK,
		LDR_MGT,
		LDR_TRDOS
	} StorageLoaderType;

	//Tape:			LOAD "NAME"
	static const char LDR_TAPE_STR[];
	//Microdrive:	LOAD *"m";0;"NAME"
	static const char LDR_MICRODRIVE_STR[];
	//Opus:			LOAD *"m";0;"NAME"
	static const char LDR_OPUS_STR[];
	//HC disk:		LOAD *"d";0;"NAME"
	static const char LDR_HC_DISK_STR[];
	//IF1 COM:		LOAD *"b";
	static const char LDR_IF1_COM_STR[];
	//Spectrum +3:	LOAD "A:NAME"
	static const char LDR_SPECTRUM_P3_DISK_STR[];
	//MGT +D:		LOAD d1"NAME"; Microdrive synthax is also supported.
	static const char LDR_MGT_STR[];
	//TRDOS
	static const char LDR_TRDOS_STR[];

	static const char* STORAGE_LOADER_EXPR[8];	

	static const char* LDR_TAG[];


	static string Bas2Tap(string basFile, string programName, word autorunLine = 0, string varFileName = "");
	static bool ConvertBASICLoaderForDevice(CFileArchive* src, CFileArchiveTape* dst, StorageLoaderType ldrType);
	static vector<string> GetLoadedBlocksInProgram(byte* buf, word progLen, word varLen = 0);
	static bool Bin2REM(CFileArchive* theFS, string blobFileName, word addrForMove, string programName);
	static bool Bin2Var(CFileArchive* theFS, string blobFileName, word addrForMove, string programName);
	static string DecodeBASICProgramToText(byte* buf, word progLen, word varLen = 0);
	static string Disassemble(byte* buf, word len, word addr, bool useHex = true);
	static bool GenerateAutorun(CFileSystem* theFS);
};

