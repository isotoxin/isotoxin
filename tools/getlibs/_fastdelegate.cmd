echo FastDelegate ....................................
rd FastDelegate /S /Q
git clone https://github.com/yxbh/FastDelegate
md %1\fastdelegate
move FastDelegate\*.hpp %1\FastDelegate\FastDelegate.h
rd FastDelegate /S /Q
