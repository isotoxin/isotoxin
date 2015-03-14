#pragma once

#pragma warning(push)
#pragma warning(disable:4127)

namespace ts
{

template <typename UNIT_PTR, typename ALLOCATOR> class DEFAULT_BEHAVIOUR : public ALLOCATOR
{
public:
    typedef typename clean_type<UNIT_PTR>::type CLEAN_OBJTYPE;

    static void destructor(UNIT_PTR) {}
    static void init(UNIT_PTR *u, uint count) {}
    template<typename KEY> static  int  compare( UNIT_PTR u1, KEY key ) { auto &unit = *u1; return unit(key); }

    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(UNIT_PTR &to, TFROM from) { to = from; } };

};

template <typename T, typename ALLOCATOR> class DEFAULT_BEHAVIOUR_CLEAR_ONLY : public ALLOCATOR
{
public:

    typedef decltype(clean_type<UNIT_PTR>::type) CLEAN_OBJTYPE;

    TS_STATIC_CHECK( std::is_pod<T>::value, "T must be pod" );
    static void destructor(T u) {}
    static void init(T *u, auint count) { blk_fill<T>(u, count); }
	template<typename KEY> static  int  compare( const T u1, const KEY key ) {	auto &unit = *u1; return unit(key);	}
    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(T &to, TFROM from) { to = from; } };
};

template <typename UNIT_PTR, typename ALLOCATOR> class DEFAULT_BEHAVIOUR_DEL : public DEFAULT_BEHAVIOUR<UNIT_PTR, ALLOCATOR>
{
public:

    typedef typename clean_type<UNIT_PTR>::type CLEAN_OBJTYPE;

    TS_STATIC_CHECK( std::is_pointer<UNIT_PTR>::value, "UNIT_PTR must be pointer" );
    static void destructor(UNIT_PTR u) { if (u) TSDEL(u); }
    static void init(UNIT_PTR *u, uint count) { blk_fill<UNIT_PTR>(u, count); }
    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(UNIT_PTR &to, TFROM from) { to = from; } };
    /*
    template<typename TFROM> struct copy<TFROM, false, false>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            ASSERT(false, "bad use of array of pointers");
            to = from;
        }
    }
    template<typename TFROM> struct copy<TFROM, true, false>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            ASSERT(false, "bad use of array of pointers");
            to = from;
        }
    }
    */
    template<typename TFROM> struct copy<TFROM, true, true>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            destructor(to);
            to = from;
        }
    };
};

template <typename UNIT_PTR, typename ALLOCATOR> class DEFAULT_BEHAVIOUR_REL : public DEFAULT_BEHAVIOUR<UNIT_PTR, ALLOCATOR>
{
public:
    typedef decltype(clean_type<UNIT_PTR>::type) CLEAN_OBJTYPE;

    TS_STATIC_CHECK( std::is_pointer<UNIT_PTR>::value, "UNIT_PTR must be pointer" );
    static void destructor(UNIT_PTR u) { if (u) u->release(); }
    static void init(UNIT_PTR *u, uint count) { blk_fill<UNIT_PTR>(u, count); }

    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(UNIT_PTR &to, TFROM from) { to = from; } };

    /*
    template<typename TFROM> struct copy<TFROM, false, false>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            ASSERT(false, "bad use of array of pointers");
            to = from;
        }
    }
    template<typename TFROM> struct copy<TFROM, true, false>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            ASSERT(false, "bad use of array of pointers");
            to = from;
        }
    }
    */
    template<typename TFROM> struct copy<TFROM, true, true>
    {
        copy(UNIT_PTR &to, TFROM from)
        {
            destructor(to);
            to = from;
        }
    };
};

