@echo off
echo LibFreeType ....................................
rd freetype2 /S /Q
git clone http://git.sv.nongnu.org/r/freetype/freetype2.git
move freetype2\include %1\freetype
move freetype2\src %1\freetype
rd freetype2 /S /Q
