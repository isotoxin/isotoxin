#include "isotoxin.h"


dialog_contact_props_c::dialog_contact_props_c(MAKE_ROOT<dialog_contact_props_c> &data) :gui_isodialog_c(data)
{
    open_details_tab = data.prms.details_tab;
    deftitle = title_contact_properties;
    contactue = contacts().rfind(data.prms.key);
    if (contactue->getkey().is_conference())
        deftitle = title_conference_properties;
    update();
}

dialog_contact_props_c::~dialog_contact_props_c()
{
    if (gui)
    {
        gui->delete_event( DELEGATE( this, refresh_details ) );
    }
}

/*virtual*/ ts::wstr_c dialog_contact_props_c::get_name() const
{
    return super::get_name().append(CONSTWSTR(" - ")).append(gui_dialog_c::get_name());
}


/*virtual*/ void dialog_contact_props_c::created()
{
    set_theme_rect(CONSTASTR("cprops"), false);
    super::created();
    if (open_details_tab)
        tabsel( CONSTASTR( "2" ) );
    else
        tabsel(CONSTASTR("1"));

}

/*virtual*/ ts::ivec2 dialog_contact_props_c::get_min_size() const
{
    return ts::ivec2(530, 385);
}

void dialog_contact_props_c::getbutton(bcreate_s &bcr)
{
    super::getbutton(bcr);
}

bool dialog_contact_props_c::custom_name( const ts::wstr_c & cn, bool )
{
    customname = to_utf8(cn);
    return true;
}

bool dialog_contact_props_c::comment( const ts::wstr_c &c, bool )
{
    ccomment = to_utf8(c);
    return true;
}

bool dialog_contact_props_c::greeting( const ts::wstr_c &c, bool )
{
    cgreeting = to_utf8( c );
    return true;
}

bool dialog_contact_props_c::greeting_per( const ts::wstr_c &p, bool )
{
    cgreeting_per = p.as_int() * 60;
    return true;
}

bool dialog_contact_props_c::tags_handler(const ts::wstr_c &ht, bool )
{
    tags.split<char>( to_utf8(ht), ',' );
    tags.trim();
    tags.sort(true);
    return true;
}

bool dialog_contact_props_c::regexp_handler( const ts::wstr_c &s, bool )
{
    regexp = s;
    return true;
}
bool dialog_contact_props_c::keywords_handler( const ts::wstr_c &s, bool )
{
    keywords = s;
    return true;
}

void dialog_contact_props_c::history_settings( const ts::str_c& v )
{
    keeph = (keep_contact_history_e)v.as_uint();
    set_combik_menu(CONSTASTR("history"), gethistorymenu());
}

void dialog_contact_props_c::aaac_settings( const ts::str_c& v )
{
    aaac = (auto_accept_audio_call_e)v.as_uint();
    set_combik_menu(CONSTASTR("aaac"), getaacmenu());
}

void dialog_contact_props_c::imb_settings( const ts::str_c&v )
{
    imb = v.as_uint();
    set_combik_menu( CONSTASTR( "imn" ), imnmenu() );
}

#define USE_GLOBAL_SETTING TTT("Use global setting",226)

menu_c dialog_contact_props_c::gethistorymenu()
{
    menu_c m;
    m.add( USE_GLOBAL_SETTING, (keeph == KCH_DEFAULT) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_DEFAULT) );
    m.add( TTT("Always save",227), (keeph == KCH_ALWAYS_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_ALWAYS_KEEP) );
    m.add( TTT("Never save",228), (keeph == KCH_NEVER_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_NEVER_KEEP) );

    return m;
}

menu_c dialog_contact_props_c::getaacmenu()
{
    menu_c m;
    m.add(TTT("Don't accept",285), (aaac == AAAC_NOT) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_NOT));
    m.add(TTT("Accept and mute microphone",286), (aaac == AAAC_ACCEPT_MUTE_MIC) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_ACCEPT_MUTE_MIC));
    m.add(TTT("Accept and don't mute microphone",287), (aaac == AAAC_ACCEPT) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_ACCEPT));

    return m;
}

