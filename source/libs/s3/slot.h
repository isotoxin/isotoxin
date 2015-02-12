#pragma once

#include "s3.h"
#include "decoder.h"


namespace s3
{
class Slot
{
public:
	Source *source;
	LPDIRECTSOUNDBUFFER8 buffer;
	LPDIRECTSOUND3DBUFFER8 buffer3D;
	DWORD prevPlayPos;
	Format format;
	int bufferSize, silenceSize;
	Decoder *decoder;//используется указатель, чтобы можно было делать std::swap

	bool paused, looping;
	float fade, fadeSpeed;

	Slot() : source(nullptr), buffer(nullptr), buffer3D(nullptr), decoder(new Decoder) {}
	~Slot()
	{
		clear();
		delete decoder;
	}
	void clear()
	{
		if (source) stop();
		releaseBuffer();
		memset(&format, 0, sizeof(format));
	}

	void releaseBuffer()
	{
		if (buffer  ) buffer  ->Release(), buffer   = nullptr;
		if (buffer3D) buffer3D->Release(), buffer3D = nullptr;
	}
	bool createBuffer(Player *player, const Format &f, bool is3d);
	void startPlay(Player *player, float time);
	void stop();
	void stop(float time) {if (time == 0) stop(); else fadeSpeed = -1/time;}

	void read(void *buffer, int size, bool afterRewind = false);
	void update(Player *player, float dt, bool full);
};

inline WAVEFORMATEX BuildWaveFormat(int channels, int sampleRate, int bitsPerSample)
{
	WAVEFORMATEX wf = {0};
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = (WORD)channels;
	wf.nSamplesPerSec = sampleRate;
	wf.wBitsPerSample = (WORD)bitsPerSample;
	wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	return wf;
}

inline int LinearToDirectSoundVolume(float k)
{
	return k <= 0 ? DSBVOLUME_MIN : k >= 1 ? DSBVOLUME_MAX : std::max(DSBVOLUME_MIN, int((100*20) * log10f(k)));//100 = -DSBVOLUME_MIN/100 dB, 20 - из определения dB для sound pressure level
}

inline void ConvertVectorToDirectSoundCS(D3DVECTOR &dest, const float v[3], bool rightHandedCS)
{
	dest.x = v[0];
	if (rightHandedCS)  dest.y = v[2], dest.z = v[1];//другой способ - менять знак z
	else                dest.y = v[1], dest.z = v[2];
}
}
