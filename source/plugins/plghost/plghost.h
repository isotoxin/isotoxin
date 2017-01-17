#pragma once

struct ph_allocator : public std::allocator<char>
{
    static void *ma(size_t sz) { return dlmalloc(sz); }
    static void mf(void *ptr) { dlfree(ptr); }
    // std stuff

    void deallocate(pointer _Ptr, size_type)
    {	// deallocate object at _Ptr, ignore size
        dlfree(_Ptr);
    }

    pointer allocate(size_type _Count)
    {	// allocate array of _Count elements
        return (pointer)dlmalloc(_Count);
    }

    pointer allocate(size_type _Count, const void *)
    {	// allocate array of _Count elements, ignore hint
        return (pointer)dlmalloc(_Count);
    }

};


struct ipcw : public byte_buffer
{
    template<typename T> struct reservation
    {
        ipcw *me;
        size_t offset;
        reservation(ipcw *pipcw, size_t offs):me(pipcw), offset(offs) {}

        operator T&() { return *(T *)(me->data() + offset); }
    };

    ipcw(commands_e cmd = NOP_COMMAND)
    {
        clear(cmd);
    }

    void clear(commands_e cmd)
    {
        resize(sizeof(data_header_s));
        *(data_header_s *)data() = data_header_s(cmd);
    }

    template<typename T> T &add()
    {
        size_t offset = size();
        resize(offset + sizeof(T));
        return *(T *)(data() + offset);
    }
    template<typename T> reservation<T> reserve()
    {
        reservation<T> r(this, size());
        memset(&add<T>(),0,sizeof(T));
        return r;
    }

    void add( const void *d, aint sz )
    {
        size_t offset = size();
        resize(offset + sz);
        memcpy(data() + offset, d, sz);
    }

    void add( const std::asptr& s )
    {
        add<unsigned short>() = static_cast<unsigned short>(s.l);
        if (s.l) add( s.s, s.l );
    }

    void add(const std::wsptr& s)
    {
        add<unsigned short>() = static_cast<unsigned short>(s.l);
        if (s.l) add(s.s, s.l * sizeof(std::widechar));
    }

    template<typename T> struct adder_t
    {
        static void a(ipcw & w, const T &v)
        {
            static_assert(std::is_pod<T>::value, "T is not pod");
            w.add<T>() = v;
        }
    };
    template<> struct adder_t<std::asptr>
    {
        static void a(ipcw & w, const std::asptr &s)
        {
            w.add(s);
        }
    };
    template<> struct adder_t<std::wsptr>
    {
        static void a(ipcw & w, const std::wsptr &s)
        {
            w.add(s);
        }
    };

    template<typename T> ipcw & operator<<(const T &v) { adder_t<T>::a(*this, v); return *this; }

};