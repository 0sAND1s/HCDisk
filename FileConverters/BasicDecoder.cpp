#include "BasicDecoder.h"
#include <string>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <streambuf>
#include <algorithm>
#include <iostream>
#include <list>

using namespace Basic;
using namespace std;


Basic::BasicDecoder::BasicDecoder()
{	
	//Initialize token map.
	for (int kwIdx = 0; kwIdx < sizeof(BASICKeywordsTable)/sizeof(BASICKeywordsType); kwIdx++)		
		keywords[(BASICKeywordsIDType)BASICKeywordsTable[kwIdx].id] = BASICKeywordsTable[kwIdx];	

	m_ShowNumbers = m_ShowAttributes = true;
}

Basic::BasicDecoder::~BasicDecoder()
{
	//keywords.clear();
}

void Basic::BasicDecoder::Test()
{		
	for (KeywordType::const_iterator it = keywords.begin(); it != keywords.end(); it++)
	{
		cout << right << setfill(' ') << setw(3) << it->first << " = " << it->second.str << endl;		
	}		
}

/*
word Basic::BasicDecoder::GetLine(byte* buf, word maxLen)
{			
	if (buf == NULL)
		return false;

	lineNo = buf[0] * 0x100 + buf[1];	
	lineLen = *(word*)&buf[2];
		
	lineBuf.clear();		
	int pos = 4;	
	
	while ((pos < lineLen + 4 && pos < maxLen))
	{			
		lineBuf.push_back(buf[pos]);				
		pos++;						
	}		

	return lineLen + 4;	
}
*/

Basic::BasicLine Basic::BasicDecoder::GetBasicLineFromLineBuffer(byte* buf, word maxLen)
{
	BasicLine bl;

	bl.lineNumber = buf[0] * 0x100 + buf[1];
	bl.basicSize = *(word*)&buf[2];

	if (bl.lineNumber <= 9999 && bl.basicSize == 0)
		bl.basicSize = maxLen - 4;

	//Line size including the 4 bytes line header.
	bl.lineSize = bl.basicSize + 4;
	
	int posRead = 4;

	if (bl.lineNumber <= 9999)
	{
		bl.lineBufBasic.clear();
		while ((posRead < bl.lineSize && posRead < maxLen))
		{
			bl.lineBufBasic.push_back(buf[posRead++]);
		}

	}
	else
	{
		bl.basicSize = bl.lineSize = 0;
	}
	
	return bl;
}


void Basic::BasicDecoder::PutBasicLineToLineBuffer(BasicLine bl, byte* buf)
{
	buf[0] = bl.lineNumber / 0x100;
	buf[1] = bl.lineNumber % 0x100;
	buf[2] = bl.basicSize % 0x100;
	buf[3] = bl.basicSize / 0x100;

	memcpy(&buf[4], bl.lineBufBasic.data(), bl.basicSize);
}

Basic::BasicLine::BasicLine()
{}


Basic::BasicLine::BasicLine(byte* bufBasic, word basicLen, word lineNo)
{
	lineNumber = lineNo;
	basicSize = basicLen;
	lineSize = basicSize + 4;
	copy(bufBasic, bufBasic + lineSize, back_insert_iterator<vector<byte>>(lineBufBasic));
}


