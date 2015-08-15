#include "rectangles.h"

bool RID::operator<(const RID&or) const
{
    HOLD me(*this);
    HOLD other(or);
    int x = ts::SIGN(me().getprops().zindex() - other().getprops().zindex());
    x = x == 0 ? (ts::SIGN((ts::aint)&me.engine() - (ts::aint)&other.engine())) : x;
    return x < 0;
}

bool RID::operator>(const RID&or) const
{
    return HOLD(or)().getparent() == *this;
}
bool RID::operator>>(const RID&or) const
{
    if (!or) return false;
    HOLD ror(or);
    if (!ror) return false;
    RID parent = ror().getparent();
    if (!parent) return false;
    if (parent == *this) return true;
    return *this >> parent;
}

ts::ivec3 RID::call_get_popup_menu_pos() const
{
    evt_data_s d;
    d.getsome.handled = false;
    HOLD ctl(*this);
    ctl().sq_evt( SQ_GET_POPUP_MENU_POS, *this, d );
    if (d.getsome.handled) return d.getsome.pos;

    return ts::ivec3( ctl().to_screen(ctl().getprops().szrect().rt()), ctl().getprops().size().x );
}

bool RID::call_is_tooltip() const
{
    evt_data_s d;
    d.getsome.handled = false;
    HOLD ctl(*this);
    ctl().sq_evt(SQ_IS_TOOLTIP, *this, d);
    return d.getsome.handled;
}

int  RID::call_get_menu_level() const
{
    evt_data_s d;
    d.getsome.handled = false;
    HOLD ctl(*this);
    ctl().sq_evt(SQ_GET_MENU_LEVEL, *this, d);
    return d.getsome.handled ? d.getsome.level : -1;
}

menu_c  RID::call_get_menu() const
{
    evt_data_s d;
    d.getsome.handled = false;
    HOLD ctl(*this);
    ctl().sq_evt(SQ_GET_MENU, *this, d);
    return d.getsome.handled ? *d.getsome.menu : menu_c();
}

void RID::call_restore_signal() const
{
    HOLD ctl(*this);
    ctl().sq_evt(SQ_RESTORE_SIGNAL, *this, ts::make_dummy<evt_data_s>(true));
}

void RID::call_setup_button(bcreate_s &bcr) const
{
    HOLD ctl(*this);
    evt_data_s d;
    d.btn = &bcr;
    ctl().sq_evt(SQ_BUTTON_SETUP, *this, d);
}

void RID::call_open_group(RID itm) const
{
    HOLD ctl(*this);
    evt_data_s d;
    d.rect.id = itm;
    ctl().sq_evt(SQ_OPEN_GROUP, *this, d);
}

void RID::call_enable(bool enableflg) const
{
    HOLD ctl(*this);
    evt_data_s d;
    d.flags.enableflag = enableflg;
    ctl().sq_evt(SQ_CTL_ENABLE, *this, d);
}

typedef ts::pair_s<ts::ivec2, RID> clickstruct;

void RID::call_lbclick(const ts::ivec2 &relpos) const
{

    int tag = gui->get_temp_buf(1.0, sizeof(clickstruct));
    clickstruct *cs = (clickstruct *)gui->lock_temp_buf(tag);
    if (!ASSERT(cs)) return;
    cs->first = relpos;
    cs->second = *this;

    DEFERRED_EXECUTION_BLOCK_BEGIN(0)

        if (clickstruct *s = (clickstruct *)gui->lock_temp_buf((int)param))
        {
            RID r = s->second;
            HOLD ctl(r);
            evt_data_s d;
            d.mouse.screenpos = ctl().getprops().screenpos() + s->first;
            d.mouse.allowchangecursor = false;

            const hover_data_s &hd = gui->get_hoverdata( d.mouse.screenpos );
            gui->dirty_hover_data();

            if (hd.rid)
            {
                HOLD x(hd.rid);
                x().sq_evt(SQ_MOUSE_LDOWN, hd.rid, d);
                x().sq_evt(SQ_MOUSE_LUP, hd.rid, d);
            }
        }

    DEFERRED_EXECUTION_BLOCK_END( (GUIPARAM)tag )
}

void RID::call_kill_child( const ts::str_c&param )
{
    HOLD ctl(*this);
    evt_data_s d;
    d.strparam = param.as_sptr();
    ctl().sq_evt(SQ_KILL_CHILD, *this, d);
}

void RID::call_item_activated( const ts::wstr_c&text, const ts::str_c&param )
{
    HOLD ctl(*this);
    evt_data_s d;
    d.item.text = text.as_sptr();
    d.item.param = param.as_sptr();
    ctl().sq_evt(SQ_ITEM_ACTIVATED, *this, d);
}

void RID::call_children_repos()
{
    DEFERRED_EXECUTION_BLOCK_BEGIN(0)
        
        RID grid = RID::from_ptr(param);
        HOLD ctl(grid);
        ctl().sq_evt(SQ_CHILDREN_REPOS, grid, ts::make_dummy<evt_data_s>(true));
        ctl().getengine().redraw();

    DEFERRED_EXECUTION_BLOCK_END( this->to_ptr() )
}

HOLD::HOLD(RID id)
{
    client = gui->get_rect(id);
}

HOLD::HOLD(guirect_c &r) :client(&r)
{
}

MODIFY::MODIFY(RID id)
{
	client = gui->get_rect(id);
	if (client)
    {
        change_to( client->getprops() );
        client->__spec_inmod(1);
    }
}

MODIFY::MODIFY(guirect_c &r):client(&r) 
{
	change_to( client->getprops() );
    client->__spec_inmod(1);
}

MODIFY::~MODIFY()
{
	if (client)
    {
        if (CHECK(client->__spec_inmod(0) == 1))
            client->apply(*this);
        client->__spec_inmod(-1);
    }
}

bool rectprops_c::change_to(const rectprops_c &p, rectengine_c *engine)
{
    ts::ivec2 oldsize = currentsize();
    evt_data_s evtd;

    // clamp size first
    evtd.rectchg.rect = p.rect();
    evtd.rectchg.area = 0;
    evtd.rectchg.apply = false;
    engine->sq_evt(SQ_RECT_CHANGING, engine->getrid(), evtd);

    bool zindex_changed = zindex() != p.zindex(); //-V550
    bool hl_changed = is_highlighted() != p.is_highlighted();
    bool ac_changed = is_active() != p.is_active();
    bool vis_changed = is_visible() != p.is_visible();
    ts::ivec2 posdelta = p.screenpos() - screenpos();
    
    change_to(p); m_size = evtd.rectchg.rect.get().size(); // change current property set

    // changed values for notifications
    evtd.changed.pos_changed = posdelta != ts::ivec2(0);
    evtd.changed.size_changed = currentsize() != oldsize;
    evtd.changed.width_changed = currentsize().x != oldsize.x;
    evtd.changed.is_visible = is_visible();
    evtd.changed.zindex = zindex_changed;
    evtd.changed.rect = rect();
    if (evtd.changed.pos_changed)
        engine->__spec_apply_screen_pos_delta_not_me(posdelta);
    if (evtd.changed.pos_changed || evtd.changed.size_changed)
    {
        engine->redraw();
        engine->sq_evt(SQ_RECT_CHANGED, engine->getrid(), evtd);
    }
    if (vis_changed)
        engine->sq_evt(SQ_VISIBILITY_CHANGED, engine->getrid(), evtd);
    if (zindex_changed)
        engine->sq_evt(SQ_ZINDEX_CHANGED, engine->getrid(), evtd);
    if (hl_changed || ac_changed)
        engine->getrect().sq_evt(SQ_THEMERECT_CHANGED, engine->getrid(), evtd);
        
    if (evtd.changed.pos_changed || evtd.changed.size_changed || vis_changed)
        gui->dirty_hover_data();

    return is_visible() && (ac_changed || hl_changed || vis_changed || evtd.changed.size_changed);
}

rectprops_c &rectprops_c::setminsize(RID r)
{
    return size(HOLD(r)().get_min_size());
}


autoparam_i::~autoparam_i()
{
    //if (owner)
    //    if (ASSERT(owner->autoparam == this)) owner->autoparam.reset();
}

#ifdef _DEBUG
void guirect_c::prepare_test_00()
{
    ASSERT(!m_test_00, "SQ_TEST_00 test failed for " << m_rid);
    m_test_00 = true;
    for(rectengine_c *e : getengine())
        if (e) e->getrect().prepare_test_00();

}
#endif

guirect_c::guirect_c(initial_rect_data_s &data):m_rid(data.id), m_parent(data.parent), m_engine(data.engine)
{
	if (ASSERT(m_engine))
		m_engine->set_controlled_rect( this );
    m_root = __spec_find_root();
}

guirect_c::~guirect_c() 
{
    make_all_ponters_expired();

    if (m_engine) m_engine->rect_ = nullptr;

    if (m_parent) if (auto h = HOLD(m_parent))
    {
        evt_data_s d;
        d.rect.id = getrid();
        d.rect.index = -1;
        h.engine().sq_evt(SQ_CHILD_DESTROYED, m_parent, d);
    }

    if (g_app)
        gui->nomorerect(getrid(), !m_parent);

    for (ts::safe_ptr<sqhandler_i> & sh : m_leeches)
        if (sqhandler_i *h = sh)
        {
            sh = nullptr;
            h->i_unleeched();
        }
}

void guirect_c::created()
{
    if (m_parent)
    {
        evt_data_s d;
        d.rect.id = getrid();
        HOLD(m_parent).engine().sq_evt(SQ_CHILD_CREATED, m_parent, d);
    }
    DEBUGCODE( m_test_01 = true; )
}

void guirect_c::leech(sqhandler_i * h)
{
    for (int i = m_leeches.size() - 1; i >= 0; --i)
    {
        sqhandler_i * hs = m_leeches.get(i).get();
        if (hs == h) return; // already subscribed
        if (!hs)
        {
            m_leeches.remove_fast(i);
            continue;
        }
    }
    m_leeches.add() = h;
    h->i_leeched(*this);
}
void guirect_c::unleech(sqhandler_i * h)
{
    for (int i = m_leeches.size() - 1; i >= 0; --i)
    {
        // full loop
        sqhandler_i * hs = m_leeches.get(i).get();
        if (hs == h)
        {
            m_leeches.remove_fast(i);
            h->i_unleeched();

        } else if (!hs)
            m_leeches.remove_fast(i);
    }
}

void guirect_c::apply(const rectprops_c &pss)
{
	if (ASSERT(m_engine))
		if (m_engine->apply( m_props, pss ))
            m_engine->redraw();
}

/*virtual*/ bool guirect_c::sq_evt(system_query_e qp, RID _rid, evt_data_s &data)
{
#ifdef _DEBUG
    if (qp == SQ_TEST_00)
    {
        for (rectengine_c *e : getengine())
            if (e) e->sq_evt(qp, e->getrid(), data);
        return (m_test_00 = false);
    }
#endif
    for (sqhandler_i *h : m_leeches)
        if (h)
            h->sq_evt(qp, _rid, data);
    return false;
}


/*virtual*/ ts::ivec2 guirect_c::get_min_size() const
{
    if (const theme_rect_s *thr = themerect())
        return thr->size_by_clientsize(thr->minsize, getprops().is_maximized());
    return ts::ivec2(0);
}
/*virtual*/ ts::ivec2 guirect_c::get_max_size() const
{
    //if (const theme_rect_s *thr = themerect())
    //    return thr->size_by_clientsize(ts::ivec2(maximum<ts::int16>::value));
    return ts::ivec2(maximum<ts::int16>::value);
}

//________________________________________________________________________________________________________________________________ gui control

/*virtual*/ gui_control_c::~gui_control_c()
{
    if (!customdatakiller.empty())
        customdatakiller(getrid(), customdata);

    if (popupmenu)
        TSDEL(popupmenu);
}

