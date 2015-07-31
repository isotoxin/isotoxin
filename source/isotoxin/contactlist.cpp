#include "isotoxin.h"

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
            contact->gui_item = this;
            g_app->need_recalc_unread( contact->getkey() );
        }
}

gui_contact_item_c::~gui_contact_item_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, update_buttons));
        gui->delete_event(DELEGATE(this, redraw_now));
    }
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
    /*virtual*/ void i_leeched( guirect_c &to ) override 
    {
        __super::i_leeched(to);
        evt_data_s d;
        sq_evt(SQ_PARENT_RECT_CHANGED, owner->getrid(), d);
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
                    gmsg<ISOGM_CHANGED_PROFILEPARAM>(0, PP_USERNAME, hstuff().curedit).send();
            }
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
                gmsg<ISOGM_CHANGED_PROFILEPARAM>(0, PP_USERSTATUSMSG, hstuff().curedit).send();
        }

    }
    DEFERRED_UNIQUE_CALL( 0, DELEGATE( this, update_buttons ), nullptr );
    return true;
}

bool gui_contact_item_c::cancel_edit( RID, GUIPARAM)
{
    flags.clear(F_EDITNAME|F_EDITSTATUS);
    DEFERRED_UNIQUE_CALL( 0, DELEGATE( this, update_buttons ), nullptr );
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


    if (contact && !contact->getkey().is_self())
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
                if (c->is_av()) now_disabled = true;
            }
        });

        gui_button_c &b_call = MAKE_CHILD<gui_button_c>(getrid());
        hstuff().call_button = &b_call;
        b_call.set_face_getter(BUTTON_FACE_PRELOADED(callb));
        b_call.tooltip(TOOLTIP(TTT("Звонок", 140)));

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
            b_call.tooltip(TOOLTIP(TTT("Звонок не поддерживается",141)));
        } else if (0 == (features_online & PF_AUDIO_CALLS))
        {
            b_call.disable();
            b_call.tooltip(TOOLTIP(TTT("Абонент не в сети",143)));
        }

        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            gmsg<ISOGM_UPDATE_BUTTONS>().send();
        DEFERRED_EXECUTION_BLOCK_END(0)

    }

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
                if (c_call == nullptr || (cdef == c && c_call->get_state() != CS_ONLINE))
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
    if ( CIR_LISTITEM == role || CIR_ME == role || CIR_METACREATE == role )
    {
        bool o = getprops().is_active();
        bool n = contact == c.contact;
        if (o != n)
        {
            MODIFY(*this).active(n);
            update_text();
        }
    }
    return 0;
}

ts::uint32 gui_contact_item_c::gm_handler(gmsg<ISOGM_SOMEUNREAD> & c)
{
    if (n_unread > 0 || (CIR_CONVERSATION_HEAD == role && contact && contact->achtung()))
        return GMRBIT_ACCEPTED | GMRBIT_ABORT;

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
        update_buttons();
    }
}

