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
        if (ASSERT(contact->is_multicontact()))
        {
            contact->gui_item = this;
            g_app->need_recalc_unread( contact->getkey() );
        }
}

gui_contact_item_c::~gui_contact_item_c()
{
    if (gui) gui->delete_event(DELEGATE(this, update_buttons));
}

/*virtual*/ ts::ivec2 gui_contact_item_c::get_min_size() const
{
    ts::ivec2 subsize(0);
    if (CIR_DNDOBJ == role)
    {
        if (const theme_rect_s *thr = themerect())
            subsize = thr->maxcutborder.lt + thr->maxcutborder.rb;
    }
    return ts::ivec2(60) - subsize;
}

/*virtual*/ ts::ivec2 gui_contact_item_c::get_max_size() const
{
    ts::ivec2 m = __super::get_max_size();
    m.y = get_min_size().y;
    return m;
}

namespace
{

enum
{
    EB_NAME,
    EB_STATUS,

    EB_MAX
};

struct ebutton
{
    RID brid;
    ts::ivec2 p;
    bool updated;
};

struct head_stuff_s
{
    ts::ivec2 last_head_text_pos;
    ebutton updr[EB_MAX];
    RID editor;
    RID bconfirm;
    RID bcancel;
    ts::wstr_c curedit;

    static bool _edt(const ts::wstr_c & e);
};

ts::static_setup<head_stuff_s,1000> hstuff;

bool head_stuff_s::_edt(const ts::wstr_c & e)
{
    hstuff().curedit = e;
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
            int width = 100 + sx + g_app->buttons().callb->size.x;

            HOLD r(owner->getparent());
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

            int width = cr.width() - sx - g_app->buttons().callb->size.x - g_app->buttons().fileb->size.x - 
                20 - g_app->buttons().confirmb->size.x - g_app->buttons().cancelb->size.x;
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
                    gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_USERNAME, hstuff().curedit).send();
            }
        } else
        {
            if (contact->get_customname() != hstuff().curedit)
            {
                contact->set_customname(hstuff().curedit);
                prf().dirtycontact(contact->getkey());
                flags.set(F_SKIPUPDATE);
                gmsg<ISOGM_UPDATE_CONTACT_V>(contact).send();
                flags.clear(F_SKIPUPDATE);
                update_text();
            }
        }

    }
    if (flags.is(F_EDITSTATUS))
    {
        flags.clear(F_EDITSTATUS);
        if (contact->getkey().is_self())
        {
            if (prf().userstatus(hstuff().curedit))
                gmsg<ISOGM_CHANGED_PROFILEPARAM>(PP_USERSTATUSMSG, hstuff().curedit).send();
        }

    }
    DELAY_CALL_R( 0, DELEGATE( this, update_buttons ), nullptr );
    return true;
}

bool gui_contact_item_c::cancel_edit( RID, GUIPARAM)
{
    flags.clear(F_EDITNAME|F_EDITSTATUS);
    DELAY_CALL_R( 0, DELEGATE( this, update_buttons ), nullptr );
    return true;
}

