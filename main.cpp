/*-----------------------------------------------------------------------------
 * File: main.cpp
 *-----------------------------------------------------------------------------
 * Copyright (c) 2005       Kazuo Ishii <k-ishii@wb4.so-net.ne.jp>
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
#include "ime_wrap.h"
#include "rsrc.h"

/*****************************************************************************/

HANDLE	gStdIn = NULL;	/* console */
HANDLE	gStdOut = NULL;
HANDLE	gStdErr = NULL;
HWND	gConWnd = NULL;

HANDLE	gChild = NULL;	/* child process */

LOGFONT	gFontLog;	/* font IME */
HFONT	gFont;		/* font */
DWORD	gFontW;		/* char width */
DWORD	gFontH;		/* char height */

DWORD	gWinW;		/* window columns */
DWORD	gWinH;		/* window rows */

RECT	gFrame;		/* window frame size */
HBITMAP gBgBmp = NULL;	/* background image */
HBRUSH	gBgBrush = NULL;/* background brush */
DWORD	gBorderSize = 0;/* internal border */
DWORD	gLineSpace = 0;	/* line space */
BOOL	gVScrollHide = FALSE;

BOOL	gImeOn = FALSE; /* IME-status */

/* screen buffer - copy */
CONSOLE_SCREEN_BUFFER_INFO* gCSI = NULL;
CHAR_INFO*	gScreen = NULL;
wchar_t*	gTitle = NULL;

/* setConsoleFont */
#define MAX_FONTS 128

typedef struct _CONSOLE_FONT {
  DWORD index;
  COORD dim;
} CONSOLE_FONT, *PCONSOLE_FONT;

typedef BOOL  (WINAPI *GetConsoleFontInfoT)( HANDLE,BOOL,DWORD,PCONSOLE_FONT );
typedef DWORD (WINAPI *GetNumberOfConsoleFontsT)( VOID );
typedef BOOL  (WINAPI *SetConsoleFontT)( HANDLE, DWORD );

GetConsoleFontInfoT		GetConsoleFontInfo;
GetNumberOfConsoleFontsT	GetNumberOfConsoleFonts;
SetConsoleFontT			SetConsoleFont;

/* index color */
enum {
	kColor0 = 0,
	          kColor1,  kColor2,  kColor3,
	kColor4,  kColor5,  kColor6,  kColor7,
	kColor8,  kColor9,  kColor10, kColor11,
	kColor12, kColor13, kColor14, kColor15,
	kColorCursorFg,
	kColorCursorBg,
	kColorCursorImeFg,
	kColorCursorImeBg,
	/**/
	kColorMax,
};
COLORREF gColorTable[ kColorMax ];


/*****************************************************************************/

#if 0
#include <stdio.h>
void trace(const char *msg)
{
	fputs(msg, stdout);
	fflush(stdout);
}
#else
#define trace(msg)
#endif

/*****************************************************************************/

BOOL WINAPI ReadConsoleOutput_Unicode(HANDLE con, CHAR_INFO* buffer,
				      COORD size, COORD pos, SMALL_RECT *sr)
{
	if(!ReadConsoleOutputA(con, buffer, size, pos, sr))
		return(FALSE);

	CHAR_INFO* s = buffer;
	CHAR_INFO* e = buffer + (size.X * size.Y);
	DWORD	codepage = GetConsoleOutputCP();
	BYTE	ch[2];
	WCHAR	wch;

	while(s < e) {
		ch[0] = s->Char.AsciiChar;

		if(s->Attributes & COMMON_LVB_LEADING_BYTE) {
			if((s+1) < e && ((s+1)->Attributes & COMMON_LVB_TRAILING_BYTE)) {
				ch[1] = (s+1)->Char.AsciiChar;
				if(MultiByteToWideChar(codepage, 0, (LPCSTR)ch, 2, &wch, 1)) {
					s->Char.UnicodeChar = wch;
					s++;
					s->Char.UnicodeChar = wch;
					s++;
					continue;
				}
			}
		}

		if(MultiByteToWideChar(codepage, 0, (LPCSTR)ch, 1, &wch, 1)) {
			s->Char.UnicodeChar = wch;
		}
		s->Attributes &= ~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
		s++;
	}
	return(TRUE);
}

/*****************************************************************************/

/*----------*/
inline void __draw_invert_char_rect(HDC hDC, RECT& rc)
{
	rc.right++;
	rc.bottom++;
	rc.left   *= gFontW;
	rc.right  *= gFontW;
	rc.top    *= gFontH;
	rc.bottom *= gFontH;
	BitBlt(hDC, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, NULL,0,0, DSTINVERT);
}

