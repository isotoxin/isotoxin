#pragma once


#define PROFILE_TABLES \
    TAB( active_protocol ) \
    TAB( conference ) \
    TAB( conference_member ) \
    TAB( contacts ) \
    TAB( history ) \
    TAB( unfinished_file_transfer ) \
    TAB( backup_protocol ) \


enum profile_table_e {
#define TAB(tab) pt_##tab,
PROFILE_TABLES
#undef TAB
    pt_count };

enum profile_options_e : unsigned
{
    MSGOP_SHOW_DATE             = SETBIT(0),
    MSGOP_SHOW_DATE_SEPARATOR   = SETBIT(1),
    MSGOP_SHOW_PROTOCOL_NAME    = SETBIT(2),
    MSGOP_KEEP_HISTORY          = SETBIT(3),
    CLOPT_GROUP_CONTACTS_PROTO  = SETBIT(4),
    MSGOP_SEND_TYPING           = SETBIT(5),
    UIOPT_TAGFILETR_BAR         = SETBIT(6),
    MSGOP_LOAD_WHOLE_HISTORY    = SETBIT(7),
    MSGOP_FULL_SEARCH           = SETBIT(8),
    MSGOP_REPLACE_SHORT_SMILEYS = SETBIT(9),
    MSGOP_MAXIMIZE_INLINE_IMG   = SETBIT(10),
    MSGOP_SPELL_CHECK           = SETBIT(11),

    UIOPT_GEN_IDENTICONS        = SETBIT(12),

    UIOPT_INTRUSIVE_BEHAVIOUR   = SETBIT(13),
    UIOPT_SHOW_INCOMING_MSG_PNL = SETBIT(14),
    UIOPT_SHOW_INCOMING_CALL_BAR = SETBIT(15),

    UIOPT_SHOW_SEARCH_BAR       = SETBIT(16),
    UIOPT_PROTOICONS            = SETBIT(17),
    UIOPT_AWAYONSCRSAVER        = SETBIT(18),
    UIOPT_SHOW_NEWCONN_BAR      = SETBIT(19),
    UIOPT_SHOW_TYPING_CONTACT   = SETBIT(20),
    UIOPT_SHOW_TYPING_MSGLIST   = SETBIT(21),
    UIOPT_KEEPAWAY              = SETBIT(22),

    MSGOP_SHOW_INLINE_IMG       = SETBIT(23),

    COPT_MUTE_MIC_ON_INVITE     = SETBIT(24),
    COPT_MUTE_SPEAKER_ON_INVITE = SETBIT(25),

    SNDOPT_MUTE_ON_AWAY         = SETBIT(26),
    SNDOPT_MUTE_ON_DND          = SETBIT(27),

    OPTOPT_POWER_USER           = SETBIT(31),
};

#define DEFAULT_MSG_OPTIONS (MSGOP_SHOW_DATE_SEPARATOR|MSGOP_SHOW_PROTOCOL_NAME|MSGOP_KEEP_HISTORY|MSGOP_SEND_TYPING|MSGOP_FULL_SEARCH|UIOPT_SHOW_SEARCH_BAR|UIOPT_TAGFILETR_BAR|UIOPT_AWAYONSCRSAVER | UIOPT_SHOW_NEWCONN_BAR | COPT_MUTE_MIC_ON_INVITE | UIOPT_SHOW_TYPING_CONTACT | UIOPT_SHOW_TYPING_MSGLIST | MSGOP_MAXIMIZE_INLINE_IMG | MSGOP_SPELL_CHECK | UIOPT_SHOW_INCOMING_CALL_BAR|UIOPT_SHOW_INCOMING_MSG_PNL|SNDOPT_MUTE_ON_DND|UIOPT_GEN_IDENTICONS|MSGOP_SHOW_INLINE_IMG)

enum profile_misc_flags_e
{
    PMISCF_SAVEAVATARS = 1,
};

