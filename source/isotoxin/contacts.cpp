#include "isotoxin.h"

void avatar_s::load( const void *body, int size, int tag_ )
{
    alpha_pixels = false;
    ts::bitmap_c bmp;
    if (bmp.load_from_file(body, size))
    {
        if (tag_ == 0) ++tag; else tag = tag_;

        if (bmp.info().bytepp() != 4)
        {
            ts::bitmap_c bmp4;
            bmp.convert_24to32(bmp4);
            bmp = bmp4;
        }
        ts::ivec2 asz = parsevec2( gui->theme().conf().get_string(CONSTASTR("avatarsize")), ts::ivec2(32));
        if (bmp.info().sz != asz)
        {
            ts::bitmap_c bmprsz;

            if (bmp.info().sz.x != bmp.info().sz.y)
            {
                bmprsz.create_RGBA(ts::ivec2(ts::tmax(bmp.info().sz.x,bmp.info().sz.y)));
                bmprsz.fill(0);
                bmprsz.copy( (bmprsz.info().sz - bmp.info().sz) / 2, bmp.info().sz, bmp.extbody(), ts::ivec2(0) );
                bmp = bmprsz;
            }
            bmp.resize(bmprsz, asz, ts::FILTER_LANCZOS3);
            bmp = bmprsz;
        }

        if (bmp.has_alpha())
        {
            alpha_pixels = true;
        } else
        {
            // apply mask
            if (const ts::image_extbody_c *mask = gui->theme().get_image(CONSTASTR("avamask")))
            {
                if (mask->info().sz >>= bmp.info().sz)
                {
                    if (mask->info().sz > bmp.info().sz)
                    {
                        ts::bitmap_c bmplarger;
                        bmplarger.create_RGBA(ts::tmax( mask->info().sz, bmp.info().sz ));
                        bmplarger.fill(0);
                        bmplarger.copy( (bmplarger.info().sz - bmp.info().sz) / 2, bmp.info().sz, bmp.extbody(), ts::ivec2(0) );
                        bmp = bmplarger;
                    }
                    bmp.before_modify();
                    ts::ivec2 offs = (bmp.info().sz - mask->info().sz) / 2;
                    ts::img_helper_copy_components(bmp.body() + 3 + offs.x * bmp.info().bytepp() + offs.y * bmp.info().pitch, mask->body() + 3, bmp.info(offs), mask->info(), 1);
                }
            }
            if (const ts::image_extbody_c *avabase = gui->theme().get_image(CONSTASTR("avabase")))
            {
                if (avabase->info().sz >>= bmp.info().sz)
                {
                    ts::bitmap_c prepared;
                    prepared.create_RGBA(avabase->info().sz);
                    ts::ivec2 offs = (avabase->info().sz - bmp.info().sz) / 2;
                    prepared.alpha_blend(offs, bmp.extbody(), avabase->extbody());
                    bmp = prepared;
                }
            }
        }
    
        bool a = create_from_bitmap(bmp, false, true, !alpha_pixels);
        if (!alpha_pixels) alpha_pixels = a;
    }

}

ts::static_setup<contacts_c> contacts;

gmsg<ISOGM_MESSAGE>::gmsg(contact_c *sender, contact_c *receiver, message_type_app_e mt) :gmsgbase(ISOGM_MESSAGE), sender(sender), receiver(receiver)
{
    post.utag = prf().uniq_history_item_tag();
    post.time = 0; // initialized after history add
    post.type = mt;
    post.sender = sender->getkey();
    post.receiver = receiver->getkey();
}

uint64 uniq_history_item_tag()
{
    return prf().uniq_history_item_tag();
}


contact_c *get_historian(contact_c *sender, contact_c * receiver)
{
    contact_c *historian;
    if (sender->getkey().is_self() || receiver->getkey().is_group())
    {
        // from self to other
        historian = receiver;
    }
    else
    {
        historian = sender;
    }
    if (!historian->is_meta()) historian = historian->getmeta();
    return historian;
}

contact_c::contact_c( const contact_key_s &key ):key(key)
{
}
contact_c::contact_c()
{
}
contact_c::~contact_c()
{
    if (gui)
    {
        gui->delete_event( DELEGATE(this,flashing_proc) );
        gui->delete_event( DELEGATE(this,check_invite) );
    }
}

bool contact_c::is_protohit( bool strong )
{
    ASSERT( !is_multicontact() );

    if ( strong && opts.unmasked().is(F_PROTOHIT) )
    {
        if (nullptr == prf().ap(getkey().protoid))
            opts.unmasked().clear(F_PROTOHIT);
    }
        
    return opts.unmasked().is(F_PROTOHIT);
}

void contact_c::protohit( bool f )
{
    ASSERT( !is_multicontact() );

    if (f && nullptr == prf().ap(getkey().protoid))
        f = false;

    if (f != opts.unmasked().is(F_PROTOHIT))
    {
        opts.unmasked().init(F_PROTOHIT, f);
        
        if (!f && get_state() == CS_ONLINE)
            state = CS_OFFLINE;

    }

    // always update gui_item
    if (contact_c *m = getmeta())
        if (m->gui_item)
            m->gui_item->protohit();

}

