#include "isotoxin.h"

//-V:getkey:807
//-V:theme:807


MAKE_CHILD<gui_contact_item_c>::~MAKE_CHILD()
{
    ASSERT(parent);
    get().update_text();
    MODIFY(get()).visible(true);
}

gui_contact_item_c::gui_contact_item_c(MAKE_ROOT<gui_contact_item_c> &data) :gui_label_c(data), role(CIR_DNDOBJ), contact(data.contact)
{
}

gui_contact_item_c::gui_contact_item_c(MAKE_CHILD<gui_contact_item_c> &data) :gui_label_c(data), role(data.role), contact(data.contact)
{
    if (contact && (CIR_LISTITEM == role || CIR_ME == role))
        if (ASSERT(contact->is_rootcontact()))
        {
            tooltip( DELEGATE(this,tt) );
            contact->gui_item = this;
            g_app->new_blink_reason( contact->getkey() ).recalc_unread();
        }
}

gui_contact_item_c::~gui_contact_item_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, update_buttons));
        gui->delete_event(DELEGATE(this, redraw_now));
        gui->delete_event(DELEGATE(this, stop_typing));
        gui->delete_event(DELEGATE(this, animate_typing));
    }
}

ts::wstr_c gui_contact_item_c::tt()
{
    if (contact && !contact->get_comment().is_empty())
        return from_utf8( contact->get_comment() );
    return ts::wstr_c();
}

/*virtual*/ ts::ivec2 gui_contact_item_c::get_min_size() const
{
    if (CIR_ME == role || CIR_CONVERSATION_HEAD == role)
    {
        return ts::ivec2( g_app->mecontactheight );
    }

    ts::ivec2 subsize(0);
    if (CIR_DNDOBJ == role)
    {
        if (const theme_rect_s *thr = themerect())
            subsize = thr->maxcutborder.lt + thr->maxcutborder.rb;
    }
    return g_app->contactheight - subsize;
}

/*virtual*/ ts::ivec2 gui_contact_item_c::get_max_size() const
{
    ts::ivec2 m = __super::get_max_size();
    m.y = get_min_size().y;
    return m;
}

namespace
{

struct head_stuff_s
{
    ts::ivec2 last_head_text_pos;
    rbtn::ebutton_s updr[rbtn::EB_MAX];
    RID editor;
    RID bconfirm;
    RID bcancel;
    ts::str_c curedit; // utf8
    ts::safe_ptr<gui_button_c> call_button;

    static bool _edt(const ts::wstr_c & e);
};

ts::static_setup<head_stuff_s,1000> hstuff;

bool head_stuff_s::_edt(const ts::wstr_c & e)
{
    hstuff().curedit = to_utf8(e);
    return true;
}

struct leech_edit : public autoparam_i
{
    int sx;
    leech_edit(int sx) :sx(sx)
    {
    }
    /*virtual*/ bool i_leeched( guirect_c &to ) override 
    {
        if (__super::i_leeched(to))
        {
            evt_data_s d;
            sq_evt(SQ_PARENT_RECT_CHANGED, owner->getrid(), d);
            return true;
        }
        return false;
    };
    virtual bool sq_evt(system_query_e qp, RID rid, evt_data_s &data) override
    {
        if (!ASSERT(owner)) return false;
        if (owner->getrid() != rid) return false;

        if (qp == SQ_PARENT_RECT_CHANGING)
        {
            HOLD r(owner->getparent());

            int width = 100 + sx + r.as<gui_contact_item_c>().contact_item_rite_margin();

            ts::ivec2 szmin = owner->get_min_size(); if (szmin.x < width) szmin.y = width;
            ts::ivec2 szmax = owner->get_max_size(); if (szmax.y < width) szmax.y = width;
            r().calc_min_max_by_client_area(szmin, szmax);
            fixrect(data.rectchg.rect, szmin, szmax, data.rectchg.area);
            return false;
        }

        if (qp == SQ_PARENT_RECT_CHANGED)
        {
            ts::ivec2 szmin = owner->get_min_size();
            HOLD r(owner->getparent());
            ts::irect cr = r().get_client_area();

            int width = cr.width() - sx - r.as<gui_contact_item_c>().contact_item_rite_margin() - 5 -
                g_app->buttons().confirmb->size.x - g_app->buttons().cancelb->size.x;
            if (width < 100) width = 100;
            int height = szmin.y;

            MODIFY(*owner).pos(sx, cr.lt.y + (cr.height()-height) / 2).sizew(width).visible(true);
            return false;
        }
        return false;

    }
};

}

bool gui_contact_item_c::apply_edit( RID r, GUIPARAM p)
{
    //text_adapt_user_input(hstuff().curedit);
    if ( flags.is(F_EDITNAME) )
    {
        flags.clear(F_EDITNAME);
        if (contact->getkey().is_self())
        {
            if (!hstuff().curedit.is_empty())
            {
                if (prf().username(hstuff().curedit))
                    gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_USERNAME, hstuff().curedit).send();
            }
            update_text();

        } else if (contact->getkey().is_group())
        {
            if (active_protocol_c *ap = prf().ap(contact->getkey().protoid))
                ap->rename_group_chat(contact->getkey().contactid, hstuff().curedit);

        } else if (contact->get_customname() != hstuff().curedit)
        {
            contact->set_customname(hstuff().curedit);
            prf().dirtycontact(contact->getkey());
            flags.set(F_SKIPUPDATE);
            gmsg<ISOGM_V_UPDATE_CONTACT>(contact).send();
            flags.clear(F_SKIPUPDATE);
            update_text();
        }

    }
    if (flags.is(F_EDITSTATUS))
    {
        flags.clear(F_EDITSTATUS);
        if (contact->getkey().is_self())
        {
            if (prf().userstatus(hstuff().curedit))
                gmsg<ISOGM_CHANGED_SETTINGS>(0, PP_USERSTATUSMSG, hstuff().curedit).send();
        }
        update_text();
    }
    g_app->update_buttons_head();
    return true;
}

bool gui_contact_item_c::cancel_edit( RID, GUIPARAM)
{
    flags.clear(F_EDITNAME|F_EDITSTATUS);
    g_app->update_buttons_head();
    return true;
}

