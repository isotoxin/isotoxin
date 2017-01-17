#pragma once

struct rsvg_stroke_s;
struct rsvg_stroke_and_fill_s;

class rsvg_cairo_surface_c
{
    ts::bmpcore_exbody_s extbody;
    cairo_surface_t *s = nullptr;
    cairo_t *c = nullptr;
public:
    rsvg_cairo_surface_c( const ts::bmpcore_exbody_s &extbody, const cairo_matrix_t *matrix );
    ~rsvg_cairo_surface_c();

    cairo_t *cr() {return c;}
    cairo_surface_t *cs() {return s;}
    const ts::bmpcore_exbody_s &extb() const { return extbody; }
};

class cairo_paths_c
{
    struct cairo_path_el_s : public ts::movable_flag<true>
    {
        cairo_path_data_t pd;
        cairo_path_el_s() {};
    };

    ts::array_inplace_t< cairo_path_el_s, 16 > paths;
    int last_move_to_index = -1;
public:

    void move_to( double x, double y );
    void line_to(double x, double y);
    void arc(double x1, double y1, double rx, double ry, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, double x2, double y2);
    void arc_segment( double xc, double yc, float th0, float th1, double rx, double ry, double x_axis_rotation);
    void curve_to( double x1, double y1, double x2, double y2, double x3, double y3);
    void close();

    void render( const rsvg_stroke_and_fill_s &snf, rsvg_cairo_surface_c &surf );
    void render( const rsvg_stroke_s &snf, rsvg_cairo_surface_c &surf );

    const cairo_path_data_t &last_moveto() const
    {
        if (last_move_to_index >= 0)
            return paths.get(last_move_to_index+1).pd;
        static cairo_path_data_t dummy;
        return dummy;
    }

    const cairo_path_data_t &last() const
    {
        if ( paths.size() )
            return paths.get( paths.size() - 1 ).pd;
        static cairo_path_data_t dummy;
        return dummy;
    }
};