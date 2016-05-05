@echo off
set getlib=libflac
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\libflac\include\flac\all.h goto alrd

set NASMARCHIVE=nasm-2.11.06-win32.zip
set NASMURL=http://www.nasm.us/pub/nasm/releasebuilds/2.11.06/win32/%NASMARCHIVE%

if exist flac rd flac /S /Q
git clone https://github.com/xiph/flac
md %libdir%\libflac\include
md %libdir%\libflac\src
move flac\include\flac %libdir%\libflac\include
move flac\include\share %libdir%\libflac\include
move flac\src\libFLAC %libdir%\libflac\src
move flac\src\share %libdir%\libflac\src
rd flac /S /Q

echo download nasm for flac compile
..\wget %NASMURL%
..\7z e %NASMARCHIVE% nasm.exe
del %NASMARCHIVE%
move nasm.exe %libdir%\libflac >nul


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
