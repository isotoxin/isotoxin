#include "isotoxin.h"


dialog_addcontact_c::dialog_addcontact_c(MAKE_ROOT<dialog_addcontact_c> &data) :gui_isodialog_c(data), inparam(data.prms)
{
    deftitle = title_new_contact;
}

dialog_addcontact_c::~dialog_addcontact_c()
{
    if ( gui )
    {
        gui->delete_event( DELEGATE( this, hidectl ) );
        gui->delete_event( DELEGATE( this, resolve_anm ) );
    }
}

/*virtual*/ ts::wstr_c dialog_addcontact_c::get_name() const
{
    if (!inparam.title.is_empty()) return inparam.title;
    return __super::get_name();
}


/*virtual*/ void dialog_addcontact_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
    if (!networks_available)
        ctlenable(CONSTASTR("dialog_button_1"), false);
}

/*virtual*/ ts::ivec2 dialog_addcontact_c::get_min_size() const
{
    if (!inparam.pubid.is_empty()) return ts::ivec2(510, 310);
    return ts::ivec2(510, 360);
}

void dialog_addcontact_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

void dialog_addcontact_c::network_selected( const ts::str_c &prm )
{
    apid = prm.as_int();
    set_combik_menu(CONSTASTR("networks"), networks());
    if (invitemessage_autogen.equals(from_utf8(invitemessage)))
    {
        set_edit_value( CONSTASTR("msg"), rtext() );
        ctldesc( CONSTASTR( "idfield" ), idname());
    }
}

menu_c dialog_addcontact_c::networks()
{
    networks_available = false;
    menu_c nm;
    prf().iterate_aps([&](const active_protocol_c &ap)
    {
        int mif = 0;
        if (ap.get_current_state() != CR_OK)
            mif |= MIF_DISABLED;
        else 
        {
            if ( apid == 0 )
            {
                apid = ap.getid();
                mif |= MIF_MARKED;
            }
            else if ( apid == ap.getid() )
                mif |= MIF_MARKED;
        }

        nm.add(from_utf8(ap.get_name()), mif, DELEGATE(this, network_selected), ts::amake(ap.getid()));

        if (0 == (mif & MIF_DISABLED))
            networks_available = true;
    });
    return nm;
}

ts::wstr_c dialog_addcontact_c::rtext()
{
    invitemessage_autogen = loc_text( loc_please_authorize );
    ts::wstr_c n;
    if ( active_protocol_c *ap = prf().ap(apid) )
    {
        if ( 0 == (ap->get_features() & PF_AUTH_NICKNAME ) )
        {
            ts::str_c nn;
            if (ap->get_uname().is_empty())
                nn = contacts().get_self().get_name();
            else
                nn = ap->get_uname();

            text_convert_from_bbcode(nn);
            text_remove_tags(nn);
            n = from_utf8(nn);
        }
    }
    if (!n.is_empty())
    {
        n.append(CONSTWSTR(": "));
        invitemessage_autogen.insert(0,n);
    }
    return invitemessage_autogen;
}

ts::wstr_c dialog_addcontact_c::idname() const
{
    ts::wstr_c _idname( TTT( "Public ID", 67 ) );
    if ( active_protocol_c *ap = prf().ap( apid ) )
    {
        ts::str_c in = ap->get_infostr( IS_IDNAME );
        if ( !in.is_empty() && !in.equals_ignore_case( CONSTASTR( "id" ) ) )
            _idname.append( CONSTWSTR( " (" ) ).append( from_utf8( in ) ).append_char( ')' );
    }
    return _idname;
}

/*virtual*/ int dialog_addcontact_c::additions(ts::irect &)
{
    bool resend = !inparam.pubid.is_empty();
    if (resend) networks_available = true;

    descmaker dm(this);
    dm << 1;

    if (inparam.key.protoid == 0)
    {
        menu_c m = networks();
        if (networks_available)
        {
            dm().combik(loc_text(loc_network)).setname(CONSTASTR("networks")).setmenu(m);
        } else
        {
            ts::wstr_c errs(CONSTWSTR("<b>"), loc_text(loc_nonetwork));
            dm().label(errs);
        }
        dm().vspace(5);
    } else
    {
        apid = inparam.key.protoid;
    }

    dm().textfield(idname(), ts::from_utf8(inparam.pubid), DELEGATE(this, public_id_handler))
        .setname(CONSTASTR("idfield"))
        .focus(!resend)
        .readonly(resend);
    dm().vspace(5);

    if (!resend)
    {
        dm().checkb( L"", DELEGATE( this, authorization_handler ), send_authorization_request ? 1 : 0 ).setname(CONSTASTR("arq")).setmenu(
            menu_c().add( TTT("Send authorization request",456), 0, MENUHANDLER(), CONSTASTR("1") ) );
    }

    dm().textfieldml(TTT("Message",72), rtext(), DELEGATE(this, invite_message_handler))
        .setname(CONSTASTR("msg"))
        .focus(resend);
    dm().vspace(5);
    dm().hiddenlabel(L"1", true).setname(CONSTASTR("err"));

    return 0;
}

