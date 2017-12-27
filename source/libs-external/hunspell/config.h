#pragma once

/*
	info for gcc users
	so, hunspell used std::string
	but this hunspell uses fake std::string. real strings are zstrings
	that is why so many defs below (z_str_fake_std.h). These defs used to avoid include of gcc-internal std files to avoid name conflict
	This is so tricky, I know. If you ported, you would, of course, have done otherwise. So humble yourself. It is done as it is done.
*/

#include <zstrings/z_str_fake_std.h> // 1st include fake std::string


#define HUNSPELL_STATIC
#define LIBHUNSPELL_DLL_EXPORTED

#include "hunspell_io.h"
