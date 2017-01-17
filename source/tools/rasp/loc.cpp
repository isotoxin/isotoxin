#include "stdafx.h"

using namespace ts;

bool findTTT(const wstr_c & l, wstr_c&txt, int &tag, bool &cmnt, int &left, int &rite, int &workindex)
{
    wstr_c ll(l);
    if (cmnt)
    {
        int c3 = ll.find_pos( CONSTWSTR("*/") );
        if (c3 < 0) return false;
        cmnt = false;
        ll.fill(0, c3+2, 32);

    } else for(;;)
    {
        int c1 = -1; //ll.find_pos( CONSTWSTR("//") );
        int c2 = -1; //ll.find_pos( CONSTWSTR("/*") );

        bool instr = false;
        for(int i=0;i<ll.get_length();++i)
        {
            wchar c = ll.get_char(i);
            if (instr)
            {
                if (c == '\"' && ll.get_char(i-1) != '\\')
                {
                    instr = false;
                    continue;
                }
            } else
            {
                if (c == '\"')
                {
                    instr = true;
                    continue;
                }
                if (c == '/' && i < ll.get_length()-1)
                {
                    if (ll.get_char(i+1) == '/')
                    {
                        c1 = i;
                        break;
                    }
                    if (ll.get_char(i+1) == '*')
                    {
                        c2 = i;
                        break;
                    }
                }
            }
        
        }

        if ((c1 < c2 || c2 < 0) && c1 >= 0)
        {
            ll.set_length(c1);
            break;
        }
        if ((c2 < c1 || c1 < 0) && c2 >= 0)
        {
            int c3 = ll.find_pos( c2, CONSTWSTR("*/") );
            if (c3 > c2)
            {
                ll.fill(c2, c3-c2+2, 32);
                continue;
            }
            cmnt = true;
            ll.set_length(c2);
            break;
        }
        break;
    }

svovasearch:
    int tttp = ll.find_pos(workindex, CONSTWSTR("TTT"));
    if ( tttp >= 0 )
    {
        workindex = tttp + 7;
        const wchar * chch = WIDE2(">.#_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (tttp > 0 && CHARz_find<wchar>(chch, ll.get_char(tttp-1)) >= 0) goto svovasearch;
        if (ll.get_length() <= tttp+3 || (ll.get_char(tttp+3) != ' ' && ll.get_char(tttp+3) != '(')) goto svovasearch;
        int skoba = ll.skip(tttp + 3);
        if (skoba >= ll.get_length()) return false;
        if (ll.get_char(skoba) != '(') goto svovasearch;
        int kav = ll.skip(skoba+1);
        if (kav >= ll.get_length()) return false;
        if (ll.get_char(kav) != '\"') goto svovasearch;
        for(int i=kav+1; i<ll.get_length();++i)
        {
            if (ll.get_char(i) == '\"' && ll.get_char(i-1) != '\\')
            {
                txt = ll.substr(kav+1,i);
                left = tttp;

                int zpt_or_skoba2 = ll.skip(i+1);
                if (zpt_or_skoba2 >= ll.get_length()) return false;
                if (ll.get_char(zpt_or_skoba2) == ')')
                {
                    tag = -1;
                    rite = zpt_or_skoba2 + 1;
                    workindex = rite;
                } else if (ll.get_char(zpt_or_skoba2) == ',')
                {
                    int tagss = ll.skip(zpt_or_skoba2+1);
                    if (tagss >= ll.get_length()) return false;
                    int tagss_e = ll.skip(tagss, [](wchar c)->bool { return CHAR_is_digit(c);});
                    if (tagss_e >= ll.get_length()) return false;
                    int tagss_es = ll.skip(tagss_e);
                    if (tagss_es >= ll.get_length()) return false;
                    if (ll.get_char(tagss_es) != ')') goto svovasearch;
                    tag = ll.substr(tagss,tagss_e).as_int();
                    if (tag < 0) tag = 0;
                    rite = tagss_es + 1;
                    workindex = rite;
                }
                return true;
            }
        }
    }

    return false;
}

static void savelines(const wsptr&fn, const wstrings_c & lines)
{
    FILE *f = fopen(to_str(fn), "wb");
    str_c fbody = to_str(lines.join('\n'));
    fwrite(fbody.cstr(), 1, fbody.get_length(), f);
    fclose(f);
}

struct rpl_s : public ts::movable_flag<true>
{
    wstr_c fn;
    wstr_c txt;
    int ln;
    int left;
    int rite;
    int tag;

    int operator()( const rpl_s&o ) const
    {
        return SIGN( tag - o.tag );
    }
};

int proc_loc(const wstrings_c & pars)
{
    if (pars.size() < 3) return 0;
    wstr_c pts = pars.get(1); fix_path(pts, FNO_SIMPLIFY);

    if (!dir_present(pts))
    { Print(FOREGROUND_RED, "path-to-source not found: %s", pts.cstr()); return 0; }

    wstr_c ptl = pars.get(2); fix_path(ptl, FNO_SIMPLIFY);
    if (!dir_present(ptl))
    { Print(FOREGROUND_RED, "path-to-loc not found: %s", ptl.cstr()); return 0; }

    wstrings_c lines;
    wstrings_c sfiles;
    fill_dirs_and_files(pts,sfiles,lines);

    tbuf_t<aint> used_tags;

    array_inplace_t<rpl_s, 128> need2rpl;

    for( const wstr_c & f : sfiles )
    {
        rpl_s r;
        r.fn = f;
        if (f.ends(CONSTWSTR(".h")) || f.ends(CONSTWSTR(".cpp")))
        {
            //tmp_str_c afn(f);
            //CHARz_copy(fnnnn, afn.cstr());

            parse_text_file(f,lines);
            bool cmnt = false;
            aint cnt = lines.size();
            for(r.ln=0;r.ln<cnt;++r.ln)
            {
                const wstr_c & l = lines.get(r.ln);
                int workindex = 0;
                while (findTTT(l, r.txt, r.tag, cmnt, r.left, r.rite, workindex))
                {
                    //Print( FOREGROUND_GREEN, "TTT(%i): %s\n", r.tag, tmp_str_c(r.txt).cstr());
                    if (r.tag >= 0)
                    {
                        aint i = used_tags.set(r.tag);
                        if (i < (used_tags.count()-1))
                        {
                            for(const rpl_s &rr : need2rpl)
                            {
                                if (rr.tag == r.tag)
                                {
                                    Print( FOREGROUND_RED, "%s(%i): Tag already used: TTT(%i): %s\n", to_str(rr.fn).cstr(), rr.ln, rr.tag, to_str(rr.txt).cstr());
                                }
                            }

                            Print( FOREGROUND_RED, "%s(%i): Tag already used: TTT(%i): %s\n", to_str(f).cstr(), r.ln, r.tag, to_str(r.txt).cstr());
                        }
                    }
                    need2rpl.add(r);
                }
                
            }
        }
    }


    //b.load_from_disk_file(str_c(fn_join(ptl, CONSTWSTR("en.labels.lng"))).as_sptr());
    //b.detect();
    //lngbp.load(b.as_str<wchar>());

    used_tags.qsort();

    auto getfreetag = [&]()->int {
        static int lastchecktag = 0;
        tbuf_t<aint>::default_comparator dc;
        for(aint tmp = 0;used_tags.find_index_sorted(tmp,++lastchecktag, dc););
        return lastchecktag;
    };

    wstr_c prevfile;
    bool lneschanged = false;
    for (rpl_s & r : need2rpl)
    {
        if (r.tag <= 0)
        {
            r.tag = getfreetag();

            if (prevfile != r.fn)
            {
                if (lneschanged)
                    savelines(prevfile, lines);
                lneschanged = false;
                parse_text_file(r.fn, lines);
                prevfile = r.fn;
            }

            wstr_c & l = lines.get(r.ln);
            l.replace( r.left, r.rite-r.left, CONSTWSTR("TTT(\"") + r.txt + CONSTWSTR("\",") + wmake(r.tag) + CONSTWSTR(")") );
            lneschanged = true;
        }
    }

    if (lneschanged)
        savelines(prevfile, lines);

    need2rpl.sort();

    FILE *f = fopen(to_str(fn_join(ptl, CONSTWSTR("en.labels.lng"))), "wb");
    swstr_c buf;

    //uint16 signa = 0xFEFF;
    //fwrite(&signa,2,1,f);

    for (const rpl_s & r : need2rpl)
    {
        buf.set_as_num( r.tag ).append(CONSTWSTR("=")).append(r.txt).append(CONSTWSTR("\r\n"));
        ts::str_c utf8 = to_utf8(buf);
        fwrite(utf8.cstr(), 1, utf8.get_length(), f);
    }
    fclose(f);

    //getch();
    return 0;
}


int proc_lochange(const wstrings_c & pars)
{
    if (pars.size() < 3) return 0;
    wstr_c pts = pars.get(1); fix_path(pts, FNO_SIMPLIFY);

    if (!dir_present(pts))
    {
        Print(FOREGROUND_RED, "path-to-source not found: %s", pts.cstr()); return 0;
    }

    wstr_c ptl = pars.get(2); fix_path(ptl, FNO_SIMPLIFY);
    if (!dir_present(ptl))
    {
        Print(FOREGROUND_RED, "path-to-loc not found: %s", ptl.cstr()); return 0;
    }

    wstr_c tolocale( CONSTWSTR("en") );
    if (pars.size() >= 4)
        tolocale = pars.get(3);

    wstrings_c lines;
    wstrings_c sfiles;
    fill_dirs_and_files(pts, sfiles, lines);


    hashmap_t<aint, wstr_c> localehash;
    wstrings_c localelines;
    parse_text_file(fn_join(ptl, tolocale) + CONSTWSTR(".labels.lng"), localelines, true);
    for (const wstr_c &ls : localelines)
    {
        token<wchar> t(ls, '=');
        pwstr_c stag = *t;
        int tag = t->as_int(-1);
        ++t;
        wstr_c l(*t); l.trim();

        if (tag > 0)
            localehash[tag] = l;
    }


    for (const wstr_c & f : sfiles)
    {
        if (f.ends(CONSTWSTR(".h")) || f.ends(CONSTWSTR(".cpp")))
        {
            parse_text_file(f, lines);
            bool cmnt = false;
            bool changed = false;
            aint cnt = lines.size();
            for (int ln = 0; ln < cnt; ++ln)
            {
                wstr_c & l = lines.get(ln);
                int workindex = 0;
                wstr_c txt;
                int tag, left, rite;

                while (findTTT(l, txt, tag, cmnt, left, rite, workindex))
                {
                    if (tag >= 0)
                    {
                        wstr_c txtto = localehash[tag];
                        l.replace( left, rite-left, CONSTWSTR("TTT(\"") + txtto + CONSTWSTR("\",") + wmake(tag) + CONSTWSTR(")") );
                        changed = true;
                    }
                }

            }
            if (changed)
                savelines(f, lines);

        }
    }
    return 0;
}

int proc_dos2unix(const wstrings_c & pars)
{
    if (pars.size() < 3) return 0;
    wstr_c pts = pars.get(1); fix_path(pts, FNO_SIMPLIFY);

    if (!dir_present(pts))
    {
        Print(FOREGROUND_RED, "path-to-files not found: %s", pts.cstr()); return 0;
    }

    wstrings_c exts(pars.get(2), '.');

    if (exts.size() == 0)
    {
        Print(FOREGROUND_RED, "no exts defined");
        return 0;
    }


    wstrings_c lines;
    wstrings_c sfiles;
    fill_dirs_and_files(pts, sfiles, lines);


    for (const wstr_c & f : sfiles)
    {
        bool ok = false;
        for (const wstr_c & e : exts)
            if (f.ends( ts::wstr_c(CONSTWSTR("."),e).as_sptr() ) || fn_get_name_with_ext(f).equals(e))
            {
                ok = true;
                break;
            }

        if (ok)
        {
            buf_c b;
            b.load_from_disk_file(f);

            for (aint i = b.size() - 1; i >= 0; --i)
            {
                if (b.data()[i] == '\r')
                    b.cut(i, 1);
            }
            b.save_to_file(f);
        }
    }


    return 0;
}

int proc_unix2dos(const wstrings_c & pars)
{
    if (pars.size() < 3) return 0;
    wstr_c pts = pars.get(1); fix_path(pts, FNO_SIMPLIFY);

    if (!dir_present(pts))
    {
        Print(FOREGROUND_RED, "path-to-files not found: %s", pts.cstr()); return 0;
    }

    wstrings_c exts(pars.get(2), '.');

    if (exts.size() == 0)
    {
        Print(FOREGROUND_RED, "no exts defined");
        return 0;
    }


    wstrings_c lines;
    wstrings_c sfiles;
    fill_dirs_and_files(pts, sfiles, lines);


    for (const wstr_c & f : sfiles)
    {
        bool ok = false;
        for (const wstr_c & e : exts)
            if (f.ends( ts::wstr_c(CONSTWSTR("."),e).as_sptr() ) || fn_get_name_with_ext(f).equals(e))
            {
                ok = true;
                break;
            }

        if (ok)
        {
            buf_c b;
            b.load_from_disk_file(f);

            for (aint i = b.size() - 1; i >= 0; --i)
            {
                if (b.data()[i] == '\r')
                    b.cut(i, 1);
            }

            for (aint i = b.size() - 1; i >= 0; --i)
            {
                if (b.data()[i] == '\n')
                    *b.expand(i, 1) = '\r';
            }

            b.save_to_file(f);
        }
    }


    return 0;
}


int proc_fxml( const wstrings_c & pars )
{
    if ( pars.size() < 2 ) return 0;
    wstr_c xmlf = pars.get( 1 );

    buf_c b; b.load_from_disk_file( xmlf );

    if ( b.size() == 0 )
    {
        Print( FOREGROUND_RED, "no xml defined" );
        return 0;
    }

    int tab = 0;
    str_c s( b.cstr() );
    str_c o;

    bool intag = false;
    bool closetag = false;
    bool data = false;
    for( int i=0;i<s.get_length();++i )
    {
        char c = s.get_char( i );
        char c2 = s.cstr()[i+1];
        if ( intag )
        {
            if ( c == '<' ) break;
            if ( c == '/' && c2 == '>' )
            {
                if ( closetag )
                    break;

                intag = false;
                o.append( CONSTASTR( "/>\r\n" ) );
                ++i;
                continue;
            }
            if ( c == '>' )
            {
                o.append( CONSTASTR( ">\r\n" ) );
                intag = false;
                if (!closetag)
                    tab += 2;
                continue;
            }

            o.append_char( c );

        } else
        {
            if ( c == '<' && c2 == '/' )
            {
                if (data)
                {
                    o.append( CONSTASTR( "\r\n" ) );
                }

                data = false;
                closetag = true;
                if (tab == 0)
                    break;
                tab -= 2;

                o.append_chars( tab, ' ' );
                o.append( CONSTASTR( "</" ) );
                intag = true;
                ++i;
                continue;
            }
            if ( c == '<' )
            {
                if ( data )
                {
                    o.append( CONSTASTR( "\r\n" ) );
                }

                data = false;
                closetag = false;
                o.append_chars(tab, ' ');
                o.append_char( c );
                intag = true;
                continue;
            }

            if (!data )
            {
                o.append_chars( tab, ' ' );
                data = true;
            }
            o.append_char(c);
        }

    }

    b.clear();
    b.append_s( o );
    b.save_to_file( xmlf + CONSTWSTR(".f") );

    return 0;
}