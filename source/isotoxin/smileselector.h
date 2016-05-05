#pragma once

class dialog_smileselector_c;
template<> struct MAKE_ROOT<dialog_smileselector_c> : public _PROOT(dialog_smileselector_c)
{
    ts::irect smilebuttonrect;
    RID editor;
    MAKE_ROOT(const ts::irect &smilebuttonrect, RID editor) :_PROOT(dialog_smileselector_c)(), smilebuttonrect(smilebuttonrect), editor(editor) { init(false); }
    ~MAKE_ROOT() {}
};

class dialog_smileselector_c : public gui_control_c
{
    struct rectdef_s
    {
        MOVABLE( true );
        emoticon_s *e;
        ts::ivec2 p;
        rectdef_s( const ts::ivec2 &p, emoticon_s *e ):p(p), e(e) {}
    };

    typedef ts::tbuf_t< rectdef_s > rects_t;
    rectdef_s *undermouse = nullptr;

    int areah = 0;
    sbhelper_s sb;
    rects_t rects;
    ts::ivec2 sz;
    RID editor;

    bool sbv = false;
    bool sbhl = false;

    bool check_focus(RID r, GUIPARAM p);

    void build_rects(rects_t&a);
    bool find_undermouse();
    bool esc_handler(RID, GUIPARAM);
protected:
    /*virtual*/ void created() override;

public:
    dialog_smileselector_c(MAKE_ROOT<dialog_smileselector_c> &data);
    ~dialog_smileselector_c();

    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
};

