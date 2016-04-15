#include "isotoxin.h"

#define GRAB_DESKTOP_FPS 15

using namespace DShow;

#ifdef DSHOW_LOGGING
static void lcb(DShow::LogType type, const wchar_t *msg, void *param)
{
    switch(type)
    {
        case LogType::Debug:
            OutputDebugStringW(ts::wstr_c(CONSTWSTR("DShow Debug: "), ts::wsptr(msg)).append_char('\n'));
            break;
        case LogType::Warning:
            OutputDebugStringW(ts::wstr_c(CONSTWSTR("DShow Warning: "), ts::wsptr(msg)).append_char('\n'));
            break;
        case LogType::Info:
            OutputDebugStringW(ts::wstr_c(CONSTWSTR("DShow Info: "), ts::wsptr(msg)).append_char('\n'));
            break;
        case LogType::Error:
            OutputDebugStringW(ts::wstr_c(CONSTWSTR("DShow Error: "), ts::wsptr(msg)).append_char('\n'));
            break;
        
    }
}
#endif

void enum_video_capture_devices(vsb_list_t &list, bool add_desktop)
{
#ifdef DSHOW_LOGGING
    DShow::SetLogCallback(lcb, nullptr);
#endif

    std::vector<VideoDevice> devices;
    Device::EnumVideoDevices(devices);

    for (const VideoDevice &vd : devices)
    {
        vsb_descriptor_s &d = list.add();
        d.desc.set(vd.name.c_str(), vd.name.length());
        d.id.set(vd.path.c_str(), vd.path.length());

        for (const VideoInfo&vi : vd.caps)
            if (vi.format == VideoFormat::XRGB)
                d.resolutions.set( ts::ivec2( vi.maxCX, vi.maxCY ) );
        
        d.resolutions.tsort<ts::ivec2>([](const ts::ivec2 *a, const ts::ivec2 *b)->bool {
            if (*a == *b) return false;
            if (a->x > b->x) return true;
            if (a->x < b->x) return false;
            if (a->y > b->y) return true;
            if (a->y < b->y) return false;
            return false;
        });

        int cnt = d.resolutions.count();
        for (int i = 1; i < cnt; ++i)
            if (d.resolutions.get(i).x < 640)
            {
                d.resolutions.set_count(i, true);
                break;
            }
    }

    if (add_desktop)
    {
        int mc = ts::monitor_count();
        if (1 == mc)
        {
            vsb_descriptor_s &d = list.add();
            d.desc = TTT("Desktop", 350);
            d.id = CONSTWSTR("desktop");

            ts::irect r = ts::monitor_get_max_size_fs(0);
            d.resolutions.add( r.size() );
            d.resolutions.add( r.size()/2 );
            d.resolutions.add( r.size()/4 );

        }
        else
        {
            for (int m = 0; m < mc; ++m)
            {
                vsb_descriptor_s &d = list.add();
                d.desc = TTT("Desktop $", 351) / ts::monitor_get_description(m);
                d.id = CONSTWSTR("desktop/"); d.id.append_as_uint(m);

                ts::irect r = ts::monitor_get_max_size_fs(m);
                d.resolutions.add(r.size());
                d.resolutions.add(r.size() / 2);
                d.resolutions.add(r.size() / 4);

            }
        }
    }
}

vsb_c *vsb_c::build()
{
    ts::wstrmap_c c(cfg().device_camera());
    if (nullptr == c.find(CONSTWSTR("id")))
        c.set(CONSTWSTR("id")) = CONSTWSTR("desktop");

    vsb_descriptor_s desc;
    desc.id = *c.find(CONSTWSTR("id"));
    return build(desc, c);
}

vsb_c *vsb_c::build( const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar)
{
    if (!desc.id.begins(CONSTWSTR("desktop")))
    {
        vsb_dshow_camera_c *cam = TSNEW(vsb_dshow_camera_c);
        if (cam->init(desc, dpar))
            return cam;
        TSDEL(cam);
    }

    vsb_desktop_c *cam = TSNEW( vsb_desktop_c );
    if (cam->init(desc, dpar))
        return cam;
        
    TSDEL(cam);
    return nullptr;
}

