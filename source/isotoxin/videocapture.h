#pragma once

#include "dshowcap/dshowcapture.hpp"

struct vcd_descriptor_s
{
    ts::wstr_c id;
    ts::wstr_c desc;
};

typedef ts::array_inplace_t< vcd_descriptor_s, 0 > vcd_list_t;

void enum_video_capture_devices( vcd_list_t &list, bool add_desktop );

class vcd_c // video capture device
{
    long sync = 0;
    ts::drawable_bitmap_c bufs[3]; // tripple buffer
    int last_updated = -1;
    int updating = -1;
    int reading = -1;
    int last_read = -1;

    ts::ivec2 desired_size = ts::ivec2(0);
    ts::ivec2 video_size = ts::ivec2(0);

    bool stoping = false;
protected:
    bool initializing = false;
    bool busy = false;


    void set_video_size( const ts::ivec2 &videosize ) { video_size = videosize; }
    void stop_lockers()
    {
        for (;;Sleep(1))
        {
            spinlock::auto_simple_lock l(sync);
            stoping = true;
            if (updating >= 0) continue;
            if (reading >= 0) continue;
            break;
        }
    }
public:
    virtual ~vcd_c() {}
    static vcd_c *build(const vcd_descriptor_s &desc);

    bool still_initializing() const {return initializing;}
    bool is_busy() const {return busy;}
    bool updated()
    {
        spinlock::auto_simple_lock l(sync);
        return last_updated >= 0;
    }

    const ts::ivec2 &get_video_size() const {return video_size;}
    const ts::ivec2 &get_desired_size() const {return desired_size;} // call only from base thread
    void set_desired_size( const ts::ivec2 &sz )
    {
        spinlock::auto_simple_lock l(sync);
        desired_size = sz;
    }
    ts::ivec2 fit_to_size( const ts::ivec2 &sz );

    ts::drawable_bitmap_c * lockbuf( ts::ivec2 *desired_size_ )
    {
        spinlock::auto_simple_lock l(sync);
        if (stoping) return nullptr;
        if (desired_size_)
        {
#ifdef _DEBUG
            if (updating >= 0)
                __debugbreak();
#endif // _DEBUG
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
                __debugbreak();
#endif // _DEBUG
            updating = last_read;
            last_read = -1;
            *desired_size_  = desired_size;
            return bufs + updating;

        } else
        {
#ifdef _DEBUG
            if (reading >= 0)
                __debugbreak();
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
        spinlock::auto_simple_lock l(sync);
        for (int i = 0; i < 3; ++i)
        {
            if (b == bufs+i)
            {
                if (updating == i)
                    updating = -1, last_updated = i;
                else if (reading == i)
                    reading = -1, last_read = i;
                else
                    __debugbreak();
                return;
            }
        }
        __debugbreak();
    }

};

class vcd_dshow_camera_c : public vcd_c
{
    class core_c : public DShow::Device, public ts::shared_object
    {
        DECLARE_EYELET( core_c );

        long sync = 0;
        ts::wstr_c id;
        static core_c *first;
        static core_c *last;
        core_c *prev = nullptr;
        core_c *next = nullptr;
        ts::pointers_t<vcd_dshow_camera_c, 0> owners;
        bool initializing = false;
        bool busy = false;

    public:


        core_c(const vcd_descriptor_s &desc);
        ~core_c();

        void dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t size, long long startTime, long long stopTime);
        void run_initializer(const DShow::VideoConfig &config);
        bool add_owner( vcd_dshow_camera_c *owner );
        void remove_owner( vcd_dshow_camera_c *owner );
        void initialized( const ts::ivec2 &videosize );
        void setbusy();
        
        static bool get( vcd_dshow_camera_c *owner, const vcd_descriptor_s &desc );
    };

    ts::shared_ptr<core_c> core;

    void dshocb(const DShow::VideoConfig &config, unsigned char *data, size_t size, long long startTime, long long stopTime);
    void initialized( const ts::ivec2 &videosize )
    {
        set_video_size(videosize);
        initializing = false;
        busy = false;
    }

public:
    vcd_dshow_camera_c() {}
    ~vcd_dshow_camera_c();

    bool init( const vcd_descriptor_s &desc );
    // /*virtual*/ bool set_config( video_config_s& cfg ) override;

};

class vcd_desktop_c : public vcd_c
{
    struct grab_desktop : public ts::task_c
    {
        vcd_desktop_c* cam;
        long sync = 0;
        int next_time;
        bool stop_job = false;

        grab_desktop(vcd_desktop_c *cam) :cam(cam) { next_time = timeGetTime() + cam->gra; }

        /*virtual*/ int iterate(int pass) override;
        void fin()
        {
            spinlock::auto_simple_lock sl(sync);
            stop_job = true;
            cam = nullptr;
        }

    } *grabber = nullptr;

    UNIQUE_PTR( ts::drawable_bitmap_c ) tmpbmp;
    ts::buf_c cursorcachedata;
    ts::irect rect = ts::irect(0);
    int monitor = -1;
    int gra = 1000/15;

    int grab();

public:
    vcd_desktop_c() {}
    ~vcd_desktop_c() { if (grabber) grabber->fin(); }

    bool init(const vcd_descriptor_s &desc);
    // /*virtual*/ bool set_config( video_config_s& cfg ) override;

};
