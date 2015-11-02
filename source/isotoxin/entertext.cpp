#include "isotoxin.h"


dialog_entertext_c::dialog_entertext_c(MAKE_ROOT<dialog_entertext_c> &data) :gui_isodialog_c(data), m_params(data.prms)
{
}

dialog_entertext_c::~dialog_entertext_c()
{
    //gui->delete_event(DELEGATE(this, refresh_current_page));
}


/*virtual*/ void dialog_entertext_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel( CONSTASTR("1") );
}

void dialog_entertext_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

/*virtual*/ int dialog_entertext_c::additions(ts::irect &)
{

    descmaker dm(descs);
    dm << 1;

    dm().page_header(m_params.desc);
    dm().vspace(25);
    dm().textfield(ts::wstr_c(), m_params.val, DELEGATE(this, on_edit)).focus(true);

    return 0;
}

bool dialog_entertext_c::on_edit(const ts::wstr_c &t)
{
    if (!m_params.checker(t)) return false;
    m_params.val = t;
    return true;
}

/*virtual*/ bool dialog_entertext_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
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

/*virtual*/ void dialog_entertext_c::on_confirm()
{
    if (m_params.okhandler) 
        if (!m_params.okhandler(m_params.val,m_params.param))
            return;
    __super::on_confirm();
}