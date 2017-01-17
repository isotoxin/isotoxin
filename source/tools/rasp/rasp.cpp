#include "stdafx.h"

#ifdef _WIN32
#if defined (_M_AMD64) || defined (WIN64)
#define LIBSUFFIX "64.lib"
#else
#define LIBSUFFIX ".lib"
#endif

#ifdef _FINAL
#define USELIB(ln) comment(lib, #ln LIBSUFFIX)
#elif defined _DEBUG_OPTIMIZED
#define USELIB(ln) comment(lib, #ln "do" LIBSUFFIX)
#else
#define USELIB(ln) comment(lib, #ln "d" LIBSUFFIX)
#endif

#ifdef _MSC_VER
#pragma USELIB( toolset )
#pragma USELIB( memspy )
#pragma USELIB( rsvg )
#pragma USELIB( ipc )

#pragma comment(lib, "freetype.lib")
#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "minizip.lib")
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "curl.lib")
#pragma comment(lib, "toxcore.lib")
#pragma comment(lib, "libsodium.lib")
#pragma comment(lib, "libqrencode.lib")
#pragma comment(lib, "cairo.lib")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "comctl32.lib")
//#pragma comment(lib, "Wldap32.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif
#endif // _MSC_VER
#endif

using namespace ts;

typedef int(*cmdproc)(const wstrings_c & pars);

int proc_help(const wstrings_c & pars);
int proc_show(const wstrings_c & pars);
int proc_loc(const wstrings_c & pars);
int proc_lochange(const wstrings_c & pars);
int proc_trunc(const wstrings_c & pars);
int proc_antic99(const wstrings_c & pars);
int proc_grabnodes(const wstrings_c & pars);
#ifdef _WIN32
int proc_toxrelay(const wstrings_c & pars);
#endif
int proc_http(const wstrings_c & pars);
int proc_hgver(const wstrings_c & pars);
int proc_upd(const wstrings_c & pars);
int proc_sign(const wstrings_c & pars);
int proc_emoji(const wstrings_c & pars);
int proc_dos2unix(const wstrings_c & pars);
int proc_unix2dos(const wstrings_c & pars);
int proc_grab(const wstrings_c & pars);
int proc_test(const wstrings_c & pars);
int proc_ut(const wstrings_c & pars);
int proc_utp(const wstrings_c & pars);
int proc_rgbi420(const wstrings_c & pars);
int proc_i420rgb(const wstrings_c & pars);
int proc_bsdl(const wstrings_c & pars);
int proc_rsvg( const wstrings_c & pars );
int proc_fxml( const wstrings_c & pars );

int proc_loc_(const wstrings_c & pars)
{
    int r = 0;
    UNSTABLE_CODE_PROLOG
        r = proc_loc(pars);
    UNSTABLE_CODE_EPILOG
    return r;
}

int proc_lochange_(const wstrings_c & pars)
{
    int r = 0;
    UNSTABLE_CODE_PROLOG
        r = proc_lochange(pars);
    UNSTABLE_CODE_EPILOG
    return r;
}

struct command_s
{
    const ts::wchar *cmd;
    const ts::wchar *help;
    cmdproc proc;