void dialog_addcontact_c::show_err( cmd_result_e r )
{
    ts::wstr_c errt;
    switch ( r )
    {
    case CR_INVALID_PUB_ID:
        errt = TTT( "Invalid Public ID!", 71 );
        break;
    case CR_MEMORY_ERROR:
        errt.set( CONSTWSTR( "memory error" ) );
        break;
    case CR_TIMEOUT:
        errt.set( CONSTWSTR( "timeout" ) );
        break;
    case CR_ALREADY_PRESENT:
        errt = TTT( "Such contact already present!", 87 );
        break;
    }

    if (errt)
    {
        if ( RID errr = find( CONSTASTR( "err" ) ) )
        {
            gui_label_simplehtml_c &ctl = HOLD( errr ).as<gui_label_simplehtml_c>();
            ctl.set_text( errt, true );
            MODIFY( errr ).visible( true );
            DEFERRED_UNIQUE_CALL( 3.0, DELEGATE( this, hidectl ), nullptr );
        }
    }
}

bool dialog_addcontact_c::resolve_anm(RID, GUIPARAM)
{
    if ( resolve_process )
    {
        DEFERRED_UNIQUE_CALL( 0.01, DELEGATE( this, resolve_anm ), nullptr );
    }

    ts::irect ir = ts::irect::from_lt_and_size( anmpos(), pa.bmp.info().sz );
    getengine().redraw( &ir );

    return true;
}

void dialog_addcontact_c::start_reslove_process()
{
    if ( resolve_process ) return;
    resolve_process = true;
    resolve_anm( RID(), nullptr );
    hidectl( RID(), nullptr );
}

ts::ivec2 dialog_addcontact_c::anmpos()
{
    int y = getprops().size().y - pa.bmp.info().sz.y;
    if ( RID m = find( CONSTASTR( "msg" ) ) )
        y = HOLD( m )( ).getprops().rect().rb.y + 5;

    ts::ivec2 p( (getprops().size().x - pa.bmp.info().sz.x) / 2, y );
    return p;
}

void dialog_addcontact_c::draw_resolve_process()
{
    pa.render();

    /*draw_data_s &dd =*/ getengine().begin_draw();
    getengine().draw( anmpos(), pa.bmp.extbody(), true );
    getengine().end_draw();

}


/*virtual*/ void dialog_addcontact_c::tabselected( ts::uint32 /*mask*/ )
{
    bool allow_unauthorized_contact = false;
    if ( active_protocol_c *ap = prf().ap( apid ) )
        allow_unauthorized_contact = ( 0 != ( ap->get_features() & PF_UNAUTHORIZED_CONTACT ) );

    ctlenable( CONSTASTR( "arq1" ), allow_unauthorized_contact );
}

bool dialog_addcontact_c::authorization_handler( RID, GUIPARAM p )
{
    send_authorization_request = p != nullptr;
    ctlenable( CONSTASTR("msg"), send_authorization_request );
    return true;
}

bool dialog_addcontact_c::invite_message_handler(const ts::wstr_c &m, bool )
{
    invitemessage = to_utf8(m);
    return true;
}

bool dialog_addcontact_c::public_id_handler( const ts::wstr_c &pid, bool )
{
    publicid = to_utf8(pid);
    return true;
}

/*virtual*/ bool dialog_addcontact_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if ( resolve_process && qp == SQ_DRAW && rid == getrid() )
    {
        __super::sq_evt( qp, rid, data );
        draw_resolve_process();
        return true;
    }

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

/*virtual*/ void dialog_addcontact_c::on_confirm()
{
    if (active_protocol_c *ap = prf().ap(apid))
    {
        bool resend = !inparam.pubid.is_empty();
        if (resend)
            ap->resend_request(inparam.key.contactid, invitemessage);
        else if (send_authorization_request)
            ap->add_contact(publicid, invitemessage);
        else
            ap->add_contact( publicid );
    } else
        __super::on_confirm();
}

bool dialog_addcontact_c::hidectl(RID,GUIPARAM p)
{
    if ( RID errr = find( CONSTASTR( "err" ) ) )
        MODIFY(errr).visible(false);
    return true;
}

ts::uint32 dialog_addcontact_c::gm_handler( gmsg<ISOGM_CMD_RESULT>& r)
{
    if (r.cmd == AQ_ADD_CONTACT)
    {
        if ( r.rslt == CR_OPERATION_IN_PROGRESS )
        {
            start_reslove_process();
            return 0;
        }
        if (r.rslt == CR_OK)
        {
            __super::on_confirm();

        } else
        {
            resolve_process = false;
            resolve_anm(RID(), nullptr);
            show_err( r.rslt );
        }
    }

    return 0;
}



//_________________________________________________________________________________________________________________________________
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


dialog_addconference_c::dialog_addconference_c(initial_rect_data_s &data) :gui_isodialog_c(data)
{
    deftitle = title_new_conference;
}

