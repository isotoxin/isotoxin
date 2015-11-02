#include "isotoxin.h"

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


void enum_video_capture_devices(vcd_list_t &list, bool add_desktop)
{
#ifdef DSHOW_LOGGING
    DShow::SetLogCallback(lcb, nullptr);
#endif

    std::vector<VideoDevice> devices;
    Device::EnumVideoDevices( devices );

    for(const VideoDevice &vd : devices)
    {
        vcd_descriptor_s &d = list.add();
        d.desc.set( vd.name.c_str(), vd.name.length() );
        d.id.set( vd.path.c_str(), vd.path.length() );
    }

    if (add_desktop)
    {
        int mc = ts::monitor_count();
        if (1 == mc)
        {
            vcd_descriptor_s &d = list.add();
            d.desc = TTT("Desktop", 350);
            d.id = CONSTWSTR("desktop");
        }
        else
        {
            for (int m = 0; m < mc; ++m)
            {
                vcd_descriptor_s &d = list.add();
                d.desc = TTT("Desktop $", 351) / ts::monitor_get_description(m);
                d.id = CONSTWSTR("desktop/"); d.id.append_as_uint(m);
            }
        }
    }
}

vcd_c *vcd_c::build( const vcd_descriptor_s &desc )
{
    if (!desc.id.begins(CONSTWSTR("desktop")))
    {
        vcd_dshow_camera_c *cam = TSNEW(vcd_dshow_camera_c);
        if (cam->init(desc))
            return cam;

        TSDEL(cam);

    }

    vcd_desktop_c *cam = TSNEW( vcd_desktop_c );
    if (cam->init(desc))
        return cam;
        
    TSDEL(cam);
    return nullptr;
}

ts::ivec2 vcd_c::fit_to_size( const ts::ivec2 &sz_ )
{
    if(initializing) return sz_;
    ts::ivec2 sz = sz_;
    int x_by_y = video_size.x * sz.y / video_size.y;
    if (x_by_y <= sz.x)
    {
        sz.x = x_by_y;
    }
    else
    {
        int y_by_x = video_size.y * sz.x / video_size.x;
        ASSERT(y_by_x <= sz.y);
        sz.y = y_by_x;
    }
    set_desired_size(sz);
    return sz;
}

/*virtual*/ int vcd_desktop_c::grab_desktop::iterate(int pass)
{
    spinlock::simple_lock(sync);
    if (stop_job)
    {
        spinlock::simple_unlock(sync);
        return R_CANCEL;
    }

    int curt = timeGetTime();
    if ((curt - next_time) < 0)
    {
        spinlock::simple_unlock(sync);
        Sleep(1);
        return stop_job ? R_CANCEL : 1;
    }

    next_time += cam->grab();
    
    if ( (curt-next_time) > 1000 )
        next_time = curt; // tooooo slooooooow grabbing :(

    spinlock::simple_unlock(sync);
    return stop_job ? R_CANCEL : 1;
}

int vcd_desktop_c::grab()
{
    ts::ivec2 dsz;
    if (ts::drawable_bitmap_c *b = lockbuf(&dsz))
    {
        if (dsz == ts::ivec2(0) || dsz == rect.size())
        {
            // just copy
            dsz.x = rect.width(), dsz.y = rect.height();
            if (b->info().sz != dsz)
                b->create(dsz, monitor);

            HWND hwnd = GetDesktopWindow();
            HDC desktop_dc = GetDC(hwnd);
            BitBlt(b->DC(), 0, 0, dsz.x, dsz.y, desktop_dc, rect.lt.x, rect.lt.y, SRCCOPY | CAPTUREBLT);
            ReleaseDC(hwnd, desktop_dc);

            b->render_cursor( rect.lt, cursorcachedata );
        }
        else
        {
            if (b->info().sz != dsz)
                b->create(dsz);

            if (!tmpbmp)
            {
                tmpbmp.reset(TSNEW(ts::drawable_bitmap_c));
                tmpbmp->create(rect.size(), monitor);
            }
            else if (tmpbmp->info().sz != rect.size())
                tmpbmp->create(rect.size());

            HWND hwnd = GetDesktopWindow();
            HDC desktop_dc = GetDC(hwnd);
            BitBlt(tmpbmp->DC(), 0, 0, tmpbmp->info().sz.x, tmpbmp->info().sz.y, desktop_dc, rect.lt.x, rect.lt.y, SRCCOPY | CAPTUREBLT);
            ReleaseDC(hwnd, desktop_dc);

            tmpbmp->render_cursor( rect.lt, cursorcachedata );

            b->resize_from(tmpbmp->extbody(), ts::FILTER_LANCZOS3);
        }
        unlock(b);
    }

    return gra;
}

bool vcd_desktop_c::init(const vcd_descriptor_s &desc)
{
    monitor = 0;
    ts::token<ts::wchar> t( desc.id, '/' ); ++t;
    if (t) monitor = t->as_int();

    rect = ts::monitor_get_max_size_fs(monitor);
    set_video_size(rect.size());

    grabber = TSNEW( grab_desktop, this );
    g_app->add_task( grabber );

    return true;
}


