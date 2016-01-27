#include "isotoxin.h"


dialog_contact_props_c::dialog_contact_props_c(MAKE_ROOT<dialog_contact_props_c> &data) :gui_isodialog_c(data)
{
    deftitle = title_contact_properties;
    contactue = contacts().rfind(data.prms.key);
    update();
}

dialog_contact_props_c::~dialog_contact_props_c()
{
}

/*virtual*/ ts::wstr_c dialog_contact_props_c::get_name() const
{
    return __super::get_name().append(CONSTWSTR(" - ")).append(gui_dialog_c::get_name());
}


/*virtual*/ void dialog_contact_props_c::created()
{
    set_theme_rect(CONSTASTR("cprops"), false);
    __super::created();
    tabsel(CONSTASTR("1"));

}

/*virtual*/ ts::ivec2 dialog_contact_props_c::get_min_size() const
{
    return ts::ivec2(530, 385);
}

void dialog_contact_props_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
}

bool dialog_contact_props_c::custom_name( const ts::wstr_c & cn )
{
    customname = to_utf8(cn);
    return true;
}

bool dialog_contact_props_c::comment( const ts::wstr_c &c )
{
    ccomment = to_utf8(c);
    return true;
}

bool dialog_contact_props_c::tags_handler(const ts::wstr_c &ht)
{
    tags.split<char>( to_utf8(ht), ',' );
    tags.trim();
    tags.sort(true);
    return true;
}

void dialog_contact_props_c::history_settings( const ts::str_c& v )
{
    keeph = (keep_contact_history_e)v.as_uint();
    set_combik_menu(CONSTASTR("history"), gethistorymenu());
}

void dialog_contact_props_c::aaac_settings( const ts::str_c& v )
{
    aaac = (auto_accept_audio_call_e)v.as_uint();
    set_combik_menu(CONSTASTR("aaac"), getaacmenu());
}

menu_c dialog_contact_props_c::gethistorymenu()
{
    menu_c m;
    m.add( TTT("Use global setting",226), (keeph == KCH_DEFAULT) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_DEFAULT) );
    m.add( TTT("Always save",227), (keeph == KCH_ALWAYS_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_ALWAYS_KEEP) );
    m.add( TTT("Never save",228), (keeph == KCH_NEVER_KEEP) ? MIF_MARKED : 0, DELEGATE(this, history_settings), ts::amake<char>(KCH_NEVER_KEEP) );

    return m;
}

menu_c dialog_contact_props_c::getaacmenu()
{
    menu_c m;
    m.add(TTT("Don't accept",285), (aaac == AAAC_NOT) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_NOT));
    m.add(TTT("Accept and mute microphone",286), (aaac == AAAC_ACCEPT_MUTE_MIC) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_ACCEPT_MUTE_MIC));
    m.add(TTT("Accept and don't mute microphone",287), (aaac == AAAC_ACCEPT) ? MIF_MARKED : 0, DELEGATE(this, aaac_settings), ts::amake<char>(AAAC_ACCEPT));

    return m;
}

/*virtual*/ int dialog_contact_props_c::additions(ts::irect &edges)
{
    descmaker dm(this);

    if (contactue)
    {
        customname = contactue->get_customname();
        ccomment = contactue->get_comment();
        keeph = contactue->get_keeph();
        aaac = contactue->get_aaac();
        tags = contactue->get_tags();

        dm << 1;

        ts::str_c n = contactue->get_name();
        text_adapt_user_input(n);

        dm().page_caption(from_utf8(n));

        dm().textfield(TTT("Custom name",229), from_utf8(customname), DELEGATE(this, custom_name))
            .focus(true);
        dm().vspace();

        dm().combik(TTT("Message history",230)).setmenu(gethistorymenu()).setname(CONSTASTR("history"));
        dm().vspace();

        dm().combik(TTT("Auto accept audio calls",288)).setmenu(getaacmenu()).setname(CONSTASTR("aaac"));
        dm().vspace();

        dm().textfieldml(TTT("Tags (comma-separated list of phrases/words)", 53), from_utf8(tags.join(CONSTASTR(", "))), DELEGATE(this, tags_handler), 3).focus(true).setname(CONSTASTR("tags"));

        dm << 2;
        dm().list(L"", L"", -250).setname(CONSTASTR("list"));

        dm << 4;
        dm().textfieldml(L"", from_utf8(ccomment), DELEGATE(this, comment), 12).focus(true);

    }

    menu_c m;
    m.add( TTT("Settings",369), 0 , TABSELMI(1) );
    m.add( TTT("Details",370), 0 , TABSELMI(2) );
    m.add( TTT("Comment",371), 0 , TABSELMI(4) );

    gui_htabsel_c &tab = MAKE_CHILD<gui_htabsel_c>(getrid(), m);
    edges = ts::irect(0, tab.get_min_size().y, 0, 0);


    return 1;
}

