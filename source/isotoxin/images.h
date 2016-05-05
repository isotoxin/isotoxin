#pragma once

class picture_c
{
public:
    picture_c() {}
    virtual ~picture_c() {}

    virtual bool load(const ts::blob_c &body) = 0;
    virtual ts::ivec2 framesize_by_width(int w) { return ts::ivec2(0); };
    virtual void fit_to_width(int w) {}
    virtual const ts::bitmap_c &curframe(ts::irect &frect) const  = 0;
    virtual ts::irect framerect() const = 0;
    virtual ts::bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &frect)  = 0;

    virtual void draw(rectengine_root_c *e, const ts::ivec2 &pos) const;
};

class picture_animated_c : public picture_c, public animation_c
{

protected:
    int numframes = 1;
    ts::Time next_frame_tick = ts::Time::current();

public:
    /*virtual*/ ~picture_animated_c();

    /*virtual*/ bool load(const ts::blob_c &body) = 0;
    /*virtual*/ void draw(rectengine_root_c *e, const ts::ivec2 &pos) const override;

};

class picture_gif_c : public picture_animated_c
{
protected:
    ts::animated_c gif;
    
public:
    bool load_only_gif( ts::bitmap_c &first_frame, const ts::blob_c &body );
    /*virtual*/ bool load(const ts::blob_c &body) override;
    /*virtual*/ bool animation_tick() override;
    virtual int nextframe();
};

class image_loader_c : public autoparam_i
{
    DUMMY(image_loader_c);

    ts::safe_ptr<gui_message_item_c> item;
    ts::wstr_c filename;
    picture_c *pic = nullptr;

public:
    ts::safe_ptr<gui_button_c> explorebtn;
    ts::ivec2 local_p = ts::ivec2(0);

    image_loader_c *prev = nullptr;
    image_loader_c *next = nullptr;

    image_loader_c(gui_message_item_c *itm, const ts::wstr_c &filename);
    ~image_loader_c();

    bool signal_loaded(RID, GUIPARAM);
    picture_c *get_picture() { return pic; }
    const picture_c *get_picture() const { return pic; }
    const ts::wstr_c &get_fn() const {return filename;}

    static bool is_image_fn( const ts::asptr &fn_utf8 );

    bool upd_btnpos(RID r = RID(), GUIPARAM p = nullptr);
    void update_ctl_pos();
    /*virtual*/ bool i_leeched(guirect_c &to) override { if (!__super::i_leeched(to)) return false; update_ctl_pos(); return true; };
    /*virtual*/ void i_unleeched() override { /* do nothing */ };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override;

};

INLINE image_loader_c &gui_message_item_c::imgloader_get(addition_file_data_s **ftb)
{
    addition_file_data_s *afd = ts::ptr_cast<addition_file_data_s *>(addition.get());
    if (ASSERT(afd))
    {
        if (ftb) *ftb = afd;
        return afd->imgloader.get<image_loader_c>();
    }
    return ts::make_dummy<image_loader_c>();
}


