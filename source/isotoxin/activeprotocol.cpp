#include "isotoxin.h"

//-V:w:807
//-V:flags:807

#define RECONNECT_TIMEOUT 5

#define ACTUAL_USER_NAME( d ) ( (d).data.user_name.is_empty() ? prf().username() : (d).data.user_name )
#define ACTUAL_STATUS_MSG( d ) ( (d).data.user_statusmsg.is_empty() ? prf().userstatus() : (d).data.user_statusmsg )

void configurable_s::set_password( const ts::asptr&p )
{
    ts::uint8 encpass[ 32 ];
    get_unique_machine_id( encpass, 32, SALT_PROTOPASS, true );
    password = encode_string_base64( encpass, p );
    crypto_zero( encpass, sizeof(encpass) );
}
bool configurable_s::get_password_decoded( ts::str_c& rslt ) const
{
    ts::uint8 encpass[ 32 ];
    get_unique_machine_id( encpass, 32, SALT_PROTOPASS, true );
    bool r = decode_string_base64( rslt, encpass, password );
    crypto_zero( encpass, sizeof( encpass ) );
    return r;
}

namespace
{
struct config_maker_s : public ipcw
{
    config_maker_s() : ipcw( AQ_SET_CONFIG )
    {
        *this << static_cast<ts::int32>(sizeof(ts::int32)); // sizeof data
        flags_offset = size();
        *this << static_cast<ts::uint32>(0); // flags
    }
    ts::aint flags_offset;
    ts::aint params_offset = -1;
    bool proto_stuff = false;

    ts::uint32 & flags()
    {
        return *( ts::uint32 * )( data() + flags_offset );
    }

    config_maker_s & par( const ts::asptr&field, const ts::asptr&value )
    {
        ASSERT( !proto_stuff );

        if (0 == (flags() & CFL_PARAMS) )
        {
            flags() = flags() | CFL_PARAMS;
            *this << static_cast<ts::int32>(0);
            params_offset = size();
        }

        ASSERT(params_offset == flags_offset + (ts::aint)sizeof(ts::int32) * 2);
        append_s(field);
        tappend<char>('=');
        append_s(value);
        tappend<char>('\n');
        *(ts::int32 *)(data() + flags_offset + sizeof(ts::int32)) = static_cast<ts::int32>(size() - params_offset);
        *(ts::int32 *)(data() + flags_offset - sizeof(ts::int32)) = static_cast<ts::int32>(size() - flags_offset);

        return *this;
    }

    config_maker_s & conf( const ts::blob_c&proto_cfg, bool is_native )
    {
        ASSERT( !proto_stuff );

        if ( is_native )
            flags() = flags() | CFL_NATIVE_DATA;

        params_offset = -1;

        append_buf( proto_cfg.data(), proto_cfg.size() );
        proto_stuff = true;

        *(ts::int32 *)(data() + flags_offset - sizeof(ts::int32)) = static_cast<ts::int32>(size() - flags_offset);

        return *this;
    }

