#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>

#include <windows.h>

#include "types.h"
#include "CFileIF1.h"
#include "CFileArchiveTape.h"

char port[10] = "\\\\.\\COM01";
const byte TIMEOUT_SEC = 60;
HANDLE m_hCom;

BOOL SetCOMForIF1(char* m_sComPort, int baud)
{
	// variables used with the com port
	BOOL     m_bPortReady;	
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
		timeouts.ReadTotalTimeoutConstant = timeouts.WriteTotalTimeoutConstant = TIMEOUT_SEC * 1000;
		SetCommTimeouts(m_hCom, &timeouts);
	}	

	return m_bPortReady;
}

BOOL CloseCOM()
{
	BOOL res = TRUE;

	if (m_hCom != INVALID_HANDLE_VALUE)
		res = CloseHandle(m_hCom);

	m_hCom = INVALID_HANDLE_VALUE;

	return res;
}

bool SendByteToIF1(HANDLE m_hCom, byte b)
{
	DWORD iBytesWritten = 1;
	BYTE b1 = b;
	return WriteFile(m_hCom, &b1, 1, &iBytesWritten, NULL) && iBytesWritten == 1;
}

bool ReadByteFromIF1(HANDLE m_hCom, byte* b)
{
	DWORD iBytesRead = 1;
	return ReadFile(m_hCom, b, 1, &iBytesRead, NULL) && iBytesRead == 1;
}

//TODO:
//1. Create a BASIC loader that includes the COM routine for 19200 BAUD using BIN2REM.
//2. Send the above block to HC using 4800 BAUD.
//3. Send the main game block using 19200 BAUD.


bool SendFileToIF1(CFileSpectrumTape* fs, const char* comName, dword baudWanted)
{		
	strncpy(port, comName, sizeof(port)-1);
	SetCOMForIF1(port, baudWanted);
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
			printf("\rSent %u/%u bytes (%0.2f%%), %d seconds left.", idx, len, (float)idx/len * 100, ((len-idx)*10/baudWanted));
	}

	if (!res)
		puts("Transfer error.");
	else if (_kbhit())
	{
		res = false;
		puts("Transfer canceled.");
	}
	
	delete[] data;	

	CloseCOM();

	return res;
}


bool GetFileFromIF1(CFileSpectrumTape* fst, const char* comName, dword baudWanted)
{
	strncpy(port, comName, sizeof(port) - 1);
	SetCOMForIF1(port, baudWanted);
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
		CloseCOM();
		puts("Transfer error.");
		return false;
	}	
	else
	{		
		printf("Receiving file type '%s' with lenght %u.\n", 
			(fileIF1.IF1Header.Type < CFileSpectrum::SPECTRUM_UNTYPED ? CFileSpectrum::SPECTRUM_FILE_TYPE_NAMES[fileIF1.IF1Header.Type] : "?"),
			fileIF1.IF1Header.Length);
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
			printf("\rReceived %u/%u bytes (%0.2f%%), %d seconds left.", idx, len, (float)idx / len * 100, ((len - idx) * 10 / baudWanted));
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
	CloseCOM();

	return res;
}	

bool ReadBufferFromIF1(byte* bufDest, word bufLen)
{
	bool res = true;
	word idx = 0;
	
	while (res && idx < bufLen)
		res = ReadByteFromIF1(m_hCom, &bufDest[idx++]);
	
	return res;
}

bool WriteBufferToIF1(byte* bufSrc, word bufLen)
{
	bool res = true;
	word idx = 0;

	while (res && idx < bufLen)
		res = SendByteToIF1(m_hCom, bufSrc[idx++]);

	return res;
}