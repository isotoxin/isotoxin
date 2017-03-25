#include "isotoxin.h"

void folder_share_toc_unpacked_s::toc_element_s::fevent(fshevent_e e)
{
    switch (e)
    {
    case FSHE_DOWNLOADED:
        status.clear(S_SZ_CHANGED | S_TM_CHANGED | S_NEW | S_REFRESH);
        status.set(S_DOWNLOADED);
        break;
    case FSHE_FAILED:
        status.set(S_REFRESH);
        break;
    case FSHE_NOSPACE:
        status.set(S_NOSPACE);
        break;
    default:
        break;
    }

}

void folder_share_toc_packed_s::encode(const folder_share_toc_unpacked_s &toc)
{
    bin.clear();
    bin.tappend<ts::uint32>(0); // flags
    bin.tappend<ts::int32>(ts::lendian(static_cast<ts::int32>(toc.elements.size()))); // num

    for (const folder_share_toc_unpacked_s::toc_element_s& e : toc.elements)
    {
        bin.tappend<ts::uint16>(ts::lendian(static_cast<ts::uint16>(e.fn.get_length())));
        bin.append_buf(e.fn.cstr(), e.fn.get_length() * sizeof(ts::wchar));
        bin.tappend<uint64>(ts::lendian(e.sz));
        bin.tappend<uint64>(ts::lendian(e.modtime));
    }

    ts::blob_c ctoc;
    ctoc.set_size(bin.size() * 3 / 2);

    uLong csz = static_cast<uLong>(ctoc.size()) - sizeof(ts::uint32);
    int res = compress2(ctoc.data() + sizeof(ts::uint32), &csz, bin.data(), static_cast<uLong>(bin.size()), Z_BEST_COMPRESSION);
    if (res != Z_OK)
        bin.clear();
    else
    {
        (*(ts::uint32 *)ctoc.data()) = ts::lendian(static_cast<ts::uint32>(bin.size()));
        ctoc.set_size(csz + sizeof(ts::uint32));
        bin = ctoc;

        ts::uint8 hash[BLAKE2B_HASH_SIZE_SMALL];
        BLAKE2B(hash, bin.data(), bin.size());
        tochash.append_as_hex(hash, sizeof(hash));
    }
}

void folder_share_toc_unpacked_s::decode(const folder_share_toc_packed_s &toc)
{
    elements.clear();

    if (toc.bin.size() <= sizeof(ts::uint32))
        return;

    uLong unc_size = static_cast<uLong>(ts::lendian(*(ts::uint32 *)toc.bin.data()));
    ts::buf0_c bunc(unc_size, true);

    int e = uncompress(bunc.data(), &unc_size, toc.bin.data()+sizeof(ts::uint32), static_cast<uLong>(toc.bin.size() - sizeof(ts::uint32)));
    if (e != Z_OK)
        return;

    ipcr r( bunc.data(), static_cast<int>(bunc.size()), 0 );
    ts::uint32 flags = r.get<ts::uint32>();
    int numels = ts::lendian(r.get<ts::int32>());
    elements.reserve(numels);
    for (int i = 0; i < numels; ++i)
    {
        toc_element_s &el = elements.add();
        el.fn = r.getwstr();
        el.sz = ts::lendian(r.get<uint64>());
        el.modtime = ts::lendian(r.get<uint64>());
    }
}

void folder_share_toc_unpacked_s::merge(const folder_share_toc_unpacked_s &from)
{
    for (toc_element_s &e : elements)
        e.status.set(toc_element_s::S_DELETED);

    for (const toc_element_s &efrom : from.elements)
    {
        toc_element_s &e = findcreate(efrom);
        e.status.clear(toc_element_s::S_DELETED);
        if (e.sz != efrom.sz)
            e.status.set(toc_element_s::S_SZ_CHANGED);
        if (e.modtime != efrom.modtime)
            e.status.set(toc_element_s::S_TM_CHANGED);
    }
}

folder_share_toc_unpacked_s::toc_element_s &folder_share_toc_unpacked_s::findcreate(const toc_element_s &el)
{
    ts::aint index;
    if (elements.find_sorted(index, el))
        return elements.get(index);

    for (ts::aint i=index; i > 0; --i)
    {
        toc_element_s &e = elements.get(i);
        if (!e.fn.equals(el.fn))
            break;
        if (e.is_dir() == el.is_dir())
            return e;
    }

    for (ts::aint i = index + 1, n = elements.size(); i < n; ++i)
    {
        toc_element_s &e = elements.get(i);
        if (!e.fn.equals(el.fn))
            break;
        if (e.is_dir() == el.is_dir())
            return e;
    }

    toc_element_s &e = elements.insert(index);
    e = el;
    e.status.set(toc_element_s::S_NEW);
    return e;
}

