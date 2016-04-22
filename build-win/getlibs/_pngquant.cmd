@echo off
set getlib=pngquant
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\pngquant\lib\libimagequant.h goto alrd

if exist pngquant rd pngquant /S /Q
git clone https://github.com/pornel/pngquant
cd pngquant
git checkout msvc
cd ..

md %libdir%\pngquant\lib
move pngquant\lib\*.c %libdir%\pngquant\lib >nul
move pngquant\lib\*.h %libdir%\pngquant\lib >nul

rd pngquant /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
