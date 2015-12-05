#include "isotoxin.h"

void picture_c::draw(rectengine_root_c *e, const ts::ivec2 &pos) const
{
    ts::irect r;
    const ts::drawable_bitmap_c &dbmp = curframe(r);
    e->draw(pos - e->get_current_draw_offset(), dbmp, r, true);
}

picture_animated_c *picture_animated_c::first = nullptr;
picture_animated_c *picture_animated_c::last = nullptr;
bool picture_animated_c::allow_tick = true;

/*virtual*/ picture_animated_c::~picture_animated_c()
{
    if ( prev || next || first == this )
        LIST_DEL( this, first, last, prev, next );
}

void picture_animated_c::draw(rectengine_root_c *e, const ts::ivec2 &pos) const
{
    __super::draw(e, pos);

    if (numframes > 1)
    {
        ts::irect cr(pos, pos + framerect().size());
        for (redraw_request_s &r : rr)
            if (r.engine.get() == e)
            {
                r.rr.combine(cr);
                return;
            }
        redraw_request_s &r = rr.add();
        r.engine = e;
        r.rr = cr;
    }
}


void picture_animated_c::redraw()
{
    for (redraw_request_s &r : rr)
        if (!r.engine.expired())
            r.engine->redraw(&r.rr);
    rr.clear();
}

void picture_animated_c::tick()
{
    if (!allow_tick) return; // don't animate inactive
    ts::Time curt = ts::Time::current();
    for (picture_animated_c *img = first; img; img = img->next)
    {
        if (!ASSERT(img->numframes > 1) || img->rr.size() == 0)
            continue;

        bool redraw_it = false;
        if (curt >= img->next_frame_tick)
        {
            img->next_frame_tick += img->nextframe();
            redraw_it = true;
            if (curt >= img->next_frame_tick)
                img->next_frame_tick = curt;
        }
        if (redraw_it)
            img->redraw();
    }

}

bool picture_gif_c::load_only_gif( ts::bitmap_c &first_frame, const ts::blob_c &body )
{
    gif.load(body.data(), body.size());
    numframes = gif.numframes();
    if (numframes == 0) return false;

    delay = gif.firstframe(first_frame);

    next_frame_tick = ts::Time::current() + delay;
    tick_it();
    return true;
}

/*virtual*/ bool picture_gif_c::load(const ts::blob_c &body)
{
    ts::bitmap_c bmp;
    if (!load_only_gif(bmp, body)) return false;
   
    ts::irect frect;
    ts::drawable_bitmap_c &frame = prepare_frame( bmp.info().sz, frect );
    frame.copy( frect.lt, bmp.info().sz, bmp.extbody(), ts::ivec2(0) );

    return true;
}

int picture_gif_c::nextframe()
{
    ts::irect frect;
    const ts::drawable_bitmap_c &frame = curframe(frect);

    return gif.nextframe(frame.extbody(frect));
}

namespace
{
    struct gif_thumb_s : public picture_gif_c
    {
        ts::drawable_bitmap_c frame;
        ts::bitmap_c bmp;
        ts::ivec2 origsz;
        bool rsz_required = false;
        bool frame_dirty = false;

        /*virtual*/ const ts::drawable_bitmap_c &curframe(ts::irect &frect) const override
        {
            frect = ts::irect(0, frame.info().sz);
            return frame;
        }
        /*virtual*/ ts::irect framerect() const override
        {
            return ts::irect(0,frame.info().sz);
        }
        /*virtual*/ ts::drawable_bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &frect) override
        {
            FORBIDDEN();
            __assume(0);
        }

        /*virtual*/ ts::ivec2 framesize_by_width(int w) override
        {
            if (w >= origsz.x)
                return origsz;

            float k = (float)w / (float)origsz.x;
            int newh = lround(k * origsz.y);
            return ts::ivec2(w, newh);
        }

