#include "isotoxin.h"

//-V:theme:807
//-V:prf:807
//-V:unmasked:807


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

    if (const theme_rect_s *thr = themerect())
    {
        ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
        if (sz.x < 0) sz.x = 0;
        if (sz.y < 0) sz.y = 0;
        int ww = sz.x;
        sz = textrect.calc_text_size(ww, custom_tag_parser_delegate());
        int h = sz.y + addheight + thr->clborder_y();
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

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_NEWVERSION>&nv)
{
    if (notice == NOTICE_NEWVERSION && nv.is_error())
    {
        g_app->download_progress = ts::ivec2(0);
        ts::wstr_c ot = textrect.get_text();
        int pp = ot.find_pos(CONSTWSTR("<null=px>"));
        if (pp >= 0)
        {
            ot.set_length(pp).append(loc_text(loc_connection_failed));
            textrect.set_text_only(ot, false);
            getengine().redraw();
        }
    }
    return 0;
}

static ts::wstr_c downloaded_info()
{
    ts::wstr_c info(CONSTWSTR("0"));

    if (g_app->download_progress.y > 0)
    {
        ts::wstr_c n;
        n.set_as_uint(g_app->download_progress.x);
        for (int ix = n.get_length() - 3; ix > 0; ix -= 3)
            n.insert(ix, '`');
        info = n;

        n.set_as_uint(g_app->download_progress.y);
        for (int ix = n.get_length() - 3; ix > 0; ix -= 3)
            n.insert(ix, '`');

        info.append(CONSTWSTR(" / "))
            .append(n)
            .append(CONSTWSTR(" ("))
            .append(ts::roundstr<ts::wstr_c>(100.0*double(g_app->download_progress.x) / g_app->download_progress.y, 2))
            .append(CONSTWSTR("%)"));
    }
    return info;
}

ts::uint32 gui_notice_c::gm_handler(gmsg<ISOGM_DOWNLOADPROGRESS>&p)
{
    if (notice == NOTICE_NEWVERSION)
    {
        g_app->download_progress = ts::ivec2(p.downloaded, p.total);

        ts::wstr_c ot = textrect.get_text();
        int pp = ot.find_pos(CONSTWSTR("<null=px>"));
        if (pp >= 0)
        {
            ot.set_length(pp + 9).append(downloaded_info());
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
            newtext.append(TTT("New version available: $",163) / from_utf8(itext_utf8));
            newtext.append(CONSTWSTR("<br>"));
            newtext.append(TTT("Current version: $",164) / ts::to_wstr(application_c::appver()));

            if ( auparams().lock_read()().in_progress )
            {
                newtext.append(CONSTWSTR("<br><b>"));
                newtext.append(TTT("-loading-",166));
                newtext.append(CONSTWSTR("</b><br><null=px>"));
                newtext.append(downloaded_info());

            } else if (auparams().lock_read()().downloaded)
            {
                g_app->download_progress = ts::ivec2(0);
                newtext.append(CONSTWSTR("<br>"));
                newtext.append(TTT("New version has been downloaded. Restart required.",167));

                gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());

                bool normal_update = ts::check_write_access(ts::fn_get_path(ts::get_exe_full_name()));

                if (normal_update)
                {
                    b_accept.set_text(TTT("Restart",168));
                    b_accept.set_handler(DELEGATE(g_app, b_restart), nullptr);
                } else
                {
                    b_accept.set_text( ts::wstr_c(CONSTWSTR("<img=uac>")).append( TTT("Install update",321) ));
                    b_accept.set_handler(DELEGATE(g_app, b_install), nullptr);
                }

                b_accept.set_face_getter(BUTTON_FACE(button));
                b_accept.leech(TSNEW(leech_dock_bottom_center_s, 150, 30, -5, 5, 0, 1));
                MODIFY(b_accept).visible(true);

                addheight = 40;

            } else
            {
                gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
                b_accept.set_text(TTT("Load",165));
                b_accept.set_face_getter(BUTTON_FACE(button));
                b_accept.set_handler(DELEGATE(g_app, b_update_ver), as_param(AUB_DOWNLOAD));
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
                newtext.append(TTT("Restore key confirm",198));
            } else
            {
                newtext.append(TTT("Unknown contact request",74));
                newtext.append(CONSTWSTR("<hr=7,2,1>"));

                ts::str_c n = itext_utf8;
                text_adapt_user_input(n);
                newtext.append(from_utf8(n));
            }

            textrect.set_text_only(newtext,false);

            gui_button_c &b_accept = MAKE_CHILD<gui_button_c>(getrid());
            b_accept.set_text(TTT("Accept",75));
            b_accept.set_face_getter(BUTTON_FACE(button));
            b_accept.set_handler(DELEGATE(sender, b_accept), this);
            b_accept.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 0, 2));
            MODIFY(b_accept).visible(true);

            gui_button_c &b_reject = MAKE_CHILD<gui_button_c>(getrid());
            b_reject.set_text(TTT("Reject",76));
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
            newtext.append(TTT("Download file [b]$[/b]?",175) / from_utf8(itext_utf8));
            newtext.append(CONSTWSTR("<hr=7,2,1>"));
            textrect.set_text_only(newtext,false);


            int minw = 0;
            gui_button_c &b_receiveas = MAKE_CHILD<gui_button_c>(getrid());
            b_receiveas.set_face_getter(BUTTON_FACE(button));
            b_receiveas.set_text(TTT("Save as...",177), minw); minw += 6; if (minw < 100) minw = 100;
            b_receiveas.set_handler(DELEGATE(sender, b_receive_file_as), this);
            b_receiveas.leech(TSNEW(leech_dock_bottom_center_s, minw, 30, -5, 5, 1, 3));
            MODIFY(b_receiveas).visible(true);

            gui_button_c &b_receive = MAKE_CHILD<gui_button_c>(getrid());
            b_receive.set_face_getter(BUTTON_FACE(button));
            b_receive.set_text(TTT("Save",176) );
            b_receive.set_handler(DELEGATE(sender, b_receive_file), this);
            b_receive.leech(TSNEW(leech_dock_bottom_center_s, minw, 30, -5, 5, 0, 3));
            MODIFY(b_receive).visible(true);

            gui_button_c &b_refuse = MAKE_CHILD<gui_button_c>(getrid());
            b_refuse.set_face_getter(BUTTON_FACE(button));
            b_refuse.set_text(TTT("No",178));
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
            b_resend.set_text(TTT("Again",81));
            b_resend.set_face_getter(BUTTON_FACE(button));
            b_resend.set_handler(DELEGATE(sender, b_resend), this);
            b_resend.leech(TSNEW(leech_dock_bottom_center_s, 100, 30, -5, 5, 0, 2));
            MODIFY(b_resend).visible(true);

            gui_button_c &b_kill = MAKE_CHILD<gui_button_c>(getrid());
            b_kill.set_text(TTT("Delete",82));
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
            b_accept.set_handler(DELEGATE(sender, b_accept_call), nullptr);
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

            int wwx = -minsz.x - 5;

            gui_button_c &b_mute_mic = MAKE_CHILD<gui_button_c>(getrid());

            (sender->getmeta() && sender->getmeta()->get_options().unmasked().is(contact_c::F_MIC_OFF)) ?
                b_mute_mic.set_face_getter(BUTTON_FACE(unmute_mic)) :
                b_mute_mic.set_face_getter(BUTTON_FACE(mute_mic));

            b_mute_mic.set_handler(DELEGATE(sender, b_mute_mic), &b_mute_mic);
            int rxx = minsz.y;
            minsz = b_mute_mic.get_min_size();
            rxx = (rxx - minsz.y)/2 + 5;
            b_mute_mic.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, wwx, rxx, 0, 2));
            MODIFY(b_mute_mic).visible(true);

            gui_button_c &b_mute_speaker = MAKE_CHILD<gui_button_c>(getrid());

            (sender->getmeta() && sender->getmeta()->get_options().unmasked().is(contact_c::F_SPEAKER_OFF)) ?
                b_mute_speaker.set_face_getter(BUTTON_FACE(unmute_speaker)) :
                b_mute_speaker.set_face_getter(BUTTON_FACE(mute_speaker));

            b_mute_speaker.set_handler(DELEGATE(sender, b_mute_speaker), &b_mute_speaker);
            minsz = b_mute_speaker.get_min_size();
            b_mute_speaker.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, wwx, rxx, 1, 2));
            MODIFY(b_mute_speaker).visible(true);

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

                ts::str_c n( m->get_name() );
                text_adapt_user_input(n);
                txt.append( from_utf8(n) ).append(CONSTWSTR(", "));
            } );

            getengine().trunc_children(0);

            if (txt.ends( CONSTWSTR(", ") ))
                txt.trunc_length(2);
            else
                txt.append(TTT("Nobody in group chat (except you)", 257));
            textrect.set_text_only(txt, false);

            if (sender->get_options().unmasked().is(contact_c::F_AUDIO_GCHAT))
            {
                gui_button_c &b_mute_mic = MAKE_CHILD<gui_button_c>(getrid());

                (sender->get_options().unmasked().is(contact_c::F_MIC_OFF)) ?
                    b_mute_mic.set_face_getter(BUTTON_FACE(unmute_mic)) :
                    b_mute_mic.set_face_getter(BUTTON_FACE(mute_mic));

                b_mute_mic.set_handler(DELEGATE(sender, b_mute_mic), &b_mute_mic);
                ts::ivec2 minsz = b_mute_mic.get_min_size();
                b_mute_mic.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 0, 2));
                MODIFY(b_mute_mic).visible(true);

                gui_button_c &b_mute_speaker = MAKE_CHILD<gui_button_c>(getrid());

                (sender->get_options().unmasked().is(contact_c::F_SPEAKER_OFF)) ?
                    b_mute_speaker.set_face_getter(BUTTON_FACE(unmute_speaker)) :
                    b_mute_speaker.set_face_getter(BUTTON_FACE(mute_speaker));

                b_mute_speaker.set_handler(DELEGATE(sender, b_mute_speaker), &b_mute_speaker);
                minsz = b_mute_speaker.get_min_size();
                b_mute_speaker.leech(TSNEW(leech_dock_bottom_center_s, minsz.x, minsz.y, -5, 5, 1, 2));
                MODIFY(b_mute_speaker).visible(true);

                addheight = 50;
            }

        }
        break;
    }

    setup_tail();
}

