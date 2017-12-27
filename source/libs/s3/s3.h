/*
    Simple Sound System
    (C) 2010-2015 TAV, ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
    Original:
        (code) https://code.google.com/p/s-3/downloads/list
        (Russian doc) http://www.gamedev.ru/projects/forum/?id=133398
*/
#pragma once

// default sound groups
#ifndef S3_SOUND_GROUPS
#define S3_SOUND_GROUPS \
	SG(SG_3D, 32, true)\
	SG(SG_MUSIC, 2, false)\
	SG(SG_UI, 5, false)\
	SG(SG_VOICE, 16, false)
#endif

#ifdef _WIN32
#define CORE_WIN32
#elif defined __linux__
#define CORE_NIX
#else
#error
#endif

namespace s3
{
#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__)
typedef ptrdiff_t s3int;
#else
typedef int s3int;
#endif

struct Format
{
    int sampleRate;
    short channels, bitsPerSample;

    Format() { memset(this, 0, sizeof(*this)); }
    bool operator==(const Format&f) const
    {
        return channels == f.channels && bitsPerSample == f.bitsPerSample && sampleRate == f.sampleRate;
    }
    bool operator!=(const Format&f) const
    {
        return channels != f.channels || bitsPerSample != f.bitsPerSample || sampleRate != f.sampleRate;
    }

    int sampleSize() const { return channels * (bitsPerSample / 8); }
    int avgBytesPerSec() const { return sampleSize() * sampleRate; }
    int avgBytesPerMSecs(int ms) const { return sampleSize() * (sampleRate * ms / 1000); }
    int bytesToMSec( int bytes ) const { return bytes * 1000 / ( sampleSize() * sampleRate ); }
    int samplesToMSec( int samples ) const { return samples * 1000 / sampleRate; }

};


enum ErrorLevel {EL_ERROR=0, EL_WARNING};
extern void (*ErrorHandler)(ErrorLevel, const char *);


#define SG(name, ...) name,
enum SoundGroup {S3_SOUND_GROUPS SG_COUNT};
#undef  SG
//typedef char SoundGroupsCountStaticCheck[SG_COUNT <= SG_MAX ? 1 : 0];

class Player;
class Slot;

class Source // base for any sound (better use MSource/PSource)
{
	friend class Slot;
    friend class Player;
    friend void slot_coredata_update(Slot *slot, Player *player);

	Source &operator=(const Source &) = delete;
	static s3int readFunc(char *dest, s3int size, void *userPtr) {return ((Source*)userPtr)->read(dest, size);}
	static Source *firstSourceToDelete;
	Source *nextSourceToDelete;
protected:
    Player *player;
	const SoundGroup soundGroup;
	volatile int slotIndex;

public:

    static void autoDeleteSources();

	float pitch; // frequency k
	float volume; // initial volume (set before play), do not change while playing
	void autoDelete();//дл€ созданного по new источника звука можно вызвать эту функцию (только после окончани€ работы с ним), чтобы источник удалилс€ автоматически по окончании проигрывани€

    Player *get_player() {return player;};

	Source(Player *player, SoundGroup soundGroup);
	virtual ~Source() {}//*if (slotIndex != -1) */stop();}//останавливать звук нужно всегда в производных классах, т.к. иначе возможен pure virtual call of read(), т.к. заход в крит. секцию внутри stop() будет уже после разрушени€ производного класса
    virtual void die() { delete this; }

	virtual s3int read(char *dest, s3int size) = 0; // read size bytes to dest and return actually read bytes (0 - no more data, -1 - stop playing)
	virtual void rewind(bool start) {if (!start) ErrorHandler(EL_ERROR, "Can't loop sound since rewind() not implemented");}

//	void canPlayNow();
	void play(bool looping = false, float time = 0);//дл€ минимизации задержки проигрывание включаетс€ сразу же, т.о. перва€ порци€ звуковых данных будет прочитана и распакована в текущем (!) потоке, а потому не используйте блокирующее чтение в read()!
	void stop(float time = 0);
	bool isPlaying() const {return slotIndex != -1;}//иногда может возвращать true, когда звук уже остановлен, но возврат false гарантирует, что звук уже не играет

	//3D-параметры (измен€ть их можно в любое врем€, но место, где они мен€ютс€ лучше обернуть в AutoCriticalSection)
	float position[3];
	float velocity[3];
	float minDist, maxDist;
};

class MSource : public Source //base for in-memory sounds
{
	const char *data;
	int size, pos;
	float startPlayOnInit;
	/*virtual*/ s3int read(char *dest, s3int size) override;
	/*virtual*/ void rewind(bool /*start*/) override {pos = 0;}

public:
	MSource(Player *player, SoundGroup soundGroup) : Source(player, soundGroup), startPlayOnInit(-1) {init();}
	~MSource() {Source::stop();}

	void init(const void *data = nullptr, int size = 0);
	void play(bool looping = false, float time = 0);
	void stop(float time = 0) {startPlayOnInit = -1; Source::stop(time);}
};

class PSource : public Source // base class for prefetched sounds (for stream sounds, eg music)
{
	char *prefetchBuffer, *buf[2]; // 0 - current buffer, 1 - next
	int bufPos, actualDataSize[2], curBuf, prBuf, eofPos[2], prefetchBytes;
	float startPlayOnPrefetchComplete;
	bool waitingForPrefetchComplete, active, eof,
		looped;// loop flag should be set before play
	/*virtual*/ s3int read(char *dest, s3int size) override;
	/*virtual*/ void rewind(bool start) override;

public:
	PSource(Player *player, SoundGroup soundGroup, bool looped = false, bool activate = false, int prefetchBufferSize = 0, char *extPrefetchBuffer = nullptr);
	~PSource() {Source::stop(); delete [] prefetchBuffer;}//в деструкторе производного (пользовательского) класса нужно дождатьс€ окончани€ префетча, иначе данные будут писатьс€ в удаленную пам€ть!

