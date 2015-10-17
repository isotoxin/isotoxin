#include "stdafx.h"
#pragma warning(push)
#pragma warning(disable : 4324) // 'crypto_generichash_blake2b_state' : structure was padded due to __declspec(align())
#include "sodium.h"
#pragma warning(pop)

// strange compile bug workaround: error C2365: 'TOX_GROUPCHAT_TYPE_TEXT' : redefinition; previous definition was 'enumerator'

#define TOX_GROUPCHAT_TYPE_TEXT ISOTOXIN_GROUPCHAT_TYPE_TEXT
#define TOX_GROUPCHAT_TYPE_AV ISOTOXIN_GROUPCHAT_TYPE_AV
#define TOX_CHAT_CHANGE_PEER_ADD ISOTOXIN_CHAT_CHANGE_PEER_ADD
#define TOX_CHAT_CHANGE_PEER_DEL ISOTOXIN_CHAT_CHANGE_PEER_DEL
#define TOX_CHAT_CHANGE_PEER_NAME ISOTOXIN_CHAT_CHANGE_PEER_NAME
#define TOX_CHAT_CHANGE ISOTOXIN_CHAT_CHANGE

#include "toxcore/toxcore/tox_old.h"

#define CHECKIT() __pragma(message(__LOC__ "check it: " __FUNCTION__))

#pragma USELIB("plgcommon")

#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "opus.lib")
#pragma comment(lib, "libvpx.lib")

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

#include "appver.inl"

#define IDENTITY_PACKETID 173
#define SAVE_VERSION 1

#define MAX_CALLS 16

#define GROUP_ID_OFFSET 10000000 // 10M contacts should be enough for everything

static void* iconptr = nullptr;
static int iconsize = 0;
extern HMODULE dll_module;

void __stdcall get_info( proto_info_s *info )
{
    if (info->protocol_name) strncpy_s(info->protocol_name, info->protocol_name_buflen, "tox", _TRUNCATE);
    if (info->description) strncpy_s(info->description, info->description_buflen, "Tox protocol", _TRUNCATE);
    if (info->description_with_tags) strncpy_s(info->description_with_tags, info->description_with_tags_buflen, "<b>Tox</b> protocol", _TRUNCATE);
    if (info->version)
    {
        sstr_t<1024> vers( "plugin: " SS(PLUGINVER) ", toxcore: " ); 

        vers.append_as_uint(TOX_VERSION_MAJOR);
        vers.append_char('.');
        vers.append_as_uint(TOX_VERSION_MINOR);
        vers.append_char('.');
        vers.append_as_uint(TOX_VERSION_PATCH);
        vers.append_char(')');

        strncpy_s(info->version, info->version_buflen, vers, _TRUNCATE);
    }

    info->priority = 500;
    info->features = PF_AVATARS | PF_OFFLINE_INDICATOR | PF_PURE_NEW | PF_IMPORT | PF_AUDIO_CALLS | PF_SEND_FILE | PF_GROUP_CHAT; //PF_INVITE_NAME | PF_UNAUTHORIZED_CHAT;
    info->connection_features = CF_PROXY_SUPPORT_HTTP | CF_PROXY_SUPPORT_SOCKS5 | CF_UDP_OPTION | CF_SERVER_OPTION;
    info->audio_fmt.sample_rate = av_DefaultSettings.audio_sample_rate;
    info->audio_fmt.channels = (short)av_DefaultSettings.audio_channels;
    info->audio_fmt.bits = 16;

    if (!iconptr)
    {
        HRSRC icon = FindResource(dll_module, MAKEINTRESOURCE(777), RT_RCDATA);
        iconsize = SizeofResource(dll_module, icon);
        HGLOBAL icondata = LoadResource(dll_module, icon);
        iconptr = LockResource(icondata);
    }
    info->icon = iconptr;
    info->icon_buflen = iconsize;
}

__forceinline int time_ms()
{
    return (int)timeGetTime();
}


static host_functions_s *hf;
static Tox *tox = nullptr;
static ToxAv *toxav = nullptr;
static Tox_Options options;
static sstr_t<256> tox_proxy_host;
static uint16_t tox_proxy_port;
static int tox_proxy_type = 0;
static byte avahash[TOX_HASH_LENGTH] = { 0 };
static std::vector<byte> gavatar;
static int gavatag = 0;
static bool reconnect = false;

int self_typing_contact = 0;
int self_typing_time = 0;

struct other_typing_s
{
    int fid = 0;
    int time = 0;
    other_typing_s(int fid, int time):fid(fid), time(time) {}
};
static std::vector<other_typing_s> other_typing;

static void set_proxy_addr(const asptr& addr)
{
    token<char> p(addr, ':');
    tox_proxy_host.clear();
    tox_proxy_port = 0;
    if (p) tox_proxy_host = *p;
    ++p; if (p) tox_proxy_port = (uint16_t)p->as_uint();
}

struct stream_settings_s : public ToxAvCSettings
{
    operator audio_format_s()
    {
        audio_format_s r;
        r.sample_rate = audio_sample_rate;
        r.channels = (short)audio_channels;
        r.bits = 16;
        return r;
    }
    fifo_stream_c fifo;
    int owner_cid = 0;
    int next_time_send = 0;
    int groupnum = -1;
    bool processing = false;

    stream_settings_s()
    {
        *(ToxAvCSettings *)this = av_DefaultSettings;
    }
    void setup()
    {
        fifo.clear();
        *(ToxAvCSettings *)this = av_DefaultSettings;
    }
};

struct audio_sender_state_s
{
    unsigned long active_calls = 0;
    bool audio_sender = false;
    bool audio_sender_need_stop = false;
    stream_settings_s local_peer_settings[MAX_CALLS * 2]; // MAX_CALLS for audio calls, another MAX_CALLS for groups
    static_assert(sizeof(unsigned long /*active_calls*/)*8 >= MAX_CALLS, "!");

    bool allow_run_audio_sender() const
    {
        return !audio_sender && !audio_sender_need_stop;
    }
};

static spinlock::syncvar<audio_sender_state_s> state;
static stream_settings_s remote_audio_fmt[MAX_CALLS * 2];


enum chunks_e // HARD ORDER!!! DO NOT MODIFY EXIST VALUES!!!
{
    chunk_tox_data,
    chunk_descriptors,
    
    chunk_descriptor,
    chunk_descriptor_id,
    chunk_magic,
    chunk_descriptor_pubid,
    chunk_descriptor_state,
    chunk_descriptor_avatartag,
    chunk_descriptor_avatarhash,

    /*

        no more need to save undelivered messages by protocol, due gui saves them itself


    chunk_msgs_sending = 10,
    chunk_msg_sending,
    chunk_msg_sending_fid,
    chunk_msg_sending_type,
    chunk_msg_sending_utag,
    chunk_msg_sending_body,
    chunk_msg_sending_mid,
    chunk_msg_sending_createtime,
    */

    chunk_msgs_receiving = 20,
    chunk_msg_receiving,
    chunk_msg_receiving_cid,
    chunk_msg_receiving_type,
    chunk_msg_receiving_body,
    chunk_msg_receiving_createtime,

    chunk_other = 30,
    chunk_proxy_type,
    chunk_proxy_address,
    chunk_server_port,
    chunk_use_udp,
    chunk_toxid,
};

struct dht_node_s
{
    str_c addr;
    byte pubid[32];
    int port;
    int used = 0;
    int random = 0;

    dht_node_s( const asptr& addr, int port, const asptr& pubid_ ):addr(addr), port(port)
    {
        pstr_c(pubid_).hex2buf<32>(pubid);
    }
};

static std::vector<dht_node_s> nodes;