bool gui_notice_c::setup_tail(RID, GUIPARAM)
{
    flags.set(F_DIRTY_HEIGHT_CACHE);

    int h = get_height_by_width(getprops().size().x);
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
                newtext.append(TTT("Your request was rejected. You can send request again.",80));
            else if (sender->get_state() == CS_INVITE_SEND)
                newtext.append(TTT("Request sent. You can repeat request.",89));
            hr = false;

            break;
        case NOTICE_INCOMING_CALL:
            newtext.append(TTT("Incoming call from $",134) / from_utf8(aname));
            break;
        case NOTICE_CALL_INPROGRESS:
            newtext.append(TTT("Call with $",136) / from_utf8(aname));
            break;
        case NOTICE_CALL:
            newtext.append(TTT("Call to $",142) / from_utf8(aname));
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
    getengine().trunc_children(0); // just kill all buttons

    pubid = pubid_;
    ts::wstr_c sost, plugdesc, uname, ustatus, netname;
    cmd_result_e curstate = CR_OK;
    bool is_autoconnect = false;
    prf().iterate_aps([&](const active_protocol_c &ap) {

        if (contact_c *c = contacts().find_subself(ap.getid()))
            if ((networkid == 0 && c->get_pubid() == pubid) || (networkid == ap.getid()))
            {
                curstate = ap.get_current_state();
                networkid = ap.getid();
                plugdesc = from_utf8(ap.get_desc());
                netname = from_utf8(ap.get_name());
                is_autoconnect = ap.is_autoconnect();

                if (CR_OK == curstate)
                {
                    if (c->get_state() == CS_ONLINE)
                        sost.set(CONSTWSTR("</l><b>"))
                        .append(maketag_color<ts::wchar>(get_default_text_color(COLOR_ONLINE_STATUS))).append(TTT("Online", 100)).append(CONSTWSTR("</color></b><l>"));

                    if (c->get_state() == CS_OFFLINE)
                        sost.set(maketag_color<ts::wchar>(get_default_text_color(COLOR_OFFLINE_STATUS))).append(TTT("Offline", 101)).append(CONSTWSTR("</color>"));
                } else
                {
                    ts::wstr_c errs;
                    switch (curstate)
                    {
                        case CR_NETWORK_ERROR:
                            errs = TTT("Network error",335);
                            break;
                        case CR_CORRUPT:
                            errs = TTT("Data corrupt",336);
                            break;
                        default:
                            errs = TTT("Error $",334)/ts::wmake<uint>( curstate );
                            break;
                    }
                    sost.set(CONSTWSTR("</l><b>"))
                        .append(maketag_color<ts::wchar>(get_default_text_color(COLOR_ERROR_STATUS))).append(errs).append(CONSTWSTR("</color></b><l>"));
                }

                int online = 0, all = 0;
                contacts().iterate_proto_contacts( [&]( contact_c *c ) {
                
                    if (c->getkey().protoid == ap.getid())
                    {
                        ++all;
                        if (c->get_state() == CS_ONLINE) ++online;
                    }
                    return true;
                } );
                sost.append(CONSTWSTR(" [")).append_as_int(online).append_char('/').append_as_int(all).append_char(']');

                ts::str_c n = ap.get_uname();
                text_convert_from_bbcode(n);
                text_remove_tags(n);
                uname = from_utf8(n);

                ustatus = from_utf8(ap.get_ustatusmsg());
            }
    });

    if (uname.is_empty())
    {
        ts::str_c n = prf().username();
        text_convert_from_bbcode(n);
        text_remove_tags(n);
        uname.set(maketag_color<ts::wchar>(get_default_text_color(COLOR_DEFAULT_VALUE))).append(from_utf8(n)).append(CONSTWSTR("</color>"));
    } else
        uname.insert(0,CONSTWSTR("</l><b>")).append(CONSTWSTR("</b><l>"));
    if (ustatus.is_empty())
        ustatus.set(maketag_color<ts::wchar>(get_default_text_color(COLOR_DEFAULT_VALUE))).append( from_utf8(prf().userstatus()) ).append(CONSTWSTR("</color>"));
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
            int networkid = as_int(p);
            if (active_protocol_c *ap = prf().ap(networkid))
                ap->set_autoconnect(false);
            offline_online(HOLD(b).as<gui_button_c>(), networkid, false);

            return true;
        }
        static bool make_online(RID b, GUIPARAM p)
        {
            int networkid = as_int(p);
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
                b.tooltip(TOOLTIP(TTT("Disconnect",98)));
                b.set_handler(make_offline, as_param(networkid));
            }
            else
            {
                b.set_face_getter(BUTTON_FACE(to_online));
                b.tooltip(TOOLTIP(TTT("Connect",99)));
                b.set_handler(make_online, as_param(networkid));
            }
        }
        static bool netsetup(RID b, GUIPARAM p)
        {
            int networkid = as_int(p);
            SUMMON_DIALOG<dialog_setup_network_c>( UD_PROTOSETUP, dialog_protosetup_params_s(networkid) );
            return true;
        }
    };

    gui_button_c &b_connect = MAKE_CHILD<gui_button_c>(getrid());
    connect_handler::offline_online(b_connect, networkid, is_autoconnect);
    b_connect.leech(TSNEW(leech_dock_right_center_s, sz2.x, sz2.y, 5, -5, 0, 1));
    MODIFY(b_connect).visible(true);
    if (curstate != CR_OK)
        b_connect.disable();

    gui_button_c &b_setup = MAKE_CHILD<gui_button_c>(getrid());
    b_setup.set_constant_size(sz1);
    b_setup.set_face_getter(BUTTON_FACE(netsetup));
    b_setup.tooltip(TOOLTIP(TTT("Setup network connection",55)));
    b_setup.leech(TSNEW(leech_at_left_s, &b_connect, 5));
    b_setup.set_handler(connect_handler::netsetup, as_param(networkid));
    MODIFY(b_setup).visible(true);
    if (curstate != CR_OK)
        b_setup.disable();

    gui_button_c &b_copy = MAKE_CHILD<gui_button_c>(getrid());
    b_copy.set_constant_size(sz0);
    b_copy.set_customdata_obj<copydata>(pubid, this);
    b_copy.set_face_getter(BUTTON_FACE(copyid));
    b_copy.tooltip(TOOLTIP(TTT("Copy ID to clipboard",97)));

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

ts::uint32 gui_notice_network_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT>&c)
{
    refresh |= c.contact->getkey().protoid == networkid;
    return 0;
}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<ISOGM_CMD_RESULT> &rslt)
{
    if (rslt.cmd == AQ_SET_CONFIG && rslt.networkid == networkid)
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, resetup), nullptr);

    return 0;
}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<GM_HEARTBEAT>&)
{
    // just check protocol available
    if (nullptr == prf().ap(networkid))
        TSDEL(this);
    else if (refresh)
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, resetup), nullptr);

    return 0;
}

ts::uint32 gui_notice_network_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass == 0)
    {
        if (PP_AVATAR == ch.sp)
            getengine().redraw();
        else if (CFG_LANGUAGE == ch.sp)
            refresh = true;
    }
    if (ch.protoid == networkid && ch.pass > 0)
    {
        if (ch.sp == PP_NETWORKNAME || ch.sp == PP_USERNAME || ch.sp == PP_USERSTATUSMSG)
            refresh = true;
    }
    return 0;
}


/*virtual*/ void gui_notice_network_c::created()
{
    DMSG( "gui_notice_network_c: " << getrid() );
    __super::created();
}

/*virtual*/ int gui_notice_network_c::get_height_by_width(int width) const
{
    if (width == -INT_MAX) return __super::get_height_by_width(width);
    return __super::get_height_by_width( width - left_margin );
}

void gui_notice_network_c::moveup(const ts::str_c&)
{
    if (active_protocol_c *me = prf().ap(networkid))
    {
        active_protocol_c *other = nullptr;
        int my_sf = me->sort_factor();
        int other_sf = 0;
        prf().iterate_aps( [&](active_protocol_c &ap) {
            
            int sf = ap.sort_factor();
            if (sf < my_sf)
            {
                if (other_sf < sf)
                {
                    other_sf = sf;
                    other = &ap;
                }
            }
        } );
        if (other)
        {
            other->set_sortfactor( my_sf );
            me->set_sortfactor( other_sf );
        }
    }
}

void gui_notice_network_c::movedn(const ts::str_c&)
{
    if (active_protocol_c *me = prf().ap(networkid))
    {
        active_protocol_c *other = nullptr;
        int my_sf = me->sort_factor();
        int other_sf = 0;
        prf().iterate_aps([&](active_protocol_c &ap) {

            int sf = ap.sort_factor();
            if (sf > my_sf)
            {
                if (other_sf == 0 || other_sf > sf)
                {
                    other_sf = sf;
                    other = &ap;
                }
            }
        });
        if (other)
        {
            other->set_sortfactor(my_sf);
            me->set_sortfactor(other_sf);
        }
    }
}

int gui_notice_network_c::sortfactor() const
{
    if (active_protocol_c *me = prf().ap(networkid))
        return me->sort_factor();
    return 0;
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
                m_engine->draw(ts::irect(ca.lt.x, ca.lt.y+y, ca.lt.x + left_margin, ca.lt.y+y+left_margin), get_default_text_color(COLOR_OVERAVATAR));
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
            flags.clear(F_RBDN);
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
                    gmsg<ISOGM_CHANGED_SETTINGS>(protoid, PP_AVATAR).send();
                }
            };

            if (contact_c *contact = contacts().find_subself(networkid))
            {

                if (contact->get_avatar())
                {
                    menu_c m;
                    m.add(TTT("Change avatar",219), 0, x::set_self_avatar, ts::amake<int>(networkid));
                    m.add(TTT("Remove avatar",220), 0, x::clear_self_avatar, ts::amake<int>(networkid));
                    popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), m);
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
    case SQ_MOUSE_RDOWN:
        flags.set(F_RBDN);
        break;
    case SQ_MOUSE_RUP:
        if (flags.is(F_RBDN))
        {
            int sortfactor = 0, minsortfactor = 0, maxsortfactor = 0;
            prf().iterate_aps([&](const active_protocol_c &ap) {
                
                int sf = ap.sort_factor();
                if (ap.getid() == networkid)
                    sortfactor = sf;
                else if (minsortfactor == 0 || minsortfactor > sf)
                    minsortfactor = sf;
                else if (maxsortfactor == 0 || maxsortfactor < sf)
                    maxsortfactor = sf;
            
            });

            bool allow_move_up = minsortfactor < sortfactor;
            bool allow_move_down = maxsortfactor > sortfactor;

            if (allow_move_up || allow_move_down)
            {
                menu_c m;
                if (allow_move_up) m.add(TTT("Move up",77), 0, DELEGATE(this, moveup));
                if (allow_move_down) m.add(TTT("Move down",78), 0, DELEGATE(this, movedn));
                gui_popup_menu_c::show(menu_anchor_s(true), m);
            }

            flags.clear(F_RBDN);
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
        text.replace(i0, i1 - i0, maketag_color<ts::wchar>(get_default_text_color(COLOR_BLINKED)));
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
    if (gui)
        gui->delete_event( DELEGATE(this, resort_children) );
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

        int parh = HOLD(getparent())().getprops().size().y / 2;
        if (parh != 0)
            if (sz.y > parh)
                sz.y = parh;

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

/*virtual*/ void gui_noticelist_c::children_repos()
{
    bool sb = is_sb_visible();
    __super::children_repos();
    if (!sb && is_sb_visible())
        scroll_to_begin();
}

bool gui_noticelist_c::resort_children(RID, GUIPARAM)
{
    auto swap_them = [](rectengine_c *e1, rectengine_c *e2)->bool
    {
        if (e1 == nullptr || e2 == nullptr) return false;
        gui_notice_network_c * n1 = dynamic_cast<gui_notice_network_c *>(&e1->getrect());
        if (n1 == nullptr) return false;
        gui_notice_network_c * n2 = dynamic_cast<gui_notice_network_c *>(&e2->getrect());
        if (n2 == nullptr) return false;

        int sf1 = n1->sortfactor();
        int sf2 = n2->sortfactor();

        return sf1 > sf2;
    };

    if (getengine().children_sort((rectengine_c::SWAP_TESTER)swap_them))
        gui->repos_children(this);
    return true;
}

/*virtual*/ bool gui_noticelist_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (SQ_CHILD_CREATED == qp && rid == getrid())
    {
        if (gui_notice_network_c *nn = dynamic_cast<gui_notice_network_c *>(&HOLD(data.rect.id)()))
            DEFERRED_UNIQUE_CALL( 0, DELEGATE(this, resort_children), 0 );
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

ts::uint32 gui_noticelist_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS> &ch)
{
    if (ch.pass == 0 && ch.sp == PP_ACTIVEPROTO_SORT)
        resort_children(RID(),nullptr);
    return 0;
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

ts::uint32 gui_noticelist_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &p)
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
            gui->repos_children(this);
        }

    }

    return 0;
}

