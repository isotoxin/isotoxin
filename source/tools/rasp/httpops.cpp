#include "stdafx.h"
//#include "curl/include/curl/curl.h"

using namespace ts;

struct context_s
{
    hashmap_t<str_c, str_c> vals;

    void podstava(str_c &s)
    {
        for(int index=s.find_pos('{'); index >= 0; index = s.find_pos(index+1,'{'))
        {
            int i2 = s.find_pos(index+1,'}');
            if (i2 < 0) break;
            if ( auto * v = vals.find(s.substr(index+1, i2)) )
            {
                s.replace(index,i2-index+1, v->value);
            }
        }
    }

    void dojob( abp_c *bp )
    {
        str_c post;
        abp_c *postbp = bp->get(CONSTASTR("post"));
        if (postbp)
        {
            for (auto it = postbp->begin(); it; ++it)
            {
                post.append(it.name());
                post.append_char('\1');
                post.append(it->as_string());
                post.append_char('\2');
            }
            post.trunc_length();
            podstava(post);
            repls:
            post.replace_all(CONSTASTR("%"), CONSTASTR("%25"));
            post.replace_all(CONSTASTR("="), CONSTASTR("%3D"));
            post.replace_all(CONSTASTR("&"), CONSTASTR("%26"));
            post.replace_all(CONSTASTR("+"), CONSTASTR("%2B"));
            post.replace_all(CONSTASTR("\""), CONSTASTR("%22"));
            post.replace_all(CONSTASTR("\\"), CONSTASTR("%5C"));
            post.replace_all(CONSTASTR("/"), CONSTASTR("%2F"));
            post.replace_all(CONSTASTR("<"), CONSTASTR("%3C"));
            post.replace_all(CONSTASTR(">"), CONSTASTR("%3E"));
            post.replace_all(' ', '+');
            post.replace_all('\1', '=');
            post.replace_all('\2', '&');

        } else
        {
            wstr_c pf = bp->get_string(CONSTASTR("postfile"));
            if (!pf.is_empty())
            {
                if (!is_file_exists(pf.as_sptr()))
                {
                    Print(FOREGROUND_RED, "postfile file not found: %s\n", pf.cstr());
                    return;
                }

                wstrings_c lines;
                parse_text_file(pf,lines, true);
                for ( wstr_c &s : lines)
                {
                    post.append( to_utf8(s.trim().replace_all('=', '\1')) );
                    post.append_char('\2');
                }
                post.trunc_length();
                goto repls;
            }
        
        }

        str_c constr = bp->get_string(CONSTASTR("constr"));
        if (constr.is_empty())
        {
            Print(FOREGROUND_RED, "empty constr");
            return;
        }
        podstava(constr);

        https_c con;
        buf_c rslt;
        con.get(constr, rslt, post);

        str_c fns = bp->get_string(CONSTASTR("save"));
        if (!fns.is_empty())
        {
            podstava(fns);
            rslt.save_to_file(to_wstr(fns));
        }

        str_c stor = bp->get_string(CONSTASTR("store"));
        if (!stor.is_empty())
            vals[stor] = rslt.cstr();
    }
};

int proc_http(const wstrings_c & pars)
{
    if (pars.size() < 2) return 0;
    wstr_c nodesf = pars.get(1); fix_path(nodesf, FNO_SIMPLIFY);
    if (!is_file_exists(nodesf.as_sptr()))
    {
        Print(FOREGROUND_RED, "command file not found: %s\n", to_str(nodesf).cstr()); return 0;
    }
    context_s ctx;
    buf_c rslt;
    rslt.load_from_disk_file(nodesf);
    abp_c bp; bp.load( rslt.cstr() );

    for (auto it = bp.begin(); it; ++it)
    {
        ctx.dojob(it);
    }

    return 0;
}

