#include "stdafx.h"
#include "slot.h"


//#pragma comment(lib, "dxguid")
#pragma comment(lib, "dsound")
#pragma comment(lib, "winmm")

MY_GUID(MY_IID_IDirectSound3DListener, 0x279AFA84, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

namespace s3
{
#include "player_data.h"

void DefaultErrorHandler(ErrorLevel el, const char *err)
{
	OutputDebugStringA(el == EL_ERROR ? "s3 ERROR: " : "s3 warning: ");
	OutputDebugStringA(err);
	OutputDebugStringA("\n");
}
void (*ErrorHandler)(ErrorLevel, const char *) = &DefaultErrorHandler;

SoundGroupSlots::~SoundGroupSlots()
{
	delete [] slots;
}

void SoundGroupSlots::initialize(const SlotInitParams &sip)
{
	slots = new Slot [max=sip.max];
	is3d = sip.is3d;
}

DWORD WINAPI UpdateThreadProc(LPVOID pData)
{
    Player *player = (Player *)pData;
    player_data_s &pd = *(player_data_s *)&player->data;

	DWORD prevTime = timeGetTime();

	while (WaitForSingleObject(pd.hQuitEvent, 17) != WAIT_OBJECT_0)
	{
        if (spinlock::try_simple_lock( pd.sync1 ))
        {
            DWORD time = timeGetTime();
            float dt = (time - prevTime)*(1 / 1000.f);

            for (int g = 0; g < pd.sgCount; g++)
            {
                Slot *slots = pd.soundGroups[g].slots;
                for (int i = 0, n = pd.soundGroups[g].active; i < n; i++) slots[i].update( player, dt, false );
            }

            /*if (dsListener) */pd.dsListener->CommitDeferredSettings();

            prevTime = time;

            spinlock::simple_unlock( pd.sync1 );
        }

	}

	return 0;
}

Player::Player() //-V730
{
    player_data_s &pd = *(player_data_s *)&data;
    pd.player_data_s::player_data_s();

}
Player::~Player()
{
    player_data_s &pd = *(player_data_s *)&data;
    pd.~player_data_s();
}

SoundGroupSlots *Player::getSoundGroups()
{
    player_data_s &pd = *(player_data_s *)&data;
    return pd.soundGroups;
}

void Player::run_thread()
{
    player_data_s &pd = *(player_data_s *)&data;
    if (pd.hQuitEvent && !pd.hUpdateThread)
    {
        pd.hUpdateThread = CreateThread( nullptr, 0, &UpdateThreadProc, this, 0, nullptr );
        SetThreadPriority( pd.hUpdateThread, THREAD_PRIORITY_HIGHEST );
    }
}

void Player::operator=( Player &&p )
{
    player_data_s &pdmy = *(player_data_s *)&data;
    player_data_s &pdother = *(player_data_s *)&p.data;

    pdmy = std::move( pdother );
    
    run_thread();
    p.run_thread();
}

bool Player::Initialize(const SlotInitParams slotsIP[], const int sgCount_)
{
    player_data_s &pd = *(player_data_s *)&data;

	// Direct Sound initialization
    if (FAILED(DirectSoundCreate8(params.device == DEFAULT_DEVICE ? nullptr : &params.device, &pd.pDS, nullptr))) { Shutdown(); return false; }

	// create hidden window
	if (params.hwnd == nullptr)
	{
		WNDCLASSW wc = {0, DefWindowProc, 0, 0, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"s3hwndclass"};
		RegisterClassW(&wc);
		pd.hwnd = CreateWindowW(wc.lpszClassName, NULL, WS_POPUP, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
	}

	//.. для того, чтобы установить Cooperative Level
	if (FAILED(pd.pDS->SetCooperativeLevel(params.hwnd ? (HWND)params.hwnd : pd.hwnd, DSSCL_PRIORITY))) {Shutdown(); return false;}

	//Получаем listener-а (для этого приходится создать primary buffer)
	DSBUFFERDESC dsbd = {sizeof(DSBUFFERDESC), DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRL3D|DSBCAPS_CTRLVOLUME};
	if (SUCCEEDED(pd.pDS->CreateSoundBuffer(&dsbd, &pd.dsPrimaryBuffer, nullptr)))
	{
		pd.dsPrimaryBuffer->QueryInterface(MY_IID_IDirectSound3DListener, (LPVOID*)&pd.dsListener);

		//Также устанавливаем формат primary buffer-а (почему-то по умолчанию там 8бит 22КГц)
		WAVEFORMATEX wf = BuildWaveFormat(params.channels, params.sampleRate, params.bitsPerSample);
		pd.dsPrimaryBuffer->SetFormat(&wf);
	}
	else {Shutdown(); return false;}

	// init internals of s3
	if (pd.soundGroups == nullptr)
	{
		pd.sgCount = sgCount_;
		pd.soundGroups = new SoundGroupSlots[pd.sgCount];
		for (int i=0; i<pd.sgCount; i++)
			pd.soundGroups[i].initialize(slotsIP[i]);
		pd.gdecoder = new Decoder;
	}

	pd.hQuitEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    run_thread();

	return true;
}


void Player::Shutdown(bool reinit)
{
    player_data_s &pd = *(player_data_s *)&data;

	if (pd.hUpdateThread)
	{
		SetEvent(pd.hQuitEvent);
		if (reinit) spinlock::simple_unlock(pd.sync1); //Shutdown(true) вызывается в местах, обложенных AutoCriticalSection, поэтому выходим из крит. секции, иначе зависнем в дедлоке!
		WaitForSingleObject(pd.hUpdateThread, INFINITE);
		CloseHandle(pd.hQuitEvent);
		CloseHandle(pd.hUpdateThread);
		pd.hQuitEvent = pd.hUpdateThread = nullptr;
		if (reinit) spinlock::simple_lock(pd.sync1);
	}

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

	if (pd.dsListener) pd.dsListener->Release(), pd.dsListener = nullptr;
	if (pd.dsPrimaryBuffer) pd.dsPrimaryBuffer->Release(), pd.dsPrimaryBuffer = nullptr;
	if (pd.pDS) pd.pDS->Release(), pd.pDS = nullptr;
	if (pd.hwnd) DestroyWindow(pd.hwnd), pd.hwnd = nullptr;

	if (reinit) Initialize();
}

void Player::SetMasterVolume(float f)
{
    player_data_s &pd = *(player_data_s *)&data;
	if (pd.dsPrimaryBuffer) pd.dsPrimaryBuffer->SetVolume(LinearToDirectSoundVolume(f));
}

void Player::SetListenerParameters(const float pos[3], const float front[3], const float top[3], const float vel[3], float distFactor, float rolloffFactor, float dopplerFactor)
{
    player_data_s &pd = *(player_data_s *)&data;
	if (pd.dsListener)
	{
		DS3DLISTENER pars;
		pars.dwSize = sizeof(DS3DLISTENER);
		ConvertVectorToDirectSoundCS(pars.vPosition   , pos, params.rightHandedCS);
		ConvertVectorToDirectSoundCS(pars.vOrientFront, front, params.rightHandedCS);
		ConvertVectorToDirectSoundCS(pars.vOrientTop  , top, params.rightHandedCS);
		pars.flDistanceFactor = distFactor;
		pars.flRolloffFactor = rolloffFactor;
		pars.flDopplerFactor = dopplerFactor;
		if (vel) ConvertVectorToDirectSoundCS(pars.vVelocity, vel, params.rightHandedCS); else memset(&pars.vVelocity, 0, sizeof(pars.vVelocity));
		pd.dsListener->SetAllParameters(&pars, DS3D_DEFERRED);
	}
}


void enum_sound_play_devices(device_enum_callback *lpDSEnumCallback, LPVOID lpContext)
{
    typedef BOOL __stdcall device_enum_callback_win32( GUID *lpGuid, const wchar_t *lpcstrDescription, const wchar_t *lpcstrModule, void *lpContext );
    DirectSoundEnumerateW((device_enum_callback_win32 *)lpDSEnumCallback,lpContext);
}

void Update()
{
    Source::autoDeleteSources();
}

}
