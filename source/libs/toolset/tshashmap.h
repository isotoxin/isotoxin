/*
    (C) 2010-2015 TAV
*/
#pragma once

namespace ts
{

inline unsigned GetHash(const int i)
{
	return (unsigned)i;
}

inline unsigned GetHash(const unsigned i)
{
	return i;
}

inline unsigned GetHash(const void *obj, unsigned len/*, unsigned int seed*/)
{
/*
	unsigned hash = 0;
	for (size_t i=0,l=size%sizeof(unsigned),e=size-l; i<l; i++)
		hash |= unsigned(((unsigned char*)obj)[e+i]) << (i*8);

	for (size_t i=0; i<size/sizeof(unsigned); i++)
		hash ^= ((unsigned*)obj)[i];

	return hash;
*/
/*	unsigned hash = 2166136261u;//FNV-1a
	for (size_t i=0; i<size; i++)
		hash = 16777619u * (hash ^ ((unsigned char*)obj)[i]);
	return hash;*/
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

template<typename TCHARACTER> unsigned GetHash(const sptr<TCHARACTER> &s)
{
    return GetHash(s.s, s.l * sizeof(TCHARACTER));
}

template<typename TCHARACTER, class CORE> unsigned GetHash(const str_t<TCHARACTER, CORE> &s)
{
	return GetHash(s.as_sptr());
}

template<typename T1, typename T2> unsigned GetHash(const pair_s<T1, T2> &s)
{
    return GetHash(s.first) ^ ts::hash_func( GetHash(s.second) );
}

//template <class T, int N> inline unsigned GetHash(const Tvec<T,N> &v)
//{
//	return GetHash(&v, sizeof(v));
//}

template <class T> inline unsigned GetHash(const T &obj)//Внимание! во избежание глюков с "дырявыми" структурами {short;int;} рекомендуется выставлять memalign в 1b, всегда объявлять поля структур в порядке уменьшения их размера и/или переопределять эту функцию для таких структур
{
	DEBUGCODE(static const T t[1] = {1L};)//compile-time check to allow only POD and non-pointer types
	return GetHash(&obj, sizeof(obj));
}

struct Void {};

template <class KEYTYPE, class VALTYPE = Void> class hashmap_t
{
public:
	struct litm_s;
private:
	litm_s **table;
	int tableSize,used;

	//friend class Iterator;

public:

	struct litm_s
	{
		KEYTYPE key;
		unsigned keyHash;
		VALTYPE value;

		litm_s *next;
	};

	class iterator
	{
		const hashmap_t *hashMap;
		int tableIndex;
		litm_s *item;

	public:
		iterator(const hashmap_t *hashMap, int startIndex = -1) : hashMap(hashMap), tableIndex(startIndex), item(nullptr) {}

		const KEYTYPE &key() const {ASSERT(operator bool()); return item->key;}
        VALTYPE &value(void) { return  item->value; }

		VALTYPE &operator* () { return  item->value; }
		VALTYPE *operator->() { return &item->value; }

		explicit operator bool() const {return tableIndex < hashMap->tableSize;}

		iterator &operator++()
		{
			if (item) item = item->next;

			while (item == nullptr)
			{
				if (++tableIndex < hashMap->tableSize)
					item = hashMap->table[ tableIndex ];
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
			return hashMap == i.hashMap && tableIndex == i.tableIndex && item == i.item;
		}

		bool operator!=(const iterator &i) const { return !operator==(i); }

		void remove()//удаляет элемент, на который указывает данный итератор, и переходит к следующему элементу
		{
			if (!ASSERT(operator bool())) return;

			litm_s *itemToRemove = item, **li = &hashMap->table[tableIndex];//сохраняем указатель на удаляемый элемент
			operator++();//и переходим к следующему элементу

			for (; *li; li = &(*li)->next)
				if (*li == itemToRemove)
				{
					litm_s *el = *li;
					*li = (*li)->next;
					TSDEL(el);
					const_cast<hashmap_t*>(hashMap)->used--;
					return;
				}

			WARNING("Element not found?");
		}
	};

	iterator begin() const { return ++iterator(this); }
	iterator   end() const { return   iterator(this, tableSize); }

	hashmap_t():table(nullptr),tableSize(0),used(0) {}
	hashmap_t(int size):table(nullptr),tableSize(0),used(0)
	{
		reserve(size);
	}

	~hashmap_t()
	{
		clear();
		if (table) MM_FREE(table);
	}

	void reserve(int size)
	{
		if (size > tableSize)
		{
			litm_s **newTable = (litm_s**)MM_ALLOC(sizeof(litm_s*)*size);
			size = MM_SIZE(newTable)/sizeof(litm_s*);
			memset(newTable, 0, sizeof(litm_s*)*size);

			if (table) //переносим все данные из старой таблицы в новую
			{
				for (int i=0; i<tableSize; i++)
					for (litm_s *sli = table[i], *next; sli; sli = next)
					{
						next = sli->next;
						litm_s **li = newTable + sli->keyHash % unsigned(size);
						sli->next = *li;
						*li = sli;
					}

				MM_FREE(table);
			}

			table = newTable;
			tableSize = size;
		}
	}

	int   size() const { return used; }
	int length() const { return used; }
	bool empty() const { return used == 0; }

	void clear()
	{
		for (int i=0; i<tableSize; i++)
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
        SWAP(tableSize, hm.tableSize);
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
			table = (litm_s**)MM_ALLOC(sizeof(litm_s*)*hm.tableSize);
			tableSize = hm.tableSize;
			used = hm.used;

			for (int i=0; i<tableSize; i++)
			{
				litm_s **prevNext = &table[i];
				for (litm_s *sli = hm.table[i]; sli; sli = sli->next)
				{
					litm_s *newLI = TSNEW(litm_s);
					*prevNext = newLI;
					newLI->key     = sli->key;
					newLI->keyHash = sli->keyHash;
					newLI->value   = sli->value;
					prevNext = &newLI->next;
				}
				*prevNext = nullptr;
			}
		}

		return *this;
	}

	template<typename COMPARTIBLE_KEY> litm_s &addAndReturnItem(const COMPARTIBLE_KEY &key, bool &added)
	{
		unsigned hash = GetHash(key);
		litm_s **li = nullptr;
		if (table)
		{
			li = table + hash % unsigned(tableSize);
			DEBUGCODE(int collisions = 0;)
			while (*li)
			{
				if ((*li)->keyHash == hash && (*li)->key == key) //такой элемент уже есть
				{
					added = false;
					return *(*li);
				}
				li=&(*li)->next;
				DEBUGCODE(collisions++;)
			}
			ASSERT(collisions < 7, "Too many hash collisions, please check hash function");//большое кол-во коллизий скорее всего обусловлено неправильной хэш функцией
		}

		if (used >= tableSize/2) //занято уже 50% таблицы или более
		{
			reserve( tmax(10, tableSize * 2) );
			li = table + hash % unsigned(tableSize);//пересчитываем указатель, т.к. размер таблицы уже другой
		}

		//Добавляем новый элемент
		added = true;
		used++;
		litm_s *newLI = TSNEW(litm_s);
		newLI->key     = key;
		newLI->keyHash = hash;
		newLI->next = *li;
		*li = newLI;
		return *newLI;
	}
	template<typename COMPARTIBLE_KEY> VALTYPE &add(const COMPARTIBLE_KEY &key, bool &added)
	{
		return addAndReturnItem(key, added).value;
	}
	template<typename COMPARTIBLE_KEY> VALTYPE &add(const COMPARTIBLE_KEY &key)
	{
		bool added;
		return addAndReturnItem(key, added).value;
	}

	template<typename COMPARTIBLE_KEY> bool remove(const COMPARTIBLE_KEY &key)
	{
		if (tableSize == 0) return false;
		unsigned hash = GetHash(key);

		for (litm_s **li = &table[hash % unsigned(tableSize)]; *li; li = &(*li)->next)
			if ((*li)->keyHash == hash && (*li)->key == key)
			{
				litm_s *el = *li;
				*li = (*li)->next;
				TSDEL(el);
				used--;
				return true;
			}

		return false;
	}

	template<typename COMPARTIBLE_KEY> const litm_s *find(const COMPARTIBLE_KEY &key) const
	{
		if (tableSize == 0) return nullptr;
		unsigned hash = GetHash(key);

		for (const litm_s *li = table[hash % unsigned(tableSize)]; li; li = li->next)
			if (li->keyHash == hash && li->key == key) return li;

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

