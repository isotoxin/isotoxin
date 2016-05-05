#include "isotoxin.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// message box //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

dialog_msgbox_params_s dialog_msgbox_c::mb_info(const ts::wstr_c &text)
{
    return dialog_msgbox_params_s(title_information, text);
}
dialog_msgbox_params_s dialog_msgbox_c::mb_warning(const ts::wstr_c &text)
{
    return dialog_msgbox_params_s(title_warning, text);
}
dialog_msgbox_params_s dialog_msgbox_c::mb_error(const ts::wstr_c &text)
{
    return dialog_msgbox_params_s(title_error, text);
}
dialog_msgbox_params_s dialog_msgbox_c::mb_qrcode(const ts::wstr_c &text)
{
    ts::wstr_c qr(CONSTWSTR("<ee>"), text); qr.append(CONSTWSTR("<br><qr>")).append(text).append(CONSTWSTR("</qr>"));
    return dialog_msgbox_params_s(title_qr_code, qr);
}

RID dialog_msgbox_params_s::summon()
{
    return SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, *this);
}


dialog_msgbox_c::dialog_msgbox_c(MAKE_ROOT<dialog_msgbox_c> &data) :gui_isodialog_c(data), m_params(data.prms)
{
    deftitle = m_params.etitle;
    gui->register_kbd_callback( DELEGATE( this, copy_text ), ts::SSK_C, ts::casw_ctrl);
    gui->register_kbd_callback( DELEGATE( this, on_enter_press_func ), ts::SSK_ENTER, 0);
    gui->register_kbd_callback( DELEGATE( this, on_enter_press_func ), ts::SSK_PADENTER, 0);

    if ( m_params.bcancel_ )
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 0;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size()-1;
        if (!m_params.cancel_button_text.is_empty())
            bcr.btext = m_params.cancel_button_text;
    }
    if (m_params.bok_)
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 1;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size() - 1;
        if (!m_params.ok_button_text.is_empty())
            bcr.btext = m_params.ok_button_text;
    }

    int qri = m_params.desc.find_pos(CONSTWSTR("<qr>"));
    if (qri >= 0)
    {
        int qrie = m_params.desc.find_pos(CONSTWSTR("</qr>"));
        if (qrie > qri)
        {
            qrbmp.gen_qrcore(10,5,ts::to_utf8(m_params.desc.substr( qri + 4, qrie )),32, ts::ARGB(0,0,0), ts::ARGB(0,0,0,0));
            ts::wstr_c r( CONSTWSTR("<rect=17,") );
            r.append_as_int( qrbmp.info().sz.x );
            r.append_char(',');
            r.append_as_int( qrbmp.info().sz.y );
            r.append_char('>');
            m_params.desc.replace( qri, qrie + 5 - qri, r );
        }
    }
}

dialog_msgbox_c::~dialog_msgbox_c()
{
    if (gui)
    {
        gui->unregister_kbd_callback(DELEGATE(this, copy_text));
        gui->unregister_kbd_callback(DELEGATE(this, on_enter_press_func));
    }
}

void dialog_msgbox_c::updrect_msgbox(const void *, int r, const ts::ivec2 &p)
{
    if (r == 17)
    {
        getengine().draw(p - getroot()->get_current_draw_offset(), qrbmp.extbody(), true);
    }
}

bool dialog_msgbox_c::on_enter_press_func(RID, GUIPARAM)
{
    on_confirm();
    return true;
}

bool dialog_msgbox_c::copy_text(RID, GUIPARAM)
{
    ts::str_c utf8 = ts::to_utf8(m_params.desc);
    text_remove_tags(utf8);
    ts::set_clipboard_text(ts::from_utf8(utf8));
    return true;
}

/*virtual*/ void dialog_msgbox_c::created()
{
    if (m_params.etitle != title_qr_code)
        gui->exclusive_input(getrid());

    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));

    switch (m_params.etitle) //-V719
    {
        case title_error:
            ts::master().sys_beep( ts::SBEEP_ERROR );
            break;
        case title_information:
            ts::master().sys_beep( ts::SBEEP_INFO );
            break;
        case title_warning:
            ts::master().sys_beep( ts::SBEEP_WARNING );
            break;
    }

    ts::ivec2 tsz = gui->textsize(ts::g_default_text_font, m_params.desc);
    int y = 55; // sorry about this ugly constant
    if (const theme_rect_s *thr = themerect())
        y += thr->clborder_y();
    if (tsz.y > height-y)
        height = tsz.y+y;

}

