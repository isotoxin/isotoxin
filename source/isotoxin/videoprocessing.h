#pragma once

#ifdef _WIN32
#include "dshowcap/dshowcapture.hpp"
#endif // _WIN32

struct vsb_descriptor_s
{
    ts::wstr_c id;
    ts::wstr_c desc;
    ts::tbuf0_t<ts::ivec2> resolutions;
    bool is_desktop_area() const
    {
        if ( id.begins(CONSTWSTR("desktop/")) )
        {
            // desktop/0/200/200/800/600
            //         m x0  y0  x1  y1
            return id.count_chars('/') == 5;
        }
        return false;
    }
};

DECLARE_MOVABLE(vsb_descriptor_s, true)

typedef ts::array_inplace_t< vsb_descriptor_s, 0 > vsb_list_t;

void enum_video_capture_devices( vsb_list_t &list, bool add_desktop );

typedef fastdelegate::FastDelegate< void( const ts::bmpcore_exbody_s &ebm ) > bmp_ready_handler_t;

class vsb_c // video streaming buffer
{
    spinlock::long3264 sync = 0;
    ts::drawable_bitmap_c bufs[3]; // tripple buffer
    int last_updated = -1;
    int updating = -1;
    int reading = -1;
    int last_read = -1;

    ts::ivec2 desired_size = ts::ivec2(0);
    ts::ivec2 video_size = ts::ivec2(0);

    bmp_ready_handler_t bmpready;

    bool stoping = false;
protected:
    bool initializing = false;
    bool busy = false;


    void set_video_size( const ts::ivec2 &videosize ) { video_size = videosize; }
    void stop_lockers()
    {
        for (;;ts::sys_sleep(1))
        {
            SIMPLELOCK(sync);
            stoping = true;
            if (updating >= 0) continue;
            if (reading >= 0) continue;
            break;
        }
    }
public:
    vsb_c() {}
    virtual ~vsb_c() {}
    static vsb_c *build(const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar);
    static vsb_c *build(); // default video source

    bool still_initializing() const {return initializing;}
    bool is_busy() const {return busy;}
    bool updated()
    {
        SIMPLELOCK(sync);
        return last_updated >= 0;
    }

    void set_bmp_ready_handler( bmp_ready_handler_t h ) { bmpready = h; }
    void call_bmp_ready_handler( const ts::bmpcore_exbody_s &ebm ) { if (bmpready) bmpready(ebm); }

    const ts::ivec2 &get_video_size() const {return video_size;}
    const ts::ivec2 &get_desired_size() const {return desired_size;} // call only from base thread
    void set_desired_size( const ts::ivec2 &sz )
    {
		ASSERT(sz >> 0);
        SIMPLELOCK(sync);
        desired_size = sz;
    }
    ts::ivec2 fit_to_size( const ts::ivec2 &sz );

    ts::drawable_bitmap_c * lockbuf( ts::ivec2 *desired_size_ )
    {
        SIMPLELOCK(sync);
        if (stoping) return nullptr;
        if (desired_size_)
        {
            if (updating >= 0)
                return nullptr; // already updating (other thread?). Anyway, video stream too big - ignore this frame

            for (int i = 0; i < 3;++i)
            {
                if (i == reading) continue;
                if (i == last_updated) continue;
                if (i == last_read) continue;
                updating = i;
                *desired_size_  = desired_size;
                return bufs + i;
            }

#ifdef _DEBUG
            if (last_read < 0)
                DEBUG_BREAK();
#endif // _DEBUG
            updating = last_read;
            last_read = -1;
            *desired_size_  = desired_size;
            return bufs + updating;

        } else
        {
#ifdef _DEBUG
            if (reading >= 0)
                DEBUG_BREAK();
#endif // _DEBUG

            if (last_updated >= 0)
            {
                reading = last_updated;
                last_updated = -1;
                return bufs + reading;
            } else if (last_read >= 0)
            {
                reading = last_read;
                last_read = -1;
                return bufs + reading;
            }
            return nullptr;
        }
    }
    void unlock(ts::drawable_bitmap_c *b)
    {
        SIMPLELOCK(sync);
        for (int i = 0; i < 3; ++i)
        {
            if (b == bufs+i)
            {
                if (updating == i)
                    updating = -1, last_updated = i;
                else if (reading == i)
                    reading = -1, last_read = i;
                else
                    DEBUG_BREAK();
                return;
            }
        }
        DEBUG_BREAK();
    }

};

#ifdef _WIN32
class vsb_dshow_camera_c : public vsb_c
{
    class core_c : public ts::shared_object
    {
        DECLARE_EYELET( core_c );

    protected:
        spinlock::long3264 sync = 0;
        ts::wstr_c id;
        static core_c *first;
        static core_c *last;
        core_c *prev = nullptr;
        core_c *next = nullptr;
        ts::pointers_t<vsb_dshow_camera_c, 0> owners;
        volatile vsb_dshow_camera_c *locked = nullptr;
        int frametag = 0;
        volatile bool owners_changed = false;

        core_c(const vsb_descriptor_s &desc);

        bool busy = false;
    public:
        bool initializing = false;


        virtual ~core_c();

        bool add_owner( vsb_dshow_camera_c *owner );
        void remove_owner( vsb_dshow_camera_c *owner );
        void initialized( const ts::ivec2 &videosize );
        void setbusy();
        
