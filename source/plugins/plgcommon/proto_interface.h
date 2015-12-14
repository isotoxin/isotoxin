#pragma once

#include "common_types.h"

#pragma pack(push)
#pragma pack(1)

struct audio_format_s
{
    int sample_rate = 48000;
    short channels = 1;
    short bits = 16;

    audio_format_s() {}
    audio_format_s(int sr, short ch, short b):sample_rate(sr), channels(ch), bits(b) {}

    bool operator!=(const audio_format_s&f) const
    {
        return channels != f.channels || bits != f.bits || sample_rate != f.sample_rate;
    }

    int blockAlign() const { return channels * (bits / 8); }
    int avgBytesPerSec() const { return blockAlign() * sample_rate; }
    int avgBytesPerMSecs(int ms) const { return blockAlign() * sample_rate * ms / 1000; }

};

struct video_format_s
{
    unsigned short width, height; // images dimensions
    unsigned short pitch[3]; // distance between lines in bytes (3 planes: Y, U, V)
    unsigned short fmt = VFMT_NONE; // image data format, see video_fmt_e
};

struct media_data_s
{
    audio_format_s afmt;
    video_format_s vfmt;
    const void *audio_frame = nullptr;
    int audio_framesize = 0;

    const void *video_frame[3]; // 3 planes
    media_data_s()
    {
        video_frame[0] = nullptr;
        video_frame[1] = nullptr;
        video_frame[2] = nullptr;
    }
};

struct proto_info_s
{
    char *protocol_name = nullptr;
    int   protocol_name_buflen = 0;

    char *description = nullptr;
    int   description_buflen = 0;

    char *description_with_tags = nullptr;
    int   description_with_tags_buflen = 0;

    char *version = nullptr;
    int   version_buflen = 0;

    int   priority = 0; // bigger -> higher (automatic select default subcontact of metacontact)
    int   features = 0;
    int   connection_features = 0;

    audio_format_s audio_fmt; // required audio format for proto plugin (app will convert audio on-the-fly if hardware not support)

    void *icon = nullptr;
    int   icon_buflen = 0;
};

struct contact_data_s
{
    /*
        Base contact identifier.
        MAIN RULE: id is not index! Delete contact must not shift values!
        0 (zero) value reserved for self
        Positive (>0) values - ids of contacts
        Negative (<0) values - ids of group chats

        id - stable identifier - must never be changed during contact lifetime
        unused values (eg: deleted contacts or group chats) can be used with new ones
    */
    int id;

    int mask; // valid fields
    const char *public_id;
    int public_id_len;
    const char *name; // utf8
    int name_len;
    const char *status_message; // utf8
    int status_message_len;
    const char *details; // utf8 // json
    int details_len;

    int avatar_tag;

    int *members; // cid's
    int members_count;

    int groupchat_permissions;

    contact_state_e state;
    contact_online_state_e ostate;
    contact_gender_e gender;

#ifdef __cplusplus
    contact_data_s(int id, int mask):id(id), mask(mask)
    {
        state = CS_ROTTEN;
        ostate = COS_ONLINE;
        gender = CSEX_UNKNOWN;
        groupchat_permissions = 0;
    }
#endif
};

#if defined _WIN32
typedef unsigned __int64 u64;
#elif defined _LINUX
typedef unsigned long long u64;
#else
#error "Sorry, can't compile: unknown target system"
#endif

struct message_s
{
    u64             utag;           // unique tag
    u64             crtime;         // message creation time_t
    const char *    message;
    int             message_len;
    message_type_e  mt;
};

struct call_info_s
{
    enum { VOICE_CALL, VIDEO_CALL } call_type = VOICE_CALL;
    int duration = 60; // seconds

    // audio data has always format as it returned from get_info in proto_info_s struct
    const void *audio_data = nullptr;
    int audio_data_size = 0;

    // video data
    const void *video_data = nullptr;
    int w = 0;
    int h = 0;
    video_fmt_e fmt = VFMT_NONE;
};

struct stream_options_s
{
    int options = 0;
    int view_w = 0, view_h = 0; // valid only with SO_RECEIVING_VIDEO // 0,0 means any size
};

struct file_send_info_s
{
    u64             utag;
    u64             filesize;
    const char *    filename;
    int             filename_len;
};

struct file_portion_s
{
    u64 offset;
    const void *data;
    int size;
};

enum long_operation_e
{
    LOP_ADDCONTACT,
    LOP_SETCONFIG,
};

enum send_av_ret_e
{
    SEND_AV_OK,
    SEND_AV_KEEP_VIDEO_DATA, // dont foget to call free_video_data to reuse video data buffer
};

struct host_functions_s // plugin can (or must) call these functions to do its job
{
    /*
        async answer
    */
    void(__stdcall *operation_result)(long_operation_e op, int rslt);
    
    /*
        main create/update contact function
    */
    void(__stdcall *update_contact)(const contact_data_s *);

    /*
        Incoming message!
        mt  - see enum for possible values
        gid - negative (groupchat id) when groupchat message received, 0 - normal message
        cid - unique contact id (known and unknown contacts), see contact_data_s::id
        sendtime - message send time (sender should send it)
        msgbody_utf8, mlen - message itself
    */
    void(__stdcall *message)(message_type_e mt, int gid, int cid, u64 sendtime, const char *msgbody_utf8, int mlen);

    /*
        plugin MUST call this function to notify Isotoxin that message was delivered to other peer
    */
    void(__stdcall *delivered)(u64 utag);

