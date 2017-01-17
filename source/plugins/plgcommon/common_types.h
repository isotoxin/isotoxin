#pragma once

#include "boost/boost_some.h"

#define PLUGIN_INTERFACE_VER 1

#define HOME_SITE "http://isotoxin.im"

#if defined(_MSC_VER)
typedef unsigned __int64 u64;
#elif defined(__GNUC__)
typedef unsigned long long u64;
#else
#error "Sorry, can't compile: unknown target system"
#endif

enum proto_features_e
{
    PF_AVATARS                  = (1 << 0),     // plugin support avatars
    PF_AUTH_NICKNAME            = (1 << 1),     // protocol support nickname in authorization request
    PF_UNAUTHORIZED_CHAT        = (1 << 2),     // allow chat with non-friend
    PF_UNAUTHORIZED_CONTACT     = (1 << 3),     // allow add unauthorized contact
    PF_AUDIO_CALLS              = (1 << 4),     // audio calls supported
    PF_VIDEO_CALLS              = (1 << 5),     // video calls supported
    PF_SEND_FILE                = (1 << 6),     // send (and recv, of course) files supported
    PF_PAUSE_FILE               = (1 << 7),     // pause file transferring supported
    PF_RESUME_FILE              = (1 << 8),     // resume file transferring supported
    PF_CONFERENCE_SEND_FILE     = (1 << 9),     // send (and recv, of course) files to/from conference supported
    PF_CONFERENCE               = (1 << 10),    // conference supported
    PF_CONFERENCE_ENTER_LEAVE   = (1 << 11),    // allow enter/leave conferences
    PF_PURE_NEW                 = (1 << 12),    // protocol support pure new
    PF_NEW_REQUIRES_LOGIN       = (1 << 13),    // user will be prompted for pubid and password on new connections
    PF_ALLOW_REGISTER           = (1 << 14),    // protocol support login/pass
    PF_IMPORT                   = (1 << 15),    // protocol support creation with import; configuration import (set_config will receive imported file as is)
    PF_OFFLINE_MESSAGING        = (1 << 16),    // protocol supports real offline messaging (not faux offline messaging)
    
    
    PF_EXPORT                   = (1 << 31),    // protocol supports export
};

enum info_string_e // hard order
{
    IS_PROTO_TAG,
    IS_PROTO_DESCRIPTION,
    IS_PROTO_DESCRIPTION_WITH_TAG,
    IS_PROTO_VERSION,
    IS_PROTO_URL,
    IS_PROTO_AUTHOR,

    IS_PROTO_ICON,
    
    IS_AUDIO_FMT, // required audio format for proto plugin (app will convert audio on-the-fly if hardware not support)
    IS_AUDIO_CODECS, // unused
    IS_VIDEO_CODECS,

    IS_IDNAME,

    IS_AVATAR_RESTRICTIOS,

    IS_CONFERENCE_OPTIONS,

    IS_IMPORT_PATH,
    IS_EXPORT_FILE,

    IS_LAST, // always set to nullptr
    _is_count_
};

#define CONN_OPTIONS \
    COPDEF( ipv6_enable, 1 ) \
    COPDEF( udp_enable, 1 ) \
    COPDEF( hole_punch, 1 ) \
    COPDEF( local_discovery, 1 ) \
    COPDEF( enc_only, 1 ) \
    COPDEF( trust_only, 1 ) \

enum auto_conn_options_e
{
#define COPDEF( n, dv ) auto_co_##n,
    CONN_OPTIONS
#undef COPDEF
    auto_co_count
};


enum connection_features_e
{
    CF_PROXY_SUPPORT_HTTP   = 1,
    CF_PROXY_SUPPORT_HTTPS  = 2,
    CF_PROXY_SUPPORT_SOCKS4 = 4,
    CF_PROXY_SUPPORT_SOCKS5 = 8,
    CF_SERVER_OPTION        = 16,
    
#define COPDEF( n, dv ) CF_##n = 65536 << auto_co_##n,
    CONN_OPTIONS
#undef COPDEF

    CF_PROXY_MASK = CF_PROXY_SUPPORT_HTTP | CF_PROXY_SUPPORT_HTTPS | CF_PROXY_SUPPORT_SOCKS4 | CF_PROXY_SUPPORT_SOCKS5,
};


