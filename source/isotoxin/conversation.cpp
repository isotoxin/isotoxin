#include "isotoxin.h"

#define ADDTIMESPACE 5

////////////////////////// gui_notice_c

MAKE_CHILD<gui_notice_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}

gui_notice_c::gui_notice_c(MAKE_CHILD<gui_notice_c> &data) :gui_label_c(data), notice(data.n)
{
    switch (notice)
    {
        case NOTICE_FRIEND_REQUEST_SEND_OR_REJECT:
            set_theme_rect(CONSTASTR("notice.reject"), false);
            break;
        case NOTICE_GROUP_CHAT:
            set_theme_rect(CONSTASTR("notice.gchat"), false);
            break;
        //case NOTICE_FRIEND_REQUEST_RECV:
        //case NOTICE_INCOMING_CALL:
        //case NOTICE_CALL_INPROGRESS:
        default:
            set_theme_rect(CONSTASTR("notice.invite"), false);
            break;
    }
}

gui_notice_c::gui_notice_c(MAKE_CHILD<gui_notice_network_c> &data):gui_label_c(data), notice(NOTICE_NETWORK)
{
    set_theme_rect(CONSTASTR("notice.net"), false);
}

gui_notice_c::~gui_notice_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, setup_tail));
    }
}

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_FILE>&ifl)
{
    if (notice == NOTICE_FILE && utag == ifl.utag && (ifl.fctl == FIC_BREAK || ifl.fctl == FIC_DISCONNECT))
    {
        RID par = getparent();
        TSDEL(this);
        HOLD(par).as<gui_noticelist_c>().refresh();
    }
    return 0;
}

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_CALL_STOPED> & n)
{
    RID par = getparent();
    if (NOTICE_INCOMING_CALL == notice || NOTICE_CALL_INPROGRESS == notice || NOTICE_CALL == notice )
    {
        TSDEL(this);
        HOLD(par).as<gui_noticelist_c>().refresh();
    }

    return 0;
}
ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_NOTICE> & n)
{
    if (n.just_created == this) return 0;
    bool die = false;
    if (n.n == notice)
    {
        if (NOTICE_FRIEND_REQUEST_RECV == notice || NOTICE_FRIEND_REQUEST_SEND_OR_REJECT == notice || NOTICE_INCOMING_CALL == notice || NOTICE_CALL_INPROGRESS == notice || NOTICE_NEWVERSION == notice || NOTICE_GROUP_CHAT == notice)
            die = true;
    }
    if (NOTICE_CALL_INPROGRESS == n.n && (NOTICE_INCOMING_CALL == notice || NOTICE_CALL == notice))
        die = true;

    if (die) TSDEL(this);
    // no need to refresh here due ISOGM_NOTICE is notice-creation event and it initiates refresh
    return 0;
}

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &c)
{
    if (NOTICE_GROUP_CHAT == notice && c.contact->getkey().is_group() && utag == ts::ref_cast<uint64>(c.contact->getkey()))
    {
        setup( c.contact );
    }
    return 0;
}


/*virtual*/ ts::ivec2 gui_notice_c::get_min_size() const
{
    return ts::ivec2(60, height ? height : 60);
}

/*virtual*/ ts::ivec2 gui_notice_c::get_max_size() const
{
    ts::ivec2 m = __super::get_max_size();
    m.y = get_min_size().y;
    return m;
}
void gui_notice_c::created()
{
    defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
    flags.set(F_DIRTY_HEIGHT_CACHE);
    __super::created();
}

/*virtual*/ bool gui_notice_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
    case SQ_DRAW:
        {
            __super::sq_evt(qp, rid, data);
            return true;
        }
    }

    return __super::sq_evt(qp, rid, data);
}

int gui_notice_c::get_height_by_width(int w) const
{
    if (flags.is(F_DIRTY_HEIGHT_CACHE))
    {
        for (int i = 0; i < ARRAY_SIZE(height_cache); ++i)
            height_cache[i] = ts::ivec2(-1);
        next_cache_write_index = 0;
        const_cast<ts::flags32_s &>(flags).clear(F_DIRTY_HEIGHT_CACHE);

    }
    else
    {
        for (int i = 0; i < ARRAY_SIZE(height_cache); ++i)
            if (height_cache[i].x == w) return height_cache[i].y;
    }

    const theme_rect_s *thr = themerect();
    if (ASSERT(thr))
    {
        ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
        int ww = sz.x;
        sz = textrect.calc_text_size(ww, custom_tag_parser_delegate());
        int h = thr->size_by_clientsize(ts::ivec2(ww, sz.y + addheight), false).y;
        height_cache[next_cache_write_index] = ts::ivec2(w, h);
        ++next_cache_write_index;
        if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;
        return h;
    }

    height_cache[next_cache_write_index] = ts::ivec2(w, height);
    ++next_cache_write_index;
    if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;

    return height;
}

extern ts::static_setup<spinlock::syncvar<autoupdate_params_s>,1000> auparams;

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_DOWNLOADPROGRESS>&p)
{
    if (notice == NOTICE_NEWVERSION)
    {
        ts::wstr_c ot = textrect.get_text();
        int pp = ot.find_pos(CONSTWSTR("<null=px>"));
        if (pp >= 0)
        {
            ts::wstr_c n;
            
            n.set_as_uint(p.downloaded);
            for (int ix = n.get_length() - 3; ix > 0; ix -= 3)
                n.insert(ix, '`');

            ot.set_length(pp+9).append( n );
            if (p.total > 0)
            {
                n.set_as_uint(p.total);
                for (int ix = n.get_length() - 3; ix > 0; ix -= 3)
                    n.insert(ix, '`');

                ot.append(CONSTWSTR(" / "))
                    .append(n)
                    .append(CONSTWSTR(" ("))
                    .append(ts::roundstr<ts::wstr_c>(100.0*double(p.downloaded)/p.total,2))
                    .append(CONSTWSTR("%)"));
            }
            textrect.set_text_only(ot, false);
            getengine().redraw();
        }
    }

    return 0;
}

void gui_notice_c::setup(const ts::str_c &itext_utf8)
{
    switch (notice)
    {
    case NOTICE_NEWVERSION:
        {
            ts::wstr_c newtext(1024,false);
            newtext.set(CONSTWSTR("<p=c>"));
            newtext.append(TTT("Доступна новая версия: $",163) / from_utf8(itext_utf8));
            newtext.append(CONSTWSTR("<br>"));
            newtext.append(TTT("Текущая версия: $",164) / ts::to_wstr(application_c::appver()));

            if ( auparams().lock_read()().in_progress )
            {
                newtext.append(CONSTWSTR("<br><b>"));
                newtext.append(TTT("-загрузка-",166));
                newtext.append(CONSTWSTR("</b><br><null=px>0"));
            } else if (auparams().lock_read()().downloaded)
            {
                newtext.append(CONSTWSTR("<br>"));
                newtext.append(TTT("Новая версия загружена. Требуется перезагрузка приложения.",167));

                gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
                b_accept.set_text(TTT("Перезагрузка",168));
                b_accept.set_face_getter(BUTTON_FACE(button));
                b_accept.set_handler(DELEGATE(g_app, b_restart), nullptr);
                b_accept.leech(TSNEW(leech_dock_bottom_center_s, 150, 30, -5, 5, 0, 1));
                MODIFY(b_accept).visible(true);

                addheight = 40;

            } else
            {
                gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
                b_accept.set_text(TTT("Загрузить",165));
                b_accept.set_face_getter(BUTTON_FACE(button));
                b_accept.set_handler(DELEGATE(g_app, b_update_ver), (GUIPARAM)AUB_DOWNLOAD);
                b_accept.leech(TSNEW(leech_dock_bottom_center_s, 150, 30, -5, 5, 0, 1));
                MODIFY(b_accept).visible(true);

                addheight = 40;
            }
            textrect.set_text_only(newtext, false);
        }
        break;
    }
    setup_tail();
}

void gui_notice_c::setup(const ts::str_c &itext_utf8, contact_c *sender, uint64 utag_)
{
    utag = utag_;
    switch (notice)
    {
    case NOTICE_FRIEND_REQUEST_RECV:
        {
            ts::wstr_c newtext(1024,false);
            newtext.set(CONSTWSTR("<p=c><b>"));
            newtext.append(from_utf8(sender->get_pubid_desc()));
            newtext.append(CONSTWSTR("</b><br>"));

            if (itext_utf8.equals(CONSTASTR("\1restorekey")))
            {
                newtext.append(TTT("Требуется подтверждение восстановления ключа",198));
            } else
            {
                newtext.append(TTT("Неизвестный контакт запрашивает разрешение на добавление вас в список контактов", 74));
                newtext.append(CONSTWSTR("<hr=7,2,1>"));
                newtext.append(from_utf8(itext_utf8));
            }

            textrect.set_text_only(newtext,false);

            gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
            b_accept.set_text(TTT("Принять", 75));
            b_accept.set_face_getter(BUTTON_FACE(button));
            b_accept.set_handler(DELEGATE(sender, b_accept), this);
            b_accept.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 0, 2));
            MODIFY(b_accept).visible(true);

            gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(getrid());
            b_reject.set_text(TTT("Отклонить", 76));
            b_reject.set_face_getter(BUTTON_FACE(button));
            b_reject.set_handler(DELEGATE(sender, b_reject), this);
            b_reject.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 1, 2));
            MODIFY(b_reject).visible(true);

            addheight = 40;
        }
        break;
    case NOTICE_FILE:
        {
            ts::wstr_c newtext(512,false);
            newtext.set(CONSTWSTR("<p=c>"));
            newtext.append(TTT("Вы согласны принять файл [b]$[/b]?",175) / from_utf8(itext_utf8));
            newtext.append(CONSTWSTR("<hr=7,2,1>"));
            textrect.set_text_only(newtext,false);


            int minw = 0;
            gui_button_c &b_receiveas = MAKE_CHILD<gui_button_c>(getrid());
            b_receiveas.set_face_getter(BUTTON_FACE(button));
            b_receiveas.set_text(TTT("Сохранить как...", 177), minw); minw += 6; if (minw < 100) minw = 100;
            b_receiveas.set_handler(DELEGATE(sender, b_receive_file_as), this);
            b_receiveas.leech(TSNEW(leech_dock_bottom_center_s, minw, 30, -5, 5, 1, 3));
            MODIFY(b_receiveas).visible(true);

            gui_button_c &b_receive = MAKE_CHILD<gui_button_c>(getrid());
            b_receive.set_face_getter(BUTTON_FACE(button));
            b_receive.set_text(TTT("Сохранить",176) );
            b_receive.set_handler(DELEGATE(sender, b_receive_file), this);
            b_receive.leech(TSNEW(leech_dock_bottom_center_s, minw, 30, -5, 5, 0, 3));
            MODIFY(b_receive).visible(true);

            gui_button_c &b_refuse = MAKE_CHILD<gui_button_c>(getrid());
            b_refuse.set_face_getter(BUTTON_FACE(button));
            b_refuse.set_text(TTT("Нет",178));
            b_refuse.set_handler(DELEGATE(sender, b_refuse_file), this);
            b_refuse.leech(TSNEW(leech_dock_bottom_center_s, minw, 30, -5, 5, 2, 3));
            MODIFY(b_refuse).visible(true);

            addheight = 40;
        }
        break;
    default:
        if (sender == nullptr)
            setup(itext_utf8);
        else
            setup(sender);
        return;
    }
    setup_tail();
}