    config_maker_s & configurable( const configurable_s &c, int features, int conn_features )
    {
        if ( 0 != ( conn_features & CF_PROXY_MASK ) )
        {
            par( CONSTASTR( CFGF_PROXY_TYPE ), ts::amake<int>( c.proxy.proxy_type ) );
            par( CONSTASTR( CFGF_PROXY_ADDR ), c.proxy.proxy_addr );
        }

        if ( 0 != ( conn_features & CF_SERVER_OPTION ) )
        {
            par( CONSTASTR( CFGF_SERVER_PORT ), ts::amake<int>( c.server_port ) );
        }

#define COPDEF( nnn, dv ) if ( 0 != ( conn_features & CF_##nnn ) ) \
        { \
            par(CONSTASTR(#nnn), (c.nnn ? CONSTASTR("1") : CONSTASTR("0"))); \
        }
        CONN_OPTIONS
#undef COPDEF

        if ( 0 != ( features & PF_NEW_REQUIRES_LOGIN ) )
        {
            par( CONSTASTR( CFGF_LOGIN ), c.login );

            ts::str_c pwd;
            if (c.get_password_decoded( pwd ))
                par( CONSTASTR( CFGF_PASSWORD ), pwd );
        }

        return *this;
    }
};
}

active_protocol_c::active_protocol_c(int id, const active_protocol_data_s &pd):id(id), lastconfig(ts::Time::past())
{
    auto w = syncdata.lock_write();
    w().data = pd;
    w().data.config.set_size(pd.config.size()); // copy content due race condition

    if (w().data.sort_factor == 0)
    {
        w().data.sort_factor = id;
        for(; nullptr != prf().find_ap([&]( const active_protocol_c &oap )->bool {
            if (&oap == this) return false;
            return oap.sort_factor() == w().data.sort_factor;
        }) ; ++w().data.sort_factor );
    }
    g_app->F_PROTOSORTCHANGED(true);

    w().manual_cos = (contact_online_state_e)prf().manual_cos();

}

active_protocol_c::~active_protocol_c()
{
    save_config(true);
    stop_and_die(true);
    if (gui)
    {
        gui->delete_event(DELEGATE(this,check_die));
        gui->delete_event(DELEGATE(this,check_save));
    }
}

void active_protocol_c::run()
{
    if (g_app->F_READONLY_MODE() && !g_app->F_READONLY_MODE_WARN())
        return; // no warn - no job

    auto r = syncdata.lock_read();
    if (r().flags.is(F_WORKER|F_WORKER_STOP)) return;
    if (r().flags.is(F_WORKER_STOPED))
        g_app->avcontacts().stop_all_av();
    r.unlock();

    ts::master().sys_start_thread( DELEGATE( this, worker ) );
}

void active_protocol_c::reset_data()
{
    auto w = syncdata.lock_write();
    w().data.config.clear();
    w().current_state = CR_OK;
    w().cbits = 0;

    config_maker_s c;
    c.configurable( w().data.configurable, get_features(), get_conn_features() );
    c.conf( w().data.config, false );

    ipcp->send( c );
}

void active_protocol_c::change_data( const ts::blob_c &b, bool is_native )
{
    auto w = syncdata.lock_write();
    w().data.config = b;
    w().data.config.set_size( b.size() ); // copy content due race condition
    w().current_state = CR_OK;
    w().cbits = 0;

    config_maker_s c;
    c.configurable( w().data.configurable, get_features(), get_conn_features() );
    c.conf( w().data.config, is_native );

    ipcp->send( c );
}

void active_protocol_c::setup_audio_fmt( ts::str_c& s )
{
    audio_fmt.sampleRate = 48000;
    audio_fmt.channels = 1;
    audio_fmt.bitsPerSample = 16;
    if (!s.is_empty())
    {
        ts::parse_values( s, [&]( const ts::pstr_c&k, const ts::pstr_c&v ) {
            if ( k.equals( CONSTASTR( "sr" ) ) ) audio_fmt.sampleRate = v.as_uint();
            else if ( k.equals( CONSTASTR( "ch" ) ) ) audio_fmt.channels = (short)v.as_uint();
            else if ( k.equals( CONSTASTR( "bps" ) ) ) audio_fmt.bitsPerSample = (short)v.as_uint();
        } );
        s = ts::str_c();
    }
}

void active_protocol_c::setup_avatar_restrictions( ts::str_c& s )
{
    ts::renew( arest );

    UNFINISHED( "gif means animated gif supported" );

    if ( !s.is_empty() )
    {
        ts::parse_values( s, [&]( const ts::pstr_c&k, const ts::pstr_c&v ) {
            if ( k.equals( CONSTASTR( "f" ) ) )
            {
                // gif means animated gif supported

            } else if ( k.equals( CONSTASTR( "s" ) ) )
                arest.maxsize = v.as_uint();
            else if ( k.equals( CONSTASTR( "a" ) ) )
                arest.minwh = v.as_uint();
            else if ( k.equals( CONSTASTR( "b" ) ) )
                arest.maxwh = v.as_uint();
        } );
        s = ts::str_c();
    }
}

bool active_protocol_c::cmdhandler(ipcr r)
{
    switch(r.header().cmd)
    {
    case HA_CMD_STATUS:
        {
            commands_e c = static_cast<commands_e>(r.get<int>());
            cmd_result_e s = static_cast<cmd_result_e>(r.get<int>());

            if (c == AQ_SET_PROTO)
            {
                if (s == CR_OK)
                {
                    priority = r.get<int>();
                    indicator = r.get<int>();
                    features = r.get<int>();
                    conn_features = r.get<int>();

                    int numstrings = r.get<int>();

                    auto w = syncdata.lock_write();
                    
                    w().current_state = CR_OK;
                    w().cbits = 0;

                    w().strings.clear();
                    for(;numstrings > 0; --numstrings )
                        w().strings.add(r.getastr());

                    ts::str_c emptystring;
                    setup_audio_fmt( w().strings.size() > IS_AUDIO_FMT ? w().strings.get( IS_AUDIO_FMT ) : emptystring );
                    setup_avatar_restrictions( w().strings.size() > IS_AVATAR_RESTRICTIOS ? w().strings.get( IS_AVATAR_RESTRICTIOS ) : emptystring );

                    {
                        ipcw uids(AQ_SET_USED_IDS);
                        g_app->getuap(static_cast<ts::uint16>(id), uids);
                        ipcp->send(uids);
                    }

                    config_maker_s cm;
                    cm.configurable( w().data.configurable, get_features(), get_conn_features() );
                    cm.par( CONSTASTR( CFGF_SETPROTO ), CONSTASTR("1") );
                    cm.conf( w().data.config, 0 != (w().data.options & active_protocol_data_s::O_CONFIG_NATIVE) );

                    ipcp->send( cm );

                } else
                {
                    return false;
                }
            } else if ( c == AQ_SET_CONFIG )
            {
                auto w = syncdata.lock_write();

                ipcp->send( ipcw( AQ_SET_NAME ) << ACTUAL_USER_NAME( w() ) );
                ipcp->send( ipcw( AQ_SET_STATUSMSG ) << ACTUAL_STATUS_MSG( w() ) );
                ipcp->send( ipcw( AQ_SET_AVATAR ) << fit_ava(w().data.avatar) );
                if ( w().manual_cos != COS_ONLINE )
                    set_ostate( w().manual_cos );

                ipcp->send( ipcw( AQ_SIGNAL ) << static_cast<ts::int32>(APPS_INIT_DONE) );
                if ( CR_OK == s &&  0 != ( w().data.options & active_protocol_data_s::O_AUTOCONNECT ) )
                {
                    ipcp->send(ipcw(AQ_SIGNAL) << static_cast<ts::int32>(APPS_ONLINE));
                    w().flags.set( F_ONLINE_SWITCH );
                }

                w().flags.set(F_SET_PROTO_OK | F_CLEAR_ICON_CACHE);
                w.unlock();

                gmsg<ISOGM_PROTO_LOADED> *m = TSNEW( gmsg<ISOGM_PROTO_LOADED>, id );
                m->send_to_main_thread();
                
                if ( s != CR_OK )
                {
                    indicator = 0;
                    auto ww = syncdata.lock_write();
                    ww().reconnect_in = s == CR_NETWORK_ERROR ? RECONNECT_TIMEOUT : 0;
                    ww().current_state = s;
                    goto we_shoud_broadcast_result;
                }

            } else if ( c == AQ_SIGNAL )
            {
                indicator = 0;
                auto ww = syncdata.lock_write();
                ww().reconnect_in = s == CR_NETWORK_ERROR ? RECONNECT_TIMEOUT : 0;
                ww().current_state = s;
                goto we_shoud_broadcast_result;

            } else
            {
            we_shoud_broadcast_result:
                DMSG( "cmd status: " << static_cast<int>(c) << static_cast<int>(s));
                gmsg<ISOGM_CMD_RESULT> *m = TSNEW(gmsg<ISOGM_CMD_RESULT>, id, c, s);
                m->send_to_main_thread();
            }
        }
        break;
    case HA_CONNECTION_BITS:
        {
            auto w = syncdata.lock_write();
            w().cbits = r.get<int>();
        }
        break;
    case HQ_UPDATE_CONTACT:
        {
            gmsg<ISOGM_UPDATE_CONTACT> *m = TSNEW( gmsg<ISOGM_UPDATE_CONTACT> );

            m->key = r.get<contact_id_s>();
            m->apid = static_cast<ts::uint16>(id);
            m->mask = r.get<int>();
            m->pubid = r.getastr();
            m->name = r.getastr();
            m->statusmsg = r.getastr();
            m->avatar_tag = r.get<int>();
            m->state = static_cast<contact_state_e>(r.get<int>());
            m->ostate = static_cast<contact_online_state_e>(r.get<int>());
            m->gender = static_cast<contact_gender_e>(r.get<int>());
            m->grants = r.get<int>();

            g_app->setuap(static_cast<ts::uint16>(id), m->key.id);

            if (0 != (m->mask & CDM_MEMBERS)) // conference members
            {
                ASSERT( m->key.is_conference() );

                int cnt = r.get<int>();
                m->members.set_count(cnt);
                for(int i=0;i<cnt;++i)
                    m->members.set( i, r.get<contact_id_s>() );
            }

            if (0 != (m->mask & CDM_DETAILS))
                m->details = r.getastr();

            if (0 != (m->mask & CDM_DATA))
            {
                int dsz;
                const void *data = r.get_data(dsz);
                m->pdata.set_size(dsz);
                memcpy(m->pdata.data(), data, dsz);
            }


            m->send_to_main_thread();
        }
        break;
    case HQ_MESSAGE:
        {
            gmsg<ISOGM_INCOMING_MESSAGE> *m = TSNEW(gmsg<ISOGM_INCOMING_MESSAGE>);
            m->gid = r.get<contact_id_s>();
            m->cid = r.get<contact_id_s>();
            m->apid = static_cast<ts::uint16>(id);
            m->mt = static_cast<message_type_app_e>(r.get<int>());
            m->create_time = r.get<uint64>();
            m->msgutf8 = r.getastr();

            m->send_to_main_thread();
        }
        break;
    case HQ_SAVE:
        syncdata.lock_write()().flags.set(F_SAVE_REQUEST);
        break;
    case HA_DELIVERED:
        {
            gmsg<ISOGM_DELIVERED> *m = TSNEW(gmsg<ISOGM_DELIVERED>, r.get<uint64>());
            m->send_to_main_thread();
        }
        break;
    case HA_CONFIG:
        {
            int sz;
            if (const void *d = r.get_data(sz))
            {

                ts::uint8 hash[BLAKE2B_HASH_SIZE_SMALL];
                BLAKE2B( hash, d, sz );

                const void *hashr = r.read_data( BLAKE2B_HASH_SIZE_SMALL );
                if (hashr && 0 == memcmp( hashr, hash, sizeof( hash ) ))
                {
                    auto w = syncdata.lock_write();
                    w().data.config.clear();
                    w().data.config.append_buf(d, sz);
                    RESETFLAG( w().data.options, active_protocol_data_s::O_CONFIG_NATIVE ); // assume config from proto is never native

                    w().flags.set(F_CONFIG_OK|F_CONFIG_UPDATED);
                    lastconfig = ts::Time::current();
                } else
                {
                    syncdata.lock_write()().flags.set(F_CONFIG_FAIL);
                }
            }
        }
        break;
    case HA_CONFIGURABLE:
        {
            auto w = syncdata.lock_write();
            w().flags.set(F_CONFIGURABLE_RCVD);
            if (w().data.configurable.initialized)
            {
                configurable_s oldc = std::move(w().data.configurable);
                w.unlock();
                set_configurable(oldc,true);
            } else
            {
                int n = r.get<int>();
                for (int i = 0; i < n; ++i)
                {
                    ts::str_c f = r.getastr();
                    ts::str_c v = r.getastr();
                    if (f.equals(CONSTASTR(CFGF_PROXY_TYPE)))
                        w().data.configurable.proxy.proxy_type = v.as_int();
                    else if (f.equals(CONSTASTR(CFGF_PROXY_ADDR)))
                        w().data.configurable.proxy.proxy_addr = v;
                    else if (f.equals(CONSTASTR(CFGF_PROXY_ADDR)))
                        w().data.configurable.proxy.proxy_addr = v;
                    else if (f.equals(CONSTASTR(CFGF_SERVER_PORT)))
                        w().data.configurable.server_port = v.as_int();

#define COPDEF( nnn, dv ) else if (f.equals(CONSTASTR(#nnn))) { w().data.configurable.nnn = v.as_int() != 0; }
        CONN_OPTIONS
#undef COPDEF
                    w().data.configurable.initialized = true;
                }
            }
        }
        break;
    case HQ_AUDIO:
        {
            contact_id_s gid = r.get<contact_id_s>();
            contact_id_s cid = r.get<contact_id_s>();
            s3::Format fmt;
            fmt.sampleRate = r.get<int>();
            fmt.channels = r.get<short>();
            fmt.bitsPerSample = r.get<short>();
            int dsz;
            const void *data = r.get_data(dsz);
            uint64 msmonotonic = r.get<uint64>();
#ifdef _DEBUG
            amsmonotonic = msmonotonic;
#endif // _DEBUG

            if (av_contact_s *avc = g_app->avcontacts().find_inprogress( contact_key_s() | contact_key_s(gid, cid, id) ))
                avc->add_audio( msmonotonic, fmt, data, dsz );
        }
        break;
    case HQ_STREAM_OPTIONS:
        {
            contact_id_s gid = r.get<contact_id_s>();
            contact_id_s cid = r.get<contact_id_s>();
            int so = r.get<int>();
            ts::ivec2 sosz;
            sosz.x = r.get<int>();
            sosz.y = r.get<int>();
            gmsg<ISOGM_PEER_STREAM_OPTIONS> *m = TSNEW(gmsg<ISOGM_PEER_STREAM_OPTIONS>, (contact_key_s() | contact_key_s(gid, cid, id)), so, sosz);
            m->send_to_main_thread();

        }
        break;
    case HQ_VIDEO:
        {
            ASSERT( r.sz < 0, "HQ_VIDEO must be xchg buffer" );
            spinlock::auto_simple_lock l(lbsync);
            locked_bufs.add((data_header_s *)r.d);
            l.unlock();
            incoming_video_frame_s *f = (incoming_video_frame_s *)(r.d + sizeof(data_header_s));
#ifdef _DEBUG
            vmsmonotonic = f->msmonotonic;
#endif // _DEBUG
            TSNEW( video_frame_decoder_c, this, f ); // not memory leak!
        }
        break;
    case AQ_CONTROL_FILE:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->i_utag = r.get<uint64>();
            m->fctl = static_cast<file_control_e>(r.get<int>());
            m->send_to_main_thread();

        }
        break;
    case HQ_INCOMING_FILE:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->sender = contact_key_s( r.get<contact_id_s>(), id );
            m->i_utag = r.get<uint64>();
            m->filesize = r.get<uint64>();
            m->filename.set_as_utf8(r.getastr());

            fix_path( m->filename, FNO_MAKECORRECTNAME );

            m->send_to_main_thread();

        }
        break;
    case HQ_QUERY_FILE_PORTION:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->i_utag = r.get<uint64>();
            m->offset = r.get<uint64>() << 20;
            m->filesize = FILE_TRANSFER_CHUNK;

