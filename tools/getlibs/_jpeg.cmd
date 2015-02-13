@echo off

set op=%cd%

set JPGARCHIVE=jpegsr6.zip
set JPGURL=http://downloads.sourceforge.net/project/libjpeg/libjpeg/6b/%JPGARCHIVE%


echo LibJpeg ....................................
..\wget %JPGURL%
cd %1\jpeg
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
