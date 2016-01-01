#include "rectangles.h"


MAKE_CHILD<gui_listitem_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
    //get().set_autoheight();
    get().set_text( text );
    get().gm = gm;
}

gui_listitem_c::gui_listitem_c(initial_rect_data_s &data, gui_listitem_params_s &prms) : gui_label_ex_c(data), param(prms.param), icon(prms.icon)
{
    if (prms.themerect.is_empty())
        set_theme_rect(CONSTASTR("lstitem"), false);
    else
        set_theme_rect(prms.themerect, false);

    height = 1;
    marginleft_icon = prms.addmarginleft_icon;
    textrect.change_option(ts::TO_VCENTER, ts::TO_VCENTER);
    textrect.make_dirty(true, true, true);
    if (icon)
    {
        textrect.set_margins(ts::ivec2(icon->info().sz.x + prms.addmarginleft_icon + prms.addmarginleft, 0));
        if (icon->info().sz.y > height)
            height = icon->info().sz.y;
    }
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
    gui_control_c &lst = HOLD(getparent()).as<gui_control_c>();
    set_updaterect( lst.get_updaterect() );

    defaultthrdraw = DTHRO_BASE;
    __super::created();
}

/*virtual*/ int gui_listitem_c::get_height_by_width(int width) const
{
    if (!textrect.get_text().is_empty())
    {
        if (const theme_rect_s *thr = themerect())
        {
            return ts::tmax(height, textrect.calc_text_size(width - thr->clborder_x(), custom_tag_parser_delegate()).y) + 4 + thr->clborder_y();
        } else
        {
            return ts::tmax(height, textrect.calc_text_size(width, custom_tag_parser_delegate()).y) + 4;
        }
    }
    return 0;
}


void gui_listitem_c::set_text(const ts::wstr_c&t, bool full_height_last_line)
{
    __super::set_text(t, full_height_last_line);
    int h = get_height_by_width(-INT_MAX);

    if (icon && icon->info().sz.y > h)
        h = icon->info().sz.y;

    if (h != height)
    {
        height = h;
        MODIFY(*this).sizeh(height);
    }
}

/*virtual*/ bool gui_listitem_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if ( rid != getrid() )
    {
        if (popupmenu && popupmenu->getrid() == rid)
        {
            //if (SQ_POPUP_MENU_DIE == qp)
            //    MODIFY(*this).highlight( gui->get_hoverdata( gui->get_cursor_pos() ).minside == getrid() );
            return false;
        }

        if (getrid() >> rid)
        {
            switch (qp)
            {
            case SQ_MOUSE_WHEELUP:
            case SQ_MOUSE_WHEELDOWN:
                return __super::sq_evt(qp, rid, data);;
            case SQ_MOUSE_IN:
                MODIFY(getrid()).highlight(true);
                break;
            case SQ_MOUSE_OUT:
                MODIFY(getrid()).highlight(!popupmenu.expired());
                break;
            }
        }

        return false;
    }

    switch (qp)
    {
    case SQ_CHILD_CREATED:
        HOLD(data.rect.id)().leech(this);
        break;
    case SQ_MOUSE_IN:
        MODIFY(getrid()).highlight(true);
        return false;
    case SQ_MOUSE_OUT:
        MODIFY(getrid()).highlight(!popupmenu.expired());
        return false;
    case SQ_MOUSE_RUP:
        if (gm)
        {
            menu_c m = gm(param, false);
            if (!m.is_empty())
            {
                popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), m);
                MODIFY(rid).active(true);
            }
        }
        return false;
    case SQ_MOUSE_L2CLICK:
        if (gm) gm(param, true);
        return false;
    case SQ_YOU_WANNA_DIE:
        return param.equals(data.strparam);
    case SQ_RECT_CHANGED:
        textrect.make_dirty(false, false, true);
        break;
    case SQ_DRAW:
        if (rid != getrid()) return false;
        __super::sq_evt(qp, rid, data);
        if (icon)
        {
            ts::irect cla = get_client_area();
            getengine().begin_draw();
            getengine().draw( ts::ivec2(cla.lt.x + marginleft_icon, (cla.height()-icon->info().sz.y)/2+cla.lt.y), icon->extbody(), ts::irect(0, icon->info().sz), true );
            getengine().end_draw();
        }
        return true;
    }
    return __super::sq_evt(qp, rid, data);
}

