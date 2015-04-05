#include "stdafx.h"
#pragma warning(push)
#pragma warning(disable : 4324) // 'crypto_generichash_blake2b_state' : structure was padded due to __declspec(align())
#include "sodium.h"
#pragma warning(pop)
//#include "toxcore/toxcore/tox_old.h"


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

#define SS2(x) #x
#define SS(x) SS2(x)
#define ISFLAG(f,mask) (((f)&(mask))!=0)
#define SETFLAG(f,mask) (f)|=(mask)
#define UNSETFLAG(f,mask) (f)&=~(mask)
#define SETUPFLAG(f,mask,val) if(val) {SETFLAG(f,mask);} else {UNSETFLAG(f,mask);}

#define AVATAR_MAX_DATA_SIZE (1024 * 512) // 0.5 mb should be enough

void __stdcall get_info( proto_info_s *info )
{
    if (info->protocol_name) strncpy_s(info->protocol_name, info->protocol_name_buflen, "tox", _TRUNCATE);
    if (info->description)
    {
        sstr_t<1024> desc( "Isotoxin tox wrapper " SS(PLUGINVER) ); 
        desc.append(CONSTASTR(" (toxcore: "));
        desc.append_as_uint(TOX_VERSION_MAJOR);
        desc.append_char('.');
        desc.append_as_uint(TOX_VERSION_MINOR);
        desc.append_char('.');
        desc.append_as_uint(TOX_VERSION_PATCH);
        desc.append_char(')');

        strncpy_s(info->description, info->description_buflen, desc, _TRUNCATE);
    }



    info->priority = 500;
    info->max_avatar_size = AVATAR_MAX_DATA_SIZE;
    info->features = PF_AUDIO_CALLS | PF_SEND_FILE; //PF_INVITE_NAME | PF_UNAUTHORIZED_CHAT;
    info->proxy_support = PROXY_SUPPORT_HTTP | PROXY_SUPPORT_SOCKS5;
    info->max_friend_request_bytes = TOX_MAX_FRIEND_REQUEST_LENGTH;
    info->audio_fmt.sample_rate = av_DefaultSettings.audio_sample_rate;
    info->audio_fmt.channels = (short)av_DefaultSettings.audio_channels;
    info->audio_fmt.bits = 16;
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

void set_proxy_addr(const asptr& addr)
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

struct state_s
{
    unsigned long active_calls = 0;
    bool audio_sender = false;
    bool audio_sender_need_stop = false;
    stream_settings_s local_peer_settings[MAX_CALLS];
    static_assert(sizeof(unsigned long /*active_calls*/)*8 >= MAX_CALLS, "!");
};

static spinlock::syncvar<state_s> state;
stream_settings_s remote_audio_fmt[MAX_CALLS];


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

    chunk_msgs_sending = 10,
    chunk_msg_sending,
    chunk_msg_sending_fid,
    chunk_msg_sending_type,
    chunk_msg_sending_utag,
    chunk_msg_sending_body,
    chunk_msg_sending_mid,
    chunk_msg_sending_createtime,

    chunk_msgs_receiving = 20,
    chunk_msg_receiving,
    chunk_msg_receiving_cid,
    chunk_msg_receiving_type,
    chunk_msg_receiving_body,
    chunk_msg_receiving_createtime,

    chunk_other = 30,
    chunk_proxy_type,
    chunk_proxy_address,
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
    int resend_time = 0;
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
            } else if ( (ct - x->resend_time) > 0 )
            {
                // seems message not delivered: no delivery notification received in 10 seconds
                x->mid = 0;
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
                    hf->message(x->mt, x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
                    delete x;
                    hf->save();
                    return;
                }
            }
            asptr m = extract_create_time(create_time, asptr(msgbody, len), cid);
            hf->message(mt, cid, create_time, m.s, m.l);
        }
    }

    static void tick(int ct)
    {
        for (message_part_s *x = first; x; x = x->next)
        {
            if ( (ct - x->send2host_time) > 0 )
            {
                hf->message(x->mt, x->cid, x->create_time, x->msgb.cstr(), x->msgb.get_length());
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
    /*virtual*/ void pause()
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
        u32 size;
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

    void transmit(const req &r)
    {
        if (r.size == 0)
        {
            finished();
            return;
        }

        //Log("req chunk: %llu:%i", r.offset, r.size);
        requests.add_data( &r, sizeof(req) );
        try_send_requested_chunk();
    }

    /*virtual*/ void accepted() override
    {
        is_accepted = true;
        is_paused = false;
    }
    /*virtual*/ void kill() override
    {
        delete this;
    }
    /*virtual*/ void finished() override
    {
        delete this;
    }
    /*virtual*/ void pause()
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
    fifo_stream_c fifo;
    u64 fifooffset = 0;
    bool is_requested_portion = false;

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

    virtual void portion(u64 offset, const void *data, int size) override
    {
        ASSERT(is_requested_portion);
        is_requested_portion = false;

        if (offset == (fifooffset + fifo.available()))
        {
            fifo.add_data(data, size);
        }
        else
        {
            fifo.clear();
            fifo.add_data(data, size);
            fifooffset = offset;
        }

        //Log("portion: %llu:%i", offset, size);
        //Log("fifo: %llu:%i", fifooffset, fifo.available());

        try_send_requested_chunk();
    }

    void request_portion()
    {
        if (is_requested_portion) return;

        req r;
        bool oof = false;
        if (requests.available())
        {
            requests.get_data(0, (byte *)&r, sizeof(req));

            if (r.offset < fifooffset || (r.offset - fifooffset) > fifo.available())
            {
                oof = true; // out of available fifo data
                fifo.clear();
            }
        }
        else if (fifo.available() == 0)
        {
            return; // no requests, no fifo - do nothing (wait chunk request from tox core)
        }

        if (fifo.available() < 32768)
        {
            is_requested_portion = true;

            if (oof || fifo.available() == 0)
            {
                int rqsz = (int)min(65536, fsz - r.offset);
                if (rqsz) hf->file_portion(utag, r.offset, nullptr, rqsz);
                //Log( "request from app 1: %llu:%i", r.offset, rqsz );
            }
            else
            {
                u64 roffs = fifooffset + fifo.available();
                int rqsz = (int)min(65536, fsz - roffs);
                hf->file_portion(utag, roffs, nullptr, rqsz);
                //Log( "request from app 2: %llu:%i", roffs, rqsz );
            }
        }
        //Log("fifo: %llu:%i", fifooffset, fifo.available());
    }

    void try_send_requested_chunk() override
    {
        u64 fifolast = fifooffset + fifo.available();

        if (requests.available())
        {
            req r;
            requests.get_data(0, (byte *)&r, sizeof(req));

            if (r.offset >= fifooffset)
            {
                if ((r.offset + r.size) <= fifolast)
                {
                    // fifo has data
                    // just send

                    byte *d = (byte *)_alloca(r.size);
                    fifo.get_data((int)(r.offset - fifooffset), d, r.size);

                    TOX_ERR_FILE_SEND_CHUNK er;
                    tox_file_send_chunk(tox, fid, fnn, r.offset, d, r.size, &er);
                    if (TOX_ERR_FILE_SEND_CHUNK_OK == er)
                    {
                        if (r.offset == fifooffset)
                        {
                            fifo.read_data(nullptr, r.size);
                            fifooffset += r.size;
                        }
                        requests.read_data(nullptr, sizeof(req));

                        //Log("send ok: %llu:%i", r.offset, r.size);
                        //Log("fifo: %llu:%i", fifooffset, fifo.available());
                    }
                }
            }
        }
        request_portion();
    }

    virtual void ontick() override
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
        auto w = sync.lock_write();
        if (w().shutdown_discover) return;
        w().waiting_thread_start = false;
        w().thread_in_progress = true;

        str_c ids = w().ids;
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
                sync.lock_write()().pubid = s;
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
            
            sync.lock_write()().pubid = s;
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
    int callindex = -1;

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

    int flags = 0;

    contact_descriptor_s(bool init_new_id, int fid);
    ~contact_descriptor_s()
    {
        LIST_DEL(this, first_desc, last_desc, prev, next);
    }
    static int find_free_id();
    static contact_descriptor_s *find( const asptr &pubid )
    {
        asptr ff = pubid;
        ff.l = TOX_PUBLIC_KEY_SIZE * 2;
        pstr_c fff; fff.set(ff);
        for(contact_descriptor_s *f = first_desc; f; f = f->next)
            if (f->pubid.substr(0, TOX_PUBLIC_KEY_SIZE * 2) == fff)
                return f;

        return nullptr;
    }

    void die();

    void on_offline()
    {
        UNSETFLAG(flags, F_IS_ONLINE);
        UNSETFLAG(flags, F_NEED_RESYNC);
        UNSETFLAG(flags, F_ISOTOXIN);
        UNSETFLAG(flags, F_AVASEND);
        avatag_self = gavatag - 1;
        correct_create_time = 0;
        avatar_recv_fnn = -1;
    }

    int get_fid() const {return fid;};
    int get_id() const {return id;};
    void set_fid(int fid_);
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
        if (!ISFLAG(flags, F_IS_ONLINE)) return;
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
        if (contact_descriptor_s *desc = find_restore_descriptor(fid))
        {
            if (!ISFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE))
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

        TOX_ERR_FRIEND_SEND_MESSAGE err;

        mid = 0;
        if (mt == MT_MESSAGE)
            mid = tox_friend_send_message(tox,fid,TOX_MESSAGE_TYPE_NORMAL, (const byte *)m.s, m.l, &err);
        if (mt == MT_ACTION)
            mid = tox_friend_send_message(tox,fid,TOX_MESSAGE_TYPE_ACTION, (const byte *)m.s, m.l, &err);
        if (mid) resend_time = time + 10000;
        next_try_time = time + 1000;
    }
}


contact_descriptor_s::contact_descriptor_s(bool init_new_id, int fid) :pubid(TOX_ADDRESS_SIZE * 2, true), fid(fid)
{
    memset(avatar_hash, 0, sizeof(avatar_hash));
    LIST_ADD(this, first_desc, last_desc, prev, next);

    if (init_new_id)
    {
        id = contact_descriptor_s::find_free_id();
        id2desc[id] = this;
    }

    if (fid >= 0)
        fid2desc[fid] = this;


}

void contact_descriptor_s::die()
{
    if (id > 0)
        id2desc.erase(id);
    if (fid >= 0)
        fid2desc.erase(fid);
    delete this;
}

int contact_descriptor_s::find_free_id()
{
    int id = 1;
    for(; id2desc.find(id) != id2desc.end() ;++id) ;
    return id;
}

void contact_descriptor_s::set_fid(int fid_)
{
    if (fid != fid_)
    {
        fid2desc.erase(fid);
        fid = fid_;
        if (fid >= 0) fid2desc[fid] = this;
    }
}
void contact_descriptor_s::set_id(int id_)
{
    if (id != id_)
    {
        id2desc.erase(id);
        id = id_;
        if (id > 0) id2desc[id] = this;
    }
}


static bool online_flag = false;
contact_state_e self_state;
byte lastmypubid[TOX_ADDRESS_SIZE];

void update_self()
{
    if (tox)
    {
        tox_self_get_address(tox,lastmypubid);
        str_c pubid(TOX_ADDRESS_SIZE * 2, true); pubid.append_as_hex(lastmypubid,TOX_ADDRESS_SIZE);

        str_c name( tox_self_get_name_size(tox), false );
        tox_self_get_name(tox,(byte*)name.str());

        str_c statusmsg(tox_self_get_status_message_size(tox), false);
        tox_self_get_status_message(tox, (byte*)statusmsg.str());

        contact_data_s self;
        self.id = 0;
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
}

static void update_contact( const contact_descriptor_s *cdesc )
{
    if (ASSERT(tox))
    {
        contact_data_s cd;
        cd.mask = CDM_PUBID | CDM_STATE | CDM_ONLINE_STATE | CDM_AVATAR_TAG;
        cd.id = cdesc->get_id();

        TOX_ERR_FRIEND_QUERY err = TOX_ERR_FRIEND_QUERY_NULL;
        TOX_USER_STATUS st = cdesc->get_fid() >= 0 ? tox_friend_get_status(tox, cdesc->get_fid(), &err) : (TOX_USER_STATUS)(-1);
        if (err != TOX_ERR_FRIEND_QUERY_OK) st = (TOX_USER_STATUS)(-1);

        sstr_t<TOX_PUBLIC_KEY_SIZE * 2 + 16> pubid;
        if (cdesc->get_fid() >= 0)
        {
            byte id[TOX_PUBLIC_KEY_SIZE];
            TOX_ERR_FRIEND_GET_PUBLIC_KEY e;
            tox_friend_get_public_key(tox, cdesc->get_fid(), id, &e);
            if (TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK != e) memset(id,0,sizeof(id));
            pubid.append_as_hex(id, TOX_PUBLIC_KEY_SIZE);
            ASSERT(pubid.beginof(cdesc->pubid));
        } else
        {
            pubid.append( asptr( cdesc->pubid.cstr(), TOX_PUBLIC_KEY_SIZE * 2 ) );
        }

        cd.public_id = pubid.cstr();
        cd.public_id_len = pubid.get_length();

        cd.avatar_tag = cdesc->avatar_tag;

        str_c name, statusmsg;

        if (st >= (TOX_USER_STATUS)0)
        {
            TOX_ERR_FRIEND_QUERY er;
            name.set_length(tox_friend_get_name_size(tox, cdesc->get_fid(), &er));
            tox_friend_get_name(tox, cdesc->get_fid(), (byte*)name.str(), &er);
            cd.name = name.cstr();
            cd.name_len = name.get_length();

            statusmsg.set_length(tox_friend_get_status_message_size(tox, cdesc->get_fid(), &er));
            tox_friend_get_status_message(tox, cdesc->get_fid(), (byte*)statusmsg.str(), &er);
            cd.status_message = statusmsg.cstr();
            cd.status_message_len = statusmsg.get_length();

            cd.mask |= CDM_NAME | CDM_STATUSMSG;
        }

        if (st < (TOX_USER_STATUS)0 || cdesc->state != CS_OFFLINE)
        {
            cd.state = cdesc->state;
            if ( cd.state == CS_INVITE_SEND )
            {
                cd.public_id = cdesc->pubid.cstr();
                cd.public_id_len = cdesc->pubid.get_length();
            }
            st = TOX_USER_STATUS_NONE;

        } else
        {
            cd.state = tox_friend_get_connection_status(tox, cdesc->get_fid(), nullptr) != TOX_CONNECTION_NONE ? CS_ONLINE : CS_OFFLINE;
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

    if (!tox || !tox_friend_exists(tox,fid)) return nullptr;

    contact_descriptor_s *desc = new contact_descriptor_s(true, fid);

    byte id[TOX_PUBLIC_KEY_SIZE];
    TOX_ERR_FRIEND_GET_PUBLIC_KEY er;
    tox_friend_get_public_key(tox, fid, id, &er);

    desc->pubid.append_as_hex(id, TOX_PUBLIC_KEY_SIZE);
    desc->state = CS_OFFLINE;

    ASSERT( contact_descriptor_s::find(desc->pubid) == desc ); // check that such pubid is single

    contact_data_s cd;
    cd.mask = CDM_PUBID | CDM_STATE;
    cd.id = desc->get_id();
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int find_tox_fid(const byte *id)
{
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
}

static void cb_friend_request(Tox *, const byte *id, const byte *msg, size_t length, void *)
{
    str_c pubid;
    pubid.append_as_hex(id,TOX_PUBLIC_KEY_SIZE);
    contact_descriptor_s *desc = contact_descriptor_s::find(pubid);
    if (!desc) desc = new contact_descriptor_s( true, -1 );
    desc->pubid = pubid;
    desc->state = CS_INVITE_RECEIVE;

    desc->set_fid( find_tox_fid(id) );

    contact_data_s cd;
    cd.mask = CDM_PUBID | CDM_STATE;
    cd.id = desc->get_id();
    cd.public_id = desc->pubid.cstr();
    cd.public_id_len = TOX_PUBLIC_KEY_SIZE * 2;
    cd.state = desc->state;
    hf->update_contact(&cd);

    time_t create_time = now();

    hf->message(MT_FRIEND_REQUEST, desc->get_id(), create_time, (const char *)msg, length);
}

static void cb_friend_message(Tox *, uint32_t fid, TOX_MESSAGE_TYPE type, const byte *message, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
        message_part_s::msg(TOX_MESSAGE_TYPE_NORMAL == type ? MT_MESSAGE : MT_ACTION, desc->get_id(), 0, (const char *)message, length);

}

static void cb_name_change(Tox *, uint32_t fid, const byte * newname, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        contact_data_s cd;
        cd.mask = CDM_NAME;
        cd.id = desc->get_id();
        cd.name = (const char *)newname;
        cd.name_len = length;
        hf->update_contact(&cd);
    }
}

static void cb_status_message(Tox *, uint32_t fid, const byte * newstatus, size_t length, void *)
{
    if (contact_descriptor_s *desc = find_restore_descriptor(fid))
    {
        contact_data_s cd;
        cd.mask = CDM_STATUSMSG;
        cd.id = desc->get_id();
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
        contact_data_s cd;
        cd.mask = CDM_ONLINE_STATE;
        cd.id = desc->get_id();
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

static void cb_typing(Tox *, uint32_t /*friend_number*/, bool /*is_typing*/, void *)
{
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
        bool prev_online = ISFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE);
        bool accepted = desc->state == CS_INVITE_SEND;
        desc->state = CS_OFFLINE;
        contact_data_s cd;
        cd.mask = CDM_STATE;
        cd.id = desc->get_id();
        cd.state = (TOX_CONNECTION_NONE != connection_status) ? CS_ONLINE : CS_OFFLINE;
        SETUPFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE, CS_ONLINE == cd.state );

        if (accepted)
        {
            cd.mask |= CDM_PUBID;
            cd.public_id = desc->pubid.cstr();
            cd.public_id_len = TOX_PUBLIC_KEY_SIZE * 2;
        }

        hf->update_contact(&cd);

        if (!prev_online && ISFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE))
        {
            desc->wait_client_id = time_ms() + 2000; // wait 2 sec client name
            if (desc->wait_client_id == 0) desc->wait_client_id++;
            desc->send_identity(false);
            desc->send_avatar();

        } else if (!ISFLAG(desc->flags, contact_descriptor_s::F_IS_ONLINE))
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
                contact_data_s cd;
                cd.id = desc->get_id();
                cd.mask = CDM_AVATAR_TAG;
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

                    contact_data_s cd;
                    cd.id = desc->get_id();
                    cd.mask = CDM_AVATAR_TAG;
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

static void cb_group_invite(Tox *, int /*fid*/, byte /*type*/, const byte * /*data*/, uint16_t /*length*/, void *)
{
}

static void cb_group_message(Tox *, int /*gid*/, int /*pid*/, const byte * /*message*/, uint16_t /*length*/, void *)
{
}

static void cb_group_action(Tox *, int /*gid*/, int /*pid*/, const byte * /*action*/, uint16_t /*length*/, void *)
{
}

static void cb_group_namelist_change(Tox *, int /*gid*/, int /*pid*/, byte /*change*/, void *)
{
}

static void cb_group_title(Tox *, int /*gid*/, int /*pid*/, const byte * /*title*/, byte /*length*/, void *)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Tox AV callbacks /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cb_toxav_invite(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        ASSERT(desc->callindex < 0);
        if (desc->callindex >= 0 && desc->callindex != call_index)
            toxav_stop_call(toxav,desc->callindex);

        desc->callindex = call_index;
        ToxAvCSettings peer_settings;
        toxav_get_peer_csettings(toxav, call_index, 0, &peer_settings);
        bool video = (peer_settings.call_type == av_TypeVideo);
        if (video)
            hf->message(MT_INCOMING_CALL, desc->get_id(), now(), "video", 5);
        else
            hf->message(MT_INCOMING_CALL, desc->get_id(), now(), "audio", 5);
    }
}


static void cb_toxav_start(void *av, int32_t call_index, void * /*userdata*/)
{
    if (av != toxav) return;

    ToxAvCSettings remote_settings;

    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        ASSERT( call_index == desc->callindex );
        toxav_get_peer_csettings(toxav, call_index, 0, &remote_settings);

        //bool video = (peer_settings.call_type == av_TypeVideo);

        toxav_prepare_transmission(toxav, call_index, 0 /*video = 1*/);
        if (ISFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START))
        {
            UNSETFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
            hf->message(MT_CALL_ACCEPTED, desc->get_id(), now(), nullptr, 0);
        }
    }
}

static void call_stop_int( int32_t call_index )
{
    if (contact_descriptor_s *desc = find_restore_descriptor(toxav_get_peer_id(toxav, call_index, 0)))
    {
        if (desc->callindex >= 0)
        {
            ASSERT(desc->callindex == call_index);
            hf->message(MT_CALL_STOP, desc->get_id(), now(), nullptr, 0);
            desc->callindex = -1;
            UNSETFLAG(desc->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
        }
        hf->close_audio(desc->get_id());
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
                hf->play_audio(desc->get_id(), &fmt, data, (int)samples * fmt.channels * fmt.bits / 8);
            }
        }
}

static DWORD WINAPI audio_sender(LPVOID)
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
        for(int i=0;i<MAX_CALLS;++i)
        {
            if (!w.is_locked()) w = state.lock_write();

            unsigned long mask = (1<<i);
            if ( 0 != ( mask & w().active_calls) )
            {
                stream_settings_s &ss = w().local_peer_settings[i];
                int avsize = ss.fifo.available();
                if (avsize == 0)
                {
                    w().active_calls &= ~mask;
                    continue;
                }
                audio_format_s fmt = ss;
                int req_frame_size = fmt.avgBytesPerMSecs(ss.audio_frame_duration);
                if (req_frame_size > avsize) 
                    continue; // not yet
                if (req_frame_size >(int)prebuffer.size()) prebuffer.resize(req_frame_size);
                int samples = ss.fifo.read_data(prebuffer.data(), req_frame_size) / fmt.blockAlign();
                w.unlock(); // no need lock anymore

                if ( fmt.bits == 8 )
                {
                    // oops. Tox works only with 16 bits
                    // converting to 16 bit...
                    int cvt_size = fmt.blockAlign() * 2 * samples;
                    compressed.clear();
                    if (cvt_size >(int)compressed.size()) compressed.resize(cvt_size); // just use compressed buffer for convertion
                    int16_t * cvt = (int16_t *)compressed.data();
                    for(int i=0;i<req_frame_size;++i)
                    {
                        byte sample8 = (byte)prebuffer.data()[i];
                        *cvt = ((int16_t)sample8-(int16_t)0x80) << 8;
                        ++cvt;
                    }
                    std::swap(compressed, prebuffer);
                    req_frame_size = cvt_size;
                }

                if (req_frame_size >(int)compressed.size()) compressed.resize(req_frame_size);

                int r = 0;
                if ((r = toxav_prepare_audio_frame(toxav, i, compressed.data(), req_frame_size, (int16_t *)prebuffer.data(), samples)) > 0)
                {
                    toxav_send_audio(toxav, i, compressed.data(), r);
                }
            }
        }

    }

    state.lock_write()().audio_sender = false;
    return 0;
}


