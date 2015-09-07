@echo off
set getlib=filter_audio
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\filter_audio\filter_audio.h goto alrd

if exist filter_audio rd filter_audio /S /Q
git clone https://github.com/Rotkaermota/filter_audio

rd filter_audio\.git /S /Q
xcopy filter_audio %libdir%\filter_audio /s /e /y /i
rd filter_audio /S /Q


echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
