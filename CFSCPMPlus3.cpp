#include "CFSCPMPlus3.h"
#include "CFilePlus3.h"
#include <algorithm>

bool CFSCPMPlus3::ReadDirectory()
{
	if (CFSCPM::ReadDirectory())
	{
		Plus3FileList.clear();
		vector<CFileCPM>::iterator it = CPM_FileList.begin();
		while (it != CPM_FileList.end())
		{
			Plus3FileList.push_back(CFilePlus3(*it));
			it++;
		}

		return true;
	}
	else
		return false;
}


CFile* CFSCPMPlus3::FindNext() 
{ 			
	CFileCPM* f = (CFileCPM*)CFSCPM::FindNext();
	if (f != NULL)
	{
		vector<CFileCPM>::iterator it = find(CPM_FileList.begin(), CPM_FileList.end(), *f);
		return &Plus3FileList[it - CPM_FileList.begin()];
	}
	else
	{
		LastError = ERR_FILE_NOT_FOUND;
		return NULL;
	}
}

CFile* CFSCPMPlus3::NewFile(char* name, long len, byte* data)
{
	CFilePlus3* f = new CFilePlus3(this);
	memset(&f->TheHeader, 0, sizeof(f->TheHeader));
	f->SetFileName(name);
	if (len > 0 && data != NULL)
		f->SetData(data, len);
	return f; 
}

bool CFSCPMPlus3::ReadFile(CFile* file)
{
	CFilePlus3* fP3 = dynamic_cast<CFilePlus3*>(file);
	if (fP3->isOpened && CFSCPM::ReadFile(fP3))
	{
		if (fP3->hasHeader)
		{
			memcpy(fP3->buffer, fP3->buffer + sizeof(CFilePlus3::Plus3FileHeader), fP3->TheHeader.Lenght - sizeof(CFilePlus3::Plus3FileHeader));
			fP3->Length = fP3->TheHeader.Lenght;
		}
		return true;
	}
	else			
		return false;
}

bool CFSCPMPlus3::WriteFile(CFile* file)
{
	CFilePlus3* f = (CFilePlus3*)file;
	
	CFileSpectrum::SpectrumFileType st = f->GetType();
	if (st != CFileSpectrum::SPECTRUM_UNTYPED)
	{
		f->hasHeader = true;
		memcpy(f->TheHeader.Signature, CFilePlus3::SIGNATURE, sizeof(f->TheHeader.Signature));
		f->TheHeader.Issue = 1;
		f->TheHeader.Ver = 0;
		f->TheHeader.Lenght = f->GetLength() + sizeof(CFilePlus3::Plus3FileHeader);					

		f->TheHeader.BasHdr.Lenght = f->SpectrumLength; 
		f->TheHeader.BasHdr.Type = st;
		if (st == CFileSpectrum::SPECTRUM_PROGRAM && f->SpectrumLength > 0 && f->SpectrumLength <= f->GetLength())
		{			
			f->TheHeader.BasHdr.Start.ProgLine = f->SpectrumStart;
			f->TheHeader.BasHdr.VarStart = f->TheHeader.BasHdr.Lenght - f->SpectrumVarLength;
		}
		else if (st == CFileSpectrum::SPECTRUM_BYTES && f->SpectrumLength > 0 && f->SpectrumLength <= f->GetLength())
		{
			f->TheHeader.BasHdr.Start.CodeStart = f->SpectrumStart;
		}
		else if ((st == CFileSpectrum::SPECTRUM_CHAR_ARRAY || st == CFileSpectrum::SPECTRUM_NUMBER_ARRAY) &&
			f->SpectrumLength > 0 && f->SpectrumLength <= f->GetLength())
		{			
			f->TheHeader.BasHdr.Start.Var.Name = f->SpectrumArrayVarName;
		}	

		f->TheHeader.Checksum = f->TheHeader.GetChecksum();

		f->Length = f->TheHeader.Lenght;

		byte* buf = new byte[f->Length];
		f->GetData(buf + sizeof(f->TheHeader));
		memcpy(buf, &f->TheHeader, sizeof(f->TheHeader));
		f->SetData(buf, f->Length);
		delete buf;		
	}
	else
	{
		//f->Length = this->GetFileSize(f, false);
		f->Length = file->GetLength();
		f->hasHeader = false;		
	}

	return CFSCPM::WriteFile(file);
}

bool CFSCPMPlus3::OpenFile(CFile* file)
{	
	CFilePlus3* f =(CFilePlus3*)file;
	byte* buf = new byte[FSParams.BlockSize];	
	if (CFSCPM::OpenFile(file) && f->Length > 0)
	{	
		if (f->FileBlocks.size() > 0 && ReadBlock(buf, f->FileBlocks[0]))			
		{
			CFilePlus3::Plus3FileHeader* hdr = (CFilePlus3::Plus3FileHeader*)buf;
			//Check if it's a valid header.
			if (memcmp(hdr->Signature, CFilePlus3::SIGNATURE, sizeof(CFilePlus3::SIGNATURE)) == 0 && 
				hdr->Checksum == hdr->GetChecksum())
			{
				f->TheHeader = *hdr;			
				f->Length = f->TheHeader.Lenght - sizeof(CFilePlus3::Plus3FileHeader);			
				f->hasHeader = true;
				f->SpectrumVarLength = 0;

				//BASIC file header initialization.			
				f->SpectrumLength = f->TheHeader.BasHdr.Lenght; 

				if (f->TheHeader.BasHdr.Type == CFileSpectrum::SPECTRUM_PROGRAM && f->SpectrumLength > 0 && f->SpectrumLength <= f->Length)
				{
					f->SpectrumType = CFileSpectrum::SPECTRUM_PROGRAM;
					f->SpectrumStart = f->TheHeader.BasHdr.Start.ProgLine;
					f->SpectrumVarLength = f->TheHeader.BasHdr.Lenght - f->TheHeader.BasHdr.VarStart;
				}
				else if (f->TheHeader.BasHdr.Type == CFileSpectrum::SPECTRUM_BYTES && f->SpectrumLength > 0 && f->SpectrumLength <= f->Length)
				{	
					f->SpectrumType = CFileSpectrum::SPECTRUM_BYTES;
					f->SpectrumStart = f->TheHeader.BasHdr.Start.CodeStart;
				}
				else if ((f->TheHeader.BasHdr.Type == CFileSpectrum::SPECTRUM_CHAR_ARRAY || 
					f->TheHeader.BasHdr.Type == CFileSpectrum::SPECTRUM_NUMBER_ARRAY) &&
					f->SpectrumLength > 0 && f->SpectrumLength <= f->Length)
				{
					f->SpectrumType = (CFileSpectrum::SpectrumFileType)f->TheHeader.BasHdr.Type;				
					f->SpectrumArrayVarName = f->TheHeader.BasHdr.Start.Var.Name;
				}						
			}
			else
			{			
				f->Length = GetFileSize(f, false);
				f->hasHeader = false;
				f->SpectrumType = CFileSpectrum::SPECTRUM_UNTYPED;
			}
		}		
	}

	delete buf;
	return f->isOpened;
}