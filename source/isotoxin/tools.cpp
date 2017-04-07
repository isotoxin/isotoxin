#include "isotoxin.h"

#ifdef _WIN32
#pragma warning (push)
#pragma warning (disable:4324)
#include "libsodium/src/libsodium/include/sodium.h"
#pragma warning (pop)
#endif
#ifdef _NIX
#include <sodium.h>
#endif // _NIX

const wraptranslate<ts::wsptr> __translation(const ts::wsptr &txt, int tag)
{
    const ts::wsptr r = g_app->label(tag);
    return wraptranslate<ts::wsptr>(r.l==0 ? txt : r);
}

bool check_profile_name(const ts::wstr_c &t, bool )
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


void path_expand_env(ts::wstr_c &path, const contact_root_c *r)
{
    ts::parse_env(path);
    //ts::wstr_c cfgfolder = cfg().get_path();
    path.replace_all(CONSTWSTR("{ConfigPath}"), ts::fn_get_path(cfg().get_path()).trunc_char(NATIVE_SLASH));
    path.replace_all_lazy(CONSTWSTR("{DownloadPath}"), [r]() { return prf().download_folder_prepared(r); });
    path.replace_all_lazy(CONSTWSTR("{ContactId}"), [r]() { if (r == nullptr) return ts::wstr_c(CONSTWSTR("0")); return ts::wmake<uint>(r->getkey().contactid); });
    path.replace_all_lazy(CONSTWSTR("{ContactName}"), [r]() { if (r == nullptr) return ts::wstr_c(CONSTWSTR("unknown")); return fn_fix_path(r->showname(), FNO_MAKECORRECTNAME); });
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
    int x = xx + xspace + ts::lround(fx + (width + fx) * index);

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
        ts::ivec2 szmax = r().get_max_size();
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

void leech_dock_bottom_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    cr.lt.x += x_space;
    cr.rb.x -= x_space;
    cr.rb.y -= y_space;

    MODIFY(*owner).pos(cr.lt.x, cr.rb.y - height).size(cr.width(), height);
}

/*virtual*/ bool leech_dock_bottom_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r(owner->getparent());
        ts::ivec2 szmin = r().get_min_size();
        ts::ivec2 szmax = r().get_max_size();
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
    int x = xx + xspace + ts::lround(fx + (width + fx) * index);

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
    int y = ts::lround(fy + (height + fy) * index);

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

void leech_dock_left_center_s::update_ctl_pos()
{
    HOLD r( owner->getparent() );
    ts::irect cr = r().get_client_area();
    int yspace = y_space;
    if (yspace < 0)
    {
        int rqh = (height * num) + (-yspace * (num + 1));
        yspace = (cr.height() - rqh) / 2;
    }
    cr.lt.y += yspace;
    cr.lt.x += x_space;
    cr.rb.y -= yspace;

    float fy = (float)(cr.height() - (height * num)) / (float)(num + 1);
    int y = ts::lround( fy + (height + fy) * index );

    MODIFY( *owner ).pos( cr.lt.x, cr.lt.y + y ).size( width, height );
}

/*virtual*/ bool leech_dock_left_center_s::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    if (!ASSERT( owner )) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        HOLD r( owner->getparent() );
        ts::ivec2 szmin( width + x_space, height * num + y_space * 2 );
        ts::ivec2 szmax = HOLD( owner->getparent() )().get_max_size();
        r().calc_min_max_by_client_area( szmin, szmax );
        fixrect( data.rectchg.rect, szmin, szmax, data.rectchg.area );
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}

void leech_dock_top_right_s::update_ctl_pos()
{
    HOLD r(owner->getparent());
    ts::irect cr = r().get_client_area();
    MODIFY(*owner).pos(ts::ivec2(cr.rb.x - x_space - width, cr.lt.y + y_space)).size(width, height);
}

/*virtual*/ bool leech_dock_top_right_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

ts::irect leech_at_left_s::calcrect() const
{
    ts::irect cr = of->getprops().rect();
    ts::ivec2 szo = owner->getprops().size();
    if (szo == ts::ivec2(0)) szo = owner->get_min_size();
    return ts::irect::from_lt_and_size(ts::ivec2(cr.lt.x - space - szo.x, cr.lt.y + (cr.height() - szo.y) / 2), szo);
}

bool leech_at_left_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (!of) return false;
    if (of->getrid() != rid) return false;

    if (qp == SQ_RECT_CHANGED)
    {
        ts::irect t = calcrect();
        MODIFY(*owner).size(t.size()).pos(t.lt).visible(true);
        return false;
    }
    return false;
}

leech_at_left_animated_s::leech_at_left_animated_s(guirect_c *of, int space, int time, bool skip_animation) :leech_at_left_s(of, space), time(time), starttime(ts::Time::current())
{
    if (skip_animation)
    {
        starttime += ts::tabs(time) + 1;
    }
    else
    {
        DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
    }
}

leech_at_left_animated_s::~leech_at_left_animated_s()
{
    if (gui)
        gui->delete_event( DELEGATE(this, tick) );
}

bool leech_at_left_animated_s::tick(RID, GUIPARAM)
{
    if (!owner)
    {
        starttime = ts::Time::current();
        DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
        return true;
    }

    ts::irect t = calcrect();

    rectengine_c &pare = HOLD(owner->getparent()).engine();

    if (of->getprops().zindex() <= owner->getprops().zindex())
    {
        MODIFY(*of).zindex(owner->getprops().zindex() + 0.1f);
        pare.z_resort_children();
    }

    MODIFY(*owner).size(t.size()).pos(t.lt).visible(true);

    pare.redraw();

    ts::Time ct = ts::Time::current();

    if (time > 0)
    {
        if ((ct - starttime) < time)
            DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
    }
    else
    {
        if ((ct - starttime) < (-time))
            DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
        else
            TSDEL( owner );
    }

    return true;
}

void leech_at_left_animated_s::forward()
{
    if (time > 0) return;
    time = -time;
    starttime = ts::Time::current();
    DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
}

void leech_at_left_animated_s::reverse()
{
    if (time < 0) return;
    time = -time;
    starttime = ts::Time::current();
    DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), 0);
}


