#include "stdafx.h"

#ifdef _FINAL
#define USELIB(ln) comment(lib, #ln ".lib")
#elif defined _DEBUG_OPTIMIZED
#define USELIB(ln) comment(lib, #ln "do.lib")
#else
#define USELIB(ln) comment(lib, #ln "d.lib")
#endif

#pragma USELIB( toolset )
#pragma USELIB( memspy )

#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "minizip.lib")
#pragma comment(lib, "sqlite3.lib")
#pragma comment(lib, "curl.lib")
#pragma comment(lib, "toxcore.lib")
#pragma comment(lib, "libsodium.lib")

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "comctl32.lib")
//#pragma comment(lib, "Wldap32.lib")

#if defined _FINAL || defined _DEBUG_OPTIMIZED
#include "crt_nomem/crtfunc.h"
#endif

using namespace ts;

typedef int(*cmdproc)(const wstrings_c & pars);

int proc_help(const wstrings_c & pars);
int proc_show(const wstrings_c & pars);
int proc_loc(const wstrings_c & pars);
int proc_trunc(const wstrings_c & pars);
int proc_antic99(const wstrings_c & pars);
int proc_grabnodes(const wstrings_c & pars);
int proc_toxrelay(const wstrings_c & pars);
int proc_http(const wstrings_c & pars);
int proc_hgver(const wstrings_c & pars);
int proc_upd(const wstrings_c & pars);
int proc_sign(const wstrings_c & pars);
int proc_emoji(const wstrings_c & pars);

int proc_loc_(const wstrings_c & pars)
{
    int r = 0;
    UNSTABLE_CODE_PROLOG
        r = proc_loc(pars);
    UNSTABLE_CODE_EPILOG
    return r;
}

struct command_s
{
    const wchar *cmd;
    const wchar *help;
    cmdproc proc;

    command_s(const wchar *_c, const wchar *_h, cmdproc _p) :cmd(_c), help(_h), proc(_p) {}
} commands[] =
{
    command_s(L"help", L"Show this help", proc_help),
    command_s(L"show", L"Show params for debug purpose", proc_show),
    command_s(L"loc", L"Generate Locale [path-to-source] [path-to-locale]", proc_loc_),
    command_s(L"trunc", L"Truncate [file] at [offset-from-begining]", proc_trunc),
    command_s(L"antic99", L"Remove C99 dependence [c-file]", proc_antic99),
    command_s(L"nodes", L"Grab nodes list from https://wiki.tox.chat/users/nodes", proc_grabnodes),
    command_s(L"http", L"Do some http ops", proc_http),
    command_s(L"toxrelay", L"Just run TOX relay", proc_toxrelay),
    command_s(L"hgver", L"Prints current hg revision", proc_hgver),
    //command_s(L"upd", L"Load isotoxin update", proc_upd),
    command_s(L"sign", L"Sign archive", proc_sign),
    command_s(L"emoji", L"Create emoji table", proc_emoji),
};


HANDLE hConsoleOutput;
CONSOLE_SCREEN_BUFFER_INFO csbi;
void(*PrintCustomHandler)(int color, const asptr& s) = nullptr;

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

void Print(WORD color, const char *format, ...)
{
    SetConsoleColor cc(color | FOREGROUND_INTENSITY);
    sstr_c buf;
    va_list arglist;
    va_start(arglist, format);
    vsprintf_s(buf.str(), buf.get_capacity(), format, arglist);
    CharToOemA(buf.str(),buf.str());
    printf("%s",buf.cstr());
    if (PrintCustomHandler) PrintCustomHandler(color, buf);
}

void Print(const char *format, ...)
{
    va_list arglist;
    sstr_c buf;
    va_start(arglist, format);
    vsprintf_s(buf.str(), buf.get_capacity(), format, arglist);
    printf("%s", buf.cstr());
    if (PrintCustomHandler) PrintCustomHandler(-1, buf);
}
extern "C" { void sodium_init(); }
int main(int argc, _TCHAR* argv[])
{
    sodium_init();
	GetConsoleScreenBufferInfo(hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	/*SetConsoleOutputCP(1251);/*/
    setlocale(LC_ALL, ".1251");
    //setlocale(LC_ALL, ".866");
    //SetConsoleOutputCP влияет на все последующие команды в той же сессии cmd

    wchar_t *cmdlb = GetCommandLineW();
    wstrings_c ql; ql.qsplit( cmdlb );

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
        wstr_c er(L"Unknown command: "); er.append( ql.get(1) );
        wprintf(L"%s\n",er.cstr());
    }
	return 1;
}

int proc_help(const wstrings_c & pars)
{
    printf("Isotoxin Rasp (%s)\n", __DATE__);
    printf("  Commands:\n");

    wstr_c cmn;
    int maxl = 0;
    int cnt = ARRAY_SIZE(commands);
    for (int i = 0; i < cnt; ++i)
    {
        maxl = tmax(maxl, CHARz_len(commands[i].cmd));
    }

    for (int i = 0; i < cnt; ++i)
    {
        cmn.fill(maxl + 1, ' ');
        memcpy(cmn.str(), commands[i].cmd, CHARz_len(commands[i].cmd) * sizeof(wchar));
        wprintf(L"    %s - %s\n", cmn.cstr(), commands[i].help);
    }

    return 0;
}

int proc_show(const wstrings_c & pars)
{
    int cnt = pars.size();
    for(int i=0;i<cnt;++i)
    {
        printf("#%i: [%s]\n", i, to_str(pars.get(i)).cstr());
    }
    return 0;
}

int proc_trunc(const wstrings_c & pars)
{
    if (pars.size() == 3)
    {
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

    
    }
    return 0;
}

int proc_hgver(const wstrings_c & pars)
{
    ts::wstr_c buf(16385,false);
    GetEnvironmentVariableW(L"path", buf.str(), buf.get_capacity()-1);
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
            __debugbreak();
        }
    }

    return 0;
}


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

static long dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

extern "C"
{
#include "dlmalloc/dlmalloc.c"
}
