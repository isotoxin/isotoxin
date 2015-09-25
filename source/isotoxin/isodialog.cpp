#include "isotoxin.h"

gui_isodialog_c::gui_isodialog_c(initial_rect_data_s &data) :gui_dialog_c(data)
{
    label_path_selector_caption = TTT("Select folder",21);
    label_file_selector_caption = TTT("File selector",69);
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
        bcr.face = BUTTON_FACE(button);
        bcr.btext = TTT("Cancel",9);
        bcr.handler = DELEGATE(this, b_close);
        break;
    case 1: // close/cancel
        bcr.face = BUTTON_FACE(button);
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
    switch (d) //-V719
    {
    case DT_MSGBOX_ERROR:
        return TTT("[appname]: error",45);
    case DT_MSGBOX_INFO:
        return TTT("[appname]: information",46);
    case DT_MSGBOX_WARNING:
        return TTT("[appname]: warning",86);
    }
    return TTT("[appname]",47);
}
