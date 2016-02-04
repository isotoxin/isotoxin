#pragma once

enum message_type_app_e : unsigned
{
    MTA_MESSAGE = MT_MESSAGE,
    MTA_ACTION = MT_ACTION,
    MTA_FRIEND_REQUEST = MT_FRIEND_REQUEST,
    MTA_INCOMING_CALL = MT_INCOMING_CALL,
    MTA_CALL_STOP = MT_CALL_STOP,
    MTA_CALL_ACCEPTED__ = MT_CALL_ACCEPTED,

    // not saved to db
    MTA_SPECIAL,
    MTA_DATE_SEPARATOR,
    MTA_TYPING,

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
};

static_assert( MTA_FRIEND_REQUEST == 2, "!" );
static_assert(MTA_INCOMING_CALL_CANCELED == 100, "!");
static_assert( MTA_OLD_REQUEST == 103, "!" );
static_assert( MTA_INCOMING_CALL_REJECTED == 104, "!" );

INLINE bool is_special_mt(message_type_app_e mt)
{
    if (mt == MTA_UNDELIVERED_MESSAGE) return false;
    return mt > message_type_app_values;
    //switch(mt)
    //{
    //    case MTA_OLD_REQUEST:
    //    case MTA_INCOMING_CALL_CANCELED:
    //    case MTA_INCOMING_CALL_REJECTED:
    //        return true;
    //}
    //return false;
}

enum buildin_tags_e
{
    BIT_ALL,
    BIT_ONLINE,
    BIT_UNTAGGED,

    BIT_count
};

struct contact_key_s
{
    DUMMY(contact_key_s);

    int contactid;  // protocol contact id. >0 - contact, <0 - group
    int protoid;    // 0 - metacontact. all visible contacts are metacontacts with history and conversation / >0 - protocol contact

    explicit contact_key_s( const ts::asptr&s )
    {
        ts::ref_cast<int64>(*this) = ts::pstr_c(s).as_num<int64>();
    }
    explicit contact_key_s(int contactid = 0, int protoid = 0):contactid(contactid), protoid(protoid) {}

    bool is_meta() const {return protoid == 0 && contactid > 0;}
    bool is_group() const {return contactid < 0 && protoid > 0;}
    bool is_self() const {return ts::ref_cast<int64>(*this) == 0; }
    bool is_empty() const {return ts::ref_cast<int64>(*this) == 0; } //-V524

    bool operator<(const contact_key_s&oc) const { return ts::ref_cast<int64>(*this) < ts::ref_cast<int64>(oc); }
    bool operator==(const contact_key_s&oc) const { return ts::ref_cast<int64>(*this) == ts::ref_cast<int64>(oc); }
    bool operator!=(const contact_key_s&oc) const { return ts::ref_cast<int64>(*this) != ts::ref_cast<int64>(oc); }
    int operator()(const contact_key_s&oc) const 
    { 
        return (int)ts::isign( ts::ref_cast<int64>(oc) - ts::ref_cast<int64>(*this) );
    }

    ts::str_c as_str() const
    {
        ts::str_c s;
        s.set_as_num<int64>( ts::ref_cast<int64>(*this) );
        return s;
    }
};

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
    return ts::calc_hash(ck.contactid) ^ ts::hash_func(ts::calc_hash(ck.protoid));
}

INLINE ts::asptr  calc_message_skin(message_type_app_e mt, const contact_key_s &sender)
{
    if (MTA_OLD_REQUEST == mt)
        return CONSTASTR("invite");
    if (MTA_ACCEPTED == mt || MTA_ACCEPT_OK == mt)
        return CONSTASTR("invite");
    if (MTA_RECV_FILE == mt)
        return CONSTASTR("filerecv");
    if (MTA_SEND_FILE == mt)
        return CONSTASTR("filesend");
    bool by_self = sender.is_self();
    return by_self ? CONSTASTR("mine") : CONSTASTR("other");
}

// initialization: [POST_INIT]
struct post_s
{
    DUMMY(post_s);
    post_s() {}
#if _USE_32BIT_TIME_T
#error "time_t must be 64 bit"
#endif

