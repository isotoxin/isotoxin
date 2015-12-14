#pragma once

class gui_filterbar_c;
template<> struct MAKE_CHILD<gui_filterbar_c> : public _PCHILD(gui_filterbar_c)
{
    MAKE_CHILD(RID parent_) { parent = parent_; }
    ~MAKE_CHILD();
};

struct found_item_s
{
    time_t mintime = 0;
    contact_key_s historian;
    ts::tbuf_t<uint64> utags;
};
struct found_stuff_s
{
    typedef ts::array_inplace_t<found_item_s, 32> FOUND_STUFF_T;
    ts::wstrings_c fsplit;
    FOUND_STUFF_T items;
};

class gui_filterbar_c : public gui_label_ex_c
{
    DUMMY(gui_filterbar_c);

    GM_RECEIVER(gui_filterbar_c, ISOGM_CHANGED_SETTINGS);

    found_stuff_s found_stuff;

    struct full_search_s : public ts::task_c
    {
        ts::safe_ptr<gui_filterbar_c> owner;
        ts::sqlitedb_c *db = nullptr;
        ts::wstrings_c flt;
        bool no_need = false;

        full_search_s() {}
        full_search_s(ts::sqlitedb_c *db, gui_filterbar_c *owner, const ts::wstrings_c &flt_) : db(db), flt(flt_), owner(owner)
        {
            for (ts::wstr_c &s : flt)
                s.set_length();
        }

        found_stuff_s::FOUND_STUFF_T found_stuff;
        found_item_s &add( const contact_key_s &historian )
        {
            for( found_item_s &itm : found_stuff )
                if (itm.historian == historian)
                    return itm;
            found_item_s &itm = found_stuff.add();
            itm.historian = historian;
            return itm;
        }

        bool reader(int row, ts::SQLITE_DATAGETTER getta);

        /*virtual*/ int iterate(int pass) override;
        /*virtual*/ void done(bool canceled) override;
    };

    full_search_s * current_search = nullptr;


    ts::safe_ptr<rectengine_c> active;
    ts::safe_ptr<gui_textedit_c> edit;
    ts::safe_ptr<gui_button_c> option1;
    int filtereditheight = 25;
    int contact_index = 0;

    bool search_in_messages = true;

    bool cancel_filter(RID, GUIPARAM);
    bool option_handler(RID, GUIPARAM);
    bool update_filter(const ts::wstr_c & e);
    bool do_contact_check(RID, GUIPARAM);

    void show_options(bool show);
    void apply_full_text_search_result();

public:
    gui_filterbar_c( MAKE_CHILD<gui_filterbar_c> &data );
    ~gui_filterbar_c();

    /*virtual*/ void created() override;
    /*virtual*/ int get_height_by_width(int width) const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

    void full_search_result( found_stuff_s::FOUND_STUFF_T &&stuff );

    void focus_edit();
};