bool Basic::BasicDecoder::DecodeBasicLineToText(BasicLine bl, ostream& strOut)
{
	KeywordType::const_iterator it;
	bool bOK = true;		
	byte b;	
	SpectrumFPType fp;
	bOK = true;	
	word lineBufIdx = 0;		
	
	strOut << right << setw(5) << bl.lineNumber;	
					
	while (lineBufIdx < bl.lineBufBasic.size() && bOK)
	{			
		b = bl.lineBufBasic[lineBufIdx];
		if (b >= ' ' && b < BUDG_COPYRIGHT)
			strOut << b;
		else
		{
			it = keywords.find((BASICKeywordsIDType)b);
			if (it != keywords.end())
			{
				if (it->second.type == BIT_FMT_EXPR) //formated expression
				{					
					if (it->second.id == BFC_NO) //encoded number
					{	
						if (m_ShowNumbers)
						{		
							//Only show numbers if the text representation is different from the actual binary value.
							memcpy(&fp, &bl.lineBufBasic[lineBufIdx+1], sizeof(fp));
							double valBin = FPSpecToDouble(&fp), valDisp, valComp;
							string valStrDisp;
							
							//Seek preceding string number and parse.							
							short idx = lineBufIdx - 1;							
							while (idx >0 && isdigit(bl.lineBufBasic[idx]) || bl.lineBufBasic[idx] == '.' || toupper(bl.lineBufBasic[idx]) == 'E')
								idx--;
							
							copy(&bl.lineBufBasic[idx+1], &bl.lineBufBasic[lineBufIdx], back_insert_iterator<string>(valStrDisp));
							stringstream sFmt;
							sFmt << valStrDisp;
							sFmt >> fixed >> setprecision(2) >> valDisp;
								
							modf(valDisp - valBin, &valComp);
							if (valComp > .001)
								strOut << "{" << valBin << "}";
						}																
					}					
					else if (m_ShowAttributes) //encoded attribute
					{							
						strOut << "{" << it->second.str;
						if (it->second.len >= 1)
							strOut << " " << (word)bl.lineBufBasic[lineBufIdx + 1];
						//AT,TAB have 2 arguments
						if (it->second.len >= 2)
							strOut << "," << (word)bl.lineBufBasic[lineBufIdx + 2];
						strOut << "}";
					}

					lineBufIdx += it->second.len;
				}
				else //keyword or UDG
					strOut << it->second.str;									
			}
			else
				bOK = false;
		}						

		lineBufIdx++;
	}

	strOut << "\r\n";	
	return bOK;
}

word Basic::BasicDecoder::GetVarSize(byte* buf, word len)
{
	word varLen = 0;
	word bufIdx = 0;	

	while (bufIdx < len && varLen == 0)
	{
		if (buf[bufIdx] == 0 || buf[bufIdx] == 1)		
			bufIdx += *(word*)&buf[bufIdx+2] + 4;		
		else
			varLen = len - bufIdx;
	}

	return varLen;
}

