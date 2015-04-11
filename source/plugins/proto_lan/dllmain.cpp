#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE /*hModule*/,
                       DWORD  ul_reason_for_call,
                       LPVOID /*lpReserved*/
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:

#if defined _DEBUG || defined _CRASH_HANDLER
#include "appver.inl"
        exception_operator_c::set_unhandled_exception_filter();
        exception_operator_c::dump_filename = fn_change_name_ext(get_exe_full_name(), wstr_c(CONSTWSTR("proto.lan.")).append(SS(PLUGINVER)).as_sptr(), CONSTWSTR("dmp"));
#endif

    case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

