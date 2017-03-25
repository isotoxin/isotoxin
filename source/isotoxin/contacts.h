#pragma once

enum message_type_app_e : unsigned
{
    MTA_MESSAGE = MT_MESSAGE,
    MTA_SYSTEM_MESSAGE = MT_SYSTEM_MESSAGE,
    MTA_FRIEND_REQUEST = MT_FRIEND_REQUEST,
    MTA_INCOMING_CALL = MT_INCOMING_CALL,
    MTA_CALL_STOP = MT_CALL_STOP,
    MTA_CALL_ACCEPTED__ = MT_CALL_ACCEPTED,
    MTA_FOLDER_SHARE_ANNONUCE__ = MT_FOLDER_SHARE_ANNOUNCE,

    // not saved to db
    MTA_HISTORY_LOAD_BUTTON,
    MTA_DATE_SEPARATOR,
    MTA_TYPING,
    MTA_SUPERMESSAGE,

    message_type_app_values = 99,
    // saved to db

    // hard order!!!!
    MTA_INCOMING_CALL_CANCELED,
    MTA_ACCEPT_OK,   // to acceptor
    MTA_ACCEPTED,    // to requester
    MTA_OLD_REQUEST, // 103
    MTA_INCOMING_CALL_REJECTED,
    MTA_CALL_ACCEPTED, // incoming or outgoing call accepted - same effect
    MTA_HANGUP,
    MTA_UNDELIVERED_MESSAGE,
    MTA_RECV_FILE,
    MTA_SEND_FILE,
    MTA_HIGHLIGHT_MESSAGE,
};

static_assert( MTA_FRIEND_REQUEST == 2, "!" );
static_assert(MTA_INCOMING_CALL_CANCELED == 100, "!");
static_assert( MTA_OLD_REQUEST == 103, "!" );
static_assert( MTA_INCOMING_CALL_REJECTED == 104, "!" );

INLINE bool is_special_mt(message_type_app_e mt)
{
    if (mt == MTA_UNDELIVERED_MESSAGE) return false;
    return mt > message_type_app_values;
}

// also fix gui_filterbar_c::full_search_s::iterate
#define CASE_MESSAGE case MTA_MESSAGE: \
                     case MTA_UNDELIVERED_MESSAGE: \
                     case MTA_HIGHLIGHT_MESSAGE: \
                     case MTA_SYSTEM_MESSAGE

INLINE bool is_message_mt( message_type_app_e mt )
{
    switch(mt)
    {
    CASE_MESSAGE:
        return true;
    }
    return false;
}

enum buildin_tags_e
{
    BIT_ALL,
    BIT_ONLINE,
    BIT_UNTAGGED,

    BIT_count
};

enum temp_contact_type_e
{
    TCT_NONE,
    TCT_CONFERENCE,
    TCT_UNKNOWN_MEMBER,
    TCT_CONFERENCE_MEMBER, // special
};

struct contact_key_s
{
    DUMMY(contact_key_s);

    unsigned contactid : 24;        // contact id (0 - conference itself)
    unsigned temp_type : 2;
    unsigned is_self : 1;
    unsigned reserved : 5;          // unused
    unsigned gid : 16;              // conference id
    unsigned protoid : 16;          // 0 - metacontact. all visible contacts are metacontacts with history and conversation / >0 - protocol contact

    explicit contact_key_s( const ts::asptr&s )
    {
        ts::ref_cast<uint64>(*this) = ts::pstr_c(s).as_num<uint64>();
    }
    explicit contact_key_s( const ts::str_c&s )
    {
        ts::ref_cast<uint64>( *this ) = s.as_num<uint64>();
    }
    contact_key_s( const contact_key_s&ck ) { ts::ref_cast<uint64>( *this ) = ts::ref_cast<uint64>( ck ); }
    explicit contact_key_s( int cid_ ) :contactid( cid_ ), is_self( 0 ), temp_type( TCT_NONE ), reserved( 0 ), gid( 0 ), protoid( 0 ) {} // meta
    explicit contact_key_s( uint64 avk ):contactid(0x00ffffff & avk), is_self( 0 ), temp_type( TCT_NONE ), reserved(0) // this constructor is compatible with meta (previous)
    {
        if ( 0 == (0xE000000000000000ull & avk) )
        {
            // avk is cid
            // simple meta
            gid = 0;
            protoid = 0;
        } else
        {
            gid = 0xffff & ( avk >> 40 );
            protoid = 0xffff & ( avk >> 24 );
            if (0 != (0x2000000000000000ull & avk))
                temp_type = TCT_CONFERENCE_MEMBER;
        }
    }

    static int cid_as_int(contact_id_s cid)
    {
        ASSERT( cid.is_contact() || cid.is_empty() );
        return cid.id;
    }

    contact_key_s(contact_id_s gidcid, int ap_ ) :contactid( 0 ), is_self( 0 ), temp_type( TCT_NONE ), reserved( 0 ), gid( 0 ), protoid( static_cast<ts::uint16>(ap_) )
    {
        ASSERT(ap_ >= 0 && ap_ < 65536);

        if (gidcid.is_contact())
        {
            contactid = cid_as_int(gidcid);
            if (gidcid.is_self() && ap_ != 0)
                is_self = 1;

        } else if (gidcid.is_conference())
            contactid = 0, gid = static_cast<ts::uint16>( gidcid.id );
    }
    contact_key_s( contact_id_s gid_, contact_id_s cid_, int ap_ ) :contactid(cid_as_int(cid_)), is_self(0), temp_type( TCT_NONE ), reserved(0),
        gid( static_cast<ts::uint16>( gid_.id ) ),
        protoid(static_cast<ts::uint16>(ap_) )
    {
        ASSERT(ap_ >= 0 && ap_ < 65536);
        ASSERT(gid_.is_empty() || (gid_.id < 65536 && gid_.is_conference()));
    }

