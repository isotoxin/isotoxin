@echo off

set RARCHIVE=libsamplerate-0.1.8.tar.gz
set RARCHIVE2=libsamplerate-0.1.8.tar
set RURL=http://www.mega-nerd.com/SRC/%RARCHIVE%
set RFOLDER=libsamplerate-0.1.8

echo LibResample ....................................
rd %RFOLDER% /S /Q
..\wget %RURL%
..\7z x %RARCHIVE%
..\7z x %RARCHIVE2%
move %RFOLDER%\src %1\libresample
move %RFOLDER%\win32\config.h %1\libresample
rd %RFOLDER% /S /Q
del %RARCHIVE%
del %RARCHIVE2%
