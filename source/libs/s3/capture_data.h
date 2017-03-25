#pragma once

#ifdef CORE_WIN32
#ifdef _M_X64
#define CAPTURE_COREDATA_SIZE (sizeof(void *)*3)
#else
#define CAPTURE_COREDATA_SIZE (sizeof(void *)*3)
#endif
#elif defined CORE_NIX
#define CAPTURE_COREDATA_SIZE 4
#endif

class capturer_c
{
public:
    spinlock::long3264 sync = 0;
    DEVICE capturedevice = DEFAULT_DEVICE;
    fmt::WAVEFORMATEX format;
    char coredata[CAPTURE_COREDATA_SIZE];

    void format_by_index(int index)
    {
        int iSampleRate = index >> 2;
        int iType = index & 3;

        switch (iSampleRate)
        {
        case 0: format.nSamplesPerSec = 44100; break;
        case 1: format.nSamplesPerSec = 22050; break;
        case 2: format.nSamplesPerSec = 11025; break;
        case 3: format.nSamplesPerSec = 8000; break;
        }

        switch (iType)
        {
        case 0: format.wBitsPerSample = 16; format.nChannels = 2; break;
        case 1: format.wBitsPerSample = 8; format.nChannels = 2; break;
        case 2: format.wBitsPerSample = 16; format.nChannels = 1; break;
        case 3: format.wBitsPerSample = 8; format.nChannels = 1; break;
        }

        format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;

    }


    // system depend implementation
    void capture_tick(capture_callback * data_callback, void *context);
    void stop();
    bool is_capturing() const;
    bool start_capture(Format & cfmt, const Format * tryformats, int try_formats_cnt);
    void enum_sound_capture_devices(device_enum_callback *cb, void * lpContext);
};

