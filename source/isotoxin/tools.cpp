#include "isotoxin.h"

#define PROTO_ICON_SIZE 32

const wraptranslate<ts::wsptr> __translation(const ts::wsptr &txt, int tag)
{
    const ts::wsptr r = g_app->label(tag);
    return wraptranslate<ts::wsptr>(r.l==0 ? txt : r);
}

ts::wstr_c lasterror()
{
    ts::wstr_c errs(1024,true);
    DWORD dw = GetLastError();

    FormatMessageW(
        /*FORMAT_MESSAGE_ALLOCATE_BUFFER |*/
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errs.str(),
        1023, NULL);
    errs.set_length();
    return errs;
}

bool check_profile_name(const ts::wstr_c &t)
{
    if (t.find_last_pos_of(CONSTWSTR(":/\\?*<>,;|$#@")) >= 0) return false;
    return true;
}

bool check_netaddr(const ts::asptr & netaddr)
{
    ts::token<char> t(netaddr, ':');
    if (t->find_pos_of(0, CONSTASTR(" _/\\&?*<>@")) > 0) return false;
    if (t->get_last_char() == '.') return false;
    ++t;
    int port = t->as_int(-1);
    if (port < 1 || port > 65535) return false;
    return true;
}

void path_expand_env(ts::wstr_c &path)
{
    ts::parse_env(path);
    //ts::wstr_c cfgfolder = cfg().get_path();
    path.replace_all(CONSTWSTR("%CONFIG%"), ts::fn_get_path(cfg().get_path()).trunc_char(NATIVE_SLASH));

}


/*virtual*/ bool leech_fill_parent_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin = owner->get_min_size();
        ts::ivec2 szmax = owner->get_max_size();
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        HOLD r(owner->getparent());
        ts::irect cr = r().get_client_area();
        MODIFY(*owner).pos(cr.lt).size(cr.size());
        return false;
    }
    return false;
}

/*virtual*/ bool leech_fill_parent_rect_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin = owner->get_min_size() + mrect.lt + mrect.rb;
        ts::ivec2 szmax = owner->get_max_size() - mrect.lt - mrect.rb;
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        HOLD r(owner->getparent());
        ts::irect cr = r().get_client_area();
        MODIFY(*owner).pos(cr.lt+mrect.lt).size(cr.size() - mrect.lt - mrect.rb);
        return false;
    }
    return false;
}

/*virtual*/ bool leech_dock_left_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin = owner->get_min_size(); if (szmin.x < width) szmin.x = width;
        ts::ivec2 szmax = owner->get_max_size(); if (szmax.x < width) szmax.x = width;
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        HOLD r(owner->getparent());
        ts::irect cr = r().get_client_area();
        MODIFY(*owner).pos(cr.lt).size(width, cr.height());
        return false;
    }
    return false;
}

void leech_dock_top_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    MODIFY(*owner).pos(cr.lt).size(cr.width(), height);
}


/*virtual*/ bool leech_dock_top_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin = owner->get_min_size(); if (szmin.y < height) szmin.y = height;
        ts::ivec2 szmax = owner->get_max_size(); if (szmax.y < height) szmax.y = height;
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }
    return false;
}

void leech_dock_bottom_center_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    int xx = cr.lt.x;
    int xspace = (num == 1) ? 0 : x_space;
    if (xspace < 0)
    {
        int rqw = (width * num) + (-xspace * (num + 1));
        xspace = (cr.width() - rqw) / 2;
    }
    cr.lt.x += xspace;
    cr.rb.x -= xspace;
    cr.rb.y -= y_space;

    float fx = (float)(cr.width() - (width * num)) / (float)(num + 1);
    int x = xx + xspace + lround(fx + (width + fx) * index);

    MODIFY(*owner).pos(x, cr.rb.y - height).size(width, height);
}

/*virtual*/ bool leech_dock_bottom_center_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin( width * num + x_space * 2, height + y_space );
        ts::ivec2 szmax = HOLD(owner->getparent())().get_max_size();
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}

void leech_dock_top_center_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    int xx = cr.lt.x;
    int xspace = (num == 1) ? 0 : x_space;
    if (xspace < 0)
    {
        int rqw = (width * num) + (-xspace * (num + 1));
        xspace = (cr.width() - rqw) / 2;
    }
    cr.lt.x += xspace;
    cr.rb.x -= xspace;
    cr.lt.y += y_space;

    float fx = (float)(cr.width() - (width * num)) / (float)(num + 1);
    int x = xx + xspace + lround(fx + (width + fx) * index);

    MODIFY(*owner).pos(x, cr.lt.y).size(width, height);
}

/*virtual*/ bool leech_dock_top_center_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin(width * num + x_space * 2, height + y_space);
        ts::ivec2 szmax = HOLD(owner->getparent())().get_max_size();
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}

void leech_dock_right_center_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    int yspace = y_space;
    if (yspace < 0)
    {
        int rqh = (height * num) + (-yspace * (num + 1));
        yspace = (cr.height() - rqh) / 2;
    }
    cr.lt.y += yspace;
    cr.rb.x -= x_space;
    cr.rb.y -= yspace;

    float fy = (float)(cr.height() - (height * num)) / (float)(num + 1);
    int y = lround(fy + (height + fy) * index);

    MODIFY(*owner).pos(cr.rb.x - width, cr.lt.y +y).size(width, height);
}

/*virtual*/ bool leech_dock_right_center_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin(width + x_space, height * num + y_space * 2);
        ts::ivec2 szmax = HOLD(owner->getparent())().get_max_size();
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}

void leech_dock_bottom_right_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    MODIFY(*owner).pos(cr.rb - ts::ivec2(x_space + width, y_space + height)).size(width, height);
}

/*virtual*/ bool leech_dock_bottom_right_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin(width + x_space, height + y_space);
        ts::ivec2 szmax = HOLD(owner->getparent())().get_max_size();
        r().calc_min_max_by_client_area(szmin, szmax);
        fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}


