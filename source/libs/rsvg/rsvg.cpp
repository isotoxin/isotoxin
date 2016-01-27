#include "_precompiled.h"

#define RSVG_ARC_MAGIC ((double) 0.5522847498)


namespace ts
{
    namespace rapidxml
    {
        void parse_error_handler(const char *what, void *where)
        {
        }
    }
}

namespace
{
    struct render_cleanup_s
    {
        double oox, ooy;
        cairo_matrix_t keepm;
        rsvg_cairo_surface_c &surf;
        int flags = 0;
        render_cleanup_s(const render_cleanup_s &) UNUSED;
        void operator=(const render_cleanup_s &) UNUSED;
        render_cleanup_s( rsvg_cairo_surface_c &surf, const cairo_matrix_t *transform, const ts::ivec2 &offset ):surf(surf)
        {
            if (transform)
            {
                cairo_get_matrix(surf.cr(), &keepm);            \
                cairo_transform(surf.cr(), transform);
                flags |= 1;
            }
            if ( offset != ts::ivec2(0) )
            {
                cairo_surface_get_device_offset( surf.cs(), &oox, &ooy );
                cairo_surface_set_device_offset( surf.cs(), oox + offset.x, ooy + offset.y );
                flags |= 2;
            }
        }
        ~render_cleanup_s()
        {
            if (0 != (flags & 1))
            {
                cairo_set_matrix(surf.cr(), &keepm);
            }
            if (0 != (flags & 2))
            {
                cairo_surface_set_device_offset( surf.cs(), oox, ooy );
            }
        }
    
    };
}

#define RENDER_PROLOG render_cleanup_s cc(surf, use_filter ? transform.get() : nullptr, offset); \
    if (use_filter && filter) { \
        filter->render(this, offset, surf); \
        return; }

#include "rsvg_internals.h"

static ts::TSCOLOR TSCALL parsecolornone(const ts::asptr &s, const ts::TSCOLOR def)
{
    if ( ts::pstr_c(s).equals(CONSTASTR("none")) ) return 0;
    return ts::parsecolor(s,def);
}

bool rsvg_stroke_s::load(ts::rapidxml::xml_attribute<char> *a)
{
    ts::pstr_c attrname(a->name());
    if (attrname.begins( CONSTASTR("stroke")) )
    {
        if (attrname.get_length() == 6)
        {
            stroke = parsecolornone(ts::asptr(a->value()), ts::ARGB(0, 0, 0));
            return true;
        }

        attrname = attrname.substr( 6 );

        if (attrname.equals(CONSTASTR("-width")))
        {
            stroke_width = ts::pstr_c(ts::asptr(a->value())).as_float();
            return true;
        }

        if (attrname.equals(CONSTASTR("-dashoffset")))
        {
            dash_offset = ts::pstr_c(ts::asptr(a->value())).as_float();
            return true;
        }

        if (attrname.equals(CONSTASTR("-dasharray")))
        {
            if ( ts::pstr_c(a->value()).equals(CONSTASTR("none")) )
                dasharray.clear();
            else
            {
                for (ts::token<char> da(a->value(), ','); da; ++da)
                {
                    double v = 0;
                    ts::pstr_c sv = da->get_trimmed();
                    if (sv.get_last_char() == '%')
                        v = 1.0;
                    else
                        v = sv.as_double();
                    if (v > 0)
                        dasharray.add(v);
                }

                if (0 != (dasharray.count() & 1))
                    dasharray.clone(0, dasharray.count());
            }
            return true;
        }
    }


    return false;
}

bool rsvg_stroke_and_fill_s::load(ts::rapidxml::xml_attribute<char> *a)
{
    if (ts::pstr_c(a->name()).equals(CONSTASTR("fill")))
    {
        fillcolor = parsecolornone(ts::asptr(a->value()), ts::ARGB(0,0,0));
        return true;
    }
    return __super::load(a);
}


rsvg_node_c::~rsvg_node_c()
{

}

void rsvg_node_c::load_filter_ref( rsvg_load_context_s &ctx, const ts::asptr&fref )
{
    if ( ts::pstr_c(fref).begins(CONSTASTR("url(#")) )
        filter = ctx.filters[ ts::pstr_c(fref).substr(5,fref.l-1) ];
}

