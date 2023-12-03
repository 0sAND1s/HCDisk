#include "GUIUtil.h"

#include <conio.h>
#include <stdio.h>

#include <iostream>
#include <string>

#include <Windows.h>
#include <WinCon.h>

#include "types.h"

using namespace std;

bool GUIUtil::Confirm(char* msg)
{
	printf("%s Press [ENTER] to confirm, [ESC] to cancel.\n", msg);
	char c = getch();
	while (c != 27 && c != 13)
		c = getch();

	return c == 13;
}


void GUIUtil::PrintIntense(char* str)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hCMD = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleScreenBufferInfo(hCMD, &csbi))
	{
		SetConsoleTextAttribute(hCMD, csbi.wAttributes | FOREGROUND_INTENSITY);

		WriteConsole(hCMD, str, strlen(str), NULL, NULL);

		SetConsoleTextAttribute(hCMD, csbi.wAttributes);
	}
}

void GetWindowSize(int& lines, int& columns)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&csbi
	))
	{
		lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
	else
		lines = columns = 0;
}


void GUIUtil::TextViewer(string str)
{
	int winLines, winColumns;
	GetWindowSize(winLines, winColumns);

	const word lnCnt = winLines - 2;
	long off1 = 0, off2 = 0;
	long lnIdx = 0;
	long len = str.length();


	while (off1 < len && off2 != string::npos)
	{
		do
		{
			off2 = str.find("\r\n", off1);
			if (off1 != off2)
				cout << str.substr(off1, off2 - off1) << endl;
			else
				cout << endl;

			off1 = off2 + 2;
			lnIdx++;
		} while (off2 != string::npos && lnIdx < lnCnt && off1 < len);

		if (off1 < len && off2 != string::npos)
		{
			char buf[80];
			sprintf(buf, "Press a key to continue, ESC to cancel. Progress: %d%%\n", (int)(((float)off1 / len) * 100));
			PrintIntense(buf);
			char key = getch();
			if (key == 27)
				break;
			else
				lnIdx = 0;
		}
	}
}