bool leech_at_right::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (!of) return false;
    if (of->getrid() != rid) return false;
    //ASSERT(of->getparent() == owner->getparent());

    if (qp == SQ_RECT_CHANGED)
    {
        ts::irect cr = of->getprops().rect();
        ts::ivec2 szo = owner->getprops().size();
        if (szo == ts::ivec2(0)) szo = owner->get_min_size();

        MODIFY(*owner).size(szo).pos(cr.rb.x + space, cr.lt.y + (cr.height()-szo.y)/2 ).visible(true);
        return false;
    }
    return false;
}

bool leech_at_left_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (!of) return false;
    if (of->getrid() != rid) return false;
    //ASSERT(of->getparent() == owner->getparent());

    if (qp == SQ_RECT_CHANGED)
    {
        ts::irect cr = of->getprops().rect();
        ts::ivec2 szo = owner->getprops().size();
        if (szo == ts::ivec2(0)) szo = owner->get_min_size();

        MODIFY(*owner).size(szo).pos(cr.lt.x - space - szo.x, cr.lt.y + (cr.height() - szo.y) / 2).visible(true);
        return false;
    }
    return false;
}



/*virtual*/ bool leech_save_proportions_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (owner->getrid() == rid)
    {
        switch(qp)
        {
        case SQ_GROUP_PROPORTIONS_CAHNGED:
            {
                ts::token<char> d( cfg().get(cfgname, defaultv.as_sptr()), ',' );
                ts::str_c v;
                for (float t : data.float_array())
                {
                    int vn = ts::lround(t * divider);
                    if (vn == 0)
                        v.append(*d);
                    else
                        v.append_as_int(vn);
                    ++d;
                    v.append_char(',');
                }
                v.trunc_length();
                cfg().param(cfgname, v);
            }
            break;
        case SQ_RESTORE_SIGNAL:
            {
                gui_hgroup_c *g = ts::ptr_cast<gui_hgroup_c *>(owner);
                g->set_proportions( cfg().get(cfgname, defaultv.as_sptr()), divider );

            }
            break;
        }
    
    }
    
    return false;
}


/*virtual*/ bool leech_save_size_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (owner->getrid() == rid)
    {
        switch (qp)
        {
        case SQ_RECT_CHANGED:
            cfg().param(cfgname, ts::amake<ts::ivec2>(owner->getprops().size()));
            break;
        case SQ_RESTORE_SIGNAL:
            MODIFY(*owner).size( cfg().get<ts::ivec2>(cfgname, defaultsz) );
            break;
        }

    }

    return false;
}










isotoxin_ipc_s::isotoxin_ipc_s(const ts::str_c &tag, datahandler_func datahandler):tag(tag), datahandler(datahandler)
{
    if (!datahandler) return;
    int memba = junct.start(tag);
    if (memba != 0) 
        return;

    if (!ts::start_app(CONSTWSTR("plghost ") + ts::to_wstr(tag), &plughost))
    {
        junct.stop();
        return;
    }

    junct.set_data_callback(handshake_func, this);
    if (!junct.wait_partner(10000))
    {
        junct.stop();
        return;
    }
    ipc_ok = true;
    send( ipcw(AQ_VERSION) );
}
isotoxin_ipc_s::~isotoxin_ipc_s()
{
    if (ipc_ok)
        junct.stop();
}

ipc::ipc_result_e isotoxin_ipc_s::wait_func( void *par )
{
    if (par)
    {
        idlejob_func f = *(idlejob_func *)par;
        if (!f()) return ipc::IPCR_BREAK;
    }
    return ipc::IPCR_OK;
}

void isotoxin_ipc_s::wait_loop( idlejob_func tickfunc )
{
    idlejob_func localtickfunc = tickfunc;
    junct.wait( processor_func, this, wait_func, tickfunc ? &tickfunc : nullptr );
}

ipc::ipc_result_e isotoxin_ipc_s::handshake_func(void *par, void *data, int datasize)
{
    isotoxin_ipc_s *me = (isotoxin_ipc_s *)par;
    ipcr r(data, datasize);
    if (ASSERT(r.header().cmd == HA_VERSION))
    {
        me->junct.set_data_callback(processor_func, par);
        me->version = r.get<int>();
        me->ipc_ok = CHECK( me->version == PLGHOST_IPC_PROTOCOL_VERSION );
        if (!me->ipc_ok)
            return ipc::IPCR_BREAK;
    }
    return ipc::IPCR_OK;
}


ipc::ipc_result_e isotoxin_ipc_s::processor_func(void *par, void *data, int datasize )
{
    if (data == nullptr) 
        return ipc::IPCR_BREAK;
    isotoxin_ipc_s *me = (isotoxin_ipc_s *)par;
    ipcr r(data,datasize);
    if (ASSERT(me->datahandler))
        if (me->datahandler( r ))
            return ipc::IPCR_OK;
    return ipc::IPCR_BREAK;
}

bool text_find_link(ts::str_c &message, int from, ts::ivec2 & rslt)
{
    int i = message.find_pos(from, CONSTASTR("http://"));
    if (i < 0) i = message.find_pos(from, CONSTASTR("https://"));
    if (i < 0) i = message.find_pos(from, CONSTASTR("ftp://"));
    if (i < 0)
    {
        int j = message.find_pos(from, CONSTASTR("www."));
        if (j == 0 || (j > 0 && message.get_char(j - 1) == ' '))
            i = j;
    }

    if (i >= 0)
    {
        int cnt = message.get_length();
        int j = i;
        for (; j < cnt; ++j)
        {
            ts::wchar c = message.get_char(j);
            if (ts::CHARz_find(L" \\<>\r\n\t", c) >= 0) break;
        }

        rslt.r0 = i;
        rslt.r1 = j;
        return true;
    }

    return false;
}



static ts::asptr bb_tags[] = { CONSTASTR("u"), CONSTASTR("i"), CONSTASTR("b"), CONSTASTR("s") };