            m->send_to_main_thread();
        }
        break;
    case HQ_FILE_PORTION:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);

            ASSERT(r.sz < 0, "HQ_FILE_PORTION must be xchg buffer");

            struct fd_s
            {
                uint64 tag;
                uint64 offset;
                int size;
            };

            fd_s *f = (fd_s *)(r.d + sizeof(data_header_s));

            m->i_utag = f->tag;
            m->offset = f->offset;
            m->data.set_size(f->size);
            memcpy(m->data.data(), f + 1, f->size);

            m->send_to_main_thread();

            if (ipcp)
                ipcp->junct.unlock_buffer(f);
        }
        break;
    case HQ_AVATAR_DATA:
        {
            gmsg<ISOGM_AVATAR> *m = TSNEW(gmsg<ISOGM_AVATAR>);
            m->contact = contact_key_s( r.get<contact_id_s>(), id );
            m->tag = r.get<int>();
            int dsz;
            const void *data = r.get_data(dsz);
            m->data.set_size(dsz);
            memcpy(m->data.data(), data, dsz);

            m->send_to_main_thread();
        }
        break;
    case HQ_EXPORT_DATA:
        {
            int dsz;
            const void *data = r.get_data(dsz);
            gmsg<ISOGM_EXPORT_PROTO_DATA> *m = TSNEW(gmsg<ISOGM_EXPORT_PROTO_DATA>, id);
            m->buf.set_size(dsz);
            memcpy(m->buf.data(), data, dsz);

            m->send_to_main_thread();
        }
        break;
    case HQ_TYPING:
        {
            gmsg<ISOGM_TYPING> *m = TSNEW(gmsg<ISOGM_TYPING>);
            contact_id_s gid = r.get<contact_id_s>();
            contact_id_s cid = r.get<contact_id_s>();
            m->contact = contact_key_s( gid, cid, id );
            m->send_to_main_thread();
        }
        break;
    case HQ_TELEMETRY:
        {
            int tlmi = r.get<int>();
            if ( tlmi >= 0 && tlmi < TLM_COUNT)
            {
                int sz = 0;
                const tlm_data_s *d = (const tlm_data_s *)r.get_data(sz);
                if ( sz == sizeof( tlm_data_s ) )
                    tlms[tlmi].newdata( d );
            }
        }
        break;
    }

    return true;
}

