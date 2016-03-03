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

#define MY_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

namespace s3
{

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

    int blockAlign() const { return channels * (bitsPerSample / 8); }
    int avgBytesPerSec() const { return blockAlign() * sampleRate; }
    int avgBytesPerMSecs(int ms) const { return blockAlign() * sampleRate * ms / 1000; }

};


enum ErrorLevel {EL_ERROR=0, EL_WARNING};
extern void (*ErrorHandler)(ErrorLevel, const char *);


#define SG(name, ...) name,
enum SoundGroup {S3_SOUND_GROUPS SG_COUNT};
#undef  SG
//typedef char SoundGroupsCountStaticCheck[SG_COUNT <= SG_MAX ? 1 : 0];

class Player;

class Source //базовый класс для источников звука (лучше использовать сразу MSource/PSource)
{
	friend class Slot;
    friend class Player;
	Source &operator=(const Source &) = delete;//запрещаем присваивание
	static int readFunc(char *dest, int size, void *userPtr) {return ((Source*)userPtr)->read(dest, size);}
	static Source *firstSourceToDelete;
	Source *nextSourceToDelete;
protected:
    Player *player;
	const SoundGroup soundGroup;
	volatile int slotIndex;

public:

    static void autoDeleteSources();

	float pitch;//коэффициент частоты звука
	float volume;//начальный уровень громкости (можно задать перед началом прогрывания, менять во время проигрывания не рекомендуется)
	void autoDelete();//для созданного по new источника звука можно вызвать эту функцию (только после окончания работы с ним), чтобы источник удалился автоматически по окончании проигрывания

    Player *get_player() {return player;};

	Source(Player *player, SoundGroup soundGroup);
	virtual ~Source() {}//*if (slotIndex != -1) */stop();}//останавливать звук нужно всегда в производных классах, т.к. иначе возможен pure virtual call of read(), т.к. заход в крит. секцию внутри stop() будет уже после разрушения производного класса
    virtual void die() { delete this; }

	virtual int read(char *dest, int size) = 0;//нужно прочитать данные в dest буфер и вернуть размер прочитанных данных (0 в случае конца файла, < 0 - в случае ошибки)
	virtual void rewind(bool start) {if (!start) ErrorHandler(EL_ERROR, "Can't loop sound since rewind() not implemented");}

//	void canPlayNow();
	void play(bool looping = false, float time = 0);//для минимизации задержки проигрывание включается сразу же, т.о. первая порция звуковых данных будет прочитана и распакована в текущем (!) потоке, а потому не используйте блокирующее чтение в read()!
	void stop(float time = 0);
	bool isPlaying() const {return slotIndex != -1;}//иногда может возвращать true, когда звук уже остановлен, но возврат false гарантирует, что звук уже не играет

	//3D-параметры (изменять их можно в любое время, но место, где они меняются лучше обернуть в AutoCriticalSection)
	float position[3];
	float velocity[3];
	float minDist, maxDist;
};

class MSource : public Source//базовый класс для пользовательских источников звука, данные которых доступны всегда, т.е. находящихся целиком в памяти
{
	const char *data;
	int size, pos;
	float startPlayOnInit;
	/*virtual*/ int read(char *dest, int size);
	/*virtual*/ void rewind(bool /*start*/) {pos = 0;}

public:
	MSource(Player *player, SoundGroup soundGroup) : Source(player, soundGroup), startPlayOnInit(-1) {init();}
	~MSource() {Source::stop();}

	void init(const void *data = nullptr, int size = 0);
	void play(bool looping = false, float time = 0);
	void stop(float time = 0) {startPlayOnInit = -1; Source::stop(time);}
};

class PSource : public Source//базовый класс для пользовательских источников звука с prefetch-ем (для подгружаемых звуков, напр. музыки)
{
	char *prefetchBuffer, *buf[2];//0 - текущий буфер, 1 - следующий
	int bufPos, actualDataSize[2], curBuf, prBuf, eofPos[2], prefetchBytes;
	float startPlayOnPrefetchComplete;
	bool waitingForPrefetchComplete, active, eof,
		looped;//зацикленность звука с префетчем нужно задавать заранее, т.к. prefetch делается заранее, еще до вызова play()
	/*virtual*/ int read(char *dest, int size);
	/*virtual*/ void rewind(bool start);

public:
	PSource(Player *player, SoundGroup soundGroup, bool looped = false, bool activate = false, int prefetchBufferSize = 0, char *extPrefetchBuffer = nullptr);
	~PSource() {Source::stop(); delete [] prefetchBuffer;}//в деструкторе производного (пользовательского) класса нужно дождаться окончания префетча, иначе данные будут писаться в удаленную память!