void text_convert_from_bbcode(ts::str_c &text)
{
    ts::sstr_t<64> t1;
    ts::sstr_t<64> t2;
    for (int k = 0; k < ARRAY_SIZE(bb_tags); ++k)
    {
        if (ASSERT((bb_tags[k].l + 3) < t1.get_capacity()))
        {
            t1.clear().append_char('[').append(bb_tags[k]).append_char(']');
            t2.clear().append_char('<').append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);

            t1.clear().append(CONSTASTR("[/")).append(bb_tags[k]).append_char(']');
            t2.clear().append(CONSTASTR("</")).append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);
        }
    }
}

void text_convert_to_bbcode(ts::str_c &text)
{
    ts::sstr_t<64> t1;
    ts::sstr_t<64> t2;
    for (int k = 0; k < ARRAY_SIZE(bb_tags); ++k)
    {
        if (ASSERT((bb_tags[k].l+3) < t1.get_capacity()))
        {
            t1.clear().append_char('<').append(bb_tags[k]).append_char('>');
            t2.clear().append_char('[').append(bb_tags[k]).append_char(']');
            text.replace_all(t1,t2);

            t1.clear().append(CONSTASTR("</")).append(bb_tags[k]).append_char('>');
            t2.clear().append(CONSTASTR("[/")).append(bb_tags[k]).append_char(']');
            text.replace_all(t1,t2);
        }
    }
}

void text_close_bbcode(ts::str_c &text_utf8)
{
    ts::astrings_c opened;
    for(int i=text_utf8.find_pos('['); i >= 0 ; i = text_utf8.find_pos(i+1,'[') )
    {
        int j = text_utf8.find_pos(i+1,']');
        if (j<0) break;
        ts::pstr_c tag = text_utf8.substr(i + 1, j);
        bool close = false;
        if (tag.get_char(0) == '/') { close = true; tag = tag.substr(1); }
        for(int k=0;k<ARRAY_SIZE(bb_tags);++k)
        {
            if (tag.equals(bb_tags[k]))
            {
                if (close)
                {
                    bool removed = false;
                    for(int x = opened.size()-1; x >=0; --x)
                    {
                        if (opened.get(x).equals(tag))
                        {
                            opened.remove_fast(x);
                            removed = true;
                            i = j;
                            break;
                        }
                    }
                    if (!removed)
                    {
                        // odd closing tag
                        //text.cut(i,j-i+1);
                        text_utf8.insert(0,ts::str_c(CONSTASTR("[")).append(tag).append_char(']'));
                        i = j + tag.get_length() + 2;
                    }

                } else
                {
                    opened.add(tag);
                    i = j;
                }
                break;
            }
        }
    }
    for( const ts::str_c& t : opened )
        text_utf8.append( CONSTASTR("[/") ).append(t).append_char(']');
}
void text_convert_char_tags(ts::str_c &text)
{
    auto t = CONSTASTR("<char=");
    for (int i = text.find_pos(t); i >= 0; i = text.find_pos(i + 1, t))
    {
        int j = text.find_pos(i+t.l,'>');
        if (j < 0) break;
        ts::sstr_t<16> utf8char; utf8char.append_unicode_as_utf8( text.substr(i+t.l, j).as_int() );
        text.replace(i,j-i+1,utf8char.as_sptr());
    }
}
void text_adapt_user_input(ts::str_c &text)
{
    text.replace_all('\t', ' ');
    text.replace_all(CONSTASTR("\r"), CONSTASTR(""));

    text.replace_all('<', '\1');
    text.replace_all('>', '\2');
    text.replace_all(CONSTASTR("\1"), CONSTASTR("<char=60>"));
    text.replace_all(CONSTASTR("\2"), CONSTASTR("<char=62>"));

    text_close_bbcode(text);

    ts::sstr_t<64> t1;
    ts::sstr_t<64> t2;
    for (int k = 0; k < ARRAY_SIZE(bb_tags); ++k)
    {
        if (ASSERT((bb_tags[k].l + 3) < t1.get_capacity()))
        {
            t1.clear().append_char('[').append(bb_tags[k]).append_char(']');
            t2.clear().append_char('<').append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);

            t1.clear().append(CONSTASTR("[/")).append(bb_tags[k]).append_char(']');
            t2.clear().append(CONSTASTR("</")).append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);
        }
    }
}


void text_prepare_for_edit(ts::str_c &text)
{
    text.replace_all(CONSTASTR("<char=60>"), CONSTASTR("\2"));
    text.replace_all(CONSTASTR("<char=62>"), CONSTASTR("\3"));

    text_convert_to_bbcode(text);
    text_convert_char_tags(text);

    text.replace_all('\2', '<');
    text.replace_all('\3', '>');

}

extern "C"
{
    int isotoxin_compare(const unsigned char *row_text, const unsigned char *pattern) // sqlite likeFunc override
    {
        const ts::wstrings_c &fsplit = *(const ts::wstrings_c *)ts::pstr_c( ts::asptr((const char *)pattern+1,2 * sizeof(void *)) ).decode_pointer();

        ts::wstr_c rowtext( ts::from_utf8( ts::asptr((const char *)row_text) ) );
        rowtext.case_down();

        for(const ts::wstr_c &s : fsplit)
            if (rowtext.find_pos( s ) < 0)
                return 0;
        
        return 1;
    }
}

