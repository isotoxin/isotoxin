#include "isotoxin.h"

//-V:cvt:807

mediasystem_c::~mediasystem_c()
{
    deinit();

    while( ref > 0 )
    {
        s3::Update();
        ts::sys_sleep(1);
    }
}

void mediasystem_c::init()
{
    init( cfg().device_talk(), cfg().device_signal() );
    deinit_time = ts::Time::current() + 60000;
}

void mediasystem_c::init(const ts::str_c &talkdevice, const ts::str_c &signaldevice)
{
    SIMPLELOCK( rawplock );

    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).get_player())
            vp(i).~voice_player();

    for( UNIQUE_PTR(loop_play) &ptr : loops ) ptr.reset();

    talks.Shutdown();
    talks.params.device = device_from_string(talkdevice);
    if (!talks.Initialize() && !talkdevice.is_empty())
    {
        talks.params.device = s3::DEFAULT_DEVICE;
        talks.Initialize();
    }

    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        TSPLACENEW( &vp(i), &talks );

    if (signaldevice.is_empty())
    {
        if (notifs)
        {
            notifs->Shutdown();
            TSDEL(notifs);
            notifs = nullptr;
        }
    }
    else
    {
        if (notifs)
            notifs->Shutdown();
        else
            notifs = TSNEW( s3::Player );

        notifs->params.device = device_from_string(signaldevice);

        if (!notifs->Initialize())
        {
            TSDEL(notifs);
            notifs = nullptr;
        }

    }
    initialized = true;
}

void mediasystem_c::deinit()
{
    if (!initialized) return;

    SIMPLELOCK(rawplock);

    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).get_player())
            vp(i).~voice_player();
    memset(rawps, 0, sizeof(rawps));

    for (UNIQUE_PTR(loop_play) &ptr : loops) 
        ptr.reset();

    talks.Shutdown();
    if (notifs)
    {
        notifs->Shutdown();
        TSDEL(notifs);
        notifs = nullptr;
    }
    initialized = false;
}

void mediasystem_c::may_be_deinit()
{
    if (!initialized) return;

    s3::Update();

    if (ref > 0)
        return;

    {
        SIMPLELOCK(rawplock);

        for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
            if (vp(i).get_player())
                if (vp(i).isPlaying())
                    return;

        for (UNIQUE_PTR(loop_play) &ptr : loops)
            if (ptr && ptr->isPlaying())
                return;
    }

    if (ts::Time::current() > deinit_time)
        deinit();
}

void mediasystem_c::test_talk(float vol)
{
    if (ts::blob_c buf = ts::g_fileop->load(CONSTWSTR("sounds/voicetest.ogg")))
        play(buf, vol, false);
}
void mediasystem_c::test_signal(float vol)
{
    if (ts::blob_c buf = ts::g_fileop->load(CONSTWSTR("sounds/ringtone.ogg")))
        play(buf, vol);
}

void mediasystem_c::play(const ts::blob_c &buf, float volume, bool signal_device)
{
    if (!initialized)
        init();

    deinit_time = ts::Time::current() + 60000;

    struct once_play : s3::MSource
    {
        ts::blob_c buf;
        mediasystem_c *owner;
        once_play(mediasystem_c *owner, s3::Player *player, const ts::blob_c &buf, float volume_) : MSource(player, s3::SG_UI), buf(buf), owner(owner)
        {
            volume = volume_;
            pitch = 1.0f;
            init(buf.data(), (int)buf.size());
            play();
            autoDelete();
            owner->addref();
        }
        ~once_play()
        {
            owner->decref();
        }
        /*virtual*/ void die() override { TSDEL(this); }
    };

    s3::Player *player = signal_device && notifs ? notifs : &talks;
    TSNEW(once_play, this, player, buf, volume); // not memory leak. autodelete

}

static ts::blob_c load_snd( sound_e snd, float &vol )
{
    ts::wstr_c fn;
    switch (snd)
    {
#define SND(s) case snd_##s: fn = cfg().snd_##s(); vol = cfg().snd_vol_##s(); break;
        SOUNDS
#undef SND
    }

    if (!fn.is_empty())
        return ts::g_fileop->load(ts::fn_join(ts::pwstr_c(CONSTWSTR("sounds")),fn));

    return ts::blob_c();
}

