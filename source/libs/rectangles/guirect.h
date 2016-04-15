#pragma once

#ifndef _INCLUDE_GUIRECT_H_
#define _INCLUDE_GUIRECT_H_

enum system_query_e
{
    SQ_NOP,

    SQ_DRAW, // [begin_draw] already called before this query and [end_draw] will be called after. (it's true only for rect sq_evt)

    // by mouse
    SQ_RESIZE_START,
    SQ_RESIZE_END,
    SQ_MOVE_START,
    SQ_MOVE_END,

    SQ_RECT_CHANGING,               // evt_data_s::rectchg .? (only engine)
    SQ_PARENT_RECT_CHANGING,        // evt_data_s::rectchg .?

    SQ_RECT_CHANGED,                // evt_data_s::changed .pos_changed .size_changed .rect
    SQ_PARENT_RECT_CHANGED,         // evt_data_s::changed .pos_changed .size_changed .rect

    SQ_CHILD_VISIBILITY_CHANGED,    // evt_data_s::rect .id .is_visible
    SQ_PARENT_VISIBILITY_CHANGED,   // evt_data_s::changed .is_visible

    SQ_ZINDEX_CHANGED,              // evt_data_s::changed .zindex
    SQ_THEMERECT_CHANGED,

    SQ_FOCUS_CHANGED,               // evt_data_s::changed .focus

    SQ_MOUSE_MOVE,
    SQ_MOUSE_MOVE_OP,

    SQ_MOUSE_IN,
    SQ_MOUSE_OUT,

    SQ_MOUSE_LDOWN,
    SQ_MOUSE_LUP,
    SQ_MOUSE_MDOWN,
    SQ_MOUSE_MUP,
    SQ_MOUSE_RDOWN,
    SQ_MOUSE_RUP,
    SQ_MOUSE_WHEELDOWN,
    SQ_MOUSE_WHEELUP,
    SQ_MOUSE_L2CLICK,

    SQ_KEYDOWN,
    SQ_KEYUP,
    SQ_CHAR,

    SQ_CUSTOM_INIT_DONE,    // optional event

    SQ_GROUP_PROPORTIONS_CAHNGED, // by xgroup to self - use subscribe
    SQ_RESTORE_SIGNAL, // by extern code
    SQ_CHECK_HINTZONE,

    SQ_DETECT_AREA,
    SQ_GET_POPUP_MENU_POS,
    SQ_GET_MENU_LEVEL,
    SQ_POPUP_MENU_DIE,
    SQ_GET_MENU,
    SQ_IS_TOOLTIP,
    SQ_BUTTON_SETUP,
    SQ_OPEN_GROUP,
    SQ_KILL_CHILD,          // parent decides what the child must die in accordance with strparam
    SQ_YOU_WANNA_DIE,       // message to child in while processing SQ_KILL_CHILD
    SQ_CTL_ENABLE,
    SQ_GET_ROOT_PARENT,

    SQ_CHILD_CREATED,
    SQ_CHILD_DESTROYED,     // child array item now nullptr, but size not yet changed
    SQ_CHILD_ARRAY_COUNT,   // child array size changed -> evt_data_s::values.count

#ifdef _DEBUG
    SQ_TEST_00
#endif
};

enum caption_button_tag_e
{
    CBT_CLOSE,
    CBT_MAXIMIZE,
    CBT_NORMALIZE,
    CBT_MINIMIZE,

    CBT_APPCUSTOM, // application can define its own custom tags for caption buttons
};


enum ui_event_e
{
    UE_MAXIMIZED,
    UE_NORMALIZED,
    UE_MINIMIZED,
    UE_THEMECHANGED,
    UE_ACTIVATE,
};

template<> struct gmsg<GM_UI_EVENT> : public gmsgbase
{
    gmsg(ui_event_e evt) :gmsgbase(GM_UI_EVENT), evt(evt) {}
    ui_event_e evt;
};

namespace ts {
template<> INLINE RID &make_dummy<RID>(bool quiet) { static RID t; DUMMY_USED_WARNING(quiet); return t; }
}

template<typename STRTYPE> ts::streamstr<STRTYPE> & operator<<(ts::streamstr<STRTYPE> &dl, RID r);

NUMGEN_START(dthr, 0);

enum dthr_options_e
{
    DTHRO_BORDER        = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_BORDER_RECT   = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_CENTER        = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_CENTER_HOLE   = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_CENTER_ONLY   = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_BASE          = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_BASE_HOLE     = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_CAPTION       = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_CAPTION_TEXT  = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_VSB           = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_HSB           = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_SB_HL         = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_LEFT_CENTER   = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_BOTTOM        = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_RIGHT         = SETBIT(NUMGEN_NEXT(dthr)),
    DTHRO_LT_T_RT       = SETBIT(NUMGEN_NEXT(dthr)),    // draw only left-top, top, rite-top rects
    DTHRO_LB_B_RB       = SETBIT(NUMGEN_NEXT(dthr)),    // draw only left-bottom, bottom, rite-bottom rects
};

enum size_policy_e
{
    SP_NORMAL, // any size between min and max size, but can be shrink to fit parent limits
    SP_NORMAL_MAX, // any size between min and max size (pref to max), but can be shrink to fit parent limits.
    SP_NORMAL_MIN, // any size between min and max size (pref to min), but can be shrink to fit parent limits.
    SP_KEEP, // try to keep size while parent resize, else as SP_NORMAL
    SP_ANY, // ignore limits
};

/*
  guirect is base ui rectangle

*/

//-V:draw_thr:807

struct evt_data_s
{
	DUMMY( evt_data_s );

    struct draw_s
    {
        int drawtag;
        bool fake_draw = false;
        draw_s() {}
        draw_s(int drawtag):drawtag(drawtag) {}
    };


	evt_data_s() {}
    evt_data_s(const draw_s &d) { draw = d; }

	union
	{
		struct
		{
			ts::make_pod<ts::ivec2> screenpos;
            bool allowchangecursor;
		} mouse;

		struct
		{
			ts::make_pod<ts::irect> rect;
			ts::uint32 area;
            bool apply;
		} rectchg;

        struct
        {
            ts::make_pod<ts::irect> rect;
            bool focus;
            bool pos_changed;
            bool size_changed;
            bool zindex;
            bool width_changed;
            bool manual;
        } changed;

        ts::make_pod<draw_s> draw;
        ts::make_pod< ts::array_wrapper_c<const float> > float_array;
        bcreate_s *btn;
        ts::make_pod<ts::asptr> strparam;
        ts::make_pod<ts::wsptr> wstrparam;

        struct
        {
            const menu_c *menu;
            ts::make_pod<GUIPARAMHANDLER> behav_handler;
        } textfield;

        struct
        {
            bool enableflag;
        } flags;

        struct
        {
            ts::make_pod<ts::wsptr> text;
            ts::make_pod<ts::asptr> param;
        } item;

        struct
        {
            ts::make_pod<RID> id;
            int index;
            bool is_visible;
        } rect;

        struct
        {
            ts::make_pod<ts::irect> rect;
            ts::make_pod<ts::irect> sbrect;
            int sbpos, sbsize;
        } draw_thr;

        struct
        {
            ts::make_pod<ts::ivec2> pos;
            ts::uint32 area;
        } detectarea;

        struct
        {
            ts::make_pod<ts::ivec2> pos;
            bool accepted;
        } hintzone;

        struct
        {
            menu_c *menu;
            ts::make_pod<menu_anchor_s> menupos;
            int level;
            bool handled;
        } getsome;

        struct
        {
            int count;
        } values;

        struct
        {
            int scan;
            int casw;
            ts::wchar charcode;
        } kbd;

	};
};

class guirect_c;
struct draw_data_s;

class text_rect_dynamic_c : public ts::text_rect_c
{
    friend class gui_c;
    DECLARE_EYELET(text_rect_dynamic_c);
    ts::Time last_use_time = ts::Time::past();
    ts::bitmap_c * curtexture = nullptr;
public:
    text_rect_dynamic_c();
    /*virtual*/ ~text_rect_dynamic_c();
    /*virtual*/ ts::bitmap_c &texture() override;
    /*virtual*/ void texture_no_need() override;
};


