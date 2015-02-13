SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set vsc="%VS120COMNTOOLS%..\..\vc\bin"
set msb="C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe"
set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

%msb% ..\source\libs-external\toxcore\vs\toxcore.vcxproj /fl1 /clp:ErrorsOnly /m:3 /t:Rebuild /p:Configuration=Debug;SolutionDir=..\..\;GARBAGE=%GARBAGE%
%msb% ..\source\libs-external\libs13.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Release;GARBAGE=%GARBAGE%
