@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

md ..\bin
rd /S /Q ..\bin\~assets
md ..\bin\~assets
md ..\bin\~assets\smiles

cd ..\assets\smiles

for /f %%i in ('"dir * /A:D /B"') do ( 
cd %%i
..\..\..\tools\7z a -tzip -r -mx=9 ..\..\..\bin\~assets\smiles\s.zip
cd ..
ren ..\..\bin\~assets\smiles\s.zip %%i.zip
)

cd ..

xcopy .\loc ..\bin\~assets\loc /s /e /y /i
xcopy .\themes ..\bin\~assets\themes /s /e /y /i
xcopy .\sounds ..\bin\~assets\sounds /s /e /y /i
copy .\*.template ..\bin\~assets\
copy .\icon*.png ..\bin\~assets\

cd ..\bin\~assets
..\..\tools\7z a -tzip -r -mx=9 ..\isotoxin.data
cd ..
rd /S /Q ~assets