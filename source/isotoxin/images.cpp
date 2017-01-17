#include "isotoxin.h"

#ifdef _WIN32
#define _NTOS_
#pragma push_macro("ERROR")
#undef ERROR
#include <winsock2.h> // htonl
#pragma pop_macro("ERROR")
#endif // _WIN32
#ifdef _NIX
#include <arpa/inet.h> // htonl
#endif // _NIX

void picture_c::draw(rectengine_root_c *e, const ts::ivec2 &pos) const
{
    ts::irect r;
    const ts::bitmap_c &dbmp = curframe(r);
    e->draw(pos - e->get_current_draw_offset(), dbmp.extbody(r), true);
}

/*virtual*/ picture_animated_c::~picture_animated_c()
{
}

void picture_animated_c::draw(rectengine_root_c *e, const ts::ivec2 &pos) const
{
    super::draw(e, pos);

    if (numframes > 1)
    {
        ts::irect cr(pos, pos + framerect().size());
        const_cast<picture_animated_c *>(this)->register_animation(e, cr);
    }
}


bool picture_gif_c::load_only_gif( ts::bitmap_c &first_frame, const ts::blob_c &body )
{
    gif.load(body.data(), body.size());
    numframes = gif.numframes();
    if (numframes == 0) return false;
    next_frame_tick = ts::Time::current() + gif.firstframe(first_frame);
    return true;
}

/*virtual*/ bool picture_gif_c::load (const ts::blob_c &body, ts::IMG_LOADING_PROGRESS /*progress*/ )
{
    ts::bitmap_c bmp;
    if (!load_only_gif(bmp, body)) return false;

    ts::irect frect;
    ts::bitmap_c &frame = prepare_frame( bmp.info().sz, frect );
    frame.copy( frect.lt, bmp.info().sz, bmp.extbody(), ts::ivec2(0) );

    return true;
}

/*virtual*/ bool picture_gif_c::animation_tick()
{
    ts::Time curt = ts::Time::current();

    if (curt >= next_frame_tick)
    {
        next_frame_tick += nextframe();
        if (curt >= next_frame_tick)
            next_frame_tick = curt;
        return true;
    }

    return false;
}

int picture_gif_c::nextframe()
{
    ts::irect frect;
    const ts::bitmap_c &frame = curframe(frect);

    return gif.nextframe(frame.extbody(frect));
}

size_t picture_gif_c::size() const
{
    return gif.size();
}

static void decrease_size( size_t sz );

namespace
{
    struct gif_thumb_s : public picture_gif_c
    {
        ts::bitmap_c frame;
        ts::bitmap_c bmp;
        ts::ivec2 origsz;
        bool rsz_required = false;
        bool frame_dirty = false;

        /*virtual*/ const ts::bitmap_c &curframe(ts::irect &frect) const override
        {
            frect = ts::irect(0, frame.info().sz);
            return frame;
        }
        /*virtual*/ ts::irect framerect() const override
        {
            return ts::irect(0,frame.info().sz);
        }
        /*virtual*/ size_t size() const override
        {
            return picture_gif_c::size();
        }
        /*virtual*/ ts::bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &frect) override
        {
            FORBIDDEN();
            UNREACHABLE();
        }

        /*virtual*/ ts::ivec2 framesize_by_width(int w) override
        {
            if (!prf_options().is(MSGOP_MAXIMIZE_INLINE_IMG))
            {
                int maxthumbh = prf().max_thumb_height();
                if (maxthumbh < 10) maxthumbh = 10;
                if (origsz.y <= maxthumbh)
                    goto maybenoresize;

                float k = (float)maxthumbh / (float)origsz.y;
                int newx = ts::lround(k * origsz.x);
                if (newx > w)
                    goto fit2width;

                return ts::ivec2(newx, maxthumbh);
            }

            maybenoresize:
            if (w >= origsz.x)
                return origsz;

            fit2width:
            float k = (float)w / (float)origsz.x;
            int newh = ts::lround(k * origsz.y);
            return ts::ivec2(w, newh);
        }