/*----------*/
static void __draw_selection(HDC hDC)
{
	SMALL_RECT sel;
	if(!selectionGetArea(sel))
		return;

	if(gCSI->srWindow.Top <= sel.Top && sel.Top <= gCSI->srWindow.Bottom)
		;
	else if(gCSI->srWindow.Top <= sel.Bottom && sel.Bottom <= gCSI->srWindow.Bottom)
		;
	else if(sel.Top < gCSI->srWindow.Top && gCSI->srWindow.Bottom < sel.Bottom)
		;
	else
		return;

	RECT	rc;

	if(sel.Top == sel.Bottom) {
		/* single line */
		rc.left  = sel.Left - gCSI->srWindow.Left;
		rc.right = sel.Right-1 - gCSI->srWindow.Left;
		rc.top   = \
		rc.bottom = sel.Top - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
		return;
	}

	/* multi line */
	if(gCSI->srWindow.Top <= sel.Top && sel.Top <= gCSI->srWindow.Bottom) {
		/* top */
		rc.left = sel.Left - gCSI->srWindow.Left;
		rc.right = gCSI->srWindow.Right - gCSI->srWindow.Left;
		rc.top = \
		rc.bottom = sel.Top - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
	if(sel.Top+1 <= sel.Bottom-1) {
		/* center */
		rc.left = 0;
		rc.right = gCSI->srWindow.Right - gCSI->srWindow.Left;

		if(gCSI->srWindow.Top <= sel.Top+1)
			rc.top = sel.Top+1 - gCSI->srWindow.Top;
		else
			rc.top = 0;

		if(gCSI->srWindow.Bottom >= sel.Bottom-1)
			rc.bottom = sel.Bottom-1 - gCSI->srWindow.Top;
		else
			rc.bottom = gCSI->srWindow.Bottom - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
	if(gCSI->srWindow.Top <= sel.Bottom && sel.Bottom <= gCSI->srWindow.Bottom) {
		/* bottom */
		rc.left = 0;
		rc.right = sel.Right-1 - gCSI->srWindow.Left;
		rc.top = \
		rc.bottom = sel.Bottom - gCSI->srWindow.Top;
		__draw_invert_char_rect(hDC, rc);
	}
}

/*----------*/
static void __draw_screen(HDC hDC)
{
	int	pntX, pntY;
	int	x, y;
	int	color_fg;
	int	color_bg;
	CHAR_INFO* ptr = gScreen;
	int	 work_color_fg = -1;
	int	 work_color_bg = -1;
	wchar_t* work_text = new wchar_t[ CSI_WndCols(gCSI) ];
	wchar_t* work_text_ptr;
	INT*	 work_width = new INT[ CSI_WndCols(gCSI) ];
	INT*	 work_width_ptr;
	int	 work_pntX;

	pntY = 0;
	for(y = gCSI->srWindow.Top ; y <= gCSI->srWindow.Bottom ; y++) {
		pntX = 0;
		work_pntX = 0;
		work_text_ptr = work_text;
		work_width_ptr = work_width;
		for(x = gCSI->srWindow.Left ; x <= gCSI->srWindow.Right ; x++) {

			if(ptr->Attributes & COMMON_LVB_TRAILING_BYTE) {
				pntX += gFontW;
				ptr++;
				continue;
			}

			color_fg = ptr->Attributes & 0xF;
			color_bg = (ptr->Attributes>>4) & 0xF;

			if(color_fg != work_color_fg ||
			   color_bg != work_color_bg) {
				if(work_text_ptr > work_text) {
					ExtTextOut(hDC, work_pntX, pntY, 0, NULL,
						   (LPCWSTR)work_text,
						   (UINT)(work_text_ptr - work_text),
						   work_width);
				}
				work_text_ptr = work_text;
				work_width_ptr = work_width;
				work_pntX = pntX;
				work_color_fg = color_fg;
				work_color_bg = color_bg;
				SetTextColor(hDC, gColorTable[work_color_fg]);
				SetBkColor(  hDC, gColorTable[work_color_bg]);
				SetBkMode(hDC, (work_color_bg) ? OPAQUE : TRANSPARENT);
			}

			if(ptr->Attributes & COMMON_LVB_LEADING_BYTE) {
				*work_text_ptr++ = ptr->Char.UnicodeChar;
				*work_width_ptr++ = gFontW * 2;
			}
			else {
				*work_text_ptr++ = ptr->Char.UnicodeChar;
				*work_width_ptr++ = gFontW;
			}
			pntX += gFontW;
			ptr++;
		}

		if(work_text_ptr > work_text) {
			ExtTextOut(hDC, work_pntX, pntY, 0, NULL,
				   (LPCWSTR)work_text,
				   (UINT)(work_text_ptr - work_text),
				   work_width);
		}

		pntY += gFontH;
	}

	/* draw selection */
	__draw_selection(hDC);

	/* draw cursor */
	if(gCSI->srWindow.Top    <= gCSI->dwCursorPosition.Y &&
	   gCSI->srWindow.Bottom >= gCSI->dwCursorPosition.Y &&
	   gCSI->srWindow.Left   <= gCSI->dwCursorPosition.X &&
	   gCSI->srWindow.Right  >= gCSI->dwCursorPosition.X) {
		color_fg = (gImeOn) ? kColorCursorImeFg : kColorCursorFg;
		color_bg = (gImeOn) ? kColorCursorImeBg : kColorCursorBg;
		SetTextColor(hDC, gColorTable[ color_fg ]);
		SetBkColor(  hDC, gColorTable[ color_bg ]);
		SetBkMode(hDC, OPAQUE);
		pntX = gCSI->dwCursorPosition.X - gCSI->srWindow.Left;
		pntY = gCSI->dwCursorPosition.Y - gCSI->srWindow.Top;
		ptr = gScreen + CSI_WndCols(gCSI) * pntY + pntX;
		pntX *= gFontW;
		pntY *= gFontH;
		*work_width = (ptr->Attributes & COMMON_LVB_LEADING_BYTE) ? gFontW*2 : gFontW;
		ExtTextOut(hDC, pntX, pntY, 0, NULL,
			   &ptr->Char.UnicodeChar, 1, work_width);
	}

	delete [] work_width;
	delete [] work_text;
}

/*----------*/
void	onPaint(HWND hWnd)
{
	PAINTSTRUCT ps;
	HDC	hDC = BeginPaint(hWnd, &ps);
	RECT	rc;
	GetClientRect(hWnd, &rc);

	HDC	hMemDC = CreateCompatibleDC(hDC);
	HBITMAP	hBmp = CreateCompatibleBitmap(hDC, rc.right-rc.left, rc.bottom-rc.top);
	HGDIOBJ	oldfont = SelectObject(hMemDC, gFont);
	HGDIOBJ	oldbmp  = SelectObject(hMemDC, hBmp);

	FillRect(hMemDC, &rc, gBgBrush);

	if(gScreen && gCSI) {
		SetWindowOrgEx(hMemDC, -(int)gBorderSize, -(int)gBorderSize, NULL);
		__draw_screen(hMemDC);
		SetWindowOrgEx(hMemDC, 0, 0, NULL);
	}

	BitBlt(hDC,rc.left,rc.top, rc.right-rc.left, rc.bottom-rc.top, hMemDC,0,0, SRCCOPY);

	SelectObject(hMemDC, oldfont);
	SelectObject(hMemDC, oldbmp);
	DeleteObject(hBmp);
	DeleteDC(hMemDC);

	EndPaint(hWnd, &ps);
}

/*----------*/
static void __set_console_window_size(LONG cols, LONG rows)
{
	CONSOLE_SCREEN_BUFFER_INFO csi;
	GetConsoleScreenBufferInfo(gStdOut, &csi);

	gWinW = cols;
	gWinH = rows;

	if(cols == CSI_WndCols(&csi) && rows == CSI_WndRows(&csi))
		return;

	//SMALL_RECT tmp = { 0,0,0,0 };
	//SetConsoleWindowInfo(gStdOut, TRUE, &tmp);

	csi.dwSize.X = (SHORT)cols;
	csi.srWindow.Left = 0;
	csi.srWindow.Right = (SHORT)(cols -1);

	if(csi.dwSize.Y < rows || csi.dwSize.Y == CSI_WndRows(&csi))
		csi.dwSize.Y = (SHORT)rows;

	csi.srWindow.Bottom += (SHORT)(rows - CSI_WndRows(&csi));
	if(csi.dwSize.Y <= csi.srWindow.Bottom) {
		csi.srWindow.Top -= csi.srWindow.Bottom - csi.dwSize.Y +1;
		csi.srWindow.Bottom = csi.dwSize.Y -1;
	}

	SetConsoleScreenBufferSize(gStdOut, csi.dwSize);
	SetConsoleWindowInfo(gStdOut, TRUE, &csi.srWindow);
}

/*----------*/
void	onSizing(HWND hWnd, DWORD side, LPRECT rc)
{
	trace("onSizing\n");
	LONG fw = (gFrame.right - gFrame.left) + (gBorderSize * 2);
	LONG fh = (gFrame.bottom - gFrame.top) + (gBorderSize * 2);
	LONG width  = rc->right - rc->left;
	LONG height = rc->bottom - rc->top;

	width  -= fw;
	width  -= width  % gFontW;
	width  += fw;

	height -= fh;
	height -= height % gFontH;
	height += fh;

	if(side==WMSZ_LEFT || side==WMSZ_TOPLEFT || side==WMSZ_BOTTOMLEFT)
		rc->left = rc->right - width;
	else
		rc->right = rc->left + width;

	if(side==WMSZ_TOP || side==WMSZ_TOPLEFT || side==WMSZ_TOPRIGHT)
		rc->top = rc->bottom - height;
	else
		rc->bottom = rc->top + height;
}

/*----------*/
void	onWindowPosChange(HWND hWnd, WINDOWPOS* wndpos)
{
	trace("onWindowPosChange\n");
	if(!(wndpos->flags & SWP_NOSIZE) && !IsIconic(hWnd)) {
		LONG fw = (gFrame.right - gFrame.left) + (gBorderSize * 2);
		LONG fh = (gFrame.bottom - gFrame.top) + (gBorderSize * 2);
		LONG width  = wndpos->cx;
		LONG height = wndpos->cy;
		width  = (width - fw) / gFontW;
		height = (height - fh) / gFontH;

		__set_console_window_size(width, height);

		wndpos->cx = width  * gFontW + fw;
		wndpos->cy = height * gFontH + fh;
	}
}

static void __set_ime_position(HWND hWnd)
{
	if(!gImeOn || !gCSI) return;
	HIMC imc = ImmGetContext(hWnd);
	LONG px = gCSI->dwCursorPosition.X - gCSI->srWindow.Left;
	LONG py = gCSI->dwCursorPosition.Y - gCSI->srWindow.Top;
	COMPOSITIONFORM cf;
	cf.dwStyle = CFS_POINT;
	cf.ptCurrentPos.x = px * gFontW + gBorderSize;
	cf.ptCurrentPos.y = py * gFontH + gBorderSize;
	ImmSetCompositionWindow(imc, &cf);
	ImmReleaseContext(hWnd, imc);
}

/*----------*/
void	onTimer(HWND hWnd)
{
	if(WaitForSingleObject(gChild, 0) != WAIT_TIMEOUT) {
		PostMessage(hWnd, WM_CLOSE, 0,0);
		return;
	}

	/* refresh handle */
	if(gStdOut) CloseHandle(gStdOut);
	gStdOut = CreateFile(L"CONOUT$", GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     NULL, OPEN_EXISTING, 0, NULL);

	/* title update */
	static int timer_count = 0;
	if((++timer_count & 0xF) == 1) {
		wchar_t *str = new wchar_t[256];
		GetConsoleTitle(str, 256);
		if(gTitle && !wcscmp(gTitle, str)) {
			delete [] str;
		}
		else {
			delete [] gTitle;
			gTitle = str;
			SetWindowText(hWnd, gTitle);
		}
	}

	CONSOLE_SCREEN_BUFFER_INFO* csi = new CONSOLE_SCREEN_BUFFER_INFO;
	COORD	size;

	GetConsoleScreenBufferInfo(gStdOut, csi);
	size.X = CSI_WndCols(csi);
	size.Y = CSI_WndRows(csi);

	/* copy screen buffer */
	DWORD      nb = size.X * size.Y;
	CHAR_INFO* buffer = new CHAR_INFO[nb];
	CHAR_INFO* ptr = buffer;
	SMALL_RECT sr;
	COORD      pos = { 0, 0 };

	/* ReadConsoleOuput - maximum read size 64kByte?? */
	size.Y = 0x8000 / sizeof(CHAR_INFO) / size.X;
	sr.Left  = csi->srWindow.Left;
	sr.Right = csi->srWindow.Right;
	sr.Top   = csi->srWindow.Top;
	do {
		sr.Bottom = sr.Top + size.Y -1;
		if(sr.Bottom > csi->srWindow.Bottom) {
			sr.Bottom = csi->srWindow.Bottom;
			size.Y = sr.Bottom - sr.Top +1;
		}
		ReadConsoleOutput_Unicode(gStdOut, ptr, size, pos, &sr);
		ptr += size.X * size.Y;
		sr.Top = sr.Bottom +1;
	} while(sr.Top <= csi->srWindow.Bottom);

	/* compare */
	if(gScreen && gCSI &&
	   !memcmp(csi, gCSI, sizeof(CONSOLE_SCREEN_BUFFER_INFO)) &&
	   !memcmp(buffer, gScreen, sizeof(CHAR_INFO) * nb)) {
		/* no modified */
		delete [] buffer;
		delete csi;
		return;
	}

	/* swap buffer */
	if(gScreen) delete [] gScreen;
	if(gCSI) delete gCSI;
	gScreen = buffer;
	gCSI = csi;

	/* redraw request */
	InvalidateRect(hWnd, NULL, TRUE);

	/* set vertical scrollbar status */
	if(!gVScrollHide) {
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_PAGE | SIF_RANGE;
		si.nPos = gCSI->srWindow.Top;
		si.nPage = CSI_WndRows(gCSI);
		si.nMin = 0;
		si.nMax = gCSI->dwSize.Y-1;
		SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
	}

	if(gImeOn) {
		__set_ime_position(hWnd);
	}

	int w = CSI_WndCols(gCSI);
	int h = CSI_WndRows(gCSI);
	if(gWinW != w || gWinH != h) {
		w = (w * gFontW) + (gBorderSize * 2) + (gFrame.right - gFrame.left);
		h = (h * gFontH) + (gBorderSize * 2) + (gFrame.bottom - gFrame.top);
		SetWindowPos(hWnd, NULL, 0,0,w,h, SWP_NOMOVE|SWP_NOZORDER);
	}
}

/*****************************************************************************/

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg) {
	case WM_CREATE:
		{
			HIMC imc = ImmGetContext(hWnd);
			ImmSetCompositionFontW(imc, &gFontLog);
			ImmReleaseContext(hWnd, imc);
		}
		SetTimer(hWnd, 0x3571, 10, NULL);
		break;
	case WM_DESTROY:
		KillTimer(hWnd, 0x3571);
		PostQuitMessage(0);
		if(WaitForSingleObject(gChild, 0) == WAIT_TIMEOUT)
			TerminateProcess(gChild, 0);
		break;
	case WM_TIMER:
		onTimer(hWnd);
		break;

	case WM_ERASEBKGND:
		break;
	case WM_PAINT:
		onPaint(hWnd);
		break;

	case WM_SIZING:
		onSizing(hWnd, (DWORD)wp, (LPRECT)lp);
		break;
	case WM_WINDOWPOSCHANGING:
	case WM_WINDOWPOSCHANGED:
		onWindowPosChange(hWnd, (WINDOWPOS*)lp);
		selectionClear(hWnd);
		break;
	case WM_LBUTTONDOWN:
		onLBtnDown(hWnd, (short)LOWORD(lp), (short)HIWORD(lp));
		break;
	case WM_LBUTTONUP:
		onLBtnUp(hWnd, (short)LOWORD(lp), (short)HIWORD(lp));
		break;
	case WM_MOUSEMOVE:
		onMouseMove(hWnd, (short)LOWORD(lp),(short)HIWORD(lp));
		// scroll when mouse is outside (craftware)
		{
			short x = (short)LOWORD(lp);
			short y = (short)HIWORD(lp);

			RECT rc;
			GetClientRect(hWnd, &rc);

			if( y<0 ) {
				PostMessage(gConWnd, WM_MOUSEWHEEL, WHEEL_DELTA<<16, y<<16|x );
			}
			else if(y>=rc.bottom) {
				PostMessage(gConWnd, WM_MOUSEWHEEL, -WHEEL_DELTA<<16, y<<16|x );
			}
		}
		break;
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		onPasteFromClipboard(hWnd);
		break;
	case WM_DROPFILES:
		onDropFile((HDROP)wp);
		break;

	case WM_IME_STARTCOMPOSITION:
		__set_ime_position(hWnd);
		return( DefWindowProc(hWnd, msg, wp, lp) );
	case WM_IME_NOTIFY:
		if(wp == IMN_SETOPENSTATUS) {
			HIMC imc = ImmGetContext(hWnd);
			gImeOn = ImmGetOpenStatus(imc);
			ImmReleaseContext(hWnd, imc);
			InvalidateRect(hWnd, NULL, TRUE);
		}
		return( DefWindowProc(hWnd, msg, wp, lp) );

	case WM_SYSCOMMAND:
		if(!onSysCommand(hWnd, (DWORD)wp))
			return( DefWindowProc(hWnd, msg, wp, lp) );
		break;
	case WM_VSCROLL:
	case WM_MOUSEWHEEL:
		/* throw console window */
		PostMessage(gConWnd, msg, wp, lp);
		break;

	case WM_IME_CHAR:
		PostMessage(gConWnd, msg, wp, lp);
		/* break */
	case WM_CHAR:
		selectionClear(hWnd);
		break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		if(wp != VK_RETURN) /* alt+enter */
			PostMessage(gConWnd, msg, wp, lp);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		if((wp == VK_NEXT || wp == VK_PRIOR ||
		    wp == VK_HOME || wp == VK_END) &&
		   (GetKeyState(VK_SHIFT) & 0x8000)) {
			if(msg == WM_KEYDOWN) {
				WPARAM  sb = SB_PAGEDOWN;
				if(wp == VK_PRIOR)     sb = SB_PAGEUP;
				else if(wp == VK_HOME) sb = SB_TOP;
				else if(wp == VK_END)  sb = SB_BOTTOM;
				PostMessage(gConWnd, WM_VSCROLL, sb, 0);
			}
		}
		else if(wp == VK_INSERT &&
			(GetKeyState(VK_SHIFT) & 0x8000)) {
			if(msg == WM_KEYDOWN)
				onPasteFromClipboard(hWnd);
		}
		else {
			PostMessage(gConWnd, msg, wp, lp);
		}
		break;
	default:
		return( DefWindowProc(hWnd, msg, wp, lp) );
	}
	return(1);
}

