SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set vsc="%VS140COMNTOOLS%..\..\vc\bin"
set msb="C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

if a%1a == a64a (
%msb% ..\source\libs2015_64.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Release;Platform=x64;GARBAGE=%GARBAGE%
) else (
%msb% ..\source\libs2015.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Release;GARBAGE=%GARBAGE%
)