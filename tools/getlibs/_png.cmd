@echo off

set PNGDIR=lpng1616
set PNGARCHIVE=lpng1616.7z
set PNGURL=http://downloads.sourceforge.net/project/libpng/libpng16/1.6.16/%PNGARCHIVE%

set op=%cd%

echo LibPng ....................................
rd %PNGDIR% /S /Q
..\wget %PNGURL%
..\7z x %PNGARCHIVE%
md %1\pnglib\src
move %PNGDIR%\*.c %1\pnglib\src
move %PNGDIR%\*.h %1\pnglib\src
rd %PNGDIR% /S /Q
del %PNGARCHIVE%