void gui_noticelist_c::refresh()
{
    gui->repos_children(this);
    bool vis = false;
    for (rectengine_c *e : getengine())
        if (e) 
        {
            vis = true;
            break;
        }

    MODIFY(*this).visible(vis);
    gui->repos_children(&HOLD(getparent()).as<gui_group_c>());
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
                gui_notice_c &nn = create_notice(NOTICE_NEWVERSION);
                nn.setup( cfg().autoupdate_newver() );
            }

            ts::tmp_tbuf_t<ts::ivec2> splist;

            for (auto &row : prf().get_table_active_protocol())
            {
                ts::ivec2 &ap = splist.add();
                ap.x = row.id;
                ap.y = row.other.sort_factor;
            }

            splist.tsort<ts::ivec2>( []( const ts::ivec2 *s1, const ts::ivec2 *s2 ) { return s1->y < s2->y; } );

            for(const ts::ivec2 &s : splist)
            {
                //if (0 != (row.other.options & active_protocol_data_s::O_SUSPENDED))
                //    continue;

                ts::str_c pubid = contacts().find_pubid(s.x);
                if (!pubid.is_empty())
                {
                    not_at_end();
                    MODIFY(*this).visible(true);
                    gui_notice_network_c &nn = MAKE_CHILD<gui_notice_network_c>(getrid());
                    nn.setup(pubid);
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

        if (get_mt() == MTA_TYPING) // just optimization
        {
            gui->delete_event(DELEGATE(this, kill_self));
            gui->delete_event(DELEGATE(this, animate_typing));
        }
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

void gui_message_item_c::mark_found()
{
    if (flags.is(F_FOUND_ITEM) && g_app->found_items && g_app->found_items->fsplit.size())
    {
        ts::tmp_tbuf_t<ts::ivec2> marki;
        ts::wstr_c t = textrect.get_text();
        text_find_inds(t, marki, g_app->found_items->fsplit);
        for(const ts::ivec2 &m : marki)
            t.insert(m.r1,CONSTWSTR("<cstm=y>")).insert(m.r0,CONSTWSTR("<cstm=x>"));
        textrect.set_text_only(t, true);
    }
}

void gui_message_item_c::rebuild_text()
{
    ts::wstr_c newtext;

#if JOIN_MESSAGES
    if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
    {
        ts::wstr_c pret, postt;
        prepare_str_prefix(pret, postt);

        rec.append(newtext, pret, postt);
        for (record &r : other_records)
            r.append(newtext, pret, postt);

        timestr.clear();

    } else 
#endif
        timestrwidth = zero_rec.append(newtext, timestr);

    textrect.set_text_only(newtext, false);
    mark_found();
}

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
    ts::set_clipboard_text(from_utf8(msg));
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
        if (zero_rec.utag == 0 || subtype == ST_RECV_FILE || subtype == ST_SEND_FILE)
        {
            if (!flags.is(F_NO_AUTHOR))
                if (rectengine_c *next = HOLD(parent).engine().get_next_child( &getengine() ))
                    if (gui_message_item_c *nitm = ts::ptr_cast<gui_message_item_c *>( &next->getrect() ))
                        if (nitm->get_author() == get_author())
                            nitm->set_no_author(false), next->redraw();

            TSDEL(this);
        }
        //gui->repos_children(&HOLD(parent).as<gui_group_c>());
    }
}

bool gui_message_item_c::try_select_link(RID, GUIPARAM p)
{
    ts::ivec2 *pt = gui->temp_restore<ts::ivec2>(as_int(p));
    if (textrect.is_invalid_glyphs())
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

static gui_messagelist_c *current_draw_list = nullptr;

/*virtual*/ bool gui_message_item_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (rid != getrid())
    {
        if (SQ_MOUSE_WHEELDOWN == qp || SQ_MOUSE_WHEELUP == qp)
            return __super::sq_evt(qp, getrid(), data);

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
            switch (subtype)
            {
            case gui_message_item_c::ST_RECV_FILE:
            case gui_message_item_c::ST_SEND_FILE:
            case gui_message_item_c::ST_TYPING:
                gui_control_c::sq_evt(qp, rid, data);
                if (m_engine)
                {
                    ts::irect ca = get_client_area();
                    draw_data_s &dd = getengine().begin_draw();

                    text_draw_params_s tdp;
                    ts::ivec2 sz(0), oo(dd.offset);

                    dd.offset += ca.lt;
                    dd.size = ca.size();

                    if (!flags.is(F_NO_AUTHOR))
                    {
                        tdp.font = g_app->font_conv_name;
                        ts::TSCOLOR c = get_default_text_color(0);
                        tdp.forecolor = &c; //-V506
                        tdp.sz = &sz;
                        m_engine->draw(hdr(), tdp);
                    }

                    sz.x = 0;
                    sz.y += m_top;
                    dd.offset += sz;
                    dd.size -= sz;
                    tdp.font = g_app->font_conv_text;
                    ts::TSCOLOR c = get_default_text_color(0);
                    tdp.forecolor = &c;
                    tdp.sz = nullptr;
                    ts::flags32_s f; f.set(ts::TO_LASTLINEADDH);
                    tdp.textoptions = &f;
                    tdp.rectupdate = DELEGATE(this, updrect);

                    __super::draw(dd, tdp);


                    /*

                    if (flags.is(F_OVERIMAGE) && imgloader())
                    {
                        image_loader_c &ldr = imgloader_get();
                        if (const picture_c *p = ldr.get_picture())
                        {
                            ts::irect r( ldr.local_p, ldr.local_p + p->framesize() + ts::ivec2(8) );
                            m_engine->draw(r, get_default_text_color(3));
                        }
                    }

                    dd.offset += ca.lt;
                    dd.size = ca.size();

                    text_draw_params_s tdp;
                    tdp.rectupdate = DELEGATE(this, updrect);
                    ts::TSCOLOR c = get_default_text_color(0);
                    tdp.forecolor = &c;

                    draw(dd, tdp);
                    */

                    if (
#if JOIN_MESSAGES
                        !prf().get_options().is(MSGOP_JOIN_MESSAGES) &&
#endif
                        gui_message_item_c::ST_TYPING != subtype)
                    {
                        dd.size.x = timestrwidth + 1;
                        dd.size.y = g_app->font_conv_time->height();
                        dd.offset = oo + ca.rb - dd.size;

                        tdp.font = g_app->font_conv_time;
                        ts::TSCOLOR cc = get_default_text_color(2);
                        tdp.forecolor = &cc; //-V506
                        m_engine->draw(timestr, tdp);
                    }


                    getengine().end_draw();
                }
                break;
            case gui_message_item_c::ST_JUST_TEXT:
                __super::sq_evt(qp, rid, data);

                if (
#if JOIN_MESSAGES
                    !prf().get_options().is(MSGOP_JOIN_MESSAGES) && 
#endif
                    m_engine)
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
                    if (historian && current_draw_list)
                    {
                        current_draw_list->update_last_seen_message_time(zero_rec.time);
#if JOIN_MESSAGES
                        for (const record &r : other_records)
                            current_draw_list->update_last_seen_message_time(r.time);
#endif
                    }

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
                            tdp.forecolor = &c; //-V506
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

#if JOIN_MESSAGES
                        if (!prf().get_options().is(MSGOP_JOIN_MESSAGES))
#endif
                        {
                            dd.size.x = timestrwidth + 1;
                            dd.size.y = g_app->font_conv_time->height();
                            dd.offset = oo + ca.rb - dd.size;
                                
                            tdp.font = g_app->font_conv_time;
                            ts::TSCOLOR c = get_default_text_color(2);
                            tdp.forecolor = &c; //-V506
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
        } else if (flags.is(F_OVERIMAGE_LBDN) && imgloader())
        {
            flags.clear(F_OVERIMAGE|F_OVERIMAGE_LBDN);
            getengine().redraw();

            image_loader_c &ldr = imgloader_get();
            if (ldr.get_picture())
                ts::open_link(ldr.get_fn());
        }
        break;
    case SQ_MOUSE_RUP:
        if (zero_rec.text.is_empty() || subtype != ST_CONVERSATION)
        {
            if (zero_rec.utag == 0)
                break;
            if (ST_SEND_FILE == subtype || ST_RECV_FILE == subtype)
                if (g_app->find_file_transfer_by_msgutag(zero_rec.utag))
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
                mnu.add(TTT("Open link",202), 0, DELEGATE(this, ctx_menu_golink), lnk);
                mnu.add(TTT("Copy link",203), 0, DELEGATE(this, ctx_menu_copylink), lnk);

                popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), mnu);
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
                try_select_link(RID(), as_param(gui->temp_store<ts::ivec2>( to_local(data.mouse.screenpos) )));
            }

            return true;
        }
        if (!gui->mtrack(getrid(), MTT_SBMOVE) && !gui->mtrack(getrid(), MTT_TEXTSELECT))
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
                mutag = zero_rec.utag;
            } else
            {
                bool some_selection = some_selected();

                mnu.add(gui->app_loclabel(LL_CTXMENU_COPY), (!some_selection) ? MIF_DISABLED : 0, DELEGATE(this, ctx_menu_copy));
                ts::str_c msg = to_utf8(get_message_under_cursor(to_local(data.mouse.screenpos), mutag));
                gui->app_prepare_text_for_copy(msg);
                mnu.add(TTT("Copy message",205), 0, DELEGATE(this, ctx_menu_copymessage), msg);
                mnu.add_separator();
            }
            mnu.add(TTT("Delete message",221), 0, DELEGATE(this, ctx_menu_delmessage), ts::str_c().append_as_hex(&mutag, sizeof(mutag)));

            popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), mnu);

        }
        break;
    case SQ_MOUSE_RDOWN:
    case SQ_MOUSE_LDOWN:
        if (!popupmenu && !some_selected() && check_overlink(to_local(data.mouse.screenpos)))
            return true;

        if (SQ_MOUSE_LDOWN == qp)
        {
            if (flags.is(F_OVERIMAGE) && imgloader())
                flags.set(F_OVERIMAGE_LBDN);
        }

        // no break here!
    case SQ_MOUSE_OUT:
        if (SQ_MOUSE_LDOWN != qp)
        {
            bool prev = flags.is(F_OVERIMAGE);
            flags.clear(F_OVERIMAGE|F_OVERIMAGE_LBDN);
            if (prev) getengine().redraw();
        }
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
        if (popupmenu.expired())
        {
            if (textrect.get_text().find_pos(CONSTWSTR("<cstm=a")) >= 0 && !gui->mtrack(getrid(), MTT_TEXTSELECT) && !some_selected())
            {
                if (check_overlink(data.detectarea.pos))
                {
                    data.detectarea.area = AREA_HAND;
                    return true;
                }
            }
            if (imgloader())
            {
                image_loader_c &ldr = imgloader_get();
                if (const picture_c *p = ldr.get_picture())
                {
                    ts::irect r( ldr.local_p, ldr.local_p + p->framerect().size() + ts::ivec2(8) );
                    bool prev = flags.is(F_OVERIMAGE);
                    if (prev != flags.init(F_OVERIMAGE, r.inside(data.detectarea.pos)))
                        getengine().redraw();

                    if (flags.is(F_OVERIMAGE))
                    {
                        data.detectarea.area = AREA_HAND;
                        return true;
                    } else
                        flags.clear(F_OVERIMAGE_LBDN);
                }
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
        if ((ST_SEND_FILE == subtype || ST_RECV_FILE == subtype) && data.changed.width_changed)
            update_text();
        calc_height_by_width( -INT_MAX );
        } return true;
    }


    return __super::sq_evt(qp, rid, data);
}

