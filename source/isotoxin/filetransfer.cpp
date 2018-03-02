#include "isotoxin.h"

file_transfer_s::file_transfer_s()
{
    auto d = data.lock_write();

}

file_transfer_s::~file_transfer_s()
{
    dip = true;

    if (g_app)
    {
        // wait for unlock
        for (; data.lock_write()().lock > 0; ts::sys_sleep(1))
            g_app->m_tasks_executor.tick();
    }

    if (void *handle = file_handle())
        ts::f_close(handle);
}

bool file_transfer_s::confirm_required() const
{
    if (prf().fileconfirm() == 0)
    {
        // required, except...
        return !file_mask_match(filename, prf().auto_confirm_masks());
    }
    else if (prf().fileconfirm() == 1)
    {
        // not required, except...
        return file_mask_match(filename, prf().manual_confirm_masks());
    }
    return true;
}

bool file_transfer_s::auto_confirm()
{
    if (folder_share_utag == 0)
    {
        bool image = image_loader_c::is_image_fn(to_utf8(filename));
        ts::wstr_c downf = image ? prf().download_folder_images() : prf().download_folder();
        path_expand_env(downf, contacts().rfind(historian));
        ts::make_path(downf, 0);
        if (!ts::is_dir_exists(downf))
            return false;
        uint64 freesz = ts::get_free_space(downf);
        if (freesz < filesize)
            return false;

        prepare_fn(ts::fn_join(downf, filename), false);
        if (image)
            blink_on_recv_done = true;

    }
    else
    {
        ts::wstr_c p = ts::fn_get_path(filename_on_disk);
        ts::make_path(p, 0);

        uint64 freesz = ts::get_free_space(p);
        if (freesz < filesize)
        {
            if (folder_share_c *sh = g_app->find_folder_share_by_utag(folder_share_utag))
                if (sh->is_type(folder_share_s::FST_RECV))
                    static_cast<folder_share_recv_c *>(sh)->fshevent(folder_share_xtag, FSHE_NOSPACE);

            return false;
        }

        prepare_fn(filename_on_disk, false);
    }


    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_accept(i_utag, 0);

    return true;
}


void file_transfer_s::prepare_fn(const ts::wstr_c &path_with_fn, bool overwrite)
{
    accepted = true;
    filename_on_disk = path_with_fn;
    if (!overwrite)
    {
        ts::wstr_c origname = ts::fn_get_name(filename_on_disk);
        int n = 1;
        while (ts::is_file_exists(filename_on_disk) || ts::is_file_exists(filename_on_disk + CONSTWSTR(TRANSFERING_EXT)))
            filename_on_disk = ts::fn_change_name(filename_on_disk, ts::wstr_c(origname).append_char('(').append_as_int(n++).append_char(')'));
    }
    filename_on_disk.append(CONSTWSTR(TRANSFERING_EXT));
    data.lock_write()().deltatime(true);

    if (folder_share_utag == 0)
    {
        upd_message_item(true);

        if (auto *row = prf().get_table_unfinished_file_transfer().find<true>([&](const unfinished_file_transfer_s &uftr)->bool { return uftr.utag == utag; }))
        {
            row->other.msgitem_utag = msgitem_utag;
            row->other.filename_on_disk = filename_on_disk;
            row->changed();
            prf().changed();
        }
    }
}

int file_transfer_s::progress(int &bps, uint64 &cursz) const
{
    auto rdata = data.lock_read();
    cursz = rdata().progrez;
    bps = rdata().bytes_per_sec;
    int prc = (int)(cursz * 100 / filesize);
    if (prc > 100) prc = 100;
    return prc;
}

void file_transfer_s::pause_by_remote(bool p)
{
    if (p)
    {
        data.lock_write()().bytes_per_sec = BPSSV_PAUSED_BY_REMOTE;
        upd_message_item(true);
    }
    else
    {
        auto wdata = data.lock_write();
        wdata().deltatime(true);
        wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
    }
}

void file_transfer_s::pause_by_me(bool p)
{
    active_protocol_c *ap = prf().ap(sender.protoid);
    if (!ap) return;

    if (p)
    {
        ap->file_control(i_utag, FIC_PAUSE);
        data.lock_write()().bytes_per_sec = BPSSV_PAUSED_BY_ME;
        upd_message_item(true);
    }
    else
    {
        ap->file_control(i_utag, FIC_UNPAUSE);
        auto wdata = data.lock_write();
        wdata().deltatime(true);
        wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
    }
}

