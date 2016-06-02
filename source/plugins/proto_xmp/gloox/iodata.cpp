/*
  Copyright (c) 2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/


#include "iodata.h"

#include "util.h"

namespace gloox
{

  static const char* ioTypes[] = {
    "io-schemata-get",
    "input",
    "getStatus",
    "getOutput",
    "io-schemata-result",
    "output",
    "error",
    "status"
  };

  static inline IOData::Type ioType( const std::string& type )
  {
    return (IOData::Type)util::lookup( type, ioTypes );
  }

  IOData::IOData( Type type )
    : AdhocPlugin( ExtIOData ),
      m_in( 0 ), m_out( 0 ), m_error( 0 ),
      m_type( type )
  {
    m_status.elapsed = -1;
    m_status.remaining = -1;
    m_status.percentage = -1;
  }

  IOData::IOData( const Tag* tag )
    : AdhocPlugin( ExtIOData ),
      m_in( 0 ), m_out( 0 ), m_error( 0 ),
      m_type( TypeInvalid )
  {
    if( !tag || !( tag->name() == "iodata" && tag->hasAttribute( XMLNS, XMLNS_IODATA ) ) )
      return;

    m_status.elapsed = -1;
    m_status.remaining = -1;
    m_status.percentage = -1;

    m_type = ioType( tag->findAttribute( "type" ) );
    Tag* m = 0;
    switch( m_type )
    {
      case TypeInput:
        m = tag->findChild( "in" );
        if( m )
          m_in = m->clone();
        break;
      case TypeIoSchemataResult:
        m = tag->findChild( "desc" );
        if( m )
          m_desc = m->cdata();

        m = tag->findChild( "out" );
        if( m )
          m_out = m->clone();

        m = tag->findChild( "in" );
        if( m )
          m_in = m->clone();
        break;
      case TypeOutput:
        m = tag->findChild( "out" );
        if( m )
          m_out = m->clone();
        break;
      case TypeError:
        m = tag->findChild( "error" );
        if( m )
          m_error = m->clone();
        break;
      case TypeStatus:
        m = tag->findChild( "status" );
        if( m )
        {
          Tag* t = m->findChild( "elapsed" );
          if( t )
            m_status.elapsed = atoi( t->cdata().c_str() );

          t = m->findChild( "remaining" );
          if( t )
            m_status.remaining = atoi( t->cdata().c_str() );

          t = m->findChild( "percentage" );
          if( t )
            m_status.percentage = atoi( t->cdata().c_str() );

          t = m->findChild( "information" );
          if( t )
            m_status.info = t->cdata();
        }
        break;
      case TypeIoSchemataGet:
      case TypeGetStatus:
      case TypeGetOutput:
      default:
        break;
    }

  }

  IOData::~IOData()
  {
    delete m_in;
    delete m_out;
    delete m_error;
  }

  Tag* IOData::tag() const
  {
    if( m_type == TypeInvalid )
      return 0;

    Tag* i = new Tag( "iodata" );
    i->setXmlns( XMLNS_IODATA );
    i->addAttribute( "type", util::lookup( m_type, ioTypes ) );

    Tag* t = 0;
    switch( m_type )
    {
      case TypeInput:
        i->addChild( m_in );
        break;
      case TypeIoSchemataResult:
        i->addChild( m_in );
        i->addChild( m_out );
        new Tag( i, "desc", m_desc );
        break;
      case TypeOutput:
        i->addChild( m_out );
        break;
      case TypeError:
        i->addChild( m_error );
        break;
      case TypeStatus:
        t = new Tag( i, "status" );
        if( m_status.elapsed >= 0 )
          new Tag( t, "elapsed", util::int2string( m_status.elapsed ) );
        if( m_status.remaining >= 0 )
          new Tag( t, "remaining", util::int2string( m_status.remaining ) );
        if( m_status.percentage >= 0 )
          new Tag( t, "percentage", util::int2string( m_status.percentage ) );
        if( m_status.info.length() )
          new Tag( t, "information", m_status.info );
        break;
      case TypeIoSchemataGet:
      case TypeGetStatus:
      case TypeGetOutput:
      default:
        break;
    }

    return i;
  }

  IOData* IOData::clone() const
  {
    IOData* i = new IOData( m_type );
    i->m_status = m_status;
    i->m_desc = m_desc;

    if( m_in )
      i->m_in = m_in->clone();
    if( m_out )
      i->m_out = m_out->clone();
    if( m_error )
      i->m_error = m_error->clone();

    return i;
  }

  void IOData::setIn( Tag* in )
  {
    if( !in )
      return;

    delete m_in;

    if( in->name() == "in" && in->xmlns() == EmptyString )
      m_in = in;
    else
    {
      m_in = new Tag( "in" );
      m_in->addChild( in );
    }
  }

  void IOData::setOut( Tag* out )
  {
    if( !out )
      return;

    delete m_out;

    if( out->name() == "out" && out->xmlns() == EmptyString )
      m_out = out;
    else
    {
      m_out = new Tag( "out" );
      m_out->addChild( out );
    }
  }

  void IOData::setError( Tag* error )
  {
    if( !error )
      return;

    delete m_error;

    if( error->name() == "error" && error->xmlns() == EmptyString )
      m_error = error;
    else
    {
      m_error = new Tag( "error" );
      m_error->addChild( error );
    }
  }


}