        /*virtual*/ void fit_to_width(int w) override
        {
            if (w >= origsz.x)
            {
                // keep-original-size mode
                // bmp - empty
                // current_frame has original size and previous frame
                // new frame rendered into current_frame directly

                if (frame.info().sz != origsz)
                {
                    // previous mode is resize mode

                    if (bmp.info().sz != origsz)
                    {
                        bmp.create_RGBA(origsz);
                        frame.resize_to(bmp.extbody(), ts::FILTER_BOX_LANCZOS3);
                    }
                    frame.create_from_bitmap(bmp);
                    bmp.clear();
                }
                rsz_required = false;

            } else
            {
                float k = (float)w / (float)origsz.x;
                int newh = lround(k * origsz.y);
                if (bmp.info().sz != origsz)
                {
                    bmp.create_RGBA(origsz);
                    frame.resize_to(bmp.extbody(), ts::FILTER_BOX_LANCZOS3);
                    frame_dirty = true;
                }
                if (frame.info().sz != ts::ivec2(w, newh))
                {
                    frame.create(ts::ivec2(w, newh));
                    frame_dirty = true;
                }
                if (frame_dirty)
                    bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);

                rsz_required = true;
            }
            frame_dirty = false;
        }

        /*virtual*/ bool load(const ts::blob_c &body) override
        {
            gif.load(body.data(), body.size());
            numframes = gif.numframes();
            if (numframes == 0) return false;

            delay = gif.firstframe(bmp);
            origsz = bmp.info().sz;

            frame.create_from_bitmap(bmp);
            bmp.clear();
            rsz_required = false;

            next_frame_tick = ts::Time::current() + delay;

            tick_it();

            return true;
        }
        /*virtual*/ int nextframe() override
        {
            if (rsz_required)
            {
                frame_dirty = true;
                int r = gif.nextframe(bmp.extbody());
                bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);
                return r;
            } 
            return gif.nextframe(frame.extbody());
        }

    };
    struct static_thumb_s : public picture_c
    {
        ts::drawable_bitmap_c frame;
        ts::bitmap_c bmp;

        /*virtual*/ const ts::drawable_bitmap_c &curframe(ts::irect &frect) const override
        {
            frect = ts::irect(0, frame.info().sz);
            return frame;
        }
        /*virtual*/ ts::irect framerect() const override
        {
            return ts::irect(0, frame.info().sz);
        }
        /*virtual*/ ts::drawable_bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &frect) override
        {
            FORBIDDEN();
            __assume(0);
        }

        /*virtual*/ ts::ivec2 framesize_by_width(int w) override
        {
            if (w >= bmp.info().sz.x)
                return bmp.info().sz;

            float k = (float)w / (float)bmp.info().sz.x;
            int newh = lround(k * bmp.info().sz.y);
            return ts::ivec2(w, newh);
        }


        /*virtual*/ void fit_to_width(int w) override
        {
            if ( w >= bmp.info().sz.x )
            {
                if (frame.info().sz != bmp.info().sz)
                    frame.create(bmp.info().sz);

                frame.copy(ts::ivec2(0), bmp.info().sz, bmp.extbody(), ts::ivec2(0));
            } else
            {
                float k = (float)w / (float)bmp.info().sz.x;
                int newh = lround(k * bmp.info().sz.y);
                if (frame.info().sz != ts::ivec2(w, newh))
                    frame.create(ts::ivec2(w, newh));
                bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);
            }
            frame.premultiply();
        }

        /*virtual*/ bool load(const ts::blob_c &b) override
        {
            if (!bmp.load_from_file(b.data(), b.size()))
                return false;

            if (bmp.info().bytepp() != 4)
            {
                ts::bitmap_c b4; b4.create_RGBA(bmp.info().sz);
                b4.copy(ts::ivec2(0), bmp.info().sz, bmp.extbody(), ts::ivec2(0));
                bmp = b4;
            }

            frame.create_from_bitmap(bmp, false, true);
            return true;
        }
    };

    class pictures_cache_c
    {
        struct pic_cached_s
        {
            UNIQUE_PTR( picture_c ) pic;
            image_loader_c *first = nullptr;
            image_loader_c *last = nullptr;
        };

        ts::hashmap_t< ts::wstr_c, pic_cached_s > stuff;

        struct loading_s : public ts::task_c
        {
            loading_s() {}
            ts::wstr_c filename;
            UNIQUE_PTR(picture_c) pic;
            pictures_cache_c *cache;

            loading_s( const ts::wsptr &fn, pictures_cache_c *cache ):filename(fn), cache(cache) {}

            /*virtual*/ int iterate(int pass) override
            {
                ts::blob_c b;
                b.load_from_disk_file(filename);
                if (b.size() > 8)
                {
                    ts::uint32 sign = htonl(*(ts::uint32 *)b.data());
                    if (1195984440 == sign)
                        pic.reset( TSNEW(gif_thumb_s) );
                    else
                        pic.reset( TSNEW(static_thumb_s) );

                    pic->load(b);
                }

                return R_DONE;
            }
            /*virtual*/ void done(bool canceled) override
            {
                if (pic && !canceled)
                {
                    if (auto *x = cache->stuff.find(filename))
                    {
                        pic_cached_s &pc = x->value;
                        pc.pic = std::move(pic);
                        for (image_loader_c *imgl = pc.first; imgl; imgl = imgl->next)
                            imgl->signal_loaded(RID(), pc.pic.get());
                    }
                }

                __super::done(canceled);
            }
        };

    public:
        ~pictures_cache_c()
        {
        }
        picture_c *get(image_loader_c *by, const ts::wstr_c &filename)
        {
            pic_cached_s &pc = stuff[filename];

            LIST_ADD(by, pc.first, pc.last, prev, next);

            if (pc.pic)
                return pc.pic.get();

            if (by == pc.first)
                load( filename );

            return nullptr;
        }

        void unsubscribe(const ts::wstr_c &filename, image_loader_c *ldr)
        {
            if (auto *x = stuff.find(filename))
            {
                for (image_loader_c *y = x->value.first; y; y = y->next)
                    if ( y == ldr )
                    {
                        LIST_DEL( ldr, x->value.first, x->value.last, prev, next );
                        if (nullptr == x->value.first)
                            stuff.remove(filename);
                        break;
                    }
            }
        }

        void load(const ts::wstr_c &filename)
        {
            loading_s *l = TSNEW( loading_s, filename.as_sptr(), this );
            g_app->add_task(l);
        }

    };
}