void gui_notice_c::setup(contact_c *sender)
{
    switch (notice)
    {
    case NOTICE_FRIEND_REQUEST_SEND_OR_REJECT:
        {
            update_text(sender);

            getengine().trunc_children(0);

            gui_button_c &b_resend = MAKE_CHILD<gui_button_c>(getrid());
            b_resend.set_text(TTT("Повторить", 81));
            b_resend.set_face_getter(BUTTON_FACE(button));
            b_resend.set_handler(DELEGATE(sender, b_resend), this);
            b_resend.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 0, 2));
            MODIFY(b_resend).visible(true);

            gui_button_c &b_kill = MAKE_CHILD<gui_button_c>(getrid());
            b_kill.set_text(TTT("Удалить", 82));
            b_kill.set_face_getter(BUTTON_FACE(button));
            b_kill.set_handler(DELEGATE(sender, b_kill), this);
            b_kill.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 1, 2));
            MODIFY(b_kill).visible(true);

            addheight = 40;

        }
        break;
    case NOTICE_INCOMING_CALL:
        {
            update_text(sender);

            getengine().trunc_children(0);

            gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
            b_accept.set_face_getter(BUTTON_FACE(accept_call));
            b_accept.set_handler(DELEGATE(sender, b_accept_call), this);
            ts::ivec2 minsz = b_accept.get_min_size();
            b_accept.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 0, 2));
            MODIFY(b_accept).visible(true);

            gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(getrid());
            b_reject.set_face_getter(BUTTON_FACE(reject_call));
            b_reject.set_handler(DELEGATE(sender, b_reject_call), this);
            minsz = b_reject.get_min_size();
            b_reject.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 1, 2));
            MODIFY(b_reject).visible(true);

            addheight = 50;
        }
        break;
    case NOTICE_CALL_INPROGRESS:
        {
            update_text(sender);

            getengine().trunc_children(0);

            gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(getrid());
            b_reject.set_face_getter(BUTTON_FACE(reject_call));
            b_reject.set_handler(DELEGATE(sender, b_hangup), this);
            ts::ivec2 minsz = b_reject.get_min_size();
            b_reject.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 0, 1));
            MODIFY(b_reject).visible(true);

            addheight = 50;
        }
        break;
    case NOTICE_CALL:
        {
            update_text(sender);

            getengine().trunc_children(0);

            gui_button_c &b_cancel = MAKE_CHILD<gui_button_c>(getrid());
            b_cancel.set_face_getter(BUTTON_FACE(call_cancel));
            b_cancel.set_handler(DELEGATE(sender, b_cancel_call), this);
            ts::ivec2 minsz = b_cancel.get_min_size();
            b_cancel.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 0, 1));
            MODIFY(b_cancel).visible(true);

            addheight = 50;
        }
        break;
    case NOTICE_GROUP_CHAT:
        {
            utag = ts::ref_cast<uint64>(sender->getkey());
            ts::wstr_c txt(CONSTWSTR("<p=c>"));
            ASSERT( sender->getkey().is_group() );
            sender->subiterate([&](contact_c *m) 
            {
                switch (m->get_state())
                {
                    case CS_OFFLINE:
                        txt.append(CONSTWSTR("<img=gch_offline,-1>"));
                        break;
                    case CS_ONLINE:
                        txt.append(CONSTWSTR("<img=gch_online,-1>"));
                        break;
                    default:
                        txt.append( CONSTWSTR("<img=gch_unknown,-1>") );
                    break;
                }
                txt.append( from_utf8(m->get_name()) ).append(CONSTWSTR(", "));
            } );

            if (txt.ends( CONSTWSTR(", ") ))
                txt.trunc_length(2);
            else
                txt.append( TTT("В этом групповом чате кроме вас никого нет",257) );
            textrect.set_text_only(txt, false);
        }
        break;
    }

    setup_tail();
}

bool gui_notice_c::setup_tail(RID, GUIPARAM)
{
    flags.set(F_DIRTY_HEIGHT_CACHE);

    int h = get_height_by_width(get_client_area().width());
    if (h != height)
    {
        height = h;
        MODIFY(*this).sizeh(height);
    }
    return true;
}

void gui_notice_c::update_text(contact_c *sender)
{
    ts::str_c aname = sender->get_name();
    text_adapt_user_input(aname);

    ts::wstr_c newtext(512,false);
    newtext.set(CONSTWSTR("<p=c>"));
    bool hr = true;
    switch (notice)
    {
        case NOTICE_FRIEND_REQUEST_SEND_OR_REJECT:

            newtext.append(CONSTWSTR("<b>"));
            newtext.append(from_utf8(sender->get_pubid_desc()));
            newtext.append(CONSTWSTR("</b><br>"));
            if (sender->get_state() == CS_REJECTED)
                newtext.append(TTT("Запрос на добавление в список контактов был отклонен. Вы можете повторить запрос.", 80));
            else if (sender->get_state() == CS_INVITE_SEND)
                newtext.append(TTT("Запрос на добавление в список контактов был отправлен. Вы можете повторить запрос.", 89));
            hr = false;

            break;
        case NOTICE_INCOMING_CALL:
            newtext.append(TTT("Входящий звонок от $", 134) / from_utf8(aname));
            break;
        case NOTICE_CALL_INPROGRESS:
            newtext.append(TTT("Разговор с $", 136) / from_utf8(aname));
            break;
        case NOTICE_CALL:
            newtext.append(TTT("Звонок $",142) / from_utf8(aname));
            break;
    }

    if (hr) newtext.append(CONSTWSTR("<hr=7,2,1>"));
    textrect.set_text_only(newtext,false);
}

MAKE_CHILD<gui_notice_network_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
}


gui_notice_network_c::~gui_notice_network_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, flash_pereflash));
        gui->delete_event(DELEGATE(this, resetup));
    }
}

void gui_notice_network_c::setup(const ts::str_c &pubid_)
{
    pubid = pubid_;
    ts::wstr_c sost, plugdesc, uname, ustatus, netname;
    bool is_autoconnect = false;
    prf().iterate_aps([&](const active_protocol_c &ap) {

        if (contact_c *c = contacts().find_subself(ap.getid()))
            if (c->get_pubid() == pubid)
            {
                networkid = ap.getid();
                plugdesc = from_utf8(ap.get_desc());
                netname = from_utf8(ap.get_name());
                is_autoconnect = ap.is_autoconnect();

                if (c->get_state() == CS_ONLINE)
                    sost.set(CONSTWSTR("</l><b>"))
                    .append(maketag_color<ts::wchar>(get_default_text_color(0))).append(TTT("Онлайн", 100)).append(CONSTWSTR("</color></b><l>"));

                if (c->get_state() == CS_OFFLINE)
                    sost.set(maketag_color<ts::wchar>(get_default_text_color(1))).append(TTT("Офлайн", 101)).append(CONSTWSTR("</color>"));

                uname = from_utf8(ap.get_uname());
                ustatus = from_utf8(ap.get_ustatusmsg());
            }
    });

    if (uname.is_empty())
        uname.set(maketag_color<ts::wchar>(get_default_text_color(4))).append( from_utf8(prf().username()) ).append(CONSTWSTR("</color>"));
    else
        uname.insert(0,CONSTWSTR("</l><b>")).append(CONSTWSTR("</b><l>"));
    if (ustatus.is_empty())
        ustatus.set(maketag_color<ts::wchar>(get_default_text_color(4))).append( from_utf8(prf().userstatus()) ).append(CONSTWSTR("</color>"));
    else
        ustatus.insert(0, CONSTWSTR("</l><b>")).append(CONSTWSTR("</b><l>"));

    textrect.set_text_only(make_proto_desc(MPD_UNAME | MPD_USTATUS | MPD_NAME | MPD_MODULE | MPD_ID | MPD_STATE)

                            .replace_all(CONSTWSTR("{uname}"), uname)
                            .replace_all(CONSTWSTR("{ustatus}"), ustatus)
                            .replace_all(CONSTWSTR("{name}"), netname)
                            .replace_all(CONSTWSTR("{module}"), plugdesc)
                            .replace_all(CONSTWSTR("{id}"), ts::wstr_c(CONSTWSTR("<ee><null=1><null=2>")).append(from_utf8(pubid)).append(CONSTWSTR("<null=3><null=4>")))
                            .replace_all(CONSTWSTR("{state}"), sost),

                            true);

    struct copydata
    {
        ts::str_c pubid;
        ts::safe_ptr<gui_notice_network_c> notice;
        bool copy_handler(RID b, GUIPARAM)
        {
            ts::set_clipboard_text(from_utf8(pubid));
            if (notice) notice->flash();
            return true;
        }
        copydata(const ts::str_c &pubid, gui_notice_network_c *notice) :pubid(pubid), notice(notice)
        {
        }
    };

    ts::ivec2 sz0(40), sz1(40), sz2(40);
    if (const theme_rect_s * thr = themerect())
    {
        if (const button_desc_s *bd = gui->theme().get_button(CONSTASTR("copyid")))
            sz0 = bd->size;

        if (const button_desc_s *bd = gui->theme().get_button(CONSTASTR("netsetup")))
            sz1 = bd->size;

        if (const button_desc_s *bd = gui->theme().get_button(CONSTASTR("to_online")))
            sz2 = bd->size;
    }

    struct connect_handler
    {
        static bool make_offline(RID b, GUIPARAM p)
        {
            int networkid = (int)p;
            if (active_protocol_c *ap = prf().ap(networkid))
                ap->set_autoconnect(false);
            offline_online(HOLD(b).as<gui_button_c>(), networkid, false);

            return true;
        }
        static bool make_online(RID b, GUIPARAM p)
        {
            int networkid = (int)p;
            if (active_protocol_c *ap = prf().ap(networkid))
                ap->set_autoconnect(true);
            offline_online(HOLD(b).as<gui_button_c>(), networkid, true);

            return true;
        }
        static void offline_online(gui_button_c &b, int networkid, bool current_is_autoconnect)
        {
            if (current_is_autoconnect)
            {
                b.set_face_getter(BUTTON_FACE(to_offline));
                b.tooltip(TOOLTIP(TTT("Разъединить (включить режим оффлайн)", 98)));
                b.set_handler(make_offline, (GUIPARAM)networkid);
            }
            else
            {
                b.set_face_getter(BUTTON_FACE(to_online));
                b.tooltip(TOOLTIP(TTT("Соединить (включить режим онлайн)", 99)));
                b.set_handler(make_online, (GUIPARAM)networkid);
            }
        }
        static bool netsetup(RID b, GUIPARAM p)
        {
            int networkid = (int)p;
            SUMMON_DIALOG<dialog_setup_network_c>( UD_PROTOSETUP, dialog_protosetup_params_s(networkid) );
            return true;
        }
    };

    gui_button_c &b_connect = MAKE_CHILD<gui_button_c>(getrid());
    connect_handler::offline_online(b_connect, networkid, is_autoconnect);
    b_connect.leech(TSNEW(leech_dock_right_center_s, sz2.x, sz2.y, 5, -5, 0, 1));
    MODIFY(b_connect).visible(true);

    gui_button_c &b_setup = MAKE_CHILD<gui_button_c>(getrid());
    b_setup.set_constant_size(sz1);
    b_setup.set_face_getter(BUTTON_FACE(netsetup));
    b_setup.tooltip(TOOLTIP(TTT("Настроить сеть",55)));
    b_setup.leech(TSNEW(leech_at_left_s, &b_connect, 5));
    b_setup.set_handler(connect_handler::netsetup, (GUIPARAM)networkid);
    MODIFY(b_setup).visible(true);

    gui_button_c &b_copy = MAKE_CHILD<gui_button_c>(getrid());
    b_copy.set_constant_size(sz0);
    b_copy.set_customdata_obj<copydata>(pubid, this);
    b_copy.set_face_getter(BUTTON_FACE(copyid));
    b_copy.tooltip(TOOLTIP(TTT("Копировть ID в буфер обмена", 97)));

    b_copy.set_handler(DELEGATE(&b_copy.get_customdata_obj<copydata>(), copy_handler), nullptr);
    b_copy.leech(TSNEW(leech_at_left_s, &b_setup, 5));
    MODIFY(b_copy).visible(true);

    left_margin = g_app->buttons().icon[CSEX_UNKNOWN]->size.x + 6;
    setup_tail();

}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_SAVED>&p)
{
    if (p.tabi == pt_active_protocol && p.pass == 0)
    {
        auto *row = prf().get_table_active_protocol().find<true>(networkid);
        if (row == nullptr /*|| 0 != (row->other.options & active_protocol_data_s::O_SUSPENDED)*/)
            TSDEL(this);
    }

    return 0;
}

bool gui_notice_network_c::resetup(RID, GUIPARAM)
{
    setup( pubid );
    return true;
}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<GM_HEARTBEAT>&)
{
    // just check protocol available
    if (nullptr == prf().ap(networkid))
        TSDEL(this);
    return 0;
}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    if (ch.pass == 0)
    {
        if (ch.pp == PP_AVATAR)
            getengine().redraw();
    }
    if (ch.protoid == networkid && ch.pass > 0)
    {
        if (ch.pp == PP_NETWORKNAME || ch.pp == PP_USERNAME || ch.pp == PP_USERSTATUSMSG)
            DEFERRED_UNIQUE_CALL(0, DELEGATE(this, resetup), nullptr);
    }
    return 0;
}


/*virtual*/ void gui_notice_network_c::created()
{
    __super::created();
}

/*virtual*/ int gui_notice_network_c::get_height_by_width(int width) const
{
    if (width == -INT_MAX) return __super::get_height_by_width(width);
    return __super::get_height_by_width( width - left_margin );
}


