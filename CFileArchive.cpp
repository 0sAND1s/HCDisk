#include "CFileArchive.h"
#include <algorithm> 


const char* CFileArchive::ERROR_TYPE_MSG[] = 
{
	"",
	"Not supported",
	"Bad parameter",
	"Physical error",
	"No catalog left",
	"No disk space left",
	"File exists",
	"File not found",
	"File is read-only"
};

const char* CFileArchive::FSFeatureNames[] = 
{
	"Disk",
	"Sinclair Spectrum",
	"Disk label",
	"File attributes",
	"File folders",
	"File timestamps",
	"Case sensitive names"
};


CFileArchive::CFileArchive(char* name /* = NULL */)
{
	LastError = ERR_NONE;	
	FSFeatures = FSFT_NONE;

	NAME_LENGHT = 8;
	EXT_LENGTH = 1;

	if (name != NULL)
		strcpy(this->Name, name);	
}

CFileArchive::~CFileArchive()
{

}

dword CFileArchive::GetFileSize(CFile* file, bool onDisk)
{
	return file->GetLength();
}

bool CFileArchive::WildCmp(char * mask, const FileNameType fileName)
{
	if (mask == NULL)
	{
		LastError = ERR_BAD_PARAM;
		return false;
	}

	bool isCaseSens = (GetFSFeatures() & FSFT_CASE_SENSITIVE_FILENAMES) > 0;
	if (!isCaseSens)
		strupr(mask);		
	const char *cp = NULL, *mp = NULL;
	FileNameType fName = "";	
	strcpy(fName, fileName);
	if (!isCaseSens)
		strupr(fName);	
	const char* string = fName;
	const char* wild = mask;

	//return false on first mismatch of non-wildcard caracters
	//while string is not finished and not encountered a '*'
	while ((*string) && (*wild != '*')) 
	{
		//if chars differ and wild char is not '?', return false
		if ((*wild != *string) && (*wild != '?'))       
			return 0;

		//get to the next char
		wild++;
		string++;
	}

	//while string is not finished
	while (*string) 
	{
		//if the first char is '*'
		if (*wild == '*') 
		{
			//and ther's no char after '*'
			if (!*++wild)         
				//return true
				return 1;

			//else, there are other chars after '*'
			mp = wild;
			cp = string+1; //cp is the rest of the string after '*'
		} 
		//increment while strings match or wildcard string is '?'
		else if ((*wild == *string) || (*wild == '?')) 
		{
			wild++;
			string++;
		}
		else //no more matches
		{
			wild = mp;
			string = cp++; //get next char
		}
	}

	//skipp all '*'
	while (*wild == '*')
		wild++;

	//return true if wild is not finished
	return !*wild;
}

bool CFileArchive::IsCharValidForFilename(char c)
{
	return c > ' ' && c < 128 && strchr("\"*?\\/", c) == NULL;
}

bool CFileArchive::CreateFileName(char* fNameIn, CFile* file)
{
	if (fNameIn != NULL && file != NULL && strlen(fNameIn) > 0)
	{					
		memset(file->FileName, 0, sizeof(FileNameType));		
		memset(file->Name, 0, sizeof(file->Name));		
		memset(file->Extension, 0, sizeof(file->Extension));		

		
		//byte lastNonBlank = 0;

		//for (byte idxExt = 0; idxExt < NAME_LENGHT /*+ EXT_LENGTH*/; idxExt++)
		//	if ((fNameIn[idxExt] & 0x7F) != ' ')
		//		lastNonBlank = idxExt;

		//char* dot = strrchr(fNameIn, '.');
		//int dotPos = dot != nullptr ? dot - fNameIn : -1;
		//int i = 0, on = 0, oe = 0;	
		//while (i < NAME_LENGHT + EXT_LENGTH && i < strlen(fNameIn))
		//{			
		//	char c = fNameIn[i] & 0x7F;
		//	if (IsCharValidForFilename(c) /*|| (EXT_LENGTH == 0 && i < lastNonBlank)*/)
		//		//c = '-';				
		//	{
		//		//Skip blanks and if we don't have extension, replace with '-'.		
		//		if ((i < NAME_LENGHT || EXT_LENGTH == 0) && i <= lastNonBlank && (dotPos > 0 ? i < dotPos : true))
		//			file->Name[on++] = c;
		//		else if (EXT_LENGTH > 0 /*&& i <= lastNonBlank  && c != '-' */ && (i >= NAME_LENGHT || (dotPos > 0 ? i > dotPos : true)))
		//			file->Extension[oe++] = c;				
		//	}						

		//	i++;
		//}	

		//file->Name[on] = '\0';
		//file->Extension[oe] = '\0';		

		FileNameType fnBuf{};
		strncpy(fnBuf, fNameIn, min(sizeof(fnBuf), strlen(fNameIn)));
		for (char& c : fnBuf)
		{
			c = c & 0x7F; //Strip attribute bit;
		}

		char* extStart = nullptr;
		char* nameEnd = &fnBuf[strlen(fnBuf) - 1];
		if (EXT_LENGTH > 0)
		{
			extStart = strrchr(fnBuf, '.');
			if (extStart != NULL)
			{
				extStart++;
				nameEnd = extStart - 2;
			}
			else if (strlen(fnBuf) > NAME_LENGHT)
			{
				extStart = fnBuf + NAME_LENGHT;
				nameEnd = extStart - 1;
			}
		}		

		if (extStart != nullptr)
		{
			word extLen = (word)strlen(extStart);
			strncpy(file->Extension, extStart, min((size_t)EXT_LENGTH, strlen(extStart)));
			strncpy(file->Name, fnBuf, min((int)NAME_LENGHT, nameEnd - fnBuf +1));
		}
		else
		{
			strncpy(file->Name, fnBuf, min((size_t)NAME_LENGHT, strlen(fnBuf)));
		}

		TrimEnd(file->Name);
		TrimEnd(file->Extension);

		if (strlen(file->Extension) > 0)
			sprintf(file->FileName, "%s.%s", file->Name, file->Extension);
		else
			sprintf(file->FileName, "%s", file->Name);

		return true;
	}	
	else
	{
		LastError = ERR_BAD_PARAM;
		return false;
	}
}


bool CFileArchive::CreateFSName(CFile* file, char* fNameOut)
{
	memset(fNameOut, ' ', MAX_FILE_NAME_LEN);
	TrimEnd(file->Extension);
	if (fNameOut != NULL && file != NULL)
	{				
		if (EXT_LENGTH > 0 && strlen(file->Extension) > 0)
		{
			byte nLen = strlen(file->Name);
			memcpy(fNameOut, file->Name, nLen < NAME_LENGHT ? nLen : NAME_LENGHT);
			file->Extension[EXT_LENGTH] = '\0';
			byte eLen = strlen(file->Extension);
			memcpy(&fNameOut[NAME_LENGHT], file->Extension, eLen < EXT_LENGTH ? eLen : EXT_LENGTH);
		}
		else
		{
			byte nLen = strlen(file->FileName);
			//memcpy(fNameOut, file->Name, nLen > NAME_LENGHT ? NAME_LENGHT : nLen);
			memcpy(fNameOut, file->FileName, nLen);
		}

		return true;
	}	
	else
		return false;
}
