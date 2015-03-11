@echo off
set getlib=ogg vorbis
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\libogg\include\ogg\ogg.h goto getvorbis

if exist ogg rd ogg /S /Q
git clone https://git.xiph.org/mirrors/ogg.git
move ogg\include %libdir%\libogg >nul
move ogg\src %libdir%\libogg >nul
rd ogg /S /Q

:getvorbis
if exist %libdir%\libvorbis\include\vorbis\vorbisenc.h goto alrd

if exist vorbis rd vorbis /S /Q
git clone https://git.xiph.org/mirrors/vorbis.git
move vorbis\include %libdir%\libvorbis >nul
move vorbis\lib %libdir%\libvorbis >nul
rd vorbis /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
