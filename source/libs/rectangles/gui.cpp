#include "rectangles.h"

gui_c *gui = nullptr;

gui_c::tempbuf_c::tempbuf_c( double ttl )
{
    if ( ttl >= 0 )
        DELAY_CALL( ttl, DELEGATE(this, selfdie), nullptr );
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
    auto w = m_events.lock_write();

    for (int index = w().size() - 1; index >= 0; --index)
    {
        delay_event_c *de = ts::ptr_cast<delay_event_c *>(w().get(index).get());
        if ((*de) == h)
        {
            w().remove_fast(index); //<< this will kill hook
            de->die(); // and now we kill delay_event itself
        }
    }

}

bool gui_c::b_close(RID r, GUIPARAM param)
{
    app_b_close(RID::from_ptr(param));
    return true;
}
bool gui_c::b_maximize(RID r, GUIPARAM param)
{
    MODIFY(RID::from_ptr(param)).maximize(true);
    return true;
}
bool gui_c::b_minimize(RID r, GUIPARAM param)
{
    app_b_minimize(RID::from_ptr(param));
    return true;
}
bool gui_c::b_normalize(RID r, GUIPARAM param)
{
    MODIFY(RID::from_ptr(param)).decollapse().maximize(false);
    return true;
}

/*virtual*/ void gui_c::app_b_minimize(RID main)
{
    MODIFY(main).minimize(true);
}
/*virtual*/ void gui_c::app_b_close(RID)
{
    sys_exit(0);
}


gui_c::gui_c()
{
    deffont = CONSTASTR("default");
    gui = this;
    dirty_hover_data();
}

gui_c::~gui_c() 
{
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

    gui = nullptr;
}

