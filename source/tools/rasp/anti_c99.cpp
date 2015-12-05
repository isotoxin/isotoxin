#include "stdafx.h"

using namespace ts;

static const char *contants[] = {
    "MAX_SENT_NODES",
    "MAX_FRIEND_CLIENTS",
    "MAX_SHARED_RELAYS",
    "DESIRED_CLOSE_CONNECTIONS",
    "NUM_SAVED_TCP_RELAYS",
    "NUM_SAVED_PATH_NODES",
    "SIZE_IPPORT",
    "INET6_ADDRSTRLEN",
    "RETURN_1",
    "RETURN_2",
    "MAX_ONION_CLIENTS",
    "MAX_ONION_CLIENTS_ANNOUNCE",
    "MAX_EVENTS",
    "MAX_SAVED_DHT_NODES",
};

struct decomment
{
    int skipcode = 0;
    bool cmnt = false;
    str_c operator()( const str_c&l_ )
    {
#define PREVC() ((i>0)?l.get_char(i-1):0)
        str_c l(l_);

        if (skipcode)
        {
            if (l.begins(CONSTASTR("#if ")) || l.begins(CONSTASTR("#ifdef ")))
                ++skipcode;
            else if (l.begins(CONSTASTR("#endif")))
                --skipcode;
            return str_c();
        }

        if (l.begins(CONSTASTR("#ifdef LOGGING")))
            ++skipcode;

        if (cmnt)
        {
            int cc = l.find_pos(CONSTASTR("*/"));
            if (cc < 0) return str_c();
            l.cut(0, cc+2);
            cmnt = false;
        }

        l.trim();
        bool in_string = false;
        for(int i=0;i<l.get_length();++i)
        {
            char ch = l.get_char(i);
            if (in_string)
            {
                if (ch == '\"')
                {
                    if (PREVC() == '\\')
                        continue;
                    in_string = false;
                }

            } else
            {
                if (ch == '\"')
                {
                    in_string = true;
                } else if (ch == '/')
                {
                    if (PREVC() == '/')
                    {
                        l.set_length(i-1);
                        break;
                    }
                }
                else if (ch == '*')
                {
                    if (PREVC() == '/')
                    {
                        int cl = l.find_pos(i, CONSTASTR("*/"));
                        if (cl > i)
                        {
                            l.cut(i-1,cl-i+3);
                        } else
                        {
                            l.set_length(i - 1);
                            cmnt = true;
                            break;
                        }

                    }
                }
            }
        }
        l.trim();
        return l;
    }

};

bool    constchar(const char c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    return c == '_';
}

bool is_const(const str_c &s)
{
    if (s.begins(CONSTASTR("crypto_box_")) || s.begins(CONSTASTR("crypto_hash_")))
        return true;

    for(int i=0;i<s.get_length();++i)
        if (!constchar(s.get_char(i)))
            return false;

    if (s.ends(CONSTASTR("_LENGTH")) || s.ends(CONSTASTR("_SIZE")))
        return true;

    for (const char *c : contants)
        if (s.equals(asptr(c)))
            return true;
    return false;
}

bool is_num(const str_c &s)
{
    for(int i=0;i<s.get_length();++i)
        if (!CHAR_is_digit(s.get_char(i))) return false;
    return true;
}

