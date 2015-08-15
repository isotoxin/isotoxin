#include "toolset.h"

namespace ts
{

void md5_init (md5_ctx *);
void md5_update (md5_ctx *, const uint8 *, auint);
void md5_final (md5_ctx *, uint8 [16]);
void md5_final (md5_ctx *);

void md5_c::reset()
{
    md5_init(&m_context);
}

void md5_c::update(const void* pach_source, auint ulen)
{
    md5_update(&m_context, (const uint8*)pach_source, ulen);
}

void md5_c::done(uint8 rslt[16])
{
    md5_final(&m_context, rslt);
}
void md5_c::done()
{
    md5_final(&m_context);
}

const uint8 *md5_c::result() const
{
    return (const uint8 *)&m_context.state;
}


void md5_init(md5_ctx *ctx) 
{
    ctx->length = 0;

    /* initialize MD5 state */
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

/* First, define four auxiliary functions that each take as input
three 32-bit words and returns a 32-bit word.*/
#define MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define MD5_ROUND1(a, b, c, d, x, s, ac) { \
    (a) += MD5_F ((b), (c), (d)) + (x) + (unsigned)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
}
#define MD5_ROUND2(a, b, c, d, x, s, ac) { \
    (a) += MD5_G ((b), (c), (d)) + (x) + (unsigned)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
}
#define MD5_ROUND3(a, b, c, d, x, s, ac) { \
    (a) += MD5_H ((b), (c), (d)) + (x) + (unsigned)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
}
#define MD5_ROUND4(a, b, c, d, x, s, ac) { \
    (a) += MD5_I ((b), (c), (d)) + (x) + (unsigned)(ac); \
    (a) = ROTATE_LEFT ((a), (s)); \
    (a) += (b); \
}

