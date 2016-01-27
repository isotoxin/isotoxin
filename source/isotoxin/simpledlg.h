#pragma once

struct dialog_msgbox_params_s
{
    rtitle_e etitle = title_app;
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
    
    dialog_msgbox_params_s(rtitle_e tt,
                           const ts::wstr_c &desc
                           ) :etitle(tt), desc(desc)  {}

    dialog_msgbox_params_s& bok(const ts::wsptr &t) {ok_button_text = t; return *this;}
    dialog_msgbox_params_s& bcancel(bool f=true, const ts::wsptr &t = ts::wsptr()) {bcancel_ = f; cancel_button_text = t; return *this;}
    dialog_msgbox_params_s& on_ok(MENUHANDLER mh, const ts::str_c &par) {on_ok_h = mh; on_ok_par = par; return *this;}
    dialog_msgbox_params_s& on_cancel(MENUHANDLER mh, const ts::str_c &par) {on_cancel_h = mh; on_cancel_par = par; return *this;}

    RID summon();
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
    int height = 190;
    ts::bitmap_c qrbmp;

    bool copy_text(RID, GUIPARAM);

    bool on_enter_press_func(RID, GUIPARAM);
    bool on_esc_press_func(RID, GUIPARAM);

    void updrect_msgbox(const void *, int r, const ts::ivec2 &p);

protected:

    /*virtual*/ ts::UPDATE_RECTANGLE getrectupdate() { return DELEGATE(this, updrect_msgbox); }

    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void on_close() override;
public:
    dialog_msgbox_c(MAKE_ROOT<dialog_msgbox_c> &data);
    ~dialog_msgbox_c();

    static dialog_msgbox_params_s mb_info(const ts::wstr_c &text);
    static dialog_msgbox_params_s mb_warning(const ts::wstr_c &text);
    static dialog_msgbox_params_s mb_error(const ts::wstr_c &text);
    static dialog_msgbox_params_s mb_qrcode(const ts::wstr_c &text);

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(500, height); }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};



class dialog_pb_c;
class pb_job_c
{
    DECLARE_EYELET(pb_job_c);
public:
    pb_job_c() {}
    virtual ~pb_job_c() {}

    virtual void on_create(dialog_pb_c *pb) { }; // progress bar gui created
    virtual void on_close() {}; // progress bar gui is about to close
    virtual void external_process( float p ) {} // some external process can call this to set current level
};

struct dialog_pb_params_s
{
    ts::iweak_ptr<pb_job_c> pbproc;
    ts::wstr_c title;
    ts::wstr_c desc;
    ts::wstr_c ok_button_text;
    ts::wstr_c cancel_button_text;
    MENUHANDLER on_ok_h;
    ts::str_c on_ok_par;
    MENUHANDLER on_cancel_h;
    ts::str_c on_cancel_par;
    bool bcancel_ = false;
    bool bok_ = false;

    dialog_pb_params_s() {}
    dialog_pb_params_s( const ts::wstr_c &title, const ts::wstr_c &desc ) :title(title), desc(desc) {}


    dialog_pb_params_s& bok(bool f, const ts::wsptr &t) { bok_ = f; ok_button_text = t; return *this; }
    dialog_pb_params_s& bcancel(bool f = true, const ts::wsptr &t = ts::wsptr()) { bcancel_ = f; cancel_button_text = t; return *this; }
    dialog_pb_params_s& on_ok(MENUHANDLER mh, const ts::str_c &par) { on_ok_h = mh; on_ok_par = par; return *this; }
    dialog_pb_params_s& on_cancel(MENUHANDLER mh, const ts::str_c &par) { on_cancel_h = mh; on_cancel_par = par; return *this; }

    dialog_pb_params_s& setpbproc(pb_job_c *pbp) { pbproc = pbp; return *this; }
};

template<> struct MAKE_ROOT<dialog_pb_c> : public _PROOT(dialog_pb_c)
{
    dialog_pb_params_s prms;
    MAKE_ROOT(const dialog_pb_params_s &prms) : _PROOT(dialog_pb_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};


class dialog_pb_c : public gui_isodialog_c
{
    dialog_pb_params_s m_params;
    ts::array_inplace_t<bcreate_s, 0> m_buttons;

protected:
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void on_close() override;

    /*virtual*/ void tabselected(ts::uint32 mask) override;

public:
    dialog_pb_c(MAKE_ROOT<dialog_pb_c> &data);
    ~dialog_pb_c();
    static dialog_pb_params_s params(
        const ts::wstr_c &title,
        const ts::wstr_c &desc) {
        return dialog_pb_params_s(title, desc);
    }

    /*virtual*/ ts::uint32 caption_buttons() const override { return 0; }
    /*virtual*/ ts::wstr_c get_name() const override { return m_params.title; }
    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(500, 190); }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void set_level( float v, const ts::wstr_c &txt );
};



