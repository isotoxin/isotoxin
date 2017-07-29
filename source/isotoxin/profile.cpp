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
            options = static_cast<int>(v.i);
            return;
        case 7:
            avatar = v.blob;
            return;
        case 8:
            sort_factor = static_cast<int>(v.i);
            return;
        case 9:
            configurable.login = v.text;
            return;
        case 10:
            configurable.set_password_as_is( v.text );
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
        case 9:
            v.text = configurable.login;
            return;
        case 10:
            v.text = configurable.get_password_encoded();
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
        case 9:
        case 10:
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
        case 9:
            cd.name_ = CONSTASTR( "login" );
            break;
        case 10:
            cd.name_ = CONSTASTR( "password" );
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
            key.contactid = static_cast<int>(v.i);
            return;
        case C_PROTO_ID:
            key.protoid = static_cast<ts::uint16>( v.i );
            return;
        case C_META_ID:
            metaid = static_cast<int>( v.i );
            return;
        case C_OPTIONS:
            options = static_cast<int>( v.i );
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
        case C_GREETING:
            greeting = v.text;
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
            avatar_tag = static_cast<int>( v.i );
            return;
        case C_PROTO_DATA:
            protodata = v.blob;
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
        case C_GREETING:
            v.text = greeting;
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
        case C_PROTO_DATA:
            v.blob = protodata;
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
        case C_GREETING:
        case C_COMMENT:
        case C_TAGS:
        case C_MESSAGEHANDLER:
            return ts::data_type_e::t_str;
        case C_READTIME:
            return ts::data_type_e::t_int64;
        case C_AVATAR:
        case C_PROTO_DATA:
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
        case C_GREETING:
            cd.name_ = CONSTASTR( "greeting" );
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
        case C_PROTO_DATA:
            cd.name_ = CONSTASTR("protodata");
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
            historian = contact_key_s::buildfromdbvalue( v.i, true );
            return;
        case C_SENDER:
            sender = contact_key_s::buildfromdbvalue( v.i, false );
            return;
        case C_RECEIVER:
            receiver = historian.temp_type == TCT_CONFERENCE ? historian : contact_key_s::buildfromdbvalue( v.i, false );
            return;
        case C_TYPE_AND_OPTIONS:
            set_type_and_options( v.i );
            return;
        case C_MSG:
            set_message_text( v.text );
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
            v.i = historian.dbvalue();
            return;
        case C_SENDER:
            v.i = sender.dbvalue();
            return;
        case C_RECEIVER:
            v.i = historian.is_conference() ? 0 : receiver.dbvalue();
            return;
        case C_TYPE_AND_OPTIONS:
            v.i = type;
            v.i |= static_cast<int64>(options) << 16;
            return;
        case C_MSG:
            if ( message_utf8 )
                v.text = message_utf8->cstr();
            else
                v.text.clear();
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
            historian = contact_key_s::buildfromdbvalue(v.i, true);
            return;
        case 2:
            sender = contact_key_s::buildfromdbvalue(v.i, false);
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
            v.i = historian.dbvalue();
            return;
        case 2:
            v.i = sender.dbvalue();
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


/////// folder share

void folder_share_s::set(int column, ts::data_value_s &v)
{
    switch (column)
    {
    case C_HISTORIAN:
        historian = contact_key_s::buildfromdbvalue(v.i, true);
        return;
    case C_UTAG:
        utag = v.i;
        return;
    case C_NAME:
        name = v.text;
        return;
    case C_PATH:
        path.set_as_utf8(v.text);
        return;
    case C_OUTBOUND:
        t = static_cast<fstype_e>(v.i);
        return;
    case C_USETIME:
        usetime = v.i;
        return;
    }
}

void folder_share_s::get(int column, ts::data_pair_s& v)
{
    ts::column_desc_s ccd;
    get_column_desc(column, ccd);
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch (column)
    {
    case C_HISTORIAN:
        v.i = historian.dbvalue();
        return;
    case C_UTAG:
        v.i = utag;
        return;
    case C_NAME:
        v.text = name;
        return;
    case C_PATH:
        v.text = to_utf8(path);
        return;
    case C_OUTBOUND:
        v.i = t;
        return;
    case C_USETIME:
        v.i = usetime;
        return;
    }
}

