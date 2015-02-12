#pragma once

typedef fastdelegate::FastDelegate<bool (const ts::wstr_c &, const ts::str_c &)> TEXTENTEROKFUNC;

struct dialog_entertext_params_s
{
    unique_dialog_e utag = UD_NOT_UNIQUE;
    ts::wstr_c title;
    ts::wstr_c desc;
    ts::wstr_c val;
    ts::str_c  param;
    TEXTENTEROKFUNC okhandler;
gui_textfield_c::TEXTCHECKFUNC checker = check_always_ok;
    
    dialog_entertext_params_s() {}
    dialog_entertext_params_s(unique_dialog_e utag,
     const ts::wstr_c &title,
     const ts::wstr_c &desc,
     const ts::wstr_c &val,
     const ts::str_c  param,
     TEXTENTEROKFUNC okhandler,
     gui_textfield_c::TEXTCHECKFUNC checker ):utag(utag), title(title), desc(desc), val(val), param(param), okhandler(okhandler), checker(checker) {}
};

class dialog_entertext_c;
template<> struct MAKE_ROOT<dialog_entertext_c> : public _PROOT(dialog_entertext_c)
{
    dialog_entertext_params_s prms;
    MAKE_ROOT(drawchecker &dch, const dialog_entertext_params_s &prms) :_PROOT(dialog_entertext_c)(dch), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};


class dialog_entertext_c : public gui_isodialog_c
{
    dialog_entertext_params_s m_params;

protected:
    /*virtual*/ int unique_tag() { return m_params.utag; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
public:
    dialog_entertext_c(MAKE_ROOT<dialog_entertext_c> &data);
    ~dialog_entertext_c();
    static dialog_entertext_params_s params(
        unique_dialog_e utag,
        const ts::wstr_c &title,
        const ts::wstr_c &desc,
        const ts::wstr_c &val,
        const ts::str_c  param,
        TEXTENTEROKFUNC okhandler = TEXTENTEROKFUNC(),
        gui_textfield_c::TEXTCHECKFUNC checker = check_always_ok)  {  return dialog_entertext_params_s(utag,title,desc,val,param,okhandler,checker); }

    /*virtual*/ ts::wstr_c get_name() const override { return m_params.title; }

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(400, 300); }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