//////////////// dshow camera

vcd_dshow_camera_c::core_c *vcd_dshow_camera_c::core_c::first = nullptr;
vcd_dshow_camera_c::core_c *vcd_dshow_camera_c::core_c::last = nullptr;

vcd_dshow_camera_c::core_c::core_c(const vcd_descriptor_s &desc_):Device(DShow::InitGraph::True), id(desc_.id)
{
    LIST_ADD( this, first, last, prev, next );
    VideoConfig config;

    config.name = desc_.desc;
    config.path = desc_.id;
    config.callback = DELEGATE(this, dshocb);

    initializing = true;
    run_initializer(config);

}

vcd_dshow_camera_c::core_c::~core_c()
{
    LIST_DEL( this, first, last, prev, next );
    if (!initializing && Active())
        Stop();
}

void vcd_dshow_camera_c::core_c::run_initializer(const VideoConfig &config)
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

            camera.SetVideoConfig(&config);

            config.useDefaultConfig = false;
            config.format = VideoFormat::XRGB;

            camera.SetVideoConfig(&config);

            if (camera.Valid())
            {
                if (camera.ConnectFilters())
                    camera.Start();
            }

            if (!camera.Active())
            {
                ts::renew(camera, DShow::InitGraph::True);
                Sleep(1000);
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
                    *((DShow::Device *)camcore) = std::move(camera);
                    camcore->initialized(ts::ivec2( config.cx, config.cy ));
                }
            }

            if (camera.Active())
                camera.Stop();

            __super::done(canceled);
        }

    };

    g_app->add_task(TSNEW(init_camera, config, this));
}


bool vcd_dshow_camera_c::core_c::get(vcd_dshow_camera_c *owner, const vcd_descriptor_s &desc)
{
    for( core_c *f = first; f; f = f->next )
        if (f->id.equals(desc.id))
            return f->add_owner( owner );

    core_c *newcore = TSNEW( core_c, desc );
    return newcore->add_owner(owner);
}

bool vcd_dshow_camera_c::core_c::add_owner( vcd_dshow_camera_c *owner )
{
    spinlock::auto_simple_lock l(sync);

    for( vcd_dshow_camera_c *ptr : owners )
        if (ptr == owner)
        {
            ASSERT( owner->core == this );
            return initializing;
        }

    owners.add( owner );
    owner->core = this;
    if (owners.size() >= 2)
        owner->set_video_size( owners.get(0)->get_video_size() );
    return initializing;
}

void vcd_dshow_camera_c::core_c::remove_owner( vcd_dshow_camera_c *owner )
{
    spinlock::auto_simple_lock l(sync);

    int cnt = owners.size();
    for(int i=0;i<cnt;++i)
    {
        if ( owners.get(i) == owner )
        {
            ts::shared_ptr<core_c> refcore = this; // up ref
            owners.remove_fast(i);
            owner->core = nullptr;
            l.unlock();
            return;
        }
    }
}

void vcd_dshow_camera_c::core_c::dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t datasize, long long startTime, long long stopTime)
{
    spinlock::auto_simple_lock l(sync);

    ts::tmpalloc_c talloc; // ugly, but this thread was created by system, not me :(

    for (vcd_dshow_camera_c * c : owners)
        c->dshocb(config,data,datasize,startTime,stopTime);
}

void vcd_dshow_camera_c::core_c::initialized( const ts::ivec2 &videosize )
{
    spinlock::auto_simple_lock l(sync);
    busy = false;
    for (vcd_dshow_camera_c * c : owners)
        c->initialized(videosize);
}

void vcd_dshow_camera_c::core_c::setbusy()
{
    spinlock::auto_simple_lock l(sync);
    busy = false;
    for (vcd_dshow_camera_c * c : owners)
        c->busy = true;
}


void vcd_dshow_camera_c::dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t datasize, long long startTime, long long stopTime)
{
    ts::ivec2 dsz;
    if (ts::drawable_bitmap_c *b = lockbuf(&dsz))
    {
        if (dsz == ts::ivec2(0) || dsz == ts::ivec2(config.cx, config.cy))
        {
            // just copy
            dsz.x = config.cx, dsz.y = config.cy;
            if (b->info().sz != dsz)
                b->create(dsz);

            b->copy(ts::ivec2(0), dsz, ts::bmpcore_exbody_s(data, ts::imgdesc_s(dsz, 32, ts::uint16(dsz.x * 4))), ts::ivec2(0));
        } else
        {
            if (b->info().sz != dsz)
                b->create(dsz);

            b->resize_from( ts::bmpcore_exbody_s(data, ts::imgdesc_s(ts::ivec2(config.cx, config.cy), 32, ts::uint16(config.cx * 4))), ts::FILTER_LANCZOS3 );
        }
        b->flip_y();
        unlock(b);
    }
}

vcd_dshow_camera_c::~vcd_dshow_camera_c()
{
    if (core)
        core->remove_owner( this );
    ASSERT(core == nullptr);

    stop_lockers();
}


bool vcd_dshow_camera_c::init(const vcd_descriptor_s &desc_)
{
    initializing = core_c::get(this, desc_);
    return true;
}



