#include "isotoxin.h"

#define minimum_encrypt_pb_duration 1.0f

ts::static_setup<profile_c> prf;

void active_protocol_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case 1:
            tag = v.text;
            return;
        case 2:
            name = v.text;
            return;
        case 3:
            user_name = v.text;
            return;
        case 4:
            user_statusmsg = v.text;
            return;
        case 5:
            config = v.blob;
            return;
        case 6:
            options = (int)v.i;
            return;
        case 7:
            avatar = v.blob;
            return;
        case 8:
            sort_factor = (int)v.i;
            return;
    }
}

void active_protocol_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
        case 1:
            v.text = tag;
            return;
        case 2:
            v.text = name;
            return;
        case 3:
            v.text = user_name;
            return;
        case 4:
            v.text = user_statusmsg;
            return;
        case 5:
            v.blob = config;
            return;
        case 6:
            v.i = options;
            return;
        case 7:
            v.blob = avatar;
            return;
        case 8:
            v.i = sort_factor;
            return;
    }
}

ts::data_type_e active_protocol_s::get_column_type(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 3:
        case 4:
            return ts::data_type_e::t_str;
        case 5:
        case 7:
            return ts::data_type_e::t_blob;
        case 6:
        case 8:
            return ts::data_type_e::t_int;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void active_protocol_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch(index)
    {
        case 1:
            cd.name_ = CONSTASTR("tag");
            break;
        case 2:
            cd.name_ = CONSTASTR("name");
            break;
        case 3:
            cd.name_ = CONSTASTR("uname");
            break;
        case 4:
            cd.name_ = CONSTASTR("ustatus");
            break;
        case 5:
            cd.name_ = CONSTASTR("conf");
            break;
        case 6:
            cd.name_ = CONSTASTR("options");
            cd.default_ = CONSTASTR("1"); // O_AUTOCONNECT
            break;
        case 7:
            cd.name_ = CONSTASTR("avatar");
            break;
        case 8:
            cd.name_ = CONSTASTR("sortfactor");
            break;
        default:
            FORBIDDEN();
    }
}

/////// backup protocols

void backup_protocol_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case 1:
            time = v.i;
            return;
        case 2:
            tick = (int)v.i;
            return;
        case 3:
            protoid = (int)v.i;
            return;
        case 4:
            config = v.blob;
            return;
    }
}

void backup_protocol_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
        case 1:
            v.i = time;
            return;
        case 2:
            v.i = tick;
            return;
        case 3:
            v.i = protoid;
            return;
        case 4:
            v.blob = config;
            return;
    }
}

ts::data_type_e backup_protocol_s::get_column_type(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 3:
            return ts::data_type_e::t_int;
        case 4:
            return ts::data_type_e::t_blob;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void backup_protocol_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch (index)
    {
        case 1:
            cd.name_ = CONSTASTR("time");
            cd.options.set( ts::column_desc_s::f_non_unique_index );
            break;
        case 2:
            cd.name_ = CONSTASTR("tick");
            break;
        case 3:
            cd.name_ = CONSTASTR("protoid");
            break;
        case 4:
            cd.name_ = CONSTASTR("conf");
            break;
        default:
            FORBIDDEN();
    }
}

/////// contacts

void contacts_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case C_CONTACT_ID:
            key.contactid = (int)v.i;
            return;
        case C_PROTO_ID:
            key.protoid = (int)v.i;
            return;
        case C_META_ID:
            metaid = (int)v.i;
            return;
        case C_OPTIONS:
            options = (int)v.i;
            return;
        case C_NAME:
            name = v.text;
            return;
        case C_STATUSMSG:
            statusmsg = v.text;
            return;
        case C_READTIME:
            readtime = v.i;
            return;
        case C_CUSTOMNAME:
            customname = v.text;
            return;
        case C_COMMENT:
            comment = v.text;
            return;
        case C_TAGS:
            tags.split<char>( v.text, ',' );
            return;
        case C_MESSAGEHANDLER:
            msghandler = v.text;
            return;
        case C_AVATAR:
            avatar = v.blob;
            return;
        case C_AVATAR_TAG:
            avatar_tag = (int)v.i;
            return;
    }
}

void contacts_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
        case C_CONTACT_ID:
            v.i = key.contactid;
            return;
        case C_PROTO_ID:
            v.i = key.protoid;
            return;
        case C_META_ID:
            v.i = metaid;
            return;
        case C_OPTIONS:
            v.i = options;
            return;
        case C_NAME:
            v.text = name;
            return;
        case C_STATUSMSG:
            v.text = statusmsg;
            return;
        case C_READTIME:
            v.i = readtime;
            return;
        case C_CUSTOMNAME:
            v.text = customname;
            return;
        case C_COMMENT:
            v.text = comment;
            return;
        case C_TAGS:
            v.text = tags.join(',');
            return;
        case C_MESSAGEHANDLER:
            v.text = msghandler;
            return;
        case C_AVATAR:
            v.blob = avatar;
            return;
        case C_AVATAR_TAG:
            v.i = avatar_tag;
            return;
    }
}

