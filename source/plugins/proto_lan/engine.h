#pragma once

#define LOGGING 1
#if defined _FINAL
#undef LOGGING
#define LOGGING 0
#endif


#define AUDIO_SAMPLERATE 48000
#define AUDIO_BITS 16
#define AUDIO_CHANNELS 1

#define OPUS_EXPORT
#include <opus.h>

#include <vpx/vpx_decoder.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vp8dx.h>
#include <vpx/vp8cx.h>
#include <vpx/vpx_image.h>

#include <memory>

struct socket_s
{
    SOCKET _socket = INVALID_SOCKET;

    void close();
    void flush_and_close();
    bool ready() const { return _socket != INVALID_SOCKET; }
    ~socket_s() {close();}
};

struct udp_listner : public socket_s
{
    int port;

    int open();
    int read( void *data, int datasize, sockaddr_in &addr );
    udp_listner(int port):port(port) {}
};

struct udp_sender
{
    int port;

    struct prepared_sock_s
    {
        SOCKET s;
        unsigned int ip;
        prepared_sock_s(SOCKET s, unsigned int ip):s(s), ip(ip) {}
    };

    std::vector<prepared_sock_s> socks; // one per interface

    void prepare();
    bool send(const void *data, int size, int portshift);
    void close();
    udp_sender(int port) :port(port) {}
    ~udp_sender()
    {
        close();
    }
};

struct tcp_pipe : public socket_s
{
    sockaddr_in addr;
    int creationtime = 0;
    byte rcvbuf[65536 * 2];
    int rcvbuf_sz = 0;

    tcp_pipe() { creationtime = time_ms(); }
    tcp_pipe( SOCKET s, const sockaddr_in& addr ):addr(addr) { _socket = s; creationtime = time_ms(); }
    tcp_pipe(const tcp_pipe&) = delete;
    tcp_pipe(tcp_pipe&&) = delete;

    void close()
    {
        rcvbuf_sz = 0;
        __super::close();
    }

    bool timeout() const
    {
        return (time_ms() - creationtime) > 10000; // 10 sec
    }

    void set_address(unsigned int IPv4, int port)
    {
        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = IPv4;
        addr.sin_port = htons((USHORT)port);
    }

    bool connect();

    tcp_pipe& operator=( tcp_pipe&& p )
    {
        if (connected()) closesocket(_socket);
        _socket = p._socket; p._socket = INVALID_SOCKET;
        addr = p.addr;
        if (p.rcvbuf_sz) memcpy(rcvbuf,p.rcvbuf,p.rcvbuf_sz);
        rcvbuf_sz = p.rcvbuf_sz;
        return *this;
    }

    bool connected() const {return ready();}

    bool send( const byte *data, int datasize );

    packet_id_e packet_id();
    void rcv_all(); // receive all, but stops when buffer size reaches 64k

    void cpdone() // current packet done
    {
        if ( rcvbuf_sz >= SIZE_PACKET_HEADER )
        {
            USHORT sz = ntohs(*(USHORT *)(rcvbuf+2));
            if (rcvbuf_sz == sz)
                rcvbuf_sz = 0;
            else if (CHECK(rcvbuf_sz > sz))
            {   
                rcvbuf_sz -= sz;
                memcpy(rcvbuf, rcvbuf + sz, rcvbuf_sz);
            }
        }
        
    }
};

struct tcp_listner : public socket_s
{
    struct state_s
    {
        int port = 0;
        bool acceptor_works = false;
        bool acceptor_stoped = false;
        bool acceptor_should_stop = false;

        bool wait_acceptor_start(int &_port) const
        {
            if (acceptor_works)
            {
                _port = port;
                return false;
            }
            if (acceptor_stoped)
            {
                _port = -1;
                return false;
            }
            return true;
        }
        bool wait_acceptor_stop() const
        {
            if (!acceptor_works) return false;
            if (acceptor_stoped) return false;
            return true;
        }
    };

    spinlock::spinlock_queue_s<tcp_pipe *> accepted;
    spinlock::syncvar<state_s> state;
    static DWORD WINAPI acceptor(LPVOID);
    int open();
    void close();
    tcp_listner(int port) { state.lock_write()().port = port; }
    ~tcp_listner();
};


#define PUB_ID_CHECKSUM_SIZE 4 // tail of pub id to check pub id valid
#define PUB_ID_INDEP_SIZE (SIZE_PUBID - PUB_ID_CHECKSUM_SIZE)