void gui_control_c::created()
{
    if (const theme_rect_s *tr = themerect())
        if (!is_root() || tr->rootalphablend())
            if (tr->is_alphablend(SI_any))
                MODIFY(*this).makealphablend();

    __super::created();
}

/*virtual*/ bool gui_control_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    switch (qp)
    {
    case SQ_CTL_ENABLE:
        enable( data.flags.enableflag );
        return true;
    case SQ_DRAW:
        if (defaultthrdraw)
        {
            if (const theme_rect_s *thr = themerect())
                if (ASSERT(m_engine))
                {
                    evt_data_s dd;
                    if (DTHRO_BASE_HOLE & defaultthrdraw)
                    {
                        dd.draw_thr.rect = thr->clientrect(getprops().currentsize(), getprops().is_maximized());
                        ASSERT(0 == (defaultthrdraw & DTHRO_CENTER_HOLE) );
                    }
                    if (DTHRO_CENTER_HOLE & defaultthrdraw)
                    {
                        ASSERT(0 == (defaultthrdraw & DTHRO_BASE_HOLE));
                        dd.draw_thr.rect = get_client_area();
                        dd.draw_thr.rect().lt += thr->hollowborder.lt;
                        dd.draw_thr.rect().rb -= thr->hollowborder.rb;
                    }

                    m_engine->draw(*thr, defaultthrdraw, &dd);
                }
        }
    }

    return __super::sq_evt(qp, rid, data);
}

void gui_control_c::set_theme_rect( const ts::asptr &thrn, bool ajust_defdraw )
{
    if (thrcache.set_themerect(thrn))
    {
        getengine().redraw();

        if (ajust_defdraw)
        {
            const theme_rect_s *thrmine;
            if (!is_root() && nullptr != (thrmine = themerect()) && thrmine->sis[SI_BASE] &&
                (
                thrmine->is_alphablend(SI_LEFT) ||
                thrmine->is_alphablend(SI_RIGHT) ||
                thrmine->is_alphablend(SI_TOP) ||
                thrmine->is_alphablend(SI_BOTTOM) ||
                thrmine->is_alphablend(SI_LEFT_TOP) ||
                thrmine->is_alphablend(SI_RIGHT_TOP) ||
                thrmine->is_alphablend(SI_LEFT_BOTTOM) ||
                thrmine->is_alphablend(SI_RIGHT_BOTTOM)
                ))
            {
                SETFLAG( defaultthrdraw, DTHRO_BASE_HOLE );
            }
            else
                RESETFLAG( defaultthrdraw, DTHRO_BASE_HOLE );
        }
        sq_evt( SQ_THEMERECT_CHANGED, getrid(), ts::make_dummy<evt_data_s>(true) );
    }
}

void gui_control_c::disable(bool f)
{
    if (flags.is(F_DISABLED) != f)
    {
        flags.init(F_DISABLED, f);
        getengine().redraw();
    }
}


//________________________________________________________________________________________________________________________________ gui stub

MAKE_CHILD<gui_stub_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}

//________________________________________________________________________________________________________________________________ gui button


bool gui_button_c::default_handler(RID r, GUIPARAM)
{
    WARNING( __FUNCTION__ " called" );
    return false;
}

bool gui_button_c::group_handler(gmsg<GM_GROUP_SIGNAL> & signal)
{
    if (signal.tag == grouptag)
    {
        bool om = flags.is(F_MARK);

        if (flags.is(F_RADIOBUTTON))
        {
            flags.init(F_MARK, signal.sender == getrid());

        } else if (flags.is(F_CHECKBUTTON))
        {
            if (signal.sender == getrid())
                flags.invert(F_MARK);
            if (flags.is(F_MARK))
                signal.mask |= (ts::uint32)param;
        }

        if (flags.is(F_MARK) != om)
            getengine().redraw();
    }
    return true;
}

void gui_button_c::draw()
{
    if (ASSERT(m_engine && desc))
    {
        button_desc_s::states drawstate = curstate;
        if (flags.is(F_RADIOBUTTON|F_CHECKBUTTON) && flags.is(F_MARK))
            drawstate = button_desc_s::PRESS;
        else if (flags.is(F_DISABLED))
            drawstate = button_desc_s::DISABLED;

        ts::ivec2 sz = getprops().size();

        if (desc->rectsf[drawstate])
        {
            m_engine->draw(*desc->rectsf[drawstate], DTHRO_BORDER | DTHRO_CENTER);
        } else
        {
            if (flags.is(F_DISABLED_USE_ALPHA) && flags.is(F_DISABLED))
            {
                draw_data_s &dd = m_engine->begin_draw();
                dd.alpha = 128;
            }
            int y = (sz.y - desc->rects[drawstate].height()) / 2;
            m_engine->draw(ts::ivec2(0, y), desc->src, desc->rects[drawstate], desc->is_alphablend(drawstate));
            
            if (flags.is(F_DISABLED_USE_ALPHA) && flags.is(F_DISABLED)) 
                m_engine->end_draw();
        }

        if (flags.is(F_RADIOBUTTON|F_CHECKBUTTON))
        {
            text_draw_params_s tdp;
            draw_data_s &dd = m_engine->begin_draw();
            if (flags.is(F_DISABLED_USE_ALPHA) && flags.is(F_DISABLED)) dd.alpha = 128;
            dd.offset.x += desc->rects[drawstate].width();
            dd.size = sz;
            tdp.forecolor = desc->colors + drawstate;
            ts::flags32_s f; f.set(ts::TO_VCENTER);
            tdp.textoptions = &f;
            tdp.rectupdate = updaterect;
            if (dd.size >> 0) m_engine->draw(text, tdp);
            m_engine->end_draw();
        } else if (!text.is_empty())
        {
            draw_data_s &dd = m_engine->begin_draw();
            if (flags.is(F_DISABLED_USE_ALPHA) && flags.is(F_DISABLED)) dd.alpha = 128;
            if (desc->rectsf[drawstate])
            {
                ts::irect clar = desc->rectsf[drawstate]->clientrect(sz, false /*buttn will never be maximized, i hope*/ );
                dd.offset += clar.lt;
                dd.size = clar.size();
            } else
            {
                dd.size = sz;
            }
            
            text_draw_params_s tdp;
            tdp.forecolor = desc->colors + drawstate;
            ts::flags32_s f; f.set(ts::TO_VCENTER | ts::TO_HCENTER);
            tdp.textoptions = &f;
            tdp.rectupdate = updaterect;
            if (dd.size >> 0) m_engine->draw(text, tdp);
            m_engine->end_draw();
        }
    }
}

void gui_button_c::update_textsize()
{
    if (!flags.is(F_TEXTSIZEACTUAL) && !text.is_empty() && ASSERT(font))
    {
        int w = getprops().size().x;
        textsize = gui->textsize(get_font(), text, w ? w : -1);
        flags.set(F_TEXTSIZEACTUAL);
    }
}

int gui_button_c::get_ctl_width()
{
    update_textsize();
    return get_min_size().x;
}

ts::uint32 gui_button_c::gm_handler(gmsg<GM_UI_EVENT> & e)
{
    if (UE_THEMECHANGED == e.evt)
    {
        if (face_getter)
            set_face(face_getter());
    }

    return 0;
}

void gui_button_c::set_face( button_desc_s *bdesc )
{
    desc = bdesc;
    if (!desc->text.is_empty())
        set_text(desc->text);

    flags.init(F_DISABLED_USE_ALPHA, desc->rects[button_desc_s::NORMAL] == desc->rects[button_desc_s::DISABLED]);
}

/*virtual*/ ts::ivec2 gui_button_c::get_min_size() const
{
    if (flags.is(F_RADIOBUTTON|F_CHECKBUTTON))
    {
        ts::ivec2 sz = desc->rects[ curstate ].size();
        const_cast<gui_button_c *>(this)->update_textsize();
        if (textsize.y > sz.y) sz.y = textsize.y;
        return sz;
    }

    if (flags.is(F_CONSTANT_SIZE_X) && flags.is(F_CONSTANT_SIZE_Y))
        return textsize;

    if (desc)
    {
        if (desc->rectsf[curstate])
        {

            if (flags.is(F_CONSTANT_SIZE_X) || flags.is(F_CONSTANT_SIZE_Y))
            {
                ts::ivec2 storesz = textsize;
                gui_button_c *me = const_cast<gui_button_c *>(this);
                me->update_textsize();
                if (flags.is(F_CONSTANT_SIZE_X)) me->textsize.x = storesz.x;
                if (flags.is(F_CONSTANT_SIZE_Y)) me->textsize.y = storesz.y;
                storesz = desc->rectsf[curstate]->size_by_clientsize( ts::ivec2(textsize), false );
                if (flags.is(F_CONSTANT_SIZE_X)) storesz.x = textsize.x;
                if (flags.is(F_CONSTANT_SIZE_Y)) storesz.y = textsize.y;
                return storesz;
            }

            const_cast<gui_button_c *>(this)->update_textsize();
            return desc->rectsf[curstate]->size_by_clientsize( ts::ivec2(textsize), false );
        }
        ts::ivec2 rtnr = desc->rects[curstate].size();
        if (flags.is(F_CONSTANT_SIZE_X)) rtnr.x = textsize.x;
        if (flags.is(F_CONSTANT_SIZE_Y)) rtnr.y = textsize.y;
        return rtnr;
    }
    
    ts::ivec2 rtnr = __super::get_min_size();
    if (flags.is(F_CONSTANT_SIZE_X)) rtnr.x = textsize.x;
    if (flags.is(F_CONSTANT_SIZE_Y)) rtnr.y = textsize.y;
    return rtnr;
}
/*virtual*/ ts::ivec2 gui_button_c::get_max_size() const
{
    if (flags.is(F_RADIOBUTTON|F_CHECKBUTTON))
    {
        ts::ivec2 sz = desc->rects[curstate].size();
        if (!text.is_empty())
        {
            int w = getprops().size().x;
            ts::ivec2 szt = gui->textsize(get_font(), text, w ? w : -1);
            if (szt.y > sz.y) sz.y = szt.y;
            if (flags.is(F_LIMIT_MAX_SIZE))
            {
                sz.x = szt.x;
                if (const theme_rect_s *thr = themerect())
                    sz = thr->size_by_clientsize(sz, false);
            } else
                sz.x = maximum<ts::int16>::value;
        }
        return sz;
    }

    if (flags.is(F_CONSTANT_SIZE_X) && flags.is(F_CONSTANT_SIZE_Y))
        return textsize;

    if (desc)
    {
        if (desc->rectsf[curstate])
        {
            if (flags.is(F_LIMIT_MAX_SIZE))
                return get_min_size();

            if (flags.is(F_CONSTANT_SIZE_Y))
            {
                ts::ivec2 sz = __super::get_max_size();
                sz.y = get_min_size().y;
                return sz;
            }

            return __super::get_max_size();
        }
        return desc->rects[curstate].size();
    }

    return __super::get_max_size();
}

