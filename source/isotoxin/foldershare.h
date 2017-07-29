#pragma once

struct folder_share_toc_packed_s;

enum fshevent_e
{
    FSHE_DOWNLOADED,
    FSHE_FAILED,
    FSHE_NOSPACE,
};

struct folder_share_toc_unpacked_s : public ts::movable_flag<true>
{
    struct toc_element_s : public ts::movable_flag<true>
    {
        ts::wstr_c fn;
        uint64 sz;
        uint64 modtime;

        static const ts::flags32_s::BITS S_DELETED = SETBIT(0);
        static const ts::flags32_s::BITS S_NEW = SETBIT(1);
        static const ts::flags32_s::BITS S_SZ_CHANGED = SETBIT(2);
        static const ts::flags32_s::BITS S_TM_CHANGED = SETBIT(3);
        static const ts::flags32_s::BITS S_DOWNLOADED = SETBIT(4);
        static const ts::flags32_s::BITS S_LOCKED = SETBIT(5);
        static const ts::flags32_s::BITS S_NOSPACE = SETBIT(6);
        static const ts::flags32_s::BITS S_REFRESH = SETBIT(7);

        ts::flags32_s status;

        bool is_dir() const { return sz == 0xffffffffffffffffull; }
        bool should_be_downloaded() const
        {
            return status.is(S_NEW | S_SZ_CHANGED | S_TM_CHANGED | S_REFRESH) && !status.is(S_DOWNLOADED | S_LOCKED | S_NOSPACE);
        }
        void fevent(fshevent_e e);

        int operator()(const toc_element_s&oe) const
        {
            int c = ts::wstr_c::compare(fn, oe.fn);
            if (c != 0) return c;
            if (sz > oe.sz) return -1; // bigger - first
            if (sz < oe.sz) return 1;
            if (modtime > oe.modtime) return -1; // newer - first
            if (modtime < oe.modtime) return 1;
            return 0;
        }

    };
    ts::array_inplace_t<toc_element_s, 128> elements;

    void decode(const folder_share_toc_packed_s &toc);
    void merge(const folder_share_toc_unpacked_s &from);
    toc_element_s &findcreate(const toc_element_s& el);
};

struct folder_share_toc_packed_s : public ts::movable_flag<true>
{
    ts::blob_c bin;
    ts::str_c tochash;

    void encode(const folder_share_toc_unpacked_s &toc);
};

class folder_share_c
{
public:
    enum fsstate_e
    {
        FSS_WORKING,
        FSS_MOVING_FOLDER,
        FSS_SUSPEND,
        FSS_REJECT,
    };

    struct pstate_s
    {
        unsigned apid : 16;
        unsigned announced : 1;
        unsigned accepted : 1;
        unsigned present : 1;
    };

protected:

    folder_share_c(contact_key_s k, const ts::str_c &name, uint64 utag);

    virtual void set_state(fsstate_e st_, bool updatenotice) { st = st_; if (updatenotice) update_data(); }

    uint64 utag;
    ts::tbuf0_t<pstate_s> apids;
    contact_key_s hkey;
    ts::str_c name;
    ts::wstr_c path;
    fsstate_e st = FSS_WORKING;

public:
    virtual ~folder_share_c() {}

    bool is_announce_present() const
    {
        for (const pstate_s &s : apids) if (s.announced) return true;
        return false;
    }

    bool is_multiapids() const
    {
        return apids.count() > 1;
    }

    template<typename X> void iterate_apids(const X &x)
    {
        for (const folder_share_c::pstate_s &s : apids)
            x(s);
    }

    pstate_s &getcreate(int apid);

    contact_key_s get_hkey() const { return hkey; };
    uint64 get_utag() const { return utag; }
    const ts::str_c &get_name() const { return name; }
    const ts::wstr_c &get_path() const { return path; }
    fsstate_e get_state() const { return st; }

    virtual bool tick5() { return true; };
    virtual void set_path(const ts::wstr_c &path, bool updatenotice = true) = 0;
    virtual void set_name(const ts::str_c &name_, bool updatenotice = true) { name = name_; if (updatenotice) update_data(); }
    virtual void refresh() {}
    virtual bool is_type(folder_share_s::fstype_e t) const = 0;

    void explore();

    void update_data(); // update notice and db
    void send_ctl(folder_share_control_e ctl);
    static folder_share_c *build(folder_share_s::fstype_e t, contact_key_s k, const ts::str_c &name, uint64 utag);

};

class folder_share_send_c : public folder_share_c, public ts::folder_spy_handler_s
{
    GM_RECEIVER(folder_share_send_c, ISOGM_FOLDER_SHARE);

    folder_share_toc_packed_s toc;

    ts::uint32 spyid = 0;
    int tocver = 0;
    ts::Time next_toc_refresh = ts::Time::past();
    bool scaning = false;
    bool refresh_after_scan = false;

    /*virtual*/ bool is_type(folder_share_s::fstype_e t) const override { return t == folder_share_s::FST_SEND; }
    /*virtual*/ void set_state(fsstate_e st_, bool update_notice) override;
    /*virtual*/ void set_path(const ts::wstr_c &path, bool updatenotice) override;
    /*virtual*/ bool tick5() override;
    /*virtual*/ void refresh() override;

    /*virtual*/ void change_event(ts::uint32 spyid);


public:
    folder_share_send_c(contact_key_s k, const ts::str_c &name_, uint64 utag) :folder_share_c(k, name_, utag)
    {
    }
    bool is_scaning() const { return scaning; }

    void fin_scan(folder_share_toc_packed_s &&toc_pack);
    const folder_share_toc_packed_s& get_toc() const { return toc; }

    bool send_toc(contact_root_c *h);
    bool share_folder_announce(contact_root_c *h);
    void again();

};

class folder_share_recv_c : public folder_share_c
{
    GM_RECEIVER(folder_share_recv_c, ISOGM_FOLDER_SHARE);
    GM_RECEIVER(folder_share_recv_c, ISOGM_V_UPDATE_CONTACT);

    void * worker = nullptr;
    spinlock::syncvar<folder_share_toc_unpacked_s> toc;
    int tocver = 0;

    ~folder_share_recv_c();
    /*virtual*/ bool is_type(folder_share_s::fstype_e t) const override { return t == folder_share_s::FST_RECV; }
    /*virtual*/ void set_path(const ts::wstr_c &path, bool updatenotice) override;
    /*virtual*/ void refresh() override;

    void run_worker();

public:

    folder_share_recv_c(contact_key_s k, const ts::str_c &name_, uint64 utag) :folder_share_c(k, name_, utag)
    {
    }
    bool recv_waiting_file(int xtag, ts::wstr_c &fnpath);
    void accept(); // doesnt update notice
    void accept(ts::wstr_c&path_ /*not const - not bug - it can be modified - just optimization*/); // doesnt update notice
    void fshevent(int xtag, fshevent_e);
    int get_apid() const;
    spinlock::syncvar<folder_share_toc_unpacked_s>::WRITER lock_toc(int &tocver);
    void fin_decode(folder_share_toc_unpacked_s &&toc);

};