static str_c dnsquery( const str_c & query, const asptr& ver )
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
                str_c retid;

                const char *r = (const char *)txt->pStringArray[0];
                asptr rr(r);
                if (ver.l == 0 && rr.l == 64)
                    return str_c(rr);
                token<char> t(rr, ';');
                for (; t; ++t)
                {
                    token<char> tt(*t, '=');
                    if (tt)
                    {
                        if (tt->equals(CONSTASTR("v")))
                        {
                            ++tt;
                            if (!tt || !tt->equals(ver))
                                return str_c();
                            ok_version = true;
                            continue;
                        }
                        if (tt->equals(CONSTASTR("id")))
                        {
                            ++tt;
                            if (!tt)
                                return str_c();

                            retid = *tt;
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

    return str_c();
}

struct tox3dns_s
{
    str_c addr;
    byte key[32];
    void *dns3 = nullptr;
    bool key_ok = false;

    void operator=(tox3dns_s && oth)
    {
        addr = oth.addr;
        memcpy( key, oth.key, 32 );
        key_ok = oth.key_ok;
        std::swap(dns3, oth.dns3);
    }
    tox3dns_s(tox3dns_s && oth):addr(oth.addr)
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

    tox3dns_s( const asptr &sn, const asptr &k = asptr()):addr(sn)
    {
        if (k.l)
        {
            pstr_c(k).hex2buf<32>(key);
            key_ok = true;

        } else
        {
            // we have to query
            str_c key_string = dnsquery(CONSTASTR("_tox.") + addr, asptr());
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

    str_c query1( const str_c &pubid )
    {
        if (dns3)
        {
            tox_dns3_kill(dns3);
            dns3 = nullptr;
        }
        str_c request(pubid);
        request.replace_all( CONSTASTR("@"), CONSTASTR("._tox.") );
        return dnsquery(request, CONSTASTR("tox1"));
    }

    str_c query3( const str_c &pubid )
    {
        if (!key_ok)
            return query1(pubid);

        if (dns3 == nullptr)
            dns3 = tox_dns3_new(key);

        uint32_t request_id;
        sstr_t<128> dns_string;
        int dns_string_len = tox_generate_dns3_string(dns3, (byte *)dns_string.str(), (uint16_t)dns_string.get_capacity(), &request_id, (byte*)pubid.cstr(), (byte)pubid.find_pos('@'));

        if (dns_string_len < 0)
            return query1(pubid);

        dns_string.set_length(dns_string_len);

        str_c rec( 256, true );
        rec.set_as_char('_').append(dns_string).append(CONSTASTR("._tox.")).append(addr);
        rec = dnsquery(rec,CONSTASTR("tox3"));
        if (!rec.is_empty())
        {
            byte tox_id[TOX_ADDRESS_SIZE];
            if (tox_decrypt_dns3_TXT(dns3, tox_id, (/*const*/ byte *)rec.cstr(), rec.get_length(), request_id) < 0)
                return query1(pubid);
        
            rec.clear();
            rec.append_as_hex(tox_id, sizeof(tox_id));
            return rec;
        }

        return query1(pubid);
    }
};

static std::vector<tox3dns_s> pinnedservs;

struct message2send_s
{
    static message2send_s *first;
    static message2send_s *last;
    message2send_s *prev;
    message2send_s *next;

    u64 utag;
    message_type_e mt;
    int fid;
    int mid;
    int next_try_time;
    int send_timeout = 0;
    str_c msg;
    time_t create_time;
    message2send_s( u64 utag, message_type_e mt, int fid, const asptr &utf8, int imid = -10000, time_t create_time = now() ):utag(utag), mt(mt), fid(fid), mid(imid), next_try_time(time_ms()), create_time(create_time)
    {
        LIST_ADD( this, first, last, prev, next );

        if (-10000 == imid)
        {
            msg.set(CONSTASTR("\1")).append_as_num<u64>(create_time).append(CONSTASTR("\2"));
            mid = 0;
        }

        msg.append( utf8 );

        int e = TOX_MAX_MESSAGE_LENGTH - 3;
        if (utf8.l >= e)
        {
            for(;e>=0;--e)
                if (msg.get_char(e) == ' ')
                    break;
            if (e < TOX_MAX_MESSAGE_LENGTH/2)
                msg.set_length(TOX_MAX_MESSAGE_LENGTH-1); // just clamp - no-spaces crap must die
            else
            {
                if ((utf8.l - e - 1) > 0) new message2send_s(utag, mt, fid, utf8.skip(e + 1), -1, create_time); // not memory leak
                msg.set_length(e).append(CONSTASTR("\t\t")); // double tabs indicates that it is multi-part message
            }
        }
    }
    ~message2send_s()
    {
        LIST_DEL( this, first, last, prev, next );
    }

    void try_send(int time);

    static void read(int fid, int receipt)
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
        {
            hf->delivered(dtag);
            hf->save();
        }
    }

    static void tick(int ct)
    {
        std::vector<int> disabled;

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
                    for (message2send_s *x = first; x; x = x->next)
                    {
                        if (x->utag == utag_multipart)
                        {
                            // next message part...
                            x->send_timeout = ct;
                        }
                    }
                }

                break;
            }
        }

        
        // toxcore not yet support groupchat delivery notification
        // simulate it

        for (message2send_s *x = first; x; x = x->next)
        {
            if (x->fid >= GROUP_ID_OFFSET && x->mid == 777)
            {
                message2send_s::read( x->fid, 777 );
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
    message_type_e mt;
    int cid;
    str_c msgb;
    int send2host_time = 0;

    message_part_s(message_type_e mt, int cid, u64 create_time_, const asptr &msgbody, bool asap):mt(mt), cid(cid), create_time(create_time_)
    {
        LIST_ADD(this, first, last, prev, next);
        send2host_time = time_ms() + (asap ? 0 : 5000);
        msgb = extract_create_time(create_time, msgbody, cid);
    }
    ~message_part_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    static asptr extract_create_time( u64 &t, const asptr &msgbody, int cid );

    static void msg(message_type_e mt, int cid, u64 create_time, const char *msgbody, int len)
    {
        if ( len > TOX_MAX_MESSAGE_LENGTH/2 && msgbody[len-1] == '\t' && msgbody[len-2] == '\t' )
        {
            // multi-part message
            // wait other parts 5 seconds
            for (message_part_s *x = first; x; x = x->next)
            {
                if (x->cid == cid)
                {
                    x->msgb.append_char(' ').append(asptr(msgbody, len-2));
                    x->send2host_time = time_ms() + 5000; // wait next part 5 second
                    hf->save();
                    return;
                }
            }
            new message_part_s(mt,cid, create_time, asptr(msgbody, len-2), false); // not memory leak
            hf->save();
        
        } else
        {
            for (message_part_s *x = first; x; x = x->next)
            {
                if (x->cid == cid)
                {
                    // looks like last part of multi-part message
                    // concatenate and send it to host
                    x->msgb.append_char(' ').append(asptr(msgbody, len));
                    hf->message(x->mt, 0, x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
                    delete x;
                    hf->save();
                    return;
                }
            }
            asptr m = extract_create_time(create_time, asptr(msgbody, len), cid);
            hf->message(mt, 0, cid, create_time, m.s, m.l);
        }
    }

    static void tick(int ct)
    {
        for (message_part_s *x = first; x; x = x->next)
        {
            if ( (ct - x->send2host_time) > 0 )
            {
                hf->message(x->mt, 0, x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
                delete x;
                hf->save();
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

    u32 fid;
    byte fileid[TOX_FILE_ID_LENGTH];

    u64 fsz;
    u64 utag;
    u32 fnn;

};

static void make_uniq_utag( u64 &utag );
struct incoming_file_s : public file_transfer_s
{
    static incoming_file_s *first;
    static incoming_file_s *last;
    incoming_file_s *prev;
    incoming_file_s *next;
    
    str_c fn;
    TOX_FILE_KIND kind;

    incoming_file_s( u32 fid_, u32 fnn_, TOX_FILE_KIND kind, u64 fsz_, const asptr &fn ):fn(fn), kind(kind)
    {
        fsz = fsz_;
        fid = fid_;
        fnn = fnn_;

        if (TOX_FILE_KIND_AVATAR != kind && tox_file_get_file_id(tox,fid,fnn,fileid,nullptr))
        {
            utag = *(const u64 *)fileid; // only 8 bytes of fileid used. isotoxin plugins api requires only 8 bytes unique
        } else
            randombytes_buf(&utag, sizeof(u64));
        
        make_uniq_utag(utag); // requirement to unique is only utag unique across current file transfers

        LIST_ADD(this, first, last, prev, next);
    }

    ~incoming_file_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    virtual void recv_data( u64 position, const byte *data, size_t datasz )
    {
        hf->file_portion(utag, position, data, datasz);
    }

    /*virtual*/ void accepted() override
    {
        if (TOX_FILE_KIND_AVATAR != kind)
            hf->file_control(utag,FIC_UNPAUSE);
    }
    /*virtual*/ void kill() override
    {
        if (TOX_FILE_KIND_AVATAR != kind)
            hf->file_control(utag,FIC_BREAK);
        delete this;
    }
    /*virtual*/ void finished() override
    {
        hf->file_control(utag,FIC_DONE);
        //delete this; do not delete now due FIC_DONE from app
    }
    /*virtual*/ void pause() override
    {
        if (TOX_FILE_KIND_AVATAR != kind)
            hf->file_control(utag,FIC_PAUSE);
    }

    static void tick(int ct);

};

struct incoming_avatar_s : public incoming_file_s
{
    std::vector<byte> avatar;
    int droptime;
    incoming_avatar_s(u32 fid_, u32 fnn_, u64 fsz_, const asptr &fn) : incoming_file_s( fid_, fnn_, TOX_FILE_KIND_AVATAR, fsz_, fn )
    {
        droptime = time_ms() + 30000;
        avatar.resize((size_t)fsz_);
    }
    
    void check_avatar(int ct);

    /*virtual*/ void recv_data(u64 position, const byte *data, size_t datasz) override
    {
        droptime = time_ms() + 30000;

        u64 newsize = position + datasz;
        if (newsize <= avatar.size())
            memcpy(avatar.data() + position, data, datasz);
    }
    /*virtual*/ void finished() override;
};

void incoming_file_s::tick(int ct)
{
    for (incoming_file_s *f = incoming_file_s::first; f;)
    {
        incoming_file_s *ff = f->next;

        if (TOX_CONNECTION_NONE == tox_friend_get_connection_status(tox,f->fid,nullptr))
        {
            // peer disconnected - transfer break
            if (TOX_FILE_KIND_AVATAR != f->kind) hf->file_control(f->utag, FIC_DISCONNECT);
            delete f;
        } else if (TOX_FILE_KIND_AVATAR == f->kind)
            ((incoming_avatar_s *)f)->check_avatar(ct);

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
    str_c fn;

    struct req
    {
        req() {};
        req(u64 offset, u32 size):offset(offset), size(size) {}
        u64 offset;
        u32 size = 0;
    };

    fifo_stream_c requests;

    bool is_accepted = false;
    bool is_paused = false;
    bool is_finished = false;

    transmitting_data_s(u32 fid_, u32 fnn_, u64 utag_, const byte * fileid_, u64 fsz_, const asptr &fn) :fn(fn)
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
    virtual void portion( u64 /*offset*/, const void * /*data*/, int /*size*/ ) {}
    virtual void try_send_requested_chunk() = 0;

    req last_req;

    void transmit(const req &r)
    {
        if (r.size == 0)
        {
            finished();
            return;
        }

        ASSERT(last_req.size == 0 || (last_req.offset + last_req.size) == r.offset);

        requests.add_data( &r, sizeof(req) );
        try_send_requested_chunk();
    }

    /*virtual*/ void accepted() override
    {
        is_accepted = true;
        is_paused = false;
        hf->file_control(utag, FIC_ACCEPT);
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
    static const int MAX_CHUNKS_REQUESTED = 16;

    fifo_stream_c fifo;
    u64 fifooffset = 0;
    u64 end_offset() const { return fifooffset + fifo.available(); }

    struct buf_s
    {
        std::vector<byte> buf;
        u64 offset = 0;
        buf_s(u64 offset, const void *data, int size):offset(offset), buf(size)
        {
            memcpy(buf.data(),data,size);
        }
    };

    std::vector<buf_s> shuffled;
    u64 requested_offset_end = 0;
    int requested_chunks = 0;


    transmitting_file_s(u32 fid_, u32 fnn_, u64 utag_, const byte * id_, u64 fsz_, const asptr &fn) : transmitting_data_s(fid_, fnn_, utag_, id_, fsz_, fn)
    {
    }

    /*virtual*/ void finished() override
    {
        hf->file_control(utag, FIC_DONE);
        __super::finished();
    }
    /*virtual*/ void kill() override
    {
        hf->file_control(utag, FIC_BREAK);
        __super::kill();
    }

    /*virtual*/ void pause() override
    {
        __super::pause();
        hf->file_control(utag, FIC_PAUSE);
    }

    /*virtual*/ void portion(u64 offset, const void *data, int size) override
    {
        ASSERT(requested_chunks > 0);
        --requested_chunks;

        ASSERT((offset+size) <= requested_offset_end);

        if (end_offset() == offset)
        {
            fifo.add_data(data, size);
        } else
        {
            bool added = false;
            for (buf_s &b : shuffled)
            {
                if (offset == b.offset + b.buf.size())
                {
                    added = true;
                    size_t oo = b.buf.size();
                    b.buf.resize( b.buf.size() + size );
                    memcpy( b.buf.data() + oo, data, size );
                }
            }
            if (!added)
            {
                shuffled.emplace_back(offset, data, size);
            }
        }

        try_send_requested_chunk();
    }

    void request_portion()
    {
        if (requested_chunks >= MAX_CHUNKS_REQUESTED) return;
        
        size_t avaialble = fifo.available();
        for (const buf_s &b : shuffled)
            avaialble += b.buf.size();
        if (avaialble >= 65536)
            return;

        // always request until end of file 
        if ( requested_offset_end < fsz )
        {
            if (int rqsz = (int)min(65536, fsz - requested_offset_end))
            {
                hf->file_portion(utag, requested_offset_end, nullptr, rqsz);
                ++requested_chunks;
                requested_offset_end += rqsz;
            }
        }
    }

    void try_send_requested_chunk() override
    {
        u64 curend = end_offset();
        for (buf_s &b : shuffled)
        {
            if (b.offset == curend)
            {
                fifo.add_data( b.buf.data(), b.buf.size() );
                shuffled.erase( shuffled.begin() + ( &b - shuffled.data() ) );
                break;
            }
        }

        if (requests.available())
        {
            req r;
            requests.get_data(0, (byte *)&r, sizeof(req));

            if (r.offset == fifooffset)
            {
                if (fifo.available() >= r.size)
                {
                    byte *d = (byte *)_alloca(r.size); //-V505
                    fifo.get_data(0, d, r.size);

                    TOX_ERR_FILE_SEND_CHUNK er;
                    tox_file_send_chunk(tox, fid, fnn, r.offset, d, r.size, &er);
                    if (TOX_ERR_FILE_SEND_CHUNK_OK == er)
                    {
                        fifo.read_data(nullptr, r.size);
                        fifooffset += r.size;
                        requests.read_data(nullptr, sizeof(req));
                    }
                }
            }
        }
        request_portion();
    }

    /*virtual*/ void ontick() override
    {
        if (tox_friend_get_connection_status(tox,fid,nullptr) == TOX_CONNECTION_NONE)
        {
            // peer disconnect - transfer broken
            hf->file_control(utag, FIC_DISCONNECT);
            delete this;
            return;
        }

        if (requests.available())
            try_send_requested_chunk();
    }


};

struct transmitting_avatar_s : public transmitting_data_s
{
    int avatag = -1;

    transmitting_avatar_s(u32 fid_, u32 fnn_, u64 utag_, const asptr &fn) : transmitting_data_s(fid_, fnn_, utag_, avahash, gavatar.size(), fn)
    {
        avatag = gavatag;
    }
    virtual void ontick() override
    {
        if (avatag != gavatag)
        {
            // avatar changed while it sending
            tox_file_control(tox, fid, fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
            delete this;
        } else
        {
            byte id[TOX_FILE_ID_LENGTH];
            if (!tox_file_get_file_id(tox, fid, fnn, id, nullptr))
            {
                // transfer broken
                delete this;
                return;
            }

            if (requests.available())
                try_send_requested_chunk();
        }
    }
    /*virtual*/ void kill() override;
    /*virtual*/ void finished() override;

    /*virtual*/ void try_send_requested_chunk() override
    {
        if (avatag != gavatag) return;

        if (requests.available())
        {
            req r;
            requests.get_data(0, (byte *)&r, sizeof(req));

            if (ASSERT((r.offset + r.size) <= gavatar.size()))
            {
                TOX_ERR_FILE_SEND_CHUNK er;
                tox_file_send_chunk(tox, fid, fnn, r.offset, gavatar.data() + r.offset, r.size, &er);
                if (TOX_ERR_FILE_SEND_CHUNK_OK == er)
                    requests.read_data(nullptr, sizeof(req));
            }
        }
    }
};

transmitting_data_s *transmitting_data_s::first = nullptr;
transmitting_data_s *transmitting_data_s::last = nullptr;

static int send_request(const char*public_id, const char* invite_message_utf8, bool resend);

struct discoverer_s
{
    static discoverer_s *first;
    static discoverer_s *last;
    discoverer_s *prev;
    discoverer_s *next;
    
    int timeout_discover = 0;
    str_c invmsg;

    struct syncdata_s
    {
        str_c ids;
        str_c pubid;

        bool waiting_thread_start = true;
        bool thread_in_progress = false;
        bool shutdown_discover = false;
    };

    spinlock::syncvar< syncdata_s > sync;

    discoverer_s(const asptr &ids, const asptr &invmsg) :invmsg(invmsg)
    {
        sync.lock_write()().ids = ids;
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
            hf->operation_result(LOP_ADDCONTACT, CR_TIMEOUT);
            delete this;
            return true;
        }

        if (r().thread_in_progress || r().waiting_thread_start)
            return false;

        if (r().pubid.is_empty())
        {
            hf->operation_result(LOP_ADDCONTACT, CR_INVALID_PUB_ID);
        } else
        {
            int rslt = send_request(r().pubid,invmsg,false);
            hf->operation_result(LOP_ADDCONTACT, rslt);
        }
        r.unlock(); // unlock now to avoid deadlock
        delete this;
        return true;
    }

    void discover_thread()
    {
        str_c ids;
        auto w = sync.lock_write();
        if (w().shutdown_discover) return;
        w().waiting_thread_start = false;
        w().thread_in_progress = true;

        ids.setcopy( w().ids );
        w.unlock();

        str_c servname = ids.substr(ids.find_pos('@') + 1);
        
        bool pinfound = false;
        for (tox3dns_s& pin : pinnedservs)
        {
            if (sync.lock_read()().shutdown_discover)
                break;

            if (servname.equals(pin.addr))
            {
                str_c s = pin.query3(ids);
                sync.lock_write()().pubid.setcopy(s);
                pinfound = true;
                break;
            }
        }

        if (!pinfound)
        {
            pinnedservs.emplace_back( servname );
            str_c s = pinnedservs.back().query3(ids);
            if ( !pinnedservs.back().key_ok )
                pinnedservs.erase( --pinnedservs.end() ); // kick non tox3 servers from list
            
            sync.lock_write()().pubid.setcopy(s);
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

static void make_uniq_utag( u64 &utag )
{
    again_check:
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->utag == utag)
        {
            // almost impossible
            // probability of this code execution less then probability of finding a billion dollars on the street
            ++utag;
            goto again_check;
        }
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->utag == utag)
        {
            ++utag;
            goto again_check;
        }
}

enum idgen_e
{
    SKIP_ID_GEN,
    ID_CONTACT,
    ID_GROUP,
    ID_UNKNOWN,
};

struct contact_descriptor_s
{
    static contact_descriptor_s *first_desc;
    static contact_descriptor_s *last_desc;

    contact_descriptor_s *next;
    contact_descriptor_s *prev;

private:
    int id = 0;
    int fid; 
public:
    int audiostream = -1;

    int avatar_tag = 0; // tag of contact's avatar
    int avatar_recv_fnn = -1; // receiving file number
    int avatag_self = -1; // tag of self avatar, if not equal to gavatag, then should be transfered (gavatag increased every time self avatar changed)
    std::vector<byte> avatar; // contact's avatar itself
    byte avatar_hash[TOX_HASH_LENGTH];

    contact_state_e state = CS_ROTTEN;
    str_c pubid;

    int correct_create_time = 0; // delta
    int wait_client_id = 0;
    int next_sync = 0;

    static const int F_IS_ONLINE = 1;
    static const int F_NOTIFY_APP_ON_START = 2;
    static const int F_ISOTOXIN = 4;
    static const int F_NEED_RESYNC = 8;
    static const int F_AVASEND = 16;
    static const int F_AVARECIVED = 32;
    static const int F_FIDVALID = 64;

    int flags = 0;

    contact_descriptor_s(idgen_e init_new_id, int fid = -1);
    ~contact_descriptor_s()
    {
        LIST_DEL(this, first_desc, last_desc, prev, next);
    }
    static int find_free_id(bool group);
    static contact_descriptor_s *find( const asptr &pubid )
    {
        pstr_c fff; fff.set(pubid.part(TOX_PUBLIC_KEY_SIZE * 2));
        for(contact_descriptor_s *f = first_desc; f; f = f->next)
            if (f->pubid.substr(0, TOX_PUBLIC_KEY_SIZE * 2) == fff)
                return f;

        return nullptr;
    }

    void die();

    bool is_online() const { return ISFLAG(flags, contact_descriptor_s::F_IS_ONLINE); }
    void on_offline()
    {
        UNSETFLAG(flags, F_IS_ONLINE);
        UNSETFLAG(flags, F_NEED_RESYNC);
        UNSETFLAG(flags, F_ISOTOXIN);
        UNSETFLAG(flags, F_AVASEND);
        avatag_self = gavatag - 1;
        correct_create_time = 0;
        avatar_recv_fnn = -1;

        if (self_typing_contact && self_typing_contact == get_id())
            self_typing_contact = 0;

        for (auto it = other_typing.begin(); it != other_typing.end(); ++it)
        {
            if (it->fid == get_fid())
            {
                other_typing.erase(it);
                break;
            }
        }
    }

    int get_fidgnum() const {return fid;}
    int get_fid() const {return fid < GROUP_ID_OFFSET ? fid : -1;};
    int get_gnum() const {return fid >= GROUP_ID_OFFSET ? (fid - GROUP_ID_OFFSET) : -1;};
    int get_id() const {return id;};
    void set_fid(int fid_, bool fid_valid);
    void set_id(int id_);

    void send_identity(bool resync)
    {
        sstr_t<-128> isotoxin_id("-isotoxin/" SS(PLUGINVER) "/"); isotoxin_id.append_as_num<u64>(now());
        if (resync) isotoxin_id.append(CONSTASTR("/resync"));
        *(byte *)isotoxin_id.str() = IDENTITY_PACKETID;
        TOX_ERR_FRIEND_CUSTOM_PACKET err = TOX_ERR_FRIEND_CUSTOM_PACKET_OK;
        tox_friend_send_lossless_packet(tox, fid, (const byte *)isotoxin_id.cstr(), isotoxin_id.get_length(), &err);
    }

    void send_avatar()
    {
        if (!is_online()) return;
        if (ISFLAG(flags, F_AVASEND)) return;
        if (avatag_self == gavatag) return;

        str_c fnc; fnc.append_as_hex( avahash, TOX_HASH_LENGTH ).append(CONSTASTR(".png"));

        u64 utag;
        randombytes_buf(&utag, sizeof(u64));
        make_uniq_utag( utag );

        TOX_ERR_FILE_SEND er = TOX_ERR_FILE_SEND_NULL;
        uint32_t tr = tox_file_send(tox, get_fid(), TOX_FILE_KIND_AVATAR, gavatar.size(), avahash, (const byte *)fnc.cstr(), (uint16_t)fnc.get_length(), &er);
        if (er != TOX_ERR_FILE_SEND_OK)
            return;

        SETFLAG(flags, F_AVASEND);

        new transmitting_avatar_s(get_fid(), tr, utag, fnc); // not memleak

    }

    void recv_avatar()
    {
        if (avatar_recv_fnn < 0)
        {
            if (ISFLAG(flags, F_AVARECIVED))
                hf->avatar_data(get_id(),avatar_tag,avatar.data(),avatar.size());
            return;
        }
        tox_file_control(tox,get_fid(),avatar_recv_fnn, TOX_FILE_CONTROL_RESUME, nullptr);
    }

};

contact_descriptor_s *contact_descriptor_s::first_desc = nullptr;
contact_descriptor_s *contact_descriptor_s::last_desc = nullptr;

static std::unordered_map<int, contact_descriptor_s*> id2desc;
static std::unordered_map<int, contact_descriptor_s*> fid2desc;

static contact_descriptor_s * find_descriptor(int cid);
static contact_descriptor_s * find_restore_descriptor(int fid);

void incoming_avatar_s::check_avatar(int ct)
{
    bool die = true;
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
        die = desc->avatar_recv_fnn != (int)fnn;
    if (!die && (ct - droptime) > 0) die = true;
    if (die)
    {
        tox_file_control(tox,fid,fnn,TOX_FILE_CONTROL_CANCEL, nullptr);
        delete this;
    }
}

/*virtual*/ void incoming_avatar_s::finished()
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        byte hash[TOX_HASH_LENGTH];
        tox_hash(hash,avatar.data(),avatar.size());

        if (0 != memcmp(hash, desc->avatar_hash, TOX_HASH_LENGTH))
        {
            memcpy(desc->avatar_hash, hash, TOX_HASH_LENGTH);
            ++desc->avatar_tag; // new avatar version
        }

        desc->avatar = std::move(avatar);
        SETFLAG(desc->flags, contact_descriptor_s::F_AVARECIVED);

        hf->avatar_data(desc->get_id(), desc->avatar_tag, desc->avatar.data(), desc->avatar.size());
    }

    delete this;
}



/*virtual*/ void transmitting_avatar_s::kill()
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        UNSETFLAG(desc->flags, contact_descriptor_s::F_AVASEND);
    }
    __super::kill();
}

/*virtual*/ void transmitting_avatar_s::finished()
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        UNSETFLAG(desc->flags, contact_descriptor_s::F_AVASEND);
        desc->avatag_self = avatag;
    }
    __super::finished();
}


/*
static bool is_isotoxin(int fid)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
        return ISFLAG(desc->flags, contact_descriptor_s::F_ISOTOXIN);
    return false;
}
*/

void message2send_s::try_send(int time)
{
    if ((time - next_try_time) > 0)
    {
        bool send_create_time = false;
        if (fid < GROUP_ID_OFFSET)
            if (contact_descriptor_s *desc = find_restore_descriptor(fid))
            {
                if (!desc->is_online())
                {
                    mid = 0;
                    return; // not yet
                }
                if (desc->wait_client_id)
                {
                    if ((time - desc->wait_client_id) < 0)
                    {
                        mid = 0;
                        return; // not yet
                    }
                    desc->wait_client_id = 0;
                }
                send_create_time = ISFLAG(desc->flags, contact_descriptor_s::F_ISOTOXIN);
            }

        asptr m = msg.as_sptr();
        if (!send_create_time)
        {
            int skipchars = pstr_c(m).find_pos('\2');
            if (skipchars >= 0)
                m = m.skip(skipchars + 1);
        }

        mid = 0;
        if (fid >= GROUP_ID_OFFSET)
        {
            // to group
            if (tox_group_message_send(tox, fid - GROUP_ID_OFFSET, (const byte *)m.s, (uint16_t)m.l) == 0)
                mid = 777;

        } else
        {
            if (mt == MT_MESSAGE)
                mid = tox_friend_send_message(tox, fid, TOX_MESSAGE_TYPE_NORMAL, (const byte *)m.s, m.l, nullptr);
            if (mt == MT_ACTION)
                mid = tox_friend_send_message(tox, fid, TOX_MESSAGE_TYPE_ACTION, (const byte *)m.s, m.l, nullptr);
        }
        if (mid) send_timeout = time + 60000;
        next_try_time = time + 1000;
    }
}

contact_descriptor_s::contact_descriptor_s(idgen_e init_new_id, int fid) :pubid(TOX_ADDRESS_SIZE * 2, true), fid(fid)
{
    memset(avatar_hash, 0, sizeof(avatar_hash));
    LIST_ADD(this, first_desc, last_desc, prev, next);

    if (init_new_id)
    {
        id = contact_descriptor_s::find_free_id(init_new_id == ID_GROUP);
        id2desc[id] = this;
        if (ID_UNKNOWN == init_new_id)
            fid = -id; // just make fid negative and unique


        SETUPFLAG( flags, F_FIDVALID, (init_new_id == ID_CONTACT && fid >= 0) || (init_new_id == ID_GROUP && fid >= GROUP_ID_OFFSET) || (init_new_id == ID_UNKNOWN && fid < 0) );
        if (ISFLAG( flags, F_FIDVALID))
            fid2desc[fid] = this;
    }
}

void contact_descriptor_s::die()
{
    if (id != 0)
        id2desc.erase(id);
    if (ISFLAG(flags, F_FIDVALID))
        fid2desc.erase(fid);
    delete this;
}

int contact_descriptor_s::find_free_id( bool group )
{
    if (group)
    {
        int id = -1;
        for (; id2desc.find(id) != id2desc.end(); --id);
        return id;
    } else
    {
        int id = 1;
        for (; id2desc.find(id) != id2desc.end(); ++id);
        return id;
    }
}

void contact_descriptor_s::set_fid(int fid_, bool fid_valid)
{
    if (ISFLAG(flags, F_FIDVALID) != fid_valid || (fid != fid_ && fid_valid))
    {
        if (ISFLAG(flags, F_FIDVALID))
            fid2desc.erase(fid);

        if (fid_valid)
        {
            fid = fid_;
            fid2desc[fid] = this;
            SETFLAG(flags, F_FIDVALID);
        } else
        {
            UNSETFLAG(flags, F_FIDVALID);
        }
    }
}
void contact_descriptor_s::set_id(int id_)
{
    if (id != id_)
    {
        id2desc.erase(id);
        id = id_;
        if (id != 0) id2desc[id] = this;
    }
}


static bool online_flag = false;
contact_state_e self_state; // cleared in handshake
byte lastmypubid[TOX_ADDRESS_SIZE]; // cleared in handshake

void update_self()
{
    if (tox)
        tox_self_get_address(tox,lastmypubid);
    
    str_c pubid(TOX_ADDRESS_SIZE * 2, true);
    pubid.append_as_hex(lastmypubid,TOX_ADDRESS_SIZE);

    int m = 0;

    str_c name( tox ? tox_self_get_name_size(tox) : 0, false );
    if (tox)
    {
        tox_self_get_name(tox,(byte*)name.str());
        m |= CDM_NAME;
    }

    str_c statusmsg(tox ? tox_self_get_status_message_size(tox) : 0, false);
    if (tox)
    {
        tox_self_get_status_message(tox, (byte*)statusmsg.str());
        m |= CDM_STATUSMSG;
    }

    contact_data_s self( 0, CDM_PUBID | CDM_STATE | CDM_ONLINE_STATE | CDM_GENDER | CDM_AVATAR_TAG | m );
    self.public_id = pubid.cstr();
    self.public_id_len = pubid.get_length();
    self.name = name.cstr();
    self.name_len = name.get_length();
    self.status_message = statusmsg.cstr();
    self.status_message_len = statusmsg.get_length();
    self.state = self_state;
    self.avatar_tag = 0;
    hf->update_contact(&self);
}

static void update_contact( const contact_descriptor_s *desc )
{
    if (ASSERT(tox))
    {
        contact_data_s cd( desc->get_id(), CDM_PUBID | CDM_STATE | CDM_ONLINE_STATE | CDM_AVATAR_TAG );

        TOX_ERR_FRIEND_QUERY err = TOX_ERR_FRIEND_QUERY_NULL;
        TOX_USER_STATUS st = desc->get_fid() >= 0 ? tox_friend_get_status(tox, desc->get_fid(), &err) : (TOX_USER_STATUS)(-1);
        if (err != TOX_ERR_FRIEND_QUERY_OK) st = (TOX_USER_STATUS)(-1);

        sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        if (desc->get_fid() >= 0)
        {
            byte id[TOX_PUBLIC_KEY_SIZE];
            TOX_ERR_FRIEND_GET_PUBLIC_KEY e;
            tox_friend_get_public_key(tox, desc->get_fid(), id, &e);
            if (TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK != e) memset(id,0,sizeof(id));
            pubid.append_as_hex(id, TOX_PUBLIC_KEY_SIZE);
            ASSERT(pubid.beginof(desc->pubid));
        } else
        {
            pubid.append( asptr( desc->pubid.cstr(), TOX_PUBLIC_KEY_SIZE * 2 ) );
        }

        cd.public_id = pubid.cstr();
        cd.public_id_len = pubid.get_length();

        cd.avatar_tag = desc->avatar_tag;

        str_c name, statusmsg;

        if (st >= (TOX_USER_STATUS)0)
        {
            TOX_ERR_FRIEND_QUERY er;
            name.set_length(tox_friend_get_name_size(tox, desc->get_fid(), &er));
            tox_friend_get_name(tox, desc->get_fid(), (byte*)name.str(), &er);
            cd.name = name.cstr();
            cd.name_len = name.get_length();

            statusmsg.set_length(tox_friend_get_status_message_size(tox, desc->get_fid(), &er));
            tox_friend_get_status_message(tox, desc->get_fid(), (byte*)statusmsg.str(), &er);
            cd.status_message = statusmsg.cstr();
            cd.status_message_len = statusmsg.get_length();

            cd.mask |= CDM_NAME | CDM_STATUSMSG;
        }

        if (st < (TOX_USER_STATUS)0 || desc->state != CS_OFFLINE)
        {
            cd.state = desc->state;
            if ( cd.state == CS_INVITE_SEND )
            {
                cd.public_id = desc->pubid.cstr();
                cd.public_id_len = desc->pubid.get_length();
            }
            st = TOX_USER_STATUS_NONE;

        } else
        {
            cd.state = tox_friend_get_connection_status(tox, desc->get_fid(), nullptr) != TOX_CONNECTION_NONE ? CS_ONLINE : CS_OFFLINE;
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

        hf->update_contact(&cd);
    }
}

asptr message_part_s::extract_create_time(u64 &t, const asptr &msgbody, int cid)
{
    int offset = 0;
    if (msgbody.s[0] == '\1')
    {
        offset = CHARz_findn(msgbody.s, '\2', msgbody.l);
        if (offset < 0)
        {
            offset = 1;
        }
        else {
            ++offset;
            t = pstr_c(asptr(msgbody.s + 1, offset - 2)).as_num<u64>();

            if (contact_descriptor_s *desc = find_descriptor(cid))
                t += desc->correct_create_time;
        }
    }

    if (t == 0) t = now();
    return msgbody.skip(offset);
}

static contact_descriptor_s * find_descriptor(int cid)
{
    auto it = id2desc.find(cid);
    if (it != id2desc.end())
        return it->second;
    return nullptr;
}

static contact_descriptor_s * find_restore_descriptor(int fid)
{
    auto it = fid2desc.find(fid);
    if (it != fid2desc.end())
        return it->second;

    if (fid >= GROUP_ID_OFFSET)
        return nullptr;

    if (!tox || !tox_friend_exists(tox,fid)) return nullptr;

    contact_descriptor_s *desc = new contact_descriptor_s(ID_CONTACT, fid);

    byte id[TOX_PUBLIC_KEY_SIZE];
    tox_friend_get_public_key(tox, fid, id, nullptr);

    desc->pubid.append_as_hex(id, TOX_PUBLIC_KEY_SIZE);
    desc->state = CS_OFFLINE;

    ASSERT( contact_descriptor_s::find(desc->pubid) == desc ); // check that such pubid is single

    contact_data_s cd( desc->get_id(), CDM_PUBID | CDM_STATE );
    cd.public_id = desc->pubid.cstr();
    cd.public_id_len = desc->pubid.get_length();
    cd.state = CS_OFFLINE;
    hf->update_contact(&cd);

    return desc;
}

static void update_init_contact(int fid)
{
    contact_descriptor_s *desc = find_restore_descriptor(fid);
    if (desc) update_contact(desc);
}

static int find_tox_unknown_contact(const byte *id, const asptr &name)
{
    str_c pubid;
    pubid.append_as_hex(id, TOX_PUBLIC_KEY_SIZE);
    contact_descriptor_s *desc = contact_descriptor_s::find(pubid);
    if (desc == nullptr)
    {
        ASSERT(name.l);
        desc = new contact_descriptor_s(ID_UNKNOWN);
        desc->state = CS_UNKNOWN;
        desc->pubid = pubid;
    }

    if (name.l)
    {
        contact_data_s cdata(desc->get_id(), CDM_NAME | CDM_STATE);
        cdata.name = name.s;
        cdata.name_len = name.l;
        cdata.state = CS_UNKNOWN;
        hf->update_contact(&cdata);
    }
    
    return desc->get_fidgnum();
}

static int find_tox_fid(const byte *id)
{
    u32 r = tox_friend_by_public_key(tox, id, nullptr);
    if (r == UINT32_MAX) return -1;
    return r;

    /*
    byte pubkey[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_FRIEND_GET_PUBLIC_KEY err;
    if (!ASSERT(tox)) return -1;
    int n = tox_self_get_friend_list_size(tox);
    for(int i=0;i<n;++i)
    {
        if (tox_friend_exists(tox,i))
        {
            tox_friend_get_public_key(tox,i,pubkey,&err);
            if (0 == memcmp(pubkey,id,TOX_PUBLIC_KEY_SIZE))
                return i;
        }
    }
    return -1;
    */
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cb_friend_request(Tox *, const byte *id, const byte *msg, size_t length, void *)
{
    str_c pubid;
    pubid.append_as_hex(id,TOX_PUBLIC_KEY_SIZE);
    contact_descriptor_s *desc = contact_descriptor_s::find(pubid);
    if (!desc) desc = new contact_descriptor_s( ID_CONTACT );
    desc->pubid = pubid;
    desc->state = CS_INVITE_RECEIVE;

    int fid = find_tox_fid(id);
    desc->set_fid( fid, fid >= 0 );

    contact_data_s cd( desc->get_id(), CDM_PUBID | CDM_STATE );
    cd.public_id = desc->pubid.cstr();
    cd.public_id_len = TOX_PUBLIC_KEY_SIZE * 2;
    cd.state = desc->state;
    hf->update_contact(&cd);

    time_t create_time = now();

    hf->message(MT_FRIEND_REQUEST, 0, desc->get_id(), create_time, (const char *)msg, length);
}

static void cb_friend_message(Tox *, uint32_t fid, TOX_MESSAGE_TYPE type, const byte *message, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        for (auto it = other_typing.begin(); it != other_typing.end(); ++it)
        {
            if (it->fid == (int)fid)
            {
                other_typing.erase(it);
                break;
            }
        }

        message_part_s::msg(TOX_MESSAGE_TYPE_NORMAL == type ? MT_MESSAGE : MT_ACTION, desc->get_id(), 0, (const char *)message, length);
    }
}

static void cb_name_change(Tox *, uint32_t fid, const byte * newname, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        contact_data_s cd(desc->get_id(), CDM_NAME);
        cd.name = (const char *)newname;
        cd.name_len = length;
        hf->update_contact(&cd);
    }
}

static void cb_status_message(Tox *, uint32_t fid, const byte * newstatus, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        contact_data_s cd( desc->get_id(), CDM_STATUSMSG );
        cd.status_message = (const char *)newstatus;
        cd.status_message_len = length;

        hf->update_contact(&cd);
    }
}

static void cb_friend_status(Tox *, uint32_t fid, TOX_USER_STATUS status, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        desc->state = CS_OFFLINE;
        contact_data_s cd(desc->get_id(), CDM_ONLINE_STATE);
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
        hf->update_contact(&cd);
    }
}

static void cb_read_receipt(Tox *, uint32_t fid, uint32_t message_id, void *)
{
    message2send_s::read(fid, message_id);
}

static void cb_isotoxin(Tox *, uint32_t fid, const byte *data, size_t len, void * /*object*/)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        desc->wait_client_id = 0;
        token<char> t(asptr((const char *)data + 1, len - 1), '/');
        SETUPFLAG(desc->flags, contact_descriptor_s::F_ISOTOXIN, t && t->equals(CONSTASTR("isotoxin")));
        if (ISFLAG(desc->flags, contact_descriptor_s::F_ISOTOXIN))
        {
            ++t; // version
            ++t; // time of peer
            if (t)
            {
                time_t remote_peer_time = t->as_num<u64>();
                desc->correct_create_time = (int)((long long)now() - (long long)remote_peer_time);
                SETUPFLAG( desc->flags, contact_descriptor_s::F_NEED_RESYNC, abs( desc->correct_create_time ) > 10 ); // 10+ seconds time delta :( resync it every 2 min
                if (ISFLAG( desc->flags, contact_descriptor_s::F_NEED_RESYNC)) desc->next_sync = time_ms() + 120000;
                ++t;
                if (t && t->equals(CONSTASTR("resync"))) desc->send_identity(false);
            }
        }
    }
}

static void cb_connection_status(Tox *, uint32_t fid, TOX_CONNECTION connection_status, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        bool prev_online = desc->is_online();
        bool accepted = desc->state == CS_INVITE_SEND;
        desc->state = CS_OFFLINE;
        contact_data_s cd(desc->get_id(), CDM_STATE);
        cd.state = (TOX_CONNECTION_NONE != connection_status) ? CS_ONLINE : CS_OFFLINE;
        SETUPFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE, CS_ONLINE == cd.state );

        if (accepted)
        {
            cd.mask |= CDM_PUBID;
            cd.public_id = desc->pubid.cstr();
            cd.public_id_len = TOX_PUBLIC_KEY_SIZE * 2;
        }

        hf->update_contact(&cd);

        if (!prev_online && desc->is_online())
        {
            desc->wait_client_id = time_ms() + 2000; // wait 2 sec client name
            if (desc->wait_client_id == 0) desc->wait_client_id++;
            desc->send_identity(false);
            desc->send_avatar();

        } else if (!desc->is_online())
        {
            desc->on_offline();
        }
    }
}

