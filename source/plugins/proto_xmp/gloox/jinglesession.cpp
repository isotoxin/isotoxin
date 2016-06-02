/*
  Copyright (c) 2007-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "jinglesession.h"

#include "clientbase.h"
#include "error.h"
#include "jinglecontent.h"
#include "jinglesessionhandler.h"
#include "tag.h"
#include "util.h"

namespace gloox
{

  namespace Jingle
  {

    static const char* actionValues [] = {
      "content-accept",
      "content-add",
      "content-modify",
      "content-reject",
      "content-remove",
      "description-info",
      "security-info",
      "session-accept",
      "session-info",
      "session-initiate",
      "session-terminate",
      "transport-accept",
      "transport-info",
      "transport-reject",
      "transport-replace"
    };

    static inline Action actionType( const std::string& type )
    {
      return (Action)util::lookup( type, actionValues );
    }

    // ---- Session::Reason ----
    static const char* reasonValues [] = {
      "alternative-session",
      "busy",
      "cancel",
      "connectivity-error",
      "decline",
      "expired",
      "failed-application",
      "failed-transport",
      "general-error",
      "gone",
      "incompatible-parameters",
      "media-error",
      "security-error",
      "success",
      "timeout",
      "unsupported-applications",
      "unsupported-transports"
    };

    static inline Session::Reason::Reasons reasonType( const std::string& type )
    {
      return (Session::Reason::Reasons)util::lookup( type, reasonValues );
    }

    Session::Reason::Reason( Reasons reason,
                             const std::string& sid,
                             const std::string& text)
      : Plugin( PluginReason ), m_reason( reason ), m_sid( sid ), m_text( text )
    {
    }

    Session::Reason::Reason( const Tag* tag )
      : Plugin( PluginReason )
    {
      if( !tag || tag->name() != "reason" )
        return;

      const TagList& l = tag->children();
      TagList::const_iterator it = l.begin();
      for( ; it != l.end(); ++it )
      {
        if( (*it)->name() == "text" )
          m_text = (*it)->cdata();
        else if( (*it)->xmlns() == XMLNS_JINGLE )
          m_reason = reasonType( (*it)->name() );
      }
    }

    Session::Reason::~Reason()
    {
    }

    const std::string& Session::Reason::filterString() const
    {
      static const std::string filter = "jingle/reason";
      return filter;
    }

    Tag* Session::Reason::tag() const
    {
      if( m_reason == InvalidReason )
        return 0;

      Tag* t = new Tag( "reason" );
      Tag* r = new Tag( t, util::lookup( m_reason, reasonValues ) );
      if( m_reason == AlternativeSession && !m_sid.empty() )
        new Tag( r, "sid", m_sid );

      if( !m_text.empty() )
        new Tag( t, "text", m_text );

      return t;
    }

    Plugin* Session::Reason::clone() const
    {
      return new Reason( *this );
    }
    // ---- ~Session::Reason ----

    // ---- Session::Jingle ----
    Session::Jingle::Jingle( Action action, const JID& initiator, const JID& responder,
                             const PluginList& plugins, const std::string& sid )
      : StanzaExtension( ExtJingle ), m_action( action ), m_sid( sid ),
        m_initiator( initiator ), m_responder( responder ), m_plugins( plugins ), m_tag( 0 )
    {
    }

#ifdef JINGLE_TEST
    Session::Jingle::Jingle( Action action, const JID& initiator, const JID& responder,
                             const Plugin* plugin, const std::string& sid )
      : StanzaExtension( ExtJingle ), m_action( action ), m_sid( sid ),
        m_initiator( initiator ), m_responder( responder ), m_tag( 0 )
    {
      if( plugin )
        m_plugins.push_back( plugin );
    }
#endif

    Session::Jingle::Jingle( const Tag* tag )
      : StanzaExtension( ExtJingle ), m_action( InvalidAction ), m_tag( 0 )
    {
      if( !tag || tag->name() != "jingle" )
        return;

      m_action = actionType( tag->findAttribute( "action" ) );
      m_initiator.setJID( tag->findAttribute( "initiator" ) );
      m_responder.setJID( tag->findAttribute( "responder" ) );
      m_sid = tag->findAttribute( "sid" );

      m_tag = tag->clone();
    }

//     Session::Jingle::Jingle( const Jingle& right )
//       : StanzaExtension( ExtJingle ), m_action( right.m_action ),
//         m_sid( right.m_sid ), m_initiator( right.m_initiator ),
//         m_responder( right.m_responder )
//     {
//       PluginList::const_iterator it = right.m_plugins.begin();
//       for( ; it != right.m_plugins.end(); ++it )
//         m_plugins.push_back( (*it)->clone() );
//     }

    Session::Jingle::~Jingle()
    {
      util::clearList( m_plugins );
    }

    const std::string& Session::Jingle::filterString() const
    {
      static const std::string filter = "/iq/jingle[@xmlns='" + XMLNS_JINGLE + "']";
      return filter;
    }

    Tag* Session::Jingle::tag() const
    {
      if( m_action == InvalidAction || m_sid.empty() )
        return 0;

      Tag* t = new Tag( "jingle" );
      t->setXmlns( XMLNS_JINGLE );
      t->addAttribute( "action", util::lookup( m_action, actionValues ) );

      if( m_initiator && m_action == SessionInitiate )
        t->addAttribute( "initiator", m_initiator.full() );

      if( m_responder && m_action == SessionAccept )
        t->addAttribute( "responder", m_responder.full() );

      t->addAttribute( "sid", m_sid );

      PluginList::const_iterator it = m_plugins.begin();
      for( ; it != m_plugins.end(); ++it )
        t->addChild( (*it)->tag() );

      return t;
    }

    StanzaExtension* Session::Jingle::clone() const
    {
      return new Jingle( *this );
    }
    // ---- ~Session::Jingle ----

    // ---- Session ----
    Session::Session( ClientBase* parent, const JID& callee, SessionHandler* jsh )
      : m_parent( parent ), m_state( Ended ), m_remote( callee ),
        m_handler( jsh ), m_valid( false )
    {
      if( !m_parent || !m_handler || !m_remote )
        return;

      m_initiator = m_parent->jid();
      m_sid = m_parent->getID();

      m_valid = true;
    }

    Session::Session( ClientBase* parent, const JID& callee, const Session::Jingle* jingle, SessionHandler* jsh )
      : m_parent( parent ), m_state( Ended ), m_handler( jsh ), m_valid( false )
    {
      if( !m_parent || !m_handler || !callee /*|| jingle->action() != SessionInitiate*/ )
        return;

      m_remote = callee;
      m_sid = jingle->sid();

      m_valid = true;
    }

    Session::~Session()
    {
      if( m_parent )
        m_parent->removeIDHandler( this );
    }

    bool Session::contentAccept( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentAccept, content );
    }

    bool Session::contentAdd( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentAdd, content );
    }

    bool Session::contentAdd( const PluginList& contents )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentAdd, contents );
    }

    bool Session::contentModify( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentModify, content );
    }

    bool Session::contentReject( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentReject, content );
    }

    bool Session::contentRemove( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( ContentRemove, content );
    }

    bool Session::descriptionInfo( const Plugin* info )
    {
      if( m_state < Pending )
        return false;

      return doAction( DescriptionInfo, info );
    }

    bool Session::securityInfo( const Plugin* info )
    {
      if( m_state < Pending )
        return false;

      return doAction( SecurityInfo, info );
    }

    bool Session::sessionAccept( const Content* content )
    {
      if( !content || m_state > Pending )
        return false;

      m_state = Active;
      return doAction( SessionAccept, content );
    }

    bool Session::sessionAccept( const PluginList& plugins )
    {
      if( plugins.empty() || m_state != Pending )
        return false;

      m_state = Active;
      return doAction( SessionAccept, plugins );
    }

    bool Session::sessionInfo( const Plugin* info )
    {
      if( m_state < Pending )
        return false;

      return doAction( SessionInfo, info );
    }

    bool Session::sessionInitiate( const Content* content )
    {
      if( !content || !m_initiator || m_state >= Pending )
        return false;

      m_state = Pending;
      return doAction( SessionInitiate, content );
    }

    bool Session::sessionInitiate( const PluginList& plugins )
    {
      if( plugins.empty() || !m_initiator || m_state >= Pending )
        return false;

      m_state = Pending;
      return doAction( SessionInitiate, plugins );
    }

    bool Session::sessionTerminate( Session::Reason* reason )
    {
      if( m_state < Pending /*|| !m_initiator*/ )
        return false;

      m_state = Ended;

      return doAction( SessionTerminate, reason );
    }

    bool Session::transportAccept( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( TransportAccept, content );
    }

    bool Session::transportInfo( const Plugin* info )
    {
      if( m_state < Pending )
        return false;

      return doAction( TransportInfo, info );
    }

    bool Session::transportReject( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( TransportReject, content );
    }

    bool Session::transportReplace( const Content* content )
    {
      if( m_state < Pending )
        return false;

      return doAction( TransportReplace, content );
    }

    bool Session::doAction( Action action, const Plugin* plugin )
    {
      PluginList pl;
      pl.push_back( plugin );
      return doAction( action, pl );
    }

    bool Session::doAction( Action action, const PluginList& plugins )
    {
      if( !m_valid || !m_parent )
        return false;

      IQ init( IQ::Set, m_remote, m_parent->getID() );
      init.addExtension( new Jingle( action, m_initiator, m_responder, plugins, m_sid ) );
      m_parent->send( init, this, action );

      return true;
    }

    bool Session::handleIq( const IQ& iq )
    {
      const Jingle* j = iq.findExtension<Jingle>( ExtJingle );
      if( !j || j->sid() != m_sid || !m_handler || !m_parent )
        return false;

      switch( j->action() )
      {
        case SessionAccept:
          m_state = Active;
          m_responder = j->responder();
          break;
        case SessionInitiate:
          m_state = Pending;
          m_initiator = j->initiator();
          if( !m_responder )
            m_responder = m_parent->jid();
          break;
        case SessionTerminate:
          m_state = Ended;
          break;
        default:
          break;
      }

      IQ re( IQ::Result, iq.from(), iq.id() );
      m_parent->send( re );

      m_handler->handleSessionAction( j->action(), this, j );

      return true;
    }

    void Session::handleIqID( const IQ& iq, int context )
    {
      if( iq.subtype() == IQ::Error )
      {

        const Error* e = iq.findExtension<Error>( ExtError );
        m_handler->handleSessionActionError( (Action)context, this, e );

        switch( context )
        {
          case ContentAccept:
            break;
          case ContentAdd:
            break;
          case ContentModify:
            break;
          case ContentReject:
            break;
          case ContentRemove:
            break;
          case DescriptionInfo:
            break;
          case SessionAccept:
            break;
          case SessionInfo:
            break;
          case SessionInitiate:
            m_state = Ended;
            break;
          case SessionTerminate:
            break;
          case TransportAccept:
            break;
          case TransportInfo:
            break;
          case TransportReject:
            break;
          case TransportReplace:
            break;
          case InvalidAction:
            break;
          default:
            break;
        }
      }
    }

  }

}
