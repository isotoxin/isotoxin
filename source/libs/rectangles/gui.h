#pragma once

enum loc_label_e
{
    LL_CTXMENU_COPY,
    LL_CTXMENU_CUT,
    LL_CTXMENU_PASTE,
    LL_CTXMENU_DELETE,
    LL_CTXMENU_SELALL,

    LL_ABTT_CLOSE,
    LL_ABTT_MAXIMIZE,
    LL_ABTT_NORMALIZE,
    LL_ABTT_MINIMIZE,

    LL_ANY_FILES,
};

enum naction_e
{
    NIA_LCLICK,
    NIA_L2CLICK,
    NIA_RCLICK,
};

class delay_event_c : public ts::timer_subscriber_c
{
protected:

    GUIPARAMHANDLER handler;
    GUIPARAM param = nullptr;

public:
    DEBUGCODE(bool dip = false;);
    delay_event_c(GUIPARAM param = nullptr):param(param) {}

    virtual ~delay_event_c();
    virtual void    timeup(void * par);
    virtual void    doit() { handler(RID(), param); };
    virtual void    die();

    bool operator==(const GUIPARAMHANDLER &h) const { return h == handler; }

    void set_handler(GUIPARAMHANDLER _handler, GUIPARAM _param) { handler = _handler; param = _param; }
    GUIPARAM par() { return param; }
};

#define DEFERRED_EXECUTION_BLOCK_BEGIN(t_sec) typedef struct UNIQIDLINE(dc) : public delay_event_c { static double gett() {return t_sec;} UNIQIDLINE(dc)(GUIPARAM param = nullptr):delay_event_c(param) {} virtual void  die() {gui->delete_event<UNIQIDLINE(dc)>(this);} virtual void doit() {
#define DEFERRED_EXECUTION_BLOCK_END(param) } } UNIQIDLINE(dc); gui->add_event_t<UNIQIDLINE(dc), GUIPARAM>(UNIQIDLINE(dc)::gett(), as_param(param));

#define DEFERRED_CALL( t_sec, h, p ) do { delay_event_c &__dc = gui->add_event_t<delay_event_c, GUIPARAM>(t_sec, nullptr); __dc.set_handler( (h), as_param(p) ); } while(false)
#define DEFERRED_UNIQUE_CALL( t_sec, h, p ) do { auto hh = (h); gui->delete_event(hh); delay_event_c &__dc = gui->add_event_t<delay_event_c, GUIPARAM>(t_sec, nullptr); __dc.set_handler( hh, as_param(p) ); } while (false)
#define DEFERRED_UNIQUE_PAR_CALL( t_sec, h, p ) do { auto hh = (h); gui->delete_event(hh, as_param(p)); delay_event_c &__dc = gui->add_event_t<delay_event_c, GUIPARAM>(t_sec, nullptr); __dc.set_handler( hh, as_param(p) ); } while (false)


struct hover_data_s
{
    RID rid;
    RID locked;
    RID focus;
    RID active_focus; // rect that actually accepts input (buttons, text inputs)
    RID minside; // mouse inside
    ts::uint32 area = 0;
    ts::ivec2 mp;
    hover_data_s():mp(maximum<ts::ivec2::TYPE>::value) {}
};

struct bcreate_s
{
    GET_BUTTON_FACE face;
    GUIPARAMHANDLER handler;
    int tag;
    GET_TOOLTIP tooltip;
    ts::wsptr btext;
};

struct selectable_core_s
{
    ts::safe_ptr<gui_label_c> owner;
    ts::ivec2 glyphs_pos = ts::ivec2(0);
    int glyph_under_cursor = -1;
    int glyph_start_sel = -1;
    int glyph_end_sel = -1;
    int char_start_sel = -1;
    int char_end_sel = -1;
    int flashing = 0;
    bool dirty = false;
    bool clear_selection_after_flashing = false;

    bool copy_hotkey_handler(RID, GUIPARAM);

    ts::wchar ggetchar( int glyphindex );

