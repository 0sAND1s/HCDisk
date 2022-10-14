//IF1 9 bytes file header, same as HC disk file header.

#ifndef _CFILEIF1_H_
#define _CFILEIF1_H_

#include "types.h"
#include "CFileSpectrum.h"

class CFileIF1
{
public:
#pragma pack(1)
	struct CFileIF1Header
	{
		byte Type;
		word Length;
		word CodeStart;
		union
		{
			word ProgLen;
			struct  
			{
				char VarName;
				byte unused;				
			};
		};
		word ProgLineNumber;
	};
#pragma pack()

	CFileIF1Header IF1Header;

	CFileIF1::CFileIF1()
	{}

	CFileIF1::CFileIF1(const CFileSpectrum& fs)
	{		
		IF1Header.Type = (byte)fs.SpectrumType;
		IF1Header.Length = fs.SpectrumLength;
		if (fs.SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
		{
			IF1Header.ProgLineNumber = fs.SpectrumStart;
			IF1Header.ProgLen = fs.SpectrumLength - fs.SpectrumVarLength;
		}
		else if (fs.SpectrumType == CFileSpectrum::SPECTRUM_BYTES)
			IF1Header.CodeStart = fs.SpectrumStart;
		else if (fs.SpectrumType == CFileSpectrum::SPECTRUM_CHAR_ARRAY || fs.SpectrumType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
			IF1Header.VarName = fs.SpectrumArrayVarName;		
	}

	operator CFileSpectrum() const
	{
		CFileSpectrum fs;
		
		fs.SpectrumType = (CFileSpectrum::SpectrumFileType)IF1Header.Type;
		fs.SpectrumLength = IF1Header.Length;
		if (fs.SpectrumType == CFileSpectrum::SPECTRUM_PROGRAM)
		{
			fs.SpectrumStart = IF1Header.ProgLineNumber;
			fs.SpectrumVarLength = fs.SpectrumLength - IF1Header.ProgLen;
		}
		else if (fs.SpectrumType == CFileSpectrum::SPECTRUM_BYTES)
			fs.SpectrumStart = IF1Header.CodeStart;
		else if (fs.SpectrumType == CFileSpectrum::SPECTRUM_CHAR_ARRAY || fs.SpectrumType == CFileSpectrum::SPECTRUM_NUMBER_ARRAY)
			fs.SpectrumArrayVarName = IF1Header.VarName;

		return fs;
	}
};

#endif 