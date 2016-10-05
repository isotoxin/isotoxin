#include "rectangles.h"

//-V:glyphs:807

gui_c *gui = nullptr;

gui_c::tempbuf_c::tempbuf_c( double ttl )
{
    if ( ttl >= 0 )
        DEFERRED_CALL( ttl, DELEGATE(this, selfdie), nullptr );
}

delay_event_c::~delay_event_c()
{
    DEBUGCODE(dip = true;);
    gui->delete_event(this);
}

void    delay_event_c::timeup(void * par)
{
    ts::iweak_ptr<ts::timer_subscriber_c, delay_event_c> hook = this;
    doit();
    if (hook) die();
}

void    delay_event_c::die()
{
    gui->delete_event<delay_event_c>(this);
}

void    gui_c::add_event(delay_event_c *de, double t)
{
    MEMT( MEMT_EVTSYSTEM );

    ASSERT( basetid == spinlock::pthread_self() );

    m_events.lock_write()().add(de);
    m_timer_processor.add(de, t, nullptr);
}
void    gui_c::delete_event(delay_event_c *de)
{
    ASSERT(de->dip);
    m_events.lock_write()().find_remove_fast(de);
}

void gui_c::delete_event(GUIPARAMHANDLER h)
{
    ASSERT( basetid == spinlock::pthread_self() );

    bool cleanup = false;
    auto w = m_events.lock_write();

    for ( ts::aint index = w().size() - 1; index >= 0; --index)
    {
        delay_event_c *de = ts::ptr_cast<delay_event_c *>(w().get(index).get());
        if ((*de) == h)
        {
            w().remove_fast(index); //<< this will kill hook
            de->die(); // and now we kill delay_event itself
            cleanup = true;
        }
    }
    w.unlock();
    if ( cleanup )
        m_timer_processor.cleanup();
}

void gui_c::delete_event(GUIPARAMHANDLER h, GUIPARAM prm)
{
    ASSERT( basetid == spinlock::pthread_self() );

    bool cleanup = false;
    auto w = m_events.lock_write();

    for ( ts::aint index = w().size() - 1; index >= 0; --index)
    {
        delay_event_c *de = ts::ptr_cast<delay_event_c *>(w().get(index).get());
        if ((*de) == h && de->par() == prm)
        {
            w().remove_fast(index); //<< this will kill hook
            de->die(); // and now we kill delay_event itself
            cleanup = true;
        }
    }
    w.unlock();
    if ( cleanup )
        m_timer_processor.cleanup();
}

bool gui_c::b_close(RID r, GUIPARAM param)
{
    app_b_close(RID::from_param(param));
    return true;
}
bool gui_c::b_maximize(RID r, GUIPARAM param)
{
    MODIFY(RID::from_param(param)).maximize(true);
    return true;
}
bool gui_c::b_minimize(RID r, GUIPARAM param)
{
    app_b_minimize(RID::from_param(param));
    return true;
}
bool gui_c::b_normalize(RID r, GUIPARAM param)
{
    MEMT( MEMT_GUI_COMMON );
    MODIFY(RID::from_param(param)).decollapse().maximize(false);
    return true;
}

/*virtual*/ void gui_c::app_b_minimize(RID main)
{
    MODIFY(main).minimize(true);
}
/*virtual*/ void gui_c::app_b_close(RID)
{
    ts::master().sys_exit(0);
}


gui_c::gui_c()
{
#ifdef _DEBUG
    basetid = spinlock::pthread_self();
#endif // _DEBUG

    ts::Time::update_thread_time();

    m_deffont_name = CONSTASTR("default");
    gui = this;
    dirty_hover_data();

}

gui_c::~gui_c() 
{
    if (dndproc) TSDEL(dndproc);
    ASSERT(gui == this);

    for(int tag : m_usedtags4buf)
    {
        tempbuf_c *b = m_tempbufs[tag];
        if (b)
        {
            delete_event( DELEGATE(b,selfdie) );
            b->selfdie(RID(),nullptr);
            ASSERT( m_tempbufs[tag].expired() );
        }
    }
    m_usedtags4buf.clear();
    m_tempbufs.clear();

    m_emptyids.clear();

    for(guirect_c *r : m_rects)
        TSDEL(r);

    gmsgbase *m;
    while( m_msgs.try_pop(m) )
        TSDEL(m);

    auto w = m_events.lock_write();

    for ( ts::aint index = w().size() - 1; index >= 0; --index )
    {
        delay_event_c *de = ts::ptr_cast<delay_event_c *>( w().get( index ).get() );
        w().remove_fast( index ); //<< this will kill hook
        de->die(); // and now we kill delay_event itself
    }
    w.unlock();
    m_timer_processor.clear();

    gui = nullptr;
}

void gui_c::reload_fonts()
{
    m_theme.reload_fonts( DELEGATE(this, font_par) );

    for (auto it = m_fonts.begin(); it; ++it)
        it->update();

}

bool gui_c::load_theme( const ts::wsptr&thn, bool summon_ch_signal )
{
    if (!m_theme.load(thn, DELEGATE(this, font_par), summon_ch_signal)) return false;

    for(auto it = m_fonts.begin(); it; ++it)
        it->update();

    return true;
}

const ts::font_desc_c & gui_c::get_font(const ts::asptr &fontname)
{
    bool add = false;
    auto &val = m_fonts.add_get_item(fontname, add);
    if (add) val.value.assign(val.key);
    else val.value.update();
    return val.value;
}

void gui_c::process_children_repos()
{
    MEMT( MEMT_CHREPOS );

    ts::tmp_pointers_t<gui_group_c, 8> g2p;

    for (gui_group_c *gg : m_children_repos)
        if (gg) g2p.add(gg);
    m_children_repos.clear();

    AUTOCLEAR( m_flags, F_PROCESSING_REPOS );

    for ( ts::aint i=0; i<g2p.size(); ++i )
    {
        gui_group_c *gg = g2p.get( i );
        m_repos_inprogress = gg;
        gg->children_repos();
        gg->getengine().redraw();
        m_repos_inprogress = nullptr;
        if ( m_children_repos.size() )
        {
            for ( gui_group_c *g : m_children_repos )
                if ( g ) g2p.add( g );
            m_children_repos.clear();
        }
    }

    ASSERT( m_children_repos.size() == 0 );
}

/*virtual*/ void gui_c::do_post_effect()
{
    if ( m_post_effect.is(PEF_CHILDREN_REPOS) )
        process_children_repos();

    m_post_effect.clear();
}

int gui_c::get_temp_buf(double ttl, ts::aint sz)
{
    int tag = get_free_tag();
    m_usedtags4buf.add(tag);
    tempbuf_c *b = tempbuf_c::build(ttl, sz);
    m_tempbufs[ tag ] = b;
    return tag;
}

void * gui_c::lock_temp_buf(int tag)
{
    tempbuf_c * b = m_tempbufs[ tag ].get();
    return b ? (b + 1) : nullptr;
}

void gui_c::kill_temp_buf(int tag)
{
    tempbuf_c * b = m_tempbufs[ tag ].get();

    if (b)
    {
        delete_event(DELEGATE(b, selfdie));
        b->selfdie(RID(), nullptr);
    }
}