static void cb_file_chunk_request(Tox *, uint32_t fid, uint32_t file_number, u64 position, size_t length, void *)
{
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->fid == fid && f->fnn == file_number)
        {
            f->transmit(transmitting_data_s::req(position, length));
            break;
        }
}

static void cb_tox_file_recv(Tox *, uint32_t fid, uint32_t filenumber, uint32_t kind, u64 filesize, const byte *filename, size_t filename_length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        if (TOX_FILE_KIND_AVATAR == kind)
        {
            if (desc->avatar_recv_fnn >= 0)
                desc->avatar_recv_fnn = -1;

            if (0 == filesize)
            {
                tox_file_control(tox, fid, filenumber, TOX_FILE_CONTROL_CANCEL, nullptr);

                memset(desc->avatar_hash, 0, TOX_HASH_LENGTH);
                if (desc->avatar_tag == 0) return;

                // avatar changed to none, so just send empty avatar to application (avatar tag == 0)

                desc->avatar_tag = 0;
                contact_data_s cd(desc->get_id(), CDM_AVATAR_TAG);
                cd.avatar_tag = 0;
                hf->update_contact(&cd);
                hf->save();

            } else
            {
                byte hash[TOX_HASH_LENGTH] = {0};
                if (!tox_file_get_file_id(tox,fid,filenumber,hash,nullptr) || 0 == memcmp(hash, desc->avatar_hash, TOX_HASH_LENGTH))
                {
                    // same avatar - cancel avatar transfer
                    tox_file_control(tox, fid, filenumber, TOX_FILE_CONTROL_CANCEL, nullptr);

                } else
                {
                    // avatar changed
                    memcpy(desc->avatar_hash, hash, TOX_HASH_LENGTH);
                    ++desc->avatar_tag; // new avatar version

                    desc->avatar_recv_fnn = filenumber;
                    new incoming_avatar_s(fid, filenumber, filesize, asptr((const char *)filename, filename_length)); // not memleak

                    contact_data_s cd(desc->get_id(), CDM_AVATAR_TAG);
                    cd.avatar_tag = desc->avatar_tag;
                    hf->update_contact(&cd);
                    hf->save();
                }
            }
            return;
        }


        incoming_file_s *f = new incoming_file_s(fid, filenumber, (TOX_FILE_KIND)kind, filesize, asptr((const char *)filename, filename_length)); // not memleak
        hf->incoming_file(desc->get_id(), f->utag, filesize, (const char *)filename, filename_length);
    }

}

