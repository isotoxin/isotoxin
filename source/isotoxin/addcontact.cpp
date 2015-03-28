#include "isotoxin.h"


dialog_addcontact_c::dialog_addcontact_c(MAKE_ROOT<dialog_addcontact_c> &data) :gui_isodialog_c(data), inparam(data.prms)
{
}

dialog_addcontact_c::~dialog_addcontact_c()
{
    if (gui)
        gui->delete_event( DELEGATE(this, hidectl) );
}

/*virtual*/ ts::wstr_c dialog_addcontact_c::get_name() const
{
    if (!inparam.title.is_empty()) return inparam.title;
    return TTT("[appname]: Новый контакт",65);
}


/*virtual*/ void dialog_addcontact_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel( CONSTASTR("1") );

}

/*virtual*/ ts::ivec2 dialog_addcontact_c::get_min_size() const
{
    if (!inparam.pubid.is_empty()) return ts::ivec2(510, 310);
    return ts::ivec2(510, 330);
}

void dialog_addcontact_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

void dialog_addcontact_c::network_selected( const ts::str_c &prm )
{
    apid = prm.as_int();
}

/*virtual*/ int dialog_addcontact_c::additions(ts::irect & border)
{
    bool resend = !inparam.pubid.is_empty();

    descmaker dm(descs);
    dm << 1;

    menu_c networks;
    prf().iterate_aps( [&]( const active_protocol_c &ap ) {
        if (apid == 0) apid = ap.getid();
        networks.add(ap.get_name(), 0, DELEGATE(this, network_selected), ts::amake(ap.getid()));
    } );
        

    if (inparam.key.protoid == 0)
    {
        dm().combik(TTT("Сеть", 66)).setmenu(networks);
        dm().vspace(5);
    } else
    {
        apid = inparam.key.protoid;
    }
    dm().textfield(TTT("Публичный идентификатор",67), ts::wstr_c(inparam.pubid), DELEGATE(this, public_id_handler))
        .focus(!resend)
        .readonly(resend);
    dm().vspace(5);
    dm().textfieldml(TTT("Сообщение",72), TTT("$: Пожалуйста, добавьте меня в свой список контактов.",73) / contacts().get_self().get_name(), DELEGATE(this, invite_message_handler))
        .focus(resend);
    dm().vspace(5);
    dm().hiddenlabel(TTT("Неправильный публичный идентификатор!",71), ts::ARGB(255,0,0)).setname(CONSTASTR("err") + ts::amake((int)CR_INVALID_PUB_ID));
    dm().hiddenlabel(TTT("Такой контакт уже есть!",87), ts::ARGB(255,0,0)).setname(CONSTASTR("err") + ts::amake((int)CR_ALREADY_PRESENT));
    dm().hiddenlabel(L"memory error", ts::ARGB(255,0,0)).setname(CONSTASTR("err") + ts::amake((int)CR_MEMORY_ERROR)); // no need to translate it due rare error
    dm().hiddenlabel(L"timeout", ts::ARGB(255,0,0)).setname(CONSTASTR("err") + ts::amake((int)CR_TIMEOUT)); // no need to translate it due rare error

    return 0;
}

bool dialog_addcontact_c::invite_message_handler(const ts::wstr_c &m)
{
    invitemessage = m;
    return true;
}

bool dialog_addcontact_c::public_id_handler( const ts::wstr_c &pid )
{
    publicid = pid;
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
        else
            ap->add_contact(publicid, invitemessage);
    } else
        __super::on_confirm();
}

bool dialog_addcontact_c::hidectl(RID,GUIPARAM p)
{
    MODIFY(RID::from_ptr(p)).visible(false);
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
            RID errr = find( CONSTASTR("err") + ts::amake((int)r.rslt) );
            if (errr)
            {
                MODIFY(errr).visible(true);
                DEFERRED_UNIQUE_CALL( 1.0, DELEGATE(this, hidectl), errr.to_ptr() );
            }
        }
    }

    return 0;
}