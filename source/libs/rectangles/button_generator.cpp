#include "rectangles.h"


namespace 
{

#if 0
    bool    square_circle_intersect(float halfsize, double sqradius, const ts::vec2& dst)
    {
        double d = 0.0f;
        ts::vec2 m = ts::vec2(fabsf(dst.x), fabsf(dst.y)) - ts::vec2(halfsize, halfsize);
        if (m.x > 0.0f) d += m.x * m.x;
        if (m.y > 0.0f) d += m.y * m.y;

        return (d <= sqradius);
    };

    bool square_inside_circle(float halfsize, double sqradius, const ts::vec2& dst)
    {
        float sqd = (dst - ts::vec2(-halfsize, -halfsize)).sqlen();
        if (sqd > sqradius) return false;
        sqd = (dst - ts::vec2(halfsize, -halfsize)).sqlen();
        if (sqd > sqradius) return false;
        sqd = (dst - ts::vec2(halfsize, halfsize)).sqlen();
        if (sqd > sqradius) return false;
        sqd = (dst - ts::vec2(-halfsize, halfsize)).sqlen();
        return sqd <= sqradius;
    }


#define ACCURACY 8

    float    square_circle_intersect_area(float halfsize, double sqradius, const ts::vec2& dst) // returns [0...1] of square area
    {
        if (square_inside_circle(halfsize, sqradius, dst))
            return 1.0f;

        if (square_circle_intersect(halfsize, sqradius, dst))
        {
            float nhs = halfsize / ACCURACY;
            float nhs2 = halfsize / (ACCURACY / 2);
            int n = 0;
            for (int x = -(ACCURACY/2); x < (ACCURACY/2); ++x)
            {
                ts::vec2 hp( dst.x - nhs2 * x - nhs, 0 );
                for (int y = -(ACCURACY/2); y < (ACCURACY/2); ++y)
                {
                    hp.y = dst.y - nhs2 * y - nhs;
                    if (square_circle_intersect(nhs, sqradius, hp))
                        ++n;
                }
            }
            return (float)n * (1.0f/(ACCURACY*ACCURACY));
        }

        return 0;
    }

    float    square_ring_intersect_area(float halfsize, double sqradius1, double sqradius2, const ts::vec2& dst) // returns [0...1] of square area
    {
        bool r = square_circle_intersect(halfsize, sqradius2, dst) && !square_inside_circle(halfsize, sqradius1, dst);
        if (r)
        {
            float nhs = halfsize / ACCURACY;
            float nhs2 = halfsize / (ACCURACY / 2);
            int n = 0;
            for (int x = -(ACCURACY/2); x < (ACCURACY/2); ++x)
            {
                ts::vec2 hp(dst.x - nhs2 * x - nhs, 0);
                for (int y = -(ACCURACY/2); y < (ACCURACY/2); ++y)
                {
                    hp.y = dst.y - nhs2 * y - nhs;
                    if (square_circle_intersect(nhs, sqradius2, hp) && !square_inside_circle(nhs, sqradius1, hp))
                        ++n;
                }
            }
            return (float)n * (1.0f / (ACCURACY*ACCURACY));
        }

        return 0;
    }