static void cb_file_recv_control(Tox *, uint32_t fid, uint32_t filenumber, TOX_FILE_CONTROL control, void * /*userdata*/)
{
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

static void cb_tox_file_recv_chunk(Tox *, uint32_t fid, uint32_t filenumber, u64 position, const byte *data, size_t length, void * /*userdata*/)
{
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->fid == fid && f->fnn == filenumber)
        {
            if (length == 0)
                f->finished();
            else
                f->recv_data( position, data, length );
            break;
        }
}

static void cb_friend_typing(Tox *, uint32_t fid, bool is_typing, void * /*userdata*/)
{
    for( auto it = other_typing.begin(); it != other_typing.end(); ++it )
    {
        if (it->fid == (int)fid)
        {
            if (!is_typing)
                other_typing.erase(it);
            return;
        }
    }

    if (is_typing)
        other_typing.emplace_back( fid, time_ms() );
}

static void setup_members_and_send(contact_data_s &cdata, int gnum) // cdata.members is undefined after call this function
{
    cdata.members_count = tox_group_number_peers(tox, gnum);
    cdata.members = (int *)_alloca(cdata.members_count * sizeof(int));
    int mm = 0;
    for (int m = 0; m < cdata.members_count; ++m)
    {
        uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
        tox_group_peer_pubkey(tox, gnum, m, pubkey);
        if (0 == memcmp(lastmypubid, pubkey, TOX_PUBLIC_KEY_SIZE))
            continue; // do not put self to members list

        sstr_t<TOX_MAX_NAME_LENGTH + 16> name;
        int nl = tox_group_peername(tox, gnum, m, (uint8_t *)name.str());
        if (nl < 0) name.clear(); else name.set_length(nl);

        int cid = 0;
        int fid = find_tox_fid(pubkey);

        if (fid < 0) // negative means not found
            fid = find_tox_unknown_contact(pubkey, name);

        if (contact_descriptor_s *desc = find_restore_descriptor(fid))
        {
            cid = desc->get_id();
            if (CS_INVITE_RECEIVE == desc->state || CS_INVITE_SEND == desc->state)
            {
                // wow, we know name of invited friend!

                contact_data_s cd(cid, CDM_NAME);
                cd.name = name.cstr();
                cd.name_len = name.get_length();
                hf->update_contact(&cd);
            }
        }

        if (cid == 0)
            continue;

        cdata.members[mm++] = cid;
    }
    cdata.members_count = mm;

    hf->update_contact(&cdata);

}

