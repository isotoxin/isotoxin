echo FastDelegate ....................................
rd FastDelegate /S /Q
git clone https://github.com/yxbh/FastDelegate
move FastDelegate\*.hpp %1\FastDelegate\FastDelegate.h
rd FastDelegate /S /Q
