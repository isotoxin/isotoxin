#include "stdafx.h"

#pragma USELIB("ipc")
#pragma USELIB("plgcommon")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "Shlwapi.lib")
//#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "comctl32.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

ipc::ipc_junction_s *ipcj = nullptr;

static void __stdcall update_contact(const contact_data_s *);
static void __stdcall message(message_type_e mt, int cid, u64 create_time, const char *msgbody_utf8, int mlen);
static void __stdcall delivered(u64 utag);
static void __stdcall save();
static void __stdcall on_save(const void *data, int dlen, void *param);
static void __stdcall play_audio(int channel_id, const audio_format_s *audio_format, const void *frame, int framesize);
static void __stdcall close_audio(int channel_id);
static void __stdcall proxy_settings(int proxy_type, const char *proxy_address);
static void __stdcall incoming_file(int cid, u64 utag, u64 filesize, const char *filename_utf8, int filenamelen);
static void __stdcall file_portion(u64 utag, u64 offset, const void *portion, int portion_size);
static void __stdcall file_control(u64 utag, file_control_e fctl);

struct protolib_s
{
    host_functions_s hostfunctions;
    HMODULE protolib;
    proto_functions_s *functions;

    cmd_result_e load(const char *protolibname, proto_info_s& pi)
    {
        protolib = LoadLibraryA(protolibname);
        if (protolib)
        {
            get_info_pf gi = (get_info_pf)GetProcAddress(protolib,"get_info");
            if (!gi) return CR_FUNCTION_NOT_FOUND;

            gi(&pi);

            handshake_pf handshake = (handshake_pf)GetProcAddress(protolib, "handshake");
            if (handshake)
            {
                functions = handshake( &hostfunctions );

            } else
            {
                FreeLibrary(protolib);
                protolib = nullptr;
                return CR_FUNCTION_NOT_FOUND;
            }
        
            return CR_OK;
        }
        return CR_MODULE_NOT_FOUND;
    }

    bool loaded() const
    {
        return protolib != nullptr;
    }

    ~protolib_s()
    {
        if (protolib)
        {
            functions->goodbye();
            FreeLibrary(protolib);
        }
    }

} protolib = 
{
    {
        update_contact,
        message,
        delivered,
        save,
        on_save,
        play_audio,
        close_audio,
        proxy_settings,
        incoming_file,
        file_portion,
        file_control,
    },
    nullptr, // protolib
    nullptr, // functions

};

struct prebuf_s
{
    uint64 utag;
    uint64 offset;
    std::vector<byte, ph_allocator> b;
    prebuf_s(uint64 utag):utag(utag), offset(0xffffffffffffffffull)
    {
        b.reserve(65536 + 4096);
        LIST_ADD(this, first, last, prev, next);
    }
    ~prebuf_s()
    {
        LIST_DEL(this, first, last, prev, next);
    }

    static prebuf_s *first;
    static prebuf_s *last;
    static long prebuflock;
    prebuf_s *next;
    prebuf_s *prev;

    static prebuf_s * getbuf( uint64 utag, bool create )
    {
        for (prebuf_s *f = first; f; f = f->next)
        {
            if (f->utag == utag)
                return f;
        }
        if (!create) return nullptr;
        prebuf_s *bb = new prebuf_s(utag);
        return bb;
    }
    static void kill(uint64 utag)
    {
        spinlock::simple_lock(prebuflock);
        for (prebuf_s *f = first; f; f = f->next)
        {
            if (f->utag == utag)
            {
                delete f;
                break;
            }
        }
        spinlock::simple_unlock(prebuflock);
    }
    static void killall()
    {
        spinlock::simple_lock(prebuflock);
        while(first)
            delete first;
        spinlock::simple_unlock(prebuflock);
    }
};

prebuf_s *prebuf_s::first = nullptr;
prebuf_s *prebuf_s::last = nullptr;
long prebuf_s::prebuflock = 0;

struct data_data_s
{
    int sz;
    data_header_s d;
    data_data_s():d(NOP_COMMAND) {} // nobody call this constructor
    
    ipcr get_reader() const
    {
        ASSERT(sz > sizeof(data_header_s));
        return ipcr(&d,sz);
    }

