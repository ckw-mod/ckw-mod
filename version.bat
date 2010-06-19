@echo off
set VERSION_MAJOR=0
set VERSION_MINOR=9
set VERSION_BGFIX=0
set VERSION_NUM=009001
set VERSION_MOD=d1
set DATE=%date% %time:~0,8% UTC+9

echo #define VERSION_MAJOR %VERSION_MAJOR%
echo #define VERSION_MINOR %VERSION_MINOR%
echo #define VERSION_BUGFIX %VERSION_BGFIX%
echo #define VERSION_STRING "%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX%-mod-%VERSION_MOD%"
echo #define VERSION_NUMSTRING "%VERSION_NUM%"
echo #define BUILDDATE_STRING "%DATE%"
echo #define COPYRIGHT_STRING "Copyright (C) 2005-2010  Kazuo Ishii and hideden and shuujin and s-yata and Shimizukawa and Deflis and nocd5 and craftware"
echo #define ABOUT_DLG_STRING "ckw version %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX%-mod+git-%VERSION_MOD% , build %DATE%\r\nCopyright (C) 2005-2010  Kazuo Ishii and ckw-moders"
