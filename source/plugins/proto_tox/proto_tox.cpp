/*
    following marks in code:
        WORKAROUND - toxcore bug workaround
*/

#include "stdafx.h"
#include <algorithm>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4324) // 'crypto_generichash_blake2b_state' : structure was padded due to __declspec(align())
#include "sodium.h"

#include "curl/include/curl/curl.h"
#include "../../shared/shared.h"

#pragma warning(disable : 4505) // unreferenced local function has been removed
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vp8dx.h>
#include <vpx/vp8cx.h>
#include <vpx/vpx_image.h>
#pragma warning(pop)

#pragma USELIB("plgcommon")

#pragma comment(lib, "shared.lib")

#if defined _DEBUG_OPTIMIZED || defined _FINAL
#pragma comment(lib, "toxcore.lib")
#else
#pragma comment(lib, "toxcored.lib")
#endif

#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

#endif
#ifdef _NIX
#include <sodium.h>
#include <curl/curl.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vp8dx.h>
#include <vpx/vp8cx.h>
#include <vpx/vpx_image.h>
#endif

#include "../appver.inl"

#define VIDEO_CODEC_DECODER_INTERFACE_VP8 (vpx_codec_vp8_dx())
#define VIDEO_CODEC_DECODER_INTERFACE_VP9 (vpx_codec_vp9_dx())
#define VIDEO_CODEC_ENCODER_INTERFACE_VP8 (vpx_codec_vp8_cx())
#define VIDEO_CODEC_ENCODER_INTERFACE_VP9 (vpx_codec_vp9_cx())
#define MAX_ENCODE_TIME_US (1000000 / 5)

#define TOX_ENC_SAVE_MAGIC_NUMBER "toxEsave"
#define TOX_ENC_SAVE_MAGIC_LENGTH 8

#define PACKET_ID_ONLINE 24
#define PACKET_ID_OFFLINE 25
#define PACKET_ID_NICKNAME 48
#define PACKET_ID_STATUSMESSAGE 49
#define PACKET_ID_USERSTATUS 50
#define PACKET_ID_TYPING 51
#define PACKET_ID_MESSAGE 64
#define PACKET_ID_ACTION (PACKET_ID_MESSAGE + MESSAGE_ACTION) /* 65 */
#define PACKET_ID_MSI 69
#define PACKET_ID_FILE_SENDREQUEST 80
#define PACKET_ID_FILE_CONTROL 81
#define PACKET_ID_FILE_DATA 82
#define PACKET_ID_INVITE_GROUPCHAT 96
#define PACKET_ID_ONLINE_PACKET 97
#define PACKET_ID_DIRECT_GROUPCHAT 98
#define PACKET_ID_MESSAGE_GROUPCHAT 99
#define PACKET_ID_LOSSY_GROUPCHAT 199

#define PACKETID_EXTENSION 174
#define PACKETID_VIDEO_EX 175
#define SAVE_VERSION 2

#define DEFAULT_AUDIO_BITRATE 32
#define DEFAULT_VIDEO_BITRATE 5000

#define FILE_TAG_NODES 1
#define FILE_TAG_PINNED 2

#define ADVSET \
    ASI( listen_port ) \
    ASI( allow_hole_punch ) \
    ASI( allow_local_discovery ) \
    ASI( restart_on_zero_online ) \
    ASI( nodes_list_file ) \
    ASI( pinned_list_file ) \
    ASI( disable_video_ex ) \
    ASI( video_codec ) \
    ASI( video_bitrate ) \
    ASI( video_quality ) \

#define ADVSETSTRING "listen_port:int:0/allow_hole_punch:bool:1/allow_local_discovery:bool:1/restart_on_zero_online:bool:0/nodes_list_file:file/pinned_list_file:file/disable_video_ex:bool:0/video_codec:enum(vp8,vp9):0/video_bitrate:int:0/video_quality:int:0"

enum advset_e
{
#define ASI(aa) adv_##aa##_e,
    ADVSET
#undef ASI
    adv_count
};

enum chunks_e // HARD ORDER!!! DO NOT MODIFY EXIST VALUES!!!
{
    chunk_tox_data,

    chunk_descriptors,
    chunk_descriptor,
    chunk_descriptor_id,

    chunk_magic,

    chunk_descriptor_pubid, // DEPRICATED, remove 05.2017
    chunk_descriptor_state,
    chunk_descriptor_avatartag,
    chunk_descriptor_avatarhash,
    chunk_descriptor_dnsname,
    chunk_descriptor_address,
    chunk_descriptor_invmsg,

    chunk_other = 30,
    chunk_proxy_type,
    chunk_proxy_address,
    chunk_server_port,
    chunk_use_udp,
    chunk_toxid,
    chunk_use_ipv6,
    chunk_use_hole_punching,
    chunk_use_local_discovery,
    chunk_video_codec,
    chunk_video_quality,
    chunk_video_bitrate,
    chunk_restart_on_zero_online,
    chunk_disable_video_ex,
    chunk_nodes_list_fn,
    chunk_pinned_list_fn,

    chunk_nodes = 101,
    chunk_node,
    chunk_node_key,
    chunk_node_push_count,
    chunk_node_accept_count,
};

enum file_type_e
{
    FT_FILE = TOX_FILE_KIND_DATA,
    FT_AVATAR = TOX_FILE_KIND_AVATAR,

    // non tox types
    FT_TOC = 77,
};

struct fid_s
{
    enum type_e
    {
        INVALID,
        NORMAL,
        CONFA,
        UNKNOWN,

    };

    unsigned n : 24;
    unsigned type : 8;

    fid_s()
    {
        n = 0;
        type = INVALID;
    }

    static fid_s make_normal(int tox_fid)
    {
        fid_s r;
        r.n = tox_fid;
        r.type = NORMAL;
        return r;
    }
    static fid_s make_confa(int gnum)
    {
        fid_s r;
        r.n = gnum;
        r.type = CONFA;
        return r;
    }

    bool is_normal() const
    {
        return NORMAL == type;
    }

    bool is_valid() const
    {
        return INVALID != type;
    }
    
    bool is_confa() const
    {
        return CONFA == type;
    }
    bool is_unknown() const
    {
        return UNKNOWN == type;
    }


    bool operator==(const fid_s&of) const
    {
        return type == of.type && (n == of.n || type == INVALID);
    }
    bool operator!=(const fid_s&of) const
    {
        return type != of.type || (n != of.n && type != INVALID);
    }

    int normal() const
    {
        return CHECK(type == NORMAL) ? n : -1;
    }
    int confa() const
    {
        return CHECK(type == CONFA) ? n : -1;
    }
    void setup_new_unknown()
    {
        static int pool = 1;
        ASSERT(type == INVALID);
        type = UNKNOWN;
        n = pool++;
    }
};

static_assert(sizeof(fid_s) == 4, "fid size error");

namespace std
{
    template<> struct hash<contact_id_s>
    {
        size_t operator()(contact_id_s id) const
        {
            size_t x = static_cast<size_t>(id.id) | (static_cast<size_t>(id.type) << 24);
            hash<size_t> h;
            return h(x);
        }
    };
    template<> struct hash<fid_s>
    {
        size_t operator()(fid_s id) const
        {
            size_t x = static_cast<size_t>(id.n) | (static_cast<size_t>(id.type) << 24);
            hash<size_t> h;
            return h(x);
        }
    };
}

template< size_t A, size_t B, size_t C = B > struct max_t
{
    static const size_t value = A > B ? (A > C ? A : C) : (B > C ? B : C);
};

struct public_key_s
{
    byte key[TOX_PUBLIC_KEY_SIZE] = {};
    bool operator==( const public_key_s & k ) const
    {
        return !memcmp( key, k.key, TOX_PUBLIC_KEY_SIZE );
    }
    bool operator<(const public_key_s & k) const
    {
        return memcmp(key, k.key, TOX_PUBLIC_KEY_SIZE) < 0;
    }

    bytes as_bytes() const
    {
        return bytes(key, TOX_PUBLIC_KEY_SIZE);
    }

};

struct conference_id_s
{
    byte id[TOX_CONFERENCE_UID_SIZE] = {};
    bool operator==( const conference_id_s & c )
    {
        return !memcmp( id, c.id, TOX_CONFERENCE_UID_SIZE );
    }
};

class tox_address_c
{
    byte id[ max_t<TOX_PUBLIC_KEY_SIZE, TOX_CONFERENCE_UID_SIZE, TOX_ADDRESS_SIZE>::value ] = {};
    uint16_t type = TAT_EMPTY;

public:
    enum type_e
    {
        TAT_EMPTY,
        TAT_FULL_ADDRESS,
        TAT_PUBLIC_KEY,
        TAT_CONFERENCE_ID,
    };

    void clear()
    {
        memset(id, 0, sizeof(id));
        type = TAT_EMPTY;
    }

    const byte *get( type_e t ) const
    {
        switch (t)
        {
        case TAT_FULL_ADDRESS:
            if (type == TAT_FULL_ADDRESS)
                return id;
            break;
        case TAT_PUBLIC_KEY:
            if (type == TAT_FULL_ADDRESS || type == TAT_PUBLIC_KEY)
                return id;
            break;
        case TAT_CONFERENCE_ID:
            if (type == TAT_CONFERENCE_ID)
                return id;
            break;
        }
        return nullptr;
    }

    static bool type_compatible(type_e t1, type_e t2)
    {
        switch (t1)
        {
        case TAT_FULL_ADDRESS:
        case TAT_PUBLIC_KEY:
            return t2 == TAT_PUBLIC_KEY || t2 == TAT_FULL_ADDRESS;
        case TAT_CONFERENCE_ID:
            return TAT_CONFERENCE_ID == t2;
        }
        return false;
    }

    bool compatible( type_e t ) const
    {
        return type_compatible( (type_e)type, t );
    }

    static size_t compare_len( type_e t )
    {
        switch (t)
        {
        case TAT_FULL_ADDRESS:
        case TAT_PUBLIC_KEY:
            return TOX_PUBLIC_KEY_SIZE;
        case TAT_CONFERENCE_ID:
            return TOX_CONFERENCE_UID_SIZE;
        }
        return 0;
    }
    static size_t len( type_e t )
    {
        switch (t)
        {
        case TAT_FULL_ADDRESS:
            return TOX_ADDRESS_SIZE;
        case TAT_PUBLIC_KEY:
            return TOX_PUBLIC_KEY_SIZE;
        case TAT_CONFERENCE_ID:
            return TOX_CONFERENCE_UID_SIZE;
        }
        return 0;
    }

    void setup_self_address();
    void setup_public_key(fid_s fid);
    void setup( const void *id_, type_e t )
    {
        type = static_cast<uint16_t>(t);
        memcpy( id, id_, len(t) );
    }
    void setup( const std::asptr&s )
    {
        if (s.l == 64)
        {
            type = static_cast<uint16_t>(TAT_PUBLIC_KEY);
            std::pstr_c( s ).hex2buf<TOX_PUBLIC_KEY_SIZE>( id );
        }
        if (s.l == 76)
        {
            type = static_cast<uint16_t>(TAT_FULL_ADDRESS);
            std::pstr_c( s ).hex2buf<TOX_ADDRESS_SIZE>( id );
        }
    }

    void operator=( const public_key_s&a )
    {
        type = TAT_PUBLIC_KEY;
        memcpy( id, a.key, TOX_PUBLIC_KEY_SIZE );
    }
    void operator=( const conference_id_s&a )
    {
        type = TAT_CONFERENCE_ID;
        memcpy( id, a.id, TOX_CONFERENCE_UID_SIZE );
    }

    bool operator==(const tox_address_c&a) const
    {
        return type_compatible(static_cast<type_e>(type), static_cast<type_e>(a.type)) && !memcmp(id, a.id, compare_len(static_cast<type_e>(type)));
    }
    bool operator!=( const tox_address_c&a ) const
    {
        return !(*this == a);
    }

    bool operator==( const public_key_s&a ) const
    {
        return ( TAT_FULL_ADDRESS == type || TAT_PUBLIC_KEY == type ) && !memcmp( id, a.key, TOX_PUBLIC_KEY_SIZE );
    }
    bool operator==( const conference_id_s&a ) const
    {
        return (TAT_CONFERENCE_ID == type) && !memcmp( id, a.id, TOX_CONFERENCE_UID_SIZE );
    }

    const conference_id_s& as_conference_id() const
    {
        static conference_id_s dummy;
        if (TAT_CONFERENCE_ID==type)
            return *(conference_id_s *)id;
        return dummy;
    }
    const public_key_s& as_public_key() const
    {
        static public_key_s dummy;
        if (type == TAT_FULL_ADDRESS || type == TAT_PUBLIC_KEY)
            return *(public_key_s *)id;
        return dummy;
    }

    bytes as_bytes() const
    {
        return bytes(id, len(static_cast<type_e>(type)));
    }

    std::string raw_str() const
    {
        std::string s;
        s.append_as_hex( id, static_cast<int>(len( (type_e)type )) );
        return s;
    }


    template<typename S> int as_str( S &s, type_e check ) const
    {
        s.clear();
        switch (check)
        {
        case TAT_FULL_ADDRESS:
            if (type == TAT_FULL_ADDRESS)
            {
                s.append_as_hex( id, TOX_ADDRESS_SIZE );
                return s.get_length();
            }
            break;
        case TAT_PUBLIC_KEY:
            if (type == TAT_FULL_ADDRESS || type == TAT_PUBLIC_KEY)
            {
                s.append_as_hex( id, TOX_PUBLIC_KEY_SIZE );
                return s.get_length();
            }
            break;
        case TAT_CONFERENCE_ID:
            if (type == TAT_CONFERENCE_ID)
            {
                s.append_as_hex( id, TOX_CONFERENCE_UID_SIZE );
                return s.get_length();
            }
            break;
        }
        return 0;
    }
};

extern "C"
{
    const char *get_conn_info(const void *tox, const uint8_t *real_pk);
    typedef void node_f(const uint8_t *public_key);
    extern node_f *node_responce;
}

static std::string dnsquery(const std::string & query, const std::asptr& ver);

struct tox3dns_s
{
    std::string addr;
    byte key[32];
    void *dns3 = nullptr;
    bool key_ok = false;

    void operator=(tox3dns_s && oth)
    {
        addr = oth.addr;
        memcpy(key, oth.key, 32);
        key_ok = oth.key_ok;
        std::swap(dns3, oth.dns3);
    }
    tox3dns_s(tox3dns_s && oth) :addr(oth.addr)
    {
        if (oth.key_ok)
        {
            memcpy(key, oth.key, 32);
            key_ok = true;
        }
        dns3 = oth.dns3;
        oth.dns3 = nullptr;
    }

    void operator=(const tox3dns_s&) = delete;
    tox3dns_s(const tox3dns_s&) = delete;

    tox3dns_s(const std::asptr &sn, const std::asptr &k = std::asptr()) :addr(sn)
    {
        if (k.l)
        {
            std::pstr_c(k).hex2buf<32>(key);
            key_ok = true;

        }
        else
        {
            // we have to query
            std::string key_string = dnsquery(std::string(STD_ASTR("_tox."), addr), std::asptr());
            if (!key_string.is_empty())
            {
                key_string.hex2buf<32>(key);
                key_ok = true;
            }
        }
    }

    ~tox3dns_s()
    {
        if (dns3) tox_dns3_kill(dns3);
    }

    std::string query1(const std::string &pubid)
    {
        if (dns3)
        {
            tox_dns3_kill(dns3);
            dns3 = nullptr;
        }
        std::string request(pubid);
        request.replace_all(STD_ASTR("@"), STD_ASTR("._tox."));
        return dnsquery(request, STD_ASTR("tox1"));
    }

    std::string query3(const std::string &pubid)
    {
        if (!key_ok)
            return query1(pubid);

        if (dns3 == nullptr)
            dns3 = tox_dns3_new(key);

        uint32_t request_id;
        std::sstr_t<128> dns_string;
        int dns_string_len = tox_generate_dns3_string(dns3, (byte *)dns_string.str(), (uint16_t)dns_string.get_capacity(), &request_id, (byte*)pubid.cstr(), (byte)pubid.find_pos('@'));

        if (dns_string_len < 0)
            return query1(pubid);

        dns_string.set_length(dns_string_len);

        std::string rec(256, true);
        rec.set_as_char('_').append(dns_string).append(STD_ASTR("._tox.")).append(addr);
        rec = dnsquery(rec, STD_ASTR("tox3"));
        if (!rec.is_empty())
        {
            byte tox_id[TOX_ADDRESS_SIZE];
            if (tox_decrypt_dns3_TXT(dns3, tox_id, (/*const*/ byte *)rec.cstr(), (u32)rec.get_length(), request_id) < 0)
                return query1(pubid);

            rec.clear();
            rec.append_as_hex(tox_id, sizeof(tox_id));
            return rec;
        }

        return query1(pubid);
    }
};

struct tox_node_s
{
    public_key_s pubid;
    std::string addr4;
    std::string addr6;

    int push_count = 0;
    int accept_count = 0;

    uint16_t port = 0;
    bool accepted = false;

    tox_node_s(const std::asptr& addr4, const std::asptr& addr6, uint16_t port, const std::asptr& pubid_) :addr4(addr4), addr6(addr6), port(port)
    {
        std::pstr_c(pubid_).hex2buf<32>(pubid.key);
    }

    tox_node_s(const public_key_s &pk, int push_count, int accept_count):pubid(pk), push_count(push_count), accept_count(accept_count)
    {
    }

    bool operator<(const tox_node_s&n) const
    {
        return memcmp(pubid.key, n.pubid.key, TOX_PUBLIC_KEY_SIZE) < 0;
    }
    bool operator<(const public_key_s&k) const
    {
        return pubid < k;
    }
};

static void operator<<(chunk &chunkm, const tox_node_s &n)
{
    chunk mm(chunkm.b, chunk_node);

    chunk(chunkm.b, chunk_node_key) << n.pubid.as_bytes();
    chunk(chunkm.b, chunk_node_push_count) << static_cast<i32>(n.push_count);
    chunk(chunkm.b, chunk_node_accept_count) << static_cast<i32>(n.accept_count);
}

struct contact_descriptor_s;
struct av_sender_state_s
{
    // list of call-in-progress descriptors
    contact_descriptor_s *first = nullptr;
    contact_descriptor_s *last = nullptr;

    bool video_encoder = false;
    bool video_sender = false;
    bool audio_sender = false;
    bool video_encoder_heartbeat = false;
    bool video_sender_heartbeat = false;
    bool audio_sender_heartbeat = false;
    volatile bool shutdown = false;

    bool senders() const
    {
        return audio_sender || video_sender || video_encoder;
    }

    bool allow_run_audio_sender() const
    {
        return !audio_sender && !shutdown;
    }
    bool allow_run_video_encoder() const
    {
        return !video_encoder && !shutdown;
    }
    bool allow_run_video_sender() const
    {
        return !video_sender && !shutdown;
    }
};

static void audio_sender();
static void video_encoder();
static void video_sender();
static void delete_all_descs();

static uint32_t cb_isotoxin_special(Tox *, uint32_t tox_fid, uint8_t *packet, uint32_t len, uint32_t /*max_len*/);
static void cb_isotoxin(Tox *, uint32_t tox_fid, const byte *data, size_t len, void * /*object*/);
static void cb_friend_request(Tox *, const byte *id, const byte *msg, size_t length, void *);
static void cb_friend_message(Tox *, uint32_t tox_fid, TOX_MESSAGE_TYPE type, const byte *message, size_t length, void *);
static void cb_name_change(Tox *, uint32_t tox_fid, const byte * newname, size_t length, void *);
static void cb_status_message(Tox *, uint32_t tox_fid, const byte * newstatus, size_t length, void *);
static void cb_friend_status(Tox *, uint32_t tox_fid, TOX_USER_STATUS status, void *);
static void cb_read_receipt(Tox *, uint32_t tox_fid, uint32_t message_id, void *);
static void cb_friend_typing(Tox *, uint32_t tox_fid, bool is_typing, void * /*userdata*/);
static void cb_connection_status(Tox *, uint32_t tox_fid, TOX_CONNECTION connection_status, void *);
static void cb_conference_invite(Tox *, uint32_t fid, TOX_CONFERENCE_TYPE t, const uint8_t * data, size_t length, void *);
static void cb_conference_message(Tox *, uint32_t gnum, uint32_t peernum, TOX_MESSAGE_TYPE t, const uint8_t * message, size_t length, void *);
static void cb_conference_namelist_change(Tox *, uint32_t gnum, uint32_t /*pid*/, TOX_CONFERENCE_STATE_CHANGE /*change*/, void *);
static void cb_conference_title(Tox *, uint32_t gnum, uint32_t /*pid*/, const uint8_t * title, size_t length, void *);
static void cb_file_recv_control(Tox *, uint32_t tox_fid, uint32_t filenumber, TOX_FILE_CONTROL control, void * /*userdata*/);
static void cb_file_chunk_request(Tox *, uint32_t tox_fid, uint32_t file_number, u64 position, size_t length, void *);
static void cb_tox_file_recv(Tox *, uint32_t tox_fid, uint32_t filenumber, uint32_t kind, u64 filesize, const byte *filename, size_t filename_length, void *);
static void cb_tox_file_recv_chunk(Tox *, uint32_t tox_fid, uint32_t filenumber, u64 position, const byte *data, size_t length, void * /*userdata*/);
static void cb_toxav_incoming_call(ToxAV *av, uint32_t tox_fid, bool audio_enabled, bool video_enabled, void * /*user_data*/);
static void cb_toxav_call_state(ToxAV *av, uint32_t tox_fid, uint32_t state, void * /*user_data*/);
static void cb_toxav_audio(ToxAV *av, uint32_t tox_fid, const int16_t *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void * /*user_data*/);
static void cb_toxav_video(ToxAV *av, uint32_t tox_fid, uint16_t width, uint16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v, int32_t ystride, int32_t ustride, int32_t vstride, void * /*user_data*/);

class tox_c
{

#ifdef _DEBUG
    time_t fake_offline_off = 0;
#endif

    struct other_typing_s
    {
        fid_s fid;
        int time = 0;
        int totaltime = 0;
        other_typing_s(fid_s fid, int time) :fid(fid), time(time), totaltime(time) {}
    };

    static DWORD WINAPI audio_sender_thread(LPVOID)
    {
        UNSTABLE_CODE_PROLOG
            audio_sender();
        UNSTABLE_CODE_EPILOG
            return 0;
    }

    static DWORD WINAPI video_encoder_thread(LPVOID)
    {
        UNSTABLE_CODE_PROLOG
            video_encoder();
        UNSTABLE_CODE_EPILOG
            return 0;
    }

    static DWORD WINAPI video_sender_thread(LPVOID)
    {
        UNSTABLE_CODE_PROLOG
            video_sender();
        UNSTABLE_CODE_EPILOG
            return 0;
    }

    std::string nodes_fn;
    std::string pinned_fn;
    std::vector<tox_node_s> nodes;
    byte_buffer buf_tox_config; // saved on offline; PURE TOX-SAVE DATA, aka native data

public:

    struct fsh_s
    {
        fsh_s(u64 utag, contact_id_s cid, const std::asptr &n, int ver, int sz) :utag(utag), cid(cid), name(n), toc_ver(ver), toc_size(sz)
        {
        }
        ~fsh_s() {}

        void die(int index = -1);

        u64 utag;
        std::str_c name;
        contact_id_s cid;
        int toc_ver = -1;
        int toc_size = 0;
        bool recv = false;

        const void * to_data() const
        {
            return this + 1;
        }
    };

    struct fsh_ptr_s
    {
        fsh_s *sf = nullptr;

        fsh_ptr_s() {}
        ~fsh_ptr_s() { if (sf) { sf->~fsh_s(); dlfree(sf); } }
        fsh_ptr_s(fsh_ptr_s &&o)
        {
            sf = o.sf;
            o.sf = nullptr;
        }
        fsh_ptr_s(u64 utag, contact_id_s cid, const std::asptr &n, int ver, const void* data, int datasize)
        {
            sf = (fsh_s *)dlmalloc(sizeof(fsh_s) + datasize);
            sf->fsh_s::fsh_s(utag, cid, n, ver, datasize);
            memcpy(sf + 1, data, datasize);
        }
        fsh_ptr_s(u64 utag, contact_id_s cid, const std::asptr &n)
        {
            sf = (fsh_s *)dlmalloc(sizeof(fsh_s));
            sf->fsh_s::fsh_s(utag, cid, n, 0, 0);
            sf->recv = true;
        }
        void operator=(fsh_ptr_s &&o)
        {
            fsh_s *keep = sf;
            sf = o.sf;
            o.sf = keep;
        }

        void clear_toc()
        {
            if (sf->toc_size > 0)
            {
                sf = (fsh_s *)dlrealloc(sf, sizeof(fsh_s));
                sf->toc_size = 0;
            }
        }

        void update_toc(const void* data, int datasize)
        {
            if (datasize > sf->toc_size)
            {
                sf = (fsh_s *)dlrealloc(sf, sizeof(fsh_s) + datasize);
            }
            memcpy(sf + 1, data, datasize);
            sf->toc_size = datasize;
        }

    private:
        fsh_ptr_s(const fsh_ptr_s &) = delete;
        void operator=(const fsh_ptr_s &) = delete;
    };

    std::vector<fsh_ptr_s> sfs;

    fsh_s *find_fsh(u64 utag)
    {

        for (fsh_ptr_s &x : sfs)
            if (x.sf->utag == utag)
                return x.sf;

        return nullptr;
    }

    fsh_ptr_s *find_fsh(u64 utag, int &index)
    {

        for (size_t i = 0, cnt = sfs.size(); i < cnt; ++i)
        {
            fsh_ptr_s &x = sfs[i];
            if (x.sf->utag == utag)
            {
                index = static_cast<int>(i);
                return &x;
            }
        }

        index = -1;
        return nullptr;
    }


    spinlock::syncvar<av_sender_state_s> callstate;

    std::vector<other_typing_s> other_typing;
    std::vector<tox3dns_s> pinnedservs;

    tox_address_c lastmypubid; // cleared in handshake

    Tox *tox = nullptr;
    ToxAV *toxav = nullptr;
    host_functions_s *hf;

    std::sstr_t<256> tox_proxy_host;
    byte avahash[TOX_HASH_LENGTH] = {};
    byte_buffer gavatar;
    int gavatag = 0;

    contact_state_e self_state; // cleared in handshake
    contact_id_s self_typing_contact;
    int self_typing_time = 0;

    int video_bitrate = 0;
    int video_quality = 0;

    int next_check_accept_nodes_time = 0;
    int accepted_nodes = 0;
    int zero_online_connect_cnt = 0;

    i32 tox_proxy_type = 0;

    Tox_Options options;

    uint16_t tox_proxy_port = 0;

    bool restart_on_zero_online = false;
    bool disabled_video_ex = false;
    bool reconnect = false;
    bool restart_module = false;
    bool use_vp9_codec = false;
    bool proxy_settings_ok = false;
    bool online_flag = false;
    bool conf_encrypted = false;

    void set_proxy_addr(const std::asptr& addr)
    {
        std::token<char> p(addr, ':');
        tox_proxy_host.clear();
        tox_proxy_port = 0;
        if (p) tox_proxy_host = *p;
        ++p; if (p) tox_proxy_port = (uint16_t)p->as_uint();
    }


    void set_proxy_curl(CURL *curl)
    {
        if (tox_proxy_type > 0)
        {
            std::string proxya(tox_proxy_host.as_sptr());

            int pt = 0;
            if (tox_proxy_type & (CF_PROXY_SUPPORT_HTTP | CF_PROXY_SUPPORT_HTTPS)) pt = CURLPROXY_HTTP;
            else if (tox_proxy_type & CF_PROXY_SUPPORT_SOCKS4) pt = CURLPROXY_SOCKS4;
            else if (tox_proxy_type & CF_PROXY_SUPPORT_SOCKS5) pt = CURLPROXY_SOCKS5_HOSTNAME;

            curl_easy_setopt(curl, CURLOPT_PROXY, proxya.cstr());
            curl_easy_setopt(curl, CURLOPT_PROXYPORT, tox_proxy_port);
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, pt);
        }
    }