static void callback_av_group_audio(Tox *, int gnum, int peernum, const int16_t *pcm, unsigned int samples, uint8_t channels, unsigned int sample_rate, void * /*userdata*/)
{
    uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
    tox_group_peer_pubkey(tox, gnum, peernum, pubkey);
    if (0 == memcmp(lastmypubid, pubkey, TOX_PUBLIC_KEY_SIZE))
        return; // ignore self message

    if (contact_descriptor_s *gdesc = find_restore_descriptor(gnum + GROUP_ID_OFFSET))
    {
        int cid = 0;
        int fid = find_tox_fid(pubkey);
        if (fid < 0)
            fid = find_tox_unknown_contact(pubkey, asptr());

        if (contact_descriptor_s *desc = find_restore_descriptor(fid))
            cid = desc->get_id();

        audio_format_s fmt;
        fmt.sample_rate = sample_rate;
        fmt.bits = 16;
        fmt.channels = channels;

        hf->play_audio(gdesc->get_id(), cid, &fmt, pcm, (int)samples * fmt.channels * fmt.bits / 8);
    }
}

static void cb_group_invite(Tox *, int fid, byte t, const byte * data, uint16_t length, void *)
{
    bool persistent = false;

    int gnum = t == TOX_GROUPCHAT_TYPE_TEXT ? tox_join_groupchat(tox, fid, data, length) : toxav_join_av_groupchat(tox, fid, data, length, callback_av_group_audio, nullptr);
    if (gnum >= 0)
    {
        sstr_t<TOX_MAX_NAME_LENGTH + 16> gn;
        int l = tox_group_get_title(tox, gnum, (uint8_t *)gn.str(), gn.get_capacity() );
        if (l < 0) gn.clear(); else gn.set_length(l);

        contact_descriptor_s *desc = new contact_descriptor_s(ID_GROUP, gnum + GROUP_ID_OFFSET);
        desc->state = CS_ONLINE; // groups always online

        unsigned int am = 0;
        if (t != TOX_GROUPCHAT_TYPE_TEXT)
        {
            am = CDF_AUDIO_GCHAT;
            auto w = state.lock_write();
            for (int i = MAX_CALLS; i < MAX_CALLS * 2; ++i)
            {
                stream_settings_s &ss = w().local_peer_settings[i];
                if (ss.owner_cid == 0)
                {
                    ss.owner_cid = desc->get_id();
                    ss.groupnum = gnum;
                    desc->audiostream = i;
                    break;
                }
            }
        }

        contact_data_s cdata(desc->get_id(), CDM_STATE | CDM_NAME | CDM_MEMBERS | CDM_PERMISSIONS | am | (persistent ? CDF_PERSISTENT_GCHAT : 0));
        cdata.state = CS_ONLINE;
        cdata.name = gn.cstr();
        cdata.name_len = gn.get_length();
        cdata.groupchat_permissions = -1;

        setup_members_and_send(cdata, gnum);
    }
}

static void gchat_message( int gnum, int peernum, const char * message, int length, bool is_message )
{
    uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
    tox_group_peer_pubkey(tox, gnum, peernum, pubkey);
    if (0 == memcmp(lastmypubid, pubkey, TOX_PUBLIC_KEY_SIZE))
        return; // ignore self message

    if (contact_descriptor_s *gdesc = find_restore_descriptor(gnum + GROUP_ID_OFFSET))
    {
        int cid = 0;
        int fid = find_tox_fid(pubkey);
        if (fid < 0)
            fid = find_tox_unknown_contact(pubkey, asptr());

        if (contact_descriptor_s *desc = find_restore_descriptor(fid))
            cid = desc->get_id();

        hf->message(is_message ? MT_MESSAGE : MT_ACTION, gdesc->get_id(), cid, now(), message, length);
    }
}

static void cb_group_message(Tox *, int gnum, int peernum, const byte * message, uint16_t length, void *)
{
    gchat_message( gnum, peernum, (const char *)message, length, true );
}

static void cb_group_action(Tox *, int gnum, int peernum, const byte * message, uint16_t length, void *)
{
    gchat_message( gnum, peernum, (const char *)message, length, false );
}

static void cb_group_namelist_change(Tox *, int gnum, int /*pid*/, byte /*change*/, void *)
{
    // any change - rebuild members list
    
    if (contact_descriptor_s *desc = find_restore_descriptor(gnum + GROUP_ID_OFFSET))
    {
        contact_data_s cdata(desc->get_id(), CDM_MEMBERS);
        setup_members_and_send(cdata, gnum);
    }
}

static void cb_group_title(Tox *, int gnum, int /*pid*/, const byte * title, byte length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(gnum + GROUP_ID_OFFSET))
    {
        str_c gn( (const char *)title, length );

        contact_data_s cdata(desc->get_id(), CDM_NAME);
        cdata.name = gn.cstr();
        cdata.name_len = gn.get_length();

        hf->update_contact( &cdata );
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox AV callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cb_toxav_invite(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        ASSERT(desc->audiostream < 0);
        if (desc->audiostream >= 0 && desc->audiostream != call_index)
            toxav_stop_call(toxav,desc->audiostream);

        desc->audiostream = call_index;
        ToxAvCSettings peer_settings;
        toxav_get_peer_csettings(toxav, call_index, 0, &peer_settings);
        bool video = (peer_settings.call_type == av_TypeVideo);
        if (video)
            hf->message(MT_INCOMING_CALL, 0, desc->get_id(), now(), "video", 5);
        else
            hf->message(MT_INCOMING_CALL, 0, desc->get_id(), now(), "audio", 5);
    }
}


static void cb_toxav_start(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;

    ToxAvCSettings remote_settings;

    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        ASSERT( call_index == desc->audiostream );
        toxav_get_peer_csettings(toxav, call_index, 0, &remote_settings);

        //bool video = (peer_settings.call_type == av_TypeVideo);

        toxav_prepare_transmission(toxav, call_index, 0 /*video = 1*/);
        if (ISFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START))
        {
            UNSETFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
            hf->message(MT_CALL_ACCEPTED, 0, desc->get_id(), now(), nullptr, 0);
        }
    }
}

static void call_stop_int( int32_t call_index )
{
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        if (desc->audiostream >= 0)
        {
            ASSERT(desc->audiostream == call_index);
            hf->message(MT_CALL_STOP, 0, desc->get_id(), now(), nullptr, 0);
            desc->audiostream = -1;
            UNSETFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
        }
    }
}

static void cb_toxav_cancel(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    toxav_kill_transmission(toxav, call_index);
    call_stop_int(call_index);
}

static void cb_toxav_reject(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    call_stop_int(call_index);
}

static void cb_toxav_end(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    toxav_kill_transmission(toxav, call_index);
    call_stop_int(call_index);
}

static void cb_toxav_ringing(void * /*av*/, int32_t /*call_index*/, void * /*userdata*/)
{
}

static void cb_toxav_requesttimeout(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    call_stop_int(call_index);
}

static void cb_toxav_peertimeout(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    toxav_kill_transmission(toxav, call_index);
    call_stop_int(call_index);
}

static void cb_toxav_selfmediachange(void * /*av*/, int32_t /*call_index*/, void * /*userdata*/)
{
}

static void cb_toxav_peermediachange(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        if (toxav_get_peer_csettings(toxav, call_index, 0, remote_audio_fmt + call_index) == av_ErrorNone)
        {
            //bool video = settings.call_type == av_TypeVideo;
        }
    }
}

static void cb_toxav_audio(void *av, int32_t call_index, const int16_t *data, uint16_t samples, void * /*userdata*/)
{
    if (av != toxav) return;
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
        if (toxav_get_peer_csettings(toxav, call_index, 0, remote_audio_fmt + call_index) == av_ErrorNone)
        {
            if (remote_audio_fmt[call_index].audio_channels == 1 || remote_audio_fmt[call_index].audio_channels == 2)
            {
                audio_format_s fmt = remote_audio_fmt[call_index];
                hf->play_audio(0, desc->get_id(), &fmt, data, (int)samples * fmt.channels * fmt.bits / 8);
            }
        }
}

