#include "stdafx.h"

using namespace ts;

static void savelines(const asptr&fn, const wstrings_c & lines)
{
    FILE *f = fopen(tmp_str_c(fn), "wb");
    tmp_str_c fbody = to_utf8( lines.join('\n') );
    fwrite(fbody.cstr(), 1, fbody.get_length(), f);
    fclose(f);
}


int proc_bsdl(const wstrings_c & pars)
{
    if (pars.size() < 2) return 0;
    wstr_c spd = pars.get(1); fix_path(spd, FNO_SIMPLIFY);

    if (!dir_present(spd))
    {
        Print(FOREGROUND_RED, "path-to-dictionaries not found: %s", spd.cstr()); return 0;
    }

    wstrings_c lst;
    wstrings_c sfiles;
    fill_dirs_and_files(spd, sfiles, lst);

    lst.clear();

    buf_c aff, dic;
    for(const wstr_c &f : sfiles)
    {
        if ( f.ends(CONSTWSTR(".aff")) )
        {
            aff.load_from_disk_file(f);
            dic.load_from_disk_file(fn_change_ext(f, CONSTWSTR("dic")));

            if (aff.size() == 0 || dic.size() == 0)
                continue;

            Print(FOREGROUND_GREEN, "processing %s...\n", to_str(fn_get_name(f)).cstr());

            
            md5_c md5;
            md5.update( aff.data(), aff.size() );
            md5.update( dic.data(), dic.size() );
            md5.done();

            lst.add( fn_get_name(f).append(CONSTWSTR("=")).append_as_hex(md5.result(), 16) );
        }
    }

    lst.sort( true );
    savelines( CONSTASTR("dictindex.txt"), lst );


    return 0;
}