#pragma once

#include <stdio.h>

#define _HAS_OLD_IOSTREAMS_MEMBERS 0

#define _FSTREAM_
#define _SSTREAM_
/*
#define _XLOCALE_
#define _XIOSBASE_

#include <xutility>

typedef long long streamsize;
struct locale {};

namespace std
{
    class ios_base
    {
        typedef int seekdir;
        typedef int openmode;

        enum _openmode
        {
            in = 0x01,
            out = 0x02,
        };
    };
}


#include <string>
#include <fstream>
*/

#define HUNSPELL_STATIC
#define LIBHUNSPELL_DLL_EXPORTED

#include "hunspell_io.h"