/*****************************************************************************/
#include "option.h"

/*----------*/
static BOOL create_window(ckOpt& opt)
{
	trace("create_window\n");

	HINSTANCE hInstance = GetModuleHandle(NULL);
	LPWSTR	className = L"CkwWindowClass";
	const char*	conf_title;
	LPWSTR	title;
	WNDCLASSEX wc;
	DWORD	style = WS_OVERLAPPEDWINDOW;
	DWORD	exstyle = WS_EX_ACCEPTFILES;
	LONG	width, height;
	LONG	posx, posy;

	if(opt.isTranspColor() ||
	   (0 < opt.getTransp() && opt.getTransp() < 255))
		exstyle |= WS_EX_LAYERED;

	if(opt.isScrollRight())
		exstyle |= WS_EX_RIGHTSCROLLBAR;
	else
		exstyle |= WS_EX_LEFTSCROLLBAR;

	if(opt.isTopMost())
		exstyle |= WS_EX_TOPMOST;

	if(opt.isScrollHide() || opt.getSaveLines() < 1)
		gVScrollHide = TRUE;
	else
		style |= WS_VSCROLL;

	if(opt.isIconic())
		style |= WS_MINIMIZE;

	conf_title = opt.getTitle();
	if(!conf_title || !conf_title[0]){
          title = L"ckw";
        }else{
          title = new wchar_t[ strlen(conf_title)+1 ];
          ZeroMemory(title, sizeof(wchar_t) * (strlen(conf_title)+1));
          MultiByteToWideChar(CP_ACP, 0, conf_title, (int)strlen(conf_title), title, (int)(sizeof(wchar_t) * (strlen(conf_title)+1)) );
        }

	/* calc window size */
	CONSOLE_SCREEN_BUFFER_INFO csi;
	GetConsoleScreenBufferInfo(gStdOut, &csi);

	AdjustWindowRectEx(&gFrame, style, FALSE, exstyle);
	if(!gVScrollHide)
		gFrame.right += GetSystemMetrics(SM_CXVSCROLL);

	gWinW = width  = csi.srWindow.Right  - csi.srWindow.Left + 1;
	gWinH = height = csi.srWindow.Bottom - csi.srWindow.Top  + 1;
	width  *= gFontW;
	height *= gFontH;
	width  += gBorderSize * 2;
	height += gBorderSize * 2;
	width  += gFrame.right  - gFrame.left;
	height += gFrame.bottom - gFrame.top;

	if(opt.isWinPos()) {
		RECT	rc;
		SystemParametersInfo(SPI_GETWORKAREA,0,(LPVOID)&rc,0);
		posx = opt.getWinPosX();
		if(posx < 0) posx = rc.right - (width - posx -1);
		else         posx += rc.left;
		if(posx < rc.left) posx = rc.left;
		if(posx > rc.right-5) posx = rc.right -5;
		posy = opt.getWinPosY();
		if(posy < 0) posy = rc.bottom - (height - posy -1);
		else         posy += rc.top;
		if(posy < rc.top) posy = rc.top;
		if(posy > rc.bottom-5) posy = rc.bottom -5;
	}
	else {
		posx = CW_USEDEFAULT;
		posy = CW_USEDEFAULT;
	}

	/**/
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, (LPCTSTR)IDR_ICON);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(gColorTable[0]);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = className;
	wc.hIconSm = NULL;
	if(! RegisterClassEx(&wc))
		return(FALSE);

	HWND hWnd = CreateWindowEx(exstyle, className, title, style,
				   posx, posy, width, height,
				   NULL, NULL, hInstance, NULL);
	if(!hWnd){
		delete [] title;
		return(FALSE);
        }

	sysmenu_init(hWnd);

	if(0 < opt.getTransp() && opt.getTransp() < 255)
		SetLayeredWindowAttributes(hWnd, 0, opt.getTransp(), LWA_ALPHA);
	else if(opt.isTranspColor())
		SetLayeredWindowAttributes(hWnd, opt.getTranspColor(), 255, LWA_COLORKEY);

	ShowWindow(hWnd, SW_SHOW);
	return(TRUE);
}