static ts::static_setup<pictures_cache_c,1000> cache;

image_loader_c::image_loader_c(gui_message_item_c *itm, const ts::wstr_c &filename):item(itm), filename(filename)
{
    pic = cache().get(this, filename);
    if (pic)
        DEFERRED_CALL( 0, DELEGATE(this, signal_loaded), pic );
}

image_loader_c::~image_loader_c()
{
    cache().unsubscribe( filename, this );

    if (gui)
    {
        gui->delete_event(DELEGATE(this, signal_loaded));
        gui->delete_event(DELEGATE(this, upd_btnpos));
    }
}

bool image_loader_c::signal_loaded(RID, GUIPARAM p)
{
    pic = (picture_c *)p;
    if (item)
    {
        item->update_text();
        gui->repos_children( &HOLD(item->getparent()).as<gui_group_c>() );
    }
    return true;
}

bool image_loader_c::is_image_fn( const ts::asptr &fn_utf8 )
{
    if (fn_utf8.l < 4) return false;
    ts::pstr_c ext( fn_utf8.skip(fn_utf8.l-3) );
    bool is_supported = false;
    ts::enum_supported_formats([&](const char *fmt) { is_supported |= ext.equals_ignore_case(ts::asptr(fmt)); });
    return is_supported;
}

bool image_loader_c::upd_btnpos(RID r, GUIPARAM p)
{
    if ( p )
        update_ctl_pos();
    else
        if ( local_p + ts::ivec2(4) != owner->getprops().pos() )
            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, upd_btnpos), 1);
    return true;
}

void image_loader_c::update_ctl_pos()
{
    const button_desc_s *explorebdsc = g_app->buttons().exploreb;

    if (item)
    {
        ts::irect rr( owner->getprops().pos(), owner->getprops().pos() + explorebdsc->size );
        item->getengine().redraw( &rr ); // redraw old position of button

        MODIFY(*owner).pos(local_p + ts::ivec2(4)).size(explorebdsc->size).visible(true);
        //MODIFY(*owner).pos(5, 5).size(explorebdsc->size);
    }

}

/*virtual*/ bool image_loader_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (!ASSERT(owner)) return false;
    if (owner->getrid() != rid) return false;

    if (qp == SQ_PARENT_RECT_CHANGING)
    {
        return false;
    }

    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_ctl_pos();
        return false;
    }

    return false;
}
