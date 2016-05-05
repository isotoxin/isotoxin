#include "stdafx.h"

using namespace ts;

int proc_rsvg( const wstrings_c & pars )
{
    if ( pars.size() < 2 ) return 0;
    wstr_c svgfile = pars.get( 1 ); fix_path( svgfile, FNO_SIMPLIFY );

    if ( !is_file_exists( svgfile.as_sptr() ) )
    {
        Print( FOREGROUND_RED, "svg file not found: %s\n", to_str( svgfile ).cstr() ); return 0;
    }

    buf_c svg; svg.load_from_disk_file( svgfile );
    str_c svgs( svg.cstr() );

    if ( rsvg_svg_c *n = rsvg_svg_c::build_from_xml( svgs.str() ) )
    {
        ts::bitmap_c face_surface;

        if ( face_surface.info().sz != n->size() )
            face_surface.create_ARGB( n->size() );
        face_surface.fill( 0 );
        n->render( face_surface.extbody() );

        face_surface.unmultiply();
        face_surface.save_as_png( fn_change_ext(svgfile, CONSTWSTR("png")) );

        n->release();
    }
    return 0;
}