bool mediasystem_c::stop_looped(sound_e snd)
{
    if (loops[snd]) 
    {
        bool ispl = loops[snd]->isPlaying();
        loops[snd]->stop(0.5f);
        return ispl;
    }
    return false;
}

void mediasystem_c::play_looped(sound_e snd, float volume, bool signal_device)
{
    if (!initialized)
        init();

    deinit_time = ts::Time::current() + 60000;

    s3::Player *player = signal_device && notifs ? notifs : &talks;
    if (loops[snd] && loops[snd]->get_player() != player)
    {
        loops[snd]->stop();
        loops[snd].reset();
    }
    if (loops[snd])
    {
        if (!loops[snd]->isPlaying()) loops[snd]->play(true);
        return;
    }

    float vol;
    if (ts::blob_c buf = load_snd(snd, vol))
        loops[snd].reset( TSNEW( loop_play, player, buf, volume * vol ) );
}

void mediasystem_c::voice_player::add_data(const s3::Format &fmt, float vol, int dsp /* see fmt_converter_s::FO_* bits */, const void *d, ts::aint dsz, int clampms)
{
    auto w = data.lock_write();

    if (fmt != format)
    {
        if (isPlaying()) stop();
        w().clear();
        format = fmt;
    } else
    {
        ts::aint clampbytes = format.avgBytesPerMSecs( clampms );
        ts::aint a = w().available();
        if ( a > clampbytes )
            w().remove_data(a-clampms);
    }

    bool filter = false;
    if (vol > 1.0f || dsp)
    {
        // activate cvt
        if (!w().cvt)
            w().cvt.reset(TSNEW(fmt_converter_s));
        w().cvt->filter_options.setup(dsp);

        if (vol > 1.0f) // gain volume will be processed by converter
        {
            volume = 1.0f;
            w().cvt->volume = vol;
        } else
        {
            volume = vol;
            w().cvt->volume = 1.0f;
        }
        filter = true;

#pragma warning(push)
#pragma warning(disable:4822) 
        struct s //-V690
        {
            protected_data_s &pd;
            s(protected_data_s &pd):pd(pd) {}
            void operator=(const s&) UNUSED;
            void adddata(const s3::Format& fmt, const void *d, ts::aint s)
            {
                if (s) pd.add_data(d, s);
            }

        } ss(w());
#pragma warning(pop)

        w().cvt->ofmt = fmt;
        w().cvt->acceptor = DELEGATE(&ss, adddata);
        w().cvt->cvt(fmt, d, dsz);


    } else 
    {
        volume = vol;
        w().cvt.reset();
        w().add_data(d, dsz);

    }
    w.unlock();

    if (!isPlaying())
        play();
}

void mediasystem_c::voice_player::protected_data_s::remove_data( ts::aint size )
{
    if ( size < ( buf[ readbuf ].size() - readpos ) )
    {
        readpos += (int)size;
        if ( newdata == readbuf ) newdata ^= 1;
    } else
    {
        ts::aint szleft = buf[ readbuf ].size() - readpos;
        ts::aint exist_data_size = szleft + buf[ readbuf ^ 1 ].size();
        if ( exist_data_size < size && begining )
        {
            if ( exist_data_size )
                remove_data( exist_data_size );
            return;
        }
        begining = false;
        size -= szleft;
        buf[ readbuf ].clear(); // this buffer fully extracted
        newdata = readbuf; // now it will be used for new data
        readbuf ^= 1; readpos = 0; // and read-buffer is second one
        if ( size )
        {
            if ( buf[ readbuf ].size() ) // there are some data in buffer - get it
            {
                remove_data( size );
                return;
            }
            newdata = readbuf = 0; // both buffers are empty - do job as from begining
            begining = true; // as begining - new data after silence
        }
    }
}

