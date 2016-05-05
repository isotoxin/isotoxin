#pragma once

#define MAX_RAW_PLAYERS 32

#define SOUNDS \
    SND( incoming_message ) \
    SND( incoming_message2 ) \
    SND( incoming_file ) \
    SND( start_recv_file ) \
    SND( file_received ) \
    SND( friend_online ) \
    SND( friend_offline ) \
    SND( ringtone ) \
    SND( ringtone2 ) \
    SND( call_accept ) \
    SND( call_cancel ) \
    SND( hangup ) \
    SND( calltone ) \
    SND( new_version ) \



enum sound_e
{
#define SND(s) snd_##s,
    SOUNDS
#undef SND
    snd_count
};

struct fmt_converter_s;

class mediasystem_c
{
    s3::Player talks;
    s3::Player *notifs = nullptr;

    struct loop_play : s3::MSource
    {
        ts::blob_c buf;
        loop_play(s3::Player *player, const ts::blob_c &buf, float volume_) : MSource(player, s3::SG_UI), buf(buf)
        {
            volume = volume_;
            pitch = 1.0f;
            init(buf.data(), (int)buf.size());
            play(true);
        }
        /*virtual*/ void die() override { TSDEL(this); }
    };

    struct voice_player : s3::RawSource
    {
        struct protected_data_s
        {
            UNIQUE_PTR(fmt_converter_s) cvt;
            ts::buf_c buf[2];
            int readbuf = 0;
            int newdata = 0;
            int readpos = 0;
            int nodata = 0;
            bool begining = true;
            void clear()
            {
                buf[0].clear();
                buf[1].clear();
                readbuf = 0;
                newdata = 0;
                readpos = 0;
                begining = true;
            }
            void add_data(const void *d, ts::aint s)
            {
                buf[newdata].append_buf(d, s);
            }
            ts::aint read_data(const s3::Format &fmt, char *dest, ts::aint size);
            ts::aint available() const { return buf[readbuf].size() - readpos + buf[readbuf ^ 1].size(); }
        };
        spinlock::syncvar<protected_data_s> data;

        voice_player( s3::Player *player ): s3::RawSource(player, s3::SG_VOICE)
        {
            format.channels = 1;
            format.sampleRate = 48000;
            format.bitsPerSample = 16;
        }

        void add_data(const s3::Format &fmt, float vol, int dsp /* see fmt_converter_s::FO_* bits */, const void *dest, ts::aint size);
        /*virtual*/ s3::s3int rawRead(char *dest, s3::s3int size) override;

        void shutdown();

        uint64 current = 0;
        bool mute = false;
    };

    spinlock::long3264 rawplock = 0;
    char rawps[ MAX_RAW_PLAYERS * sizeof(voice_player) ];
    UNIQUE_PTR( loop_play ) loops[ snd_count ];

    int ref = 0;

    ts::Time deinit_time = ts::Time::current() + 60000;
    bool initialized = false;

    voice_player &vp(int i) { return *(voice_player *)(rawps + i * sizeof(voice_player)); }
    void init(); // loads params from cfg
public:

    mediasystem_c()
    {
        memset( rawps, 0, sizeof(rawps) );
    }
    ~mediasystem_c();

    void addref() { ++ref; }
    void decref() { --ref; }

    void init(const ts::str_c &talkdevice, const ts::str_c &signaldevice);
    void deinit();
    void may_be_deinit();

    void test_talk(float vol);
    void test_signal(float vol);

    void play(const ts::blob_c &buf, float volume = 1.0f, bool signal_device = true);
    void play_looped(sound_e snd, float volume = 1.0f, bool signal_device = true);
    bool stop_looped(sound_e snd);

    template <typename CHK> void voice_mute(CHK chk, bool mute)
    {
        SIMPLELOCK(rawplock);
        for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
            if (vp(i).current && chk(vp(i).current))
            {
                vp(i).mute = mute;
                return;
            }
    }
    void voice_mute(const uint64 &key, bool mute);
    void voice_volume( const uint64 &key, float vol ); 
    bool play_voice( const uint64 &key, const s3::Format &fmt, const void *data, ts::aint size, float vol, int dsp );
    void free_voice_channel( const uint64 &key ); 
};


INLINE ts::asptr soundname( sound_e s )
{
    static ts::asptr sounds[] = {
#define SND(s) CONSTASTR( #s ),
        SOUNDS
#undef SND
    };
    return sounds[s];
}

void play_sound( sound_e snd, bool looped );
bool stop_sound( sound_e snd );

INLINE s3::DEVICE device_from_string( const ts::asptr& s )
{
    s3::DEVICE device = s3::DEFAULT_DEVICE;

    if (s.l == sizeof( s3::DEVICE ) * 2)
        ts::pstr_c(s).hex2buf<sizeof( s3::DEVICE )>((ts::uint8 *)&device);
    return device;
}

ts::str_c string_from_device(const s3::DEVICE& device);

typedef fastdelegate::FastDelegate<void (const s3::Format &fmt, const void *, ts::aint )> accept_sound_func;

struct fmt_converter_s
{
#if _RESAMPLER == RESAMPLER_SPEEXFA
    SpeexResamplerState *resampler[2];
#elif _RESAMPLER == RESAMPLER_SRC
    SRC_STATE *resampler[2];
#endif

    Filter_Audio *filter[2];
    ts::buf_c tail; // used for filter ( it handles only (SampleRate/100)*n samples at once )

    static const ts::flags32_s::BITS FO_NOISE_REDUCTION = 1;
    static const ts::flags32_s::BITS FO_GAINER = 2;

    fmt_converter_s();
    ~fmt_converter_s();

    s3::Format ofmt;
    accept_sound_func acceptor;
    
    float volume = 1.0;
    
private:
    void cvt_portion( const s3::Format &ifmt, const void *idata, ts::aint isize );
    ts::flags32_s::BITS active_filter_options = 0;
public:
    ts::flags32_s filter_options;

    void cvt( const s3::Format &ifmt, const void *idata, ts::aint isize );
};