//bool check_pubid_valid( const byte *raw_pub_id );
//bool check_pubid_valid( const asptr &pub_id );

#pragma pack(push,1)

enum block_type_e // hard order!!!
{
    BT_MESSAGE = MT_MESSAGE,
    __UNUSED_BT_ACTION, // = MT_ACTION,

    __bt_service,
    __bt_no_save_begin = 100,
    __bt_no_need_ater_save_begin = __bt_no_save_begin,

    BT_CHANGED_NAME,
    BT_CHANGED_STATUSMSG,
    BT_OSTATE,
    BT_GENDER,
    
    __bt_no_need_ater_save_end,
    __bt_0 = 200,

    BT_CALL,
    BT_CALL_CANCEL,
    BT_CALL_ACCEPT,
    BT_AUDIO_FRAME,
    BT_GETAVATAR,
    BT_AVATARHASH,
    BT_AVATARDATA,
    BT_SENDFILE,
    BT_FILE_ACCEPT,
    BT_FILE_BREAK,
    BT_FILE_PAUSE,
    BT_FILE_UNPAUSE,
    BT_FILE_DONE,
    BT_FILE_CHUNK,
    BT_TYPING,
    BT_STREAM_OPTIONS,
    BT_INITDECODER,
    BT_VIDEO_FRAME,

    __bt_no_save_end,

};

static_assert(__bt_no_need_ater_save_end < __bt_0, "!");

struct datablock_s
{
    u64 delivery_tag;
    datablock_s *next;
    datablock_s *prev;
    block_type_e bt;
    int len;
    int sent;

    u64 create_time() const { ASSERT(BT_MESSAGE == bt); return *(u64 *)(this + 1); }
    asptr text() const { ASSERT(BT_MESSAGE == bt); return asptr(((const char *)(this + 1)) + sizeof(u64), len - sizeof(u64)); }
    const byte *data() const {return (const byte *)(this + 1);}

    static datablock_s *build(block_type_e mt, u64 delivery_tag, const void *data, aint datasize, const void *data1 = nullptr, aint datasize1 = 0);
    void die();

    int left() const { return max( 0, len - sent ); }

};
#pragma pack(pop)

enum video_codec_e
{
    vc_vp8,
    vc_vp9,
};

class lan_engine : public packetgen
{
    friend struct chunk;

    udp_listner broadcast_trap;
    udp_sender  broadcast_seek;
    tcp_listner tcp_in;
    host_functions_s *hf;

    time_t last_activity = now();

    std::vector<byte> avatar;
    bool avatar_set = false;
    bool media_data_transfer = false;
    bool fatal_error = false;

    int nextmastertaggeneration = 0;
    int nexthallo = 0;
    int listen_port = -1;

    int changed_some = 0;


    struct stream_reader
    {
        int offset;
        int maxlen;
        const byte *data;
        stream_reader(const void *d, int maxl) :data((const byte *)d), maxlen(maxl)
        {
            offset = SIZE_PACKET_HEADER;
            USHORT sz = ntohs( *(USHORT *)(data+2) );
            if (maxlen > sz) maxlen = sz;
            if (maxlen < sz) maxlen = SIZE_PACKET_HEADER; // not all data
        } // tcp
        stream_reader(int offset, const void *d, int maxl) :offset(offset), data((const byte *)d), maxlen(maxl) {} // udp
        bool end() const {return offset >= maxlen; }
        byte readb(byte def = 0)
        {
            const byte *v = (const byte *)read(1);
            if (!v) return def;
            return *v;
        }
        USHORT readus(USHORT def = 0)
        {
            const USHORT *v = (const USHORT *)read( sizeof(USHORT) );
            if (!v) return def;
            return ntohs( *v );
        }
        int readi(int def = 0)
        {
            const int *v = (const int *)read(sizeof(int));
            if (!v) return def;
            return ntohl(*v);
        }
        u64 readll(u64 def = 0)
        {
            const u64 *v = (const u64 *)read(sizeof(u64));
            if (!v) return def;
            return my_ntohll(*v);
        }
        const byte *read(int size)
        {
            if (!ASSERT(size <= last())) return nullptr;
            int ofsstore = offset;
            offset += size;
            return data + ofsstore;
        }
        str_c reads()
        {
            int sz = readus();
            const byte *p = read(sz);
            if (!p) return str_c();
            return str_c((const char *)p, sz);
        }
        int last() const { return maxlen - offset; }
    };

