@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

cd getlibs

for /f %%i in ('"dir _*.cmd /B"') do ( 
call %%i ..\..\source\libs-external
rem echo %%i
)
rem _jpeg.cmd ..\..\source\libs-external
cd ..
