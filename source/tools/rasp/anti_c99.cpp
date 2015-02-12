#include "stdafx.h"

using namespace ts;

bool find_c99( const astrings_c & lines, int &sl, int &el  )
{
    int cnt = lines.size();
    for(int i=0;i<cnt;++i)
    {
        str_c l = lines.get(i);
        l.trim();
        if (l.ends("// C99"))
        {
            if (l.begins("//")) continue;;

            int c1 = l.count_chars(']');
            if (c1 == 0) continue;; // not dynamic array

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
        if (sll == (el-1)) return false;
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
    for(int i=sl;i<=sll;++i)
        lines.get(i).insert(ins,"//");
    

    str_c sizeofarr( CONSTASTR("sizeof_") ); sizeofarr.append(name_of_array);
    str_c s1; s1.fill(ins,' ').append( CONSTASTR("size_t ") ).append(sizeofarr).append( CONSTASTR(" = sizeof(") ).append(type_of_elements).append( CONSTASTR(") * (") ).append(num_of_elements).append(CONSTASTR("); // -C99\r"));
    str_c s2; s2.fill(ins,' ').append( type_of_elements ).append( CONSTASTR("* ") ).append(name_of_array).append( CONSTASTR(" = _alloca( ") ).append(sizeofarr).append( CONSTASTR(" ); // -C99\r") );

    lines.insert(sll+1, s2);
    lines.insert(sll+1, s1);

    el += 2;

    for(int i=sl+3;i<el;)
    {
        str_c &l = lines.get(i);
        int i0, i1;
        if ( find_sizeof(l, name_of_array, i0, i1) )
        {
            l.insert(i0,CONSTASTR("/*"));
            l.insert(i1+3,CONSTASTR("*/ "));
            l.insert(i1+6,sizeofarr);
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
    wstr_c c99 = simplify_path(pars.get(1));

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
        if (!fix_c99(lines, sl, el))
        {
            Print(FOREGROUND_RED, "bad line: <%s>\n", lines.get(sl).cstr());
            return 0;
        }
        ++fixes;
    }


    //savelines( str_c(c99).append(asptr(".fixed")), lines );
    //if (fixes) savelines( str_c(c99).append( asptr("99free.c") ), lines );
    if (fixes) savelines( str_c(c99), lines );

    Print(FOREGROUND_GREEN, "done %s. fixes: %i\n", fn_get_name_with_ext(to_str(c99)).cstr(), fixes);

    return 0;
}