    explicit contact_key_s( bool self = false )
    {
        ts::ref_cast<uint64>( *this ) = 0;
        if ( self )
            is_self = 1;
    }
    contact_key_s( temp_contact_type_e tt, int cid_, int ap_ ) :contactid( (unsigned)cid_ ), is_self( 0 ), temp_type( tt ), reserved( 0 ), gid( 0 ), protoid( ( ts::uint16 )ap_ ) {}

    contact_id_s gidcid() const;

    bool is_meta() const {return (protoid == 0 && contactid > 0) && temp_type == TCT_NONE;}
    bool is_conference() const {return (contactid == 0 && protoid > 0 && gid > 0) || temp_type == TCT_CONFERENCE;}
    bool is_conference_member() const { return (contactid > 0 && protoid > 0 && gid > 0) || temp_type == TCT_UNKNOWN_MEMBER; } // two types of conference members: 1 - member of particular conference, 2 - unknown member of a conference
    bool is_empty() const {return ts::ref_cast<uint64>(*this) == 0; }

    operator uint64() const { return ts::ref_cast<uint64>( *this ); }

    static contact_key_s buildfromdbvalue( int64 v, bool historian )
    {
        if ( v == 0 )
            return contact_key_s( true );

        if ( v < 0 )
            return contact_key_s( TCT_UNKNOWN_MEMBER, (-v) & 0xffffff, ((-v) >> 32) & 0xffff );

        if ( historian )
        {
            int high_part = v >> 32;
            if ( high_part > 0 )
                return contact_key_s( TCT_CONFERENCE, high_part, v & 0xffff );
        }

        return contact_key_s(contact_id_s(contact_id_s::CONTACT, v & 0xffffff), v >> 32 );
    }
    int64 dbvalue() const
    {
        if ( TCT_CONFERENCE == temp_type )
            return (static_cast<int64>( contactid ) << 32) | protoid; // historian // see CONFDBVAL

        if ( TCT_UNKNOWN_MEMBER == temp_type )
            return -( ( static_cast<int64>( protoid ) << 32 ) | contactid ); // see MEMBDBVAL

        return (static_cast<int64>(protoid) << 32) | contactid; // always positive
    }

    bool operator<(const contact_key_s&oc) const { return ts::ref_cast<uint64>(*this) < ts::ref_cast<uint64>(oc); }
    bool operator==(const contact_key_s&oc) const { return ts::ref_cast<uint64>(*this) == ts::ref_cast<uint64>(oc); }
    bool operator!=(const contact_key_s&oc) const { return ts::ref_cast<uint64>(*this) != ts::ref_cast<uint64>(oc); }
    int operator()(const contact_key_s&oc) const 
    { 
        return (int)ts::isign( ts::ref_cast<int64>(oc) - ts::ref_cast<int64>(*this) );
    }

    ts::str_c as_str() const
    {
        ts::str_c s;
        s.set_as_num<uint64>( ts::ref_cast<uint64>(*this) );
        return s;
    }

    contact_root_c *find_root_contact() const;
    contact_c *find_sub_contact() const;
    contact_c *find_conference_member( contact_root_c *confa ) const;
};

DECLARE_MOVABLE(contact_key_s, true);

uint64 operator |( const contact_key_s&root, const contact_key_s &sub );

TS_STATIC_CHECK( sizeof( contact_key_s ) == sizeof(uint64), "keysize!" );

template<typename STRTYPE> INLINE ts::streamstr<STRTYPE> & operator<<(ts::streamstr<STRTYPE> &dl, const contact_key_s &k)
{
    dl.begin();
    dl.raw_append("CK=[cid:");
    dl << k.contactid;
    dl.raw_append(",pid:");
    dl << k.protoid;
    dl.raw_append("]");
    return dl;
}

INLINE unsigned calc_hash(const contact_key_s &ck)
{
    return ts::calc_hash( ((uint64)ck) & 0xffffffff ) ^ ts::hash_func(ts::calc_hash( ((uint64)ck ) >> 32 ));
}

ts::asptr  calc_message_skin( message_type_app_e mt, const contact_key_s &sender );

// initialization: [POST_INIT]
struct post_s
{
    DUMMY(post_s);
    post_s() {}

#ifdef _WIN32
#if defined _USE_32BIT_TIME_T
#error "time_t must be 64 bit"
#endif
#endif // _WIN32

    TS_STATIC_CHECK( sizeof(time_t) == 8, "time_t must be 64 bit" );

    static const int type_size_bits = 8;
    static const int options_size_bits = 8;

    uint64 utag;
    time_t recv_time = 0;
    time_t cr_time = 0;
    ts::shared_ptr< ts::refstring_t<char> > message_utf8;
    contact_key_s sender;
    contact_key_s receiver;
    unsigned type : type_size_bits;
    unsigned options : options_size_bits;

    void set_message_text( const ts::asptr&m );
    const time_t &get_crtime() const { return cr_time != 0 ? cr_time : recv_time; }
    message_type_app_e mt() const {return (message_type_app_e)type;}

    void set_type_and_options( int64 dbval ) // db value
    {
        type = dbval & ( SETBIT( type_size_bits ) - 1 );
        options = ( dbval >> 16 ) & ( SETBIT( options_size_bits ) - 1 );
    }
};