bool gui_contact_item_c::update_buttons( RID r, GUIPARAM p )
{
    ASSERT( CIR_CONVERSATION_HEAD == role );

    if (p)
    {
        // just update pos of edit buttons
        bool redr = false;
        bool noedt = !flags.is(F_EDITNAME|F_EDITSTATUS);
        for (rbtn::ebutton_s &b : hstuff().updr)
            if (b.updated)
            {
                MODIFY bbm(b.brid);
                redr |= bbm.pos() != b.p;
                bbm.pos(b.p).setminsize(b.brid).visible(noedt);
            }

        if (redr)
            getengine().redraw();

        if (noedt && hstuff().editor)
        {
        } else
            return true;
    }

    if ( getrid() == r )
    {
        for(rbtn::ebutton_s &b : hstuff().updr)
            if (b.brid)
            {
                b.updated = false;
                MODIFY(b.brid).visible(false);
            }
    }

    if (!hstuff().updr[rbtn::EB_NAME].brid)
    {
        gui_button_c &b = MAKE_CHILD<gui_button_c>(getrid());
        b.set_face_getter(BUTTON_FACE_PRELOADED(editb));
        b.set_handler(DELEGATE(this, edit0), nullptr);
        hstuff().updr[rbtn::EB_NAME].brid = b.getrid();
    }

    if (!hstuff().updr[rbtn::EB_STATUS].brid)
    {
        gui_button_c &b = MAKE_CHILD<gui_button_c>(getrid());
        b.set_face_getter(BUTTON_FACE_PRELOADED(editb));
        b.set_handler(DELEGATE(this, edit1), nullptr);
        hstuff().updr[rbtn::EB_STATUS].brid = b.getrid();
    }

    if (flags.is(F_EDITNAME|F_EDITSTATUS))
    {
        ts::str_c eval;
        if (contact->getkey().is_self())
        {
            if (flags.is(F_EDITNAME))
                eval = prf().username();
            else
                eval = prf().userstatus();
        } else
        {
            eval = contact->get_customname();
            if (eval.is_empty())
                eval = contact->get_name();
        }
        text_prepare_for_edit(eval);
        if (hstuff().editor)
        {
        } else
        {
            hstuff().curedit.clear();

            getengine().trunc_children(2);

            gui_textfield_c &tf = (MAKE_CHILD<gui_textfield_c>(getrid(), from_utf8(eval), MAX_PATH, 0, false) << (gui_textedit_c::TEXTCHECKFUNC)head_stuff_s::_edt);
            hstuff().editor = tf.getrid();
            tf.leech(TSNEW(leech_edit, hstuff().last_head_text_pos.x));
            gui->set_focus(hstuff().editor, true);
            tf.end();
            tf.register_kbd_callback(DELEGATE( this, cancel_edit ), SSK_ESC, false);
            tf.register_kbd_callback(DELEGATE( this, apply_edit ), SSK_ENTER, false);

            gui_button_c &bok = MAKE_CHILD<gui_button_c>(getrid());
            bok.set_face_getter(BUTTON_FACE_PRELOADED(confirmb));
            bok.set_handler(DELEGATE(this, apply_edit), nullptr);
            bok.leech(TSNEW(leech_at_right, &tf, 5));
            hstuff().bconfirm = bok.getrid();

            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face_getter(BUTTON_FACE_PRELOADED(cancelb));
            bc.set_handler(DELEGATE(this, cancel_edit), nullptr);
            bc.leech(TSNEW(leech_at_right, &bok, 5));
            hstuff().bcancel = bc.getrid();

        }

    } else
    {
        getengine().trunc_children(2);
        hstuff().editor = RID();
        hstuff().bconfirm = RID();
        hstuff().bcancel = RID();

    }

    flags.clear(F_CALLBUTTON);

    if (contact && !contact->getkey().is_self() && !contact->getkey().is_group())
    {
        int features = 0;
        int features_online = 0;
        bool now_disabled = false;
        contact->subiterate([&](contact_c *c) {
            if (active_protocol_c *ap = prf().ap(c->getkey().protoid)) 
            {
                int f = ap->get_features();
                features |= f;
                if (c->get_state() == CS_ONLINE) features_online |= f;
                if (c->is_av() || c->is_ringtone() || c->is_calltone()) now_disabled = true;
            }
        });

        gui_button_c &b_call = MAKE_CHILD<gui_button_c>(getrid());
        hstuff().call_button = &b_call;
        b_call.set_face_getter(BUTTON_FACE_PRELOADED(callb));
        b_call.tooltip(TOOLTIP(TTT("Call",140)));

        b_call.set_handler(DELEGATE(this, audio_call), nullptr);
        ts::ivec2 minsz = b_call.get_min_size();
        b_call.leech(TSNEW(leech_dock_right_center_s, minsz.x, minsz.y, 2, -1, 0, 1));
        MODIFY(b_call).visible(true);
        if (now_disabled)
        {
            b_call.disable();
            b_call.tooltip(GET_TOOLTIP());
        } else if (0 == (features & PF_AUDIO_CALLS))
        {
            b_call.disable();
            b_call.tooltip(TOOLTIP(TTT("Call not supported",141)));
        } else if (0 == (features_online & PF_AUDIO_CALLS))
        {
            b_call.disable();
            b_call.tooltip(TOOLTIP(TTT("Contact offline",143)));
        }

        flags.set(F_CALLBUTTON);
    }

    g_app->update_buttons_msg();

    return true;
}

void gui_contact_item_c::created()
{
    switch (role)
    {
        case CIR_DNDOBJ:
            set_theme_rect(CONSTASTR("contact.dnd"), false);
            defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
            if (const gui_contact_item_c *r = dynamic_cast<const gui_contact_item_c *>(gui->dragndrop_underproc()))
            {
                if (r->is_protohit()) flags.set(F_PROTOHIT);
                if (r->is_noprotohit()) flags.set(F_NOPROTOHIT);
            }
            break;
        case CIR_METACREATE:
        case CIR_LISTITEM:
            set_theme_rect(CONSTASTR("contact"), false);
            defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
            break;
        case CIR_ME:
            set_theme_rect(CONSTASTR("contact.me"), false);
            defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
            break;
        case CIR_CONVERSATION_HEAD:
            set_theme_rect(CONSTASTR("contact.head"), false);
            defaultthrdraw |= DTHRO_BASE_HOLE;
            break;
    }

    if (const theme_rect_s * thr = themerect())
    {
        ts::ivec2 s = parsevec2(thr->addition.get_string(CONSTASTR("shiftstateicon")), ts::ivec2(0));
        shiftstateicon.x = (ts::int16)s.x;
        shiftstateicon.y = (ts::int16)s.y;
    }

    gui_control_c::created();

    if (CIR_CONVERSATION_HEAD == role)
    {
    }
}

bool gui_contact_item_c::audio_call(RID btn, GUIPARAM)
{
    if (contact)
    {
        contact_c *cdef = contact->subget_default();
        contact_c *c_call = nullptr;

        contact->subiterate([&](contact_c *c) {
            active_protocol_c *ap = prf().ap(c->getkey().protoid);
            if (ap && 0 != (PF_AUDIO_CALLS & ap->get_features()))
            {
                if (c_call == nullptr || (cdef == c && c->get_state() == CS_ONLINE))
                    c_call = c;
                if (c->get_state() == CS_ONLINE && c_call != c && c_call->get_state() != CS_ONLINE)
                    c_call = c;
            }
        });

        if (c_call)
        {
            c_call->b_call(RID(), nullptr);
            btn.call_enable(false);
        }

    }
    return true;
}

ts::uint32 gui_contact_item_c::gm_handler( gmsg<ISOGM_SELECT_CONTACT> & c )
{
    switch (role)
    {
    case CIR_LISTITEM:
    case CIR_METACREATE:
        if (!c.contact)
        {
            TSDEL( this );
            return 0;
        }
    case CIR_ME:
        {
            bool o = getprops().is_active();
            bool n = contact == c.contact;
            if (o != n)
            {
                MODIFY(*this).active(n);
                update_text();

                if (n)
                    g_app->active_contact_item = CIR_ME == role ? nullptr : contact->gui_item;
            }
        }
        break;
    case CIR_CONVERSATION_HEAD:
        if (!c.contact) contact = nullptr;
        break;
    }

    return 0;
}

void gui_contact_item_c::setcontact(contact_c *c)
{
    ASSERT(c->is_rootcontact());
    bool changed = contact != c;
    contact = c;
    update_text(); 

    if ( CIR_CONVERSATION_HEAD == role )
    {
        if (changed) cancel_edit();
        g_app->update_buttons_head();
    }
}

void gui_contact_item_c::typing()
{
    flags.set(F_SHOWTYPING);
    DEFERRED_UNIQUE_CALL(1.5, DELEGATE(this, stop_typing), nullptr);
    animate_typing(RID(), nullptr);
}

bool gui_contact_item_c::stop_typing(RID, GUIPARAM)
{
    typing_buf.clear();
    flags.clear(F_SHOWTYPING);
    update_text();
    return true;
}
bool gui_contact_item_c::animate_typing(RID, GUIPARAM)
{
    update_text();
    DEFERRED_UNIQUE_CALL(1.0 / 15.0, DELEGATE(this, animate_typing), nullptr);
    return true;
}