static void audio_sender()
{
    state.lock_write()().audio_sender = true;

    std::vector<byte> prebuffer; // just ones allocated buffer for raw audio
    std::vector<byte> compressed; // just ones allocated buffer for raw audio
    
    int sleepms = 100;
    for(;; Sleep(sleepms))
    {
        auto w = state.lock_write();
        if (w().audio_sender_need_stop) break;
        if (w().active_calls == 0) { sleepms = 100; continue; }
        sleepms = 1;
        int ct = time_ms();
        for(int i=0;i<(MAX_CALLS*2);++i)
        {
            if (!w.is_locked()) w = state.lock_write();

            unsigned long mask = (1<<i);
            if (w().active_calls < mask) break;

            if ( 0 != ( mask & w().active_calls) )
            {
                stream_settings_s &ss = w().local_peer_settings[i];
                int avsize = ss.fifo.available();
                if (avsize == 0)
                {
                    ss.owner_cid = 0;
                    ss.processing = false;
                    ss.groupnum = -1;
                    w().active_calls &= ~mask;
                    continue;
                }

                if ((ss.next_time_send - ct) > 0 && (ss.next_time_send - ct) <= ss.audio_frame_duration)
                    continue;

                audio_format_s fmt = ss;
                int req_frame_size = fmt.avgBytesPerMSecs(ss.audio_frame_duration);
                if (req_frame_size > avsize) 
                    continue; // not yet

                if (!ss.processing && (req_frame_size * 3) > avsize)
                    continue;

                ss.processing = true;
                ss.next_time_send += ss.audio_frame_duration;
                if ((ss.next_time_send - ct) < 0) ss.next_time_send = ct + ss.audio_frame_duration/2;

                if (req_frame_size >(int)prebuffer.size()) prebuffer.resize(req_frame_size);
                int samples = ss.fifo.read_data(prebuffer.data(), req_frame_size) / fmt.blockAlign();
                w.unlock(); // no need lock anymore

                ASSERT( fmt.bits == 16 );

                if (req_frame_size >(int)compressed.size()) compressed.resize(req_frame_size);

                if (ss.groupnum >= 0)
                {
                    toxav_group_send_audio(tox, ss.groupnum, (int16_t *)prebuffer.data(), samples, (byte)fmt.channels, fmt.sample_rate );

                } else
                {
                    int r = 0;
                    if ((r = toxav_prepare_audio_frame(toxav, i, compressed.data(), req_frame_size, (int16_t *)prebuffer.data(), samples)) > 0)
                    {
                        toxav_send_audio(toxav, i, compressed.data(), r);
                    }
                }

            }
        }
        if (w.is_locked()) w.unlock();

    }

    state.lock_write()().audio_sender = false;
}

static DWORD WINAPI audio_sender(LPVOID)
{
    UNSTABLE_CODE_PROLOG
    audio_sender();
    UNSTABLE_CODE_EPILOG
    return 0;
}

static TOX_ERR_NEW prepare(const byte *data, size_t length)
{
    if (tox) tox_kill(tox);

    OSVERSIONINFO v = {sizeof(OSVERSIONINFO),0};
    GetVersionExW(&v);
    options.ipv6_enabled = (v.dwMajorVersion >= 6) ? 1 : 0;
    options.proxy_host = tox_proxy_host.cstr();
    options.proxy_port = 0;
    options.proxy_type = TOX_PROXY_TYPE_NONE;
    if (tox_proxy_type & CF_PROXY_SUPPORT_HTTP)
        options.proxy_type = TOX_PROXY_TYPE_HTTP;
    if (tox_proxy_type & (CF_PROXY_SUPPORT_SOCKS4|CF_PROXY_SUPPORT_SOCKS5))
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

    options.savedata_type = data ? TOX_SAVEDATA_TYPE_TOX_SAVE : TOX_SAVEDATA_TYPE_NONE;
    options.savedata_data = data;
    options.savedata_length = length;

    TOX_ERR_NEW errnew;
    tox = tox_new(&options, &errnew);
    if (!tox)
        return errnew;

    tox_callback_friend_lossless_packet(tox, cb_isotoxin, nullptr);

    tox_callback_friend_request(tox, cb_friend_request, nullptr);
    tox_callback_friend_message(tox, cb_friend_message, nullptr);
    tox_callback_friend_name(tox, cb_name_change, nullptr);
    tox_callback_friend_status_message(tox, cb_status_message, nullptr);
    tox_callback_friend_status(tox, cb_friend_status, nullptr);
    tox_callback_friend_typing(tox, cb_friend_typing, nullptr);
    tox_callback_friend_read_receipt(tox, cb_read_receipt, nullptr);
    tox_callback_friend_connection_status(tox, cb_connection_status, nullptr);

    tox_callback_group_invite(tox, cb_group_invite, nullptr);
    tox_callback_group_message(tox, cb_group_message, nullptr);
    tox_callback_group_action(tox, cb_group_action, nullptr);
    tox_callback_group_namelist_change(tox, cb_group_namelist_change, nullptr);
    tox_callback_group_title(tox, cb_group_title, nullptr);

    tox_callback_file_recv_control(tox, cb_file_recv_control, nullptr);
    tox_callback_file_chunk_request(tox, cb_file_chunk_request, nullptr);
    tox_callback_file_recv(tox, cb_tox_file_recv, nullptr);
    tox_callback_file_recv_chunk(tox, cb_tox_file_recv_chunk, nullptr);


    toxav = toxav_new( tox, MAX_CALLS );

    toxav_register_callstate_callback(toxav, cb_toxav_invite, av_OnInvite, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_start, av_OnStart, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_cancel, av_OnCancel, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_reject, av_OnReject, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_end, av_OnEnd, nullptr);

    toxav_register_callstate_callback(toxav, cb_toxav_ringing, av_OnRinging, nullptr);

    toxav_register_callstate_callback(toxav, cb_toxav_requesttimeout, av_OnRequestTimeout, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_peertimeout, av_OnPeerTimeout, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_selfmediachange, av_OnSelfCSChange, nullptr);
    toxav_register_callstate_callback(toxav, cb_toxav_peermediachange, av_OnPeerCSChange, nullptr);

    toxav_register_audio_callback(toxav, cb_toxav_audio, nullptr);
    //toxav_register_video_callback(toxav, cb_toxav_video, nullptr);

    if (state.lock_read()().allow_run_audio_sender())
    {
        CloseHandle(CreateThread(nullptr, 0, audio_sender, nullptr, 0, nullptr));
        for (; !state.lock_read()().audio_sender; Sleep(1)); // wait audio sender start
    }

    return TOX_ERR_NEW_OK;
}


static void connect()
{
    auto get_node = []()->const dht_node_s&
    {
        int min_used = INT_MAX;
        for( dht_node_s &node : nodes )
        {
            if (node.used < min_used)
                min_used = node.used;
            node.random = rand();
        }
        dht_node_s *n2r = nullptr;
        int max_rand = -1;
        for (dht_node_s &node : nodes)
        {
            if ( node.used == min_used && node.random > max_rand )
            {
                n2r = &node;
            }
        }

        ++n2r->used;
        return *n2r;

    };
    
    int n = min(4,nodes.size());
    for (int i = 0; i < n; ++i)
    {
        const dht_node_s &node = get_node();
        TOX_ERR_BOOTSTRAP er;
        tox_bootstrap(tox, node.addr.cstr(), (uint16_t)node.port, node.pubid, &er);
        if (TOX_ERR_BOOTSTRAP_BAD_HOST == er)
        {
            ++n;
            if (n > (int)nodes.size() && i >= (int)nodes.size()) break;
            continue;
        }
        tox_add_tcp_relay(tox, node.addr.cstr(), (uint16_t)node.port, node.pubid, &er);
    }

    
}

void __stdcall offline();
void __stdcall online();

void __stdcall tick(int *sleep_time_ms)
{
    if (!online_flag)
    {
        *sleep_time_ms = 100;
        self_typing_contact = 0;
        return;
    }
    *sleep_time_ms = 1;
    int curt = time_ms();
    static int nextt = curt;
    static int nexttav = curt;
    static int nexttresync = curt;
    static time_t tryconnect = now() + 60;
    if (tox)
    {
        // self typing
        // may be time to stop typing?
        if (self_typing_contact && (curt-self_typing_time) > 0)
        {
            if (contact_descriptor_s *cd = find_descriptor(self_typing_contact))
                tox_self_set_typing(tox, cd->get_fid(), false, nullptr);
            self_typing_contact = 0;
        }

        // other peers typing
        // notify host
        for(other_typing_s &ot : other_typing)
        {
            if ( (curt-ot.time) > 0 )
            {
                if (contact_descriptor_s *desc = find_restore_descriptor(ot.fid))
                    hf->typing(desc->get_id());
                ot.time += 1000;
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
            //DWORD xxx = time_ms();
            tox_iterate(tox);
            //DWORD yyy = time_ms();
            //DWORD zzz = yyy - xxx;
            nextt = curt + tox_iteration_interval(tox);

            byte id[TOX_ADDRESS_SIZE];
            tox_self_get_address(tox,id);
            if (0 != memcmp(id, lastmypubid, TOX_ADDRESS_SIZE))
                forceupdateself = true;
        }
        contact_state_e nst = tox_self_get_connection_status(tox) != TOX_CONNECTION_NONE ? CS_ONLINE : CS_OFFLINE;
        if (forceupdateself || nst != self_state)
        {
            if (nst == CS_OFFLINE && self_state != CS_OFFLINE)
                tryconnect = now() + 10;

            self_state = nst;
            update_self();

        }
        if (nst == CS_OFFLINE)
        {
            time_t ctime = now();
            if (ctime > tryconnect)
            {
                connect();
                tryconnect = ctime + 10;
            }
        }

        if ((curt - nexttav) > 0)
        {
            toxav_do(toxav);
            nexttav = curt + toxav_do_interval(toxav);
        }

        if ((curt - nexttresync) > 0)
        {
            for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
                if (ISFLAG(f->flags, contact_descriptor_s::F_NEED_RESYNC) && f->is_online() && (curt - f->next_sync) > 0)
                {
                    f->next_sync = curt + 120000;
                    f->send_identity(true);
                    break;
                }
            nexttresync = curt + 3001;
        }

        int nextticktime = min( min( nexttresync - curt, nexttav - curt ), nextt - curt );
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

void __stdcall goodbye()
{
    id2desc.clear();
    fid2desc.clear();
    for(;contact_descriptor_s::first_desc;)
        contact_descriptor_s::first_desc->die();

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

    while (state.lock_read()().audio_sender)
    {
        state.lock_write()().audio_sender_need_stop = true;
        Sleep(1);
    }

    state.lock_write()().audio_sender_need_stop = false;

    if (tox)
    {
        toxav_kill(toxav);
        toxav = nullptr;

        tox_kill(tox);
        tox = nullptr;
    }
}
void __stdcall set_name(const char*utf8name)
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
void __stdcall set_statusmsg(const char*utf8status)
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

void __stdcall set_ostate(int ostate)
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
void __stdcall set_gender(int /*gender*/)
{
}

void __stdcall get_avatar(int id)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        cd->recv_avatar();
    }
}

void __stdcall set_avatar(const void*data, int isz)
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
    }
    return CR_UNKNOWN_ERROR;
}

static void send_configurable()
{
    const char * fields[] = { CFGF_PROXY_TYPE, CFGF_PROXY_ADDR, CFGF_UDP_ENABLE, CFGF_SERVER_PORT };
    str_c svalues[4];
    svalues[0].set_as_int(tox_proxy_type);
    if (tox_proxy_type) svalues[1].set(tox_proxy_host).append_char(':').append_as_uint(tox_proxy_port);
    svalues[2].set_as_int(options.udp_enabled ? 1 : 0);
    svalues[3].set_as_int(options.tcp_port);
    const char * values[] = { svalues[0].cstr(), svalues[1].cstr(), svalues[2].cstr(), svalues[3].cstr() };

    hf->configurable(4, fields, values);
}



