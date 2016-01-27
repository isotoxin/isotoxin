#pragma once

struct rsvg_load_context_s;
struct rsvg_working_surf_s;
class rsvg_node_c;

#define RSVG_FILTER_SOURCE_GRAPHIC 0
#define RSVG_FILTER_SOURCE_ALPHA -1

#define RSVG_FILTER_TARGRET_CURRENT 1

class rsvg_filter_c
{
protected:
    int source;
    int target = RSVG_FILTER_TARGRET_CURRENT;
    bool fix_source(rsvg_load_context_s &ctx, int &olds, int &news);
    bool load_s(rsvg_load_context_s &ctx, const ts::rapidxml::xml_attribute<char>& attr);

public:
    rsvg_filter_c(int sourcesrfc):source(sourcesrfc) {}
    virtual ~rsvg_filter_c() {}

    virtual void load( rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node ) = 0;
    virtual void apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const = 0;
    virtual bool fix(rsvg_load_context_s &ctx, int &olds, int &news) { return false; }
    
    bool repltarget(int olds, int news) { if (target == olds) { target = news; return true; } return false; }

    int get_source_index() const {return source;}
    int get_target_index() const {return target;}
};

class rsvg_filter_offset_c : public rsvg_filter_c
{
    ts::vec2 offset = ts::vec2(0);
public:
    rsvg_filter_offset_c(int sourcesrfc):rsvg_filter_c(sourcesrfc) {}
    /*virtual*/ void load( rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node ) override;
    /*virtual*/ void apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const override;
    /*virtual*/ bool fix(rsvg_load_context_s &ctx, int &olds, int &news) override;
};

class rsvg_filter_blend_c : public rsvg_filter_c
{
    int source2;
    enum blendmode_e
    {
        BM_NORMAL,
        BM_MULTIPLY,
        BM_SCREEN,
        BM_DARKEN,
        BM_LIGHTEN,
    } blendmode = BM_NORMAL;
public:
    rsvg_filter_blend_c(int sourcesrfc):rsvg_filter_c(sourcesrfc), source2(sourcesrfc) {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node) override;
    /*virtual*/ void apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const override;
};

class rsvg_filter_gaussian_c : public rsvg_filter_c
{
    ts::vec2 sd = ts::vec2(0);
public:
    rsvg_filter_gaussian_c(int sourcesrfc) :rsvg_filter_c(sourcesrfc) {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node) override;
    /*virtual*/ void apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const override;
};

class rsvg_filter_ctransfer_c : public rsvg_filter_c
{
    struct param_s
    {
        typedef ts::TSCOLOR CTR(const param_s &, ts::TSCOLOR);
        CTR * func;

        int c;
        float slope;
        float intercept;
    };

    ts::tbuf0_t<param_s> processing;

    static ts::TSCOLOR CTR_A_LINEAR_SLOPE(const param_s &, ts::TSCOLOR);
    static ts::TSCOLOR CTR_C_LINEAR_SLOPE(const param_s &, ts::TSCOLOR);

    void c_load(rsvg_load_context_s & ctx, ts::rapidxml::xml_node<char>* node, int c);

public:
    rsvg_filter_ctransfer_c(int sourcesrfc) :rsvg_filter_c(sourcesrfc) {}
    /*virtual*/ void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node) override;
    /*virtual*/ void apply(rsvg_working_surf_s * surfs, const cairo_matrix_t *m) const override;

};


class rsvg_filters_group_c : public ts::shared_object
{
    ts::array_del_t<rsvg_filter_c, 0> filters;
    ts::ivec2 offset;
    ts::ivec2 size;
    int numsurfaces = 2; // at least 2 surfaces: source and target
public:
    rsvg_filters_group_c() {}
    ~rsvg_filters_group_c() {}

    void load(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node);
    void render( rsvg_node_c *node, const ts::ivec2 &pos, rsvg_cairo_surface_c &surf );
};

bool load_filters(rsvg_load_context_s &ctx, ts::rapidxml::xml_node<char>* node);
