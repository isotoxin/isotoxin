#include "isotoxin.h"

//-V::807

#define OTS 6
#define ZAZ 1

/*virtual*/ void dialog_smileselector_c::created()
{
    set_theme_rect(CONSTASTR("smilesel"), false);
    __super::created();

    gui->register_kbd_callback(DELEGATE(this, esc_handler), ts::SSK_ESC, 0);

}

bool dialog_smileselector_c::esc_handler(RID, GUIPARAM)
{
    TSDEL(this);
    return true;
}

void dialog_smileselector_c::build_rects(rects_t&a)
{
    ts::ivec2 p(0);
    int maxh = 0;
    int i = 0;
    emoti().iterate_current_pack([&](emoticon_s &e) {
        
        ts::irect fr = e.framerect();
        if (p.x == 0)
        {
            a.add(rectdef_s(p + ts::ivec2(OTS), &e));
            if (fr.height() > maxh) maxh = fr.height();

        } else if (p.x + fr.width() > sz.x)
        {
            p.x = 0;
            p.y += maxh + ZAZ;
            a.add( rectdef_s(p + ts::ivec2(OTS), &e) );
            maxh = fr.height();

        } else
        {
            a.add(rectdef_s(p + ts::ivec2(OTS), &e));
            if (fr.height() > maxh) maxh = fr.height();

        }
        p.x += ZAZ + fr.width();

        ++i;
    });

    areah = p.y + maxh + 3;
    sbv = areah > sz.y;
    sb.set_size(areah, sz.y);

}

dialog_smileselector_c::dialog_smileselector_c(MAKE_ROOT<dialog_smileselector_c> &data):gui_control_c(data)
{
    editor = data.editor;
    sz = ts::ivec2( emoti().selector_min_width, 200);

    build_rects(rects);

}

dialog_smileselector_c::~dialog_smileselector_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, check_focus));
        gui->unregister_kbd_callback(DELEGATE(this, esc_handler));
    }
}

/*virtual*/ ts::wstr_c dialog_smileselector_c::get_name() const
{
    return ts::wstr_c();
}
/*virtual*/ ts::ivec2 dialog_smileselector_c::get_min_size() const 
{
    if (const theme_rect_s *thr = themerect())
        return thr->size_by_clientsize(sz + ts::ivec2(2+OTS+(sbv?ts::tmax(OTS,thr->sbwidth()):OTS),OTS+OTS), false);
    return sz + ts::ivec2(OTS+OTS);
}

bool dialog_smileselector_c::check_focus(RID r, GUIPARAM p)
{
    if (getrid() >>= gui->get_focus()) return false;
    TSDEL( this );
    return true;
}

bool dialog_smileselector_c::find_undermouse()
{
    const rectdef_s *pum = undermouse;
    undermouse = nullptr;
    ts::ivec2 mp = to_local(gui->get_cursor_pos()) - get_client_area().lt;
    mp.y -= sb.shift;
    for (rectdef_s &rd : rects)
    {
        if (ts::irect(rd.p, rd.p + rd.e->framerect().size()).inside(mp))
        {
            undermouse = &rd;
            break;
        }
    }
    return pum != undermouse;
}

/*virtual*/ bool dialog_smileselector_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
    case SQ_DRAW:
        if (rid != getrid()) return false;
        __super::sq_evt(qp, rid, data);
        {
            ts::irect drawarea = get_client_area();
            if (sbv)
            if (const theme_rect_s *thr = themerect())
            {
                evt_data_s ds;
                ds.draw_thr.sbrect = drawarea;
                ds.draw_thr.sbrect().lt.y += OTS;
                ds.draw_thr.sbrect().rb.y -= OTS;
                sb.draw(thr, getengine(), ds, sbhl);
            }
            draw_data_s &dd = getengine().begin_draw();
            dd.cliprect = ts::irect(dd.offset + OTS, dd.offset + sz + ts::ivec2(OTS+2,OTS));
            ts::ivec2 d = ts::ivec2(drawarea.lt.x, drawarea.lt.y + sb.shift);
            for( const rectdef_s &rd : rects )
            {
                if (undermouse == &rd)
                {
                    ts::irect sr(rd.p + d - ZAZ, rd.p + rd.e->framerect().size() + d + ZAZ);
                    m_engine->draw(sr, get_default_text_color(0));
                    sr.lt += ZAZ;
                    sr.rb -= ZAZ;
                    m_engine->draw(sr, get_default_text_color(1));
                }
                rd.e->draw( getroot(), rd.p + d );
            }
            getengine().end_draw();
        }
        return true;
    case SQ_MOUSE_WHEELUP:
        sb.shift += MWHEELPIXELS * 2;
    case SQ_MOUSE_WHEELDOWN:
        sb.shift -= MWHEELPIXELS;
        sb.set_size(areah, sz.y);
        find_undermouse();
        getengine().redraw();
        break;
    case SQ_MOUSE_OUT:
        if (sbhl || undermouse)
        {
            sbhl = false;
            undermouse = nullptr;
            getengine().redraw();
        }
        break;
    case SQ_MOUSE_MOVE:
        if (!gui->mtrack(getrid(), MTT_SBMOVE))
        {
            bool of = sbhl;
            sbhl = sb.sbrect.inside(to_local(data.mouse.screenpos));
            if (find_undermouse() || sbhl != of)
                getengine().redraw();
        }
        break;
    case SQ_MOUSE_LDOWN:
        {
            ts::ivec2 mplocal = to_local(data.mouse.screenpos);
            if (sbv && sb.sbrect.inside(mplocal))
            {
                sbhl = true;
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_SBMOVE);
                opd.mpos = data.mouse.screenpos;
                opd.rect.lt.y = sb.sbrect.lt.y - OTS - get_client_area().lt.y;
            }
            find_undermouse();
            if ( undermouse )
            {
                HOLD e(editor);
                if (e)
                {
                    gui_textedit_c &ee = e.as<gui_textedit_c>();
                    ee.insert_active_element(undermouse->e->get_edit_element(ee.get_font().height()));
                    gui->set_focus(ee.getrid());
                }
            }
        }
        break;
    case SQ_MOUSE_LUP:
        gui->end_mousetrack(getrid(), MTT_SBMOVE);
        break;
    case SQ_MOUSE_MOVE_OP:
        if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_SBMOVE))
        {
            int dmouse = data.mouse.screenpos().y - opd->mpos.y;
            if (sb.scroll(opd->rect.lt.y + dmouse, sz.y))
                getengine().redraw();
            sb.set_size(areah, sz.y); // clamp

        }
        break;
    case SQ_FOCUS_CHANGED:
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, check_focus), nullptr);
        return true;
    }

    return __super::sq_evt(qp,rid,data);
}
