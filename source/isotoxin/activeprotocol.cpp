#include "isotoxin.h"

//-V:w:807
//-V:flags:807

active_protocol_c::active_protocol_c(int id, const active_protocol_data_s &pd):id(id), lastconfig(ts::Time::past())
{
    int dspflags = cfg().dsp_flags();

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
    g_app->F_PROTOSORTCHANGED = true;

    w().volume = cfg().vol_talk();
    w().dsp_flags = dspflags;
    w().manual_cos = (contact_online_state_e)prf().manual_cos();

    cvt.volume = cfg().vol_mic();
    cvt.filter_options.init(fmt_converter_s::FO_NOISE_REDUCTION, FLAG(dspflags, DSP_MIC_NOISE));
    cvt.filter_options.init(fmt_converter_s::FO_GAINER, FLAG(dspflags, DSP_MIC_AGC));

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
    if (g_app->F_READONLY_MODE && !g_app->F_READONLY_MODE_WARN)
        return; // no warn - no job

    if (syncdata.lock_read()().flags.is(F_WORKER)) return;

    CloseHandle(CreateThread(nullptr, 0, worker, this, 0, nullptr));
}

bool active_protocol_c::cmdhandler(ipcr r)
{
    switch(r.header().cmd)
    {
    case HA_CMD_STATUS:
        {
            commands_e c = (commands_e)r.get<int>();
            cmd_result_e s = (cmd_result_e)r.get<int>();

            if (c == AQ_SET_PROTO)
            {
                if (s == CR_OK)
                {
                    priority = r.get<int>();
                    features = r.get<int>();
                    conn_features = r.get<int>();
                    auto desc = r.getastr();
                    audio_fmt.sampleRate = r.get<int>();
                    audio_fmt.channels = r.get<short>();
                    audio_fmt.bitsPerSample = r.get<short>();

                    auto w = syncdata.lock_write();
                    
                    w().set_config_result = CR_OK;
                    w().description.set_as_utf8(desc);

                    int icondatasize;
                    const void *icondata = r.get_data(icondatasize);
                    if (icondatasize)
                    {
                        w().icon.set_size(icondatasize);
                        memcpy( w().icon.data(), icondata, icondatasize );
                    }

                    ipcp->send(ipcw(AQ_SET_CONFIG) << w().data.config);
                    ipcp->send(ipcw(AQ_SET_NAME) << (w().data.user_name.is_empty() ? prf().username() : w().data.user_name));
                    ipcp->send(ipcw(AQ_SET_STATUSMSG) << (w().data.user_statusmsg.is_empty() ? prf().userstatus() : w().data.user_statusmsg));
                    ipcp->send(ipcw(AQ_SET_AVATAR) << w().data.avatar);
                    if (w().manual_cos != COS_ONLINE)
                        set_ostate( w().manual_cos );

                    ipcp->send(ipcw(AQ_INIT_DONE));
                    if (0 != (w().data.options & active_protocol_data_s::O_AUTOCONNECT))
                    {
                        ipcp->send(ipcw(AQ_ONLINE));
                        w().flags.set(F_ONLINE_SWITCH);
                    }

                    w().flags.set(F_SET_PROTO_OK);
                    w.unlock();

                    gmsg<ISOGM_PROTO_LOADED> *m = TSNEW( gmsg<ISOGM_PROTO_LOADED>, id );
                    m->send_to_main_thread();

                } else
                {
                    return false;
                }
            } else if (c == AQ_SET_CONFIG)
            {
                if (s != CR_OK)
                {
                    syncdata.lock_write()().set_config_result = s;
                    goto we_shoud_broadcast_result;
                }

            } else
            {
            we_shoud_broadcast_result:
                DMSG( "cmd status: " << (int)c << (int)s );
                gmsg<ISOGM_CMD_RESULT> *m = TSNEW(gmsg<ISOGM_CMD_RESULT>, id, c, s);
                m->send_to_main_thread();
            }
        }
        break;
    case HQ_UPDATE_CONTACT:
        {
            gmsg<ISOGM_UPDATE_CONTACT> *m = TSNEW( gmsg<ISOGM_UPDATE_CONTACT> );
            m->key.protoid = id;
            m->key.contactid = r.get<int>();
            m->mask = r.get<int>();
            m->pubid = r.getastr();
            m->name = r.getastr();
            m->statusmsg = r.getastr();
            m->avatar_tag = r.get<int>();
            m->state = (contact_state_e)r.get<int>();
            m->ostate = (contact_online_state_e)r.get<int>();
            m->gender = (contact_gender_e)r.get<int>();

            /*int grants =*/ r.get<int>();

            if (0 != (m->mask & CDM_MEMBERS)) // groupchat members
            {
                ASSERT( m->key.is_group() );

                int cnt = r.get<int>();
                m->members.reserve(cnt);
                for(int i=0;i<cnt;++i)
                    m->members.add( r.get<int>() );
            }

            m->send_to_main_thread();
        }
        break;
    case HQ_MESSAGE:
        {
            gmsg<ISOGM_INCOMING_MESSAGE> *m = TSNEW(gmsg<ISOGM_INCOMING_MESSAGE>);
            m->groupchat.contactid = r.get<int>();
            m->groupchat.protoid = m->groupchat.contactid ? id : 0;
            m->sender.protoid = id;
            m->sender.contactid = r.get<int>();
            m->mt = (message_type_app_e)r.get<int>();
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
            auto w = syncdata.lock_write();
            int sz;
            if (const void *d = r.get_data(sz))
            {
                ts::md5_c md5;
                md5.update(d,sz);
                md5.done();

                const void *md5r = r.read_data(16);
                if (md5r && 0 == memcmp(md5r, md5.result(), 16))
                {
                    w().data.config.clear();
                    w().data.config.append_buf(d, sz);

                    w().flags.set(F_CONFIG_OK);
                    lastconfig = ts::Time::current();
                }
            }
        }
        break;
    case HA_CONFIGURABLE:
        {
            auto w = syncdata.lock_write();

            int n = r.get<int>();
            for(int i = 0;i<n;++i)
            {
                ts::str_c f = r.getastr();
                ts::str_c v = r.getastr();
                if ( f.equals(CONSTASTR(CFGF_PROXY_TYPE)) )
                    w().data.configurable.proxy.proxy_type = v.as_int();
                else if ( f.equals(CONSTASTR(CFGF_PROXY_ADDR)) )
                    w().data.configurable.proxy.proxy_addr = v;
                else if (f.equals(CONSTASTR(CFGF_PROXY_ADDR)))
                    w().data.configurable.proxy.proxy_addr = v;
                else if (f.equals(CONSTASTR(CFGF_SERVER_PORT)))
                    w().data.configurable.server_port = v.as_int();
                else if (f.equals(CONSTASTR(CFGF_UDP_ENABLE)))
                    w().data.configurable.udp_enable = v.as_int() != 0;
                
                w().data.configurable.initialized = true;
            }
            w().flags.set(F_CONFIGURABLE_RCVD);
        }
        break;
    case HQ_PLAY_AUDIO:
        {
            int gid = r.get<int>();
            contact_key_s ck;
            ck.protoid = id | (gid << 16); // assume 65536 unique groups max
            ck.contactid = r.get<int>();
            s3::Format fmt;
            fmt.sampleRate = r.get<int>();
            fmt.channels = r.get<short>();
            fmt.bitsPerSample = r.get<short>();
            int dsz;
            const void *data = r.get_data(dsz);

            auto r = syncdata.lock_read();
            float vol = r().volume;
            int dsp_flags = r().dsp_flags;
            r.unlock();

            int dspf = 0;
            if (0 != (dsp_flags & DSP_SPEAKERS_NOISE)) dspf |= fmt_converter_s::FO_NOISE_REDUCTION;
            if (0 != (dsp_flags & DSP_SPEAKERS_AGC)) dspf |= fmt_converter_s::FO_GAINER;

            g_app->mediasystem().play_voice(ts::ref_cast<uint64>(ck), fmt, data, dsz, vol, dspf);

            /*
            ts::Time t = ts::Time::current();
            static int cntc = 0;
            static ts::Time prevt = t;
            if ((t-prevt) >= 0)
            {
                prevt += 1000;
                DMSG("ccnt: " << cntc);
                cntc = 0;
        }
            ++cntc;
            */

        }
        break;
    case AQ_CONTROL_FILE:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->utag = r.get<uint64>();
            m->fctl = (file_control_e)r.get<int>();
            m->send_to_main_thread();

        }
        break;
    case HQ_INCOMING_FILE:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->sender.protoid = id;
            m->sender.contactid = r.get<int>();
            m->utag = r.get<uint64>();
            m->filesize = r.get<uint64>();
            m->filename.set_as_utf8(r.getastr());

            m->send_to_main_thread();

        }
        break;
    case HQ_QUERY_FILE_PORTION:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->utag = r.get<uint64>();
            m->offset = r.get<uint64>();
            m->filesize = r.get<int>();
            m->send_to_main_thread();
        }
        break;
    case HQ_FILE_PORTION:
        {
            gmsg<ISOGM_FILE> *m = TSNEW(gmsg<ISOGM_FILE>);
            m->utag = r.get<uint64>();
            m->offset = r.get<uint64>();
            int dsz;
            const void *data = r.get_data(dsz);
            m->data.set_size(dsz);
            memcpy(m->data.data(), data, dsz);

            m->send_to_main_thread();
        }
        break;
    case HQ_AVATAR_DATA:
        {
            gmsg<ISOGM_AVATAR> *m = TSNEW(gmsg<ISOGM_AVATAR>);
            m->contact.protoid = id;
            m->contact.contactid = r.get<int>();
            m->tag = r.get<int>();
            int dsz;
            const void *data = r.get_data(dsz);
            m->data.set_size(dsz);
            memcpy(m->data.data(), data, dsz);

            m->send_to_main_thread();

        }
        break;
    case HQ_TYPING:
        {
            gmsg<ISOGM_TYPING> *m = TSNEW(gmsg<ISOGM_TYPING>);
            m->contact.protoid = id;
            m->contact.contactid = r.get<int>();
            m->send_to_main_thread();
        }
        break;
    }

    return true;
}