dialog_addconference_c::~dialog_addconference_c()
{
    if (gui)
        gui->delete_event(DELEGATE(this, hidectl));
}

/*virtual*/ void dialog_addconference_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    networks();
    update_options();
    __super::created();
    tabsel(CONSTASTR("1"));
    if (!networks_available)
        ctlenable( CONSTASTR("dialog_button_1"), false );
}

/*virtual*/ ts::ivec2 dialog_addconference_c::get_min_size() const
{
    return ts::ivec2(510, 280);
}

void dialog_addconference_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

void dialog_addconference_c::network_selected(const ts::str_c &prm)
{
    apid = prm.as_int();
    set_combik_menu(CONSTASTR("networks"), networks() );
    update_options();
    tabsel( CONSTASTR( "1" ) );
}

menu_c dialog_addconference_c::networks()
{
    networks_available = false;
    menu_c nm;
    prf().iterate_aps([&](const active_protocol_c &ap) {
        int mif = 0;
        if (ap.get_current_state() != CR_OK)
            mif |= MIF_DISABLED;
        int f = ap.get_features();
        if (0 != (f & PF_CONFERENCE))
        {
            if (apid == 0)
            {
                apid = ap.getid();
                mif |= MIF_MARKED;
            } else if (apid == ap.getid())
                mif |= MIF_MARKED;
        } else
            mif |= MIF_DISABLED;
        nm.add(from_utf8(ap.get_name()), mif, DELEGATE(this, network_selected), ts::amake(ap.getid()));
        if (0 == (mif & MIF_DISABLED))
            networks_available = true;
    });
    return nm;
}

bool dialog_addconference_c::chatoptions(RID,GUIPARAM p)
{
    o = oset.get( as_int(p) );
    return true;
}

void dialog_addconference_c::update_options()
{
    if (active_protocol_c *ap = prf().ap(apid))
    {
        ts::str_c oo = ap->get_infostr( IS_CONFERENCE_OPTIONS );
        ts::parse_values( oo, [this]( const ts::pstr_c&k, const ts::pstr_c&v ) {
            if ( k.equals( CONSTASTR( CONFA_OPT_TYPE ) ) )
                oset.split( v.as_sptr(), '/' );
        } );

        if ( oset.size() == 0 )
            o.clear();
        else if ( o.is_empty() || oset.find( o ) < 0 )
            o = oset.get(0);
    }
}

/*virtual*/ int dialog_addconference_c::additions(ts::irect &)
{
    descmaker dm(this);
    dm << 1;

    menu_c m = networks();
    if (networks_available)
    {
        dm().combik(loc_text(loc_network)).setmenu(m).setname(CONSTASTR("networks"));
    } else
    {
        ts::wstr_c errs(CONSTWSTR("<b>"), loc_text(loc_nonetwork));
        dm().label(errs);
    }
    dm().vspace(5);

    dm().textfield(TTT("Conference name",250), CONSTWSTR(""), DELEGATE(this, confaname_handler)).focus(true);
    dm().vspace(5);

    if (oset.size())
    {
        int incht = 0;

        menu_c menu;
        int i = 0;
        for( const ts::str_c &ov : oset )
        {
            if ( ov.equals( CONSTASTR( "t" ) ) )
                menu.add( TTT( "Text only conference", 253 ), 0, MENUHANDLER(), ts::amake( i ) );

            if ( ov.equals( CONSTASTR( "a" ) ) )
                menu.add( TTT( "Text and audio conference", 254 ), 0, MENUHANDLER(), ts::amake( i ) );

            ++i;
        }

        dm().radio( TTT( "Conference options", 252 ), DELEGATE( this, chatoptions ), incht ).setmenu( menu );
    }

    dm().vspace(5);

    dm().hiddenlabel(TTT("Conference name cannot be empty",251), true).setname(CONSTASTR("err1"));

    return 0;
}

bool dialog_addconference_c::confaname_handler(const ts::wstr_c &m, bool )
{
    confaname = to_utf8(m);
    return true;
}

/*virtual*/ bool dialog_addconference_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

bool dialog_addconference_c::hidectl(RID, GUIPARAM p)
{
    MODIFY(RID::from_param(p)).visible(false);
    return true;
}


void dialog_addconference_c::showerror(int id)
{
    RID errr = find(CONSTASTR("err") + ts::amake(id));
    if (errr)
    {
        MODIFY(errr).visible(true);
        DEFERRED_UNIQUE_CALL(1.0, DELEGATE(this, hidectl), errr);
    }

}

/*virtual*/ void dialog_addconference_c::on_confirm()
{
    if (confaname.is_empty())
    {
        showerror(1);
        return;
    }

    ts::str_c co;

    if ( oset.size() )
        co.set( CONSTASTR( CONFA_OPT_TYPE ) ).append_char('=').append(o);

    if (active_protocol_c *ap = prf().ap(apid))
        ap->create_conference(confaname, co);
    __super::on_confirm();
}

