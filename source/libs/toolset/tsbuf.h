#pragma once

namespace ts
{

struct bufmod_nothing
{
    bufmod_nothing() {}
    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        return newl;
    }
};

struct bufmod_preserve
{
    aint real_len;
    bufmod_preserve(aint real_len) :real_len(real_len) {}
    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        if (newb != oldb)
        {
            if (newl <= oldl) // decrease len - copy content
                memcpy(newb, oldb, newl);
            else
                memcpy(newb, oldb, oldl);
        }
        return real_len;
    }
};

struct bufmod_fill
{
    aint offset, sz;
    uint8 c;
    bufmod_fill(aint offset, aint sz, uint8 c) :offset(offset), sz(sz), c(c) {}

    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        if (newb != oldb)
        {
            memcpy(newb, oldb, offset);
            memcpy(newb + offset + sz, oldb + offset + sz, oldl - offset - sz);
        }
        for (int i = offset; i < (offset + sz); ++i)
            newb[i] = c;
        return newl;
    }
};

struct bufmod_expand
{
    aint offset, sz;
    bufmod_expand(aint offset, aint sz) :offset(offset), sz(sz) {}

    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        ASSERT( oldl + sz == newl );
        if (newb != oldb)
        {
            memcpy(newb, oldb, offset);
            memcpy(newb + offset + sz, oldb + offset, oldl - offset);
        } else
        {
            blk_copy_back(oldb + offset + sz, oldb + offset, oldl - offset);
        }
        return newl;
    }
};

struct bufmod_cut
{
    aint offset, sz;
    bufmod_cut(aint offset, aint sz) :offset(offset), sz(sz) {}

    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        ASSERT(oldl - sz == newl);
        if (newb != oldb)
            memcpy(newb, oldb, offset);
        memcpy(newb + offset, oldb + offset + sz, oldl - offset - sz);

        return newl;
    }
};

struct bufmod_cuttail
{
    aint offset, sz;
    bufmod_cuttail(aint offset, aint sz) :offset(offset), sz(sz) {}

    aint operator()(uint8 *newb, aint newl, uint8 *oldb, aint oldl) const
    {
        ASSERT(oldl - sz == newl);
        ASSERT(offset < 0 || offset + sz <= newl);

        if (offset >= 0)
        {
            memcpy(newb + offset, oldb + newl, sz);
            if (newb != oldb)
            {
                memcpy(newb, oldb, offset);
                memcpy(newb + offset + sz, oldb + offset + sz, newl - offset - sz);
            }
        } else if (newb != oldb)
        {
            memcpy(newb, oldb, newl);
        }

        return newl;
    }
};


template<aint GRANULA, typename ALLOCATOR> struct BUFFER_RESIZABLE : public ALLOCATOR
{
    BUFFER_RESIZABLE(aint cap):m_data(nullptr), m_size(0), m_capacity(cap)
    {
        if (m_capacity > 0)
            m_data = (uint8 *)ma( m_capacity );
        else if (GRANULA > 0)
        {
            m_data = (uint8 *)ma(GRANULA);
            m_capacity = GRANULA;
        }
    }
    BUFFER_RESIZABLE( const BUFFER_RESIZABLE &b ):m_data(nullptr), m_size(b.size()), m_capacity(b.size())
    {
        m_data = (uint8 *)ma( b.size() );
        memcpy(m_data, b(), b.size());
    }
    BUFFER_RESIZABLE(BUFFER_RESIZABLE &&b) :m_data(b.m_data), m_size(b.m_size), m_capacity(b.m_capacity)
    {
        b.m_data = nullptr;
        b.m_capacity = 0;
        b.m_size = 0;
    }
    ~BUFFER_RESIZABLE()
    {
        mf(m_data);
    }

    uint8   *m_data;
    int     m_size;        //bytes
    int     m_capacity;    //allocated memory

    uint8  *operator()() { return m_data; }
    const uint8  *operator()() const { return m_data; }
    aint    size() const { return m_size; };
    aint    cap() const { return m_capacity; };
    bool writable() const {return true;}

    void operator=(BUFFER_RESIZABLE && tb)
    {
        SWAP(m_data, tb.m_data);
        SWAP(m_size, tb.m_size);
        SWAP(m_capacity, tb.m_capacity);
    }
    void operator=(const BUFFER_RESIZABLE & tb)
    {
        fitcapacity(tb.size());
        memcpy(m_data, tb.m_data, tb.size());
        m_size = tb.size();
    }


    void fitcapacity(aint newsz)
    {
        if (newsz > m_capacity)
        {
            m_capacity = newsz + GRANULA;
            m_data = (uint8 *)mra(m_data, sizeof(uint8)*m_capacity);
        }
    }

    void clear()
    {
        m_size = 0; // nothing special
    }

    template<typename MODER> void change(aint newlen, const MODER &moder)
    {
        fitcapacity(newlen);
        aint fixlen = moder(m_data, newlen, m_data, m_size);
        m_size = fixlen;
    }
};