menu_c dialog_contact_props_c::getmhtmenu()
{
    menu_c m;
    m.add( TTT( "Do not handle", 436 ), ( mh == MH_NOT ) ? MIF_MARKED : 0, DELEGATE( this, msghandler_m ), ts::amake<char>( MH_NOT ) );
    m.add( TTT( "Run external command and pass message as parameter", 437 ), ( mh == MH_AS_PARAM ) ? MIF_MARKED : 0, DELEGATE( this, msghandler_m ), ts::amake<char>( MH_AS_PARAM ) );
    m.add( TTT( "Run external command and pass message via file", 438 ), ( mh == MH_VIA_FILE ) ? MIF_MARKED : 0, DELEGATE( this, msghandler_m ), ts::amake<char>( MH_VIA_FILE ) );
    return m;
}

menu_c dialog_contact_props_c::imnmenu()
{
    menu_c m;
    m.add( USE_GLOBAL_SETTING, ( imb == IMB_DEFAULT ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_DEFAULT ) );
    m.add( TTT("Don't notify",460), ( imb == IMB_DONT_NOTIFY ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_DONT_NOTIFY ) );
    m.add( TTT("Suppress only intrusive behaviour",461), ( imb == IMB_SUPPRESS_INTRUSIVE_BEHAVIOUR ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_SUPPRESS_INTRUSIVE_BEHAVIOUR ) );
    m.add( TTT("Forced desktop notification and intrusive behaviour",462), ( imb == IMB_ALL ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_ALL ) );
    m.add( TTT("Forced only desktop notification",463), ( imb == IMB_DESKTOP_NOTIFICATION ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_DESKTOP_NOTIFICATION ) );
    m.add( TTT("Forced only intrusive behaviour",464), ( imb == IMB_INTRUSIVE_BEHAVIOUR ) ? MIF_MARKED : 0, DELEGATE( this, imb_settings ), ts::amake<char>( IMB_INTRUSIVE_BEHAVIOUR ) );
    return m;
}

void dialog_contact_props_c::msghandler_m( const ts::str_c&v )
{
    mh = (msg_handler_e)v.as_uint();
    set_combik_menu( CONSTASTR( "mht" ), getmhtmenu() );

}

bool dialog_contact_props_c::msghandler_h( const ts::wstr_c & t, bool )
{
    msghandler = t;
    return true;
}

bool dialog_contact_props_c::msghandler_p_h( const ts::wstr_c & t, bool )
{
    msghandler_p = t;
    return true;
}

