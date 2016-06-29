#include "stdafx.h"
#include "gloox/client.h"
#include <gloox/messagehandler.h>
#include <gloox/messagesession.h>
#include <gloox/messagesessionhandler.h>
#include <gloox/messageeventfilter.h>
#include <gloox/chatstatefilter.h>
#include <gloox/chatstatehandler.h>
#include <gloox/chatstate.h>
#include <gloox/connectionlistener.h>
#include <gloox/rosterlistener.h>
#include <gloox/rostermanager.h>
#include <gloox/receipt.h>
#include <gloox/message.h>
#include <gloox/delayeddelivery.h>
#include <gloox/disco.h>
#include <gloox/discohandler.h>
#include <gloox/capabilities.h>
#include <gloox/nickname.h>
#include <gloox/iq.h>
#include <gloox/vcard.h>
#include <gloox/vcardmanager.h>
#include <gloox/vcardhandler.h>
#include <gloox/connectiontcpclient.h>
#include <gloox/connectionsocks5proxy.h>
#include <gloox/connectionhttpproxy.h>
#include <gloox/siprofileft.h>
#include <gloox/siprofilefthandler.h>
#include <gloox/bytestreamdatahandler.h>
#include <gloox/bytestream.h>
#include <gloox/error.h>
#include <gloox/softwareversion.h>
#include <gloox/socks5bytestream.h>
#include "memory"

#pragma warning(push)
#pragma warning(disable : 4324) // 'crypto_generichash_blake2b_state' : structure was padded due to __declspec(align())
#include "sodium.h"

#pragma USELIB("plgcommon")

#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "Dnsapi.lib")
#pragma comment(lib, "Winmm.lib")
//#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "zlib.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

#include "appver.inl"

#define XMPP_SAVE_VERSION 1
#define FEATURE_VERSION 2

#define FE_INIT     (1<<0)
#define FE_XEP_0184 (1<<1)
#define FE_XEP_0084 (1<<2) // avatars
#define FE_XEP_0085 (1<<3) // chat states
#define FE_XEP_0065 (1<<4) // chat states

#define XMLNS_AVATARS "urn:xmpp:avatar:data"


#define AR_NOT_SENT 0
#define AR_SENT 1
#define AR_SENT_AND_ACCEPTED 2

#define AR_NOT_RCVD 0
#define AR_RCVD (1<<2)
#define AR_RCVD_AND_ACCEPTED (2<<2)


inline asptr xsptr( const std::string&s )
{
    return asptr( s.c_str(), (int)s.length() );
}

inline bool samejid( const gloox::JID& jid1, const gloox::JID& jid2 )
{
    asptr b1 = xsptr( jid1.bare() );
    asptr b2 = xsptr( jid2.bare() );
    if ( b1.l != b2.l ) return false;
    return CHARz_equal_ignore_case( b1.s, b2.s, b1.l );
}

std::string decodeerror( gloox::StanzaError e )
{
    switch ( e )
    {
    case gloox::StanzaErrorBadRequest:
        return "Bad request";

    case gloox::StanzaErrorConflict:
        return "Access cannot be granted because an existing resource or session exists with the same name or address";

    case gloox::StanzaErrorFeatureNotImplemented:
        return "The feature requested is not implemented by the recipient or server and therefore cannot be processed";

    case gloox::StanzaErrorForbidden:
        return "The requesting entity does not possess the required permissions to perform the action";

    case gloox::StanzaErrorGone:
        return "The recipient or server can no longer be contacted at this address (the error stanza MAY contain a new address in the XML character data of the <gone> element)";

    case gloox::StanzaErrorInternalServerError:
        return "The server could not process the stanza because of a misconfiguration or an otherwise-undefined internal server error";

    case gloox::StanzaErrorItemNotFound:
        return "The addressed JID or item requested cannot be found";

    case gloox::StanzaErrorJidMalformed:
        return "The sending entity has provided or communicated an XMPP address (e.g., a value of the 'to' attribute) or aspect thereof (e.g., a resource identifier) that does not adhere to the syntax defined in Addressing Scheme (Section 3)";

    case gloox::StanzaErrorNotAcceptable:
        return "The recipient or server understands the request but is refusing to process it because it does not meet criteria defined by the recipient or server (e.g., a local policy regarding acceptable words in messages)";

    case gloox::StanzaErrorNotAllowed:
        return "The recipient or server does not allow any entity to perform the action";

    case gloox::StanzaErrorNotAuthorized:
        return "The sender must provide proper credentials before being allowed to perform the action, or has provided improper credentials";

    case gloox::StanzaErrorNotModified:
        return "The item requested has not changed since it was last requested";

    case gloox::StanzaErrorPaymentRequired:
        return "The requesting entity is not authorized to access the requested service because payment is required";

    case gloox::StanzaErrorRecipientUnavailable:
        return "The intended recipient is temporarily unavailable";

    case gloox::StanzaErrorRedirect:
        return "The recipient or server is redirecting requests for this information to another entity, usually temporarily";

    case gloox::StanzaErrorRegistrationRequired:
        return "The requesting entity is not authorized to access the requested service because registration is required";

    case gloox::StanzaErrorRemoteServerNotFound:
        return "A remote server or service specified as part or all of the JID of the intended recipient does not exist";

    case gloox::StanzaErrorRemoteServerTimeout:
        return "A remote server or service specified as part or all of the JID of the intended recipient (or required to fulfill a request) could not be contacted within a reasonable amount of time";

    case gloox::StanzaErrorResourceConstraint:
        return "The server or recipient lacks the system resources necessary to service the request";

    case gloox::StanzaErrorServiceUnavailable:
        return "The server or recipient does not currently provide the requested service";

    case gloox::StanzaErrorSubscribtionRequired:
        return "The requesting entity is not authorized to access the requested service because a subscription is required";

    case gloox::StanzaErrorUndefinedCondition:
        return "The error condition is not one of those defined by the other conditions in this list";

    case gloox::StanzaErrorUnexpectedRequest:
        return "The recipient or server understood the request but was not expecting it at this time (e.g., the request was out of order)";

    case gloox::StanzaErrorUnknownSender:
        return "The stanza 'from' address specified by a connected client is not valid for the stream (e.g., the stanza does not include a 'from' address when multiple resources are bound to the stream)";

    }

    return gloox::EmptyString;
}

