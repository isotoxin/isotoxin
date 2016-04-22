@echo off
set getlib=jpeglib
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\jpeg\src\jconfig.h goto alrd

set op=%cd%

set JPGARCHIVE=jpegsr6.zip
set JPGURL=http://downloads.sourceforge.net/project/libjpeg/libjpeg/6b/%JPGARCHIVE%

..\wget %JPGURL%
cd %libdir%\jpeg
md src
cd src
%op%\..\7z e %op%\%JPGARCHIVE%
ren jconfig.vc jconfig.h
rd jpeg-6b
del *.obj
del *.jpg
del *.bmp
del *.ppm
del *.doc
del makefile.*
cd /D %op%
del %JPGARCHIVE%


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
