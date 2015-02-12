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

    dm().label(m_params.desc);

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