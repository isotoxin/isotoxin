#pragma once

struct dialog_addcontact_params_s
{
    ts::wstr_c title;
    ts::str_c pubid;
    contact_key_s key;

    dialog_addcontact_params_s() {}
    dialog_addcontact_params_s(const ts::wstr_c &title, const ts::str_c &pubid, const contact_key_s &key ) :title(title), pubid(pubid), key(key)  {}
};

class dialog_addcontact_c;
template<> struct MAKE_ROOT<dialog_addcontact_c> : public _PROOT(dialog_addcontact_c)
{
    dialog_addcontact_params_s prms;
    MAKE_ROOT() : _PROOT(dialog_addcontact_c)() { init( RS_NORMAL ); }
    MAKE_ROOT(const dialog_addcontact_params_s &prms) :_PROOT(dialog_addcontact_c)(), prms(prms) { init( RS_NORMAL ); }
    ~MAKE_ROOT() {}
};

class dialog_addcontact_c : public gui_isodialog_c
{
    GM_RECEIVER(dialog_addcontact_c, ISOGM_CMD_RESULT);

    int apid = 0;
    ts::str_c publicid; // utf8
    ts::str_c invitemessage; // utf8
    ts::wstr_c invitemessage_autogen;

    dialog_addcontact_params_s inparam;
    bool networks_available = false;
    bool send_authorization_request = true;
    bool resolve_process = false;

    ts::wstr_c rtext();
    ts::wstr_c idname() const;

    void show_err( cmd_result_e r );
    process_animation_s pa;
    void draw_resolve_process();
    void start_reslove_process();
    bool resolve_anm( RID, GUIPARAM );
    ts::ivec2 anmpos();

protected:
    /*virtual*/ int unique_tag() override { return UD_ADDCONTACT; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void tabselected( ts::uint32 /*mask*/ );

    menu_c networks();
    bool hidectl(RID,GUIPARAM);

    bool public_id_handler( const ts::wstr_c &, bool );
    bool invite_message_handler( const ts::wstr_c &, bool );
    void network_selected( const ts::str_c & );
    bool authorization_handler( RID, GUIPARAM );
public:
    dialog_addcontact_c(MAKE_ROOT<dialog_addcontact_c> &data);
    ~dialog_addcontact_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};



class dialog_addgroup_c : public gui_isodialog_c
{
    int apid = 0;
    ts::str_c groupname; // utf8
    bool persistent = true;
    bool networks_available = false;

protected:
    /*virtual*/ int unique_tag() override { return UD_ADDGROUP; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ void on_confirm() override;
    
    bool chatlifetime(RID, GUIPARAM);
    void update_lifetime();

    menu_c networks();
    bool hidectl(RID,GUIPARAM);
    void showerror(int id);

    bool groupname_handler(const ts::wstr_c &, bool );
    void network_selected(const ts::str_c &);
public:
    dialog_addgroup_c(initial_rect_data_s &data);
    ~dialog_addgroup_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

