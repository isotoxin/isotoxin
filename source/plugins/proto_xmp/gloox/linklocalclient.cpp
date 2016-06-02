/*
  Copyright (c) 2012-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/

#include "linklocalclient.h"

#ifdef HAVE_MDNS

#include "gloox.h"
#include "tag.h"
#include "util.h"
#include "connectiontcpclient.h"

#include <cstdio>

#if ( !defined( _WIN32 ) && !defined( _WIN32_WCE ) ) || defined( __SYMBIAN32__ )
# include <arpa/inet.h>
#endif

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
# include <winsock.h>
#elif defined( _WIN32_WCE )
# include <winsock2.h>
#endif

namespace gloox
{

  namespace LinkLocal
  {

    Client::Client( const JID& jid )
      : gloox::Client( jid, EmptyString ), m_qRef( 0 ), m_rRef( 0 ), m_currentRef( 0 ),
        m_interface( 0 ), m_port( 0 ), m_streamSent( false )
    {
    }

    Client::~Client()
    {
    }

    bool Client::connect()
    {
      return ClientBase::connect( false );
    }

    bool Client::connect( const std::string& service, const std::string& type,
                          const std::string& domain, int iface )
    {
      m_interface = interface;
      return resolve( service, type, domain );
    }

    ConnectionError Client::recv( int timeout )
    {
      if( m_connection && m_connection->state() == StateConnected )
        return ClientBase::recv( timeout );
      else
      {
        if( !m_currentRef )
          return ConnNoError;

        struct timeval tv;

        fd_set fds;
        FD_ZERO( &fds );
        // the following causes a C4127 warning in VC++ Express 2008 and possibly other versions.
        // however, the reason for the warning can't be fixed in gloox.
        FD_SET( DNSServiceRefSockFD( m_currentRef ), &fds );

        tv.tv_sec = timeout / 1000000;
        tv.tv_usec = timeout % 1000000;

        if( select( FD_SETSIZE, &fds, 0, 0, timeout == -1 ? 0 : &tv ) > 0 )
        {
          if( FD_ISSET( DNSServiceRefSockFD( m_currentRef ), &fds ) != 0 )
            DNSServiceProcessResult( m_currentRef );
        }

        return ConnNoError;
      }
    }


    bool Client::resolve( const std::string& service, const std::string& type,
                          const std::string& domain )
    {
      m_to = service;
      m_rRef = 0;
      DNSServiceErrorType e = DNSServiceResolve( &m_rRef, 0, m_interface, service.c_str(), type.c_str(),
                                                 domain.c_str(), (DNSServiceResolveReply)&handleResolveReply, this );
      if( e != kDNSServiceErr_NoError )
      {
        DNSServiceRefDeallocate( m_rRef );
        m_rRef = 0;
        return false;
      }
      m_currentRef = m_rRef;

      return true;
    }

    void Client::handleResolveReply( DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                     DNSServiceErrorType errorCode, const char* fullname, const char* hosttarget,
                                     uint16_t port, uint16_t txtLen, const unsigned char* txtRecord, void* context )
    {
      if( !context || errorCode != kDNSServiceErr_NoError )
        return;

      // printf("Client::handleResolveReply susccessful, querying %s\n", hosttarget );

      static_cast<Client*>( context )->query( hosttarget, ntohs( port ) );
    }

    bool Client::query( const std::string& hostname, int port )
    {
      m_port = port;
      m_qRef = 0;
      DNSServiceErrorType e = DNSServiceQueryRecord( &m_qRef, 0, m_interface, hostname.c_str(), kDNSServiceType_A,
                                                     kDNSServiceClass_IN, (DNSServiceQueryRecordReply)&handleQueryReply, this );
      if( e != kDNSServiceErr_NoError )
      {
        // printf( "Client::query() failed\n" );
        DNSServiceRefDeallocate( m_qRef );
        m_qRef = 0;
        return false;
      }
      m_currentRef = m_qRef;

      return true;
    }

    void Client::handleQueryReply( DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                   DNSServiceErrorType errorCode, const char *fullname, uint16_t rrtype,
                                   uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl,
                                   void *context )
    {
      // printf("Client::handleQueryReply returned\n" );

      if( !context || errorCode != kDNSServiceErr_NoError )
        return;

      const unsigned char* rd = static_cast<const unsigned char*>( rdata );
      std::string addr = util::int2string( rd[0] );
      addr += '.';
      addr += util::int2string( rd[1] );
      addr += '.';
      addr += util::int2string( rd[2] );
      addr += '.';
      addr += util::int2string( rd[3] );
      // printf( "host %s is at %s\n", fullname, addr.c_str() );
      static_cast<Client*>( context )->handleQuery( addr );
    }

    void Client::handleQuery( const std::string& addr )
    {
      if( m_rRef )
      {
        DNSServiceRefDeallocate( m_rRef );
        m_rRef = 0;
      }

      ConnectionTCPClient* connection = new ConnectionTCPClient( this, logInstance(), addr, m_port );
      // printf( "LinkLocal::Client: connecting to %s:%d\n", addr.c_str(), m_port );
      ConnectionError e = connection->connect();
      if( e != ConnNoError )
      {
        // printf( "connection error: %d\n", e );
        delete connection;
      }
    }

    void Client::handleConnect( const ConnectionBase* connection )
    {
      if( m_qRef )
      {
        DNSServiceRefDeallocate( m_qRef );
        m_qRef = 0;
        m_currentRef = 0;
      }

      // printf( "LinkLocal::Client::handleConnect()!!!\n" );
      ConnectionBase* cb = const_cast<ConnectionBase*>( connection );
      gloox::Client::setConnectionImpl( cb );
      gloox::Client::connect( false );
      sendStart( m_to );
    }

    void Client::handleStartNode( const Tag* start )
    {
      // printf( "LinkLocal::Client::handleStartNode()\n" );
      if( start && !m_streamSent )
        sendStart( start->findAttribute( "from" ) );
    }

    void Client::sendStart( const std::string& to )
    {
      m_streamSent = true;
      std::string s = "<?xml version='1.0' encoding='UTF-8'?><stream:stream xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' to='";
      s += to;
      s += "' from='";
      s += m_jid.full().c_str();
      s += "' version='1.0'>";
      send( s );
    }

  }

}

#endif // HAVE_MDNS
