#include "stdafx.h"
#include <Iphlpapi.h>

#define AUDIO_BITRATE 32000
#define AUDIO_SAMPLEDURATION_MAX 60
#define AUDIO_SAMPLEDURATION_MIN 20

#define DEFAULT_VIDEO_BITRATE 5000

#define BROADCAST_PORT 0x8ee8
#define BROADCAST_RANGE 10

#define TCP_PORT 0x7ee7
#define TCP_RANGE 10

#define VIDEO_CODEC_DECODER_INTERFACE_VP8 (vpx_codec_vp8_dx())
#define VIDEO_CODEC_ENCODER_INTERFACE_VP8 (vpx_codec_vp8_cx())
#define VIDEO_CODEC_DECODER_INTERFACE_VP9 (vpx_codec_vp9_dx())
#define VIDEO_CODEC_ENCODER_INTERFACE_VP9 (vpx_codec_vp9_cx())
#define MAX_ENCODE_TIME_US (1000000 / 5)

namespace
{
    struct av_sender_state_s
    {
        // list of call-in-progress descriptos
        lan_engine::media_stuff_s *first = nullptr;
        lan_engine::media_stuff_s *last = nullptr;

        bool video_encoder = false;
        bool video_encoder_heartbeat = false;
        volatile bool shutdown = false;

        bool senders() const
        {
            return video_encoder;
        }

        bool allow_run_video_encoder() const
        {
            return !video_encoder && !shutdown;
        }
    };
}

static spinlock::syncvar<av_sender_state_s> callstate;

lan_engine::media_stuff_s::media_stuff_s( contact_s *owner ) :owner( owner )
{
    enc_cfg.g_h = 0;
    enc_cfg.g_w = 0;
    cfg_dec.threads = 1;
    cfg_dec.w = 0;
    cfg_dec.h = 0;

    auto w = callstate.lock_write();
    LIST_ADD( this, w().first, w().last, prev, next );
}

lan_engine::media_stuff_s::~media_stuff_s()
{
    auto w = callstate.lock_write();
    LIST_DEL( this, w().first, w().last, prev, next );

    while ( locked > 0 ) // waiting unlock
    {
        w.unlock();
        Sleep( 1 );
        w = callstate.lock_write();
    }
    w.unlock();

    if (audio_encoder)
        opus_encoder_destroy(audio_encoder);

    if (audio_decoder)
        opus_decoder_destroy(audio_decoder);

    if ( video_data )
        engine->hf->free_video_data( video_data );

    if ( enc_cfg.g_w ) vpx_codec_destroy( &v_encoder );
    if ( decoder ) vpx_codec_destroy( &v_decoder );

    if ( nblock )
        nblock->die();
}

namespace
{
#pragma pack(push,1)
    struct framehead_s
    {
        u64 msmonotonic;
        u_long frame;
    };
#pragma pack(pop)
    static_assert( sizeof( framehead_s ) == 12, "size!" );
}


void lan_engine::media_stuff_s::encode_video_and_send( u64 msmonotonic, const byte *y, const byte *u, const byte *v )
{
    if ( engine->use_vquality < 0 )
    {
        vquality = -1;
        if ( enc_cfg.g_w ) vpx_codec_destroy( &v_encoder );
        if ( decoder ) vpx_codec_destroy( &v_decoder );
        enc_cfg.g_w = 0;
        decoder = false;

        i32 vals[ 3 ] = {};
        vals[ 2 ] = (i32)htonl( (u_long)vc_vp8 );
        owner->send_block( BT_INITDECODER, 0, &vals, sizeof(vals) );

        if ( nblock )
        {
            nblock->die();
            nblock = nullptr;
        }

        return;
    }

    if ( enc_cfg.g_w != video_w || enc_cfg.g_h != video_h || vcodec != engine->use_vcodec || vbitrate != engine->use_vbitrate )
    {
        sblock = 0;
        if ( nblock )
        {
            nblock->die();
            nblock = nullptr;
        }

        i32 vals[ 3 ];
        vals[ 0 ] = htonl( video_w );
        vals[ 1 ] = htonl( video_h );
        vals[ 2 ] = htonl( engine->use_vcodec );
        owner->send_block( BT_INITDECODER, 0, &vals, sizeof( vals ) );

        vcodec = engine->use_vcodec;
        vbitrate = engine->use_vbitrate;

        if ( enc_cfg.g_w )
        {
            vpx_codec_destroy( &v_encoder );
            if ( vcodec != engine->use_vcodec )
                goto reinit_cfg;
        } else
        {
        reinit_cfg:
            vcodec = engine->use_vcodec;

            if ( VPX_CODEC_OK != vpx_codec_enc_config_default( vcodec == vc_vp8 ? VIDEO_CODEC_ENCODER_INTERFACE_VP8 : VIDEO_CODEC_ENCODER_INTERFACE_VP9, &enc_cfg, 0 ) )
            {
                enc_cfg.g_w = 0;
                vpx_codec_destroy( &v_decoder );
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

        vcodec = engine->use_vcodec;
        enc_cfg.g_w = video_w;
        enc_cfg.g_h = video_h;
        enc_cfg.rc_target_bitrate = (vbitrate ? vbitrate : DEFAULT_VIDEO_BITRATE);

        if ( vpx_codec_enc_init( &v_encoder, vcodec == vc_vp8 ? VIDEO_CODEC_ENCODER_INTERFACE_VP8 : VIDEO_CODEC_ENCODER_INTERFACE_VP9, &enc_cfg, 0 ) != VPX_CODEC_OK )
        {
            enc_cfg.g_w = 0;
            return;
        }
    }

    if ( vquality != engine->use_vquality )
    {
        vquality = engine->use_vquality;
        if ( vquality < 0 ) vquality = 0;
        if ( vquality > 100 ) vquality = 100;
        if ( vquality == 0 )
            vpx_codec_control( &v_encoder, VP8E_SET_CPUUSED, 0 );
        else
        {
            int vv = vcodec == vc_vp8 ? ( ( 100 - vquality ) * 16 / 99 ) : ( ( 100 - vquality ) * 8 / 99 );
            vpx_codec_control( &v_encoder, VP8E_SET_CPUUSED, vv );
        }
    }

    for ( auto r = engine->delivery.lock_read(); sblock;)
    {
        if ( r().find( sblock ) == r().end() )
        {
            if ( nblock )
            {
                sblock = owner->send_block( nblock );
                nblock = nullptr;
                break;
            }
            else
                sblock = 0;
        }

        break;
    }

    if ( nblock )
        return; // skip frame before encoding to keep quality of video (just lower fps)

    // encoding and send

    vpx_image_t img = { VPX_IMG_FMT_I420, VPX_CS_UNKNOWN, VPX_CR_STUDIO_RANGE,
        video_w, video_h, 8, video_w, video_h, 0, 0, 1, 1,
        (byte *)y, (byte *)u, (byte *)v, nullptr, (int)video_w, (int)video_w / 2, (int)video_w / 2, (int)video_w, 12 };

    int vrc = vpx_codec_encode( &v_encoder, &img, frame_counter, 1, 0, /*MAX_ENCODE_TIME_US*/ 0 );
    if ( vrc != VPX_CODEC_OK ) return;

    vpx_codec_iter_t iter = nullptr;
    const vpx_codec_cx_pkt_t *pkt;

    while ( nullptr != ( pkt = vpx_codec_get_cx_data( &v_encoder, &iter ) ) )
    {
        if ( pkt->kind == VPX_CODEC_CX_FRAME_PKT )
        {
            //add_frame_to_queue( pkt->data.frame.buf, pkt->data.frame.sz, pkt->data.frame.flags );

            framehead_s fh;
            fh.frame = htonl( frame_counter );
            fh.msmonotonic = my_htonll( msmonotonic );

            if ( !sblock )
                sblock = owner->send_block( BT_VIDEO_FRAME, 0, &fh, sizeof( fh ), pkt->data.frame.buf, pkt->data.frame.sz );
            else
                nblock = owner->build_block( BT_VIDEO_FRAME, 0, &fh, sizeof( fh ), pkt->data.frame.buf, pkt->data.frame.sz );
        }
    }

    ++frame_counter;
}


void lan_engine::media_stuff_s::video_frame( u64 msmonotonic, int /*framen*/, const byte *frame_data, int framesize )
{
    if ( !decoder )
    {
        int rc = vpx_codec_dec_init( &v_decoder, vdecodec == vc_vp8 ? VIDEO_CODEC_DECODER_INTERFACE_VP8 : VIDEO_CODEC_DECODER_INTERFACE_VP9, &cfg_dec, 0 );
        if ( rc != VPX_CODEC_OK ) return;
        decoder = true;
    }

    if ( VPX_CODEC_OK == vpx_codec_decode( &v_decoder, frame_data, framesize, nullptr, 0 ) )
    {
        vpx_codec_iter_t iter = nullptr;
        vpx_image_t *dest = vpx_codec_get_frame( &v_decoder, &iter );

        // Play decoded images
        for ( ; dest; dest = vpx_codec_get_frame( &v_decoder, &iter ) )
        {
            media_data_s mdt;
            mdt.vfmt.width = (unsigned short)dest->d_w;
            mdt.vfmt.height = (unsigned short)dest->d_h;
            mdt.vfmt.pitch[ 0 ] = (unsigned short)dest->stride[ 0 ];
            mdt.vfmt.pitch[ 1 ] = (unsigned short)dest->stride[ 1 ];
            mdt.vfmt.pitch[ 2 ] = (unsigned short)dest->stride[ 2 ];
            mdt.vfmt.fmt = VFMT_I420;
            mdt.video_frame[ 0 ] = dest->planes[ 0 ];
            mdt.video_frame[ 1 ] = dest->planes[ 1 ];
            mdt.video_frame[ 2 ] = dest->planes[ 2 ];
            mdt.msmonotonic = msmonotonic;
            engine->hf->av_data(contact_id_s(), owner->id, &mdt );

            vpx_img_free( dest );
        }
    }

}

void lan_engine::media_stuff_s::init_audio_encoder()
{
    processing = false;

    if (audio_encoder)
        opus_encoder_destroy(audio_encoder);

    int rc = OPUS_OK;
    audio_encoder = opus_encoder_create(AUDIO_SAMPLERATE, AUDIO_CHANNELS, OPUS_APPLICATION_VOIP, &rc);

    if (rc != OPUS_OK)
        return;

    rc = opus_encoder_ctl(audio_encoder, OPUS_SET_BITRATE(AUDIO_BITRATE));

    if (rc != OPUS_OK)
        return;

    rc = opus_encoder_ctl(audio_encoder, OPUS_SET_COMPLEXITY(10));

    if (rc != OPUS_OK)
        return;

}

void lan_engine::media_stuff_s::add_audio( u64 msmonotonic, const void *data, int datasize )
{
    if (audio_encoder == nullptr)
    {
        enc_fifo.clear();
        init_audio_encoder();
    }

    if ( msmonotonic )
        a_msmonotonic = msmonotonic - audio_format_s( AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS ).bytesToMSec( enc_fifo.available() );

    enc_fifo.add_data(data, datasize);

    if ( (int)enc_fifo.available() > audio_format_s( AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS ).avgBytesPerSec() )
    {
        // remove some overbuffer data

        enc_fifo.read_data( nullptr, datasize );
        if ( a_msmonotonic )
            a_msmonotonic += audio_format_s( AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS ).bytesToMSec( datasize );
    }
}

int lan_engine::media_stuff_s::encode_audio(byte *dest, aint dest_max, const void *uncompressed_frame, aint frame_size)
{
    if (audio_encoder)
        return opus_encode(audio_encoder, (opus_int16 *)uncompressed_frame, (int)frame_size, dest, (int)dest_max);

    return 0;
}

int lan_engine::media_stuff_s::prepare_audio4send(int ct)
{
    aint avsize = enc_fifo.available();
    if (avsize == 0)
    {
        processing = false;
        return 0;
    }

    if ((next_time_send - ct) > 10)
        return 0;

    int sd = AUDIO_SAMPLEDURATION_MAX;
    int req_frame_size = audio_format_s(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS).avgBytesPerMSecs(AUDIO_SAMPLEDURATION_MAX);
    if (req_frame_size > avsize)
    {
        req_frame_size = audio_format_s(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS).avgBytesPerMSecs(AUDIO_SAMPLEDURATION_MIN);
        if (req_frame_size > avsize)
        {
            next_time_send = ct;
            return 0;
        }
        sd = AUDIO_SAMPLEDURATION_MIN;
    }

    if (!processing && (req_frame_size*2) > avsize)
        return 0;

    processing = true;
    next_time_send += sd;
    if ((next_time_send - ct) < 0) next_time_send = ct+sd/2;

    if (req_frame_size > (int)uncompressed.size()) uncompressed.resize(req_frame_size);
    aint samples = enc_fifo.read_data(uncompressed.data(), req_frame_size) / audio_format_s( AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS ).sampleSize();

    if (req_frame_size >(int)compressed.size()) compressed.resize(req_frame_size);

    if ( a_msmonotonic )
    {
        a_msmonotonic_compressed = a_msmonotonic;
        a_msmonotonic += audio_format_s( AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS ).samplesToMSec( samples );
    }

    return encode_audio(compressed.data(), req_frame_size, uncompressed.data(), samples);
}

int lan_engine::media_stuff_s::decode_audio( const void *data, int datasize )
{
    engine->media_data_transfer = true;

    int rc;
    if (audio_decoder == nullptr)
    {
        audio_decoder = opus_decoder_create(AUDIO_SAMPLERATE, AUDIO_CHANNELS, &rc);
        if (rc != OPUS_OK) return 0;
    }

    int frames = AUDIO_SAMPLERATE /** AUDIO_SAMPLEDURATION / 1000*/;
    int dec_size = frames * AUDIO_CHANNELS * AUDIO_BITS / 8;
    if ((int)uncompressed.size() < dec_size) uncompressed.resize(dec_size);

    rc = opus_decode(audio_decoder, (const byte *)data, datasize, (opus_int16 *)uncompressed.data(), frames, 0);
    if (rc > 0)
    {
        return rc * AUDIO_CHANNELS * AUDIO_BITS / 8;
    }
    return 0;
}

void lan_engine::media_stuff_s::tick(contact_s *c, int ct)
{
    engine->media_data_transfer = true;
    int sz = prepare_audio4send(ct);
    if ( sz > 0 )
    {
        u64 timelabel = my_htonll( a_msmonotonic_compressed );
        c->send_block( BT_AUDIO_FRAME, 0, &timelabel, sizeof( timelabel ), compressed.data(), sz );
    }
}


#if LOGGING
std::asptr pid_name(packet_id_e pid)
{
#define DESC_PID( p ) case p: return STD_ASTR( #p )
    switch (pid)
    {
        DESC_PID( PID_NONE );
        DESC_PID( PID_SEARCH );
        DESC_PID( PID_HALLO );
        DESC_PID( PID_MEET );
        DESC_PID( PID_NONCE );
        DESC_PID( PID_INVITE );
        DESC_PID( PID_ACCEPT );
        DESC_PID( PID_READY );
        DESC_PID( PID_DATA );
        DESC_PID( PID_DELIVERED );
        DESC_PID( PID_REJECT );
        DESC_PID( PID_KEEPALIVE );
    }
    return STD_ASTR("pid unknown");
}

void log_auth_key( const char * /*what*/, const byte * /*key*/ )
{
    /*
    str_c nonce_part; nonce_part.append_as_hex(key, SIZE_KEY_NONCE_PART);
    str_c pass_part; pass_part.append_as_hex(key + SIZE_KEY_NONCE_PART, SIZE_KEY_PASS_PART);
    Log( "%s / %s - %s", nonce_part.cstr(), pass_part.cstr(), what );
    */
}

void log_bytes( const char * /*what*/, const byte * /*b*/, int /*sz*/ )
{
    /*
    str_c bs; bs.append_as_hex(b, sz);
    Log("%s - %s", bs.cstr(), what);
    */
}

#define logm Log
#define logfn(fn,s,...) LogToFile(fn,s,__VA_ARGS__)
#else
#define log_auth_key(a,b)
#define log_bytes(a, b, c)
#define logm(...)
#define logfn(...)
#endif

void socket_s::close()
{
    if (_socket != INVALID_SOCKET)
    {
        MaskLog( LFLS_CLOSE, "%i", _socket );
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}
void socket_s::flush_and_close()
{
    if (_socket != INVALID_SOCKET)
    {
        /*int errm =*/ shutdown(_socket, SD_SEND);
        MaskLog( LFLS_CLOSE, "shutdown %i", _socket );
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
}

#pragma comment(lib, "Iphlpapi.lib")


void udp_sender::close()
{
    for( prepared_sock_s &s : socks )
    {
        MaskLog(LFLS_CLOSE, "udp sender %i", s.s);
        closesocket(s.s);
    }
}

void udp_sender::prepare()
{
    char cn[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD cnSize = ARRAY_SIZE(cn);
    memset(cn, 0, sizeof(cn));
    GetComputerNameA(cn, &cnSize);


    std::vector<IP_ADAPTER_INFO> adapters;
    adapters.resize(16);
    adapters.begin()->Next = nullptr; //-V807
    memcpy(adapters.begin()->Address, cn, 6);//-V512 - mac address length is actually 6 bytes


    ULONG sz = (ULONG)adapters.size() * sizeof(IP_ADAPTER_INFO);
    while (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(adapters.data(), &sz))
    {
        adapters.resize((sizeof(IP_ADAPTER_INFO) + sz) / sizeof(IP_ADAPTER_INFO));
        adapters.begin()->Next = nullptr;
        memcpy(adapters.begin()->Address, cn, 6);//-V512
    }

    const IP_ADAPTER_INFO *infos = adapters.data();
    do
    {
        unsigned int ip = inet_addr( infos->IpAddressList.IpAddress.String );
        unsigned int mask = inet_addr( infos->IpAddressList.IpMask.String );

        bool skip_this = false;
        prepared_sock_s *repl = nullptr;
        for( prepared_sock_s &s : socks )
        {
            if (s.ip == ip)
            {
                skip_this = true;
                break;
            }
            
            if ((mask & s.ip) == (mask & ip))
            {
                repl = &s;
                break;
            }
        }

        if (skip_this)
        {
            infos = infos->Next;
            if (!infos) break;
            continue;
        }

        SOCKET _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (repl)
        {
            closesocket(repl->s);
            repl->s = _socket;
        }

        int val = 1;
        if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_BROADCAST, (char*)&val, sizeof(val)))
        {
            oops:
            if (repl)
            {
                repl->ip = 0;
                repl->s = 0;
                socks.erase( socks.begin() + (repl - socks.data()) );
            }

            closesocket(_socket);
            infos = infos->Next;
            if (!infos) break;
            continue;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.S_un.S_addr = ip;

        if (SOCKET_ERROR == bind(_socket, (SOCKADDR*)&addr, sizeof(addr)))
            goto oops;

        if (!repl)
            socks.emplace_back( _socket, ip );

        infos = infos->Next;
    } while (infos);
}

bool udp_sender::send(const void *data, int size, int portshift)
{
    prepare();
    sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.S_un.S_addr = 0xFFFFFFFF; // BROADCAST
    dest_addr.sin_port = htons((unsigned short)(port + portshift));

    for( prepared_sock_s &s : socks )
    {
        for (;;)
        {
            int r = sendto(s.s, (const char *)data, size, 0, (sockaddr *)&dest_addr, sizeof(dest_addr));
            if (0 > r)
            {
                //int error = WSAGetLastError();
                //if (10054 == error) continue;
                //if ((10049 == error || 10065 == error) /*&& ADDR_BROADCAST == dest_addr->sin_addr.S_un.S_addr*/) continue;
                close();
                return false;
            };
            break;
        }
    }

    return true;
}

int udp_listner::open()
{
    sockaddr_in addr;

    for(int maxport = port + BROADCAST_RANGE;port < maxport;++port)
    {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (INVALID_SOCKET == _socket)
            continue;

        int val = 1;
        if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(val)))
        {
            close();
            continue;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = 0;
        addr.sin_port = htons((unsigned short)port);

        if (SOCKET_ERROR == bind(_socket, (SOCKADDR*)&addr, sizeof(addr))){
            close();
            continue;
        };
        return port;
    }
    return -1;
}

int udp_listner::read( void *data, int datasize, sockaddr_in &addr )
{
    fd_set rs;
    FD_ZERO(&rs);
    FD_SET(_socket, &rs);
    timeval ti = {0,100};
    if (1 != select(0, &rs, nullptr, nullptr, &ti)) return 0;

    int sizeaddr = sizeof(sockaddr_in);

    for(;;)
    {
        int numr = recvfrom(_socket, (char *)data, datasize, 0, (sockaddr*)&addr, &sizeaddr);
        if (0 > numr)
        {
            int error = WSAGetLastError();
            if (10054 == error) continue;
            close();
        } else
            return numr;
        break;
    }

    return 0;
}

static void sacceptor(tcp_listner *me)
{
    sockaddr_in addr;

    int port = me->state.lock_read()().port;

    for (int maxport = port + BROADCAST_RANGE; port < maxport; ++port)
    {
        me->_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (INVALID_SOCKET == me->_socket)
            continue;

        MaskLog( LFLS_ESTBLSH, "listener %i, port %i", me->_socket, port );

        addr.sin_family = AF_INET;
        addr.sin_addr.S_un.S_addr = 0;
        addr.sin_port = htons((unsigned short)port);

        if (SOCKET_ERROR == bind(me->_socket, (SOCKADDR*)&addr, sizeof(addr)))
        {
            MaskLog( LFLS_CLOSE, "bind fail %i, port %i", me->_socket, port );
            me->close();
            continue;
        };
        
        if (SOCKET_ERROR == listen(me->_socket, SOMAXCONN))
        {
            MaskLog( LFLS_CLOSE, "listen fail %i, port %i", me->_socket, port );
            me->close();
            continue;
        }

        auto w = me->state.lock_write();
        w().port = port;
        w().acceptor_works = true;
        w.unlock();


        for(;!me->state.lock_read()().acceptor_should_stop;)
        {
            int AddrLen = sizeof(addr);
            SOCKET s = accept(me->_socket, (sockaddr*)&addr, &AddrLen);
            if (INVALID_SOCKET == s)
                break;

            MaskLog( LFLS_ESTBLSH, "accept %i from %i.%i.%i.%i", s, (int)addr.sin_addr.S_un.S_un_b.s_b1, (int)addr.sin_addr.S_un.S_un_b.s_b2, (int)addr.sin_addr.S_un.S_un_b.s_b3, (int)addr.sin_addr.S_un.S_un_b.s_b4 );

            me->accepted.push( new tcp_pipe(s, addr) );
        }

        break;
    }

    auto ww = me->state.lock_write();
    ww().acceptor_stoped = true;
    ww().acceptor_works = false;
}

DWORD WINAPI tcp_listner::acceptor(LPVOID p)
{
    UNSTABLE_CODE_PROLOG
    sacceptor((tcp_listner *)p);
    UNSTABLE_CODE_EPILOG
    return 0;
}

int tcp_listner::open()
{
    CloseHandle(CreateThread(nullptr, 0, acceptor, this, 0, nullptr));

    int port;
    for(;state.lock_read()().wait_acceptor_start(port); Sleep(0)) ;
    return port;
}

void tcp_listner::close()
{
    state.lock_write()().acceptor_should_stop = true;
    super::close();
    for (; state.lock_read()().wait_acceptor_stop(); Sleep(0));

    tcp_pipe *p;
    for (; accepted.try_pop(p);)
        delete p;

    auto w = state.lock_write();
    w().acceptor_should_stop = false;
    w().acceptor_stoped = false;
    w().acceptor_works = false;
}

tcp_listner::~tcp_listner()
{
    close();
}

bool tcp_pipe::connect()
{
    if (connected()) closesocket(_socket);
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == _socket)
        return false;

    MaskLog( LFLS_ESTBLSH, "connect %i", _socket );

    int val = 1024 * 128;
    if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char*)&val, sizeof(val)))
    {
        close();
        return false;
    }
    if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char*)&val, sizeof(val)))
    {
        close();
        return false;
    }

    if (SOCKET_ERROR == ::connect(_socket, (LPSOCKADDR)&addr, sizeof(addr)))
    {
        close();
        //error_ = WSAGetLastError();
        //if (WSAEWOULDBLOCK != error_)
            return false;
    }

    MaskLog( LFLS_ESTBLSH, "connected %i", _socket );

    return true;
}