void gui_c::heartbeat()
{
#ifdef _DEBUG
    // check something 1 per sec
    {
        ts::tmp_array_inplace_t<RID, 1> rootstemp;
        for (RID rid : roots())
            rootstemp.add(rid);

        for (RID rid : rootstemp)
        {
            HOLD r(rid);
            r().prepare_test_00();
            r.engine().sq_evt(SQ_TEST_00, rid, ts::make_dummy<evt_data_s>(true));
        }
    }

#endif

    if (m_usedtags4buf.count())
    {
        m_checkindex = m_checkindex % m_usedtags4buf.count();
        int chktag = m_usedtags4buf.get(m_checkindex);
        if (lock_temp_buf(chktag) == nullptr)
        {
            m_tempbufs.remove(chktag);
            m_usedtags4buf.remove_fast(m_checkindex);
        }
        ++m_checkindex;
    }
    if (nullptr == ts::master().get_focus() && ts::master().is_active)
        if (rectengine_root_c *root = root_by_rid( roots().last(RID()) ))
            if (!root->getrect().getprops().is_collapsed())
                root->set_system_focus();

    if ( m_textures_check_index > m_textures.size() )
        m_textures_check_index = 0;
    if ( m_textures_check_index < m_textures.size() )
    {
        texture_s *tt = m_textures.get( m_textures_check_index );
        if ( ( ts::Time::current() - tt->lastusetime ) > 5000 ) // old texture! release it!
        {
            if ( tt->owner )
                tt->owner->textures_no_need();
            tt->clear();
            tt->pixels_capacity = 0;
        }
    }
    ++m_textures_check_index;

    gmsg<GM_HEARTBEAT>().send();

#if 0
    static struct d
    {
        HWND massiv[2048];
        int numw = 0;
        int curi = 0;
        int oldnumw = 0;
    
        void add(HWND h)
        {
            if (!IsWindowVisible(h)) return;

            for(int i=0;i<oldnumw;++i)
                if (massiv[i] == h)
                {
                    if (i > curi) 
                    {
                        SWAP( massiv[i], massiv[curi] );
                    }
                    if (i >= curi) 
                        ++curi;
                    return;
                }
            for (int i = oldnumw; i < numw; ++i)
                if (massiv[i] == h)
                    return;

            if (ASSERT(numw < (ARRAY_SIZE(massiv) - 1)))
                massiv[numw++] = h;
        }

        static BOOL CALLBACK EnumChildProc(
            _In_  HWND hwnd,
            _In_  LPARAM lParam
            )
        {
            ((d *)lParam)->add(hwnd);
            EnumChildWindows(hwnd, EnumChildProc, lParam);
            return TRUE;
        }

        void logw(HWND hwnd, const char *inf)
        {
            char b[256], c[256];
            b[0] = 0; c[0] = 0;
            HWND parent = nullptr;
            HWND owner = nullptr;
            if (inf[0] != 'd')
            {
                GetWindowTextA(hwnd, b, 255);
                GetClassNameA(hwnd, c, 255);
                parent = GetParent(hwnd);
                owner = GetWindow(hwnd, GW_OWNER);
                
            }

            static const char *ignore[] = {
                "tooltips_class32",
                "MSCTFIME UI",
                "IME",
                "NotifyIconOverflowWindow",
                "TaskListThumbnailWnd",
            };
            
            for(int i=0;i<ARRAY_SIZE(ignore);++i)
            {
                if (ts::CHARz_equal(ignore[i], c)) return;
            }

            DMSG("" << inf << hwnd << parent << owner << c << "|" << b);
        }

        void logw()
        {
            // new windows
            for(int i=oldnumw; i<numw;++i)
                logw(massiv[i], "new");

            for (int i = curi; i < oldnumw; ++i)
                logw(massiv[i], "del");

            memcpy( massiv + curi, massiv + oldnumw, (numw - oldnumw) * sizeof(HWND) );
            numw -= oldnumw - curi;

        }

    } data;

    data.curi = 0;
    data.oldnumw = data.numw;

    EnumWindows(&d::EnumChildProc, (LPARAM)&data);

    data.logw();
#endif
}

void gui_c::simulate_kbd(int scancode, ts::uint32 casw)
{
    ts::uint32 signalkbd = scancode | casw;
    for ( ts::aint i = m_kbdhandlers.size() - 1; i >= 0; --i) // back order! latest handler handle key first
    {
        const kbd_press_callback_s &cb = m_kbdhandlers.get(i);
        if (cb.scancode == signalkbd)
        {
            if (cb.handler(RID(), nullptr))
                break;
        }
    }
}

void gui_c::unregister_kbd_callback(GUIPARAMHANDLER handler)
{
    for ( ts::aint i = m_kbdhandlers.size() - 1; i >= 0; --i)
    {
        kbd_press_callback_s &cb = m_kbdhandlers.get(i);
        if (cb.handler == handler)
            m_kbdhandlers.get_remove_slow(i);
    }
}


bool gui_c::handle_keyboard(int scan, bool dn, int casw)
{
    redraw_collector_s dch;

    if (RID f = gui->get_focus())
        if (allow_input(f))
        {

            evt_data_s d;
            d.kbd.scan = scan;
            d.kbd.charcode = 0;
            d.kbd.casw = casw;
            if (HOLD(f).engine().sq_evt(dn ? SQ_KEYDOWN : SQ_KEYUP, f, d))
                return true;
        }

    if (dn)
        simulate_kbd( scan, casw );

    return false;
}

bool gui_c::handle_char( wchar_t c )
{
    if ( c == 8 || c == 9 || c == 27 ) // some codes cannot be handled by wchar
        return false;

    if (RID f = gui->get_focus())
        if (allow_input(f))
        {
            redraw_collector_s dch;

            evt_data_s d;
            d.kbd.scan = 0;
            d.kbd.charcode = c;
            d.kbd.casw = 0;
            HOLD(f).engine().sq_evt(SQ_CHAR, f, d);
            return true;
        }
    return false;
}

void gui_c::handle_mouse( ts::mouse_event_e me, const ts::ivec2 &scrpos )
{
    redraw_collector_s dch;

    if (dndproc)
    {
        if (me == ts::MEVT_MOVE)
            dndproc->mm( ts::ref_cast<ts::ivec2>( scrpos ) );
        else if (me == ts::MEVT_LUP)
            dndproc->droped();
    }
    
    if (me == ts::MEVT_WHEELUP || me == ts::MEVT_WHEELDN)
        if (RID f = gui->get_minside())
            if (allow_input(f))
            {
                evt_data_s d;
                d.mouse.screenpos = scrpos;

                rectengine_c &engine = HOLD(f).engine();
                if (me == ts::MEVT_WHEELUP)
                    engine.sq_evt(SQ_MOUSE_WHEELUP, engine.getrid(), d);

                if ( me == ts::MEVT_WHEELDN )
                    engine.sq_evt(SQ_MOUSE_WHEELDOWN, engine.getrid(), d);
            }
}

void gui_c::sys_loop()
{
    int sleep = 1;

    { // draw collector should be flushed before sleep
        redraw_collector_s dch;

        ts::Time::update_thread_time();
        m_frametime.takt();
        if (m_1second.it_is_time_ones()) heartbeat();

        sleep = ts::lround(500.0f * m_timer_processor.takt(m_frametime.frame_time())); // sleep time 0.5 of time to next event
        if (sleep < 0 || sleep > 50) sleep = 50;

        if (m_5seconds.it_is_time_ones()) app_5second_event();
        animation_c::tick();
        app_loop_event();

        ts::tmp_pointers_t<gmsgbase,1> executing;

        gmsgbase *m;
        while ( executing.size() < 100 /* limit maximum to avoid interface freeze */ && m_msgs.try_pop(m) )
            executing.add(m); // pop messages as fast as possible

        for( gmsgbase * me : executing ) // now executing
        {
            me->send();
            TSDEL(me);
        }

        if (m_flags.is(F_DO_MOUSEMOUVE))
        {
            const hover_data_s &d = get_hoverdata(ts::get_cursor_pos());
            if (d.rid)
            {
                rectengine_root_c *rr = HOLD(d.rid)().getroot();
                if (rr) rr->simulate_mousemove();
            }
            m_flags.clear(F_DO_MOUSEMOUVE);
        }

        app_fix_sleep_value(sleep);
    }
    ts::master().sleep = sleep;
}

ts::uint32 gui_c::gm_handler(gmsg<GM_UI_EVENT>&e)
{
    if (UE_THEMECHANGED == e.evt)
    {
        for (RID r : roots())
            HOLD(r).engine().redraw();
    }
    return 0;
}