/*virtual*/ bool gui_button_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    if (__super::sq_evt(qp,rid,data)) return true;

    bool action = false;

    switch(qp)
    {
    case SQ_DRAW:
        draw();
        return true;
    case SQ_MOUSE_MOVE:
        if (flags.is(F_PRESS))
        {
            button_desc_s::states ost = curstate;
            if (getprops().screenrect().inside(data.mouse.screenpos))
                curstate = button_desc_s::PRESS;
            else
                curstate = button_desc_s::NORMAL;
            if (ost != curstate)
                getengine().redraw();
        }
        break;
    case SQ_MOUSE_IN:
        if (!flags.is(F_DISABLED))
        {
            curstate = button_desc_s::HOVER;
            getengine().redraw();
        }
        break;
    case SQ_MOUSE_OUT:
        if (!flags.is(F_DISABLED))
        {
            curstate = button_desc_s::NORMAL;
            getengine().redraw();
        }
        break;
    case SQ_MOUSE_LDOWN:
        if (!flags.is(F_DISABLED))
        {
            curstate = button_desc_s::PRESS;
            getengine().redraw();
            getengine().mouse_lock();
            flags.set(F_PRESS);
            gmsg<GM_KILL_TOOLTIPS>().send();
        }
        break;
    case SQ_MOUSE_LUP:
        if (!flags.is(F_DISABLED) && flags.is(F_PRESS))
        {
            if (getprops().screenrect().inside(data.mouse.screenpos))
            {
                curstate = button_desc_s::HOVER;
                getengine().redraw();
                action = true;
            }
            getengine().mouse_unlock();
            flags.clear(F_PRESS);
        }
        break;
    case SQ_BUTTON_SETUP:
        handler = data.btn->handler;
        if (data.btn->tooltip) tooltip(data.btn->tooltip);
        break;
    }

    if (action)
    {
        push();
    }

    return false;
}

void gui_button_c::push()
{
    if (flags.is(F_CHECKBUTTON))
    {
        gmsg<GM_GROUP_SIGNAL> s(getrid(), grouptag);
        s.send();
        CHECK(handler && handler(getrid(), (GUIPARAM)s.mask));
        return;
    }

    if (flags.is(F_RADIOBUTTON))
        gmsg<GM_GROUP_SIGNAL>( getrid(), grouptag ).send();
    CHECK(handler && handler(getrid(), param));
}

//________________________________________________________________________________________________________________________________ gui label


void gui_label_c::set_selectable(bool f)
{
    bool oldf = flags.is(FLAGS_SELECTABLE);
    flags.init(FLAGS_SELECTABLE, f);
    if (oldf != f) getengine().redraw();
}

void gui_label_c::draw()
{
    if (ASSERT(m_engine) && !textrect.get_text().is_empty())
    {
        ts::irect ca = get_client_area();
        draw_data_s &dd = getengine().begin_draw();

        if (flags.is(FLAGS_SELECTABLE) && gui->selcore().owner == this)
        {
            selectable_core_s &selcore = gui->selcore();
            selcore.glyphs_pos = ca.lt;
        }
        dd.offset += ca.lt;
        dd.size = ca.size();

        text_draw_params_s tdp;
        tdp.rectupdate = updaterect;
        draw(dd, tdp);
        getengine().end_draw();
    }
}

void gui_label_c::draw( draw_data_s &dd, const text_draw_params_s &tdp )
{
    if (dd.size >> 0)
    {
    } else
        return;

    if (textrect.is_dirty_size()) textrect.set_size(dd.size);
    if (tdp.font) textrect.set_font(tdp.font);
    if (tdp.textoptions) textrect.set_options(*tdp.textoptions);
    if (tdp.forecolor) textrect.set_def_color(*tdp.forecolor);
    bool do_updr = true;
    if (flags.is(FLAGS_SELECTABLE) && gui->selcore().owner == this)
    {
        flags.set(FLAGS_SELECTION);
        selectable_core_s &selcore = gui->selcore();

        if (gui->selcore().is_dirty() || textrect.is_dirty())
        {
            do_updr = false;
            textrect.parse_and_render_texture(nullptr, custom_tag_parser_delegate(), false); // it changes glyphs array
            bool still_selected = selcore.sure_selected();

            if (tdp.rectupdate)
            {
                ts::rectangle_update_s updr;
                updr.updrect = tdp.rectupdate;
                updr.offset = dd.offset;
                updr.param = getrid().to_ptr();
                if (still_selected)
                    textrect.render_texture(&updr, DELEGATE(&selcore, selection_stuff));
                else
                    textrect.render_texture(&updr);
            }
            else
            {
                if (still_selected)
                    textrect.render_texture(nullptr, DELEGATE(&selcore, selection_stuff));
                else
                    textrect.render_texture(nullptr);
            }
        }


    } else if (textrect.is_dirty() || flags.is(FLAGS_SELECTION))
    {
        flags.clear(FLAGS_SELECTION);
        textrect.parse_and_render_texture(nullptr, custom_tag_parser_delegate(), false); // it changes glyphs array
        do_updr = false;
        if (tdp.rectupdate)
        {
            ts::rectangle_update_s updr;
            updr.updrect = tdp.rectupdate;
            updr.offset = dd.offset;
            updr.param = getrid().to_ptr();
            textrect.render_texture(&updr);
        } else
        {
            textrect.render_texture(nullptr);
        }
    }

    m_engine->draw(ts::ivec2(0), textrect.get_texture(), ts::irect(ts::ivec2(0), tmin(dd.size, textrect.size)), true);
    if (do_updr && tdp.rectupdate)
    {
        ts::rectangle_update_s updr;
        updr.updrect = tdp.rectupdate;
        updr.offset = dd.offset;
        updr.param = getrid().to_ptr();
        textrect.update_rectangles(&updr);
    }
}

void gui_label_c::set_text(const ts::wstr_c&_text)
{
    if (textrect.set_text(_text,custom_tag_parser_delegate(),false))
        getengine().redraw();
}

void gui_label_c::set_font(const ts::font_desc_c *f)
{
    if (textrect.set_font(f))
        getengine().redraw();
}

/*virtual*/ int gui_label_c::get_height_by_width(int width) const
{
    if (flags.is(FLAGS_AUTO_HEIGHT) && !textrect.get_text().is_empty())
        return textrect.calc_text_size(width, custom_tag_parser_delegate()).y;
    return 0;
}

/*virtual*/ ts::ivec2 gui_label_c::get_min_size() const
{
    ts::ivec2 sz = __super::get_min_size();
    if (flags.is(FLAGS_AUTO_HEIGHT) && !textrect.get_text().is_empty())
    {
        int w = getprops().size().x;
        ts::ivec2 szt = textrect.calc_text_size( w ? w : -1, custom_tag_parser_delegate() );
        sz.y = szt.y;
    }
    return sz;
}

/*virtual*/ ts::ivec2 gui_label_c::get_max_size() const
{
    ts::ivec2 sz = __super::get_max_size();
    if (flags.is(FLAGS_AUTO_HEIGHT) && !textrect.get_text().is_empty())
    {
        int w = getprops().size().x;
        ts::ivec2 szt = textrect.calc_text_size( w ? w : -1, custom_tag_parser_delegate() );
        sz.y = szt.y;
    }
    return sz;
}

/*virtual*/ bool gui_label_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    switch (qp)
    {
    case SQ_DRAW:
        draw();
        return true;
    case SQ_MOUSE_L2CLICK:
        if (flags.is(FLAGS_SELECTABLE))
        {
            ts::ivec2 mplocal = to_local(data.mouse.screenpos);
            ts::irect clar = get_client_area();
            if (clar.inside(mplocal))
                gui->selcore().selectword(RID(),nullptr);
        }
        break;
    case SQ_MOUSE_LDOWN:
        if (flags.is(FLAGS_SELECTABLE))
        {
            ts::ivec2 mplocal = to_local(data.mouse.screenpos);
            ts::irect clar = get_client_area();
            if (clar.inside(mplocal))
            {
                gui->selcore().begin( this );

                gui->begin_mousetrack(getrid(), MTT_TEXTSELECT);
                gui->set_focus(getrid());
                getengine().redraw();
                break;
            }
        }
        break;
    case SQ_MOUSE_LUP:
        if (gui->end_mousetrack(getrid(), MTT_TEXTSELECT))
            gui->selcore().endtrack();
        break;
    case SQ_MOUSE_MOVE_OP:
        if (gui->mtrack(getrid(), MTT_TEXTSELECT))
            if (flags.is(FLAGS_SELECTABLE) && gui->selcore().owner == this)
                gui->selcore().track();
        break;
    case SQ_DETECT_AREA:
        if (data.detectarea.area == 0 && flags.is(FLAGS_SELECTABLE))
        {
            if (gui->selcore().try_begin(this))
                data.detectarea.area = gui->selcore().detect_area(data.detectarea.pos);
            else data.detectarea.area = AREA_EDITTEXT;
        }
        return true;
    case SQ_RECT_CHANGED:
        textrect.set_size( get_client_area().size() );
        return true;
    case SQ_THEMERECT_CHANGED:

        set_defcolor(get_default_text_color());
        if (const theme_rect_s *tr = themerect())
            set_font(tr->deffont);
        else
            set_font(nullptr);

        return true;
    }
    return false;
}




//________________________________________________________________________________________________________________________________ gui label ex

static int detect_link(const ts::wstr_c &message, int chari)
{
    int overlink = -1;

    int e = message.find_pos(chari, CONSTWSTR("<cstm=b"));
    if (e > 0)
    {
        int s = message.substr(0, chari).find_last_pos(CONSTWSTR("<cstm=a"));
        if (s >= 0)
        {
            int ne = message.as_num_part(-1, e + 7);
            if (ne >= 0 && ne == message.as_num_part(-1, s + 7))
                overlink = ne;
        }
    }

    return overlink;
}

static ts::ivec2 extract_link(const ts::wstr_c &message, int chari)
{
    int e = message.find_pos(chari, CONSTWSTR("<cstm=b"));
    if (e > 0)
    {
        int s = message.substr(0, chari).find_last_pos(CONSTWSTR("<cstm=a"));
        if (s >= 0)
        {
            int ne = message.as_num_part(-1, e + 7);
            if (ne >= 0 && ne == message.as_num_part(-1, s + 7))
            {
                s = message.find_pos(s + 7, '>') + 1;
                return ts::ivec2(s, e);
            }
        }
    }
    return ts::ivec2(-1);
}

bool gui_label_ex_c::check_overlink(const ts::ivec2 &pos)
{
    if (textrect.is_dirty_glyphs()) return false;
    ts::irect clar = get_client_area();
    bool set = false;
    if (clar.inside(pos))
    {
        ts::GLYPHS &glyphs = get_glyphs();
        ts::irect gr = ts::glyphs_bound_rect(glyphs);
        gr += glyphs_pos;
        if (gr.inside(pos))
        {
            ts::ivec2 cp = pos - glyphs_pos;
            int glyph_under_cursor = ts::glyphs_nearest_glyph(glyphs, cp, true);
            int char_index = ts::glyphs_get_charindex(glyphs, glyph_under_cursor);
            if (char_index >= 0 && char_index < textrect.get_text().get_length())
            {
                set = true;
                int boverlink = overlink;
                overlink = detect_link(textrect.get_text(), char_index);
                if (overlink != boverlink)
                {
                    textrect.make_dirty();
                    getengine().redraw();
                }
            }
        }

    }

    if (!set && overlink >= 0)
    {
        overlink = -1;
        textrect.make_dirty();
        getengine().redraw();
    }

    return overlink >= 0;
}

ts::ivec2 gui_label_ex_c::get_link_pos_under_cursor(const ts::ivec2 &localpos) const
{
    if (textrect.is_dirty_glyphs()) return ts::ivec2(-1);
    ts::irect clar = get_client_area();
    if (clar.inside(localpos))
    {
        const ts::GLYPHS &glyphs = get_glyphs();
        ts::irect gr = ts::glyphs_bound_rect(glyphs);
        gr += glyphs_pos;
        if (gr.inside(localpos))
        {
            ts::ivec2 cp = localpos - glyphs_pos;
            int glyph_under_cursor = ts::glyphs_nearest_glyph(glyphs, cp, true);
            int char_index = ts::glyphs_get_charindex(glyphs, glyph_under_cursor);
            if (char_index >= 0 && char_index < textrect.get_text().get_length())
                return extract_link(textrect.get_text(), char_index);
        }
    }
    return ts::ivec2(-1);
}