#define FUNC1( rt, fn, p0 ) rt fn(p0);
#define FUNC2( rt, fn, p0, p1 ) rt fn(p0, p1);
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1

    static void node_accept_handler(const uint8_t *public_key);

    void on_handshake(host_functions_s *hf_)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif // _WIN32

        srand(time_ms());

        hf = hf_;
        node_responce = node_accept_handler;

        self_state = CS_OFFLINE;
        lastmypubid.clear();

        memset(&options, 0, sizeof(options));
        options.udp_enabled = true;
        options.hole_punching_enabled = true;
        options.local_discovery_enabled = true;
    }


    void reset_accepted_nodes()
    {
        zero_online_connect_cnt = 0;
        accepted_nodes = 0;
        for (tox_node_s &n : nodes)
            n.accepted = false;

        next_check_accept_nodes_time = time_ms() + 60000;
    }

    void update_self()
    {
        hf->connection_bits(CB_ENCRYPTED | CB_TRUSTED);

        if (tox)
            lastmypubid.setup_self_address();

        int m = 0;

        std::string name(tox ? static_cast<int>(tox_self_get_name_size(tox)) : 0, false);
        if (tox)
        {
            tox_self_get_name(tox, (byte*)name.str());
            m |= CDM_NAME;
        }

        std::string statusmsg(tox ? static_cast<int>(tox_self_get_status_message_size(tox)) : 0, false);
        if (tox)
        {
            tox_self_get_status_message(tox, (byte*)statusmsg.str());
            m |= CDM_STATUSMSG;
        }

        contact_data_s self(contact_id_s::make_self(), CDM_PUBID | CDM_STATE | CDM_ONLINE_STATE | CDM_GENDER | CDM_AVATAR_TAG | m);

        //ADDRESS
        std::sstr_t<TOX_ADDRESS_SIZE * 2 + 16> pubid;
        self.public_id_len = lastmypubid.as_str(pubid, tox_address_c::TAT_FULL_ADDRESS);
        self.public_id = pubid.cstr();

        self.name = name.cstr();
        self.name_len = name.get_length();
        self.status_message = statusmsg.cstr();
        self.status_message_len = statusmsg.get_length();
        self.state = online_flag ? self_state : CS_OFFLINE;
        self.avatar_tag = 0;
        hf->update_contact(&self);
    }

    void restore_core_contacts();

    void add_to_callstate(contact_descriptor_s *d);

    void run_senders()
    {
        if (callstate.lock_read()().allow_run_audio_sender())
        {
            CloseHandle(CreateThread(nullptr, 0, audio_sender_thread, nullptr, 0, nullptr));
            for (; !callstate.lock_read()().audio_sender; Sleep(1)); // wait audio sender start
        }

        if (callstate.lock_read()().allow_run_video_encoder())
        {
            CloseHandle(CreateThread(nullptr, 0, video_encoder_thread, nullptr, 0, nullptr));
            for (; !callstate.lock_read()().video_encoder; Sleep(1)); // wait video encoder start
        }

        if (callstate.lock_read()().allow_run_video_sender())
        {
            CloseHandle(CreateThread(nullptr, 0, video_sender_thread, nullptr, 0, nullptr));
            for (; !callstate.lock_read()().video_sender; Sleep(1)); // wait video sender start
        }
    }

    void stop_senders()
    {
        int st = time_ms();

        auto wh = callstate.lock_write();
        wh().audio_sender_heartbeat = false;
        wh().video_sender_heartbeat = false;
        wh().video_encoder_heartbeat = false;
        wh.unlock();

        while (callstate.lock_read()().senders())
        {
            callstate.lock_write()().shutdown = true;

            Sleep(1);

            if ((time_ms() - st) > 3000)
            {
                // 3 seconds still waiting senders?
                // something wrong
                auto w = callstate.lock_write();
                if (!w().audio_sender_heartbeat)
                    w().audio_sender = false, restart_module = true;
                w().audio_sender_heartbeat = false;

                if (!w().video_sender_heartbeat)
                    w().video_sender = false, restart_module = true;
                w().video_sender_heartbeat = false;

                if (!w().video_encoder_heartbeat)
                    w().video_encoder = false, restart_module = true;
                w().video_encoder_heartbeat = false;

                st = time_ms();
            }
        }

        callstate.lock_write()().shutdown = false;

    }

    TOX_ERR_NEW prepare(bool kill_descs)
    {
        if (kill_descs)
            delete_all_descs();

        reconnect = false;
        if (tox) tox_kill(tox);

        if (options.ipv6_enabled)
        {
            bool allow_ipv6 = false;

#ifdef _WIN32 
#pragma warning( push )
#pragma warning( disable : 4996 )
            OSVERSIONINFO v = { sizeof(OSVERSIONINFO), 0 };
            GetVersionExW(&v);
#pragma warning( pop )
            if (v.dwMajorVersion >= 6)
            {
                SOCKET sock6 = socket(AF_INET6, SOCK_STREAM, 0);
                if (INVALID_SOCKET != sock6)
                {
                    allow_ipv6 = true;
                    closesocket(sock6);
                }
            }
#endif
#ifdef _NIX
            allow_ipv6 = true;
#endif
            options.ipv6_enabled = allow_ipv6 ? 1 : 0;
        }

        options.proxy_host = tox_proxy_host.cstr();
        options.proxy_port = 0;
        options.proxy_type = TOX_PROXY_TYPE_NONE;
        if (tox_proxy_type & (CF_PROXY_SUPPORT_HTTP | CF_PROXY_SUPPORT_HTTPS))
            options.proxy_type = TOX_PROXY_TYPE_HTTP;
        if (tox_proxy_type & (CF_PROXY_SUPPORT_SOCKS4 | CF_PROXY_SUPPORT_SOCKS5))
            options.proxy_type = TOX_PROXY_TYPE_SOCKS5;
        if (TOX_PROXY_TYPE_NONE != options.proxy_type)
        {
            options.proxy_port = tox_proxy_port;
            if (options.proxy_port == 0 && options.proxy_type == TOX_PROXY_TYPE_HTTP) options.proxy_port = 3128;
            if (options.proxy_port == 0 && options.proxy_type == TOX_PROXY_TYPE_SOCKS5) options.proxy_port = 1080;
            if (options.proxy_port == 0)
            {
                options.proxy_host = nullptr;
                options.proxy_type = TOX_PROXY_TYPE_NONE;
            }
        }
        options.start_port = 0;
        options.end_port = 0;

        //options.tcp_port = (uint16_t)server_port; // TODO
        //options.udp_enabled = options.proxy_type == TOX_PROXY_TYPE_NONE;

        options.savedata_type = buf_tox_config.size() ? TOX_SAVEDATA_TYPE_TOX_SAVE : TOX_SAVEDATA_TYPE_NONE;
        options.savedata_data = buf_tox_config.data();
        options.savedata_length = buf_tox_config.size();

        TOX_ERR_NEW errnew;

        options.client_capabilities = "client:isotoxin/" SS(PLUGINVER) "\n"
            "support_bbtags:b,s,u,i\n"
            "support_viewsize:1\n"
            "support_msg_chain:1\n"
            "support_msg_cr_time:1\n"
            "support_video_ex:1\n"
            "support_folder_share:1\n";

        tox = tox_new(&options, &errnew);
        if (!tox)
            return errnew;

        buf_tox_config.clear();

        tox_callback_cryptpacket_before_send(tox, cb_isotoxin_special);

        tox_callback_friend_lossless_packet(tox, cb_isotoxin);

        tox_callback_friend_request(tox, cb_friend_request);
        tox_callback_friend_message(tox, cb_friend_message);
        tox_callback_friend_name(tox, cb_name_change);
        tox_callback_friend_status_message(tox, cb_status_message);
        tox_callback_friend_status(tox, cb_friend_status);
        tox_callback_friend_typing(tox, cb_friend_typing);
        tox_callback_friend_read_receipt(tox, cb_read_receipt);
        tox_callback_friend_connection_status(tox, cb_connection_status);

        tox_callback_conference_invite(tox, cb_conference_invite);
        tox_callback_conference_message(tox, cb_conference_message);
        tox_callback_conference_namelist_change(tox, cb_conference_namelist_change);
        tox_callback_conference_title(tox, cb_conference_title);

        tox_callback_file_recv_control(tox, cb_file_recv_control);
        tox_callback_file_chunk_request(tox, cb_file_chunk_request);
        tox_callback_file_recv(tox, cb_tox_file_recv);
        tox_callback_file_recv_chunk(tox, cb_tox_file_recv_chunk);


        toxav = toxav_new(tox, nullptr);

        toxav_callback_call(toxav, cb_toxav_incoming_call, nullptr);
        toxav_callback_call_state(toxav, cb_toxav_call_state, nullptr);

        toxav_callback_audio_receive_frame(toxav, cb_toxav_audio, nullptr);
        toxav_callback_video_receive_frame(toxav, cb_toxav_video, nullptr);

        if (!kill_descs)
            restore_core_contacts();

        return TOX_ERR_NEW_OK;
    }

    tox_node_s& get_node()
    {
        tox_node_s *n2r = nullptr;
        uint32_t prevrnd = 0;

        int minpc = INT_MAX;
        for (tox_node_s &node : nodes)
        {
            if (node.push_count < minpc)
                minpc = node.push_count;
        }

        for (tox_node_s &node : nodes)
        {
            if (node.push_count != minpc)
                continue;

            uint32_t r = random64([](u64) {return false; }) & 0xffffffff;
            if (r > prevrnd)
                n2r = &node, prevrnd = r;
        }

        if (n2r == nullptr)
            n2r = &nodes[(random64([](u64) {return false; }) & 0xffffffff) % nodes.size()];

        return *n2r;

    };

    void connect()
    {
        if (!tox)
        {
            hf->operation_result(LOP_ONLINE, CR_NETWORK_ERROR);
            return;
        }


        size_t n = min(nodes.size(), 4);
        for (size_t i = 0; i < n; ++i)
        {
            tox_node_s &node = get_node();
            if (node.port == 0)
            {
                ++n;
                if (n > nodes.size() && i >= nodes.size()) break;
                continue;
            }

            ++node.push_count;
            if (!node.accepted) --node.accept_count; if (node.accept_count < 0) node.accept_count = 0;
            if (node.push_count > 1 && node.accept_count == 0)
                ++n;
            if (n > nodes.size() && i >= nodes.size()) break;

            TOX_ERR_BOOTSTRAP er;
            //if (options.proxy_type == TOX_PROXY_TYPE_NONE)
            {
                tox_bootstrap(tox, node.addr4.cstr(), node.port, node.pubid.key, &er);
                if (options.ipv6_enabled && !node.addr6.is_empty())
                    tox_bootstrap(tox, node.addr6.cstr(), node.port, node.pubid.key, &er);
                if (TOX_ERR_BOOTSTRAP_BAD_HOST == er)
                {
                    ++n;
                    if (n > nodes.size() && i >= nodes.size()) break;
                    continue;
                }
            }

            tox_add_tcp_relay(tox, node.addr4.cstr(), node.port, node.pubid.key, &er);
            if (options.ipv6_enabled && !node.addr6.is_empty())
                tox_add_tcp_relay(tox, node.addr6.cstr(), node.port, node.pubid.key, &er);
        }


    }

    int send_request(const char *dnsname, const tox_address_c &toxaddr, const char *invite_message_utf8, bool resend);
    void add_conference(TOX_CONFERENCE_TYPE t, const conference_id_s &id, const char *name);

    void save_current_stuff(savebuffer &b);
    void init_done();
    void goodbye();
    void do_on_offline_stuff();
    void online();
    void offline()
    {
        online_flag = false;
        do_on_offline_stuff();
    }


    std::string adv_get_listen_port()
    {
        std::string s;
        s.set_as_int(options.tcp_port);
        return s;
    }
    void adv_set_listen_port(const std::pstr_c &val)
    {
        int new_server_port = val.as_int();
        if (new_server_port != options.tcp_port)
            options.tcp_port = static_cast<uint16_t>(new_server_port), reconnect = true;
    }

    std::string adv_get_allow_hole_punch()
    {
        std::string s;
        s.set(options.hole_punching_enabled ? STD_ASTR("1") : STD_ASTR("0"));
        return s;
    }
    void adv_set_allow_hole_punch(const std::pstr_c &val)
    {
        bool v = val.as_int() != 0;
        if (options.hole_punching_enabled != v)
            options.hole_punching_enabled = v, reconnect = true;
    }

    std::string adv_get_allow_local_discovery()
    {
        std::string s;
        s.set(options.local_discovery_enabled ? STD_ASTR("1") : STD_ASTR("0"));
        return s;
    }
    void adv_set_allow_local_discovery(const std::pstr_c &val)
    {
        bool v = val.as_int() != 0;
        if (options.local_discovery_enabled != v)
            options.local_discovery_enabled = v, reconnect = true;
    }

    std::string adv_get_restart_on_zero_online()
    {
        std::string s;
        s.set(restart_on_zero_online ? STD_ASTR("1") : STD_ASTR("0"));
        return s;
    }
    void adv_set_restart_on_zero_online(const std::pstr_c &val)
    {
        restart_on_zero_online = val.as_int() != 0;
    }

    std::string adv_get_disable_video_ex()
    {
        std::string s;
        s.set(disabled_video_ex ? STD_ASTR("1") : STD_ASTR("0"));
        return s;
    }
    void adv_set_disable_video_ex(const std::pstr_c &val)
    {
        disabled_video_ex = val.as_int() != 0;
    }

    std::string adv_get_video_codec()
    {
        std::string s;
        s.set(use_vp9_codec ? STD_ASTR("vp9") : STD_ASTR("vp8"));
        return s;
    }
    void adv_set_video_codec(const std::pstr_c &val)
    {
        use_vp9_codec = val.equals(STD_ASTR("vp9"));
    }

    std::string adv_get_video_bitrate()
    {
        std::string s;
        s.set_as_int(video_bitrate);
        return s;
    }
    void adv_set_video_bitrate(const std::pstr_c &val)
    {
        video_bitrate = val.as_int();
    }

    std::string adv_get_video_quality()
    {
        std::string s;
        s.set_as_int(video_quality);
        return s;
    }
    void adv_set_video_quality(const std::pstr_c &val)
    {
        video_quality = val.as_int();
    }

    std::string adv_get_nodes_list_file()
    {
        return nodes_fn.is_empty() ? std::string(STD_ASTR("tox_nodes.txt")) : nodes_fn;
    }
    void adv_set_nodes_list_file(const std::pstr_c &val)
    {
        std::string oldf = nodes_fn;
        nodes_fn.set(val);
        nodes_fn = adv_get_nodes_list_file();

        if (!oldf.equals(nodes_fn))
            hf->get_file(nodes_fn.cstr(), nodes_fn.get_length(), FILE_TAG_NODES);
    }

    std::string adv_get_pinned_list_file()
    {
        return pinned_fn.is_empty() ? std::string(STD_ASTR("tox_pinned.txt")) : pinned_fn;
    }
    void adv_set_pinned_list_file(const std::pstr_c &val)
    {
        std::string oldf = pinned_fn;
        pinned_fn.set(val);
        pinned_fn = adv_get_pinned_list_file();

        if (!oldf.equals(pinned_fn))
            hf->get_file(pinned_fn.cstr(), pinned_fn.get_length(), FILE_TAG_PINNED);
    }

    void send_configurable()
    {
        static const int copts = 4;
        const char * fields[copts + adv_count] = { CFGF_PROXY_TYPE, CFGF_PROXY_ADDR, copname<auto_co_ipv6_enable>::name().s, copname<auto_co_udp_enable>::name().s,
        
#define ASI(aa) #aa,
            ADVSET
#undef ASI

        };
        std::string svalues[copts + adv_count];
        svalues[0].set_as_int(tox_proxy_type);
        if (tox_proxy_type) svalues[1].set(tox_proxy_host).append_char(':').append_as_uint(tox_proxy_port);
        svalues[2].set_as_int(options.ipv6_enabled ? 1 : 0);
        svalues[3].set_as_int(options.udp_enabled ? 1 : 0);
        const char * values[copts + adv_count];

        static_assert(ARRAY_SIZE(fields) == ARRAY_SIZE(values) && ARRAY_SIZE(values) == ARRAY_SIZE(svalues), "check len");

        int i = copts;
#define ASI(aa) svalues[i++] = adv_get_##aa();
        ADVSET
#undef ASI

        i = 0;
        for (const std::string &s : svalues) values[i++] = s.cstr();

        hf->configurable(ARRAY_SIZE(fields), fields, values);
    }

    void load_node(loader &l)
    {
        int node_data_size = l(chunk_node);
        if (node_data_size == 0)
            return;

        loader lc(l.chunkdata(), node_data_size);

        if (int asz = lc(chunk_node_key))
        {
            loader al(lc.chunkdata(), asz);
            int dsz;
            if (const void *pubk = al.get_data(dsz))
            {
                if (TOX_PUBLIC_KEY_SIZE == dsz)
                {
                    const public_key_s &pk = *(const public_key_s *)pubk;
                    auto x = std::lower_bound(nodes.begin(), nodes.end(), pk);
                    if (&*x == nullptr)
                    {
                        int pc = 0, ac = 0;
                        if (lc(chunk_node_push_count))
                            pc = lc.get_i32();

                        if (lc(chunk_node_accept_count))
                            ac = lc.get_i32();

                        if (pc || ac)
                        {
                            nodes.emplace_back(pk, pc, ac);
                            std::sort(nodes.begin(), nodes.end());
                        }

                    } else if (x->pubid == pk)
                    {
                        if (lc(chunk_node_push_count))
                            x->push_count = lc.get_i32();

                        if (lc(chunk_node_accept_count))
                            x->accept_count = lc.get_i32();
                    }
                }
            }
        }
    }

};

static tox_c cl;

void tox_c::fsh_s::die(int index)
{
    if (index >= 0)
    {
        removeFast(cl.sfs, index);
        return;
    }

    size_t cnt = cl.sfs.size();
    for (size_t i = 0; i < cnt; ++i)
    {
        fsh_ptr_s &sf = cl.sfs[i];
        if (sf.sf == this)
        {
            removeFast(cl.sfs, i);
            break;
        }
    }
}



void tox_c::node_accept_handler(const uint8_t *public_key)
{
    const public_key_s &pk = *(const public_key_s *)public_key;
    auto x = std::lower_bound(cl.nodes.begin(), cl.nodes.end(), pk);
    if (&*x == nullptr)
    {
        cl.nodes.emplace_back(pk, 0, 0);
        std::sort(cl.nodes.begin(), cl.nodes.end());
        x = std::lower_bound(cl.nodes.begin(), cl.nodes.end(), pk);
    }

    if (x->pubid == pk)
    {
        x->accept_count += 2;
        if (!x->accepted)
            ++cl.accepted_nodes, x->accepted = true;
    }
}


void tox_address_c::setup_self_address()
{
    tox_self_get_address( cl.tox, id );
    type = TAT_FULL_ADDRESS;
}

void tox_address_c::setup_public_key(fid_s fid)
{
    type = TAT_EMPTY;
    if (fid.is_normal() && tox_friend_get_public_key( cl.tox, fid.normal(), id, nullptr ))
        type = TAT_PUBLIC_KEY;
}

struct stream_settings_s : public audio_format_s
{
    fifo_stream_c fifo;
    int next_time_send = 0;
    int video_w = 0, video_h = 0;
    const void *video_data = nullptr;
    int locked = 0;
    bool processing = false;

    stream_settings_s()
    {
    }
};

static std::string dnsquery( const std::string & query, const std::asptr& ver )
{
    bool ok_version = false;
    DNS_RECORDW *record = nullptr;
    DnsQuery_UTF8(query, DNS_TYPE_TEXT, 0, nullptr, &record, nullptr);
    while (record)
    {
        DNS_TXT_DATA *txt = &record->Data.Txt;
        if (record->wType == DNS_TYPE_TEXT && txt->dwStringCount)
        {
            if (txt->pStringArray[0])
            {
                std::string retid;

                const char *r = (const char *)txt->pStringArray[0];
                std::asptr rr(r);
                if (ver.l == 0 && rr.l == 64)
                    return std::string(rr);
                std::token<char> t(rr, ';');
                for (; t; ++t)
                {
                    std::token<char> tt(*t, '=');
                    if (tt)
                    {
                        if (tt->equals(STD_ASTR("v")))
                        {
                            ++tt;
                            if (!tt || !tt->equals(ver))
                                return std::string();
                            ok_version = true;
                            continue;
                        }
                        if (tt->equals(STD_ASTR("id")))
                        {
                            ++tt;
                            if (!tt)
                                return std::string();

                            retid = tt->as_sptr();
                            break;
                        }
                    }
                }
                if (ok_version && !retid.is_empty())
                    return retid;
            }
        }

        record = record->pNext;
    }

    return std::string();
}


struct message2send_s
{
    static message2send_s *first;
    static message2send_s *last;
    message2send_s *prev;
    message2send_s *next;

    u64 utag;
    fid_s fid;
    int mid;
    int next_try_time;
    int send_timeout = 0;
    std::string msg, sentmsg;
    time_t create_time = 0;

    message2send_s( u64 utag, fid_s fid, const std::asptr &utf8, time_t create_time, int imid = -10000 ):utag(utag), fid(fid), mid(imid), next_try_time(time_ms()), create_time(create_time)
    {
        LIST_ADD( this, first, last, prev, next );

        ASSERT(fid.is_normal() || fid.is_confa());

        if (-10000 == imid)
        {
            msg.set(STD_ASTR("\1")).append_as_num<u64>(create_time).append(STD_ASTR("\2"));
            mid = 0;
        }

        int timepreffix = msg.get_length();
        msg.append( utf8 );

        int e = TOX_MAX_MESSAGE_LENGTH - 3;
        if (utf8.l >= e)
        {
            for(;e>=(TOX_MAX_MESSAGE_LENGTH/2-8);--e)
                if (msg.get_char(e) == ' ')
                    break;

            if (e < TOX_MAX_MESSAGE_LENGTH/2)
            {
                // no spaces from TOX_MAX_MESSAGE_LENGTH/2 to end of message!
                // it is not good
                // so, force clamp such long long word

                std::asptr mclamp = utf8clamp( msg, TOX_MAX_MESSAGE_LENGTH-3 );
                e = mclamp.l;
            }

            {
                if ((utf8.l - e - timepreffix) > 0) new message2send_s(utag, fid, utf8.skip(e - timepreffix), create_time, -1); // not memory leak
                msg.set_length(e).append(STD_ASTR("\1\1")); // double tabs indicates that it is multi-part message
            }
        }
    }
    ~message2send_s()
    {
        LIST_DEL( this, first, last, prev, next );
    }

    void try_send(int time);

    static void read( u64 dtag )
    {
        if (dtag)
        {
            for (message2send_s *x = first; x; x = x->next)
            {
                if (x->utag == dtag)
                {
                    // next message part...
                    x->mid = 0; // allow send
                    x->next_try_time = time_ms();
                    dtag = 0;
                    break;
                }
            }
        }

        if (dtag)
            cl.hf->delivered( dtag );
    }


    static void read( const std::asptr &m )
    {
        u64 dtag = 0;
        for (message2send_s *x = first; x; x = x->next)
        {
            if (x->mid == 777777777 && m == x->sentmsg)
            {
                dtag = x->utag;
                delete x; // no need anymore
                break;
            }
        }
        read( dtag );
    }

    static void read(fid_s fid, int receipt)
    {
        u64 dtag = 0;
        for (message2send_s *x = first; x; x = x->next)
        {
            if (x->fid == fid && receipt == x->mid)
            {
                dtag = x->utag;
                delete x; // no need anymore
                break;
            }
        }

        read( dtag );
    }

    static void tick(int ct)
    {
        std::vector<fid_s> disabled;

        for(message2send_s *x = first; x; x = x->next)
        {
            if (x->mid < 0)
            {
                // block send messages to friend while multi-part message sending
                addIfNotPresent(disabled, x->fid);
                continue;
            }
            if ( x->mid == 0 )
            {
                if (isPresent(disabled, x->fid)) continue;
                x->try_send(ct);
                if ( x->mid == 0 )
                    addIfNotPresent(disabled, x->fid); // to preserve delivery order
            } else if ( (ct - x->send_timeout) > 0 )
            {
                // seems message not delivered - just kill it
                // application will send it again
                u64 utag_multipart = x->utag;
                delete x;

                if (utag_multipart)
                {
                    // set timeout for other parts of message
                    for (message2send_s *m = first; m; m = m->next)
                    {
                        if (m->utag == utag_multipart)
                        {
                            // next message part...
                            m->send_timeout = ct;
                        }
                    }
                }

                break;
            }
        }
    }
};

message2send_s *message2send_s::first = nullptr;
message2send_s *message2send_s::last = nullptr;


struct message_part_s
{
    static message_part_s *first;
    static message_part_s *last;
    message_part_s *prev;
    message_part_s *next;
    u64 create_time;
    contact_id_s cid;
    std::string msgb;
    int send2host_time = 0;

    message_part_s(contact_id_s cid, u64 create_time_, const std::asptr &msgbody, bool asap): cid(cid), create_time(create_time_)
    {
        LIST_ADD(this, first, last, prev, next);
        send2host_time = time_ms() + (asap ? 0 : 5000);
        msgb.set( extract_create_time(create_time, msgbody, cid) );
    }
    ~message_part_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    static std::asptr extract_create_time( u64 &t, const std::asptr &msgbody, contact_id_s cid );