static double g_ascii_strtod( const char *s, const char **endptr )
{
    const char *st = s;
    while (ts::CHAR_is_digit(*st) || *st == '-' || *st == '.')
        ++st;
    *endptr = st;
    double rslt = 0;
    ts::CHARz_to_double(rslt, s, st - s);
    return rslt;
}

/*
 * _rsvg_cairo_matrix_init_shear: Set up a shearing matrix.
 * @dst: Where to store the resulting affine transform.
 * @theta: Shear angle in degrees.
 *
 * Sets up a shearing matrix. In the standard libart coordinate system
 * and a small value for theta, || becomes \\. Horizontal lines remain
 * unchanged.
 **/
static void _rsvg_cairo_matrix_init_shear (cairo_matrix_t *dst, double theta)
{
    cairo_matrix_init (dst, 1., 0., tan (theta * M_PI / 180.0), 1., 0., 0);
}


void rsvg_node_c::load_transform(rsvg_load_context_s &ctx, const ts::asptr&src)
{
    char keyword[32];
    double args[6];

    transform.reset( TSNEW(cairo_matrix_t) );
    cairo_matrix_init_identity(transform.get());

    int idx = 0;
    while (idx < src.l)
    {
        /* skip initial whitespace */
        while (ts::CHAR_is_hollow(src.s[idx]))
            idx++;

        if (idx == src.l)
            break;

        /* parse keyword */
        uint key_len = 0;
        for (; key_len < sizeof(keyword); key_len++)
        {
            char c;

            c = src.s[idx];
            if (ts::CHAR_is_letter(c) || c == '-')
                keyword[key_len] = src.s[idx++];
            else
                break;
        }
        if (key_len >= sizeof(keyword))
        {
            transform.reset();
            return;
        }
        keyword[key_len] = '\0';

        /* skip whitespace */
        while (ts::CHAR_is_hollow(src.s[idx]))
            idx++;

        if (src.s[idx] != '(')
        {
            transform.reset();
            return;
        }

        ++idx;

        int n_args = 0;
        for (; ; ++n_args)
        {
            const char *end_ptr;

            /* skip whitespace */
            while (ts::CHAR_is_hollow(src.s[idx]))
                idx++;

            char c = src.s[idx];
            if (ts::CHAR_is_digit(c) || c == '+' || c == '-' || c == '.')
            {
                if (n_args == sizeof(args) / sizeof(args[0]))
                {
                    transform.reset();
                    return; /* too many args */
                }

                args[n_args] = g_ascii_strtod(src.s + idx, &end_ptr);
                idx = end_ptr - src.s;

                while (ts::CHAR_is_hollow(src.s[idx]))
                    idx++;

                /* skip optional comma */
                if (src.s[idx] == ',')
                    idx++;
            }
            else if (c == ')')
                break;
            else
            {
                transform.reset();
                return;
            }
        }
        idx++;

        /* ok, have parsed keyword and args, now modify the transform */
        if (ts::CHARz_equal(keyword, "matrix"))
        {
            if (n_args != 6)
            {
                transform.reset();
                return;
            }

            cairo_matrix_t affine;
            cairo_matrix_init(&affine, args[0], args[1], args[2], args[3], args[4], args[5]);
            cairo_matrix_multiply(transform.get(), &affine, transform.get());

        } else if (ts::CHARz_equal(keyword, "translate"))
        {
            if (n_args == 1)
                args[1] = 0;
            else if (n_args != 2)
            {
                transform.reset();
                return;
            }
            cairo_matrix_t affine;
            cairo_matrix_init_translate(&affine, args[0], args[1]);
            cairo_matrix_multiply(transform.get(), &affine, transform.get());
        }
        else if (ts::CHARz_equal(keyword, "scale")) {
            if (n_args == 1)
                args[1] = args[0];
            else if (n_args != 2)
            {
                transform.reset();
                return;
            }

            cairo_matrix_t affine;
            cairo_matrix_init_scale(&affine, args[0], args[1]);
            cairo_matrix_multiply(transform.get(), &affine, transform.get());
        }
        else if (ts::CHARz_equal(keyword, "rotate"))
        {
            cairo_matrix_t affine;
            if (n_args == 1)
            {

                cairo_matrix_init_rotate(&affine, args[0] * M_PI / 180.);
                cairo_matrix_multiply(transform.get(), &affine, transform.get());

            } else if (n_args == 3)
            {
                cairo_matrix_init_translate(&affine, args[1], args[2]);
                cairo_matrix_multiply(transform.get(), &affine, transform.get());

                cairo_matrix_init_rotate(&affine, args[0] * M_PI / 180.);
                cairo_matrix_multiply(transform.get(), &affine, transform.get());

                cairo_matrix_init_translate(&affine, -args[1], -args[2]);
                cairo_matrix_multiply(transform.get(), &affine, transform.get());
            } else
            {
                transform.reset();
                return;
            }
        } else if (ts::CHARz_equal(keyword, "skewX"))
        {
            if (n_args != 1)
            {
                transform.reset();
                return;
            }
            cairo_matrix_t affine;
            _rsvg_cairo_matrix_init_shear(&affine, args[0]);
            cairo_matrix_multiply(transform.get(), &affine, transform.get());
        } else if (ts::CHARz_equal(keyword, "skewY"))
        {
            if (n_args != 1)
            {
                transform.reset();
                return;
            }
            cairo_matrix_t affine;
            _rsvg_cairo_matrix_init_shear(&affine, args[0]);
            /* transpose the affine, given that we know [1] is zero */
            affine.yx = affine.xy;
            affine.xy = 0.;
            cairo_matrix_multiply(transform.get(), &affine, transform.get());

        } else
        {
            transform.reset();
            return;
        }
    }
}

