#include "rectangles.h"

menu_anchor_s::menu_anchor_s(bool setup_by_mousepos, relpos_e rp):rect(0), relpos(rp)
{
    if (setup_by_mousepos && gui)
    {
        rect.lt = gui->get_cursor_pos();
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
    size = lround(ts::tmax<float>( k * viewsize, minsbsize ));
    float sbmove = float(viewsize - size);


    pos = lround(-float(shift) / datamove * sbmove);

}

int calcsbshift( int pos, int viewsize, int datasize )
{
    if (viewsize >= datasize)
        return 0;

    float k = float(viewsize) / float(datasize);
    float datamove = float(datasize - viewsize);
    int size = lround(ts::tmax<float>(k * viewsize, minsbsize));
    float sbmove = float(viewsize - size);

    ASSERT(sbmove > 0);

    return lround(-float(pos) / sbmove * datamove);
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
    for (int i = text.find_pos('<'); i >= 0; i = text.find_pos(i, '<'))
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


process_animation_s::process_animation_s(int sz):sz(sz)
{
    DWORD col = RGB(150,150,150);// RGB(IRND(100), IRND(100), IRND(100) + 155);
    pen = CreatePen(PS_SOLID, sz * 2 / 32, col);
    //pen2 = CreatePen(PS_SOLID, sz / 32, RGB(0, 0, 0));
    pen2 = CreatePen(PS_SOLID, 1, col);
    br = CreateSolidBrush(col);
    br2 = CreateSolidBrush(RGB(255,0,255));
    pen3 = CreatePen(PS_SOLID, 1, RGB(255, 0, 255));
    LOGBRUSH lbr;
    lbr.lbStyle = BS_HOLLOW;
    br3 = CreateBrushIndirect( &lbr );
    angle = 0;
    devia = 0;
}

void process_animation_s::restart()
{
    angle = 0;
    devia = 0;
    nexttime = ts::Time::current() + 50;
}

process_animation_s::~process_animation_s()
{
    DeleteObject(pen);
    DeleteObject(pen2);
    DeleteObject(pen3);
    DeleteObject(br);
    DeleteObject(br2);
    DeleteObject(br3);
}

static void draw_circle(HDC dc, const ts::ivec2 &p, int r)
{
    Ellipse(dc, p.x - r, p.y - r, p.x + r, p.y + r);

}

void process_animation_s::render(HDC dc, const ts::ivec2& shift)
{
    ts::Time ct = ts::Time::current();
    if ((ct - nexttime) > 0)
    {
        nexttime += 50;
        angle = ts::angle_norm<float>(angle + 0.01f);
        devia = ts::angle_norm<float>(devia + 0.13f);

        if((ct - nexttime) > 1000)
            nexttime = ct;
    }


    float r1 = sz / 4.6f;
    //float r2 = sz / 4.f;
    float r2 = sz / 3.1f;

    ts::ivec2 pts[N];
    ts::ivec2 pts2[N];

    for (int i = 0; i < N; ++i)
    {
        float a = (float)(M_PI * 2 / N*i);

        float ss = (float)(sin(devia) * (2 * M_PI / 3));
        float s = sin(angle+a+ss);
        float c = cos(angle + a +ss);

        //float s2 = sin(angles[i]-0.3f);
        //float c2 = cos(angles[i] - 0.3f);

        pts[i] = ts::ivec2(ts::lround(s * r1), lround(c * r1));
        pts2[i] = ts::ivec2(ts::lround(s * r2), lround(c * r2));


        pts[i] += shift + ts::ivec2(sz / 2);
        pts2[i] += shift + ts::ivec2(sz / 2);

        //MoveToEx(dc, pos.x, pos.y, 0);
        //LineTo(dc, pos.x, pos.y);
        //LineTo(dc, pos2.x, pos2.y);
    }

    HGDIOBJ oo = SelectObject(dc, pen2);

    /*
    int pri = N - 1;
    for (int i = 0; i < N; ++i)
    {
        MoveToEx(dc, pts[pri].x, pts[pri].y, 0);
        LineTo(dc, pts[i].x, pts[i].y);
        pri = i;
    }
    */


    HGDIOBJ obr = SelectObject(dc, br);
    for (int i = 0; i < N; ++i)
    {
        //MoveToEx(dc, pts[i].x, pts[i].y, 0);
        //LineTo(dc, pts[i].x, pts[i].y);

        draw_circle(dc,pts[i], ts::lround(sz/3.3f));

    }

    SelectObject(dc, br2);
    SelectObject(dc, pen3);
    for (int i = 0; i < N; ++i)
    {
        draw_circle(dc, pts2[i], ts::lround(sz / 5.0f));
    }


    draw_circle(dc, shift + ts::ivec2(sz / 2), sz/12);

    SelectObject(dc, pen);
    SelectObject(dc, br3);

    draw_circle(dc, shift + ts::ivec2(sz / 2), ts::lround(sz / 3.7));

    SelectObject(dc, pen3);
    for (int i = 0; i < N; ++i)
    {
        draw_circle(dc, pts2[i], ts::lround(sz / 5.0f));
    }


    SelectObject(dc, obr);
    SelectObject(dc, oo);
}


process_animation_bitmap_s::process_animation_bitmap_s():process_animation_s(64)
{
    bmpr.create(ts::ivec2(sz));
    bmp.create(ts::ivec2(sz/2));
}

void process_animation_bitmap_s::render()
{
    bmpr.fill( ts::ARGB(0,0,0) );
    __super::render( bmpr.DC(), ts::ivec2(0) );
    
    byte *b = bmpr.body();
    for(int y=0;y<sz;++y, b += bmpr.info().pitch)
    {
        ts::TSCOLOR *c = (ts::TSCOLOR *)b;
        for (int x = 0; x < sz; ++x, ++c)
        {
            ts::TSCOLOR cc = *c;
            if ((cc & 0x00FFFFFF) == 0x00FF00FF)
            {
                *c = 0;
            } else
            {
                cc = (cc & 0x00FFFFFFu) | ((255 - ts::ALPHA(cc)) << 24);
                *c = cc;
            }
        }
    }
    bmpr.shrink_2x_to( bmp.extbody(), ts::ivec2(0), ts::ivec2(sz) );
    bmpr.premultiply();
}
