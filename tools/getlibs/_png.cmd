@echo off
set getlib=pnglib
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\pnglib\src\png.h goto alrd

set PNGDIR=lpng1616
set PNGARCHIVE=lpng1616.7z
set PNGURL=http://downloads.sourceforge.net/project/libpng/libpng16/1.6.16/%PNGARCHIVE%

if exist %PNGDIR% rd %PNGDIR% /S /Q
..\wget %PNGURL%
..\7z x %PNGARCHIVE%
if not exist %libdir%\pnglib\src md %libdir%\pnglib\src
move %PNGDIR%\*.c %libdir%\pnglib\src >nul
move %PNGDIR%\*.h %libdir%\pnglib\src >nul
rd %PNGDIR% /S /Q
del %PNGARCHIVE%

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
