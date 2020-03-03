#include "TapeBlock.h"
#include "string.h"

//Use short lead tone.
//const CTapeBlock::TapeTimgins CTapeBlock::ROM_TIMINGS_HEAD = {2168, 8063, 667, 735, 855, 1710, 1000};
const CTapeBlock::TapeTimgins CTapeBlock::ROM_TIMINGS_HEAD = {2168, 3223, 667, 735, 855, 1710, 1000};
const CTapeBlock::TapeTimgins CTapeBlock::ROM_TIMINGS_DATA = {2168, 3223, 667, 735, 855, 1710, 1000};

const char* CTapeBlock::TapBlockTypeNames [CTapeBlock::TAP_BLOCK_TYPE_COUNT] =
{
	"Program",
	"Number Array",
	"Character Array",
	"Bytes",	
};																	   


CTapeBlock::CTapeBlock()
{	
	Data = 0;	
	Length = 0;	
}

CTapeBlock::CTapeBlock(const CTapeBlock& src)
{
	Length = src.Length;
	m_BlkType = src.m_BlkType;
	m_Timings = src.m_Timings;
	Flag = src.Flag;
	Checksum = src.Checksum;

	Data = new byte[src.Length];
	memcpy(Data, src.Data, src.Length);
}

CTapeBlock& CTapeBlock::operator=(const CTapeBlock& src)
{
	Length = src.Length;
	m_BlkType = src.m_BlkType;
	m_Timings = src.m_Timings;	
	Flag = src.Flag;
	Checksum = src.Checksum;

	Data = new byte[src.Length];
	memcpy(Data, src.Data, src.Length);

	return *this;
}

CTapeBlock::~CTapeBlock()
{	
	if (Length > 0 && Data != NULL)
	{
		delete Data;
		Data = NULL;
		Length = 0;
	}	
}

byte CTapeBlock::GetBlockChecksum()
{
	byte CheckSum = Flag;
	for (word i = 0; i<Length - sizeof(Flag) - sizeof(Checksum); i++)
		CheckSum ^= Data[i];

	return CheckSum;
}

bool CTapeBlock::IsBlockHeader()
{
	return (Flag == TAPE_BLOCK_FLAG_HEADER) && 
		(Length - sizeof(Flag) - sizeof(Checksum) == 17/*sizeof(TapeBlockHeaderBasic)*/); 		
}



bool CTapeBlock::GetName(char * szName)
{
	if (IsBlockHeader())
	{
		memcpy(szName, ((TapeBlockHeaderBasic*)Data)->FileName, TAP_FILENAME_LEN);
		szName[TAP_FILENAME_LEN] = 0;

		for (byte i=0; i<TAP_FILENAME_LEN; i++)
			if (szName[i] < 32 ||  szName[i] > 127)
				szName[i] = '_';

		return true;
	}

	return false;
}


bool CTapeBlock::SetName(const char * szName)
{
	memset(((TapeBlockHeaderBasic*)Data)->FileName, ' ', TAP_FILENAME_LEN);	
	for (byte i = 0; i < TAP_FILENAME_LEN && szName[i] != '\0'; i++)
		if (szName[i] < 32 ||  szName[i] > 127)
			((TapeBlockHeaderBasic*)Data)->FileName[i] = '_';
		else
			((TapeBlockHeaderBasic*)Data)->FileName[i] = *(szName + i);

	return true;
}
