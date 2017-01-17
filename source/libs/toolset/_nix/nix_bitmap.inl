#include "nix_common.inl"

template<typename CORE> void bitmap_t<CORE>::render_cursor( const ivec2&pos, buf_c &cache )
{
}

namespace
{
    struct drawable_bitmap_internal_s
    {
        void *dummy;
    };
}

void drawable_bitmap_c::clear()
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

}

void    drawable_bitmap_c::create( const ivec2 &sz, int monitor )
{
    clear();

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

}

bool drawable_bitmap_c::create_from_bitmap( const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy, bool premultiply, bool detect_alpha_pixels )
{
    clear();

    bool alphablend = false;

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    return alphablend;
}

void    drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, const ivec2 & pos_from_dc )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );


}

void drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, bool save16as32 )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

}



drawable_bitmap_c::drawable_bitmap_c( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );

}

drawable_bitmap_c &drawable_bitmap_c::operator=( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );

    return *this;
}

void drawable_bitmap_c::grab_screen( const ts::irect &screenr, const ts::ivec2& p_in_bitmap )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

}

#if 0
template <class W> void work( W & w )
{
    if ( body() == nullptr ) return;
    BITMAP bmpInfo;
    GetObject( m_mem_bitmap, sizeof( BITMAP ), &bmpInfo );

    for ( aint j = 0; j < aint( bmpInfo.bmHeight ); ++j )
    {
        uint32 *x = (uint32 *)( (BYTE *)body() + j * aint( bmpInfo.bmWidthBytes ) );
        for ( aint i = 0; i < bmpInfo.bmWidth; ++i )
        {
            x[ i ] = w( i, j, x[ i ] );
        }
    }
}

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, int alpha )
{
    if ( dt.eb )
    {
        bmp.draw( ( *dt.eb ), xx, yy, alpha );
    } else
    {
        drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( bmp.data );

        if ( alpha > 0 )
        {
            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
            AlphaBlend( dt.dc, (int)xx, (int)yy, bmp.info().sz.x, bmp.info().sz.y, d.m_mem_dc, 0, 0, bmp.info().sz.x, bmp.info().sz.y, blendPixelFunction );
        }
        else if ( alpha < 0 )
        {
            BitBlt( dt.dc, (int)xx, (int)yy, bmp.info().sz.x, bmp.info().sz.y, d.m_mem_dc, 0, 0, SRCCOPY );
        }
    }
}

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, const irect &r, int alpha )
{
    if ( dt.eb )
    {
        bmp.draw( ( *dt.eb ), xx, yy, r, alpha );
    } else
    {
        drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( bmp.data );

        if ( alpha > 0 )
        {
            BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
            AlphaBlend( dt.dc, (int)xx, (int)yy, r.width(), r.height(), d.m_mem_dc, r.lt.x, r.lt.y, r.width(), r.height(), blendPixelFunction );
        }
        else if ( alpha < 0 )
        {
            BitBlt( dt.dc, (int)xx, (int)yy, r.width(), r.height(), d.m_mem_dc, r.lt.x, r.lt.y, SRCCOPY );
        }
    }
}

#endif
