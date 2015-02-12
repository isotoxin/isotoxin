echo dlmalloc ....................................
rd dlmalloc /S /Q
git clone https://github.com/colorer/dlmalloc
move dlmalloc\dlmalloc.c %1\dlmalloc
rd dlmalloc /S /Q