enum dsp_flags_e
{
    DSP_MIC_NOISE = SETBIT( 0 ),
    DSP_MIC_AGC = SETBIT( 1 ),
    DSP_SPEAKERS_NOISE = SETBIT( 2 ),
    DSP_SPEAKERS_AGC = SETBIT( 3 ),

    DSP_MIC = DSP_MIC_NOISE | DSP_MIC_AGC,
    DSP_SPEAKER = DSP_SPEAKERS_NOISE | DSP_SPEAKERS_AGC
};

enum use_proxy_for_e
{
    USE_PROXY_FOR_AUTOUPDATES = SETBIT(0),
    USE_PROXY_FOR_LOAD_SPELLING_DICTS = SETBIT(1),

    UPF_CURRENT_MASK = 3 // just | of all bits
};

struct active_protocol_s : public active_protocol_data_s
{
    void set(int column, ts::data_value_s& v);
    void get(int column, ts::data_pair_s& v);

    static const int maxid = 65000;
    static const int columns = 1 + 10; // tag, name, uname, ustatus, conf, options, avatar, sortfactor, login, password
    static ts::asptr get_table_name() {return CONSTASTR("protocols");}
    static void get_column_desc(int index, ts::column_desc_s&cd);
    static ts::data_type_e get_column_type(int index);
};

struct backup_protocol_s
{
    MOVABLE( true );
    time_t time;
    int tick;
    int protoid;
    ts::blob_c config;

    void set(int column, ts::data_value_s& v);
    void get(int column, ts::data_pair_s& v);

    static const int columns = 1 + 4; // time, tick, protoid, conf
    static ts::asptr get_table_name() { return CONSTASTR("backup_protocols"); }
    static void get_column_desc(int index, ts::column_desc_s&cd);
    static ts::data_type_e get_column_type(int index);
};

struct contacts_s
{
    MOVABLE( true );
    enum column_e
    {
        C_ID,
        C_CONTACT_ID,
        C_PROTO_ID,
        C_META_ID,
        C_OPTIONS,
        C_NAME,
        C_STATUSMSG,
        C_READTIME,
        C_CUSTOMNAME,
        C_GREETING,
        C_COMMENT,
        C_TAGS,
        C_MESSAGEHANDLER,
        C_AVATAR,
        C_AVATAR_TAG,

        C_count,
    };

    contact_key_s key;
    int metaid;
    int options; // 16:17 - keeph, 19:20 - aaac, 21:24 - mht
    ts::str_c name;
    ts::str_c statusmsg;
    ts::str_c customname;
    ts::str_c comment;
    ts::str_c greeting;
    ts::str_c msghandler;
    ts::astrings_c tags;
    time_t readtime; // time of last seen message
    ts::blob_c avatar;
    int avatar_tag;

    void set(int column, ts::data_value_s& v);
    void get(int column, ts::data_pair_s& v);

    static const int columns = C_count; // contact_id, proto_id, meta_id, options, name, statusmsg, readtime, customname, greeting, comment, tags, msghandler, avatar, avatar_tag
    static ts::asptr get_table_name() { return CONSTASTR("contacts"); }
    static void get_column_desc(int index, ts::column_desc_s&cd);
    static ts::data_type_e get_column_type(int index);
};

struct history_s : public post_s
{
    enum column_e
    {
        C_ID,
        C_RECV_TIME,
        C_CR_TIME,
        C_HISTORIAN,
        C_SENDER,
        C_RECEIVER,
        C_TYPE_AND_OPTIONS,
        C_MSG,
        C_UTAG,

        C_count
    };

    contact_key_s historian;

    void set(int column, ts::data_value_s& v);
    void get(int column, ts::data_pair_s& v);

    static const int columns = C_count; // mtime, crtime, historian, sender, receiver, mtype, msg, utag
    static ts::asptr get_table_name() { return CONSTASTR("history"); }
    static void get_column_desc(int index, ts::column_desc_s&cd);
    static ts::data_type_e get_column_type(int index);
};