ts::data_type_e folder_share_s::get_column_type(int index)
{
    switch (index)
    {
    case C_HISTORIAN:
    case C_UTAG:
    case C_USETIME:
        return ts::data_type_e::t_int64;
    case C_PATH:
    case C_NAME:
        return ts::data_type_e::t_str;
    case C_OUTBOUND:
        return ts::data_type_e::t_int;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void folder_share_s::get_column_desc(int index, ts::column_desc_s&cd)
{
    cd.type_ = get_column_type(index);
    switch (index)
    {
    case C_HISTORIAN:
        cd.name_ = CONSTASTR("historian");
        break;
    case C_UTAG:
        cd.name_ = CONSTASTR("utag");
        break;
    case C_NAME:
        cd.name_ = CONSTASTR("name");
        break;
    case C_PATH:
        cd.name_ = CONSTASTR("path");
        break;
    case C_OUTBOUND:
        cd.name_ = CONSTASTR("outb");
        break;
    case C_USETIME:
        cd.name_ = CONSTASTR("lastuse");
        break;
    default:
        FORBIDDEN();
    }
}

// folder share


/////// conferences

void conference_s::set( int column, ts::data_value_s &v )
{
    switch ( column )
    {
    case C_PUBID:
        pubid = v.text;
        ASSERT( !confa );
        return;
    case C_NAME:
        name = v.text;
        ASSERT( !confa );
        return;
    case C_COMMENT:
        comment = v.text;
        ASSERT( !confa );
        return;
    case C_ID:
        id = static_cast<int>(v.i);
        return;
    case C_APID:
        proto_id = static_cast<ts::uint16>( v.i );
        return;
    case C_READTIME:
        readtime = v.i;
        return;
    case C_FLAGS:
        flags.__bits = static_cast<ts::flags32_s::BITS>(v.i);
        return;
    case C_TAGS:
        tags.split<char>( v.text, ',' );
        return;
    case C_KEYWORDS:
        keywords = ts::from_utf8(v.text);
        textmatchkeywords.reset( text_match_c::build_from_template( keywords ) );
        return;
    }
}

void conference_s::get( int column, ts::data_pair_s& v )
{
    ts::column_desc_s ccd;
    get_column_desc( column, ccd );
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch ( column )
    {
    case C_PUBID:
        v.text = confa ? confa->get_pubid() : pubid;
        return;
    case C_NAME:
        v.text = confa ? confa->get_name(false) : name;
        return;
    case C_COMMENT:
        v.text = confa ? confa->get_comment() : comment;
        return;
    case C_ID:
        v.i = id;
        ASSERT( !confa || ( confa->getkey().contactid == (unsigned)id && confa->getkey().temp_type == TCT_CONFERENCE ) );
        return;
    case C_APID:
        v.i = proto_id;
        ASSERT( (!confa && proto_key.is_empty()) || ( confa && confa->getkey().protoid == proto_id && confa->getkey().temp_type == TCT_CONFERENCE ) );
        return;
    case C_READTIME:
        v.i = readtime;
        return;
    case C_FLAGS:
        v.i = flags.__bits;
        return;
    case C_TAGS:
        v.text = tags.join( ',' );
        return;
    case C_KEYWORDS:
        v.text = ts::to_utf8(keywords);
        return;
    }
}

ts::data_type_e conference_s::get_column_type( int index )
{
    switch ( index )
    {
    case C_NAME:
    case C_PUBID:
    case C_COMMENT:
    case C_TAGS:
    case C_KEYWORDS:
        return ts::data_type_e::t_str;
    case C_ID:
    case C_APID:
    case C_FLAGS:
        return ts::data_type_e::t_int;
    case C_READTIME:
        return ts::data_type_e::t_int64;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void conference_s::get_column_desc( int index, ts::column_desc_s&cd )
{
    cd.type_ = get_column_type( index );
    switch ( index )
    {
    case C_PUBID:
        cd.name_ = CONSTASTR( "pubid" );
        break;
    case C_NAME:
        cd.name_ = CONSTASTR( "name" );
        break;
    case C_COMMENT:
        cd.name_ = CONSTASTR( "comment" );
        break;
    case C_ID:
        cd.name_ = CONSTASTR( "confid" );
        cd.options.set( ts::column_desc_s::f_unique_index );
        break;
    case C_APID:
        cd.name_ = CONSTASTR( "protoid" );
        break;
    case C_READTIME:
        cd.name_ = CONSTASTR( "readtime" );
        break;
    case C_FLAGS:
        cd.name_ = CONSTASTR( "flags" );
        break;
    case C_TAGS:
        cd.name_ = CONSTASTR( "tags" );
        return;
    case C_KEYWORDS:
        cd.name_ = CONSTASTR( "keywords" );
        return;
    default:
        FORBIDDEN();
    }
}

void conference_s::change_keywords( const ts::wstr_c& newkeywords )
{
    if (!newkeywords.equals( keywords ))
    {
        textmatchkeywords.reset( text_match_c::build_from_template( newkeywords ) );
        keywords = newkeywords;
        row_by_type( this ).changed();
        prf().changed();
    }
}


bool conference_s::update_name()
{
    if ( ASSERT( confa ) && !name.equals(confa->get_name(false)))
    {
        name = confa->get_name( false );
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }

    return false;
}

bool conference_s::update_comment()
{
    if (ASSERT( confa ) && !comment.equals( confa->get_comment() ))
    {
        comment = confa->get_comment();
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }

    return false;
}

bool conference_s::update_tags()
{
    if (ASSERT( confa ) && tags != confa->get_tags())
    {
        tags = confa->get_tags();
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }

    return false;
}

bool conference_s::update_keeph()
{
    if (ASSERT( confa ))
    {
        ts::flags32_s::BITS old = flags.__bits;
        flags.__bits = flags.__bits & 0xffff;
        flags.__bits |= (confa->get_keeph() << 16);

        if (old != flags.__bits)
        {
            row_by_type( this ).changed();
            prf().changed();
            return true;
        }
    }
    return false;
}

bool conference_s::update_readtime()
{
    if ( ASSERT( confa ) && readtime != confa->get_readtime() )
    {
        readtime = confa->get_readtime();
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }

    return false;
}

bool conference_s::change_flag( ts::flags32_s::BITS mask, bool val )
{
    ts::flags32_s::BITS old = flags.__bits;
    flags.init( mask, val );
    if (old != flags.__bits)
    {
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }
    return false;
}

bool conference_s::is_hl_message( const ts::wsptr &message, const ts::wsptr &my_name ) const
{
    if (my_name.l > 2 && ts::pwstr_c( message ).find_pos( my_name ) >= 0)
        return true;

    if (textmatchkeywords)
        return textmatchkeywords->match( message );

    return false;
}

// conferences

/////// conference members

void conference_member_s::set( int column, ts::data_value_s &v )
{
    switch ( column )
    {
    case C_PUBID:
        pubid = v.text;
        ASSERT( !memba );
        return;
    case C_NAME:
        name = v.text;
        ASSERT( !memba );
        return;
    case C_ID:
        id = static_cast<int>( v.i );
        return;
    case C_APID:
        proto_id = static_cast<ts::uint16>( v.i );;
        return;
    }
}

void conference_member_s::get( int column, ts::data_pair_s& v )
{
    ts::column_desc_s ccd;
    get_column_desc( column, ccd );
    v.type_ = ccd.type_;
    v.name = ccd.name_;
    switch ( column )
    {
    case C_PUBID:
        v.text = memba ? memba->get_pubid() : pubid;
        return;
    case C_NAME:
        v.text = memba ? memba->get_name( false ) : name;
        return;
    case C_ID:
        v.i = id;
        ASSERT( !memba || ( memba->getkey().contactid == (unsigned)id && memba->getkey().temp_type == TCT_UNKNOWN_MEMBER ) );
        return;
    case C_APID:
        v.i = proto_id;
        ASSERT( ( !memba && proto_key.is_empty() ) || ( memba && memba->getkey().protoid == proto_id && memba->getkey().temp_type == TCT_UNKNOWN_MEMBER ) );
        return;
    }
}

ts::data_type_e conference_member_s::get_column_type( int index )
{
    switch ( index )
    {
    case C_PUBID:
    case C_NAME:
        return ts::data_type_e::t_str;
    case C_ID:
    case C_APID:
        return ts::data_type_e::t_int;
    }
    FORBIDDEN();
    return ts::data_type_e::t_null;
}

void conference_member_s::get_column_desc( int index, ts::column_desc_s&cd )
{
    cd.type_ = get_column_type( index );
    switch ( index )
    {
    case C_PUBID:
        cd.name_ = CONSTASTR( "pubid" );
        break;
    case C_NAME:
        cd.name_ = CONSTASTR( "name" );
        break;
    case C_ID:
        cd.name_ = CONSTASTR( "membid" );
        cd.options.set( ts::column_desc_s::f_unique_index );
        break;
    case C_APID:
        cd.name_ = CONSTASTR( "protoid" );
        break;
    default:
        FORBIDDEN();
    }
}

bool conference_member_s::update_name()
{
    if ( !name.equals( memba->get_name( false ) ) )
    {
        name = memba->get_name( false );
        row_by_type( this ).changed();
        prf().changed();
        return true;
    }

    return false;
}

// conference members


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
    if ( !db ) return false;

    ts::db_transaction_c __transaction( db );

    MEMT( MEMT_SQLITE );

    ts::tmp_array_inplace_t<ts::data_pair_s, 0> vals( T::columns );
    const int n_per_call = 10;
    int n_done = 0;
    bool some_action = false;
    for(row_s &r: rows)
    {
        switch (r.st)
        {
        case row_s::s_new:
            if ( n_done >= n_per_call ) return true;
        case row_s::s_changed:
            if ( n_done >= n_per_call ) return true;
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
                    if (limit_id<T>::value > 0 && newid > limit_id<T>::value)
                    {
                        int other_newid = db->find_free(T::get_table_name(), CONSTASTR("id"));
                        ts::data_pair_s idp;
                        idp.name = CONSTASTR("id");
                        idp.type_ = ts::data_type_e::t_int;
                        idp.i = other_newid;
                        db->update(T::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(&idp, 1), ts::amake<uint>(CONSTASTR("id="), newid));
                        newid = other_newid;
                    }

                    new2ins[r.id] = newid;
                }
                r.id = newid;
                r.st = row_s::s_unchanged;
                if ( !all )
                    ++n_done;
            }
            continue;
        case row_s::s_delete:
            if ( n_done >= n_per_call ) return true;
            some_action = true;
            db->delrow(T::get_table_name(), r.id);
            r.st = row_s::s_deleted;
            if ( !all )
                ++n_done;
            cleanup_requred = true;
            continue;
        case row_s::s_temp:
            continue;
        }
    }

    __transaction.end();
    if (notify_saved && some_action) gmsg<ISOGM_PROFILE_TABLE_SL>( tabi, true ).send();
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
        if (read_ids) read_ids->add( static_cast<int>(v.i) );
        row_s &row = getcreate( static_cast<int>( v.i ) );
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
    gmsg<ISOGM_PROFILE_TABLE_SL>( tabi, false ).send();
}

template<typename T, profile_table_e tabi> void tableview_t<T, tabi>::read( ts::sqlitedb_c *db, const ts::asptr &where_items )
{
    if (db)
        db->read_table(T::get_table_name(), DELEGATE(this, reader), where_items);
}

ts::wstr_c& profile_c::path_by_name(ts::wstr_c &profname)
{
    profname.insert(0,CONSTWSTR("profile" NATIVE_SLASH_S ));
    profname.append(CONSTWSTR(".profile"));
    profname = ts::fn_change_name_ext(cfg().get_path(), profname);
    return profname;
}

ts::uint32 profile_c::gm_handler(gmsg<ISOGM_MESSAGE>&msg) // record history
{
    if (msg.resend || msg.info) return 0;
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
            g_app->undelivered_message( historian->getkey(), post );
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
    uint64 utag = prf().getuid();

    while(db)
    {
        REMOVE_CODE_REMINDER( 564 ); // always unique, no need 2 check it anymore

        ts::tmp_str_c whr(CONSTASTR("utag=")); whr.append_as_num<int64>(utag);
        if (0 == db->count( history_s::get_table_name(), whr))
            break;

        utag = prf().getuid();
    }

    return utag;
}

ts::flags32_s INLINE profile_c::get_options()
{
    if (!profile_flags.is(F_OPTIONS_VALID))
    {
        ts::flags32_s::BITS opts = msgopts();
        ts::flags32_s::BITS optse = msgopts_edited();
        current_options.setup((opts & optse) | (~optse & DEFAULT_MSG_OPTIONS));
        profile_flags.set(F_OPTIONS_VALID);
    }
    return current_options;
}

ts::flags32_s prf_options()
{
    return prf().get_options();
}

bool profile_c::set_options(ts::flags32_s::BITS mo, ts::flags32_s::BITS mask)
{
    ts::flags32_s::BITS cur = get_options().__bits;
    ts::flags32_s::BITS curmod = cur & mask;
    ts::flags32_s::BITS newbits = mo & mask;
    ts::flags32_s::BITS edited = msgopts_edited();
    edited |= (curmod ^ newbits);
    msgopts_edited(edited);
    cur = (cur & ~mask) | newbits;
    current_options.setup(cur);
    return msgopts(cur);
}


void profile_c::record_history( const contact_key_s&historian, const post_s &history_item )
{
    ASSERT(history_item.recv_time != 0);
    g_app->lock_recalc_unread(historian);
    auto &row = table_history.getcreate(0);
    row.other.historian = historian;
    static_cast<post_s &>(row.other) = history_item;

    ASSERT( !historian.is_conference() || historian.temp_type == TCT_CONFERENCE );
    ASSERT( historian.is_conference() || row.other.sender.temp_type == TCT_NONE );

    changed();
}

int profile_c::min_history_load(bool for_button) 
{
    return get_options().is(MSGOP_LOAD_WHOLE_HISTORY) ? -1 : (for_button ? add_history() : min_history());
}

void profile_c::kill_history_item(uint64 utag)
{
    if(auto *row = table_history.find<true>([&](history_s &h) ->bool { return h.utag == utag; }))
        if ( row->deleted() )
        {
            changed();
            return;
        }

    ts::tmp_str_c whr(CONSTASTR("utag=")); whr.append_as_num<int64>(utag);
    db->delrows( history_s::get_table_name(), whr);
}

void profile_c::kill_history(const contact_key_s&historian)
{
    unload_history(historian);
    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num( historian.dbvalue() );
    db->delrows( history_s::get_table_name(), whr );
}

void profile_c::unload_history( const contact_key_s&historian )
{
    for ( ts::aint i = table_history.rows.size()-1; i>=0; --i )
    {
        if ( table_history.rows.get(i).other.historian == historian )
            table_history.rows.remove_slow( i );
    }
}

void profile_c::change_history_items( const contact_key_s &historian, const contact_key_s &old_sender, const contact_key_s &new_sender )
{
    ts::db_transaction_c __transaction( db );

    table_history.cleanup();
    for ( auto &row : table_history )
        if ( row.other.historian == historian && row.other.sender == old_sender )
            row.other.sender = new_sender, row.changed();

    table_history.flush( db, true, false );

    ts::tmp_str_c whr( CONSTASTR( "historian=" ) ); whr.append_as_num( historian.dbvalue() );
    whr.append( CONSTASTR( " and sender=" ) ).append_as_num( old_sender.dbvalue() );

    ts::data_pair_s dp;
    dp.name = CONSTASTR( "sender" );
    dp.type_ = ts::data_type_e::t_int64;
    dp.i = new_sender.dbvalue();

    db->update( history_s::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>( &dp, 1 ), whr );
}

bool profile_c::change_history_item(uint64 utag, contact_key_s & historian)
{
    ts::db_transaction_c __transaction( db );

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

    db->update( history_s::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(&dp, 1), whr);

    return ok;
}

void profile_c::change_history_item(const contact_key_s&historian, const post_s &post, ts::uint32 change_what)
{
    if (!change_what) return;
    ts::db_transaction_c __transaction( db );

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

    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
    whr.append( CONSTASTR(" and utag=") ).append_as_num<int64>(ts::ref_cast<int64>(post.utag));
    whr.append( CONSTASTR(" and sender=") ).append_as_num(post.sender.dbvalue());

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
        dp[n].text = post.message_utf8->cstr();
        ++n;
    }
    ASSERT(n<=ARRAY_SIZE(dp));
    db->update( history_s::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(dp,n), whr );
}

