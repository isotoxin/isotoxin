#pragma once

#include "rsvg_cairo.h"
#include "rsvg_filter.h"
#include "rsvg_animation.h"

typedef ts::array_del_t< rsvg_animation_c, 4 > allinims_t;

struct rsvg_stroke_s
{
    float stroke_width = 0;
    float dash_offset = 0;
    ts::TSCOLOR stroke = 0;
    ts::tbuf_t<double> dasharray;
    bool load(ts::rapidxml::xml_attribute<char> *a);
};

struct rsvg_stroke_and_fill_s : public rsvg_stroke_s
{
    ts::TSCOLOR fillcolor = ts::ARGB(0, 0, 0);
    bool load(ts::rapidxml::xml_attribute<char> *a);
};

struct rsvg_load_context_s;
class rsvg_svg_c;

class rsvg_node_c
{
protected:

    ts::shared_ptr< rsvg_filters_group_c > filter;

    struct transform_s
    {
        cairo_matrix_t m;
        rsvg_animation_transform_c *a = nullptr;
        transform_s()
        {
            cairo_matrix_init_identity(&m);
        }
        const cairo_matrix_t *get_transform() const
        {
            return a ? a->get_tansform() : &m;
        };
    };

    UNIQUE_PTR(transform_s) transform;
    rsvg_node_c() {}

    virtual void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) = 0;

    const cairo_matrix_t *get_transform() const
    {
        return transform.get() ? transform.get()->get_transform() : nullptr;
    }

public:
    virtual ~rsvg_node_c();
    virtual void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter ) = 0;

    void load_filter_ref(rsvg_load_context_s &ctx, const ts::asptr&fref);
    void load_transform(rsvg_load_context_s &ctx, const ts::asptr&fref);
    void load_animation(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n);

    void set_transform_animation( rsvg_animation_transform_c *ta );
};

class rsvg_group_c
{
protected:
    ts::array_del_t<rsvg_node_c, 0> children;
    void load(rsvg_node_c *self, rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *node);
    void render( const ts::ivec2 &offset, rsvg_cairo_surface_c &surf );
public:

};

class rsvg_svg_c : public rsvg_node_c, public rsvg_group_c
{
    ts::ivec2 sz = ts::ivec2(0);
    allinims_t anims;
    ts::Time ticktime = ts::Time::undefined();
    ts::Time zerotime = ts::Time::undefined();
public:
    rsvg_svg_c() {}

    static rsvg_svg_c *build_from_xml(char * svgxml /* string can be modified! */);

    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    void render( const ts::bmpcore_exbody_s &pixbuf, const ts::ivec2 &pos = ts::ivec2(0), const cairo_matrix_t *matrix = nullptr );
    /*virtual*/ void render( const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter ) override;

    const ts::ivec2 &size() const { return sz; }

    void release();


    void anim_tick();
    void anim_reset();

    bool is_animated() const { return anims.size() > 0; }

};

class rsvg_g_c : public rsvg_node_c, public rsvg_group_c
{
public:
    rsvg_g_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    /*virtual*/ void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter) override;
};

class rsvg_circle_c : public rsvg_node_c
{
    ts::vec3 xyr = ts::vec3(0);
    rsvg_stroke_and_fill_s strokenfill;
public:
    rsvg_circle_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    /*virtual*/ void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter) override;
};

class rsvg_rect_c : public rsvg_node_c
{
    ts::vec2 xy = ts::vec2(0);
    ts::vec2 sz = ts::vec2(0);
    ts::vec2 r = ts::vec2(0);
    rsvg_stroke_and_fill_s strokenfill;
public:
    rsvg_rect_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    /*virtual*/ void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter) override;
};

class rsvg_line_c : public rsvg_node_c
{
    ts::vec2 xy1 = ts::vec2(0);
    ts::vec2 xy2 = ts::vec2(0);
    rsvg_stroke_s stroke;
public:
    rsvg_line_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    /*virtual*/ void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter) override;
};

class rsvg_path_c : public rsvg_node_c
{
    void parse_path(const ts::asptr &pstr);
protected:
    cairo_paths_c path;
    rsvg_stroke_and_fill_s strokenfill;
public:
    rsvg_path_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
    /*virtual*/ void render(const ts::ivec2 &offset, rsvg_cairo_surface_c &surf, bool use_filter) override;
};

class rsvg_polygon_c : public rsvg_path_c
{
protected:
    void parse_points(const ts::asptr &pstr, bool close);
public:
    rsvg_polygon_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
};

class rsvg_polyline_c : public rsvg_polygon_c
{
public:
    rsvg_polyline_c() {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char> *n) override;
};
