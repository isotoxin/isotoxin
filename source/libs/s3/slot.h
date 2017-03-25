#pragma once

#include "s3.h"
#include "decoder.h"

#ifdef CORE_WIN32
#ifdef _M_X64
#define SLOT_COREDATA_SIZE (sizeof(void *)*3)
#else
#define SLOT_COREDATA_SIZE (sizeof(void *)*4)
#endif
#define PLAYER_COREDATA_SIZE (sizeof(void *)*6)
#elif defined CORE_NIX
#define SLOT_COREDATA_SIZE 4
#define PLAYER_COREDATA_SIZE 4
#endif

namespace s3
{
    void slot_coredata_clear(Slot *slot);
    bool slot_coredata_prepare(Slot *slot, Player *player, bool is3d);
    void slot_coredata_start_play(Slot *slot, Player *player);
    void slot_coredata_stop_play(Slot *slot);
    void slot_coredata_update(Slot *slot, Player *player, bool full);
    bool slot_coredata_fail(Slot *slot);

class Slot
{
public:
	Source *source = nullptr;
    char coredata[SLOT_COREDATA_SIZE] = {};
	Format format;
	int silenceSize;
	Decoder *decoder;//используется указатель, чтобы можно было делать std::swap

	bool paused, looping;
	float fade, fadeSpeed;

    Slot() : decoder(new Decoder) {}
	~Slot()
	{
		clear();
		delete decoder;
	}
	void clear()
	{
		if (source) stop();
        slot_coredata_clear(this);
		memset(&format, 0, sizeof(format));
	}

	bool prepare(Player *player, const Format &f, bool is3d);


	void startPlay(Player *player, float time);
	void stop();
	void stop(float time) {if (time == 0) stop(); else fadeSpeed = -1/time;}

	void read(void *buffer, s3int size, bool afterRewind = false);
	void update(Player *player, float dt, bool full);
};

}