    static void msg(contact_id_s cid, u64 create_time, const char *msgbody, int len)
    {
        if ( len > TOX_MAX_MESSAGE_LENGTH/2 && msgbody[len-1] == '\1' && msgbody[len-2] == '\1' )
        {
            // multi-part message
            // wait other parts 5 seconds
            for (message_part_s *x = first; x; x = x->next)
            {
                if (x->cid == cid)
                {
                    x->msgb.append_char(' ').append(std::asptr(msgbody, len-2));
                    x->send2host_time = time_ms() + 5000; // wait next part 5 second
                    return;
                }
            }
            new message_part_s(cid, create_time, std::asptr(msgbody, len-2), false); // not memory leak
        
        } else
        {
            for (message_part_s *x = first; x; x = x->next)
            {
                if (x->cid == cid)
                {
                    // looks like last part of multi-part message
                    // concatenate and send it to host
                    x->msgb.append_char(' ').append(std::asptr(msgbody, len));
                    cl.hf->message(MT_MESSAGE, contact_id_s(), x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
                    delete x;
                    return;
                }
            }
            std::asptr m = extract_create_time(create_time, std::asptr(msgbody, len), cid);
            cl.hf->message(MT_MESSAGE, contact_id_s(), cid, create_time, m.s, m.l);
        }
    }

    static void tick(int ct)
    {
        for (message_part_s *x = first; x; x = x->next)
        {
            if ( (ct - x->send2host_time) > 0 )
            {
                cl.hf->message(MT_MESSAGE, contact_id_s(), x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
                delete x;
                break;
            }
        }
    }

};

message_part_s *message_part_s::first = nullptr;
message_part_s *message_part_s::last = nullptr;


struct file_transfer_s
{
    virtual ~file_transfer_s() {}
    virtual void accepted() {}
    virtual void kill() {}
    virtual void pause() {}
    virtual void finished() {}
    virtual void resume() {}

    fid_s fid;
    byte fileid[TOX_FILE_ID_LENGTH];

    u64 fsz;
    u64 utag;
    u32 fnn;

    int check_count = 0;

};

static u64 gen_file_utag();
struct incoming_file_s : public file_transfer_s
{
    static incoming_file_s *first;
    static incoming_file_s *last;
    incoming_file_s *prev;
    incoming_file_s *next;
    
    u64 chunk_offset = 0;
    u64 next_recv_offset = 0;
    byte_buffer chunk;

    int last_time_portion_flush = time_ms();

    incoming_file_s(fid_s fid_, u32 fnn_, u64 fsz_ )
    {
        fsz = fsz_;
        fid = fid_;
        fnn = fnn_;

        tox_file_get_file_id( cl.tox, fid.normal(), fnn, fileid, nullptr );
        utag = gen_file_utag();

        LIST_ADD(this, first, last, prev, next);
    }

    /*virtual*/ ~incoming_file_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    virtual void recv_data( u64 position, const byte *data, size_t datasz )
    {
        if (next_recv_offset == 0 && position > 0)
        {
            // file resume from position
            chunk_offset = position;
        } else if ( position != next_recv_offset )
        {
            kill();
            return;
        }

        next_recv_offset = position + datasz;
        u64 newsize = position + datasz - chunk_offset;
        if (newsize > chunk.size())
            chunk.resize((u32)newsize);
        
        memcpy(chunk.data() + position - chunk_offset, data, datasz);

        if (newsize >= FILE_TRANSFER_CHUNK )
        {
        portion_anyway_flush:
            int portion_size = static_cast<int>( min( FILE_TRANSFER_CHUNK, chunk.size() ) );
            if (cl.hf->file_portion(utag, chunk_offset, chunk.data(), portion_size ))
            {
                chunk_offset += portion_size;
                aint ostsz = chunk.size() - portion_size;
                memcpy(chunk.data(), chunk.data() + portion_size, ostsz);
                chunk.resize(ostsz);
            }
        } else
        {
            int current_time = time_ms();
            if ( ( current_time - last_time_portion_flush ) >= 1000 && chunk.size() )
            {
                last_time_portion_flush = current_time;
                goto portion_anyway_flush;
            }
        }

    }

    /*virtual*/ void accepted() override
    {
        cl.hf->file_control(utag,FIC_UNPAUSE);
    }
    /*virtual*/ void kill() override
    {
        cl.hf->file_control(utag,FIC_BREAK);
        delete this;
    }
    /*virtual*/ void finished() override
    {
        if (chunk.size())
        {
            if (!cl.hf->file_portion(utag, chunk_offset, chunk.data(), static_cast<int>(chunk.size())))
                return; // just keep file unfinished
        }

        cl.hf->file_control(utag,FIC_DONE);
        //delete this; do not delete now due FIC_DONE from app
    }
    /*virtual*/ void pause() override
    {
        cl.hf->file_control(utag,FIC_PAUSE);
    }

    virtual void on_disconnect()
    {
        cl.hf->file_control(utag, FIC_DISCONNECT);
        delete this;
    }
    virtual void on_tick(int /*ct*/) {}

    static void tick(int ct);

};

struct incoming_blob_s : public incoming_file_s
{
    int droptime;
    incoming_blob_s(fid_s fid_, u32 fnn_, u64 fsz_) : incoming_file_s(fid_, fnn_, fsz_)
    {
        droptime = time_ms() + 30000;
        chunk.resize(static_cast<size_t>(fsz_));
    }

    /*virtual*/ void recv_data(u64 position, const byte *data, size_t datasz) override
    {
        droptime = time_ms() + 30000;

        u64 newsize = position + datasz;
        if (newsize <= chunk.size())
            memcpy(chunk.data() + position, data, datasz);
    }
    /*virtual*/ void kill() override { delete this; }
    /*virtual*/ void pause() override {}
    /*virtual*/ void accepted() override {}
    /*virtual*/ void on_disconnect() override { delete this; }

    /*virtual*/ void finished() override
    {
        delete this;
    }

    /*virtual*/ void on_tick(int ct) override
    {
        if ((ct - droptime) > 0)
        {
            tox_file_control(cl.tox, fid.normal(), fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
            delete this;
        }
    }

};


struct incoming_avatar_s : public incoming_blob_s
{
    incoming_avatar_s(fid_s fid_, u32 fnn_, u64 fsz_) : incoming_blob_s( fid_, fnn_, fsz_ )
    {
    }
    
    /*virtual*/ void finished() override;
    /*virtual*/ void on_tick(int ct) override;

};

void incoming_file_s::tick(int ct)
{
    for (incoming_file_s *f = incoming_file_s::first; f;)
    {
        incoming_file_s *ff = f->next;

        if (TOX_CONNECTION_NONE == tox_friend_get_connection_status(cl.tox,f->fid.normal(),nullptr))
        {
            // peer disconnected - transfer break
            f->on_disconnect();
        }
        else
            f->on_tick(ct);

        f = ff;
    }
}


incoming_file_s *incoming_file_s::first = nullptr;
incoming_file_s *incoming_file_s::last = nullptr;


struct transmitting_data_s : public file_transfer_s
{
    static transmitting_data_s *first;
    static transmitting_data_s *last;
    transmitting_data_s *prev;
    transmitting_data_s *next;
    std::string fn;

    u64 next_offset_send = 0;
    u64 req_offset = 0;
    u32 req_max_size = 0;

#ifdef _DEBUG
    struct creq_s
    {
        u64 offset;
        aint size;
        creq_s( u64 offset, aint size ):offset(offset), size(size) {}
    };
    std::vector<creq_s> allcorereqs;
    std::vector<creq_s> allappreqs;
    std::vector<creq_s> allapppor;
#endif // _DEBUG

    bool is_accepted = false;
    bool is_paused = false;
    bool is_finished = false;

    transmitting_data_s(fid_s fid_, u32 fnn_, u64 utag_, const byte * fileid_, u64 fsz_, const std::string &fn) :fn(fn)
    {
        fsz = fsz_;
        fid = fid_;
        fnn = fnn_;
        memcpy( fileid, fileid_, TOX_FILE_ID_LENGTH );
        utag = utag_;

        LIST_ADD(this, first, last, prev, next);
    }

    ~transmitting_data_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    virtual void ontick() = 0;
    virtual bool portion(const file_portion_prm_s * /*portion*/) { return false; }
    virtual void send_data() = 0;
    virtual void resume_from(u64 /*offset*/) {};

    void toxcore_request(u64 offset, aint size)
    {
#ifdef _DEBUG
        allcorereqs.emplace_back( offset, size );
#endif // _DEBUG
        
        if (size == 0)
        {
            finished();
            return;
        }

        if ( req_offset == 0 && offset > 0 )
        {
            // resume from offset
            resume_from( offset );
        }
        req_offset = offset + size;
        if (size > (aint)req_max_size) req_max_size = (u32)size;
    }

    /*virtual*/ void accepted() override
    {
        is_accepted = true;
        is_paused = false;
        cl.hf->file_control(utag, FIC_ACCEPT);
    }
    /*virtual*/ void kill() override
    {
        delete this;
    }
    /*virtual*/ void finished() override
    {
        delete this;
    }
    /*virtual*/ void pause() override
    {
        is_paused = true;
    }

    static void tick()
    {
        for (transmitting_data_s *f = transmitting_data_s::first; f;)
        {
            transmitting_data_s *ff = f->next;
            f->ontick();
            f = ff;
        }
    }
};

struct transmitting_file_s : public transmitting_data_s
{
    typedef transmitting_data_s super;

    struct app_req_s
    {
        const void * buf = nullptr;
        u64 offset = 0;
        int size = 0;
        
        bool set(const file_portion_prm_s *fp)
        {
            if (nullptr == buf && fp->offset == offset)
            {
                buf = fp->data;
                offset = fp->offset;
                size = fp->size;
                return true;
            }
            return false;
        }


        void clear()
        {
            if (buf)
                cl.hf->file_portion(0, 0, buf, 0); // release buffer

            memset(this, 0, sizeof(*this));
        }
        ~app_req_s()
        {
            clear();
        }
    };
    app_req_s rch[2];
    bool request = false;

    transmitting_file_s(fid_s fid_, u32 fnn_, u64 utag_, const byte * id_, u64 fsz_, const std::string &fn) : transmitting_data_s(fid_, fnn_, utag_, id_, fsz_, fn)
    {
    }

    /*virtual*/ void finished() override
    {
        cl.hf->file_control(utag, FIC_DONE);
        super::finished();
    }
    /*virtual*/ void kill() override
    {
        cl.hf->file_control(utag, FIC_BREAK);
        super::kill();
    }

    /*virtual*/ void pause() override
    {
        super::pause();
        cl.hf->file_control(utag, FIC_PAUSE);
    }

    /*virtual*/ void resume_from(u64 offset) override
    {
        rch[0].clear();
        rch[1].clear();

        u64 roffs = offset & FILE_TRANSFER_CHUNK_MASK;
        next_offset_send = offset;

        rch[0].offset = roffs;
        cl.hf->file_portion(utag, roffs, nullptr, FILE_TRANSFER_CHUNK); // request now
        request = true;
    };

    /*virtual*/ bool portion(const file_portion_prm_s *fp) override
    {
#ifdef _DEBUG
        allapppor.emplace_back( fp->offset, time_ms() );
#endif // _DEBUG

        ASSERT(fp->size == FILE_TRANSFER_CHUNK || (fp->offset + fp->size) == fsz);

        if (rch[0].set(fp))
        {
            ASSERT(rch[1].buf == nullptr);
            send_data();
            query_next_chunk();

        }
        else
        {
            ASSERT(rch[0].offset + FILE_TRANSFER_CHUNK == fp->offset && rch[1].buf == nullptr);
            CHECK(rch[1].set(fp));
        }

        return true;
    }

    void query_next_chunk()
    {
        if (req_max_size == 0)
            return; // there is no request from toxcore, so do nothing

        if ( !request )
        {
            ASSERT(rch[0].offset == 0 && rch[0].buf == nullptr);
            cl.hf->file_portion(utag, 0, nullptr, FILE_TRANSFER_CHUNK); // request very first chunk now

#ifdef _DEBUG
            allappreqs.emplace_back( 0, time_ms() );
#endif // _DEBUG
            request = true;
            return;
        }

        if (rch[1].offset > 0)
            return;

        u64 nextr = rch[0].offset + FILE_TRANSFER_CHUNK;

        if (fsz > nextr)
        {
            cl.hf->file_portion(utag, nextr, nullptr, FILE_TRANSFER_CHUNK); // request now
#ifdef _DEBUG
            allappreqs.emplace_back( nextr, time_ms() );
#endif // _DEBUG

            rch[1].offset = nextr;
        }
    }

    void send_data() override
    {
        if (nullptr == rch[0].buf || req_max_size == 0)
        {
            query_next_chunk();
            return;
        }

        uint8_t *temp = (uint8_t *)_alloca( req_max_size );

        for ( ; next_offset_send >= rch[0].offset && next_offset_send < req_offset; )
        {
            if ( (next_offset_send + req_max_size) <= (rch[0].offset + rch[0].size) )
            {
                // within pre-chunk block
                // safely send

                TOX_ERR_FILE_SEND_CHUNK er;
                tox_file_send_chunk(cl.tox, fid.normal(), fnn, next_offset_send, (const uint8_t *)rch[0].buf + (next_offset_send - rch[0].offset), req_max_size, &er);
                if (TOX_ERR_FILE_SEND_CHUNK_OK != er)
                    return; // pipe overload. do nothing now

                check_count = 0;

                next_offset_send += req_max_size;
                continue;
            }
            if (next_offset_send == (rch[0].offset + rch[0].size))
            {
                // pre-chunk block completely send

                if (rch[1].offset > 0)
                {
                    // at least next chunk requested
                    rch[0].clear();
                    rch[0] = rch[1];
                    rch[1].buf = nullptr;
                    rch[1].clear();
                    query_next_chunk();

                } else
                {
                    // next chunk not yet requested
                    // request it now

                    query_next_chunk();

                    rch[0].clear();
                    rch[0] = rch[1];
                    rch[1].buf = nullptr;
                    rch[1].clear();
                }

                if (nullptr == rch[0].buf)
                    return;

                continue;
            }

            int chunk0part = (int)((rch[0].offset + rch[0].size) - next_offset_send);

            if ( fsz == (rch[0].offset + rch[0].size))
            {
                // last chunk

                ASSERT((next_offset_send + chunk0part) == fsz);

                TOX_ERR_FILE_SEND_CHUNK er;
                tox_file_send_chunk(cl.tox, fid.normal(), fnn, next_offset_send, (const uint8_t *)rch[0].buf + (next_offset_send - rch[0].offset), chunk0part, &er);
                if (TOX_ERR_FILE_SEND_CHUNK_OK != er)
                    return; // pipe overload. do nothing now

                check_count = 0;

                next_offset_send += chunk0part;
                return;
            }

            // we should send packet with end-of-0 and begin-of-1 chunk

            int chunk1part = (int)((next_offset_send + req_max_size) - (rch[0].offset + rch[0].size));
            
            ASSERT(chunk0part > 0 && chunk0part < (int)req_max_size);
            ASSERT(chunk1part > 0 && chunk1part < (int)req_max_size);
            ASSERT(u32(chunk0part + chunk1part) == req_max_size);
            ASSERT( next_offset_send < (rch[0].offset + rch[0].size) && (next_offset_send + req_max_size) >(rch[0].offset + rch[0].size));

            if (nullptr == rch[1].buf)
            {
                // chunk 1 not yet ready
                query_next_chunk();
                return;
            }

            if (chunk1part > rch[1].size)
                chunk1part = rch[1].size;

            memcpy( temp, (const uint8_t *)rch[0].buf + (next_offset_send - rch[0].offset), chunk0part);
            memcpy( temp + chunk0part, (const uint8_t *)rch[1].buf, chunk1part);
            
            TOX_ERR_FILE_SEND_CHUNK er;
            tox_file_send_chunk(cl.tox, fid.normal(), fnn, next_offset_send, temp, chunk0part + chunk1part, &er);
            if (TOX_ERR_FILE_SEND_CHUNK_OK != er)
                return; // pipe overload. do nothing now

            check_count = 0;
            
            next_offset_send = rch[1].offset + chunk1part;

            rch[0].clear();
            rch[0] = rch[1];
            rch[1].buf = nullptr;
            rch[1].clear();
            query_next_chunk();
        }
    }

    /*virtual*/ void ontick() override
    {
        if (tox_friend_get_connection_status(cl.tox,fid.normal(),nullptr) == TOX_CONNECTION_NONE)
        {
            // peer disconnect - transfer broken
            cl.hf->file_control(utag, FIC_DISCONNECT);
            delete this;
            return;
        }

        send_data();
        if ( next_offset_send == fsz )
            finished();
    }


};

struct transmitting_blob_s : public transmitting_data_s
{
    typedef transmitting_data_s super;

    std::vector<byte> data;

    static void start(const std::string &fnc, fid_s fid_, file_type_e kind, const void *blob, size_t blobsize)
    {
        byte hash[TOX_HASH_LENGTH];
        tox_hash(hash, (const byte *)blob, blobsize);

        TOX_ERR_FILE_SEND er = TOX_ERR_FILE_SEND_NULL;
        uint32_t tr = tox_file_send(cl.tox, fid_.normal(), kind, blobsize, hash, (const byte *)fnc.cstr(), fnc.get_length(), &er);
        if (er != TOX_ERR_FILE_SEND_OK)
            return;

        new transmitting_blob_s(fid_, tr, fnc, hash, blob, blobsize); // not memleak

    }

    transmitting_blob_s(fid_s fid_, u32 fnn_, const std::string &fn, byte *hash, const void *blob, size_t blobsize) : transmitting_data_s(fid_, fnn_, gen_file_utag(), hash, blobsize, fn)
    {
        ASSERT(fid_.is_normal());

        data.resize(blobsize);
        memcpy(data.data(), blob, blobsize);
    }
    virtual void ontick() override
    {
        byte id[TOX_FILE_ID_LENGTH];
        if (!tox_file_get_file_id(cl.tox, fid.normal(), fnn, id, nullptr))
        {
            // transfer broken
            delete this;
            return;
        }

        send_data();

        if (next_offset_send == fsz)
            finished();

    }
    /// /*virtual*/ void kill() { super.kill(); }
    /*virtual*/ void finished() override
    {
        super::finished();
    }

    /*virtual*/ void send_data() override
    {
        if (next_offset_send < req_offset)
        {
            u32 sb = static_cast<u32>(data.size() - next_offset_send);
            if (sb > req_max_size) sb = req_max_size;

            TOX_ERR_FILE_SEND_CHUNK er;
            tox_file_send_chunk(cl.tox, fid.normal(), fnn, next_offset_send, data.data() + next_offset_send, sb, &er);
            if (TOX_ERR_FILE_SEND_CHUNK_OK != er)
                return;

            check_count = 0;

            next_offset_send += sb;
        }

    }
};


struct transmitting_avatar_s : public transmitting_data_s
{
    typedef transmitting_data_s super;
    int avatag = -1;

    transmitting_avatar_s(fid_s fid_, u32 fnn_, const std::string &fn) : transmitting_data_s(fid_, fnn_, gen_file_utag(), cl.avahash, cl.gavatar.size(), fn)
    {
        ASSERT(fid_.is_normal());
        avatag = cl.gavatag;
    }
    virtual void ontick() override
    {
        if (avatag != cl.gavatag)
        {
            // avatar changed while it sending
            tox_file_control(cl.tox, fid.normal(), fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
            delete this;
        } else
        {
            byte id[TOX_FILE_ID_LENGTH];
            if (!tox_file_get_file_id(cl.tox, fid.normal(), fnn, id, nullptr))
            {
                // transfer broken
                delete this;
                return;
            }

            send_data();

            if (next_offset_send == fsz)
                finished();
        }

    }
    /*virtual*/ void kill() override;
    /*virtual*/ void finished() override;

    /*virtual*/ void send_data() override
    {
        if (avatag != cl.gavatag) return;

        if (next_offset_send < req_offset)
        {
            u32 sb = static_cast<u32>(cl.gavatar.size() - next_offset_send);
            if (sb > req_max_size) sb = req_max_size;

            TOX_ERR_FILE_SEND_CHUNK er;
            tox_file_send_chunk(cl.tox, fid.normal(), fnn, next_offset_send, cl.gavatar.data() + next_offset_send, sb, &er);
            if (TOX_ERR_FILE_SEND_CHUNK_OK != er)
                return;

            check_count = 0;
            next_offset_send += sb;
        }
        
    }
};


transmitting_data_s *transmitting_data_s::first = nullptr;
transmitting_data_s *transmitting_data_s::last = nullptr;

struct discoverer_s
{
    static discoverer_s *first;
    static discoverer_s *last;
    discoverer_s *prev;
    discoverer_s *next;
    
    int timeout_discover = 0;
    std::string invmsg;

    struct syncdata_s
    {
        std::string ids;
        tox_address_c pubid;

        bool waiting_thread_start = true;
        bool thread_in_progress = false;
        bool shutdown_discover = false;
    };

    spinlock::syncvar< syncdata_s > sync;

    discoverer_s(const std::asptr &ids, const std::asptr &invmsg) :invmsg(invmsg)
    {
        sync.lock_write()().ids.set( ids );
        timeout_discover = time_ms() + 30000; // 30 seconds should be enough
        LIST_ADD(this, first, last, prev, next);
        CloseHandle(CreateThread(nullptr, 0, discover_thread, this, 0, nullptr));
    }
    ~discoverer_s()
    {
        sync.lock_write()().shutdown_discover = true;
        while(sync.lock_read()().thread_in_progress) Sleep(1);

        LIST_DEL(this, first, last, prev, next);
    }

    bool do_discover( int ct )
    {
        if ((ct - timeout_discover) > 0)
            sync.lock_write()().shutdown_discover = true;

        auto r = sync.lock_read();

        if (r().shutdown_discover && !r().thread_in_progress)
        {
            cl.hf->operation_result(LOP_ADDCONTACT, CR_TIMEOUT);
            delete this;
            return true;
        }

        if (r().thread_in_progress || r().waiting_thread_start)
            return false;

        if (!r().pubid.compatible(tox_address_c::TAT_PUBLIC_KEY))
        {
            cl.hf->operation_result(LOP_ADDCONTACT, CR_INVALID_PUB_ID);
        } else
        {
            int rslt = cl.send_request(r().ids, r().pubid, invmsg, false);
            cl.hf->operation_result(LOP_ADDCONTACT, rslt);
        }
        r.unlock(); // unlock now to avoid deadlock
        delete this;
        return true;
    }

    static size_t header_callback( char * /*buffer*/, size_t size, size_t nitems, void * /*userdata*/ )
    {
        return size * nitems;
    }

    static size_t write_callback( char *ptr, size_t size, size_t nmemb, void *userdata )
    {
        byte_buffer *resultad = (byte_buffer * )userdata;
        size_t s = resultad->size();

        resultad->resize( size * nmemb + s );
        memcpy( resultad->data(), ptr, size * nmemb );
        return size * nmemb;
    }

    std::string json_request( const std::string&url, const std::string&json )
    {
        byte_buffer d;
        CURL *curl = curl_easy_init();
        set_common_curl_options(curl);
        cl.set_proxy_curl(curl);
        int rslt = 0;
        rslt = curl_easy_setopt( curl, CURLOPT_WRITEDATA, &d );
        rslt = curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, header_callback );
        rslt = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_callback );

        curl_slist hl = { "Content-Type: application/json", nullptr };
        curl_easy_setopt( curl, CURLOPT_HTTPHEADER, &hl );

        curl_easy_setopt( curl, CURLOPT_POST, 1 );
        curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json.cstr() );

        rslt = curl_easy_setopt( curl, CURLOPT_URL, url.cstr() );

        rslt = curl_easy_perform( curl );

        curl_easy_cleanup( curl );

        return std::string((const char *)d.data(), d.size());
    }

    std::string try_resolve_via_https_api( const std::string&addr )
    {
        std::string address = addr;
        address.trim();
        address.replace_all( STD_ASTR( "\\" ), STD_ASTR( "\\\\" ) );
        address.replace_all( STD_ASTR( "\"" ), STD_ASTR( "\\\"" ) );

        std::string json( STD_ASTR( "{\"action\":3,\"name\":\"" ), address );
        json.append( STD_ASTR( "\"}" ) );

        int uho = address.find_pos( '@' );

        std::string apiUrl( STD_ASTR( "https://" ), address.substr( uho+1 ) );
        apiUrl.append( STD_ASTR("/api"));

        std::string response = json_request( apiUrl, json );
        if (!response.is_empty())
        {
            int idi = response.find_pos( STD_ASTR( "tox_id\"" ) );
            if ( idi > 0 )
            {
                idi = response.find_pos( idi + 7, '\"' );
                int idi2 = -1;
                if ( idi > 0 )
                    idi2 = response.find_pos( idi + 1, '\"' );
                ++idi;
                if ( idi2 > idi && idi2 - idi == TOX_ADDRESS_SIZE*2 )
                {
                    response.set_length(idi2);
                    response.cut(0, idi);
                    return response;
                }
            }
        }
        return std::string();
    }

    void discover_thread()
    {
        std::string ids;
        auto w = sync.lock_write();
        if (w().shutdown_discover) return;
        w().waiting_thread_start = false;
        w().thread_in_progress = true;

        ids.setcopy( w().ids );
        w.unlock();

        std::string pubid = try_resolve_via_https_api( ids );
        if (!pubid.is_empty())
        {
            sync.lock_write()( ).pubid.setup( pubid );
        }
        else
        {

            std::string servname = ids.substr( ids.find_pos( '@' ) + 1 );

            bool pinfound = false;
            for ( tox3dns_s& pin : cl.pinnedservs )
            {
                if ( sync.lock_read()( ).shutdown_discover )
                    break;

                if ( servname.equals( pin.addr ) )
                {
                    std::string s = pin.query3( ids );
                    sync.lock_write()( ).pubid.setup( s );
                    pinfound = true;
                    break;
                }
            }

            if ( !pinfound )
            {
                cl.pinnedservs.emplace_back( servname );
                std::string s = cl.pinnedservs.back().query3( ids );
                if ( !cl.pinnedservs.back().key_ok )
                    cl.pinnedservs.erase( --cl.pinnedservs.end() ); // kick non tox3 servers from list

                sync.lock_write()( ).pubid.setup( s );
            }
        }

        sync.lock_write()().thread_in_progress = false;
    }

    static u32 WINAPI discover_thread(LPVOID p)
    {
        UNSTABLE_CODE_PROLOG
            ((discoverer_s *)p)->discover_thread();
        UNSTABLE_CODE_EPILOG
        return 0;
    }

    static void tick(int ct)
    {
        for(discoverer_s *d = first; d; d = d->next)
            if (d->do_discover(ct))
                break;
    }
};

discoverer_s *discoverer_s::first = nullptr;
discoverer_s *discoverer_s::last = nullptr;

static u64 gen_file_utag()
{
    return random64( []( u64 t ) {
    
        for ( transmitting_data_s *f = transmitting_data_s::first; f; f = f->next )
            if ( f->utag == t )
                return true;

        for ( incoming_file_s *f = incoming_file_s::first; f; f = f->next )
            if ( f->utag == t )
                return true;

        return false;
    });
}

enum idgen_e
{
    SKIP_ID_GEN,
    ID_CONTACT,
    ID_CONFERENCE,
    ID_UNKNOWN,
};

static fid_s find_tox_fid(const public_key_s &id, bool strong_check = false);

enum contact_caps_tox_e
{
    CCC_MSG_CHAIN = 1,  // contact's client support message chaining
    CCC_MSG_CRTIME = 2, // contact's client support message create time
    CCC_VIEW_SIZE = 4,  // contact's client support video view size adjustment
    CCC_VIDEO_EX = 8,   // contact's client support vp9 and lossless video
    CCC_FOLDER_SHARE = 16,   // contact's client support folder share feature
};

struct contact_descriptor_s
{
    static contact_descriptor_s *first_desc;
    static contact_descriptor_s *last_desc;

    contact_descriptor_s *next;
    contact_descriptor_s *prev;

    contact_descriptor_s *next_call = nullptr;
    contact_descriptor_s *prev_call = nullptr;

private:
    contact_id_s id;
    fid_s fid;
public:

    struct call_in_progress_s
    {
        contact_descriptor_s *desc;
        stream_options_s local_so;
        stream_options_s remote_so;
        stream_settings_s local_settings;
        vpx_codec_ctx_t v_encoder;
        vpx_codec_ctx_t v_decoder;
        vpx_codec_enc_cfg_t enc_cfg;

        struct sending_frame_s
        {
            union
            {
                int dummy_int;
                byte dummy_byte[sizeof(int)];
            };
            int sframe;
            int soffset;

            int allocated;
            int flg;
            int offset;
            int size;
            int frame;
            bool is_done() const { return offset == size; }

        };


        spinlock::long3264 sync_frame = 0;
        sending_frame_s *current = nullptr;
        sending_frame_s *next = nullptr;

        int call_stop_time = 0;
        uint32_t frame_counter = 0;

        int vbitrate = 0;
        int vquality = -1;

        vpx_codec_dec_cfg_t cfg_dec;

        bool current_busy = false;
        bool calling = false;
        bool started = false;
        bool video_ex = false;
        bool decoder = false;
        bool encoder_vp8 = false;
        bool decoder_vp8 = false;

        call_in_progress_s(contact_descriptor_s *desc, bool videoex) :desc(desc)
        {
            if (videoex)
            {
                enc_cfg.g_w = 0;
                video_ex = true;
            }
            vbitrate = cl.video_bitrate;
            vquality = cl.video_quality;
            encoder_vp8 = !cl.use_vp9_codec;
            cfg_dec.threads = 1;
            cfg_dec.w = 0;
            cfg_dec.h = 0;
        }

