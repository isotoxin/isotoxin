/*
 *  Copyright (c) 2013-2015 by Jakob Schröter <js@camaya.net>
 *  This file is part of the gloox library. http://camaya.net/gloox
 *
 *  This software is distributed under a license. The full license
 *  agreement can be found in the file LICENSE in this distribution.
 *  This software may not be copied, modified, sold or distributed
 *  other than expressed in the named license agreement.
 *
 *  This software is distributed without any warranty.
 */


#include "carbons.h"

#include "forward.h"
#include "util.h"

namespace gloox
{
  /* chat state type values */
  static const char* typeValues [] = {
    "received",
    "sent",
    "enable",
    "disable",
    "private"
  };

  Carbons::Carbons( Carbons::Type type )
    : StanzaExtension( ExtCarbons ), m_forward( 0 ), m_type( type )
  {
  }

  Carbons::Carbons( const Tag* tag )
    : StanzaExtension( ExtCarbons ), m_forward( 0 ), m_type( Invalid )
  {
    if( !tag )
      return;

    const std::string& name = tag->name();
    m_type = (Type)util::lookup( name, typeValues );

    switch( m_type )
    {
      case Sent:
      case Received:
      {
        Tag* f = tag->findChild( "forwarded", XMLNS, XMLNS_STANZA_FORWARDING );
        if( f )
          m_forward = new Forward( f );
        break;
      }
      default:
        break;
    }
  }

  Carbons::~Carbons()
  {
    delete m_forward;
  }

  const std::string& Carbons::filterString() const
  {
    static const std::string filter = "/message/*[@xmlns='" + XMLNS_MESSAGE_CARBONS + "']";
    return filter;
  }

  Stanza* Carbons::embeddedStanza() const
  {
    if( !m_forward || m_type == Invalid )
      return 0;

    return m_forward->embeddedStanza();
  }

  Tag* Carbons::embeddedTag() const
  {
    if( !m_forward || m_type == Invalid )
      return 0;

    return m_forward->embeddedTag();
  }

  Tag* Carbons::tag() const
  {
    if( m_type == Invalid )
      return 0;

    Tag* t = new Tag( util::lookup( m_type, typeValues ), XMLNS, XMLNS_MESSAGE_CARBONS );
    if( m_forward && ( m_type == Received || m_type == Sent ) )
      t->addChild( m_forward->tag() );

    return t;
  }

  StanzaExtension* Carbons::clone() const
  {
    return 0; // TODO
  }

}
