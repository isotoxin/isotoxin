/*
    spinlock module
    (C) 2010-2015 BMV, ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

#include "list.h"

namespace spinlock
{

struct defallocator
{
    static void *ma( size_t sz ) { return malloc(sz); }
    static void mf( void *ptr ) { free(ptr); }
};

// T must be a pointer

template< typename T, typename A = defallocator > class
#ifdef _WIN32
__declspec(align(16))
#endif
spinlock_queue_s
{
	struct node_t;

	struct pointer_t 
	{
		node_t* ptr;
		long3264 count;
		pointer_t() : ptr(nullptr),count(0){};
		pointer_t(node_t* node, const long3264 c ) : ptr(node),count(c){}
		pointer_t(const volatile pointer_t& p) : ptr(p.ptr),count(p.count){}
        pointer_t& operator=(const volatile pointer_t& p){ ptr=p.ptr; count=p.count; return(*this); }
	};

	struct node_t 
	{
		pointer_t next;
		node_t* StackNextFree;
		T value;
		node_t(){} //-V730
	};

	volatile pointer_t Head;
	volatile pointer_t Tail;
	spinlock_list_s<node_t> freenodes;
	node_t* get_node()
	{
		node_t* result=freenodes.pop();
		if (!result) result=(node_t*)A::ma(sizeof(node_t));
		if (result)
		{
			result->next.count=0;
			result->next.ptr=nullptr;
		}
		return(result);
 	}
	void free_node(node_t* node)
	{
		freenodes.push(node);
	}

public:	
	spinlock_queue_s()
	{
		node_t* n = get_node();
		n->value=nullptr;
		Head.ptr = Tail.ptr = n;
	}
	~spinlock_queue_s()
	{
		// remove the dummy head
		SLASSERT(Head.ptr==Tail.ptr);
        free_node(Head.ptr);

        while (node_t* node=freenodes.pop())
            A::mf(node);
	}

	// insert items of class T in the back of the queue
	// items of class T must implement a default and copy constructor
	// Enqueue method
	__forceinline bool enqueue(const T& t)
	{
		// Allocate a new node from the free list
		node_t* n = get_node(); 

		if (!n) return false;

		// Copy enqueued value into node
		n->value = t;

		for(;;)
		{
			// Read Tail.ptr and Tail.count together
			pointer_t tail(Tail);

			bool nNullTail = (nullptr==tail.ptr); 
			// Read next ptr and count fields together
			pointer_t next( // ptr 
				(nNullTail)? nullptr : tail.ptr->next.ptr,
				// count
				(nNullTail)? 0 : tail.ptr->next.count
				) ;

			// Are tail and next consistent?
			if(tail.count == Tail.count && tail.ptr == Tail.ptr)
			{
				if(nullptr == next.ptr) // Was Tail pointing to the last node?
				{
					// Try to link node at the end of the linked list										
					if(CAS2( tail.ptr->next, next, pointer_t(n,next.count+1) ) )
					{
						return true;
					} // endif

				} // endif

				else // Tail was not pointing to the last node
				{
					// Try to swing Tail to the next node
					CAS2(Tail, tail, pointer_t(next.ptr,tail.count+1) );
				}

			} // endif

		} // endloop
	}

	__forceinline bool push(const T& t){ return(enqueue(t)); };

	// remove items of class T from the front of the queue
	// items of class T must implement a default and copy constructor
	// Dequeue method
	__forceinline bool dequeue(T &t)
	{
		pointer_t head;
		// Keep trying until Dequeue is done
		for(;;)
		{
			// Read Head
			head = Head;
			// Read Tail
			pointer_t tail(Tail);

			if(head.ptr == nullptr)
			{
				// queue is empty
				return false;
			}

			// Read Head.ptr->next
			pointer_t next(head.ptr->next);

			// Are head, tail, and next consistent
			if(head.count == Head.count && head.ptr == Head.ptr)
			{
				if(head.ptr == tail.ptr) // is tail falling behind?
				{
					// Is the Queue empty
					if(nullptr == next.ptr)
					{
						// queue is empty cannot deque
						return false;
					}
					CAS2(Tail, tail, pointer_t(next.ptr,tail.count+1)); // Tail is falling behind. Try to advance it
				} // endif

				else // no need to deal with tail
				{
                    if(nullptr != next.ptr)
                    {
                        // read value before CAS otherwise another deque might try to free the next node
                        t = next.ptr->value;

                        // try to swing Head to the next node
                        if(CAS2(Head, head, pointer_t(next.ptr,head.count+1) ) )
                        {
                            // It is now safe to free the old dummy node
                            free_node(head.ptr);
                            // queue was not empty, deque succeeded
                            return true;
                        }
                    }
				}

			} // endif

		} // endloop		
	}
	__forceinline bool try_pop(T &t){ return(dequeue(t)); };
}
#ifdef _LINUX
__attribute__((aligned(16)))
#endif
;

} // namespace spinlock