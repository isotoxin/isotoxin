#include "toolset.h"

namespace ts
{

aint fifo_buf_c::read_data(void *dest, aint size)
{
    if (size < ((aint)buf[readbuf].size() - readpos))
    {
        memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        aint szleft = buf[readbuf].size() - readpos;
        memcpy(dest, buf[readbuf].data() + readpos, szleft);
        size -= szleft;
        buf[readbuf].clear(); // this buf fully extracted
        newdata = readbuf; // now it is for new data
        readbuf ^= 1; readpos = 0; // and read will be from other one
        if (size)
        {
            if (buf[readbuf].size()) // something in buf - extract it
                return szleft + read_data((char *)dest + szleft, size);
            newdata = readbuf = 0; // both bufs are empty, but data still required
        }
        return szleft;
    }
}


bool check_disk_file(const wsptr &name, const uint8 *data, aint size)
{
    buf_c b;
    if (!b.load_from_disk_file( name ) )
        return false;

    return blk_cmp(b.data(), data, size);
}

} // namespace ts