bool tcp_pipe::send( const byte *data, int datasize )
{
    const byte *d2s = data;
    int sent = 0;

    do
    {
        int iRetVal = ::send(_socket, (const char *)(d2s + sent), datasize - sent, 0);
        if (iRetVal == SOCKET_ERROR)
        {
            return false;
        }
        if (iRetVal == 0)
            __debugbreak();
        sent += iRetVal;
    } while (sent < datasize);

    return true;
}

packet_id_e tcp_pipe::packet_id()
{
    rcv_all();
    if ( rcvbuf_sz >= SIZE_PACKET_HEADER )
    {
        USHORT sz = ntohs(*(USHORT *)(rcvbuf+2));
        if (rcvbuf_sz >= sz)
        {
            packet_id_e pid = (packet_id_e)ntohs(*(short *)rcvbuf);;
            return pid;
        }
    }
    return connected() ? PID_NONE : PID_DEAD;
}

void tcp_pipe::rcv_all()
{
    if (rcvbuf_sz >= sizeof(rcvbuf))
        return;

    for (;;)
    {
        fd_set rs;
        rs.fd_count = 1;
        rs.fd_array[0] = _socket;

        TIMEVAL tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int n = ::select(0, &rs, nullptr, nullptr, &tv);
        if (n == 1)
        {
            int buf_free_space = sizeof(rcvbuf) - rcvbuf_sz;
            if (buf_free_space > 1300)
            {
                int reqread = SIZE_MAX_SEND_AUTH; 
                if (buf_free_space < reqread)
                    reqread = buf_free_space;

                int _bytes = ::recv(_socket, (char *)rcvbuf + rcvbuf_sz, reqread, 0);
                if (_bytes == 0 || _bytes == SOCKET_ERROR)
                {
                    // connection closed
                    close();
                    return;
                }
                rcvbuf_sz += _bytes;
                if ((sizeof(rcvbuf) - rcvbuf_sz) < 1300)
                    break;
                continue;
            }
        } else if(n == SOCKET_ERROR)
        {
            close();
        }

        break;
    }
}



static void protect_raw_id( byte *raw_pub_id )
{
    for (int i = PUB_ID_INDEP_SIZE; i < SIZE_PUBID; ++i)
        raw_pub_id[i] = (byte)i;

    byte hash[crypto_shorthash_BYTES];
    static_assert(crypto_shorthash_KEYBYTES <= PUB_ID_INDEP_SIZE, "pub id too short");
    crypto_shorthash(hash, raw_pub_id, PUB_ID_INDEP_SIZE, raw_pub_id + SIZE_PUBID - crypto_shorthash_KEYBYTES);
    memcpy(raw_pub_id + PUB_ID_INDEP_SIZE, hash, PUB_ID_CHECKSUM_SIZE); //-V512
}

static bool check_pubid_valid(const byte *raw_pub_id_i)
{
    byte raw_pub_id[SIZE_PUBID];
    memcpy(raw_pub_id,raw_pub_id_i, PUB_ID_INDEP_SIZE); //-V512
    protect_raw_id(raw_pub_id);
    return 0 == memcmp(raw_pub_id+PUB_ID_INDEP_SIZE, raw_pub_id_i+PUB_ID_INDEP_SIZE,PUB_ID_CHECKSUM_SIZE);
}

bool make_raw_pub_id( byte *raw_pub_id, const std::asptr &pub_id )
{
    if (ASSERT(pub_id.l == SIZE_PUBID * 2))
    {
        std::pstr_c(pub_id).hex2buf<SIZE_PUBID>(raw_pub_id);
        return true;
    }
    return false;
}

/*
static bool check_pubid_valid(const asptr &pub_id)
{
    byte raw_pub_id[SIZE_PUBID];
    if (make_raw_pub_id(raw_pub_id, pub_id))
        return check_pubid_valid(raw_pub_id);
    return false;
}
*/

void make_raw_pub_id( byte *raw_pub_id, const byte *pk )
{
    static_assert(crypto_generichash_BYTES_MIN >= PUB_ID_INDEP_SIZE, "pub id too short");

    crypto_generichash(raw_pub_id, PUB_ID_INDEP_SIZE, pk, SIZE_PUBLIC_KEY, nullptr, 0);
    protect_raw_id(raw_pub_id);
}

datablock_s *datablock_s::build(block_type_e mt, u64 delivery_tag_, const void *data, aint datasize, const void *data1, aint datasize1)
{
    datablock_s * m = (datablock_s *)dlmalloc( sizeof(datablock_s) + datasize + datasize1 );
    m->delivery_tag = delivery_tag_;
    m->next = nullptr;
    m->prev = nullptr;
    m->bt = mt;
    m->sent = 0;
    m->len = (int)(datasize + datasize1);
    memcpy( m+1, data, datasize );
    if (data1) memcpy( ((byte *)(m+1)) + datasize, data1, datasize1 );
    return m;
}
void datablock_s::die()
{
    dlfree(this);
}

void lan_engine::video_encoder()
{
    callstate.lock_write()( ).video_encoder = true;

    int sleepms = 100;
    int timeoutcountdown = 50; // 5 sec
    for ( ;; Sleep( sleepms ) )
    {
        auto w = callstate.lock_write();
        w().video_encoder_heartbeat = true;
        if ( w().shutdown )
        {
            w().video_encoder = false;
            return;
        }
        if ( nullptr == w().first )
        {
            --timeoutcountdown;
            if ( timeoutcountdown <= 0 )
            {
                w().video_encoder = false;
                return;
            }
            sleepms = 100;
            continue;
        }
        timeoutcountdown = 50;
        sleepms = 1;
        for ( lan_engine::media_stuff_s *d = w().first; d; d = d->next )
        {
            if ( nullptr == d->video_data )
                continue;

            u64 timelabel = d->v_msmonotonic;
            const uint8_t *y = (const uint8_t *)d->video_data;
            d->video_data = nullptr;
            d->v_msmonotonic = 0;

            ++d->locked;
            w.unlock(); // no need lock anymore

                        // encoding here
            int ysz = d->video_w * d->video_h;
                
            d->encode_video_and_send( timelabel, y, y + ysz, y + ( ysz + ysz / 4 ) );
            engine->hf->free_video_data( y );

            w = callstate.lock_write();
            --d->locked;
            if ( w().shutdown )
                return;

            bool ok = false;
            for ( media_stuff_s *dd = w().first; dd; dd = dd->next )
                if ( dd == d )
                {
                    ok = true;
                    break;
                }
            if ( !ok ) break;

        }
        if ( w.is_locked() ) w.unlock();
    }
}


