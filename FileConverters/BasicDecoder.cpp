#include "BasicDecoder.h"
#include <string>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <streambuf>
#include <algorithm>
#include <iostream>

using namespace Basic;

bool Basic::BasicLine::GetLine(byte* buf, word len)
{
	lineNumber = *((byte*)buf+1) + 256 * *(byte*)buf;
	lineLen = *((word*)buf + 1);

	//if (lineNumber > 9999)
	//	return false;

	/*
	if (lineLen == 0)
		return false;
	*/

	memcpy(lineBuf, ((word*)buf + 2), len);

	return true;
}	


Basic::BasicDecoder::BasicDecoder()
{	
	//Initialize token map.
	for (int kwIdx = 0; kwIdx < sizeof(BASICKeywordsTable)/sizeof(BASICKeywordsType); kwIdx++)		
		keywords[(BASICKeywordsIDType)BASICKeywordsTable[kwIdx].id] = BASICKeywordsTable[kwIdx];	

	m_ShowNumbers = m_ShowAttributes = true;
}

Basic::BasicDecoder::~BasicDecoder()
{
	keywords.clear();
}

void Basic::BasicDecoder::Test()
{		
	for (KeywordType::const_iterator it = keywords.begin(); it != keywords.end(); it++)
	{
		cout << right << setfill(' ') << setw(3) << it->first << " = " << it->second.str << endl;		
	}		
}

word Basic::BasicDecoder::GetLine(byte* buf, word maxLen)
{			
	if (buf == NULL)
		return false;

	lineNo = buf[0] * 0x100 + buf[1];	
	lineLen = *(word*)&buf[2];
		
	lineBuf.clear();		
	int pos = 4;	
	
	while ((pos < lineLen + 4 /*&& b != 0x0D*/ && pos < maxLen))
	{			
		lineBuf.push_back(buf[pos]);				
		pos++;						
	}		

	return lineLen + 4;	
}	




bool Basic::BasicDecoder::DecodeLine(ostream& strOut)
{
	KeywordType::const_iterator it;
	bool bOK = true;		
	byte b;	
	SpectrumFPType fp;
	bOK = true;	
	word lineBufIdx = 0;		
	
	strOut << right << setw(5) << lineNo;	
					
	while (lineBufIdx < lineBuf.size() && bOK)
	{			
		b = lineBuf[lineBufIdx];
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
							memcpy(&fp, &lineBuf[lineBufIdx+1], sizeof(fp));
							double valBin = FPSpecToDouble(&fp), valDisp, valComp;
							string valStrDisp;
							
							//Seek preceding string number and parse.							
							short idx = lineBufIdx - 1;							
							while (idx >0 && isdigit(lineBuf[idx]) || lineBuf[idx] == '.' || toupper(lineBuf[idx]) == 'E')
								idx--;
							
							copy(&lineBuf[idx+1], &lineBuf[lineBufIdx], back_insert_iterator<string>(valStrDisp));
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
							strOut << " " << (word)lineBuf[lineBufIdx + 1];
						//AT,TAB have 2 arguments
						if (it->second.len >= 2)
							strOut << "," << (word)lineBuf[lineBufIdx + 2];
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
				if (c >= ' ' && c <= 128)
					str << c;
				else
					str << hex << showbase << setw(2) << setfill('0') << "'" << (short unsigned)c << "'";
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

	return res;
}

ostream& Basic::BasicDecoder::operator>>(ostream& sPrm)
{		
	DecodeLine(sPrm);
	//sPrm << sTmp.str();
	return sPrm;
}

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