/*virtual*/ int dialog_contact_props_c::additions(ts::irect &edges)
{
    descmaker dm(this);

    if (contactue)
    {
        customname = contactue->getkey().is_conference() ? contactue->get_name() : contactue->get_customname();
        ccomment = contactue->get_comment();
        cgreeting = contactue->get_greeting();
        cgreeting_per = contactue->get_greeting_period();
        keeph = contactue->get_keeph();
        aaac = contactue->get_aaac();
        imb = contactue->get_imnb();
        tags = contactue->get_tags();

        mh = contactue->get_mhtype();

        ts::wstrings_c h;
        h.qsplit( contactue->get_mhcmd() );

        if (conference_s *c = contactue->find_conference())
        {
            int i = c->keywords.find_pos( '\1' );
            if (i >= 0)
            {
                regexp = c->keywords.substr( 0, i );
                keywords = c->keywords.substr( i + 1 );
            }
        }

        if ( h.size() )
        {
            msghandler = h.get( 0 );
            h.remove_slow( 0 );
            msghandler_p = h.join(' ');
        }
        if ( msghandler_p.is_empty() )
            msghandler_p = CONSTWSTR("<param>");


        dm << 1;

        ts::str_c n = contactue->get_name();
        text_adapt_user_input(n);

        dm().page_caption(from_utf8(n));

        if ( contactue->getkey().is_conference() )
        {
            ts::wstr_c ttt(TTT("Conference name",506));
            if (contactue->get_conference_permissions() & CP_CHANGE_NAME)
                dm().textfield( ttt, from_utf8( customname ), DELEGATE( this, custom_name ) );
            else
                dm().rotext( ttt, from_utf8( customname ) );

            dm().vspace();
            dm().rotext( TTT("Conference ID",507), from_utf8( contactue->get_pubid() ) );
        } else
        {
            dm().textfield( TTT( "Custom name", 229 ), from_utf8( customname ), DELEGATE( this, custom_name ) )
                .focus( true );
        }

        dm().vspace();

        dm().combik(TTT("Message history",230)).setmenu(gethistorymenu()).setname(CONSTASTR("history"));
        dm().vspace();

        if (!contactue->getkey().is_conference())
        {
            dm().combik( TTT( "Auto accept audio calls", 288 ) ).setmenu( getaacmenu() ).setname( CONSTASTR( "aaac" ) );
            dm().vspace();
        }

        dm().textfieldml(TTT("Tags (comma-separated list of phrases/words)", 53), from_utf8(tags.join(CONSTASTR(", "))), DELEGATE(this, tags_handler), 3).focus(true).setname(CONSTASTR("tags"));

        dm << 2;
        if (contactue->getkey().is_conference())
        {
            dm().list( ts::wsptr(), ts::wsptr(), -250 ).setname( CONSTASTR( "det" ) );
        } else
        {
            dm().list( ts::wsptr(), ts::wsptr(), -250 ).setname( CONSTASTR( "list" ) );
        }

        dm << 4;
        dm().textfieldml(ts::wsptr(), from_utf8(ccomment), DELEGATE(this, comment), 12).focus(true);

        dm << 8;

        dm().label( genmhi() ).setname( CONSTASTR( "mhi" ) );
        dm().vspace();

        dm().combik( TTT("Message handler",439) ).setmenu( getmhtmenu() ).setname( CONSTASTR( "mht" ) );
        dm().file( TTT("External command",443), CONSTWSTR(""), msghandler, DELEGATE( this, msghandler_h ) );
        dm().textfield( TTT("External command param",444), msghandler_p, DELEGATE( this, msghandler_p_h ) );

        dm << 16;

        dm().combik( TTT("On incoming message",465) ).setmenu( imnmenu() ).setname( CONSTASTR( "imn" ) );
        dm().vspace();

        dm << 32;

        ts::wstr_c ctl;
        dm().textfield( ts::wstr_c(), ts::wmake( cgreeting_per/60 ), DELEGATE( this, greeting_per ) ).setname( CONSTASTR( "gper" ) ).width( 50 ).subctl( 1, ctl );
        dm().label( TTT("Greeting will be sent every time contact appears online, but only once per $ minutes",486) / ctl );

        dm().vspace();
        dm().textfieldml( ts::wsptr(), from_utf8( cgreeting ), DELEGATE( this, greeting ), 10 ).focus( true );

        if (contactue->getkey().is_conference())
        {
            dm << 64;
            dm().list( ts::wsptr(), ts::wsptr(), -250 ).setname( CONSTASTR( "list" ) );

            dm << 128;
            dm().label( TTT("Regular expression (egrep syntax)",511) );
            dm().textfieldml( ts::wsptr(), regexp, DELEGATE( this, regexp_handler ), 5 ).focus( true );

            dm().label( TTT("Keywords (comma separated phrases/words per line; each line represents a separate rule)",512) );
            dm().textfieldml( ts::wsptr(), keywords, DELEGATE( this, keywords_handler ), 5 );
        }
    }

    menu_c m;
    m.add( TTT("Settings",369), 0 , TABSELMI(1) );

    if (!contactue->getkey().is_conference())
    {
        m.add( TTT( "Notification", 466 ), 0, TABSELMI( 16 ) );
    } else
        m.add( TTT("Members",508),  0, TABSELMI( 64 ) );

    m.add( TTT( "Details", 370 ), open_details_tab ? MIF_MARKED : 0, TABSELMI( 2 ) );

    if (!contactue->getkey().is_conference())
    {
        m.add( TTT( "Processing", 440 ), 0, TABSELMI( 8 ) );
        m.add( TTT( "Greeting", 485 ), 0, TABSELMI( 32 ) );
    } else
    {
        m.add( TTT("Keywords",513), 0, TABSELMI( 128 ) );
    }
    m.add( TTT( "Comment", 371 ), 0, TABSELMI( 4 ) );

    gui_htabsel_c &tab = MAKE_CHILD<gui_htabsel_c>(getrid(), m);
    edges = ts::irect(0, tab.get_min_size().y, 0, 0);


    return 1;
}

