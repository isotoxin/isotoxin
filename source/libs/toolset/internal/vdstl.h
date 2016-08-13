//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef VD2_SYSTEM_VDSTL_H
#define VD2_SYSTEM_VDSTL_H

#include "vdtypes.h"
#include "memory.h"

///////////////////////////////////////////////////////////////////////////
//
//	glue
//
///////////////////////////////////////////////////////////////////////////

template<class Category, class T, class Distance = ptrdiff_t, class Pointer = T*, class Reference = T&>
struct vditerator {
#if defined(VD_COMPILER_MSVC) && (VD_COMPILER_MSVC < 1310 || (defined(VD_COMPILER_MSVC_VC8_PSDK) || defined(VD_COMPILER_MSVC_VC8_DDK)))
	typedef std::iterator<Category, T, Distance> type;
#else
	typedef std::iterator<Category, T, Distance, Pointer, Reference> type;
#endif
};

template<class Iterator, class T>
struct vdreverse_iterator {
#if defined(VD_COMPILER_MSVC) && (VD_COMPILER_MSVC < 1310 || (defined(VD_COMPILER_MSVC_VC8_PSDK) || defined(VD_COMPILER_MSVC_VC8_DDK)))
	typedef std::reverse_iterator<Iterator, T> type;
#else
	typedef std::reverse_iterator<Iterator> type;
#endif
};

struct vdlist_node {
	vdlist_node *mListNodeNext, *mListNodePrev;
};

template<class T, class T_Nonconst>
class vdlist_iterator : public vditerator<std::bidirectional_iterator_tag, T, ptrdiff_t>::type {
public:
	vdlist_iterator() {}
	vdlist_iterator(vdlist_node *p) : mp(p) {}
	vdlist_iterator(const vdlist_iterator<T_Nonconst, T_Nonconst>& src) : mp(src.mp) {}

	T* operator *() const {
		return static_cast<T*>(mp);
	}

	bool operator==(const vdlist_iterator<T, T_Nonconst>& x) const {
		return mp == x.mp;
	}

	bool operator!=(const vdlist_iterator<T, T_Nonconst>& x) const {
		return mp != x.mp;
	}

	vdlist_iterator& operator++() {
		mp = mp->mListNodeNext;
		return *this;
	}

	vdlist_iterator& operator--() {
		mp = mp->mListNodePrev;
		return *this;
	}

	vdlist_node *mp;
};

template<class T>
class vdlist {
public:
	typedef	vdlist_node						node;
	typedef	T*								value_type;
	typedef	T**								pointer;
	typedef	const T**						const_pointer;
	typedef	T*&								reference;
	typedef	const T*&						const_reference;
	typedef	size_t							size_type;
	typedef	ptrdiff_t						difference_type;
	typedef	vdlist_iterator<T, T>						iterator;
	typedef vdlist_iterator<const T, T>					const_iterator;
	typedef typename vdreverse_iterator<iterator, T>::type			reverse_iterator;
	typedef typename vdreverse_iterator<const_iterator, const T>::type	const_reverse_iterator;

	vdlist() {
		mAnchor.mListNodePrev	= &mAnchor;
		mAnchor.mListNodeNext	= &mAnchor;
	}

	bool empty() const {
		return mAnchor.mListNodeNext == &mAnchor;
	}

	size_type size() const {
		node *p = { mAnchor.mListNodeNext };
		size_type s = 0;

		if (p != &mAnchor)
			do {
				++s;
				p = p->mListNodeNext;
			} while(p != &mAnchor);

		return s;
	}

	iterator begin() {
		iterator it(mAnchor.mListNodeNext);
		return it;
	}

	const_iterator begin() const {
		const_iterator it(mAnchor.mListNodeNext);
		return it;
	}

	iterator end() {
		iterator it(&mAnchor);
		return it;
	}

	const_iterator end() const {
		const_iterator it(&mAnchor);
		return it;
	}

	reverse_iterator rbegin() {
		return reverse_iterator(begin());
	}

	const_reverse_iterator rbegin() const {
		return const_reverse_iterator(begin());
	}

	reverse_iterator rend() {
		return reverse_iterator(end);
	}

	const_reverse_iterator rend() const {
		return const_reverse_iterator(end());
	}

	const value_type front() const {
		return static_cast<T *>(mAnchor.mListNodeNext);
	}

	const value_type back() const {
		return static_cast<T *>(mAnchor.mListNodePrev);
	}

	iterator find(T *p) {
		iterator it(mAnchor.mListNodeNext);

		if (it.mp != &mAnchor)
			do {
				if (it.mp == static_cast<node *>(p))
					break;

				it.mp = it.mp->mListNodeNext;
			} while(it.mp != &mAnchor);

		return it;
	}

	const_iterator find(T *p) const {
		const_iterator it(mAnchor.mListNodeNext);

		if (it.mp != &mAnchor)
			do {
				if (it.mp == static_cast<node *>(p))
					break;

				it.mp = it.mp->mListNodeNext;
			} while(it.mp != &mAnchor);

		return it;
	}

	iterator fast_find(T *p) {
		iterator it(p);
		return it;
	}

	const_iterator fast_find(T *p) const {
		iterator it(p);
        return it;
	}

	void clear() {
		while(!empty()) {
			T *p = back();
			erase(p);
			pop_back();
		}
	}

	void push_front(T *p) {
		node& n = *p;
		n.mListNodePrev = &mAnchor;
		n.mListNodeNext = mAnchor.mListNodeNext;
		n.mListNodeNext->mListNodePrev = &n;
		mAnchor.mListNodeNext = &n;
	}

	void push_back(T *p) {
		node& n = *p;
		n.mListNodeNext = &mAnchor;
		n.mListNodePrev = mAnchor.mListNodePrev;
		n.mListNodePrev->mListNodeNext = &n;
		mAnchor.mListNodePrev = &n;
	}

	void pop_front() {
		mAnchor.mListNodeNext = mAnchor.mListNodeNext->mListNodeNext;
		mAnchor.mListNodeNext->mListNodePrev = &mAnchor;
	}

	void pop_back() {
		mAnchor.mListNodePrev = mAnchor.mListNodePrev->mListNodePrev;
		mAnchor.mListNodePrev->mListNodeNext = &mAnchor;
	}

	iterator erase(T *p) {
		return erase(fast_find(p));
	}

	iterator erase(iterator it) {
		node& n = *it.mp;

		n.mListNodePrev->mListNodeNext = n.mListNodeNext;
		n.mListNodeNext->mListNodePrev = n.mListNodePrev;

		it.mp = n.mListNodeNext;
		return it;
	}

	void insert(iterator dst, T *src) {
		node& ns = *src;
		node& nd = *dst.mp;

		ns.mListNodeNext = &nd;
		ns.mListNodePrev = nd.mListNodePrev;
		nd.mListNodePrev->mListNodeNext = &ns;
		nd.mListNodePrev = &ns;
	}

	void splice(iterator dst, vdlist<T>& srclist, iterator src) {
		T *v = *src;
		srclist.erase(src);
		insert(dst, v);
	}

protected:
	node	mAnchor;
};


#endif