void contact_c::reselect(bool scrollend)
{
    contact_c *h = get_historian();
    if (scrollend)
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            gmsg<ISOGM_SELECT_CONTACT>((contact_c *)param).send();
        DEFERRED_EXECUTION_BLOCK_END(h)
    } else
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            gmsg<ISOGM_SELECT_CONTACT>((contact_c *)param, false).send();
        DEFERRED_EXECUTION_BLOCK_END(h)
    }
}

ts::wstr_c contact_c::compile_pubid() const
{
    ASSERT(is_meta());
    ts::wstr_c x;
    for (contact_c *c : subcontacts)
        x.append('~', c->get_pubid_desc());
    return x;
}
ts::wstr_c contact_c::compile_name() const
{
    ASSERT(is_meta());
    contact_c *defc = subget_default();
    if (defc == nullptr) return ts::wstr_c();
    ts::wstr_c x(defc->get_name(false));
    for(contact_c *c : subcontacts)
        if (x.find_pos(c->get_name(false)) < 0)
            x.append_char('~').append(c->get_name(false));
    return x;
}

ts::wstr_c contact_c::compile_statusmsg() const
{
    ASSERT(is_meta());
    contact_c *defc = subget_default();
    if (defc == nullptr) return ts::wstr_c();
    ts::wstr_c x(defc->get_statusmsg(false));
    for (contact_c *c : subcontacts)
        if (x.find_pos(c->get_statusmsg(false)) < 0)
            x.append_char('~').append(c->get_statusmsg(false));
    return x;
}

contact_state_e contact_c::get_meta_state() const
{
    if (subcontacts.size() == 0) return CS_ROTTEN;
    contact_state_e rst = contact_state_check;
    for (contact_c *c : subcontacts)
        if (c->get_state() == CS_REJECTED) return CS_REJECTED;
        else if (c->get_state() == CS_INVITE_SEND) return CS_INVITE_SEND;
        else if (c->get_state() == CS_INVITE_RECEIVE) return CS_INVITE_RECEIVE;
        else if (c->get_state() != rst)
        {
            if (rst != contact_state_check)
                return contact_state_check;
            rst = c->get_state();
        }
    return rst;
}

contact_online_state_e contact_c::get_meta_ostate() const
{
    if (subcontacts.size() == 0) return COS_ONLINE;
    contact_online_state_e s = COS_ONLINE;
    for (contact_c *c : subcontacts)
        if (c->get_ostate() > s) s = c->get_ostate();
    return s;
}

contact_gender_e contact_c::get_meta_gender() const
{
    if (subcontacts.size() == 0) return CSEX_UNKNOWN;
    contact_gender_e rgender = CSEX_UNKNOWN;
    for (contact_c *c : subcontacts)
        if (c->get_gender() != rgender)
        {
            if (rgender != CSEX_UNKNOWN) return CSEX_UNKNOWN; 
            rgender = c->get_gender();
        }
    return rgender;
}

contact_c * contact_c::subget_for_send() const
{
    if (subcontacts.size() == 1) return subcontacts.get(0);
    contact_c *maxpriority = nullptr;
    int prior = 0;
    contact_state_e st = contact_state_check;
    for (contact_c *c : subcontacts)
    {
        if (c->options().is(contact_c::F_DEFALUT) && c->get_state() == CS_ONLINE) return c;

        if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
        {
            if (maxpriority == nullptr || (st != CS_ONLINE && c->get_state() == CS_ONLINE) || (st != CS_ONLINE && c->get_state() != CS_ONLINE && ap->get_priority() > prior))
            {
                maxpriority = c;
                prior = ap->get_priority();
                st = c->get_state();
            }
        }
    }
    return maxpriority;
}

contact_c * contact_c::subget_default() const
{
    if (subcontacts.size() == 1) return subcontacts.get(0);
    contact_c *maxpriority = nullptr;
    int prior = 0;
    for (contact_c *c : subcontacts)
    {
        if (c->options().is(contact_c::F_DEFALUT)) return c;

        if (active_protocol_c *ap = prf().ap( c->getkey().protoid ))
        {
            if (maxpriority == nullptr || ap->get_priority() > prior)
            {
                maxpriority = c;
                prior = ap->get_priority();
            }
        }
    }

    return maxpriority;
}

const avatar_s *contact_c::get_avatar() const
{
    if (!is_multicontact())
        return avatar.get();

    const avatar_s * r = nullptr;
    for (const contact_c *c : subcontacts)
    {
        if (c->get_options().is(contact_c::F_AVA_DEFAULT) && c->get_avatar())
            return c->get_avatar();
        if (c->get_avatar() && (c->get_options().is(contact_c::F_DEFALUT) || r == nullptr) )
            r = c->get_avatar();
    }
    return r;
}

bool contact_c::keep_history() const
{
    if (KCH_ALWAYS_KEEP == keeph) return true;
    if (KCH_NEVER_KEEP == keeph) return false;
    return prf().get_msg_options().is(MSGOP_KEEP_HISTORY);
}

void contact_c::del_history(uint64 utag)
{
    int cnt = history.size();
    for (int i = 0; i < cnt; ++i)
    {
        if (history.get(i).utag == utag)
        {
            history.remove_slow(i);
            break;
        }
    }
    prf().kill_history_item(utag);
}