ts::irect leech_at_left_animated_s::calcrect() const
{
    ts::Time ct = ts::Time::current();

    if (time > 0)
    {
        // forward mode
        if ((ct - starttime) >= time)
            return leech_at_left_s::calcrect();
    }
    else if (time < 0)
    {
        if ((ct - starttime) >= -time)
            return of->getprops().rect();
    }


    float t = owner ? (static_cast<float>(ct - starttime) / static_cast<float>(time)) : 0;

    t = static_cast<float>(0.5f * (sin((t*M_PI) - M_PI_2) + 1));
    t = static_cast<float>(0.5f * (sin((t*M_PI) - M_PI_2) + 1));

    if (time < 0)
        t = 1.0f - t;

    ts::irect r0 = of->getprops().rect();
    ts::irect r1 = leech_at_left_s::calcrect();

    ts::irect r;
    r.lt.x = ts::lround(LERPFLOAT(r0.lt.x, r1.lt.x, t));
    r.lt.y = ts::lround(LERPFLOAT(r0.lt.y, r1.lt.y, t));

    r.rb.x = ts::lround(LERPFLOAT(r0.rb.x, r1.rb.x, t));
    r.rb.y = ts::lround(LERPFLOAT(r0.rb.y, r1.rb.y, t));

    return r;
}