void active_protocol_c::tlm_statistic_s::newdata( const tlm_data_s *d, bool full )
{
    if ( full )
    {
        newdata( d, false );
        for ( tlm_statistic_s * i = next; i; i = i->next )
            if ( i->uid == d->uid )
            {
                i->newdata( d, false );
                return;
            }
        tlm_statistic_s *ns = TSNEW( tlm_statistic_s );
        ns->uid = d->uid;
        ns->newdata( d, false );
        ns->next = next;

        next = ns; // final action due multi threaded
    } else
    {
        ts::Time c = ts::Time::current();
        accum += d->sz;
        accumcur += d->sz;
        int dt = ( c - last_update );
        if ( dt >= 1000 )
        {
            float clmp = 1000.0f / dt;
            accumps = (uint64)(accumcur * clmp);
            accumcur = 0;
            last_update = c;
        }
    }
}

void active_protocol_c::unlock_video_frame( incoming_video_frame_s *f )
{
    if (ipcp)
    {
        void *dh = ((char *)f) - sizeof(data_header_s);
        ipcp->junct.unlock_buffer(dh);

        SIMPLELOCK(lbsync);
        locked_bufs.find_remove_fast((data_header_s *)dh);
    }
}

void active_protocol_c::once_per_5sec_tick()
{
    if (ipcp)
        ipcp->junct.cleanup_buffers();
}

bool active_protocol_c::tick()
{
    if (syncdata.lock_read()().flags.is(F_WORKER_STOP))
        return false;

    return true;
}

void active_protocol_c::worker()
{
    MEMT( MEMT_AP );

    ts::str_c ipcname(CONSTASTR("isotoxin_ap_"));
    uint64 utg = random64();
    ipcname.append_as_hex(&utg, sizeof(utg)).append_char('_');

    auto ww = syncdata.lock_write();
    ww().flags.set(F_WORKER);
    ww().flags.clear(F_WORKER_STOPED|F_WORKER_STOP);
    ipcname.append( ww().data.tag );
    ww.unlock();

    // ipc allocated at stack
    isotoxin_ipc_s ipcs(ipcname, DELEGATE(this,cmdhandler));
    if (ipcs.ipc_ok)
    {
        ipcp = &ipcs;
        push_debug_settings();
        ipcp->send(ipcw(AQ_SET_PROTO) << syncdata.lock_read()().data.tag);
        ipcp->wait_loop(DELEGATE(this, tick));
        auto w = syncdata.lock_write();
        ipcp = nullptr;
        bool stop_signal = w().flags.is(F_WORKER_STOP);
        w().flags.clear(F_WORKER);
        w().flags.set(F_WORKER_STOPED);
        w.unlock();

        if (!stop_signal)
            TSNEW( gmsg<ISOGM_PROTO_CRASHED>, id )->send_to_main_thread();

        SIMPLELOCK(lbsync);
        for(data_header_s *dh : locked_bufs)
            ipcs.junct.unlock_buffer(dh);

    } else
        syncdata.lock_write()().flags.clear(F_WORKER);
    
}

ts::uint32 active_protocol_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SL>&p )
{
    if (!p.saved)
        return 0;

    if (p.tabi == pt_active_protocol && p.pass == 0)
    {
        tableview_active_protocol_s &t = prf().get_table_active_protocol();
        bool dematerialization = false;
        if (id < 0)
        {
            if (auto *v = t.new2ins.find(id))
                id = v->value;
            else
                dematerialization = true;
        } else
        {
            auto *row = t.find<true>(id);
            dematerialization = row == nullptr /*|| FLAG(row->other.options, active_protocol_data_s::O_SUSPENDED)*/;
            if (!dematerialization)
            {
                auto w = syncdata.lock_write();
                w().data.name = row->other.name;
                w().data.user_name = row->other.user_name;
                w().data.user_statusmsg = row->other.user_statusmsg;
                w().data.options = row->other.options;

                if (row->other.configurable.initialized)
                    set_configurable(row->other.configurable);
            }
        }
        if (dematerialization)
        {
            int protoid = id;
            stop_and_die();
            contacts().nomore_proto(protoid);
            g_app->recreate_ctls(true, true);
        }
    }
    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<GM_UI_EVENT>&e)
{
    if (UE_THEMECHANGED == e.evt)
    {
        // self avatar must be recreated to fit new gui theme
        set_avatar( contacts().find_subself(id) );
        icons_cache.clear(); // FREE MEMORY
    }
    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<GM_HEARTBEAT>&)
{
    run();

    bool do_reconnect = false;
    if (auto sss = syncdata.lock_write(true))
    {
        if (sss().current_state == CR_NETWORK_ERROR && sss().reconnect_in > 0)
        {
            --sss().reconnect_in;
            if (sss().reconnect_in == 0)
                do_reconnect = true;
        }

        if (!sss().flags.is(F_CFGSAVE_CHECKER))
        {
            if (sss().flags.clearr(F_SAVE_REQUEST))
            {
                save_config(false);
            }
            else if (sss().flags.clearr(F_CONFIG_UPDATED))
            {
                if (sss().flags.is(F_CONFIG_OK))
                    save_config(sss().data.config, 0 != (sss().data.options & active_protocol_data_s::O_CONFIG_NATIVE));
            }
        }
    }

    bool is_online = false;

    // brackets to destruct r
    {
        auto r = syncdata.lock_read();
        cmd_result_e curstate = r().current_state;
        is_online = r().flags.is(F_CURRENT_ONLINE);

        if (r().flags.is(F_NEED_SEND_CONFIGURABLE))
        {
            countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;
            if (ipcp)
            {
                configurable_s c;
                c = r().data.configurable;
                r.unlock();
                send_configurable(c);
                r = syncdata.lock_read();
            }
        }


        bool is_ac = false;
        if (curstate != CR_OK && r().flags.is(F_ONLINE_SWITCH))
            goto to_offline;

        if (r().flags.is(F_SET_PROTO_OK))
        {
            is_ac = 0 != (r().data.options & active_protocol_data_s::O_AUTOCONNECT);
            if (is_ac)
                countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;

            if (r().flags.is(F_ONLINE_SWITCH))
            {
                countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;

                if (!is_ac)
                {
            to_offline:
                    r.unlock();
                    ipcp->send(ipcw(AQ_SIGNAL) << static_cast<ts::int32>(APPS_OFFLINE));
                    syncdata.lock_write()().flags.clear(F_ONLINE_SWITCH);
                }
            }
            else if (is_ac && curstate == CR_OK)
            {
                r.unlock();
                if (ipcp)
                {
                    ipcp->send(ipcw(AQ_SIGNAL) << static_cast<ts::int32>(APPS_ONLINE));
                    syncdata.lock_write()().flags.set(F_ONLINE_SWITCH);
                }
            }
        } else
        {
            countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;
        }
    }

    if (--countdown_unload < 0 && ipcp)
    {
        auto w = syncdata.lock_write();
        w().flags.set(F_WORKER_STOP);
        ipcp->something_happens();
    }

    if (is_online)
    {
        if (!typingsendcontact.is_empty() && (typingtime - ts::Time::current()) > 0)
        {
            // still typing
            ipcp->send(ipcw(AQ_TYPING) << typingsendcontact);
        } else
        {
            typingsendcontact.clear();
        }

    } else
    {
        typingsendcontact.clear();
    }

    if (do_reconnect)
        set_autoconnect(true);

    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_MESSAGE>&msg) // send message to other peer
{
    if ( msg.info ) return 0;

    if (msg.pass == 0 && msg.post.sender.is_self) // handle only self-to-other messages
    {
        if (msg.post.receiver.protoid != (unsigned)id)
            return 0;

        contact_c *target = contacts().find( msg.post.receiver );
        if (!target) return 0;

        if ( !is_current_online() )
            return 0; // protocol is offline, so there is no way to send message now

        if ( !msg.resend && g_app->present_undelivered_messages( target->get_historian()->getkey(), msg.post.utag ) )
            return 0; // do not send message now, if there are undelivered messages for this historian

        bool online = true;

        for(;;)
        {
            if (CS_INVITE_RECEIVE == target->get_state() || CS_INVITE_SEND == target->get_state() || CS_UNKNOWN == target->get_state() )
                if (0 != (get_features() & PF_UNAUTHORIZED_CHAT))
                    break;

            if (target->get_state() != CS_ONLINE)
            {
                if (0 == (get_features() & PF_OFFLINE_MESSAGING))
                    return 0;
                online = false;
            }
            break;
        }

        typingsendcontact.clear();
        ipcp->send( ipcw(AQ_MESSAGE ) << target->getkey().gidcid() << msg.post.utag << ((online && !msg.resend) ? 0 : msg.post.cr_time) << msg.post.message_utf8->cstr() );
    }
    return 0;
}

