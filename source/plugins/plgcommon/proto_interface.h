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

struct proto_info_s
{
    char *protocol_name = nullptr;
    int   protocol_name_buflen = 0;

    char *description = nullptr;
    int   description_buflen = 0;
    int   max_avatar_size; // 0 - avatars not supported
    int   priority = 0; // bigger -> higher (automatic select default subcontact of metacontact)
    int   features = 0;
    int   proxy_support = 0;
    int   max_friend_request_bytes;

    audio_format_s audio_fmt; // required audio format for proto plugin (app will convert audio on-the-fly if hardware not support)
};

struct contact_data_s
{
    int mask = -1; // valid fields
    int id; // MAIN RULE: id is not index! Delete contact must not shift values! AND!!! 0 (zero) value reserved for self!
    const char *public_id;
    int public_id_len;
    const char *name; // utf8
    int name_len;
    const char *status_message; // utf8
    int status_message_len;

    int avatar_tag;

    unsigned state : contact_state_bits;
    unsigned ostate : contact_online_state_bits;
    unsigned gender : contact_gender_bits;

    contact_data_s()
    {
        state = CS_ROTTEN;
        ostate = COS_ONLINE;
        gender = CSEX_UNKNOWN;
    }
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
};

struct host_functions_s
{
    void(__stdcall *operation_result)(long_operation_e op, int rslt);
    void(__stdcall *update_contact)(const contact_data_s *);
    void(__stdcall *message)(message_type_e mt, int cid, u64 sendtime, const char *msgbody_utf8, int mlen);
    void(__stdcall *delivered)(u64 utag);
    void(__stdcall *save)(); // initiate save
    void(__stdcall *on_save)(const void *data, int dlen, void *param);
    void(__stdcall *play_audio)(int cid, const audio_format_s *audio_format, const void *frame, int framesize); // plugin can request play any format
    void(__stdcall *close_audio)(int cid); // close audio player, allocated for client
    void(__stdcall *proxy_settings)(int proxy_type, const char *proxy_address);
    void(__stdcall *avatar_data)(int cid, int tag, const void *avatar_body, int avatar_body_size);
    void(__stdcall *incoming_file)(int cid, u64 utag, u64 filesize, const char *filename_utf8, int filenamelen);

    /* 
        1. data received (all params are valid)
        2. data request (portion == nullptr && portion_size > 0)
    */
    void(__stdcall *file_portion)(u64 utag, u64 offset, const void *portion, int portion_size);
    void(__stdcall *file_control)(u64 utag, file_control_e fctl);
};

#define PROTO_FUNCTIONS \
    FUNC0( void, tick ) \
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
    FUNC2( void, send,           int, const message_s * ) \
    FUNC1( void, accept,         int ) \
    FUNC1( void, reject,         int ) \
    FUNC2( void, call,           int, const call_info_s * ) \
    FUNC2( void, stop_call,      int, stop_call_e ) \
    FUNC1( void, accept_call,    int ) \
    FUNC2( void, send_audio,     int, const call_info_s * ) \
    FUNC2( void, proxy_settings, int, const char *) \
    FUNC2( void, file_send,      int, const file_send_info_s *) \
    FUNC2( void, file_control,   u64, file_control_e) \
    FUNC2( void, file_portion,   u64, const file_portion_s *) \
    FUNC1( void, get_avatar,     int ) \
    

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
 
 
 
 x. idle work.....
*/