ts::aint mediasystem_c::voice_player::protected_data_s::read_data(const s3::Format &fmt, char *dest, ts::aint size)
{
    if (size < (buf[readbuf].size() - readpos))
    {
        memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += (int)size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        ts::aint szleft = buf[readbuf].size() - readpos;
        ts::aint exist_data_size = szleft + buf[readbuf ^ 1].size();
        if (exist_data_size < size && begining)
        {
            // silence first, then data, if exist
            ts::aint silence_size = size - exist_data_size;
            memset(dest, fmt.bitsPerSample == 8 ? 0x80 : 0, silence_size); // fill silence before data
            if (exist_data_size)
                return silence_size + read_data(fmt, dest + silence_size, exist_data_size);
            return silence_size;
        }
        begining = false;
        memcpy(dest, buf[readbuf].data() + readpos, szleft);
        size -= szleft;
        buf[readbuf].clear(); // this buffer fully extracted
        newdata = readbuf; // now it will be used for new data
        readbuf ^= 1; readpos = 0; // and read-buffer is second one
        if (size)
        {
            if (buf[readbuf].size()) // there are some data in buffer - get it
                return szleft + read_data(fmt, dest + szleft, size);
            newdata = readbuf = 0; // both buffers are empty - do job as from begining
            memset(dest + szleft, fmt.bitsPerSample == 8 ? 0x80 : 0, size); // fill silence to end of request
            begining = true; // as begining - new data after silence
        }
        return szleft + size;
    }
}

/*virtual*/ s3::s3int mediasystem_c::voice_player::rawRead(char *dest, s3::s3int size)
{
    auto w = data.lock_write();

    if (!mute && w().available() == 0)
    {
        w().nodata += (int)size;
        if (w().nodata > format.avgBytesPerSec())
            return -1;
    } else
        w().nodata = 0;

    return w().read_data(format, dest, size);
}

void mediasystem_c::voice_player::shutdown()
{
    stop();
    current = 0;
    mute = false;
    data.lock_write()().clear();
}

void mediasystem_c::voice_mute(const uint64 &key, bool mute)
{
    SIMPLELOCK(rawplock);
    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).current == key)
        {
            vp(i).mute = mute;
            return;
        }
}

void mediasystem_c::voice_volume( const uint64 &key, float vol )
{
    SIMPLELOCK( rawplock );
    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).current == key)
        {
            vp(i).volume = vol;
            return;
        }
}


bool mediasystem_c::play_voice( const uint64 &key, const s3::Format &fmt, const void *data, ts::aint size, float vol, int dsp, int clampms )
{
    if (!initialized)
        init();

    deinit_time = ts::Time::current() + 60000;

    SIMPLELOCK( rawplock );

    int j = -1, k = -1;
    int i = 0;
    for (; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).current == key)
        {
            if (vp(i).mute)
                return true;
        namana:
            vp(i).add_data(fmt, vol, dsp, data, size, clampms );
            return true;
        } else 
        {
            if ( j < 0 && vp(i).current == 0 )
            j = i;
            if (k < 0 && j < 0 && !vp(i).isPlaying())
                k = i;
        }

    if (j>=0)
    {
        i = j;
        vp(j).current = key;
        vp(j).mute = false;
        goto namana;
    }
    if (k >= 0)
    {
        i = k;
        vp(k).current = key;
        vp(k).mute = false;
        goto namana;
    }
    return false;
}

void mediasystem_c::free_voice_channel( const uint64 &key )
{
    SIMPLELOCK(rawplock);
    for (int i = 0; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).current == key)
        {
            vp(i).shutdown();
            break;
        }
}



void play_sound( sound_e sss, bool looped, bool forced )
{
    if ( cfg().sounds_flags() )
        return;

    if ( !forced )
    {
        if ( contacts().get_self().get_ostate() == COS_AWAY && prf().get_options().is( SNDOPT_MUTE_ON_AWAY ) )
            return;
        if ( contacts().get_self().get_ostate() == COS_DND && prf().get_options().is( SNDOPT_MUTE_ON_DND ) )
            return;
    }

    if (looped)
    {
        g_app->mediasystem().play_looped(sss, 1.0);
        return;
    }

    float vol;
    if (ts::blob_c buf = load_snd(sss, vol)) 
        g_app->mediasystem().play(buf, vol);
}



bool stop_sound( sound_e snd )
{
    return g_app->mediasystem().stop_looped(snd);
}

ts::str_c string_from_device(const s3::DEVICE& device)
{
    ts::str_c s;
    s.append_as_hex(&device, sizeof(s3::DEVICE));
    return s;
}