void file_transfer_s::kill(file_control_e fctl, unfinished_file_transfer_s *uft)
{
    //DMSG("kill " << utag << fctl << filename_on_disk);

    folder_share_c *share = nullptr;
    if (folder_share_utag != 0 && (FIC_DISCONNECT == fctl || FIC_STUCK == fctl))
    {
        if (folder_share_c *sh = g_app->find_folder_share_by_utag(folder_share_utag))
            if (sh->is_type(folder_share_s::FST_RECV))
            {
                static_cast<folder_share_recv_c *>(sh)->fshevent(folder_share_xtag, FSHE_FAILED);
                share = sh;
            }
    }


    /*
    if (FIC_UNKNOWN == fctl)
    {
        if (uft)
            *uft = *this;

        g_app->unregister_file_transfer(utag, false);
        return;
    }
    */

    if (!upload && !accepted && (fctl == FIC_NONE || fctl == FIC_DISCONNECT))
    {
        // kill without accept - just do nothing
        g_app->new_blink_reason(historian).file_download_remove(utag);

        if (uft)
            *uft = *this;

        g_app->unregister_file_transfer(utag, false);
        return;
    }

    if (fctl != FIC_NONE)
    {
        if (active_protocol_c *ap = prf().ap(sender.protoid))
            ap->file_control(i_utag, fctl);
    }

    auto update_history = [this]()
    {
        post_s p;
        p.sender = sender;
        p.message_utf8 = ts::refstring_t<char>::build(to_utf8(filename_on_disk), g_app->global_allocator);
        p.utag = msgitem_utag;
        prf().change_history_item(historian, p, HITM_MESSAGE);
        if (contact_root_c * h = contacts().rfind(historian)) h->iterate_history([this](post_s &p)->bool {
            if (p.utag == msgitem_utag)
            {
                p.message_utf8 = ts::refstring_t<char>::build(to_utf8(filename_on_disk), g_app->global_allocator);
                return true;
            }
            return false;
        });
    };

    void *handle = file_handle();
    if (handle && (!upload || fctl != FIC_DONE)) // close before update message item
    {
        uint64 fsz = 0;
        if (!upload)
        {
            fsz = ts::f_size(handle);
            if (fctl == FIC_DONE && fsz != filesize)
                return;
        }
        ts::f_close(handle);
        data.lock_write()().handle = nullptr;
        if (filename_on_disk.ends(CONSTWSTR(TRANSFERING_EXT)))
            filename_on_disk.trunc_length(5);
        if (fsz != filesize || upload)
        {
            filename_on_disk.insert(0, fctl != FIC_DISCONNECT ? '*' : '?');
            
            if (folder_share_utag == 0)
                update_history();

            if (!upload && fctl != FIC_DISCONNECT)
                ts::kill_file(ts::wstr_c(filename_on_disk.as_sptr().skip(1), CONSTWSTR(TRANSFERING_EXT)));

            if (!upload && fctl == FIC_DISCONNECT && folder_share_utag != 0)
            {
                if (folder_share_c *sh = g_app->find_folder_share_by_utag(folder_share_utag))
                    if (sh->is_type(folder_share_s::FST_RECV))
                    {
                        static_cast<folder_share_recv_c *>(sh)->fshevent(folder_share_xtag, FSHE_FAILED);
                        share = sh;
                    }
            }
        }
        else if (!upload && fctl == FIC_DONE)
        {
            ts::ren_or_move_file(filename_on_disk + CONSTWSTR(TRANSFERING_EXT), filename_on_disk);
            if (folder_share_utag == 0)
                play_sound(snd_file_received, false);
        }
    }
    data.lock_write()().deltatime(true, -60);
    if (fctl != FIC_REJECT) upd_message_item(true);
    if (fctl == FIC_DONE)
    {
        done_transfer = true;
        if (folder_share_utag != 0)
        {
            if (folder_share_c *sh = g_app->find_folder_share_by_utag(folder_share_utag))
                if (sh->is_type(folder_share_s::FST_RECV))
                {
                    static_cast<folder_share_recv_c *>(sh)->fshevent(folder_share_xtag, FSHE_DOWNLOADED);
                    share = sh;
                }
        } else
        {
            update_history();
            upd_message_item(true);
            if (blink_on_recv_done)
                g_app->new_blink_reason(historian).up_unread();

        }
    }
    else if (fctl == FIC_DISCONNECT)
    {
        bool upd = false;
        if (filename_on_disk.get_char(0) != '?')
            filename_on_disk.insert(0, '?'), upd = true;

        if (upd)
            update_history();
    }
    if (uft)
        *uft = *this;
    g_app->unregister_file_transfer(utag, fctl == FIC_DISCONNECT);

    if (share)
        share->update_data();
}


