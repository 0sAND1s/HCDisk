#include <map>
#include <vector>
#include <cmath>
#include <iostream>

#include "..\types.h"

#define BAS_LINE_MAX_LEN 0xFFFF
#define BAS_FMT_CHR_CNT	11
#define BAS_KEYWORD_CNT	(BK_COPY - BK_RND + 1)
#define BAS_UDG_CNT	(BUDG_U - BUDG_COPYRIGHT + 1)
#define BAS_ASCII_CNT (BUDG_COPYRIGHT - ' ')

using namespace std;

namespace Basic
{	
	//0..31 - special chars
	//32..127 - printable ASCII chars
	//128..164 - UDGs
	//165..255 - BASIC keywords

	//The type of an element in a basic line.
	typedef enum
	{		
		BIT_FMT_EXPR,
		BIT_ASCII,
		BIT_UDG,
		BIT_KEYWORD				
	} BASICItemClass;


	//Basic keywords type
	typedef enum
	{
		BFC_BACKSPACE = 8,	
		BFC_CR = 13,
		BFC_NO = 14,
		BFC_INK = 16, 
		BFC_PAPER = 17,
		BFC_FLASH = 18,
		BFC_BRIGHT = 19,
		BFC_INVERSE = 20,
		BFC_OVER = 21,
		BFC_AT = 22,
		BFC_TAB = 23,

		//Position of a quarter of a cell in a char cell is numbers like this:
		//12
		//34
		BUDG_COPYRIGHT = 127,
		BUDG_BLANK = 128,
		BUDG_2 = 129,
		BUDG_1 = 130,
		BUDG_12 = 131,
		BUDG_4 = 132,
		BUDG_24 = 133,
		BUDG_14 = 134,
		BUDG_124 = 135,
		BUDG_3 = 136,
		BUDG_23 = 137,
		BUDG_13 = 138,
		BUDG_123 = 139,
		BUDG_34 = 140,
		BUDG_234 = 141,
		BUDG_134 = 142,
		BUDG_1234 = 143,
		BUDG_A = 144,
		BUDG_B = 145,
		BUDG_C = 146,
		BUDG_D = 147,
		BUDG_E = 148,
		BUDG_F = 149,
		BUDG_G = 150,
		BUDG_H = 151,
		BUDG_I = 152,
		BUDG_J = 153,
		BUDG_K = 154,
		BUDG_L = 155,
		BUDG_M = 156,
		BUDG_N = 157,
		BUDG_O = 158,
		BUDG_P = 159,
		BUDG_Q = 160,
		BUDG_R = 161,
		BUDG_S = 162,
		BUDG_T = 163,
		BUDG_U = 164,

		BK_RND = 165,
		BK_INKEY = 166,
		BK_PI = 167,
		BK_FN  = 168,
		BK_POINT  = 169,
		BK_SCREEN  = 170,
		BK_ATTR  = 171,
		BK_AT  = 172,
		BK_TAB  = 173,
		BK_VAL_D  = 174,
		BK_CODE  = 175,
		BK_VAL  = 176,
		BK_LEN  = 177,
		BK_SIN  = 178,
		BK_COS  = 179,
		BK_TAN  = 180,
		BK_ASN  = 181,
		BK_ACS  = 182,
		BK_ATN  = 183,
		BK_LN  = 184,
		BK_EXP  = 185,
		BK_INT  = 186,
		BK_SQR  = 187,
		BK_SGN  = 188,
		BK_ABS  = 189,
		BK_PEEK  = 190,
		BK_IN  = 191,
		BK_USR  = 192,
		BK_STR  = 193,
		BK_CHR  = 194,
		BK_NOT  = 195,
		BK_BIN  = 196,
		BK_OR = 197,
		BK_AND = 198,
		BK_LESS_EQ = 199,
		BK_GREAT_EQ = 200,
		BK_DIFF = 201,
		BK_LINE = 202,
		BK_THEN = 203,
		BK_TO = 204,
		BK_STEP = 205,
		BK_DEF_FN = 206,
		BK_CAT = 207,
		BK_FORMAT = 208,
		BK_MOVE = 209,
		BK_ERASE = 210,
		BK_OPEN_CH = 211,
		BK_CLOSE_CH =212,
		BK_MERGE = 213,
		BK_VERIFY = 214,
		BK_BEEP = 215,
		BK_CIRCLE = 216,
		BK_INK = 217,
		BK_PAPER = 218,
		BK_FLASH = 219,
		BK_BRIGHT = 220,
		BK_INVERSE = 221,
		BK_OVER = 222,
		BK_OUT = 223,
		BK_LPRINT = 224,
		BK_LLIST = 225,
		BK_STOP = 226,
		BK_READ = 227,
		BK_DATA = 228,
		BK_RESTORE = 229,
		BK_NEW = 230,
		BK_BORDER = 231,
		BK_CONTINUE = 232,
		BK_DIM = 233,
		BK_REM = 234,
		BK_FOR = 235,
		BK_GO_TO = 236,
		BK_GO_SUB = 237,
		BK_INPUT = 238,
		BK_LOAD = 239,
		BK_LIST = 240,
		BK_LET = 241,
		BK_PAUSE = 242,
		BK_NEXT = 243,
		BK_POKE = 244,
		BK_PRINT = 245,
		BK_PLOT = 246,
		BK_RUN = 247,
		BK_SAVE = 248,
		BK_RANDOMIZE = 249,
		BK_IF = 250,
		BK_CLS = 251,
		BK_DRAW = 252,
		BK_CLEAR = 253,
		BK_RETURN = 254,
		BK_COPY = 255
	} BASICKeywordsIDType;

