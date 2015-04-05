#include "isotoxin.h"

void mediasystem_c::init()
{
    init( cfg().device_talk(), cfg().device_signal() );
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

}

void mediasystem_c::test_talk()
{
    ts::blob_c buf = ts::g_fileop->load(CONSTWSTR("sounds/voicetest.ogg"));
    if (buf.size()) play(buf, 1.0, false);
}
void mediasystem_c::test_signal()
{
    //play_looped(snd_ringtone);
    ts::blob_c buf = ts::g_fileop->load(CONSTWSTR("sounds/ringtone.ogg"));
    if (buf.size()) play(buf);
}

void mediasystem_c::play(const ts::blob_c &buf, float volume, bool signal_device)
{
    struct once_play : s3::MSource
    {
        ts::blob_c buf;
        once_play(s3::Player *player, const ts::blob_c &buf, float volume_) : MSource(player, s3::SG_UI), buf(buf)
        {
            volume = volume_;
            pitch = 1.0f;
            init(buf.data(), buf.size());
            play();
            autoDelete();
        }
        /*virtual*/ void die() override { TSDEL(this); }
    };

    s3::Player *player = signal_device && notifs ? notifs : &talks;
    TSNEW(once_play, player, buf, volume); // not memory leak. autodelete

}

static ts::blob_c load_snd( sound_e snd )
{
    ts::wstr_c fn;
    switch (snd)
    {
#define SND(s) case snd_##s: fn = prf().snd_##s(); break;
        SOUNDS
#undef SND
    }

    if (!fn.is_empty())
        return ts::g_fileop->load(fn);

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

    if (ts::blob_c buf = load_snd(snd))
        loops[snd].reset( TSNEW( loop_play, player, buf, volume ) );
}

void mediasystem_c::voice_player::add_data(const void *d, int s)
{
    data.lock_write()().add_data(d,s);
    if (!isPlaying()) play();
}

int mediasystem_c::voice_player::protected_data_s::read_data(s3::Format fmt, char *dest, int size)
{
    if (size < (buf[readbuf].size() - readpos))
    {
        memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        int szleft = buf[readbuf].size() - readpos;
        int exist_data_size = szleft + buf[readbuf ^ 1].size();
        if (exist_data_size < size && begining)
        {
            // вначале тулим тишину, а потом данные, если есть
            int silence_size = size - exist_data_size;
            memset(dest, fmt.bitsPerSample == 8 ? 0x80 : 0, silence_size); // fill silence before data
            if (exist_data_size)
                return silence_size + read_data(fmt, dest + silence_size, exist_data_size);
            return silence_size;
        }
        begining = false;
        memcpy(dest, buf[readbuf].data() + readpos, szleft);
        size -= szleft;
        buf[readbuf].clear(); // этот буфер полностью выгребли
        newdata = readbuf; // теперь в него буду поступать новые данные
        readbuf ^= 1; readpos = 0; // а читать мы будем уже из другого буфера
        if (size)
        {
            if (buf[readbuf].size()) // если в буфере что-то есть, то выгребаем
                return szleft + read_data(fmt, dest + szleft, size);
            newdata = readbuf = 0; // оба буфера пустые, но еще треба данные. работаем с нуля
            memset(dest + szleft, fmt.bitsPerSample == 8 ? 0x80 : 0, size); // заполняем тишиной до конца запроса
            begining = true; // как бы сначала - новые данные пойдут после тишины
        }
        return szleft + size;
    }
}

/*virtual*/ int mediasystem_c::voice_player::rawRead(char *dest, int size)
{
    return data.lock_write()().read_data(format, dest, size);
}

bool mediasystem_c::play_voice( const uint64 &key, const s3::Format &fmt, const void *data, int size )
{
    SIMPLELOCK( rawplock );

    int j = -1;
    int i = 0;
    for (; i < MAX_RAW_PLAYERS; ++i)
        if (vp(i).current == key)
        {
        namana:
            vp(i).set_fmt(fmt);
            vp(i).add_data(data, size);
            return true;
        } else if ( j < 0 && vp(i).current == 0 )
            j = i;
    if (j>=0)
    {
        i = j;
        vp(j).current = key;
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
            vp(i).stop();
            vp(i).current = 0;
            break;
        }
}



void play_sound( sound_e sss, bool looped )
{
    if (looped)
    {
        g_app->mediasystem().play_looped(sss, 1.0);
        return;
    }

    if (ts::blob_c buf = load_snd(sss)) 
        g_app->mediasystem().play(buf, 1.0);
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
    memset(resampler, 0, sizeof(resampler));
}
fmt_converter_s::~fmt_converter_s()
{
    for (SRC_STATE *s : resampler)
        if (s) src_delete(s);
}