bool active_protocol_c::tick()
{
    if (syncdata.lock_read()().flags.is(F_DIP))
        return false;

    return true;
}

void active_protocol_c::worker()
#if defined _DEBUG || defined _CRASH_HANDLER
{
    UNSTABLE_CODE_PROLOG
    worker_check();
    UNSTABLE_CODE_EPILOG
}

void active_protocol_c::worker_check()
#endif
{
    syncdata.lock_write()().flags.set(F_WORKER);

    // ipc allocated at stack
    isotoxin_ipc_s ipcs(ts::str_c(CONSTASTR("_isotoxin_ap_")).append_as_num(ts::uuid()), DELEGATE(this,cmdhandler));
    if (ipcs.ipc_ok)
    {
        ipcp = &ipcs;
        ipcp->send(ipcw(AQ_SET_PROTO) << syncdata.lock_read()().data.tag);
        ipcp->wait_loop(DELEGATE(this, tick));
        auto w = syncdata.lock_write();
        ipcp = nullptr;
        w().flags.clear(F_WORKER);
    } else
        syncdata.lock_write()().flags.clear(F_WORKER);
    
}

DWORD WINAPI active_protocol_c::worker(LPVOID ap)
{
    ts::tmpalloc_c tmp;
    ((active_protocol_c *)ap)->worker();
    return 0;
}

