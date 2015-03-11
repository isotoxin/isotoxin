#pragma once

struct framedrawer_s
{
    ts::drawable_bitmap_c h;
    ts::drawable_bitmap_c v;

    void prepare( ts::TSCOLOR col1, ts::TSCOLOR col2 );
    void draw(rectengine_c &e, const ts::irect &r, int tickvalue);
};

class dialog_avaselector_c : public gui_isodialog_c
{
    GM_RECEIVER(dialog_avaselector_c, GM_DROPFILES);
    GM_RECEIVER(dialog_avaselector_c, GM_PASTE_HOTKEY);
    
    framedrawer_s fd;

    ts::bitmap_c bitmap;
    ts::drawable_bitmap_c image;

    ts::irect out;
    ts::irect imgrect;
    ts::irect viewrect;
    ts::ivec2 offset;
    ts::irect inforect;
    ts::irect avarect;
    ts::irect storeavarect; // for scale

    ts::ivec2 user_offset = 0;
    float resize_k = 1.0f;
    float click_resize_k;
    ts::uint32 area = 0;
    int tickvalue = 0;
    int prevsize = 0;
    ts::wstr_c savebtn;

    bool alpha = false;
    bool dirty = true;
    bool disabled_ok = false;

    bool flashavarect(RID, GUIPARAM);
    void rebuild_bitmap();
    void prepare_stuff();
    void clamp_user_offset();
    void recompress();

    void statrt_scale();
    void do_scale(float scale);

    struct syncstruct_s
    {
        ts::bitmap_c bitmap2encode;
        ts::buf_c    encoded;
        ts::buf_c    encoded_fit_16kb;
        int source_tag = 0;
        bool compressor_working = false;
        bool compressor_should_stop = false;
    };

    spinlock::syncvar<syncstruct_s> sync;
    static DWORD WINAPI dialog_avaselector_c::worker(LPVOID ap);
    void compressor();

protected:
    /*virtual*/ int unique_tag() { return UD_AVASELECTOR; }
    /*virtual*/ void created() override;

    bool save_image(RID, GUIPARAM);
    bool open_image(RID, GUIPARAM);
    void load_image(const ts::wstr_c &fn);

public:
    dialog_avaselector_c(initial_rect_data_s &data);
    ~dialog_avaselector_c();

    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override { return ts::ivec2(570, 500); }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void on_confirm() override;
};