void profile_c::kill_message( uint64 msgutag )
{
    if ( !db ) return;
    ts::db_transaction_c __transaction( db );

    ts::tmp_str_c whr( CONSTASTR( "utag=" ) ); whr.append_as_num( msgutag );
    db->delrows( history_s::get_table_name(), whr );

}

void profile_c::load_history( const contact_key_s&historian, allocpost *cb, void *prm )
{
    ts::tmp_str_c whr( CONSTASTR( "historian=" ) ); whr.append_as_num( historian.dbvalue() );
    whr.append( CONSTASTR( " order by mtime" ) );

    struct rht
    {
        allocpost *cb;
        void *prm;
        ts::data_value_s dv;

        bool dr( int row, ts::SQLITE_DATAGETTER dg )
        {
            dg( history_s::C_MSG, dv );

            post_s *p = cb( dv.text, prm );

            dg( history_s::C_RECV_TIME, dv );
            p->recv_time = dv.i;

            dg( history_s::C_CR_TIME, dv );
            p->cr_time = dv.i;

            //dg( history_s::C_HISTORIAN, dv );
            //p->historian = contact_key_s::buildfromdbvalue( dv.i );

            dg( history_s::C_SENDER, dv );
            p->sender = contact_key_s::buildfromdbvalue( dv.i, false );

            if ( p->type != 0 ) // hint! non zero p->type means conference ( so unclear, I know, I know )
            {
                // conference!
                dg( history_s::C_HISTORIAN, dv ); // read historian
                p->receiver = contact_key_s::buildfromdbvalue( dv.i, true ); // receiver value of conference history items is always conference itself

            } else
            {
                dg( history_s::C_RECEIVER, dv );
                p->receiver = contact_key_s::buildfromdbvalue( dv.i, false );
            }


            dg( history_s::C_TYPE_AND_OPTIONS, dv );
            p->set_type_and_options( dv.i );

            dg( history_s::C_UTAG, dv );
            p->utag = dv.i;

            TS_STATIC_CHECK( history_s::C_count == 9, "new fields?" );

            return true;
        }

    } r;

    r.cb = cb;
    r.prm = prm;

    db->read_table( history_s::get_table_name(), DELEGATE( &r, dr ), whr );
}

