#pragma once

#ifndef _INCLUDE_RECTENGINE_H_
#define _INCLUDE_RECTENGINE_H_

enum mousetrack_type_e
{
	MTT_RESIZE = SETBIT(0),
	MTT_MOVE = SETBIT(1),
    MTT_SBMOVE = SETBIT(2),
    MTT_TEXTSELECT = SETBIT(3),
    MTT_MOVESPLITTER = SETBIT(4),
    MTT_MOVECONTENT = SETBIT(5),
    MTT_SCALECONTENT = SETBIT(6),
    MTT_APPDEFINED1 = SETBIT(7),
    MTT_APPDEFINED2 = SETBIT(8),

	MTT_ANY = 0xFFFF,
};

struct mousetrack_data_s
{
	ts::irect rect;
	ts::uint32 area;
	ts::ivec2 mpos;
    RID rid;
	mousetrack_type_e mtt;

	mousetrack_data_s(mousetrack_type_e mtt, RID rid):mtt(mtt), rid(rid) {}
};

struct draw_data_s
{
    MOVABLE( true );
    DUMMY(draw_data_s);

    ts::safe_ptr<rectengine_c> engine;
    ts::ivec2 offset; // offset relative to root rect
    ts::ivec2 size; // size of current rect
    ts::irect cliprect; // in root coordinates
    int alpha;
    draw_data_s()
    { reset(); }
    void reset()
    {
        engine = nullptr;
        offset = ts::ivec2(0);
        size = ts::ivec2(0);
        cliprect = ts::irect(ts::ivec2(0), ts::ivec2(maximum<ts::int16>::value));
        alpha = 255;
    }
};

struct text_draw_params_s
{
    const ts::font_desc_c *font = nullptr;
    const ts::TSCOLOR *forecolor = nullptr;
    //ts::TSCOLOR *bkcolor = nullptr;
    const ts::flags32_s *textoptions = nullptr;
    ts::UPDATE_RECTANGLE rectupdate;
    ts::ivec2 *sz = nullptr;
    //ts::GLYPHS *glyphs = nullptr;
};

enum area_e
{
	AREA_LEFT = 1,
	AREA_RITE = 2,
	AREA_TOP = 4,
	AREA_BOTTOM = 8,
	AREA_CAPTION = 16,
    AREA_EDITTEXT = 32,
    AREA_HAND = 64,

    AREA_NORESIZE = 65536,
	AREA_RESIZE = AREA_LEFT|AREA_RITE|AREA_TOP|AREA_BOTTOM,
	AREA_MOVE = AREA_CAPTION,
};
class rectengine_root_c;
class rectengine_c : public sqhandler_i
{
    DUMMY(rectengine_c);

	friend class guirect_c;
	guirect_c::sptr_t rect_;
    RID rid_;
	UNIQUE_PTR(mousetrack_data_s) mtrack_;

	void set_controlled_rect(guirect_c *r)
	{
		rect_ = r;
        rid_ = r->getrid();
	}

protected:
    bool cleanup_children(RID,GUIPARAM);
    ts::array_safe_t<rectengine_c, 2> children;
    ts::array_safe_t<rectengine_c, 2> children_sorted; // zindex sorted
    int drawntag;

    int child_index(RID rid) const
    {
        int cnt = children.size();
        for (int i = 0; i < cnt; ++i)
            if (rectengine_c *e = children.get(i))
                if (e->getrid() == rid) return i;
        return -1;
    }
    guirect_c *rect() { return rect_; }
    const guirect_c *rect() const { return rect_; }

public:
    int operator()(const rectengine_c *r) const
    {
        const rectengine_c *me = this;
        if (r == nullptr) return 1;
        if (me == nullptr) return -1;
        int x = ts::SIGN( getrect().getprops().zindex() - r->getrect().getprops().zindex() );
        return x == 0 ? ( ts::SIGN((ts::aint)this - (ts::aint)r) ) : x;
    }
public:
	rectengine_c();
	virtual ~rectengine_c();