rsvg_svg_c * rsvg_svg_c::build_from_xml(char * svgxml)
{
    ts::rapidxml::xml_document<> doc;
    doc.parse<0>( svgxml );

    if ( auto *fn = doc.first_node() )
        if (ts::pstr_c(fn->name()).equals(CONSTASTR("svg")))
        {
            rsvg_load_context_s ctx;
            rsvg_svg_c *svg = TSNEW(rsvg_svg_c);
            svg->load( ctx, fn );
            return svg;
        }

    return nullptr;
}

void rsvg_group_c::load(rsvg_node_c *self, rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *node)
{
    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (ctx.deffillstroke.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            self->load_filter_ref(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("transform")))
            self->load_transform(ctx, ai->value());
    }

    for (ts::rapidxml::node_iterator<char> ni(node), e; ni != e; ++ni)
    {
        ts::pstr_c tagname(ni->name());
        if (tagname.equals(CONSTASTR("circle")))
        {
            rsvg_circle_c *n = TSNEW(rsvg_circle_c);
            n->load(ctx, &*ni);
            children.add(n);
        } else if (tagname.equals(CONSTASTR("rect")))
        {
            rsvg_rect_c *n = TSNEW(rsvg_rect_c);
            n->load(ctx, &*ni);
            children.add(n);

        } else if (tagname.equals(CONSTASTR("line")))
        {
            rsvg_line_c *n = TSNEW(rsvg_line_c);
            n->load(ctx, &*ni);
            children.add(n);

        } else if (tagname.equals(CONSTASTR("path")))
        {
            rsvg_path_c *n = TSNEW(rsvg_path_c);
            n->load(ctx, &*ni);
            children.add(n);
        } else if (tagname.equals(CONSTASTR("polygon")))
        {
            rsvg_polygon_c *n = TSNEW(rsvg_polygon_c);
            n->load(ctx, &*ni);
            children.add(n);
        }
        else if (tagname.equals(CONSTASTR("polyline")))
        {
            rsvg_polyline_c *n = TSNEW(rsvg_polyline_c);
            n->load(ctx, &*ni);
            children.add(n);

        } else if (tagname.equals(CONSTASTR("g")))
        {
            rsvg_g_c *n = TSNEW(rsvg_g_c);
            n->load(ctx, &*ni);
            children.add(n);
        } else if (load_filters(ctx, &*ni)) {
        } else if (tagname.equals(CONSTASTR("defs")))
        {
            for (ts::rapidxml::node_iterator<char> def(&*ni), ee; def != ee; ++def)
            {
                if (load_filters(ctx, &*def)) {
                }
            }
        }
    }
}