fmt_converter_s::fmt_converter_s()
{
}

fmt_converter_s::~fmt_converter_s()
{
    clear();
}

void fmt_converter_s::clear()
{
#if _RESAMPLER == RESAMPLER_SPEEXFA
    for ( SpeexResamplerState *s : resampler )
        if ( s ) speex_resampler_destroy( s );
#elif _RESAMPLER == RESAMPLER_SRC
    for ( SRC_STATE *s : resampler )
        if ( s ) src_delete( s );
#endif

    for ( Filter_Audio *f : filter )
        if (f) kill_filter_audio(f);

    memset( resampler, 0, sizeof( resampler ) );
    memset( filter, 0, sizeof( filter ) );

    active_filter_options = 0;
    tail.clear();
}

void fmt_converter_s::cvt( const s3::Format &ifmt, const void *idata, ts::aint isize )
{
    if (!ASSERT(acceptor)) return;

    bool volume_changed = volume == 1.0f; // -V550

    if (ifmt == ofmt && volume_changed && !filter_options.__bits && !active_filter_options && tail.size() == 0)
    {
        acceptor( ofmt, idata, isize );
        return;
    }

    //int maxssz = ifmt.avgBytesPerMSecs(100);
    //while (isize > maxssz)
    //{
    //    cvt_portion(ifmt, idata, maxssz);
    //    idata = ((char *)idata) + maxssz;
    //    isize -= maxssz;
    //}

    ASSERT( isize >= 0 );

    if (isize)
        cvt_portion(ifmt, idata, isize);

}

