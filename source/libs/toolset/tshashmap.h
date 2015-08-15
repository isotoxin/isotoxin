/*
    (C) 2010-2015 TAV, ROTKAERMOTA
*/
#pragma once

namespace ts
{

inline unsigned calc_hash(const int i)
{
	return (unsigned)i;
}

inline unsigned calc_hash(const unsigned i)
{
	return i;
}

inline unsigned calc_hash(const void *obj, unsigned len/*, unsigned int seed*/)
{
	//MurmurHash2
	const unsigned m = 0x5bd1e995;//'m' and 'r' are mixing constants generated offline.
	const int r = 24;             //They're not really 'magic', they just happen to work well.
	unsigned h = /*seed ^*/ len;  //Initialize the hash to a 'random' value

	const unsigned char *data = (unsigned char*)obj;//Mix 4 bytes at a time into the hash
	unsigned rem = len & 3;
	for (len >>= 2; len > 0; data += 4, len--)
	{
		unsigned k = *(unsigned*)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 

		h *= m; 
		h ^= k;
	}

	switch (rem)//Handle the last few bytes of the input array
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
			h *= m;
	};

	h ^= h >> 13;//Do a few final mixes of the hash to ensure the last few
	h *= m;      //bytes are well-incorporated.
	h ^= h >> 15;

	return h;
}

template<typename TCHARACTER> unsigned calc_hash(const sptr<TCHARACTER> &s)
{
    return calc_hash(s.s, s.l * sizeof(TCHARACTER));
}

template<typename TCHARACTER, class CORE> unsigned calc_hash(const str_t<TCHARACTER, CORE> &s)
{
	return calc_hash(s.as_sptr());
}

template<typename T1, typename T2> unsigned calc_hash(const pair_s<T1, T2> &s)
{
    return calc_hash(s.first) ^ ts::hash_func( calc_hash(s.second) );
}

//template <class T, int N> inline unsigned calc_hash(const vec_t<T,N> &v)
//{
//	return calc_hash(&v, sizeof(v));
//}

template <class T> inline unsigned calc_hash(const T &obj) // bad for aligned structures. please write its own calc_hash
{
	DEBUGCODE(static const T t[1] = {1L};) //-V808 // compile-time check to allow only POD and non-pointer types
	return calc_hash(&obj, sizeof(obj));
}

struct Void {};