lan_engine::contact_s::~contact_s()
{
    delete media;
    sendblocks.lock_write()().reset();
}

datablock_s *lan_engine::contact_s::build_block( block_type_e bt, u64 delivery_tag, const void *data, aint datasize, const void *data1, aint datasize1 )
{
    if ( delivery_tag == 0 )
    {
        auto r = engine->delivery.lock_read();
        delivery_tag = random64( [&]( u64 t )->bool { return r().find( t ) != r().end(); } );
    }

    engine->delivery.lock_write()( )[ delivery_tag ].reset();

    u64 temp;
    if ( data == nullptr )
    {
        randombytes_buf( &temp, sizeof( temp ) );
        data = &temp;
        datasize = sizeof( temp );
    }

    if ( BT_FILE_CHUNK == bt )
    {
        logfn( "filetr.log", "send_block %llu %i", delivery_tag, datasize );
    }

    return datablock_s::build( bt, delivery_tag, data, datasize, data1, datasize1 );
}

u64 lan_engine::contact_s::send_block(block_type_e bt, u64 delivery_tag, const void *data, aint datasize, const void *data1, aint datasize1)
{
    if (BT_AUDIO_FRAME == bt)
    {
        // special block

        bool is_auth = false;
        const byte *k = message_key(&is_auth);

        byte *sb = (byte *)_alloca( datasize + datasize1 );
        memcpy( sb, data, datasize );
        if (data1) memcpy( sb + datasize, data1, datasize1 );

        engine->pg_raw_data(k, bt, (const byte *)sb, datasize + datasize1);
        pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

        if ( IS_TLM( TLM_AUDIO_SEND_BYTES ) )
        {
            tlm_data_s d1 = { static_cast<u64>(id.id), static_cast<u64>( datasize + datasize1 ) };
            engine->hf->telemetry( TLM_AUDIO_SEND_BYTES, &d1, sizeof( d1 ) );
        }

        return 0;
    }

    return send_block( build_block(bt, delivery_tag, data, datasize, data1, datasize1) );
}

u64 lan_engine::contact_s::send_block( datablock_s *b )
{
    switch ( b->bt )
    {
    case BT_AUDIO_FRAME:
        if ( IS_TLM(TLM_AUDIO_SEND_BYTES) )
        {
            tlm_data_s d1 = { static_cast<u64>( id.id ), static_cast<u64>( b->len ) };
            engine->hf->telemetry( TLM_AUDIO_SEND_BYTES, &d1, sizeof( d1 ) );
        }
        break;
    case BT_VIDEO_FRAME:
        if ( IS_TLM(TLM_VIDEO_SEND_BYTES) )
        {
            tlm_data_s d1 = { static_cast<u64>( id.id ), static_cast<u64>( b->len ) };
            engine->hf->telemetry( TLM_VIDEO_SEND_BYTES, &d1, sizeof( d1 ) );
        }
        break;
    }

    sendblocks.lock_write()( ).add( b );
    if ( state == ONLINE )
        nextactiontime = time_ms();

    return b->delivery_tag;
}

bool lan_engine::contact_s::del_block( u64 utag )
{
    return sendblocks.lock_write()( ).del( utag );
}

void lan_engine::contact_s::calculate_pub_id( const byte *pk )
{
    memcpy(public_key, pk, SIZE_PUBLIC_KEY);
    make_raw_pub_id(raw_public_id, pk);
    public_id.clear().append_as_hex(raw_public_id, SIZE_PUBID);
}

enum chunks_e // HADR ORDER!!!!!!!!
{
    chunk_secret_key,
    chunk_contacts,

    chunk_contact,

    chunk_contact_id,
    chunk_contact_public_key,
    chunk_contact_name,
    chunk_contact_statusmsg,
    chunk_contact_state,
    chunk_contact_changedflags,

    chunk_contact_invitemsg,
    chunk_contact_tmpkey,
    chunk_contact_authkey,
    __unused__chunk_contact_sendmessages,

    __unused__chunk_contact_sendmessage,
    __unused__chunk_contact_sendmessage_type,
    __unused__chunk_contact_sendmessage_body,
    __unused__chunk_contact_sendmessage_utag,
    __unused__chunk_contact_sendmessage_createtime,

    chunk_magic,

    chunk_contact_avatar_hash,
    chunk_contact_avatar_tag,

    chunk_video_codec,
    chunk_video_quality,
    chunk_video_bitrate,
    chunk_video_telemetry,

};

//lan_engine::contact_s *contact;

void operator<<(chunk &chunkm, const lan_engine::contact_s &c)
{
    if (c.state == lan_engine::contact_s::ROTTEN) return;
    //contact = const_cast<lan_engine::contact_s *>(&c);

    chunk cc(chunkm.b, chunk_contact);

    chunk(chunkm.b, chunk_contact_id) << c.id;
    chunk(chunkm.b, chunk_contact_public_key) << bytes(c.public_key, SIZE_PUBLIC_KEY);
    chunk(chunkm.b, chunk_contact_name) << c.name;
    chunk(chunkm.b, chunk_contact_statusmsg) << c.statusmsg;
    chunk(chunkm.b, chunk_contact_state) << (i32)c.state;


    if (c.state == lan_engine::contact_s::SEARCH
        || c.state == lan_engine::contact_s::MEET
        || c.state == lan_engine::contact_s::INVITE_SEND
        || c.state == lan_engine::contact_s::INVITE_RECV)
    {
        chunk(chunkm.b, chunk_contact_invitemsg) << c.invitemessage;
    }

    if (c.state != lan_engine::contact_s::ONLINE && c.state != lan_engine::contact_s::OFFLINE && c.state != lan_engine::contact_s::BACKCONNECT)
    {
        chunk(chunkm.b, chunk_contact_tmpkey) << bytes(c.temporary_key, SIZE_KEY);

    }
    else if (!c.id.is_self())
    {
        chunk(chunkm.b, chunk_contact_authkey) << bytes(c.authorized_key + SIZE_KEY_NONCE_PART, SIZE_KEY_PASS_PART);

        log_auth_key("saved", c.authorized_key);
    }

    /*
    if (c.sendblock_f)
    chunk(chunkm.b, chunk_contact_sendmessages) << serlist<datablock_s>(c.sendblock_f);
    */

    chunk(chunkm.b, chunk_contact_changedflags) << (i32)c.changed_self;

    auto sbs = const_cast<lan_engine::contact_s &>(c).sendblocks.lock_write();
    for (datablock_s *m = sbs().sendblock_f; m; )
    {
        datablock_s *x = m; m = m->next;
        if (x->bt > __bt_no_need_ater_save_begin && x->bt < __bt_no_need_ater_save_end)
        {
            LIST_DEL(x, sbs().sendblock_f, sbs().sendblock_l, prev, next);
            x->die();
        }
    }
    sbs.unlock();

    chunk(chunkm.b, chunk_contact_avatar_tag) << c.avatar_tag;
    if (c.avatar_tag != 0)
    {
        chunk(chunkm.b, chunk_contact_avatar_hash) << bytes(c.avatar_hash, 16);
    }

}


void lan_engine::contact_s::fill_data(contact_data_s &cd, savebuffer *protodata)
{
    data_changed = false;

    cd.id = id;
    cd.mask = CDM_PUBID | CDM_NAME | CDM_STATUSMSG | CDM_STATE | CDM_ONLINE_STATE | CDM_GENDER | CDM_AVATAR_TAG | CDM_CAPS;
    cd.public_id = public_id.cstr();
    cd.public_id_len = (int)public_id.get_length();
    cd.name = name.cstr();
    cd.name_len = static_cast<int>(name.get_length());
    cd.status_message = statusmsg.cstr();
    cd.status_message_len = static_cast<int>(statusmsg.get_length());
    cd.avatar_tag = 0;
    cd.caps = CCAPS_SUPPORT_BBCODES | CCAPS_SUPPORT_SHARE_FOLDER;
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

    if (protodata)
    {
        chunk s(*protodata);
        s << *this;
        cd.data = protodata->data();
        cd.data_size = static_cast<int>(protodata->size());
        cd.mask |= CDM_DATA;
    }

}

void lan_engine::setup_communication_stuff()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    broadcast_trap.open();
    broadcast_seek.prepare();
    listen_port = tcp_in.open();
}

void lan_engine::shutdown_communication_stuff()
{
    listen_port = -1;
    tcp_in.close();
    broadcast_seek.close();
    broadcast_trap.close();
    WSACleanup();
}

lan_engine *lan_engine::engine = nullptr;
void lan_engine::create(host_functions_s *hf)
{
#pragma warning( disable: 4316 ) // 'lan_engine' : object allocated on the heap may not be aligned 16
    if (ASSERT(engine == nullptr))
        engine = new lan_engine(hf);
}

bool set_cfg_called = false;

lan_engine::lan_engine( host_functions_s *hf ):hf(hf),broadcast_trap(BROADCAST_PORT), broadcast_seek(BROADCAST_PORT), tcp_in(TCP_PORT)
{
    set_cfg_called = false;
    nexthallo = time_ms();
    //last_message = lasthallo;
    addnew()->state = contact_s::OFFLINE; /* create me */ 
};

lan_engine::~lan_engine()
{
    while (first)
        del(first);

    shutdown_communication_stuff();
}


void lan_engine::tick(int *sleep_time_ms)
{
    if ( fatal_error )
    {
        *sleep_time_ms = -1;
        return;
    }

    if (first->state != contact_s::ONLINE || listen_port < 0) 
    {
        *sleep_time_ms = 100;
        return;
    }
    *sleep_time_ms = first_ftr ? 0 : (10);
    media_data_transfer = false;
    bool reset_mastertag = true;

    contact_s *rotten = nullptr;
    int ct = time_ms();

    tick_ftr(ct);

    for (contact_s *c = first->next; c; c=c->next)
    {
        if (rotten == nullptr && c->state == contact_s::ROTTEN)
            rotten = c;

        c->changed_self |= changed_some;

        int deltat = (int)(ct - c->nextactiontime);
        if (deltat >= 0)
        {
            switch (c->state)
            {
            case contact_s::SEARCH:
                
                pg_search(listen_port, c->raw_public_id);
                if (!broadcast_seek.send(packet_buf_encoded, packet_buf_encoded_len, c->portshift))
                    fatal_error = true;
                ++c->portshift;
                if (c->portshift >= BROADCAST_RANGE)
                {
                    c->nextactiontime = ct + 5000;
                    c->portshift = 0;
                } else
                    c->nextactiontime = ct + 100;
                break;
            case contact_s::TRAPPED:
            case contact_s::ACCEPT_RESTORE_CONNECT:
                if (c->pipe.connected())
                {
                    if (c->key_sent)
                    {
                        MaskLog( LFLS_CLOSE, "c: %i, state: %i / key sent", c->id.id, c->state );

                        // oops. no invite still received
                        // it means no valid connection
                        c->pipe.close(); // goodbye connection

                    } else
                    {
                        pg_meet(c->public_key, c->temporary_key);
                        if (c->pipe.send(packet_buf_encoded, packet_buf_encoded_len))
                        {
                            c->key_sent = true;
                            c->nextactiontime = ct + 2000;
                        } else
                        {
                            MaskLog( LFLS_CLOSE, "c: %i, state: %i / key send fail", c->id.id, c->state );
                            c->pipe.close();
                        }
                    }

                } else
                {
                    if (c->key_sent)
                    {
                        ++c->reconnect;
                        c->key_sent = false;

                        if (c->reconnect > 10)
                        {
                            c->state = contact_s::ROTTEN;
                            if (rotten == nullptr) rotten = c;
                        }
                    }
                    if (c->state != contact_s::ROTTEN)
                    {
                        c->pipe.connect();
                        c->nextactiontime = ct + 2000;
                        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / connect / time: %i", c->id.id, c->state, c->nextactiontime);
                    }
                }
                break;
            case contact_s::MEET:
                if (c->pipe.connected())
                {
                    pg_invite(first->name, c->invitemessage, c->temporary_key);
                    bool ok = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                    if (ok)
                    {
                        c->state = contact_s::INVITE_SEND;
                        c->data_changed = true;
                    } else
                    {
                        MaskLog( LFLS_CLOSE, "c: %i, state: %i / invite send fail", c->id.id, c->state );
                        c->pipe.close();
                        c->nextactiontime = ct;
                    }

                } else
                {
                    c->state = contact_s::SEARCH;
                    c->nextactiontime = ct;
                }
                break;
            case contact_s::BACKCONNECT:
                if (c->pipe.connected())
                {
                    if (c->key_sent)
                    {
                        MaskLog( LFLS_CLOSE, "c: %i, state: %i / key sent / time: %i", c->id.id, c->state, c->nextactiontime );
                        c->pipe.close();
                    } else
                    {
                        randombytes_buf(c->authorized_key, SIZE_KEY_NONCE_PART); // rebuild nonce

                        pg_nonce(c->public_key, c->authorized_key);
                        if (c->pipe.send(packet_buf_encoded, packet_buf_encoded_len))
                        {
                            c->key_sent = true;
                            c->nextactiontime = ct + 5000; // waiting PID_READY in 5 seconds, then disconnect

                            MaskLog( LFLS_ESTBLSH, "c: %i, state: %i / key sent ok / time: %i", c->id.id, c->state, c->nextactiontime );

                        } else
                        {
                            MaskLog( LFLS_CLOSE, "c: %i, state: %i / key send fail", c->id.id, c->state );
                            c->pipe.close();
                            c->nextactiontime = ct;
                        }
                    }

                } else
                {
                    MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / not connected", c->id.id, c->state);

                    if (c->key_sent)
                    {
                        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / ++reconnect", c->id.id, c->state);

                        ++c->reconnect;
                        c->key_sent = false;

                        if (c->reconnect > 10)
                        {
                            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / max reconnect", c->id.id, c->state);
                            c->state = contact_s::OFFLINE;
                        }
                    }

                    if (c->state != contact_s::OFFLINE)
                    {
                        // just connect
                        c->pipe.connect();
                        c->nextactiontime = ct + 2000;

                        MaskLog( LFLS_ESTBLSH, "c: %i, state: %i / connect / time: %i", c->id.id, c->state, c->nextactiontime );
                    }
                }
                break;
            case contact_s::INVITE_SEND:
                if (c->pipe.connected())
                {
                    // do nothing. waiting accept or reject
                    c->online_tick(ct, 1100);
                } else
                {
                    // connection lost :(
                    c->state = contact_s::SEARCH;
                    c->nextactiontime = ct;
                }

                break;
            case contact_s::INVITE_RECV:
                if (c->pipe.connected())
                {
                    c->online_tick(ct, 1100);
                }
                break;
            case contact_s::ACCEPT:
                // send accept and kill contact
                if (c->pipe.connected())
                {
                    if (!c->key_sent)
                    {
                        randombytes_buf(c->authorized_key, sizeof(c->authorized_key));

                        log_auth_key( "before send accept", c->authorized_key );

                        pg_accept(first->name, c->authorized_key, c->temporary_key);
                        c->key_sent = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                    }
                    c->nextactiontime = ct + 5000;
                } else
                {
                    // just wait
                    c->nextactiontime = ct + 10000;
                    c->key_sent = false;
                }
                break;
            case contact_s::REJECT:
                // send reject and kill contact
                if (c->pipe.connected())
                {
                    pg_reject();
                    c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);
                    c->state = contact_s::ALMOST_ROTTEN;
                    c->nextactiontime = ct + 5000;
                } else
                {
                    c->state = contact_s::ROTTEN;
                    c->nextactiontime = ct;
                    if (rotten == nullptr) rotten = c;
                }
                c->data_changed = true;
                break;
            case contact_s::ONLINE:
                if (c->pipe.connected())
                {
                    c->online_tick(ct);

                } else if ( (ct - c->nextactiontime) > 10000 )
                {
                    c->to_offline(ct);
                }
                break;
            case contact_s::ALMOST_ROTTEN:
                if (c->pipe.connected())
                {
                    MaskLog(LFLS_CLOSE, "c: %i, state: %i / almost rotten", c->id.id, c->state);
                    c->pipe.close();
                }
                break;
            }
            
        }

        bool need_some_hallo = false;
        if (c->state == contact_s::OFFLINE)
        {
            reset_mastertag = false;
            if (!c->pipe.connected())
                need_some_hallo = true;
        }

        c->recv();

        if (!c->pipe.connected())
        {
            if (c->state == contact_s::ONLINE)
                c->to_offline( ct );
            else if (c->state == contact_s::ALMOST_ROTTEN)
            {
                c->state = contact_s::ROTTEN;
                c->nextactiontime = ct;
                c->data_changed = true;
            }
        }
        
        if (need_some_hallo)
        {
            if ((ct - nexthallo) > 0)
            {
                nexthallo = ct + randombytes_uniform( 10000 ) + 5000;
                // one hallo per 10 sec

                if ((ct - nextmastertaggeneration) > 0)
                    my_mastertag = 0, nextmastertaggeneration = ct + randombytes_uniform( 10000 ) + 5000;

                pg_hallo(listen_port);
                for(int i=0;i<BROADCAST_RANGE;++i)
                    broadcast_seek.send(packet_buf_encoded, packet_buf_encoded_len, i);

            }
        }
    }

    if (reset_mastertag)
        my_mastertag = 0;

    changed_some = 0;

    if (rotten)
    {
        ASSERT(!rotten->data_changed, "sure \'rotten\' state was sent to host");
        del(rotten);
        hf->save();
    }

    if (first->data_changed) // send self status
    {
        contact_data_s cd(contact_id_s(), 0);
        first->fill_data(cd, nullptr);
        hf->update_contact(&cd);
    }

    // broadcast receive
    sockaddr_in addr;
    packet_buf_encoded_len = broadcast_trap.read(packet_buf_encoded,512,addr);
    if ( packet_buf_encoded_len >= 2 )
    {
        decode();

        stream_reader reader(2, packet_buf, packet_buf_len);

        packet_id_e pid = (packet_id_e)ntohs(*(short *)packet_buf);
        switch (pid)
        {
            case PID_SEARCH:
            {
                USHORT version = reader.readus(0);
                if (version != LAN_PROTOCOL_VERSION)
                    break;
                USHORT back_port = reader.readus(0);
                const byte *trapped_contact_public_key = reader.read(SIZE_PUBLIC_KEY);
                const byte *seeking_raw_public_id = reader.read(SIZE_PUBID);
                if (ASSERT(trapped_contact_public_key && seeking_raw_public_id && reader.last() == 0))
                    pp_search( addr.sin_addr.S_un.S_addr, back_port, trapped_contact_public_key, seeking_raw_public_id );
                break;
            }
            case PID_HALLO:
            {
                USHORT version = reader.readus(0);
                if (version != LAN_PROTOCOL_VERSION)
                    break;
                USHORT back_port = reader.readus(0);
                int mastertag = reader.readi(0);
                const byte *hallo_contact_public_key = reader.read(SIZE_PUBLIC_KEY);
                if (ASSERT(hallo_contact_public_key && reader.last() == 0))
                    pp_hallo(addr.sin_addr.S_un.S_addr, back_port, mastertag, hallo_contact_public_key);
                break;
            }
        }
    }

    // incoming connection
    tcp_pipe *pipe = nullptr;
    if (tcp_in.accepted.try_pop(pipe))
    {
        switch (pipe->packet_id())
        {
        case PID_MEET:
            MaskLog(LFLS_ESTBLSH, "accepted meet %i", pipe->_socket);
            pipe = pp_meet(pipe, stream_reader(pipe->rcvbuf, pipe->rcvbuf_sz));
            break;
        case PID_NONCE:
            MaskLog(LFLS_ESTBLSH, "accepted nonce %i", pipe->_socket);
            pipe = pp_nonce(pipe, stream_reader(pipe->rcvbuf, pipe->rcvbuf_sz));
            break;
        case PID_NONE:
            // not yet received
            if (pipe->timeout()) break;

            tcp_in.accepted.push(pipe); // push to queue
            pipe = nullptr;
            break;
        }

        delete pipe;
    }

    if (media_data_transfer)
        *sleep_time_ms = 0;

    if (fatal_error)
        *sleep_time_ms = -1;

    time_t t = now();
    if ( ( t - last_activity ) > 3600 )
        fatal_error = true;

}

