#include "stdafx.h"
#include "curl/include/curl/curl.h"

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
    httpsclient.get( "https://wiki.tox.im/Nodes", rslt );
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
        pstr_c ipv6 = get_tag_inner( roffset, row, CONSTASTR("td") );
        pstr_c port = get_tag_inner( roffset, row, CONSTASTR("td") );
        pstr_c clid = get_tag_inner( roffset, row, CONSTASTR("td") );

        tmp_str_c s(CONSTASTR("nodes.emplace_back( CONSTASTR(\"<ip>\"), <port>, CONSTASTR(\"<clid>\") );\r\n"));
        s.replace_all( CONSTASTR("<ip>"), tmp_str_c(ipv4).trim() );
        s.replace_all( CONSTASTR("<port>"), tmp_str_c(port).trim() );
        s.replace_all( CONSTASTR("<clid>"), tmp_str_c(clid).trim() );
        outb.append_buf(s.cstr(), s.get_length());
    }

    outb.save_to_file(nodesf);

    return 0;
}

