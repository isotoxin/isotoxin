# Isotoxin
Isotoxin source
http://isotoxin.im

# Build instruction
- Threre are 4 external utils used: **git**, **7z**, **grep**, **wget**<br />
You have to install command line **git**<br />
**7z**, **grep** and **wget** alredy in repo.<br />
Also, Visual Studio 2013 must be installed.<br />
- run **1_get_libs.cmd** - it will download some libs (such as zlib, pnglib and so on) from external sources. All you need is internet access. **wget** and **git** used to download, **7z** used to unpack files<br />
- run **2_build_libs.cmd** - it builds downloaded external libs<br />
- run **3_make_libcmt_nomem.cmd** - it creates libcmt_nomem.lib - croped version of standard libcmt.lib without any memory-related functions. We want to use dlmalloc everywhere :) **grep** utility used at this step<br />
- run **4_build_isotoxin.cmd** - final build of isotoxin.exe, plghost.exe and proto.dll's<br />
- to run isotoxin, you need isotoxin.data - zip archive of isotoxin assets. You can take it from http://isotoxin.im/files from latest version archive.<br />