bool text_find_inds( const ts::wstr_c &t, ts::tmp_tbuf_t<ts::ivec2> &marks, const ts::wstrings_c &fsplit )
{
    ASSERT(fsplit.size());

    ts::tmp_tbuf_t<ts::ivec2> cc;
    ts::swstr_t<2> c(1, false);

    auto do_find = [&]()
    {
        for (const ts::wstr_c &f : fsplit)
        {
            int fl = f.get_length();
            if (fl < cc.count())
            {
                bool fail = false;
                for (int j = 0; j < fl; ++j)
                {
                    if (cc.get(j).y != f.get_char(j))
                    {
                        fail = true;
                        break;
                    }
                }
                if (!fail)
                {
                    ts::ivec2 &m = marks.add();
                    m.r0 = cc.get(0).x;
                    m.r1 = cc.get(fl).x;
                }
            }
        }
    };

    bool in_tag = false;
    int l = t.get_length();
    for (int i = 0; i < l;)
    {
        c.set_char(0, t.get_char(i));
        if (in_tag)
        {
            if (c.get_char(0) == '>')
                in_tag = false;
            ++i;
            continue;
        }
        int ci = i;
        if (c.get_char(0) == '<')
        {
            if (t.substr(i + 1).begins(CONSTWSTR("char=")))
            {
                int j = i + 6;
                for (; j<l; ++j)
                    if (t.get_char(j) == '>')
                        break;
                if (j == l) return false;
                c.set_char(0, (ts::wchar)t.substr(i + 6, j).as_int());
                i = j+1;
            }
            else
            {
                in_tag = true;
                ++i;
                continue;
            }
        } else
            ++i;

        if (cc.count() > fsplit.get(0).get_length())
            cc.remove_slow(0); // keep cc length same as longest search word

        ts::ivec2 &nc = cc.add();
        nc.x = ci;
        nc.y = c.case_down().get_char(0);

        do_find();
    }

    cc.add( ts::ivec2(l, 0) );
    while( cc.count() > 1 )
    {
        do_find();
        cc.remove_slow(0);
    }

    for(int i=marks.count()-1;i>0;--i)
    {
        const ts::ivec2 &range = marks.get(i);
        ASSERT(range.r1 > range.r0);
        for (int j = i-1; j >= 0; --j)
        {
            ts::ivec2 &range_test = marks.get(j);
            ASSERT(range_test.r1 > range_test.r0);
            ASSERT(range_test.r0 <= range.r0);

            if (range_test.r1 < range.r0)
                continue;

            if (range.r1 > range_test.r1)
                range_test.r1 = range.r1;
            
            marks.remove_fast(i);
            break;
        }
    }

    marks.tsort<ts::ivec2>( []( const ts::ivec2 *s1, const ts::ivec2 *s2 ) { return s1->x > s2->x; } );

    return marks.count() > 0;
}

ts::wstr_c loc_text(loctext_e lt)
{
    switch (lt)
    {
        case loc_connection_failed:
            return TTT("Connection failed", 302);
        case loc_autostart:
            return TTT("Start [appname] with system", 310);
        case loc_please_create_profile:
            return TTT("Please, create profile first", 144);
        case loc_yes:
            return TTT("yes",315);
        case loc_no:
            return TTT("no",316);
        case loc_exit:
            return TTT("Exit",3);
        case loc_network:
            return TTT("Network",66);
        case loc_nonetwork:
            return TTT("No available networks to select",248);
        case loc_disk_write_error:
            return TTT("Disk write error (full?)",142);
        case loc_import_from_file:
            return TTT("Import configuration from file",56);
        case loc_loading:
            return TTT("Loading",345);
        case loc_anyfiles:
            return TTT("Any files",207);
        case loc_camerabusy:
            return TTT("Probably camera already used.[br]You have to stop using camera by another application.", 353);
        case loc_initialization:
            return TTT("Initialization...", 352);

    }
    return ts::wstr_c();
}

ts::wstr_c text_typing(const ts::wstr_c &prev, ts::wstr_c &workbuf, const ts::wsptr &preffix)
{
    ts::wstr_c tt(TTT("Typing", 271));
    ts::wstr_c ttc = prev;

    ts::wsptr rectb = CONSTWSTR("_");

    if (workbuf.is_empty())
    {
        ttc.clear();
        workbuf.set(CONSTWSTR("__  __  __  __"));
    }


    if (ttc.is_empty()) ttc.set(preffix).append(rectb);
    else
    {
        if (ttc.get_length() - preffix.l - rectb.l < tt.get_length())
        {
            ttc.set(preffix).append(tt.substr(0, ttc.get_length() - preffix.l - rectb.l + 1)).append(rectb);
        }
        else
        {
            ttc.trunc_length(rectb.l).append_char(workbuf.get_last_char()).append(rectb.skip(1));
            workbuf.trunc_length();
        }
    }
    return ttc;
}

void draw_initialization(rectengine_c *e, ts::drawable_bitmap_c &pab, const ts::irect&viewrect, ts::TSCOLOR tcol, const ts::wsptr &additiontext)
{
    draw_data_s&dd = e->begin_draw();

    ts::wstr_c t( additiontext );
    ts::ivec2 tsz = gui->textsize( ts::g_default_text_font, t );
    ts::ivec2 tpos = (viewrect.size() - tsz) / 2;

    ts::ivec2 processpos(tpos.x - pab.info().sz.x - 5, (viewrect.height() - pab.info().sz.y) / 2);
    e->draw(processpos + viewrect.lt, pab, ts::irect(0, pab.info().sz), true);


    text_draw_params_s tdp;
    tdp.forecolor = &tcol;

    dd.offset += tpos + viewrect.lt;
    dd.size = tsz;

    e->draw(t, tdp);

    e->end_draw();
}


void add_status_items(menu_c &m)
{
    struct handlers
    {
        static void m_ost(const ts::str_c&ost)
        {
            g_app->set_status( (contact_online_state_e)ost.as_uint(), true );
        }
    };

    contact_online_state_e ost = contacts().get_self().get_ostate();

    m.add(TTT("Online", 244), COS_ONLINE == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_ONLINE));
    m.add(TTT("Away", 245), COS_AWAY == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_AWAY));
    m.add(TTT("Busy", 246), COS_DND == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_DND));

}



SLANGID detect_language()
{
    ts::wstrings_c fns;
    ts::g_fileop->find(fns, CONSTWSTR("loc/*.lng*.lng"), false);


    ts::wchar x[32];
    GetLocaleInfoW( NULL, LOCALE_SISO639LANGNAME, x, 32 );

    for (const ts::wstr_c &f : fns)
        if ( f.substr(0, 2).equals( ts::wsptr(x,2) ) ) return SLANGID( ts::to_str(ts::wsptr(x,2)) );

    return SLANGID("en");
}

