#include "isotoxin.h"


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
    path.replace_all(CONSTWSTR("%CONFIG%"), ts::fn_get_path(cfg().get_path()).trunc_char('\\'));

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
        HOLD r(owner->getparent());
        ts::irect cr = r().get_client_area();
        MODIFY(*owner).pos(cr.lt).size(cr.width(), height);
        return false;
    }
    return false;
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
        HOLD r(owner->getparent());
        ts::irect cr = r().get_client_area();
        int xspace = x_space;
        if (xspace < 0)
        {
            int rqw = (width * num) + (-xspace * (num+1));
            xspace = (cr.width() - rqw) / 2;
        }
        cr.lt.x += xspace;
        cr.rb.x -= xspace;
        cr.rb.y -= y_space;

        //float w = (float)cr.width() / (float)num;
        //int x = lround(w * index + (w-width)/2);

        float fx = (float)(cr.width() - (width * num)) / (float)(num + 1);
        int x = xspace + lround(fx + (width + fx) * index);


        MODIFY(*owner).pos(x,cr.rb.y-height).size(width, height);
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

bool leech_at_left::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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
                ts::str_c v;
                for (float t : data.float_array())
                {
                    v.append_as_int(ts::lround(t * 10000)).append_char(',');
                }
                v.trunc_length();
                cfg().param(cfgname, v);
            }
            break;
        case SQ_GROUP_RESTORE_PROPORTIONS:
            {
                gui_hgroup_c *g = ts::ptr_cast<gui_hgroup_c *>(owner);
                g->set_proportions( cfg().get(cfgname, defaultv.as_sptr()), 10000 );

            }
            break;
        }
    
    }
    
    return false;
}











isotoxin_ipc_s::isotoxin_ipc_s(const ts::str_c &tag, datahandler_func datahandler):tag(tag), datahandler(datahandler)
{
    int memba = junct.start(tag);
    if (memba != 0) 
    {
        return;
    }
    if (!ts::start_app(CONSTWSTR("plghost ") + ts::tmp_wstr_c(tag), &plughost))
        return;
    junct.set_data_callback(handshake_func, this);
    if (!junct.wait_partner(10000))
        return;
    ipc_ok = true;
    send( ipcw(AQ_VERSION) );
}
isotoxin_ipc_s::~isotoxin_ipc_s()
{
    junct.stop();
}

ipc::ipc_result_e isotoxin_ipc_s::wait_func( void *par )
{
    idlejob_func f = *(idlejob_func *)par;
    if (!f()) return ipc::IPCR_BREAK;
    return ipc::IPCR_OK;
}

void isotoxin_ipc_s::wait_loop( idlejob_func tickfunc )
{
    idlejob_func localtickfunc = tickfunc;
    junct.wait( processor_func, this, wait_func, &tickfunc );
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


static ts::wsptr bb_tags[] = { CONSTWSTR("u"), CONSTWSTR("i"), CONSTWSTR("b"), CONSTWSTR("s") };


void text_convert_to_bbcode(ts::wstr_c &text)
{
    ts::swstr_t<64> t1;
    ts::swstr_t<64> t2;
    for (int k = 0; k < ARRAY_SIZE(bb_tags); ++k)
    {
        if (ASSERT((bb_tags[k].l+3) < t1.get_capacity()))
        {
            t1.clear().append_char('<').append(bb_tags[k]).append_char('>');
            t2.clear().append_char('[').append(bb_tags[k]).append_char(']');
            text.replace_all(t1,t2);

            t1.clear().append(CONSTWSTR("</")).append(bb_tags[k]).append_char('>');
            t2.clear().append(CONSTWSTR("[/")).append(bb_tags[k]).append_char(']');
            text.replace_all(t1,t2);
        }
    }
}
void text_close_bbcode(ts::wstr_c &text)
{
    ts::wstrings_c opened;
    for(int i=text.find_pos('['); i >= 0 ; i = text.find_pos(i+1,'[') )
    {
        int j = text.find_pos(i+1,']');
        if (j<0) break;
        ts::pwstr_c tag = text.substr(i + 1, j);
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
                        // лишний закрывающий тэг
                        //text.cut(i,j-i+1);
                        text.insert(0,ts::wstr_c(CONSTWSTR("[")).append(tag).append_char(']'));
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
    for( const ts::wstr_c& t : opened )
        text.append( CONSTWSTR("[/") ).append(t).append_char(']');
}
void text_convert_char_tags(ts::wstr_c &text)
{
    auto t = CONSTWSTR("<char=");
    for (int i = text.find_pos(t); i >= 0; i = text.find_pos(i + 1, t))
    {
        int j = text.find_pos(i+t.l,'>');
        if (j < 0) break;
        ts::wchar charcode = (ts::wchar)text.substr(i+t.l, j).as_int();
        text.replace(i,j-i+1,ts::wsptr(&charcode,1));
    }
}
void text_adapt_user_input(ts::wstr_c &text)
{
    text.replace_all('<', '\1');
    text.replace_all('>', '\2');
    text.replace_all(CONSTWSTR("\1"), CONSTWSTR("<char=60>"));
    text.replace_all(CONSTWSTR("\2"), CONSTWSTR("<char=62>"));

    text_close_bbcode(text);

    ts::swstr_t<64> t1;
    ts::swstr_t<64> t2;
    for (int k = 0; k < ARRAY_SIZE(bb_tags); ++k)
    {
        if (ASSERT((bb_tags[k].l + 3) < t1.get_capacity()))
        {
            t1.clear().append_char('[').append(bb_tags[k]).append_char(']');
            t2.clear().append_char('<').append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);

            t1.clear().append(CONSTWSTR("[/")).append(bb_tags[k]).append_char(']');
            t2.clear().append(CONSTWSTR("</")).append(bb_tags[k]).append_char('>');
            text.replace_all(t1, t2);
        }
    }
}


void text_prepare_for_edit(ts::wstr_c &text)
{
    text.replace_all(CONSTWSTR("<char=60>"), CONSTWSTR("\2"));
    text.replace_all(CONSTWSTR("<char=62>"), CONSTWSTR("\3"));

    text_convert_to_bbcode(text);
    text_convert_char_tags(text);

    text.replace_all('\2', '<');
    text.replace_all('\3', '>');

}


SLANGID detect_language()
{
    ts::wstrings_c fns;
    ts::g_fileop->find(fns, CONSTWSTR("loc/*.lng*.lng"));


    ts::wchar x[32];
    GetLocaleInfoW( LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, x, 32 );

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

    ts::g_fileop->find(fns, path.append(CONSTWSTR("*.lng*.lng")));

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
    ts::str_c curdef;
    for (const ts::str_c &f : fns)
    {
        lang_s &lng = langs.add();
        ts::swstr_t<4> wlng = f.substr(0, 2);
        lng.langtag = wlng;
        if (lng.langtag.equals(curlang))
        {
            curdef = f;
        }

        path.set_length(cl).append(f);
        ts::parse_text_file(path, ps);
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
            if (l.langtag.equals(*t))
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


sound_capture_handler_c::sound_capture_handler_c()
{
    g_app->register_capture_handler(this);
}
sound_capture_handler_c::~sound_capture_handler_c()
{
    g_app->unregister_capture_handler(this);
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
