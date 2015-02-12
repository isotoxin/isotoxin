
echo LibOgg ....................................
rd ogg /S /Q
git clone https://git.xiph.org/mirrors/ogg.git
move ogg\include %1\libogg
move ogg\src %1\libogg
rd ogg /S /Q

echo LibVorbis ....................................
rd vorbis /S /Q
git clone https://git.xiph.org/mirrors/vorbis.git
move vorbis\include %1\libvorbis
move vorbis\lib %1\libvorbis
rd vorbis /S /Q