bool Basic::BasicDecoder::DecodeVariables(byte* buf, word len, ostream& str)
{
	bool res = true;
	word bufIdx = 0;	
	str << "Variables:";

	while (bufIdx < len && res)
	{
		byte varCode = buf[bufIdx++];		
		double varVal;
		word varLen = 0;
		byte arrDim;
		word* xSize;
		byte arrIdx;
		SpectrumFPType* valPtr;
		byte* chrPtr;

		str << "\r\n" << (char)((varCode & 0x1F) + 'a' - 1);				

		switch (varCode >> 5)
		{
		case BV_ONE_LETTER:
			varVal = FPSpecToDouble((SpectrumFPType*)&buf[bufIdx]);			
			str << " \t= " << varVal;
			bufIdx += 5;
			break;
		case BV_MORE_LETTERS:			
			while ((buf[bufIdx] & 0x80) == 0)			
				str << buf[bufIdx++];
			//Put last var name char.
			str << (char)(buf[bufIdx] & 0x7F);
			bufIdx++;
			varVal = FPSpecToDouble((SpectrumFPType*)&buf[bufIdx]);
			str << " \t= " << varVal;
			bufIdx += 5;
			break;
		case BV_STRING:
			str << "$ \t= \"";
			varLen = *(word*)&buf[bufIdx];
			bufIdx += 2;
			while (varLen > 0)
			{
				byte c = buf[bufIdx++];
				if (c >= ' ' && c < 128)
					str << c;
				else
					str << "$" << hex << uppercase << setw(2) << setfill('0') << (short unsigned)c;
				varLen--;				
			}
			str << "\"";
			break;
		case BV_ARR_NO:
			varLen = *(word*)&buf[bufIdx];			
			str << " \t= ";
			arrDim = buf[bufIdx + 2];
			xSize = (word*)&buf[bufIdx + 3];
			arrIdx = 0;
			valPtr = (SpectrumFPType*)&buf[bufIdx + 3 + arrDim * 2];
			while (arrIdx < arrDim)
			{		
				str << "(";								
				for (word x = 0; x < *xSize; x++)				
				{
					str << FPSpecToDouble(valPtr++);
					if (*xSize - x > 1)
						str << ",";
				}
				str << ")";
				xSize++;
				arrIdx++;				
			}
			bufIdx += varLen + 2;
			break;
		case BV_ARR_STR:
			varLen = *(word*)&buf[bufIdx];			
			str << "$ \t= ";
			arrDim = buf[bufIdx + 2];
			xSize = (word*)&buf[bufIdx + 3];
			arrIdx = 0;
			chrPtr = &buf[bufIdx + 3 + arrDim * 2];
			while (arrIdx < arrDim)
			{		
				str << "\"";								
				for (word x = 0; x < *xSize; x++)		
				{
					//str << *chrPtr++;					
					byte c = *chrPtr++;
					if (c >= ' ' && c <= 128)
						str << c;
					else
						str << hex << showbase << setw(2) << setfill('0') << "'" << (short unsigned)c << "'";					
				}
				str << "\"";
				xSize++;
				arrIdx++;
			}
			bufIdx += varLen + 2;
			break;
		case BV_FOR:
			str << " \t= ";
			valPtr = (SpectrumFPType*)&buf[bufIdx];
			str << "Step: " << FPSpecToDouble(valPtr++) << ", Limit: " << FPSpecToDouble(valPtr++) << ", Current value: " << FPSpecToDouble(valPtr++);
			chrPtr = (byte*)valPtr;
			str << ", Line: " << (chrPtr[0] + 0x100 * chrPtr[1]) << ", Statement: " << (int)chrPtr[2];
			bufIdx += 18;
			break;
		default:
			res = false;
		}
	}	

	str << endl;
	return res;
}

/*
ostream& Basic::BasicDecoder::operator>>(ostream& sPrm)
{		
	DecodeLine(sPrm);
	//sPrm << sTmp.str();
	return sPrm;
}
*/

//Taken from Taper by M. Heide.
double Basic::BasicDecoder::FPSpecToDouble (SpectrumFPType *FPNumber)
{
	double FPValue;
	double Power;
	double Raised;
	double Result;

	if (FPNumber->_Exponent == 0x00)                                                                        /* Small integer form ? */
	{
		FPValue = (double)FPNumber->_Mantissa[1] + 256 * (double)FPNumber->_Mantissa[2];
		if (FPNumber->_Mantissa[0])                                                                                     /* Negative ? */
			FPValue -= (double)65536;                                                                 /* (Maintain bug in Spectrum ROM) */
	}
	else
	{
		Power = pow (2, ((double)FPNumber->_Exponent - 129));
		if (FPNumber->_Mantissa[0] & 0x80)                                                              /* Determine negative numbers */
			Power *= -1;
		Raised = 128;
		Result = (FPNumber->_Mantissa[0] & 0x7F) / Raised;
		Raised *= 256;
		Result += (FPNumber->_Mantissa[1]) / Raised;
		Raised *= 256;
		Result += (FPNumber->_Mantissa[2]) / Raised;
		Raised *= 256;
		Result += (FPNumber->_Mantissa[3]) / Raised;
		FPValue = Power * (1 + Result);
	}
	return (FPValue);
}

