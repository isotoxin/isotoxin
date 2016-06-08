#include "win32_common.inl"

class disable_fp_exceptions_c
{
    unsigned int state_store;
public:
    disable_fp_exceptions_c()
    {
        _controlfp_s( &state_store, 0, 0 );
        unsigned int unused;
        _controlfp_s( &unused, state_store | ( EM_OVERFLOW | EM_ZERODIVIDE | EM_DENORMAL | EM_INVALID ), MCW_EM );
    }
    ~disable_fp_exceptions_c()
    {
        _clearfp();
        unsigned int unused;
        _controlfp_s( &unused, state_store, MCW_EM );
    }
};

class enable_fp_exceptions_c
{
    unsigned int state_store;
public:
    enable_fp_exceptions_c()
    {
        _clearfp();
        _controlfp_s( &state_store, 0, 0 );
        unsigned int unused;
        _controlfp_s( &unused, state_store & ~( EM_OVERFLOW | EM_ZERODIVIDE | EM_DENORMAL | EM_INVALID ), MCW_EM );
    }
    ~enable_fp_exceptions_c()
    {
        unsigned int unused;
        _controlfp_s( &unused, state_store, MCW_EM );
    }
};

template<typename CORE> void bitmap_t<CORE>::render_cursor( const ivec2&pos, buf_c &cache )
{
    struct data_s
    {
        HICON iconcached;
        ts::ivec2 sz, hotspot;
    };

    data_s *d = nullptr;
    if ( cache.size() >= sizeof( data_s ) )
        d = (data_s *)cache.data();

    CURSORINFO ci = { sizeof( CURSORINFO ) };

    auto preparedata = [&]( HICON icn, HBITMAP hmask, HBITMAP hcolor )
    {
        BITMAP bm;

        if ( 0 == GetObject( hmask, sizeof( BITMAP ), &bm ) )
            return false;

        ts::ivec2 sz( bm.bmWidth, bm.bmHeight );
        if ( !hcolor ) sz.y /= 2;

        drawable_bitmap_c dbmp;
        dbmp.create( ivec2( sz.x, sz.y * 2 ) );
        dbmp.fill( ivec2( 0 ), sz, ts::ARGB( 0, 0, 0 ) );
        dbmp.fill( ivec2( 0, sz.y ), sz, ts::ARGB( 255, 255, 255 ) );

        drawable_bitmap_internal_s &bd = ref_cast<drawable_bitmap_internal_s>( dbmp.data );

        DrawIconEx( bd.m_mem_dc, 0, 0, icn, sz.x, sz.y, 0, nullptr, DI_NORMAL );
        DrawIconEx( bd.m_mem_dc, 0, sz.y, icn, sz.x, sz.y, 0, nullptr, DI_NORMAL );

        dbmp.fill_alpha( 255 );
        //dbmp.save_as_png(L"bmp.png");

        cache.set_size( sizeof( data_s ) + ( 4 * sz.x * sz.y ) );
        d = (data_s *)cache.data();
        d->iconcached = ci.hCursor;
        d->sz = sz;

        TSCOLOR *dst = (TSCOLOR *)( d + 1 );
        TSCOLOR *src1 = (TSCOLOR *)dbmp.body();
        TSCOLOR *src2 = (TSCOLOR *)( dbmp.body() + dbmp.info().pitch * sz.y );
        int addsrc = ( dbmp.info().pitch - sz.x * 4 ) / 4;

        for ( int y = 0; y < sz.y; ++y, src1 += addsrc, src2 += addsrc )
        {
            for ( int x = 0; x < sz.x; ++x, ++dst, ++src1, ++src2 )
            {
                auto detectcolor = []( TSCOLOR c1, TSCOLOR c2 )->TSCOLOR
                {
                    if ( c1 == c2 ) return c1;
                    if ( c1 == 0xFF000000u && c2 == 0xFFFFFFFFu ) return 0;
                    if ( c2 == 0xFF000000u && c1 == 0xFFFFFFFFu ) return 0x00FFFFFFu; // inversion!

                                                                                      // detect premultiplied color

                    int a0 = 255 - RED( c2 ) + RED( c1 );
                    int a1 = 255 - GREEN( c2 ) + GREEN( c1 );
                    int a2 = 255 - BLUE( c2 ) + BLUE( c1 );
                    int a = tmax( a0, a1, a2 );

                    return ( c1 & 0x00FFFFFFu ) | ( CLAMP<uint8>( a ) << 24 );
                };

                *dst = detectcolor( *src1, *src2 );
            }
        }
        return true;
    };

    ts::ivec2 drawpos;
    if ( GetCursorInfo( &ci ) && ci.flags == CURSOR_SHOWING )
    {
        if ( d == nullptr || d->iconcached != ci.hCursor )
        {
            HICON hicon = CopyIcon( ci.hCursor );
            ICONINFO ii;
            if ( GetIconInfo( hicon, &ii ) )
            {
                if ( !preparedata( hicon, ii.hbmMask, ii.hbmColor ) )
                {
                    if ( nullptr == d )
                    {
                        if ( cache.size() < sizeof( data_s ) )
                            cache.set_size( sizeof( data_s ) );
                        d = (data_s *)cache.data();
                        d->iconcached = ci.hCursor;
                    }
                    d->sz = ivec2( 0 ); // no render
                }
                d->hotspot.x = ii.xHotspot;
                d->hotspot.y = ii.yHotspot;

                DeleteObject( ii.hbmColor );
                DeleteObject( ii.hbmMask );
            }
            DestroyIcon( hicon );
        }

        drawpos = ts::ivec2( ci.ptScreenPos.x - d->hotspot.x - pos.x, ci.ptScreenPos.y - d->hotspot.y - pos.y );
    }
    else
    {
        cache.clear();
        return;
    }

    if ( d == nullptr || 0 == d->sz.x )
        return;

    int yy = d->sz.y;
    if ( drawpos.y + yy < 0 || drawpos.y >= info().sz.y ) return; // full over top / full below bottom

    int xx = d->sz.x;
    if ( drawpos.x + xx < 0 || drawpos.x >= info().sz.x ) return;

    byte *my_body = body( drawpos );
    const byte *src_color = (const byte *)( d + 1 );

    if ( drawpos.y < 0 )
    {
        my_body -= info().pitch * drawpos.y;
        src_color -= d->sz.x * 4 * drawpos.y;
        yy += drawpos.y;
        drawpos.y = 0;
    }
    if ( ( info().sz.y - drawpos.y ) < yy ) yy = ( info().sz.y - drawpos.y );

    if ( drawpos.x < 0 )
    {
        my_body -= info().bytepp() * drawpos.x;
        src_color -= 4 * drawpos.x;
        xx += drawpos.x;
        drawpos.x = 0;
    }
    if ( ( info().sz.x - drawpos.x ) < xx ) xx = ( info().sz.x - drawpos.x );

    for ( int y = 0; y < yy; ++y, my_body += info().pitch, src_color += d->sz.x * 4 )
    {
        TSCOLOR *rslt = (TSCOLOR *)my_body;
        const TSCOLOR *src = (TSCOLOR *)src_color;

        for ( int x = 0; x < xx; ++x, ++rslt, ++src )
        {
            TSCOLOR c = *src;
            if ( ALPHA( c ) )
                *rslt = ALPHABLEND_PM( *rslt, c );
            else if ( 0x00FFFFFFu == c )
                *rslt = ts::ARGB<auint>( 255 - RED( *rslt ), 255 - GREEN( *rslt ), 255 - BLUE( *rslt ), ALPHA( *rslt ) );
        }
    }
}