ts::ivec2 gui_c::textsize( const ts::font_desc_c& font, const ts::wstr_c& text, int width_limit, int flags )
{
    MEMT( MEMT_TEXTRECT );

    return m_textrect.calc_text_size(font,text,width_limit < 0 ? 16384 : width_limit, flags, nullptr);
}

RID gui_c::get_free_rid()
{
    // добываем из массива свободных RID'ов такой, который был удален более чем секунду назад
    // будем считать, что за секунду успеют отработать таски, оперируещие RID'ами

    ts::Time cur = ts::Time::current();
    for( free_rid &fr : m_emptyids )
    {
        if (cur > fr.first)
        {
            RID rr = fr.second;
            fr = m_emptyids.pop(); // на самом деле это плохая стратегия - заменять выбывший RID самым свежим. Это сделано исключительно из соображений, что пройтись по массиву все же менее затратно, чем сдвинуть его элементы в памяти
            return rr;
        }
    }
    return RID();
}

void gui_c::exclusive_input(RID r, bool set)
{
    if (set)
    {
        ASSERT( m_roots.find_index(r) >= 0, "Only root rect can be exclusive" );
        m_exclusive_input.add(r);
        mouse_lock(RID());
        mouse_outside();
        dirty_hover_data();
        ts::master().release_capture();

    } else
    {
        m_exclusive_input.find_remove_slow(r);
        if (rectengine_root_c *root = root_by_rid(m_roots.last(RID())))
        {
            ASSERT(allow_input(root->getrid()));
            root->update_foreground();
        }
    }
}

bool gui_c::is_menu(RID r) const
{
    if (!r) return false;
    HOLD hr(r);
    if (!hr) return false;
    
    guirect_c *rct = &hr.engine().getrect().getroot()->getrect();
    return nullptr != dynamic_cast<gui_popup_menu_c *>(rct);
}

bool gui_c::allow_input(RID r, bool check_click) const
{
    if (check_click)
    {
        gmsg<GM_CHECK_ALLOW_CLICK> chk(r);
        ts::flags32_s f = chk.send();
        if (f.is(GMRBIT_ACCEPTED)) return true;
        if (!chk.mnu.expired())
        {
            TSDEL(chk.mnu.get());
            return false;
        }

    }

    return !ts::master().is_sys_loop && (m_exclusive_input.count() == 0 || (m_exclusive_input.last(RID()) >>= r) || is_menu(r));
}

guirect_watch_c::guirect_watch_c(RID r, GUIPARAMHANDLER h, GUIPARAM p):watchrid(r), h(h), p(p)
{
    if(gui && r)
    {
        LIST_ADD(this, gui->first_watch, gui->last_watch, prev, next);
    }
}
guirect_watch_c::~guirect_watch_c()
{
    if (gui && watchrid)
        LIST_DEL( this, gui->first_watch, gui->last_watch, prev, next );
}

void gui_c::restore_focus( RID rid )
{
    bool dhd = false;
    if ( rid == m_hoverdata.active_focus ) { m_hoverdata.active_focus = RID(); dhd = true; }
    if ( rid == m_hoverdata.root_focus ) { m_hoverdata.root_focus = RID(); m_hoverdata.active_focus = RID(); dhd = true; }
    if ( dhd ) dirty_hover_data();
    m_hoverdata.rootfocushistory.find_remove_slow( rid );

    if ( !get_focus() )
    {

        if ( RID rooti = m_hoverdata.rootfocushistory.last( RID() ) )
        {
            m_hoverdata.rootfocushistory.remove_last();
            if ( allow_input( rooti ) )
                if ( rectengine_root_c *root = HOLD( rooti )( ).getroot() )
                {
                    gui->set_focus( rooti );
                    root->update_foreground();
                }
        }
    }
}

void gui_c::nomorerect(RID rid )
{
    bool isroot = m_roots.find_remove_fast(rid);
    if ( isroot ) m_hoverdata.rootfocushistory.find_remove_slow( rid );
    bool dhd = false;
    if (rid == m_hoverdata.active_focus) { m_hoverdata.active_focus = RID(); dhd = true; }
    if (rid == m_hoverdata.root_focus) { m_hoverdata.root_focus = RID(); m_hoverdata.active_focus = RID(); dhd = true; }
    if (rid == m_hoverdata.locked) { m_hoverdata.locked = RID(); dhd = true; }
    if (rid == m_hoverdata.minside) { m_hoverdata.minside = RID(); dhd = true; }
    if ( rid == m_hoverdata.mrealinside ) { m_hoverdata.mrealinside = RID(); dhd = true; }
    if (rid == m_hoverdata.rid) { m_hoverdata.rid = RID(); dhd = true; }
    if (dhd) dirty_hover_data();
    if (isroot)
        m_exclusive_input.find_remove_slow( rid );

    m_selcores.remove(rid);

    if ( m_hoverdata.root_focus && !m_hoverdata.active_focus )
        m_hoverdata.active_focus = m_hoverdata.root_focus;

    // provide RID's reuse
    //due RID is index, we want to minimize RID grow
    m_emptyids.add( ts::pair_s<ts::Time, RID>( ts::Time::current() + 1000, rid ) ); // reuse timeout set to 1 second

    for(guirect_watch_c *x = first_watch; x; )
    {
        if (x->watchrid == rid)
        {
            LIST_DEL_CLEAR(x, first_watch, last_watch, prev, next);
            x->watchrid = RID();
            x->h(rid,x->p); // after call of this handler, watch list can be totally changed
            x = first_watch; // so, we have to restart search from begining
            continue;
        }
        x = x->next;
    }
}

void gui_c::resort_roots()
{
    if (m_roots.qsort())
    {
        // надо установить родителей в соответствие zindex
        // этот способ работает в windows, т.к. родитель всегда отображается под потомком
        // TODO : ^^
        // есть еще заморочка с видимостью. невидимый рут-рект вообще не имеет hwnd окна
        // написание этой функции пока отложим due может быть вообще не понадобится
        STOPSTUB();
    }
}

void gui_c::no_repos_children( gui_group_c *g )
{
    m_children_repos.find_remove_fast(g);
    if ( m_children_repos.size() == 0 )
        m_post_effect.clear( PEF_CHILDREN_REPOS );
}

void gui_c::repos_children(gui_group_c *g)
{
    if (m_flags.is(F_PROCESSING_REPOS))
    {
        if ( m_repos_inprogress != g )
            g->children_repos();
        else
            m_children_repos.add(g);
        return;
    }

    m_post_effect.set(PEF_CHILDREN_REPOS);
    for (gui_group_c *gg : m_children_repos)
        if (gg == g)
            return;

    m_children_repos.add(g);
}

void gui_c::prepare_redraw_collector()
{
    ++m_dcolls_ref;
    if ( m_dcolls_ref == 1 )
    {
        m_post_effect.clear();
        if ( m_children_repos.size() ) m_post_effect.set( PEF_CHILDREN_REPOS );
        for ( RID r : m_roots )
            m_dcolls.add() = HOLD( r )( ).getroot();
    }
}
void gui_c::flush_redraw_collector()
{
    if ( m_post_effect.__bits && m_dcolls_ref == 1 )
        do_post_effect();

    --m_dcolls_ref;
    ASSERT( m_dcolls_ref >= 0 );
    if ( m_dcolls_ref == 0 )
        m_dcolls.clear();
}