        ~call_in_progress_s()
        {
            if (video_ex)
            {
                if (enc_cfg.g_w) vpx_codec_destroy(&v_encoder);
                if (decoder) vpx_codec_destroy(&v_decoder);
            }
            if (current) dlfree(current);
            if (next) dlfree(next);
        }

        void isotoxin_video_send_frame(int fid, unsigned int width, unsigned int height, const byte *y, const byte *u, const byte *v)
        {
            if (cl.disabled_video_ex)
            {
                vquality = -1;
                if (enc_cfg.g_w) vpx_codec_destroy(&v_encoder);
                if (decoder) vpx_codec_destroy(&v_decoder);
                enc_cfg.g_w = 0;
                decoder = false;
                std::sstr_t<-128> pstr("-initvdecoder/0/0/vp8");
                *(byte *)pstr.str() = PACKETID_EXTENSION;
                tox_friend_send_lossless_packet(cl.tox, fid, (const byte *)pstr.cstr(), pstr.get_length(), nullptr);
                return;
            }

            if (enc_cfg.g_w != width || enc_cfg.g_h != height || encoder_vp8 == cl.use_vp9_codec || vbitrate != cl.video_bitrate)
            {
                std::sstr_t<-128> pstr("-initvdecoder/");
                *(byte *)pstr.str() = PACKETID_EXTENSION;
                pstr.append_as_uint( width ).append_char('/').append_as_uint( height ).append_char('/');
                if (cl.use_vp9_codec)
                    pstr.append( STD_ASTR("vp9") );
                else
                    pstr.append(STD_ASTR("vp8"));

                tox_friend_send_lossless_packet(cl.tox, fid, (const byte *)pstr.cstr(), pstr.get_length(), nullptr);

                vbitrate = cl.video_bitrate;

                if (enc_cfg.g_w)
                {
                    vpx_codec_destroy(&v_encoder);
                    if (encoder_vp8 == cl.use_vp9_codec)
                        goto reinit_cfg;
                } else
                {
                reinit_cfg:
                    encoder_vp8 = !cl.use_vp9_codec;
                    if (VPX_CODEC_OK != vpx_codec_enc_config_default(encoder_vp8 ? VIDEO_CODEC_ENCODER_INTERFACE_VP8 : VIDEO_CODEC_ENCODER_INTERFACE_VP9, &enc_cfg, 0))
                    {
                        enc_cfg.g_w = 0;
                        vpx_codec_destroy(&v_decoder);
                        return;
                    }

                    enc_cfg.rc_target_bitrate = DEFAULT_VIDEO_BITRATE;
                    enc_cfg.g_w = 0;
                    enc_cfg.g_h = 0;
                    enc_cfg.g_pass = VPX_RC_ONE_PASS;
                    /* FIXME If we set error resilience the app will crash due to bug in vp8.
                    Perhaps vp9 has solved it?*/
                    //     cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT | VPX_ERROR_RESILIENT_PARTITIONS;
                    enc_cfg.g_lag_in_frames = 0;
                    enc_cfg.kf_min_dist = 0;
                    enc_cfg.kf_max_dist = 48;
                    enc_cfg.kf_mode = VPX_KF_AUTO;
                }

                encoder_vp8 = !cl.use_vp9_codec;
                enc_cfg.g_w = width;
                enc_cfg.g_h = height;
                enc_cfg.rc_target_bitrate = (vbitrate ? vbitrate : DEFAULT_VIDEO_BITRATE);

                if (vpx_codec_enc_init(&v_encoder, encoder_vp8 ? VIDEO_CODEC_ENCODER_INTERFACE_VP8 : VIDEO_CODEC_ENCODER_INTERFACE_VP9, &enc_cfg, 0) != VPX_CODEC_OK)
                {
                    enc_cfg.g_w = 0;
                    return;
                }
            }

            if (vquality != cl.video_quality)
            {
                vquality = cl.video_quality;
                int limit = encoder_vp8 ? 16 : 8;
                if (vquality < -limit) vquality = -limit;
                if (vquality > limit) vquality = limit;
                vpx_codec_control(&v_encoder, VP8E_SET_CPUUSED, vquality);
            }

            for (;;)
            {
                SIMPLELOCK(sync_frame);

                if (current_busy)
                    goto usenext;

                if (nullptr == current || current->is_done())
                    break;
                else
                {
                usenext:

                    if (nullptr == next)
                        break;
                    else if (!next->is_done())
                        return; // skip frame before encoding to keep quality of video (just lower fps)
                }
                break;
            }

            // encoding and send

            vpx_image_t img = { VPX_IMG_FMT_I420, VPX_CS_UNKNOWN, VPX_CR_STUDIO_RANGE,
                width, height, 8, width, height, 0, 0, 1, 1,
                (byte *)y, (byte *)u, (byte *)v, nullptr, (int)width, (int)width / 2, (int)width / 2, (int)width, 12 };

            int vrc = vpx_codec_encode(&v_encoder, &img, frame_counter, 1, 0, /*MAX_ENCODE_TIME_US*/ 0);
            if (vrc != VPX_CODEC_OK) return;

            vpx_codec_iter_t iter = nullptr;
            const vpx_codec_cx_pkt_t *pkt;

            while (nullptr != (pkt = vpx_codec_get_cx_data(&v_encoder, &iter)))
            {
                if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
                    add_frame_to_queue(pkt->data.frame.buf, pkt->data.frame.sz, pkt->data.frame.flags);
            }

            ++frame_counter;
        }

        void add_frame_to_queue(const void *framedata, aint framesize, int f)
        {
            sending_frame_s *sf = nullptr;
            SIMPLELOCK(sync_frame);

            if (current_busy)
                goto usenext;

            if (nullptr == current )
            {
                current = (sending_frame_s *)dlmalloc( sizeof(sending_frame_s) + framesize );
                current->allocated = static_cast<int>(framesize);
                sf = current;
            } else if ( current->is_done() )
            {
                if (current->allocated < framesize)
                {
                    current = (sending_frame_s *)dlrealloc(current, sizeof(sending_frame_s) + framesize);
                    current->allocated = static_cast<int>( framesize );
                }
                sf = current;
            } else if (current->frame == static_cast<int>( frame_counter ))
                __debugbreak();
            else
            {
            usenext:

                if (nullptr == next)
                {
                    next = (sending_frame_s *)dlmalloc(sizeof(sending_frame_s) + framesize);
                    next->allocated = static_cast<int>( framesize );
                    sf = next;
                }
                else
                {
                    /* do not skip encoded data

                    if (!next->is_done() && 0 != (next->flg & VPX_FRAME_IS_KEY) && 0 == (f & VPX_FRAME_IS_KEY))
                        return; // non-key frame can't overwrite key frame
                        */

                    if (next->allocated < framesize)
                    {
                        next = (sending_frame_s *)dlrealloc(next, sizeof(sending_frame_s) + framesize);
                        next->allocated = (int)framesize;
                    }
                    sf = next;
                }
            }

            ASSERT(sf && framesize <= sf->allocated );

            sf->frame = frame_counter;
            sf->flg = f;
            sf->offset = 0;
            sf->size = (int)framesize;
            memcpy( sf + 1, framedata, framesize );

#ifdef _DEBUG
            static bool xxx = true;
            if (xxx)
            {
                xxx = false;
                FILE *ff = fopen("1stframe.bin", "wb");
                fwrite(framedata, framesize, 1, ff);
                fclose(ff);
            }

            byte hash[crypto_generichash_BYTES_MIN];
            crypto_generichash( hash, sizeof( hash ), (const byte *)framedata, framesize, nullptr, 0 );
            std::string s(STD_ASTR("send frame: "));
            s.append_as_int( sf->frame );
            s.append_char( ' ' );
            s.append_as_num( framesize );
            s.append_char( ' ' );
            s.append_as_hex( hash, sizeof( hash ) );

            Log( "%s", s.cstr() );

#endif // _DEBUG


            if (VPX_FRAME_IS_FRAGMENT & f)
                __debugbreak();

            if (VPX_FRAME_IS_INVISIBLE & f)
                __debugbreak();
        }

        int current_recv_frame = -1;
        byte_buffer framebody;

        void video_packet( const byte *d, aint dsz )
        {
            int frnum = ntohl( *(int *)d );
            int offset = ntohl(*(int *)(d+4));
            dsz -= 8;
            d += 8;

            if (frnum == current_recv_frame)
            {
                if (offset >= 0 && offset <= (int)(framebody.size() - dsz) )
                    memcpy( framebody.data() + offset, d, dsz );

                return;

            } else if (current_recv_frame >= 0)
            {
                decode_frame();
            }

            current_recv_frame = frnum;
            if (offset < 0) framebody.resize( -offset );
            if (dsz <= (int)framebody.size())
                memcpy(framebody.data(), d, dsz);
        }

        void decode_frame()
        {
#ifdef _DEBUG
            static bool xxx = true;
            if (xxx)
            {
                xxx = false;
                FILE *f = fopen( "1stframe.bin", "wb" );
                fwrite( framebody.data(), framebody.size(), 1, f );
                fclose(f);
            }

            byte hash[crypto_generichash_BYTES_MIN];
            crypto_generichash( hash, sizeof( hash ), framebody.data(), framebody.size(), nullptr, 0 );
            std::string s( STD_ASTR( "recv frame: " ) );
            s.append_as_int( current_recv_frame );
            s.append_char( ' ' );
            s.append_as_num( framebody.size() );
            s.append_char( ' ' );
            s.append_as_hex( hash, sizeof( hash ) );

            Log( "%s", s.cstr() );

#endif // _DEBUG

            if (!decoder)
            {
                int rc = vpx_codec_dec_init(&v_decoder, decoder_vp8 ? VIDEO_CODEC_DECODER_INTERFACE_VP8 : VIDEO_CODEC_DECODER_INTERFACE_VP9, &cfg_dec, 0);
                if (rc != VPX_CODEC_OK) return;
                decoder = true;
            }

            if (VPX_CODEC_OK == vpx_codec_decode(&v_decoder, framebody.data(), static_cast<unsigned int>(framebody.size()), nullptr, 0))
            {
                vpx_codec_iter_t iter = nullptr;
                vpx_image_t *dest = vpx_codec_get_frame(&v_decoder, &iter);

                // Play decoded images
                for (; dest; dest = vpx_codec_get_frame(&v_decoder, &iter))
                {
                    media_data_s mdt;
                    mdt.vfmt.width = static_cast<unsigned short>(dest->d_w);
                    mdt.vfmt.height = static_cast<unsigned short>(dest->d_h);
                    mdt.vfmt.pitch[0] = static_cast<unsigned short>(dest->stride[0]);
                    mdt.vfmt.pitch[1] = static_cast<unsigned short>(dest->stride[1]);
                    mdt.vfmt.pitch[2] = static_cast<unsigned short>(dest->stride[2]);
                    mdt.vfmt.fmt = VFMT_I420;
                    mdt.video_frame[0] = dest->planes[0];
                    mdt.video_frame[1] = dest->planes[1];
                    mdt.video_frame[2] = dest->planes[2];
                    cl.hf->av_data(contact_id_s(), desc->get_id(true), &mdt);

                    vpx_img_free(dest);
                }
            }
        }

    } *cip = nullptr; // if not nullptr, in call list

    void prepare_call(int calldurationsec = 0)
    {
        cl.run_senders();

        ASSERT(nullptr == cip);
        cip = new call_in_progress_s(this, 0 != (ccc_caps & CCC_VIDEO_EX) );

        if (calldurationsec)
        {
            cip->call_stop_time = time_ms() + (calldurationsec * 1000);
            cip->calling = true;
        }

        cl.add_to_callstate(this);
    }

    void stop_call()
    {
        if (cip)
        {
            auto w = cl.callstate.lock_write();
            LIST_DEL(this, w().first, w().last, prev_call, next_call);

            while(cip->local_settings.locked > 0) // waiting unlock
            {
                w.unlock();
                Sleep(1);
                w = cl.callstate.lock_write();
            }

            if (cip->local_settings.video_data)
                cl.hf->free_video_data(cip->local_settings.video_data);

            delete cip;
            cip = nullptr;
            w.unlock();

            if (!is_conference())
                cl.hf->message(MT_CALL_STOP, contact_id_s(), get_id(true), now(), nullptr, 0);
        }
    }


    i32 avatar_tag = 0; // tag of contact's avatar
    int avatar_recv_fnn = -1; // receiving file number
    int avatag_self = -1; // tag of self avatar, if not equal to gavatag, then should be transfered (gavatag increased every time self avatar changed)
    byte_buffer avatar; // contact's avatar itself
    byte avatar_hash[TOX_HASH_LENGTH] = {};

    contact_state_e state = CS_ROTTEN;
    tox_address_c address;

    static const int F_IS_ONLINE = 1;
    static const int F_AVASEND = 2;
    static const int F_AVARECIVED = 4;
    static const int F_DETAILS_SENT = 8;

    int flags = 0;
    int ccc_caps = 0;

    std::string dnsname;
    std::string clientname;
    std::string bbcodes_supported;
    std::string invitemsg;

    contact_descriptor_s(idgen_e init_new_id, fid_s fid = fid_s());
    ~contact_descriptor_s()
    {
        LIST_DEL(this, first_desc, last_desc, prev, next);
        ASSERT(cip == nullptr);
    }
    static contact_descriptor_s *find( const tox_address_c &addr )
    {
        for(contact_descriptor_s *f = first_desc; f; f = f->next)
            if (f->address == addr)
                return f;

        return nullptr;
    }
    static contact_descriptor_s *find( const public_key_s &pk )
    {
        for (contact_descriptor_s *f = first_desc; f; f = f->next)
            if (f->address == pk)
                return f;

        return nullptr;
    }
    static contact_descriptor_s *find( const conference_id_s &gid )
    {
        for (contact_descriptor_s *f = first_desc; f; f = f->next)
            if (f->address == gid)
                return f;

        return nullptr;
    }

    void die();

    bool is_online() const { return ISFLAG(flags, contact_descriptor_s::F_IS_ONLINE); }
    void on_offline()
    {
        UNSETFLAG(flags, F_IS_ONLINE);
        UNSETFLAG(flags, F_AVASEND);
        UNSETFLAG(flags, F_DETAILS_SENT);
        
        bbcodes_supported.clear();
        ccc_caps = 0;
        avatag_self = cl.gavatag - 1;
        avatar_recv_fnn = -1;

        if (!cl.self_typing_contact.is_empty() && cl.self_typing_contact == get_id(false))
            cl.self_typing_contact.clear();

        for (auto it = cl.other_typing.begin(); it != cl.other_typing.end(); ++it)
        {
            if (it->fid == get_fid())
            {
                cl.other_typing.erase(it);
                break;
            }
        }

        if ( is_conference() )
        {
            state = CS_OFFLINE;
            stop_call();
        }
    }

    fid_s restore_tox_friend()
    {
        if (!cl.tox)
            return fid_s();

        const public_key_s &pk = address.as_public_key();
        fid_s rfid = find_tox_fid(pk, true);

        if (rfid.is_normal())
            return rfid;

        if (state != CS_INVITE_RECEIVE)
        {
            TOX_ERR_FRIEND_ADD er;
            if (state != CS_INVITE_SEND)
            {
                rfid.n = tox_friend_add_norequest(cl.tox, address.as_public_key().key, &er);
                if (TOX_ERR_FRIEND_ADD_OK == er)
                {
                    rfid.type = fid_s::NORMAL;
                    state = CS_OFFLINE;
                    return rfid;
                }
            }
            else
            {
                ASSERT(address.get(tox_address_c::TAT_FULL_ADDRESS) != nullptr);
                if (invitemsg.is_empty())
                    invitemsg.set(STD_ASTR("Please add"));
                rfid.n = tox_friend_add(cl.tox, address.get(tox_address_c::TAT_FULL_ADDRESS), (const uint8_t *)invitemsg.cstr(), invitemsg.get_length(), &er);
                if (TOX_ERR_FRIEND_ADD_OK == er)
                {
                    rfid.type = fid_s::NORMAL;
                    return rfid;
                }
            }
        }

        return fid_s();
    }

    int get_caps() const
    {
        int caps = 0;
        if (ccc_caps & CCC_FOLDER_SHARE) caps |= CCAPS_SUPPORT_SHARE_FOLDER;
        if (!bbcodes_supported.is_empty()) caps |= CCAPS_SUPPORT_BBCODES;
        return caps;
    }

    void prepare_details(std::string &tmps, contact_data_s &cd ) const
    {
        if (!ISFLAG(flags, F_DETAILS_SENT))
        {
            if (is_conference())
            {
                bool dummy;
                std::string idstr = get_details_pubid( dummy );
                tmps.set( STD_ASTR( "{\"" CDET_PUBLIC_UNIQUE_ID "\":\"" ) );
                tmps.append( idstr ).append_char( '\"' );

            } else
            {
                bool zero_nospam = false;
                std::string idstr = get_details_pubid( zero_nospam );
                if (zero_nospam)
                    tmps.set( STD_ASTR( "{\"" CDET_PUBLIC_ID_BAD "\":\"" ) );
                else
                    tmps.set( STD_ASTR( "{\"" CDET_PUBLIC_ID "\":\"" ) );
                tmps.append( idstr ).append_char( '\"' );


                if (const char *cinfo = get_conn_info(cl.tox, address.as_public_key().key ))
                    tmps.append( STD_ASTR( ",\"" CDET_CONN_INFO "\":\"" ) ).append( std::asptr(cinfo) ).append_char( '\"' );

            }

            if ( !dnsname.is_empty() )
                tmps.append( STD_ASTR(",\"" CDET_DNSNAME "\":\"")).append(dnsname).append_char( '\"' );

            tmps.append( STD_ASTR(",\"" CDET_CLIENT_CAPS "\":["));
            for (std::token<char> bbsupported(bbcodes_supported, ','); bbsupported; ++bbsupported)
                tmps.append(STD_ASTR("\"bb")).append(bbsupported->as_sptr()).append(STD_ASTR("\","));
            tmps.append( STD_ASTR( "\"tox\"]" ) );

            if (!is_conference())
                tmps.append( STD_ASTR( ",\"" CDET_CLIENT "\":\"" ) ).append( clientname ).append_char( '\"' );

            tmps.append_char( '}' );

            cd.mask |= CDM_DETAILS;
            cd.details = tmps.cstr();
            cd.details_len = tmps.get_length();

            contact_descriptor_s *desc = const_cast<contact_descriptor_s *>(this);
            SETFLAG(desc->flags, F_DETAILS_SENT);
        }

    }

    std::string get_details_pubid( bool &zero_nospam ) const
    {
        std::string s;
        if ( is_conference() )
        {
            if (const byte *bid = address.get( tox_address_c::TAT_CONFERENCE_ID ))
            {
                s.append_as_hex( bid, TOX_CONFERENCE_UID_SIZE );

                byte hash[crypto_shorthash_BYTES];
                crypto_shorthash( hash, bid, TOX_CONFERENCE_UID_SIZE, (const byte *)"tox-chat-tox-chat" /* at least 16 bytes hash key */ );

                s.append_char( cip ? '+' : '-' );
                s.append_as_hex( hash, 4 );
            }
            return s;
        }

        if (const byte *bid = address.get( tox_address_c::TAT_FULL_ADDRESS ))
            s.append_as_hex( bid, TOX_ADDRESS_SIZE), zero_nospam = false;
        else if (const byte *bid1 = address.get( tox_address_c::TAT_PUBLIC_KEY ))
        {
            auto address_checksum = []( uint8_t *store, const uint8_t *address, uint32_t len )
            {
                uint8_t checksum[2] = {};
                for (uint32_t i = 0; i < len; ++i) {
                    checksum[i % 2] ^= address[i];
                }
                memcpy( store, checksum, sizeof( uint16_t ) );
            };

            uint8_t addr[TOX_ADDRESS_SIZE] = {};
            memcpy( addr, bid1, TOX_PUBLIC_KEY_SIZE );
            address_checksum( addr + TOX_ADDRESS_SIZE - sizeof( uint16_t ), addr, TOX_ADDRESS_SIZE - sizeof( uint16_t ) );

            s.append_as_hex( addr, TOX_ADDRESS_SIZE );
            zero_nospam = true;
        }
        return s;
    }

    bool is_conference() const { return fid.is_confa(); }
    fid_s get_fid() const { return fid; }


    contact_id_s get_id( bool init_flags ) const
    {
        if (init_flags)
        {
            contact_id_s x = id;
            if (is_conference())
                x.audio = is_audio_conference();
            if (CS_UNKNOWN == state)
                x.confmember = true, x.unknown = true;
            return x;
        }
        return id;
    };
    void set_fid(fid_s fid_);
    void set_id(contact_id_s id_);

    void send_viewsize(int w, int h)
    {
        if (0 != (ccc_caps & CCC_VIEW_SIZE))
        {
            ASSERT(w >= 0 && h >= 0);

            std::sstr_t<-128> viewinfo("-viewsize/");
            viewinfo.append_as_int(w).append_char('/').append_as_int(h);
            *(byte *)viewinfo.str() = PACKETID_EXTENSION;
            tox_friend_send_lossless_packet(cl.tox, fid.normal(), (const byte *)viewinfo.cstr(), viewinfo.get_length(), nullptr);
        }
    }


    void send_avatar()
    {
        if (!is_online()) return;
        if (state == CS_UNKNOWN || !fid.is_normal()) return;
        if (ISFLAG(flags, F_AVASEND)) return;
        if (avatag_self == cl.gavatag) return;

        std::string fnc; fnc.append_as_hex( cl.avahash, TOX_HASH_LENGTH ).append(STD_ASTR(".png"));

        TOX_ERR_FILE_SEND er = TOX_ERR_FILE_SEND_NULL;
        uint32_t tr = tox_file_send(cl.tox, fid.normal(), FT_AVATAR, cl.gavatar.size(), cl.avahash, (const byte *)fnc.cstr(), fnc.get_length(), &er);
        if (er != TOX_ERR_FILE_SEND_OK)
            return;

        SETFLAG(flags, F_AVASEND);

        new transmitting_avatar_s(fid, tr, fnc); // not memleak

    }

    void recv_avatar()
    {
        if (avatar_recv_fnn < 0)
        {
            if (ISFLAG(flags, F_AVARECIVED))
                cl.hf->avatar_data(get_id(true),avatar_tag,avatar.data(), static_cast<int>(avatar.size()));
            return;
        }
        tox_file_control(cl.tox,get_fid().normal(),avatar_recv_fnn, TOX_FILE_CONTROL_RESUME, nullptr);
    }

    void set_conference_id();
    void setup_members_and_send( contact_data_s &cd );
    void update_members();

    void prepare_protodata(contact_data_s &cd, savebuffer &cdata);
    void update_contact(bool data);

    bool is_audio_conference() const
    {
        if (!is_conference()) return false;
        bool av = TOX_CONFERENCE_TYPE_AV == tox_conference_get_type(cl.tox, fid.confa(), nullptr);
        if (nullptr == cip && av)
        {
            const_cast<contact_descriptor_s *>(this)->prepare_call();
            cip->remote_so.options |= SO_RECEIVING_AUDIO | SO_SENDING_AUDIO;
        }
        return av;
    }
};

void tox_c::restore_core_contacts()
{
    for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
    {
        if (f->is_conference())
            continue;
        f->set_fid(f->restore_tox_friend());
    }
}

void tox_c::add_to_callstate(contact_descriptor_s *d)
{
    auto w = callstate.lock_write();
    LIST_ADD(d, w().first, w().last, prev_call, next_call);
}


contact_descriptor_s *contact_descriptor_s::first_desc = nullptr;
contact_descriptor_s *contact_descriptor_s::last_desc = nullptr;

static std::unordered_map<contact_id_s, contact_descriptor_s* > id2desc;
static std::unordered_map<fid_s, contact_descriptor_s*> fid2desc;

static contact_descriptor_s * find_descriptor(contact_id_s cid);
static contact_descriptor_s * find_descriptor(fid_s fid);

void contact_descriptor_s::set_conference_id()
{
    int idi = cl.hf->find_free_id();
    id = contact_id_s(contact_id_s::CONFERENCE, idi);
    id2desc[ id ] = this;
}

static void delete_all_descs()
{
    id2desc.clear();
    fid2desc.clear();
    for ( ; contact_descriptor_s::first_desc;)
        contact_descriptor_s::first_desc->die();
}

void incoming_avatar_s::on_tick(int ct)
{
    bool die = true;
    if (contact_descriptor_s *desc = find_descriptor(fid))
        die = desc->avatar_recv_fnn != static_cast<int>(fnn);
    if (!die && (ct - droptime) > 0) die = true;
    if (die)
    {
        tox_file_control(cl.tox,fid.normal(),fnn,TOX_FILE_CONTROL_CANCEL, nullptr);
        delete this;
    }
}

/*virtual*/ void incoming_avatar_s::finished()
{
    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        byte hash[TOX_HASH_LENGTH];
        tox_hash(hash,chunk.data(), chunk.size());

        if (0 != memcmp(hash, desc->avatar_hash, TOX_HASH_LENGTH))
        {
            memcpy(desc->avatar_hash, hash, TOX_HASH_LENGTH);
            ++desc->avatar_tag; // new avatar version
        }

        desc->avatar = std::move(chunk);
        SETFLAG(desc->flags, contact_descriptor_s::F_AVARECIVED);

        cl.hf->avatar_data(desc->get_id(true), desc->avatar_tag, desc->avatar.data(), static_cast<int>(desc->avatar.size()));
    }

    delete this;
}



/*virtual*/ void transmitting_avatar_s::kill()
{
    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        UNSETFLAG(desc->flags, contact_descriptor_s::F_AVASEND);
    }
    super::kill();
}

/*virtual*/ void transmitting_avatar_s::finished()
{
    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        UNSETFLAG(desc->flags, contact_descriptor_s::F_AVASEND);
        desc->avatag_self = avatag;
    }
    super::finished();
}

void message2send_s::try_send(int time)
{
    if ((time - next_try_time) > 0)
    {
        bool send_create_time = false;
        bool support_msg_chain = false;
        if (fid.is_normal())
            if (contact_descriptor_s *desc = find_descriptor(fid))
            {
                if (!desc->is_online())
                {
                    mid = 0;
                    return; // not yet
                }
                send_create_time = create_time != 0 && ISFLAG(desc->ccc_caps, CCC_MSG_CRTIME);
                support_msg_chain = ISFLAG(desc->ccc_caps, CCC_MSG_CHAIN);
            }

        std::asptr m = msg.as_sptr();
        if (!send_create_time)
        {
            int skipchars = std::pstr_c(m).find_pos('\2');
            if (skipchars >= 0)
                m = m.skip(skipchars + 1);
        }
        if (!support_msg_chain && std::pstr_c(m).ends(STD_ASTR("\1\1")))
            m = m.trim(2);

        mid = 0;
        if (fid.is_confa())
        {
            // to conference
            if (tox_conference_send_message(cl.tox, fid.confa(), TOX_MESSAGE_TYPE_NORMAL, (const byte *)m.s, m.l, nullptr ))
            {
                sentmsg.set(m);
                mid = 777777777;
            }

        } else
        {
            mid = tox_friend_send_message(cl.tox, fid.normal(), TOX_MESSAGE_TYPE_NORMAL, (const byte *)m.s, m.l, nullptr);
        }
        if (mid) send_timeout = time + 60000;
        next_try_time = time + 20000;
    }
}

contact_descriptor_s::contact_descriptor_s(idgen_e init_new_id, fid_s fid_) :fid( fid_ )
{
    LIST_ADD(this, first_desc, last_desc, prev, next);

    if (init_new_id != SKIP_ID_GEN)
    {
        int idi = cl.hf->find_free_id();
        id = contact_id_s(init_new_id == ID_CONFERENCE ? contact_id_s::CONFERENCE : contact_id_s::CONTACT, idi);
        id2desc[id] = this;
        if (ID_UNKNOWN == init_new_id)
            fid.setup_new_unknown();

        if (fid.is_valid())
            fid2desc[fid] = this;
    }
}