namespace
{
    struct drawable_bitmap_internal_s
    {
        HBITMAP m_mem_bitmap = nullptr;
        HDC     m_mem_dc = nullptr;
    };
}

//HBITMAP bitmap()    const { return m_mem_bitmap; }
//HDC     DC()	    const { return m_mem_dc; }

#ifdef _DEBUG
int dbmpcnt = 0;
#endif // _DEBUG

HDC get_dbmp_dc( drawable_bitmap_c&dbmp )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( dbmp.data );
    return d.m_mem_dc;
}

void drawable_bitmap_c::clear()
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    if ( d.m_mem_dc )
    {
#ifdef _DEBUG
        --dbmpcnt;
        DMSG( "dbmp " << dbmpcnt );
#endif // _DEBUG
        DeleteDC( d.m_mem_dc );
        d.m_mem_dc = nullptr;
    }
    if ( d.m_mem_bitmap )
    {
        DeleteObject( d.m_mem_bitmap );
        d.m_mem_bitmap = nullptr;
    }
    core.m_body = nullptr;
    memset( &core.m_info, 0, sizeof( imgdesc_s ) );
}

void    drawable_bitmap_c::create( const ivec2 &sz, int monitor )
{
    clear();

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

#ifdef _DEBUG
    ++dbmpcnt;
    DMSG( "dbmp " << dbmpcnt );
    //if (dbmpcnt > 300)
    //    __debugbreak();
#endif // _DEBUG


    core.m_info.sz = sz;
    core.m_info.bitpp = 32;

    DEVMODEW devmode;
    devmode.dmSize = sizeof( DEVMODE );

    if ( monitor < 0 )
        EnumDisplaySettingsW( nullptr, ENUM_CURRENT_SETTINGS, &devmode );
    else
    {
        struct mdata
        {
            DEVMODEW *devm;
            int mi;
            static BOOL CALLBACK calcmc( HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData )
            {
                mdata *m = (mdata *)dwData;
                if ( m->mi == 0 )
                {
                    MONITORINFOEXW minf;
                    minf.cbSize = sizeof( MONITORINFOEXW );
                    GetMonitorInfo( hMonitor, &minf );
                    EnumDisplaySettingsW( minf.szDevice, ENUM_CURRENT_SETTINGS, m->devm );
                    return FALSE;
                }
                --m->mi;
                return TRUE;
            }
        } mm; mm.mi = monitor; mm.devm = &devmode;

        EnumDisplayMonitors( nullptr, nullptr, mdata::calcmc, (LPARAM)&mm );

    }

    devmode.dmBitsPerPel = 32;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW( L"DISPLAY", nullptr, nullptr, &devmode );

    d.m_mem_dc = CreateCompatibleDC( tdc );
    if ( d.m_mem_dc == 0 )
        DEBUG_BREAK();;

    //m_memBitmap = CreateCompatibleBitmap(tdc, w, h);

    BITMAPV4HEADER bmi;

    int ll = sz.x * 4;
    core.m_info.pitch = ( ll + 3 ) & ( ~3 );

    ZeroMemory( &bmi, sizeof( bmi ) );
    bmi.bV4Size = sizeof( bmi );
    bmi.bV4Width = sz.x;
    bmi.bV4Height = -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = 32;
    bmi.bV4V4Compression = BI_RGB;
    bmi.bV4SizeImage = 0;

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif
        d.m_mem_bitmap = CreateDIBSection( tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0 );

    }
    ASSERT( d.m_mem_bitmap );
    CHECK( SelectObject( d.m_mem_dc, d.m_mem_bitmap ) );
    DeleteDC( tdc );
}