ts::wstr_c dialog_contact_props_c::genmhi()
{
    ts::wstr_c t( CONSTWSTR("<p=c>") );
    t.append( TTT("Enter full path to executable. It will be called on every incoming message from this contact. <param> will be replaced with either $ message or filename of just saved message.",445) / ts::wstr_c(CONSTWSTR("<a href=\"https://en.wikipedia.org/wiki/Percent-encoding\">percent-encoding</a>")) );
    return t;
}

ts::wstr_c dialog_contact_props_c::buildmh()
{
    ts::wstr_c t( msghandler );
    if ( t.find_pos( ' ' ) >= 0 )
        t.trim().insert( 0, '\"' ).append_char('\"');
    t.append_char( ' ' ).append( msghandler_p );
    return t;
}

/*virtual*/ bool dialog_contact_props_c::sq_evt(system_query_e qp, RID rid, evt_data_s &d)
{
    if (SQ_RECT_CHANGED == qp)
    {
        if (!lst_ajust_height.is_empty())
        {
            set_list_height( lst_ajust_height, getprops().size().y - 135 );
        }
    }

    if (super::sq_evt(qp, rid, d)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

/*virtual*/ void dialog_contact_props_c::on_confirm()
{
    if (contactue && contacts().find(contactue->getkey()) != nullptr)
    {
        bool changed = false;

        if (contactue->getkey().is_conference())
        {
            if (contactue->get_name() != customname && (contactue->get_conference_permissions() & CP_CHANGE_NAME) != 0)
            {
                changed = true;
                contactue->set_name( customname );
            }

            if (conference_s *c = contactue->find_conference())
            {
                ts::wstr_c newkeywors( regexp, CONSTWSTR( "\1" ), keywords );
                c->change_keywords( newkeywors );
            }

        } else
        {
            if (contactue->get_customname() != customname)
            {
                changed = true;
                contactue->set_customname( customname );
            }
            if (contactue->get_greeting() != cgreeting)
            {
                changed = true;
                contactue->set_greeting( cgreeting );
            }
            if (contactue->get_greeting_period() != cgreeting_per)
            {
                changed = true;
                contactue->set_greeting_period( cgreeting_per );
            }
            if (contactue->get_aaac() != aaac)
            {
                changed = true;
                contactue->set_aaac( aaac );
            }
            if (contactue->get_imnb() != imb)
            {
                changed = true;
                contactue->set_imnb( imb );
            }
            if (contactue->get_mhtype() != mh)
            {
                changed = true;
                contactue->set_mhtype( mh );
            }
            if (contactue->get_mhcmd() != buildmh())
            {
                changed = true;
                contactue->set_mhcmd( buildmh() );
            }
        }

        if (contactue->get_comment() != ccomment)
        {
            changed = true;
            contactue->set_comment(ccomment);
        }
        if (contactue->get_keeph() != keeph)
        {
            changed = true;
            contactue->set_keeph(keeph);
        }
        if (contactue->get_tags() != tags)
        {
            changed = true;
            contactue->set_tags(tags);
            contacts().rebuild_tags_bits();
        }

        if (changed)
        {
            if (contactue->getkey().is_conference())
            {
                conference_s *c = contactue->find_conference();
                c->update_comment();
                c->update_tags();
                c->update_keeph();
                if (c->update_name())
                    if (active_protocol_c *ap = prf().ap( contactue->getkey().protoid ))
                        ap->rename_conference( contactue->getkey().gidcid(), c->name );

            } else
                prf().dirtycontact(contactue->getkey());
            contactue->reselect();
        }
    }

    super::on_confirm();
}


void dialog_contact_props_c::update()
{
    if (contactue)
    {
        if (contactue->getkey().is_conference())
        {
            cinfo_s &inf = getinfo( contactue );
            inf.cldets.parse( contactue->get_details() );
        }

        contactue->subiterate( [&]( contact_c *c )
        {
            ts::str_c par;

            cinfo_s &inf = getinfo( c );

            if (const avatar_s *ava = c->get_avatar())
            {
                inf.bmp = ava;
            }
            else
            {
                if (!inf.btmp)
                    inf.btmp.reset( TSNEW( ts::bitmap_c ) );

                const theme_image_s *icon = g_app->preloaded_stuff().icon[c->get_gender()];
                inf.bmp = inf.btmp.get();
                ts::irect rect = icon->rect;

                if (inf.btmp->info().sz != rect.size())
                    inf.btmp->create_ARGB( rect.size() );

                inf.btmp->copy( ts::ivec2( 0 ), rect.size(), icon->dbmp->extbody(), rect.lt );


            }

            inf.cldets.parse( c->get_details() );
        } );
    }
}


ts::uint32 dialog_contact_props_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &c)
{
    if (contactue && (contactue->subpresent(c.contact->getkey()) || c.contact == contactue))
    {
        if (lstrid && lstrid.call_ctxmenu_present())
            return 0;

        if (gui->mtrack( MTT_RESIZE | MTT_MOVE | MTT_SBMOVE ))
            return 0;


        update();
        fill_list();
    }

    return 0;
}

namespace customel
{
    class gui_lli_c;
}

template<> struct MAKE_CHILD<customel::gui_lli_c> : public _PCHILD(customel::gui_lli_c), public gui_listitem_params_s
{
    ts::astrings_c links;
    ts::buf_c qrs;
    MAKE_CHILD(ts::astrings_c &&links_, ts::buf_c &&qrs_, RID parent_, const ts::wstr_c &text_, const ts::str_c &param_)
    {
        parent = parent_;
        text = text_;
        param = param_;
        addmarginleft = 5;
        addmarginleft_icon = 3;
        links = std::move(links_);
        qrs = std::move(qrs_);
    }
    ~MAKE_CHILD();

    MAKE_CHILD &operator<<(const ts::bitmap_c *_icon) { icon = _icon; return *this; }
    MAKE_CHILD &operator<<(GETMENU_FUNC _gm) { gm = _gm; return *this; }
    //MAKE_CHILD &threct(const ts::asptr&thr) { themerect = thr; return *this; }
};

namespace customel
{
    class gui_lli_c : public gui_listitem_c
    {
        typedef gui_listitem_c super;
        ts::astrings_c links;
        ts::buf_c qrs;
        int clicklink = -1;
        system_query_e clicka = SQ_NOP;
        int flashing = 0;
        void flash()
        {
            flashing = 4;
            flash_pereflash(RID(), nullptr);
        }

        bool flash_pereflash(RID, GUIPARAM)
        {
            --flashing;
            if (flashing > 0) DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, flash_pereflash), nullptr);

            textrect.make_dirty();
            getengine().redraw();

            if (flashing == 0)
                clicklink = -1;

            return true;
        }

    public:

        gui_lli_c(MAKE_CHILD<gui_lli_c> &data):gui_listitem_c(data, data)
        {
            links = std::move(data.links);
            qrs = std::move(data.qrs);
        }
        ~gui_lli_c()
        {
            if (gui)
                gui->delete_event(DELEGATE(this, flash_pereflash));
        }

        void set( ts::astrings_c &&links_, ts::buf_c &&qrs_, const ts::wstr_c &text_, const ts::str_c &param_, const ts::bitmap_c *icon_ )
        {
            links = std::move(links_);
            qrs = std::move(qrs_);
            super::set( text_, param_, icon_ );
            getengine().redraw();
        }

        /*virtual*/ void get_link_prolog(ts::wstr_c &r, int linknum) const override
        {
            r.clear();
            if (linknum == clicklink)
            {
                if (popupmenu || flashing > 0)
                {
                    r = maketag_mark<ts::wchar>(0 != (flashing & 1) ? gui->selection_bg_color_blink : gui->selection_bg_color);
                    appendtag_color(r, gui->selection_color);
                }
                return;
            }

            if (linknum == overlink)
                r.set(CONSTWSTR("<u>"));

        }
        /*virtual*/ void get_link_epilog(ts::wstr_c &r, int linknum) const override
        {
            r.clear();
            if (linknum == clicklink)
            {
                if (popupmenu || flashing > 0)
                {
                    r = CONSTWSTR("</color></mark>");
                }
                return;
            }
            if (linknum == overlink)
                r.set(CONSTWSTR("</u>"));
        }

        void ctx_copy(const ts::str_c &cc)
        {
            ts::str_c c(cc);
            text_convert_to_bbcode(c);
            ts::set_clipboard_text( from_utf8(c) );
            flash();
        }

        void ctx_qrcode(const ts::str_c &cc)
        {
            dialog_msgbox_c::mb_qrcode(from_utf8(cc)).summon(true);
        }

        /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data)
        {
            if (rid != getrid())
            {
                if (popupmenu && popupmenu->getrid() == rid)
                {
                    if (SQ_POPUP_MENU_DIE == qp)
                    {
                        textrect.make_dirty();
                        if (flashing == 0)
                        {
                            MODIFY(*this).highlight(false);
                            clicklink = -1;
                        }
                        getengine().redraw();
                    }
                }

                return super::sq_evt(qp, rid, data);
            }

            switch(qp)
            {
            case SQ_MOUSE_LDOWN:
                if (popupmenu.expired())
                {
                    clicklink = -1;
                    flashing = 0;
                    clicka = SQ_NOP;
                    if (overlink >= 0)
                        clicka = SQ_MOUSE_LUP, clicklink = overlink;
                }
                break;
            case SQ_MOUSE_RDOWN:
                if (popupmenu.expired())
                {
                    clicklink = -1;
                    flashing = 0;
                    clicka = SQ_NOP;
                    if (overlink >= 0)
                        clicka = SQ_MOUSE_RUP, clicklink = overlink;
                }
                break;
            case SQ_CTX_MENU_PRESENT:
                if (flashing)
                    data.getsome.handled = true;
                break;
            }

            if (SQ_MOUSE_LUP == qp || SQ_MOUSE_RUP == qp)
            {
                if (clicka == qp && overlink == clicklink && ASSERT(overlink < links.size()))
                {
                    menu_c mnu;
                    mnu.add(loc_text(loc_copy), 0, DELEGATE(this, ctx_copy), links.get(overlink));
                    if (qrs.get_bit(overlink)) mnu.add(loc_text(loc_qrcode), 0, DELEGATE(this, ctx_qrcode), links.get(overlink));

                    popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), mnu);
                    popupmenu->leech(this);
                    textrect.make_dirty();
                    getengine().redraw();
                }
                clicka = SQ_NOP;
                return true;
            }
            return super::sq_evt(qp, rid, data);
        }

    };
}