void dialog_msgbox_c::getbutton(bcreate_s &bcr)
{
    if (bcr.tag >= m_buttons.size())
        return;
    bcr = m_buttons.get(bcr.tag);
}

/*virtual*/ int dialog_msgbox_c::additions(ts::irect &)
{

    descmaker dm(this);
    dm << 1;

    dm().label(CONSTWSTR("<p=c>") + m_params.desc).setname(CONSTASTR("label"));

    return 0;
}

/*virtual*/ bool dialog_msgbox_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (SQ_DRAW == qp && rid == getrid())
    {
        if (RID lbl = find(CONSTASTR("label")))
        {
            ts::ivec2 sz1( getprops().size() - ts::ivec2(30) );
            if (RID b = find(CONSTASTR("dialog_button_0")))
                sz1 = HOLD(b)().getprops().pos();

            ts::irect ca = get_client_area();
            int y0 = ca.lt.y;
            ts::irect viewrect = ts::irect( ca.lt.x + 10, y0 + 2, ca.rb.x - 10, sz1.y - 2 );
            HOLD rlbl(lbl);

            gui_label_c &l = rlbl.as<gui_label_c>();
            l.set_autoheight(false);
            l.set_vcenter();

            MODIFY(lbl).pos( viewrect.lt ).size(viewrect.size());
        }
    }

    if (__super::sq_evt(qp, rid, data)) return true;

    return false;
}

/*virtual*/ void dialog_msgbox_c::on_confirm()
{
    if (m_params.on_ok_h) m_params.on_ok_h(m_params.on_ok_par);
    __super::on_confirm();
}

/*virtual*/ void dialog_msgbox_c::on_close()
{
    if (m_params.on_cancel_h) m_params.on_cancel_h(m_params.on_cancel_par);
    __super::on_close();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// progress bar /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

dialog_pb_c::dialog_pb_c(MAKE_ROOT<dialog_pb_c> &data) :gui_isodialog_c(data), m_params(data.prms)
{
    if (m_params.bcancel_)
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 0;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size() - 1;
        if (!m_params.cancel_button_text.is_empty())
            bcr.btext = m_params.cancel_button_text;
    }
    if (m_params.bok_)
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 1;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size() - 1;
        if (!m_params.ok_button_text.is_empty())
            bcr.btext = m_params.ok_button_text;
    }
}

dialog_pb_c::~dialog_pb_c()
{
    if (m_params.pbproc)
        m_params.pbproc->on_close();
}

/*virtual*/ void dialog_pb_c::created()
{
    gui->exclusive_input(getrid());
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
}

void dialog_pb_c::getbutton(bcreate_s &bcr)
{
    if (bcr.tag >= m_buttons.size())
        return;
    bcr = m_buttons.get(bcr.tag);
}

/*virtual*/ int dialog_pb_c::additions(ts::irect &)
{
    descmaker dm(this);
    dm << 1;

    if (!m_params.desc.is_empty())
        dm().label(CONSTWSTR("<p=c>") + m_params.desc).setname(CONSTASTR("label"));
    else
        dm().vspace(32);
    dm().hslider(ts::wstr_c(), 0, CONSTWSTR("0/1/0/1"), nullptr).setname(CONSTASTR("bp"));

    return 0;
}

/*virtual*/ void dialog_pb_c::tabselected(ts::uint32 /*mask*/)
{
    if (RID pb = find(CONSTASTR("bp")))
        HOLD(pb).as<gui_hslider_c>().set_pb_mode();

    if (m_params.pbproc)
        m_params.pbproc->on_create(this);

}

void dialog_pb_c::set_level( float v, const ts::wstr_c &txt )
{
    if (RID pb = find(CONSTASTR("bp")))
        HOLD(pb).as<gui_hslider_c>().set_level(v, txt);
}

/*virtual*/ bool dialog_pb_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (SQ_DRAW == qp && rid == getrid())
    {
    }

    if (__super::sq_evt(qp, rid, data)) return true;

    return false;
}