void lan_engine::pp_search( unsigned int IPv4, int back_port, const byte *trapped_contact_public_key, const byte *seeking_raw_public_id )
{
    if ( 0 != memcmp(seeking_raw_public_id, first->raw_public_id, SIZE_PUBID) ) return; // do nothin
    contact_s *c = find_by_pk( trapped_contact_public_key ); // may be already trapped
    if (!c) c = addnew(); // no. 1st search
    else if (c->pipe.connected()) return; // already connected - ignore. we think it is parasite packet
    if (c->state == contact_s::ACCEPT)
        c->state = contact_s::ACCEPT_RESTORE_CONNECT;
    else
        c->state = contact_s::TRAPPED;
    c->key_sent = false;
    c->reconnect = 0;
    c->calculate_pub_id(trapped_contact_public_key);
    c->pipe.set_address(IPv4, back_port);
    randombytes_buf(c->temporary_key, SIZE_KEY);
}

/*
    handle broadcast udp PID_HALLO packet
    mastertag - mastertag of broadcaster
*/
void lan_engine::pp_hallo( unsigned int IPv4, int back_port, int mastertag, const byte *hallo_contact_public_key )
{
    if (my_mastertag == 0 || my_mastertag < mastertag)
        return; // just ignore this packet: my_mastertag == 0 - means no offline contacts here - nothing to connect / my_mastertag < mastertag - means concurent connection fail (my broadcast PID_HALLO will be processed)

    if (contact_s *c = find_by_pk( hallo_contact_public_key ))
    {
        // yo! this guy is in my contact list!
        if (!c->pipe.connected() && c->state == contact_s::OFFLINE)
        {
            // only offline (authorized) contacts can be initialized with PID_HALLO packet
            c->state = contact_s::BACKCONNECT;
            c->key_sent = false;
            c->reconnect = 0;
            c->pipe.set_address(IPv4, back_port);
            c->reconnect = 0;
            c->nextactiontime = time_ms();

            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / on hallo from: %i.%i.%i.%i", c->id.id, c->state, (int)(IPv4&0xff), (int)((IPv4>>8)&0xff), (int)((IPv4>>16)&0xff), (int)((IPv4>>24)&0xff));
        }
    }
}

