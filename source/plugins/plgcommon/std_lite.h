#pragma once

#ifdef _WIN32
#include <stdint.h>
#endif // _WIN32

#include "zstrings/z_str_fake_std.h"

#ifdef _WIN32

#include <xstddef>
#include <xmemory0>

namespace std
{
    template<typename A, typename B, typename C> class basic_string {};

    inline size_t hash_value(const string& _Str)
    {	// hash string to size_t value
        return _Hash_seq((const unsigned char *)_Str.c_str(), _Str.size() * sizeof(char));
    }

    template<> struct hash<string>
    {
        size_t operator()(const string& _Str) const
        {
            return hash_value(_Str);
        }
    };
}
#endif

class byte_buffer
{
    uint8_t *m_data = nullptr;
    uint32_t  m_size = 0;        //bytes
    uint32_t  m_capacity = 0;    //allocated memory
public:
    size_t size() const { return m_size; }
    void resize(size_t nsz)
    {
        if (nsz <= m_capacity)
        {
            m_size = static_cast<uint32_t>(nsz);
        }
        else
        {
            size_t ncap = (nsz + 128) & ~127;
            uint8_t * nb = (uint8_t *)realloc(m_data, ncap);
            if (nb)
            {
                m_data = nb;
                m_capacity = static_cast<uint32_t>(ncap);
                m_size = static_cast<uint32_t>(nsz);
            }
        }
    }
    uint8_t * data() { return m_data; }
    const unsigned char * data() const { return m_data; }
    void clear()
    {
        m_size = 0;
    }
};

#include <unordered_map>
