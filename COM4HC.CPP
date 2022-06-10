#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>

#include <windows.h>

#include "types.h"
#include "CFileIF1.h"

enum
{
	PC2HC,
	HC2PC
} TransDir;

char port[5] = "COM1";
int baud = CBR_4800;
BOOL useHeader = TRUE;
HANDLE m_hCom;
CFileIF1::CFileIF1Header fileHdr;

//Can be defined to enable block transfer instead of byte-by-byte transfer, but for block transfer progress cannot be displayed.
//#define BLOCK_TRANSFER

HANDLE SetCOMForHC(char* m_sComPort)
{
	// variables used with the com port
	BOOL     m_bPortReady;
	HANDLE   m_hCom;
	DCB      m_dcb;

	m_hCom = CreateFile(m_sComPort,
		GENERIC_READ | GENERIC_WRITE,
		0, // exclusive access
		NULL, // no security
		OPEN_EXISTING,
		0, // no overlapped I/O
		NULL); // null template

	m_bPortReady = SetupComm(m_hCom, 1, 1); // set buffer sizes

	m_bPortReady = GetCommState(m_hCom, &m_dcb);
	m_dcb.BaudRate			= baud;
	m_dcb.ByteSize			= 8;
	m_dcb.Parity			= NOPARITY;
	m_dcb.StopBits			= ONESTOPBIT;
	
	m_dcb.fParity			= TRUE;
	m_dcb.fBinary			= TRUE;

	m_dcb.fAbortOnError		= FALSE;

	m_dcb.fOutxCtsFlow		= TRUE;						//Required by HC when it receives, the PC will only send if the HC is ready by raising PC CTS pin connected to HC DTR pin.
	m_dcb.fOutxDsrFlow		= TRUE;
	m_dcb.fDsrSensitivity	= FALSE;					//Must be disabled.
	
	m_dcb.fRtsControl		= RTS_CONTROL_ENABLE;		//Must be ENABLED or HANDSHAKE or TOGGLE.
	m_dcb.fDtrControl		= DTR_CONTROL_ENABLE;	

	m_dcb.fInX				= FALSE;
	m_dcb.fOutX				= FALSE;	
	m_dcb.fErrorChar		= FALSE;
	m_dcb.fTXContinueOnXoff = TRUE;

	m_bPortReady = SetCommState(m_hCom, &m_dcb);

	COMMTIMEOUTS timeouts = { 0 };
	if (GetCommTimeouts(m_hCom, &timeouts))
	{		
		//Timeout must be enabled for transfer direction HC2PC to work.
		timeouts.ReadTotalTimeoutConstant = timeouts.WriteTotalTimeoutConstant = 60 * 1000;
		SetCommTimeouts(m_hCom, &timeouts);
	}


	return m_hCom;
}

BOOL SendToHCByte(HANDLE m_hCom, byte b)
{
	DWORD iBytesWritten = 1;
	return WriteFile(m_hCom, &b, 1, &iBytesWritten, NULL) && iBytesWritten == 1;
}

BOOL SendToHCBuf(HANDLE m_hCom, byte* b, word len)
{
	DWORD iBytesWritten = 1;
	return WriteFile(m_hCom, b, len, &iBytesWritten, NULL) && iBytesWritten == len;
}


BOOL ReadFromHCByte(HANDLE m_hCom, byte *b)
{
	DWORD iBytesRead = 1;
	return ReadFile(m_hCom, b, 1, &iBytesRead, NULL) && iBytesRead == 1;
}

BOOL ReadFromHCBuf(HANDLE m_hCom, byte *b, word len)
{
	DWORD iBytesRead = 1;
	return ReadFile(m_hCom, b, len, &iBytesRead, NULL) && iBytesRead == len;
}

bool SendToHCForBASIC(int comIdx, FILE* pcFile)
{
	//TODO:
	//1. Create a BASIC loader that includes the COM routine for 19200 BAUD using BIN2REM.
	//2. Send the above block to HC using 4800 BAUD.
	//3. Send the main game block using 19200 BAUD.
	return true;
}

