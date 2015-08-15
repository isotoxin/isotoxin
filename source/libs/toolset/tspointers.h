/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

namespace ts
{

class safe_object
{
	template <class T> friend class safe_ptr;

	struct pointer_container_s
	{
		safe_object *pointer;
		int ref;
        static pointer_container_s *create(safe_object *p);
        void die();
	} *__pc = nullptr;

	safe_object(const safe_object &) UNUSED;
	void operator=(const safe_object &) UNUSED;

public:

    static const aint POINTER_CONTAINER_SIZE = sizeof(pointer_container_s);

	safe_object() {}

    void make_all_ponters_expired() // use carefully! you can get memory leak if there are no regular pointers to this object
    {
        if (__pc)
        {
            __pc->pointer = nullptr;
            __pc = nullptr;
        }
    }

	~safe_object()
	{
		if (__pc) __pc->pointer = nullptr;
	}
};


/*
	movable intrusive weak pointer
*/

template <typename T> class safe_ptr // T must be public child of safe_object
{
    MOVABLE(true);
	friend class safe_object;
	template <class T> friend class safe_ptr;

	safe_object::pointer_container_s *pc = nullptr;

	void unconnect() //for internal use only
	{
		if (pc)
		{
			if (--pc->ref == 0)
			{
				if (pc->pointer) pc->pointer->__pc = nullptr;
				pc->die();
			}
		}
	}

	void connect(T *p) //for internal use only
	{
		if (p)
		{
			if (p->__pc)
			{
				pc = p->__pc;
				pc->ref++;
			}
			else
			{
				pc = p->__pc = safe_object::pointer_container_s::create(p);
			}
		}
		else pc = nullptr;
	}

	T *pointer() const {return pc ? static_cast<T*>(pc->pointer) : nullptr;}

public:

	safe_ptr() {}
	safe_ptr(T *p) {connect(p);}
	safe_ptr(const safe_ptr &p)
	{
		if (p.pc) p.pc->ref++;
		pc = p.pc;
	}
	~safe_ptr() {unconnect();}

	operator  T *() const {return pointer();}
	T *operator->() const {return pointer();}
    T *get() const {return pointer();}
    bool expired() const {return get() == nullptr;}

	safe_ptr &operator=(T *p)
	{
		if (!p || pointer() != p)
		{
			unconnect();
			connect(p);
		}
		return *this;
	}
	safe_ptr &operator=(const safe_ptr &p)
	{
		unconnect();
		if (p.pc) p.pc->ref++;
		pc = p.pc;
		return *this;
	}
};

/*
	intrusive shared pointer
	
    example:
    shared_ptr<MyClass> p(new MyClass(...)), p2(p), p3=p;
	. . .
*/
template <class T> class shared_ptr // T must be public child of shared_object
{
	T *object = nullptr;

	void unconnect()
	{
		if (object) T::dec_ref(object);
	}

	void connect(T *p)
	{
		object = p;
		if (object) object->add_ref();
	}

public:
	shared_ptr() {}
	//shared_ptr(const T &obj):object(new T (obj)) {object->ref = 1;}
	shared_ptr(T *p) {connect(p);} // now safe todo: shared_ptr p1(obj), p2(obj);
	shared_ptr(const shared_ptr &p) {connect(p.object);}
	shared_ptr(shared_ptr &&p):object(p.object) { p.object = nullptr; }
	shared_ptr operator=(shared_ptr &&p) { SWAP(object,p.object); return *this; }

	shared_ptr &operator=(T *p)
	{
		if (p) p->add_ref(); // ref up - to correct self assign
		unconnect();
		object = p;
		return *this;
	}
	shared_ptr &operator=(const shared_ptr &p)
	{
        return *this = p.object;
    }

	~shared_ptr() {unconnect();}

	void swap(shared_ptr &p) {SWAP(*this, p);}

	operator T *() const  {return object;}
	T *operator->() const {return object;}

	T *get() {return object;}
    const T *get() const {return object;}
};

class shared_object
{
	int ref = 0;

	shared_object(const shared_object &) UNUSED;
	void operator=(const shared_object &) UNUSED;

public:
	shared_object() {}