/*virtual*/ void dialog_pb_c::on_confirm()
{
    if (m_params.on_ok_h) m_params.on_ok_h(m_params.on_ok_par);
    __super::on_confirm();
}

/*virtual*/ void dialog_pb_c::on_close()
{
    if (m_params.on_cancel_h) m_params.on_cancel_h(m_params.on_cancel_par);
    __super::on_close();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enter text ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


dialog_entertext_c::dialog_entertext_c(MAKE_ROOT<dialog_entertext_c> &data) :gui_isodialog_c(data), m_params(data.prms), watch(data.prms.watch, DELEGATE(this, watchdog))
{
}

dialog_entertext_c::~dialog_entertext_c()
{
    //gui->delete_event(DELEGATE(this, refresh_current_page));
}


/*virtual*/ void dialog_entertext_c::created()
{
    gui->exclusive_input(getrid());
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
}

void dialog_entertext_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

bool dialog_entertext_c::watchdog(RID, GUIPARAM p)
{
    TSDEL(this);
    return true;
}

ts::uint32 dialog_entertext_c::gm_handler(gmsg<ISOGM_APPRISE> &)
{
    if (g_app->F_MODAL_ENTER_PASSWORD)
    {
        if (getroot()) getroot()->set_system_focus(true);
    }
    return 0;
}

bool dialog_entertext_c::showpass_handler(RID, GUIPARAM p)
{
    if (RID txt = find(CONSTASTR("txt")))
        HOLD(txt).as<gui_textedit_c>().set_password_char(p ? 0 : '*');

    return true;
}


/*virtual*/ int dialog_entertext_c::additions(ts::irect &)
{

    descmaker dm(this);
    dm << 1;

    dm().page_header(m_params.desc);
    dm().vspace(25);

    switch (m_params.utag)
    {
        case UD_ENTERPASSWORD:
            dm().textfield(ts::wstr_c(), m_params.val, DELEGATE(this, on_edit)).focus(true).passwd(true).setname(CONSTASTR("txt"));
            dm().vspace();
            dm().checkb(ts::wstr_c(), DELEGATE(this, showpass_handler), 0).setmenu(
                menu_c().add(TTT("Show password", 379), 0, MENUHANDLER(), CONSTASTR("1"))
                );
            break;
        case UD_NEWTAG:
            dm().textfieldml(ts::wstr_c(), m_params.val, DELEGATE(this, on_edit), 3).focus(true).setname(CONSTASTR("txt"));
            break;
        default:
            dm().textfield(ts::wstr_c(), m_params.val, DELEGATE(this, on_edit)).focus(true).setname(CONSTASTR("txt"));
            break;
    }

    return 0;
}

bool dialog_entertext_c::on_edit(const ts::wstr_c &t)
{
    m_params.val = t;
    ctlenable( CONSTASTR("dialog_button_1"), m_params.checker(t) );
    return true;
}

bool dialog_entertext_c::on_enter_press_func(RID, GUIPARAM)
{
    on_confirm();
    return true;
}

bool dialog_entertext_c::on_esc_press_func(RID, GUIPARAM)
{
    on_close();
    return true;
}


/*virtual*/ void dialog_entertext_c::tabselected(ts::uint32 /*mask*/)
{
    if (RID pb = find(CONSTASTR("txt")))
    {
        gui_textfield_c &tf = HOLD(pb).as<gui_textfield_c>();
        
        tf.register_kbd_callback( DELEGATE( this, on_enter_press_func ), ts::SSK_ENTER, false );
        tf.register_kbd_callback( DELEGATE( this, on_enter_press_func ), ts::SSK_PADENTER, false );
        tf.register_kbd_callback( DELEGATE( this, on_esc_press_func ), ts::SSK_ESC, false );
    }
}



/*virtual*/ bool dialog_entertext_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

/*virtual*/ void dialog_entertext_c::on_confirm()
{
    if (m_params.okhandler)
    {
        gui->exclusive_input(getrid(), false);
        if (!m_params.okhandler(m_params.val, m_params.param))
        {
            gui->exclusive_input(getrid());
            return;
        }
    }
    __super::on_confirm();
}

/*virtual*/ void dialog_entertext_c::on_close()
{
    if (m_params.cancelhandler)
    {
        gui->exclusive_input(getrid(), false);
        if (!m_params.cancelhandler(getrid(), nullptr))
        {
            gui->exclusive_input(getrid());
            return;
        }
    }
    __super::on_close();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// about ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



dialog_about_c::dialog_about_c(initial_rect_data_s &data):gui_isodialog_c(data)
{
}

dialog_about_c::~dialog_about_c()
{
    if (gui)
    {
        gui->delete_event( DELEGATE(this, updanim) );
    }
}

/*virtual*/ void dialog_about_c::created()
{
    gui->exclusive_input(getrid());
    set_theme_rect(CONSTASTR("about"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
}

void dialog_about_c::getbutton(bcreate_s &bcr)
{
    if (bcr.tag == 0)
    {
        bcr.tag = 1;
        __super::getbutton(bcr);
    }
}


/*virtual*/ int dialog_about_c::additions(ts::irect &)
{

    descmaker dm(this);
    dm << 1;

    ts::wstr_c title(256,true);
    title.set( CONSTWSTR("<p=c><b>") );
    title.append( CONSTWSTR("<a href=\"http://isotoxin.im\">Isotoxin</a>") );
    title.append( CONSTWSTR("</b> ") );
    title.appendcvt( application_c::appver() );
    title.append( CONSTWSTR("<br>Coding, design and sounds by <a href=\"https://github.com/Rotkaermota\">Rotkaermota</a>") );
    title.append( CONSTWSTR("<br>Isotoxin is open-source freeware, licensed under <a href=\"https://github.com/Rotkaermota/Isotoxin/blob/master/LICENSE\">GPL3</a>") );
    title.append( CONSTWSTR("<br>See version history <a href=\"https://github.com/Rotkaermota/Isotoxin/releases\">here</a>") );

    dm().label( title );

    dm().vspace(5);
    dm().hiddenlabel(ts::wstr_c(), false).setname(CONSTASTR("upd"));
    dm().vspace(5);
    dm().button(ts::wstr_c(), TTT("Check for update",354), DELEGATE(this, check_update_now)).height(35).setname(CONSTASTR("checkupdb"));
    dm().vspace(5);

    title.set( CONSTWSTR("<p=c>Used libs<hr><l><a href=\"https://github.com/irungentoo/toxcore/\">Tox core</a>") );
    title.append( CONSTWSTR(" <a href=\"https://www.opus-codec.org\">Opus audio codec</a>") );
    title.append( CONSTWSTR(" <a href=\"http://vorbis.com\">Ogg Vorbis audio codec</a>"));
    title.append( CONSTWSTR(" <a href=\"https://xiph.org/flac/\">Flac audio codec</a>"));
    title.append( CONSTWSTR(" <a href=\"https://github.com/Rotkaermota/filter_audio\">Irungentoo's filter_audio fork</a>"));
    title.append( CONSTWSTR(" <a href=\"http://webmproject.org\">VP8 video codec</a>") );
    title.append( CONSTWSTR(" <a href=\"http://libsodium.org\">Sodium crypto library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://freetype.org\">Free Type font render library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://zlib.net\">zlib compression library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.libpng.org/pub/png/libpng.html\">png image library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://pngquant.org\">pngquant library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://libjpeg.sourceforge.net\">jpg image library</a>") );
    title.append( CONSTWSTR(" <a href=\"http://giflib.sourceforge.net\">The GIFLIB project</a>") );
    title.append( CONSTWSTR(" <a href=\"http://fukuchi.org/works/qrencode\">libqrencode</a>") );
    title.append( CONSTWSTR(" <a href=\"http://cairographics.org\">Cairo Graphics</a>") );
    title.append( CONSTWSTR(" <a href=\"http://g.oswego.edu/dl/html/malloc.html\">dlmalloc</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/yxbh/FastDelegate\">C++11 fastdelegates</a>") );
    title.append( CONSTWSTR(" <a href=\"http://curl.haxx.se/libcurl\">Curl</a>") );
    title.append( CONSTWSTR(" <a href=\"http://hunspell.github.io\">Hunspell</a>") );
    title.append( CONSTWSTR("</l><br><br>Adapded libs<hr><l><a href=\"https://bitbucket.org/alextretyak/s3\">Simple Sound System</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/jp9000/libdshowcapture\">DirectShow capture lib</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.codeproject.com/Articles/11132/Walking-the-callstack\">Walking the callstack</a>") );
    title.append( CONSTWSTR(" <a href=\"https://sqlite.org\">SQLite</a>") );
    title.append( CONSTWSTR("</l><br><br>Other sources and assets used<hr><l><a href=\"http://virtualdub.sourceforge.net/\">VirtualDub</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/sarbian/libsquish\">libsquish</a>") );
    title.append( CONSTWSTR(" <a href=\"http://code.google.com/p/libyuv\">libuv</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/GNOME/librsvg\">librsvg</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/libyal/liblnk\">liblink</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.efgh.com/software/md5.htm\">md5</a>") );
    title.append( CONSTWSTR(" <a href=\"http://dejavu-fonts.org\">DejaVu fonts</a>") );
    title.append( CONSTWSTR(" <a href=\"http://opensans.com\">Open Sans fonts</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.kolobok.us\">Copyright<nbsp>©<nbsp>Aiwan.<nbsp>Kolobok<nbsp>Smiles</a>"));
    title.append( CONSTWSTR(" <a href=\"https://github.com/titoBouzout/Dictionaries\">Hunspell utf8 dictionaries</a>"));

    
    
    dm().label( title );

    return 0;
}

void dialog_about_c::updrect_about(const void *rr, int r, const ts::ivec2 &p)
{
    if (1000 == r)
    {
        rectengine_root_c *root = getroot();
        root->draw(p - root->get_current_draw_offset(), pa.bmp.extbody(), true );

    } else
        updrect_def(rr,r,p);
    
}

bool dialog_about_c::updanim(RID, GUIPARAM)
{
    if (checking_new_version)
    {
        pa.render();
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, updanim), nullptr);
    }
    
    if (RID upd = find(CONSTASTR("upd")))
        HOLD(upd).engine().redraw();
    return true;
}

bool dialog_about_c::check_update_now(RID, GUIPARAM)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(false);

    checking_new_version = true;
    pa.restart();
    updanim(RID(),nullptr);

    set_label_text(CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c><rect=1000,32,32>")));
    if (RID no = find(CONSTASTR("upd")))
        MODIFY(no).visible(true);

    gui->repos_children(this);

    g_app->b_update_ver(RID(), as_param(AUB_ONLY_CHECK));

    return true;
}

ts::uint32 dialog_about_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(true);

    if (!checking_new_version) return 0;
    checking_new_version = false;
    gui->repos_children(this);

    if (nv.error_num == gmsg<ISOGM_NEWVERSION>::E_NETWORK)
    {
        set_label_text(CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(0)) + loc_text(loc_connection_failed));
        if (RID no = find(CONSTASTR("upd")))
            MODIFY(no).visible(true);
        return 0;
    }

    if (nv.ver.is_empty() || nv.error_num != gmsg<ISOGM_NEWVERSION>::E_OK)
    {
        set_label_text( CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(0)) + TTT("Update not found",355) );
        if (RID no = find(CONSTASTR("upd")))
            MODIFY(no).visible(true);
        return 0;
    }

    set_label_text( CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(1)) + (TTT("New version detected: $",357) / ts::to_wstr(nv.ver.as_sptr())) );
    if (RID yes = find(CONSTASTR("upd")))
        MODIFY(yes).visible(true);

    return 0;
}


