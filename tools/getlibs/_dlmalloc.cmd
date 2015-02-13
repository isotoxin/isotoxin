echo dlmalloc ....................................
rd dlmalloc /S /Q
git clone https://github.com/colorer/dlmalloc
md %1\dlmalloc
move dlmalloc\dlmalloc.c %1\dlmalloc
rd dlmalloc /S /Q