struct sqhandler_i : public ts::safe_object
{
    virtual bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) = 0;
    virtual bool i_leeched( guirect_c &to ) { return true; };
    virtual void i_unleeched() {};
    virtual ~sqhandler_i() {}
};

class rectengine_c;

class rectprops_c // pod
{
	ts::ivec2 m_pos; // left-top corner position in parent coordiantes
    ts::ivec2 m_screenpos; // left-top corner position in screen coordiantes
	ts::ivec2 m_size;
    ts::irect m_fsrect; // full screen rect (in maximized mode)
    float     m_zindex;
	ts::flags32_s m_flags;

    NUMGEN_START(propbits, 0);
#define DECLAREBIT( bn ) static const ts::flags32_s::BITS bn = SETBIT(NUMGEN_NEXT(propbits))

	DECLAREBIT( F_VISIBLE );
	DECLAREBIT( F_ALPHABPLEND );
    DECLAREBIT( F_HIGHLIGHT );
    DECLAREBIT( F_ACTIVE );
    DECLAREBIT( F_MAXIMIZED ); // only for root rect
    DECLAREBIT( F_MINIMIZED ); // only for root rect
    DECLAREBIT( F_MICROMIZED ); // only for root rect

    DECLAREBIT( F_ALLOW_RESIZE ); // allow resize by mouse
    DECLAREBIT( F_ALLOW_MOVE ); // allow move by mouse

    DECLAREBIT( F_OUT_OF_BOUND ); // set by gui system

#undef DECLAREBIT

    rectprops_c(const rectprops_c&) UNUSED;
    rectprops_c operator=(const rectprops_c &) UNUSED;

    void updatefs()
    {
        m_fsrect = ts::wnd_get_max_size( screenrect(false) );
    }

public:

	rectprops_c()
	{
		memset( this, 0, sizeof(rectprops_c) );
	}

	bool is_visible() const {return m_flags.is(F_VISIBLE); }
    bool is_highlighted() const {return m_flags.is(F_HIGHLIGHT); }
    bool is_active() const {return m_flags.is(F_ACTIVE); }
    
	bool is_alphablend() const {return m_flags.is(F_ALPHABPLEND); }
    bool is_maximized() const {return m_flags.is(F_MAXIMIZED); }
    bool is_minimized() const {return m_flags.is(F_MINIMIZED); }
    bool is_micromized() const {return m_flags.is(F_MICROMIZED); }
    bool is_collapsed() const {return m_flags.is(F_MINIMIZED|F_MICROMIZED); }

    bool allow_move() const {return m_flags.is(F_ALLOW_MOVE); }
    bool allow_resize() const {return m_flags.is(F_ALLOW_RESIZE); }

    bool out_of_bound() const {return m_flags.is(F_OUT_OF_BOUND); }

    float zindex() const {return m_zindex;}
	const ts::ivec2 &pos() const {return m_pos;}
	const ts::ivec2 &size() const {return m_size;}
    const ts::ivec2 currentsize() const {return is_maximized() ? m_fsrect.size() : m_size;}
	ts::irect rect() const {return ts::irect(m_pos, m_pos + m_size);}
    ts::irect screenrect(bool maxmask = true) const { return (is_maximized()&&maxmask) ? m_fsrect : ts::irect(m_screenpos, m_screenpos + m_size); }
	ts::irect szrect() const { return ts::irect(ts::ivec2(0), m_size); } // just size
    ts::irect currentszrect() const { return ts::irect(ts::ivec2(0), currentsize() ); } // just size
    const ts::ivec2 &screenpos() const {return is_maximized() ? m_fsrect.lt : m_screenpos;}

    rectprops_c &zindex(float zi) { m_zindex = zi; return *this; }
    rectprops_c &setminsize( RID r );
    rectprops_c &sizew(int w) { m_size.x = w; updatefs(); ASSERT(m_size << 40000); return *this; }
    rectprops_c &sizeh(int h) { m_size.y = h; updatefs(); ASSERT(m_size << 40000); return *this; }
	rectprops_c &size(int w, int h) { m_size.x = w; m_size.y = h; updatefs(); ASSERT(m_size << 40000); return *this; }
	rectprops_c &size(const ts::ivec2 &sz) { m_size = sz; updatefs(); ASSERT(m_size << 40000); return *this; }
    rectprops_c &setcenterpos() { return pos(ts::wnd_get_center_pos(size())); }
	rectprops_c &pos(int x, int y) { ts::ivec2 delta( x - m_pos.x, y - m_pos.y ); m_pos.x = x; m_pos.y = y; m_screenpos += delta; updatefs(); return *this; }
	rectprops_c &pos(const ts::ivec2 &p) { return pos(p.x, p.y); }
	rectprops_c &show() { m_flags.set(F_VISIBLE); return *this; }
	rectprops_c &hide() { m_flags.clear(F_VISIBLE); return *this; }
	rectprops_c &visible(bool f) { m_flags.init(F_VISIBLE, f); return *this; }

    rectprops_c &highlight(bool h = true) { m_flags.init(F_HIGHLIGHT, h); return *this; }
    rectprops_c &active(bool a = true) { m_flags.init(F_ACTIVE, a); return *this; }
	rectprops_c &makealphablend() { m_flags.set(F_ALPHABPLEND); return *this; }
    rectprops_c &maximize(bool m)
    {
        m_flags.init(F_MAXIMIZED, m);
        updatefs();
        return *this;
    }
    rectprops_c &maximize( const ts::irect& maxrect )
    {
        m_flags.set( F_MAXIMIZED );
        m_fsrect = ts::wnd_get_max_size( maxrect );
        return *this;
    }

    rectprops_c &decollapse() { m_flags.clear(F_MINIMIZED|F_MICROMIZED); m_flags.set(F_VISIBLE); return *this; }
    rectprops_c &minimize(bool m) { m_flags.init(F_MINIMIZED, m); return *this; }
    rectprops_c &micromize(bool m) { m_flags.init(F_MICROMIZED, m); return *this; }
	
    rectprops_c &allow_move_resize(bool _move = true, bool _resize = true) { m_flags.init(F_ALLOW_MOVE, _move); m_flags.init(F_ALLOW_RESIZE, _resize); return *this; }

    bool change_to(const rectprops_c &p, rectengine_c *engine);
    void change_to(const rectprops_c &p) { memcpy( this, &p, sizeof(rectprops_c) ); /* rectprops_c is pod by design */ }

    // special function
    void __spec_movescreenpos(const ts::ivec2 &delta) { m_screenpos += delta; }
    void __spec_set_zindex(float zindex) {m_zindex = zindex;}
    void __spec_set_outofbound(bool f) {m_flags.init(F_OUT_OF_BOUND,f);}
};

class HOLD
{
    guirect_c *client;
public:
    HOLD(RID id);
    HOLD(guirect_c &r);

    template<typename T> const T & as() const { return SAFE_REF(dynamic_cast<const T *>(client)); }
    template<typename T> T & as() { return SAFE_REF(dynamic_cast<T *>(client)); }
    const guirect_c & operator()() const { return SAFE_REF(client); }
    guirect_c & operator()() { return SAFE_REF(client); }
    rectengine_c &engine();
    explicit operator bool() const { return client != nullptr; }
};

class MODIFY : public rectprops_c
{
	guirect_c *client;
public:
	MODIFY(RID id);
	MODIFY(guirect_c &r);
	~MODIFY();

    template<typename T> T & as() { return SAFE_REF(dynamic_cast<T *>(client)); }
};

class autoparam_i : public sqhandler_i
{
    friend class guirect_c;
protected:
    guirect_c *owner = nullptr;
    autoparam_i() {}
    /*virtual*/ bool i_leeched( guirect_c &to ) override { owner = &to; return true; };
    /*virtual*/ void i_unleeched() override { TSDEL(this); };
public:
    virtual ~autoparam_i();
};

namespace newrectkitchen
{

template<typename R> struct rectwrapper
{
    typedef R type;
};
template<class R> struct rectwrapper< rectwrapper<R> > { typedef R type; };
}

