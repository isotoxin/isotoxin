@echo off
set getlib=giflib
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\giflib\lib\gif_lib.h goto alrd

if exist giflib rd giflib /S /Q
git clone git://git.code.sf.net/p/giflib/code giflib
move giflib\lib %libdir%\giflib >nul
rd giflib /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
