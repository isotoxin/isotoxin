#pragma once

struct dialog_metacontact_params_s
{
    contact_key_s key;

    dialog_metacontact_params_s() {}
    dialog_metacontact_params_s(const contact_key_s &key) :key(key)  {}
};

template<> struct gmsg<ISOGM_METACREATE> : public gmsgbase
{
    enum
    {
        ADD,
        REMOVE,
        ADDED,
        MAKEFIRST,
        CHECKINLIST,

    } state = ADD;
    gmsg(contact_key_s ck) :gmsgbase(ISOGM_METACREATE), ck(ck) {}
    contact_key_s ck;
};


class dialog_metacontact_c;
template<> struct MAKE_ROOT<dialog_metacontact_c> : public _PROOT(dialog_metacontact_c)
{
    dialog_metacontact_params_s prms;
    MAKE_ROOT() :_PROOT(dialog_metacontact_c)() { init(false); }
    MAKE_ROOT(const dialog_metacontact_params_s &prms) :_PROOT(dialog_metacontact_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};

class dialog_metacontact_c : public gui_isodialog_c
{
    GM_RECEIVER( dialog_metacontact_c, ISOGM_METACREATE );
    GM_RECEIVER( dialog_metacontact_c, GM_DRAGNDROP );

    ts::array_inplace_t<contact_key_s, 2> clist;

    ts::safe_ptr<gui_contactlist_c> cl;

    //bool dndtgt = false;

protected:
    /*virtual*/ int unique_tag() { return UD_METACONTACT; }
    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ void on_confirm() override;

    bool makelist(RID, GUIPARAM);

public:
    dialog_metacontact_c(MAKE_ROOT<dialog_metacontact_c> &data);
    ~dialog_metacontact_c();

    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

