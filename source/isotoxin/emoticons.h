#pragma once

struct emoticon_s
{
    struct redraw_request_s
    {
        ts::safe_ptr<rectengine_root_c> engine;
        ts::irect rr;
    };

    emoticon_s( int unicode ):unicode(unicode == 1 ? 0 : unicode) {}
    virtual ~emoticon_s();

    ts::drawable_bitmap_c current_frame;
    ts::ivec2 framesize;
    ts::str_c def; // utf8
    ts::str_c repl; // utf8
    int numframes = 1;
    int unicode = 0;
    int sort_dactor = 100000;
    bool current_pack = false;

    mutable ts::array_inplace_t<redraw_request_s, 32> rr;
    void draw(rectengine_root_c *e, const ts::ivec2 &pos) const;
    void redraw();

    ts::Time next_frame_tick = ts::Time::current();

    bool load(const ts::wsptr &fn);
    virtual bool load(const ts::blob_c &body) = 0;

    virtual ts::irect framerect() const
    {
        return ts::irect(0, framesize);
    }
    virtual int nextframe() = 0;

    gui_textedit_c::active_element_s * ee = nullptr;
    gui_textedit_c::active_element_s * get_edit_element(int maxh);
};

class emoticons_c
{
    GM_RECEIVER(emoticons_c, GM_UI_EVENT);

    struct match_point_s
    {
        const emoticon_s *e;
        ts::str_c s;
    };

    ts::array_inplace_t< match_point_s, 128 > matchs;

    struct emo_gif_s : public emoticon_s
    {
        ts::animated_c gif;
        int delay = 1;
        emo_gif_s(int unicode):emoticon_s(unicode) {}
        /*virtual*/ bool load(const ts::blob_c &body) override;
        /*virtual*/ int nextframe() override;
    };

    struct emo_static_image_s : public emoticon_s
    {
        emo_static_image_s(int unicode):emoticon_s(unicode) {}
        /*virtual*/ bool load(const ts::blob_c &body) override;
        /*virtual*/ int nextframe() override;
    };

    ts::array_del_t<emoticon_s, 8> arr;
    bool allow_tick = true;

    bool process_pak_file(const ts::arc_file_s &f);

    ts::wstrings_c packs;

public:

    int maxheight = 0;

    template<typename F> void iterate_current_pack( F f )
    {
        for (emoticon_s *ae : arr)
            if (ae->current_pack)
                f(*ae);
    }

    void reload();

    void parse( ts::str_c &t, bool to_unicode = false );
    
    void draw( rectengine_root_c *e, const ts::ivec2 &p, int smlnum );

    void tick();

    const emoticon_s* get(int i) const
    {
        if (i >= 0 && i<arr.size())
            return arr.get(i);
        return nullptr;
    }

    ts::str_c load_gif_smile( const ts::wstr_c& fn, const ts::blob_c &body, bool current_pack );
    ts::str_c load_png_smile( const ts::wstr_c& fn, const ts::blob_c &body, bool current_pack );

    menu_c get_list_smile_pack( const ts::wstr_c &curpack, MENUHANDLER mh );

};


extern ts::static_setup<emoticons_c,1000> emoti;