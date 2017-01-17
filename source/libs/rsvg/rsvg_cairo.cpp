#include "_precompiled.h"

//-V::550

#ifdef _MSC_VER
#define FORCEINLINE __forceinline
#endif
#ifdef __GNUC__
#define FORCEINLINE inline
#endif

FORCEINLINE double fRED( ts::TSCOLOR c )
{
    return (double)ts::RED(c) * (1.0/255.0);
}
FORCEINLINE double fGREEN(ts::TSCOLOR c)
{
    return (double)ts::GREEN(c) * (1.0 / 255.0);
}
FORCEINLINE double fBLUE(ts::TSCOLOR c)
{
    return (double)ts::BLUE(c) * (1.0 / 255.0);
}
FORCEINLINE double fALPHA(ts::TSCOLOR c)
{
    return (double)ts::ALPHA(c) * (1.0 / 255.0);
}

rsvg_cairo_surface_c::rsvg_cairo_surface_c(const ts::bmpcore_exbody_s &extbody, const cairo_matrix_t *matrix):extbody(extbody)
{
    if ( extbody.info().bytepp() == 4 )
    {
        s = cairo_image_surface_create_for_data(extbody(), CAIRO_FORMAT_ARGB32, extbody.info().sz.x, extbody.info().sz.y, extbody.info().pitch);
        c = cairo_create(s);
        cairo_set_antialias(c,CAIRO_ANTIALIAS_GOOD);

        if (matrix)
            cairo_set_matrix( c, matrix );
    }
}

rsvg_cairo_surface_c::~rsvg_cairo_surface_c()
{
    if (s)
        cairo_surface_destroy(s);

    if (c)
        cairo_destroy(c);
}



void cairo_paths_c::move_to(double x, double y)
{
    last_move_to_index = (int)paths.size();
    cairo_path_data_t &data = paths.add().pd;
    data.header.type = CAIRO_PATH_MOVE_TO;
    data.header.length = 2;
    cairo_path_data_t &p = paths.add().pd;
    p.point.x = x;
    p.point.y = y;
}

void cairo_paths_c::close()
{
    cairo_path_data_t &data = paths.add().pd;
    data.header.type = CAIRO_PATH_CLOSE_PATH;
    data.header.length = 1;

    /* Add a 'move-to' element */
    if (last_move_to_index >= 0)
    {
        const auto &p = paths.get(last_move_to_index + 1).pd.point;
        move_to( p.x, p.y );
    }
}

void cairo_paths_c::line_to(double x, double y)
{
    cairo_path_data_t &data = paths.add().pd;
    data.header.type = CAIRO_PATH_LINE_TO;
    data.header.length = 2;

    cairo_path_data_t &p = paths.add().pd;
    p.point.x = x;
    p.point.y = y;

}

void cairo_paths_c::arc(double x1, double y1, double rx, double ry, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, double x2, double y2)
{
    double sinf, cosf;
    double x1_, y1_;
    double cx_, cy_, cx, cy;
    double gamma;
    double theta1, delta_theta;
    double k1, k2, k3, k4, k5;

    int i, n_segs;

    if (x1 == x2 && y1 == y2)
        return;

    /* X-axis */
    ts::sincos((float)(x_axis_rotation * M_PI / 180.0), sinf, cosf);

    rx = fabs (rx);
    ry = fabs (ry);

    /* Check the radius against floading point underflow.
       See http://bugs.debian.org/508443 */
    if ((rx < DBL_EPSILON) || (ry < DBL_EPSILON))
    {
        line_to (x2, y2);
        return;
    }

    k1 = (x1 - x2) / 2;
    k2 = (y1 - y2) / 2;

    x1_ = cosf * k1 + sinf * k2;
    y1_ = -sinf * k1 + cosf * k2;

    gamma = (x1_ * x1_) / (rx * rx) + (y1_ * y1_) / (ry * ry);
    if (gamma > 1) {
        rx *= sqrt (gamma);
        ry *= sqrt (gamma);
    }

    /* Compute the center */

    k1 = rx * rx * y1_ * y1_ + ry * ry * x1_ * x1_;
    if (k1 == 0)
        return;

    k1 = sqrt (fabs ((rx * rx * ry * ry) / k1 - 1));
    if (sweep_flag == large_arc_flag)
        k1 = -k1;

    cx_ = k1 * rx * y1_ / ry;
    cy_ = -k1 * ry * x1_ / rx;
    
    cx = cosf * cx_ - sinf * cy_ + (x1 + x2) / 2;
    cy = sinf * cx_ + cosf * cy_ + (y1 + y2) / 2;

    /* Compute start angle */

    k1 = (x1_ - cx_) / rx;
    k2 = (y1_ - cy_) / ry;
    k3 = (-x1_ - cx_) / rx;
    k4 = (-y1_ - cy_) / ry;

    k5 = sqrt (fabs (k1 * k1 + k2 * k2));
    if (k5 == 0)
        return;

    k5 = k1 / k5;
    k5 = ts::CLAMP (k5, -1, 1);
    theta1 = acos (k5);
    if (k2 < 0)
        theta1 = -theta1;

    /* Compute delta_theta */

    k5 = sqrt (fabs ((k1 * k1 + k2 * k2) * (k3 * k3 + k4 * k4)));
    if (k5 == 0)
        return;

    k5 = (k1 * k3 + k2 * k4) / k5;
    k5 = ts::CLAMP (k5, -1, 1);
    delta_theta = acos (k5);
    if (k1 * k4 - k3 * k2 < 0)
        delta_theta = -delta_theta;

    if (sweep_flag && delta_theta < 0)
        delta_theta += M_PI * 2;
    else if (!sweep_flag && delta_theta > 0)
        delta_theta -= M_PI * 2;
   
    /* Now draw the arc */

    n_segs = ts::lround( ceil (fabs (delta_theta / (M_PI * 0.5 + 0.001))));

    for (i = 0; i < n_segs; i++)
        arc_segment (   cx, cy,
                        (float)(theta1 + i * delta_theta / n_segs),
                        (float)(theta1 + (i + 1) * delta_theta / n_segs),
                        rx, ry, x_axis_rotation);
}

