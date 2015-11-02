/*
 *  Copyright (C) 2014 Hugh Bailey <obs.jim@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#pragma once

namespace DShow {

struct DeviceEncodedData {
	long long                      lastStartTime = 0;
	long long                      lastStopTime  = 0;
	std::vector<unsigned char>          bytes;
};

struct EncodedDevice {
	VideoFormat videoFormat;
	ULONG       videoPacketID;
	long        width;
	long        height;
	long long   frameInterval;

#ifdef AUDIO_CAPTURE
	AudioFormat audioFormat;
	ULONG       audioPacketID;
	DWORD       samplesPerSec;
#endif
};

struct HDevice {
	ComPtr<IGraphBuilder>          graph;
	ComPtr<ICaptureGraphBuilder2>  builder;
	ComPtr<IMediaControl>          control;

	ComPtr<IBaseFilter>            videoFilter;
	ComPtr<CaptureFilter>          videoCapture;
	ComPtr<IBaseFilter>            rocketEncoder;
	MediaType                      videoMediaType;
	VideoConfig                    videoConfig;
    DeviceEncodedData              encodedVideo;

#ifdef AUDIO_CAPTURE
    ComPtr<IBaseFilter>            audioFilter;
    ComPtr<CaptureFilter>          audioCapture;
    ComPtr<IBaseFilter>            audioOutput;
    MediaType                      audioMediaType;
	AudioConfig                    audioConfig;
    DeviceEncodedData              encodedAudio;
#endif

	bool                           encodedDevice = false;
	bool                           initialized;
	bool                           active;


	HDevice();
	~HDevice();

	void ConvertVideoSettings();
#ifdef AUDIO_CAPTURE
	void ConvertAudioSettings();
#endif

	bool EnsureInitialized(const wchar_t *func);
	bool EnsureActive(const wchar_t *func);
	bool EnsureInactive(const wchar_t *func);

	inline void SendToCallback(bool video,
			unsigned char *data, size_t size,
			long long startTime, long long stopTime);
    
    void ReceiveVideo(IMediaSample *s) { Receive(true, s); };
    void ReceiveNonVideo(IMediaSample *s) { Receive(false, s); };
	void Receive(bool video, IMediaSample *sample);

	bool SetupEncodedVideoCapture(IBaseFilter *filter,
				VideoConfig &config,
				const EncodedDevice &info);

	bool SetupExceptionVideoCapture(IBaseFilter *filter,
			VideoConfig &config);

	bool SetupExceptionAudioCapture(IPin *pin);

	bool SetupVideoCapture(IBaseFilter *filter, VideoConfig &config);
    bool SetVideoConfig(VideoConfig *config);

#ifdef AUDIO_CAPTURE
	bool SetupAudioCapture(IBaseFilter *filter, AudioConfig &config);
	bool SetupAudioOutput(IBaseFilter *filter, AudioConfig &config);
    bool SetAudioConfig(AudioConfig *config);
#endif


	bool CreateGraph();
	bool FindCrossbar(IBaseFilter *filter, IBaseFilter **crossbar);
	bool ConnectPins(const GUID &category, const GUID &type,
			IBaseFilter *filter, IBaseFilter *capture);
	bool RenderFilters(const GUID &category, const GUID &type,
			IBaseFilter *filter, IBaseFilter *capture);
	bool ConnectFilters();
	void DisconnectFilters();
	Result Start();
	void Stop();
};

}; /* namespace DShow */