folder_share_c *folder_share_c::build(folder_share_s::fstype_e t, contact_key_s k, const ts::str_c &name, uint64 utag)
{
    if (t == folder_share_s::FST_RECV)
        return TSNEW(folder_share_recv_c, k, name, utag);

    return TSNEW(folder_share_send_c, k, name, utag);

}


namespace
{

    struct scan_folder_task_c : public ts::task_c
    {
        contact_key_s hkey;
        ts::wstr_c path;
        folder_share_toc_packed_s toc_pack;
        uint64 utag;

        scan_folder_task_c(const contact_key_s &hkey, ts::wstr_c path, uint64 utag) :hkey(hkey), path(path), utag(utag) {}

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            folder_share_toc_unpacked_s toc;

            scan_dir_t(path, [&](int lv, ts::scan_dir_file_descriptor_c &fd) {

                folder_share_toc_unpacked_s::toc_element_s &el = toc.elements.add();
                if (fd.is_dir())
                    el.fn = fd.path(), el.sz = 0xffffffffffffffffull, el.modtime = 0;
                else
                    el.fn = fd.fullname(), el.sz = fd.size(), el.modtime = fd.modtime();

                el.fn.cut(0, path.get_length());

                return ts::SD_CONTINUE;
            });

            toc.elements.sort();

            toc_pack.encode(toc);