/*virtual*/ bool dialog_contact_props_c::sq_evt(system_query_e qp, RID rid, evt_data_s &d)
{
    if (__super::sq_evt(qp, rid, d)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

/*virtual*/ void dialog_contact_props_c::on_confirm()
{
    if (contactue && contacts().find(contactue->getkey()) != nullptr)
    {
        bool changed = false;
        if (contactue->get_customname() != customname)
        {
            changed = true;
            contactue->set_customname(customname);
        }
        if (contactue->get_comment() != ccomment)
        {
            changed = true;
            contactue->set_comment(ccomment);
        }
        if (contactue->get_keeph() != keeph)
        {
            changed = true;
            contactue->set_keeph(keeph);
        }
        if (contactue->get_aaac() != aaac)
        {
            changed = true;
            contactue->set_aaac(aaac);
        }
        if (contactue->get_tags() != tags)
        {
            changed = true;
            contactue->set_tags(tags);
            contacts().rebuild_tags_bits();
        }

        if (changed)
        {
            prf().dirtycontact(contactue->getkey());
            contactue->reselect();
        }
    }

    __super::on_confirm();
}


void dialog_contact_props_c::update()
{
    if (contactue) contactue->subiterate([&](contact_c *c)
    {
        ts::str_c par;

        cinfo_s &inf = getinfo(c);

        if (const avatar_s *ava = c->get_avatar())
        {
            inf.bmp = ava;
        }
        else
        {
            if (!inf.btmp)
                inf.btmp.reset( TSNEW(ts::bitmap_c) );

            const theme_image_s *icon = g_app->preloaded_stuff().icon[c->get_gender()];
            inf.bmp = inf.btmp.get();
            ts::irect rect = icon->rect;

            if (inf.btmp->info().sz != rect.size())
                inf.btmp->create_ARGB( rect.size() );

            inf.btmp->copy( ts::ivec2(0), rect.size(), icon->dbmp->extbody(), rect.lt );


        }

        inf.cldets.parse(c->get_details());
    });
}


ts::uint32 dialog_contact_props_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &c)
{
    if (contactue && contactue->subpresent(c.contact->getkey()))
    {
        update();
        fill_list();
    }

    return 0;
}

namespace customel
{
    class gui_lli_c;
    template<> struct MAKE_CHILD<gui_lli_c> : public _PCHILD(gui_lli_c), public gui_listitem_params_s
    {
        ts::astrings_c links;
        ts::buf_c qrs;
        MAKE_CHILD(ts::astrings_c &&links_, ts::buf_c &&qrs_, RID parent_, const ts::wstr_c &text_, const ts::str_c &param_)
        {
            parent = parent_;
            text = text_;
            param = param_;
            addmarginleft = 5;
            addmarginleft_icon = 3;
            links = std::move(links_);
            qrs = std::move(qrs_);
        }
        ~MAKE_CHILD();

        MAKE_CHILD &operator<<(const ts::bitmap_c *_icon) { icon = _icon; return *this; }
        MAKE_CHILD &operator<<(GETMENU_FUNC _gm) { gm = _gm; return *this; }
        //MAKE_CHILD &threct(const ts::asptr&thr) { themerect = thr; return *this; }
    };

    class gui_lli_c : public gui_listitem_c
    {
        ts::astrings_c links;
        ts::buf_c qrs;
        int clicklink = -1;
        system_query_e clicka = SQ_NOP;
        int flashing = 0;
        void flash()
        {
            flashing = 4;
            flash_pereflash(RID(), nullptr);
        }

        bool flash_pereflash(RID, GUIPARAM)
        {
            --flashing;
            if (flashing > 0) DEFERRED_UNIQUE_CALL(0.1, DELEGATE(this, flash_pereflash), nullptr);

            textrect.make_dirty();
            getengine().redraw();

            if (flashing == 0)
                clicklink = -1;

            return true;
        }

    public:

        gui_lli_c(MAKE_CHILD<gui_lli_c> &data):gui_listitem_c(data, data)
        {
            links = std::move(data.links);
            qrs = std::move(data.qrs);
        }
        ~gui_lli_c()
        {
            if (gui)
                gui->delete_event(DELEGATE(this, flash_pereflash));
        }
        /*virtual*/ void get_link_prolog(ts::wstr_c &r, int linknum) const override
        {
            r.clear();
            if (linknum == clicklink)
            {
                if (popupmenu || flashing > 0)
                {
                    r = maketag_mark<ts::wchar>(0 != (flashing & 1) ? gui->selection_bg_color_blink : gui->selection_bg_color);
                    r.append(maketag_color<ts::wchar>(gui->selection_color));
                }
                return;
            }
                    
            if (linknum == overlink)
                r.set(CONSTWSTR("<u>"));

        }
        /*virtual*/ void get_link_epilog(ts::wstr_c &r, int linknum) const override
        {
            r.clear();
            if (linknum == clicklink)
            {
                if (popupmenu || flashing > 0)
                {
                    r = CONSTWSTR("</color></mark>");
                }
                return;
            }
            if (linknum == overlink)
                r.set(CONSTWSTR("</u>"));
        }

        void ctx_copy(const ts::str_c &cc)
        {
            ts::str_c c(cc);
            text_convert_to_bbcode(c);
            ts::set_clipboard_text( from_utf8(c) );
            flash();
        }

        void ctx_qrcode(const ts::str_c &cc)
        {
            dialog_msgbox_c::mb_qrcode(from_utf8(cc)).summon();
        }

        /*virtual*/ bool sq_evt(system_query_e qp, RID rid, evt_data_s &data)
        {
            if (rid != getrid())
            {
                if (popupmenu && popupmenu->getrid() == rid)
                {
                    if (SQ_POPUP_MENU_DIE == qp)
                    {
                        textrect.make_dirty();
                        if (flashing == 0)
                        {
                            MODIFY(*this).highlight(false);
                            clicklink = -1;
                        }
                        getengine().redraw();
                    }
                }

                return __super::sq_evt(qp, rid, data);
            }

            switch(qp)
            {
            case SQ_MOUSE_LDOWN:
                if (popupmenu.expired())
                {
                    clicklink = -1;
                    flashing = 0;
                    clicka = SQ_NOP;
                    if (overlink >= 0)
                        clicka = SQ_MOUSE_LUP, clicklink = overlink;
                }
                break;
            case SQ_MOUSE_RDOWN:
                if (popupmenu.expired())
                {
                    clicklink = -1;
                    flashing = 0;
                    clicka = SQ_NOP;
                    if (overlink >= 0)
                        clicka = SQ_MOUSE_RUP, clicklink = overlink;
                }
                break;
            }

            if (SQ_MOUSE_LUP == qp || SQ_MOUSE_RUP == qp)
            {
                if (clicka == qp && overlink == clicklink && ASSERT(overlink < links.size()))
                {
                    menu_c mnu;
                    mnu.add(loc_text(loc_copy), 0, DELEGATE(this, ctx_copy), links.get(overlink));
                    if (qrs.get_bit(overlink)) mnu.add(loc_text(loc_qrcode), 0, DELEGATE(this, ctx_qrcode), links.get(overlink));

                    popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), mnu);
                    popupmenu->leech(this);
                    textrect.make_dirty();
                    getengine().redraw();
                }
                clicka = SQ_NOP;
                return true;
            }
            return __super::sq_evt(qp, rid, data);
        }

    };
}

