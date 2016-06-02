/*
  Copyright (c) 2013-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "forward.h"

#include "delayeddelivery.h"
#include "message.h"
#include "stanza.h"
#include "util.h"

namespace gloox
{

  Forward::Forward( Stanza *stanza, DelayedDelivery *delay )
    : StanzaExtension( ExtForward ),
      m_stanza( stanza ), m_tag( 0 ), m_delay( delay )
  {
  }

  Forward::Forward( const Tag* tag )
    : StanzaExtension( ExtForward ),
      m_stanza( 0 ), m_tag( 0 ), m_delay( 0 )
  {
    if( !tag || !( tag->name() == "forwarded" && tag->hasAttribute( XMLNS, XMLNS_STANZA_FORWARDING ) ) )
      return;

    m_delay = new DelayedDelivery( tag->findChild( "delay", XMLNS, XMLNS_DELAY ) );

    Tag* m = tag->findChild( "message" );
    if( !m )
      return;

    m_tag = m->clone();
    m_stanza = new Message( m );
  }

  Forward::~Forward()
  {
    delete m_delay;
    delete m_stanza;
    delete m_tag;
  }

  const std::string& Forward::filterString() const
  {
    static const std::string filter = "/message/forwarded[@xmlns='" + XMLNS_STANZA_FORWARDING + "']"
                                      "|/iq/forwarded[@xmlns='" + XMLNS_STANZA_FORWARDING + "']"
                                      "|/presence/forwarded[@xmlns='" + XMLNS_STANZA_FORWARDING + "']";
    return filter;
  }

  Tag* Forward::tag() const
  {
    if( !m_stanza )
      return 0;

    Tag* f = new Tag( "forwarded" );
    f->setXmlns( XMLNS_STANZA_FORWARDING );
    if( m_delay )
      f->addChild( m_delay->tag() );
    if( m_stanza )
      f->addChild( m_stanza->tag() );

    return f;
  }

  StanzaExtension* Forward::clone() const
  {
    if( !m_tag || !m_delay )
      return 0;

    return new Forward( new Message( m_tag ), static_cast<DelayedDelivery*>( m_delay->clone() ) );
  }

}
