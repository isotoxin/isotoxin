

bool player_is_coredata_initialized(const char *coredata);

struct player_data_s
{
    spinlock::long3264 sync = 0;

    char coredata[PLAYER_COREDATA_SIZE] = {};

    int sgCount = 0;
    SoundGroupSlots *soundGroups = nullptr;
    Decoder *gdecoder = nullptr;

    player_data_s( const player_data_s & ) = delete;
    player_data_s( player_data_s && ) = delete;

    player_data_s()
    {
    }
    ~player_data_s()
    {
        _ASSERT(sync == 0);
    }

    bool update(Player *player, float dt); // called from update thread

    bool is_coredata_initialized() const
    {
        return player_is_coredata_initialized(coredata);
    }

    static player_data_s & cvt(Player *pl)
    {
        return *(player_data_s *)&pl->data;
    }

};

static_assert(sizeof(player_data_s) <= Player::player_data_size, "check data");

