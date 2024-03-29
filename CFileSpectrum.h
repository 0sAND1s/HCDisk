#ifndef _CFILESPECTRUM_H_
#define _CFILESPECTRUM_H_

#include "types.h"

class CFileSpectrum
{
public:
	enum SpectrumFileType
	{
		SPECTRUM_PROGRAM		= 0,
		SPECTRUM_NUMBER_ARRAY	= 1,
		SPECTRUM_CHAR_ARRAY		= 2,		
		SPECTRUM_BYTES			= 3,
		SPECTRUM_UNTYPED		= 4
	};	

	SpectrumFileType SpectrumType;
	word SpectrumStart;				//Code start address OR program start line
	word SpectrumLength;			//Program lenght WITH variables
	byte SpectrumArrayVarName;		//Char/number array variable
	word SpectrumVarLength;			//Variables lenght

	static char* SPECTRUM_FILE_TYPE_NAMES[];

	CFileSpectrum()
	{
		SpectrumType = SPECTRUM_UNTYPED;
		SpectrumStart = SpectrumLength = 0;
		SpectrumVarLength = SpectrumArrayVarName = 0;		
	}

	virtual SpectrumFileType GetType() 
	{ 		
		return ((byte)SpectrumType <= SPECTRUM_UNTYPED ? SpectrumType : SPECTRUM_UNTYPED); 
	};

	char GetArrVarName()
	{
		return 'A' + (SpectrumArrayVarName & 0x3F) - 1;
	}

	virtual CFileSpectrum& operator= (const CFileSpectrum& src)
	{
		this->SpectrumStart = src.SpectrumStart;
		this->SpectrumType = src.SpectrumType;
		this->SpectrumLength = src.SpectrumLength;
		this->SpectrumVarLength = src.SpectrumVarLength;
		this->SpectrumArrayVarName = src.SpectrumArrayVarName;

		return *this;
	}
};


#endif