enum cd_mask_e
{
    // no CDM_ID because id always valid
    CDM_PUBID           = 1 << 0,
    CDM_NAME            = 1 << 1,
    CDM_STATUSMSG       = 1 << 2,
    CDM_STATE           = 1 << 3,
    CDM_ONLINE_STATE    = 1 << 4,
    CDM_GENDER          = 1 << 5,
    CDM_AVATAR_TAG      = 1 << 6,
    CDM_MEMBERS         = 1 << 7,
    CDM_PERMISSIONS     = 1 << 8,
    CDM_DETAILS         = 1 << 9,
    CDM_DATA            = 1 << 10,
};

enum conference_permission_e
{
    CP_CHANGE_NAME     = 1 << 0,
    CP_DELETE_GCHAT    = 1 << 1,
    CP_ADD_CONTACT     = 1 << 2,
    CP_KICK_CONTACT    = 1 << 3,
};

enum connection_bits_e
{
    CB_ENCRYPTED = 1 << 0,
    CB_TRUSTED = 1 << 1,
    CP_UDP_USED = 1 << 2,
    CP_TCP_USED = 1 << 3,
    CP_IPv6_USED = 1 << 4,
};

enum cmd_result_e
{
    CR_OK,
    CR_UNKNOWN_ERROR,
    CR_OPERATION_IN_PROGRESS,
    CR_MODULE_NOT_FOUND,
    CR_MODULE_VERSION_MISMATCH,
    CR_FUNCTION_NOT_FOUND,
    CR_INVALID_PUB_ID,
    CR_ALREADY_PRESENT,
    CR_MULTIPLE_CALL,
    CR_MEMORY_ERROR,
    CR_TIMEOUT,
    CR_NETWORK_ERROR,
    CR_CORRUPT,
    CR_ENCRYPTED,
    CR_AUTHENTICATIONFAILED,
    CR_UNTRUSTED_CONNECTION,
    CR_NOT_ENCRYPTED_CONNECTION,
    CR_SERVER_CLOSED_CONNECTION,
};

enum request_entity_e
{
    RE_DETAILS,
    RE_AVATAR,
    RE_EXPORT_DATA,
};

enum app_signal_e
{
    APPS_INIT_DONE,
    APPS_ONLINE,
    APPS_OFFLINE,
    APPS_GOODBYE,
};

enum file_control_e
{
    FIC_NONE,
    FIC_ACCEPT,
    FIC_REJECT,
    FIC_BREAK,
    FIC_DONE,
    FIC_PAUSE,
    FIC_UNPAUSE,
    FIC_DISCONNECT, // file transfer broken due disconnect. It will be resumed asap
    FIC_CHECK,
    FIC_UNKNOWN,
};

enum message_type_e : unsigned // hard order
{
    MT_MESSAGE,
    MT_SYSTEM_MESSAGE,
    MT_FRIEND_REQUEST,
    MT_INCOMING_CALL,
    MT_CALL_STOP,
    MT_CALL_ACCEPTED,

    message_type_check,
    message_type_bits = 1 + (::boost::static_log2<(message_type_check - 1)>::value)

};

struct contact_id_s
{
    unsigned id : 24;
    unsigned type : 2;

    // addition flags, NOT state of contact! used only during transferring A <-> H
    unsigned unknown : 1;
    unsigned confmember : 1;
    unsigned sysuser : 1;
    unsigned allowinvite : 1; // valid if unknown
    unsigned audio : 1; // valid for conference type

    enum type_e
    {
        EMPTY,
        CONTACT,
        CONFERENCE,
    };

    contact_id_s():id(0), type(EMPTY), unknown(0), confmember(0), sysuser(0), allowinvite(0), audio(0) {}
    contact_id_s(type_e t, int id) :id(0x00ffffff & id), type(t), unknown(0), confmember(0), sysuser(0), allowinvite(0), audio(0) {}

    void clear() {
        id = 0; type = EMPTY; unknown = 0, confmember = 0; sysuser = 0; allowinvite = 0; audio = 0;
    }

    bool is_empty() const { return type == EMPTY; }
    bool is_self() const { return id == 0 && type == CONTACT; }
    bool is_conference() const{ return type == CONFERENCE; }
    bool is_contact() const { return type == CONTACT; }
    

    bool operator == (const contact_id_s&id_) const
    {
        return id == id_.id && type == id_.type; // ignore unknown and confmember bits
    }
    bool operator != (const contact_id_s&id_) const
    {
        return id != id_.id || type != id_.type; // ignore unknown and confmember bits
    }

    static contact_id_s make_self()
    {
        return contact_id_s( CONTACT, 0 );
    }

};

static_assert(sizeof(contact_id_s) == 4, "size!");

