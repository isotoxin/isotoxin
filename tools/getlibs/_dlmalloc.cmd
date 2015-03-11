@echo off
set getlib=dlmalloc
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\dlmalloc\dlmalloc.c goto alrd

if exist dlmalloc rd dlmalloc /S /Q
git clone https://github.com/colorer/dlmalloc
if not exist %libdir%\dlmalloc md %libdir%\dlmalloc
move dlmalloc\dlmalloc.c %libdir%\dlmalloc >nul
rd dlmalloc /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