class xmpp :
    public gloox::MessageHandler,
    public gloox::ConnectionListener,
    public gloox::RosterListener,
    public gloox::TagHandler,
    public gloox::PresenceHandler,
    public gloox::VCardHandler,
    public gloox::SIProfileFTHandler,
    public gloox::DiscoHandler,
    public gloox::IqHandler
{
    struct nodefeatures_s;
    struct contact_descriptor_s;
    friend struct contact_descriptor_s;
    friend void operator<<( chunk &chunkm, const nodefeatures_s &f );
    friend void operator<<( chunk &chunkm, const contact_descriptor_s &f );

    struct file_transfer_s
    {
        virtual ~file_transfer_s();
        virtual void accepted() {}
        virtual void kill() {}
        virtual void pause() {}
        virtual void finished() {}
        virtual void resume() {}

        u64 fsz;
        u64 utag;

        gloox::Bytestream *bs = nullptr;

        gloox::JID jid;
        std::string fileid;

        i32 cid;

        void die();
    };

    struct incoming_file_s;
    struct transmitting_file_s;
    incoming_file_s *if_first = nullptr;
    incoming_file_s *if_last = nullptr;
    transmitting_file_s *of_first = nullptr;
    transmitting_file_s *of_last = nullptr;

    struct incoming_file_s : public file_transfer_s, public gloox::BytestreamDataHandler
    {
        incoming_file_s *prev;
        incoming_file_s *next;

        u64 chunk_offset = 0;
        u64 next_recv_offset = 0;
        u64 position = 0;
        std::vector<byte> chunk;

        str_c fn;

        bool fin = false;

        incoming_file_s( const gloox::JID &jid, i32 cid_, const std::string &fileid_, u64 fsz_, const asptr &fn );
        ~incoming_file_s();

        void recv_data( const byte *data, size_t datasz );

        /*virtual*/ void accepted() override
        {
            //hf->file_control( utag, FIC_UNPAUSE );
        }
        /*virtual*/ void kill() override;
        /*virtual*/ void finished() override;
        /*virtual*/ void pause() override
        {
            //hf->file_control( utag, FIC_PAUSE );
        }


        /*virtual*/ void handleBytestreamData( gloox::Bytestream* /*ibs*/, const std::string& data ) override
        {
            recv_data( (const byte *)data.data(), data.length() );
        }
        /*virtual*/ void handleBytestreamError( gloox::Bytestream* /*ibs*/, const gloox::IQ& /*iq*/ ) override
        {

        }
        /*virtual*/ void handleBytestreamOpen( gloox::Bytestream* /*ibs*/ ) override
        {
            position = 0;
        }
        /*virtual*/ void handleBytestreamClose( gloox::Bytestream* /*ibs*/ ) override
        {
            if ( next_recv_offset == fsz )
                finished();
            else
                kill();
        }
    };

    struct transmitting_file_s : public file_transfer_s, public gloox::BytestreamDataHandler
    {
        transmitting_file_s *prev;
        transmitting_file_s *next;
        str_c fn;

        u64 next_offset_send = 0;
        static const u32 chunk_size = 4096;

        bool is_accepted = false;
        bool is_paused = false;
        bool is_finished = false;
        bool killed = false;

        transmitting_file_s( const gloox::JID &jid_, u32 cid_, u64 utag_, const std::string &fileid_, u64 fsz_, const asptr &fn );
        ~transmitting_file_s();

        /*virtual*/ void accepted() override;
        /*virtual*/ void kill() override;
        /*virtual*/ void finished() override;

        /*virtual*/ void pause() override
        {
            is_paused = true;
            //hf->file_control( utag, FIC_PAUSE );
        }

        struct app_req_s
        {
            const void * buf = nullptr;
            u64 offset = 0;
            int size = 0;

            bool set( const file_portion_s *fp )
            {
                if ( nullptr == buf && fp->offset == offset )
                {
                    buf = fp->data;
                    offset = fp->offset;
                    size = fp->size;
                    return true;
                }
                return false;
            }


            void clear();
            ~app_req_s()
            {
                clear();
            }
        };
        app_req_s rch[ 2 ];
        bool request = false;

        void resume_from( u64 offset );

        bool portion( const file_portion_s *fp )
        {
            ASSERT( fp->size == FILE_TRANSFER_CHUNK || ( fp->offset + fp->size ) == fsz );

            if ( rch[ 0 ].set( fp ) )
            {
                ASSERT( rch[ 1 ].buf == nullptr );
                send_data();
                query_next_chunk();

            }
            else
            {
                ASSERT( rch[ 0 ].offset + FILE_TRANSFER_CHUNK == fp->offset && rch[ 1 ].buf == nullptr );
                CHECK( rch[ 1 ].set( fp ) );
            }

            return true;
        }

        void query_next_chunk();

        void send_data()
        {
            if ( !bs )
                return;

            if ( nullptr == rch[ 0 ].buf )
            {
                query_next_chunk();
                return;
            }

            char temp[ chunk_size ];

            for ( ; next_offset_send >= rch[ 0 ].offset && next_offset_send < fsz; )
            {
                if ( ( next_offset_send + chunk_size ) <= ( rch[ 0 ].offset + rch[ 0 ].size ) )
                {
                    // within pre-chunk block
                    // safely send

                    if ( bs->isOpen() )
                    {
                        std::string s2s( (const char *)rch[ 0 ].buf + ( next_offset_send - rch[ 0 ].offset ), chunk_size );
                        if ( !bs->send( s2s ) )
                            return; // pipe overload. do nothing now
                        bs->recv( 1 );
                    } else
                    {
                        bs->recv( 1 );
                        return;
                    }

                    next_offset_send += chunk_size;
                    continue;
                }
                if ( next_offset_send == ( rch[ 0 ].offset + rch[ 0 ].size ) )
                {
                    // pre-chunk block completely send

                    if ( rch[ 1 ].offset > 0 )
                    {
                        // at least next chunk requested
                        rch[ 0 ].clear();
                        rch[ 0 ] = rch[ 1 ];
                        rch[ 1 ].buf = nullptr;
                        rch[ 1 ].clear();
                        query_next_chunk();

                    }
                    else
                    {
                        // next chunk not yet requested
                        // request it now

                        query_next_chunk();

                        rch[ 0 ].clear();
                        rch[ 0 ] = rch[ 1 ];
                        rch[ 1 ].buf = nullptr;
                        rch[ 1 ].clear();
                    }

                    if ( nullptr == rch[ 0 ].buf )
                        return;

                    continue;
                }

                int chunk0part = (int)( ( rch[ 0 ].offset + rch[ 0 ].size ) - next_offset_send );

                if ( fsz == ( rch[ 0 ].offset + rch[ 0 ].size ) )
                {
                    // last chunk

                    ASSERT( ( next_offset_send + chunk0part ) == fsz );

                    if ( bs->isOpen() )
                    {
                        std::string s2s( (const char *)rch[ 0 ].buf + ( next_offset_send - rch[ 0 ].offset ), chunk0part );
                        if ( !bs->send( s2s ) )
                            return; // pipe overload. do nothing now
                        bs->recv( 1 );
                    } else
                    {
                        bs->recv( 1 );
                        return;
                    }

                    next_offset_send += chunk0part;
                    return;
                }

                // we should send packet with end-of-0 and begin-of-1 chunk

                int chunk1part = (int)( ( next_offset_send + chunk_size ) - ( rch[ 0 ].offset + rch[ 0 ].size ) );

                ASSERT( chunk0part > 0 && chunk0part < (int)chunk_size );
                ASSERT( chunk1part > 0 && chunk1part < (int)chunk_size );
                ASSERT( u32( chunk0part + chunk1part ) == chunk_size );
                ASSERT( next_offset_send < ( rch[ 0 ].offset + rch[ 0 ].size ) && ( next_offset_send + chunk_size ) >( rch[ 0 ].offset + rch[ 0 ].size ) );

                if ( nullptr == rch[ 1 ].buf )
                {
                    // chunk 1 not yet ready
                    query_next_chunk();
                    return;
                }

                if ( chunk1part > rch[ 1 ].size )
                    chunk1part = rch[ 1 ].size;

                memcpy( temp, (const uint8_t *)rch[ 0 ].buf + ( next_offset_send - rch[ 0 ].offset ), chunk0part );
                memcpy( temp + chunk0part, (const uint8_t *)rch[ 1 ].buf, chunk1part );

                if ( bs->isOpen() )
                {
                    std::string s2s( temp, chunk0part + chunk1part );
                    if ( !bs->send( s2s ) )
                        return; // pipe overload. do nothing now
                    bs->recv( 1 );
                } else
                {
                    bs->recv( 1 );
                    return;
                }

                next_offset_send = rch[ 1 ].offset + chunk1part;

                rch[ 0 ].clear();
                rch[ 0 ] = rch[ 1 ];
                rch[ 1 ].buf = nullptr;
                rch[ 1 ].clear();
                query_next_chunk();
            }
        }

        void ontick();

        /*virtual*/ void handleBytestreamData( gloox::Bytestream* /*ibs*/, const std::string& data ) override
        {
        }
        /*virtual*/ void handleBytestreamError( gloox::Bytestream* /*ibs*/, const gloox::IQ& /*iq*/ ) override
        {

        }
        /*virtual*/ void handleBytestreamOpen( gloox::Bytestream* /*ibs*/ ) override
        {
        }
        /*virtual*/ void handleBytestreamClose( gloox::Bytestream* /*ibs*/ ) override
        {
            kill();
        }

    };


    void make_uniq_utag( u64 &utag )
    {
    again_check:
        //for ( transmitting_data_s *f = of_first; f; f = f->next )
        //    if ( f->utag == utag )
        //    {
        //        ++utag;
        //        goto again_check;
        //    }
        for ( incoming_file_s *f = if_first; f; f = f->next )
            if ( f->utag == utag )
            {
                ++utag;
                goto again_check;
            }
    }


    struct contact_descriptor_s;
    struct mses : public gloox::MessageSession
    {
        contact_descriptor_s *owner = nullptr;
        //gloox::MessageEventFilter *flt = nullptr;
        //gloox::ChatStateFilter *chflt = nullptr;
        mses( contact_descriptor_s *owner, gloox::ClientBase *clb, const gloox::JID& jid ) :owner( owner ), gloox::MessageSession( clb, jid ) {}
        virtual ~mses()
        {
            if ( owner )
            {
                for( contact_descriptor_s::resource_s &r : owner->resources )
                    if (r.session == this)
                        r.session = nullptr;
            }
        }
    };

    struct contact_descriptor_s : public gloox::DiscoHandler, public gloox::VCardHandler, public gloox::IqHandler
    {
        contact_descriptor_s *next = nullptr;
        contact_descriptor_s *prev = nullptr;

        gloox::JID jid;
        std::vector<u64> waiting_receipt;

        byte gavatar_hash[16]; // md5 hash of avatar
        std::string gavatar; // use string to keep binary data...
        int gavatar_tag = 0;

        struct resource_s
        {
            mses* session = nullptr;
            str_c resname;
            str_c cl_url;
            str_c cl_name_ver;
            str_c os;
            int id = 0;
            int features = 0;
            int priority = 0;
            resource_s(const asptr& rn, int id, int pr):resname(rn), id(id), priority(pr) {}
            resource_s() {}
        };
        std::vector<resource_s> resources;

        str_c name;
        str_c statusmsg;

        contact_state_e st = CS_UNKNOWN;
        contact_online_state_e ost = COS_ONLINE;
        int cid = 0;
        int changed = CDM_PUBID | CDM_NAME | CDM_STATE | CDM_ONLINE_STATE | CDM_AVATAR_TAG;
        int residpool = 1;

        gloox::ChatStateType chstlast = gloox::ChatStateInactive;

        int subscription_flags = AR_NOT_RCVD | AR_NOT_SENT;
        void slv_set_sentrequest() { subscription_flags = ( subscription_flags & ~3 ) | AR_SENT; }
        void slv_set_subscribed() { subscription_flags = ( subscription_flags & ~3 ) | AR_SENT_AND_ACCEPTED; }
        void slv_set_unsubscribed() { subscription_flags = AR_NOT_RCVD | AR_NOT_SENT; }
        int slv_from_self() const { return subscription_flags & 3; };
        int slv_from_other() const { return (subscription_flags>>2) & 3; };
        void slv_up_from_self() { if (CHECK( slv_from_self() < 2 )) subscription_flags = (subscription_flags & ~3) | (slv_from_self() + 1); };
        void slv_up_from_other() { if (CHECK( slv_from_other() < 2)) subscription_flags = ( subscription_flags & ~(3<<2) ) | ( (slv_from_other() + 1) << 2 ); };

        bool nickname = false;
        bool getavatar = false;
        bool details_sent = false;
        bool rejected = false;

        contact_descriptor_s( const gloox::JID& jid, int id ) :jid( jid ), cid(id) 
        {
            name.set( xsptr(jid.username()) );
            memset( gavatar_hash, 0, sizeof( gavatar_hash ) );
            if ( id == 0 )
                st = CS_OFFLINE;
        }
        ~contact_descriptor_s()
        {
            for ( resource_s &r : resources )
                if ( r.session )
                    r.session->owner = nullptr;
        }

        void on_offline();

        bool init_state( bool is_online )
        {
            contact_state_e newst = CS_UNKNOWN;
            switch ( (slv_from_other() << 2) | slv_from_self() )
            {
            case AR_NOT_RCVD | AR_SENT: // invite sent
                newst = CS_INVITE_SEND; break;
            case AR_NOT_RCVD | AR_SENT_AND_ACCEPTED: // invite sent and accepted, so it is no matter about request from other
            case AR_RCVD_AND_ACCEPTED | AR_SENT_AND_ACCEPTED:
                newst = is_online ? CS_ONLINE : CS_OFFLINE; break;

            case AR_RCVD | AR_NOT_SENT: // invite received
                newst = CS_INVITE_RECEIVE; break;

            case AR_RCVD | AR_SENT: // invite received and invite sent, but invite from other must be accepted
            case AR_RCVD | AR_SENT_AND_ACCEPTED: // invite received and invite sent and accepted, but invite from other must be accepted
            case AR_RCVD_AND_ACCEPTED | AR_NOT_SENT: // invite received and accepted, but invite must be sent
                ERROR("impossible");
                break;
            case AR_RCVD_AND_ACCEPTED | AR_SENT: // invite received and accepted, waiting form accept invite from self
                newst = CS_WAIT; break;
            }
            if (st != newst)
            {
                st = newst;
                changed |= CDM_STATE;
            }
            return changed != 0;
        }

        void remove_resource( const asptr &rname )
        {
            for ( auto it = resources.begin(); it != resources.end(); ++it )
            {
                if ( it->resname.equals(rname) )
                {
                    *it = resources.back();
                    resources.resize( resources.size() - 1 );
                    break;
                }
            }
            changed |= CDM_DETAILS;
            details_sent = false;
        }
        resource_s & getcreateres( const asptr &rname, int pr )
        {
            for ( auto it = resources.begin(); it != resources.end(); ++it )
            {
                if ( it->resname.equals( rname ) )
                {
                    if (pr >= 0)
                        it->priority = pr;
                    return *it;
                }
            }
            resources.push_back( resource_s(rname, residpool++, pr >= 0 ? pr : 0) );
            changed |= CDM_DETAILS;
            details_sent = false;
            return resources.back();
        }
        bool is_feature( int mask ) const
        {
            for ( const resource_s &r : resources )
                if ( ISFLAG( r.features, mask ) )
                    return true;
            return false;
        }

        void prepare_details( str_c &tmps, contact_data_s &cd )
        {
            if ( !details_sent )
            {
                tmps.set( CONSTASTR( "{\"" CDET_PUBLIC_ID "\":\"" ) );
                tmps.append( xsptr(jid.full()) );

                //tmps.append( asptr( "\",\"" CDET_CLIENT_CAPS "\":[" ) );
                //for ( token<char> bbsupported( bbcodes_supported, ',' ); bbsupported; ++bbsupported )
                //    tmps.append( CONSTASTR( "\"bb" ) ).append( *bbsupported ).append( CONSTASTR( "\"," ) );

                tmps.append( CONSTASTR( "\",\"" CDET_CLIENT "\":[" ) );
                for ( auto&r : resources )
                {
                    tmps.append( CONSTASTR( "\"" ) ).append( json_value(r.cl_name_ver) )
                        .append( CONSTASTR( " (" ) ).append( json_value(r.cl_url)).append(CONSTASTR(")"));
                    if ( !r.os.is_empty() )
                        tmps.append( CONSTASTR( " (" ) ).append( json_value( r.os ) ).append( CONSTASTR( ")" ) );
                    tmps.append( CONSTASTR( "\"," ) );
                }
                tmps.trunc_length().append( CONSTASTR( "]" ) );


                tmps.append( CONSTASTR( "}" ) );

                cd.mask |= CDM_DETAILS;
                cd.details = tmps.cstr();
                cd.details_len = (int)tmps.get_length();

                details_sent = true;
            }

        }

        void setup_subscription( const gloox::RosterItem& ritm, bool cancel_subscription = false )
        {
            int osf = subscription_flags;
            switch( ritm.subscription() )
            {
            case gloox::S10nNone:
                subscription_flags = AR_NOT_RCVD | AR_NOT_SENT;
                break;
            case gloox::S10nNoneOut:
                subscription_flags = AR_NOT_RCVD | AR_SENT;
                break;
            case gloox::S10nNoneIn:
                subscription_flags = AR_RCVD | AR_NOT_SENT;
                break;
            case gloox::S10nNoneOutIn:
                subscription_flags = AR_RCVD | AR_SENT;
                break;
            case gloox::S10nTo:
                subscription_flags = AR_NOT_RCVD | AR_SENT_AND_ACCEPTED;
                break;
            case gloox::S10nToIn:
                subscription_flags = AR_RCVD | AR_SENT_AND_ACCEPTED;
                break;
            case gloox::S10nFrom:
                subscription_flags = AR_RCVD_AND_ACCEPTED | AR_NOT_SENT;
                break;
            case gloox::S10nFromOut:
                if ( cancel_subscription )
                    subscription_flags = AR_RCVD_AND_ACCEPTED | AR_NOT_SENT; // contact still not accept request, so cancel our subscription
                else
                    subscription_flags = AR_RCVD_AND_ACCEPTED | AR_SENT;
                break;
            case gloox::S10nBoth:
                subscription_flags = AR_RCVD_AND_ACCEPTED | AR_SENT_AND_ACCEPTED;
                break;
            }
            if (osf != subscription_flags)
            {
                fix_bad_subscription_state( cancel_subscription );
                init_state( ritm.online() );
            }
        }

        void fix_bad_subscription_state( bool cancel_subscription = false );

        void setup( const gloox::RosterItem& ritm, const asptr& resname, gloox::Presence::PresenceType presence )
        {
            if ( presence == gloox::Presence::Unavailable && rejected )
            {
                remove_resource( resname );
                if ( resources.empty() )
                    rejected = false;
                return;
            }

            rejected = false;

            setup_subscription( ritm, presence == gloox::Presence::Probe && resname.l == 0 );
            bool o = ritm.online();

            if ( !nickname )
            {
                std::string n = ritm.name();
                if ( n.empty() )
                    n = ritm.jidJID().username();

                if ( !n.empty() && !name.equals( xsptr( n ) ) )
                {
                    name.set( xsptr( n ) );
                    changed |= CDM_NAME;
                }
            }
            if ( o && (st == CS_OFFLINE) )
            {
                st = CS_ONLINE;
                changed |= CDM_STATE;
            } else if ( !o && (st == CS_ONLINE) )
            {
                st = CS_OFFLINE;
                changed |= CDM_STATE;
            }

            contact_online_state_e ost1 = COS_ONLINE;
            switch ( presence )
            {
            case gloox::Presence::Away:
            case gloox::Presence::XA:
                ost1 = COS_AWAY;
                break;
            case gloox::Presence::DND:
                ost1 = COS_DND;
                break;
            case gloox::Presence::Unavailable:
                if ( resname.l )
                {
                    remove_resource( resname );
                }
                break;
            }
            if ( ost != ost1 )
                ost = ost1, changed |= CDM_ONLINE_STATE;
        }

        void prepare_session( xmpp * me )
        {
            for ( resource_s &r : resources )
                if ( !r.session )
                {
                    gloox::JID rjid( jid );
                    rjid.setResource( std::string(r.resname.cstr(),r.resname.get_length()) );
                    r.session = new mses( this, me->j.get(), rjid );
                    r.session->registerMessageHandler( me );
                }
        }

        void chat_state( xmpp * me, gloox::ChatStateType chst )
        {
            prepare_session( me );
            if ( chstlast != chst )
            {
                chstlast = chst;
                for ( resource_s &r : resources )
                {
                    gloox::StanzaExtensionList lst;
                    lst.push_back( new gloox::ChatState( chst ) );
                    r.session->send( gloox::EmptyString, gloox::EmptyString, lst );
                }
            }
        }

        void send(xmpp * me, const message_s *msg)
        {
            prepare_session( me );

            bool need_recp = false;
            for ( resource_s &r : resources )
            {
                if ( need_recp && 0 != ( r.features & FE_XEP_0184 ) )
                {
                    waiting_receipt.push_back( msg->utag );
                    sstr_t<-32> id; id.set_as_num( msg->utag );
                    gloox::StanzaExtensionList lst;
                    lst.push_back( new gloox::Receipt( gloox::Receipt::Request ) );
                    r.session->send( std::string( msg->message, msg->message_len ), std::string( id.cstr(), id.get_length() ), lst );
                    need_recp = false;
                }
                else
                {
                    r.session->send( std::string( msg->message, msg->message_len ) );
                }
            }

            if ( !need_recp )
                me->hf->delivered( msg->utag );
        }

        /*virtual*/ void handleDiscoInfo( const gloox::JID& from, const gloox::Disco::Info& info, int context ) override;
        /*virtual*/ void handleDiscoItems( const gloox::JID& from, const gloox::Disco::Items& items, int context ) override
        {
        }
        /*virtual*/ void handleDiscoError( const gloox::JID& from, const gloox::Error* error, int context ) override
        {

        }
        /*virtual*/ void handleVCard( const gloox::JID& ijid, const gloox::VCard* vcard ) override;
        /*virtual*/ void handleVCardResult( VCardContext context, const gloox::JID& ijid, gloox::StanzaError se ) override
        {
        }

        /*virtual*/ bool handleIq( const gloox::IQ& iq ) override { return false; }
        /*virtual*/ void handleIqID( const gloox::IQ& iq, int context ) override;

    };
    contact_descriptor_s *first = nullptr; // first points to zero contact - self
    contact_descriptor_s *last = nullptr;

    void del( contact_descriptor_s * c )
    {
        LIST_DEL( c, first, last, prev, next );
        delete c;
    }
    contact_descriptor_s *find( int id )
    {
        for ( contact_descriptor_s *i = first; i; i = i->next )
            if ( i->cid == id ) return i;
        return nullptr;
    }
    contact_descriptor_s *find( const gloox::JID jid )
    {
        for ( contact_descriptor_s *i = first; i; i = i->next )
            if ( samejid(i->jid, jid) ) return i;
        return nullptr;
    }


    host_functions_s *hf = nullptr;
    std::unique_ptr<gloox::Client> j;
    std::unique_ptr<gloox::VCardManager> cm;
    std::unique_ptr<gloox::SIProfileFT> fm;
    

    struct nodefeatures_s
    {
        str_c node;
        time_t crtime = 0;
        i32 features = 0;
        nodefeatures_s() {}
        explicit nodefeatures_s( const asptr&s, time_t t = now() ):node(s), crtime(t) {}
        nodefeatures_s( const asptr&s, time_t t, i32 f ) :node( s ), crtime( t ), features(f) {}
    };
    std::vector< nodefeatures_s > node2features;

    nodefeatures_s & getcreatef( const asptr &node )
    {
        for ( auto it = node2features.begin(); it != node2features.end(); ++it )
        {
            if ( it->node.equals( node ) )
            {
                return *it;
            }
        }
        node2features.push_back( nodefeatures_s( node ) );
        return node2features.back();
    }

    time_t next_reconnect_time = 0;

    int self_typing_contact = 0;
    int self_typing_start_time = 0;

    struct other_typing_s
    {
        int cid = 0;
        int time = 0;
        int totaltime = 0;
        other_typing_s( int cid, int time ) :cid( cid ), time( time ), totaltime( time ) {}
    };
    std::vector<other_typing_s> other_typing;

    struct host_s
    {
        str_c host;
        uint16_t port = 0;
        host_s() {}
    };

    i32 proxy_type = 0;
    host_s proxy;
    void set_proxy_addr( const asptr& addr )
    {
        token<char> p( addr, ':' );
        proxy.host.clear();
        proxy.port = 0;
        if ( p ) proxy.host = *p;
        ++p; if ( p ) proxy.port = (uint16_t)p->as_uint();
    }

    std::vector<gloox::StreamHost> bytestreamproxy;

    bool restart_module = false;
    bool connecting = false;
    bool online_flag = false;
    bool set_cfg_called = false;
    bool reconnect = false;
    bool dirty_vcard = true;
    bool auth_failed = false;

    enum chunks_e // HADR ORDER!!!!!!!!
    {
        chunk_magic,
        //chunk_jid,
        //chunk_password,

        chunk_features = 100,
        chunk_feature,
        chunk_feature_crtime,
        chunk_feature_node,
        chunk_feature_flags,
        chunk_feature_version,

        chunk_contacts = 200,

        chunk_contact,

        chunk_contact_id,
        chunk_contact_jid,
        chunk_contact_name,
        chunk_contact_nickname,
        __chunk_contact_state,
        chunk_contact_avatar_hash,
        chunk_contact_avatar_tag,
        chunk_contact_subscription,

        chunk_other = 240,
        chunk_proxy_type,
        chunk_proxy_address,

    };

    /*virtual*/ void onConnect() override
    {
        connecting = false;
        first->st = CS_ONLINE;
        first->changed |= CDM_STATE;
        update_contact(nullptr);
        next_reconnect_time = 0;

        gloox::JID serverjid = first->jid;
        serverjid.setUsername( gloox::EmptyString );
        serverjid.setResource( gloox::EmptyString );

        j->disco()->getDiscoItems( serverjid, gloox::EmptyString, this, 0 );
    }

    /*virtual*/ void onDisconnect( gloox::ConnectionError ) override
    {
        connecting = false;

        for ( contact_descriptor_s *f = first; f; f = f->next )
        {
            if (f->st == CS_ONLINE)
            {
                f->st = CS_OFFLINE;
                f->changed |= CDM_STATE;
                update_contact( f );
            }
            f->on_offline();
        }
    }
    /*virtual*/ bool onTLSConnect( const gloox::CertInfo&  ) override
    {
        return true;
    }

    /*virtual*/ bool handleIq( const gloox::IQ& iq ) override
    {
        return false;
    }
    /*virtual*/ void handleIqID( const gloox::IQ& iq, int context ) override
    {
        if ( 2 == context )
        {
            bytestreamproxy.clear();
            if ( const gloox::SOCKS5BytestreamManager::Query *bytestr = iq.findExtension<gloox::SOCKS5BytestreamManager::Query>( gloox::ExtS5BQuery ) )
            {
                for ( const auto &h : bytestr->hosts() )
                    bytestreamproxy.emplace_back( h );
                if ( !bytestreamproxy.empty() )
                    fm->setStreamHosts( bytestr->hosts() );
            }
        }
    }

    /*virtual*/ void handleDiscoInfo( const gloox::JID& from, const gloox::Disco::Info& info, int context ) override
    {
        if ( info.hasFeature( gloox::XMLNS_BYTESTREAMS ) )
        {
            gloox::Tag* t = new gloox::Tag( "iq" );
            t->addAttribute( "from", first->jid.full() );
            t->addAttribute( "to", from.full() );
            std::string id = j->getID();
            t->addAttribute( "id", id );
            t->addAttribute( gloox::TYPE, "get" );
            gloox::Tag* q = new gloox::Tag( "query" );
            q->setXmlns( gloox::XMLNS_BYTESTREAMS );
            t->addChild(q);

            j->add_iq_handler( id, this, 2 );

            j->send(t);
        }
    }
    /*virtual*/ void handleDiscoItems( const gloox::JID& from, const gloox::Disco::Items& items, int context ) override
    {
        for( const auto &itm : items.items() )
        {
            j->disco()->getDiscoInfo( itm->jid(), gloox::EmptyString, this, 1 );
        }
    }
    /*virtual*/ void handleDiscoError( const gloox::JID& from, const gloox::Error* error, int context ) override
    {

    }


    /*virtual*/ void handleMessage( const gloox::Message& stanza, gloox::MessageSession*ses  ) override
    {
        const gloox::Nickname *nn = stanza.findExtension<gloox::Nickname>( gloox::ExtNickname );

        const gloox::Receipt *r = stanza.findExtension<gloox::Receipt>( gloox::ExtReceipt );
        if ( r && gloox::Receipt::Received == r->rcpt() )
        {
            contact_descriptor_s &cd = getcreate_descriptor( stanza.from(), true );
            u64 utag = pstr_c( xsptr( r->id() ) ).as_num<u64>();
            if ( removeIfPresentFast( cd.waiting_receipt, utag ) )
                hf->delivered( utag );
            return;
        }


        time_t msgtime = now();
        if ( const gloox::DelayedDelivery *dd = stanza.when() )
        {
            //"2016-05-25T10:14:08.780Z"
            str_c tstamp( xsptr( dd->stamp() ) );
            tstamp.replace_all( 'T', '-' );
            tstamp.replace_all( ':', '-' );
            tstamp.replace_all( '.', '-' );

            token<char> dt( tstamp, '-' );

            tm tt = {0};
            tt.tm_year = dt->as_int() - 1900; ++dt;
            tt.tm_mon = dt->as_int() - 1; ++dt;
            tt.tm_mday = dt->as_int(); ++dt;

            tt.tm_hour = dt->as_int(); ++dt;
            tt.tm_min = dt->as_int(); ++dt;
            tt.tm_sec = dt->as_int();

            msgtime = _mkgmtime(&tt);
        }

        const gloox::Error *e = stanza.findExtension<gloox::Error>( gloox::ExtError );
        gloox::JID from( stanza.from() );
        std::string s = stanza.subject();
        std::string b = stanza.body();
        std::string d;
        if (e)
        {
            s = "Error: "; s.append( from.full() );
            from.setUsername( gloox::EmptyString );
            from.setResource( gloox::EmptyString );
            d = decodeerror( e->error() );
        }

        contact_descriptor_s &cd = getcreate_descriptor( from, true );
        if ( first == &cd ) return; // something wrong
        cd.prepare_session( this );

        if ( from.username().empty() )
        {
            if ( !s.empty() || !b.empty() || !d.empty() )
            {
                str_c msg( CONSTASTR( "{\"" SMF_SUBJECT "\":\"" ), json_value(xsptr( s )) );
                msg.append( CONSTASTR( "\",\"" SMF_TEXT "\":\"" ) ).append( json_value(xsptr(b)) );
                msg.append( CONSTASTR( "\",\"" SMF_DESCRIPTION "\":\"" ) ).append( json_value( xsptr( d ) ) );

                msg.append( CONSTASTR( "\"}" ) );

                // message from server
                hf->message( MT_SYSTEM_MESSAGE, 0, cd.cid, msgtime, msg.cstr(), msg.get_length() );
            }
            return;
        }

        if ( const gloox::ChatState *chst = stanza.findExtension<gloox::ChatState>( gloox::ExtChatState ) )
        {
            bool is_typing = chst->state() == gloox::ChatStateComposing;

            for ( auto it = other_typing.begin(); it != other_typing.end(); ++it )
            {
                if ( it->cid == cd.cid )
                {
                    if ( !is_typing )
                        other_typing.erase( it );
                    break;
                }
            }

            if ( is_typing )
                other_typing.emplace_back( cd.cid, time_ms() );
        }


        if ( nn )
        {
            if ( !cd.name.equals( xsptr( nn->nick() ) ) )
            {
                cd.name = xsptr( nn->nick() );
                cd.changed |= CDM_NAME;
                update_contact( &cd );
            }
            cd.nickname = true;
        }

        if ( !b.empty() )
        {
            for ( auto it = other_typing.begin(); it != other_typing.end(); ++it )
            {
                if ( it->cid == cd.cid )
                {
                    other_typing.erase( it );
                    break;
                }
            }

            hf->message( MT_MESSAGE, 0, cd.cid, msgtime, b.c_str(), b.length() );

            if ( r )
            {
                gloox::StanzaExtensionList lst;
                lst.push_back( new gloox::Receipt( gloox::Receipt::Received, stanza.id() ) );
                ses->send( gloox::EmptyString, j->getID(), lst );
            }
        }
    }

    /*virtual*/ void handleItemAdded( const gloox::JID& jid ) override
    {
        getcreate_descriptor( jid, true );
    }

    /*virtual*/ void handleItemSubscribed( const gloox::JID& jid ) override
    {
        if ( contact_descriptor_s *c = find( jid ) )
        {
            if ( c == first )
                return;

            if ( gloox::RosterItem *ritm = j->rosterManager()->getRosterItem( jid ) )
                c->setup_subscription( *ritm );

            if (c->init_state( c->st == CS_ONLINE ))
                update_contact( c );
            hf->save();
        }
    }

    /*virtual*/ void handleItemRemoved( const gloox::JID& jid ) override
    {
        handleItemUnsubscribed( jid );
    }

    /*virtual*/ void handleItemUpdated( const gloox::JID& jid ) override
    {

    }
    /*virtual*/ void handleItemUnsubscribed( const gloox::JID& jid ) override
    {
        if ( contact_descriptor_s *c = find( jid ) )
        {
            if ( c == first )
                return;

            j->rosterManager()->cancel( c->jid ); // revoke too!
            j->rosterManager()->remove( c->jid );
            c->rejected = c->st == CS_INVITE_SEND;

            c->slv_set_unsubscribed();
            if (c->init_state( c->st == CS_ONLINE ))
                update_contact( c );
            hf->save();
        }
    }

    /*virtual*/ void handleRoster( const gloox::Roster& roster )
    {
        for ( gloox::Roster::const_iterator it = roster.begin(); it != roster.end(); ++it )
        {
            contact_descriptor_s &cd = getcreate_descriptor( it->second->jidJID(), false );
            if ( first == &cd ) continue;; // something wrong
            cd.setup( *it->second, asptr(), gloox::Presence::Probe );
            if (cd.changed) update_contact( &cd );
        }
    }
    /*virtual*/ void handleRosterPresence( const gloox::RosterItem& item, const std::string& resource, gloox::Presence::PresenceType presence, const std::string& msg ) override
    {
        if ( resource.empty() && presence == gloox::Presence::Unavailable )
            return; //wot?

        contact_descriptor_s &cd = getcreate_descriptor( item.jidJID(), false );
        if ( first == &cd ) return; // something wrong
        cd.setup( item, xsptr( resource ), presence );
        if ( cd.changed ) update_contact( &cd );

    }
    /*virtual*/ void handlePresence( const gloox::Presence& presence ) override
    {
        if ( contact_descriptor_s *cd = find( presence.from() ) )
        {
            if ( cd == first )
                return;

            if ( gloox::Presence::Unavailable != presence.presence() )
            {
                contact_descriptor_s::resource_s &res = cd->getcreateres( xsptr( presence.from().resource() ), presence.priority() );
                if ( const gloox::Capabilities* caps = presence.capabilities() )
                {
                    std::string n( caps->node() + '#' + caps->ver() );
                    res.cl_url = xsptr( caps->node() );
                    if ( res.cl_url.ends( CONSTASTR( "/caps" ) ) )
                        res.cl_url.trunc_length(5);
                    nodefeatures_s &f = getcreatef( xsptr(n) );
                    res.features = f.features;
                    
#ifndef _DEBUG
                    if ( 0 == ( f.features & FE_INIT ) )
#endif // _DEBUG
                        j->disco()->getDiscoInfo( presence.from(), n, cd, res.id );
                }
                if ( const gloox::Nickname *nn = presence.findExtension<gloox::Nickname>( gloox::ExtNickname ) )
                {
                    cd->name = xsptr(nn->nick());
                    cd->changed |= CDM_NAME;
                    cd->nickname = true;
                }
                
                if (!cd->statusmsg.equals( xsptr(presence.status() ) ))
                {
                    cd->statusmsg = xsptr( presence.status() );
                    cd->changed |= CDM_STATUSMSG;
                }

                cm->fetchVCard( cd->jid, cd );

                {
                    gloox::Tag* t = new gloox::Tag( "iq" );
                    t->addAttribute( "from", j->jid().full() );
                    t->addAttribute( "to", presence.from().full() );
                    std::string id = j->getID();
                    t->addAttribute( "id", id );
                    t->addAttribute( gloox::TYPE, "get" );
                    gloox::Tag* q = new gloox::Tag( "query" );
                    q->setXmlns( gloox::XMLNS_VERSION );
                    t->addChild( q );

                    j->add_iq_handler( id, cd, 4 );

                    j->send( t );
                }


                if (cd->changed)
                    update_contact( cd );
            }
        }
    }

    /*virtual*/ void handleSelfPresence( const gloox::RosterItem& item, const std::string& resource, gloox::Presence::PresenceType presence, const std::string& msg ) override
    {

    }
    /*virtual*/ bool handleSubscriptionRequest( const gloox::JID& jid, const std::string& msg ) override
    {
        contact_descriptor_s &cd = getcreate_descriptor( jid, false );
        if ( first == &cd ) return true; // something wrong

        if (gloox::RosterItem *ritm = j->rosterManager()->getRosterItem( jid ))
            cd.setup_subscription( *ritm );

        if ( cd.slv_from_other() == 2 )
        {
            cd.fix_bad_subscription_state();
            if ( cd.init_state( cd.st == CS_ONLINE ) )
                update_contact( &cd );
            return true;
        }

        if ( cd.slv_from_other() == 1 )
        {
            // already received request
            // so, notify app again
            hf->message( MT_FRIEND_REQUEST, 0, cd.cid, now(), msg.c_str(), msg.length() );
            return true;
        }

        cd.slv_up_from_other(); // now set lv to 1
        cd.fix_bad_subscription_state();

        if ( cd.init_state( cd.st == CS_ONLINE ) )
            update_contact( &cd );

        if ( cd.st == CS_INVITE_RECEIVE )
            hf->message( MT_FRIEND_REQUEST, 0, cd.cid, now(), msg.c_str(), msg.length() );

        hf->save();


        return true;
    }

    /*virtual*/ bool handleUnsubscriptionRequest( const gloox::JID& jid, const std::string& msg ) override
    {

        return true;
    }
    /*virtual*/ void handleNonrosterPresence( const gloox::Presence& presence ) override
    {
        handlePresence( presence );
    }
    /*virtual*/ void handleRosterError( const gloox::IQ& iq ) override
    {
    }
    /*virtual*/ void handleTag( gloox::Tag* tag ) override
    {

    }

    /*virtual*/ void handleVCard( const gloox::JID& ijid, const gloox::VCard* vcard ) override
    {
        __debugbreak();
    }

    /*virtual*/ void handleVCardResult( VCardContext context, const gloox::JID& ijid, gloox::StanzaError se ) override
    {

    }

    /*virtual*/ void handleFTRequest( const gloox::JID& from, const gloox::JID& to, const std::string& sid,
        const std::string& name, long size, const std::string& hash,
        const std::string& date, const std::string& mimetype,
        const std::string& desc, int stypes ) override
    {
        if ( 0 == (stypes & gloox::SIProfileFT::FTTypeIBB) )
            if ( bytestreamproxy.empty() )
            {
                // there is no supported way to transfer
                fm->declineFT( from, sid, gloox::SIManager::RequestRejected );
                return;
            }

        if (contact_descriptor_s *cd = find( from ))
        {
            if ( cd == first || cd->st != CS_ONLINE )
            {
                fm->declineFT( from, sid, gloox::SIManager::RequestRejected );
                return;
            }
            incoming_file_s *ifl = new incoming_file_s( from, cd->cid, sid, size, xsptr(name) );
            hf->incoming_file( cd->cid, ifl->utag, ifl->fsz, ifl->fn.cstr(), ifl->fn.get_length() );
        }
    }
    /*virtual*/ void handleFTRequestError( const gloox::IQ& iq, const std::string& sid ) override
    {
        for ( transmitting_file_s *f = of_first; f; f = f->next )
            if ( f->fileid == sid )
            {
                f->kill();
                break;
            }
    }
    /*virtual*/ void handleFTBytestream( gloox::Bytestream* bs ) override
    {
        for ( transmitting_file_s *f = of_first; f; f = f->next )
            if ( f->fileid == bs->sid() )
            {
                f->bs = bs;
                bs->registerBytestreamDataHandler( f );

                setup_proxy(bs);

                if ( bs->connect() )
                    f->accepted();
                else
                {
                    f->bs = bs;
                    fm->dispose( bs );
                    f->kill();
                }
                break;
            }
        for ( incoming_file_s *f = if_first; f; f = f->next )
            if ( f->fileid == bs->sid() )
            {
                f->bs = bs;
                bs->registerBytestreamDataHandler( f );
                
                setup_proxy( bs );

                if ( !bs->connect() )
                {
                    f->bs = nullptr;
                    fm->dispose( bs );
                    f->kill();
                }
                break;
            }

    }
    /*virtual*/ const std::string handleOOBRequestResult( const gloox::JID& from, const gloox::JID& to, const std::string& sid ) override
    {
        return gloox::EmptyString;
    }

    void setup_proxy( gloox::Bytestream *bs )
    {
        if (gloox::SOCKS5Bytestream *s = dynamic_cast<gloox::SOCKS5Bytestream *>( bs ))
        {
            if ( proxy_type & CF_PROXY_SUPPORT_HTTPS )
            {
                //gloox::ConnectionTCPClient *tcp = new gloox::ConnectionTCPClient( j->logInstance(), std::string( proxy.host.cstr(), proxy.host.get_length() ), proxy.port );
                //s->setConnectionImpl( new gloox::ConnectionHTTPProxy( j.get(), tcp, j->logInstance(), j->server(), j->port() ) );

            } else if ( proxy_type & ( CF_PROXY_SUPPORT_SOCKS4 | CF_PROXY_SUPPORT_SOCKS5 ) )
            {
                //gloox::ConnectionTCPClient *tcp = new gloox::ConnectionTCPClient( j->logInstance(), std::string( proxy.host.cstr(), proxy.host.get_length() ), proxy.port );
                //s->setConnectionImpl( new gloox::ConnectionSOCKS5Proxy( j.get(), tcp, j->logInstance(), j->server(), j->port() ) );
            }
        }
    }

    contact_descriptor_s &getcreate_descriptor( const gloox::JID& jid, bool update_if_new )
    {
        if ( contact_descriptor_s *d = find( jid ) )
            return *d;
        
        int newid = 1;
        for ( ; find( newid ) != nullptr; ++newid ); // slooooow. but fast enough on current cpus
        contact_descriptor_s *nc = new contact_descriptor_s( jid, newid );
        LIST_ADD( nc, first, last, prev, next );

        if ( update_if_new )
        {
            if ( !jid.resource().empty() )
                nc->jid.setResource( gloox::EmptyString );

            update_contact( nc );
        }

        return *nc;
    }

    void update_contact( contact_descriptor_s *cd )
    {
        if ( nullptr == cd )
            cd = first;

        contact_data_s self( cd->cid, CDM_GENDER | cd->changed );
        self.public_id = cd->jid.full().c_str();
        self.public_id_len = cd->jid.full().length();
        self.name = cd->name.cstr();
        self.name_len = cd->name.get_length();

        if (cd->changed & CDM_NAME)
        {
            Log( "changed name for %i to <%s>", cd->cid, cd->name.cstr() );
        }

        self.status_message = cd->statusmsg.cstr();
        self.status_message_len = cd->statusmsg.get_length();
        self.state = cd->st;
        self.ostate = cd->ost;
        self.avatar_tag = cd->gavatar_tag;

        str_c tmps;
        if ( cd == first )
        {
            if ( !self.public_id_len )
                self.public_id = "?", self.public_id_len = 1;
            self.state = online_flag ? cd->st : CS_OFFLINE;
        } else
        {
            if ( self.state == CS_UNKNOWN && 0 != ( self.mask & CDM_STATE ) )
                self.mask |= CDF_ALLOW_INVITE;

            if ( cd->jid.username().empty() )
            {
                // system user
                self.mask |= CDF_SYSTEM_USER;

            } else
            {
                cd->prepare_details( tmps, self );
            }
        }

        hf->update_contact( &self );
        cd->changed = 0;
    }

    gloox::VCard *build_vcard()
    {
        gloox::VCard *vcard = new gloox::VCard();
        if (first)
        {
            vcard->setNickname( std::string( first->name.cstr() ) );

            if ( first->gavatar.length() )
                vcard->setPhoto( "image/png", first->gavatar );
            else
                vcard->setPhoto();
        }
        dirty_vcard = false;
        return vcard;
    }


    void send_configurable()
    {
        const char * fields[] = { CFGF_PROXY_TYPE, CFGF_PROXY_ADDR };
        str_c svalues[ 2 ];
        svalues[ 0 ].set_as_int( proxy_type );
        if ( proxy_type ) svalues[ 1 ].set( proxy.host ).append_char( ':' ).append_as_uint( proxy.port );
        const char * values[] = { svalues[ 0 ].cstr(), svalues[ 1 ].cstr() };

        static_assert( ARRAY_SIZE( fields ) == ARRAY_SIZE( values ) && ARRAY_SIZE( values ) == ARRAY_SIZE( svalues ), "check len" );

        hf->configurable( 2, fields, values );
    }

    void letsconnect();

public:
    ~xmpp()
    {
        cm.reset();
        fm.reset();
        j.reset();
    }

    void on_handshake( host_functions_s *hf_ )
    {
        WSADATA wsa;
        WSAStartup( MAKEWORD( 2, 2 ), &wsa );

        hf = hf_;
    }

#define FUNC0( rt, fn ) rt fn();
#define FUNC1( rt, fn, p0 ) rt fn(p0);
#define FUNC2( rt, fn, p0, p1 ) rt fn(p0, p1);
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0

};