bool drawable_bitmap_c::create_from_bitmap( const bitmap_c &bmp, const ivec2 &p, const ivec2 &sz, bool flipy, bool premultiply, bool detect_alpha_pixels )
{
    clear();

    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

#ifdef _DEBUG
    ++dbmpcnt;
    DMSG( "dbmp " << dbmpcnt );
#endif // _DEBUG


    BITMAPV4HEADER bmi;

    ASSERT( ( p.x + sz.x ) <= bmp.info().sz.x && ( p.y + sz.y ) <= bmp.info().sz.y );

    ZeroMemory( &bmi, sizeof( bmi ) );
    bmi.bV4Size = sizeof( bmi );
    bmi.bV4Width = sz.x;
    bmi.bV4Height = flipy ? sz.y : -sz.y;
    bmi.bV4Planes = 1;
    bmi.bV4BitCount = bmp.info().bitpp;
    if ( bmp.info().bytepp() >= 3 )
    {
        bmi.bV4V4Compression = BI_RGB;
        bmi.bV4SizeImage = 0;
        bmi.bV4AlphaMask = 0xFF000000;
    }
    else
    {
        bmi.bV4V4Compression = BI_BITFIELDS;
        bmi.bV4RedMask = 0x0000f800;
        bmi.bV4GreenMask = 0x000007e0;
        bmi.bV4BlueMask = 0x0000001f;
        bmi.bV4AlphaMask = 0;
        bmi.bV4SizeImage = (DWORD)(sz.x * sz.y * bmp.info().bytepp());
    }

    //HDC tdc=GetDC(0);

    DEVMODEW devmode;
    devmode.dmSize = sizeof( DEVMODE );

    EnumDisplaySettingsW( nullptr, ENUM_CURRENT_SETTINGS, &devmode );

    devmode.dmBitsPerPel = bmp.info().bitpp;
    devmode.dmPelsWidth = sz.x;
    devmode.dmPelsHeight = sz.y;

    HDC tdc = CreateDCW( L"DISPLAY", nullptr, nullptr, &devmode );

    {
#ifndef _FINAL
        disable_fp_exceptions_c __x;
#endif

        d.m_mem_bitmap = CreateDIBSection( tdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&core.m_body, 0, 0 );
    }

    bool alphablend = false;

    if ( d.m_mem_bitmap == nullptr ) DEBUG_BREAK();;

    core.m_info.sz = sz;
    core.m_info.bitpp = bmp.info().bitpp;

    aint ll = sz.x * bmp.info().bytepp();
    core.m_info.pitch = ( ll + 3 ) & ( ~3 );
    uint8 * bdes = core(); //+lls*uint32(sz.y-1);
    const uint8 * bsou = bmp.body() + p.x * bmp.info().bytepp() + bmp.info().pitch * p.y;

    if ( premultiply && bmp.info().bytepp() == 4 )
    {
        for ( int y = 0; y < sz.y; ++y )
        {
            for ( int x = 0; x < sz.x; ++x )
            {
                uint32 ocolor;
                uint32 color = ( (uint32 *)bsou )[ x ];
                uint8 alpha = ALPHA( color );
                if ( alpha == 255 )
                {
                    ocolor = color;
                }
                else if ( alpha == 0 )
                {
                    ocolor = 0;
                    alphablend = true;
                }
                else
                {

                    ocolor = multbl[ alpha ][ color & 0xff ] |
                        ( (uint)multbl[ alpha ][ ( color >> 8 ) & 0xff ] << 8 ) |
                        ( (uint)multbl[ alpha ][ ( color >> 16 ) & 0xff ] << 16 ) |
                        ( color & 0xff000000 );

                    alphablend = true;

                }

                ( (uint32 *)bdes )[ x ] = ocolor;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;

        }

    }
    else if ( detect_alpha_pixels && bmp.info().bytepp() == 4 )
    {
        for ( int y = 0; y < sz.y; ++y )
        {
            if ( alphablend )
            {
                if ( info().pitch == bmp.info().pitch && ll == info().pitch )
                {
                    memcpy( bdes, bsou, ll * ( sz.y - y ) );
                }
                else
                {
                    for ( ; y < sz.y; ++y )
                    {
                        memcpy( bdes, bsou, ll );
                        bsou = bsou + bmp.info().pitch;
                        bdes = bdes + info().pitch;
                    }
                }

                break;
            }

            for ( aint i = 0; i < ll; i += 4 )
            {
                uint32 color = *(uint32 *)( bsou + i );
                *(uint32 *)( bdes + i ) = color;
                uint8 alpha = uint8( color >> 24 );
                if ( alpha < 255 )
                    alphablend = true;
            }

            bsou = bsou + bmp.info().pitch;
            bdes = bdes + info().pitch;
        }

    }
    else
    {
        if ( info().pitch == bmp.info().pitch && ll == info().pitch )
        {
            memcpy( bdes, bsou, ll * sz.y );
        }
        else
        {
            img_helper_copy( bdes, bsou, info(), bmp.info() );
        }
    }

    d.m_mem_dc = CreateCompatibleDC( tdc );
    if ( d.m_mem_dc == nullptr ) DEBUG_BREAK();;
    if ( SelectObject( d.m_mem_dc, d.m_mem_bitmap ) == nullptr ) DEBUG_BREAK();;

    DeleteDC( tdc );

    return alphablend;
}

void    drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, const ivec2 & pos_from_dc )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    if ( !d.m_mem_dc || !d.m_mem_bitmap ) return;

    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[ 256 ];
    } b;

    memset( &b, 0, sizeof( BITMAPV4HEADER ) );
    b.bmi.bmiHeader.bV4Size = sizeof( BITMAPINFOHEADER );
    if ( GetDIBits( d.m_mem_dc, d.m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS ) == 0 ) return;

    if ( b.bmi.bmiHeader.bV4BitCount == 32 && core() )
    {
        const uint8 * src = core() + info().pitch* ( b.bmi.bmiHeader.bV4Height - 1 - pos_from_dc.y );
        src += pos_from_dc.x * 4;
        //bmp->create_RGBA(b.bmi.bmiHeader.bV4Width,b.bmi.bmiHeader.bV4Height);
        uint8 * dst = bmp.body();
        int hh = tmin( bmp.info().sz.y, b.bmi.bmiHeader.bV4Height - pos_from_dc.y );
        auint ll = sizeof( uint32 ) * tmin( bmp.info().sz.x, b.bmi.bmiHeader.bV4Width - pos_from_dc.x );
        for ( int y = 0; y < hh; y++ )
        {
            //memcpy(dst,src,ll);

            for ( uint i = 0; i < ll; i += 4 )
            {
                uint32 s = *(uint32 *)( src + i );
                s |= 0xFF000000;
                *(uint32 *)( dst + i ) = s;
            }


            src -= info().pitch;
            dst += bmp.info().pitch;
        }
    }

}