tcp_pipe * lan_engine::pp_meet( tcp_pipe * pipe, stream_reader &&r )
{
    if (r.end())
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const byte *meet_public_key = r.read(SIZE_PUBLIC_KEY);
    if (!meet_public_key)
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    byte pubid[SIZE_PUBID];
    make_raw_pub_id(pubid, meet_public_key);

    contact_s *c = find_by_rpid(pubid);
    if (c == nullptr || (c->state != contact_s::SEARCH && c->state != contact_s::INVITE_SEND))
    {
        // only way to handle this packet - send broadcast udp PID_SEARCH packet and get incoming tcp connection
        // so, no client, no PID_SEARCH
        return pipe;
    }


    const byte *nonce = r.read(crypto_box_NONCEBYTES);
    if (!nonce) 
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int cipher_len = SIZE_KEY + crypto_box_MACBYTES;

    const byte *cipher = r.read(cipher_len);
    if (!cipher)
    {
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int decoded_size = cipher_len - crypto_box_MACBYTES;
    static_assert(decoded_size == SIZE_KEY, "check size");

    if (crypto_box_open_easy(c->temporary_key, cipher, cipher_len, nonce, meet_public_key, my_secret_key) != 0)
        return pipe;

    memcpy(c->public_key, meet_public_key, SIZE_PUBLIC_KEY);
    c->pipe = std::move( *pipe );
    c->state = contact_s::MEET;
    c->nextactiontime = time_ms();
    c->pipe.cpdone();

    return pipe;
}

tcp_pipe *lan_engine::pp_nonce(tcp_pipe * pipe, stream_reader &&r)
{
    //logm("pp_nonce (rcvd) ==============================================================");
    //log_bytes("my_public_key", my_public_key, SIZE_PUBLIC_KEY);

    if (r.end())
    {
        MaskLog(LFLS_ESTBLSH, "nonce no data");

        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const byte *peer_public_key = r.read(SIZE_PUBLIC_KEY);
    if (!peer_public_key)
    {
        MaskLog(LFLS_ESTBLSH, "nonce no pub key");

        tcp_in.accepted.push(pipe);
        return nullptr;
    }
   
    byte pubid[SIZE_PUBID];
    make_raw_pub_id(pubid, peer_public_key);

    contact_s *c = find_by_rpid(pubid);

    MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / nonce", (c ? c->id.id : -1), (c ? c->state : -1));

    if (c == nullptr || (c->state != contact_s::OFFLINE))
    {
        if (c && c->state == contact_s::BACKCONNECT && 0 > memcmp(c->public_key, my_public_key, SIZE_PUBLIC_KEY))
        {
            // race condition :(
            // create new pipe instead of backconnect one
            c->pipe.close();
            c->state = contact_s::OFFLINE;
            c->key_sent = false;

            MaskLog(LFLS_CLOSE, "c: %i, state: %i / race condition win", c->id.id, c->state);

        } else
        {
            // contact is not offline or not found
            // or race condition loose
            // disconnect

            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / race condition loose", (c ? c->id.id : -1), (c ? c->state : -1));
            return pipe;
        }
    }

    MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / concurrent ok", c->id.id, c->state);

    //log_auth_key("c auth_key", c->authorized_key);
    //log_bytes("c public_key", c->public_key, SIZE_PUBLIC_KEY);

    const byte *nonce = r.read(crypto_box_NONCEBYTES);
    if (!nonce)
    {
        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / no nonce yet", c->id.id, c->state);
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    //log_bytes("nonce (rcvd)", nonce, crypto_box_NONCEBYTES);

    const int cipher_len = SIZE_KEY_NONCE_PART + crypto_box_MACBYTES;

    const byte *cipher = r.read(cipher_len);
    if (!cipher)
    {
        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / no cipher yet", c->id.id, c->state);
        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    const int decoded_size = cipher_len - crypto_box_MACBYTES;
    static_assert(decoded_size == SIZE_KEY_NONCE_PART, "check size");

    if (crypto_box_open_easy(c->authorized_key, cipher, cipher_len, nonce, peer_public_key, my_secret_key) != 0)
    {
        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / cipher cannot be decrypted", c->id.id, c->state);
        return pipe;
    }

    //log_auth_key( "authkey decrypted", c->authorized_key );

    const byte *rhash = r.read(crypto_generichash_BYTES);
    if (!rhash)
    {
        MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / no hash yet", c->id.id, c->state);

        tcp_in.accepted.push(pipe);
        return nullptr;
    }

    //log_bytes("hash (rcvd)", rhash, crypto_generichash_BYTES);

    byte hash[crypto_generichash_BYTES];
    crypto_generichash(hash, sizeof(hash), c->authorized_key, SIZE_KEY, nullptr, 0);

    //log_bytes("hash of authkey", hash, crypto_generichash_BYTES);

    if ( 0 == memcmp(hash, rhash, crypto_generichash_BYTES) )
    {
        if (ASSERT(0 == memcmp(c->public_key, peer_public_key, SIZE_PUBLIC_KEY)))
        {
            c->pipe = std::move(*pipe);
            c->state = contact_s::ONLINE;
            c->data_changed = true;
            c->nextactiontime = time_ms();
            c->pipe.cpdone();
            c->changed_self = -1;
            c->client.clear();

            if ( !r.end() )
                c->client = r.reads();

            pg_ready(c->raw_public_id, c->authorized_key);
            bool send_ready = c->pipe.send(packet_buf_encoded, packet_buf_encoded_len);

            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / send ready: %s", c->id.id, c->state, (send_ready ? "yes" : "no"));
        } else
        {
            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / fail public key", c->id.id, c->state);
        }
    } else
    {
        // authorized_key is damaged
        c->invitemessage.set( STD_ASTR("\1restorekey") );
        c->state = contact_s::SEARCH;
        c->data_changed = true;
    }


    return pipe;
}

void lan_engine::stop_encoder()
{
    int st = time_ms();

    auto wh = callstate.lock_write();
    wh().video_encoder_heartbeat = false;
    wh.unlock();

    while ( callstate.lock_read()( ).senders() )
    {
        callstate.lock_write()( ).shutdown = true;

        Sleep( 1 );

        if ( ( time_ms() - st ) > 3000 )
        {
            // 3 seconds still waiting senders?
            // something wrong
            auto w = callstate.lock_write();

            if ( !w().video_encoder_heartbeat )
                w().video_encoder = false, fatal_error = true;
            w().video_encoder_heartbeat = false;

            st = time_ms();
        }
    }

    callstate.lock_write()( ).shutdown = false;

}

void lan_engine::send_configurable()
{
    const char * fields[adv_count] = { 
#define ASI(aa) #aa,
        ADVSET
#undef ASI
    };

    std::string svalues[adv_count];
    const char * values[adv_count];

    static_assert(ARRAY_SIZE(fields) == ARRAY_SIZE(values) && ARRAY_SIZE(values) == ARRAY_SIZE(svalues), "check len");

    int i = 0;
#define ASI(aa) svalues[i++] = adv_##aa();
    ADVSET
#undef ASI

    i = 0;
    for (const std::string &s : svalues) values[i++] = s.cstr();

    hf->configurable(ARRAY_SIZE(fields), fields, values);
}


void lan_engine::signal(contact_id_s id, signal_e s)
{
    switch (s)
    {
    case APPS_INIT_DONE:
        {
            // send contacts to host
            savebuffer sb;
            contact_data_s cd(contact_id_s(), 0);
            for (contact_s *c = first; c; c = c->next)
            {
                sb.clear();
                c->fill_data(cd, c == first ? nullptr : &sb);
                hf->update_contact(&cd);
            }
        }
        break;
    case APPS_ONLINE:
        if (first->state == contact_s::OFFLINE)
            setup_communication_stuff();

        if (broadcast_trap.ready())
        {
            hf->connection_bits(CB_ENCRYPTED | CB_TRUSTED);

            first->state = contact_s::ONLINE;
            first->data_changed = true;

            contact_data_s cd(contact_id_s(), 0);
            first->fill_data(cd, nullptr);
            hf->update_contact(&cd);

        }
        break;
    case APPS_OFFLINE:
        stop_encoder();

        if (first->state == contact_s::ONLINE)
        {
            first->state = contact_s::OFFLINE;

            contact_data_s cd(contact_id_s(), 0);
            first->fill_data(cd, nullptr);
            hf->update_contact(&cd);

            savebuffer sb;
            for (contact_s *c = first->next; c; c = c->next)
            {
                if (c->pipe.connected())
                {
                    MaskLog(LFLS_CLOSE, "c: %i, state: %i / offline", c->id.id, c->state);
                    c->pipe.flush_and_close();
                }
                if (c->state == contact_s::ONLINE || c->state == contact_s::BACKCONNECT)
                    c->state = contact_s::OFFLINE;
                if (c->state == contact_s::INVITE_SEND || c->state == contact_s::MEET)
                    c->state = contact_s::SEARCH;
                sb.clear();
                c->fill_data(cd, c == first ? nullptr : &sb);
                hf->update_contact(&cd);
            }

            shutdown_communication_stuff();
        }
        break;
    case APPS_GOODBYE:
        stop_encoder();
        set_cfg_called = false;
        delete this;
        engine = nullptr;
        break;

    case CONS_ACCEPT_INVITE:
        if (contact_s *c = find(id))
            if (c->state == contact_s::INVITE_RECV)
            {
                c->state = contact_s::ACCEPT;
                c->key_sent = false;
                c->invitemessage.clear();
                hf->save();
            }

        break;
    case CONS_REJECT_INVITE:
        if (contact_s *c = find(id))
            if (c->state == contact_s::INVITE_RECV)
            {
                c->state = contact_s::REJECT;
                c->nextactiontime = time_ms();
            }
        break;
    case CONS_ACCEPT_CALL:
        if (contact_s *c = find(id))
            if (c->state == contact_s::ONLINE && contact_s::IN_CALL == c->call_status)
            {
                c->send_block(BT_CALL_ACCEPT, 0);
                c->start_media();
            }

        break;
    case CONS_STOP_CALL:
        if (contact_s *c = find(id))
            if (c->state == contact_s::ONLINE && contact_s::CALL_OFF != c->call_status)
            {
                c->send_block(BT_CALL_CANCEL, 0);
                c->stop_call_activity(false);

            }
        break;
    case CONS_DELETE:

        if (!id.is_self())
            if (contact_s *c = find(id))
                if (c->state != contact_s::ROTTEN && c->state != contact_s::ALMOST_ROTTEN && c->state != contact_s::REJECT)
                {
                    c->state = contact_s::ROTTEN;
                    c->data_changed = false; // no need to send data to host
                }

        break;
    case CONS_TYPING:
        if (contact_s *c = find(id))
            c->send_block(BT_TYPING, 0);

        break;

    case REQS_DETAILS:
        break;
    case REQS_AVATAR:
        if (contact_s *c = find(id))
            c->send_block(BT_GETAVATAR, 0);
        break;
    case REQS_EXPORT_DATA:
        break;

    }
}

void lan_engine::set_name(const char*utf8name)
{
    first->name = utf8name;
    changed_some |= CDM_NAME;
}
void lan_engine::set_statusmsg(const char*utf8status)
{
    first->statusmsg = utf8status;
    changed_some |= CDM_STATUSMSG;
}

void lan_engine::set_ostate(int ostate)
{
    first->ostate = (contact_online_state_e)ostate;
    changed_some |= CDM_ONLINE_STATE;
}
void lan_engine::set_gender(int gender)
{
    first->gender = (contact_gender_e)gender;
    changed_some |= CDM_GENDER;
}

void lan_engine::set_avatar(const void*data, int isz)
{
    byte hash[AVATAR_HASH_SIZE];
    bool avachange = ((int)avatar.size() != isz);
    if (!avachange)
    {
        if (isz)
        {
            crypto_generichash( hash, sizeof( hash ), (const byte *)data, isz, nullptr, 0 );
            if (0 != memcmp(first->avatar_hash, hash, sizeof( hash ) ))
                avachange = true;
        }
    } else if (isz)
    {
        crypto_generichash( hash, sizeof( hash ), (const byte *)data, isz, nullptr, 0 );
    }
    if (avachange)
    {
        if (isz)
        {
            memcpy( first->avatar_hash, hash, sizeof( hash ) );
            avatar.resize(isz);
            memcpy(avatar.data(), data, isz);
        } else
        {
            memset(first->avatar_hash, 0, AVATAR_HASH_SIZE );
            avatar.clear();
        }
        changed_some |= CDM_AVATAR_TAG;
        avatar_set = true;
    }
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

bool lan_engine::load_contact(contact_id_s cid, loader &l)
{
    bool loaded = false;
    if (int contact_size = l(chunk_contact))
    {
        contact_s *c = nullptr;

        loader lc(l.chunkdata(), contact_size);
        if (lc(chunk_contact_id))
        {
            contact_id_s id = as_contactid(lc.get_u32());
            ASSERT(cid.is_empty() || cid == id);
            hf->use_id(id.id);

            if (int pksz = lc(chunk_contact_public_key))
            {
                loader pkl(lc.chunkdata(), pksz);
                int dsz;
                if (const byte *pk = (const byte *)pkl.get_data(dsz))
                    if (ASSERT(dsz == SIZE_PUBLIC_KEY))
                    {
                        c = first ? find_by_pk( pk ) : nullptr;
                        if (c)
                        {
                            ASSERT(c->id == id);

                        } else
                        {
                            c = addnew(id);
                        }

                        loaded = true;
                        c->calculate_pub_id(pk);
                        if (c->id.is_self()) memcpy(my_public_key, c->public_key, SIZE_PUBLIC_KEY);
                    }
            }
        }

        if (c)
        {
            if (lc(chunk_contact_name))
                c->name.set(lc.get_astr());

            if (lc(chunk_contact_statusmsg))
                c->statusmsg.set(lc.get_astr());

            if (lc(chunk_contact_state))
                c->state = (decltype(c->state))lc.get_i32();

            if (c->state == contact_s::SEARCH && c->id.is_self())
                c->state = contact_s::OFFLINE;

            if (c->state == contact_s::ONLINE || c->state == contact_s::BACKCONNECT)
                c->state = contact_s::OFFLINE;

            if (c->state == contact_s::INVITE_SEND || c->state == contact_s::MEET)
                c->state = contact_s::SEARCH; // if contact not yet authorized - initial search is only valid way to establish connection

            if (c->state == lan_engine::contact_s::SEARCH
                || c->state == lan_engine::contact_s::MEET
                || c->state == lan_engine::contact_s::INVITE_SEND
                || c->state == lan_engine::contact_s::INVITE_RECV)
                if (lc(chunk_contact_invitemsg))
                    c->invitemessage.set(lc.get_astr());

            if (c->state != lan_engine::contact_s::ONLINE && c->state != lan_engine::contact_s::OFFLINE)
            {
                if (int tksz = lc(chunk_contact_tmpkey))
                {
                    loader tkl(lc.chunkdata(), tksz);
                    int dsz;
                    if (const void *tk = tkl.get_data(dsz))
                        if (ASSERT(dsz == SIZE_KEY))
                            memcpy(c->temporary_key, tk, SIZE_KEY);
                }

            }
            else if (!c->id.is_self())
            {
                randombytes_buf(c->authorized_key, sizeof(c->authorized_key));

                if (int tksz = lc(chunk_contact_authkey))
                {
                    loader tkl(lc.chunkdata(), tksz);
                    int dsz;
                    if (const void *tk = tkl.get_data(dsz))
                        if (ASSERT(dsz == SIZE_KEY_PASS_PART))
                            memcpy(c->authorized_key + SIZE_KEY_NONCE_PART, tk, SIZE_KEY_PASS_PART);
                }

                log_auth_key("loaded", c->authorized_key);
            }

            if (lc(chunk_contact_changedflags))
                c->changed_self = lc.get_i32();

            if (lc(chunk_contact_avatar_tag))
                c->avatar_tag = lc.get_i32();

            if (c->avatar_tag != 0)
            {
                if (int bsz = lc(chunk_contact_avatar_hash))
                {
                    loader hbl(lc.chunkdata(), bsz);
                    int dsz;
                    if (const void *mb = hbl.get_data(dsz))
                        memcpy(c->avatar_hash, mb, min(dsz, 16));
                }
            }

        }
    }
    return loaded;
}

void lan_engine::adv_video_codec(const std::pstr_c &val)
{
    if (val.equals(STD_ASTR("vp8")))
        use_vcodec = vc_vp8;
    if (val.equals(STD_ASTR("vp9")))
        use_vcodec = vc_vp9;
}

std::string lan_engine::adv_video_codec() const
{
    switch (use_vcodec)
    {
    case vc_vp9:
        return std::string(STD_ASTR("vp9"));
    }

    return std::string( STD_ASTR("vp8") );
}

void lan_engine::adv_video_bitrate(const std::pstr_c &val)
{
    use_vbitrate = val.as_int();
}
std::string lan_engine::adv_video_bitrate() const
{
    std::string s;
    s.set_as_int(use_vbitrate);
    return s;
}

void lan_engine::adv_video_quality(const std::pstr_c &val)
{
    use_vquality = val.as_int();
}

std::string lan_engine::adv_video_quality() const
{
    std::string s;
    s.set_as_int(use_vquality);
    return s;
}

void lan_engine::adv_video_telemetry(const std::pstr_c &val)
{
    tlmflags = val.as_int() != 0? -1 : 0;
}
std::string lan_engine::adv_video_telemetry() const
{
    std::string s( tlmflags ? STD_ASTR("1") : STD_ASTR("0") );
    return s;
}


void lan_engine::set_config(const void*data, int isz)
{
    if ( isz < 4 ) return;
    config_accessor_s ca( data, isz );

    bool setproto = false;

    auto parsev = [&] ( const std::pstr_c &field, const std::pstr_c &val )
    {
#define ASI(aa) if (field.equals(STD_ASTR(#aa))) { adv_##aa(val); return; }
        ADVSET
#undef ASI

        if ( field.equals( STD_ASTR( CFGF_SETPROTO ) ) )
        {
            setproto = val.as_int() != 0;
            return;
        }
    };

    if ( ca.params.l )
        parse_values( ca.params, parsev ); // parse params

    if ( !setproto && !ca.native_data && !ca.protocol_data )
        return;

    bool loaded = false;
    set_cfg_called = true;

    loader ldr(ca.protocol_data,ca.protocol_data_len);

    if (int sz = ldr(chunk_secret_key))
    {
        loader l( ldr.chunkdata(), sz );
        int dsz;
        if (const void *sk = l.get_data(dsz))
            if (ASSERT(dsz == SIZE_SECRET_KEY))
                memcpy( my_secret_key, sk, SIZE_SECRET_KEY );
    }

    while (first)
        del(first);

    if (int sz = ldr(chunk_contacts))
    {
        loader l( ldr.chunkdata(), sz );
        for (int cnt = l.read_list_size(); cnt > 0; --cnt)
            loaded |= load_contact( contact_id_s(), l );
    }

    if (ldr(chunk_video_codec))
        use_vcodec = ldr.get_i32() != 0 ? vc_vp9 : vc_vp8;

    if (ldr(chunk_video_quality))
        use_vquality = ldr.get_i32();

    if (ldr(chunk_video_bitrate))
        use_vbitrate = ldr.get_i32();

    if (ldr(chunk_video_telemetry))
        tlmflags = ldr.get_i32();

    if (!loaded)
    {
        // setup default
        if (!first)
            first = addnew(contact_id_s::make_self());

        crypto_box_keypair(my_public_key, my_secret_key);
        first->calculate_pub_id( my_public_key );
    }

    hf->operation_result( LOP_SETCONFIG, CR_OK );
    send_configurable();
}

void lan_engine::save_config(void *param)
{
    if (engine && set_cfg_called)
    {
        savebuffer b;
        chunk(b, chunk_magic) << (u64)(0x555BADF00D2C0FE6ull + LAN_SAVE_VERSION);
        chunk(b, chunk_secret_key) << bytes(my_secret_key, SIZE_SECRET_KEY);
        chunk(b, chunk_contacts) << serlist<contact_s, nonext<contact_s> >(first);

        chunk(b, chunk_video_codec) << static_cast<i32>(use_vcodec == vc_vp8 ? 0 : 1);
        chunk(b, chunk_video_quality) << static_cast<i32>(use_vquality);
        chunk(b, chunk_video_bitrate) << static_cast<i32>(use_vbitrate);
        chunk(b, chunk_video_telemetry) << static_cast<i32>(tlmflags);

        hf->on_save(b.data(), (int)b.size(), param);
    }
}

int lan_engine::resend_request(contact_id_s id, const char* invite_message_utf8)
{
    if (contact_s *c = find(id))
        return add_contact( c->public_id, invite_message_utf8 );
    return CR_FUNCTION_NOT_FOUND;
}

int lan_engine::add_contact(const char* public_id, const char* invite_message_utf8)
{
    std::string pubid( public_id );
    byte raw_pub_id[SIZE_PUBID];
    make_raw_pub_id(raw_pub_id,pubid.as_sptr());
    if (!check_pubid_valid(raw_pub_id))
        return CR_INVALID_PUB_ID;

    contact_s *c = find_by_rpid(raw_pub_id);
    if (c) 
    {
        if (c->state != contact_s::REJECTED && c->state != contact_s::INVITE_SEND)
            return CR_ALREADY_PRESENT;
    } else
    {
        c = addnew();
        c->public_id = pubid;
        make_raw_pub_id(c->raw_public_id, c->public_id);
    }
    c->invitemessage.set( utf8clamp(std::asptr( invite_message_utf8 ), SIZE_MAX_FRIEND_REQUEST_BYTES) );
    c->state = contact_s::SEARCH;
    if (c->pipe.connected()) c->pipe.close();

    contact_data_s cd(contact_id_s(), 0);
    savebuffer sb;
    c->fill_data(cd,&sb);
    hf->update_contact(&cd);
    return CR_OK;
}

lan_engine::contact_s *lan_engine::find_by_pk(const byte *public_key)
{
    for (contact_s *i = first->next; i; i = i->next)
        if (i->state != contact_s::ROTTEN)
            if (0 ==memcmp(i->public_key, public_key, SIZE_PUBLIC_KEY))
                return i;
    return nullptr;
}

lan_engine::contact_s *lan_engine::find_by_rpid(const byte *raw_public_id)
{
    for (contact_s *i = first->next; i; i = i->next)
        if (i->state != contact_s::ROTTEN)
            if (0 == memcmp(i->raw_public_id, raw_public_id, SIZE_PUBID))
                return i;
    return nullptr;
}

lan_engine::contact_s *lan_engine::addnew(contact_id_s iid)
{
    contact_id_s id = iid;
    if (id.is_empty())
    {
        int x = hf->find_free_id();
        id = contact_id_s(contact_id_s::CONTACT, x);
    }
    contact_s *nc = new contact_s( id );
    LIST_ADD(nc,first,last,prev,next);
    return nc;
}

void lan_engine::del(contact_s *c)
{
    LIST_DEL(c, first, last, prev, next);
    delete c;
}

void lan_engine::send_message(contact_id_s id, const message_s *msg)
{
    if (contact_s *c = find(id))
        c->send_message( msg->crtime, std::asptr(msg->message, msg->message_len), msg->utag);
}

void lan_engine::del_message( u64 utag )
{
    for (contact_s *i = first->next; i; i = i->next)
        if (i->del_block(utag))
        {
            hf->save();
            break;
        }
}

void lan_engine::call(contact_id_s id, const call_prm_s *callinf)
{
    if (contact_s *c = find(id))
        if (c->state == contact_s::ONLINE && c->call_status == contact_s::CALL_OFF)
        {
            c->send_block( BT_CALL /*callinf->call_type ==  call_prm_s::VOICE_CALL ? BT_VOICE_CALL : BT_VIDEO_CALL*/, 0);
            c->call_status = contact_s::OUT_CALL;
            c->call_stop_time = time_ms() + (callinf->duration * 1000);
        }
}

void lan_engine::stream_options(contact_id_s id, const stream_options_s *so )
{
    if ( contact_s *c = find( id ) )
        if ( c->state == contact_s::ONLINE )
        {
            if ( c->media == nullptr ) c->media = new media_stuff_s(c);

            if ( c->media->local_so.options != so->options || so->view_w != c->media->local_so.view_w || so->view_h != c->media->local_so.view_h )
            {
                c->media->local_so.options = so->options;
                c->media->local_so.view_w = so->view_w;
                c->media->local_so.view_h = so->view_h;

                if ( c->call_status == contact_s::IN_PROGRESS )
                {
                    stream_options_s so2s;
                    so2s.options = htonl( so->options );
                    so2s.view_w = htonl( so->view_w );
                    so2s.view_h = htonl( so->view_h );

                    c->send_block( BT_STREAM_OPTIONS, 0, &so2s, sizeof( so2s ) );
                }

                //if ( 0 == ( c->media->local_so.options & SO_RECEIVING_VIDEO ) )
                //    c->media->current_recv_frame = -1;

                if ( c->media->local_so.view_w < 0 || c->media->local_so.view_h < 0 )
                {
                    c->media->local_so.view_w = 0;
                    c->media->local_so.view_h = 0;
                }
            }
        }
}

int lan_engine::send_av(contact_id_s id, const call_prm_s * ci)
{
    contact_s *c = nullptr;
    if ( ci->audio_data )
    {
        c = find( id );
        if ( !c ) return SEND_AV_OK;

        if (c->state == contact_s::ONLINE && contact_s::IN_PROGRESS == c->call_status && c->media)
            c->media->add_audio( ci->ms_monotonic, ci->audio_data, ci->audio_data_size );
    }
    if ( ci->video_data && ci->fmt == VFMT_I420 )
    {
        if (!c)
            c = find( id );
        if ( !c || !c->media ) return SEND_AV_OK;

        if ( 0 != ( c->media->remote_so.options & SO_RECEIVING_VIDEO ) )
        {
            if ( c->media->video_data )
                engine->hf->free_video_data( c->media->video_data );

            c->media->video_w = ci->w;
            c->media->video_h = ci->h;
            c->media->video_data = ci->video_data;
            c->media->v_msmonotonic = ci->ms_monotonic;

            return SEND_AV_KEEP_VIDEO_DATA;
        }

    }

    return SEND_AV_OK;
}


bool decrypt( byte *outbuf, const byte* inbuf, int inbufsz, const byte *tmpkey )
{
    //int decrypt_len = inbufsz - crypto_secretbox_MACBYTES;
    *(USHORT *)outbuf = *(USHORT *)inbuf;
    USHORT datasz = ntohs(*(USHORT *)(inbuf+2));
    *(USHORT *)(outbuf + 2) = (USHORT)htons( datasz - crypto_secretbox_MACBYTES );

    if (datasz > inbufsz) return false; // not all data received

    if (crypto_secretbox_open_easy(outbuf + SIZE_PACKET_HEADER, inbuf + SIZE_PACKET_HEADER, datasz-SIZE_PACKET_HEADER, tmpkey, tmpkey + crypto_secretbox_NONCEBYTES) != 0)
        return false;

    return true;
}

void lan_engine::contact_s::to_offline(int ct)
{
    state = contact_s::OFFLINE;
    sfs.clear();
    data_changed = true;
    waiting_keepalive_answer = false;
    engine->nexthallo = ct + min(500, abs(engine->nexthallo - ct));
}


void lan_engine::contact_s::online_tick(int ct, int nexttime)
{
    if (state == ONLINE)
    {
        if (waiting_keepalive_answer && (ct-keepalive_answer_deadline)>0)
        {
            // oops
            waiting_keepalive_answer = false;
            pipe.close();
            MaskLog(LFLS_CLOSE, "c: %i, state: %i / no keepalive", id.id, state);
            return;
        }

        if ((ct-next_keepalive_packet)>0)
        {
            engine->pg_sync(true, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
            next_keepalive_packet = ct + 50000;
            keepalive_answer_deadline = ct + 10000;
            waiting_keepalive_answer = true;
        }


        if (changed_self)
        {
            if (0 != (changed_self & CDM_NAME))
                send_block(BT_CHANGED_NAME, 0, engine->first->name.cstr(), engine->first->name.get_length());

            if (0 != (changed_self & CDM_STATUSMSG))
                send_block(BT_CHANGED_STATUSMSG, 0, engine->first->statusmsg.cstr(), engine->first->statusmsg.get_length());

            if (0 != (changed_self & CDM_ONLINE_STATE))
            {
                int v = htonl(engine->first->ostate);
                send_block(BT_OSTATE, 0, &v, sizeof(v));
            }

            if (0 != (changed_self & CDM_GENDER))
            {
                int v = htonl(engine->first->gender);
                send_block(BT_GENDER, 0, &v, sizeof(v));
            }

            if (0 != (changed_self & CDM_AVATAR_TAG) && engine->avatar_set)
                send_block(BT_AVATARHASH, 0, engine->first->avatar_hash, 16);

            changed_self = 0;
        }

        if (call_status == OUT_CALL)
        {
            int deltat = (int)(ct - call_stop_time);
            if (deltat >= 0)
            {
                send_block(BT_CALL_CANCEL, 0);
                stop_call_activity(true);
            }
        }
    }

    if (media) media->tick( this, ct );

    auto sbs = sendblocks.lock_write();
    datablock_s *m = sbs().sendblock_f;
    while (m && m->sent > m->len) m = m->next;
    if (m)
    {
        bool is_auth = false;
        const byte *k = message_key(&is_auth);
        engine->pg_data(m, k, is_auth ? SIZE_MAX_SEND_AUTH : SIZE_MAX_SEND_NONAUTH);
        pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

        if (BT_FILE_CHUNK == m->bt)
        {
            logfn("filetr.log", "subblock send %llu %i %i", m->delivery_tag, m->sent, (is_auth ? SIZE_MAX_SEND_AUTH : SIZE_MAX_SEND_NONAUTH));
        }
    }

    bool asap = media != nullptr || engine->first_ftr != nullptr || (m && m->next) || (m && m->sent < m->len);

    nextactiontime = ct;
    if (!asap) nextactiontime += nexttime;
}

static DWORD WINAPI video_encoder_thread( LPVOID )
{
    UNSTABLE_CODE_PROLOG
    lan_engine::get()->video_encoder();
    UNSTABLE_CODE_EPILOG
    return 0;
}

void lan_engine::contact_s::start_media()
{
    call_status = contact_s::IN_PROGRESS;
    if (media == nullptr) media = new media_stuff_s(this);

    stream_options_s so2s;
    so2s.options = htonl( media->local_so.options );
    so2s.view_w = htonl( media->local_so.view_w );
    so2s.view_h = htonl( media->local_so.view_h );

    send_block( BT_STREAM_OPTIONS, 0, &so2s, sizeof( so2s ) );

    if ( callstate.lock_read()( ).allow_run_video_encoder() )
    {
        CloseHandle( CreateThread( nullptr, 0, video_encoder_thread, nullptr, 0, nullptr ) );
        for ( ; !callstate.lock_read()( ).video_encoder; Sleep( 1 ) ); // wait video encoder start
    }

}


void lan_engine::contact_s::recv()
{
    if (ALMOST_ROTTEN == state)
        return;

    byte tmpbuf[65536];
    if (pipe.connected() && state != ROTTEN)
    {
        packet_id_e pid = pipe.packet_id();

        if (_tcp_recv_begin_ < pid  && pid < _tcp_recv_end_)
        {
            if (_tcp_encrypted_begin_ < pid  && pid < _tcp_encrypted_end_)
            {
                if (decrypt(tmpbuf, pipe.rcvbuf, pipe.rcvbuf_sz, message_key()))
                {
                    //logm("decrypt ok: %s", pidname.s);

                    stream_reader r(tmpbuf, pipe.rcvbuf_sz - crypto_secretbox_MACBYTES);
                    if (!r.end()) handle_packet(pid, r);
                } else
                {
                    //logm("decrypt fail: %s", pidname.s);
                }

            } else
            {
                stream_reader r(pipe.rcvbuf, pipe.rcvbuf_sz);
                handle_packet(pid, r);
            }
        }

        if (pid != PID_NONE && pid != PID_DEAD)
            pipe.cpdone();
    }

    if (data_changed)
    {
        if (state == OFFLINE && call_status != CALL_OFF)
            stop_call_activity();

        contact_data_s cd(contact_id_s(), 0);
        savebuffer sb;
        fill_data(cd,&sb);

        cd.mask |= CDM_DETAILS;
        std::string dstr( STD_ASTR("{\"" CDET_PUBLIC_ID "\":\""));
        dstr.append( public_id );

        dstr.append( STD_ASTR("\",\"" CDET_CLIENT "\":\""));
        if (client.is_empty())
            dstr.append(STD_ASTR("isotoxin/0.?.???"));
        else
            dstr.append(client);

        dstr.append( STD_ASTR("\",\"" CDET_CLIENT_CAPS "\":[\"" CLCAP_BBCODE_B "\",\"" CLCAP_BBCODE_U "\",\"" CLCAP_BBCODE_I "\",\"" CLCAP_BBCODE_S "\"]}"));

        cd.details = dstr.cstr();
        cd.details_len = (int)dstr.get_length();

        engine->hf->update_contact(&cd);
    }

}

void lan_engine::contact_s::stop_call_activity(bool notify_me)
{
    if (notify_me) engine->hf->message(MT_CALL_STOP, contact_id_s(), id, now(),  nullptr, 0);
    call_status = CALL_OFF;
    if (media)
    {
        delete media;
        media = nullptr;
    }
}


void lan_engine::contact_s::handle_packet( packet_id_e pid, stream_reader &r )
{
    switch (pid)
    {
    case PID_INVITE:
        {
            name = r.reads();
            std::string oim = invitemessage;
            invitemessage = r.reads();
            if (state == contact_s::ACCEPT_RESTORE_CONNECT)
            {
                state = contact_s::ACCEPT;
                key_sent = false;
                nextactiontime = time_ms();
                data_changed = false;
            } else
            {
                bool sendrq = invitemessage != oim;
                state = contact_s::INVITE_RECV;
                data_changed = true;
                contact_data_s cd(contact_id_s(), 0);
                savebuffer sb;
                fill_data(cd,&sb);
                engine->hf->update_contact(&cd);
                if (sendrq)
                    engine->hf->message(MT_FRIEND_REQUEST, contact_id_s(), id, now(), invitemessage.cstr(), static_cast<int>(invitemessage.get_length()));
            }
        }
        break;
    case PID_ACCEPT:
        if (const byte *key = r.read( SIZE_KEY ))
        {
            std::string rname = r.reads();
            name = rname;
            memcpy( authorized_key, key, SIZE_KEY );

            engine->pg_ready(raw_public_id, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

            changed_self = -1; // name, status and other data will be sent
            state = ONLINE;
            data_changed = true;

            MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / accept online", id.id, state);

            next_keepalive_packet = nextactiontime + 5000;
            engine->pg_sync(false, authorized_key);
            pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);

        }
        break;
    case PID_REJECT:
        state = REJECTED;
        data_changed = true;
        pipe.close();
        MaskLog(LFLS_CLOSE, "c: %i, state: %i / reject", id.id, state);
        break;
    case PID_READY:
        if (const byte *pubid = r.read(SIZE_PUBID))
        {
            if (0 == memcmp(engine->first->raw_public_id,pubid, SIZE_PUBID))
            {
                ASSERT( state == ONLINE || state == ACCEPT || state == BACKCONNECT );
                data_changed |= state != ONLINE;
                state = ONLINE;

                MaskLog(LFLS_ESTBLSH, "c: %i, state: %i / ready online", id.id, state);

                if (data_changed)
                {
                    nextactiontime = time_ms();
                    changed_self = -1;
                    next_keepalive_packet = nextactiontime + 5000;
                    engine->pg_sync(false, authorized_key);
                    pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
                }

                if (!r.end())
                {
                    client = r.reads();
                    data_changed = true;
                }

            }
        }
        break;
    case PID_DATA:
        {
            r.readi(); // read random stub
            /*USHORT flags =*/ r.readus();
            block_type_e bt = (block_type_e)r.readus();

            if (BT_AUDIO_FRAME == bt)
            {
                USHORT flen = r.readus();
                const byte *frame = r.read(flen);
                u64 timelabel = my_ntohll( *(u64 *)frame );
                frame += sizeof( timelabel );
                flen -= sizeof( timelabel );

                if (IN_PROGRESS == call_status && media)
                {
                    audio_format_s fmt(AUDIO_SAMPLERATE, AUDIO_CHANNELS, AUDIO_BITS);
                    int sz = media->decode_audio(frame, flen);
                    if (sz > 0)
                    {
                        media_data_s mdt;
                        mdt.afmt.sample_rate = AUDIO_SAMPLERATE;
                        mdt.afmt.channels = AUDIO_CHANNELS;
                        mdt.afmt.bits = AUDIO_BITS;
                        mdt.audio_frame = media->uncompressed.data();
                        mdt.audio_framesize = sz;
                        mdt.msmonotonic = timelabel;
                        engine->hf->av_data(contact_id_s(), id, &mdt);
                    }
                }

                if ( IS_TLM( TLM_AUDIO_RECV_BYTES ) )
                {
                    tlm_data_s d1 = { static_cast<u64>( id.id ), static_cast<u64>( flen ) };
                    engine->hf->telemetry( TLM_AUDIO_RECV_BYTES, &d1, sizeof( d1 ) );
                }

                break;
            }


            u64 dtb = r.readll(0);
            int sent = r.readi(-1);
            int len = r.readi(-1);
            //time_t create_time = 0;
            //if (flags & 1)
            //    create_time = r.readll(now()) + correct_create_time;

            USHORT msgl = r.readus();
            const byte *msg = r.read(msgl);

            if (dtb && sent >= 0 && len >= 0 && (sent + msgl <= len) && msg)
            {
                auto ddlock = engine->delivery.lock_write();
                std::unique_ptr<delivery_data_s> & ddp = ddlock()[ dtb ];
                if (ddp.get() == nullptr) ddp = std::make_unique<delivery_data_s>();
                delivery_data_s &dd1 = *ddp;
                if ( dd1.buf.size() == 0 || sent == 0 )
                {
                    dd1.buf.resize(len);
                    dd1.rcv_size = 0;
                }
                dd1.rcv_size += msgl;
                memcpy( dd1.buf.data() + sent, msg, msgl );

                if ( BT_FILE_CHUNK == bt )
                {
                    logfn("filetr.log", "subblock recv %llu %i %i %i", dtb, sent, len, msgl);
                }

                if (dd1.rcv_size == dd1.buf.size())
                {
                    std::unique_ptr<delivery_data_s> u = std::move( ddp );
                    delivery_data_s &dd = *u;
                    
                    ddlock().erase( dtb );
                    ddlock.unlock();

                    engine->last_activity = now();

                    // delivered full message
                    engine->pg_delivered( dtb, message_key() );
                    pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
                    //pstr_c rstr; rstr.set(asptr((const char *)dd.buf.data(), dd.buf.size()));
                    switch (bt)
                    {
                    case BT_CHANGED_NAME:
                        name.set((const char *)dd.buf.data(), (int)dd.buf.size());
                        data_changed = true;
                        break;
                    case BT_CHANGED_STATUSMSG:
                        statusmsg.set((const char *)dd.buf.data(), (int)dd.buf.size());
                        data_changed = true;
                        break;
                    case BT_OSTATE:
                        if (dd.buf.size() == sizeof(int))
                        {
                            ostate = (contact_online_state_e)ntohl(*(int *)dd.buf.data());
                            data_changed = true;
                        }
                        break;
                    case BT_GENDER:
                        if (dd.buf.size() == sizeof(int))
                        {
                            gender = (contact_gender_e)ntohl(*(int *)dd.buf.data());
                            data_changed = true;
                        }
                        break;
                    case BT_CALL:
                    //case BT_VOICE_CALL:
                    //case BT_VIDEO_CALL:
                        if (CALL_OFF == call_status)
                        {
                            engine->hf->message(MT_INCOMING_CALL, contact_id_s(), id, now(), /*BT_VOICE_CALL != bt ? "video" :*/ "audio", 5);
                            call_status = IN_CALL;
                        }
                        break;
                    case BT_CALL_CANCEL:
                        if (CALL_OFF != call_status)
                            stop_call_activity();
                        break;
                    case BT_CALL_ACCEPT:
                        if (OUT_CALL == call_status)
                        {
                            engine->hf->message(MT_CALL_ACCEPTED, contact_id_s(), id, now(), nullptr, 0);
                            start_media();
                        }
                        break;
                    case BT_STREAM_OPTIONS:
                        if ( dd.buf.size() == sizeof(stream_options_s) )
                        {
                            if ( media == nullptr )
                                media = new media_stuff_s( this );
                            const stream_options_s *sos = (stream_options_s *)dd.buf.data();
                            media->remote_so.options = ntohl(sos->options);
                            media->remote_so.view_w = ntohl( sos->view_w );
                            media->remote_so.view_h = ntohl( sos->view_h );

                            engine->hf->av_stream_options(contact_id_s(), id, &media->remote_so );
                        }
                        break;
                    case BT_INITDECODER:
                        if ( dd.buf.size() == sizeof( i32 ) * 3 )
                        {
                            if ( media )
                            {
                                if ( media->decoder )
                                {
                                    vpx_codec_destroy( &media->v_decoder );
                                    media->decoder = false;
                                }
                                media->cfg_dec.w = ntohl( *(i32 *)dd.buf.data() );
                                media->cfg_dec.h = ntohl( *( ( (i32 *)dd.buf.data() ) + 1 ) );
                                media->vdecodec = (video_codec_e)ntohl( *( ( (i32 *)dd.buf.data() ) + 2 ) );
                            }
                        }
                        break;
                    case BT_VIDEO_FRAME:
                        if (media)
                            if ( 0 != ( media->local_so.options & SO_RECEIVING_VIDEO ) && 0 != ( media->remote_so.options & SO_SENDING_VIDEO ) )
                            {
                                const framehead_s *fh = (const framehead_s *)dd.buf.data();
                                media->video_frame( my_ntohll( fh->msmonotonic ), ntohl( fh->frame ), dd.buf.data() + sizeof( framehead_s ), static_cast<int>( dd.buf.size() - sizeof( framehead_s ) ) );
                            }

                        if ( IS_TLM( TLM_VIDEO_RECV_BYTES ) )
                        {
                            tlm_data_s d1 = { static_cast<u64>( id.id ), dd.buf.size() };
                            engine->hf->telemetry( TLM_VIDEO_RECV_BYTES, &d1, sizeof( d1 ) );
                        }

                        break;
                    case BT_AUDIO_FRAME:
                        break;
                    case BT_GETAVATAR:
                        send_block(BT_AVATARDATA, 0, engine->avatar.data(), engine->avatar.size());
                        break;
                    case BT_AVATARHASH:
                        if (dd.buf.size() == AVATAR_HASH_SIZE && 0!=memcmp(avatar_hash,dd.buf.data(), AVATAR_HASH_SIZE ))
                        {
                            memcpy(avatar_hash, dd.buf.data(), AVATAR_HASH_SIZE );
                            if (0 == *(int *)avatar_hash && 0 == *(int *)(avatar_hash+4) && 0 == *(int *)(avatar_hash+8) && 0 == *(int *)(avatar_hash+12))
                                avatar_tag = 0;
                            else
                                avatar_tag++;

                            contact_data_s cd(id, CDM_AVATAR_TAG);
                            cd.avatar_tag = avatar_tag;
                            engine->hf->update_contact(&cd);
                            engine->hf->save();
                        }
                        break;
                    case BT_AVATARDATA:
                        {
                            byte hash[AVATAR_HASH_SIZE];
                            crypto_generichash( hash, sizeof( hash ), (const byte *)dd.buf.data(), dd.buf.size(), nullptr, 0 );

                            if (0 != memcmp( hash, avatar_hash, sizeof( hash ) ))
                            {
                                memcpy(avatar_hash, hash, sizeof( hash ) );
                                ++avatar_tag; // new avatar version
                            }
                                
                            engine->hf->avatar_data(id, avatar_tag, dd.buf.data(), static_cast<int>(dd.buf.size()));
                        }
                        break;
                    case BT_SENDFILE:
                        if (dd.buf.size() > 16)
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 sid = my_ntohll(d[0]);
                            u64 fsz = my_ntohll(d[1]);
                            new incoming_file_s(id, sid, fsz, std::string((const char *)dd.buf.data() + 16, dd.buf.size() - 16));
                        }
                        break;
                    case BT_FILE_BREAK:
                        if (dd.buf.size() == sizeof(u64))
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 sid = my_ntohll(d[0]);
                            if(file_transfer_s *f = engine->find_ftr_by_sid(sid))
                                f->kill(false);
                        }
                        break;
                    case BT_FILE_ACCEPT:
                        if (dd.buf.size() == sizeof(u64) * 2)
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 sid = my_ntohll(d[0]);
                            u64 offset = my_ntohll(d[1]);
                            if (file_transfer_s *f = engine->find_ftr_by_sid(sid))
                                f->accepted(offset);
                        }
                        break;
                    case BT_FILE_DONE:
                        logfn("filetr.log", "BT_FILE_DONE buffsize %u", dd.buf.size());
                        if (dd.buf.size() == sizeof(u64))
                        {
                            u64 sid = my_ntohll(*(u64 *)dd.buf.data());

                            logfn("filetr.log", "BT_FILE_CHUNK done %llu", sid);

                            if (file_transfer_s *f = engine->find_ftr_by_sid(sid))
                                f->finished(false);
                        }
                        break;
                    case BT_FILE_PAUSE:
                        if (dd.buf.size() == sizeof(u64))
                        {
                            u64 sid = my_ntohll(*(u64 *)dd.buf.data());
                            if (file_transfer_s *f = engine->find_ftr_by_sid(sid))
                                f->pause(false);
                        }
                        break;
                    case BT_FILE_UNPAUSE:
                        if (dd.buf.size() == sizeof(u64))
                        {
                            u64 sid = my_ntohll(*(u64 *)dd.buf.data());
                            if (file_transfer_s *f = engine->find_ftr_by_sid(sid))
                                f->unpause(false);
                        }
                        break;
                    case BT_FILE_CHUNK:
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 sid = my_ntohll(d[0]);
                            u64 offset = my_ntohll(d[1]);

                            logfn("filetr.log", "BT_FILE_CHUNK %llu %llu", sid, offset);

                            if ( file_transfer_s *f = engine->find_ftr_by_sid( sid ) )
                            {
                                f->chunk_received( offset, d + 2, dd.buf.size() - sizeof( u64 ) * 2 );

                                if ( IS_TLM( TLM_FILE_RECV_BYTES ) )
                                {
                                    tlm_data_s d2 = { f->utag, dd.buf.size() };
                                    engine->hf->telemetry( TLM_FILE_RECV_BYTES, &d2, sizeof( d2 ) );
                                }

                            }
                        }
                        break;
                    case BT_TYPING:
                        engine->hf->typing(contact_id_s(), id );
                        break;
                    case BT_FOLDERSHARE_ANNOUNCE:
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 utag = my_ntohll(d[0]);

                            if (contact_s::fsh_s *fsh = find_fsh(utag))
                            {
                                // if fsh already present, it already announced
                                // no need to do it again
                            }
                            else
                            {
                                const char *shname = (const char *)(d + 1);
                                int l = static_cast<int>(dd.buf.size() - sizeof(u64));
                                engine->hf->message(MT_FOLDER_SHARE_ANNOUNCE, contact_id_s(), id, utag, shname, l);
                                sfs.emplace_back(utag, id, std::asptr(shname, l));
                            }

                        }
                        break;
                    case BT_FOLDERSHARE_TOC:
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 fsutag = my_ntohll(d[0]);

                            for (contact_s::fsh_ptr_s &sf : sfs)
                            {
                                if (sf.sf->utag == fsutag)
                                {
                                    engine->hf->folder_share(fsutag, d + 1, static_cast<int>(dd.buf.size() - sizeof(u64))); // toc
                                    break;
                                }
                            }

                        }
                        break;
                    case BT_FOLDERSHARE_CTL:
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 fsutag = my_ntohll(d[0]);
                            folder_share_control_e ctl = static_cast<folder_share_control_e>( *(byte *)(d + 1) );
                            size_t cnt = sfs.size();
                            for (size_t i = 0; i < cnt; ++i)
                            {
                                contact_s::fsh_ptr_s &sf = sfs[i];
                                if (sf.sf->utag == fsutag)
                                {
                                    switch (ctl)
                                    {
                                    case FSC_ACCEPT:
                                        // send toc
                                        if (sf.sf->toc_size > 0)
                                        {
                                            send_block(BT_FOLDERSHARE_TOC, 0, d, sizeof(u64), sf.sf->to_data(), sf.sf->toc_size);
                                            sf.clear_toc(); // no need to after send on accept
                                        }

                                        break;
                                    case FSC_REJECT:
                                        removeFast(sfs, i);
                                        break;
                                    }
                                    engine->hf->folder_share_ctl(sf.sf->utag, ctl);
                                    break;
                                }
                            }
                        }
                        break;
                    case BT_FOLDERSHARE_QUERY:
                        {
                            const u64 *d = (u64 *)dd.buf.data();
                            u64 fsutag = my_ntohll(d[0]);
                            std::asptr tocname, fakename;
                            tocname.l = ntohs( *(uint16_t *)(d + 1) );
                            fakename.l = ntohs(*(((uint16_t *)(d + 1)) + 1));
                            tocname.s = (const char *)(((uint16_t *)(d + 1)) + 2);
                            fakename.s = tocname.s + tocname.l;
                            engine->hf->folder_share_query(fsutag, tocname.s, tocname.l, fakename.s, fakename.l);
                        }
                        break;
                    default:
                        if (bt < __bt_service)
                        {
                            u64 crtime = my_ntohll(*(u64 *)dd.buf.data());
                            engine->hf->message((message_type_e)bt, contact_id_s(), id, crtime, (const char *)dd.buf.data() + sizeof(u64), static_cast<int>(dd.buf.size() - sizeof(u64)));
                        }
                        break;
                    }
                }
            }

        }
        break;
    case PID_DELIVERED:
        if ( u64 dtb = r.readll( 0 ) )
        {
            auto sbs = sendblocks.lock_write();
            u64 n = 0;
            for ( datablock_s *m = sbs().sendblock_f; m; m = m->next )
                if ( m->delivery_tag == dtb )
                {
                    if ( m->bt < __bt_service )
                        n = m->delivery_tag;

                    if ( m->bt == BT_FILE_CHUNK )
                    {
                        logfn( "filetr.log", "PID_DELIVERED %llu", m->delivery_tag );

                        for ( file_transfer_s *f = engine->first_ftr; f; f = f->next )
                            if ( f->delivered( m->delivery_tag ) )
                                break;
                    }

                    LIST_DEL( m, sbs().sendblock_f, sbs().sendblock_l, prev, next );
                    m->die();
                    sbs.unlock();

                    engine->delivery.lock_write()() .erase( dtb );
                    break;
                }
            if (n)
                engine->hf->delivered( n );
        }
        break;
    case PID_KEEPALIVE:
        {
            waiting_keepalive_answer = false;
            r.read(sizeof(u64)); // skip random stuff
            r.readll(0); // skip zero ll
            bool resync = r.readb() != 0;
            if (resync)
            {
                engine->pg_sync(false, authorized_key);
                pipe.send(engine->packet_buf_encoded, engine->packet_buf_encoded_len);
            }
        }
        break;
    }

}

