//IF1 9 bytes file header, same as HC disk file header.

#ifndef _CFILEIF1_H_
#define _CFILEIF1_H_

#include "types.h"

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
};

#endif 