    bool flash(RID r = RID(), GUIPARAM p = as_param(4));
    void flash_and_clear_selection() { flash(RID(), as_param(100)); }
    bool selectword(RID, GUIPARAM);

    selectable_core_s();
    ~selectable_core_s();

    bool is_dirty() { bool r = dirty; dirty = false; return r; }

    void select_by_charinds(gui_label_c *label, int char_start_sel, int char_end_sel);
    void begin( gui_label_c *label );
    bool try_begin( gui_label_c *label );
    bool sure_selected();
    bool some_selected() const { return owner && char_start_sel >= 0 && char_end_sel >= 0 && char_start_sel != char_end_sel; }
    void selection_stuff(ts::bitmap_c &bmp, const ts::ivec2 &size);
    void clear_selection();
    void track();
    void endtrack();
    ts::uint32 detect_area(const ts::ivec2 &pos);

    ts::wstr_c get_selected_text();

};

class dragndrop_processor_c : public ts::safe_object
{
    ts::ivec2 clickpos;
    ts::ivec2 clickpos_rel;
    ts::safe_ptr<guirect_c> dndrect;
    ts::safe_ptr<guirect_c> dndobj;
    bool dndobjdropable = false;
public:
    dragndrop_processor_c( guirect_c *dndrect );
    virtual ~dragndrop_processor_c();
    guirect_c *underproc() {return dndrect;}
    ts::irect rect() const
    {
        return dndobj ? dndobj->getprops().screenrect() : ts::irect(0);
    }

    void mm(const ts::ivec2& cursorpos);
    void update();
    void droped();
};

enum dndaction_e
{
    DNDA_DRAG,
    DNDA_DROP,
    DNDA_CLEAN,
};

template<> struct gmsg<GM_DRAGNDROP> : public gmsgbase
{
    gmsg(dndaction_e a) :gmsgbase(GM_DRAGNDROP), a(a) {}
    dndaction_e a;
};

extern int sysmodal;

class guirect_watch_c
{
    friend class gui_c;
    guirect_watch_c *prev = nullptr;
    guirect_watch_c *next = nullptr;
    GUIPARAMHANDLER h;
    GUIPARAM p;
    RID watchrid;
public:
    guirect_watch_c( RID r, GUIPARAMHANDLER h, GUIPARAM p = nullptr );
    ~guirect_watch_c();
};

class gui_c
{
	friend class HOLD;
    friend class MODIFY;
    friend class delay_event_c;
    friend class guirect_watch_c;
    template<typename R> friend struct MAKE_CHILD;
    template<typename R> friend struct MAKE_VISIBLE_CHILD;

	SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_BEFORE_INIT );
	SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_LOOP );
	SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_IDLE );

    SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_KEYBOARD );
    SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_WCHAR );
    SIMPLE_SYSTEM_EVENT_RECEIVER( gui_c, SEV_MOUSE );

    GM_RECEIVER(gui_c, GM_ROOT_FOCUS);
    GM_RECEIVER(gui_c, GM_UI_EVENT);

    guirect_watch_c *first_watch = nullptr;
    guirect_watch_c *last_watch = nullptr;


    ts::uint32 handle_char( ts::wchar c );
    ts::uint32 keyboard(const system_event_param_s & p);
    ts::uint32 mouse(const system_event_param_s & p);
    void loop();

    ts::safe_ptr<dragndrop_processor_c> dndproc;
    mousetrack_data_s mtrack_;

    class tempbuf_c : public ts::safe_object
    {
    public:
        bool selfdie(RID, GUIPARAM)
        {
            this->~tempbuf_c();
            MM_FREE(this);
            return true;
        }
        tempbuf_c(double ttl);
        static tempbuf_c *build( double ttl, ts::aint bufsize )
        {
            tempbuf_c *b = (tempbuf_c *)MM_ALLOC(bufsize + sizeof(tempbuf_c));
            TSPLACENEW(b, ttl);
            return b;
        }
    };

    ts::hashmap_t< int, ts::safe_ptr<tempbuf_c> > m_tempbufs;
    ts::tbuf_t<int> m_usedtags4buf;
    int m_checkindex = 0;

    int get_temp_buf(double ttl, ts::aint sz);
    void *lock_temp_buf(int tag);
    void kill_temp_buf(int tag);

    ts::flags32_s m_flags;

	static const ts::flags32_s::BITS F_INITIALIZATION   = SETBIT(0);
    static const ts::flags32_s::BITS F_DIRTY_HOVER_DATA = SETBIT(1);
    static const ts::flags32_s::BITS F_DO_MOUSEMOUVE = SETBIT(2);
    static const ts::flags32_s::BITS F_PROCESSING_REPOS = SETBIT(3);

    int m_tagpool = 1;
    text_rect_dynamic_c m_textrect; // temp usage
	theme_c m_theme;
    hover_data_s m_hoverdata;
	
    selectable_core_s m_selcore;

    ts::tbuf_t<RID> m_exclusive_input;

    ts::safe_ptr<guirect_c> m_active_hint_zone;
    ts::array_safe_t<guirect_c, 1> m_hint_zone;
	ts::array_safe_t<guirect_c, 2> m_rects;
    ts::tbuf_t<RID> m_roots;
    typedef ts::pair_s<ts::Time,RID> free_rid;
	ts::tbuf_t< free_rid > m_emptyids;

    ts::array_safe_t<gui_group_c, 4> m_children_repos;

    ts::array_inplace_t<drawcollector, 4> m_dcolls;
    int m_dcolls_ref = 0;

    static const ts::flags32_s::BITS PEF_CHILDREN_REPOS = SETBIT(0);
protected:
    ts::flags32_s m_post_effect;
    static const ts::flags32_s::BITS PEF_FREEBITSTART = SETBIT(1);
private:

    struct kbd_press_callback_s
    {
        DUMMY(kbd_press_callback_s);
        kbd_press_callback_s() {}
        GUIPARAMHANDLER handler;

        ts::uint32 scancode;
    };
    ts::array_inplace_t<kbd_press_callback_s, 1> m_kbdhandlers;


    struct texture_s
    {
        ts::iweak_ptr<text_rect_dynamic_c> owner;
        ts::bitmap_c texture;
    };
    ts::array_del_t<texture_s, 32> m_textures;



    RID get_free_rid();

	guirect_c *get_rect(RID id)
	{
		if (CHECK(id.index() >= 0 && id.index() < m_rects.size()))
			return m_rects.get(id.index());
		return nullptr;
	}

    bool b_close(RID r, GUIPARAM param);
    bool b_maximize(RID r, GUIPARAM param);
    bool b_minimize(RID r, GUIPARAM param);
    bool b_normalize(RID r, GUIPARAM param);

    ts::wstr_c tt_close() { return app_loclabel(LL_ABTT_CLOSE); }
    ts::wstr_c tt_maximize() { return app_loclabel(LL_ABTT_MAXIMIZE); }
    ts::wstr_c tt_minimize() { return app_loclabel(LL_ABTT_MINIMIZE); }
    ts::wstr_c tt_normalize() { return app_loclabel(LL_ABTT_NORMALIZE); }

    typedef ts::iweak_ptr<ts::timer_subscriber_c> dehook;
    spinlock::syncvar< ts::array_inplace_t<dehook, 4> > m_events;
    ts::timerprocessor_c    m_timer_processor;
    ts::frame_time_c m_frametime;
    ts::time_reducer_s<1000> m_1second;
    ts::time_reducer_s<5000> m_5seconds;
    ts::struct_pool_t<sizeof(delay_event_c), 128> m_evpool;

    ts::str_c m_deffont_name;
    ts::hashmap_t<ts::str_c, ts::font_desc_c> m_fonts;

    struct slallocator
    {
        static void *ma(size_t sz) { return MM_ALLOC(sz); }
        static void mf(void *ptr) { MM_FREE(ptr); }
    };

    spinlock::spinlock_queue_s<gmsgbase *, slallocator> m_msgs;

    void    add_event(delay_event_c *dc, double t);
    void    delete_event(delay_event_c *dc);

    void heartbeat(); // every 1 second
    void font_par(const ts::str_c& fn, ts::font_params_s& fprm) { app_font_par(fn, fprm); }
