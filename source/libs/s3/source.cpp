#include "stdafx.h"
#include "slot.h"


namespace s3
{
#include "player_data.h"

Source::Source(Player *player, SoundGroup soundGroup) : player(player), soundGroup(soundGroup), nextSourceToDelete(nullptr), slotIndex(-1), pitch(1), volume(1), minDist(1), maxDist(1e9)
{
	memset(position, 0, sizeof(position));
	memset(velocity, 0, sizeof(velocity));
}

Source *Source::firstSourceToDelete = nullptr;

void Source::autoDeleteSources()
{
	for (Source **s=&firstSourceToDelete; *s;)
	{
		if ((*s)->slotIndex == -1)//звук не проигрывается - можно удалять
		{
			Source *next = (*s)->nextSourceToDelete;
			(*s)->die();
			*s = next;
		}
		else
			s=&(*s)->nextSourceToDelete;
	}
}

void Source::autoDelete()
{
	nextSourceToDelete = firstSourceToDelete;
	firstSourceToDelete = this;
}

void Source::play(bool looping, float time)
{
    player_data_s &pd = *(player_data_s *)&player->data;

	if (pd.pDS == nullptr) return;
    SIMPLELOCK(pd.sync1);

	autoDeleteSources();

	if (slotIndex != -1 && time > 0)//в виде исключения разрешено повторное включение проигрывания звука при задании времени time, т.к. в этом случае резкой остановки звука не будет
	{
		pd.soundGroups[soundGroup].slots[slotIndex].fadeSpeed = 1/time;
		return;
	}
	if (slotIndex != -1) {ErrorHandler(EL_ERROR, "Sound source is already playing"); return;}
	//if (format.channels == 0) {ErrorHandler(EL_ERROR, "Sound format is not specified"); return;}

	rewind(true);
	if (!pd.gdecoder->init(readFunc, this)) {ErrorHandler(EL_ERROR, "Decoder error"); return;}

	int found = -1;
	SoundGroupSlots &sg = pd.soundGroups[soundGroup];
	for (int i=0; i<sg.active; i++) // is active slot free?
	{
		if (sg.slots[i].source == nullptr)
		{
			found = i;
			if (memcmp(&sg.slots[i].format, &pd.gdecoder->format, sizeof(Format)) == 0) goto skip_create;//perfect match!
		}
	}

	if (found != -1) // slot is free, but format mismatch - need to recreate buffer
	{
		sg.slots[found].releaseBuffer();
	}
	else // no free slots
	{
		if (sg.active == sg.max) {ErrorHandler(EL_WARNING, "Not enought slots to play sound"); return;}
		found = sg.active++; // add new slot
	}

	if (!sg.slots[found].createBuffer(player, pd.gdecoder->format, sg.is3d)) return;

skip_create:

	Slot &slot = sg.slots[found];
	std::swap(pd.gdecoder, slot.decoder);

	slotIndex = found;
	slot.source = this;
	slot.looping = looping;
	slot.startPlay(player,time);
}

void Source::stop(float time)
{
	if (slotIndex == -1 || player == nullptr) return;//предварительная быстрая, но неточная проверка, которая допустима из расчета, что запись int - атомарная операция, а в параллельном потоке может быть записано только одно значение (-1)
    player_data_s &pd = *(player_data_s *)&player->data;
	if (pd.pDS == nullptr) return;
    SIMPLELOCK(pd.sync1);
	if (slotIndex == -1) {/*ErrorHandler(EL_ERROR, "Sound source is already stopped");*/ return;}
	pd.soundGroups[soundGroup].slots[slotIndex].stop(time);
}


s3int MSource::read(char *dest, s3int sz)
{
    s3int r = std::min(sz, (s3int)(size-pos));
	memcpy(dest, data+pos, r);
	pos += (int)r;
	return r;
}

void MSource::init(const void *data_, int size_)
{
	if (slotIndex != -1) {ErrorHandler(EL_ERROR, "init() can not be called during playing"); return;}

	data = (const char*)data_;
	size = size_;
//	pos = 0;//это не обязательно, т.к. перед началом проигрывания гарантированно вызовется rewind()
	if (startPlayOnInit >= 0 && data)
	{
		Source::play(true, startPlayOnInit);
		startPlayOnInit = -1;
	}
}

void MSource::play(bool looping, float time)
{
    player_data_s &pd = *(player_data_s *)&player->data;
	if (pd.pDS == nullptr) return;

	if (data == nullptr) // no data (yet)
	{
		if (looping) startPlayOnInit = time; // start playing on data
		return;
	}

	Source::play(looping, time);
}


PSource::PSource(Player *player, SoundGroup soundGroup, bool looped, bool activate_, int prefetchBufferSize, char *extPrefetchBuffer) : Source(player, soundGroup), looped(looped), startPlayOnPrefetchComplete(-1), active(false)
{
	if (prefetchBufferSize >= 0) init(activate_, prefetchBufferSize, extPrefetchBuffer);
}

void PSource::init(bool activate_, int prefetchBufferSize, char *extPrefetchBuffer)
{
	prefetchBytes = prefetchBufferSize ? prefetchBufferSize/2 : player->params.prefetchBytes;
	if (extPrefetchBuffer) {prefetchBuffer = nullptr; buf[0] = extPrefetchBuffer;} else
	buf[0] = prefetchBuffer = new char [2*prefetchBytes];
	buf[1] = buf[0] + prefetchBytes;
	if (activate_) activate();
//	prefetch(prefetchBuffer, 2*prefetchBytes);
}

void PSource::activate()
{
	if (active) {ErrorHandler(EL_ERROR, "Already in an active state"); return;}

	curBuf = prBuf = bufPos = 0;
	actualDataSize[0] = actualDataSize[1] = 0;
	eofPos[0] = eofPos[1] = prefetchBytes;
	waitingForPrefetchComplete = false;
//	eof = false;//это не обязательно, т.к. перед началом проигрывания гарантированно вызовется rewind()
	active = true;
}

s3int PSource::read(char *dest, s3int sz)
{
	if (eof) return 0;

	int total = 0;

	while (sz > 0 && actualDataSize[curBuf] > 0)
	{
        s3int r = std::min(sz, (s3int)std::min(actualDataSize[curBuf], eofPos[curBuf]) - bufPos);
		memcpy(dest + total, buf[curBuf] + bufPos, r);
		bufPos += (int)r;
		sz -= r;
		total += (int)r;
		if (bufPos == prefetchBytes)
		{
			bufPos = 0;
			actualDataSize[curBuf] = 0;
			eofPos[curBuf] = prefetchBytes;
			curBuf ^= 1;
		}
		else if (bufPos == eofPos[curBuf])
		{
			eofPos[curBuf] = actualDataSize[curBuf];//снимаем пометку, либо переставляем её на след. блок данных
			eof = true;
			break;
		}
	}

	if (sz > 0)//eof reached (eof == true или данные не успели подгрузиться)
		if (!looped) active = false;//все данные отдали - можно деактивировать PSource

	return total;
}

void PSource::rewind(bool start)
{
	eof = false;
	if (!start && eofPos[curBuf] < prefetchBytes)
	{
		bufPos = eofPos[curBuf];//пропускаем "мусор" в конце файла (некоторые программы любят добавлять служебные блоки в конец), иначе зацикливание не будет работать в этих случаях
		eofPos[curBuf] = actualDataSize[curBuf];//снимаем пометку, либо переставляем её на след. блок данных
	}
}

bool PSource::prefetchComplete(int size)
{
    player_data_s &pd = *(player_data_s *)&player->data;
    SIMPLELOCK(pd.sync1);//защищаем доступ к actualDataSize из read(), которая вызывается в параллельном потоке
	bool res = false;

	do//цикл нужен только для обработки случая начальной загрузки 2*prefetchBytes байт, т.к. конец файла может быть достигнут на меньшем кол-ве байт
	{
		int r = std::min(size, prefetchBytes - actualDataSize[prBuf]);
		actualDataSize[prBuf] += r;
		size -= r;
		if (actualDataSize[prBuf] == prefetchBytes) prBuf ^= 1;
		else//запись неполных данных означает, что достигнут конец файла - ставим соответствующую пометку
		{
			if (eofPos[prBuf] == prefetchBytes)//но только при условии, что эта пометка еще не стоит (нужно для коротких файлов, размер которых меньше prefetchBytes)
			{
				_ASSERT(size == 0);
				eofPos[prBuf] = actualDataSize[prBuf];//это даст возможность продолжать префетч (уже для следующего файла)
			}
			res = true;
		}
	} while (size > 0);

	waitingForPrefetchComplete = false;

// 	if (startPlayOnPrefetchComplete >= 0)//т.к. prefetchComplete может вызываться из другого потока, лучше запускать звук в update (the thread that plays the buffer must continue to exist as long as the buffer is playing)
// 	{
// 		Source::play(looped, startPlayOnPrefetchComplete);
// 		startPlayOnPrefetchComplete = -1;
// 	}

	return res;
}

void PSource::play(float time)
{
	if (!active) {ErrorHandler(EL_ERROR, "PSource should be activated before play"); return;}
    
    player_data_s &pd = *(player_data_s *)&player->data;

	if (pd.pDS == nullptr) return;
    spinlock::auto_simple_lock asl(pd.sync1); //защищаем доступ к actualDataSize из read(), которая вызывается в параллельном потоке

	if (waitingForPrefetchComplete || (actualDataSize[0] == 0 && actualDataSize[1] == 0))
	{
		startPlayOnPrefetchComplete = time;
		return;
	}

    asl.unlock();

	Source::play(looped, time);
}

bool PSource::update()
{
    player_data_s &pd = *(player_data_s *)&player->data;
	if (pd.pDS == nullptr) return false;
    spinlock::auto_simple_lock asl(pd.sync1);//защищаем доступ к actualDataSize из read(), которая вызывается в параллельном потоке

	if (!active) return slotIndex == -1;

	DWORD status;
	if (slotIndex != -1 && pd.soundGroups[soundGroup].slots[slotIndex].buffer->GetStatus(&status) == DSERR_BUFFERLOST)//такое может быть при вытаскивании наушников в Windows 7
	{
		player->Shutdown(true);//try reinit
		if (pd.pDS == nullptr) return false;
	}

	if (!waitingForPrefetchComplete)
	{
		if (!looped && eofPos[prBuf] < prefetchBytes) ;//достигнут конец файла незацикленного звука - ничего не делаем
		else if (actualDataSize[prBuf] == (eofPos[prBuf] < prefetchBytes ? eofPos[prBuf] : 0))
		{
			waitingForPrefetchComplete = true;

			if (actualDataSize[0] == 0 && actualDataSize[1] == 0)//первый раз грузим оба буфера сразу
				prefetch(buf[0], 2*prefetchBytes);
			else
				prefetch(buf[prBuf]+actualDataSize[prBuf], prefetchBytes-actualDataSize[prBuf]);
		}

		if (startPlayOnPrefetchComplete >= 0 && actualDataSize[0] > 0)//начинаем играть как только появились данные
		{
            asl.unlock();
			Source::play(looped, startPlayOnPrefetchComplete);
			startPlayOnPrefetchComplete = -1;
		}
	}

	return slotIndex == -1;
}


s3int RawSource::read(char *dest, s3int sz)
{
	switch (readStage)
	{
	case 0:
		_ASSERT(sz == 4);
		*(DWORD*)dest = MAKEFOURCC('R','A','W',' ');
		readStage++;
		return 4;

	case 1:
		_ASSERT(sz == sizeof(Format));
		memcpy(dest, &format, sizeof(Format));
		readStage++;
		return sizeof(Format);

	default:
		return rawRead(dest, sz);
	}
}

void RawSource::rewind(bool start)
{
	readStage = 0;
	rawRewind(start);
}
}
