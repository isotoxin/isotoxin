#pragma once

namespace s3
{
void enum_sound_capture_devices(device_enum_callback *lpDSEnumCallback, LPVOID lpContext);
void set_capture_device( const DEVICE *dev );
void get_capture_device( DEVICE *dev );

typedef void capture_callback( const void *data, int datasize, void *context );

bool get_capture_format(Format & cfmt);
bool start_capture(Format & cfmt, const Format * tryformats = nullptr, int try_formats_cnt = 0);
bool is_capturing();
void stop_capture();
void capture_tick( capture_callback * data_callback, void *context );

}