void gui_contact_item_c::update_text()
{
    if (CIR_CONVERSATION_HEAD == role)
        protocols(true)->clear();

    ts::str_c newtext;
    if (contact)
    {
        if (contact->getkey().is_self())
        {
            newtext = prf().username();
            text_adapt_user_input(newtext);
            ts::str_c t2(prf().userstatus());
            text_adapt_user_input(t2);
            if (CIR_CONVERSATION_HEAD == role)
            {
                ts::ivec2 sz = g_app->buttons().editb->size;
                newtext.append( CONSTASTR(" <rect=0,") );
                newtext.append_as_uint( sz.x ).append_char(',').append_as_uint( -sz.y ).append(CONSTASTR(",2><br><l>"));
                newtext.append(t2).append(CONSTASTR(" <rect=1,"));
                newtext.append_as_uint( sz.x ).append_char(',').append_as_uint( -sz.y ).append(CONSTASTR(",2></l>"));
            } else
            {
                newtext.append(CONSTASTR("<br><l>")).append(t2).append(CONSTASTR("</l>"));
            }
        } else if (CIR_METACREATE == role && contact->subcount() > 1)
        {
            contact->subiterate([&](contact_c *c) {
                newtext.append(CONSTASTR("<nl>"));
                ts::str_c n = c->get_name(false);
                text_adapt_user_input(n);
                newtext.append( n );
                if (active_protocol_c *ap = prf().ap( c->getkey().protoid ))
                    newtext.append(CONSTASTR(" (")).append(ap->get_name()).append(CONSTASTR(")"));
            });

        } else if (contact->getkey().is_group())
        {
            newtext = contact->get_customname();
            if (newtext.is_empty()) newtext = contact->get_name();
            text_adapt_user_input(newtext);

            if (CIR_CONVERSATION_HEAD == role)
            {
                ts::ivec2 sz = g_app->buttons().editb->size;
                newtext.append(CONSTASTR(" <rect=0,"));
                newtext.append_as_uint(sz.x).append_char(',').append_as_int(-sz.y).append(CONSTASTR(",2>"));
            } else
            {
                newtext.append(CONSTASTR("<br>(")).append_as_int( contact->subonlinecount() ).append_char('/').append_as_int( contact->subcount() ).append_char(')');
                if (active_protocol_c *ap = prf().ap( contact->getkey().protoid ))
                    newtext.append(CONSTASTR(" <l>")).append(ap->get_name()).append(CONSTASTR("</l>"));
            }
            if (!contact->get_options().unmasked().is(contact_c::F_PERSISTENT_GCHAT))
                newtext.append(CONSTASTR("<br>")).append(to_utf8(TTT("temporary group chat",256)));

        } else if (contact->is_meta())
        {
            int live = 0, count = 0, rej = 0, invsend = 0, invrcv = 0, wait = 0, deactivated = 0;
            contact->subiterate( [&](contact_c *c) {
                ++count;
                if (c->get_state() != CS_ROTTEN) ++live;
                if (c->get_state() == CS_REJECTED) ++rej;
                if (c->get_state() == CS_INVITE_SEND) ++invsend;
                if (c->get_state() == CS_INVITE_RECEIVE) ++invrcv;
                if (c->get_state() == CS_WAIT) ++wait;
                if (nullptr==prf().ap(c->getkey().protoid)) ++deactivated;
            } );
            if (live == 0)
            {
                DEFERRED_EXECUTION_BLOCK_BEGIN(0)

                    if (contact_key_s *ck = gui->temp_restore<contact_key_s>(as_int(param)))
                        contacts().kill( *ck );

                DEFERRED_EXECUTION_BLOCK_END( gui->temp_store<contact_key_s>(contact->getkey()) )
            } else if (rej > 0)
                newtext = colorize( to_utf8(TTT("Rejected",79)).as_sptr(), get_default_text_color(COLOR_TEXT_SPECIAL) );
            else if (invsend > 0)
            {
                newtext = contact->get_customname();
                if (newtext.is_empty()) newtext = contact->get_name();
                text_adapt_user_input(newtext);
                if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                newtext.append( colorize(to_utf8(TTT("Request sent",88)).as_sptr(), get_default_text_color(COLOR_TEXT_SPECIAL)) );
            }
            else {
                newtext = contact->get_customname();
                if (newtext.is_empty()) newtext = contact->get_name();
                text_adapt_user_input(newtext);
                ts::str_c t2(contact->get_statusmsg());
                text_adapt_user_input(t2);

                if (CIR_CONVERSATION_HEAD == role)
                {
                    ts::ivec2 sz = g_app->buttons().editb->size;
                    newtext.append(CONSTASTR(" <rect=0,"));
                    newtext.append_as_uint(sz.x).append_char(',').append_as_int(-sz.y).append(CONSTASTR(",2>"));
                    t2.trim();
                    if (!t2.is_empty()) newtext.append(CONSTASTR("<br><l>")).append(t2).append(CONSTASTR("</l>"));

                } else
                {
                    if (invrcv)
                    {
                        if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                        newtext.append( colorize( to_utf8(TTT("Please, accept or reject",153)).as_sptr(), get_default_text_color(COLOR_TEXT_SPECIAL) ) );
                        t2.clear();

                        g_app->new_blink_reason(contact->getkey()).friend_invite();
                    }

                    if (wait)
                    {
                        if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                        newtext.append(colorize(to_utf8(TTT("Waiting...",154)).as_sptr(), get_default_text_color(COLOR_TEXT_SPECIAL)));
                        t2.clear();
                    } else if (contact->is_full_search_result() && g_app->found_items)
                    {
                        // Simple linear iteration... not so fast, yeah. 
                        // But I hope number of found items is not so huge

                        int cntitems = 0;
                        for (const found_item_s &fi : g_app->found_items->items)
                            if (fi.historian == contact->getkey())
                            {
                                cntitems = fi.utags.count();
                                break;
                            }
                        if (cntitems > 0)
                        {

                            if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                            newtext.append(CONSTASTR("<l>")).
                                append(colorize(to_utf8(TTT("Found: $",338)/ts::wmake(cntitems)).as_sptr(), get_default_text_color(COLOR_TEXT_FOUND))).
                                append(CONSTASTR("</l>"));
                            t2.clear();
                        }

                    } else if (flags.is(F_SHOWTYPING))
                    {
                        ts::wstr_c t,b;
                        typing_buf.split(t,b,'\1');
                        ts::wstr_c ins(CONSTWSTR("<fullheight><i>"));
                        t = text_typing( t, b, ins );
                        typing_buf.set(t).append_char('\1').append(b);
                        if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));

                        newtext.append(colorize(to_utf8(t).as_sptr(), get_default_text_color(COLOR_TEXT_TYPING)));
                        t2.clear();
                    }

                    if (count == 1 && deactivated > 0)
                    {
                        t2.clear();
                    }
                    t2.trim();
                    if (!t2.is_empty()) newtext.append(CONSTASTR("<br><l>")).append(t2).append(CONSTASTR("</l>"));
                }
            }

        } else
            newtext.set( CONSTASTR("<error>") );


    } else
    {
        newtext.clear();
    }
    textrect.set_text_only(from_utf8(newtext), false);

    if (CIR_CONVERSATION_HEAD == role && textrect.is_dirty())
    {
        for (rbtn::ebutton_s &b : hstuff().updr)
            b.updated = false;
        update_buttons(getrid());
    }
    getengine().redraw();

}

bool gui_contact_item_c::edit0(RID, GUIPARAM p)
{
    flags.set(F_EDITNAME);
    update_buttons(getrid());
    return true;
}
bool gui_contact_item_c::edit1(RID, GUIPARAM)
{
    flags.set(F_EDITSTATUS);
    update_buttons(getrid());
    return true;
}


void gui_contact_item_c::updrect(const void *, int r, const ts::ivec2 &p)
{
    if (r >= rbtn::EB_MAX) return;

    if (prf().is_loaded())
    {
        hstuff().updr[r].p = root_to_local(p);
        hstuff().updr[r].updated = true;
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, update_buttons), 1 );
    }
}

/*virtual*/ void gui_contact_item_c::update_dndobj(guirect_c *donor)
{
    update_text();
    if (const gui_contact_item_c *r = dynamic_cast<const gui_contact_item_c *>(donor))
    {
        flags.init(F_PROTOHIT, r->is_protohit());
        flags.init(F_NOPROTOHIT, r->is_noprotohit());
    }
}

void gui_contact_item_c::target(bool tgt)
{
    if (tgt)
        set_theme_rect(CONSTASTR("contact.dndtgt"), false);
    else
        set_theme_rect(CONSTASTR("contact"), false);
}

void gui_contact_item_c::on_drop(gui_contact_item_c *ondr)
{
    if (contact->getkey().is_group())
    {
        contact->join_groupchat( ondr->contact );
        return;
    }

    if (dialog_already_present(UD_METACONTACT)) return;

    SUMMON_DIALOG<dialog_metacontact_c>(UD_METACONTACT, dialog_metacontact_params_s(contact->getkey()));

    gmsg<ISOGM_METACREATE> mca(ondr->contact->getkey());
    mca.state = gmsg<ISOGM_METACREATE>::ADD;
    mca.send();

    getengine().redraw();
}

/*virtual*/ guirect_c * gui_contact_item_c::summon_dndobj(const ts::ivec2 &deltapos)
{
    flags.set( F_DNDDRAW );
    gui_contact_item_c &dnd = MAKE_ROOT<gui_contact_item_c>(contact);
    ts::ivec2 subsize(0);
    if (const theme_rect_s *thr = dnd.themerect())
        subsize = thr->maxcutborder.lt + thr->maxcutborder.rb;
    dnd.update_text();
    MODIFY(dnd).pos( getprops().screenpos() + deltapos ).size( getprops().size() - subsize ).visible(true);
    return &dnd;
}

void gui_contact_item_c::protohit()
{
    ts::flags32_s::BITS old = flags.__bits & (F_PROTOHIT|F_NOPROTOHIT);
    flags.clear(F_PROTOHIT|F_NOPROTOHIT);
    if (contact)
    {
        contact->subiterate([this](contact_c *c) {
            if (c->is_protohit(true))
                flags.set(F_PROTOHIT);
            else
                flags.set(F_NOPROTOHIT);
        });
    }
    if ( (flags.__bits & (F_PROTOHIT|F_NOPROTOHIT)) != old )
        getengine().redraw();

}

void gui_contact_item_c::clearprotocols()
{
    if (CIR_CONVERSATION_HEAD == role)
    {
        protocols(true)->clear();
        getengine().redraw();
    }
}

