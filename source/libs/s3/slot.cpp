#include "stdafx.h"
#include "slot.h"

MY_GUID(MY_IID_IDirectSoundBuffer8, 0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e);
MY_GUID(MY_IID_IDirectSound3DBuffer, 0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

namespace s3
{
#include "player_data.h"

bool Slot::createBuffer(Player *player, const Format &f, bool is3d)
{
    player_data_s &pd = *(player_data_s *)&player->data;

	DSBUFFERDESC dsbd = {sizeof(DSBUFFERDESC)};
	dsbd.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2
		| (is3d ? DSBCAPS_CTRL3D : 0)
		| ( player->params.ctrlFrequency ? DSBCAPS_CTRLFREQUENCY : 0)
		| (!player->params.useHW ? DSBCAPS_LOCSOFTWARE : 0)
		| (!player->params.hwnd  ? DSBCAPS_GLOBALFOCUS : 0);
	WAVEFORMATEX wf = BuildWaveFormat(f.channels, f.sampleRate, f.bitsPerSample);
	dsbd.dwBufferBytes = int(player->params.bufferLength * wf.nAvgBytesPerSec);
	dsbd.dwBufferBytes += wf.nBlockAlign - dsbd.dwBufferBytes % wf.nBlockAlign;//остаток от деления dwBufferBytes на wf.nBlockAlign должен быть равен 0
	dsbd.lpwfxFormat = &wf;
	bufferSize = dsbd.dwBufferBytes;

	LPDIRECTSOUNDBUFFER buf;
	if (FAILED(pd.pDS->CreateSoundBuffer(&dsbd, &buf, nullptr)))
	{
		player->Shutdown(true);//try reinit (такое может быть при вытаскивании наушников в Windows 7 - CreateSoundBuffer при этом возвращает DSERR_BUFFERLOST)
		if (pd.pDS == nullptr) return false;
		if (FAILED(pd.pDS->CreateSoundBuffer(&dsbd, &buf, nullptr)))
		{
			ErrorHandler(EL_ERROR, "Can't create sound buffer");
			return false;
		}
	}
	buf->QueryInterface(MY_IID_IDirectSoundBuffer8, (LPVOID*)&buffer);//на всякий случай получаем новый интерфейс буфера
	if (is3d) buf->QueryInterface(MY_IID_IDirectSound3DBuffer, (LPVOID*)&buffer3D);
	buf->Release();

	format = f;

	return true;
}

void Slot::startPlay(Player *player, float time)
{
	buffer->GetCurrentPosition(&prevPlayPos, nullptr);
	silenceSize = -1;
	paused = false;
	if (time) fade = 0, fadeSpeed = 1/time; else fade = 1, fadeSpeed = 0;
	update(player, 0, true);
	if (player->params.ctrlFrequency) buffer->SetFrequency(DWORD(decoder->format.sampleRate * source->pitch));
	buffer->Play(0, 0, DSBPLAY_LOOPING);
}

void Slot::read(void *b, s3int size, bool afterRewind)
{
	if (silenceSize == -1)
	{
        s3int r = decoder->read(b, size);
        if (r < 0)
        {
            // stop request
            stop();
            return;
        }
		if (r != size)
		{
			if (looping && !(afterRewind && r == 0)) // вторая проверка - на случай, когда сразу после rewind вызов read вернул 0 (если файл пустой напр.) - если это не обработать, может получиться бесконечный цикл
			{
				source->rewind(false);
				if (decoder->init(Source::readFunc, source))
					return read((char*)b + r, size - r, true);
			}

			silenceSize = 0;
			read((char*)b + r, size - r);
		}
	}
	else
	{
		memset(b, format.bitsPerSample == 8 ? 0x80 : 0, size); // fill with silence
		silenceSize += (int)size;
	}
}

void Slot::stop()
{
	if (source == nullptr) {ErrorHandler(EL_ERROR, "???"); return;}

	buffer->Stop();
//	buffer->SetCurrentPosition(0);
	source->slotIndex = -1;
	source = nullptr;
}

void Slot::update(Player *player, float dt, bool full)
{
	if (source == nullptr) return;
    player_data_s &pd = *(player_data_s *)&player->data;

	// refresh volume
	fade += fadeSpeed * dt;
	if (fade < 0) {stop(); return;}//если сделать проверку на <= 0, то play() с time > 0 работать не будет :), см. if (time) fade = 0, ...
	if (fade > 1) fade = 1, fadeSpeed = 0;
	buffer->SetVolume(LinearToDirectSoundVolume(fade * source->volume * pd.soundGroups[source->soundGroup].volume));

	// fill sound buffer
	DWORD playPos;
	if (SUCCEEDED(buffer->GetCurrentPosition(&playPos, nullptr)))
	{
		DWORD readSize = (playPos >= prevPlayPos && !full) ? playPos - prevPlayPos : bufferSize - prevPlayPos + playPos;//если в условии оставить только playPos > prevPlayPos, то в случае если позиция не успела измениться (playPos == prevPlayPos), будет ошибочно прочитан весь буфер

		LPVOID ptr1, ptr2;
		DWORD size1, size2;
		HRESULT hr = buffer->Lock(prevPlayPos, readSize, &ptr1, &size1, &ptr2, &size2, 0);

		if (hr == DSERR_BUFFERLOST)//код взят из DX SDK: Using Streaming Buffers
		{
			buffer->Restore();
			hr = buffer->Lock(prevPlayPos, readSize, &ptr1, &size1, &ptr2, &size2, 0);
		}

		if (SUCCEEDED(hr))
		{
			read(ptr1, size1);
			if (ptr2) read(ptr2, size2);
			buffer->Unlock(ptr1, size1, ptr2, size2);

			prevPlayPos = playPos;

			if (silenceSize >= bufferSize) {stop(); return;}//как только весь буфер заполнили тишиной, это значит, что play cursor прошел последний семпл звука и проигрывание можно останавливать
		}
	}

	//Обновляем 3D-параметры
	if (buffer3D)
	{
		DS3DBUFFER pars;
 		pars.dwSize = sizeof(DS3DBUFFER);
 		ConvertVectorToDirectSoundCS(pars.vPosition, source->position, player->params.rightHandedCS);
 		ConvertVectorToDirectSoundCS(pars.vVelocity, source->velocity, player->params.rightHandedCS);
		pars.vConeOrientation.x = 0;
		pars.vConeOrientation.y = 0;
 		pars.vConeOrientation.z = 1;
		pars.dwInsideConeAngle = 360;
		pars.dwOutsideConeAngle = 360;
		pars.lConeOutsideVolume = 0;
		pars.flMinDistance = source->minDist;
		pars.flMaxDistance = source->maxDist;
		pars.dwMode = DS3DMODE_NORMAL;
		buffer3D->SetAllParameters(&pars, full/*целиком буфер обновляется только в одном случае - сразу перед началом проигрывания*/ ? DS3D_IMMEDIATE : DS3D_DEFERRED);
	}
}
}
