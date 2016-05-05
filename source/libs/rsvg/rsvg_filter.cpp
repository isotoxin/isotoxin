#include "_precompiled.h"
#include "rsvg_internals.h"

//-V::550

struct rsvg_working_surf_s
{
    MOVABLE( true );
    ts::bitmap_c bmp;
    rsvg_working_surf_s(const ts::ivec2&sz)
    {
        bmp.create_ARGB(sz);
    }
};

static float get_value( float basv, const ts::asptr &s )
{
    if ( ts::pstr_c(s).get_last_char() == '%' )
    {
        float prc = ts::pstr_c(s).substr(0, s.l - 1).as_float();
        return basv * prc / 100.0f;
    }

    return ts::pstr_c(s).as_float();
}

static int surface_index(rsvg_load_context_s &ctx, const ts::asptr &s)
{
    if (ts::pstr_c(s).equals(CONSTASTR("SourceAlpha")))
        return RSVG_FILTER_SOURCE_ALPHA;
    if (ts::pstr_c(s).equals(CONSTASTR("SourceGraphic")))
        return RSVG_FILTER_SOURCE_GRAPHIC;

    return 2 + (int)ctx.targets.get_string_index( s );
}

bool load_filters(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    if (ts::pstr_c(node->name()).equals(CONSTASTR("filter")))
    {
        for (ts::rapidxml::attribute_iterator<char> ai(&*node), e; ai != e; ++ai)
        {
            ts::pstr_c attrname(ai->name());
            if (attrname.equals(CONSTASTR("id")))
            {
                rsvg_filters_group_c *f = TSNEW(rsvg_filters_group_c);
                ctx.filters[ts::str_c(ai->value())] = f;
                f->load(ctx, &*node);
                break;
            }
        }

        return true;
    }

    return false;
}

bool rsvg_filter_c::fix_source(rsvg_load_context_s &ctx, int &olds, int &news)
{
    if (source == target)
    {
        olds = source;
        source = 2 + (int)ctx.targets.get_string_index("#temp");
        news = source;
        return true;
    }
    return false;
}

bool rsvg_filter_c::load_s(rsvg_load_context_s &ctx, const ts::rapidxml::xml_attribute<char>& attr)
{
    if (ts::pstr_c(attr.name()).equals(CONSTASTR("in")))
    {
        source = surface_index(ctx, attr.value());
        return true;
    } else if (ts::pstr_c(attr.name()).equals(CONSTASTR("result")))
    {
        target = ts::tmax(0, surface_index(ctx, attr.value()));
        return true;
    }
    return false;
}

void rsvg_filters_group_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    offset = ctx.size * 10 / 100;
    size = ctx.size * 120 / 100;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("width")))
            size.x = ts::lround(get_value( (float)ctx.size.x, ai->value() ));
        else if (attrname.equals(CONSTASTR("height")))
            size.y = ts::lround(get_value((float)ctx.size.y, ai->value()));
        else if (attrname.equals(CONSTASTR("x")))
            offset.x = -ts::lround( get_value((float)ctx.size.x, ai->value()) );
        else if (attrname.equals(CONSTASTR("y")))
            offset.y = -ts::lround( get_value((float)ctx.size.y, ai->value()) );
    }

    if (offset.x < 0) offset.x = 0;
    if (offset.y < 0) offset.y = 0;

    ctx.targets.clear();
    int ss = RSVG_FILTER_SOURCE_GRAPHIC;

    for (ts::rapidxml::node_iterator<char> ni(node), e; ni != e; ++ni)
    {
        rsvg_filter_c *ff = nullptr;
        if (ts::pstr_c(ni->name()).equals(CONSTASTR("feGaussianBlur")))
        {
            rsvg_filter_gaussian_c *f = TSNEW(rsvg_filter_gaussian_c, ss);
            f->load(ctx, &*ni);
            filters.add(f);
            ff = f;

        } else if (ts::pstr_c(ni->name()).equals(CONSTASTR("feOffset")))
        {
            rsvg_filter_offset_c *f = TSNEW( rsvg_filter_offset_c, ss );
            f->load( ctx, &*ni );
            filters.add( f );
            ff = f;

        } else if (ts::pstr_c(ni->name()).equals(CONSTASTR("feMerge")))
        {
        
        } else if (ts::pstr_c(ni->name()).equals(CONSTASTR("feBlend")))
        {
            rsvg_filter_blend_c *f = TSNEW(rsvg_filter_blend_c, ss);
            f->load(ctx, &*ni);
            filters.add(f);
            ff = f;
        } else if (ts::pstr_c(ni->name()).equals(CONSTASTR("feComponentTransfer")))
        {
            rsvg_filter_ctransfer_c *f = TSNEW(rsvg_filter_ctransfer_c, ss);
            f->load(ctx, &*ni);
            filters.add(f);
            ff = f;
        }

        if (ff)
        {
            if (ff->get_target_index() == RSVG_FILTER_TARGRET_CURRENT)
                ss = RSVG_FILTER_TARGRET_CURRENT;
        }
    }

    // some filters require different source and target surfaces
    // now fix such filters
    for (ts::aint i = filters.size() - 1; i >= 0; --i )
    {
        rsvg_filter_c *f = filters.get(i);
        int olds, news;
        while ( f->fix(ctx, olds, news) )
        {
            for ( ts::aint j = i - 1; j >= 0; --j)
            {
                if ( filters.get(j)->repltarget(olds,news))
                    break;
            }
        }
    }

    numsurfaces += (int)ctx.targets.size();
}