    void cleanup_children_now();
    void resort_children(); // resort children according to zindex
    bool children_sort( fastdelegate::FastDelegate< bool (rectengine_c *, rectengine_c *) > swap_them );
    void child_move_top( rectengine_c *e ) { child_move_to(0, e); }
    void child_move_to( int index, rectengine_c *e );
    

    const guirect_c &getrect() const { ASSERT(this); return SAFE_REF(rect_); }
    guirect_c &getrect() { ASSERT(this); return SAFE_REF(rect_); }

    virtual ts::uint32 detect_area(const ts::ivec2 &localpt);
    virtual bool detect_hover(const ts::ivec2 & screenpos) const { return false; };
    
    RID find_child_by_point(const ts::ivec2 & screenpos) const 
    {
        RID rr = getrid();
        for (rectengine_c *c : children_sorted)
            if (c)
            {
                const rectprops_c &rps = c->getrect().getprops();
                if (rps.is_visible() && !rps.out_of_bound() && rps.screenrect().inside(screenpos))
                {
                    rr = c->find_child_by_point(screenpos);
                    break;
                }
            }
        return rr;
    };

    ts::ivec2 to_screen(const ts::ivec2 &p) const
    {
        if (const guirect_c *r = rect_)
            return r->to_screen(p);
        WARNING( "no rect" );
        return p;
    }
    ts::ivec2 to_local(const ts::ivec2 &screenpos) const
    {
        if (const guirect_c *r = rect_)
            return r->to_local(screenpos);
        WARNING( "no rect" );
        return screenpos;
    }
   
    void trunc_children(int index);
    void add_child(rectengine_c *re, RID after);
    ts::aint children_count() const { return children.size();}
    rectengine_c *get_child(ts::aint index) {return children.get(index);};
    const rectengine_c *get_child(ts::aint index) const {return children.get(index);};
    int get_child_index(const rectengine_c *e) const { return children.find(e); }
    rectengine_c *get_last_child();
    rectengine_c *get_next_child( const rectengine_c *e )
    {
        int i = children.find(e);
        if (i >= 0)
            while( ++i < children.size() )
                if ( rectengine_c * re = children.get(i) )
                    return re;
        return nullptr;
    }

    RID getrid() const { return rid_; }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override; // system query - called by system

    void mouse_lock();
    void mouse_unlock();

    const mousetrack_data_s *mtrack(ts::uint32 o) const
    {
        if (const mousetrack_data_s *mt = &*mtrack_)
            if ((mt->mtt & o) != 0)
                return mt;
        return nullptr;
    };

    mousetrack_data_s *mtrack(RID rid, ts::uint32 o)
    {
        if (mousetrack_data_s *mt = &*mtrack_)
            if (mt->rid == rid && (mt->mtt & o) != 0)
                return mt;
        return nullptr;
    };

    mousetrack_data_s &begin_mousetrack(RID rid, mousetrack_type_e o)
    {
        mouse_lock();
        mtrack_.reset(TSNEW(mousetrack_data_s, o, rid));
        return *mtrack_;
    }
    void end_mousetrack(ts::uint32 o)
    {
        if (ASSERT(mtrack(o)))
        {
            mouse_unlock();
            mtrack_.reset();
        }
    }

    virtual void redraw(const ts::irect *invalidate_rect=nullptr) {}
	virtual bool apply(rectprops_c &rpss, const rectprops_c &pss) { return rpss.change_to(pss, this); }

