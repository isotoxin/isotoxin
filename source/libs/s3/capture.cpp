#include "stdafx.h"
#include "s3.h"

namespace s3
{

static spinlock::long3264 sync = 0;

MY_GUID(MY_DSDEVID_DefaultVoiceCapture, 0xdef00003, 0x9c6d, 0x47ed, 0xaa, 0xf1, 0x4d, 0xda, 0x8f, 0x2b, 0x5c, 0x03);

static LPDIRECTSOUNDCAPTURE8 pDSCapture = nullptr;
static LPDIRECTSOUNDCAPTUREBUFFER pDSCaptureBuffer = nullptr;
static DEVICE captureguid = DEFAULT_DEVICE;
static WAVEFORMATEX fmt;
static int next_offset = 0;

void capture_tick( capture_callback * data_callback, void *context )
{
    spinlock::auto_simple_lock l(sync);

    if (pDSCaptureBuffer)
    {
        DWORD readp;
        pDSCaptureBuffer->GetCurrentPosition(nullptr,&readp);

        int lock_size = (int)readp - next_offset;
        if (lock_size < 0)
            lock_size += (int)fmt.nAvgBytesPerSec;

        if (lock_size > 0)
        {
            void*   pbCaptureData = nullptr;
            DWORD   dwCaptureLength = 0;
            void*   pbCaptureData2 = nullptr;
            DWORD   dwCaptureLength2 = 0;

            // Lock the capture buffer down
            if (FAILED(pDSCaptureBuffer->Lock(next_offset, lock_size,
                &pbCaptureData, &dwCaptureLength,
                &pbCaptureData2, &dwCaptureLength2, 0L)))
                return;

            data_callback(pbCaptureData, dwCaptureLength, context);
            if (pbCaptureData2)
                data_callback(pbCaptureData2, dwCaptureLength2, context);

            pDSCaptureBuffer->Unlock(pbCaptureData, dwCaptureLength, pbCaptureData2, dwCaptureLength2);
            next_offset = readp;
        }
    }
}

void format_by_index(int index)
{
    int iSampleRate = index >> 2;
    int iType = index & 3;

    switch (iSampleRate)
    {
        case 0: fmt.nSamplesPerSec = 44100; break;
        case 1: fmt.nSamplesPerSec = 22050; break;
        case 2: fmt.nSamplesPerSec = 11025; break;
        case 3: fmt.nSamplesPerSec = 8000; break;
    }

    switch (iType)
    {
        case 0: fmt.wBitsPerSample = 16; fmt.nChannels = 2; break;
        case 1: fmt.wBitsPerSample = 8; fmt.nChannels = 2; break;
        case 2: fmt.wBitsPerSample = 16; fmt.nChannels = 1; break;
        case 3: fmt.wBitsPerSample = 8; fmt.nChannels = 1; break;
    }

    fmt.nBlockAlign = fmt.nChannels * (fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nBlockAlign * fmt.nSamplesPerSec;

}

void stop_capture()
{
    spinlock::auto_simple_lock l(sync);

    if (pDSCaptureBuffer)
    {
        LPDIRECTSOUNDCAPTUREBUFFER b = pDSCaptureBuffer;
        pDSCaptureBuffer = nullptr;

        DWORD status = 0;
        b->GetStatus(&status);
        if (0 != (status & DSCBSTATUS_CAPTURING)) b->Stop();
        b->Release();
    }
    if (pDSCapture) { pDSCapture->Release(); pDSCapture = nullptr; }
}

bool is_capturing()
{
    spinlock::auto_simple_lock l(sync, false);
    if (!l.is_locked()) return true;

    if (pDSCaptureBuffer)
    {
        DWORD status = 0;
        pDSCaptureBuffer->GetStatus(&status);
        if (0 != (status & DSCBSTATUS_CAPTURING)) return true;
    }
    return false;
}

bool get_capture_format(Format & cfmt)
{
    if (!is_capturing()) return false;

    cfmt.channels = fmt.nChannels;
    cfmt.sampleRate = fmt.nSamplesPerSec;
    cfmt.bitsPerSample = fmt.wBitsPerSample;

    return true;
}

bool start_capture(Format & cfmt, const Format * tryformats, int try_formats_cnt)
{
    stop_capture();

    spinlock::auto_simple_lock l(sync);

    DEVICE device = (captureguid == DEFAULT_DEVICE) ? (DEVICE &)MY_DSDEVID_DefaultVoiceCapture : captureguid;

    if (FAILED(DirectSoundCaptureCreate8(&device, &pDSCapture, nullptr)))
        return false;

    DSCBUFFERDESC dscbd;

    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;

    memset(&dscbd, 0, sizeof(dscbd));
    dscbd.dwSize = sizeof(dscbd);

    // Try 16 different standard format to see if they are supported
    for (int i = 0, cnt = 16 + try_formats_cnt; i < cnt; ++i)
    {
        if (i < try_formats_cnt)
        {
            const Format &tf = tryformats[i];
            fmt.nChannels = tf.channels;
            fmt.nSamplesPerSec = tf.sampleRate;
            fmt.wBitsPerSample = tf.bitsPerSample;
            fmt.nAvgBytesPerSec = tf.avgBytesPerSec();
            fmt.nBlockAlign = (WORD)tf.sampleSize();
        } else
        {
            format_by_index(i - try_formats_cnt);
        }

        dscbd.dwBufferBytes = fmt.nAvgBytesPerSec;
        dscbd.lpwfxFormat = &fmt;

        if (FAILED(pDSCapture->CreateCaptureBuffer(&dscbd, &pDSCaptureBuffer, nullptr)))
            continue;
        break;
    }
    if (pDSCaptureBuffer)
    {
        next_offset = 0;
        pDSCaptureBuffer->Start(DSCBSTART_LOOPING);

        cfmt.channels = fmt.nChannels;
        cfmt.sampleRate = fmt.nSamplesPerSec;
        cfmt.bitsPerSample = fmt.wBitsPerSample;
    }

    return pDSCaptureBuffer != nullptr;
}

void enum_sound_capture_devices(device_enum_callback *lpDSEnumCallback, LPVOID lpContext)
{
    typedef BOOL __stdcall device_enum_callback_win32( GUID *lpGuid, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext );

    DirectSoundCaptureEnumerateW( (device_enum_callback_win32 *)lpDSEnumCallback, lpContext);
}

void set_capture_device(const DEVICE *device)
{
    captureguid = *device;
}
void get_capture_device(DEVICE *device)
{
    *device = captureguid;
}


}