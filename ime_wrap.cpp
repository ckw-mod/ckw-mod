/*-----------------------------------------------------------------------------
 * File: ime_wrap.cpp
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
#include <windows.h>
#include <imm.h>

static HMODULE gDll = NULL;

typedef HIMC (WINAPI* FN_GetContext)(HWND);
typedef BOOL (WINAPI* FN_ReleaseContext)(HWND,HIMC);
typedef BOOL (WINAPI* FN_GetOpenStatus)(HIMC);
typedef BOOL (WINAPI* FN_SetCompositionFontW)(HIMC, LPLOGFONTW);
typedef BOOL (WINAPI* FN_SetCompositionWindow)(HIMC, PCOMPOSITIONFORM);

static FN_GetContext		fn_GetContext	 = NULL;
static FN_ReleaseContext	fn_ReleaseContext	= NULL;
static FN_GetOpenStatus		fn_GetOpenStatus	= NULL;
static FN_SetCompositionFontW	fn_SetCompositionFontW	= NULL;
static FN_SetCompositionWindow	fn_SetCompositionWindow	= NULL;

HIMC WINAPI ImmGetContext(HWND hwnd)
{
	if(fn_GetContext)
		return( fn_GetContext(hwnd) );
	return(NULL);
}

BOOL WINAPI ImmReleaseContext(HWND hwnd, HIMC imc)
{
	if(fn_ReleaseContext)
		return( fn_ReleaseContext(hwnd, imc) );
	return(FALSE);
}

BOOL WINAPI ImmGetOpenStatus(HIMC imc)
{
	if(fn_GetOpenStatus)
		return( fn_GetOpenStatus(imc) );
	return(FALSE);
}

BOOL WINAPI ImmSetCompositionFontW(HIMC imc, LPLOGFONTW lf)
{
	if(fn_SetCompositionFontW)
		return( fn_SetCompositionFontW(imc, lf) );
	return(FALSE);
}

BOOL WINAPI ImmSetCompositionWindow(HIMC imc, PCOMPOSITIONFORM cf)
{
	if(fn_SetCompositionWindow)
		return( fn_SetCompositionWindow(imc, cf) );
	return(FALSE);
}

#define GET_PROC(name) \
	if(result) { \
		fn_##name = (FN_##name) GetProcAddress(gDll, "Imm" #name); \
		if(! fn_##name) result = FALSE; \
	}

BOOL	ime_wrap_init()
{
	BOOL result = TRUE;
	gDll = LoadLibraryA("imm32.dll");
	if(!gDll)
		return(FALSE);
	GET_PROC( GetContext );
	GET_PROC( ReleaseContext );
	GET_PROC( GetOpenStatus );
	GET_PROC( SetCompositionFontW );
	GET_PROC( SetCompositionWindow );
	if(!result) {
		fn_GetContext = NULL;
		fn_ReleaseContext = NULL;
		fn_GetOpenStatus = NULL;
		fn_SetCompositionFontW = NULL;
		fn_SetCompositionWindow = NULL;
		FreeLibrary(gDll);
		gDll = NULL;
	}
	return(result);
}

void ime_wrap_term()
{
	if (gDll) {
		FreeLibrary(gDll);
		gDll = NULL;
	}
}

/* EOF */
