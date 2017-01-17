#include "isotoxin.h"

//-V::807

dialog_colors_c::dialog_colors_c(initial_rect_data_s &data):gui_isodialog_c(data)
{
}

dialog_colors_c::~dialog_colors_c()
{
}


/*virtual*/ void dialog_colors_c::created()
{
    set_theme_rect( CONSTASTR("main"), false );
    super::created();
    tabsel(CONSTASTR("1"));
}
/*virtual*/ void dialog_colors_c::getbutton(bcreate_s &bcr)
{
    super::getbutton(bcr);
    if (bcr.tag == 1)
    {
        bcr.btext = CONSTWSTR("Apply");
    }
}

menu_c dialog_colors_c::presets()
{
    menu_c m;

    ts::wstrings_c fns;

    ts::wstr_c thn( cfg().theme() );
    ts::wstr_c cln( CONSTWSTR("defcolors") );
    int s = thn.find_pos('@');
    if (s > 0)
    {
        cln = thn.substr( s + 1 );
        thn.set_length(s);
    }

    ts::wstr_c path(CONSTWSTR("themes/"), thn);
    path.append( CONSTWSTR("/*.decl") );

    ts::g_fileop->find(fns, path, false);
    fns.kill_dups_and_sort();

    for (const ts::wstr_c &f : fns)
    {
        if ( f.equals(CONSTWSTR("struct.decl")) )
            continue;

        ts::wstr_c ccln = ts::fn_get_name(f);
        bool cur = cln == ccln;
        m.add(f, cur ? (MIF_MARKED) : 0, DELEGATE(this, selcolors), to_utf8(thn).append_char('@').append(to_utf8(ccln)));
    }

    return m;
}

menu_c dialog_colors_c::colors()
{
    menu_c m;

    for (const ts::str_c &c : colorsarr)
    {
        bool cur = curcolor == c;
        m.add(from_utf8(c), cur ? (MIF_MARKED) : 0, nullptr, c);
    }

    return m;
}

bool dialog_colors_c::selcolor( RID, GUIPARAM p )
{
    behav_s *b = (behav_s *)p;
    if (behav_s::EVT_ON_CLICK == b->e)
    {

        curcolor = b->param;
        set_selector_menu(CONSTASTR("colors"), colors());
        color_selected();
    }

    return true;
}


void dialog_colors_c::selcolors( const ts::str_c&t )
{
    ts::wstr_c tt(from_utf8(t));
    cfg().theme( tt );
    g_app->load_theme(tt);
    load_preset(tt);
}

void dialog_colors_c::load_preset( const ts::wstr_c &tt )
{
    colsmap.clear();
    colorsarr.clear();

    ts::wstr_c colorsdecl;
    int dd = tt.find_pos('@');
    if (dd >= 0)
    {
        colorsdecl.set(tt.cstr() + dd + 1, tt.get_length() - dd - 1).append(CONSTWSTR(".decl"));
    }
    else
    {
        dd = tt.get_length();
        colorsdecl = CONSTWSTR("defcolors.decl");
    }

    ts::wstr_c path(CONSTWSTR("themes/"));
    path.append(ts::wsptr(tt.cstr(), dd)).append_char('/').append(colorsdecl);


    ts::abp_c bp;
    if (ts::g_fileop->load(path, bp))
    {
        colsfn = path;
        if (ts::abp_c * cols = bp.get("colors"))
        {
            for (auto it = cols->begin(); it; ++it)
            {
                ts::TSCOLOR col = ts::parsecolor<char>(it->as_string(), ts::ARGB(0, 0, 0));
                colsmap.add(it.name()) = col;
                colorsarr.add(it.name());
            }
        }
    }
    colorsarr.sort(true);
    curcolor = colorsarr.get(0);

    set_combik_menu(CONSTASTR("colors"), colors());
    set_combik_menu(CONSTASTR("presets"), presets());
    color_selected();
}

bool dialog_colors_c::cprev(RID, GUIPARAM)
{
    ts::aint curci = colorsarr.find(curcolor) - 1;
    if (curci < 0)
        curci = colorsarr.size() - 1;
    curcolor = colorsarr.get(curci);
    set_combik_menu(CONSTASTR("colors"), colors());
    color_selected();

    return true;
}
bool dialog_colors_c::cnext(RID, GUIPARAM)
{
    ts::aint curci = colorsarr.find(curcolor) + 1;
    if (curci >= colorsarr.size())
        curci = 0;
    curcolor = colorsarr.get(curci);
    set_combik_menu(CONSTASTR("colors"), colors());
    color_selected();

    return true;
}

void dialog_colors_c::color_selected()
{
    ts::TSCOLOR oc = curcol;
    curcol = ts::ARGB(0,0,0);
    if (const auto* c = colsmap.find( curcolor ))
        curcol = c->value;
    if (oc != curcol)
        getengine().redraw();

    set_slider_value(CONSTASTR("R"), (float)ts::RED(curcol) / 255.0f);
    set_slider_value(CONSTASTR("G"), (float)ts::GREEN(curcol) / 255.0f);
    set_slider_value(CONSTASTR("B"), (float)ts::BLUE(curcol) / 255.0f);
    set_slider_value(CONSTASTR("A"), (float)ts::ALPHA(curcol) / 255.0f);

    updatetext();
}

