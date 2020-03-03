#ifndef _CFILEPLUS3_
#define _CFILEPLUS3_

#include "types.h"
#include "CFileCPM.h"
#include "CFileSpectrum.h"
#include "CFSCPM.h"
#include <memory>

class CFilePlus3: public CFileCPM, public CFileSpectrum
{
	friend class CFSCPMPlus3;	
	

public:
#pragma pack(1)
	static const char SIGNATURE[9];

	typedef struct  
	{
		byte Type;
		word Lenght;
		union
		{
			word CodeStart;
			word ProgLine;
			struct
			{
				byte Unused;
				byte Name;
			} Var;
		} Start;		
		word VarStart;		
	} Plus3BasHeader;

	typedef struct
	{
		char	Signature[9];		
		byte	Issue;
		byte	Ver;
		dword	Lenght;
		Plus3BasHeader BasHdr;
		byte	Unused[105];
		byte	Checksum;		

		byte GetChecksum() 
		{
			byte cs = 0;
			for (byte i = 0; i < sizeof(Plus3FileHeader) - 1; i++)
				cs += *((byte*)this + i);

			return cs;
		};
	} Plus3FileHeader;

	Plus3FileHeader TheHeader;
#pragma pack()
	
	CFilePlus3(CFSCPM*);
	CFilePlus3(const CFilePlus3&);
	CFilePlus3(const CFileCPM&);
	virtual dword GetLength();

protected:
	bool hasHeader;
};

#endif// _CFILEPLUS3_