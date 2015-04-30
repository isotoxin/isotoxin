/*
    IPC module
    (C) 2015 ROTKAERMOTA (TOX: ED783DA52E91A422A48F984CAC10B6FF62EE928BE616CE41D0715897D7B7F6050B2F04AA02A1)
*/
#pragma once

/*
  WM_COPYDATA and shared-in-memory-file based Inter-Process Communication library
  only point-to-point connection supported
*/

namespace ipc
{
    enum ipc_result_e
    {
        IPCR_OK,
        IPCR_BREAK, // return it only when using processor_func via ipc_junction_s::wait
    };

    typedef ipc_result_e processor_func( void *par, void *data, int datasize ); // callback
    typedef ipc_result_e idlejob_func( void *par ); // callback

    struct ipc_junction_s
    {
        char buffer[63]; // internal data. ipc_junction_s must be allocated at your application. good news: no any new/malloc/delete/free memory routines called inside lib engine
        bool stop_called;

        ipc_junction_s():stop_called(true) {} // constructor do nothing: all initialization stuff in this->start
        ~ipc_junction_s() { if (!stop_called) __debugbreak(); } // destructor do nothing: all finalization stuff in this->stop

        int start(const char *junction_name); // application should call this, to connect to ipc junction. junction_name is unique per system and only two apps (or one app twice) can use one junction_name
        void stop(); // don't forget to execute stop at end of all

        void wait( processor_func *df, void *par_data, idlejob_func *ij, void *par_ij ); // set callback and wait. processor_func should return IPCR_BREAK to stop waiting; it is possible to call idlejob from another thread
        void idlejob(); // initiate idlejob call - usable to call from another thread

        void set_data_callback( processor_func *f, void *par ); // use it, if your app already run windows message loop at current thread (WINDOWS only)

        bool send( const void *data, int datasize ); // send data to partner

        bool wait_partner(int timeout_ms); // returns false if timeout
    };

} // namespace ipc