void Basic::BasicDecoder::ConvertLoader(BasicLine* blSrc, BasicLine* blDst, const char* loadPrefix, vector<string>* actualNames, byte* nameIdx)
{
	blDst->lineNumber = blSrc->lineNumber;
	blDst->lineBufBasic.clear();

	map<word, pair<word, string>> namesLoaded = GetLoadingBlocks(blSrc->lineBufBasic.data(), blSrc->basicSize);	
	//If no LOAD statement in this line, just copy it over.
	if (namesLoaded.size() == 0)
	{
		copy(blSrc->lineBufBasic.begin(), blSrc->lineBufBasic.end(), back_insert_iterator<vector<byte>>(blDst->lineBufBasic));
	}
	else
	{
		word inIdx = 0;		
		while (inIdx < blSrc->basicSize)
		{
			//If LOAD found at this index in line, copy load prefix and name.
			if (namesLoaded.find(inIdx) != namesLoaded.end())
			{
				//Remove previous prefix, if any.
				while (blDst->lineBufBasic.size() > 0 && blDst->lineBufBasic[blDst->lineBufBasic.size() - 1] != Basic::BK_LOAD)
					blDst->lineBufBasic.pop_back();

				word prefIdx = 0;
				while (prefIdx < strlen(loadPrefix))
				{
					if (loadPrefix[prefIdx] != '?')
						blDst->lineBufBasic.push_back(loadPrefix[prefIdx]);
					else if (*nameIdx < actualNames->size())
					{
						for (char c : (*actualNames)[*nameIdx])
							blDst->lineBufBasic.push_back(c);
						(*nameIdx)++;
					}					

					prefIdx++;
				}
				
				//Skip after the loaded name.
				inIdx = namesLoaded[inIdx].first;
			}
			else
			{
				blDst->lineBufBasic.push_back(blSrc->lineBufBasic[inIdx]);
				inIdx++;
			}			
			
		}
	}
	

	blDst->basicSize = (word)blDst->lineBufBasic.size();
	blDst->lineSize = blDst->basicSize + 4;
}

map<word, pair<word, std::string>> Basic::BasicDecoder::GetLoadingBlocks(byte* bufBasic, word progLen)
{
	map<word, pair<word, std::string>> res;
	word lineSize = 0;
	word bufPos = 0;	
	vector<byte> lineBuf(bufBasic, bufBasic + progLen);

	do
	{		
		lineSize = progLen;
		auto remPos = find(lineBuf.begin(), lineBuf.end(), BK_REM);
		//auto remPos = lineBuf.end();
		
		//The LOAD statement, after the name, has CODE or SCREEN$.
		auto loadPos = find(lineBuf.begin(), lineBuf.end(), BK_LOAD);
		auto loadCode = find(loadPos, lineBuf.end(), BK_CODE);
		auto loadSCR = find(loadPos, lineBuf.end(), BK_SCREEN);
		auto loadEnd = find(loadPos, lineBuf.end(), ':');

		if (loadEnd > loadCode)
			loadEnd = loadCode;
		if (loadEnd > loadSCR)
			loadEnd = loadSCR;		

		while (loadPos != lineBuf.end() && loadPos < remPos && loadEnd != lineBuf.end())
		{
			std::vector<byte>::iterator loadIt = loadEnd - 1;
			string blockName;

			//Search backwards from LOAD statement end towards the beginning, to find the end quote of file name.
			while (loadIt > loadPos+1 && *loadIt != '"')
			{
				loadIt--;
			}
			if (*loadIt == '"')
				loadIt--;			
			
			//Search backwards for the start quote of file name.
			while (loadIt > loadPos+1 && *loadIt != '"')
			{
				blockName.insert(blockName.begin(), *loadIt);
				loadIt--;
			}
			res[loadIt - lineBuf.begin()] = pair<word, string>(loadEnd - lineBuf.begin(), blockName);

			loadPos = find(loadEnd, lineBuf.end(), BK_LOAD);
			loadEnd = find(loadPos, lineBuf.end(), ':');
			auto loadCode = find(loadPos, lineBuf.end(), BK_CODE);
			auto loadSCR = find(loadPos, lineBuf.end(), BK_SCREEN);

			if (loadEnd > loadCode)
				loadEnd = loadCode;
			if (loadEnd > loadSCR)
				loadEnd = loadSCR;
		}			
		
		
		bufPos += lineSize;
	} while (lineSize > 0 && bufPos < progLen);	
	
	return res;
}