#define _PROOT(classname) MAKE_ROOT< newrectkitchen::rectwrapper<classname> >
#define _PCHILD(classname) MAKE_CHILD< newrectkitchen::rectwrapper<classname> >

struct initial_rect_data_s
{
    rectengine_c *engine;
    RID id;
    RID parent;
    RID after;
};
struct drawcollector;
// template 1 // see description
template<typename R> struct MAKE_ROOT : public initial_rect_data_s
{
    R *me = nullptr;
    MAKE_ROOT();
    operator RID() const { return id; }
    operator R&() { ASSERT(me); return *me; }
    void operator=(const MAKE_ROOT&) UNUSED;
    MAKE_ROOT(const MAKE_ROOT&) UNUSED;
};

// template 2 // see description
template<typename R> struct MAKE_ROOT< newrectkitchen::rectwrapper<R> > : public initial_rect_data_s
{
    R *me = nullptr;
    MAKE_ROOT() {}
    operator RID() const { return id; }
    operator R&() { ASSERT(me); return *me; }
    operator ts::safe_ptr<R>() { ASSERT(me); return ts::safe_ptr<R>(me); }
    void init(bool sys);
    void operator=(const MAKE_ROOT&) UNUSED;
    MAKE_ROOT(const MAKE_ROOT&) UNUSED;
};

// template 3 // see description
template<typename R> struct MAKE_CHILD : public initial_rect_data_s
{
    R *me = nullptr;
    MAKE_CHILD( RID parent );
    operator RID() const { return id; }
    operator R&() { ASSERT(me); return *me; }
    operator ts::safe_ptr<R>() { ASSERT(me); return ts::safe_ptr<R>(me); }
};

// template 4 // see description
template<typename R> struct MAKE_CHILD< newrectkitchen::rectwrapper<R> > : public initial_rect_data_s
{
    R *me = nullptr;
    operator RID() { init(); return id; }
    operator R&() { init(); ASSERT(me); return *me; }
    R& get() { init(); ASSERT(me); return *me; }
    operator ts::safe_ptr<R>() { init(); ASSERT(me); return ts::safe_ptr<R>(me); }
    void init();
    ~MAKE_CHILD()
    {
        init();
    }
};

template<typename R> struct MAKE_VISIBLE_CHILD : public initial_rect_data_s
{
    R *me = nullptr;
    bool visible;
    MAKE_VISIBLE_CHILD(RID parent, bool visible = true);
    ~MAKE_VISIBLE_CHILD()
    {
        if (visible && ASSERT(me))
            MODIFY(*me).visible(true);
    }
    operator RID() const { return id; }
    operator R&() { ASSERT(me); return *me; }
    operator ts::safe_ptr<R>() { ASSERT(me); return ts::safe_ptr<R>(me); }
};


/*
  sorry, my dear english-speaking friends, but my english is too bad to write this description in english
  in russian:
  вобщем, тут конечно дикий изврат с шаблонами. Попробую объяснить
  MAKE_ROOT и MAKE_CHILD по сути ничем не отличаются, кроме того, какой движок прямоугольнику они создают.
  template 1 и template 3 эквивалентны и используются для создания прямоугольника без параметров, точнее с дефолтовым параметром в виде структуры initial_rect_data_s
  веселье начинается, когда нужно передать дополнительные параметры в конструктор прямоугольника. Вот тут начинают работать template 2 и/или template 4
  по сути эти шаблоны - это предки для специализированных структур с дополнительными параметрами
  как можно заметить при внимательном рассмотрении, эти шаблоны по сути специализация MAKE_ROOT и MAKE_CHILD, но специальным враппером: newrectkitchen::rectwrapper
  такой изврат позволяет вынести дефолтовую инициализацию движка в отдельную функцию init и вызывать ее из специализированного потомка
  на самом деле ничего сложного. Это ж не буст.
*/

typedef fastdelegate::FastDelegate<ts::wstr_c ()> GET_TOOLTIP;
class rectengine_root_c;
class guirect_c : public sqhandler_i
{
    friend class MODIFY;

	DUMMY( guirect_c );
	rectprops_c m_props; // never! never change this struct directly! its private

    rectengine_root_c *__spec_find_root() const
    {
        if (m_parent == RID()) return ts::ptr_cast<rectengine_root_c *>(m_engine.get());
        return HOLD(m_parent)().__spec_find_root();
    }
    int m_inmod = 0;
    int __spec_inmod(int v) {m_inmod += v; return m_inmod;}


#ifndef _FINAL
    bool m_test_00 = false; // проверка на прохождение SQ_TEST_00 по базовым классам guirect_c
public:
    bool m_test_01 = false; // проверка вызова created из потомков
    void prepare_test_00();
#endif

protected:
	UNIQUE_PTR(rectengine_c) m_engine;
    ts::array_safe_t<sqhandler_i,1> m_leeches;
    RID m_rid;
    RID m_parent;
    ts::safe_ptr<rectengine_root_c> m_root;
    GET_TOOLTIP m_tooltip;
    guirect_c() {}

public:

	typedef ts::safe_ptr<guirect_c> sptr_t;

	guirect_c(initial_rect_data_s &data);
	virtual ~guirect_c();

    virtual void update_offset(int offset) {} // called from gui_vscrollgroup_c; offset - value of scrollbar shift, required to make rect top

    virtual void update_dndobj(guirect_c *donor) {}
    virtual guirect_c * summon_dndobj(const ts::ivec2 &deltapos) { return nullptr; };

    const ts::wstr_c tooltip() const {return m_tooltip ? m_tooltip() : ts::wstr_c();}
    void tooltip(GET_TOOLTIP gtt)  {m_tooltip = gtt;}

    ts::ivec2 to_screen(const ts::ivec2 &p) const // convers p to screen space
    {
        return getprops().screenpos() + p;
    }
    ts::ivec2 to_local(const ts::ivec2 &screenpos) const // convers screenpos to local space
    {
        return screenpos - getprops().screenpos();
    }
    ts::irect to_screen(const ts::irect &r) const // convers r to screen space
    {
        return r + getprops().screenpos();
    }
    ts::irect to_local(const ts::irect &screenrect) const // convers screenpos to local space
    {
        return screenrect - getprops().screenpos();
    }
    ts::irect local_to_root(const ts::irect &localr) const;
    ts::ivec2 local_to_root(const ts::ivec2 &localpt) const;
    ts::ivec2 root_to_local(const ts::ivec2 &rootpt) const;

    virtual void created(); // fully created - notify parent

    void leech( sqhandler_i * h );
    void unleech( sqhandler_i * h );

    bool inmod() const {return m_inmod > 0;} // modification in progress

    RID getparent() const {return m_parent;}
    RID getrid() const {return m_rid;}
    rectengine_root_c *getroot() const;
    RID getrootrid() const;
    bool is_root() const {return m_parent == RID();}

    rectengine_c &getengine() {return SAFE_REF(m_engine);}
    const rectengine_c &getengine() const {return SAFE_REF(m_engine);}

	const rectprops_c &getprops() const {return m_props;}
	//rectprops_c &getprops() {return props;} // NEVER UNCOMMENT THIS METHOD! ONLY CONST ACCESS TO props MUST BE PROVIDED

    virtual const theme_rect_s *themerect() const { return nullptr; };
    virtual ts::ivec2 get_min_size() const;
    virtual ts::ivec2 get_max_size() const;
    virtual int       get_height_by_width(int width) const { return 0; /* 0 means not calculated */ } // calculate rect height by given rect width
    virtual size_policy_e size_policy() const {return SP_NORMAL;}
    virtual bool accept_focus() const {return true;} // default. tooltip should not steal
    virtual ts::uint32 caption_buttons() const { return SETBIT(CBT_CLOSE) | SETBIT(CBT_MINIMIZE);}

    virtual ts::wstr_c get_name() const {return ts::wstr_c(); }

