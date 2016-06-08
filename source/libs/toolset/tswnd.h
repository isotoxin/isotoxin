#pragma once

namespace ts
{
enum notification_icon_action_e
{
    NIA_LCLICK,
    NIA_L2CLICK,
    NIA_RCLICK,
};

enum mouse_event_e
{
    MEVT_NOPE,

    MEVT_MOVE,
    MEVT_LDOWN,
    MEVT_LUP,
    MEVT_L2CLICK,
    MEVT_RDOWN,
    MEVT_RUP,
    MEVT_MDOWN,
    MEVT_MUP,
    MEVT_WHEELUP,
    MEVT_WHEELDN
};

enum disposition_e
{
    D_UNKNOWN,

    D_NORMAL,
    D_MAX,
    D_MIN,
    D_MICRO,

    D_RESTORE, // D_MAX of D_NORMAL, depend on last disposition
};

enum specborder_e
{
    SPB_TOP,
    SPB_RIGHT,
    SPB_BOTTOM,
    SPB_LEFT,
};

class wnd_c;
struct wnd_show_params_s
{
    wnd_c *parent = nullptr;
    wstr_c name;
    irect rect = irect(0);
    float opacity = 1.0f;
    disposition_e d = D_NORMAL;


    bool collapsed() const
    {
        return d == D_MIN || d == D_MICRO;
    }

    bool visible = false;
    bool focus = true;
    bool mainwindow = false;
    bool toolwindow = false;
    bool layered = false;
    bool acceptfiles = false;

    virtual void apply( bool update_frame, bool force_apply_now ) {};
};

struct specialborder_s
{
    bitmap_c img;
    irect s,r,e;
};

struct wnd_callbacks_s
{
    virtual bool evt_specialborder( specialborder_s bd[ 4 ] ) { return false; }

    virtual void evt_mouse( mouse_event_e me, const ivec2 &clpos /* relative to wnd */, const ivec2 &scrpos ) {}
    virtual void evt_mouse_out() {};
    virtual bool evt_mouse_activate() { return true; };

    virtual void evt_notification_icon( notification_icon_action_e a ) {}
    virtual void evt_draw() {}

    virtual void evt_refresh_pos( const irect &scr, disposition_e d ) {}
    virtual void evt_focus_changed( wnd_c *wnd ) {}

    virtual bool evt_on_file_drop( const wstr_c& fn, const ivec2 &clp ) { return false;  }

    virtual bitmap_c    app_get_icon( bool for_tray ) { return bitmap_c(); }
    virtual irect       app_get_redraw_rect() { return irect( maximum<int>::value, minimum<int>::value ); }

    virtual void evt_destroy() {}
};

class wnd_c : public safe_object
{
protected:

    wnd_callbacks_s *cbs = nullptr;
    flags32_s flags;
    static const flags32_s::BITS F_LAYERED = SETBIT( 0 );
    
    static const flags32_s::BITS FREEBITS = SETBIT( 1 );

    disposition_e dp = D_UNKNOWN;

    virtual void vshow( wnd_show_params_s *shp ) = 0;

public:

    typedef safe_ptr<wnd_c> sptr_t;

    wnd_c( wnd_callbacks_s *cbs ) :cbs( cbs ) { ASSERT(cbs); }
    virtual ~wnd_c();

    disposition_e disposition() const { return dp; }
    void show( wnd_show_params_s *shp );

    virtual bool is_collapsed() const = 0;
    virtual bool is_maximized() const = 0;
    virtual bool is_foreground() const = 0;
    virtual bool is_hover( const ivec2& screenpos ) const = 0;
    virtual void try_close() = 0;
    virtual void set_focus( bool bring_to_front ) = 0;
    virtual void flash() = 0;
    virtual void set_capture() = 0;

    virtual void make_hole( const ts::irect &holerect ) = 0;

    virtual bmpcore_exbody_s get_backbuffer() = 0;
    virtual void flush_draw( const irect &r ) = 0;

    virtual wstr_c get_load_filename_dialog( const wsptr &iroot, const wsptr &name, extensions_s &exts, const wchar *title ) { return wstr_c();  }
    virtual bool   get_load_filename_dialog( wstrings_c &files, const wsptr &iroot, const wsptr& name, ts::extensions_s & exts, const wchar *title ) { return false; }
    virtual wstr_c get_save_directory_dialog( const wsptr &root, const wsptr &title, const wsptr &selectpath = wsptr(), bool nonewfolder = false ) { return wstr_c(); }
    virtual wstr_c get_save_filename_dialog( const wsptr &iroot, const wsptr &name, extensions_s &exts, const wchar *title ) { return wstr_c(); }

    wnd_callbacks_s *get_callbacks() { return cbs; }

    static wnd_c *build( wnd_callbacks_s *cbs );
};


} // namespace ts