void gui_contact_item_c::protocols_s::update()
{
    str.clear();

    if (!owner)
        return;

    contact_c *cc = owner->contact;
    if (!cc)
        return;

    const contact_c *def = !cc->getkey().is_self() ? cc->subget_default() : nullptr;

    struct preproto_s
    {
        contact_c *c;
        tableview_t<active_protocol_s,pt_active_protocol>::row_s *row;
        int sindex;
    };
    ts::tmp_tbuf_t<preproto_s> splist;

    cc->subiterate([&](contact_c *c) {
        if (auto *row = prf().get_table_active_protocol().find<true>(c->getkey().protoid))
        {
            preproto_s &ap = splist.add();
            ap.c = c;
            ap.row = row;
            ap.sindex = 0;
        }
    });

    splist.tsort<preproto_s>([](preproto_s *s1, preproto_s *s2)->bool
    {
        if (s1->row->other.sort_factor == s2->row->other.sort_factor)
            return s1->c->getkey().contactid < s2->c->getkey().contactid;
        return s1->row->other.sort_factor < s2->row->other.sort_factor;
    });

    int maxheight = owner->getprops().size().y;
    if (const theme_rect_s *thr = owner->themerect())
        maxheight -= thr->clborder_y();

    str.set(CONSTWSTR("<p=r>"));

    for (preproto_s &s : splist)
    {
        s.sindex = str.get_length();

        if (s.c->get_state() == CS_ONLINE)
        {
            str.append(CONSTWSTR("<nl><shadow>")).append(maketag_color<ts::wchar>(owner->get_default_text_color(COLOR_PROTO_TEXT_ONLINE)));
            if (def == s.c) str.append(CONSTWSTR(" <u>")); else str.append_char(' ');
            str.append(from_utf8(s.row->other.name));
            if (def == s.c) str.append(CONSTWSTR("</u>"));
            str.append(CONSTWSTR("</color></shadow> "));
        }
        else
        {
            str.append(CONSTWSTR("<nl>")).append(maketag_color<ts::wchar>(owner->get_default_text_color(COLOR_PROTO_TEXT_OFFLINE)));
            if (def == s.c) str.append(CONSTWSTR(" <u>")); else str.append_char(' ');
            str.append(from_utf8(s.row->other.name));
            if (def == s.c) str.append(CONSTWSTR("</u>"));
            str.append(CONSTWSTR("</color> "));
        }

    }

    for(;splist.count();)
    {
        size = gui->textsize( ts::g_default_text_font, str );
        if (size.y <= maxheight)
            break;

        int ii = splist.get( splist.count() - 1 ).sindex;
        splist.remove_fast( splist.count() - 1 );
        str.set_length(ii);
    }

    dirty = false;
}

void gui_contact_item_c::generate_protocols()
{
    if (CIR_CONVERSATION_HEAD != role) return;
    if (!contact || contact->getkey().is_group()) return;

    protocols(true)->update();
}

void gui_contact_item_c::draw_online_state_text(draw_data_s &dd)
{
    if (const protocols_s *p = protocols())
    {
        if (!p->str.is_empty())
        {
            text_draw_params_s tdp;

            ts::flags32_s f; f.setup(ts::TO_VCENTER|ts::TO_LINE_END_ELLIPSIS);
            tdp.textoptions = &f;

            getengine().draw(p->str, tdp);
        }
    }
}

int gui_contact_item_c::contact_item_rite_margin()
{
    if (!flags.is(F_CALLBUTTON)) return 5;
    return g_app->buttons().callb->size.x + 15;
}

bool gui_contact_item_c::allow_drag() const
{
    if (!allow_drop())
        return false;

    if (CIR_LISTITEM == role && !contact->getkey().is_group())
    {
        gmsg<ISOGM_METACREATE> mca(contact->getkey());
        mca.state = gmsg<ISOGM_METACREATE>::CHECKINLIST;
        if (!mca.send().is(GMRBIT_ACCEPTED))
            return true;;
    }

    return false;
}

bool gui_contact_item_c::allow_drop() const
{
    int failcount = 0;
    contact->subiterate([&](contact_c *c) {
        if (c->get_state() == CS_INVITE_SEND) ++failcount;
        if (c->get_state() == CS_INVITE_RECEIVE) ++failcount;
        if (c->get_state() == CS_WAIT) ++failcount;
        if (nullptr == prf().ap(c->getkey().protoid)) ++failcount;
    });
    if (failcount > 0)
        return false;

    return true;
}

