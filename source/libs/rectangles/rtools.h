#pragma once

#include "toolset/toolset.h"

#define FREE_BIT_START_CHECK(freebit, bit) TS_STATIC_CHECK( (unsigned long)(freebit << ((bit)-1)) < (unsigned long)(freebit << (bit)), "NO_MORE_BITS_IN_FLAGS" )

class rectengine_c;
struct evt_data_s;
struct theme_rect_s;
struct bcreate_s;

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

    static RID from_ptr(const void *ptr)
    {
        RID r;
        r.id = (int)ptr;
        return r;
    }
    void* to_ptr() const
    {
        return (void *)id;
    }

    // calls
    ts::ivec3   call_get_popup_menu_pos() const;
    int         call_get_menu_level() const;
    menu_c      call_get_menu() const;
    bool        call_is_tooltip() const;
    void        call_restore_signal() const;
    void        call_setup_button(bcreate_s &bcr) const;
    void        call_open_group(RID itm) const;
    void        call_lbclick(ts::ivec2 relpos = ts::ivec2(0)) const;
    void        call_kill_child( const ts::str_c&param );
    void        call_item_activated( const ts::wstr_c&text, const ts::str_c&param );
    void        call_children_repos();
    void        call_enable(bool enableflg) const;
};

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

typedef const void * GUIPARAM;
typedef fastdelegate::FastDelegate<bool (RID, GUIPARAM)> GUIPARAMHANDLER;

void fixrect(ts::irect &r, const ts::ivec2 &minsz, const ts::ivec2 &maxsz, ts::uint32 hitarea);

void calcsb( int &pos, int &size, int shift, int viewsize, int datasize );
int calcsbshift( int pos, int viewsize, int datasize );

struct sbhelper_s
{
    ts::irect sbrect = 0;
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

void text_remove_tags(ts::wstr_c &text);

#define TOOLTIP( ttt ) (GET_TOOLTIP) ([]()->ts::wstr_c { return ttt; } )