DECLARE_MOVABLE(post_s, true)

struct avatar_s : public ts::bitmap_c
{
    int tag = 0;
    bool alpha_pixels = false;
    avatar_s() {}
    avatar_s( const void *body, ts::aint size, int tag ) { load(body, size, tag); }
    void load( const void *body, ts::aint size, int tag );
};

template<> struct gmsg<ISOGM_UPDATE_CONTACT> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_UPDATE_CONTACT) {}
    contact_id_s key;
    ts::uint16 apid;
    int mask;
    int caps = 0;
    ts::str_c pubid;
    ts::str_c name;
    ts::str_c statusmsg;
    ts::str_c details;
    int avatar_tag = 0;
    contact_state_e state = CS_INVITE_SEND;
    contact_online_state_e ostate = COS_ONLINE;
    contact_gender_e gender = CSEX_UNKNOWN;
    int grants = -1;

    ts::tbuf0_t<contact_id_s> members;
    ts::blob_c pdata;
};

template<> struct gmsg<ISOGM_PEER_STREAM_OPTIONS> : public gmsgbase
{
    gmsg( uint64 avkey, int so, const ts::ivec2 &sz) :gmsgbase(ISOGM_PEER_STREAM_OPTIONS), avkey( avkey ), so(so), videosize(sz) {}
    uint64 avkey;
    ts::ivec2 videosize;
    int so;
};

template<> struct gmsg<ISOGM_GRABDESKTOPEVENT> : public gmsgbase
{
    uint64 avk;
    ts::irect r;
    int monitor = -1;
    bool av_call = false;
    gmsg( const ts::irect &r, uint64 avk, int monitor, bool av_call ) :gmsgbase( ISOGM_GRABDESKTOPEVENT ), r( r ), avk( avk ), monitor(monitor), av_call( av_call ) {}
    gmsg() :gmsgbase( ISOGM_GRABDESKTOPEVENT ), r( 0 ) {}
};


enum keep_contact_history_e // hard order
{
    KCH_DEFAULT,
    KCH_ALWAYS_KEEP,
    KCH_NEVER_KEEP,
};

enum incoming_message_beh_e
{
    IMB_DEFAULT,
    IMB_DESKTOP_NOTIFICATION = 1,
    IMB_INTRUSIVE_BEHAVIOUR = 2,
    IMB_ALL = 3,
    IMB_SUPPRESS = 4,

    IMB_DONT_NOTIFY = IMB_ALL | IMB_SUPPRESS,
    IMB_SUPPRESS_INTRUSIVE_BEHAVIOUR = IMB_INTRUSIVE_BEHAVIOUR | IMB_SUPPRESS,
};

enum auto_accept_audio_call_e
{
    AAAC_NOT,
    AAAC_ACCEPT_MUTE_MIC,
    AAAC_ACCEPT,
};

enum msg_handler_e
{
    MH_NOT,
    MH_AS_PARAM,
    MH_VIA_FILE,
};

enum reselect_options_e
{
    RSEL_SCROLL_END = 1,
    RSEL_INSERT_NEW = 2,
    RSEL_CHECK_CURRENT = 4,
};

//-V:options():807

#define FPROTOCALL( fname, ... ) do { if (active_protocol_c *ap = prf().ap(getkey().protoid)) { ap->fname(getkey().gidcid(), ## __VA_ARGS__); } } while(false)

class gui_contact_item_c;
struct contacts_s;
class contact_root_c;
class contact_c : public ts::shared_object
{
    friend class contact_root_c;
    friend class contacts_c;

    DUMMY(contact_c);
    contact_key_s key;
    ts::shared_ptr<contact_root_c> metacontact; // valid for non-meta contacts
    ts::str_c pubid;
    ts::str_c name;
    ts::str_c customname;
    ts::str_c statusmsg;
    ts::str_c details;
    UNIQUE_PTR( avatar_s ) avatar;
    ts::blob_c protodata;

    void setmeta(contact_root_c *metac);

public:

    union
    {
        uint64 sombits;
        struct
        {
            // saved
            unsigned is_default : 1;
            unsigned is_ava_default : 1;
            unsigned is_unknown : 1;
            unsigned is_allow_invite : 1; // only valid for unknown contacts
            unsigned is_system_user : 1;
            unsigned __unused_05 : 1;
            unsigned __unused_06 : 1;
            unsigned __unused_07 : 1;
            unsigned __unused_08 : 1;
            unsigned __unused_09 : 1;
            unsigned __unused_10 : 1;
            unsigned __unused_11 : 1;
            unsigned __unused_12 : 1;
            unsigned __unused_13 : 1;
            unsigned __unused_14 : 1;
            unsigned __unused_15 : 1;

            // not saved
            unsigned state : contact_state_bits;
            unsigned ostate : contact_online_state_bits;
            unsigned gender : contact_gender_bits;

            unsigned flag_dip : 1;
            unsigned flag_allowacceptdata : 1;
            unsigned flag_just_rejected : 1;
            unsigned flag_just_accepted : 1;
            unsigned flag_is_ringtone : 1;
            unsigned flag_is_calltone : 1;
            unsigned flag_is_av : 1;
            unsigned flag_is_videocall : 1;
            unsigned flag_history_need_load : 1;
            unsigned flag_full_search_result : 1;
            unsigned flag_support_folder_share : 1;
            unsigned flag_folder_share_mode : 1;
            unsigned flag_last_activity : 1;
            unsigned flag_caps_received : 1;
        };
    };