void rsvg_filters_group_c::render( rsvg_node_c *node, const ts::ivec2 &pos, rsvg_cairo_surface_c &surf )
{
    ts::tmp_array_inplace_t< rsvg_working_surf_s, 0 >  surfs( numsurfaces );
    surfs.add_and_init( numsurfaces, size );
    surfs.get(0).bmp.fill(0); // make blank only source

    cairo_matrix_t m;
    cairo_get_matrix( surf.cr(), &m );

    rsvg_cairo_surface_c s( surfs.get(0).bmp.extbody(), &m );
    node->render( offset + pos, s, false ); // prepare source

    // do filters
    for( const rsvg_filter_c *f : filters )
        f->apply( surfs.begin(), &m );

    ts::bitmap_c &rslt = surfs.get(RSVG_FILTER_TARGRET_CURRENT).bmp;
    ts::irect rr(0, surf.extb().info().sz);
    rr.intersect(ts::irect(0, rslt.info().sz - offset));
    if (rr)
    {
        //rslt.premultiply(rr);
        ts::img_helper_alpha_blend_pm(surf.extb()(), surf.extb().info().pitch, rslt.body(offset), rslt.info(rr), 255);
    } else
    {
        ts::img_helper_fill(surf.extb()(), surf.extb().info(), 0);
    }
}


/*virtual*/ void rsvg_filter_offset_c::load( rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node )
{
    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("dx")))
            offset.x = get_value((float)ctx.size.x, ai->value());
        if (attrname.equals(CONSTASTR("dy")))
            offset.y = get_value((float)ctx.size.y, ai->value());
        else if (load_s(ctx, *ai)) {}
    }

}

/*virtual*/ bool rsvg_filter_offset_c::fix(rsvg_load_context_s &ctx, int &olds, int &news)
{
    return fix_source(ctx, olds, news);
}

/*virtual*/ void rsvg_filter_offset_c::apply(rsvg_working_surf_s *surfs, const cairo_matrix_t *m) const
{
    ts::ivec2 xoffset( ts::lround( m->xx * offset.x + m->xy * offset.y ),
                       ts::lround( m->yx * offset.x + m->yy * offset.y ) );

    ts::bitmap_c &src = surfs[ts::tmax(0,source)].bmp;
    ts::bitmap_c &tgt = surfs[target].bmp;

    const ts::TSCOLOR newcolor = 0; // ts::ARGB(255, 0, 255);

    ts::irect srcr( xoffset, xoffset + src.info().sz );
    srcr.intersect( ts::irect(0, tgt.info().sz) );
    if (srcr)
        tgt.copy(srcr.lt, srcr.size(), src.extbody(), ts::ivec2(0));
    else
    {
        tgt.fill(newcolor);
        return;
    }

    if (source < 0)
    {
        // now kill color
        ts::ivec2 sz = srcr.size();
        ts::uint8 *c = tgt.body(srcr.lt);
        int addy = tgt.info().pitch - sz.x * 4;
        for( int y=0; y<sz.y; ++y, c += addy )
            for( int x=0; x<sz.x; ++x, c += 4 )
                *((ts::uint32 *)c) &= 0xff000000;
    }

    // now make blank new area after shift

    int y0 = 0;
    int y1 = 0;
    if ( xoffset.y > 0 )
    {
        y0 = xoffset.y;
        y1 = tgt.info().sz.y;
        tgt.fill(ts::ivec2(0), ts::ivec2(tgt.info().sz.x, xoffset.y), newcolor);
    }
    else if ( xoffset.y < 0 )
    {
        y1 = tgt.info().sz.y + xoffset.y;
        tgt.fill(ts::ivec2(0, y1), ts::ivec2(tgt.info().sz.x, -xoffset.y), newcolor);
    }

    if ( xoffset.x > 0 )
        tgt.fill( ts::ivec2(0, y0), ts::ivec2( xoffset.x, y1 ), newcolor);
    else if (xoffset.x < 0)
        tgt.fill(ts::ivec2( tgt.info().sz.x + xoffset.x, y0), ts::ivec2(tgt.info().sz.x, y1), newcolor);
}


