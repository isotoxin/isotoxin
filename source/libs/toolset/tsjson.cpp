#include "toolset.h"

//-V::813

namespace ts
{

    static uint16 get_unicode_char( asptr hex4 )
    {
        uint16 unicode;
        pstr_c(hex4).hex2buf<2>((uint8 *)&unicode);
        return (unicode >> 8) | (unicode << 8);
    }

    static aint search_char( char c, asptr s )
    {
        aint i = 0;
        for (; i < s.l; ++i)
        {
            if (CHAR_is_hollow(s.s[i]))
                continue;

            if ( c == s.s[i] )
                return i + 1;
        }
        return -1;
    }

    static aint skip_hollow( asptr s )
    {
        aint i = 0;
        for (; i < s.l;)
        {
            if (CHAR_is_hollow(s.s[i]))
            {
                ++i;
                continue;
            }
            return i;
        }
        return -1;
    }

    static aint parse_str(asptr s, str_c &str)
    {
        aint i = 0;
        for (; i < s.l;)
        {
            switch (s.s[i])
            {
                case '\"':
                    return i + 1;
                case '\\':
                    ++i;
                    if (i >= s.l)
                        return 0;

                    switch (s.s[i])
                    {
                        case '\"': str.append_char('\"'); break;
                        case '\\': str.append_char('\\'); break;
                        case '/': str.append_char('/'); break;
                        case 'b': str.append_char('\b'); break;
                        case 'f': str.append_char('\f'); break;
                        case 'r': str.append_char('\r'); break;
                        case 'n': str.append_char('\n'); break;
                        case 't': str.append_char('\t'); break;
                        case 'u': str.append_unicode_as_utf8(get_unicode_char(s.skip(i + 1).part(4))); i += 4; break;
                    }

                    break;

                default:
                    str.append_char(s.s[i]);
                    break;
            }
            ++i;
        }
        return i;
    }

    aint json_c::parse( asptr s )
    {
        elems.clear();
        aint i = skip_hollow(s);
        if (i >= 0)
        {
            switch (s.s[i])
            {
                case '{':
                    return i + 1 + parse_obj(s.skip(i+1));
                case '[':
                    return i + 1 + parse_arr(s.skip(i+1));
                case '-':
                    return i + 1 + parse_num(true, s.skip(i+1));
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    return i + parse_num(false, s.skip(i));
                case 't':
                    if ( pstr_c(s.skip(i+1)).begins(CONSTASTR("rue")) )
                    {
                        n = 1.0;
                        ni = 1;
                        spec = SV_TRUE;
                        return i+4;
                    }
                    return 0;
                case 'f':
                    if (pstr_c(s.skip(i + 1)).begins(CONSTASTR("alse")))
                    {
                        n = 0;
                        ni = 0;
                        spec = SV_FALSE;
                        return i + 5;
                    }
                    return 0;
                case 'n':
                    if (pstr_c(s.skip(i + 1)).begins(CONSTASTR("ull")))
                    {
                        n = 0;
                        ni = 0;
                        spec = SV_NULL;
                        return i + 4;
                    }
                    return 0;
                case '\"':
                    {
                        aint i2 = parse_str( s.skip(i+1), str );
                        if (i2 > 0)
                        {
                            ni = str.as_num<decltype(ni)>(-1);
                            n = str.as_num<double>();
                            spec = SV_STRING;
                            return i + i2 + 1;
                        }

                    }
                    return 0;

                default:
                    break;
            }
        }
        return 0;
    }

    aint json_c::parse_num( bool negative,  asptr s )
    {
        spec = SV_NUMBER;
        ni = 0;
        aint i = 0;
        for(;i<s.l;)
        {
            if (CHAR_is_digit(s.s[i]))
            {
                ni = ni * 10 + (s.s[i]-'0');
                ++i;
                continue;
            }
            break;
        }

        n = (double)ni;

        if (s.s[i] == '.')
        {
            ++i;
            aint decs = 0;
            aint top = 0;
            for (; i < s.l;)
            {
                if (CHAR_is_digit(s.s[i]))
                {
                    top = top * 10 + (s.s[i]-'0');
                    ++i;
                    ++decs;
                    continue;
                }
                break;
            }
            if (decs)
            {
                n += (double)top / pow( 10.0, decs );
            }
        }

        if (i == 0) return -1;

        if (negative)
        {
            n = -n;
            ni = -ni;
        }
    
        return i;
    }

    aint json_c::parse_obj( asptr s )
    {
        spec = SV_OBJ;
        n = 0;
        ni = 0;

        aint i = 0;
        
        for(;i < s.l;)
        {
            int i2 = search_char('\"', s.skip(i));
            if (i2 < 0) return 0;

            i += i2;
            str_c vnam;
            i2 = parse_str(s.skip(i), vnam);
            if (!i2) return 0;
            i += i2;
            i2 = search_char(':', s.skip(i));
            if (i2 < 0) return 0;
            i += i2;

            elem_s el;
            json_c *njson = TSNEW(json_c);
            el.v.reset( njson );
            i2 = njson->parse(s.skip(i));
            if (i2 > 0)
            {
                i += i2;
                i2 = skip_hollow(s.skip(i));
                if (i2 < 0) return 0;
                i += i2;
                if ( s.s[i] == ',' )
                {
                    el.s = vnam;
                    elems.add() = std::move(el);
                    ++i;
                    continue;
                }

                if (s.s[i] == '}')
                {
                    el.s = vnam;
                    elems.add() = std::move(el);
                    return i + 1;
                }
            }

            break;
        }

        return 0;
    }

    aint json_c::parse_arr(asptr s)
    {
        spec = SV_ARRAY;
        n = 0;
        ni = 0;

        aint i = 0;

        for (;i<s.l;)
        {
            elem_s el;
            json_c *njson = TSNEW(json_c);
            el.v.reset(njson);
            aint i2 = njson->parse(s.skip(i));
            if (i2 > 0)
            {
                i += i2;
                i2 = skip_hollow(s.skip(i));
                if (i2 < 0) return 0;
                i += i2;
                if (s.s[i] == ',')
                {
                    elems.add() = std::move(el);
                    ++i;
                    continue;
                }

                if (s.s[i] == ']')
                {
                    elems.add() = std::move(el);
                    return i + 1;
                }
            }

            break;
        }

        return 0;
    }



} // namespace ts