	virtual draw_data_s & begin_draw() { return ts::make_dummy<draw_data_s>(); }
	virtual void end_draw() {}
    virtual void draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d = nullptr ) {}; // draw theme rect stuff
    virtual void draw( const ts::wstr_c & text, const text_draw_params_s & dp ) {}; // draw text
    virtual void draw( const ts::ivec2 & p, const ts::drawable_bitmap_c &bmp, const ts::irect& bmprect, bool alphablend) {}; // draw image
    virtual void draw( const ts::irect & rect, ts::TSCOLOR color, bool clip = true) {}; // draw rectangle


    auto begin() -> decltype(children)::OBJTYPE * { return children.begin(); }
    auto begin() const -> const decltype(children)::OBJTYPE * { return children.begin(); }

    auto end() -> decltype(children)::OBJTYPE *{ return children.end(); }
    auto end() const -> const decltype(children)::OBJTYPE *{ return children.end(); }


    // special functions
    ts::ivec2 __spec_to_screen_calc(const ts::ivec2 &p) const // calculates (slow)
    {
        if (const guirect_c *r = rect_)
            return r->__spec_to_screen_calc(p);
        return p;
    }
    void __spec_apply_screen_pos_delta(const ts::ivec2 &delta)
    {
        if (guirect_c *r = rect_)
            r->__spec_apply_screen_pos_delta(delta);
        __spec_apply_screen_pos_delta_not_me(delta);
    }
    void __spec_apply_screen_pos_delta_not_me(const ts::ivec2 &delta)
    {
        for (rectengine_c *c : children)
            if (c) c->__spec_apply_screen_pos_delta(delta);
    }
    void __spec_set_outofbound(bool f)
    {
        if (guirect_c *r = rect_)
            r->__spec_set_outofbound(f);
    }
};

/*
Only root engine knows system-specific gui machinery
*/

class rectengine_root_c : public rectengine_c
{
    DUMMY(rectengine_root_c);

    friend struct drawcollector;
	HWND hwnd;
	HDC dc;
    ts::irect redraw_rect;
    void *borderdata = nullptr;
	ts::drawable_bitmap_c backbuffer;
    ts::array_inplace_t<draw_data_s, 4> drawdata;

    int drawtag = 0;
    ts::flags32_s flags;
    static const ts::flags32_s::BITS F_DIP = SETBIT(0);
    static const ts::flags32_s::BITS F_REDRAW_COLLECTOR = SETBIT(1);
    static const ts::flags32_s::BITS F_NOTIFY_ICON = SETBIT(2);
    static const ts::flags32_s::BITS F_SYSTEM = SETBIT(3);

