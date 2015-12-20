#pragma once

struct dialog_contactprops_params_s
{
    contact_key_s key;

    dialog_contactprops_params_s() {}
    dialog_contactprops_params_s(const contact_key_s &key) :key(key)  {}
};

class dialog_contact_props_c;
template<> struct MAKE_ROOT<dialog_contact_props_c> : public _PROOT(dialog_contact_props_c)
{
    dialog_contactprops_params_s prms;
    MAKE_ROOT() :_PROOT(dialog_contact_props_c)() { init(false); }
    MAKE_ROOT(const dialog_contactprops_params_s &prms) :_PROOT(dialog_contact_props_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};

class dialog_contact_props_c : public gui_isodialog_c
{
    GM_RECEIVER(dialog_contact_props_c, ISOGM_V_UPDATE_CONTACT);

    ts::shared_ptr<contact_c> contactue;

    ts::str_c customname; // utf8
    bool custom_name( const ts::wstr_c & );

    ts::str_c ccomment; // utf8
    bool comment( const ts::wstr_c & );

    keep_contact_history_e keeph = KCH_DEFAULT;
    auto_accept_audio_call_e aaac = AAAC_NOT;
    
    void history_settings( const ts::str_c& );
    void aaac_settings( const ts::str_c& );

    menu_c gethistorymenu();
    menu_c getaacmenu();


    void add_det(RID lst, contact_c *c);

    struct cinfo_s
    {
        ts::shared_ptr<contact_c> c;
        UNIQUE_PTR(ts::bitmap_c) btmp;
        const ts::bitmap_c *bmp = nullptr;
        ts::json_c cldets;
    };

    ts::array_inplace_t<cinfo_s, 0> infs;

    cinfo_s & getinfo(contact_c *c)
    {
        for (cinfo_s &inf : infs)
            if (inf.c == c) return inf;
        cinfo_s & inf = infs.add();
        inf.c = c;
        return inf;
    }

    void update();
    void fill_list();


protected:
    /*virtual*/ int unique_tag() { return UD_CONTACTPROPS; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void tabselected(ts::uint32 mask) override;

public:
    dialog_contact_props_c(MAKE_ROOT<dialog_contact_props_c> &data);
    ~dialog_contact_props_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};


