#pragma once

struct av_contact_s
{
    DECLARE_EYELET( av_contact_s );

    GM_RECEIVER( av_contact_s, ISOGM_CHANGED_SETTINGS );
    GM_RECEIVER( av_contact_s, ISOGM_PEER_STREAM_OPTIONS );

public:

    time_t starttime;
    uint64 mstime = 1; // monotonic milliseconds from beginning of call
    ts::shared_ptr<contact_root_c> c;
    contact_key_s avkey;
    enum state_e
    {
        AV_NONE,
        AV_RINGING,
        AV_INPROGRESS,
    };

    fmt_converter_s cvt;

    int ticktag = -1;
    float volume = 1.0f;
    int dsp_flags = 0;

    int so = SO_SENDING_AUDIO | SO_RECEIVING_AUDIO | SO_RECEIVING_VIDEO; // default stream options
    int remote_so = 0;
    ts::ivec2 sosz = ts::ivec2( 0 );
    ts::ivec2 remote_sosz = ts::ivec2( 0 );
    ts::ivec2 prev_video_size = ts::ivec2( 0 );
    vsb_descriptor_s currentvsb;
    ts::wstrmap_c cpar;
    UNIQUE_PTR( vsb_c ) vsb;
    active_protocol_c *ap = nullptr;
    ts::Time prevtick = ts::Time::undefined();

    unsigned state : 4;
    unsigned inactive : 1; // true - selected other av contact
    unsigned dirty_cam_size : 1;

    av_contact_s( const contact_key_s &avk, state_e st );
    ~av_contact_s();

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
    void mic_switch();
    void speaker_switch();
    void set_video_res( const ts::ivec2 &vsz );
    void send_so();
    int cur_so() const { return inactive ? ( so & ~( SO_SENDING_AUDIO | SO_RECEIVING_AUDIO | SO_RECEIVING_VIDEO ) ) : so; }

    vsb_c *createcam();


    void add_audio( uint64 timestamp, const s3::Format &fmt, const void *data, ts::aint size );
    void play_audio( const s3::Format &fmt, const void *data, ts::aint size );
    void send_audio( const s3::Format& ifmt, const void *data, ts::aint size, bool clear_cvt_buffer );
};


class av_contacts_c
{
    spinlock::long3264 sync = 0;
    ts::array_del_t< av_contact_s, 0 > m_contacts;

public:

    int get_avinprogresscount() const;
    int get_avringcount() const;
    av_contact_s & get( const contact_key_s &avk, av_contact_s::state_e st );
    av_contact_s * find_inprogress( const contact_key_s &avk );
    void del( contact_root_c *c ); // del by root
    template<typename R> void iterate( const R &r ) const { for ( const av_contact_s *avc : m_contacts ) r( *avc ); }
    template<typename R> void iterate( const R &r ) { for ( av_contact_s *avc : m_contacts ) r( *avc ); }
    void stop_all_av();
    void clear();

    bool camera_tick();
};