template <typename inarraytype, typename OBJ, typename ALLOCATOR> class DEFAULT_BEHAVIOUR_SMARTPTR : public ALLOCATOR
{
public:
    typedef typename clean_type<OBJ>::type CLEAN_OBJTYPE;

	static void destructor(inarraytype &s) {s.~inarraytype();}
	static void init(inarraytype * p, uint n)
	{
		for(uint i=0;i<n;++i)
			TSPLACENEW(p+i);
	}
	template<typename KEY> static int  compare( const inarraytype &u1, const KEY &key ) { return (*u1.get())(key); }

    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(inarraytype &to, TFROM from); };

    template<typename TFROM> struct copy<TFROM, false, false>
    {
        copy(inarraytype &to, TFROM from)
        {
            TSPLACENEW(&to, from);
        }
    };
    template<typename TFROM> struct copy<TFROM, false, true>
    {
        copy(inarraytype &to, TFROM from)
        {
            TS_STATIC_CHECK( is_movable<inarraytype>::value, "safe_ptr is not movable :(" );
            memcpy(&to, &from, sizeof(inarraytype));
        }
    };
    template<typename TFROM> struct copy<TFROM, true, false>
    {
        copy(inarraytype &to, TFROM from)
        {
            to = from;
        }
    };
    template<typename TFROM> struct copy<TFROM, true, true>
    {
        copy(inarraytype &to, TFROM from)
        {
            destructor(to);
            memcpy(&to, &from, sizeof(inarraytype));
        }
    };

};


template<typename T, typename ALLOCATOR> class DEFAULT_BEHAVIOUR_INPLACE : public ALLOCATOR
{
public:
    typedef typename clean_type<T>::type CLEAN_OBJTYPE;

    static void destructor(T &s) { s.~T(); }
    static void init(T * p, uint n)
    {
        for (uint i = 0; i < n; ++i)
            TSPLACENEW(p + i);
    }
    template<typename KEY> static int  compare(const T &u1, const KEY &key) { return u1(key); }
    
    template<typename TFROM, bool target_initialized, bool moveit> struct copy { copy(T &to, TFROM from); };

    template<typename TFROM> struct copy<TFROM, false, false>
    {
        copy(T &to, TFROM from)
        {
            TSPLACENEW(&to, from);
        }
    };
    template<typename TFROM> struct copy<TFROM, false, true>
    {
        template<bool same, bool movable> static void _copy( T &to, TFROM from )
        {
            TSPLACENEW(&to, from);
            destructor(from);
        }
        template<> static void _copy<true, false>( T &to, TFROM from )
        {
            TSPLACENEW(&to, std::move(from));
            destructor(from);
        }
        template<> static void _copy<true, true>( T &to, TFROM from )
        {
            memcpy(&to, &from, sizeof(T));
        }

        copy(T &to, TFROM from)
        {
            _copy<std::is_same<T, std::remove_reference<TFROM>::type>::value, is_movable<T>::value>(to,from);
        }
    };
    template<typename TFROM> struct copy<TFROM, true, false>
    {
        copy(T &to, TFROM from)
        {
            to = from;
        }
    };
    template<typename TFROM> struct copy<TFROM, true, true>
    {
        template<bool same, bool movable> static void _copy(T &to, TFROM from)
        {
            to = std::move(from);
            destructor(from);
        }
        template<> static void _copy<true, true>(T &to, TFROM from)
        {
            destructor(to);
            memcpy(&to, &from, sizeof(T));
        }

        copy(T &to, TFROM from)
        {
            _copy<std::is_same<T, std::remove_reference<TFROM>::type>::value, is_movable<T>::value>(to, from);
        }
    };

};

