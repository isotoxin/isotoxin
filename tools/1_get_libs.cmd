@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

cd getlibs

for /f %%i in ('"dir _*.cmd /B"') do ( 
call %%i
)
cd ..