static xmpp cl;

xmpp::incoming_file_s::incoming_file_s( const gloox::JID &jid_, i32 cid_, const std::string &fileid_, u64 fsz_, const asptr &fn ) :fn( fn )
{
    jid = jid_;
    fsz = fsz_;
    cid = cid_;
    fileid = fileid_;

    randombytes_buf( &utag, sizeof( u64 ) );

    cl.make_uniq_utag( utag ); // requirement to unique is only utag unique across current file transfers

    LIST_ADD( this, cl.if_first, cl.if_last, prev, next );
}

xmpp::file_transfer_s::~file_transfer_s()
{
    ASSERT( bs == nullptr );
}

void xmpp::file_transfer_s::die()
{
    if ( bs )
    {
        cl.fm->dispose( bs );
        bs = nullptr;
    }
    delete this;
}


xmpp::incoming_file_s::~incoming_file_s()
{
    LIST_DEL( this, cl.if_first, cl.if_last, prev, next );
}

void xmpp::incoming_file_s::recv_data( const byte *data, size_t datasz )
{
    if ( next_recv_offset == 0 && position > 0 )
    {
        // file resume from position
        chunk_offset = position;
    }
    else if ( position != next_recv_offset )
    {
        kill();
        return;
    }

    next_recv_offset = position + datasz;
    u64 newsize = position + datasz - chunk_offset;
    if ( newsize > chunk.size() )
        chunk.resize( (u32)newsize );

    memcpy( chunk.data() + position - chunk_offset, data, datasz );

    if ( newsize >= FILE_TRANSFER_CHUNK )
    {
        if ( cl.hf->file_portion( utag, chunk_offset, chunk.data(), FILE_TRANSFER_CHUNK ) )
        {
            chunk_offset += FILE_TRANSFER_CHUNK;
            aint ostsz = chunk.size() - FILE_TRANSFER_CHUNK;
            memcpy( chunk.data(), chunk.data() + FILE_TRANSFER_CHUNK, ostsz );
            chunk.resize( ostsz );
        }
    }
    position += datasz;
}