/*
void GetOpts(int argc, char * argv[])
{
	for (int argIdx = 2; argIdx < argc; argIdx += 2)
	{
		if (strcmp(argv[argIdx], "-a") == 0)
			FileHeader.FileStart = atoi(argv[argIdx + 1]);
		else if (strcmp(argv[argIdx], "-t") == 0)
			FileHeader.FileType = atoi(argv[argIdx + 1]);
		else if (strcmp(argv[argIdx], "-c") == 0)
			strncpy(port, argv[argIdx + 1], 4);
		else if (strcmp(argv[argIdx], "-l") == 0)
			FileHeader.FileLen = atoi(argv[argIdx + 1]);
		else if (strcmp(argv[argIdx], "-b") == 0)
			baud = atoi(argv[argIdx + 1]);
		else if (strcmp(argv[argIdx], "-d") == 0)
		{
			if (strcmp(argv[argIdx +  1], "PC2HC") == 0)
				TransDir = PC2HC;
			else if (strcmp(argv[argIdx +  1], "HC2PC") == 0)
				TransDir = HC2PC;
		}
		else if (strcmp(argv[argIdx], "-nh") == 0)
		{
			useHeader = FALSE;
		}
	}
}


int main(int argc, char * argv[])
{
	//_exec("mode com1");
	//system("mode com1 baud=19200 parity=N data=8 stop=1 xon=off octs=on odsr=off idsr=off dtr=on rts=hs");
	//mode com7 BAUD=4800 PARITY=n DATA=8 STOP=1 to=on octs=on rts=on
	HANDLE m_hCom;
	byte b;
	BOOL transOK = FALSE;
	byte * cp;
	FileHeader.FileType = 3;

	if (argc >= 2)
	{						
		GetOpts(argc, argv);

		FILE * file = fopen(argv[1], TransDir == PC2HC ? "rb" : "wb");		
		if (file)
		{
			int FileLen = 0, idx = 0;
			fseek(file, 0, SEEK_END);
			FileLen = ftell(file);
			fseek(file, 0, SEEK_SET);
						
			if (TransDir == PC2HC && (FileHeader.FileLen > FileLen || FileHeader.FileLen == 0))
				FileHeader.FileLen = FileLen;								
						
			m_hCom = SetCOMForHC(port);
			if (m_hCom == INVALID_HANDLE_VALUE)
			{
				printf("Couldn't open %s!", port);
				fclose(file);
				return -3;
			}			
			
			printf("Starting with these options: filename=%s; dir=%s; start=%d; type=%d; len=%d; port=%s, baud=%d, header=%s.\n", 
				argv[1], (TransDir == PC2HC ? "PC2HC" : "HC2PC"), FileHeader.FileStart, FileHeader.FileType, FileHeader.FileLen, port, baud, (useHeader ? "yes" : "no"));
			
			transOK = TRUE;
			if (useHeader)
			{
				idx = sizeof(FileHeader);
				cp = ((byte*)&FileHeader);				
#ifndef	BLOCK_TRANSFER
				while (idx-- > 0 && transOK)
					transOK = (TransDir == PC2HC ? SendToHCByte(m_hCom, *cp++) : ReadFromHCByte(m_hCom, cp++));
#else			
				transOK = (TransDir == PC2HC ? SendToHCBuf(m_hCom, cp, sizeof(FileHeader)) : ReadFromHCBuf(m_hCom, cp, sizeof(FileHeader)));
#endif			
			}
			
			idx = FileHeader.FileLen;
			clock_t time_start = clock();			
#ifndef BLOCK_TRANSFER						
			while (idx-- > 0 && transOK)
			{
				if (TransDir == PC2HC)
				{
					b = fgetc(file);
					transOK = SendToHCByte(m_hCom, b);

					//The HC still needs some time to process that one byte, so let's sleep on it.				
					//Sleep(1);
				}
				else
				{
					transOK = ReadFromHCByte(m_hCom, &b);
					fputc(b, file);
				}

				printf("\rTransferred %i/%i (%0.2f%%). ", FileHeader.FileLen - idx, FileHeader.FileLen, ((float)(FileHeader.FileLen - idx) / FileHeader.FileLen) * 100);
			}
#else			
			
			byte* buf = malloc(FileHeader.FileLen);
			if (TransDir == PC2HC)
			{
				fread(buf, 1, FileHeader.FileLen, file);
				transOK = SendToHCBuf(m_hCom, buf, FileHeader.FileLen);
			}
			else
			{
				transOK = ReadFromHCBuf(m_hCom, buf, FileHeader.FileLen);
				fwrite(buf, 1, FileHeader.FileLen, file);
			}				
			free(buf);
#endif			

			float transTime = (float)(clock() - time_start)/CLOCKS_PER_SEC;
			printf("It took %.2f seconds, resulting in %.2f effective baud.\n", transTime, (FileHeader.FileLen*(8+1))/transTime);

			if (!transOK)
				puts("Transfer error!");

			fclose(file);
			CloseHandle(m_hCom);

		}
		else
		{
			printf("Couldn't open file '%s'", argv[1]);
			return -2;
		}
	}
	else
	{
		//HC supports these speeds: 50, 110, 300, 600, 1200, 2400, 4800, 9600, 19200, also accepting non-standard speed up to 22000, but 9600 is fastest.
		printf("COM2HC (C) George Chirtoaca %s. Utility for transferring binary files to a HC with IF1 trough serial.\n", __DATE__);		
		puts("Set HC using the command: LOAD *\"b\" for Program or LOAD *\"b\"CODE for Bytes.");		
		puts("Usage: <filename> [-a <address>] [-t <0=Program|3=Bytes>] [-l lenght] [-c <COM1>] [-b 9600] [-d HC2PC|PC2HC] [-nh]");
		return -1;
	}

	return 0;
}
*/