void gui_message_item_c::init_date_separator( const tm &tmtm )
{
    ASSERT(MTA_DATE_SEPARATOR == mt);

    subtype = ST_JUST_TEXT;
    flags.set(F_DIRTY_HEIGHT_CACHE);

    ts::swstr_t<-128> tstr;
    text_set_date(tstr, from_utf8(prf().date_sep_template()), tmtm);

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
        t.append(TTT("Key restored",199));
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
    b_load.set_text(TTT("Load $ message(s)",124) / ts::wstr_c().set_as_int(n_load));
    b_load.set_face_getter(BUTTON_FACE(button));
    b_load.set_handler(DELEGATE(author.get(), b_load), as_param(n_load));
    b_load.leech(TSNEW(leech_dock_bottom_center_s, 300, 30, -5, 5, 0, 1));
    MODIFY(b_load).visible(true);
}

bool gui_message_item_c::with_utag(uint64 utag) const
{
    if (utag == zero_rec.utag)
        return true;
#if JOIN_MESSAGES
    for (const record &rec : other_records)
        if (rec.utag == utag) return true;
#endif
    return false;
}

bool gui_message_item_c::remove_utag(uint64 utag)
{
    if (zero_rec.utag == utag)
    {
        zero_rec.utag = 0;
        return true;
    }
#if JOIN_MESSAGES
    int cnt = other_records.size();
    for (int i=0;i<cnt;++i)
    {
        record &rec = other_records.get(i);
        if (rec.utag == utag)
        {
            other_records.remove_slow(i);
            flags.set(F_DIRTY_HEIGHT_CACHE);
            rebuild_text();
            return true;
        }
    }
#endif
    return false;
}

bool gui_message_item_c::delivered(uint64 utag)
{
    if (utag == zero_rec.utag)
    {
        if (zero_rec.undelivered == 0) return false;
        zero_rec.undelivered = 0;

        goto rbld;
        rbld:
        rebuild_text();
        flags.set(F_DIRTY_HEIGHT_CACHE);

        int h = get_height_by_width(getprops().size().x);
        if (h != height)
        {
            height = h;
            MODIFY(*this).sizeh(height);
        }

        return true;
    }
#if JOIN_MESSAGES
    for(record &rec : other_records)
    {
        if (rec.utag == utag)
        {
            if (rec.undelivered == 0) return false;
            rec.undelivered = 0;
            goto rbld;
        }
    }
#endif

    return false;
}

bool gui_message_item_c::kill_self(RID, GUIPARAM)
{
    RID par = getparent();
    ASSERT( get_mt() == MTA_TYPING );
    TSDEL(this);
    gui->repos_children(&HOLD(par).as<gui_group_c>());
    return true;
}
bool gui_message_item_c::animate_typing(RID, GUIPARAM)
{
    update_text();
    DEFERRED_UNIQUE_CALL( 1.0/15.0, DELEGATE(this, animate_typing), nullptr );
    return true;
}

void gui_message_item_c::refresh_typing()
{
    ASSERT( get_mt() == MTA_TYPING );
    subtype = ST_TYPING;
    DEFERRED_UNIQUE_CALL( 1.5, DELEGATE(this, kill_self), nullptr );
    animate_typing(RID(),nullptr);
}


ts::uint16 gui_message_item_c::record::append( ts::wstr_c &t, ts::wstr_c &pret, const ts::wsptr &postt )
{
    ts::wstr_c tstr;
    tm tt;
    _localtime64_s(&tt, &time);
    if ( prf().get_options().is(MSGOP_SHOW_DATE) )
    {
        text_set_date(tstr,from_utf8(prf().date_msg_template()),tt);
        tstr.append_char(' ');
    }
    tstr.append_as_uint(tt.tm_hour);
    if (tt.tm_min < 10)
        tstr.append(CONSTWSTR(":0"));
    else
        tstr.append_char(':');
    tstr.append_as_uint(tt.tm_min);

    ts::uint16 time_str_width = 0;
#if JOIN_MESSAGES
    if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
        t.append(pret).append(tstr).append(postt);
    else
#endif
    {
        ts::ivec2 timeszie = g_app->tr().calc_text_size( *g_app->font_conv_time, tstr, 1024, 0, nullptr );
        time_str_width = (ts::uint16)timeszie.x;
        pret = tstr;
    }

    if (undelivered)
    {
        t.append(maketag_color<ts::wchar>(undelivered)).append(from_utf8(text)).append(CONSTWSTR("</color>"));
    }
    else
        t.append(from_utf8(text));

    if (time_str_width)
        t.append(CONSTWSTR("<nbsp=")).append_as_uint(time_str_width + ADDTIMESPACE).append_char('>');

    t.append(CONSTWSTR("<null=t")).append_as_hex(&utag, sizeof(utag)).append_char('>');

    return time_str_width;
}

static int prepare_link(ts::str_c &message, const ts::ivec2 &lrange, int n)
{
    ts::sstr_t<-128> inst(CONSTASTR("<cstm=b"));
    inst.append_as_uint(n).append(CONSTASTR(">"));
    message.insert(lrange.r1,inst);
    inst.set_char(6,'a');
    message.insert(lrange.r0,inst);
    return lrange.r1 + inst.get_length() * 2;
}

static void parse_links(ts::str_c &message, bool reset_n)
{
    static int n = 0;
    if (reset_n) n = 0;

    ts::ivec2 linkinds;
    for(int i = 0; text_find_link(message, i, linkinds) ;)
        i = prepare_link(message, linkinds, n++);
}

ts::pwstr_c gui_message_item_c::get_message_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const
{
    ts::ivec2 p = get_message_pos_under_cursor(localpos, mutag);
    if (p.r0 >= 0 && p.r1 >= p.r0) return textrect.get_text().substr(p.r0, p.r1);
    return ts::pwstr_c();
}


ts::ivec2 gui_message_item_c::get_message_pos_under_cursor(const ts::ivec2 &localpos, uint64 &mutag) const
{
    if (textrect.is_invalid_glyphs()) return ts::ivec2(-1);
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
    int rite = textrect.get_text().find_pos(chari, CONSTWSTR("<p><r>")); //-V807
    if (rite < 0) rite = textrect.get_text().get_length();
    int left = 0;
    if ( textrect.get_text().begins( CONSTWSTR("<cstm=z>") ) )
        left = 8;

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
    ts::TSCOLOR c = get_default_text_color(3);
    r.append(CONSTWSTR("<color=#")).append_as_hex(ts::RED(c))
        .append_as_hex(ts::GREEN(c))
        .append_as_hex(ts::BLUE(c))
        .append_as_hex(ts::ALPHA(c))
        .append(CONSTWSTR(">"));
    
    if (linknum == overlink)
        r.append(CONSTWSTR("<u>"));

}
/*virtual*/ void gui_message_item_c::get_link_epilog(ts::wstr_c & r, int linknum) const
{
    if (linknum == overlink)
        r.set(CONSTWSTR("</u></color>"));
    else
        r.set(CONSTWSTR("</color>"));

}

void gui_message_item_c::setup_found_item( uint64 prev, uint64 next )
{
    addition_prevnext_s *pn = TSNEW( addition_prevnext_s );
    pn->prev = prev;
    pn->next = next;
    addition.reset( pn );
    flags.set(F_FOUND_ITEM);
    MODIFY( *this ).highlight(true);
}