            return R_DONE;
        }
        /*virtual*/ void done(bool canceled) override
        {
            if (toc_pack.bin.size())
            {
                if (folder_share_send_c * sfc = static_cast<folder_share_send_c *>(g_app->find_folder_share_by_utag(utag)))
                    sfc->fin_scan(std::move(toc_pack));
            }

            ts::task_c::done(canceled);
        }

    };

    struct decode_toc_task_c : public ts::task_c
    {
        folder_share_toc_packed_s toc_pack;
        folder_share_toc_unpacked_s toc;
        uint64 utag;
        decode_toc_task_c(const ts::blob_c &toc_bin, const folder_share_toc_unpacked_s &toc, uint64 utag):toc(toc), utag(utag)
        {
            toc_pack.bin = toc_bin;
        }

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            folder_share_toc_unpacked_s temptoc;
            temptoc.decode(toc_pack);
            toc.merge(temptoc);
            return R_DONE;
        }

        /*virtual*/ void done(bool canceled) override
        {
            if (folder_share_recv_c * sfc = static_cast<folder_share_recv_c *>(g_app->find_folder_share_by_utag(utag)))
                sfc->fin_decode(std::move(toc));
            ts::task_c::done(canceled);
        }
    };

    struct recv_worker_task_c : public ts::task_c
    {
        static const int maxnumrecv = 2;

        spinlock::long3264 sync = 0;

        struct recv_s
        {
            ts::wstr_c filedn;
            ts::str_c fakefn;
            folder_share_toc_unpacked_s::toc_element_s *e;
            int xtag;
        } rcv[maxnumrecv];

        folder_share_recv_c *share;
        uint64 utag;
        ts::wstr_c path;
        int apid;
        int tocver = 0;
        int numrecv = 0;
        int xtagpool = 0;

        recv_worker_task_c(folder_share_recv_c *share) :share(share)
        {
            utag = share->get_utag();
            apid = share->get_apid();
            path.setcopy(share->get_path());
        }

        void on_folder_share_die()
        {
            SIMPLELOCK(sync);
            share = nullptr;
        }

        bool recv_waiting_file(int xtag, ts::wstr_c &fnpath)
        {
            SIMPLELOCK(sync);

            for (int i = 0; i < numrecv; ++i)
            {
                recv_s &r = rcv[i];
                if (r.xtag == xtag)
                {
                    fnpath = ts::fn_join(path, r.filedn);
                    return true;
                }
            }
            return false;
        }

        void file_event(int xtag, fshevent_e e)
        {
            ASSERT(spinlock::pthread_self() == g_app->base_tid());

            SIMPLELOCK(sync);
            bool processed = false;
            int tocver_value = -1;
            auto tocw = share->lock_toc(tocver_value);
            if (tocver_value != tocver)
                numrecv = 0, tocver = tocver_value;
            else {
                for (int i = 0; i < numrecv; ++i)
                {
                    recv_s &r = rcv[i];
                    if (r.xtag == xtag)
                    {
                        //DMSG("event: " << xtag << (int)e << ts::to_str(r.e->fn));

                        r.e->fevent(e);

                        if (i < numrecv - 1)
                            rcv[i] = rcv[numrecv - 1];
                        --numrecv;
                        processed = true;
                        break;
                    }
                }
            }
            if (!processed)
            {
                //share->update_notice();
            }
        }

        bool move_to_trash(ts::wstr_c f)
        {
            ts::wstr_c trash = ts::fn_join(path, CONSTWSTR(".trash"));
            ts::make_path(trash, 0);

            ts::wstr_c newfn( ts::fn_join(trash, ts::fn_get_name_with_ext(f)) );
            int nfnl = newfn.get_length();
            for (int n = 1; ts::is_file_exists(newfn); ++n)
                newfn.set_length(nfnl).append_char('.').append_as_int(n,3);
            return ts::ren_or_move_file(f,newfn);
        }

        /*virtual*/ int iterate(ts::task_executor_c *) override
        {
            spinlock::auto_simple_lock slock(sync);
            if (!share) return R_CANCEL;

            int tocver_value = -1;
            auto tocw = share->lock_toc(tocver_value);
            slock.unlock();

            if (tocver_value != tocver)
                numrecv = 0, tocver = tocver_value;

            for (folder_share_toc_unpacked_s::toc_element_s &el : tocw().elements)
            {
                if (el.should_be_downloaded())
                {
                    if (el.is_dir())
                    {
                        el.status.set(folder_share_toc_unpacked_s::toc_element_s::S_DOWNLOADED);
                        ts::make_path( ts::fn_join(path, el.fn), FNO_SIMPLIFY_NOLOWERCASE);
                        return 0;
                    }

                    if (el.sz == 0)
                    {
                        // zero file
                        el.status.set(folder_share_toc_unpacked_s::toc_element_s::S_DOWNLOADED);
                        ts::f_create(ts::fn_join(path, el.fn));
                        el.fevent(FSHE_DOWNLOADED);
                        return 0;
                    }

                    bool cont = false;
                    for (int i = 0; i < numrecv; ++i)
                        if (rcv[i].e == &el)
                        {
                            cont = true;
                            break;
                        }
                    if (cont)
                        continue;

                    if (numrecv == maxnumrecv)
                    {
                        ts::sys_sleep(1);
                        return 0;
                    }

                    ts::wstr_c ffn( ts::fn_join(path, el.fn), CONSTWSTR(TRANSFERING_EXT) );
                    if (ts::is_file_exists(ffn))
                    {
                        if (!ts::kill_file(ffn))
                        {
                            if (!move_to_trash(ffn))
                            {
                                el.status.set(folder_share_toc_unpacked_s::toc_element_s::S_LOCKED);
                                return 0;
                            }
                        }
                    }

                    ffn.set_length(ffn.get_length()-ASTR_LENGTH(TRANSFERING_EXT));
                    if (ts::is_file_exists(ffn))
                    {
                        void *f = ts::f_open(ffn);
                        uint64 t = ts::f_time_last_write(f);
                        uint64 sz = ts::f_size(f);
                        ts::f_close(f);
                        if (sz == el.sz && t >= el.modtime)
                        {
                            el.fevent(FSHE_DOWNLOADED);
                            return 0;
                        }
                        else
                        {
                            if (!move_to_trash(ffn))
                            {
                                el.status.set(folder_share_toc_unpacked_s::toc_element_s::S_LOCKED);
                                return 0;
                            }
                        }
                    }

                    rcv[numrecv].filedn.setcopy(el.fn);
                    rcv[numrecv].e = &el;
                    tocw.unlock();

                    ++xtagpool; if (xtagpool <= 0) xtagpool = 1;

                    rcv[numrecv].fakefn.set(CONSTASTR("?sfdn?")).append_as_num(utag).append_char('?').append_as_int(xtagpool);
                    rcv[numrecv].xtag = xtagpool;

                    if (active_protocol_c *ap = prf().ap(apid))
                    {
                        DMSG("query: " << xtagpool << ts::to_utf8(rcv[numrecv].filedn));
                        ap->query_folder_share_file(utag, ts::to_utf8(rcv[numrecv].filedn), rcv[numrecv].fakefn);
                    }
                    ++numrecv;
                    return 0;
                }
            }

            return 1111;
        }

        /*virtual*/ void done(bool canceled) override
        {
            ASSERT(nullptr == share); // lifetime of worker is always bigger then folder share
            ts::task_c::done(canceled);
        }
    };

}