ts::data_type_e contacts_s::get_column_type(int index)
{
    switch (index)
    {
        case C_CONTACT_ID:
        case C_PROTO_ID:
        case C_META_ID:
        case C_OPTIONS:
        case C_AVATAR_TAG:
            return ts::data_type_e::t_int;
        case C_NAME:
        case C_STATUSMSG:
        case C_CUSTOMNAME:
        case C_COMMENT:
        case C_TAGS:
        case C_MESSAGEHANDLER:
            return ts::data_type_e::t_str;
        case C_READTIME:
            return ts::data_type_e::t_int64;
        case C_AVATAR:
            return ts::data_type_e::t_blob;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void contacts_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch (index)
    {
        case C_CONTACT_ID:
            cd.name_ = CONSTASTR("contact_id");
            break;
        case C_PROTO_ID:
            cd.name_ = CONSTASTR("proto_id");
            break;
        case C_META_ID:
            cd.name_ = CONSTASTR("meta_id");
            break;
        case C_OPTIONS:
            cd.name_ = CONSTASTR("options");
            break;
        case C_NAME:
            cd.name_ = CONSTASTR("name");
            break;
        case C_STATUSMSG:
            cd.name_ = CONSTASTR("statusmsg");
            break;
        case C_READTIME:
            cd.name_ = CONSTASTR("readtime");
            break;
        case C_CUSTOMNAME:
            cd.name_ = CONSTASTR("customname");
            break;
        case C_COMMENT:
            cd.name_ = CONSTASTR("comment");
            break;
        case C_TAGS:
            cd.name_ = CONSTASTR("tags");
            break;
        case C_MESSAGEHANDLER:
            cd.name_ = CONSTASTR( "msghandler" );
            break;
        case C_AVATAR:
            cd.name_ = CONSTASTR("avatar");
            break;
        case C_AVATAR_TAG:
            cd.name_ = CONSTASTR("avatar_tag");
            break;
        default:
            FORBIDDEN();
    }
}


/////// history

void history_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case C_RECV_TIME:
            recv_time = v.i;
            return;
        case C_CR_TIME:
            cr_time = v.i;
            return;
        case C_HISTORIAN:
            historian = ts::ref_cast<contact_key_s>(v.i);
            return;
        case C_SENDER:
            sender = ts::ref_cast<contact_key_s>(v.i);
            return;
        case C_RECEIVER:
            receiver = ts::ref_cast<contact_key_s>(v.i);
            return;
        case C_TYPE_AND_OPTIONS:
            type = v.i & (SETBIT(type_size_bits)-1);
            options = (v.i >> 16) & (SETBIT(options_size_bits)-1);
            return;
        case C_MSG:
            message_utf8 = v.text;
            return;
        case C_UTAG:
            utag = v.i;
            return;
    }
}

void history_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
        case C_RECV_TIME:
            v.i = recv_time;
            return;
        case C_CR_TIME:
            v.i = cr_time;
            return;
        case C_HISTORIAN:
            v.i = ts::ref_cast<int64>(historian);
            return;
        case C_SENDER:
            v.i = ts::ref_cast<int64>( sender );
            return;
        case C_RECEIVER:
            v.i = ts::ref_cast<int64>( receiver );
            return;
        case C_TYPE_AND_OPTIONS:
            v.i = type;
            v.i |= ((int64)options) << 16;
            return;
        case C_MSG:
            v.text = message_utf8;
            return;
        case C_UTAG:
            v.i = utag;
            return;
    }
}

ts::data_type_e history_s::get_column_type(int index)
{
    switch (index)
    {
        case C_RECV_TIME:
        case C_CR_TIME:
        case C_HISTORIAN:
        case C_SENDER:
        case C_RECEIVER:
        case C_UTAG:
            return ts::data_type_e::t_int64;
        case C_TYPE_AND_OPTIONS:
            return ts::data_type_e::t_int;
        case C_MSG:
            return ts::data_type_e::t_str;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void history_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch (index)
    {
        case C_RECV_TIME:
            cd.name_ = CONSTASTR("mtime");
            cd.options.set( ts::column_desc_s::f_non_unique_index );
            break;
        case C_CR_TIME:
            cd.name_ = CONSTASTR("crtime");
            break;
        case C_HISTORIAN:
            cd.name_ = CONSTASTR("historian");
            break;
        case C_SENDER:
            cd.name_ = CONSTASTR("sender");
            break;
        case C_RECEIVER:
            cd.name_ = CONSTASTR("receiver");
            break;
        case C_TYPE_AND_OPTIONS:
            cd.name_ = CONSTASTR("mtype");
            break;
        case C_MSG:
            cd.name_ = CONSTASTR("msg");
            break;
        case C_UTAG:
            cd.name_ = CONSTASTR("utag");
            cd.options.set( ts::column_desc_s::f_unique_index );
            break;
        default:
            FORBIDDEN();
    }
}


/////// transfer file

void unfinished_file_transfer_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case 1:
            historian = ts::ref_cast<contact_key_s>(v.i);
            return;
        case 2:
            sender = ts::ref_cast<contact_key_s>(v.i);
            return;
        case 3:
            filename.set_as_utf8(v.text);
            return;
        case 4:
            filename_on_disk.set_as_utf8(v.text);
            return;
        case 5:
            filesize = v.i;
            return;
        case 6:
            utag = v.i;
            return;
        case 7:
            msgitem_utag = v.i;
            return;
        case 8:
            upload = v.i != 0;
            return;
    }
}

void unfinished_file_transfer_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
        case 1:
            v.i = ts::ref_cast<int64>(historian);
            return;
        case 2:
            v.i = ts::ref_cast<int64>(sender);
            return;
        case 3:
            v.text = to_utf8(filename);
            return;
        case 4:
            v.text = to_utf8(filename_on_disk);
            return;
        case 5:
            v.i = filesize;
            return;
        case 6:
            v.i = utag;
            return;
        case 7:
            v.i = msgitem_utag;
            return;
        case 8:
            v.i = upload ? 1 : 0;
            return;
    }
}

ts::data_type_e unfinished_file_transfer_s::get_column_type(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 5:
        case 6:
        case 7:
            return ts::data_type_e::t_int64;
        case 3:
        case 4:
            return ts::data_type_e::t_str;
        case 8:
            return ts::data_type_e::t_int;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void unfinished_file_transfer_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch (index)
    {
        case 1:
            cd.name_ = CONSTASTR("historian");
            break;
        case 2:
            cd.name_ = CONSTASTR("sender");
            break;
        case 3:
            cd.name_ = CONSTASTR("fn");
            break;
        case 4:
            cd.name_ = CONSTASTR("fnod");
            break;
        case 5:
            cd.name_ = CONSTASTR("sz");
            break;
        case 6:
            cd.name_ = CONSTASTR("utag");
            break;
        case 7:
            cd.name_ = CONSTASTR("guiutag");
            break;
        case 8:
            cd.name_ = CONSTASTR("upl");
            break;
        default:
            FORBIDDEN();
    }
}