/*virtual*/ ts::wstr_c dialog_about_c::get_name() const
{
    return APPNAME_CAPTION;
}
/*virtual*/ ts::ivec2 dialog_about_c::get_min_size() const
{
    return ts::ivec2(450, 525);
}

/*virtual*/ bool dialog_about_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    return false;
}


#define ICP_MARGIN_LEFT_RITE 15
#define ICP_MARGIN_TOP_BOTTOM 10
#define ICP_BTN_SPACE 10
#define ICP_ELEMENT_V_SPACE 5

ts::wstr_c incoming_call_text(const ts::str_c &utf8name);

MAKE_ROOT<incoming_call_panel_c>::~MAKE_ROOT()
{
    MODIFY(*me).setminsize(me->getrid()).setcenterpos().visible(true);
}

incoming_call_panel_c::incoming_call_panel_c(MAKE_ROOT<incoming_call_panel_c> &data) :gui_control_c(data)
{
    animation_c::allow_tick += 256;

    sender = data.c;
    video_supported = true;
    if (active_protocol_c *ap = prf().ap(data.c->getkey().protoid))
        video_supported = (0 != (ap->get_features() & PF_VIDEO_CALLS));

    buttons[0] = gui->theme().get_button(CONSTASTR("p_accept_call"));
    buttons[1] = video_supported ? gui->theme().get_button(CONSTASTR("p_accept_call_video")) : nullptr;
    buttons[2] = gui->theme().get_button(CONSTASTR("p_reject_call"));
    buttons[3] = gui->theme().get_button(CONSTASTR("p_ignore_call"));
    for (const button_desc_s *b : buttons)
    {
        if (b)
        {
            sz.x += b->size.x + ICP_BTN_SPACE;
            sz.y = ts::tmax(sz.y, b->size.y);
            ++nbuttons;
        }
    }

    sz.x -= ICP_BTN_SPACE;

    aname = data.c->get_name();
    text_adapt_user_input(aname);

    tsz = gui->textsize( ts::g_default_text_font, incoming_call_text(aname));
    image = gui->theme().get_image( CONSTASTR("ringtone_anim") );
}