/*virtual*/ bool leech_inout_handler_s::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (owner->getrid() == rid)
    {
        switch (qp)
        {
        case SQ_MOUSE_IN:
            return h(rid, as_param(1));
        case SQ_MOUSE_OUT:
            return h(rid, nullptr);
        }
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

    if (!ts::master().start_app(ts::wstr_c(PLGHOSTNAME), ts::to_wstr(tag), &plughost, false))
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

template <typename TCH> int fmail( const ts::sptr<TCH> &m, int from, int &e )
{
    e = -1;
    ts::pstr_t<TCH> message( m );
    int i = message.find_pos( from, '@' );
    if ( i < 0 ) return -1;
    int j = i - 1;
    for ( ; j >= from; j-- )
    {
        {
            TCH c = message.get_char( j );
            if ( ts::CHAR_is_digit( c ) ) continue;
            if ( ts::CHAR_is_letter( c ) ) continue;
            if ( ts::CHARz_find<ts::wchar>( WIDE2(".!#$%&'*+-/=?^_`{|}~"), c ) >= 0 ) continue;
            if ( c == ' ' )
            {
                ++j;
                break;
            }
        }
    try_next:
        i = message.find_pos( i + 1, '@' );
        if ( i < 0 )
            return -1;
        j = i;
    }
    if ( j < from )
        ++j;
    if ( j == i || message.get_char( j ) == '.' || message.get_char( i - 1 ) == '.' || message.substr( j, i ).find_pos( CONSTSTR( TCH, ".." ) ) >= 0 )
        goto try_next;

    int k = i + 1;
    if ( k >= m.l )
        return -1;
    for ( ; k < m.l; ++k )
    {
        TCH c = message.get_char( k );
        if ( ts::CHAR_is_digit( c ) ) continue;
        if ( ts::CHAR_is_letter( c ) ) continue;
        if ( c == '-' || c == '.' ) continue;
        if ( ts::CHARz_find<ts::wchar>( WIDE2(" \\<>\r\n\t"), c ) >= 0 ) break;
        goto try_next;
    }
    if ( message.get_char( i+1 ) == '-' || message.get_char( k - 1 ) == '-' || message.substr( i+1, k ).find_pos( CONSTSTR( TCH, ".." ) ) >= 0 )
        goto try_next;

    e = k;
    return j;
}

template <typename TCH> int fpref( const ts::sptr<TCH> &m, const ts::sptr<TCH> &p, int from, int curi, bool needspace = false )
{
    ts::pstr_t<TCH> message( m );
    int i = message.find_pos( from, p );

    while (needspace && i >= 0)
    {
        if ( i > 0 && message.get_char( i - 1 ) != ' ' )
        {
            i = message.find_pos( i + 1, p );
        } else
            break;
    }

    if ( curi < 0 )
        return i;
    if ( i >= 0 && i < curi )
        return i;
    return curi;
}

template <typename TCH> bool text_find_link(const ts::sptr<TCH> &m, int from, ts::ivec2 & rslt)
{
    ts::pstr_t<TCH> message(m);
    int ie;
    int mi, i = fmail( m, from, ie );
    mi = i;
    i = fpref(m, CONSTSTR( TCH, "http://" ), from, i);
    i = fpref(m, CONSTSTR(TCH, "https://"), from, i);
    i = fpref( m, CONSTSTR( TCH, "ftp://" ), from, i );
    i = fpref( m, CONSTSTR( TCH, "www." ), from, i, true );
    i = fpref( m, CONSTSTR( TCH, "magnet:?" ), from, i );
    i = fpref( m, CONSTSTR( TCH, "dchub://" ), from, i );
    i = fpref( m, CONSTSTR( TCH, "nmdcs://" ), from, i );
    i = fpref( m, CONSTSTR( TCH, "retroshare://" ), from, i );

    if (i >= 0)
    {
        if ( i == mi )
        {
            rslt.r0 = mi;
            rslt.r1 = ie;
            return true;
        }

        ts::wchar in_quote = 0;
        if (i > 0)
        {
            if (m.s[i-1] == '\"')
                in_quote = '\"';
            else if (m.s[i-1] == '\'')
                in_quote = '\'';
        }

        int cnt = message.get_length();
        int j = i;
        for (; j < cnt; ++j)
        {
            ts::wchar c = message.get_char(j);
            if (ts::CHARz_find(WIDE2(" \\<>\r\n\t"), c) >= 0 || in_quote == c) break;
        }

        rslt.r0 = i;
        rslt.r1 = j;
        return true;
    }

    return false;
}

template bool text_find_link<char>(const ts::asptr &message, int from, ts::ivec2 & rslt);
template bool text_find_link<ts::wchar>(const ts::wsptr &message, int from, ts::ivec2 & rslt);


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
    MEMT( MEMT_TEMP );

    ts::astrings_c opened;
    for( int i=text_utf8.find_pos('['); i >= 0 ; i = text_utf8.find_pos(i+1,'[') )
    {
        int j = text_utf8.find_pos(i+1,']');
        if (j<0) break;
        ts::pstr_c tag = text_utf8.substr(i + 1, j);
        bool close = false;
        if (tag.get_char(0) == '/') { close = true; tag = tag.substr(1); }
        for( ts::aint k=0;k<ARRAY_SIZE(bb_tags);++k)
        {
            if (tag.equals(bb_tags[k]))
            {
                if (close)
                {
                    bool removed = false;
                    for( ts::aint x = opened.size()-1; x >=0; --x)
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
                        text_utf8.insert(0,ts::str_c(CONSTASTR("["),tag, CONSTASTR("]")));
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
    for ( int i = text.find_pos(t); i >= 0; i = text.find_pos(i + 1, t))
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

    // markdown

    ts::tmp_tbuf_t< ts::ivec2 > nochange;
    ts::ivec2 linkinds;
    for (int i = 0; text_find_link(text.as_sptr(), i, linkinds);)
        nochange.add() = linkinds, i = linkinds.r1;

    struct md_s
    {
        ts::asptr md, x, y;
    };
    static md_s mdrepl[] =
    {
        { CONSTASTR("**"), CONSTASTR("<b>"), CONSTASTR("</b>") },
        { CONSTASTR("__"), CONSTASTR("<b>"), CONSTASTR("</b>") },
        { CONSTASTR("~~"), CONSTASTR("<s>"), CONSTASTR("</s>") },
        { CONSTASTR("*"), CONSTASTR("<i>"), CONSTASTR("</i>") },
        { CONSTASTR("_"), CONSTASTR("<i>"), CONSTASTR("</i>") },
    };

    auto in_nochange_range = [&]( int i, int sz ) -> int
    {
        for (const ts::ivec2 &dr : nochange)
            if (i > (dr.r0 - sz) && i < dr.r1)
                return dr.r1;
        return -1;
    };

    auto process_text_md = [&](const md_s &md, ts::str_c &text)
    {
        for ( int i = 0; i < text.get_length();)
        {
            int md0 = text.find_pos(i, md.md);
            if (md0 >= 0)
            {
                if (md0 > 0)
                {
                    int sepi = text_non_letters().find_pos(text.get_char(md0-1));
                    if (sepi < 0 || text_non_letters().get_char(sepi) == md.md.s[0])
                    {
                        i = md0 + md.md.l;
                        continue;
                    }
                }

                int inr = in_nochange_range(md0, md.md.l);
                if (inr >= 0)
                {
                    i = inr;
                    continue;
                }

                i = md0 + md.md.l + 1;
            search_end_again:
                int md1 = text.find_pos(i, md.md);
                if (md1 > md0)
                {
                    inr = in_nochange_range(md1, md.md.l);
                    if (inr >= 0)
                    {
                        i = inr;
                        goto search_end_again;
                    }
                    if (md1 < text.get_length()-md.md.l)
                    {
                        int sepi = text_non_letters().find_pos(text.get_char(md1 + md.md.l));
                        if (sepi < 0 || text_non_letters().get_char(sepi) == md.md.s[md.md.l-1])
                        {
                            i = md1+md.md.l;
                            goto search_end_again;
                        }
                    }

                    text.replace(md0, md.md.l, md.x);
                    text.replace(md1 + md.x.l - md.md.l, md.md.l, md.y);
                    int addi = md.x.l + md.y.l - md.md.l * 2;

                    for (ts::ivec2 &dr : nochange)
                    {
                        if (dr.r0 > md1)
                        {
                            dr.r0 += addi;
                            dr.r1 += addi;
                        }
                    }

                    i = md1 + addi;
                }
                else
                    i = md0 + md.md.l;
            }
            else
                break;
        }
    };

    for (int i = 0; i < ARRAY_SIZE(mdrepl); ++i)
        process_text_md( mdrepl[i], text );
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

void render_pixel_text( ts::bitmap_c&tgtb, const ts::irect& r, const ts::wsptr& t, ts::TSCOLOR bgcol, ts::TSCOLOR col )
{
    const char *digits =
        "88888" "88." "88888" "8888" "8..8" "88888" "88888" "8888" "88888" "88888"
        "8...8" ".8." "....8" "...8" "8..8" "8...." "8...." "...8" "8...8" "8...8"
        "8...8" ".8." "88888" ".888" "8888" "88888" "88888" "..8." ".888." "88888"
        "8...8" ".8." "8...." "...8" "...8" "....8" "8...8" ".8.." "8...8" "....8"
        "88888" "888" "88888" "8888" "...8" "88888" "88888" "8..." "88888" ".8888";

    int widths[ 10 ] = { 5, 3, 5, 4, 4, 5, 5, 4, 5, 5 };
    int xxx[ 10 ] = {};
    int height = 5;
    int pitch = widths[0];

    ASSERT( r.height() >= ( height + 4) );


    auto render_letter = [&]( ts::uint8 *tgt, const char *lsrc, int lwidth )
    {
        for ( int y = 0; y < height;++y, tgt += tgtb.info().pitch, lsrc += pitch )
        {
            ts::TSCOLOR *tgt0 = ( ts::TSCOLOR *)tgt;
            const char *s = lsrc;
            for ( int x = 0; x < lwidth; ++x, ++tgt0, ++s )
            {
                if ( *s != '.' )
                    *tgt0 = col;
                else
                    *tgt0 = bgcol;
            }
            *tgt0 = bgcol;
        }
    };

    for ( int i = 1; i < 10; ++i )
    {
        xxx[ i ] = xxx[ i - 1 ] + widths[ i - 1 ];
        pitch += widths[ i ];
    }

    int w = 0;
    for ( int i = 0; i < t.l; ++i)
    {
        int c = t.s[ i ] - 48;
        if ( c < 0 ) c = 0;
        if ( c > 9 ) c = 9;
        w += widths[ c ] + 1;
    }

    ASSERT( r.width() >= (w + 3) );

    int x = r.rb.x - w-1;
    int y = r.lt.y+2;

    int hlw = x + w + 1;
    for ( int i = x-2; i < hlw;++i )
    {
        bool edge = i == ( x - 2 ) || i == ( hlw - 1 );
        ts::uint8 *b = tgtb.body( ts::ivec2( i, y - 2 ) );

        if (!edge ) *( ts::TSCOLOR * )b = bgcol;
        *( ts::TSCOLOR * )( b + tgtb.info().pitch ) = bgcol;
        *( ts::TSCOLOR * )( b + tgtb.info().pitch * ( height + 2 ) ) = bgcol;
        if (!edge ) *( ts::TSCOLOR * )( b + tgtb.info().pitch * ( height + 3 ) ) = bgcol;
    }

    for ( int i = 0; i < t.l; ++i )
    {
        int c = t.s[ i ] - 48;
        if ( c < 0 ) c = 0;
        if ( c > 9 ) c = 9;

        const char *lsrc = digits + xxx[c];

        render_letter( tgtb.body( ts::ivec2( x, y ) ), lsrc, widths[ c ] );
        x += widths[ c ] + 1;
    }

    for ( int i = y; i < y + height; ++i )
    {
        ts::uint8 *b = tgtb.body( ts::ivec2( x - 2 - w, i ) );

        *( ts::TSCOLOR * )b = bgcol;
        *( ts::TSCOLOR * )( b + 4 ) = bgcol;
        *( ts::TSCOLOR * )( b + (4 * w) + 8 ) = bgcol;
    }

}

void gen_identicon( ts::bitmap_c&tgt, const unsigned char *random_bytes )
{
    static const int dotsize = 10;
    static const int szsz = 5;
    static const int brd = 10;
    static const int shsh = 3;
    static const int szsz2 = ( szsz + 1 ) / 2;

    bool matrix[ szsz ][ szsz ];
    uint64 bits = *(uint64 *)random_bytes;
    for ( int x = 0; x < szsz2; ++x )
        for ( int y = 0; y < szsz; ++y, bits >>= 1 )
            matrix[ x ][ y ] = ( bits & 1 ) != 0;
    for ( int x = szsz2; x < szsz; ++x )
        for ( int y = 0; y < szsz; ++y )
            matrix[ x ][ y ] = matrix[ szsz - 1 - x ][ y ];

    tgt.create_ARGB( ts::ivec2( dotsize * szsz + brd * 2 ) );
    tgt.fill( 0 );
    ts::TSCOLOR col = 0xff808080 | *( ts::uint32 * )( random_bytes + 8 );

    for ( int x = 0; x < szsz; ++x )
        for ( int y = 0; y < szsz; ++y )
            if ( matrix[ x ][ y ] )
                tgt.fill( ts::ivec2( shsh + brd + x * dotsize, shsh + brd + y * dotsize ), ts::ivec2( dotsize ), 0x8f000000 );

    rsvg_filter_gaussian_c::do_filter( tgt, ts::vec2( 1.0f, 3.0f ) );

    for ( int x = 0; x < szsz; ++x )
        for ( int y = 0; y < szsz; ++y )
            if ( matrix[ x ][ y ] )
                tgt.fill( ts::ivec2( brd + x * dotsize, brd + y * dotsize ), ts::ivec2( dotsize ), col );

}

extern "C"
{
    int isotoxin_compare(const unsigned char *row_text, int row_text_len, const unsigned char *pattern) // sqlite likeFunc override
    {
        compare_context_s &cc = *(compare_context_s *)ts::pstr_c( ts::asptr((const char *)pattern+1,2 * sizeof(void *)) ).decode_pointer();

        int newsz = row_text_len * sizeof( ts::wchar );
        if ( cc.wstr.size() < newsz )
            cc.wstr.set_size( newsz, false );

        ts::ZSTRINGS_SIGNED clen = ts::str_wrap_text_utf8_to_ucs2( (ts::wchar *)cc.wstr.data(), row_text_len, ts::asptr( (const char *)row_text, row_text_len ) );
        ts::str_wrap_text_lowercase( ( ts::wchar * )cc.wstr.data(), clen );

        ts::pwstr_c p( ts::wsptr( ( const ts::wchar * )cc.wstr.data(), clen ) );

        for(const ts::wstr_c &s : cc.fsplit)
            if (p.find_pos( s ) < 0)
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
            ts::aint fl = f.get_length();
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
    for ( int i = 0; i < l;)
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

    for( ts::aint i=marks.count()-1;i>0;--i)
    {
        const ts::ivec2 &range = marks.get(i);
        ASSERT(range.r1 > range.r0);
        for ( ts::aint j = i-1; j >= 0; --j)
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

    marks.q_sort<ts::ivec2>( []( const ts::ivec2 *s1, const ts::ivec2 *s2 ) { return s1->x > s2->x; } );

    return marks.count() > 0;
}

ts::wsptr loc_text(loctext_e lt)
{
    switch (lt)
    {
        case loc_connection_failed:
            return TTT("Connection failed", 302);
        case loc_autostart:
            return TTT("Start [appname] with system", 310);
        case loc_please_create_profile:
            return TTT("Please, first create or load profile", 144);
        case loc_please_authorize:
            return TTT( "Please, add me to your contact list", 73 );
        case loc_minimize:
            return TTT( "Minimize", 6 );
        case loc_yes:
            return TTT("yes",315);
        case loc_no:
            return TTT("no",316);
        case loc_exit:
            return TTT("Exit",3);
        case loc_continue:
            return TTT("Continue",117);
        case loc_network:
            return TTT("Network",66);
        case loc_nonetwork:
            return TTT("No available networks to select",248);
        case loc_networks:
            return TTT("Networks", 33);
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
        case loc_copy:
             return TTT("Copy",92);

        case loc_image:
            return TTT("Image", 214);
        case loc_images:
            return TTT("Images", 215);
        case loc_space2takeimage:
            return TTT("Press [i]space[/i] key to take image", 216);
        case loc_dropimagehere:
            return TTT("Drop [i]image[/i] here",222);
        case loc_loadimagefromfile:
            return TTT("Load image from file", 210);
        case loc_capturecamera:
            return TTT("Capture camera", 212);
        case loc_qrcode:
            return TTT("QR code", 44);

        case loc_moveup:
            return TTT( "Move up", 77 );
        case loc_movedn:
            return TTT( "Move down", 78 );

    }
    return ts::wsptr();
}

ts::wstr_c text_sizebytes( uint64 sz, bool numbers_only )
{
    ts::wstr_c n; n.set_as_num(sz);
    for ( int ix = n.get_length() - 3; ix > 0; ix -= 3)
        n.insert(ix, ' ');

    if (numbers_only)
        return n;

    return TTT("size: $ bytes", 220) / n;
}

ts::wstr_c text_contact_state(ts::TSCOLOR color_online, ts::TSCOLOR color_offline, contact_state_e st, int link)
{
    ts::wstr_c sost, ins0, ins1;
    if (link >= 0)
    {
        ins0.append(CONSTWSTR("<cstm=a")).append_as_int(link).append_char('>');
        ins1 = ins0;
        ins1.set_char(6,'b');
    }

    if ( CS_ONLINE == st )
    {
        sost.set( CONSTWSTR( "<b>" ) );
        appendtag_color( sost, color_online ).append( ins0 ).append( TTT( "Online", 100 ) ).append( ins1 ).append( CONSTWSTR( "</color></b>" ) );
    }
    else if ( CS_OFFLINE == st )
    {
        sost.set( CONSTWSTR( "<l>" ) );
        appendtag_color( sost, color_offline ).append( ins0 ).append( TTT( "Offline", 101 ) ).append( ins1 ).append( CONSTWSTR( "</color></l>" ) );
    }
    else if (CS_ROTTEN == st)
    {
        sost.set( CONSTWSTR( "<l>" ) );
        appendtag_color( sost, color_offline ).append( ins0 ).append( TTT("Inactive",510) ).append( ins1 ).append( CONSTWSTR( "</color></l>" ) );
    }

    return sost;
}

ts::wstr_c text_typing(const ts::wstr_c &prev, ts::wstr_c &workbuf, const ts::wsptr &preffix)
{

    ts::wstr_c tt( preffix, workbuf );
    if ( tt.get_length() > (4 + preffix.l) ) tt.set_length( 4 + preffix.l );
    tt.append( CONSTWSTR( "<img=typing>" ) );
    workbuf.append_char( '.' );
    if ( workbuf.get_length() > 10 )
        workbuf.clear();
    return tt;

}

namespace
{
    class text_match_regex_c : public text_match_c
    {
        ts::regex_c r;
    public:
        text_match_regex_c( const ts::wsptr& t ):r(t)
        {
        }

        /*virtual*/ bool match( const ts::wsptr& t ) const override
        {
            return r.present( t );
        }
    };
    class text_match_keywords_c : public text_match_c
    {
        ts::array_inplace_t< ts::wstrings_c, 0 > arr;

    public:
        text_match_keywords_c( const ts::wsptr& t )
        {
            for( ts::token<ts::wchar> lines(t, '\n'); lines; ++lines )
            {
                ts::wstrings_c &a = arr.add();
                for (ts::token<ts::wchar> words( *lines, ',' ); words; ++words)
                    a.add( words->get_trimmed() );
            }
            for (ts::aint i = arr.size() - 1; i >= 0; --i)
                if (arr.get( i ).size() == 0)
                    arr.remove_fast( i );
        }

        /*virtual*/ bool match( const ts::wsptr& t ) const override
        {
            for( const ts::wstrings_c &s : arr )
            {
                bool all_match = true;
                for( const ts::wstr_c& keyword : s )
                {
                    if ( ts::pwstr_c(t).find_pos( keyword ) < 0 )
                    {
                        all_match = false;
                        break;
                    }
                }
                if (all_match) return true;
            }

            return false;
        }
    };
    class text_match_regex_or_keywords_c : public text_match_keywords_c
    {
        ts::regex_c r;
    public:
        text_match_regex_or_keywords_c( const ts::wsptr& rs, const ts::wsptr& ks ) :r( rs ), text_match_keywords_c(ks)
        {
        }

        /*virtual*/ bool match( const ts::wsptr& t ) const override
        {
            return text_match_keywords_c::match( t ) || r.present(t);
        }
    };

}

text_match_c *text_match_c::build_from_template( const ts::wsptr& t )
{
    ts::pwstr_c s( t );
    int i = s.find_pos( '\1' );
    if (i < 0)
        return nullptr;

    ts::pwstr_c regext( s.substr( 0, i ) );
    ts::pwstr_c keywords( s.substr( i + 1 ) );

    if (regext.get_length() > 0)
    {
        if (keywords.get_length() > 0)
            return TSNEW( text_match_regex_or_keywords_c, regext, keywords );

        return TSNEW( text_match_regex_c, regext );
    }

    return TSNEW( text_match_keywords_c, keywords );
}


void draw_initialization(rectengine_c *e, ts::bitmap_c &pab, const ts::irect&viewrect, ts::TSCOLOR tcol, const ts::wsptr &additiontext)
{
    draw_data_s&dd = e->begin_draw();

    ts::wstr_c t( additiontext );
    ts::ivec2 tsz = gui->textsize( ts::g_default_text_font, t );
    ts::ivec2 tpos = (viewrect.size() - tsz) / 2;

    ts::ivec2 processpos(tpos.x - pab.info().sz.x - 5, (viewrect.height() - pab.info().sz.y) / 2);
    e->draw(processpos + viewrect.lt, pab.extbody(), true);


    text_draw_params_s tdp;
    tdp.forecolor = &tcol;

    dd.offset += tpos + viewrect.lt;
    dd.size = tsz;

    e->draw(t, tdp);

    e->end_draw();
}

void draw_chessboard(rectengine_c &e, const ts::irect & r, ts::TSCOLOR c1, ts::TSCOLOR c2)
{
    ts::ivec2 sz(32);
    ts::TSCOLOR c[2] = { c1, c2 };
    int mx = r.width() / sz.x + 1;
    int my = r.height() / sz.y + 1;
    for (int y = 0; y < my; ++y)
    {
        for (int x = 0; x < mx; ++x)
        {
            ts::irect rr(x * sz.x + r.lt.x, y * sz.y + r.lt.y, 0, 0);
            rr.rb.x = ts::tmin(rr.lt.x + sz.x, r.rb.x);
            rr.rb.y = ts::tmin(rr.lt.y + sz.y, r.rb.y);

            if (rr)
                e.draw(rr, c[1 & (x ^ y)]);
        }
    }
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

    ts::bitmap_c i1, i2, i3;
    int sz = gui_popup_menu_c::icon_size();
    if ( sz >= 16 )
    {
        ts::bitmap_c icons = g_app->build_icon( sz * 2 );

        i1.create_ARGB( ts::ivec2(sz) );
        i2.create_ARGB( ts::ivec2( sz ) );
        i3.create_ARGB( ts::ivec2( sz ) );

        sz *= 2;

        int index = 0; i1.resize_from( icons.extbody( ts::irect( 0, sz * index, sz, sz * index + sz ) ), ts::FILTER_BOX_LANCZOS3 );
        index = 1; i2.resize_from( icons.extbody( ts::irect( 0, sz * index, sz, sz * index + sz ) ), ts::FILTER_BOX_LANCZOS3 );
        index = 2; i3.resize_from( icons.extbody( ts::irect( 0, sz * index, sz, sz * index + sz ) ), ts::FILTER_BOX_LANCZOS3 );
    }

    m.add(TTT("Online", 244), COS_ONLINE == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_ONLINE), ( ( sz >= 16 ) ? &i1 : nullptr ) );
    m.add(TTT("Away", 245), COS_AWAY == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_AWAY), ( ( sz >= 16 ) ? &i2 : nullptr ) );
    m.add(TTT("Busy", 246), COS_DND == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_DND), ( ( sz >= 16 ) ? &i3 : nullptr ) );

}



SLANGID detect_language()
{
    ts::wstrings_c fns;
    ts::g_fileop->find(fns, CONSTWSTR("loc/*.lng*.lng"), false);


    ts::wstr_c lng = ts::master().sys_language();

    for (const ts::wstr_c &f : fns)
        if ( f.substr(0, 2).equals( lng ) ) return SLANGID( ts::to_str( lng ) );

    return SLANGID("en");
}

namespace
{
    struct lang_s
    {
        SLANGID langtag;
        ts::wstr_c name;
        int operator()( const lang_s &ol ) const
        {
            return ts::wstr_c::compare( name, ol.name );
        }
    };
}

DECLARE_MOVABLE(lang_s, true);

menu_c list_langs( SLANGID curlang, MENUHANDLER h, menu_c *mp)
{
    menu_c mtmp;
    if (mp == nullptr)
        mp = &mtmp;

    ts::wstrings_c fns;
    ts::wstr_c path(CONSTWSTR("loc/"));
    int cl = path.get_length();

    ts::g_fileop->find(fns, path.append(CONSTWSTR("*.lng*.lng")), false);
    fns.kill_dups();

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
                if (!l.name.equals(ln))
                {
                    ln.append(CONSTWSTR(" (")).append(l.name).append_char(')');
                    l.name = ln;
                }

                break;
            }
    }

    langs.sort();

    for (const lang_s &l : langs)
    {
        bool cur = curlang == l.langtag;
        mp->add(l.name, cur ? (MIF_MARKED) : 0, h, l.langtag);
    }

    return *mp;
}

ts::wstr_c make_proto_desc( const ts::wsptr&idname, int mask )
{
    ts::wstr_c r(1024,true);
    if (0 != (mask & MPD_UNAME))    r.append(TTT("Your name",259)).append(CONSTWSTR(": <l>{uname}</l><br>"));
    if (0 != (mask & MPD_USTATUS))  r.append(TTT("Your status",260)).append(CONSTWSTR(": <l>{ustatus}</l><br>"));
    if (0 != (mask & MPD_NAME))     r.append(LOC_CONNECTION_NAME).append(CONSTWSTR(": <l>{name}</l><br>"));
    if (0 != (mask & MPD_MODULE))   r.append(LOC_MODULE).append(CONSTWSTR(": <l>{module}</l><br>"));
    if (0 != (mask & MPD_ID))       r.append( idname ).append(CONSTWSTR(": <l>{id}</l><br>"));
    if (0 != (mask & MPD_STATE))    r.append(LOC_STATE).append(CONSTWSTR(": <l>{state}</l><br>"));

    if (r.ends(CONSTWSTR("<br>"))) r.trunc_length(4);
    return r;
}


bool new_version( const ts::asptr &current, const ts::asptr &newver, bool same_version )
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
    return same_version;
}

bool new_version()
{
    bool new64 = false;
    ts::str_c newvv = cfg().autoupdate_newver( new64 );
#ifdef MODE64
    new64 = false;
#endif // MODE64
    return new_version( application_c::appver(), newvv, new64 );
}

const ts::bitmap_c &prepare_proto_icon( const ts::asptr &prototag, const ts::asptr &icond, int imgsize, icon_type_e icot )
{
    ts::wstr_c iname(CONSTWSTR("proto?"), ts::to_wstr(prototag));
    iname.append_as_int(imgsize).append_char('?').append_as_int(icot);

    ts::TSCOLOR c = 0, fillc = 0;
    switch (icot)
    {
    case IT_NORMAL:
        c = gui->deftextcolor; break;
    case IT_ONLINE:
        c = GET_THEME_VALUE(state_online_color); break;
    case IT_AWAY:
        c = GET_THEME_VALUE(state_away_color); break;
    case IT_DND:
        c = GET_THEME_VALUE(state_dnd_color); break;
    case IT_OFFLINE:
        c = ts::GRAYSCALE( GET_THEME_VALUE( state_online_color ) ); break;
    case IT_UNKNOWN:
        c = g_app->deftextcolor; break;
        break;
    case IT_WHITE:
        c = ts::ARGB(255,255,255); break;
        break;
    }

    if (icond.l)
    {
        ts::bitmap_c & bmp = g_app->prepareimageplace(iname);
        if (bmp.info().sz.x > 0)
            return bmp;
        iname.append(CONSTWSTR("?g"));
        g_app->clearimageplace(iname);

        ts::str_c svg( CONSTASTR("<svg width=\"[sz]\" height=\"[sz]\" ><path stroke=\"none\" fill=\"[col]\" transform=\"translate([sz2] [sz2]) scale(0.[sz]) translate(-50 -50)\" d=\"[svg]\"/></svg>") );
        svg.replace_all( CONSTASTR("[sz]"), ts::amake(imgsize) );
        svg.replace_all(CONSTASTR("[sz2]"), ts::amake(imgsize/2));
        svg.replace_all(CONSTASTR("[col]"), make_color(c));
        svg.replace_all(CONSTASTR("[svg]"), icond);

        bmp.create_ARGB(ts::ivec2(imgsize));
        bmp.fill( fillc );

        if (rsvg_svg_c *svgg = rsvg_svg_c::build_from_xml( svg.str() ))
        {
            svgg->render( bmp.extbody() );
            TSDEL(svgg);
            return bmp;
        }
    }

    iname.append(CONSTWSTR("?g"));
    ts::bitmap_c & bmp = g_app->prepareimageplace(iname);
    if (bmp.info().sz.x > 0)
        return bmp;

    ts::text_rect_static_c tr;
    tr.set_size(ts::ivec2(64,32));

    ts::wstr_c t = ts::to_wstr(prototag);
    t.case_up();
    t.insert(0, maketag_outline<ts::wchar>(c));
    t.insert(0, CONSTWSTR("<l>"));

    tr.set_text(t, nullptr, false);
    tr.set_def_color(ts::ARGB(255, 255, 255));
    tr.set_font(&ts::g_default_text_font);
    tr.parse_and_render_texture(nullptr, nullptr, false);
    tr.set_size(tr.lastdrawsize);
    tr.parse_and_render_texture(nullptr, nullptr, true);

    bmp = tr.make_bitmap();

    if (bmp.info().sz != ts::ivec2(imgsize))
    {
        ts::bitmap_c bmpsz;
        bmp.resize_to(bmpsz, ts::ivec2(imgsize), ts::FILTER_LANCZOS3);
        bmp = bmpsz;
    }

    bmp.premultiply();
    return bmp;
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
        if (ts::fn_mask_match<ts::wchar>(fn, fnm)) return true;
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

bool elevate()
{
    ts::wstr_c prm(CONSTWSTR("wait ")); prm.append_as_uint(ts::master().process_id());
    ts::wstr_c exe(ts::get_exe_full_name());
    return ts::master().start_app( exe, prm, nullptr, true );
}

static void ChuckNorrisCopy(const ts::wstr_c &copyto)
{
    ts::wstr_c prm( CONSTWSTR( "installto " ), copyto ); prm.replace_all( 10, ' ', '*' );
    ts::wstr_c exe( ts::get_exe_full_name() );

    ts::process_handle_s ph;
    if (ts::master().start_app( exe, prm, &ph, true ))
        ts::master().wait_process( ph, 0 );
}

void install_to(const ts::wstr_c &path, bool acquire_admin_if_need)
{
    if (!ts::is_dir_exists(path))
        if (!make_path(path, 0))
        {
            if (acquire_admin_if_need)
                ChuckNorrisCopy(path);
            return;
        }

    ts::wstr_c path_from = ts::fn_get_path( ts::get_exe_full_name() );

    ts::wstrings_c files;
    ts::find_files( ts::fn_join(path_from, CONSTWSTR("*.*")), files, ATTR_ANY, ATTR_DIR, true);

    for(const ts::wstr_c &fn2c : files)
    {
        if ( fn2c.ends_ignore_case( CONSTWSTR( ".exe" ) ) || fn2c.ends_ignore_case( CONSTWSTR( ".dll" ) ) || fn2c.ends_ignore_case( CONSTWSTR( ".data" ) ) )
        {
            ts::copy_rslt_e cr = copy_file( fn2c, ts::fn_join( path, ts::fn_get_name_with_ext( fn2c ) ) );
            if ( ts::CRSLT_ACCESS_DENIED == cr && acquire_admin_if_need )
                ChuckNorrisCopy( path );
            if ( cr != ts::CRSLT_OK )
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
                            proto.features = r.get<int>();
                            proto.connection_features = r.get<int>();

                            int numstrings = r.get<int>();
                            proto.strings.clear();
                            for ( ; numstrings > 0; --numstrings )
                                proto.strings.add( r.getastr() );

                        }

                        result_x = R_DONE;
                    }
                    return false;
                }
            }

            return true;
        }


        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            isotoxin_ipc_s ipcj(ts::str_c(CONSTASTR("get_protocols_list_")).append_as_uint(spinlock::pthread_self()), DELEGATE(this, ipchandler));
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

            ts::task_c::done(canceled);
        }
    };
}