void drawable_bitmap_c::save_to_bitmap( bitmap_c &bmp, bool save16as32 )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    if ( !d.m_mem_dc || !d.m_mem_bitmap ) return;


    struct {
        struct {
            BITMAPV4HEADER bmiHeader;
        } bmi;
        uint32 pal[ 256 ];
    } b;

    memset( &b, 0, sizeof( BITMAPV4HEADER ) );
    b.bmi.bmiHeader.bV4Size = sizeof( BITMAPINFOHEADER );
    if ( GetDIBits( d.m_mem_dc, d.m_mem_bitmap, 0, 0, nullptr, (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS ) == 0 ) return;

    if ( b.bmi.bmiHeader.bV4BitCount == 32 && core() )
    {
        uint32 ll = uint32( b.bmi.bmiHeader.bV4Width * 4 );
        const uint8 * src = core() + info().pitch*uint32( b.bmi.bmiHeader.bV4Height - 1 );
        bmp.create_ARGB( ref_cast<ivec2>( b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height ) );
        uint8 * dst = bmp.body();
        for ( int y = 0; y < bmp.info().sz.y; y++ )
        {
            memcpy( dst, src, ll );
            src -= info().pitch;
            dst += bmp.info().pitch;
        }
        return;
    }



    if ( b.bmi.bmiHeader.bV4BitCount != 16 && b.bmi.bmiHeader.bV4BitCount != 24 && b.bmi.bmiHeader.bV4BitCount != 32 ) return;

    if ( save16as32 && b.bmi.bmiHeader.bV4BitCount == 16 )
    {
        b.bmi.bmiHeader.bV4BitCount = 32;
    }

    if ( b.bmi.bmiHeader.bV4BitCount == 16 ) bmp.create_16( ref_cast<ivec2>( b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height ) );
    else if ( b.bmi.bmiHeader.bV4BitCount == 24 ) bmp.create_RGB( ref_cast<ivec2>( b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height ) );
    else if ( b.bmi.bmiHeader.bV4BitCount == 32 ) bmp.create_ARGB( ref_cast<ivec2>( b.bmi.bmiHeader.bV4Width, b.bmi.bmiHeader.bV4Height ) );
    bmp.fill( 0 );

    if ( GetDIBits( d.m_mem_dc, d.m_mem_bitmap, 0, b.bmi.bmiHeader.bV4Height, bmp.body(), (LPBITMAPINFO)&b.bmi, DIB_RGB_COLORS ) == 0 ) return;

    bmp.fill_alpha( 255 );

}