void __stdcall set_config(const void*data, int isz)
{
    struct on_return
    {
        TOX_ERR_NEW toxerr = TOX_ERR_NEW_OK;
        ~on_return()
        {
            if (!tox)
                hf->operation_result(LOP_SETCONFIG, tox_err_to_cmd_result(toxerr));
        }
        void operator=(TOX_ERR_NEW err)
        {
            toxerr = err;
        }
    } toxerr;

    u64 _now = now();
    if (isz>8 && (*(uint32_t *)data) == 0 && (*((uint32_t *)data+1)) == 0x15ed1b1f)
    {
        tox_proxy_type = 0;
        memset( &options, 0, sizeof(options) );
        options.udp_enabled = true;

        // raw tox_save
        toxerr = prepare( (const byte *)data, isz );

    } else if (isz>4 && (*(uint32_t *)data) != 0)
    {
        loader ldr(data,isz);

        if (ldr(chunk_proxy_type))
            tox_proxy_type = ldr.get_int();
        if (ldr(chunk_proxy_address))
            set_proxy_addr(ldr.get_astr());
        if (ldr(chunk_server_port))
            options.tcp_port = (uint16_t)ldr.get_int();
        if (ldr(chunk_use_udp))
            options.udp_enabled = ldr.get_int() != 0;
        if (int sz = ldr(chunk_toxid))
        {
            loader l(ldr.chunkdata(), sz);
            int dsz;
            if (const void *toxid = l.get_data(dsz))
                if (TOX_ADDRESS_SIZE == dsz)
                    memcpy( lastmypubid, toxid, TOX_ADDRESS_SIZE);
        }

        if (int sz = ldr(chunk_tox_data))
        {
            loader l(ldr.chunkdata(), sz);
            int dsz;
            if (const void *toxdata = l.get_data(dsz))
            {
                toxerr = prepare((const byte *)toxdata, dsz);
            }
        } else
            if (!tox) toxerr = prepare(nullptr,0); // prepare anyway
        
        if (!tox)
            return;

        if (int sz = ldr(chunk_descriptors))
        {
            id2desc.clear();
            fid2desc.clear();
            for (; contact_descriptor_s::first_desc;)
                contact_descriptor_s::first_desc->die();


            loader l(ldr.chunkdata(), sz);
            for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                if (int descriptor_size = l(chunk_descriptor))
                {
                    contact_descriptor_s *desc = new contact_descriptor_s( SKIP_ID_GEN );
                    loader lc(l.chunkdata(), descriptor_size);

                    int id = 0, fid = -1;
                    bool fid_ok = false;

                    if (lc(chunk_descriptor_id))
                        id = lc.get_int();
                    if (lc(chunk_descriptor_pubid))
                        desc->pubid = lc.get_astr();
                    if (lc(chunk_descriptor_state))
                        desc->state = (contact_state_e)lc.get_int();
                    if (lc(chunk_descriptor_avatartag))
                        desc->avatar_tag = lc.get_int();

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

                    if (id == 0) continue;

                    if (desc->state != CS_UNKNOWN)
                    {
                        byte buf[TOX_PUBLIC_KEY_SIZE];
                        desc->pubid.hex2buf<TOX_PUBLIC_KEY_SIZE>(buf);
                        fid = find_tox_fid(buf);

                        if (fid >= 0)
                        {
                            auto it = fid2desc.find(fid);
                            if (it != fid2desc.end())
                            {
                                // oops oops
                                // contact dup!!!
                                delete desc;
                                continue;
                            }
                            fid_ok = true;
                        }
                    } else
                    {
                        fid = -id;
                        fid_ok = true;
                    }


                    if (id != 0) desc->set_id(id);
                    desc->set_fid(fid, fid_ok);
                }
        }

        /*
        if (int sz = ldr(chunk_msgs_sending))
        {
            for (; message2send_s::first;)
                delete message2send_s::first;

            loader l(ldr.chunkdata(), sz);
            for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                if (int message_data_size = l(chunk_msg_sending))
                {
                    loader lc(l.chunkdata(), message_data_size);

                    int fid = 0, mid = 0;
                    message_type_e mt(MT_MESSAGE);
                    u64 utag = 0;
                    u64 create_time = _now;
                    if (lc(chunk_msg_sending_fid))
                        fid = lc.get_int();
                    if (lc(chunk_msg_sending_type))
                        mt = (message_type_e)lc.get_byte();
                    if (lc(chunk_msg_sending_utag))
                        utag = lc.get_u64();
                    if (lc(chunk_msg_sending_mid))
                        mid = lc.get_int();
                    if (lc(chunk_msg_sending_createtime))
                        create_time = lc.get_u64();
                    
                    if (fid && utag)
                    {
                        if (int bsz = lc(chunk_msg_sending_body))
                        {
                            loader mbl(lc.chunkdata(), bsz);
                            int dsz;
                            if (const void *mb = mbl.get_data(dsz))
                                new message2send_s(utag,mt,fid,asptr((const char *)mb, dsz), mid, create_time);
                        }
                    }
                        
                }
        }
        */

        if (int sz = ldr(chunk_msgs_receiving))
        {
            for (; message_part_s::first;)
                delete message_part_s::first;

            loader l(ldr.chunkdata(), sz);
            for (int cnt = l.read_list_size(); cnt > 0; --cnt)
                if (int message_data_size = l(chunk_msg_receiving))
                {
                    loader lc(l.chunkdata(), message_data_size);

                    u64 create_time = _now;
                    int cid = 0;
                    message_type_e mt(MT_MESSAGE);
                    if (lc(chunk_msg_receiving_cid))
                        cid = lc.get_int();
                    if (lc(chunk_msg_receiving_type))
                        mt = (message_type_e)lc.get_byte();
                    if (lc(chunk_msg_receiving_createtime))
                        create_time = lc.get_u64();
                    if (cid)
                        if (int bsz = lc(chunk_msg_receiving_body))
                        {
                            loader mbl(lc.chunkdata(), bsz);
                            int dsz;
                            if (const void *mb = mbl.get_data(dsz))
                                new message_part_s(mt, cid, create_time, asptr((const char *)mb, dsz), true);
                        }
                }
        }

    } else
        if (!tox) toxerr = prepare(nullptr, 0);

    // now send configurable fields to application
    send_configurable();
}

void __stdcall init_done()
{
    update_self();

    if (tox)
    {
        int cnt = tox_self_get_friend_list_size(tox);
        for(int i=0;i<cnt;++i)
        {
            if (tox_friend_exists(tox,i))
                update_init_contact(i);
        }
        for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
        {
            if (f->get_fid() < 0 && f->state == CS_INVITE_RECEIVE)
                update_contact(f);
        }
    }
}

void __stdcall online()
{
    online_flag = true;
    connect();
}

void operator<<(chunk &chunkm, const contact_descriptor_s &desc)
{
    chunk mm(chunkm.b, chunk_descriptor);

    chunk(chunkm.b, chunk_descriptor_id) << desc.get_id();
    chunk(chunkm.b, chunk_descriptor_pubid) << desc.pubid;
    chunk(chunkm.b, chunk_descriptor_state) << (int)desc.state;
    chunk(chunkm.b, chunk_descriptor_avatartag) << desc.avatar_tag;
    if (desc.avatar_tag != 0)
    {
        chunk(chunkm.b, chunk_descriptor_avatarhash) << bytes(desc.avatar_hash, TOX_HASH_LENGTH);
    }
    
}

/*
void operator<<(chunk &chunkm, const message2send_s &m)
{
    chunk mm(chunkm.b, chunk_msg_sending);

    chunk(chunkm.b, chunk_msg_sending_fid) << m.fid;
    chunk(chunkm.b, chunk_msg_sending_type) << (byte)m.mt;
    chunk(chunkm.b, chunk_msg_sending_utag) << m.utag;
    chunk(chunkm.b, chunk_msg_sending_mid) << (m.mid > 0 ? 0 : m.mid);
    chunk(chunkm.b, chunk_msg_sending_createtime) << (u64)m.create_time;
    chunk(chunkm.b, chunk_msg_sending_body) << bytes(m.msg.cstr(), m.msg.get_length());
}
*/

void operator<<(chunk &chunkm, const message_part_s &m)
{
    chunk mm(chunkm.b, chunk_msg_receiving);

    chunk(chunkm.b, chunk_msg_receiving_cid) << m.cid;
    chunk(chunkm.b, chunk_msg_receiving_type) << (byte)m.mt;
    chunk(chunkm.b, chunk_msg_receiving_createtime) << (u64)m.create_time;
    chunk(chunkm.b, chunk_msg_receiving_body) << bytes(m.msgb.cstr(), m.msgb.get_length());
}

static void save_current_stuff( savebuffer &b )
{
    chunk(b, chunk_magic) << (u64)(0x111BADF00D2C0FE6ull + SAVE_VERSION);
    chunk(b, chunk_proxy_type) << tox_proxy_type;
    chunk(b, chunk_proxy_address) << ( str_c(tox_proxy_host).append_char(':').append_as_uint(tox_proxy_port) );
    chunk(b, chunk_server_port) << (int)options.tcp_port;
    chunk(b, chunk_use_udp) << (int)(options.udp_enabled ? 1 : 0);
    chunk(b, chunk_toxid) << bytes(lastmypubid, TOX_ADDRESS_SIZE);

    size_t sz = tox_get_savedata_size(tox);
    void *data = chunk(b, chunk_tox_data).alloc(sz);
    tox_get_savedata(tox, (byte *)data);

    chunk(b, chunk_descriptors) << serlist<contact_descriptor_s>(contact_descriptor_s::first_desc);
    //chunk(b, chunk_msgs_sending) << serlist<message2send_s>(message2send_s::first);
    chunk(b, chunk_msgs_receiving) << serlist<message_part_s>(message_part_s::first);
}

void __stdcall offline()
{
    online_flag = false;

    while (state.lock_read()().audio_sender)
    {
        state.lock_write()().audio_sender_need_stop = true;
        Sleep(1);
    }

    state.lock_write()().audio_sender_need_stop = false;

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

        size_t sz = tox_get_savedata_size(tox);
        std::vector<byte> buf(sz);
        tox_get_savedata(tox, buf.data());
        prepare( (const byte *)buf.data(),sz );

        contact_data_s self(0, CDM_STATE);
        self.state = CS_OFFLINE;
        hf->update_contact(&self);

        for (contact_descriptor_s *f = contact_descriptor_s::first_desc; f; f = f->next)
        {
            f->on_offline();
            update_contact(f);
        }
    }
}

void __stdcall save_config(void * param)
{
    if (tox)
    {
        savebuffer b;
        save_current_stuff(b);
        hf->on_save(b.data(), b.size(), param);
    }
}

static int send_request(const char*public_id, const char* invite_message_utf8, bool resend)
{
    if (tox)
    {
        byte toxaddr[TOX_ADDRESS_SIZE];

        pstr_c s; s.set( asptr(public_id, TOX_ADDRESS_SIZE * 2) );
        s.hex2buf<TOX_ADDRESS_SIZE>(toxaddr);

        if (resend) // before resend we have to delete contact in Tox system, otherwise we will get an error TOX_FAERR_ALREADYSENT
            if (contact_descriptor_s *desc = contact_descriptor_s::find(asptr(public_id, TOX_PUBLIC_KEY_SIZE * 2)))
                tox_friend_delete(tox, desc->get_fid(), nullptr);

        str_c invmsg = utf8clamp(invite_message_utf8, TOX_MAX_FRIEND_REQUEST_LENGTH);
        if (invmsg.get_length() == 0) invmsg = CONSTASTR("?");

        TOX_ERR_FRIEND_ADD er = TOX_ERR_FRIEND_ADD_NULL;
        int fid = tox_friend_add(tox, toxaddr, (const byte *)invmsg.cstr(),invmsg.get_length(), &er);
        switch (er)
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

        contact_descriptor_s *desc = contact_descriptor_s::find( asptr(public_id, TOX_PUBLIC_KEY_SIZE * 2) );
        if (desc) desc->set_fid(fid, fid >= 0);
        else desc = new contact_descriptor_s( ID_CONTACT, fid );
        desc->pubid = s;
        desc->state = CS_INVITE_SEND;

        contact_data_s cdata( desc->get_id(), CDM_PUBID | CDM_STATE );
        cdata.public_id = desc->pubid.cstr();
        cdata.public_id_len = desc->pubid.get_length();
        cdata.state = CS_INVITE_SEND;
        hf->update_contact(&cdata);

        return CR_OK;
    }
    return CR_FUNCTION_NOT_FOUND;
}

int __stdcall resend_request(int id, const char* invite_message_utf8)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return CR_FUNCTION_NOT_FOUND;
        contact_descriptor_s *cd = it->second;
        return send_request(cd->pubid, invite_message_utf8, true);
    }
    return CR_FUNCTION_NOT_FOUND;
}


int __stdcall add_contact(const char* public_id, const char* invite_message_utf8)
{
    pstr_c s = asptr(public_id);
    if (s.get_length() == (TOX_ADDRESS_SIZE * 2) && s.contain_chars(CONSTASTR("0123456789abcdefABCDEF")))
        return send_request(public_id, invite_message_utf8, false);

    if (s.find_pos('@') > 0)
    {
        new discoverer_s(s, asptr(invite_message_utf8));
        return CR_OPERATION_IN_PROGRESS;
    }

    return CR_INVALID_PUB_ID;
}

void __stdcall del_contact(int id)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;
        if (id > 0)
        {
            int chatscount = tox_count_chatlist(tox);
            if (chatscount)
            {
                uint8_t buf[ TOX_PUBLIC_KEY_SIZE ];
                uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
                int32_t *chats = (int *)_alloca( sizeof(int32_t) * chatscount );
                desc->pubid.hex2buf<TOX_PUBLIC_KEY_SIZE>(buf);
                tox_get_chatlist(tox, chats, chatscount);
                for(int i=0;i<chatscount;++i)
                {
                    int gnum = chats[i];
                    int peers = tox_group_number_peers(tox, gnum);
                    for(int p = 0; p < peers; ++p)
                    {
                        tox_group_peer_pubkey(tox, gnum, p, pubkey );
                        if (0 == memcmp(buf, pubkey, sizeof(buf)))
                        {
                            // in da groupchat
                            // no kill
                            // just change to unknown state

                            tox_friend_delete(tox, desc->get_fid(), nullptr);

                            desc->state = CS_UNKNOWN;
                            desc->set_fid( -id, true ); // fid of unknown contacts is -id

                            contact_data_s cdata(desc->get_id(), CDM_STATE);
                            cdata.state = CS_UNKNOWN;
                            hf->update_contact(&cdata);
                            hf->save();
                            return;
                        }
                    }
                }
            }

            tox_friend_delete(tox, desc->get_fid(), nullptr);
            desc->die();
        }
        else
        {
            tox_del_groupchat(tox, desc->get_gnum());
            desc->die();
        }
    }
}