incoming_call_panel_c::~incoming_call_panel_c()
{
    animation_c::allow_tick -= 256;
}

/*virtual*/ void incoming_call_panel_c::created()
{
    set_theme_rect( CONSTASTR("icp"), false );
    __super::created();

    gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
    b_accept.set_face_getter(BUTTON_FACE(p_accept_call));
    b_accept.set_handler(DELEGATE(this, b_accept_call), nullptr);
    MODIFY(b_accept).visible(true);

    if (video_supported)
    {
        gui_button_c &b_accept_v = MAKE_CHILD<gui_button_c>(getrid());
        b_accept_v.set_face_getter(BUTTON_FACE(p_accept_call_video));
        b_accept_v.set_handler(DELEGATE(this, b_accept_call_video), nullptr);
        MODIFY(b_accept_v).visible(true);
    }

    gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(getrid());
    b_reject.set_face_getter(BUTTON_FACE(p_reject_call));
    b_reject.set_handler(DELEGATE(this, b_reject_call), this);
    MODIFY(b_reject).visible(true);

    gui_button_c &b_ignore = MAKE_CHILD<gui_button_c>(getrid());
    b_ignore.set_face_getter(BUTTON_FACE(p_ignore_call));
    b_ignore.set_handler(DELEGATE(this, b_ignore_call), this);
    MODIFY(b_ignore).visible(true);

    ts::aint cnt = getengine().children_count();
    int x = ICP_MARGIN_LEFT_RITE;
    if (tsz.x > sz.x) x += (tsz.x - sz.x) / 2;
    int vspc = image ? (ICP_ELEMENT_V_SPACE * 2 + image->info().sz.y) : ICP_ELEMENT_V_SPACE;
    for (int i = 0; i < cnt; ++i )
    {
        const rectengine_c *e = getengine().get_child(i);
        ts::ivec2 bsz = e->getrect().get_min_size();
        MODIFY( e->getrid() ).pos( x, (sz.y -bsz.y)/2 + ICP_MARGIN_TOP_BOTTOM + tsz.y + vspc).size(bsz);
        x += bsz.x + ICP_BTN_SPACE;
    }
}

