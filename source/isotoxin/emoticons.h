#pragma once

struct emoticon_s
{
    emoticon_s( int unicode ):unicode(unicode == 1 ? 0 : unicode) {}
    virtual ~emoticon_s();

    ts::str_c def; // utf8
    ts::str_c repl; // utf8
    int unicode = 0;
    int sort_factor = 100000;

    ts::irect frect = ts::irect(0);

    bool current_pack = false;
    bool ispreframe = false;

    union 
    {
        ts::bitmap_c *frame;
        ts::bitmap_c *preframe;
    };

    bool load(const ts::wsptr &fn);
    virtual bool load(const ts::blob_c &body) = 0;

    virtual void draw(rectengine_root_c *e, const ts::ivec2 &pos) const = 0;
    virtual const ts::bitmap_c &curframe(ts::irect &frect) const = 0;
    virtual ts::irect framerect() const = 0;

    gui_textedit_c::active_element_s * ee = nullptr;
    gui_textedit_c::active_element_s * get_edit_element(int maxh);
};

class emoticons_c
{
    struct match_point_s
    {
        const emoticon_s *e;
        ts::str_c s;
    };

    ts::bitmap_c fullframe;
    ts::array_inplace_t< match_point_s, 128 > matchs;

    struct emo_gif_s : public emoticon_s, public picture_gif_c
    {
        emo_gif_s(int unicode):emoticon_s(unicode) {}
        /*virtual*/ bool load(const ts::blob_c &body) override;
        
        /*virtual*/ void draw(rectengine_root_c *e, const ts::ivec2 &pos) const { picture_gif_c::draw(e,pos); }

        /*virtual*/ const ts::bitmap_c &curframe(ts::irect &fr) const override
        {
            ASSERT(!ispreframe);
            fr = frect;
            return *frame;
        }
        /*virtual*/ ts::irect framerect() const override { return frect; }
        /*virtual*/ ts::bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &fr) override
        {
            FORBIDDEN();
            __assume(0);
        }
        /*virtual*/ int nextframe() override { int r = picture_gif_c::nextframe(); adapt_bg(nullptr); return r; }
        void adapt_bg(const ts::bitmap_c *bmpx);
    };

    struct emo_static_image_s : public emoticon_s, public picture_c
    {
        emo_static_image_s(int unicode):emoticon_s(unicode) {}
        /*virtual*/ bool load(const ts::blob_c &body) override;

        /*virtual*/ void draw(rectengine_root_c *e, const ts::ivec2 &pos) const { picture_c::draw(e,pos); }

        /*virtual*/ const ts::bitmap_c &curframe(ts::irect &fr) const override
        {
            ASSERT(!ispreframe);
            fr = frect;
            return *frame;
        }
        /*virtual*/ ts::irect framerect() const override { return frect; }
        /*virtual*/ ts::bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &fr) override
        {
            FORBIDDEN();
            __assume(0);
        }
    };

    ts::array_del_t<emoticon_s, 8> arr;

    bool process_pak_file(const ts::arc_file_s &f);

    ts::wstrings_c packs;

    void generate_full_frame();

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