void available_protocols_s::load()
{
    g_app->add_task(TSNEW(load_proto_list_s, this));
}


db_check_e check_db(const ts::wstr_c &fn, ts::uint8 *salt /* 16 bytes buffer */)
{
    void *handle = ts::f_open( fn );
    if (!handle)
        return DBC_IO_ERROR;

    ts::aint rslt = ts::f_read(handle, salt, 16);
    ts::f_close(handle);

    if ( rslt == 0 )
        return DBC_DB;

    if ( rslt != 16 )
        return DBC_NOT_DB;

    const char * sql3hdr = "SQLite format 3";
    if (!memcmp(sql3hdr, salt, 16))
        return DBC_DB;

    ts::uint8 x = 0;
    for (int i = 0; i < 16; ++i)
    {
        x ^= salt[i];
    }
    if (x == (255 - salt[0]))
        return DBC_DB_ENCRTPTED;

    return DBC_NOT_DB;
}


void gen_salt( ts::uint8 *buf, int blen )
{
    randombytes_buf( buf, blen );
    ts::uint8 x = 0;
    for (int i = 0; i < blen; ++i)
    {
        x ^= buf[i];
    }
    x = (255 - buf[0]) ^ x;
    buf[1 + randombytes_uniform( blen - 1 )] ^= x;

    // salt has feature: xor(all salt bytes) == not(first salt byte)

#ifdef _DEBUG
    x = 0;
    for (int i = 0; i < blen; ++i)
    {
        x ^= buf[i];
    }
    ASSERT( x == (255 - buf[0]) );
#endif // _DEBUG
}

