#include "stdafx.h"
#ifdef _WIN32
#include "curl/include/curl/curl.h"
#endif
#ifdef _NIX
#include <curl/curl.h>
#endif // _NIX

using namespace ts;

int find_tag( const asptr&buf, int from, const asptr& tag )
{
    pstr_c d( buf );
    int i = d.find_pos(from, tmp_str_c(CONSTASTR("<"),tag));
    if (i >= 0)
    {
        int j = d.find_pos(i, CONSTASTR(">"));
        if (j > 0) return j+1;
    }

    return -1;
}

pstr_c get_tag_inner( int &offset, const asptr&buf, const asptr& tag )
{
    int tri = find_tag(buf, offset, tag);
    if (tri < 0) return pstr_c();
    int tri_e = find_tag(buf, tri, tmp_str_c(CONSTASTR("/"),tag));
    if (tri_e < 0) return pstr_c();
    offset = tri_e;

    return pstr_c(asptr(buf.s + tri, tri_e - tri - 3 - tag.l));

}

int proc_grabnodes(const wstrings_c & pars)
{
    if (pars.size() < 2) return 0;
    wstr_c nodesf = pars.get(1); fix_path(nodesf, FNO_SIMPLIFY);

    buf_c rslt;

    https_c httpsclient;
    httpsclient.get( "https://wiki.tox.chat/users/nodes", rslt );
    //rslt.load_from_disk_file(nodesf);


    int offset = find_tag( rslt.cstr(), 0, CONSTASTR("table") );
    if (offset < 0)
    {
        Print(FOREGROUND_RED, "tag <table> not found\n");
        return 0;
    }

    get_tag_inner( offset, rslt.cstr(), CONSTASTR("tr") ); // skip row with th's

    buf_c outb;

    for(;;)
    {
        pstr_c row = get_tag_inner( offset, rslt.cstr(), CONSTASTR("tr") );
        if (row.get_length() == 0) break;

        int roffset = 0;
        pstr_c ipv4 = get_tag_inner( roffset, row, CONSTASTR("td") );

        if (ipv4.is_empty())
            break;

        str_c ipv6 = get_tag_inner( roffset, row, CONSTASTR("td") );
        pstr_c port = get_tag_inner( roffset, row, CONSTASTR("td") );
        pstr_c clid = get_tag_inner( roffset, row, CONSTASTR("td") );

        ipv6.trim();
        if (ipv6.equals(CONSTASTR("NONE")) || ipv6.equals(CONSTASTR("-")))
            ipv6.clear();

        tmp_str_c s(CONSTASTR("<ip4>/<ip6>/<port>/<clid>\n"));
        s.replace_all( CONSTASTR("<ip4>"), tmp_str_c(ipv4).trim() );
        s.replace_all( CONSTASTR("<ip6>"), ipv6 );
        s.replace_all( CONSTASTR("<port>"), tmp_str_c(port).trim() );
        s.replace_all( CONSTASTR("<clid>"), tmp_str_c(clid).trim() );
        outb.append_buf(s.cstr(), s.get_length());
    }

    outb.save_to_file(nodesf);

    return 0;
}


str_c get_image_link(const asptr& base, pstr_c img)
{
    int a = img.find_pos('\"');
    int b = img.find_pos(a+1,'\"');
    str_c r(base);
    r.append( img.substr(a+1,b) );
    return r;
}

int proc_emoji(const wstrings_c & pars)
{
    if (pars.size() < 4) return 0;
    str_c r1 = to_str(pars.get(1));
    str_c r2 = to_str(pars.get(2));
    wstr_c n = pars.get(3);

    buf_c rslt;
    https_c httpsclient;
    httpsclient.get("http://unicodey.com/emoji-data/table.htm", rslt);
    //rslt.load_from_disk_file(nodesf);


    int offset = find_tag(rslt.cstr(), 0, CONSTASTR("table"));
    if (offset < 0)
    {
        Print(FOREGROUND_RED, "tag <table> not found\n");
        return 0;
    }

    get_tag_inner(offset, rslt.cstr(), CONSTASTR("tr")); // skip row with th's

    buf_c outb;

    r1.insert(0, CONSTASTR("U+"));
    r2.insert(0, CONSTASTR("U+"));

    make_path(ts::wstr_c(CONSTWSTR("emoji_apple")), 0);
    make_path(ts::wstr_c(CONSTWSTR("emoji_google")), 0);
    make_path(ts::wstr_c(CONSTWSTR("emoji_1")), 0);

    for (;;)
    {
        pstr_c row = get_tag_inner(offset, rslt.cstr(), CONSTASTR("tr"));
        if (row.get_length() == 0) break;

        int roffset = 0;
        pstr_c apple_link = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c google_link = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c noneed = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c emojione_link = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c symb = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c name = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c tag = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c ascii = get_tag_inner(roffset, row, CONSTASTR("td"));
        pstr_c utf8 = get_tag_inner(roffset, row, CONSTASTR("td"));
        if (utf8 >= r1 && utf8 <= r2)
        {
            ts::str_c lnk = get_image_link( CONSTASTR("http://unicodey.com/emoji-data/"), google_link );

            buf_c imgb;
            httpsclient.get(lnk, imgb);
            imgb.save_to_file( fn_join(wstr_c(CONSTWSTR("emoji_google")),CONSTWSTR("0x") + to_wstr(utf8.substr(2).as_sptr())).append(CONSTWSTR(".png")) );


            tmp_str_c s( tag );
            if (!ascii.is_empty())
                s.append_char(',').append(ascii);

            s.append(CONSTASTR("\r\n"));
            outb.append_buf(s.cstr(), s.get_length());
        }

        /*
        tmp_str_c s(CONSTASTR("nodes.emplace_back( CONSTASTR(\"<ip>\"), <port>, CONSTASTR(\"<clid>\") );\r\n"));
        s.replace_all(CONSTASTR("<ip>"), tmp_str_c(ipv4).trim());
        s.replace_all(CONSTASTR("<port>"), tmp_str_c(port).trim());
        s.replace_all(CONSTASTR("<clid>"), tmp_str_c(clid).trim());
        outb.append_buf(s.cstr(), s.get_length());
        */
    }

    outb.save_to_file(n + CONSTWSTR(".ini"));

    return 0;
}