/*virtual*/ bool gui_contact_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid())
    {
        // from submenu
        if (popupmenu && popupmenu->getrid() == rid)
        {
            if (SQ_POPUP_MENU_DIE == qp)
                MODIFY(*this).highlight(false);
        }
        return false;
    }


    switch (qp)
    {
    case SQ_DRAW:
        if (flags.is( F_DNDDRAW ))
        {
            if ( gui->dragndrop_underproc() == this )
                return true;
            flags.clear( F_DNDDRAW );
        }
        gui_control_c::sq_evt(qp, rid, data);
        if (m_engine && contact)
        {
            ts::irect ca = get_client_area();

            contact_state_e st = CS_OFFLINE;
            contact_online_state_e ost = COS_ONLINE;
            if (contact)
            {
                st = contact->get_meta_state();
                ost = contact->get_meta_ostate();
                if (role == CIR_CONVERSATION_HEAD)
                    st = contact_state_check;
            }
            button_desc_s::states bst =  button_desc_s::DISABLED;
            button_desc_s *bdesc = g_app->buttons().online;
            bool force_state_icon = CIR_ME == role || CIR_METACREATE == role;
            switch (st)
            {
                case CS_INVITE_SEND:
                    bdesc = g_app->buttons().invite;
                    bst = button_desc_s::NORMAL;
                    force_state_icon = true;
                    break;
                case CS_INVITE_RECEIVE:
                    bdesc = g_app->buttons().invite;
                    bst = button_desc_s::HOVER;
                    force_state_icon = true;
                    break;
                case CS_REJECTED:
                    bdesc = g_app->buttons().invite;
                    bst = button_desc_s::DISABLED;
                    force_state_icon = true;
                    break;
                case CS_ONLINE:
                    if (ost == COS_ONLINE) bst = button_desc_s::NORMAL;
                    else if (ost == COS_AWAY) bst = button_desc_s::HOVER;
                    else if (ost == COS_DND) bst = button_desc_s::PRESS;
                    break;
                case CS_WAIT:
                    bdesc = nullptr;
                    break;
                case CS_ROTTEN:
                case CS_OFFLINE:
                    if (CIR_ME == role)
                        if ( contact->subcount() == 0 )
                            bdesc = nullptr;
                    break;
                case contact_state_check: // some online, some offline
                    if (role == CIR_CONVERSATION_HEAD)
                    {
                        bdesc = nullptr;
                    } else
                    {
                        bdesc = g_app->buttons().online2;
                        bst = button_desc_s::NORMAL;
                    }
                    break;
            }

            m_engine->begin_draw();
            
            if (!force_state_icon && prf().get_options().is(UIOPT_PROTOICONS))
                bdesc = nullptr;

            // draw state
            if (( force_state_icon || (flags.is(F_PROTOHIT) && st != CS_ROTTEN)) && bdesc)
            {
                bdesc->draw( m_engine.get(), bst, ca + ts::ivec2(shiftstateicon.x, shiftstateicon.y), button_desc_s::ARIGHT | button_desc_s::ABOTTOM );

            } else if (contact->is_meta() && flags.is(F_PROTOHIT)) // not group
            {
                // draw proto icons
                int isz = g_app->protoiconsize;
                ts::ivec2 p(ca.rb.x,ca.rb.y-isz);
                contact->subiterate([&](contact_c *c) {
                    if (c->is_protohit(false))
                        if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                        {
                            p.x -= isz;

                            icon_type_e icot = IT_OFFLINE;
                            if (c->get_state() == CS_ONLINE)
                            {
                                contact_online_state_e ost1 = c->get_ostate();
                                if (COS_AWAY == ost1) icot = IT_AWAY;
                                else if (COS_DND == ost1) icot = IT_DND;
                                else icot = IT_ONLINE;
                            }

                            m_engine->draw(p, ap->get_icon(isz, icot).extbody(), ts::irect(0, 0, isz, isz), true);
                        }
                });
            }

            if (contact->is_av() && !contact->is_full_search_result() && CIR_CONVERSATION_HEAD != role)
            {
                if (const av_contact_s *avc = g_app->find_avcontact_inprogress(contact))
                {
                    const theme_image_s *img_voicecall = contact->getkey().is_group() ? nullptr : gui->theme().get_image(CONSTASTR("voicecall"));
                    const theme_image_s *img_micoff = gui->theme().get_image(CONSTASTR("micoff"));
                    const theme_image_s *img_speakeroff = gui->theme().get_image(CONSTASTR("speakeroff"));
                    const theme_image_s *img_speakeron = contact->getkey().is_group() ? gui->theme().get_image(CONSTASTR("speakeron")) : nullptr;
                    const theme_image_s * drawarr[3];
                    int drawarr_cnt = 0;
                    if (img_voicecall)
                        drawarr[drawarr_cnt++] = img_voicecall;
                    if (avc->is_mic_off() && img_micoff)
                        drawarr[drawarr_cnt++] = img_micoff;
                    if (avc->is_speaker_off() && img_speakeroff)
                        drawarr[drawarr_cnt++] = img_speakeroff;
                    if (avc->is_speaker_on() && img_speakeron)
                        drawarr[drawarr_cnt++] = img_speakeron;

                    int h = 0;
                    for (int i = 0; i < drawarr_cnt; ++i)
                    {
                        int hh = drawarr[i]->info().sz.y;
                        if (hh > h) h = hh;
                    }
                    int addh[3];
                    for (int i = 0; i < drawarr_cnt; ++i)
                    {
                        int hh = drawarr[i]->info().sz.y;
                        addh[i] = (h - hh)/2;
                    }

                    ts::ivec2 p(ca.rt());
                    for(int i=0;i<drawarr_cnt;++i)
                    {
                        p.x -= drawarr[i]->info().sz.x;
                        drawarr[i]->draw(*m_engine, ts::ivec2(p.x, p.y + addh[i]));
                        --p.x;
                    }
                }
            }

            /*
            if (contact->is_meta() && flags.is(F_PROTOHIT)) // not group
            {
                int isz = g_app->protoiconsize;
                statedd.alpha = 128;
                ts::ivec2 p(ca.rt());
                contact->subiterate([&](contact_c *c) {
                    if (c->is_protohit(false))
                        if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                        {
                            p.x -= isz;
                            m_engine->draw( p, ap->get_icon(isz), ts::irect(0,0, isz, isz), true );
                        }
                } );
            }
            */

            m_engine->end_draw();

            if (contact)
            {
                text_draw_params_s tdp;
                
                const application_c::blinking_reason_s * achtung = nullptr;
                int ritem = 0, curpww = 0;
                bool draw_ava = !contact->getkey().is_self();
                bool draw_proto = !contact->getkey().is_group();
                //bool draw_btn = true;
                if (CIR_CONVERSATION_HEAD == role)
                {
                    ritem = contact_item_rite_margin();
                    if (draw_proto)
                    {
                        if (nullptr == protocols() || protocols()->dirty)
                            generate_protocols();
                        curpww = protocols()->size.x;
                        ritem += curpww;
                    }

                    ts::irect cac(ca);

                    int x_offset = draw_ava ? (contact->get_avatar() ? g_app->buttons().icon[CSEX_UNKNOWN]->size.x : g_app->buttons().icon[contact->get_meta_gender()]->size.x) : 0;

                    cac.lt.x += x_offset + 5;
                    cac.rb.x -= 5;
                    cac.rb.x -= ritem;
                    int curw = cac.width();
                    int w = gui->textsize( *textrect.font, textrect.get_text() ).x;
                    if (draw_ava && w > curw)
                    {
                        curw += x_offset;
                        draw_ava = false;
                    }
                    while (draw_proto && w > curw)
                    {
                        if ( curpww > g_app->minprotowidth )
                        {
                            int shift = w - curw;
                            if (shift <= (curpww - g_app->minprotowidth))
                            {
                                ritem -= curpww;
                                curw += curpww;
                                curpww -= shift;
                                curw -= curpww;
                                ritem += curpww;
                                ASSERT( w <= curw );
                                break;
                            }
                        }

                        curw += curpww;
                        draw_proto = false;
                    }

                } else if (nullptr != (achtung = g_app->find_blink_reason(contact->getkey(), false)))
                    if (!achtung->get_blinking())
                        achtung = nullptr;

                
                int x_offset = 0;
                if (draw_ava)
                {
                    m_engine->begin_draw();
                    if (const avatar_s *ava = contact->get_avatar())
                    {
                        int y = (ca.size().y - ava->info().sz.y) / 2;
                        m_engine->draw(ca.lt + ts::ivec2(y), ava->extbody(), ts::irect(0, ava->info().sz), ava->alpha_pixels);
                        x_offset = g_app->buttons().icon[CSEX_UNKNOWN]->size.x;
                    }
                    else
                    {
                        button_desc_s *icon = contact->getkey().is_group() ? g_app->buttons().groupchat : g_app->buttons().icon[contact->get_meta_gender()];
                        icon->draw(m_engine.get(), button_desc_s::NORMAL, ca, button_desc_s::ALEFT | button_desc_s::ATOP | button_desc_s::ABOTTOM);
                        x_offset = icon->size.x;
                    }
                    m_engine->end_draw();
                }

                ts::irect noti_draw_area = ca;

                if (!flags.is(F_EDITNAME|F_EDITSTATUS))
                {
                    ca.lt += ts::ivec2(x_offset + 5, 2);
                    ca.rb.x -= 5;
                    if (CIR_CONVERSATION_HEAD == role)
                    {
                        ca.rb.x -= ritem;

                        if (!draw_proto)
                            ca.rb.x += curpww;
#if 0
                        if (hstuff().call_button)
                        {
                            if (!draw_btn)
                            {
                                MODIFY(*hstuff().call_button).visible(false);
                                ca.rb.x += g_app->buttons().callb->size.x;
                            }
                            else
                                MODIFY(*hstuff().call_button).visible(true);
                        }
#endif
                    }
                    draw_data_s &dd = m_engine->begin_draw();
                    dd.size = ca.size();
                    if (dd.size >> 0)
                    {
                        if (CIR_CONVERSATION_HEAD == role)
                            hstuff().last_head_text_pos = ca.lt;
                        dd.offset += ca.lt;
                        int oldxo = dd.offset.x;
                        ts::flags32_s f; f.setup(ts::TO_VCENTER | ts::TO_LINE_END_ELLIPSIS);
                        tdp.textoptions = &f; //-V506
                        tdp.forecolor = nullptr;
                        tdp.rectupdate = DELEGATE( this, updrect );
                        draw(dd, tdp);

                        if (CIR_CONVERSATION_HEAD == role && draw_proto)
                        {
                            dd.offset.x = oldxo + dd.size.x + 5;
                            dd.size.x = curpww;
                            draw_online_state_text(dd);
                        }
                    }
                    m_engine->end_draw();
                }
                if (achtung)
                {
                    draw_data_s &dd = m_engine->begin_draw();
                    ts::ivec2 pos;
                    if (button_desc_s *bachtung = g_app->buttons().achtung)
                    {
                        dd.offset += bachtung->draw(m_engine.get(), button_desc_s::NORMAL, noti_draw_area, button_desc_s::ALEFT | button_desc_s::ABOTTOM);
                        dd.size = bachtung->size;
                        tdp.forecolor = bachtung->colors + button_desc_s::NORMAL;
                    } else
                        pos = noti_draw_area.lt;

                    if (CS_INVITE_RECEIVE != st && achtung->is_file_download() || achtung->flags.is(application_c::blinking_reason_s::F_RINGTONE))
                    {
                        const theme_image_s *img = nullptr;
                        if (achtung->flags.is(application_c::blinking_reason_s::F_RINGTONE))
                            img = gui->theme().get_image(CONSTASTR("call"));
                        else if (achtung->is_file_download())
                            img = gui->theme().get_image(CONSTASTR("file"));

                        img->draw(*m_engine, (dd.size - img->info().sz) / 2);

                    } else if ( CS_INVITE_RECEIVE == st || achtung->unread_count > 0 || achtung->flags.is(application_c::blinking_reason_s::F_NEW_VERSION | application_c::blinking_reason_s::F_INVITE_FRIEND) )
                    {
                        ts::flags32_s f; f.setup(ts::TO_VCENTER | ts::TO_HCENTER);
                        tdp.textoptions = &f; //-V506

                        if (CS_INVITE_RECEIVE == st || achtung->flags.is(application_c::blinking_reason_s::F_NEW_VERSION) || achtung->unread_count == 0)
                        {
                            m_engine->draw(CONSTWSTR("!"), tdp);
                        } else
                        {
                            int n_unread = achtung->unread_count;
                            if (n_unread > 99) n_unread = 99;
                            m_engine->draw(ts::wstr_c().set_as_uint(n_unread), tdp);
                        }
                    }

                    m_engine->end_draw();
                }
            }
        }
        return true;
    case SQ_RECT_CHANGED:
        textrect.make_dirty(false,false,true);
        return true;
    case SQ_MOUSE_IN:
        if ( CIR_LISTITEM == role || CIR_METACREATE == role )
        {
            if (!getprops().is_highlighted())
            {
                MODIFY(*this).highlight(true);
                update_text();
            }
        }
        break;
    case SQ_MOUSE_OUT:
        {
            if (CIR_LISTITEM == role || CIR_METACREATE == role)
            {
                if (getprops().is_highlighted() && popupmenu == nullptr)
                {
                    MODIFY(*this).highlight(false);
                    update_text();
                }
            }
            flags.clear(F_LBDN);
        }
        break;
    case SQ_MOUSE_LDOWN:
        flags.set(F_LBDN);

        if (allow_drag())
            gui->dragndrop_lb( this );

        return true;
    case SQ_MOUSE_LUP:
        if (flags.is(F_LBDN))
        {
            if (CIR_CONVERSATION_HEAD != role)
                gmsg<ISOGM_SELECT_CONTACT>(contact, RSEL_SCROLL_END).send();

            flags.clear(F_LBDN);
        }
        return false;
    case SQ_MOUSE_RUP:
        if (CIR_LISTITEM == role && !contact->getkey().is_self())
        {
            struct handlers
            {
                static void m_delete_doit(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    contacts().kill(ck);
                }
                static void m_delete(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    contact_c * c = contacts().find(ck);
                    if (c)
                    {
                        ts::wstr_c txt;
                        if ( c->getkey().is_group() )
                            txt = TTT("Leave group chat?[br]$",258) / from_utf8(c->get_description());
                        else
                            txt = TTT("Contact will be deleted:[br]$",84) / from_utf8(c->get_description());
                        
                        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                            DT_MSGBOX_WARNING,
                            txt
                            ).bcancel().on_ok(m_delete_doit, cks) );
                    }
                }
                static void m_metacontact_detach(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    ts::shared_ptr<contact_c> c = contacts().find(ck);
                    if (c && c->getmeta())
                    {
                        contact_c *historian = c->get_historian();
                        prf().load_history(historian->getkey()); // load whole history into memory table
                        historian->unload_history(); // clear history cache in contact
                        contact_c *detached_meta = contacts().create_new_meta();
                        historian->subdel(c);
                        detached_meta->subadd(c);
                        if (historian->gui_item) historian->gui_item->update_text();
                        prf().dirtycontact( c->getkey() );
                        prf().detach_history( historian->getkey(), detached_meta->getkey(), c->getkey() );
                        gmsg<ISOGM_V_UPDATE_CONTACT>( c ).send();
                        gmsg<ISOGM_UPDATE_CONTACT> upd;
                        upd.key = c->getkey();
                        upd.mask = CDM_STATE;
                        upd.state = c->get_state();
                        upd.send();
                        historian->reselect();
                    }
                }

                static void m_contact_props(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    SUMMON_DIALOG<dialog_contact_props_c>(UD_CONTACTPROPS, dialog_contactprops_params_s(ck));
                }

                static void m_export_history(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);

                    if (contact_c *c = contacts().find(ck))
                    {
                        if (c->gui_item)
                        {
                            ts::wstr_c downf = prf().download_folder();
                            path_expand_env(downf, c->contactidfolder());
                            ts::make_path(downf, 0);

                            ts::wstr_c n = from_utf8(c->get_name());
                            ts::fix_path(n, FNO_MAKECORRECTNAME);


                            ts::wstrings_c fns, fnsa;
                            ts::g_fileop->find(fns, CONSTWSTR("*.template"), false);

                            ts::tmp_array_inplace_t<ts::extension_s,1> es;

                            for (const ts::wstr_c &tn : fns)
                            {
                                ts::wstrings_c tnn(ts::fn_get_name_with_ext(tn), '.');
                                if (tnn.size() < 2) continue;
                                int exti = 0; if (tnn.size() > 2) exti = 1;
                                ts::wstr_c saveas(tnn.get(0));
                                ts::extension_s &e = es.add(); fnsa.add(tn);
                                e.desc = TTT("As $ file", 326) / saveas;
                                e.ext = tnn.get(exti);
                                e.desc.append(CONSTWSTR(" (*.")).append(e.ext).append_char(')');
                            }

                            ts::extensions_s exts(es.begin(), es.size());
                            exts.index = 0;

                            ts::wstr_c title = TTT("Export history",324);
                            ts::wstr_c fn = c->gui_item->getroot()->save_filename_dialog(downf, n, exts, title);

                            if (!fn.is_empty() && exts.index >= 0)
                            {
                                c->export_history(fnsa.get(exts.index), fn);
                            }
                        }
                    }

                }
            };

            if (!dialog_already_present(UD_CONTACTPROPS))
            {
                menu_c m;
                if (contact->is_meta() && contact->subcount() > 1) 
                {
                    menu_c mc = m.add_sub(TTT("Metacontact",145));
                    contact->subiterate([&](contact_c *c) {

                        ts::str_c text(c->get_name(false));
                        text_adapt_user_input(text);
                        if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                            text.append(CONSTASTR(" (")).append(ap->get_name()).append(CONSTASTR(")"));
                        mc.add( TTT("Detach: $",197) / from_utf8(text), 0, handlers::m_metacontact_detach, c->getkey().as_str() );
                    });

                }

                if (contact->getkey().is_group())
                    m.add(TTT("Leave group chat",304),0,handlers::m_delete,contact->getkey().as_str());
                else
                    m.add(TTT("Delete",85),0,handlers::m_delete,contact->getkey().as_str());
                if (!contact->getkey().is_group())
                    m.add(TTT("Contact properties",225),0,handlers::m_contact_props,contact->getkey().as_str());

                ts::wstrings_c fns;
                ts::g_fileop->find(fns, CONSTWSTR("*.template"), false);
                if (fns.size())
                {
                    m.add_separator();
                    m.add(TTT("Export history to file...", 325), 0, handlers::m_export_history, contact->getkey().as_str());
                }

                popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), m);
                popupmenu->leech(this);
            }

        } else if (CIR_METACREATE == role)
        {
            struct handlers
            {
                static void m_remove(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    contact_c * c = contacts().find(ck);
                    if (c)
                    {
                        gmsg<ISOGM_METACREATE> mca(ck); mca.state = gmsg<ISOGM_METACREATE>::REMOVE; mca.send();
                    }
                }
                static void m_mfirst(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    contact_c * c = contacts().find(ck);
                    if (c)
                    {
                        gmsg<ISOGM_METACREATE> mca(ck); mca.state = gmsg<ISOGM_METACREATE>::MAKEFIRST; mca.send();
                    }
                }
            };

            rectengine_c &pareng = HOLD(getparent()).engine();
            int nchild = pareng.children_count();


            menu_c m;
            if (nchild > 1 && pareng.get_child(0) != &getengine())
                m.add(TTT("Move to top",151), 0, handlers::m_mfirst, contact->getkey().as_str());
            m.add(TTT("Remove",148), 0, handlers::m_remove, contact->getkey().as_str());
            gui_popup_menu_c::show(menu_anchor_s(true), m);

        } else if (CIR_ME == role)
        {
            menu_c m;
            add_status_items( m );
            gui_popup_menu_c::show(menu_anchor_s(true), m);
        } else if (CIR_CONVERSATION_HEAD == role && !contact->getkey().is_self())
        {
            int curpww = g_app->minprotowidth;
            if ( protocols() ) curpww = protocols()->size.x;

            ts::irect ca = get_client_area();
            ca.rb.x -= contact_item_rite_margin();
            ca.lt.x = ca.rb.x - curpww;
            if (ca.inside(to_local(gui->get_cursor_pos())))
            {
                menu_c m;

                struct preproto_s
                {
                    contact_c *c;
                    tableview_t<active_protocol_s, pt_active_protocol>::row_s *row;
                };
                ts::tmp_tbuf_t<preproto_s> splist;

                contact->subiterate([&](contact_c *c) {
                    if (auto *row = prf().get_table_active_protocol().find<true>(c->getkey().protoid))
                    {
                        preproto_s &ap = splist.add();
                        ap.c = c;
                        ap.row = row;
                    }
                });

                splist.tsort<preproto_s>([](preproto_s *s1, preproto_s *s2)->bool
                {
                    if (s1->row->other.sort_factor == s2->row->other.sort_factor)
                        return s1->c->getkey().contactid < s2->c->getkey().contactid;
                    return s1->row->other.sort_factor < s2->row->other.sort_factor;
                });

                const contact_c *def = contact->subget_default();
                for (const preproto_s &s : splist)
                {
                    m.add(from_utf8(s.row->other.name), def == s.c ? MIF_MARKED : 0, DELEGATE(this,set_default_proto), s.c->getkey().as_str());
                }

                gui_popup_menu_c::show(menu_anchor_s(true), m);
            }
        }
        return false;

    }

    return __super::sq_evt(qp,rid,data);
}

