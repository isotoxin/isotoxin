#pragma once

class dialog_prepareimage_c;
template<> struct MAKE_ROOT<dialog_prepareimage_c> : public _PROOT(dialog_prepareimage_c)
{
    contact_key_s ck;
    ts::bitmap_c bitmap;
    MAKE_ROOT(const contact_key_s &ck) : _PROOT(dialog_prepareimage_c)(), ck(ck)  { init(false); }
    MAKE_ROOT(const contact_key_s &ck, const ts::bitmap_c &bitmap) : _PROOT(dialog_prepareimage_c)(), ck(ck), bitmap(bitmap)  { init(false); }
    ~MAKE_ROOT() {}
};

class dialog_prepareimage_c : public gui_isodialog_c
{
    GM_RECEIVER(dialog_prepareimage_c, GM_DROPFILES);

    struct saver_s : public ts::task_c
    {
        dialog_prepareimage_c *dlg;
        ts::bitmap_c    bitmap2save;
        ts::blob_c      saved_jpg;
        ts::img_format_e saved_img_format = ts::if_none;

        saver_s(dialog_prepareimage_c *dlg, ts::bitmap_c bitmap2save) :dlg(dlg), bitmap2save(bitmap2save)
        {
        }

        /*virtual*/ int iterate(int pass) override;
        /*virtual*/ void done(bool canceled) override;
    } *saver = nullptr;

    contact_key_s ck;
    ts::blob_c  loaded_image;
    ts::blob_c  saved_image;
    ts::img_format_e loaded_img_format = ts::if_none;
    ts::img_format_e saved_img_format = ts::if_none;

    void saver_job_done(saver_s *s);

    process_animation_s pa;
    UNIQUE_PTR(vsb_c) camera;

    framedrawer_s fd;
    ts::animated_c anm;

    ts::bitmap_c bitmap;    // original
    ts::bitmap_c image;     // premultiplied fit to rect

    ts::irect out;
    ts::irect imgrect;
    ts::irect viewrect;
    ts::ivec2 offset;
    ts::irect bitmapcroprect;
    ts::irect inforect;
    ts::irect croprect;
    ts::irect storecroprect;
    float bckscale = 1.0f;

    void backscale_croprect();

    ts::ivec2 wsz;

    ts::ivec2 user_offset = ts::ivec2(0);
    ts::uint32 area = 0;
    int tickvalue = 0;
    int prevsize = 0;
    ts::Time next_frame_tick = ts::Time::past();

    ts::shared_ptr<theme_rect_s> shadow;

    enum camst_e
    {
        CAMST_NONE,
        CAMST_VIEW,
        CAMST_WAIT_ORIG_SIZE,
    } camst = CAMST_NONE;

    bool alpha = false;
    bool dirty = true;
    bool disabled_ok = false;
    bool animated = false;
    bool videodevicesloaded = false;
    bool selrectvisible = false;
    bool allowdndinfo = true;

    void nextframe();

    void update_info();

    void on_newimage();
    bool prepare_working_image(RID r = RID(), GUIPARAM p = nullptr);

    bool animation(RID, GUIPARAM);
    void prepare_stuff();
    void clamp_user_offset();
    void resave();

    bool space_key(RID, GUIPARAM);
    bool paste_hotkey_handler(RID, GUIPARAM);

    vsb_list_t video_devices;
    bool start_capture_menu(RID, GUIPARAM);
    void start_capture_menu_sel(const ts::str_c& prm);
    void start_capture(const vsb_descriptor_s &desc);

    void draw_process(ts::TSCOLOR col, bool cam, bool cambusy);

    void allow_ok( bool allow );

protected:
    /*virtual*/ int unique_tag() override { return UD_PREPARE_IMAGE; }
    /*virtual*/ void created() override;

    bool open_image(RID, GUIPARAM);
    void load_image(const ts::wstr_c &fn);

public:
    dialog_prepareimage_c(MAKE_ROOT<dialog_prepareimage_c> &data);
    ~dialog_prepareimage_c();

    /*virtual*/ ts::uint32 caption_buttons() const override { return SETBIT(CBT_MAXIMIZE) | SETBIT(CBT_NORMALIZE) | __super::caption_buttons(); }
    /*virtual*/ int additions(ts::irect & border) override;
    /*virtual*/ ts::ivec2 get_min_size() const override;
    /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;
    /*virtual*/ void on_confirm() override;
    /*virtual*/ void getbutton(bcreate_s &bcr) override;

    /*virtual*/ void children_repos() override
    {
        __super::children_repos();
        dirty = true;
    }

    void set_video_devices(vsb_list_t &&_video_devices);

};