bool gui_contact_item_c::update_buttons( RID r, GUIPARAM p )
{
    ASSERT( CIR_CONVERSATION_HEAD == role );

    if (p)
    {
        // just update pos of edit buttons
        bool noedt = !flags.is(F_EDITNAME|F_EDITSTATUS);
        for (ebutton &b : hstuff().updr)
            if (b.updated)
                MODIFY(b.brid).pos(b.p).setminsize(b.brid).visible(noedt);
        if (noedt && hstuff().editor)
        {
        } else
            return true;
    }

    if ( getrid() == r )
    {
        for(ebutton &b : hstuff().updr)
            if (b.brid)
            {
                b.updated = false;
                MODIFY(b.brid).visible(false);
            }
    }

    if (!hstuff().updr[EB_NAME].brid)
    {
        gui_button_c &b = MAKE_CHILD<gui_button_c>(getrid());
        b.set_face(g_app->buttons().editb);
        b.set_handler(DELEGATE(this, edit0), nullptr);
        hstuff().updr[EB_NAME].brid = b.getrid();
    }

    if (!hstuff().updr[EB_STATUS].brid)
    {
        gui_button_c &b = MAKE_CHILD<gui_button_c>(getrid());
        b.set_face(g_app->buttons().editb);
        b.set_handler(DELEGATE(this, edit1), nullptr);
        hstuff().updr[EB_STATUS].brid = b.getrid();
    }

    if (flags.is(F_EDITNAME|F_EDITSTATUS))
    {
        ts::wstr_c eval;
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

            gui_textfield_c &tf = (MAKE_CHILD<gui_textfield_c>(getrid(), eval, MAX_PATH, 0, false) << (gui_textedit_c::TEXTCHECKFUNC)head_stuff_s::_edt);
            hstuff().editor = tf.getrid();
            tf.leech(TSNEW(leech_edit, hstuff().last_head_text_pos.x));
            gui->set_focus(hstuff().editor, true);
            tf.end();
            tf.on_escape_press = DELEGATE( this, cancel_edit );
            tf.on_enter_press = DELEGATE( this, apply_edit );

            gui_button_c &bok = MAKE_CHILD<gui_button_c>(getrid());
            bok.set_face(g_app->buttons().confirmb);
            bok.set_handler(DELEGATE(this, apply_edit), nullptr);
            bok.leech(TSNEW(leech_at_right, &tf, 5));
            hstuff().bconfirm = bok.getrid();

            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face(g_app->buttons().cancelb);
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
            active_protocol_c *ap = prf().ap(c->getkey().protoid);
            if (CHECK(ap)) 
            {
                int f = ap->get_features();
                features |= f;
                if (c->get_state() == CS_ONLINE) features_online |= f;
                if (c->is_av()) now_disabled = true;
            }
        });

        gui_button_c &b_call = MAKE_CHILD<gui_button_c>(getrid());
        b_call.set_face(g_app->buttons().callb);
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

        gui_button_c &b_file = MAKE_CHILD<gui_button_c>(getrid());
        b_file.set_face(g_app->buttons().fileb);
        b_file.tooltip(TOOLTIP(TTT("Отправить файл",187)));

        b_file.set_handler(DELEGATE(this, send_file), nullptr);
        minsz = b_file.get_min_size();
        b_file.leech(TSNEW(leech_at_left, &b_call, 2));
        MODIFY(b_file).visible(true);
        if (0 == (features & PF_SEND_FILE))
        {
            b_file.disable();
            b_file.tooltip(TOOLTIP(TTT("Передача файлов не поддерживается",188)));
        }

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
                if (r->is_protohit()) flags.set(F_PROTOHIT);
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

    gui_control_c::created();

    if (CIR_CONVERSATION_HEAD == role)
    {
        //gui_button_c &b_editname = MAKE_CHILD<gui_button_c>(getrid());
        //b_editname.set_face(CONSTASTR("edit"));
        //b_editname.tooltip(TOOLTIP(TTT("Копировть ID в буфер обмена")));

        //b_editname.set_handler(x::copy_handler, nullptr);
        //b_editname.leech(TSNEW(leech_dock_right_center_s, 40, 40, 5, -5, 0, 1));
        //MODIFY(b_editname).visible(true);
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
            if (CHECK(ap) && 0 != (PF_AUDIO_CALLS & ap->get_features()))
            {
                if (c_call == nullptr || cdef == c)
                    c_call = cdef;
            }
        });

        if (CHECK(c_call))
        {
            c_call->b_call(RID(), nullptr);
            btn.call_enable(false);
        }

    }
    return true;
}

bool gui_contact_item_c::send_file(RID, GUIPARAM)
{
    ts::wstrings_c files;
    ts::wstr_c fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());
    ts::wstr_c title(TTT("Отправить файлы", 180));
    if (ts::get_load_filename_dialog(files, fromdir, CONSTWSTR(""), CONSTWSTR(""), nullptr, title))
    {
        if (files.size())
            prf().last_filedir(ts::fn_get_path(files.get(0)));

        if (contact_c *c = contact)
            for (const ts::wstr_c &fn : files)
                c->send_file(fn);
    }

    return true;
}

ts::uint32 gui_contact_item_c::gm_handler( gmsg<ISOGM_SELECT_CONTACT> & c )
{
    if ( CIR_LISTITEM == role || CIR_ME == role || CIR_METACREATE == role )
    {
        MODIFY(*this).active( contact == c.contact );
    }
    return 0;
}

ts::uint32 gui_contact_item_c::gm_handler(gmsg<ISOGM_SOMEUNREAD> & c)
{
    if (n_unread > 0 || (CIR_CONVERSATION_HEAD == role && contact->achtung()))
        return GMRBIT_ACCEPTED | GMRBIT_ABORT;

    return 0;
}