/*virtual*/ bool gui_notice_network_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid())
    {
        // from submenu
        if (popupmenu && popupmenu->getrid() == rid)
        {
            if (SQ_POPUP_MENU_DIE == qp)
            {
                MODIFY(*this).highlight(false);
                bool prev = flags.is(F_OVERAVATAR);
                flags.clear(F_OVERAVATAR);
                if (prev)
                {
                    //update_text();
                    getengine().redraw();
                }
            }
        }
        return false;
    }

    switch (qp)
    {
    case SQ_DRAW:
        gui_control_c::sq_evt(qp, rid, data);
        if (m_engine)
        {
            ts::irect ca = get_client_area();

            if (flags.is(F_OVERAVATAR))
            {
                int y = (ca.size().y - left_margin) / 2;
                m_engine->draw(ts::irect(ca.lt.x, ca.lt.y+y, ca.lt.x + left_margin, ca.lt.y+y+left_margin), get_default_text_color(3));
            }

            if (contact_c *contact = contacts().find_subself(networkid))
            {
                if (const avatar_s *ava = contact->get_avatar())
                {
                    int x = (left_margin - ava->info().sz.x) / 2;
                    int y = (ca.size().y - ava->info().sz.y) / 2;
                    m_engine->draw(ca.lt + ts::ivec2(x, y), *ava, ts::irect(0, ava->info().sz), ava->alpha_pixels);
                }
                else
                {
                    button_desc_s *icon = g_app->buttons().icon[CSEX_UNKNOWN];
                    int x = (left_margin - icon->size.x) / 2;
                    icon->draw(m_engine.get(), button_desc_s::NORMAL, ts::irect(ca.lt + ts::ivec2(x,0), ca.rb), button_desc_s::ALEFT | button_desc_s::ATOP | button_desc_s::ABOTTOM);
                }
            }

            ca.lt.x += left_margin;

            draw_data_s &dd = m_engine->begin_draw();
            dd.size = ca.size();
            if (dd.size >> 0)
            {
                if (dd.size.x != textrect.size.x)
                    textrect.make_dirty(false,false,true);

                text_draw_params_s tdp;
                dd.offset += ca.lt;
                draw(dd, tdp);
            }
            m_engine->end_draw();


        }
        return true;
    case SQ_DETECT_AREA:
        if (popupmenu.expired())
        {
            ts::irect ca = get_client_area();
            ca.setwidth(left_margin);
            bool prev = flags.is(F_OVERAVATAR);
            if (prev != flags.init(F_OVERAVATAR, ca.inside(data.detectarea.pos)))
                getengine().redraw();
        }
        break;
    case SQ_MOUSE_OUT:
        {
            bool prev = flags.is(F_OVERAVATAR);
            flags.clear(F_OVERAVATAR);
            if (prev && popupmenu) flags.set(F_OVERAVATAR);
            if (prev) getengine().redraw();
        }
        break;
    case SQ_MOUSE_LUP:
        if (flags.is(F_OVERAVATAR))
        {
            struct x
            {
                static void set_self_avatar(const ts::str_c&nid)
                {
                    SUMMON_DIALOG<dialog_avaselector_c>(UD_AVASELECTOR, dialog_avasel_params_s(nid.as_int()));
                }
                static void clear_self_avatar(const ts::str_c&nid)
                {
                    int protoid = nid.as_int();
                    prf().iterate_aps([&](const active_protocol_c &cap) {
                        if (protoid == 0 || protoid == cap.getid())
                            if (active_protocol_c *ap = prf().ap(cap.getid())) // bad
                                ap->set_avatar(ts::blob_c());
                    });
                    gmsg<ISOGM_CHANGED_PROFILEPARAM>(protoid, PP_AVATAR).send();
                }
            };

            if (contact_c *contact = contacts().find_subself(networkid))
            {

                if (contact->get_avatar())
                {
                    menu_c m;
                    m.add(TTT("Изменить аватар", 219), 0, x::set_self_avatar, ts::amake<int>(networkid));
                    m.add(TTT("Убрать аватар", 220), 0, x::clear_self_avatar, ts::amake<int>(networkid));
                    popupmenu = &gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), m);
                    popupmenu->leech(this);
                }
                else
                {
                    SUMMON_DIALOG<dialog_avaselector_c>(UD_AVASELECTOR, dialog_avasel_params_s(networkid));
                    flags.clear(F_OVERAVATAR);
                }
            }
            getengine().redraw();

        }
        break;
    case SQ_RECT_CHANGED:
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, setup_tail), 0);
        break;
    }

    return __super::sq_evt(qp,rid,data);
}

void gui_notice_network_c::flash()
{
    flashing = 4;
    flash_pereflash(RID(), nullptr);
}

bool gui_notice_network_c::flash_pereflash(RID, GUIPARAM)
{
    --flashing;
    if (flashing > 0) DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, flash_pereflash), nullptr);

    ts::wstr_c text = textrect.get_text();

    int i0 = text.find_pos(CONSTWSTR("<null=1>")) + 8;
    int i1 = text.find_pos(CONSTWSTR("<null=2>"));
    int i2 = text.find_pos(CONSTWSTR("<null=3>")) + 8;
    int i3 = text.find_pos(CONSTWSTR("<null=4>"));

    ASSERT(i1 >= i0 && i2 > i1 && i3 >= i2);

    if (flashing & 1)
    {
        text.replace(i2, i3 - i2, CONSTWSTR("</color>"));
        text.replace(i0, i1 - i0, maketag_color<ts::wchar>(get_default_text_color(2)));
    }
    else
    {
        // cut flash color
        if (i3 > i2) text.cut(i2, i3 - i2);
        if (i1 > i0) text.cut(i0, i1 - i0);
    }
    textrect.set_text_only(text, false);

    getengine().redraw();
    return true;
}



gui_noticelist_c::~gui_noticelist_c()
{
}

/*virtual*/ ts::ivec2 gui_noticelist_c::get_min_size() const
{
    int h = 0;
    int w = width_for_children();
    for (rectengine_c *e : getengine())
        if (e) { h = ts::tmax(h, e->getrect().get_height_by_width(w)); }

    return ts::ivec2(70,h);
}
/*virtual*/ ts::ivec2 gui_noticelist_c::get_max_size() const
{
    ts::ivec2 sz = __super::get_max_size();
    if (!owner || owner->getkey().is_self())
    {
        sz.y = 0;
        int w = width_for_children();
        for (rectengine_c *e : getengine())
            if (e) { sz.y += e->getrect().get_height_by_width(w); }
    } else
        sz.y = get_min_size().y;
    return sz;
}

/*virtual*/ void gui_noticelist_c::created()
{
    set_theme_rect(CONSTASTR("noticelist"), false);
    defaultthrdraw = DTHRO_BASE;
    __super::created();
}

/*virtual*/ bool gui_noticelist_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch (qp)
    {
    case SQ_VISIBILITY_CHANGED:
        getparent().call_children_repos();
        break;
    }

    return __super::sq_evt(qp,rid,data);
}

gui_notice_c &gui_noticelist_c::create_notice(notice_e n)
{
    MODIFY( *this ).visible(true);
    ASSERT(NOTICE_NETWORK != n);
    return MAKE_CHILD<gui_notice_c>(getrid(), n);
}


void gui_noticelist_c::clear_list(bool hide)
{
    if (hide) MODIFY(*this).visible(false);
    for (rectengine_c *e : getengine())
        if (e) TSDEL(e);
    getengine().redraw();
}

static bool self_selected = false;
ts::uint32 gui_noticelist_c::gm_handler(gmsg<ISOGM_PROTO_LOADED> &)
{
    if (owner && owner->getkey().is_self())
    {
        // just update self, if self-contactitem selected
        DEFERRED_EXECUTION_BLOCK_BEGIN(0.4)
            if (self_selected)
                gmsg<ISOGM_SELECT_CONTACT>(&contacts().get_self()).send();
        DEFERRED_EXECUTION_BLOCK_END(0)
    }
    return 0;
}

ts::uint32 gui_noticelist_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> & p)
{
    if (owner == nullptr) return 0;

    if (owner->getkey().is_self())
    {
        // just update self, if self-contactitem selected
        DEFERRED_EXECUTION_BLOCK_BEGIN(0.4)
            if (self_selected)
            gmsg<ISOGM_SELECT_CONTACT>(&contacts().get_self()).send();
        DEFERRED_EXECUTION_BLOCK_END(0)
    }

    contact_c *sender = nullptr;
    if (p.contact->is_rootcontact())
    {
        if (owner != p.contact) return 0;

        if (p.contact->get_state() == CS_ROTTEN)
        {
            clear_list();
            owner = nullptr;
            return 0;
        }

        owner->subiterate( [&](contact_c *c) {
            if (c->get_state() == CS_REJECTED)
            {
                sender = c;
            }
        } );

    } else if (owner->subpresent(p.contact->getkey()))
    {
        sender = p.contact;
    }

    if (sender && !owner->getkey().is_group())
    {
        if (sender->get_state() == CS_ROTTEN)
        {
            ASSERT(sender->getmeta() && sender->getmeta()->subcount() == 1);
            clear_list();
            owner = nullptr;
            return 0;
        }

        gui_notice_c *nrej = nullptr;
        for (rectengine_c *e : getengine())
        {
            if (e)
            {
                gui_notice_c &n = *ts::ptr_cast<gui_notice_c *>(&e->getrect());
                if (n.get_notice() == NOTICE_FRIEND_REQUEST_SEND_OR_REJECT)
                {
                    nrej = &n;
                    if (sender->get_state() != CS_REJECTED && sender->get_state() != CS_INVITE_SEND)
                    {
                        TSDEL(e);
                        return 0;
                    }
                }
            }
        }
        if ((sender->get_state() == CS_REJECTED || sender->get_state() == CS_INVITE_SEND))
        {
            if (nrej == nullptr)
                nrej = &create_notice(NOTICE_FRIEND_REQUEST_SEND_OR_REJECT);

            nrej->setup(sender);
            children_repos();
        }

    }

    return 0;
}

void gui_noticelist_c::refresh()
{
    children_repos();

    for (rectengine_c *e : getengine())
        if (e) return;

    MODIFY(*this).visible(false);
}

ts::uint32 gui_noticelist_c::gm_handler(gmsg<GM_UI_EVENT> & e)
{
    if (UE_MAXIMIZED == e.evt || UE_NORMALIZED == e.evt)
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
        if (self_selected)
            gmsg<ISOGM_SELECT_CONTACT>(&contacts().get_self()).send();
        DEFERRED_EXECUTION_BLOCK_END(0)
    }
    return 0;
}

ts::uint32 gui_noticelist_c::gm_handler(gmsg<ISOGM_NOTICE> & n)
{
    if (n.n == NOTICE_NETWORK)
    {
        clear_list(false);
        owner = n.owner;

        if (owner->getkey().is_self())
        {
            if (g_app->newversion())
            {
                not_at_end();
                gui_notice_c &n = create_notice(NOTICE_NEWVERSION);
                n.setup( cfg().autoupdate_newver() );
            
            }

            for (auto &row : prf().get_table_active_protocol())
            {
                //if (0 != (row.other.options & active_protocol_data_s::O_SUSPENDED))
                //    continue;

                ts::str_c pubid = contacts().find_pubid(row.id);
                if (!pubid.is_empty())
                {
                    not_at_end();
                    MODIFY(*this).visible(true);
                    gui_notice_network_c &n = MAKE_CHILD<gui_notice_network_c>(getrid());
                    n.setup(pubid);
                }
            }
        } else
        {
            owner->subiterate([this](contact_c *c) {
                if (c->is_calltone())
                {
                    gui_notice_c &n = create_notice(NOTICE_CALL);
                    n.setup(c);
                }
            });

            g_app->enum_file_transfers_by_historian(owner->getkey(), [this](file_transfer_s &ftr) {
                if (ftr.upload || ftr.accepted) return;
                contact_c *sender = contacts().find(ftr.sender);
                if (sender)
                {
                    gui_notice_c &n = create_notice(NOTICE_FILE);
                    n.setup(to_utf8(ftr.filename), sender, ftr.utag);
                }
            });

            if (!owner->getkey().is_group())
                owner->subiterate([this](contact_c *c) {
                    if (c->get_state() == CS_REJECTED || c->get_state() == CS_INVITE_SEND)
                    {
                        gui_notice_c &n = create_notice(NOTICE_FRIEND_REQUEST_SEND_OR_REJECT);
                        n.setup(c);
                    }
                });
        }

        refresh();

        return 0;
    }

    if (owner == n.owner)
    {
        gui_notice_c &nn = create_notice(n.n);
        n.just_created = &nn;
        nn.setup(n.text, n.sender, n.utag);
        if (n.sender == nullptr || n.utag)
        {
            getengine().child_move_top(&nn.getengine());
            refresh();
        }
    }
    
    return 0;
}

////////////////////////// gui_message_item_c

MAKE_CHILD<gui_message_item_c>::~MAKE_CHILD()
{
    get().set_selectable();
    MODIFY(get()).visible(true);
}

gui_message_item_c::~gui_message_item_c()
{
    if (gui)
    {
        gui->delete_event(DELEGATE(this, b_pause_unpause));
        gui->delete_event(DELEGATE(this, try_select_link));
    }
}

/*virtual*/ ts::ivec2 gui_message_item_c::get_min_size() const
{
    return ts::ivec2(60, height ? height : 60);
}

/*virtual*/ ts::ivec2 gui_message_item_c::get_max_size() const
{
    ts::ivec2 m = __super::get_max_size();
    m.y = get_min_size().y;
    return m;
}

void gui_message_item_c::created()
{
    defaultthrdraw = DTHRO_BORDER | DTHRO_CENTER;
    flags.set(F_DIRTY_HEIGHT_CACHE);
    __super::created();
}

void gui_message_item_c::rebuild_text()
{
    ts::wstr_c newtext;
    if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
    {
        ts::wstr_c pret, postt;
        prepare_str_prefix(pret, postt);

        for (record &r : records)
            r.append(newtext, pret, postt);

        timestr.clear();

    } else if (records.size())
    {
         timestrwidth = records.get(0).append(newtext, timestr);
    }

    textrect.set_text_only(newtext, false);
}


static time_t readtime; // ugly static... but it is fastest
static contact_c * readtime_historian = nullptr;

