#include "isotoxin.h"

MAKE_CHILD<gui_filterbar_c>::~MAKE_CHILD()
{
    //MODIFY(get()).visible(true);
}


gui_filterbar_c::gui_filterbar_c(MAKE_CHILD<gui_filterbar_c> &data):gui_label_ex_c(data)
{
    g_app->found_items = &found_stuff;
    //set_text(L"test test test test 111 klasdf lksd fjklasdfj wepo pweo powe opew oprweop rwepor powe");
}
gui_filterbar_c::~gui_filterbar_c()
{
    g_app->found_items = nullptr;
}

/*virtual*/ void gui_filterbar_c::created()
{
    set_theme_rect(CONSTASTR("filter"), false);

    filtereditheight = gui->theme().conf().get_int(CONSTASTR("filtereditheight"), 25);

    gui_textfield_c &e = (MAKE_CHILD<gui_textfield_c>(getrid(), L"", MAX_PATH, 0, false) << (gui_textedit_c::TEXTCHECKFUNC)DELEGATE(this,update_filter));
    edit = &e;
    e.set_placeholder( TOOLTIP(TTT("Search",277)), get_default_text_color(0) );
    e.register_kbd_callback(DELEGATE( this, cancel_filter ), SSK_ESC, false);

    search_in_messages = prf().is_loaded() && prf().get_options().is(MSGOP_FULL_SEARCH);

    __super::created();
}

bool gui_filterbar_c::cancel_filter(RID, GUIPARAM)
{
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
    update_filter(edit->get_text());
    return true;
}

void gui_filterbar_c::full_search_result( found_stuff_s::FOUND_STUFF_T &&stuff )
{
    found_stuff.items = std::move( stuff );
    if (contact_index >= contacts().count())
        apply_full_text_search_result();
}

