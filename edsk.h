#ifndef _EDSK_H_
#define _EDSK_H_

#include "types.h"
#include "dsk.h"

class CEDSK: public CDSK
{
public:		
	CEDSK();
	CEDSK(CDiskBase::DiskDescType);
	CEDSK(const CEDSK& src): CDSK(src) {}
	bool AddDiskHeader();
	bool Seek(byte trackNo);
	bool AddSectorHeaders(byte track, byte side);
	bool SeekSide(byte side);

protected:	
	static char EDSK_SIGNATURE[];
	static char DSK_CREATOR_NAME[];
	virtual char* GetSignature() {return EDSK_SIGNATURE;};
};

#endif //_EDSK_H_
