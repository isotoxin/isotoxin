@echo off
set getlib=fastdelegate
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\FastDelegate\FastDelegate.h goto alrd

if exist FastDelegate rd FastDelegate /S /Q
git clone https://github.com/yxbh/FastDelegate
if not exist %libdir%\fastdelegate md %libdir%\fastdelegate
move FastDelegate\*.hpp %libdir%\FastDelegate\FastDelegate.h >nul
rd FastDelegate /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
