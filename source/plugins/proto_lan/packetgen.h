#pragma once

/*
    the difference between PUBLIC KEY and PUBLIC ID is:

    PUBLIC KEY - is one of pair keys: PRIVATE KEY and PUBLIC KEY itself

    PUBLIC ID - is hash of PUBLIC KEY with integrity check
    PUBLIC ID is 20 bytes len and 40 hexadecimal symbols

    if you know PUBLIC KEY, you can calculate PUBLIC ID
    if you know correct true PUBLIC ID of your friend, nobody can mimic your friend's contact
*/

#define LAN_PROTOCOL_VERSION 3 // only clients with same version can interact
#define LAN_SAVE_VERSION 1

#define SIZE_PACKET_HEADER 4
#define SIZE_MAX_SEND_NONAUTH (512 - SIZE_PACKET_HEADER - crypto_secretbox_MACBYTES)
#define SIZE_MAX_SEND_AUTH (65000 - SIZE_PACKET_HEADER - crypto_secretbox_MACBYTES)
#define SIZE_PUBID 20 // full pub id size
#define SIZE_KEY (crypto_secretbox_NONCEBYTES + crypto_secretbox_KEYBYTES)
#define SIZE_KEY_NONCE_PART (crypto_secretbox_NONCEBYTES)
#define SIZE_KEY_PASS_PART (crypto_secretbox_KEYBYTES)
#define SIZE_PUBLIC_KEY (crypto_box_PUBLICKEYBYTES)
#define SIZE_SECRET_KEY (crypto_box_SECRETKEYBYTES)

#define SIZE_MAX_FRIEND_REQUEST_BYTES 256

enum packet_id_e
{
    PID_NONE,

    // udp
    PID_SEARCH, // udp broadcast 1st client search
    PID_HALLO,

    // tcp
    PID_MEET,       // tcp 1st packet if non authorized
    PID_NONCE,      // tcp 1st packet if authorized

    _tcp_recv_begin_,
    _tcp_encrypted_begin_,

    PID_INVITE,
    PID_ACCEPT,
    PID_READY,  // answer to accept
    PID_DATA,
    PID_DELIVERED,
    PID_SYNC, // sync time

    _tcp_encrypted_end_,

    PID_REJECT,
    
    _tcp_recv_end_,

    PID_DEAD = 0xFFFF,
};

struct datablock_s;

class packetgen
{
    void push_pid( packet_id_e pid )
    {
        packet_buf_len = SIZE_PACKET_HEADER;

        *(USHORT *)(packet_buf + 0) = (USHORT)htons((USHORT)pid);
        *(USHORT *)(packet_buf + 2) = (USHORT)htons((USHORT)SIZE_PACKET_HEADER);
    }

    void pushb(byte v, bool correct_size = true)
    {
        push(&v, 1, correct_size);
    }
    void pushus( USHORT v, bool correct_size = true)
    {
        USHORT vv = htons( v );
        push(&vv,sizeof(vv), correct_size);
    }
    void pushi(int v, bool correct_size = true)
    {
        int vv = htonl(v);
        push(&vv, sizeof(vv), correct_size);
    }
    void pushll(u64 v, bool correct_size = true)
    {
        u64 vv = my_htonll(v);
        push(&vv, sizeof(vv), correct_size);
    }

    void push( int clamp_chars, const asptr&s )
    {
        int l = s.l;
        if (l > clamp_chars) l = clamp_chars;
        pushus((USHORT)l);
        push(s.s, l);
    }

    void push(const void *data, int size, bool correct_size = true)
    {
        if (ASSERT(packet_buf_len + size <= sizeof(packet_buf)))
        {
            memcpy(packet_buf + packet_buf_len, data, size);
            packet_buf_len += size;

            if (correct_size) *(USHORT *)(packet_buf + 2) = (USHORT)htons((USHORT)packet_buf_len);
        }
    }

    byte *push(int size, bool correct_size = true)
    {
        byte *r = nullptr;
        if (ASSERT(packet_buf_len + size <= sizeof(packet_buf)))
        {
            r = packet_buf + packet_buf_len;
            packet_buf_len += size;
            if (correct_size) *(USHORT *)(packet_buf + 2) = (USHORT)htons((USHORT)packet_buf_len);
        }
        return r;
    }

    void encode( const byte *enckey )
    {
        packet_buf_encoded_len = crypto_secretbox_MACBYTES + packet_buf_len;
        *(USHORT *)packet_buf_encoded = *(USHORT *)packet_buf;
        *(USHORT *)(packet_buf_encoded + 2) = (USHORT)htons((USHORT)packet_buf_encoded_len);

        ASSERT(packet_buf_encoded_len <= sizeof(packet_buf_encoded));

        crypto_secretbox_easy(packet_buf_encoded + SIZE_PACKET_HEADER, packet_buf + SIZE_PACKET_HEADER, packet_buf_len - SIZE_PACKET_HEADER, enckey, enckey + SIZE_KEY_NONCE_PART);
    }

    void encode()
    {
        memcpy(packet_buf_encoded, packet_buf, packet_buf_len);
        packet_buf_encoded_len = packet_buf_len;
    }
    void encopy()
    {
        memcpy(packet_buf_encoded, packet_buf, packet_buf_len);
        packet_buf_encoded_len = packet_buf_len;
    }

protected:
    void decode()
    {
        memcpy(packet_buf, packet_buf_encoded, packet_buf_encoded_len);
        packet_buf_len = packet_buf_encoded_len;
    }

    byte packet_buf[65536], packet_buf_encoded[65536];
    int packet_buf_len = 0, packet_buf_encoded_len = 0;

    byte my_public_key[SIZE_PUBLIC_KEY];
    byte my_secret_key[SIZE_SECRET_KEY];
    
    int my_mastertag = 0; // changes every hallo, 0 - means no offline contacts

public:
    packetgen();
    ~packetgen();

    void pg_search( int back_tcp_port, const byte *seeking_contact_raw_pub_id ); // gen search udp packet
    void pg_hallo( int back_tcp_port ); // gen hallo udp packet

    void pg_meet(const byte *other_public_key, const byte *temporary_key);

    void pg_invite(const asptr &inviter_name, const asptr& invite_message, const byte *crypt_packet_key);
    void pg_accept(const asptr&name, const byte *auth_key, const byte *crypt_packet_key);
    void pg_ready(const byte *raw_public_id, const byte *crypt_packet_key);
    void pg_reject(); // no crypt
    
    void pg_nonce(const byte *other_public_key, const byte *auth_key /*nonce + contact key*/ );

    void pg_raw_data(const byte *crypt_packet_key, int bt, const byte *data, int size);
    void pg_data(datablock_s *m, const byte *crypt_packet_key, int maxsize);
    void pg_delivered(u64 dtag, const byte *crypt_packet_key);
    void pg_sync(bool resync, const byte *crypt_packet_key);
};