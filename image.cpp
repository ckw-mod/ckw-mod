/*-----------------------------------------------------------------------------
 * File: image.cpp
 *-----------------------------------------------------------------------------
 * Copyright (c) 2013       mecab <mecab@misosi.ru>
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
 *
 *-----------------------------------------------------------------------------
 * Portions are originally under MIT License
 *-----------------------------------------------------------------------------
 * Copyright 2007-2008 Logos Bible Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *---------------------------------------------------------------------------*/

#include "image.h"
#include <wincodec.h>
#include <wincodecsdk.h>

HBITMAP createHBITMAPFromFile(LPCSTR path) {
	HBITMAP bmp;

	if (isOSGreaterThan2k()) {
		// Load image using WIC
		IWICBitmapSource *bitmapSource = createBitmapSourceFromFile(path);
		if (bitmapSource) {
			bmp = createHBITMAPFromBitmapSource(bitmapSource);
			bitmapSource->Release();
		}
	}
	else {
		// Load image using Win32 API. Only BMP is supported
		bmp = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	}

	return bmp;
}

BOOL isOSGreaterThan2k() {
	OSVERSIONINFO osVer;
	osVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx( &osVer );

	return (osVer.dwMajorVersion > 5) || (osVer.dwMajorVersion == 5 && osVer.dwMajorVersion > 0);
}

IWICBitmapSource* createBitmapSourceFromFile(LPCSTR path)
{
	HRESULT hr;
	size_t wbufSize = MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, path, -1, NULL, 0);
	LPWSTR wpath = new WCHAR[wbufSize];
	MultiByteToWideChar(CP_OEMCP, MB_PRECOMPOSED, path, -1, wpath, wbufSize);

	IWICImagingFactory* wicFactory = NULL;
	IWICBitmapDecoder* decoder = NULL;
	IWICBitmapFrameDecode* frame = NULL;
	IWICBitmapSource* ipBitmap = NULL;

	hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<LPVOID *>(&wicFactory));
	if(SUCCEEDED(hr)) {
		hr = wicFactory->CreateDecoderFromFilename(wpath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	}
	if(SUCCEEDED(hr)) {
		hr = decoder->GetFrame(0, &frame);
	}
	if(SUCCEEDED(hr)) {
		// convert the image to 32bpp BGRA format with pre-multiplied alpha
		// (it may not be stored in that format natively in the PNG resource,
		// but we need this format to create the DIB to use on-screen)
		hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, frame, &ipBitmap);
	}

	delete[] wpath;
	if (frame) {
		frame->Release();
	}
	if (decoder) {
		decoder->Release();
	}
	if (wicFactory) {
		wicFactory->Release();
	}

	return ipBitmap;
 }

HBITMAP createHBITMAPFromBitmapSource(IWICBitmapSource * ipBitmap)
{
    // initialize return value
    HBITMAP hbmp = NULL;

    // get image attributes and check for valid image
    UINT width = 0;
    UINT height = 0;
    if (FAILED(ipBitmap->GetSize(&width, &height)) || width == 0 || height == 0)
        return NULL;

    // prepare structure giving bitmap information (negative height indicates a top-down DIB)
    BITMAPINFO bminfo;
    ZeroMemory(&bminfo, sizeof(bminfo));
    bminfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bminfo.bmiHeader.biWidth = width;
    bminfo.bmiHeader.biHeight = -((LONG) height);
    bminfo.bmiHeader.biPlanes = 1;
    bminfo.bmiHeader.biBitCount = 32;
    bminfo.bmiHeader.biCompression = BI_RGB;

    // create a DIB section that can hold the image
    void * pvImageBits = NULL;
    HDC hdcScreen = GetDC(NULL);
    hbmp = CreateDIBSection(hdcScreen, &bminfo, DIB_RGB_COLORS, &pvImageBits, NULL, 0);
    ReleaseDC(NULL, hdcScreen);
    if (hbmp == NULL)
        return NULL;

    // extract the image into the HBITMAP
    const UINT cbStride = width * 4;
    const UINT cbImage = cbStride * height;
    if (FAILED(ipBitmap->CopyPixels(NULL, cbStride, cbImage, static_cast<BYTE *>(pvImageBits))))
    {
        // couldn't extract image; delete HBITMAP
        DeleteObject(hbmp);
        hbmp = NULL;
    }

	return hbmp;
}
