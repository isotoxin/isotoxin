@echo off
set getlib=cairo
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\cairo\src\cairo.h goto alrd

if exist cairo rd cairo /S /Q
if exist pixman rd pixman /S /Q
git clone git://anongit.freedesktop.org/git/cairo
git clone git://anongit.freedesktop.org/git/pixman.git pixman

md %libdir%\cairo\src
md %libdir%\cairo\pixman

move cairo\src\cairo-*.c %libdir%\cairo\src >nul
move cairo\src\cairo-*.h %libdir%\cairo\src >nul
move cairo\src\cairo.c %libdir%\cairo\src >nul
move cairo\src\cairo.h %libdir%\cairo\src >nul
move cairo\src\win32\cairo*.c %libdir%\cairo\src >nul
move cairo\src\win32\cairo*.h %libdir%\cairo\src >nul

move pixman\pixman\pixman*.c %libdir%\cairo\pixman >nul
move pixman\pixman\pixman*.h %libdir%\cairo\pixman >nul

rd cairo /S /Q
rd pixman /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
