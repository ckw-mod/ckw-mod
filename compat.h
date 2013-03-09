#ifndef __COMPAT_H__
#define __COMPAT_H__ 1

#include <stdio.h>

#define _makepath_s _makepath
#define _splitpath_s _splitpath
#define sprintf_s snprintf
#define sscanf_s sscanf
#define strcat_s strcat
#define strcpy_s strcpy
#define wcscat_s wcscat

inline int
fopen_s(FILE** pFile, const char *filename, const char *mode)
{
  *pFile = fopen(filename, mode);
  return 0;
}

#endif