/*virtual*/ void rsvg_filter_blend_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (load_s(ctx, *ai)) {}
        else if (attrname.equals(CONSTASTR("in2")))
            source2 = surface_index(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("mode")))
        {
            if (ts::pstr_c(ai->value()).equals(CONSTASTR("normal")))
                blendmode = BM_NORMAL;
            else if (ts::pstr_c(ai->value()).equals(CONSTASTR("multiply")))
                blendmode = BM_MULTIPLY;
            else if (ts::pstr_c(ai->value()).equals(CONSTASTR("screen")))
                blendmode = BM_SCREEN;
            else if (ts::pstr_c(ai->value()).equals(CONSTASTR("darken")))
                blendmode = BM_DARKEN;
            else if (ts::pstr_c(ai->value()).equals(CONSTASTR("lighten")))
                blendmode = BM_LIGHTEN;
        }
    }
}

namespace ts
{
    extern uint8 ALIGN(256) multbl[256][256]; // mul table from toolset
}

template< typename OP > void do_blend(const ts::bmpcore_exbody_s &in, const ts::bmpcore_exbody_s &in2, const ts::bmpcore_exbody_s &out, const OP &op)
{
    const ts::uint8 *src1 = in();
    const ts::uint8 *src2 = in2();
    ts::uint8 *tgt = out();

    ts::ivec2 sz = in.info().sz;

    int add_src1 = in.info().pitch - sz.x * 4;
    int add_src2 = in2.info().pitch - sz.x * 4;
    int add_tgt = out.info().pitch - sz.x * 4;

    for (int y = 0; y < sz.y; ++y, src1 += add_src1, src2 += add_src2, tgt += add_tgt)
    {
        for (int x = 0; x < sz.x; ++x, src1 += 4, src2 += 4, tgt += 4)
        {
            ts::TSCOLOR a = *(const ts::TSCOLOR *)src1;
            ts::TSCOLOR b = *(const ts::TSCOLOR *)src2;
            ts::uint8 qr = 255 - ts::multbl[255-ts::ALPHA(a)][ 255- ts::ALPHA(b)];
            auto oR = op(ts::RED(a), ts::RED(b), ts::ALPHA(a), ts::ALPHA(b));
            auto oG = op(ts::GREEN(a), ts::GREEN(b), ts::ALPHA(a), ts::ALPHA(b));
            auto oB = op(ts::BLUE(a), ts::BLUE(b), ts::ALPHA(a), ts::ALPHA(b));
            *(ts::TSCOLOR *)tgt = ts::CLAMP<ts::uint8>(oB) | (ts::CLAMP<ts::uint8>(oG) << 8) | (ts::CLAMP<ts::uint8>(oR) << 16) | (qr << 24);
        }
    }

}

