#include "isotoxin.h"


dialog_msgbox_c::dialog_msgbox_c(MAKE_ROOT<dialog_msgbox_c> &data) :gui_isodialog_c(data), m_params(data.prms)
{
    if ( m_params.bcancel_ )
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 0;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size()-1;
    }
    if (m_params.bok_)
    {
        bcreate_s &bcr = m_buttons.add();
        bcr.tag = 1;
        __super::getbutton(bcr);
        bcr.tag = m_buttons.size() - 1;
    }
}

dialog_msgbox_c::~dialog_msgbox_c()
{
}


/*virtual*/ void dialog_msgbox_c::created()
{
    gui->exclusive_input(getrid());
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
}

void dialog_msgbox_c::getbutton(bcreate_s &bcr)
{
    if (bcr.tag >= m_buttons.size())
        return;
    bcr = m_buttons.get(bcr.tag);
}

/*virtual*/ int dialog_msgbox_c::additions(ts::irect & border)
{

    descmaker dm(descs);
    dm << 1;

    dm().label(CONSTWSTR("<p=c>") + m_params.desc);

    return 0;
}

/*virtual*/ bool dialog_msgbox_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
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














dialog_about_c::dialog_about_c(initial_rect_data_s &data):gui_isodialog_c(data)
{
}

dialog_about_c::~dialog_about_c()
{
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


/*virtual*/ int dialog_about_c::additions(ts::irect & border)
{

    descmaker dm(descs);
    dm << 1;

    ts::wstr_c title(256,true);
    title.set( CONSTWSTR("<p=c><b>") );
    title.append( CONSTWSTR("<a href=\"http://isotoxin.im\">Isotoxin</a>") );
    title.append( CONSTWSTR("</b> ") );
    title.appendcvt( g_app->appver() );
    title.append( CONSTWSTR("<br>Coding, design and sounds by <a href=\"https://github.com/Rotkaermota\">Rotkaermota</a>") );
    title.append( CONSTWSTR("<br>Isotoxin is open-source freeware, licensed under <a href=\"https://github.com/Rotkaermota/Isotoxin/blob/master/LICENSE\">GPL3</a>") );

    dm().label( title );

    dm().vspace(5);
    dm().hiddenlabel(ts::wstr_c(), 0).setname(CONSTASTR("upd"));
    dm().vspace(5);
    dm().button(ts::wstr_c(), TTT("Check for update",208), DELEGATE(this, check_update_now)).height(35).setname(CONSTASTR("checkupdb"));
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
    title.append( CONSTWSTR(" <a href=\"http://g.oswego.edu/dl/html/malloc.html\">dlmalloc</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/yxbh/FastDelegate\">C++11 fastdelegates</a>") );
    title.append( CONSTWSTR(" <a href=\"http://curl.haxx.se/libcurl\">Curl</a>") );
    title.append( CONSTWSTR("</l><br><br>Adapded libs<hr><l><a href=\"https://bitbucket.org/alextretyak/s3\">Simple Sound System</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.codeproject.com/Articles/11132/Walking-the-callstack\">Walking the callstack</a>") );
    title.append( CONSTWSTR(" <a href=\"https://sqlite.org\">SQLite</a>") );
    title.append( CONSTWSTR("</l><br><br>Other sources and assets used<hr><l><a href=\"http://virtualdub.sourceforge.net/\">VirtualDub</a>") );
    title.append( CONSTWSTR(" <a href=\"https://github.com/sarbian/libsquish\">libsquish</a>") );
    title.append( CONSTWSTR(" <a href=\"http://www.efgh.com/software/md5.htm\">md5</a>") );
    title.append( CONSTWSTR(" <a href=\"http://dejavu-fonts.org\">DejaVu fonts</a>") );
    title.append(CONSTWSTR(" <a href=\"http://www.kolobok.us\">Copyright<nbsp>©<nbsp>Aiwan.<nbsp>Kolobok<nbsp>Smiles</a>"));

    dm().label( title );

    return 0;
}

bool dialog_about_c::check_update_now(RID, GUIPARAM)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(false);

    checking_new_version = true;
    g_app->b_update_ver(RID(), as_param(AUB_ONLY_CHECK));

    return true;
}

ts::uint32 dialog_about_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    if (RID b = find(CONSTASTR("checkupdb")))
        b.call_enable(true);

    if (!checking_new_version) return 0;
    checking_new_version = false;

    if (nv.is_error())
    {
        set_label_text(CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(0)) + connection_failed_text());
        if (RID no = find(CONSTASTR("upd")))
            MODIFY(no).visible(true);
        return 0;
    }

    if (nv.ver.is_empty())
    {
        set_label_text( CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(0)) + TTT("Update not found",207) );
        if (RID no = find(CONSTASTR("upd")))
            MODIFY(no).visible(true);
        return 0;
    }

    set_label_text( CONSTASTR("upd"), ts::wstr_c(CONSTWSTR("<p=c>")) + maketag_color<ts::wchar>(get_default_text_color(1)) + (TTT("New version detected: $",209) / ts::to_wstr(nv.ver.as_sptr())) );
    if (RID yes = find(CONSTASTR("upd")))
        MODIFY(yes).visible(true);

    return 0;
}


/*virtual*/ ts::wstr_c dialog_about_c::get_name() const
{
    return ts::wstr_c( CONSTWSTR(APPNAME) );
}
/*virtual*/ ts::ivec2 dialog_about_c::get_min_size() const
{
    return ts::ivec2(450, 440);
}

/*virtual*/ bool dialog_about_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    return false;
}