MAKE_CHILD<customel::gui_lli_c>::~MAKE_CHILD()
{
    MODIFY(get()).visible(true);
    //get().set_autoheight();
    get().set_text(text);
    get().set_gm(gm);
}


void dialog_contact_props_c::add_det(RID lst, contact_c *c)
{
    ts::str_c par;

    cinfo_s &inf = getinfo(c);

    ts::wstr_c desc;
    ts::astrings_c vals;
    ts::buf_c qrs;

    auto addl = [&]( const ts::wsptr &fn, const ts::asptr &v, bool empty_as_unknown, bool ee, bool qr, int link_i )
    {
        if (ee)
            desc.append(CONSTWSTR("<ee>"));
        desc.append(maketag_color<ts::wchar>(get_default_text_color(2))).append(fn).append(CONSTWSTR("</color>: "));
        if (v.l > 0)
        {
            if (link_i >= 0)
            {
                desc.append(CONSTWSTR("<cstm=a")).append_as_uint(link_i).append_char('>');
                if (qr) qrs.set_bit(link_i, true);
            }
            desc.append(ts::from_utf8(v));
            if (link_i >= 0) desc.append(CONSTWSTR("<cstm=b")).append_as_uint(link_i).append_char('>');

        } else if (empty_as_unknown)
        {
            desc.append(CONSTWSTR("<l>")).append(TTT("unknown",362)).append(CONSTWSTR("</l>"));
        }
        desc.append(CONSTWSTR("<br>"));

    };

    auto extractpubids = [](ts::astrings_c &pubids, const ts::json_c &ids)
    {
        if (ids.is_string())
        {
            pubids.add( ids.as_string() );
        } else if (ids.is_array())
        {
            ids.iterate([&](const ts::str_c&, const ts::json_c &id){
                pubids.add( id.as_string() );
            });
        }
    };

    ts::str_c temp( c->get_name() );
    text_adapt_user_input( temp );
    addl( TTT("User name",364), temp, false, false, false, vals.add(temp) );

    temp = c->get_statusmsg();
    text_adapt_user_input(temp);
    addl(TTT("User status",365), temp, false, false, false, vals.add(temp) );

    //desc.insert(desc.get_length()-1, CONSTWSTR("=3"));

    bool something = false;
    inf.cldets.iterate([&](const ts::str_c &dname, const ts::json_c &v) {

        if (dname.equals(CONSTASTR(CDET_CLIENT)))
        {
            something = true;
            addl( TTT("Client",366), v.as_string(), true, false, false, vals.add(v.as_string()));
        } else if (dname.equals(CONSTASTR(CDET_PUBLIC_ID)))
        {
            ts::astrings_c pubids;
            extractpubids(pubids, v);
            for(const ts::str_c &idx : pubids)
                addl( loc_text(loc_id), idx, true, true, true, vals.add(idx));
        } else if (dname.equals(CONSTASTR(CDET_PUBLIC_ID_BAD)))
        {
            ts::astrings_c pubids;
            extractpubids(pubids, v);
            for (const ts::str_c &idx : pubids)
                addl(loc_text(loc_id), maketag_color<char>(get_default_text_color(3)) + idx + CONSTASTR("</color>"), true, true, true, vals.add(idx));
        } else if (dname.equals(CONSTASTR(CDET_DNSNAME)))
        {
            ts::astrings_c pubids;
            extractpubids(pubids, v);
            for (const ts::str_c &idx : pubids)
                addl(TTT("DNS name",367), idx, true, true, true, vals.add(idx));
        } else if (dname.equals(CONSTASTR(CDET_EMAIL)))
        {
            ts::astrings_c pubids;
            extractpubids(pubids, v);
            for (const ts::str_c &idx : pubids)
                addl(TTT("Email",368), idx, true, true, true, vals.add(idx));
        }

    });

    //if (something)
        //desc.insert(desc.get_length()-1, CONSTWSTR("=3"));

    if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
    {
        addl(loc_text(loc_connection_name), ap->get_name(), false, false, false, vals.add(ap->get_name()));
        addl(loc_text(loc_module), ap->get_desc_t(), false, false, false, vals.add(ap->get_desc_t()));
    }
    addl(loc_text(loc_state), to_utf8(text_contact_state(get_default_text_color(0), get_default_text_color(1), c->get_state())), true, false, false, -1);

    MAKE_CHILD<customel::gui_lli_c>(std::move(vals), std::move(qrs), lst, desc, par) << (const ts::bitmap_c *)(inf.bmp);
}

