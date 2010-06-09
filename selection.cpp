/*-----------------------------------------------------------------------------
 * File: selection.cpp
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

static int	gSelectMode = 0;
static COORD	gSelectPos = { -1, -1 }; // pick point
static SMALL_RECT gSelectRect = { -1, -1, -1, -1 }; // expanded selection area

static const wchar_t WORD_BREAK_CHARS[] = {
	L' ',  L'\t', L'\"', L'&', L'\'', L'(', L')', L'*',
	L',',  L';',  L'<',  L'=', L'>',  L'?', L'@', L'[',
	L'\\', L']',  L'^',  L'`', L'{',  L'}', L'~',
	0x3000,
	0x3001,
	0x3002,
	/**/
	0,
};

/*****************************************************************************/

#define SCRN_InvalidArea(x,y) \
	(y < gCSI->srWindow.Top    ||	\
	 y > gCSI->srWindow.Bottom ||	\
	 x < gCSI->srWindow.Left   ||	\
	 x > gCSI->srWindow.Right)

#define SELECT_GetScrn(x,y) \
	(gScreen + CSI_WndCols(gCSI) * (y - gCSI->srWindow.Top) + x)

static void __select_word_expand_left()
{
	if(SCRN_InvalidArea(gSelectRect.Left, gSelectRect.Top))
		return;
	CHAR_INFO* base  = SELECT_GetScrn(gSelectRect.Left, gSelectRect.Top);
	CHAR_INFO* ptr = base;
	int c = gSelectRect.Left;

	for( ; c >= gCSI->srWindow.Left ; c--, ptr--) {
		if(wcschr(WORD_BREAK_CHARS, ptr->Char.UnicodeChar)) {
			c++;
			break;
		}
	}
	if(c < 0)
		c = 0;

	if(gSelectRect.Left > c)
		gSelectRect.Left = c;
}

static void __select_word_expand_right()
{
	if(SCRN_InvalidArea(gSelectRect.Right, gSelectRect.Bottom))
		return;
	CHAR_INFO* base  = SELECT_GetScrn(gSelectRect.Right, gSelectRect.Bottom);
	CHAR_INFO* ptr = base;
	int c = gSelectRect.Right;

	for( ; c <= gCSI->srWindow.Right ; c++, ptr++) {
		if(wcschr(WORD_BREAK_CHARS, ptr->Char.UnicodeChar)) {
			break;
		}
	}

	if(gSelectRect.Right < c)
		gSelectRect.Right = c;
}

static void __select_char_expand()
{
	CHAR_INFO* base;

	if(SCRN_InvalidArea(gSelectRect.Left, gSelectRect.Top)) {
	}
	else if(gSelectRect.Left-1 >= gCSI->srWindow.Left) {
		base  = SELECT_GetScrn(gSelectRect.Left, gSelectRect.Top);
		if(base->Attributes & COMMON_LVB_TRAILING_BYTE)
			gSelectRect.Left--;
	}

	if(SCRN_InvalidArea(gSelectRect.Right, gSelectRect.Bottom)) {
	}
	else {
		base  = SELECT_GetScrn(gSelectRect.Right, gSelectRect.Bottom);
		if(base->Attributes & COMMON_LVB_TRAILING_BYTE)
			gSelectRect.Right++;
	}
}

inline void __select_expand()
{
	if(gSelectMode == 0) {
		__select_char_expand();
	}
	else if(gSelectMode == 1) {
		__select_word_expand_left();
		__select_word_expand_right();
	}
	else if(gSelectMode == 2) {
		gSelectRect.Left = gCSI->srWindow.Left;
		gSelectRect.Right = gCSI->srWindow.Right+1;
	}
}

static void window_to_charpos(int& x, int& y)
{
	x -= gBorderSize;
	y -= gBorderSize;
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	x /= gFontW;
	y /= gFontH;
	x += gCSI->srWindow.Left;
	y += gCSI->srWindow.Top;
	if(x > gCSI->srWindow.Right)  x = gCSI->srWindow.Right+1;
	if(y > gCSI->srWindow.Bottom) y = gCSI->srWindow.Bottom;
}

/*****************************************************************************/

static inline bool __select_invalid()
{
	return ( gSelectRect.Top > gSelectRect.Bottom ||
	         (gSelectRect.Top == gSelectRect.Bottom &&
	         gSelectRect.Left >= gSelectRect.Right) );
}

/*----------*/
BOOL	selectionGetArea(SMALL_RECT& sr)
{
	if( __select_invalid() )
		return(FALSE);
	sr = gSelectRect;
	return(TRUE);
}

/*----------*/
void	selectionClear(HWND hWnd)
{
	if( __select_invalid() )
		return;
	gSelectRect.Left = gSelectRect.Right = \
	gSelectRect.Top = gSelectRect.Bottom = 0;
	InvalidateRect(hWnd, NULL, FALSE);
}

