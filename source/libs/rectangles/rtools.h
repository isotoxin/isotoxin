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
    return (int)p; //-V205
}

INLINE GUIPARAM as_param(int v)
{
    return (GUIPARAM)v; //-V204
}


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

    bool operator==(const RID&or) const { return id == or.id; }
    bool operator!=(const RID&or) const { return id != or.id; }

    // check parent-child relations, not compare
    bool operator>(const RID&or) const; // true if [this] is direct parent of [or]
    bool operator>>(const RID&or) const; // true if [or] is child of [*] that is child of [this]
    bool operator>>=(const RID&or) const { return or == *this || (*this >> or); } // true if [or] is child of [*] that is child of [this]

    // compare for sort
    bool operator<(const RID&or) const;

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
    bool        call_is_tooltip() const;
    void        call_restore_signal() const;
    void        call_setup_button(bcreate_s &bcr) const;
    void        call_open_group(RID itm) const;
    void        call_lbclick(const ts::ivec2 &relpos = ts::ivec2(0)) const;
    void        call_kill_child( const ts::str_c&param );
    void        call_enable(bool enableflg) const;
};

typedef fastdelegate::FastDelegate<bool(RID, GUIPARAM)> GUIPARAMHANDLER;

INLINE GUIPARAM as_param(GUIPARAM v)
{
    return v;
}
INLINE GUIPARAM as_param(RID r)
{
    return r.to_param();
}

template<typename STRTYPE> INLINE ts::streamstr<STRTYPE> & operator<<(ts::streamstr<STRTYPE> &dl, RID r)
{
    dl.begin();
    if (r)
    {
        dl.raw_append("RID=[");
        dl << ts::ref_cast<int>(r);
        ts::wstr_c n = HOLD(r)().get_name();
        if (!n.is_empty())
        {
            dl << ',';
            dl << n;
        }
        dl << ']';
    }
    else
    {
        dl.raw_append("RID=[]");
    }
    return dl;
}

void fixrect(ts::irect &r, const ts::ivec2 &minsz, const ts::ivec2 &maxsz, ts::uint32 hitarea);

void calcsb( int &pos, int &size, int shift, int viewsize, int datasize );
int calcsbshift( int pos, int viewsize, int datasize );

struct sbhelper_s
{
    ts::irect sbrect = ts::irect(0);
    int datasize = 0;
    int shift = 0; // zero or negative

    void set_size(int _datasize, int viewsize)
    {
        datasize = _datasize;
        if (shift > 0 || viewsize >= datasize) shift = 0;
        else
        {
            ts::aint vshift_lim = viewsize - datasize;
            if (shift < vshift_lim) shift = vshift_lim;
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


bool check_always_ok(const ts::wstr_c &);

void text_remove_tags(ts::str_c &text_utf8);
ts::str_c text_remove_cstm(const ts::str_c &text_utf8);

#define TOOLTIP( ttt ) (GET_TOOLTIP) ([]()->ts::wstr_c { return ttt; } )





struct process_animation_s
{
    int sz = 32;
    static const int N = 3;
    float angle;
    float devia;
    ts::Time nexttime = ts::Time::current() + 50;
    HPEN    pen, pen2, pen3;
    HBRUSH br, br2, br3;

    process_animation_s(int sz = 32);
    ~process_animation_s();
    void render(HDC dc, const ts::ivec2& shift);
    void restart();

};

struct process_animation_bitmap_s : public process_animation_s
{
    ts::drawable_bitmap_c bmpr;
    ts::bitmap_c bmp;
    process_animation_bitmap_s();
    void render();
};


