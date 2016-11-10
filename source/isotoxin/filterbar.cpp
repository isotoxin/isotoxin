#include "isotoxin.h"

#define COL_PLACEHOLDER 0
#define COL_INACTIVE 1
#define COL_INACTIVE_HOVER 2
#define COL_ACTIVE 3
#define COL_ACTIVE_HOVER 4
#define COL_ACTIVE_MUTED 5
#define COL_ACTIVE_MUTED_HOVER 6

MAKE_CHILD<gui_filterbar_c>::~MAKE_CHILD()
{
    //MODIFY(get()).visible(true);
}


gui_filterbar_c::gui_filterbar_c(MAKE_CHILD<gui_filterbar_c> &data):gui_label_ex_c(data)
{
    bitags = prf().bitags();
    binames.parse( prf().bitagnames() );
    g_app->found_items = &found_stuff;
}
gui_filterbar_c::~gui_filterbar_c()
{
    if (g_app)
    {
        g_app->found_items = nullptr;
        g_app->delete_event( DELEGATE(this, do_contact_check) );
    }
}

/*virtual*/ void gui_filterbar_c::created()
{
    set_theme_rect(CONSTASTR("filter"), false);

    filtereditheight = gui->theme().conf().get_int(CONSTASTR("filtereditheight"), 25);

    if (prf().get_options().is(UIOPT_SHOW_SEARCH_BAR))
    {
        gui_textfield_c &e = (MAKE_CHILD<gui_textfield_c>(getrid(), L"", MAX_PATH_LENGTH, 0, false) << (gui_textedit_c::TEXTCHECKFUNC)DELEGATE(this, update_filter));
        edit = &e;
        e.set_placeholder(TOOLTIP(TTT("Search", 277)), get_default_text_color(COL_PLACEHOLDER));
        e.register_kbd_callback(DELEGATE(this, cancel_filter), ts::SSK_ESC, false);
    }
    if (prf().get_options().is(UIOPT_TAGFILETR_BAR))
    {
        fill_tags();
    }

    search_in_messages = prf().is_loaded() && prf().get_options().is(MSGOP_FULL_SEARCH);

    if (!is_all())
        refresh_list(true);

    __super::created();
}

void gui_filterbar_c::get_link_prolog(ts::wstr_c &r, int linknum) const
{
    bool is_active = false;
    if (linknum < BIT_count)
    {
        is_active = 0 != (bitags & (1 << linknum));
    } else
    {
        is_active = contacts().is_tag_enabled(linknum - BIT_count);
    }

    bool all = (0 != (bitags & (1 << BIT_ALL))) && linknum != BIT_ALL;

    ts::TSCOLOR c;
    if (linknum == overlink)
    {
        r.set(CONSTWSTR("<u>"));
        c = get_default_text_color(is_active ? ( all ? COL_ACTIVE_MUTED_HOVER : COL_ACTIVE_HOVER) : COL_INACTIVE_HOVER);
    } else
    {
        c = get_default_text_color(is_active ? (all ? COL_ACTIVE_MUTED : COL_ACTIVE) : COL_INACTIVE);
    }
    r.append(CONSTWSTR("<color=#")).append_as_hex(ts::RED(c))
        .append_as_hex(ts::GREEN(c))
        .append_as_hex(ts::BLUE(c))
        .append_as_hex(ts::ALPHA(c))
        .append_char('>');

}
void gui_filterbar_c::get_link_epilog(ts::wstr_c &r, int linknum) const
{
    if (linknum == overlink)
        r.set(CONSTWSTR("</color></u>"));
    else
        r.set(CONSTWSTR("</color>"));

}

void gui_filterbar_c::do_tag_click(int lnk)
{
    if ( lnk < BIT_count )
    {
        // process buildin tags
        INVERTFLAG( bitags, (1<<lnk) );
        prf().bitags( bitags );
    } else
    {
        lnk -= BIT_count;
        contacts().toggle_tag(lnk);
    }
    refresh_list(true);
    textrect.make_dirty();
    getengine().redraw();
}

ts::wstr_c gui_filterbar_c::tagname( int index ) const
{
    if (index < BIT_count)
    {
        if ( const ts::wstr_c * n = binames.find( ts::wmake(index) ) )
            return *n;

        ts::wsptr bit[BIT_count] =
        {
            TTT("All", 83),
            TTT("Online", 86),
            TTT("Untagged",263),
        };
        return bit[index];
    }
    return from_utf8( contacts().get_all_tags().get(index - BIT_count) );
}

