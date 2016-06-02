/*
  Copyright (c) 2007-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "mutex.h"

#include "config.h"

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
# include <windows.h>
#endif

#ifdef _WIN32_WCE
# include <winbase.h>
#endif

#ifdef HAVE_PTHREAD
# include <pthread.h>
#endif

namespace gloox
{

  namespace util
  {

    class Mutex::MutexImpl
    {
      public:
        MutexImpl();
        ~MutexImpl();
        void lock();
        bool trylock();
        void unlock();
      private:
        MutexImpl( const MutexImpl& );
        MutexImpl& operator=( const MutexImpl& );

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
        CRITICAL_SECTION m_cs;
#elif defined( HAVE_PTHREAD )
        pthread_mutex_t m_mutex;
#endif

    };

    Mutex::MutexImpl::MutexImpl()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      // NOTE: Critical sections by nature allow "recursive"
      //  (the same thread can get it again, and just bump the ref count).
      InitializeCriticalSection( &m_cs );
#elif defined( HAVE_PTHREAD )
      // For pthreads, configured the mutex to be recursive
      //  (the same thread can get it again, and just bump the ref count).
      pthread_mutexattr_t mutexAttribute;
      pthread_mutexattr_init( &mutexAttribute );
      pthread_mutexattr_settype( &mutexAttribute, PTHREAD_MUTEX_RECURSIVE );
      pthread_mutex_init( &m_mutex, &mutexAttribute );
      pthread_mutexattr_destroy( &mutexAttribute );
#endif
    }

    Mutex::MutexImpl::~MutexImpl()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      DeleteCriticalSection( &m_cs );
#elif defined( HAVE_PTHREAD )
      pthread_mutex_destroy( &m_mutex );
#endif
    }

    void Mutex::MutexImpl::lock()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      EnterCriticalSection( &m_cs );
#elif defined( HAVE_PTHREAD )
      pthread_mutex_lock( &m_mutex );
#endif
    }

    bool Mutex::MutexImpl::trylock()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      return TryEnterCriticalSection( &m_cs ) ? true : false;
#elif defined( HAVE_PTHREAD )
      return !( pthread_mutex_trylock( &m_mutex ) );
#else
      return true;
#endif
    }

    void Mutex::MutexImpl::unlock()
    {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      LeaveCriticalSection( &m_cs );
#elif defined( HAVE_PTHREAD )
      pthread_mutex_unlock( &m_mutex );
#endif
    }

    Mutex::Mutex()
      : m_mutex( new MutexImpl() )
    {
    }

    Mutex::~Mutex()
    {
      delete m_mutex;
    }

    void Mutex::lock()
    {
      m_mutex->lock();
    }

    bool Mutex::trylock()
    {
      return m_mutex->trylock();
    }

    void Mutex::unlock()
    {
      m_mutex->unlock();
    }

  }

}
