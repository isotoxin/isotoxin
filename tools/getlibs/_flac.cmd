@echo off
set getlib=libflac
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\flac\include\flac\all.h goto alrd

set NASMARCHIVE=nasm-2.11.06-win32.zip
set NASMURL=http://www.nasm.us/pub/nasm/releasebuilds/2.11.06/win32/%NASMARCHIVE%

if exist flac rd flac /S /Q
git clone https://git.xiph.org/flac.git
md %libdir%\flac\include
md %libdir%\flac\src
move flac\include\flac %libdir%\flac\include
move flac\include\share %libdir%\flac\include
move flac\src\libFLAC %libdir%\flac\src
move flac\src\share %libdir%\flac\src
rd flac /S /Q

echo download nasm for flac compile
..\wget %NASMURL%
..\7z e %NASMARCHIVE% nasm.exe
del %NASMARCHIVE%
move nasm.exe %libdir%\flac >nul


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