bool gui_filterbar_c::renamed(const ts::wstr_c &tn, const ts::str_c &tis)
{
    int ti = tis.as_int();

    if (ti < BIT_count)
    {
        if (tn.is_empty())
        {
            binames.unset( to_wstr(tis) );
        } else
        {
            binames.set( to_wstr(tis) ) = tn;
        }
        prf().bitagnames( binames.to_str() );
    } else
    {
        ti -= BIT_count;
        contacts().replace_tags( ti, to_utf8(tn) );
    }

    g_app->recreate_ctls(true, false);
    return true;
}

void gui_filterbar_c::ctx_rename_tag(const ts::str_c &tis)
{
    int ti = tis.as_int();

    ts::wstr_c hint = TTT("Rename tag $", 218) / ts::wstr_c(CONSTWSTR("<b>"),tagname(ti),CONSTWSTR("</b>"));
    if (ti < BIT_count)
        hint.append(CONSTWSTR("<br>")).append( TTT("Empty - set default",249) );


    SUMMON_DIALOG<dialog_entertext_c>(UD_RENTAG, true, dialog_entertext_c::params(
        UD_RENTAG,
        gui_isodialog_c::title(title_rentag),
        hint,
        tagname(ti),
        tis,
        DELEGATE(this, renamed),
        nullptr,
        ti < BIT_count ? check_always_ok : check_always_ok_except_empty,
        getrid()));

}

void gui_filterbar_c::do_tag_rclick(int lnk)
{
    menu_c mnu;
    mnu.add(TTT("Rename tag",224), 0, DELEGATE(this, ctx_rename_tag), ts::amake(lnk));
    popupmenu = &gui_popup_menu_c::show(menu_anchor_s(true), mnu);
    popupmenu->leech(this);
    textrect.make_dirty();
    getengine().redraw();

}

void gui_filterbar_c::fill_tags()
{
    int lnki = 0;
    auto make_ht = [&]( const ts::wsptr &htt ) -> ts::wstr_c
    {
        ts::wstr_c x( CONSTWSTR("<cstm=a\1>"), htt );
        x.append( CONSTWSTR("<cstm=b\1>, ") );
        x.replace_all( CONSTWSTR("\1"), ts::wmake<int>( lnki++ ) );
        return x;
    };

    ts::wstr_c t;
    for (int i = 0; i < BIT_count; ++i)
        t.append( make_ht(tagname(i)) );

    for(const ts::str_c &ht : contacts().get_all_tags())
        t.append(make_ht(from_utf8(ht)));
    
    t.trunc_length(2);
    set_text(t);
}


bool gui_filterbar_c::cancel_filter(RID, GUIPARAM)
{
    if (edit->is_empty())
        return false;
    edit->set_text(ts::wstr_c());
    return true;
}



void gui_filterbar_c::show_options(bool show)
{
    if (show && option1 != nullptr)
        return;
    if (!show && option1 == nullptr)
        return;

    if (show)
    {
        int tag = gui->get_free_tag();
        gui_button_c &o1 = MAKE_VISIBLE_CHILD<gui_button_c>(getrid());
        o1.set_check(tag);
        o1.set_handler(DELEGATE(this, option_handler), as_param(1));
        o1.set_face_getter(BUTTON_FACE(check));
        o1.set_text(TTT("Search in messages",337));
        if (search_in_messages)
            o1.mark();

        option1 = &o1;
    } else
    {
        TSDEL( option1 );
    }

    HOLD( getparent() ).as<gui_contactlist_c>().update_filter_pos();
}

bool gui_filterbar_c::option_handler(RID, GUIPARAM p)
{
    search_in_messages = p != nullptr;
    if (prf().is_loaded())
        prf().set_options( search_in_messages ? MSGOP_FULL_SEARCH : 0, MSGOP_FULL_SEARCH );
    found_stuff.fsplit.clear();
    refresh_list(false);
    return true;
}

extern "C"
{
    int isotoxin_compare( const unsigned char *row_text, int row_text_len, const unsigned char *pattern );
};