/*virtual*/ void rsvg_filter_blend_c::apply(rsvg_working_surf_s *surfs, const cairo_matrix_t *) const
{
    ts::bitmap_c &src1 = surfs[ts::tmax(0, source)].bmp;
    ts::bitmap_c &src2 = surfs[ts::tmax(0, source2)].bmp;
    ts::bitmap_c &tgt = surfs[target].bmp;

    int srctype = blendmode;
    if (source < 0) srctype |= 16;
    if (source2 < 0) srctype |= 32;

    switch(srctype)
    {
    case BM_NORMAL:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->uint {
            return ts::multbl[255 - aa][cb] + ca;
        });
        break;
    case BM_NORMAL + 16:
    case BM_MULTIPLY + 16:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::multbl[255 - aa][cb];
        });
        break;
    case BM_NORMAL + 32:
    case BM_SCREEN + 32:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ca;
        });
        break;
    case BM_NORMAL + 16 + 32:
    case BM_MULTIPLY + 16 + 32:
    case BM_SCREEN + 16 + 32:
    case BM_DARKEN + 16 + 32:
    case BM_LIGHTEN + 16 + 32:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8, ts::uint8, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return 0;
        });
        break;
    case BM_MULTIPLY:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->uint {
            return ts::multbl[255 - aa][cb] + ts::multbl[255 - ab][ca] + ts::multbl[ca][cb];
        });
        break;
    case BM_MULTIPLY + 32:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::multbl[255 - ab][ca];
        });
        break;
    case BM_SCREEN:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->int {
            return cb + ca - ts::multbl[ca][cb];
        });
        break;
    case BM_SCREEN + 16:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return cb;
        });
        break;
    case BM_DARKEN:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->uint {
            return ts::tmin( ts::multbl[255 - aa][cb] + ca, ts::multbl[255 - ab][ca] + cb );
        });
        break;
    case BM_DARKEN + 16:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::tmin(ts::multbl[255 - aa][cb], cb);
        });
        break;
    case BM_DARKEN + 32:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::tmin(ca, ts::multbl[255 - ab][ca]);
        });
        break;
    case BM_LIGHTEN:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->uint {
            return ts::tmax(ts::multbl[255 - aa][cb] + ca, ts::multbl[255 - ab][ca] + cb);
        });
        break;
    case BM_LIGHTEN + 16:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8, ts::uint8 cb, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::tmax(ts::multbl[255 - aa][cb], cb);
        });
        break;
    case BM_LIGHTEN + 32:
        do_blend(src1.extbody(), src2.extbody(), tgt.extbody(), [](ts::uint8 ca, ts::uint8, ts::uint8 aa, ts::uint8 ab)->ts::uint8 {
            return ts::tmax(ca, ts::multbl[255 - ab][ca]);
        });
        break;
    }
}


/*virtual*/ void rsvg_filter_gaussian_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (load_s(ctx, *ai)) {}
        else if (attrname.equals(CONSTASTR("stdDeviation")))
        {
            sd = ts::parsevec2f(ai->value(), ts::vec2(0), true);
            if (sd.x < 0) sd.x = 0;
            if (sd.y < 0) sd.y = 0;
            sd.x *= 0.5f;
        }
    }
}

static inline int compute_box_blur_width(double radius)
{
    double width = radius * 3 * sqrt(2 * M_PI) / 4;
    return ts::lround(width);
}

static void make_gaussian_convolution_matrix(double radius, ts::tmp_tbuf_t<double> &out_matrix)
{
    out_matrix.clear();

    double std_dev = radius + 1.0;
    radius = std_dev * 2;

    int matrix_len = ts::lround( 2 * ceil(radius - 0.5) ) + 1;
    if (matrix_len <= 0)
        matrix_len = 1;

    out_matrix.set_count(matrix_len);
    double *matrix = out_matrix.begin();

    /* Fill the matrix by doing numerical integration approximation
    * from -2*std_dev to 2*std_dev, sampling 50 points per pixel.
    * We do the bottom half, mirror it to the top half, then compute the
    * center point.  Otherwise asymmetric quantization errors will occur.
    * The formula to integrate is e^-(x^2/2s^2).
    */

    for (int i = matrix_len / 2 + 1; i < matrix_len; ++i)
    {
        double base_x = (double)(i - (matrix_len >> 1)) - 0.5;

        double sum = 0;
        for (int j = 1; j <= 50; j++)
        {
            double r = base_x + 0.02 * j;

            if (r <= radius)
                sum += exp(-ts::dsqr(r) / (2 * ts::dsqr(std_dev)));
        }

        matrix[i] = sum / 50;
    }

    /* mirror to the bottom half */
    for (int i = 0; i <= matrix_len / 2; i++)
        matrix[i] = matrix[matrix_len - 1 - i];

    /* find center val -- calculate an odd number of quanta to make it
    * symmetric, even if the center point is weighted slightly higher
    * than others.
    */
    double sum = 0;
    for (int j = 0; j <= 50; ++j)
        sum += exp(-ts::dsqr(-0.5 + 0.02 * j) / (2 * ts::dsqr(std_dev)));

    matrix[matrix_len / 2] = sum / 51;

    /* normalize the distribution by scaling the total sum to one */
    sum = 0;
    for (int i = 0; i < matrix_len; ++i)
        sum += matrix[i];

    for (int i = 0; i < matrix_len; ++i)
        matrix[i] = matrix[i] / sum;

}

