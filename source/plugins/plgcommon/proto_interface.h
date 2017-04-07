#pragma once

#include "common_types.h"

#ifdef MODE64
#define PROTOCALL
#else
#ifdef _MSC_VER
#define PROTOCALL __stdcall
#else
#error
#endif
#endif

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

    int sampleSize() const { return channels * (bits / 8); }
    int avgBytesPerSec() const { return sampleSize() * sample_rate; }
    int avgBytesPerMSecs(int ms) const { return sampleSize() * (sample_rate * ms / 1000); }
    size_t bytesToMSec( size_t bytes ) const { return bytes * 1000 / ( sampleSize() * sample_rate ); }
    size_t samplesToMSec( size_t samples ) const { return samples * 1000 / sample_rate; }
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

    u64 msmonotonic = 0; // >0 means milliseconds from beginning of call (gui will try to syncronize audio and video)

    media_data_s()
    {
        video_frame[0] = nullptr;
        video_frame[1] = nullptr;
        video_frame[2] = nullptr;
    }
};

struct proto_info_s
{
    const char **strings = nullptr;

    int   plugin_interface_ver = PLUGIN_INTERFACE_VER;

    int   priority = 0; // bigger -> higher (automatic select default subcontact of metacontact)
    int   indicator = 0; // online indicator level
    int   features = 0;
    int   connection_features = 0;
};

struct contact_data_s
{
    const char *public_id; // set to "?" to tell to gui that pub id is unknown; empty value means not yet initialized
    const char *name; // utf8
    const char *status_message; // utf8
    const char *details; // utf8 // json
    contact_id_s *members; // cid's
    const void *data;

    contact_id_s id;

    int mask; // valid fields
    int public_id_len;
    int name_len;
    int status_message_len;
    int details_len;
    int avatar_tag;
    int members_count = 0;
    int data_size = 0;
    int conference_permissions = 0;
    int caps = 0;

    contact_state_e state = CS_ROTTEN;
    contact_online_state_e ostate = COS_ONLINE;
    contact_gender_e gender = CSEX_UNKNOWN;

    contact_data_s(contact_id_s id, int mask):id(id), mask(mask)
    {
    }
};

struct message_s
{
    u64             utag;           // unique tag
    u64             crtime;         // message creation time_t
    const char *    message;
    int             message_len;
};

struct call_prm_s
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

    u64 ms_monotonic; // monotonic milliseconds from beginning of call
};

struct stream_options_s
{
    int options = 0;
    int view_w = 0, view_h = 0; // valid only with SO_RECEIVING_VIDEO // 0,0 means any size
};

struct file_send_info_prm_s
{
    u64             utag;
    u64             filesize;
    const char *    filename;
    int             filename_len;
};

struct file_portion_prm_s
{
    u64 offset;
    const void *data;
    int size;
};

struct folder_share_prm_s
{
    const void *data;
    const char *name; // zero terminated name of share
    u64 utag; // unique 64 bit hash of name and send/recv flag
    i32 ver;
    int size; // size of data (toc)
};

struct folder_share_query_prm_s
{
    const char *tocname; // utf8
    int tocname_len;
    const char *fakename;
    int fakename_len;
};

enum long_operation_e
{
    LOP_ADDCONTACT,
    LOP_SETCONFIG,
    LOP_ONLINE,
};

enum send_av_ret_e
{
    SEND_AV_OK,
    SEND_AV_KEEP_VIDEO_DATA, // don't forget to call free_video_data to reuse video data buffer
};

struct host_functions_s // plugin can (or must) call these functions to do its job
{
    /*
        async answer
    */
    void(PROTOCALL *operation_result)(long_operation_e op, int rslt);

    /*
        connection bits
    */
    void(PROTOCALL *connection_bits)(int cbits);

    /*
        main create/update contact function
    */
    void(PROTOCALL *update_contact)(const contact_data_s *);