/*virtual*/ int dialog_colors_c::additions(ts::irect & )
{
    load_preset( cfg().theme() );

    descmaker dm(this);
    dm << 1;

    dm().page_header(CONSTWSTR("colors"));

    dm().combik(CONSTWSTR("Preset")).setmenu(presets()).setname(CONSTASTR("presets"));
    dm().vspace();
    dm().hgroup(CONSTWSTR("Color"));
    dm().selector(ts::wsptr(), from_utf8(colorsarr.get(0)), DELEGATE(this, selcolor)).setmenu(colors()).setname(CONSTASTR("colors"));
    dm().button(ts::wsptr(), CONSTWSTR("<char=60>"), DELEGATE(this, cprev) ).height(30).width(30).setname(CONSTASTR("bprev"));
    dm().button(ts::wsptr(), CONSTWSTR("<char=62>"), DELEGATE(this, cnext) ).height(30).width(30).setname(CONSTASTR("bnext"));
    dm().vspace();

    dm().hslider(ts::wsptr(), 0, CONSTWSTR("0/0/1/1"), DELEGATE(this, hslider_r)).setname(CONSTASTR("R"));
    dm().vspace(2);
    dm().hslider(ts::wsptr(), 0, CONSTWSTR("0/0/1/1"), DELEGATE(this, hslider_g)).setname(CONSTASTR("G"));
    dm().vspace(2);
    dm().hslider(ts::wsptr(), 0, CONSTWSTR("0/0/1/1"), DELEGATE(this, hslider_b)).setname(CONSTASTR("B"));
    dm().vspace(2);
    dm().hslider(ts::wsptr(), 0, CONSTWSTR("0/0/1/1"), DELEGATE(this, hslider_a)).setname(CONSTASTR("A"));
    dm().vspace(2);
    dm().textfield(ts::wsptr(), curcolastext(), DELEGATE(this,colastext)).setname(CONSTASTR("str"));

    color_selected();

    return 0;
}

bool dialog_colors_c::colastext(const ts::wstr_c &c, bool )
{
    ts::wstr_c cc(c);
    if (cc.get_char(0) != '#') cc.insert(0,'#');
    curcol = ts::parsecolor<ts::wchar>(cc, curcol);
    colsmap[curcolor] = curcol;
    color_selected();
    return true;
}

ts::wstr_c dialog_colors_c::curcolastext() const
{
    return ts::to_wstr( make_color(curcol) );
}

bool dialog_colors_c::hslider_r(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    curcol = ts::ARGB<int>( ts::lround(pp->value * 255.0), ts::GREEN(curcol), ts::BLUE(curcol), ts::ALPHA(curcol) );
    colsmap[ curcolor ] = curcol;
    pp->custom_value_text.set( CONSTWSTR("R: ") ).append_as_int( ts::RED(curcol) );
    getengine().redraw();
    updatetext();
    return true;
}
bool dialog_colors_c::hslider_g(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    curcol = ts::ARGB<int>( ts::RED(curcol), ts::lround(pp->value * 255.0), ts::BLUE(curcol), ts::ALPHA(curcol) );
    colsmap[curcolor] = curcol;
    pp->custom_value_text.set(CONSTWSTR("G: ")).append_as_int(ts::GREEN(curcol));
    getengine().redraw();
    updatetext();
    return true;
}
bool dialog_colors_c::hslider_b(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    curcol = ts::ARGB<int>(ts::RED(curcol), ts::GREEN(curcol), ts::lround(pp->value * 255.0), ts::ALPHA(curcol));
    colsmap[curcolor] = curcol;
    pp->custom_value_text.set(CONSTWSTR("B: ")).append_as_int(ts::BLUE(curcol));
    getengine().redraw();
    updatetext();
    return true;
}
bool dialog_colors_c::hslider_a(RID, GUIPARAM p)
{
    gui_hslider_c::param_s *pp = (gui_hslider_c::param_s *)p;
    curcol = ts::ARGB<int>(ts::RED(curcol), ts::GREEN(curcol), ts::BLUE(curcol), ts::lround(pp->value * 255.0));
    colsmap[curcolor] = curcol;
    pp->custom_value_text.set(CONSTWSTR("A: ")).append_as_int(ts::ALPHA(curcol));
    getengine().redraw();
    updatetext();
    return true;
}

bool dialog_colors_c::updatetext(RID, GUIPARAM p)
{
    if (p)
    {
        set_edit_value(CONSTASTR("str"), curcolastext());
    } else
    {
        DEFERRED_UNIQUE_CALL(0,DELEGATE(this, updatetext), as_param(1));
    }
    return true;
}

