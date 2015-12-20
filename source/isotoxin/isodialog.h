#pragma once

enum unique_dialog_e
{
    UD_NOT_UNIQUE, // reserve zero value

    UD_ADDCONTACT,
    UD_ADDGROUP,
    UD_METACONTACT,
    UD_SETTINGS,
    UD_PROFILENAME,
    UD_ABOUT,
    UD_AVASELECTOR,
    UD_CONTACTPROPS,
    UD_PROTOSETUP,
    UD_PROTOSETUPSETTINGS,
    UD_PREPARE_IMAGE,

};

enum dlg_title_e
{
    DT_MSGBOX_ERROR,
    DT_MSGBOX_INFO,
    DT_MSGBOX_WARNING,

    DT_CUSTOM,
};

class gui_isodialog_c : public gui_dialog_c
{
    bool b_close(RID,GUIPARAM) { on_close(); return true; }
    bool b_confirm(RID,GUIPARAM) { on_confirm(); return true; }
protected:
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ ts::irect get_client_area() const override;

public:
    gui_isodialog_c(initial_rect_data_s &data);
    ~gui_isodialog_c();

    GUIPARAMHANDLER get_close_button_handler() { return DELEGATE(this, b_close); }
    GUIPARAMHANDLER get_confirm_button_handler() { return DELEGATE(this, b_confirm); }

    static ts::wstr_c title( dlg_title_e d );

};

template<typename DLGT, class... _Valty> void SUMMON_DIALOG(unique_dialog_e udtag, _Valty&&... _Val)
{
    if (udtag && dialog_already_present(udtag)) return;

    RID r = MAKE_ROOT<DLGT>(std::forward<_Valty>(_Val)...);
    {
        MODIFY(r)
            .setminsize(r)
            .setcenterpos()
            .allow_move_resize(true, true)
            .show();
    }

    HOLD(r)().getroot()->set_system_focus(true);
}

template<typename DLGT> void SUMMON_DIALOG(unique_dialog_e udtag = UD_NOT_UNIQUE)
{
    if (udtag && dialog_already_present(udtag)) return;
    RID r = MAKE_ROOT<DLGT>();
    
    {
        MODIFY(r)
            .setminsize(r)
            .setcenterpos()
            .allow_move_resize(true, true)
            .show();
    }
    HOLD(r)().getroot()->set_system_focus(true);
}


