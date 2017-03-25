#include "stdafx.h"

#ifdef _WIN32
#pragma warning (push)
#pragma warning (disable:4324)
#include "libsodium/src/libsodium/include/sodium.h"
#pragma warning (pop)
#endif

#ifdef _NIX
#include <sodium.h>
#endif


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

    if (!is_dir_exists(spd))
    {
        Print(FOREGROUND_RED, "path-to-dictionaries not found: %s", spd.cstr()); return 0;
    }

    wstrings_c lst;
    wstrings_c sfiles;

    scan_dir_t(spd, [&](int lv, ts::scan_dir_file_descriptor_c &fd) {
        sfiles.add(fd.fullname());
        return ts::SD_CONTINUE;
    });

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

            uint8 hash[crypto_generichash_BYTES_MIN];
            crypto_generichash_state st;
            crypto_generichash_init( &st, nullptr, 0, sizeof( hash ) );
            crypto_generichash_update( &st, aff.data(), aff.size() );
            crypto_generichash_update( &st, dic.data(), dic.size() );
            crypto_generichash_final( &st, hash, sizeof(hash) );

            ts::wstr_c fnold = fn_get_name( f );
            ts::wstr_c fn( fnold );

            if ( fn.equals( CONSTWSTR( "la" ) ) )
                fn.set( CONSTWSTR("Latin") );

            if ( !fn.equals( fnold ) )
                ren_or_move_file( fnold + CONSTWSTR(".zip"), fn + CONSTWSTR( ".zip" ) );

            lst.add( fn.append(CONSTWSTR("=")).append_as_hex( hash, sizeof( hash ) ) );
        }
    }

    lst.sort( true );
    savelines( CONSTASTR("dictindex.txt"), lst );


    return 0;
}