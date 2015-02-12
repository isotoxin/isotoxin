#include "isotoxin.h"

gui_isodialog_c::gui_isodialog_c(initial_rect_data_s &data) :gui_dialog_c(data)
{
    label_path_selector_caption = TTT("Выбор папки", 21);
}

gui_isodialog_c::~gui_isodialog_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, b_close));
        gui->delete_event(DELEGATE(this, b_confirm));
    }
}

/*virtual*/ void gui_isodialog_c::getbutton(bcreate_s &bcr)
{
    switch (bcr.tag)
    {
    case 0: // close/cancel
        bcr.face = CONSTASTR("button");
        bcr.btext = TTT("Отмена",9);
        bcr.handler = DELEGATE(this, b_close);
        break;
    case 1: // close/cancel
        bcr.face = CONSTASTR("button");
        bcr.btext = TTT("Ok",10);
        bcr.handler = DELEGATE(this, b_confirm);
        break;
    }
}

/*virtual*/ ts::irect gui_isodialog_c::get_client_area() const
{
    ts::irect clar = __super::get_client_area();
    clar.lt += 5;
    clar.rb -= 5;
    return clar;
}

ts::wstr_c gui_isodialog_c::title( dlg_title_e d )
{
    switch (d)
    {
    case DT_MSGBOX_ERROR:
        return TTT("[appname]: ошибка",45);
    case DT_MSGBOX_INFO:
        return TTT("[appname]: информация",46);
    case DT_MSGBOX_WARNING:
        return TTT("[appname]: предупреждение",86);
    }
    return TTT("[appname]",47);
}