ts::uint32 active_protocol_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SAVED>&p )
{
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
            prf().dirty_sort();
            g_app->recreate_ctls();
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
    }
    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<GM_HEARTBEAT>&)
{
    run();

    if (syncdata.lock_write()().flags.clearr(F_SAVE_REQUEST))
        save_config(false);

    bool is_online = false;

    // brackets to destruct r
    {
        auto r = syncdata.lock_read();
        cmd_result_e curstate = r().set_config_result;
        is_online = r().flags.is(F_CURRENT_ONLINE);

        bool is_ac = false;
        if (curstate != CR_OK && r().flags.is(F_ONLINE_SWITCH))
            goto to_offline;

        if (r().flags.is(F_SET_PROTO_OK))
        {
            is_ac = 0 != (r().data.options & active_protocol_data_s::O_AUTOCONNECT);
            if (r().flags.is(F_ONLINE_SWITCH))
            {
                if (!is_ac)
                {
            to_offline:
                    r.unlock();
                    ipcp->send(ipcw(AQ_OFFLINE));
                    syncdata.lock_write()().flags.clear(F_ONLINE_SWITCH);
                }
            }
            else if (is_ac)
            {
                r.unlock();
                ipcp->send(ipcw(AQ_ONLINE));
                syncdata.lock_write()().flags.set(F_ONLINE_SWITCH);
            }
        }
    }

    if (is_online)
    {
        if (typingsendcontact && (typingtime - ts::Time::current()) > 0)
        {
            // still typing
            ipcp->send(ipcw(AQ_TYPING) << typingsendcontact);
        } else
        {
            typingsendcontact = 0;
        }

    } else
    {
        typingsendcontact = 0;
    }

    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_MESSAGE>&msg) // send message to other peer
{
    if (msg.pass == 0 && msg.post.sender.is_self()) // handle only self-to-other messages
    {
        if (msg.post.receiver.protoid != id)
            return 0;

        contact_c *target = contacts().find( msg.post.receiver );
        if (!target) return 0;


        for(;;)
        {
            if (CS_INVITE_RECEIVE == target->get_state() || CS_INVITE_SEND == target->get_state())
                if (0 != (get_features() & PF_UNAUTHORIZED_CHAT))
                    break;

            if (0 == (get_features() & PF_OFFLINE_MESSAGING))
                if (target->get_state() != CS_ONLINE)
                    return 0;

            break;
        }

        if (typingsendcontact == target->getkey().contactid)
            typingsendcontact = 0;

        ipcp->send( ipcw(AQ_MESSAGE ) << target->getkey().contactid << (int)MTA_MESSAGE << msg.post.utag << msg.post.message_utf8 );
    }
    return 0;
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass == 0 && ipcp && (ch.protoid == 0 || ch.protoid == id))
    {
        switch (ch.sp)
        {
        case PP_USERNAME:
            if (ch.protoid == id)
            {
                syncdata.lock_write()().data.user_name = ch.s;
                ipcp->send(ipcw(AQ_SET_NAME) << ch.s);
            } else if (syncdata.lock_read()().data.user_name.is_empty())
                ipcp->send(ipcw(AQ_SET_NAME) << ch.s);
            return GMRBIT_CALLAGAIN;
        case PP_USERSTATUSMSG:
            if (ch.protoid == id)
            {
                syncdata.lock_write()().data.user_statusmsg = ch.s;
                ipcp->send(ipcw(AQ_SET_STATUSMSG) << ch.s);
            } else if (syncdata.lock_read()().data.user_statusmsg.is_empty())
                ipcp->send(ipcw(AQ_SET_STATUSMSG) << ch.s);
            return GMRBIT_CALLAGAIN;
        case PP_NETWORKNAME:
            if (ch.protoid == id)
                syncdata.lock_write()().data.name = ch.s;
            return GMRBIT_CALLAGAIN;
        case PP_ONLINESTATUS:
            if (contact_c *c = contacts().get_self().subget( contact_key_s(0, id) ))
                set_ostate(c->get_ostate());
            break;
        case PP_PROFILEOPTIONS:
            if (!prf().get_options().is(UIOPT_PROTOICONS))
                icons_cache.clear();
            break;
        case CFG_TALKVOLUME:
            syncdata.lock_write()().volume = cfg().vol_talk();
            break;
        case CFG_MICVOLUME:
            cvt.volume = cfg().vol_mic();
            break;
        case CFG_DSPFLAGS:
            {
                int flags = cfg().dsp_flags();
                cvt.filter_options.init( fmt_converter_s::FO_NOISE_REDUCTION, FLAG(flags, DSP_MIC_NOISE) );
                cvt.filter_options.init( fmt_converter_s::FO_GAINER, FLAG(flags, DSP_MIC_AGC) );
                syncdata.lock_write()().dsp_flags = flags;
            }
            break;
        }
    }
    return 0;
}