void gui_message_item_c::append_text( const post_s &post, bool resize_now )
{
#if JOIN_MESSAGES
    bool use0rec = (zero_rec.utag == post.utag);
    record &rec = use0rec ? zero_rec : other_records.add();
    if (!use0rec)
    {
        rec.utag = post.utag;
        rec.time = post.time;
    }
#else
    record &rec = zero_rec;
    if (zero_rec.utag == 0)
    {
        zero_rec.utag = post.utag;
        zero_rec.time = post.time;
    }
#endif

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
                newtext.append(TTT("Your request was accepted by [b]$[/b]",91) / from_utf8(aname));
            if (MTA_ACCEPT_OK == mt)
                newtext.append(TTT("You accepted contact [b]$[/b].",90) / from_utf8(aname));
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
                newtext.append(TTT("Call failed",137));
            else if (MTA_INCOMING_CALL_REJECTED == mt)
                newtext.append(TTT("Call rejected",135));
            else if (MTA_CALL_ACCEPTED == mt)
                newtext.append(TTT("Call started with $",138)/from_utf8(aname));
            else if (MTA_HANGUP == mt)
                newtext.append(TTT("Call with $ finished",139)/from_utf8(aname));

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
#if JOIN_MESSAGES
                parse_links(message, other_records.size() == 0);
#else
                parse_links(message, true);
#endif
                emoti().parse(message);
            }

            rec.text = message;
            rec.undelivered = post.type == MTA_UNDELIVERED_MESSAGE ? get_default_text_color(1) : 0;

            ts::wstr_c newtext(textrect.get_text());
#if JOIN_MESSAGES
            if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
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
#endif
            {
                if (flags.is(F_FOUND_ITEM))
                {
                    //ts::wstr_c bbb(CONSTWSTR("<p=c><rect=65000,120,-25,3><nbsp><rect=65001,120,-25,3></p><br>"));
                    //bbb.append_as_uint(100).append_char(',').append_as_int(-sz.y).append(CONSTWSTR(",2>"));
                    if ( addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()) )
                        pn->prevnext = CONSTWSTR("<p=c><rect=65000,120,-25,3><nbsp><rect=65001,120,-25,3></p><br>");
                    newtext.insert(0,CONSTWSTR("<cstm=z>"));
                }

                timestrwidth = rec.append(newtext, timestr);
                textrect.set_text_only(newtext, false);
                mark_found();
            }

        }
        break;
    
    }

    flags.set(F_DIRTY_HEIGHT_CACHE);

    if (resize_now)
    {
        int h = get_height_by_width(getprops().size().x);
        if (h != height)
        {
            height = h;
            MODIFY(*this).sizeh(height);
        }
    }
}

bool gui_message_item_c::message_prefix(ts::wstr_c &newtext, time_t posttime)
{
#if JOIN_MESSAGES
    if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
    {
        newtext.set(CONSTWSTR("<r><font=conv_text><color=#"));
        ts::TSCOLOR c = get_default_text_color(2);
        newtext.append_as_hex(ts::RED(c)).append_as_hex(ts::GREEN(c)).append_as_hex(ts::BLUE(c)).append_as_hex(ts::ALPHA(c)).append(CONSTWSTR("> "));
    } else
#endif
        newtext.clear();

    tm tt;
    _localtime64_s(&tt, &posttime);
    if (prf().get_options().is(MSGOP_SHOW_DATE))
    {
        ts::swstr_t<-128> tstr;
        text_set_date(tstr, from_utf8(prf().date_msg_template()), tt);
        newtext.append(tstr).append_char(' ');
    }

    newtext.append_as_uint(tt.tm_hour);
    if (tt.tm_min < 10)
        newtext.append(CONSTWSTR(":0"));
    else
        newtext.append_char(':');
    newtext.append_as_uint(tt.tm_min);

#if JOIN_MESSAGES
    if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
    {
        newtext.append(CONSTWSTR("</color></font></r>"));
        timestrwidth = 0;
        timestr.clear();
        return true;

    } else
#endif
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

void gui_message_item_c::goto_item( uint64 utag )
{
    rectengine_c &lst = HOLD(getparent()).engine();

    for (rectengine_c *e : lst)
        if (e)
        {
            gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>(&e->getrect());
            if (mi.get_mt() == MTA_TYPING)
                continue;

            if (mi.with_utag(utag))
            {
                gui_messagelist_c &ml = *ts::ptr_cast<gui_messagelist_c *>(&lst.getrect());
                ml.scroll_to_child( &mi.getengine(), true );
            }
        }
}

bool gui_message_item_c::b_prev(RID, GUIPARAM)
{
    if ( addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()) )
        goto_item( pn->prev );
    return true;
}
bool gui_message_item_c::b_next(RID, GUIPARAM)
{
    if (addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()))
        goto_item(pn->next);
    return true;
}