void gui_contact_item_c::set_default_proto(const ts::str_c&cks)
{
    contact_key_s ck(cks);
    bool regen_prots = false;
    contact->subiterate([&](contact_c *c) {
        if (ck == c->getkey())
        {
            if ( !c->get_options().is(contact_c::F_DEFALUT) )
            {
                c->options().set(contact_c::F_DEFALUT);
                prf().dirtycontact(c->getkey());
                regen_prots = true;
                
            }
        } else
        {
            if (c->get_options().is(contact_c::F_DEFALUT))
            {
                c->options().clear(contact_c::F_DEFALUT);
                prf().dirtycontact(c->getkey());
                regen_prots = true;
            }
        }
    } );

    if (regen_prots)
        generate_protocols();
    getengine().redraw();
}

INLINE int statev(contact_state_e v)
{
    switch (v) //-V719
    {
        case CS_INVITE_RECEIVE:
            return 100;
        case CS_INVITE_SEND:
        case CS_REJECTED:
            return 10;
        case CS_ONLINE:
            return 50;
        case contact_state_check:
            return 40;
        case CS_WAIT:
            return 30;
        case CS_OFFLINE:
            return 1;
    }
    return 0;
}

bool gui_contact_item_c::is_after(gui_contact_item_c &ci)
{
    int mystate = statev(contact->get_meta_state()) + sort_power();
    int otherstate = statev(ci.contact->get_meta_state()) + ci.sort_power();
    if (otherstate > mystate) return true;

    return false;
}