void contact_c::make_time_unique(time_t &t)
{
    if (history.size() == 0 || t < history.get(0).time )
        prf().load_history(getkey()); // load whole history to correct uniquzate t

    for( const post_s &p : history )
        if (p.time == t)
            ++t;
        else if (p.time > t)
            break;
}

void contact_c::load_history(int n_last_items)
{
    ASSERT( get_historian() == this );

    time_t before = 0;
    if (history.size()) before = history.get(0).time;
    ts::tmp_tbuf_t<int> ids;
    prf().load_history( getkey(), before, n_last_items, ids );

    for(int id : ids)
    {
        auto * row = prf().get_table_history().find<true>(id);
        if (ASSERT(row && row->other.historian == getkey()))
        {
            if (MTA_RECV_FILE == row->other.mt() || MTA_SEND_FILE == row->other.mt())
            {
                if (nullptr == prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.msgitem_utag == row->other.utag; }))
                {
                    if (row->other.message.get_char(0) == '?')
                    {
                        row->other.message.set_char(0, '*');
                        row->changed();
                        prf().changed();
                    }
                } else
                {
                    if (row->other.message.get_char(0) != '?')
                    {
                        if (row->other.message.get_char(0) == '*')
                            row->other.message.set_char(0, '?');
                        else
                            row->other.message.insert(0, '?');
                        row->changed();
                        prf().changed();
                    }
                }
            }

            add_history(row->other.time) = row->other;
        }
    }
}

bool contact_c::check_invite(RID r, GUIPARAM p)
{
    ASSERT( !is_multicontact() );
    if (!opts.unmasked().is(F_SHOW_FRIEND_REQUEST))
    {
        if (p)
        {
            DEFERRED_UNIQUE_CALL(1.0, DELEGATE(this, check_invite), (GUIPARAM)((int)p - 1));
        } else
        {
            // no invite... looks like already confirmed, but not saved
            // so, accept again
            b_accept(RID(), nullptr);
        }
    }
    return true;
}

void contact_c::send_file(const ts::wstr_c &fn)
{
    contact_c *historian = get_historian();
    contact_c *cdef = historian->subget_default();
    contact_c *c_file_to = nullptr;
    historian->subiterate([&](contact_c *c) {
        active_protocol_c *ap = prf().ap(c->getkey().protoid);
        if (ap && 0 != (PF_SEND_FILE & ap->get_features()))
        {
            if (c_file_to == nullptr || cdef == c)
                c_file_to = cdef;
        }
    });

    if (c_file_to)
    {
        uint64 utag = ts::uuid();
        while( nullptr != prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.utag == utag; })) 
            ++utag;

        g_app->register_file_transfer(historian->getkey(), c_file_to->getkey(), utag, fn, 0 /* 0 means send */);
    }
}

bool contact_c::recalc_unread()
{
    if (gui_contact_item_c *gi = gui_item)
    {
        int unread = prf().calc_history_after(getkey(), readtime);
        if (unread > 99) unread = 99;
        if (unread != gi->n_unread)
        {
            gi->n_unread = unread;
            gi->getengine().redraw();
            g_app->F_SETNOTIFYICON = true;
            g_app->F_NEEDFLASH |= gi->n_unread > 0;
        }
        return gi->n_unread > 0;
    }
    return false;
}

bool contact_c::flashing_proc(RID, GUIPARAM)
{
    if (gui_item)
    {
        if (opts.unmasked().is(F_RINGTONE))
        {
            opts.unmasked().invert(F_RINGTONE_BLINK);
            DEFERRED_UNIQUE_CALL(0.5f, DELEGATE(this, flashing_proc), nullptr);
        }
        gui_item->getengine().redraw();
    }
    return true;
}
bool contact_c::ringtone(bool activate, bool play_stop_snd)
{
    if (is_multicontact())
    {
        opts.unmasked().init(F_RINGTONE, false);
        subiterate( [this](contact_c *c) { if (c->is_ringtone()) opts.unmasked().init(F_RINGTONE, true); } );

        flashing_proc(RID(), nullptr);

    } else if (getmeta())
    {
        bool wasrt = opts.unmasked().is(F_RINGTONE);
        opts.unmasked().init(F_RINGTONE, activate);
        getmeta()->ringtone(activate);
        g_app->update_ringtone( this, play_stop_snd );
        if (wasrt && !activate)
        {
            gmsg<ISOGM_CALL_STOPED>(this, STOPCALL_REJECT).send();
            return true;
        }
    }
    return false;
}

void contact_c::av( bool f )
{
    if (is_multicontact())
    {
        bool wasav = opts.unmasked().is(F_AV_INPROGRESS);
        opts.unmasked().init(F_AV_INPROGRESS, false);
        subiterate([this](contact_c *c) { if (c->is_av()) opts.unmasked().init(F_AV_INPROGRESS, true); });
        if ( opts.unmasked().is(F_AV_INPROGRESS) != wasav )
        {
            gmsg<ISOGM_AV>(this, !wasav).send();
            if (wasav)
            {
                play_sound(snd_hangon, false);
            }
        }
        
    } else if (CHECK(getmeta()))
    {
        bool wasav = opts.unmasked().is(F_AV_INPROGRESS);
        opts.unmasked().init(F_AV_INPROGRESS, f);
        getmeta()->av(f);
        if (wasav && !f)
            gmsg<ISOGM_CALL_STOPED>(this, STOPCALL_HANGUP).send();

    }
}


