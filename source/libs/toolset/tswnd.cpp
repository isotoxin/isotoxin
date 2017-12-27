#include "toolset.h"
#include "internal/platform.h"

namespace ts
{

wnd_c::~wnd_c() 
{
    cbs->evt_destroy();
    
    if ( this == master().activewindow )
        master().activewindow = nullptr;

    if ( this == master().mainwindow )
        master().mainwindow = nullptr;

}

void wnd_c::show( wnd_show_params_s *shp )
{
    vshow( shp );

    if ( master().mainwindow == nullptr && shp && shp->mainwindow )
        master().mainwindow = this;

    if ( shp && shp->taskbar )
        master().activewindow = this;

    flags.init( F_LAYERED, shp && shp->layered );
}

#ifdef _WIN32
#include "_win32/win32_wnd.inl"

wnd_c *wnd_c::build( wnd_callbacks_s *cbs )
{
    return TSNEW( win32_wnd_c, cbs );
}

#endif

#ifdef _NIX
#include "_nix/nix_wnd.inl"

wnd_c *wnd_c::build(wnd_callbacks_s *cbs)
{
    return TSNEW(x11_wnd_c, cbs);
}

#endif


} // namespace ts