bool gui_contact_item_c::redraw_now(RID, GUIPARAM)
{
    update_text();
    getengine().redraw();
    return true;
}

void gui_contact_item_c::redraw(float delay)
{
    DEFERRED_CALL( delay, DELEGATE(this, redraw_now), nullptr );
}

MAKE_CHILD<gui_contactlist_c>::~MAKE_CHILD()
{
    ASSERT(parent);
    MODIFY(get()).visible(true);
}

gui_contactlist_c::~gui_contactlist_c()
{
    if (gui)
        gui->delete_event( DELEGATE(this, on_filter_deactivate) );
}

void gui_contactlist_c::array_mode( ts::array_inplace_t<contact_key_s, 2> & arr_ )
{
    ASSERT(role != CLR_MAIN_LIST);
    arr = &arr_;
    refresh_array();
}

void gui_contactlist_c::refresh_array()
{
    ASSERT(role != CLR_MAIN_LIST && arr != nullptr);

    int count = getengine().children_count();
    int index = 0;
    for (int i = skipctl; i < count; ++i, ++index)
    {
        rectengine_c *ch = getengine().get_child(i);
        if (ch)
        {
            gui_contact_item_c *ci = ts::ptr_cast<gui_contact_item_c *>(&ch->getrect());

            loopcheg:
            if (index >= arr->size())
            {
                getengine().trunc_children(index);
                return;
            }

            const contact_key_s &ck = arr->get(index);
            if (ci->getcontact().getkey() == ck)
                continue;

            contact_c * c = contacts().find(ck);
            if (!c)
            {
                arr->remove_slow(index);
                goto loopcheg;
            }
            ci->setcontact(c);
            MODIFY(*ci).active(false);
        }
    }

    for (;index < arr->size();)
    {
        const contact_key_s &ck = arr->get(index);
        contact_c * c = contacts().find(ck);
        if (!c)
        {
            arr->remove_slow(index);
            continue;;
        }
        MAKE_CHILD<gui_contact_item_c>(getrid(), c->get_historian()) << CIR_METACREATE;
        ++index;
    }
}

/*virtual*/ ts::ivec2 gui_contactlist_c::get_min_size() const
{
    ts::ivec2 sz(200);
    sz.y += skip_top_pixels + skip_bottom_pixels;
    return sz;
}

/*virtual*/ void gui_contactlist_c::created()
{
    set_theme_rect(CONSTASTR("contacts"), false);
    __super::created();

    if (role == CLR_MAIN_LIST)
    {
        recreate_ctls();
        contacts().update_meta();
    } else if (role == CLR_NEW_METACONTACT_LIST)
    {
        skip_top_pixels = 0;
        skip_bottom_pixels = 0;
        skipctl = 0;
    }
}

void gui_contactlist_c::recreate_ctls(bool focus_filter)
{
    if (filter)
    {
        TSDEL(filter);
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, on_filter_deactivate), nullptr );
    }
    if (addcbtn) TSDEL(addcbtn);
    if (addgbtn) TSDEL(addgbtn);
    if (self) TSDEL(self);

    if (button_desc_s *baddc = gui->theme().get_button(CONSTASTR("addcontact")))
    {
        struct handlers
        {
            static bool summon_addcontacts(RID, GUIPARAM)
            {
                if (prf().is_loaded())
                    SUMMON_DIALOG<dialog_addcontact_c>(UD_ADDCONTACT);
                else
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_ERROR,
                    loc_text(loc_please_create_profile)
                    ));
                return true;
            }
            static bool summon_addgroup(RID, GUIPARAM)
            {
                if (prf().is_loaded())
                    SUMMON_DIALOG<dialog_addgroup_c>(UD_ADDGROUP);
                else
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    DT_MSGBOX_ERROR,
                    loc_text(loc_please_create_profile)
                    ));
                return true;
            }
        };

        button_desc_s *baddg = gui->theme().get_button(CONSTASTR("addgroup"));
        int nbuttons = baddg ? 2 : 1;

        flags.set(F_NO_LEECH_CHILDREN);

        addcbtn = MAKE_CHILD<gui_button_c>(getrid());
        addcbtn->tooltip(TOOLTIP(TTT("Add contact",64)));
        addcbtn->set_face_getter(BUTTON_FACE(addcontact));
        addcbtn->set_handler(handlers::summon_addcontacts, nullptr);
        addcbtn->leech(TSNEW(leech_dock_bottom_center_s, baddc->size.x, baddc->size.y, -10, 10, 0, nbuttons));
        MODIFY(*addcbtn).zindex(1.0f).visible(true);
        getengine().child_move_to(0, &addcbtn->getengine());

        if ( !prf().is_any_active_ap() )
        {
            addcbtn->tooltip(TOOLTIP(loc_text(loc_nonetwork)));
            addcbtn->disable();
        }

        if (baddg)
        {
            addgbtn = MAKE_CHILD<gui_button_c>(getrid());
            addgbtn->tooltip(TOOLTIP(TTT("Add group chat",243)));
            addgbtn->set_face_getter(BUTTON_FACE(addgroup));
            addgbtn->set_handler(handlers::summon_addgroup, nullptr);
            addgbtn->leech(TSNEW(leech_dock_bottom_center_s, baddg->size.x, baddg->size.y, -10, 10, 1, 2));
            MODIFY(*addgbtn).zindex(1.0f).visible(true);
            getengine().child_move_to(1, &addgbtn->getengine());

            if (!prf().is_any_active_ap(PF_GROUP_CHAT))
            {
                addgbtn->tooltip(TOOLTIP(TTT("No any active networks with group chat support",247)));
                addgbtn->disable();
            }
        }

        self = MAKE_CHILD<gui_contact_item_c>(getrid(), &contacts().get_self()) << CIR_ME;
        self->leech(TSNEW(leech_dock_top_s, g_app->mecontactheight));
        self->protohit();
        MODIFY(*self).zindex(1.0f).visible(true);
        getengine().child_move_to(nbuttons, &self->getengine());

        flags.clear(F_NO_LEECH_CHILDREN);

        int other_ctls = 1;
        if (prf().get_options().is(UIOPT_SHOW_SEARCH_BAR))
        {
            other_ctls = 2;
            filter = MAKE_CHILD<gui_filterbar_c>(getrid());
            getengine().child_move_to(nbuttons + 1, &filter->getengine());
            DEBUGCODE(skip_top_pixels = 0);
            update_filter_pos();
            MODIFY(*filter).zindex(1.0f).visible(true);

            ASSERT(skip_top_pixels > 0); // it will be calculated via update_filter_pos

            if (focus_filter)
                filter->focus_edit();
        } else
        {
            skip_top_pixels = gui->theme().conf().get_int(CONSTASTR("cltop"), 70);
        }
       
        getengine().z_resort_children();

        skipctl = other_ctls + nbuttons;
        //skip_top_pixels = gui->theme().conf().get_int(CONSTASTR("cltop"), 70) + filter->get_height_by_width( get_client_area().width() );
        skip_bottom_pixels = gui->theme().conf().get_int(CONSTASTR("clbottom"), 70);
        return;
    }
    skipctl = 0;
    gui->repos_children( this );
}