    command_s(const ts::wchar *_c, const ts::wchar *_h, cmdproc _p) :cmd(_c), help(_h), proc(_p) {}
} commands[] =
{
    command_s(WIDE2( "help" ), WIDE2("Show this help"), proc_help),
    command_s(WIDE2( "show" ), WIDE2("Show params for debug purpose"), proc_show),
    command_s(WIDE2( "loc" ), WIDE2("Generate Locale [path-to-source] [path-to-locale]"), proc_loc_),
    command_s(WIDE2( "changeloc" ), WIDE2("Change Locale [path-to-source] [path-to-locale] [locale default en]"), proc_lochange_),
    command_s(WIDE2( "trunc" ), WIDE2("Truncate [file] at [offset-from-begining]"), proc_trunc),
    command_s(WIDE2( "antic99" ), WIDE2("Remove C99 dependence [c-file]"), proc_antic99),
    command_s(WIDE2( "fxml" ), WIDE2("format [xml-file]"), proc_fxml ),
    command_s(WIDE2( "nodes" ), WIDE2("Grab nodes list from https://wiki.tox.chat/users/nodes"), proc_grabnodes ),
    command_s(WIDE2( "http" ), WIDE2("Do some http ops"), proc_http),
#ifdef _WIN32
    command_s( WIDE2( "toxrelay"), WIDE2( "Just run TOX relay"), proc_toxrelay),
#endif
    command_s( WIDE2( "hgver"), WIDE2( "Prints current hg revision"), proc_hgver),
    //command_s(WIDE2( "upd"), WIDE2( "Load isotoxin update"), proc_upd),
    command_s( WIDE2("sign"), WIDE2( "Sign archive"), proc_sign),
    command_s( WIDE2("emoji"), WIDE2( "Create emoji table"), proc_emoji),
    command_s( WIDE2("dos2unix"), WIDE2( "Convert CRLF to LF"), proc_dos2unix),
    command_s( WIDE2("unix2dos"), WIDE2( "Convert LF to CRLF"), proc_unix2dos),
    command_s( WIDE2("grab"), WIDE2( "grab monitor 0 to png"), proc_grab),
    command_s( WIDE2("test"), WIDE2( "internal tests, do not use"), proc_test),
    command_s( WIDE2("ut"), WIDE2("unit tests, do not use"), proc_ut),
    command_s( WIDE2("utp"), WIDE2("unit tests params, do not use"), proc_utp),
    command_s( WIDE2("rgbi420"), WIDE2( "convert image [file] to i420"), proc_rgbi420),
    command_s( WIDE2("i420rgb"), WIDE2( "convert image [file] to png"), proc_i420rgb),
    command_s( WIDE2("bsdl"), WIDE2( "Build spelling dictionary list of [path] with *.aff and *.dic files"), proc_bsdl),
    command_s( WIDE2("rsvg"), WIDE2( "Render [svg-file] to png"), proc_rsvg ),
};


#ifdef _WIN32
HANDLE hConsoleOutput;
CONSOLE_SCREEN_BUFFER_INFO csbi;