bool find_c99(const astrings_c & lines, int &sl, int &el)
{
    decomment dc;
    int cnt = lines.size();
    for (int ln = 0; ln < cnt; ++ln)
    {
        str_c l = dc(lines.get(ln));

        decomment dc2(dc);
        int c1 = l.count_chars(']');
        int c2 = l.count_chars('[');
        int sll = ln;
        while (c2 > c1)
        {
            if (sll == (cnt - 1)) return false;
            str_c lnext = dc2(lines.get(++sll));
            l.append(lnext);
            c1 += lnext.count_chars(']');
            c2 += lnext.count_chars('[');
        }

        int c = l.find_pos(CONSTASTR("//"));
        if (c >= 0)
            l.set_length(c);
        l.trim();
        if (l.is_empty())
            continue;

        int brs = l.find_pos('[');
        if (brs < 0)
            continue;

        int vars = -1;
        for (int i = brs - 1; i > 0; --i)
        {
            if (!CHAR_is_letter(l.get_char(i)) && l.get_char(i) != '_' && !CHAR_is_digit(l.get_char(i)))
            {
                vars = i + 1;
                break;
            }
        }
        if (vars < 0)
            continue;

        int bre = -1; int brc = 0;
        for (int i = brs + 1; i < l.get_length(); ++i)
        {
            if (l.get_char(i) == '[') ++brc;
            else if (l.get_char(i) == ']') --brc;
            if (brc < 0)
            {
                bre = i;
                break;
            }
        }
        if (bre < 0)
            continue;

        bool fail = true;
        for(int i=bre+1; i<l.get_length();++i)
        {
            char ch = l.get_char(i);
            if (ch == ' ') continue;
            if (ch == ';' || ch == ',')
            {
                fail = false;
            }
            break;
        }
        if (fail)
            continue;

        str_c type_of_elements = l.substr(0, vars); type_of_elements.trim();
        char lch = type_of_elements.get_last_char();
        if (lch == '=' || lch == '&' || lch == '>' || lch == '+' || lch == '-' || lch == '|' || lch == ')' || 
            type_of_elements.equals(CONSTASTR("return")) ||
            type_of_elements.count_chars('(') != type_of_elements.count_chars(')'))
            continue;

        str_c name_of_array = l.substr(vars, brs); name_of_array.trim();
        str_c num_of_elements = l.substr(brs + 1, bre); num_of_elements.trim();
        
        if (is_const(num_of_elements))
            continue;

        num_of_elements.replace_all('+', '$');
        num_of_elements.replace_all('-', '$');
        num_of_elements.replace_all('*', '$');
        num_of_elements.replace_all('/', '$');
        num_of_elements.replace_all('%', '$');
        num_of_elements.replace_all('(', '$');
        num_of_elements.replace_all(')', '$');
        astrings_c ppp(num_of_elements, '$'); ppp.trim();
        fail = true;
        bool skipnext = false;
        for(const str_c &e : ppp)
        {
            if (skipnext)
            {
                skipnext = false;
                continue;
            }
            if (e.is_empty()) continue;
            if (is_const(e)) continue;
            if (e.equals(CONSTASTR("sizeof")))
            {
                skipnext = true;
                continue;
            }
            if (is_num(e)) continue;
            fail = false;
            break;
        }
        if (fail)
            continue;

        sl = ln;
        el = sll;

        decomment dc3(dc);
        int braket = 0;
        for (int j = ln + 1; j < cnt; ++j)
        {
            str_c ll = dc3(lines.get(j));
            braket += ll.count_chars('{');
            braket -= ll.count_chars('}');
            if (braket < 0)
            {
                el = j;
                return true;
            }
        }

        el = cnt - 1;

        return true;

    }
    return false;
}

bool __find_c99( const astrings_c & lines, int &sl, int &el  )
{
    int cnt = lines.size();
    for(int i=0;i<cnt;++i)
    {
        str_c l = lines.get(i);
        l.trim();
        if (l.ends("// C99"))
        {
            if (l.begins("//")) continue;

            int c1 = l.count_chars(']');
            if (c1 == 0) continue; // not dynamic array

            int c2 = l.count_chars('[');
            int ii = i;
            while (c2 < c1)
            {
                if (ii == 0) return false;
                // may be prev line...
                str_c lprev = lines.get(--ii);

                int c10 = lprev.count_chars(']');
                int c20 = lprev.count_chars('[');

                c1 += c10;
                c2 += c20;
            }

            sl = ii;

            int braket = 0;
            for(int j=i+1;j<cnt;++j)
            {
                str_c ll = lines.get(j);
                braket += ll.count_chars('{');
                braket -= ll.count_chars('}');
                if (braket < 0)
                {
                    el = j;
                    return true;
                }
            }
        
            el = cnt - 1;
            return true;
        }
        
    }


    return false;
}

bool find_sizeof(const str_c &l, const str_c &arrayname, int &i0, int &i1)
{
    int sti = 0;
    doagaindude:
    int szofs = l.find_pos( sti, CONSTASTR("sizeof") );
    if (szofs < 0) return false;
    sti = szofs + 6;
    if (szofs > 1 &&l.get_char(szofs-1) == '*' &&l.get_char(szofs-2) == '/' ) goto doagaindude;
    int arrni = l.find_pos(szofs + 6,arrayname);
    if (arrni < 0) goto doagaindude;
    int brs = -1;
    for(int i= szofs + 6; i<l.get_length();++i)
    {
        if (l.get_char(i) == ' ') continue;
        if (l.get_char(i) == '(')
        {
            brs = i;
            break;
        }
        goto doagaindude;
    }
    if (brs < 0) return false;
    int bre = -1;
    for (int i = brs+1; i < l.get_length(); ++i)
    {
        if (l.get_char(i) == ')')
        {
            bre = i;
            break;
        }
    }
    if (bre < 0) goto doagaindude;
    if (arrni < brs || arrni+arrayname.get_length() > bre) goto doagaindude;
    str_c arrn = l.substr(brs+1,bre); arrn.trim();
    if (!arrn.equals(arrayname)) goto doagaindude;

    i0 = szofs;
    i1 = bre;

    return true;
}