void gui_message_item_c::ctx_menu_golink(const ts::str_c & lnk)
{
    open_link(from_utf8(lnk));
    gui->selcore().flash_and_clear_selection();
}
void gui_message_item_c::ctx_menu_copylink(const ts::str_c & lnk)
{
    ts::set_clipboard_text(ts::from_utf8(lnk));
    gui->selcore().flash_and_clear_selection();
}

void gui_message_item_c::ctx_menu_copymessage(const ts::str_c &msg)
{
    ts::set_clipboard_text(ts::wstr_c().set_as_utf8(msg));
}

void gui_message_item_c::ctx_menu_delmessage(const ts::str_c &mutag)
{
    uint64 utag = 0;
    mutag.hex2buf<sizeof(uint64)>((ts::uint8 *)&utag);

    historian->del_history(utag);
    historian->subiterate([&](contact_c *c){
        if (active_protocol_c *ap = prf().ap(c->getkey().protoid)) // all protocols
            ap->del_message(utag);
    });

    if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.msgitem_utag == utag; }))
        if (row->deleted())
            prf().changed();

    if (remove_utag(utag))
    {
        RID parent = getparent();
        if (records.size() == 0 || subtype == ST_RECV_FILE || subtype == ST_SEND_FILE)
        {
            if (!flags.is(F_NO_AUTHOR))
                if (rectengine_c *next = HOLD(parent).engine().get_next_child( &getengine() ))
                    if (gui_message_item_c *nitm = ts::ptr_cast<gui_message_item_c *>( &next->getrect() ))
                        if (nitm->get_author() == get_author())
                            nitm->set_no_author(false), next->redraw();

            TSDEL(this);
        }
        parent.call_children_repos();
    }
}

bool gui_message_item_c::try_select_link(RID, GUIPARAM p)
{
    ts::ivec2 *pt = (ts::ivec2 *)gui->lock_temp_buf((int)p);
    if (textrect.is_dirty_glyphs())
    {
        if (pt)
        {
            DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, try_select_link), p );
        }
        return true;
    }
    if (pt)
    {
        ts::ivec2 pp = get_link_pos_under_cursor(*pt);
        gui->selcore().select_by_charinds(this, pp.r0, pp.r1);
    }

    return true;
}

/*virtual*/ bool gui_message_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid())
    {
        // from submenu
        if (popupmenu && popupmenu->getrid() == rid)
        {
            if (SQ_POPUP_MENU_DIE == qp)
            {
                // only link menu here
                // clear selection
                if (!gui->selcore().flashing)
                    gui->selcore().clear_selection();
            }
        }
        return false;
    }

    bool del_only = false;

    switch (qp)
    {
    case SQ_DRAW:
        {
            if (historian && readtime_historian == historian)
                for (const record &r : records)
                    if (r.time >= readtime) readtime = r.time+1; // refresh time of items we see

            switch (subtype)
            {
            case gui_message_item_c::ST_RECV_FILE:
            case gui_message_item_c::ST_SEND_FILE:
                gui_control_c::sq_evt(qp, rid, data);
                if (m_engine)
                {
                    ts::irect ca = get_client_area();
                    draw_data_s &dd = getengine().begin_draw();
                    ts::ivec2 oo(dd.offset); 

                    dd.offset += ca.lt;
                    dd.size = ca.size();

                    text_draw_params_s tdp;
                    tdp.rectupdate = DELEGATE(this, updrect);
                    ts::TSCOLOR c = get_default_text_color(0);
                    tdp.forecolor = &c;

                    draw(dd, tdp);

                    if (!prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
                    {
                        dd.size.x = timestrwidth + 1;
                        dd.size.y = g_app->font_conv_time->height();
                        dd.offset = oo + ca.rb - dd.size;

                        tdp.font = g_app->font_conv_time;
                        ts::TSCOLOR c = get_default_text_color(2);
                        tdp.forecolor = &c;
                        m_engine->draw(timestr, tdp);
                    }


                    getengine().end_draw();
                }
                break;
            case gui_message_item_c::ST_JUST_TEXT:
                __super::sq_evt(qp, rid, data);

                if (!prf().get_msg_options().is(MSGOP_JOIN_MESSAGES) && m_engine)
                {
                    ts::irect ca = get_client_area();
                    draw_data_s &dd = getengine().begin_draw();

                    dd.size.x = timestrwidth + 1;
                    dd.size.y = g_app->font_conv_time->height();
                    dd.offset += ca.rb - dd.size;

                    text_draw_params_s tdp;
                    tdp.font = g_app->font_conv_time;
                    ts::TSCOLOR c = get_default_text_color(2);
                    tdp.forecolor = &c;
                    m_engine->draw(timestr, tdp);

                    getengine().end_draw();
                }

                break;
            case gui_message_item_c::ST_CONVERSATION:
                gui_control_c::sq_evt(qp, rid, data);
                if (m_engine)
                {
                    //button_desc_s::states onlinestate = ((int)this & 16) ? button_desc_s::NORMAL : button_desc_s::DISABLED;
                    //m_engine->draw(ca.rb - online->rects[onlinestate].size(), online->src, online->rects[onlinestate], online->is_alphablend(onlinestate));
                    //m_engine->draw(ts::ivec2(ca.lt.x, ca.rb.y - icon->size.y), icon->src, icon->size, icon->is_alphablend(button_desc_s::NORMAL));

                    ts::irect ca = get_client_area();
                    if (ca.size() >> 0)
                    {
                        draw_data_s &dd = m_engine->begin_draw();

                        text_draw_params_s tdp;
                        ts::ivec2 sz(0), oo(dd.offset); 

                        dd.offset += ca.lt;
                        dd.size = ca.size();

                        if (!flags.is(F_NO_AUTHOR))
                        {
                            tdp.font = g_app->font_conv_name;
                            ts::TSCOLOR c = get_default_text_color( 0 );
                            tdp.forecolor = &c;
                            tdp.sz = &sz;
                            m_engine->draw(hdr(), tdp);
                        }

                        sz.x = m_left;
                        sz.y += m_top;
                        dd.offset += sz;
                        dd.size -= sz;
                        tdp.font = g_app->font_conv_text;
                        tdp.forecolor = nullptr; //get_default_text_color();
                        tdp.sz = nullptr;
                        ts::flags32_s f; f.set(ts::TO_LASTLINEADDH);
                        tdp.textoptions = &f;
                        tdp.rectupdate = DELEGATE(this, updrect_emoticons);

                        __super::draw( dd, tdp );

                        glyphs_pos = sz + ca.lt;
                        if (gui->selcore().owner == this) gui->selcore().glyphs_pos = glyphs_pos;

                        if (!prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
                        {
                            dd.size.x = timestrwidth + 1;
                            dd.size.y = g_app->font_conv_time->height();
                            dd.offset = oo + ca.rb - dd.size;
                                
                            tdp.font = g_app->font_conv_time;
                            ts::TSCOLOR c = get_default_text_color(2);
                            tdp.forecolor = &c;
                            tdp.sz = nullptr;
                            tdp.rectupdate = ts::UPDATE_RECTANGLE();
                            m_engine->draw(timestr, tdp);
                        }

                        m_engine->end_draw();
                    }

                }
                break;
            }
            
        }
        return true;
    case SQ_MOUSE_LUP:
        if (!some_selected() && check_overlink(to_local(data.mouse.screenpos)))
        {
            ts::str_c lnk = get_link_under_cursor(to_local(data.mouse.screenpos));
            if (!lnk.is_empty()) ctx_menu_golink(lnk);
        }
        break;
    case SQ_MOUSE_RUP:
        if ((records.size() == 1 && records.get(0).text.is_empty()) || subtype != ST_CONVERSATION)
        {
            if (records.size() == 0) break;
            if (ST_SEND_FILE == subtype || ST_RECV_FILE == subtype)
                if (g_app->find_file_transfer_by_msgutag(records.get(0).utag))
                    break; // do not delete file transfer in progress
            del_only = true;
        } else if (!some_selected() && check_overlink(to_local(data.mouse.screenpos)))
        {
            if (popupmenu)
            {
                TSDEL(popupmenu);
                return true;
            }
            menu_c mnu;

            ts::str_c lnk = get_link_under_cursor(to_local(data.mouse.screenpos));
            if (!lnk.is_empty())
            {
                mnu.add(TTT("Перейти по ссылке", 202), 0, DELEGATE(this, ctx_menu_golink), lnk);
                mnu.add(TTT("Копировать ссылку", 203), 0, DELEGATE(this, ctx_menu_copylink), lnk);

                popupmenu = &gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), mnu);
                popupmenu->leech(this);
            }

            if (overlink >= 0)
            {
                overlink = -1;
                textrect.make_dirty(true);
                getengine().redraw();
            }

            if (popupmenu)
            {
                int b = gui->get_temp_buf(1.0, sizeof(ts::ivec2));
                *(ts::ivec2 *)gui->lock_temp_buf(b) = to_local(data.mouse.screenpos);
                try_select_link(RID(), (GUIPARAM)b);
            }

            return true;
        }
        if (!getengine().mtrack(getrid(), MTT_SBMOVE) && !getengine().mtrack(getrid(), MTT_TEXTSELECT))
        {
            if (popupmenu)
            {
                TSDEL(popupmenu);
                return true;
            }
            menu_c mnu;

            uint64 mutag = 0;
            if (del_only)
            {
                mutag = records.get(0).utag;
            } else
            {
                bool some_selection = some_selected();

                mnu.add(gui->app_loclabel(LL_CTXMENU_COPY), (!some_selection) ? MIF_DISABLED : 0, DELEGATE(this, ctx_menu_copy));
                ts::str_c msg = to_utf8(get_message_under_cursor(to_local(data.mouse.screenpos), mutag));
                gui->app_prepare_text_for_copy(msg);
                mnu.add(TTT("Копировать сообщение", 205), 0, DELEGATE(this, ctx_menu_copymessage), msg);
                mnu.add_separator();
            }
            mnu.add(TTT("Удалить сообщение",221), 0, DELEGATE(this, ctx_menu_delmessage), ts::str_c().append_as_hex(&mutag, sizeof(mutag)));

            popupmenu = &gui_popup_menu_c::show(ts::ivec3(gui->get_cursor_pos(), 0), mnu);

        }
        break;
    case SQ_MOUSE_RDOWN:
    case SQ_MOUSE_LDOWN:
        if (!popupmenu && !some_selected() && check_overlink(to_local(data.mouse.screenpos)))
            return true;
        // no break here!
    case SQ_MOUSE_OUT:
        if (!popupmenu && textrect.get_text().find_pos(CONSTWSTR("<cstm=a")) >= 0)
        {
            if (overlink >= 0)
            {
                overlink = -1;
                textrect.make_dirty();
                getengine().redraw();
            }
            return gui_label_c::sq_evt(qp,rid,data);
        }
        break;
    case SQ_DETECT_AREA:
        if (!popupmenu && textrect.get_text().find_pos(CONSTWSTR("<cstm=a")) >= 0 && !getengine().mtrack(getrid(), MTT_TEXTSELECT) && !some_selected())
        {
            if (check_overlink(data.detectarea.pos))
            {
                data.detectarea.area = AREA_HAND;
                return true;
            }
        }
        break;

    //case SQ_RECT_CHANGING:
    //    {
    //        int h = get_height_by_width( data.rectchg.rect.get().width() );
    //        if (h != height)
    //        {
    //            height = h;
    //            data.rectchg.rect.get().setheight(h);
    //        }
    //        break;
    //
    //    }
    case SQ_RECT_CHANGED: {
        int h = get_height_by_width( -INT_MAX );
        ASSERT( h >= getprops().size().y );
        } return true;
    }


    return __super::sq_evt(qp, rid, data);
}

template<typename SCORE> void set_date(ts::str_t<ts::wchar, SCORE> & tstr, const ts::wstr_c &fmt, const tm &tt)
{
    SYSTEMTIME st;
    st.wYear = (ts::uint16)(tt.tm_year + 1900);
    st.wMonth = (ts::uint16)(tt.tm_mon + 1);
    st.wDayOfWeek = (ts::uint16)tt.tm_wday;
    st.wDay = (ts::uint16)tt.tm_mday;
    st.wHour = (ts::uint16)tt.tm_hour;
    st.wMinute = (ts::uint16)tt.tm_min;
    st.wSecond = (ts::uint16)tt.tm_sec;
    st.wMilliseconds = 0;

    if (tstr.get_capacity() > 32)
        tstr.set_length( tstr.get_capacity() - 1 );
    else
        tstr.set_length( 64 );
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, &st, fmt, tstr.str(), tstr.get_capacity() - 1);
    tstr.set_length();
}


void gui_message_item_c::init_date_separator( const tm &tmtm )
{
    ASSERT(MTA_DATE_SEPARATOR == mt);

    subtype = ST_JUST_TEXT;
    flags.set(F_DIRTY_HEIGHT_CACHE);

    ts::swstr_t<-128> tstr;
    set_date(tstr, from_utf8(prf().date_sep_template()), tmtm);

    ts::wstr_c newtext( CONSTWSTR("<p=c>") );
    newtext.append(tstr);
    textrect.set_text_only(newtext, false);
}