ts::str_c gui_label_ex_c::get_link_under_cursor(const ts::ivec2 &localpos) const
{
    ts::ivec2 p = get_link_pos_under_cursor(localpos);
    if (p.r0 >= 0 && p.r1 >= p.r0) return to_utf8(textrect.get_text().substr(p.r0, p.r1));
    return ts::str_c();
}


/*virtual*/ bool gui_label_ex_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
        case SQ_MOUSE_OUT:
            if (/*!popupmenu &&*/ textrect.get_text().find_pos(CONSTWSTR("<cstm=a")) >= 0)
            {
                if (overlink >= 0)
                {
                    overlink = -1;
                    textrect.make_dirty();
                    getengine().redraw();
                }
            }
            break;
        case SQ_DETECT_AREA:
            if (/*!popupmenu &&*/ textrect.get_text().find_pos(CONSTWSTR("<cstm=a")) >= 0 && !gui->mtrack(getrid(), MTT_TEXTSELECT))
            {
                if (check_overlink(data.detectarea.pos))
                    data.detectarea.area = AREA_HAND;
            }
            break;
    }
    return __super::sq_evt(qp, rid, data);
}

void gui_label_ex_c::get_link_prolog(ts::wstr_c &r, int linknum) const
{
    r.set(CONSTWSTR("<u>"));
    if (linknum == overlink)
    {
        ts::TSCOLOR c = get_default_text_color(0);
        r.append(CONSTWSTR("<color=#")).append_as_hex(ts::RED(c))
            .append_as_hex(ts::GREEN(c))
            .append_as_hex(ts::BLUE(c))
            .append_as_hex(ts::ALPHA(c))
            .append_char('>');
    }

}
void gui_label_ex_c::get_link_epilog(ts::wstr_c &r, int linknum) const
{
    if (linknum == overlink)
        r.set(CONSTWSTR("</color></u>"));
    else
        r.set(CONSTWSTR("</u>"));
}


/*virtual*/ bool gui_label_ex_c::custom_tag_parser(ts::wstr_c& r, const ts::wsptr &tv) const
{
    if (tv.l)
    {
        if (tv.s[0] == 'a')
        {
            get_link_prolog(r,ts::pwstr_c(tv).as_num_part(-2, 1));
            return true;
        }
        else if (tv.s[0] == 'b')
        {
            get_link_epilog(r,ts::pwstr_c(tv).as_num_part(-2, 1));
            return true;
        }
    }
    return false;
}

ts::wstr_c gui_label_simplehtml_c::tt() const
{
    if (overlink >= 0)
        return links.get(overlink);
    return ts::wstr_c();
}

void gui_label_simplehtml_c::created()
{
    set_theme_rect(CONSTASTR("html"), false);
    __super::created();
}

/*virtual*/ void gui_label_simplehtml_c::set_text(const ts::wstr_c&text)
{
    ts::wstr_c t(text);
    ts::swstr_t<32> ct( CONSTWSTR("<cstm=b") ); int ctl = ct.get_length();
    int x = 0;
    for(;(x = t.find_pos(x, CONSTWSTR("<a href=\""))) >= 0;)
    {
        int y = t.find_pos(x+9, '\"');
        if (y < 0) break;
        if (t.get_char(y+1) != '>') break;
        int z = t.find_pos(y + 1, CONSTWSTR("</a>"));
        if (z < 0) break;
        ct.set_char(6,'b').set_length(ctl).append_as_uint(links.size()).append_char('>');
        links.add( t.substr(x+9,y) );
        t.replace(z,4, ct );
        ct.set_char(6,'a');
        t.replace(x,y+2-x,ct);
    }
    __super::set_text(t);
}

/*virtual*/ bool gui_label_simplehtml_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (SQ_MOUSE_LUP == qp)
    {
        if (overlink >= 0 && on_link_click)
            on_link_click( links.get(overlink) );
        return true;
    }
    return __super::sq_evt(qp,rid,data);
}

//________________________________________________________________________________________________________________________________ gui tooltip

DEBUGCODE( int gui_tooltip_c::ttcount = 0; )

gui_tooltip_c::~gui_tooltip_c()
{
    gui->delete_event( DELEGATE(this, check_text) );
    DEBUGCODE(--ttcount;)
}

bool gui_tooltip_c::check_text(RID r, GUIPARAM param)
{
    ts::ivec2 cp = gui->get_cursor_pos();
    if (!gui->active_hintzone())
    {
        if (gui->get_hoverdata(cp).rid != ownrect)
        {
            TSDEL(this);
            return true;
        }
    }
    const ts::wstr_c &tt = HOLD(ownrect)().tooltip();
    if (tt.is_empty())
    {
        MODIFY( *this ).visible(false);
    } else
    {
        textrect.set_text_only(tt, false);
        if (textrect.is_dirty())
            getengine().redraw();
        cp += 20;
        ts::ivec2 sz = textrect.size;
        if (textrect.is_dirty() || textrect.size == ts::ivec2(0))
        {
            sz = textrect.calc_text_size(300, custom_tag_parser_delegate());
            textrect.set_size(sz);
        }
        if (const theme_rect_s *thr = themerect())
            sz = thr->size_by_clientsize(sz + 5, false);
        
        ts::irect maxsz = ts::wnd_get_max_size( ts::irect(cp, cp + sz) );
        if (cp.x + sz.x >= maxsz.rb.x) cp.x = maxsz.rb.x - sz.x;
        if (cp.y + sz.y >= maxsz.rb.y) cp.y = maxsz.rb.y - sz.y;
        if (cp.x < maxsz.lt.x) cp.x = maxsz.lt.x;
        if (cp.y < maxsz.lt.y) cp.y = maxsz.lt.y;

        MODIFY( *this ).pos(cp).size(sz).visible(true);
    }

    DEFERRED_UNIQUE_CALL( 0.1, DELEGATE(this, check_text), nullptr );
    return true;
}


ts::ivec2 gui_tooltip_c::get_min_size() const
{
    return __super::get_min_size();
}

/*virtual*/ void gui_tooltip_c::created()
{
    set_theme_rect(CONSTASTR("tooltip"), false);
    __super::created();
    textrect.set_margins(5,5,0);
    DEFERRED_UNIQUE_CALL( 0.1, DELEGATE(this, check_text), nullptr );
    HOLD(ownrect)().leech(this);
}
/*virtual*/ bool gui_tooltip_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid == ownrect)
    {
        switch (qp)
        {
        case SQ_MOUSE_MOVE:
        case SQ_CHECK_HINTZONE:
            check_text(RID(),nullptr);
            return false;
        case SQ_MOUSE_OUT:
            DEFERRED_UNIQUE_CALL( 0.5, DELEGATE(this, check_text), nullptr );
            return false;
        }

        return false;
    }

    ASSERT(rid == getrid());

    if (qp == SQ_RECT_CHANGED)
    {
        //return gui_label_c::sq_evt(qp,rid,data);
        return false;
    }

    return __super::sq_evt(qp,rid,data);
}

void gui_tooltip_c::create(RID owner)
{
#ifdef _DEBUG
    if ((GetAsyncKeyState(VK_CONTROL)  & 0x8000)==0x8000)
        return;
#endif
    if (owner.call_is_tooltip()) return;
    if (gmsg<GM_TOOLTIP_PRESENT>(owner).send().is(GMRBIT_ACCEPTED)) return;
    drawcollector dch;
    MAKE_ROOT<gui_tooltip_c> with_par (dch, owner);
}

ts::uint32 gui_tooltip_c::gm_handler(gmsg<GM_TOOLTIP_PRESENT> & p)
{
    if (p.rid == ownrect) return GMRBIT_ACCEPTED;
    textrect.size = ts::ivec2(0);
    if (ownrect && HOLD(ownrect)) HOLD(ownrect)().unleech( this );
    ownrect = p.rid;
    HOLD(ownrect)().leech( this );
    DEFERRED_UNIQUE_CALL( 0.5, DELEGATE(this, check_text), nullptr );
    return GMRBIT_ACCEPTED;
}

ts::uint32 gui_tooltip_c::gm_handler(gmsg<GM_KILL_TOOLTIPS> & p)
{
    TSDEL(this);
    return 0;
}

//________________________________________________________________________________________________________________________________ gui group

gui_group_c::~gui_group_c()
{
    gui->delete_event( DELEGATE(this, children_repos_delay_do) );
}

bool gui_group_c::children_repos_delay_do(RID, GUIPARAM)
{
    children_repos();
    getengine().redraw();
    return true;
}


void gui_group_c::children_repos_delay()
{
    DEFERRED_UNIQUE_CALL(0, DELEGATE(this, children_repos_delay_do), nullptr);
}


void gui_group_c::children_repos()
{
}

void gui_group_c::children_repos_info( cri_s &info ) const
{
    info.area = get_client_area();
    info.from = 0;
    info.count = getengine().children_count();
    info.areasize = 0;
}

bool gui_group_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    switch (qp)
    {
    case SQ_CHILD_CREATED:
        on_add_child(data.rect.id);
        // no break here
    case SQ_RECT_CHANGED:
        //if (data.changed.size_changed)
            children_repos();
        return true;
    case SQ_CHILD_DESTROYED:
        on_die_child(data.rect.index);
        children_repos();
        return true;
    case SQ_CHILD_ARRAY_COUNT:
        on_change_children(data.values.count);
        children_repos();
        return true;
    case SQ_CHILDREN_REPOS:
        children_repos();
        return true;
    }
    return false;
}


//________________________________________________________________________________________________________________________________ gui hgroup

gui_hgroup_c::~gui_hgroup_c()
{
}

void gui_hgroup_c::update_proportions()
{
    int fsz = 0;
    for (const rsize &x : rsizes) if (x.sz > 0) fsz += x.sz;
    if (fsz == 0) fsz = 1;
    int cnt = rsizes.count();
    ASSERT(cnt == proportions.count());
    for (int i = 0; i < cnt; ++i)
        proportions.set(i, ts::tmax(0.0f, (float)((double)cnt * rsizes.get(i).sz / fsz)));

    evt_data_s d;
    d.float_array.get() = proportions.array();
    sq_evt(SQ_GROUP_PROPORTIONS_CAHNGED, getrid(), d);
}

void gui_hgroup_c::children_repos_info(cri_s &info) const
{
    info.area = get_client_area();
    info.from = 0;
    info.count = getengine().children_count();
    info.areasize = info.area.width();
}