	static int regclassref;
	void registerclass();
	static const ts::wchar *classname() {return L"rectenginewindow";}
#if defined _DEBUG || defined _CRASH_HANDLER
    static LRESULT CALLBACK wndhandler_dojob(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
    static LRESULT CALLBACK wndhandler_border_dojob(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif
	static LRESULT CALLBACK wndhandler(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
    static LRESULT CALLBACK wndhandler_border(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);

	static rectengine_root_c * hwnd2engine(HWND hwnd)
	{
		return ts::p_cast<rectengine_root_c *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
	}
	void recreate_back_buffer(const ts::ivec2 &sz, bool exact_size = false);

    //sqhandler_i
	/*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;

    static rectengine_root_c *redraw_collector(rectengine_root_c * root) { if (!root || root->flags.is(F_REDRAW_COLLECTOR)) return nullptr; root->flags.set(F_REDRAW_COLLECTOR); return root; }
    void redraw_now();
    bool redraw_required() const { return drawtag != drawntag; }

    bool refresh_frame(RID r = RID(), GUIPARAM p = nullptr);
    void kill_window();

    void draw_back_buffer();

    bool is_collapsed() const
    {
        return IsIconic(hwnd) != 0;
    }

    bool is_maximized() const
    {
        RECT r;
        GetWindowRect(hwnd,&r);
        return ts::ref_cast<ts::irect>(r) == ts::wnd_get_max_size( getrect().getprops().screenrect() );
    }

public:
	rectengine_root_c(bool sys);
	~rectengine_root_c();

    bool is_dip() const {return flags.is(F_DIP) || nullptr == rect();}

    int current_drawtag() const {return drawtag;}

    /*virtual*/ bool detect_hover(const ts::ivec2 & screenmousepos) const override { return getrect().getprops().is_visible() && hwnd == WindowFromPoint((POINT &)screenmousepos); };

    /*virtual*/ void redraw(const ts::irect *invalidate_rect = nullptr) override;
	/*virtual*/ bool apply(rectprops_c &rpss, const rectprops_c &pss) override;
	
	/*virtual*/ draw_data_s & begin_draw() override;
	/*virtual*/ void end_draw() override;
	/*virtual*/ void draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d = nullptr ) override;
	/*virtual*/ void draw( const ts::wstr_c & text, const text_draw_params_s & dp ) override;
    /*virtual*/ void draw( const ts::ivec2 & p, const ts::drawable_bitmap_c &bmp, const ts::irect& bmprect, bool alphablend) override;
    /*virtual*/ void draw( const ts::irect & rect, ts::TSCOLOR color, bool clip ) override;

    const ts::ivec2 &get_current_draw_offset() const { return drawdata.last().offset; }

    bool update_foreground();
    void set_system_focus(bool bring_to_front = false);
    void flash();
    void notification_icon( const ts::wsptr& text );

    bool is_foreground() const
    {
        return GetForegroundWindow() == hwnd;
    }


    ts::wstr_c  load_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title);
    bool        load_filename_dialog(ts::wstrings_c &files, const ts::wsptr &iroot, const ts::wsptr& name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title);
    ts::wstr_c  save_directory_dialog(const ts::wsptr &root, const ts::wsptr &title, const ts::wsptr &selectpath = ts::wsptr(), bool nonewfolder = false);
    ts::wstr_c  save_filename_dialog(const ts::wsptr &iroot, const ts::wsptr &name, const ts::wsptr &filt, const ts::wchar *defext, const ts::wchar *title);

};

INLINE rectengine_root_c *root_by_rid( RID r )
{
    if (r)
    {
        HOLD rr(r);
        if (rr) return rr().getroot();
    }
    return nullptr;
}

INLINE rectengine_root_c *guirect_c::getroot() const { return (m_root && !m_root->is_dip()) ? m_root : nullptr; }

INLINE RID guirect_c::getrootrid() const { return m_root ? m_root->getrid() : RID(); }

INLINE ts::irect guirect_c::local_to_root(const ts::irect &localr) const
{
    if (is_root() || !m_root) return localr;
    return m_root->getrect().to_local(to_screen(localr));
}

INLINE ts::ivec2 guirect_c::local_to_root(const ts::ivec2 &localpt) const
{
    if (is_root() || !m_root) return localpt;
    return m_root->getrect().to_local(to_screen(localpt));
}

INLINE ts::ivec2 guirect_c::root_to_local(const ts::ivec2 &rootpt) const
{
    if (is_root() || !m_root) return rootpt;
    return to_local(m_root->getrect().to_screen(rootpt));
}

struct drawcollector
{
    drawcollector() : engine(nullptr) {}
    explicit drawcollector(rectengine_root_c *root) :engine(rectengine_root_c::redraw_collector(root)) {}
    ~drawcollector()
    {
        if (engine)
        {
            if (engine->redraw_required())
                engine->redraw_now();
            engine->flags.clear(rectengine_root_c::F_REDRAW_COLLECTOR);
        }
    }
    drawcollector(drawcollector&&odch) :engine(odch.engine) { odch.engine = nullptr; }
    drawcollector &operator=(rectengine_root_c *root)
    {
        ASSERT(engine == nullptr);
        engine = rectengine_root_c::redraw_collector(root);
        return *this;
    }
    drawcollector &operator=(const drawcollector&) UNUSED;
    drawcollector(const drawcollector &) UNUSED;
private:
    ts::safe_ptr<rectengine_root_c> engine;
};

class rectengine_child_c : public rectengine_c
{
	guirect_c::sptr_t parent;

    //sqhandler_i
    /*virtual*/ bool sq_evt( system_query_e qp, RID rid, evt_data_s &data ) override;

public:
	rectengine_child_c(guirect_c *parent, RID after);
	~rectengine_child_c();

    /*virtual*/ void redraw(const ts::irect *invalidate_rect = nullptr) override;

    /*virtual*/ draw_data_s & begin_draw() override;
    /*virtual*/ void end_draw() override { getrect().getroot()->end_draw(); }

	/*virtual*/ void draw( const theme_rect_s &thr, ts::uint32 options, evt_data_s *d = nullptr ) override;
	/*virtual*/ void draw(const ts::wstr_c & text, const text_draw_params_s & dp) override;
    /*virtual*/ void draw( const ts::ivec2 & p, const ts::drawable_bitmap_c &bmp, const ts::irect& bmprect, bool alphablend) override;
    /*virtual*/ void draw( const ts::irect & rect, ts::TSCOLOR color, bool clip) override;
};

#endif