void gui_contact_item_c::update_text()
{
    protocols.clear();
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
                newtext.append(CONSTASTR("<br>")).append(to_utf8(TTT("временный групповой чат",256)));

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
            if (live == 0 && count > 0)
                newtext = colorize( to_utf8(TTT("Удален",77)).as_sptr(), get_default_text_color(0) );
            else if (count == 0)
                newtext = colorize( to_utf8(TTT("Пустой",78)).as_sptr(), get_default_text_color(0) );
            else if (rej > 0)
                newtext = colorize( to_utf8(TTT("Отказано",79)).as_sptr(), get_default_text_color(0) );
            else if (invsend > 0)
            {
                newtext = contact->get_customname();
                if (newtext.is_empty()) newtext = contact->get_name();
                text_adapt_user_input(newtext);
                if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                newtext.append( colorize(to_utf8(TTT("Запрос отправлен", 88)).as_sptr(), get_default_text_color(0)) );
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
                        newtext.append( colorize( to_utf8(TTT("Требуется подтверждение",153)).as_sptr(), get_default_text_color(0) ) );
                        t2.clear();
                    }

                    if (wait)
                    {
                        if (!newtext.is_empty()) newtext.append(CONSTASTR("<br>"));
                        newtext.append(colorize(to_utf8(TTT("Ожидание",154)).as_sptr(), get_default_text_color(0)));
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


void gui_contact_item_c::updrect(void *, int r, const ts::ivec2 &p)
{
    if (ASSERT(r < rbtn::EB_MAX) && prf().is_loaded())
    {
        hstuff().updr[r].p = root_to_local(p);
        hstuff().updr[r].updated = true;
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, update_buttons), (void *)1 );
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
        ts::tmp_tbuf_t<int> c2a;

        ondr->contact->subiterate([&](contact_c *c) {
            if ( contact->getkey().protoid == c->getkey().protoid )
                c2a.add( c->getkey().contactid );
        });

        if (c2a.count() == 0)
        {
            SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                gui_isodialog_c::title(DT_MSGBOX_WARNING),
                TTT("В групповой чат можно добавлять только контакты из той же сети.",255)
                ));
        } else if (active_protocol_c *ap = prf().ap(contact->getkey().protoid))
        {
            for( int cid : c2a )
                ap->join_group_chat( contact->getkey().contactid, cid );
        }

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
    drawcollector dcoll;
    gui_contact_item_c &dnd = MAKE_ROOT<gui_contact_item_c>(dcoll, contact);
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
    {
        getengine().redraw();
        //if (is_protohit())
        //    DMSG("protohit" << contact->getkey() << true);
        //else
        //    DMSG("protohit" << contact->getkey() << false);
    }

}

void gui_contact_item_c::generate_protocols()
{
    protocols.clear();
    if (!contact || contact->getkey().is_group()) return;

    protocols.set(CONSTWSTR("<p=r>"));

    const contact_c *def = CIR_CONVERSATION_HEAD == role && !contact->getkey().is_self() ? contact->subget_default() : nullptr;
    contact->subiterate([&](contact_c *c) {

        if (auto *row = prf().get_table_active_protocol().find<true>(c->getkey().protoid))
        {
            //if (0 == (row->other.options & active_protocol_data_s::O_SUSPENDED))
            {
                if (c->get_state() == CS_ONLINE)
                {
                    protocols.append(CONSTWSTR("<nl><shadow>")).append(maketag_color<ts::wchar>(get_default_text_color(1)));
                    if (def == c) protocols.append(CONSTWSTR(" +")); else protocols.append_char(' ');
                    protocols.append(from_utf8(row->other.name));
                    protocols.append(CONSTWSTR("</color></shadow> "));
                } else
                {
                    protocols.append(CONSTWSTR("<nl>")).append(maketag_color<ts::wchar>(get_default_text_color(2)));
                    if (def == c) protocols.append(CONSTWSTR(" +")); else protocols.append_char(' ');
                    protocols.append(from_utf8(row->other.name));
                    protocols.append(CONSTWSTR("</color> "));
                }
            //} else
            //{
            //    protocols.append(CONSTWSTR("<nl><s> ")).append( maketag_color<ts::wchar>( get_default_text_color(3) ) );
            //    protocols.append( row->other.name );
            //    protocols.append(CONSTWSTR("</color> </s>"));
            }
        }
    });

}

void gui_contact_item_c::draw_online_state_text(draw_data_s &dd)
{
    if (protocols.is_empty())
        generate_protocols();

    if (!protocols.is_empty())
    {

        text_draw_params_s tdp;

        ts::flags32_s f; f.setup(ts::TO_VCENTER|ts::TO_LINE_END_ELLIPSIS);
        tdp.textoptions = &f;

        getengine().draw(protocols, tdp);
    }
}

int gui_contact_item_c::contact_item_rite_margin()
{
    if (contact && contact->getkey().is_self()) return 5;
    return g_app->buttons().callb->size.x + 15;
}

