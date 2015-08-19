#include "rectangles.h"


MAKE_CHILD<gui_listitem_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
    //get().set_autoheight();
    get().set_text( text );
    get().gm = gm;
}

gui_listitem_c::gui_listitem_c(MAKE_CHILD<gui_listitem_c> &data) :gui_label_c(data), param(data.param)
{
    if (data.themerect.is_empty())
        set_theme_rect(CONSTASTR("lstitem"), false);
    else
        set_theme_rect(data.themerect, false);

    height = 1;
    textrect.make_dirty(true, true, true);
}

gui_listitem_c::~gui_listitem_c()
{
}

/*virtual*/ ts::ivec2 gui_listitem_c::get_min_size() const
{
    return ts::ivec2(60, height);
}

/*virtual*/ ts::ivec2 gui_listitem_c::get_max_size() const
{
    ts::ivec2 m = __super::get_max_size();
    m.y = get_min_size().y;
    return m;
}

void gui_listitem_c::created()
{
    defaultthrdraw = DTHRO_BASE;
    __super::created();
}

/*virtual*/ int gui_listitem_c::get_height_by_width(int width) const
{
    int h = 0;
    if (!textrect.get_text().is_empty())
        h = textrect.calc_text_size(width, custom_tag_parser_delegate()).y + 4;
    if (const theme_rect_s *thr = themerect()) h += thr->clientborder.lt.y + thr->clientborder.rb.y;
    return h;
}


void gui_listitem_c::set_text(const ts::wstr_c&t)
{
    __super::set_text(t);
    int h = get_height_by_width(-INT_MAX);
    if (h != height)
    {
        height = h;
        MODIFY(*this).sizeh(height);
    }
}

/*virtual*/ bool gui_listitem_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
        case SQ_MOUSE_IN:
            MODIFY(getrid()).highlight(true);
            return false;
        case SQ_MOUSE_OUT:
            MODIFY(getrid()).highlight(false);
            return false;
        case SQ_MOUSE_RUP:
            if (gm)
            {
                menu_c m = gm(param, false);
                if (!m.is_empty())
                {
                    popupmenu = &gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), m);
                    MODIFY(rid).active(true);
                }
            }
            return false;
        case SQ_MOUSE_L2CLICK:
            gm(param, true);
            return false;
        case SQ_YOU_WANNA_DIE:
            return param.equals(data.strparam);
        case SQ_RECT_CHANGED:
            textrect.make_dirty(false, false, true);
            break;
    }
    return __super::sq_evt(qp, rid, data);
}

ts::uint32 gui_listitem_c::gm_handler(gmsg<GM_POPUPMENU_DIED> & p)
{
    if (p.level == 0 && getprops().is_active())
        MODIFY(getrid()).active(false);
    return 0;
}


bool TSCALL dialog_already_present( int udtag )
{
    return gmsg<GM_DIALOG_PRESENT>( udtag ).send().is(GMRBIT_ACCEPTED);
}

ts::uint32 gui_dialog_c::gm_handler(gmsg<GM_DIALOG_PRESENT> & p)
{
    int udt = unique_tag();
    if (udt == 0) return 0;
    if (p.unique_tag == udt) return GMRBIT_ACCEPTED;
    return 0;
}

ts::uint32 gui_dialog_c::gm_handler(gmsg<GM_CLOSE_DIALOG> & p)
{
    int udt = unique_tag();
    if (udt == 0) return 0;
    if (p.unique_tag == udt)
        on_close();
    return 0;
}