/*virtual*/ int file_transfer_s::iterate(ts::task_executor_c *)
{
    if (dip)
        return R_CANCEL;

    auto rr = data.lock_read();

    ASSERT(rr().lock > 0);

    if (rr().query_job.size() == 0) // job queue is empty - just do nothing
    {
        ++queueemptycounter;
        if (queueemptycounter > 10)
            return 10;
        return 0;
    }
    queueemptycounter = 0;

    job_s cj = rr().query_job.get(0);
    rr.unlock();

    auto xx = data.lock_write();
    void *handler = xx().handle;
    xx.unlock();

    bool rslt = false;

    if (handler)
    {
        // always set file pointer
        ts::f_set_pos(handler, cj.offset);

        ts::aint sz = (ts::aint)ts::tmin<int64, int64>(cj.sz, (int64)(filesize - cj.offset));
        ts::tmp_buf_c b(sz, true);

        if (sz != ts::f_read(handler, b.data(), sz))
        {
            read_fail = true;
            return R_CANCEL;
        }

        if (active_protocol_c *ap = prf().ap(sender.protoid))
        {
            while (!ap->file_portion(i_utag, cj.offset, b.data(), sz))
            {
                ts::sys_sleep(100);

                if (dip)
                    return R_CANCEL;
            }
        }

        if (dip)
            return R_CANCEL;

        xx = data.lock_write();

        if (xx().bytes_per_sec >= file_transfer_s::BPSSV_ALLOW_CALC)
        {
            xx().transfered_last_tick += cj.sz;
            xx().upduitime += xx().deltatime(true);

            if (xx().upduitime > 0.3f)
            {
                xx().upduitime -= 0.3f;

                ts::Time curt = ts::Time::current();
                int delta = curt - xx().speedcalc;

                if (delta >= 500)
                {
                    xx().bytes_per_sec = (int)((uint64)xx().transfered_last_tick * 1000 / delta);

                    xx().speedcalc = curt;
                    xx().transfered_last_tick = 0;
                }

                update_item = true;
                rslt = true;
            }
        }

        xx().progrez = cj.offset;
        xx().query_job.remove_slow(0);
        xx.unlock();


    }

    if (rslt && !dip)
        return R_RESULT;

    return dip ? R_CANCEL : 0;

}
/*virtual*/ void file_transfer_s::done(bool canceled)
{
    if (canceled || dip)
    {
        --data.lock_write()().lock;
        return;
    }

    if (read_fail)
    {
        --data.lock_write()().lock;
        kill();
        return;
    }

    upd_message_item(false);
}

/*virtual*/ void file_transfer_s::result()
{
    upd_message_item(false);
}

void file_transfer_s::query(uint64 offset_, int sz)
{
    auto ww = data.lock_write();
    ww().query_job.addnew(offset_, sz);

    if (ww().lock > 0)
        return;

    if (file_handle())
    {
        ++ww().lock;
        ww.unlock();
        g_app->add_task(this);
    }
}

void file_transfer_s::upload_accepted()
{
    auto wdata = data.lock_write();
    ASSERT(wdata().bytes_per_sec == BPSSV_WAIT_FOR_ACCEPT || wdata().bytes_per_sec == BPSSV_PAUSED_BY_REMOTE);
    wdata().bytes_per_sec = BPSSV_ALLOW_CALC;
}