void gui_c::load_theme( const ts::wsptr&thn )
{
    m_theme.load(thn);
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
    // раз в секунду проверим кое что
    for (RID rid : roots())
    {
        HOLD r(rid);
        r().prepare_test_00();
        r.engine().sq_evt(SQ_TEST_00, rid, ts::make_dummy<evt_data_s>(true));
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
    if (nullptr == GetFocus())
        HOLD( roots().last(RID()) ).engine().getroot()->set_system_focus();

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

DWORD gui_c::handler_SEV_BEFORE_INIT( const system_event_param_s & p )
{
	SetClassLong( g_sysconf.mainwindow, GCL_HICON, (LONG)app_icon(false) );
	g_sysconf.is_loop_in_background = true;
	return 0;
}

DWORD gui_c::handler_SEV_KEYBOARD(const system_event_param_s & p)
{
    ts::tmp_array_inplace_t<drawchecker, 4> dchs;
    for (RID r : roots())
        dchs.add() = HOLD(r).engine().getroot();

    if (RID f = gui->get_active_focus())
        if (allow_input(f))
        {

            evt_data_s d;
            d.kbd.scan = p.kbd.scan;
            d.kbd.charcode = 0;
            if (HOLD(f).engine().sq_evt(p.kbd.down ? SQ_KEYDOWN : SQ_KEYUP, f, d))
                return SRBIT_ACCEPTED;
        }

    if (p.kbd.down && GetKeyState(VK_CONTROL) < 0)
    {
        switch (p.kbd.scan)
        {
            case SSK_C:
            case SSK_INSERT:
                if (gmsg<GM_COPY_HOTKEY>().send().is(GMRBIT_ACCEPTED)) return 0;
                break;
        }
    }

    return 0;
}

DWORD gui_c::handler_SEV_WCHAR(const system_event_param_s & p)
{
    if (RID f = gui->get_active_focus())
        if (allow_input(f))
        {
            ts::tmp_array_inplace_t<drawchecker, 4> dchs;
            for (RID r : roots())
                dchs.add() = HOLD(r).engine().getroot();

            evt_data_s d;
            d.kbd.scan = 0;
            d.kbd.charcode = p.c;
            HOLD(f).engine().sq_evt(SQ_CHAR, f, d);
            return SRBIT_ACCEPTED;
        }
    return 0;
}

DWORD gui_c::handler_SEV_MOUSE(const system_event_param_s & p)
{
    if (p.mouse.message == WM_MOUSEWHEEL)
        if (RID f = gui->get_minside())
            if (allow_input(f))
            {
                ts::tmp_array_inplace_t<drawchecker, 4> dchs;
                for (RID r : roots())
                    dchs.add() = HOLD(r).engine().getroot();


                evt_data_s d;
                d.mouse.screenpos = ts::ref_cast<ts::ivec2>( p.mouse.pos );

                int rot = GET_WHEEL_DELTA_WPARAM(p.mouse.wp);

                int rot2 = lround((float)rot / (float)WHEEL_DELTA);
                if (rot2 == 0) rot2 = ts::isign(rot);
                if (rot2 > 10) rot2 = 10;
                if (rot2 < -10) rot2 = -10;

                rectengine_c &engine = HOLD(f).engine();
                for (; rot2 > 0; --rot2)
                    engine.sq_evt(SQ_MOUSE_WHEELUP, engine.getrid(), d);

                for (; rot2 < 0; ++rot2)
                    engine.sq_evt(SQ_MOUSE_WHEELDOWN, engine.getrid(), d);

                return SRBIT_ACCEPTED;
            }
    return 0;
}

DWORD gui_c::handler_SEV_LOOP( const system_event_param_s & p )
{
    ts::tmp_array_inplace_t<drawchecker, 4> dchs;
    for (RID r : roots())
        dchs.add() = HOLD(r).engine().getroot();

    int sleep = 1;

    ts::Time::updateForThread();
    m_frametime.takt();
    m_1second.takt(m_frametime.frame_time());
    if (m_1second.it_is_time_ones()) heartbeat();
    if (m_timer_processor.takt(m_frametime.frame_time())) sleep = 10;

    m_5seconds.takt(m_frametime.frame_time());
    if (m_5seconds.it_is_time_ones()) app_5second_event();

    gmsgbase *m;
    while (m_msgs.try_pop(m))
    {
        m->send();
        TSDEL(m);
    }

    dchs.clear();

    app_fix_sleep_value(sleep);

    if (sleep > 0) Sleep(sleep);
	return 0;
}

DWORD gui_c::handler_SEV_IDLE( const system_event_param_s & p )
{
    handler_SEV_LOOP(p);
	return 0;
}

ts::uint32 gui_c::gm_handler(gmsg<GM_ROOT_FOCUS>&p)
{
    if (p.pass > 0)
    {
        if (p.activefocus) set_focus(p.activefocus);
        return GMRBIT_ABORT;
    }
    return 0;
}


ts::ivec2 gui_c::textsize( const ts::asptr& font, const ts::wstr_c& text, int width_limit, int flags )
{
    ts::font_desc_c f;
    f.assign(font);
    return m_textrect.calc_text_size(f,text,width_limit < 0 ? 16384 : width_limit, flags);
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
        ReleaseCapture();

    } else
    {
        m_exclusive_input.find_remove_slow(r);
        if (RID rooti = m_roots.last(RID()))
        {
            ASSERT(allow_input(rooti));
            HOLD(rooti).engine().getroot()->update_foreground();
        }
    }
}

void gui_c::nomorerect(RID rid, bool isroot)
{
    if (isroot) m_roots.find_remove_fast(rid);
    bool dhd = false;
    if (rid == m_hoverdata.active_focus) { m_hoverdata.active_focus = RID(); dhd = true; }
    if (rid == m_hoverdata.focus) { m_hoverdata.focus = RID(); dhd = true; }
    if (rid == m_hoverdata.locked) { m_hoverdata.locked = RID(); dhd = true; }
    if (rid == m_hoverdata.minside) { m_hoverdata.minside = RID(); dhd = true; }
    if (rid == m_hoverdata.rid) { m_hoverdata.rid = RID(); dhd = true; }
    if (dhd) dirty_hover_data();
    if (isroot)
    {
        if (m_exclusive_input.find_remove_slow(rid) >= 0)
            if (RID rooti = m_roots.last(RID()))
            {
                ASSERT(allow_input(rooti));
                rectengine_root_c *root = HOLD(rooti).engine().getroot();
                gui->set_focus( root->getrid() );
                root->update_foreground();
            }
    }

    // обеспечиваем реюз RID-ов, т.к. RID по сути - индекс в массиве прямоугольников, а рост этого массива желательно минимизировать
    m_emptyids.add( ts::pair_s<ts::Time, RID>( ts::Time::current() + 1000, rid ) ); // выставляем разрешающее время на секунду вперед
}

void gui_c::resort_roots()
{
    if (m_roots.sort())
    {
        // надо установить родителей в соответствие zindex
        // этот способ работает в windows, т.к. родитель всегда отображается под потомком
        // TODO : ^^
        // есть еще заморочка с видимостью. невидимый рут-рект вообще не имеет hwnd окна
        // написание этой функции пока отложим due может быть вообще не понадобится
        STOPSTUB();
    }
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
        HOLD root(owner->getroot());

        const theme_rect_s *themerect = root().themerect();
        if (themerect)
        {
            int shiftleft = owner->getprops().size().x;

            switch (index)
            {
            case 0:
                if (pc && owner->getrid() == rid) // query for owner
                {
                    const rectprops_c &rootprops = root().getprops();
                    ts::irect ca = themerect->captionrect(rootprops.currentszrect(), rootprops.is_maximized());
                    ts::ivec2 capshift = (rootprops.is_maximized() || themerect->fastborder()) ? themerect->capbuttonsshift_max : themerect->capbuttonsshift;
                    MODIFY(*owner).pos(ca.rb.x - shiftleft + capshift.x, ca.lt.y + capshift.y);
                }
                break;
            default:
                if (c && owner->getrid() != rid) // query other
                {
                    bool _visible = true;
                    int tag = (int)ts::ptr_cast<gui_control_c *>(owner)->get_data();
                    switch (tag)
                    {
                    case ABT_CLOSE:
                        break;
                    case ABT_MAXIMIZE:
                        _visible = !root().getprops().is_maximized();
                        if (!_visible) shiftleft = 0;
                        break;
                    case ABT_MINIMIZE:
                        break;
                    case ABT_NORMALIZE:
                        _visible = root().getprops().is_maximized();
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

void gui_c::make_app_buttons(RID rootappwindow, ts::uint32 allowed_buttons, GET_TOOLTIP closebhint)
{
    bcreate_s buttons[] = { 
        {"close", DELEGATE( this, b_close ), ABT_CLOSE, DELEGATE( this, tt_close ) },
        {"maximize", DELEGATE( this, b_maximize ), ABT_MAXIMIZE, DELEGATE( this, tt_maximize ) },
        {"normalize", DELEGATE( this, b_normalize ), ABT_NORMALIZE, DELEGATE( this, tt_normalize ) },
        {"minimize", DELEGATE( this, b_minimize ), ABT_MINIMIZE, DELEGATE( this, tt_minimize ) },
    };
    if (closebhint) buttons[0].tooltip = closebhint;
    auto setupcustom = [this](bcreate_s &bcr, int tag) -> bcreate_s * {

        ASSERT(tag >= ABT_APPCUSTOM);
        bcr.tag = tag;
        bcr.face.s = nullptr;
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
        if (cbc->face.s == nullptr) break;

        gui_button_c &b = MAKE_CHILD<gui_button_c>(rootappwindow);
        b.tooltip(cbc->tooltip);
        b.set_face(cbc->face);
        b.set_handler(cbc->handler, rootappwindow.to_ptr());
        b.set_data( (GUIPARAM)cbc->tag );
        b.leech( TSNEW(auto_app_buttons, prev, i) );
        MODIFY(b).visible(true).size(b.get_min_size());
        prev = &b;
    }
}

const hover_data_s &gui_c::get_hoverdata( const ts::ivec2 & screenmousepos )
{
    if (m_hoverdata.locked)
    {
        m_hoverdata.rid = m_hoverdata.locked;
        HOLD r(m_hoverdata.rid);
        m_hoverdata.area = r().getengine().detect_area(r().to_local(screenmousepos));
        m_flags.clear(F_DIRTY_HOVER_DATA);
        return m_hoverdata;
    }

    if (!m_flags.is(F_DIRTY_HOVER_DATA) && screenmousepos == m_hoverdata.mp) return m_hoverdata;
    m_hoverdata.rid = RID();
    m_hoverdata.area = 0;
    m_hoverdata.mp = screenmousepos;
    
    for(RID rrid : m_roots)
        if (allow_input(rrid))
            if (guirect_c *rr = m_rects.get(rrid.index()))
                if (rr->getengine().detect_hover(screenmousepos))
                {
                    m_hoverdata.rid = rr->getengine().find_child_by_point(screenmousepos);
                    break;
                }
    if (m_hoverdata.rid)
    {
        HOLD r(m_hoverdata.rid);
        m_hoverdata.area = r().getengine().detect_area( r().to_local(screenmousepos) );
    }
    m_flags.clear(F_DIRTY_HOVER_DATA);
    return m_hoverdata;
}

void gui_c::set_focus(RID rid, bool force_active_focus)
{
    if (m_hoverdata.focus == rid) 
    {
        if (force_active_focus) m_hoverdata.active_focus = rid;
        if (rid)
        {
            rectengine_root_c * root = HOLD(rid).engine().getroot();
            if (root) root->set_system_focus();
        }
        return;
    }
    evt_data_s d;
    d.changed.focus = false;
    d.changed.is_active_focus = force_active_focus;
    if (m_hoverdata.focus)
    {
        RID old = m_hoverdata.focus;
        m_hoverdata.focus = RID();
        HOLD(old).engine().sq_evt(SQ_FOCUS_CHANGED, old, d);
        if (!rid || !HOLD(rid))
            return;
    }
    d.changed.focus = true;
    d.changed.is_active_focus = force_active_focus;
    m_hoverdata.focus = rid;
    HOLD(rid).engine().sq_evt(SQ_FOCUS_CHANGED, rid, d);
    if ( d.changed.is_active_focus )
        m_hoverdata.active_focus = rid;

    if (m_hoverdata.focus)
    {
        rectengine_root_c * root = HOLD(m_hoverdata.focus).engine().getroot();
        root->set_system_focus();
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
    d.mouse.screenpos = get_cursor_pos();

    m_hoverdata.minside = rid;

    DMSG( "in: "<< rid );

    r.engine().sq_evt(SQ_MOUSE_IN, rid, d);
    gui_tooltip_c::create(rid);
}

void gui_c::mouse_outside(RID rid)
{
    ASSERT( rid == m_hoverdata.minside || rid == RID() );
    if (m_hoverdata.minside == RID()) return;
    HOLD r(m_hoverdata.minside);

    evt_data_s d;
    d.mouse.screenpos = get_cursor_pos();

    m_hoverdata.minside = RID();
    r.engine().sq_evt(SQ_MOUSE_OUT, r().getrid(), d);
}


ts::ivec2 gui_c::get_cursor_pos() const
{
    ts::ivec2 cp;
    GetCursorPos(&ts::ref_cast<POINT>(cp));
    return cp;
}

selectable_core_s::~selectable_core_s()
{
    //gui->delete_event( DELEGATE(this,flash) );
}

bool selectable_core_s::flash(RID, GUIPARAM)
{
    --flashing;
    if (flashing > 0) DELAY_CALL_R( 0.1, DELEGATE(this, flash), nullptr );
    owner->getengine().redraw();
    return true;
}

ts::uint32 selectable_core_s::gm_handler(gmsg<GM_COPY_HOTKEY> &s)
{
    if (some_selected())
    {
        if (char_start_sel > char_end_sel) SWAP(char_start_sel, char_end_sel);
        int endi = char_end_sel;

        if ( owner->get_text().get_char(endi-1) == '<' )
        {
            int closebr = owner->get_text().find_pos(endi, '>');
            if (closebr > endi && owner->get_text().substr(endi,closebr).begins(CONSTWSTR("char=")))
            {
                endi = closebr +1;
            }
        }

        ts::wstr_c text = owner->get_text().substr(char_start_sel, endi);
        gui->app_prepare_text_for_copy( text );

        ts::set_clipboard_text(text);

        flashing = 3;
        DELAY_CALL_R( 0.1, DELEGATE(this, flash), nullptr );
        owner->getengine().redraw();
        return GMRBIT_ABORT|GMRBIT_ACCEPTED;
    }
    return 0;
}

ts::wchar selectable_core_s::ggetchar( int glyphindex )
{
    if (glyphindex >= 0 && glyphindex < glyphs.count())
        if (glyphs.get(glyphindex).pixels == nullptr || glyphs.get(glyphindex).charindex < 0) return 0; // word break

    int chari = ts::glyphs_get_charindex(glyphs, glyphindex);
    if (chari >= 0 && chari < owner->get_text().get_length())
        return owner->get_text().get_char(chari);
    return 0;
}

bool selectable_core_s::selectword(RID, GUIPARAM p)
{
    if (glyph_under_cursor < 0)
    {
        // current glyph not yet known. waiting it
        if (!p && owner) owner->getengine().redraw();
        DELAY_CALL_R( 0, DELEGATE(this, selectword), GUIPARAM(1) );
        return true;
    }
    if (owner)
    {
        int chari = ts::glyphs_get_charindex(glyphs,glyph_under_cursor);
        if (chari >= 0 && chari < owner->get_text().get_length())
        {
            glyph_end_sel = glyph_start_sel = glyph_under_cursor;
            for (; glyph_start_sel > 0 && !IS_WORDB(ggetchar(glyph_start_sel - 1)); glyph_start_sel--);
            for (; glyph_end_sel < glyphs.count() && !IS_WORDB(ggetchar(glyph_end_sel)); glyph_end_sel++);

            char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
            char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
            owner->getengine().redraw();

        }
    }

    return true;
}

void selectable_core_s::begin( gui_label_c *label )
{
    if (label == owner)
    {
    } else
    {
        if (owner && owner != label)
            clear_selection();
        owner = label;
        glyphs.clear();
    }
    glyph_under_cursor = -1;
    glyph_start_sel = -1 /*glyph_under_cursor*/;
    glyph_end_sel = -1 /*glyph_under_cursor*/;
    char_start_sel = -1;
    char_end_sel = -1;
    owner->getengine().redraw();
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
        int new_char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
        int new_char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
        if (new_char_start_sel != char_start_sel || new_char_end_sel != char_end_sel)
        {
            // oops, glyphs array was changed
            glyph_start_sel = -1;
            glyph_end_sel = -1;
            char_start_sel = -1;
            char_end_sel = -1;

            if (owner->getengine().mtrack(owner->getrid(), MTT_TEXTSELECT))
                owner->getengine().end_mousetrack(MTT_TEXTSELECT);

        }

        if (char_start_sel >= 0 && char_end_sel >= 0)
        {
            return true;
        }
    }
    return false;
}

void selectable_core_s::selection_stuff(ts::drawable_bitmap_c &texture)
{
    ts::text_rect_c &tr = gui->tr();
    texture.fill(ts::ivec2(0), tr.size, 0);

    int isel0 = glyph_start_sel;
    int isel1 = glyph_end_sel;
    if (isel0 > isel1) SWAP(isel0, isel1);
    if (isel0 >= glyphs.count()) return;

    ts::TSCOLOR selectionColor = ts::ARGB(255, 255, 0);
    ts::TSCOLOR selectionBgColor = ts::ARGB(100, 100, 255);
    if (flashing & 1) selectionBgColor = ts::ARGB(0, 0, 155);
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
            y = gi.line_lt.y + 2;
            h = gi.line_rb.y - y + 2;
            break;
        }
    }

    int glyphc = glyphs.count();
    if (glyphc && glyphs.get(0).pixels == nullptr && glyphs.get(0).outline_index > 0)
        glyphc = glyphs.get(0).outline_index;

    for (int i = isel0; i < isel1; ++i)
    {
        const ts::glyph_image_s &gi = glyphs.get(i);
        if (gi.pixels == nullptr)
        {
            if (gi.outline_index < 0)
            {
                y = gi.line_lt.y + 2;
                h = gi.line_rb.y - y + 2;
            }
            continue;
        }
        if (gi.charindex < 0) continue;
        int w = gi.width;
        if (i < glyphc - 1)
        {
            ts::glyph_image_s &gi_next = glyphs.get(i + 1);
            if (gi_next.pixels == nullptr || gi_next.charindex < 0) w += 1;
            else w = gi_next.pos.x - gi.pos.x;
        }
        texture.fill(ts::ivec2(gi.pos.x, y), ts::ivec2(w, h), selectionBgColor);
    }
}

void selectable_core_s::clear_selection()
{
    glyph_start_sel = -1;
    glyph_end_sel = -1;
    char_start_sel = -1;
    char_end_sel = -1;
    owner->getengine().redraw();
    gui->delete_event( DELEGATE(this, selectword) );
}

ts::uint32 selectable_core_s::detect_area(const ts::ivec2 &pos)
{
    glyph_under_cursor = -1;
    ts::irect gr = ts::glyphs_bound_rect(glyphs);
    gr += glyphs_pos;
    gr.lt.x -= 5; if (gr.lt.x < 0) gr.lt.x = 0;

    ts::ivec2 cp = pos - glyphs_pos;
    glyph_under_cursor = ts::glyphs_nearest_glyph(glyphs, cp);
    if (glyph_under_cursor >= 0)
    {
        const ts::glyph_image_s &gi = glyphs.get(glyph_under_cursor);
        if (cp.x >(gi.pos.x + gi.width / 2 + 2))
        {
            // looks like cursor near back of glyph
            ++glyph_under_cursor;
        }
    }

    if (gr.inside(pos))
        return AREA_EDITTEXT;
    return 0;
}

void selectable_core_s::track()
{
    if (glyph_under_cursor != glyph_end_sel)
    {
        if (glyph_start_sel < 0) glyph_start_sel = glyph_under_cursor;
        if (glyph_under_cursor >= 0) glyph_end_sel = glyph_under_cursor;
        if (glyph_start_sel >= 0 && glyph_end_sel >= 0)
        {
            char_start_sel = ts::glyphs_get_charindex(glyphs, glyph_start_sel);
            char_end_sel = ts::glyphs_get_charindex(glyphs, glyph_end_sel);
            owner->getengine().redraw();
        }
    }

    //DMSG("h" << glyph_under_cursor <<glyph_start_sel << glyph_end_sel << char_start_sel << char_end_sel );
}