namespace
{
class auto_app_buttons : public autoparam_i
{
    int index;
public:
    auto_app_buttons(guirect_c *prevr, int index):index(index)
    {
        if (prevr) prevr->leech(this);
    }
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
    {
        bool pc = qp == SQ_PARENT_RECT_CHANGED;
        bool c = qp == SQ_RECT_CHANGED;
        if (!pc && !c) return false;

        ASSERT(owner);
        guirect_c &root = SAFE_REF(owner->getroot()).getrect();

        const theme_rect_s *themerect = root.themerect();
        if (themerect)
        {
            int shiftleft = owner->getprops().size().x;

            switch (index)
            {
            case 0:
                if (pc && owner->getrid() == rid) // query for owner
                {
                    const rectprops_c &rootprops = root.getprops();
                    ts::irect ca = themerect->captionrect(rootprops.currentszrect(), rootprops.is_maximized());
                    ts::ivec2 capshift = (rootprops.is_maximized() || themerect->fastborder()) ? themerect->capbuttonsshift_max : themerect->capbuttonsshift;
                    MODIFY(*owner).pos(ca.rb.x - shiftleft + capshift.x, ca.lt.y + capshift.y);
                }
                break;
            default:
                if (c && owner->getrid() != rid) // query other
                {
                    bool _visible = true;
                    int tag = as_int(ts::ptr_cast<gui_control_c *>(owner)->get_customdata());
                    switch (tag)
                    {
                    case CBT_CLOSE:
                        break;
                    case CBT_MAXIMIZE:
                        _visible = !root.getprops().is_maximized();
                        if (!_visible) shiftleft = 0;
                        break;
                    case CBT_MINIMIZE:
                        break;
                    case CBT_NORMALIZE:
                        _visible = root.getprops().is_maximized();
                        if (!_visible) shiftleft = 0;
                        break;
                    default:
                        _visible = gui->app_custom_button_state(tag, shiftleft);
                        break;
                    }

                    HOLD other(rid);
                    MODIFY(*owner).pos( other().getprops().pos() - ts::ivec2(shiftleft,0) ).visible(_visible);
                }
                break;
            }
        }

        return false;
    }
};
}

void gui_c::make_app_buttons(RID rootappwindow, ts::uint32 allowed_buttons, bcreate_s *closeb, bcreate_s *minb )
{
    bcreate_s buttons[] = { 
        {BUTTON_FACE(close), DELEGATE( this, b_close ), CBT_CLOSE, DELEGATE( this, tt_close ) },
        {BUTTON_FACE(maximize), DELEGATE( this, b_maximize ), CBT_MAXIMIZE, DELEGATE( this, tt_maximize ) },
        {BUTTON_FACE(normalize), DELEGATE( this, b_normalize ), CBT_NORMALIZE, DELEGATE( this, tt_normalize ) },
        {BUTTON_FACE(minimize), DELEGATE( this, b_minimize ), CBT_MINIMIZE, DELEGATE( this, tt_minimize ) },
    };
    if ( closeb )
    {
        if ( closeb->tooltip ) buttons[ CBT_CLOSE ].tooltip = closeb->tooltip;
        if ( closeb->handler ) buttons[ CBT_CLOSE ].handler = closeb->handler;
    }
    if ( minb )
    {
        if ( minb->tooltip ) buttons[ CBT_MINIMIZE ].tooltip = minb->tooltip;
        if ( minb->handler ) buttons[ CBT_MINIMIZE ].handler = minb->handler;
    }
    auto setupcustom = [this](bcreate_s &bcr, int tag) -> bcreate_s * {

        ASSERT(tag >= CBT_APPCUSTOM);
        bcr.tag = tag;
        bcr.face = GET_BUTTON_FACE();
        bcr.handler = GUIPARAMHANDLER();
        bcr.tooltip = GET_TOOLTIP();
        app_setup_custom_button(bcr);
        return &bcr;
    };

    gui_button_c * prev = nullptr;
    bcreate_s custom;
    for (int i=0;;++i)
    {
        if (i < (sizeof(allowed_buttons)*8))
            if (0 == (allowed_buttons & SETBIT(i))) continue;

        bcreate_s *cbc = i < ARRAY_SIZE(buttons) ? buttons + i : setupcustom(custom, i);
        if (!cbc->face) break;

        gui_button_c &b = MAKE_CHILD<gui_button_c>(rootappwindow);
        b.tooltip(cbc->tooltip);
        b.set_face_getter(cbc->face);
        b.set_handler(cbc->handler, rootappwindow.to_param());
        b.set_customdata( as_param(cbc->tag) );
        b.leech( TSNEW(auto_app_buttons, prev, i) );
        MODIFY(b).show().size(b.get_min_size());
        prev = &b;
    }
}

const hover_data_s &gui_c::get_hoverdata( const ts::ivec2 & screenmousepos )
{
    bool need_update_hover_data = m_flags.is( F_DIRTY_HOVER_DATA ) || screenmousepos != m_hoverdata.mp;

    auto gethover = [&]() ->RID
    {
        for ( RID rrid : m_roots )
            if ( allow_input( rrid ) )
                if ( guirect_c *rr = m_rects.get( rrid.index() ) )
                    if ( rr->getengine().detect_hover( screenmousepos ) )
                        return rr->getengine().find_child_by_point( screenmousepos );
        return RID();
    };

    if (m_hoverdata.locked)
    {
        m_hoverdata.rid = m_hoverdata.locked;
        HOLD r(m_hoverdata.rid);
        m_hoverdata.area = r().getengine().detect_area(r().to_local(screenmousepos));
        if ( need_update_hover_data )
            m_hoverdata.mrealinside = gethover();
        m_flags.clear(F_DIRTY_HOVER_DATA);
        return m_hoverdata;
    }

    if ( !need_update_hover_data ) return m_hoverdata;

    RID ridhover = gethover();
    m_hoverdata.rid = RID();
    m_hoverdata.area = 0;
    m_hoverdata.mp = screenmousepos;
    m_hoverdata.mrealinside = ridhover;
    m_hoverdata.rid = ridhover;

    if (m_hoverdata.rid)
    {
        HOLD r(m_hoverdata.rid);
        m_hoverdata.area = r().getengine().detect_area( r().to_local(screenmousepos) );
    }
    m_flags.clear(F_DIRTY_HOVER_DATA);
    return m_hoverdata;
}

void gui_c::set_focus(RID rid)
{
    if ( rid )
    {
        HOLD hr( rid );
        if (hr && !hr( ).accept_focus() )
            return;
        if ( !hr ) rid = RID();
    }

    MEMT( MEMT_SETFOCUS );

    RID oldfocus = get_focus();

    auto set_root_focus = [this]( RID rid )
    {
        m_hoverdata.rootfocushistory.find_remove_slow( rid );
        if ( m_hoverdata.root_focus != rid )
        {
            if ( m_hoverdata.root_focus )
            {
                m_hoverdata.rootfocushistory.find_remove_slow( m_hoverdata.root_focus );
                m_hoverdata.rootfocushistory.add( m_hoverdata.root_focus );
            }
        }

        m_hoverdata.root_focus = rid;
    };

    if (rid)
    {
        if ( m_roots.present( rid ) )
        {
            set_root_focus( rid );
            m_hoverdata.active_focus = HOLD( rid )( ).getroot()->active_focus( RID() );
        }
        else
        {
            rectengine_root_c *root = HOLD( rid )( ).getroot();
            set_root_focus( root->getrid() );
            m_hoverdata.active_focus = root->active_focus( rid );
        }
        rectengine_root_c * root = HOLD( m_hoverdata.root_focus )( ).getroot();
        if ( root ) root->set_system_focus();
    } else
    {
        set_root_focus( RID() );
        m_hoverdata.active_focus = RID();
    }


    RID newfocus = get_focus();
    if (oldfocus != newfocus )
    {
        evt_data_s d;
        d.changed.focus = false;
        if ( oldfocus )
        {
            HOLD of( oldfocus );
            if (of)
                of.engine().sq_evt( SQ_FOCUS_CHANGED, oldfocus, d );
        }
        d.changed.focus = true;
        if ( newfocus )
        {
            HOLD( newfocus ).engine().sq_evt( SQ_FOCUS_CHANGED, newfocus, d );
        }
    }
}

void gui_c::mouse_lock( RID rid )
{
    if (m_hoverdata.locked != rid)
    {
        m_hoverdata.locked = rid;
        dirty_hover_data();
    }
}

