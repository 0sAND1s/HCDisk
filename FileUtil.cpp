#include <string>
#include <iostream>
#include <conio.h>

#include "FileUtil.h"

using namespace std;

string FileUtil::GetExtension(string fileName)
{
	int dotIdx = fileName.find_last_of('.');
	string res = "";
	if (dotIdx != string::npos)
		res = fileName.substr(dotIdx + 1);
	if (res.length() <= 3)
		return res;
	else
		return "";
}

string FileUtil::GetNameWithoutExtension(string fileName)
{
	int dotIdx = fileName.find_last_of('.');
	string res = "";
	if (dotIdx != string::npos)
		res = fileName.substr(0, dotIdx);
	return res;
}

long FileUtil::fsize(const char* fname)
{
	long fs;

	FILE* f = fopen(fname, "rb");
	if (f != NULL)
	{
		fseek(f, 0, SEEK_END);
		fs = ftell(f);
		fclose(f);
	}
	else
		fs = 0;

	return fs;
}

bool FileUtil::BinCut(char* fnameIn, char* fnameOut, char* offsetStr, char* lenStr)
{	
	long offset = atoi(offsetStr);
	long fsizeIn = FileUtil::fsize(fnameIn);
	long lenBlock = atoi(lenStr);

	if (lenBlock == 0)
		lenBlock = fsizeIn - offset;

	if (offset + lenBlock > fsizeIn)
	{
		cout << offset << " + " << lenBlock << " do not fit the file " << fnameIn << " lenght of " << fsizeIn << "." << endl;
		return false;
	}

	FILE* fin = fopen(fnameIn, "rb");
	FILE* fout = fopen(fnameOut, "wb");
	if (fin == nullptr || fout == nullptr)
	{
		cout << "Couln't open input or output file." << endl;
		return false;
	}

	fseek(fin, offset, SEEK_SET);
	long leftBytes = lenBlock;
	while (leftBytes > 0)
	{
		fputc(fgetc(fin), fout);
		leftBytes--;
	}

	fclose(fin);
	fclose(fout);

	cout << "Created file " << fnameOut << " with lenght " << lenBlock << "." << endl;

	return true;
}


bool FileUtil::BinPatch(char* fnameIn, char* fnamePatch, char* offsetStr)
{			
	long offset = atoi(offsetStr);
	long fsizeIn = FileUtil::fsize(fnameIn);
	long fsizePatch = FileUtil::fsize(fnamePatch);


	FILE* fToPatch = fopen(fnameIn, "r+b");
	FILE* fThePatch = fopen(fnamePatch, "rb");
	if (fToPatch == nullptr || fThePatch == nullptr)
	{
		cout << "Couln't open input or output file." << endl;
		return false;
	}

	fseek(fToPatch, offset, SEEK_SET);
	long leftBytes = fsizePatch;
	while (leftBytes > 0)
	{
		fputc(fgetc(fThePatch), fToPatch);
		leftBytes--;
	}

	fclose(fToPatch);
	fclose(fThePatch);

	cout << "Applied patch file " << fnamePatch << " with lenght " << fsizePatch << " to file " << fnameIn << " at offset " << offset << "." << endl;

	return true;
}


string FileUtil::GetHexPrint(byte* buff, dword len)
{
	string res;

	char buf[80] = {};
	byte bufIdx = 0;
	dword lenEven = (dword)ceil((float)len / 16) * 16;

	for (dword c = 0; c < lenEven; c++)
	{
		if (c % 16 == 0)
			bufIdx += sprintf(&buf[bufIdx], "%04X: ", c);

		if (c < len)
			bufIdx += sprintf(&buf[bufIdx], "%02X ", buff[c]);
		else
			bufIdx += sprintf(&buf[bufIdx], "   ");

		if ((c + 1) % 16 == 0)
		{
			bufIdx += sprintf(&buf[bufIdx], "| ");
			for (int a = 0; a < 16; a++)
			{
				byte chr = buff[c - 15 + a];
				if (chr < ' ' || chr > 128)
					chr = '.';
				bufIdx += sprintf(&buf[bufIdx], "%c", (c - 15 + a < len ? chr : ' '));
			}

			bufIdx += sprintf(&buf[bufIdx], "\r\n");

			res += string(buf);
			bufIdx = 0;
		}
		else if ((c + 1) % 8 == 0)
			bufIdx += sprintf(&buf[bufIdx], "- ");
	}

	return res;
}