/*virtual*/ ts::ivec2 incoming_call_panel_c::get_min_size() const
{
    int vspc = image ? (ICP_ELEMENT_V_SPACE * 2 + image->info().sz.y) : ICP_ELEMENT_V_SPACE;
    return ts::ivec2(ts::tmax(sz.x, tsz.x) + ICP_MARGIN_LEFT_RITE * 2, sz.y + ICP_MARGIN_TOP_BOTTOM * 2 + tsz.y + vspc);
}

/*virtual*/ bool incoming_call_panel_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_DRAW && rid == getrid())
    {
        __super::sq_evt(qp, rid, data);

        draw_data_s &dd = getengine().begin_draw();

        if (image)
        {
            image->draw(getengine(), ts::ivec2((getprops().size().x - image->info().sz.x)/2, ICP_MARGIN_TOP_BOTTOM + tsz.y + ICP_ELEMENT_V_SPACE));
        }

        dd.offset.x += (getprops().size().x - tsz.x)/2;
        dd.offset.y += ICP_MARGIN_TOP_BOTTOM;
        dd.size = tsz;

        text_draw_params_s tdp;
        ts::TSCOLOR c = get_default_text_color();
        tdp.forecolor = &c;
        getengine().draw(incoming_call_text(aname), tdp);

        getengine().end_draw();

        return true;
    }

    return __super::sq_evt(qp, rid, data);
}