    int operator()(const contact_c&oc) const { return key(oc.key); }
    int operator()(const contact_key_s&k) const { return key(k); }

    const contact_key_s& getkey() const {return key;}
    uint64 avkey( const contact_key_s &rootkey ) const
    {
        ASSERT( !is_rootcontact() );
        return rootkey | getkey();
    }

    explicit contact_c( const contact_key_s &key );
    contact_c();
    virtual ~contact_c();

    virtual void setup(const contacts_s * c, time_t nowtime);
    virtual bool save(contacts_s * c) const;

    void prepare4die(contact_root_c *owner);

    void reselect(int options = RSEL_SCROLL_END);

    void redraw();
    void redraw(float delay);

    bool is_meta() const { return key.is_meta() && getmeta() == nullptr; }; // meta, but not conference
    bool is_rootcontact() const { return !is_unknown && (is_meta() || getkey().is_conference() || (getkey().protoid == 0 && getkey().is_self)); } // root contact - in contact list

    bool is_rejected() const { return CS_REJECTED == get_state() || flag_just_rejected; };

    bool is_unknown_conference_member() const
    {
        return CS_UNKNOWN == get_state() && getmeta() == nullptr;
    }
    void make_unknown_conference_member( int id );

    //const Options &get_options() const {return opts;}
    //Options &options() {return opts;}

    contact_root_c *getmeta() {return metacontact;}
    const contact_root_c *getmeta() const {return metacontact;}

    void detach();

    const contact_root_c *get_historian() const;
    contact_root_c *get_historian();

    void set_pubid( const ts::str_c &pubid_ ) { pubid = pubid_; }
    void set_name( const ts::str_c &name_ ) { name = name_; }
    void set_customname( const ts::str_c &name_ ) { customname = name_; }
    void set_statusmsg( const ts::str_c &statusmsg_ ) { statusmsg = statusmsg_; }
    void set_details( const ts::str_c &details_ ) { details = details_; }
    void set_state( contact_state_e st ) { state = st; is_unknown = (st == CS_UNKNOWN) ? 1 : 0; }
    void set_ostate( contact_online_state_e ost ) { ostate = ost; }
    void set_gender( contact_gender_e g ) { gender = g; }
    
    const ts::str_c &get_details() const { return details; }

    const ts::str_c &get_pubid() const {return pubid;};
    virtual const ts::str_c get_pubid_desc() const { return get_pubid(); }
    ts::str_c get_description() const
    {
        ts::str_c t = get_name();
        if (t.is_empty())
            t.set(get_pubid_desc());
        else
            text_adapt_user_input(t);
        return t;
    }
    ts::str_c get_name(bool metacompile = true) const;
    const ts::str_c &get_customname() const { return customname; }
    ts::str_c get_statusmsg(bool metacompile = true) const;
    contact_state_e get_state() const {return (contact_state_e)state;}
    contact_online_state_e get_ostate() const {return (contact_online_state_e)ostate;}
    contact_gender_e get_gender() const {return (contact_gender_e)gender;}

    const ts::blob_c &get_protodata() const { return protodata; }
    bool set_protodata(const ts::blob_c &d);

    bool authorized() const { return get_state() == CS_OFFLINE || get_state() == CS_ONLINE; }

    void accept_call( auto_accept_audio_call_e aaac, bool video ); // sub

    bool b_accept(RID, GUIPARAM);
    bool b_reject(RID, GUIPARAM);
    bool b_resend(RID, GUIPARAM);
    bool b_kill(RID, GUIPARAM);
    bool b_accept_call_with_video(RID, GUIPARAM);
    bool b_accept_call(RID, GUIPARAM);
    bool b_reject_call(RID, GUIPARAM); // sub
    bool b_hangup(RID, GUIPARAM); // root and sub
    bool b_call(RID, GUIPARAM); // sub
    bool b_cancel_call(RID, GUIPARAM); // sub

    bool b_receive_file(RID, GUIPARAM);
    bool b_receive_file_as(RID, GUIPARAM);
    bool b_refuse_file(RID, GUIPARAM);
    
    void rebuild_system_user_avatar( active_protocol_c *ap );
    int avatar_tag() const {return avatar ? avatar->tag : 0; }
    void set_avatar( const void *body, ts::aint size, int tag )
    {
        if (size == 0)
        {
            avatar.reset();
            return;
        }

        if (avatar)
            avatar->load( body, size, tag );
        else
            avatar.reset( TSNEW(avatar_s, body, size, tag) );
    }
    virtual const avatar_s *get_avatar() const;

};

TS_STATIC_CHECK( sizeof(contact_c) == offsetof(contact_c, sombits) + 8, "somwrong" );

typedef ts::array_shared_t<contact_c, 8> contacts_array_t;

struct conference_s;

enum calltone_e
{
    CALLTONE_HANGUP,
    CALLTONE_CALL_ACCEPTED,
    CALLTONE_VOICE_CALL,
    CALLTONE_VIDEO_CALL,
};

enum why_this_subget_e
{
    WTS_ONLY_ONE,
    WTS_MARKED_DEFAULT,
    WTS_BY_LAST_ACTIVITY,
};

class contact_root_c : public contact_c // metas and conferences
{
    typedef contact_c super;

    DUMMY(contact_root_c);
    ts::array_shared_t<contact_c, 0> subcontacts; // valid for meta contact

    ts::pointers_t<post_s, 128> history;
    ts::struct_buf_t< post_s, 128 > posts;
    ts::str_c comment;
    ts::str_c greeting; // utf8 greeting; send as message on contact online
    ts::wstr_c mhc;
    ts::astrings_c tags;
    ts::buf0_c tags_bits; // rebuilt
    //ts::wstr_c sfp; FOLDER_SHARE_PATH

