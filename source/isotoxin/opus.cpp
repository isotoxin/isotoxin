#include "isotoxin.h"

#include <ogg/ogg.h>
#include <opus.h>

typedef struct {
    int version;
    int channels; /* Number of channels: 1..255 */
    int preskip;
    ogg_uint32_t input_sample_rate;
    int gain; /* in dB S7.8 should be zero whenever possible */
    int channel_mapping;
    /* The rest is only used if channel_mapping != 0 */
    int nb_streams;
    int nb_coupled;
    unsigned char stream_map[255];
} OpusHeader;


typedef struct {
    unsigned char *data;
    int maxlen;
    int pos;
} Packet;

typedef struct {
    const unsigned char *data;
    int maxlen;
    int pos;
} ROPacket;

static int write_uint32(Packet *p, ogg_uint32_t val)
{
    if (p->pos>p->maxlen - 4)
        return 0;
    p->data[p->pos] = (val) & 0xFF;
    p->data[p->pos + 1] = (val >> 8) & 0xFF;
    p->data[p->pos + 2] = (val >> 16) & 0xFF;
    p->data[p->pos + 3] = (val >> 24) & 0xFF;
    p->pos += 4;
    return 1;
}

static int write_uint16(Packet *p, ogg_uint16_t val)
{
    if (p->pos>p->maxlen - 2)
        return 0;
    p->data[p->pos] = (val) & 0xFF;
    p->data[p->pos + 1] = (val >> 8) & 0xFF;
    p->pos += 2;
    return 1;
}

static int write_chars(Packet *p, const unsigned char *str, int nb_chars)
{
    int i;
    if (p->pos>p->maxlen - nb_chars)
        return 0;
    for (i = 0; i<nb_chars; i++)
        p->data[p->pos++] = str[i];
    return 1;
}

static int read_uint32(ROPacket *p, ogg_uint32_t *val)
{
    if (p->pos>p->maxlen - 4)
        return 0;
    *val = (ogg_uint32_t)p->data[p->pos];
    *val |= (ogg_uint32_t)p->data[p->pos + 1] << 8;
    *val |= (ogg_uint32_t)p->data[p->pos + 2] << 16;
    *val |= (ogg_uint32_t)p->data[p->pos + 3] << 24;
    p->pos += 4;
    return 1;
}

static int read_uint16(ROPacket *p, ogg_uint16_t *val)
{
    if (p->pos>p->maxlen - 2)
        return 0;
    *val = (ogg_uint16_t)p->data[p->pos];
    *val |= (ogg_uint16_t)p->data[p->pos + 1] << 8;
    p->pos += 2;
    return 1;
}

static int read_chars(ROPacket *p, unsigned char *str, int nb_chars)
{
    int i;
    if (p->pos>p->maxlen - nb_chars)
        return 0;
    for (i = 0; i<nb_chars; i++)
        str[i] = p->data[p->pos++];
    return 1;
}

int opus_header_parse(const unsigned char *packet, int len, OpusHeader *h)
{
    int i;
    char str[9];
    ROPacket p;
    unsigned char ch;
    ogg_uint16_t shortval;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    str[8] = 0;
    if (len<19)return 0;
    read_chars(&p, (unsigned char*)str, 8);
    if (memcmp(str, "OpusHead", 8) != 0)
        return 0;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->version = ch;
    if ((h->version & 240) != 0) /* Only major version 0 supported. */
        return 0;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->channels = ch;
    if (h->channels == 0)
        return 0;

    if (!read_uint16(&p, &shortval))
        return 0;
    h->preskip = shortval;

    if (!read_uint32(&p, &h->input_sample_rate))
        return 0;

    if (!read_uint16(&p, &shortval))
        return 0;
    h->gain = (short)shortval;

    if (!read_chars(&p, &ch, 1))
        return 0;
    h->channel_mapping = ch;

    if (h->channel_mapping != 0)
    {
        if (!read_chars(&p, &ch, 1))
            return 0;

        if (ch<1)
            return 0;
        h->nb_streams = ch;

        if (!read_chars(&p, &ch, 1))
            return 0;

        if (ch>h->nb_streams || (ch + h->nb_streams)>255)
            return 0;
        h->nb_coupled = ch;

        /* Multi-stream support */
        for (i = 0; i<h->channels; i++)
        {
            if (!read_chars(&p, &h->stream_map[i], 1))
                return 0;
            if (h->stream_map[i]>(h->nb_streams + h->nb_coupled) && h->stream_map[i] != 255)
                return 0;
        }
    }
    else {
        if (h->channels>2)
            return 0;
        h->nb_streams = 1;
        h->nb_coupled = h->channels>1;
        h->stream_map[0] = 0;
        h->stream_map[1] = 1;
    }
    /*For version 0/1 we know there won't be any more data
    so reject any that have data past the end.*/
    if ((h->version == 0 || h->version == 1) && p.pos != len)
        return 0;
    return 1;
}

