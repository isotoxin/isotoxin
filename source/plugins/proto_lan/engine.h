#pragma once

#define LOGGING 0
#if defined _DEBUG_OPTIMIZED || defined _FINAL
#undef LOGGING
#define LOGGING 0
#endif


#define AUDIO_SAMPLERATE 48000
#define AUDIO_BITS 16
#define AUDIO_CHANNELS 1

#define OPUS_EXPORT
#include <opus.h>

__forceinline int time_ms()
{
    return (int)timeGetTime();
}

struct socket_s
{
    SOCKET _socket = INVALID_SOCKET;

    void close()
    {
        if (_socket != INVALID_SOCKET)
        {
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
    }
    void flush_and_close()
    {
        if (_socket != INVALID_SOCKET)
        {
            shutdown(_socket, SD_SEND);
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
    }
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

struct udp_sender : public socket_s
{
    int port;

    void prepare();
    void send(const void *data, int size, int portshift);
    udp_sender(int port) :port(port) {}
};

struct tcp_pipe : public socket_s
{
    sockaddr_in addr;
    int creationtime = 0;
    byte rcvbuf[65536];
    int rcvbuf_sz = 0;

    tcp_pipe() { creationtime = time_ms(); }
    tcp_pipe( SOCKET s, const sockaddr_in& addr ):addr(addr) { _socket = s; creationtime = time_ms(); }

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
            else if (ASSERT(rcvbuf_sz > sz))
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

bool check_pubid_valid( const byte *raw_pub_id );
bool check_pubid_valid( const asptr &pub_id );

#pragma pack(push,1)

enum message_type_lan_e // hard order!!!
{
    MTL_MESSAGE = MT_MESSAGE,
    MTL_ACTION = MT_ACTION,

    __mtl_no_save_begin = 100,
    __mtl_no_need_ater_save_begin = __mtl_no_save_begin,

    MTL_CHANGED_NAME,
    MTL_CHANGED_STATUSMSG,
    MTL_OSTATE,
    MTL_GENDER,
    
    __mtl_no_need_ater_save_end,
    __mtl_0 = 200,

    MTL_CALL,
    MTL_CALL_CANCEL,
    MTL_CALL_ACCEPT,
    MTL_AUDIO_FRAME,

    __mtl_no_save_end,

};

static_assert(__mtl_no_need_ater_save_end < __mtl_0, "!");

struct msg_s
{
    u64 create_time;
    u64 delivery_tag;
    msg_s *next;
    msg_s *prev;
    message_type_lan_e mt;
    int len;
    int sent;

    asptr text() const { return asptr((const char *)(this + 1), len); }

    static msg_s *build(message_type_lan_e mt, u64 create_time, const asptr&text, u64 utag);
    void die();

    int left() const { return len - sent; }

};
#pragma pack(pop)

class lan_engine : public packetgen
{
    friend struct chunk;

    udp_listner broadcast_trap;
    udp_sender  broadcast_seek;
    tcp_listner tcp_in;
    host_functions_s *hf;

    //int last_message;

    int lasthallo;
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
            return ntohll(*v);
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

public:

    struct contact_s;
    struct media_stuff_s
    {
        OpusDecoder *audio_decoder = nullptr;
        OpusEncoder *audio_encoder = nullptr;
        fifo_stream_c enc_fifo;
        std::vector<byte> uncompressed;
        std::vector<byte> compressed;

        void init_audio_encoder();
        void tick(contact_s *);
        void add_audio( const void *data, int datasize );
        int encode_audio(byte *dest, int dest_max, const void *uncompressed_frame, int frame_size);
        int prepare_audio4send(); // grabs enc_fifo buffer and put compressed frame to this->compressed buffer
        int decode_audio( const void *data, int datasize ); // pcm decoded audio stored to this->uncompressed

        ~media_stuff_s();
    };

    struct contact_s
    {
        contact_s *next = nullptr;
        contact_s *prev = nullptr;

        media_stuff_s *media = nullptr;

        struct delivery_data_s
        {
            u64 create_time = 0;
            size_t rcv_size = 0;
            std::vector<byte> buf;
        };

        std::unordered_map< u64, delivery_data_s > delivery;

        tcp_pipe pipe;

        int id;
        int portshift = 0;
        int nextactiontime = 0;
        int call_stop_time = 0;
        int correct_create_time = 0;
        int next_sync = 0;

        byte temporary_key[SIZE_KEY]; // crypto key for unauthorized interactions (before accept invite)
        byte authorized_key[SIZE_KEY];

        byte public_key[SIZE_PUBLIC_KEY];
        byte raw_public_id[SIZE_PUBID];
        str_c public_id;
        str_c name;
        str_c statusmsg;
        str_c invitemessage;

        contact_online_state_e ostate = COS_ONLINE;
        contact_gender_e gender = CSEX_UNKNOWN;

        int changed_self = 0;

        int reconnect = 0;
        bool key_sent = false;
        bool data_changed = false;
        bool need_resync = false;

        enum { CALL_OFF, OUT_CALL, IN_CALL, IN_PROGRESS } call_status = CALL_OFF;
        enum { SEARCH, MEET, TRAPPED, INVITE_SEND, INVITE_RECV, REJECTED, ACCEPT, ACCEPT_RESTORE_CONNECT, REJECT, ONLINE, OFFLINE, BACKCONNECT, ROTTEN } state = SEARCH;

        msg_s *sendmessage_f = nullptr;
        msg_s *sendmessage_l = nullptr;

        void add_message(message_type_lan_e mt, u64 put_time, asptr text, u64 utag);

        void online_tick(int ct, int nexttime = 500); // not always online
        void start_media();

        const byte *message_key() const
        {
            if (state == ONLINE || state == OFFLINE || state == BACKCONNECT || state == ACCEPT) return authorized_key;
            return temporary_key;
        }

        void calculate_pub_id( const byte *pk );

        void fill_data( contact_data_s &cd )
        {
            data_changed = false;

            cd.id = id;
            cd.mask = -1;
            cd.public_id = public_id.cstr();
            cd.public_id_len = public_id.get_length();
            cd.name = name.cstr();
            cd.name_len = name.get_length();
            cd.status_message = statusmsg.cstr();
            cd.status_message_len = statusmsg.get_length();
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
                    cd.state = CS_ROTTEN;
                    break;
            }
            cd.ostate = ostate;
            cd.gender = gender;
        }

        contact_s(int id):id(id)
        {
            nextactiontime = time_ms() - 10000000;
        }
        ~contact_s();

        void recv();
        void stop_call_activity(bool notify_me = true);
        void handle_packet(packet_id_e pid, stream_reader &r );

    };
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
    void pp_hallo( unsigned int IPv4, int back_port, const byte *hallo_contact_public_key );

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