void gui_contact_item_c::setcontact(contact_c *c)
{
    ASSERT(c->is_multicontact());
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
    ts::wstr_c newtext;
    if (contact)
    {
        if (contact->getkey().is_self())
        {
            newtext = prf().username();
            text_adapt_user_input(newtext);
            ts::wstr_c t2(prf().userstatus());
            text_adapt_user_input(t2);
            if (CIR_CONVERSATION_HEAD == role)
            {
                ts::ivec2 sz = g_app->buttons().editb->size;
                newtext.append( CONSTWSTR(" <rect=0,") );
                newtext.append_as_uint( sz.x ).append_char(',').append_as_uint( -sz.y ).append(CONSTWSTR(",2><br><l>"));
                newtext.append(t2).append(CONSTWSTR(" <rect=1,"));
                newtext.append_as_uint( sz.x ).append_char(',').append_as_uint( -sz.y ).append(CONSTWSTR(",2></l>"));
            } else
            {
                newtext.append(CONSTWSTR("<br><l>")).append(t2).append(CONSTWSTR("</l>"));
            }
        } else if (CIR_METACREATE == role && contact->subcount() > 1)
        {
            contact->subiterate([&](contact_c *c) {
                newtext.append(CONSTWSTR("<nl>"));
                ts::wstr_c n = c->get_name(false);
                text_adapt_user_input(n);
                newtext.append( n );
                active_protocol_c *ap = prf().ap( c->getkey().protoid );
                if (CHECK(ap)) newtext.append(CONSTWSTR(" (")).append(ap->get_name()).append(CONSTWSTR(")"));
            });

        } else if (contact->is_multicontact())
        {
            int live = 0;
            int count = 0;
            int rej = 0;
            int invsend = 0;
            int invrcv = 0;
            int wait = 0;
            contact->subiterate( [&](contact_c *c) {
                ++count;
                if (c->get_state() != CS_ROTTEN) ++live;
                if (c->get_state() == CS_REJECTED) ++rej;
                if (c->get_state() == CS_INVITE_SEND) ++invsend;
                if (c->get_state() == CS_INVITE_RECEIVE) ++invrcv;
                if (c->get_state() == CS_WAIT) ++wait;
            } );
            if (live == 0 && count > 0)
                newtext = colorize( TTT("Удален",77), get_default_text_color(0) );
            else if (count == 0)
                newtext = colorize( TTT("Пустой",78), get_default_text_color(0) );
            else if (rej > 0)
                newtext = colorize( TTT("Отказано",79), get_default_text_color(0) );
            else if (invsend > 0)
            {
                newtext = contact->get_customname();
                if (newtext.is_empty()) newtext = contact->get_name();
                text_adapt_user_input(newtext);
                if (!newtext.is_empty()) newtext.append(CONSTWSTR("<br>"));
                newtext.append( colorize(TTT("Запрос отправлен", 88), get_default_text_color(0)) );
            }
            else {
                newtext = contact->get_customname();
                if (newtext.is_empty()) newtext = contact->get_name();
                text_adapt_user_input(newtext);
                ts::wstr_c t2(contact->get_statusmsg());
                text_adapt_user_input(t2);

                if (CIR_CONVERSATION_HEAD == role)
                {
                    ts::ivec2 sz = g_app->buttons().editb->size;
                    newtext.append(CONSTWSTR(" <rect=0,"));
                    newtext.append_as_uint(sz.x).append_char(',').append_as_int(-sz.y).append(CONSTWSTR(",2><br><l>"));
                    //newtext.append(t1).append(CONSTWSTR("<br><l>"));
                    newtext.append(t2).append(CONSTWSTR("</l>"));

                } else
                {
                    if (invrcv)
                    {
                        if (!newtext.is_empty()) newtext.append(CONSTWSTR("<br>"));
                        newtext.append( colorize( TTT("Требуется подтверждение",153), get_default_text_color(0) ) );
                        t2.clear();
                    }

                    if (wait)
                    {
                        if (!newtext.is_empty()) newtext.append(CONSTWSTR("<br>"));
                        newtext.append(colorize(TTT("Ожидание",154), get_default_text_color(0)));
                        t2.clear();
                    }
                    if (!t2.is_empty()) newtext.append(CONSTWSTR("<br><l>")).append(t2).append(CONSTWSTR("</l>"));

                }
            }

        } else
            newtext.set( CONSTWSTR("<error>") );


    } else
    {
        newtext.clear();
    }
    text.set_text_only(newtext, false);

    if (CIR_CONVERSATION_HEAD == role && text.is_dirty())
    {
        for (ebutton &b : hstuff().updr)
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
    if (ASSERT(r < EB_MAX) && prf().is_loaded())
    {
        hstuff().updr[r].p = to_local(HOLD(getroot())().to_screen(p));
        hstuff().updr[r].updated = true;
        DELAY_CALL_R( 0, DELEGATE(this, update_buttons), (void *)1 );
    }
}

/*virtual*/ void gui_contact_item_c::update_dndobj(guirect_c *donor)
{
    update_text();
    if (const gui_contact_item_c *r = dynamic_cast<const gui_contact_item_c *>(donor))
        flags.init(F_PROTOHIT, r->is_protohit());
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
    drawchecker dch;
    gui_contact_item_c &dnd = MAKE_ROOT<gui_contact_item_c>(dch, contact);
    ts::ivec2 subsize(0);
    if (const theme_rect_s *thr = dnd.themerect())
        subsize = thr->maxcutborder.lt + thr->maxcutborder.rb;
    dnd.update_text();
    MODIFY(dnd).pos( getprops().screenpos() + deltapos ).size( getprops().size() - subsize ).visible(true);
    return &dnd;
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
            bool force_state_icon = false;
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

            if (( force_state_icon || CIR_METACREATE == role || (flags.is(F_PROTOHIT) && st != CS_ROTTEN)) && bdesc)
                bdesc->draw( m_engine.get(), bst, ca, button_desc_s::ARIGHT | button_desc_s::ABOTTOM );

            if (contact)
            {
                text_draw_params_s tdp;
                button_desc_s *icon = g_app->buttons().icon[contact->get_meta_gender()];
                icon->draw( m_engine.get(), button_desc_s::NORMAL, ca, button_desc_s::ALEFT | button_desc_s::ATOP | button_desc_s::ABOTTOM );
                bool achtung = (CIR_CONVERSATION_HEAD == role) ? false : contact->achtung();
                if (n_unread > 0 || achtung)
                {
                    if (n_unread > 99) n_unread = 99;
                    button_desc_s *unread = g_app->buttons().unread;

                    ts::ivec2 pos = unread->draw( m_engine.get(), button_desc_s::NORMAL, ca, button_desc_s::ALEFT | button_desc_s::ABOTTOM );

                    if (!contact->is_ringtone() || contact->is_ringtone_blink())
                    {
                        ts::flags32_s f; f.setup(ts::TO_VCENTER | ts::TO_HCENTER);
                        tdp.textoptions = &f;
                        tdp.forecolor = unread->colors + button_desc_s::NORMAL;

                        draw_data_s &dd = m_engine->begin_draw();
                        dd.offset += pos;
                        dd.size = unread->size;
                        if ( contact->is_ringtone() )
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

                if (!flags.is(F_EDITNAME|F_EDITSTATUS))
                {
                    ca.lt += ts::ivec2(icon->size.x + 5, 2);
                    draw_data_s &dd = m_engine->begin_draw();
                    dd.size = ca.size();
                    if (dd.size >> 0)
                    {
                        if (CIR_CONVERSATION_HEAD == role)
                            hstuff().last_head_text_pos = ca.lt;
                        dd.offset += ca.lt;
                        ts::flags32_s f; f.setup(ts::TO_VCENTER);
                        tdp.textoptions = &f;
                        tdp.forecolor = nullptr;
                        tdp.rectupdate = DELEGATE( this, updrect );
                        draw(dd, tdp);
                    }
                    m_engine->end_draw();
                }
            }
        }
        return true;
    case SQ_MOUSE_OUT:
        flags.clear(F_LBDN);
        break;
    case SQ_MOUSE_LDOWN:
        flags.set(F_LBDN);
        if ( CIR_LISTITEM == role ) 
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
            gmsg<ISOGM_SELECT_CONTACT>( contact ).send();
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
                        SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                            gui_isodialog_c::title(DT_MSGBOX_WARNING),
                            TTT("Будет полностью удален контакт:[br]$",84) / c->get_description()
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
                        gmsg<ISOGM_UPDATE_CONTACT_V>( c ).send();
                        gmsg<ISOGM_UPDATE_CONTACT> upd;
                        upd.key = c->getkey();
                        upd.mask = CDM_STATE;
                        upd.state = c->get_state();
                        upd.send();
                        historian->reselect(true);
                    }
                }
            };

            menu_c m;
            if (contact->subcount() > 1) 
            {
                menu_c mc = m.add_sub(TTT("Метаконтакт", 145));
                contact->subiterate([&](contact_c *c) {

                    ts::wstr_c text(c->get_name(false));
                    text_adapt_user_input(text);
                    active_protocol_c *ap = prf().ap(c->getkey().protoid);
                    if (CHECK(ap)) text.append(CONSTWSTR(" (")).append(ap->get_name()).append(CONSTWSTR(")"));
                    mc.add( TTT("Отделить: $",197) / text, 0, handlers::m_metacontact_detach, c->getkey().as_str() );
                });

            }
            m.add(TTT("Удалить",85),0,handlers::m_delete,contact->getkey().as_str());
            gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(),0), m);

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
        }
        return false;

    }

    return __super::sq_evt(qp,rid,data);
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
    int mystate = statev(contact->get_meta_state());
    int otherstate = statev(ci.contact->get_meta_state());
    if (otherstate > mystate) return true;
    
    return false;
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
    ts::ivec2 sz(100);
    sz.y += skip_top_pixels + skip_bottom_pixels;
    return sz;
}