/*----------*/
wchar_t * selectionGetString()
{
	if( __select_invalid() )
		return(NULL);

	int nb, y;

	if(gSelectRect.Top == gSelectRect.Bottom) {
		nb = gSelectRect.Right - gSelectRect.Left;
	}
	else {
		nb = gCSI->srWindow.Right - gSelectRect.Left+1;
		for(y = gSelectRect.Top+1 ; y <= gSelectRect.Bottom-1 ; y++)
			nb += CSI_WndCols(gCSI);
		nb += gSelectRect.Right - gCSI->srWindow.Left;
	}

	COORD      size = { CSI_WndCols(gCSI), 1 };
	CHAR_INFO* work = new CHAR_INFO[ size.X ];
	wchar_t*   buffer = new wchar_t[ nb +32 ];
	wchar_t*   wp = buffer;
	COORD      pos = { 0,0 };
	SMALL_RECT sr = { gCSI->srWindow.Left, 0, gCSI->srWindow.Right, 0 };

	*wp = 0;

	if(gSelectRect.Top == gSelectRect.Bottom) {
		sr.Top = sr.Bottom = gSelectRect.Top;
		ReadConsoleOutput_Unicode(gStdOut, work, size, pos, &sr);
		copyChar(wp, work, gSelectRect.Left, gSelectRect.Right-1, false);
	}
	else {
		sr.Top = sr.Bottom = gSelectRect.Top;
		ReadConsoleOutput_Unicode(gStdOut, work, size, pos, &sr);
		copyChar(wp, work, gSelectRect.Left, gCSI->srWindow.Right);
		for(y = gSelectRect.Top+1 ; y <= gSelectRect.Bottom-1 ; y++) {
			sr.Top = sr.Bottom = y;
			ReadConsoleOutput_Unicode(gStdOut, work, size, pos, &sr);
			copyChar(wp, work, gCSI->srWindow.Left, gCSI->srWindow.Right);
		}
		sr.Top = sr.Bottom = gSelectRect.Bottom;
		ReadConsoleOutput_Unicode(gStdOut, work, size, pos, &sr);
		copyChar(wp, work, gCSI->srWindow.Left, gSelectRect.Right-1, false);
	}

	delete [] work;
	return(buffer);
}

/*----------*/
void	onLBtnDown(HWND hWnd, int x, int y)
{
	static DWORD	prev_time = 0;
	static int	prevX = -100;
	static int	prevY = -100;

	{
		/* calc click count */
		DWORD now_time = GetTickCount();
		DWORD stime;
		if(prev_time > now_time)
			stime = now_time + ~prev_time+1;
		else
			stime = now_time - prev_time;
		if(stime <= GetDoubleClickTime()) {
			int sx = (prevX > x) ? prevX-x : x-prevX;
			int sy = (prevY > y) ? prevY-y : y-prevY;
			if(sx <= GetSystemMetrics(SM_CXDOUBLECLK) &&
			   sy <= GetSystemMetrics(SM_CYDOUBLECLK)) {
				if(++gSelectMode > 2)
					gSelectMode = 0;
			}
			else {
				gSelectMode = 0;
			}
		}
		else {
			gSelectMode = 0;
		}
		prev_time = now_time;
		prevX = x;
		prevY = y;
	}

	if(!gScreen || !gCSI)
		return;
	window_to_charpos(x, y);
	SetCapture(hWnd);

	gSelectPos.X = x;
	gSelectPos.Y = y;
	gSelectRect.Left = gSelectRect.Right = x;
	gSelectRect.Top  = gSelectRect.Bottom = y;

	__select_expand();
	InvalidateRect(hWnd, NULL, FALSE);
}

/*----------*/
void	onLBtnUp(HWND hWnd, int x, int y)
{
	if(hWnd != GetCapture())
		return;
	ReleaseCapture();
	if(!gScreen || !gCSI)
		return;
	//window_to_charpos(x, y);

	wchar_t* str = selectionGetString();
	if(!str) return;

	copyStringToClipboard( hWnd, str );

	delete [] str;
}

/*----------*/
void	onMouseMove(HWND hWnd, int x, int y)
{
	if(hWnd != GetCapture())
		return;
	if(!gScreen || !gCSI)
		return;
	window_to_charpos(x, y);

	SMALL_RECT bak = gSelectRect;

	if(y < gSelectPos.Y || (y == gSelectPos.Y && x < gSelectPos.X)) {
		gSelectRect.Left   = x;
		gSelectRect.Top    = y;
		gSelectRect.Right  = gSelectPos.X;
		gSelectRect.Bottom = gSelectPos.Y;
	}
	else {
		gSelectRect.Left   = gSelectPos.X;
		gSelectRect.Top    = gSelectPos.Y;
		gSelectRect.Right  = x;
		gSelectRect.Bottom = y;
	}
	__select_expand();

	if(memcmp(&bak, &gSelectRect, sizeof(bak))) {
		InvalidateRect(hWnd, NULL, FALSE);
	}
}

/* EOF */
