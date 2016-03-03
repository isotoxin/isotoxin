

struct player_data_s
{
    RWLOCK sync = 0;
    int sgCount = 0;
    LPDIRECTSOUND8 pDS = nullptr;
    HWND hwnd = nullptr;
    LPDIRECTSOUND3DLISTENER8 dsListener = nullptr;
    LPDIRECTSOUNDBUFFER dsPrimaryBuffer = nullptr;
    HANDLE hQuitEvent = nullptr, hUpdateThread = nullptr;
    SoundGroupSlots *soundGroups = nullptr;
    Decoder *gdecoder = nullptr;

    player_data_s()
    {
    }
    ~player_data_s()
    {
    }
};

static_assert(sizeof(player_data_s) <= (sizeof(Player) - sizeof(InitParams)), "check data");