void contact_descriptor_s::die()
{
    if (cip)
    {
        auto w = cl.callstate.lock_write();
        LIST_DEL(this, w().first, w().last, prev_call, next_call);
        delete cip;
        cip = nullptr;
        if (cl.tox && !is_conference())
            toxav_call_control(cl.toxav, get_fid().normal(), TOXAV_CALL_CONTROL_CANCEL, nullptr);
    }

    if (!id.is_self())
        id2desc.erase(id);
    if (fid.is_valid())
        fid2desc.erase(fid);
    delete this;
}

void contact_descriptor_s::set_fid(fid_s fid_)
{
    if (fid_ != fid)
    {
        if (fid.is_valid())
            fid2desc.erase(fid);

        fid = fid_;
        if (fid.is_valid())
            fid2desc[fid] = this;
    }
}
void contact_descriptor_s::set_id(contact_id_s id_)
{
    if (id != id_)
    {
        id2desc.erase(id);
        id = id_;
        if (!id.is_self()) id2desc[id] = this;
    }
}

static bool zero_online()
{
    if (!cl.tox) return false;
    for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
        if (f->is_online())
            return false;
    return true;
}

void operator<<(chunk &chunkm, const contact_descriptor_s &desc)
{
    if ((!desc.get_fid().is_valid() && desc.state != CS_INVITE_RECEIVE)
        || desc.state == contact_state_check || desc.state == CS_UNKNOWN || desc.state == CS_ROTTEN
        || desc.is_conference() || desc.get_fid().is_unknown())
        return; // tox protocol does not support unknown contacts... and no persistent conferences supported too

    chunk mm(chunkm.b, chunk_descriptor);

    chunk(chunkm.b, chunk_descriptor_id) << desc.get_id(false);
    chunk(chunkm.b, chunk_descriptor_address) << desc.address.as_bytes();
    chunk(chunkm.b, chunk_descriptor_dnsname) << desc.dnsname;
    chunk(chunkm.b, chunk_descriptor_state) << static_cast<i32>(desc.state);
    chunk(chunkm.b, chunk_descriptor_avatartag) << desc.avatar_tag;
    if (desc.avatar_tag != 0)
    {
        chunk(chunkm.b, chunk_descriptor_avatarhash) << bytes(desc.avatar_hash, TOX_HASH_LENGTH);
    }
    chunk(chunkm.b, chunk_descriptor_invmsg) << desc.invitemsg;
    
}

void contact_descriptor_s::prepare_protodata(contact_data_s &cd, savebuffer &cdata)
{
    chunk s(cdata);
    s << *this;
    cd.data = cdata.data();
    cd.data_size = static_cast<int>(cdata.size());
    cd.mask |= CDM_DATA;
}

void contact_descriptor_s::update_contact(bool data)
{
    if ( state == contact_state_check )
        return;

    if (cl.tox)
    {
        contact_data_s cd( get_id(true), CDM_PUBID | CDM_STATE | CDM_ONLINE_STATE | CDM_AVATAR_TAG );

        TOX_ERR_FRIEND_QUERY err = TOX_ERR_FRIEND_QUERY_NULL;
        TOX_USER_STATUS st = fid.is_normal() ? tox_friend_get_status(cl.tox, get_fid().normal(), &err) : static_cast<TOX_USER_STATUS>(-1);
        if (err != TOX_ERR_FRIEND_QUERY_OK) st = static_cast<TOX_USER_STATUS>(-1);

        std::sstr_t<max_t<TOX_PUBLIC_KEY_SIZE, TOX_CONFERENCE_UID_SIZE>::value * 2 + 16> pubid;

        if (is_conference())
        {
            cd.public_id_len = address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
            cd.id.audio = is_audio_conference();
        }
        else
            cd.public_id_len = address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );

        cd.public_id = pubid.cstr();

        cd.avatar_tag = avatar_tag;

        std::string name, statusmsg;

        if (st >= static_cast<TOX_USER_STATUS>(0))
        {
            TOX_ERR_FRIEND_QUERY er;
            name.set_length(static_cast<int>(tox_friend_get_name_size(cl.tox, fid.normal(), &er)));
            tox_friend_get_name(cl.tox, fid.normal(), (byte*)name.str(), &er);
            cd.name = name.cstr();
            cd.name_len = name.get_length();

            statusmsg.set_length(static_cast<int>(tox_friend_get_status_message_size(cl.tox, fid.normal(), &er)));
            tox_friend_get_status_message(cl.tox, fid.normal(), (byte*)statusmsg.str(), &er);
            cd.status_message = statusmsg.cstr();
            cd.status_message_len = statusmsg.get_length();

            cd.mask |= CDM_NAME | CDM_STATUSMSG;
        }

        if (st < static_cast<TOX_USER_STATUS>(0) || state != CS_OFFLINE)
        {
            cd.state = state;
            st = TOX_USER_STATUS_NONE;

        } else
        {
            if (fid.is_normal())
                cd.state = tox_friend_get_connection_status(cl.tox, fid.normal(), nullptr) != TOX_CONNECTION_NONE ? CS_ONLINE : CS_OFFLINE;
            else
                cd.state = CS_UNKNOWN;
        }
        
        switch (st)
        {
            case TOX_USER_STATUS_NONE:
                cd.ostate = COS_ONLINE;
                break;
            case TOX_USER_STATUS_AWAY:
                cd.ostate = COS_AWAY;
                break;
            case TOX_USER_STATUS_BUSY:
                cd.ostate = COS_DND;
                break;
        }

        std::string details_json_string;
        prepare_details(details_json_string, cd);

        savebuffer cdata;
        if (data)
            prepare_protodata(cd, cdata);

        cl.hf->update_contact(&cd);
    } else
    {
        contact_data_s cd(get_id(true), CDM_STATE);
        cd.state = CS_OFFLINE;
        cl.hf->update_contact(&cd);
    }
}

std::asptr message_part_s::extract_create_time(u64 &t, const std::asptr &msgbody, contact_id_s /*cid*/)
{
    int offset = 0;
    if (msgbody.s[0] == '\1')
    {
        offset = std::CHARz_findn(msgbody.s, '\2', msgbody.l);
        if (offset < 0)
        {
            offset = 1;
        }
        else {
            ++offset;
            t = std::pstr_c(std::asptr(msgbody.s + 1, offset - 2)).as_num<u64>();
        }
    }

    if (t == 0) t = now();
    return msgbody.skip(offset);
}

static contact_descriptor_s * find_descriptor(contact_id_s cid)
{
    auto it = id2desc.find(cid);
    if (it != id2desc.end())
        return it->second;
    return nullptr;
}

static contact_descriptor_s * find_descriptor(fid_s fid)
{
    auto it = fid2desc.find(fid);
    if (it != fid2desc.end())
        return it->second;
    return nullptr;
}

static void restore_descriptor(int tox_fid)
{
    fid_s fid = fid_s::make_normal(tox_fid);

    if (!cl.tox || !tox_friend_exists(cl.tox,fid.normal())) return;

    tox_address_c a;
    a.setup_public_key(fid);

    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        ASSERT( a == desc->address );
        return;
    }

    contact_descriptor_s *desc = new contact_descriptor_s(ID_CONTACT, fid);

    desc->address = a;
    desc->state = CS_OFFLINE;

    ASSERT( contact_descriptor_s::find(desc->address) == desc ); // check that such pubid is single

    contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE | CDM_NAME | CDM_STATUSMSG );
    
    std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
    cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
    cd.public_id = pubid.cstr();

    std::string name, statusmsg;
    TOX_ERR_FRIEND_QUERY er;
    name.set_length(static_cast<int>(tox_friend_get_name_size(cl.tox, fid.normal(), &er)));
    tox_friend_get_name(cl.tox, fid.normal(), (byte*)name.str(), &er);
    cd.name = name.cstr();
    cd.name_len = name.get_length();

    statusmsg.set_length(static_cast<int>(tox_friend_get_status_message_size(cl.tox, fid.normal(), &er)));
    tox_friend_get_status_message(cl.tox, fid.normal(), (byte*)statusmsg.str(), &er);
    cd.status_message = statusmsg.cstr();
    cd.status_message_len = statusmsg.get_length();

    savebuffer cdata;
    desc->prepare_protodata(cd, cdata);

    cd.state = CS_OFFLINE;
    cl.hf->update_contact(&cd);
}

static fid_s find_tox_unknown_contact(const public_key_s &id, const std::asptr &name)
{
    contact_descriptor_s *desc = contact_descriptor_s::find(id);

    if (desc == nullptr)
    {
        ASSERT(name.l);
        desc = new contact_descriptor_s(ID_UNKNOWN);
        desc->state = CS_UNKNOWN;
        desc->address = id;
    }

    if (name.l)
    {
        contact_data_s cd(desc->get_id(true), CDM_NAME | CDM_STATE | CDM_PUBID );
        cd.name = name.s;
        cd.name_len = name.l;
        std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
        cd.public_id = pubid.cstr();
        cd.state = CS_UNKNOWN;

        std::string details_json_string;
        desc->prepare_details(details_json_string, cd);

        cl.hf->update_contact(&cd);
    }
    
    return desc->get_fid();
}

static fid_s find_tox_fid(const public_key_s &id, bool strong_check)
{
    u32 r = tox_friend_by_public_key(cl.tox, id.key, nullptr);
    if ( r == UINT32_MAX )
    {
        if (strong_check)
            return fid_s();

        if ( contact_descriptor_s *desc = contact_descriptor_s::find( id ) )
            if (desc->get_fid().is_valid())
                return desc->get_fid();

        return fid_s();
    }
    return fid_s::make_normal(r);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cb_friend_request(Tox *, const byte *id, const byte *msg, size_t length, void *)
{
    const public_key_s &pk = *(const public_key_s *)id;
    contact_descriptor_s *desc = contact_descriptor_s::find(pk);
    if (!desc) desc = new contact_descriptor_s( ID_CONTACT );
    desc->address = pk;
    desc->state = CS_INVITE_RECEIVE;

    fid_s fid = find_tox_fid(pk);
    desc->set_fid( fid );

    contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE );

    //PUBKEY
    std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
    cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
    cd.public_id = pubid.cstr();
    cd.state = desc->state;

    savebuffer cdata;
    desc->prepare_protodata(cd, cdata);

    cl.hf->update_contact(&cd);

    time_t create_time = now();

    cl.hf->message(MT_FRIEND_REQUEST, contact_id_s(), desc->get_id(true), create_time, (const char *)msg, static_cast<int>(length));
    cl.hf->save();
}

static void cb_friend_message(Tox *, uint32_t tox_fid, TOX_MESSAGE_TYPE type, const byte *message, size_t length, void *)
{
    fid_s fid = fid_s::make_normal(tox_fid);

    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        for (auto it = cl.other_typing.begin(); it != cl.other_typing.end(); ++it)
        {
            if (it->fid == fid)
            {
                cl.other_typing.erase(it);
                break;
            }
        }

        if ( TOX_MESSAGE_TYPE_NORMAL == type )
            message_part_s::msg(desc->get_id(true), 0, (const char *)message, static_cast<int>(length));
        else
        {
            // TODO
        }
    }
}

static void cb_name_change(Tox *, uint32_t tox_fid, const byte * newname, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        contact_data_s cd(desc->get_id(true), CDM_NAME);
        cd.name = (const char *)newname;
        cd.name_len = static_cast<int>(length);
        cl.hf->update_contact(&cd);
    }
}

static void cb_status_message(Tox *, uint32_t tox_fid, const byte * newstatus, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        contact_data_s cd( desc->get_id(true), CDM_STATUSMSG );
        cd.status_message = (const char *)newstatus;
        cd.status_message_len = static_cast<int>(length);

        cl.hf->update_contact(&cd);
    }
}

static void cb_friend_status(Tox *, uint32_t tox_fid, TOX_USER_STATUS status, void *)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        desc->state = CS_OFFLINE;
        contact_data_s cd(desc->get_id(true), CDM_ONLINE_STATE);
        switch (status)
        {
            case TOX_USER_STATUS_NONE:
                cd.ostate = COS_ONLINE;
                break;
            case TOX_USER_STATUS_AWAY:
                cd.ostate = COS_AWAY;
                break;
            case TOX_USER_STATUS_BUSY:
                cd.ostate = COS_DND;
                break;
        }
        cl.hf->update_contact(&cd);
    }
}

static void cb_read_receipt(Tox *, uint32_t tox_fid, uint32_t message_id, void *)
{
    message2send_s::read(fid_s::make_normal(tox_fid), message_id);
}

static std::asptr bb_tags[] = { STD_ASTR("u"), STD_ASTR("i"), STD_ASTR("b"), STD_ASTR("s") };

static uint32_t cb_isotoxin_special(Tox *, uint32_t tox_fid, uint8_t *packet, uint32_t len, uint32_t /*max_len*/)
{
    switch (packet[0])
    {
    case PACKET_ID_NICKNAME:
    case PACKET_ID_STATUSMESSAGE:
    //case PACKET_ID_MESSAGE:
    //case PACKET_ID_ACTION:
        if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
        {
            std::pstr_c s(std::asptr((const char *)packet+1,len-1) );
            std::sstr_t<32> bopen(STD_ASTR("[")), bclose(STD_ASTR("[/"));
            std::string fixed;
            for( const std::asptr & bb : bb_tags )
            {
                bool supported = false;
                for (std::token<char> bbsupported(desc->bbcodes_supported,','); bbsupported; ++bbsupported )
                    if (bbsupported->equals(bb))
                    {
                        supported = true;
                        break;
                    }
                if (supported)
                    continue;

                bopen.set_length(1).append( bb ).append_char(']');
                if (s.find_pos(bopen) >= 0)
                {
                    if (fixed.is_empty())
                        fixed = s.as_sptr();

                    bclose.set_length(2).append(bb).append_char(']');
                    fixed.replace_all( bopen, STD_ASTR("") );
                    fixed.replace_all( bclose, STD_ASTR("") );
                    s.set( fixed.as_sptr() );
                }
            }
            if ( fixed.get_length() )
            {
                memcpy( packet + 1, fixed.cstr(), fixed.get_length() );
                return (u32)fixed.get_length() + 1;
            }
        }
        break;
    }
    return len;
}

static void cb_isotoxin(Tox *, uint32_t tox_fid, const byte *data, size_t len, void * /*object*/)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        switch (*data)
        {
        case PACKETID_EXTENSION:
            {
                std::token<char> t(std::asptr((const char *)data + 1, (int)len - 1), '/');
                if (!t) break;


                if (t->equals(STD_ASTR("viewsize")))
                {
                    if (desc->cip)
                    {
                        ++t;
                        int w = t ? t->as_uint() : 0;
                        ++t;
                        int h = t ? t->as_uint() : 0;
                        if (w > 65536) w = 0;
                        if (h > 65536) h = 0;

                        if (desc->cip->remote_so.view_w != w || desc->cip->remote_so.view_h != h)
                        {
                            desc->cip->remote_so.view_w = w;
                            desc->cip->remote_so.view_h = h;
                            cl.hf->av_stream_options(contact_id_s(), desc->get_id(true), &desc->cip->remote_so);
                        }
                    }
                } else if (t->equals(STD_ASTR("initvdecoder")))
                {
                    if (desc->cip)
                    {
                        desc->cip->cfg_dec.w = 0;
                        desc->cip->cfg_dec.h = 0;
                        if ( desc->cip->decoder )
                        {
                            vpx_codec_destroy( &desc->cip->v_decoder );
                            desc->cip->decoder = false;
                        }
                        ++t;
                        desc->cip->cfg_dec.w = t ? t->as_uint() : 0;
                        ++t;
                        desc->cip->cfg_dec.h = t ? t->as_uint() : 0;
                        ++t;
                        desc->cip->decoder_vp8 = !t || t->equals( STD_ASTR( "vp8" ) );
                    }
                } else if (t->equals(STD_ASTR("fshctl")))
                {
                    ++t;
                    int index;
                    u64 utag = t->as_num<u64>();
                    if (tox_c::fsh_ptr_s *shp = cl.find_fsh(utag, index))
                    {
                        if (shp->sf->cid == desc->get_id(false))
                        {
                            ++t;
                            folder_share_control_e ctl = static_cast<folder_share_control_e>(t->as_int());
                            switch (ctl)
                            {
                            case FSC_ACCEPT:
                                // send toc
                                if (shp->sf->toc_size > 0)
                                {
                                    std::string fn(STD_ASTR("toc/"));
                                    fn.append_as_num(utag);
                                    transmitting_blob_s::start(fn, desc->get_fid(), FT_TOC, shp->sf->to_data(), shp->sf->toc_size);
                                    shp->clear_toc(); // no need to after send on accept
                                }

                                break;
                            case FSC_REJECT:
                                shp->sf->die(index);
                                break;
                            }
                            cl.hf->folder_share_ctl(utag, ctl);
                            break;
                        }
                    }

                } else if (t->equals(STD_ASTR("fsha")))
                {
                    ++t;
                    int index;
                    u64 utag = t->as_num<u64>();

                    if (nullptr == cl.find_fsh(utag, index))
                    {
                        ++t;
                        cl.hf->message(MT_FOLDER_SHARE_ANNOUNCE, contact_id_s(), desc->get_id(true), utag, t->as_sptr().s, t->as_sptr().l);
                        cl.sfs.emplace_back(utag, desc->get_id(false), t->as_sptr());
                    }

                } else if (t->equals(STD_ASTR("fshq")))
                {
                    ++t;
                    int index;
                    u64 utag = t->as_num<u64>();
                    if (tox_c::fsh_ptr_s *shp = cl.find_fsh(utag, index))
                    {
                        if (shp->sf->cid == desc->get_id(false))
                        {
                            auto doesc = [](std::string &s)
                            {
                                for (int i = 0; i < s.get_length(); ++i)
                                    if (s.get_char(i) == '\1')
                                        s.set_char(i, '/');
                            };


                            ++t;
                            std::string tocname(*t); doesc(tocname);
                            ++t;
                            std::string fakename(*t); doesc(fakename);

                            cl.hf->folder_share_query(utag, tocname.cstr(), tocname.get_length(), fakename.cstr(), fakename.get_length());
                        }
                    }
                }
            }
            break;
        case PACKETID_VIDEO_EX:
            if (desc->cip)
            {
                if (0 != (desc->cip->local_so.options & SO_RECEIVING_VIDEO) && 0 != (desc->cip->remote_so.options & SO_SENDING_VIDEO))
                    desc->cip->video_packet(data + 1, len - 1);
            }
            break;
        }
    }
}

static void cb_connection_status(Tox *, uint32_t tox_fid, TOX_CONNECTION connection_status, void *)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        bool prev_online = desc->is_online();
        bool accepted = desc->state == CS_INVITE_SEND;
        desc->state = CS_OFFLINE;
        contact_data_s cd(desc->get_id(true), CDM_STATE);
        cd.state = (TOX_CONNECTION_NONE != connection_status) ? CS_ONLINE : CS_OFFLINE;
        SETUPFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE, CS_ONLINE == cd.state );

        std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        if (accepted)
        {
            cd.mask |= CDM_PUBID;

            //PUBKEY
            cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
            cd.public_id = pubid.cstr();

            UNSETFLAG( desc->flags, contact_descriptor_s::F_DETAILS_SENT );
        }

        if (!prev_online && desc->is_online() || accepted)
        {
            if (const char * clidcaps = (const char *)tox_friend_get_client_caps(cl.tox, tox_fid))
            {
                for (std::token<char> t(std::asptr(clidcaps), '\n'); t; ++t)
                {
                    std::token<char> ln(*t, ':');
                    if (ln->equals(STD_ASTR("client")))
                    {
                        ++ln;
                        if (!desc->clientname.equals(*ln))
                        {
                            desc->clientname = ln->as_sptr();
                            UNSETFLAG( desc->flags, contact_descriptor_s::F_DETAILS_SENT );
                        }
                    } else if (ln->equals(STD_ASTR("support_viewsize")))
                    {
                        ++ln;
                        if (ln->as_uint((unsigned)-1) == 1)
                            desc->ccc_caps |= CCC_VIEW_SIZE;
                    } else if (ln->equals(STD_ASTR("support_msg_chain")))
                    {
                        ++ln;
                        if (ln->as_uint((unsigned)-1) == 1)
                            desc->ccc_caps |= CCC_MSG_CHAIN;
                    } else if (ln->equals(STD_ASTR("support_msg_cr_time")))
                    {
                        ++ln;
                        if (ln->as_uint((unsigned)-1) == 1)
                            desc->ccc_caps |= CCC_MSG_CRTIME;
                    } else if (ln->equals(STD_ASTR("support_bbtags")))
                    {
                        ++ln;
                        desc->bbcodes_supported = ln->as_sptr();
                    } else if (ln->equals(STD_ASTR("support_video_ex")))
                    {
                        ++ln;
                        if (ln->as_uint((unsigned)-1) == 1)
                            desc->ccc_caps |= CCC_VIDEO_EX;
                    } else if (ln->equals(STD_ASTR("support_folder_share")))
                    {
                        ++ln;
                        if (ln->as_uint((unsigned)-1) == 1)
                            desc->ccc_caps |= CCC_FOLDER_SHARE;
                    }
                }

            }

            desc->send_avatar();

        } else if (!desc->is_online())
        {
            desc->on_offline();
        }

        cd.mask |= CDM_CAPS;
        cd.caps = desc->get_caps();

        std::string details_json_string;
        desc->prepare_details(details_json_string, cd);

        savebuffer cdata;
        if (accepted)
            desc->prepare_protodata(cd, cdata);

        cl.hf->update_contact(&cd);
        if (accepted)
            cl.hf->save();
    }
}

static void cb_file_chunk_request(Tox *, uint32_t tox_fid, uint32_t file_number, u64 position, size_t length, void *)
{
    fid_s fid = fid_s::make_normal(tox_fid);
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->fid == fid && f->fnn == file_number)
        {
            f->toxcore_request(position, length);
            break;
        }
}

static void cb_tox_file_recv(Tox *, uint32_t tox_fid, uint32_t filenumber, uint32_t kind, u64 filesize, const byte *filename, size_t filename_length, void *)
{
    fid_s fid = fid_s::make_normal(tox_fid);
    if (contact_descriptor_s *desc = find_descriptor(fid))
    {
        if (FT_AVATAR == kind)
        {
            if (desc->avatar_recv_fnn >= 0)
                desc->avatar_recv_fnn = -1;

            if (0 == filesize)
            {
                tox_file_control(cl.tox, tox_fid, filenumber, TOX_FILE_CONTROL_CANCEL, nullptr);

                memset(desc->avatar_hash, 0, TOX_HASH_LENGTH);
                if (desc->avatar_tag == 0) return;

                // avatar changed to none, so just send empty avatar to application (avatar tag == 0)

                desc->avatar_tag = 0;
                contact_data_s cd(desc->get_id(true), CDM_AVATAR_TAG);
                cd.avatar_tag = 0;

                savebuffer cdata;
                desc->prepare_protodata(cd, cdata);

                cl.hf->update_contact(&cd);
                cl.hf->save();

            } else
            {
                byte hash[TOX_HASH_LENGTH] = {};
                if (!tox_file_get_file_id(cl.tox, tox_fid,filenumber,hash,nullptr) || 0 == memcmp(hash, desc->avatar_hash, TOX_HASH_LENGTH))
                {
                    // same avatar - cancel avatar transfer
                    tox_file_control(cl.tox, tox_fid, filenumber, TOX_FILE_CONTROL_CANCEL, nullptr);

                } else
                {
                    // avatar changed
                    memcpy(desc->avatar_hash, hash, TOX_HASH_LENGTH);
                    ++desc->avatar_tag; // new avatar version

                    desc->avatar_recv_fnn = filenumber;
                    new incoming_avatar_s(fid, filenumber, filesize); // not memleak

                    contact_data_s cd(desc->get_id(true), CDM_AVATAR_TAG);
                    cd.avatar_tag = desc->avatar_tag;

                    savebuffer cdata;
                    desc->prepare_protodata(cd, cdata);

                    cl.hf->update_contact(&cd);
                    cl.hf->save();
                }
            }
            return;
        }

        if (FT_TOC == kind)
        {
            byte hash[TOX_HASH_LENGTH] = {};
            bool recvstarted = false;
            if (tox_file_get_file_id(cl.tox, tox_fid, filenumber, hash, nullptr))
            {
                std::token<char> t(std::asptr((const char *)filename, static_cast<int>(filename_length)), '/');
                if (t->equals(STD_ASTR("toc")))
                {
                    ++t;
                    u64 utag = t->as_num<u64>();
                    int index;
                    if (tox_c::fsh_ptr_s *shp = cl.find_fsh(utag, index))
                    {
                        struct incoming_toc_s : public incoming_blob_s
                        {
                            u64 fsutag;
                            incoming_toc_s(u64 fsutag, fid_s fid_, u32 fnn_, u64 fsz_) : incoming_blob_s(fid_, fnn_, fsz_), fsutag(fsutag)
                            {
                                tox_file_control(cl.tox, fid_.normal(), fnn_, TOX_FILE_CONTROL_RESUME, nullptr);
                            }

                            /*virtual*/ void finished() override
                            {
                                cl.hf->folder_share(fsutag, chunk.data(), static_cast<int>(chunk.size())); // toc
                                delete this;
                            }
                        };

                        new incoming_toc_s(utag, fid, filenumber, filesize); // not memleak
                        recvstarted = true;
                    }

                }
            }

            if (!recvstarted)
                tox_file_control(cl.tox, tox_fid, filenumber, TOX_FILE_CONTROL_CANCEL, nullptr);

            return;
        }

        incoming_file_s *f = new incoming_file_s(fid, filenumber, filesize); // not memleak
        cl.hf->incoming_file(desc->get_id(true), f->utag, filesize, (const char *)filename, static_cast<int>(filename_length));
    }

}

static void cb_file_recv_control(Tox *, uint32_t tox_fid, uint32_t filenumber, TOX_FILE_CONTROL control, void * /*userdata*/)
{
    fid_s fid = fid_s::make_normal(tox_fid);
    file_transfer_s *ft = nullptr;
    // outgoing
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->fid == fid && f->fnn == filenumber)
        {
            ft = f;
            break;
        }

    // incoming
    if (nullptr == ft)
    {
        for(incoming_file_s *f = incoming_file_s::first;f;f=f->next)
            if (f->fid == fid && f->fnn == filenumber)
            {
                ft = f;
                break;
            }
    }
    
    if (!ft) return;

    switch (control)
    {
    case TOX_FILE_CONTROL_RESUME:
        ft->accepted();
        break;

    case TOX_FILE_CONTROL_CANCEL:
        ft->kill();
        break;

    case TOX_FILE_CONTROL_PAUSE:
        ft->pause();
        break;

    }
}

static void cb_tox_file_recv_chunk(Tox *, uint32_t tox_fid, uint32_t filenumber, u64 position, const byte *data, size_t length, void * /*userdata*/)
{
    fid_s fid = fid_s::make_normal(tox_fid);
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->fid == fid && f->fnn == filenumber)
        {
            f->check_count = 0;

            if (length == 0)
                f->finished();
            else
                f->recv_data( position, data, length );
            break;
        }
}

static void cb_friend_typing(Tox *, uint32_t tox_fid, bool is_typing, void * /*userdata*/)
{
    fid_s fid = fid_s::make_normal(tox_fid);
    for( auto it = cl.other_typing.begin(); it != cl.other_typing.end(); ++it )
    {
        if (it->fid == fid)
        {
            if (!is_typing)
                cl.other_typing.erase(it);
            return;
        }
    }

    if (is_typing)
        cl.other_typing.emplace_back( fid, time_ms() );
}