void profile_c::load_history( const contact_key_s&historian, time_t time, ts::aint nload, ts::tmp_tbuf_t<int>& loaded_ids )
{
    MEMT( MEMT_PROFILE_HISTORY );

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
        time = ts::now();
        time_t ct = time;
        ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
        whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>( time );
        table_history.read(db, whr);

        ts::db_transaction_c __transaction( db );

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

            db->update( history_s::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(dp, fixed ? 2 : 1), whr);
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

    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num( historian.dbvalue() );
    whr.append( CONSTASTR(" and mtime<") ).append_as_num<int64>( time );
    whr.append( CONSTASTR(" order by mtime desc limit ") ).append_as_num( nload );

    table_history.read( db, whr );

    for( int idl : loaded_ids)
    {
        auto * row = table_history.find<true>(idl);
        if (row && fix( row->other ))
        {
            whr.set(CONSTASTR("utag=")); whr.append_as_num<int64>(ts::ref_cast<int64>(row->other.utag));

            ts::data_pair_s dp;
            dp.name = CONSTASTR("mtype");
            dp.type_ = ts::data_type_e::t_int;
            dp.i = row->other.mt();

            db->update( history_s::get_table_name(), ts::array_wrapper_c<const ts::data_pair_s>(&dp, 1), whr);
        }
    }

    table_history.read_ids = nullptr;
}

void profile_c::load_history( const contact_key_s&historian )
{
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
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
        table_history.flush(db, true, false); // very important to save now
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
    ts::aint cnt = baseitems.size();
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
    if ( !db ) return 0;
    ts::tmp_str_c whr( CONSTASTR("historian=") ); whr.append_as_num( historian.dbvalue() );
    if (ignore_invites) whr.append( CONSTASTR(" and mtype<>2 and mtype<>103") ); // MTA_FRIEND_REQUEST MTA_OLD_REQUEST
    return db->count( history_s::get_table_name(), whr );
}

int  profile_c::calc_history( const contact_key_s&historian, const contact_key_s&sender )
{
    if ( !db ) return 0;
    ts::tmp_str_c whr( CONSTASTR( "historian=" ) ); whr.append_as_num( historian.dbvalue() );
    whr.append( CONSTASTR( " and sender=" ) ).append_as_num( sender.dbvalue() );
    return db->count( history_s::get_table_name(), whr );
}

int  profile_c::calc_history_before( const contact_key_s&historian, time_t time )
{
    if ( !db ) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
    whr.append( CONSTASTR(" and mtime<") ).append_as_num<int64>( time );
    return db->count( history_s::get_table_name(), whr);
}

int  profile_c::calc_history_after(const contact_key_s&historian, time_t time, bool only_messages)
{
    if (!db) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
    if (only_messages) whr.append( CONSTASTR(" and mtype==0") );
    whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>(time);
    return db->count( history_s::get_table_name(), whr);
}

int  profile_c::calc_history_between( const contact_key_s&historian, time_t time1, time_t time2 )
{
    if (!db) return 0;
    ts::tmp_str_c whr(CONSTASTR("historian=")); whr.append_as_num(historian.dbvalue());
    whr.append(CONSTASTR(" and mtime>=")).append_as_num<int64>(time1);
    whr.append(CONSTASTR(" and mtime<")).append_as_num<int64>(time2);
    return db->count( history_s::get_table_name(), whr);
}

ts::bitmap_c profile_c::load_avatar( const contact_key_s& ck )
{
    if ( !db ) return ts::bitmap_c();

    struct rdr
    {
        ts::blob_c avablob;
        bool dr( int row, ts::SQLITE_DATAGETTER dg )
        {
            ts::data_value_s dv;
            dg( contacts_s::C_AVATAR, dv );
            avablob = dv.blob;
            return true;
        }
    } r;


    ts::tmp_str_c whr( CONSTASTR( "contact_id=" ) );
    whr.append_as_num<int>( ck.contactid ).append( CONSTASTR( " and proto_id=" ) ).append_as_num<int>( ck.protoid );
    db->read_table( CONSTASTR( "contacts" ), DELEGATE( &r, dr ), whr );

    ts::bitmap_c bmp;
    if ( r.avablob )
        if ( bmp.load_from_file( r.avablob ) && bmp.info().bytepp() != 4)
        {
            ts::bitmap_c bmp4;
            bmp4 = bmp.extbody();
            bmp = bmp4;
        }
    bmp.premultiply();
    return bmp;
}

