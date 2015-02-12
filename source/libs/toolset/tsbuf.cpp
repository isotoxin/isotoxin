#include "toolset.h"

namespace ts
{

aint fifo_buf_c::read_data(void *dest, aint size)
{
    if (size < ((int)buf[readbuf].size() - readpos))
    {
        memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        int szleft = buf[readbuf].size() - readpos;
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




} // namespace ts