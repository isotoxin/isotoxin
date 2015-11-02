#include "rectangles.h"


namespace 
{

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

    void draw_circle( ts::drawable_bitmap_c &bmp, float cx, float cy, float r, ts::TSCOLOR col )
    {
        int y0 = lround(cy - r - 1);
        int y1 = lround(cy + r + 1);
        int x0 = lround(cx - r - 1);
        int x1 = lround(cx + r + 2);
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
                    int alpha = lround(k*255.0f);
                    if (alpha == 255)
                        bmp.ARGBPixel(x, y, col);
                    else
                        bmp.ARGBPixel(x, y, col, alpha);
                }
            }
        }
    }
    void draw_ring(ts::drawable_bitmap_c &bmp, float cx, float cy, float r0, float r1, ts::TSCOLOR col)
    {
        int y0 = lround(cy - r1 - 1);
        int y1 = lround(cy + r1 + 1);
        int x0 = lround(cx - r1 - 1);
        int x1 = lround(cx + r1 + 1);
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
                    bmp.ARGBPixel(x, y, col, lround(k*255.0f));
            }
        }
    }

    void draw_shadow(ts::drawable_bitmap_c &bmp, float cx, float cy, float r0, float r1, ts::TSCOLOR col0, ts::TSCOLOR col1)
    {
        int y0 = lround(cy - r1 - 1);
        int y1 = lround(cy + r1 + 1);
        int x0 = lround(cx - r1 - 1);
        int x1 = lround(cx + r1 + 1);
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
            ts::vec2 p( 0, y + 0.5f );
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

    ts::vec2 parsevec2f(const ts::asptr &s, const ts::vec2 &def)
    {
        ts::vec2 v(def);
        typedef decltype(v.x) ivt;
        ivt * hackptr = (ivt *)&v;
        ivt * hackptr_end = hackptr + 4;
        for (ts::token<char> t(s, ','); t && hackptr < hackptr_end; ++t, ++hackptr)
        {
            *hackptr = t->as_num<ivt>(0);
        }
        return v;
    }

    ts::ivec2 as_ivec(const ts::vec2 &v)
    {
        return ts::ivec2( lround(v.x), lround(v.y) );
    }

struct gb_circle_s : public generated_button_data_s
{
    gb_circle_s(const ts::abp_c *gen)
    {

        ts::TSCOLOR col[button_desc_s::numstates];
        col[button_desc_s::NORMAL] = ts::parsecolor<char>( gen->get_string(CONSTASTR("color")), ts::ARGB(255,255,255) );
        col[button_desc_s::HOVER] = ts::parsecolor<char>( gen->get_string(CONSTASTR("color-hover")), ts::PREMULTIPLY(col[button_desc_s::NORMAL], 1.1f) );
        col[button_desc_s::PRESS] = ts::parsecolor<char>( gen->get_string(CONSTASTR("color-press")), col[button_desc_s::HOVER] );
        col[button_desc_s::DISABLED] = ts::parsecolor<char>( gen->get_string(CONSTASTR("color-hover")), ts::GRAYSCALE(col[button_desc_s::NORMAL]) );
        
        float r = gen->get_string(CONSTASTR("radius")).as_float(16.0f);
        bool border = gen->get_string(CONSTASTR("border")).as_int() != 0;
        bool shadow = gen->get_string(CONSTASTR("shadow")).as_int() != 0;
        float border_add_size = border ? gen->get_string(CONSTASTR("border-add-size")).as_float() : 0;
        float border_width = border ? gen->get_string(CONSTASTR("border-width")).as_float() : 0;
        ts::vec2 shadowshift = shadow ? parsevec2f( gen->get_string(CONSTASTR("shadow-shift")), ts::vec2(2) ) : ts::vec2(0);
        float shadow_radius_in = shadow ? gen->get_string(CONSTASTR("shadow-radius-in")).as_float(0) : 0;
        float shadow_radius_out = shadow ? gen->get_string(CONSTASTR("shadow-radius-out")).as_float(0) : 0;
        
        int margin_right = gen->get_string(CONSTASTR("margin-right")).as_int(0);

        ts::irect one_image_rect(0,lround(r * 2)); one_image_rect.rb.x += 2;
        ts::irect shadowr( one_image_rect.center() - ts::ivec2(lround(shadow_radius_out)), one_image_rect.center() + ts::ivec2(lround(shadow_radius_out)) );
        one_image_rect.combine( shadowr + as_ivec(shadowshift) );
        one_image_rect.combine( shadowr - as_ivec(shadowshift) );
        one_image_rect.combine( ts::irect(one_image_rect.center() - ts::ivec2(lround(r + border_add_size + border_width)), one_image_rect.center() + ts::ivec2(lround(r + border_add_size + border_width))) );

        ts::ivec2 one_image_size = one_image_rect.size();
        src.create( ts::ivec2((one_image_size.x + margin_right)* 4, one_image_size.y) );
        src.fill(0);

        ts::TSCOLOR bcol = border ? ts::parsecolor<char>(gen->get_string(CONSTASTR("border-color")), ts::ARGB(0, 0, 0)) : 0;

        ts::TSCOLOR shadow_col_in = shadow ? ts::parsecolor<char>(gen->get_string(CONSTASTR("shadow-color-in")), ts::ARGB(0, 0, 0)) : 0;
        ts::TSCOLOR shadow_col_out = shadow ? ts::parsecolor<char>(gen->get_string(CONSTASTR("shadow-color-out")), ts::ARGB(0, 0, 0, 0)) : 0;

        ts::wstr_c imgname = to_wstr(gen->get_string(CONSTASTR("image")));
        ts::bmpcore_exbody_s img; if (!imgname.is_empty()) img = get_image(imgname.as_sptr());

        for(int i=0;i<button_desc_s::numstates;++i)
        {
            int shiftx = (one_image_size.x + margin_right) * i;
            float k = 1.0f;
            if (i == button_desc_s::PRESS) k = gen->get_string(CONSTASTR("k-press")).as_float(1.0f);

            if (shadow)
            {
                ts::vec2 ss = i == button_desc_s::PRESS ? parsevec2f( gen->get_string(CONSTASTR("shadow-shift-press")), ts::vec2(shadowshift) ) : shadowshift;

                if (shadow_radius_in == shadow_radius_out) //-V550
                    draw_circle(src, (float)(one_image_size.x) * 0.5f + ss.x + shiftx, (float)(one_image_size.y) * 0.5f + ss.y,shadow_radius_in * k, shadow_col_in);
                else
                    draw_shadow(src, (float)(one_image_size.x) * 0.5f + ss.x + shiftx, (float)(one_image_size.y) * 0.5f + ss.y, shadow_radius_in * k, shadow_radius_out * k, shadow_col_in, shadow_col_out);
            }

            if (ts::ALPHA(col[i]))
                draw_circle(src, (float)(one_image_size.x) * 0.5f + shiftx, (float)(one_image_size.y) * 0.5f, r * k, col[i]);

            if (border)
            {
                draw_ring(src, (float)(one_image_size.x) * 0.5f + shiftx, (float)(one_image_size.y) * 0.5f, (r + border_add_size) * k, (r + border_add_size + border_width) * k, bcol);
            }

            if (img())
            {
                src.alpha_blend( (one_image_size - img.info().sz) / 2 + ts::ivec2(shiftx, 0), img );
            }
        }

        src.premultiply();
    }



    /*virtual*/ void setup(button_desc_s &desc) override
    {
        __super::setup(desc);
    }
};

}









generated_button_data_s *generated_button_data_s::generate(const ts::abp_c *gen)
{
    ts::str_c shape = gen->get_string(CONSTASTR("shape"));
    if (shape.equals(CONSTASTR("circle")))
        return TSNEW(gb_circle_s, gen);


    return nullptr;
}

/*virtual*/ void generated_button_data_s::setup(button_desc_s &desc)
{
    desc.genb.reset(this);

    int w = src.info().sz.x / button_desc_s::numstates;
    desc.size.x = w;
    desc.size.y = src.info().sz.y;
    for (int i = 0; i < button_desc_s::numstates; ++i)
    {
        desc.rects[i].lt = ts::ivec2(w * i, 0);
        desc.rects[i].rb = ts::ivec2(w * i + w, desc.size.y);
    }
}