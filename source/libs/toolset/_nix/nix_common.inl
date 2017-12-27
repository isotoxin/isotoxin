#pragma once

#include <X11/Xutil.h>
#include <X11/Xatom.h>

enum clientevents
{
    EVT_LOOPER_TICK,
};

class x11_wnd_c;
class wnd_c;
Window wnd2x11( const wnd_c * );
void x11expose(x11_wnd_c *, bool mapnotify);
void x11configure(x11_wnd_c *, const XConfigureEvent &ce);

struct draw_target_s
{
    Window x11w;
    explicit draw_target_s( Window wnd ) :x11w( wnd ) {}
};

struct master_internal_stuff_s
{
    ts::safe_ptr<wnd_c> grabbing;
    Display *X11;
    Window root;
    int screen = 0;
    int depth = 0;
    ts::Time lasttick = ts::Time::past();

    ts::hashmap_t<Window, x11_wnd_c *> x112wnd_map;

    Window move_x11w = 0;
    ts::irect move_rect = ts::irect(0);

    int sysmodal = 0;
    int cnttick = 0;
    bool looper = false;
    bool looper_staring = false;
    bool looper_allow_tick = true;
    bool folder_spy_evt = false;

    master_internal_stuff_s():X11(nullptr)
    {

    }

    ~master_internal_stuff_s()
    {
        if (X11)
            XCloseDisplay(X11);
    }

    x11_wnd_c * x112wnd( Window w )
    {
        if (auto *x = x112wnd_map.find(w))
            return x->value;
        return nullptr;
    }

    void add_window( Window w, x11_wnd_c *obj )
    {
        bool a = false;
        auto &itm = x112wnd_map.add_get_item(w,a);
        itm.value = obj;
    }

    void remove_window( x11_wnd_c *obj )
    {
        for(auto itr = x112wnd_map.begin(); itr; ++itr)
        {
            if (itr.value() == obj)
            {
                itr.remove();
                return;
            }
        }

    }


    int monitor_count()
    {
        return 1;
        //if (X11 == nullptr)
        //    X11 = XOpenDisplay(nullptr);

        //return X11 ? ScreenCount(X11) : 0;
    }

    void prepare()
    {
        if (X11 == nullptr)
        {
            X11 = XOpenDisplay(nullptr);
            if (X11 != nullptr)
            {
                screen = DefaultScreen(X11);
                depth = DefaultDepth(X11, screen);
                root = XDefaultRootWindow(X11);
            }
        }
    }

    ts::irect monitor_fs(int monitor)
    {
        monitor = 0; // ahh X11... ugly support of multimonitor

        prepare();

        if (X11 == nullptr)
            return ts::irect(0);

        int w = DisplayWidth(X11, screen);
        int h = DisplayHeight(X11, screen);

        return ts::irect(0,0,w,h);
    }

    ts::irect max_window_size(const ts::irect &rfrom)
    {
        ts::irect r = monitor_fs(0); //
        r.lt += ts::ivec2(100);
        r.rb -= ts::ivec2(100);
        return r;
        /*
        if (X11 == nullptr)
            X11 = XOpenDisplay(nullptr);

        Window rw = DefaultRootWindow(X11);
        XSizeHints sh;

        sh.width = sh.min_width = rfrom.width();
        sh.height = sh.min_height =rfrom.height();
        sh.x = rfrom.lt.x;
        sh.y = rfrom.lt.y;
        sh.flags = PSize | PMinSize | PPosition;

        long x;
        Atom a = XInternAtom( X11, "WM_SIZE_HINTS", True );
        XGetWMSizeHints(X11,rw,&sh,&x,a);

        return ts::irect(0);
        */
    }
    ts::irect max_window_size(const ts::ivec2 &pt)
    {
        return max_window_size(ts::irect(pt));
    }
};