    void stop_encoder();
public:

    static void video_encoder();
    video_codec_e use_vcodec = vc_vp8;
    int use_vquality = 0;
    int use_vbitrate = 0;

    struct contact_s;
    struct media_stuff_s
    {
        contact_s *owner;
        media_stuff_s *prev = nullptr;
        media_stuff_s *next = nullptr;

        spinlock::long3264 sync_frame = 0;
        u64 sblock = 0;
        datablock_s *nblock = nullptr;

        const void *video_data = nullptr;
        OpusDecoder *audio_decoder = nullptr;
        OpusEncoder *audio_encoder = nullptr;
        fifo_stream_c enc_fifo;
        std::vector<byte> uncompressed;
        std::vector<byte> compressed;

        stream_options_s local_so;
        stream_options_s remote_so;
        vpx_codec_ctx_t v_encoder;
        vpx_codec_ctx_t v_decoder;
        vpx_codec_enc_cfg_t enc_cfg;
        vpx_codec_dec_cfg_t cfg_dec;

        int locked = 0; // locked in encoder

        video_codec_e vcodec = vc_vp8;
        video_codec_e vdecodec = vc_vp8;
        int vbitrate = 0;
        int vquality = -1;
        uint32_t video_w = 0;
        uint32_t video_h = 0;
        uint32_t frame_counter = 0;

        int next_time_send = 0;
        bool processing = false;
        bool decoder = false;

        void init_audio_encoder();
        void tick(contact_s *, int ct);
        void add_audio( const void *data, int datasize );
        int encode_audio(byte *dest, aint dest_max, const void *uncompressed_frame, aint frame_size);
        int prepare_audio4send(int ct); // grabs enc_fifo buffer and put compressed frame to this->compressed buffer
        int decode_audio( const void *data, int datasize ); // pcm decoded audio stored to this->uncompressed

        void encode_video_and_send( const byte *y, const byte *u, const byte *v );
        void video_frame( int framen, const byte *frame_data, int framesize );

        media_stuff_s( contact_s *owner );
        ~media_stuff_s();
    };

    struct delivery_data_s
    {
        size_t rcv_size = 0;
        std::vector<byte> buf;
    };

    typedef std::unordered_map< u64, std::unique_ptr<delivery_data_s> > delivery_map_t;

    spinlock::syncvar< delivery_map_t > delivery;


    struct contact_s
    {
        contact_s *next = nullptr;
        contact_s *prev = nullptr;

        media_stuff_s *media = nullptr;

        tcp_pipe pipe;

        i32 id;
        int portshift = 0;
        int nextactiontime = 0;
        int call_stop_time = 0;
        int next_keepalive_packet = 0;
        int keepalive_answer_deadline = 0;

        byte temporary_key[SIZE_KEY]; // crypto key for unauthorized interactions (before accept invite)
        byte authorized_key[SIZE_KEY];

        byte public_key[SIZE_PUBLIC_KEY];
        byte raw_public_id[SIZE_PUBID];
        str_c public_id;
        str_c name;
        str_c statusmsg;
        str_c invitemessage;
        str_c client;

        contact_online_state_e ostate = COS_ONLINE;
        contact_gender_e gender = CSEX_UNKNOWN;

        int changed_self = 0;
        i32 avatar_tag = 0;
        byte avatar_hash[16];

        int reconnect = 0;
       
        bool key_sent = false;
        bool data_changed = false;
        bool waiting_keepalive_answer = false;

        enum { CALL_OFF, OUT_CALL, IN_CALL, IN_PROGRESS } call_status = CALL_OFF;
        enum { SEARCH, MEET, TRAPPED, INVITE_SEND, INVITE_RECV, REJECTED, ACCEPT, ACCEPT_RESTORE_CONNECT, REJECT, ONLINE, OFFLINE, BACKCONNECT, ROTTEN, ALMOST_ROTTEN } state = SEARCH;

        struct dblist_s
        {
            datablock_s *sendblock_f = nullptr;
            datablock_s *sendblock_l = nullptr;

            void add( datablock_s *m )
            {
                LIST_ADD( m, sendblock_f, sendblock_l, prev, next );
            }