typedef post_s * allocpost( const ts::asptr&t, void *prm );

struct unfinished_file_transfer_s
{
    MOVABLE( true );
    contact_key_s historian;
    contact_key_s sender;
    uint64 utag = 0;
    uint64 filesize = 0;
    uint64 msgitem_utag = 0;
    ts::wstr_c filename; // full filename (with path)
    ts::wstr_c filename_on_disk;
    bool upload = false; // true - upload, false - download

    void set(int column, ts::data_value_s& v);
    void get(int column, ts::data_pair_s& v);

    static const int columns = 1 + 8; // historian, sender, filename, filename_on_disk, size, utag, gui_utag, upload
    static ts::asptr get_table_name() { return CONSTASTR("transfer"); }
    static void get_column_desc(int index, ts::column_desc_s&cd);
    static ts::data_type_e get_column_type(int index);
};

struct conference_s
{
    MOVABLE( true );

    enum column_e
    {
        C_PUBID = 1,
        C_NAME,
        C_ID,
        C_APID,
        C_READTIME,
        C_FLAGS,
        C_COMMENT,
        C_TAGS,
        C_KEYWORDS,

        C_count
    };

    ts::shared_ptr< contact_root_c > confa;
    UNIQUE_PTR( text_match_c ) textmatchkeywords;

    time_t readtime = 0;
    ts::str_c pubid;
    ts::str_c name;
    ts::str_c comment;
    ts::wstr_c keywords;
    contact_key_s proto_key;
    ts::astrings_c tags;
    ts::flags32_s flags;
    int id = 0;

    static const ts::flags32_s::BITS F_SUPPRESS_NOTIFICATIONS = SETBIT(0);
    static const ts::flags32_s::BITS F_MIC_ENABLED = SETBIT(1);
    static const ts::flags32_s::BITS F_SPEAKER_ENABLED = SETBIT(2);
    static const ts::flags32_s::BITS F_COLLAPSED = SETBIT(3);

    void set( int column, ts::data_value_s& v );
    void get( int column, ts::data_pair_s& v );

    static const int columns = C_count; // pubid, name, id, apid, readtime, flags, comment
    static ts::asptr get_table_name() { return CONSTASTR( "conferences" ); }
    static void get_column_desc( int index, ts::column_desc_s&cd );
    static ts::data_type_e get_column_type( int index );

    contact_key_s history_key() const
    {
        return contact_key_s( TCT_CONFERENCE, id, proto_key.protoid );
    }
    bool update_name();
    bool update_comment();
    bool update_readtime();
    bool update_tags();
    bool update_keeph();
    bool change_flag( ts::flags32_s::BITS mask, bool val );

    void change_keywords( const ts::wstr_c& newkeywords );
    bool is_hl_message( const ts::wsptr &message, const ts::wsptr &my_name ) const;
};


struct conference_member_s
{
    MOVABLE( true );

    ts::shared_ptr< contact_c > memba;

    ts::str_c pubid;
    ts::str_c name;
    contact_key_s proto_key;
    int id;

    void set( int column, ts::data_value_s& v );
    void get( int column, ts::data_pair_s& v );

    static const int columns = 1 + 4; // pubid, name, id, apid
    static ts::asptr get_table_name() { return CONSTASTR( "confa_members" ); }
    static void get_column_desc( int index, ts::column_desc_s&cd );
    static ts::data_type_e get_column_type( int index );

    contact_key_s history_key() const
    {
        return contact_key_s( TCT_UNKNOWN_MEMBER, id, proto_key.protoid );
    }
    bool update_name();
};

template<typename T> struct load_on_start { static const bool value = true; };
template<> struct load_on_start<history_s> { static const bool value = false; };
template<> struct load_on_start<backup_protocol_s> { static const bool value = false; };