    static data_data_s *build( data_header_s *dd, int szsz )
    {
        if (szsz == sizeof(data_header_s))
        {
            data_data_s *me = (data_data_s*)ph_allocator::ma(sizeof(data_data_s));
            me->sz = 0;
            me->d = *dd;
            return me;
        } else
        {
            data_data_s *me = (data_data_s*)ph_allocator::ma(sizeof(data_data_s) - sizeof(data_header_s) + szsz);
            me->sz = szsz;
            memcpy(&me->d, dd, szsz);
            return me;
        }
    }
    void die()
    {
        ph_allocator::mf(this);
    }
};

struct ipcwbuf_s
{
    struct ipcwspace : public ipcw
    {
        ipcwspace *next = nullptr;
        ipcwspace *prev = nullptr;
    };
    static const int preallocated_bitems = 64;
    ipcwspace data[preallocated_bitems];
    ipcwspace *first = nullptr;
    ipcwspace *last = nullptr;

    ipcwspace *first_a = nullptr;
    ipcwspace *last_a = nullptr;

    ipcwbuf_s()
    {
        for(int i=0;i<preallocated_bitems;++i)
            LIST_ADD( (data+i), first, last, prev, next );
    }
    ~ipcwbuf_s()
    {
        while (first_a)
        {
            ipcwspace *spc = first_a;
            LIST_DEL(spc, first_a, last_a, prev, next);
            spc->~ipcwspace();
            ph_allocator::mf(spc);
        }
    }

    ipcw *get(commands_e cmd)
    {
        if (first)
        {
            if (first_a)
            {
                ipcwspace *spc = first_a;
                LIST_DEL(spc, first_a, last_a, prev, next );
                spc->~ipcwspace();
                ph_allocator::mf(spc);
            }

            ipcwspace *spc = first;
            LIST_DEL(spc, first, last, prev, next );
            spc->clear(cmd);
            return spc;
        }
        if (first_a)
        {
            ipcwspace *spc = first_a;
            LIST_DEL(spc, first_a, last_a, prev, next);
            spc->clear(cmd);
            return spc;
        }
        ipcwspace *spc = (ipcwspace *)ph_allocator::ma(sizeof(ipcwspace));
        spc->ipcwspace::ipcwspace();
        spc->clear(cmd);
        return spc;
    }

    void kill(ipcw *w)
    {
        if (w >= (data+0) && w < (data + preallocated_bitems))
        {
            LIST_ADD((ipcwspace *)w, first, last, prev, next );
        } else
        {
            LIST_ADD((ipcwspace *)w, first_a, last_a, prev, next);
        }
    }
};

spinlock::syncvar<ipcwbuf_s> ipcwbuf;

spinlock::spinlock_queue_s<data_data_s *, ph_allocator> tasks;
spinlock::spinlock_queue_s<ipcw *, ph_allocator> sendbufs;

struct state_s
{
    int working;
    bool need_stop;
    state_s():working(0), need_stop(false) {}
};

spinlock::syncvar<state_s> state;

struct data_block_s
{
    const void *data;
    int datasize;
    data_block_s(const void *data, int datasize):data(data), datasize(datasize) {}
};

struct IPCW
{
    ipcw *w;
    IPCW(commands_e cmd)
    {
        w = ipcwbuf.lock_write()().get(cmd);
    }
    ~IPCW()
    {
        if (w) sendbufs.push(w);
    }

    template<typename T> IPCW & operator<<(const T &v) { if (w) w->add<T>() = v; return *this; }
    template<> IPCW & operator<<(const asptr &s) { if (w) w->add(s); return *this; }
    template<> IPCW & operator<<(const wsptr &s) { if (w) w->add(s); return *this; }
    template<> IPCW & operator<<(const std::vector<char, ph_allocator> &v)
    {
        if (w)
        {
            w->add<int>() = v.size();
            w->add(v.data(), v.size());
        }
        return *this;
    }
    template<> IPCW & operator<<(const data_block_s &d)
    {
        if (w)
        {
            w->add<int>() = d.datasize;
            w->add(d.data, d.datasize);
        }
        return *this;
    }
    template<> IPCW & operator<<(const md5_c &md5)
    {
        if (w) w->add(md5.result(),16);
        return *this;
    }

};