/*----------*/
static BOOL create_child_process(const char* cmd, const char* curdir)
{
	trace("create_child_process\n");

	char* buf = NULL;

	if(!cmd || !cmd[0]) {
		buf = new char[32768];
		buf[0] = 0;
		if(!GetEnvironmentVariableA("COMSPEC", buf, 32768))
			strcpy(buf, "cmd.exe");
	}
	else {
		buf = new char[ strlen(cmd)+1 ];
		strcpy(buf, cmd);
	}

	PROCESS_INFORMATION pi;
	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput  = gStdIn;
	si.hStdOutput = gStdOut;
	si.hStdError  = gStdErr;

	if (curdir)
		if (char *p = strstr((char*)curdir, ":\""))
			*(p+1) = '\\';

	if(! CreateProcessA(NULL, buf, NULL, NULL, TRUE,
			    0, NULL, curdir, &si, &pi)) {
		delete [] buf;
		return(FALSE);
	}
	delete [] buf;
	CloseHandle(pi.hThread);
	gChild = pi.hProcess;
	return(TRUE);
}

/*----------*/
static BOOL create_font(const char* name, int height)
{
	trace("create_font\n");

	memset(&gFontLog, 0, sizeof(gFontLog));
	gFontLog.lfHeight = -height;
	gFontLog.lfWidth = 0;
	gFontLog.lfEscapement = 0;
	gFontLog.lfOrientation = 0;
	gFontLog.lfWeight = FW_NORMAL;
	gFontLog.lfItalic = 0;
	gFontLog.lfUnderline = 0;
	gFontLog.lfStrikeOut = 0;
	gFontLog.lfCharSet = DEFAULT_CHARSET;
	gFontLog.lfOutPrecision = OUT_DEFAULT_PRECIS;
	gFontLog.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	gFontLog.lfQuality = DEFAULT_QUALITY;
	gFontLog.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
	if(name) {
		MultiByteToWideChar(CP_ACP,0, name, -1, gFontLog.lfFaceName, LF_FACESIZE);
	}

	gFont = CreateFontIndirect(&gFontLog);

	/* calc font size */
	HDC	hDC = GetDC(NULL);
	HGDIOBJ	oldfont = SelectObject(hDC, gFont);
	TEXTMETRIC met;
	INT	width1[26], width2[26], width = 0;

	GetTextMetrics(hDC, &met);
	GetCharWidth32(hDC, 0x41, 0x5A, width1);
	GetCharWidth32(hDC, 0x61, 0x7A, width2);
	SelectObject(hDC, oldfont);
	ReleaseDC(NULL, hDC);

	for(int i = 0 ; i < 26 ; i++) {
		width += width1[i];
		width += width2[i];
	}
	width /= 26 * 2;
	gFontW = width; /* met.tmAveCharWidth; */
	gFontH = met.tmHeight + gLineSpace;

	return(TRUE);
}