enum contact_state_e : unsigned // hard order
{
    CS_INVITE_SEND,     // friend invite was sent
    CS_INVITE_RECEIVE,
    CS_REJECTED,
    CS_ONLINE,
    CS_OFFLINE,
    CS_ROTTEN,
    CS_WAIT,
    CS_UNKNOWN,         // unknown (non-authorized)

    contact_state_check,
    contact_state_bits = 1 + (::boost::static_log2<(contact_state_check - 1)>::value)
};

enum contact_online_state_e : unsigned
{
    COS_ONLINE,
    COS_AWAY,
    COS_DND,

    contact_online_state_check,
    contact_online_state_bits = 1 + (::boost::static_log2<(contact_online_state_check - 1)>::value)
};

enum contact_gender_e : unsigned
{
    CSEX_UNKNOWN,
    CSEX_MALE,
    CSEX_FEMALE,

    contact_gender_count,
    contact_gender_bits = 1 + (::boost::static_log2<(contact_gender_count - 1)>::value)
};

 /*
     plugin should provide format without any conversions, as possible
     so, if decoder's output is I420, do not convert it to XRGB
 */
enum video_fmt_e
{
    VFMT_NONE, // used to indicate empty video data
    VFMT_XRGB, // following 3 pixels == 12 bytes: (0x00, 0x00, 0xff, 0x??) - red, (0x00, 0xff, 0x00, 0x??) - green, (0xff, 0x00, 0x00, 0x??) - blue
    VFMT_I420, // Y plane, U plane (4x smaller), V plane (4x smaller)
};

enum stream_options_e
{
    SO_SENDING_AUDIO = 1,
    SO_RECEIVING_AUDIO = 2,
    SO_SENDING_VIDEO = 4,
    SO_RECEIVING_VIDEO = 8,
};

enum telemetry_e
{
    TLM_AUDIO_SEND_BYTES,
    TLM_AUDIO_RECV_BYTES,
    TLM_VIDEO_SEND_BYTES,
    TLM_VIDEO_RECV_BYTES,
    TLM_FILE_SEND_BYTES,
    TLM_FILE_RECV_BYTES,

    TLM_COUNT,
};

struct tlm_data_s
{
    u64 uid;
    u64 sz;
};


enum config_flags_e
{
    CFL_PARAMS = 1 << 0,
    CFL_NATIVE_DATA = 1 << 1,
};


#define FILE_TRANSFER_CHUNK 1048576
#define FILE_TRANSFER_CHUNK_MASK (~1048575ull)

#define VIDEO_FRAME_HEADER_SIZE 32

#define CFGF_SETPROTO       "setproto" // value is 1, when configuration set on set_proto result (initial call)

// known configurable fields, handled by app
// app stores values of these field in db
#define CFGF_LOGIN          "login"
#define CFGF_PASSWORD       "password"
#define CFGF_VIDEO_CODEC    "vcodec"
#define CFGF_VIDEO_BITRATE  "vbitrate"
#define CFGF_VIDEO_QUALITY  "vquality"

// known configurable fields, handled by protocol
// app will wait values of these fields from protocol
#define CFGF_PROXY_TYPE     "proxy_type"
#define CFGF_PROXY_ADDR     "proxy_addr"
#define CFGF_SERVER_PORT    "server_port"
// other options see CONN_OPTIONS

// conference options
#define CONFA_OPT_TYPE      "type"

// contact details
#define CDET_PUBLIC_UNIQUE_ID "uniqid"
#define CDET_PUBLIC_ID      "pubid"     // id, login, uin, etc...
#define CDET_PUBLIC_ID_BAD  "badpubid"  // id broken
#define CDET_EMAIL          "email"     // email
#define CDET_DNSNAME        "dnsname"   // dns name (alias for id, ToxDNS for example)
#define CDET_CLIENT         "client"    // client name and version. eg: isotoxin/0.3.393
#define CDET_CLIENT_CAPS    "clcaps"    // client caps - caps keywords. eg: 
#define CDET_CONN_INFO      "cinfo"     // connection info (ip address)

// system message fields
#define SMF_SUBJECT         "subj"
#define SMF_TEXT            "text"
#define SMF_DESCRIPTION     "desc"

// clients caps
#define CLCAP_BBCODE_B      "bbb"
#define CLCAP_BBCODE_U      "bbu"
#define CLCAP_BBCODE_S      "bbs"
#define CLCAP_BBCODE_I      "bbi"


#define DEBUG_OPT_FULL_DUMP "full_dump"
#define DEBUG_OPT_LOGGING   "logging"
#define DEBUG_OPT_TELEMETRY "tlm"
