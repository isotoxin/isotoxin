@echo off
set getlib=zlib
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\zlib\src\zlib.h goto alrd

if exist zlib rd zlib /S /Q
git clone https://github.com/madler/zlib

md %libdir%\zlib\src
move zlib\*.c %libdir%\zlib\src >nul
move zlib\*.h %libdir%\zlib\src >nul

md %libdir%\zlib\src\contrib
move zlib\contrib\minizip %libdir%\zlib\src\contrib >nul
move zlib\contrib\masmx64 %libdir%\zlib\src\contrib >nul
move zlib\contrib\masmx86 %libdir%\zlib\src\contrib >nul

rd zlib /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