void gui_hgroup_c::children_repos()
{
    cri_s info;
    children_repos_info(info);

    rsizes.clear();
    ASSERT(proportions.count() == info.count);
    if (info.count <= 0) return;

    auto movable_splitter = [&](int i)->bool // i == 1..(child_num-1)
    {
        ASSERT(i > 0 && i < info.count);
        bool cs0 = rsizes.get(i - 1).const_size();
        bool cs1 = rsizes.get(i).const_size();
        if (!cs0 && !cs1) return true;
        if (cs0 && cs1) return false;
        if (cs0 && i == 1) return false;
        if (cs1 && i == (info.count-1)) return false;
        if (cs0) for (int x = 1; x < i; ++x)
            if (!rsizes.get(x - 1).const_size()) return true;
        if (cs1) for (int x = info.count-1; x > i; --x)
            if (!rsizes.get(x).const_size()) return true;
        return false;
    };

    ts::irect clar = get_client_area();
    if (!clar) return;

    int vecindex = __vec_index();
    double proposum = 0;

    float *tpropo = (float *)_alloca(proportions.count() * sizeof(float) + info.count);
    ts::uint8 *szpol_override = (ts::uint8 *)(tpropo + proportions.count());
    float *tpropo_ = tpropo;
    for(float v : proportions)
    {
        float vv = v > 0 ? v : 0;
        *tpropo_++ = vv;
        proposum += vv;
    }
    if (proposum == 0) proposum = 1.0; //-V550

    // get min max size
    int szt = 0;
    for (ts::aint t = 0; t < info.count; ++t)
    {
        rsize &ww = rsizes.add();
        rectengine_c * e = getengine().get_child(t+info.from);
        if (e == nullptr)
        {
            ww.no();
            proposum -= tpropo[t];
            continue;
        }
        const guirect_c &r = e->getrect();
        if (!r.getprops().is_visible())
        {
            ww.no();
            proposum -= tpropo[t];
            continue;
        }
        szpol_override[t] = ww.sizepolicy = (ts::uint8)r.size_policy();
        ww.szmin = (ts::int16)r.get_min_size()[vecindex];
        ww.szmax = (ts::int16)r.get_max_size()[vecindex];
        ww.szsplit = 0;
        ww.sz = (ts::int16)r.getprops().size()[vecindex];
        szt += ww.sz;
    }

    if (szt > 0)
        for (ts::aint t = 0; t < info.count; ++t)
        {
            rsize &ww = rsizes.get(t);
            if (ww.sz > 0 && tpropo[t] == 0) //-V550
            {
                tpropo[t] = float(ww.sz) / (float)szt;
                proposum += tpropo[t];
            }
        }

    int splitters_sz = 0;
    if (int splitter_sz = __splitter_size())
        for (int i = 1; i < info.count; ++i)
            if (bool _ = movable_splitter(i)) { splitters_sz+=splitter_sz; rsizes.get(i-1).szsplit = (ts::uint8)splitter_sz; }

    mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_MOVESPLITTER);

    double proposum_working = proposum;
    int current_size_goal = info.areasize-splitters_sz;
    int current_prop_vsize = 0;
    bool bad_prop_size_detected = false;
    for (ts::aint t = 0; t < info.count; ++t)
    {
        rsize &ww = rsizes.get(t);
        if (ww.sz < 0) continue;

        if (opd || ww.sz == 0)
        {
            ww.sz = (ts::int16)lround((double)current_size_goal * tpropo[t] / proposum_working);
        } else
        {
            switch (ww.sizepolicy)
            {
            case SP_NORMAL:
            case SP_ANY:
                ww.sz = (ts::int16)lround((double)current_size_goal * tpropo[t] / proposum_working);
                break;
            case SP_NORMAL_MIN:
                ww.sz = ww.szmin;
                break;
            case SP_NORMAL_MAX:
                ww.sz = ww.szmax;
                if (ww.sz > current_size_goal) ww.sz = (ts::int16)lround(current_size_goal);
                break;
            }
        }
        current_size_goal -= ww.sz;
        proposum_working -= tpropo[t];
        current_prop_vsize += ww.sz;
        if (ww.sz >= 0)
            bad_prop_size_detected |= (ww.sz < ww.szmin || ww.sz > ww.szmax);
    }

    if ( bad_prop_size_detected || current_prop_vsize != (info.areasize-splitters_sz) )
    {
        looploop: // uncondition loop to fix limit size
        current_size_goal = info.areasize-splitters_sz;
        proposum_working = proposum;
        int polices = 0;
        for (ts::aint t = 0; t < info.count; ++t)
        {
            rsize &ww = rsizes.get(t);
            if (ww.sz >= 0)
            {
                int newsz_unclamped = lround((double)current_size_goal * ts::tabs(tpropo[t]) / proposum_working);
                polices |= SETBIT(szpol_override[t]);
                if (opd)
                    ww.sz = (ts::int16)ts::CLAMP(newsz_unclamped, ww.szmin, ww.szmax);
                else switch (szpol_override[t])
                {
                    case SP_NORMAL:
                        ww.sz = (ts::int16)ts::CLAMP(newsz_unclamped, ww.szmin, ww.szmax);
                        break;
                    case SP_ANY:
                        ww.sz = (ts::int16)newsz_unclamped;
                        break;
                    case SP_NORMAL_MIN:
                        ww.sz = ww.szmin;
                        break;
                    case SP_NORMAL_MAX:
                        ww.sz = ww.szmax;
                        if (ww.sz > current_size_goal) ww.sz = (ts::int16)lround(current_size_goal);
                        break;
                    case SP_KEEP:
                        break;
                }

                if (newsz_unclamped != ww.sz && tpropo[t] >= 0)
                {
                    float oldt = tpropo[t];
                    tpropo[t] = -(float)((double)ww.sz * proposum_working / (double)current_size_goal); // make negative to indicate changed

                    double sum_other = 0;
                    for (int i = 0; i < info.count; ++i)
                    {
                        if (tpropo[i] < 0) continue;
                        sum_other += tpropo[i];
                    }
                    double need_sum_other = sum_other + oldt + tpropo[t]; // remember, tpropo[t] is negative now
                    double k = need_sum_other / sum_other;
                    
                    for (int i = 0; i < info.count; ++i)
                    {
                        if (tpropo[i] < 0) continue;
                        tpropo[i] *= (float)k;
                    }

                    goto looploop;
                }
                current_size_goal -= ww.sz;
            }
            proposum_working -= ts::tabs(tpropo[t]);
            if (current_size_goal < 0 && 0 != (polices & ~SETBIT(SP_ANY)))
            {
                ts::uint8 frompol = SP_NORMAL;
                if (polices & SETBIT(SP_NORMAL_MAX)) frompol = SP_NORMAL_MAX;
                else if (polices & SETBIT(SP_KEEP)) frompol = SP_KEEP;
                else if (polices & SETBIT(SP_NORMAL_MIN)) frompol = SP_NORMAL_MIN;
                ts::uint8 topol = (0 == (polices & ~(SETBIT(SP_NORMAL)|SETBIT(SP_NORMAL_MIN)))) ? (ts::uint8)SP_ANY : (ts::uint8)SP_NORMAL;
                ASSERT( (topol == SP_NORMAL && frompol != SP_NORMAL && frompol != SP_NORMAL_MIN) || (topol == SP_ANY && (frompol == SP_NORMAL || frompol == SP_NORMAL_MIN)) );
                int maxszi = -1;
                int maxsz = 0;
                for (int i = 0; i < info.count; ++i)
                {
                    if (szpol_override[i] == frompol)
                    {
                        rsize &www = rsizes.get(i);
                        if (www.sz > maxsz)
                        {
                            maxszi = i;
                            maxsz = www.sz;
                        }
                    }
                }
                if (CHECK(maxszi >= 0 && frompol == szpol_override[maxszi]))
                    szpol_override[maxszi] = topol;

                proposum = 0;
                for(int i=0;i<info.count;++i)
                {
                    MAKE_ABS_FLOAT( tpropo[i] );
                    proposum += tpropo[i];
                }
                float normk = (float)((float)info.count / proposum);
                for(int i=0;i<info.count;++i) tpropo[i] *= normk;
                proposum *= normk;
                goto looploop;
            }
        }
    }

    if (opd)
    {
        // splitter between (opd->area) and (opd->area - 1) children must be shifted
        // but, we must check size limits

        auto try_change_size = [this]( int i, int shift, bool apply )->int
        {
            int actual_shift = shift;
            rsize &ww = rsizes.get(i);

            if (shift < 0)
            {
                int largest_shift = ww.szmin - ww.sz;
                if (actual_shift < largest_shift) actual_shift = largest_shift;

            } else
            {
                int largest_shift = ww.szmax - ww.sz;
                if (actual_shift > largest_shift) actual_shift = largest_shift;

            }
            if (apply) ww.sz += (ts::int16)actual_shift;
            return actual_shift;
        };

        int shift = opd->rect.lt.r0;
        for(int x=0;x<2;++x)
        {
            int curshift_0 = shift;
            for (int i = opd->area-1; i >= 0 && curshift_0; --i)
            {
                int done_shift = try_change_size(i, curshift_0, x!=0);
                curshift_0 -= done_shift;
            }
            int curshift_1 = -shift;
            for (int i = opd->area; i < info.count && curshift_1; ++i)
            {
                int done_shift = try_change_size(i, curshift_1, x!=0);
                curshift_1 -= done_shift;
            }
            int cannot_shift = ts::absmax(curshift_0, curshift_1);
            if ((cannot_shift > 0 && shift < 0) || (cannot_shift < 0 && shift > 0)) cannot_shift = -cannot_shift;
            shift -= cannot_shift;
        }

    } else
        update_proportions();

    ts::aint i;
    ts::ivec2 xy(0), sz( clar.size() );
    for (i = 0; i < info.count; ++i)
    {
        rectengine_c * e = getengine().get_child(i+info.from);
        if (e==nullptr) continue;
        const rsize &ww = rsizes.get(i);
        if (ww.sz < 0) continue;
        sz[vecindex] = ww.sz;
        MODIFY(e->getrect()).pos(xy + clar.lt).size(sz);
        xy[vecindex] += ww.sz + ww.szsplit;
    }
}

/*virtual*/ ts::uint32 gui_hgroup_c::__splitcursor() const
{
    return AREA_LEFT;
}

/*virtual*/ ts::ivec2 gui_hgroup_c::get_min_size() const
{
    ts::ivec2 sz = __super::get_min_size();
    if (rsizes.count() == getengine().children_count() && ASSERT(0 == __vec_index()))
    {
        int chcnt = rsizes.count();
        int cminy = 0;
        for(int i=0;i<chcnt;++i)
        {
            const rsize &szsz = rsizes.get(i);
            if (szsz.sz >= 0)
            {
                sz.x += szsz.szsplit;
                if (szsz.szmin > 0) sz.x += szsz.szmin;

                if (const rectengine_c *e = getengine().get_child(i))
                {
                    int miny = e->getrect().get_min_size().y;
                    if (cminy < miny) cminy = miny;
                }
            }
        }
        sz.y += cminy;
    }
    return sz;
}

/*virtual*/ ts::ivec2 gui_hgroup_c::get_max_size() const
{
    ts::ivec2 sz = __super::get_max_size();
    if (rsizes.count() == getengine().children_count() && ASSERT(0 == __vec_index()))
    {
        ts::ivec2 minsz = get_min_size();
        sz.y = minsz.y;
        int chcnt = rsizes.count();
        int maxx = 0;
        for (int i = 0; i < chcnt; ++i)
        {
            const rsize &szsz = rsizes.get(i);
            if (szsz.sz >= 0)
            {
                maxx += szsz.szsplit;
                if (szsz.szmax > 0) maxx += szsz.szmax;
                if (maxx > sz.x) maxx = sz.x;

                if (const rectengine_c *e = getengine().get_child(i))
                {
                    int maxy = e->getrect().get_max_size().y;
                    if (sz.y < maxy) sz.y = maxy; // take maximum
                }
            }
        }
        sz.x = maxx;
        sz += gui_control_c::get_min_size();
    }
    return sz;
}


void gui_hgroup_c::on_add_child(RID id)
{
    if (proportions.count()) proportions.add(proportions.average()); else proportions.add(1.0f);
    HOLD(id)().leech(this);
}
void gui_hgroup_c::on_die_child(int index)
{
    if (index >= 0)
        proportions.set(index, -1.0f);
}
void gui_hgroup_c::on_change_children(int count)
{
    for (int n = proportions.count() - count; n >= 0; --n)
        proportions.find_remove_slow(-1.0f);
}

void gui_hgroup_c::set_proportions(const ts::str_c&values, int div)
{
    ts::token<char> t(values,',');
    double antidiv = 1.0 / (double)div;
    for(int index = 0;t;++index)
    {
        if (ASSERT(index < proportions.count()))
        {
            proportions.set(index, (float)(((float)t->as_int()) * antidiv) );
        }
        ++t;
    }
    children_repos();
}