    virtual ts::irect get_client_area() const
    {
        const theme_rect_s *tr = themerect();
        if (tr) return tr->clientrect( getprops().currentsize(), getprops().is_maximized() );
        return getprops().szrect();
    }
    ts::TSCOLOR get_default_text_color(int index = -1) const
    {
        const theme_rect_s *tr = themerect();
        if (tr) return index < 0 ? tr->deftextcolor : tr->color(index);
        return ts::ARGB(0,0,0,255);
    }
    void calc_min_max_by_client_area( ts::ivec2 &minsz, ts::ivec2 &maxsz ) const
    {
        const theme_rect_s *tr = themerect();
        if (tr)
        {
            bool is_max = getprops().is_maximized();
            minsz = tr->size_by_clientsize(minsz, is_max);
            maxsz = tr->size_by_clientsize(maxsz, is_max);
        }
        minsz = tmax(minsz, get_min_size());
        maxsz = tmin(maxsz, get_max_size());
    }

	virtual void apply(const rectprops_c &pss);

    // sqhandler_i
	/*virtual*/ bool sq_evt( system_query_e qp, RID _rid, evt_data_s &data ) override;




    // special function. only internal mechanics should use them

    ts::ivec2 __spec_to_screen_calc(const ts::ivec2 &p) const // convers p to screen space (slow)
    {
        ts::ivec2 r = getprops().pos() + p;
        return (m_parent == RID()) ? r : HOLD(m_parent)().__spec_to_screen_calc(r);
    }
    void __spec_apply_screen_pos_delta(const ts::ivec2 &delta) { m_props.__spec_movescreenpos(delta); }
    void __spec_remove_engine() { UNIQUE_PTR(rectengine_c) t; memcpy(&m_engine, &t, sizeof(m_engine)); make_all_ponters_expired(); } //-V614 // hack! hack! hack!
    void __spec_set_zindex(float zindex) {m_props.__spec_set_zindex(zindex);}
    void __spec_set_outofbound(bool f) { m_props.__spec_set_outofbound(f); }
};

template<> struct gmsg<GM_CHECK_ALLOW_CLICK> : public gmsgbase
{
    gmsg(RID onrid) :gmsgbase(GM_CHECK_ALLOW_CLICK), onrid(onrid) {}
    RID onrid;
    ts::safe_ptr<guirect_c> mnu;
};

INLINE rectengine_c &HOLD::engine() {return SAFE_REF(client).getengine(); }

// gui controls
class gui_popup_menu_c;

class gui_control_c : public guirect_c
{
    DUMMY(gui_control_c);
protected:
    cached_theme_rect_c thrcache;
    ts::uint32 get_state() const;

    static const ts::flags32_s::BITS F_DISABLED         = SETBIT(0);
    static const ts::flags32_s::BITS FLAGS_FREEBITSTART = SETBIT(1);

    ts::UPDATE_RECTANGLE updaterect;
    GUIPARAMHANDLER customdatakiller;
    GUIPARAM customdata = nullptr;
    ts::uint32 defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
    ts::flags32_s flags;
    gui_control_c() {}

    const ts::font_desc_c & safe_font( const ts::font_desc_c *f )
    {
        if (f == nullptr)
        {
            if (const theme_rect_s *thr = themerect())
                return *thr->deffont;
            else
                return ts::g_default_text_font;
        }
        return *f;
    }

public:
    gui_control_c(initial_rect_data_s &data):guirect_c(data) {}
    /*virtual*/ ~gui_control_c();

    ts::safe_ptr<gui_popup_menu_c> popupmenu;

    /*virtual*/ const theme_rect_s *themerect() const
    {
        return thrcache(get_state());
    }

    template<typename F> void iterate_children(F & f)
    {
        for (rectengine_c *e : getengine())
            if (e) if (!f(e->getrect())) break;
    }

    void set_updaterect( ts::UPDATE_RECTANGLE h ) { updaterect = h; }
    ts::UPDATE_RECTANGLE get_updaterect() { return updaterect; }

    // custom data
    void set_customdata(GUIPARAM _data, GUIPARAMHANDLER _datakiller = GUIPARAMHANDLER()) 
    {
        if (!customdatakiller.empty()) customdatakiller(getrid(), customdata);
        customdata = _data; customdatakiller = _datakiller;
    }
    GUIPARAM get_customdata() const {return customdata;}
    GUIPARAMHANDLER get_customdata_killer() const {return customdatakiller;}

    template<typename T> T& get_customdata_obj()
    {
        T * t = (T *)customdata;
        return *t;
    }
    template<typename T> const T& get_customdata_obj() const
    {
        const T * t = (const T *)customdata;
        return *t;
    }
    template<typename T, class... _Valty> void set_customdata_obj(_Valty&&... _Val)
    {
        if (!customdatakiller.empty()) customdatakiller(getrid(), customdata);

        struct x
        {
            static bool killa(RID, GUIPARAM p)
            {
                T * t = (T *)p;
                TSDEL( t );
                return true;
            }
        };

        customdata = TSNEW(T, std::forward<_Valty>(_Val)...);
        customdatakiller = (GUIPARAMHANDLER)x::killa;
    }

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;

    const ts::str_c &themename() const {return thrcache.get_themerect();}
    virtual void set_theme_rect( const ts::asptr &thrn, bool ajust_defdraw );
    /*virtual*/ bool accept_focus() const override { return is_enabled(); }

    void enable(bool f = true) { disable(!f); }
    void disable(bool f = true);
    bool is_enabled() const {return !flags.is(F_DISABLED);};
    bool is_disabled() const {return flags.is(F_DISABLED);};

};

class gui_stub_c;
template<> struct MAKE_CHILD<gui_stub_c> : public _PCHILD(gui_stub_c)
{
    ts::ivec2 minsz;
    ts::ivec2 maxsz;
    MAKE_CHILD(RID parent_, ts::ivec2 minsz = ts::ivec2(-1), ts::ivec2 maxsz = ts::ivec2(-1)):minsz(minsz), maxsz(maxsz) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_panel_c;
template<> struct MAKE_CHILD<gui_panel_c> : public _PCHILD(gui_panel_c)
{
    ts::ivec2 minsz;
    ts::ivec2 maxsz;
    MAKE_CHILD(RID parent_, ts::ivec2 minsz = ts::ivec2(-1), ts::ivec2 maxsz = ts::ivec2(-1)) :minsz(minsz), maxsz(maxsz) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_stub_c : public gui_control_c
{
    DUMMY(gui_stub_c);

protected:
    ts::ivec2 minsz;
    ts::ivec2 maxsz;
    gui_stub_c() {}
public:
    gui_stub_c(MAKE_CHILD<gui_panel_c> &data) :gui_control_c(data), minsz(data.minsz), maxsz(data.maxsz) {}
    gui_stub_c(MAKE_CHILD<gui_stub_c> &data) :gui_control_c(data), minsz(data.minsz), maxsz(data.maxsz) {}
    /*virtual*/ ~gui_stub_c() {}

    /*virtual*/ ts::ivec2 get_min_size() const override
    {
        ts::ivec2 sz = __super::get_min_size();
        if (minsz.x >= 0) sz.x = minsz.x;
        if (minsz.y >= 0) sz.y = minsz.y;
        return sz;
    }
    /*virtual*/ ts::ivec2 get_max_size() const override
    {
        ts::ivec2 sz = __super::get_max_size();
        if (maxsz.x >= 0) sz.x = maxsz.x;
        if (maxsz.y >= 0) sz.y = maxsz.y;
        return sz;
    }

    void set_min_size(const ts::ivec2&sz) { minsz = sz; }
    void set_max_size(const ts::ivec2&sz) { maxsz = sz; }


    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &d) override
    {
        if (qp == SQ_DRAW) return false; // stub control - nothing to draw
        return __super::sq_evt(qp,rid,d);
    }
};


class gui_panel_c : public gui_stub_c
{
    DUMMY(gui_panel_c);
public:
    gui_panel_c() {}
    gui_panel_c(MAKE_CHILD<gui_panel_c> &data) :gui_stub_c(data) {}
    /*virtual*/ ~gui_panel_c() {}
    GUIPARAMHANDLER drawhandler = nullptr;

    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &d) override
    {
        if (qp == SQ_DRAW && drawhandler) return drawhandler(rid,&getengine()); // stub control - nothing to draw
        return __super::sq_evt(qp, rid, d);
    }

};