	virtual void prefetch(char *dest, int size) = 0;//вызов означает, что нужно начать читать данные в буфер dest; по окончании нужно вызвать prefetchComplete(); гарантируется, что до вызова prefetchComplete, повторный prefetch вызван не будет
	bool prefetchComplete(int size);//если size меньше размера запрошенных данных - это признак конца файла (в этом случае функция вернет true - это можно использовать как признак eof, и это значит, что следующий prefetch будет для начала файла)
	void init(bool activate = false, int prefetchBufferSize = 0, char *extPrefetchBuffer = nullptr);//инициализацию нужно делать, если она была отменена при вызове конструктора с prefetchBytes < 0
	void activate();//вызов этой функции инициирует начало префетча
	void play(float time = 0);
	void stop(float time = 0) {startPlayOnPrefetchComplete = -1; Source::stop(time); active = false;}//перед вызовом этой функции нужно прервать prefetch (или дождаться его окончания)!
	bool update();//эту функцию нужно вызывать где-то в главном цикле приложения; функция возвращает true, если звук остановлен
};

class RawSource : public Source//базовый класс для пользовательских источников звука, данные которых уже распакованы (т.е. представлены в виде RAW-семплов без заголовка)
{
	int readStage = 0;
	/*virtual*/ int read(char *dest, int size);
	/*virtual*/ void rewind(bool start);

public:
	//Эти параметры нужно заполнить перед началом проигрывания
    Format format;

	RawSource(Player *player, SoundGroup soundGroup) : Source(player, soundGroup) {}
	~RawSource() {Source::stop();}

	void play(bool looping = false, float time = 0) {Source::play(looping, time);}

	virtual int rawRead(char *dest, int size) = 0;
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

#ifdef WIN32
typedef GUID DEVICE;
inline GUID __default_device() /*constexpr*/ { GUID g = {0}; return g; }
#define DEFAULT_DEVICE __default_device()
#endif

struct InitParams
{
    DEVICE device = DEFAULT_DEVICE;
    void *hwnd = nullptr; // HWND not defined yet
    int channels = 2, sampleRate = 44100, bitsPerSample = 16, prefetchBytes = 512 * 1024;
    float bufferLength = .25f; //длительность звуковых буферов для всех слотов (в секундах)
	bool useHW = false, ctrlFrequency = false, rightHandedCS = false;
};

struct SlotInitParams {int max; bool is3d;};
#define SG(name, max, is3d, ...) {max, is3d},
static const SlotInitParams defaultSlotsInitParams[SG_COUNT] = {S3_SOUND_GROUPS};
#undef  SG

class Player
{
public:
    unsigned char data[64]; // its public, but you should not modify it - internal data
    InitParams params;

    Player();
    ~Player();

    bool Initialize(const SlotInitParams slotsIP[] = defaultSlotsInitParams, const int sgCount = SG_COUNT);
    void Shutdown(bool reinit = false /*internal use only!!! NEVER! NEVER call this method with true argument!*/ );
    void SetMasterVolume(float f);
    inline void SetSoundGroupVolume(SoundGroup sg, float volume)
    {
        SoundGroupSlots *sgs = getSoundGroups();
        if (sgs) sgs[sg].volume = volume;
    }
    void SetListenerParameters(const float pos[3], const float front[3], const float top[3], const float vel[3] = nullptr, float distFactor = 1, float rolloffFactor = 1, float dopplerFactor = 1);

    SoundGroupSlots *getSoundGroups(); // can return nullptr

};

void Update(); // call it at least once per 5 second - it will cleanup autodeleted sources

typedef BOOL __stdcall device_enum_callback(DEVICE *lpGuid, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext);

void enum_sound_play_devices(device_enum_callback *lpDSEnumCallback, LPVOID lpContext);

}

#include "capture.h"