menu_c list_langs( SLANGID curlang, MENUHANDLER h )
{
    menu_c m;

    ts::wstrings_c fns;
    ts::wstr_c path(CONSTWSTR("loc/"));
    int cl = path.get_length();

    ts::g_fileop->find(fns, path.append(CONSTWSTR("*.lng*.lng")), false);
    fns.kill_dups();

    struct lang_s
    {
        SLANGID langtag;
        ts::wstr_c name;
        int operator()(const lang_s &ol) const
        {
            return ts::wstr_c::compare(name, ol.name);
        }
    };

    ts::tmp_array_inplace_t<lang_s, 1> langs;

    ts::wstrings_c ps;
    ts::wstr_c curdef;
    for (const ts::wstr_c &f : fns)
    {
        lang_s &lng = langs.add();
        ts::swstr_t<4> wlng = f.substr(0, 2);
        lng.langtag = to_str(wlng);
        if (lng.langtag.equals(curlang))
            curdef = f;

        path.set_length(cl).append(f);
        ts::parse_text_file(path, ps, true);
        for (const ts::wstr_c &ls : ps)
        {
            ts::token<ts::wchar> t(ls, '=');
            if (t->equals(wlng))
            {
                ++t;
                ts::wstr_c ln(*t); ln.trim();
                lng.name = ln;
                break;
            }
        }
    }

    // and update names by current language
    path.set_length(cl).append(curdef);
    ts::parse_text_file(path, ps);
    for (const ts::wstr_c &ls : ps)
    {
        ts::token<ts::wchar> t(ls, '=');

        for (lang_s &l : langs)
            if (l.langtag.equals(to_str(*t)))
            {
                ++t;
                ts::wstr_c ln(*t); ln.trim();
                l.name = ln;
                break;
            }
    }

    langs.sort();

    for (const lang_s &l : langs)
    {
        bool cur = curlang == l.langtag;
        m.add(l.name, cur ? (MIF_MARKED) : 0, h, l.langtag);
    }

    return m;
}

ts::wstr_c make_proto_desc( int mask )
{
    ts::wstr_c r(1024,true);
    if (0 != (mask & MPD_UNAME))    r.append(TTT("Your name",259)).append(CONSTWSTR(": <l>{uname}</l><br>"));
    if (0 != (mask & MPD_USTATUS))  r.append(TTT("Your status",260)).append(CONSTWSTR(": <l>{ustatus}</l><br>"));
    if (0 != (mask & MPD_NAME))     r.append(TTT("Connection name",102)).append(CONSTWSTR(": <l>{name}</l><br>"));
    if (0 != (mask & MPD_MODULE))   r.append(TTT("Module",105)).append(CONSTWSTR(": <l>{module}</l><br>"));
    if (0 != (mask & MPD_ID))       r.append(TTT("ID",103)).append(CONSTWSTR(": <l>{id}</l><br>"));
    if (0 != (mask & MPD_STATE))    r.append(TTT("State",104)).append(CONSTWSTR(": <l>{state}</l><br>"));

    if (r.ends(CONSTWSTR("<br>"))) r.trunc_length(4);
    return r;
}


bool new_version(const ts::asptr &current, const ts::asptr &newver)
{
    if (current.l == 0)
        return newver.l > 0;
    ts::token<char> cver(current, '.');
    ts::token<char> lver(newver, '.');
    for (; cver; ++cver)
    {
        int ncver = cver->as_uint();
        int nlver = lver ? lver->as_uint() : 0;
        if (ncver > nlver) return false;
        if (ncver < nlver) return true;
        if (lver) ++lver;
    }
    return false;
}

bool new_version()
{
    return new_version( application_c::appver(), cfg().autoupdate_newver() );
}