bool gui_hgroup_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT(getrid() >> rid)) // child?
    {
        switch (qp)
        {
            case SQ_VISIBILITY_CHANGED:
                children_repos_delay();
                return false;
        }
        return false;
    }

    if (__super::sq_evt(qp, rid, data)) return true;

    switch (qp)
    {
    case SQ_MOUSE_LDOWN:
        if (allow_move_splitter())
        {
            ts::irect clar = get_client_area();
            ts::aint chcnt = getengine().children_count();
            ts::ivec2 localmp = to_local(data.mouse.screenpos);
            int vecindex = __vec_index();
            for (ts::aint t = 1; t < chcnt; ++t)
            {
                rsize &szsz = rsizes.get(t - 1);
                clar.lt[vecindex] += szsz.sz;
                clar.rb[vecindex] = clar.lt[vecindex] + szsz.szsplit;
                if (szsz.szsplit && clar.inside(localmp))
                {
                    mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_MOVESPLITTER);
                    opd.mpos = data.mouse.screenpos;
                    opd.area = t;
                    opd.rect.lt.r0 = 0;
                }
                clar.lt[vecindex] = clar.rb[vecindex];
            }
        }
        break;
    case SQ_MOUSE_LUP:
        if (allow_move_splitter())
        if (gui->end_mousetrack(getrid(), MTT_MOVESPLITTER))
            update_proportions();
        break;
    case SQ_MOUSE_MOVE_OP:
        if (allow_move_splitter())
        if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_MOVESPLITTER))
        {
            int vecindex = __vec_index();
            int dmouse = data.mouse.screenpos()[vecindex] - opd->mpos[vecindex];
            opd->rect.lt.r0 = dmouse;
            children_repos();
        }
        break;
    case SQ_DETECT_AREA:
        if (allow_move_splitter())
        {
            if (gui->mtrack(getrid(), MTT_MOVESPLITTER))
            {
                data.detectarea.area = __splitcursor();
            } else if (data.detectarea.area == 0)
            {
                if (allow_move_splitter())
                {
                    ts::irect clar = get_client_area();
                    ts::aint chcnt = getengine().children_count();
                    int vecindex = __vec_index();
                    for (ts::aint t = 1; t < chcnt; ++t)
                    {
                        rsize &szsz = rsizes.get(t - 1);
                        clar.lt[vecindex] += szsz.sz;
                        clar.rb[vecindex] = clar.lt[vecindex] + szsz.szsplit;
                        if (szsz.szsplit && clar.inside(data.detectarea.pos))
                        {
                            data.detectarea.area = __splitcursor();
                            break;
                        }
                        clar.lt[vecindex] = clar.rb[vecindex];
                    }

                }
            }
        }
        return true;
    case SQ_DRAW:
        {
        if (allow_move_splitter())
            if (const theme_rect_s *thr = themerect())
            {
                evt_data_s d;
                d.draw_thr.rect = get_client_area();
                ts::aint chcnt = getengine().children_count();
                int vecindex = __vec_index();
                ASSERT(chcnt == rsizes.count());
                for (ts::aint t = 1; t < chcnt; ++t)
                {
                    rsize &szsz = rsizes.get(t - 1);
                    if (szsz.sz < 0) continue;
                    d.draw_thr.rect().lt[vecindex] += szsz.sz; //-V807
                    int next = d.draw_thr.rect().rb[vecindex] = d.draw_thr.rect().lt[vecindex] + szsz.szsplit;
                    if (szsz.szsplit && getroot()) getroot()->draw(*thr, DTHRO_CENTER_ONLY, &d);
                    d.draw_thr.rect().lt[vecindex] = next;
                }
            }

        }
        break;
    }

    return false;
}


//________________________________________________________________________________________________________________________________ gui vgroup

/*virtual*/ void gui_vgroup_c::children_repos_info(cri_s &info) const
{
    info.area = get_client_area();
    info.from = 0;
    info.count = getengine().children_count();
    info.areasize = info.area.height();
}

/*virtual*/ ts::uint32 gui_vgroup_c::__splitcursor() const
{
    return AREA_TOP;
}

/*virtual*/ ts::ivec2 gui_vgroup_c::get_min_size() const
{
    ts::ivec2 sz = gui_control_c::get_min_size();
    if (rsizes.count() == getengine().children_count() && ASSERT(1 == __vec_index()))
    {
        int chcnt = rsizes.count();
        int cminx = 0;
        for (int i = 0; i < chcnt; ++i)
        {
            const rsize &szsz = rsizes.get(i);
            if (szsz.sz >= 0)
            {
                sz.y += szsz.szsplit;
                if (szsz.szmin > 0) sz.y += szsz.szmin;

                if (const rectengine_c *e = getengine().get_child(i))
                {
                    int minx = e->getrect().get_min_size().x;
                    if (cminx < minx) cminx = minx;
                }
            }
        }
        sz.x += cminx;
    }
    return sz;
}

/*virtual*/ ts::ivec2 gui_vgroup_c::get_max_size() const
{
    ts::ivec2 sz = gui_group_c::get_max_size();
    if (rsizes.count() == getengine().children_count() && ASSERT(1 == __vec_index()))
    {
        int chcnt = rsizes.count();
        int maxy = 0;
        for (int i = 0; i < chcnt; ++i)
        {
            const rsize &szsz = rsizes.get(i);
            if (szsz.sz >= 0)
            {
                maxy += szsz.szsplit;
                if (szsz.szmax > 0) maxy += szsz.szmax;
                if (maxy > sz.y) maxy = sz.y;

                if (const rectengine_c *e = getengine().get_child(i))
                {
                    int maxx = e->getrect().get_max_size().x;
                    if (sz.x > maxx) sz.x = maxx;
                }
            }
        }
        sz.y = maxy;
        sz += gui_control_c::get_min_size();
    }
    return sz;
}


//________________________________________________________________________________________________________________________________ gui vscrollgroup

void gui_vscrollgroup_c::children_repos()
{
    if (flags.is(F_NO_REPOS)) return;
    cri_s info;
    children_repos_info(info);

    if (info.count <= 0) return;
    if (!info.area) return;

    int height_need = info.area.height() / info.count;

    struct ctl_info_s
    {
        int h;
        int maxw;
    };

    ts::aint sz = sizeof(ctl_info_s) * info.count;
    ctl_info_s *infs = (ctl_info_s *)_alloca(sz);

    int sbwidth = 0;

    repeat_calc_h:
    int scroll_target_y = -1;
    int vheight = 0;
    for(int i=0;i<info.count;++i)
    {
        rectengine_c * e = getengine().get_child(i+info.from);
        if (e == nullptr) { infs[i].h = 0; continue; }
        const guirect_c &r = e->getrect();
        int h = r.get_height_by_width( info.area.width()-sbwidth );
        ts::ivec2 maxsz = r.get_max_size();
        if (h == 0)
            h = r.getprops().is_visible() ? ts::CLAMP(height_need, r.get_min_size().y, maxsz.y) : 0;
        e->__spec_set_outofbound(true);

        infs[i].h = h;
        infs[i].maxw = maxsz.x;
        if (e == scroll_target)
            scroll_target_y = vheight;
        vheight += h;
    }

    drawflags.set_size((info.count + 7) >> 3);
    drawflags.fill(0);

    flags.init( F_SBVISIBLE, vheight > info.area.height() );

    if (flags.is(F_SBVISIBLE) && sbwidth == 0)
    {
        sbwidth = sbhelper.sbrect.width();
        if (!sbwidth)
            if (const theme_rect_s *thr = themerect())
                sbwidth = thr->sis[SI_SBREP].width();

        if (sbwidth) goto repeat_calc_h;
    }
    sbhelper.set_size(vheight, info.area.height());
    if (scroll_target_y >= 0)
        sbhelper.shift = -scroll_target_y;
    else if (flags.is(F_LAST_REPOS_AT_END) && flags.is(F_SBVISIBLE))
        sbhelper.shift = info.area.height() - vheight;

    for( ts::aint i = 0, y = sbhelper.shift; i < info.count; ++i )
    {
        int h = infs[i].h;
        if (h == 0)
            continue;

        ts::irect crect( ts::ivec2(info.area.lt.x, info.area.lt.y + y), ts::ivec2(info.area.rb.x, info.area.lt.y + y + h) );
        crect.intersect(info.area);
        if (!crect)
        {
            if (y < 0)
            {
                y += h;
                continue;
            }

            break;
        }
        rectengine_c *e = getengine().get_child(i+info.from);
        e->__spec_set_outofbound(false);
        int w = ts::tmin( infs[i].maxw, info.area.width()-sbwidth );
        MODIFY( e->getrect() ).pos( info.area.lt.x, info.area.lt.y + y ).size( w, h );
        drawflags.set_bit(i, true);
        y += h;
    }

    scroll_target = nullptr;

    flags.init( F_LAST_REPOS_AT_END, !flags.is(F_SBVISIBLE) || sbhelper.at_end(info.area.height()) );

}

void gui_vscrollgroup_c::on_add_child(RID id)
{
    HOLD(id)().leech(this); // no we'll got all queries of child! MU HA HA HA
}

bool gui_vscrollgroup_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT( (getrid() >> rid) || HOLD(rid)().is_root() )) // child?
    {
        switch (qp)
        {
        case SQ_CHILD_CREATED:
            return false;
        case SQ_VISIBILITY_CHANGED:
                children_repos_delay();
            return false;
        case SQ_MOUSE_WHEELUP:
        case SQ_MOUSE_WHEELDOWN:
        case SQ_MOUSE_LDOWN:
        case SQ_MOUSE_LUP:
        case SQ_MOUSE_MOVE:
        case SQ_MOUSE_MOVE_OP:
            break;

        default:
            return false;
        }
    } else 
        if (__super::sq_evt(qp, rid, data)) return true;

    switch (qp)
    {
    case SQ_DRAW:
        {
            rectengine_root_c &root = SAFE_REF(getroot());

            const theme_rect_s *thr = themerect();

            cri_s info;
            children_repos_info(info);

            ts::irect clar = root.getrect().to_local(to_screen( info.area ));
            draw_data_s &dd = getengine().begin_draw();
            dd.cliprect.intersect(clar);
            for(int i=0;i<info.count;++i)
                if (rectengine_c *e = getengine().get_child(i+info.from))
                {
                    evt_data_s d = data.draw.get();
                    d.draw().fake_draw = !drawflags.get_bit(i);
                    e->sq_evt(qp, e->getrid(), d);
                }

            getengine().end_draw();

            if (thr && flags.is(F_SBVISIBLE))
            {
                evt_data_s ds;
                ds.draw_thr.sbrect = info.area;
                sbhelper.draw(thr, root, ds, flags.is(F_SBHL));
            }
        }
        break;
    case SQ_MOUSE_WHEELUP:
        sbhelper.shift += MWHEELPIXELS * 2;
    case SQ_MOUSE_WHEELDOWN:
        sbhelper.shift -= MWHEELPIXELS;
        flags.clear(F_LAST_REPOS_AT_END);
        children_repos();
        getengine().redraw();
        break;
    case SQ_MOUSE_OUT:
        if (flags.is(F_SBHL))
        {
            flags.clear(F_SBHL);
            getengine().redraw();
        }
        break;
    case SQ_MOUSE_MOVE:
        if (flags.is(F_SBVISIBLE) && !gui->mtrack(getrid(), MTT_SBMOVE))
        {
            bool of = flags.is(F_SBHL);
            flags.init(F_SBHL, sbhelper.sbrect.inside(to_local(data.mouse.screenpos)));
            if (flags.is(F_SBHL) != of)
                getengine().redraw();
        }
        break;
    case SQ_MOUSE_LDOWN:
        {
            ts::ivec2 mplocal = to_local(data.mouse.screenpos);
            if (sbhelper.sbrect.inside(mplocal))
            {
                flags.set(F_SBHL);
                cri_s info;
                children_repos_info(info);
                mousetrack_data_s &opd = gui->begin_mousetrack(getrid(), MTT_SBMOVE);
                opd.mpos = data.mouse.screenpos;
                opd.rect.lt.y = sbhelper.sbrect.lt.y;
                opd.rect.lt.y -= info.area.lt.y;

            }
        }
        break;
    case SQ_MOUSE_LUP:
        gui->end_mousetrack(getrid(), MTT_SBMOVE);
        break;
    case SQ_MOUSE_MOVE_OP:
        if (mousetrack_data_s *opd = gui->mtrack(getrid(), MTT_SBMOVE)) 
        {
            flags.clear(F_LAST_REPOS_AT_END);
            int dmouse = data.mouse.screenpos().y - opd->mpos.y;

            cri_s info;
            children_repos_info(info);

            if (sbhelper.scroll(opd->rect.lt.y + dmouse, info.area.height()))
            {
                children_repos();
                getengine().redraw();
            }
        }
        break;
    }

    return false;
}