folder_share_c::folder_share_c(contact_key_s k, const ts::str_c &name, uint64 utag_) :hkey(k), name(name), utag(utag_)
{
    while (utag == 0 || g_app->find_folder_share_by_utag(utag) != nullptr) utag = random64();
}

folder_share_c::pstate_s &folder_share_c::getcreate(int apid)
{
    for (pstate_s &s : apids)
        if (s.apid == (unsigned)apid)
            return s;
    pstate_s &s = apids.add();

    TS_STATIC_CHECK(sizeof(pstate_s) == sizeof(int), "!");
    *(int *)&s = apid; // it also clears other bit fields
    return s;
}

void folder_share_c::update_data()
{
    gmsg<ISOGM_FOLDER_SHARE_UPDATE>(utag).send();

    auto &tbl = prf().get_table_folder_share();
    auto *row = tbl.find<true>([this](const folder_share_s &sh) {
        return sh.utag == utag;
    });

    bool ch = false, chi = false;
    if (row == nullptr)
    {
        ch = true;
        row = &tbl.getcreate(0);
        row->other.utag = utag;
        row->other.historian = hkey;
        row->other.t = is_type(folder_share_s::FST_RECV) ? folder_share_s::FST_RECV : folder_share_s::FST_SEND;
    }

    if (!row->other.name.equals(name))
        row->other.name = name, chi = true;
    if (!row->other.path.equals(path))
        row->other.path = path, chi = true;

    if (chi)
        row->changed();

    if (ch || chi)
        prf().changed();
}

void folder_share_c::send_ctl(folder_share_control_e ctl)
{
    if (contact_root_c *h = contacts().rfind(hkey))
    {
        h->subiterate([&](contact_c *c) {
            if (c->flag_support_folder_share && c->get_state() == CS_ONLINE)
            {
                if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                    ap->send_folder_share_ctl(utag, ctl);
            }
        });
    }
}

folder_share_recv_c::~folder_share_recv_c()
{
    if (worker)
        ((recv_worker_task_c *)worker)->on_folder_share_die();
}

ts::uint32 folder_share_recv_c::gm_handler(gmsg<ISOGM_V_UPDATE_CONTACT> &p)
{
    if (p.contact->getmeta() && p.contact->getmeta()->getkey() == hkey)
    {
        if (p.contact->get_state() != CS_ONLINE && apids.count() == 1 && apids.get(0).apid == p.contact->getkey().protoid && apids.get(0).announced)
        {
            uint64 utag1 = this->utag;
            g_app->remove_folder_share(this);
            gmsg<ISOGM_FOLDER_SHARE_UPDATE>(utag1).send();
        }
    }

    return 0;
}

ts::uint32 folder_share_recv_c::gm_handler(gmsg<ISOGM_FOLDER_SHARE> &p)
{
    if (p.pass != 0)
        return 0;

    if (p.utag == utag)
    {
        if (gmsg<ISOGM_FOLDER_SHARE>::DT_TOC == p.dt)
        {
#ifdef _DEBUG
            pstate_s &s = getcreate(p.apid);
            ASSERT(s.accepted);
#endif // _DEBUG

            g_app->add_task(TSNEW(decode_toc_task_c, p.toc, toc.lock_read()(), utag));
            run_worker();
        }
        else if (gmsg<ISOGM_FOLDER_SHARE>::DT_CTL == p.dt)
        {
            if (p.ctl == FSC_REJECT)
            {
                g_app->remove_folder_share(this);
                return 0;
            }
        }

    }

    return 0;
}

bool folder_share_recv_c::recv_waiting_file(int xtag, ts::wstr_c &fnpath)
{
    return worker && ((recv_worker_task_c *)worker)->recv_waiting_file(xtag, fnpath);
}

