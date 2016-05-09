@echo off

set libdir=..\source\libs-external

if exist %libdir%\opus rd %libdir%\opus /S /Q
if exist %libdir%\libsodium rd %libdir%\libsodium /S /Q
if exist %libdir%\libvpx rd %libdir%\libvpx /S /Q
if exist %libdir%\toxcore rd %libdir%\toxcore /S /Q

call 1-get-libs.cmd
