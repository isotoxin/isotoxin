#pragma once

namespace ts
{
ivec2 TSCALL parsevec2(const asptr &s, const ivec2 &def, bool cloneval = false);
vec2 TSCALL parsevec2f(const asptr &s, const vec2 &def, bool cloneval = false);
irect TSCALL parserect(token<char> &tokens, const irect &def, str_c *tail = nullptr);
irect TSCALL parserect(const asptr &s, const irect &def, str_c *tail = nullptr);
template<typename THCARACTER> TSCOLOR TSCALL parsecolor(const sptr<THCARACTER> &s, const TSCOLOR def);

#define RAPIDXML_NO_EXCEPTIONS
#define RAPIDXML_NO_STDLIB

#include "internal/rapidxml_iterators.h"

}