void folder_share_recv_c::accept()
{
    ts::wstr_c path_ = ts::fn_join(prf().download_folder_fshare(), ts::from_utf8(name));
    accept(path_);
}
void folder_share_recv_c::accept(ts::wstr_c&path_)
{
    ASSERT(apids.count() == 1); // recv always has 1 apid
    folder_share_c::pstate_s &s = apids.get(0);
    s.accepted = 1;
    s.announced = 0;

    send_ctl(FSC_ACCEPT);

    path_expand_env(path_, contacts().rfind(hkey));

    ts::make_path(path_, 0);
    set_path(path_, false);
    set_state(folder_share_c::FSS_WORKING, false);
}

void folder_share_recv_c::fshevent(int xtag, fshevent_e x)
{
    if (ASSERT(worker))
    {
        ((recv_worker_task_c *)worker)->file_event(xtag, x);
        update_data();
    }
}

void folder_share_recv_c::run_worker()
{
    if (!worker)
    {
        recv_worker_task_c *task = TSNEW(recv_worker_task_c, this);
        worker = task;
        g_app->add_task(task);
    }
}

int folder_share_recv_c::get_apid() const
{
    if (ASSERT(apids.count() == 1))
        return apids.get(0).apid;
    return 0;
}

spinlock::syncvar<folder_share_toc_unpacked_s>::WRITER folder_share_recv_c::lock_toc(int &tocver_value)
{
    auto tocw = toc.lock_write();
    tocver_value = tocver;
    return tocw;
}

void folder_share_recv_c::fin_decode(folder_share_toc_unpacked_s &&toc2)
{
    auto tocw = toc.lock_write();
    tocw() = std::move(toc2);
    tocver = g_app->get_free_tag();
}

ts::uint32 folder_share_send_c::gm_handler(gmsg<ISOGM_FOLDER_SHARE> &p)
{
    if (p.pass != 0)
        return 0;

    if (p.utag == utag)
    {
        if (gmsg<ISOGM_FOLDER_SHARE>::DT_CTL == p.dt)
        {
            if (FSC_REJECT == p.ctl)
            {
                for (ts::aint i = 0, c = apids.count(); i < c; ++i)
                {
                    if (apids.get(i).apid == (unsigned)p.apid)
                    {
                        apids.remove_fast(i);
                        break;
                    }
                }

                if (apids.count() == 0)
                    set_state(folder_share_c::FSS_REJECT, false);

            }
            else if (FSC_ACCEPT == p.ctl)
            {
                if (st != folder_share_c::FSS_MOVING_FOLDER)
                    set_state(folder_share_c::FSS_WORKING, false);

                pstate_s &s = getcreate(p.apid);
                s.accepted = 1;
                s.announced = 0;
            }
        } else if (gmsg<ISOGM_FOLDER_SHARE>::DT_TOC == p.dt)
        {
            FORBIDDEN();

        } else if (gmsg<ISOGM_FOLDER_SHARE>::DT_QUERY == p.dt)
        {
            ts::wstr_c fpath = ts::fn_join( path, ts::from_utf8(p.tocname) );
            ts::fix_path(fpath, FNO_SIMPLIFY_NOLOWERCASE);
            if (fpath.begins(path) && ts::is_file_exists(fpath))
            {
                if (contact_root_c *h = contacts().rfind(hkey))
                {
                    if (contact_c *c = h->subget_proto(p.apid))
                    {
                        if (file_transfer_s *f = g_app->register_file_hidden_send(hkey, c->getkey(), fpath, p.fakename))
                        {
                            f->folder_share_utag = utag;
                            update_data();
                        }
                    }
                }

            }
        }



        return GMRBIT_CALLAGAIN;
    }

    return 0;
}

/*virtual*/ void folder_share_send_c::set_state(fsstate_e st_, bool updatenotice)
{
    //sfstate_e ost = st;
    folder_share_c::set_state(st_, updatenotice);

    //if (!pause)
    //    sfc.share_folder_anonce(r);
    //else
    //    sfc.share_folder_cancel(r);

}

/*virtual*/ void folder_share_send_c::set_path(const ts::wstr_c &path_, bool updatenotice)
{
    if (!path.equals(path_))
    {
        if (spyid)
            ts::master().folder_spy_stop(spyid);

        path = path_;
        ts::fix_path(path, FNO_SIMPLIFY_NOLOWERCASE | FNO_APPENDSLASH);

        spyid = ts::master().folder_spy(path, this);

        if (contact_root_c *r = contacts().rfind(hkey))
            share_folder_announce(r);
    }

    if (updatenotice)
        update_data();
}