// transfer file


template<typename T, profile_table_e tabi> bool tableview_t<T, tabi>::prepare( ts::sqlitedb_c *db )
{
    ts::tmp_array_inplace_t<ts::column_desc_s, 0> cds( T::columns );

    ts::column_desc_s &idc = cds.add();
    idc.name_ = CONSTASTR("id");
    idc.type_ = ts::data_type_e::t_int;
    idc.options.set( ts::column_desc_s::f_primary | ts::column_desc_s::f_autoincrement );

    for(int i=1;i<T::columns;++i)
        T::get_column_desc(i, cds.add());

    int rslt = db->update_table_struct(T::get_table_name(), cds.array(), false);
    if (rslt < 0) return false;
    if (rslt)
    {
        // ok
    }

    flush( db, true, false );
    return true;
}


template<typename T, profile_table_e tabi> bool tableview_t<T, tabi>::flush( ts::sqlitedb_c *db, bool all, bool notify_saved )
{
    ts::db_transaction_c __transaction( db );

    ts::tmp_array_inplace_t<ts::data_pair_s, 0> vals( T::columns );
    bool one_done = false;
    bool some_action = false;
    for(row_s &r: rows)
    {
        switch (r.st)
        {
            case row_s::s_new:
                if (one_done) return true;
            case row_s::s_changed:
                if (one_done) return true;
                if (ASSERT(r.id != 0))
                {
                    vals.clear();
                    if (r.id > 0)
                    {
                        ts::data_pair_s &dpair = vals.add();
                        dpair.type_ = ts::data_type_e::t_int;
                        dpair.name = CONSTASTR("id");
                        dpair.i = r.id;
                    }

                    for(int i=1;i<T::columns;++i)
                    {
                        ts::data_pair_s &dpair = vals.add();
                        r.other.get(i,dpair);
                    }
                    some_action = true;
                    int newid = db->insert(T::get_table_name(), vals.array());
                    if (r.id < 0)
                    {
                        __if_exists(T::maxid)
                        {
                            if (T::maxid > 0 && newid > T::maxid)
                            {
                                int other_newid = db->find_free(T::get_table_name(), CONSTASTR("id"));
                                ts::data_pair_s idp;
                                idp.name = CONSTASTR("id");
                                idp.type_ = ts::data_type_e::t_int;
                                idp.i = other_newid;
                                db->update(T::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(&idp, 1), ts::amake<uint>(CONSTASTR("id="), newid));
                                newid = other_newid;
                            }
                        }

                        new2ins[r.id] = newid;
                    }
                    r.id = newid;
                    r.st = row_s::s_unchanged;
                    one_done = !all;
                }
                continue;
            case row_s::s_delete:
                if (one_done) return true;
                some_action = true;
                db->delrow(T::get_table_name(), r.id);
                r.st = row_s::s_deleted;
                one_done = !all;
                cleanup_requred = true;
                continue;

        }
    }

    __transaction.end();

    if (notify_saved && some_action) gmsg<ISOGM_PROFILE_TABLE_SAVED>( tabi ).send();
    return false;
}

/*
template<typename T> typename tableview_t<T>::row_s tableview_t<T>::del(int id)
{
    int cnt = rows.size();
    for(int i=0;i<cnt;++i)
    {
        const row_s &r = rows.get(i);
        if (r.id == id)
            return rows.get_remove_slow(i);
    }
    return row_s();
}
*/

template<typename T, profile_table_e tabi> typename tableview_t<T, tabi>::row_s &tableview_t<T, tabi>::getcreate(int id)
{
    if (id)
        for(row_s &r : rows)
            if(r.id == id) return r;

    row_s &r = rows.add();
    if (id != 0)
    {
        // loading
        r.id = id;
        r.st = row_s::s_unchanged;
    } else
    {
        r.id = newidpool--;
    }
    return r;
}


template<typename T, profile_table_e tabi> bool tableview_t<T, tabi>::reader(int /*rowi*/, ts::SQLITE_DATAGETTER getta)
{
    ts::data_value_s v;
    if (CHECK(ts::data_type_e::t_int == getta(0, v))) // always id
    {
        if (read_ids) read_ids->add( (int)v.i );
        row_s &row = getcreate((int)v.i);
        if (!CHECK(row.st == row_s::s_unchanged)) return true;

        for(int i=1;i<T::columns;++i)
        {
            auto t = getta(i, v);
            if (CHECK(t == ts::data_type_e::t_null || same_sqlite_type(T::get_column_type(i),t)))
                row.other.set(i, v);
        }

        row.st = row_s::s_unchanged;
    }
    return true;
}

template<typename T, profile_table_e tabi> void tableview_t<T, tabi>::read( ts::sqlitedb_c *db )
{
    db->read_table( T::get_table_name(), DELEGATE(this, reader) );
    gmsg<ISOGM_PROFILE_TABLE_LOADED>( tabi ).send();
}

template<typename T, profile_table_e tabi> void tableview_t<T, tabi>::read( ts::sqlitedb_c *db, const ts::asptr &where_items )
{
    if (db)
        db->read_table(T::get_table_name(), DELEGATE(this, reader), where_items);
}

ts::wstr_c& profile_c::path_by_name(ts::wstr_c &profname)
{
    profname.append(CONSTWSTR(".profile"));
    profname = ts::fn_change_name_ext(cfg().get_path(), profname);
    return profname;
}