ts::drawable_bitmap_c * prepare_proto_icon( const ts::asptr &prototag, const void *icodata, int icodatasz, int imgsize, icon_type_e icot )
{
    ts::drawable_bitmap_c * ricon = nullptr;
    if (icodatasz)
    {
        ts::bitmap_c bmp;
        if (bmp.load_from_file(icodata, icodatasz))
        {
            int input_images_size = bmp.info().sz.x;
            bool do_online_offline_effects = false;
            ts::bmpcore_exbody_s srcb = bmp.extbody();

            int numimages = bmp.info().sz.y / input_images_size; // vertical tiles
            if (numimages > 1)
            {
                if (icot < numimages)
                    srcb.m_body = srcb.m_body + srcb.info().pitch * (input_images_size * icot);
                else
                    do_online_offline_effects = true;
                srcb.m_info.sz.y = input_images_size;

            } else
                do_online_offline_effects = true;
            if (srcb.m_info.sz.y > input_images_size)
                srcb.m_info.sz.y = input_images_size;


            if (input_images_size != imgsize || numimages == 0)
            {
                ts::bitmap_c bmpsz;
                bmpsz.create_RGBA( ts::ivec2(imgsize) );
                bmpsz.resize_from(srcb, ts::FILTER_LANCZOS3);
                bmp = bmpsz;
            } else if (input_images_size != bmp.info().sz.y)
            {
                ASSERT( srcb.info().sz == ts::ivec2(imgsize) );
                ts::bitmap_c bmpx;
                bmpx.create_RGBA( ts::ivec2(imgsize) );
                bmpx.copy(ts::ivec2(0), bmpx.info().sz, srcb, ts::ivec2(0));
                bmp = bmpx;
            }

            if ( do_online_offline_effects )
            {
                switch (icot) //-V719
                {
                case IT_ONLINE:
                    {
                        auto make_online = [](ts::uint8 * me, const ts::image_extbody_c::FMATRIX &m)
                        {
                            // make it green
                            if (me[3] == 255)
                                *(ts::TSCOLOR *)me = ts::ARGB(112,255,80);
                        };

                        bmp.apply_filter(ts::ivec2(0), bmp.info().sz, make_online);
                    }
                    break;
                case IT_OFFLINE:
                    {
                        auto make_offline = [](ts::uint8 * me, const ts::image_extbody_c::FMATRIX &m)
                        {
                            // make it gray and semitransparent
                            *(ts::TSCOLOR *)me = ts::MULTIPLY( ts::GRAYSCALE( *(ts::TSCOLOR *)me ), ts::ARGB(255,255,255,128) );
                        };

                        bmp.apply_filter(ts::ivec2(0), bmp.info().sz, make_offline);
                    }
                    break;
                case IT_AWAY:
                    {
                        auto make_online = [](ts::uint8 * me, const ts::image_extbody_c::FMATRIX &m)
                        {
                            // make it yellow
                            if (me[3] == 255)
                                *(ts::TSCOLOR *)me = ts::ARGB(207, 205, 0);
                        };

                        bmp.apply_filter(ts::ivec2(0), bmp.info().sz, make_online);
                    }
                    break;
                case IT_DND:
                    {
                        auto make_online = [](ts::uint8 * me, const ts::image_extbody_c::FMATRIX &m)
                        {
                            // make it dark red
                            if (me[3] == 255)
                                *(ts::TSCOLOR *)me = ts::ARGB(141, 36, 0);
                        };

                        bmp.apply_filter(ts::ivec2(0), bmp.info().sz, make_online);
                    }
                    break;
                }
            }

            ricon = TSNEW(ts::drawable_bitmap_c, bmp, false, true);
        }
    }
    if (!ricon)
    {
        ts::text_rect_static_c tr;
        tr.set_size(ts::ivec2(64,32));

        ts::wstr_c t = ts::to_wstr(prototag);
        t.case_up();
        switch (icot)
        {
        case IT_NORMAL:
            t.insert(0, CONSTWSTR("<l><outline=#000088>"));
            break;
        case IT_ONLINE:
            t.insert(0, CONSTWSTR("<l><outline=#70ff50>"));
            break;
        case IT_OFFLINE:
            t.insert(0, CONSTWSTR("<l><outline=#000000>"));
            break;
        case IT_AWAY:
            t.insert(0, CONSTWSTR("<l><outline=#cfcd00>"));
            break;
        case IT_DND:
            t.insert(0, CONSTWSTR("<l><outline=#8d2400>"));
            break;
        }

        tr.set_text(t, nullptr, false);
        tr.set_def_color(ts::ARGB(255, 255, 255));
        tr.set_font(&ts::g_default_text_font);
        tr.parse_and_render_texture(nullptr, nullptr, false);
        tr.set_size(tr.lastdrawsize);
        tr.parse_and_render_texture(nullptr, nullptr, true);

        ts::bitmap_c bmp;
        bmp.create_RGBA(tr.size);
        bmp.copy(ts::ivec2(0), tr.size, tr.get_texture().extbody(), ts::ivec2(0));

        if (bmp.info().sz != ts::ivec2(imgsize))
        {
            ts::bitmap_c bmpsz;
            bmp.resize_to(bmpsz, ts::ivec2(imgsize), ts::FILTER_LANCZOS3);
            bmp = bmpsz;
        }

        ricon = TSNEW(ts::drawable_bitmap_c, bmp, false, true);
    }
    return ricon;
}

bool file_mask_match( const ts::wsptr &filename, const ts::wsptr &masks )
{
    if (masks.l == 0) return false;
    ts::wstr_c fn(filename);
    ts::fix_path(fn, FNO_LOWERCASEAUTO|FNO_NORMALIZE);
    

    for(ts::token<ts::wchar> t(masks, ';');t;++t)
    {
        ts::wstr_c fnmask( *t );
        ts::fix_path(fnmask, FNO_LOWERCASEAUTO|FNO_NORMALIZE);
        fnmask.trim();
        ts::wsptr fnm = fnmask.as_sptr();
        if (fnmask.get_char(0) == '\"')
        {
            ++fnm;
            --fnm.l;
        }
        if (ts::fn_mask_match(fn, fnm)) return true;
    }

    return false;
}

sound_capture_handler_c::sound_capture_handler_c()
{
    if (g_app) g_app->register_capture_handler(this);
}
sound_capture_handler_c::~sound_capture_handler_c()
{
    if (g_app) g_app->unregister_capture_handler(this);
}

void sound_capture_handler_c::start_capture()
{
    capture = true;
    g_app->start_capture(this);
}
void sound_capture_handler_c::stop_capture()
{
    capture = false;
    g_app->stop_capture(this);
}

namespace
{
    enum enm_reg_e
    {
        ER_CONTINUE = 0,
        ER_BREAK = 1,
        ER_DELETE_AND_CONTINUE = 2,
    };
}

template< typename ENM > void enum_reg( const ts::wsptr &regpath_, ENM e )
{
    HKEY okey = HKEY_CURRENT_USER;

    ts::wsptr regpath = regpath_;

    if (regpath.l > 5)
    {
        if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKCR\\"))) { okey = HKEY_CLASSES_ROOT; regpath = regpath_.skip(5); }
        else if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKCU\\"))) { okey = HKEY_CURRENT_USER; regpath = regpath_.skip(5); }
        else if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKLM\\"))) { okey = HKEY_LOCAL_MACHINE; regpath = regpath_.skip(5); }
    }

    HKEY k;
    if (RegOpenKeyExW(okey, ts::tmp_wstr_c(regpath), 0, KEY_READ, &k) != ERROR_SUCCESS) return;

    ts::wstrings_c to_delete;

    DWORD t, len = 1024;
    ts::wchar buf[1024];
    for (int index = 0; ERROR_SUCCESS == RegEnumValueW(k, index, buf, &len, nullptr, &t, nullptr, nullptr); ++index, len = 1024)
        if (REG_SZ == t)
        {
            ts::tmp_wstr_c kn(ts::wsptr(buf, len));
            DWORD lt = REG_BINARY;
            DWORD sz = 1024;
            int rz = RegQueryValueExW(k, kn, 0, &lt, (LPBYTE)buf, &sz);
            if (rz != ERROR_SUCCESS) continue;

            ts::wsptr b(buf, sz/sizeof(ts::wchar)-1);

            enm_reg_e r = e(b);
            if (r == ER_DELETE_AND_CONTINUE)
            {
                to_delete.add(kn);
                continue;
            }
            if (r == ER_CONTINUE) continue;

            break;
        }


    RegCloseKey(k);

    if (to_delete.size())
    {
        if (RegOpenKeyExW(okey, ts::tmp_wstr_c(regpath), 0, KEY_READ|KEY_WRITE, &k) != ERROR_SUCCESS) return;

        for (const ts::wstr_c &kns : to_delete)
            RegDeleteValueW(k, kns);

        RegCloseKey(k);
    }

}

