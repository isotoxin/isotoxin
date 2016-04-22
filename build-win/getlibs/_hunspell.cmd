@echo off
set getlib=hunspell
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\hunspell\src\hunspell.h goto alrd

if exist hunspell rd  /S /Q
git clone https://github.com/hunspell/hunspell

md %libdir%\hunspell\src

move hunspell\src\hunspell\*.?xx %libdir%\hunspell\src >nul
move hunspell\src\hunspell\*.h %libdir%\hunspell\src >nul

rd hunspell /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