ts::ivec2 vsb_c::fit_to_size( const ts::ivec2 &sz_ )
{
    if (sz_ == ts::ivec2(0))
    {
        ts::ivec2 s = get_video_size();
        set_desired_size( s );
        return s;
    }

    if(initializing) return sz_;

    ts::ivec2 sz = sz_;
    sz.y &= ~1;

    int x_by_y = (video_size.x * sz.y / video_size.y) & (~1);
    if (x_by_y <= sz.x)
    {
        sz.x = x_by_y;
    } else
    {
        sz.x &= ~1;
        int y_by_x = (video_size.y * sz.x / video_size.x) & (~1);
        ASSERT(y_by_x <= sz.y);
        sz.y = y_by_x;
    }
    ASSERT( 0 == ((sz.x | sz.y) & 1) );
    set_desired_size(sz);
    return sz;
}

vsb_desktop_c::grab_desktop *vsb_desktop_c::grab_desktop::first = nullptr;
vsb_desktop_c::grab_desktop *vsb_desktop_c::grab_desktop::last = nullptr;

vsb_desktop_c::grab_desktop::grab_desktop(const ts::irect &grabrect, int monitor):grabrect(grabrect), monitor(monitor), next_time( ts::Time::current() + ( 1000 / GRAB_DESKTOP_FPS ) )
{
    LIST_ADD(this, first, last, prev, next);
    g_app->add_task(this);
}
vsb_desktop_c::grab_desktop::~grab_desktop()
{
    LIST_DEL(this, first, last, prev, next);
}

void vsb_desktop_c::grab_desktop::get(vsb_desktop_c *owner, const ts::irect &grabrect, int monitor)
{
    for (grab_desktop *f = first; f; f = f->next)
        if (f->grabrect == grabrect && f->monitor == monitor)
            return f->add_owner(owner);

    grab_desktop *newcore = TSNEW(grab_desktop, grabrect, monitor);
    newcore->add_owner(owner);
}

void vsb_desktop_c::grab_desktop::add_owner(vsb_desktop_c *owner)
{
    spinlock::auto_simple_lock l(sync);

    for (vsb_desktop_c *ptr : owners)
        if (ptr == owner)
        {
            ASSERT(owner->grabber == this);
            return;
        }

    owners_changed = true;
    owners.add(owner);
    owner->grabber = this;
    if (owners.size() >= 2)
        owner->set_video_size(owners.get(0)->get_video_size());
}

void vsb_desktop_c::grab_desktop::remove_owner(vsb_desktop_c *owner)
{
    spinlock::auto_simple_lock l(sync);

    while (locked == owner)
    {
        l.unlock();
        ts::master().sys_sleep(1);
        l.lock(sync);
    }

    int cnt = owners.size();
    for (int i = 0; i < cnt; ++i)
    {
        if (owners.get(i) == owner)
        {
            owners_changed = true;
            owners.remove_fast(i);
            owner->grabber = nullptr;

            if (owners.size() == 0)
            {
                grabrect = ts::irect(0);
                stop_job = true;
            }

            l.unlock();
            return;
        }
    }
}

void vsb_desktop_c::grab_desktop::grab(const ts::irect &gr)
{
    if (gr.size() != grabbuff.info().sz)
        grabbuff.create(gr.size(), monitor);

    grabbuff.grab_screen( gr, ts::ivec2(0) );
    grabbuff.render_cursor(gr.lt, cursorcachedata);

}