/*virtual*/ bool gui_contact_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
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
        if (m_engine)
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

            if (( force_state_icon || (flags.is(F_PROTOHIT) && st != CS_ROTTEN)) && bdesc)
                bdesc->draw( m_engine.get(), bst, ca + ts::ivec2(shiftstateicon.x, shiftstateicon.y), button_desc_s::ARIGHT | button_desc_s::ABOTTOM );

            if (contact)
            {
                text_draw_params_s tdp;
                
                bool achtung = (CIR_CONVERSATION_HEAD == role) ? false : contact->achtung();
                bool drawnotify = false;
                if (n_unread > 0 || achtung)
                {
                    drawnotify = true;
                    if (CIR_LISTITEM == role && g_app->flashingicon())
                    {
                        drawnotify = g_app->flashiconflag();
                        g_app->flashredraw( getrid() );
                    }
                }

                int ritem = 0;
                bool draw_ava = !contact->getkey().is_self();
                bool draw_proto = !contact->getkey().is_group();
                //bool draw_btn = true;
                if (CIR_CONVERSATION_HEAD == role)
                {
                    ritem = contact_item_rite_margin() + (draw_proto ? g_app->protowidth : 0);
                    ts::irect cac(ca);

                    int x_offset = contact->get_avatar() ? g_app->buttons().icon[CSEX_UNKNOWN]->size.x : g_app->buttons().icon[contact->get_meta_gender()]->size.x;

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
                    if (draw_proto && w > curw)
                    {
                        curw += g_app->protowidth;
                        draw_proto = false;
                    }
                    // never hide call button
                    //if (w > curw)
                    //{
                    //    draw_btn = false;
                    //}
                }
                
                int x_offset = 0;
                if (draw_ava)
                {
                    if (const avatar_s *ava = contact->get_avatar())
                    {
                        int y = (ca.size().y - ava->info().sz.y) / 2;
                        m_engine->draw(ca.lt + ts::ivec2(y), *ava, ts::irect(0, ava->info().sz), ava->alpha_pixels);
                        x_offset = g_app->buttons().icon[CSEX_UNKNOWN]->size.x;
                    }
                    else
                    {
                        button_desc_s *icon = contact->getkey().is_group() ? g_app->buttons().groupchat : g_app->buttons().icon[contact->get_meta_gender()];
                        icon->draw(m_engine.get(), button_desc_s::NORMAL, ca, button_desc_s::ALEFT | button_desc_s::ATOP | button_desc_s::ABOTTOM);
                        x_offset = icon->size.x;
                    }
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
                            ca.rb.x += g_app->protowidth;
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
                        tdp.textoptions = &f;
                        tdp.forecolor = nullptr;
                        tdp.rectupdate = DELEGATE( this, updrect );
                        draw(dd, tdp);

                        if (CIR_CONVERSATION_HEAD == role && draw_proto)
                        {
                            dd.offset.x = oldxo + dd.size.x + 5;
                            dd.size.x = g_app->protowidth;
                            draw_online_state_text(dd);
                        }
                    }
                    m_engine->end_draw();
                }
                if (drawnotify)
                {
                    if (n_unread > 99) n_unread = 99;
                    button_desc_s *unread = g_app->buttons().unread;

                    ts::ivec2 pos = unread->draw(m_engine.get(), button_desc_s::NORMAL, noti_draw_area, button_desc_s::ALEFT | button_desc_s::ABOTTOM);

                    if (!contact->is_ringtone() || contact->is_ringtone_blink())
                    {
                        ts::flags32_s f; f.setup(ts::TO_VCENTER | ts::TO_HCENTER);
                        tdp.textoptions = &f;
                        tdp.forecolor = unread->colors + button_desc_s::NORMAL;

                        draw_data_s &dd = m_engine->begin_draw();
                        dd.offset += pos;
                        dd.size = unread->size;
                        if (contact->is_ringtone())
                            m_engine->draw(ts::wstr_c(CONSTWSTR("<img=call>")), tdp);
                        else
                        {
                            if (st == CS_INVITE_RECEIVE || n_unread == 0)
                                m_engine->draw(CONSTWSTR("!"), tdp);
                            else
                                m_engine->draw(ts::wstr_c().set_as_uint(n_unread), tdp);
                        }
                        m_engine->end_draw();
                    }
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
        if ( CIR_LISTITEM == role && !contact->getkey().is_group() ) 
        {
            gmsg<ISOGM_METACREATE> mca(contact->getkey());
            mca.state = gmsg<ISOGM_METACREATE>::CHECKINLIST;
            if (!mca.send().is(GMRBIT_ACCEPTED))
                gui->dragndrop_lb( this );
        }

        return true;
    case SQ_MOUSE_LUP:
        if (flags.is(F_LBDN))
        {
            if (CIR_CONVERSATION_HEAD != role)
            {
                gmsg<ISOGM_SELECT_CONTACT>(contact).send();
            }

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
                    contacts().get_self().reselect(true);
                }
                static void m_delete(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    contact_c * c = contacts().find(ck);
                    if (c)
                    {
                        ts::wstr_c txt;
                        if ( c->getkey().is_group() )
                            txt = TTT("Покинуть групповой чат?[br]$",258) / from_utf8(c->get_description());
                        else
                            txt = TTT("Будет полностью удален контакт:[br]$",84) / from_utf8(c->get_description());
                        
                        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                            gui_isodialog_c::title(DT_MSGBOX_WARNING),
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
                        historian->reselect(true);
                    }
                }

                static void m_contact_props(const ts::str_c&cks)
                {
                    contact_key_s ck(cks);
                    SUMMON_DIALOG<dialog_contact_props_c>(UD_CONTACTPROPS, dialog_contactprops_params_s(ck));
                }
            };

            if (!dialog_already_present(UD_CONTACTPROPS))
            {
                menu_c m;
                if (contact->is_meta() && contact->subcount() > 1) 
                {
                    menu_c mc = m.add_sub(TTT("Метаконтакт", 145));
                    contact->subiterate([&](contact_c *c) {

                        ts::str_c text(c->get_name(false));
                        text_adapt_user_input(text);
                        if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                            text.append(CONSTASTR(" (")).append(ap->get_name()).append(CONSTASTR(")"));
                        mc.add( TTT("Отделить: $",197) / from_utf8(text), 0, handlers::m_metacontact_detach, c->getkey().as_str() );
                    });

                }

                m.add(TTT("Удалить",85),0,handlers::m_delete,contact->getkey().as_str());
                m.add(TTT("Настройки контакта",223),0,handlers::m_contact_props,contact->getkey().as_str());
                popupmenu = &gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(),0), m);
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
                m.add(TTT("Сделать первым",151), 0, handlers::m_mfirst, contact->getkey().as_str());
            m.add(TTT("Убрать",148), 0, handlers::m_remove, contact->getkey().as_str());
            gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), m);

        } else if (CIR_ME == role)
        {
            struct handlers
            {
                static void m_ost(const ts::str_c&ost)
                {
                    contact_online_state_e cos_ = (contact_online_state_e)ost.as_uint();

                    contacts().get_self().subiterate( [&](contact_c *c) {
                        c->set_ostate(cos_);
                    } );
                    contacts().get_self().set_ostate(cos_);
                    gmsg<ISOGM_CHANGED_PROFILEPARAM>(0, PP_ONLINESTATUS).send();
                }
            };

            contact_online_state_e ost = contacts().get_self().get_ostate();

            menu_c m;
            m.add(TTT("Онлайн",244), COS_ONLINE == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_ONLINE));
            m.add(TTT("Отошёл",245), COS_AWAY == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_AWAY));
            m.add(TTT("Занят",246), COS_DND == ost ? MIF_MARKED : 0, handlers::m_ost, ts::amake<uint>(COS_DND));
            gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), m);
        } else if (CIR_CONVERSATION_HEAD == role && !contact->getkey().is_self())
        {
            ts::irect ca = get_client_area();
            ca.rb.x -= contact_item_rite_margin();
            ca.lt.x = ca.rb.x - g_app->protowidth;
            if (ca.inside(to_local(gui->get_cursor_pos())))
            {
                menu_c m;

                const contact_c *def = contact->subget_default();
                contact->subiterate([&](contact_c *c) {
                    if (auto *row = prf().get_table_active_protocol().find<true>(c->getkey().protoid))
                    {
                        m.add(from_utf8(row->other.name), def == c ? MIF_MARKED : 0, DELEGATE(this,set_default_proto), c->getkey().as_str());
                    }
                });

                gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), m);
            }
        }
        return false;

    }

    return __super::sq_evt(qp,rid,data);
}