class SetConsoleColor
{
    HANDLE hConsoleOutput;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

public:
    SetConsoleColor(WORD color)
    {
        GetConsoleScreenBufferInfo(hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        SetConsoleTextAttribute(hConsoleOutput, color | (csbi.wAttributes & 0xF0));
    }
    ~SetConsoleColor()
    {
        SetConsoleTextAttribute(hConsoleOutput, csbi.wAttributes);
    }
};
#endif // _WIN32

void Print(int color, const char *format, ...)
{
#ifdef _WIN32
    SetConsoleColor cc((WORD)color | FOREGROUND_INTENSITY);
#endif // _WIN32
    sstr_c buf;
    va_list arglist;
    va_start(arglist, format);
#ifdef _WIN32
    vsprintf_s(buf.str(), buf.get_capacity(), format, arglist);
    CharToOemA(buf.str(),buf.str());
#endif // _WIN32
#ifdef _NIX
    vsnprintf(buf.str(), buf.get_capacity(), format, arglist);
#endif
    printf("%s",buf.cstr());
}

void Print(const char *format, ...)
{
    va_list arglist;
    sstr_c buf;
    va_start(arglist, format);
#ifdef _WIN32
    vsprintf_s(buf.str(), buf.get_capacity(), format, arglist);
#endif
#ifdef _NIX
    vsnprintf(buf.str(), buf.get_capacity(), format, arglist);
#endif
    printf("%s", buf.cstr());
}

bool TSCALL ts::app_preinit( const ts::wchar *cmdl )
{
    return true;
}

int main(int argc, char* argv[])
{
    sodium_init();
#ifdef _WIN32
	GetConsoleScreenBufferInfo(hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    setlocale(LC_ALL, ".1251");
#endif

    MEMT( MEMT_LAST + 1 );

    wstrings_c ql;
#ifdef _WIN32
    wchar *cmdlb = GetCommandLineW();
    ql.qsplit( cmdlb );
#endif // _WIN32
#ifdef _NIX
    for( int i=0;i<argc;++i )
        ql.add( from_utf8(argv[i]) );
#endif

    if (ql.size() < 2)
    {
        return proc_help( ql );
    } else
    {
        int cnt = ARRAY_SIZE(commands);
        for (int i=0;i<cnt;++i)
        {
            if ( ql.get(1).equals_ignore_case(wsptr(commands[i].cmd)) )
            {
				ql.remove_slow(0);
                return commands[i].proc( ql );
            }
        }
        Print("Unknown command : %s\n",to_utf8( ql.get( 1 ) ).cstr());
    }
	return 1;
}

#define HOME_SITE "http://isotoxin.im"

#if 0
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

int proc_help(const wstrings_c & pars)
{
#if 0
    int pagesize = sysconf(_SC_PAGESIZE);
    int l = shm_open("rasp_shared_block", O_CREAT|O_RDWR, 0666 );
    ftruncate(l, pagesize * 33);

    void *ptr = mmap(nullptr, pagesize * 33, PROT_READ|PROT_WRITE, MAP_SHARED, l, 0 );
    close(l);
    munmap(ptr, pagesize * 33);
#endif



    //ts::wstr_c title;
    //title.append( CONSTWSTR("<a href=\"" HOME_SITE "\">Isotoxin</a>") );

#ifdef MODE64
    Print("Isotoxin Rasp (x64, %s)\n", __DATE__);
    #ifdef _NIX
        //Print( "%s\n", ts::to_utf8(title).cstr() );
    #endif
#else
    Print( "Isotoxin Rasp (%s)\n", __DATE__ );
#endif
    Print("  Commands:\n");

    wstr_c cmn;
    int maxl = 0;
    ts::aint cnt = ARRAY_SIZE(commands);
    for ( ts::aint i = 0; i < cnt; ++i)
    {
        maxl = tmax(maxl, CHARz_len(commands[i].cmd));
    }

    for ( ts::aint i = 0; i < cnt; ++i)
    {
        cmn.fill(maxl + 1, ' ');
        memcpy(cmn.str(), commands[i].cmd, CHARz_len(commands[i].cmd) * sizeof(wchar));
        Print("    %s - %s\n", to_utf8(cmn).cstr(), to_utf8(commands[i].help).cstr());
    }

    return 0;
}

int proc_show(const wstrings_c & pars)
{
    ts::aint cnt = pars.size();
    for( int i=0;i<cnt;++i)
    {
        Print("#%i: [%s]\n", i, to_str(pars.get(i)).cstr());
    }
    return 0;
}

int proc_trunc(const wstrings_c & pars)
{
    if (pars.size() == 3)
    {
#ifdef _WIN32
        Print("Open: %s\n", to_str(pars.get(1)).cstr());
        HANDLE f = CreateFileW(pars.get(1), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE)
        {
            Print("Trunc at: %s\n", to_str(pars.get(2)).cstr());
            uint64 offset = pars.get(2).as_num<uint64>();
            uint64 size;
            GetFileSizeEx(f,(LARGE_INTEGER *)&size);
            if (offset < size)
            {
                uint64 s;
                SetFilePointerEx(f,ref_cast<LARGE_INTEGER>(offset), (LARGE_INTEGER *)&s,FILE_BEGIN);
                SetEndOfFile(f);
            }

            CloseHandle(f);
        }
#endif
    }
    return 0;
}

#ifdef _NIX
int GetEnvironmentVariableW(const ts::wsptr &name, ts::wchar *buf, int bufl);
#endif

int proc_hgver(const wstrings_c & pars)
{
    ts::wstr_c buf(16385,false);
    GetEnvironmentVariableW(ts::wstr_c(CONSTWSTR("path")), buf.str(), (int)buf.get_capacity()-1);
    buf.set_length();
    ts::wstrings_c paths(buf,';');
    paths.trim();
    for( const ts::wstr_c &p : paths )
    {
        ts::wstr_c hgp( ts::fn_join(p, CONSTWSTR("hg.")) );
        bool fnd = false;
        if ( ts::is_file_exists( (hgp + CONSTWSTR("exe")).as_sptr() ) )
        {
            hgp.append( CONSTWSTR("exe") );
            fnd = true;
        } else if (ts::is_file_exists((hgp + CONSTWSTR("bat")).as_sptr()))
        {
            hgp.append(CONSTWSTR("bat"));
            fnd = true;
        } else if (ts::is_file_exists((hgp + CONSTWSTR("cmd")).as_sptr()))
        {
            hgp.append(CONSTWSTR("cmd"));
            fnd = true;
        }
        if (fnd)
        {
            //ts::start_app();
            DEBUG_BREAK();
        }
    }

    return 0;
}

int proc_grab(const wstrings_c & pars)
{
    ts::sys_sleep(5000);
    #ifdef _WIN32
    Beep(1000,100);
    #endif
    int monitor = 0;
    irect gr = monitor_get_max_size_fs(monitor);

    ts::drawable_bitmap_c grabbuff;
    if (gr.size() != grabbuff.info().sz)
        grabbuff.create(gr.size(), monitor);

    grabbuff.grab_screen( gr, ts::ivec2(0) );

    buf_c cursorcachedata;
    grabbuff.render_cursor(gr.lt, cursorcachedata);

    grabbuff.fill_alpha(255);
    grabbuff.save_as_png( CONSTWSTR("m0.png") );

    return 0;
}

int proc_rgbi420(const wstrings_c & pars)
{
    if (pars.size() < 2)
        return 0;

    bitmap_c bmp;
    bmp.load_from_file( pars.get(1) );

    buf_c buf;
    bmp.convert_to_yuv( ivec2(0), bmp.info().sz, buf, YFORMAT_I420);
    buf.save_to_file( CONSTWSTR("i420.bin") );
    return 0;
}

int proc_i420rgb(const wstrings_c & pars)
{
    if (pars.size() < 4)
        return 0;

    buf_c buf;
    buf.load_from_disk_file(pars.get(1));

    int w = pars.get(2).as_int();
    int h = pars.get(3).as_int();

    Print( "converting from i420 %i x %i\n", w, h );

    int i420sz = w * h + w * h/2;
    if (i420sz != buf.size())
    {
        Print( "bad file size, %i expected\n", i420sz );
        return 0;
    }

    bitmap_c bmp;

    bmp.create_ARGB( ivec2(w, h) );
    bmp.convert_from_yuv(ivec2(0), bmp.info().sz, buf.data(), YFORMAT_I420);
    //bmp.save_as_png(L"i420.png");
    bmp.save_as_png(fn_get_name( pars.get(1) ).append(CONSTWSTR(".png")));


    bmp.create_ARGB(ivec2(w/2, h/2));
    bmp.convert_from_yuv(ivec2(0), bmp.info().sz, buf.data(), YFORMAT_I420x2);
    //bmp.save_as_png(L"i420div2.png");
    bmp.save_as_png(fn_get_name( pars.get(1) ).append(CONSTWSTR("_div2.png")));

    return 0;
}

#ifdef _WIN32
// dlmalloc -----------------
#define SLASSERT ASSERTO
#define SLERROR ERROR
#include "spinlock/spinlock.h"
#pragma warning (disable:4559)
#pragma warning (disable:4127)
#pragma warning (disable:4057)
#pragma warning (disable:4702)

#define MALLOC_ALIGNMENT ((size_t)16U)
#define USE_DL_PREFIX
#define USE_LOCKS 0

static spinlock::long3264 dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

extern "C"
{
#include "dlmalloc/dlmalloc.c"
}
#endif