@echo off

set NASMARCHIVE=nasm-2.11.06-win32.zip
set NASMURL=http://www.nasm.us/pub/nasm/releasebuilds/2.11.06/win32/%NASMARCHIVE%

echo LibFlac ....................................
rd flac /S /Q
git clone https://git.xiph.org/flac.git
md %1\flac\include
md %1\flac\src
move flac\include\flac %1\flac\include
move flac\include\share %1\flac\include
move flac\src\libFLAC %1\flac\src
move flac\src\share %1\flac\src
rd flac /S /Q

echo Download nasm for flac compile
..\wget %NASMURL%
..\7z e %NASMARCHIVE% nasm.exe
del %NASMARCHIVE%
move nasm.exe %1\flac