template<aint GRANULA, typename ALLOCATOR> struct BUFFER_RESIZABLE_COPY_ON_DEMAND : public ALLOCATOR
{
    struct core_s
    {
        int     m_size;        //bytes
        int     m_capacity;    //allocated memory
        int     m_ref;
        bool release()
        {
            --m_ref;
            return m_ref == 0;
        }
    } *m_core;

    void build(aint cap)
    {
        m_core = (core_s *)ma(cap + sizeof(core_s));
        m_core->m_ref = 1;
        m_core->m_size = 0;
        m_core->m_capacity = cap;
    }

    BUFFER_RESIZABLE_COPY_ON_DEMAND(aint cap) :m_core(nullptr)
    {
        if (cap > 0)
            build(cap);
        else if (GRANULA > 0)
            build(GRANULA);
    }
    BUFFER_RESIZABLE_COPY_ON_DEMAND(const BUFFER_RESIZABLE_COPY_ON_DEMAND &b):m_core(b.m_core)
    {
        //if (m_core) if (m_core->release()) mf(m_core);
        if (m_core) ++m_core->m_ref;
    }
    BUFFER_RESIZABLE_COPY_ON_DEMAND(BUFFER_RESIZABLE_COPY_ON_DEMAND &&b) :m_core(b.m_core)
    {
        b.m_core = nullptr;
    }
    ~BUFFER_RESIZABLE_COPY_ON_DEMAND()
    {
        if (m_core && m_core->release()) mf(m_core);
    }

    uint8  *operator()() { return m_core ? ((uint8*)(m_core+1)) : nullptr; }
    const uint8  *operator()() const { return m_core ? ((uint8*)(m_core+1)) : nullptr; }
    aint    size() const { return m_core ? m_core->m_size : 0; };
    aint    cap() const { return m_core ? m_core->m_capacity : 0; };
    bool writable() const { return m_core == nullptr || m_core->m_ref == 1; }

    void operator=(BUFFER_RESIZABLE_COPY_ON_DEMAND && tb)
    {
        SWAP(m_core, tb.m_core);
    }
    void operator=(const BUFFER_RESIZABLE_COPY_ON_DEMAND & tb)
    {
        if (m_core) if (m_core->release()) mf(m_core);
        m_core = tb.m_core;
        if (m_core) ++m_core->m_ref;
    }

    void clear()
    {
        if (m_core)
        {
            if (m_core->m_ref == 1)
            {
                m_core->m_size = 0; // nothing special
            } else
            {
                --m_core->m_ref;
                m_core = nullptr;
            }
        }
    }

    template<typename MODER> void change(aint newlen, const MODER &moder)
    {
        if (m_core == nullptr)
        {
            build(newlen + GRANULA);
            aint fixlen = moder((uint8 *)(m_core+1), newlen, nullptr, 0);
            m_core->m_size = fixlen;
            return;
        }
        if (m_core->m_ref > 1)
        {
            core_s *oldcore = m_core;
            build(newlen + GRANULA);
            m_core->m_size = oldcore->m_size;
            aint fixlen = moder((uint8 *)(m_core+1), newlen, (uint8 *)(oldcore+1), oldcore->m_size);
            m_core->m_size = fixlen;
            --oldcore->m_ref;
            return;
        }
        if (newlen > m_core->m_capacity)
        {
            m_core->m_capacity = newlen + GRANULA;
            m_core = (core_s *)mra(m_core, sizeof(uint8)*m_core->m_capacity + sizeof(core_s));
        }

        aint fixlen = moder((uint8 *)(m_core + 1), newlen, (uint8 *)(m_core + 1), m_core->m_size);
        m_core->m_size = fixlen;
    }
};


