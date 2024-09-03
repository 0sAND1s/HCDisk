#pragma once
#include "types.h"
#include <string>

using namespace std;

class FileUtil
{
public: 
	static string GetExtension(string fileName);
	static string GetNameWithoutExtension(string fileName);
	static long fsize(const char* fname);
	static bool BinCut(char* fnameIn, char* fnameOut, char* offsetStr, char* lenStr);
	static bool BinPatch(char* fnameIn, char* fnamePatch, char* offsetStr);	
	static bool BitMirror(char* fnameIn, char* fnameOut);
	static string GetHexPrint(byte* buff, dword len);
};

