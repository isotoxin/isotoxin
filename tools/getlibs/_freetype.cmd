@echo off
set getlib=freetype
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\freetype\include\freetype.h goto alrd

if exist freetype2 rd freetype2 /S /Q
git clone http://git.sv.nongnu.org/r/freetype/freetype2.git
move freetype2\include %libdir%\freetype >nul
move freetype2\src %1\freetype >nul
rd freetype2 /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