void gui_c::mouse_inside(RID rid)
{
    if (m_hoverdata.minside) mouse_outside( m_hoverdata.minside );
    ASSERT( m_hoverdata.minside == RID() );

    HOLD r( rid );

    evt_data_s d;
    d.mouse.screenpos = ts::get_cursor_pos();

    m_hoverdata.minside = rid;

    DMSG( "in: "<< rid << HOLD(rid).engine().getrect().getprops().rect() );

    r.engine().sq_evt(SQ_MOUSE_IN, rid, d);
    if (m_active_hint_zone.expired()) gui_tooltip_c::create(rid);
}

void gui_c::mouse_outside(RID rid)
{
    ASSERT( rid == m_hoverdata.minside || rid == RID() );
    if (m_hoverdata.minside == RID()) return;
    HOLD r(m_hoverdata.minside);

    evt_data_s d;
    d.mouse.screenpos = ts::get_cursor_pos();

    m_hoverdata.minside = RID();
    r.engine().sq_evt(SQ_MOUSE_OUT, r().getrid(), d);
}

void gui_c::crop_selcore( RID rid )
{
    if ( auto *csc = m_selcores.find( rid ) )
    {
        UNIQUE_PTR( selectable_core_s ) tmp = std::move( csc->value );
        m_selcores.clear();
        if ( tmp )
            m_selcores.add( rid ) = std::move( tmp );
    } else
        m_selcores.clear();
}

void gui_c::del_selcore( RID rid )
{
    m_selcores.remove(rid);
}

selectable_core_s *gui_c::try_activate_selcore( gui_label_c *lbl )
{
    selectable_core_s *s = nullptr;

    if ( m_selcores.is_empty() )
        s = &activate_selcore( lbl, false );
    else
        s = get_selcore( lbl->getrid() );

    if ( s && s->try_begin( lbl ) )
        return s;

    return nullptr;
}

selectable_core_s &gui_c::activate_selcore( gui_label_c *lbl, bool reset_all )
{
    if ( reset_all )
        m_selcores.clear();

    bool added = false;
    auto &scp = m_selcores.add_get_item( lbl->getrid(), added );
    if ( added )
        scp.value.reset( TSNEW( selectable_core_s ) );

    scp.value->begin( lbl );

    return *scp.value;
}


void gui_c::register_hintzone(guirect_c *r)
{
    ts::aint cnt = m_hint_zone.size();
    int ff = -1;
    for (int i=0;i<cnt;++i)
    {
        if (m_hint_zone.get(i).expired() && ff < 0)
            ff = i;
        else if (m_hint_zone.get(i) == r)
            return;
    }
    if (ff < 0)
        m_hint_zone.add(r);
    else
        m_hint_zone.set(ff, r);

    if (!m_active_hint_zone)
        check_hintzone( ts::get_cursor_pos() );
}

void gui_c::unregister_hintzone(guirect_c *r)
{
    ts::aint cnt = m_hint_zone.size();
    for (int i = 0; i < cnt; ++i)
    {
        if (m_hint_zone.get(i) == r)
        {
            m_hint_zone.remove_fast(i);
            break;
        }
    }
    if (r == m_active_hint_zone)
    {
        m_active_hint_zone = nullptr;
        check_hintzone( ts::get_cursor_pos() );
    }
}

void gui_c::check_hintzone( const ts::ivec2 & screenmousepos )
{
    evt_data_s d;
    d.hintzone.accepted = false;
    if (m_active_hint_zone)
    {
        d.hintzone.pos = m_active_hint_zone->to_local(screenmousepos);
        m_active_hint_zone->sq_evt(SQ_CHECK_HINTZONE, m_active_hint_zone->getrid(), d);
        if (d.hintzone.accepted) return;
    }

    for( guirect_c *r : m_hint_zone )
    {
        if (r)
        {
            d.hintzone.accepted = false;
            d.hintzone.pos = r->to_local(screenmousepos);
            r->sq_evt(SQ_CHECK_HINTZONE, r->getrid(), d);
            if (d.hintzone.accepted)
            {
                m_active_hint_zone = r;
                gui_tooltip_c::create(r->getrid());
                r->sq_evt(SQ_CHECK_HINTZONE, r->getrid(), d); // touch hint now
                return;
            }
        }
    }
    m_active_hint_zone = nullptr;
    if (RID cr = get_hoverdata(screenmousepos).rid) gui_tooltip_c::create(cr);
}

dragndrop_processor_c::dragndrop_processor_c(guirect_c *dndrect):dndrect(dndrect)
{
    clickpos = ts::get_cursor_pos();
    clickpos_rel = dndrect->to_local(clickpos);
}
dragndrop_processor_c::~dragndrop_processor_c()
{
    if (dndobj)
        TSDEL(dndobj.get());
    if (dndrect)
        dndrect->getengine().redraw();

    gmsg<GM_DRAGNDROP>(DNDA_CLEAN).send();
}

void dragndrop_processor_c::update()
{
    if (!dndrect)
    {
        TSDEL(this);
        return;
    }
    if (dndobj)
        dndobj->update_dndobj(dndrect);
}

void dragndrop_processor_c::droped()
{
    if (dndobj)
    {
        gmsg<GM_DRAGNDROP>(DNDA_DROP).send();
    }
    TSDEL(this);
}

void dragndrop_processor_c::mm(const ts::ivec2& cursorpos)
{
    if (!dndrect)
    {
        TSDEL(this);
        return;
    }
    if (dndobj == nullptr && (cursorpos-clickpos).sqlen() > 25.0f)
    {
        dndobj = dndrect->summon_dndobj(cursorpos - clickpos);
        if (!dndobj)
        {
            TSDEL(this);
            return;
        }

        clickpos = cursorpos;
        clickpos_rel = dndobj->to_local(clickpos);
    }
    if (dndobj)
    {
        MODIFY(*dndobj).pos(cursorpos - clickpos_rel);
        bool isdropable = gmsg<GM_DRAGNDROP>(DNDA_DRAG).send().is(GMRBIT_ACCEPTED);
        if (dndobjdropable != isdropable)
        {
            dndobjdropable = isdropable;
            MODIFY(*dndobj).highlight(dndobjdropable);
            dndobj->getengine().redraw();
        }
    }
}

void gui_c::dragndrop_update( guirect_c *r )
{
    if (dndproc && (dndproc->underproc() == r || r == nullptr))
        dndproc->update();
}

void gui_c::dragndrop_lb( guirect_c *r )
{
    MEMT( MEMT_GUI_COMMON );

    if (dndproc) TSDEL(dndproc);
    dndproc = TSNEW(dragndrop_processor_c, r);
}

ts::irect gui_c::dragndrop_objrect()
{
    if (dndproc) return dndproc->rect();
    return ts::irect(0);
}


selectable_core_s::selectable_core_s()
{
    gui->register_kbd_callback( DELEGATE( this, copy_hotkey_handler ), ts::SSK_C, ts::casw_ctrl );
    gui->register_kbd_callback( DELEGATE( this, copy_hotkey_handler ), ts::SSK_INSERT, ts::casw_ctrl );
}

selectable_core_s::~selectable_core_s()
{
    if ( prev )
        prev->next = next;
    if ( next )
        next->prev = prev;

    clear_selection();

    if (gui)
    {
        gui->delete_event( DELEGATE( this, flash ) );
        gui->delete_event( DELEGATE( this, selectword ) );
        gui->unregister_kbd_callback( DELEGATE( this, copy_hotkey_handler ) );
    }
}

bool selectable_core_s::flash(RID, GUIPARAM p)
{
    flashing = as_int(p);
    if (flashing >= 100)
    {
        clear_selection_after_flashing = true;
        flashing = 4;
    }
    if (flashing > 0) DEFERRED_UNIQUE_CALL( 0.1, DELEGATE(this, flash), flashing-1 );
    if (clear_selection_after_flashing && flashing == 0)
        clear_selection();
    else if (owner) {
        dirty = true;
        owner->getengine().redraw();
    }
    return true;
}