/*virtual*/ void dialog_colors_c::on_confirm()
{
    ts::abp_c bp;
    if (ts::g_fileop->load(colsfn, bp))
    {
        ts::abp_c *cols = bp.get(CONSTASTR("colors"));

        for (const ts::str_c &cn : colorsarr)
        {
            ts::str_c ct = make_color(colsmap[cn]);
            ct.case_down();
            ts::abp_c&bc = cols->set(cn);
            bc.set_value( ct );

        }

    }

    ts::make_path( ts::fn_get_path(colsfn), 0 );

    ts::str_c stor = bp.store();
    ts::buf0_c b; b.append_buf( stor.cstr(), stor.get_length() );
    b.save_to_file( colsfn );

    g_app->load_theme(cfg().theme());

}

/*virtual*/ ts::ivec2 dialog_colors_c::get_min_size() const
{
    return ts::ivec2(500, 400);
}
/*virtual*/ bool dialog_colors_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_DRAW && rid == getrid())
    {
        super::sq_evt(qp, rid, data);

        ts::irect cl = get_client_area();
        cl.rb.x = cl.lt.x + 32 * 7;
        cl.lt.y = cl.rb.y - 32 * 3;

        draw_chessboard(getengine(), cl, ts::ARGB(128,128,128), ts::ARGB(228,228,228));

        cl.lt += ts::ivec2(10);
        cl.rb -= ts::ivec2(10);

        getengine().begin_draw();
        getengine().draw( cl, ts::ALPHA(curcol) == 255 ? curcol : ts::PREMULTIPLY(curcol) );
        getengine().end_draw();

        return true;
    }

    return super::sq_evt(qp, rid, data);
}

















dialog_cmdlinegenerator_c::dialog_cmdlinegenerator_c( initial_rect_data_s &data ) :gui_isodialog_c( data )
{
}

dialog_cmdlinegenerator_c::~dialog_cmdlinegenerator_c()
{
}


/*virtual*/ void dialog_cmdlinegenerator_c::created()
{
    set_theme_rect( CONSTASTR( "main" ), false );
    super::created();
    tabsel( CONSTASTR( "1" ) );
}
/*virtual*/ void dialog_cmdlinegenerator_c::getbutton( bcreate_s &bcr )
{
    if ( bcr.tag > 0 )
        return;

    bcr.tag = 1;
    super::getbutton( bcr );
}

bool dialog_cmdlinegenerator_c::passt( const ts::wstr_c &p, bool )
{
    password = p;
    ts::wstr_c ep = enc();
    set_edit_value( CONSTASTR("ep"), ep );

    ts::wstr_c t( ts::get_exe_full_name(), CONSTWSTR( " profile " ) );
    t.append( ts::fn_get_name( prf().get_path() ) ).append( CONSTWSTR( " password " ) ).append( ep ); //.append( CONSTWSTR( "</l>" ) );
    set_edit_value( CONSTASTR( "cmdl" ), t );

    return true;
}

/*virtual*/ void dialog_cmdlinegenerator_c::tabselected( ts::uint32 /*mask*/ )
{
    passt(ts::wstr_c(), false);
}

/*virtual*/ int dialog_cmdlinegenerator_c::additions( ts::irect & )
{
    descmaker dm( this );
    dm << 1;

    dm().page_header( TTT("Command line generator for encrypted profiles. You can use this command line to run [appname] with specified profile and password.[br][b]IMPORTANT!!![/b][br]Password will be encrypted by machine-specific key and will be valid only for current computer.",490) );

    dm().textfield( ts::wstr_c(TTT("Password to encrypt",491)), ts::wsptr(), DELEGATE( this, passt ) ).passwd(true);

    dm().vspace();
    dm().rotext( TTT( "Current profile", 492 ), ts::fn_get_name( prf().get_path() ) );
    dm().rotext( TTT( "Encrypted password", 493 ), ts::wsptr() ).setname( CONSTASTR("ep") );
    dm().rotext( TTT( "Command line", 494 ), ts::wsptr() ).setname( CONSTASTR( "cmdl" ) );

    return 0;
}

ts::wstr_c dialog_cmdlinegenerator_c::enc() const
{
    ts::uint8 encpass[ 32 ];
    get_unique_machine_id( encpass, 32, SALT_CMDLINEPROFILE, false );
    ts::str_c ep = encode_string_base64( encpass, ts::to_utf8(password) );
    crypto_zero( encpass, sizeof( encpass ) );

    return ts::from_utf8(ep);
}

/*virtual*/ ts::ivec2 dialog_cmdlinegenerator_c::get_min_size() const
{
    return ts::ivec2( 540, 400 );
}

/*virtual*/ bool dialog_cmdlinegenerator_c::sq_evt( system_query_e qp, RID rid, evt_data_s &data )
{
    if ( qp == SQ_DRAW && rid == getrid() )
    {
        super::sq_evt( qp, rid, data );
        return true;
    }

    return super::sq_evt( qp, rid, data );
}
