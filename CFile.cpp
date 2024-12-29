#include <memory.h>
#include <stdio.h>
#include <algorithm>
#include "CFile.h"
#include "CFSCPM.h"


CFile::CFile()
{	
	Length = 0;
	buffer = NULL;
	isOpened = false;
	FileName[0] = Name[0] = Extension[0] = NULL;
}

CFile::CFile(char* name, dword length, const byte* buffer)
{
	strncpy(FileName, name, sizeof(FileName));		
	Name[0] = Extension[0] = NULL;
	Length = length;
	this->buffer = new byte[length];
	memcpy(this->buffer, buffer, length);
	isOpened = false;
}

CFile::~CFile()
{
	if (buffer != NULL)
	{
		delete[] buffer;
		buffer = NULL;
	}
};

CFile::CFile(const CFile& src)
{
	strncpy(this->FileName, src.FileName, sizeof(this->FileName));	
	strncpy(this->Name, src.Name, sizeof(this->FileName));	
	strncpy(this->Extension, src.Extension, sizeof(this->Extension));
	
	this->FileBlocks = src.FileBlocks;
	this->FileDirEntries = src.FileDirEntries;

	this->Length = src.Length;
	if (src.buffer != NULL)
	{
		this->buffer = new byte[src.Length];
		memcpy(buffer, src.buffer, src.Length);		
	}	
	else
		this->buffer = NULL;
}


bool CFile::operator== (const CFile& src)
{	
	return strcmp(this->FileName, src.FileName) == 0;
}

CFile& CFile::operator= (const CFile& src)
{
	strcpy(this->FileName, src.FileName);
	strcpy(this->Name, src.Name);
	strcpy(this->Extension, src.Extension);
	
	this->Length = src.Length;
	this->FileBlocks = src.FileBlocks;
	this->FileDirEntries = src.FileDirEntries;

	if (src.buffer != NULL)
	{
		this->buffer = new byte[src.Length];
		memcpy(buffer, src.buffer, src.Length);		
	}	
	else
		this->buffer = NULL;

	return *this;
}

bool CFile::GetData(byte* buf)
{	
	if (buf != NULL)		
	{
		memcpy(buf, this->buffer, GetLength());
		return true;
	}
	else
		return false;		
}

bool CFile::SetData(byte* buf, dword length)
{
	if (buf != NULL && length > 0)
	{
		Length = length;
		buffer = new byte[length];
		memcpy(buffer, buf, length);
		return true;
	}
	else
		return false;
}

dword CFile::GetDataAsText(byte* bufOut, byte wrapOff)
{	
	dword txtLen = GetLength();		
	dword o = 0, i = 0, crlfIdx = 0;
	bool eof = false;

	while (i < txtLen && !eof)
	{				
		bufOut[o++] = buffer[i] & 0x7F;	
		crlfIdx++;
		if (buffer[i] == '\r' || buffer[i] == '\n')
			crlfIdx = 0;

		if (wrapOff > 0 && (crlfIdx > wrapOff) && ((i + 1) % wrapOff == 0))
		{
			bufOut[o++] = '\r';			
			bufOut[o++] = '\n';			
		}

		eof = (buffer[i] == CFSCPM::EOF_MARKER);		

		i++;
	}			

	return o - eof;
}

bool CFile::GetFileName(char* dest, bool trim)
{ 			
	memset(dest, ' ', sizeof(CFileArchive::FileNameType));
	dest[sizeof(CFileArchive::FileNameType) - 1] = '\0';
	strcpy(dest, FileName);

	if (trim)
		CFileArchive::TrimEnd(dest);
	/*
	if (strlen(Extension) > 0)
	{
		strcat(dest, ".");
		strcat(dest, Extension);
		CFileArchive::TrimEnd(dest);
	}
	*/

	return true;
};

bool CFile::GetFileName(char* name, char* ext)
{
	if (strlen(Extension) > 0)
	{
		return strcpy(name, Name) && strcpy(ext, Extension) && CFileArchive::TrimEnd(name) && CFileArchive::TrimEnd(ext);
	}
	else
	{
		*ext = '\0';
		return strcpy(name, Name) != nullptr;
	}
}

bool CFile::SetFileName(const char* src)
{
	bool res = true;

	if (src == NULL || strlen(src) == 0)
		res = false;	

	if (res)
	{
		memset(Name, ' ', sizeof(Name));
		memset(Extension, ' ', sizeof(Extension));
		Name[sizeof(Name) - 1] = '\0';
		Extension[sizeof(Extension) - 1] = '\0';

		const char* dot = strrchr(src, '.');
		if (dot != NULL && strlen(dot) <= 3+1 && strlen(dot) > 1)
		{
			word extLen = (word)strlen(dot+1);
			res = memcpy(Name, src, dot - src) != NULL && memcpy(Extension, dot+1, extLen) != NULL &&
				strncpy(FileName, src, sizeof(FileName)-1);
		}
		else
		{
			res = memcpy(Name, src, strlen(src)) != NULL &&
				strncpy(FileName, src, sizeof(FileName) - 1);
		}
	}	

	return res;
}

bool CFile::SetFileName(const char* name, const char* ext)
{
	if (name == NULL || strlen(name) == 0)
		return false;
	else
	{
		if (ext != NULL && strlen(Extension) > 0)
			return strcpy(Name, name) && strcpy(Extension, ext);
		else
			return sprintf(Name, "%s", name) != NULL; 	
	}	
}