MAKE_CHILD<customel::gui_lli_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
    //get().set_autoheight();
    get().set_text(text);
    get().set_gm(gm);
}


void dialog_contact_props_c::add_det(RID lstctl, int index, contact_c *c)
{
    ts::str_c par;

    cinfo_s &inf = getinfo(c);

    ts::wstr_c desc;
    ts::astrings_c vals;
    ts::buf_c qrs;

    active_protocol_c *ap = prf().ap( c->getkey().protoid );

    auto addl = [&]( const ts::wsptr &fn, const ts::asptr &v, bool empty_as_unknown, bool ee, bool qr, ts::aint link_i )
    {
        if (ee)
            desc.append(CONSTWSTR("<ee>"));
        appendtag_color( desc, get_default_text_color( 2 ) ).append( fn ).append( CONSTWSTR( ":</color> " ) );
        if (v.l > 0)
        {
            if (link_i >= 0)
            {
                desc.append(CONSTWSTR("<cstm=a")).append_as_num(link_i).append_char('>');
                if (qr) qrs.set_bit(link_i, true);
            }
            desc.append(ts::from_utf8(v));
            if (link_i >= 0) desc.append(CONSTWSTR("<cstm=b")).append_as_num(link_i).append_char('>');

        } else if (empty_as_unknown)
        {
            desc.append(CONSTWSTR("<l>")).append(TTT("unknown",362)).append(CONSTWSTR("</l>"));
        }
        desc.append(CONSTWSTR("<br>"));

    };

    auto extractitems = [](ts::astrings_c &pubids, const ts::json_c &ids)
    {
        if (ids.is_string())
        {
            pubids.add( ids.as_string() );
        } else if (ids.is_array())
        {
            ids.iterate([&](const ts::str_c&, const ts::json_c &id){
                pubids.add( id.as_string() );
            });
        }
    };

    if ( !c->get_options().is(contact_c::F_SYSTEM_USER) && !c->getkey().is_conference() )
    {
        ts::str_c temp( c->get_name() );
        text_adapt_user_input( temp );
        addl( TTT( "User name", 364 ), temp, false, true, false, vals.add( temp ) );

        if (!c->get_statusmsg().is_empty() || c->get_state() != CS_UNKNOWN) // don't show empty status message for unknown contacts
        {
            temp = c->get_statusmsg();
            text_adapt_user_input( temp );
            addl( TTT( "User status message", 365 ), temp, false, true, false, vals.add( temp ) );
        }
    }

    //desc.insert(desc.get_length()-1, CONSTWSTR("=3"));

    inf.cldets.iterate([&](const ts::str_c &dname, const ts::json_c &v) {

        if ( dname.equals( CONSTASTR( CDET_CLIENT ) ) )
        {
            ts::astrings_c lst;
            extractitems( lst, v );
            lst.kill_dups_and_sort();
            for ( const ts::str_c &idx : lst )
                addl( TTT( "Client", 366 ), idx, true, false, false, vals.add( idx ) );
        } else if (dname.equals(CONSTASTR(CDET_PUBLIC_ID)))
        {
            ts::astrings_c lst;
            extractitems( lst, v);
            for( const ts::str_c &idx : lst )
                addl( CONSTWSTR("<id>"), idx, true, true, true, vals.add(idx));
        } else if (dname.equals( CONSTASTR( CDET_PUBLIC_UNIQUE_ID ) ))
        {
            ts::astrings_c lst;
            extractitems( lst, v );
            for (const ts::str_c &idx : lst)
                addl( TTT("Unique ID",509), idx, true, true, true, vals.add( idx ) );
        } else if (dname.equals(CONSTASTR(CDET_PUBLIC_ID_BAD)))
        {
            ts::astrings_c lst;
            extractitems( lst, v);
            for  ( const ts::str_c &idx : lst )
                addl( CONSTWSTR( "<id>" ), maketag_color<char>(get_default_text_color(3)) + idx + CONSTASTR("</color>"), true, true, true, vals.add(idx));
        } else if (dname.equals(CONSTASTR(CDET_DNSNAME)))
        {
            ts::astrings_c lst;
            extractitems( lst, v );
            for ( const ts::str_c &idx : lst )
                addl(TTT("DNS name",367), idx, true, true, true, vals.add(idx));
        } else if (dname.equals(CONSTASTR(CDET_EMAIL)))
        {
            ts::astrings_c lst;
            extractitems( lst, v );
            for ( const ts::str_c &idx : lst )
                addl(TTT("Email",368), idx, true, true, true, vals.add(idx));
        } else if (dname.equals( CONSTASTR( CDET_CONN_INFO ) ))
        {
            ts::astrings_c lst;
            extractitems( lst, v );
            for (const ts::str_c &idx : lst)
                addl( TTT("Connection info",518), idx, true, true, true, vals.add( idx ) );
        }

    });

    //if (something)
        //desc.insert(desc.get_length()-1, CONSTWSTR("=3"));

    ts::wstr_c id( CONSTWSTR( "ID" ) );
    if (ap)
    {
        ts::wstr_c id_ = from_utf8( ap->get_infostr( IS_IDNAME ) );
        if ( !id_.is_empty() )
            id = id_;

        if (!contactue->getkey().is_conference() || contactue == c)
        {
            addl( loc_text( loc_connection_name ), ap->get_name(), false, false, false, vals.add( ap->get_name() ) );
            addl( loc_text( loc_module ), ap->get_infostr( IS_PROTO_DESCRIPTION_WITH_TAG ), false, false, false, vals.add( ap->get_infostr( IS_PROTO_DESCRIPTION_WITH_TAG ) ) );
        }
    }

    if (!c->get_options().is( contact_c::F_SYSTEM_USER ))
    {
        if ( c->getkey().is_conference() && c->get_state() == CS_OFFLINE )
        {
            addl( loc_text( loc_state ), to_utf8( text_contact_state( get_default_text_color( 0 ), get_default_text_color( 1 ), CS_ROTTEN ) ), true, false, false, -1 );

        } else
            addl( loc_text( loc_state ), to_utf8( text_contact_state( get_default_text_color( 0 ), get_default_text_color( 1 ), c->get_state() ) ), true, false, false, -1 );
    }

    desc.replace_all( CONSTWSTR( "<id>" ), id );

    if (index >= HOLD(lstctl).engine().children_count())
    {
        additem:
        MAKE_CHILD<customel::gui_lli_c>( std::move( vals ), std::move( qrs ), lstctl, desc, par ) << (const ts::bitmap_c *)(inf.bmp);
    } else
    {
        rectengine_c *itme = HOLD( lstctl ).engine().get_child( index );
        if (!itme) goto additem;
        customel::gui_lli_c *itm = ts::ptr_cast<customel::gui_lli_c *>(&itme->getrect());
        itm->set( std::move( vals ), std::move( qrs ), desc, par, (const ts::bitmap_c *)(inf.bmp) );

    }
}

