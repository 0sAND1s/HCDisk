#include "BASICUtil.h"
#include "FileUtil.h"
#include "types.h"
#include "CFileArchiveTape.h"
#include "GUIUtil.h"

extern "C" {	
#include "FileConverters/bas2tap.h"
}

#include <Windows.h>

#include "FileConverters/BasicDecoder.h"
#include "FileConverters/dz80/dissz80.h"
#include "CFileSystem.h"
#include "CFileSpectrum.h"
#include "TapeUtil.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>

using namespace std;

//Tape:			LOAD "NAME"
const char BASICUtil::LDR_TAPE_STR[4] = { '"', '?', '"', 0 };
//Microdrive:	LOAD *"m";0;"NAME"
const char BASICUtil::LDR_MICRODRIVE_STR[12] = { '*', '"', 'm', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//Opus:			LOAD *"m";0;"NAME"
const char BASICUtil::LDR_OPUS_STR[12] = { '*', '"', 'm', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//HC disk:		LOAD *"d";0;"NAME"
const char BASICUtil::LDR_HC_DISK_STR[12] = { '*', '"', 'd', '"', ';', (char)Basic::BK_NOT, (char)Basic::BK_PI, ';', '"', '?', '"', 0 };
//IF1 COM:		LOAD *"b";
const char BASICUtil::LDR_IF1_COM_STR[5] = { '*', '"', 'b', '"', 0 };
//Spectrum +3:	LOAD "A:NAME"
const char BASICUtil::LDR_SPECTRUM_P3_DISK_STR[4] = { '"', '?', '"', 0 };
//MGT +D:		LOAD d1"NAME"; Microdrive synthax is also supported.
const char BASICUtil::LDR_MGT_STR[6] = { 'd', '1', '"', '?', '"', 0 };
//TRDOS
const char BASICUtil::LDR_TRDOS_STR[16] = { (char)Basic::BK_RANDOMIZE, (char)Basic::BK_USR, (char)Basic::BK_VAL, '1', '5', '6', '1', '9', ':', (char)Basic::BK_REM, ':', (char)Basic::BK_RUN, '"', '?', '"', 0 };

const char* BASICUtil::LDR_TAG[8] = { "TAPE", "MICRODRIVE", "OPUS", "HCDISK", "IF1COM", "PLUS3", "MGT", "TRDOS"};

const char* BASICUtil::STORAGE_LOADER_EXPR[8] =
{
	BASICUtil::LDR_TAPE_STR,
	BASICUtil::LDR_MICRODRIVE_STR,
	BASICUtil::LDR_OPUS_STR,
	BASICUtil::LDR_HC_DISK_STR,
	BASICUtil::LDR_IF1_COM_STR,
	BASICUtil::LDR_SPECTRUM_P3_DISK_STR,
	BASICUtil::LDR_MGT_STR,
	BASICUtil::LDR_TRDOS_STR
};



string BASICUtil::Bas2Tap(string basFile, string programName, word autorunLine, string varFileName)
{	
	bool res = bas2tap((char*)basFile.c_str(), programName.c_str(), autorunLine) == 0;

	//Add binary as variable.
	byte* varBuf = nullptr;
	word varBufLen = 0;
	if (res && varFileName.length() > 0)
	{		
		auto varLen = FileUtil::fsize(varFileName.c_str());
		const byte varHdr[] = { ((Basic::BASVarType::BV_STRING << 5) | ('x' - 'a' + 1)), (byte)(varLen % 256), (byte)(varLen / 256) };

		auto varFile = fopen(varFileName.c_str(), "rb");
		varBufLen = (word)(sizeof(varHdr) + varLen);
		varBuf = new byte[varBufLen];
		memcpy(varBuf, varHdr, sizeof(varHdr));
		fread(varBuf + sizeof(varHdr), 1, varLen, varFile);
		fclose(varFile);
	}

	char* tapToImport = "temp.tap";
	if (res)
	{
		//Open temporary file created by BasImp.
		CFileArchiveTape* tapeOld = new CFileArchiveTape(tapToImport);
		tapeOld->Open(tapToImport);
		tapeOld->Init();

		//Get program block blob and copy it into memory.
		auto oldProg = (CFileSpectrumTape*)tapeOld->FindFirst("*");
		word progLen = (word)oldProg->GetLength();
		byte* newProgBuf = new byte[progLen + varBufLen];
		oldProg->GetData(newProgBuf);
		if (varBufLen > 0)
		{
			memcpy(newProgBuf + progLen, varBuf, varBufLen);
			delete[] varBuf;
		}		

		//Create new program block and copy old program + new variables.
		CFileSpectrumTape* newProg = new CFileSpectrumTape(*oldProg);
		newProg->SetData(newProgBuf, progLen + varBufLen);
		newProg->SpectrumVarLength = varBufLen;
		newProg->SpectrumLength = progLen + varBufLen;
		delete[] newProgBuf;
		//delete oldProg;
		delete tapeOld;

		//Create new program tape.
		CFileArchiveTape* tapeNew = new CFileArchiveTape("newProg.tap");
		tapeNew->Open("newProg.tap", true);
		tapeNew->AddFile(newProg);
		tapeNew->Close();
		delete newProg;
		delete tapeNew;

		tapToImport = "newProg.tap";
		remove("temp.tap");
	}
	
	return tapToImport;
}

vector<string> BASICUtil::GetLoadedBlocksInProgram(byte* buf, word progLen, word varLen)
{
	Basic::BasicDecoder BasDec;
	word bufPos = 0;
	Basic::BasicLine bl;
	vector<string> blocksLoaded;

	do
	{
		bl = BasDec.GetBasicLineFromLineBuffer(buf + bufPos, progLen - varLen - bufPos);
		//Some loaders have extra data after the BASIC program that is not valid BASIC nor variables.
		if (bl.lineSize > 0)
		{
			auto loadsInLine = BasDec.GetLoadingBlocks(bl.lineBufBasic.data(), (word)bl.lineBufBasic.size());
			for (auto load : loadsInLine)
				blocksLoaded.push_back(load.second.second);
		}

		bufPos += bl.lineSize;
	} while (bl.lineSize > 0 && bufPos < progLen - varLen);

	return blocksLoaded;
}


//Automatic BASIC loader conversion for different storage devices (HC disk, IF1 serial, Spectrum disk, Opus, etc).
//Must first check that the tape is convertible:
//- it has the number of blocks with header (typed files) equal to the number of blocks present in the BASIC loader to be loaded
//- multilevel games have more blocks loaded by machine code (not present in the BASIC loader), so those games cannot be converted
//- if headerless blocks (untyped blocks) are present in the tape, it means machine code routine is loading blocks, so those games cannot be converted.
//If the BASIC loader is addressing blocks using empty strings, we must find the actual block names following the BASIC loader and use those in the loader, as empty names are not allowed on disk, even if are allowed on tapes.
//If the block names contain unprintable chars or are empty strings, we must convert names to printable chars and use those names in the BASIC loader, making sure names are unique, as duplicate names are not allowed on disk.	
bool BASICUtil::ConvertBASICLoaderForDevice(CFileArchive* src, CFileArchiveTape* dst, StorageLoaderType ldrType)
{
	if ((src->GetFSFeatures() & CFileArchive::FSFT_ZXSPECTRUM_FILES) == 0 ||
		(dst->GetFSFeatures() & CFileArchive::FSFT_ZXSPECTRUM_FILES) == 0)
	{
		puts("Can only convert loader for ZX Spectrum files.");
		return false;
	}

	bool hasHeaderless = false;
	Basic::BasicDecoder bd;
	CFileSpectrumTape* fileToCheck = (CFileSpectrumTape*)src->FindFirst("*");
	map<string, vector<string>> loadedBlocks;
	byte loadedBlocksCnt = 0, nonProgramBlocksCount = 0;
	string programName;

	while (fileToCheck != nullptr)
	{
		CFileArchive::FileNameType fileName;
		fileToCheck->GetFileName(fileName);

		if (fileToCheck->GetType() == CFileSpectrum::SPECTRUM_PROGRAM)
		{
			dword fileLen = fileToCheck->GetLength();
			byte* fileData = new byte[fileLen];
			fileToCheck->GetData(fileData);
			loadedBlocks[fileName] = GetLoadedBlocksInProgram(fileData, fileToCheck->SpectrumLength, fileToCheck->SpectrumVarLength);
			loadedBlocksCnt += (byte)loadedBlocks[fileName].size();
			delete[] fileData;
			programName = fileName;
		}
		else if (fileToCheck->GetType() != CFileSpectrum::SPECTRUM_UNTYPED)
		{
			//Update the loaded name to match the converted block name for destination TAP.
			if (nonProgramBlocksCount < loadedBlocks[programName].size())
				loadedBlocks[programName][nonProgramBlocksCount] = fileName;
			nonProgramBlocksCount++;
		}
		else
		{
			hasHeaderless = true;
		}

		fileToCheck = (CFileSpectrumTape*)src->FindNext();
	}

	if (hasHeaderless)
	{
		puts("Warning: The source has headerless/untyped blocks.");
	}

	if (loadedBlocksCnt < nonProgramBlocksCount)
	{
		printf("Warning: The BASIC loader(s) are referencing %d blocks, but %d non-program blocks are available on disk!\n", loadedBlocksCnt, nonProgramBlocksCount);
	}

	Basic::BasicLine blSrc, blDst;
	CFile* srcFile = src->FindFirst("*");
	while (srcFile != nullptr)
	{
		CFileSpectrumTape* srcFileSpec = (CFileSpectrumTape*)srcFile;
		CFileArchive::FileNameType fileName;
		srcFile->GetFileName(fileName);
		string programName;
		byte loadedBlockIndex = 0;

		if (srcFileSpec->GetType() == CFileSpectrum::SPECTRUM_PROGRAM)
		{
			word bufPosIn = 0, bufPosOut = 0;
			dword srcLen = srcFile->GetLength();
			byte* bufSrc = new byte[srcLen];
			byte* bufDst = new byte[srcLen + 100];

			srcFile->GetData(bufSrc);
			programName = fileName;
			loadedBlockIndex = 0;

			byte nameIdx = 0;
			do
			{
				blSrc = bd.GetBasicLineFromLineBuffer(bufSrc + bufPosIn, srcFileSpec->SpectrumLength - srcFileSpec->SpectrumVarLength - bufPosIn);
				if (blSrc.lineSize == 0)
					break;

				//Some BASIC loaders have more LOAD commands than the actual blocks.
				if (nameIdx < loadedBlocks[fileName].size())
				{
					bd.ConvertLoader(&blSrc, &blDst, BASICUtil::STORAGE_LOADER_EXPR[ldrType], &loadedBlocks[fileName], &nameIdx);
					bd.PutBasicLineToLineBuffer(blDst, bufDst + bufPosOut);

					bufPosIn += blSrc.lineSize;
					bufPosOut += blDst.lineSize;
				}
				else
				{
					memcpy(bufDst + bufPosOut, bufSrc + bufPosIn, blSrc.lineSize);
					bufPosIn += blSrc.lineSize;
					bufPosOut += blSrc.lineSize;
				}
			} while (bufPosIn < srcFileSpec->SpectrumLength - srcFileSpec->SpectrumVarLength);

			//Copy variables too.
			if (srcFileSpec->SpectrumVarLength > 0)
			{
				memcpy(bufDst + bufPosOut, bufSrc + bufPosIn, srcFileSpec->SpectrumVarLength);
				bufPosOut += srcFileSpec->SpectrumVarLength;
			}

			CFile* file = new CFile(fileName, bufPosOut, bufDst);
			CFileSpectrumTape* dstFile = new CFileSpectrumTape(*srcFileSpec, *file);
			dstFile->SpectrumLength = bufPosOut;
			dst->AddFile(dstFile);

			delete file;
			delete dstFile;

			if (bufSrc != nullptr)
				delete[] bufSrc;
			if (bufSrc != nullptr)
				delete[] bufDst;

		}
		else //Copy rest of blocks that are not program.
		{
			dst->AddFile(srcFileSpec);
		}

		srcFile = src->FindNext();
	}

	return true;
}

bool BASICUtil::Bin2REM(CFileArchive* theFS, string blobFileName, word addrForMove, string programName)
{
	const long blobSize = FileUtil::fsize(blobFileName.c_str());
	if (blobSize > 48 * 1024)
	{
		puts("Blob size is too big.");
		return false;
	}

	FILE* fBlob = fopen(blobFileName.c_str(), "rb");
	if (fBlob == NULL)
	{
		printf("Could not open blob file %s.", blobFileName.c_str());
		return false;
	}

	byte basLdrREM[] = {
		Basic::BASICKeywordsIDType::BK_RANDOMIZE,
		Basic::BASICKeywordsIDType::BK_USR,
		Basic::BASICKeywordsIDType::BK_VAL,
		'"', '3', '1', '+',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '5', '+', '2', '5', '6', '*',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '3', '6',
		'"', ':', Basic::BASICKeywordsIDType::BK_REM
	};
	word basLdrSize = sizeof(basLdrREM);

	byte asmLdr[] = {
		0x21, 14, 0,										//ld hl, lenght of ASM loader, the blob follows it
		0x09,												//add hl, bc	;BC holds the address called from RANDOMIZE USR.
		0x11, (byte)(addrForMove % 256), (byte)(addrForMove / 256),			//ld de, start addr
		0xD5,												//push de		;Jump to start of blob. 
		0x01, (byte)(blobSize % 256), (byte)(blobSize / 256),	//ld bc, blob size
		0xED, 0xB0,											//ldir
		0xC9												//ret		
	};
	word asmLdrSize = (addrForMove > 0 ? sizeof(asmLdr) : 0);

	const word ldrSize = basLdrSize + asmLdrSize;
	byte* blobBuf = new byte[ldrSize + blobSize];

	memcpy(blobBuf, basLdrREM, basLdrSize);
	//Copy LDIR loader in front of the blob.
	if (addrForMove > 0)
		memcpy(blobBuf + basLdrSize, asmLdr, asmLdrSize);

	fread(&blobBuf[ldrSize], 1, blobSize, fBlob);
	fclose(fBlob);

	//Create BASIC line from buffer.	
	const word progLineNo = 0;
	Basic::BasicLine bl(blobBuf, ldrSize + (word)blobSize, progLineNo);

	//Create header for Spectrum file.
	CFile* f = theFS->NewFile(programName.c_str());
	CFileSpectrum* fs = dynamic_cast<CFileSpectrum*>(f);
	fs->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
	fs->SpectrumStart = bl.lineNumber;
	fs->SpectrumVarLength = 0;
	fs->SpectrumArrayVarName = 0;
	fs->SpectrumLength = bl.lineSize;

	//Write BASIC line to buffer.
	Basic::BasicDecoder bd;
	byte* lastBuf = new byte[bl.lineSize];
	bd.PutBasicLineToLineBuffer(bl, lastBuf);
	f->SetData(lastBuf, bl.lineSize);
	delete[] lastBuf;
	delete[] blobBuf;

	bool res = theFS->WriteFile(f);
	delete f;

	return true;
}


bool BASICUtil::Bin2Var(CFileArchive* theFS, string blobFileName, word addrForMove, string programName)
{
	const long blobSize = FileUtil::fsize(blobFileName.c_str());
	if (blobSize > 48 * 1024)
	{
		puts("Blob size is too big.");
		return false;
	}

	FILE* fBlob = fopen(blobFileName.c_str(), "rb");
	if (fBlob == NULL)
	{
		printf("Could not open blob file %s.", blobFileName.c_str());
		return false;
	}

	byte asmLdr[] = {
		0x21, 14, 0,										//ld hl, lenght of ASM loader, the blob follows it
		0x09,												//add hl, bc	;BC holds the address called from RANDOMIZE USR.
		0x11, (byte)(addrForMove % 256), (byte)(addrForMove / 256),			//ld de, start addr
		0xD5,												//push de		;Jump to start of blob. 
		0x01, (byte)(blobSize % 256), (byte)(blobSize / 256),	//ld bc, blob size
		0xED, 0xB0,											//ldir
		0xC9												//ret		
	};
	word asmLdrSize = (addrForMove > 0 ? sizeof(asmLdr) : 0);

	byte varHdr[] = { ((Basic::BASVarType::BV_STRING << 5) | ('x' - 'a' + 1)), (byte)((blobSize + asmLdrSize) % 256), (byte)((blobSize + asmLdrSize) / 256) };
	const word varHdrSize = sizeof(varHdr);

	byte basLdrVar[] = {
		Basic::BASICKeywordsIDType::BK_RANDOMIZE,
		Basic::BASICKeywordsIDType::BK_USR,
		Basic::BASICKeywordsIDType::BK_VAL, '"',
		//Address of blob is variable area + 3 bytes for variable header
		(varHdrSize + '0'), '+',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '2', '7', '+', '2', '5', '6', '*',
		Basic::BASICKeywordsIDType::BK_PEEK, '2', '3', '6', '2', '8', '"',
		0x0D
	};
	word basLdrSize = sizeof(basLdrVar);

	const word ldrSize = basLdrSize + asmLdrSize;
	const word varLen = varHdrSize + asmLdrSize + (word)blobSize;
	byte* blobBuf = new byte[ldrSize + varHdrSize + (word)blobSize];

	//Copy basic loader.
	memcpy(blobBuf, basLdrVar, basLdrSize);
	//Copy variables header.
	memcpy(blobBuf + basLdrSize, varHdr, varHdrSize);

	if (addrForMove > 0)
		//Copy LDIR loader in front of the blob.
		memcpy(blobBuf + basLdrSize + varHdrSize, asmLdr, asmLdrSize);

	fread(&blobBuf[ldrSize + varHdrSize], 1, blobSize, fBlob);
	fclose(fBlob);

	//Create BASIC line from buffer.	
	const word progLineNo = 0;
	Basic::BasicLine bl(blobBuf, basLdrSize, progLineNo);

	//Create header for Spectrum file.
	CFile* f = theFS->NewFile(programName.c_str());
	CFileSpectrum* fs = dynamic_cast<CFileSpectrum*>(f);
	fs->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
	fs->SpectrumStart = bl.lineNumber;
	fs->SpectrumVarLength = varLen;
	fs->SpectrumArrayVarName = 0;
	fs->SpectrumLength = bl.lineSize + varLen;

	//Write BASIC line to buffer.
	Basic::BasicDecoder bd;
	byte* lastBuf = new byte[bl.lineSize + varLen];
	bd.PutBasicLineToLineBuffer(bl, lastBuf);
	memcpy(&lastBuf[bl.lineSize], &blobBuf[basLdrSize], varLen);
	f->SetData(lastBuf, bl.lineSize + varLen);
	delete[] lastBuf;
	delete[] blobBuf;

	bool res = theFS->WriteFile(f);
	delete f;

	return true;
}


string BASICUtil::DecodeBASICProgramToText(byte* buf, word progLen, word varLen)
{
	stringstream res;
	Basic::BasicDecoder BasDec;
	Basic::BasicLine bl;
	word bufPos = 0;
	list<string> blocksLoaded;

	do
	{
		bl = BasDec.GetBasicLineFromLineBuffer(buf + bufPos, progLen - varLen - bufPos);
		//Some loaders have extra data after the BASIC program that is not valid BASIC nor variables.
		if (bl.lineSize > 0)
		{
			BasDec.DecodeBasicLineToText(bl, res);

			auto loadsInLine = BasDec.GetLoadingBlocks(bl.lineBufBasic.data(), (word)bl.lineBufBasic.size());
			for (auto load : loadsInLine)
				blocksLoaded.push_back(load.second.second);
		}

		bufPos += bl.lineSize;
	} while (bl.lineSize > 0 && bufPos < progLen - varLen);

	if (varLen > 0)
		BasDec.DecodeVariables(&buf[progLen - varLen], varLen, res);

	//Display block names for blocks loaded by this BASIC block.	
	if (blocksLoaded.size() > 0)
		res << "Loading blocks: ";
	auto blIt = blocksLoaded.begin();
	while (blIt != blocksLoaded.end())
	{
		res << "\"" << *blIt << "\" ";
		blIt++;
	}
	res << endl;


	return res.str();
}


string BASICUtil::Disassemble(byte* buf, word len, word addr = 0, bool useHex)
{
	DISZ80* d;			/* Pointer to the Disassembly structure */
	int		err;		/* line count */
	string res;

	d = (DISZ80*)malloc(sizeof(DISZ80));
	if (d != NULL)
	{
		memset(d, 0, sizeof(DISZ80));
		dZ80_SetDefaultOptions(d);

		d->cpuType = DCPU_Z80;
		d->flags |= (DISFLAG_SINGLE | DISFLAG_ADDRDUMP | DISFLAG_OPCODEDUMP | DISFLAG_UPPER | DISFLAG_ANYREF | DISFLAG_RELCOMMENT | DISFLAG_VERBOSE);
		if (useHex)
			d->layoutRadix = DRADIX_HEX;

		//Fix address.
		byte* buf1 = new byte[64 * 1024];
		memcpy(&buf1[addr], buf, len);
		d->mem0Start = buf1;
		d->start = d->end = addr;
		err = dZ80_Disassemble(d);

		stringstream s;
		s.unsetf(ios::skipws);
		s.setf(ios::uppercase);
		while (err == DERR_NONE && len > 0)
		{
			if (useHex)
				s << hex;

			s << setw(4) << setfill('0') << addr << "\t" << setfill(' ') << setw(8) << d->hexDisBuf << "\t" << d->disBuf << "\t" << d->commentBuf << "\r\n";

			addr += (word)d->bytesProcessed;
			//Instruction/buffer boundary mismatch.
			if (d->bytesProcessed < len)
				len -= (word)d->bytesProcessed;
			else
				len = 0;
			d->start = d->end = addr;

			err = dZ80_Disassemble(d);
		}

		res = s.str();
		delete[] buf1;
	}

	free(d);
	return res;
}

bool BASICUtil::GenerateAutorun(CFileSystem* theFS)
{	
	if (theFS == nullptr)
		return false;

	bool IsHCDisk = (strstr(theFS->Name, "HC BASIC") != nullptr);
	bool IsPlus3Disk = (strstr(theFS->Name, "Spectrum +3") != nullptr);
	bool IsTRDOSDisk = (strstr(theFS->Name, "TRDOS") != nullptr);	

	if (!IsHCDisk && !IsPlus3Disk && !IsTRDOSDisk)
	{
		cout << "Can generate autorun file only for Spectrum+3, TRDOS, HC disks." << endl;
		return false;
	}

	vector<string> programNames;
	CFile* file = (CFile*)theFS->FindFirst("*");
	CFileArchive::FileNameType fn{};

	while (file != nullptr)
	{
		theFS->OpenFile(file);
		CFileSpectrum* fileSpectrum = dynamic_cast<CFileSpectrum*>(file);
		if (fileSpectrum->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
		{			
			//TRDOS needs only the file name.
			if (IsTRDOSDisk)
			{
				char ext[4];
				((CFile*)file)->GetFileName(fn, ext);
			}
			else
				((CFile*)file)->GetFileName(fn);
			programNames.push_back(fn);
		}

		file = theFS->FindNext();
	}

	if (programNames.size() == 0)
	{
		cout << "The disk doesn't contain any programs. " << endl;
		return false;
	}	

	const char* basLoaderTemplate = 
		"10 PAPER VAL\"0\": INK VAL\"2\": BORDER VAL\"0\": CLS\n"
		"20 PRINT #VAL\"0\";INK VAL\"2\";\"AUTORUN generated by HCDisk 2.0.\"\n"
		"30 LET n=%d\n"
		"40 FOR i=VAL \"1\" TO n\n"
		"50 READ n$: IF i<VAL\"10\" THEN PRINT \"0\";\n"
		"55 PRINT i; \".\"; n$,\n"
		"60 NEXT i\n"
		"65 PRINT \n"
		"70 PRINT \"Type program number: \";\n"
		"80 GO SUB VAL\"450\"\n"
		"90 LET d=c: PRINT c;\n"
		"100 GO SUB VAL\"450\"\n"
		"110 LET u = c: PRINT c\n"
		"120 LET itm = d * VAL\"10\" + u\n"
		"130 IF itm<VAL\"1\" OR itm>n THEN GO TO VAL\"70\"\n"
		"140 RESTORE: FOR i=VAL\"1\" TO itm: READ n$: NEXT i\n"
		"150 PRINT \"Loading \"; n$\n"
		"160 %s\n"
		"450 PAUSE VAL\"0\": LET c=CODE INKEY$: IF c<CODE \"0\" OR c>CODE \"9\" THEN GO TO VAL\"450\"\n"
		"455 LET c=c-CODE \"0\"\n"
		"460 RETURN\n"
		"500 DATA %s";	
	
	//Put program names in DATA.
	string progNamesStr;
	for (auto progName : programNames)
	{
		progNamesStr += "\"" + progName + "\",";
	}
	progNamesStr[progNamesStr.length() - 1] = '\n';
	
	char* bootFileName;
	theFS->GetAutorunFilename(&bootFileName);
	if (theFS->FindFirst(bootFileName))
	{		
		if (GUIUtil::Confirm("Autorun file already exists! Overwrite?"))
			theFS->Delete(bootFileName);
		else
			return false;
	}
	
	string diskLoadCmd;
	if (IsHCDisk)
		diskLoadCmd = "LOAD *\"d\";0;n$";
	else if (IsTRDOSDisk)
		diskLoadCmd = "RANDOMIZE USR 15619: REM: RUN n$";
	else if (IsPlus3Disk)
		diskLoadCmd = "LOAD n$";

	char* basLoader = new char[strlen(basLoaderTemplate) + 2 + diskLoadCmd.size() + programNames.size() * sizeof(CFileArchive::FileNameType)];
	sprintf(basLoader, basLoaderTemplate, programNames.size(), diskLoadCmd.c_str(), progNamesStr.c_str());
	const char* basFileName = "temp.bas";
	FILE* fBAS = fopen(basFileName, "wb");
	fwrite(basLoader, 1, strlen(basLoader), fBAS);
	fclose(fBAS);	
	delete[] basLoader;
	
	string basTapName = Bas2Tap(basFileName, bootFileName, 10);
	TapeUtil::ImportTape(theFS, basTapName.c_str());
	
	remove(basFileName);		
	remove(basTapName.c_str());

	return true;
}