    /*
        Incoming message!
        mt  - see enum for possible values
        gid - conference id when conference message received, is_empty() - normal message
        cid - contact id
        sendtime - message send time (sender should send it)
        msgbody_utf8, mlen - message itself
    */
    void(PROTOCALL *message)(message_type_e mt, contact_id_s gid, contact_id_s cid, u64 sendtime, const char *msgbody_utf8, int mlen);

    /*
        plugin MUST call this function to notify Isotoxin that message was delivered to other peer
    */
    void(PROTOCALL *delivered)(u64 utag);

    /*
        plugin should call this function, to force save
    */
    void(PROTOCALL *save)(); // initiate save

    /*
        plugin should call this function only from call of save_config
    */
    void(PROTOCALL *on_save)(const void *data, int dlen, void *param);

    /*
        plugin should call this function only from call of export_data
    */
    void(PROTOCALL *export_data)(const void *data, int dlen);

    /*
        plugin should use this func to play audio or show video
        audio/video player will be automatically allocated for gid/cid client

        gid - conference id; if not conference, gid is empty
        cid - unique contact id

        audio system automatically allocates unique audio channel for gid-cid pair
        and releases it when the audio buffer empties
    */
    void(PROTOCALL *av_data)(contact_id_s gid, contact_id_s cid, const media_data_s *data);

    /*
        call to free video data, passed to send_av
        if send_av return SEND_AV_KEEP_VIDEO_DATA
    */
    void(PROTOCALL *free_video_data)(const void *ptr);

    /*
        stream options from peer

        gid - conference id; if not conference, gid is empty
        cid - unique contact id

    */
    void(PROTOCALL *av_stream_options)(contact_id_s gid, contact_id_s cid, const stream_options_s *so);

    /*
        plugin tells to Isotoxin its current config values
        see known configurable fields (eg CFGF_PROXY_TYPE)
        see IS_ADVANCED_SETTINGS
    */
    void(PROTOCALL *configurable)(int n, const char **fields, const char **values);

    /*
        plugin sends avatar data on request avatar (get_avatar)
        tag - avatar version. every client have its own avatar tag
        plugin should increment tag value every time avatar changed
        plugin should store avatar tag into save
    */
    void(PROTOCALL *avatar_data)(contact_id_s cid, int tag, const void *avatar_body, int avatar_body_size);

    /*
        plugin notifies Isotoxin about incoming file
        cid - unique contact id
        utag - unique 64-bit file transfer identifier. Plugin can simply generate 64-bit random value
    */
    void(PROTOCALL *incoming_file)(contact_id_s cid, u64 utag, u64 filesize, const char *filename_utf8, int filenamelen);

    /* 
        there are three cases:
        1. plugin has received file portion and now forward it to Isotoxin: all params are valid
        2. plugin requests next file portion while it send file: portion == nullptr && offset multiplies by 1M  && portion_size == 1048576 (1M) even last piece of file is less then 1M
        3. plugin releases requested buffer. offset - no matter, portion is pointer to buffer, portion_size == 0
    */
    bool(PROTOCALL *file_portion)(u64 utag, u64 offset, const void *portion, int portion_size);
    
    /*
        control file transfer
    */
    void(PROTOCALL *file_control)(u64 utag, file_control_e fctl);


    /*
        plugin sends to Isotoxin toc of folder share
        utag - unique tag
        toc - share content
    */
    void(PROTOCALL *folder_share)(u64 utag, const void *toc, int tocsize);

    void(PROTOCALL *folder_share_ctl)(u64 utag, folder_share_control_e ctl);

    void(PROTOCALL *folder_share_query)(u64 utag, const char *tocname, int tocname_len, const char *fakename, int fakename_len);

    /*
        plugin should call this function 1 per second for every typing client

        gid - conference id; if not conference, gid is empty
        cid - unique contact id

    */
    void(PROTOCALL *typing)(contact_id_s gid, contact_id_s cid);