profile_load_result_e profile_c::xload(const ts::wstr_c& pfn, const ts::uint8 *k)
{
    MEMT( MEMT_PROFILE_COMMON );

    AUTOCLEAR( profile_flags, F_LOADING );
    profile_flags.clear(F_LOADED_TABLES);

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

    path = pfn;

    if (g_app->F_BACKUP_PROFILE())
    {
        g_app->F_BACKUP_PROFILE(false);
        ts::buf_c p; p.load_from_disk_file( path );
        ts::zip_c zip;
        zip.addfile( ts::to_utf8( ts::fn_get_name_with_ext( path ) ), p.data(), p.size() );

        ts::localtime_s tt;

        ts::wstr_c tstr( cfg().folder_backup() );
        path_expand_env( tstr, nullptr );
        ts::fix_path( tstr, FNO_APPENDSLASH| FNO_SIMPLIFY_NOLOWERCASE );
        ts::make_path( tstr, 0 );

        tstr.append( ts::fn_get_name( path ) ).append_char('_');
        ts::wstr_c filesmask(tstr);
        tstr.append_as_uint( tt.tm_year + 1900 ).append_char('-').append_as_uint( tt.tm_mon + 1, 2 ).append_char( '-' ).append_as_uint( tt.tm_mday, 2 );
        tstr.append_char('_').append_as_uint(tt.tm_hour, 2).append_char('-').append_as_uint(tt.tm_min, 2).append_char('-').append_as_uint(tt.tm_sec, 2);
        tstr.append( CONSTWSTR(".zip") );
        zip.getblob().save_to_file( tstr );

        int miscf = ~cfg().misc_flags();
        if (0 != ((MISCF_DONT_LIM_BACKUP_DAYS | MISCF_DONT_LIM_BACKUP_SIZE | MISCF_DONT_LIM_BACKUP_COUNT) & miscf))
        {
            filesmask.append(CONSTWSTR("*.zip"));
            ts::wstrings_c files;
            ts::find_files(filesmask, files, ATTR_ANY, ATTR_DIR, true);

            struct ff : public ts::movable_flag<true>
            {
                DUMMY(ff);
                ff() {}
                ts::wstr_c fn;
                uint64 filetime = 0;
                uint64 size = 0;

                int operator()(const ff& f) const
                {
                    if (filetime > f.filetime) return 1;
                    if (filetime < f.filetime) return -1;
                    return 0;
                }
            };
            ts::tmp_array_inplace_t<ff, 0> fls;

            for (const ts::wstr_c &fn : files)
            {
                if (void * fh = ts::f_open(fn))
                {
                    ff & f = fls.add();
                    f.fn = fn;
                    f.filetime = ts::f_time_last_write(fh);
                    f.size = ts::f_size(fh);
                    ts::f_close(fh);
                }
            }

            fls.dsort();

            ts::aint clampn = fls.size();
            if (0 != (MISCF_DONT_LIM_BACKUP_COUNT & miscf))
            {
                int cntlim = cfg().lim_backup_count();
                if (cntlim > 0 && cntlim < clampn)
                    clampn = cntlim;
            }
            if (0 != (MISCF_DONT_LIM_BACKUP_SIZE & miscf))
            {
                uint64 mb = cfg().lim_backup_size();
                mb *= 1024 * 1024;
                uint64 sz = fls.get(0).size;
                for (int i=1;i<clampn;++i)
                {
                    sz += fls.get(i).size;
                    if (sz > mb)
                    {
                        clampn = i;
                        break;
                    }
                }
            }
            if (0 != (MISCF_DONT_LIM_BACKUP_DAYS & miscf))
            {
                uint64 days = cfg().lim_backup_days();
                days *= 86400; // 86400 is number of seconds per day
                days *= 10000000; // 10000000 is number of 100 nanosecond intervals per second
                uint64 deltime = fls.get(0).filetime - days; // time of current backup - days
                for (; clampn > 0; --clampn)
                {
                    if (fls.get(clampn-1).filetime >= deltime)
                        break;
                }
            }

            if (clampn > 0)
            {
                while (clampn < fls.size())
                {
                    ts::kill_file(fls.last().fn);
                    fls.remove_fast(fls.size() - 1);
                }
            }
        }
    }

    profile_flags.clear();
    values.clear();
    dirty.clear();
    db = ts::sqlitedb_c::connect(path, k, g_app->F_READONLY_MODE());
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

    {
        MEMT( MEMT_PROFILE_CONF );
        db->read_table( CONSTASTR( "conf" ), get_cfg_reader() );
    }

    ts::str_c utag = unique_profile_tag();
    bool generated = false;
    if (utag.is_empty())
        utag.set_as_num<uint64>( random64() ), generated = true;

    mutex.reset( ts::master().sys_global_atom( CONSTWSTR( "isotoxin_db_" ) + ts::to_wstr( utag ) ) );
    if (!mutex)
    {
        db->close();
        db = nullptr;
        return PLR_BUSY;
    }
    if (generated)
        unique_profile_tag( utag );

    cleanup_tables();

    #define TAB(tab) if (load_on_start<tab##_s>::value) { MEMT( MEMT_PROFILE_##tab ); table_##tab.read( db ); }
    PROFILE_TABLES
    #undef TAB

    profile_flags.set( F_LOADED_TABLES );

    uuid = get<uint64>( CONSTASTR( "uuid" ), 0 );

    if ( uuid == 0 )
    {
        REMOVE_CODE_REMINDER( 564 );

        ++uuid;
        decltype( table_history ) tmphist;

        ts::tmp_str_c whr;
        tmphist.read( db, whr );
        for ( auto &row : tmphist )
            row.other.utag = uuid++, row.changed();

        tmphist.flush(db,true,false);

        param( CONSTASTR( "uuid" ), ts::tmp_str_c().set_as_num<uint64>( uuid ) );
    }

    {

        REMOVE_CODE_REMINDER(603);

        auto fix = [](const ts::wstr_c &p)
        {
            ts::wstr_c s(p);
            s.replace_all(CONSTWSTR("%CONFIG%"), CONSTWSTR(PATH_MACRO_CONFIG));
            s.replace_all(CONSTWSTR("%CONTACTID%"), CONSTWSTR(PATH_MACRO_CONTACTID));
            return s;
        };

        download_folder(fix(download_folder()));
        download_folder_images(fix(download_folder_images()));
        download_folder_images(fix(download_folder_images()));

    }


    load_undelivered();

    contact_c &self = contacts().get_self();
    self.set_name(username());
    self.set_statusmsg(userstatus());

    gmsg<ISOGM_PROFILE_TABLE_SL>( pt_active_protocol, true ).send(); // initiate active protocol reconfiguration/creation

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
    load_protosort();

    return PLR_OK;
}

