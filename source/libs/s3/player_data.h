

struct player_data_s
{
    LPDIRECTSOUND8 pDS = nullptr;
    HWND hwnd = nullptr;
    LPDIRECTSOUND3DLISTENER8 dsListener = nullptr;
    LPDIRECTSOUNDBUFFER dsPrimaryBuffer = nullptr;
    HANDLE hQuitEvent = nullptr, hUpdateThread = nullptr;
    CRITICAL_SECTION critSect;
    int sgCount = 0;
    SoundGroupSlots *soundGroups = nullptr;
    Decoder *gdecoder = nullptr;

    player_data_s()
    {
        InitializeCriticalSectionAndSpinCount(&critSect, 5000);
    }
    ~player_data_s()
    {
        DeleteCriticalSection(&critSect);
    }
};

static_assert(sizeof(player_data_s) <= (sizeof(Player) - sizeof(InitParams)), "check data");

class AutoCriticalSection
{
    LPCRITICAL_SECTION cs;

public:
    AutoCriticalSection(CRITICAL_SECTION &cs_) : cs(&cs_) { EnterCriticalSection(cs); }
    ~AutoCriticalSection() { LeaveCriticalSection(cs); }
};