const ts::drawable_bitmap_c &active_protocol_c::get_icon(int sz, icon_type_e icot)
{
    for(const icon_s &icon : icons_cache)
        if (icon.icot == icot && icon.bmp->info().sz.x == sz)
            return *icon.bmp;

    auto r = syncdata.lock_read();

    ts::drawable_bitmap_c *icon = prepare_proto_icon( r().data.tag, r().icon.data(), r().icon.size(), sz, icot );
    icon_s &ic = icons_cache.add();
    ic.bmp.reset(icon);
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
    auto ttt = syncdata.lock_read();
    if (!ttt().flags.is(F_CONFIG_OK))
    {
        // config still not received. waiting again
        DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, check_die), nullptr);
    }
    else
    {
        tableview_active_protocol_s &t = prf().get_table_active_protocol();
        if (auto *r = t.find<true>(id))
        {
            r->other.config = ttt().data.config;
            r->other.config.set_size(ttt().data.config.size()); // copy content
            r->changed();
            prf().changed();
        }

    }
    return true;
}

void active_protocol_c::set_sortfactor(int sf)
{
    syncdata.lock_write()().data.sort_factor = sf;
    g_app->F_PROTOSORTCHANGED = true;
}


void active_protocol_c::set_avatar(contact_c *c)
{
    auto r = syncdata.lock_read();
    c->set_avatar(r().data.avatar.data(),r().data.avatar.size(),1);
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

    ipcp->send(ipcw(AQ_SET_AVATAR) << ava);

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
    ts::Time t = ts::Time::current();

    auto w = syncdata.lock_write();

    if (w().flags.is(F_CONFIG_OK))
        if ((t - lastconfig) < 1000) return;

    w().flags.clear(F_CONFIG_OK|F_CONFIG_FAIL|F_SAVE_REQUEST);
    w().data.config.clear();
    w.unlock();

    DWORD ttt = t.raw();
try_again_save:
    if (!ipcp) return;
    ipcp->send( ipcw(AQ_SAVE_CONFIG) );
    DMSG("save request" << id);

    if (wait)
    {
        Sleep(10);
        while( !syncdata.lock_read()().flags.is(F_CONFIG_OK|F_CONFIG_FAIL) )
        {
            Sleep(100);
            sys_idle();
            DWORD ct = timeGetTime(); 
            if (((int)ct - (int)ttt) > 1000)
            {
                ttt = ct;
                goto try_again_save;
            }
        }
        check_save(RID(),nullptr);

    } else
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,check_save), nullptr );
}

