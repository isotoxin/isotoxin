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
    case 1: // ok
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

ts::wstr_c gui_isodialog_c::title( rtitle_e lbl )
{
#define MAKE_TITLE( s ) APPNAME_CAPTION.append(CONSTWSTR(": ")).append(s)

    switch (lbl) //-V719
    {
    case title_information:                 return MAKE_TITLE(TTT("information",900));
    case title_warning:                     return MAKE_TITLE(TTT("warning",901));
    case title_error:                       return MAKE_TITLE(TTT("error",902));

    case title_first_run:                   return MAKE_TITLE(TTT("First run",903));
    case title_settings:                    return MAKE_TITLE(TTT("Settings",904));
    case title_new_contact:                 return MAKE_TITLE(TTT("New contact",905));
    case title_repeat_request:              return MAKE_TITLE(TTT("Repeat request",906));
    case title_new_conference:              return MAKE_TITLE(TTT("New conference",907));
    case title_avatar_creation_tool:        return MAKE_TITLE(TTT("Avatar creation tool",908));
    case title_contact_properties:          return MAKE_TITLE(TTT("Contact properties",909));
    case title_new_meta_contact:            return MAKE_TITLE(TTT("New metacontact",910));
    case title_prepare_image:               return MAKE_TITLE(TTT("Prepare image",911));
    case title_encrypting:                  return MAKE_TITLE(TTT("Encrypting...",912));
    case title_removing_encryption:         return MAKE_TITLE(TTT("Removing encryption...",913));
    case title_new_network:                 return MAKE_TITLE(TTT("New network",914));
    case title_connection_properties:       return MAKE_TITLE(TTT("Connection properties",915));
    case title_profile_name:                return MAKE_TITLE(TTT("Profile name",916));
    case title_enter_password:              return MAKE_TITLE(TTT("Enter password",917));
    case title_reenter_password:            return MAKE_TITLE(TTT("Re-enter password",918));
    case title_qr_code:                     return MAKE_TITLE(TTT("QR code",919));
    case title_newtags:                     return MAKE_TITLE(TTT("New contact tag(s)",920));
    case title_rentag:                      return MAKE_TITLE(TTT("Rename tag",921));
    case title_avatar:                      return MAKE_TITLE(TTT("Avatar",922));
    case title_conference_properties:       return MAKE_TITLE(TTT("Conference properties",923));
    }

    return APPNAME_CAPTION;
}