        /*virtual*/ void fit_to_width(int w) override
        {
            ts::ivec2 newsize;
            if (!prf_options().is(MSGOP_MAXIMIZE_INLINE_IMG))
            {
                int maxthumbh = prf().max_thumb_height();
                if (maxthumbh < 10) maxthumbh = 10;

                if (origsz.y <= maxthumbh)
                    goto maybenoresize;

                float k = (float)maxthumbh / (float)origsz.y;
                int newx = ts::lround(k * origsz.x);
                if (newx > w)
                    goto fit2width;

                newsize = ts::ivec2(newx, maxthumbh);
                goto do_resize;
            }

            maybenoresize:
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
                        bmp.create_ARGB(origsz);
                        frame.resize_to(bmp.extbody(), ts::FILTER_BOX_LANCZOS3);
                    }
                    frame = std::move(bmp);
                }
                rsz_required = false;

            } else
            {
            fit2width:
                {
                    newsize.x = w;
                    float k = (float)w / (float)origsz.x;
                    newsize.y = ts::lround(k * origsz.y);
                }
            do_resize:
                if (bmp.info().sz != origsz)
                {
                    bmp.create_ARGB(origsz);
                    frame.resize_to(bmp.extbody(), ts::FILTER_BOX_LANCZOS3);
                    frame_dirty = true;
                }
                if (frame.info().sz != newsize)
                {
                    frame.create_ARGB(newsize);
                    frame_dirty = true;
                }
                if (frame_dirty)
                    bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);

                rsz_required = true;
            }
            frame_dirty = false;
        }

        /*virtual*/ bool load( const ts::blob_c &body, ts::IMG_LOADING_PROGRESS /*progress*/ ) override
        {
            if (!gif.load(body.data(), body.size()))
            {
                rsz_required = false;
                next_frame_tick = ts::Time::current();
                origsz = ts::ivec2(32);
                frame.create_ARGB(origsz);
                frame.fill(ts::ARGB(255,0, 255));
                numframes = 1;
                return true;
            }
            numframes = gif.numframes();
            if (numframes == 0) return false;

            int delay = gif.firstframe(bmp);
            origsz = bmp.info().sz;

            frame = std::move(bmp);
            rsz_required = false;

            next_frame_tick = ts::Time::current() + delay;
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
        ts::bitmap_c frame;
        ts::bitmap_c bmp;

        /*virtual*/ ~static_thumb_s()
        {
        }

        /*virtual*/ const ts::bitmap_c &curframe(ts::irect &frect) const override
        {
            frect = ts::irect(0, frame.info().sz);
            return frame;
        }
        /*virtual*/ ts::irect framerect() const override
        {
            return ts::irect(0, frame.info().sz);
        }
        /*virtual*/ size_t size() const override
        {
            return bmp.info().sz.x * bmp.info().sz.y * bmp.info().bytepp();
        }
        /*virtual*/ ts::bitmap_c &prepare_frame(const ts::ivec2 &sz, ts::irect &frect) override
        {
            FORBIDDEN();
            UNREACHABLE();
        }

        /*virtual*/ ts::ivec2 framesize_by_width(int w) override
        {
            if (!prf_options().is(MSGOP_MAXIMIZE_INLINE_IMG))
            {
                int maxthumbh = prf().max_thumb_height();
                if (maxthumbh < 10) maxthumbh = 10;

                if (bmp.info().sz.y <= maxthumbh)
                    goto maybenoresize;

                float k = (float)maxthumbh / (float)bmp.info().sz.y;
                int newx = ts::lround(k * bmp.info().sz.x);
                if (newx > w)
                    goto fit2width;

                return ts::ivec2(newx, maxthumbh);
            }

            maybenoresize:
            if (w >= bmp.info().sz.x)
                return bmp.info().sz;

            fit2width:
            float k = (float)w / (float)bmp.info().sz.x;
            int newh = ts::lround(k * bmp.info().sz.y);
            return ts::ivec2(w, newh);
        }


        /*virtual*/ void fit_to_width(int w) override
        {
            if (!prf_options().is(MSGOP_MAXIMIZE_INLINE_IMG))
            {
                int maxthumbh = prf().max_thumb_height();
                if (maxthumbh < 10) maxthumbh = 10;

                if (bmp.info().sz.y <= maxthumbh)
                    goto maybenoresize;

                float k = (float)maxthumbh / (float)bmp.info().sz.y;
                int newx = ts::lround(k * bmp.info().sz.x);
                if (newx > w)
                    goto fit2width;

                if (frame.info().sz != ts::ivec2(newx, maxthumbh))
                    frame.create_ARGB(ts::ivec2(newx, maxthumbh));
                bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);

            } else
            {
            maybenoresize:

                if (w >= bmp.info().sz.x)
                {
                    frame = bmp.extbody();
                }
                else
                {
                fit2width:
                    float k = (float)w / (float)bmp.info().sz.x;
                    int newh = ts::lround(k * bmp.info().sz.y);
                    if (frame.info().sz != ts::ivec2(w, newh))
                        frame.create_ARGB(ts::ivec2(w, newh));
                    bmp.resize_to(frame.extbody(), ts::FILTER_BOX_LANCZOS3);
                }
            }

            frame.premultiply();
        }

        /*virtual*/ bool load( const ts::blob_c &b, ts::IMG_LOADING_PROGRESS progress ) override
        {
            if (!bmp.load_from_file(b.data(), b.size(), ts::ivec2(1024), progress))
                return false;

            if (bmp.info().bytepp() != 4)
            {
                ts::bitmap_c b4;
                b4 = bmp.extbody();
                bmp = b4;
            }

            frame = bmp;
            frame.premultiply();
            return true;
        }
    };

