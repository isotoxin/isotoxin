

struct player_data_s
{
    spinlock::long3264 sync1 = 0;
    int sgCount = 0;
    LPDIRECTSOUND8 pDS = nullptr;
    HWND hwnd = nullptr;
    LPDIRECTSOUND3DLISTENER8 dsListener = nullptr;
    LPDIRECTSOUNDBUFFER dsPrimaryBuffer = nullptr;
    HANDLE hQuitEvent = nullptr, hUpdateThread = nullptr;
    SoundGroupSlots *soundGroups = nullptr;
    Decoder *gdecoder = nullptr;

    player_data_s( const player_data_s & ) = delete;
    player_data_s( player_data_s && ) = delete;

    player_data_s()
    {
    }
    ~player_data_s()
    {
    }

    void operator=( player_data_s && pd )
    {
        if (hQuitEvent)
            SetEvent( hQuitEvent );
        if (pd.hQuitEvent)
            SetEvent( pd.hQuitEvent );

        if (hUpdateThread)
        {
            WaitForSingleObject( hUpdateThread, INFINITE );
            CloseHandle( hUpdateThread );
            hUpdateThread = nullptr;
            ResetEvent( hQuitEvent );
        }
        if (pd.hUpdateThread)
        {
            WaitForSingleObject( pd.hUpdateThread, INFINITE );
            CloseHandle( pd.hUpdateThread );
            pd.hUpdateThread = nullptr;
            ResetEvent( pd.hQuitEvent );
        }

        char tmp[sizeof( player_data_s )];
        memcpy(tmp, this, sizeof( player_data_s ));
        memcpy( this, &pd, sizeof( player_data_s ) );
        memcpy( &pd, tmp, sizeof( player_data_s ) );
    }
};

static_assert(sizeof(player_data_s) <= Player::player_data_size, "check data");