ipc::ipc_result_e event_processor( void *, void *data, int datasize )
{
    if (data == nullptr)
    {
        // lost partner
        return ipc::IPCR_BREAK;
    }

    data_header_s *d = (data_header_s *)data;
    switch (d->cmd)
    {
    case AQ_VERSION:
        IPCW(HA_VERSION) << PLGHOST_IPC_PROTOCOL_VERSION;
        break;

    default:
        tasks.push(data_data_s::build(d, datasize));
        break;
    }
    return ipc::IPCR_OK;
}

unsigned long exec_task( data_data_s *d, unsigned long flags );

DWORD WINAPI worker(LPVOID)
{
    UNSTABLE_CODE_PROLOG

    data_data_s* d = nullptr;
    ipcw *w;

    ++state.lock_write()().working;

    for(;!state.lock_read()().need_stop;Sleep(1))
    {
        if (protolib.loaded()) protolib.functions->tick();

        unsigned long flags = 0;
        while (tasks.try_pop(d))
            flags = exec_task(d, flags);

        while (sendbufs.try_pop(w))
        {
             ipcj->send(w->data(), w->size());
             ipcwbuf.lock_write()().kill(w);
        }

    }

    --state.lock_write()().working;

    UNSTABLE_CODE_EPILOG
    return 0;
}

int CALLBACK WinMainProtect(
    _In_  LPSTR lpCmdLine
    )
{

    int num_workers = 1; // TODO : set to number of processor cores minus 1 for better performance

    for(int i=0;i<num_workers;++i)
    {
        CloseHandle(CreateThread(nullptr, 0, worker, nullptr, 0, nullptr));
    }

    ipc::ipc_junction_s ipcblob;
    ipcj = &ipcblob;
    int member = ipcblob.start(lpCmdLine);

    if (member == 1)
    {
        ipcblob.wait(event_processor, nullptr, nullptr, nullptr);
    }
    state.lock_write()().need_stop = true;
    while (state.lock_read()().working) Sleep(0); // worker must die

    ipcblob.stop();
    ipcj = nullptr;

    data_data_s* d = nullptr;

    while (tasks.try_pop(d))
        d->die();

    ipcw *w;
    while (sendbufs.try_pop(w))
        ipcwbuf.lock_write()().kill(w);

    prebuf_s::killall();

    return 0;
}

int CALLBACK WinMain(
    _In_  HINSTANCE,
    _In_  HINSTANCE,
    _In_  LPSTR lpCmdLine,
    _In_  int
    )
{
#if defined _DEBUG || defined _CRASH_HANDLER
    exception_operator_c::set_unhandled_exception_filter();
#endif

    UNSTABLE_CODE_PROLOG
        WinMainProtect(lpCmdLine);
    UNSTABLE_CODE_EPILOG
        return 0;
}

str_c mypath()
{
    str_c path;
    path.set_length(2048 - 8);
    int len = GetModuleFileNameA(nullptr, path.str(), 2048 - 8);
    path.set_length(len);

    if (path.get_char(0) == '\"')
    {
        int s = path.find_pos(1, '\"');
        path.set_length(s);
    }
    path.trim_right();
    path.trim_right('\"');
    path.trim_left('\"');

    return path;

}

inline bool check_loaded(int cmd, bool f)
{
    if (!f)
    {
        IPCW(HA_CMD_STATUS) << cmd << (int)CR_MODULE_NOT_FOUND;
    }

    return f;
}

#define LIBLOADED() check_loaded(d->d.cmd, protolib.loaded())