static void md5_process_message_block(uint32 state[4], unsigned* x) {
    register unsigned a, b, c, d;
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];

    MD5_ROUND1(a, b, c, d, x[ 0],  7, 0xd76aa478);
    MD5_ROUND1(d, a, b, c, x[ 1], 12, 0xe8c7b756);
    MD5_ROUND1(c, d, a, b, x[ 2], 17, 0x242070db);
    MD5_ROUND1(b, c, d, a, x[ 3], 22, 0xc1bdceee);
    MD5_ROUND1(a, b, c, d, x[ 4],  7, 0xf57c0faf);
    MD5_ROUND1(d, a, b, c, x[ 5], 12, 0x4787c62a);
    MD5_ROUND1(c, d, a, b, x[ 6], 17, 0xa8304613);
    MD5_ROUND1(b, c, d, a, x[ 7], 22, 0xfd469501);
    MD5_ROUND1(a, b, c, d, x[ 8],  7, 0x698098d8);
    MD5_ROUND1(d, a, b, c, x[ 9], 12, 0x8b44f7af);
    MD5_ROUND1(c, d, a, b, x[10], 17, 0xffff5bb1);
    MD5_ROUND1(b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_ROUND1(a, b, c, d, x[12],  7, 0x6b901122);
    MD5_ROUND1(d, a, b, c, x[13], 12, 0xfd987193);
    MD5_ROUND1(c, d, a, b, x[14], 17, 0xa679438e);
    MD5_ROUND1(b, c, d, a, x[15], 22, 0x49b40821);

    MD5_ROUND2(a, b, c, d, x[ 1],  5, 0xf61e2562);
    MD5_ROUND2(d, a, b, c, x[ 6],  9, 0xc040b340);
    MD5_ROUND2(c, d, a, b, x[11], 14, 0x265e5a51);
    MD5_ROUND2(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    MD5_ROUND2(a, b, c, d, x[ 5],  5, 0xd62f105d);
    MD5_ROUND2(d, a, b, c, x[10],  9,  0x2441453);
    MD5_ROUND2(c, d, a, b, x[15], 14, 0xd8a1e681);
    MD5_ROUND2(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    MD5_ROUND2(a, b, c, d, x[ 9],  5, 0x21e1cde6);
    MD5_ROUND2(d, a, b, c, x[14],  9, 0xc33707d6);
    MD5_ROUND2(c, d, a, b, x[ 3], 14, 0xf4d50d87);
    MD5_ROUND2(b, c, d, a, x[ 8], 20, 0x455a14ed);
    MD5_ROUND2(a, b, c, d, x[13],  5, 0xa9e3e905);
    MD5_ROUND2(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    MD5_ROUND2(c, d, a, b, x[ 7], 14, 0x676f02d9);
    MD5_ROUND2(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    MD5_ROUND3(a, b, c, d, x[ 5],  4, 0xfffa3942);
    MD5_ROUND3(d, a, b, c, x[ 8], 11, 0x8771f681);
    MD5_ROUND3(c, d, a, b, x[11], 16, 0x6d9d6122);
    MD5_ROUND3(b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_ROUND3(a, b, c, d, x[ 1],  4, 0xa4beea44);
    MD5_ROUND3(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    MD5_ROUND3(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
    MD5_ROUND3(b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_ROUND3(a, b, c, d, x[13],  4, 0x289b7ec6);
    MD5_ROUND3(d, a, b, c, x[ 0], 11, 0xeaa127fa);
    MD5_ROUND3(c, d, a, b, x[ 3], 16, 0xd4ef3085);
    MD5_ROUND3(b, c, d, a, x[ 6], 23,  0x4881d05);
    MD5_ROUND3(a, b, c, d, x[ 9],  4, 0xd9d4d039);
    MD5_ROUND3(d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_ROUND3(c, d, a, b, x[15], 16, 0x1fa27cf8);
    MD5_ROUND3(b, c, d, a, x[ 2], 23, 0xc4ac5665);

    MD5_ROUND4(a, b, c, d, x[ 0],  6, 0xf4292244);
    MD5_ROUND4(d, a, b, c, x[ 7], 10, 0x432aff97);
    MD5_ROUND4(c, d, a, b, x[14], 15, 0xab9423a7);
    MD5_ROUND4(b, c, d, a, x[ 5], 21, 0xfc93a039);
    MD5_ROUND4(a, b, c, d, x[12],  6, 0x655b59c3);
    MD5_ROUND4(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    MD5_ROUND4(c, d, a, b, x[10], 15, 0xffeff47d);
    MD5_ROUND4(b, c, d, a, x[ 1], 21, 0x85845dd1);
    MD5_ROUND4(a, b, c, d, x[ 8],  6, 0x6fa87e4f);
    MD5_ROUND4(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_ROUND4(c, d, a, b, x[ 6], 15, 0xa3014314);
    MD5_ROUND4(b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_ROUND4(a, b, c, d, x[ 4],  6, 0xf7537e82);
    MD5_ROUND4(d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_ROUND4(c, d, a, b, x[ 2], 15, 0x2ad7d2bb);
    MD5_ROUND4(b, c, d, a, x[ 9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void md5_update(md5_ctx *ctx, const uint8* msg, auint size)
{
    
    unsigned index = (unsigned)ctx->length & 63;
    uint left;
    ctx->length += size;

    /* Try to fill partial block */
    if (index) {
        left = md5_block_size - index;
        if (size < left) {
            memcpy(ctx->message + index, msg, size);
            return;
        } else {
            memcpy(ctx->message + index, msg, left);
            md5_process_message_block(ctx->state, (unsigned*)ctx->message);
            msg += left;
            size -= left;
        }
    }
    while (size >= md5_block_size) {
        if( (3&(uint64_t)msg)==0 ) {
            md5_process_message_block(ctx->state, (unsigned*)msg);
        } else {
            memcpy(ctx->message, msg, md5_block_size);
            md5_process_message_block(ctx->state, (unsigned*)ctx->message);
        }

        msg += md5_block_size;
        size -= md5_block_size;
    }
    if(size) {
        /* save leftovers */
        memcpy(ctx->message, msg, size);
    }
}

void md5_final(md5_ctx *ctx)
{
    unsigned index = (unsigned)ctx->length & 63;
    /* pad message and run for last block */
    ctx->message[ index++ ] = 0x80;

    /* if no room left in the message to store 64-bit message length */
    if(index>56) {
        /* then pad the rest with zeros and process it */
        while(index < 64) {
            ctx->message[index++] = 0;
        }
        md5_process_message_block(ctx->state, (unsigned*)ctx->message);
        index = 0;
    }
    while(index < 56) {
        ctx->message[index++] = 0;
    }
    ((uint64_t*)(&(ctx->message[56])))[0] = (ctx->length << 3);
    md5_process_message_block(ctx->state, (unsigned*)ctx->message);
}

void md5_final(md5_ctx *ctx, uint8 result[16])
{
    md5_final(ctx);
    memcpy(result, &ctx->state, 16);
}

} // namespace ts