void gui_message_item_c::init_request( const ts::str_c &pre_utf8 )
{
    ASSERT(MTA_OLD_REQUEST == mt);
    ASSERT(MTA_OLD_REQUEST == mt || author->authorized());
    subtype = ST_JUST_TEXT;
    flags.set(F_DIRTY_HEIGHT_CACHE);

    ts::wstr_c t(CONSTWSTR("<p=c>"));
    if (pre_utf8.equals(CONSTASTR("\1restorekey")))
    {
        t.append(TTT("Ключ восстановлен",199));
    } else
    {
        ts::str_c message = pre_utf8;
        text_adapt_user_input(message);
        t.append(from_utf8(message));
    }

    textrect.set_text_only(t,false);
}

void gui_message_item_c::init_load( int n_load )
{
    ASSERT(MTA_SPECIAL == mt);
    subtype = ST_JUST_TEXT;
    flags.set(F_DIRTY_HEIGHT_CACHE);
    addheight = 40;

    gui_button_c &b_load = MAKE_CHILD<gui_button_c>(getrid());
    b_load.set_text(TTT("Загрузить еще $ сообщений",124) / ts::wstr_c().set_as_int(n_load));
    b_load.set_face_getter(BUTTON_FACE(button));
    b_load.set_handler(DELEGATE(author.get(), b_load), (GUIPARAM)n_load);
    b_load.leech(TSNEW(leech_dock_bottom_center_s, 300, 30, -5, 5, 0, 1));
    MODIFY(b_load).visible(true);
}

bool gui_message_item_c::with_utag(uint64 utag) const
{
    for (const record &rec : records)
        if (rec.utag == utag) return true;
    return false;
}

bool gui_message_item_c::remove_utag(uint64 utag)
{
    int cnt = records.size();
    for (int i=0;i<cnt;++i)
    {
        record &rec = records.get(i);
        if (rec.utag == utag)
        {
            records.remove_slow(i);
            flags.set(F_DIRTY_HEIGHT_CACHE);
            rebuild_text();
            return true;
        }
    }
    return false;
}

bool gui_message_item_c::delivered(uint64 utag)
{
    for(record &rec : records)
    {
        if (rec.utag == utag)
        {
            if (rec.undelivered == 0) return false;
            rec.undelivered = 0;

            rebuild_text();

            flags.set(F_DIRTY_HEIGHT_CACHE);

            int h = get_height_by_width(get_client_area().width());
            if (h != height)
            {
                height = h;
                MODIFY(*this).sizeh(height);
            }

            return true;
        }
    }

    return false;
}

ts::uint16 gui_message_item_c::record::append( ts::wstr_c &t, ts::wstr_c &pret, const ts::wsptr &postt )
{
    ts::wstr_c tstr;
    tm tt;
    _localtime64_s(&tt, &time);
    if ( prf().get_msg_options().is(MSGOP_SHOW_DATE) )
    {
        set_date(tstr,from_utf8(prf().date_msg_template()),tt);
        tstr.append_char(' ');
    }
    tstr.append_as_uint(tt.tm_hour);
    if (tt.tm_min < 10)
        tstr.append(CONSTWSTR(":0"));
    else
        tstr.append_char(':');
    tstr.append_as_uint(tt.tm_min);

    ts::uint16 timestrwidth = 0;
    if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
        t.append(pret).append(tstr).append(postt);
    else
    {
        ts::ivec2 timeszie = g_app->tr().calc_text_size( *g_app->font_conv_time, tstr, 1024, 0, nullptr );
        timestrwidth = (ts::uint16)timeszie.x;
        pret = tstr;
    }

    if (undelivered)
    {
        t.append(maketag_color<ts::wchar>(undelivered)).append(from_utf8(text)).append(CONSTWSTR("</color>"));
    }
    else
        t.append(from_utf8(text));

    if (timestrwidth)
        t.append(CONSTWSTR("<nbsp=")).append_as_uint(timestrwidth + ADDTIMESPACE).append_char('>');

    t.append(CONSTWSTR("<null=t")).append_as_hex(&utag, sizeof(utag)).append_char('>');

    return timestrwidth;
}

static int prepare_link(ts::str_c &message, int i, int n)
{
    int cnt = message.get_length();
    int j=i;
    for(;j<cnt;++j)
    {
        ts::wchar c = message.get_char(j);
        if (ts::CHARz_find(L" \\<>\r\n\t", c)>=0) break;
    }
    ts::sstr_t<-128> inst(CONSTASTR("<cstm=b"));
    inst.append_as_uint(n).append(CONSTASTR(">"));
    message.insert(j,inst);
    inst.set_char(6,'a');
    message.insert(i,inst);
    return j + inst.get_length() * 2;
}

static void parse_links(ts::str_c &message, bool reset_n)
{
    static int n = 0;
    if (reset_n) n = 0;
    int i = 0;
    for(;;)
    {
        int j = message.find_pos(i, CONSTASTR("http://"));
        if (j>=0)
        {
            i = prepare_link(message, j, n);
            ++n;
            continue;
        }
        j = message.find_pos(i, CONSTASTR("https://"));
        if (j >= 0)
        {
            i = prepare_link(message, j, n);
            ++n;
            continue;
        }
        j = message.find_pos(i, CONSTASTR("ftp://"));
        if (j >= 0)
        {
            i = prepare_link(message, j, n);
            ++n;
            continue;
        }
        j = message.find_pos(i, CONSTASTR("www."));
        if (j == 0 || (j > 0 && message.get_char(j-1) == ' '))
        {
            i = prepare_link(message, j, n);
            ++n;
            continue;
        }
        break;
    }
}

ts::pwstr_c gui_message_item_c::get_message_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const
{
    ts::ivec2 p = get_message_pos_under_cursor(localpos, mutag);
    if (p.r0 >= 0 && p.r1 >= p.r0) return textrect.get_text().substr(p.r0, p.r1);
    return ts::pwstr_c();
}


ts::ivec2 gui_message_item_c::get_message_pos_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const
{
    if (textrect.is_dirty_glyphs()) return ts::ivec2(-1);
    ts::irect clar = get_client_area();
    if (clar.inside(localpos))
    {
        const ts::GLYPHS &glyphs = get_glyphs();
        ts::ivec2 cp = localpos - glyphs_pos;
        int glyph_under_cursor = ts::glyphs_nearest_glyph(glyphs, cp);
        int char_index = ts::glyphs_get_charindex(glyphs, glyph_under_cursor);
        if (char_index >= 0 && char_index < textrect.get_text().get_length())
            return extract_message(char_index, mutag);
    }
    return ts::ivec2(-1);
}

ts::ivec2 gui_message_item_c::extract_message(int chari, uint64 &mutag) const
{
    int rite = textrect.get_text().find_pos(chari, CONSTWSTR("<p><r>"));
    if (rite < 0) rite = textrect.get_text().get_length();
    int left = textrect.get_text().substr(0, chari).find_last_pos(CONSTWSTR("</r>")) + 4;

    int r2 = rite - 8 - 16;
    if (CHECK(textrect.get_text().substr(r2, r2+7).equals(CONSTWSTR("<null=t"))))
    {
        rite = r2;
        ts::pwstr_c hex = textrect.get_text().substr(r2+7,r2+7+16);
        hex.hex2buf<sizeof(uint64)>((ts::uint8 *)&mutag);
    }
    return ts::ivec2(left, rite);
}

/*virtual*/ void gui_message_item_c::get_link_prolog(ts::wstr_c & r, int linknum) const
{
    r.set(CONSTWSTR("<b>"));
    if (linknum == overlink)
    {
        ts::TSCOLOR c = get_default_text_color(3);
        r.append(CONSTWSTR("<color=#")).append_as_hex(ts::RED(c))
            .append_as_hex(ts::GREEN(c))
            .append_as_hex(ts::BLUE(c))
            .append_as_hex(ts::ALPHA(c))
            .append(CONSTWSTR("><u>"));
    }

}
/*virtual*/ void gui_message_item_c::get_link_epilog(ts::wstr_c & r, int linknum) const
{
    if (linknum == overlink)
        r.set(CONSTWSTR("</b></u></color>"));
    else
        r.set(CONSTWSTR("</b>"));

}

void gui_message_item_c::append_text( const post_s &post, bool resize_now )
{
    bool use0rec = (records.size() && records.get(0).utag == post.utag);
    record &rec = use0rec ? records.get(0) : records.add();
    if (!use0rec)
    {
        rec.utag = post.utag;
        rec.time = post.time;
    }

    if (MTA_UNDELIVERED_MESSAGE != post.mt())
        mt = post.mt();

    textrect.set_options(ts::TO_LASTLINEADDH);

    switch(mt)
    {
    case MTA_OLD_REQUEST:
        init_request(post.message_utf8);
        break;
    case MTA_ACCEPT_OK:
    case MTA_ACCEPTED:
        {
            subtype = ST_JUST_TEXT;
            ts::wstr_c newtext(512,false);
            newtext.set(CONSTWSTR("<p=c>"));
            ts::str_c aname = author->get_name();
            text_adapt_user_input(aname);
            if (MTA_ACCEPTED == mt)
                newtext.append(TTT("Вы получили разрешение на добавление контакта [b]$[/b] в список контактов.", 91) / from_utf8(aname));
            if (MTA_ACCEPT_OK == mt)
                newtext.append(TTT("Вы разрешили добавление контакта [b]$[/b] в список контактов.", 90) / from_utf8(aname));
            textrect.set_text_only(newtext,false);
        }
        break;
    case MTA_INCOMING_CALL_REJECTED:
    case MTA_INCOMING_CALL_CANCELED:
    case MTA_CALL_ACCEPTED:
    case MTA_HANGUP:
        {
            subtype = ST_JUST_TEXT;

            ts::wstr_c newtext(512, false);
            message_prefix(newtext, post.time);
            newtext.append(CONSTWSTR("<img=call,-1>"));
            ts::str_c aname = author->get_name();
            text_adapt_user_input(aname);

            if (MTA_INCOMING_CALL_CANCELED == mt)
                newtext.append(TTT("Звонок не удался",137));
            else if (MTA_INCOMING_CALL_REJECTED == mt)
                newtext.append(TTT("Отказ от звонка",135));
            else if (MTA_CALL_ACCEPTED == mt)
                newtext.append(TTT("Начат разговор с $",138)/from_utf8(aname));
            else if (MTA_HANGUP == mt)
                newtext.append(TTT("Закончен разговор с $",139)/from_utf8(aname));

            message_postfix(newtext);

            textrect.set_text_only(newtext, false);

        }
        break;
    case MTA_INCOMING_CALL:
    case MTA_FRIEND_REQUEST:
    case MTA_CALL_ACCEPTED__:
        FORBIDDEN();
        break;
    case MTA_RECV_FILE:
        {
            subtype = ST_RECV_FILE;
            rec.text = post.message_utf8;
            update_text();
        }
        break;
    case MTA_SEND_FILE:
        {
            subtype = ST_SEND_FILE;
            rec.text = post.message_utf8;
            update_text();
        }
    break;
    default:
        {
            subtype = ST_CONVERSATION;
            textrect.set_font(g_app->font_conv_text);

            ts::str_c message = post.message_utf8;
            if (message.is_empty()) message.set(CONSTASTR("error"));
            else {
                text_adapt_user_input(message);
                emoti().parse(message);
                parse_links(message, records.size() == 1);
            }

            rec.text = message;
            rec.undelivered = post.type == MTA_UNDELIVERED_MESSAGE ? get_default_text_color(1) : 0;

            ts::wstr_c newtext(textrect.get_text());
            if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
            {
                ts::wstr_c pret, postt;
                prepare_str_prefix(pret, postt);
                rec.append(newtext, pret, postt);
                bool rebuild = false;
                if (records.size() >= 2 && rec.time < records.get(records.size() - 2).time)
                    if (records.sort())
                    {
                        rebuild = true;
                        rebuild_text();
                    }

                if (!rebuild) textrect.set_text_only(newtext, false);
                timestr.clear();

            } else
            {
                timestrwidth = rec.append(newtext, timestr);
                textrect.set_text_only(newtext, false);
            }

        }
        break;
    
    }

    flags.set(F_DIRTY_HEIGHT_CACHE);

    if (resize_now)
    {
        int h = get_height_by_width(get_client_area().width());
        if (h != height)
        {
            height = h;
            MODIFY(*this).sizeh(height);
        }
    }
}

