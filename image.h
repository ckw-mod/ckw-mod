#ifndef __CWK_IMAGE_H__
#define __CWK_IMAGE_H__ 1

#include "ckw.h"
#include <wincodec.h>
IWICBitmapSource* CreateBitmapSourceFromFile(LPCSTR lpname);
HBITMAP CreateHBITMAPFromBitmapSource(IWICBitmapSource* ipBitmap);

#endif /* __CKW_IMAGE_H__ */
