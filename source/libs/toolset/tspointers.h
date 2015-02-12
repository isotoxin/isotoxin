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
	} *pc;

	safe_object(const safe_object &) UNUSED;
	void operator=(const safe_object &) UNUSED;

public:

    static const aint POINTER_CONTAINER_SIZE = sizeof(pointer_container_s);

	safe_object():pc(nullptr) {}

    void make_all_ponters_expired() // use carefully! you can get memory leak if there are no regular pointers to this object
    {
        if (pc)
        {
            pc->pointer = nullptr;
            pc = nullptr;
        }
    }

	~safe_object()
	{
		if (pc) pc->pointer = nullptr;
	}
};


/*
	Перемещаемый слабый указатель (weak pointer)
	Автоматически обнуляется при уничтожении объекта, на который указывает.
*/
template <typename T> class safe_ptr // T must be public child of safe_object
{
    MOVABLE(true);
	friend class safe_object;
	template <class T> friend class safe_ptr;

	safe_object::pointer_container_s *pc;

	void unconnect()//for internal use only
	{
		if (pc)
		{
			if (--pc->ref == 0)
			{
				if (pc->pointer) pc->pointer->pc = nullptr;
				pc->die();
			}
		}
	}

	void connect(T *p)//for internal use only
	{
		if (p)
		{
			if (p->pc)
			{
				pc = p->pc;
				pc->ref++;
			}
			else
			{
				pc = p->pc = safe_object::pointer_container_s::create(p);
			}
		}
		else pc = nullptr;
	}

	T *pointer() const {return pc ? static_cast<T*>(pc->pointer) : nullptr;}

public:

	safe_ptr():pc(nullptr) {}
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
	Указатель на разделяемый объект (intrusive shared pointer)
	Гарантирует существование объекта пока существует хоть один разделяемый указатель на него,
	т.о. указатель не может быть nullptr.

	Пример использования:
	shared_ptr<MyClass> p(new MyClass(...)), p2(p), p3=p;
	. . .
*/
template <class T> class shared_ptr // T must be public child of shared_object
{
	T *object;

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
	shared_ptr():object(nullptr) {}
	//shared_ptr(const T &obj):object(new T (obj)) {object->refCount = 1;}
	shared_ptr(T *p) {connect(p);}//это намного безопаснее чем refCount=0, т.к. ситуации shared_ptr p1(obj), p2(obj); работают корректно, более того это возможно только для интрузивного shared_ptr
	shared_ptr(const shared_ptr &p) {connect(p.object);}
	shared_ptr(shared_ptr &&p):object(p.object) { p.object = nullptr; }
	shared_ptr operator=(shared_ptr &&p) { SWAP(object,p.object); return *this; }

	shared_ptr &operator=(T *p)
	{
		if (p) p->add_ref();//сначала поднимаем refCount, чтобы работало присваивание shared_ptr'а самому себе
		unconnect();
		object = p;
		return *this;
	}
	shared_ptr &operator=(const shared_ptr &p)//нужно, т.к. иначе при использовании shared_ptr в других классах некорректно сгенерируется дефолтный оператор присваивания
	{   return *this = p.object;   }

	~shared_ptr() {unconnect();}

	void swap(shared_ptr &p) {SWAP(*this, p);}

	operator T *() const  {return object;}
	T *operator->() const {return object;}

	T *get() {return object;}
    const T *get() const {return object;}
private:
	//shared_ptr &operator=(T *p);//чтобы нельзя было написать: MyClass *p=new MyClass; sp=p; sp2=p; (но делать так теперь уже в принципе можно)
};

class shared_object
{
	int ref;

	shared_object(const shared_object &) UNUSED;//запрещаем копирование
	void operator=(const shared_object &) UNUSED;//и присваивание

public:
	shared_object() : ref(0) {}

    bool is_multi_ref() const {return ref > 1;}
	void add_ref() {ref++;}
	template <class T> static void dec_ref(T *object)
	{
		ASSERT(object->ref > 0);
		if (--object->ref == 0) TSDEL(object);
	}
};


/*
	Не-самый-глупый-указатель (аналог std::auto_ptr)
	Семантика разрушения при копировании. Допускает присваивание указателю, в отличие от auto_ptr, но его также нельзя ложить в массив.
*/
template <typename PTR> class auto_ptr
{
	PTR *pointer;

public:

	auto_ptr():pointer(nullptr) {}
	explicit auto_ptr(PTR *p) {pointer = p;}
	auto_ptr(auto_ptr &p) {pointer = p.pointer; p.pointer = nullptr;}
	~auto_ptr() {TSDEL(pointer);}

	operator  PTR *() const {return pointer;}
	PTR *operator->() const {return pointer;}

	auto_ptr &operator=(PTR *p)
	{
		if (!ASSERT(pointer != p || !p)) return *this;//запрещаем присваивание самому себе
		TSDEL(pointer);
		pointer = p;
		return *this;
	}
	auto_ptr &operator=(auto_ptr &p) {(*this)=p.pointer; p.pointer = nullptr; return *this;}
};

// (RUS) интрузивный [b]НЕПЕРМЕЩАЕМЫЙ[/b] слабый указатель (weak)
// intrusive UNMOVABLE weak pointer
// UNMOVABLE means that you cannot use memcpy to copy this pointer

template<class OO> struct eyelet_s;
template<class OO, class OO1 = OO> struct iweak_ptr
{
    typedef eyelet_s<OO> eyelet_my;
	friend struct eyelet_my;
private:
    MOVABLE(false);
	iweak_ptr *prev;
	iweak_ptr *next;
	OO *oobject;

public:

	iweak_ptr(void):oobject(nullptr),prev(nullptr),next(nullptr) {}
	iweak_ptr(const iweak_ptr &hook):oobject(nullptr),prev(nullptr),next(nullptr)
	{
		if (hook.get()) const_cast<OO *>( hook.get() )->hook_connect( this );
	}

	iweak_ptr(OO1 *ob):oobject(nullptr),prev(nullptr),next(nullptr)
	{
		if (ob) ((OO *)ob)->OO::hook_connect( this );
	}
	~iweak_ptr()
	{
		unconnect();
	}

	void unconnect(void)
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
	iweak_ptr<OO> *first;

	eyelet_s(void) { first = nullptr; }
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