void file_transfer_s::resume()
{
    ASSERT(file_handle() == nullptr);

    void * h = f_continue(filename_on_disk);
    if (!h)
    {
        kill();
        return;
    }
    auto wdata = data.lock_write();
    wdata().handle = h;

    uint64 fsz = ts::f_size(wdata().handle);
    uint64 offset = fsz > 1024 ? fsz - 1024 : 0;
    wdata().progrez = offset;
    fsz = offset;
    ts::f_set_pos(wdata().handle, fsz);

    accepted = true;

    if (active_protocol_c *ap = prf().ap(sender.protoid))
        ap->file_accept(i_utag, offset);

}

void file_transfer_s::save(uint64 offset_, ts::buf0_c&bdata)
{
    if (!accepted) return;

    if (file_handle() == nullptr)
    {
        if (folder_share_utag == 0)
            play_sound(snd_start_recv_file, false);

        void *h = ts::f_recreate(filename_on_disk);
        if (!h)
        {
            kill();
            return;
        }
        data.lock_write()().handle = h;
        
        if (folder_share_utag == 0)
            if (contact_c *c = contacts().find(historian))
                c->redraw();
    }
    if (offset_ + bdata.size() > filesize)
    {
        kill();
        return;
    }

    if (write_buffer.size() == 0)
        write_buffer = std::move(bdata), write_buffer_offset = offset_;
    else
    {
        if (offset_ != (write_buffer_offset + write_buffer.size()))
        {
            kill();
            return;
        }
        write_buffer.append_buf(bdata);
    }


    auto wdata = data.lock_write();

    ts::f_set_pos(wdata().handle, write_buffer_offset);

    ts::aint wrslt = ts::f_write(wdata().handle, write_buffer.data(), write_buffer.size());
    if (wrslt == ts::FE_WRITE_NO_SPACE)
    {
        pause_by_me(true);
        return;
    }
    if (wrslt != write_buffer.size())
    {
        kill();
        return;
    }

    wdata().progrez += write_buffer.size();

    if (write_buffer.size())
    {
        if (wdata().bytes_per_sec >= BPSSV_ALLOW_CALC)
        {
            wdata().transfered_last_tick += write_buffer.size();
            wdata().upduitime += wdata().deltatime(true);
            if (wdata().bytes_per_sec == 0)
                wdata().bytes_per_sec = 1;

            if (wdata().upduitime > 0.3f)
            {
                wdata().upduitime -= 0.3f;

                ts::Time curt = ts::Time::current();
                int delta = curt - wdata().speedcalc;

                if (delta >= 500)
                {
                    wdata().bytes_per_sec = (int)((uint64)wdata().transfered_last_tick * 1000 / delta);
                    wdata().speedcalc = curt;
                    wdata().transfered_last_tick = 0;
                }

                wdata.unlock();
                upd_message_item(true);
            }
        }
    }

    write_buffer.clear();

}

void file_transfer_s::upd_message_item(unfinished_file_transfer_s &uft)
{
    post_s p;
    p.type = uft.upload ? MTA_SEND_FILE : MTA_RECV_FILE;

    ts::str_c m = to_utf8(uft.filename_on_disk);
    if (m.ends(CONSTASTR(TRANSFERING_EXT)))
        m.trunc_length(5);

    p.message_utf8 = ts::refstring_t<char>::build(m, g_app->global_allocator);

    p.utag = uft.msgitem_utag;
    p.sender = uft.sender;
    p.receiver = contacts().get_self().getkey();
    p.recv_time = ts::now();
    gmsg<ISOGM_SUMMON_POST>(p, true).send();

    if (!uft.upload)
        g_app->new_blink_reason(uft.historian).file_download_progress_add(uft.utag);

}

void file_transfer_s::upd_message_item(bool force)
{
    if (folder_share_utag != 0)
    {
        return;
    }

    if (!force && !update_item) return;
    update_item = false;

    if (msgitem_utag)
    {
        upd_message_item(*this);

    }
    else if (contact_c *c = contacts().find(sender))
    {
        gmsg<ISOGM_MESSAGE> msg(c, &contacts().get_self(), upload ? MTA_SEND_FILE : MTA_RECV_FILE, 0);
        msg.post.cr_time = ts::now();

        ts::str_c m = to_utf8(filename_on_disk);
        if (m.ends(CONSTASTR(TRANSFERING_EXT)))
            m.trunc_length(5);

        msg.post.message_utf8 = ts::refstring_t<char>::build(m, g_app->global_allocator);

        msgitem_utag = msg.post.utag;
        msg.send();
    }
}