/*virtual*/ void xmpp::incoming_file_s::kill()
{
    cl.hf->file_control( utag, FIC_BREAK );
    die();
}


/*virtual*/ void xmpp::incoming_file_s::finished()
{
    if ( chunk.size() )
    {
        if ( !cl.hf->file_portion( utag, chunk_offset, chunk.data(), (int)chunk.size() ) )
            return; // just keep file unfinished
    }

    cl.hf->file_control( utag, FIC_DONE );
    fin = true;

    //die(); do not delete now due FIC_DONE from app
}


xmpp::transmitting_file_s::transmitting_file_s( const gloox::JID &jid_, u32 cid_, u64 utag_, const std::string &fileid_, u64 fsz_, const asptr &fn ) :fn( fn )
{
    jid = jid_;
    fsz = fsz_;
    cid = cid_;
    fileid = fileid_;
    utag = utag_;

    LIST_ADD( this, cl.of_first, cl.of_last, prev, next );
}

xmpp::transmitting_file_s::~transmitting_file_s()
{
    LIST_DEL( this, cl.of_first, cl.of_last, prev, next );
}

/*virtual*/ void xmpp::transmitting_file_s::accepted()
{
    is_accepted = true;
    is_paused = false;
    cl.hf->file_control( utag, FIC_ACCEPT );
}
/*virtual*/ void xmpp::transmitting_file_s::kill()
{
    if ( killed )
        return;
    killed = true;
    cl.hf->file_control( utag, FIC_BREAK );
    die();
}
/*virtual*/ void xmpp::transmitting_file_s::finished()
{
    cl.hf->file_control( utag, FIC_DONE );

    if ( killed )
        return;
    killed = true;

    die();
}