            void reset()
            {
                for ( ; sendblock_f;)
                {
                    datablock_s *m = sendblock_f;
                    LIST_DEL( m, sendblock_f, sendblock_l, prev, next );
                    m->die();
                }
            }
            bool del( u64 utag )
            {

                for ( datablock_s *m = sendblock_f; m; m = m->next )
                {
                    if ( m->delivery_tag == utag )
                    {
                        if ( m->sent == 0 )
                        {
                            LIST_DEL( m, sendblock_f, sendblock_l, prev, next );
                            m->die();
                        }
                        return true;
                    }
                }
                return false;

            }
        };
        spinlock::syncvar< dblist_s > sendblocks;

        void send_message( u64 create_time, const asptr &text, u64 dtag)
        {
            u64 create_time_net = my_htonll(create_time);
            send_block(BT_MESSAGE, dtag, &create_time_net, sizeof(u64), text.s, text.l);
        }
        datablock_s *build_block( block_type_e bt, u64 delivery_tag, const void *data = nullptr, aint datasize = 0, const void *data1 = nullptr, aint datasize1 = 0 );
        u64 send_block(block_type_e bt, u64 delivery_tag, const void *data = nullptr, aint datasize = 0, const void *data1 = nullptr, aint datasize1 = 0);
        u64 send_block( datablock_s *b );
        bool del_block( u64 delivery_tag );

        void to_offline(int ct);
        void online_tick(int ct, int nexttime = 500); // not always online
        void start_media();

        
        const byte *message_key(bool *is_auth = nullptr) const
        {
            if (state == ONLINE || state == OFFLINE || state == BACKCONNECT || state == ACCEPT) { if (is_auth) *is_auth = true; return authorized_key; }
            if (is_auth) *is_auth = false;
            return temporary_key;
        }

        void calculate_pub_id( const byte *pk );

        void fill_data( contact_data_s &cd )
        {
            data_changed = false;

            cd.id = id;
            cd.mask = CDM_PUBID | CDM_NAME | CDM_STATUSMSG | CDM_STATE | CDM_ONLINE_STATE | CDM_GENDER | CDM_AVATAR_TAG;
            cd.public_id = public_id.cstr();
            cd.public_id_len = (int)public_id.get_length();
            cd.name = name.cstr();
            cd.name_len = (int)name.get_length();
            cd.status_message = statusmsg.cstr();
            cd.status_message_len = (int)statusmsg.get_length();
            cd.avatar_tag = 0;
            switch (state)
            {
                case lan_engine::contact_s::SEARCH:
                case lan_engine::contact_s::MEET:
                case lan_engine::contact_s::INVITE_SEND:
                    cd.state = CS_INVITE_SEND;
                    break;
                case lan_engine::contact_s::REJECTED:
                    cd.state = CS_REJECTED;
                    break;
                case lan_engine::contact_s::INVITE_RECV:
                case lan_engine::contact_s::TRAPPED:
                case lan_engine::contact_s::REJECT:
                case lan_engine::contact_s::ACCEPT_RESTORE_CONNECT:
                    cd.state = CS_INVITE_RECEIVE;
                    break;
                case lan_engine::contact_s::ACCEPT:
                    cd.state = CS_WAIT;
                    break;
                case lan_engine::contact_s::ONLINE:
                    cd.state = CS_ONLINE;
                    break;
                case lan_engine::contact_s::OFFLINE:
                case lan_engine::contact_s::BACKCONNECT:
                    cd.state = CS_OFFLINE;
                    break;
                case lan_engine::contact_s::ROTTEN:
                case lan_engine::contact_s::ALMOST_ROTTEN:
                    cd.state = CS_ROTTEN;
                    break;
            }
            cd.ostate = ostate;
            cd.gender = gender;
            cd.avatar_tag = avatar_tag;
        }

        contact_s(int id):id(id)
        {
            nextactiontime = time_ms() - 10000000;
            memset(avatar_hash, 0, sizeof(avatar_hash));
        }
        ~contact_s();

        void recv();
        void stop_call_activity(bool notify_me = true);
        void handle_packet(packet_id_e pid, stream_reader &r );

    };
private:
    
    struct file_transfer_s
    {
        file_transfer_s *prev;
        file_transfer_s *next;

        file_transfer_s(const asptr &fn);
        virtual ~file_transfer_s();
        virtual void accepted(u64 /*offset*/) {}
        virtual void kill(bool /*from_self*/);
        virtual void pause(bool /*from_self*/) {}
        virtual void unpause(bool /*from_self*/) {}
        virtual void finished(bool /*from_self*/) {}
        virtual void tick(int ct) = 0;