bool contact_c::calltone(bool f, bool call_accepted)
{
    if (is_multicontact())
    {
        bool wasct = opts.unmasked().is(F_CALLTONE);
        opts.unmasked().clear(F_CALLTONE);
        subiterate([this](contact_c *c) { if (c->is_calltone()) opts.unmasked().set(F_CALLTONE); });
        if (opts.unmasked().is(F_CALLTONE) != wasct)
        {
            if (wasct)
            {
                stop_sound(snd_calltone);
            } else play_sound(snd_calltone, true);
            //gmsg<ISOGM_AV>(this, !wasav).send();

            return wasct && !f;
        }
    } else if (CHECK(getmeta()))
    {
        opts.unmasked().init(F_CALLTONE, f);
        if ( getmeta()->calltone(f) )
        {
            if (!call_accepted)
                gmsg<ISOGM_CALL_STOPED>(this, STOPCALL_CANCEL).send();
            return true;
        }
    }
    return false;
}

bool contact_c::is_filein() const
{
    if (is_multicontact())
        return g_app->present_file_transfer_by_historian( getkey(), true );

    return g_app->present_file_transfer_by_sender( getkey(), true );
}


bool contact_c::b_accept_call(RID, GUIPARAM)
{
    if (ringtone(false, false))
    {
        if (active_protocol_c *ap = prf().ap(getkey().protoid))
            ap->accept_call(getkey().contactid);

        if (CHECK(getmeta()))
        {
            const post_s *p = get_historian()->fix_history(MTA_INCOMING_CALL, MTA_CALL_ACCEPTED, getkey(), get_historian()->nowtime());
            if (p) gmsg<ISOGM_SUMMON_POST>(*p, true).send();
        }

        //gmsg<ISOGM_NOTICE>( get_historian(), this, NOTICE_CALL_INPROGRESS ).send();
        av( true );
    }

    return true;
}

bool contact_c::b_hangup(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);

    if (active_protocol_c *ap = prf().ap(getkey().protoid))
        ap->stop_call(getkey().contactid, STOPCALL_HANGUP);

    if (CHECK(getmeta()))
    {
        const post_s *p = get_historian()->fix_history(MTA_CALL_ACCEPTED, MTA_HANGUP, getkey(), get_historian()->nowtime());
        if (p) gmsg<ISOGM_SUMMON_POST>(*p, true).send();
    }
    av( false );
    return true;
}

bool contact_c::b_call(RID, GUIPARAM)
{
    if (!ASSERT(!is_av())) return true;
    if (active_protocol_c *ap = prf().ap(getkey().protoid))
        ap->call(getkey().contactid, 60);

    gmsg<ISOGM_NOTICE>( get_historian(), this, NOTICE_CALL ).send();
    calltone();

    return true;
}

bool contact_c::b_cancel_call(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);

    if (calltone(false))
        if (active_protocol_c *ap = prf().ap(getkey().protoid))
            ap->stop_call(getkey().contactid, STOPCALL_CANCEL);

    return true;
}


bool contact_c::b_reject_call(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);

    if (ringtone(false))
        if (active_protocol_c *ap = prf().ap(getkey().protoid))
            ap->stop_call(getkey().contactid, STOPCALL_REJECT);

    if (CHECK(getmeta()))
    {
        const post_s *p = get_historian()->fix_history(MTA_INCOMING_CALL, MTA_INCOMING_CALL_REJECTED, getkey(), get_historian()->nowtime());
        if (p) gmsg<ISOGM_SUMMON_POST>( *p, true ).send();
    }

    return true;
}


bool contact_c::b_accept(RID, GUIPARAM par)
{
    if (par)
    {
        gui_notice_c *ownitm = (gui_notice_c *)par;
        HOLD(ownitm->getparent())().getparent().call_children_repos();
        TSDEL(ownitm);
    }

    if (active_protocol_c *ap = prf().ap(getkey().protoid))
        ap->accept(getkey().contactid);

    get_historian()->fix_history(MTA_FRIEND_REQUEST, MTA_OLD_REQUEST, getkey());
    get_historian()->reselect(false);

    prf().dirtycontact(getkey());
    return true;
}
bool contact_c::b_reject(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);

    if (active_protocol_c *ap = prf().ap( getkey().protoid ))
        ap->reject(getkey().contactid);

    get_historian()->reselect(false);

    prf().dirtycontact(getkey());
    return true;
}

bool contact_c::b_resend(RID, GUIPARAM)
{
    SUMMON_DIALOG<dialog_addcontact_c>(UD_ADDCONTACT, dialog_addcontact_params_s( TTT("[appname]: Повторный запрос",83), get_pubid(), getkey() ));
    return true;
}

bool contact_c::b_kill(RID, GUIPARAM)
{
    contacts().kill( getkey() );
    return true;
}

bool contact_c::b_load(RID, GUIPARAM n)
{
    int n_load = (int)n;
    get_historian()->load_history(n_load);
    get_historian()->reselect(false);
    return true;
}

bool contact_c::b_receive_file(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    uint64 utag = ownitm->get_utag();
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);
    if (file_transfer_s *ft = g_app->find_file_transfer(utag))
        ft->auto_confirm();

    return true;
}