    static const int type_size_bits = 8;
    static const int options_size_bits = 8;

    uint64 utag;
    time_t recv_time = 0;
    time_t cr_time = 0;
    ts::str_c message_utf8;
    contact_key_s sender;
    contact_key_s receiver;
    unsigned type : type_size_bits;
    unsigned options : options_size_bits;

    const time_t &get_crtime() const { return cr_time != 0 ? cr_time : recv_time; }
    message_type_app_e mt() const {return (message_type_app_e)type;}
};

struct avatar_s : public ts::bitmap_c
{
    int tag = 0;
    bool alpha_pixels = false;
    avatar_s( const void *body, int size, int tag ) { load(body, size, tag); }
    void load( const void *body, int size, int tag );
};

template<> struct gmsg<ISOGM_UPDATE_CONTACT> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_UPDATE_CONTACT) {}
    contact_key_s key;
    int mask;
    ts::str_c pubid;
    ts::str_c name;
    ts::str_c statusmsg;
    ts::str_c details;
    int avatar_tag = 0;
    contact_state_e state = CS_INVITE_SEND;
    contact_online_state_e ostate = COS_ONLINE;
    contact_gender_e gender = CSEX_UNKNOWN;

    ts::tbuf_t<int> members;
};

template<> struct gmsg<ISOGM_PEER_STREAM_OPTIONS> : public gmsgbase
{
    gmsg(const contact_key_s &ck, int so, const ts::ivec2 &sz) :gmsgbase(ISOGM_PEER_STREAM_OPTIONS), ck(ck), so(so), videosize(sz) {}
    contact_key_s ck;
    int so;
    ts::ivec2 videosize;
};


enum keep_contact_history_e
{
    KCH_DEFAULT,
    KCH_ALWAYS_KEEP,
    KCH_NEVER_KEEP,
};

enum auto_accept_audio_call_e
{
    AAAC_NOT,
    AAAC_ACCEPT_MUTE_MIC,
    AAAC_ACCEPT,
};

enum reselect_options_e
{
    RSEL_SCROLL_END = 1,
    RSEL_INSERT_NEW = 2,
};

//-V:options():807

class gui_contact_item_c;
struct contacts_s;
class contact_root_c;
class contact_c : public ts::shared_object
{
    friend class contact_root_c;

    DUMMY(contact_c);
    contact_key_s key;
    ts::shared_ptr<contact_root_c> metacontact; // valid for non-meta contacts
    ts::str_c pubid;
    ts::str_c name;
    ts::str_c customname;
    ts::str_c statusmsg;
    ts::str_c details;
    UNIQUE_PTR( avatar_s ) avatar;

    // TODO: lower memory usage: use bits
    contact_state_e state = CS_OFFLINE;
    contact_online_state_e ostate = COS_ONLINE;
    contact_gender_e gender = CSEX_UNKNOWN;

    typedef ts::flags_t<ts::uint32, 0xFFFF> Options;
    Options opts;

    void setmeta(contact_root_c *metac);

public:

    static const ts::flags32_s::BITS F_DEFALUT      = SETBIT(0);
    static const ts::flags32_s::BITS F_AVA_DEFAULT  = SETBIT(1);
    static const ts::flags32_s::BITS F_UNKNOWN      = SETBIT(2);


    // not saved
    static const ts::flags32_s::BITS F_LAST_ACTIVITY = SETBIT(23);
    static const ts::flags32_s::BITS F_FULL_SEARCH_RESULT = SETBIT(24);
    static const ts::flags32_s::BITS F_AUDIO_GCHAT = SETBIT(25);
    static const ts::flags32_s::BITS F_PERSISTENT_GCHAT = SETBIT(26);
    static const ts::flags32_s::BITS F_PROTOHIT = SETBIT(27);
    static const ts::flags32_s::BITS F_CALLTONE = SETBIT(28);
    static const ts::flags32_s::BITS F_AV_INPROGRESS = SETBIT(29);
    static const ts::flags32_s::BITS F_RINGTONE = SETBIT(30);
    static const ts::flags32_s::BITS F_DIP = SETBIT(31);