bool incoming_call_panel_c::b_accept_call_video(RID, GUIPARAM)
{
    ts::safe_ptr<incoming_call_panel_c> me(this);
    sender->b_accept_call_with_video(RID(), nullptr);
    if (me) TSDEL(this);
    return true;
}

bool incoming_call_panel_c::b_accept_call(RID, GUIPARAM)
{
    ts::safe_ptr<incoming_call_panel_c> me(this);
    sender->b_accept_call(RID(), nullptr);
    if (me) TSDEL(this);
    return true;
}

bool incoming_call_panel_c::b_reject_call(RID, GUIPARAM)
{
    ts::safe_ptr<incoming_call_panel_c> me( this );
    sender->b_reject_call(RID(), nullptr);
    if (me) TSDEL(this);
    return true;
}

bool incoming_call_panel_c::b_ignore_call(RID, GUIPARAM)
{
    stop_sound(snd_ringtone);
    stop_sound(snd_ringtone2);

    TSDEL(this);
    return true;
}

ts::uint32 incoming_call_panel_c::gm_handler(gmsg<ISOGM_CALL_STOPED> &c)
{
    TSDEL(this);
    return 0;
}

ts::uint32 incoming_call_panel_c::gm_handler(gmsg<GM_UI_EVENT>&ue)
{
    if (ue.evt == UE_THEMECHANGED)
    {
        image = gui->theme().get_image(CONSTASTR("ringtone_anim"));
    }

    return 0;
}




MAKE_ROOT<incoming_msg_panel_c>::~MAKE_ROOT()
{
    ts::irect r = HOLD( g_app->main )( ).getprops().screenrect(false);
    r = ts::wnd_get_max_size(r);

    me->tgt_y = r.rb.y - 3;
    MODIFY( *me ).setminsize( me->getrid() ).pos( r.rb - ts::ivec2( me->get_min_size().x, 3 ) ).visible( true );
}

incoming_msg_panel_c::incoming_msg_panel_c( MAKE_ROOT<incoming_msg_panel_c> &data ) :gui_control_c( data ), hist( data.hist ), sender( data.sender ), post( data.post )
{
}

incoming_msg_panel_c::~incoming_msg_panel_c()
{
    if ( gui )
    {
        dip = true;
        gmsg<ISOGM_UPDATE_MESSAGE_NOTIFICATION>().send();
        gui->delete_event( DELEGATE( this, endoflife ) );
        gui->delete_event( DELEGATE( this, tick ) );
    }
}

bool incoming_msg_panel_c::endoflife( RID, GUIPARAM )
{
    TSDEL( this );
    return true;
}

/*virtual*/ void incoming_msg_panel_c::created()
{
    set_theme_rect( CONSTASTR( "imp" ), false );
    __super::created();


    DEFERRED_UNIQUE_CALL( prf().dmn_duration(), DELEGATE( this, endoflife ), 0 );
    DEFERRED_UNIQUE_CALL( 0.01, DELEGATE( this, tick ), 0 );

    avarect = g_app->preloaded_stuff().icon[ CSEX_UNKNOWN ]->info().sz + ts::ivec2(10);

    msgitm = MAKE_CHILD<gui_message_item_c>( getrid(), hist, sender, CONSTASTR( "other" ), MTA_MESSAGE );
    msgitm->set_passive();
    msgitm->append_text( post, false );
    int h = msgitm->calc_height_by_width( sz.x - avarect.x );

    while(h > sz.y)
    {
        int i = post.message_utf8.find_last_pos( ' ' );
        if (i < 0) break;
        post.message_utf8.set_length( i ).append( CONSTASTR("...") );
        msgitm->append_text( post, false, true );
        h = msgitm->calc_height_by_width( sz.x - avarect.x );
    }
    sz.y = h;
    if ( sz.y < avarect.y )
        sz.y = avarect.y;

    MODIFY( *msgitm ).pos( avarect.x, 0 ).size( ts::ivec2(sz.x- avarect.x, sz.y) ).show();
    MODIFY( *this ).size( sz );

    avarect.y = sz.y;

    const button_desc_s *clb = gui->theme().get_button( CONSTASTR("close") );

    gui_button_c &b = MAKE_CHILD<gui_button_c>( msgitm->getrid() );
    b.set_face_getter( BUTTON_FACE( close ) );
    b.set_handler( DELEGATE( this, endoflife ), nullptr );
    b.leech( TSNEW( leech_dock_top_right_s, clb->size.x, clb->size.y, -2, -2 ) );
    MODIFY( b ).show().size( b.get_min_size() );

    DEFERRED_EXECUTION_BLOCK_BEGIN(0)
    gmsg<ISOGM_UPDATE_MESSAGE_NOTIFICATION>().send();
    DEFERRED_EXECUTION_BLOCK_END( 0 )

}