void rsvg_group_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf)
{
    for (rsvg_node_c *svgnode : children)
        svgnode->render(offset, surf, true);
}


void rsvg_svg_c::load(rsvg_load_context_s& ctx, ts::rapidxml::xml_node<char> *node)
{
    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("width")))
            ctx.size.x = sz.x = ts::pstr_c(ai->value()).as_int();
        else if (attrname.equals(CONSTASTR("height")))
            ctx.size.y = sz.y = ts::pstr_c(ai->value()).as_int();
    }

    rsvg_group_c::load(this, ctx, node);
}

void rsvg_svg_c::release()
{
    TSDEL( this );
}

void rsvg_svg_c::render( const ts::bmpcore_exbody_s &pixbuf, const ts::ivec2 &offset, const cairo_matrix_t *matrix )
{
    rsvg_cairo_surface_c s(pixbuf, matrix);
    render( offset, s, true );
}

/*virtual*/ void rsvg_svg_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG
    rsvg_group_c::render(offset, surf);
}



/*virtual*/ void rsvg_g_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *node)
{
    rsvg_group_c::load(this, ctx, node);
}
/*virtual*/ void rsvg_g_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG
    rsvg_group_c::render(offset, surf);
}



void rsvg_circle_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    strokenfill = ctx.deffillstroke;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("cx")))
            xyr.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("cy")))
            xyr.y = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("r")))
            xyr.z = ts::pstr_c(ai->value()).as_float();
        else if (strokenfill.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}

/*virtual*/ void rsvg_circle_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG

    cairo_paths_c paths;

    float cx = xyr.x;
    float cy = xyr.y;
    float r = xyr.z;

    paths.move_to(cx + r, cy);
    paths.curve_to( cx + r, cy + r * RSVG_ARC_MAGIC,
                    cx + r * RSVG_ARC_MAGIC, cy + r,
                    cx, cy + r);

    paths.curve_to( cx - r * RSVG_ARC_MAGIC, cy + r,
                    cx - r, cy + r * RSVG_ARC_MAGIC,
                    cx - r, cy);

    paths.curve_to( cx - r, cy - r * RSVG_ARC_MAGIC,
                    cx - r * RSVG_ARC_MAGIC, cy - r,
                    cx, cy - r);

    paths.curve_to( cx + r * RSVG_ARC_MAGIC, cy - r,
                    cx + r, cy - r * RSVG_ARC_MAGIC,
                    cx + r, cy);

    paths.close();
    paths.render( strokenfill, surf);
}


void rsvg_rect_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node)
{
    strokenfill = ctx.deffillstroke;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("x")))
            xy.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("y")))
            xy.y = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("width")))
            sz.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("height")))
            sz.y = ts::pstr_c(ai->value()).as_float();
        else if (strokenfill.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("rx")))
            r.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("ry")))
            r.y = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}

/*virtual*/ void rsvg_rect_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG

    cairo_paths_c paths;

    if (r.x == 0 || r.y == 0)
    {
        /* Easy case, no rounded corners */

        paths.move_to(xy.x, xy.y);
        paths.line_to(xy.x + sz.x, xy.y);
        paths.line_to(xy.x + sz.x, xy.y + sz.y);
        paths.line_to(xy.x, xy.y + sz.y);

    } else
    {
        double top_x1 = xy.x + r.x;
        double top_x2 = xy.x + sz.x - r.x;
        double bottom_y  = xy.y + sz.y;
        double left_y1 = xy.y + r.y;
        double left_y2 = xy.y + sz.y - r.y;
        double right_x = xy.x + sz.x;

        paths.move_to(top_x1, xy.y);
        paths.line_to(top_x2, xy.y);
        paths.arc( top_x2, xy.y, r.x, r.y, 0, false, true, right_x, left_y1);
        paths.line_to(right_x, left_y2);
        paths.arc( right_x, left_y2, r.x, r.y, 0, false, true, top_x2, bottom_y);
        paths.line_to(top_x1, bottom_y);
        paths.arc( top_x1, bottom_y, r.x, r.y, 0, false, true, xy.x, left_y2);
        paths.line_to(xy.x, left_y1);
        paths.arc(  xy.x, left_y1, r.x, r.y, 0, false, true, top_x1, xy.y);
    }
    paths.close();
    paths.render(strokenfill, surf);
}

