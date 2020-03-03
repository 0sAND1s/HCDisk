#include "CFSCPMHC.h"
#include "CFileHC.h"
#include "CFileIF1.h"
#include <algorithm>

bool CFSCPMHC::IsCharValidForFilename(char c)
{
	return c >= ' ' && c < 128 && strchr("\"*?\\/", c) == NULL;
}

bool CFSCPMHC::ReadDirectory()
{
	if (CFSCPM::ReadDirectory())
	{
		HCFileList.clear();
		vector<CFileCPM>::iterator it = CPM_FileList.begin();
		while (it != CPM_FileList.end())
		{
			HCFileList.push_back(CFileHC(*it));
			it++;
		}

		return true;
	}
	else
		return false;
}

CFile* CFSCPMHC::FindNext()
{ 
	CFileCPM* f = (CFileCPM*)CFSCPM::FindNext();
	if (f != NULL)
	{
		vector<CFileCPM>::iterator it = find(CPM_FileList.begin(), CPM_FileList.end(), *f);
		return &HCFileList[it - CPM_FileList.begin()];
	}
	else
	{
		LastError = ERR_FILE_NOT_FOUND;
		return NULL;
	}
}

CFile* CFSCPMHC::NewFile(char* name, long len, byte* data)
{
	CFileHC* f = new CFileHC(this);
	memset(&f->IF1Header, 0, sizeof(f->IF1Header));
	f->SetFileName(name);
	if (data != NULL && len > 0)
		f->SetData(data, len);
	return f;
}

bool CFSCPMHC::WriteFile(CFile* file)
{
	bool res = true;
	CFileHC* f = (CFileHC*)file;
	f->User = 0;

	if (f->GetType() != CFileSpectrum::SPECTRUM_UNTYPED)
	{
		f->IF1Header.Type = (byte)f->SpectrumType;
		f->IF1Header.Length = f->SpectrumLength;
		if (f->SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
		{
			f->IF1Header.ProgLineNumber = f->SpectrumStart;
			f->IF1Header.ProgLen = f->SpectrumLength - f->SpectrumVarLength;		
		}
		else if (f->SpectrumType == CFileSpectrum::SPECTRUM_BYTES)
			f->IF1Header.CodeStart = f->SpectrumStart;	
		else if (f->SpectrumType == CFileSpectrum::SPECTRUM_CHAR_ARRAY ||
			f->SpectrumType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
			f->IF1Header.VarName = f->SpectrumArrayVarName;

		byte* newBuf = new byte[f->Length + sizeof(CFileIF1::CFileIF1Header)];
		memcpy(newBuf, &f->IF1Header, sizeof(CFileIF1::CFileIF1Header));
		memcpy(newBuf + sizeof(CFileIF1::CFileIF1Header), f->buffer, f->Length);	
		delete f->buffer;
		f->SetData(newBuf, f->Length + sizeof(CFileIF1::CFileIF1Header));
		res = CFSCPM::WriteFile(f);
		delete newBuf;
	}
	else
	{
		res = CFSCPM::WriteFile(f);
	}
	
	return res;
}

bool CFSCPMHC::ReadFile(CFile* f)
{	
	CFileHC* fHC = dynamic_cast<CFileHC*>(f);

	bool res = CFSCPM::ReadFile(fHC);
	if (res)
	{
		if (fHC->GetType() != CFileSpectrum::SPECTRUM_UNTYPED)
		{
			memmove(fHC->buffer, fHC->buffer + sizeof(CFileIF1), fHC->Length - sizeof(CFileIF1));			
		}	
	}	

	return res;
}

/*
bool CFSCPMHC::CreateFSName(CFile* file, char* fNameOut)
{
	return file->GetFileName(fNameOut);
}
*/

bool CFSCPMHC::OpenFile(CFile* file)
{	
	CFileHC* f = (CFileHC*)file;
	byte* buf = new byte[FSParams.BlockSize];	
	if (CFSCPM::OpenFile(f) && f->GetLength() > 0)
	{		
		if (f->FileBlocks.size() > 0 && ReadBlock(buf, f->FileBlocks[0], 1))
		{
			f->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
			f->IF1Header = *(CFileIF1::CFileIF1Header*)buf;				
			f->Length = GetFileSize(f, true);

			if (f->IF1Header.Type < CFileSpectrum::SPECTRUM_UNTYPED)
			{			
				if (f->IF1Header.Type == CFileSpectrum::SPECTRUM_PROGRAM)
				{				
					f->SpectrumLength = f->IF1Header.Length;
					if (f->SpectrumLength > 0 && f->SpectrumLength <= f->IF1Header.Length && f->SpectrumLength <= f->Length)
					{
						f->SpectrumStart = f->IF1Header.ProgLineNumber;
						f->SpectrumVarLength = f->IF1Header.Length - f->IF1Header.ProgLen;
						f->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
					}				
				}
				else if (f->IF1Header.Type == CFileSpectrum::SPECTRUM_BYTES)
				{
					f->SpectrumLength = f->IF1Header.Length;	
					if (f->SpectrumLength > 0 && f->SpectrumLength <= f->Length)
					{
						f->SpectrumStart = f->IF1Header.CodeStart;						
						f->SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
					}				
				}
				else if (f->IF1Header.Type == CFileSpectrum::SPECTRUM_CHAR_ARRAY || 
					f->IF1Header.Type == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
				{
					f->SpectrumLength = f->IF1Header.Length;
					if (f->SpectrumLength > 0 && f->SpectrumLength <= f->Length)
					{
						f->SpectrumType = (CFileSpectrum::SpectrumFileType)f->IF1Header.Type;
						f->SpectrumArrayVarName = f->IF1Header.VarName;							
					}				
				}			
			}						
			else
				f->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;	
		}
	}

	delete buf;		
	return f->isOpened;
}