ts::str_c active_protocol_c::get_actual_username() const
{
    auto r = syncdata.lock_read();
    return ACTUAL_USER_NAME( r() );
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass == 0 && ipcp && (ch.protoid == 0 || ch.protoid == id))
    {
        switch (ch.sp)
        {
        case PP_USERNAME:
            if (ch.protoid == id)
                syncdata.lock_write()().data.user_name = ch.s;
            {
                auto r = syncdata.lock_read();
                ipcp->send( ipcw( AQ_SET_NAME ) << ACTUAL_USER_NAME( r() ) );
            }
            return GMRBIT_CALLAGAIN;
        case PP_USERSTATUSMSG:
            if (ch.protoid == id)
                syncdata.lock_write()().data.user_statusmsg = ch.s;

            {
                auto r = syncdata.lock_read();
                ipcp->send( ipcw( AQ_SET_STATUSMSG ) << ACTUAL_STATUS_MSG( r() ) );
            }
            return GMRBIT_CALLAGAIN;
        case PP_NETWORKNAME:
            if (ch.protoid == id)
                syncdata.lock_write()().data.name = ch.s;
            return GMRBIT_CALLAGAIN;
        case PP_ONLINESTATUS:
            if (contact_c *c = contacts().get_self().subget( contact_key_s(contact_id_s::make_self(), id) ))
                set_ostate(c->get_ostate());
            break;
        case PP_PROFILEOPTIONS:
            if (0 != (ch.bits & UIOPT_PROTOICONS) && !prf_options().is(UIOPT_PROTOICONS))
                icons_cache.clear(); // FREE MEMORY
            break;
        case PP_VIDEO_ENCODING_SETTINGS:
            apply_encoding_settings();
            break;
        case CFG_DEBUG:
            push_debug_settings();
            break;
        }
    }
    return 0;
}

void active_protocol_c::push_debug_settings()
{
    if (prf_options().is(OPTOPT_POWER_USER))
        ipcp->send(ipcw(AQ_DEBUG_SETTINGS) << cfg().debug());
    else
        ipcp->send(ipcw(AQ_DEBUG_SETTINGS) << ts::str_c());
}

const ts::bitmap_c &active_protocol_c::get_icon(int sz, icon_type_e icot)
{
    MEMT( MEMT_AP_ICON );

    auto w = syncdata.lock_write();
    if (w().flags.is(F_CLEAR_ICON_CACHE|F_DIP))
    {
        icons_cache.clear();
        w().flags.clear(F_CLEAR_ICON_CACHE);
    }
    w.unlock();

    for(const icon_s &icon : icons_cache)
        if (icon.icot == icot && icon.bmp->info().sz.x == sz)
            return *icon.bmp;

    auto r = syncdata.lock_read();

    const ts::bitmap_c *icon = &prepare_proto_icon( r().data.tag, r().getstr(IS_PROTO_ICON), sz, icot );
    icon_s &ic = icons_cache.add();
    ic.bmp = icon;
    ic.icot = icot;
    return *icon;
}

bool active_protocol_c::check_die(RID, GUIPARAM)
{
    auto r = syncdata.lock_read();
    if (r().flags.is(F_WORKER))
    {
        // worker still works. waiting again
        if (ipcp) ipcp->something_happens();
        r.unlock();
        DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, check_die), nullptr);
    } else
    {
        r.unlock();
        TSDEL( this ); // actual death
    }
    return true;
}
bool active_protocol_c::check_save(RID, GUIPARAM)
{
    auto ttt = syncdata.lock_write();
    ttt().flags.clear(F_CFGSAVE_CHECKER);
    if (!ttt().flags.is(F_CONFIG_OK))
    {
        // config still not received. waiting again
        ttt().flags.set(F_CFGSAVE_CHECKER);
        DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, check_save), nullptr);
    }
    else
    {
        ttt().flags.clear(F_CONFIG_UPDATED);
        save_config( ttt().data.config, 0 != (ttt().data.options & active_protocol_data_s::O_CONFIG_NATIVE) );
    }
    return true;
}

void active_protocol_c::save_config( const ts::blob_c &cfg_, bool native_config )
{
    tableview_active_protocol_s &t = prf().get_table_active_protocol();
    if (auto *r = t.find<true>(id))
    {
        if ( r->other.config != cfg_)
        {
            time_t n = ts::now();
            if ( (n-last_backup_time) > prf().backup_period() )
            {
                last_backup_time = n;
                tableview_backup_protocol_s &backup = prf().get_table_backup_protocol();
                auto &row = backup.getcreate(0);
                row.other.time = ts::now();
                row.other.tick = ts::Time::current().raw();
                row.other.protoid = id;
                row.other.config = r->other.config;
            }
        }

        r->other.config = cfg_;
        r->other.config.set_size(cfg_.size()); // copy content
        INITFLAG( r->other.options, active_protocol_data_s::O_CONFIG_NATIVE, native_config );
        r->changed();
        prf().changed();
    }
}

void active_protocol_c::set_sortfactor(int sf)
{
    syncdata.lock_write()().data.sort_factor = sf;
    g_app->F_PROTOSORTCHANGED(true);
}

void encode_lossy_png( ts::blob_c &buf, const ts::bitmap_c &bmp );