/*virtual*/ void rsvg_line_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *node)
{
    stroke = ctx.deffillstroke;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("x1")))
            xy1.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("y1")))
            xy1.y = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("x2")))
            xy2.x = ts::pstr_c(ai->value()).as_float();
        else if (attrname.equals(CONSTASTR("y2")))
            xy2.y = ts::pstr_c(ai->value()).as_float();
        else if (stroke.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx,ts::asptr(ai->value()));
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}

/*virtual*/ void rsvg_line_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG

    cairo_paths_c paths;
    paths.move_to(xy1.x, xy1.y);
    paths.line_to(xy2.x, xy2.y);
    paths.render(stroke, surf);
}

namespace
{
    struct path_parser_s
    {
        cairo_paths_c &paths;
        cairo_path_data_t cp;       /* current point */
        cairo_path_data_t rp;       /* reflection point (for 's' and 't' commands) */
        double params[7];           /* parameters that have been parsed */
        int param = 0;              /* parameter number */
        char cmd = 0;               /* current command (lowercase) */
        bool rel = false;           /* true if relative coords */

        path_parser_s(cairo_paths_c &paths, const ts::asptr &data):paths(paths)
        {
            cp.point.x = 0.0;
            cp.point.y = 0.0;
            rp.point.x = 0.0;
            rp.point.y = 0.0;


            for (int i = 0; i < data.l; ++i)
            {
                char c = data.s[i];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    /* digit */
                    i += parse_number(data.skip(i)) - 1;

                }
                else if (c == 'z' || c == 'Z')
                {
                    if (param)
                        parse_path_do_cmd(true);

                    cp = rp = paths.last();

                }
                else if (c >= 'A' && c < 'Z' && c != 'E')
                {
                    if (param)
                        parse_path_do_cmd(true);
                    cmd = c + 'a' - 'A';
                    rel = false;

                }
                else if (c >= 'a' && c < 'z' && c != 'e')
                {
                    if (param)
                        parse_path_do_cmd(true);
                    cmd = c;
                    rel = true;
                }
                /* else c _should_ be whitespace or , */
            }

        }

        path_parser_s(const path_parser_s&) UNUSED;
        void operator=(const path_parser_s&) UNUSED;

        /* supply defaults for missing parameters, assuming relative coordinates
           are to be interpreted as x,y */
        void parse_path_default_xy (int n_params)
        {
            if (rel)
            {
                for (int i = param; i < n_params; ++i)
                {
                    /* we shouldn't get 0 here (usually ctx->param > 0 as
                       precondition) */
                    if (i == 0)
                        params[i] = cp.point.x;
                    else if (i == 1)
                        params[i] = cp.point.y;
                    else
                        params[i] = params[i - 2];
                }
            } else
            {
                for (int i = param; i < n_params; ++i)
                    params[i] = 0.0;
            }
        }