    bool is_multi_ref() const {return ref > 1;}
	void add_ref() {ref++;}
	template <class T> static void dec_ref(T *object)
	{
		ASSERT(object->ref > 0);
		if (--object->ref == 0) TSDEL(object);
	}
};


/*
	not most stupid pointer (аналог std::auto_ptr)
*/
template <typename PTR> class auto_ptr
{
	PTR *pointer = nullptr;

public:

    auto_ptr() {}
	explicit auto_ptr(PTR *p) {pointer = p;}
	auto_ptr(auto_ptr &p) {pointer = p.pointer; p.pointer = nullptr;}
	~auto_ptr() {TSDEL(pointer);}

	operator  PTR *() const {return pointer;}
	PTR *operator->() const {return pointer;}

	auto_ptr &operator=(PTR *p)
	{
		if (!ASSERT(pointer != p || !p)) return *this; // disable assign to self
		TSDEL(pointer);
		pointer = p;
		return *this;
	}
	auto_ptr &operator=(auto_ptr &p) {(*this)=p.pointer; p.pointer = nullptr; return *this;}
};

// (RUS) интрузивный [b]Ќ≈ѕ≈–ћ≈ўј≈ћџ…[/b] слабый указатель (weak)
// intrusive UNMOVABLE weak pointer
// UNMOVABLE means that you cannot use memcpy to copy this pointer

template<class OO> struct eyelet_s;
template<class OO, class OO1 = OO> struct iweak_ptr
{
    typedef eyelet_s<OO> eyelet_my;
	friend struct eyelet_my;
private:
    MOVABLE(false);
	iweak_ptr *prev = nullptr;
	iweak_ptr *next = nullptr;
	OO *oobject = nullptr;

public:

    iweak_ptr() {}
	iweak_ptr(const iweak_ptr &hook)
	{
		if (hook.get()) const_cast<OO *>( hook.get() )->hook_connect( this );
	}

	iweak_ptr(OO1 *ob)
	{
		if (ob) ((OO *)ob)->OO::hook_connect( this );
	}
	~iweak_ptr()
	{
		unconnect();
	}

	void unconnect()
	{
		if (oobject) oobject->hook_unconnect( this );
	}

	iweak_ptr &operator = ( const iweak_ptr & hook )
	{
		if ( hook.get() != get() )
		{
			unconnect();
			if (hook.get()) const_cast<OO *>( hook.get() )->hook_connect( this );
		}
		return *this;
	}

	iweak_ptr &operator = ( OO1 * obj )
	{
		if ( obj != get() )
		{
			unconnect();
			if (obj) obj->OO::hook_connect( this );
		}
		return *this;
	}

    explicit operator bool() {return get() != nullptr;}

    template<typename OO2> bool operator==( const OO2 * obj ) const { return oobject == ptr_cast<const OO2 *>(obj); }

	OO1* operator()() {return static_cast<OO1*>(oobject); }
	const OO1* operator()() const {return static_cast<const OO1*>(oobject); }

	operator OO1* () const { return static_cast<OO1*>(oobject); }
	OO1* operator->() const { return static_cast<OO1*>(oobject); }

	OO1* get() {return static_cast<OO1*>(oobject);}
	const OO1* get() const {return static_cast<OO1*>(oobject);}

    bool expired() const {return get() == nullptr;}
};

template<class OO> struct eyelet_s
{
	iweak_ptr<OO> *first = nullptr;

	eyelet_s() {}
	~eyelet_s()
	{
		iweak_ptr<OO> *f = first;
		for (;f;)
		{
			iweak_ptr<OO> *next = f->next;

			f->oobject = nullptr;
			f->prev = nullptr;
			f->next = nullptr;

			f=next;
		}
	}

	void connect( OO *object, iweak_ptr<OO, OO> * hook )
	{
		if (hook->get()) hook->get()->hook_unconnect( hook );
		hook->oobject = object;
		hook->prev = nullptr;
		hook->next = first;
		if (first) first->prev = hook;
		first = hook;
	}

	void    unconnect( iweak_ptr<OO, OO> * hook )
	{
#ifndef _FINAL
		iweak_ptr<OO> *f = first;
		for (;f;f=f->next)
		{
			if (f == hook) break;
		}
		ASSERT( f == hook, "foreigner hook!!!" );

#endif
		if (first == hook)
		{
			ASSERT( first->prev == nullptr );
			first = hook->next;
			if (first)
			{
				first->prev = nullptr;
			}
			hook->next = nullptr;
		} else
		{
			ASSERT( hook->prev != nullptr );
			hook->prev->next = hook->next;
			if (hook->next) { hook->next->prev = hook->prev; hook->next = nullptr; };
			hook->prev = nullptr;
		}
		hook->oobject = nullptr;
	}
};

#define DECLARE_EYELET( obj ) private: ts::eyelet_s<obj> m_eyelet; public: \
	template<class OO1> void hook_connect( ts::iweak_ptr<obj, OO1> * hook ) { m_eyelet.connect(this, reinterpret_cast<ts::iweak_ptr<obj>*>(hook)); } \
	template<class OO1> void hook_unconnect( ts::iweak_ptr<obj, OO1> * hook ) { m_eyelet.unconnect(reinterpret_cast<ts::iweak_ptr<obj>*>(hook)); } private:


} // namespace ts