ts::uint32 profile_c::gm_handler(gmsg<ISOGM_MESSAGE>&msg) // record history
{
    if (msg.resend) return 0;
    if (msg.pass != 0) return 0;
    bool second_pass_requred = msg.post.recv_time == 1;
    contact_root_c *historian = msg.get_historian();
    if (historian == nullptr) return 0;
    //bool record_it = true;

    //if (record_it)
    {
        time_t nowt = historian->nowtime();
        time_t crt = msg.post.cr_time;
        if (crt == 0)
            crt = nowt;

        post_s &post = historian->add_history(nowt, crt);
        msg.post.recv_time = post.recv_time;
        msg.post.cr_time = post.cr_time;
        // [POST_INIT]
        post.sender = msg.post.sender;
        post.receiver = msg.post.receiver;
        post.type = msg.post.type;
        post.message_utf8 = msg.post.message_utf8;
        post.utag = msg.post.utag;
        post.options = 0;

        if (historian->keep_history())
            record_history(historian->getkey(), post);

        if (post.mt() == MTA_UNDELIVERED_MESSAGE)
            g_app->undelivered_message(post);
    }

    return second_pass_requred ? GMRBIT_CALLAGAIN : 0;
}

ts::uint32 profile_c::gm_handler(gmsg<ISOGM_CHANGED_SETTINGS>&ch)
{
    bool changed = false;
    if (ch.protoid)
        switch (ch.sp)
        {
        case PP_NETWORKNAME:
            if (auto *row = get_table_active_protocol().find<true>(ch.protoid))
                row->other.name = ch.s, row->changed(), changed = true;
            break;
        case PP_USERNAME:
            if (auto *row = get_table_active_protocol().find<true>(ch.protoid))
                row->other.user_name = ch.s, row->changed(), changed = true;
            break;
        case PP_USERSTATUSMSG:
            if (auto *row = get_table_active_protocol().find<true>(ch.protoid))
                row->other.user_statusmsg = ch.s, row->changed(), changed = true;
            break;
        }

    if (ch.pass == 0 && PP_ACTIVEPROTO_SORT == ch.sp)
    {
        for (const active_protocol_c *ap : protocols)
            if (ap && !ap->is_dip())
            {
                int sortfactor = ap->sort_factor();
                if (auto *row = get_table_active_protocol().find<true>(ap->getid()))
                    if (row->other.sort_factor != sortfactor)
                        row->other.sort_factor = sortfactor, row->changed(), changed = true;
            }
    }

    if (changed)
        this->changed();

    return 0;
}

uint64 profile_c::uniq_history_item_tag()
{
    uint64 utag = ts::uuid();

    while(db)
    {
        ts::tmp_str_c whr(CONSTASTR("utag=")); whr.append_as_num<int64>(utag);
        if (0 == db->count(CONSTASTR("history"), whr))
            break;

        utag = ts::uuid();
    }
    
    return utag;
}

void profile_c::record_history( const contact_key_s&historian, const post_s &history_item )
{
    ASSERT(history_item.recv_time != 0);
    g_app->lock_recalc_unread(historian);
    auto &row = table_history.getcreate(0);
    row.other.historian = historian;
    static_cast<post_s &>(row.other) = history_item;
    changed();
}


void profile_c::kill_history_item(uint64 utag)
{
    if(auto *row = table_history.find<true>([&](history_s &h) ->bool { return h.utag == utag; }))
        if (row->deleted())
            changed();

    ts::tmp_str_c whr(CONSTASTR("utag=")); whr.append_as_num<int64>(utag);
    db->delrows(CONSTASTR("history"), whr);
}

void profile_c::kill_history(const contact_key_s&historian)
{
    bool bchanged = false;
    for (auto &row : table_history.rows)
    {
        if (row.other.historian == historian)
            bchanged |= row.deleted();
    }
    if (bchanged)
        changed();

    // apply modification to db now due not all history items loaded
    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num<int64>( ts::ref_cast<int64>( historian ) );
    db->delrows( CONSTASTR("history"), whr );
}

bool profile_c::change_history_item(uint64 utag, contact_key_s & historian)
{
    bool ok = false;
    table_history.cleanup();
    table_history.find<true>([&](history_s &h) ->bool
    {
        if (h.utag == utag && h.type == MTA_UNDELIVERED_MESSAGE)
        {
            h.type = MTA_MESSAGE;
            historian = h.historian;
            ok = true;
            return true;
        }
        return false;
    });

    ts::tmp_str_c whr(CONSTASTR("utag=")); whr.append_as_num<int64>(ts::ref_cast<int64>(utag));
    whr.append(CONSTASTR(" and sender=0"));

    ts::data_pair_s dp;
    dp.name = CONSTASTR("mtype");
    dp.type_ = ts::data_type_e::t_int;
    dp.i = MTA_MESSAGE;

    db->update(CONSTASTR("history"), ts::array_wrapper_c<const ts::data_pair_s>(&dp, 1), whr);

    return ok;
}

void profile_c::change_history_item(const contact_key_s&historian, const post_s &post, ts::uint32 change_what)
{
    if (!change_what) return;
    table_history.cleanup();
    table_history.find<true>([&](history_s &h) ->bool
    {
        if (h.historian == historian && h.utag == post.utag)
        {
            if (0 != (change_what & HITM_MT)) h.type = post.type;
            if (0 != (change_what & HITM_TIME)) h.recv_time = post.recv_time, h.cr_time = post.cr_time;;
            if (0 != (change_what & HITM_MESSAGE)) h.message_utf8 = post.message_utf8;
            return true;
        }
        return false;
    });

    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    whr.append( CONSTASTR(" and utag=") ).append_as_num<int64>(ts::ref_cast<int64>(post.utag));
    whr.append( CONSTASTR(" and sender=") ).append_as_num<int64>(ts::ref_cast<int64>(post.sender));

    ts::data_pair_s dp[4]; int n = 0;
    if (0 != (change_what & HITM_MT))
    {
        dp[n].name = CONSTASTR("mtype");
        dp[n].type_ = ts::data_type_e::t_int;
        dp[n].i = post.mt();
        ++n;
    }
    if (0 != (change_what & HITM_TIME))
    {
        dp[n].name = CONSTASTR("mtime");
        dp[n].type_ = ts::data_type_e::t_int64;
        dp[n].i = post.recv_time;
        ++n;

        dp[n].name = CONSTASTR("crtime");
        dp[n].type_ = ts::data_type_e::t_int64;
        dp[n].i = post.cr_time;
        ++n;
    }
    if (0 != (change_what & HITM_MESSAGE))
    {
        dp[n].name = CONSTASTR("msg");
        dp[n].type_ = ts::data_type_e::t_str;
        dp[n].text = post.message_utf8;
        ++n;
    }
    ASSERT(n<=ARRAY_SIZE(dp));
    db->update( CONSTASTR("history"), ts::array_wrapper_c<const ts::data_pair_s>(dp,n), whr );
}