#define _SWAP_LONG(l)                \
            ( ( ((l) >> 24) & 0x000000FFL ) |       \
              ( ((l) >>  8) & 0x0000FF00L ) |       \
              ( ((l) <<  8) & 0x00FF0000L ) |       \
              ( ((l) << 24) & 0xFF000000L ) )

#if LITTLEENDIAN
    long INLINE htonl( long l ) {
        return _SWAP_LONG( l );
    }
#endif


    class pictures_cache_c
    {
        struct loading_s;

        struct pic_cached_s
        {
            DECLARE_EYELET( pic_cached_s );
        public:
            UNIQUE_PTR( picture_c ) pic;
            ts::iweak_ptr<loading_s> loader;
            image_loader_c *first = nullptr;
            image_loader_c *last = nullptr;
            ts::aint size = 0;
            ~pic_cached_s()
            {
                if (size > 0)
                    decrease_size( size );

                if ( loader )
                    noneed_loader( loader );
            }
        };

        static void noneed_loader( loading_s *l );

        ts::hashmap_t< ts::wstr_c, pic_cached_s > stuff;
        size_t size = 0;

        void check()
        {
#ifdef _DEBUG

            for( auto it = stuff.begin(); it; ++it )
            {
                pic_cached_s &pc = it.value();
                for ( image_loader_c *imgl = pc.first; imgl; imgl = imgl->next )
                {
                    ASSERT( it.key().equals( imgl->get_fn() ) );
                }
            }

#endif // _DEBUG
        }

        struct loading_s : public ts::task_c
        {
            DECLARE_EYELET( loading_s );
        public:

            loading_s() {}
            ts::wstr_c filename;
            UNIQUE_PTR(picture_c) pic;
            pictures_cache_c *cache;
            ts::aint progress = 0;
            ts::task_executor_c *papa;
            bool no_need = false;

            loading_s( const ts::wsptr &fn, pictures_cache_c *cache ):filename(fn), cache(cache) {}

            bool loading( int row, int rows )
            {
                if (rows)
                    progress = row * 100 / rows;
                return no_need || should_stop(papa);
            }

            /*virtual*/ int iterate(ts::task_executor_c *e) override
            {
                papa = e;
                ts::blob_c b;
                b.load_from_disk_file(filename,false,10*1024*1024);
                if (b.size() > 8)
                {
                    ts::uint32 sign = htonl(*(ts::uint32 *)b.data());
                    if (1195984440 == sign)
                        pic.reset( TSNEW(gif_thumb_s) );
                    else
                        pic.reset( TSNEW(static_thumb_s) );

                    if ( !pic->load( b, DELEGATE(this, loading) ) )
                        pic.reset();
                }

                return no_need ? R_CANCEL : R_DONE;
            }
            /*virtual*/ void done(bool canceled) override
            {
                bool n = false;
                if (pic && !canceled && !no_need)
                {
                    if (auto *x = cache->stuff.find(filename))
                    {
                        pic_cached_s &pc = x->value;
                        pc.pic = std::move(pic);

                        if (pc.size > 0)
                            cache->size -= pc.size;

                        pc.size = pc.pic->size();
                        cache->size += pc.size;

                        ASSERT( pc.loader.get() == this );
                        pc.loader = nullptr;

                        for (image_loader_c *imgl = pc.first; imgl; imgl = imgl->next)
                            imgl->signal_loaded(RID(), pc.pic.get());

                        n = true;
                    }
                }

                if (!n && !no_need)
                    if ( auto *x = cache->stuff.find( filename ) )
                    {
                        pic_cached_s &pc = x->value;
                        ASSERT( pc.loader.get() == this );
                        pc.loader = nullptr;
                        ts::iweak_ptr<pic_cached_s> pcguard = &pc;
                        for ( image_loader_c *imgl = pc.first; imgl; imgl = imgl->next )
                        {
                            imgl->not_loaded();
                            if ( pcguard.expired() ) break;
                        }
                    }

                ts::task_c::done(canceled);
            }
        };

    public:
        ~pictures_cache_c()
        {
        }

        void decrease_cache_size(size_t sz)
        {
            ASSERT( size >= sz );
            size -= sz;
        }

        void cleanup()
        {
            if (size > 10000000) // FREE MEMORY
            {
                ts::Time oldest = ts::Time::current();
                ts::wstr_c fn;

                for( auto itr = stuff.begin(); itr; ++itr )
                {
                    ts::Time newest = ts::Time::past();
                    for ( image_loader_c *imgl = itr->first; imgl; imgl = imgl->next )
                    {
                        if ( imgl->get_last_draw_time() > newest )
                            newest = imgl->get_last_draw_time();
                    }
                    if ( newest < oldest )
                    {
                        oldest = newest;
                        fn = itr.key();
                    }
                }

                if ( !fn.is_empty() )
                {
                    pic_cached_s &pc = stuff.find( fn )->value;
                    ts::iweak_ptr<pic_cached_s> pcguard = &pc;
                    for ( ;pcguard && pc.first; )
                        pc.first->unloaded();
                    ASSERT( nullptr == stuff.find( fn ) );

                    check();
                }
            }
        }

        picture_c *try_get( image_loader_c *by, const ts::wstr_c &filename )
        {
            if (auto *f = stuff.find( filename ))
                if ( f->value.pic )
                {
                    pic_cached_s &pc = f->value;
                    if ( by->prev == nullptr && by->next == nullptr && pc.first != by )
                        LIST_ADD( by, pc.first, pc.last, prev, next );

                    check();

                    return pc.pic.get();
                }
            return nullptr;
        }

        picture_c *get(image_loader_c *by, const ts::wstr_c &filename)
        {
            pic_cached_s &pc = stuff[filename];

            if ( by->prev == nullptr && by->next == nullptr && pc.first != by )
                LIST_ADD(by, pc.first, pc.last, prev, next);

            check();

            if (pc.pic)
                return pc.pic.get();

            ASSERT( pc.size <= 0 );

            if ( pc.loader == nullptr )
            {
                loading_s *l = TSNEW( loading_s, filename.as_sptr(), this );
                pc.loader = l;
                g_app->add_task( l );
            }

            return nullptr;
        }

        void unsubscribe(const ts::wstr_c &filename, image_loader_c *ldr)
        {
            if (auto *x = stuff.find(filename))
            {
                for (image_loader_c *y = x->value.first; y; y = y->next)
                    if ( y == ldr )
                    {
                        LIST_DEL_CLEAR( ldr, x->value.first, x->value.last, prev, next );
                        if (nullptr == x->value.first)
                            stuff.remove(filename);

                        check();

                        break;
                    }
            }
        }

        ts::aint loading_progress( const ts::wstr_c &filename )
        {
            if ( auto *x = stuff.find( filename ) )
            {
                if ( x->value.loader )
                    return x->value.loader->progress;

            }
            return 0;
        }

    };

    void pictures_cache_c::noneed_loader( loading_s *l )
    {
        l->no_need = true;
    }
}