template<typename T, profile_table_e tabi> struct tableview_t
{
    typedef T ROWTYPE;
    static const profile_table_e tabt = tabi;
    struct row_s
    {
        MOVABLE( ts::is_movable<T>::value );
        DUMMY(row_s);
        row_s() {}
        int id = 0;
        enum { s_unchanged, s_changed, s_new, s_delete, s_deleted, s_temp } st = s_new;
        T other;

        bool present() const { return st == s_unchanged || st == s_changed || st == s_new; }
        void changed() { st = s_changed; };
        bool deleted()
        {
            if (st == s_deleted) 
                return false;

            if (st == s_new || st == s_temp)
            {
                st = s_deleted;
                return false;
            }
            bool c = st != s_delete;
            st = s_delete; return c;
        };
        bool is_deleted() const {return st == s_delete || st == s_deleted;}
        void temp() { st = s_temp; }
        bool is_temp() const { return st == s_temp; }
    };
    ts::tmp_tbuf_t<int> *read_ids = nullptr;
    ts::array_inplace_t<row_s, 0> rows;
    ts::hashmap_t<int, int> new2ins; // new id (negative) to inserted. valid after save
    int newidpool = -1;
    bool cleanup_requred = false;

    void cleanup()
    {
        // remove s_deleted
        if (cleanup_requred)
            for( ts::aint i=rows.size()-1; i>=0; --i)
                if (rows.get(i).st == row_s::s_deleted)
                    rows.remove_slow(i);
        cleanup_requred = false;
    }
    tableview_t() {}
    tableview_t(const tableview_t&) UNUSED;
    tableview_t(tableview_t &&) UNUSED;
    void operator=(const tableview_t& other)
    {
        rows = other.rows;
        newidpool = other.newidpool;
        cleanup_requred = other.cleanup_requred;
        cleanup();
    }
    void operator=(tableview_t&& other)
    {
        rows = std::move(other.rows);
        SWAP(newidpool, other.newidpool);
        cleanup_requred = other.cleanup_requred;
        cleanup();
    }

    /*
    row_s del(int id) // returns deleted row
    {
        cleanup();
        int cnt = rows.size();
        for (int i = 0; i < cnt; ++i)
        {
            const row_s &r = rows.get(i);
            if (r.id == id)
                return rows.get_remove_slow(i);
        }
        return row_s();
    }
    */

    template<bool skip_deleted, typename F> row_s *find(F f)
    {
        cleanup();
        for (row_s &r : rows)
            if ( ( !skip_deleted || r.st != row_s::s_delete ) && r.st != row_s::s_deleted )
                if (f(r.other)) return &r;
        return nullptr;
    }
    template<bool skip_deleted, typename F> const row_s *find(F f) const
    {
        ASSERT(!cleanup_requred);
        for (const row_s &r : rows)
            if ((!skip_deleted || r.st != row_s::s_delete) && r.st != row_s::s_deleted)
                if (f(r.other)) return &r;
        return nullptr;
    }

    template<bool skip_deleted> row_s *find( int id )
    {
        cleanup();
        for (row_s &r : rows)
            if ( ( !skip_deleted || r.st != row_s::s_delete ) && r.st != row_s::s_deleted )
                if (r.id == id) return &r;
        return nullptr;
    }
    row_s &getcreate(int id);
    bool reader(int row, ts::SQLITE_DATAGETTER);

    void clear() { newidpool = -1; rows.clear(); new2ins.clear(); }
    bool prepare( ts::sqlitedb_c *db );
    void read( ts::sqlitedb_c *db );
    void read( ts::sqlitedb_c *db, const ts::asptr &where_items );
    bool flush( ts::sqlitedb_c *db, bool all, bool notify_saved = true );

    template<typename TABTYPE> class iterator
    {
        friend struct tableview_t;
        TABTYPE *tab;
        ts::aint index;
        iterator(TABTYPE *_tab, bool end):tab(_tab), index(end ? _tab->rows.size(): 0) { if (!end) skipdel(); }
        void skipdel()
        {
            ts::aint cnt = tab->rows.size();
            while( index < cnt && !tab->rows.get(index).present() )
                ++index;
        }
        bool is_end() const { return index >= tab->rows.size(); }
        row_s *get() { return is_end() ? nullptr : (row_s *)&tab->rows.get(index); }
    public:
        int id() const { return is_end() ? 0 : tab->rows.get(index).id; }
        operator typename row_s*() { return get(); }
        typename row_s *operator->() { return get(); }
        void operator++() { ++index; skipdel(); }
        bool operator!=(const iterator &it) const { return index != it.index || tab != it.tab; }
    };
    iterator<const tableview_t<T,tabi> > begin() const { return iterator<const tableview_t<T,tabi> >(this, false); }
    iterator<const tableview_t<T,tabi> > end() const { return iterator<const tableview_t<T,tabi> >(this, true); }
};