bool gui_message_item_c::b_explore(RID, GUIPARAM)
{
    if (zero_rec.text.get_char(0) == '*' || zero_rec.text.get_char(0) == '?') return true;
    ts::wstr_c fn = from_utf8(zero_rec.text);
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
    ts::safe_ptr<rectengine_c> e = &HOLD(btn).engine();

    file_transfer_s *ft = g_app->find_file_transfer_by_msgutag(zero_rec.utag);
    if (ft && ft->is_active())
        ft->kill();

    if (e) kill_button(e, 1);

    if (zero_rec.text.get_char(0) == '?')
    {
        // unfinished
        zero_rec.text.set_char(0, '*');

        if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.msgitem_utag == zero_rec.utag; }))
        {

            post_s p;
            p.sender = row->other.sender;
            p.message_utf8 = zero_rec.text;
            p.utag = zero_rec.utag;
            prf().change_history_item(row->other.historian, p, HITM_MESSAGE);
            if (contact_c * h = contacts().find(row->other.historian)) h->iterate_history([&](post_s &p)->bool {
                if (p.utag == zero_rec.utag)
                {
                    p.message_utf8 = zero_rec.text;
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
        DEFERRED_UNIQUE_CALL( 0, DELEGATE(this,b_pause_unpause), btn );
        return true;
    }

    gui_button_c &b = HOLD(RID::from_param(p)).as<gui_button_c>();
    int r = as_int(b.get_customdata());
    int r_to = r ^ 1;
    ASSERT(r == BTN_PAUSE || r == BTN_UNPAUSE);

    if (r == BTN_PAUSE)
        b.set_face_getter(BUTTON_FACE_PRELOADED(unpauseb));
    else
        b.set_face_getter(BUTTON_FACE_PRELOADED(pauseb));

    b.set_customdata(as_param(r_to));
    repl_button(r, r_to);

    file_transfer_s *ft = g_app->find_file_transfer_by_msgutag(zero_rec.utag);
    if (ft && ft->is_active())
        ft->pause_by_me(r == BTN_PAUSE);

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
    
    if (addition_file_data_s *ftb = ts::ptr_cast<addition_file_data_s *>( addition.get() ))
    {
        for( addition_file_data_s::btn_s &b : ftb->btns )
        {
            if (b.r == r_from)
            {
                b.r = r_to;
                break;
            }
        }
    }
}

void gui_message_item_c::kill_button( rectengine_c *beng, int r )
{
    if (addition_file_data_s *ftb = ts::ptr_cast<addition_file_data_s *>(addition.get()))
    {
        for (addition_file_data_s::btn_s &b : ftb->btns)
        {
            if (b.rid == beng->getrid())
            {
                ftb->btns.remove_fast( &b - ftb->btns.begin() );
                break;
            }
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

void gui_message_item_c::updrect_emoticons(const void *, int r, const ts::ivec2 &p)
{
    if (flags.is(F_FOUND_ITEM) && r >= BTN_PREV)
    {
        addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get());
        if (r == BTN_PREV && pn->rid_prev)
        {
            MODIFY(pn->rid_prev).pos(root_to_local(p));
            return;
        }
        if (r == BTN_NEXT && pn->rid_next)
        {
            MODIFY(pn->rid_next).pos(root_to_local(p));
            return;
        }

        gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
        bc.set_face_getter(BUTTON_FACE(button));
        MODIFY(bc).size(120, 25).pos(root_to_local(p)).visible(true);
        if (r == BTN_PREV)
        {
            bc.set_handler(DELEGATE(this, b_prev), nullptr);
            bc.set_text(TTT("Previous", 339));
            bc.leech(this);
            if (!pn->prev)
                bc.disable();
            pn->rid_prev = bc.getrid();
        }
        else if (r == BTN_NEXT)
        {
            bc.set_handler(DELEGATE(this, b_next), nullptr);
            bc.set_text(TTT("Next", 340));
            bc.leech(this);
            if (!pn->next)
                bc.disable();
            pn->rid_next = bc.getrid();
        }

        return;
    }


    emoti().draw( getroot(), p, r );
}

void gui_message_item_c::updrect(const void *, int r, const ts::ivec2 &p)
{
    if (RECT_IMAGE == r)
    {
        // image
        if (imgloader())
        {
            image_loader_c &ldr = imgloader_get();
            ldr.local_p = root_to_local(p) - ts::ivec2(4, 0);
            ldr.upd_btnpos();
            if (const picture_c *pic = ldr.get_picture())
                pic->draw(getroot(), p + ts::ivec2(0, 4));
        }
        return;
    } else if (TYPING_SPACERECT == r)
        return;


    addition_file_data_s &ftb = addition_file_data();
    for (addition_file_data_s::btn_s &b : ftb.btns)
    {
        if (b.r == r)
        {
            MODIFY(b.rid).pos(root_to_local(p));
            return;
        }
    }

    RID b;
    switch (r)
    {
        case BTN_EXPLORE:
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face_getter(BUTTON_FACE_PRELOADED(exploreb));
            bc.set_handler(DELEGATE(this, b_explore), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case BTN_BREAK:
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_face_getter(BUTTON_FACE_PRELOADED(breakb));
            bc.set_handler(DELEGATE(this, b_break), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case BTN_PAUSE:
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_customdata(as_param(BTN_PAUSE));
            bc.set_face_getter(BUTTON_FACE_PRELOADED(pauseb));
            bc.set_handler(DELEGATE(this, b_pause_unpause), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
        case BTN_UNPAUSE:
        {
            gui_button_c &bc = MAKE_CHILD<gui_button_c>(getrid());
            bc.set_customdata(as_param(BTN_UNPAUSE));
            bc.set_face_getter(BUTTON_FACE_PRELOADED(unpauseb));
            bc.set_handler(DELEGATE(this, b_pause_unpause), nullptr);
            MODIFY(bc).setminsize(bc.getrid()).pos(root_to_local(p)).visible(true);
            b = bc.getrid();
            break;
        }
    }
    if (b)
    {
        addition_file_data_s::btn_s &btn = ftb.btns.add();
        btn.rid = b;
        btn.r = r;
    }
}

ts::wstr_c gui_message_item_c::prepare_button_rect(int r, const ts::ivec2 &sz)
{
    ts::wstr_c button(CONSTWSTR("<rect=")); button.append_as_uint(r).append_char(',');
    button.append_as_uint(sz.x).append_char(',').append_as_int(-sz.y).append(CONSTWSTR(",2>"));


    addition_file_data_s &ftb = addition_file_data();
    for (addition_file_data_s::btn_s &b : ftb.btns)
        if (b.r == r)
        {
            b.used = true;
            break;
        }
    return button;
}

/*virtual*/ gui_message_item_c::addition_file_data_s::~addition_file_data_s()
{
    imgloader.destroy<image_loader_c>();
}

void gui_message_item_c::update_text(int for_width)
{
    bool is_send = false;
    switch (subtype)
    {
    case ST_SEND_FILE:
        is_send = true;
    case ST_RECV_FILE:
        {
            // for_width != 0 means fake update, just for correct height calculation

            ASSERT(for_width == 0 || imgloader());

            int rectw = for_width ? for_width : getprops().size().x;
            int oldh = for_width ? 0 : get_height_by_width(rectw);

            set_selectable(false);
            
            if (addition_file_data_s *ftb = ts::ptr_cast<addition_file_data_s *>(addition.get()))
                for (addition_file_data_s::btn_s &b : ftb->btns)
                    b.used = false;

            ts::wstr_c newtext(256,false);
            bool r_inserted = message_prefix(newtext, zero_rec.time);
            ts::wstr_c btns;
            auto insert_button = [&]( int r, const ts::ivec2 &sz )
            {
                if (r_inserted)
                {
                    if (newtext.substr(3).begins(CONSTWSTR("<rect=")))
                        newtext.insert(3, CONSTWSTR("<space=4>"));
                    newtext.insert(3, prepare_button_rect(r, sz));
                } else
                {
                    if (btns.begins(CONSTWSTR("<rect=")))
                        btns.insert(0, CONSTWSTR("<space=4>"));
                    btns.insert(0, prepare_button_rect(r, sz));
                }
            };

            ts::wstr_c fn = ts::fn_get_name_with_ext(from_utf8(zero_rec.text));
            ts::wchar fc = zero_rec.text.get_char(0);
            file_transfer_s *ft = nullptr;
            if (fc == '*')
            {
                newtext.append(CONSTWSTR("<img=file,-1>"));
                if (is_send)
                    newtext.append(TTT("Upload canceled: $",181) / fn);
                else
                    newtext.append(TTT("Download canceled: $",182) / fn);
            } else if (fc == '?')
            {
                ft = g_app->find_file_transfer_by_msgutag(zero_rec.utag);
                if (ft && ft->is_active()) goto transfer_alive;

                insert_button( BTN_BREAK, g_app->buttons().breakb->size );
                newtext.append(CONSTWSTR("<img=file,-1>"));
                newtext.append(TTT("Disconnected: $",235) / fn);
            }
            else
            {
                transfer_alive:
                if (!is_send)
                    insert_button( BTN_EXPLORE, g_app->buttons().exploreb->size );

                newtext.append(CONSTWSTR("<img=file,-1>"));

                if (nullptr == ft) ft = g_app->find_file_transfer_by_msgutag(zero_rec.utag);
                if (ft && ft->is_active())
                {
                    insert_button( BTN_BREAK, g_app->buttons().breakb->size );

                    if (is_send)
                        newtext.append(TTT("Upload: $",183) / fn);
                    else
                        newtext.append(TTT("Download: $",184) / fn);

                    int bps;
                    newtext.append(CONSTWSTR(" (<b>")).append_as_uint( ft->progress(bps) ).append(CONSTWSTR("</b>%, "));
                    
                    if (bps >= file_transfer_s::BPSSV_ALLOW_CALC)
                        insert_button( BTN_PAUSE, g_app->buttons().pauseb->size );

                    if (bps < file_transfer_s::BPSSV_ALLOW_CALC)
                    {
                        if (bps == file_transfer_s::BPSSV_PAUSED_BY_ME)
                            insert_button( BTN_UNPAUSE, g_app->buttons().unpauseb->size );
                        if (bps == file_transfer_s::BPSSV_WAIT_FOR_ACCEPT) 
                            newtext.append(TTT("waiting",185));
                        else
                            newtext.append(TTT("pause",186));
                    } else if (bps < 1024)
                        newtext.append(TTT("$ bytes per second",189) / ts::wmake(bps));
                    else if (bps < 1024 * 1024)
                        newtext.append(TTT("$ kbytes per second",190) / ts::wmake(bps/1024));
                    else
                        newtext.append(TTT("$ Mb per second",191) / ts::wmake(bps/(1024*1024)));

                    newtext.append_char(')');
                } else
                {
                    if (is_send)
                        newtext.append(TTT("File uploaded: $",193) / fn);
                    else
                        newtext.append(TTT("File: $",192) / fn);

                    
                    addition_file_data_s &ab = addition_file_data();
                    if (!ab.imgloader && image_loader_c::is_image_fn(zero_rec.text))
                        ab.imgloader.initialize<image_loader_c>( this, from_utf8(zero_rec.text) );
                }
            }

            if (imgloader() && rectw > 0)
            {
                image_loader_c &imgldr = imgloader_get();
                if (picture_c *pic = imgldr.get_picture())
                {
                    const theme_rect_s *thr = themerect();
                    ts::ivec2 picsz;
                    if (for_width == 0)
                    {
                        // normal picture size ajust
                        pic->fit_to_width(rectw - (thr ? thr->clborder_x() : 0));
                        picsz = pic->framerect().size();
                    } else
                    {
                        picsz = pic->framesize_by_width(rectw - (thr ? thr->clborder_x() : 0));
                        // no need to real picture size ajust (it is slow op), but just calculate height of picture by width
                    }

                    newtext.clear().append(CONSTWSTR("<p=c><rect=1000,")).append_as_uint(picsz.x).append_char(',').append_as_int(picsz.y + 8).append(CONSTWSTR("><br></p>"));
                    //                                           RECT_IMAGE

                    const button_desc_s *explorebdsc = g_app->buttons().exploreb;
                    if (picsz.x > explorebdsc->size.x && picsz.y > explorebdsc->size.y)
                    {
                        if ( imgldr.explorebtn.expired() )
                        {
                            if (addition_file_data_s *ftb = ts::ptr_cast<addition_file_data_s *>(addition.get()))
                                for (addition_file_data_s::btn_s &b : ftb->btns)
                                    b.used = false;

                            imgldr.explorebtn = MAKE_CHILD<gui_button_c>(getrid());
                            imgldr.explorebtn->set_face_getter(BUTTON_FACE_PRELOADED(exploreb));
                            imgldr.explorebtn->set_handler(DELEGATE(this, b_explore), nullptr);
                            imgldr.explorebtn->leech(&imgldr);
                        }

                        btns.clear();
                    } else if ( !imgldr.explorebtn.expired() )
                        TSDEL( imgldr.explorebtn.get() );
                }
            }

            if (!btns.is_empty())
                newtext.append(CONSTWSTR("<p=c>")).append(btns);
            else
                message_postfix(newtext);

            textrect.set_text_only(newtext,false);

            if (addition_file_data_s *ftb = ts::ptr_cast<addition_file_data_s *>(addition.get()))
            {
                for (int i =  ftb->btns.size() - 1; i >= 0; --i)
                {
                    addition_file_data_s::btn_s &b = ftb->btns.get(i);
                    if (!b.used)
                    {
                        TSDEL(&HOLD(RID(b.rid)).engine());
                        ftb->btns.remove_fast(i);
                    }
                }
            }

            if (!for_width)
            {
                flags.set(F_DIRTY_HEIGHT_CACHE);
                int newh = get_height_by_width(rectw);
                if (newh == oldh)
                    getengine().redraw();
                else
                    gui->repos_children(&HOLD(getparent()).as<gui_group_c>());
            }
        }
        break;
    case ST_TYPING:
        {
            int rectw = getprops().size().x;
            int oldh = get_height_by_width(rectw);

            ts::wstr_c tt(TTT("Typing",271));
            ts::wstr_c ttc = textrect.get_text();

            ts::wsptr recta = CONSTWSTR("<rect=1001,10,10>"); // width must be equal to m_left
            //                                 TYPING_SPACERECT
            ts::wsptr rectb = CONSTWSTR("_");

            if (timestr.is_empty())
            {
                ttc.clear();
                timestr.set(CONSTWSTR("__  __  __  __"));
            }


            if (ttc.is_empty()) ttc.set(recta).append(rectb);
            else
            {
                if (ttc.get_length() - recta.l - rectb.l < tt.get_length())
                {
                    ttc.set(recta).append(tt.substr(0, ttc.get_length()-recta.l-rectb.l+1)).append(rectb);
                }
                else
                {
                    ttc.trunc_length(rectb.l).append_char(timestr.get_last_char()).append( rectb.skip(1) );
                    timestr.trunc_length();
                }
            }

            textrect.set_text_only(ttc, false);
            flags.set(F_DIRTY_HEIGHT_CACHE);
            int newh = get_height_by_width(rectw);
            if (newh == oldh)
                getengine().redraw();
            else
                gui->repos_children(&HOLD(getparent()).as<gui_group_c>());
        }
        break;
    default:
        break;
    }
}

ts::wstr_c gui_message_item_c::hdr() const
{
    if (!author) return ts::wstr_c();
    ts::str_c n(author->get_name());
    text_adapt_user_input(n);
    if (prf().is_loaded() && prf().get_options().is(MSGOP_SHOW_PROTOCOL_NAME) && !historian->getkey().is_group())
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

/*virtual*/ bool gui_message_item_c::custom_tag_parser(ts::wstr_c& r, const ts::wsptr &tv) const
{
    if (tv.l)
    {
        if (tv.s[0] == 'x')
        {
            r = CONSTWSTR("<b>");
            return true;
        } else if (tv.s[0] == 'y')
        {
            r = CONSTWSTR("</b>");
            return true;
        } else if (tv.s[0] == 'z')
        {
            if ( addition_prevnext_s *pn = ts::ptr_cast<addition_prevnext_s *>(addition.get()) )
            {
                r = pn->prevnext;
                return true;
            }
        }
    }
    return __super::custom_tag_parser(r,tv);
}


int gui_message_item_c::calc_height_by_width(int w)
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
        flags.clear(F_DIRTY_HEIGHT_CACHE);

    } else
    {
        for(int i=0;i<ARRAY_SIZE(height_cache);++i)
            if ( height_cache[i].x == w ) return height_cache[i].y;
    }

update_size_mode:

    const theme_rect_s *thr = themerect();
    if (ASSERT(thr))
    {
        bool authorlineheight = true;
        switch (subtype)
        {
        case gui_message_item_c::ST_JUST_TEXT:
            authorlineheight = false;
            // no break here
        case gui_message_item_c::ST_RECV_FILE:
        case gui_message_item_c::ST_SEND_FILE:
        case gui_message_item_c::ST_TYPING:
            {
                ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
                if (sz.x < 0) sz.x = 0;
                if (sz.y < 0) sz.y = 0;
                int ww = sz.x;
                int h = m_top;
                if (authorlineheight && !flags.is(F_NO_AUTHOR))
                {
                    sz = gui->textsize(*g_app->font_conv_name, hdr(), ww);
                    h += sz.y;
                }

                if (imgloader() && ww)
                {
                    ts::wstr_c storetext = textrect.get_text();
                    update_text(ww); // do not resize picture now, just generate text with correct-sized image
                    sz = textrect.calc_text_size(ww, custom_tag_parser_delegate());
                    textrect.set_text_only(storetext, false);
                } else
                {
                    sz = textrect.calc_text_size(ww, custom_tag_parser_delegate());
                }

                if (gui_message_item_c::ST_TYPING == subtype) sz.y = textrect.font->height() + 2;
                if (update_size_mode)
                    textrect.set_size( ts::ivec2(ww, sz.y) );
                h = thr->size_by_clientsize(ts::ivec2(ww, sz.y + h + addheight), false).y;

                height_cache[next_cache_write_index] = ts::ivec2(w, h);
                ++next_cache_write_index;
                if (next_cache_write_index >= ARRAY_SIZE(height_cache)) next_cache_write_index = 0;
                return h;
            }
            break;
        case gui_message_item_c::ST_CONVERSATION:
            {
                ts::ivec2 sz = thr->clientrect(ts::ivec2(w, height), false).size();
                if (sz.x < 0) sz.x = 0;
                if (sz.y < 0) sz.y = 0;
                int ww = sz.x;

                int h = m_top;
                if ( !flags.is(F_NO_AUTHOR) )
                {
                    sz = gui->textsize(*g_app->font_conv_name, hdr(), ww);
                    h += sz.y;
                }
                sz = textrect.calc_text_size(ww - m_left, custom_tag_parser_delegate());
                if (update_size_mode)
                    textrect.set_size(ts::ivec2(ww - m_left, sz.y));
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
        struct indraw
        {
            indraw(gui_messagelist_c *ml) { ASSERT(current_draw_list == nullptr); current_draw_list = ml; }
            ~indraw() { ASSERT(current_draw_list); current_draw_list = nullptr; }
        } indraw_(this);

        bool r = __super::sq_evt(qp, rid, data);

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
                m_tooltip = TOOLTIP(TTT("Message history is not saved!",231));
                data.hintzone.accepted = true;
            }
        }
        // don't return, parent should process message
    }
    return __super::sq_evt(qp, rid, data);
}

static bool same_author( rectengine_c *prev, contact_c *author, const ts::str_c &skin )
{
    if (prev)
    {
        gui_message_item_c &pmi = *ts::ptr_cast<gui_message_item_c *>(&prev->getrect());
        if (!is_special_mt(pmi.get_mt()))
        {
            if (pmi.get_author() == author && pmi.themename().equals(CONSTASTR("message."), skin))
                return true;
        }
    }
    return false;
}

gui_message_item_c &gui_messagelist_c::get_message_item(message_type_app_e mt, contact_c *author, const ts::str_c &skin, time_t post_time, uint64 replace_post)
{
    ASSERT(MTA_FRIEND_REQUEST != mt);

    if (MTA_TYPING == mt)
    {
        bool free_index = false;
        for(gui_message_item_c *mi : typing)
        {
            if (mi == nullptr)
            {
                free_index = true;
                continue;
            }
            if (mi->get_author() == author)
            {
                if ( same_author (getengine().get_prev_child(&mi->getengine()), author, skin ))
                    mi->set_no_author();
                mi->refresh_typing();
                return *mi;
            }
        }

        rectengine_c *prev = getengine().get_last_child();
        gui_message_item_c &mi = MAKE_CHILD<gui_message_item_c>(getrid(), historian, author, skin, MTA_TYPING);
        mi.refresh_typing();
        if ( same_author (prev, author, skin ))
            mi.set_no_author();

        ts::safe_ptr<gui_message_item_c> smi( &mi );
        if (free_index)
        {
            int i = typing.find( nullptr );
            typing.set(i, smi);
        } else
            typing.add(smi);

        return mi;
    }

    if ( prf().get_options().is(MSGOP_SHOW_DATE_SEPARATOR) && post_time )
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
                if (mi.get_mt() == MTA_TYPING)
                    continue;

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

    if (MTA_MESSAGE == mt || MTA_UNDELIVERED_MESSAGE == mt)
    {
        for (gui_message_item_c *mi : typing)
        {
            TSDEL( mi );
        }
        typing.clear();
    }

    bool is_same_author = false;
    if (rectengine_c *e = getengine().get_last_child())
    {
        gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>( &e->getrect() );

        if (!is_special_mt(mi.get_mt()))
        {
            if (mi.get_author() == author && mi.themename().equals(CONSTASTR("message."), skin))
            {
#if JOIN_MESSAGES
                if (prf().get_options().is(MSGOP_JOIN_MESSAGES))
                    return mi;
#endif
                is_same_author = true;
            }
#if JOIN_MESSAGES
            if (post_time < mi.get_last_post_time())
                author->reselect(true);
#endif
        }
    }

    gui_message_item_c &r = MAKE_CHILD<gui_message_item_c>(getrid(), historian, author, skin, mt);
    if (is_same_author) r.set_no_author();
    return r;
}


ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_MESSAGE> &p) // show message in message list
{
    if (p.resend) return 0;
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

    gui->repos_children(this);

    return 0;
}

void gui_messagelist_c::clear_list()
{
    last_seen_post_time = 0;
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

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_REFRESH_SEARCH_RESULT> &)
{
    if (historian)
        if (historian->is_full_search_result() != flags.is(F_SEARCH_RESULTS_HERE) ||
            (g_app->active_contact_item == historian->gui_item && historian->is_full_search_result()))
        {
            flags.init(F_SEARCH_RESULTS_HERE, historian->is_full_search_result());
            historian->reselect(true);
        }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &p)
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

ts::uint32 gui_messagelist_c::gm_handler(gmsg<GM_HEARTBEAT> &)
{
    if (historian)
    {
        if (last_seen_post_time > historian->get_readtime())
        {
            historian->set_readtime(last_seen_post_time);
            prf().dirtycontact(historian->getkey());
        }

        g_app->update_blink_reason(historian->getkey());
    }
    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<GM_DROPFILES> &p)
{
    if (historian && !historian->getkey().is_self() && p.root == getrootrid())
    {
        historian->send_file(p.fn);
    }
    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_DELIVERED> &p)
{
    for(rectengine_c *e : getengine())
        if (e)
        {
            gui_message_item_c &mi = *ts::ptr_cast<gui_message_item_c *>( &e->getrect() );
            if ( mi.delivered(p.utag) )
            {
                mi.getengine().redraw();
                gui->repos_children(this);
                break;
            }
        }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_PROTO_LOADED> &p)
{
    if (historian)
        historian->reselect(false);
    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_TYPING> &p)
{
    if (prf().get_options().is(MSGOP_IGNORE_OTHER_TYPING))
        return 0;

    if (historian)
        if (contact_c *tc = historian->subget(p.contact))
            get_message_item(MTA_TYPING, tc, CONSTASTR("other"));

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_SUMMON_POST> &p)
{
    contact_c *sender = contacts().find(p.post.sender);
    if (!sender) return 0;
    contact_c *receiver = contacts().find(p.post.receiver);
    if (!receiver) return 0;

    if ( MTA_SEND_FILE == p.post.mt() )
        SWAP( sender, receiver ); // sorry for ugly code design

    contact_c * h = p.historian ? p.historian : get_historian(sender, receiver);
    if (h != historian) return 0;

    switch (p.post.mt())
    {
        case MTA_FRIEND_REQUEST:
            gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_FRIEND_REQUEST_RECV, p.post.message_utf8).send();
            break;
        case MTA_INCOMING_CALL:
            if (!historian->get_aaac())
                gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_INCOMING_CALL, p.post.message_utf8).send();
            break;
        case MTA_CALL_ACCEPTED:
            gmsg<ISOGM_NOTICE>(historian, sender, NOTICE_CALL_INPROGRESS).send();
            //break; // NO break here!
        default:
        {
            contact_c *cs = sender;
            if (cs->getkey().is_self() && receiver->getkey().protoid)
                cs = contacts().find_subself(receiver->getkey().protoid);

            gui_message_item_c &mi = get_message_item(p.post.mt(), cs, calc_message_skin(p.post.mt(), p.post.sender), p.post.time, p.replace_post ? p.post.utag : 0);

            if (p.found_item)
            {
                mi.setup_found_item(p.prev_found, p.next_found);
                if (p.unread && *p.unread == nullptr)
                    *p.unread = &mi.getengine();
            }
            else if (p.unread && *p.unread == nullptr && p.post.time >= historian->get_readtime())
                *p.unread = &mi.getengine();

            mi.append_text(p.post, false);
            if (p.replace_post && p.post.mt() != MTA_RECV_FILE && p.post.mt() != MTA_SEND_FILE)
                h->reselect(true);
        }
    }

    return 0;
}

ts::uint32 gui_messagelist_c::gm_handler(gmsg<ISOGM_SELECT_CONTACT> &p)
{
    self_selected = p.contact && p.contact->getkey().is_self();

    memset( &last_post_time, 0, sizeof(last_post_time) );

    clear_list();

    historian = p.contact;
    last_seen_post_time = historian ? historian->get_readtime() : 0;

    if (historian && !historian->keep_history())
        gui->register_hintzone(this);
    else
        gui->unregister_hintzone(this);

    if (!historian)
    {
        gmsg<ISOGM_NOTICE>( &contacts().get_self(), nullptr, NOTICE_NETWORK, ts::str_c() ).send(); // init notice list
        return 0;
    }
    flags.init(F_SEARCH_RESULTS_HERE, historian->is_full_search_result());
    gmsg<ISOGM_NOTICE>( historian, nullptr, NOTICE_NETWORK, ts::str_c() ).send(); // init notice list

    time_t before = now();
    if (historian->history_size()) before = historian->get_history(0).time;

    int min_hist_size = prf().min_history_load();
    int cur_hist_size = p.contact->history_size();

    int needload = min_hist_size - cur_hist_size;
    time_t at_least = historian->get_readtime();
    const found_item_s *found_item = nullptr;
    if (historian->is_full_search_result() && g_app->found_items)
        for (const found_item_s &fi : g_app->found_items->items)
            if (fi.historian == historian->getkey())
            {
                found_item = &fi;
                if (fi.mintime < at_least) at_least = fi.mintime;
                break;
            }
    if (at_least < before)
    {
        // we have to load all read
        int unread = prf().calc_history_between(historian->getkey(), at_least, before);
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
        needload = ts::tmax(10, prf().min_history_load());
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
        uint64 utag_prev = 0;
        uint64 utag_next = 0;
        bool fitm = false;
        if (found_item)
        {
            int index = found_item->utags.find_index( p.utag );
            if (index >= 0)
            {
                fitm = true;
                if (index > 0)
                    utag_prev = found_item->utags.get( index - 1 );
                if ( index < found_item->utags.count() - 1 )
                    utag_next = found_item->utags.get( index + 1 );
            }
        }

        gmsg<ISOGM_SUMMON_POST> summon(p, &first_unread, historian);
        if (fitm)
        {
            summon.prev_found = utag_prev;
            summon.next_found = utag_next;
            summon.found_item = true;
        }
        summon.send();
        return false;
    });

    not_at_end();
    if (p.scrollend)
    {
        if (first_unread)
            scroll_to_child(first_unread, true);
        else
            scroll_to_end();
    } else
        scroll_to_begin();
        
    gui->repos_children(this);

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
        static ts::Time last_enter_pressed = ts::Time::past();
        enter_key_options_s behsend = (enter_key_options_s)prf().ctl_to_send();
        bool ctrl_or_shift = GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_SHIFT) < 0;
        if (behsend == EKO_ENTER_NEW_LINE_DOUBLE_ENTER)
        {
            ts::Time tpressed = ts::Time::current();
            if ((tpressed - last_enter_pressed) < 500)
            {
                ctrl_or_shift = true;
                fix_last_enter = true;
            }
            behsend = EKO_ENTER_NEW_LINE;
        }
        if (behsend == EKO_ENTER_SEND) ctrl_or_shift = !ctrl_or_shift;
        if (!ctrl_or_shift)
        {
            last_enter_pressed = ts::Time::current();
            return false;
        }
    }

    if (fix_last_enter)
        backspace();

    contact_c *receiver = historian;
    if (receiver == nullptr) return true;
    if (receiver->is_meta())
        receiver = receiver->subget_for_send(); // get default subcontact for message
    if (receiver == nullptr) return true;
    
    gmsg<ISOGM_MESSAGE> msg(&contacts().get_self(), receiver, MTA_UNDELIVERED_MESSAGE );

    msg.post.message_utf8 = get_text_utf8();
    if (msg.post.message_utf8.is_empty()) return true;
    emoti().parse( msg.post.message_utf8, true );
    msg.post.message_utf8.trim_right( CONSTASTR("\r\n") );

    msg.send();

    return true;
}