void contact_descriptor_s::update_members()
{
    TOX_ERR_CONFERENCE_PEER_QUERY e;
    int num = tox_conference_peer_count(cl.tox, fid.confa(), &e );

    if (e != TOX_ERR_CONFERENCE_PEER_QUERY_OK)
        num = 0;

    for (int m = 0; m < num; ++m)
    {
        TOX_ERR_CONFERENCE_PEER_QUERY pq;
        public_key_s member_pubkey;
        tox_conference_peer_get_public_key(cl.tox, fid.confa(), m, member_pubkey.key, &pq );
        if (pq != TOX_ERR_CONFERENCE_PEER_QUERY_OK)
            continue;

        if (cl.lastmypubid == member_pubkey)
            continue; // do not put self to members list

        if (contact_descriptor_s *desc = contact_descriptor_s::find( member_pubkey ))
        {
            UNSETFLAG( desc->flags, contact_descriptor_s::F_DETAILS_SENT );

            if ( desc->state == CS_UNKNOWN )
            {
                std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;

                contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_DETAILS );
                cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
                cd.public_id = pubid.cstr();

                std::string details_json_string;
                desc->prepare_details( details_json_string, cd );

                cl.hf->update_contact(&cd);
            } else
                desc->update_contact(true);
        }
    }
}


void contact_descriptor_s::setup_members_and_send(contact_data_s &cd)
{
    TOX_ERR_CONFERENCE_PEER_QUERY e;
    cd.members_count = tox_conference_peer_count(cl.tox, fid.confa(), &e);
    
    if ( e != TOX_ERR_CONFERENCE_PEER_QUERY_OK )
        cd.members_count = 0;

    cd.members = (contact_id_s *)_alloca( cd.members_count * sizeof(contact_id_s) ); //-V630

    int mm = 0;
    std::string name;
    for (int m = 0; m < cd.members_count; ++m)
    {
        TOX_ERR_CONFERENCE_PEER_QUERY pq;
        public_key_s member_pubkey;
        tox_conference_peer_get_public_key(cl.tox, fid.confa(), m, member_pubkey.key, &pq);
        if (pq != TOX_ERR_CONFERENCE_PEER_QUERY_OK)
            continue;

        if (cl.lastmypubid == member_pubkey)
            continue; // do not put self to members list

        cd.mask |= CDM_STATE;
        cd.state = CS_ONLINE;
        state = CS_ONLINE;

        size_t nsz = tox_conference_peer_get_name_size(cl.tox, fid.confa(), m, nullptr );
        name.set_length( static_cast<int>( nsz ), false );
        if (!tox_conference_peer_get_name(cl.tox, fid.confa(), m, (uint8_t *)name.str(), nullptr ) )
            name.clear();

        contact_id_s cid;
        fid_s mfid = find_tox_fid( member_pubkey );
        bool disablenamesend = false;
        if (!mfid.is_valid()) // negative means not found
            mfid = find_tox_unknown_contact( member_pubkey, name ), disablenamesend = true;

        if (contact_descriptor_s *desc = find_descriptor( mfid ))
        {
            cid = desc->get_id(true);
            if ((CS_UNKNOWN == desc->state && !disablenamesend) || CS_INVITE_RECEIVE == desc->state || CS_INVITE_SEND == desc->state)
            {
                // wow, we know name of invited friend!

                contact_data_s cdn(cid, CDM_NAME);

                cdn.name = name.cstr();
                cdn.name_len = name.get_length();
                cl.hf->update_contact(&cdn);
            }
        }

        if (cid.is_self())
            continue;

        cd.members[mm++] = cid;
    }

    cd.members_count = mm;
    cd.mask |= CDM_PUBID;

    //CONFAID
    std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
    cd.public_id_len = address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
    cd.public_id = pubid.cstr();

    cd.id.audio = is_audio_conference();

    cl.hf->update_contact(&cd);
}

static void callback_av_conference_audio(void *, int gnum, int peernum, const int16_t *pcm, unsigned int samples, uint8_t channels, unsigned int sample_rate, void * /*userdata*/)
{
    public_key_s pubkey;
    tox_conference_peer_get_public_key(cl.tox, gnum, peernum, pubkey.key, nullptr);
    if (cl.lastmypubid == pubkey)
        return; // ignore self message

    if (contact_descriptor_s *gdesc = find_descriptor( fid_s::make_confa(gnum) ))
    {
        contact_id_s cid;
        fid_s fid = find_tox_fid(pubkey);
        if (!fid.is_valid())
            fid = find_tox_unknown_contact(pubkey, std::asptr());

        if (contact_descriptor_s *desc = find_descriptor(fid))
            cid = desc->get_id(true);

        media_data_s mdt;
        mdt.afmt.sample_rate = sample_rate;
        mdt.afmt.bits = 16;
        mdt.afmt.channels = channels;
        mdt.audio_frame = pcm;
        mdt.audio_framesize = static_cast<int>(samples) * channels * mdt.afmt.bits / 8;

        cl.hf->av_data(gdesc->get_id(true), cid, &mdt);
    }
}

static void cb_conference_invite(Tox *, uint32_t fid, TOX_CONFERENCE_TYPE t, const uint8_t * data, size_t length, void * )
{
    int gnum = -1;
    if ( TOX_CONFERENCE_TYPE_TEXT == t )
    {
        TOX_ERR_CONFERENCE_JOIN jr;
        gnum = tox_conference_join(cl.tox, fid, data, length, &jr );
        if ( jr != TOX_ERR_CONFERENCE_JOIN_OK )
            gnum = -1;

    } else
    {
        gnum = toxav_join_av_groupchat(cl.tox, fid, data, static_cast<uint16_t>( length ), callback_av_conference_audio, nullptr );
    }
    if (gnum >= 0)
    {
        std::string gn;
        TOX_ERR_CONFERENCE_TITLE gte;
        size_t l = tox_conference_get_title_size(cl.tox, gnum, &gte );
        if ( gte == TOX_ERR_CONFERENCE_TITLE_OK && l )
        {
            gn.set_length( static_cast<int>( l ) );
            tox_conference_get_title(cl.tox, gnum, (uint8_t *)gn.str(), nullptr );
        }

        conference_id_s gid;
        CHECK(tox_conference_get_uid(cl.tox, gnum, gid.id ));

        contact_descriptor_s *desc = nullptr;
        for ( contact_descriptor_s *d = contact_descriptor_s::first_desc; d; d = d->next )
        {
            if ( d->address == gid )
            {
                d->set_fid( fid_s::make_confa(gnum) );
                desc = d;
                if ( desc->cip )
                    desc->stop_call();
                break;
            }
        }

        if ( !desc )
        {
            desc = new contact_descriptor_s( ID_CONFERENCE, fid_s::make_confa(gnum));
            desc->address = gid;
            desc->state = CS_OFFLINE;
        }

        if ( t == TOX_CONFERENCE_TYPE_AV )
        {
            desc->prepare_call();
            desc->cip->remote_so.options |= SO_RECEIVING_AUDIO | SO_SENDING_AUDIO;
        }

        contact_data_s cd(desc->get_id(true), CDM_PUBID | CDM_STATE | CDM_NAME | CDM_MEMBERS | CDM_PERMISSIONS );
        if ( desc->state != CS_ONLINE )
            cd.state = CS_OFFLINE;
        cd.name = gn.cstr();
        cd.name_len = gn.get_length();
        cd.conference_permissions = -1;

        std::string details_json_string;
        desc->prepare_details( details_json_string, cd );

        desc->setup_members_and_send(cd);

        if (t == TOX_CONFERENCE_TYPE_AV )
            cl.hf->av_stream_options( desc->get_id(true), contact_id_s(), &desc->cip->remote_so );
    }
}

static void conference_message( int gnum, int peernum, const char * message, int length, bool is_message )
{
    public_key_s pubkey;
    tox_conference_peer_get_public_key(cl.tox, gnum, peernum, pubkey.key, nullptr);

    if (cl.lastmypubid == pubkey)
    {
        // confirm delivery
        message2send_s::read(std::asptr( message, length ) );
        return;
    }

    if (contact_descriptor_s *gdesc = find_descriptor(fid_s::make_confa(gnum)))
    {
        contact_id_s cid;
        fid_s fid = find_tox_fid(pubkey);
        if (!fid.is_valid())
            fid = find_tox_unknown_contact(pubkey, std::asptr());

        if (contact_descriptor_s *desc = find_descriptor(fid))
            cid = desc->get_id(true);

        if ( is_message )
            cl.hf->message(MT_MESSAGE, gdesc->get_id(true), cid, now(), message, length);
        else
        {
            // TODO: MT_ACTION
        }
    }
}

static void cb_conference_message(Tox *, uint32_t gnum, uint32_t peernum, TOX_MESSAGE_TYPE t, const uint8_t * message, size_t length, void *)
{
    conference_message( gnum, peernum, (const char *)message, static_cast<int>( length ), t == TOX_MESSAGE_TYPE_NORMAL );
}

static void cb_conference_namelist_change(Tox *, uint32_t gnum, uint32_t /*pid*/, TOX_CONFERENCE_STATE_CHANGE /*change*/, void *)
{
    // any change - rebuild members list
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_confa(gnum)))
    {
        contact_data_s cd(desc->get_id(true), CDM_MEMBERS);
        desc->setup_members_and_send(cd);
    }
}

static void cb_conference_title(Tox *, uint32_t gnum, uint32_t /*pid*/, const uint8_t * title, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_confa(gnum)))
    {
        std::string gn( (const char *)title, static_cast<int>( length ) );

        contact_data_s cd(desc->get_id(true), CDM_NAME | CDM_PUBID);
        cd.name = gn.cstr();
        cd.name_len = gn.get_length();

        //CONFAID
        std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
        cd.public_id = pubid.cstr();

        cl.hf->update_contact( &cd );
        cl.hf->save();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox AV callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cb_toxav_incoming_call(ToxAV *av, uint32_t tox_fid, bool audio_enabled, bool video_enabled, void * /*user_data*/)
{
    if (av != cl.toxav) return;
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        if (nullptr != desc->cip)
        {
            toxav_call_control(cl.toxav, tox_fid, TOXAV_CALL_CONTROL_CANCEL, nullptr);
            return;
        }

        desc->prepare_call();

        SETFLAG( desc->cip->remote_so.options, SO_RECEIVING_AUDIO | SO_RECEIVING_VIDEO );
        SETUPFLAG( desc->cip->remote_so.options, SO_SENDING_AUDIO, audio_enabled );
        SETUPFLAG( desc->cip->remote_so.options, SO_SENDING_VIDEO, video_enabled );

        if (video_enabled)
            cl.hf->message(MT_INCOMING_CALL, contact_id_s(), desc->get_id(true), now(), "video", 5);
        else
            cl.hf->message(MT_INCOMING_CALL, contact_id_s(), desc->get_id(true), now(), "audio", 5);
    }
}

static void cb_toxav_call_state(ToxAV *av, uint32_t tox_fid, uint32_t state, void * /*user_data*/)
{
    if (av != cl.toxav) return;

    if (TOXAV_FRIEND_CALL_STATE_ERROR == state || TOXAV_FRIEND_CALL_STATE_FINISHED == state)
    {
        if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
            desc->stop_call();
        return;
    }

    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        if (!desc->cip) return;

        SETUPFLAG(desc->cip->remote_so.options, SO_SENDING_AUDIO, 0 != (state & TOXAV_FRIEND_CALL_STATE_SENDING_A));
        SETUPFLAG(desc->cip->remote_so.options, SO_SENDING_VIDEO, 0 != (state & TOXAV_FRIEND_CALL_STATE_SENDING_V));
        SETUPFLAG(desc->cip->remote_so.options, SO_RECEIVING_AUDIO, 0 != (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A));
        SETUPFLAG(desc->cip->remote_so.options, SO_RECEIVING_VIDEO, 0 != (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V));

        if (!desc->cip->started && state != 0)
        {
            // start call
            desc->cip->started = true;
            desc->cip->calling = false;
            cl.hf->message(MT_CALL_ACCEPTED, contact_id_s(), desc->get_id(true), now(), nullptr, 0);

            // WORKAROUND
            // signal to other peer that we always accept video, even audio call
            //toxav_call_control(toxav, desc->get_fid(), 0 == (desc->cip->local_so.options & SO_RECEIVING_VIDEO) ? TOXAV_CALL_CONTROL_HIDE_VIDEO : TOXAV_CALL_CONTROL_SHOW_VIDEO, nullptr);
        }

        if (!ISFLAG(desc->cip->remote_so.options, SO_SENDING_AUDIO) && desc->cip->video_ex)
            desc->cip->current_recv_frame = -1;

        cl.hf->av_stream_options(contact_id_s(), desc->get_id(true), &desc->cip->remote_so);
    }
}

static void cb_toxav_audio(ToxAV *av, uint32_t tox_fid, const int16_t *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void * /*user_data*/)
{
    if (av != cl.toxav) return;
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        if (desc->cip == nullptr)
            return;

        media_data_s mdt;
        mdt.afmt = audio_format_s(sampling_rate,channels,16);
        mdt.audio_frame = pcm;
        mdt.audio_framesize = static_cast<int>(sample_count) * mdt.afmt.channels * mdt.afmt.bits / 8;

        cl.hf->av_data(contact_id_s(), desc->get_id(true), &mdt);
    }
}

static void cb_toxav_video(ToxAV *av, uint32_t tox_fid, uint16_t width,
                                          uint16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v,
                                          int32_t ystride, int32_t ustride, int32_t vstride, void * /*user_data*/)
{
    if (av != cl.toxav) return;
    if (contact_descriptor_s *desc = find_descriptor(fid_s::make_normal(tox_fid)))
    {
        media_data_s mdt;
        mdt.vfmt.width = width;
        mdt.vfmt.height = height;
        mdt.vfmt.pitch[0] = static_cast<unsigned short>(ystride);
        mdt.vfmt.pitch[1] = static_cast<unsigned short>(ustride);
        mdt.vfmt.pitch[2] = static_cast<unsigned short>(vstride);
        mdt.vfmt.fmt = VFMT_I420;
        mdt.video_frame[0] = y;
        mdt.video_frame[1] = u;
        mdt.video_frame[2] = v;

        cl.hf->av_data(contact_id_s(), desc->get_id(true), &mdt );
    }
}

static void audio_sender()
{
    cl.callstate.lock_write()().audio_sender = true;

    byte_buffer prebuffer; // just ones allocated buffer for raw audio
    
    const int audio_frame_duration = 60;

    int sleepms = 100;
    int timeoutcountdown = 50; // 5 sec
    for( ;; Sleep(sleepms))
    {
        auto w = cl.callstate.lock_write();
        w().audio_sender_heartbeat = true;
        if (w().shutdown)
        {
            w().audio_sender = false;
            return;
        }
        if (nullptr == w().first)
        {
            --timeoutcountdown;
            if (timeoutcountdown <= 0)
            {
                w().audio_sender = false;
                return;
            }
            sleepms = 100;
            continue;
        }
        timeoutcountdown = 50;
        sleepms = 1;
        int ct = time_ms();
        for(contact_descriptor_s *d = w().first;d;d=d->next_call)
        {
            stream_settings_s &ss = d->cip->local_settings;
            aint avsize = ss.fifo.available();
            if (avsize == 0)
            {
                ss.processing = false;
                continue;
            }

            if ((ss.next_time_send - ct) > 0 && (ss.next_time_send - ct) <= audio_frame_duration)
                continue;

            audio_format_s fmt = ss;
            int req_frame_size = fmt.avgBytesPerMSecs(audio_frame_duration);
            if (req_frame_size > avsize) 
                continue; // not yet

            if (!ss.processing && (req_frame_size * 3) > avsize)
                continue;

            ss.processing = true;
            ss.next_time_send += audio_frame_duration;
            if ((ss.next_time_send - ct) < 0) ss.next_time_send = ct + audio_frame_duration/2;

            if (req_frame_size >(int)prebuffer.size()) prebuffer.resize(req_frame_size);
            aint samples = ss.fifo.read_data(prebuffer.data(), req_frame_size) / fmt.sampleSize();
            int remote_so = d->cip->remote_so.options;

            if (0 == (remote_so & SO_RECEIVING_AUDIO))
                continue;

            fid_s fid = d->get_fid();
            ++ss.locked;
            w.unlock(); // no need lock anymore

            ASSERT( fmt.bits == 16 );

            if (fid.is_confa())
            {
                toxav_group_send_audio(cl.tox, fid.confa(), (int16_t *)prebuffer.data(), (int)samples, (byte)fmt.channels, fmt.sample_rate );

            } else
            {
                toxav_audio_send_frame(cl.toxav, fid.normal(), (int16_t *)prebuffer.data(), (int)samples, (byte)fmt.channels, fmt.sample_rate, nullptr );
            }
            w = cl.callstate.lock_write();
            --ss.locked;
            if (w().shutdown)
                return;

            bool ok = false;
            for (contact_descriptor_s *dd = w().first; dd; dd = dd->next_call)
                if (dd == d)
                {
                    ok = true;
                    break;
                }
            if (!ok) break;

        }
        if (w.is_locked()) w.unlock();
    }
}

static void video_encoder()
{
    cl.callstate.lock_write()().video_encoder = true;

    int sleepms = 100;
    int timeoutcountdown = 50; // 5 sec
    for (;; Sleep(sleepms))
    {
        auto w = cl.callstate.lock_write();
        w().video_encoder_heartbeat = true;
        if (w().shutdown)
        {
            w().video_encoder = false;
            return;
        }
        if (nullptr == w().first)
        {
            --timeoutcountdown;
            if (timeoutcountdown <= 0)
            {
                w().video_encoder = false;
                return;
            }
            sleepms = 100;
            continue;
        }
        timeoutcountdown = 50;
        sleepms = 1;
        for (contact_descriptor_s *d = w().first; d; d = d->next_call)
        {
            stream_settings_s &ss = d->cip->local_settings;
            
            if (nullptr == ss.video_data)
                continue;

            bool video_ex = d->cip->video_ex;
            fid_s fid = d->get_fid();
            const uint8_t *y = (const uint8_t *)ss.video_data;
            ss.video_data = nullptr;

            ++ss.locked;
            w.unlock(); // no need lock anymore

            // encoding here
            int ysz = ss.video_w * ss.video_h;
            if (video_ex && d->cip->vquality >= 0)
            {
                // isotoxin video ex transfer
                d->cip->isotoxin_video_send_frame(fid.normal(), ss.video_w, ss.video_h, y, y + ysz, y + (ysz + ysz / 4));
            
            } else
            {
                // toxcore video transfer
                toxav_video_send_frame(cl.toxav, fid.normal(), (uint16_t)ss.video_w, (uint16_t)ss.video_h, y, y + ysz, y + (ysz + ysz / 4), nullptr);
                d->cip->vquality = cl.video_quality;
            }
            cl.hf->free_video_data(y);

            w = cl.callstate.lock_write();
            --ss.locked;
            if (w().shutdown)
                return;

            bool ok = false;
            for (contact_descriptor_s *dd = w().first; dd; dd = dd->next_call)
                if (dd == d)
                {
                    ok = true;
                    break;
                }
            if (!ok) break;

        }
        if (w.is_locked()) w.unlock();
    }
}

static void video_sender()
{
    cl.callstate.lock_write()().video_sender = true;

    int sleepms = 100;
    int timeoutcountdown = 50; // 5 sec
    for (;; Sleep(sleepms))
    {
        auto w = cl.callstate.lock_write();
        w().video_sender_heartbeat = true;
        if (w().shutdown)
        {
            w().video_sender = false;
            return;
        }
        if (nullptr == w().first)
        {
            --timeoutcountdown;
            if (timeoutcountdown <= 0)
            {
                w().video_sender = false;
                return;
            }
            sleepms = 100;
            continue;
        }
        timeoutcountdown = 50;
        sleepms = 1;
        for (contact_descriptor_s *d = w().first; d; d = d->next_call)
        {
            stream_settings_s &ss = d->cip->local_settings;

            if (!d->cip->video_ex || nullptr == d->cip->current)
                continue;

            spinlock::simple_lock(d->cip->sync_frame);

            if (d->cip->current->is_done())
            {
                auto *t = d->cip->current;
                d->cip->current = d->cip->next;
                d->cip->next = t;
            }

            if (nullptr == d->cip->current || d->cip->current->is_done())
            {
                d->cip->current_busy = false;
                spinlock::simple_unlock(d->cip->sync_frame);
                continue;
            }

            d->cip->current_busy = true;
            spinlock::simple_unlock(d->cip->sync_frame);

            fid_s fid = d->get_fid();

            ++ss.locked;
            w.unlock(); // no need lock anymore

            // send video ex data

            contact_descriptor_s::call_in_progress_s::sending_frame_s * f = d->cip->current;
            f->dummy_byte[sizeof(int) - 1] = PACKETID_VIDEO_EX;
            f->sframe = htonl( f->frame );
            f->soffset = htonl( f->offset ? f->offset : (-f->size) );
            const uint8_t *d1 = f->dummy_byte + (sizeof(int) - 1);
            const int d1sz = sizeof(int) * 2 + 1;
            const uint8_t *d2 = ((const uint8_t *)(f+1)) + f->offset;
            int d2sz = f->size - f->offset;
            if (d2sz > TOX_MAX_CUSTOM_PACKET_SIZE - d1sz) d2sz = TOX_MAX_CUSTOM_PACKET_SIZE - d1sz;

            if (tox_friend_send_lossless_packet2(cl.tox, fid.normal(), d1, d1sz, d2, d2sz))
                f->offset += d2sz;

            if (f->is_done())
                d->cip->current_busy = false; // safe to reset this flag without locking due busy means exclusive access

            w = cl.callstate.lock_write();
            --ss.locked;
            if (w().shutdown)
                return;

            bool ok = false;
            for (contact_descriptor_s *dd = w().first; dd; dd = dd->next_call)
                if (dd == d)
                {
                    ok = true;
                    break;
                }
            if (!ok) break;

        }
        if (w.is_locked()) w.unlock();
    }
}

void tox_c::tick(int *sleep_time_ms)
{
#ifdef _DEBUG
    if (fake_offline_off)
    {
        if (fake_offline_off < now())
            fake_offline_off = 0, restart_module = true;
    }
#endif


    if (restart_module)
    {
        if (tox) tox_kill(tox);
        tox = nullptr;
        *sleep_time_ms = -1;
        return;
    }

    int curt = time_ms();
    static int nextt = curt;
    static int nexttav = curt;
    static time_t tryconnect = now() + 60;

    if (!online_flag)
    {
        nextt = curt - 10000;
        nexttav = curt - 10000;
        tryconnect = now() - 60;

        *sleep_time_ms = 100;
        self_typing_contact.clear();
        return;
    }
    *sleep_time_ms = 1;
    if (tox)
    {
        // self typing
        // may be time to stop typing?
        if (!self_typing_contact.is_empty() && (curt-self_typing_time) > 0)
        {
            if (contact_descriptor_s *cd = find_descriptor(self_typing_contact))
                tox_self_set_typing(tox, cd->get_fid().normal(), false, nullptr);
            self_typing_contact.clear();
        }

        // other peers typing
        // notify host
        for(other_typing_s &ot : other_typing)
        {
            if ( (curt-ot.time) > 0 )
            {
                if (contact_descriptor_s *desc = find_descriptor(ot.fid))
                    hf->typing(contact_id_s(), desc->get_id(true));
                ot.time += 1000;
            }
        }
        for (aint i= other_typing.size() -1;i>=0;--i)
        {
            other_typing_s &ot = other_typing[ i ];
            if ( ( curt - ot.time ) > 60000 )
            {
                // moar then minute!
                other_typing.erase( other_typing.begin() + i );
                break;
            }
        }

        message2send_s::tick(curt);
        message_part_s::tick(curt);
        incoming_file_s::tick(curt);
        transmitting_data_s::tick();
        discoverer_s::tick(curt);

        bool forceupdateself = false;
        if ((curt - nextt) > 0)
        {
            tox_iterate(tox, nullptr);
            nextt = curt + tox_iteration_interval(tox);


            tox_address_c temp; temp.setup_self_address();
            if (lastmypubid != temp)
                forceupdateself = true;

            if ((curt - next_check_accept_nodes_time) > 0)
            {
                next_check_accept_nodes_time = time_ms() + 60000;
                if (accepted_nodes < 4 || zero_online())
                    ++zero_online_connect_cnt, connect();

                if (restart_on_zero_online && zero_online_connect_cnt > 10)
                {
                    restart_module = true;
                    MaskLog(LFLS_TIMEOUT, "Zero online, Restart... (%u)", GetCurrentProcessId());
                }
            }
            
        }
        bool on_offline = false;
        contact_state_e nst = tox_self_get_connection_status( tox ) != TOX_CONNECTION_NONE ? CS_ONLINE : CS_OFFLINE;

#ifdef _DEBUG
        if (fake_offline_off)
        {
            nst = CS_OFFLINE;
        }
#endif

        if (forceupdateself || nst != self_state)
        {
            if ( nst == CS_OFFLINE && self_state != CS_OFFLINE )
                tryconnect = now() + 10, on_offline = true;

            self_state = nst;

            if ( !on_offline ) // dont do update self on offline due do_on_offline_stuff will be called later
                update_self();
        }

        static int reconnect_try = 0;

        if ( on_offline )
            do_on_offline_stuff();

        if (nst == CS_OFFLINE)
        {
            time_t ctime = now();
            if (ctime > tryconnect)
            {
                connect();
                tryconnect = ctime + 10;

                MaskLog( LFLS_TIMEOUT, "Offline, Reconnect %i", reconnect_try );
                ++reconnect_try;
            }
            if (reconnect_try >= 10)
            {
                restart_module = true;
                MaskLog( LFLS_TIMEOUT, "Offline, Restart... (%u)", GetCurrentProcessId() );
            }
        } else
            reconnect_try = 0;

        if ((curt - nexttav) > 0)
        {
            toxav_iterate(toxav);
            nexttav = curt + toxav_iteration_interval(toxav);

            auto r = callstate.lock_read();
            if (r().first)
            {
                for (const contact_descriptor_s *f = r().first; f; f = f->next_call)
                {
                    if (f->cip->calling && (curt - f->cip->call_stop_time) > 0)
                    {
                        contact_id_s id = f->get_id(false);
                        r.unlock();
                        signal(id, CONS_STOP_CALL);
                        break;
                    }
                }
            }
            if (r.is_locked())
                r.unlock();
        }

        int nextticktime = min( nexttav - curt, nextt - curt );
        if (nextticktime <= 0) nextticktime = 1;
        *sleep_time_ms = nextticktime;
    }

    if (reconnect)
    {
        offline();
        online();
        hf->save();
        reconnect = false;
    }
}

void tox_c::goodbye()
{
    delete_all_descs();

    while(message2send_s::first)
        delete message2send_s::first;

    while (message_part_s::first)
        delete message_part_s::first;

    while (incoming_file_s::first)
        delete incoming_file_s::first;

    while (transmitting_data_s::first)
        delete transmitting_data_s::first;

    while (discoverer_s::first)
        delete discoverer_s::first;

    stop_senders();

    if (tox)
    {
        toxav_kill(toxav);
        toxav = nullptr;

        tox_kill(tox);
        tox = nullptr;
    }

    WSACleanup();
}

void tox_c::set_name(const char*utf8name)
{
    if (tox)
    {
        size_t len = strlen(utf8name);
        if (len == 0)
        {
            utf8name = "noname";
            len = 6;
        } else if (len > TOX_MAX_NAME_LENGTH)
            len = TOX_MAX_NAME_LENGTH;
        TOX_ERR_SET_INFO er;
        tox_self_set_name(tox,(const byte *)utf8name,len,&er);
    }
}
void tox_c::set_statusmsg(const char*utf8status)
{
    if (tox)
    {
        size_t len = strlen(utf8status);
        if (len > TOX_MAX_STATUS_MESSAGE_LENGTH)
            len = TOX_MAX_STATUS_MESSAGE_LENGTH;

        TOX_ERR_SET_INFO er;
        tox_self_set_status_message(tox, (const byte *)utf8status, len, &er);
    }
}