ts::wstr_c selectable_core_s::get_selected_text()
{
    if (some_selected())
    {
        if (char_start_sel > char_end_sel) SWAP(char_start_sel, char_end_sel);
        int endi = char_end_sel;

        if (owner->get_text().get_char(endi - 1) == '<') //-V807
        {
            int closebr = owner->get_text().find_pos(endi, '>');
            if (closebr > endi && owner->get_text().substr(endi, closebr).begins(CONSTWSTR("char=")))
            {
                endi = closebr + 1;
            }
        }

        ts::str_c text = to_utf8(owner->get_text().substr(char_start_sel, endi));
        gui->app_prepare_text_for_copy(text);
        return from_utf8(text);
    }
    return ts::wstr_c();
}

ts::wstr_c selectable_core_s::get_selected_text_part_header()
{
    return owner->get_selected_text_part_header( prev ? prev->owner : nullptr );
}

void selectable_core_s::get_all_selected_labels( ts::tmp_pointers_t< gui_label_c, 0 >& ptrs )
{
    ptrs.clear();
    selectable_core_s *a = this;
    for ( ; a->prev; ) a = a->prev;
    for ( ; a; a = a->next )
        if ( a->some_selected() && a->owner )
            ptrs.add( a->owner );
}

ts::wstr_c selectable_core_s::get_all_selected_text( bool flash, bool insert_hdr_0 )
{
    selectable_core_s *a = this;
    for ( ; a->prev; ) a = a->prev;

    ts::wstr_c t;

    for ( ; a; a = a->next )
        if ( a->some_selected() )
        {
            if ( !t.is_empty() || insert_hdr_0 )
                t.append( a->get_selected_text_part_header() );
            t.append( a->get_selected_text() );
            if ( flash ) a->flash();
        }

    return t;
}

bool selectable_core_s::copy_hotkey_handler(RID, GUIPARAM)
{
    ts::wstr_c t( get_all_selected_text(true) );

    if (!t.is_empty())
    {
        ts::set_clipboard_text(t);
        return true;
    }
    return false;
}

ts::wchar selectable_core_s::ggetchar( int glyphindex )
{
    ts::GLYPHS &glyphs = owner->get_glyphs();

    if (glyphindex >= 0 && glyphindex < glyphs.count())
        if (glyphs.get(glyphindex).pixels == nullptr || glyphs.get(glyphindex).charindex < 0) return 0; // word break

    int chari = ts::glyphs_get_charindex(glyphs, glyphindex);
    if (chari >= 0 && chari < owner->get_text().get_length())
        return owner->get_text().get_char(chari);
    return 0;
}

void selectable_core_s::begin_from_start()
{
    glyph_under_cursor = -1;
    ts::GLYPHS &glyphs = owner->get_glyphs();

    glyph_start_sel = ts::glyphs_first_glyph( glyphs );
    glyph_end_sel = glyph_start_sel;

    char_start_sel = ts::glyphs_get_charindex( glyphs, glyph_start_sel );
    char_end_sel = ts::glyphs_get_charindex( glyphs, glyph_end_sel );
    dirty = true;
    owner->getengine().redraw();

}
void selectable_core_s::begin_from_end()
{
    glyph_under_cursor = -1;
    ts::GLYPHS &glyphs = owner->get_glyphs();

    glyph_start_sel = ts::glyphs_last_glyph( glyphs );
    glyph_end_sel = glyph_start_sel;

    char_start_sel = ts::glyphs_get_charindex( glyphs, glyph_start_sel );
    char_end_sel = ts::glyphs_get_charindex( glyphs, glyph_end_sel );
    dirty = true;
    owner->getengine().redraw();

}


void selectable_core_s::select_all()
{
    glyph_under_cursor = -1;
    ts::GLYPHS &glyphs = owner->get_glyphs();

    glyph_start_sel = ts::glyphs_first_glyph(glyphs);
    glyph_end_sel = ts::glyphs_last_glyph( glyphs );

    char_start_sel = ts::glyphs_get_charindex( glyphs, glyph_start_sel );
    char_end_sel = ts::glyphs_get_charindex( glyphs, glyph_end_sel );
    dirty = true;
    owner->getengine().redraw();
}

bool selectable_core_s::selectword(RID, GUIPARAM p)
{
    if (glyph_under_cursor < 0)
    {
        // current glyph not yet known. waiting it
        if (!p && owner) owner->getengine().redraw();
        dirty = true;
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, selectword), 1 );
        return true;
    }
    if (owner)
    {
        ts::GLYPHS &glyphs = owner->get_glyphs();
        int chari = ts::glyphs_get_charindex(glyphs,glyph_under_cursor);
        if (chari >= 0 && chari < owner->get_text().get_length())
        {
            int glyphs_start = 0;
            ts::aint glyph_count = glyphs.count();
            if (glyph_count && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
            {
                glyphs_start = 1;
                glyph_count = glyphs.get(0).outline_index;
            }

            glyph_end_sel = glyph_start_sel = glyph_under_cursor;
            for (; glyph_start_sel > glyphs_start && !IS_WORDB(ggetchar(glyph_start_sel - 1)); glyph_start_sel--);
            for (; glyph_end_sel < glyph_count && !IS_WORDB(ggetchar(glyph_end_sel)); glyph_end_sel++);

            char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
            char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
            dirty = true;
            owner->getengine().redraw();

        }
    }

    return true;
}

void selectable_core_s::select_by_charinds(gui_label_c *label, int char_start_sel_, int char_end_sel_)
{
    begin(label);
    ts::GLYPHS &glyphs = owner->get_glyphs();
    ts::aint cnt = glyphs.count();
    int glyphs_start = 0;
    if (cnt && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
    {
        glyphs_start = 1;
        cnt = glyphs.get(0).outline_index;
    }


    bool almost = false;
    bool altend = false;
    for(int i=glyphs_start;i<cnt;++i)
    {
        const ts::glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels >= GTYPE_DRAWABLE)
        {
            if (gi.charindex == char_start_sel_)
            {
                char_start_sel = char_start_sel_;
                glyph_start_sel = i;
            }
            if (gi.charindex == char_end_sel_ || ((glyph_end_sel < 0 || altend) && gi.charindex > char_end_sel_))
            {
                char_end_sel = gi.charindex;
                glyph_end_sel = i;
                if (altend) break;
            } else if (gi.charindex == char_end_sel_-1)
                almost = true;
            if (almost && gi.charindex < 0)
            {
                almost = false;
                char_end_sel = char_end_sel_;
                glyph_end_sel = i;
                altend = true;
            }
        }
        if (!altend && some_selected())
        {
            dirty = true;
            owner->getengine().redraw();
            return;
        }
    }
    if (altend && some_selected())
    {
        dirty = true;
        owner->getengine().redraw();
    }
}

void selectable_core_s::begin( gui_label_c *label )
{
    clear_selection();
    owner = label;
}

bool selectable_core_s::try_begin( gui_label_c *label )
{
    if (label == owner) return true;
    if (glyph_start_sel >= 0 && glyph_end_sel >= 0 && glyph_start_sel != glyph_end_sel) return false;
    begin(label);
    return true;
}

bool selectable_core_s::sure_selected()
{
    if (glyph_start_sel >= 0 && glyph_end_sel >= 0 &&
        char_start_sel >= 0 && char_end_sel >= 0)
    {
        ts::GLYPHS &glyphs = owner->get_glyphs();
        int new_char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
        int new_char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
        if (new_char_start_sel != char_start_sel || new_char_end_sel != char_end_sel)
        {
            // oops, glyphs array was changed
            glyph_start_sel = -1;
            glyph_end_sel = -1;
            char_start_sel = -1;
            char_end_sel = -1;
            clear_selection_after_flashing = false;

            gui->end_mousetrack(owner->getrid(), MTT_TEXTSELECT);
        }

        if (char_start_sel >= 0 && char_end_sel >= 0)
        {
            return true;
        }
    }
    return false;
}