        void parse_path_do_cmd (bool final)
        {
            switch (cmd) 
            {
            case 'm':
                /* moveto */
                if (param == 2 || final)
                {
                    parse_path_default_xy (2);
                    paths.move_to(params[0], params[1]);
                    cp.point.x = rp.point.x = params[0];
                    cp.point.y = rp.point.y = params[1];
                    param = 0;
                    cmd = 'l'; /* implicit linetos after a moveto */
                }
                break;
            case 'l':
                /* lineto */
                if (param == 2 || final)
                {
                    parse_path_default_xy (2);
                    paths.line_to (params[0], params[1]);
                    cp.point.x = rp.point.x = params[0];
                    cp.point.y = rp.point.y = params[1];
                    param = 0;
                }
                break;
            case 'c':
                /* curveto */
                if (param == 6 || final)
                {
                    parse_path_default_xy (6);
                    double x1 = params[0], y1 = params[1];
                    double x2 = params[2], y2 = params[3];
                    double x3 = params[4], y3 = params[5];
                    paths.curve_to (x1, y1, x2, y2, x3, y3);
                    rp.point.x = x2; rp.point.y = y2;
                    cp.point.x = x3; cp.point.y = y3;
                    param = 0;
                }
                break;
            case 's':
                /* smooth curveto */
                if (param == 4 || final)
                {
                    parse_path_default_xy (4);
                    double x1 = 2 * cp.point.x - rp.point.x;
                    double y1 = 2 * cp.point.y - rp.point.y;
                    double x2 = params[0], y2 = params[1];
                    double x3 = params[2], y3 = params[3];
                    paths.curve_to (x1, y1, x2, y2, x3, y3);
                    rp.point.x = x2; rp.point.y = y2;
                    cp.point.x = x3; cp.point.y = y3;
                    param = 0;
                }
                break;
            case 'h':
                /* horizontal lineto */
                if (param == 1)
                {
                    paths.line_to (params[0], cp.point.y);
                    cp.point.x = rp.point.x = params[0];
                    rp.point.y = cp.point.y;
                    param = 0;
                }
                break;
            case 'v':
                /* vertical lineto */
                if (param == 1)
                {
                    paths.line_to (cp.point.x, params[0]);
                    rp.point.x = cp.point.x;
                    cp.point.y = rp.point.y = params[0];
                    param = 0;
                }
                break;
            case 'q':
                /* quadratic bezier curveto */

                /* non-normative reference:
                   http://www.icce.rug.nl/erikjan/bluefuzz/beziers/beziers/beziers.html
                 */
                if (param == 4 || final)
                {
                    parse_path_default_xy (4);
                    /* raise quadratic bezier to cubic */
                    double x1 = (cp.point.x + 2 * params[0]) * (1.0 / 3.0);
                    double y1 = (cp.point.y + 2 * params[1]) * (1.0 / 3.0);
                    double x3 = params[2], y3 = params[3];
                    double x2 = (x3 + 2 * params[0]) * (1.0 / 3.0);
                    double y2 = (y3 + 2 * params[1]) * (1.0 / 3.0);
                    paths.curve_to (x1, y1, x2, y2, x3, y3);
                    rp.point.x = params[0];
                    rp.point.y = params[1];
                    cp.point.x = x3;
                    cp.point.y = y3;
                    param = 0;
                }
                break;
            case 't':
                /* Truetype quadratic bezier curveto */
                if (param == 2 || final)
                {
                    /* quadratic control point */
                    double xc = 2 * cp.point.x - rp.point.x;
                    double yc = 2 * cp.point.y - rp.point.y;

                    /* generate a quadratic bezier with control point = xc, yc */
                    double x1 = (cp.point.x + 2 * xc) * (1.0 / 3.0);
                    double y1 = (cp.point.y + 2 * yc) * (1.0 / 3.0);
                    double x3 = params[0], y3 = params[1];
                    double x2 = (x3 + 2 * xc) * (1.0 / 3.0);
                    double y2 = (y3 + 2 * yc) * (1.0 / 3.0);
                    paths.curve_to (x1, y1, x2, y2, x3, y3);
                    rp.point.x = xc;
                    rp.point.y = yc;
                    cp.point.x = x3;
                    cp.point.y = y3;
                    param = 0;
                } else if (final)
                {
                    if (param > 2)
                    {
                        parse_path_default_xy (4);
                        /* raise quadratic bezier to cubic */
                        double x1 = (cp.point.x + 2 * params[0]) * (1.0 / 3.0);
                        double y1 = (cp.point.y + 2 * params[1]) * (1.0 / 3.0);
                        double x3 = params[2], y3 = params[3];
                        double x2 = (x3 + 2 * params[0]) * (1.0 / 3.0);
                        double y2 = (y3 + 2 * params[1]) * (1.0 / 3.0);
                        paths.curve_to (x1, y1, x2, y2, x3, y3);
                        rp.point.x = params[0];
                        rp.point.y = params[1];
                        cp.point.x = x3;
                        cp.point.y = y3;
                    } else {
                        parse_path_default_xy (2);
                        paths.line_to (params[0], params[1]);
                        cp.point.x = rp.point.x = params[0];
                        cp.point.y = rp.point.y = params[1];
                    }
                    param = 0;
                }
                break;
            case 'a':
                if (param == 7 || final)
                {
                    double x1 = cp.point.x;
                    double y1 = cp.point.y;

                    double rx = params[0];
                    double ry = params[1];

                    double x_axis_rotation = params[2];

                    bool large_arc_flag = params[3] != 0;
                    bool sweep_flag = params[4] != 0;

                    double x2 = params[5];
                    double y2 = params[6];

                    paths.arc (x1, y1, rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x2, y2);
                    rp.point.x = cp.point.x = x2;
                    rp.point.y = cp.point.y = y2;
                    param = 0;
                }
                break;
            default:
                param = 0;
            }
        }


