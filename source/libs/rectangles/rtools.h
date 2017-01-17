#pragma once

#include "toolset/toolset.h"

#define FREE_BIT_START_CHECK(freebit, bit) TS_STATIC_CHECK( (unsigned long)(freebit << ((bit)-1)) < (unsigned long)(freebit << (bit)), "NO_MORE_BITS_IN_FLAGS" )

class rectengine_c;
struct evt_data_s;
struct theme_rect_s;
struct bcreate_s;

//-V:RID:813

typedef const void * GUIPARAM;

INLINE int as_int(GUIPARAM p)
{
    return (int)(size_t)p; //-V205 //-V221
}

namespace rtoolsinternals
{
    template< typename T > struct asparam
    {
        static GUIPARAM cvt( T v )
        {
            return (GUIPARAM)(size_t)v;
        }
    };
}

template<typename T> GUIPARAM as_param( T v ) { return rtoolsinternals::asparam<T>::cvt( v ); }

struct menu_anchor_s
{
    ts::irect rect;

    enum relpos_e
    {
        RELPOS_TYPE_RD, // at rite of rect, menu down
        RELPOS_TYPE_BD, // at bottom of rect, menu down
        RELPOS_TYPE_TU, // at top of rect, menu up
        RELPOS_TYPE_SYS, // same as RELPOS_TYPE_RD, useful for sysmenu

        relpos_check,
    } relpos = RELPOS_TYPE_RD;

    explicit menu_anchor_s(bool setup_by_mousepos = false, relpos_e rp = RELPOS_TYPE_RD);
    menu_anchor_s(const ts::irect &r, relpos_e rp = RELPOS_TYPE_RD) : rect(r), relpos(rp) {}
    ts::ivec2 pos() const
    {
        switch (relpos) //-V719
        {
        case menu_anchor_s::RELPOS_TYPE_RD:
        case menu_anchor_s::RELPOS_TYPE_SYS:
            return rect.rt();
        case menu_anchor_s::RELPOS_TYPE_BD:
            return rect.lb();
        case menu_anchor_s::RELPOS_TYPE_TU:
            return rect.lt;
        }
        return rect.center();
    }
};

class RID
{
    enum rid_e
    {
        EMPTY,
        BAD = 0x7FFFFFFF
    };
    int id;
public:
    RID() :id(EMPTY) {}
    explicit RID(int index) : id(index + 1) {}
    int index() const { return id - 1; }
    explicit operator bool() const { return id != EMPTY; }

    bool operator==(RID r) const { return id == r.id; }
    bool operator!=(RID r) const { return id != r.id; }

    // check parent-child relations, not compare
    bool operator>(RID r) const; // true if [this] is direct parent of [or]
    bool operator>>(RID r) const; // true if [or] is child of [*] that is child of [this]
    bool operator>>=(RID r) const { return r == *this || (*this >> r); } // true if [or] is child of [*] that is child of [this]

    // compare for sort
    bool operator<(RID r) const;

    static RID from_param(GUIPARAM p)
    {
        RID r;
        r.id = as_int(p);
        return r;
    }
    GUIPARAM to_param() const
    {
        return as_param(id);
    }

    // calls
    menu_anchor_s  call_get_popup_menu_pos( menu_anchor_s::relpos_e rp = menu_anchor_s::RELPOS_TYPE_RD ) const;
    int         call_get_menu_level() const;
    menu_c      call_get_menu() const;
    bool        call_ctxmenu_present() const;
    bool        call_is_tooltip() const;
    void        call_restore_signal() const;
    void        call_setup_button(bcreate_s &bcr) const;
    void        call_open_group(RID itm) const;
    void        call_lbclick(const ts::ivec2 &relpos = ts::ivec2(0)) const;
    void        call_kill_child( const ts::str_c&param );
    void        call_enable(bool enableflg) const;
};

DECLARE_MOVABLE( RID, true )

INLINE unsigned calc_hash( RID r )
{
    return r.index();
}


typedef fastdelegate::FastDelegate<bool(RID, GUIPARAM)> GUIPARAMHANDLER;

INLINE GUIPARAM as_param(GUIPARAM v)
{
    return v;
}
INLINE GUIPARAM as_param(RID r)
{
    return r.to_param();
}

void fixrect(ts::irect &r, const ts::ivec2 &minsz, const ts::ivec2 &maxsz, ts::uint32 hitarea);

void calcsb( int &pos, int &size, int shift, int viewsize, int datasize );
int calcsbshift( int pos, int viewsize, int datasize );

struct sbhelper_s
{
    ts::irect sbrect = ts::irect(0);
    ts::smart_int datasize;
    ts::smart_int shift; // zero or negative

    void set_size( ts::aint _datasize, int viewsize)
    {
        datasize = _datasize;
        if (shift > 0 || viewsize >= datasize) shift = 0;
        else
        {
            ts::aint vshift_lim = viewsize - datasize;
            if (shift < vshift_lim) shift = (int)vshift_lim;
        }
    }

    bool scroll(int pos, int viewsize)
    {
        int new_shift = calcsbshift(pos, viewsize, datasize);
        if (new_shift != shift)
        {
            shift = new_shift;
            return true;
        }
        return false;
    }

    void draw( const theme_rect_s *thr, rectengine_c &engine, evt_data_s &dd, bool hl );

    bool at_end( int viewsize ) const
    {
        return (datasize + shift) <= viewsize;
    }
};


bool check_always_ok(const ts::wstr_c &, bool);
bool check_always_ok_except_empty(const ts::wstr_c &, bool);

void text_remove_tags(ts::str_c &text_utf8);
ts::str_c text_remove_cstm(const ts::str_c &text_utf8);

#define TOOLTIP( ttt ) (GET_TOOLTIP) ([]()->ts::wstr_c { return ts::wstr_c(ttt); } )

INLINE void make_color(ts::str_c &s, ts::TSCOLOR c)
{
    s.set_as_char('#');
    s.append_as_hex(ts::RED(c));
    s.append_as_hex(ts::GREEN(c));
    s.append_as_hex(ts::BLUE(c));
    if (ts::ALPHA(c) != 0xff)
        s.append_as_hex(ts::ALPHA(c));
}

INLINE ts::str_c make_color(ts::TSCOLOR c)
{
    ts::str_c s;
    make_color(s, c);
    return s;
}


struct process_animation_s
{
    ts::bitmap_c bmp;
    UNIQUE_PTR( rsvg_svg_c ) svg;
    float angle;
    float devia;
    ts::Time nexttime = ts::Time::current() + 50;

    process_animation_s();
    ~process_animation_s();
    void render();
    void restart();

    void operator=( process_animation_s&&opa )
    {
        bmp = std::move( opa.bmp );
        svg = std::move( opa.svg );
        angle = opa.angle;
        devia = opa.devia;
        nexttime = opa.nexttime;
    }

};

class rectengine_root_c;
class animation_c
{
    struct redraw_request_s : public ts::movable_flag<true>
    {
        ts::safe_ptr<rectengine_root_c> engine;
        ts::irect rr;
    };
    ts::array_inplace_t<redraw_request_s, 32> rr;

    animation_c *prev = nullptr;
    animation_c *next = nullptr;

    static animation_c *first;
    static animation_c *last;
    void redraw();
    animation_c * do_tick();
protected:
    virtual ~animation_c();
    virtual void just_registered() {}
public:
    virtual bool animation_tick() = 0;
    static void tick();

    void register_animation(rectengine_root_c *e, const ts::irect &ar);
    void unregister_animation();

    static uint allow_tick;
};