protected:
    virtual void app_setup_custom_button(bcreate_s &) {};
    virtual void app_font_par(const ts::str_c&, ts::font_params_s&fprm) { }

    theme_c &get_theme() {return m_theme;}

public:

    ts::TSCOLOR selection_color = ts::ARGB(255, 255, 0);
    ts::TSCOLOR selection_bg_color = ts::ARGB(100, 100, 255);
    ts::TSCOLOR selection_bg_color_blink = ts::ARGB(0, 0, 155);

    static const ts::uint32 casw_ctrl = 1U << 31;
    static const ts::uint32 casw_alt = 1U << 30;
    static const ts::uint32 casw_shift = 1U << 29;
    static const ts::uint32 casw_win = 1U << 28;

    virtual ts::wsptr app_loclabel(loc_label_e ll) { return CONSTWSTR("???"); }
    virtual bool app_custom_button_state(int tag, int &shiftleft) { return true; }
    virtual void app_prepare_text_for_copy( ts::str_c &text_utf8 ) {}
    virtual void app_notification_icon_action(naction_e act, RID iconowner) {}

    virtual void app_fix_sleep_value(int &sleep_ms) {}
    virtual void app_5second_event() {}
    virtual void app_loop_event() {}

    virtual void app_b_minimize(RID main);
    virtual void app_b_close(RID main);

    virtual void app_path_expand_env(ts::wstr_c &path) {}

    gui_c();
	~gui_c();

    ts::bitmap_c * acquire_texture( text_rect_dynamic_c *requester, const ts::ivec2 &size );
    void release_texture( ts::bitmap_c * t );

    void reload_fonts();
    bool load_theme( const ts::wsptr&thn );
    
    const ts::font_desc_c & get_font( const ts::asptr &fontname );
    const ts::str_c &default_font_name() const { return m_deffont_name; }

    void resort_roots();

    bool repos_in_progress() const { return m_flags.is(F_PROCESSING_REPOS); }
    void repos_children( gui_group_c *g );
    void prepare_redraw_collector()
    {
        ++m_dcolls_ref;
        if (m_dcolls_ref == 1)
        {
            m_post_effect.clear();
            if (m_children_repos.size()) m_post_effect.set(PEF_CHILDREN_REPOS);
            for (RID r : m_roots)
                m_dcolls.add() = HOLD(r)().getroot();
        }
    }
    void flush_redraw_collector()
    {
        if (m_post_effect.__bits && m_dcolls_ref == 1)
            do_post_effect();

        --m_dcolls_ref;
        ASSERT(m_dcolls_ref >= 0);
        if (m_dcolls_ref == 0)
            m_dcolls.clear();
    }

    drawcollector& allocate_dcoll()
    {
        ASSERT( m_dcolls_ref > 0 );
        return m_dcolls.add();
    }
    void process_children_repos();
    virtual void do_post_effect();

    template<typename T> int temp_store( const T&t, double ttl = 1.0 )
    {
        TS_STATIC_CHECK( ts::is_movable<T>::value, "not movable!" );
        int b = get_temp_buf(ttl, sizeof(T));
        T *st = (T *)gui->lock_temp_buf(b);
        memcpy(st, &t, sizeof(T));
        return b;
    }
    template<typename T> T *temp_restore( int b )
    {
        return (T *)lock_temp_buf(b);
    }

    void enqueue_gmsg( gmsgbase *m )
    {
        m_msgs.push(m);
    }

    template<typename EVT, typename PRM> EVT  &add_event_t(double t, PRM param)
    {
        EVT *dc;
        if (sizeof(EVT) == sizeof(delay_event_c))
        {
            dc = (EVT *)m_evpool.alloc();
            TSPLACENEW(dc, param);
        }
        else
        {
            dc = TSNEW(EVT, param);
        }
        add_event(dc, t);
        return *dc;
    }
    template<typename EVT> void  delete_event(EVT *e)
    {
        if (sizeof(EVT) == sizeof(delay_event_c))
        {
            e->~EVT();
            m_evpool.dealloc(e);
        }
        else
            TSDEL(e);
    }
    void delete_event(GUIPARAMHANDLER h);
    void delete_event(GUIPARAMHANDLER h, GUIPARAM prm);

    void simulate_kbd( int scancode, ts::uint32 casw );
    void register_kbd_callback(GUIPARAMHANDLER handler, int scancode, ts::uint32 casw)
    {
        kbd_press_callback_s &cb = m_kbdhandlers.add();
        cb.handler = handler;
        cb.scancode = casw | scancode;
    }

    void unregister_kbd_callback(GUIPARAMHANDLER handler);

    ts::text_rect_c &tr() {return m_textrect;}
	const theme_c &theme() const {return m_theme;}
    DEBUGCODE( const theme_c &xtheme(); );
    const ts::tbuf_t<RID>& roots() const {return m_roots;}
    void nomorerect(RID rootrid, bool isroot);

    ts::ivec2 textsize( const ts::font_desc_c& font, const ts::wstr_c& text, int width_limit = -1, int flags = 0 );

    void make_app_buttons(RID rootappwindow, ts::uint32 allowed_buttons = 0xFFFFFFFF, GET_TOOLTIP closebhint = nullptr);

    int get_free_tag() {return m_tagpool++;}

    void dirty_hover_data() {m_flags.set(F_DIRTY_HOVER_DATA|F_DO_MOUSEMOUVE);};
    const hover_data_s &get_hoverdata( const ts::ivec2 & screenmousepos );
    void mouse_lock( RID rid );
    void mouse_inside( RID rid );
    void mouse_outside( RID rid = RID() );
    
    const mousetrack_data_s *mtrack(ts::uint32 o) const
    {
        if ((mtrack_.mtt & o) != 0)
            return &mtrack_;
        return nullptr;
    };

    mousetrack_data_s *mtrack(RID rid, ts::uint32 o)
    {
        if (mtrack_.rid == rid && (mtrack_.mtt & o) != 0)
            return &mtrack_;
        return nullptr;
    };

    mousetrack_data_s &begin_mousetrack(RID rid, mousetrack_type_e o)
    {
        mouse_lock( rid );
        mtrack_.rid = rid;
        mtrack_.mtt = o;
        return mtrack_;
    }
    bool end_mousetrack(RID r, ts::uint32 o)
    {
        if (mtrack(r,o))
        {
            mouse_lock(RID());
            mtrack_.mtt = MTT_none;
            return true;
        }
        return false;
    }
    void end_mousetrack()
    {
        mouse_lock( RID() );
        mtrack_.mtt = MTT_none;
    }


    guirect_c *active_hintzone() {return m_active_hint_zone;};
    void register_hintzone( guirect_c *r );
    void unregister_hintzone( guirect_c *r );
    void check_hintzone( const ts::ivec2 & screenmousepos );

    void exclusive_input(RID r, bool set = true);
    RID get_exclusive() const { return m_exclusive_input.last( RID() ); }
    bool allow_input(RID r) const { return sysmodal == 0 && (m_exclusive_input.count() == 0 || ( m_exclusive_input.last(RID()) >>= r )); }

    void dragndrop_lb( guirect_c *r );
    void dragndrop_update( guirect_c *r );
    ts::irect dragndrop_objrect();
    guirect_c *dragndrop_underproc() { return dndproc.expired() ? nullptr : dndproc->underproc(); }

    ts::ivec2 get_cursor_pos() const;

    void set_focus(RID rid, bool force_active_focus = false);
    RID get_focus() const {return m_hoverdata.focus; }
    RID get_active_focus() const {return m_hoverdata.active_focus; }
    RID get_minside() const {return m_hoverdata.minside; }

    selectable_core_s &selcore() { return m_selcore; }

    virtual HICON app_icon(bool for_tray) = 0;   //HICON icon = LoadIcon(g_sysconf.instance, MAKEINTRESOURCE(IDI_ICON));

    template<typename R> R * find_rect( GUIPARAM ptr )
    {
        for( guirect_c * r : m_rects )
            if (r == (guirect_c *)ptr)
                return dynamic_cast<R *>(r);
        return nullptr;
    }

    template<typename R, typename PAR> void newrect( PAR &data )
    {
        if (RID er = get_free_rid())
        {
            data.id = er;
            data.me = TSNEW(R, data);
            m_rects.get(data.id.index()) = data.me;
        }
        else
        {
            data.id = RID(m_rects.size());
            auto &sptr = m_rects.add();
            data.me = TSNEW(R, data);
            sptr = data.me;
        }
        
        if (data.parent)
        {
            ASSERT(data.me->getprops().screenpos() == ts::ivec2(0));
            data.me->__spec_apply_screen_pos_delta(HOLD(data.parent)().getprops().screenpos());
        }
        else
        {
            if (m_roots.count())
                data.me->__spec_set_zindex( HOLD(m_roots.last())().getprops().zindex()+1.0f );

            m_roots.add(data.id);
        }
        ((guirect_c *)data.me)->created();
        ASSERT( ((guirect_c *)data.me)->m_test_01, "Please call __super::created()" );
    }

};