void xmpp::transmitting_file_s::resume_from( u64 offset )
{
    rch[ 0 ].clear();
    rch[ 1 ].clear();

    u64 roffs = offset & FILE_TRANSFER_CHUNK_MASK;
    next_offset_send = offset;

    rch[ 0 ].offset = roffs;
    cl.hf->file_portion( utag, roffs, nullptr, FILE_TRANSFER_CHUNK ); // request now
    request = true;
};

void xmpp::transmitting_file_s::query_next_chunk()
{
    if ( !request )
    {
        ASSERT( rch[ 0 ].offset == 0 && rch[ 0 ].buf == nullptr );
        cl.hf->file_portion( utag, 0, nullptr, FILE_TRANSFER_CHUNK ); // request very first chunk now
        request = true;
        return;
    }

    if ( rch[ 1 ].offset > 0 )
        return;

    u64 nextr = rch[ 0 ].offset + FILE_TRANSFER_CHUNK;

    if ( fsz > nextr )
    {
        cl.hf->file_portion( utag, nextr, nullptr, FILE_TRANSFER_CHUNK ); // request now
        rch[ 1 ].offset = nextr;
    }
}

void xmpp::transmitting_file_s::ontick()
{
    bool is_cid_online = false;
    if ( contact_descriptor_s *cd = cl.find( cid ) )
        is_cid_online = cd->st == CS_ONLINE;

    if ( !is_cid_online )
    {
        // peer disconnect - transfer broken
        cl.hf->file_control( utag, FIC_DISCONNECT );
        die();
        return;
    }

    send_data();
    if ( next_offset_send == fsz )
        finished();
}


void  xmpp::transmitting_file_s::app_req_s::clear()
{
    if ( buf )
        cl.hf->file_portion( 0, 0, buf, 0 ); // release buffer

    memset( this, 0, sizeof( *this ) );
}



void xmpp::contact_descriptor_s::on_offline()
{
    if ( cl.self_typing_contact && cl.self_typing_contact == cid )
        cl.self_typing_contact = 0;

    for ( auto it = cl.other_typing.begin(); it != cl.other_typing.end(); ++it )
    {
        if ( it->cid == cid )
        {
            cl.other_typing.erase( it );
            break;
        }
    }
}


void xmpp::contact_descriptor_s::fix_bad_subscription_state( bool cancel_subscription )
{
    switch ( subscription_flags ) // now fix bad states
    {
    case AR_RCVD | AR_SENT: // invite received and invite sent, but invite from other must be accepted
        cl.j->rosterManager()->ackSubscriptionRequest( jid, true );
        slv_up_from_other();
        ASSERT( subscription_flags == ( AR_RCVD_AND_ACCEPTED | AR_SENT ) );
        break;

    case AR_RCVD | AR_SENT_AND_ACCEPTED: // invite received and invite sent and accepted, but invite from other must be accepted
        cl.j->rosterManager()->ackSubscriptionRequest( jid, true );
        slv_up_from_other();
        ASSERT( subscription_flags == ( AR_RCVD_AND_ACCEPTED | AR_SENT_AND_ACCEPTED ) );
        break;

    case AR_RCVD_AND_ACCEPTED | AR_NOT_SENT: // invite received and accepted, but invite must be sent
        if ( cancel_subscription )
        {
            cl.j->rosterManager()->cancel( jid );
            slv_set_unsubscribed();
        } else
        {
            slv_up_from_self();
            cl.j->rosterManager()->subscribe( jid, gloox::EmptyString, gloox::StringList(), "plz accept" );
            ASSERT( subscription_flags == ( AR_RCVD_AND_ACCEPTED | AR_SENT ) );
        }
        break;
    }

}


/*virtual*/ void xmpp::contact_descriptor_s::handleVCard( const gloox::JID& ijid, const gloox::VCard* vcard )
{
    ASSERT( jid == ijid );

    str_c onn = name;
    if ( !vcard->nickname().empty() )
        name = xsptr( vcard->nickname() ), nickname = true;
    if ( !onn.equals( name ) )
        changed |= CDM_NAME;
    const gloox::VCard::Photo& phi = vcard->photo();

    byte avahash[ 16 ];
    if ( phi.binval.length() )
    {
        gavatar = phi.binval;
        md5_c md5;
        md5.update( phi.binval.data(), phi.binval.length() );
        md5.done( avahash );
        if ( 0 != memcmp( avahash, gavatar_hash, 16 ) )
        {
            memcpy( gavatar_hash, avahash, 16 );
            ++gavatar_tag;
        }
    }
    else
    {
        gavatar.clear();
        gavatar_tag = 0;
        memset( gavatar_hash, 0, sizeof( avahash ) );
    }

    changed |= CDM_AVATAR_TAG; // always send avatar tag

    if ( changed )
        cl.update_contact( this );

    if ( getavatar )
        cl.hf->avatar_data( cid, gavatar_tag, gavatar.data(), gavatar.size() ), getavatar = false;
}

/*virtual*/ void xmpp::contact_descriptor_s::handleIqID( const gloox::IQ& iq, int context )
{
    if ( 4 == context )
    {
        if ( const gloox::SoftwareVersion *sv = iq.findExtension<gloox::SoftwareVersion>( gloox::ExtVersion ) )
        {
            resource_s &r = getcreateres( xsptr( iq.from().resource() ), -1 );
            r.cl_name_ver.set( xsptr( sv->name() ) ).append_char( '/' ).append( xsptr( sv->version() ) );
            r.os = xsptr( sv->os() );
            details_sent = false;
            changed |= CDM_DETAILS;
            cl.update_contact( this );
        }
    }
}


