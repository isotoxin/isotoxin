/*
  Copyright (c) 2012-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/

#include "linklocalmanager.h"

#ifdef HAVE_MDNS

#include "linklocalhandler.h"
#include "connectiontcpclient.h"
#include "jid.h"
#include "util.h"

#include <cstdio>

#if ( !defined( _WIN32 ) && !defined( _WIN32_WCE ) ) || defined( __SYMBIAN32__ )
# include <unistd.h>
# include <arpa/inet.h>
#endif

#if defined( _WIN32 ) && !defined( __SYMBIAN32__ )
# include <winsock.h>
#elif defined( _WIN32_WCE )
# include <winsock2.h>
#endif

#define LINKLOCAL_SERVICE_PORT 5562

const std::string LINKLOCAL_SERVICE_TYPE = "_presence._tcp";

namespace gloox
{

  namespace LinkLocal
  {

    Manager::Manager( const std::string& user, ConnectionHandler* connHandler, const LogSink &logInstance )
      : m_publishRef( 0 ), m_browseRef( 0 ), m_user( user ), m_interface( 0 ), m_port( 0 ),
        m_logInstance( logInstance ), m_browseFd( 0 ), m_server( connHandler, m_logInstance, EmptyString, LINKLOCAL_SERVICE_PORT ),
        m_linkLocalHandler( 0 ), m_connectionHandler( connHandler )
    {

      setPort( LINKLOCAL_SERVICE_PORT ); // does more than just setting m_port
      addTXTData( "node", GLOOX_CAPS_NODE );
    }

    Manager::~Manager()
    {
      deregisterService();
      stopBrowsing();
    }

    void Manager::addTXTData( const std::string& key, const std::string& value )
    {
      if( value.empty() || key.empty() || key == "txtvers" )
        return;

      m_txtData[key] = value;
    }

    void Manager::removeTXTData( const std::string& key )
    {
      m_txtData.erase( key );
    }

    void Manager::registerService()
    {
      if( m_publishRef )
        deregisterService();

      m_server.connect();

      std::string txtRecord;
      txtRecord += (char)9; // length of mandatory txtvers=1
      txtRecord += "txtvers=1"; // this is here because it SHOULD be the first entry
      StringMap::const_iterator it = m_txtData.begin();
      for( ; it != m_txtData.end(); ++it )
      {
        txtRecord += (char)( (*it).first.length() + (*it).second.length() + 1 );
        txtRecord += (*it).first;
        txtRecord += '=';
        txtRecord += (*it).second;
      }

      std::string service = m_user + "@";
      if( m_host.empty() )
      {
        char host[65];
        gethostname( host, 65 );
        service += host;
      }
      else
        service += m_host;

      /*DNSServiceErrorType e =*/ DNSServiceRegister( &m_publishRef,
                                                  0,                                      // flags
                                                  m_interface,                            // interface, 0 = any, -1 = local only
                                                  service.c_str(),                        // service name, 0 = local computer name
                                                  LINKLOCAL_SERVICE_TYPE.c_str(),         // service type
                                                  m_domain.c_str(),                       // domain, 0 = default domain(s)
                                                  m_host.c_str(),                         // host, 0 = default host name(s)
                                                  htons( m_port ),                        // port
                                                  (short unsigned int)txtRecord.length(), // TXT record length
                                                  (const void*)txtRecord.c_str(),         // TXT record
                                                  0,                                      // callback
                                                  0 );                                    // context
    }

    void Manager::deregisterService()
    {
      if( m_publishRef )
      {
        DNSServiceRefDeallocate( m_publishRef );
        m_publishRef = 0;
      }
    }

    bool Manager::startBrowsing()
    {
      if( !m_linkLocalHandler )
        return false;

      if( m_browseRef )
        stopBrowsing();

      DNSServiceErrorType e = DNSServiceBrowse( &m_browseRef,
                                                0,                              // flags, currently ignored
                                                m_interface,                    // interface, 0 = any, -1 = local only
                                                LINKLOCAL_SERVICE_TYPE.c_str(), // service type
                                                m_domain.c_str(),               // domain, 0 = default domain(s)
                                                &handleBrowseReply,             // callback
                                                this );                         // context
      if ( e != kDNSServiceErr_NoError )
        return false;

      return true;
    }

    void Manager::stopBrowsing()
    {
      if( m_browseRef )
      {
        DNSServiceRefDeallocate( m_browseRef );
        m_browseRef = 0;
      }
    }

    void Manager::recv( int timeout )
    {
      if( !m_browseRef )
        return;

      struct timeval tv;

      fd_set fds;
      FD_ZERO( &fds );
      // the following causes a C4127 warning in VC++ Express 2008 and possibly other versions.
      // however, the reason for the warning can't be fixed in gloox.
      FD_SET( DNSServiceRefSockFD( m_browseRef ), &fds );

      tv.tv_sec = timeout / 1000000;
      tv.tv_usec = timeout % 1000000;

      if( select( FD_SETSIZE, &fds, 0, 0, timeout == -1 ? 0 : &tv ) > 0 )
      {
        if( FD_ISSET( DNSServiceRefSockFD( m_browseRef ), &fds ) != 0 )
          DNSServiceProcessResult( m_browseRef );
      }

      m_server.recv( timeout );
    }


    void Manager::handleBrowseReply( DNSServiceRef /*sdRef*/, DNSServiceFlags flags, uint32_t interfaceIndex,
                                     DNSServiceErrorType errorCode, const char* serviceName, const char* regtype,
                                     const char* replyDomain, void* context )
    {
      if( !context || errorCode != kDNSServiceErr_NoError )
        return;

      Flag f = ( flags & kDNSServiceFlagsAdd ) == kDNSServiceFlagsAdd
                                              ? AddService
                                              : RemoveService;

      Manager* m = static_cast<Manager*>( context );
      m->handleBrowse( f, serviceName, regtype, replyDomain, interfaceIndex, ( flags & kDNSServiceFlagsMoreComing ) == kDNSServiceFlagsMoreComing );

    }

    void Manager::handleBrowse( Flag flag, const std::string& service, const std::string& regtype, const std::string& domain, int iface, bool moreComing )
    {
      Service s( flag, service, regtype, domain, interface );
      m_tmpServices.push_back( s );

//       switch( flag )
//       {
//         case AddService:
//         {
//           m_services.push_back( s );
//           break;
//         }
//         case RemoveService:
//         {
//           ServiceList::iterator it = m_services.begin();
//           for( ; it != m_services.end(); ++it )
//           {
//             if( (*it)->service == service && (*it)->regtype == regtype && (*it)->domain == domain )
//             {
//               m_services.erase( it );
//             }
//           }
//           break;
//         }
//       }

      if( !moreComing )
      {
        m_linkLocalHandler->handleBrowseReply( m_tmpServices );
        m_tmpServices.clear();
      }
    }


//     const StringMap Manager::decodeTXT( const std::string& txt )
//     {
//       StringMap result;
//       if( txt.empty() )
//         return result;
//
//       std::string::const_iterator it = txt.begin();
//       while( it < txt.end() )
//       {
//         int len = (int)(*it);
//         std::string tmp( ++it, it + len + 1 );
//         it += len;
//         size_t pos = tmp.find( '=' );
//         result.insert( std::make_pair( tmp.substr( 0, pos ), tmp.substr( pos + 1 ) ) );
//       }
//
//       return result;
//     }

  }

}

#endif // HAVE_MDNS
