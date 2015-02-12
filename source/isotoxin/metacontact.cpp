#include "isotoxin.h"


dialog_metacontact_c::dialog_metacontact_c(MAKE_ROOT<dialog_metacontact_c> &data) :gui_isodialog_c(data)
{
    clist.add() = data.prms.key;
}

dialog_metacontact_c::~dialog_metacontact_c()
{
    //if (gui)
    //    gui->delete_event(DELEGATE(this, hidectl));
}

/*virtual*/ ts::wstr_c dialog_metacontact_c::get_name() const
{
    return TTT("[appname]: Новый метаконтакт",146);
}


/*virtual*/ void dialog_metacontact_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));

}

/*virtual*/ ts::ivec2 dialog_metacontact_c::get_min_size() const
{
    return ts::ivec2(510, 460);
}

void dialog_metacontact_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
    if (bcr.tag == 1)
    {
        bcr.btext = TTT("Создать метаконтакт",149);
    }
}

/*virtual*/ int dialog_metacontact_c::additions(ts::irect & border)
{
    descmaker dm(descs);
    dm << 1;

    dm().page_header(TTT("Будет создан новый метаконтакт - объединение нескольких контактов с общей историей сообщений.[br][l]Добавляйте в этот список другие контакты из основного списока контактов ([nbr]правый клик -> Метаконтакт[/nbr])[/l]",147));
    dm().vspace(300, DELEGATE(this,makelist));

    //dm().textfield(TTT("Публичный идентификатор", 67), ts::wstr_c(inparam.pubid), DELEGATE(this, public_id_handler))
    //    .focus(!resend)
    //    .readonly(resend);
    //dm().vspace(5);
    //dm().textfieldml(TTT("Сообщение", 72), TTT("$: Пожалуйста, добавьте меня в свой список контактов.", 73) / contacts().get_self().get_name(), DELEGATE(this, invite_message_handler))
    //    .focus(resend);
    //dm().vspace(5);
    //dm().hiddenlabel(TTT("Неправильный публичный идентификатор!", 71), ts::ARGB(255, 0, 0)).setname(CONSTASTR("err") + ts::amake((int)CR_INVALID_PUB_ID));
    //dm().hiddenlabel(TTT("Такой контакт уже есть!", 87), ts::ARGB(255, 0, 0)).setname(CONSTASTR("err") + ts::amake((int)CR_ALREADY_PRESENT));
    //dm().hiddenlabel(TTT("Мало памяти!", 92), ts::ARGB(255, 0, 0)).setname(CONSTASTR("err") + ts::amake((int)CR_MEMORY_ERROR));

    return 0;
}

bool dialog_metacontact_c::makelist(RID stub, GUIPARAM)
{
    gui_contactlist_c& cl_ = MAKE_CHILD<gui_contactlist_c>( stub, CLR_NEW_METACONTACT_LIST );
    cl = &cl_;
    cl_.leech( TSNEW(leech_fill_parent_s) );
    cl_.array_mode( clist );
    return true;
}

ts::uint32 dialog_metacontact_c::gm_handler( gmsg<ISOGM_METACREATE> & mca )
{
    switch (mca.state)
    {
    case gmsg<ISOGM_METACREATE>::ADD:
        {
            if (clist.find(mca.ck) < 0)
                clist.add(mca.ck);
            if (cl)
                cl->refresh_array();
            mca.state = gmsg<ISOGM_METACREATE>::ADDED;
            break;
        }
    case gmsg<ISOGM_METACREATE>::REMOVE:
        {
            if (cl && clist.find_remove_slow(mca.ck) >= 0)
                cl->refresh_array();
            break;
        }
    case gmsg<ISOGM_METACREATE>::MAKEFIRST:
        {
            if (cl && clist.find_remove_slow(mca.ck) >= 0)
            {
                clist.insert(0, mca.ck);
                cl->refresh_array();
            }
            break;
        }
    }

    return 0;
}

/*virtual*/ bool dialog_metacontact_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

/*virtual*/ void dialog_metacontact_c::on_confirm()
{
    if (clist.size() < 2)
    {
        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
            gui_isodialog_c::title(DT_MSGBOX_ERROR),
            TTT("Метаконтакт - это объединение двух и более контактов. Сейчас список насчитывает менее двух контактов и для создания метаконтакта этого количества недостаточно.",150)
            ));
        return;
    }

    for (const contact_key_s &ck : clist)
        if (contact_c * c = contacts().find(ck))
            prf().load_history( c->get_historian()->getkey() );

    contact_c *basec = contacts().find(clist.get_remove_slow());
    if (CHECK(basec))
    {
        basec->unload_history();
        for (const contact_key_s &ck : clist)
            if (contact_c * c = contacts().find(ck))
            {
                ASSERT(c->is_multicontact());
                c->unload_history();
                prf().merge_history( basec->getkey(), ck );
                c->subiterate([&](contact_c *cc) {
                    cc->ringtone(false);
                    cc->av(false);
                    cc->calltone(false);
                    basec->subadd(cc);
                    prf().dirtycontact( cc->getkey() );
                });
                c->subclear();
                contacts().kill(ck);
            }
    }
    basec->subiterate([&](contact_c *cc) {
        if ( cc->get_options().is(contact_c::F_DEFALUT) )
        {
            cc->get_options().clear(contact_c::F_DEFALUT);
            prf().dirtycontact(cc->getkey());
        }
    });
    prf().dirtycontact( basec->getkey() );
    __super::on_confirm();
}