gui_popup_menu_c::~gui_popup_menu_c()
{
    gmsg<GM_POPUPMENU_DIED>( menu.lv() ).send();

    if (gui)
    {
        gui->delete_event(DELEGATE(this, check_focus));
        gui->delete_event(DELEGATE(this, update_size));
    }
}

bool gui_popup_menu_c::update_size(RID, GUIPARAM)
{
    ts::aint chcnt = getengine().children_count();
    if (!chcnt) return false;
    ts::aint asz = sizeof(int) * chcnt;
    int *rheights = (int *)_alloca(asz);
    memset(rheights, -1, asz);
    ts::ivec2 csz(0);

    for (int t = 0; t < chcnt; ++t)
    {
        rectengine_c * e = getengine().get_child(t);
        if (!e)
        {
            rheights[t] = 0;
            continue;
        }
        const guirect_c &r = e->getrect();
        ts::ivec2 sz = r.get_min_size();
        int h = r.getprops().is_visible() ? sz.y : 0;
        rheights[t] = h;
        if (sz.x > csz.x) csz.x = sz.x;
        csz.y += h;
    }

    const theme_rect_s *thr = themerect();
    ts::ivec2 sz = thr ? thr->size_by_clientsize(csz, false) : csz;
    ts::ivec2 cp = getprops().screenpos();

    ts::irect maxsz = ts::wnd_get_max_size_fs(ts::irect(cp, cp + sz));
    if (cp.x + sz.x >= maxsz.rb.x) cp.x = maxsz.rb.x - sz.x;
    if (cp.y + sz.y >= maxsz.rb.y) cp.y = maxsz.rb.y - sz.y;
    if (cp.x < maxsz.lt.x) cp.x = maxsz.lt.x;
    if (cp.y < maxsz.lt.y) cp.y = maxsz.lt.y;
    if (cp.y < showpoint.y) cp.y = showpoint.y - sz.y;
    if (cp.x < showpoint.x) cp.x = showpoint.x - sz.x - showpoint.z;


    MODIFY(*this).pos(cp).size(sz);
    ts::irect clar = get_client_area();

    int y = clar.lt.y;
    for (int t = 0; t < chcnt; ++t)
    {
        int h = rheights[t];
        if (h == 0)
            continue;

        MODIFY( getengine().get_child(t)->getrect() ).pos( clar.lt.x, y ).size( clar.width(), h );

        y += h;
    }
    return true;
}

bool gui_popup_menu_c::check_focus(RID r, GUIPARAM p)
{
    if (getrid() >>= gui->get_focus()) return false;
    int lv = -1;
    if (RID fr = gui->get_focus()) 
    {
        if (fr == hostrid) return false;
        lv = fr.call_get_menu_level();
        if (lv >= menu.lv()) return false; // submenu clicked/shown
        if (lv >= 0 && lv == (menu.lv() - 1)) return false;
    }

    gmsg<GM_KILLPOPUPMENU_LEVEL>( lv + 1 ).send();
    return true;
}

/*virtual*/ bool gui_popup_menu_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT(getrid() >> rid)) // child?
    {
        switch (qp)
        {
        //case SQ_MOUSE_IN:
        //    MODIFY(rid).highlight(true);
        //    return false;
        //case SQ_MOUSE_OUT:
        //    MODIFY(rid).highlight(false);
        //    return false;
        case SQ_MOUSE_LDOWN:
        case SQ_MOUSE_LUP:
        case SQ_MOUSE_MOVE:
        case SQ_MOUSE_MOVE_OP:
        case SQ_FOCUS_CHANGED:
        case SQ_GET_MENU_LEVEL:
            break;
        case SQ_RECT_CHANGED:
            if (data.changed.size_changed)
                DEFERRED_UNIQUE_CALL(0, DELEGATE(this, update_size), nullptr);
            return false;
        default:
            return false;
        }
    }

    switch (qp)
    {
    case SQ_GET_MENU_LEVEL:
        data.getsome.handled = true;
        data.getsome.level = menu.lv();
        return true;
    case SQ_RECT_CHANGED:
        return true;
    case SQ_FOCUS_CHANGED:
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, check_focus), nullptr);
        if (data.changed.focus) data.changed.is_active_focus = true;
        return true;
    case SQ_CHILD_CREATED:
        HOLD(data.rect.id)().leech(this);
        // no break here
    case SQ_CHILD_DESTROYED:
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, update_size), nullptr);
        return true;
    case SQ_DRAW:
        return gui_control_c::sq_evt(qp,getrid(),data);
    case SQ_KEYDOWN:
        if (data.kbd.scan == SSK_ESC)
            gmsg<GM_KILLPOPUPMENU_LEVEL>(menu.lv()).send();
        return true;
    }

    return __super::sq_evt(qp,rid,data);
}

/*virtual*/ void gui_popup_menu_c::created()
{
    set_theme_rect(CONSTASTR("menu"), false);
    __super::created();
}

gui_popup_menu_c & gui_popup_menu_c::show( const ts::ivec3& screenpos, const menu_c &menu, bool sys )
{
    drawcollector dch;
    gui_popup_menu_c &m = gui_popup_menu_c::create(dch, screenpos, menu, sys);

    int dummy = 0;
    menu.iterate_items(m, dummy);

    return m;
}

MAKE_ROOT<gui_popup_menu_c>::~MAKE_ROOT()
{
    MODIFY(*me).pos(screenpos.xy()).visible(true);
    gui->set_focus(id);
    me->getroot()->set_system_focus(true);
}

gui_popup_menu_c & gui_popup_menu_c::create(drawcollector &dch, const ts::ivec3& screenpos_, const menu_c &mnu, bool sys)
{
    gmsg<GM_KILLPOPUPMENU_LEVEL>( mnu.lv() ).send();
    gmsg<GM_KILL_TOOLTIPS>().send();
    return MAKE_ROOT<gui_popup_menu_c>( dch, screenpos_, mnu, sys );
}

bool gui_popup_menu_c::operator()(int, const ts::wsptr& txt)
{
    gui_menu_item_c::create(getrid(), txt, 0, MENUHANDLER(), ts::str_c()).separator(true);
    return true;
}
bool gui_popup_menu_c::operator()(int, const ts::wsptr& txt, const menu_c &sm)
{
    gui_menu_item_c::create(getrid(), txt, 0, MENUHANDLER(), ts::str_c()).submenu( sm );
    return true;
}
bool gui_popup_menu_c::operator()(int, const ts::wsptr& txt, ts::uint32 f, MENUHANDLER handler, const ts::str_c& prm)
{
    gui_menu_item_c::create(getrid(), txt, f, handler, prm);
    return true;
}

ts::uint32 gui_popup_menu_c::gm_handler( gmsg<GM_KILLPOPUPMENU_LEVEL> & p )
{
    if (p.level <= menu.lv())
    {
        if (closehandler) closehandler(getrid(), nullptr);
        sq_evt(SQ_POPUP_MENU_DIE, getrid(), ts::make_dummy<evt_data_s>(true)); // send evt to self - leech will intercept it
        TSDEL(this);
    }
    return 0;
}
    
ts::uint32 gui_popup_menu_c::gm_handler(gmsg<GM_POPUPMENU_DIED> & p)
{
    if (p.level == menu.lv() + 1)
        gui->set_focus( getrid() );
    return 0;
}


ts::uint32 gui_popup_menu_c::gm_handler(gmsg<GM_TOOLTIP_PRESENT> & p)
{
    return GMRBIT_ACCEPTED;
}

gui_menu_item_c::~gui_menu_item_c()
{
    if (g_app)
    {
        gui->delete_event(DELEGATE(this, open_submenu));
    }
}

/*virtual*/ ts::ivec2 gui_menu_item_c::get_min_size() const
{
    if (const theme_rect_s *thr = themerect())
    {
        if (flags.is(F_SEPARATOR))
        {
            return ts::ivec2(thr->clientborder.lt.x + thr->clientborder.rb.x, 5);
        } else
        {
            int tl = textrect.calc_text_size(-1, custom_tag_parser_delegate()).x;
            if (submnu) tl += thr->sis[SI_RIGHT].width();
            return ts::ivec2(thr->clientborder.lt.x + tl + thr->clientborder.rb.x, thr->sis[SI_LEFT].height());
        }
    }
    return __super::get_min_size();
}

/*virtual*/ bool gui_menu_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (submnu_shown && submnu_shown->getrid() == rid)
    {
        if (qp == SQ_POPUP_MENU_DIE)
        {
            submnu_shown = nullptr;
            sq_evt(SQ_THEMERECT_CHANGED, getrid(), ts::make_dummy<evt_data_s>(true));
        }
        return false;
    }

    switch (qp)
    {
    case SQ_MOUSE_LUP:
        if (submnu)
            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, open_submenu), nullptr);
        else if (!flags.is(F_DISABLED))
        {
            ts::safe_ptr<gui_popup_menu_c> pm = &HOLD(getparent()).as<gui_popup_menu_c>();
            if (handler) handler(param);
            if (pm)
            {
                pm->set_close_handler(GUIPARAMHANDLER()); // disable close handler
                if (RID host = pm->host()) // notify host
                    host.call_item_activated(textrect.get_text(), param);
            }
            gmsg<GM_KILLPOPUPMENU_LEVEL>(0).send();
            return true;
        }
        break;
    case SQ_MOUSE_IN:
        if (submnu)
            DEFERRED_UNIQUE_CALL(0.5,DELEGATE(this, open_submenu), nullptr);
        else
            gmsg<GM_KILLPOPUPMENU_LEVEL>( getparent().call_get_menu_level() + 1 ).send();
        MODIFY(getrid()).highlight(true);
        break;
    case SQ_MOUSE_OUT:
        gui->delete_event(DELEGATE(this, open_submenu));
        MODIFY(getrid()).highlight(false);
        break;
    case SQ_GET_POPUP_MENU_POS:
        {
            int dback = 0;
            if (const theme_rect_s *thr = themerect())
                dback = thr->sis[SI_RIGHT].width() / 2;

            data.getsome.pos = ts::ivec3( to_screen(getprops().szrect().rt() - ts::ivec2(dback, -3)), getprops().size().x - dback * 2 );
            data.getsome.handled = true;
        }
        return true;
    case SQ_DRAW:
        
        if (ASSERT(m_engine))
        {
            if (const theme_rect_s *thr = themerect())
            {
                ts::uint32 options = DTHRO_LEFT_CENTER;
                if (flags.is(F_SEPARATOR))
                    options |= DTHRO_BOTTOM;
                if (submnu)
                    options |= DTHRO_RIGHT;
                m_engine->draw(*thr, options);
            }

            if (flags.is(F_SEPARATOR))
            {
            } else
            {
                draw_data_s &dd = m_engine->begin_draw();
                if (flags.is(F_DISABLED)) dd.alpha = 128;
                ts::irect ca = get_client_area();
                dd.size = ca.size();
                if (dd.size >> 0)
                {
                    dd.offset += ca.lt;
                    text_draw_params_s tdp;
                    draw(dd, tdp);
                }
                m_engine->end_draw();
            }
        }

        return true;
    }
    return __super::sq_evt(qp,rid,data);
}

