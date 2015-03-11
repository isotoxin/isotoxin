@echo off
set getlib=SampleRateConverter
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\libresample\src\samplerate.h goto alrd

set RARCHIVE=libsamplerate-0.1.8.tar.gz
set RARCHIVE2=libsamplerate-0.1.8.tar
set RURL=http://www.mega-nerd.com/SRC/%RARCHIVE%
set RFOLDER=libsamplerate-0.1.8

if exist %RFOLDER% rd %RFOLDER% /S /Q
..\wget %RURL%
..\7z x %RARCHIVE%
..\7z x %RARCHIVE2%
move %RFOLDER%\src %libdir%\libresample >nul
move %RFOLDER%\win32\config.h %libdir%\libresample >nul
rd %RFOLDER% /S /Q
del %RARCHIVE%
del %RARCHIVE2%


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