void gui_filterbar_c::full_search_result( found_stuff_s::FOUND_STUFF_T &&stuff )
{
    found_stuff.items = std::move( stuff );

    // try find in no-keep historians

    compare_context_s cc;
    cc.fsplit = found_stuff.fsplit;
    ts::str_c ccs;
    ccs.append_char('%');
    ccs.encode_pointer( &cc );

    auto get_post_time = []( const post_s *post )
    {
        return post ? post->recv_time : 0;
    };

    contacts().iterate_root_contacts( [&]( contact_root_c *r )->bool
    {
        if (!r->keep_history())
        {
            found_item_s *hitm = nullptr;

            time_t prevtime = 0;
            bool need_resort = false;

            r->iterate_history( [&]( post_s &p )->bool {

                if (isotoxin_compare( (const unsigned char *)p.message_utf8->cstr().s, p.message_utf8->getlen(), (const unsigned char *)ccs.cstr() ))
                {
                    if (!hitm)
                        for ( found_item_s &itm : found_stuff.items )
                            if ( itm.historian == r->getkey() )
                            {
                                hitm = &itm;
                                prevtime = itm.utags.count() ? get_post_time(r->find_post_by_utag( itm.utags.get( itm.utags.count() - 1 ) )) : 0;
                            }

                    if ( !hitm )
                    {
                        hitm = &found_stuff.items.add();
                        hitm->historian = r->getkey();
                    }

                    if ( hitm->mintime == 0 )
                        hitm->mintime = p.recv_time;

                    if ( hitm->utags.find_index( p.utag ) < 0 )
                        hitm->utags.add( p.utag );

                    if ( prevtime > p.recv_time )
                        need_resort = true;

                    prevtime = p.recv_time;
                }
                return false;
            } );

            if (need_resort && hitm)
            {
                hitm->utags.q_sort<uint64>( [&]( const uint64 *a, const uint64 *b )->bool
                {
                    if ( *a == *b )
                        return false;
                    return get_post_time( r->find_post_by_utag( *a ) ) < get_post_time( r->find_post_by_utag( *b ) );
                } );
            }

        }
        return true;
    } );

    if (contact_index >= contacts().count())
        apply_full_text_search_result();
}

bool gui_filterbar_c::do_contact_check(RID, GUIPARAM p)
{
    for ( ts::aint n = ts::tmax(1, contacts().count() / 10 ); contact_index < contacts().count() && n > 0; --n)
    {
        contact_c &c = contacts().get(contact_index++);
        if (c.is_rootcontact() && !c.getkey().is_self)
        {
            contact_root_c *cr = ts::ptr_cast<contact_root_c *>(&c);

            if (cr->is_full_search_result())
            {
                cr->full_search_result(false);
                if (cr->gui_item) cr->gui_item->update_text();
            }

            if ( cr->gui_item )
            {
                ASSERT( cr->gui_item->getrole() != CIR_ME );
                cr->gui_item->vis_filter( check_one( cr ) );
            }
        }
    }

    if (contact_index < contacts().count())
    {
        if (active)
        {
            gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
            cl.scroll_to_child( active, ST_ANY_POS );
        }
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, do_contact_check), 0);
    }
    else
    {
        apply_full_text_search_result();
    }

    return true;
}

void gui_filterbar_c::apply_full_text_search_result()
{
    for (found_item_s &itm : found_stuff.items)
        if (contact_root_c *c = contacts().rfind(itm.historian))
        {
            c->full_search_result(true);
            if (c->gui_item)
            {
                c->gui_item->update_text();
                c->gui_item->vis_filter(true);
            }
        }

    if (active)
    {
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        cl.fix_sep_visibility();
        cl.scroll_to_child(active, ST_ANY_POS);
    }

    gmsg<ISOGM_REFRESH_SEARCH_RESULT>().send();


}

void gui_filterbar_c::refresh_list( bool tags_ch )
{
    ts::wstr_c t;
    if (edit)
        t = edit->get_text();

    tagschanged |= tags_ch;

    update_filter(t, true);
}