void profile_c::load_history( const contact_key_s&historian, time_t time, int nload, ts::tmp_tbuf_t<int>& loaded_ids )
{
    table_history.cleanup();

    auto fix = []( post_s &p )->bool
    {
        auto olft = p.mt();
        if (p.mt() == MTA_INCOMING_CALL) // если в загруженной истории есть это сообщение, то имел место краш
            p.type = MTA_INCOMING_CALL_CANCELED;
        else if (p.mt() == MTA_CALL_ACCEPTED) // если в загруженной истории есть это сообщение, то имел место краш во время разговора
            p.type = MTA_HANGUP;
        return p.mt() != olft;
    };

    if (time == 0)
    {
        time = now();
        time_t ct = time;
        ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
        whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>( time );
        table_history.read(db, whr);

        for (auto &row : table_history.rows)
        {
            if (row.other.historian != historian || row.other.recv_time < time) continue;
            row.other.recv_time = (++ct);
            bool fixed = fix(row.other);

            whr.set(CONSTASTR("utag=")); whr.append_as_num<int64>(ts::ref_cast<int64>(row.other.utag));

            ts::data_pair_s dp[2];
            dp[0].name = CONSTASTR("mtime");
            dp[0].type_ = ts::data_type_e::t_int64;
            dp[0].i = ct;
            if (fixed)
            {
                dp[1].name = CONSTASTR("mtype");
                dp[1].type_ = ts::data_type_e::t_int;
                dp[1].i = row.other.mt();
            }

            db->update(CONSTASTR("history"), ts::array_wrapper_c<const ts::data_pair_s>(dp, fixed ? 2 : 1), whr);
        }
        time = ct;
    }

    typedef tableview_t<history_s, pt_history>::row_s hitm;
    ts::tmp_pointers_t< hitm, 16 > candidates;
    for (auto &hi : table_history.rows)
        if (!hi.is_deleted())
            if (hi.other.historian == historian && hi.other.recv_time < time)
                candidates.add(&hi);
    if (candidates.size() > nload)
    {
        candidates.sort([](hitm *p1, hitm *p2)->bool {
            return p1->other.recv_time > p2->other.recv_time;
        });
        candidates.truncate(nload);
    } // else - no else here
    if (candidates.size() == nload)
    {
        for (auto *hi : candidates)
            loaded_ids.add(hi->id);
        return;
    }
    if (candidates.size())
    {
        time_t mint = time;
        for (auto *hi : candidates)
        {
            loaded_ids.add(hi->id);
            if (hi->other.recv_time < mint)
                mint = hi->other.recv_time;
        }
        time = mint;
        nload -= candidates.size();
    }

    table_history.read_ids = &loaded_ids;

    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num<int64>( ts::ref_cast<int64>( historian ) );
    whr.append( CONSTASTR(" and mtime<") ).append_as_num<int64>( time );
    whr.append( CONSTASTR(" order by mtime desc limit ") ).append_as_int( nload );
    
    table_history.read( db, whr );

    for(int idl : loaded_ids)
    {
        auto * row = table_history.find<true>(idl);
        if (row && fix( row->other ))
        {
            whr.set(CONSTASTR("utag=")); whr.append_as_num<int64>(ts::ref_cast<int64>(row->other.utag));

            ts::data_pair_s dp;
            dp.name = CONSTASTR("mtype");
            dp.type_ = ts::data_type_e::t_int;
            dp.i = row->other.mt();

            db->update(CONSTASTR("history"), ts::array_wrapper_c<const ts::data_pair_s>(&dp, 1), whr);
        }
    }

    table_history.read_ids = nullptr;
}

void profile_c::load_history( const contact_key_s&historian )
{
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    table_history.read( db, whr );
}

void profile_c::detach_history( const contact_key_s&prev_historian, const contact_key_s&new_historian, const contact_key_s&sender )
{
    // already loaded
    // just detach
    // don't care about time

    bool changed = false;
    for (auto &hi : table_history.rows)
    {
        if (hi.other.historian == prev_historian && (hi.other.sender == sender || hi.other.receiver == sender))
        {
            hi.other.historian = new_historian;
            hi.changed();
            changed = true;
        }
    }
    if (changed)
    {
        this->changed();
        table_history.flush(db, true); // very important to save now
    }
}

void profile_c::merge_history( const contact_key_s&base_historian, const contact_key_s&from_historian )
{
    table_history.cleanup();

    // just merge already loaded stuff

    bool changed = false;
    typedef tableview_t<history_s, pt_history>::row_s hitm;
    ts::tmp_pointers_t< hitm, 16 > baseitems;
    for (auto &hi : table_history.rows)
    {
        if (hi.other.historian == base_historian)
            baseitems.add(&hi);
        if (hi.other.historian == from_historian)
        {
            hi.other.historian = base_historian;
            baseitems.add(&hi);
            hi.changed();
            changed = true;
        }
    }
    baseitems.sort([](hitm *p1, hitm *p2)->bool {
        return p1->other.recv_time < p2->other.recv_time;
    });
    int cnt = baseitems.size();
    for( int i=1; i<cnt;++i )
    {
        hitm *prevh = baseitems.get(i-1);
        hitm *h = baseitems.get(i);
        if (h->other.recv_time <= prevh->other.recv_time)
        {
            h->other.recv_time = prevh->other.recv_time + 1;
            h->changed();
            changed = true;
        }
    }
    if (changed)
        this->changed();
}

