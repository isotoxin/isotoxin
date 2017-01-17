/*
  Copyright (c) 2004-2016 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/



#include "gloox.h"

#include "config.h"

#include "connectiontcpserver.h"
#include "connectiontcpclient.h"
#include "connectionhandler.h"
#include "dns.h"
#include "logsink.h"
#include "mutex.h"
#include "mutexguard.h"
#include "util.h"

#ifdef __MINGW32__
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

#if ( !defined( _WIN32 ) && !defined( _WIN32_WCE ) ) || defined( __SYMBIAN32__ )
# include <netinet/in.h>
# include <arpa/nameser.h>
# include <resolv.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <sys/select.h>
# include <unistd.h>
# include <errno.h>
#endif

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
# include <winsock2.h>
# include <ws2tcpip.h>
#elif defined( _WIN32_WCE )
# include <winsock2.h>
#endif

#include <cstdlib>
#include <string>

#ifndef _WIN32_WCE
# include <sys/types.h>
#endif

// remove for 1.1
#ifndef INVALID_SOCKET
# define INVALID_SOCKET -1
#endif

namespace gloox
{

  ConnectionTCPServer::ConnectionTCPServer( ConnectionHandler* ch, const LogSink& logInstance,
                                            const std::string& ip, int port )
    : ConnectionTCPBase( 0, logInstance, ip, port ),
      m_connectionHandler( ch )
  {
  }

  ConnectionTCPServer::~ConnectionTCPServer()
  {
  }

  ConnectionBase* ConnectionTCPServer::newInstance() const
  {
    return new ConnectionTCPServer( m_connectionHandler, m_logInstance, m_server, m_port );
  }

  // remove for 1.1
  int ConnectionTCPServer::getSocket( int af, int socktype, int proto, const LogSink& logInstance )
  {
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
    SOCKET fd;
#else
    int fd;
#endif
    if( ( fd = ::socket( af, socktype, proto ) ) == INVALID_SOCKET )
    {
      std::string message = "getSocket( "
      + util::int2string( af ) + ", "
      + util::int2string( socktype ) + ", "
      + util::int2string( proto )
      + " ) failed. "
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      "WSAGetLastError: " + util::int2string( ::WSAGetLastError() );
#else
      "errno: " + util::int2string( errno ) + ": " + strerror( errno );
#endif
      logInstance.dbg( LogAreaClassDns, message );

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
      if( WSACleanup() != 0 )
      {
        logInstance.dbg( LogAreaClassDns, "WSACleanup() failed. WSAGetLastError: "
        + util::int2string( ::WSAGetLastError() ) );
      }
#endif

      return -ConnConnectionRefused;
    }

#ifdef HAVE_SETSOCKOPT
    int timeout = 5000;
    int reuseaddr = 1;
    setsockopt( fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof( timeout ) );
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof( reuseaddr ) );
#endif

    return (int)fd;
  }

  ConnectionError ConnectionTCPServer::connect()
  {
    util::MutexGuard mg( &m_sendMutex );

    if( m_socket >= 0 || m_state > StateDisconnected )
      return ConnNoError;

    m_state = StateConnecting;

    if( m_socket < 0 )
      m_socket = DNS::getSocket( m_logInstance );

    if( m_socket < 0 )
      return ConnIoError;

#ifdef HAVE_SETSOCKOPT
    int buf = 0;
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
    int bufbytes = sizeof( int );
#else
    socklen_t bufbytes = sizeof( int );
#endif
    if( ( getsockopt( m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&buf, &bufbytes ) != -1 ) &&
        ( m_bufsize > buf ) )
      setsockopt( m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&m_bufsize, sizeof( m_bufsize ) );

    if( ( getsockopt( m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&buf, &bufbytes ) != -1 ) &&
        ( m_bufsize > buf ) )
      setsockopt( m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&m_bufsize, sizeof( m_bufsize ) );
#endif

    int status = 0;
    int err = 0;
    struct addrinfo hints;
    struct addrinfo *res;

    memset( &hints, 0, sizeof hints );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    status = getaddrinfo( m_server.empty() ? 0 : m_server.c_str(), util::int2string( m_port ).c_str(), &hints, &res );
    if( status != 0 )
    {
      err = errno;
      std::string message = "getaddrinfo() for " + ( m_server.empty() ? std::string( "*" ) : m_server )
          + " (" + util::int2string( m_port ) + ") failed. "
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
          "WSAGetLastError: " + util::int2string( ::WSAGetLastError() );
#else
          + strerror( err ) + " (errno: " + util::int2string( err ) + ")";
#endif
      m_logInstance.dbg( LogAreaClassConnectionTCPServer, message );

      return ConnIoError;
    }

    m_socket = ::socket( res->ai_family, res->ai_socktype, res->ai_protocol );

    if( bind( m_socket, res->ai_addr, res->ai_addrlen ) < 0 )
    {
      err = errno;
      std::string message = "bind() to " + ( m_server.empty() ? std::string( "*" ) : m_server )
          + " (" + /*inet_ntoa( local.sin_addr ) + ":" +*/ util::int2string( m_port ) + ") failed. "
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
          "WSAGetLastError: " + util::int2string( ::WSAGetLastError() );
#else
          + strerror( err ) + " (errno: " + util::int2string( err ) + ")";
#endif
      m_logInstance.dbg( LogAreaClassConnectionTCPServer, message );

      DNS::closeSocket( m_socket, m_logInstance );
      return ConnIoError;
    }

    if( listen( m_socket, 10 ) < 0 )
    {
      err = errno;
      std::string message = "listen() on " + ( m_server.empty() ? std::string( "*" ) : m_server )
          + " (" + /*inet_ntoa( local.sin_addr ) +*/ ":" + util::int2string( m_port ) + ") failed. "
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
          "WSAGetLastError: " + util::int2string( ::WSAGetLastError() );
#else
          + strerror( err ) + " (errno: " + util::int2string( err ) + ")";
#endif
      m_logInstance.dbg( LogAreaClassConnectionTCPServer, message );

      DNS::closeSocket( m_socket, m_logInstance );
      return ConnIoError;
    }

    m_cancel = false;
    return ConnNoError;
  }

  ConnectionError ConnectionTCPServer::recv( int timeout )
  {
    m_recvMutex.lock();

    if( m_cancel || m_socket < 0 || !m_connectionHandler )
    {
      m_recvMutex.unlock();
      return ConnNotConnected;
    }

    if( !dataAvailable( timeout ) )
    {
      m_recvMutex.unlock();
      return ConnNoError;
    }

    struct sockaddr_storage they;
    int addr_size = sizeof( struct sockaddr_storage );
#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
    int newfd = static_cast<int>( accept( static_cast<SOCKET>( m_socket ), (struct sockaddr*)&they, &addr_size ) );
#else
    int newfd = accept( m_socket, (struct sockaddr*)&they, (socklen_t*)&addr_size );
#endif

    m_recvMutex.unlock();

    char buffer[INET6_ADDRSTRLEN];
    char portstr[NI_MAXSERV];
    int err = getnameinfo( (struct sockaddr*)&they, addr_size, buffer, sizeof( buffer ),
                           portstr, sizeof( portstr ), NI_NUMERICHOST | NI_NUMERICSERV );
    if( err )
      return ConnIoError;

    ConnectionTCPClient* conn = new ConnectionTCPClient( m_logInstance, buffer,
                                                         atoi( portstr ) );
    conn->setSocket( newfd );
    m_connectionHandler->handleIncomingConnection( this, conn );

    return ConnNoError;
  }

}