static void box_blur_line(int box_width, int even_offset, const ts::uint8 *src, ts::uint8 *dest, int len)
{
    int  lead = 0;      /* This marks the leading edge of the kernel              */
    int  output;        /* This marks the center of the kernel                    */
    int  trail;         /* This marks the pixel BEHIND the last 1 in the
                           kernel; it's the pixel to remove from the accumulator. */
    int  ac[4] = {0};   /* Accumulator for each channel                           */


    /* The algorithm differs for even and odd-sized kernels.
    * With the output at the center,
    * If odd, the kernel might look like this: 0011100
    * If even, the kernel will either be centered on the boundary between
    * the output and its left neighbor, or on the boundary between the
    * output and its right neighbor, depending on even_lr.
    * So it might be 0111100 or 0011110, where output is on the center
    * of these arrays.
    */

    if (box_width % 2 != 0)
    {
        // Odd-width kernel
        output = lead - (box_width - 1) / 2;
        trail = lead - box_width;
    } else
    {
        // Even-width kernel.
        if (even_offset == 1)
        {
            // Right offset
            output = lead + 1 - box_width / 2;
            trail = lead - box_width;
        } else if (even_offset == -1)
        {
            // Left offset
            output = lead - box_width / 2;
            trail = lead - box_width;
        } else
        {
            /* If even_offset isn't 1 or -1, there's some error. */
            FORBIDDEN();
            return;
        }
    }

    /* As the kernel moves across the image, it has a leading edge and a
    * trailing edge, and the output is in the middle. */
    while (output < len)
    {
        /* The number of pixels that are both in the image and
        * currently covered by the kernel. This is necessary to
        * handle edge cases. */
        uint coverage = (lead < len ? lead : len - 1) - (trail >= 0 ? trail : -1);

#ifdef READABLE_BOXBLUR_CODE
        /* The code here does the same as the code below, but the code below
        * has been optimized by moving the if statements out of the tight for
        * loop, and is harder to understand.
        * Don't use both this code and the code below. */
        for (i = 0; i < bpp; i++) {
            /* If the leading edge of the kernel is still on the image,
            * add the value there to the accumulator. */
            if (lead < len)
                ac[i] += src[bpp * lead + i];

            /* If the trailing edge of the kernel is on the image,
            * subtract the value there from the accumulator. */
            if (trail >= 0)
                ac[i] -= src[bpp * trail + i];

            /* Take the averaged value in the accumulator and store
            * that value in the output. The number of pixels currently
            * stored in the accumulator can be less than the nominal
            * width of the kernel because the kernel can go "over the edge"
            * of the image. */
            if (output >= 0)
                dest[bpp * output + i] = (ac[i] + (coverage >> 1)) / coverage;
        }
#endif

        int curpixelc = 4 * output;

        /* If the leading edge of the kernel is still on the image... */
        if (lead < len)
        {
            if (trail >= 0)
            {
                /* If the trailing edge of the kernel is on the image. (Since
                * the output is in between the lead and trail, it must be on
                * the image. */
                for (int i = 0; i < 4; ++i)
                {
                    ac[i] += src[4 * lead + i];
                    ac[i] -= src[4 * trail + i];
                    dest[curpixelc + i] = ts::CLAMP<ts::uint8>( (ac[i] + (coverage >> 1)) / coverage );
                }
            }
            else if (output >= 0)
            {
                /* If the output is on the image, but the trailing edge isn't yet
                * on the image. */

                for (int i = 0; i < 4; ++i)
                {
                    ac[i] += src[4 * lead + i];
                    dest[curpixelc + i] = ts::CLAMP<ts::uint8>( (ac[i] + (coverage >> 1)) / coverage );
                }
            }
            else {
                /* If leading edge is on the image, but the output and trailing
                * edge aren't yet on the image. */
                for (int i = 0; i < 4; ++i)
                    ac[i] += src[4 * lead + i];
            }
        } else if (trail >= 0)
        {
            /* If the leading edge has gone off the image, but the output and
            * trailing edge are on the image. (The big loop exits when the
            * output goes off the image. */
            for (int i = 0; i < 4; ++i)
            {
                ac[i] -= src[4 * trail + i];
                dest[curpixelc + i] = ts::CLAMP<ts::uint8>((ac[i] + (coverage >> 1)) / coverage );
            }
        } else if (output >= 0)
        {
            /* Leading has gone off the image and trailing isn't yet in it
            * (small image) */
            for (int i = 0; i < 4; ++i)
                dest[curpixelc + i] = ts::CLAMP<ts::uint8>((ac[i] + (coverage >> 1)) / coverage);
        }

        lead++;
        output++;
        trail++;
    }
}

