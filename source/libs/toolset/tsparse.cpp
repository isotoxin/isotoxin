#include "toolset.h"

namespace ts
{


ivec2 TSCALL parsevec2(const asptr &s, const ivec2 &def, bool cloneval)
{
    ivec2 v(def);
    typedef decltype(v.x) ivt;
    ivt * hackptr = (ivt *)&v;
    ivt * hackptr_end = hackptr + 2;
    for (token<char> t(s, ','); t && hackptr < hackptr_end; ++t, ++hackptr)
    {
        *hackptr = t->as_num<ivt>(0);
        if (cloneval)
            *(hackptr + 1) = *hackptr, cloneval = false;
    }
    return v;
}

vec2 TSCALL parsevec2f(const asptr &s, const vec2 &def, bool cloneval)
{
    vec2 v(def);
    typedef decltype(v.x) ivt;
    ivt * hackptr = (ivt *)&v;
    ivt * hackptr_end = hackptr + 2;
    for (ts::token<char> t(s, ','); t && hackptr < hackptr_end; ++t, ++hackptr)
    {
        *hackptr = t->as_num<ivt>(0);
        if (cloneval)
            *(hackptr + 1) = *hackptr, cloneval = false;
    }
    return v;
}


irect TSCALL parserect(token<char> & tokens, const irect &def, str_c *tail)
{
    irect r(def);
    typedef decltype(r.lt.x) irt;
    irt * hackptr = (irt *)&r;
    irt * hackptr_end = hackptr + 2;
    for (; tokens && hackptr < hackptr_end; ++tokens, ++hackptr)
        *hackptr = tokens->as_num<irt>(0);
    hackptr_end += 2;
    for (; tokens && hackptr < hackptr_end; ++tokens, ++hackptr)
    {
        if (tokens->get_last_char() == 's')
            *hackptr = *(hackptr - 2) + tokens->substr(0, tokens->get_length()-1).as_num<irt>(0);
        else
            *hackptr = tokens->as_num<irt>(0);
    }
    if (tokens && tail)
        tail->set(*tokens).append(tokens.sep(),tokens.tail());
    return r;
}

irect TSCALL parserect(const asptr &s, const irect &def, str_c *tail)
{
    token<char> t(s, ',');
    return parserect(t,def,tail);
}

template<typename THCARACTER> TSCOLOR TSCALL parsecolor(const sptr<THCARACTER> &s, const TSCOLOR def)
{
    uint8 color[4] = { RED(def), GREEN(def), BLUE(def), ALPHA(def) };
    if (s.s[0] == L'#')
        hex_to_data(color, sptr<THCARACTER>(s.s + 1, tmin(s.l - 1, 8)));
    else
    {
        token<THCARACTER> t(s, L',');
        for (int i = 0; t; ++t, ++i)
            color[i] = as_byte(t->as_uint());
    }
    return ARGB(color[0], color[1], color[2], color[3]);
}

template TSCOLOR TSCALL parsecolor(const sptr<char> &s, const TSCOLOR def);
template TSCOLOR TSCALL parsecolor(const sptr<wchar> &s, const TSCOLOR def);


}