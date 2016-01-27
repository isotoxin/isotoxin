#pragma once

class dialog_colors_c : public gui_isodialog_c
{
    ts::wstr_c preset;
    ts::str_c curcolor;
    colors_map_s colsmap;
    ts::astrings_c colorsarr;
    ts::TSCOLOR curcol = 0;
    ts::wstr_c colsfn;

protected:

    /*virtual*/ void created() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;
    /*virtual*/ int additions(ts::irect & border) override;

    /*virtual*/ void on_confirm() override;

    menu_c presets();
    menu_c colors();

    void selcolors( const ts::str_c& );
    bool selcolor( RID, GUIPARAM );

    bool cprev( RID, GUIPARAM );
    bool cnext( RID, GUIPARAM );

    bool hslider_r( RID, GUIPARAM );
    bool hslider_g( RID, GUIPARAM );
    bool hslider_b( RID, GUIPARAM );
    bool hslider_a( RID, GUIPARAM );
    bool updatetext(RID r = RID(), GUIPARAM p = nullptr);

    void color_selected();
    void load_preset( const ts::wstr_c &p );
    ts::wstr_c curcolastext() const;
    bool colastext(const ts::wstr_c &);

public:
    dialog_colors_c(initial_rect_data_s &data);
    ~dialog_colors_c();

    /*virtual*/ ts::ivec2 get_min_size() const;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};


