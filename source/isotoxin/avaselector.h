#pragma once

struct dialog_avasel_params_s
{
    int protoid = 0;

    dialog_avasel_params_s() {}
    dialog_avasel_params_s(int protoid) :protoid(protoid)  {}
};

class dialog_avaselector_c;
template<> struct MAKE_ROOT<dialog_avaselector_c> : public _PROOT(dialog_avaselector_c)
{
    dialog_avasel_params_s prms;
    MAKE_ROOT(const dialog_avasel_params_s &prms) : _PROOT(dialog_avaselector_c)(), prms(prms) { init(false); }
    ~MAKE_ROOT() {}
};


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
    
    struct compressor_s : public ts::task_c
    {
        dialog_avaselector_c *dlg;
        ts::bitmap_c    bitmap2encode;
        ts::blob_c      encoded;
        ts::blob_c      encoded_fit_16kb;
        ts::blob_c      temp;

        int sz1 = 16;
        int sz2 = 16;
        int best = 0;
        ts::ivec2 bestsz;

        compressor_s( dialog_avaselector_c *dlg, ts::bitmap_c bitmap2encode ):dlg(dlg), bitmap2encode(bitmap2encode)
        {
        }

        /*virtual*/ int iterate(int pass) override;
        /*virtual*/ void done(bool canceled) override;
    } *compressor = nullptr;

    ts::blob_c  encoded;
    ts::blob_c  encoded_fit_16kb; ts::ivec2 encoded_fit_16kb_size = ts::ivec2(0);
    void compressor_job_done( compressor_s *c )
    {
        ASSERT( compressor == c );
        encoded = c->encoded;
        encoded_fit_16kb = c->encoded_fit_16kb;
        encoded_fit_16kb_size = c->bestsz;
        compressor = nullptr;
    }

    process_animation_bitmap_s pa;
    UNIQUE_PTR( vsb_c ) camera;

    framedrawer_s fd;
    ts::animated_c anm;

    ts::bitmap_c bitmap;
    ts::drawable_bitmap_c image;

    ts::irect out;
    ts::irect imgrect;
    ts::irect viewrect;
    ts::ivec2 offset;
    ts::irect inforect;
    ts::irect avarect;
    ts::irect storeavarect; // for scale

    int protoid = 0;

    ts::ivec2 user_offset = ts::ivec2(0);
    float resize_k = 1.0f;
    float click_resize_k;
    ts::uint32 area = 0;
    int tickvalue = 0;
    int prevsize = 0;
    ts::wstr_c savebtn1, savebtn2;

    ts::shared_ptr<theme_rect_s> shadow;

    bool alpha = false;
    bool dirty = true;
    bool disabled_ok = false;
    bool animated = false;
    bool videodevicesloaded = false;
    bool selrectvisible = false;
    bool allowdndinfo = true;
    bool caminit = false;

    void nextframe();

    void update_info();

    void newimage();
    bool animation(RID, GUIPARAM);
    void rebuild_bitmap();
    void prepare_stuff();
    void clamp_user_offset();
    void recompress();

    void statrt_scale();
    void do_scale(float scale);

    bool space_key(RID, GUIPARAM);
    bool paste_hotkey_handler(RID, GUIPARAM);

    vsb_list_t video_devices;
    bool start_capture_menu(RID, GUIPARAM);
    void start_capture_menu_sel( const ts::str_c& prm );
    void start_capture( const vsb_descriptor_s &desc );

    void draw_process(ts::TSCOLOR col, bool cam, bool cambusy);

protected:
    /*virtual*/ int unique_tag() { return UD_AVASELECTOR; }
    /*virtual*/ void created() override;

    bool save_image1(RID, GUIPARAM);
    bool save_image2(RID, GUIPARAM);
    bool open_image(RID, GUIPARAM);
    void load_image(const ts::wstr_c &fn);

public:
    dialog_avaselector_c(MAKE_ROOT<dialog_avaselector_c> &data);
    ~dialog_avaselector_c();

    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ ts::wstr_c get_name() const override;
    /*virtual*/ ts::ivec2 get_min_size() const override { return ts::ivec2(570, 520); }
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void on_confirm() override;

    /*virtual*/ void children_repos() override
    {
        __super::children_repos();
        dirty = true;
    }

    void set_video_devices(vsb_list_t &&_video_devices);

};