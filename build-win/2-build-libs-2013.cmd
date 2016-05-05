SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set vsc="%VS120COMNTOOLS%..\..\vc\bin"
set msb="C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe"
set GARBAGE=%cd%\..\~garbage
md %GARBAGE%

%msb% ..\source\libs2013.sln /fl2 /clp:ErrorsOnly;PerformanceSummary;Summary /m:3 /t:Rebuild /p:Configuration=Release;GARBAGE=%GARBAGE%

rem create nomem crt lib

if exist "..\source\libs\libcmt_nomem.lib" goto fin

md %TMP%\nomemlib

copy "%VS120COMNTOOLS%..\..\VC\lib\libcmt.lib" "%TMP%\nomemlib" /Y
%vsc%\lib.exe /LIST "%TMP%\nomemlib\libcmt.lib" > "%TMP%\nomemlib\allobj"

del "%TMP%\nomemlib\nomem"
for /f %%i in ('"type nomem"') do ( 
grep %%i "%TMP%\nomemlib\allobj" >> "%TMP%\nomemlib\nomem"
)

copy "%TMP%\nomemlib\libcmt.lib" "%TMP%\nomemlib\in.lib"

for /f %%i in ('"type "%TMP%\nomemlib\nomem""') do ( 

echo %%i
%vsc%\lib.exe /remove:%%i /out:"%TMP%\nomemlib\out.lib" "%TMP%\nomemlib\in.lib"
del "%TMP%\nomemlib\in.lib"
ren "%TMP%\nomemlib\out.lib" in.lib

)

move "%TMP%\nomemlib\in.lib" "..\source\libs\libcmt_nomem.lib"

:fin
