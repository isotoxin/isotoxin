#pragma once

// AQ_* - application query
// HQ_* - plugin host query
// HA_* - answer

// typical scenario : AQ -> plghost, HA -> application

#define PLGHOST_IPC_PROTOCOL_VERSION 4

enum commands_e
{
    NOP_COMMAND,

    AQ_VERSION,
    HA_VERSION,

    AQ_GET_PROTOCOLS_LIST,
    HA_PROTOCOLS_LIST,

    AQ_SET_PROTO,
    AQ_SET_NAME,
    AQ_SET_STATUSMSG,
    AQ_SET_CONFIG,
    AQ_INIT_DONE,
    AQ_ONLINE,
    AQ_OFFLINE,

    HA_PROXY_SETTINGS,
    AQ_PROXY_SETTINGS,

    HQ_SAVE,
    AQ_SAVE_CONFIG,
    HA_CONFIG,

    AQ_ADD_CONTACT,
    AQ_DEL_CONTACT,
    HQ_UPDATE_CONTACT,

    AQ_ACCEPT,
    AQ_REJECT,

    AQ_MESSAGE,
    AQ_DEL_MESSAGE, // delete undelivered message

    HQ_MESSAGE,
    HA_DELIVERED,

    AQ_STOP_CALL,
    AQ_ACCEPT_CALL,
    AQ_CALL,
    AQ_SEND_AUDIO,
    HQ_PLAY_AUDIO,
    HQ_CLOSE_AUDIO,

    AQ_FILE_SEND,           // application initiate file send
    HQ_INCOMING_FILE,       // remote peer initiate send file
    AQ_CONTROL_FILE,        // accept / reject / break transfer
    HQ_FILE_PORTION,        // remote peer's file portion received
    HQ_QUERY_FILE_PORTION,
    AA_FILE_PORTION,        // application provides file portion

    AQ_GET_AVATAR_DATA,
    HQ_AVATAR_DATA,
    AQ_SET_AVATAR, // set self avatar

    HA_CMD_STATUS,
    
    XX_PING,
    XX_PONG,

    MAX_COMMANDS
};

static_assert( MAX_COMMANDS < 256, "255 values max" );

#pragma pack(push)
#pragma pack(1)
struct data_header_s 
{
    int cmd; // ok, ok align! 32bit! happy?
    data_header_s(commands_e c):cmd((int)c) {}
    data_header_s(const data_header_s &dh):cmd(dh.cmd) {}

    operator const void *() const {return this;}
};

struct ipcr // reader
{
    const char *d;
    int sz;
    int ptr = sizeof(data_header_s);
    ipcr(const void *dd, int sz) :d((const char *)dd), sz(sz) {}

    const data_header_s&header() const { return *(data_header_s *)d; }
    template<typename T> const T& get() { int rptr = ptr; ptr += sizeof(T); ASSERT(ptr <= sz); return *(T *)(d+rptr); }
    const void *read_data(int rsz)
    {
        int rptr = ptr;
        ptr += rsz;
        if (ASSERT(ptr <= sz))
            return d + rptr;
        return nullptr;
    }
    const void *get_data(int &rsz)
    {
        rsz = get<int>();
        return read_data(rsz);
    }

    const char *readastr(int &len)
    {
        len = get<unsigned short>();
        const char *s = d + ptr;
        ptr += len;
        ASSERT(ptr <= sz);
        return s;
    }

#ifdef STRTYPE
    STRTYPE(char) getastr() { int l = get<unsigned short>(); const char *s = d + ptr; ptr += l; ASSERT(ptr <= sz); return MAKESTRTYPE( char, s, l ); }
    STRTYPE(wchar_t) getwstr() { int l = get<unsigned short>(); const wchar_t *s = (const wchar_t *)(d + ptr); ptr += l * sizeof(wchar_t); ASSERT(ptr <= sz); return MAKESTRTYPE( wchar_t, s, l ); }
#endif
};


#pragma pack(pop)