/*virtual*/ void xmpp::contact_descriptor_s::handleDiscoInfo( const gloox::JID& from, const gloox::Disco::Info& info, int context )
{
    int f = FE_INIT;
    SETUPFLAG( f, FE_XEP_0184, info.hasFeature( gloox::XMLNS_RECEIPTS ) );
    SETUPFLAG( f, FE_XEP_0084, info.hasFeature( XMLNS_AVATARS ) );
    SETUPFLAG( f, FE_XEP_0085, info.hasFeature( gloox::XMLNS_CHAT_STATES ) );
    SETUPFLAG( f, FE_XEP_0065, info.hasFeature( gloox::XMLNS_BYTESTREAMS ) );

#ifdef _DEBUG
    Log("features of: %s", from.full().c_str());
    for( const auto &s : info.features() )
        Log( " - %s", s.c_str() );
#endif
    
    for ( resource_s &r : resources )
        if ( r.id == context )
        {
            r.features = f;
            break;
        }
    cl.getcreatef( xsptr(info.node()) ).features = f;
    cl.hf->save();
}

void xmpp::offline()
{
    if ( !online_flag )
        return;

    online_flag = false;
    if (j)
    {
        j->disconnect();
    }
}

void xmpp::letsconnect()
{
    if ( j )
    {
        connecting = true;

        if ( proxy_type & CF_PROXY_SUPPORT_HTTPS )
        {
            gloox::ConnectionTCPClient *tcp = new gloox::ConnectionTCPClient( j->logInstance(), std::string(proxy.host.cstr(), proxy.host.get_length()), proxy.port );
            j->setConnectionImpl( new gloox::ConnectionHTTPProxy( j.get(), tcp, j->logInstance(), j->server(), j->port() ) );

        } else if ( proxy_type & ( CF_PROXY_SUPPORT_SOCKS4 | CF_PROXY_SUPPORT_SOCKS5 ) )
        {
            gloox::ConnectionTCPClient *tcp = new gloox::ConnectionTCPClient( j->logInstance(), std::string( proxy.host.cstr(), proxy.host.get_length() ), proxy.port );
            j->setConnectionImpl( new gloox::ConnectionSOCKS5Proxy( j.get(), tcp, j->logInstance(), j->server(), j->port() ) );

        } else
            j->setConnectionImpl( new gloox::ConnectionTCPClient( j.get(), j->logInstance(), j->server(), j->port() ) );

        j->connect( false );
    }
}
void xmpp::online()
{
    if ( online_flag )
        return;

    online_flag = true;
    next_reconnect_time = now() + 5;
    letsconnect();
}


void xmpp::call(int id, const call_info_s *ci)
{
}


void xmpp::stop_call(int id)
{
}

void xmpp::accept_call(int id)
{
}

void xmpp::stream_options(int id, const stream_options_s * so)
{
}

int xmpp::send_av(int id, const call_info_s * ci)
{
    return SEND_AV_OK;
}

void xmpp::tick(int *sleep_time_ms)
{
    if ( restart_module )
    {

        *sleep_time_ms = -1;
        return;
    }

    if ( !online_flag || !j )
    {
        *sleep_time_ms = 100;
        reconnect = false;
        return;
    }

    if ( reconnect )
    {
        offline();
        online();
        *sleep_time_ms = 1;
        reconnect = false;
        return;
    }

    int curt = time_ms();

    if ( first->st == CS_ONLINE )
    {
        if ( dirty_vcard && cm )
            cm->storeVCard( build_vcard(), this );

        // self typing
        // may be time to stop typing?
        if ( self_typing_contact )
        {
            int typing_time = ( curt - self_typing_start_time );

            if ( contact_descriptor_s *cd = find( self_typing_contact ) )
            {
                if ( typing_time > 3500 )
                    self_typing_contact = 0, cd->chat_state( this, gloox::ChatStateActive );
                else if ( typing_time > 1500 )
                    cd->chat_state( this, gloox::ChatStatePaused );
            }
        }

        // other peers typing
        // notify host
        for ( other_typing_s &ot : other_typing )
        {
            if ( ( curt - ot.time ) > 0 )
            {
                if ( contact_descriptor_s *desc = find( ot.cid ) )
                    hf->typing( 0, desc->cid );
                ot.time += 1000;
            }
        }
        for ( aint i = other_typing.size() - 1; i >= 0; --i )
        {
            other_typing_s &ot = other_typing[ i ];
            if ( ( curt - ot.time ) > 60000 )
            {
                // moar then minute!
                other_typing.erase( other_typing.begin() + i );
                break;
            }
        }
    }

    switch( j->recv( 1000 ) )
    {
    case gloox::ConnProxyAuthRequired:
    case gloox::ConnProxyAuthFailed:
    case gloox::ConnIoError:
    case gloox::ConnDnsError:
    case gloox::ConnConnectionRefused:
        hf->operation_result( LOP_ONLINE, CR_NETWORK_ERROR );
        break;
    case gloox::ConnAuthenticationFailed:
        hf->operation_result( LOP_ONLINE, CR_AUTHENTICATIONFAILED );
        break;
    case gloox::ConnNotConnected:
        {
            time_t n = now();
            if ( n > next_reconnect_time )
            {
                connecting = true;
                next_reconnect_time = now() + 10;
                letsconnect();
            }
        }
        break;
    }

    for ( incoming_file_s *f = if_first; f;)
    {
        incoming_file_s *ff = f->next;

        bool is_cid_online = false;
        if ( contact_descriptor_s *cd = find( f->cid ) )
            is_cid_online = cd->st == CS_ONLINE;

        if ( is_cid_online )
        {
            if ( f->bs && !f->fin )
                if ( gloox::ConnNotConnected == f->bs->recv( 1000 ) )
                    is_cid_online = false;
        }

        if (!is_cid_online)
        {
            // peer disconnected - transfer break
            hf->file_control( f->utag, FIC_DISCONNECT );
            f->die();
        }

        f = ff;
    }

    for ( transmitting_file_s *f = of_first; f;)
    {
        transmitting_file_s *ff = f->next;
        f->ontick();
        f = ff;
    }

    if ( auth_failed )
    {
        hf->operation_result( LOP_ONLINE, CR_AUTHENTICATIONFAILED );
        offline();
    }

    *sleep_time_ms = 20;
}

void xmpp::goodbye()
{
    set_cfg_called = false;

    while ( first )
        del(first);

    cm.reset();
    fm.reset();
    j.reset();
}

void xmpp::set_name(const char*utf8name)
{
    if ( first )
    {
        ASSERT( j );
        first->name.set( asptr(utf8name) );
        j->addPresenceExtension( new gloox::Nickname( std::string(utf8name) ) );
        
        if (cm) cm->storeVCard( build_vcard(), this );

        if ( first->st == CS_ONLINE )
        {
            j->send( j->presence() );

            for( contact_descriptor_s *cd = first->next; cd; cd = cd->next )
                if ( cd->st == CS_ONLINE )
                {
                    cd->prepare_session(this);
                    for ( const auto& r : cd->resources )
                    {
                        gloox::StanzaExtensionList lst;
                        lst.push_back( new gloox::Nickname( std::string( utf8name ) ) );
                        r.session->send( gloox::EmptyString, j->getID(), lst );
                    }
                }
        }
    }
}

void xmpp::set_statusmsg(const char*utf8status)
{
    if ( first )
    {
        ASSERT( j );
        first->statusmsg.set( asptr( utf8status ) );
        if ( first->statusmsg.is_empty() )
            j->presence().resetStatus();
        else
            j->presence().addStatus( std::string( first->statusmsg.cstr(), first->statusmsg.get_length() ) );
        if ( first->st == CS_ONLINE )
            j->send( j->presence() );
    }
}

void xmpp::set_ostate(int ostate)
{
    gloox::Presence::PresenceType t = gloox::Presence::Available;
    switch ( ostate )
    {
    case COS_ONLINE:
        t = gloox::Presence::Available;
        break;
    case COS_AWAY:
        t = gloox::Presence::Away;
        break;
    case COS_DND:
        t = gloox::Presence::DND;
        break;
    }

    if( first )
    {
        ASSERT( j );
        j->presence().setPresence( t );
        if ( first->st == CS_ONLINE )
            j->send( j->presence() );
    }
}

void xmpp::set_gender(int /*gender*/)
{
}

void xmpp::get_avatar(int id)
{
    if ( contact_descriptor_s *c = find( id ) )
    {
        if ( c->gavatar.size() )
            hf->avatar_data( id, c->gavatar_tag, c->gavatar.data(), c->gavatar.size() );
        else
            c->getavatar = true;
    }
    
}

void xmpp::set_avatar(const void*data, int isz)
{
    if ( !first ) return;

    static int prevavalen = 0;

    if ( isz == 0 && prevavalen > 0 )
    {
        prevavalen = 0;
        memset( first->gavatar_hash, 0, sizeof( first->gavatar_hash ) );
        first->gavatar.clear();
        dirty_vcard = true;

    } else
    {
        byte newavahash[ 16 ];

        md5_c md5;
        md5.update( data, isz );
        md5.done( newavahash );

        if ( 0 != memcmp( newavahash, first->gavatar_hash, 16 ) )
        {
            memcpy( first->gavatar_hash, newavahash, 16 );
            first->gavatar = std::string( (const char *)data, isz );
            dirty_vcard = true;
        }
        prevavalen = isz;
    }

}

