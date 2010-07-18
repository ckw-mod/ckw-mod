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

#include <string>

#include "ckw.h"
#include "rsrc.h"
#include "option.h"

BOOL init_options(ckOpt& opt);

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
		p->Event.KeyEvent.uChar.UnicodeChar = 0;
		p->Event.KeyEvent.dwControlKeyState = 0;
		if(*str == '\r') {
			str++;
			length--;
		}
		if(*str == '\n') {
			p->Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
			str++;
		} else {
			p->Event.KeyEvent.uChar.UnicodeChar = *str++;
		}
	}

	WriteConsoleInput(gStdIn, buf, length, &length);
	delete [] buf;
}

/*----------*/
void copyChar(wchar_t*& p, CHAR_INFO* src, SHORT start, SHORT end, bool ret)
{
	CHAR_INFO* pend = src + end;
	CHAR_INFO* test = src + start;
	CHAR_INFO* last = test-1;

	/* search last char */
	for( ; test <= pend ; test++) {
		if(test->Char.UnicodeChar > 0x20)
			last = test;
	}
	/* copy */
	for(test = src+start ; test <= last ; test++) {
		if(!(test->Attributes & COMMON_LVB_TRAILING_BYTE))
			*p++ = test->Char.UnicodeChar;
	}
	if(ret && last < pend) {
		*p++ = L'\r';
		*p++ = L'\n';
	}
	*p = 0;
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
/* (craftware) */
void copyStringToClipboard( HWND hWnd, const wchar_t * str )
{
	size_t length = wcslen(str) +1;
	HANDLE hMem;
	wchar_t* ptr;
	bool	result = true;

	hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * length);
	if(!hMem) result = false;

	if(result && !(ptr = (wchar_t*) GlobalLock(hMem))) {
		result = false;
	}
	if(result) {
		memcpy(ptr, str, sizeof(wchar_t) * length);
		GlobalUnlock(hMem);
	}
	if(result && !OpenClipboard(hWnd)) {
		Sleep(10);
		if(!OpenClipboard(hWnd))
			result = false;
	}
	if(result) {
		if(!EmptyClipboard() ||
		   !SetClipboardData(CF_UNICODETEXT, hMem))
			result = false;
		CloseClipboard();
	}
	if(!result && hMem) {
		GlobalFree(hMem);
	}
}

/*----------*/
/* (craftware) */
wchar_t * getAllString()
{
	int nb;

	nb = gCSI->dwSize.X * gCSI->dwSize.Y;

	COORD      size = { gCSI->dwSize.X, 1 };
	CHAR_INFO* work = new CHAR_INFO[ gCSI->dwSize.X ];
	wchar_t*   buffer = new wchar_t[ nb ];
	wchar_t*   wp = buffer;
	COORD      pos = { 0,0 };
	SMALL_RECT sr = { 0, 0, gCSI->dwSize.X-1, 0 };

	*wp = 0;

	for( int y=0 ; y<gCSI->dwSize.Y ; ++y )
	{
		sr.Top = sr.Bottom = y;
		ReadConsoleOutput_Unicode(gStdOut, work, size, pos, &sr);
		copyChar( wp, work, 0, gCSI->dwSize.X-1 );
	}

	delete [] work;

	return(buffer);
}

/*----------*/
/* (craftware) */
void copyAllStringToClipboard(HWND hWnd)
{
	wchar_t* str = getAllString();
	if(!str) return;
	
	std::wstring s = str;

	// skip empty line
	size_t begin = s.find_first_not_of(L"\r\n");
	size_t end = s.find_last_not_of(L"\r\n");
	if(begin!=s.npos && end!=s.npos)
	{
		s = s.substr( begin, end+1-begin );
	}

	copyStringToClipboard( hWnd, s.c_str() );

	delete [] str;
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
INT_PTR CALLBACK AboutDlgProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
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
void	sysmenu_init_topmost(HWND hWnd, HMENU hMenu);
void	sysmenu_init_subconfig(HWND hWnd, HMENU hMenu);
void	changeStateTopMostMenu(HWND hWnd, HMENU hMenu);

void	sysmenu_init(HWND hWnd)
{
	MENUITEMINFO mii;
	HMENU hMenu = GetSystemMenu(hWnd, FALSE);

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_ID;

	mii.fType = MFT_STRING;
	mii.wID = IDM_COPYALL;
	mii.dwTypeData = L"Copy All(&C)";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	mii.fType = MFT_STRING;
	mii.wID = IDM_NEW;
	mii.dwTypeData = L"New (&N)";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	sysmenu_init_topmost(hWnd, hMenu);

    // sysmenu_init_subconfig(hWnd, hMenu);

	mii.fType = MFT_SEPARATOR;
	mii.wID = 0;
	mii.dwTypeData = 0;
	mii.cch = 0;
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	mii.fType = MFT_STRING;
	mii.wID = IDM_ABOUT;
	mii.dwTypeData = L"About (&A)";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	mii.fType = MFT_SEPARATOR;
	mii.wID = 0;
	mii.dwTypeData = 0;
	mii.cch = 0;
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);
}

