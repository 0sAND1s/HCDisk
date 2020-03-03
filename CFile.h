//Base class for file objects. Will hold the data buffer and in derived classes, the file connections with the file system.
#ifndef CFILE_H
#define CFILE_H

#include <string.h>
#include <vector>
#include "types.h"

using namespace std;
class CFileSystem;

class CFile
{
	friend class CFileArchive;
	friend class CFileSystem;

public:		
	CFile();
	CFile(const CFile& src);
	CFile(char* name, dword length, const byte* buffer);
	~CFile();

	virtual bool operator== (const CFile&);
	virtual bool operator!= (const CFile& src) { return !(this->operator==(src));};
	virtual CFile& operator= (const CFile&);


	virtual dword GetLength() { return Length; }
	virtual bool GetData(byte* buf);	
	virtual dword GetDataAsText(byte* buf, byte wrapOff = 80);
	virtual bool SetData(byte* buf, dword length);

	//Returns the internal file name.
	virtual bool GetFileName(char* dest);
	virtual bool GetFileName(char* name, char* ext);
	virtual bool SetFileName(char* src);
	virtual bool SetFileName(char* name, char* ext);	

protected:	
	//CFileSystem* fs;
	bool isOpened;
	vector<word> FileBlocks;
	vector<word> FileDirEntries;
	dword Length;
	byte* buffer;	
	char FileName[13];
	char Name[13];
	char Extension[4];	
};
#endif//CFILE_H