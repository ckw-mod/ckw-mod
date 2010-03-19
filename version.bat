@echo off
set VERSION_MAJOR=0
set VERSION_MINOR=8
set VERSION_BGFIX=10
set VERSION_NUM=008101
set DATE=%date% %time:~0,8% UTC+9

echo #define VERSION_MAJOR %VERSION_MAJOR%
echo #define VERSION_MINOR %VERSION_MINOR%
echo #define VERSION_BUGFIX %VERSION_BGFIX%
echo #define VERSION_STRING "%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX%-mod"
echo #define VERSION_NUMSTRING "%VERSION_NUM%"
echo #define BUILDDATE_STRING "%DATE%"
echo #define COPYRIGHT_STRING "Copyright (C) 2005  Kazuo Ishii <k-ishii@wb4.so-net.ne.jp>"
echo #define ABOUT_DLG_STRING "ckw version %VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_BGFIX%-mod , build %DATE%\r\nCopyright (C) 2005  Kazuo Ishii <k-ishii@wb4.so-net.ne.jp>"