    time_t readtime = 0; // all messages after readtime considered unread
    keep_contact_history_e keeph = KCH_DEFAULT;
    auto_accept_audio_call_e aaac = AAAC_NOT;
    msg_handler_e mht = MH_NOT;
    int imnb = 0; // incoming message notification behavior
    ts::Time last_history_touch = ts::Time::past();

    int confa_grants = -1;

public:
    ts::safe_ptr<gui_contact_item_c> gui_item;

    contact_root_c() {} // dummy
    contact_root_c( const contact_key_s &key ):contact_c(key) {}
    ~contact_root_c() { unload_history(); }

    /*virtual*/ void setup(const contacts_s * c, time_t nowtime) override;
    /*virtual*/ bool save(contacts_s * c) const override;
    
    void set_conference_permissions( int grants ) { confa_grants = grants; }
    int get_conference_permissions() const { return confa_grants; }
    void join_conference(contact_root_c *c);

    /*virtual*/ const ts::str_c get_pubid_desc() const override { return compile_pubid(); };
    /*virtual*/ const avatar_s *get_avatar() const override;

    void send_file(ts::wstr_c fn);

    void stop_av();
    
    bool ringtone( contact_c *sub, bool activate = true, bool play_stop_snd = true);
    void av( contact_c *sub, bool f, bool video);
    bool calltone( contact_c *sub, calltone_e ctt ); // returns video flag on call accepted

    bool hasproto(int apid) const
    {
        if ( key.is_conference() )
            return CS_ONLINE == state && key.protoid == (unsigned)apid;

        for ( const contact_c *c : subcontacts )
            if ( c->getkey().protoid == (unsigned)apid ) return true;
        return false;
    }

    int subfind( const contact_c *c ) const
    {
        ts::aint cnt = subcontacts.size();
        for (int i=0;i<cnt;++i)
        {
            if (subcontacts.get( i ) == c)
                return i;
        }
        return -1;
    }

    bool subpresent(const contact_key_s&k) const
    {
        for ( const contact_c *c : subcontacts )
            if (c->getkey() == k) return true;
        return false;
    }
    bool subpresent( const contact_c *sc ) const
    {
        for ( const contact_c *c : subcontacts )
            if ( c == sc ) return true;
        return false;
    }
    contact_c * subgetadd(const contact_key_s&k);
    contact_c * subget_proto(int protoid);
    contact_c * subget(const contact_key_s&k)
    {
        for ( contact_c *c : subcontacts )
            if (c->getkey() == k) return c;
        return nullptr;
    }
    ts::aint subcount() const { return subcontacts.size(); }
    contact_c * subget( ts::aint indx)
    {
        return subcontacts.get(indx);
    }

    contact_c * subget_only_marked_defaul() const;
    contact_c * subget_smart(why_this_subget_e &why) const;
    void subactivity(const contact_key_s &ck);

    void subadd(contact_c *c);
    void subaddconference(contact_c *c);
    bool subdel(contact_c *c);
    bool subremove( contact_c *c );
    void subdelall();
    void subclear() // do not use it!!! this function used only while metacontact creation or cleanup conference
    {
        subcontacts.clear();
    }

    template<typename ITR> void subiterate(ITR itr)
    {
        for (contact_c *c : subcontacts)
            itr(c);
    }
    template<typename ITR> void subiterate( ITR itr ) const
    {
        for ( const contact_c *c : subcontacts )
            itr( c );
    }

    int subonlinecount() const
    {
        int online_count = 0;
        for (contact_c *c : subcontacts)
            if (c->get_state() == CS_ONLINE)
                ++online_count;
        return online_count;
    }
    int subunknowncount() const
    {
        int unknown_count = 0;
        for (contact_c *c : subcontacts)
            if (c->get_state() == CS_UNKNOWN)
                ++unknown_count;
        return unknown_count;
    }

    bool fully_unknown() const
    {
        if ( subcontacts.size() == 0 ) return false;
        for ( contact_c *c : subcontacts )
            if ( c->get_state() != CS_UNKNOWN ) return false;
        return true;
    }

    time_t get_readtime() const { return readtime; }
    void set_readtime(time_t t) { readtime = t; }

    const post_s * fix_history(message_type_app_e oldt, message_type_app_e newt, const contact_key_s& sender = contact_key_s() /* self - empty - no matter */, time_t update_time = 0 /* 0 - no need update */, const ts::str_c *update_text = nullptr /* null - no need update */);
    void change_history_items( const contact_key_s &old_sender, const contact_key_s &new_sender );

    const post_s& get_history( ts::aint index) const { return *history.get(index); }
    ts::aint history_size() const { return history.size(); }

    time_t nowtime() const
    {
        time_t time = ts::now();
        if (history.size() && history.last()->recv_time >= time) time = history.last()->recv_time + 1;
        return time;
    }

#if 0
    void make_time_unique(time_t &t) const;
#endif

    void add_message( const ts::str_c& utf8msg ); // add system message to conversation (don't update history)

    bool b_load( RID, GUIPARAM );
    
    void del_history(); // all
    void del_history(uint64 utag);

    post_s* add_history_unsafe()
    {
        post_s *p = posts.alloc();
        history.add( p );
        return p;
    }

