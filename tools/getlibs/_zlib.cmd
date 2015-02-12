echo zlib ....................................

rd zlib /S /Q
git clone https://github.com/madler/zlib

md %1\zlib\src
move zlib\*.c %1\zlib\src
move zlib\*.h %1\zlib\src

md %1\zlib\src\contrib
move zlib\contrib\minizip %1\zlib\src\contrib
move zlib\contrib\masmx64 %1\zlib\src\contrib
move zlib\contrib\masmx86 %1\zlib\src\contrib

rd zlib /S /Q