void cairo_paths_c::arc_segment( double xc, double yc, float th0, float th1, double rx, double ry, double x_axis_rotation)
{
    double x1, y1, x2, y2, x3, y3;
    double t;
    double th_half;
    double sinf, cosf;

    ts::sincos((float)(x_axis_rotation * M_PI / 180.0), sinf, cosf);

    double sin_th0, cos_th0;
    ts::sincos(th0, sin_th0, cos_th0);

    double sin_th1, cos_th1;
    ts::sincos(th1, sin_th1, cos_th1);

    th_half = 0.5 * (th1 - th0);
    double sin_temp1 = sin(th_half * 0.5);

    t = (8.0 / 3.0) * sin_temp1 * sin_temp1 / sin(th_half);
    x1 = rx*(cos_th0 - t * sin_th0);
    y1 = ry*(sin_th0 + t * cos_th0);
    x3 = rx*cos_th1;
    y3 = ry*sin_th1;
    x2 = x3 + rx*(t * sin_th1);
    y2 = y3 + ry*(-t * cos_th1);

    curve_to(   xc + cosf*x1 - sinf*y1,
                yc + sinf*x1 + cosf*y1,
                xc + cosf*x2 - sinf*y2,
                yc + sinf*x2 + cosf*y2,
                xc + cosf*x3 - sinf*y3,
                yc + sinf*x3 + cosf*y3  );

}


void cairo_paths_c::curve_to(double x1, double y1, double x2, double y2, double x3, double y3)
{
    cairo_path_data_t &data = paths.add().pd;
    data.header.type = CAIRO_PATH_CURVE_TO;
    data.header.length = 4;

    cairo_path_data_t &p1 = paths.add().pd;
    p1.point.x = x1;
    p1.point.y = y1;

    cairo_path_data_t &p2 = paths.add().pd;
    p2.point.x = x2;
    p2.point.y = y2;

    cairo_path_data_t &p3 = paths.add().pd;
    p3.point.x = x3;
    p3.point.y = y3;
}

static void setup_stroke_dash( cairo_t *cr, const rsvg_stroke_s&stroke )
{
    if (stroke.dasharray.count())
        cairo_set_dash(cr, stroke.dasharray.begin(), (int)stroke.dasharray.count(), stroke.dash_offset);
}

void cairo_paths_c::render( const rsvg_stroke_and_fill_s &snf, rsvg_cairo_surface_c &surf )
{
    cairo_path_t p;
    p.status = CAIRO_STATUS_SUCCESS;
    p.data = &paths.begin()->pd;
    p.num_data = static_cast<int>(paths.size());

    cairo_new_path(surf.cr());
    cairo_append_path(surf.cr(), &p);

    if ( ts::ALPHA(snf.stroke) > 0 )
        cairo_set_line_width(surf.cr(), snf.stroke_width);

    if ( ts::ALPHA(snf.fillcolor) > 0 )
    {
        cairo_set_source_rgba (surf.cr(), fRED(snf.fillcolor), fGREEN(snf.fillcolor), fBLUE(snf.fillcolor), fALPHA(snf.fillcolor));
    
        if (ts::ALPHA(snf.stroke) > 0)
            cairo_fill_preserve(surf.cr());
        else
            cairo_fill(surf.cr());
    }

    if (ts::ALPHA(snf.stroke) > 0)
    {
        setup_stroke_dash( surf.cr(), snf );
        cairo_set_source_rgba(surf.cr(), fRED(snf.stroke), fGREEN(snf.stroke), fBLUE(snf.stroke), fALPHA(snf.stroke));
        cairo_stroke(surf.cr());
    }
}

void cairo_paths_c::render( const rsvg_stroke_s &snf, rsvg_cairo_surface_c &surf )
{
    cairo_path_t p;
    p.status = CAIRO_STATUS_SUCCESS;
    p.data = &paths.begin()->pd;
    p.num_data = static_cast<int>(paths.size());

    cairo_new_path(surf.cr());
    cairo_append_path(surf.cr(), &p);

    cairo_set_line_width(surf.cr(), snf.stroke_width);
    setup_stroke_dash( surf.cr(), snf );
    cairo_set_source_rgba(surf.cr(), fRED(snf.stroke), fGREEN(snf.stroke), fBLUE(snf.stroke), fALPHA(snf.stroke));
    cairo_stroke(surf.cr());

}