bool gui_filterbar_c::do_contact_check(RID, GUIPARAM p)
{
    for (int n = ts::tmax(1, contacts().count() / 10 ); contact_index < contacts().count() && n > 0; --n)
    {
        contact_c &c = contacts().get(contact_index++);
        if (c.is_full_search_result())
        {
            c.full_search_result(false);
            if (c.gui_item) c.gui_item->update_text();
        }
        if (c.is_meta())
        {
            if (c.gui_item)
            {
                bool match = true;
                if (found_stuff.fsplit.size())
                {
                    ts::str_c an = c.get_customname();
                    if (an.is_empty())
                        an = c.get_name();

                    const ts::wstr_c wn = from_utf8(an).case_down();
                    for (const ts::wstr_c &f : found_stuff.fsplit)
                        if (wn.find_pos(f) < 0)
                        {
                            match = false;
                            break;
                        }
                }
                MODIFY(*c.gui_item).visible(match);
            }
        }
    }

    if (contact_index < contacts().count())
    {
        if (active)
        {
            gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
            cl.scroll_to_child(active, false);
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
        if (contact_c *c = contacts().find(itm.historian))
        {
            c->full_search_result(true);
            if (c->gui_item)
            {
                c->gui_item->update_text();
                MODIFY(*c->gui_item).visible(true);
            }
        }

    if (active)
    {
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        cl.scroll_to_child(active, false);
    }

    gmsg<ISOGM_REFRESH_SEARCH_RESULT>().send();


}

bool gui_filterbar_c::update_filter(const ts::wstr_c & e)
{
    ts::wstrings_c ospl( found_stuff.fsplit );
    found_stuff.fsplit.split<ts::wchar>(e, ' ');
    found_stuff.fsplit.trim();
    found_stuff.fsplit.kill_empty_fast();
    found_stuff.fsplit.case_down();

    // kill dups
    for(int i = found_stuff.fsplit.size() - 1;i>=0;--i)
    {
        for(int j = i-1;j>=0;--j)
            if ( found_stuff.fsplit.get(j).equals( found_stuff.fsplit.get(i) ) )
            {
                found_stuff.fsplit.remove_fast(i);
                break;
            }
    }

    // sort by length
    found_stuff.fsplit.sort([](const ts::wstr_c &s1,const ts::wstr_c &s2)->bool { return s1.get_length() == s2.get_length() ? (ts::wstr_c::compare(s1,s2) > 0) : s1.get_length() > s2.get_length(); });

    if (found_stuff.fsplit == ospl)
        return true;

    if (current_search)
    {
        current_search->no_need = true;
        current_search = nullptr;
    }

    show_options(0 != found_stuff.fsplit.size());

    if (!found_stuff.fsplit.size())
    {
        found_stuff.items.clear();
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        cl.on_filter_deactivate(RID(),nullptr);
        return true;
    }

    if (search_in_messages && found_stuff.fsplit.size())
    {
        current_search = TSNEW( full_search_s, prf().get_db(), this, found_stuff.fsplit );
        found_stuff.items.clear();
        g_app->add_task( current_search );
    } else
        found_stuff.items.clear();


    active = g_app->active_contact_item ? &g_app->active_contact_item->getengine() : nullptr;

    if (!active)
    {
        gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
        active = cl.get_first_contact_item();
    }

    contact_index = 0;
    do_contact_check(RID(),nullptr);
    return true;
}

/*virtual*/ int gui_filterbar_c::get_height_by_width(int width) const
{
    ts::ivec2 sz(3);
    const theme_rect_s *thr = themerect();
    if (!textrect.get_text().is_empty())
        sz = textrect.calc_text_size(width - (thr ? thr->clborder_x() : 0), custom_tag_parser_delegate());
    sz.y += edit->getprops().size().y;
    if (thr) sz.y += thr->clborder_y();

    if (option1)
    {
        sz.y += option1->get_min_size().y;
    }

    return sz.y;
}

/*virtual*/ bool gui_filterbar_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    switch(qp)
    {
    case SQ_RECT_CHANGED:
        {
            ts::irect cla = get_client_area();
            MODIFY( *edit ).pos( cla.lt ).size( cla.width(), filtereditheight );
            if (option1)
            {
                MODIFY( *option1 ).pos( cla.lt + ts::ivec2(0,filtereditheight) ).size( cla.width(), option1->get_min_size().y );
            }
        }
        break;
    case SQ_DRAW:
        if (rid == getrid())
        {
            /*
            ts::irect ca = get_client_area();
            draw_data_s &dd = getengine().begin_draw();

            dd.offset += ca.lt;
            dd.size = ca.size();

            text_draw_params_s tdp;
            tdp.rectupdate = updaterect;
            draw(dd, tdp);
            getengine().end_draw();
            return gui_control_c::sq_evt(qp,rid,data);
            */

            return gui_control_c::sq_evt(qp,rid,data);

            //return __super::sq_evt(qp,rid,data);
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


bool gui_filterbar_c::full_search_s::reader(int row, ts::SQLITE_DATAGETTER getta)
{
    if (no_need)
        return false;

    ts::data_value_s v;
    if (CHECK(ts::data_type_e::t_int == getta(history_s::C_ID, v))) // always id
    {
        if (CHECK(ts::data_type_e::t_int == getta(history_s::C_HISTORIAN, v)))
        {
            found_item_s &itm = add( ts::ref_cast<contact_key_s>(v.i) );

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

/*virtual*/ int gui_filterbar_c::full_search_s::iterate(int pass)
{
    // mtype 0 == MTA_MESSAGE
    // mtype 107 == MTA_UNDELIVERED_MESSAGE

    ts::str_c where(CONSTASTR("(mtype == 0 or mtype == 107) and msg like \"%"));
    where.encode_pointer(&flt).append(CONSTASTR("\" order by mtime") );

    db->read_table(CONSTASTR("history"), DELEGATE(this, reader), where);

    return R_DONE;
}
/*virtual*/ void gui_filterbar_c::full_search_s::done(bool canceled)
{
    if (!no_need && owner)
        owner->full_search_result( std::move(found_stuff) );
    if (owner && owner->current_search == this)
        owner->current_search = nullptr;
        
    __super::done(canceled);
}