/*virtual*/ ts::ivec2 incoming_msg_panel_c::get_min_size() const
{
    return sz;
}

/*virtual*/ bool incoming_msg_panel_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    if ( qp == SQ_DRAW && rid == getrid() )
    {
        __super::sq_evt( qp, rid, data );

        m_engine->begin_draw();
        ts::ivec2 addp = get_client_area().lt;

        contact_root_c *cc = hist;
        if ( cc->getkey().is_group() )
            cc = sender->get_historian();

        if ( const avatar_s *ava = cc->get_avatar() )
        {
            m_engine->draw( addp + ( avarect - ava->info().sz ) / 2, ava->extbody(), ava->alpha_pixels );
        } else
        {
            const theme_image_s *icon = cc->getkey().is_group() ? g_app->preloaded_stuff().groupchat : g_app->preloaded_stuff().icon[ hist->get_meta_gender() ];
            icon->draw( *m_engine.get(), addp + ( avarect - icon->info().sz ) / 2 );
        }
        m_engine->end_draw();


        return true;
    }

    if ( msgitm && rid == msgitm->getrid() )
    {
        switch ( qp )
        {
        case SQ_MOUSE_LUP:

            if ( HOLD( g_app->main )( ).getprops().is_collapsed() )
                MODIFY( g_app->main ).decollapse();
            else
                HOLD( g_app->main )().getroot()->set_system_focus(true);

            hist->reselect();
            DEFERRED_UNIQUE_CALL( 0, DELEGATE( this, endoflife ), 0 );
            return true;

        case SQ_DETECT_AREA:
            data.detectarea.area = AREA_HAND;
            return true;
        }
        return true;
    }

    return __super::sq_evt( qp, rid, data );
}

bool incoming_msg_panel_c::tick( RID, GUIPARAM )
{
    if ( tgt_y != getprops().screenpos().y )
    {
        int y = ts::lround( ts::LERP<float>( (float)getprops().screenpos().y, (float)tgt_y, 0.3f ) );
        if ( y != getprops().screenpos().y )
        {
            MODIFY( *this ).pos( getprops().screenpos().x, y );
        }
    }
    DEFERRED_UNIQUE_CALL( 0.01, DELEGATE( this, tick ), 0 );
    return true;
}

ts::uint32 incoming_msg_panel_c::gm_handler( gmsg<ISOGM_UPDATE_MESSAGE_NOTIFICATION>&n )
{
    if ( dip ) return 0;

    if ( n.pass == 0 )
    {
        n.panels.set( this );
        n.panels.sort( []( incoming_msg_panel_c *p1, incoming_msg_panel_c *p2 ) {
            return p1->appeartime < p2->appeartime;
        } );
        return GMRBIT_CALLAGAIN;
    } else
    {
        ts::irect r = HOLD( g_app->main )( ).getprops().screenrect( false );
        r = ts::wnd_get_max_size( r );

        int myh = -1;
        for( incoming_msg_panel_c *p : n.panels )
        {
            if ( p && r.inside(p->getprops().screenpos()))
            {
                if ( p == this )
                    myh = 0;
                if ( myh >= 0 )
                    myh += p->sz.y + 2;
            }
        }
        tgt_y = r.rb.y + 2 - myh;
    }

    return 0;
}

ts::uint32 incoming_msg_panel_c::gm_handler( gmsg<GM_UI_EVENT>&ue )
{

    return 0;
}