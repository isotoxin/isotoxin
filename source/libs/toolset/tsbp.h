/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

namespace ts {

template <typename TCHARACTER=char> class bp_t
{
	typedef str_t<TCHARACTER> string_type;
	string_type value;
	mutable double double_value_cache = -DBL_MAX;
    struct element_s;
    element_s *first_element = nullptr, *last_element = nullptr; // list with original order
	hashmap_t<string_type, bp_t*> elements;

	bool preserve_comments;

	DEBUGCODE(const TCHARACTER *source_basis;)
	int get_current_line(const TCHARACTER *s);

	bp_t(const bp_t &) UNUSED;
    void operator=(const bp_t &oth) UNUSED;

    void overwrite(const bp_t &oth)
    {
        value = oth.value;
        double_value_cache = oth.double_value_cache;
        for (auto it = oth.begin(); it; ++it)
            set(it.name()).overwrite(*it);
    }

public:

    enum mergeoptions
    {
        SKIP_EXIST = 1,
    };

	bp_t(bool presComments = false): preserve_comments(presComments) DEBUGCODE(,source_basis(nullptr)) {}
	~bp_t()
	{
		for (element_s *e=first_element, *next; e; e=next) {next = e->next; TSDEL(e);}
	}

    bp_t& operator=(bp_t &&oth)
    {
        preserve_comments = oth.preserve_comments;
        value = oth.value;
        SWAP(double_value_cache, oth.double_value_cache);
        SWAP(first_element, oth.first_element);
        SWAP(last_element, oth.last_element);
        elements = std::move(oth.elements);
        return *this;
    }

    bool value_not_specified() const
    {
        return double_value_cache == DBL_MAX;
    }
	bool is_empty() const {return first_element == nullptr;}

	void clear()
	{
		for (element_s *e=first_element, *next; e; e=next) {next = e->next; TSDEL(e);}
		first_element = last_element = nullptr;
		value.clear();
		double_value_cache = DBL_MAX;
		elements.clear();
	}

	class iterator
	{
		friend class bp_t;
		element_s *el;
	public:
		const string_type name() const {return el ? *el->name : string_type();}
		operator bp_t*() {return el ? &el->bp : nullptr;}
		bp_t *operator->() {ASSERT(el); return &el->bp;}
		void operator++() {el = el->next;}
        bool operator!=(const iterator &it) const {return el != it.el;}
        //bool operator==(const bp_t *bp) const {return &el->bp == bp;}
	};
	iterator begin() const {iterator it; it.el=first_element; return it;}
    iterator end() const {iterator it; it.el = nullptr; return it;}

    void merge( const bp_t &oth, uint32 options )
    {
        if (0 != (options & SKIP_EXIST))
        {
            for (auto it = oth.begin(); it; ++it)
            {
                if (auto *ibp = get(it.name()))
                {
                    ibp->merge( *it, options );
                    continue;
                }
                set(it.name()).overwrite(*it);
            }
        } else 
            overwrite(oth);
    }

    bool present( const bp_t * bp ) const
    {
        for (auto it = begin(); it; ++it)
            if (it == bp) return true;
        return false;
    }

    bool present_r(const bp_t * bp) const
    {
        for (auto it = begin(); it; ++it)
            if (it == bp)
                return true;
            else
                if (it->present_r(bp))
                    return true;
        return false;
    }

    bool get_remove(const sptr<TCHARACTER> &name, bp_t &bpout)
    {
        element_s *el = nullptr;
        elements.getremove(name, [&](bp_t *bp) {

            iterator it;
            element_s *prev = nullptr;
            for (it = begin(); it; prev = it.el, ++it)
                if (it == bp) break;
            if (!ASSERT(it)) return;

            (prev ? prev->next : first_element) = it.el->next;
            if (it.el->next == nullptr) last_element = prev;
            it.el->next = nullptr;
            el = it.el;

        });
        if (el)
        {
            bpout = std::move(el->bp);
            TSDEL(el);
            return true;
        }
        return false;
    }

	bp_t *get(const sptr<TCHARACTER> &name)
	{
#ifdef _DEBUG
		if (bp_t *const*bp = elements.get(name))
		{
			if (*bp) return *bp;
			WARNING("duplicate blockpar get attempt");
			for (element_s *e=first_element; e; e=e->next)
				if (*e->name == name) return &e->bp;
		}
		return nullptr;
#else
		if (bp_t *const*bp = elements.get(name)) return *bp;
		return nullptr;
#endif
	}
	const bp_t *get(const sptr<TCHARACTER> &name) const {return const_cast<bp_t*>(this)->get(name);}

	bp_t &get_safe(const sptr<TCHARACTER> &name)
	{
		if (bp_t *bp = get(name)) return *bp;
		static static_setup<bp_t> defaultBP;
		return defaultBP();
	}
	const bp_t &get_safe(const sptr<TCHARACTER> &name) const {return const_cast<bp_t*>(this)->get_safe(name);}

	bp_t *get_at_path(const sptr<TCHARACTER> &path)
	{
		bp_t *bp = this;
		for (token<TCHARACTER> t(path, TCHARACTER('/')); t; ++t)
			if ((bp = bp->get(*t)) == nullptr) break;
		return bp;
	}
	const bp_t *get_at_path(const sptr<TCHARACTER> &path) const {return const_cast<bp_t*>(this)->get_at_path(path);}

	bp_t &set(const sptr<TCHARACTER> &name)
	{
		if (bp_t *bp = get(name)) return *bp;
		return add_block(name);
	}

	bp_t &set_at_path(const sptr<TCHARACTER> &path)
	{
		bp_t *bp = this, *nbp;
		for (token<TCHARACTER> t(path, TCHARACTER('/')); t; ++t)
		{
			if ((nbp = bp->get(*t)) == nullptr) nbp = &bp->add_block(*t);
			bp = nbp;
		}
		return *bp;
	}

	void remove(const sptr<TCHARACTER> &name)
	{
        elements.getremove(name, [this](bp_t *bp){
            iterator it;
            element_s *prev = nullptr;
            for (it = begin(); it; prev = it.el, ++it)
                if (it == bp) break;
            if (!ASSERT(it)) return;

            (prev ? prev->next : first_element) = it.el->next;
            if (it.el->next == nullptr) last_element = prev;
            TSDEL(it.el);
        });
	}

	void remove_at_path(const sptr<TCHARACTER> &path)
	{
		token<TCHARACTER>::tokentype name;
		bp_t *bp = this, *nbp;
		for (token<TCHARACTER> t(path, TCHARACTER('/')); ; bp = nbp)
		{
			if ((nbp = bp->get(name = *t)) == nullptr) break;
			++t;
			if (!t) {bp->remove(name); break;}
		}
	}

	string_type as_string(const string_type &def = string_type()) const
	{
		if (value_not_specified()) return def;
		return value;
	}

	double as_double(double def = 0.0) const
	{
		if (value_not_specified()) return def;
		if (double_value_cache == -DBL_MAX) double_value_cache = value.as_double();
		return double_value_cache;
	}
	int as_int(int def = 0) const {return (int)as_double(def);}

	string_type get_string(const sptr<TCHARACTER> &name, const string_type &def = string_type()) const
	{
		if (const bp_t *bp = get(name)) return bp->as_string(def);
		return def;
	}

	string_type get_string_at_path(const sptr<TCHARACTER> &path, const string_type &def = string_type()) const
	{
		if (const bp_t *bp = get_at_path(path)) return bp->as_string(def);
		return def;
	}

	double get_double(const sptr<TCHARACTER> &name, double def = 0.0) const
	{
		if (const bp_t *bp = get(name)) return bp->as_double(def);
		return def;
	}
	int get_int(const sptr<TCHARACTER> &name, int def = 0) const {return (int)get_double(name, def);}

	template <class T> void get_value(T &val, const string_type &name, const T &def = T())
	{	val = (T)get_double(name, def);   }
	template <> void get_value<string_type>(string_type &val, const string_type &name, const string_type &def)
	{	val = get_string(name, def);   }

	template <class T> void as_value(T &val, const T &def = T())
	{	val = (T)as_double(def);   }
	template <> void as_value<string_type>(string_type &val, const string_type &def)
	{	val = as_string(def);   }

	void set_value(const sptr<TCHARACTER> &val)
	{
		value = val;
		double_value_cache = -DBL_MAX;
	}
	void set_value(const string_type &val)
	{
		value = val;
		double_value_cache = -DBL_MAX;
	}
	void set_value(double val)
	{
		value.set_as_double(val);
		double_value_cache = val;
	}
	void set_value(float val)
	{
		value.set_as_float(val);
		double_value_cache = val;
	}
	void set_value(int val)
	{
		value.set_as_int(val);
		double_value_cache = (double)val;
	}

	bp_t &add_block(const string_type &name);

	bool read_bp(const TCHARACTER *&s, const TCHARACTER *end);
	const TCHARACTER *load(const TCHARACTER *data, const TCHARACTER *end);
	const TCHARACTER *load(const sptr<TCHARACTER> &data) {return load(data.s, data.s+data.l);}
	const TCHARACTER *load(const string_type &str) {return load(str.as_sptr());}
	const string_type store(int=0) const;
};

template <typename TCHARACTER> struct bp_t<TCHARACTER>::element_s
{
    string_type *name;// if name == "=", then comment
    bp_t<TCHARACTER> bp;
    element_s *next;
};

template <typename TCHARACTER> class bpreader
{
	const TCHARACTER *data, *end;
	bp_t<TCHARACTER> bp;
public:

	bpreader(const TCHARACTER *data, const TCHARACTER *end) : data(data), end(end) {operator++();}
	bpreader(const TCHARACTER *data, int size)  : data(data), end(data+size) {operator++();}
	explicit operator bool() const {return !bp.empty();}
	const str_t<TCHARACTER> name() const {return bp.begin().name();}
	operator   bp_t<TCHARACTER>*() {return bp.begin();}
	bp_t<TCHARACTER> *operator->() {return bp.begin();}
	void operator++()
	{
		bp.clear();
		if (data)
		while (bp.read_bp(data, end))
		{
			if (!ASSERT(&*bp.begin())) continue;
			break;
		}
	}
};

typedef bp_t<char> abp_c;
typedef bp_t<wchar> wbp_c;

} // namespace ts