	virtual void prefetch(char *dest, int size) = 0;//вызов означает, что нужно начать читать данные в буфер dest; по окончании нужно вызвать prefetchComplete(); гарантируетс€, что до вызова prefetchComplete, повторный prefetch вызван не будет
	bool prefetchComplete(int size);//если size меньше размера запрошенных данных - это признак конца файла (в этом случае функци€ вернет true - это можно использовать как признак eof, и это значит, что следующий prefetch будет дл€ начала файла)
	void init(bool activate = false, int prefetchBufferSize = 0, char *extPrefetchBuffer = nullptr);//инициализацию нужно делать, если она была отменена при вызове конструктора с prefetchBytes < 0
	void activate();//вызов этой функции инициирует начало префетча
	void play(float time = 0);
	void stop(float time = 0) {startPlayOnPrefetchComplete = -1; Source::stop(time); active = false;}//перед вызовом этой функции нужно прервать prefetch (или дождатьс€ его окончани€)!
	bool update();//эту функцию нужно вызывать где-то в главном цикле приложени€; функци€ возвращает true, если звук остановлен
};

class RawSource : public Source // base class for user RAW sounds
{
	int readStage = 0;
	/*virtual*/ s3int read(char *dest, s3int size) override;
	/*virtual*/ void rewind(bool start) override;

public:
	//fill this params before play
    Format format;

	RawSource(Player *player, SoundGroup soundGroup) : Source(player, soundGroup) {}
	~RawSource() {Source::stop();}

	void play(bool looping = false, float time = 0) {Source::play(looping, time);}

	virtual s3int rawRead(char *dest, s3int size) = 0;
	virtual void rawRewind(bool start) {if (!start) ErrorHandler(EL_ERROR, "Can't loop sound since rawRewind() not implemented");}
};

struct SoundGroupSlots
{
	class Slot *slots = nullptr;
	int active = 0, max = 0;
	float volume = 1.0f;
    bool is3d = false;

	SoundGroupSlots() {}
	~SoundGroupSlots();
	void initialize(const struct SlotInitParams &sip);
};


#ifdef CORE_WIN32
struct WIN32GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
};
#endif

struct DEVICE
{
#ifdef CORE_WIN32
    WIN32GUID guid;
#endif

    DEVICE()
    {
        memset( this, 0, sizeof(DEVICE) );
    }

    bool operator == ( const DEVICE &d ) const
    {
        return memcmp( this, &d, sizeof(DEVICE) ) == 0;
    }
};
#define DEFAULT_DEVICE DEVICE()

struct InitParams
{
    DEVICE device = DEFAULT_DEVICE;
    int channels = 2, sampleRate = 48000, bitsPerSample = 16, prefetchBytes = 512 * 1024;
    float bufferLength = .25f; //length of sound buffers (in seconds)
	bool useHW = false, ctrlFrequency = false, rightHandedCS = false;

    // useHW - hardware mixing
    // tunePitch - decrease play frequency to avoid audio stream data holes

};

struct SlotInitParams {int max; bool is3d;};
#define SG(name, max, is3d, ...) {max, is3d},
static const SlotInitParams defaultSlotsInitParams[SG_COUNT] = {S3_SOUND_GROUPS};
#undef  SG

class Player
{
public:
#if defined (_M_AMD64) || defined (WIN64) || defined (__LP64__)
    enum { player_data_size = 128 };
#else
    enum { player_data_size = 64 };
#endif
    char data[ player_data_size ]; // its public, but you should not modify it - internal data
    InitParams params;
    bool nodata = false; // sound engine will set this to true if no data

    Player( const Player& ) = delete;
    Player( Player && ) = delete;

    Player();
    ~Player();

    void operator=( Player &&p );

    bool Initialize(const SlotInitParams slotsIP[] = defaultSlotsInitParams, const int sgCount = SG_COUNT);
    void Shutdown(bool reinit = false /*internal use only!!! NEVER! NEVER call this method with true argument!*/ );
    void SetMasterVolume(float f);
    void SetPitch(float k);
    float GetPitch() const;


    inline void SetSoundGroupVolume(SoundGroup sg, float volume)
    {
        SoundGroupSlots *sgs = getSoundGroups();
        if (sgs) sgs[sg].volume = volume;
    }
    void SetListenerParameters(const float pos[3], const float front[3], const float top[3], const float vel[3] = nullptr, float distFactor = 1, float rolloffFactor = 1, float dopplerFactor = 1);

    SoundGroupSlots *getSoundGroups(); // can return nullptr

};

void Update(); // call it at least once per 5 second - it will cleanup autodeleted sources

#ifdef _MSC_VER
typedef wchar_t wchar;
#define S3CALL __stdcall
#elif defined __GNUC__
typedef char16_t wchar;
#define S3CALL
#endif

typedef bool S3CALL device_enum_callback(DEVICE *device, const wchar *lpcstrDescription, void *lpContext); // return true to continue enum

void enum_sound_play_devices(device_enum_callback *cb, void * lpContext);

}

#include "capture.h"