bool gui_message_editor_c::clear_text(RID, GUIPARAM)
{
    set_text(ts::wstr_c());
    return true;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_SEND_MESSAGE> &p)
{
    return on_enter_press_func(RID(), nullptr) ? GMRBIT_ACCEPTED : 0;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_MESSAGE> & msg) // clear message editor
{
    if (msg.resend) return 0;
    if ( msg.pass == 0 && msg.sender->getkey().is_self() )
    {
        DEFERRED_CALL( 0, DELEGATE(this,clear_text), nullptr );
        messages.remove(historian->getkey());
        gui->set_focus(getrid());
        return GMRBIT_ACCEPTED;
    }

    return 0;
}

ts::uint32 gui_message_editor_c::gm_handler(gmsg<ISOGM_SELECT_CONTACT> &p)
{
    if ( !p.contact )
    {
        historian = nullptr;
        messages.clear();
        set_text(ts::wstr_c());
        return 0;
    }

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

    smiles.tooltip((GET_TOOLTIP)[]()->ts::wstr_c { return TTT("Insert emoticon ($)",268) / CONSTWSTR("Ctrl+S"); });
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

    RID r = MAKE_ROOT<dialog_smileselector_c>(smilebuttonrect, getrid());
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
    if (t.get_length() > 4096)
        return false; // limit size

    send_button->disable( t.is_empty() );

    if (contact_c * contact = message_editor->get_historian())
        if (!contact->getkey().is_self())
        {
            contact_c *tgt = contact->getkey().is_group() ? contact : contact->subget_for_send();
            if (tgt && tgt->get_state() == CS_ONLINE)
                if (active_protocol_c *ap = prf().ap(tgt->getkey().protoid))
                    ap->typing( t.is_empty() ? 0 : tgt->getkey().contactid );
        }

    return true;
}