ts::blob_c active_protocol_c::gen_system_user_avatar()
{

    ts::blob_c b;
    b.load_from_text_file( CONSTWSTR( "proto-avatar.svg" ) );
    ts::str_c xmls( b.cstr() );
    xmls.replace_all( CONSTASTR("[proto]"), get_infostr(IS_PROTO_ICON) );

    ts::TSCOLOR c = GET_THEME_VALUE( state_online_color );
    if ( !is_current_online() )
        c = ts::GRAYSCALE( c );
    xmls.replace_all( CONSTASTR( "[color]" ), make_color(c) );
    xmls.replace_all( CONSTASTR( "[color-bg]" ), make_color( GET_THEME_VALUE( common_bg_color ) ) );

    ts::bitmap_c ava;
    if ( rsvg_svg_c *svgg = rsvg_svg_c::build_from_xml( xmls.str() ) )
    {
        ts::bitmap_c bmp;
        bmp.create_ARGB( svgg->size() );
        svgg->render( bmp.extbody() );
        TSDEL( svgg );

        ts::irect vr = bmp.calc_visible_rect();
        if ( vr.lt.x ) --vr.lt.x;
        if ( vr.lt.y ) --vr.lt.y;
        if ( vr.rb.x < bmp.info().sz.x ) ++vr.rb.x;
        if ( vr.rb.y < bmp.info().sz.y ) ++vr.rb.y;
        ava = bmp.extbody( vr );
    }
    encode_lossy_png( b, ava );
    return b;
}

void active_protocol_c::set_avatar(contact_c *c)
{
    auto r = syncdata.lock_read();
    c->set_avatar(r().data.avatar.data(),r().data.avatar.size(),1);
}

ts::blob_c active_protocol_c::fit_ava( const ts::blob_c&ava ) const
{
    if ( ava.size() == 0 || arest.minwh == 0 && arest.maxwh == 0 && ava.size() <= arest.maxsize )
        return ava;

    ts::bitmap_c bmp;
    /*ts::img_format_e fmt =*/ bmp.load_from_file( ava );

    if ( bmp.info().bytepp() != 4 )
    {
        ts::bitmap_c b_temp;
        b_temp = bmp.extbody();
        bmp = b_temp;
    }

    if ( bmp.info().sz < ts::ivec2( arest.minwh ) )
    {
        ts::ivec2 nsz( arest.minwh );
        ts::bitmap_c upbmp;
        upbmp.create_ARGB( nsz );
        upbmp.resize_from( bmp.extbody(), ts::FILTER_LANCZOS3 );
        ts::blob_c b;
        encode_lossy_png( b, upbmp );
        return b;
    }

    ts::blob_c b; ts::ivec2 nsz( bmp.info().sz );

    if ( bmp.info().sz > ts::ivec2( arest.maxwh ) )
    {
        nsz = ts::ivec2( arest.maxwh );
        ts::bitmap_c upbmp;
        upbmp.create_ARGB( nsz );
        upbmp.resize_from( bmp.extbody(), ts::FILTER_BOX_LANCZOS3 );
        encode_lossy_png( b, upbmp );
    } else
        b = ava;

    while ( b.size() > arest.maxsize )
    {
        nsz  -= 8;
        if ( nsz.x <= arest.minwh || nsz.y < arest.minwh )
            return ts::blob_c();

        ts::bitmap_c upbmp;
        upbmp.create_ARGB( nsz );
        upbmp.resize_from( bmp.extbody(), ts::FILTER_BOX_LANCZOS3 );
        encode_lossy_png( b, upbmp );
    }

    return b;
}

void active_protocol_c::set_avatar( const ts::blob_c &ava )
{
    auto w = syncdata.lock_write();
    w().data.avatar = ava;
    w().data.avatar.set_size(ava.size()); // make copy due refcount not multithreaded
    w.unlock();

    tableview_active_protocol_s &t = prf().get_table_active_protocol();
    if (auto *r = t.find<true>(id))
    {
        r->other.avatar = ava;
        r->changed();
        prf().changed();
    }

    ipcp->send(ipcw(AQ_SET_AVATAR) << fit_ava(ava));

    if (contact_c *c = contacts().find_subself(getid()))
        c->set_avatar(ava.data(), ava.size(), 1);

}

void active_protocol_c::set_ostate(contact_online_state_e _cos)
{
    ipcp->send( ipcw(AQ_OSTATE) << (int)_cos );
}

void active_protocol_c::save_config(bool wait)
{
    if (!ipcp) return;

    MEMT( MEMT_AP );

    ts::Time t = ts::Time::current();

    auto w = syncdata.lock_write();
    if (w().flags.is(F_CONFIG_OK))
        if ((t - lastconfig) < 1000) return;

    w().flags.clear(F_CONFIG_OK|F_CONFIG_FAIL|F_SAVE_REQUEST|F_CONFIG_UPDATED);
    w().data.config.clear();
    w.unlock();

    if (!ipcp) return;
    ipcp->send( ipcw(AQ_SAVE_CONFIG) );
    DMSG("save request" << id);

    if (wait)
    {
        ts::sys_sleep(10);
        while( !syncdata.lock_read()().flags.is(F_CONFIG_OK|F_CONFIG_FAIL) )
        {
            ts::sys_sleep(10);
            ts::master().sys_idle();
            if (!syncdata.lock_read()().flags.is(F_WORKER))
                return;
        }
        check_save(RID(),nullptr);

    } else
    {
        syncdata.lock_write()().flags.set(F_CFGSAVE_CHECKER);
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,check_save), nullptr );
    }
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_CMD_RESULT>& r)
{
    if ( r.networkid == id )
    {
        if ( r.cmd == AQ_SAVE_CONFIG )
        {
            // save failed
            if ( r.rslt == CR_FUNCTION_NOT_FOUND )
                syncdata.lock_write()( ).flags.set( F_CONFIG_FAIL );
        } else if (r.cmd == AQ_SET_CONFIG)
        {
            if ( r.rslt == CR_ENCRYPTED )
            {
                dialog_msgbox_c::mb_error( TTT("Encrypted protocol data not supported",446) ).summon(true);
            }
        }
    }
    return 0;
}

void active_protocol_c::set_autoconnect( bool v )
{
    auto w = syncdata.lock_write();
    bool oldac = 0 != (w().data.options & active_protocol_data_s::O_AUTOCONNECT);
    if (oldac != v)
    {
        INITFLAG( w().data.options, active_protocol_data_s::O_AUTOCONNECT, v );

        auto row = prf().get_table_active_protocol().find<true>(id);
        if (CHECK(row))
        {
            INITFLAG(row->other.options, active_protocol_data_s::O_AUTOCONNECT, v);
            row->changed();
        }
    }
    w().current_state = CR_OK;
    w().cbits = 0;
    if (v)
    {
        w().flags.clear(F_WORKER_STOP);
        countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;
    }
}

void active_protocol_c::stop_and_die(bool wait_worker_end)
{
    auto w = syncdata.lock_write();
    if (w().flags.is(F_WORKER_STOP|F_DIP) || !w().flags.is(F_WORKER))
    {
        w().flags.set(F_WORKER_STOP|F_DIP);
        return;
    }
    w().flags.set(F_WORKER_STOP|F_DIP);
    if (ipcp) ipcp->something_happens();
    w.unlock();

    g_app->avcontacts().del( this );
        
    if (wait_worker_end)
    {
        auto lr = syncdata.lock_read();
        while( lr().flags.is(F_WORKER) )
        {
            if (ipcp) ipcp->something_happens();
            lr.unlock();
            ts::sys_sleep(0);
            lr = syncdata.lock_read();
        }
    } else
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,check_die), nullptr );
}