ts::uint32 active_protocol_c::gm_handler(gmsg<ISOGM_CMD_RESULT>& r)
{
    if (r.cmd == AQ_SAVE_CONFIG)
    {
        // save failed
        if ( r.rslt == CR_FUNCTION_NOT_FOUND )
            syncdata.lock_write()().flags.set(F_CONFIG_FAIL);
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
}

void active_protocol_c::stop_and_die(bool wait_worker_end)
{
    auto w = syncdata.lock_write();
    if (w().flags.is(F_DIP) || !w().flags.is(F_WORKER))
    {
        w().flags.set(F_DIP);
        return;
    }
    w().flags.set(F_DIP);
    if (ipcp) ipcp->something_happens();
    w.unlock();
        
    if (wait_worker_end)
    {
        auto lr = syncdata.lock_read();
        while( lr().flags.is(F_WORKER) )
        {
            if (ipcp) ipcp->something_happens();
            lr.unlock();
            Sleep(0);
            lr = syncdata.lock_read();
        }
    } else
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,check_die), nullptr );
}

void active_protocol_c::del_contact(int cid)
{
    ipcp->send( ipcw(AQ_DEL_CONTACT) << cid );
}

void active_protocol_c::resend_request( int cid, const ts::str_c &msg_utf8 )
{
    ipcp->send( ipcw(AQ_ADD_CONTACT) << (char)1 << cid << msg_utf8 );
}

void active_protocol_c::add_contact( const ts::str_c& pub_id, const ts::str_c &msg_utf8 )
{
    ipcp->send( ipcw(AQ_ADD_CONTACT) << (char)0 << pub_id << msg_utf8 );
}

void active_protocol_c::add_group_chat( const ts::str_c &groupname, bool persistent )
{
    ipcp->send( ipcw(AQ_ADD_GROUPCHAT) << groupname << (persistent ? 1 : 0) );
}

void active_protocol_c::rename_group_chat(int gid, const ts::str_c &groupname)
{
    ipcp->send(ipcw(AQ_REN_GROUPCHAT) << gid << groupname);
}

void active_protocol_c::join_group_chat(int gid, int cid)
{
    ipcp->send(ipcw(AQ_JOIN_GROUPCHAT) << gid << cid);
}


void active_protocol_c::accept(int cid)
{
    ipcp->send( ipcw(AQ_ACCEPT) << cid );
}

