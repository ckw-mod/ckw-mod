@echo off
set VERSION_MAJOR=0
set VERSION_MINOR=9
set VERSION_BGFIX=1
set VERSION_NUM=00901
set DATE=%date% %time:~0,8% UTC+9

echo #define VERSION_MAJOR %VERSION_MAJOR%
echo #define VERSION_MINOR %VERSION_MINOR%
echo #define VERSION_BUGFIX %VERSION_BGFIX%
echo #define VERSION_STRING "%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX%"
echo #define VERSION_NUMSTRING "%VERSION_NUM%"
echo #define BUILDDATE_STRING "%DATE%"
echo #define COPYRIGHT_STRING "Copyright (C) 2005-2010  Kazuo Ishii and ckw-mod creators and more..."
echo #define ABOUT_DLG_STRING "ckw-mod version %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX% , build %DATE%\r\nCopyright (C) 2005-2010  Kazuo Ishii and ckw-moders"
