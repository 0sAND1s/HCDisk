//////////////////////////////////////////////////////////////////////////
//TZX file driver, (C) George Chirtoaca, 2014
//Specifications used: http://www.worldofspectrum.org/TZXformat.html
//////////////////////////////////////////////////////////////////////////

#ifndef _TZX_H_
#define _TZX_H_

#include "tap.h"
#include "TapeBlock.h"

class CTZXFile: public CTapFile
{
public:	
	static const char SIGNATURE[8];
	
	typedef enum
	{
		BLK_ID_STD			= 0x10, //Standard speed data block
		BLK_ID_TURBO		= 0x11, //Turbo speed data block
		BLK_ID_PURE_TONE	= 0x12, //Pure tone
		BLK_ID_PULSES		= 0x13, //Sequence of pulses of various lengths
		BLK_ID_PURE_DATA	= 0x14, //Pure data block
		BLK_ID_DIRECT_REC	= 0x15, //Direct recording block
		BLK_ID_CSW			= 0x18, //CSW recording block
		BLK_ID_GENERALIZED	= 0x19, //Generalized data block
		BLK_ID_PAUSE		= 0x20, //Pause (silence) or 'Stop the tape' command
		BLK_ID_GRP_STRT		= 0x21, //Group start
		BLK_ID_GRP_END		= 0x22, //Group end
		BLK_ID_JMP			= 0x23, //Jump to block
		BLK_ID_LOOP_STRT	= 0x24, //Loop start
		BLK_ID_LOOP_END		= 0x25, //Loop end
		BLK_ID_CALL_SEQ		= 0x26, //Call sequence
		BLK_ID_RET_SEQ		= 0x27, //Return from sequence
		BLK_ID_SELECT		= 0x28, //Select block
		BLK_ID_STOP_48K		= 0x2A, //Stop the tape if in 48K mode
		BLK_ID_SET_SGN		= 0x2B, //Set signal level
		BLK_ID_TXT_DESCR	= 0x30, //Text description
		BLK_ID_MSG			= 0x31, //Message block
		BLK_ID_ARH_INF		= 0x32, //Archive info
		BLK_ID_HARD_TYPE	= 0x33, //Hardware type
		BLK_ID_CUST_INFO	= 0x35, //Custom info block
		BLK_ID_GLUE			= 0x5A, //"Glue" block (90 dec, ASCII Letter 'Z')
	} TZXBlockTypeID;

#pragma pack(1)
	typedef struct
	{
		char Signature[sizeof(SIGNATURE)];
		byte VerMajor;
		byte VerMinor;
	}TZXHeader;

	//Standard block
	typedef struct  
	{
		word	Pause;
		word	Len;
	}TZXBlkStd;

	//Turbo block
	typedef struct  
	{
		word	Pilot;
		word	Sync1;
		word	Sync2;
		word	Zero;
		word	One;
		word	PilotLen;
		byte	LastBitsCnt;
		word	Pause;
		word	Len1;		//3 byte number
		byte	Len2;
	}TZXBlkTurbo;	

	typedef struct
	{
		byte Len;
		char Msg[0xFF];
	} TZXBlkMessage;

	typedef struct
	{
		byte Len;
		char GrpName[0xFF];
	} TZXBlkGrpStrt;

	typedef struct  
	{
		word pulseLen;
		word pulseCnt;
	} TZXBlkPureTone;
	
	typedef struct  
	{
		word Bit0;
		word Bit1;
		byte LastBitsCnt;
		word Pause;
		word Len1;
		byte Len2;
	} TZXBlkPureData;

	typedef enum
	{
		TXT_ID_TITLE, 
		TXT_ID_PUBLISHER,
		TXT_ID_AUTHOR,
		TXT_ID_YEAR,
		TXT_ID_LANGUAGE,
		TXT_ID_TYPE,
		TXT_ID_PRICE,
		TXT_ID_LOADER,
		TXT_ID_ORIGIN,
		TXT_ID_COMMENTS = 0xFF
	} TZXBlkTxtID;

	typedef struct  
	{
		word Len;
		byte StrCnt;
		typedef struct
		{
			byte TxtID;
			byte TxtLen;
		} TextItem;
		TextItem TxtItem;
	} TZXBlkArhiveInfo;

	typedef struct  
	{
		word SampleLen;
		word Pause;
		byte LastBitsCnt;
		word Len1;
		byte Len2;
	} TZXBlkDirectRec;

	typedef struct  
	{
		word CallCnt;
		short CallItem[1];
	} TZXBlkCallSeq;

	//XTape!",0x1A,MajR,MinR 
	typedef struct
	{
		char Sig[7];
		byte VerMajor;
		byte VerMinor;
	} TZXBlkGlue;

	TZXBlockTypeID m_CurrBlkID;
	bool m_bInGroup;
	dword m_BlkLoopStrt, m_BlkLoopIdx;
	bool m_bInLoop;
	TZXBlkCallSeq* m_CallSeq;
	word m_CallSeqIdx;
	word m_CallSeqRetIdx;
	word m_LoopRptCnt;

	union
	{
		TZXBlkStd blkStd;
		TZXBlkTurbo blkTrb;
		TZXBlkMessage blkMsg;
		TZXBlkGrpStrt blkGrpStrt;
		short jmpType;
		word pauseLen;
		TZXBlkPureTone blkPureTone;
		TZXBlkPureData blkPureData;
		TZXBlkArhiveInfo blkArhiveInfo;		
		TZXBlkDirectRec blkDirRec;
	} m_CurrBlk;
	
#pragma pack()	
	static const byte VER_MAJOR = 1;
	static const byte VER_MINOR = 20;
	static const char* TZXArhBlkNames[];
	static const char* TZXBlkTypeNames[];

	CTZXFile();
	~CTZXFile();

	virtual bool Open(char * fileName, TapeOpenMode mode = TAP_OPEN_EXISTING);
	virtual bool GetFirstBlock(CTapeBlock* tb);
	virtual bool GetNextBlock(CTapeBlock* tb);
	virtual bool IndexTape();
	//RELATIVE jump for current position.
	bool Jump(short iRelBlkIdx);
};

#endif//_TZX_H_