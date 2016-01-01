#include "isotoxin.h"


dialog_metacontact_c::dialog_metacontact_c(MAKE_ROOT<dialog_metacontact_c> &data) :gui_isodialog_c(data)
{
    deftitle = title_new_meta_contact;
    clist.add() = data.prms.key;
}

dialog_metacontact_c::~dialog_metacontact_c()
{
    //if (gui)
    //    gui->delete_event(DELEGATE(this, hidectl));
}

/*virtual*/ void dialog_metacontact_c::created()
{
    set_theme_rect(CONSTASTR("main"), false);
    __super::created();
    tabsel(CONSTASTR("1"));

}

/*virtual*/ ts::ivec2 dialog_metacontact_c::get_min_size() const
{
    return ts::ivec2(510, 460);
}

void dialog_metacontact_c::getbutton(bcreate_s &bcr)
{
    __super::getbutton(bcr);
    if (bcr.tag == 1)
    {
        bcr.btext = TTT("Create metacontact",149);
    }
}

/*virtual*/ int dialog_metacontact_c::additions(ts::irect &)
{
    descmaker dm(descs);
    dm << 1;

    dm().page_header(TTT("Metacontact - union of some contacts with common message history.[br][l]Drag and drop addition contacts to this list.[/l]",147));
    dm().vspace(300, DELEGATE(this,makelist));

    return 0;
}

bool dialog_metacontact_c::makelist(RID stub, GUIPARAM)
{
    gui_contactlist_c& cl_ = MAKE_CHILD<gui_contactlist_c>( stub, CLR_NEW_METACONTACT_LIST );
    cl = &cl_;
    cl_.leech( TSNEW(leech_fill_parent_s) );
    cl_.array_mode( clist );
    return true;
}

ts::uint32 dialog_metacontact_c::gm_handler(gmsg<GM_DRAGNDROP> & dnda)
{
    if (dnda.a == DNDA_CLEAN)
    {
        return 0;
    }

    gui_contact_item_c *ciproc = dynamic_cast<gui_contact_item_c *>(gui->dragndrop_underproc());
    if (!ciproc) return 0;
    if (dnda.a == DNDA_DROP)
    {
        if (clist.find(ciproc->getcontact().getkey()) < 0)
            clist.add(ciproc->getcontact().getkey());
        if (cl)
            cl->refresh_array();

        return 0;
    }

    ts::irect rect = gui->dragndrop_objrect();
    int carea = getprops().screenrect().intersect_area( rect );
    bool dndtgt = carea > rect.area()/2;

    return dndtgt ? GMRBIT_ACCEPTED : 0;
}

ts::uint32 dialog_metacontact_c::gm_handler( gmsg<ISOGM_METACREATE> & mca )
{
    switch (mca.state)
    {
    case gmsg<ISOGM_METACREATE>::ADD:
        {
            if (clist.find(mca.ck) < 0)
                clist.add(mca.ck);
            if (cl)
                cl->refresh_array();
            mca.state = gmsg<ISOGM_METACREATE>::ADDED;
            break;
        }
    case gmsg<ISOGM_METACREATE>::REMOVE:
        {
            if (cl && clist.find_remove_slow(mca.ck) >= 0)
                cl->refresh_array();
            break;
        }
    case gmsg<ISOGM_METACREATE>::MAKEFIRST:
        {
            if (cl && clist.find_remove_slow(mca.ck) >= 0)
            {
                clist.insert(0, mca.ck);
                cl->refresh_array();
            }
            break;
        }
    case gmsg<ISOGM_METACREATE>::CHECKINLIST:
        if (cl && clist.find(mca.ck) >= 0)
            return GMRBIT_ACCEPTED | GMRBIT_ABORT;
        return 0;
    }

    return 0;
}

/*virtual*/ bool dialog_metacontact_c::sq_evt(system_query_e qp, RID rid, evt_data_s &data)
{
    if (__super::sq_evt(qp, rid, data)) return true;

    //switch (qp)
    //{
    //case SQ_DRAW:
    //    if (const theme_rect_s *tr = themerect())
    //        draw(*tr);
    //    break;
    //}

    return false;
}

/*virtual*/ void dialog_metacontact_c::on_confirm()
{
    if (clist.size() < 2)
    {
        dialog_msgbox_c::mb_error( TTT("Metacontact - union of two or more contacts. The current list contains less then two contacts and to create metacontact this amount is not enough.",150) ).summon();
        return;
    }

    for (const contact_key_s &ck : clist)
        if (contact_c * c = contacts().find(ck))
            prf().load_history( c->get_historian()->getkey() );

    contact_c *basec = contacts().find(clist.get_remove_slow());
    if (CHECK(basec))
    {
        ts::db_transaction_c __transaction( prf().get_db() );

        basec->unload_history();
        for (const contact_key_s &ck : clist)
            if (contact_c * c = contacts().find(ck))
            {
                ASSERT(c->is_meta());
                c->unload_history();
                prf().merge_history( basec->getkey(), ck );
                c->stop_av();
                c->subiterate([&](contact_c *cc) {
                    basec->subadd(cc);
                    prf().dirtycontact( cc->getkey() );
                });
                c->subclear();
                contacts().kill(ck);
            }
        prf().flush_history_now();
    }
    basec->subiterate([&](contact_c *cc) {
        if ( cc->get_options().is(contact_c::F_DEFALUT) )
        {
            cc->options().clear(contact_c::F_DEFALUT);
            prf().dirtycontact(cc->getkey());
        }
    });
    prf().dirtycontact( basec->getkey() );
    if (basec->gui_item)
    {
        basec->gui_item->protohit();
        basec->gui_item->update_text();
    }
    basec->reselect();
    __super::on_confirm();
}