bool gui_filterbar_c::update_filter(const ts::wstr_c & e, bool)
{
    ts::wstrings_c ospl( found_stuff.fsplit );
    found_stuff.fsplit.split<ts::wchar>(e, ' ');
    found_stuff.fsplit.trim();
    found_stuff.fsplit.kill_empty_fast();
    found_stuff.fsplit.case_down();

    // kill dups
    for( ts::aint i = found_stuff.fsplit.size() - 1;i>=0;--i)
    {
        for( ts::aint j = i-1;j>=0;--j)
            if ( found_stuff.fsplit.get(j).equals( found_stuff.fsplit.get(i) ) )
            {
                found_stuff.fsplit.remove_fast(i);
                break;
            }
    }

    // sort by length
    found_stuff.fsplit.sort([](const ts::wstr_c &s1,const ts::wstr_c &s2)->bool { return s1.get_length() == s2.get_length() ? (ts::wstr_c::compare(s1,s2) > 0) : s1.get_length() > s2.get_length(); });

    if (!tagschanged && found_stuff.fsplit == ospl)
        return true;

    process_animation_s *oldpa = nullptr;
    if (current_search)
    {
        oldpa = &current_search->pa;
        current_search->no_need = true;
        current_search = nullptr;
    }

    show_options(0 != found_stuff.fsplit.size());

    if (!tagschanged && is_all())
    {
        found_stuff.items.clear();
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        cl.on_filter_deactivate(RID(),nullptr);
        return true;
    }

    bool refresh_height = false;
    if (search_in_messages && found_stuff.fsplit.size())
    {
        current_search = TSNEW( full_search_s, this, found_stuff.fsplit );
        if ( oldpa )
            current_search->pa = std::move( *oldpa );
        found_stuff.items.clear();
        g_app->add_task( current_search );
        refresh_height = true;
    } else
        found_stuff.items.clear();


    active = g_app->active_contact_item ? &g_app->active_contact_item->getengine() : nullptr;

    if (!active)
    {
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        active = cl.get_first_contact_item();
    }

    tagschanged = false;
    contact_index = 0;
    do_contact_check(RID(),nullptr);
    if ( refresh_height )
        HOLD( getparent() ).as<gui_contactlist_c>().update_filter_pos();
    return true;
}

void gui_filterbar_c::redraw_anm()
{
    if ( current_search )
    {
        const theme_rect_s *thr = themerect();
        int y = getprops().size().y - current_search->pa.bmp.info().sz.y - 5;
        if ( thr )
            y -= thr->clborder_y()/2;
        int w = current_search->pa.bmp.info().sz.x;
        ts::irect r = ts::irect::from_lt_and_size( ts::ivec2((getprops().size().x-w)/2, y), current_search->pa.bmp.info().sz );
        getengine().redraw( &r );
    }
}

/*virtual*/ int gui_filterbar_c::get_height_by_width(int width) const
{
    ts::ivec2 sz(3);
    const theme_rect_s *thr = themerect();
    if (!textrect.get_text().is_empty())
    {
        sz = textrect.calc_text_size(width - 10 - (thr ? thr->clborder_x() : 0), custom_tag_parser_delegate());
        sz.y += 3;
    }

    if (thr) sz.y += thr->clborder_y();

    if (edit)
        sz.y += edit->getprops().size().y;
    if (option1)
        sz.y += option1->get_min_size().y;

    return sz.y + ( current_search ? current_search->pa.bmp.info().sz.y + 10 : 0 );
}

/*virtual*/ bool gui_filterbar_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    MEMT( MEMT_FILTERBAR );


    switch(qp)
    {
    case SQ_RECT_CHANGED:
        {
            ts::irect cla = get_client_area();
            fake_margin.x = 5;
            fake_margin.y = 0;
            if (prf().get_options().is(UIOPT_TAGFILETR_BAR))
            {
                if (!textrect.get_text().is_empty())
                {
                    ts::ivec2 sz = textrect.calc_text_size(cla.width() - 10, custom_tag_parser_delegate());
                    sz.y += 3;
                    textrect.set_size(sz);
                }
            }
            if (edit)
            {
                MODIFY(*edit).pos(cla.lt).size(cla.width(), filtereditheight);
                fake_margin.y += filtereditheight;

            }
            if (option1)
            {
                int omy = option1->get_min_size().y;
                MODIFY( *option1 ).pos( cla.lt + ts::ivec2(0,filtereditheight) ).size( cla.width(), omy );
                fake_margin.y += omy;
            }
        }
        return true;
    case SQ_MOUSE_LUP:
        if (overlink == clicklink && clicklink >= 0)
            do_tag_click(clicklink);
        clicklink = -1;
        break;
    case SQ_MOUSE_LDOWN:
        if (overlink >= 0)
            clicklink = overlink;
        break;
    case SQ_MOUSE_RUP:
        if (overlink == rclicklink && rclicklink >= 0)
            do_tag_rclick(rclicklink);
        rclicklink = -1;
        break;
    case SQ_MOUSE_RDOWN:
        if (overlink >= 0)
            rclicklink = overlink;
        break;
    case SQ_DRAW:
        if (rid == getrid())
        {
            gui_control_c::sq_evt( qp, rid, data );

            if (prf().get_options().is(UIOPT_TAGFILETR_BAR))
            {
                ts::irect ca = get_client_area();
                draw_data_s &dd = getengine().begin_draw();

                dd.offset += ca.lt; dd.offset.x += 5;
                dd.size = ca.size();
                dd.size.x -= 10;

                if (edit)
                    dd.offset.y += edit->getprops().size().y;
                if (option1)
                    dd.offset.y += option1->get_min_size().y;

                text_draw_params_s tdp;
                tdp.rectupdate = updaterect;
                draw(dd, tdp);
                getengine().end_draw();

            }
            if ( current_search )
            {
                /*draw_data_s &dd =*/ getengine().begin_draw();
                const theme_rect_s *thr = themerect();
                int y = getprops().size().y - current_search->pa.bmp.info().sz.y - 5;
                if ( thr )
                    y -= thr->clborder_y() / 2;
                int w = current_search->pa.bmp.info().sz.x;
                getengine().draw( ts::ivec2( ( getprops().size().x - w ) / 2, y ), current_search->pa.bmp.extbody(), true );
                getengine().end_draw();
            }
            return true;
        }
        break;
    }

    return __super::sq_evt(qp,rid,data);
}