/*virtual*/ int vsb_desktop_c::grab_desktop::iterate(int pass)
{
    spinlock::simple_lock(sync);
    if (stop_job)
    {
        spinlock::simple_unlock(sync);
        return R_CANCEL;
    }

    ts::Time curt = ts::Time::current();
    if ((curt - next_time) < 0)
    {
        spinlock::simple_unlock(sync);
        ts::master().sys_sleep(1);
        return stop_job ? R_CANCEL : 1;
    }

    ts::irect gr = grabrect;
    spinlock::simple_unlock(sync);
    grab(gr);
    spinlock::simple_lock(sync);

    if (stop_job)
    {
        spinlock::simple_unlock(sync);
        return R_CANCEL;
    }

    ++grabtag;
    bool some_processed = false;
    do
    {
        some_processed = false;
        for (vsb_desktop_c * c : owners)
        {
            if (c->grabtag == grabtag) continue;
            c->grabtag = grabtag;
            locked = c;
            owners_changed = false;
            spinlock::simple_unlock(sync);

            c->grabcb(grabbuff);

            spinlock::simple_lock(sync);
            locked = nullptr;
            some_processed = true;
            if (owners_changed) break;
        }
    } while (some_processed);

    next_time += 1000/GRAB_DESKTOP_FPS;
    
    if ( (curt-next_time) > 1000 )
        next_time = curt; // tooooo slooooooow grabbing :(

    spinlock::simple_unlock(sync);
    return stop_job ? R_CANCEL : 1;
}

void vsb_desktop_c::grabcb(ts::drawable_bitmap_c &gbmp)
{
    ts::ivec2 dsz;
    if (ts::drawable_bitmap_c *b = lockbuf(&dsz))
    {
        if (dsz == ts::ivec2(0))
            dsz = rect.size();

        if (dsz.x > maxsize.x && maxsize.x > 0)
            dsz = maxsize;

        if (dsz == rect.size())
        {
            // just copy
            if (b->info().sz != dsz)
                b->create(dsz, monitor);

            b->copy( ts::ivec2(0), dsz, gbmp.extbody(), ts::ivec2(0) );
        }
        else
        {
            if (b->info().sz != dsz)
                b->create(dsz);

            b->resize_from(gbmp.extbody(), ts::FILTER_BOX_LANCZOS3);
        }
        call_bmp_ready_handler(b->extbody());
        unlock(b);
    }

}

bool vsb_desktop_c::init(const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar)
{
    monitor = 0;
    ts::token<ts::wchar> t( desc.id, '/' ); ++t;
    if (t) monitor = t->as_int();

    maxsize = ts::parsevec2(ts::to_str(dpar.get(CONSTWSTR("res"))), ts::ivec2(0));

    rect = ts::monitor_get_max_size_fs(monitor);
    set_video_size(rect.size());

    grab_desktop::get( this, rect, monitor );

    return true;
}


//////////////// dshow camera

vsb_dshow_camera_c::core_c *vsb_dshow_camera_c::core_c::first = nullptr;
vsb_dshow_camera_c::core_c *vsb_dshow_camera_c::core_c::last = nullptr;

vsb_dshow_camera_c::core_c::core_c(const vsb_descriptor_s &desc_):id(desc_.id)
{
    LIST_ADD( this, first, last, prev, next );
    initializing = true;
}

vsb_dshow_camera_c::core_c::~core_c()
{
    LIST_DEL( this, first, last, prev, next );
}

bool vsb_dshow_camera_c::core_c::get(vsb_dshow_camera_c *owner, const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar)
{
    for( core_c *f = first; f; f = f->next )
        if (f->id.equals(desc.id))
            return f->add_owner( owner );

    core_c *newcore = TSNEW(core_dshow_c, desc, dpar);
    return newcore->add_owner(owner);
}

bool vsb_dshow_camera_c::core_c::add_owner( vsb_dshow_camera_c *owner )
{
    spinlock::auto_simple_lock l(sync);

    for( vsb_dshow_camera_c *ptr : owners )
        if (ptr == owner)
        {
            ASSERT( owner->core == this );
            return initializing;
        }

    owners_changed = true;
    owners.add( owner );
    owner->core = this;
    if (owners.size() >= 2)
        owner->set_video_size( owners.get(0)->get_video_size() );
    return initializing;
}