bool gui_contactlist_c::i_leeched( guirect_c &to )
{
    if (flags.is(F_NO_LEECH_CHILDREN))
        return false;

    return true;
}

bool gui_contactlist_c::on_filter_deactivate(RID, GUIPARAM)
{
    ts::safe_ptr<rectengine_c> active = g_app->active_contact_item ? &g_app->active_contact_item->getengine() : nullptr;

    if (!active)
        active = get_first_contact_item();


    contacts().iterate_meta_contacts([](contact_c *c)->bool{
    
        bool redraw = false;
        if (c->is_full_search_result())
        {
            c->full_search_result(false);
            redraw = true;
        }
        if (c->gui_item)
        {
            MODIFY(*c->gui_item).visible(true);
            if (redraw)
            {
                c->gui_item->update_text();
                c->redraw();
            }
        }
        return true;
    });

    if (active)
        scroll_to_child(active, false);

    gmsg<ISOGM_REFRESH_SEARCH_RESULT>().send();

    return true;
}

void gui_contactlist_c::update_filter_pos()
{
    ASSERT(!filter.expired() && !self.expired());
    ts::irect cla = get_client_area();
    int claw = cla.width();
    int fh = claw ? filter->get_height_by_width( claw ) : 20;
    int selfh = gui->theme().conf().get_int(CONSTASTR("cltop"), 70);
    MODIFY(*filter.get()).pos( cla.lt.x, cla.lt.y + selfh ).size( claw, fh );

    int otp = skip_top_pixels;
    skip_top_pixels = selfh + fh;
    if (skip_top_pixels != otp)
        gui->repos_children(this);
}

bool gui_contactlist_c::filter_proc(system_query_e qp, evt_data_s &data)
{
    if (qp == SQ_PARENT_RECT_CHANGED)
    {
        update_filter_pos();
        return false;
    }

    return false;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_CALL_STOPED> &p)
{
    p.subcontact->redraw();
    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_TYPING> &p)
{
    if (prf().get_options().is(UIOPT_SHOW_TYPING_CONTACT))
        if (contact_c *c = contacts().find(p.contact))
            if (contact_c *historian = c->get_historian())
                if (historian->gui_item)
                    historian->gui_item->typing();

    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_SAVED>&ch)
{
    if (ch.tabi == pt_active_protocol)
        g_app->recreate_ctls();
    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_PROTO_LOADED>&ch)
{
    g_app->recreate_ctls();
    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass > 0 && self)
    {
        switch (ch.sp)
        {
            case PP_USERNAME:
            case PP_USERSTATUSMSG:
                if (0 != ch.protoid)
                    break;
            case PP_NETWORKNAME:
                self->update_text();
                break;
        }
    } else if ( ch.pass == 0 && ch.protoid == 0 && self )
    {
        switch (ch.sp)
        {
            case PP_USERNAME:
            case PP_USERSTATUSMSG:
                self->update_text();
                break;
        }
    }

    if (ch.pass == 0 && self)
        if (PP_ONLINESTATUS == ch.sp)
            self->getengine().redraw();

    if (ch.pass == 0)
        if (PP_PROFILEOPTIONS == ch.sp && (ch.bits & UIOPT_SHOW_SEARCH_BAR))
            recreate_ctls(true);

    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<GM_DRAGNDROP> &dnda)
{
    if (dnda.a == DNDA_CLEAN)
    {
        if (dndtarget) dndtarget->target(false);
        dndtarget = nullptr;
        return 0;
    }
    gui_contact_item_c *ciproc = dynamic_cast<gui_contact_item_c *>(gui->dragndrop_underproc());
    if (!ciproc) return 0;
    if (dnda.a == DNDA_DROP)
    {
        if (dndtarget) dndtarget->on_drop(ciproc);
        dndtarget = nullptr;
    }

    ts::irect rect = gui->dragndrop_objrect();
    int area = rect.area() / 3;
    rectengine_c *yo = nullptr;
    int count = getengine().children_count();
    for (int i = skipctl; i < count; ++i)
    {
        rectengine_c *ch = getengine().get_child(i);
        if (ch)
        {
            const guirect_c &cirect = ch->getrect();
            if (!cirect.getprops().is_visible()) continue;
            if (&cirect == ciproc) continue;
            int carea = cirect.getprops().screenrect().intersect_area( rect );
            if (carea > area)
            {
                const gui_contact_item_c *ci = ts::ptr_cast<const gui_contact_item_c *>(&cirect);
                if (ci->allow_drop())
                {
                    area = carea;
                    yo = ch;
                }
            }
        }
    }
    if (yo)
    {
        gui_contact_item_c *ci = ts::ptr_cast<gui_contact_item_c *>(&yo->getrect());
        if (dndtarget != ci)
        {
            if (dndtarget)
                dndtarget->target(false);
            ci->target(true);
            dndtarget = ci;
        }
    } else if (dndtarget)
    {
        dndtarget->target(false);
        dndtarget = nullptr;
    }

    return yo ? GMRBIT_ACCEPTED : 0;
}

ts::uint32 gui_contactlist_c::gm_handler( gmsg<ISOGM_V_UPDATE_CONTACT> & c )
{
    if (c.contact->get_historian()->getkey().is_self())
    {
        if (self && self->contacted())
            if (ASSERT(c.contact == &self->getcontact() || c.contact->getmeta() == &self->getcontact()))
                self->update_text();
    
        return 0;
    }

    int count = getengine().children_count();
    for(int i=skipctl;i<count;++i)
    {
        rectengine_c *ch = getengine().get_child(i);
        if (ch)
        {
            gui_contact_item_c *ci = ts::ptr_cast<gui_contact_item_c *>(&ch->getrect());
            bool same = c.contact == &ci->getcontact();
            if (same && c.contact->get_state() == CS_ROTTEN)
            {
                TSDEL(ch);
                getengine().redraw();
                gui->dragndrop_update(nullptr);
                return 0;
            }
            if (same || ci->getcontact().subpresent( c.contact->getkey() ))
            {
                ci->update_text();
                gui->dragndrop_update(ci);
                if (same || !ci->getcontact().getkey().is_group())
                    return 0;
            }
        }
    }

    if (role == CLR_MAIN_LIST && c.contact->get_state() != CS_UNKNOWN)
    {
        ASSERT( c.contact->get_historian()->get_state() != CS_UNKNOWN );
        MAKE_CHILD<gui_contact_item_c>(getrid(), c.contact->get_historian());
    }

    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_DO_POSTEFFECT> &f)
{
    if (f.bits & application_c::PEF_RECREATE_CTLS)
        recreate_ctls();
    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<GM_HEARTBEAT> &)
{
    if (prf().sort_tag() != sort_tag && role == CLR_MAIN_LIST && gui->dragndrop_underproc() == nullptr)
    {
        auto swap_them = []( rectengine_c *e1, rectengine_c *e2 )->bool
        {
            if (e1 == nullptr) return false;
            gui_contact_item_c * ci1 = dynamic_cast<gui_contact_item_c *>( &e1->getrect() );
            if (ci1 == nullptr || ci1->getrole() != CIR_LISTITEM) return false;
            if (e2 == nullptr) return false;
            gui_contact_item_c * ci2 = dynamic_cast<gui_contact_item_c *>(&e2->getrect());
            if (ci2 == nullptr || ci2->getrole() != CIR_LISTITEM) return false;
            return ci1->is_after(*ci2);
        };

        if (getengine().children_sort( (rectengine_c::SWAP_TESTER)swap_them ))
        {
            if (g_app->active_contact_item)
                scroll_to_child(&g_app->active_contact_item->getengine(), false);

            gui->repos_children(this);
        }
        sort_tag = prf().sort_tag();
    }
    return 0;
}

/*virtual*/ void gui_contactlist_c::children_repos_info(cri_s &info) const
{
    info.area = get_client_area();
    info.area.lt.y += skip_top_pixels;
    info.area.rb.y -= skip_bottom_pixels;

    info.from = skipctl;
    info.count = getengine().children_count() - skipctl;
}

/*virtual*/ bool gui_contactlist_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT((getrid() >> rid) || HOLD(rid)().is_root())) // child?
    {
        if (filter && rid == filter->getrid())
            return filter_proc(qp, data);

        return __super::sq_evt(qp,rid,data);
    }
    return __super::sq_evt(qp, rid, data);
}