bool check_reg_writte_access(const ts::wsptr &regpath_)
{
    HKEY okey = HKEY_CURRENT_USER;

    ts::wsptr regpath = regpath_;

    if (regpath.l > 5)
    {
        if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKCR\\"))) { okey = HKEY_CLASSES_ROOT; regpath = regpath_.skip(5); }
        else if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKCU\\"))) { okey = HKEY_CURRENT_USER; regpath = regpath_.skip(5); }
        else if (ts::pwstr_c(regpath).begins(CONSTWSTR("HKLM\\"))) { okey = HKEY_LOCAL_MACHINE; regpath = regpath_.skip(5); }
    }

    //RegGetKeySecurity() - too complex to use; also it requres Advapi32.lib
    //now way! just try open key for write

    HKEY k;
    if (RegOpenKeyExW(okey, ts::tmp_wstr_c(regpath), 0, KEY_WRITE, &k) != ERROR_SUCCESS) return false;
    RegCloseKey(k);
    return true;

}

int detect_autostart(ts::str_c &cmdpar)
{
    ts::wstrings_c lks;
    ts::find_files(ts::fn_join(ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%STARTUP%")), FNO_FULLPATH | FNO_PARSENENV), CONSTWSTR("*.lnk")), lks, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY, true);
    ts::find_files(ts::fn_join(ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%COMMONSTARTUP%")), FNO_FULLPATH | FNO_PARSENENV), CONSTWSTR("*.lnk")), lks, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY, true);

    ts::wstr_c exepath = ts::get_exe_full_name();
    ts::fix_path(exepath, FNO_NORMALIZE);

    ts::tmp_buf_c lnk;
    for(const ts::wstr_c &lnkf : lks)
    {
        lnk.load_from_disk_file(lnkf);
        ts::lnk_s lnkreader( lnk.data(), lnk.size() );
        if (lnkreader.read())
        {
            ts::fix_path(lnkreader.local_path, FNO_NORMALIZE);
            if (exepath.equals_ignore_case(lnkreader.local_path))
            {
                cmdpar = ts::to_str(lnkreader.command_line_arguments);

                if (ts::check_write_access( ts::fn_get_path(lnkf) ))
                    return DETECT_AUSTOSTART;

                return DETECT_AUSTOSTART | DETECT_READONLY;
            }
        }
    }

    ts::wsptr regpaths2check[] = 
    {
        CONSTWSTR("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        CONSTWSTR("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
        CONSTWSTR("HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run"),
    };

    ts::wstrings_c cmd;
    for (const ts::wsptr & path : regpaths2check)
    {
        int detect_stuff = 0;

        enum_reg(path, [&](const ts::wsptr& regvalue)->enm_reg_e
        {
            cmd.qsplit( regvalue );
            
            if (cmd.size())
                ts::fix_path(cmd.get(0), FNO_NORMALIZE);

            if (cmd.size() && exepath.equals_ignore_case(cmd.get(0)))
            {
                detect_stuff = DETECT_AUSTOSTART;
                if (!check_reg_writte_access(path))
                    detect_stuff |= DETECT_READONLY;

                if (cmd.size() == 2)
                    cmdpar = ts::to_str(cmd.get(1));

                return ER_BREAK;
            }

            return ER_CONTINUE;
        });

        if (detect_stuff) return detect_stuff;
    }

    return 0;
}
void autostart(const ts::wstr_c &exepath, const ts::wsptr &cmdpar)
{
    ts::wstr_c my_exe = exepath;
    if (my_exe.is_empty())
        my_exe = ts::get_exe_full_name();
    ts::fix_path(my_exe, FNO_NORMALIZE);

    // 1st of all - delete lnk file

    ts::wstrings_c lks;
    ts::find_files(ts::fn_join(ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%STARTUP%")), FNO_FULLPATH | FNO_PARSENENV), CONSTWSTR("*.lnk")), lks, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY, true);
    ts::find_files(ts::fn_join(ts::fn_fix_path(ts::wstr_c(CONSTWSTR("%COMMONSTARTUP%")), FNO_FULLPATH | FNO_PARSENENV), CONSTWSTR("*.lnk")), lks, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY, true);

    ts::tmp_buf_c lnk;
    for (const ts::wstr_c &lnkf : lks)
    {
        lnk.load_from_disk_file(lnkf);
        ts::lnk_s lnkreader(lnk.data(), lnk.size());
        if (lnkreader.read())
            if (my_exe.equals_ignore_case(lnkreader.local_path))
                DeleteFileW(lnkf);
    }

    ts::wsptr regpaths2check[] =
    {
        CONSTWSTR("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        CONSTWSTR("HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
        CONSTWSTR("HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run"),
    };

    // delete all reg records

    ts::wstrings_c cmd;
    for (const ts::wsptr & path : regpaths2check)
    {
        enum_reg(path, [&](const ts::wsptr& regvalue)->enm_reg_e
        {
            cmd.qsplit(regvalue);
            if (cmd.size())
                ts::fix_path(cmd.get(0), FNO_NORMALIZE);

            if (cmd.size() && my_exe.equals_ignore_case(cmd.get(0)))
                return ER_DELETE_AND_CONTINUE;

            return ER_CONTINUE;
        });
    }

    if (!exepath.is_empty())
    {
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, ts::tmp_wstr_c(regpaths2check[0].skip(5)), 0, KEY_WRITE, &k) != ERROR_SUCCESS) return;

        ts::wstr_c path(CONSTWSTR("\"")); path.append(exepath);
        if (cmdpar.l) path.append(CONSTWSTR("\" ")).append(to_wstr(cmdpar));
        else path.append_char('\"');

        RegSetValueEx(k, L"Isotoxin", 0, REG_SZ, (const BYTE *)path.cstr(), (path.get_length() + 1) * sizeof(ts::wchar));
        RegCloseKey(k);
    }

}

bool elevate()
{
    ts::wstr_c prm(CONSTWSTR("wait ")); prm.append_as_uint(GetCurrentProcessId());
    ts::wstr_c exe(ts::get_exe_full_name());

    SHELLEXECUTEINFOW shExInfo = { 0 };
    shExInfo.cbSize = sizeof(shExInfo);
    shExInfo.fMask = 0;
    shExInfo.hwnd = 0;
    shExInfo.lpVerb = L"runas";
    shExInfo.lpFile = exe;
    shExInfo.lpParameters = prm;
    shExInfo.lpDirectory = 0;
    shExInfo.nShow = SW_NORMAL;
    shExInfo.hInstApp = 0;

    if (ShellExecuteExW(&shExInfo))
    {
        return true;
    }
    return false;
}

static void ChuckNorrisCopy(const ts::wstr_c &copyto)
{
    ts::wstr_c prm(CONSTWSTR("installto ")); prm.append(copyto).replace_all(10, ' ', '*');
    ts::wstr_c exe( ts::get_exe_full_name() );

    SHELLEXECUTEINFOW shExInfo = { 0 };
    shExInfo.cbSize = sizeof(shExInfo);
    shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shExInfo.hwnd = 0;
    shExInfo.lpVerb = L"runas";
    shExInfo.lpFile = exe;
    shExInfo.lpParameters = prm;
    shExInfo.lpDirectory = 0;
    shExInfo.nShow = SW_NORMAL;
    shExInfo.hInstApp = 0;

    if (ShellExecuteExW(&shExInfo))
    {
        WaitForSingleObject(shExInfo.hProcess, INFINITE);
        CloseHandle(shExInfo.hProcess);
    }
}

void install_to(const ts::wstr_c &path, bool acquire_admin_if_need)
{
    if (!ts::dir_present(path))
        if (!make_path(path, 0))
        {
            if (acquire_admin_if_need)
                ChuckNorrisCopy(path);
            return;
        }

    ts::wstr_c path_from = ts::fn_get_path( ts::get_exe_full_name() );

    ts::wstrings_c files;
    ts::find_files( ts::fn_join(path_from, CONSTWSTR("*.*")), files, 0xFFFFFFFF, FILE_ATTRIBUTE_DIRECTORY, true);

    for(const ts::wstr_c &fn2c : files)
    {
        if (fn2c.ends_ignore_case(CONSTWSTR(".exe")) || fn2c.ends_ignore_case(CONSTWSTR(".dll")) || fn2c.ends_ignore_case(CONSTWSTR(".data")))
            if (0 == CopyFileW(fn2c, ts::fn_join(path, ts::fn_get_name_with_ext(fn2c)), false))
            {
                if (acquire_admin_if_need && ERROR_ACCESS_DENIED == GetLastError())
                    ChuckNorrisCopy(path);
                return;
            }
    }
}


namespace
{
    struct load_proto_list_s : public ts::task_c
    {
        ts::iweak_ptr<available_protocols_s> prots2load;
        load_proto_list_s(available_protocols_s *prots) :prots2load(prots) {}
        ~load_proto_list_s() {}

        int result_x = 1;
        available_protocols_s available_prots;

        bool ipchandler(ipcr r)
        {
            if (r.d == nullptr)
            {
                // lost contact to plghost
                // just close settings
                if (prots2load)
                    prots2load->done(true);

                result_x = R_CANCEL;
                return false;
            }
            else
            {
                switch (r.header().cmd)
                {
                    case HA_PROTOCOLS_LIST:
                    {
                        int n = r.get<int>();
                        while (--n >= 0)
                        {
                            protocol_description_s &proto = available_prots.add();
                            r.getwstr(); // dll name / unused
                            proto.tag = r.getastr();
                            proto.description = r.getastr();
                            proto.description_t = r.getastr();
                            proto.version = r.getastr();
                            proto.features = r.get<int>();
                            proto.connection_features = r.get<int>();

                            int icosz;
                            const void *icodata = r.get_data(icosz);
                            proto.icon.reset(prepare_proto_icon(proto.tag, icodata, icosz, PROTO_ICON_SIZE, IT_NORMAL));
                        }

                        result_x = R_DONE;
                    }
                    return false;
                }
            }

            return true;
        }


        /*virtual*/ int iterate(int pass) override
        {
            isotoxin_ipc_s ipcj(ts::str_c(CONSTASTR("get_protocols_list_")).append_as_uint(GetCurrentThreadId()), DELEGATE(this, ipchandler));
            ipcj.send(ipcw(AQ_GET_PROTOCOLS_LIST));
            ipcj.wait_loop(nullptr);
            ASSERT(result_x != 1);
            return result_x;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (!canceled && prots2load)
            {
                *prots2load = std::move( available_prots );
                prots2load->done(false);
            }

            __super::done(canceled);
        }
    };
}

void available_protocols_s::load()
{
    g_app->add_task(TSNEW(load_proto_list_s, this));
}


// dlmalloc -----------------

#pragma warning (disable:4559)
#pragma warning (disable:4127)
#pragma warning (disable:4057)
#pragma warning (disable:4702)

#define MALLOC_ALIGNMENT ((size_t)16U)
#define USE_DL_PREFIX
#define USE_LOCKS 0

static long dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

extern "C"
{
#include "dlmalloc/dlmalloc.c"
}