/*virtual*/ void gui_contactlist_c::created()
{
    set_theme_rect(CONSTASTR("contacts"), false);
    __super::created();

    if (role == CLR_MAIN_LIST)
    {
        struct handlers
        {
            static bool summon_addcontacts(RID, GUIPARAM)
            {
                if (prf().is_loaded())
                    SUMMON_DIALOG<dialog_addcontact_c>(UD_ADDCONTACT);
                else
                    SUMMON_DIALOG<dialog_msgbox_c>(UD_NOT_UNIQUE, dialog_msgbox_c::params(
                    gui_isodialog_c::title(DT_MSGBOX_ERROR),
                    TTT("Пожалуйста, создайте профиль", 144)
                    ));
                return true;
            }
        };

        gui_button_c &b_add = MAKE_CHILD<gui_button_c>( getrid() );
        b_add.tooltip(TOOLTIP(TTT("Добавить контакт",64)));
        b_add.set_face(CONSTASTR("addcontact"));
        b_add.set_handler(handlers::summon_addcontacts,nullptr);
        b_add.leech( TSNEW(leech_dock_bottom_center_s, 40, 40, 0, 10) );
        MODIFY(b_add).zindex(1.0f).visible(true);
    
        //b_add.set_handler();

        gui_contact_item_c &selfci = MAKE_CHILD<gui_contact_item_c>(getrid(), &contacts().get_self()) << CIR_ME;
        selfci.leech( TSNEW(leech_dock_top_s, 70 ) );
        self = &selfci;

        skipctl = 2;

        contacts().update_meta();
    } else if (role == CLR_NEW_METACONTACT_LIST)
    {
        skip_top_pixels = 0;
        skip_bottom_pixels = 0;
        skipctl = 0;
    }
}

ts::uint32 gui_contactlist_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    if (ch.pass > 0 && self)
    {
        switch (ch.pp)
        {
            case PP_USERNAME:
            case PP_USERSTATUSMSG:
                self->update_text();
                break;
        }
    }
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

ts::uint32 gui_contactlist_c::gm_handler( gmsg<ISOGM_UPDATE_CONTACT_V> & c )
{
    if (c.contact->get_historian()->getkey().is_self())
    {
        if (self && self->contacted())
            if (ASSERT(c.contact == &self->getcontact() || c.contact->getmeta() == &self->getcontact()))
            {
                self->protohit();
                self->update_text();
            }
    
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
                ci->protohit();
                ci->update_text();
                gui->dragndrop_update(ci);
                return 0;
            }
        }
    }

    if (role == CLR_MAIN_LIST)
        MAKE_CHILD<gui_contact_item_c>(getrid(), c.contact->get_historian());

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
        switch (qp)
        {
        case SQ_MOUSE_IN:
            MODIFY(rid).highlight(true);
            return false;
        case SQ_MOUSE_OUT:
            MODIFY(rid).highlight(false);
            return false;
        default:
            return __super::sq_evt(qp,rid,data);
        }
    }
    return __super::sq_evt(qp, rid, data);
}