drawable_bitmap_c::drawable_bitmap_c( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );
    
    d.m_mem_bitmap = obd.m_mem_bitmap;
    d.m_mem_dc = obd.m_mem_dc;
    core.m_body = ob.core.m_body;
    core.m_info = ob.core.m_info;
    memset( &ob, 0, sizeof( drawable_bitmap_c ) );
}

drawable_bitmap_c &drawable_bitmap_c::operator=( drawable_bitmap_c && ob )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );
    drawable_bitmap_internal_s &obd = ref_cast<drawable_bitmap_internal_s>( ob.data );

    SWAP( d.m_mem_bitmap, obd.m_mem_bitmap );
    SWAP( d.m_mem_dc, obd.m_mem_dc );
    SWAP( core.m_body, ob.core.m_body );

    SWAP( core.m_info.sz, ob.core.m_info.sz );
    SWAP( core.m_info.pitch, ob.core.m_info.pitch );
    SWAP( core.m_info.bitpp, ob.core.m_info.bitpp );
    return *this;
}

void drawable_bitmap_c::grab_screen( const ts::irect &screenr, const ts::ivec2& p_in_bitmap )
{
    drawable_bitmap_internal_s &d = ref_cast<drawable_bitmap_internal_s>( data );

    HWND hwnd = GetDesktopWindow();
    HDC desktop_dc = GetDC( hwnd );
    BitBlt( d.m_mem_dc, p_in_bitmap.x, p_in_bitmap.y, screenr.width(), screenr.height(), desktop_dc, screenr.lt.x, screenr.lt.y, SRCCOPY | CAPTUREBLT );
    ReleaseDC( hwnd, desktop_dc );
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