	typedef enum
	{				
		BV_STRING		= 0x02,	//010
		BV_ONE_LETTER	= 0x03, //011
		BV_ARR_NO		= 0x04,	//100
		BV_MORE_LETTERS	= 0x05, //101
		BV_ARR_STR		= 0x06,	//110
		BV_FOR			= 0x07	//111
	} BASVarType;

	typedef struct
	{
		byte id;
		char* str;
		byte len; //length if the argument, excluding keyword
		BASICItemClass type;
	} BASICKeywordsType;


	static BASICKeywordsType BASICKeywordsTable[] =	
	{
		{BFC_BACKSPACE, "BACKSP", 0, BIT_FMT_EXPR},	
		/*{BFC_CR, "\n", 0, BIT_FMT_EXPR},*/
		{BFC_NO, "#", 5, BIT_FMT_EXPR},		/* number */	
		{BFC_INK, "INK", 1, BIT_FMT_EXPR},		/* INK */
		{BFC_PAPER,"PAPER", 1, BIT_FMT_EXPR},		/* PAPER */
		{BFC_FLASH, "FLASH", 1, BIT_FMT_EXPR},		/* FLASH */
		{BFC_BRIGHT, "BRIGHT", 1, BIT_FMT_EXPR},	/* BRIGHT */
		{BFC_INVERSE, "INVERSE", 1, BIT_FMT_EXPR},	/* INVERSE */
		{BFC_OVER, "OVER", 1, BIT_FMT_EXPR},		/* OVER */
		{BFC_AT, "AT", 2, BIT_FMT_EXPR},		/* AT */
		{BFC_TAB, "TAB", 2, BIT_FMT_EXPR},		/* TAB */		 

		{BUDG_COPYRIGHT, "(C)", 0, BIT_UDG},      
		{BUDG_BLANK, "\\  ", 0, BIT_UDG},
		{BUDG_2, "\\ '", 0, BIT_UDG},
		{BUDG_1, "\\' ", 0, BIT_UDG},
		{BUDG_12, "\\''", 0, BIT_UDG},
		{BUDG_4, "\\ .", 0, BIT_UDG},
		{BUDG_24, "\\ :", 0, BIT_UDG},
		{BUDG_14, "\\'.", 0, BIT_UDG},
		{BUDG_124, "\\':", 0, BIT_UDG},
		{BUDG_3, "\\. ", 0, BIT_UDG},
		{BUDG_23, "\\.'", 0, BIT_UDG},
		{BUDG_13, "\\: ", 0, BIT_UDG},
		{BUDG_123, "\\:'", 0, BIT_UDG},
		{BUDG_34, "\\..", 0, BIT_UDG},
		{BUDG_234, "\\.:", 0, BIT_UDG},
		{BUDG_134, "\\:.", 0, BIT_UDG},
		{BUDG_1234, "\\::", 0, BIT_UDG},
		{BUDG_A, "\\a", 0, BIT_UDG},
		{BUDG_B, "\\b", 0, BIT_UDG},
		{BUDG_C, "\\c", 0, BIT_UDG},
		{BUDG_D, "\\d", 0, BIT_UDG},
		{BUDG_E, "\\e", 0, BIT_UDG},
		{BUDG_F, "\\f", 0, BIT_UDG},
		{BUDG_G, "\\g", 0, BIT_UDG},
		{BUDG_H, "\\h", 0, BIT_UDG},
		{BUDG_I, "\\i", 0, BIT_UDG},
		{BUDG_J, "\\j", 0, BIT_UDG},
		{BUDG_K, "\\k", 0, BIT_UDG},
		{BUDG_L, "\\l", 0, BIT_UDG},
		{BUDG_M, "\\m", 0, BIT_UDG},
		{BUDG_N, "\\n", 0, BIT_UDG},
		{BUDG_O, "\\o", 0, BIT_UDG},
		{BUDG_P, "\\p", 0, BIT_UDG},
		{BUDG_Q, "\\q", 0, BIT_UDG},
		{BUDG_R, "\\r", 0, BIT_UDG},
		{BUDG_S, "\\s", 0, BIT_UDG},
		{BUDG_T, "\\t", 0, BIT_UDG},
		{BUDG_U, "\\u", 0, BIT_UDG},

		{BK_RND, "RND", 0, BIT_KEYWORD},
		{BK_INKEY, "INKEY$", 0, BIT_KEYWORD},
		{BK_PI, "PI", 0, BIT_KEYWORD},
		{BK_FN, "FN ", 0, BIT_KEYWORD},
		{BK_POINT, "POINT ", 0, BIT_KEYWORD},
		{BK_SCREEN, "SCREEN$ ", 0, BIT_KEYWORD},
		{BK_ATTR, "ATTR ", 0, BIT_KEYWORD},
		{BK_AT, "AT ", 0, BIT_KEYWORD},
		{BK_TAB, "TAB ", 0, BIT_KEYWORD},
		{BK_VAL_D, "VAL$ ", 0, BIT_KEYWORD},
		{BK_CODE, "CODE ", 0, BIT_KEYWORD},
		{BK_VAL, "VAL ", 0, BIT_KEYWORD},
		{BK_LEN, "LEN ", 0, BIT_KEYWORD},
		{BK_SIN, "SIN ", 0, BIT_KEYWORD},
		{BK_COS, "COS ", 0, BIT_KEYWORD},
		{BK_TAN, "TAN ", 0, BIT_KEYWORD},
		{BK_ASN, "ASN ", 0, BIT_KEYWORD},
		{BK_ACS, "ACS ", 0, BIT_KEYWORD},
		{BK_ATN, "ATN ", 0, BIT_KEYWORD},
		{BK_LN, "LN ", 0, BIT_KEYWORD},
		{BK_EXP, "EXP ", 0, BIT_KEYWORD},
		{BK_INT, "INT ", 0, BIT_KEYWORD},
		{BK_SQR, "SQR ", 0, BIT_KEYWORD},
		{BK_SGN, "SGN ", 0, BIT_KEYWORD},
		{BK_ABS, "ABS ", 0, BIT_KEYWORD},
		{BK_PEEK, "PEEK ", 0, BIT_KEYWORD},
		{BK_IN, "IN ", 0, BIT_KEYWORD},
		{BK_USR, "USR ", 0, BIT_KEYWORD},
		{BK_STR, "STR$ ", 0, BIT_KEYWORD},
		{BK_CHR, "CHR$ ", 0, BIT_KEYWORD},
		{BK_NOT, "NOT ", 0, BIT_KEYWORD},
		{BK_BIN, "BIN ", 0, BIT_KEYWORD},
		{BK_OR, " OR ", 0, BIT_KEYWORD},
		{BK_AND, " AND ", 0, BIT_KEYWORD},
		{BK_LESS_EQ, "<=", 0, BIT_KEYWORD},
		{BK_GREAT_EQ, ">=", 0, BIT_KEYWORD},
		{BK_DIFF, "<>", 0, BIT_KEYWORD},
		{BK_LINE, " LINE ", 0, BIT_KEYWORD},
		{BK_THEN, " THEN ", 0, BIT_KEYWORD},
		{BK_TO, " TO ", 0, BIT_KEYWORD},
		{BK_STEP, " STEP ", 0, BIT_KEYWORD},
		{BK_DEF_FN, " DEF FN ", 0, BIT_KEYWORD},
		{BK_CAT, " CAT ", 0, BIT_KEYWORD},
		{BK_FORMAT, " FORMAT ", 0, BIT_KEYWORD},
		{BK_MOVE, " MOVE ", 0, BIT_KEYWORD},
		{BK_ERASE, " ERASE ", 0, BIT_KEYWORD},
		{BK_OPEN_CH, " OPEN #", 0, BIT_KEYWORD},
		{BK_CLOSE_CH, " CLOSE #", 0, BIT_KEYWORD},
		{BK_MERGE, " MERGE ", 0, BIT_KEYWORD},
		{BK_VERIFY, " VERIFY ", 0, BIT_KEYWORD},
		{BK_BEEP, " BEEP ", 0, BIT_KEYWORD},
		{BK_CIRCLE, " CIRCLE ", 0, BIT_KEYWORD},
		{BK_INK, " INK ", 0, BIT_KEYWORD},
		{BK_PAPER, " PAPER ", 0, BIT_KEYWORD},
		{BK_FLASH, " FLASH ", 0, BIT_KEYWORD},
		{BK_BRIGHT, " BRIGHT ", 0, BIT_KEYWORD},
		{BK_INVERSE, " INVERSE ", 0, BIT_KEYWORD},
		{BK_OVER, " OVER ", 0, BIT_KEYWORD},
		{BK_OUT, " OUT ", 0, BIT_KEYWORD},
		{BK_LPRINT, " LPRINT ", 0, BIT_KEYWORD},
		{BK_LLIST, " LLIST ", 0, BIT_KEYWORD},
		{BK_STOP, " STOP ", 0, BIT_KEYWORD},
		{BK_READ, " READ ", 0, BIT_KEYWORD},
		{BK_DATA, " DATA ", 0, BIT_KEYWORD},
		{BK_RESTORE, " RESTORE ", 0, BIT_KEYWORD},
		{BK_NEW, " NEW ", 0, BIT_KEYWORD},
		{BK_BORDER, " BORDER ", 0, BIT_KEYWORD},
		{BK_CONTINUE, " CONTINUE ", 0, BIT_KEYWORD},
		{BK_DIM, " DIM ", 0, BIT_KEYWORD},
		{BK_REM, " REM ", 0, BIT_KEYWORD},
		{BK_FOR, " FOR ", 0, BIT_KEYWORD},
		{BK_GO_TO, " GO TO ", 0, BIT_KEYWORD},
		{BK_GO_SUB, " GO SUB ", 0, BIT_KEYWORD},
		{BK_INPUT, " INPUT ", 0, BIT_KEYWORD},
		{BK_LOAD, " LOAD ", 0, BIT_KEYWORD},
		{BK_LIST, " LIST ", 0, BIT_KEYWORD},
		{BK_LET, " LET ", 0, BIT_KEYWORD},
		{BK_PAUSE, " PAUSE ", 0, BIT_KEYWORD},
		{BK_NEXT, " NEXT ", 0, BIT_KEYWORD},
		{BK_POKE, " POKE ", 0, BIT_KEYWORD},
		{BK_PRINT, " PRINT ", 0, BIT_KEYWORD},
		{BK_PLOT, " PLOT ", 0, BIT_KEYWORD},
		{BK_RUN, " RUN ", 0, BIT_KEYWORD},
		{BK_SAVE, " SAVE ", 0, BIT_KEYWORD},
		{BK_RANDOMIZE, " RANDOMIZE ", 0, BIT_KEYWORD},
		{BK_IF, " IF ", 0, BIT_KEYWORD},
		{BK_CLS, " CLS ", 0, BIT_KEYWORD},
		{BK_DRAW, " DRAW ", 0, BIT_KEYWORD},
		{BK_CLEAR, " CLEAR ", 0, BIT_KEYWORD},
		{BK_RETURN, " RETURN ", 0, BIT_KEYWORD},
		{BK_COPY, " COPY ", 0, BIT_KEYWORD}
	};

//////////////////////////////////////////////////////////////////////////
	class BasicLine
	{
	public:
		word lineNumber;
		word lineLen;
		byte lineBuf[BAS_LINE_MAX_LEN];

		bool GetLine(byte* buf, word len);
	protected:	
		byte* pos;
		byte* buf;		
	};
//////////////////////////////////////////////////////////////////////////

	class BasicDecoder
	{	
		/* The Spectrum-style 5-byte floating point form */
		/* (Small integers are catered for) */
		//Taken from Taper by M. Heide.
#pragma pack(1)
		typedef struct		                                                      
		{
			byte               _Exponent;
			byte               _Mantissa[4];
		} SpectrumFPType;	
#pragma pack()

	protected:		
		typedef map<BASICKeywordsIDType, BASICKeywordsType> KeywordType;		
		KeywordType keywords;		
		vector<byte> lineBuf;		
		word lineNo;
		word lineLen;
		bool m_ShowNumbers;
		bool m_ShowAttributes;

	public:
		BasicDecoder();
		~BasicDecoder();
		static double FPSpecToDouble (SpectrumFPType *FPNumber);
		void Test();
		word GetLine(byte* buf, word len);
		bool DecodeLine(ostream& str);		
		bool DecodeVariables(byte* buf, word len, ostream& str);
		static word GetVarSize(byte* buf, word len);
		
		ostream& operator>>(ostream&);
	};
}