typedef fastdelegate::FastDelegate<bool(const ts::wstr_c &, const ts::str_c &)> TEXTENTEROKFUNC;

struct dialog_entertext_params_s
{
    unique_dialog_e utag = UD_NOT_UNIQUE;
    ts::wstr_c title;
    ts::wstr_c desc;
    ts::wstr_c val;
    ts::str_c  param;
    TEXTENTEROKFUNC okhandler;
    gui_textfield_c::TEXTCHECKFUNC checker = check_always_ok;
    GUIPARAMHANDLER cancelhandler = nullptr;
    RID watch;

    dialog_entertext_params_s() {}
    dialog_entertext_params_s(unique_dialog_e utag,
        const ts::wstr_c &title,
        const ts::wstr_c &desc,
        const ts::wstr_c &val,
        const ts::str_c  &param,
        TEXTENTEROKFUNC okhandler,
        GUIPARAMHANDLER cancelhandler,
        gui_textfield_c::TEXTCHECKFUNC checker,
        RID watch = RID()) :utag(utag), title(title), desc(desc), val(val), param(param), okhandler(okhandler), checker(checker), cancelhandler(cancelhandler), watch(watch) {}
};

class dialog_entertext_c;
template<> struct MAKE_ROOT<dialog_entertext_c> : public _PROOT(dialog_entertext_c)
{
    dialog_entertext_params_s prms;
    MAKE_ROOT(const dialog_entertext_params_s &prms) :_PROOT(dialog_entertext_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};


class dialog_entertext_c : public gui_isodialog_c
{
    GM_RECEIVER( dialog_entertext_c, ISOGM_APPRISE );

    guirect_watch_c watch;
    dialog_entertext_params_s m_params;

protected:
    /*virtual*/ int unique_tag() { return m_params.utag; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    bool on_edit(const ts::wstr_c &);
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void on_close() override;

    bool on_enter_press_func(RID, GUIPARAM);
    bool on_esc_press_func(RID, GUIPARAM);

    bool showpass_handler(RID, GUIPARAM p);
    bool watchdog(RID, GUIPARAM p);
public:
    dialog_entertext_c(MAKE_ROOT<dialog_entertext_c> &data);
    ~dialog_entertext_c();
    static dialog_entertext_params_s params(
        unique_dialog_e utag,
        const ts::wstr_c &title,
        const ts::wstr_c &desc,
        const ts::wstr_c &val,
        const ts::str_c  &param,
        TEXTENTEROKFUNC okhandler = TEXTENTEROKFUNC(),
        GUIPARAMHANDLER cancelhandler = nullptr,
        gui_textfield_c::TEXTCHECKFUNC checker = check_always_ok,
        RID watch = RID()
        ) {
        return dialog_entertext_params_s(utag, title, desc, val, param, okhandler, cancelhandler, checker, watch);
    }

    /*virtual*/ ts::wstr_c get_name() const override { return m_params.title; }
    /*virtual*/ void tabselected(ts::uint32 mask) override;

    /*virtual*/ ts::ivec2 get_min_size() const { return ts::ivec2(400, 300); }

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};










class dialog_about_c : public gui_isodialog_c
{
    process_animation_s pa;
    bool checking_new_version = false;
    bool check_update_now(RID, GUIPARAM);
    GM_RECEIVER(dialog_about_c, ISOGM_NEWVERSION);

    bool updanim(RID, GUIPARAM);

protected:
    /*virtual*/ int unique_tag() { return UD_ABOUT; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    void updrect_about(const void *rr, int r, const ts::ivec2 &p);
    /*virtual*/ ts::UPDATE_RECTANGLE getrectupdate() override { return DELEGATE(this, updrect_about); }

public:
    dialog_about_c(initial_rect_data_s &data);
    ~dialog_about_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const;

    //sqhandler_i
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