lan_engine::file_transfer_s::file_transfer_s(const std::string &fn):fn(fn)
{
    LIST_ADD(this, engine->first_ftr, engine->last_ftr, prev, next);
}
lan_engine::file_transfer_s::~file_transfer_s()
{
    LIST_DEL(this, engine->first_ftr, engine->last_ftr, prev, next);
}

void lan_engine::file_transfer_s::kill(bool from_self)
{
    if (from_self)
    {
        if (contact_s *c = engine->find(cid))
        {
            u64 sidnet = my_htonll(sid);
            c->send_block(BT_FILE_BREAK, 0, &sidnet, sizeof( sidnet ));
        }
    }
    else
        engine->hf->file_control(utag, FIC_BREAK);
    delete this;
}

lan_engine::incoming_file_s::incoming_file_s(contact_id_s cid_, u64 utag_, u64 fsz_, const std::string &fn) :file_transfer_s(fn)
{
    utag = engine->genfutag();
    sid = utag_;
    fsz = fsz_;
    cid = cid_;

    engine->hf->incoming_file(cid, utag, fsz, fn.cstr(), fn.get_length());

}

/*virtual*/ void lan_engine::incoming_file_s::chunk_received( u64 offset, const void *d, aint dsz )
{
    if (is_accepted)
    {
        if (nextoffset != offset)
        {
            pause(true);
            resumein = time_ms() + 1000;
            stuck = true;
            return;
        }

        if (engine->hf->file_portion( utag, offset, d, (int)dsz ))
        {
            nextoffset = offset + dsz;
            logfn("filetr.log", "chunk_received %llu %llu %u", utag, offset, dsz);
        } else
        {
            logfn("filetr.log", "chunk_received but buffer stuck %llu %llu %u", utag, offset, dsz);
            pause(true);
            resumein = time_ms() + 1000;
            stuck = true;
        }
    }
}