    post_s& add_history()
    {
        history_touch();

        post_s *p = posts.alloc();
        history.add( p );
        p->recv_time = nowtime();
        p->cr_time = p->recv_time;
        return *p;
    }
    post_s& add_history(time_t recv_t, time_t send_t, ts::aint *index = nullptr)
    {
        history_touch();

        ts::aint cnt = history.size();
        for (int i = 0; i < cnt; ++i)
        {
            if (recv_t < history.get(i)->recv_time)
            {
                post_s *p = posts.alloc();
                history.insert(i, p);
                p->recv_time = recv_t;
                p->cr_time = send_t;
                if ( index ) *index = i;
                return *p;
            }
        }
        post_s *p = posts.alloc();
        if ( index ) *index = history.size();
        history.add(p);
        p->recv_time = recv_t;
        p->cr_time = send_t;
        return *p;
    }

    void history_touch()
    {
        last_history_touch = ts::Time::current();
    }
    bool is_ancient_history()
    {
        if ( !keep_history() )
            return false;
        return (ts::Time::current() - last_history_touch) > 10000; // assume 10 seconds is ancient enough
    }

    void load_history( ts::aint n_last_items);
    void unload_history()
    {
        for ( post_s *p : history )
            posts.dealloc( p );
        history.clear();
        flag_history_need_load = true;
    }

    void export_history(const ts::wsptr &templatename, const ts::wsptr &fname);

    ts::aint find_post_index( const post_s *p ) const
    {
        for ( ts::aint i = 0, c = history.size(); i < c; ++i )
            if ( p == history.get( i ) )
                return i;
        return -1;
    }
    ts::aint find_post_index( uint64 utg ) const
    {
        for ( ts::aint i = 0, c = history.size(); i < c; ++i )
            if ( utg == history.get( i )->utag )
                return i;
        return -1;
    }

    const post_s *find_post_by_utag(uint64 utg) const
    {
        const_cast<contact_root_c *>(this)->history_touch();

        for (const post_s *p : history)
            if (p->utag == utg)
                return p;
        return nullptr;
    }

    const post_s *find_post_by_utag( uint64 utg, const ts::ivec2& inrange ) const
    {
        for ( int i = inrange.r0; i <= inrange.r1; ++i )
        {
            const post_s *p = history.get(i);
            if ( p->utag == utg )
                return p;
        }
        return nullptr;
    }

    template<typename F> void iterate_history(F f) const
    {
        const_cast<contact_root_c *>( this )->history_touch();

        for (const post_s *p : history)
            if (f(*p)) return;
    }
    template<typename F> void iterate_history(F f)
    {
        history_touch();
        for (post_s *p : history)
            if (f(*p)) return;
    }
    template<typename F> ts::aint iterate_history_changetime(F f) // return last iterated post index
    {
        history_touch();
        ts::aint r = -1;
        ts::tmp_array_inplace_t<post_s, 4> temp;
        for ( ts::aint i = 0; i < history.size();)
        {
            r = i;
            post_s &p = *history.get(i);
            time_t t = p.recv_time;
            bool cont = f(p);
            if (t != p.recv_time)
            {
                temp.add(p);
                history.remove_slow(i);
                posts.dealloc( &p );
                if (!cont) break;
                continue;
            }
            if (!cont) break;
            ++i;
        }
        for (post_s &p : temp)
        {
            post_s &x = add_history(p.recv_time, p.cr_time, &r);
            x = p;
        }

        return r;
    }

    bool match_tags(int bitags) const;
    void rebuild_tags_bits(const ts::astrings_c &allhts);
    void toggle_tag(const ts::asptr &t);

    const ts::astrings_c &get_tags() const {return tags;}
    void set_tags( const ts::astrings_c &ht ) { tags = ht;}

    void set_comment(const ts::str_c &c_) { comment = c_; }
    const ts::str_c &get_comment() const { return comment; }

    void set_greeting( const ts::asptr &g_ )
    {
        time_t lastg = get_greeting_last();
        int per = get_greeting_period();
        greeting.set_as_num(lastg).append_char('\1').append_as_int(per).append_char( '\2' ).append( g_ );
    }
    void set_greeting_last( time_t lastg )
    {
        int per = get_greeting_period();
        ts::str_c g = get_greeting();
        greeting.set_as_num( lastg ).append_char( '\1' ).append_as_int( per ).append_char( '\2' ).append( g );
    }
    void set_greeting_period( int per )
    {
        time_t lastg = get_greeting_last();
        ts::str_c g = get_greeting();
        greeting.set_as_num( lastg ).append_char( '\1' ).append_as_int( per ).append_char( '\2' ).append( g );
    }

    const ts::pstr_c get_greeting() const { return greeting.substr( greeting.find_pos('\2') + 1 ); }
    time_t get_greeting_last() const
    {
        int i = greeting.find_pos( '\1' );
        if ( i > 0 )
            return greeting.substr( 0, i ).as_num<time_t>();
        return 0;
    }
    int get_greeting_period() const // seconds
    {
        int i = greeting.find_pos( '\1' );
        if ( i > 0 )
        {
            int j = greeting.find_pos( '\2' );
            if (j>i)
                return greeting.substr( i+1, j ).as_int();
        }
        return 60;
    }
    time_t get_greeting_allow_time() const
    {
        int i = greeting.find_pos( '\1' );
        if ( i > 0 )
        {
            time_t l = greeting.substr( 0, i ).as_num<time_t>();
            int j = greeting.find_pos( '\2' );
            if ( j > i )
            {
                int s = greeting.substr( i + 1, j ).as_int();
                return l + s;
            }
            return l + 60;
        }
        return 0;
    }

