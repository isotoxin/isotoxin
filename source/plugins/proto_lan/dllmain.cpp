#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID /*lpReserved*/
					 )
{
    g_module = hModule;

    switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:

#if defined _DEBUG || defined _CRASH_HANDLER
#include "../appver.inl"
        exception_operator_c::set_unhandled_exception_filter();
        exception_operator_c::dump_filename = fn_change_name_ext(get_exe_full_name(), std::wstr_c(STD_WSTR("proto.lan.")).appendcvt(SS(PLUGINVER)).as_sptr(),
#ifdef MODE64
            STD_WSTR( "x64.dmp" ) );
#else
            STD_WSTR( "dmp" ) );
#endif // MODE64                   
#endif

    case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

