#include "stdafx.h"
#include "s3.h"
#include "fmt_wav.h"

namespace s3
{

#include "capture_data.h"

static capturer_c capturer;

void capture_tick( capture_callback * data_callback, void *context )
{
    spinlock::auto_simple_lock l(capturer.sync, false);
    if (!l.is_locked()) return;
    capturer.capture_tick(data_callback,context);
}

void stop_capture()
{
    SIMPLELOCK(capturer.sync);
    capturer.stop();
}

bool is_capturing()
{
    spinlock::auto_simple_lock l(capturer.sync, false);
    if (!l.is_locked()) return true;
    return capturer.is_capturing();
}

bool get_capture_format(Format & cfmt)
{
    if (!is_capturing()) return false;

    cfmt.channels = capturer.format.nChannels;
    cfmt.sampleRate = capturer.format.nSamplesPerSec;
    cfmt.bitsPerSample = capturer.format.wBitsPerSample;

    return true;
}

bool start_capture(Format & cfmt, const Format * tryformats, int try_formats_cnt)
{
    stop_capture();
    SIMPLELOCK(capturer.sync);

    return capturer.start_capture(cfmt, tryformats, try_formats_cnt);
}

void enum_sound_capture_devices(device_enum_callback *cb, void * lpContext)
{
    capturer.enum_sound_capture_devices(cb,lpContext);
}

void set_capture_device(const DEVICE *device)
{
    capturer.capturedevice = *device;
}
void get_capture_device(DEVICE *device)
{
    *device = capturer.capturedevice;
}


}