#define TAB(tab) typedef tableview_t<tab##_s, pt_##tab> tableview_##tab##_s;
PROFILE_TABLES
#undef TAB

#define TAB(tab) INLINE tableview_##tab##_s::row_s & row_by_type( tab##_s * t ) { return *(tableview_##tab##_s::row_s *)(((ts::uint8 *)t) - offsetof( tableview_##tab##_s::row_s, other )); }
PROFILE_TABLES
#undef TAB

enum hitsory_item_field_e
{
    HITM_MT = SETBIT(0),
    HITM_TIME = SETBIT(1),
    HITM_MESSAGE = SETBIT(2),
    HITM_SENDER = SETBIT(3),
};

enum enter_key_options_s
{
    EKO_ENTER_SEND,
    EKO_ENTER_NEW_LINE,
    EKO_ENTER_NEW_LINE_DOUBLE_ENTER,
};

enum camview_snap_e
{
    CVS_LEFT_TOP,
    CVS_LEFT_BOTTOM,
    CVS_TOP_LEFT,
    CVS_TOP_RITE,
    CVS_RITE_TOP,
    CVS_RITE_BOTTOM,
    CVS_BOTTOM_LEFT,
    CVS_BOTTOM_RITE,
};

struct dialog_protosetup_params_s;
class pb_job_c;

enum profile_load_result_e
{
    PLR_OK,
    PLR_CORRUPT_OR_ENCRYPTED,
    PLR_UPGRADE_FAILED,
    PLR_CONNECT_FAILED,
    PLR_BUSY,
};

class profile_c : public config_base_c
{
    #define TAB(tab) tableview_##tab##_s table_##tab;
    PROFILE_TABLES
    #undef TAB

    GM_RECEIVER( profile_c, ISOGM_PROFILE_TABLE_SAVED );
    GM_RECEIVER( profile_c, ISOGM_MESSAGE );
    GM_RECEIVER( profile_c, ISOGM_CHANGED_SETTINGS );

    ts::array_safe_t< active_protocol_c, 0 > protocols;
    ts::tbuf_t< contact_key_s > dirtycontacts;
    ts::tbuf0_t<ts::uint16> protogroupsortdata;

    /*virtual*/ void onclose() override;
    /*virtual*/ bool save() override;

    bool present_active_protocol(int id) const
    {
        for(const active_protocol_c *ap : protocols)
            if (ap && ap->getid() == id)
                return true;
        return false;
    }

    static const ts::flags32_s::BITS F_OPTIONS_VALID = SETBIT(0);
    static const ts::flags32_s::BITS F_LOADING = SETBIT(1);
    static const ts::flags32_s::BITS F_CLOSING = SETBIT(2);
    static const ts::flags32_s::BITS F_ENCRYPTED = SETBIT(3);
    static const ts::flags32_s::BITS F_ENCRYPT_PROCESS = SETBIT(4);
    static const ts::flags32_s::BITS F_LOADED_TABLES = SETBIT(5);

    ts::flags32_s profile_flags;
    ts::flags32_s current_options;

	UINT32PAR(msgopts, 0)
	UINT32PAR(msgopts_edited, 0)

