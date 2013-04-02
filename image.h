#ifndef __CWK_IMAGE_H__
#define __CWK_IMAGE_H__ 1

#include "ckw.h"
#include <wincodec.h>
HBITMAP createHBITMAPFromFile(LPCSTR path);
BOOL isOSGreaterThan2k();
IWICBitmapSource* createBitmapSourceFromFile(LPCSTR lpname);
HBITMAP createHBITMAPFromBitmapSource(IWICBitmapSource* ipBitmap);

#endif /* __CKW_IMAGE_H__ */
