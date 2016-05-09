SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set msb="C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe"

set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

md ..\tools
copy kill_crap.cmd ..\tools
copy prebuild.cmd ..\tools


%msb% ..\source\isotoxin.2013.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Final;GARBAGE=%GARBAGE%