    void draw_circle( ts::bitmap_c &bmp, float cx, float cy, float r, ts::TSCOLOR col )
    {
        int y0 = ts::lround(cy - r - 1);
        int y1 = ts::lround(cy + r + 1);
        int x0 = ts::lround(cx - r - 1);
        int x1 = ts::lround(cx + r + 2);
        if (x0 < 0) x0 = 0;
        if (x1 > bmp.info().sz.x) x1 = bmp.info().sz.x;
        if (y0 < 0) y0 = 0;
        if (y1 > bmp.info().sz.y) y1 = bmp.info().sz.y;
        float r2 = r * r;
        for(int y = y0; y<y1; ++y)
        {
            for(int x = x0; x<x1; ++x)
            {
                float k = square_circle_intersect_area(0.5f,r2, ts::vec2(cx,cy) - ts::vec2(x+0.5f,y+0.5f));
                if (k>0)
                {
                    int alpha = ts::lround(k*255.0f);
                    if (alpha == 255)
                        bmp.ARGBPixel(x, y, col);
                    else
                        bmp.ARGBPixel(x, y, col, alpha);
                }
            }
        }
    }
    void draw_ring(ts::bitmap_c &bmp, float cx, float cy, float r0, float r1, ts::TSCOLOR col)
    {
        int y0 = ts::lround(cy - r1 - 1);
        int y1 = ts::lround(cy + r1 + 1);
        int x0 = ts::lround(cx - r1 - 1);
        int x1 = ts::lround(cx + r1 + 1);
        if (x0 < 0) x0 = 0;
        if (x1 > bmp.info().sz.x) x1 = bmp.info().sz.x;
        if (y0 < 0) y0 = 0;
        if (y1 > bmp.info().sz.y) y1 = bmp.info().sz.y;
        float r0_2 = r0 * r0;
        float r1_2 = r1 * r1;
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                float k = square_ring_intersect_area(0.5f, r0_2, r1_2, ts::vec2(cx, cy) - ts::vec2(x + 0.5f, y + 0.5f));
                if (k>0)
                    bmp.ARGBPixel(x, y, col, ts::lround(k*255.0f));
            }
        }
    }

    void draw_shadow(ts::bitmap_c &bmp, float cx, float cy, float r0, float r1, ts::TSCOLOR col0, ts::TSCOLOR col1)
    {
        int y0 = ts::lround(cy - r1 - 1);
        int y1 = ts::lround(cy + r1 + 1);
        int x0 = ts::lround(cx - r1 - 1);
        int x1 = ts::lround(cx + r1 + 1);
        if (x0 < 0) x0 = 0;
        if (x1 > bmp.info().sz.x) x1 = bmp.info().sz.x;
        if (y0 < 0) y0 = 0;
        if (y1 > bmp.info().sz.y) y1 = bmp.info().sz.y;
        float r0_2 = r0 * r0;
        float r1_2 = r1 * r1;
        float rr = 1.0f / (r1 - r0);
        ts::vec2 c(cx, cy);
        for (int y = y0; y < y1; ++y)
        {
            ts::vec2 p(0, y + 0.5f);
            for (int x = x0; x < x1; ++x)
            {
                p.x = x + 0.5f;
                float sqrad = (p - c).sqlen();
                if (sqrad > r1_2) continue;
                if (sqrad < r0_2)
                    bmp.ARGBPixel(x, y, col0, 255);
                else
                {
                    float k = (sqrt(sqrad) - r0) * rr;

                    ts::TSCOLOR col = ts::LERPCOLOR(col0, col1, k);
                    bmp.ARGBPixel(x, y, col, 255);
                }
            }
        }
    }

    ts::ivec2 as_ivec(const ts::vec2 &v)
    {
        return ts::ivec2( ts::lround(v.x), ts::lround(v.y) );
    }