bool contact_c::b_receive_file_as(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    uint64 utag = ownitm->get_utag();

    if (file_transfer_s *ft = g_app->find_file_transfer(utag))
    {
        ts::wstr_c downf = prf().download_folder();
        path_expand_env(downf);
        ts::make_path(downf);
                    
        ts::wstr_c title = TTT("Сохранение файла",179);
        ++sysmodal;
        ts::wstr_c fn = ts::get_save_filename_dialog(downf, ft->filename, CONSTWSTR(""), nullptr, title);
        --sysmodal;

        if (!fn.is_empty())
        {
            HOLD(ownitm->getparent())().getparent().call_children_repos();
            TSDEL(ownitm);

            ft->prepare_fn(fn, true);
            if (active_protocol_c *ap = prf().ap(ft->sender.protoid))
                ap->file_control(utag, FIC_ACCEPT);
        }


    }

    return true;
}

bool contact_c::b_refuse_file(RID, GUIPARAM par)
{
    gui_notice_c *ownitm = (gui_notice_c *)par;
    uint64 utag = ownitm->get_utag();
    HOLD(ownitm->getparent())().getparent().call_children_repos();
    TSDEL(ownitm);

    if (file_transfer_s *ft = g_app->find_file_transfer(utag))
    {
        ASSERT( getkey() == ft->sender );
        ft->kill( FIC_REJECT );
    }

    return true;
}


const post_s * contact_c::fix_history( message_type_app_e oldt, message_type_app_e newt, const contact_key_s& sender, time_t update_time )
{
    contact_c *historian = this;
    if (!historian->is_multicontact()) historian = historian->getmeta();
    const post_s * r = nullptr;
    if (ASSERT(historian))
    {
        ts::tmp_tbuf_t<uint64> updated;
        int ri = historian->iterate_history_changetime([&](post_s &p) 
        {
            if (oldt == p.type && (sender.is_empty() || p.sender == sender))
            {
                p.type = newt; 
                updated.add(p.utag);
                if (update_time)
                    p.time = update_time++;
            }
            return true;
        });
        if (ri >= 0) r = &historian->history.get(ri);
        for( uint64 utag : updated )
        {
            const post_s *p = historian->get_post_by_utag(utag);
            if (ASSERT(p))
                prf().change_history_item(historian->getkey(), *p, HITM_MT|HITM_TIME);
        }
    }
    return r;
}

contacts_c::contacts_c()
{
    self = TSNEW(contact_c);
    arr.add(self);
}
contacts_c::~contacts_c()
{
    for(contact_c *c : arr)
        c->prepare4die();
}

ts::str_c contacts_c::find_pubid(int protoid) const
{
    if (contact_c *sc = find_subself(protoid))
        return sc->get_pubid();

    return ts::str_c();
}
contact_c *contacts_c::find_subself(int protoid) const
{
    return self->subget( contact_key_s(0, protoid) );
}

void contacts_c::nomore_proto(int id)
{
    for (contact_c *c : arr)
        if (!c->is_multicontact() && c->getkey().protoid == id)
            c->protohit(false);
}

bool contacts_c::present_protoid(int id) const
{
    for(const contact_c *c : arr)
        if (!c->is_multicontact() && c->getkey().protoid == id)
            return true;
    return false;
}

int contacts_c::find_free_meta_id() const
{
    int id=1;
    for(; present( contact_key_s(id) ) || !prf().isfreecontact(contact_key_s(id))  ;++id) ;
    return id;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    if (ch.pass == 0)
    {
        switch (ch.pp)
        {
        case PP_USERNAME:
            get_self().set_name(ch.s);
            break;
        case PP_USERSTATUSMSG:
            get_self().set_statusmsg(ch.s);
            break;
        }
    }

    return 0;
}

void contact_c::setup( const contacts_s * c, time_t nowtime )
{
    set_name(c->name);
    set_customname(c->customname);
    set_statusmsg(c->statusmsg);
    set_avatar(c->avatar.data(), c->avatar.size(), c->avatar_tag);
    set_readtime(ts::tmin(nowtime, c->readtime));

    options().setup(c->options);
    keeph = (keep_contact_history_e)((c->options >> 16) & 3);
}