void    get_directory_path(wchar_t *path)
{
	wchar_t *c;
	GetModuleFileName(NULL, path, MAX_PATH);
	c = wcsrchr(path, L'\\');
	if(c) *c = 0;
}

void	sysmenu_init_topmost(HWND hWnd, HMENU hMenu)
{
	MENUITEMINFO mii;

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_ID | MIIM_CHECKMARKS;

	mii.fType = MFT_STRING;
	mii.wID = IDM_TOPMOST;
	mii.dwTypeData = L"TopMost (&T)";
	mii.cch = (UINT) wcslen(mii.dwTypeData);

	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);

	changeStateTopMostMenu(hWnd,hMenu);
}

void	sysmenu_init_subconfig(HWND hWnd, HMENU hMenu)
{
	MENUITEMINFO mii;
    HMENU hSubMenu = CreatePopupMenu();

	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_ID;

	wchar_t path[MAX_PATH+1];
    get_directory_path(path);
    wcscat_s(path, L"\\*.cfg");

    WIN32_FIND_DATA fd;
	memset(&fd, 0, sizeof(fd));
    HANDLE hFile = FindFirstFile(path, &fd);
    //MessageBox(hWnd, path, L"", MB_OK);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        for(int i = 0;;i++)
        {
            mii.fType = MFT_STRING;
            mii.wID = IDM_CONFIG_SELECT_1 + i;
            mii.dwTypeData = fd.cFileName;
            mii.cch = (UINT) wcslen(mii.dwTypeData);
            InsertMenuItem(hSubMenu, 0, TRUE, &mii);
            if(FindNextFile(hFile, &fd) == 0) { break; }
        }
        FindClose(hFile);
    }

	mii.fMask |= MIIM_SUBMENU;
	mii.fType = MFT_STRING;
	mii.wID = IDM_CONFIG_SELECT;
	mii.hSubMenu = hSubMenu;
	mii.dwTypeData = L"Config (&O)";
	mii.cch = (UINT) wcslen(mii.dwTypeData);
	InsertMenuItem(hMenu, SC_CLOSE, FALSE, &mii);
}

void reloadConfig(wchar_t *path)
{
	char filepath[MAX_PATH+1];
	wcstombs(filepath, path, MAX_PATH);

	ckOpt opt;
	opt.setFile(filepath);
    init_options(opt);
}

BOOL    onConfigMenuCommand(HWND hWnd, DWORD id)
{
	wchar_t path[MAX_PATH+1];
    get_directory_path(path);

	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE | MIIM_ID;
    mii.fType = MFT_STRING;

	HMENU hMenu = GetSystemMenu(hWnd, FALSE);
    if (GetMenuItemInfo(hMenu, id, 0, &mii))
    {
        wchar_t sz[MAX_PATH+1], filepath[MAX_PATH+1];
        mii.dwTypeData = sz;
        mii.cch++;
        GetMenuItemInfo(hMenu, id, 0, &mii);
        wsprintf(filepath, L"%s\\%s", path, sz);
        /* need to open file and update config here */
        MessageBox(hWnd, filepath, L"", MB_OK);
		reloadConfig(filepath);
    }

    return(TRUE);
}

BOOL	onTopMostMenuCommand(HWND hWnd)
{
	HMENU hMenu = GetSystemMenu(hWnd, FALSE);

	UINT uState = GetMenuState( hMenu, IDM_TOPMOST, MF_BYCOMMAND);
	DWORD dwExStyle = GetWindowLong(hWnd,GWL_EXSTYLE);
	if( uState & MFS_CHECKED )
	{
		SetWindowPos(hWnd, HWND_NOTOPMOST,NULL,NULL,NULL,NULL,SWP_NOMOVE | SWP_NOSIZE); 
	}else{
		SetWindowPos(hWnd, HWND_TOPMOST,NULL,NULL,NULL,NULL,SWP_NOMOVE | SWP_NOSIZE); 
	}

	changeStateTopMostMenu(hWnd, hMenu);

	return(TRUE);
}

void	changeStateTopMostMenu(HWND hWnd,HMENU hMenu)
{
	DWORD dwExStyle = GetWindowLong(hWnd,GWL_EXSTYLE);

	if ((dwExStyle & WS_EX_TOPMOST) == 0) {
		CheckMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MFS_UNCHECKED);
	} else {
		CheckMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MFS_CHECKED);
	}
}

/*----------*/
BOOL	onSysCommand(HWND hWnd, DWORD id)
{
	switch(id) {
	case IDM_COPYALL:
		copyAllStringToClipboard(hWnd);
		return(TRUE);
	case IDM_ABOUT:
		DialogBox(GetModuleHandle(NULL),
			  MAKEINTRESOURCE(IDD_DIALOG1),
			  hWnd,
			  AboutDlgProc);
		return(TRUE);
	case IDM_NEW:
		makeNewWindow();
		return(TRUE);
	case IDM_TOPMOST:
		return onTopMostMenuCommand(hWnd);
	}
    if(IDM_CONFIG_SELECT < id && id <= IDM_CONFIG_SELECT_MAX) {
        return onConfigMenuCommand(hWnd, id);
    }
	return(FALSE);
}

/* EOF */