    int operator()(const contact_c&oc) const { return key(oc.key); }
    int operator()(const contact_key_s&k) const { return key(k); }

    const contact_key_s& getkey() const {return key;}

    explicit contact_c( const contact_key_s &key );
    contact_c();
    virtual ~contact_c();

    virtual void setup(const contacts_s * c, time_t nowtime);
    virtual bool save(contacts_s * c) const;

    void prepare4die(contact_root_c *owner);

    void reselect(int options = RSEL_SCROLL_END);

    void redraw();
    void redraw(float delay);

    bool is_protohit( bool strong );
    void protohit(bool f);

    bool is_meta() const { return key.is_meta() && getmeta() == nullptr; }; // meta, but not group
    bool is_rootcontact() const { return !opts.is(F_UNKNOWN) && (is_meta() || getkey().is_group() || getkey().is_self()); } // root contact - in contact list

    const Options &get_options() const {return opts;}
    Options &options() {return opts;}

    contact_root_c *getmeta() {return metacontact;}
    const contact_root_c *getmeta() const {return metacontact;}

    const contact_root_c *get_historian() const;
    contact_root_c *get_historian();

    void set_pubid( const ts::str_c &pubid_ ) { pubid = pubid_; }
    void set_name( const ts::str_c &name_ ) { name = name_; }
    void set_customname( const ts::str_c &name_ ) { customname = name_; }
    void set_statusmsg( const ts::str_c &statusmsg_ ) { statusmsg = statusmsg_; }
    void set_details( const ts::str_c &details_ ) { details = details_; }
    void set_state( contact_state_e st ) { state = st; opts.init(F_UNKNOWN, st == CS_UNKNOWN); }
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
    contact_state_e get_state() const {return state;}
    contact_online_state_e get_ostate() const {return ostate;}
    contact_gender_e get_gender() const {return gender;}

    bool authorized() const { return get_state() == CS_OFFLINE || get_state() == CS_ONLINE; }

    void accept_call( auto_accept_audio_call_e aaac, bool video );

    bool b_accept(RID, GUIPARAM);
    bool b_reject(RID, GUIPARAM);
    bool b_resend(RID, GUIPARAM);
    bool b_kill(RID, GUIPARAM);
    bool b_load(RID, GUIPARAM);
    bool b_accept_call_with_video(RID, GUIPARAM);
    bool b_accept_call(RID, GUIPARAM);
    bool b_reject_call(RID, GUIPARAM);
    bool b_hangup(RID, GUIPARAM);
    bool b_call(RID, GUIPARAM);
    bool b_cancel_call(RID, GUIPARAM);

    bool b_receive_file(RID, GUIPARAM);
    bool b_receive_file_as(RID, GUIPARAM);
    bool b_refuse_file(RID, GUIPARAM);
    
    virtual void stop_av();

    virtual bool ringtone(bool activate = true, bool play_stop_snd = true);
    virtual void av(bool f, bool video);
    virtual bool calltone(bool f = true, bool call_accepted = false);

    bool is_ringtone() const { return opts.unmasked().is(F_RINGTONE); }
    bool is_av() const { return opts.unmasked().is(F_AV_INPROGRESS) || (getkey().is_group() && opts.unmasked().is(F_AUDIO_GCHAT)); }
    bool is_calltone() const { return opts.unmasked().is(F_CALLTONE); }

    int avatar_tag() const {return avatar ? avatar->tag : 0; }
    void set_avatar( const void *body, int size, int tag = 0 )
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

class contact_root_c : public contact_c // metas and groups
{
    DUMMY(contact_root_c);
    ts::array_shared_t<contact_c, 0> subcontacts; // valid for meta contact
    ts::array_inplace_t<post_s, 128> history; // valid for meta contact
    ts::str_c comment;
    ts::astrings_c tags;
    ts::buf0_c tags_bits; // rebuilded

    time_t readtime = 0; // all messages after readtime considered unread
    keep_contact_history_e keeph = KCH_DEFAULT;
    auto_accept_audio_call_e aaac = AAAC_NOT;


public:
    ts::safe_ptr<gui_contact_item_c> gui_item;