        static bool get( vsb_dshow_camera_c *owner, const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar);
    };

    class core_dshow_c : public core_c, public DShow::Device
    {
        void run_initializer(const DShow::VideoConfig &config);
    public:
        core_dshow_c(const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar);
        virtual ~core_dshow_c();

        void dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t size, long long startTime, long long stopTime);

    };

    ts::shared_ptr<core_c> core;
    int frametag = 0;

    void dshocb( const ts::bmpcore_exbody_s &eb );
    void initialized( const ts::ivec2 &videosize )
    {
        set_video_size(videosize);
        initializing = false;
        busy = false;
    }

public:
    vsb_dshow_camera_c() {}
    ~vsb_dshow_camera_c();

    bool init( const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar);
    // /*virtual*/ bool set_config( video_config_s& cfg ) override;

};
#define REAL_CAMERA_CLASS vsb_dshow_camera_c
#define DSHOW_CAM_DEFINED
#endif



class vsb_desktop_c : public vsb_c
{
    struct grab_desktop : public ts::task_c
    {
        spinlock::long3264 sync = 0;
        ts::buf_c cursorcachedata;
        ts::drawable_bitmap_c grabbuff;
        ts::irect grabrect;
        int monitor;
        static grab_desktop *first;
        static grab_desktop *last;
        grab_desktop *prev = nullptr;
        grab_desktop *next = nullptr;
        ts::pointers_t<vsb_desktop_c, 0> owners;
        volatile vsb_desktop_c *locked = nullptr;

        int grabtag = 0;
        ts::Time next_time;
        bool stop_job = false;
        volatile bool owners_changed = false;

        grab_desktop(const ts::irect &grabrect, int monitor);
        ~grab_desktop();

        void grab(const ts::irect &gr);
        /*virtual*/ int iterate(ts::task_executor_c *e) override;

        void add_owner(vsb_desktop_c *owner);
        void remove_owner(vsb_desktop_c *owner);

        static void get(vsb_desktop_c *owner, const ts::irect &grabrect, int monitor);
    } *grabber = nullptr;

    ts::irect rect = ts::irect(0);
    ts::ivec2 maxsize = ts::ivec2(maximum<int>::value);
    int monitor = -1;
    int grabtag = -1;

    void grabcb(ts::drawable_bitmap_c &gbmp);

public:
    vsb_desktop_c() {}
    ~vsb_desktop_c() { if (grabber) grabber->remove_owner(this); }

    bool init(const vsb_descriptor_s &desc, const ts::wstrmap_c &dpar);
};

class gui_notice_callinprogress_c;
class vsb_display_c : public vsb_c
{
    friend class vsb_displays_pool_c;

    DECLARE_DYNAMIC_BEGIN(vsb_display_c)
    vsb_display_c() {}
    ~vsb_display_c() {}
    DECLARE_DYNAMIC_END(public)

    contact_key_s avkey;
    gui_notice_callinprogress_c* notice = nullptr; // pure pointer due it will be checked with nullptr in other thread
    int ref = 1;

    void addref() {++ref;}
    void release();

    void update_video_size( const ts::ivec2 &videosize, const ts::ivec2 &viewportsize );
};

class vsb_displays_pool_c
{
    spinlock::long3264 sync = 0;
    ts::pointers_t<vsb_display_c,0> ptrs;
public:

    ~vsb_displays_pool_c()
    {
        reset();
    }
    vsb_display_c *get( const contact_key_s &avkey )
    {
        SIMPLELOCK(sync);
        for ( vsb_display_c *d : ptrs )
        {
            if ( d->avkey == avkey )
            {
                d->addref();
                return d;
            }
        }
        vsb_display_c *d = TSNEW(vsb_display_c);
        d->avkey = avkey;
        d->addref();
        ptrs.add( d );
        return d;
    }

    void release( vsb_display_c * p )
    {
        SIMPLELOCK(sync);

        for ( ts::aint i = 0, c = ptrs.size(); i<c; ++i )
        {
            vsb_display_c *d = ptrs.get(i);
            if ( d == p )
            {
                ASSERT( p->ref > 1 );
                if ( --p->ref == 0 )
                {
                    ptrs.remove_fast( i );
                    p->ref = 1;
                    p->release();
                }
                return;
            }
        }

        p->release();
    }

    void reset()
    {
        SIMPLELOCK(sync);
        for ( vsb_display_c *d : ptrs )
            d->release();
        ptrs.clear();
    }
};



struct incoming_video_frame_s // XRGB always
{
    contact_id_s gid, cid;
    ts::ivec2 sz;
    uint64 msmonotonic;
    uint64 padding;

    ts::uint8 *data() {  return (ts::uint8 *)(this + 1);}
};

static_assert( sizeof( incoming_video_frame_s ) == VIDEO_FRAME_HEADER_SIZE, "size!" );

class active_protocol_c;
class video_frame_decoder_c : public ts::task_c
{
    active_protocol_c *ap;
    ts::ivec2 videosize = ts::ivec2(640,480);
    incoming_video_frame_s *f;
    vsb_display_c *display = nullptr;

    /*virtual*/ int iterate(ts::task_executor_c *e) override;
    /*virtual*/ void done(bool canceled) override;
    /*virtual*/ void result() override;
public:
    video_frame_decoder_c( active_protocol_c *ap, incoming_video_frame_s *f );
    ~video_frame_decoder_c();

};
