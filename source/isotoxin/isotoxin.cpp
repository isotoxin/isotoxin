#include "isotoxin.h"

#pragma USELIB(system)
#pragma USELIB(toolset)
#pragma USELIB(rectangles)
#pragma USELIB(ipc)
#pragma USELIB(s3)
#pragma USELIB(dshowcapture)
#ifndef _FINAL
#pragma USELIB(memspy)
#endif

#pragma comment(lib, "libflac.lib")
#pragma comment(lib, "libvorbis.lib")
#pragma comment(lib, "libogg.lib")

#pragma comment(lib, "filter_audio.lib")
#if _RESAMPLER == RESAMPLER_SRC
#pragma comment(lib, "libresample.lib")
#endif

#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "minizip.lib")
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "curl.lib")
#pragma comment(lib, "libsodium.lib")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")

extern "C"
{
	void* dlmalloc(size_t);
	void  dlfree(void*);
	void* dlrealloc(void*, size_t);
	void* dlcalloc(size_t, size_t);
	size_t dlmalloc_usable_size(void*);
};

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

namespace
{
    static volatile HANDLE popup_event = nullptr;
    static DWORD WINAPI multiinstanceblocker(LPVOID)
    {
        if (!popup_event) return 0;
        HANDLE h = popup_event;
        for(;;)
        {
            WaitForSingleObject(h, INFINITE);
            if (popup_event == nullptr) break;
            gmsg<ISOGM_APPRISE> *m = TSNEW( gmsg<ISOGM_APPRISE> );
            m->send_to_main_thread();
        }
        CloseHandle(h);
        return 0;
    }


	class fileop_c : public ts::tsfileop_c
	{
		ts::tsfileop_c * deffop;
		ts::ccollection_c packs;

	public:
		fileop_c( ts::tsfileop_c ** oldfop  ) 
		{
			deffop = *oldfop;
			*oldfop = nullptr;

			ts::wstr_c wd;
			ts::set_work_dir(wd);

			ts::enum_files( wd, *this, ts::wstr_c(), L"*.data" );
			packs.open_containers();

            /*
            pack in pack not supported
            ts::wstrings_c fns;
            bool zips = false;
            packs.find_by_mask( fns, CONSTWSTR("*.zip"), true );
            for(const ts::wstr_c &fn : fns)
                packs.add_container(fn, 1000), zips = true;

            if (zips) packs.open_containers();
            */

		}

		bool operator()( const ts::wstr_c& base, const ts::wstr_c& fn )
		{
			int prior = 0, i0, i1;
			if (fn.find_inds(0, i0, i1, '.', '.'))
			{
				prior = fn.substr(i0+1, i1).as_int();
			}
			packs.add_container(fn_join(base,fn), prior);
			return true;
		}

		virtual ~fileop_c()
		{
			TSDEL(deffop);
            HANDLE h = popup_event; popup_event = nullptr;
            if (h) SetEvent(h);
		}
		/*virtual*/ bool read( const ts::wsptr &fn, ts::buf_wrapper_s &b ) override
		{
			if (deffop->read(fn,b))
                return true;

			return packs.read(fn,b);
		}
		/*virtual*/ bool size( const ts::wsptr &fn, size_t &sz ) override
		{
            if (deffop->size(fn, sz))
                return true;
            sz = packs.size(fn);
            return sz > 0;
		}
        /*virtual*/ void find(ts::wstrings_c & files, const ts::wsptr &fnmask, bool full_paths) override
        {
            deffop->find(files, fnmask, full_paths);
            packs.find_by_mask( files, fnmask, full_paths );
        }
	};

}

#ifndef _FINAL
void debug_name()
{
    ts::CHARz_add_str<wchar_t>(g_sysconf.name, L" - CRC:");
    ts::buf_c b;
    b.load_from_disk_file(ts::get_exe_full_name());
    int sz;
    wchar_t bx[32];
    wchar_t * t = ts::CHARz_make_str_unsigned<ts::wchar, uint>(bx, sz, b.crc());
    ts::CHARz_add_str<wchar_t>(g_sysconf.name, t);
}
#endif

#pragma warning (disable : 4505) //: 'check_instance' : unreferenced local function has been removed

