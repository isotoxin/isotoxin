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
    return TTT("[appname]: Contact settings",224);
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
    customname = to_utf8(cn);
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

menu_c dialog_contact_props_c::gethistorymenu()
{
    menu_c m;
    m.add( TTT("Default",226), (keeph == KCH_DEFAULT) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_DEFAULT) );
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

/*virtual*/ int dialog_contact_props_c::additions(ts::irect &)
{
    descmaker dm(descs);
    dm << 1;

    if (contactue)
    {
        customname = contactue->get_customname();
        keeph = contactue->get_keeph();
        aaac = contactue->get_aaac();

        ts::str_c n = contactue->get_name();
        text_adapt_user_input(n);

        dm().page_header(from_utf8(n));
        dm().vspace(10);

        dm().textfield(TTT("Custom name",229), from_utf8(customname), DELEGATE(this, custom_name))
            .focus(true);
        dm().vspace();

        dm().combik(TTT("Message history",230)).setmenu(gethistorymenu()).setname(CONSTASTR("history"));
        dm().vspace();

        dm().combik(TTT("Auto accept audio calls",288)).setmenu(getaacmenu()).setname(CONSTASTR("aaac"));
        dm().vspace();
    }

    return 0;
}

/*virtual*/ bool dialog_contact_props_c::sq_evt(system_query_e qp, RID rid, evt_data_s &d)
{
    if (__super::sq_evt(qp, rid, d)) return true;

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
        if (contactue->get_aaac() != aaac)
        {
            changed = true;
            contactue->set_aaac(aaac);
        }

        if (changed)
        {
            prf().dirtycontact(contactue->getkey());
            contactue->reselect();
        }
    }

    __super::on_confirm();
}