bool gui_message_item_c::message_prefix(ts::wstr_c &newtext, time_t posttime)
{
    if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
    {
        newtext.set(CONSTWSTR("<r><font=conv_text><color=#"));
        ts::TSCOLOR c = get_default_text_color(2);
        newtext.append_as_hex(ts::RED(c)).append_as_hex(ts::GREEN(c)).append_as_hex(ts::BLUE(c)).append_as_hex(ts::ALPHA(c)).append(CONSTWSTR("> "));
    } else
        newtext.clear();

    tm tt;
    _localtime64_s(&tt, &posttime);
    if (prf().get_msg_options().is(MSGOP_SHOW_DATE))
    {
        ts::swstr_t<-128> tstr;
        set_date(tstr, from_utf8(prf().date_msg_template()), tt);
        newtext.append(tstr).append_char(' ');
    }

    newtext.append_as_uint(tt.tm_hour);
    if (tt.tm_min < 10)
        newtext.append(CONSTWSTR(":0"));
    else
        newtext.append_char(':');
    newtext.append_as_uint(tt.tm_min);

    if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
    {
        newtext.append(CONSTWSTR("</color></font></r>"));
        timestrwidth = 0;
        timestr.clear();
        return true;

    } else
    {
        timestr = newtext;
        newtext.clear();
        ts::ivec2 timeszie = g_app->tr().calc_text_size(*g_app->font_conv_time, timestr, 1024, 0, nullptr);
        timestrwidth = (ts::uint16)timeszie.x;
        return false;
    }
}

void gui_message_item_c::message_postfix(ts::wstr_c &newtext)
{
    if (timestrwidth)
        newtext.append( CONSTWSTR("<nbsp=") ).append_as_uint(timestrwidth + ADDTIMESPACE).append_char('>');
}

bool gui_message_item_c::b_explore(RID, GUIPARAM)
{
    ASSERT(records.size());
    record &rec = records.get(0);
    if (rec.text.get_char(0) == '*' || rec.text.get_char(0) == '?') return true;
    ts::wstr_c fn = from_utf8(rec.text);
    if (ts::is_file_exists(fn))
    {
        ShellExecuteW(nullptr, L"open", L"explorer", CONSTWSTR("/select,") + ts::fn_autoquote(ts::fn_get_name_with_ext(fn)), ts::fn_get_path(fn), SW_SHOWDEFAULT);
    } else
    {
        ts::wstr_c path = fn_get_path(fn);
        ts::fix_path(path, FNO_NORMALIZE);
        for(; !ts::dir_present(path);)
        {
            if (path.find_pos(NATIVE_SLASH) < 0) return true;
            path.trunc_char(NATIVE_SLASH);
            path = fn_get_path(path);
        }
        ShellExecuteW(nullptr, L"explore", path, nullptr, nullptr, SW_SHOWDEFAULT);
    }

    return true;
}
bool gui_message_item_c::b_break(RID btn, GUIPARAM)
{
    ASSERT(records.size());
    record &rec = records.get(0);

    ts::safe_ptr<rectengine_c> e = &HOLD(btn).engine();

    file_transfer_s *ft = g_app->find_file_transfer_by_msgutag(rec.utag);
    if (ft && ft->is_active())
        ft->kill();

    if (e) kill_button(e, 1);

    if (rec.text.get_char(0) == '?')
    {
        // unfinished
        rec.text.set_char(0, '*');

        if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.msgitem_utag == rec.utag; }))
        {

            post_s p;
            p.sender = row->other.sender;
            p.message_utf8 = rec.text;
            p.utag = rec.utag;
            prf().change_history_item(row->other.historian, p, HITM_MESSAGE);
            if (contact_c * h = contacts().find(row->other.historian)) h->iterate_history([&](post_s &p)->bool {
                if (p.utag == rec.utag)
                {
                    p.message_utf8 = rec.text;
                    return true;
                }
                return false;
            });

            if (row->deleted())
                prf().changed();
        }

        update_text();
    }

    return true;
}

bool gui_message_item_c::b_pause_unpause(RID btn, GUIPARAM p)
{
    if (!p)
    {
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,b_pause_unpause), btn.to_ptr() );
        return true;
    }

    ASSERT(records.size());
    record &rec = records.get(0);
    gui_button_c &b = HOLD(RID::from_ptr(p)).as<gui_button_c>();
    int r = (int)b.get_customdata();
    int r_to = r ^ 1;
    ASSERT(r == 2 || r == 3);

    if (r == 2)
        b.set_face_getter(BUTTON_FACE_PRELOADED(unpauseb));
    else
        b.set_face_getter(BUTTON_FACE_PRELOADED(pauseb));

    b.set_customdata((GUIPARAM)r_to);
    repl_button(r, r_to);

    file_transfer_s *ft = g_app->find_file_transfer_by_msgutag(rec.utag);
    if (ft && ft->is_active())
        ft->pause_by_me(r == 2);

    return true;
}

void gui_message_item_c::repl_button( int r_from, int r_to )
{
    ts::wstr_c f(CONSTWSTR("<rect=")); f.append_as_uint(r_from).append_char(',');
    int ri = textrect.get_text().find_pos(f);
    if (ri >= 0)
    {
        ts::wstr_c t; t.set_as_uint(r_to);
        ts::wstr_c tt = textrect.get_text();
        tt.replace(ri + 6, f.get_length() - 7, t);
        textrect.set_text_only(tt, false);
    }
    
    int cnt = records.size();
    for (int i = 1; i < cnt; ++i)
    {
        record &rec = records.get(i);
        if ((rec.utag & 0xFFFFFFFF) == r_from)
        {
            rec.utag = (rec.utag & 0xFFFFFFFF00000000ull) | r_to;
            break;
        }
    }
}

void gui_message_item_c::kill_button( rectengine_c *beng, int r )
{
    for (int i = records.size() - 1; i > 0; --i)
    {
        RID br = RID(records.get(i).utag >> 32);
        if (br == beng->getrid())
        {
            ASSERT( (records.get(i).utag & 0xFFFFFFFF) == r );
            records.remove_fast(i);
            break;
        }
    }

    // hint! remove rect tag from text, to avoid button creation on next redraw
    int ri = textrect.get_text().find_pos( ts::wstr_c(CONSTWSTR("<rect=")).append_as_uint(r).append_char(',') );
    if (ri >= 0)
    {
        int ri2 = textrect.get_text().find_pos(ri,'>');
        if (ri2 > ri)
        {
            ts::wstr_c cht( textrect.get_text() );
            cht.cut(ri,ri2-ri+1);
            textrect.set_text_only(cht, false);
        }
    }

    TSDEL(beng);
    flags.set(F_DIRTY_HEIGHT_CACHE);
}

void gui_message_item_c::updrect_emoticons(void *, int r, const ts::ivec2 &p)
{
    emoti().draw( getroot(), p, r );
}

