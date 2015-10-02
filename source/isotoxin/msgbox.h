#pragma once

struct dialog_msgbox_params_s
{
    dlg_title_e etitle = DT_CUSTOM;
    ts::wstr_c title;
    ts::wstr_c desc;
    ts::wstr_c ok_button_text;
    ts::wstr_c cancel_button_text;
    MENUHANDLER on_ok_h;
    ts::str_c on_ok_par;
    MENUHANDLER on_cancel_h;
    ts::str_c on_cancel_par;
    bool bcancel_ = false;
    bool bok_ = true;

    dialog_msgbox_params_s() {}
    dialog_msgbox_params_s(   const ts::wstr_c &title,
                              const ts::wstr_c &desc
                            ) :title(title), desc(desc)  {}
    
    dialog_msgbox_params_s(dlg_title_e tt,
                           const ts::wstr_c &desc
                           ) :etitle(tt), desc(desc)  {}

    dialog_msgbox_params_s& bok(const ts::wsptr &t) {ok_button_text = t; return *this;}
    dialog_msgbox_params_s& bcancel(bool f=true, const ts::wsptr &t = ts::wsptr()) {bcancel_ = f; cancel_button_text = t; return *this;}
    dialog_msgbox_params_s& on_ok(MENUHANDLER mh, const ts::str_c &par) {on_ok_h = mh; on_ok_par = par; return *this;}
    dialog_msgbox_params_s& on_cancel(MENUHANDLER mh, const ts::str_c &par) {on_cancel_h = mh; on_cancel_par = par; return *this;}
};

class dialog_msgbox_c;
template<> struct MAKE_ROOT<dialog_msgbox_c> : public _PROOT(dialog_msgbox_c)
{
    dialog_msgbox_params_s prms;
    MAKE_ROOT(const dialog_msgbox_params_s &prms) : _PROOT(dialog_msgbox_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};


class dialog_msgbox_c : public gui_isodialog_c
{
    dialog_msgbox_params_s m_params;
    ts::array_inplace_t<bcreate_s, 0> m_buttons;

    bool copy_text(RID, GUIPARAM);

protected:
    // /*virtual*/ int unique_tag() { return UD_NOT_UNIQUE; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void on_close() override;
public:
    dialog_msgbox_c(MAKE_ROOT<dialog_msgbox_c> &data);
    ~dialog_msgbox_c();
    static dialog_msgbox_params_s params(
        const ts::wstr_c &title,
        const ts::wstr_c &desc)  {
        return dialog_msgbox_params_s(title, desc);
    }
    static dialog_msgbox_params_s params(
        dlg_title_e title,
        const ts::wstr_c &desc)  {
        return dialog_msgbox_params_s(title, desc);
    }

    /*virtual*/ ts::wstr_c get_name() const override { return m_params.title; }

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(500, 190); }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};


class dialog_about_c : public gui_isodialog_c
{
    bool checking_new_version = false;
    bool check_update_now(RID, GUIPARAM);
    GM_RECEIVER(dialog_about_c, ISOGM_NEWVERSION);
protected:
    /*virtual*/ int unique_tag() { return UD_ABOUT; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

public:
    dialog_about_c(initial_rect_data_s &data);
    ~dialog_about_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const;

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