void tox_c::set_ostate(int ostate)
{
    if (tox)
    {
        switch (ostate)
        {
            case COS_ONLINE:
                tox_self_set_status(tox, TOX_USER_STATUS_NONE);
                break;
            case COS_AWAY:
                tox_self_set_status(tox, TOX_USER_STATUS_AWAY);
                break;
            case COS_DND:
                tox_self_set_status(tox, TOX_USER_STATUS_BUSY);
                break;
        }
    }
}
void tox_c::set_gender(int /*gender*/)
{
}

void tox_c::set_avatar(const void*data, int isz)
{
    static int prevavalen = 0;

    if (isz == 0 && prevavalen > 0)
    {
        prevavalen = 0;
        memset(avahash,0,sizeof(avahash));
        gavatar.clear();
        ++gavatag;
    } else if (tox)
    {
        byte newavahash[ TOX_HASH_LENGTH ];
        tox_hash(newavahash,(const byte *)data,isz);
        if (0 != memcmp(newavahash,avahash,TOX_HASH_LENGTH))
        {
            memcpy(avahash, newavahash, TOX_HASH_LENGTH);
            gavatar.resize(isz);
            memcpy(gavatar.data(), data, isz);
            ++gavatag;
        }
        prevavalen = isz;
    }
    
    for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
        f->send_avatar();

}

static cmd_result_e tox_err_to_cmd_result( TOX_ERR_NEW toxerr )
{
    switch (toxerr) //-V719
    {
        case TOX_ERR_NEW_OK:
            return CR_OK;

        case TOX_ERR_NEW_PORT_ALLOC:
        case TOX_ERR_NEW_PROXY_BAD_TYPE:
        case TOX_ERR_NEW_PROXY_BAD_HOST:
        case TOX_ERR_NEW_PROXY_BAD_PORT:
        case TOX_ERR_NEW_PROXY_NOT_FOUND:
            return CR_NETWORK_ERROR;
        case TOX_ERR_NEW_LOAD_BAD_FORMAT:
            return CR_CORRUPT;
        case TOX_ERR_NEW_LOAD_ENCRYPTED:
            return CR_ENCRYPTED;
    }
    return CR_UNKNOWN_ERROR;
}

static contact_id_s as_contactid(u32 v)
{
    if (v & 0xff000000)
    {
        contact_id_s id;
        *(u32 *)&id = v;
        return id;
    }
    return contact_id_s(contact_id_s::CONTACT, v);
}

static contact_descriptor_s * load_descriptor(contact_id_s id, loader &l)
{
    int descriptor_size = l(chunk_descriptor);
    if (descriptor_size == 0)
        return nullptr;

    loader lc(l.chunkdata(), descriptor_size);

    if (lc(chunk_descriptor_id))
    {
        contact_id_s idd = as_contactid(lc.get_u32());
        ASSERT(id.is_empty() || idd == id);
        id = idd;
    }

    if (id.is_conference() || id.is_self())
        return nullptr;

    cl.hf->use_id(id.id);

    tox_address_c address;

    if (int asz = lc(chunk_descriptor_address, false))
    {
        loader al(lc.chunkdata(), asz);
        int dsz;
        if (const void *toxid = al.get_data(dsz))
        {
            if (TOX_ADDRESS_SIZE == dsz)
                address.setup(toxid, tox_address_c::TAT_FULL_ADDRESS);
            else if (TOX_PUBLIC_KEY_SIZE == dsz)
                address.setup(toxid, tox_address_c::TAT_PUBLIC_KEY);
        }

    } else if (lc(chunk_descriptor_pubid))
        address.setup(lc.get_astr());

    contact_descriptor_s *desc = contact_descriptor_s::find(address);
    if (!desc) desc = new contact_descriptor_s(SKIP_ID_GEN);

    desc->address = address;

    if (lc(chunk_descriptor_dnsname))
        desc->dnsname.set(lc.get_astr());
    if (lc(chunk_descriptor_state))
        desc->state = static_cast<contact_state_e>(lc.get_i32());
    if (lc(chunk_descriptor_avatartag))
        desc->avatar_tag = lc.get_i32();

    if (desc->avatar_tag != 0)
    {
        if (int bsz = lc(chunk_descriptor_avatarhash))
        {
            loader hbl(lc.chunkdata(), bsz);
            int dsz;
            if (const void *mb = hbl.get_data(dsz))
                memcpy(desc->avatar_hash, mb, min(dsz, TOX_HASH_LENGTH));
        }
    }

    if (lc(chunk_descriptor_invmsg))
        desc->invitemsg.set(lc.get_astr());
    

    fid_s fid;
    if (desc->state != CS_UNKNOWN)
    {
        if (cl.tox)
        {
            fid = desc->restore_tox_friend();
        }

    } else
    {
        fid.setup_new_unknown();
    }


    if (!id.is_empty()) desc->set_id(id);
    desc->set_fid(fid);
    return desc;
}

void tox_c::set_config(const void*idata, int iisz)
{
    proxy_settings_ok = false;
    if ( iisz < 4 ) return;
    config_accessor_s ca( idata, iisz );

    bool setproto = false;
    if (ca.params.l)
    {
        parse_values(ca.params, [&](const std::pstr_c &field, const std::pstr_c &val)
        {

#define ASI(aa) if (field.equals(STD_ASTR(#aa))) { adv_set_##aa(val); return; }
            ADVSET
#undef ASI
            if (field.equals(STD_ASTR(CFGF_PROXY_TYPE)))
            {
                int new_proxy_type = val.as_int();
                if (new_proxy_type != tox_proxy_type)
                    tox_proxy_type = new_proxy_type, reconnect = true;
                proxy_settings_ok = true;
                return;
            }
            if (field.equals(STD_ASTR(CFGF_PROXY_ADDR)))
            {
                std::string paddr(tox_proxy_host.as_sptr()); paddr.append_char(':').append_as_uint(tox_proxy_port);
                if (!paddr.equals(val))
                {
                    set_proxy_addr(val);
                    reconnect = true;
                }
                proxy_settings_ok = true;
                return;
            }

            handle_cop< SETBIT(auto_co_ipv6_enable) | SETBIT(auto_co_udp_enable) >(field.as_sptr(), val.as_sptr(), [&](auto_conn_options_e co, bool v) {
                if (auto_co_ipv6_enable == co && options.ipv6_enabled != v)
                    options.ipv6_enabled = v, reconnect = true;
                else if (auto_co_udp_enable == co && options.udp_enabled != v)
                    options.udp_enabled = v, reconnect = true;
            });

            if (field.equals(STD_ASTR(CFGF_SETPROTO)))
            {
                setproto = val.as_int() != 0;
                return;
            }
        });
    }


    if ( !setproto && !ca.native_data && !ca.protocol_data )
        return;

    struct on_return
    {
        TOX_ERR_NEW toxerr = TOX_ERR_NEW_OK;
        ~on_return()
        {
            cl.hf->operation_result(LOP_SETCONFIG, tox_err_to_cmd_result(toxerr));
        }
        void operator=(TOX_ERR_NEW err)
        {
            toxerr = err;
        }
    } toxerr;

    conf_encrypted = false;

    delete_all_descs();

    if ( ca.native_data && ca.native_data_len > TOX_ENC_SAVE_MAGIC_LENGTH && !memcmp( TOX_ENC_SAVE_MAGIC_NUMBER, ca.native_data, TOX_ENC_SAVE_MAGIC_LENGTH ) )
    {
        toxerr = TOX_ERR_NEW_LOAD_ENCRYPTED;
        if ( tox ) tox_kill( tox );
        tox = nullptr;
        buf_tox_config.clear();
        conf_encrypted = true;

    } else if ( ca.native_data && ca.native_data_len > 8 && ( *(uint32_t *)ca.native_data ) == 0 && ( *( (uint32_t *)ca.native_data + 1 ) ) == 0x15ed1b1f )
    {
        if ( !proxy_settings_ok )
        {
            tox_proxy_type = 0;
            memset( &options, 0, sizeof( options ) );
            options.udp_enabled = true;
        }

        buf_tox_config.resize( ca.native_data_len );
        memcpy( buf_tox_config.data(), ca.native_data, ca.native_data_len );

        // raw tox_save
        toxerr = prepare(true);

        // import - restore descriptors
        aint cnt = tox_self_get_friend_list_size(tox);
        for (int fid = 0; fid < cnt; ++fid)
            restore_descriptor(fid);


    } else if (ca.protocol_data_len>4 && (*(uint32_t *)ca.protocol_data ) != 0)
    {
        loader ldr( ca.protocol_data, ca.protocol_data_len );

        if ( !proxy_settings_ok )
        {
            tox_proxy_type = 0;
            memset( &options, 0, sizeof( options ) );
            options.udp_enabled = true;
        }

        size_t version = 0;
        if (ldr(chunk_magic,false))
            version = static_cast<size_t>(ldr.get_u64() - 0x111BADF00D2C0FE6ull);

        if (ldr(chunk_proxy_type))
            tox_proxy_type = ldr.get_i32();
        if (ldr(chunk_proxy_address))
            set_proxy_addr(ldr.get_astr());
        if (ldr(chunk_server_port))
            options.tcp_port = static_cast<uint16_t>(ldr.get_i32());
        if (ldr(chunk_use_ipv6))
            options.ipv6_enabled = ldr.get_i32() != 0;
        if (ldr(chunk_use_udp))
            options.udp_enabled = ldr.get_i32() != 0;

        if (ldr(chunk_use_hole_punching, false))
            options.hole_punching_enabled = ldr.get_i32() != 0;

        if (ldr(chunk_use_local_discovery, false))
            options.local_discovery_enabled = ldr.get_i32() != 0;

        if (ldr(chunk_video_codec, false))
            use_vp9_codec = ldr.get_i32() != 0;

        if (ldr(chunk_video_quality, false))
            video_quality = ldr.get_i32();

        if (ldr(chunk_video_bitrate, false))
            video_bitrate = ldr.get_i32();

        if (ldr(chunk_restart_on_zero_online, false))
            restart_on_zero_online = ldr.get_i32() != 0;
        
        if (ldr(chunk_disable_video_ex, false))
            disabled_video_ex = ldr.get_i32() != 0;

        if (ldr(chunk_nodes_list_fn, false))
            adv_set_nodes_list_file( std::pstr_c(ldr.get_astr()) );
        else
            adv_set_nodes_list_file(std::pstr_c());
        
        if (ldr(chunk_pinned_list_fn, false))
            adv_set_pinned_list_file(std::pstr_c(ldr.get_astr()));
        else
            adv_set_pinned_list_file(std::pstr_c());


        if (int sz = ldr(chunk_toxid))
        {
            loader l(ldr.chunkdata(), sz);
            int dsz;
            if (const void *toxid = l.get_data( dsz ))
                if (TOX_ADDRESS_SIZE == dsz)
                    lastmypubid.setup( toxid, tox_address_c::TAT_FULL_ADDRESS );
        }

        if (int sz = ldr(chunk_tox_data))
        {
            loader l(ldr.chunkdata(), sz);
            int dsz;
            if (const void *toxdata = l.get_data(dsz))
            {
                buf_tox_config.resize(dsz);
                memcpy(buf_tox_config.data(), toxdata, dsz);

                toxerr = prepare(true);
            }

        } else if (!tox)
        {
            buf_tox_config.clear();
            toxerr = prepare(true); // prepare anyway
        }

        if (version == 1)
        if (int sz = ldr(chunk_descriptors))
        {
            loader l(ldr.chunkdata(), sz);
            for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                load_descriptor(contact_id_s(), l);

            aint cnt = tox_self_get_friend_list_size(tox);
            for (int fid = 0; fid < cnt; ++fid)
                restore_descriptor(fid);
        }

        if (int sz = ldr(chunk_nodes))
        {
            loader l(ldr.chunkdata(), sz);
            for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                load_node(l);
        }

    } else if (!tox)
    {
        buf_tox_config.clear();
        toxerr = prepare(true);
    }

    // now send configurable fields to application
    send_configurable();
}

void tox_c::init_done()
{
    update_self();

    if (tox)
    {
        if (tox_self_get_friend_list_size(tox))
        {
            // because Version 2 of save doesn't contain friends, so current is 1 and must be saved as 2

            for (contact_descriptor_s *desc = contact_descriptor_s::first_desc; desc; desc = desc->next)
            {
                if (desc->is_conference())
                    continue;

                contact_data_s cd(desc->get_id(true), 0);
                savebuffer cdata;
                desc->prepare_protodata(cd, cdata);
                hf->update_contact(&cd);
            }

            hf->save(); 
        }
    }
}

void tox_c::online()
{
    reset_accepted_nodes();

    if ( conf_encrypted )
    {
        hf->operation_result( LOP_ONLINE, CR_ENCRYPTED );
        return;
    }

    online_flag = true;

    if (!tox && buf_tox_config.size())
        prepare(false);

    restore_core_contacts();
    connect();
}

void tox_c::signal(contact_id_s cid, signal_e s)
{
    switch (s)
    {
    case APPS_INIT_DONE:
        init_done();
        break;
    case APPS_ONLINE:
        online();
        break;
    case APPS_OFFLINE:
        offline();
        break;
    case APPS_GOODBYE:
        goodbye();
        break;
    case REQS_EXPORT_DATA:
        if (tox)
        {
            size_t sz = tox_get_savedata_size(tox, 1);
            void *data = _alloca(sz);
            tox_get_savedata(tox, (byte *)data, 1);
            hf->export_data(data, static_cast<int>(sz));
        }
        else if (buf_tox_config.size())
        {
            hf->export_data(buf_tox_config.data(), static_cast<int>(buf_tox_config.size()));
        }
        else
        {
            hf->export_data(nullptr, 0);
        }
        break;

    case CONS_ACCEPT_INVITE:
        if (tox)
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *desc = it->second;

            if (desc->address.compatible(tox_address_c::TAT_PUBLIC_KEY) && !desc->get_fid().is_valid())
            {
                TOX_ERR_FRIEND_ADD er;
                int tox_fid = tox_friend_add_norequest(tox, desc->address.as_public_key().key, &er);
                if (TOX_ERR_FRIEND_ADD_OK != er)
                    return;
                desc->set_fid(fid_s::make_normal(tox_fid));
                desc->state = CS_OFFLINE;

                contact_data_s cd(desc->get_id(true), CDM_STATE);
                cd.state = CS_WAIT;

                savebuffer cdata;
                desc->prepare_protodata(cd, cdata);

                hf->update_contact(&cd);
                hf->save();
            }
        }

        break;
    case CONS_REJECT_INVITE:
        if (tox)
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *desc = it->second;
            desc->state = CS_ROTTEN;

            contact_data_s cd(desc->get_id(true), CDM_STATE);
            cd.state = CS_ROTTEN;
            hf->update_contact(&cd);

            if (desc->get_fid().is_normal())
                tox_friend_delete(tox, desc->get_fid().normal(), nullptr);

            desc->die();
            hf->save();
        }
        break;
    case CONS_ACCEPT_CALL:
        if (tox)
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *desc = it->second;

            if (nullptr == desc->cip)
                return; // not call

                        // desc->cip->local_so is initialized before accept

            desc->cip->started = toxav_answer(toxav, desc->get_fid().normal(),
                0 != (desc->cip->local_so.options & SO_SENDING_AUDIO) ? DEFAULT_AUDIO_BITRATE : 0,
                0 != (desc->cip->local_so.options & SO_SENDING_VIDEO) ? DEFAULT_VIDEO_BITRATE : 0, nullptr);

            // desc->cip->remote_so is initialized on incoming call callback

            hf->av_stream_options(contact_id_s(), desc->get_id(true), &desc->cip->remote_so);
        }
        break;
    case CONS_STOP_CALL:
        if (tox)
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *cd = it->second;
            if (nullptr == cd->cip)
                return; // not call

            toxav_call_control(toxav, cd->get_fid().normal(), TOXAV_CALL_CONTROL_CANCEL, nullptr);
            cd->stop_call();
        }
        break;
    case CONS_DELETE:
        if (tox && !cid.is_self())
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *desc = it->second;
            if (!desc->get_fid().is_valid())
            {
                desc->die();
                hf->save();
                return;
            }
            if (!desc->is_conference())
            {
                std::vector< contact_descriptor_s * > confas;
                size_t chatscount = tox_conference_get_chatlist_size(tox);
                if (chatscount)
                {
                    public_key_s mkey;
                    uint32_t *chats = (uint32_t *)_alloca(sizeof(uint32_t) * chatscount);
                    tox_conference_get_chatlist(tox, chats);
                    for (size_t i = 0; i < chatscount; ++i)
                    {
                        int gnum = chats[i];
                        int peers = tox_conference_peer_count(tox, gnum, nullptr);
                        for (int p = 0; p < peers; ++p)
                        {
                            tox_conference_peer_get_public_key(tox, gnum, p, mkey.key, nullptr);
                            if (desc->address == mkey)
                            {
                                auto itc = fid2desc.find(fid_s::make_confa(gnum));
                                if (itc != fid2desc.end())
                                {
                                    confas.push_back(itc->second);
                                    break;
                                }

                            }
                        }
                    }
                }

                tox_friend_delete(tox, desc->get_fid().normal(), nullptr);
                desc->die();
                hf->save();

                for (contact_descriptor_s *confa : confas)
                {
                    contact_data_s cd(confa->get_id(true), CDM_MEMBERS);
                    confa->setup_members_and_send(cd);
                }
            }
            else
            {
                tox_conference_delete(tox, desc->get_fid().n, nullptr);
                desc->die();
            }
        }

        break;
    case CONS_TYPING:
        if (cid.is_conference())
        {
            // oops. toxcore does not support conference typing notification... :(
            return;
        }

        if (contact_descriptor_s *cd = find_descriptor(cid))
            if (cd->is_online())
            {
                if (self_typing_contact.is_empty())
                {
                    self_typing_contact = cid;
                    if (tox) tox_self_set_typing(tox, cd->get_fid().normal(), true, nullptr);
                    self_typing_time = time_ms() + 1500;
                }
                else if (self_typing_contact == cid)
                    self_typing_time = time_ms() + 1500;
            }
        break;

    case REQS_DETAILS:
        if (tox && !cid.is_self())
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *desc = it->second;
            UNSETFLAG(desc->flags, contact_descriptor_s::F_DETAILS_SENT);

            if (desc->is_conference())
                desc->update_members();

            desc->update_contact(false);
        }
        break;
    case REQS_AVATAR:
        if (tox)
        {
            auto it = id2desc.find(cid);
            if (it == id2desc.end()) return;
            contact_descriptor_s *cd = it->second;
            cd->recv_avatar();
        }
        break;

    }
}

void tox_c::save_current_stuff( savebuffer &b )
{
    chunk(b, chunk_magic) << static_cast<u64>(0x111BADF00D2C0FE6ull + SAVE_VERSION);
    chunk(b, chunk_proxy_type) << tox_proxy_type;
    chunk(b, chunk_proxy_address) << ( std::string(tox_proxy_host.as_sptr()).append_char(':').append_as_uint(tox_proxy_port) );
    chunk(b, chunk_server_port) << static_cast<i32>(options.tcp_port);
    chunk(b, chunk_use_ipv6) << static_cast<i32>(options.ipv6_enabled ? 1 : 0);
    chunk(b, chunk_use_udp) << static_cast<i32>(options.udp_enabled ? 1 : 0);
    chunk(b, chunk_use_hole_punching) << static_cast<i32>(options.hole_punching_enabled ? 1 : 0);
    chunk(b, chunk_use_local_discovery) << static_cast<i32>(options.local_discovery_enabled ? 1 : 0);
    chunk(b, chunk_video_codec) << static_cast<i32>(use_vp9_codec ? 1 : 0);
    chunk(b, chunk_video_quality) << static_cast<i32>(video_quality);
    chunk(b, chunk_video_bitrate) << static_cast<i32>(video_bitrate);
    chunk(b, chunk_restart_on_zero_online) << static_cast<i32>(restart_on_zero_online ? 1 : 0);
    chunk(b, chunk_disable_video_ex) << static_cast<i32>(disabled_video_ex ? 1 : 0);
    chunk(b, chunk_nodes_list_fn) << nodes_fn;
    
    chunk(b, chunk_toxid) << lastmypubid.as_bytes();

    if (tox)
    {
        size_t sz = tox_get_savedata_size(tox,0);
        void *data = chunk(b, chunk_tox_data).alloc(sz);
        tox_get_savedata(tox, (byte *)data,0);

    } else if (buf_tox_config.size())
    {
        void *data = chunk(b, chunk_tox_data).alloc(buf_tox_config.size());
        memcpy(data, buf_tox_config.data(), buf_tox_config.size());
    }

    chunk(b, chunk_nodes) << servec<tox_node_s>(nodes);
}

void tox_c::do_on_offline_stuff()
{
    reset_accepted_nodes();
    stop_senders();

    if (tox)
    {
        for (; transmitting_data_s::first;)
        {
            transmitting_data_s *f = transmitting_data_s::first;
            hf->file_control(f->utag, FIC_DISCONNECT);
            delete f;
        }
        for (; incoming_file_s::first;)
        {
            incoming_file_s *f = incoming_file_s::first;
            hf->file_control(f->utag, FIC_DISCONNECT);
            delete f;
        }

        for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f;)
        {
            contact_descriptor_s *fnext = f->next;
            if (f->state == CS_UNKNOWN)
            {
                f->die();
                f = fnext;
                continue;
            }
            if (f->is_conference())
            {
                contact_data_s cd(f->get_id(true), CDM_STATE);
                cd.state = CS_OFFLINE;
                hf->update_contact(&cd);

                f->on_offline();
                f->die();
                f = fnext;
                continue;
            }
            if (f->state != CS_INVITE_RECEIVE && f->state != CS_INVITE_SEND)
                f->on_offline();
            if (!tox) f->set_fid(fid_s());

            contact_data_s cd(f->get_id(true), CDM_STATE);
            cd.state = CS_OFFLINE;
            hf->update_contact(&cd);

            f = fnext;
        }

        size_t sz = tox_get_savedata_size(tox, 0);
        buf_tox_config.resize(sz);
        tox_get_savedata(tox, buf_tox_config.data(), 0);
        prepare(false);
        if (tox) buf_tox_config.clear();

        contact_data_s self(contact_id_s::make_self(), CDM_STATE);
        self.state = CS_OFFLINE;
        hf->update_contact(&self);

    }
}

void tox_c::save_config(void * param)
{
    if (tox || buf_tox_config.size())
    {
        savebuffer b;
        save_current_stuff(b);
        hf->on_save(b.data(), (int)b.size(), param);
    }
}

int tox_c::send_request(const char *dnsname, const tox_address_c &toxaddr, const char *invite_message_utf8, bool resend)
{
    if (tox)
    {
        if (resend) // before resend we have to delete contact in Tox system, otherwise we will get an error TOX_FAERR_ALREADYSENT
            if (contact_descriptor_s *desc = contact_descriptor_s::find(toxaddr.as_public_key()))
                tox_friend_delete(tox, desc->get_fid().normal(), nullptr);

        std::string invmsg( utf8clamp(invite_message_utf8, TOX_MAX_FRIEND_REQUEST_LENGTH) );
        if (invmsg.get_length() == 0) invmsg.set( STD_ASTR("?") );

        TOX_ERR_FRIEND_ADD er = TOX_ERR_FRIEND_ADD_NULL;
        int tox_fid = tox_friend_add(tox, toxaddr.get(tox_address_c::TAT_FULL_ADDRESS), (const byte *)invmsg.cstr(), invmsg.get_length(), &er);
        switch (er) //-V719
        {
            case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
                return CR_ALREADY_PRESENT;
            case TOX_ERR_FRIEND_ADD_OWN_KEY:
            case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
                return CR_INVALID_PUB_ID;
            case TOX_ERR_FRIEND_ADD_MALLOC:
                return CR_MEMORY_ERROR;
        }

        contact_descriptor_s *desc = contact_descriptor_s::find( toxaddr.as_public_key() );
        if (desc) desc->set_fid(fid_s::make_normal(tox_fid));
        else desc = new contact_descriptor_s( ID_CONTACT, fid_s::make_normal(tox_fid));
        memset( desc->avatar_hash, 0, sizeof( desc->avatar_hash ) );
        desc->avatar_tag = 0;
        desc->dnsname = dnsname;
        desc->address = toxaddr;
        desc->invitemsg = invmsg;
        desc->state = CS_INVITE_SEND;

        contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE );

        //PUBID
        std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
        cd.public_id = pubid.cstr();

        cd.state = CS_INVITE_SEND;

        savebuffer cdata;
        desc->prepare_protodata(cd, cdata);

        hf->update_contact(&cd);

        return CR_OK;
    }
    return CR_FUNCTION_NOT_FOUND;
}

int tox_c::resend_request(contact_id_s id, const char* invite_message_utf8)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return CR_FUNCTION_NOT_FOUND;
        contact_descriptor_s *desc = it->second;
        return send_request( desc->dnsname, desc->address, invite_message_utf8, true);
    }
    return CR_FUNCTION_NOT_FOUND;
}

void tox_c::add_conference( TOX_CONFERENCE_TYPE t, const conference_id_s &id, const char *name )
{
    cb_conference_invite( tox, UINT32_MAX, t, id.id, TOX_CONFERENCE_UID_SIZE, nullptr );
    if (contact_descriptor_s *desc = contact_descriptor_s::find( id ))
    {
        int nlen = 0;
        if (name)
        {
            nlen = std::CHARz_len( name );
            tox_conference_set_title( tox, desc->get_fid().confa(), (const byte *)name, nlen, nullptr );
        }

        desc->state = CS_ONLINE;

        contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE | (name ? CDM_NAME : 0) );
        cd.state = CS_ONLINE;
        cd.name = name;
        cd.name_len = nlen;
        //CONFAID
        std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
        cd.public_id = pubid.cstr();

        hf->update_contact( &cd );
    }

}

void tox_c::contact(const contact_data_s * icd)
{
    if (icd->data_size)
    {
        loader ldr(icd->data, icd->data_size);
        if (contact_descriptor_s *desc = load_descriptor(icd->id, ldr))
        {
            contact_data_s cd(desc->get_id(true), CDM_STATE|CDM_PUBID);
            std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
            cd.public_id_len = desc->address.as_str(pubid, tox_address_c::TAT_PUBLIC_KEY);
            cd.public_id = pubid.cstr();

            cd.state = desc->state;
            hf->update_contact(&cd);
        }

    }
}