extern gui_c *gui;

struct redraw_collector_s
{
    redraw_collector_s()
    {
        gui->prepare_redraw_collector();
    }
    ~redraw_collector_s()
    {
        gui->flush_redraw_collector();
    }
};

INLINE ts::uint32 gui_control_c::get_state() const
{
    ts::uint32 st = 0;
    if (getprops().is_highlighted()) st |= RST_HIGHLIGHT;
    if (getprops().is_active()) st |= RST_ACTIVE;
    if (gui->get_focus() == getrid()) st |= RST_FOCUS;
    return st;
}

INLINE const ts::font_desc_c &gui_label_c::get_font() const
{
    if (!ASSERT(textrect.font))
    {
        if (const theme_rect_s *thr = themerect()) return *thr->deffont;
        return ts::g_default_text_font;
    }
    return *textrect.font;
}

INLINE const ts::font_desc_c &gui_button_c::get_font() const
{
    if (!font)
    {
        if (const theme_rect_s *thr = themerect()) return *thr->deffont;
        return ts::g_default_text_font;
    }
    return *font;
}


template<typename R> MAKE_ROOT<R>::MAKE_ROOT()
{
    engine = TSNEW(rectengine_root_c, false);
    gui->allocate_dcoll() = (rectengine_root_c *)engine;
    gui->newrect<newrectkitchen::rectwrapper<R>::type>( *this );
}
template<typename R> void MAKE_ROOT< newrectkitchen::rectwrapper<R> >::init( bool sys )
{
    if (me) return;
    engine = TSNEW(rectengine_root_c, sys);
    gui->allocate_dcoll() = (rectengine_root_c *)engine;
    gui->newrect<R, MAKE_ROOT<R> >( (MAKE_ROOT<R> &)(*this) );
}

template<typename R> MAKE_CHILD<R>::MAKE_CHILD( RID parent_ )
{
    parent = parent_;
    engine = TSNEW(rectengine_child_c, gui->get_rect(parent), after);
    gui->newrect<newrectkitchen::rectwrapper<R>::type>( *this );
}

template<typename R> MAKE_VISIBLE_CHILD<R>::MAKE_VISIBLE_CHILD(RID parent_, bool visible):visible(visible)
{
    parent = parent_;
    engine = TSNEW(rectengine_child_c, gui->get_rect(parent), after);
    gui->newrect<newrectkitchen::rectwrapper<R>::type>(*this);
}

template<typename R> void MAKE_CHILD< newrectkitchen::rectwrapper<R> >::init()
{
    if (me) return;
    ASSERT(parent);
    engine = TSNEW(rectengine_child_c, gui->get_rect(parent), after);
    gui->newrect<R, MAKE_CHILD<R> >((MAKE_CHILD<R> &)(*this));
}
