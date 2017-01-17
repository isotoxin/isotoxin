#include "stdafx.h"
#include "ipc/ipc.h"


static ipc::ipc_junction_s *ipcj = nullptr;

static ipc::ipc_result_e processor_func(void *par, void *data, int datasize)
{
    if (datasize < 0)
    {
        ts::uint8 hash[32];
        crypto_generichash(hash, 32, (const unsigned char *)data, -datasize, nullptr, 0);
        Print("%u, recv block %s (%i)\n", GetCurrentProcessId(), ts::str_c().append_as_hex(hash, 32).cstr(), -datasize);
        ipcj->unlock_buffer(data);
    }
    else
    {
        Print("%u, received back: %s\n", GetCurrentProcessId(), ts::str_c((const char *)data, datasize).cstr());
    }
    return ipc::IPCR_OK;
}

static ipc::ipc_result_e wait_func(void *par)
{
    return ipc::IPCR_OK;
}

#define bsz (1024 * 1024 * 5)


static void send_random_block()
{
    void *b = ipcj->lock_buffer(bsz);
    randombytes_buf(b, bsz);

    ts::uint8 hash[32];
    crypto_generichash(hash, 32, (const unsigned char *)b, bsz, nullptr, 0);

    Print("%u, block %s send\n", GetCurrentProcessId(), ts::str_c().append_as_hex(hash, 32).cstr());
    ipcj->unlock_send_buffer(b, bsz);
}

static void sender()
{
    int n = 0;
    for (;; )
    {
        ts::sys_sleep(1000 + ts::rnd(0, 1000));

        ts::str_c s;
        s.set(CONSTASTR("test string ")).append_as_int(n++);

        ipcj->send( s.cstr(), s.get_length() );

        ts::sys_sleep(1000 + ts::rnd(0, 1000));

        send_random_block();
    }
}


void test_ipc_0( ts::wstr_c exe )
{
    ipc::ipc_junction_s junct;
    ipcj = &junct;

    int memba = junct.start("test_0");
    if (memba != 0)
    {
        Print("Fail 1");
        return;
    }

    ts::process_handle_s ph;
    if (!ts::master().start_app( exe, ts::wstr_c(CONSTWSTR("utp test_0")), &ph, false))
    {
        junct.stop();
        return;
    }

    junct.set_data_callback(processor_func, nullptr);
    if (!junct.wait_partner(10000))
    {
        junct.stop();
        return;
    }

    ts::master().sys_start_thread(sender);

    junct.wait(processor_func, nullptr, wait_func, nullptr);
    junct.stop();
}


ipc::ipc_result_e event_processor(void *dptr, void *data, int datasize)
{
    if (datasize < 0)
    {
        ts::uint8 hash[32];
        crypto_generichash(hash, 32, (const unsigned char *)data, -datasize, nullptr, 0);
        Print("%u, recv block %s (%i)\n", GetCurrentProcessId(), ts::str_c().append_as_hex(hash, 32).cstr(), -datasize);
        ipcj->unlock_buffer(data);

        send_random_block();


    } else
    {
        Print("%u: received: %s\n", GetCurrentProcessId(), ts::str_c((const char *)data, datasize).cstr());
        ipcj->send(data, datasize);
    }


    return ipc::IPCR_OK;
}

void test_ipc_1( const char *n )
{
    ipc::ipc_junction_s ipcblob;
    ipcj = &ipcblob;
    int member = ipcblob.start(n);

    if (member == 1)
    {
        ipcblob.wait(event_processor, nullptr, nullptr, nullptr);
    }

    ipcblob.stop();
}






int proc_ut(const ts::wstrings_c & pars)
{
    if (pars.size() < 2)
        return 0;

    int tt = pars.get(1).as_int();

    switch (tt)
    {
    case 0:
        test_ipc_0( ts::get_exe_full_name() );
        return 0;
    }
    

    return 0;
}

int proc_utp(const ts::wstrings_c & pars)
{
    if (pars.size() < 2)
        return 0;

    test_ipc_1(to_str( pars.get(1) ));

    return 0;
}