    /*
        plugin can call this func to inform gui
        see DEBUG_OPT_TELEMETRY
    */
    void( PROTOCALL *telemetry )( telemetry_e k, const void *data, int datasize );

    //return id of contact not yet used
    //also mark found id used
    int (PROTOCALL *find_free_id)();

    //mark id used
    void (PROTOCALL *use_id)(int);

    void (PROTOCALL *get_file)(const char *fn, int fn_len, int tag);
    
};

/*
    from-isotoxin/host-calls
*/

#define PROTO_FUNCTIONS \
    FUNC1( void, tick,              int * ) \
    FUNC1( void, set_name,          const char* ) \
    FUNC1( void, set_statusmsg,     const char* ) \
    FUNC2( void, set_config,        const void*, int ) \
    FUNC2( void, set_avatar,        const void*, int ) \
    FUNC1( void, set_ostate,        int ) \
    FUNC1( void, set_gender,        int ) \
    FUNC2( void, signal,            contact_id_s, signal_e ) \
    FUNC1( void, save_config,       void * ) \
    FUNC1( void, contact,           const contact_data_s * ) \
    FUNC2( int,  add_contact,       const char*, const char* ) \
    FUNC2( int,  resend_request,    contact_id_s, const char* ) \
    FUNC2( void, send_message,      contact_id_s, const message_s * ) \
    FUNC1( void, del_message,       u64 ) \
    FUNC2( void, call,              contact_id_s, const call_prm_s * ) \
    FUNC2( int,  send_av,           contact_id_s, const call_prm_s * ) \
    FUNC2( void, stream_options,    contact_id_s, const stream_options_s * ) \
    FUNC2( void, file_send,         contact_id_s, const file_send_info_prm_s *) \
    FUNC2( void, file_accept,       u64, u64) \
    FUNC2( void, file_control,      u64, file_control_e) \
    FUNC2( bool, file_portion,      u64, const file_portion_prm_s *) \
    FUNC2( void, folder_share_toc,  contact_id_s, const folder_share_prm_s *) \
    FUNC2( void, folder_share_ctl,  u64, folder_share_control_e) \
    FUNC2( void, folder_share_query, u64, const folder_share_query_prm_s *) \
    FUNC2( void, create_conference, const char *, const char * ) \
    FUNC2( void, ren_conference,    contact_id_s, const char * ) \
    FUNC2( void, join_conference,   contact_id_s, contact_id_s ) \
    FUNC1( void, del_conference,    const char * ) \
    FUNC1( void, enter_conference,  const char * ) \
    FUNC2( void, leave_conference,  contact_id_s, int ) \
    FUNC1( void, logging_flags,     unsigned int ) \
    FUNC2( void, proto_file,        i32, const file_portion_prm_s *) \

struct proto_functions_s
{
#define FUNC1( rt, fn, p0 ) rt (PROTOCALL * fn)(p0);
#define FUNC2( rt, fn, p0, p1 ) rt (PROTOCALL * fn)(p0, p1);
PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
};
#pragma pack(pop)


typedef void(PROTOCALL *getinfo_pf)(proto_info_s *);
typedef proto_functions_s * ( PROTOCALL *handshake_pf)( host_functions_s* );


/* initialization scenario:
 1. library loaded by host
 2. api_handshake(...) called by host - library should init all necessary internal structures to work with protocol
 3. set_config(...) called by host. it can be zero size - it means new default settings
 4. set_name(...) and set_statusmsg(...) called by host - not need call update_contact
 5. init_done() called.  ACHTUNG! at this point library MUST call update_contact with 0 (zero) id - it means self user. also other contacts (apparently loaded) must be provided

 Plugin must call operation_result(LOP_SETCONFIG, ERROR) to indicate initialization fail. If CR_OK, no need to call.
 
 
 x. idle work.....
*/

/*
add_contact:
if second parameter is nullptr, no authorization request should be sent
*/