static ts::static_setup<pictures_cache_c,1000> cache;

static void decrease_size( size_t sz )
{
    cache().decrease_cache_size( sz );
}

image_loader_c::image_loader_c(gui_message_item_c *itm, const ts::wstr_c &filename):item(itm), filename(filename)
{
    pic = cache().try_get(this, filename);
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

void image_loader_c::on_draw()
{
    if ( !pic )
    {
        pic = cache().get( this, filename );

        if ( pic )
            DEFERRED_CALL( 0, DELEGATE( this, signal_loaded ), pic );
    }
    last_draw = ts::Time::current();
}

void image_loader_c::not_loaded()
{
    if (gui->is_dip()) return;

    if ( gui_message_item_c *itm = item )
    {
        itm->image_not_loaded();
        itm->update_text();
        gui->repos_children( &HOLD( itm->getparent() ).as<gui_group_c>() );
    }
    if ( explorebtn.expired() )
        TSDEL( explorebtn.get() );
}

void image_loader_c::unloaded()
{
    if ( gui_message_item_c *itm = item )
    {
        itm->image_unloaded();

        itm->disable_image_loading(true);
            itm->update_text(); // it will try to run image loader. That is why disable_image_loading used
        itm->disable_image_loading(false);

        gui->repos_children( &HOLD( itm->getparent() ).as<gui_group_c>() );
    }
    if ( explorebtn.expired() )
        TSDEL( explorebtn.get() );
}

bool image_loader_c::signal_loaded(RID, GUIPARAM p)
{
    pic = (picture_c *)p;
    if ( CHECK( item ) && g_app )
    {
        item->update_text();
        gui->repos_children( &HOLD(item->getparent()).as<gui_group_c>() );
    }
    return true;
}

int image_loader_c::get_loading_progress()
{
    if ( pic )
        return 100;

    return (int)cache().loading_progress( filename );
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
        if (owner && local_p + ts::ivec2(4) != owner->getprops().pos() )
            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, upd_btnpos), 1);
    return true;
}

void image_loader_c::update_ctl_pos()
{
    const button_desc_s *explorebdsc = g_app->preloaded_stuff().exploreb;

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

void cleanup_images_cache()
{
    cache().cleanup();
}

