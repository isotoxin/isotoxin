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
        Print("Fail 1\n");
        return;
    }

    ts::process_handle_s ph;
    if (!ts::master().start_app( exe, ts::wstrings_c(CONSTWSTR("utp"), CONSTWSTR("test_0")), &ph, false))
    {
        Print("Fail 2\n");
        junct.stop();
        return;
    }

    junct.set_data_callback(processor_func, nullptr);
    if (!junct.wait_partner(10000))
    {
        Print("Fail 3\n");
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



void expe()
{
    /*
    int fifo1 = mkfifo("/tmp/abcdxxx", O_RDONLY);
    unlink("/tmp/abcdxxx");
    int fifo2 = mkfifo("/tmp/abcdxxx", O_RDONLY);

    int er1 = EEXIST;
    __debugbreak();
    */
}

struct threaddata_s
{
    HANDLE evt1 = nullptr;
    HANDLE evt2 = nullptr;
    HANDLE evt3 = nullptr;
    HANDLE evt4 = nullptr;
    int ev1c = 0;
    int ev2c = 0;
    int ev3c = 0;
    int ev4c = 0;

    bool stop = false;

    ~threaddata_s()
    {
        CloseHandle(evt1);
        CloseHandle(evt2);
        CloseHandle(evt3);
        CloseHandle(evt4);
    }

    static DWORD __stdcall th1(void *p)
    {
        threaddata_s *d = (threaddata_s *)p;

        WaitForSingleObject(d->evt1, INFINITE);
        ++d->ev1c;
        WaitForSingleObject(d->evt1, INFINITE);
        ++d->ev1c;
        WaitForSingleObject(d->evt1, INFINITE);
        ++d->ev1c;
        WaitForSingleObject(d->evt1, INFINITE);
        ++d->ev1c;

        return 0;
    }
    static DWORD __stdcall th2(void *p)
    {
        threaddata_s *d = (threaddata_s *)p;

        for (; !d->stop;)
        {
            WaitForSingleObject(d->evt2, INFINITE);
            ++d->ev2c;
            Sleep(1);
        }
        return 0;
    }
    static DWORD __stdcall th3(void *p)
    {
        threaddata_s *d = (threaddata_s *)p;

        WaitForSingleObject(d->evt3, INFINITE);
        ++d->ev3c;
        WaitForSingleObject(d->evt3, INFINITE);
        ++d->ev3c;
        WaitForSingleObject(d->evt3, INFINITE);
        ++d->ev3c;
        WaitForSingleObject(d->evt3, INFINITE);
        ++d->ev3c;

        return 0;
    }
    static DWORD __stdcall th4(void *p)
    {
        threaddata_s *d = (threaddata_s *)p;

        for (;!d->stop;)
        {
            WaitForSingleObject(d->evt4, INFINITE);
            ++d->ev4c;
            Sleep(1);
        }

        return 0;
    }
};

void logresult(const char *step, bool ok)
{
    Print("Step %s is %s\n", step, ok ? "passed" : "failed");
}

void threadtest()
{
    Print("threadtest begin\n");

    threaddata_s thd;
    thd.evt1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    thd.evt2 = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    thd.evt3 = CreateEvent(nullptr, FALSE, TRUE, nullptr);
    thd.evt4 = CreateEvent(nullptr, TRUE, TRUE, nullptr);

    CloseHandle(CreateThread(nullptr, 0, threaddata_s::th1, &thd, 0, nullptr));
    CloseHandle(CreateThread(nullptr, 0, threaddata_s::th2, &thd, 0, nullptr));

    Sleep(1000);
    logresult("Thread 1.1", thd.ev1c == 0);
    ++thd.ev1c;
    SetEvent(thd.evt1);
    Sleep(1000);
    logresult("Thread 1.2", thd.ev1c == 2);
    SetEvent(thd.evt1);
    Sleep(1000);
    SetEvent(thd.evt1);
    Sleep(1000);
    logresult("Thread 1.3", thd.ev1c == 4);

    ResetEvent(thd.evt2);
    Sleep(1000);
    logresult("Thread 2.1", thd.ev2c == 0);
    SetEvent(thd.evt2);
    Sleep(1000);
    logresult("Thread 2.2", thd.ev2c > 100);
    int nnn = thd.ev2c;
    ResetEvent(thd.evt2);
    Sleep(1000);
    logresult("Thread 2.3", thd.ev2c == nnn);
    SetEvent(thd.evt2);
    thd.stop = true;
    Sleep(1000);



    thd.stop = false;
    CloseHandle(CreateThread(nullptr, 0, threaddata_s::th3, &thd, 0, nullptr));
    CloseHandle(CreateThread(nullptr, 0, threaddata_s::th4, &thd, 0, nullptr));

    Sleep(1000);
    logresult("Thread 3.1", thd.ev3c == 1);
    ++thd.ev3c;
    SetEvent(thd.evt3);
    Sleep(1000);
    logresult("Thread 3.2", thd.ev3c == 3);
    SetEvent(thd.evt3);
    Sleep(1000);
    SetEvent(thd.evt3);
    Sleep(1000);
    logresult("Thread 3.3", thd.ev3c == 5);

    ResetEvent(thd.evt4);
    Sleep(1000);
    int nn = thd.ev4c;
    logresult("Thread 4.1", thd.ev4c > 100);
    Sleep(1000);
    logresult("Thread 4.2", thd.ev4c == nn);
    SetEvent(thd.evt4);
    Sleep(1000);
    logresult("Thread 4.3", thd.ev4c > nn);
    thd.stop = true;


    Print("threadtest end\n");
    Sleep(100000);
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
    case 1:
        expe();
        return 0;
    case 2:
        threadtest();
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