void contact_c::save( contacts_s * c ) const
{
    c->metaid = getmeta() ? getmeta()->getkey().contactid : 0;
    c->options = get_options() | (get_keeph() << 16);
    c->name = get_name(false);
    c->customname = get_customname();
    c->statusmsg = get_statusmsg(false);
    c->readtime = get_readtime();
    // avatar data copied not here, see profile_c::set_avatar
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_LOADED>&msg)
{
    if (msg.tabi == pt_contacts)
    {
        time_t nowtime = now();
        // 1st pass - create meta
        ts::tmp_pointers_t<contacts_s, 32> notmeta;
        //ts::tmp_pointers_t<contact_c, 32> meta;
        for( auto &row : prf().get_table_contacts() )
        {
            if (row.other.metaid == 0)
            {
                contact_key_s metakey(row.other.key.contactid);
                contact_c *metac = nullptr;
                ts::aint index;
                if (arr.find_sorted(index, metakey))
                    metac = arr.get(index);
                else
                {
                    metac = TSNEW(contact_c, metakey);
                    arr.insert(index, metac);
                }
                metac->setup(&row.other, nowtime);
            } else
                notmeta.add(&row.other);
        }
        for(contacts_s *c : notmeta)
        {
            contact_c *metac = nullptr;
            ts::aint index;
            if (CHECK(arr.find_sorted(index, contact_key_s(c->metaid))))
            {
            meta_restored:
                metac = arr.get(index);
                contact_c *cc = metac->subgetadd(c->key);
                cc->setup(c, 0);

                if (!arr.find_sorted(index, cc->getkey()))
                    arr.insert(index, cc);
            } else
            {
                contact_key_s metakey(c->metaid);
                contact_c *metac = TSNEW(contact_c, metakey);
                arr.insert(index, metac);
                goto meta_restored;
            }
        }
        //for (contact_c *c : meta)
        //    gmsg<ISOGM_UPDATE_CONTACT_V>(c).send();
    }
    if (msg.tabi == pt_unfinished_file_transfer)
    {
        // cleanup transfers
        TS_STATIC_CHECK( pt_unfinished_file_transfer > pt_contacts, "unfinished transfer table must be loaded after contacts" );
        while( auto *row =  prf().get_table_unfinished_file_transfer().find<true>([](unfinished_file_transfer_s &uft){
        
            if (0 == uft.msgitem_utag) return true;
            contact_c *c = contacts().find( uft.historian );
            if (c == nullptr) return true;
            prf().load_history( c->getkey() );
            if (nullptr == prf().get_table_history().find<true>([&](history_s &h){
                if (h.historian != uft.historian) return false;
                if (uft.msgitem_utag == h.utag) return true;
                return false;
            }))
                return true;

            return false;
        })) { if (row->deleted()) prf().changed(); }

    }
    return 0;
}

void contacts_c::update_meta()
{
    for (contact_c *c : arr)
        if (c->is_meta() && !c->getkey().is_self())
            gmsg<ISOGM_UPDATE_CONTACT_V>(c).send();
}

void contacts_c::kill(const contact_key_s &ck)
{
    contact_c * cc = find(ck);
    ASSERT(!cc->getkey().is_group()); // TODO

    if (cc->is_multicontact())
    {
        cc->subiterate([this](contact_c *c) {
            c->ringtone(false);
            c->av(false);
            c->calltone(false);
        });
    } else
    {
        cc->ringtone(false);
        cc->av(false);
        cc->calltone(false);
    }

    cc->set_state(CS_ROTTEN);
    gmsg<ISOGM_UPDATE_CONTACT_V>(cc).send();
    if (cc->is_multicontact())
    {
        cc->subiterate([this](contact_c *c) {
            if (active_protocol_c *ap = prf().ap(c->getkey().protoid)) 
                ap->del_contact(c->getkey().contactid);
            prf().killcontact(c->getkey());
            del(c->getkey());
        });
        prf().kill_history( cc->getkey() );
    } else
    {
        contact_c *meta = cc->getmeta();
        if (ASSERT(meta))
        {
            if (prf().calc_history(meta->getkey()) == 0)
            {
                int live = 0;
                meta->subiterate([&](contact_c *c) {
                    if (c->get_state() != CS_ROTTEN) ++live;
                });
                if (live == 0)
                {
                    meta->set_state(CS_ROTTEN);
                    gmsg<ISOGM_UPDATE_CONTACT_V>(meta).send();
                    prf().killcontact(meta->getkey());
                    prf().kill_history(meta->getkey());
                    del(meta->getkey());
                }
            }
        }

        if (active_protocol_c *ap = prf().ap(cc->getkey().protoid))
            ap->del_contact(cc->getkey().contactid);

    }
    prf().killcontact(cc->getkey());
    del(cc->getkey());
}

contact_c *contacts_c::create_new_meta()
{
    contact_c *metac = TSNEW(contact_c, contact_key_s(find_free_meta_id()));

    ts::aint index;
    if (CHECK(!arr.find_sorted(index, metac->getkey())))
        arr.insert(index, metac);
    prf().dirtycontact(metac->getkey());

    return metac;
}

ts::uint32 contacts_c::gm_handler(gmsg<GM_UI_EVENT> &e)
{
    if (UE_THEMECHANGED == e.evt)
    {
        for(contact_c *c : arr)
        {
            if (c->getmeta() != nullptr && !c->getkey().is_self())
            {
                if (const avatar_s *a = c->get_avatar())
                {
                    ts::blob_c ava = prf().get_avatar(c->getkey());
                    c->set_avatar(ava.data(), ava.size(), ava.size() ? a->tag : 0);
                }
            }
        }
    }
    return 0;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_AVATAR> &ava)
{
    if (contact_c *c = find(ava.contact))
        c->set_avatar( ava.data.data(), ava.data.size(), ava.tag );

    prf().set_avatar( ava.contact, ava.data, ava.tag );
    
    return 0;
}