static void gaussian_blur_line(const double *matrix, ts::aint matrix_len, const ts::uint8 *src, ts::uint8 *dest, int len)
{
    ts::aint matrix_middle = matrix_len / 2;

    /* picture smaller than the matrix? */
    if (matrix_len > len)
    {
        for (int row = 0; row < len; ++row)
        {
            /* find the scale factor */
            double scale = 0;

            for (int j = 0; j < len; ++j)
            {
                /* if the index is in bounds, add it to the scale counter */
                if (j + matrix_middle - row >= 0 && j + matrix_middle - row < matrix_len)
                    scale += matrix[j];
            }

            scale = 1.0 / scale;

            const ts::uint8 *src_p = src;

            for (int i = 0; i < 4; ++i)
            {
                double sum = 0;
                const ts::uint8 *src_p1 = src_p++;

                for (int j = 0; j < len; ++j)
                {
                    if (j + matrix_middle - row >= 0 &&
                        j + matrix_middle - row < matrix_len)
                        sum += matrix[j] * (*src_p1);

                    src_p1 += 4;
                }

                *dest++ = ts::CLAMP<ts::uint8>( sum * scale );
            }
        }
    } else
    {
        /* left edge */

        int row = 0;

        for (; row < matrix_middle; ++row)
        {
            /* find scale factor */
            double scale = 0;

            for ( ts::aint j = matrix_middle - row; j < matrix_len; j++)
                scale += matrix[j];

            scale = 1.0 / scale;

            const ts::uint8 *src_p = src;

            for (int i = 0; i < 4; ++i)
            {
                double sum = 0;
                const ts::uint8 *src_p1 = src_p++;

                for ( ts::aint j = matrix_middle - row; j < matrix_len; ++j)
                {
                    sum += matrix[j] * (*src_p1);
                    src_p1 += 4;
                }

                *dest++ = ts::CLAMP<ts::uint8>(sum * scale);
            }
        }

        /* go through each pixel in each col */
        for (; row < len - matrix_middle; ++row)
        {
            const ts::uint8 *src_p = src + (row - matrix_middle) * 4;

            for (int i = 0; i < 4; ++i)
            {
                double sum = 0;
                const ts::uint8 *src_p1 = src_p++;

                for (int j = 0; j < matrix_len; ++j)
                {
                    sum += matrix[j] * (*src_p1);
                    src_p1 += 4;
                }

                *dest++ = ts::CLAMP<ts::uint8>(sum);
            }
        }

        /* for the edge condition, we only use available info and scale to one */
        for (; row < len; ++row)
        {
            /* find scale factor */
            double scale = 0;

            for (int j = 0; j < len - row + matrix_middle; ++j)
                scale += matrix[j];

            scale = 1.0 / scale;

            const ts::uint8 *src_p = src + (row - matrix_middle) * 4;

            for (int i = 0; i < 4; ++i)
            {
                double sum = 0;

                const ts::uint8 *src_p1 = src_p++;

                for (int j = 0; j < len - row + matrix_middle; ++j)
                {
                    sum += matrix[j] * (*src_p1);
                    src_p1 += 4;
                }

                *dest++ = ts::CLAMP<ts::uint8>(sum * scale);
            }
        }
    }
}

static void get_column(ts::uint8 *column_data, const  ts::uint8 *src_data, int src_stride, int height)
{
    const ts::uint8 *src = src_data;
    for (int y = 0; y < height; ++y, src += src_stride, column_data += 4)
        *(ts::uint32 *)column_data = *(ts::uint32 *)src;
}