unsigned long exec_task(data_data_s *d, unsigned long flags)
{
#define FLAG_SAVE_CONFIG 1
#define FLAG_SAVE_CONFIG_MULTI 2

    switch (d->d.cmd)
    {
    case AQ_GET_PROTOCOLS_LIST:
        {
            IPCW w(HA_PROTOCOLS_LIST);
            auto cnt = w.w->reserve<int>();

            str_c path = mypath(), outstr;
            sstr_c proto_name, description;
            int truncp = path.find_last_pos_of(CONSTASTR("/\\"));
            if (truncp>0)
            {
                path.set_length(truncp+1).append(CONSTASTR("proto.*.dll"));

            
                WIN32_FIND_DATAA find_data;
                HANDLE           fh = FindFirstFileA(path, &find_data);

                while (fh != INVALID_HANDLE_VALUE)
                {
                    path.set_length(truncp+1).append( find_data.cFileName );
                    HMODULE l = LoadLibraryA(path);
                    get_info_pf f = (get_info_pf)GetProcAddress(l,"get_info");
                    proto_info_s info;
                    info.protocol_name = proto_name.str();
                    info.protocol_name_buflen = proto_name.get_capacity();
                    info.description = description.str();
                    info.description_buflen = description.get_capacity();
                    f(&info);

                    outstr.set( asptr(info.protocol_name) ).append(CONSTASTR(": ")).append(asptr(info.description));

                    w << outstr.as_sptr();
                    w << info.proxy_support;
                    ++cnt;

                    FreeLibrary(l);

                    if (!FindNextFileA(fh, &find_data)) break;
                }

                if (fh != INVALID_HANDLE_VALUE) FindClose(fh);
            }
        }
        break;
    case AQ_SET_PROTO:
        {
            ipcr r(d->get_reader());
            tmp_str_c proto = r.getastr();
            tmp_str_c path = mypath();
            str_c desc(1024,true);
            proto_info_s pi;

            int truncp = path.find_last_pos_of(CONSTASTR("/\\"));
            cmd_result_e rst = CR_MODULE_NOT_FOUND;
            if (truncp > 0)
            {
                path.set_length(truncp + 1).append(CONSTASTR("proto.")).append(proto).append(CONSTASTR(".dll"));

                WIN32_FIND_DATAA find_data;
                HANDLE fh = FindFirstFileA(path, &find_data);

                if (fh != INVALID_HANDLE_VALUE)
                {
                    desc.set_length(1024);
                    pi.description = desc.str();
                    pi.description_buflen = 1023;

                    rst = protolib.load(path, pi);

                    FindClose(fh);
                    desc.set_length();
                }
            }
            IPCW(HA_CMD_STATUS) << (int)AQ_SET_PROTO << (int)rst << pi.priority << pi.features << desc.as_sptr()
                << pi.audio_fmt.sample_rate
                << pi.audio_fmt.channels
                << pi.audio_fmt.bits
                << (unsigned short)pi.proxy_support
                << (unsigned short)pi.max_friend_request_bytes
                ;

        }
        break;
    case AQ_SET_NAME:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            tmp_wstr_c name = r.getwstr();
            protolib.functions->set_name(to_utf8(name));
        }
        break;
    case AQ_SET_STATUSMSG:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            tmp_wstr_c status = r.getwstr();
            protolib.functions->set_statusmsg(to_utf8(status));
        }
        break;
    case AQ_SET_CONFIG:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int sz;
            if (const void *d = r.get_data(sz))
                protolib.functions->set_config(d,sz);
        }
        break;
    case AQ_INIT_DONE:
        if (LIBLOADED())
        {
            protolib.functions->init_done();
        }
        break;
    case AQ_ONLINE:
        if (LIBLOADED())
        {
            protolib.functions->online();
        }
        break;
    case AQ_OFFLINE:
        if (LIBLOADED())
        {
            protolib.functions->offline();
        }
        break;
    case AQ_ADD_CONTACT:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int rslt = 0;
            if (r.get<char>())
            {
                // resend
                int cid = r.get<int>();
                tmp_wstr_c invitemsg = r.getwstr();
                rslt = protolib.functions->resend_request(cid, to_utf8(invitemsg));
            } else
            {
                tmp_str_c publicid = r.getastr();
                tmp_wstr_c invitemsg = r.getwstr();
                rslt = protolib.functions->add_contact(publicid, to_utf8(invitemsg));
            }
            IPCW(HA_CMD_STATUS) << (int)AQ_ADD_CONTACT << rslt;
        }
        break;
    case AQ_DEL_CONTACT:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            protolib.functions->del_contact(id);
        }
        break;
    case AQ_MESSAGE:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            int mt = r.get<int>();
            uint64 utag = r.get<uint64>();
            str_c message = to_utf8(r.getwstr());
            message_s m;
            m.mt = (message_type_e)mt;
            m.utag = utag;
            m.message = message.cstr();
            m.message_len = message.get_length();
            protolib.functions->send(id, &m);
        }
        break;
    case AQ_SAVE_CONFIG:
        if (0 != (FLAG_SAVE_CONFIG_MULTI & flags)) break;
        if (0 != (FLAG_SAVE_CONFIG & flags))
        {
            IPCW(HA_CMD_STATUS) << (int)AQ_SAVE_CONFIG << (int)CR_MULTIPLE_CALL;
            flags |= FLAG_SAVE_CONFIG_MULTI;
            break;
        }
        flags |= FLAG_SAVE_CONFIG;
        if (LIBLOADED())
        {
            std::vector<char, ph_allocator> cfg;
            protolib.functions->save_config(&cfg);
            if (cfg.size()) IPCW(HA_CONFIG) << cfg << md5_c(cfg);
            else IPCW(HA_CMD_STATUS) << (int)AQ_SAVE_CONFIG << (int)CR_FUNCTION_NOT_FOUND;
        }
        break;
    case AQ_ACCEPT:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            protolib.functions->accept(id);
        }
        break;
    case AQ_REJECT:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            protolib.functions->reject(id);
        }
        break;
    case AQ_STOP_CALL:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            stop_call_e sc = (stop_call_e)r.get<char>();
            protolib.functions->stop_call(id, sc);
        }
        break;
    case AQ_ACCEPT_CALL:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            protolib.functions->accept_call(id);
        }
        break;
    case AQ_SEND_AUDIO:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            call_info_s cinf;
            cinf.audio_data = r.get_data(cinf.audio_data_size);
            protolib.functions->send_audio(id, &cinf);
        }
        break;
    case AQ_CALL:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int id = r.get<int>();
            call_info_s cinf;
            cinf.duration = r.get<int>();
            protolib.functions->call(id, &cinf);
        }
        break;
    case AQ_PROXY_SETTINGS:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            int proxy_type = r.get<int>();
            str_c proxy_addr = r.getastr();
            protolib.functions->proxy_settings(proxy_type, proxy_addr.cstr());
        }
        break;
    case AQ_FILE_SEND:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            file_send_info_s fi;
            int id = r.get<int>();
            fi.utag = r.get<u64>();
            fi.filename = r.readastr(fi.filename_len);
            fi.filesize = r.get<u64>();
            protolib.functions->file_send(id, &fi);
        }
        break;
    case AQ_CONTROL_FILE:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            u64 utag = r.get<u64>();
            file_control_e fc = (file_control_e)r.get<int>();
            protolib.functions->file_control(utag, fc);
            if (fc == FIC_BREAK || fc == FIC_REJECT || fc == FIC_DONE)
                prebuf_s::kill(utag);
        }
        break;
    case AA_FILE_PORTION:
        if (LIBLOADED())
        {
            ipcr r(d->get_reader());
            u64 utag = r.get<u64>();
            file_portion_s fp;
            fp.offset = r.get<u64>();
            fp.data = r.get_data(fp.size);
            protolib.functions->file_portion(utag, &fp);
        }
        break;
    }

    d->die();

    return flags;
}