ts::uint32 contacts_c::gm_handler( gmsg<ISOGM_UPDATE_CONTACT>&contact )
{
    prf().dirty_sort();

    int serious_change = 0;
    if (contact.state == CS_ROTTEN && 0 != (contact.mask & CDM_STATE))
    {
        kill(contact.key);
        return 0;
    }

    contact_c *c;
    bool is_self = false;

    if (contact.key.contactid == 0)
    {
        c = self->subgetadd(contact.key);
        if (active_protocol_c *ap = prf().ap(contact.key.protoid))
            ap->set_avatar(c);
        is_self = true;
        
    } else
    {
        ts::aint index;
        if (!arr.find_sorted(index, contact.key))
        {
            c = create_new_meta()->subgetadd( contact.key );
            
            if (CHECK(!arr.find_sorted(index, c->getkey()))) // find index again due create_new_meta can make current index invalid
                arr.insert(index, c);

            prf().purifycontact( c->getkey() );

            // serious change - new contact added. proto data must be saved
            serious_change = c->getkey().protoid;

        } else
        {
            c = arr.get(index);
            ASSERT( c->getmeta() != nullptr ); // it must be part of metacontact
        }
    }

    if (0 != (contact.mask & CDM_PUBID))
    {
        // allow change pubid if
        // - not yet set
        // - of self
        // - just accepted contact (Tox: because send request pubid and accepted pubid are not same (see nospam tail) )

        bool accepted = c->get_state() == CS_INVITE_SEND && (0 != (contact.mask & CDM_STATE)) && ( contact.state == CS_ONLINE || contact.state == CS_OFFLINE );
        if (c->get_pubid().is_empty() || is_self || accepted)
            c->set_pubid(contact.pubid);
        else
        {
            ASSERT( c->get_pubid().equals(contact.pubid) );
        }
    }

    if (0 != (contact.mask & CDM_NAME))
        c->set_name( ts::wstr_c().set_as_utf8(contact.name) );

    if (0 != (contact.mask & CDM_STATUSMSG))
        c->set_statusmsg( ts::wstr_c().set_as_utf8(contact.statusmsg) );

    if (0 != (contact.mask & CDM_STATE))
    {
        c->protohit(true);
        contact_state_e oldst = c->get_state();
        c->set_state( contact.state );

        if ( contact.state == CS_ONLINE || contact.state == CS_OFFLINE )
        {
            if ( oldst == CS_INVITE_RECEIVE )
            {
                // accept ok
                gmsg<ISOGM_MESSAGE> msg( c, &get_self(), MTA_ACCEPT_OK );
                c->fix_history(MTA_FRIEND_REQUEST, MTA_OLD_REQUEST);
                msg.send();
                serious_change = c->getkey().protoid;
                c->reselect(true);

            } else if (oldst == CS_INVITE_SEND)
            {
                // accepted
                gmsg<ISOGM_MESSAGE>( c, &get_self(), MTA_ACCEPTED ).send();
                serious_change = c->getkey().protoid;
            }
        }

        if ( c->get_state() != oldst )
        {
            if (c->get_state() == CS_INVITE_RECEIVE)
                c->check_invite();
        }

        prf().dirty_sort();
    }
    if (0 != (contact.mask & CDM_GENDER))
    {
        c->set_gender(contact.gender);
    }
    if (0 != (contact.mask & CDM_ONLINE_STATE))
    {
        c->set_ostate(contact.ostate);
    }

    if (0 != (contact.mask & CDM_AVATAR_TAG) && !is_self)
    {
        if (c->avatar_tag() != contact.avatar_tag)
        {
            if (contact.avatar_tag == 0)
            {
                c->set_avatar(nullptr,0);
                prf().set_avatar(c->getkey(), ts::blob_c(), 0);
            }
            else if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                ap->avatar_data_request( contact.key.contactid );
        }
    }


    prf().dirtycontact(c->getkey());

    gmsg<ISOGM_UPDATE_CONTACT_V>( c ).send();


    if (serious_change)
    {
        // serious change - new contact added. proto data must be saved
        if (active_protocol_c *ap = prf().ap(serious_change))
            ap->save_config(false);
    }

    return 0;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    if (nv.ver.is_empty()) return 0; // notification about no new version
    if ( new_version( cfg().autoupdate_newver(), nv.ver ) )
        cfg().autoupdate_newver( nv.ver );
    g_app->newversion( new_version() );
    self->gui_item->getengine().redraw();
    if ( g_app->newversion() )
        gmsg<ISOGM_NOTICE>( self, nullptr, NOTICE_NEWVERSION, ts::to_wstr(nv.ver.as_sptr())).send();

    play_sound( snd_incoming_message, false );

    return 0;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_DELIVERED>&d)
{
    contact_key_s historian;
    if (prf().change_history_item(d.utag, historian))
    {
        contact_c *c = find(historian);
        if (CHECK(c))
        {
            c->iterate_history([&](post_s &p)->bool {
                if ( p.utag == d.utag )
                {
                    ASSERT(p.type == MTA_UNDELIVERED_MESSAGE);
                    p.type = MTA_MESSAGE;
                    return true;
                }
                return false;
            });
        }
    }

    return 0;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_FILE>&ifl)
{
    switch (ifl.fctl)
    {
    case FIC_NONE:
        {
            if (ifl.data.size())
            {
                // incoming file data
                if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
                    ft->save( ifl.offset, ifl.data );
                return 0;
            }

            if (ifl.filename.get_length() == 0)
            {
                // query
                if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
                    ft->query(ifl.offset, (int)ifl.filesize);
                return 0;
            }

            contact_c *sender = find(ifl.sender);
            if (sender == nullptr) return 0; // sender. no sender - no message
            contact_c *historian = get_historian(sender, &get_self());

            if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool 
                {
                    if (uftr.upload) return false;
                    return ifl.filename.equals( uftr.filename ) && ifl.filesize == uftr.filesize;
                }))
            {
                // resume
                uint64 guiitm_utag = row->other.msgitem_utag;
                ts::wstr_c fod = row->other.filename_on_disk;

                row->deleted(); // this row not needed anymore
                prf().changed();

                if (guiitm_utag == 0 || !is_file_exists(fod) || nullptr != g_app->find_file_transfer(ifl.utag))
                    goto not_resume;

                auto &tft = prf().get_table_unfinished_file_transfer().getcreate(0);
                tft.other.utag = ifl.utag;
                tft.other.msgitem_utag = guiitm_utag;
                tft.other.filesize = ifl.filesize;
                tft.other.filename = ifl.filename;

                if (file_transfer_s *ft = g_app->register_file_transfer(historian->getkey(), ifl.sender, ifl.utag, ifl.filename, ifl.filesize))
                {
                    ft->filename_on_disk = fod;
                    tft.other.filename_on_disk = fod;
                    ft->resume();

                } else if (active_protocol_c *ap = prf().ap(ifl.sender.protoid))
                    ap->file_control(ifl.utag, FIC_REJECT);


            } else
            {
                not_resume:
                if (file_transfer_s *ft = g_app->register_file_transfer(historian->getkey(), ifl.sender, ifl.utag, ifl.filename, ifl.filesize))
                {
                    if (ft->confirm_required())
                    {
                        play_sound(snd_incoming_file, false);
                        gmsg<ISOGM_NOTICE> n(historian, sender, NOTICE_FILE, ifl.filename);
                        n.utag = ifl.utag;
                        n.send();
                    } else
                        ft->auto_confirm();
                }
                else if (active_protocol_c *ap = prf().ap(ifl.sender.protoid))
                    ap->file_control(ifl.utag, FIC_REJECT);
            }

            if (historian->gui_item)
                historian->gui_item->getengine().redraw();

        }
        break;
    case FIC_BREAK:
    case FIC_REJECT:
        DMSG("ftbreak " << ifl.utag);
        if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
            ft->kill(FIC_NONE);
        break;
    case FIC_DONE:
        DMSG("ftdone " << ifl.utag);
        if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
            ft->kill( FIC_DONE );
        break;
    case FIC_PAUSE:
    case FIC_UNPAUSE:
        if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
            ft->pause_by_remote(FIC_PAUSE == ifl.fctl);
        break;
    case FIC_DISCONNECT:
        DMSG("ftdisc " << ifl.utag);
        if (file_transfer_s *ft = g_app->find_file_transfer(ifl.utag))
            ft->kill(FIC_DISCONNECT);
    }

    return 0;
}