        virtual void chunk_received( u64 /*offset*/, const void * /*d*/, aint /*dsz*/ ) {}; // incoming
        virtual bool delivered(u64 /*dtg*/) { return false; } // transmitting

        u32 cid;    // client id
        u64 fsz;    // filesize
        u64 utag;   // unique tag
        u64 sid;    // stream id
        str_c fn;   // filename
    };

    file_transfer_s *first_ftr = nullptr;
    file_transfer_s *last_ftr = nullptr;

    void tick_ftr(int ct);

    struct incoming_file_s : public file_transfer_s
    {
        u64 nextoffset = 0;
        int resumein = 0;
        bool stuck = false;
        bool is_accepted = false;

        incoming_file_s(u32 cid_, u64 utag_, u64 fsz_, const asptr &fn);
        ~incoming_file_s() {}

        //void recv_data(u64 position, const byte *data, size_t datasz)
        //{
        //    hf->file_portion(utag, position, data, datasz);
        //}

        /*virtual*/ void accepted(u64 offset) override;
        /*virtual*/ void finished(bool from_self) override;
        /*virtual*/ void pause(bool from_self) override;
        /*virtual*/ void unpause(bool from_self) override;
        /*virtual*/ void tick(int ct) override;

        /*virtual*/ void chunk_received( u64 offset, const void *d, aint dsz ) override;
    };


    struct transmitting_file_s : public file_transfer_s
    {
        struct ready_chunk_s
        {
            u64 dtg = 0;
            u64 offset = 0;
            const void *buf = nullptr;
            int size = 0;

            bool set(const file_portion_s *fp);
            void clear();

            ~ready_chunk_s() { clear(); }

        } rch[2];

        void reques_next_block( u64 offset_ );
        void send_block( contact_s *c );

        bool request = false;
        bool is_accepted = false;
        bool is_paused = false;
        bool is_finished = false;
        bool last_chunk_delivered = false;

        transmitting_file_s(contact_s *to_contact, u64 utag_, u64 fsz_, const asptr &fn);
        ~transmitting_file_s() {}

        /*virtual*/ void accepted(u64 offset) override;
        /*virtual*/ void finished(bool from_self) override;
        /*virtual*/ void pause(bool from_self) override;
        /*virtual*/ void unpause(bool from_self) override;
        /*virtual*/ void tick(int ct) override;

        bool fresh_file_portion(const file_portion_s *fp);
        /*virtual*/ bool delivered(u64 dtg) override;
    };

    incoming_file_s *find_incoming_ftr( u64 utag );
    transmitting_file_s *find_transmitting_ftr( u64 utag );
    file_transfer_s *find_ftr_by_sid( u64 sid );
    u64 gensid();
    u64 genfutag();

private:
    contact_s *first = nullptr; // first points to zero contact - self
    contact_s *last = nullptr;

    void del(contact_s *c);
    contact_s *addnew(int id = 0);
    contact_s *find(int id)
    {
        for(contact_s *i = first; i; i = i->next)
            if (i->id == id) return i;
        return nullptr;
    }
    contact_s *find_by_pk(const byte *public_key); // by public key (NOT PUBLIC ID!!!)
    contact_s *find_by_rpid(const byte *raw_public_id);

    
    void setup_communication_stuff();
    void shutdown_communication_stuff();


    void pp_search( unsigned int IPv4, int back_port, const byte *trapped_contact_public_key, const byte *seeking_raw_public_id );
    void pp_hallo( unsigned int IPv4, int back_port, int mastertag, const byte *hallo_contact_public_key );

    tcp_pipe * pp_meet( tcp_pipe * pipe, stream_reader &&r );
    tcp_pipe * pp_nonce( tcp_pipe * pipe, stream_reader &&r );

    lan_engine(host_functions_s *hf);
    ~lan_engine();

    static lan_engine *engine;
public:

    static void create( host_functions_s *hf );
    static lan_engine *get(bool rugacco = true) { if (rugacco) { ASSERT(engine != nullptr); } return engine; }

#define FUNC0( rt, fn ) rt fn();
#define FUNC1( rt, fn, p0 ) rt fn(p0);
#define FUNC2( rt, fn, p0, p1 ) rt fn(p0, p1);
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0

};