#include "toolset.h"
#include "internal/platform.h"

namespace ts
{

ts::static_setup<MASTER_CLASS, 0> master;

void sys_master_c::app_loop()
{
    if ( on_init && on_init() )
        sys_loop();

    is_shutdown = true;
    mainwindow = nullptr;

    if ( on_exit )
        on_exit();

    shutdown();
}

#ifdef _WIN32
#include "_win32/win32_master.inl"
#endif


}

