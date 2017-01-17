#pragma once

enum unique_dialog_e
{
    UD_NOT_UNIQUE, // reserve zero value

    UD_ENTERPASSWORD,

    UD_ADDCONTACT,
    UD_ADDGROUP,
    UD_METACONTACT,
    UD_SETTINGS,
    UD_PROFILENAME,
    UD_ABOUT,
    UD_AVASELECTOR,
    UD_CONTACTPROPS,
    UD_NEWTAG,
    UD_RENTAG,
    UD_PROTOSETUP,
    UD_PROTOSETUPSETTINGS,
    UD_PREPARE_IMAGE,
    UD_ENCRYPT_PROFILE_PB,
    UD_DICTIONARIES,

};

enum rtitle_e
{
    title_app,

    title_information,
    title_warning,
    title_error,

    title_first_run,
    title_settings,
    title_new_contact,
    title_repeat_request,
    title_new_conference,
    title_avatar_creation_tool,
    title_contact_properties,
    title_new_meta_contact,
    title_prepare_image,
    title_encrypting,
    title_removing_encryption,
    title_new_network,
    title_connection_properties,
    title_profile_name,
    title_enter_password,
    title_reenter_password,
    title_qr_code,
    title_newtags,
    title_rentag,
    title_avatar,
    title_conference_properties,
};


class gui_isodialog_c : public gui_dialog_c
{
    typedef gui_dialog_c super;
    bool b_close(RID,GUIPARAM) { on_close(); return true; }
    bool b_confirm(RID,GUIPARAM) { on_confirm(); return true; }
protected:
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ ts::irect get_client_area() const override;
    rtitle_e deftitle = title_app;

    /*virtual*/ ts::wstr_c get_name() const override { return title(deftitle); }

public:
    gui_isodialog_c(initial_rect_data_s &data);
    ~gui_isodialog_c();

    GUIPARAMHANDLER get_close_button_handler() { return DELEGATE(this, b_close); }
    GUIPARAMHANDLER get_confirm_button_handler() { return DELEGATE(this, b_confirm); }

    static ts::wstr_c title( rtitle_e t );

};

template<typename DLGT, class... _Valty> RID SUMMON_DIALOG(unique_dialog_e udtag, bool mainparent, _Valty&&... _Val)
{
    if (udtag && dialog_already_present(udtag)) return RID();

    RID r = MAKE_ROOT<DLGT>( mainparent, std::forward<_Valty>(_Val)... );
    {
        MODIFY(r)
            .setminsize(r)
            .setcenterpos()
            .allow_move_resize(true, true)
            .show();
    }

    HOLD(r)().getroot()->set_system_focus(true);
    return r;
}

template<typename DLGT> RID SUMMON_DIALOG(unique_dialog_e udtag = UD_NOT_UNIQUE)
{
    if (udtag && dialog_already_present(udtag)) return RID();
    RID r = MAKE_ROOT<DLGT>();
    
    {
        MODIFY(r)
            .setminsize(r)
            .setcenterpos()
            .allow_move_resize(true, true)
            .show();
    }
    HOLD(r)().getroot()->set_system_focus(true);
    return r;
}

