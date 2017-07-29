#include "stdafx.h"
#include "slot.h"


namespace s3
{
#include "player_data.h"

bool Slot::prepare(Player *player, const Format &f, bool is3d)
{
    format = f;
    return slot_coredata_prepare(this, player, is3d);
}

void Slot::startPlay(Player *player, float time)
{
    silenceSize = 0;
    paused = false;
    if (time) fade = 0, fadeSpeed = 1 / time; else fade = 1, fadeSpeed = 0;

    slot_coredata_start_play(this, player);
}

s3int Slot::read(void *b, s3int size, bool afterRewind)
{
    s3int r = decoder->read(b, size);
    if (r < 0)
    {
        // stop request
        stop();
        return 0;
    }

    if (r > 0)
        silenceSize = 0;

	if (r != size)
	{
		if (looping && !(afterRewind && r == 0)) // вторая проверка - на случай, когда сразу после rewind вызов read вернул 0 (если файл пустой напр.) - если это не обработать, может получиться бесконечный цикл
		{
			source->rewind(false);
			if (decoder->init(Source::readFunc, source))
				return read((char*)b + r, size - r, true);
		}

        s3int ragain = decoder->read((char*)b + r, size - r);
        if (ragain < 0)
        {
            if (r > 0)
            {
                silenceSize += (int)(size - r);
                memset((char *)b + r, format.bitsPerSample == 8 ? 0x80 : 0, size - r); // fill tail with silence
                return r;
            }

            stop();
            return 0;
        }

        if (ragain == 0)
        {
            silenceSize += (int)(size - r);
            memset((char *)b + r, format.bitsPerSample == 8 ? 0x80 : 0, size - r); // fill tail with silence
        }

        return r + ragain;
	}

    return r;
}

void Slot::stop()
{
	if (source == nullptr) {ErrorHandler(EL_ERROR, "???"); return;}

    slot_coredata_stop_play(this);

	source->slotIndex = -1;
	source = nullptr;
}

void Slot::update(Player *player, float dt)
{
	if (source == nullptr) return;
    
	// refresh volume
	fade += fadeSpeed * dt;
	if (fade < 0) {stop(); return;}//если сделать проверку на <= 0, то play() с time > 0 работать не будет :), см. if (time) fade = 0, ...
	if (fade > 1) fade = 1, fadeSpeed = 0;

    slot_coredata_update(this, player);

}
}