    /*
        plugin should call this function, to force save
    */
    void(__stdcall *save)(); // initiate save

    /*
        plugin should call this function only from call of save_config
    */
    void(__stdcall *on_save)(const void *data, int dlen, void *param);

    /*
        plugin should use this func to play audio or show video
        audio/video player will be automatically allocated for gid/cid client

        gid - negative (groupchat id) when groupchat message received, 0 - normal message
        cid - unique contact id (known and unknown contacts), see contact_data_s::id

        audio system automatically allocates unique audio channel for gid-cid pair
        and releases it when the audio buffer empties
    */
    void(__stdcall *av_data)(int gid, int cid, const media_data_s *data);

    /*
        call to free video data, passed to send_av
        if send_av return SEND_AV_KEEP_VIDEO_DATA
    */
    void(__stdcall *free_video_data)(const void *ptr);

    /*
        stream options from peer
    */
    void(__stdcall *av_stream_options)(int gid, int cid, const stream_options_s *so);

    /*
        plugin tells to Isotoxin its current values
        see known configurable fields (eg CFGF_PROXY_TYPE)
    */
    void(__stdcall *configurable)(int n, const char **fields, const char **values);

    /*
        plugin sends avatar data on request avatar (get_avatar)
        tag - avatar version. every client have its own avatar tag
        plugin should increment tag value every time avatar changed
        plugin should store avatar tag into save
    */
    void(__stdcall *avatar_data)(int cid, int tag, const void *avatar_body, int avatar_body_size);

    /*
        plugin notifies Isotoxin about incoming file
        cid - contact_data_s::id - unique client id
        utag - unique 64-bit file transfer identifier. Plugin can simply generate 64-bit random value
    */
    void(__stdcall *incoming_file)(int cid, u64 utag, u64 filesize, const char *filename_utf8, int filenamelen);

    /* 
        there are two cases:
        1. plugin has received file portion and now forward it to Isotoxin: all params are valid
        2. plugin requests next file portion while it send file: portion == nullptr && portion_size > 0
    */
    void(__stdcall *file_portion)(u64 utag, u64 offset, const void *portion, int portion_size);
    
    /*
        control file transfer
    */
    void(__stdcall *file_control)(u64 utag, file_control_e fctl);

    /*
        plugin should call this function 1 per second for every typing client
    */
    void(__stdcall *typing)(int cid);
};

/*
    from-isotoxin/host-calls
*/

#define PROTO_FUNCTIONS \
    FUNC1( void, tick,           int * ) \
    FUNC0( void, goodbye ) \
    FUNC1( void, set_name,       const char* ) \
    FUNC1( void, set_statusmsg,  const char* ) \
    FUNC2( void, set_config,     const void*, int ) \
    FUNC2( void, set_avatar,     const void*, int ) \
    FUNC1( void, set_ostate,     int ) \
    FUNC1( void, set_gender,     int ) \
    FUNC0( void, init_done ) \
    FUNC0( void, online ) \
    FUNC0( void, offline ) \
    FUNC1( void, save_config,    void * ) \
    FUNC2( int,  add_contact,    const char*, const char* ) \
    FUNC2( int,  resend_request, int, const char* ) \
    FUNC1( void, del_contact,    int ) \
    FUNC2( void, send_message,   int, const message_s * ) \
    FUNC1( void, del_message,    u64 ) \
    FUNC1( void, accept,         int ) \
    FUNC1( void, reject,         int ) \
    FUNC2( void, call,           int, const call_info_s * ) \
    FUNC1( void, stop_call,      int ) \
    FUNC1( void, accept_call,    int ) \
    FUNC2( int,  send_av,        int, const call_info_s * ) \
    FUNC2( void, stream_options, int, const stream_options_s * ) \
    FUNC2( void, configurable,   const char *, const char *) \
    FUNC2( void, file_send,      int, const file_send_info_s *) \
    FUNC2( void, file_resume,    u64, u64) \
    FUNC2( void, file_control,   u64, file_control_e) \
    FUNC2( void, file_portion,   u64, const file_portion_s *) \
    FUNC1( void, get_avatar,     int ) \
    FUNC2( void, add_groupchat,  const char *, bool ) \
    FUNC2( void, ren_groupchat,  int, const char * ) \
    FUNC2( void, join_groupchat, int, int ) \
    FUNC1( void, typing,         int ) \
    

struct proto_functions_s
{
#define FUNC0( rt, fn ) rt (__stdcall * fn)();
#define FUNC1( rt, fn, p0 ) rt (__stdcall * fn)(p0);
#define FUNC2( rt, fn, p0, p1 ) rt (__stdcall * fn)(p0, p1);
PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0
};
#pragma pack(pop)


typedef void(__stdcall *get_info_pf)(proto_info_s *);
typedef proto_functions_s * ( __stdcall *handshake_pf)( host_functions_s* );


/* initialization scenario:
 1. library loaded by host
 2. handshake(...) called by host - library should init all necessary internal structures to work with protocol
 3. set_config(...) called by host. it can be zero size - it means new default settings
 4. set_name(...) and set_statusmsg(...) called by host - not need call update_contact
 5. init_done() called.  ACHTUNG! at this point library MUST call update_contact with 0 (zero) id - it means self user. also other contacts (apparently loaded) must be provided

 Plugin must call operation_result(LOP_SETCONFIG, ERROR) to indicate initialization fail. If CR_OK, no need to call.
 
 
 x. idle work.....
*/