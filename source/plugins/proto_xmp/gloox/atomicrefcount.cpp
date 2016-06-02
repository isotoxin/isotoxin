/*
  Copyright (c) 2007-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "atomicrefcount.h"

#include "config.h"

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
# include <windows.h>
#elif defined( __APPLE__ )
# include <libkern/OSAtomic.h>
#elif defined( HAVE_GCC_ATOMIC_BUILTINS )
 // Use intrinsic functions - no #include required.
#else
# include "mutexguard.h"
#endif

#ifdef _WIN32_WCE
# include <winbase.h>
#endif

namespace gloox
{

  namespace util
  {
    AtomicRefCount::AtomicRefCount()
      : m_count( 0 )
    {
    }

    int AtomicRefCount::increment()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      return (int) ::InterlockedIncrement( (volatile LONG*)&m_count );
#elif defined( __APPLE__ )
      return (int) OSAtomicIncrement32Barrier( (volatile int32_t*)&m_count );
#elif defined( HAVE_GCC_ATOMIC_BUILTINS )
      // Use the gcc intrinsic for atomic increment if supported.
      return (int) __sync_add_and_fetch( &m_count, 1 );
#else
      // Fallback to using a lock
      MutexGuard m( m_lock );
      return ++m_count;
#endif
    }

    int AtomicRefCount::decrement()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      return (int) ::InterlockedDecrement( (volatile LONG*)&m_count );
#elif defined( __APPLE__ )
      return (int) OSAtomicDecrement32Barrier( (volatile int32_t*)&m_count );
#elif defined( HAVE_GCC_ATOMIC_BUILTINS )
      // Use the gcc intrinsic for atomic decrement if supported.
      return (int) __sync_sub_and_fetch( &m_count, 1 );
#else
      // Fallback to using a lock
      MutexGuard m( m_lock );
      return --m_count;
#endif
    }

    void AtomicRefCount::reset()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      ::InterlockedExchange( (volatile LONG*)&m_count, (volatile LONG)0 );
#elif defined( __APPLE__ )
      OSAtomicAnd32Barrier( (uint32_t)0, (volatile uint32_t*)&m_count );
#elif defined( HAVE_GCC_ATOMIC_BUILTINS )
      // Use the gcc intrinsic for atomic decrement if supported.
      __sync_fetch_and_and( &m_count, 0 );
#else
      // Fallback to using a lock
      MutexGuard m( m_lock );
      m_count = 0;
#endif
    }

  }

}