static void put_column(const ts::uint8 *column_data, ts::uint8 *dest_data, int dest_stride, int height)
{
    ts::uint8 *dst = dest_data;

    for (int y = 0; y < height; ++y, dst += dest_stride, column_data += 4)
        *(ts::uint32 *)dst = *(ts::uint32 *)column_data;
}




/*virtual*/ void rsvg_filter_gaussian_c::apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const
{
    double sdx = sd.x * m->xx;
    double sdy = sd.y * m->yy;

    bool use_box_blur = sdx >= 10.0 || sdy >= 10.0;

    ts::bitmap_c &tgt = surfs[target].bmp;

    /* Bail out by just copying? */
    if ((sdx == 0.0 && sdy == 0.0) || sdx > 1000 || sdy > 1000)
    {
        tgt.fill(0);
        return;
    }

    ts::bitmap_c &src = surfs[ts::tmax(0, source)].bmp;
    ts::ivec2 sz = src.info().sz;


    ts::tmp_tbuf_t<double> gaussian_matrix;
    ts::uint8 *pix_buffer = (ts::uint8 *)alloca( ts::tmax(sz.x, sz.y) * 4 * 2 );

    bool has_data = false;

    if (sdx != 0.0)
    {
        const ts::uint8* srcb = src.body();
        ts::uint8* tgtb = tgt.body();
        
        if (use_box_blur)
        {
            int box_width = compute_box_blur_width(sdx);

            /* twice the size so we can have "two" scratch rows */
            ts::uint8 *row1 = pix_buffer, *row2 = pix_buffer + sz.x * 4;

            for (int y = 0; y < sz.y; ++y, srcb += src.info().pitch, tgtb += tgt.info().pitch)
            {
                if (box_width & 1)
                {
                    /* Odd-width box blur: repeat 3 times, centered on output pixel */

                    box_blur_line(box_width, 0, srcb, row1, sz.x);
                    box_blur_line(box_width, 0, row1, row2, sz.x);
                    box_blur_line(box_width, 0, row2, tgtb, sz.x);
                }
                else {
                    /* Even-width box blur:
                    * This method is suggested by the specification for SVG.
                    * One pass with width n, centered between output and right pixel
                    * One pass with width n, centered between output and left pixel
                    * One pass with width n+1, centered on output pixel
                    */
                    box_blur_line(box_width, -1, srcb, row1, sz.x);
                    box_blur_line(box_width, 1, row1, row2, sz.x);
                    box_blur_line(box_width + 1, 0, row2, tgtb, sz.x);
                }
            }
        } else
        {
            make_gaussian_convolution_matrix(sdx, gaussian_matrix);
            for (int y = 0; y < sz.y; ++y, srcb += src.info().pitch, tgtb += tgt.info().pitch)
                gaussian_blur_line(gaussian_matrix.begin(), gaussian_matrix.count(), srcb, tgtb, sz.x);
        }

        has_data = true;
    }

    if (sdy != 0.0)
    {
        const ts::uint8* srcb = src.body();
        ts::uint8* tgtb = tgt.body();

        /* twice the size so we can have the source pixels and the blurred pixels */
        ts::uint8 *col1 = pix_buffer, *col2 = pix_buffer + sz.y * 4;

        if (use_box_blur)
        {
            int box_height = compute_box_blur_width(sdy);

            for (int x = 0; x < sz.x; ++x, srcb += 4, tgtb += 4)
            {
                if (has_data)
                    get_column(col1, tgtb, tgt.info().pitch, sz.y);
                else
                    get_column(col1, srcb, src.info().pitch, sz.y);

                if (box_height & 1)
                {
                    /* Odd-width box blur */
                    box_blur_line(box_height, 0, col1, col2, sz.y);
                    box_blur_line(box_height, 0, col2, col1, sz.y);
                    box_blur_line(box_height, 0, col1, col2, sz.y);
                }
                else
                {
                    /* Even-width box blur */
                    box_blur_line(box_height, -1, col1, col2, sz.y);
                    box_blur_line(box_height, 1, col2, col1, sz.y);
                    box_blur_line(box_height + 1, 0, col1, col2, sz.y);
                }

                put_column(col2, tgtb, tgt.info().pitch, sz.y);
            }
        } else
        {
            make_gaussian_convolution_matrix(sdy, gaussian_matrix);

            for (int x = 0; x < sz.x; ++x, srcb += 4, tgtb += 4)
            {
                if (has_data)
                    get_column(col1, tgtb, tgt.info().pitch, sz.y);
                else
                    get_column(col1, srcb, src.info().pitch, sz.y);

                gaussian_blur_line(gaussian_matrix.begin(), gaussian_matrix.count(), col1, col2, sz.y);

                put_column(col2, tgtb, tgt.info().pitch, sz.y);
            }
        }
    }

}