void profile_c::flush_history_now()
{
    table_history.flush(db, true);
    table_history.cleanup();
}

int  profile_c::calc_history( const contact_key_s&historian, bool ignore_invites )
{
    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num<int64>( ts::ref_cast<int64>( historian ) );
    if (ignore_invites) whr.append( CONSTASTR(" and mtype<>2 and mtype<>103") ); // MTA_FRIEND_REQUEST MTA_OLD_REQUEST
    return db->count( CONSTASTR("history"), whr );
}

int  profile_c::calc_history_before( const contact_key_s&historian, time_t time )
{
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    whr.append( CONSTASTR(" and mtime<") ).append_as_num<int64>( time );
    return db->count(CONSTASTR("history"), whr);
}

int  profile_c::calc_history_after(const contact_key_s&historian, time_t time, bool only_messages)
{
    if (!db) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    if (only_messages) whr.append( CONSTASTR(" and mtype==0") );
    whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>(time);
    return db->count(CONSTASTR("history"), whr);
}

int  profile_c::calc_history_between( const contact_key_s&historian, time_t time1, time_t time2 )
{
    if (!db) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>(time1);
    whr.append(CONSTASTR(" and mtime<")).append_as_num<int64>(time2);
    return db->count(CONSTASTR("history"), whr);
}


profile_load_result_e profile_c::xload(const ts::wstr_c& pfn, const ts::uint8 *k)
{
    AUTOCLEAR( profile_flags, F_LOADING );

    if (db)
    {
        save_dirty(RID(), as_param(1));
        db->close();
        db = nullptr;
    }

    mutex.reset();

    closed = false;

#define TAB(tab) table_##tab.clear();
    PROFILE_TABLES
#undef TAB

    profile_flags.clear();
    values.clear();
    dirty.clear();
    path = pfn;
    db = ts::sqlitedb_c::connect(path, k, g_app->F_READONLY_MODE);
    if (!db)
        return PLR_CONNECT_FAILED;

    if (!db->is_correct())
    {
        db->close();
        db = nullptr;
        return PLR_CORRUPT_OR_ENCRYPTED;
    }

    {
        prepare_conf_table(db);

#define TAB(tab) if (!table_##tab.prepare( db )) { return PLR_UPGRADE_FAILED; }
        PROFILE_TABLES
#undef TAB

    }

    db->read_table( CONSTASTR("conf"), get_cfg_reader() );

    ts::str_c utag = unique_profile_tag();
    bool generated = false;
    if (utag.is_empty())
        utag.set_as_num<uint64>(ts::uuid()), generated = true;

    mutex.reset( ts::master().sys_global_atom( CONSTWSTR( "isotoxin_db_" ) + ts::to_wstr( utag ) ) );
    if (!mutex)
    {
        db->close();
        db = nullptr;
        return PLR_BUSY;
    }
    if (generated)
        unique_profile_tag( utag );
    
    #define TAB(tab) if (load_on_start<tab##_s>::value) table_##tab.read( db );
    PROFILE_TABLES
    #undef TAB

    load_undelivered();

    contact_c &self = contacts().get_self();
    self.set_name(username());
    self.set_statusmsg(userstatus());

    gmsg<ISOGM_PROFILE_TABLE_SAVED>( pt_active_protocol ).send(); // initiate active protocol reconfiguration/creation

    emoti().reload();

    if (1.0f != fontscale_conv_text() || 1.0f != fontscale_msg_edit()) //-V550
    {
        g_app->reload_fonts();
    }

    if (k)
    {
        memcpy(keyhash, k + CC_SALT_SIZE, CC_HASH_SIZE);
        profile_flags.set(F_ENCRYPTED);
    }

    g_app->apply_debug_options();
    g_app->resetup_spelling();

    return PLR_OK;
}

void profile_c::load_undelivered()
{
    ts::tmp_str_c whr(CONSTASTR("mtype=")); whr.append_as_int(MTA_UNDELIVERED_MESSAGE);

    tableview_history_s table;
    table.read(db, whr);

    for (const auto &row : table.rows)
        g_app->undelivered_message(row.other);

}

contact_root_c *profile_c::find_corresponding_historian(const contact_key_s &subcontact, ts::array_wrapper_c<contact_root_c * const> possible_historians) //-V813
{
    ts::tmp_str_c whr(CONSTASTR("sender=")); whr.append_as_num<int64>(ts::ref_cast<int64>(subcontact));
    whr.append(CONSTASTR(" or receiver=")).append_as_num<int64>(ts::ref_cast<int64>(subcontact));

    tableview_history_s table;
    table.read(db, whr);

    for (const auto &row : table.rows)
        for( contact_root_c * const c : possible_historians )
            if (c->getkey() == row.other.historian)
                return c;

    return nullptr;
}

profile_c::~profile_c()
{
    if (db)
        shutdown_aps();
}

int encrypt_process_i = 0, encrypt_process_n = 0;

namespace
{
    struct encrypt_task_s : public ts::task_c
    {
        ts::sqlitedb_c *db;
        ts::uint8 key[CC_SALT_SIZE + CC_HASH_SIZE];
        int errc = 0;
        bool remove_enc = false;

        encrypt_task_s(ts::sqlitedb_c *db, const ts::uint8 *passwdhash):db(db)
        {
            if (passwdhash)
                memcpy(key + CC_SALT_SIZE, passwdhash, CC_HASH_SIZE);
            else
                remove_enc = true;
        }

        void process_callback( int i, int n )
        {
            encrypt_process_i = i;
            encrypt_process_n = n;
        }