static void prepare(const byte *data, size_t length)
{
    if (tox) tox_kill(tox);

    OSVERSIONINFO v = {sizeof(OSVERSIONINFO),0};
    GetVersionExW(&v);
    options.ipv6_enabled = (v.dwMajorVersion >= 6) ? 1 : 0;
    options.proxy_host = tox_proxy_host.cstr();
    options.proxy_port = 0;
    options.proxy_type = TOX_PROXY_TYPE_NONE;
    if (tox_proxy_type & PROXY_SUPPORT_HTTP)
        options.proxy_type = TOX_PROXY_TYPE_HTTP;
    if (tox_proxy_type & (PROXY_SUPPORT_SOCKS4|PROXY_SUPPORT_SOCKS5))
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
    options.udp_enabled = options.proxy_type == TOX_PROXY_TYPE_NONE;

    TOX_ERR_NEW er;
    tox = tox_new(&options, data, length, &er);

    tox_callback_friend_lossless_packet(tox, cb_isotoxin, nullptr);

    tox_callback_friend_request(tox, cb_friend_request, nullptr);
    tox_callback_friend_message(tox, cb_friend_message, nullptr);
    tox_callback_friend_name(tox, cb_name_change, nullptr);
    tox_callback_friend_status_message(tox, cb_status_message, nullptr);
    tox_callback_friend_status(tox, cb_friend_status, nullptr);
    tox_callback_friend_typing(tox, cb_typing, nullptr);
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


    CloseHandle(CreateThread(nullptr, 0, audio_sender, nullptr, 0, nullptr));
    for (; !state.lock_read()().audio_sender; Sleep(1)); // wait audio sender start
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

void __stdcall tick()
{
    if (!online_flag) return;
    int curt = time_ms();
    static int nextt = curt;
    static int nexttav = curt;
    static int nexttresync = curt;
    time_t tryconnect = now() + 60;
    if (tox)
    {
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
                tryconnect = now() + 60;

            self_state = nst;
            update_self();

        }
        if (nst == CS_OFFLINE)
        {
            time_t ctime = now();
            if (ctime > tryconnect)
            {
                connect();
                tryconnect = ctime + 60;
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
                if (ISFLAG(f->flags, contact_descriptor_s::F_NEED_RESYNC) && ISFLAG(f->flags, contact_descriptor_s::F_IS_ONLINE) && (curt - f->next_sync) > 0)
                {
                    f->next_sync = curt + 120000;
                    f->send_identity(true);
                    break;
                }
            nexttresync = curt + 3001;
        }
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
    
    if (tox)
    {

        state.lock_write()().audio_sender_need_stop = true;
        for (; state.lock_read()().audio_sender; Sleep(1)); // wait audio sender stop


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

    if (isz > AVATAR_MAX_DATA_SIZE) isz = 0;
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

void __stdcall set_config(const void*data, int isz)
{
    u64 _now = now();
    if (isz>8 && (*(uint32_t *)data) == 0 && (*((uint32_t *)data+1)) == 0x15ed1b1f)
    {
        // raw tox_save
        prepare( (const byte *)data, isz );

    } else if (isz>4 && (*(uint32_t *)data) != 0)
    {
        loader ldr(data,isz);

        if (ldr(chunk_proxy_type))
            tox_proxy_type = ldr.get_int();
        if (ldr(chunk_proxy_address))
            set_proxy_addr(ldr.get_astr());

        if (int sz = ldr(chunk_tox_data))
        {
            loader l(ldr.chunkdata(), sz);
            int dsz;
            if (const void *toxdata = l.get_data(dsz))
                prepare((const byte *)toxdata, dsz);
        } else
            if (!tox) prepare(nullptr,0); // prepare anyway

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
                    contact_descriptor_s *cd = new contact_descriptor_s( false, -1 );
                    loader lc(l.chunkdata(), descriptor_size);

                    int id = 0, fid = -1;

                    if (lc(chunk_descriptor_id))
                        id = lc.get_int();
                    //if (lc(chunk_descriptor_fid))
                    //    fid = lc.get_int();
                    if (lc(chunk_descriptor_pubid))
                        cd->pubid = lc.get_astr();
                    if (lc(chunk_descriptor_state))
                        cd->state = (contact_state_e)lc.get_int();
                    if (lc(chunk_descriptor_avatartag))
                        cd->avatar_tag = lc.get_int();

                    if (cd->avatar_tag != 0)
                    {
                        if (int bsz = lc(chunk_descriptor_avatarhash))
                        {
                            loader hbl(lc.chunkdata(), bsz);
                            int dsz;
                            if (const void *mb = hbl.get_data(dsz))
                                memcpy(cd->avatar_hash, mb, min(dsz, TOX_HASH_LENGTH));
                        }
                    }

                    if (id == 0) continue;

                    byte buf[TOX_PUBLIC_KEY_SIZE];
                    cd->pubid.hex2buf<TOX_PUBLIC_KEY_SIZE>(buf);
                    fid = find_tox_fid(buf);

                    if (fid >= 0)
                    {
                        auto it = fid2desc.find(fid);
                        if (it != fid2desc.end())
                        {
                            // oops oops
                            // contact dup!!!
                            delete cd;
                            continue;
                        }
                    }

                    if (id > 0) cd->set_id(id);
                    if (fid >= 0) cd->set_fid(fid);
                }
        }
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
        if (!tox) prepare(nullptr, 0);

    hf->proxy_settings(tox_proxy_type, str_c(tox_proxy_host).append_char(':').append_as_uint(tox_proxy_port));
}
void __stdcall init_done()
{
    if (tox)
    {
        update_self();

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
    //chunk(chunkm.b, chunk_descriptor_fid) << desc.get_fid();
    chunk(chunkm.b, chunk_descriptor_pubid) << desc.pubid;
    chunk(chunkm.b, chunk_descriptor_state) << (int)desc.state;
    chunk(chunkm.b, chunk_descriptor_avatartag) << desc.avatar_tag;
    if (desc.avatar_tag != 0)
    {
        chunk(chunkm.b, chunk_descriptor_avatarhash) << bytes(desc.avatar_hash, TOX_HASH_LENGTH);
    }
    
}

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

    size_t sz = tox_get_savedata_size(tox);
    void *data = chunk(b, chunk_tox_data).alloc(sz);
    tox_get_savedata(tox, (byte *)data);

    chunk(b, chunk_descriptors) << serlist<contact_descriptor_s>(contact_descriptor_s::first_desc);
    chunk(b, chunk_msgs_sending) << serlist<message2send_s>(message2send_s::first);
    chunk(b, chunk_msgs_receiving) << serlist<message_part_s>(message_part_s::first);
}

void __stdcall offline()
{
    online_flag = false;
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

        contact_data_s self;
        self.mask = CDM_STATE;
        self.id = 0;
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
        {
            contact_descriptor_s *cd = contact_descriptor_s::find(asptr(public_id, TOX_PUBLIC_KEY_SIZE));
            TOX_ERR_FRIEND_DELETE er;
            if (cd) tox_friend_delete(tox, cd->get_fid(), &er);
        }

        asptr invmsg(invite_message_utf8);
        if (invmsg.l > TOX_MAX_FRIEND_REQUEST_LENGTH) invmsg.l = TOX_MAX_FRIEND_REQUEST_LENGTH;
        if (invmsg.l == 0) invmsg.s = "?";

        TOX_ERR_FRIEND_ADD er = TOX_ERR_FRIEND_ADD_NULL;
        int fid = tox_friend_add(tox, toxaddr, (const byte *)invmsg.s,invmsg.l, &er);
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

        contact_descriptor_s *cd = contact_descriptor_s::find( asptr(public_id, TOX_PUBLIC_KEY_SIZE) );
        if (cd) cd->set_fid(fid);
        else cd = new contact_descriptor_s( true, fid );
        cd->pubid = s;
        cd->state = CS_INVITE_SEND;

        contact_data_s cdata;
        cdata.mask = CDM_PUBID | CDM_STATE;
        cdata.id = cd->get_id();
        cdata.public_id = cd->pubid.cstr();
        cdata.public_id_len = cd->pubid.get_length();
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
        contact_descriptor_s *cd = it->second;
        TOX_ERR_FRIEND_DELETE er;
        tox_friend_delete(tox, cd->get_fid(), &er);
        cd->die();
    }
}

void __stdcall send(int id, const message_s *msg)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;

        new message2send_s( msg->utag, msg->mt, cd->get_fid(), asptr(msg->message, msg->message_len) ); // not memleak!
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
        contact_descriptor_s *cd = it->second;
        
        if (cd->pubid.get_length() >= TOX_PUBLIC_KEY_SIZE * 2 && cd->get_fid() < 0)
        {
            byte buf[TOX_PUBLIC_KEY_SIZE];
            cd->pubid.hex2buf<TOX_PUBLIC_KEY_SIZE>(buf);

            TOX_ERR_FRIEND_ADD er;
            int fid = tox_friend_add_norequest(tox, buf, &er);
            if (TOX_ERR_FRIEND_ADD_OK != er)
                return;
            cd->set_fid(fid);
            cd->state = CS_OFFLINE;

            contact_data_s cdata;
            cdata.mask = CDM_STATE;
            cdata.id = cd->get_id();
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
        contact_descriptor_s *cd = it->second;
        cd->state = CS_ROTTEN;

        contact_data_s cdata;
        cdata.mask = CDM_STATE;
        cdata.id = cd->get_id();
        cdata.state = CS_ROTTEN;
        hf->update_contact(&cdata);

        TOX_ERR_FRIEND_DELETE er;
        if (cd->get_fid() >= 0) tox_friend_delete(tox,cd->get_fid(), &er);

        cd->die();
    }
}

void __stdcall call(int id, const call_info_s *ci)
{
    if (tox)
    {
        auto it = id2desc.find(id);
        if (it == id2desc.end()) return;
        contact_descriptor_s *cd = it->second;
        if (cd->callindex < 0)
        {
            stream_settings_s sets;
            if (0 == toxav_call(toxav, &cd->callindex, cd->get_fid(), &sets, ci->duration ))
            {
                SETFLAG(cd->flags, contact_descriptor_s::F_NOTIFY_APP_ON_START);
                auto w = state.lock_write();
                w().local_peer_settings[cd->callindex] = sets;
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
        if (cd->callindex >= 0)
            if (sc == STOPCALL_HANGUP)
                toxav_hangup(toxav, cd->callindex);
            else if (sc == STOPCALL_REJECT)
                toxav_reject(toxav, cd->callindex, nullptr);
            else if (sc == STOPCALL_CANCEL)
                toxav_cancel(toxav, cd->callindex, cd->get_fid(), nullptr);
        cd->callindex = -1;
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
        if (cd->callindex >= 0)
        {
            auto w = state.lock_write();
            w().local_peer_settings[cd->callindex].setup();
            toxav_answer(toxav, cd->callindex, w().local_peer_settings + cd->callindex);
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
        if (cd->callindex >= 0)
        {
            auto w = state.lock_write();
            stream_settings_s &ss = w().local_peer_settings[cd->callindex];
            //toxav_change_settings(toxav, cd->callindex, &ss);
            
            ss.fifo.add_data( ci->audio_data, ci->audio_data_size );
            w().active_calls |= (1 << cd->callindex);
        }
    }
}

void __stdcall proxy_settings(int proxy_type, const char *proxy_address)
{
    asptr pa(proxy_address);
    if (pa.l == 0) proxy_type = 0;
    str_c paddr(tox_proxy_host); paddr.append_char(':').append_as_uint(tox_proxy_port);
    if (tox_proxy_type != proxy_type || !paddr.equals(pa))
    {
        tox_proxy_type = proxy_type;
        set_proxy_addr(pa);
        offline();
        online();
        hf->save();
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