void fmt_converter_s::cvt_portion(const s3::Format &ifmt, const void *idata, ts::aint isize)
{
    bool volume_changed = volume == 1.0f; // -V550

    //int bszmax = 65536;
    //ts::uint8 b_temp[65536*2];
    //ts::uint8 *b[2];
    //b[0] = b_temp;
    //b[1] = b_temp + bszmax;
    //int bsz[2] = {};

    ts::tmp_buf_c b[2];

    int curtarget = 0;
    const void *idata_orig = idata;

//#define CHECK_INSIDE(p, sz) ASSERT( (ts::uint8 *)(p) >= b[curtarget] && ((ts::uint8 *)(p) + sz) <= b[curtarget] + bszmax )
//#define SETSZ(sz) bsz[curtarget] = sz; if (!CHECK( int(sz) <= bszmax )) return;
//#define GETB() b[curtarget]
//#define GETSZ() bsz[curtarget]

#define CHECK_INSIDE(p, sz) ASSERT( b[curtarget].inside(p,sz) )
#define SETSZ(sz) b[curtarget].set_size(sz,false)
#define GETB() b[curtarget].data()
#define GETSZ() b[curtarget].size()

    if (ifmt.bitsPerSample == 8)
    {
        // 8 -> 16
        SETSZ(isize * 2);

        ts::int16 *cvt = (ts::int16 *)GETB();
        for (int i = 0; i < isize; ++i, ++cvt)
        {
            ts::uint8 sample8 = ((ts::uint8 *)idata)[i];
            if (CHECK_INSIDE(cvt, 1))
                *cvt = ((ts::int16)sample8 - (ts::int16)0x80) << 8;
        }
        idata = GETB();
        isize = GETSZ();
        curtarget ^= 1;
    }

    int ichnls = ifmt.channels;
    if (ifmt.channels != ofmt.channels && ichnls > 1) // convert to 1 channel before resample (in case input chnls and output chnls are not same)
    {
        ts::aint blocks = isize / (ichnls * 2);
        SETSZ(blocks * 2);

        ts::int16 *cvt = (ts::int16 *)GETB();
        for (int i = 0; i < blocks; ++i)
        {
            ts::int16 *sample16 = ((ts::int16 *)idata) + (i * ichnls);
            int sum = 0;
            for (int c = 0; c < ichnls; ++c, ++sample16)
                sum += *sample16;
            sum /= ichnls;

            if (CHECK_INSIDE(cvt, 2))
                *cvt = (ts::int16)sum;
            ++cvt;
        }
        idata = GETB();
        isize = GETSZ();
        curtarget ^= 1;

        ichnls = 1;
    }

    if (ifmt.sampleRate != ofmt.sampleRate)
    {
        if (resampler[0] == nullptr)
        {
            int error;

#if _RESAMPLER == RESAMPLER_SPEEXFA
            resampler[0] = speex_resampler_init(1, ifmt.sampleRate, ofmt.sampleRate, 10, &error);
            if (ichnls > 1) resampler[1] = speex_resampler_init(1, ifmt.sampleRate, ofmt.sampleRate, 10, &error);

#elif _RESAMPLER == RESAMPLER_SRC
            resampler[0] = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
            if (ichnls > 1) resampler[1] = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
#endif
            if (ichnls > 2) ichnls = 2; // moar zen 2!!! it is impossibru!!!
        } else
        {
            if (ichnls > 1 && !resampler[1])
            {
                // number of input channels increased! Create 2nd resampler
                int error;

#if _RESAMPLER == RESAMPLER_SPEEXFA
                resampler[1] = speex_resampler_init(1, ifmt.sampleRate, ofmt.sampleRate, 10, &error);
            }

            speex_resampler_set_rate(resampler[0], ifmt.sampleRate, ofmt.sampleRate);
            if (resampler[1]) speex_resampler_set_rate(resampler[1], ifmt.sampleRate, ofmt.sampleRate);
#elif _RESAMPLER == RESAMPLER_SRC
                resampler[1] = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
            }
#endif
        }

        ts::aint samples_per_chnl = isize / (ichnls * 2);
        SETSZ(samples_per_chnl * sizeof(float) * ichnls);
        const ts::int16 *source = (const ts::int16 *)idata;
        for (int ch = 0; ch < ichnls; ++ch)
        {
            float *out = ((float *)GETB()) + (samples_per_chnl * ch);

            if (volume_changed)
            {
                for (const ts::int16 *from = source + ch, *end = source + (isize/2); from < end; from += ichnls, ++out)
                    if (CHECK_INSIDE(out, 4))
                        *out = (float)(*from) * (float)(1.0 / 32767.0);
            } else
            {
                for (const ts::int16 *from = source + ch, *end = source + (isize/2); from < end; from += ichnls, ++out)
                    if (CHECK_INSIDE(out, 4))
                    {
                        *out = ts::CLAMP( (float)(*from) * (float)(volume / 32767.0), -1.0f, 1.0f);
                    }
                volume_changed = true;
            }
        }

        // so float stuff ready

        idata = GETB();
        //isize = GETSZ();
        curtarget ^= 1;

        ts::aint outsamples_per_chnl = samples_per_chnl * ofmt.sampleRate / ifmt.sampleRate + 256;
        SETSZ(outsamples_per_chnl * ichnls * sizeof(float));

        uint osamples = 0;

#if _RESAMPLER == RESAMPLER_SPEEXFA

        for (int ch = 0; ch < ichnls; ++ch)
        {
            osamples = (uint)outsamples_per_chnl;

            float *indata = (float *)idata + (samples_per_chnl * ch);
            float *odata = ((float *)GETB()) + outsamples_per_chnl * ch;
            
            uint isamples = (uint)samples_per_chnl;
            speex_resampler_process_float(resampler[ch], 0, indata, &isamples, odata, &osamples);

            ASSERT(isamples == (uint)samples_per_chnl);
        }

#elif _RESAMPLER == RESAMPLER_SRC
        SRC_DATA	src_data = {};
        src_data.end_of_input = 0;
        src_data.src_ratio = (double)ofmt.sampleRate / (double)ifmt.sampleRate;
        for (int ch = 0; ch < ichnls; ++ch)
        {
            float *indata = (float *)idata + (samples_per_chnl * ch);
            float *odata = ((float *)GETB()) + outsamples_per_chnl * ch;

            src_data.input_frames = samples_per_chnl;
            src_data.data_in = indata;

            src_data.data_out = odata;
            src_data.output_frames = outsamples_per_chnl;

            src_process(resampler[ch], &src_data);

            ASSERT(src_data.input_frames_used == samples_per_chnl);
            CHECK_INSIDE(odata, src_data.output_frames_gen * sizeof(float));
        }
        osamples = src_data.output_frames_gen;
#endif

        idata = GETB();
        //isize = GETSZ();
        curtarget ^= 1;

        SETSZ(osamples * 2 * ichnls);

        // back to int16
        for (int ch = 0; ch < ichnls; ++ch)
        {
            const float *indata = (const float *)idata + (outsamples_per_chnl * ch); // outsamples_per_chnl used here - its right
            ts::int16 *odata = ((ts::int16 *)GETB()) + ch;
            for (const float *indata_e = indata + osamples; indata < indata_e; ++indata, odata += ichnls)
                if (CHECK_INSIDE(odata, 2))
                    *odata = (ts::int16) ((*indata) * 32767.0f);
        }

        idata = GETB();
        isize = GETSZ();
        curtarget ^= 1;
    }

    if (!volume_changed)
    {
        ts::aint samples = isize / 2;
        ts::int16 *odata = (ts::int16 *)idata;
        
        if (idata_orig == idata)
        {
            // idata is const, we cannot modify data

            SETSZ(isize);
            odata = (ts::int16 *)GETB();
        }

        for (int i = 0; i < samples; ++i, ++odata)
        {
            ts::int16 sample16 = ((ts::int16 *)idata)[i];
            float rslt = ts::CLAMP( (float)(sample16) * (float)(volume / 32767.0), -1.0f, 1.0f );
            *odata = (ts::int16) (rslt * 32767.0f);
        }
        volume_changed = true;

        if (idata_orig == idata)
        {
            idata = GETB();
            curtarget ^= 1;
        }
    }

    if (active_filter_options != filter_options.__bits)
    {
        if (filter[0] && !filter_options.__bits)
        {
            kill_filter_audio(filter[0]);
            filter[0] = nullptr;
            if (filter[1]) { kill_filter_audio(filter[1]); filter[1] = nullptr; }

        } else
        {
            if (!filter[0] && filter_options.__bits)
            {
                filter[0] = new_filter_audio(ofmt.sampleRate /* sample rate already converted */);
                if (ichnls > 1 && !filter[1]) filter[1] = new_filter_audio(ofmt.sampleRate /* sample rate already converted */);
            }

            for(Filter_Audio *f : filter)
                if (f) enable_disable_filters(f, 0, filter_options.is(FO_NOISE_REDUCTION) ? 1 : 0, filter_options.is(FO_GAINER) ? 1 : 0, 0);
        }

        active_filter_options = filter_options.__bits;
    }

    if (filter[0])
    {
        ts::aint samples_per_chnl;
        if (tail.size())
        {
            ts::aint oldisize = isize;
            ts::aint full_isize = isize + tail.size();
            samples_per_chnl = full_isize / (ichnls * 2);

            int filter_quant = ofmt.sampleRate / 100;
            ts::aint samples_per_chnl_1 = (samples_per_chnl / filter_quant) * filter_quant;
            if (samples_per_chnl_1 < samples_per_chnl)
            {
                if (samples_per_chnl_1 == 0)
                {
                    tail.append_buf(idata, isize);
                    acceptor(ofmt, nullptr, 0);
                    return;
                }

                isize = samples_per_chnl_1 * (ichnls * 2);
                ASSERT(isize < full_isize);
            } else
            {
                isize = full_isize;
            }
            samples_per_chnl = samples_per_chnl_1;

            SETSZ(isize);

            ASSERT(isize >= tail.size());
            CHECK_INSIDE(GETB(), tail.size());
            CHECK_INSIDE(GETB() + tail.size(), isize - tail.size() );

            memcpy( GETB(), tail.data(), tail.size() );
            memcpy( GETB() + tail.size(), idata, isize - tail.size() );

            tail.clear();
            if ( ts::aint tail_size = full_isize - isize)
                tail.append_buf( (const char *)idata+(oldisize-tail_size), tail_size );

            idata = GETB();
            curtarget ^= 1;

        } else
        {
            samples_per_chnl = isize / (ichnls * 2);
            int filter_quant = ofmt.sampleRate / 100;
            ts::aint samples_per_chnl_1 = (samples_per_chnl / filter_quant) * filter_quant;
            if (samples_per_chnl_1 < samples_per_chnl)
            {
                ts::aint oldisize = isize;
                isize = samples_per_chnl_1 * (ichnls * 2);
                tail.append_buf( (char *)idata + isize, oldisize - isize ); // store tail for next iteration
                samples_per_chnl = samples_per_chnl_1;
                if (samples_per_chnl == 0)
                {
                    acceptor( ofmt, nullptr, 0 );
                    return;
                }
            }
        }

        if (idata_orig == idata || ichnls > 1)
        {
            // idata is const, we cannot modify data
            // or we should split interleaved stereo stream to separate streams

            SETSZ(isize);
            
            const ts::int16 *source = (const ts::int16 *)idata;
            for (int ch = 0; ch < ichnls; ++ch)
            {
                ts::int16 *target = ((ts::int16 *)GETB()) + (samples_per_chnl * ch);
                for (const ts::int16 *from = source + ch, *end = source + (isize/2); from < end; from += ichnls, ++target)
                    if (CHECK_INSIDE(target, 2))
                        *target = *from;
            }

            idata = GETB();
            curtarget ^= 1;
        }

        for (int ch = 0; ch < ichnls; ++ch)
        {
            ts::int16 *indata = (ts::int16 *)idata + (samples_per_chnl * ch);
            filter_audio(filter[ch], indata, (int)samples_per_chnl);
        }

        if (ichnls > 1)
        {
            // mix two streams into one stereo stream

            SETSZ(isize);

            for (int ch = 0; ch < ichnls; ++ch)
            {
                ts::int16 *indata = (ts::int16 *)idata + (samples_per_chnl * ch);
                ts::int16 *odata = ((ts::int16 *)GETB()) + ch;

                for (const ts::int16 *indata_e = indata + samples_per_chnl; indata < indata_e; ++indata, odata += ichnls)
                    if (CHECK_INSIDE(odata, 2))
                        *odata = *indata;
            }

            idata = GETB();
            isize = GETSZ();
            curtarget ^= 1;
        }
    } else if (tail.size())
    {
        isize += tail.size();
        SETSZ(isize);

        memcpy(GETB(), tail.data(), tail.size());
        memcpy(GETB() + tail.size(), idata, isize - tail.size());
        tail.clear();

        idata = GETB();
        curtarget ^= 1;
    }

    if (ichnls != ofmt.channels && ASSERT(ichnls == 1))
    {
        ts::aint samples = isize / 2;
        SETSZ(samples * 2 * ofmt.channels);
        ts::int16 *cvt = (ts::int16 *)GETB();
        for (int i = 0; i < samples; ++i)
        {
            ts::int16 sample16 = ((ts::int16 *)idata)[i];
            for (int c = 0; c < ofmt.channels; ++c, ++cvt)
                if (CHECK_INSIDE(cvt, 2))
                    *cvt = sample16;
        }
        idata = GETB();
        isize = GETSZ();
        curtarget ^= 1;
    }

    if (ofmt.bitsPerSample == 8)
    {
        // o_O plugin requirement. So strange plugin :)
        // 16 -> 8
        ts::aint samples = isize / 2;
        SETSZ(samples);
        ts::uint8 *cvt = GETB();
        for (int i = 0; i < samples; ++i, ++cvt)
        {
            ts::int16 sample16 = ((ts::int16 *)idata)[i];
            if (CHECK_INSIDE(cvt, 1))
                *cvt = (ts::uint8) ((sample16 >> 8) + (ts::int16)0x80);
        }
        idata = GETB();
        isize = GETSZ();
    }

    acceptor( ofmt, idata, isize );

}


#if _RESAMPLER == RESAMPLER_SILK
#elif _RESAMPLER == RESAMPLER_SRC
#endif


avmuxer_c::avmuxer_c( uint64 key ) : key( key )
{
}

avmuxer_c::~avmuxer_c()
{
    SIMPLELOCK( sync );
}

void avmuxer_c::add_audio( uint64 timestamp, const s3::Format &fmt, const void *data, ts::aint size, active_protocol_c *ctx )
{
    audio = true;
    if ( !video )
    {
        //ctx->play_audio( key, fmt, data, size );
        return;
    }

    SIMPLELOCK( sync );

}