int tox_c::add_contact(const char* public_id, const char* invite_message_utf8)
{
    std::pstr_c s; s.set(std::asptr(public_id));

    if (s.get_length() == TOX_CONFERENCE_UID_SIZE * 2 + 1 + 8 && (s.get_char( TOX_CONFERENCE_UID_SIZE * 2 ) == '-' || s.get_char( TOX_CONFERENCE_UID_SIZE * 2 ) == '+'))
    {
        conference_id_s confaid;
        byte hash_check[4];
        s.hex2buf<TOX_CONFERENCE_UID_SIZE>( confaid.id );
        s.hex2buf<4>( hash_check, TOX_CONFERENCE_UID_SIZE * 2 + 1 );

        byte hash[crypto_shorthash_BYTES];
        crypto_shorthash( hash, confaid.id, TOX_CONFERENCE_UID_SIZE, (const byte *)"tox-chat-tox-chat" /* at least 16 bytes hash key */ );
        if ( !memcmp( hash, hash_check, 4 ) )
        {
            // yo! conference!
            // use invite_message_utf8 as name

            TOX_CONFERENCE_TYPE t = s.get_char( TOX_CONFERENCE_UID_SIZE * 2 ) == '-' ? TOX_CONFERENCE_TYPE_TEXT : TOX_CONFERENCE_TYPE_AV;
            add_conference(t, confaid, invite_message_utf8);
            return CR_OK;
        }
    }

    if (s.get_length() == (TOX_PUBLIC_KEY_SIZE * 2) && s.contain_chars( STD_ASTR( "0123456789abcdefABCDEF" ) ))
    {
        public_key_s pk;
        s.hex2buf<TOX_PUBLIC_KEY_SIZE>( pk.key );

        TOX_ERR_FRIEND_ADD er = TOX_ERR_FRIEND_ADD_NULL;
        int tox_fid = tox_friend_add_norequest( tox, pk.key, &er );
        switch (er) //-V719
        {
        case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
            return CR_ALREADY_PRESENT;
        case TOX_ERR_FRIEND_ADD_OWN_KEY:
        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
        case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
            return CR_INVALID_PUB_ID;
        case TOX_ERR_FRIEND_ADD_MALLOC:
            return CR_MEMORY_ERROR;
        }

        contact_descriptor_s *desc = contact_descriptor_s::find( pk );
        if (desc) desc->set_fid(fid_s::make_normal(tox_fid));
        else
        {
            desc = new contact_descriptor_s(ID_CONTACT, fid_s::make_normal(tox_fid));
            desc->address = pk;
        }
        memset( desc->avatar_hash, 0, sizeof( desc->avatar_hash ) );
        desc->avatar_tag = 0;
        desc->state = CS_OFFLINE;

        contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE );

        //PUBID
        std::sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_PUBLIC_KEY );
        cd.public_id = pubid.cstr();

        cd.state = CS_WAIT;
        hf->update_contact( &cd );
        return CR_OK;
    }

    if (s.get_length() == (TOX_ADDRESS_SIZE * 2) && s.contain_chars( STD_ASTR( "0123456789abcdefABCDEF" ) ))
    {
        tox_address_c toxaddr;
        toxaddr.setup(std::asptr( public_id, TOX_ADDRESS_SIZE * 2) );

        return send_request( "", toxaddr, invite_message_utf8, false );
    }

    if (s.find_pos('@') > 0)
    {
        new discoverer_s(s, std::asptr(invite_message_utf8));
        return CR_OPERATION_IN_PROGRESS;
    }

    return CR_INVALID_PUB_ID;
}

//message2send_s *gms = nullptr;

void tox_c::send_message(contact_id_s id, const message_s *msg)
{
#ifdef _DEBUG
    if (std::pstr_c(std::asptr(msg->message, msg->message_len)).begins(STD_ASTR("/fakeoffline")))
    {
        Sleep(3000);
        std::str_c x(std::asptr(msg->message, msg->message_len));
        x.cut( 0, 12 );
        x.trim();
        fake_offline_off = now() + x.as_int();
        hf->delivered( msg->utag );
        return;
    }
#endif // _DEBUG

    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;

        contact_descriptor_s *desc = it->second;

        if (id == self_typing_contact)
        {
            if (desc->get_fid().is_normal())
                tox_self_set_typing(tox, desc->get_fid().n, false, nullptr);

            self_typing_contact.clear();
        }
        /*gms =*/ new message2send_s( msg->utag, desc->get_fid(), std::asptr(msg->message, msg->message_len), msg->crtime ); // not memleak!
    }
}

void tox_c::del_message( u64 utag )
{
    for (message2send_s *x = message2send_s::first; x; x = x->next)
    {
        if (x->utag == utag)
        {
            delete x;
            hf->save();
            break;
        }
    }
}

void tox_c::call(contact_id_s id, const call_prm_s *ci)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (nullptr != cd->cip)
            return; // already call
        
        if (toxav_call(toxav, cd->get_fid().normal(), DEFAULT_AUDIO_BITRATE, ci->call_type == call_prm_s::VIDEO_CALL ? DEFAULT_VIDEO_BITRATE : 0, nullptr ))
            cd->prepare_call(ci->duration);
    }
}

void tox_c::stream_options(contact_id_s id, const stream_options_s * so)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (nullptr != cd->cip)
        {
            int oldso = cd->cip->local_so.options;
            int oldx = cd->cip->local_so.view_w;
            int oldy = cd->cip->local_so.view_h;
            cd->cip->local_so = *so;

            if ( cd->cip->started )
            {
                if (cd->get_fid().is_confa())
                {
                } else
                {
                    toxav_bit_rate_set(toxav, cd->get_fid().normal(),
                        0 != (cd->cip->local_so.options & SO_SENDING_AUDIO) ? DEFAULT_AUDIO_BITRATE : 0,
                        0 != (cd->cip->local_so.options & SO_SENDING_VIDEO) ? DEFAULT_VIDEO_BITRATE : 0, nullptr);

                    int changedso = (cd->cip->local_so.options ^ oldso) & (SO_RECEIVING_AUDIO|SO_RECEIVING_VIDEO);
                    if (changedso)
                    {
                        if (SO_RECEIVING_AUDIO & changedso)
                            toxav_call_control(toxav, cd->get_fid().normal(), 0 == (cd->cip->local_so.options & SO_RECEIVING_AUDIO) ? TOXAV_CALL_CONTROL_MUTE_AUDIO : TOXAV_CALL_CONTROL_UNMUTE_AUDIO, nullptr);

                        if (SO_RECEIVING_VIDEO & changedso)
                            toxav_call_control(toxav, cd->get_fid().normal(), 0 == (cd->cip->local_so.options & SO_RECEIVING_VIDEO) ? TOXAV_CALL_CONTROL_HIDE_VIDEO : TOXAV_CALL_CONTROL_SHOW_VIDEO, nullptr);
                    }

                    if (0 == (cd->cip->local_so.options & SO_RECEIVING_VIDEO) && cd->cip->video_ex)
                        cd->cip->current_recv_frame = -1;

                    if (cd->cip->local_so.view_w < 0 || cd->cip->local_so.view_h < 0)
                    {
                        cd->cip->local_so.view_w = 0;
                        cd->cip->local_so.view_h = 0;
                    }

                    if (oldx != cd->cip->local_so.view_w || oldy != cd->cip->local_so.view_h)
                        cd->send_viewsize( cd->cip->local_so.view_w, cd->cip->local_so.view_h );
                }
            }
        }
    }
}

int tox_c::send_av(contact_id_s id, const call_prm_s * ci)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return SEND_AV_OK;
        contact_descriptor_s *cd = it->second;

        if (nullptr != cd->cip)
        {
            auto w = callstate.lock_write();

            if (ci->audio_data)
            {
                stream_settings_s &ss = cd->cip->local_settings;
            
                ss.fifo.add_data( ci->audio_data, ci->audio_data_size );

                if ((int)ss.fifo.available() > ss.avgBytesPerSec())
                    ss.fifo.read_data(nullptr, ci->audio_data_size);
            }
            if (ci->video_data && ci->fmt == VFMT_I420 && 0 != (cd->cip->remote_so.options & SO_RECEIVING_VIDEO))
            {
                stream_settings_s &ss = cd->cip->local_settings;
                
                if (ss.video_data)
                    hf->free_video_data(ss.video_data);

                ss.video_w = ci->w;
                ss.video_h = ci->h;
                ss.video_data = ci->video_data;
                return SEND_AV_KEEP_VIDEO_DATA;
            }
        }
    }

    return SEND_AV_OK;
}

void tox_c::file_send(contact_id_s cid, const file_send_info_prm_s *finfo)
{
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->utag == finfo->utag)
        {
            // almost impossible
            // but we should check it anyway
            hf->file_control( finfo->utag, FIC_REJECT );
            return;
        }

    if (tox)
    {
        if (contact_descriptor_s *cd = find_descriptor(cid))
        {
            ASSERT(cd->get_fid().is_normal());

            std::string fnc;
            std::asptr fn(finfo->filename, finfo->filename_len);
            while(fn.l >= TOX_MAX_FILENAME_LENGTH)
            {
                std::wstr_c w = to_wstr(fn);
                fnc.super::set( to_utf8( w.as_sptr().skip(1) ) );
                fn = fnc.as_sptr();
            }

            static_assert( crypto_hash_sha256_BYTES == TOX_FILE_ID_LENGTH, "hash!" );

            byte fileid[ TOX_FILE_ID_LENGTH ];

            std::string temp(std::asptr(finfo->filename, finfo->filename_len) );
            temp.append_char('-');
            temp.append_as_num( finfo->utag );
            temp.append_char( '-' );
            temp.append_as_num( finfo->filesize );

            crypto_hash_sha256( fileid, (const unsigned char *)temp.cstr(), temp.get_length() );

            TOX_ERR_FILE_SEND er = TOX_ERR_FILE_SEND_NULL;
            uint32_t tr = tox_file_send(tox, cd->get_fid().normal(), FT_FILE, finfo->filesize, fileid, (const byte *)fn.s, fn.l, &er);
            if (er != TOX_ERR_FILE_SEND_OK)
            {
                hf->file_control( finfo->utag, FIC_DISCONNECT ); // put it to send queue: assume transfer broken
                return;
            }
            new transmitting_file_s(cd->get_fid(), tr, finfo->utag, fileid, finfo->filesize, std::string(fn)); // not memleak
        }
    }

}

void tox_c::file_accept(u64 utag, u64 offset)
{
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->utag == utag)
        {
            if (offset)
                tox_file_seek(tox, f->fid.normal(), f->fnn, offset, nullptr);
            tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
            break;
        }
}

void tox_c::file_control(u64 utag, file_control_e fctl)
{
    if (tox)
    {
        for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
            if (f->utag == utag)
            {
                switch (fctl)
                {
                case FIC_UNPAUSE:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
                    break;
                case FIC_PAUSE:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_PAUSE, nullptr);
                    break;
                case FIC_BREAK:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
                    delete f;
                    break;
                case FIC_CHECK:
                    if (f->check_count > 5)
                        hf->file_control(utag, FIC_STUCK);
                    break;
                }
                return;
            }

        for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
            if (f->utag == utag)
            {
                switch(fctl) //-V719
                {
                case FIC_ACCEPT:
                case FIC_UNPAUSE:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
                    break;
                case FIC_PAUSE:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_PAUSE, nullptr);
                    break;
                case FIC_REJECT:
                case FIC_BREAK:
                    tox_file_control(tox, f->fid.normal(), f->fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
                    delete f;
                    break;
                case FIC_DONE:
                    tox_file_send_chunk(tox, f->fid.normal(), f->fnn, 0, nullptr, 0, nullptr);
                    delete f;
                    break;
                case FIC_CHECK:
                    if (++f->check_count > 5)
                        hf->file_control(utag, FIC_STUCK);
                    break;
                }
                return;
            }

        if ( FIC_CHECK == fctl )
        {
            hf->file_control( utag, FIC_UNKNOWN );
        }
    }
}

bool tox_c::file_portion(u64 utag, const file_portion_prm_s *portion)
{
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->utag == utag)
            return f->portion( portion );

    return false;
}

void tox_c::folder_share_ctl(u64 utag, const folder_share_control_e ctl)
{
    if (fsh_s *fsh = find_fsh(utag))
    {
        if (contact_descriptor_s *c = find_descriptor(fsh->cid))
        {
            std::sstr_t<-128> ctlpacket("-fshctl/");
            ctlpacket.append_as_num(fsh->utag).append_char('/').append_as_int(ctl);
            *(byte *)ctlpacket.str() = PACKETID_EXTENSION;
            tox_friend_send_lossless_packet(cl.tox, c->get_fid().normal(), (const byte *)ctlpacket.cstr(), ctlpacket.get_length(), nullptr);
        }


        switch (ctl)
        {
        case FSC_ACCEPT:
            break;

        case FSC_REJECT:
            fsh->die();
            return;
        }
    }

}

void tox_c::folder_share_toc(contact_id_s cid, const folder_share_prm_s *sfd)
{
    auto it = id2desc.find(cid);
    if (it == id2desc.end()) return;
    contact_descriptor_s *desc = it->second;

    auto send_announce = [&]()
    {
        std::asptr n(sfd->name);
        sfs.emplace_back(sfd->utag, cid, n, sfd->ver, sfd->data, sfd->size);

        std::sstr_t<-(TOX_MAX_CUSTOM_PACKET_SIZE - 1)> annonce("-fsha/");
        annonce.append_as_num(sfd->utag).append_char('/').append(n);
        *(byte *)annonce.str() = PACKETID_EXTENSION;
        tox_friend_send_lossless_packet(cl.tox, desc->get_fid().normal(), (const byte *)annonce.cstr(), annonce.get_length(), nullptr);

    };

    if (desc->is_online())
    {
        for (fsh_ptr_s &sf : sfs)
        {
            if (sf.sf->utag == sfd->utag)
            {
                if (sf.sf->toc_size > 0)
                {
                    // not yet confirm
                    // just update toc
                    sf.update_toc(sfd->data, sfd->size);

                    // and send announce again
                    send_announce();
                    return;
                }

                if (sf.sf->toc_ver < sfd->ver)
                {
                    sf.sf->toc_ver = sfd->ver;

                    std::string fn(STD_ASTR("toc/"));
                    fn.append_as_num(sfd->utag);
                    transmitting_blob_s::start(fn, desc->get_fid(), FT_TOC, sfd->data, sfd->size);
                    return;
                }
            }
        }

        send_announce();
    }

}

void  tox_c::folder_share_query(u64 utag, const folder_share_query_prm_s *prm)
{
    if (fsh_s *fsh = find_fsh(utag))
    {
        auto it = id2desc.find(fsh->cid);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;

        std::sstr_t<-(TOX_MAX_CUSTOM_PACKET_SIZE - 1)> qpacket("-fshq/");
        qpacket.append_as_num(utag).append_char('/');
        int l = qpacket.get_length();

        auto doesc = [&]()
        {
            for (int i = l; i < qpacket.get_length(); ++i)
                if (qpacket.get_char(i) == '/')
                    qpacket.set_char(i, '\1');
        };

        qpacket.append(std::asptr(prm->tocname, prm->tocname_len));
        doesc();
        qpacket.append_char('/');
        l = qpacket.get_length();
        qpacket.append(std::asptr(prm->fakename, prm->fakename_len));
        doesc();

        *(byte *)qpacket.str() = PACKETID_EXTENSION;
        tox_friend_send_lossless_packet(cl.tox, desc->get_fid().normal(), (const byte *)qpacket.cstr(), qpacket.get_length(), nullptr);
    }

}


void tox_c::del_conference( const char *conference_id )
{
    conference_id_s id;
    std::pstr_c( std::asptr( conference_id ) ).hex2buf<TOX_CONFERENCE_UID_SIZE>( id.id );

    if (contact_descriptor_s *desc = contact_descriptor_s::find( id ))
    {
        tox_conference_delete( tox, desc->get_fid().confa(), nullptr );
        desc->die();
    } else
    {
        uint32_t cid = tox_conference_by_uid( tox, id.id, NULL );
        if (cid != UINT32_MAX)
            tox_conference_delete( tox, cid, nullptr );
    }

    hf->save();
}

void tox_c::enter_conference( const char *conference_id )
{
    conference_id_s id;
    std::pstr_c(std::asptr(conference_id) ).hex2buf<TOX_CONFERENCE_UID_SIZE>( id.id );

    uint32_t cid = tox_conference_by_uid( tox, id.id, NULL );
    if (cid != UINT32_MAX)
    {
        tox_conference_enter( tox, cid, nullptr );
        add_conference( tox_conference_get_type( tox, cid, nullptr ), id, nullptr );
        hf->save();
    } else
    {
        // just restore as text confa
        add_conference( TOX_CONFERENCE_TYPE_TEXT, id, nullptr );
    }
}

void tox_c::leave_conference(contact_id_s gid, int keep_leave )
{
    auto it = id2desc.find( gid );
    if ( it == id2desc.end() ) return;
    contact_descriptor_s *desc = it->second;
    if ( desc->is_conference() )
    {
        tox_conference_leave( tox, desc->get_fid().confa(), keep_leave & 1, nullptr );

        contact_data_s cd( desc->get_id(true), CDM_STATE | CDM_MEMBERS | CDM_PUBID );
        cd.state = CS_OFFLINE;
        cd.members_count = 0;

        //CONFAID
        std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
        cd.public_id = pubid.cstr();

        hf->update_contact( &cd );

        desc->die();
        hf->save();
    }
}

void tox_c::create_conference(const char *conferencename, const char *o)
{
    bool audio_conference = false;
    std::parse_values(std::asptr( o ), [&]( const std::pstr_c &field, const std::pstr_c &val ) {
        if ( field.equals( STD_ASTR( CONFA_OPT_TYPE ) ) )
            audio_conference = val.equals( STD_ASTR( "a" ) );
    } );

    if (tox)
    {
        int gnum = -1;
        if ( audio_conference)
            gnum = toxav_add_av_groupchat( tox, callback_av_conference_audio, nullptr );
        else
        {
            TOX_ERR_CONFERENCE_NEW err;
            uint32_t g = tox_conference_new( tox, &err );
            if ( err == TOX_ERR_CONFERENCE_NEW_OK )
                gnum = static_cast<int>( g );
        }

        if (gnum >= 0)
        {
            std::asptr gn = utf8clamp(conferencename, TOX_MAX_NAME_LENGTH);
            tox_conference_set_title(tox, gnum, (const uint8_t *)gn.s, gn.l, nullptr);

            contact_descriptor_s *desc = new contact_descriptor_s(ID_CONFERENCE, fid_s::make_confa(gnum));
            desc->state = CS_ONLINE; // conferences are always online

            conference_id_s gid;
            tox_conference_get_uid( tox, gnum, gid.id );

            desc->address = gid;

            if ( audio_conference )
                desc->prepare_call();

            contact_data_s cd( desc->get_id(true), CDM_PUBID | CDM_STATE | CDM_NAME | CDM_PERMISSIONS );
            cd.state = CS_ONLINE;
            cd.name = gn.s;
            cd.name_len = gn.l;
            //CONFAID
            std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
            cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
            cd.public_id = pubid.cstr();

            cd.conference_permissions = -1;

            std::string details_json_string;
            desc->prepare_details( details_json_string, cd );

            hf->update_contact(&cd);
        }
    }
}

void tox_c::ren_conference(contact_id_s gid, const char *conferencename)
{
    if (tox && gid.is_conference())
    {
        auto it = id2desc.find(gid);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;
        std::asptr gn = utf8clamp(conferencename, TOX_MAX_NAME_LENGTH);
        tox_conference_set_title(tox, desc->get_fid().confa(), (const uint8_t *)gn.s, gn.l, nullptr);

        contact_data_s cd(desc->get_id(true), CDM_NAME | CDM_PUBID);
        cd.name = gn.s;
        cd.name_len = gn.l;
        //CONFAID
        std::sstr_t<TOX_CONFERENCE_UID_SIZE * 2 + 16> pubid;
        cd.public_id_len = desc->address.as_str( pubid, tox_address_c::TAT_CONFERENCE_ID );
        cd.public_id = pubid.cstr();
        hf->update_contact(&cd);
    }
}

void tox_c::join_conference(contact_id_s gid, contact_id_s cid)
{
    if (tox && gid.is_conference() && cid.is_contact())
    {
        auto git = id2desc.find(gid);
        if (git == id2desc.end()) return;

        auto cit = id2desc.find(cid);
        if (cit == id2desc.end()) return;

        contact_descriptor_s *gcd = git->second;
        contact_descriptor_s *ccd = cit->second;

        if (!ccd->is_online()) return; // persistent conferences not yet supported

        tox_conference_invite(tox, ccd->get_fid().normal(), gcd->get_fid().confa(), nullptr);
    }
}

void tox_c::logging_flags(unsigned int f)
{
    g_logging_flags = f;
}

void tox_c::proto_file(i32 fid, const file_portion_prm_s *p)
{
    switch (fid)
    {
    case FILE_TAG_NODES:
        {
            std::vector<tox_node_s> nodesback = std::move(nodes);
            nodes.clear();
            for (std::token<char> lns(std::asptr((const char *)p->data, p->size), '\n'); lns; ++lns)
            {
                std::token<char> l(lns->get_trimmed(), '/');
                std::asptr ipv4;
                std::asptr ipv6;
                std::asptr pubid;
                uint16_t port = 0;
                if (l) ipv4 = l->as_sptr();
                ++l; if (l) ipv6 = l->as_sptr();
                ++l; if (l) port = l->as_num<uint16_t>();
                ++l; if (l) pubid = l->as_sptr();
                if (port != 0 && pubid.l && (ipv4.l || ipv6.l))
                {
                    nodes.emplace_back(ipv4, ipv6, port, pubid);
                    tox_node_s &n = nodes[nodes.size() - 1];

                    auto x = std::lower_bound(nodesback.begin(), nodesback.end(), n.pubid);
                    if (&*x && x->pubid == n.pubid)
                    {
                        n.push_count = x->push_count;
                        n.accept_count = x->accept_count;
                    }
                }
            }

            std::sort(nodes.begin(), nodes.end());
        }
        if (online_flag)
            connect();
        break;
    case FILE_TAG_PINNED:
        pinnedservs.clear();
        parse_values(std::asptr((const char *)p->data, p->size), [&](const std::pstr_c &field, const std::pstr_c &val)
        {
            pinnedservs.emplace_back(field, val);
        });

        break;
    }
}

#define FUNC1( rt, fn, p0 ) rt PROTOCALL static_##fn(p0 pp0) { return cl.fn(pp0); }
#define FUNC2( rt, fn, p0, p1 ) rt PROTOCALL static_##fn(p0 pp0, p1 pp1) { return cl.fn(pp0, pp1); }
PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1

proto_functions_s funcs =
{
#define FUNC1( rt, fn, p0 ) &static_##fn,
#define FUNC2( rt, fn, p0, p1 ) &static_##fn,
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
};

proto_functions_s* PROTOCALL api_handshake(host_functions_s *hf_)
{
    cl.on_handshake(hf_);
    return &funcs;
}

void PROTOCALL api_getinfo( proto_info_s *info )
{
    static std::sstr_t<1024> vers( "plugin: " SS( PLUGINVER ) ", toxcore: " );

    vers.append_as_uint( TOX_VERSION_MAJOR );
    vers.append_char( '.' );
    vers.append_as_uint( TOX_VERSION_MINOR );
    vers.append_char( '.' );
    vers.append_as_uint( TOX_VERSION_PATCH );
    vers.append_char( ')' );
#define NL "\n"
    static const char *strings[ _is_count_ ] =
    {
        "tox",
        "Tox protocol",
        "<b>Tox</b> protocol",
        vers.cstr(),
        HOME_SITE,
        "",
        "m 74.044499,38.828783 c 0,-4.166708 0.06581,-9.385543 -0.02935,-13.551362 -0.0418,-1.794753 -0.238352,-3.62508 -0.663471,-5.366471 C 70.309137,7.460631 58.257256,-0.28224198 45.602382,2.007891 34.399851,4.0347699 26.06021,14.030422 26.06021,25.430393 l 0,13.371709 c -3.678661,-3.2e-4 -7.473489,0 -11.204309,0 -2.466229,0 -4.467316,2.000197 -4.467316,4.467315 l 0,50.583035 c 0,2.466229 2.001087,4.467316 4.467316,4.467316 l 70.661498,0 c 2.468007,0 4.468205,-2.001087 4.468205,-4.467316 l 0,-50.583035 c 0,-2.467118 -2.000198,-4.467315 -4.468205,-4.467315 z m -17.313405,47.37418 -13.140472,0 c -2.065122,0 -3.738031,-1.702258 -3.738031,-3.801176 0.452829,-4.54018 0.52948,-5.74902 1.833885,-7.830921 1.45946,-2.150501 3.42853,-3.548594 5.564801,-4.17738 -2.815752,-1.148179 -4.799051,-3.911458 -4.799051,-7.137211 0,-4.259202 3.450763,-7.709077 7.708187,-7.709077 4.259203,0 7.708188,3.449875 7.708188,7.709077 0,3.307575 -2.084688,6.128663 -5.01339,7.221701 2.074015,0.669697 4.03508,2.057118 5.57903,4.186274 1.608048,2.307856 2.032215,4.874192 2.032215,7.571225 -0.0053,0.05514 0,0.111171 0,0.168091 8.9e-4,2.097139 -1.672019,3.799397 -3.735362,3.799397 M 65.3331,31.049446 c -1.389199,4.100894 -4.019961,7.386234 -6.768121,10.594199 -2.532759,2.638121 -5.259444,5.204412 -7.880726,6.825041 1.583972,-2.735708 2.86111,-5.334453 3.231089,-8.363654 -3.765601,0.891151 -7.316864,0.501606 -10.755176,-1.013884 -6.102871,-2.688572 -9.750186,-8.553981 -9.127625,-15.175356 0.50961,-5.449182 3.605395,-10.490611 8.391106,-13.0271 7.708187,-4.0848867 17.24706,-1.0668343 21.653898,6.200225 2.567617,4.231632 2.810178,9.372258 1.255555,13.960529",
        ADVSETSTRING,
        "sr=48000" NL "ch=1" NL "bps=16",
        "ToxID",
        "f=png",
        CONFA_OPT_TYPE "=t/a",
        "%APPDATA%\\tox", WINDOWS_ONLY // path to import
        "%APPDATA%\\tox\\isotoxin_tox_save.tox", WINDOWS_ONLY // file
        nullptr
    };

    //COPDEF( hole_punch, 1 ) \
        //COPDEF( local_discovery, 1 ) \


    info->strings = strings;

    info->priority = 500;
    info->indicator = 500;
    info->features = PF_AVATARS | PF_PURE_NEW | PF_IMPORT | PF_EXPORT | PF_AUDIO_CALLS | PF_VIDEO_CALLS | PF_SEND_FILE | PF_PAUSE_FILE | PF_CONFERENCE | PF_CONFERENCE_ENTER_LEAVE; //PF_AUTH_NICKNAME | PF_UNAUTHORIZED_CHAT;
    info->connection_features = CF_PROXY_SUPPORT_HTTP | CF_PROXY_SUPPORT_SOCKS5 | CF_ipv6_enable | CF_udp_enable;
}

extern "C"
{
    bool tox_options_get_ipv6_enabled(const struct Tox_Options *o)
    {
        return o->ipv6_enabled;
    }
    bool tox_options_get_udp_enabled(const struct Tox_Options *o)
    {
        return o->udp_enabled;
    }
    bool tox_options_get_local_discovery_enabled(const struct Tox_Options *o)
    {
        return o->local_discovery_enabled;
    }
    TOX_PROXY_TYPE tox_options_get_proxy_type(const struct Tox_Options *o)
    {
        return o->proxy_type;
    }
    const char *tox_options_get_proxy_host(const struct Tox_Options *o)
    {
        return o->proxy_host;
    }
    uint16_t tox_options_get_proxy_port(const struct Tox_Options *o)
    {
        return o->proxy_port;
    }
    uint16_t tox_options_get_start_port(const struct Tox_Options *o)
    {
        return o->start_port;
    }
    uint16_t tox_options_get_end_port(const struct Tox_Options *o)
    {
        return o->end_port;
    }
    uint16_t tox_options_get_tcp_port(const struct Tox_Options *o)
    {
        return o->tcp_port;
    }
    bool tox_options_get_hole_punching_enabled(const struct Tox_Options *o)
    {
        return o->hole_punching_enabled;
    }
    TOX_SAVEDATA_TYPE tox_options_get_savedata_type(const struct Tox_Options *o)
    {
        return o->savedata_type;
    }
    const uint8_t *tox_options_get_savedata_data(const struct Tox_Options *o)
    {
        return o->savedata_data;
    }
    size_t tox_options_get_savedata_length(const struct Tox_Options *o)
    {
        return o->savedata_length;
    }
    tox_log_cb *tox_options_get_log_callback(const struct Tox_Options *)
    {
        return nullptr;
    }
    void *tox_options_get_log_user_data(const struct Tox_Options *)
    {
        return nullptr;
    }
}