        void path_end_of_number (double val, int sign, int exp_sign, int exp)
        {
            val *= sign * pow (10, exp_sign * exp);
            if (rel)
            {
                /* Handle relative coordinates. This switch statement attempts
                   to determine _what_ the coords are relative to. This is
                   underspecified in the 12 Apr working draft. */
                switch (cmd)
                {
                case 'm':
                    if ((param & 1) == 0)
                        val += paths.last_moveto().point.x;
                    else if ((param & 1) == 1)
                        val += paths.last_moveto().point.y;
                    break;
                case 'l':
                case 'c':
                case 's':
                case 'q':
                case 't':
                    /* rule: even-numbered params are x-relative, odd-numbered
                       are y-relative */
                    if ((param & 1) == 0)
                        val += cp.point.x;
                    else if ((param & 1) == 1)
                        val += cp.point.y;
                    break;
                case 'a':
                    /* rule: sixth and seventh are x and y, rest are not
                       relative */
                    if (param == 5)
                        val += cp.point.x;
                    else if (param == 6)
                        val += cp.point.y;
                    break;
                case 'h':
                    /* rule: x-relative */
                    val += cp.point.x;
                    break;
                case 'v':
                    /* rule: y-relative */
                    val += cp.point.y;
                    break;
                }
            }
            params[param++] = val;
            parse_path_do_cmd (false);
        }

