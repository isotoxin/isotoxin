#include "isotoxin.h"

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
        default:
            FORBIDDEN();
    }
}

void default_rows<active_protocol_s>::setup_default(int index, active_protocol_s& d)
{
    switch (index)
    {
        case 0:
            d.tag = CONSTASTR("tox");
            d.name = CONSTASTR("Tox");
            d.user_name.clear();
            d.user_statusmsg.clear();
            d.config.clear();
            d.avatar.clear();
            d.options = active_protocol_data_s().options;
            return;
        case 1:
            d.tag = CONSTASTR("lan");
            d.name = CONSTASTR("Lan");
            d.user_name.clear();
            d.user_statusmsg.clear();
            d.config.clear();
            d.avatar.clear();
            d.options = active_protocol_data_s().options;
            return;
        default:
            FORBIDDEN();
    }
}

/////// contacts

void contacts_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
        case 1:
            key.contactid = (int)v.i;
            return;
        case 2:
            key.protoid = (int)v.i;
            return;
        case 3:
            metaid = (int)v.i;
            return;
        case 4:
            options = (int)v.i;
            return;
        case 5:
            name = v.text;
            return;
        case 6:
            statusmsg = v.text;
            return;
        case 7:
            readtime = v.i;
            return;
        case 8:
            customname = v.text;
            return;
        case 9:
            avatar = v.blob;
            return;
        case 10:
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
        case 1:
            v.i = key.contactid;
            return;
        case 2:
            v.i = key.protoid;
            return;
        case 3:
            v.i = metaid;
            return;
        case 4:
            v.i = options;
            return;
        case 5:
            v.text = name;
            return;
        case 6:
            v.text = statusmsg;
            return;
        case 7:
            v.i = readtime;
            return;
        case 8:
            v.text = customname;
            return;
        case 9:
            v.blob = avatar;
            return;
        case 10:
            v.i = avatar_tag;
            return;
    }
}

ts::data_type_e contacts_s::get_column_type(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 10:
            return ts::data_type_e::t_int;
        case 5:
        case 6:
        case 8:
            return ts::data_type_e::t_str;
        case 7:
            return ts::data_type_e::t_int64;
        case 9:
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
        case 1:
            cd.name_ = CONSTASTR("contact_id");
            break;
        case 2:
            cd.name_ = CONSTASTR("proto_id");
            break;
        case 3:
            cd.name_ = CONSTASTR("meta_id");
            break;
        case 4:
            cd.name_ = CONSTASTR("options");
            break;
        case 5:
            cd.name_ = CONSTASTR("name");
            break;
        case 6:
            cd.name_ = CONSTASTR("statusmsg");
            break;
        case 7:
            cd.name_ = CONSTASTR("readtime");
            break;
        case 8:
            cd.name_ = CONSTASTR("customname");
            break;
        case 9:
            cd.name_ = CONSTASTR("avatar");
            break;
        case 10:
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
        case 1:
            time = v.i;
            return;
        case 2:
            historian = ts::ref_cast<contact_key_s>(v.i);
            return;
        case 3:
            sender = ts::ref_cast<contact_key_s>(v.i);
            return;
        case 4:
            receiver = ts::ref_cast<contact_key_s>(v.i);
            return;
        case 5:
            type = v.i & (SETBIT(type_size_bits)-1);
            options = (v.i >> 16) & (SETBIT(options_size_bits)-1);
            return;
        case 6:
            message_utf8 = v.text;
            return;
        case 7:
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
        case 1:
            v.i = time;
            return;
        case 2:
            v.i = ts::ref_cast<int64>(historian);
            return;
        case 3:
            v.i = ts::ref_cast<int64>( sender );
            return;
        case 4:
            v.i = ts::ref_cast<int64>( receiver );
            return;
        case 5:
            v.i = type;
            v.i |= ((int64)options) << 16;
            return;
        case 6:
            v.text = message_utf8;
            return;
        case 7:
            v.i = utag;
            return;
    }
}

