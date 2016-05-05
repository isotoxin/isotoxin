SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set msb="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"

set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

%msb% ..\source\isotoxin\(build)\vs2015\isotoxin.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Final;GARBAGE=%GARBAGE%
