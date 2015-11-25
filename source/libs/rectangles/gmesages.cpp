#include "rectangles.h"

namespace
{
struct evlst_s
{
    gm_receiver_c *_gmer_first;
    gm_receiver_c *_gmer_last;
    gm_receiver_c *_curnext;
};

struct evt_internals_s
{
#ifdef _DEBUG
    int evs_count = 0;
#endif // _DEBUG
    evlst_s *evs = nullptr;

    ~evt_internals_s()
    {
#ifdef _DEBUG
        evs_count = -1;
#endif // _DEBUG
        if (evs)
        {
            MM_FREE(evs);
            evs = nullptr;
        }
    }
    void prepare(int evcnt)
    {
        ASSERT(evs_count == 0);
        int sz = evcnt * sizeof(evlst_s);
        evs = (evlst_s *)MM_ALLOC(sz);
        memset(evs, 0, sz);
#ifdef _DEBUG
        evs_count = evcnt;
#endif // _DEBUG
    }
    evlst_s & operator()(int ev)
    {
        ASSERT(ev < evs_count, "please, use GM_PREPARE(evcnt)");
        return evs[ev];
    }
};
ts::static_setup<evt_internals_s, -2> internals;
}

gm_receiver_c::gm_receiver_c(int ev)
{
    evlst_s &l = internals()(ev);
    LIST_ADD(this, l._gmer_first, l._gmer_last, _gmer_prev, _gmer_next);
}

void gm_receiver_c::unsubscribe(int ev)
{
    evlst_s &l = internals()(ev);
    if (l._curnext == this) l._curnext = _gmer_next;
    LIST_DEL(this, l._gmer_first, l._gmer_last, _gmer_prev, _gmer_next);
}

void gm_receiver_c::prepare( int evcnt )
{
    internals().prepare(evcnt);
}

DWORD gm_receiver_c::notify_receivers(int ev, gmsgbase &par)
{
    evlst_s &l = internals()(ev);

    if (l._curnext)
        return GMRBIT_FAIL; // recursive ev call! NOT supported

    for(;ASSERT(par.pass < 100, "100 iterations! vow vow");)
    {
        DWORD bits = 0;
        gm_receiver_c *f = l._gmer_first;
        for (; f;)
        {
            l._curnext = f->_gmer_next;
            bits |= f->event_happens(par);
            if (FLAG(bits, GMRBIT_ABORT)) 
            {
                l._curnext = nullptr;
                return bits;
            }
            f = l._curnext;
        }
        if (!FLAG(bits, GMRBIT_CALLAGAIN))
        {
            ASSERT( nullptr == l._curnext );
            return bits;
        }
        ++par.pass;
    }

    return GMRBIT_FAIL;

}


ts::flags32_s gmsgbase::send()
{
    ts::flags32_s r = gm_receiver_c::notify_receivers(m, *this);
    ASSERT(!r.is(GMRBIT_FAIL));
    return r;
}

void gmsgbase::send_to_main_thread()
{
    if (gui)
        gui->enqueue_gmsg( this );
    else
        TSDEL( this );
}