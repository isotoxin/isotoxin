#include "isotoxin.h"

MAKE_CHILD<gui_filterbar_c>::~MAKE_CHILD()
{
    //MODIFY(get()).visible(true);
}


gui_filterbar_c::gui_filterbar_c(MAKE_CHILD<gui_filterbar_c> &data):gui_label_ex_c(data)
{
    //set_text(L"test test test test 111 klasdf lksd fjklasdfj wepo pweo powe opew oprweop rwepor powe");
}
gui_filterbar_c::~gui_filterbar_c()
{
}


/*virtual*/ void gui_filterbar_c::created()
{
    set_theme_rect(CONSTASTR("filter"), false);

    filtereditheight = gui->theme().conf().get_int(CONSTASTR("filtereditheight"), 25);

    gui_textfield_c &e = (MAKE_CHILD<gui_textfield_c>(getrid(), L"", MAX_PATH, 0, false) << (gui_textedit_c::TEXTCHECKFUNC)DELEGATE(this,update_filter));
    edit = &e;
    e.set_placeholder( TOOLTIP(TTT("Search",275)), get_default_text_color(0) );

    __super::created();
}

bool gui_filterbar_c::do_contact_check(RID, GUIPARAM p)
{
    for (int n = ts::tmax(1, contacts().count() / 10 ); contact_index < contacts().count() && n > 0; --n)
    {
        contact_c &c = contacts().get(contact_index++);
        if (c.is_meta())
        {
            if (c.gui_item)
            {
                bool match = true;
                if (fsplit.size())
                {
                    ts::str_c an = c.get_customname();
                    if (an.is_empty())
                        an = c.get_name();

                    const ts::wstr_c n = from_utf8(an).case_down();
                    for (const ts::wstr_c &f : fsplit)
                        if (n.find_pos(f) < 0)
                        {
                            match = false;
                            break;
                        }
                }
                MODIFY(*c.gui_item).visible(match);
            }
        }
    }

    gui_contactlist_c &cl = HOLD(getparent()).as<gui_contactlist_c>();
    if (contact_index < contacts().count())
    {
        if (active)
            cl.scroll_to(active, false);
        DEFERRED_UNIQUE_CALL(0, DELEGATE(this, do_contact_check), 0);
    }
    else if (active)
    {
        cl.scroll_to(active, false);
    }

    return true;
}

bool gui_filterbar_c::update_filter(const ts::wstr_c & e)
{
    fsplit.split<ts::wchar>(e, ' ');
    fsplit.trim();
    fsplit.kill_empty_fast();
    fsplit.case_down();

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
        }
        break;
    case SQ_DRAW:
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
        getengine().redraw();

    return 0;
}