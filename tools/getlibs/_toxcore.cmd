@echo off
echo toxcore-vs ....................................

goto zzz

rd toxcore-vs /S /Q
git clone https://github.com/Rotkaermota/toxcore-vs

rd %1\opus /S /Q
move /Y toxcore-vs\opus %1

:zzz

rd %1\libsodium /S /Q
move /Y toxcore-vs\libsodium %1

rd %1\libvpx /S /Q
move /Y toxcore-vs\libvpx %1

rd %1\toxcore /S /Q
move /Y toxcore-vs\toxcore %1

rd toxcore-vs /S /Q
