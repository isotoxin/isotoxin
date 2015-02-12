@echo off

echo curl ....................................
rd curl /S /Q
git clone https://github.com/bagder/curl.git
move curl\include %1\curl
move curl\lib %1\curl
copy %1\curl\include\curlbuild.h.dist %1\curl\include\curlbuild.h
rd curl /S /Q

