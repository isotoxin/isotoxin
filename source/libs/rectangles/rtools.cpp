#include "rectangles.h"

menu_anchor_s::menu_anchor_s(bool setup_by_mousepos, relpos_e rp):rect(0), relpos(rp)
{
    if (setup_by_mousepos && gui)
    {
        rect.lt = ts::get_cursor_pos();
        rect.rb = rect.lt;
    }
}


void fixrect(ts::irect &r, const ts::ivec2 &minsz, const ts::ivec2 &maxsz, ts::uint32 hitarea)
{
    if (r.width() < minsz.x)
    {
        if (0 != (hitarea & AREA_LEFT))
        {
            r.lt.x = r.rb.x - minsz.x;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_RITE))
        {
            r.rb.x = r.lt.x + minsz.x;
        }
        else ERROR("fixrect failure");
    }
    if (r.width() > maxsz.x)
    {
        if (0 != (hitarea & AREA_LEFT))
        {
            r.lt.x = r.rb.x - maxsz.x;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_RITE))
        {
            r.rb.x = r.lt.x + maxsz.x;
        }
        else ERROR("fixrect failure");
    }

    if (r.height() < minsz.y)
    {
        if (0 != (hitarea & AREA_TOP))
        {
            r.lt.y = r.rb.y - minsz.y;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_BOTTOM))
        {
            r.rb.y = r.lt.y + minsz.y;
        }
        else ERROR("fixrect failure");
    }
    if (r.height() > maxsz.y)
    {
        if (0 != (hitarea & AREA_TOP))
        {
            r.lt.y = r.rb.y - maxsz.y;
        }
        else if (hitarea == 0 || 0 != (hitarea & AREA_BOTTOM))
        {
            r.rb.y = r.lt.y + maxsz.y;
        }
        else ERROR("fixrect failure");
    }

}

static const int minsbsize = 20;

void calcsb( int &pos, int &size, int shift, int viewsize, int datasize )
{
    if (viewsize >= datasize)
    {
        pos = 0;
        size = viewsize;
        return;
    }

    float k = float(viewsize) / float(datasize);
    float datamove = float(datasize - viewsize);
    size = ts::lround(ts::tmax<float>( k * viewsize, minsbsize ));
    float sbmove = float(viewsize - size);


    pos = ts::lround(-float(shift) / datamove * sbmove);

}

int calcsbshift( int pos, int viewsize, int datasize )
{
    if (viewsize >= datasize)
        return 0;

    float k = float(viewsize) / float(datasize);
    float datamove = float(datasize - viewsize);
    int size = ts::lround(ts::tmax<float>(k * viewsize, minsbsize));
    float sbmove = float(viewsize - size);

    ASSERT(sbmove > 0);

    return ts::lround(-float(pos) / sbmove * datamove);
}

void sbhelper_s::draw( const theme_rect_s *thr, rectengine_c &engine, evt_data_s &dd, bool hl )
{
    calcsb(dd.draw_thr.sbpos, dd.draw_thr.sbsize, shift, dd.draw_thr.sbrect().height(), datasize);
    engine.draw(*thr, hl ? (DTHRO_VSB|DTHRO_SB_HL) : DTHRO_VSB, &dd);
    sbrect = dd.draw_thr.sbrect();
}

ts::str_c text_remove_cstm(const ts::str_c &text_utf8)
{
    ts::str_c text(text_utf8);
    for (int i = text.find_pos('<'); i >= 0; i = text.find_pos(i, '<'))
    {
        if (text.substr(i+1).begins(CONSTASTR("cstm=")))
        {
            int j = text.find_pos(i + 1, '>');
            if (j < 0)
            {
                text.set_length(i);
                break;
            }
            text.cut(i, j - i + 1);
        }
    }
    return text;
}

void text_remove_tags(ts::str_c &text)
{
    for ( int i = text.find_pos('<'); i >= 0; i = text.find_pos(i, '<'))
    {
        int j = text.find_pos(i + 1, '>');
        if (j < 0)
        {
            text.set_length(i);
            break;
        }
        text.cut(i, j - i + 1);
    }
}

bool check_always_ok(const ts::wstr_c &)
{
    return true;
}

bool check_always_ok_except_empty(const ts::wstr_c &t)
{
    return !t.is_empty();
}


process_animation_s::process_animation_s()
{
    bmp.create_ARGB(ts::ivec2(32));
    bmp.fill(0);
    ts::buf_c b;
    b.load_from_text_file(CONSTWSTR("process.svg"));
    svg.reset( rsvg_svg_c::build_from_xml((char *)b.data()) );
    restart();
}

void process_animation_s::restart()
{
    angle = 0;
    devia = 0;
    nexttime = ts::Time::current() + 50;
}

process_animation_s::~process_animation_s()
{
}

void process_animation_s::render()
{
    ts::Time ct = ts::Time::current();
    if ((ct - nexttime) > 0)
    {
        nexttime += 50;
        angle = ts::angle_norm<float>(angle + 0.01f);
        devia = ts::angle_norm<float>(devia + 0.13f);

        if((ct - nexttime) > 1000)
            nexttime = ct;

        float ss = (float)(sin(devia) * (2 * M_PI / 3));

        cairo_matrix_t m, mt1, mr, mt2;
        cairo_matrix_init_translate(&mt1, -16, -16);
        cairo_matrix_init_translate(&mt2, 16, 16);
        cairo_matrix_init_rotate(&mr, angle + ss);
        cairo_matrix_multiply(&m, &mt1, &mr);
        cairo_matrix_multiply(&m, &m, &mt2);

        bmp.fill(0);
        svg->render(bmp.extbody(), ts::ivec2(0), &m);
    }

}


animation_c *animation_c::first = nullptr;
animation_c *animation_c::last = nullptr;
uint animation_c::allow_tick = 1;
animation_c::~animation_c()
{
    if (prev || next || first == this)
        LIST_DEL(this, first, last, prev, next);
}
void animation_c::redraw()
{
    for (redraw_request_s &r : rr)
        if (!r.engine.expired())
            r.engine->redraw(&r.rr);
    rr.clear();
}

animation_c * animation_c::do_tick()
{
    animation_c *n = next;

    if (rr.size() == 0)
    {
        unregister_animation();
        return n;
    }

    if (rr.get(rr.size() - 1).engine.expired())
        rr.remove_fast(rr.size() - 1);

    if (animation_tick())
        redraw();

    return n;
}

void animation_c::register_animation(rectengine_root_c *e, const ts::irect &ar)
{
    if (!prev && !next && first != this)
    {
        LIST_ADD(this, first, last, prev, next);
        just_registered();
    }

    redraw_request_s *er = nullptr;
    for (redraw_request_s &r : rr)
    {
        if (r.engine.get() == e)
        {
            r.rr.combine(ar);
            return;
        }
        if (r.engine.expired())
            er = &r;
    }
    redraw_request_s &r = er ? *er : rr.add();
    r.engine = e;
    r.rr = ar;
}

void animation_c::unregister_animation()
{
    if (prev || next || first == this)
    {
        LIST_DEL_CLEAR(this, first, last, prev, next);
    }
}

void animation_c::tick()
{
    if (!allow_tick) return;
    for (animation_c *a = first; a; )
        a = a->do_tick();
}