void gui_contact_item_c::set_default_proto(const ts::str_c&cks)
{
    contact_key_s ck(cks);

    contact->subiterate([&](contact_c *c) {
        if (ck == c->getkey())
        {
            if ( !c->get_options().is(contact_c::F_DEFALUT) )
            {
                c->options().set(contact_c::F_DEFALUT);
                prf().dirtycontact(c->getkey());
                protocols.clear();
                
            }
        } else
        {
            if (c->get_options().is(contact_c::F_DEFALUT))
            {
                c->options().clear(contact_c::F_DEFALUT);
                prf().dirtycontact(c->getkey());
                protocols.clear();
            }
        }
    } );
    getengine().redraw();
}

INLINE int statev(contact_state_e v)
{
    switch (v)
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

void gui_contactlist_c::recreate_ctls()
{
    if (addcbtn) TSDEL(addcbtn);
    if (addgbtn) TSDEL(addgbtn);
    if (self) TSDEL(self);

    if (button_desc_s *baddc = gui->theme().get_button(CONSTASTR("addcontact")))
    {
        AUTOCLEAR(flags, F_NO_REPOS);

        struct handlers
        {
            static ts::wstr_c please_create_profile()
            {
                return TTT("Пожалуйста, создайте профиль", 144);
            }
            static bool summon_addcontacts(RID, GUIPARAM)
            {
                if (prf().is_loaded())
                    SUMMON_DIALOG<dialog_addcontact_c>(UD_ADDCONTACT);
                else
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_ERROR),
                    please_create_profile()
                    ));
                return true;
            }
            static bool summon_addgroup(RID, GUIPARAM)
            {
                if (prf().is_loaded())
                    SUMMON_DIALOG<dialog_addgroup_c>(UD_ADDGROUP);
                else
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_ERROR),
                    please_create_profile()
                    ));
                return true;
            }
        };

        button_desc_s *baddg = gui->theme().get_button(CONSTASTR("addgroup"));
        int nbuttons = baddg ? 2 : 1;


        addcbtn = MAKE_CHILD<gui_button_c>(getrid());
        addcbtn->tooltip(TOOLTIP(TTT("Добавить контакт", 64)));
        addcbtn->set_face_getter(BUTTON_FACE(addcontact));
        addcbtn->set_handler(handlers::summon_addcontacts, nullptr);
        addcbtn->leech(TSNEW(leech_dock_bottom_center_s, baddc->size.x, baddc->size.y, -10, 10, 0, nbuttons));
        MODIFY(*addcbtn).zindex(1.0f).visible(true);
        getengine().child_move_to(0, &addcbtn->getengine());

        if (baddg)
        {
            addgbtn = MAKE_CHILD<gui_button_c>(getrid());
            addgbtn->tooltip(TOOLTIP(TTT("Добавить групповой чат",243)));
            addgbtn->set_face_getter(BUTTON_FACE(addgroup));
            addgbtn->set_handler(handlers::summon_addgroup, nullptr);
            addgbtn->leech(TSNEW(leech_dock_bottom_center_s, baddg->size.x, baddg->size.y, -10, 10, 1, 2));
            MODIFY(*addgbtn).zindex(1.0f).visible(true);
            getengine().child_move_to(1, &addgbtn->getengine());

            bool support_groupchats = false;
            prf().iterate_aps( [&](const active_protocol_c &ap) {
                if (ap.get_features() & PF_GROUP_CHAT)
                    support_groupchats = true;
            } );

            if (!support_groupchats)
            {
                addgbtn->tooltip(TOOLTIP(TTT("Ни одна из активных сетей не поддерживает групповые чаты",247)));
                addgbtn->disable();
            }
        }

        self = MAKE_CHILD<gui_contact_item_c>(getrid(), &contacts().get_self()) << CIR_ME;
        self->leech(TSNEW(leech_dock_top_s, g_app->mecontactheight));
        self->protohit();
        MODIFY(*self).zindex(1.0f).visible(true);
        getengine().child_move_to(2, &self->getengine());

        getengine().resort_children();

        skipctl = 1 + nbuttons;
        skip_top_pixels = gui->theme().conf().get_int(CONSTASTR("cltop"), 70);
        skip_bottom_pixels = gui->theme().conf().get_int(CONSTASTR("clbottom"), 70);

        return;
    }
    skipctl = 0;
    children_repos();

}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_SAVED>&ch)
{
    if (ch.tabi == pt_active_protocol)
        recreate_ctls();

    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_PROTO_LOADED>&ch)
{
    recreate_ctls();

    return 0;
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    if (ch.pass > 0 && self)
    {
        switch (ch.pp)
        {
            case PP_USERNAME:
            case PP_USERSTATUSMSG:
                if (0 != ch.protoid)
                    break;
            case PP_NETWORKNAME:
                self->update_text();
                break;
        }
    }
    if (ch.pass == 0 && self)
        if (PP_ONLINESTATUS == ch.pp)
        self->getengine().redraw();
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
            if (&cirect == ciproc) continue;
            int carea = cirect.getprops().screenrect().intersect_area( rect );
            if (carea > area)
            {
                area = carea;
                yo = ch;
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

ts::uint32 gui_contactlist_c::gm_handler(gmsg<GM_HEARTBEAT> &)
{
    if (prf().sort_tag() != sort_tag && role == CLR_MAIN_LIST && gui->dragndrop_underproc() == nullptr)
    {
        struct ss
        {
            static bool swap_them( rectengine_c *e1, rectengine_c *e2 )
            {
                if (e1 == nullptr) return false;
                gui_contact_item_c * ci1 = dynamic_cast<gui_contact_item_c *>( &e1->getrect() );
                if (ci1 == nullptr || ci1->getrole() != CIR_LISTITEM) return false;
                if (e2 == nullptr) return false;
                gui_contact_item_c * ci2 = dynamic_cast<gui_contact_item_c *>(&e2->getrect());
                if (ci2 == nullptr || ci2->getrole() != CIR_LISTITEM) return false;
                return ci1->is_after(*ci2);
            }
        };

        if (getengine().children_sort( ss::swap_them ))
        {
            children_repos();
        }
        sort_tag = prf().sort_tag();
    }
    return 0;
}

/*virtual*/ void gui_contactlist_c::children_repos_info(cri_s &info) const
{
    info.area = get_client_area();
    info.area.lt.y += skip_top_pixels; // TODO : top controls
    info.area.rb.y -= skip_bottom_pixels; // TODO : bottom controls

    info.from = skipctl;
    info.count = getengine().children_count() - skipctl;
    info.areasize = 0;
}

/*virtual*/ bool gui_contactlist_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid() && ASSERT(getrid() >> rid)) // child?
    {
        return __super::sq_evt(qp,rid,data);
    }
    return __super::sq_evt(qp, rid, data);
}