void profile_c::load_undelivered()
{
    ts::tmp_str_c whr(CONSTASTR("mtype=")); whr.append_as_int(MTA_UNDELIVERED_MESSAGE);

    tableview_history_s table;
    table.read(db, whr);

    for (const auto &row : table.rows)
        g_app->undelivered_message(row.other.historian, row.other);

}

contact_root_c *profile_c::find_corresponding_historian(const contact_key_s &subcontact, ts::array_wrapper_c<contact_root_c * const> possible_historians) //-V813
{
    ts::tmp_str_c whr(CONSTASTR("sender=")); whr.append_as_num(subcontact.dbvalue());
    whr.append(CONSTASTR(" or receiver=")).append_as_num(subcontact.dbvalue());

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

uint64 profile_c::getuid( uint cnt )
{
    ASSERT( uuid > 0 );

    uint64 r = uuid;
    uuid += cnt;

    if (cnt <= num_locked_uids)
    {
        num_locked_uids -= cnt;
    } else
    {
        param( CONSTASTR( "uuid" ), ts::tmp_str_c().set_as_num<uint64>( r + cnt + 101 ) );
        num_locked_uids = 101;
    }

    return r;
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

        /*virtual*/ int iterate(ts::task_executor_c *) override
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

            ts::task_c::done(canceled);
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
                    dialog_msgbox_c::mb_error(TTT("Your profile has not been encrypted. Error code: $", 378) / ts::wmake(e)).summon(true);
                else
                {
                    if (enc)
                        dialog_msgbox_c::mb_info(TTT("Your profile has been successfully encrypted. After [appname] restart, you will be prompted for password. Don't forget it!", 381)).summon( true );
                    else
                        dialog_msgbox_c::mb_info(TTT("Your profile has been successfully decrypted.", 377)).summon( true );
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
            SUMMON_DIALOG<dialog_pb_c>(UD_ENCRYPT_PROFILE_PB, true, dialog_pb_c::params(
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
    if (g_app->F_READONLY_MODE())
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
            if (g_app->F_READONLY_MODE())
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
            ts::master().activewindow = nullptr;
            ts::master().mainwindow = nullptr;
            ts::master().sys_exit(10);
        }
    };

    redraw_collector_s dch;
    dialog_msgbox_c::mb_error( text )
        .bok( modal ? loc_text(loc_exit) : ts::wstr_c() )
        .on_ok(modal ? s::exit_now : MENUHANDLER(), ts::str_c())
        .summon( true );
}

bool profile_c::addeditnethandler(dialog_protosetup_params_s &params)
{
    active_protocol_data_s *apd = nullptr;
    int id = 0;
    if (params.protoid == 0)
    {
        auto &r = get_table_active_protocol().getcreate(0);
        id = r.id;
        params.to( r.other );
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

void profile_c::flush_contacts()
{
    for ( const contact_key_s &ck : dirtycontacts )
    {
        const contact_c * c = contacts().find( ck );

        if (c->getkey().is_conference())
            continue; // never save conferences here

        auto *row = table_contacts.find<false>( [&]( const contacts_s &k )->bool { return k.key == ck; } );
        if ( c && !c->flag_dip )
        {
            if ( !row )
            {
                row = &table_contacts.getcreate( 0 );
                row->other.key = ck;
                row->other.avatar_tag = 0;
            }
            else
            {
                if ( row->is_deleted() )
                    continue;

                row->changed();
            }
            if ( !c->save( &row->other ) )
                row->deleted();
        } else if (row)
        {
            row->deleted();
        }
    }
    dirtycontacts.clear();

}

bool profile_c::flush_tables()
{
    ts::db_transaction_c __transaction( db );

#define TAB(tab) if (table_##tab.flush(db, false)) return true;
    PROFILE_TABLES
#undef TAB

    if ( db )
    {
        time_t oldbackuptime = ts::now() - backup_keeptime() * 86400;
        db->delrows( backup_protocol_s::get_table_name(), ts::tmp_str_c( CONSTASTR( "time<" ) ).append_as_num<int64>( oldbackuptime ) );
    }
    return false;
}

/*virtual*/ bool profile_c::save()
{
    if (profile_flags.is(F_ENCRYPT_PROCESS))
    {
        ts::sys_sleep(1);
        return false;
    }

    MEMT( MEMT_PROFILE_COMMON );

    if (super::save()) return true;

    flush_contacts();
    if ( flush_tables() ) return true;

    return false;
}

void profile_c::check_aps()
{
    bool createaps = false;
    ts::aint cnt = protocols.size();
    for( ts::aint i=cnt-1;i>=0;--i)
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

ts::uint32 profile_c::gm_handler( gmsg<ISOGM_PROFILE_TABLE_SL>&p )
{
    if (!p.saved)
        return 0;

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

void profile_c::del_folder_share(uint64 utag)
{
    if (auto *row = prf().get_table_folder_share().find<true>([utag](folder_share_s &fsh) {

        if (fsh.utag == utag)
            return true;
        return false;
    })) {
        if (row->deleted()) prf().changed();
    }
}

void profile_c::del_folder_share_by_historian(const contact_key_s &h)
{
    bool ch = false;
    
    for (auto &row : prf().get_table_folder_share())
    {
        if (row.other.historian == h)
        {
            ch |= row.deleted();
        }
    }

    if (ch)
        prf().changed();
}

bool profile_c::delete_conference( int id )
{
    ASSERT( profile_flags.is( F_LOADED_TABLES ) );

    get_table_conference().cleanup();

    for ( auto &c : get_table_conference() )
    {
        if ( c.other.id == id )
        {
            if (active_protocol_c *ap = prf().ap( c.other.proto_id ))
            {
                if (c.is_temp())
                {
                    ap->del_contact( contact_key_s( c.other.proto_key, c.other.proto_id ) );
                } else
                {
                    ap->del_conference( c.other.pubid );
                }
            }
            c.other.confa = nullptr;
            if (c.deleted())
                changed();
            return true;
        }
    }

    return false;
}

conference_s * profile_c::find_conference(const ts::asptr &pubid, ts::uint16 apid)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));

    if (pubid.l == 0)
        return nullptr;

    get_table_conference().cleanup();

    for (auto &c : get_table_conference())
        if (c.other.proto_id == apid && c.other.pubid == pubid)
            return &c.other;
    return nullptr;
}


conference_s * profile_c::find_conference(contact_id_s ck, ts::uint16 protoid)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));
    ASSERT(ck.is_conference());

    get_table_conference().cleanup();

    for (auto &c : get_table_conference())
        if (c.other.proto_key == ck && c.other.proto_id == protoid)
            return &c.other;
    return nullptr;
}