static void __stdcall update_contact(const contact_data_s *cd)
{
    IPCW(HQ_UPDATE_CONTACT) << cd->id
      << cd->mask
      << ((0 != (cd->mask & CDM_PUBID)) ? asptr(cd->public_id, cd->public_id_len) : asptr("",0))
      << ((0 != (cd->mask & CDM_NAME)) ? asptr(cd->name, cd->name_len) : asptr("",0))
      << ((0 != (cd->mask & CDM_STATUSMSG)) ? asptr(cd->status_message, cd->status_message_len) : asptr("",0))
      << (int)cd->state
      << (int)cd->ostate
      << (int)cd->gender;
        
}

static void __stdcall message(message_type_e mt, int cid, u64 create_time, const char *msgbody_utf8, int mlen)
{
    static u64 last_createtime = 0;
    static byte lastmessage_md5[16] = { 0 };
    if ((create_time - last_createtime) < 60)
    {
        md5_c md5;
        md5.update(msgbody_utf8, mlen);
        md5.done();
        if (0 == memcmp(lastmessage_md5, md5.result(), 16)) return; // double
        memcpy(lastmessage_md5, md5.result(), 16);
    }
    last_createtime = create_time;
    IPCW(HQ_MESSAGE) << cid << ((int)mt) << create_time << asptr(msgbody_utf8, mlen);
}

