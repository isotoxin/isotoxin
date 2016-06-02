/*
  Copyright (c) 2008-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "jinglecontent.h"
#include "jinglepluginfactory.h"
#include "util.h"

namespace gloox
{

  namespace Jingle
  {

    static const char* creatorValues [] = {
      "initiator",
      "responder"
    };

    static inline Content::Creator creatorType( const std::string& type )
    {
      return (Content::Creator)util::lookup( type, creatorValues );
    }

    static const char* sendersValues [] = {
      "initiator",
      "responder",
      "both",
      "none"
    };

    static inline Content::Senders sendersType( const std::string& type )
    {
      return (Content::Senders)util::lookup( type, sendersValues );
    }

    Content::Content( const std::string& name, const PluginList& plugins, Creator creator,
                      Senders senders, const std::string& disposition )
      : Plugin( PluginContent ), m_creator( creator ), m_disposition( disposition ),
        m_name( name ), m_senders( senders )
    {
      m_plugins = plugins;
    }

    Content::Content( const Tag* tag, PluginFactory* factory )
     : Plugin( PluginContent )
    {
      if( !tag || tag->name() != "content" )
        return;

      m_name = tag->findAttribute( "name" );
      m_creator = (Creator)util::lookup( tag->findAttribute( "creator" ), creatorValues );
      m_senders = (Senders)util::lookup( tag->findAttribute( "senders" ), sendersValues );
      m_disposition = tag->findAttribute( "disposition" );

      if( factory )
        factory->addPlugins( *this, tag );
    }

    Content::~Content()
    {
    }

    const std::string& Content::filterString() const
    {
      static const std::string filter = "jingle/content";
      return filter;
    }

    Tag* Content::tag() const
    {
      if( m_creator == InvalidCreator || m_name.empty() )
        return 0;

      Tag* t = new Tag( "content" );
      t->addAttribute( "creator", util::lookup( m_creator, creatorValues ) );
      t->addAttribute( "disposition", m_disposition );
      t->addAttribute( "name", m_name );
      t->addAttribute( "senders", util::lookup( m_senders, sendersValues ) );

      PluginList::const_iterator it = m_plugins.begin();
      for( ; it != m_plugins.end(); ++it )
        t->addChild( (*it)->tag() );

      return t;
    }

    Plugin* Content::clone() const
    {
      return 0;
    }

  }

}