void active_protocol_c::del_contact( const contact_key_s &ck )
{
    ASSERT( ck.protoid == (unsigned)id );
    ASSERT( ck.temp_type != TCT_UNKNOWN_MEMBER );

    ipcp->send( ipcw(AQ_DEL_CONTACT) << ck.gidcid() );
}

void active_protocol_c::refresh_details( const contact_key_s &ck )
{
    ASSERT( ck.protoid == (unsigned)id );
    ASSERT( ck.temp_type != TCT_UNKNOWN_MEMBER );

    ipcp->send( ipcw( AQ_REFRESH_DETAILS ) << ck.gidcid() );
}

void active_protocol_c::resend_request( contact_id_s cid, const ts::str_c &msg_utf8 )
{
    ipcp->send( ipcw(AQ_ADD_CONTACT) << (char)1 << cid << msg_utf8 );
}

void active_protocol_c::add_contact( const ts::str_c& pub_id, const ts::str_c &msg_utf8 )
{
    ipcp->send( ipcw(AQ_ADD_CONTACT) << (char)0 << pub_id << msg_utf8 );
}

void active_protocol_c::add_contact( const ts::str_c& pub_id )
{
    ipcp->send( ipcw( AQ_ADD_CONTACT ) << (char)2 << pub_id );
}

void active_protocol_c::create_conference( const ts::str_c &conferencename, const ts::str_c &o )
{
    ipcp->send( ipcw(AQ_CREATE_CONFERENCE) << conferencename << o );
}

void active_protocol_c::del_conference( const ts::str_c &confa_id )
{
    ipcp->send( ipcw( AQ_DEL_CONFERENCE ) << confa_id );
}

void active_protocol_c::enter_conference( const ts::str_c &confa_id )
{
    ipcp->send( ipcw( AQ_ENTER_CONFERENCE ) << confa_id );
}

void active_protocol_c::leave_conference(contact_id_s gid, bool keep_leave )
{
    ipcp->send( ipcw( AQ_LEAVE_CONFERENCE ) << gid << static_cast<ts::int32>(keep_leave ? 1 : 0) );
}


void active_protocol_c::rename_conference(contact_id_s gid, const ts::str_c &conferencename)
{
    ipcp->send(ipcw(AQ_REN_CONFERENCE) << gid << conferencename);
}

void active_protocol_c::join_conference(contact_id_s gid, contact_id_s cid)
{
    ipcp->send(ipcw(AQ_JOIN_CONFERENCE) << gid << cid);
}


void active_protocol_c::accept(contact_id_s cid)
{
    ipcp->send( ipcw(AQ_ACCEPT) << cid );
}

void active_protocol_c::reject(contact_id_s cid)
{
    ipcp->send( ipcw(AQ_REJECT) << cid );
}

void active_protocol_c::accept_call(contact_id_s cid)
{
    ipcp->send(ipcw(AQ_ACCEPT_CALL) << cid);
}

void active_protocol_c::send_proto_data(contact_id_s cid, const ts::blob_c &pdata)
{
    ipcp->send(ipcw(AQ_CONTACT) << cid << pdata);
}

void active_protocol_c::send_video_frame(contact_id_s cid, const ts::bmpcore_exbody_s &eb, uint64 msmonotonic )
{
    if (!ipcp) return;
    isotoxin_ipc_s *ipcc = ipcp;

    struct inf_s
    {
        contact_id_s cid;
        int w;
        int h;
        int fmt;
        uint64 msmonotonic;
        uint64 padding;
    };
    static_assert( sizeof( inf_s ) == VIDEO_FRAME_HEADER_SIZE, "size!" );

    const ts::imgdesc_s &src_info = eb.info();

    int i420sz = src_info.sz.x * src_info.sz.y;
    i420sz += i420sz / 2;
    i420sz += sizeof(data_header_s) + sizeof(inf_s);

    if (data_header_s *dh = (data_header_s *)ipcc->junct.lock_buffer( i420sz ))
    {
        dh->cmd = AQ_VIDEO;
        inf_s *inf = (inf_s *)(dh + 1);
        inf->cid = cid;
        inf->w = src_info.sz.x;
        inf->h = src_info.sz.y;
        inf->fmt = VFMT_I420;
        inf->msmonotonic = msmonotonic;

        ts::uint8 *dst_y = ((ts::uint8 *)(dh + 1)) + sizeof(inf_s);
        int y_sz = src_info.sz.x * inf->h;
        ts::uint8 *dst_u = dst_y + y_sz;
        ts::uint8 *dst_v = dst_u + y_sz / 4;
        ts::img_helper_ARGB_to_i420(eb(), src_info.pitch, dst_y, src_info.sz.x, dst_u, src_info.sz.x / 2, dst_v, src_info.sz.x / 2, src_info.sz.x, src_info.sz.y);

        ipcc->junct.unlock_send_buffer(dh, i420sz);
    }
}

void active_protocol_c::send_audio(contact_id_s cid, const void *data, int size, uint64 timestamp)
{
    if (!ipcp) return;
    ipcp->send( ipcw( AQ_SEND_AUDIO ) << cid << data_block_s( data, size ) << timestamp );
}

void active_protocol_c::call(contact_id_s cid, int seconds, bool videocall)
{
    apply_encoding_settings();
    ipcp->send(ipcw(AQ_CALL) << cid << seconds << static_cast<ts::int32>(videocall ? 1 : 0));
}

void active_protocol_c::stop_call(contact_id_s cid)
{
    ipcp->send(ipcw(AQ_STOP_CALL) << cid);
}

void active_protocol_c::set_stream_options(contact_id_s cid, int so, const ts::ivec2 &vr)
{
    ipcp->send(ipcw(AQ_STREAM_OPTIONS) << cid << so << vr.x << vr.y);
}

void active_protocol_c::apply_encoding_settings()
{
    config_maker_s cm;

    cm.par( CONSTASTR( CFGF_VIDEO_CODEC ), ts::astrmap_c( prf().video_codec() ).get( get_tag(), ts::str_c() ) )
        .par( CONSTASTR( CFGF_VIDEO_BITRATE ), ts::amake<int>( prf().video_bitrate() ) )
        .par( CONSTASTR( CFGF_VIDEO_QUALITY ), ts::amake<int>( prf().video_enc_quality() ) );

    ipcp->send( cm );
}

void active_protocol_c::send_configurable(const configurable_s &c)
{
    ASSERT(ipcp);

    config_maker_s cm;
    cm.configurable(c, get_features(), get_conn_features());
    ipcp->send(cm);

    // do not save config now
    // protocol will initiate save procedure itself

    auto w = syncdata.lock_write();
    if (w().current_state == CR_NETWORK_ERROR || w().current_state == CR_AUTHENTICATIONFAILED)
    {
        w().current_state = CR_OK; // set to ok now. if error still exist, state will be set back to CR_NETWORK_ERROR by protocol
        w().cbits = 0;
        w().flags.set(F_ONLINE_SWITCH);
        w.unlock();
        ipcp->send(ipcw(AQ_SIGNAL) << static_cast<ts::int32>(APPS_ONLINE));
    }
    else
    {
        w().flags.clear(F_NEED_SEND_CONFIGURABLE);
    }
}