int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len)
{
    int i;
    Packet p;
    unsigned char ch;

    p.data = packet;
    p.maxlen = len;
    p.pos = 0;
    if (len<19)return 0;
    if (!write_chars(&p, (const unsigned char*)"OpusHead", 8))
        return 0;
    /* Version is 1 */
    ch = 1;
    if (!write_chars(&p, &ch, 1))
        return 0;

    ch = h->channels;
    if (!write_chars(&p, &ch, 1))
        return 0;

    if (!write_uint16(&p, h->preskip))
        return 0;

    if (!write_uint32(&p, h->input_sample_rate))
        return 0;

    if (!write_uint16(&p, h->gain))
        return 0;

    ch = h->channel_mapping;
    if (!write_chars(&p, &ch, 1))
        return 0;

    if (h->channel_mapping != 0)
    {
        ch = h->nb_streams;
        if (!write_chars(&p, &ch, 1))
            return 0;

        ch = h->nb_coupled;
        if (!write_chars(&p, &ch, 1))
            return 0;

        /* Multi-stream support */
        for (i = 0; i<h->channels; i++)
        {
            if (!write_chars(&p, &h->stream_map[i], 1))
                return 0;
        }
    }

    return p.pos;
}

/*Write an Ogg page to a file pointer*/
static INLINE ts::aint oe_write_page(ogg_page *page, ts::blob_c &buf)
{
    ts::aint written = buf.size();
    
    buf.append_buf(page->header, page->header_len);
    buf.append_buf(page->body, page->body_len);
    
    return buf.size() - written;
}

/* This is just here because it's a convenient file linked by both opusenc and
opusdec (to guarantee this maps stays in sync). */
const int wav_permute_matrix[8][8] =
{
    { 0 },              /* 1.0 mono   */
    { 0,1 },            /* 2.0 stereo */
    { 0,2,1 },          /* 3.0 channel ('wide') stereo */
    { 0,1,2,3 },        /* 4.0 discrete quadraphonic */
    { 0,2,1,3,4 },      /* 5.0 surround */
    { 0,2,1,4,5,3 },    /* 5.1 surround */
    { 0,2,1,5,6,4,3 },  /* 6.1 surround */
    { 0,2,1,6,7,4,5,3 } /* 7.1 surround (classic theater 8-track) */
};

ts::blob_c opus_saver()
{
    ts::blob_c buf;
    ogg_stream_state   os;
    int serialno = rand();

    /*Initialize Ogg stream struct*/
    if (ogg_stream_init(&os, serialno) == -1)
        return buf;

    OpusHeader         header;
    ogg_packet         op;
    ogg_page           og;

    opus_int64         bytes_written = 0;
    opus_int64         pages_out = 0;
    int ret;

    /*Write header*/
    {
        /*The Identification Header is 19 bytes, plus a Channel Mapping Table for
        mapping families other than 0. The Channel Mapping Table is 2 bytes +
        1 byte per channel. Because the maximum number of channels is 255, the
        maximum size of this header is 19 + 2 + 255 = 276 bytes.*/
        unsigned char header_data[276];
        int packet_size = opus_header_to_packet(&header, header_data, sizeof(header_data));
        op.packet = header_data;
        op.bytes = packet_size;
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;
        ogg_stream_packetin(&os, &op);

        while ((ret = ogg_stream_flush(&os, &og))) {
            if (!ret)break;
            ret = oe_write_page(&og, buf);
            bytes_written += ret;
            pages_out++;
        }

        ts::asptr appn( CONSTASTR(APPNAME) );

        op.packet = (unsigned char *)appn.s;
        op.bytes = appn.l;
        op.b_o_s = 0;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 1;
        ogg_stream_packetin(&os, &op);
    }

    /* writing the rest of the Opus header packets */
    while ((ret = ogg_stream_flush(&os, &og))) {
        if (!ret)break;
        ret = oe_write_page(&og, buf);
        bytes_written += ret;
        pages_out++;
    }



}