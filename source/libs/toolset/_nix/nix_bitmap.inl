#include "nix_common.inl"

template<typename CORE> void bitmap_t<CORE>::render_cursor( const ivec2&pos, buf_c &cache )
{
}

namespace
{
    struct drawable_bitmap_internal_s
    {
        XImage image = {};
        Pixmap pixmap;
        char *data;

        void setup_with_size(const ivec2 &sz)
        {
            image.width = sz.x;
            image.height = sz.y;
            image.depth = 24;
            image.bits_per_pixel = 32;
            image.format = ZPixmap;
            image.byte_order = LSBFirst;
            image.bitmap_unit = 32;
            image.bitmap_bit_order = LSBFirst;
            image.bitmap_pad = 32;
            image.bytes_per_line = sz.x * 4;
            image.red_mask = 0xFF0000;
            image.green_mask = 0xFF00;
            image.blue_mask = 0xFF;
            data = (char*)MM_ALLOC(sz.x * sz.y * 4 + 4);
            image.data = (char *)(((size_t)(data+3)) & (~3ull));

            //master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
            //Visual *v = DefaultVisual(istuff.X11, istuff.screen);

            //XImage* xWindowBuffer = XCreateImage(istuff.X11, v, 24, ZPixmap, 0, image.data, sz.x, sz.y, 32, 0);
            //XDestroyImage(xWindowBuffer);

        }

    };
}

void drawable_bitmap_c::clear()
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    TS_STATIC_CHECK(sizeof(drawable_bitmap_internal_s) <= sizeof(data), "1");

    if (nullptr != body())
    {
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        if (d.pixmap)
            XFreePixmap(istuff.X11, d.pixmap);
        MM_FREE(d.data);
    }
    memset(&d.image, 0, sizeof(XImage));
    d.pixmap = 0;

    core.m_info.sz = ivec2(0);
    core.m_info.bitpp = 0;
    core.m_info.pitch = 0;
    core.m_body = nullptr;

}

void    drawable_bitmap_c::create( const ivec2 &sz, int monitor )
{
    clear();

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    d.setup_with_size(sz);

    core.m_info.sz = sz;
    core.m_info.bitpp = d.image.bits_per_pixel;
    core.m_info.pitch = d.image.bytes_per_line;
    core.m_body = (uint8 *)d.image.data;

}

bool drawable_bitmap_c::create_from_bitmap( const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy, bool premultiply, bool detect_alpha_pixels )
{
    clear();

    bool alphablend = false;

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    d.setup_with_size(bmp.info().sz);

    DEBUG_BREAK();

    return alphablend;
}

void    drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, const ivec2 & pos_from_dc )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    DEBUG_BREAK();
}

void drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, bool save16as32 )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    DEBUG_BREAK();
}



drawable_bitmap_c::drawable_bitmap_c( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );
    memcpy(&d,&obd,sizeof(drawable_bitmap_internal_s));
    memset(&obd,0,sizeof(drawable_bitmap_internal_s));
}

drawable_bitmap_c &drawable_bitmap_c::operator=( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );
    clear();
    memcpy(&d,&obd,sizeof(drawable_bitmap_internal_s));
    memset(&obd,0,sizeof(drawable_bitmap_internal_s));
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


#endif

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, int alpha )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( bmp.data );

    if ( alpha > 0 )
    {
        //BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
        //AlphaBlend( dt.dc, (int)xx, (int)yy, bmp.info().sz.x, bmp.info().sz.y, d.m_mem_dc, 0, 0, bmp.info().sz.x, bmp.info().sz.y, blendPixelFunction );
        DEBUG_BREAK();
    }
    else if ( alpha < 0 )
    {
        //BitBlt( dt.dc, (int)xx, (int)yy, bmp.info().sz.x, bmp.info().sz.y, d.m_mem_dc, 0, 0, SRCCOPY );
        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        GC default_gc = DefaultGC(istuff.X11, istuff.screen);

        if (!d.pixmap)
            d.pixmap = XCreatePixmap(istuff.X11, dt.x11w, bmp.info().sz.x, bmp.info().sz.y, istuff.depth);
        XPutImage(istuff.X11, d.pixmap, default_gc, &d.image, 0, 0, xx, yy, bmp.info().sz.x, bmp.info().sz.y);
        XCopyArea(istuff.X11, d.pixmap, dt.x11w, default_gc, 0, 0, bmp.info().sz.x, bmp.info().sz.y, xx, yy);
        //XFreePixmap(istuff.X11, pixmap);
    }
}

void drawable_bitmap_draw( drawable_bitmap_c&bmp, const draw_target_s &dt, aint xx, aint yy, const irect &r, int alpha )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( bmp.data );

    if ( alpha > 0 )
    {
        //BLENDFUNCTION blendPixelFunction = { AC_SRC_OVER, 0, (uint8)alpha, AC_SRC_ALPHA };
        //AlphaBlend( dt.dc, (int)xx, (int)yy, r.width(), r.height(), d.m_mem_dc, r.lt.x, r.lt.y, r.width(), r.height(), blendPixelFunction );
    }
    else if ( alpha < 0 )
    {
        //bmp.save_as_png(CONSTWSTR("~/zzz.png"));

        //BitBlt( dt.dc, (int)xx, (int)yy, r.width(), r.height(), d.m_mem_dc, r.lt.x, r.lt.y, SRCCOPY );

        master_internal_stuff_s &istuff = *(master_internal_stuff_s *)&master().internal_stuff;
        GC default_gc = DefaultGC(istuff.X11, istuff.screen);

        if (!d.pixmap)
            d.pixmap = XCreatePixmap(istuff.X11, dt.x11w, bmp.info().sz.x, bmp.info().sz.y, istuff.depth);
        int er = XPutImage(istuff.X11, d.pixmap, default_gc, &d.image, r.lt.x, r.lt.y, r.lt.x, r.lt.y, r.width(), r.height());
        XCopyArea(istuff.X11, d.pixmap, dt.x11w, default_gc, r.lt.x, r.lt.y, r.width(), r.height(), xx, yy);
        //XFreePixmap(istuff.X11, pixmap);

    }
}