// for Windows SDK v7.0 エラーが発生する場合はコメントアウト。
#ifdef _MSC_VER
#include <winternl.h>
#endif

/*----------*/
static void __hide_alloc_console()
{
	bool bResult = false;

	/*
	 * Open Console Window
	 * hack StartupInfo.wShowWindow flag
	 */

#ifdef _MSC_VER
#ifdef _WIN64
	INT_PTR peb = *(INT_PTR*)((INT_PTR)NtCurrentTeb() + 0x60);
	INT_PTR param = *(INT_PTR*) (peb + 0x20);
	DWORD* pflags = (DWORD*) (param + 0xa4);
	WORD* pshow = (WORD*) (param + 0xa8); 
#else
#ifndef _WINTERNL_
	INT_PTR peb = *(INT_PTR*)((INT_PTR)NtCurrentTeb() + 0x30);
	INT_PTR param = *(INT_PTR*) (peb + 0x10);
#else
	// for Windows SDK v7.0
	PPEB peb = *(PPEB*)((INT_PTR)NtCurrentTeb() + 0x30);
	PRTL_USER_PROCESS_PARAMETERS param = peb->ProcessParameters;
#endif // _WINTERNL_

	DWORD* pflags = (DWORD*)((INT_PTR)param + 0x68);
	WORD* pshow = (WORD*)((INT_PTR)param + 0x6C);
#endif // _WIN64
#else
	// for gcc
	INT_PTR peb = *(INT_PTR*)((INT_PTR)NtCurrentTeb() + 0x30);
    PRTL_USER_PROCESS_PARAMETERS param = *(PRTL_USER_PROCESS_PARAMETERS*)(peb + 0x10);
	DWORD* pflags = (DWORD*)&(param->dwFlags);
	WORD* pshow = (WORD*)&(param->wShowWindow); 
#endif // _MSC_VER

	DWORD	backup_flags = *pflags;
	WORD	backup_show  = *pshow;

	STARTUPINFO si;
	GetStartupInfo(&si);

	/* check */
	if(si.dwFlags == backup_flags && si.wShowWindow == backup_show) {
		// 詳細は不明だがSTARTF_TITLEISLINKNAMEが立っていると、
		// Console窓隠しに失敗するので除去(Win7-64bit)
		if (*pflags & STARTF_TITLEISLINKNAME) {
			*pflags &= ~STARTF_TITLEISLINKNAME;
		}
		*pflags |= STARTF_USESHOWWINDOW;
		*pshow  = SW_HIDE;
		bResult = true;
	}

	AllocConsole();

	/* restore */
	*pflags = backup_flags;
	*pshow  = backup_show;

	while((gConWnd = GetConsoleWindow()) == NULL) {
		Sleep(10);
	}

	if (!bResult){
		while (!IsWindowVisible(gConWnd)) {
			Sleep(10);
		}
		while(IsWindowVisible(gConWnd)) {
			ShowWindow(gConWnd, SW_HIDE);
			Sleep(10);
		}
	}
}

