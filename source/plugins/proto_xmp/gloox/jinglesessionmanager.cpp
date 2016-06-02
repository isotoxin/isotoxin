/*
  Copyright (c) 2013-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "jinglesessionmanager.h"

#include "clientbase.h"
#include "jinglesession.h"
#include "jinglesessionhandler.h"
#include "disco.h"
#include "util.h"

namespace gloox
{

  namespace Jingle
  {

    SessionManager::SessionManager( ClientBase* parent, SessionHandler* sh )
      : m_parent( parent ), m_handler( sh )
    {
      if( m_parent )
      {
        m_parent->registerStanzaExtension( new Session::Jingle() );
        m_parent->registerIqHandler( this, ExtJingle );
        m_parent->disco()->addFeature( XMLNS_JINGLE );
      }
    }

    SessionManager::~SessionManager()
    {
      util::clearList( m_sessions );
    }

    void SessionManager::registerPlugin( Plugin* plugin )
    {
      if( !plugin )
        return;

      m_factory.registerPlugin( plugin );
      if( m_parent )
      {
        StringList features = plugin->features();
        StringList::const_iterator it = features.begin();
        for( ; it != features.end(); ++it )
          m_parent->disco()->addFeature( (*it) );
      }
    }

    Session* SessionManager::createSession( const JID& callee, SessionHandler* handler )
    {
      if( !( handler || m_handler ) || !callee )
        return 0;

      Session* sess = new Session( m_parent, callee, handler ? handler : m_handler );
      m_sessions.push_back( sess );
      return sess;
    }

    void SessionManager::discardSession( Session* session )
    {
      if( !session )
        return;

      m_sessions.remove( session );
      delete session;
    }

    bool SessionManager::handleIq( const IQ& iq )
    {
      const Session::Jingle* j = iq.findExtension<Session::Jingle>( ExtJingle );
      if( !j )
        return false;

      m_factory.addPlugins( const_cast<Session::Jingle&>( *j ), j->embeddedTag() );

      SessionList::iterator it = m_sessions.begin();
      for( ; it != m_sessions.end() && (*it)->sid() != j->sid(); ++it ) ;
      if( it == m_sessions.end() )
      {
        Session* s = new Session( m_parent, iq.from(), j, m_handler );
        m_sessions.push_back( s );
        m_handler->handleIncomingSession( s );
        s->handleIq( iq );
      }
      else
      {
        (*it)->handleIq( iq );
      }
      return true;
    }

  }

}