bool fix_c99(astrings_c & lines, int sl, int el)
{
    str_c l = lines.get(sl);
    l.trim();

    int c1 = l.count_chars(']');
    int c2 = l.count_chars('[');
    int sll = sl;
    while(c2 > c1)
    {
        if (sll == el) return false;
        str_c lnext = lines.get(++sll); lnext.trim();
        l.append(lnext);
        c1 += lnext.count_chars(']');
        c2 += lnext.count_chars('[');
    }


    int brs = l.find_pos('[');
    if (brs < 0) return false;
    int vars = -1;
    for(int i=brs-1; i>0;--i)
    {
        if ( !CHAR_is_letter(l.get_char(i)) && l.get_char(i)!='_' && !CHAR_is_digit(l.get_char(i)) )
        {
            vars = i+1;
            break;
        }
    }
    if (vars < 0) return false;
    int bre = -1; int brc = 0;
    for (int i = brs + 1; i < l.get_length(); ++i)
    {
        if (l.get_char(i) == '[') ++brc;
        else if (l.get_char(i) == ']') --brc;
        if (brc < 0)
        {
            bre = i;
            break;
        }
    }
    if (bre < 0) return false;
    str_c type_of_elements = l.substr(0,vars); type_of_elements.trim();
    str_c name_of_array = l.substr(vars,brs); name_of_array.trim();
    str_c num_of_elements = l.substr(brs+1,bre); num_of_elements.trim();

    int ins = lines.get(sl).find_pos(type_of_elements);
    if (type_of_elements.ends(CONSTASTR(",")))
    {
        // multiple vars in line
        type_of_elements.trunc_length(1).append(CONSTASTR(";\r"));
        lines.insert(sl, str_c(lines.get(sl).substr(0, ins)).append(type_of_elements));
        ++sl; ++sll;
        astrings_c t(type_of_elements, ' ');
        int tr = 1;
        if (t.get(0).equals(CONSTASTR("unsigned"))) ++tr;
        t.truncate(tr);
        type_of_elements = t.join(' ');
    }
    str_c ss1; ss1.fill(ins, ' ').append(CONSTASTR("DYNAMIC( ")).append(type_of_elements).append(CONSTASTR(", ")).append(name_of_array).append(CONSTASTR(", ")).append(num_of_elements).append(CONSTASTR(" ); // -C99\r"));
    lines.set(sl, ss1);
    while(sll > sl)
        lines.remove_slow(sll--), --el;

#if 0
    for(int i=sl;i<=sll;++i)
        lines.get(i).insert(ins,"//");
    
    str_c sizeofarr( CONSTASTR("sizeof_") ); sizeofarr.append(name_of_array);
    str_c s1; s1.fill(ins,' ').append( CONSTASTR("size_t ") ).append(sizeofarr).append( CONSTASTR(" = sizeof(") ).append(type_of_elements).append( CONSTASTR(") * (") ).append(num_of_elements).append(CONSTASTR("); // -C99\r"));
    str_c s2; s2.fill(ins,' ').append( type_of_elements ).append( CONSTASTR("* ") ).append(name_of_array).append( CONSTASTR(" = _alloca( ") ).append(sizeofarr).append( CONSTASTR(" ); // -C99\r") );

    lines.insert(sll+1, s2);
    lines.insert(sll+1, s1);

    el += 2;
#endif

    for(int i=sl+1;i<el;)
    {
        str_c &ln = lines.get(i);
        int i0, i1;
        if ( find_sizeof(ln, name_of_array, i0, i1) )
        {
            ln.replace(i0,6,CONSTASTR("sizeOf"));
            continue;
        }
        ++i;
    }

    return true;
}

static void savelines(const asptr&fn, const astrings_c & lines)
{
    FILE *f = fopen(tmp_str_c(fn), "wb");
    tmp_str_c fbody = lines.join('\n');
    fwrite(fbody.cstr(), 1, fbody.get_length(), f);
    fclose(f);
}

int proc_antic99(const wstrings_c & pars)
{
    if (pars.size() < 2) return 0;
    wstr_c c99 = pars.get(1); fix_path( c99, FNO_SIMPLIFY );

    if (!is_file_exists(c99.as_sptr()))
    {
        Print(FOREGROUND_RED, "c99 file not found: %s\n", to_str(c99).cstr()); return 0;
    }

    astrings_c lines;
    parse_text_file(c99, lines);

    int fixes = 0;
    int sl = 0;
    int el = 0;
    for(; find_c99(lines, sl, el) ;)
    {
        //for(int i = sl; i<= el; ++i)
        //    Print(FOREGROUND_BLUE, "%s\n", lines.get(i).cstr());

//#if 0
        if (!fix_c99(lines, sl, el))
        {
            Print(FOREGROUND_RED, "bad line: <%s>\n", lines.get(sl).cstr());
            return 0;
        }
        ++fixes;
        
        //break;
//#endif
    }

    //savelines( str_c(c99).append(asptr(".fixed")), lines );
    //if (fixes) savelines( str_c(c99).append( asptr("99free.c") ), lines );
    if (fixes) savelines( to_str(c99), lines );

    Print(FOREGROUND_GREEN, "done %s. fixes: %i\n", to_str(fn_get_name_with_ext(c99)).cstr(), fixes);

    return 0;
}