ts::uint32 contacts_c::gm_handler(gmsg<ISOGM_INCOMING_MESSAGE>&imsg)
{
    contact_c *sender = find( imsg.sender );
    if (sender == nullptr) return 0; // sender. no sender - no message

    contact_c *historian = get_historian( sender, &get_self() );
    prf().dirty_sort();
    switch (imsg.mt)
    {
    case MTA_CALL_STOP:
        if (historian->is_ringtone())
        {
            const post_s * p = historian->fix_history(MTA_INCOMING_CALL, MTA_INCOMING_CALL_CANCELED, sender->getkey(), historian->nowtime());
            if (p) gmsg<ISOGM_SUMMON_POST>(*p, true).send();
            sender->ringtone(false);
        } else if (sender->is_av())
        {
            // hangup by remote peer
            const post_s * p = historian->fix_history(MTA_CALL_ACCEPTED, MTA_HANGUP, sender->getkey(), historian->nowtime());
            if (p) gmsg<ISOGM_SUMMON_POST>(*p, true).send();
            sender->av(false);
        } else if (sender->is_calltone())
        {
            sender->calltone(false);
        }
        return 0;
    case MTA_CALL_ACCEPTED__:
        if (!sender->is_calltone()) // ignore this message if no calltone
            return 0;
        imsg.mt = MTA_CALL_ACCEPTED; // replace type of message here
        break;
    }


    gmsg<ISOGM_MESSAGE> msg(sender, &get_self(), imsg.mt);
    msg.create_time = imsg.create_time;
    msg.post.message.set_as_utf8( imsg.msgutf8 );
    msg.post.message.trim();
    msg.send();

    bool up_unread = true;
    switch (imsg.mt)
    {
    case MTA_FRIEND_REQUEST:
        sender->friend_request();
    case MTA_MESSAGE:
    case MTA_ACTION:
        if (g_app->is_inactive(true) || !msg.current)
            play_sound( snd_incoming_message, false );
        break;
    case MTA_INCOMING_CALL:
        up_unread = false;
        msg.sender->ringtone();
        break;
    case MTA_CALL_ACCEPTED:
        sender->calltone(false, true);
        sender->av(true);
        up_unread = false;
        break;
    }

    if (up_unread)
    {
        contact_c *historian = msg.get_historian();
        if (ASSERT(historian) && historian->gui_item)
        {
            ++historian->gui_item->n_unread;
            g_app->F_NEEDFLASH = true;
            g_app->F_SETNOTIFYICON = true;
            historian->gui_item->getengine().redraw();
            g_app->need_recalc_unread(historian->getkey());
        }
    }

    return 0;
}

