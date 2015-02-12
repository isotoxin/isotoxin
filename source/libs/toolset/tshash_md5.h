#pragma once

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
*/



namespace ts
{
    //class str_c;

#define md5_block_size 64
    
    struct md5_ctx
    {
        /* 512-bit buffer for leftovers */
        uint8 message[md5_block_size];
        uint64 length;
        uint32 state[4];
    };

    class md5_c
    {
        md5_ctx         m_context;
    public:
        template<typename B> md5_c(const buf_t<B> &buf) { reset(); update(buf.data(), buf.size()); done(); }
        md5_c() {reset();}
        ~md5_c() {};

        void update(const void* pach_source, uint len);
        void done(uint8 rslt[16]);
        void done();
        const unsigned char *result() const; // make sure done called
        void reset();
    };

} // namespace ts

