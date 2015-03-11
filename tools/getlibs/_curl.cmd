@echo off
set getlib=libcurl
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\curl\include\curl\curl.h goto alrd

if exist curl rd curl /S /Q
git clone https://github.com/bagder/curl.git
move curl\include %libdir%\curl >nul
move curl\lib %libdir%\curl >nul
copy %libdir%\curl\include\curl\curlbuild.h.dist %libdir%\curl\include\curl\curlbuild.h >nul
rd curl /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
