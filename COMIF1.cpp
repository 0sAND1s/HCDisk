#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>

#include <windows.h>

#include "types.h"
#include "CFileIF1.h"
#include "CFileArchiveTape.h"

char port[6] = "COM1";
HANDLE m_hCom;

HANDLE SetCOMForIF1(char* m_sComPort, int baud)
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
		//Timeout must be enabled for transfer direction HC2PC to work. Set it at 2 seconds.
		timeouts.ReadTotalTimeoutConstant = timeouts.WriteTotalTimeoutConstant = 2 * 1000;
		SetCommTimeouts(m_hCom, &timeouts);
	}


	return m_hCom;
}

bool SendByteToIF1(HANDLE m_hCom, byte b)
{
	DWORD iBytesWritten = 1;
	return WriteFile(m_hCom, &b, 1, &iBytesWritten, NULL) && iBytesWritten == 1;
}

bool ReadByteFromIF1(HANDLE m_hCom, byte *b)
{
	DWORD iBytesRead = 1;
	return ReadFile(m_hCom, b, 1, &iBytesRead, NULL) && iBytesRead == 1;
}

//TODO:
//1. Create a BASIC loader that includes the COM routine for 19200 BAUD using BIN2REM.
//2. Send the above block to HC using 4800 BAUD.
//3. Send the main game block using 19200 BAUD.


bool SendFileToIF1(CFileSpectrumTape* fs, byte comIdx, dword baudWanted)
{		
	sprintf(&port[3], "%d", comIdx);
	m_hCom = SetCOMForIF1(port, baudWanted);
	if (m_hCom == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't open port %s!", port);
		return false;
	}

	CFileIF1 fileIF1(*fs);	

	dword len = fs->GetLength();
	len += sizeof(fileIF1.IF1Header);
	byte* data = new byte[len];
	memcpy(data, &fileIF1.IF1Header, sizeof(fileIF1.IF1Header));
	bool res = fs->GetData(&data[sizeof(fileIF1.IF1Header)]);
	dword idx = 0;
	
	while (idx < len && res && !_kbhit())
	{
		res = SendByteToIF1(m_hCom, data[idx]);
		idx++;
		if (idx % 10 == 0)
			printf("\rSent %u/%u bytes (%0.2f%%).", idx, len, (float)idx/len * 100);
	}

	if (!res)
		puts("Transfer error.");
	else if (_kbhit())
	{
		res = false;
		puts("Transfer canceled.");
	}
	
	delete[] data;	

	CloseHandle(m_hCom);

	return res;
}


bool GetFileFromIF1(CFileSpectrumTape* fst, byte comIdx, dword baudWanted)
{
	sprintf(&port[3], "%d", comIdx);
	m_hCom = SetCOMForIF1(port, baudWanted);
	if (m_hCom == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't open port %s!", port);
		return false;
	}

	CFileIF1 fileIF1;	
	int idx = 0, len = sizeof(fileIF1.IF1Header);
	bool res = true;	

	//Read IF1 header to get file lenght.
	while (idx < len && res)
	{
		res = ReadByteFromIF1(m_hCom, ((byte*)&fileIF1.IF1Header) + idx);
		idx++;		
	}

	if (!res)
	{
		CloseHandle(m_hCom);
		puts("Transfer error.");
		return false;
	}	
	else
	{
		printf("Sending file type '%s' with lenght %u.\n", CFileSpectrum::SPECTRUM_FILE_TYPE_NAMES[fileIF1.IF1Header.Type], fileIF1.IF1Header.Length);
	}

	*(CFileSpectrum*)fst = (CFileSpectrum)fileIF1;

	len = fileIF1.IF1Header.Length;
	byte* buf = new byte[sizeof(fileIF1.IF1Header) + len];
	memcpy(buf, &fileIF1.IF1Header, sizeof(fileIF1.IF1Header));
	idx = 0;	

	while (idx < len && res && !_kbhit())
	{
		res = ReadByteFromIF1(m_hCom, &buf[idx++]);
		if (idx % 10 == 0)
			printf("\rReceived %u/%u bytes (%0.2f%%).", idx, len, (float)idx / len * 100);
	}	

	if (res && !_kbhit())
	{
		fst->SetData(buf, sizeof(fileIF1.IF1Header) + len);
		puts("Transfer finished.");
	}
	else
	{
		if (!res)
			puts("Transfer error.");
		else
			puts("Transfer canceled.");
	}
	
	delete[] buf;
	CloseHandle(m_hCom);

	return res;
}