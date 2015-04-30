#include "stdafx.h"

#if LOGGING
#define logm Log
void log_auth_key( const char *what, const byte *key );
void log_bytes( const char *what, const byte *b, int sz );
#else
#define log_auth_key(a,b)
#define log_bytes(a, b, c)
#define logm(...)
#endif 

packetgen::packetgen()
{
}
packetgen::~packetgen()
{
}

void packetgen::pg_search( int back_tcp_port, const byte *seeking_contact_raw_pub_id )
{
    packet_buf_len = 0;

    pushus( PID_SEARCH, false );
    pushus( LAN_PROTOCOL_VERSION, false ); // version
    pushus( (USHORT)back_tcp_port, false );

    push(my_public_key, SIZE_PUBLIC_KEY, false);
    push(seeking_contact_raw_pub_id, SIZE_PUBID, false);

    encode();
}

void packetgen::pg_hallo( int back_tcp_port )
{
    packet_buf_len = 0;

    pushus(PID_HALLO, false);
    pushus(LAN_PROTOCOL_VERSION, false); // version
    pushus((USHORT)back_tcp_port, false);
    if (my_mastertag == 0)
        my_mastertag = randombytes_random();
    pushi(my_mastertag, false);

    push(my_public_key, SIZE_PUBLIC_KEY, false);

    encode();
}

void packetgen::pg_meet(const byte *other_public_key, const byte *temporary_key)
{
    push_pid( PID_MEET );

    push(my_public_key, SIZE_PUBLIC_KEY);

    byte *nonce = push(crypto_box_NONCEBYTES);
    randombytes_buf(nonce, crypto_box_NONCEBYTES);

    int cipher_len = crypto_box_MACBYTES + SIZE_KEY;
    byte *cipher = push(cipher_len);
    crypto_box_easy(cipher, temporary_key, SIZE_KEY, nonce, other_public_key, my_secret_key);

    encopy();
}

void packetgen::pg_nonce(const byte *other_public_key, const byte *auth_key /* nonce + contact key */)
{
    logm("pg_nonce =================================================================================");
    log_bytes("other_public_key", other_public_key, SIZE_PUBLIC_KEY);
    log_auth_key("auth_key", auth_key);

    push_pid(PID_NONCE);

    push(my_public_key, SIZE_PUBLIC_KEY);
    log_bytes("my_public_key (send)", my_public_key, SIZE_PUBLIC_KEY);

    // nonce for decrypt nonce part of contact key
    byte *nonce = push(crypto_box_NONCEBYTES);
    randombytes_buf(nonce, crypto_box_NONCEBYTES);

    log_bytes("nonce 4 nonce (send)", nonce, crypto_box_NONCEBYTES);

    // encrypt nonce part of contact key by public key
    int cipher_len = crypto_box_MACBYTES + SIZE_KEY_NONCE_PART;
    byte *cipher = push(cipher_len);
    crypto_box_easy(cipher, auth_key, SIZE_KEY_NONCE_PART, nonce, other_public_key, my_secret_key);

    // hash to verify we have same contact key
    byte *hash = push(crypto_generichash_BYTES);
    crypto_generichash(hash, crypto_generichash_BYTES, auth_key, SIZE_KEY, nullptr, 0 );

    log_bytes("hash of authkey", hash, crypto_generichash_BYTES);

    encopy();

}

void packetgen::pg_invite(const asptr &inviter_name, const asptr& invite_message, const byte *crypt_packet_key)
{
    push_pid( PID_INVITE );
    push( 64, inviter_name );
    push( SIZE_MAX_FRIEND_REQUEST_BYTES, invite_message );
    encode(crypt_packet_key);
    log_auth_key("PID_INVITE encoded", crypt_packet_key);
}

void packetgen::pg_accept(const asptr&name, const byte *auth_key, const byte *crypt_packet_key)
{
    push_pid( PID_ACCEPT );
    push(auth_key, SIZE_KEY);
    push( 64, name );

    encode(crypt_packet_key);
    log_auth_key("PID_ACCEPT encoded", crypt_packet_key);
}

void packetgen::pg_ready(const byte *raw_public_id, const byte *crypt_packet_key)
{
    push_pid( PID_READY );
    push(raw_public_id, SIZE_PUBID);
    encode(crypt_packet_key);
    log_auth_key("PID_READY encoded", crypt_packet_key);
}

void packetgen::pg_reject()
{
    push_pid(PID_REJECT);
    encopy();
}

void packetgen::pg_data(datablock_s *m, const byte *crypt_packet_key, int maxsize)
{
    push_pid(PID_DATA);
    pushi(randombytes_random()); // just random int

    USHORT flags = 0;
    //if ( m->sent == 0 ) flags |= 1; // put time
    pushus( flags ); // 
    pushus( (USHORT)m->mt );
    pushll( m->delivery_tag );
    pushi( m->sent );
    pushi( m->len );
    //if (flags & 1) pushll( m->create_time );

    int sb = maxsize;
    if (m->left() < sb) sb = m->left();

    ASSERT( maxsize < 65536, "bad max send size" );

    pushus( (USHORT)sb );

    push( m->data() + m->sent, sb );

    m->sent += sb;

    encode(crypt_packet_key);

    log_auth_key("PID_DATA encoded", crypt_packet_key);
}

void packetgen::pg_delivered(u64 dtag, const byte *crypt_packet_key)
{
    push_pid(PID_DELIVERED);
    pushll( dtag );
    encode(crypt_packet_key);
    log_auth_key("PID_DELIVERED encoded", crypt_packet_key);
}

void packetgen::pg_sync(bool resync, const byte *crypt_packet_key)
{
    push_pid(PID_SYNC);
    byte *randombytes = push(sizeof(u64));;
    randombytes_buf(randombytes, sizeof(u64));
    
    pushll( now() );
    pushb( resync ? 1 : 0 );
    encode(crypt_packet_key);
    log_auth_key("PID_SYNC encoded", crypt_packet_key);
}