        /*virtual*/ int iterate(int pass) override
        {
            if (remove_enc)
            {
                db->rekey(nullptr, DELEGATE(this, process_callback));
            } else
            {
                gen_salt(key, CC_SALT_SIZE);
                db->rekey(key, DELEGATE(this, process_callback));
            }
            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (errc == 0)
                prf().encrypt_done(remove_enc ? nullptr : key + CC_SALT_SIZE);
            else
                prf().encrypt_failed();

            __super::done(canceled);
        }
    };

    class encryptor_c : public pb_job_c
    {
        ts::uint8 passwhash[CC_HASH_SIZE];
        ts::safe_ptr<dialog_pb_c> pbd;
        ts::Time starttime;
        float oldv = -1;
        int errc = 0;
        bool remove_enc = false;

    public:
        encryptor_c(const ts::uint8 *passwhash_):starttime(ts::Time::undefined())
        {
            if (passwhash_)
                memcpy(passwhash, passwhash_, sizeof(passwhash));
            else
                remove_enc = true;
        }
        ~encryptor_c()
        {
            gui->delete_event(DELEGATE(this, tick));
        }

        /*virtual*/ void on_create(dialog_pb_c *pb) override
        {
            starttime = ts::Time::current();
            tick(RID(), nullptr);
            pbd = pb;
            ts::sqlitedb_c *db = prf().begin_encrypt();
            g_app->add_task(TSNEW(encrypt_task_s, db, remove_enc ? nullptr : passwhash));
        }

        /*virtual*/ void on_close() override
        {
            TSDEL(this);
        }

        bool tick(RID, GUIPARAM)
        {
            float processtime_t = (float)(ts::Time::current() - starttime) /  ( minimum_encrypt_pb_duration * 1000.0f );
            float newv = encrypt_process_n ? (float)encrypt_process_i / (float)encrypt_process_n : 1.0f;
            if (!encrypt_process_n) errc = encrypt_process_i;

            float vv = ts::tmin(processtime_t, newv);
            if (vv > 1.0f) vv = 1.0f;

            if (oldv < vv)
            {
                if (pbd)
                {
                    pbd->set_level(vv, ts::roundstr<ts::wstr_c>(vv * 100.0f, 1).append(CONSTWSTR("%")));
                }

                oldv = vv;
            }

            if (!encrypt_process_n && oldv > 0)
            {
                int e = errc;
                bool enc = !remove_enc;
                if (pbd)
                    TSDEL(pbd);
                else
                    TSDEL(this);

                if (e > 0)
                    dialog_msgbox_c::mb_error(TTT("Your profile has not been encrypted. Error code: $", 378) / ts::wmake(e)).summon();
                else
                {
                    if (enc)
                        dialog_msgbox_c::mb_info(TTT("Your profile has been successfully encrypted. After [appname] restart, you will be prompted for password. Don't forget it!", 381)).summon();
                    else
                        dialog_msgbox_c::mb_info(TTT("Your profile has been successfully decrypted.", 377)).summon();
                }

                return true;
            }


            DEFERRED_UNIQUE_CALL(0.01, DELEGATE(this, tick), nullptr);
            return true;
        }
    };


    class encrypt_c : public delay_event_c
    {
        ts::uint8 passwhash[CC_HASH_SIZE];
        bool remove_enc = false;
    public:
        encrypt_c(const ts::uint8 *passwhash_)
        {
            if (passwhash_)
                memcpy(passwhash, passwhash_, sizeof(passwhash));
            else
                remove_enc = true;
        }

        /*virtual*/ void    doit() override
        {
            SUMMON_DIALOG<dialog_pb_c>(UD_ENCRYPT_PROFILE_PB, dialog_pb_c::params(
                gui_isodialog_c::title(remove_enc ? title_removing_encryption : title_encrypting),
                ts::wstr_c()
                ).setpbproc(TSNEW(encryptor_c, remove_enc ? nullptr : passwhash)));
        };
        /*virtual*/ void    die() override { gui->delete_event<encrypt_c>(this); }
    };

}

ts::sqlitedb_c *profile_c::begin_encrypt()
{
    profile_flags.set(F_ENCRYPT_PROCESS);
    return db;
}

void profile_c::encrypt_done(const ts::uint8 *newpasshash)
{
    profile_flags.clear(F_ENCRYPT_PROCESS);
    if (newpasshash)
    {
        memcpy(keyhash, newpasshash, sizeof(keyhash));
        profile_flags.set(F_ENCRYPTED);

    } else
    {
        profile_flags.clear(F_ENCRYPTED);
    }
}

void profile_c::encrypt_failed()
{
    profile_flags.clear(F_ENCRYPT_PROCESS);
}


void profile_c::encrypt( const ts::uint8 *passwdhash /* 256 bit (CC_HASH_SIZE bytes) hash */, float delay_before_start )
{
    if (g_app->F_READONLY_MODE)
        return;

    gui->add_event_t<encrypt_c, const ts::uint8 *>(delay_before_start, passwdhash);
}

void profile_c::mb_error_load_profile( const ts::wsptr & prfn, profile_load_result_e plr, bool modal )
{
    ts::wstr_c text;
    
    switch (plr)
    {
        case PLR_CORRUPT_OR_ENCRYPTED:
            text = TTT("Profile [b]$[/b] is corrupted or incorrect password!",47) / prfn;
            break;
        case PLR_UPGRADE_FAILED:
            if (g_app->F_READONLY_MODE)
                text = TTT("Profile [b]$[/b] has old format and can not be upgraded due it write protected!", 333) / prfn;
            break;
        case PLR_CONNECT_FAILED:
            text = TTT("Can't open profile [b]$[/b]!", 388) / prfn;
            break;
        case PLR_BUSY:
            text = TTT("Profile [b]$[/b] is busy!", 270) / prfn;
            break;
    }
    
    struct s
    {
        static void exit_now(const ts::str_c&)
        {
            ts::master().mainwindow = nullptr;
            ts::master().sys_exit(10);
        }
    };

    redraw_collector_s dch;
    dialog_msgbox_c::mb_error( text )
        .bok( modal ? loc_text(loc_exit) : ts::wsptr() )
        .on_ok(modal ? s::exit_now : MENUHANDLER(), ts::asptr())
        .summon();
}