    ts::str_c compile_pubid() const;
    ts::str_c compile_name() const;
    ts::str_c compile_statusmsg() const;

    ts::wstr_c showname() const;

    contact_state_e get_meta_state() const;
    contact_online_state_e get_meta_ostate() const;
    contact_gender_e get_meta_gender() const;

    keep_contact_history_e get_keeph() const { return keeph; }
    void set_keeph(keep_contact_history_e v) { keeph = v; }

    auto_accept_audio_call_e get_aaac() const { return aaac; }
    void set_aaac(auto_accept_audio_call_e v) { aaac = v; }

    int get_imnb() const { return imnb; }
    void set_imnb( int v ) { imnb = v; }

    void execute_message_handler( const ts::asptr &utf8msg );
    msg_handler_e get_mhtype() const { return mht; }
    void set_mhtype( msg_handler_e v ) { mht = v; }

    const ts::wstr_c &get_mhcmd() const { return mhc; }
    void set_mhcmd( const ts::wstr_c &v ) { mhc = v; }

    //FOLDER_SHARE_PATH
    //const ts::wstr_c &get_share_folder_path() const { return sfp; }
    //void set_share_folder_path(const ts::wstr_c &v) { sfp = v; }

    bool keep_history() const;
    int calc_unread() const;

    bool is_active() const;

    bool is_live() const
    {
        return !getkey().is_conference() || get_state() == CS_ONLINE;
    }

    conference_s *find_conference();
};

INLINE ts::str_c contact_c::get_name(bool metacompile) const { return ( metacompile && name.is_empty() && is_meta() ) ? get_historian()->compile_name() : name; };
INLINE ts::str_c contact_c::get_statusmsg(bool metacompile) const { return ( metacompile && statusmsg.is_empty() && is_meta() ) ? get_historian()->compile_statusmsg() : statusmsg; };

INLINE const contact_root_c *contact_c::get_historian() const { return getmeta() ? getmeta() : dynamic_cast<const contact_root_c *>(this); }
INLINE contact_root_c *contact_c::get_historian() { return getmeta() ? getmeta() : dynamic_cast<contact_root_c *>(this); }


contact_root_c *get_historian(contact_c *sender, contact_c * receiver);

template<> struct gmsg<ISOGM_V_UPDATE_CONTACT> : public gmsgbase
{
    gmsg(contact_c *c, bool refilter) :gmsgbase(ISOGM_V_UPDATE_CONTACT),contact(c), refilter( refilter ) { }
    ts::shared_ptr<contact_c> contact;
    bool refilter;
};


template<> struct gmsg<ISOGM_INCOMING_MESSAGE> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_INCOMING_MESSAGE) {}
    contact_id_s gid;
    contact_id_s cid;
    union
    {
        uint64 create_time;
        uint64 utag; // for some messages
    };
    ts::str_c msgutf8;
    message_type_app_e mt;
    ts::uint16 apid;
};

template<> struct gmsg<ISOGM_FILE> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_FILE) {}
    contact_key_s sender;
    uint64 i_utag = 0; // incoming utag
    uint64 filesize = 0;
    uint64 offset = 0;
    ts::wstr_c filename;
    ts::buf0_c data;
    file_control_e fctl = FIC_NONE;
};

template<> struct gmsg<ISOGM_AVATAR> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_AVATAR) {}
    contact_key_s contact;
    ts::blob_c data;
    int tag = 0;
};

template<> struct gmsg<ISOGM_FOLDER_SHARE> : public gmsgbase
{
    enum data_type_e
    {
        DT_TOC,
        DT_CTL,
        DT_QUERY,
    };

    gmsg(uint64 utag, int apid, data_type_e dt) :gmsgbase(ISOGM_FOLDER_SHARE), utag(utag), apid(apid), dt(dt) {}
    uint64 utag;
    ts::blob_c toc;
    ts::str_c tocname;
    ts::str_c fakename;
    folder_share_control_e ctl = FSC_NONE;
    int apid;
    data_type_e dt;
};

template<> struct gmsg<ISOGM_FOLDER_SHARE_UPDATE> : public gmsgbase
{
    gmsg(uint64 utag) :gmsgbase(ISOGM_FOLDER_SHARE_UPDATE), utag(utag) {}
    uint64 utag;
};

template<> struct gmsg<ISOGM_TYPING> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_TYPING) {}
    contact_key_s contact;
};

template<> struct gmsg<ISOGM_SELECT_CONTACT> : public gmsgbase
{
    gmsg(contact_root_c *c, int options) :gmsgbase(ISOGM_SELECT_CONTACT), contact(c), options(options) {}
    ts::shared_ptr<contact_root_c> contact;
    int options;
};

template<> struct gmsg<ISOGM_INIT_CONVERSATION> : public gmsgbase
{
    gmsg( contact_root_c *c, int options, RID crid ) :gmsgbase( ISOGM_INIT_CONVERSATION ), contact( c ), options( options ), conversation(crid) {}
    ts::shared_ptr<contact_root_c> contact;
    RID conversation;
    int options;
};

template<> struct gmsg<ISOGM_CALL_STOPED> : public gmsgbase
{
    gmsg(contact_c *c) :gmsgbase(ISOGM_CALL_STOPED), subcontact(c) { ASSERT(!c->is_meta()); }
    ts::shared_ptr<contact_c> subcontact;
};