void xmpp::set_config(const void*data, int isz)
{
    std::string cfg_password;
    gloox::JID cfg_jid;

    bool login_present = false;
    bool password_present = false;

    if ( first )
        cfg_jid = first->jid, cfg_password = j->password();
    bool proxy_settings_ok = false;

    auto prepare_j = [&]()
    {
        cm.reset();
        fm.reset();
        j = std::make_unique<gloox::Client>( first->jid, cfg_password );

        j->registerMessageHandler( this );
        j->registerConnectionListener( this );
        j->registerPresenceHandler( this );
        j->rosterManager()->registerRosterListener( this, false );

        j->registerStanzaExtension( new gloox::DelayedDelivery() );
        j->registerStanzaExtension( new gloox::Receipt( gloox::Receipt::Request ) );
        j->registerStanzaExtension( new gloox::Nickname( gloox::EmptyString ) );
        j->registerStanzaExtension( new gloox::ChatState( gloox::ChatStateActive ) );

        gloox::Disco *d = j->disco();
        d->addFeature( gloox::XMLNS_RECEIPTS );
        d->addFeature( gloox::XMLNS_NICKNAME );
        d->addFeature( gloox::XMLNS_CHAT_STATES );
        d->addFeature( gloox::XMLNS_BYTESTREAMS );
        d->addFeature( gloox::XMLNS_IBB );
        //d->addFeature( XMLNS_AVATARS );

        d->setVersion( "isotoxin", SS( PLUGINVER ) );

        gloox::Capabilities *caps = new gloox::Capabilities( d );
        caps->setNode( HOME_SITE );
        j->addPresenceExtension( caps );
        j->addPresenceExtension( new gloox::Nickname( std::string( first->name.cstr() ) ) );
        if ( first->statusmsg.is_empty() )
            j->presence().resetStatus();
        else
            j->presence().addStatus( std::string( first->statusmsg.cstr(), first->statusmsg.get_length() ) );

        cm = std::make_unique<gloox::VCardManager>( j.get() );
        cm->storeVCard( build_vcard(), this );

        fm = std::make_unique<gloox::SIProfileFT>( j.get(), this );

    };

    auto parsev = [&]( const pstr_c &field, const pstr_c &val )
    {
        if ( field.equals( CONSTASTR( CFGF_LOGIN ) ) )
        {
            gloox::JID o = cfg_jid;
            cfg_jid.setJID( std::string( val.as_sptr().s, val.get_length() ) );
            if ( o != cfg_jid )
                reconnect = true;
            login_present = true;
            return;
        }
        if ( field.equals( CONSTASTR( CFGF_PASSWORD ) ) )
        {
            if ( !val.equals( xsptr( cfg_password ) ) )
                cfg_password = std::string( val.as_sptr().s, val.get_length() ), reconnect = true;;
            password_present = true;
            return;
        }
        if ( field.equals( CONSTASTR( CFGF_PROXY_TYPE ) ) )
        {
            int new_proxy_type = val.as_int();
            if ( new_proxy_type != proxy_type )
                proxy_type = new_proxy_type, reconnect = true;
            proxy_settings_ok = true;
            return;
        }
        if ( field.equals( CONSTASTR( CFGF_PROXY_ADDR ) ) )
        {
            str_c paddr( proxy.host ); paddr.append_char( ':' ).append_as_uint( proxy.port );
            if ( !paddr.equals( val ) )
            {
                set_proxy_addr( val );
                reconnect = true;
            }
            proxy_settings_ok = true;
            return;
        }
    };

    proxy_settings_ok = false;

    config_accessor_s ca( data, isz );

    if ( ca.params.l )
        parse_values( ca.params, parsev ); // parse params

    auth_failed = login_present && !password_present;

    if ( reconnect )
    {
        if ( first )
        {
            j->setPassword( cfg_password );
            if ( first->jid != cfg_jid )
            {
                first->jid = cfg_jid;
                first->changed |= CDM_PUBID;
            }
            if (j->jid() != cfg_jid)
                prepare_j();
            update_contact( nullptr );
        }
    }

    if ( !ca.native_data && !ca.protocol_data && j )
        return;

    set_cfg_called = true;
    loader ldr( ca.protocol_data, ca.protocol_data_len );
    time_t n = now();
    if ( int sz = ldr( xmpp::chunk_features ) )
    {
        node2features.clear();

        loader l( ldr.chunkdata(), sz );
        for ( int cnt = l.read_list_size(); cnt > 0; --cnt )
            if ( int feature_size = l( chunk_feature ) )
            {
                loader lf( l.chunkdata(), feature_size );

                i32 ver = 0;
                if ( lf( chunk_feature_version ) )
                    ver = lf.get_i32();
                if (ver >= FEATURE_VERSION && lf( chunk_feature_crtime ) )
                {
                    time_t crtime = lf.get_u64();
                    if ( crtime + 86400 * 7 > n )
                    {
                        i32 flags = 0;
                        if ( lf( chunk_feature_flags ) )
                            flags = lf.get_i32();

                        if ( lf( chunk_feature_node ) )
                        {
                            str_c ns = lf.get_astr();
                            if ( !ns.is_empty() )
                                node2features.emplace_back( ns, crtime, flags );
                        }
                    }
                }

            }
    }

    if ( !first )
        first = last = new contact_descriptor_s( cfg_jid, 0 );

    if ( int sz = ldr( chunk_contacts ) )
    {
        while ( first->next )
            del( first->next );

        loader l( ldr.chunkdata(), sz );
        for ( int cnt = l.read_list_size(); cnt > 0; --cnt )
            if ( int contact_size = l( chunk_contact ) )
            {
                contact_descriptor_s *c = nullptr;

                loader lc( l.chunkdata(), contact_size );
                if ( lc( chunk_contact_id ) )
                {
                    int id = lc.get_i32();
                    if (id)
                    {
                        gloox::JID jid;
                        if ( lc( chunk_contact_jid ) )
                        {
                            asptr js = lc.get_astr();
                            jid.setJID( std::string( js.s, js.l ) );
                        }
                        c = new contact_descriptor_s( jid, id );
                        LIST_ADD( c, first, last, prev, next );
                    }
                }

                if ( c )
                {
                    if ( lc( chunk_contact_name ) )
                        c->name = lc.get_astr();

                    if ( lc( chunk_contact_nickname ) )
                        c->nickname = lc.get_byte() != 0;

                    if ( lc( chunk_contact_subscription ) )
                        c->subscription_flags = lc.get_i32();
                    else
                        c->slv_set_subscribed();

                    c->init_state( false );

                    if ( lc( chunk_contact_avatar_tag ) )
                        c->gavatar_tag = lc.get_i32();

                    if ( c->gavatar_tag )
                    {
                        if ( int ahsz = lc( chunk_contact_avatar_hash ) )
                        {
                            loader ahl( lc.chunkdata(), ahsz );
                            int dsz;
                            if ( const void *ah = ahl.get_data( dsz ) )
                                if ( ASSERT( dsz == 16 ) )
                                    memcpy( c->gavatar_hash, ah, 16 );
                        }
                    }
                }
            }
    }

    if ( !proxy_settings_ok )
    {
        proxy_type = 0;
        proxy.port = 0;
        proxy.host.clear();
    }

    if ( ldr( chunk_proxy_type ) )
        proxy_type = ldr.get_i32();
    if ( ldr( chunk_proxy_address ) )
        set_proxy_addr( ldr.get_astr() );

    prepare_j();
    send_configurable();
    hf->operation_result( LOP_SETCONFIG, cfg_jid.full().empty() || auth_failed ? CR_AUTHENTICATIONFAILED : CR_OK );
}
void xmpp::init_done()
{
    for ( contact_descriptor_s *c = first; c; c = c->next )
        update_contact( c );
}

void operator<<( chunk &chunkm, const xmpp::nodefeatures_s &f )
{
    chunk cc( chunkm.b, xmpp::chunk_feature );

    chunk( chunkm.b, xmpp::chunk_feature_version ) << (i32)FEATURE_VERSION;
    chunk( chunkm.b, xmpp::chunk_feature_crtime ) << (u64)f.crtime;
    chunk( chunkm.b, xmpp::chunk_feature_flags ) << f.features;
    chunk( chunkm.b, xmpp::chunk_feature_node ) << f.node;
}

void operator<<( chunk &chunkm, const xmpp::contact_descriptor_s &c )
{
    if ( c.cid == 0 )
        return;

    chunk cc( chunkm.b, xmpp::chunk_contact );

    chunk( chunkm.b, xmpp::chunk_contact_id ) << (i32)c.cid;
    chunk( chunkm.b, xmpp::chunk_contact_jid ) << xsptr(c.jid.full());
    chunk( chunkm.b, xmpp::chunk_contact_name ) << c.name;
    chunk( chunkm.b, xmpp::chunk_contact_nickname ) << (byte)(c.nickname ? 1 : 0);
    chunk( chunkm.b, xmpp::chunk_contact_subscription ) << (i32)c.subscription_flags;
    chunk( chunkm.b, xmpp::chunk_contact_avatar_tag ) << (i32)c.gavatar_tag;
    if ( c.gavatar_tag != 0 )
    {
        chunk( chunkm.b, xmpp::chunk_contact_avatar_hash ) << bytes( c.gavatar_hash, 16 );
    }
}

void xmpp::save_config(void * param)
{
    if ( set_cfg_called )
    {
        savebuffer b;
        chunk( b, chunk_magic ) << (u64)( 0x123BADF00D2C0FE6ull + XMPP_SAVE_VERSION );
        chunk( b, chunk_features ) << servec<nodefeatures_s>( node2features );
        chunk( b, chunk_contacts ) << serlist<contact_descriptor_s>( first );

        chunk( b, chunk_proxy_type ) << proxy_type;
        chunk( b, chunk_proxy_address ) << ( str_c( proxy.host ).append_char( ':' ).append_as_uint( proxy.port ) );

        hf->on_save( b.data(), (int)b.size(), param );
    }
}

int xmpp::resend_request(int id, const char* invite_message_utf8)
{
    if ( contact_descriptor_s *c = find( id ) )
        return add_contact( c->jid.bare().c_str(), invite_message_utf8 );
    return CR_FUNCTION_NOT_FOUND;
}

int xmpp::add_contact(const char* public_id, const char* invite_message_utf8)
{
    gloox::JID jidreq;
    if (!jidreq.setJID( public_id ))
        return CR_INVALID_PUB_ID;

    if ( jidreq.server().empty() || jidreq.username().empty() )
        return CR_INVALID_PUB_ID;

    jidreq.setResource( gloox::EmptyString );

    contact_descriptor_s &cd = getcreate_descriptor( jidreq, false );
    if (&cd == first)
        return CR_ALREADY_PRESENT;

    if ( nullptr == invite_message_utf8 )
    {
        update_contact( &cd );
        return CR_OK;
    }

    if (cd.slv_from_self() == 2 )
        return CR_ALREADY_PRESENT;

    cd.slv_set_sentrequest();

    cd.fix_bad_subscription_state();

    if (cd.init_state( false ))
        update_contact( &cd );

    //j->rosterManager()->subscribe( jidreq, gloox::EmptyString, gloox::StringList(), invite_message_utf8 );

    gloox::Subscription s( gloox::Subscription::Subscribe, jidreq.bareJID(), invite_message_utf8 );
    s.addExtension( new gloox::Nickname( std::string( first->name ) ) );
    j->send( s );

    hf->save();
    return CR_OK;
}

void xmpp::del_contact(int id)
{
    if ( contact_descriptor_s *c = find( id ) )
    {
        if ( c->st != CS_UNKNOWN && !c->jid.username().empty() )
            j->rosterManager()->remove( c->jid );
        del(c);
        hf->save();
    }
}

void xmpp::send_message(int id, const message_s *msg)
{
    if ( contact_descriptor_s *c = find( id ) )
        c->send( this, msg );
}

void xmpp::del_message( u64 utag )
{
}

void xmpp::accept(int id)
{
    if ( contact_descriptor_s *c = find( id ) )
    {
        if ( c->slv_from_other() == 0 )
            return; // hm. no invite received; nothing to accept

        j->rosterManager()->ackSubscriptionRequest( c->jid, true );

        if ( gloox::RosterItem *ritm = j->rosterManager()->getRosterItem( c->jid ) )
            c->setup_subscription( *ritm );

        if (c->slv_from_other() == 1)
            c->slv_up_from_other();

        c->fix_bad_subscription_state();

        if ( c->init_state( c->st == CS_ONLINE ) )
            update_contact( c );
        hf->save();
    }
}

void xmpp::reject(int id)
{
    if ( contact_descriptor_s *c = find( id ) )
    {
        j->rosterManager()->ackSubscriptionRequest( c->jid, false );
        j->rosterManager()->remove( c->jid );
        del( c );
        hf->save();
    }
}

void xmpp::file_send(int cid, const file_send_info_s *finfo)
{
    for ( incoming_file_s *f = if_first; f; f = f->next )
        if ( f->utag == finfo->utag )
        {
            // almost impossible
            // but we should check it anyway
            hf->file_control( finfo->utag, FIC_REJECT );
            return;
        }

    if ( finfo->filesize > 0x7fffffff )
    {
        // too big file for jabber
        hf->file_control( finfo->utag, FIC_REJECT );
        return;
    }


    if ( contact_descriptor_s *cd = find( cid ) )
    {
        str_c fnc;
        std::string fn( finfo->filename, finfo->filename_len );

        gloox::JID jid = cd->jid;

        int pr = -10000;
        for ( const auto& r : cd->resources )
        {
            if ( pr < r.priority )
            {
                pr = r.priority;
                jid.setResource( std::string( r.resname.cstr(), r.resname.get_length() ) );
            }
        }

        std::string sid = fm->requestFT( jid, fn, (long)finfo->filesize, gloox::EmptyString, gloox::EmptyString, gloox::EmptyString, gloox::EmptyString, gloox::SIProfileFT::FTTypeS5B | gloox::SIProfileFT::FTTypeIBB );
        if (sid.empty())
        {
            hf->file_control( finfo->utag, FIC_DISCONNECT ); // put it to send queue: assume transfer broken
            return;
        }
        /*transmitting_file_s *f =*/ new transmitting_file_s( jid, cd->cid, finfo->utag, sid, finfo->filesize, asptr( finfo->filename, finfo->filename_len ) ); // not memleak
    }

}

void xmpp::file_accept(u64 utag, u64 /*offset*/)
{
    for ( incoming_file_s *f = if_first; f; f = f->next )
        if ( f->utag == utag )
        {
            fm->acceptFT( f->jid, f->fileid, bytestreamproxy.empty() ? gloox::SIProfileFT::FTTypeIBB : gloox::SIProfileFT::FTTypeS5B );
            break;
        }
}

void xmpp::file_control(u64 utag, file_control_e fctl)
{
    for ( transmitting_file_s *f = of_first; f; f = f->next )
        if ( f->utag == utag )
        {
            switch ( fctl )
            {
            case FIC_UNPAUSE:
                break;
            case FIC_PAUSE:
                break;
            case FIC_BREAK:
                if (f->bs) fm->cancel( f->bs );
                f->die();
                break;
            }
            return;
        }
    for ( incoming_file_s *f = if_first; f; f = f->next )
        if ( f->utag == utag )
        {
            contact_descriptor_s *cd = find( f->cid );
            if ( cd == nullptr || cd == first )
                return;

            switch ( fctl ) //-V719
            {
            //case FIC_ACCEPT:
            //    fm->acceptFT( f->jid, f->fileid, bytestreamproxy.empty() ? gloox::SIProfileFT::FTTypeIBB : gloox::SIProfileFT::FTTypeS5B );
            case FIC_UNPAUSE:
                break;
            case FIC_PAUSE:
                break;
            case FIC_REJECT:
            case FIC_BREAK:
                fm->declineFT( f->jid, f->fileid, gloox::SIManager::RequestRejected );
                f->die();
                break;
            case FIC_DONE:
                f->die();
                break;
            }
            return;
        }

    if ( FIC_CHECK == fctl )
    {
        hf->file_control( utag, FIC_UNKNOWN );
    }

}
bool xmpp::file_portion(u64 utag, const file_portion_s *portion)
{
    for ( transmitting_file_s *f = of_first; f; f = f->next )
        if ( f->utag == utag )
            return f->portion( portion );

    return false;
}