    contact_root_c() {} // dummy
    contact_root_c( const contact_key_s &key ):contact_c(key) {}
    ~contact_root_c() {}

    /*virtual*/ void setup(const contacts_s * c, time_t nowtime) override;
    /*virtual*/ bool save(contacts_s * c) const override;
    
    void join_groupchat(contact_root_c *c);

    /*virtual*/ const ts::str_c get_pubid_desc() const override { return compile_pubid(); };
    /*virtual*/ const avatar_s *get_avatar() const override;

    ts::wstr_c contactidfolder() const;
    void send_file(const ts::wstr_c &fn);

    /*virtual*/ void stop_av() override;
    
    /*virtual*/ bool ringtone(bool activate = true, bool play_stop_snd = true) override;
    /*virtual*/ void av(bool f, bool video) override;
    /*virtual*/ bool calltone(bool f = true, bool call_accepted = false) override;

    bool subpresent(const contact_key_s&k) const
    {
        for (contact_c *c : subcontacts)
            if (c->getkey() == k) return true;
        return false;
    }
    contact_c * subgetadd(const contact_key_s&k);
    contact_c * subget_proto(int protoid);
    contact_c * subget(const contact_key_s&k)
    {
        for (contact_c *c : subcontacts)
            if (c->getkey() == k) return c;
        return nullptr;
    }
    int subcount() const { return subcontacts.size(); }
    contact_c * subget(int indx)
    {
        return subcontacts.get(indx);
    }

    contact_c * subget_default() const;
    contact_c * subget_for_send() const;
    void subactivity(const contact_key_s &ck);

    void subadd(contact_c *c);
    void subaddgchat(contact_c *c);
    bool subdel(contact_c *c);
    void subdelall();
    void subclear() // do not use it!!! this function used only while metacontact creation or cleanup group
    {
        subcontacts.clear();
    }

    template<typename ITR> void subiterate(ITR itr)
    {
        for (contact_c *c : subcontacts)
            itr(c);
    }

    int subonlinecount() const
    {
        int online_count = 0;
        for (contact_c *c : subcontacts)
            if (c->get_state() == CS_ONLINE)
                ++online_count;
        return online_count;
    }


    time_t get_readtime() const { return readtime; }
    void set_readtime(time_t t) { readtime = t; }

    const post_s * fix_history(message_type_app_e oldt, message_type_app_e newt, const contact_key_s& sender = contact_key_s() /* self - empty - no matter */, time_t update_time = 0 /* 0 - no need update */, const ts::str_c *update_text = nullptr /* null - no need update */);

    const post_s& get_history(int index) const { return history.get(index); }
    int history_size() const { return history.size(); }

    time_t nowtime() const
    {
        time_t time = now();
        if (history.size() && history.last().recv_time >= time) time = history.last().recv_time + 1;
        return time;
    }
    void make_time_unique(time_t &t) const;

    void del_history(uint64 utag);

    post_s& add_history()
    {
        post_s &p = history.add();
        p.recv_time = nowtime();
        p.cr_time = p.recv_time;
        return p;
    }
    post_s& add_history(time_t recv_t, time_t send_t)
    {
        int cnt = history.size();
        for (int i = 0; i < cnt; ++i)
        {
            if (recv_t < history.get(i).recv_time)
            {
                post_s &p = history.insert(i);
                p.recv_time = recv_t;
                p.cr_time = send_t;
                return p;
            }
        }
        post_s &p = history.add();
        p.recv_time = recv_t;
        p.cr_time = send_t;
        return p;
    }

    //void load_history(); // whole
    void load_history(int n_last_items);
    void unload_history()
    {
        history.clear();
    }

    void export_history(const ts::wsptr &templatename, const ts::wsptr &fname);

    const post_s *find_post_by_utag(uint64 utg) const
    {
        for (const post_s &p : history)
            if (p.utag == utg)
                return &p;
        return nullptr;
    }