/*----------*/
BOOL WINAPI sig_handler(DWORD n)
{
	return(TRUE);
}

static BOOL create_console(ckOpt& opt)
{
	const char*	conf_title;
	LPWSTR	title;

	conf_title = opt.getTitle();
	if(!conf_title || !conf_title[0]){
		title = L"ckw";
	}else{
		title = new wchar_t[ strlen(conf_title)+1 ];
		ZeroMemory(title, sizeof(wchar_t) * (strlen(conf_title)+1));
		MultiByteToWideChar(CP_ACP, 0, conf_title, (int)strlen(conf_title), title, (int)(sizeof(wchar_t) * (strlen(conf_title)+1)) );
	}

	__hide_alloc_console();

	SetConsoleTitle(title);

	SetConsoleCtrlHandler(sig_handler, TRUE);

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	gStdIn  = CreateFile(L"CONIN$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, NULL);
	gStdOut = CreateFile(L"CONOUT$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, NULL);
	gStdErr = CreateFile(L"CONOUT$",  GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE,
			     &sa, OPEN_EXISTING, 0, NULL);

	if(!gConWnd || !gStdIn || !gStdOut || !gStdErr)
		return(FALSE);

	HINSTANCE hLib;
	hLib = LoadLibraryW( L"KERNEL32.DLL" );
	if (hLib == NULL)
		goto done;

	#define GetProc( proc ) \
	do { \
		proc = (proc##T)GetProcAddress( hLib, #proc ); \
		if (proc == NULL) \
			goto freelib; \
	} while (0)
	GetProc( GetConsoleFontInfo );
	GetProc( GetNumberOfConsoleFonts );
	GetProc( SetConsoleFont );
	#undef GetProc

	{
		CONSOLE_FONT font[MAX_FONTS];
		DWORD fonts;
		fonts = GetNumberOfConsoleFonts();
		if (fonts > MAX_FONTS)
			fonts = MAX_FONTS;
	
		GetConsoleFontInfo(gStdOut, 0, fonts, font);
		CONSOLE_FONT minimalFont = { 0, {0, 0}};
		for(DWORD i=0;i<fonts;i++){
			if(minimalFont.dim.X < font[i].dim.X && minimalFont.dim.Y < font[i].dim.Y)
				minimalFont = font[i];
		}
		SetConsoleFont(gStdOut, minimalFont.index);
	}
	freelib:
		FreeLibrary( hLib );
	done:

	/* set buffer & window size */
	COORD size;
	SMALL_RECT sr = {0,0,0,0};
	SetConsoleWindowInfo(gStdOut, TRUE, &sr);
	size.X = opt.getWinCharW();
	size.Y = opt.getWinCharH() + opt.getSaveLines();
	SetConsoleScreenBufferSize(gStdOut, size);
	sr.Left = 0;
	sr.Right = opt.getWinCharW()-1;
	sr.Top = size.Y - opt.getWinCharH();
	sr.Bottom = size.Y-1;
	SetConsoleWindowInfo(gStdOut, TRUE, &sr);
	size.X = sr.Left;
	size.Y = sr.Top;
	SetConsoleCursorPosition(gStdOut, size);
	return(TRUE);
}

/*----------*/
BOOL init_options(ckOpt& opt)
{
	/* create argv */
	int	i, argc;
	LPWSTR*	wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	char**	argv = new char*[argc+1];
	argv[argc] = 0;
	for(i = 0 ; i < argc ; i++) {
		DWORD wlen = (DWORD) wcslen(wargv[i]);
		DWORD alen = wlen * 2 + 16;
		argv[i] = new char[alen];
		alen = WideCharToMultiByte(CP_ACP, 0, wargv[i],wlen, argv[i],alen,NULL,NULL);
		argv[i][alen] = 0;
	}

	opt.loadXdefaults();
	bool result = opt.set(argc, argv);

	for(i = 0 ; i < argc ; i++)
		delete [] argv[i];
	delete [] argv;

	if(!result) return(FALSE);

	/* set */
	for(i = kColor0 ; i <= kColor15 ; i++)
		gColorTable[i] = opt.getColor(i);
	gColorTable[kColor7] = opt.getColorFg();
	gColorTable[kColor0] = opt.getColorBg();

	gColorTable[kColorCursorBg] = opt.getColorCursor();
	gColorTable[kColorCursorFg] = ~gColorTable[kColorCursorBg] & 0xFFFFFF;
	gColorTable[kColorCursorImeBg] = opt.getColorCursorIme();
	gColorTable[kColorCursorImeFg] = ~gColorTable[kColorCursorImeBg] & 0xFFFFFF;

	gBorderSize = opt.getBorderSize();
	gLineSpace = opt.getLineSpace();

	if(opt.getBgBmp()) {
		gBgBmp = (HBITMAP)LoadImageA(NULL, opt.getBgBmp(),
				IMAGE_BITMAP, 0,0, LR_LOADFROMFILE);
	}
	if(gBgBmp)    gBgBrush = CreatePatternBrush(gBgBmp);
	if(!gBgBrush) gBgBrush = CreateSolidBrush(gColorTable[0]);

	return(TRUE);
}

/*----------*/
static BOOL initialize()
{
	ckOpt opt;

	if(! ime_wrap_init()) {
		trace("ime_wrap_init failed\n");
	}

	if(! init_options(opt)) {
		return(FALSE);
	}
	if(! create_console(opt)) {
		trace("create_console failed\n");
		return(FALSE);
	}
	if(! create_font(opt.getFont(), opt.getFontSize())) {
		trace("create_font failed\n");
		return(FALSE);
	}
	if(! create_child_process(opt.getCmd(), opt.getCurDir())) {
		trace("create_child_process failed\n");
		return(FALSE);
	}
	if(! create_window(opt)) {
		trace("create_window failed\n");
		return(FALSE);
	}

	/*
	wchar_t path[MAX_PATH+1];
	GetSystemDirectory(path, MAX_PATH);
	SetCurrentDirectory(path);
	*/
	return(TRUE);
}

#define SAFE_CloseHandle(handle) \
	if(handle) { CloseHandle(handle); handle = NULL; }

#define SAFE_DeleteObject(handle) \
	if(handle) { DeleteObject(handle); handle = NULL; }

/*----------*/
static void _terminate()
{
	if(gTitle) {
		delete [] gTitle;
		gTitle = NULL;
	}
	if(gScreen) {
		delete [] gScreen;
		gScreen = NULL;
	}
	if(gCSI) {
		delete gCSI;
		gCSI = NULL;
	}
	gConWnd = NULL;
	SAFE_CloseHandle(gStdIn);
	SAFE_CloseHandle(gStdOut);
	SAFE_CloseHandle(gStdErr);
	SAFE_CloseHandle(gChild);
	SAFE_DeleteObject(gFont);
	SAFE_DeleteObject(gBgBrush);
	SAFE_DeleteObject(gBgBmp);
	ime_wrap_term();
}

#ifdef _DEBUG
#include <crtdbg.h>
#endif

/*----------*/
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	char *a = new char[1];
	*a = 0x22;
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF |
		       _CRTDBG_LEAK_CHECK_DF |
		       /*_CRTDBG_CHECK_ALWAYS_DF |*/
		       _CRTDBG_DELAY_FREE_MEM_DF);
	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
	_CrtSetReportMode( _CRT_WARN,   _CRTDBG_MODE_FILE );
	_CrtSetReportMode( _CRT_ERROR,  _CRTDBG_MODE_FILE );
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
	_CrtSetReportFile( _CRT_WARN,   _CRTDBG_FILE_STDERR );
	_CrtSetReportFile( _CRT_ERROR,  _CRTDBG_FILE_STDERR );
#endif

	if(initialize()) {
		MSG msg;
		while(GetMessage(&msg, NULL, 0,0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	_terminate();
	return(0);
}

/* 新規ウインドウの作成 */
void makeNewWindow()
{
	LPWSTR cd = new TCHAR[MAX_PATH+1];
	GetCurrentDirectory(MAX_PATH, cd);

	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	if(CreateProcess(NULL, GetCommandLine(), NULL, NULL, FALSE, 0,
					   NULL, NULL, &si, &pi)){
		// 使用しないので，すぐにクローズしてよい
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

/* EOF */
