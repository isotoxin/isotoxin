#include "isotoxin.h"


dialog_addcontact_c::dialog_addcontact_c(MAKE_ROOT<dialog_addcontact_c> &data) :gui_isodialog_c(data), inparam(data.prms)
{
    deftitle = title_new_contact;
}

dialog_addcontact_c::~dialog_addcontact_c()
{
    if (gui)
        gui->delete_event( DELEGATE(this, hidectl) );
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
            DEFERRED_UNIQUE_CALL( 1.0, DELEGATE( this, hidectl ), errr );
        }
    }

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

bool dialog_addcontact_c::invite_message_handler(const ts::wstr_c &m)
{
    invitemessage = to_utf8(m);
    return true;
}

bool dialog_addcontact_c::public_id_handler( const ts::wstr_c &pid )
{
    publicid = to_utf8(pid);
    return true;
}

/*virtual*/ bool dialog_addcontact_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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
    MODIFY(RID::from_param(p)).visible(false);
    return true;
}

ts::uint32 dialog_addcontact_c::gm_handler( gmsg<ISOGM_CMD_RESULT>& r)
{
    if (r.cmd == AQ_ADD_CONTACT)
    {
        if (r.rslt == CR_OPERATION_IN_PROGRESS) return 0; // TODO : show progress animation
        if (r.rslt == CR_OK)
        {
            __super::on_confirm();

        } else
        {
            show_err( r.rslt );
        }
    }

    return 0;
}



//_________________________________________________________________________________________________________________________________
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


dialog_addgroup_c::dialog_addgroup_c(initial_rect_data_s &data) :gui_isodialog_c(data)
{
    deftitle = title_new_groupchat;
}

dialog_addgroup_c::~dialog_addgroup_c()
{
    if (gui)
        gui->delete_event(DELEGATE(this, hidectl));
}

/*virtual*/ void dialog_addgroup_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));
    update_lifetime();
    if (!networks_available)
        ctlenable( CONSTASTR("dialog_button_1"), false );
}

/*virtual*/ ts::ivec2 dialog_addgroup_c::get_min_size() const
{
    return ts::ivec2(510, 280);
}

void dialog_addgroup_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

void dialog_addgroup_c::network_selected(const ts::str_c &prm)
{
    apid = prm.as_int();
    set_combik_menu(CONSTASTR("networks"), networks() );
    update_lifetime();
}

menu_c dialog_addgroup_c::networks()
{
    networks_available = false;
    menu_c nm;
    prf().iterate_aps([&](const active_protocol_c &ap) {
        int mif = 0;
        if (ap.get_current_state() != CR_OK)
            mif |= MIF_DISABLED;
        int f = ap.get_features();
        if (0 != (f & PF_GROUP_CHAT))
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

bool dialog_addgroup_c::chatlifetime(RID,GUIPARAM p)
{
    persistent = p == nullptr;
    return true;
}

void dialog_addgroup_c::update_lifetime()
{
    if (active_protocol_c *ap = prf().ap(apid))
    {
        if ( 0 == (ap->get_features() & PF_GROUP_CHAT_PERSISTENT) )
        {
            // persistent group chats not supported yet
            ctlenable(CONSTASTR("lifetime0"), false);
            if (persistent)
                if (RID p = find(CONSTASTR("lifetime1")))
                    p.call_lbclick();
        } else
        {
            ctlenable(CONSTASTR("lifetime0"), true);
        }
    }
}

/*virtual*/ int dialog_addgroup_c::additions(ts::irect &)
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

    dm().textfield(TTT("Group chat name",250), CONSTWSTR(""), DELEGATE(this, groupname_handler)).focus(true);
    dm().vspace(5);

    int incht = 0;
    dm().radio(TTT("Group chat lifetime",252), DELEGATE(this, chatlifetime), incht).setmenu(
        menu_c().add(TTT("Persistent (saved across client restarts)",253), 0, MENUHANDLER(), CONSTASTR("0"))
        .add(TTT("Temporary (history log not saved; offline - leave group chat)",254), 0, MENUHANDLER(), CONSTASTR("1"))
       ).setname("lifetime");

    dm().vspace(5);

    dm().hiddenlabel(TTT("Group chat name cannot be empty",251), true).setname(CONSTASTR("err1"));

    return 0;
}

bool dialog_addgroup_c::groupname_handler(const ts::wstr_c &m)
{
    groupname = to_utf8(m);
    return true;
}

/*virtual*/ bool dialog_addgroup_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

bool dialog_addgroup_c::hidectl(RID, GUIPARAM p)
{
    MODIFY(RID::from_param(p)).visible(false);
    return true;
}


void dialog_addgroup_c::showerror(int id)
{
    RID errr = find(CONSTASTR("err") + ts::amake(id));
    if (errr)
    {
        MODIFY(errr).visible(true);
        DEFERRED_UNIQUE_CALL(1.0, DELEGATE(this, hidectl), errr);
    }

}

/*virtual*/ void dialog_addgroup_c::on_confirm()
{
    if (groupname.is_empty())
    {
        showerror(1);
        return;
    }

    if (active_protocol_c *ap = prf().ap(apid))
        ap->add_group_chat(groupname, persistent);
    __super::on_confirm();
}