        /* Returns the length of the number parsed, so it can be skipped
         * in rsvg_parse_path_data. Calls rsvg_path_end_number to have the number
         * processed in its command.
         */
        int parse_number (const ts::asptr &data)
        {
            enum parse_part_e
            {
                RSVGN_IN_PREINTEGER,
                RSVGN_IN_INTEGER,
                RSVGN_IN_FRACTION,
                RSVGN_IN_PREEXPONENT,
                RSVGN_IN_EXPONENT,

                RSVGN_GOT_SIGN = 0x1,
                RSVGN_GOT_EXPONENT_SIGN = 0x2

            } in = RSVGN_IN_PREINTEGER; // Current location within the number

            int got = 0x0; /* [bitfield] Having 2 of each of these is an error */
            bool end = false; /* Set to true if the number should end after a char */
            bool error = false; /* Set to true if the number ended due to an error */

            double value = 0.0;
            double fraction = 1.0;
            int sign = +1; /* Presume the INTEGER is positive if it has no sign */
            int exponent = 0;
            int exponent_sign = +1; /* Presume the EXPONENT is positive if it has no sign */

            int length = 0;
            while (length < data.l && !end && !error)
            {
                char c = data.s[length];
                switch (in)
                {
                case RSVGN_IN_PREINTEGER: // No numbers yet, we're just starting out
                /* LEGAL: + - .->FRACTION DIGIT->INTEGER */
                    if (c == '+' || c == '-')
                    {
                        if (got & RSVGN_GOT_SIGN)
                        {
                            error = true; // Two signs: not allowed
                        } else 
                        {
                            sign = c == '+' ? +1 : -1;
                            got |= RSVGN_GOT_SIGN;
                        }
                    } else if (c == '.')
                    {
                        in = RSVGN_IN_FRACTION;
                    } else if (c >= '0' && c <= '9')
                    {
                        value = c - '0';
                        in = RSVGN_IN_INTEGER;
                    }
                    break;
                case RSVGN_IN_INTEGER: // Previous character(s) was/were digit(s)
                    /* LEGAL: DIGIT .->FRACTION E->PREEXPONENT */
                    if (c >= '0' && c <= '9')
                        value = value * 10 + (c - '0');
                    else if (c == '.')
                        in = RSVGN_IN_FRACTION;
                    else if (c == 'e' || c == 'E')
                        in = RSVGN_IN_PREEXPONENT;
                    else
                        end = true;
                    break;
                case RSVGN_IN_FRACTION: // Previously, digit(s) in the fractional part
                    /* LEGAL: DIGIT E->PREEXPONENT */
                    if (c >= '0' && c <= '9')
                    {
                        fraction *= 0.1;
                        value += fraction * (c - '0');
                    } else if (c == 'e' || c == 'E')
                    {
                        in = RSVGN_IN_PREEXPONENT;
                    } else
                    {
                        end = true;
                    }
                    break;
                case RSVGN_IN_PREEXPONENT: /* Right after E */
                    /* LEGAL: + - DIGIT->EXPONENT */
                    if (c == '+' || c == '-') {
                        if (got & RSVGN_GOT_EXPONENT_SIGN) {
                            error = TRUE; /* Two signs: not allowed */
                        } else {
                            exponent_sign = c == '+' ? +1 : -1;
                            got |= RSVGN_GOT_EXPONENT_SIGN;
                        }
                    } else if (c >= '0' && c <= '9') {
                        exponent = c - '0';
                        in = RSVGN_IN_EXPONENT;
                    }
                    break;
                case RSVGN_IN_EXPONENT: /* After E and the sign, if applicable */
                    /* LEGAL: DIGIT */
                    if (c >= '0' && c <= '9') {
                        exponent = exponent * 10 + (c - '0');
                    } else {
                        end = true;
                    }
                    break;
                }
                length++;
            }

            /* TODO? if (error) report_the_error_somehow(); */
            path_end_of_number(value, sign, exponent_sign, exponent);
            return end /* && !error */ ? length - 1 : length;
        }

    };
}


void rsvg_path_c::parse_path(const ts::asptr &data)
{
    path_parser_s ctx( path, data );
}

/*virtual*/ void rsvg_path_c::load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *node)
{
    strokenfill = ctx.deffillstroke;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("d")))
            parse_path( ai->value() );
        else if (strokenfill.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx, ts::asptr(ai->value()));
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}

/*virtual*/ void rsvg_path_c::render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter)
{
    RENDER_PROLOG
    path.render(strokenfill, surf);
}

void rsvg_polygon_c::parse_points(const ts::asptr &pstr, bool close)
{
    bool first_point = false;
    for( ts::token<char> pts( pstr, ' ' ); pts; ++pts )
    {
        ts::token<char> v( *pts, ',' );
        double x = v->as_double();
        ++v;
        if (!v) return;
        double y = v->as_double();
        if (first_point)
            path.move_to(x,y), first_point = false;
        else
            path.line_to(x,y);
    }
    if (close)
        path.close();
}

void rsvg_polygon_c::load(rsvg_load_context_s & ctx, ts::rapidxml::xml_node<char>* node)
{
    strokenfill = ctx.deffillstroke;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("points")))
            parse_points(ai->value(), true);
        else if (strokenfill.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}


void rsvg_polyline_c::load(rsvg_load_context_s & ctx, ts::rapidxml::xml_node<char>* node)
{
    strokenfill = ctx.deffillstroke;
    strokenfill.fillcolor = 0;

    for (ts::rapidxml::attribute_iterator<char> ai(node), e; ai != e; ++ai)
    {
        ts::pstr_c attrname(ai->name());
        if (attrname.equals(CONSTASTR("points")))
            parse_points(ai->value(), false);
        else if (strokenfill.load(&*ai)) {}
        else if (attrname.equals(CONSTASTR("filter")))
            load_filter_ref(ctx, ai->value());
        else if (attrname.equals(CONSTASTR("transform")))
            load_transform(ctx, ai->value());
    }
}