void selectable_core_s::endtrack()
{
    selectable_core_s *a = this;
    for ( ; a->prev; ) a = a->prev;

    for ( ; a; a = a->next )
    {
        if ( a->glyph_start_sel > a->glyph_end_sel )
        {
            SWAP( a->glyph_start_sel, a->glyph_end_sel );
            SWAP( a->char_start_sel, a->char_end_sel );
        }
    }
}

void selectable_core_s::selection_stuff(ts::bitmap_c &texture, int yshift, const ts::ivec2 &size)
{
    texture.fill(ts::ivec2(0), size, 0);

    int isel0 = glyph_start_sel;
    int isel1 = glyph_end_sel;
    if (isel0 > isel1) SWAP(isel0, isel1);
    ts::GLYPHS &glyphs = owner->get_glyphs();
    if (isel0 >= glyphs.count()) return;

    ts::TSCOLOR selectionColor = gui->selection_color;
    ts::TSCOLOR selectionBgColor = gui->selection_bg_color;
    if (flashing & 1) selectionBgColor = gui->selection_bg_color_blink;
    for (int i = isel0; i < isel1; ++i)
    {
        ts::glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr || gi.charindex < 0) continue;
        if (gi.color) gi.color = selectionColor;
    }
    int y = 0;
    int h = 0;
    for (int i = isel0; i >= 0; --i)
    {
        const ts::glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr && gi.outline_index < 0)
        {
            y = gi.line_lt().y + 2;
            h = gi.line_rb().y - y + 3;
            break;
        }
    }

    ts::aint glyphc = glyphs.count();
    if (glyphc && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
        glyphc = glyphs.get(0).outline_index;

    for (int i = isel0; i < isel1; ++i)
    {
        const ts::glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr)
        {
            if (gi.outline_index < 0)
            {
                y = gi.line_lt().y + 2;
                h = gi.line_rb().y - y + 3;
            }
            continue;
        }
        if (gi.charindex < 0) continue;
        int w = gi.width;
        if (i < glyphc - 1)
        {
            ts::glyph_image_s &gi_next = glyphs.get(i + 1);
            if (gi_next.pixels == nullptr || gi_next.charindex < 0) w += 1;
            else w = gi_next.pos().x - gi.pos().x;
        }

        int yy = y - yshift;
        if ( yy >= size.y )
            break;

        if ( yy + h < 0 )
            continue;

        texture.fill(ts::ivec2(gi.pos().x, y-yshift), ts::ivec2(w, h), selectionBgColor);
    }
}

void selectable_core_s::clear_selection()
{
    if (glyph_start_sel >= 0 && glyph_end_sel >= 0 &&
        char_start_sel >= 0 && char_end_sel >= 0)
    {
        glyph_start_sel = -1;
        glyph_end_sel = -1;
        char_start_sel = -1;
        char_end_sel = -1;
        clear_selection_after_flashing = false;
        dirty = true;
        if (owner) owner->getengine().redraw();
        gui->delete_event(DELEGATE(this, selectword));
    }
}

ts::uint32 selectable_core_s::detect_area(const ts::ivec2 &pos)
{
    glyph_under_cursor = -1;
    ts::GLYPHS &glyphs = owner->get_glyphs();
    ts::irect gr = ts::glyphs_bound_rect(glyphs);
    gr += glyphs_pos;
    gr.lt.x -= 5; if (gr.lt.x < 0) gr.lt.x = 0;

    ts::ivec2 cp = pos - glyphs_pos;
    glyph_under_cursor = ts::glyphs_nearest_glyph(glyphs, cp);
    if (glyph_under_cursor >= 0)
    {
        const ts::glyph_image_s &gi = glyphs.get(glyph_under_cursor);
        if (cp.x >(gi.pos().x + gi.width / 2 + 2))
        {
            // looks like cursor near back of glyph
            ++glyph_under_cursor;
        }
    }

    if (gr.inside(pos))
        return AREA_EDITTEXT;
    return 0;
}

void selectable_core_s::track( bool self_only )
{
    if (glyph_under_cursor != glyph_end_sel)
    {
        if (glyph_start_sel < 0) glyph_start_sel = glyph_under_cursor;
        if (glyph_under_cursor >= 0) glyph_end_sel = glyph_under_cursor;
        if (glyph_start_sel >= 0 && glyph_end_sel >= 0)
        {
            ts::GLYPHS &glyphs = owner->get_glyphs();
            char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
            char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
            dirty = true;
            owner->getengine().redraw();
        }
    }

    if ( self_only ) return;

    if ( gui->get_mrealinside() == gui->get_hover() )
    {
        gui->crop_selcore( owner->getrid() );
    } else
    {
        ts::tmp_pointers_t< gui_label_c, 64 > search_up;
        ts::tmp_pointers_t< gui_label_c, 64 > search_dn;

        if ( gui_vscrollgroup_c *parent = dynamic_cast<gui_vscrollgroup_c *>( &HOLD( owner->getparent() )( ) ) )
        {
            ts::aint mei = parent->getengine().get_child_index( &owner->getengine() );
            ts::aint cnt = parent->getengine().children_count();
            bool still_search_up = true;
            bool still_search_dn = true;
            for ( ts::aint d = 1; still_search_up || still_search_dn; ++d )
            {
                if ( still_search_up )
                {
                    ts::aint index = mei - d;
                    if ( index < 0 )
                    {
                        still_search_up = false;
                        search_up.clear();

                    }
                    else
                    {
                        gui_label_c * l = dynamic_cast<gui_label_c *>( &parent->getengine().get_child( index )->getrect() );
                        if ( l && l->is_selectable() )
                        {
                            search_up.add( l );
                            if ( l->getrid() == gui->get_mrealinside() )
                            {
                                search_dn.clear();
                                break;
                            }
                        }
                    }
                }

                if ( still_search_dn )
                {
                    ts::aint index = mei + d;
                    if ( index >= cnt )
                    {
                        still_search_dn = false;
                        search_dn.clear();

                    }
                    else
                    {
                        gui_label_c * l = dynamic_cast<gui_label_c *>( &parent->getengine().get_child( index )->getrect() );
                        if ( l && l->is_selectable() )
                        {
                            search_dn.add( l );
                            if ( l->getrid() == gui->get_mrealinside() )
                            {
                                search_up.clear();
                                break;
                            }
                        }
                    }
                }
            }

            // dn
            if ( ts::aint scnt = search_dn.size() )
            {
                ts::GLYPHS &glyphs = owner->get_glyphs();
                glyph_end_sel = glyphs_last_glyph( glyphs );
                char_end_sel = ts::glyphs_get_charindex( glyphs, glyph_end_sel );
                dirty = true;

                selectable_core_s *scprev = this;

                for( ts::aint i = 0; i<scnt -1;++i )
                {
                    selectable_core_s &scn = gui->activate_selcore( search_dn.get(i), false );
                    if ( scn.prev != scprev )
                    {
                        ASSERT( scprev->next == nullptr );
                        scprev->next = &scn;
                        scn.prev = scprev;
                    } else
                    {
                        ASSERT( scn.prev == scprev && scprev->next == &scn );
                    }
                    scn.select_all();
                    scprev = &scn;
                }
                selectable_core_s &sclast = gui->activate_selcore( search_dn.last(), false );
                if ( sclast.prev != scprev )
                {
                    ASSERT( scprev->next == nullptr );
                    scprev->next = &sclast;
                    sclast.prev = scprev;

                } else
                {
                    ASSERT( sclast.prev == scprev && scprev->next == &sclast );
                    search_up.clear();
                    for( selectable_core_s *clearnext = sclast.next; clearnext; clearnext = clearnext->next )
                        search_up.add( clearnext->owner );

                    for ( gui_label_c *l : search_up )
                        gui->del_selcore( l->getrid() );

                    search_up.clear();
                }
                sclast.begin_from_start();
                sclast.detect_area( sclast.owner->to_local( ts::get_cursor_pos() ) );
                sclast.track( true );

            }

            if ( ts::aint scnt = search_up.size() )
            {
                ts::GLYPHS &glyphs = owner->get_glyphs();
                glyph_end_sel = glyphs_first_glyph( glyphs );
                char_end_sel = ts::glyphs_get_charindex( glyphs, glyph_end_sel );
                dirty = true;

                selectable_core_s *scprev = this;

                for ( ts::aint i = 0; i < scnt - 1; ++i )
                {
                    selectable_core_s &scn = gui->activate_selcore( search_up.get( i ), false );
                    if ( scn.next != scprev )
                    {
                        ASSERT( scprev->prev == nullptr );
                        scprev->prev = &scn;
                        scn.next = scprev;
                    }
                    else
                    {
                        ASSERT( scn.next == scprev && scprev->prev == &scn );
                    }
                    scn.select_all();
                    scprev = &scn;
                }
                selectable_core_s &sclast = gui->activate_selcore( search_up.last(), false );
                if ( sclast.next != scprev )
                {
                    ASSERT( scprev->prev == nullptr );
                    scprev->prev = &sclast;
                    sclast.next = scprev;

                }
                else
                {
                    ASSERT( sclast.next == scprev && scprev->prev == &sclast );
                    search_dn.clear();
                    for ( selectable_core_s *clearprev = sclast.prev; clearprev; clearprev = clearprev->prev )
                        search_dn.add( clearprev->owner );

                    for ( gui_label_c *l : search_dn )
                        gui->del_selcore( l->getrid() );

                    search_dn.clear();
                }
                sclast.begin_from_end();
                sclast.detect_area( sclast.owner->to_local( ts::get_cursor_pos() ) );
                sclast.track( true );

            }


        }

    }

    //DMSG("h" << glyph_under_cursor <<glyph_start_sel << glyph_end_sel << char_start_sel << char_end_sel );
    //DMSG( "seltrack" << gui->get_mrealinside() << gui->get_hover() );
}

