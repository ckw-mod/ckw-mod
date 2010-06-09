/*-----------------------------------------------------------------------------
 * File: misc.cpp
 *-----------------------------------------------------------------------------
 * Copyright (c) 2004-2005  Kazuo Ishii <k-ishii@wb4.so-net.ne.jp>
 *				- original version
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *---------------------------------------------------------------------------*/
#include "ckw.h"
#include "rsrc.h"

static void __write_console_input(LPCWSTR str, DWORD length)
{
	if(!str || !length) return;

	INPUT_RECORD *p, *buf;
	DWORD	i = 0;
	p = buf = new INPUT_RECORD[ length ];

	for( ; i < length ; i++, p++) {
		p->EventType = KEY_EVENT;
		p->Event.KeyEvent.bKeyDown = TRUE;
		p->Event.KeyEvent.wRepeatCount = 1;
		p->Event.KeyEvent.wVirtualKeyCode = 0;
		p->Event.KeyEvent.wVirtualScanCode = 0;
		p->Event.KeyEvent.uChar.UnicodeChar = *str++;
		p->Event.KeyEvent.dwControlKeyState = 0;
	}

	WriteConsoleInput(gStdIn, buf, length, &length);
	delete [] buf;
}

/*----------*/
void	onPasteFromClipboard(HWND hWnd)
{
	bool	result = true;
	HANDLE	hMem;
	wchar_t	*ptr;

	if(! IsClipboardFormatAvailable(CF_UNICODETEXT))
		return;
	if(!OpenClipboard(hWnd)) {
		Sleep(10);
		if(!OpenClipboard(hWnd))
			return;
	}
	hMem = GetClipboardData(CF_UNICODETEXT);
	if(!hMem)
		result = false;
	if(result && !(ptr = (wchar_t*)GlobalLock(hMem)))
		result = false;
	if(result) {
		__write_console_input(ptr, (DWORD)wcslen(ptr));
		GlobalUnlock(hMem);
	}
	CloseClipboard();
}

/*----------*/
void	onDropFile(HDROP hDrop)
{
	DWORD	i, nb, len;
	wchar_t	wbuf[MAX_PATH+32];
	wchar_t* wp;

	nb = DragQueryFile(hDrop, (DWORD)-1, NULL, 0);
	for(i = 0 ; i < nb ; i++) {
		len = DragQueryFile(hDrop, i, NULL, 0);
		if(len < 1 || len > MAX_PATH)
			continue;
		wp = wbuf + 1;
		if(! DragQueryFile(hDrop, i, wp, MAX_PATH))
			continue;
		wp[len] = 0;
		while(*wp > 0x20) wp++;
		if(*wp) {
			wp = wbuf;
			len++;
			wp[0] = wp[len++] = L'\"';
		}
		else {
			wp = wbuf + 1;
		}
		wp[len++] = L' ';

		__write_console_input(wp, len);
	}
	DragFinish(hDrop);
}

/*----------*/
BOOL CALLBACK AboutDlgProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg) {
	case WM_INITDIALOG:
		{
			HWND hEdit = GetDlgItem(hWnd, IDC_EDIT1);
			SetWindowText(hEdit,
L"This program is free software; you can redistribute it and/or\r\n"
L"modify it under the terms of the GNU General Public License\r\n"
L"as published by the Free Software Foundation; either version 2\r\n"
L"of the License, or (at your option) any later version.\r\n"
L"\r\n"
L"This program is distributed in the hope that it will be useful,\r\n"
L"but WITHOUT ANY WARRANTY; without even the implied warranty of\r\n"
L"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\r\n"
L"See the GNU General Public License for more details.\r\n"
L"\r\n"
L"You should have received a copy of the GNU General Public License\r\n"
L"along with this program; if not, write to the Free Software Foundation, Inc.,\r\n"
L" 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA."
			);
		}
		return(TRUE);
	case WM_COMMAND:
		switch(LOWORD(wp)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hWnd, 0);
			return(TRUE);
		}
	}
	return(FALSE);
}

/*----------*/
void	sysmenu_init(HWND hWnd)
{
	MENUITEMINFO mii;
	HMENU hMenu = GetSystemMenu(hWnd, FALSE);

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_ID;

	#if 0 // preparing "Copy All" command (craftware)
	mii.fType = MFT_STRING;
	mii.wID = IDM_COPYALL;
	mii.dwTypeData = L"&Copy All";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	mii.fType = MFT_SEPARATOR;
	mii.wID = 0;
	mii.dwTypeData = 0;
	mii.cch = 0;
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);
	#endif

	mii.fType = MFT_STRING;
	mii.wID = IDM_ABOUT;
	mii.dwTypeData = L"&About";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	mii.fType = MFT_SEPARATOR;
	mii.wID = 0;
	mii.dwTypeData = 0;
	mii.cch = 0;
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);
}

/*----------*/
BOOL	onSysCommand(HWND hWnd, DWORD id)
{
	switch(id) {
	case IDM_COPYALL:
		return(TRUE);
	case IDM_ABOUT:
		DialogBox(GetModuleHandle(NULL),
			  MAKEINTRESOURCE(IDD_DIALOG1),
			  hWnd,
			  AboutDlgProc);
		return(TRUE);
	}
	return(FALSE);
}

/* EOF */
