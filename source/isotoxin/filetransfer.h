#pragma once

#define TRANSFERING_EXT ".!rcv"

struct file_transfer_s : public unfinished_file_transfer_s, public ts::task_c
{
    static const int BPSSV_WAIT_FOR_ACCEPT = -3;
    static const int BPSSV_PAUSED_BY_REMOTE = -2;
    static const int BPSSV_PAUSED_BY_ME = -1;
    static const int BPSSV_ALLOW_CALC = 0;

    uint64 i_utag = 0; // protocol's internal tag
    uint64 folder_share_utag = 0; // not 0 if folder share transfer
    uint64 write_buffer_offset = 0;
    ts::buf0_c write_buffer;
    int folder_share_xtag = 0;

    struct job_s : public ts::movable_flag<true>
    {
        DUMMY(job_s);
        uint64 offset = 0xFFFFFFFFFFFFFFFFull;
        int sz = 0;
        job_s(uint64 offset, int sz) :offset(offset), sz(sz) {}
        job_s() {}
    };

    /*virtual*/ int iterate(ts::task_executor_c *e) override;
    /*virtual*/ void done(bool canceled) override;
    /*virtual*/ void result() override;

    struct data_s
    {
        //uint64 offset = 0;
        uint64 progrez = 0;
        void * handle = nullptr;
        ts::array_inplace_t<job_s, 1> query_job;
        ts::Time prevt = ts::Time::current();
        ts::Time speedcalc = ts::Time::current();
        ts::aint transfered_last_tick = 0;
        int bytes_per_sec = BPSSV_ALLOW_CALC;
        float upduitime = 0;
        int lock = 0;

        float deltatime(bool updateprevt, int addseconds = 0)
        {
            ts::Time cur = ts::Time::current();
            float dt = (float)((double)(cur - prevt) * (1.0 / 1000.0));
            if (updateprevt)
            {
                prevt = cur;
                if (addseconds)
                    prevt += addseconds * 1000;
            }
            return dt;
        }

    };

    spinlock::syncvar<data_s> data;
    int queueemptycounter = 0;

    void * file_handle() const { return data.lock_read()().handle; }
    //uint64 get_offset() const { return data.lock_read()().offset; }

    bool dip = false;
    bool accepted = false; // prepare_fn called - file receive accepted // used only for receive
    bool update_item = false;
    bool read_fail = false;
    bool done_transfer = false;
    bool blink_on_recv_done = false;

    file_transfer_s();
    ~file_transfer_s();

    bool auto_confirm();

    int progress(int &bytes_per_sec, uint64 &cursz) const;
    void upd_message_item(bool force);
    static void upd_message_item(unfinished_file_transfer_s &uft);

    void upload_accepted();
    void resume();
    void prepare_fn(const ts::wstr_c &path_with_fn, bool overwrite);
    void kill(file_control_e fctl = FIC_BREAK, unfinished_file_transfer_s *uft = nullptr);
    void save(uint64 offset, ts::buf0_c&data);
    void query(uint64 offset, int sz);
    void pause_by_remote(bool p);
    void pause_by_me(bool p);
    bool is_freeze()
    {
        auto wdata = data.lock_write();
        return (const_cast<data_s *>(&wdata())->deltatime(false)) > 60; /* last activity in 60 sec */
    }
    bool confirm_required() const;
};

