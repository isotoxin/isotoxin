#pragma once

class picture_c
{
protected:
    ts::drawable_bitmap_c current_frame;

public:
    picture_c() {}
    virtual ~picture_c() {}

    virtual bool load(const ts::blob_c &body) = 0;
    virtual void fit_to_width(int w) {}
    const ts::drawable_bitmap_c &curframe() const {return current_frame;};
    const ts::ivec2 &framesize() const { return current_frame.info().sz; };
    ts::irect framerect() const { return ts::irect(0, framesize()); }

    virtual void draw(rectengine_root_c *e, const ts::ivec2 &pos) const;
};

class picture_animated_c : public picture_c
{
    struct redraw_request_s
    {
        ts::safe_ptr<rectengine_root_c> engine;
        ts::irect rr;
    };
    mutable ts::array_inplace_t<redraw_request_s, 32> rr;

    picture_animated_c *prev = nullptr;
    picture_animated_c *next = nullptr;

    static picture_animated_c *first;
    static picture_animated_c *last;

    void redraw();

protected:
    int numframes = 1;
    ts::Time next_frame_tick = ts::Time::current();

    void tick_it()
    {
        ASSERT( prev == nullptr && next == nullptr && first != this );
        if (numframes > 1)
        {
            LIST_ADD( this, first, last, prev, next );
        }
    }

public:
    /*virtual*/ ~picture_animated_c();

    /*virtual*/ bool load(const ts::blob_c &body) = 0;
    /*virtual*/ void draw(rectengine_root_c *e, const ts::ivec2 &pos) const;
    virtual int nextframe() = 0;
    static void tick();

    static bool allow_tick;

};

class picture_gif_c : public picture_animated_c
{
protected:
    ts::animated_c gif;
    int delay = 1;

public:
    /*virtual*/ bool load(const ts::blob_c &body) override;
    /*virtual*/ int nextframe() override;
};

class image_loader_c : public autoparam_i
{
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