/*virtual*/ void gui_menu_item_c::created()
{
    set_theme_rect(CONSTASTR("menuitem"), false);
    __super::created();
}

MAKE_CHILD<gui_menu_item_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
    get().set_text(text);
    get().applymif(flags);
}

gui_menu_item_c::gui_menu_item_c(MAKE_CHILD<gui_menu_item_c> &data) :gui_label_c(data)
{
    handler = data.handler;
    param = data.prm;
}

gui_menu_item_c & gui_menu_item_c::create(RID owner, const ts::wsptr &text, ts::uint32 flags, MENUHANDLER handler, const ts::str_c &param)
{
    MAKE_CHILD<gui_menu_item_c> maker(owner, text);
    maker << flags << handler << param;
    return maker.get();
}

gui_menu_item_c & gui_menu_item_c::applymif(ts::uint32 miflags)
{
    bool oldm = flags.is(F_MARKED);
    flags.init(F_MARKED, 0 != (miflags & MIF_MARKED));
    flags.init(F_DISABLED, 0 != (miflags & MIF_DISABLED));
    if (oldm != flags.is(F_MARKED))
        MODIFY(getrid()).active( flags.is(F_MARKED) );

    return *this;
}

gui_menu_item_c & gui_menu_item_c::separator(bool f)
{
    bool oldsep = flags.is(F_SEPARATOR);
    flags.init(F_SEPARATOR, f);
    if (oldsep != flags.is(F_SEPARATOR))
        MODIFY(getrid()).sizeh( get_min_size().y );

    if (flags.is(F_SEPARATOR)) submnu.reset();

    return *this;
}

gui_menu_item_c & gui_menu_item_c::submenu(const menu_c &m)
{
    if (textrect.get_text().is_empty()) textrect.set_text_only( CONSTWSTR("???"), true );
    flags.clear(F_SEPARATOR);
    submnu = m;
    MODIFY(getrid()).sizeh( get_min_size().y );
    getengine().redraw();

    return *this;
}

bool gui_menu_item_c::open_submenu(RID r, GUIPARAM p)
{
    if (submnu_shown) return false;
    submnu_shown = &gui_popup_menu_c::show( getrid().call_get_popup_menu_pos(), submnu );
    submnu_shown->leech(this);
    return true;
}


//________________________________________________________________________________________________________________________________ gui_textfield_c

MAKE_CHILD<gui_textfield_c>::~MAKE_CHILD()
{
    if (selector)
    {
        get().selector = &(gui_button_c &)MAKE_CHILD<gui_button_c>(get().getrid());
        get().selector->set_face_getter(selectorface ? selectorface : BUTTON_FACE(selector)); //-V807
        get().selector->set_handler(handler, param);
        ts::ivec2 minsz = get().selector->get_min_size();
        get().set_margins(0, minsz.x);
        get().height = ts::tmax( minsz.y, get().get_font()->height );
    } else
    {
        get().height = get().get_font()->height;
    }
    if (multiline) get().height = multiline;
    MODIFY(get()).setminsize(get().getrid()).visible(true);
}

gui_textfield_c::gui_textfield_c(MAKE_CHILD<gui_textfield_c> &data) :gui_textedit_c(data)
{
    check_text_func = data.checker;
    set_chars_limit(data.chars_limit);
    set_multiline(data.multiline != 0);
    set_text(data.text);
}


gui_textfield_c::~gui_textfield_c()
{
}

ts::uint32 gui_textfield_c::gm_handler( gmsg<GM_ROOT_FOCUS>&p )
{
    if (p.pass == 0 && !is_disabled_caret() && (p.rootrid >>= getrid()))
    {
        if (/*!p.activefocus ||*/ getrid() == gui->get_active_focus())
            p.activefocus = getrid();
        return GMRBIT_CALLAGAIN;
    }
    return 0;
}

bool gui_textfield_c::push_selector(RID, GUIPARAM)
{
    if (selector && is_enabled()) selector->push();
    return true;
}

void gui_textfield_c::badvalue( bool b )
{
    if (b)
        set_theme_rect(CONSTASTR("textfield.bad"), false);
    else
        set_theme_rect(CONSTASTR("textfield"), false);
}

/*virtual*/ void gui_textfield_c::created()
{
    set_theme_rect(CONSTASTR("textfield"), false);
    defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;

    __super::created();
}

/*virtual*/ ts::ivec2 gui_textfield_c::get_min_size() const
{
    ts::ivec2 sz = __super::get_min_size();
    if (height)
    {
        sz.y = height;
        if (const theme_rect_s *thr = themerect())
            sz.y += thr->clientborder.lt.y + thr->clientborder.rb.y;
    }
    return sz;
}
/*virtual*/ ts::ivec2 gui_textfield_c::get_max_size() const
{
    ts::ivec2 sz = __super::get_max_size();
    if (height)
    {
        sz.y = height;
        if (const theme_rect_s *thr = themerect())
            sz.y += thr->clientborder.lt.y + thr->clientborder.rb.y;
    }
    return sz;
}


/*virtual*/ bool gui_textfield_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_RECT_CHANGED && selector)
    {
        ts::irect clar = get_client_area();
        ts::ivec2 ssz = selector->get_min_size();
        MODIFY(*selector).pos( clar.rb.x - ssz.x, clar.lt.y + (clar.height()-ssz.y)/2 ).size(ssz).visible(true);
    }
    if (qp == SQ_ITEM_ACTIVATED)
    {
        set_text( data.item.text() );
    }
    if (qp == SQ_CTL_ENABLE)
    {
        __super::sq_evt(qp,rid,data);
        if (selector) selector->sq_evt(qp,rid,data);
        return true;
    }
    return __super::sq_evt(qp,rid,data);
}

////

//________________________________________________________________________________________________________________________________ gui_vtabsel_c

MAKE_CHILD<gui_vtabsel_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}


/*virtual*/ gui_vtabsel_c::~gui_vtabsel_c()
{}

/*virtual*/ void gui_vtabsel_c::created()
{
    set_theme_rect(CONSTASTR("vtab"), false);
    __super::created();
    {
        AUTOCLEAR(flags, F_NO_REPOS);
        ts::pair_s<RID, int> cp(RID(), 0);
        menu.iterate_items(*this, cp);
    }
    children_repos();
    
}

ts::uint32 gui_vtabsel_c::gm_handler( gmsg<GM_HEARTBEAT> & )
{
    if (!currentgroup)
    {
        for (rectengine_c *c : getengine())
            if (c)
            {
                getrid().call_open_group( c->getrid() );
                break;
            }
        return 0;
    }
    if (!activeitem)
    {
        for (rectengine_c *c : getengine())
        {
            if (c && c->getrect().getprops().is_active())
            {
                activeitem = c->getrid();
                return 0;
            }
        }
        if (currentgroup) currentgroup.call_lbclick();
    }
    return 0;
}

/*virtual*/ bool gui_vtabsel_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT(getrid() >> rid)) // child?
    {
        switch (qp)
        {
        case SQ_MOUSE_LDOWN:
            for(rectengine_c *c : getengine())
                if (c && c->getrid() != rid) MODIFY(c->getrid()).active(false);
            return false;
        default:
            return false;
        }
    }

    switch (qp)
    {
    case SQ_OPEN_GROUP:
        {
            // find 1st rect after initiator
            currentgroup = data.rect.id;
            for (rectengine_c *c : getengine())
                if (c)
                {
                    if (!currentgroup)
                    {
                        currentgroup = c->getrid();
                        break;
                    }
                    if (c->getrid() == currentgroup) currentgroup = RID();
                }
            if (ASSERT(currentgroup != data.rect.id))
            {
                currentgroup.call_lbclick();
            }
        }


        return true;
    }

    return __super::sq_evt(qp,rid,data);
}

bool gui_vtabsel_c::operator()(ts::pair_s<RID, int>&, const ts::wsptr& txt) {return true;}
bool gui_vtabsel_c::operator()(ts::pair_s<RID, int>& cp, const ts::wsptr& txt, const menu_c &sm)
{
    MAKE_CHILD<gui_vtabsel_item_c> make(getrid(), txt, cp.second);
    make << sm << cp.first;
    if (cp.first)
        cp.first = make;

    ts::pair_s<RID, int> cpx(make, sm.lv());
    sm.iterate_items(*this, cpx);

    return true;
}
bool gui_vtabsel_c::operator()(ts::pair_s<RID, int>& cp, const ts::wsptr& txt, ts::uint32 /*flags*/, MENUHANDLER handler, const ts::str_c& prm)
{
    MAKE_CHILD<gui_vtabsel_item_c> make(getrid(), txt, cp.second);
    make << handler << prm << cp.first;
    if (cp.first)
        cp.first = make;
    return true;
}

MAKE_CHILD<gui_vtabsel_item_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}

gui_vtabsel_item_c::~gui_vtabsel_item_c()
{}

/*virtual*/ ts::ivec2 gui_vtabsel_item_c::get_min_size() const
{
    if (const theme_rect_s *thr = themerect())
    {
        int tl = textrect.calc_text_size(-1, custom_tag_parser_delegate()).x;
        if (submnu) tl += thr->sis[SI_RIGHT].width();
        return ts::ivec2(thr->clientborder.lt.x + tl + thr->clientborder.rb.x, thr->sis[SI_LEFT].height());
    }
    return __super::get_min_size();
}

/*virtual*/ ts::ivec2 gui_vtabsel_item_c::get_max_size() const
{
    if (const theme_rect_s *thr = themerect())
    {
        int tl = textrect.calc_text_size(-1, custom_tag_parser_delegate()).x;
        if (submnu) tl += thr->sis[SI_RIGHT].width();
        return ts::ivec2(thr->clientborder.lt.x + tl + thr->clientborder.rb.x, thr->sis[SI_LEFT].height());
    }
    return __super::get_max_size();
}

/*virtual*/ bool gui_vtabsel_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
    case SQ_GET_MENU_LEVEL:
        data.getsome.handled = true;
        data.getsome.level = lv;
        return true;
    case SQ_GET_MENU:
        data.getsome.handled = true;
        data.getsome.menu = &submnu;
        return true;
    case SQ_MOUSE_LDOWN:
        if (submnu)
        {
            getparent().call_open_group( getrid() );
        } else 
        {
            MODIFY(*this).active(true);
            if (handler)
                handler(param);
        }
        break;
    case SQ_MOUSE_IN:
        MODIFY(*this).highlight(true);
        return false;
    case SQ_MOUSE_OUT:
        MODIFY(*this).highlight(false);
        return false;
    case SQ_DRAW:

        if (ASSERT(m_engine))
        {
            const theme_rect_s *thr = themerect();
            if (thr)
            {
                ts::uint32 options = DTHRO_LEFT_CENTER;
                m_engine->draw(*thr, options);
            }
            ts::irect ca = get_client_area();
            {
                draw_data_s &dd = m_engine->begin_draw();
                dd.size = ca.size();
                if (dd.size >> 0)
                {
                    dd.offset += ca.lt;
                    text_draw_params_s tdp;
                    draw(dd, tdp);
                }
                m_engine->end_draw();
            }
        }

        return true;
    }
    return __super::sq_evt(qp, rid, data);
}

/*virtual*/ void gui_vtabsel_item_c::created()
{
    set_theme_rect(CONSTASTR("vtabitem"), false);
    __super::created();
}