void gui_message_item_c::updrect(void *, int r, const ts::ivec2 &p)
{
    int cnt = records.size();
    for(int i=1; i<cnt; ++i)
    {
        record &rec = records.get(i);
        if ( (rec.utag & 0xFFFFFFFF) == r )
        {
            RID rid(rec.utag >> 32); 
            MODIFY(rid).pos(root_to_local(p));
            return;
        }
    }

    RID b;
    switch (r)
    {
        case 0: //explore
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face_getter(BUTTON_FACE_PRELOADED(exploreb));
            bc.set_handler(DELEGATE(this, b_explore), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case 1: //break
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face_getter(BUTTON_FACE_PRELOADED(breakb));
            bc.set_handler(DELEGATE(this, b_break), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case 2: //pause
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_customdata((GUIPARAM)2);
            bc.set_face_getter(BUTTON_FACE_PRELOADED(pauseb));
            bc.set_handler(DELEGATE(this, b_pause_unpause), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case 3: //unpause
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_customdata((GUIPARAM)3);
            bc.set_face_getter(BUTTON_FACE_PRELOADED(unpauseb));
            bc.set_handler(DELEGATE(this, b_pause_unpause), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
    }
    if (b)
    {
        record &rec = records.add();
        rec.utag = r | (((uint64)b.index()) << 32);
    }
}

ts::wstr_c gui_message_item_c::prepare_button_rect(int r, const ts::ivec2 &sz)
{
    ts::wstr_c button(CONSTWSTR("<rect=")); button.append_as_uint(r).append_char(',');
    button.append_as_uint(sz.x).append_char(',').append_as_int(-sz.y).append(CONSTWSTR(",2>"));

    int cnt = records.size();
    for (int i = 1; i < cnt; ++i)
    {
        record &rec = records.get(i);
        if ((rec.utag & 0xFFFFFFFF) == r)
            rec.time = 0;
    }

    return button;
}

void gui_message_item_c::update_text()
{
    bool is_send = false;
    switch (subtype)
    {
    case ST_SEND_FILE:
        is_send = true;
    case ST_RECV_FILE:
        {
            int w = get_client_area().width();
            int oldh = get_height_by_width(w);

            set_selectable(false);
            ASSERT(records.size());
            record &rec = records.get(0);
            
            for (int i = 1, cnt = records.size(); i < cnt; ++i)
                records.get(i).time = 1;

            ts::wstr_c newtext(256,false);
            bool r_inserted = message_prefix(newtext, rec.time);
            ts::wstr_c btns;
            auto insert_button = [&]( int r, const ts::ivec2 &sz )
            {
                if (r_inserted)
                    newtext.insert(3, prepare_button_rect(r, sz));
                else
                    btns.insert(0, prepare_button_rect(r, sz));
            };

            ts::wstr_c fn = ts::fn_get_name_with_ext(from_utf8(rec.text));
            ts::wchar fc = rec.text.get_char(0);
            file_transfer_s *ft = nullptr;
            if (fc == '*')
            {
                newtext.append(CONSTWSTR("<img=file,-1>"));
                if (is_send)
                    newtext.append(TTT("Передача отменена: $",181) / fn);
                else
                    newtext.append(TTT("Прием отменен: $",182) / fn);
            } else if (fc == '?')
            {
                ft = g_app->find_file_transfer_by_msgutag(rec.utag);
                if (ft && ft->is_active()) goto transfer_alive;

                insert_button( 1, g_app->buttons().breakb->size );
                newtext.append(CONSTWSTR("<img=file,-1>"));
                newtext.append(TTT("Соединение отсутствует: $",235) / fn);
            }
            else
            {
                transfer_alive:
                if (!is_send)
                    insert_button( 0, g_app->buttons().exploreb->size );

                newtext.append(CONSTWSTR("<img=file,-1>"));

                if (nullptr == ft) ft = g_app->find_file_transfer_by_msgutag(rec.utag);
                if (ft && ft->is_active())
                {
                    insert_button( 1, g_app->buttons().breakb->size );

                    if (is_send)
                        newtext.append(TTT("Передача файла: $",183) / fn);
                    else
                        newtext.append(TTT("Прием файла: $",184) / fn);

                    int bps;
                    newtext.append(CONSTWSTR(" (<b>")).append_as_uint( ft->progress(bps) ).append(CONSTWSTR("</b>%, "));
                    if (bps >= 0)
                        insert_button( 2, g_app->buttons().pauseb->size );
                    if (bps < 0)
                    {
                        if (bps == -1)
                            insert_button(3,g_app->buttons().unpauseb->size);
                        if (bps == -3)
                            newtext.append(TTT("ожидание",185));
                        else
                            newtext.append(TTT("пауза",186));
                    } else if (bps < 1024)
                        newtext.append(TTT("$ байт в секунду",189) / ts::wmake(bps));
                    else if (bps < 1024 * 1024)
                        newtext.append(TTT("$ кб в секунду",190) / ts::wmake(bps/1024));
                    else
                        newtext.append(TTT("$ Мб в секунду",191) / ts::wmake(bps/(1024*1024)));

                    newtext.append_char(')');
                } else
                {
                    if (is_send)
                        newtext.append(TTT("Файл отправлен: $",193) / fn);
                    else
                        newtext.append(TTT("Файл: $",192) / fn);
                }
            }

            if (!btns.is_empty())
                newtext.append(CONSTWSTR("<p=c>")).append(btns);
            else
                message_postfix(newtext);

            textrect.set_text_only(newtext,false);

            for (int i = records.size()-1; i > 0; --i)
                if (records.get(i).time)
                {
                    TSDEL(&HOLD(RID(records.get(i).utag >> 32)).engine());
                    records.remove_fast(i);
                }


            flags.set(F_DIRTY_HEIGHT_CACHE);
            int newh = get_height_by_width(w);
            if (newh == oldh)
                getengine().redraw();
            else
            {
                getparent().call_children_repos();
            }
        }
        break;
    }
}

ts::wstr_c gui_message_item_c::hdr() const
{
    ts::str_c n(author->get_name());
    text_adapt_user_input(n);
    if (prf().is_loaded() && prf().get_msg_options().is(MSGOP_SHOW_PROTOCOL_NAME) && !historian->getkey().is_group())
    {
        if (protodesc.is_empty() && author->getkey().protoid)
        {
            if (active_protocol_c *ap = prf().ap(author->getkey().protoid))
                protodesc.set(CONSTASTR(" (")).append(ap->get_name()).append_char(')');
        }
        n.append(protodesc);
    }
    return from_utf8(n);
}

int gui_message_item_c::get_height_by_width(int w) const
{
    bool update_size_mode = false;
    if (w == -INT_MAX)
    {
        update_size_mode = true;
        w = getprops().size().x;
        goto update_size_mode;
    }
    if (w < 0) return 0;

    if (flags.is(F_DIRTY_HEIGHT_CACHE))
    {
        for(int i=0;i<ARRAY_SIZE(height_cache);++i)
            height_cache[i] = ts::ivec2(-1);
        next_cache_write_index = 0;
        const_cast<ts::flags32_s &>(flags).clear(F_DIRTY_HEIGHT_CACHE);

    } else
    {
        for(int i=0;i<ARRAY_SIZE(height_cache);++i)
            if ( height_cache[i].x == w ) return height_cache[i].y;
    }

update_size_mode:

    const theme_rect_s *thr = themerect();
    if (ASSERT(thr))
    {
        switch (subtype)
        {
        case gui_message_item_c::ST_RECV_FILE:
        case gui_message_item_c::ST_SEND_FILE:
        case gui_message_item_c::ST_JUST_TEXT:
            {
                ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
                int ww = sz.x;
                sz = textrect.calc_text_size(ww, custom_tag_parser_delegate());
                if (update_size_mode)
                    const_cast<ts::text_rect_c &>(textrect).set_size( ts::ivec2(ww, sz.y) );
                int h = thr->size_by_clientsize(ts::ivec2(ww, sz.y + addheight), false).y;
                height_cache[next_cache_write_index] = ts::ivec2(w, h);
                ++next_cache_write_index;
                if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;
                return h;
            }
            break;
        case gui_message_item_c::ST_CONVERSATION:
            {
                ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
                int ww = sz.x;

                int h = m_top;
                if ( !flags.is(F_NO_AUTHOR) )
                {
                    sz = gui->textsize(*g_app->font_conv_name, hdr(), ww);
                    h += sz.y;
                }
                sz = textrect.calc_text_size(ww - m_left, custom_tag_parser_delegate());
                if (update_size_mode)
                    const_cast<ts::text_rect_c &>(textrect).set_size(ts::ivec2(ww - m_left, sz.y));
                h = thr->size_by_clientsize(ts::ivec2(ww, sz.y + h + addheight), false).y;
                height_cache[next_cache_write_index] = ts::ivec2(w, h);
                ++next_cache_write_index;
                if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;
                return h;
            }
            break;
        }
    }

    height_cache[next_cache_write_index] = ts::ivec2(w, height);
    ++next_cache_write_index;
    if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;

    return height;
}

gui_messagelist_c::gui_messagelist_c(initial_rect_data_s &data) :gui_vscrollgroup_c(data) 
{
    memset(&last_post_time, 0, sizeof(last_post_time));
}
gui_messagelist_c::~gui_messagelist_c()
{
}

/*virtual*/ ts::ivec2 gui_messagelist_c::get_min_size() const
{
    return ts::ivec2(100);
}

/*virtual*/ void gui_messagelist_c::created()
{
    set_theme_rect(CONSTASTR("msglist"), false);
    defaultthrdraw = DTHRO_BASE;
    __super::created();
}

/*virtual*/ bool gui_messagelist_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (qp == SQ_DRAW && rid == getrid())
    {
        time_t rt = historian ? historian->get_readtime() : 0;
        readtime = rt;
        readtime_historian = historian;
        bool r = __super::sq_evt(qp, rid, data);
        if (readtime > rt && historian)
        {
            if (!g_app->is_inactive(false) && historian->gui_item && historian->gui_item->getprops().is_active())
            {
                historian->set_readtime(readtime);
                prf().dirtycontact(historian->getkey());
                g_app->need_recalc_unread(historian->getkey());
            }
        }
        readtime_historian = nullptr;

        if (historian && !historian->keep_history())
            g_app->buttons().nokeeph->draw( m_engine.get(), button_desc_s::NORMAL, ts::irect(10,10,100,100), button_desc_s::ALEFT | button_desc_s::ATOP );

        return r;
    }
    if (qp == SQ_CHECK_HINTZONE && rid == getrid())
    {
        data.hintzone.accepted = false;
        m_tooltip = GET_TOOLTIP();
        if (historian && !historian->keep_history())
        {
            if (ts::irect(10, g_app->buttons().nokeeph->size + 10).inside(data.hintzone.pos))
            {
                m_tooltip = TOOLTIP(TTT("История сообщений не сохраняется!", 231));
                data.hintzone.accepted = true;
            }
        }
        // don't return, parent should process message
    }
    return __super::sq_evt(qp, rid, data);
}

gui_message_item_c &gui_messagelist_c::get_message_item(message_type_app_e mt, contact_c *author, const ts::str_c &skin, time_t post_time, uint64 replace_post)
{
    ASSERT(MTA_FRIEND_REQUEST != mt);

    if ( prf().get_msg_options().is(MSGOP_SHOW_DATE_SEPARATOR) && post_time )
    {
        tm tmtm;
        _localtime64_s(&tmtm, &post_time);
        if (tmtm.tm_year != last_post_time.tm_year || tmtm.tm_mon != last_post_time.tm_mon || tmtm.tm_mday != last_post_time.tm_mday)
        {
            gui_message_item_c &sep = MAKE_CHILD<gui_message_item_c>(getrid(), nullptr, nullptr, ("date"), MTA_DATE_SEPARATOR);
            sep.init_date_separator(tmtm);
        }
        last_post_time = tmtm;
    }

    if (replace_post)
        for (rectengine_c *e : getengine())
            if (e)
            {
                gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>(&e->getrect());
                if (MTA_RECV_FILE == mt || MTA_SEND_FILE == mt )
                {
                    if (mi.with_utag(replace_post))
                        return mi;
                }
                if (mi.remove_utag(replace_post))
                    return mi;
            }


    if (is_special_mt(mt))
        return MAKE_CHILD<gui_message_item_c>(getrid(), historian, author, skin, mt);

    bool same_author = false;
    if (rectengine_c *e = getengine().get_last_child())
    {
        gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>( &e->getrect() );
        if (!is_special_mt(mi.get_mt()))
        {
            if (mi.get_author() == author && mi.themename().equals(CONSTASTR("message."), skin))
            {
                if (prf().get_msg_options().is(MSGOP_JOIN_MESSAGES))
                    return mi;
                same_author = true;
            }
            if (post_time < mi.get_last_post_time())
                author->reselect(true);
        }
    }

    gui_message_item_c &r = MAKE_CHILD<gui_message_item_c>(getrid(), historian, author, skin, mt);
    if (same_author) r.set_no_author();
    return r;
}


ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_MESSAGE> & p) // show message in message list
{
    if (historian != p.get_historian()) return 0;

    if (p.post.time == 0 && p.pass == 0)
    {
        // time will be initialized at 2nd pass
        p.post.time = 1; // indicate 2nd pass required
        return 0;
    }

    p.current = true;
    bool at_and = p.post.sender.is_self() || is_at_end();

    gmsg<ISOGM_SUMMON_POST>( p.post, p.sender ).send();

    if (at_and) scroll_to_end();

    children_repos();

    return 0;
}

void gui_messagelist_c::clear_list()
{
    getengine().trunc_children(0);
}

DWORD gui_messagelist_c::handler_SEV_ACTIVE_STATE(const system_event_param_s & p)
{
    if (p.active)
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            if (gui_messagelist_c *ml = gui->find_rect<gui_messagelist_c>(param))
                ml->getengine().redraw();
        DEFERRED_EXECUTION_BLOCK_END(this)
    }
    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> & p)
{
    if (historian == nullptr) return 0;

    contact_c *sender = nullptr;
    if (p.contact->is_rootcontact())
    {
        if (historian != p.contact) return 0;

        if (p.contact->get_state() == CS_ROTTEN)
        {
            clear_list();
            historian = nullptr;
            return 0;
        }

        historian->subiterate( [&](contact_c *c) {
            if (c->get_state() == CS_REJECTED)
            {
                sender = c;
            }
        } );

    } else if (historian->subpresent(p.contact->getkey()))
    {
        sender = p.contact;
    }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<GM_DROPFILES> & p)
{
    if (historian && !historian->getkey().is_self() && p.root == getrootrid())
    {
        historian->send_file(p.fn);
    }
    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_DELIVERED> & p)
{
    for(rectengine_c *e : getengine())
        if (e)
        {
            gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>( &e->getrect() );
            if ( mi.delivered(p.utag) )
            {
                mi.getengine().redraw();
                children_repos();
                break;
            }
        }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_SUMMON_POST> & p)
{
    contact_c *sender = contacts().find(p.post.sender);
    if (!sender) return 0;
    contact_c *receiver = contacts().find(p.post.receiver);
    if (!receiver) return 0;
    contact_c * h = p.historian ? p.historian : get_historian(sender, receiver);
    if (h != historian) return 0;

    switch (p.post.mt())
    {
        case MTA_FRIEND_REQUEST:
            gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_FRIEND_REQUEST_RECV, p.post.message_utf8).send();
            sender->friend_request();
            break;
        case MTA_INCOMING_CALL:
            gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_INCOMING_CALL, p.post.message_utf8).send();
            break;
        case MTA_CALL_ACCEPTED:
            gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_CALL_INPROGRESS).send();
            //break; // NO break here!
        default:
        {
            gui_message_item_c &mi = get_message_item(p.post.mt(), sender, calc_message_skin(p.post.mt(), p.post.sender), p.post.time, p.replace_post ? p.post.utag : 0);
            mi.append_text(p.post, false);
            if (p.unread && *p.unread == nullptr && p.post.time >= historian->get_readtime())
                *p.unread = &mi.getengine();
            if (p.replace_post && p.post.mt() != MTA_RECV_FILE && p.post.mt() != MTA_SEND_FILE)
                h->reselect(true);
        }
    }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_SELECT_CONTACT> & p)
{
    self_selected = p.contact->getkey().is_self();
    memset( &last_post_time, 0, sizeof(last_post_time) );

    clear_list();

    historian = p.contact;

    if (historian && !historian->keep_history())
        gui->register_hintzone(this);
    else
        gui->unregister_hintzone(this);

    gmsg<ISOGM_NOTICE>( historian, nullptr, NOTICE_NETWORK, ts::str_c() ).send(); // init notice list

    time_t before = now();
    if (historian->history_size()) before = historian->get_history(0).time;

    int min_hist_size = prf().min_history();
    int cur_hist_size = p.contact->history_size();

    int needload = min_hist_size - cur_hist_size;
    if (historian->get_readtime() < before)
    {
        // we have to load all read
        int unread = prf().calc_history_between(historian->getkey(), historian->get_readtime(), before);
        if (unread > needload)
            needload = unread;
    }

    if (needload > 0)
    {
        p.contact->load_history(needload);
        if (historian->history_size()) before = historian->get_history(0).time;
    }

    if (!historian->getkey().is_self())
    {
        int not_yet_loaded = prf().calc_history_before(historian->getkey(), before);
        int needload = ts::tmax(10, prf().min_history());
        if ( not_yet_loaded )
        {
            if ((not_yet_loaded - needload) < 10) needload = not_yet_loaded;
            gui_message_item_c &lhb = get_message_item(MTA_SPECIAL, historian, CONSTASTR("load"));
            lhb.init_load(ts::tmin(not_yet_loaded,needload));
        }
    }

    rectengine_c *first_unread = nullptr;
    p.contact->iterate_history([&](const post_s &p)
    {
        gmsg<ISOGM_SUMMON_POST>(p, &first_unread, historian).send();
        return false;
    });

    not_at_end();
    if (p.scrollend)
    {
        if (first_unread)
            scroll_to(first_unread);
        else
            scroll_to_end();
    }
    children_repos();

    return 0;
}

gui_message_editor_c::~gui_message_editor_c()
{
    if (gui)
        gui->delete_event( DELEGATE(this,clear_text) );
}

bool gui_message_editor_c::on_enter_press_func(RID, GUIPARAM param)
{
    bool fix_last_enter = false;
    if (param != nullptr)
    {
        static DWORD last_enter_pressed = 0;
        int behsend = prf().ctl_to_send();
        bool ctrl = GetKeyState(VK_CONTROL) < 0;
        if (behsend == 2)
        {
            DWORD tpressed = timeGetTime();
            if ((tpressed - last_enter_pressed) < 500)
            {
                ctrl = true;
                fix_last_enter = true;
            }
            behsend = 1;
        }
        if (behsend == 0) ctrl = !ctrl;
        if (!ctrl)
        {
            last_enter_pressed = timeGetTime();
            return false;
        }
    }

    if (fix_last_enter)
        backspace();

    contact_c *receiver = historian;
    if (receiver == nullptr) return true;
    if (receiver->is_meta())
        receiver = receiver->subget_for_send();
    if (receiver == nullptr) return true;
    gmsg<ISOGM_MESSAGE> msg(&contacts().get_self(), receiver, MTA_UNDELIVERED_MESSAGE );

    msg.post.message_utf8 = get_text_utf8();
    if (msg.post.message_utf8.is_empty()) return true;
    emoti().parse( msg.post.message_utf8, true );

    msg.send();

    return true;
}

bool gui_message_editor_c::clear_text(RID, GUIPARAM)
{
    set_text(ts::wstr_c());
    return true;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_SEND_MESSAGE> & p)
{
    return on_enter_press_func(RID(), nullptr) ? GMRBIT_ACCEPTED : 0;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_MESSAGE> & msg) // clear message editor
{
    if ( msg.pass == 0 && msg.sender->getkey().is_self() )
    {
        DEFERRED_CALL( 0, DELEGATE(this,clear_text), nullptr );
        messages.remove(historian->getkey());
        gui->set_focus(getrid());
        return GMRBIT_ACCEPTED;
    }

    return 0;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_SELECT_CONTACT> & p)
{
    bool added;
    auto &x = messages.add_get_item(historian ? historian->getkey() : contact_key_s(), added);
    x.value.text = get_text();
    x.value.cp = get_caret_char_index();
    historian = p.contact;
    ts::wstr_c t;
    int cp = -1;
    if (auto *val = messages.find(historian->getkey()))
    {
        t = val->value.text;
        cp = val->value.cp;
    }
    set_text(t);
    if (cp >= 0) set_caret_pos(cp);
    if (check_text_func) check_text_func(t);

    return 0;
}