template <class KEYTYPE, class VALTYPE = Void> class hashmap_t
{
public:
	struct litm_s;
private:
	litm_s **table = nullptr;
	int table_size = 0;
    int used = 0;

public:

	struct litm_s
	{
		KEYTYPE key;
		unsigned key_hash;
		VALTYPE value;

		litm_s *next;
	};

	class iterator
	{
		const hashmap_t *hashmap;
		int table_index;
		litm_s *item = nullptr;

	public:
		iterator(const hashmap_t *hashmap, int start_index = -1) : hashmap(hashmap), table_index(start_index) {}

		const KEYTYPE &key() const {ASSERT(operator bool()); return item->key;}
        VALTYPE &value() { return  item->value; }

		VALTYPE &operator* () { return  item->value; }
		VALTYPE *operator->() { return &item->value; }

		explicit operator bool() const {return table_index < hashmap->table_size;}

		iterator &operator++()
		{
			if (item)
                item = item->next;

			while (nullptr == item)
			{
				if (++table_index < hashmap->table_size)
					item = hashmap->table[ table_index ];
				else
				{
					item = nullptr;
					break;
				}
			}

			return *this;
		}

		iterator operator++(int)
		{
			iterator p = *this;
			++*this;
			return p;
		}

		bool operator==(const iterator &i) const
		{
			return hashmap == i.hashmap && table_index == i.table_index && item == i.item;
		}

		bool operator!=(const iterator &i) const { return !operator==(i); }

		void remove() // remove current element and go to next element
		{
			if (!ASSERT(operator bool())) return;

			litm_s *item2remove = item, **li = &hashmap->table[table_index];
			operator++();

			for (; *li; li = &(*li)->next)
				if (*li == item2remove)
				{
					litm_s *el = *li;
					*li = (*li)->next;
					TSDEL(el);
					const_cast<hashmap_t*>(hashmap)->used--;
					return;
				}

			WARNING("Element not found?");
		}
	};

	iterator begin() const { return ++iterator(this); }
	iterator   end() const { return   iterator(this, table_size); }

	hashmap_t() {}
	hashmap_t(int size)
	{
		reserve(size);
	}
    hashmap_t(const hashmap_t &hm)
    {
        *this = hm;
    }
    hashmap_t(hashmap_t &&hm)
    {
        *this = std::move(hm);
    }

	~hashmap_t()
	{
		clear();
		if (table) MM_FREE(table);
	}

	void reserve(int size)
	{
		if (size > table_size)
		{
			litm_s **new_table = (litm_s**)MM_ALLOC(sizeof(litm_s*)*size);
			size = MM_SIZE(new_table)/sizeof(litm_s*);
			memset(new_table, 0, sizeof(litm_s*)*size);

			if (table) // move data from old table to new one
			{
				for (int i=0; i<table_size; i++)
					for (litm_s *sli = table[i], *next; sli; sli = next)
					{
						next = sli->next;
						litm_s **li = new_table + sli->key_hash % unsigned(size);
						sli->next = *li;
						*li = sli;
					}

				MM_FREE(table);
			}

			table = new_table;
			table_size = size;
		}
	}

	int  size() const { return used; }
	bool is_empty() const { return used == 0; }

	void clear()
	{
		for (int i=0; i<table_size; ++i)
		{
			for (litm_s *li = table[i], *next; li; li = next)
			{
				next = li->next;
				TSDEL(li);
			}
			table[i] = nullptr;
		}

		used = 0;
	}

    hashmap_t &operator=(hashmap_t &&hm)
    {
        SWAP(table, hm.table);
        SWAP(table_size, hm.table_size);
        SWAP(used, hm.used);

        return *this;
    }

	hashmap_t &operator=(const hashmap_t &hm)
	{
		if (&hm == this) return *this;

        clear();
        if (!hm.empty())
        {
            if (table) MM_FREE(table);
            table = (litm_s**)MM_ALLOC(sizeof(litm_s*)*hm.table_size);
            table_size = hm.table_size;
            used = hm.used;

            for (int i=0; i<table_size; i++)
            {
                litm_s **prev_next = &table[i];
                for (litm_s *sli = hm.table[i]; sli; sli = sli->next)
                {
                    litm_s *new_li = TSNEW(litm_s);
                    *prev_next = new_li;
                    new_li->key = sli->key;
                    new_li->key_hash = sli->key_hash;
                    new_li->value = sli->value;
                    prev_next = &new_li->next;
				}
				*prev_next = nullptr;
            }
        }

        return *this;
    }

    template<typename COMPARTIBLE_KEY> litm_s &add_get_item(const COMPARTIBLE_KEY &key, bool &added)
	{
		unsigned hash = calc_hash(key);
		litm_s **li = nullptr;
		if (table)
		{
			li = table + hash % unsigned(table_size);
			DEBUGCODE(int collisions = 0;)
			while (*li)
			{
				if ((*li)->key_hash == hash && (*li)->key == key) // element already present
				{
					added = false;
					return *(*li);
				}
				li=&(*li)->next;
				DEBUGCODE(collisions++;)
			}
			ASSERT(collisions < 7, "Too many hash collisions, please check hash function"); // bad hash func :(
		}

		if (used >= table_size/2) // 50% table fill
		{
			reserve( tmax(10, table_size * 2) );
			li = table + hash % unsigned(table_size); // recalc ptr due new table size
		}

		// add new element
		added = true;
		used++;
		litm_s *new_li = TSNEW(litm_s);
		new_li->key = key;
		new_li->key_hash = hash;
		new_li->next = *li;
		*li = new_li;
		return *new_li;
	}
	template<typename COMPARTIBLE_KEY> VALTYPE &add(const COMPARTIBLE_KEY &key, bool &added)
	{
		return add_get_item(key, added).value;
	}
	template<typename COMPARTIBLE_KEY> VALTYPE &add(const COMPARTIBLE_KEY &key)
	{
		bool added;
		return add_get_item(key, added).value;
	}

	template<typename COMPARTIBLE_KEY> bool remove(const COMPARTIBLE_KEY &key)
	{
		if (table_size == 0) return false;
		unsigned hash = calc_hash(key);

		for (litm_s **li = &table[hash % unsigned(table_size)]; *li; li = &(*li)->next)
			if ((*li)->key_hash == hash && (*li)->key == key)
			{
				litm_s *el = *li;
				*li = (*li)->next;
				TSDEL(el);
				used--;
				return true;
			}

		return false;
	}

    template<typename COMPARTIBLE_KEY, typename GETHANDLER> bool getremove(const COMPARTIBLE_KEY &key, GETHANDLER gh)
    {
        if (table_size == 0) return false;
        unsigned hash = calc_hash(key);

        for (litm_s **li = &table[hash % unsigned(table_size)]; *li; li = &(*li)->next)
            if ((*li)->key_hash == hash && (*li)->key == key)
            {
                litm_s *el = *li;
                gh( el->value );
                *li = (*li)->next;
                TSDEL(el);
                used--;
                return true;
            }

        return false;
    }

	template<typename COMPARTIBLE_KEY> const litm_s *find(const COMPARTIBLE_KEY &key) const
	{
		if (table_size == 0) return nullptr;
		unsigned hash = calc_hash(key);

		for (const litm_s *li = table[hash % unsigned(table_size)]; li; li = li->next)
			if (li->key_hash == hash && li->key == key) return li;

		return nullptr;
	}
	template<typename COMPARTIBLE_KEY> litm_s *find(const COMPARTIBLE_KEY &key)
	{
		return const_cast<litm_s*>(const_cast<const hashmap_t*>(this)->find(key));
	}

	template<typename COMPARTIBLE_KEY> const VALTYPE *get(const COMPARTIBLE_KEY &key) const
	{
		if (const litm_s *li = find(key)) return &li->value;
		return nullptr;
	}

	template<typename COMPARTIBLE_KEY> VALTYPE *get(const COMPARTIBLE_KEY &key)
	{
		if (litm_s *li = find(key)) return &li->value;
		return nullptr;
	}

	template<typename COMPARTIBLE_KEY> VALTYPE &operator[](const COMPARTIBLE_KEY &key)
	{
		bool added;
		return add(key, added);
	}

	template<typename COMPARTIBLE_KEY> const VALTYPE &operator[](const COMPARTIBLE_KEY &key) const
	{
		if (const VALTYPE *val = get(key)) return *val;
		static VALTYPE defValue;
		return defValue;
	}
};

} // namespace ts

