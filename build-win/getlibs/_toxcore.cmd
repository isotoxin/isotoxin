@echo off
set getlib=toxcore-vs
echo getting: %getlib%
set libdir=%1
if x%libdir%==x set libdir=..\..\source\libs-external
if exist %libdir%\toxcore\toxcore\tox.h goto alrd

if exist toxcore-vs rd toxcore-vs /S /Q
git clone https://github.com/Rotkaermota/toxcore-vs

if exist %libdir%\opus rd %libdir%\opus /S /Q
move /Y toxcore-vs\opus %libdir% >nul

if exist %libdir%\libsodium rd %libdir%\libsodium /S /Q
move /Y toxcore-vs\libsodium %libdir% >nul

if exist %libdir%\libvpx rd %libdir%\libvpx /S /Q
move /Y toxcore-vs\libvpx %libdir% >nul

if exist %libdir%\toxcore rd %libdir%\toxcore /S /Q
move /Y toxcore-vs\toxcore %libdir% >nul

rd toxcore-vs /S /Q

echo ok: %getlib% obtained
goto enda
:alrd
echo ok: %getlib% already here
:enda