static void __stdcall save()
{
    IPCW(HQ_SAVE) << 0;
}

static void __stdcall delivered(u64 utag)
{
    IPCW(HA_DELIVERED) << utag;
}

static void __stdcall on_save(const void *data, int dlen, void *param)
{
    std::vector<char, ph_allocator> *buffer = (std::vector<char, ph_allocator> *)param;
    size_t offset = buffer->size();
    buffer->resize( buffer->size() + dlen );
    memcpy( buffer->data() + offset, data, dlen );
}

static void __stdcall play_audio(int cid, const audio_format_s *audio_format, const void *frame, int framesize)
{
    IPCW(HQ_PLAY_AUDIO) << cid
        << audio_format->sample_rate << audio_format->channels << audio_format->bits
        << data_block_s(frame, framesize);
}

static void __stdcall close_audio(int cid)
{
    IPCW(HQ_CLOSE_AUDIO) << cid;
}

static void __stdcall proxy_settings(int proxy_type, const char *proxy_address)
{
    IPCW(HA_PROXY_SETTINGS) << proxy_type << asptr(proxy_address);
}

static void __stdcall incoming_file(int cid, u64 utag, u64 filesize, const char *filename_utf8, int filenamelen)
{
    if (filesize && filenamelen)
    {
        IPCW(HQ_INCOMING_FILE) << cid << utag << filesize << asptr(filename_utf8, filenamelen);
    } else
    {
        protolib.functions->file_control(utag, FIC_REJECT);
    }
}

static void __stdcall file_control(u64 utag, file_control_e fctl)
{
    switch(fctl)
    {
    case FIC_DONE:
        {
            spinlock::simple_lock(prebuf_s::prebuflock);
            if (prebuf_s *b = prebuf_s::getbuf(utag, false))
            {
                IPCW(HQ_FILE_PORTION) << utag << uint64(b->offset - b->b.size()) << data_block_s(b->b.data(), b->b.size());
                delete b;
            }
            spinlock::simple_unlock(prebuf_s::prebuflock);
        }
        break;
    case FIC_BREAK:
        prebuf_s::kill(utag);
        break;
    }
    IPCW(AQ_CONTROL_FILE) << utag << (int)fctl;
}

static void __stdcall file_portion(u64 utag, u64 offset, const void *portion, int portion_size)
{
    if (portion == nullptr)
    {
        IPCW(HQ_QUERY_FILE_PORTION) << utag << offset << portion_size;
        return;
    }
    spinlock::simple_lock(prebuf_s::prebuflock);
    if (portion_size >= 65536)
    {
        prebuf_s *b = prebuf_s::getbuf(utag, false);
        if (b == nullptr)
        {
            send_now:
            spinlock::simple_unlock(prebuf_s::prebuflock);
            IPCW(HQ_FILE_PORTION) << utag << offset << data_block_s(portion, portion_size);
            return;
        }
    }

    prebuf_s *b = prebuf_s::getbuf(utag, true);
    if (b->offset == 0xffffffffffffffffull)
        b->offset = offset;

    if (b->offset != offset)
    {
        IPCW(HQ_FILE_PORTION) << utag << uint64(b->offset - b->b.size()) << data_block_s(b->b.data(), b->b.size());
        b->b.clear();
        goto send_now;
    }

    auto offs = b->b.size();
    b->b.resize(offs + portion_size);
    memcpy(b->b.data() + offs, portion, portion_size);
    b->offset += portion_size;
    if (b->b.size() >= 65536)
    {
        IPCW(HQ_FILE_PORTION) << utag << uint64(b->offset - b->b.size()) << data_block_s(b->b.data(), b->b.size());
        b->b.clear();
    }
    spinlock::simple_unlock(prebuf_s::prebuflock);
}