void dialog_contact_props_c::fill_list()
{
    if (contactue)
    {
        if (RID lst = find( CONSTASTR( "list" ) ))
        {
            HOLD( lst ).engine().trunc_children( contactue->subcount() );

            int index = 0;
            contactue->subiterate( [&]( contact_c *c ) {
                add_det( lst, index, c );
                ++index;
            } );

            lstrid = lst;
            lst_ajust_height.set( CONSTASTR( "list" ) );

        }

        if (contactue->getkey().is_conference())
        {
            if (RID lst = find( CONSTASTR( "det" ) ))
            {
                HOLD( lst ).engine().trunc_children( 1 );
                add_det( lst, 0, contactue );

                lstrid = lst;
                lst_ajust_height.set( CONSTASTR( "det" ) );
            }
        }
    }
}

void dialog_contact_props_c::tags_menu_handler(const ts::str_c&h)
{
    ts::aint hi = tags.find(h);
    if (hi >= 0)
    {
        tags.remove_slow(hi);
    } else
    {
        tags.add(h);
        tags.sort(true);
    }
    set_edit_value(CONSTASTR("tags"), from_utf8(tags.join(CONSTASTR(", "))) );
}

void dialog_contact_props_c::tags_menu(menu_c &m, ts::aint )
{
    ts::astrings_c ctags( tags );
    ctags.add(contacts().get_all_tags());
    ctags.kill_dups_and_sort(true);

    for (const ts::str_c &h : ctags)
    {
        m.add( ts::from_utf8(h), tags.find(h) >= 0 ? MIF_MARKED : 0, DELEGATE(this, tags_menu_handler), h );
    }
}