/*virtual*/ void gui_message_editor_c::created()
{
    register_kbd_callback( DELEGATE( this, on_enter_press_func ), SSK_ENTER, false );
    register_kbd_callback( DELEGATE( this, on_enter_press_func ), SSK_ENTER, true );
    register_kbd_callback( DELEGATE( this, show_smile_selector ), SSK_S, true );
    set_multiline(true);
    set_theme_rect(CONSTASTR("entertext"), false);
    defaultthrdraw = DTHRO_BORDER;

    gui_button_c &smiles = MAKE_CHILD<gui_button_c>(getrid());
    smiles.set_face_getter(BUTTON_FACE_PRELOADED(smile));
    smiles.tooltip(TOOLTIP(TTT("Вставить смайлик (Ctrl+S)",268)));
    smiles.set_handler(DELEGATE(this, show_smile_selector), nullptr);
    rb = smiles.get_min_size();
    smile_pos_corrector = TSNEW(leech_dock_bottom_right_s, rb.x, rb.y, 2, 2);
    smiles.leech(smile_pos_corrector);
    MODIFY(smiles).visible(true);

    __super::created();
}

ts::ivec2 calc_smls_position(const ts::irect &buttonrect, const ts::ivec2 &menusize)
{
    ts::irect maxsize = ts::wnd_get_max_size(buttonrect);

    int x = (buttonrect.rb.x + buttonrect.lt.x - menusize.x)/2;
    
    if (x + menusize.x > maxsize.rb.x)
        x = maxsize.rb.x - menusize.x;

    if (x < maxsize.lt.x)
        x = maxsize.lt.x;

    int y = buttonrect.lt.y - menusize.y;
    if (y < maxsize.lt.y)
        y = buttonrect.rb.y;

    return ts::ivec2(x, y);
}

bool gui_message_editor_c::show_smile_selector(RID, GUIPARAM)
{
    ts::irect smilebuttonrect = getengine().get_child(0)->getrect().getprops().screenrect();

    drawcollector dcoll;
    RID r = MAKE_ROOT<dialog_smileselector_c>(dcoll, smilebuttonrect, getrid());
    ts::ivec2 size = HOLD(r)().get_min_size();
    ts::ivec2 pos = calc_smls_position( smilebuttonrect, size );

    MODIFY(r)
        .size(size)
        .pos(pos)
        .allow_move_resize(false, false)
        .show();

    return true;
}

/*virtual*/ void gui_message_editor_c::cb_scrollbar_width(int w)
{
    if ( smile_pos_corrector )
        smile_pos_corrector->x_space = 2 + w, smile_pos_corrector->update_ctl_pos();
}

gui_message_area_c::~gui_message_area_c()
{
    flags.clear( F_INITIALIZED );
}

/*virtual*/ ts::ivec2 gui_message_area_c::get_min_size() const
{
    ts::ivec2 sz = __super::get_min_size();
    int miny = 0;
    if ( guirect_c * r = send_button )
    {
        ts::ivec2 bsz = r->get_min_size();
        sz.x += bsz.x;
        if (miny < bsz.y) miny = bsz.y;
    }
    if (guirect_c * r = file_button)
    {
        ts::ivec2 bsz = r->get_min_size();
        sz.x += bsz.x;
        if (miny < bsz.y) miny = bsz.y;
    }
    if ( guirect_c * r = message_editor )
    {
        ts::ivec2 mesz = r->get_min_size();
        sz.x += mesz.x;
        if (miny < mesz.y) miny = mesz.y;
    }
    sz.y += miny;
    return sz;
}

bool gui_message_area_c::change_text_handler(const ts::wstr_c &t)
{
    send_button->disable( t.is_empty() );
    return true;
}

/*virtual*/ void gui_message_area_c::created()
{
    leech(TSNEW( leech_save_size_s, CONSTASTR("msg_area_size"), ts::ivec2(0,60)) );

    message_editor = MAKE_VISIBLE_CHILD<gui_message_editor_c>( getrid() );
    message_editor->check_text_func = DELEGATE(this, change_text_handler);
    send_button = MAKE_VISIBLE_CHILD<gui_button_c>( getrid() );
    send_button->set_face_getter(BUTTON_FACE(send));
    send_button->tooltip( TOOLTIP( TTT("Отправить",7) ) );
    send_button->set_handler( DELEGATE(g_app, b_send_message), nullptr );
    
    MODIFY(*send_button).visible(true).size(send_button->get_min_size());

    update_buttons();

    set_theme_rect(CONSTASTR("entertext"), false);
    defaultthrdraw = DTHRO_BASE;
    __super::created();

    flags.set( F_INITIALIZED );
}

bool gui_message_area_c::send_file(RID, GUIPARAM)
{
    ts::wstrings_c files;
    ts::wstr_c fromdir = prf().last_filedir();
    if (fromdir.is_empty())
        fromdir = ts::fn_get_path(ts::get_exe_full_name());
    ts::wstr_c title(TTT("Отправить файлы", 180));
    if (getroot()->load_filename_dialog(files, fromdir, CONSTWSTR(""), CONSTWSTR(""), nullptr, title))
    {
        if (files.size())
            prf().last_filedir(ts::fn_get_path(files.get(0)));

        if (contact_c *c = message_editor->get_historian())
            for (const ts::wstr_c &fn : files)
                c->send_file(fn);
    }

    return true;
}


void gui_message_area_c::update_buttons()
{
    if (!file_button)
    {
        file_button = MAKE_CHILD<gui_button_c>(getrid());
        file_button->set_face_getter(BUTTON_FACE_PRELOADED(fileb));
        file_button->set_handler(DELEGATE(this, send_file), nullptr);
    }

    int features = 0;
    int features_online = 0;
    bool now_disabled = false;
    if (contact_c * contact = message_editor->get_historian())
        contact->subiterate([&](contact_c *c) {
            if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
            {
                int f = ap->get_features();
                features |= f;
                if (c->get_state() == CS_ONLINE) features_online |= f;
                if (c->is_av()) now_disabled = true;
            }
        });

    MODIFY(*file_button).visible(true);
    if (0 == (features & PF_SEND_FILE))
    {
        file_button->disable();
        file_button->tooltip(TOOLTIP(TTT("Передача файлов не поддерживается", 188)));
    } else
    {
        file_button->enable();
        file_button->tooltip(TOOLTIP(TTT("Отправить файл", 187)));
    }

}

void gui_message_area_c::children_repos()
{
    if (!flags.is( F_INITIALIZED )) return;
    if (!send_button) return;
    if (!message_editor) return;
    if (!send_button) return;
    ts::irect clar = get_client_area();
    ts::ivec2 bsz = send_button->getprops().size();
    ts::ivec2 sfsz = file_button->get_min_size();
    MODIFY(*file_button).pos( clar.lt.x, clar.lt.y + (clar.height()-sfsz.y)/2 ).size(sfsz);
    MODIFY(*send_button).pos( clar.rb.x - bsz.x, clar.lt.y + (clar.height()-bsz.y)/2 );
    MODIFY(*message_editor).pos(clar.lt.x + sfsz.x, clar.lt.y).size( clar.width() - bsz.x - sfsz.x, clar.height() );
}


MAKE_CHILD<gui_conversation_c>::~MAKE_CHILD()
{
    get().allow_move_splitter(true);
    MODIFY(get()).visible(true);
}

gui_conversation_c::~gui_conversation_c()
{
    if (gui) gui->delete_event( DELEGATE( this, hide_show_messageeditor ) );
}


ts::ivec2 gui_conversation_c::get_min_size() const
{
    return ts::ivec2(300,200);
}
void gui_conversation_c::created()
{
    leech(TSNEW(leech_save_proportions_s, CONSTASTR("msg_splitter"), CONSTASTR("4255,0,30709,5035")));

    caption = MAKE_CHILD<gui_contact_item_c>(getrid(), &contacts().get_self()) << CIR_CONVERSATION_HEAD;
    noticelist = MAKE_CHILD<gui_noticelist_c>( getrid() );
    msglist = MAKE_VISIBLE_CHILD<gui_messagelist_c>( getrid() );
    messagearea = MAKE_CHILD<gui_message_area_c>( getrid() );
    message_editor = messagearea->message_editor->getrid();
    getrid().call_restore_signal();
    messagearea->getrid().call_restore_signal();
    return __super::created();
}

bool gui_conversation_c::hide_show_messageeditor(RID, GUIPARAM)
{
    bool show = flags.is(F_ALWAYS_SHOW_EDITOR);
    if (caption->contacted() && !caption->getcontact().getkey().is_self())
    {
        if ( caption->getcontact().getkey().is_group() )
            show = true;
        else
            caption->getcontact().subiterate([&](contact_c *c) {
                if (show) return;
                if (c->is_protohit(true))
                    show = true;
            });
    }
    MODIFY(*messagearea).visible(show);
    return false;
}

bool gui_conversation_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    return __super::sq_evt(qp,rid,data);
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &c)
{
    if (!caption->contacted()) return 0;

    if (c.contact->is_rootcontact() && c.contact == &caption->getcontact())
    {
        if (c.contact->get_state() == CS_ROTTEN)
            caption->resetcontact();

        if (c.contact->getkey().is_group())
        {
            caption->update_text();
            hide_show_messageeditor();
        }
        return 0;
    }

    ASSERT(caption->getcontact().is_rootcontact());
    if (caption->getcontact().subpresent( c.contact->getkey() ))
        caption->update_text();

    DEFERRED_EXECUTION_BLOCK_BEGIN(0)
        if (gui_contact_item_c *ci = gui->find_rect<gui_contact_item_c>(param))
            ci->update_buttons();
    DEFERRED_EXECUTION_BLOCK_END(caption.get())

    hide_show_messageeditor();

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_UPDATE_BUTTONS> &c)
{
    messagearea->update_buttons();
    return 0;
}


/*virtual*/ void gui_conversation_c::datahandler(const void *data, int size)
{
    if (contact_c *c = get_other())
    {
        c->subiterate([&](contact_c *sc) {
            if (sc->is_av())
                if (active_protocol_c *ap = prf().ap(sc->getkey().protoid))
                    ap->send_audio(sc->getkey().contactid, capturefmt, data, size);
        });
    }

}
/*virtual*/ s3::Format *gui_conversation_c::formats(int &count)
{
    avformats.clear();
    if (contact_c *c = get_other())
    {
        c->subiterate( [this](contact_c *sc) {
            if (sc->is_av())
                if (active_protocol_c *ap = prf().ap(sc->getkey().protoid)) avformats.set(ap->defaudio());
        } );

        //avformats.get(0).sampleRate = 44100;
        count = avformats.count();
        return avformats.begin();
    }
    

    count = 0;
    return nullptr;
}


ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_AV> &av)
{
    if (av.multicontact == get_other())
    {
        if (av.activated)
        {
            start_capture();
            caption->update_buttons(); // it updates some stuff
        }
        else
            stop_capture();
    }
    return 0;
}


ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_CALL_STOPED> &c)
{
    contact_c *owner = c.subcontact->getmeta();
    if (owner == &caption->getcontact())
        caption->update_buttons();
    return 0;
}

ts::uint32 gui_conversation_c::gm_handler( gmsg<ISOGM_SELECT_CONTACT> &c )
{
    caption->setcontact( c.contact );

    if (c.contact->is_av())
        start_capture();

    hide_show_messageeditor();

    if (c.contact->getkey().is_group())
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            gmsg<ISOGM_NOTICE>((contact_c *)param, (contact_c *)param, NOTICE_GROUP_CHAT).send();
        DEFERRED_EXECUTION_BLOCK_END(c.contact.get());
    }

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    if (ch.pass > 0 && caption->contacted() && caption->getcontact().getkey().is_self())
    {
        switch (ch.pp)
        {
            case PP_USERNAME:
            case PP_USERSTATUSMSG:
                if (0 != ch.protoid)
                    break;
            case PP_NETWORKNAME:
                caption->update_text();
                break;
        }
    }
    if (ch.pass == 0 && caption->contacted())
    {
        if (ch.pp == PP_MSGOPTIONS)
            caption->getcontact().reselect(true);
    }
    if (ch.pp == PP_EMOJISET && caption->contacted())
        caption->getcontact().reselect(true);

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_SAVED>&p)
{
    if (p.tabi == pt_active_protocol)
    {
        if (caption->contacted()) caption->getcontact().reselect(true);
        caption->update_text();
        caption->update_buttons();
        DEFERRED_UNIQUE_CALL( 0.3, DELEGATE( this, hide_show_messageeditor ), nullptr );
    }

    return 0;
}
