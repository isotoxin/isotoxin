#include "stdafx.h"
#include "slot.h"


namespace s3
{
#include "player_data.h"

void DefaultErrorHandler(ErrorLevel el, const char *err);
void (*ErrorHandler)(ErrorLevel, const char *) = &DefaultErrorHandler;


bool player_coredata_init(Player *player);
void player_coredata_on_update(Player *player);
void player_coredata_pre_shutdown(Player *player, bool reinit);
void player_coredata_post_shutdown(Player *player);
void player_coredata_set_master_vol(Player *player, float vol);
void player_coredata_set_listner_params(Player *player, const float pos[3], const float front[3], const float top[3], const float vel[3], float distFactor, float rolloffFactor, float dopplerFactor, bool rightHandedCS);
void player_coredata_exchange(Player *coredatato, Player *coredatafrom);

SoundGroupSlots::~SoundGroupSlots()
{
	delete [] slots;
}

void SoundGroupSlots::initialize(const SlotInitParams &sip)
{
	slots = new Slot [max=sip.max];
	is3d = sip.is3d;
}

bool player_data_s::update(Player *player, float dt)
{
    if (spinlock::try_simple_lock(sync))
    {
        for (int g = 0; g < sgCount; g++)
        {
            Slot *slots = soundGroups[g].slots;
            for (int i = 0, n = soundGroups[g].active; i < n; i++) slots[i].update(player, dt);
        }

        player_coredata_on_update( player );

        spinlock::simple_unlock(sync);
        return true;
    }

    return false;
}

Player::Player() //-V730
{
    player_data_s &pd = player_data_s::cvt(this);
    pd.player_data_s::player_data_s();
}
Player::~Player()
{
    player_data_s &pd = player_data_s::cvt(this);
    pd.~player_data_s();
}

SoundGroupSlots *Player::getSoundGroups()
{
    player_data_s &pd = player_data_s::cvt(this);
    return pd.soundGroups;
}

void Player::operator=( Player &&p )
{
    player_coredata_exchange(this, &p);
}

bool Player::Initialize(const SlotInitParams slotsIP[], const int sgCount_)
{
    player_data_s &pd = player_data_s::cvt(this);

	// init internals of s3
	if (pd.soundGroups == nullptr)
	{
		pd.sgCount = sgCount_;
		pd.soundGroups = new SoundGroupSlots[pd.sgCount];
		for (int i=0; i<pd.sgCount; i++)
			pd.soundGroups[i].initialize(slotsIP[i]);
		pd.gdecoder = new Decoder;
	}

    return player_coredata_init(this);
}

void Player::Shutdown(bool reinit)
{
    player_coredata_pre_shutdown(this, reinit);

    player_data_s &pd = player_data_s::cvt(this);

	//Просто autoDeleteSources тут не достаточно, т.к. звуки могут еще играть, а необходимо удалить их в любом случае
	for (Source *s=Source::firstSourceToDelete, *next; s; s=next)
	{
		next = s->nextSourceToDelete;
		s->die();
	}
	Source::firstSourceToDelete = nullptr;

	if (!reinit)
	{
		delete [] pd.soundGroups;
		pd.soundGroups = nullptr;
		delete pd.gdecoder;
		pd.gdecoder = nullptr;
	} else
	{
		//Удаляем звуковые буферы во всех слотах
		for (int g=0; g<pd.sgCount; g++)
		{
			Slot *slots = pd.soundGroups[g].slots;
			for (int i=0, n=pd.soundGroups[g].active; i<n; i++) slots[i].clear();
		}
	}

    player_coredata_post_shutdown(this);

	if (reinit) Initialize();
}

void Player::SetMasterVolume(float f)
{
    player_coredata_set_master_vol(this,f);
}

void Player::SetPitch(float k)
{
    player_data_s &pd = player_data_s::cvt(this);
    spinlock::auto_simple_lock l(pd.sync);
    if (pd.pitch != k)
    {
        pd.pitch = k;
        pd.pitchchanged = true;
    }
}
float Player::GetPitch() const
{
    player_data_s &pd = player_data_s::cvt(const_cast<Player *>(this));
    spinlock::auto_simple_lock l(pd.sync);
    return pd.pitch;
}


void Player::SetListenerParameters(const float pos[3], const float front[3], const float top[3], const float vel[3], float distFactor, float rolloffFactor, float dopplerFactor)
{
    player_coredata_set_listner_params(this, pos, front, top, vel, distFactor, rolloffFactor, dopplerFactor, params.rightHandedCS);
}

void Update()
{
    Source::autoDeleteSources();
}

} // namespace s3