void gen_passwdhash( ts::uint8 *passwhash, const ts::wstr_c &passwd )
{
    // this is my public key
    // it used as static application salt for password hashing
    // it can be any random sequence, but it should never been changed, due password encrypted profiles became a noise
    ts::uint8 pk[] = {
        0x6B, 0xBE, 0x62, 0xE2, 0x3C, 0x2A, 0x94, 0x86, 0xCC, 0x59, 0x7D, 0xE2, 0x17, 0x08, 0x47, 0xA7, 0xC0, 0x64, 0xDB, 0x20, 0xFE, 0x63, 0x4E, 0xEA, 0x98, 0x8D, 0x3D, 0xFD, 0x6C, 0xCA, 0x9D, 0x4F
    };

    ts::wstr_c p; p.append_as_hex( pk, sizeof( pk ) ).append( passwd );

    crypto_generichash( passwhash, CC_HASH_SIZE, (const ts::uint8 *)p.cstr(), p.get_length() * 2, nullptr, 0 );
}

void crypto_zero( ts::uint8 *buf, int bufsize )
{
    sodium_memzero( buf, bufsize );
}

void get_unique_machine_id( ts::uint8 *buf, int bufsize, const char *salt, bool use_profile_uniqid )
{
    ts::tmp_buf_c b;
    b.append_s( salt ); // salt
    b.append_s( ts::gen_machine_unique_string() );
    if (use_profile_uniqid)
    {
        b.tappend<char>( '-' );
        b.append_s( prf().unique_profile_tag() );
    }

    crypto_generichash( buf, bufsize, b.data(), b.size(), nullptr, 0 );
    sodium_memzero( b.data(), b.size() );
}