ts::data_type_e history_s::get_column_type(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 7:
            return ts::data_type_e::t_int64;
        case 5:
            return ts::data_type_e::t_int;
        case 6:
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
        case 1:
            cd.name_ = CONSTASTR("mtime");
            break;
        case 2:
            cd.name_ = CONSTASTR("historian");
            break;
        case 3:
            cd.name_ = CONSTASTR("sender");
            break;
        case 4:
            cd.name_ = CONSTASTR("receiver");
            break;
        case 5:
            cd.name_ = CONSTASTR("mtype");
            break;
        case 6:
            cd.name_ = CONSTASTR("msg");
            break;
        case 7:
            cd.name_ = CONSTASTR("utag");
            cd.index = true;
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


template<typename T, profile_table_e tabi> void tableview_t<T, tabi>::prepare( ts::sqlitedb_c *db )
{
    ts::tmp_array_inplace_t<ts::column_desc_s, 0> cds( T::columns );

    ts::column_desc_s &idc = cds.add();
    idc.name_ = CONSTASTR("id");
    idc.type_ = ts::data_type_e::t_int;
    idc.primary = true;
    idc.autoincrement = true;

    for(int i=1;i<T::columns;++i)
        T::get_column_desc(i, cds.add());

    if (db->update_table_struct(T::get_table_name(), cds.array(), false))
        for(int i = 0; i<default_rows<T>::value; ++i)
            default_rows<T>::setup_default(i, getcreate(0).other);

    flush( db, true, false );
}


template<typename T, profile_table_e tabi> bool tableview_t<T, tabi>::flush( ts::sqlitedb_c *db, bool all, bool notify_saved )
{
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
                        new2ins[r.id] = newid;
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


template<typename T, profile_table_e tabi> bool tableview_t<T, tabi>::reader(int row, ts::SQLITE_DATAGETTER getta)
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
    if (msg.pass != 0) return 0;
    bool second_pass_requred = msg.post.time == 1;
    contact_c *historian = msg.get_historian();
    if (historian == nullptr) return 0;
    //bool record_it = true;

    //if (record_it)
    {
        time_t nowt = historian->nowtime();
        if (msg.create_time && msg.create_time <= nowt)
        {
            historian->make_time_unique(msg.create_time);
            nowt = msg.create_time;
        }

        post_s &post = historian->add_history(nowt);
        msg.post.time = post.time;
        // [POST_INIT]
        post.sender = msg.post.sender;
        post.receiver = msg.post.receiver;
        post.type = msg.post.type;
        post.message_utf8 = msg.post.message_utf8;
        post.utag = msg.post.utag;
        post.options = 0;

        if (historian->keep_history())
            record_history(historian->getkey(), post);
    }

    return second_pass_requred ? GMRBIT_CALLAGAIN : 0;
}

ts::uint32 profile_c::gm_handler(gmsg<ISOGM_CHANGED_PROFILEPARAM>&ch)
{
    bool changed = false;
    if (ch.protoid)
        switch (ch.pp)
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
    ASSERT(history_item.time != 0);
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
            if (0 != (change_what & HITM_TIME)) h.time = post.time;
            if (0 != (change_what & HITM_MESSAGE)) h.message_utf8 = post.message_utf8;
            return true;
        }
        return false;
    });

    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
    whr.append( CONSTASTR(" and utag=") ).append_as_num<int64>(ts::ref_cast<int64>(post.utag));
    whr.append( CONSTASTR(" and sender=") ).append_as_num<int64>(ts::ref_cast<int64>(post.sender));

    ts::data_pair_s dp[3]; int n = 0;
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
        dp[n].i = post.time;
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
            if (row.other.historian != historian || row.other.time < time) continue;
            row.other.time = (++ct);
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
            if (hi.other.historian == historian && hi.other.time < time)
                candidates.add(&hi);
    if (candidates.size() > nload)
    {
        candidates.sort([](hitm *p1, hitm *p2)->bool {
            return p1->other.time > p2->other.time;
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
            if (hi->other.time < mint)
                mint = hi->other.time;
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
        if (hi.other.historian == prev_historian && hi.other.sender == sender)
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
        return p1->other.time < p2->other.time;
    });
    int cnt = baseitems.size();
    for( int i=1; i<cnt;++i )
    {
        hitm *prevh = baseitems.get(i-1);
        hitm *h = baseitems.get(i);
        if (h->other.time <= prevh->other.time)
        {
            h->other.time = prevh->other.time + 1;
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

int  profile_c::calc_history_after(const contact_key_s&historian, time_t time)
{
    if (!db) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num<int64>(ts::ref_cast<int64>(historian));
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


void profile_c::load(const ts::wstr_c& pfn)
{
    if (db)
    {
        save_dirty(RID(), (GUIPARAM)1);
        db->close();
    }
    closed = false;

#define TAB(tab) table_##tab.clear();
    PROFILE_TABLES
#undef TAB

    values.clear();
    dirty.clear();
    path = pfn;
    db = ts::sqlitedb_c::connect(path);

    if (ASSERT(db))
    {
        prepare_conf_table(db);

#define TAB(tab) table_##tab.prepare( db );
        PROFILE_TABLES
#undef TAB

    }

    db->read_table( CONSTASTR("conf"), get_cfg_reader() );
    
    #define TAB(tab) if (load_on_start<tab##_s>::value) table_##tab.read( db );
    PROFILE_TABLES
    #undef TAB

    contact_c &self = contacts().get_self();
    self.set_name(username());
    self.set_statusmsg(userstatus());

    gmsg<ISOGM_PROFILE_TABLE_SAVED>( pt_active_protocol ).send(); // initiate active protocol reconfiguration/creation

    emoti().reload();
}


profile_c::~profile_c()
{
    shutdown_aps();
}

void profile_c::shutdown_aps()
{
    for (active_protocol_c *ap : protocols)
        if (ap) TSDEL(ap);
}

/*virtual*/ void profile_c::onclose()
{
    for (active_protocol_c *ap : protocols)
        if (ap) ap->save_config(true);
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
    if (__super::save()) return true;

    for(const contact_key_s &ck : dirtycontacts)
    {
        const contact_c * c = contacts().find(ck);
        if (c)
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

#define TAB(tab) if (table_##tab.flush(db, false)) return true;
    PROFILE_TABLES
#undef TAB

    return false;
}

ts::uint32 profile_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SAVED>&p )
{
    if (p.tabi == pt_active_protocol)
    {
        if (p.pass == 0)
            for(active_protocol_c *ap : protocols)
                if (ap && ap->is_new()) return GMRBIT_CALLAGAIN;
        
        for( const auto& row : table_active_protocol.rows )
        {
            //if (0 != (row.other.options & active_protocol_data_s::O_SUSPENDED)) continue;
            if ( active_protocol_c *ap = this->ap(row.id) )
                continue;
            protocols.add( TSNEW(active_protocol_c, row.id, row.other) );
        }
    }
    return 0;
}
