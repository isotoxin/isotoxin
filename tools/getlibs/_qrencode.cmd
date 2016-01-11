@echo off
set getlib=libqrencode
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\libqrencode\src\qrencode.h goto alrd

if exist libqrencode rd libqrencode /S /Q
git clone https://github.com/fukuchi/libqrencode

md %libdir%\libqrencode\src
move libqrencode\*.c %libdir%\libqrencode\src >nul
move libqrencode\*.h %libdir%\libqrencode\src >nul

rd libqrencode /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