ts::str_c encode_string_base64( ts::uint8 *key /* 32 bytes */, const ts::asptr& s )
{
    ts::tmp_buf_c b;
    b.append_s( s );
    b.append_s( ts::str_c( CONSTASTR( "/" ) ).append_as_num( s.l ).append_char( '=' ).as_sptr() );
    //while ( b.size() < 64 ) b.tappend<ts::uint8>( '=' );
    ts::uint8 *nonce = b.expand( crypto_stream_chacha20_NONCEBYTES );
    memset( nonce, 1, crypto_stream_chacha20_NONCEBYTES );
    TS_STATIC_CHECK( 32 == crypto_stream_chacha20_KEYBYTES, "cha cha key" );
    crypto_stream_chacha20_xor( b.data(), b.data(), b.size() - crypto_stream_chacha20_NONCEBYTES, nonce, key );
    ts::str_c r; r.encode_base64( b.data(), b.size() - crypto_stream_chacha20_NONCEBYTES );
    sodium_memzero( b.data(), b.size() );
    return r;
}

bool decode_string_base64( ts::str_c& rslt, ts::uint8 *key /* 32 bytes */, const ts::asptr& s )
{
    if (s.l == 0) return true;
    ts::tmp_buf_c b;
    b.set_size( s.l * 2, false );
    memset( b.data(), 0, s.l * 2 );
    int sz = ts::pstr_c( s ).decode_base64( b.data(), (int)b.size() );
    b.set_size( sz );

    ts::uint8 *nonce = b.expand( crypto_stream_chacha20_NONCEBYTES );
    memset( nonce, 1, crypto_stream_chacha20_NONCEBYTES );
    TS_STATIC_CHECK( 32 == crypto_stream_chacha20_KEYBYTES, "cha cha key" );
    crypto_stream_chacha20_xor( b.data(), b.data(), b.size() - crypto_stream_chacha20_NONCEBYTES, nonce, key );

    bool eqf = false;
    --sz;
    for (; sz > 0 && b.data()[sz] == '='; --sz) eqf = true;
    if (!eqf)
        return false; // at least one '=' at end of string

    int n = sz;
    for (; sz > 0 && b.data()[sz] != '/'; --sz);
    if (b.data()[sz] != '/')
        return false;

    int psz = ts::pstr_c( ts::asptr( (const char *)b.data() + sz + 1, n - sz ) ).as_uint();
    if (psz != sz)
        return false;

    rslt.set( (const char *)b.data(), psz );
    sodium_memzero( b.data(), b.size() );
    return true;
}