struct gb_circle_s : public generated_button_data_s
{
    gb_circle_s(const ts::abp_c *gen, const colors_map_s &colsmap)
    {

        ts::TSCOLOR col[button_desc_s::numstates];
        col[button_desc_s::NORMAL] = colsmap.parse( gen->get_string(CONSTASTR("color")), ts::ARGB(255,255,255) );
        col[button_desc_s::HOVER] = colsmap.parse( gen->get_string(CONSTASTR("color-hover")), ts::PREMULTIPLY(col[button_desc_s::NORMAL], 1.1f) );
        col[button_desc_s::PRESS] = colsmap.parse( gen->get_string(CONSTASTR("color-press")), col[button_desc_s::HOVER] );
        col[button_desc_s::DISABLED] = colsmap.parse( gen->get_string(CONSTASTR("color-hover")), ts::GRAYSCALE(col[button_desc_s::NORMAL]) );
        
        float r = gen->get_string(CONSTASTR("radius")).as_float(16.0f);
        bool border = gen->get_string(CONSTASTR("border")).as_int() != 0;
        bool shadow = gen->get_string(CONSTASTR("shadow")).as_int() != 0;
        float border_add_size = border ? gen->get_string(CONSTASTR("border-add-size")).as_float() : 0;
        float border_width = border ? gen->get_string(CONSTASTR("border-width")).as_float() : 0;
        ts::vec2 shadowshift = shadow ? ts::parsevec2f( gen->get_string(CONSTASTR("shadow-shift")), ts::vec2(2) ) : ts::vec2(0);
        float shadow_radius_in = shadow ? gen->get_string(CONSTASTR("shadow-radius-in")).as_float(0) : 0;
        float shadow_radius_out = shadow ? gen->get_string(CONSTASTR("shadow-radius-out")).as_float(0) : 0;
        
        int margin_right = gen->get_string(CONSTASTR("margin-right")).as_int(0);

        ts::irect one_image_rect(0,ts::lround(r * 2)); one_image_rect.rb.x += 2;
        ts::irect shadowr( one_image_rect.center() - ts::ivec2(ts::lround(shadow_radius_out)), one_image_rect.center() + ts::ivec2(ts::lround(shadow_radius_out)) );
        one_image_rect.combine( shadowr + as_ivec(shadowshift) );
        one_image_rect.combine( shadowr - as_ivec(shadowshift) );
        one_image_rect.combine( ts::irect(one_image_rect.center() - ts::ivec2(ts::lround(r + border_add_size + border_width)), one_image_rect.center() + ts::ivec2(ts::lround(r + border_add_size + border_width))) );

        ts::ivec2 one_image_size = one_image_rect.size();
        src.create_ARGB( ts::ivec2(one_image_size.x + margin_right, one_image_size.y * 4) );
        src.fill(0);

        ts::TSCOLOR bcol = border ? colsmap.parse(gen->get_string(CONSTASTR("border-color")), ts::ARGB(0, 0, 0)) : 0;

        ts::TSCOLOR shadow_col_in = shadow ? colsmap.parse(gen->get_string(CONSTASTR("shadow-color-in")), ts::ARGB(0, 0, 0)) : 0;
        ts::TSCOLOR shadow_col_out = shadow ? colsmap.parse(gen->get_string(CONSTASTR("shadow-color-out")), ts::ARGB(0, 0, 0, 0)) : 0;

        ts::wstr_c imgname = to_wstr(gen->get_string(CONSTASTR("image")));
        ts::bmpcore_exbody_s img; if (!imgname.is_empty()) img = get_image(imgname.as_sptr());

        for(int i=0;i<button_desc_s::numstates;++i)
        {
            int shifty = one_image_size.y * i;
            float k = 1.0f;
            if (i == button_desc_s::PRESS) k = gen->get_string(CONSTASTR("k-press")).as_float(1.0f);

            if (shadow)
            {
                ts::vec2 ss = i == button_desc_s::PRESS ? ts::parsevec2f( gen->get_string(CONSTASTR("shadow-shift-press")), ts::vec2(shadowshift) ) : shadowshift;

                if (shadow_radius_in == shadow_radius_out) //-V550
                    draw_circle(src, (float)(one_image_size.x) * 0.5f + ss.x, (float)(one_image_size.y) * 0.5f + ss.y + shifty, shadow_radius_in * k, shadow_col_in);
                else
                    draw_shadow(src, (float)(one_image_size.x) * 0.5f + ss.x, (float)(one_image_size.y) * 0.5f + ss.y + shifty, shadow_radius_in * k, shadow_radius_out * k, shadow_col_in, shadow_col_out);
            }

            if (ts::ALPHA(col[i]))
                draw_circle(src, (float)(one_image_size.x) * 0.5f, (float)(one_image_size.y) * 0.5f + shifty, r * k, col[i]);

            if (border)
            {
                draw_ring(src, (float)(one_image_size.x) * 0.5f, (float)(one_image_size.y) * 0.5f + shifty, (r + border_add_size) * k, (r + border_add_size + border_width) * k, bcol);
            }

            if (img())
            {
                src.alpha_blend( (one_image_size - img.info().sz) / 2 + ts::ivec2(0, shifty), img );
            }
        }

        src.premultiply();
    }



    /*virtual*/ void setup(button_desc_s &desc) override
    {
        super::setup(desc);
    }
};
#endif


struct gb_svg_s : public generated_button_data_s
{
    rsvg_svg_c *asvg = nullptr;
    ts::ivec2 rshift;
    virtual rsvg_svg_c *get_svg(ts::ivec2 &s) { s = rshift;  rsvg_svg_c *r = asvg; asvg = nullptr; return r; }

