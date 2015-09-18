#pragma once

class gui_filterbar_c;
template<> struct MAKE_CHILD<gui_filterbar_c> : public _PCHILD(gui_filterbar_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

class gui_filterbar_c : public gui_label_ex_c
{
    DUMMY(gui_filterbar_c);

    GM_RECEIVER(gui_filterbar_c, ISOGM_CHANGED_SETTINGS);

    ts::safe_ptr<rectengine_c> active;
    ts::safe_ptr<gui_textedit_c> edit;
    ts::wstrings_c fsplit;
    int filtereditheight = 25;
    int contact_index = 0;

    bool update_filter(const ts::wstr_c & e);
    bool do_contact_check(RID, GUIPARAM);

public:
    gui_filterbar_c( MAKE_CHILD<gui_filterbar_c> &data );
    ~gui_filterbar_c();

    /*virtual*/ void created() override;
    /*virtual*/ int get_height_by_width(int width) const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void focus_edit();
};