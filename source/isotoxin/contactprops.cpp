#include "isotoxin.h"


dialog_contact_props_c::dialog_contact_props_c(MAKE_ROOT<dialog_contact_props_c> &data) :gui_isodialog_c(data)
{
    contactue = contacts().find(data.prms.key);
}

dialog_contact_props_c::~dialog_contact_props_c()
{
}

/*virtual*/ ts::wstr_c dialog_contact_props_c::get_name() const
{
    return TTT("[appname]: Свойства контакта",224);
}


/*virtual*/ void dialog_contact_props_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));

}

/*virtual*/ ts::ivec2 dialog_contact_props_c::get_min_size() const
{
    return ts::ivec2(510, 360);
}

void dialog_contact_props_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

bool dialog_contact_props_c::custom_name( const ts::wstr_c & cn )
{
    customname = cn;
    return true;
}

void dialog_contact_props_c::history_settings( const ts::str_c& v )
{
    keeph = (keep_contact_history_e)v.as_uint();
    set_combik_menu(CONSTASTR("history"), gethistorymenu());
}

menu_c dialog_contact_props_c::gethistorymenu()
{
    menu_c m;
    m.add( TTT("По умолчанию",226), (keeph == KCH_DEFAULT) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_DEFAULT) );
    m.add( TTT("Всегда сохранять",227), (keeph == KCH_ALWAYS_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_ALWAYS_KEEP) );
    m.add( TTT("Не сохранять",228), (keeph == KCH_NEVER_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_NEVER_KEEP) );

    return m;
}

/*virtual*/ int dialog_contact_props_c::additions(ts::irect & border)
{
    descmaker dm(descs);
    dm << 1;

    ts::wstr_c cname(TTT("Свойства контакта",225));
    if (contactue)
    {
        customname = contactue->get_customname();
        keeph = contactue->get_keeph();

        ts::wstr_c n = contactue->get_name();
        text_adapt_user_input(n);
        cname.append(CONSTWSTR("<br>")).append(n); 

        dm().page_header(cname);
        dm().vspace(10);

        dm().textfield(TTT("Отображаемое имя",229), customname, DELEGATE(this, custom_name))
            .focus(true);
        dm().vspace();

        dm().combik(TTT("История сообщений",230)).setmenu(gethistorymenu()).setname(CONSTASTR("history"));

    }

    return 0;
}

/*virtual*/ bool dialog_contact_props_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

/*virtual*/ void dialog_contact_props_c::on_confirm()
{
    if (contactue)
    {
        bool changed = false;
        if (contactue->get_customname() != customname)
        {
            changed = true;
            contactue->set_customname(customname);
        }
        if (contactue->get_keeph() != keeph)
        {
            changed = true;
            contactue->set_keeph(keeph);
        }

        if (changed)
        {
            prf().dirtycontact(contactue->getkey());
            contactue->reselect(true);
        }
    }

    __super::on_confirm();
}