void gui_filterbar_c::focus_edit()
{
    if (edit)
        gui->set_focus( edit->getrid() );
}

ts::uint32 gui_filterbar_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    if (ch.pass == 0 && CFG_LANGUAGE == ch.sp)
    {
        if (option1)
        {
            show_options(false);
            show_options(true);
        }
        getengine().redraw();
    }

    return 0;
}

gui_filterbar_c::full_search_s::full_search_s( gui_filterbar_c *owner, const ts::wstrings_c &flt_ ) : db( db ), owner( owner )
{
    db = prf().get_db();

    for ( const ts::wstr_c &s : flt_ )
        cc.fsplit.add( s.as_sptr() );

    DEFERRED_UNIQUE_CALL( 0.05, DELEGATE(this, anm), 0 );
}

gui_filterbar_c::full_search_s::~full_search_s()
{
    if ( gui )
        gui->delete_event( DELEGATE( this, anm ) );
}

bool gui_filterbar_c::full_search_s::anm( RID, GUIPARAM )
{
    if ( !no_need && owner )
    {
        pa.render();
        owner->redraw_anm();
        DEFERRED_UNIQUE_CALL( 0.05, DELEGATE( this, anm ), nullptr );
    }
    return true;
}


bool gui_filterbar_c::full_search_s::reader(int row, ts::SQLITE_DATAGETTER getta)
{
    if (no_need)
        return false;

    ts::data_value_s v;

    if (CHECK(ts::data_type_e::t_int == getta(history_s::C_ID, v))) // always id
    {
        if (CHECK(ts::data_type_e::t_int == getta(history_s::C_HISTORIAN, v)))
        {
            found_item_s &itm = add( contact_key_s::buildfromdbvalue(v.i, true) );

            getta(history_s::C_UTAG, v);
            if (ASSERT( itm.utags.find_index((uint64)v.i) < 0 ))
                itm.utags.add( v.i );

            if (itm.mintime == 0)
            {
                getta(history_s::C_RECV_TIME, v);
                itm.mintime = v.i;
            } else
            {
#ifdef _DEBUG
                getta(history_s::C_RECV_TIME, v);
                ASSERT( itm.mintime <= v.i );
#endif
            }

        }
    }
    return true;
}

/*virtual*/ int gui_filterbar_c::full_search_s::iterate()
{
    // mtype 0 == MTA_MESSAGE
    // mtype 1 == MTA_SYSTEM_MESSAGE
    // mtype 107 == MTA_UNDELIVERED_MESSAGE
    // mtype 110 == MTA_HIGHLIGHT_MESSAGE

    // see is_message_mt()

    cc.wstr.set_size( 4096 ); // most messages have smaller size

    ts::str_c where(CONSTASTR("(mtype == 0 or mtype == 1 or mtype == 107 or mtype == 110) and msg like \"%" ) );
    where.encode_pointer( &cc ).append( CONSTASTR( "\" order by mtime" ) );
    db->read_table(CONSTASTR("history"), DELEGATE(this, reader), where);
    return R_DONE;
}
/*virtual*/ void gui_filterbar_c::full_search_s::done(bool canceled)
{
    if (!no_need && owner)
        owner->full_search_result( std::move(found_stuff) );

    if ( owner && owner->current_search == this )
    {
        owner->current_search = nullptr;
        HOLD( owner->getparent() ).as<gui_contactlist_c>().update_filter_pos();
    }
        
    __super::done(canceled);
}