gui_dialog_c::description_s&  gui_dialog_c::description_s::label( const ts::wsptr& text_ )
{
    ctl = _STATIC;
    text.set(text_);
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::hiddenlabel( const ts::wsptr& text_, ts::TSCOLOR col )
{
    ctl = _STATIC_HIDDEN;
    color = col;
    text.set(text_);
    return *this;
}

void gui_dialog_c::description_s::page_header(const ts::wsptr& text_)
{
    ctl = _STATIC;
    text.set(CONSTWSTR("<p=c>"))
        .append(text_)
        .append(CONSTWSTR("<color=#808080><hr=5,1,1>"));
}

gui_dialog_c::description_s&gui_dialog_c::description_s::subctl(int tag, ts::wstr_c &ctldesc)
{
    ASSERT(tag >= 0);
    subctltag = tag;
    if (ctl == _TEXT)
    {
        ASSERT(height_ == 0);

        height_ = ts::g_default_text_font->height;

        if (const theme_rect_s *thr = gui->theme().get_rect(CONSTASTR("textfield")))
            height_ += thr->clientborder.lt.y + thr->clientborder.rb.y;
    }
    ctldesc.append(CONSTWSTR("<rect=")).append_as_int(tag).append_char(',');
    ctldesc.append_as_uint(width_).append_char(',').append_as_uint(-height_).append(CONSTWSTR(",3>"));

    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::vspace(int h, GUIPARAMHANDLER oncreatehanler)
{
    ctl = _VSPACE;
    height_ = h;
    handler = oncreatehanler;
    return *this;
}

void gui_dialog_c::description_s::hgroup( const ts::wsptr& desc_ )
{
    ctl = _HGROUP;
    desc = desc_;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::path( const ts::wsptr &desc_, const ts::wsptr &path, gui_textedit_c::TEXTCHECKFUNC checker )
{
    ctl = _PATH;
    desc = desc_;
    text = path;
    textchecker = checker;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::file(const ts::wsptr &desc_, const ts::wsptr &iroot, const ts::wsptr &fn, gui_textedit_c::TEXTCHECKFUNC checker)
{
    ctl = _FILE;
    desc = desc_;
    text = fn;
    hint = iroot;
    textchecker = checker;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::textfield( const ts::wsptr &desc_, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker )
{
    ctl = _TEXT;
    desc = desc_;
    text = val;
    textchecker = checker;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::textfieldml( const ts::wsptr &desc_, const ts::wsptr &val, gui_textedit_c::TEXTCHECKFUNC checker, int lines)
{
    ctl = _TEXT;
    desc = desc_;
    text = val;
    textchecker = checker;
    height_ = lines * ts::g_default_text_font->height;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::combik( const ts::wsptr &desc_ )
{
    ctl = _COMBIK;
    desc = desc_;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::checkb(const ts::wsptr &desc_, GUIPARAMHANDLER handler_, ts::uint32 initial)
{
    ctl = _CHECKBUTTONS;
    desc = desc_;
    text.set_as_uint( initial );
    handler = handler_;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::radio(const ts::wsptr &desc_, GUIPARAMHANDLER handler_, int initial)
{
    ctl = _RADIO;
    desc = desc_;
    text.set_as_int(initial);
    handler = handler_;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::button(const ts::wsptr &desc_, const ts::wsptr &text_, GUIPARAMHANDLER handler_)
{
    ctl = _BUTTON;
    desc = desc_;
    text = text_;
    handler = handler_;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::list( const ts::wsptr &desc_, int lines )
{
    ctl = _LIST;
    desc = desc_;
    height_ = lines > 0 ? (lines * ts::g_default_text_font->height) : (-lines);
    return *this;
}

gui_dialog_c::~gui_dialog_c()
{
}

RID gui_dialog_c::find( const ts::asptr &name ) const
{
    auto *v = ctl_by_name.find(name);
    if (v && !v->value.expired()) return v->value->getrid();
    return RID();
}

void gui_dialog_c::setctlname( const ts::asptr &name, guirect_c &r )
{
    bool added;
    ctl_by_name.add_get_item(name, added).value = &r;
}

namespace
{
class buttons_pos : public autoparam_i
{
    int index;
    guirect_c::sptr_t prevr;
public:
    buttons_pos(guirect_c *prevr, int index) :index(index), prevr(prevr)
    {
        if (prevr) prevr->leech(this);
    }
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
    {
        bool pc = qp == SQ_PARENT_RECT_CHANGED || qp == SQ_CUSTOM_INIT_DONE;
        bool c = qp == SQ_RECT_CHANGED;
        if (!pc && !c) return false;

        ASSERT(owner);
        HOLD parent(owner->getparent());

        ts::irect clar = parent().get_client_area();

        ts::ivec2 minsz = owner->get_min_size();
        minsz.x += 20;
        minsz.y += 10;


        int shiftleft = ts::tmax( owner->getprops().size().x, minsz.x) +5;

        switch (index)
        {
        case 0:
            if (pc && owner->getrid() == rid) // query for owner
            {
                MODIFY(*owner).pos(clar.rb.x - shiftleft, clar.rb.y - minsz.y - 5).size(minsz);
            }
            break;
        default:
            if ((c && owner->getrid() != rid) || (qp == SQ_CUSTOM_INIT_DONE && ASSERT(prevr)) ) // query other
            {
                const rectprops_c &oprops = prevr->getprops();
                MODIFY(*owner).pos(oprops.pos() - ts::ivec2(shiftleft, 0)).size(minsz.x, oprops.size().y);
            }
            break;
        }

        return false;
    }
};

}

bool gui_dialog_c::file_selector(RID, GUIPARAM param)
{
    if (is_disabled()) return true;
    gui_textfield_c &tf = HOLD(RID::from_ptr(param)).as<gui_textfield_c>();
    ts::wstr_c curp = tf.get_text();
    if (curp.is_empty())
        curp = tf.get_customdata_obj<ts::wstr_c>();
    gui->app_path_expand_env(curp);


    ts::wstr_c fn = getroot()->load_filename_dialog(ts::fn_get_path(curp), CONSTWSTR(""), CONSTWSTR(""), nullptr, label_file_selector_caption);

    if (!fn.is_empty())
    {
        tf.set_text(fn);
    }

    return true;
}

bool gui_dialog_c::path_selector(RID, GUIPARAM param)
{
    if (is_disabled()) return true;
    gui_textfield_c &tf = HOLD(RID::from_ptr(param)).as<gui_textfield_c>();
    ts::wstr_c curp = tf.get_text();
    gui->app_path_expand_env(curp);
    while (!curp.is_empty() && !ts::dir_present(curp))
    {
        int cutit = curp.find_last_pos_of(CONSTWSTR("/\\"));
        if (cutit < 0) break;
        curp.set_length(cutit);
    }

    ts::wstr_c path = getroot()->save_directory_dialog(CONSTWSTR("/"), label_path_selector_caption, curp);
    if (!path.is_empty())
    {
        tf.set_text(path);
    }

    return true;
}

bool gui_dialog_c::path_explore(RID rid, GUIPARAM param)
{
    gui_textfield_c &tf = HOLD(RID::from_ptr(param)).as<gui_textfield_c>();
    ts::wstr_c curp = tf.get_text();

    ShellExecuteW( NULL, L"open", L"explorer", CONSTWSTR("/select,") + ts::fn_autoquote(ts::fn_get_name_with_ext(curp)), ts::fn_get_path(curp), SW_SHOWDEFAULT );

    return true;
}

bool gui_dialog_c::combo_drop(RID rid, GUIPARAM param)
{
    gui_textfield_c &tf = HOLD(RID::from_ptr(param)).as<gui_textfield_c>();
    if (tf.popupmenu)
    {
        TSDEL(tf.popupmenu);
        return true;
    }
    if (tf.is_disabled()) return true;
    const menu_c *mnu = (const menu_c *)tf.get_customdata();
    tf.popupmenu = &gui_popup_menu_c::show(ts::ivec3(tf.getprops().screenrect().lb(),0),*mnu).host(tf.getrid());
    return true;
}

void gui_dialog_c::set_combik_menu( const ts::asptr& ctl_name, const menu_c& m )
{
    if (RID crid = find(ctl_name))
    {
        gui_control_c &ctl = HOLD(crid).as<gui_control_c>();
        ctl.set_customdata_obj<menu_c>( m );
    }

    for (description_s &d : descs)
    {
        if (d.ctl == description_s::_COMBIK && d.name == ctl_name)
        {
            d.setmenu(m);
            break;
        }
    }
}

void gui_dialog_c::set_label_text( const ts::asptr& ctl_name, const ts::wstr_c& t )
{
    if (RID lrid = find(ctl_name))
    {
        gui_label_simplehtml_c &ctl = HOLD(lrid).as<gui_label_simplehtml_c>();
        ctl.set_text(t);
    }
}

RID gui_dialog_c::textfield( const ts::wsptr &deftext, int chars_limit, tfrole_e role, gui_textedit_c::TEXTCHECKFUNC checker, const evt_data_s *addition, int multiline, RID parent )
{
    //int tag = gui->get_free_tag();

    GUIPARAMHANDLER selector;
    switch (role)
    {
    case TFR_TEXT_FILED:
    case TFR_TEXT_FILED_RO:
        break;
    case TFR_PATH_SELECTOR:
        selector = DELEGATE( this, path_selector );
        break;
    case TFR_PATH_VIEWER:
        selector = DELEGATE( this, path_explore );
        break;
    case TFR_COMBO:
        selector = DELEGATE(this, combo_drop);
        break;
    case TFR_FILE_SELECTOR:
        selector = DELEGATE(this, file_selector);
        break;
        
    }
    
    MAKE_CHILD<gui_textfield_c> creator(parent ? parent : getrid(),deftext, chars_limit, multiline, selector != GUIPARAMHANDLER());
    creator << selector << checker;
    gui_textfield_c &tf = creator;
    creator << (GUIPARAM)((RID)creator).to_ptr();

    if (role == TFR_TEXT_FILED_RO)
    {
        ts::TSCOLOR c = ts::ARGB(0, 0, 0);
        if (const theme_rect_s *thr = tf.themerect()) c = thr->deftextcolor;
        tf.set_color(c);
        tf.set_readonly();

    } else if (role == TFR_PATH_VIEWER)
    {
        ts::TSCOLOR c = ts::ARGB(0, 0, 0);
        if (const theme_rect_s *thr = tf.themerect()) c = thr->deftextcolor;
        tf.set_color( c );
        tf.set_readonly();
    } else if (role == TFR_FILE_SELECTOR)
    {
        tf.set_customdata_obj<ts::wstr_c>( addition->wstrparam.get() );

    } else if (role == TFR_COMBO)
    {
        creator.selectorface = BUTTON_FACE(combo);
        tf.set_readonly();
        tf.disable_caret();
        tf.arrow_cursor();
        tf.register_kbd_callback( DELEGATE( &tf, push_selector ), SSK_LB, false );
        tf.set_customdata_obj<menu_c>( *addition->menu );
    } else if (role == TFR_TEXT_FILED)
    {
        tf.selectall();
    }

    //ctls[ ts::pair_s<int,int>(tag, 0) ] = &creator.get();
    return creator;
}

class gui_simple_dialog_list_c;
template<> struct MAKE_CHILD<gui_simple_dialog_list_c> : public _PCHILD(gui_simple_dialog_list_c)
{
    int height;
    MAKE_CHILD(RID parent_, int height):height(height) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_simple_dialog_list_c : public gui_vscrollgroup_c
{
    DUMMY(gui_simple_dialog_list_c);
    int height;
public:
    gui_simple_dialog_list_c(MAKE_CHILD<gui_simple_dialog_list_c> &data) :gui_vscrollgroup_c(data), height(data.height) {}
    /*virtual*/ ~gui_simple_dialog_list_c()
    {
    }

    /*virtual*/ ts::ivec2 get_min_size() const override
    {
        return ts::ivec2(100, height);
    }
    /*virtual*/ ts::ivec2 get_max_size() const override
    {
        return ts::ivec2(maximum<ts::int16>::value, height);
    }
    /*virtual*/ void created() override
    {
        set_theme_rect(CONSTASTR("simplelst"), false);
        defaultthrdraw = DTHRO_CENTER | DTHRO_BORDER;
        __super::created();

    }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
    {
        if (SQ_KILL_CHILD == qp)
        {
            ts::tmp_tbuf_t<RID> sentenced_to_death;
            for(rectengine_c *e : getengine())
                if (e && e->getrect().sq_evt(SQ_YOU_WANNA_DIE, e->getrid(), data))
                    sentenced_to_death.add() = e->getrid();
            for (RID r : sentenced_to_death)
                TSDEL( &HOLD(r)() );
            if (sentenced_to_death.count()) getengine().redraw();
            
        }
        return __super::sq_evt(qp, rid, data);
    }
};
MAKE_CHILD<gui_simple_dialog_list_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}

void gui_dialog_c::ctlenable( const ts::asptr&name, bool enblflg )
{
    if (RID ctl = find(name))
        ctl.call_enable(enblflg);
}

RID gui_dialog_c::list(int height)
{
    gui_simple_dialog_list_c &lst = MAKE_CHILD<gui_simple_dialog_list_c>(getrid(), height);

    return lst.getrid();
}

int gui_dialog_c::radio( const ts::array_wrapper_c<const radio_item_s> & items, GUIPARAMHANDLER handler, GUIPARAM current )
{
    int tag = gui->get_free_tag();
    //int index = 0;
    for(const radio_item_s &ri : items)
    {
        gui_button_c &r = MAKE_VISIBLE_CHILD<gui_button_c>(getrid());
        r.set_radio(tag);
        r.set_handler(handler, ri.param);
        r.set_face_getter(BUTTON_FACE(radio));
        r.set_text(ri.text);

        if ( !ri.name.is_empty() )
            ctl_by_name[ ri.name ] = &r;

        if (current == ri.param) r.mark();
    }

    return tag;
}

int gui_dialog_c::check( const ts::array_wrapper_c<const check_item_s> & items, GUIPARAMHANDLER handler, ts::uint32 initial, int tag )
{
    if (!tag) tag = gui->get_free_tag();
    //int index = 0;
    for (const check_item_s &ci : items)
    {
        gui_button_c &c = MAKE_VISIBLE_CHILD<gui_button_c>(getrid());
        c.set_check(tag);
        c.set_handler(handler, (GUIPARAM)ci.mask);
        c.set_face_getter(BUTTON_FACE(check));
        c.set_text(ci.text);
        c.set_updaterect( DELEGATE(this, updrect) );

        if (!ci.name.is_empty())
            ctl_by_name[ci.name] = &c;

        if (0 != (initial & ci.mask)) c.mark();
    }

    return tag;

}

RID gui_dialog_c::hgroup( const ts::wsptr& desc )
{
    if (desc.l) label( CONSTWSTR("<l>") + ts::wstr_c(desc) + CONSTWSTR("</l>") );
    gui_hgroup_c &g = MAKE_VISIBLE_CHILD<gui_hgroup_c>( getrid() );
    return g.getrid();
}

RID gui_dialog_c::combik(const menu_c &m, RID parent)
{
    evt_data_s dd;
    dd.menu = &m;

    struct findamarked
    {
        ts::wstr_c name;

        bool operator()(findamarked&, const ts::wsptr&) { return true; } // skip separator
        bool operator()(findamarked&, const ts::wsptr&, const menu_c&) { return true; } // skip submenu
        bool operator()(findamarked&, const ts::wstr_c&txt, ts::uint32 f, MENUHANDLER h, const ts::str_c&prm)
        {
            if (f & MIF_MARKED) name = txt;
            return true;
        }
    } s;

    m.iterate_items(s, s);
    if (s.name.is_empty()) s.name = m.get_text(0);


    return textfield(s.name, 128, TFR_COMBO, check_always_ok, &dd, 0, parent);
}

RID gui_dialog_c::label(const ts::wstr_c &text, ts::TSCOLOR col, bool visible)
{
    gui_label_simplehtml_c &l = MAKE_VISIBLE_CHILD<gui_label_simplehtml_c>(getrid(),visible);
    l.on_link_click = (CLICK_LINK)ts::open_link;
    l.set_autoheight();
    l.set_text(text);
    l.set_updaterect( DELEGATE(this, updrect) );
    if (col) l.set_defcolor(col);
    return l.getrid();
}

RID gui_dialog_c::vspace(int height)
{
    RID stub = MAKE_CHILD<gui_stub_c>( getrid(), ts::ivec2(-1, height), ts::ivec2(-1, height) );
    return stub;
}

void gui_dialog_c::removerctl(int r)
{
    ts::safe_ptr<guirect_c> & ctl = subctls[r];
    if (ctl)
    {
        int i = getengine().get_child_index(&ctl->getengine());
        if (i >= 0 && i <= skipctls) --skipctls;
        TSDEL( ctl.get() );
    }
}

void gui_dialog_c::updrect(void *rr, int r, const ts::ivec2 &p)
{
    ts::safe_ptr<guirect_c> & ctl = subctls[r];
    RID par = RID::from_ptr(rr);
    if (!ctl)
    {
        for (description_s &d : descs)
            if (d.subctltag == r)
            {
                int h = d.height_;
                d.height_ = 0;
                ctl = &HOLD( d.make_ctl(this, par) )();
                MODIFY(*ctl).size( d.width_, h ).pos(HOLD(par)().to_local(to_screen(p)));
                if (par == getrid())
                {
                    getengine().child_move_to(skipctls++, &ctl->getengine());
                    ctl->getengine().__spec_set_outofbound(false);
                }
                d.height_ = h;
                if (!d.name.is_empty()) setctlname(d.name, *ctl);
                return;
            }
        return;
    }

    MODIFY(*ctl).pos(HOLD(par)().to_local(to_screen(p)));
}

void gui_dialog_c::reset(bool keep_skip)
{
    int cnt = getengine().children_count();
    for(int i=keep_skip ? skipctls : numtopbuttons;i<cnt;++i)
        TSDEL(getengine().get_child(i));
    getengine().cleanup_children_now();

    if (keep_skip) return;
        
    skipctls = numtopbuttons;

    guirect_c *prevb = nullptr;
    
    int bottom_area_height = 0;
    for (int tag = 0;; ++tag)
    {
        bcreate_s bcr;
        bcr.face = GET_BUTTON_FACE();
        bcr.handler = GUIPARAMHANDLER();
        bcr.tag = tag;

        getbutton(bcr);
        if (!bcr.face) break;

        ++skipctls;
        gui_button_c &b = MAKE_VISIBLE_CHILD<gui_button_c>(getrid());
        ctl_by_name[ts::str_c(CONSTASTR("dialog_button_")).append_as_uint(tag)] = &b;
        b.set_face_getter(bcr.face);
        b.tooltip(bcr.tooltip);
        if (bcr.btext.l) b.set_text(bcr.btext);
        b.leech(TSNEW(buttons_pos, prevb, tag));
        b.set_handler(bcr.handler, getrid().to_ptr());
        b.sq_evt(SQ_CUSTOM_INIT_DONE, b.getrid(), ts::make_dummy<evt_data_s>(true));
        prevb = &b;
        if (bottom_area_height == 0) bottom_area_height = b.get_min_size().y + 10;
    }

    skipctls += additions( border );
    border.rb.y += bottom_area_height;
}

void gui_dialog_c::tabsel(const ts::str_c& par)
{
    reset(true);

    struct checkset
    {
        GUIPARAMHANDLER handler;
        int tag;
    };

    ts::tmp_array_inplace_t<checkset,2> chset;

    ts::uint32 mask = par.as_uint();
    RID lasthgroup;
    for(description_s &d : descs)
    {
        if (0 == (d.mask & mask)) continue;
        if (d.subctltag >= 0) continue;
        RID parent = getrid();
        if (d.desc.is_empty() && lasthgroup)
        {
            parent = lasthgroup;
        } else if (!d.desc.is_empty())
        {
            label( CONSTWSTR("<l>") + d.desc + CONSTWSTR("</l>") );
        }
        RID rctl;
        switch (d.ctl)
        {
        case description_s::_HGROUP:
            {
                gui_hgroup_c &g = MAKE_VISIBLE_CHILD<gui_hgroup_c>( getrid() );
                lasthgroup = g.getrid();
            }
            break;
        case description_s::_STATIC:
            rctl = label(d.text, d.color, true);
            break;
        case description_s::_STATIC_HIDDEN:
            rctl = label(d.text, d.color, false);
            break;
        case description_s::_VSPACE:
            lasthgroup = RID();
            rctl = vspace(d.height_);
            if (d.handler) d.handler( rctl, nullptr );
            break;
        case description_s::_PATH:
            rctl = textfield(d.text,MAX_PATH, d.readonly_ ? TFR_PATH_VIEWER : TFR_PATH_SELECTOR, d.readonly_ ? nullptr : DELEGATE(&d, updvalue), nullptr, 0, parent);
            break;
        case description_s::_FILE: {
            evt_data_s dd;
            dd.wstrparam = d.hint.as_sptr();
            rctl = textfield(d.text, MAX_PATH, TFR_FILE_SELECTOR, d.readonly_ ? nullptr : DELEGATE(&d, updvalue), &dd, 0, parent);
            break; }
        case description_s::_COMBIK:
            if (!d.items.is_empty())
            {
                rctl = combik( d.items, parent );
            }
            break;
        case description_s::_CHECKBUTTONS:
            if (!d.items.is_empty())
            {
                int tag = 0;
                for(checkset &ch : chset)
                    if (ch.handler == d.handler)
                    {
                        tag = ch.tag;
                    }
                if (tag == 0)
                {
                    checkset &ch = chset.add();
                    ch.handler = d.handler;
                    ch.tag = gui->get_free_tag();
                    tag = ch.tag;
                }

                struct getta
                {
                    ts::tmp_array_inplace_t< check_item_s, 4 > cis;

                    bool operator()(getta&, const ts::wsptr&) { return true; } // skip separator
                    bool operator()(getta&, const ts::wsptr&, const menu_c&) { return true; } // skip submenu
                    bool operator()(getta&, const ts::wstr_c&txt, ts::uint32 /*flags*/, MENUHANDLER h, const ts::str_c&prm)
                    {
                        cis.addnew( txt, prm.as_uint() );
                        return true;
                    }
                } s;

                d.items.iterate_items(s,s);
                check( s.cis.array(), DELEGATE(&d, updvalue2), d.text.as_uint(), tag );
            }
            break;
        case description_s::_RADIO:
            if (!d.items.is_empty())
            {
                struct getta
                {
                    ts::tmp_array_inplace_t< radio_item_s, 4 > ris;
                    ts::str_c radioname;

                    bool operator()(getta&, const ts::wsptr&) { return true; } // skip separator
                    bool operator()(getta&, const ts::wsptr&, const menu_c&) { return true; } // skip submenu
                    bool operator()(getta&g, const ts::wstr_c&txt, ts::uint32 /*flags*/, MENUHANDLER h, const ts::str_c&prm)
                    {
                        ris.addnew(txt, (GUIPARAM)prm.as_int(), g.radioname + prm);
                        return true;
                    }
                } s;
                s.radioname = d.name;

                d.items.iterate_items(s, s);
                radio(s.ris.array(), DELEGATE(&d, updvalue2), (GUIPARAM)d.text.as_int());
            }
            break;
        case description_s::_LIST:
            {
                rctl = list(d.height_);
            }
            break;
        case description_s::_TEXT:
        case description_s::_BUTTON:
            rctl = d.make_ctl(this, parent);
            break;
        }
        if (rctl && d.focus_)
            gui->set_focus(rctl);
        if (rctl && !d.name.is_empty())
            ctl_by_name[d.name] = &HOLD(rctl)();
    }
    tabselected(mask);
}

RID gui_dialog_c::description_s::make_ctl(gui_dialog_c *dlg, RID parent)
{
    ASSERT(!initialization);

    switch (ctl)
    {
    case _TEXT:
        return dlg->textfield(text, MAX_PATH, readonly_ ? TFR_TEXT_FILED_RO : TFR_TEXT_FILED, DELEGATE(this, updvalue), nullptr, height_, parent);
    case _BUTTON:
        {
            gui_button_c &b = MAKE_CHILD<gui_button_c>(parent);
            if (text.begins(CONSTWSTR("face=")))
            {
                b.set_face_getter( getgetface() );
            }
            else
            {
                b.set_text(text);
                b.set_face_getter(BUTTON_FACE(button));
            }
            b.set_handler(handler, this);
            b.tooltip(gethintproc());

            if (width_)
                b.set_constant_width(width_);

            if (height_)
                b.set_constant_height(height_);

            MODIFY(b).visible(true);
            return b.getrid();
        }
        break;

    }
    FORBIDDEN();
    return RID();
}

/*virtual*/ void gui_dialog_c::created()
{
    ASSERT( getengine().children_count() == 0, "Please, do not create any controls before call this method" );
    defaultthrdraw = DTHRO_BORDER | DTHRO_BASE | /*DTHRO_CENTER_HOLE |*/ DTHRO_CAPTION | DTHRO_CAPTION_TEXT;

    ts::uint32 allowb = SETBIT(ABT_CLOSE) | (g_sysconf.mainwindow ? 0 : SETBIT(ABT_MINIMIZE));
    GET_TOOLTIP ttt = nullptr;
    if (g_sysconf.mainwindow)
    {
        ttt = (GET_TOOLTIP)[]()->ts::wstr_c { return ts::wstr_c(); };
    }
    gui->make_app_buttons(getrid(), allowb, ttt);

    if (ASSERT(0 < getengine().children_count()))
    {
        bcreate_s bcr;
        bcr.tag = 0;
        getbutton(bcr);
        if (bcr.face) getengine().get_child(0)->getrid().call_setup_button(bcr);
    }

    numtopbuttons = getengine().children_count();

    reset();

    return __super::created();
}
/*virtual*/ bool gui_dialog_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    return __super::sq_evt(qp, rid, data);
}

/*virtual*/ void gui_dialog_c::children_repos_info(cri_s &info) const
{
    info.area = get_client_area();
    info.area.lt += border.lt;
    info.area.rb -= border.rb;
    info.from = skipctls;
    info.count = getengine().children_count() - skipctls;
    info.areasize = 0;

}