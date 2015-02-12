#pragma once

#define MAX_RAW_PLAYERS 16

#define SOUNDS \
    SND( incoming_message ) \
    SND( incoming_file ) \
    SND( ringtone ) \
    SND( call_cancel ) \
    SND( hangon ) \
    SND( calltone ) \


enum sound_e
{
#define SND(s) snd_##s,
    SOUNDS
#undef SND
    snd_count
};

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
            init(buf.data(), buf.size());
            play(true);
        }
        /*virtual*/ void die() override { TSDEL(this); }
    };

    struct voice_player : s3::RawSource
    {
        struct protected_data_s
        {
            ts::buf_c buf[2];
            int readbuf = 0;
            int newdata = 0;
            int readpos = 0;
            bool begining = true;
            void add_data(const void *d, int s)
            {
                buf[newdata].append_buf(d, s);
            }
            int read_data(s3::Format fmt, char *dest, int size);
        };
        spinlock::syncvar<protected_data_s> data;

        voice_player( s3::Player *player ): s3::RawSource(player, s3::SG_VOICE)
        {
            format.channels = 1;
            format.sampleRate = 48000;
            format.bitsPerSample = 16;
        }

        void set_fmt( const s3::Format &fmt )
        {
            if (fmt != format)
            {
                if (isPlaying()) stop();
                format = fmt;
            }
        }

        void add_data(const void *dest, int size);
        /*virtual*/ int rawRead(char *dest, int size) override;

        uint64 current = 0;
    };

    long rawplock = 0;
    char rawps[ MAX_RAW_PLAYERS * sizeof(voice_player) ];
    UNIQUE_PTR( loop_play ) loops[ snd_count ];

    voice_player &vp(int i) { return *(voice_player *)(rawps + i * sizeof(voice_player)); }
public:

    void init(const ts::str_c &talkdevice, const ts::str_c &signaldevice);
    void init(); // loads params from cfg

    mediasystem_c()
    {
        memset( rawps, 0, sizeof(rawps) );
    }
    ~mediasystem_c()
    {
        for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
            if (vp(i).get_player())
                vp(i).~voice_player();

        talks.Shutdown();
        if (notifs)
        {
            notifs->Shutdown();
            TSDEL( notifs );
        }
    }

    void test_talk();
    void test_signal();

    void play(const ts::blob_c &buf, float volume = 1.0f, bool signal_device = true);
    void play_looped(sound_e snd, float volume = 1.0f, bool signal_device = true);
    bool stop_looped(sound_e snd);

    bool play_voice( const uint64 &key, const s3::Format &fmt, const void *data, int size ); 
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
    s3::DEVICE device(s3::DEFAULT_DEVICE);

    if (s.l / 2 == sizeof(device))
        for (int i = 0; i < sizeof(device); ++i)
            ((ts::uint8 *)&device)[i] = ts::pstr_c(s).as_byte_hex(i * 2);

    return device;
}

ts::str_c string_from_device(const s3::DEVICE& device);

typedef fastdelegate::FastDelegate<void (const void *, int)> accept_sound_func;

struct fmt_converter_s
{
    SRC_STATE *resampler[2]; // stereo?
    fmt_converter_s();
    ~fmt_converter_s();

    void cvt( const s3::Format &ifmt, const void *idata, int isize, const s3::Format &ofmt, accept_sound_func acceptor );
};