void xmpp::add_groupchat(const char *groupaname, bool persistent)
{
}
void xmpp::ren_groupchat(int gid, const char *groupaname)
{
}
void xmpp::join_groupchat(int gid, int cid)
{
}
void xmpp::typing( int cid )
{
    if ( cid < 0 )
    {
        // group typing notification...
        return;
    }

    if ( contact_descriptor_s *c = find( cid ) )
    {
        if ( c->st == CS_ONLINE )
        {
            if ( !self_typing_contact )
            {
                self_typing_contact = cid;
                c->chat_state( this, gloox::ChatStateComposing );
                self_typing_start_time = time_ms();
            } else if ( self_typing_contact == cid )
                self_typing_start_time = time_ms();
        }

    }
}
void xmpp::export_data()
{
}
void xmpp::logging_flags(unsigned int f)
{
}

#define FUNC0( rt, fn ) rt __stdcall static_##fn() { return cl.fn(); }
#define FUNC1( rt, fn, p0 ) rt __stdcall static_##fn(p0 pp0) { return cl.fn(pp0); }
#define FUNC2( rt, fn, p0, p1 ) rt __stdcall static_##fn(p0 pp0, p1 pp1) { return cl.fn(pp0, pp1); }
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0

proto_functions_s funcs =
{
#define FUNC0( rt, fn ) &static_##fn,
#define FUNC1( rt, fn, p0 ) &static_##fn,
#define FUNC2( rt, fn, p0, p1 ) &static_##fn,
    PROTO_FUNCTIONS
#undef FUNC2
#undef FUNC1
#undef FUNC0
};

proto_functions_s* __stdcall api_handshake(host_functions_s *hf_)
{
    cl.on_handshake(hf_);
    return &funcs;
}


void __stdcall api_getinfo( proto_info_s *info )
{
    sstr_t<1024> vers( "plugin: " SS( PLUGINVER ) ", gloox: " );
    vers.append( xsptr( gloox::GLOOX_VERSION ) );

#define  NL "\n"

    static const char *strings[ _is_count_ ] =
    {
        "xmp",
        "XMPP (Jabber)",
        "<b>XMPP</b> (Jabber)",
        vers.cstr(),
        HOME_SITE,
        "",
        "m 4.9325858,22.946568 c 0.5487349,21.4588 18.1769072,43.713547 39.3746742,56.129555 -4.846477,3.759294 -10.312893,6.622086 -16.264974,8.120824 -0.0076,0.429798 -0.0075,0.890126 -0.014,1.28315 7.580261,-1.02812 15.451876,-3.501307 21.728625,-6.455133 7.249049,3.523305 15.796858,6.304913 22.897411,6.959039 0.0048,-0.425597 -0.0072,-0.87415 -0.01398,-1.283087 C 66.365608,86.12091 60.638252,83.014858 55.600825,78.950147 76.72364,66.739459 94.126528,44.562328 94.679221,22.948901 85.860244,26.806022 75.627343,29.821243 67.342306,32.266499 l -0.0024,0.0024 c 0.04002,0.681554 0.06294,1.367504 0.06294,2.055287 0,12.771327 -6.49847,28.367452 -17.28445,39.519312 C 39.609264,62.728786 33.279549,47.415326 33.279549,34.82808 c 0,-0.688814 0.0212,-1.371406 0.06066,-2.052951 l 0,-0.0024 C 23.585914,30.325955 13.251707,26.176417 4.9325858,22.946568 Z",
        //"m 33.518146,3.1792645 c -3.285248,0.507244 -6.432568,1.52916 -9.50931,2.760236 -1.120341,0.448058 -2.224198,0.9472024 -3.309908,1.4726992 -0.404021,0.1955147 -1.021441,0.3505793 -1.346342,0.6643718 -0.278741,0.8251012 -0.280816,1.8364719 -0.364498,2.6103365 -0.229768,2.372675 -0.392734,4.81874 -0.05298,7.190092 0.109995,0.76627 0.348706,2.154756 1.290449,2.301636 1.44824,0.22657 2.572706,-1.555597 3.283376,-2.539683 1.659934,-2.298154 3.550395,-4.437013 6.272894,-5.464568 3.204424,-1.209719 6.872253,-1.495937 10.261755,-1.728439 9.512518,-0.6519475 19.909665,1.085992 26.916943,8.092417 4.823201,4.896238 7.428423,12.445751 8.336389,18.35125 0.375701,2.547394 0.633979,5.123686 0.05298,7.660349 -1.008357,4.401077 -3.652292,8.232241 -6.690278,11.493503 -2.07939,2.232099 -4.419602,4.250799 -6.854936,6.084807 -3.206279,2.049144 -6.064378,4.488624 -7.789727,7.630906 -2.504872,4.630455 -3.176585,10.202095 -2.439858,15.370566 0.151016,1.059462 0.286134,2.123899 0.526159,3.168865 0.07784,1.361283 1.076021,1.641539 2.275189,1.8755 0.603123,0.02915 1.004411,-0.43517 1.152322,-0.975901 0.253007,-0.926604 0.102558,-1.951529 -0.04409,-2.883583 -0.300337,-1.909186 -0.576024,-3.832299 -0.49097,-5.770268 0.156949,-3.578442 1.540827,-6.91262 4.02123,-9.506406 2.522461,-2.63772 5.931162,-4.160294 8.797902,-6.369901 5.777506,-4.453098 10.952088,-10.460478 12.93966,-17.610575 0.710372,-2.556714 0.944655,-5.266233 0.620235,-7.898398 -0.409946,-3.32938 -1.627754,-6.656034 -2.7749,-9.791498 C 75.101901,19.812349 69.103285,10.725872 59.579289,6.3542521 53.576801,3.9555254 47.89323,3.0101933 41.986304,2.7416612 39.16196,2.603517 36.107972,2.8248901 33.518146,3.1792645 Z m 1.43751,39.1303305 c -0.950496,0.0076 -1.895164,0.249997 -2.716094,0.749615 -1.206995,0.735129 -1.878983,2.018805 -2.257504,3.336344 -0.751405,2.616278 -0.404969,5.60016 0.120528,8.227717 0.167952,0.840319 0.340417,1.68252 0.576108,2.507408 0.08516,0.297611 0.123915,0.731362 0.317464,0.978824 0.337787,0.432815 1.163987,0.55833 1.669624,0.473276 1.517208,-0.254985 1.142254,-2.753271 1.090601,-3.815551 -0.121096,-2.48821 -0.854334,-4.903221 -0.144052,-7.378173 0.300809,-1.047226 0.87937,-2.225891 1.937128,-2.68082 0.954735,-0.41108 1.920568,0.09045 2.298721,1.022948 0.61215,1.510055 0.456801,3.255527 0.840697,4.826737 0.08078,0.819218 0.308483,1.402631 0.884726,1.984172 0.376256,0.185787 1.1893,-0.86563 1.557942,-1.170006 1.190436,-0.983812 2.415389,-2.035079 3.786096,-2.76315 1.483581,-0.543879 1.985584,-1.050082 2.786672,0.196926 0.774265,1.212727 0.0067,2.159656 -0.496798,3.31282 -0.651568,1.492656 -1.319327,2.969584 -2.263427,4.303408 -0.811725,1.147052 -1.920673,2.153914 -2.569138,3.406912 -0.417477,0.806824 -0.654767,1.690991 -0.482112,2.601405 0.140001,0.736254 0.709998,1.180831 1.460937,1.00827 1.393535,-0.714344 2.128964,-2.336011 2.933638,-3.468633 1.098312,-1.53988 2.395063,-2.980219 3.31865,-4.635546 1.113271,-1.99565 1.650339,-4.252596 1.649024,-6.525712 -7.47e-4,-1.857532 -0.675754,-4.698868 -2.83964,-5.088307 -2.426297,-0.08378 -4.505066,1.424522 -5.952518,3.007147 -0.569439,0.628813 -1.079597,1.4171 -1.754878,1.934212 -0.03288,-4.246051 -2.432987,-6.320012 -5.75268,-6.352309 z M 24.094174,64.54687 c -2.141593,0.334 -2.623806,2.11738 -2.375212,3.462785 0.439211,2.14817 2.129633,3.786182 3.80378,5.076537 4.28929,3.30614 9.828476,5.092537 15.229525,4.856088 2.107719,-0.09235 4.406251,-0.440253 6.340464,-1.334571 1.037149,-0.479303 2.120881,-2.206323 0.708399,-2.945399 -1.368747,-0.895518 -3.914364,0.60849 -5.676178,0.899496 -3.170559,0.509221 -6.599111,-0.214432 -9.224227,-2.092941 -2.123237,-1.518987 -3.838402,-3.521886 -5.458635,-5.546802 -1.259631,-1.167264 -1.714999,-2.549438 -3.347916,-2.375193 z m -4.150698,10.870214 c -0.843518,0.619199 0.08525,1.866464 0.529072,2.431001 1.286967,1.635286 2.979737,2.992255 4.670822,4.188803 4.714667,3.33484 10.05636,5.258985 15.799814,5.652654 2.486911,0.09793 4.654563,-0.132008 6.857851,-0.679047 1.561709,-0.730426 1.111673,-3.679771 -0.282178,-4.335768 -0.608669,-0.286133 -1.586917,0.09064 -2.225136,0.179241 -1.509487,0.210475 -3.023403,0.230989 -4.541454,0.194011 -3.978593,-0.09739 -8.214914,-1.323189 -11.68157,-3.256929 -1.362807,-0.760724 -2.615996,-1.686192 -3.786096,-2.716104 -0.711513,-0.626258 -1.440614,-1.650055 -2.363357,-1.966479 -1.049266,-0.221876 -2.118366,0.06119 -2.977768,0.308617 z m 2.492825,8.498153 c -0.04305,9.4e-5 -0.08582,9.47e-4 -0.129374,0.0057 -1.228392,0.26606 -1.815816,1.123654 -2.389794,2.013617 -0.964993,2.38707 1.606682,4.639965 3.38339,5.76736 1.977209,1.254128 4.13677,2.182797 6.33754,2.965914 4.96372,1.76664 10.786211,2.899198 15.940942,1.319793 1.532253,-0.469032 3.045412,-1.327601 4.159441,-2.486893 0.627489,-0.653073 1.059925,-1.642434 -0.09121,-2.060574 -0.68197,-0.247546 -1.509676,0.38295 -2.128233,0.632002 -1.46018,0.588816 -2.974096,1.03622 -4.541445,1.225721 -3.881543,0.05279 -7.592762,-0.919677 -10.973077,-2.163518 -2.0151,-0.756614 -3.60285,-1.659946 -5.126506,-2.839645 -0.981929,-1.104804 -1.722036,-2.397317 -2.774912,-3.448025 -0.426502,-0.425757 -1.0204,-0.933842 -1.666709,-0.93177 z",
        "",
        "",
        "",
        "JID",
        "f=png,jpg,gif" NL "s=8192" NL "a=32" NL "b=96",
        nullptr
    };

    info->strings = strings;

    info->priority = 500;
    info->indicator = 1000;
    info->features = PF_UNAUTHORIZED_CHAT | PF_UNAUTHORIZED_CONTACT | PF_AVATARS | PF_NEW_REQUIRES_LOGIN | PF_OFFLINE_MESSAGING | PF_AUTH_NICKNAME | PF_SEND_FILE; // | PF_IMPORT | PF_EXPORT | PF_AUDIO_CALLS | PF_VIDEO_CALLS | PF_GROUP_CHAT | PF_INVITE_NAME;
    info->connection_features = CF_PROXY_SUPPORT_HTTPS | CF_PROXY_SUPPORT_SOCKS5; // | CF_IPv6_OPTION | CF_UDP_OPTION | CF_SERVER_OPTION;

}