void dialog_contact_props_c::fill_list()
{
    if (contactue)
        if (RID lst = find(CONSTASTR("list")))
        {
            HOLD(lst).engine().trunc_children(0);

            contactue->subiterate([&](contact_c *c) {
                add_det(lst, c);
            });

        }
}

void dialog_contact_props_c::tags_menu_handler(const ts::str_c&h)
{
    int hi = tags.find(h.as_sptr());
    if (hi >= 0)
    {
        tags.remove_slow(hi);
    } else
    {
        tags.add(h);
        tags.sort(true);
    }
    set_edit_value(CONSTASTR("tags"), from_utf8(tags.join(CONSTASTR(", "))) );
}

void dialog_contact_props_c::tags_menu(menu_c &m)
{
    ts::astrings_c ctags( tags );
    ctags.add(contacts().get_all_tags());
    ctags.kill_dups_and_sort(true);

    for (const ts::str_c &h : ctags)
    {
        m.add( ts::from_utf8(h), tags.find(h.as_sptr()) >= 0 ? MIF_MARKED : 0, DELEGATE(this, tags_menu_handler), h );
    }
}

/*virtual*/ void dialog_contact_props_c::tabselected(ts::uint32 mask)
{
    if (mask & 1)
        if (RID e = find(CONSTASTR("tags")))
            HOLD(e).as<gui_textedit_c>().ctx_menu_func = DELEGATE(this, tags_menu);

    if (mask & 2)
        fill_list();

}