ts::uint32 gui_listitem_c::gm_handler(gmsg<GM_POPUPMENU_DIED> &p)
{
    if (p.level == 0 && getprops().is_active())
        MODIFY(getrid()).active(false);
    return 0;
}


bool TSCALL dialog_already_present( int udtag )
{
    return gmsg<GM_DIALOG_PRESENT>( udtag ).send().is(GMRBIT_ACCEPTED);
}

ts::uint32 gui_dialog_c::gm_handler(gmsg<GM_DIALOG_PRESENT> &p)
{
    int udt = unique_tag();
    if (udt == 0) return 0;
    if (p.unique_tag == udt) return GMRBIT_ACCEPTED;
    return 0;
}

namespace
{
    struct finda
    {
        bool found = false;
        bool operator()(ts::str_c&, const ts::wsptr&) { return true; } // skip separator
        bool operator()(ts::str_c& fn, const ts::wsptr&, menu_c&m)
        {
            m.iterate_items(*this, fn);
            return true;
        }
        bool operator()(ts::str_c& fn, const ts::wstr_c&txt, ts::uint32 &flags, MENUHANDLER h, const ts::str_c&prm)
        {
            if (found)
            {
                RESETFLAG(flags, MIF_MARKED);
            }
            else
            {
                found = prm.equals(fn);
                INITFLAG(flags, MIF_MARKED, found);
            }
            return true;
        }
    };
}

