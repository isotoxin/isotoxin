/*
    spinlock module
    (C) 2010-2015 BMV, ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

#include "spinlock.h"

#pragma warning(push)
#pragma warning(disable:4324) // : structure was padded due to __declspec(align())

namespace spinlock
{
template<typename T> class
#ifdef _WINDOWS
__declspec(align(16))
#endif
spinlock_list_s
{
	struct pointer_t
	{
		T* ptr;
		long3264 count;
		// default to a null pointer with a count of zero
		pointer_t() : ptr(nullptr),count(0){}
		pointer_t(T* element, const long3264 c ) : ptr(element),count(c){}
		pointer_t(const volatile pointer_t& p) : ptr(p.ptr),count(p.count){}
	};
	volatile pointer_t first;

    __forceinline void add(T* element)
    {
        if (element)
        {
            pointer_t old_val(first);
            for (;;)
            {
                element->StackNextFree = old_val.ptr;
                if (CAS2(first, old_val, pointer_t(element, old_val.count + 1))) break; // success - exit loop
            }
        }
    }

    T* get()
    {
        __try{
            pointer_t old_val(first);
            while (old_val.ptr)
            {
                T* next = old_val.ptr->StackNextFree;
                if (CAS2(first, old_val, pointer_t(next, old_val.count + 1)))
                {
                    return(old_val.ptr); // success - exit loop
                }
            }
        }

#ifdef _WINDOWS
        __except (EXCEPTION_EXECUTE_HANDLER){
            SLERROR("spinlock list get crush");
        }
#else
        catch (...){
            SLERROR("spinlock list get crush");
        }
#endif
        return(nullptr);
    }

public:
    spinlock_list_s() {}
	void clear(){ first.ptr=nullptr; }
    inline void push(T* element) { add(element); }
    inline T* pop() { return(get()); }
}
#ifdef _LINUX
__attribute__ (( aligned (16)))
#endif
;


} // namespace spinlock

#pragma warning(pop)