void active_protocol_c::set_configurable( const configurable_s &c, bool force_send )
{
    ASSERT(c.initialized);
    auto w = syncdata.lock_write();
    if (!w().flags.is(F_CONFIGURABLE_RCVD))
    {
        w().data.configurable = c; // just update, not send
        return;
    }

    configurable_s oldc = std::move(w().data.configurable);
    w().data.configurable = c;

    if (!check_netaddr(w().data.configurable.proxy.proxy_addr))
        w().data.configurable.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);

    bool cch = oldc != w().data.configurable;
    if ( cch )
    {
        tableview_active_protocol_s &t = prf().get_table_active_protocol();
        auto *row = t.find<true>( id );
        row->other.configurable = c;
        row->changed();
    }

    if ( force_send || cch )
    {
        if (ipcp)
        {
            oldc = w().data.configurable;
            w.unlock();
            send_configurable(oldc);
        }
        else
        {
            ASSERT(w.is_locked());
            w().flags.set(F_NEED_SEND_CONFIGURABLE);
            w().flags.clear(F_WORKER_STOP);
            countdown_unload = NUMTICKS_COUNTDOWN_UNLOAD;
        }
    }

}

void active_protocol_c::file_accept(uint64 utag, uint64 offset)
{
    ipcp->send(ipcw( AQ_ACCEPT_FILE ) << utag << offset);
}

void active_protocol_c::file_control(uint64 utag, file_control_e fctl)
{
    ipcp->send(ipcw(AQ_CONTROL_FILE) << utag << ((ts::int32)fctl));
}

void active_protocol_c::send_file(contact_id_s cid, uint64 utag, const ts::wstr_c &filename, uint64 filesize)
{
    ipcp->send(ipcw(AQ_FILE_SEND) << cid << utag << ts::to_utf8(filename) << filesize);
}

bool active_protocol_c::file_portion(uint64 utag, uint64 offset, const void *data, ts::aint sz)
{
    if (!ipcp) return false;
    isotoxin_ipc_s *ipcc = ipcp;

    struct fd_s
    {
        uint64 utag;
        uint64 offset;
        ts::int32 sz;
    };

    ts::aint bsz = sz;
    bsz += sizeof(data_header_s) + sizeof(fd_s);

    if (data_header_s *dh = (data_header_s *)ipcc->junct.lock_buffer(static_cast<int>(bsz)))
    {
        dh->cmd = AA_FILE_PORTION;
        fd_s *d = (fd_s *)(dh + 1);
        d->utag = utag;
        d->offset = offset;
        d->sz = (ts::int32)sz;

        memcpy( d + 1, data, sz );

        ipcc->junct.unlock_send_buffer(dh, static_cast<int>(bsz));
        return true;
    }

    return false;
}

void active_protocol_c::avatar_data_request(contact_id_s cid)
{
    ipcp->send(ipcw(AQ_GET_AVATAR_DATA) << cid);
}

void active_protocol_c::del_message(uint64 utag)
{
    ipcp->send(ipcw(AQ_DEL_MESSAGE) << utag);
}

void active_protocol_c::typing( const contact_key_s &ck )
{
    ASSERT( ck.temp_type != TCT_UNKNOWN_MEMBER );

    if (!prf_options().is( MSGOP_SEND_TYPING ) || ck.is_empty())
    {
        g_app->F_TYPING(false);
        typingsendcontact.clear();
        return;
    }

    ASSERT( ck.protoid == (unsigned)id );

    g_app->F_TYPING(true);
    contact_id_s gidcid = ck.gidcid();
    if (typingsendcontact != gidcid)
        ipcp->send(ipcw(AQ_TYPING) << gidcid);
    typingsendcontact = gidcid;
    typingtime = ts::Time::current() + 5000;
}

void active_protocol_c::export_data()
{
    ipcp->send( ipcw(AQ_EXPORT_DATA) );
}









void active_protocol_c::draw_telemtry( rectengine_c&e, const uint64& uid, const ts::irect& r, ts::uint32 tlmmask )
{
    const ts::wsptr tlmss[] =
    {
        // no need to localize these text due it just debug
        CONSTWSTR( "Audio data send: " ),
        CONSTWSTR( "Audio data recv: " ),
        CONSTWSTR( "Video data send: " ),
        CONSTWSTR( "Video data recv: " ),
        CONSTWSTR( "File data send: " ),
        CONSTWSTR( "File data recv: " ),
    };

    ts::wstr_c text;

    auto appendsz = [&]( uint64 sz )
    {
        if ( sz < 1024 )
        {
            text.append_as_num( sz ).append( CONSTWSTR( " bytes" ) );
            return;
        }
        if ( sz < 1024 * 1024 )
        {
            uint64 kb = sz / ( 1024 );
            uint64 ost = sz - kb * ( 1024 );
            uint64 pt = ost * 10 / ( 1024 );

            text.append_as_num( kb ).append_char( '.' ).append_as_num( pt ).append( CONSTWSTR( " kbytes" ) );
            return;
        }

        uint64 mb = sz / ( 1024 * 1024 );
        uint64 ost = sz - mb * ( 1024 * 1024 );
        uint64 pt = ost * 10 / ( 1024 * 1024 );

        text.append_as_num( mb ).append_char( '.' ).append_as_num( pt ).append( CONSTWSTR( " Mbytes" ) );;
    };

    ts::Time c = ts::Time::current();
    for( int i=0;i<TLM_COUNT;++i )
    {
        int delta = ( c - tlms[ i ].last_update );
        if ( delta > 10000 )
            continue;

        for ( tlm_statistic_s * s = tlms[i].next; s; s = s->next )
            if ( s->uid == uid )
            {
                if ( !text.is_empty() ) text.append( CONSTWSTR( "<br>" ) );
                text.append( tlmss[i] );
                appendsz( s->accumps );
                text.append( CONSTWSTR( " per sec, " ) );
                appendsz( s->accum );
                text.append( CONSTWSTR(" total") );
                if ( delta > 1500 )
                {
                    text.append( CONSTWSTR( ", no data " ) );
                    text.append_as_int( delta / 1000 );
                    text.append( CONSTWSTR( " sec" ) );
                }
            }
    }

#ifdef _DEBUG
    if ( amsmonotonic )
    {
        if ( !text.is_empty() ) text.append( CONSTWSTR( "<br>" ) );
        text.append( CONSTWSTR( "audio ms: " ) ).append_as_num( amsmonotonic );
    }
    if ( vmsmonotonic )
    {
        if ( !text.is_empty() ) text.append( CONSTWSTR( "<br>" ) );
        text.append( CONSTWSTR( "video ms: " ) ).append_as_num( vmsmonotonic );
    }
#endif // _DEBUG

    if (!text.is_empty())
    {
        ts::irect rr(r.lt, r.lt + gui->textsize( ts::g_default_text_font, text ));
        rr.intersect(r);
        draw_data_s &d = e.begin_draw();

        d.offset += rr.lt;
        d.size = rr.size();

        text_draw_params_s tdp;
        ts::TSCOLOR col( ts::ARGB(0,255,0) );
        tdp.forecolor = &col;
        e.draw( text, tdp );

        e.end_draw();

    }
}

