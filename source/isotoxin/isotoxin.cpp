#include "isotoxin.h"

#pragma USELIB(toolset)
#pragma USELIB(rectangles)
#pragma USELIB(ipc)
#pragma USELIB(s3)
#pragma USELIB(dshowcapture)
#pragma USELIB(rsvg)
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

#if defined _DEBUG_OPTIMIZED || defined _FINAL
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "hunspell.lib")
#else
#pragma comment(lib, "sqlite3d.lib")
#pragma comment(lib, "hunspelld.lib")
#endif

#pragma comment (lib, "freetype.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "minizip.lib")
#pragma comment(lib, "curl.lib")
#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "libqrencode.lib")
#pragma comment(lib, "cairo.lib")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msacm32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")

#ifdef _WIN32
#include "toolset/_win32/win32_inc.inl"
#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

// disable VS2015 telemetry (Th you, kidding? Fuck spies.)
extern "C"
{
    void _cdecl __vcrt_initialize_telemetry_provider() {}
    void _cdecl __telemetry_main_invoke_trigger() {}
    void _cdecl __telemetry_main_return_trigger() {}
    void _cdecl __vcrt_uninitialize_telemetry_provider() {}
};
#endif

namespace
{

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
            long i0, i1;
            int prior = 0;
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
void dotests0();
#endif

#pragma warning (disable : 4505) //: 'check_instance' : unreferenced local function has been removed

static void popup_notify()
{
    gmsg<ISOGM_APPRISE> *m = TSNEW( gmsg<ISOGM_APPRISE> );
    m->send_to_main_thread();
}

static bool check_instance()
{
    return ts::master().sys_one_instance( ts::wstr_c(CONSTWSTR("isotoxin_popup_event")), popup_notify );
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
                    g_commandline.alternative_config_path.cut(0,1);
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
        ts::process_handle_s ph;
        if ( ts::master().open_process( processwait, ph ) )
            if ( !ts::master().wait_process( ph, 10000 ) )
                return false;
    }

    return true;
}

bool check_autoupdate();
extern "C" { void sodium_init(); }

void set_unhandled_exception_filter();
void set_dump_filename( const ts::wsptr& n );

bool _cdecl ts::app_preinit( const wchar_t *cmdl )
{
#if defined _DEBUG || defined _CRASH_HANDLER
    set_unhandled_exception_filter();
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
    set_dump_filename( ts::fn_change_name_ext( ts::get_exe_full_name(), ts::wstr_c( CONSTWSTR( APPNAME ) ).append_char( '.' ).append( ts::to_wstr( application_c::appver() ) ).as_sptr(), 

#ifdef MODE64
        CONSTWSTR( "x64.dmp" ) ) );
#else
        CONSTWSTR( "dmp" ) ) );
#endif // MODE64                   

#endif

	ts::tsfileop_c::setup<fileop_c>();

#ifndef _FINAL
    dotests0();
#endif

    ts::wstrings_c fns;
    ts::g_fileop->find( fns, CONSTWSTR( "loc/*.lng*.lng" ), false );
    if (!fns.size() || !ts::is_file_exists( PLGHOSTNAME ))
    {
        //bool download = false;
        //ts::wstr_c path_exe( ts::fn_get_path( ts::get_exe_full_name() ) );
        //if (ts::check_write_access( path_exe ))
        //    download = ts::SMBR_YES == ts::sys_mb( L"error", L"Data not found! Do you want to download data now?", ts::SMB_YESNO_ERROR );
        //else
            ts::sys_mb( L"error", L"Application data not found. Please re-install application", ts::SMB_OK_ERROR );

        return false;
    }

	TSNEW(application_c, cmdl); // not a memory leak! see SEV_EXIT handler

	return true;
}