typedef fastdelegate::FastDelegate< button_desc_s *() > GET_BUTTON_FACE;
#define BUTTON_FACE( face ) (GET_BUTTON_FACE) ([]()->button_desc_s * { return gui->theme().get_button(CONSTASTR(#face)); } )

class gui_button_c : public gui_control_c
{
    DUMMY(gui_button_c);

    GM_RECEIVER(gui_button_c, GM_UI_EVENT);

    static const ts::flags32_s::BITS F_PRESS                = FLAGS_FREEBITSTART << 0;
    static const ts::flags32_s::BITS F_MARK                 = FLAGS_FREEBITSTART << 1;
    static const ts::flags32_s::BITS F_RADIOBUTTON          = FLAGS_FREEBITSTART << 2;
    static const ts::flags32_s::BITS F_CHECKBUTTON          = FLAGS_FREEBITSTART << 3;
    static const ts::flags32_s::BITS F_TEXTSIZEACTUAL       = FLAGS_FREEBITSTART << 4;
    static const ts::flags32_s::BITS F_LIMIT_MAX_SIZE       = FLAGS_FREEBITSTART << 5;
    static const ts::flags32_s::BITS F_CONSTANT_SIZE_X      = FLAGS_FREEBITSTART << 6;
    static const ts::flags32_s::BITS F_CONSTANT_SIZE_Y      = FLAGS_FREEBITSTART << 7;
    static const ts::flags32_s::BITS F_DISABLED_USE_ALPHA   = FLAGS_FREEBITSTART << 8;
    static const ts::flags32_s::BITS F_HOVER                = FLAGS_FREEBITSTART << 9;

    typedef gm_redirect_s<GM_GROUP_SIGNAL> GROUPHANDLER;
    UNIQUE_PTR( GROUPHANDLER ) grouphandler;
    int grouptag = -1;
    const ts::font_desc_c *font = &ts::g_default_text_font;
    ts::str_c image;
    ts::wstr_c text;
    ts::shared_ptr<button_desc_s> desc;
    button_desc_s::states curstate = button_desc_s::NORMAL;
    ts::ivec2 textsize = ts::ivec2(0);

    GUIPARAMHANDLER handler;
    GUIPARAM param = nullptr;

    GET_BUTTON_FACE face_getter;

    bool group_handler(gmsg<GM_GROUP_SIGNAL> & signal);
    bool default_handler(RID r, GUIPARAM param);
    void draw();
    void update_textsize();
    int get_ctl_width();

    void set_face(button_desc_s *bdesc);

    void set_text_internal(const ts::wsptr&t)
    {
        text.clear();
        image.clear();

        if (ts::pwstr_c(t).begins(CONSTWSTR("<img=")))
        {
            ts::aint x = ts::pwstr_c(t).find_pos(5,'>');
            if (ASSERT(x>5))
            {
                text.set( t.skip(x+1) );
                image = ts::to_str( ts::pwstr_c(t).substr(5,x) );
            }
        } else
            text.set(t);

        flags.clear(F_TEXTSIZEACTUAL);
    }

public:
    gui_button_c(initial_rect_data_s &data):gui_control_c(data) { handler = DELEGATE(this, default_handler); }
    /*virtual*/ ~gui_button_c() {}

    void set_check_value(bool v);

    void mark() {flags.set(F_MARK);}
    void set_radio( int radiotag )
    {
        grouptag = radiotag;
        flags.set(F_RADIOBUTTON);
        flags.clear(F_CONSTANT_SIZE_X|F_CONSTANT_SIZE_Y);
        auto h = DELEGATE(this, group_handler);
        grouphandler.reset( TSNEW( GROUPHANDLER, h ) );
    }
    void set_check(int checktag)
    {
        grouptag = checktag;
        flags.set(F_CHECKBUTTON);
        flags.clear(F_CONSTANT_SIZE_X|F_CONSTANT_SIZE_Y);
        auto h = DELEGATE(this, group_handler);
        grouphandler.reset(TSNEW(GROUPHANDLER, h));
    }

    const ts::font_desc_c &get_font() const;

    void set_face_getter( GET_BUTTON_FACE _face_getter ) { face_getter = _face_getter; if (face_getter) set_face( face_getter() ); }; // be careful!!! face_getter will be called every time theme changed! make sure object life time is enough

    void set_handler( GUIPARAMHANDLER _handler, GUIPARAM _param ) { handler = _handler; param = _param; }
    void set_text( const ts::wsptr&t ) { set_text_internal(t); }
    void set_text( const ts::wsptr&t, int &minw ) { set_text_internal(t); minw = get_ctl_width(); }

    void set_limit_max_size( bool f = true ) { flags.init(F_LIMIT_MAX_SIZE,f); }
    void set_constant_size( const ts::ivec2 &sz ) { ASSERT(!flags.is(F_CHECKBUTTON|F_RADIOBUTTON)); flags.set(F_CONSTANT_SIZE_X|F_CONSTANT_SIZE_Y); textsize = sz; }
    void set_constant_width( int w ) { ASSERT(!flags.is(F_CHECKBUTTON|F_RADIOBUTTON)); flags.set(F_CONSTANT_SIZE_X); textsize.x = w; }
    void set_constant_height( int h ) { ASSERT(!flags.is(F_CHECKBUTTON|F_RADIOBUTTON)); flags.set(F_CONSTANT_SIZE_Y); textsize.y = h; }

    void push(); // do all stuff like button was pushed

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;
};

struct text_draw_params_s;
class gui_label_c : public gui_control_c
{
    DUMMY(gui_label_c);

    void draw();

protected:

    static const ts::flags32_s::BITS FLAGS_AUTO_HEIGHT          = FLAGS_FREEBITSTART << 0;
    static const ts::flags32_s::BITS FLAGS_SELECTION            = FLAGS_FREEBITSTART << 1;
    static const ts::flags32_s::BITS FLAGS_SELECTABLE           = FLAGS_FREEBITSTART << 2;
    static const ts::flags32_s::BITS FLAGS_VCENTER              = FLAGS_FREEBITSTART << 3;
    static const ts::flags32_s::BITS FLAGS_FREEBITSTART_LABEL   = FLAGS_FREEBITSTART << 4;

    text_rect_dynamic_c textrect;

    void draw( draw_data_s &dd, const text_draw_params_s &tdp ); // use it if want selection

    virtual bool custom_tag_parser(ts::wstr_c& r, const ts::wsptr &tv) const { return false; }
    bool _custom_tag_parser_delegate(ts::wstr_c& r, const ts::wsptr &tv) const { return custom_tag_parser(r,tv); }
    ts::CUSTOM_TAG_PARSER custom_tag_parser_delegate() const { return DELEGATE(this, _custom_tag_parser_delegate); }

    gui_label_c() {}
public:
    gui_label_c(initial_rect_data_s &data) :gui_control_c(data) {}
    /*virtual*/ ~gui_label_c() { }

    const ts::font_desc_c &get_font() const;
    virtual void set_text(const ts::wstr_c&text, bool full_height_last_line = false);
    void set_font(const ts::font_desc_c *font);
    void set_vcenter(bool f = true) { flags.init(FLAGS_VCENTER, f); }
    void set_autoheight(bool f = true) { flags.init(FLAGS_AUTO_HEIGHT, f); }
    void set_defcolor(ts::TSCOLOR col) { textrect.set_def_color(col); }
    void set_selectable( bool f = true );

    ts::GLYPHS &get_glyphs() { return textrect.glyphs(); }
    const ts::GLYPHS &get_glyphs() const { return textrect.glyphs(); }

    /*virtual*/ int       get_height_by_width(int width) const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    const ts::wstr_c &get_text() const {return textrect.get_text();};

    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

class gui_label_ex_c : public gui_label_c
{
    DUMMY(gui_label_ex_c);

    ts::ivec2 glyphs_shift() const;

protected:
    gui_label_ex_c() {}
    ts::ivec2 glyphs_pos = ts::ivec2(0);
    int overlink = -1;
    ts::ivec2 fake_margin = ts::ivec2(0);