void active_protocol_c::reject(int cid)
{
    ipcp->send( ipcw(AQ_REJECT) << cid );
}

void active_protocol_c::accept_call(int cid)
{
    ipcp->send(ipcw(AQ_ACCEPT_CALL) << cid);
}

void active_protocol_c::send_audio(int cid, const s3::Format &ifmt, const void *data, int size)
{
    struct s
    {
        int cid;
        isotoxin_ipc_s *ipcp;
        void send_audio(const s3::Format& ofmt, const void *data, int size)
        {
            ipcp->send(ipcw(AQ_SEND_AUDIO) << cid << data_block_s(data,size));
        }
    } ss = { cid, ipcp };
    
    cvt.ofmt = audio_fmt;
    cvt.acceptor = DELEGATE( &ss, send_audio );
    cvt.cvt(ifmt, data, size );

}

void active_protocol_c::call(int cid, int seconds)
{
    ipcp->send(ipcw(AQ_CALL) << cid << seconds);
}

void active_protocol_c::stop_call(int cid, stop_call_e sc)
{
    contact_key_s ck;
    ck.protoid = id;
    ck.contactid = cid;
    g_app->mediasystem().free_voice_channel(ts::ref_cast<uint64>(ck));

    ipcp->send(ipcw(AQ_STOP_CALL) << cid << ((char)sc));
}

void active_protocol_c::set_configurable( const configurable_s &c )
{
    ASSERT(c.initialized);

    auto w = syncdata.lock_write();
    if (!w().flags.is(F_CONFIGURABLE_RCVD)) return;

    configurable_s oldc = std::move(w().data.configurable);
    w().data.configurable = c;

    if (!check_netaddr(w().data.configurable.proxy.proxy_addr))
        w().data.configurable.proxy.proxy_addr = CONSTASTR(DEFAULT_PROXY);

    if (oldc != w().data.configurable)
    {
        if (ipcp)
        {
            oldc = w().data.configurable;
            w.unlock();

            ipcw s(AQ_CONFIGURABLE);
            s << (int)4;
            s << CONSTASTR( CFGF_PROXY_TYPE ) << ts::amake<int>(oldc.proxy.proxy_type);
            s << CONSTASTR( CFGF_PROXY_ADDR ) << oldc.proxy.proxy_addr;
            s << CONSTASTR( CFGF_SERVER_PORT ) << ts::amake<int>(oldc.server_port);
            s << CONSTASTR( CFGF_UDP_ENABLE ) << (oldc.udp_enable ? CONSTASTR("1") : CONSTASTR("0"));

            ipcp->send(s);
            // do not save config now
            // protocol will initiate save procedure itself
        }
    }

}

void active_protocol_c::file_resume(uint64 utag, uint64 offset)
{
    ipcp->send(ipcw(AQ_CONTROL_FILE) << utag << ((int)FIC_ACCEPT) << offset);
}

void active_protocol_c::file_control(uint64 utag, file_control_e fctl)
{
    ipcp->send(ipcw(AQ_CONTROL_FILE) << utag << ((int)fctl) << (uint64)0);
}

void active_protocol_c::send_file(int cid, uint64 utag, const ts::wstr_c &filename, uint64 filesize)
{
    ipcp->send(ipcw(AQ_FILE_SEND) << cid << utag << ts::to_utf8(filename) << filesize);
}

void active_protocol_c::file_portion(uint64 utag, uint64 offset, const void *data, int sz)
{
    ipcp->send(ipcw(AA_FILE_PORTION) << utag << offset << data_block_s(data, sz));
}

void active_protocol_c::avatar_data_request(int cid)
{
    ipcp->send(ipcw(AQ_GET_AVATAR_DATA) << cid);
}

void active_protocol_c::del_message(uint64 utag)
{
    ipcp->send(ipcw(AQ_DEL_MESSAGE) << utag);
}

void active_protocol_c::typing(int cid)
{
    if ( !prf().get_options().is(MSGOP_SEND_TYPING) )
        return;

    if (cid)
    {
        if (!typingsendcontact)
            ipcp->send(ipcw(AQ_TYPING) << cid);
        typingsendcontact = cid;
        typingtime = ts::Time::current() + 5000;
    } else
        typingsendcontact = 0;
}