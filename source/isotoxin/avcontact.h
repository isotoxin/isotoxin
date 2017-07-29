#pragma once

struct av_contact_s
{
    DECLARE_EYELET( av_contact_s );

    GM_RECEIVER( av_contact_s, ISOGM_CHANGED_SETTINGS );

public:

    enum state_e
    {
        AV_NONE,
        AV_RINGING,
        AV_INPROGRESS,
    };

    struct ccore_s : public ts::shared_object // self settings, common core per all av contacts in conference 
    {
        time_t starttime;
        uint64 mstime = 1; // monotonic milliseconds from beginning of call
        ts::shared_ptr<contact_root_c> c;
        active_protocol_c *ap = nullptr;
        ts::Time prevtick = ts::Time::undefined();
        int ticktag = 0;

        fmt_converter_s cvt;

        vsb_descriptor_s currentvsb;
        ts::wstrmap_c cpar;
        UNIQUE_PTR( vsb_c ) vsb;

        int mic_dsp_flags = 0;
        int so = SO_SENDING_AUDIO | SO_RECEIVING_AUDIO | SO_RECEIVING_VIDEO; // default stream options
        ts::ivec2 sosz = ts::ivec2( 0 );
        ts::ivec2 prev_video_size = ts::ivec2( 0 );

        unsigned state : 4;
        unsigned inactive : 1; // true - selected other av contact
        unsigned dirty_cam_size : 1;
        unsigned mute : 1;

        int cur_so() const { return inactive ? ( so & ~( SO_SENDING_AUDIO | SO_RECEIVING_AUDIO | SO_RECEIVING_VIDEO ) ) : so; }
        ccore_s( contact_root_c *c, state_e st ):c(c), state(st), mute(0) {}

    };
    ts::shared_ptr<ccore_s> core;

    ts::shared_ptr<contact_c> sub;
    uint64 avkey = 0;

    int tag;
    float volume = 1.0f;
    int speaker_dsp_flags = 0;
    int remote_so = 0;
    ts::ivec2 remote_sosz = ts::ivec2( 0 );

    av_contact_s( contact_root_c *c, contact_c *sub, state_e st );
    ~av_contact_s();

    bool with_apid(int apid) const
    {
        return sub.get() && sub.get()->getkey().protoid == (unsigned)apid;
    }

    void call_tick();

    void on_frame_ready( const ts::bmpcore_exbody_s &ebm );
    void camera_tick();

    bool is_mic_off() const { return 0 == ( cur_so() & SO_SENDING_AUDIO ); }
    bool is_mic_on() const { return 0 != ( cur_so() & SO_SENDING_AUDIO ); }

    bool is_speaker_off() const { return 0 == ( cur_so() & SO_RECEIVING_AUDIO ); }
    bool is_speaker_on() const { return 0 != ( cur_so() & SO_RECEIVING_AUDIO ); }

    bool is_video_show() const { return is_receive_video() && 0 != ( remote_so & SO_SENDING_VIDEO ); }
    bool is_receive_video() const { return 0 != ( cur_so() & SO_RECEIVING_VIDEO ); }
    bool is_camera_on() const { return 0 != ( cur_so() & SO_SENDING_VIDEO ); }

    bool b_mic_switch( RID, GUIPARAM p );
    bool b_speaker_switch( RID, GUIPARAM p );

    void update_btn_face_camera( gui_button_c &btn );

    void update_speaker();

    void set_recv_video( bool allow_recv );
    void set_inactive( bool inactive );
    void set_so_audio( bool inactive, bool enable_mic, bool enable_speaker );
    void camera( bool on );

    void camera_switch();
    void mic_off();
    void mic_switch(bool enable);
    void speaker_switch(bool enable);
    void set_video_res( const ts::ivec2 &vsz );
    void send_so();
    int cur_so() const { return core->cur_so(); }

    vsb_c *createcam();


    void add_audio( uint64 timestamp, const s3::Format &fmt, const void *data, ts::aint size );
    void play_audio( const s3::Format &fmt, const void *data, ts::aint size );
    void send_audio( const s3::Format& ifmt, const void *data, ts::aint size, bool clear_cvt_buffer );
};


class av_contacts_c
{
    GM_RECEIVER( av_contacts_c, ISOGM_PEER_STREAM_OPTIONS );

    volatile mutable spinlock::long3264 sync = 0;
    ts::array_del_t< av_contact_s, 0 > m_contacts;

    struct so_s
    {
        ts::ivec2 videosize;
        int so;
    };

    ts::hashmap_t< uint64, so_s > m_preso;

    struct ind_s
    {
        uint64 avk;
        int cooldown;
        int indicator;
    };
    spinlock::syncvar< ts::tbuf0_t< ind_s > > m_indicators;
    bool clean_started = false;

    bool clean_indicators(RID, GUIPARAM);

    void remove_by_apid(int apid);

public:

    ~av_contacts_c();

    void set_indicator( uint64 avk, int iv );
    int get_indicator( uint64 avk );


    int get_avinprogresscount() const;
    int get_avringcount() const;
    av_contact_s & get( uint64 avkey, av_contact_s::state_e st );
    av_contact_s * find_inprogress( uint64 avk );
    av_contact_s * find_inprogress_any( contact_root_c *c );
    bool is_any_inprogress(contact_root_c *cr);
    bool is_any_inprogress();
    void set_tag( contact_root_c *cr, int tag );
    void del( contact_root_c *c ); // del by root
    void del( int tag ); // del by tag
    void del( active_protocol_c *ap ); // del by ap
    template<typename R> void iterate( const R &r ) const 
    {
        SIMPLELOCK(sync);
        for ( const av_contact_s *avc : m_contacts ) r( *avc );
    }
    template<typename R> void iterate( const R &r )
    {
        SIMPLELOCK(sync);
        for ( av_contact_s *avc : m_contacts ) r( *avc );
    }
    void stop_all_av(int apid);
    void clear();

    bool tick();
};