static bool check_instance()
{
    popup_event = CreateEventW(nullptr, FALSE, FALSE, L"isotoxin_popup_event");
    if (!popup_event) return true;
    if (ERROR_ALREADY_EXISTS == GetLastError())
    {
        // second instance
        SetEvent(popup_event);
        CloseHandle(popup_event);
        popup_event = nullptr;
        return false;
    }
    CloseHandle(CreateThread(nullptr, 0, multiinstanceblocker, nullptr, 0, nullptr));
    return true;
}

parsed_command_line_s g_commandline;

static bool parsecmdl(const wchar_t *cmdl)
{
    ts::wstr_c wd;
    ts::set_work_dir(wd);

    ts::token<wchar_t> cmds(cmdl, ' ');
    bool wait_cmd = false;
    bool installto = false;
    bool conf = false;
    uint processwait = 0;
    for (; cmds; ++cmds)
    {
        if (wait_cmd)
        {
            processwait = cmds->as_uint();
            wait_cmd = false;
            continue;
        }

        if (installto)
        {
            if (ts::is_admin_mode())
            {
                ts::wstr_c installto_path(*cmds);
                installto_path.replace_all('*', ' ');
                install_to(installto_path, false);
            }
            return false;
        }

        if (conf)
        {
            if (!g_commandline.alternative_config_path.is_empty()) g_commandline.alternative_config_path.append_char(' ');
            g_commandline.alternative_config_path.append(*cmds);
            if ( g_commandline.alternative_config_path.get_char(0) != '\"' || g_commandline.alternative_config_path.get_last_char() == '\"' )
            {
                conf = false;
                g_commandline.alternative_config_path.trunc_char('\"');
                if (g_commandline.alternative_config_path.get_char(0) == '\"')
                    g_commandline.alternative_config_path.cut(0);
            }
            continue;
        }

        if (cmds->equals(CONSTWSTR("wait")))
        {
            wait_cmd = true;
            continue;
        }

        if (cmds->equals(CONSTWSTR("multi")))
        {
            g_commandline.checkinstance = false;
            continue;
        }

        if (cmds->equals(CONSTWSTR("minimize")))
        {
            g_commandline.minimize = true;
            continue;
        }

        if (cmds->equals(CONSTWSTR("readonly")))
        {
            g_commandline.readonlymode = true;
            continue;
        }

        if (cmds->equals(CONSTWSTR("installto")))
        {
            installto = true;
            continue;
        }

        if (cmds->equals(CONSTWSTR("config")))
        {
            conf = true;
            continue;
        }

    }

    if (processwait)
    {
        HANDLE h = OpenProcess(SYNCHRONIZE,FALSE,processwait);
        if (h)
        {
            DWORD r = WaitForSingleObject(h, 10000);
            CloseHandle(h);
            if (r == WAIT_TIMEOUT ) return false;
        }
    }

    return true;
}

bool check_autoupdate();
extern "C" { void sodium_init(); }

bool _cdecl app_preinit( const wchar_t *cmdl )
{
#if defined _DEBUG || defined _CRASH_HANDLER
    ts::exception_operator_c::set_unhandled_exception_filter();
#endif

    sodium_init();

    if (!parsecmdl(cmdl))
        return false;

    if (!check_autoupdate())
        return false;

#ifdef _FINAL
    if (g_commandline.checkinstance)
        if (!check_instance()) return false;
#endif // _FINAL


#if defined _DEBUG || defined _CRASH_HANDLER
    ts::exception_operator_c::dump_filename = ts::fn_change_name_ext(ts::get_exe_full_name(), ts::wstr_c(CONSTWSTR(APPNAME)).append_char('.').append(ts::to_wstr(application_c::appver())).as_sptr(), CONSTWSTR("dmp"));
#endif

	ts::tsfileop_c::setup<fileop_c>();

	ts::CHARz_copy( g_sysconf.name, CONSTWSTR(APPNAME) .s );
    ts::CHARz_add_str( g_sysconf.name, L" " );
    ts::CHARz_add_str( g_sysconf.name, ts::to_wstr(application_c::appver()).cstr() );
    
#ifndef _FINAL
    debug_name();
#endif

	TSNEW(application_c, cmdl); // not a memory leak! see SEV_EXIT handler

	return true;
}