/*virtual*/ void dialog_contact_props_c::tabselected(ts::uint32 mask)
{
    enable_refresh_details( false );
    lst_ajust_height.clear();
    lstrid = RID();

    if (mask & 1)
        if (RID e = find(CONSTASTR("tags")))
            HOLD(e).as<gui_textedit_c>().ctx_menu_func = DELEGATE(this, tags_menu);

    if (mask & (2|64))
        fill_list(), enable_refresh_details(true);

}

bool dialog_contact_props_c::refresh_details( RID, GUIPARAM )
{
    if (do_refresh_details)
        DEFERRED_UNIQUE_CALL( 2.0, DELEGATE( this, refresh_details ), 0 );

    if (contactue->getkey().is_conference())
    {
        if (contactue->get_state() == CS_ONLINE)
        {
            if (active_protocol_c *ap = prf().ap( contactue->getkey().protoid ))
                ap->refresh_details( contactue->getkey() );
        }
    } else
    {
        contactue->subiterate( [&]( contact_c *c ) {

            if (active_protocol_c *ap = prf().ap( c->getkey().protoid ))
                ap->refresh_details( c->getkey() );
        } );
    }

    return true;
}
void dialog_contact_props_c::enable_refresh_details( bool f )
{
    do_refresh_details = f;
    if (do_refresh_details)
        DEFERRED_UNIQUE_CALL( 2.0, DELEGATE(this, refresh_details), 0 );
}