bool profile_c::addeditnethandler(dialog_protosetup_params_s &params)
{
    active_protocol_data_s *apd = nullptr;
    int id = 0;
    if (params.protoid == 0)
    {
        auto &r = get_table_active_protocol().getcreate(0);
        id = r.id;
        r.other.tag = params.networktag;
        if (!params.importcfg.is_empty())
            r.other.config.load_from_disk_file(params.importcfg);
        apd = &r.other;
    }
    else
    {
        auto *row = get_table_active_protocol().find<true>(params.protoid);
        apd = &row->other;
        id = params.protoid;
        row->changed();
    }

    apd->user_name = params.uname;
    apd->user_statusmsg = params.ustatus;
    apd->tag = params.networktag;
    apd->name = params.networkname;
    INITFLAG(apd->options, active_protocol_data_s::O_AUTOCONNECT, params.connect_at_startup);
    apd->configurable = params.configurable;

    changed(true);

    return true;
}

void profile_c::shutdown_aps()
{
    save_dirty(RID(), as_param(1));
    for (active_protocol_c *ap : protocols)
        if (ap) TSDEL(ap);
}

/*virtual*/ void profile_c::onclose()
{
    profile_flags.set(F_CLOSING);

    for (active_protocol_c *ap : protocols)
        if (ap) ap->save_config(true);

    save();
}

void profile_c::killcontact( const contact_key_s&ck )
{
    table_contacts.cleanup();
    dirtycontacts.find_remove_fast(ck);
    if (auto *row = table_contacts.find<false>([&](const contacts_s &k)->bool { return k.key == ck; }))
        if (row->deleted())
            changed();
}

void profile_c::purifycontact( const contact_key_s&ck )
{
    table_contacts.cleanup();
    dirtycontacts.find_remove_fast(ck);
    if (auto *row = table_contacts.find<true>([&](const contacts_s &k)->bool { return k.key == ck; }))
    {
        row->other.key = contact_key_s();
        if (row->deleted())
            changed();
    }
}

bool profile_c::isfreecontact( const contact_key_s&ck ) const
{
    return nullptr == table_contacts.find<true>([&](const contacts_s &k)->bool { return k.key == ck; });
}

ts::blob_c profile_c::get_avatar( const contact_key_s&ck ) const
{
    auto *row = table_contacts.find<false>([&](const contacts_s &k)->bool { return k.key == ck; });
    if (row) return row->other.avatar;
    return ts::blob_c();
}

void profile_c::set_avatar( const contact_key_s&ck, const ts::blob_c &avadata, int tag )
{
    auto *row = table_contacts.find<false>([&](const contacts_s &k)->bool { return k.key == ck; });

    if (!row)
    {
        row = &table_contacts.getcreate(0);
        row->other.key = ck;
    }
    else
    {
        if (row->is_deleted())
            return;

        row->changed();
    }

    row->other.avatar = avadata;
    row->other.avatar_tag = tag;
    dirtycontact(ck);
}

/*virtual*/ bool profile_c::save()
{
    if (profile_flags.is(F_ENCRYPT_PROCESS))
    {
        ts::master().sys_sleep(1);
        return false;
    }

    if (__super::save()) return true;

    for(const contact_key_s &ck : dirtycontacts)
    {
        const contact_c * c = contacts().find(ck);
        if (c && !c->get_options().unmasked().is(contact_c::F_DIP))
        {
            auto *row = table_contacts.find<false>( [&](const contacts_s &k)->bool { return k.key == ck; } );
            if (!row)
            {
                row = &table_contacts.getcreate(0);
                row->other.key = ck;
                row->other.avatar_tag = 0;
            } else
            {
                if (row->is_deleted())
                    continue;

                row->changed();
            }
            if (!c->save( &row->other ))
                row->deleted();
        }
    }
    dirtycontacts.clear();

    ts::db_transaction_c __transaction( db );

#define TAB(tab) if (table_##tab.flush(db, false)) return true;
    PROFILE_TABLES
#undef TAB

    if (db)
    {
        time_t oldbackuptime = now() - backup_keeptime() * 86400;
        db->delrows(backup_protocol_s::get_table_name(), ts::tmp_str_c(CONSTASTR("time<")).append_as_num<int64>(oldbackuptime));
    }

    return false;
}

void profile_c::check_aps()
{
    bool createaps = false;
    int cnt = protocols.size();
    for(int i=cnt-1;i>=0;--i)
    {
        active_protocol_c *ap = protocols.get(i);
        if (nullptr == ap)
        {
            protocols.remove_fast(i);
            createaps = true;
        } else
            ap->once_per_5sec_tick();
    }
    if (createaps)
        create_aps();
}

void profile_c::create_aps()
{
    for (const auto& row : table_active_protocol.rows)
    {
        if (active_protocol_c *ap = this->ap(row.id))
            continue;
        protocols.add(TSNEW(active_protocol_c, row.id, row.other));
    }

}

ts::uint32 profile_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SAVED>&p )
{
    if (p.tabi == pt_active_protocol && !profile_flags.is(F_LOADING) && !profile_flags.is(F_CLOSING))
    {
        if (p.pass == 0)
            for(active_protocol_c *ap : protocols)
                if (ap && ap->is_new()) return GMRBIT_CALLAGAIN;

        create_aps();
    }
    return 0;
}

ts::wstr_c profile_c::get_disabled_dicts()
{
    ts::wstr_c dd = disabled_spellchk();
    if (!dd.equals(CONSTWSTR("?")))
        return dd;

    return ts::wstr_c();
}