ts::TSCOLOR rsvg_filter_ctransfer_c::CTR_A_LINEAR_SLOPE(const param_s &p, ts::TSCOLOR col)
{
    // alpha premultiplied special

    ts::uint8 alpha = ts::CLAMP<ts::uint8>(p.slope * ts::ALPHA(col) + p.intercept);

    if (alpha == 0)
        return 0;

    if (alpha == 255)
        return col | 0xff000000;

    if (ts::RED(col) > alpha)
        col = (col & 0x0000ffff) | (alpha << 16);
    if (ts::GREEN(col) > alpha)
        col = (col & 0x00ff00ff) | (alpha << 8);
    if (ts::BLUE(col) > alpha)
        col = (col & 0x00ffff00) | alpha;

    return (col & 0x00ffffff) | (alpha << 24);
}

ts::TSCOLOR rsvg_filter_ctransfer_c::CTR_C_LINEAR_SLOPE(const param_s &p, ts::TSCOLOR col)
{
    ts::uint8 &c = ((ts::uint8 *)(&col))[p.c];
    c = ts::CLAMP<ts::uint8>( p.slope * c + p.intercept );
    return col;
}

void rsvg_filter_ctransfer_c::c_load(rsvg_load_context_s & ctx, ts::rapidxml::xml_node<char>* node, int c)
{
    enum
    {
        T_UNKNOWN,
        T_LINEAR
    } t = T_UNKNOWN;

    float slope = 0;
    float intercept = 0;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("type")))
        {
            if (ts::pstr_c(ai->value()).equals(CONSTASTR("linear")))
                t = T_LINEAR;
        } else if (attrname.equals(CONSTASTR("slope")))
        {
            slope = ts::pstr_c(ai->value()).as_float();
        } else if (attrname.equals(CONSTASTR("intercept")))
        {
            intercept = ts::pstr_c(ai->value()).as_float();
        }
    }

    switch (t)
    {
    case T_LINEAR:
        {
            param_s &p = processing.add();
            p.c = c;
            p.slope = slope;
            p.intercept = intercept * 255.0f;
            p.func = c == 3 ? CTR_A_LINEAR_SLOPE : CTR_C_LINEAR_SLOPE;
        }
        break;
    default:
        break;
    }

}

void rsvg_filter_ctransfer_c::load(rsvg_load_context_s & ctx, ts::rapidxml::xml_node<char>* node)
{
    for (ts::rapidxml::node_iterator<char> ni(node), e; ni != e; ++ni)
    {
        ts::pstr_c tagname(ni->name());
        if (tagname.begins(CONSTASTR("feFunc")) && tagname.get_length() == 7)
        {
            switch (tagname.get_char(6))
            {
            case 'R':
                c_load(ctx, &*ni, 2); break;
            case 'G':
                c_load(ctx, &*ni, 1); break;
            case 'B':
                c_load(ctx, &*ni, 0); break;
            case 'A':
                c_load(ctx, &*ni, 3); break;
            }
        }
    }
}

void rsvg_filter_ctransfer_c::apply(rsvg_working_surf_s * surfs, const cairo_matrix_t * m) const
{
    ts::bitmap_c &in = surfs[ts::tmax(0, source)].bmp;
    ts::bitmap_c &out = surfs[target].bmp;

    const ts::uint8 *src = in.body();
    ts::uint8 *tgt = out.body();

    ts::ivec2 sz = in.info().sz;

    int add_src = in.info().pitch - sz.x * 4;
    int add_tgt = out.info().pitch - sz.x * 4;

    for (int y = 0; y < sz.y; ++y, src += add_src, tgt += add_tgt)
    {
        for (int x = 0; x < sz.x; ++x, src += 4, tgt += 4)
        {
            ts::TSCOLOR col = *(const ts::TSCOLOR *)src;
            
            for (const param_s &p : processing)
            {
                col = p.func(p, col);
            }
            
            *(ts::TSCOLOR *)tgt = col;
        }
    }

}
