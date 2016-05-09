SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set msb="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"

set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

md ..\tools
copy kill_crap.cmd ..\tools
copy prebuild.cmd ..\tools


if a%1a == a64a (
%msb% ..\source\isotoxin.2015.64.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Final;Platform=x64;GARBAGE=%GARBAGE%
) else (
%msb% ..\source\isotoxin.2015.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Final;GARBAGE=%GARBAGE%
)