    bool check_overlink(const ts::ivec2 &localpos);
    ts::str_c get_link_under_cursor(const ts::ivec2 &localpos) const;
    ts::ivec2 get_link_pos_under_cursor(const ts::ivec2 &localpos) const;
    /*virtual*/ bool custom_tag_parser(ts::wstr_c& r, const ts::wsptr &tv) const override;

    virtual void get_link_prolog(ts::wstr_c & r, int linknum) const;
    virtual void get_link_epilog(ts::wstr_c & r, int linknum) const;

public:

    gui_label_ex_c(initial_rect_data_s &data) :gui_label_c(data) {}
    /*virtual*/ ~gui_label_ex_c() {}

    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

typedef fastdelegate::FastDelegate<void (const ts::wstr_c &)> CLICK_LINK;

class gui_label_simplehtml_c : public gui_label_ex_c
{
    DUMMY(gui_label_simplehtml_c);
    ts::wstrings_c links;
    ts::wstr_c tt() const;

public:
    gui_label_simplehtml_c(initial_rect_data_s &data) :gui_label_ex_c(data) { m_tooltip = DELEGATE(this, tt); }
    /*virtual*/ ~gui_label_simplehtml_c() {}

    CLICK_LINK on_link_click;

    /*virtual*/ void created() override;
    /*virtual*/ void set_text(const ts::wstr_c&text, bool full_height_last_line) override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

class gui_tooltip_c;
template<> struct MAKE_ROOT<gui_tooltip_c> : public _PROOT(gui_tooltip_c)
{
    RID owner;
    MAKE_ROOT(RID owner):_PROOT(gui_tooltip_c)(), owner(owner) { init(false); }
};

class gui_tooltip_c : public gui_label_c
{
    DUMMY(gui_tooltip_c);

    GM_RECEIVER( gui_tooltip_c, GM_TOOLTIP_PRESENT );
    GM_RECEIVER( gui_tooltip_c, GM_KILL_TOOLTIPS );

    RID ownrect;

    bool check_text(RID r, GUIPARAM param);

    DEBUGCODE( static int ttcount; )

protected:
    gui_tooltip_c() {}
public:

    gui_tooltip_c(MAKE_ROOT<gui_tooltip_c> &data) :gui_label_c(data), ownrect(data.owner) { ASSERT(++ttcount == 1); }
    /*virtual*/ ~gui_tooltip_c();

    /*virtual*/ bool accept_focus() const override {return false;} // default. tooltip should not steal
    /*virtual*/ ts::ivec2 get_min_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    static void create(RID owner);
};

class gui_group_c : public gui_control_c // group
{
    DUMMY(gui_group_c);

protected:

    struct cri_s
    {
        ts::irect   area = ts::irect(0);
        int         from = 0;
        int         count = 0;
        int         areasize = 0;
        bool        update_offset = false;
    };

    virtual void children_repos_info( cri_s &info ) const;
    virtual void on_add_child(RID id) {} // new child was added and children array was increased
    virtual void on_die_child(int index) {} // child has died, but children array not yet changed
    virtual void on_change_children(int count) {} // children array has been shrink
    gui_group_c() {}
public:
    gui_group_c(initial_rect_data_s &data) :gui_control_c(data) {}
    ~gui_group_c();
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    virtual void children_repos();

};


class gui_hgroup_c : public gui_group_c // vertical or horizontal group
{
    DUMMY(gui_hgroup_c);
    ts::tbuf_t<float> proportions;

    static const ts::flags32_s::BITS F_ALLOW_MOVE_SPLITTER = FLAGS_FREEBITSTART << 0;
    
    void update_proportions();
protected:

    static const ts::flags32_s::BITS F_HGROUP_FREEBITSTART = FLAGS_FREEBITSTART << 1;

    struct rsize
    {
        ts::int16 szmin;
        ts::int16 szmax;
        ts::int16 sz;
        ts::uint8 szsplit;
        ts::uint8 sizepolicy;

        bool const_size() const {return szmin == szmax || sizepolicy == SP_NORMAL_MAX || sizepolicy == SP_NORMAL_MIN;}
        void no()
        {
            szmin = 0;
            szmax = 0;
            sz = -1;
            szsplit = 0;
            sizepolicy = SP_NORMAL;
        }
    };

    ts::tbuf_t<rsize> rsizes;
    virtual int __splitter_size() const
    {
        if (allow_move_splitter())
            if (const theme_rect_s *thr = themerect())
                return thr->sis[SI_CENTER].width();
        return 0;
    }
    virtual ts::uint32 __splitcursor() const;
    
    virtual int __vec_index() const {return 0;}
    virtual ts::asptr __themerect_name() const
    {
        return CONSTASTR("hgroup");
    }
    /*virtual*/ void children_repos_info(cri_s &info) const override;
    /*virtual*/ void children_repos() override;
    /*virtual*/ void on_add_child(RID id) override;
    /*virtual*/ void on_die_child(int index) override;
    /*virtual*/ void on_change_children(int count) override;
    gui_hgroup_c() {}
public:
    gui_hgroup_c(initial_rect_data_s &data) :gui_group_c(data) { defaultthrdraw = 0; }
    /*virtual*/ ~gui_hgroup_c();

    void set_proportions(const ts::str_c&values, int div);

    bool allow_move_splitter() const {return flags.is(F_ALLOW_MOVE_SPLITTER); }
    void allow_move_splitter( bool b );

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override
    {
        set_theme_rect(__themerect_name(), false);
        __super::created();
    }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

class gui_vgroup_c : public gui_hgroup_c
{
    DUMMY(gui_vgroup_c);
protected:

    /*virtual*/ int __splitter_size() const override
    {
        if (allow_move_splitter())
            if (const theme_rect_s *thr = themerect())
                return thr->sis[SI_CENTER].height();
        return 0;
    }
    /*virtual*/ ts::uint32 __splitcursor() const override;
    /*virtual*/ int __vec_index() const override
    {
        return 1;
    }
    virtual ts::asptr __themerect_name() const override
    {
        return CONSTASTR("vgroup");
    }

    /*virtual*/ void children_repos_info(cri_s &info) const override;

    gui_vgroup_c() {}
public:
    gui_vgroup_c(initial_rect_data_s &data) :gui_hgroup_c(data) {}
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
};

class gui_vscrollgroup_c : public gui_group_c // vertical group with vertical scrollbar
{
    DUMMY(gui_vscrollgroup_c);

    sbhelper_s sbhelper;
    ts::buf0_c drawflags;
    ts::safe_ptr<rectengine_c> scroll_target;
    ts::safe_ptr<rectengine_c> top_visible;
    int top_visible_offset = 0;

    static const ts::flags32_s::BITS F_SBVISIBLE = FLAGS_FREEBITSTART << 0;
    static const ts::flags32_s::BITS F_SBHL = FLAGS_FREEBITSTART << 1;
protected:
    static const ts::flags32_s::BITS F_SCROLL_TO_END = FLAGS_FREEBITSTART << 2;
    static const ts::flags32_s::BITS F_SCROLL_TO_MAX_TOP = FLAGS_FREEBITSTART << 3;
    static const ts::flags32_s::BITS F_SB_OVER_ITEMS = FLAGS_FREEBITSTART << 4;
    static const ts::flags32_s::BITS F_HCENTER_SMALL_CTLS = FLAGS_FREEBITSTART << 5;
    static const ts::flags32_s::BITS F_KEEP_TOP_VISIBLE = FLAGS_FREEBITSTART << 6;
    static const ts::flags32_s::BITS F_VSCROLLFREEBITSTART = FLAGS_FREEBITSTART << 7;

    virtual void on_manual_scroll() { scroll_target = nullptr; flags.clear(F_SCROLL_TO_END); flags.clear(F_KEEP_TOP_VISIBLE); }; // called on scroll initiated by user (mouse wheel or scrollbar)
    /*virtual*/ void children_repos() override;
    /*virtual*/ void on_add_child(RID id) override;
    gui_vscrollgroup_c() {}

