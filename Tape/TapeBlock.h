//Class than completely contains and describes a tape block.
#ifndef _TAPE_BLOCK_H
#define _TAPE_BLOCK_H

#include "types.h"

class CTapeBlock
{
public:		
#pragma pack(1)	
	static const byte TAPE_BLOCK_FLAG_HEADER = 0x00;
	static const byte TAPE_BLOCK_FLAG_DATA = 0xFF;
	static const word TAPE_UNUSED_PARAM_VAL = 1<<15;	
	static const dword TAPE_BLOCK_MAX_LEN = 1<<16;

	//Tape header block name
	static const byte TAP_FILENAME_LEN = 10;
	typedef char TapeBlockName[TAP_FILENAME_LEN];

	//BASIC tape block type
	typedef enum
	{
		TAPE_BLK_BAS_HDR_PROGRAM,
		TAPE_BLK_BAS_HDR_NR_ARR,
		TAPE_BLK_BAS_HDR_CHR_ARR,
		TAPE_BLK_BAS_HDR_CODE,		

		TAP_BLOCK_TYPE_COUNT
	} TapeBlockHeaderBasicType;

	//a special kind of block is the header
	typedef struct
	{
		byte BlockType;
		TapeBlockName FileName;
		word Length;
		word Param1;
		word Param2;
	} TapeBlockHeaderBasic;			

	static const char* CTapeBlock::TapBlockTypeNames [CTapeBlock::TAP_BLOCK_TYPE_COUNT];

	//Length in T-states of each pulse type
	typedef struct  
	{
		word Pilot;			//Pilot pulse length
		word PilotPulseCnt;	//Pilot pulse count
		word Sync1;			//Sync1 pulse length
		word Sync2;			//Sync2 pulse length
		word Bit0;			//Bit 0 pulse length
		word Bit1;			//Bit 1 pulse length
		word Pause;			//Inter-block pause length (ms)
	} TapeTimgins;	
		
	//Tape blocks for samples
	typedef enum
	{
		TAPE_BLK_INVALID,
		TAPE_BLK_METADATA,
		TAPE_BLK_PAUSE,
		TAPE_BLK_STD,				//The only one that can be stored in TAP.
		TAPE_BLK_TURBO,
		TAPE_BLK_PURE_TONE,
		TAPE_BLK_PULSE_SEQ,
		TAPE_BLK_PURE_DATA,
		TAPE_BLK_DIRECT_REC,
		TAPE_BLK_CSW_REC,
		TAPE_BLK_GENERALIZED
	} TapeBlockType;


	dword Length;			//Length containing flag and checksum bytes
	byte* Data;
	byte Flag;	
	byte Checksum;
	byte LastBitCnt;

	TapeBlockType m_BlkType;
	TapeTimgins m_Timings;

	typedef struct  
	{
		word pulseLen;
		word pulseCnt;
	} TapeBlockPureTone;

	typedef struct  
	{
		byte pulseCnt;
		word pulseLen[0xFF];
	} TapeBlockPulseSequence;	
	

	 TapeBlockPureTone m_PureTone;
	 TapeBlockPulseSequence m_PulseSeq;

	static const TapeTimgins ROM_TIMINGS_HEAD;	
	static const TapeTimgins ROM_TIMINGS_DATA;	
	word Index;
#pragma pack()

	CTapeBlock();
	CTapeBlock(const CTapeBlock& src);
	CTapeBlock& operator =(const CTapeBlock& src);
	virtual ~CTapeBlock();		

	byte GetBlockChecksum();	
	bool IsBlockHeader();
	bool GetName(char * szName);
	bool SetName(const char * szName);
};


#endif//_TAPE_BLOCK_H