/**
*  universal array
*/
template <typename T, int SMALLGRANULA = 0, typename BEHAVIOUR = DEFAULT_BEHAVIOUR<T, TS_DEFAULT_ALLOCATOR> > class array_c 
{
#ifdef _FINAL
    void fill_dbg(auint, auint) {}
#else
    void fill_dbg(auint i, auint n) { blk_fill<uint8>(m_list + i, n * sizeof(T), 0xcd); }
#endif

    BEHAVIOUR beh;
public:
    typedef typename BEHAVIOUR::CLEAN_OBJTYPE CLEAN_OBJTYPE;
    typedef T OBJTYPE;

private:

    T *m_list;
    int m_count;     //items in list
    int m_capacity;     //allocated memory in list

    void    _destroy(aint idx)
    {
        BEHAVIOUR::destructor( m_list[idx] );
    }
    void _upsize(aint new_count, aint index, aint count)
    {
        if (new_count > m_capacity)
        {
            aint oalocated = m_capacity;
            if (SMALLGRANULA > 0)
            {
                m_capacity = new_count + SMALLGRANULA;
            } else
            {
                auint allocsize = DetermineGreaterPowerOfTwo( new_count * sizeof(T) );
                m_capacity = (decltype(m_capacity))(allocsize / sizeof(T));
            }

            ASSERT((m_count + count) <= m_capacity);

            if (is_movable<T>::value)
            {
                m_list = (T *)beh.mra(m_list, sizeof(T)*(m_capacity));
                if (index < m_count)
                    blk_copy_back( m_list + index + count, m_list + index, (m_count-index) * sizeof(T) );

            } else {
                T *new_list = (T *)beh.ma(sizeof(T)*(m_capacity));
                for (int i = 0; i < index; ++i)
                    BEHAVIOUR::copy<T&, false, true>( new_list[i], m_list[i] );

                for (int i = index; i < m_count; ++i)
                    BEHAVIOUR::copy<T&, false, true>(new_list[i+count], m_list[i]);

                beh.mf(m_list);
                m_list = new_list;
                oalocated = m_count;
            }

            // все равно дыра в массиве будет заполнена. незачем ее заполнять дебаговыми цифирками
            //if (index < m_count)
            //    fill_dbg(index, count);

            m_count += count;

            aint dbgfill = m_capacity - oalocated - count;
            if (dbgfill > 0)
                fill_dbg(m_capacity-dbgfill, dbgfill);

        } else if (index <= m_count)
        {
            ASSERT((m_count + count) <= m_capacity);
            if (is_movable<T>::value)
            {
                if (aint cntmove = (m_count - index))
                    blk_copy_back(m_list + index + count, m_list + index, cntmove * sizeof(T));
            } else
            {
                for (int i = m_count-1; i >= index; --i)
                    BEHAVIOUR::copy<T&, false, true>(m_list[i + count], m_list[i]);
            }
            
            // все равно дыра в массиве будет заполнена. незачем ее заполнять дебаговыми цифирками
            //fill_dbg(index, count);
            m_count += count;
        }
    }

    void    _remove_slow(aint index, aint count) // make sure removed elements already destroyed
    {
        if (is_movable<T>::value)
        {
            blk_copy_fwd(m_list + index, m_list + index + count, (m_count - index - count) * sizeof(T));
        }
        else
        {
            for (int i = index + count; i < m_count; ++i)
                BEHAVIOUR::copy<T&, false, true>(m_list[i - count], m_list[i]);
        }
        fill_dbg( m_count - count, count );
        m_count -= count;
    }

    T &__addraw()
    {
        aint newcount = m_count + 1;
        _upsize(newcount, m_count, 0);
        m_count = newcount;
        return m_list[newcount-1];
    }

public:

    template<class... _Valty> T &addnew(_Valty&&... _Val)
    {
        T &t = __addraw();
        TSPLACENEW(&t, std::forward<_Valty>(_Val)...);
        return t;
    }

	T &add()
	{
		int newcount = m_count+1;
		_upsize(newcount, m_count, 0);
		BEHAVIOUR::init(m_list + m_count, 1);
		m_count = newcount;
		return m_list[newcount-1];
	}

    template<typename TT> aint add(const TT &d)
    {
        int ret = size();
        set(ret,d);
        return ret;
    }

    T& duplast() // if array is empty, default item will be created
    {
        if (size() == 0) return add();
        int newcount = size() + 1;
        _upsize(newcount, m_count, 0);
        BEHAVIOUR::copy<const T&, false,false>( m_list[newcount - 1], m_list[newcount - 2] );
        m_count = newcount;
        return m_list[newcount - 1];
    }

    void    swap_unsafe( aint i0, aint i1 )
    {
        make_pod<T> tmp;
        BEHAVIOUR::copy<T&,false,true>( tmp, m_list[i0] );
        BEHAVIOUR::copy<T&,false,true>( m_list[i0], m_list[i1] );
        BEHAVIOUR::copy<T&,false,true>( m_list[i1], tmp );
    }

    void    move_up_unsafe(aint idx)
    {
        swap_unsafe(idx,idx-1);
    }
    template<typename TT> void    move_up(const TT &p)
    {
        int idx = find(p);
        if (idx > 0)
            move_up_unsafe(idx);
    }

    void    move_down_unsafe(aint idx)
    {
        swap_unsafe(idx,idx+1);
    }
    template<typename TT> void    move_down(const TT &p)
    {
        if (m_count > 1)
        {
            int idx = find(p);
            if ((idx >= 0) && (idx < int(m_count - 1)))
            {
                move_down_unsafe( idx );
            }
        }
    }

    /**
    * Expands buffer, if index is out of current size of buffer.
    */
    void    set(aint idx, const T &d)
    {
        if (idx<m_count)
        {
            BEHAVIOUR::copy<const T&, true,false>( m_list[idx], d );
            return;
        }

        ASSERT( !is_movable<T>::value || &d < m_list || &d > (m_list + m_capacity), "Can't set object from this array due buffer resize" );

        int newcount = idx+1;
        _upsize(idx+1, m_count, 0);

        ASSERT(newcount > m_count);
        
        BEHAVIOUR::copy<const T&, false,false>( m_list[idx], const_cast<T &>(d) );
        BEHAVIOUR::init(m_list + m_count, idx-m_count); // инициализация элементов между вставляемым и последним концом массива, если вставляемы оказался далеко от конца

        m_count = newcount;
    }

    template<typename TT> void set(aint idx, const TT &d)
    {
        T t = d;
        set(idx,t);
    }

    bool    set(const T & d)
    {
        int idx = find(d);
        if (idx >=0)
        {
            return false;
        } else
        {
            set(size(),d);
            return true;
        }
    }

    template<typename TT>void replace(aint pos, aint num, const TT *arr, aint len)
    {
        if (!ASSERT(pos >= 0 && pos + num <= m_count && len >= 0 && (const void *)arr != (const void *)m_list)) return;
        if (len > num)
        {
            _upsize(m_count - num + len, pos + num, len - num);

            for (aint i = 0; i < num; ++i)
                BEHAVIOUR::copy<const TT&, true, false>(m_list[pos + i], arr[i]);

            for (aint i = num; i < len; ++i)
                BEHAVIOUR::copy<const TT&, false, false>(m_list[pos + i], arr[i]);


        } else
        {
            for (aint i = 0; i < len; ++i)
                BEHAVIOUR::copy<const TT&, true, false>(m_list[pos + i], arr[i]);

            if (len < num)
                remove_some(pos+len, num-len);
        }
    }

    void replace(aint pos, aint num, const array_c &a) { replace(pos, num, &a.get(0), a.size()); }

    void insert(aint pos, const T *arr, aint len) { replace(pos, 0, arr, len); }
    void insert(aint pos, const array_c &a) { replace(pos, 0, &a.get(0), a.size()); }

    aint    insert(aint idx, const T & d)
    {
        if (idx >= m_count)
        {
            set(idx,d);
            return idx;
        }

        ASSERT( !is_movable<T>::value || &d < m_list || &d > (m_list + m_capacity), "Can't insert object from this array due buffer resize" );

        _upsize(m_count+1, idx, 1);

        BEHAVIOUR::copy<const T&, false,false>( m_list[idx], const_cast<T &>(d) );

        return idx;
    }

    T  &insert(aint idx)
    {
        if (idx >= m_count)
            return m_list[idx];

        _upsize(m_count+1, idx, 1);
        BEHAVIOUR::init(m_list + idx, 1);
        return m_list[idx];
    }

    template <typename KEY> aint  insert_sorted_next( const T & d, const KEY &key )
    {
        aint index;
        if ( find_sorted( index, key ) )
            return insert( index + 1, d );
        return insert( index, d );
    }
    template <typename KEY> aint  insert_sorted_uniq( const T & d, const KEY &key )
    {
        aint index;
        if ( find_sorted( index, key ) )
            return -1;
        return insert( index, d );
    }

    template <typename KEY> aint  insert_sorted_overwrite( const T & d, const KEY &key )
    {
        aint index;
        if ( find_sorted( index, key ) )
        {
            set( index, d );
        } else
        {
            insert( index, d );
        }
        return index;
    }

    template <typename KEY, typename INHERITOR> int  insert_sorted_inherit_old( INHERITOR &inh, const T & d, const KEY &key )
    {
        int index;
        if ( find_sorted( index, key ) )
        {
            inh( d, get(index) );
            set( index, d );
        } else
        {
            insert( index, d );
        }
        return index;
    }

    void    reserve( aint count )
    {
        _upsize(count, m_count, 0);
    }

    void    expand(aint idx, aint count)
    {
        if (idx >= m_count)
        {
            int new_count = idx + count;

            _upsize(new_count, m_count, 0); // т.к. idx за пределами массива, то никакие переносы внутри _upsize не требуются

            BEHAVIOUR::init(m_list + m_count, new_count - m_count);
            m_count = new_count;

            return;
        }

        _upsize(m_count + count, idx, count);
        BEHAVIOUR::init(m_list + idx, count);
    }

    template<typename TT> aint     find(const TT & p) const
    {
        for (const T& t : *this)
            if (t == p) return &t - m_list;
        return -1;
    }

    template<typename KEY> bool find_sorted(aint &index, const KEY &key, int maxcount = -1) const
    {
        if (maxcount < 0) maxcount = m_count;

        if ( maxcount == 0 )
        {
            index = 0;
            return false;
        }
        if ( maxcount == 1 )
        {
            int cmp = BEHAVIOUR::compare(m_list[0], key);
            index = cmp <= 0 ? 0 : 1;
            return cmp == 0;
        }


        int left = 0;
        int rite = maxcount;

        int test;
        do
        {
            test = (rite + left) >> 1;

            int cmp = BEHAVIOUR::compare(m_list[test], key);
            if ( cmp == 0 )
            {
                index = test;
                return true;
            }
            if (cmp < 0)
            {
                // do left
                rite = test;
            } else
            {
                // do rite
                left = test + 1;
            }
        } while (left < (rite-1));

        if ( left >= maxcount )
        {
            index = left;
            return false;
        }

        int cmp = BEHAVIOUR::compare(m_list[left], key);
        index = cmp <= 0 ? left : left + 1;
        return cmp == 0;
    }

    void    reinit(int idx)
    {
        BEHAVIOUR::destructor(m_list[idx]);
        BEHAVIOUR::init(m_list + idx, 1);
    }

    void    remove_fast(aint idx) // moves last element to removed one
    {
        ASSERT( idx < size(), "remove_fast: out of range of array" );
        BEHAVIOUR::copy<T&, true,true>( m_list[idx], m_list[--m_count] );
        fill_dbg(m_count,1);
    }
    template<typename TT> void    find_remove_fast(const TT &t)
    {
        aint idx = find(t);
        if (idx >= 0)
            remove_fast(idx);
    }

    T get_remove_slow(int idx = 0)
    {
        ASSERT( idx < size(), "get_remove_slow: out of range of array" );

        if (size() == 0) return make_dummy<T>();
        
        make_pod<T> r;
        BEHAVIOUR::copy<T&, false,true>( r, m_list[idx] );

        _remove_slow(idx, 1);
        return r;
    }

    T get_remove_fast(int idx = 0)
    {
        if (size() == 0) return make_dummy<T>();

        make_pod<T> r;
        BEHAVIOUR::copy<T&, false, true>(r, m_list[idx]);
        BEHAVIOUR::copy<T&, false, true>(m_list[idx], m_list[--m_count]);
        fill_dbg(m_count, 1);
        return r;
    }

    T get_last_remove(void)
    {
        if (size() == 0) return make_dummy<T>();

        make_pod<T> r;
        BEHAVIOUR::copy<T&, false, true>(r, m_list[m_count-1]);

        --m_count;
        fill_dbg(m_count, 1);
        return r;
    }

    void    remove_slow(aint idx)
    {
        ASSERT( idx < size(), "remove_slow: out of range of array" );
        _destroy(idx);
        _remove_slow(idx,1);
    }
    aint    find_remove_slow(const T &t)
    {
        aint idx = find(t);
        if (idx >=0)
            remove_slow(idx);
        return idx;
    }

    void    remove_some( aint start, aint count = -1 )
    {
        if (count < 0) count = m_count - start;
        ASSERT( (start+count) <= size() && start >= 0 && count >= 0, "remove_slow: out of range of array" );
        for (int i=0;i<count;++i)
            _destroy(i + start);
        _remove_slow(start,count);
    }

    void    truncate( aint idx )
    {
        if (m_count <= idx) return;
        for (int i = idx;i < m_count;++i)
            _destroy(i);
        fill_dbg(idx, m_count - idx);
        m_count = idx;
    }
    void    clear(bool free_memory=false)
    {
        T *ptr = m_list + m_count - 1;
        while (ptr>=m_list) { BEHAVIOUR::destructor( *ptr ); ptr--; }
        aint fillcount = m_count;
        if (free_memory)
        {
            if (SMALLGRANULA > 0)
            {
                if ( m_capacity != SMALLGRANULA )
                {
                    beh.mf(m_list);
                    m_list = (T *)beh.ma(sizeof(T)*SMALLGRANULA);
                    m_capacity = SMALLGRANULA;
                    fillcount = m_capacity;
                }
            } else
            {
                if ( m_capacity != 2 )
                {
                    beh.mf(m_list);
                    m_list = (T *)beh.ma(sizeof(T)*2);
                    m_capacity = 2;
                    fillcount = m_capacity;
                }
            }
        }
        fill_dbg(0, fillcount);

        m_count = 0;
    }

    aint size(void) const {return m_count;};

	const T &operator[](int index) const { return get(index); }
	T& operator[](int index) { return get(index); }


    const T &get(int idx) const
    {
        ASSERT( idx < size(), "get: out of range of array" );
        return m_list[idx];
    };
    T &get(int idx)
    {
        ASSERT(idx < size(), "get: out of range of array");
        return m_list[idx];
    };
    const T &last(void) const { return (m_count == 0) ? make_dummy<T>() : m_list[m_count - 1]; };
    T &last(void) {return (m_count==0)?make_dummy<T>():m_list[m_count-1];};

    void    absorb_add( array_c &from )
    {
        uint ns = m_count + from.m_count;

        _upsize(ns, m_count, 0);

        if (is_movable<T>::value)
        {
            blk_copy_fwd(m_list + count, from.m_list, from.m_count * sizeof(T));
        }
        else
        {
            for (int i = 0; i < from.m_count; ++i)
                BEHAVIOUR::copy<T&, false, true>(m_list[i + m_count], from.m_list[i]);
        }

        m_count = ns;
        from.fill_dbg(0, from.m_count);
        from.m_count = 0;
    }

    void        absorb( array_c &from )
    {
        SWAP( m_capacity, from.m_capacity );
        SWAP( m_count, from.m_count );
        SWAP( m_list, from.m_list );
        from.clear(false);
    }

    template<typename ARR> bool sortcopy(ARR &arr)
    {
        bool other_order = false;
        for(const T &t : *this)
            if ( arr.insert_sorted_next( t, t ) < (arr.size()-1) ) other_order = true;
        return other_order;
    }

    /// Quick-sort the array.
    /// Parameter is functional object
    template < typename F > bool sort ( const F &comp, aint ileft=0, aint irite=-1 )
    {
        if (size() <= 1) return false;

        if ( ileft<0 ) ileft = size()+ileft;
        if ( irite<0 ) irite = size()+irite;
        ASSERT( ileft<=irite, "incorrect range for servarray_c::sort" );

        aint st0[32], st1[32];
        aint a, b, k, i, j;

        bool sorted = false;

        k = 1;
        st0[0] = ileft;
        st1[0] = irite;
        while (k != 0)
        {
            k--;
            i = a = st0[k];
            j = b = st1[k];
            int centeri = ((a + b) / 2);
            T *x = m_list + centeri;
            while (a < b)
            {
                while (i <= j)
                {
                    while ( comp ( m_list[i], *x ) ) i++;
                    while ( comp ( *x, m_list[j] ) ) j--;
                    if (i <= j)
                    {
                        swap_unsafe( i, j );

                        if (centeri == i) { x = m_list + j; centeri = j; }
                        else if (centeri == j) { x = m_list + i; centeri = i; }

                        i++;
                        j--;
                        sorted = true;
                    }
                }
                if (j-a >= b-i)
                {
                    if (a < j)
                    {
                        st0[k] = a;
                        st1[k] = j;
                        k++;
                    }
                    a = i;
                } else
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

    bool sort()
    {
        return sort([](const T &o1, const T &o2)->bool
        {
            return BEHAVIOUR::compare(o1, o2) < 0;
        });
    }

    array_c& operator=(const array_c&op)
    {
        replace( 0, size(), op );
        return *this;
    }
    array_c& operator=(array_c&&op)
    {
        clear();
        SWAP(m_count, op.m_count);
        SWAP(m_capacity, op.m_capacity);
        SWAP(m_list, op.m_list);
        return *this;
    }

    array_c(const array_c&a)
    {
        m_count = 0;
        m_capacity = a.size();
        m_list = (T *)beh.ma(sizeof(T)*m_capacity);
        *this = a;
    }
    array_c(array_c &&a)
    {
        m_count = a.m_count;
        m_capacity = a.m_capacity;
        m_list = a.m_list;
        a.m_count = 0;
        a.m_capacity = 0;
        a.m_list = nullptr;
    }

    array_c(std::initializer_list<T> list)
    {
        m_count = list.size();
        m_capacity = m_count;
        m_list = (T *)beh.ma(sizeof(T)*m_capacity);

        aint i = 0;
        for(const T &el : list) BEHAVIOUR::copy<const T&, false,false>( m_list[i++], const_cast<T &>(el) );
    }


    array_c( aint capacity = 0 )
    {
        if (capacity == 0)
        {
            if (SMALLGRANULA > 0)
            {
                m_list = (T *)beh.ma(sizeof(T)*SMALLGRANULA);
                m_capacity = SMALLGRANULA;
            }
            else
            {
                m_list = (T *)beh.ma(sizeof(T) * 2);
                m_capacity = 2;
            }
        } else
        {
            m_list = (T *)beh.ma(sizeof(T) * capacity);
            m_capacity = capacity;
        }
        m_count = 0;
        fill_dbg(0, m_capacity);
    }
    ~array_c()
    {
        T *ptr = m_list + m_count - 1;
        while (ptr>=m_list) { beh.destructor( *ptr ); ptr--; }
        beh.mf(m_list);
    }

    array_wrapper_c<const T> array() const { return array_wrapper_c<const T>( m_list, m_count ); }

    // begin() end() semantics

    T * begin() { return m_list + 0; }
    const T *begin() const { return m_list + 0; }

    T *end() { return m_list + m_count; }
    const T *end() const { return m_list + m_count; }

};

template<typename obj, int granula> using pointers_t = array_c < obj *, granula, DEFAULT_BEHAVIOUR<obj *, TS_DEFAULT_ALLOCATOR> >;
template<typename obj, int granula> using array_del_t = array_c < obj *, granula, DEFAULT_BEHAVIOUR_DEL<obj *, TS_DEFAULT_ALLOCATOR> >;
template<typename obj, int granula> using array_release_t = array_c < obj *, granula, DEFAULT_BEHAVIOUR_REL<obj *, TS_DEFAULT_ALLOCATOR> >;
template<typename obj, int granula> using array_safe_t = array_c<safe_ptr<obj>, granula, DEFAULT_BEHAVIOUR_SMARTPTR<safe_ptr<obj>, obj, TS_DEFAULT_ALLOCATOR> >;
template<typename obj, int granula> using array_shared_t = array_c<shared_ptr<obj>, granula, DEFAULT_BEHAVIOUR_SMARTPTR<shared_ptr<obj>, obj, TS_DEFAULT_ALLOCATOR> >;
template<typename obj, int granula> using array_inplace_t = array_c<obj, granula, DEFAULT_BEHAVIOUR_INPLACE<obj, TS_DEFAULT_ALLOCATOR> >;

template<typename obj, int granula> using tmp_pointers_t = ts::array_c<obj*,0,DEFAULT_BEHAVIOUR<obj*, TMP_ALLOCATOR> >;
template<typename obj, int granula> using tmp_array_del_t = array_c < obj *, granula, DEFAULT_BEHAVIOUR_DEL<obj *, TMP_ALLOCATOR> > ;
template<typename obj, int granula> using tmp_array_release_t = array_c < obj *, granula, DEFAULT_BEHAVIOUR_REL<obj *, TMP_ALLOCATOR> > ;
template<typename obj, int granula> using tmp_array_safe_t = array_c < safe_ptr<obj>, granula, DEFAULT_BEHAVIOUR_SMARTPTR<safe_ptr<obj>, obj, TMP_ALLOCATOR> > ;
template<typename obj, int granula> using tmp_array_shared_t = array_c < shared_ptr<obj>, granula, DEFAULT_BEHAVIOUR_SMARTPTR<shared_ptr<obj>, obj, TMP_ALLOCATOR> > ;
template<typename obj, int granula> using tmp_array_inplace_t = array_c < obj, granula, DEFAULT_BEHAVIOUR_INPLACE<obj, TMP_ALLOCATOR> > ;


template <typename UNIT, int MAX_SIZE = 2048, typename BEHAVIOUR = DEFAULT_BEHAVIOUR<UNIT> > class insert_sorter_c
{
    UNIT    m_array[MAX_SIZE];
    int     m_left;
    int     m_rite;

public:

    insert_sorter_c(void):m_left(MAX_SIZE >> 1), m_rite(MAX_SIZE >> 1)
    {
    }

    uint    size(void)
    {
        return m_rite - m_left;
    }

    UNIT    get(int index)
    {
        return m_array[m_left + index];
    }

    void    clear(void)
    {
        for (;m_left < m_rite; ++m_left)
        {
            BEHAVIOUR::destructor( m_array[m_left] );
        }

        m_left = MAX_SIZE >> 1;
        m_rite = MAX_SIZE >> 1;
    }

    void destroy( UNIT u )
    {
        int index = get_index_by<UNIT>( u );
        if (index >= 0)
        {
            int idx = m_left + index;
            BEHAVIOUR::destructor( m_array[idx] );
            blk_UNIT_copy_fwd( m_array + idx, m_array + idx + 1, m_rite-idx-1 );
            --m_rite;
        }
    }

    template <typename ZZZ> int get_index_by( ZZZ zzz )
    {
        int idx;
        int idx0 = m_left;
        int idx1 = m_rite;
        if (m_left == m_rite) return -1;
        for(;;)
        {
            idx = ((idx1-idx0) >> 1) + idx0;

            int cmpr = BEHAVIOUR()(m_array[idx], zzz);
            if (cmpr == 0)
            {
                return idx - m_left;
            }
            if (cmpr > 0)
            {
                //left
                if (idx == idx0) break;
                idx1 = idx;
            } else
            {
                //rite
                ++idx;
                if (idx == idx1) break;
                idx0 = idx;
            }
        }
        return -1;
    }

    template <typename ZZZ> bool find_by( UNIT & out, ZZZ zzz )
    {
        int index = get_index_by<ZZZ>( zzz );
        if (index >= 0)
        {
            out = m_array[index + m_left];
            return true;
        }
        return false;
    }

    bool    is_valid(void)
    {
        for (int i=m_left;i<m_rite-2;++i)
        {
            if (BEHAVIOUR()(m_array[i], m_array[i+1]) > 0) return false;
        }
        return true;
    }

    bool    add(UNIT u)
    {
        bool noleft = (m_left <= 0);
        bool norite = (m_rite >= MAX_SIZE);
        if (noleft && norite) return false;

        if (m_left == m_rite)
        {
            m_array[m_left] = u;
            ++m_rite;
            return true;
        }
        //seek index
        int idx;
        int idx0 = m_left;
        int idx1 = m_rite;
        for(;;)
        {
            idx = ((idx1-idx0) >> 1) + idx0;

            if (BEHAVIOUR()(m_array[idx], u) > 0)
            {
                //left
                if (idx == idx0) break;
                idx1 = idx;
            } else
            {
                //rite
                ++idx;
                if (idx == idx1) break;
                idx0 = idx;
            }
        }
        if (!norite && (idx == m_rite))
        {
            ++m_rite;
            m_array[idx] = u;
        } else  if (!noleft && (idx == m_left))
        {
            --m_left;
            m_array[idx-1] = u;
        } else
        {
            int lc = (idx-m_left);
            int rc = (m_rite-idx);
            bool expand_left = norite || ((lc <= rc) && !noleft);

            if (expand_left)
            {
                memcpy(&m_array[m_left-1], &m_array[m_left], sizeof(UNIT) * lc);
                --m_left;
                m_array[idx-1] = u;
            } else
            {
                blk_UNIT_copy_back(&m_array[idx+1], &m_array[idx], rc);
                ++m_rite;
                m_array[idx] = u;
            }
        }

        return true;

    }

    template <typename INITER> void allocate_and_init(int count, INITER &initer)
    {
        ASSERT( count <= MAX_SIZE );

        clear();
        m_left = ( MAX_SIZE >> 1 ) - ( count >> 1 );
        m_rite = m_left + count;
        for (int i=0;i<count;++i)
        {
            m_array[i + m_left] = initer( i );
        }
    }

};

} // namespace ts

#pragma warning(pop)