    void nosb() {flags.clear(F_SBVISIBLE);};
    int sbshift() const {return sbhelper.shift;}
    void sbshift( int s ) {sbhelper.shift = s;} // please call gui->repos_children(this);

public:
    gui_vscrollgroup_c(initial_rect_data_s &data) :gui_group_c(data) {}
    /*virtual*/ ~gui_vscrollgroup_c() {}

    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void hcenter_small_ctls(bool f) { flags.init(F_HCENTER_SMALL_CTLS,f); }

    bool is_sb_visible() const { return flags.is(F_SBVISIBLE); };

    bool is_at_end() const
    {
        if (!flags.is(F_SBVISIBLE)) return true;
        return flags.is(F_SCROLL_TO_END);
    }

    void not_at_end()
    {
        flags.clear(F_SCROLL_TO_END);
    }

    void keep_top_visible(bool force = false)
    {
        if (force)
        {
            scroll_target = nullptr;
            flags.clear(F_SCROLL_TO_END);
        }
        if (!flags.is(F_SCROLL_TO_END) && scroll_target.expired() && !top_visible.expired())
            flags.set(F_KEEP_TOP_VISIBLE);
    }

    void scroll_to_begin();
    void scroll_to_end();
    void scroll_to_child( rectengine_c *reng, bool maxtop );
    rectengine_c *get_top_visible(int *offset)
    {
        if (offset) *offset = top_visible_offset;
        return top_visible;
    }
    
    int width_for_children() const
    {
        cri_s info;
        children_repos_info(info);
        if (!flags.is(F_SBVISIBLE)) return info.area.width();
        return info.area.width() - sbhelper.sbrect.width();
    }
};

class gui_popup_menu_c;
template<> struct MAKE_ROOT<gui_popup_menu_c> : public _PROOT(gui_popup_menu_c)
{
    menu_anchor_s screenpos;
    menu_c menu;
    RID parentmenu;
    bool sys;
    MAKE_ROOT(const menu_anchor_s& screenpos, const menu_c &menu, bool sys, RID parentmenu) : _PROOT(gui_popup_menu_c)(), screenpos(screenpos), menu(menu), parentmenu(parentmenu), sys(sys) { init(sys); }
    ~MAKE_ROOT();
};

class gui_popup_menu_c : public gui_vscrollgroup_c
{
    DUMMY(gui_popup_menu_c);

    GM_RECEIVER( gui_popup_menu_c, GM_KILLPOPUPMENU_LEVEL );
    GM_RECEIVER( gui_popup_menu_c, GM_POPUPMENU_DIED );
    GM_RECEIVER( gui_popup_menu_c, GM_TOOLTIP_PRESENT );
    GM_RECEIVER( gui_popup_menu_c, GM_SYSMENU_PRESENT );
    GM_RECEIVER( gui_popup_menu_c, GM_CHECK_ALLOW_CLICK );
    

    bool check_focus(RID r, GUIPARAM p);
    bool update_size(RID r, GUIPARAM p);

    GUIPARAMHANDLER closehandler; // only if no any action
    GUIPARAMHANDLER clickhandler;

    RID parent_menu;

    RID hostrid;
    menu_c menu;

    menu_anchor_s showpoint;

    static gui_popup_menu_c & create(const menu_anchor_s& screenpos, const menu_c &mnu, bool sys, RID parentmenu);

    static const ts::flags32_s::BITS F_SYSMENU = F_VSCROLLFREEBITSTART << 0;
    

public:

    struct click_data_s
    {
        ts::wstr_c itemtext;
        ts::str_c itemparam;
    };

    bool operator()(int, const ts::wsptr& txt);
    bool operator()(int, const ts::wsptr& txt, const menu_c &sm);
    bool operator()(int, const ts::wsptr& txt, ts::uint32 flags, MENUHANDLER handler, const ts::str_c& prm);

    gui_popup_menu_c(MAKE_ROOT<gui_popup_menu_c> &data) :gui_vscrollgroup_c(data), menu(data.menu), showpoint(data.screenpos), parent_menu(data.parentmenu) { defaultthrdraw = DTHRO_BORDER; flags.init(F_SYSMENU, data.sys); }
    /*virtual*/ ~gui_popup_menu_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    RID host() const {return hostrid;}
    gui_popup_menu_c &host(RID hostrid_) { hostrid = hostrid_; return *this; } // host can obtain focus without popmenu destruction

    GUIPARAMHANDLER get_close_handler() { return closehandler; }
    GUIPARAMHANDLER get_click_handler() { return clickhandler; }
    void set_close_handler( GUIPARAMHANDLER ch ) { closehandler = ch; }
    void set_click_handler( GUIPARAMHANDLER ch ) { clickhandler = ch; }

    void menu_item_click( const click_data_s &prm );

    static gui_popup_menu_c & show( const menu_anchor_s& screenpos, const menu_c &menu, bool sys = false, RID parentmenu = RID() );
};

class gui_menu_item_c;
template<> struct MAKE_CHILD<gui_menu_item_c> : public _PCHILD(gui_menu_item_c)
{
    ts::wstr_c text;
    ts::str_c  prm;
    MENUHANDLER handler;
    ts::uint32 flags = 0;
    MAKE_CHILD(RID parent_, const ts::wsptr &text):text(text) { parent = parent_; }
    ~MAKE_CHILD();
    MAKE_CHILD & operator<<(MENUHANDLER m) { handler = m; return *this;}
    MAKE_CHILD & operator<<(const ts::str_c &p) { prm = p; return *this;}
    MAKE_CHILD & operator<<(ts::uint32 f) { flags = f; return *this;}
};

class gui_menu_item_c : public gui_label_c
{
    DUMMY(gui_menu_item_c);

    ts::safe_ptr<gui_popup_menu_c> submnu_shown;
    menu_c submnu;
    MENUHANDLER handler;
    ts::str_c param;

    static const ts::flags32_s::BITS F_SEPARATOR = FLAGS_FREEBITSTART_LABEL << 0;
    static const ts::flags32_s::BITS F_MARKED = FLAGS_FREEBITSTART_LABEL << 1;

    bool open_submenu(RID r, GUIPARAM p);
protected:
    gui_menu_item_c() {}
public:
    gui_menu_item_c(MAKE_CHILD<gui_menu_item_c> &data);
    /*virtual*/ ~gui_menu_item_c();

    /*virtual*/ const theme_rect_s *themerect() const
    {
        ts::uint32 st = flags.is(F_SEPARATOR) ? 0 : get_state();
        if (submnu_shown) st |= RST_HIGHLIGHT;
        if (flags.is(F_DISABLED)) RESETFLAG( st, RST_HIGHLIGHT );
        return thrcache(st);
    }
    /*virtual*/ ts::ivec2 get_min_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    gui_menu_item_c & applymif(ts::uint32 miflags);
    gui_menu_item_c & separator(bool f);
    gui_menu_item_c & submenu(const menu_c &m);

    static gui_menu_item_c & create(RID owner, const ts::wsptr &text, ts::uint32 flags, MENUHANDLER handler, const ts::str_c &param);
};


#include "guieditrect.h"

class gui_textfield_c;
template<> struct MAKE_CHILD<gui_textfield_c> : public _PCHILD(gui_textfield_c)
{
    gui_textedit_c::TEXTCHECKFUNC checker;
    GUIPARAMHANDLER handler;
    GUIPARAM param;
    ts::wstr_c text;
    int chars_limit;
    GET_BUTTON_FACE selectorface;
    int multiline;
    bool selector;
    bool create_visible;
    MAKE_CHILD(RID parent_, const ts::wstr_c &text, int chars_limit = 0, int multiline = 0, bool selector = true, bool visible = true) :text(text), chars_limit(chars_limit), multiline(multiline), selector(selector), create_visible(visible) { parent = parent_; }
    ~MAKE_CHILD();

    MAKE_CHILD &operator << (GUIPARAMHANDLER h) { handler = h; return *this; }
    MAKE_CHILD &operator << (GUIPARAM prm) { param = prm; return *this; }
    MAKE_CHILD &operator << (gui_textedit_c::TEXTCHECKFUNC checker_) { checker = checker_; return *this; }
};

class gui_textfield_c : public gui_textedit_c
{
    DUMMY(gui_textfield_c);
    friend struct MAKE_CHILD<gui_textfield_c>;
    ts::safe_ptr<gui_button_c> selector;
    int height = 0;

protected:
public:

    struct behav_s
    {
        RID tfrid;
        menu_c menu;
        GUIPARAMHANDLER handler;
        bool set_value = true;

        behav_s(RID tfrid, menu_c menu, GUIPARAMHANDLER handler, bool set_value = true):tfrid(tfrid), menu(menu), handler(handler), set_value(set_value) {}
        bool onclick(RID, GUIPARAM);
    };

    gui_textfield_c(MAKE_CHILD<gui_textfield_c> &data);
    /*virtual*/ ~gui_textfield_c();

    bool push_selector(RID, GUIPARAM);

    void badvalue( bool b );

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;
    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};


//////////////

class gui_htabsel_c;
template<> struct MAKE_CHILD<gui_htabsel_c> : public _PCHILD(gui_htabsel_c)
{
    menu_c menu;
    MAKE_CHILD(RID parent_, const menu_c &menu) :menu(menu) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_htabsel_c : public gui_hgroup_c
{
    DUMMY(gui_htabsel_c);

    GM_RECEIVER( gui_htabsel_c, GM_HEARTBEAT );
    GM_RECEIVER( gui_htabsel_c, GM_UI_EVENT );

    menu_c menu;
    RID activeitem;
    int height = 32;

public:
    bool operator()(RID, const ts::wsptr& txt);
    bool operator()(RID, const ts::wsptr& txt, const menu_c &sm);
    bool operator()(RID, const ts::wsptr& txt, ts::uint32 flags, MENUHANDLER handler, const ts::str_c& prm);

    gui_htabsel_c(MAKE_CHILD<gui_htabsel_c> &data) :gui_hgroup_c(data) { menu = data.menu; defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER; }
    /*virtual*/ ~gui_htabsel_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

class gui_htabsel_item_c;
template<> struct MAKE_CHILD<gui_htabsel_item_c> : public _PCHILD(gui_htabsel_item_c)
{
    MENUHANDLER handler;
    ts::wstr_c txt;
    ts::str_c  param;
    MAKE_CHILD(RID parent_, const ts::wstr_c &txt): txt(txt) { parent = parent_; }
    ~MAKE_CHILD();

    MAKE_CHILD &operator << (MENUHANDLER h) { handler = h; return *this; }
    MAKE_CHILD &operator << (const ts::str_c &prm) { param = prm; return *this; }
    //MAKE_CHILD &operator << (RID prev) { after = prev; return *this; }
};

class gui_htabsel_item_c : public gui_label_c
{
    DUMMY(gui_htabsel_item_c);

    MENUHANDLER handler;
    ts::str_c param;

    int tw = 0;

protected:
    gui_htabsel_item_c() {}
public:
    gui_htabsel_item_c(MAKE_CHILD<gui_htabsel_item_c> &data) :gui_label_c(data), handler(data.handler), param(data.param)
    {
        set_text(data.txt);
        tw = textrect.calc_text_size(-1, nullptr).x;
    }
    /*virtual*/ ~gui_htabsel_item_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void activate();
};

//////////////

class gui_vtabsel_c;
template<> struct MAKE_CHILD<gui_vtabsel_c> : public _PCHILD(gui_vtabsel_c)
{
    menu_c menu;
    MAKE_CHILD(RID parent_, const menu_c &menu) :menu(menu) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_vtabsel_c : public gui_vscrollgroup_c
{
    DUMMY(gui_vtabsel_c);

    GM_RECEIVER(gui_vtabsel_c, GM_HEARTBEAT);
    GM_RECEIVER(gui_vtabsel_c, GM_UI_EVENT);

    menu_c menu;
    RID currentgroup;
    RID activeitem;

public:
    bool operator()(ts::pair_s<RID, int>&, const ts::wsptr& txt);
    bool operator()(ts::pair_s<RID, int>&, const ts::wsptr& txt, const menu_c &sm);
    bool operator()(ts::pair_s<RID, int>&, const ts::wsptr& txt, ts::uint32 flags, MENUHANDLER handler, const ts::str_c& prm);

    gui_vtabsel_c(MAKE_CHILD<gui_vtabsel_c> &data) :gui_vscrollgroup_c(data), menu(data.menu) { defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER; }
    /*virtual*/ ~gui_vtabsel_c();

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

class gui_vtabsel_item_c;
template<> struct MAKE_CHILD<gui_vtabsel_item_c> : public _PCHILD(gui_vtabsel_item_c)
{
    menu_c menu;
    MENUHANDLER handler;
    ts::wstr_c txt;
    ts::str_c  param;
    int lv;
    MAKE_CHILD(RID parent_, const ts::wstr_c &txt, int lv) : txt(txt), lv(lv) { parent = parent_; }
    ~MAKE_CHILD();

    MAKE_CHILD &operator << (MENUHANDLER h) { handler = h; return *this; }
    MAKE_CHILD &operator << (const ts::str_c &prm) { param = prm; return *this; }
    MAKE_CHILD &operator << (const menu_c&mnu) { menu = mnu; return *this; }
    MAKE_CHILD &operator << (RID prev) { after = prev; return *this; }
};

class gui_vtabsel_item_c : public gui_label_c
{
    DUMMY(gui_vtabsel_item_c);

    menu_c submnu;
    MENUHANDLER handler;
    ts::str_c param;
    int lv;

protected:
    gui_vtabsel_item_c() {}
public:
    gui_vtabsel_item_c(MAKE_CHILD<gui_vtabsel_item_c> &data) :gui_label_c(data), submnu(data.menu), handler(data.handler), param(data.param), lv(data.lv)
    {
        if (lv > 0)
            set_text(ts::tmp_wstr_c(lv + data.txt.get_length() + 7, true).set(CONSTWSTR("<l>")).append_chars(ts::tmin(3, lv), '.').append(data.txt).append(CONSTWSTR("</l>")));
        else
            set_text(data.txt);
    }
    /*virtual*/ ~gui_vtabsel_item_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};


//////////////

typedef fastdelegate::FastDelegate< menu_c(const ts::str_c &, bool) > GETMENU_FUNC;

class gui_hslider_c;
template<> struct MAKE_CHILD<gui_hslider_c> : public _PCHILD(gui_hslider_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();

    float val = 0;
    ts::wstr_c initstr;
    GUIPARAMHANDLER handler;
    GETMENU_FUNC gm;

    MAKE_CHILD &operator << (float val_) { val = val_; return *this; }
    MAKE_CHILD &operator << (const ts::wsptr &inits) { initstr = inits; return *this; }
    MAKE_CHILD &operator << (GUIPARAMHANDLER h) { handler = h; return *this; }
    MAKE_CHILD &operator << (GETMENU_FUNC _gm) { gm = _gm; return *this; }
};


class gui_hslider_c : public gui_control_c
{
    DUMMY(gui_hslider_c);

    float level = -1.0f;
    float pos = 0;
    ts::wstr_c current_value_text;
    ts::time_float_c values;

    static const ts::flags32_s::BITS F_LBDOWN = FLAGS_FREEBITSTART << 0;
    static const ts::flags32_s::BITS F_PBMODE = FLAGS_FREEBITSTART << 1;

    GUIPARAMHANDLER handler;
    GETMENU_FUNC gm;

    bool setval(RID, GUIPARAM);
    bool onclick(RID, GUIPARAM);
    void update_text_value();

    ts::svec2 levelshift = ts::svec2(0);

public:

    struct param_s
    {
        float pos;
        float value;

        ts::wstr_c custom_value_text;
    };

    gui_hslider_c(MAKE_CHILD<gui_hslider_c> &data);
    /*virtual*/ ~gui_hslider_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ ts::ivec2 get_max_size() const override;

    /*virtual*/ void created() override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void set_value(float value);
    void set_level(float level, const ts::wstr_c &txt);
    void set_level(float level);

    void set_pb_mode() { flags.set(F_PBMODE); }
};

#endif