uint64 uniq_history_item_tag();
template<> struct gmsg<ISOGM_MESSAGE> : public gmsgbase
{
    post_s post;

    gmsg( uint64 utag ) :gmsgbase(ISOGM_MESSAGE)
    {
        post.utag = utag ? utag : uniq_history_item_tag();
        post.recv_time = 0; // initialized after history add
        post.cr_time = 0;
        post.type = MTA_MESSAGE;
    }
    gmsg(contact_c *sender, contact_c *receiver, message_type_app_e mt, uint64 utag );

    contact_c *sender = nullptr;
    contact_c *receiver = nullptr;
    bool current = false;
    bool resend = false;
    bool info = false;

    contact_root_c *get_historian()
    {
        return ::get_historian(sender, receiver);
    }
};


class contacts_c
{
    friend struct contact_key_s;

    GM_RECEIVER(contacts_c, ISOGM_PROFILE_TABLE_SL );
    GM_RECEIVER(contacts_c, ISOGM_UPDATE_CONTACT);
    GM_RECEIVER(contacts_c, ISOGM_INCOMING_MESSAGE);
    GM_RECEIVER(contacts_c, ISOGM_FILE);
    GM_RECEIVER(contacts_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(contacts_c, ISOGM_DELIVERED);
    GM_RECEIVER(contacts_c, ISOGM_NEWVERSION);
    GM_RECEIVER(contacts_c, ISOGM_AVATAR);
    GM_RECEIVER(contacts_c, GM_UI_EVENT);
    GM_RECEIVER(contacts_c, ISOGM_PROTO_CRASHED);
    GM_RECEIVER(contacts_c, ISOGM_PROTO_LOADED);

    ts::tbuf_t<contact_key_s> activity; // last active historian at end of array
    contacts_array_t arr;
    ts::shared_ptr<contact_root_c> self;

    bool is_conference_member( const contact_key_s &ck, bool strong_check = false );

    int find_free_meta_id() const;
    void delbykey( const contact_key_s&k )
    {
        ts::aint index;
        if (arr.find_sorted(index, k))
        {
            contact_c *c = arr.get(index);
            c->prepare4die(nullptr);
            arr.remove_slow(index);
        }

    }

    ts::astrings_c all_tags;
    ts::buf0_c enabled_tags;

    int sorttag = 0;
    int cleanup_index = 0;

public:

    contacts_c();
    ~contacts_c();

    const ts::astrings_c &get_all_tags() const { return all_tags; }
    const ts::buf0_c &get_tags_bits() const { return enabled_tags; };

    ts::str_c find_pubid(int protoid) const;
    contact_c *find_subself(int protoid) const;

    ts::wstr_c calc_self_name( int protoid ) const;

    contact_root_c *create_new_meta();

    void nomore_proto(int id);
    bool present_protoid(int id) const;

    bool present( const contact_key_s&k ) const
    {
        ts::aint index;
        return arr.find_sorted(index,k);
    }

    const contact_root_c *rfind(const contact_key_s&k) const
    {
        ts::aint index;
        if (arr.find_sorted(index, k)) return ts::ptr_cast<const contact_root_c *>( arr.get(index).get() );
        return nullptr;
    }
    contact_root_c *rfind( const contact_key_s&k )
    {
        ts::aint index;
        if ( arr.find_sorted( index, k ) ) return ts::ptr_cast<contact_root_c *>( arr.get( index ).get() );
        return nullptr;
    }
    contact_root_c *rfind( const contact_key_s&k, ts::aint &index )
    {
        if ( arr.find_sorted( index, k ) ) return ts::ptr_cast<contact_root_c *>( arr.get( index ).get() );
        return nullptr;
    }

    contact_c *find( const contact_key_s&k )
    {
        ts::aint index;
        if (arr.find_sorted(index, k)) return arr.get(index);
        return nullptr;
    }
    const contact_c *find(const contact_key_s&k) const
    {
        ts::aint index;
        if (arr.find_sorted(index, k)) return arr.get(index);
        return nullptr;
    }

    const contact_root_c & get_self() const { return *self; }
    contact_root_c & get_self() { return *self; }

    void kill(const contact_key_s &ck, bool kill_with_history = false);

    void update_roots();

    void change_key( const contact_key_s &oldkey, const contact_key_s &newkey );

    ts::aint count() const {return arr.size();}
    contact_c & get(int index) {return *arr.get(index);};

    template <typename R> void iterate_proto_contacts( R r, int proto = 0 )
    {
        for( contact_c *c : arr )
            if (!c->is_meta() && c->getkey().protoid)
                if (proto == 0 || static_cast<ts::uint16>(proto) == c->getkey().protoid)
                    if (!r( c )) break;
    }
    template <typename R> void iterate_root_contacts(R r)
    {
        for (contact_c *c : arr)
            if (c->is_rootcontact())
                if (!r( ts::ptr_cast<contact_root_c *>(c) )) break;
    }

    void unload(); // called before profile switch

    int sort_tag() const { return sorttag; }
    void resort_list() { ++sorttag; };

    void contact_activity( const contact_key_s &ck );
    ts::aint contact_activity_power(const contact_key_s &ck) const
    {
        ASSERT(ck.is_meta() || ck.is_conference());
        ts::aint cnt = activity.count();
        for ( ts::aint i = 0; i < cnt; ++i)
            if (activity.get(i) == ck)
                return i;
        return -1;
    }

    void replace_tags( int i, const ts::str_c &ntn);
    void rebuild_tags_bits(bool refresh_ui = true);
    void toggle_tag( ts::aint i);
    bool is_tag_enabled( ts::aint i) const { return enabled_tags.get_bit(i); };

    void cleanup(); // tries to free some memory; can be rarely called (once per 5 seconds)
};

extern ts::static_setup<contacts_c> contacts;