void __stdcall send_message(int id, const message_s *msg)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;

        contact_descriptor_s *desc = it->second;

        if (id == self_typing_contact)
        {
            tox_self_set_typing(tox, desc->get_fid(), false, nullptr);
            self_typing_contact = 0;
        }


        new message2send_s( msg->utag, msg->mt, desc->get_fidgnum(), asptr(msg->message, msg->message_len) ); // not memleak!
        hf->save();
    }
}

void __stdcall del_message( u64 utag )
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

void __stdcall accept(int id)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;
        
        if (desc->pubid.get_length() >= TOX_PUBLIC_KEY_SIZE * 2 && desc->get_fid() < 0)
        {
            byte buf[TOX_PUBLIC_KEY_SIZE];
            desc->pubid.hex2buf<TOX_PUBLIC_KEY_SIZE>(buf);

            TOX_ERR_FRIEND_ADD er;
            int fid = tox_friend_add_norequest(tox, buf, &er);
            if (TOX_ERR_FRIEND_ADD_OK != er)
                return;
            desc->set_fid(fid, fid >= 0);
            desc->state = CS_OFFLINE;

            contact_data_s cdata( desc->get_id(), CDM_STATE );
            cdata.state = CS_WAIT;
            hf->update_contact(&cdata);
        }
    }
}

void __stdcall reject(int id)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;
        desc->state = CS_ROTTEN;

        contact_data_s cdata(desc->get_id(), CDM_STATE);
        cdata.state = CS_ROTTEN;
        hf->update_contact(&cdata);

        if (desc->get_fid() >= 0)
            tox_friend_delete(tox, desc->get_fid(), nullptr);

        desc->die();
    }
}

void __stdcall call(int id, const call_info_s *ci)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (cd->audiostream < 0)
        {
            stream_settings_s sets;
            if (0 == toxav_call(toxav, &cd->audiostream, cd->get_fid(), &sets, ci->duration ))
            {
                SETFLAG(cd->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
                auto w = state.lock_write();
                w().local_peer_settings[cd->audiostream] = sets;
            }
        }
    }
}

void __stdcall stop_call(int id, stop_call_e sc)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (cd->audiostream >= 0)
            if (sc == STOPCALL_HANGUP)
                toxav_hangup(toxav, cd->audiostream);
            else if (sc == STOPCALL_REJECT)
                toxav_reject(toxav, cd->audiostream, nullptr);
            else if (sc == STOPCALL_CANCEL)
                toxav_cancel(toxav, cd->audiostream, cd->get_fid(), nullptr);
        cd->audiostream = -1;
        UNSETFLAG(cd->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
    }
}

void __stdcall accept_call(int id)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (cd->audiostream >= 0)
        {
            auto w = state.lock_write();
            w().local_peer_settings[cd->audiostream].setup();
            toxav_answer(toxav, cd->audiostream, w().local_peer_settings + cd->audiostream);
        }
    }
}

void __stdcall send_audio(int id, const call_info_s * ci)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;

        if (id < 0 && cd->audiostream < 0)
        {
            // group id
            auto w = state.lock_write();
            for(int i=MAX_CALLS; i<MAX_CALLS*2; ++i)
            {
                stream_settings_s &ss = w().local_peer_settings[i];
                if (ss.owner_cid == 0)
                {
                    ss.owner_cid = id;
                    ss.groupnum = cd->get_gnum();
                    cd->audiostream = i;
                    break;
                }
            }
        }

        if (cd->audiostream >= 0)
        {
            auto w = state.lock_write();
            stream_settings_s &ss = w().local_peer_settings[cd->audiostream];
            
            ASSERT(ss.owner_cid == 0 || ss.owner_cid == id);
            ss.owner_cid = id;
            if (cd->audiostream >= MAX_CALLS) 
            {
                ASSERT(id < 0);
                ss.groupnum = cd->get_gnum();
            }
            
            ss.fifo.add_data( ci->audio_data, ci->audio_data_size );
            w().active_calls |= (1 << cd->audiostream);

            if ((int)ss.fifo.available() > ((audio_format_s)ss).avgBytesPerSec())
                ss.fifo.read_data(nullptr, ci->audio_data_size);
        }
    }
}

void __stdcall configurable(const char *field, const char *val)
{
    if ( 0 == strcmp(CFGF_PROXY_TYPE, field) )
    {
        int new_proxy_type = pstr_c( asptr(val) ).as_int();
        if (new_proxy_type != tox_proxy_type)
            tox_proxy_type = new_proxy_type, reconnect = true;
        return;
    }
    if (0 == strcmp(CFGF_PROXY_ADDR, field))
    {
        asptr pa(val);
        str_c paddr(tox_proxy_host); paddr.append_char(':').append_as_uint(tox_proxy_port);
        if (!paddr.equals(pa))
        {
            set_proxy_addr(pa);
            reconnect = true;
        }
        return;
    }
    if (0 == strcmp(CFGF_SERVER_PORT, field))
    {
        int new_server_port = pstr_c(asptr(val)).as_int();
        if (new_server_port != options.tcp_port)
            options.tcp_port = (uint16_t)new_server_port, reconnect = true;
        return;
    }

    if (0 == strcmp(CFGF_UDP_ENABLE, field))
    {
        bool udp = pstr_c(asptr(val)).as_int() != 0;
        if (udp != options.udp_enabled)
            options.udp_enabled = udp, reconnect = true;
        return;
    }
}

void __stdcall file_send(int cid, const file_send_info_s *finfo)
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
            str_c fnc;
            asptr fn(finfo->filename, finfo->filename_len);
            while(fn.l >= TOX_MAX_FILENAME_LENGTH)
            {
                wstr_c w = to_wstr(fn);
                fnc = to_str( w.as_sptr().skip(1) );
                fn = fnc.as_sptr();
            }

            byte fileid[TOX_FILE_ID_LENGTH] = {0,0,0,0,0,0,0,0,'i','s','o','t','o','x','i','n'}; // not so random, yeah. toxcore requirement to provide same file id across client restarts
            *(u64 *)fileid = finfo->utag; // utag will be same every send

            TOX_ERR_FILE_SEND er = TOX_ERR_FILE_SEND_NULL;
            uint32_t tr = tox_file_send(tox, cd->get_fid(), TOX_FILE_KIND_DATA, finfo->filesize, fileid, (const byte *)fn.s, (uint16_t)fn.l, &er);
            if (er != TOX_ERR_FILE_SEND_OK)
            {
                hf->file_control( finfo->utag, FIC_DISCONNECT ); // put it to send queue: assume transfer broken
                return;
            }
            new transmitting_file_s(cd->get_fid(), tr, finfo->utag, fileid, finfo->filesize, fn); // not memleak
        }
    }

}

void __stdcall file_resume(u64 utag, u64 offset)
{
    for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
        if (f->utag == utag)
        {
            if (offset)
                tox_file_seek(tox, f->fid, f->fnn, offset, nullptr);
            tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
            break;
        }
}

void __stdcall file_control(u64 utag, file_control_e fctl)
{
    if (tox)
    {
        for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
            if (f->utag == utag)
            {
                switch (fctl)
                {
                case FIC_UNPAUSE:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
                    break;
                case FIC_PAUSE:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_PAUSE, nullptr);
                    break;
                case FIC_BREAK:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
                    delete f;
                    break;
                }
                return;
            }

        for (incoming_file_s *f = incoming_file_s::first; f; f = f->next)
            if (f->utag == utag)
            {
                switch(fctl)
                {
                case FIC_ACCEPT:
                case FIC_UNPAUSE:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_RESUME, nullptr);
                    break;
                case FIC_PAUSE:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_PAUSE, nullptr);
                    break;
                case FIC_REJECT:
                case FIC_BREAK:
                    tox_file_control(tox, f->fid, f->fnn, TOX_FILE_CONTROL_CANCEL, nullptr);
                    delete f;
                    break;
                case FIC_DONE:
                    tox_file_send_chunk(tox, f->fid, f->fnn, 0, nullptr, 0, nullptr);
                    delete f;
                    break;
                }
                return;
            }
    }
}

void __stdcall file_portion(u64 utag, const file_portion_s *portion)
{
    for (transmitting_data_s *f = transmitting_data_s::first; f; f = f->next)
        if (f->utag == utag)
        {
            f->portion( portion->offset, portion->data, portion->size );
            return;
        }
}

void __stdcall add_groupchat(const char *groupaname, bool persistent)
{
    if (persistent) return; // tox not yet supported persistent groups

    if (tox)
    {
        int gnum = toxav_add_av_groupchat(tox, callback_av_group_audio, nullptr);
        if (gnum >= 0)
        {
            asptr gn = utf8clamp(groupaname, TOX_MAX_NAME_LENGTH);
            tox_group_set_title(tox, gnum, (const uint8_t *)gn.s, (uint8_t)gn.l);

            contact_descriptor_s *desc = new contact_descriptor_s(ID_GROUP, gnum + GROUP_ID_OFFSET);
            desc->state = CS_ONLINE; // groups always online

            contact_data_s cdata( desc->get_id(), CDM_STATE | CDM_NAME | CDF_AUDIO_GCHAT | (persistent ? CDF_PERSISTENT_GCHAT : 0) );
            cdata.state = CS_ONLINE;
            cdata.name = gn.s;
            cdata.name_len = gn.l;
            hf->update_contact(&cdata);

            auto w = state.lock_write();
            for (int i = MAX_CALLS; i < MAX_CALLS * 2; ++i)
            {
                stream_settings_s &ss = w().local_peer_settings[i];
                if (ss.owner_cid == 0)
                {
                    ss.owner_cid = desc->get_id();
                    ss.groupnum = gnum;
                    desc->audiostream = i;
                    break;
                }
            }

        }
    }
}

void __stdcall ren_groupchat(int gid, const char *groupaname)
{
    if (tox && gid < 0)
    {
        auto it = id2desc.find(gid);
        if (it == id2desc.end()) return;
        contact_descriptor_s *desc = it->second;
        asptr gn = utf8clamp(groupaname, TOX_MAX_NAME_LENGTH);
        tox_group_set_title(tox, desc->get_gnum(), (const uint8_t *)gn.s, (uint8_t)gn.l);

        contact_data_s cdata(desc->get_id(), CDM_NAME);
        cdata.name = gn.s;
        cdata.name_len = gn.l;
        hf->update_contact(&cdata);
    }
}

void __stdcall join_groupchat(int gid, int cid)
{
    if (tox && gid < 0 && cid > 0)
    {
        auto git = id2desc.find(gid);
        if (git == id2desc.end()) return;

        auto cit = id2desc.find(cid);
        if (cit == id2desc.end()) return;

        contact_descriptor_s *gcd = git->second;
        contact_descriptor_s *ccd = cit->second;

        if (!ccd->is_online()) return; // persistent groups not yet supported

        tox_invite_friend(tox, ccd->get_fid(), gcd->get_gnum());
    }
}

void __stdcall typing(int cid)
{
    if (cid < 0)
    {
        // oops. toxcore does not support group typing notification... :(
        return;
    }

    if (contact_descriptor_s *cd = find_descriptor(cid))
        if (cd->is_online())
        {
            if (!self_typing_contact)
            {
                self_typing_contact = cid;
                if (tox) tox_self_set_typing(tox, cd->get_fid(), true, nullptr);
            }
            self_typing_time = time_ms() + 1500;
        }
}

proto_functions_s funcs =
{
#define FUNC0( rt, fn ) &fn,
#define FUNC1( rt, fn, p0 ) &fn,
#define FUNC2( rt, fn, p0, p1 ) &fn,
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0
};

proto_functions_s* __stdcall handshake(host_functions_s *hf_)
{
    srand(time_ms());

    hf = hf_;

    self_state = CS_OFFLINE;
    memset(lastmypubid, 0, TOX_ADDRESS_SIZE);

    memset( &options, 0, sizeof(options) );
    options.udp_enabled = true;

    nodes.clear();
    nodes.reserve(32);

#include "dht_nodes.inl"

    pinnedservs.clear();
    pinnedservs.reserve(4);
#define PINSERV(a, b) pinnedservs.emplace_back( CONSTASTR(a), CONSTASTR(b) );
    #include "pinned_srvs.inl"
#undef PINSERV

    //prepare(); // do not prepare now, prepare after config received due proxy settings

    return &funcs;
}