void vsb_dshow_camera_c::core_c::remove_owner( vsb_dshow_camera_c *owner )
{
    spinlock::auto_simple_lock l(sync);

    while (locked == owner)
    {
        l.unlock();
        ts::master().sys_sleep(1);
        l.lock(sync);
    }

    int cnt = owners.size();
    for(int i=0;i<cnt;++i)
    {
        if ( owners.get(i) == owner )
        {
            owners_changed = true;
            ts::shared_ptr<core_c> refcore = this; // up ref
            owners.remove_fast(i);
            owner->core = nullptr;
            l.unlock();
            return;
        }
    }
}

void vsb_dshow_camera_c::core_c::initialized( const ts::ivec2 &videosize )
{
    spinlock::auto_simple_lock l(sync);
    busy = false;
    for (vsb_dshow_camera_c * c : owners)
        c->initialized(videosize);
}

void vsb_dshow_camera_c::core_c::setbusy()
{
    spinlock::auto_simple_lock l(sync);
    busy = false;
    for (vsb_dshow_camera_c * c : owners)
        c->busy = true;
}



vsb_dshow_camera_c::core_dshow_c::core_dshow_c(const vsb_descriptor_s &desc_, const ts::wstrmap_c &dpar):core_c(desc_), Device(DShow::InitGraph::True)
{
    VideoConfig config;

    config.name = desc_.desc;
    config.path = desc_.id;
    config.callback = DELEGATE(this, dshocb);
    
    ts::ivec2 sz = ts::parsevec2(to_str(dpar.get(CONSTWSTR("res"))), ts::ivec2(0));
    config.cx = sz.x;
    config.cy = sz.y;

    run_initializer(config);

}
vsb_dshow_camera_c::core_dshow_c::~core_dshow_c()
{
    if (!initializing && Active())
        Stop();
}

void vsb_dshow_camera_c::core_dshow_c::run_initializer(const VideoConfig &config)
{
    struct init_camera : public ts::task_c
    {
        DShow::Device camera;
        ts::iweak_ptr<core_c> camcore;
        VideoConfig config;
        bool stop_job = false;

        init_camera(const VideoConfig &config, core_c *camcore) :camcore(camcore), config(config), camera(DShow::InitGraph::True) {}

        /*virtual*/ int iterate(int pass) override
        {
            if (stop_job)
                return R_CANCEL;

            //UNFINISHED("remove sleep");
            //Sleep(2000);
            //return R_RESULT;

            config.useDefaultConfig = config.cx == 0 || config.cy == 0;
            config.format = VideoFormat::XRGB;

            camera.SetVideoConfig(&config);

            if (VideoFormat::XRGB != config.format)
            {
                config.useDefaultConfig = false;
                config.format = VideoFormat::XRGB;
                camera.SetVideoConfig(&config);
            }


            if (camera.Valid())
            {
                if (camera.ConnectFilters())
                    camera.Start();
            }

            if (!camera.Active())
            {
                ts::renew(camera, DShow::InitGraph::True);
                ts::master().sys_sleep(1000);
                return R_RESULT;
            }

            return R_DONE;
        }

        /*virtual*/ void result() override
        {
            if (camcore.expired())
                stop_job = true;
            else
                camcore->setbusy();
        }

        /*virtual*/ void done(bool canceled) override
        {
            if (!camcore.expired())
            {
                camcore->initializing = false;
                if (!canceled && camera.Valid() && camera.Active())
                {
                    core_dshow_c *dshowcam = ts::ptr_cast<core_dshow_c *>( camcore.get() );
                    DShow::Device *ll = ts::ptr_cast<DShow::Device *>(dshowcam);
                    *ll = std::move(camera);
                    camcore->initialized(ts::ivec2(config.cx, config.cy));
                }
            }

            if (camera.Active())
                camera.Stop();

            __super::done(canceled);
        }

    };

    g_app->add_task(TSNEW(init_camera, config, this));
}