/*virtual*/ void lan_engine::incoming_file_s::accepted(u64 offset)
{
    if (contact_s *c = engine->find(cid))
    {
        struct
        {
            u64 sidnet;
            u64 offsetnet;
        } d = { my_htonll(sid), my_htonll(offset) };
        c->send_block(BT_FILE_ACCEPT, 0, &d, sizeof(d));
        is_accepted = true;
    }
}

/*virtual*/ void lan_engine::incoming_file_s::finished(bool from_self)
{
    logfn("filetr.log", "finished %llu", utag);

    if (from_self)
        delete this;
    else
        engine->hf->file_control(utag, FIC_DONE);
}
/*virtual*/ void lan_engine::incoming_file_s::pause(bool from_self)
{
    if (from_self)
    {
        if (contact_s *c = engine->find(cid))
        {
            u64 sidnet = my_htonll(sid);
            c->send_block(BT_FILE_PAUSE, 0, &sidnet, sizeof( sidnet ));
        }
    }
    else
        engine->hf->file_control(utag, FIC_PAUSE);
}

/*virtual*/ void lan_engine::incoming_file_s::unpause(bool from_self)
{
    if (from_self)
    {
        if (contact_s *c = engine->find(cid))
        {
            u64 sidnet = my_htonll(sid);
            c->send_block(BT_FILE_UNPAUSE, 0, &sidnet, sizeof( sidnet ));
        }
    }
    else
        engine->hf->file_control(utag, FIC_UNPAUSE);
}