bool gui_dialog_c::description_s::updvalue2(RID r, GUIPARAM p)
{
    switch (ctl)
    {
        case gui_dialog_c::description_s::_RADIO:
            if (handler) handler(r, p);
            text.set_as_int((int)p); //-V205
            break;
        case gui_dialog_c::description_s::_CHECKBUTTONS:
            if (handler) handler(r, p);
            text.set_as_uint((ts::uint32)p); //-V205
            break;
        case gui_dialog_c::description_s::_HSLIDER:
            {
                if (handler) handler(r, p);
                gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
                ts::wstr_c k(text);
                text.set_as_float(pp->value).append_char('/').append(k.substr(k.find_pos('/') + 1));
            }
            break;
        case gui_dialog_c::description_s::_SELECTOR:
            {
                if (handler) handler(r, p);
                if (behav_s::EVT_ON_OPEN_MENU == ((behav_s *)p)->e)
                {
                    finda ssss;
                    ((behav_s *)p)->menu->iterate_items(ssss, ((behav_s *)p)->param);

                } else if (behav_s::EVT_ON_CLICK == ((behav_s *)p)->e)
                    text = ts::from_utf8( ((behav_s *)p)->param );
            }
            break;
    }

    return true;
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

void gui_dialog_c::description_s::page_caption(const ts::wsptr& text_)
{
    ctl = _CAPTION;
    text = text_;
}

void gui_dialog_c::description_s::page_header(const ts::wsptr& text_)
{
    ctl = _HEADER;
    text = text_;
}

gui_dialog_c::description_s&gui_dialog_c::description_s::subctl(int tag, ts::wstr_c &ctldesc)
{
    ASSERT(tag >= 0);
    subctltag = tag;
    if (ctl == _TEXT || ctl == _PASSWD)
    {
        ASSERT(height_ == 0);
        height_ = ts::g_default_text_font->height;

        if (const theme_rect_s *thr = gui->theme().get_rect(CONSTASTR("textfield")))
            height_ += thr->clborder_y();
    } else if (ctl == _SELECTOR)
    {
        const button_desc_s *selb = gui->theme().get_button(CONSTASTR("selector"));
        ASSERT(height_ == 0);
        height_ = ts::tmax( ts::g_default_text_font->height, selb ? selb->size.y : 0 );
        if (const theme_rect_s *thr = gui->theme().get_rect(CONSTASTR("textfield")))
            height_ += thr->clborder_y();

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

gui_dialog_c::description_s& gui_dialog_c::description_s::panel(int h, GUIPARAMHANDLER drawhandler)
{
    ctl = _PANEL;
    height_ = h;
    handler = drawhandler;
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

gui_dialog_c::description_s& gui_dialog_c::description_s::selector( const ts::wsptr &desc_, const ts::wsptr &t_, GUIPARAMHANDLER behaviourhandler )
{
    ctl = _SELECTOR;
    desc = desc_;
    text = t_;
    handler = behaviourhandler;
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

gui_dialog_c::description_s& gui_dialog_c::description_s::hslider(const ts::wsptr &desc_, float val, const ts::wsptr &initstr, GUIPARAMHANDLER handler_)
{
    ctl = _HSLIDER;
    desc = desc_;
    text.set_as_float(val).append_char('/').append(initstr);
    handler = handler_;
    return *this;
}

gui_dialog_c::description_s& gui_dialog_c::description_s::list( const ts::wsptr &desc_, const ts::wsptr &emptytext_, int lines )
{
    ctl = _LIST;
    desc = desc_;
    text = emptytext_;
    height_ = lines > 0 ? (lines * ts::g_default_text_font->height) : (-lines);
    return *this;
}

gui_dialog_c::~gui_dialog_c()
{
}

ts::str_c gui_dialog_c::find( RID crid ) const
{
    const guirect_c *r = &HOLD(crid)();
    for (const description_s &d : descs)
        if (d.ctlptr == r)
            return d.name;
    return ts::str_c();
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
    gui_textfield_c &tf = HOLD(RID::from_param(param)).as<gui_textfield_c>();
    ts::wstr_c curp = tf.get_text();
    if (curp.is_empty())
        curp = tf.get_customdata_obj<ts::wstr_c>();
    gui->app_path_expand_env(curp);

    ts::extension_s ext;
    ext.desc = gui->app_loclabel(LL_ANY_FILES);
    ext.desc.append(CONSTWSTR(" (*.*)"));
    ext.ext = CONSTWSTR("*.*");
    ts::extensions_s exts(&ext, 1, 0);

    ts::wstr_c fn = getroot()->load_filename_dialog(ts::fn_get_path(curp), CONSTWSTR(""), exts, label_file_selector_caption);

    if (!fn.is_empty())
    {
        tf.set_text(fn);
    }

    return true;
}

bool gui_dialog_c::path_selector(RID, GUIPARAM param)
{
    if (is_disabled()) return true;
    gui_textfield_c &tf = HOLD(RID::from_param(param)).as<gui_textfield_c>();
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
    gui_textfield_c &tf = HOLD(RID::from_param(param)).as<gui_textfield_c>();
    ts::wstr_c curp = tf.get_text();

    ShellExecuteW( NULL, L"open", L"explorer", CONSTWSTR("/select,") + ts::fn_autoquote(ts::fn_get_name_with_ext(curp)), ts::fn_get_path(curp), SW_SHOWDEFAULT );

    return true;
}

bool gui_dialog_c::combo_drop(RID rid, GUIPARAM param)
{
    gui_textfield_c &tf = HOLD(RID::from_param(param)).as<gui_textfield_c>();
    if (tf.popupmenu)
    {
        TSDEL(tf.popupmenu);
        return true;
    }
    if (tf.is_disabled()) return true;
    gui_textfield_c::behav_s &beh = tf.get_customdata_obj<gui_textfield_c::behav_s>();
    tf.popupmenu = &gui_popup_menu_c::show(menu_anchor_s(tf.getprops().screenrect(), menu_anchor_s::RELPOS_TYPE_BD), beh.menu).host(tf.getrid());
    tf.popupmenu->set_click_handler( DELEGATE(&beh, onclick) );
    return true;
}

bool gui_dialog_c::custom_menu(RID rid, GUIPARAM param)
{
    gui_textfield_c &tf = HOLD(RID::from_param(param)).as<gui_textfield_c>();
    if (tf.popupmenu)
    {
        TSDEL(tf.popupmenu);
        return true;
    }
    if (tf.is_disabled()) return true;
    gui_textfield_c::behav_s &beh = tf.get_customdata_obj<gui_textfield_c::behav_s>();

    ASSERT( beh.tfrid == tf.getrid() );

    if (beh.handler)
    {
        behav_s b;
        b.e = behav_s::EVT_ON_OPEN_MENU;
        b.param = ts::to_utf8(tf.get_text());
        b.menu = &beh.menu;
        beh.handler( tf.getrid(), &b );
    }

    ts::irect r = tf.getprops().screenrect();
    r.lt.x = r.rb.x - r.height();
    tf.popupmenu = &gui_popup_menu_c::show(menu_anchor_s(r, menu_anchor_s::RELPOS_TYPE_BD), beh.menu).host(tf.getrid());
    tf.popupmenu->set_click_handler( DELEGATE(&beh, onclick) );

    return true;
}

void gui_dialog_c::set_selector_menu(const ts::asptr& ctl_name, const menu_c& m)
{
    if (RID crid = find(ctl_name))
    {
        gui_control_c &ctl = HOLD(crid).as<gui_control_c>();
        gui_textfield_c::behav_s &beh = ctl.get_customdata_obj<gui_textfield_c::behav_s>();
        beh.menu = m;
    }

    for (description_s &d : descs)
    {
        if (d.ctl == description_s::_SELECTOR && d.name == ctl_name)
        {
            d.setmenu(m);
            break;
        }
    }
}

void gui_dialog_c::set_check_value( const ts::asptr&name, bool v )
{
    if (RID crid = find(name))
    {
        gui_button_c &cb = HOLD(crid).as<gui_button_c>();
        cb.set_check_value(v);
    }
}

void gui_dialog_c::set_combik_menu( const ts::asptr& ctl_name, const menu_c& m )
{
    if (RID crid = find(ctl_name))
    {
        gui_textfield_c &ctl = HOLD(crid).as<gui_textfield_c>();
        gui_textfield_c::behav_s &beh = ctl.get_customdata_obj<gui_textfield_c::behav_s>();
        beh.menu = m;

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
        ctl.set_text(s.name);
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

void gui_dialog_c::set_edit_value( const ts::asptr& ctl_name, const ts::wstr_c& t )
{
    if (RID lrid = find(ctl_name))
    {
        gui_textfield_c &ctl = HOLD(lrid).as<gui_textfield_c>();
        ctl.set_text(t,true);
        ctl.selectall();
    }
    for (description_s &d : descs)
    {
        if ((d.ctl == description_s::_TEXT || d.ctl == description_s::_PASSWD || d.ctl == description_s::_SELECTOR) && d.name == ctl_name)
        {
            d.text = t;
            d.changed = true;
            break;
        }
    }
}

void gui_dialog_c::set_label_text( const ts::asptr& ctl_name, const ts::wstr_c& t, bool full_height_last_line )
{
    if (RID lrid = find(ctl_name))
    {
        gui_label_simplehtml_c &ctl = HOLD(lrid).as<gui_label_simplehtml_c>();
        ctl.set_text(t, full_height_last_line);
    }
}

void gui_dialog_c::set_slider_value( const ts::asptr& ctl_name, float val )
{
    if (RID rid = find(ctl_name))
    {
        gui_hslider_c &ctl = HOLD(rid).as<gui_hslider_c>();
        ctl.set_value(val);
    }
    for (description_s &d : descs)
    {
        if (d.ctl == description_s::_HSLIDER && d.name == ctl_name)
        {
            ts::wstr_c k(d.text);
            d.text.set_as_float(val).append_char('/').append(k.substr(k.find_pos('/')+1));
            break;
        }
    }
}

void gui_dialog_c::set_pb_pos( const ts::asptr& ctl_name, float val )
{
    if (RID rid = find(ctl_name))
    {
        gui_hslider_c &ctl = HOLD(rid).as<gui_hslider_c>();
        ctl.set_level(val);
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
    case TFR_TEXT_FILED_PASSWD:
        break;
    case TFR_PATH_SELECTOR:
        selector = DELEGATE( this, path_selector );
        break;
    case TFR_PATH_VIEWER:
        selector = DELEGATE( this, path_explore );
        break;
    case TFR_CUSTOM_SELECTOR:
        selector = DELEGATE(this, custom_menu);
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
    creator << ((RID)creator).to_param();

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
        tf.set_customdata_obj<gui_textfield_c::behav_s>(tf.getrid(), *addition->textfield.menu, addition->textfield.behav_handler.get(), false);
    } else if (role == TFR_CUSTOM_SELECTOR)
    {
        tf.set_readonly();
        tf.disable_caret();
        tf.set_customdata_obj<gui_textfield_c::behav_s>(tf.getrid(), *addition->textfield.menu, addition->textfield.behav_handler.get());

    } else if (role == TFR_TEXT_FILED)
    {
        tf.selectall();

    } else if (role == TFR_TEXT_FILED_PASSWD)
    {
        tf.selectall();
        tf.set_password_char('*');
    }

    //ctls[ ts::pair_s<int,int>(tag, 0) ] = &creator.get();
    return creator;
}

class gui_simple_dialog_list_c;
template<> struct MAKE_CHILD<gui_simple_dialog_list_c> : public _PCHILD(gui_simple_dialog_list_c)
{
    int height;
    ts::wstr_c emptymessage;
    MAKE_CHILD(RID parent_, int height, const ts::wstr_c &emptymessage):height(height) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_simple_dialog_list_c : public gui_vscrollgroup_c
{
    DUMMY(gui_simple_dialog_list_c);
    int height;
    ts::wstr_c emptymessage;
public:
    gui_simple_dialog_list_c(MAKE_CHILD<gui_simple_dialog_list_c> &data) :gui_vscrollgroup_c(data), height(data.height), emptymessage(data.emptymessage) {}
    /*virtual*/ ~gui_simple_dialog_list_c()
    {
    }

    void set_emptymessage( const ts::wstr_c &emt )
    {
        if (emptymessage != emt)
        {
            emptymessage = emt;
            if (getengine().children_count() == 0)
                getengine().redraw();
        }
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
        switch (qp)
        {
        case SQ_KILL_CHILD:
            {
                ts::tmp_tbuf_t<RID> sentenced_to_death;
                for (rectengine_c *e : getengine())
                    if (e && e->getrect().sq_evt(SQ_YOU_WANNA_DIE, e->getrid(), data))
                        sentenced_to_death.add() = e->getrid();
                for (RID r : sentenced_to_death)
                    TSDEL(&HOLD(r)());
                if (sentenced_to_death.count()) getengine().redraw();
            }
            break;
        case SQ_DRAW:
            if (rid != getrid()) return false;
            if ( getengine().children_count() == 0 && !emptymessage.is_empty() )
            {
                __super::sq_evt(qp, rid, data);

                draw_data_s &dd = getengine().begin_draw();
                ts::irect cla = get_client_area();
                dd.offset += cla.lt;
                dd.size = cla.size();
                text_draw_params_s tdp;
                ts::TSCOLOR c = get_default_text_color();
                tdp.forecolor = &c;
                ts::flags32_s f; f.setup(ts::TO_VCENTER | ts::TO_HCENTER);
                tdp.textoptions = &f;
                getengine().draw( emptymessage, tdp );
                getengine().end_draw();
                return true;
            }
            break;
        }

        return __super::sq_evt(qp, rid, data);
    }
};
MAKE_CHILD<gui_simple_dialog_list_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}

void gui_dialog_c::set_list_emptymessage(const ts::asptr& ctl_name, const ts::wstr_c& t)
{
    if (RID lrid = find(ctl_name))
    {
        gui_simple_dialog_list_c &ctl = HOLD(lrid).as<gui_simple_dialog_list_c>();
        ctl.set_emptymessage(t);
    }
}

void gui_dialog_c::ctlenable( const ts::asptr&name, bool enblflg )
{
    if (RID ctl = find(name))
        ctl.call_enable(enblflg);
}

RID gui_dialog_c::list(int height, const ts::wstr_c & emptymessage)
{
    gui_simple_dialog_list_c &lst = MAKE_CHILD<gui_simple_dialog_list_c>(getrid(), height, emptymessage);
    lst.set_updaterect(getrectupdate());
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
        r.set_updaterect(getrectupdate());

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
        c.set_handler(handler, as_param(ci.mask));
        c.set_face_getter(BUTTON_FACE(check));
        c.set_text(ci.text);
        c.set_updaterect(getrectupdate());

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
    dd.textfield.menu = &m;
    dd.textfield.behav_handler = nullptr;

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
    l.set_text(text, true);
    l.set_updaterect(getrectupdate());
    if (col) l.set_defcolor(col);
    return l.getrid();
}

RID gui_dialog_c::vspace(int height)
{
    RID stub = MAKE_CHILD<gui_stub_c>( getrid(), ts::ivec2(-1, height), ts::ivec2(-1, height) );
    return stub;
}

RID gui_dialog_c::panel(int height, GUIPARAMHANDLER drawhandler)
{
    gui_panel_c &pnl = MAKE_CHILD<gui_panel_c>(getrid(), ts::ivec2(-1, height), ts::ivec2(-1, height));
    pnl.drawhandler = drawhandler;
    return pnl.getrid();
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

void gui_dialog_c::updrect_def(const void *rr, int r, const ts::ivec2 &p)
{
    ts::safe_ptr<guirect_c> & ctl = subctls[r];
    RID par = RID::from_param(rr);
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
                d.ctlptr = ctl;
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
        b.set_handler(bcr.handler, getrid().to_param());
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
        case description_s::_CAPTION:
            {
                captiontext = d.text;
                if (const theme_rect_s *thr = themerect())
                {
                    ts::irect r = getprops().szrect();
                    r.rb.y = ts::tmax(thr->capheight, thr->capheight_max);
                    getengine().redraw(&r);
                }
            }
            break;
        case description_s::_HEADER:
            rctl = label(ts::wstr_c(header_prepend,d.text).append(header_append), d.color, true);
            break;
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
        case description_s::_PANEL:
            lasthgroup = RID();
            rctl = panel(d.height_, d.handler);
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
                    ts::str_c checkname;

                    bool operator()(getta&, const ts::wsptr&) { return true; } // skip separator
                    bool operator()(getta&, const ts::wsptr&, const menu_c&) { return true; } // skip submenu
                    bool operator()(getta&g, const ts::wstr_c&txt, ts::uint32 /*flags*/, MENUHANDLER h, const ts::str_c&prm)
                    {
                        cis.addnew( txt, prm.as_uint(), g.checkname + prm );
                        return true;
                    }
                } s;
                s.checkname = d.name;

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
                        ris.addnew(txt, as_param(prm.as_int()), g.radioname + prm);
                        return true;
                    }
                } s;
                s.radioname = d.name;

                d.items.iterate_items(s, s);
                radio(s.ris.array(), DELEGATE(&d, updvalue2), as_param(d.text.as_int()));
            }
            break;
        case description_s::_LIST:
            {
                rctl = list(d.height_, d.text);
            }
            break;
        case description_s::_TEXT:
        case description_s::_PASSWD:
        case description_s::_BUTTON:
        case description_s::_HSLIDER:
        case description_s::_SELECTOR:
            rctl = d.make_ctl(this, parent);
            break;
        }
        if (rctl && d.focus_)
            gui->set_focus(rctl);

        if (rctl)
        {
            d.ctlptr = &HOLD(rctl)();
            if (!d.name.is_empty())
                ctl_by_name[d.name] = d.ctlptr;
        }
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
    case _PASSWD:
        return dlg->textfield(text, MAX_PATH, TFR_TEXT_FILED_PASSWD, DELEGATE(this, updvalue), nullptr, height_, parent);
    case _SELECTOR:
        {
            evt_data_s dd;
            dd.textfield.menu = &items;
            dd.textfield.behav_handler() = DELEGATE( this, updvalue2 );

            RID rr = dlg->textfield(text, MAX_PATH, TFR_CUSTOM_SELECTOR, nullptr, &dd, 0, parent);

            if (handler)
            {
                gui_dialog_c::behav_s b;
                b.e = gui_dialog_c::behav_s::EVT_ON_CREATE;
                b.menu = &items;
                handler(rr, &b);
            }
            return rr;
        }
        break;
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
    case _HSLIDER:
        {
            int x = text.find_pos('/');

            gui_hslider_c &sldr = ( MAKE_CHILD<gui_hslider_c>(parent) << text.substr(0, x).as_float() << text.substr(x+1).as_sptr() << DELEGATE(this, updvalue2) << DELEGATE(this, getcontextmenu) );
            return sldr.getrid();
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

    ts::uint32 allowb = caption_buttons();
    GET_TOOLTIP ttt = nullptr;
    if (g_sysconf.mainwindow)
    {
        RESETFLAG(allowb, SETBIT(CBT_MINIMIZE));
        ttt = (GET_TOOLTIP)[]()->ts::wstr_c { return ts::wstr_c(); };
    }
    gui->make_app_buttons(getrid(), allowb, ttt);

    if (0 < getengine().children_count())
    {
        bcreate_s bcr;
        bcr.tag = 0;
        getbutton(bcr);
        if (bcr.face) getengine().get_child(0)->getrid().call_setup_button(bcr);
    }

    numtopbuttons = getengine().children_count();

    reset();

    header_prepend = to_wstr(gui->theme().conf().get_string(CONSTASTR("header_prepend"), CONSTASTR("")));
    header_append = to_wstr(gui->theme().conf().get_string(CONSTASTR("header_append"), CONSTASTR("")));
    

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
}