void vsb_dshow_camera_c::core_dshow_c::dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t datasize, long long startTime, long long stopTime)
{
    spinlock::auto_simple_lock l(sync);
    ts::tmpalloc_c talloc; // ugly, but this thread was created by system, not me :(

    ++frametag;
    bool some_processed = false;
    do
    {
        some_processed = false;
        for (vsb_dshow_camera_c * c : owners)
        {
            if (c->frametag == frametag) continue;
            c->frametag = frametag;
            locked = c;
            owners_changed = false;
            l.unlock();

            ts::bmpcore_exbody_s eb(data + config.cx * 4 * (config.cy - 1), ts::imgdesc_s(ts::ivec2(config.cx, config.cy), 32, ts::int16(-config.cx * 4)));
            c->dshocb(eb);

            l.lock(sync);
            locked = nullptr;
            some_processed = true;
            if (owners_changed)
                break;
        }
    } while (some_processed);

}

void vsb_dshow_camera_c::dshocb(const ts::bmpcore_exbody_s &eb)
{
    ts::ivec2 dsz;
    if (ts::drawable_bitmap_c *b = lockbuf(&dsz))
    {
        if (dsz == ts::ivec2(0) || dsz == eb.info().sz)
        {
            // just copy
            if (b->info().sz != eb.info().sz)
                b->create(eb.info().sz);

            b->copy(ts::ivec2(0), eb.info().sz, eb, ts::ivec2(0));
        } else
        {
            if (b->info().sz != dsz)
                b->create(dsz);

            b->resize_from( eb, ts::FILTER_BOX_LANCZOS3 );
        }
        call_bmp_ready_handler(b->extbody());
        unlock(b);
    }
}

vsb_dshow_camera_c::~vsb_dshow_camera_c()
{
    if (core)
        core->remove_owner( this );
    ASSERT(core == nullptr);

    stop_lockers();
}

bool vsb_dshow_camera_c::init(const vsb_descriptor_s &desc_, const ts::wstrmap_c &dpar)
{
    initializing = core_c::get(this, desc_, dpar);
    return true;
}

video_frame_decoder_c::video_frame_decoder_c(active_protocol_c *ap, incoming_video_frame_s *f_) :f(f_), ap(ap)
{
    videosize = f->sz;

    display = g_app->current_video_display.get();
    if (nullptr == display->notice || display->gid != f->gid || display->cid != f->cid)
    {
        ap->unlock_video_frame(f);
        f = nullptr;
    }

    g_app->add_task( this );
}

video_frame_decoder_c::~video_frame_decoder_c()
{
    if (f)
        ap->unlock_video_frame(f);
    if (display)
    {
        if (g_app)
            g_app->current_video_display.release(display);
        else
            display->release();
    }

};

/*virtual*/ int video_frame_decoder_c::iterate(int pass)
{
    if (nullptr == f || nullptr == display->notice) return R_CANCEL;
    if ( display->get_desired_size() == ts::ivec2(0) ) return R_CANCEL;


    ts::ivec2 lsz;
    if (ts::drawable_bitmap_c *b = display->lockbuf(&lsz))
    {
        if (lsz != b->info().sz)
            b->create(lsz);

        b->resize_from(ts::bmpcore_exbody_s(f->data(), ts::imgdesc_s(f->sz, 32)), ts::FILTER_BOX_LANCZOS3);

        display->unlock(b);
    }

    return R_DONE;
}
/*virtual*/ void video_frame_decoder_c::done(bool canceled)
{
    gmsg<ISOGM_VIDEO_TICK>(videosize).send();
    __super::done(canceled);
}
/*virtual*/ void video_frame_decoder_c::result()
{
}

void vsb_display_c::release()
{
    --ref;
    if (ref <= 0)
        TSDEL(this);
}

void vsb_display_c::update_video_size(const ts::ivec2 &videosize, const ts::ivec2 &viewportsize)
{
    set_video_size(videosize);
    fit_to_size(viewportsize);
}