/*virtual*/ void gui_message_area_c::created()
{
    leech(TSNEW( leech_save_size_s, CONSTASTR("msg_area_size"), ts::ivec2(0,60)) );

    message_editor = MAKE_VISIBLE_CHILD<gui_message_editor_c>( getrid() );
    message_editor->check_text_func = DELEGATE(this, change_text_handler);
    send_button = MAKE_VISIBLE_CHILD<gui_button_c>( getrid() );
    send_button->set_face_getter(BUTTON_FACE(send));
    send_button->tooltip( TOOLTIP( TTT("Send",7) ) );
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
    ts::wstr_c title(TTT("Send files",180));
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
        file_button->tooltip(TOOLTIP(TTT("File transmition not supported",188)));
    } else
    {
        file_button->enable();
        file_button->tooltip(TOOLTIP(TTT("Send file",187)));
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

    DMSG("CONVERSATION RID: " << getrid());
    DMSG("MSGLIST RID: " << msglist->getrid());
    DMSG("MESSAGEAREA RID: " << messagearea->getrid());
    DMSG("NOTICELIST RID: " << noticelist->getrid());

    return __super::created();
}

bool gui_conversation_c::hide_show_messageeditor(RID, GUIPARAM)
{
    bool show = flags.is(F_ALWAYS_SHOW_EDITOR);
    if (caption->contacted() && !caption->getcontact().getkey().is_self()) //-V807
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
            g_app->hide_show_messageeditor();
        }
        return 0;
    }

    ASSERT(caption->getcontact().is_rootcontact());
    if (caption->getcontact().subpresent( c.contact->getkey() ))
        caption->update_text();

    g_app->update_buttons_head();
    g_app->hide_show_messageeditor();

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_DO_POSTEFFECT> &f)
{
    if (f.bits & application_c::PEF_UPDATE_BUTTONS_MSG)
        messagearea->update_buttons();

    if (f.bits & application_c::PEF_UPDATE_BUTTONS_HEAD)
        caption->update_buttons();

    if (f.bits & application_c::PEF_SHOW_HIDE_EDITOR)
        hide_show_messageeditor();

    return 0;
}

/*virtual*/ void gui_conversation_c::datahandler(const void *data, int size)
{
    contact_key_s current_receiver;

    for (contact_c *c : avs) // transfer audio to all av contacts
    {
        if (c->is_mic_off())
            continue;

        if (c->getkey().is_group())
        {
            current_receiver = c->getkey();
            break;
        }

        c->subiterate([&](contact_c *sc) {
            if (sc->is_av())
                current_receiver = sc->getkey();
        });

        if (!current_receiver.is_empty())
            break;
    }

    if (!current_receiver.is_empty()) // only one contact receives sound at one time
        if (active_protocol_c *ap = prf().ap(current_receiver.protoid))
            ap->send_audio(current_receiver.contactid, capturefmt, data, size);
}

/*virtual*/ s3::Format *gui_conversation_c::formats(int &count)
{
    avformats.clear();
    for (contact_c *c : avs)
    {
        if (c->getkey().is_group())
        {
            if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                avformats.set(ap->defaudio());
        } else c->subiterate( [this](contact_c *sc)
        {
            if (sc->is_av())
                if (active_protocol_c *ap = prf().ap(sc->getkey().protoid))
                    avformats.set(ap->defaudio());
        } );
    }
    
    count = avformats.count();
    return count ? avformats.begin() : nullptr;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_AV_COUNT> &avc)
{
    avc.count = avs.size();
    return GMRBIT_ABORT;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_AV> &av)
{
    bool add = false;
    if (av.activated)
    {
        add = avs.set( av.multicontact );
    } else
    {
        avs.find_remove_fast( av.multicontact );
    }

    if ( avs.size() == 1 )
        start_capture();
    else if (avs.size() == 0)
        stop_capture();

    if (add)
        for (contact_c *cc : avs)
            cc->call_inactive(cc != av.multicontact);


    if (av.multicontact == get_selected_contact())
        if (av.activated)
            caption->update_buttons(); // it updates some stuff

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<GM_DRAGNDROP> &dnda)
{
    if (dnda.a == DNDA_CLEAN)
    {
        flags.clear(F_DNDTARGET);
        return 0;
    }
    gui_contact_item_c *ciproc = dynamic_cast<gui_contact_item_c *>(gui->dragndrop_underproc());
    if (!ciproc) return 0;
    if (dnda.a == DNDA_DROP)
    {
        if (flags.is(F_DNDTARGET))
        {
            contact_c *c = get_selected_contact();
            if (c->getkey().is_group())
                c->join_groupchat(&ciproc->getcontact());
        }
        return 0;
    }

    ts::irect rect = gui->dragndrop_objrect();
    int area = rect.area() / 3;
    int carea = getprops().screenrect().intersect_area(rect);
    flags.init(F_DNDTARGET, carea > area);
    return flags.is(F_DNDTARGET) ? GMRBIT_ACCEPTED : 0;
}


ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_CALL_STOPED> &c)
{
    contact_c *owner = c.subcontact->getmeta();
    if (owner == get_selected_contact())
        caption->update_buttons();
    return 0;
}

ts::uint32 gui_conversation_c::gm_handler( gmsg<ISOGM_SELECT_CONTACT> &c )
{
    if (!c.contact)
    {
        caption->resetcontact();
        g_app->hide_show_messageeditor();
        return 0;
    }

    caption->setcontact( c.contact );

    if (c.contact->is_av())
    {
        start_capture();
        for (contact_c *cc : avs)
            cc->call_inactive( cc != c.contact );
    }

    g_app->hide_show_messageeditor();

    if (c.contact->getkey().is_group())
    {
        DEFERRED_EXECUTION_BLOCK_BEGIN(0)
            gmsg<ISOGM_NOTICE>((contact_c *)param, (contact_c *)param, NOTICE_GROUP_CHAT).send();
        DEFERRED_EXECUTION_BLOCK_END(c.contact.get());
    }

    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass > 0 && caption->contacted() && caption->getcontact().getkey().is_self())
    {
        switch (ch.sp)
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
        if (ch.sp == PP_PROFILEOPTIONS)
            caption->getcontact().reselect(true);
    }
    if (ch.sp == PP_EMOJISET && caption->contacted())
        caption->getcontact().reselect(true);

    if (ch.pass == 0 && ch.sp == PP_ACTIVEPROTO_SORT)
        caption->clearprotocols();


    return 0;
}

ts::uint32 gui_conversation_c::gm_handler(gmsg<ISOGM_PROFILE_TABLE_SAVED>&p)
{
    if (p.tabi == pt_active_protocol)
    {
        if (caption->contacted()) caption->getcontact().reselect(true);
        caption->update_text();
        g_app->update_buttons_head();
        g_app->hide_show_messageeditor();
    }

    return 0;
}