conference_s * profile_c::find_conference_by_id(int id)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));

    get_table_conference().cleanup();

    for (auto &c : get_table_conference())
        if (c.other.id == id)
            return &c.other;
    return nullptr;
}


conference_s * profile_c::add_conference( const ts::str_c &pubid, contact_id_s protocol_key, ts::uint16 protoid )
{
    ASSERT( protocol_key.is_conference() );
    ASSERT( find_conference(pubid, protoid) == nullptr && find_conference(protocol_key, protoid) == nullptr );

    get_table_conference().cleanup();

    auto is_present = [this]( int id )
    {
        for ( const auto &c : get_table_conference() )
            if ( c.other.id == id )
                return true;
        return false;
    };


    int newid = 1;
    for ( ; is_present( newid ); ++newid );

    auto &nc = get_table_conference().getcreate(0);
    nc.other.pubid = pubid;
    nc.other.proto_key = protocol_key;
    nc.other.proto_id = protoid;
    nc.other.id = newid;

    nc.other.flags.init( conference_s::F_MIC_ENABLED, !get_options().is( COPT_MUTE_MIC_ON_INVITE ) );
    nc.other.flags.init( conference_s::F_SPEAKER_ENABLED, !get_options().is( COPT_MUTE_SPEAKER_ON_INVITE ) );

    if (nc.other.pubid.is_empty())
    {
        nc.other.pubid.set( CONSTASTR( "temp-" ) ).append_as_num( prf().getuid() );
        nc.temp();
    }

    changed();

    return &nc.other;
}

conference_member_s * profile_c::find_conference_member(contact_id_s member_protokey, ts::uint16 protoid)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));
    ASSERT(member_protokey.is_contact() && member_protokey.unknown && member_protokey.confmember); // abstract conference member has no conference id (gid)

    get_table_conference_member().cleanup();

    for (auto &cm : get_table_conference_member())
        if (cm.other.proto_key == member_protokey && cm.other.proto_id == protoid)
            return &cm.other;

    return nullptr;
}

conference_member_s * profile_c::find_conference_member(const ts::asptr &pubid, ts::uint16 apid)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));

    get_table_conference_member().cleanup();

    for (auto &cm : get_table_conference_member())
        if (cm.other.proto_id == apid && cm.other.pubid == pubid)
            return &cm.other;

    return nullptr;
}

conference_member_s * profile_c::find_conference_member_by_id(int id)
{
    ASSERT(profile_flags.is(F_LOADED_TABLES));

    get_table_conference_member().cleanup();

    for (auto &cm : get_table_conference_member())
        if (cm.other.id == id)
            return &cm.other;

    return nullptr;
}

conference_member_s * profile_c::add_conference_member( const ts::str_c &pubid, contact_id_s protocol_key, ts::uint16 protoid )
{
    ASSERT(protocol_key.unknown && protocol_key.confmember);
    ASSERT(find_conference_member(pubid, protoid) == nullptr && (protocol_key.is_empty() || find_conference_member(protocol_key, protoid) == nullptr));

    get_table_conference_member().cleanup();

    auto is_present = [this]( int id )
    {
        for ( const auto &c : get_table_conference_member() )
            if ( c.other.id == id )
                return true;
        return false;
    };


    int newid = 1;
    for ( ; is_present( newid ); ++newid );

    auto &ncm = get_table_conference_member().getcreate( 0 );
    ncm.other.pubid = pubid;
    ncm.other.proto_key = protocol_key;
    ncm.other.proto_id = protoid;
    ncm.other.id = newid;

    ASSERT( !ncm.other.pubid.is_empty() );

    changed();

    return &ncm.other;
}

bool profile_c::delete_conference_member( int id )
{
    ASSERT( profile_flags.is( F_LOADED_TABLES ) );

    get_table_conference_member().cleanup();

    for ( auto &c : get_table_conference_member() )
    {
        if ( c.other.id == id )
        {
            c.other.memba = nullptr;
            if ( c.deleted() )
                changed();
            return true;
        }
    }

    return false;
}

bool profile_c::is_conference_member( const contact_key_s &ck )
{
    if ( ck.temp_type == TCT_UNKNOWN_MEMBER )
        return nullptr != find_conference_member_by_id( ck.contactid );

    get_table_conference().cleanup();

    for ( const auto &confa : get_table_conference() )
        if ( calc_history( confa.other.history_key(), ck ) > 0 )
            return true;
    return false;
}

void profile_c::make_contact_unknown_member( contact_c * c )
{
    if (c->get_state() != CS_OFFLINE && c->get_state() != CS_ONLINE || find_conference_member(c->get_pubid(), c->getkey().protoid) != nullptr)
        return;

    ts::db_transaction_c __transaction( db );

    contact_key_s oldkey = c->getkey();
    killcontact( oldkey ); // remove from db
    conference_member_s * cm = add_conference_member( c->get_pubid(), contact_id_s(), oldkey.protoid );
    cm->memba = c;
    cm->update_name();
    c->make_unknown_conference_member( cm->id );

    get_table_conference().cleanup();

    // fix history of conferences
    for ( const auto &confa : get_table_conference() )
    {
        if ( confa.other.confa )
        {
            ASSERT( confa.other.confa->getkey() == confa.other.history_key() );
            confa.other.confa->change_history_items( oldkey, c->getkey() );

        } else
            change_history_items( confa.other.history_key(), oldkey, c->getkey() );
    }
}

void profile_c::make_contact_known( contact_c * c, const contact_key_s &key )
{
    ts::db_transaction_c __transaction( db );
    contact_key_s oldkey = c->getkey();

    contacts().change_key( oldkey, key);

    get_table_conference().cleanup();

    // fix history of conferences
    for ( const auto &confa : get_table_conference() )
    {
        if ( confa.other.confa )
        {
            ASSERT( confa.other.confa->getkey() == confa.other.history_key() );
            confa.other.confa->change_history_items( oldkey, c->getkey() );

        }
        else
            change_history_items( confa.other.history_key(), oldkey, c->getkey() );
    }

}

void profile_c::load_protosort()
{
    renew(protogroupsortdata);
    ts::str_c ps( protosort() );

    if ( !ps.is_empty() )
    {
        protogroupsortdata.set_count( ps.count_chars( ',' ) + 1, false );
        ts::uint16 *data = protogroupsortdata.data16();

        for ( ts::token<char> p( ps, ',' ); p; ++p )
            *data++ = p->as_num<ts::uint16>();
    }
}