void fmt_converter_s::cvt( const s3::Format &ifmt, const void *idata, int isize, const s3::Format &ofmt, accept_sound_func acceptor )
{
    if (ifmt == ofmt)
    {
        acceptor( idata, isize );
        return;
    }

    ts::tmp_buf_c b[2];
    int curtarget = 0;

    if (ifmt.bitsPerSample == 8)
    {
        // 8 -> 16
        b[curtarget].set_size(isize * 2, false);
        ts::int16 *cvt = (ts::int16 *)b[curtarget].data16();
        for (int i = 0; i < isize; ++i, ++cvt)
        {
            ts::uint8 sample8 = ((ts::uint8 *)idata)[i];
            if (ASSERT(b[curtarget].inside(cvt, 1)))
                *cvt = ((ts::int16)sample8 - (ts::int16)0x80) << 8;
        }
        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;
    }

    int ichnls = ifmt.channels;
    if (ifmt.channels != ofmt.channels && ichnls > 1) // convert to 1 channel before resample (in case input chnls and output chnls are not same)
    {
        int blocks = isize / (ichnls * 2);
        b[curtarget].set_size(blocks * 2, false);
        ts::int16 *cvt = (ts::int16 *)b[curtarget].data16();
        for (int i = 0; i < blocks; ++i)
        {
            ts::int16 *sample16 = ((ts::int16 *)idata) + (i * ichnls);
            int sum = 0;
            for (int i = 0; i < ichnls; ++i, ++sample16)
                sum += *sample16;
            sum /= ichnls;

            if (ASSERT(b[curtarget].inside(cvt, 2)))
                *cvt = (ts::int16)sum;
            ++cvt;
        }
        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;

        ichnls = 1;
    }

    if (ifmt.sampleRate != ofmt.sampleRate)
    {
        if (resampler[0] == nullptr)
        {
            int error;
            resampler[0] = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
            if (ichnls > 1) resampler[1] = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
            if (ichnls > 2) ichnls = 2; // moar zen 2!!! it is impossibru!!!
        }

        int samples_per_chnl = isize / (ichnls * 2);
        b[curtarget].set_size(samples_per_chnl * sizeof(float) * ichnls);
        const ts::int16 *ind = (const ts::int16 *)idata;
        for (int ch = 0; ch < ichnls; ++ch)
        {
            float *out = ((float *)b[curtarget].data()) + (samples_per_chnl * ch);
            for (const ts::int16 *from = ind + ch, *end = ind + samples_per_chnl + ch; from < end; ++from, ++out)
                if (ASSERT(b[curtarget].inside(out, 4)))
                    *out = (float)(*from) * (float)(1.0 / 32768.0);
        }

        // so float stuff ready

        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;

        int outsamples_per_chnl = samples_per_chnl * ofmt.sampleRate / ifmt.sampleRate + 256;
        b[curtarget].set_size(outsamples_per_chnl * ichnls * sizeof(float));

        SRC_DATA	src_data = {0};
        src_data.end_of_input = 0;
        src_data.src_ratio = (double)ofmt.sampleRate / (double)ifmt.sampleRate;

        for (int ch = 0; ch < ichnls; ++ch)
        {
            float *indata = (float *)idata + (samples_per_chnl * ch);
            float *odata = ((float *)b[curtarget].data()) + outsamples_per_chnl * ch;

            src_data.input_frames = samples_per_chnl;
            src_data.data_in = indata;

            src_data.data_out = odata;
            src_data.output_frames = outsamples_per_chnl;

            src_process(resampler[ch], &src_data);

            ASSERT(src_data.input_frames_used == samples_per_chnl);
            ASSERT(b[curtarget].inside(odata, src_data.output_frames_gen * sizeof(float)));
        }

        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;

        b[curtarget].set_size(src_data.output_frames_gen * 2 * ichnls);

        // back to int16
        for (int ch = 0; ch < ichnls; ++ch)
        {
            const float *indata = (const float *)idata + (outsamples_per_chnl * ch); // outsamples_per_chnl used here - its right
            ts::int16 *odata = ((ts::int16 *)b[curtarget].data()) + ch;
            for (const float *indata_e = indata + src_data.output_frames_gen; indata < indata_e; ++indata, odata += ichnls)
                if (ASSERT(b[curtarget].inside(odata, 2)))
                    *odata = (ts::int16) ((*indata) * 32768.0f);
        }

        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;
    }


    if (ichnls != ofmt.channels && ASSERT(ichnls == 1))
    {
        int samples = isize / 2;
        b[curtarget].set_size(samples * 2 * ofmt.channels, false);
        ts::int16 *cvt = (ts::int16 *)b[curtarget].data16();
        for (int i = 0; i < samples; ++i)
        {
            ts::int16 sample16 = ((ts::int16 *)idata)[i];
            for (int i = 0; i < ofmt.channels; ++i, ++cvt)
                if (ASSERT(b[curtarget].inside(cvt, 2)))
                    *cvt = sample16;
        }
        idata = b[curtarget].data();
        isize = b[curtarget].size();
        curtarget ^= 1;
    }

    if (ofmt.bitsPerSample == 8)
    {
        // o_O plugin requirement. So strange plugin :)
        // 16 -> 8
        int samples = isize / 2;
        b[curtarget].set_size(samples, false);
        ts::uint8 *cvt = b[curtarget].data();
        for (int i = 0; i < samples; ++i, ++cvt)
        {
            ts::int16 sample16 = ((ts::int16 *)idata)[i];
            if (ASSERT(b[curtarget].inside(cvt, 1)))
                *cvt = (ts::uint8) ((sample16 >> 8) + (ts::int16)0x80);
        }
        idata = b[curtarget].data();
        isize = b[curtarget].size();
    }

    acceptor( idata, isize );

}