    template<typename F> void iterate_history(F f) const
    {
        for (const post_s &p : history)
            if (f(p)) return;
    }
    template<typename F> void iterate_history(F f)
    {
        for (post_s &p : history)
            if (f(p)) return;
    }
    template<typename F> int iterate_history_changetime(F f) // return last iterated post index
    {
        int r = -1;
        ts::tmp_array_inplace_t<post_s, 4> temp;
        for (int i = 0; i < history.size();)
        {
            r = i;
            post_s &p = history.get(i);
            time_t t = p.recv_time;
            bool cont = f(p);
            if (t != p.recv_time)
            {
                temp.add(p);
                history.remove_slow(i);
                if (!cont) break;
                continue;
            }
            if (!cont) break;
            ++i;
        }
        for (post_s &p : temp)
        {
            post_s *x = &add_history(p.recv_time, p.cr_time);
            *x = p;
            r = x - history.begin();
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

    ts::str_c compile_pubid() const;
    ts::str_c compile_name() const;
    ts::str_c compile_statusmsg() const;

    contact_state_e get_meta_state() const;
    contact_online_state_e get_meta_ostate() const;
    contact_gender_e get_meta_gender() const;

    keep_contact_history_e get_keeph() const { return keeph; }
    void set_keeph(keep_contact_history_e v) { keeph = v; }

    auto_accept_audio_call_e get_aaac() const { return aaac; }
    void set_aaac(auto_accept_audio_call_e v) { aaac = v; }

    bool keep_history() const;
    int calc_unread() const;

    bool is_full_search_result() const { return opts.unmasked().is(F_FULL_SEARCH_RESULT); }
    void full_search_result(bool f) { opts.unmasked().init(F_FULL_SEARCH_RESULT, f); }
};

INLINE ts::str_c contact_c::get_name(bool metacompile) const { return (name.is_empty() && is_meta() && metacompile) ? get_historian()->compile_name() : name; };
INLINE ts::str_c contact_c::get_statusmsg(bool metacompile) const { return (statusmsg.is_empty() && is_meta() && metacompile) ? get_historian()->compile_statusmsg() : statusmsg; };

INLINE const contact_root_c *contact_c::get_historian() const { return getmeta() ? getmeta() : dynamic_cast<const contact_root_c *>(this); }
INLINE contact_root_c *contact_c::get_historian() { return getmeta() ? getmeta() : dynamic_cast<contact_root_c *>(this); }


contact_root_c *get_historian(contact_c *sender, contact_c * receiver);

template<> struct gmsg<ISOGM_V_UPDATE_CONTACT> : public gmsgbase
{
    gmsg(contact_c *c) :gmsgbase(ISOGM_V_UPDATE_CONTACT),contact(c) { ASSERT(c->get_state() != CS_UNKNOWN); }
    ts::shared_ptr<contact_c> contact;
};


template<> struct gmsg<ISOGM_INCOMING_MESSAGE> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_INCOMING_MESSAGE) {}
    contact_key_s groupchat;
    contact_key_s sender;
    time_t create_time;
    message_type_app_e mt;
    ts::str_c msgutf8;
};

template<> struct gmsg<ISOGM_FILE> : public gmsgbase
{
    gmsg() :gmsgbase(ISOGM_FILE) {}
    contact_key_s sender;
    uint64 utag = 0;
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

template<> struct gmsg<ISOGM_CALL_STOPED> : public gmsgbase
{
    gmsg(contact_c *c) :gmsgbase(ISOGM_CALL_STOPED), subcontact(c) { ASSERT(!c->is_meta()); }
    ts::shared_ptr<contact_c> subcontact;
};

uint64 uniq_history_item_tag();
template<> struct gmsg<ISOGM_MESSAGE> : public gmsgbase
{
    post_s post;

    gmsg() :gmsgbase(ISOGM_MESSAGE)
    {
        post.utag = uniq_history_item_tag();
        post.recv_time = 0; // initialized after history add
        post.cr_time = 0;
        post.type = MTA_MESSAGE;
    }
    gmsg(contact_c *sender, contact_c *receiver, message_type_app_e mt = MTA_MESSAGE);

    contact_c *sender = nullptr;
    contact_c *receiver = nullptr;
    bool current = false;
    bool resend = false;

    contact_root_c *get_historian()
    {
        return ::get_historian(sender, receiver);
    }
};



class contacts_c
{
    GM_RECEIVER(contacts_c, ISOGM_PROFILE_TABLE_LOADED);
    GM_RECEIVER(contacts_c, ISOGM_UPDATE_CONTACT);
    GM_RECEIVER(contacts_c, ISOGM_INCOMING_MESSAGE);
    GM_RECEIVER(contacts_c, ISOGM_FILE);
    GM_RECEIVER(contacts_c, ISOGM_CHANGED_SETTINGS);
    GM_RECEIVER(contacts_c, ISOGM_DELIVERED);
    GM_RECEIVER(contacts_c, ISOGM_NEWVERSION);
    GM_RECEIVER(contacts_c, ISOGM_AVATAR);
    GM_RECEIVER(contacts_c, GM_UI_EVENT);
    
    uint64 sorttag;

    ts::tbuf_t<contact_key_s> activity; // last active historian at end of array
    ts::array_shared_t<contact_c, 8> arr;
    ts::shared_ptr<contact_root_c> self;

    bool is_groupchat_member( const contact_key_s &ck );

    int find_free_meta_id() const;
    void delbykey( const contact_key_s&k )
    {
        int index;
        if (arr.find_sorted(index, k))
        {
            contact_c *c = arr.get(index);
            c->prepare4die(nullptr);
            arr.remove_slow(index);
        }

    }

    ts::astrings_c all_tags;
    ts::buf0_c enabled_tags;


public:

    contacts_c();
    ~contacts_c();

    const ts::astrings_c &get_all_tags() const { return all_tags; }
    const ts::buf0_c &get_tags_bits() const { return enabled_tags; };

    ts::str_c find_pubid(int protoid) const;
    contact_c *find_subself(int protoid) const;

    contact_root_c *create_new_meta();

    void nomore_proto(int id);
    bool present_protoid(int id) const;

    bool present( const contact_key_s&k ) const
    {
        int index;
        return arr.find_sorted(index,k);
    }

    contact_root_c *rfind(const contact_key_s&k)
    {
        int index;
        if (arr.find_sorted(index, k)) return ts::ptr_cast<contact_root_c *>( arr.get(index).get() );
        return nullptr;
    }

    contact_c *find( const contact_key_s&k )
    {
        int index;
        if (arr.find_sorted(index, k)) return arr.get(index);
        return nullptr;
    }
    const contact_c *find(const contact_key_s&k) const
    {
        int index;
        if (arr.find_sorted(index, k)) return arr.get(index);
        return nullptr;
    }

    const contact_root_c & get_self() const { return *self; }
    contact_root_c & get_self() { return *self; }

    void kill(const contact_key_s &ck, bool kill_with_history = false);

    void update_meta();

    int count() const {return arr.size();}
    contact_c & get(int index) {return *arr.get(index);};

    template <typename R> void iterate_proto_contacts( R r )
    {
        for( contact_c *c : arr )
            if (!c->is_meta())
                if (!r(c)) break;
    }
    template <typename R> void iterate_meta_contacts(R r)
    {
        for (contact_c *c : arr)
            if (c->is_meta())
                if (!r( ts::ptr_cast<contact_root_c *>(c) )) break;
    }

    void unload(); // called before profile switch

    uint64 sort_tag() const { return sorttag; }
    void dirty_sort() { sorttag = ts::uuid(); };

    void contact_activity( const contact_key_s &ck );
    int contact_activity_power(const contact_key_s &ck) const
    {
        ASSERT(ck.is_meta() || ck.is_group());
        int cnt = activity.count();
        for (int i = 0; i < cnt; ++i)
            if (activity.get(i) == ck)
                return i;
        return -1;
    }

    void replace_tags(int i, const ts::str_c &ntn);
    void rebuild_tags_bits(bool refresh_ui = true);
    void toggle_tag(int i);
    bool is_tag_enabled(int i) const { return enabled_tags.get_bit(i); };
};

extern ts::static_setup<contacts_c> contacts;