template<typename CORE> class buf_t
{
    CORE core;

public:

    explicit buf_t( aint prealloc_capacity = 0, bool setsize = false ):core( prealloc_capacity ) { if (setsize) set_size(prealloc_capacity, false); }
    buf_t(const buf_t<CORE>&b) :core(b.core) {} // same core
    buf_t(buf_t<CORE>&&b) :core(std::move(b.core)) {} // same core
    template<typename CORE2> explicit buf_t( const buf_t<CORE2>&b ):core( b.size() ) // other core
    {
        TS_STATIC_CHECK( (!std::is_same<CORE, CORE2>::value), "CORE and CORE2 are same" );
        set_size( b.size() );
        if (ASSERT(core.writable()))
            memcpy( core(), b.data(), b.size() );
    }
    ~buf_t() {}

    explicit operator bool() const { return size() > 0; }

    INLINE aint    size() const { return core.size(); };
    INLINE aint    capacity() const { return core.cap(); };

    INLINE aint    size16() const { return core.size() / sizeof(uint16); };
    INLINE aint    size32() const { return core.size() / sizeof(uint32); };

    INLINE uint8  *data() { return core(); }
    INLINE const uint8 *data() const { return core(); }

    INLINE uint16   *data16() { return (uint16 *)core(); }
    INLINE const uint16 *data16() const { return (const uint16 *)core(); }

    INLINE uint32   *data32() { return (uint32 *)core(); }
    INLINE const uint32 *data32() const { return (const uint32 *)core(); }

    const asptr cstr() const { return asptr((const char*)core(), core.size()); }

    bool inside(const void *ptr, int sz) const
    {
        return ptr >= data() && (((uint8 *)ptr)-data()+sz) <= size();
    }

    template <typename T> const T *tbegin() const { return ((T *)core()); }
    template <typename T> T *tbegin() { return ((T *)core()); }
    template <typename T> const T *tend() const { return ((T *)(core() + core.size())); }
    template <typename T> const T *tget(aint index) const { ASSERT(index * sizeof(T) <= (size()-sizeof(T)) ); return tbegin<T>()+index; }
    template <typename T> T *tget(aint index) { ASSERT(index * sizeof(T) <= (size()-sizeof(T)) ); return tbegin<T>()+index; }

    bool is_writable() const { return core.writable(); }

    uint32    crc() const
    {
        return CRC32(core(), core.size());
    }


    void    clear() { core.clear(); }

    void fill(uint8 filler)
    {
        core.change(core.size(), bufmod_fill(0, core.size(), filler));
    }

    template< typename CORE2 > void append_buf(const buf_t<CORE2>&b)
    {
        append_buf( b.data(), b.size() );
    }

    void    append_buf(const void *data, aint datasize)
    {
        aint oldsize = core.size();
        aint newsize = oldsize + datasize;
        core.change( newsize, bufmod_preserve(newsize) );
        memcpy(core() + oldsize, data, datasize);
    }
    void    append_s(const asptr& s)
    {
        append_buf(s.s, s.l);
    }

    template <class T> T &tappend(const T &t, aint clone = 1)
    {
        aint oldsize = core.size();
        aint newsize = oldsize + sizeof(T) * clone;
        core.change(newsize, bufmod_preserve(newsize));

        // TODO : optimize memcpy from already copied
        for (T * ft = (T *)(data() + oldsize), *et = (T *)(data() + newsize); ft < et; ++ft)
            memcpy(ft, &t, sizeof(T));

        return *(T *)(data() + oldsize);
    }

    template <typename T> void tpush(const T &t)
    {
        tappend(t, 1);
    }

    template <typename T> T tpop()
    {
        ASSERT(core.size() >= sizeof(T));
        make_pod<T> t;
        memcpy( &t.get(), tend<T>()-1, sizeof(T) );
        set_size( size() - sizeof(T), true );
        return t;
    }


    uint8 *expand(aint offset, aint sz)
    {
        if (ASSERT(offset <= core.size()))
        {
            core.change(core.size() + sz, bufmod_expand(offset, sz));
            return core() + offset;
        }
        return nullptr;
    }

    uint8 * expand(aint sz)
    {
        aint oldsize = core.size();
        aint newsize = oldsize + sz;
        core.change(newsize, bufmod_preserve(newsize));
        return core() + oldsize;
    }

    buf_t &operator=(const buf_t & tb)
    {
        core = tb.core;
        return *this;
    }
    buf_t &operator=(buf_t && tb)
    {
        core=std::move(tb.core);
        return *this;
    }

    void set_size(aint sz, bool preserve_content = true)
    {
        aint newsize = sz;
        if (preserve_content)
            core.change(newsize, bufmod_preserve(sz));
        else
            core.change(newsize, bufmod_nothing());
    }

    void cut( aint offset, aint sz )
    {
        if (ASSERT( offset + sz <= core.size() ))
            core.change(core.size() - sz, bufmod_cut(offset,sz));
    }

    void cut_tail( aint sz, aint offset = -1 )
    {
        // cut sz from tail
        // but
        // if offset >= 0, copy sz tail to offset
        if (offset == core.size() - sz) offset = -1; // classic fast remove last - no need to copy
        core.change(core.size() - sz, bufmod_cuttail(offset, sz));
    }

    void *use( aint sz ) // very special method - use portion of preallocated buffer to simulate fast allocation
    {
        aint oldsz = core.size();
        if (ASSERT((oldsz + sz) <= core.cap())) // check that preallocated bytes enough for requested sz
        {
#ifndef _FINAL
            void *r = core() + oldsz;
#endif
            set_size( core.size() + sz, true );
            if ( ASSERT((oldsz == 0 || r == core() + oldsz) && core.writable()) ) // check: 1st resize or no resize
                return core() + oldsz;
        }
        return nullptr;
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    // sort

    /// Quick-sort the array.
    /// Parameter is functional object
    /// Spizheno u skyfallen
    template < typename T, typename F > bool tsort(F &comp, int ileft = 0, int irite = -1)
    {
        TS_STATIC_CHECK(is_movable<T>::value, "movable type expected!");
        
        if (!core.writable()) set_size(size(),true);
        ASSERT(core.writable());

        aint icnt = tend<T>() - tbegin<T>();
        if (icnt <= 1) return false;

        if (ileft < 0) ileft = icnt + ileft;
        if (irite < 0) irite = icnt + irite;
        ASSERT(ileft <= irite, "incorrect range for buf_granula_depricated::sort");

        aint st0[32], st1[32];
        aint a, b, k, i, j;

        bool sorted = false;
        make_pod<T> temp;

        k = 1;
        st0[0] = ileft;
        st1[0] = irite;
        while (k != 0)
        {
            k--;
            i = a = st0[k];
            j = b = st1[k];
            int center = (a + b) / 2;
            T *x = tget<T>(center);
            while (a < b)
            {
                while (i <= j)
                {
                    while (comp(tget<T>(i), x)) i++;
                    while (comp(x, tget<T>(j))) j--;
                    if (i <= j)
                    {
                        memcpy(&temp.get(), tget<T>(i), sizeof(T));
                        memcpy(tget<T>(i), tget<T>(j), sizeof(T));
                        memcpy(tget<T>(j), &temp.get(), sizeof(T));

                        sorted = true;

                        if (i == center)
                        {
                            center = j;
                            x = tget<T>(center);
                        }
                        else if (j == center)
                        {
                            center = i;
                            x = tget<T>(center);
                        }

                        i++;
                        j--;
                    }
                }
                if (j - a >= b - i)
                {
                    if (a < j)
                    {
                        st0[k] = a;
                        st1[k] = j;
                        k++;
                    }
                    a = i;
                }
                else
                {
                    if (i < b)
                    {
                        st0[k] = i;
                        st1[k] = b;
                        k++;
                    }
                    b = j;
                }
            }
        }
        return sorted;
    }

    template<typename T> bool tsort(int ileft = 0, int irite = -1)
    {
        struct
        {
            bool operator() (const T * x1, const T * x2) { return (*x1) < (*x2); }
        } comparator;
        return tsort<T>(comparator, ileft, irite);
    }


    //////////////////////////////////////////////////////////////////////////////////////////////
    // bit ops

    bool get_bit(aint index) const
    {
        if (index >= aint(core.size() * 8)) return false;
        return 0 != (core()[index >> 3] & (1 << (index & 7)));
    }
    void set_bit(aint index, bool b)
    {
        aint oldsz = core.size();
        aint newsz = (index + 8) >> 3;
        if (newsz > oldsz)
        {
            set_size(newsz, true);
            if (ASSERT(core.writable()))
                memset(core() + oldsz, 0, newsz - oldsz);
        } else
        {
            set_size(oldsz, true);
        }
        if (ASSERT(core.writable()))
        {
            if (b) core()[index >> 3] |= (1 << (index & 7));
            else core()[index >> 3] &= ~(uint8(1 << (index & 7)));
        }
    }
    bool is_any_bit() const
    {
        int cnt = core.size() / sizeof(auint);
        for (int i = 0; i < cnt; ++i)
        {
            if (((auint *)core())[i] != 0) return true;
        }
        cnt = core.size() & (sizeof(auint) - 1);
        for (int i = 0; i<cnt; ++i)
        {
            if (core()[core.size() - cnt + i]) return true;
        }
        return false;
    }
    template<typename CORE2> bool or_bits(const buf_t<CORE2> &bset)
    {
        bool changed = false;
        aint oldsz = core.size();
        if (bset.size() > oldsz)
        {
            set_size(bset.size(), true);
            if (ASSERT(core.writable()))
                memset(core() + oldsz, 0, core.size() - oldsz);
        } else
        {
            set_size(core.size(), true);
        }

        if (ASSERT(core.writable()))
        {
            aint cnt = tmin(core.size(), bset.size()) / sizeof(auint);
            for (int i = 0; i < cnt; ++i)
            {
                auint old = tbegin<auint>()[i];
                auint n = old | bset.tbegin<auint>()[i];
                if (n != old)
                {
                    changed = true;
                    tbegin<auint>()[i] = n;
                }
            }
            int index = cnt*sizeof(auint);
            cnt = tmin(core.size() - index, bset.size() - index);
            for (int i = 0; i < cnt; ++i)
            {
                uint8 old = core()[index + i];
                uint8 n = old | bset.data()[index + i];
                if (n != old)
                {
                    changed = true;
                    data()[index + i] = n;
                }
            }
        }
        return changed;
    }
    template<typename CORE2> bool is_any_common_bit(const buf_t<CORE2> &bset) const
    {
        int cnt = tmin(core.size(), bset.size()) / sizeof(auint);
        for (int i = 0; i < cnt; ++i)
        {
            if (0 != (((auint *)core())[i] & bset.tbegin<auint>()[i])) return true;
        }
        int index = cnt*sizeof(auint);
        cnt = tmin(m_size - index, bset.size() - index);
        for (int i = 0; i < cnt; ++i)
        {
            if (0 != (core()[index + i] & bset.data()[index + i])) return true;
        }
        return false;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////

    void save_to_file(const wsptr &name, int disp = 0) const
    {
        HANDLE f = CreateFileW(tmp_wstr_c(name), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE)
        {
            DWORD w;
            WriteFile(f, core() + disp, core.size() - disp, &w, nullptr);
            CloseHandle(f);
        }
    }

    bool    load_from_disk_file(const wsptr &fn, bool text = false)
    {
        set_size(0, false);

        HANDLE hand = CreateFileW(tmp_wstr_c(fn), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (hand != INVALID_HANDLE_VALUE)
        {
            aint size = GetFileSize(hand, nullptr);
            aint m_size = size; if (text) m_size += 2;
            set_size(m_size, false);
            DWORD r;
            ReadFile(hand, core(), size, &r, nullptr);
            CloseHandle(hand);
            if (text && ASSERT(core.writable()))
            {
                core()[size] = 0;
                core()[size + 1] = 0;
            }
            return ((aint)r == size);
        }
        return false;
    }

    bool    load_from_file(int reservebefore, const wsptr &fn, int reserveafter)
    {
        set_size(0, false);

        typedef clean_type<decltype(this)>::type mytype;
        struct bw : public buf_wrapper_s
        {
            mytype *b;
            bw(mytype * b) :b(b) {}
            /*virtual*/ void * alloc(aint sz) override
            {
                b->set_size(sz, false);
                return b->data();
            }
        } me(this);

        return g_fileop->load(fn, me, reservebefore, reserveafter);
    }

    bool    load_from_file(const wsptr &fn)
    {
        return load_from_file(0, fn, 0);
    }

    bool    load_from_text_file(const wsptr &fn)
    {
        bool log = load_from_file(0, fn, 1);
        if (ASSERT(core.writable()))
            core()[size() - 1] = 0;
        return log;
    }

};


template <typename T, typename CORE = BUFFER_RESIZABLE<sizeof(T) * 32, TS_DEFAULT_ALLOCATOR> > class tbuf_t : public buf_t< CORE > // only simple types can be used due memcpy
{
    static const size_t tsize = sizeof(T);
    TS_STATIC_CHECK(is_movable<T>::value, "wrong type for tbuf_t! use array_c");
    aint    size() const { return __super::size(); };
public:

    typedef T type;

    explicit tbuf_t(aint prealloc_capacity_items = 0) :buf_t(prealloc_capacity_items * sizeof(T))
    {
    }
    template<typename CORE2> explicit tbuf_t(const buf_t<CORE2> & buf) :buf_t(buf)
    {
        ASSERT( (buf.size() & sizeof(T)) == 0 );
    }

    aint    byte_size() const { return __super::size(); };

    /*
    tbuf_t(type_buf_c && tb)
    {
        SWAP(m_data, tb.m_data);
        SWAP(m_size, tb.m_size);
        SWAP(m_capacity, tb.m_capacity);
    }
    tbuf_t(const T *t, int count)
    {
        uint sz = sizeof(T) * count;
        set_size(sz);
        memcpy(get_uint8_array(), t, sz);
    }
    tbuf_t(std::initializer_list<T> list) :buf_granula_depricated(0)
    {
        set_size(sizeof(T) * list.size());
        aint i = 0;
        for (const T &el : list) set(i++, el);
    }
    */

    void operator=(std::initializer_list<T> list)
    {
        set_size(sizeof(T) * list.size(), false);
        aint i = 0;
        for (const T &el : list) set(i++, el);
    }

    void set(std::initializer_list<T> list)
    {
        set_size(sizeof(T) * list.size(), false);
        aint i = 0;
        for (const T &el : list) set(i++, el);
    }

    void set(const T *t, aint count)
    {
        aint sz = sizeof(T) * count;
        set_size(sz, false);
        memcpy(data(), t, sz);
    }

    bool present(const T &t) const
    {
        for( const T&x : *this )
            if (x == t) return true;
        return false;
    }

    aint  get_index(const T &t)
    {
        for (const T&x : *this)
            if (x == t) return &x - tbegin<T>();

        tappend<T>(t, 1);
        return tend<T>() - tbegin<T>() - 1;
    }

    aint  get_index_replace(const T &t, const T &f)
    {
        for (T&x : *this)
            if (x == t) return &x - tbegin<T>();
                else if (x == f) { x = f; return &x - tbegin<T>(); }

        tappend<T>(t, 1);
        return tend<T>() - tbegin<T>() - 1;
    }

    aint  find_index(const T &t) const
    {
        for (const T&x : *this)
            if (x == t) return &x - tbegin<T>();

        return -1;
    }

    template<typename SOME> aint  find_index(const SOME &t) const
    {
        for (const T&x : *this)
            if (x == t) return &x - tbegin<T>();

        return -1;
    }

    void    insert(aint index, const T &t)
    {
        T * tt = (T*)expand(index * sizeof(T), sizeof(T));
        *tt = t;
    }

    template< typename CORE2 > void insert(aint index, const tbuf_t<T, CORE2> &t)
    {
        T * tt = (T*)expand(index * sizeof(T), t.size());
        memcpy(tt, t.data(), t.size());
    }

    struct default_comparator
    {
        int operator()(const T&t1, const T&t2) const
        {
            if (t1 == t2) return 0;
            if (t1 < t2) return 1;
            return -1;
        }
    };

    template <typename F> bool  insert_sorted_uniq(const T &t, F &comp)
    {
        int index;
        if (find_index_sorted(index, t, comp))
        {
            return false;
        }
        else
        {
            insert(index, t);
        }
        return true;
    }

    template <typename F> int  set_sorted_uniq(const T &t, F &comp)
    {
        int index;
        if (find_index_sorted(index, t, comp))
        {
        }
        else
        {
            insert(index, t);
        }
        return index;
    }


    template<typename F> bool find_index_sorted(int &index, const T &t, F &comp) const
    {
        if (count() == 0)
        {
            index = 0;
            return false;
        }
        if (count() == 1)
        {
            int cmp = comp(get(0), t);
            index = cmp <= 0 ? 0 : 1;
            return cmp == 0;
        }


        int left = 0;
        int rite = count();

        int test;
        do
        {
            test = (rite + left) >> 1;

            int cmp = comp(get(test), t);
            if (cmp == 0)
            {
                index = test;
                return true;
            }
            if (cmp < 0)
            {
                // do left
                rite = test;
            }
            else
            {
                // do rite
                left = test + 1;
            }
        } while (left < (rite - 1));

        if (left >= (int)count())
        {
            index = left;
            return false;
        }

        int cmp = comp(get(left), t);
        index = cmp <= 0 ? left : left + 1;
        return cmp == 0;
    }

    void set_count(aint cnt, bool preserve_content = true)
    {
        set_size(cnt * sizeof(T), preserve_content);
    }
    void reserve(aint cnt, bool preserve_content = false)
    {
        aint osz = size();
        set_size(cnt * sizeof(T), preserve_content);
        set_size(osz, preserve_content);
    }

    aint count() const
    {
        const T * b1 = tbegin<T>();
        const T * b2 = tend<T>();
        return (aint)(b2 - b1);
    }

    const T & last(const T &def) const
    {
        if (count() == 0) return def;
        return tbegin<T>()[count() - 1];
    }

    T & last()
    {
        ASSERT(is_writable());
        if (count() == 0) return make_dummy< T >();
        return tbegin<T>()[count() - 1];
    }


    const T & get(aint index) const
    {
        ASSERT(index >= 0 && index < count());
        return tbegin<T>()[index];
    }
    T & get(aint index)
    {
        ASSERT(is_writable());
        ASSERT(index >= 0 && index < count());
        return tbegin<T>()[index];
    }

    const T * get() const
    {
        return tbegin<T>();
    }

    void set(aint index, const T &t)
    {
        ASSERT(is_writable()); // TODO: asserted here, use overwrite base method. oops. you have to write it too :)
        ASSERT(index >= 0 && index < count());
        tbegin<T>()[index] = t;
    }
    void add(const T &t)
    {
        tappend<T>(t, 1);
    }
    template< typename CORE2 > void append_buf(const tbuf_t<T, CORE2> &t)
    {
        __super::append_buf(t.data(), t.size());
    }

    T & add()
    {
        return *(T*)expand(sizeof(T));
    }

    void swap_unsafe(aint index1, aint index2)
    {
        ASSERT(is_writable());
        ASSERT(index1 >= 0 && index1 < count());
        ASSERT(index2 >= 0 && index2 < count());

        T * b = tbegin<T>();

        T temp = b[index1];
        b[index1] = b[index2];
        b[index2] = temp;

    }

    T pop() { return tpop<T>(); }
    void  push(const T &d) {tpush<T>(d);}

    bool sort(int ileft = 0, int irite = -1) { return tsort<T>(ileft, irite); }

    void remove_slow(aint index, aint co = 1)
    {
        ASSERT(index >= 0 && (index + co) <= count());
        cut( index * sizeof(T), co * sizeof(T) );
    }

    void remove_fast(aint index)
    {
        ASSERT(index >= 0 && index < count());
        cut_tail( sizeof(T), index * sizeof(T) );
    }

    bool find_remove_fast(const T &d)
    {
        aint i = find_index(d);
        if (i >= 0)
        {
            remove_fast(i);
            return true;
        }
        return false;
    }

    aint find_remove_slow(const T &d)
    {
        aint i = find_index(d);
        if (i >= 0) remove_slow(i);
        return i;
    }

    void move_up_unsafe(int index)
    {
        swap_unsafe(index, index - 1);
    }
    void move_down_unsafe(int index)
    {
        swap_unsafe(index, index + 1);
    }

    array_wrapper_c<const T> array() const { return array_wrapper_c<const T>(get(), count()); }

    // begin() end() semantics

    T * begin() { ASSERT(is_writable()); return tbegin<T>(); }
    const T *begin() const { return tbegin<T>(); }

    T *end() { return const_cast<T *>(tend<T>()); }
    const T *end() const { return tend<T>(); }

    // math
    T average() const
    {
        T sum = { 0 };
        if (count() == 0) return sum;
        for (const T & t : *this)
            sum += t;
        return sum / count();
    }
    T sum() const
    {
        T sum = { 0 };
        for (const T & t : *this)
            sum += t;
        return sum;
    }
};


typedef buf_t< BUFFER_RESIZABLE<0, TS_DEFAULT_ALLOCATOR> > buf0_c;
typedef buf_t< BUFFER_RESIZABLE<1024, TS_DEFAULT_ALLOCATOR> > buf_c;

typedef buf_t< BUFFER_RESIZABLE<1024, TMP_ALLOCATOR> > tmp_buf_c;

template<typename T> using tbuf0_t = tbuf_t< T, BUFFER_RESIZABLE<0, TS_DEFAULT_ALLOCATOR> >;
template<typename T> using tmp_tbuf_t = tbuf_t< T, BUFFER_RESIZABLE<0, TMP_ALLOCATOR> >;

typedef buf_t< BUFFER_RESIZABLE_COPY_ON_DEMAND<0, TS_DEFAULT_ALLOCATOR> > blob_c;


template <uint block_size> class block_buf_c
{
    struct block_s
    {
        block_s *next;
        uint cursize;
        uint8 data[ block_size ];
    };
    block_s *m_first;
    block_s *m_current;

public:
    block_buf_c(bool allocate_1st_block = false):m_first( nullptr ), m_current( nullptr )
    {
        if (allocate_1st_block)
        {
            m_first = (block_s *)MM_ALLOC( sizeof(block_s) );
            m_first->cursize = 0;
            m_first->next = nullptr;
            m_current = m_first;
        }
    }
    ~block_buf_c()
    {
        block_s *z = m_first;
        for (;z;)
        {
            block_s *zn = z->next;
            MM_FREE( z );
            z = zn;
        }
    }

    void clear(void)
    {
        block_s *z = m_first;
        for (;z;)
        {
            block_s *zn = z->next;
            MM_FREE( z );
            z = zn;
        }
        m_first = nullptr;
        m_current = nullptr;
    }

    void *alloc( uint szz )
    {
        if ( m_first == nullptr )
        {
            m_first = (block_s *)MM_ALLOC( tmax( sizeof(block_s), szz + (sizeof(block_s)-block_size) ) );
            m_first->cursize = szz;
            m_first->next = nullptr;
            m_current = m_first;
            return m_current->data;
        } else
        {
            int available = block_size - m_current->cursize;
            if ( available < (int)szz )
            {
                m_current->next = (block_s *)MM_ALLOC( tmax( sizeof(block_s), szz + (sizeof(block_s)-block_size) ) );
                m_current->next->cursize = szz;
                m_current->next->next = nullptr;
                m_current = m_current->next;
                return m_current->data;
            }

            uint scs = m_current->cursize;
            m_current->cursize += szz;
            return m_current->data + scs;
        }
    }

};


namespace internal
{
    template <size_t BITS_PER_ELEMENT> struct minrvsize
    {
        enum
        {
            ELSIZE = BITS_PER_ELEMENT,
            ELBYTES = ((BITS_PER_ELEMENT + 7) & (~7)) / 8,
        };
        static const bool MULTIBYTE = (BITS_PER_ELEMENT == 3 || (BITS_PER_ELEMENT >= 5 && BITS_PER_ELEMENT < 8) || BITS_PER_ELEMENT > 8);
        static const size_t MINIMUMRVSIZE = MULTIBYTE ? (BITS_PER_ELEMENT - 8 * size_t(BITS_PER_ELEMENT / 8) == 1 ? ELBYTES : (ELBYTES + 1)) : 1;
    };
};

template< size_t BITS_PER_ELEMENT, size_t NUM_OF_ELEMENTS, typename TYPEOFRV = sztype< internal::minrvsize<BITS_PER_ELEMENT>::MINIMUMRVSIZE >::type > class packed_buf_c
{

    enum
    {
        ELSIZE = BITS_PER_ELEMENT,
        NUMELEMENTS = NUM_OF_ELEMENTS,
        RVSIZE = sizeof(TYPEOFRV),
    };

    //размер массива - максимальный индекс элемента => смещение в байтах + размер возвращаемого значения, чтобы не случилось выхода за границы массива
    uint8 data[ (((NUM_OF_ELEMENTS-1) * BITS_PER_ELEMENT) >> 3) + sizeof(TYPEOFRV) ];
public:
    packed_buf_c( bool _clear = true )
    {
        if (_clear) clear();
    }

    void clear()
    {
        memset( data, 0, sizeof(data) );
    }

    /*
    при вычислении минимального необходимого размера RV, нужно учитывать следующее:
    1. одним вычитом RV из памяти мы должны гарантированно захватывать все биты элемента
    2. битовое смещение элемента в общем случае любое
    3. элементы с размерами 1,2,4 и 8 бит всегда можно вычитать одним байтом, т.к. они никогда не попадают на границы двух байтов в памяти

    например для 9-битного элемента требуется 2-байтовая вычитка, а для 10-битного - минимум 3-байтная
    */

    TS_STATIC_CHECK( RVSIZE >= internal::minrvsize<BITS_PER_ELEMENT>::MINIMUMRVSIZE, "element too large" );

    TYPEOFRV get(aint index) const
    {
#ifdef __BIG_ENDIAN__
#error "sorry, big endian not yet supported"
#endif

        if (!ASSERT(index >= 0 && index < NUM_OF_ELEMENTS)) return TYPEOFRV(0);
        aint disp = (index * BITS_PER_ELEMENT) >> 3;
        int bitsshift = (index * BITS_PER_ELEMENT) - (disp * 8);
        return (MAKEMASK(BITS_PER_ELEMENT, bitsshift) & (*(TYPEOFRV *)(data+disp))) >> bitsshift;
    }

    void set(aint index, TYPEOFRV v) // single thread only!
    {
        if (!ASSERT(index >= 0 && index < NUM_OF_ELEMENTS)) return;
        aint disp = (index * BITS_PER_ELEMENT) >> 3;
        int bitsshift = (index * BITS_PER_ELEMENT) - (disp * 8);
        TYPEOFRV mask = MAKEMASK(BITS_PER_ELEMENT, bitsshift);
        TYPEOFRV vv = *(TYPEOFRV *)(data + disp);
        TYPEOFRV newvv = (vv & (~mask)) | (v << bitsshift);
        *(TYPEOFRV *)(data + disp) = newvv;
    }

};

class fifo_buf_c
{
    buf0_c buf[2];
    aint readpos = 0;

    unsigned readbuf : 1;
    unsigned newdata : 1;

public:

    fifo_buf_c()
    {
        readbuf = 0;
        newdata = 0;
    }

    void add_data(const void *d, aint s)
    {
        buf0_c &b = buf[newdata];
        b.append_buf(d,s);
    }

    aint read_data(void *dest, aint size);
    void clear()
    {
        buf[0].clear();
        buf[1].clear();
        readbuf = 0;
        newdata = 0;
        readpos = 0;
    }
    int available()  const { return buf[readbuf].size() - readpos + buf[readbuf ^ 1].size(); }
};


} // namespace ts
