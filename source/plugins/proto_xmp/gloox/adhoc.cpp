/*
  Copyright (c) 2004-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "adhoc.h"
#include "adhochandler.h"
#include "adhoccommandprovider.h"
#include "disco.h"
#include "dataform.h"
#include "error.h"
#include "iodata.h"
#include "discohandler.h"
#include "clientbase.h"
#include "adhocplugin.h"
#include "util.h"
#include "mutexguard.h"

namespace gloox
{

  static const char* cmdActionStringValues[] =
  {
    "execute", "cancel", "prev", "next", "complete"
  };

  static inline const std::string actionString( Adhoc::Command::Action action )
  {
    return util::lookup2( action, cmdActionStringValues );
  }

  static const char* cmdStatusStringValues[] =
  {
    "executing", "completed", "canceled"
  };

  static inline const std::string statusString( Adhoc::Command::Status status )
  {
    return util::lookup( status, cmdStatusStringValues );
  }

  static const char* cmdNoteStringValues[] =
  {
    "info", "warn", "error"
  };

  static inline const std::string noteString( Adhoc::Command::Note::Severity sev )
  {
    return util::lookup( sev, cmdNoteStringValues );
  }

  // ---- Adhoc::Command::Note ----
  Adhoc::Command::Note::Note( const Tag* tag )
    : m_severity( InvalidSeverity )
  {
    if( !tag || tag->name() != "note" )
      return;

    m_severity = (Severity)util::deflookup( tag->findAttribute( "type" ), cmdNoteStringValues, Info );
    m_note = tag->cdata();
  }

  Tag* Adhoc::Command::Note::tag() const
  {
    if( m_note.empty() || m_severity == InvalidSeverity )
      return 0;

    Tag* n = new Tag( "note", m_note );
    n->addAttribute( TYPE, noteString( m_severity ) );
    return n;
  }
  // ---- ~Adhoc::Command::Note ----

  // ---- Adhoc::Command ----
  Adhoc::Command::Command( const std::string& node, Adhoc::Command::Action action,
                           AdhocPlugin* plugin )
    : StanzaExtension( ExtAdhocCommand ), m_node( node ), m_plugin( plugin ), m_action( action ),
      m_status( InvalidStatus ), m_actions( 0 )
  {
  }

  Adhoc::Command::Command( const std::string& node, const std::string& sessionid, Status status,
                           AdhocPlugin* plugin )
  : StanzaExtension( ExtAdhocCommand ), m_node( node ), m_sessionid( sessionid ),
    m_plugin( plugin ), m_action( InvalidAction ), m_status( status ), m_actions( 0 )
  {
  }

  Adhoc::Command::Command( const std::string& node, const std::string& sessionid,
                           Adhoc::Command::Action action,
                           AdhocPlugin* plugin )
    : StanzaExtension( ExtAdhocCommand ), m_node( node ), m_sessionid( sessionid ),
      m_plugin( plugin ), m_action( action ), m_actions( 0 )
  {
  }

  Adhoc::Command::Command( const std::string& node, const std::string& sessionid, Status status,
                           Action executeAction, int allowedActions,
                           AdhocPlugin* plugin )
    : StanzaExtension( ExtAdhocCommand ), m_node( node ), m_sessionid( sessionid ),
      m_plugin( plugin ), m_action( executeAction ), m_status( status ), m_actions( allowedActions )
  {
  }

  Adhoc::Command::Command( const Tag* tag )
    : StanzaExtension( ExtAdhocCommand ), m_plugin( 0 ), m_actions( 0 )
  {
    if( !tag || tag->name() != "command" || tag->xmlns() != XMLNS_ADHOC_COMMANDS )
      return;

    m_node = tag->findAttribute( "node" );
    m_sessionid = tag->findAttribute( "sessionid" );
    m_status = (Status)util::lookup( tag->findAttribute( "status" ), cmdStatusStringValues );

    Tag* a = tag->findChild( "actions" );
    if( a )
    {
      // Multi-stage response
      m_action = (Action)util::deflookup2( a->findAttribute( "action" ), cmdActionStringValues, Complete );
      if( a->hasChild( "prev" ) )
        m_actions |= Previous;
      if( a->hasChild( "next" ) )
        m_actions |= Next;
      if( a->hasChild( "complete" ) )
        m_actions |= Complete;
    }
    else
    {
      m_action = (Action)util::deflookup2( tag->findAttribute( "action" ), cmdActionStringValues, Execute );
    }

    const ConstTagList& l = tag->findTagList( "/command/note" );
    ConstTagList::const_iterator it = l.begin();
    for( ; it != l.end(); ++it )
      m_notes.push_back( new Note( (*it) ) );

    Tag* x = tag->findChild( "x", "xmlns", XMLNS_X_DATA );
    if( x )
      m_plugin = new DataForm( x );
    else
    {
      Tag* x = tag->findChild( "iodata", "xmlns", XMLNS_IODATA );
      if( x )
        m_plugin = new IOData( x );
    }
  }

  Adhoc::Command::~Command()
  {
    util::clearList( m_notes );
    delete m_plugin;
  }

  const std::string& Adhoc::Command::filterString() const
  {
    static const std::string filter = "/iq/command[@xmlns='" + XMLNS_ADHOC_COMMANDS + "']";
    return filter;
  }

  Tag* Adhoc::Command::tag() const
  {
    if( m_node.empty() )
      return 0;

    Tag* c = new Tag( "command" );
    c->setXmlns( XMLNS_ADHOC_COMMANDS );
    c->addAttribute( "node", m_node );
    if( m_actions != 0 )
    {
      // Multi-stage command response

      if( m_status != InvalidStatus )
        c->addAttribute( "status", statusString( m_status ) );
      else
        c->addAttribute( "status", statusString( Executing ) );

      Tag* actions = new Tag( c, "actions" );

      if( m_action != InvalidAction )
        c->addAttribute( "execute", actionString( m_action ) );
      else
        c->addAttribute( "execute", actionString( Complete ) );

      if( ( m_actions & Previous ) == Previous )
        new Tag( actions, "prev" );
      if( ( m_actions & Next ) == Next )
        new Tag( actions, "next" );
      if( ( m_actions & Complete ) == Complete )
        new Tag( actions, "complete" );
    }
    else
    {
      // Single-stage command request/response or Multi-stage command request

      if( m_action != InvalidAction )
        c->addAttribute( "action", actionString( m_action ) );
      if( m_status != InvalidStatus )
        c->addAttribute( "status", statusString( m_status ) );
    }

    if ( !m_sessionid.empty() )
      c->addAttribute( "sessionid", m_sessionid );

    if( m_plugin && *m_plugin )
      c->addChild( m_plugin->tag()->clone() );

    NoteList::const_iterator it = m_notes.begin();
    for( ; it != m_notes.end(); ++it )
      c->addChild( (*it)->tag() );

    return c;
  }
  // ---- ~Adhoc::Command ----

  // ---- Adhoc ----
  Adhoc::Adhoc( ClientBase* parent )
    : m_parent( parent )
  {
    if( !m_parent || !m_parent->disco() )
      return;

    m_parent->disco()->addFeature( XMLNS_ADHOC_COMMANDS );
    m_parent->disco()->registerNodeHandler( this, XMLNS_ADHOC_COMMANDS );
    m_parent->disco()->registerNodeHandler( this, EmptyString );
    m_parent->registerIqHandler( this, ExtAdhocCommand );
    m_parent->registerStanzaExtension( new Adhoc::Command() );
  }

  Adhoc::~Adhoc()
  {
    m_adhocTrackMapMutex.lock();
    m_adhocTrackMap.clear();
    m_adhocTrackMapMutex.unlock();

    if( !m_parent || !m_parent->disco() )
      return;

    m_parent->disco()->removeFeature( XMLNS_ADHOC_COMMANDS );
    m_parent->disco()->removeNodeHandler( this, XMLNS_ADHOC_COMMANDS );
    m_parent->disco()->removeNodeHandler( this, EmptyString );
    m_parent->removeIqHandler( this, ExtAdhocCommand );
    m_parent->removeIDHandler( this );
    m_parent->removeStanzaExtension( ExtAdhocCommand );
  }

  StringList Adhoc::handleDiscoNodeFeatures( const JID& /*from*/, const std::string& /*node*/ )
  {
    StringList features;
    features.push_back( XMLNS_ADHOC_COMMANDS );
    return features;
//    return StringList( 1, XMLNS_ADHOC_COMMANDS );
  }

  Disco::ItemList Adhoc::handleDiscoNodeItems( const JID& from, const JID& /*to*/, const std::string& node )
  {
    Disco::ItemList l;
    if( node.empty() )
    {
      l.push_back( new Disco::Item( m_parent->jid(), XMLNS_ADHOC_COMMANDS, "Ad-Hoc Commands" ) );
    }
    else if( node == XMLNS_ADHOC_COMMANDS )
    {
      StringMap::const_iterator it = m_items.begin();
      for( ; it != m_items.end(); ++it )
      {
        AdhocCommandProviderMap::const_iterator itp = m_adhocCommandProviders.find( (*it).first );
        if( itp != m_adhocCommandProviders.end()
            && (*itp).second
            && (*itp).second->handleAdhocAccessRequest( from, (*it).first ) )
        {
          l.push_back( new Disco::Item( m_parent->jid(), (*it).first, (*it).second ) );
        }
      }
    }
    return l;
  }

  Disco::IdentityList Adhoc::handleDiscoNodeIdentities( const JID& /*from*/, const std::string& node )
  {
    Disco::IdentityList l;
    StringMap::const_iterator it = m_items.find( node );
    l.push_back( new Disco::Identity( "automation",
                               node == XMLNS_ADHOC_COMMANDS ? "command-list" : "command-node",
                               it == m_items.end() ? "Ad-Hoc Commands" : (*it).second ) );
    return l;
  }

  bool Adhoc::handleIq( const IQ& iq )
  {
    if( iq.subtype() != IQ::Set )
      return false;

    const Adhoc::Command* ac = iq.findExtension<Adhoc::Command>( ExtAdhocCommand );
    if( !ac || ac->node().empty())
      return false;

    AdhocCommandProviderMap::const_iterator it = m_adhocCommandProviders.find( ac->node() );
    if( it != m_adhocCommandProviders.end() )
    {
      const std::string& sess = ac->sessionID().empty() ? m_parent->getID() : ac->sessionID();
      m_activeSessions[sess] = iq.id();
      (*it).second->handleAdhocCommand( iq.from(), *ac, sess );
      return true;
    }

    return false;
  }

  void Adhoc::handleIqID( const IQ& iq, int context )
  {
    if( context != ExecuteAdhocCommand )
      return;

    m_adhocTrackMapMutex.lock();
    AdhocTrackMap::iterator it = m_adhocTrackMap.find( iq.id() );
    bool haveIdHandler = ( it != m_adhocTrackMap.end() );
    m_adhocTrackMapMutex.unlock();
    if( !haveIdHandler || (*it).second.context != context
        || (*it).second.remote != iq.from() )
      return;

    switch( iq.subtype() )
    {
      case IQ::Error:
        (*it).second.ah->handleAdhocError( iq.from(), iq.error(), (*it).second.handlerContext );
        break;
      case IQ::Result:
      {
        const Adhoc::Command* ac = iq.findExtension<Adhoc::Command>( ExtAdhocCommand );
        if( ac )
          (*it).second.ah->handleAdhocExecutionResult( iq.from(), *ac, (*it).second.handlerContext );
        break;
      }
      default:
        break;
    }
    m_adhocTrackMapMutex.lock();
    m_adhocTrackMap.erase( it );
    m_adhocTrackMapMutex.unlock();
  }

  void Adhoc::registerAdhocCommandProvider( AdhocCommandProvider* acp, const std::string& command,
                                            const std::string& name )
  {
    if( !m_parent || !m_parent->disco() )
      return;

    m_parent->disco()->registerNodeHandler( this, command );
    m_adhocCommandProviders[command] = acp;
    m_items[command] = name;
  }

  void Adhoc::handleDiscoInfo( const JID& from, const Disco::Info& info, int context )
  {
    if( context != CheckAdhocSupport )
      return;

    util::MutexGuard m( m_adhocTrackMapMutex );

    AdhocTrackMap::iterator it = m_adhocTrackMap.begin();
    for( ; it != m_adhocTrackMap.end() && (*it).second.context != context
                                       && (*it).second.remote  != from; ++it )
      ;
    if( it == m_adhocTrackMap.end() )
      return;

    (*it).second.ah->handleAdhocSupport( from, info.hasFeature( XMLNS_ADHOC_COMMANDS ), (*it).second.handlerContext );
    m_adhocTrackMap.erase( it );
  }

  void Adhoc::handleDiscoItems( const JID& from, const Disco::Items& items, int context )
  {
    if( context != FetchAdhocCommands )
      return;

    util::MutexGuard m( m_adhocTrackMapMutex );

    AdhocTrackMap::iterator it = m_adhocTrackMap.begin();
    for( ; it != m_adhocTrackMap.end(); ++it )
    {
      if( (*it).second.context == context && (*it).second.remote == from )
      {
        StringMap commands;
        const Disco::ItemList& l = items.items();
        Disco::ItemList::const_iterator it2 = l.begin();
        for( ; it2 != l.end(); ++it2 )
        {
          commands[(*it2)->node()] = (*it2)->name();
        }
        (*it).second.ah->handleAdhocCommands( from, commands, (*it).second.handlerContext );

        m_adhocTrackMap.erase( it );
        break;
      }
    }
  }

  void Adhoc::handleDiscoError( const JID& from, const Error* error, int context )
  {
    util::MutexGuard m( m_adhocTrackMapMutex );
    for( AdhocTrackMap::iterator it = m_adhocTrackMap.begin(); it != m_adhocTrackMap.end(); )
    {
      if( (*it).second.context == context && (*it).second.remote == from )
      {
        (*it).second.ah->handleAdhocError( from, error, (*it).second.handlerContext );

          // Normally we'd just assign it to the return value of the .erase() call,
          // which is either the next element, or .end().  However,
          // it's only since C++11 that this works; C++03 version returns void.
          // So instead, we do a post-increment. this increments the iterator to point
          // to the next element, then passes a copy of the old iterator (that is to the item to be deleted)
        m_adhocTrackMap.erase( it++ );
      }
      else
      {
        ++it;
      }
    }
  }

  void Adhoc::checkSupport( const JID& remote, AdhocHandler* ah, int context )
  {
    if( !remote || !ah || !m_parent || !m_parent->disco() )
      return;

    TrackStruct track;
    track.remote = remote;
    track.context = CheckAdhocSupport;
    track.ah = ah;
    track.handlerContext = context;
    const std::string& id = m_parent->getID();
    m_adhocTrackMapMutex.lock();
    m_adhocTrackMap[id] = track;
    m_adhocTrackMapMutex.unlock();
    m_parent->disco()->getDiscoInfo( remote, EmptyString, this, CheckAdhocSupport, id );
  }

  void Adhoc::getCommands( const JID& remote, AdhocHandler* ah, int context )
  {
    if( !remote || !ah || !m_parent || !m_parent->disco() )
      return;

    TrackStruct track;
    track.remote = remote;
    track.context = FetchAdhocCommands;
    track.ah = ah;
    track.handlerContext = context;
    const std::string& id = m_parent->getID();
    m_adhocTrackMapMutex.lock();
    m_adhocTrackMap[id] = track;
    m_adhocTrackMapMutex.unlock();
    m_parent->disco()->getDiscoItems( remote, XMLNS_ADHOC_COMMANDS, this, FetchAdhocCommands, id );
  }

  void Adhoc::execute( const JID& remote, const Adhoc::Command* command, AdhocHandler* ah, int context )
  {
    if( !remote || !command || !m_parent || !ah )
      return;

    const std::string& id = m_parent->getID();
    IQ iq( IQ::Set, remote, id );
    iq.addExtension( command );

    TrackStruct track;
    track.remote = remote;
    track.context = ExecuteAdhocCommand;
    track.session = command->sessionID();
    track.ah = ah;
    track.handlerContext = context;
    m_adhocTrackMapMutex.lock();
    m_adhocTrackMap[id] = track;
    m_adhocTrackMapMutex.unlock();

    m_parent->send( iq, this, ExecuteAdhocCommand );
  }

  void Adhoc::respond( const JID& remote, const Adhoc::Command* command, const Error* error )
  {
    if( !remote || !command || !m_parent )
      return;

    StringMap::iterator it = m_activeSessions.find( command->sessionID() );
    if( it == m_activeSessions.end() )
      return;

    IQ re( error ? IQ::Error : IQ::Result, remote, (*it).second );
    re.addExtension( command );
    if( error )
      re.addExtension( error );
    m_parent->send( re );
    m_activeSessions.erase( it );
  }

  void Adhoc::removeAdhocCommandProvider( const std::string& command )
  {
    if( !m_parent || !m_parent->disco() )
      return;

    m_parent->disco()->removeNodeHandler( this, command );
    m_adhocCommandProviders.erase( command );
    m_items.erase( command );
  }

}
