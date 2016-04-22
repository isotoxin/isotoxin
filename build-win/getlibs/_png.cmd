@echo off
set getlib=pnglib
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\pnglib\src\png.h goto alrd

if exist pnglib rd pnglib /S /Q
git clone git://git.code.sf.net/p/libpng/code pnglib

cd pnglib
git checkout "libpng-1.6.21-signed"
cd ..

if not exist %libdir%\pnglib\src md %libdir%\pnglib\src
move pnglib\*.c %libdir%\pnglib\src >nul
move pnglib\*.h %libdir%\pnglib\src >nul
rd pnglib /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