static bool is_better_size( int needpixels, int current_area, int new_area )
{
    if ( current_area < needpixels && new_area >= needpixels )
        return true;

    if ( current_area >= needpixels && new_area < needpixels )
        return false;

    return ts::tabs( needpixels - current_area ) > ts::tabs( needpixels - new_area );
}

const ts::bitmap_c * gui_c::acquire_texture(text_rect_dynamic_c *requester, ts::ivec2 size)
{
    MEMT( MEMT_DYNAMIC_TEXTURE );

    texture_s *candidate = nullptr;

    static const int max_pool_size = 50;
    static const int max_height_of_one_texture = 512;
    static const int max_pixels_capacity = 1024 * 1024; // limit 1M data

    if ( size.y > max_height_of_one_texture )
        size.t = max_height_of_one_texture;

    int needpixels = size.x * size.y * 4;
    ASSERT( needpixels > 0 );
    if ( needpixels > max_pixels_capacity )
    {
        needpixels = max_pixels_capacity;
        size.y = max_pixels_capacity / 4 / size.x;
    }

    if (m_textures.size() < max_pool_size)
    {
        for (texture_s *tt : m_textures)
        {
            if (tt->owner.expired())
            {
                if ( candidate == nullptr || is_better_size( needpixels, candidate->pixels_capacity, tt->pixels_capacity ) )
                    candidate = tt;
            }
        }
    } else
    {
        ts::Time curt = ts::Time::current();
        int max_use_delta = -INT_MAX;

        auto better_time = [&](texture_s *newtt) ->bool
        {
            int d = ( curt - newtt->lastusetime );
            return (d - max_use_delta) > 5000; // assume better time is > 5 seconds
        };

        ts::tmp_pointers_t<texture_s, max_pool_size> postponed;

        for (texture_s *tt : m_textures)
        {
            if (tt->owner.expired())
            {
                if (candidate == nullptr ||
                    !candidate->owner.expired() ||
                    is_better_size( needpixels, candidate->pixels_capacity, tt->pixels_capacity ) )
                {
                    candidate = tt;
                    max_use_delta = -INT_MAX;
                }
            } else if ( requester != tt->owner )
            {
                if ( (curt - tt->lastusetime) < 5000 )
                {
                    postponed.add(tt); // last use was early 5 sec ago
                    continue; // skip it for now
                }

                if (candidate && candidate->owner.expired())
                    continue;

                if (candidate == nullptr || is_better_size(size, candidate->pixels_capacity, tt->pixels_capacity ) || better_time(tt))
                {
                    candidate = tt;
                    max_use_delta = curt - tt->lastusetime;
                }
            }
        }
        if (!candidate)
        {
            for (texture_s *tt : postponed)
            {
                ASSERT(!tt->owner.expired());

                if (candidate == nullptr || is_better_size(size, candidate->pixels_capacity, tt->pixels_capacity ) || better_time(tt))
                {
                    candidate = tt;
                    max_use_delta = curt - tt->lastusetime;
                }
            }
        }
    }

    if (!candidate)
    { 
        ASSERT(m_textures.size() < max_pool_size);
        candidate = TSNEW(texture_s);
        m_textures.add(candidate);

    } else if (!candidate->owner.expired())
    {
        candidate->owner->nomoretextures();
        text_rect_dynamic_c *dt = candidate->owner;
        for ( texture_s *tt : m_textures )
        {
            if ( tt->owner.get() == dt )
                tt->owner.unconnect();
        }
    }

    candidate->owner = requester;

    if ( candidate->pixels_capacity < needpixels )
    {
        candidate->create_ARGB( size );
        candidate->pixels_capacity = candidate->info().sz.y * candidate->info().pitch;
    } else
    {
        candidate->change_size( size );
    }

    return candidate;
}

void gui_c::release_texture( const ts::bitmap_c *bmp )
{
    for ( texture_s *tt : m_textures )
    {
        if ( tt == bmp )
        {
            tt->owner.unconnect();
            return;
        }
    }
}

void gui_c::release_textures( text_rect_dynamic_c *requester )
{
    for(texture_s *tt :m_textures)
    {
        if (tt->owner.get() == requester)
            tt->owner.unconnect();
    }
}

text_rect_dynamic_c::text_rect_dynamic_c()
{
    if (gui)
        default_color = gui->deftextcolor;
}

text_rect_dynamic_c::~text_rect_dynamic_c()
{
    if ( gui )
        gui->release_textures( this );
    curtextures.clear();
}

/*virtual*/ int text_rect_dynamic_c::prepare_textures( const ts::ivec2 &minsz )
{
    if ( int n = ok_size( minsz ) )
    {
        for ( ;n < curtextures.size(); )
        {
            ts::aint trnk = curtextures.size() - 1;
            gui->release_texture( curtextures.get( trnk ) );
            curtextures.truncate( trnk );
        }

        ts::Time curt = ts::Time::current();
        for ( const ts::bitmap_c *t : curtextures )
            gui_c::update_texture_time( t, curt );

        return n;
    }

    flags.set(F_INVALID_TEXTURE);

    gui->release_textures( this );
    curtextures.clear();

    for( int rqh = minsz.y; rqh > 0; )
    {
        const ts::bitmap_c *t = gui->acquire_texture( this, ts::ivec2(minsz.x, rqh) );
        curtextures.add(t);
        rqh -= t->info().sz.y;
    }
    return (int)curtextures.size();
}

/*virtual*/ int text_rect_dynamic_c::get_textures( const ts::bitmap_c **tarr )
{
    if (tarr)
        memcpy( tarr, curtextures.begin(), curtextures.size() * sizeof( ts::bitmap_c * ) );
    return (int)curtextures.size();
}


/*virtual*/ void text_rect_dynamic_c::textures_no_need()
{
    gui->release_textures( this );
    curtextures.clear();
}

void text_rect_dynamic_c::nomoretextures()
{
    curtextures.clear();
}