uint64 random64()
{
    uint64 v;
    randombytes_buf( &v, sizeof( v ) );
    return v;
}

template<ts::aint hashsize> void blake2b( ts::uint8 *hash, const void *inbytes1, ts::aint insize1, const void *inbytes2, ts::aint insize2 )
{
    crypto_generichash_state st;
    crypto_generichash_init( &st, nullptr, 0, hashsize );
    crypto_generichash_update( &st, (const unsigned char *)inbytes1, insize1 );
    crypto_generichash_update( &st, (const unsigned char *)inbytes2, insize2 );
    crypto_generichash_final( &st, hash, hashsize );
}

template void blake2b<BLAKE2B_HASH_SIZE_SMALL>( ts::uint8 *hash, const void *inbytes1, ts::aint insize1, const void *inbytes2, ts::aint insize2 );
template void blake2b<BLAKE2B_HASH_SIZE>( ts::uint8 *hash, const void *inbytes1, ts::aint insize1, const void *inbytes2, ts::aint insize2 );

TS_STATIC_CHECK( crypto_generichash_BYTES_MIN == BLAKE2B_HASH_SIZE_SMALL, "16" );
TS_STATIC_CHECK( crypto_generichash_BYTES == BLAKE2B_HASH_SIZE, "32" );

#ifdef _WIN32
// dlmalloc -----------------

#ifdef _MSC_VER
#pragma warning (disable:4559)
#pragma warning (disable:4127)
#pragma warning (disable:4057)
#pragma warning (disable:4702)
#endif // _MSC_VEW

#define MALLOC_ALIGNMENT ((size_t)16U)
#define USE_DL_PREFIX
#define USE_LOCKS 0

static spinlock::long3264 dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

#define _NTOS_
#undef ERROR

#include "dlmalloc/dlmalloc.c"
#endif