/*virtual*/ bool folder_share_send_c::tick5()
{
    if (contact_root_c *r = contacts().rfind(hkey))
        if (!share_folder_announce(r))
            return false;

    update_data();

    return true;
}

/*virtual*/ void folder_share_send_c::refresh()
{
    next_toc_refresh = ts::Time::current() - 1;
    if (contact_root_c *r = contacts().rfind(hkey))
        share_folder_announce(r);
}

/*virtual*/ void folder_share_send_c::change_event(ts::uint32 spyid_)
{
    refresh_after_scan = scaning;
    if (!scaning && ASSERT(spyid == spyid_))
        refresh();
}

void folder_share_send_c::again()
{
    set_state(folder_share_c::FSS_WORKING, false);
    if (contact_root_c *r = contacts().rfind(hkey))
        share_folder_announce(r);
}

void folder_share_send_c::fin_scan(folder_share_toc_packed_s &&toc_pack)
{
    scaning = false;
    if (!toc.tochash.equals(toc_pack.tochash))
    {
        toc = std::move(toc_pack);
        ++tocver;

        if (contact_root_c *r = contacts().rfind(hkey))
            if (!send_toc(r))
                g_app->remove_folder_share(this);
    }
    else
        update_data();

    if (refresh_after_scan)
    {
        refresh_after_scan = false;
        refresh();
    }
}

bool folder_share_send_c::send_toc(contact_root_c *h)
{
    for (pstate_s &s : apids) s.present = 0;

    bool some_support = false;
    bool some_not_yet_initialized = false;
    h->subiterate([&](contact_c *c) {

        if (!c->flag_caps_received)
        {
            some_not_yet_initialized = true;
            return;
        }

        some_support |= c->flag_support_folder_share;
        pstate_s &st = getcreate(c->getkey().protoid);
        st.present = 1;

        if (c->flag_support_folder_share)
        {
            if (c->get_state() == CS_ONLINE)
            {
                if (active_protocol_c *ap = prf().ap(c->getkey().protoid))
                {
                    if (!st.accepted)
                        st.announced = 1;
                    ap->send_folder_share_toc(c->getkey().gidcid(), tocver, *this);
                }
            }
            else
            {
                st.accepted = 0;
                st.announced = 0;
            }
        }
    });

    if (some_not_yet_initialized)
        return true;

    for (ts::aint i = 0, c = apids.count(); i < c; ++i)
    {
        if (!apids.get(i).present)
        {
            apids.remove_fast(i);
            break;
        }
    }

    update_data();

    return some_support /*|| !h->get_share_folder_path().is_empty()*/; //FOLDER_SHARE_PATH
}

bool folder_share_send_c::share_folder_announce(contact_root_c *h)
{
    ts::wstr_c p(path);
    //if (p.is_empty()) p = h->get_share_folder_path(); //FOLDER_SHARE_PATH
    if (p.is_empty()) return false;
    path_expand_env(p, h);
    if (!ts::is_dir_exists(p))
        return true; // no dir - just do nothing: may dir will appear in future

    if (toc.bin.size() == 0 || (ts::Time::current() - next_toc_refresh) > 0)
    {
        if (!scaning)
        {
            next_toc_refresh = ts::Time::current() + 60000 * 3; // in 3 min
            g_app->add_task(TSNEW(scan_folder_task_c, h->getkey(), p, utag));
            scaning = true;
            update_data();
        }
        return true;
    }

    return send_toc(h);

}

/*virtual*/ void folder_share_recv_c::refresh()
{
    auto tocw = toc.lock_write();

    for (folder_share_toc_unpacked_s::toc_element_s &el : tocw().elements)
    {
        el.status.clear(
            folder_share_toc_unpacked_s::toc_element_s::S_NOSPACE |
            folder_share_toc_unpacked_s::toc_element_s::S_LOCKED |
            folder_share_toc_unpacked_s::toc_element_s::S_DOWNLOADED);
        el.status.set(folder_share_toc_unpacked_s::toc_element_s::S_REFRESH);
    }

}

/*virtual*/ void folder_share_recv_c::set_path(const ts::wstr_c &path_, bool updatenotice)
{
    if (path.is_empty())
    {
        path = path_;

    }
    else if (!path.equals(path_))
    {
        set_state(FSS_MOVING_FOLDER, false);
        int move_folder;

        path = path_;
    }

    ts::fix_path(path, FNO_SIMPLIFY_NOLOWERCASE | FNO_APPENDSLASH);

    if (updatenotice)
        update_data();
}