    gb_svg_s(const ts::abp_c *gen, const colors_map_s &colsmap, const ts::str_c &svgs, bool one_face, const ts::asptr *ifilter)
    {
        ts::TSCOLOR col[button_desc_s::numstates];
        float shift[button_desc_s::numstates] = {};
        ts::str_c ins[button_desc_s::numstates];

        col[button_desc_s::NORMAL] = colsmap.parse(gen->get_string(CONSTASTR("color")), ts::ARGB(255, 255, 255));
        shift[button_desc_s::NORMAL] = gen->get_string(CONSTASTR("shift")).as_float(0.0f);
        ins[button_desc_s::NORMAL] = gen->get_string(CONSTASTR("insert"));

        bool gen_disabled = false;
        if (!one_face)
        {
            col[button_desc_s::HOVER] = colsmap.parse(gen->get_string(CONSTASTR("color-hover")), ts::PREMULTIPLY(col[button_desc_s::NORMAL], 1.1f));
            col[button_desc_s::PRESS] = colsmap.parse(gen->get_string(CONSTASTR("color-press")), col[button_desc_s::HOVER]);
            col[button_desc_s::DISABLED] = colsmap.parse(gen->get_string(CONSTASTR("color-disabled")), ts::GRAYSCALE(col[button_desc_s::NORMAL]));

            if ( col[button_desc_s::DISABLED] != col[button_desc_s::NORMAL] )
                gen_disabled = true;

            shift[button_desc_s::HOVER] = shift[button_desc_s::NORMAL];
            shift[button_desc_s::PRESS] = gen->get_string(CONSTASTR("shift-press")).as_float(0.0f);
            shift[button_desc_s::DISABLED] = shift[button_desc_s::NORMAL];

            if (!gen_disabled && shift[button_desc_s::DISABLED] != shift[button_desc_s::NORMAL])
                gen_disabled = true;

            ins[button_desc_s::HOVER] = gen->get_string(CONSTASTR("insert-hover"));
            ins[button_desc_s::PRESS] = gen->get_string(CONSTASTR("insert-press"));
            ins[button_desc_s::DISABLED] = gen->get_string(CONSTASTR("insert-disabled"));

            if (!gen_disabled && ins[button_desc_s::DISABLED] != ins[button_desc_s::NORMAL])
                gen_disabled = true;

        }

        ts::bitmap_c face_surface;
        ts::str_c rs;
        ts::irect vr;

        num_states = one_face ? 1 : ( gen_disabled ? button_desc_s::numstates : button_desc_s::numstates - 1);

        for (int i = 0; i < num_states; ++i)
        {
            ts::str_c svg(svgs);

            if (ifilter)
            {
                ts::str_c keep_b( CONSTASTR("{if-"), ifilter[i], CONSTASTR("}") );
                ts::str_c keep_e(CONSTASTR("{fi-"), ifilter[i], CONSTASTR("}"));
                svg.replace_all(keep_b, ts::asptr());
                svg.replace_all(keep_e, ts::asptr());

                for (int index = 0;;)
                {
                    int s = svg.find_pos(index, CONSTASTR("{if-"));
                    if (s < 0)
                        break;
                    index = s;
                    int s1 = svg.find_pos(s, '}');
                    if (s1 < 0)
                        break;
                    keep_e.set_length(4).append( svg.substr(s+4,s1+1) );
                    int s2 = svg.find_pos(s1, keep_e);
                    if (s2 < 0)
                        break;
                    svg.cut(s,s2+keep_e.get_length()-s);

                }
            }

            
            svg.replace_all(CONSTASTR("[insert]"), ins[i]);

            ts::str_c ct = make_color(col[i]);
            svg.replace_all( CONSTASTR("[color]"), ct );

            for(;;)
            {
                int cri = svg.find_pos(CONSTASTR("=\"##"));
                if (cri >= 0)
                {
                    int y = svg.find_pos(cri + 4, '\"');
                    if (y > cri)
                    {
                        ts::TSCOLOR c = colsmap.parse(svg.substr(cri + 2, y), ts::ARGB(0, 0, 0));
                        rs.clear();
                        rs.append_as_hex(ts::RED(c));
                        rs.append_as_hex(ts::GREEN(c));
                        rs.append_as_hex(ts::BLUE(c));
                        if (ts::ALPHA(c) != 0xff)
                            rs.append_as_hex(ts::ALPHA(c));
                        svg.replace(cri + 3, y - cri - 3, rs);
                        continue;
                    }
                }
                break;
            }

            float k = 1.0f;
            if (i == button_desc_s::PRESS) k = gen->get_string(CONSTASTR("k-press")).as_float(1.0f);
            svg.replace_all( CONSTASTR("[scale]"), ts::amake<float>(k) );
            svg.replace_all(CONSTASTR("[shift]"), ts::amake<float>(shift[i]));

            if (rsvg_svg_c *n = rsvg_svg_c::build_from_xml(svg.str()))
            {
                if ( face_surface.info().sz != n->size() )
                    face_surface.create_ARGB(n->size());
                face_surface.fill(0);
                n->render( face_surface.extbody() );
                if ( i == 0 )
                {
#ifdef _DEBUG
                    if (gen->get_int(CONSTASTR("break")))
                        DEBUG_BREAK();
                    if (const ts::abp_c *f = gen->get(CONSTASTR("save")))
                        face_surface.save_as_png( to_wstr(f->as_string()) );
#endif // _DEBUG

                    vr = face_surface.calc_visible_rect();

                    if ( !vr )
                    {
                        n->release();
                        src.clear();
                        return;
                    }

                    if ( const ts::abp_c *center = gen->get(CONSTASTR("center")))
                    {
                        ts::vec2 c = ts::parsevec2f( center->as_string(), ts::vec2(0) );
                        if (c.x > 0)
                        {
                            float cr = vr.rb.x - c.x;
                            float cl = c.x - vr.lt.x;
                            if (cl < cr)
                                vr.lt.x = ts::lround(c.x - cr);
                            else if (cl > cr)
                                vr.rb.x = ts::lround(c.x + cl);
                        }

                        if (c.y > 0)
                        {
                            float cb = vr.rb.y - c.y;
                            float ctop = c.y - vr.lt.y;
                            if (ctop < cb)
                                vr.lt.y = ts::lround(c.y - cb);
                            else if (ctop > cb)
                                vr.rb.y = ts::lround(c.y + ctop);
                        }

                    }
                    if (const ts::abp_c *addw = gen->get(CONSTASTR("add-width")))
                    {
                        ts::ivec2 addwi = ts::parsevec2( addw->as_string(), ts::ivec2(0), true );
                        vr.lt.x -= addwi.x;
                        vr.rb.x += addwi.y;
                    }
                    if (const ts::abp_c *addw = gen->get(CONSTASTR("add-height")))
                    {
                        ts::ivec2 addhi = ts::parsevec2(addw->as_string(), ts::ivec2(0), true);
                        vr.lt.y -= addhi.x;
                        vr.rb.y += addhi.y;
                    }
                    if (const ts::abp_c *cbsize = gen->get(CONSTASTR("size"))) // h-center / v-center size
                    {
                        ts::ivec2 cbsizei = ts::parsevec2(cbsize->as_string(), ts::ivec2(0), true);
                        vr.lt.x = (vr.lt.x + vr.rb.x - cbsizei.x) / 2;
                        vr.rb.x = vr.lt.x + cbsizei.x;
                        vr.lt.y = (vr.lt.y + vr.rb.y - cbsizei.y) / 2;
                        vr.rb.y = vr.lt.y + cbsizei.y;
                    }
                    if (const ts::abp_c *cbsize = gen->get(CONSTASTR("cbsize"))) // h-center / v-bottom size
                    {
                        ts::ivec2 cbsizei = ts::parsevec2(cbsize->as_string(), ts::ivec2(0), true);
                        vr.lt.x = (vr.lt.x + vr.rb.x - cbsizei.x)/2;
                        vr.rb.x = vr.lt.x + cbsizei.x;
                        vr.lt.y = vr.rb.y - cbsizei.y;
                    }
                    vr.intersect(ts::irect(0, face_surface.info().sz));
                    rshift = -vr.lt;
                    src.create_ARGB(ts::ivec2(vr.width(), vr.height() * num_states));
                }

#if 0
                for (int i = vr.lt.x; i < vr.rb.x; ++i)
                    face_surface.ARGBPixel(i, vr.lt.y, 0xff000000), face_surface.ARGBPixel(i, vr.rb.y-1, 0xff000000);
                for (int j = vr.lt.y; j < vr.rb.y; ++j)
                    face_surface.ARGBPixel(vr.lt.x, j, 0xff000000), face_surface.ARGBPixel(vr.rb.x-1, j, 0xff000000);
                for (int i = vr.lt.x; i < vr.rb.x; ++i)
                    for (int j = vr.lt.y; j < vr.rb.y; ++j)
                        if ((0xff000000 & face_surface.ARGBPixel(i, j)) == 0x01000000)
                            face_surface.ARGBPixel(i, j, 0xffff0000);
#endif

                src.copy( ts::ivec2(0, vr.height() * i), vr.size(), face_surface.extbody(), vr.lt );
                if (n->is_animated())
                    asvg = n;
                else
                    n->release();
            } else
            {
                src.clear();
                return;
            }
        }

        //src.premultiply(); // cairo generates already premultiplied images
    }
    ~gb_svg_s()
    {
        if (asvg)
            asvg->release();
    }
};

}









generated_button_data_s *generated_button_data_s::generate(const ts::abp_c *gen, const colors_map_s &colsmap, bool one_face, const ts::asptr *filter)
{
    if (const ts::abp_c *svg = gen->get(CONSTASTR("svg")))
    {
        gb_svg_s *g = TSNEW(gb_svg_s, gen, colsmap, svg->as_string(), one_face, filter);
        if (g->is_valid())
            return g;
        TSDEL(g);
    }

#if 0
    ts::str_c shape = gen->get_string(CONSTASTR("shape"));
    if (shape.equals(CONSTASTR("circle")))
        return TSNEW(gb_circle_s, gen, colsmap);
#endif

    return nullptr;
}