    void create_aps();

    ts::uint8 keyhash[CC_HASH_SIZE]; // 256 bit hash

    uint64 uuid = 0; // zero - freezed; cant be used
    uint num_locked_uids = 0;

    void load_protosort();
    void save_protosort();


public:

    profile_c() {}
    ~profile_c();

    uint64 getuid( uint cnt = 1 );

    ts::sqlitedb_c *begin_encrypt();
    void encrypt_done(const ts::uint8 *newpasshash);
    void encrypt_failed();

    void encrypt( const ts::uint8 *passwdhash /* 256 bit (32 bytes) hash */, float delay_before_start = 0.5f );
    bool is_encrypted() const { return profile_flags.is(F_ENCRYPTED); }
    ts::str_c get_keyhash_str() const { ts::str_c h; if (is_encrypted()) h.append_as_hex(keyhash, CC_HASH_SIZE); return h; }

    static void mb_error_load_profile(const ts::wsptr & prfn, profile_load_result_e plr, bool modal = false);

    bool addeditnethandler(dialog_protosetup_params_s &params);


    bool delete_conference( int id );

    /// find conference by pubid
    conference_s * find_conference( const ts::asptr &pubid, ts::uint16 apid )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );
        
        if ( pubid.l == 0 )
            return nullptr;

        get_table_conference().cleanup();

        for ( auto &c : get_table_conference() )
            if (c.other.proto_key.protoid == apid && c.other.pubid == pubid )
                return &c.other;
        return nullptr;
    }

    /// find conference by protocol key
    conference_s * find_conference( const contact_key_s &ck )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );
        ASSERT( ck.is_conference() && ck.temp_type == TCT_NONE );

        get_table_conference().cleanup();

        for ( auto &c : get_table_conference() )
            if ( c.other.proto_key == ck )
                return &c.other;
        return nullptr;
    }

    conference_s * find_conference_by_id( int id )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );

        get_table_conference().cleanup();

        for ( auto &c : get_table_conference() )
            if ( c.other.id == id )
                return &c.other;
        return nullptr;
    }

    conference_s * add_conference( const ts::str_c &pubid, const contact_key_s &protocol_key );

    conference_member_s * find_conference_member( const contact_key_s &member_protokey )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );
        ASSERT( member_protokey.gid == 0 && member_protokey.temp_type == TCT_NONE ); // abstract conference member has no conference id (gid)

        get_table_conference_member().cleanup();

        for ( auto &cm : get_table_conference_member() )
            if ( cm.other.proto_key == member_protokey )
                return &cm.other;

        return nullptr;
    }

    conference_member_s * find_conference_member( const ts::asptr &pubid, ts::uint16 apid )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );

        get_table_conference_member().cleanup();

        for ( auto &cm : get_table_conference_member() )
            if (cm.other.proto_key.protoid == apid && cm.other.pubid == pubid )
                return &cm.other;

        return nullptr;
    }

    conference_member_s * find_conference_member_by_id( int id )
    {
        ASSERT( profile_flags.is( F_LOADED_TABLES ) );

        get_table_conference_member().cleanup();

        for ( auto &cm : get_table_conference_member() )
            if ( cm.other.id == id )
                return &cm.other;

        return nullptr;
    }

    bool delete_conference_member( int id );
    conference_member_s * add_conference_member( const ts::str_c &pubid, const contact_key_s &proto_key );
    void make_contact_unknown_member( contact_c * c );
    void make_contact_known( contact_c * c, const contact_key_s& proto_key );
    bool is_conference_member( const contact_key_s &ck );

    void flush_contacts();
    bool flush_tables();

    void check_aps();
    void shutdown_aps();
    template<typename APR> void iterate_aps( APR apr ) const
    {
        for( const active_protocol_c *ap : protocols )
            if (ap && !ap->is_dip()) apr(*ap);
    }
    template<typename APR> void iterate_aps(APR apr)
    {
        for (active_protocol_c *ap : protocols)
            if (ap && !ap->is_dip()) apr(*ap);
    }
    template<typename APR> const active_protocol_c * find_ap(APR apr) const
    {
        for (const active_protocol_c *ap : protocols)
            if (ap && !ap->is_dip())
                if (apr(*ap))
                    return ap;
        return nullptr;
    }
    template<typename APR> active_protocol_c * find_ap(APR apr)
    {
        for (active_protocol_c *ap : protocols)
            if (ap && !ap->is_dip())
                if (apr(*ap))
                    return ap;
        return nullptr;
    }
    active_protocol_c *ap(int id) { for( active_protocol_c *ap : protocols ) if (ap && ap->getid() == id && !ap->is_dip()) return ap; return nullptr; }
    bool is_any_active_ap(int required_features = -1) const
    {
        for (const active_protocol_c *ap : protocols)
            if (ap && !ap->is_dip() && ap->get_current_state() == CR_OK && (required_features == -1 || (ap->get_features() & required_features) == required_features))
                return true;
        return false;
    }

    profile_load_result_e xload( const ts::wstr_c& pfn, const ts::uint8 *k );

    ts::blob_c get_avatar( const contact_key_s&k ) const;
    void set_avatar( const contact_key_s&k, const ts::blob_c &avadata, int tag );

    void dirtycontact( const contact_key_s&k )
    {
        ASSERT( k.temp_type == TCT_NONE && !k.is_self );
        dirtycontacts.set(k); changed();
    }
    void killcontact( const contact_key_s&k );
    void purifycontact( const contact_key_s&k );
    bool isfreecontact( const contact_key_s&k ) const;

    static ts::wstr_c& path_by_name( ts::wstr_c &profname );

    void record_history( const contact_key_s&historian, const post_s &history_item );
    int  calc_history( const contact_key_s&historian, bool ignore_invites = true );
    int  calc_history( const contact_key_s&historian, const contact_key_s&sender );
    int  calc_history_before( const contact_key_s&historian, time_t time );
    int  calc_history_after( const contact_key_s&historian, time_t time, bool only_messages );
    int  calc_history_between( const contact_key_s&historian, time_t time1, time_t time2 );
    
    void unload_history( const contact_key_s&historian );
    void kill_history_item( uint64 utag );
    void kill_history(const contact_key_s&historian);

    void load_history( const contact_key_s&historian, allocpost *cb, void *prm ); // just callback, no table

    void kill_message( uint64 msgutag );

    void load_history( const contact_key_s&historian, time_t time, ts::aint nload, ts::tmp_tbuf_t<int>& loaded_ids );
    void load_history( const contact_key_s&historian ); // load all history items to internal table
    void merge_history( const contact_key_s&base_historian, const contact_key_s&from_historian );
    void detach_history( const contact_key_s&prev_historian, const contact_key_s&new_historian, const contact_key_s&sender );
    void change_history_item(const contact_key_s&historian, const post_s &post, ts::uint32 change_what);
    bool change_history_item(uint64 utag, contact_key_s & historian); // find item by tag and change type form MTA_UNDELIVERED_MESSAGE to MTA_MESSAGE, then return historian (if loaded)
    void change_history_items( const contact_key_s &historian, const contact_key_s &old_sender, const contact_key_s &new_sender );
    void flush_history_now();
    void load_undelivered();
    contact_root_c *find_corresponding_historian(const contact_key_s &subcontact, ts::array_wrapper_c<contact_root_c * const> possible_historians);

    ts::bitmap_c load_avatar( const contact_key_s& ck );

    uint64 uniq_history_item_tag();

    ts::flags32_s get_options()
    {
        if (!profile_flags.is(F_OPTIONS_VALID))
        {
            ts::flags32_s::BITS opts = msgopts();
            ts::flags32_s::BITS optse = msgopts_edited();
            current_options.setup( (opts & optse) | ( ~optse & DEFAULT_MSG_OPTIONS ) );
            profile_flags.set(F_OPTIONS_VALID);
        }
        return current_options;
    }
    bool set_options( ts::flags32_s::BITS mo, ts::flags32_s::BITS mask )
    {
        ts::flags32_s::BITS cur = get_options().__bits;
        ts::flags32_s::BITS curmod = cur & mask;
        ts::flags32_s::BITS newbits = mo & mask;
        ts::flags32_s::BITS edited = msgopts_edited();
        edited |= (curmod ^ newbits);
        msgopts_edited( edited );
        cur = (cur & ~mask) | newbits;
        current_options.setup(cur);
        return msgopts( cur );
    }

    int min_history_load(bool for_button) { return get_options().is(MSGOP_LOAD_WHOLE_HISTORY) ? -1 : ( for_button ? add_history() : min_history() ); }

    ts::wstr_c get_disabled_dicts();

    ts::aint protogroupsort( const ts::uint16 * set_of_prots, ts::aint cnt );
    bool protogroupsort_up( const ts::uint16 * set_of_prots, ts::aint cnt, bool test );
    bool protogroupsort_dn( const ts::uint16 * set_of_prots, ts::aint cnt, bool test );

    TEXTAPAR( username, "IsotoxinUser" )
    TEXTAPAR( userstatus, "" )
    INTPAR( min_history, 10 )
    INTPAR( add_history, 100 )
    INTPAR( ctl_to_send, EKO_ENTER_NEW_LINE )
    INTPAR( inactive_time, 5 )
    INTPAR( manual_cos, COS_ONLINE )
    INTPAR( dmn_duration, 5 )
    
    TEXTAPAR( date_msg_template, "d MMMM" )
    TEXTAPAR( date_sep_template, "dddd d MMMM yyyy" )
    TEXTWPAR( download_folder, "%CONFIG%\\download" )
    TEXTWPAR( download_folder_images, "%CONFIG%\\images\\%CONTACTID%" )
    TEXTWPAR( last_filedir, "" )

    TEXTWPAR( emoticons_pack, "" )
    TEXTWPAR( emoticons_set, "" )

    TEXTWPAR(auto_confirm_masks, "*.png; *.jpg; *.gif; *.avi; *.mp4; *.mkv");
    TEXTWPAR(manual_confirm_masks, "*.exe; *.com; *.bat; *.cmd; *.vbs");
    INTPAR(fileconfirm, 0);

    INTPAR( misc_flags, 0 );

    INTPAR(useproxyfor, 0xffff);

    INTPAR( camview_size, 25 );
    INTPAR( camview_snap, CVS_RITE_BOTTOM );
    INTPAR( camview_pos, 0 );

    TEXTAPAR( tags, "" );
    TEXTWPAR( bitagnames, "" );
    INTPAR(bitags, (1<<BIT_ALL) );

    FLOATPAR(fontscale_conv_text, 1.0f);
    FLOATPAR(fontscale_msg_edit, 1.0f);

    TEXTWPAR(disabled_spellchk, "?");

    TEXTAPAR(unique_profile_tag, "")

#define TAB(tab) tableview_##tab##_s &get_table_##tab() { return table_##tab; }; const tableview_##tab##_s &get_table_##tab() const { return table_##tab; };
    PROFILE_TABLES
#undef TAB

    INTPAR(video_enc_quality, 0);
    INTPAR(video_bitrate, 0);
    TEXTAPAR(video_codec, "");

    INTPAR( backup_period, 3600 ); // seconds
    INTPAR( backup_keeptime, 30 ); // days

    INTPAR(max_thumb_height, 80); // hidden

    TEXTAPAR( protosort, "" );

    UNIQUE_PTR( ts::global_atom_s ) mutex;


#ifdef _DEBUG
    void test();
#endif // _DEBUG
};

#undef TEXTPAR

//-V:prf():807
extern ts::static_setup<profile_c> prf;