/*virtual*/ void lan_engine::incoming_file_s::tick(int ct)
{
    if (contact_s *c = engine->find(cid))
    {
        if (c->state == contact_s::ONLINE)
        {
            if (stuck && (ct - resumein) > 0)
            {
                stuck = false;
                accepted(nextoffset);
            }
            return;
        }
    }

    engine->hf->file_control(utag, FIC_DISCONNECT);
    delete this;
}

lan_engine::transmitting_file_s::transmitting_file_s(contact_s *to_contact, u64 utag_, u64 fsz_, const std::string &fn) :file_transfer_s(fn)
{
    fsz = fsz_;
    cid = to_contact->id;
    utag = utag_;
    sid = engine->gensid();

    struct
    {
        u64 sid;
        u64 fsz;
    } d = { my_htonll( sid ), my_htonll(fsz) };
    to_contact->send_block(BT_SENDFILE, 0, &d, sizeof(d), fn.cstr(), fn.get_length());
}


/*virtual*/ void lan_engine::transmitting_file_s::accepted(u64 offset_)
{
    if ((!request && rch[0].buf == nullptr) || offset_ > 0)
    {
        rch[0].clear();
        rch[1].clear();

        reques_next_block(offset_ & FILE_TRANSFER_CHUNK_MASK);
    }
    
    is_accepted = true;
    is_paused = false;

    engine->hf->file_control(utag, FIC_ACCEPT);
}
/*virtual*/ void lan_engine::transmitting_file_s::finished(bool from_self)
{
    ASSERT(from_self);

    if (contact_s *c = engine->find(cid))
    {
        u64 sidnet = my_htonll(sid);
        c->send_block(BT_FILE_DONE, 0, &sidnet, sizeof( sidnet ));
    }

    engine->hf->file_control(utag, FIC_DONE);
    delete this;
}
/*virtual*/ void lan_engine::transmitting_file_s::pause(bool from_self)
{
    is_paused = true;
    if (from_self)
    {
        if (contact_s *c = engine->find(cid))
        {
            u64 utagnet = my_htonll(utag);
            c->send_block(BT_FILE_PAUSE, 0, &utagnet, sizeof(utagnet));
        }
    } else
        engine->hf->file_control(utag, FIC_PAUSE);
}

/*virtual*/ void lan_engine::transmitting_file_s::unpause(bool from_self)
{
    is_paused = false;
    if (from_self)
    {
        if (contact_s *c = engine->find(cid))
        {
            u64 utagnet = my_htonll(utag);
            c->send_block(BT_FILE_UNPAUSE, 0, &utagnet, sizeof(utagnet));
        }
    }
    else
        engine->hf->file_control(utag, FIC_UNPAUSE);
}

DELTA_TIME_PROFILER(xxx, 1024);

bool lan_engine::transmitting_file_s::ready_chunk_s::set(const file_portion_prm_s *fp)
{
    if (nullptr == buf && fp->offset == offset)
    {
        buf = fp->data;
        offset = fp->offset;
        size = fp->size;
        dtg = 0;
        return true;
    }
    return false;
}

void lan_engine::transmitting_file_s::ready_chunk_s::clear()
{
    if (buf)
        engine->hf->file_portion(0, 0, buf, 0); // release buffer

    memset( this, 0, sizeof(*this) );
}

void lan_engine::transmitting_file_s::reques_next_block(u64 offset_)
{
    if (fsz > offset_)
    {
        engine->hf->file_portion(utag, offset_, nullptr, FILE_TRANSFER_CHUNK); // request now
        int i = 0;
        if (rch[0].buf) ++i;
        ASSERT(rch[i].buf == nullptr && rch[i].offset == 0);
        rch[i].offset = offset_;
    }
    request = true;
}

void lan_engine::transmitting_file_s::send_block(contact_s *c)
{
    struct
    {
        u64 sid;
        u64 offset_net;
    } d = { my_htonll(sid), my_htonll(rch[0].offset) };

    ASSERT(rch[0].buf && rch[0].dtg == 0);
    rch[0].dtg = c->send_block(BT_FILE_CHUNK, 0, &d, sizeof(d), rch[0].buf, rch[0].size);

    if ( IS_TLM( TLM_FILE_SEND_BYTES ) )
    {
        tlm_data_s d2 = { utag, static_cast<u64>( rch[ 0 ].size ) };
        engine->hf->telemetry( TLM_FILE_SEND_BYTES, &d2, sizeof( d2 ) );
    }
}

bool lan_engine::transmitting_file_s::fresh_file_portion(const file_portion_prm_s *fp)
{
    logfn("filetr.log", "fresh_file_portion fp %llu (%llu)", utag, fp->offset);

    if (contact_s *c = engine->find(cid))
    {
        if (c->state == contact_s::ONLINE)
        {
            DELTA_TIME_CHECKPOINT(xxx);

            ASSERT( fp->size == FILE_TRANSFER_CHUNK || (fp->offset + fp->size) == fsz );

            if ( rch[0].set(fp) )
            {
                ASSERT( rch[1].buf == nullptr );
                send_block( c );
                reques_next_block(fp->offset + FILE_TRANSFER_CHUNK);

            } else
            {
                ASSERT( rch[0].offset + FILE_TRANSFER_CHUNK == fp->offset && rch[1].buf == nullptr );
                CHECK( rch[1].set(fp) );
            }

            return true;

        }
    }
    return false;
}

/*virtual*/ bool lan_engine::transmitting_file_s::delivered(u64 idtg)
{
    if (rch[0].dtg == idtg)
    {
        if (rch[0].offset + rch[0].size == fsz)
            last_chunk_delivered = true;

        rch[0].clear();

        rch[0] = rch[1];
        rch[1].buf = nullptr;
        rch[1].clear();

        if ( rch[0].buf )
        {
            if (contact_s *c = engine->find(cid))
            {
                if (c->state == contact_s::ONLINE)
                {
                    send_block(c);
                    reques_next_block(rch[0].offset + FILE_TRANSFER_CHUNK);
                }
            }
        }


        logfn("filetr.log", "delivered %llu", idtg);
        return true;
    }

    return false;
}

/*virtual*/ void lan_engine::transmitting_file_s::tick(int /*ct*/)
{
    if (contact_s *c = engine->find(cid))
    {
        if (c->state == contact_s::ONLINE)
        {
            if (is_accepted && !is_paused && !is_finished)
            {
                //logm("portion request %llu (%llu)", utag, offset);

                if ( nullptr == rch[0].buf && !request)
                {
                    reques_next_block(0);

                } else if (last_chunk_delivered)
                {
                    // send done
                    finished(true);
                }
            }

            return;
        }
    }

    engine->hf->file_control(utag, FIC_DISCONNECT);
    delete this;

}


void lan_engine::tick_ftr(int ct)
{
    for (file_transfer_s *f = first_ftr; f;)
    {
        file_transfer_s *ff = f->next;
        f->tick(ct);
        f = ff;
    }
}


void lan_engine::file_send(contact_id_s id, const file_send_info_prm_s *finfo)
{
    bool ok = false;
    if (contact_s *c = find(id))
    {
        if (c->state == contact_s::ONLINE)
        {
            new transmitting_file_s(c, finfo->utag, finfo->filesize, std::string(finfo->filename, finfo->filename_len));
            ok = true;
        }
    }
    if (!ok)
        hf->file_control(finfo->utag, FIC_DISCONNECT); // put it to send queue: assume transfer broken
}

void lan_engine::file_accept(u64 utag, u64 offset)
{
    if ( incoming_file_s *ft = find_incoming_ftr( utag ) )
        ft->accepted( offset );
}

lan_engine::incoming_file_s *lan_engine::find_incoming_ftr(u64 utag)
{
    for(file_transfer_s *f = first_ftr; f; f = f->next)
        if ( f->utag == utag )
            if ( incoming_file_s *i = dynamic_cast<incoming_file_s *>(f) )
                return i;

    return nullptr;
}

lan_engine::transmitting_file_s *lan_engine::find_transmitting_ftr( u64 utag )
{
    for ( file_transfer_s *f = first_ftr; f; f = f->next )
        if ( f->utag == utag )
            if ( transmitting_file_s *t = dynamic_cast<transmitting_file_s *>( f ) )
                return t;

    return nullptr;
}

lan_engine::file_transfer_s *lan_engine::find_ftr_by_sid( u64 sid )
{
    for ( file_transfer_s *f = first_ftr; f; f = f->next )
        if ( f->sid == sid )
            return f;

    return nullptr;
}

u64 lan_engine::gensid()
{
    return random64( [this]( u64 t )->bool {

        for ( file_transfer_s *f = first_ftr; f; f = f->next )
            if ( f->sid == t )
                return true;
        return false;
    } );
}

u64 lan_engine::genfutag()
{
    return random64( [this]( u64 t )->bool {

        for ( file_transfer_s *f = first_ftr; f; f = f->next )
            if ( f->utag == t )
                return true;
        return false;
    } );
}

void lan_engine::file_control(u64 utag, file_control_e ctl)
{
    file_transfer_s *f = find_incoming_ftr( utag );
    if (!f) f = find_transmitting_ftr( utag );

    if(f)
    {
        switch (ctl) //-V719
        {
        //case FIC_ACCEPT:
        //    f->accepted( 0 );
        //    break;
        case FIC_REJECT:
        case FIC_BREAK:
            f->kill(true);
            break;
        case FIC_DONE:
            f->finished(true);
            break;
        case FIC_PAUSE:
            f->pause(true);
            break;
        case FIC_UNPAUSE:
            f->unpause(true);
            break;
        }
        return;
    }
    if ( FIC_CHECK == ctl )
    {
        hf->file_control( utag, FIC_UNKNOWN );
    }

}

bool lan_engine::file_portion(u64 utag, const file_portion_prm_s *fp)
{
    if (transmitting_file_s *f = find_transmitting_ftr(utag))
        if (f->fresh_file_portion(fp))
            return true;

    return false;
}

void lan_engine::folder_share_ctl(u64 utag, const folder_share_control_e ctl)
{
    if (contact_s::fsh_s *fsh = find_fsh(utag))
    {
        if (contact_s *c = engine->find(fsh->cid))
        {
            struct ctls
            {
                u64 utag;
                byte ctl;
            } x;
            x.utag = my_htonll(fsh->utag);
            x.ctl = static_cast<byte>(ctl);
            c->send_block(BT_FOLDERSHARE_CTL, 0, &x, 1 + sizeof(u64));
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

void lan_engine::contact_s::fsh_s::enlist()
{
    LIST_ADD(this, engine->first_fsh, engine->last_fsh, prev, next);
}
void lan_engine::contact_s::fsh_s::unlist()
{
    LIST_DEL(this, engine->first_fsh, engine->last_fsh, prev, next);
}
void lan_engine::contact_s::fsh_s::die()
{
    if (contact_s *c = engine->find(cid))
    {
        size_t cnt = c->sfs.size();
        for (size_t i=0;i<cnt;++i)
        {
            contact_s::fsh_ptr_s &sf = c->sfs[i];
            if (sf.sf == this)
            {
                removeFast(c->sfs, i);
                break;
            }
        }

    }
}


void lan_engine::folder_share_toc(contact_id_s cid, const folder_share_prm_s *sfd)
{

    if (contact_s *c = find(cid))
    {
        auto send_announce = [&]()
        {
            std::asptr n(sfd->name);
            c->sfs.emplace_back(sfd->utag, c->id, n, sfd->ver, sfd->data, sfd->size);

            u64 utag = my_htonll(sfd->utag);
            c->send_block(BT_FOLDERSHARE_ANNOUNCE, 0, &utag, sizeof(utag), n.s, n.l);

        };

        if (c->state == contact_s::ONLINE)
        {
            for (contact_s::fsh_ptr_s &sf : c->sfs)
            {
                if (sf.sf->utag == sfd->utag)
                {
                    if (sf.sf->toc_size > 0)
                    {
                        // not yet confirm
                        // just update toc
                        sf.update_toc(sfd->data, sfd->size);
                        send_announce();
                        return;
                    }

                    if (sf.sf->toc_ver < sfd->ver)
                    {
                        sf.sf->toc_ver = sfd->ver;
                        u64 utag = my_htonll(sfd->utag);
                        c->send_block(BT_FOLDERSHARE_TOC, 0, &utag, sizeof(utag), sfd->data, sfd->size);
                        return;
                    }
                }
            }

            send_announce();
        }
    }

}

void  lan_engine::folder_share_query(u64 utag, const folder_share_query_prm_s *prm)
{
    if (contact_s::fsh_s *fsh = find_fsh(utag))
    {
        if (contact_s *c = engine->find(fsh->cid))
        {
            int len = sizeof(u64) + sizeof(uint16_t) * 2 + prm->tocname_len + prm->fakename_len;
            byte *b = (byte *)_alloca(len);
            *(u64 *)b = my_htonll(utag);
            *(uint16_t *)(b + sizeof(u64)) = htons(static_cast<uint16_t>(prm->tocname_len));
            *(uint16_t *)(b + sizeof(u64) + sizeof(uint16_t)) = htons(static_cast<uint16_t>(prm->fakename_len));
            memcpy(b + sizeof(u64) + sizeof(uint16_t) * 2, prm->tocname, prm->tocname_len);
            memcpy(b + sizeof(u64) + sizeof(uint16_t) * 2 + prm->tocname_len, prm->fakename, prm->fakename_len);

            c->send_block(BT_FOLDERSHARE_QUERY, 0, b, len);
        }
    }

}

void lan_engine::create_conference(const char * /*confaname*/, const char * /*options*/)
{
}

void lan_engine::ren_conference(contact_id_s /*gid*/, const char * /*confaname*/)
{
}

void lan_engine::join_conference(contact_id_s /*gid*/, contact_id_s /*cid*/ )
{
}
void lan_engine::del_conference( const char * /*conference_id*/ )
{
}
void lan_engine::enter_conference( const char * /*conference_id*/ )
{
}
void lan_engine::leave_conference(contact_id_s /*gid*/, int /*keepleave*/ )
{
}

void lan_engine::contact(const contact_data_s * cdata)
{
    if (cdata->data_size)
    {
        loader ldr(cdata->data, cdata->data_size);
        load_contact(cdata->id, ldr);
        if (contact_s *c = find(cdata->id))
        {
            contact_data_s cd(contact_id_s(), 0);
            c->fill_data(cd, nullptr);
            hf->update_contact(&cd);
        }
    }
}

void lan_engine::logging_flags(unsigned int f)
{
    g_logging_flags = f;
}

void lan_engine::proto_file(i32, const file_portion_prm_s *)
{

}