void profile_c::save_protosort()
{
    ts::astrings_c ss;

    bool seq_start = false;
    ts::tbuf_t<ts::uint16> seq;
    ts::str_c tmp;
    int ff = 0;
    for ( ts::uint16 v : protogroupsortdata )
    {
        if (!seq_start && v == 0)
        {
            seq.clear();
            seq_start = true;
            continue;
        }

        if (!seq_start)
            continue;

        if ( v == 0 )
        {
            seq_start = false;
            seq.qsort();
            tmp.set( CONSTASTR("0,") );
            if (ff == 2 && seq.count() == 0)
                tmp.append( CONSTASTR("$ffff,$ffff,") );
            else
                for ( ts::uint16 vv : seq )
                    tmp.append_as_num( vv ).append_char(',');
            tmp.append_char( '0' );
            ss.get_string_index(tmp.as_sptr());
            continue;
        }

        if (ap( v ) != nullptr)
            seq.add( v );
        else if (v == 0xffff)
            ++ff;
    }
    protosort( ss.join(',') );
}

static void setup_prots( ts::tmp_tbuf_t<ts::uint16>& prots, const ts::uint16 * set_of_prots, ts::aint cnt )
{
    if (cnt)
    {
        prots.set_count( cnt + 2 );
        prots.set( (ts::aint)0, (ts::uint16)0 );
        prots.set( cnt + 1, 0 );
        memcpy( prots.data16() + 1, set_of_prots, sizeof( ts::uint16 ) * cnt );
    }
    else
    {
        prots.set_count( 4 );
        prots.set( (ts::aint)0, (ts::uint16)0 );
        prots.set( (ts::aint)1, (ts::uint16)0xffff );
        prots.set( (ts::aint)2, (ts::uint16)0xffff );
        prots.set( (ts::aint)3, (ts::uint16)0 );
    }
}

ts::aint profile_c::protogroupsort( const ts::uint16 * set_of_prots, ts::aint cnt )
{
    ts::tmp_tbuf_t<ts::uint16> prots;
    setup_prots( prots, set_of_prots, cnt );

    ts::aint i = protogroupsortdata.find_offset( prots.data16(), prots.count() );
    if (i >= 0)
        return i;

    i = protogroupsortdata.count();
    protogroupsortdata.append_buf(prots);

    return i;
}
bool profile_c::protogroupsort_up( const ts::uint16 * set_of_prots, ts::aint cnt, bool test )
{
    ts::tmp_tbuf_t<ts::uint16> prots;
    setup_prots( prots, set_of_prots, cnt );

    ts::aint i = protogroupsortdata.find_offset( prots.data16(), prots.count() );
    if (i <= 0) return false;

    if (!test)
    {
        ASSERT( protogroupsortdata.get( i-1 ) == 0 );
        ts::aint seek = i - 2;
        for (; seek >= 0; --seek)
            if (protogroupsortdata.get( seek ) == 0)
                break;
        ASSERT( seek >= 0 );
        protogroupsortdata.cut( i * sizeof(ts::uint16), prots.byte_size() );
        memcpy( protogroupsortdata.expand( seek * sizeof( ts::uint16 ), prots.byte_size() ), prots.data(), prots.byte_size() );
        save_protosort();
    }

    return true;
}
bool profile_c::protogroupsort_dn( const ts::uint16 * set_of_prots, ts::aint cnt, bool test )
{
    ts::tmp_tbuf_t<ts::uint16> prots;
    setup_prots( prots, set_of_prots, cnt );

    ts::aint i = protogroupsortdata.find_offset( prots.data16(), prots.count() );
    if (i >= protogroupsortdata.count() - prots.count()) return false;

    if (!test)
    {
        protogroupsortdata.cut( i * sizeof( ts::uint16 ), prots.byte_size() );

        ASSERT( protogroupsortdata.get( i ) == 0 );
        ts::aint seek = i+1;
        for (ts::aint pcnt = protogroupsortdata.count(); seek < pcnt; ++seek)
            if (protogroupsortdata.get( seek ) == 0)
                break;
        ++seek;
        ASSERT( seek <= protogroupsortdata.count() );
        memcpy( protogroupsortdata.expand( seek * sizeof( ts::uint16 ), prots.byte_size() ), prots.data(), prots.byte_size() );
        save_protosort();
    }

    return true;
}

ts::wstr_c profile_c::download_folder_prepared(const contact_root_c *r)
{
    ts::wstr_c downf = download_folder();
    downf.replace_all(CONSTWSTR(PATH_MACRO_DOWNLOAD), ts::wsptr()); // avoid recursion
    path_expand_env(downf, r);
    return downf;
}

void profile_c::cleanup_tables()
{
    struct g
    {
        ts::tmp_tbuf_t<int64> hst;
        void gg( const ts::data_value_s& v )
        {
            hst.add( v.i );
        }
        bool ggx( int row, ts::SQLITE_DATAGETTER dg )
        {
            ts::data_value_s dv;
            dg( contacts_s::C_CONTACT_ID, dv );
            hst.find_remove_fast( dv.i );
            return true;
        }
        bool ggy( int row, ts::SQLITE_DATAGETTER dg )
        {
            ts::data_value_s dv;
            dg( conference_s::C_ID, dv );
            int64 cid = dv.i;
            dg( conference_s::C_APID, dv );
            int64 k = (cid << 32) | dv.i; // see CONFDBVAL
            hst.find_remove_fast( k );
            return true;
        }
        bool ggz( int row, ts::SQLITE_DATAGETTER dg )
        {
            ts::data_value_s dv;
            dg( conference_member_s::C_ID, dv );
            int64 cid = dv.i;
            dg( conference_member_s::C_APID, dv );
            int64 k = -((dv.i << 32) | cid); // see MEMBDBVAL
            hst.add(k);
            return true;
        }
    } ggg;

    db->unique_values( history_s::get_table_name(), DELEGATE( &ggg, gg ), CONSTASTR( "historian" ) );
    db->read_table( contacts_s::get_table_name(), DELEGATE( &ggg, ggx ) );
    db->read_table( conference_s::get_table_name(), DELEGATE( &ggg, ggy ) );

    if (ggg.hst.count())
    {
        db->delrows( history_s::get_table_name(), ts::str_c( CONSTASTR( "historian=" ) ).append_as_num( ggg.hst.get(0) ) );
        return;
    }

    ggg.hst.clear();
    db->read_table( conference_member_s::get_table_name(), DELEGATE( &ggg, ggz ) );

    for(int64 k : ggg.hst)
    {
        if (0 == db->count( history_s::get_table_name(), ts::str_c( CONSTASTR( "sender=" ) ).append_as_num( k ) ))
        {
            db->delrows( conference_member_s::get_table_name(), ts::str_c( CONSTASTR( "membid=" ) ).append_as_num( (uint64)(-k) & 0xffffffff ) ); // see MEMBDBVAL
            return;
        }
    }

}


#ifdef _DEBUG
void profile_c::test()
{
}
#endif // _DEBUG
