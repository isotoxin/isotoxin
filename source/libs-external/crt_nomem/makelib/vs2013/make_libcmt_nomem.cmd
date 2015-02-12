SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set vsc="%VS120COMNTOOLS%..\..\vc\bin"

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

move "%TMP%\nomemlib\in.lib" ".\libcmt_nomem.lib"
