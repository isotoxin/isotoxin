#include "toolset.h"
#include "internal/platform.h"

#include <stdio.h>

#ifdef _WIN32
#if defined _DEBUG || defined _CRASH_HANDLER
#include "internal/excpn.h"
#endif

#pragma comment(lib, "dbghelp.lib")
#pragma message("Automatically linking with dbghelp.lib (dbghelp.dll)")

namespace ts {
#include "_win32/win32_common.inl"
}
#endif

namespace ts {


#ifndef _FINAL
tmcalc_c::tmcalc_c( const char *tag ) :m_tag( tag )
{
#ifdef _WIN32
    QueryPerformanceCounter( (LARGE_INTEGER *)&m_timestamp );
#endif // _WIN32
};
#endif // _FINAL

bool g_warning_inprogress = false;

void LogMessage(const char *caption, const char *msg)
{
#if defined _DEBUG || defined _DEBUG_OPTIMIZED || defined _CRASH_HANDLER
	FILE *f = fopen("messages.log", "ab");
	if (f)
	{
		char module[MAX_PATH_LENGTH];
        aint l = get_exe_full_name(module, ARRAY_SIZE(module));
        aint lastslash = 1 + pstr_c(asptr(module,l)).find_last_pos_of(CONSTASTR("\\/"));
		time_t curtime;
		time(&curtime);
		const tm &t = *localtime(&curtime);
        if (caption)
		    fprintf(f, "-------------------------\r\nMODULE: %s\r\nDATETIME: %i-%02i-%02i %02i:%02i\r\nCAPTION: %s\r\nMESSAGE: %s\r\n\r\n", module+lastslash, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, caption, msg);
        else
            fprintf(f, "%s (%i-%02i-%02i %02i:%02i) : %s\r\n", module+lastslash, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, msg);
		fclose(f);
	}
#endif
}

bool sys_is_debugger_present()
{
#ifdef _WIN32
    return IsDebuggerPresent() != FALSE;
#elif __linux__
    char buf[1024];
    int debugger_present = 0;

    int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
        return 0;

    ssize_t num_read = read(status_fd, buf, sizeof(buf));

    if (num_read > 0)
    {
        static const char TracerPid[] = "TracerPid:";
        char *tracer_pid;

        buf[num_read] = 0;
        tracer_pid    = strstr(buf, TracerPid);
        if (tracer_pid)
            debugger_present = !!atoi(tracer_pid + sizeof(TracerPid) - 1);
    }

    return debugger_present != 0;
#endif
}

smbr_e TSCALL sys_mb( const wchar *caption, const wchar *text, smb_e options )
{
#ifdef _WIN32

    ts::wnd_c *tbw = master().activewindow;
    if ( !tbw ) tbw = master().mainwindow;

    if ( tbw && IsDebuggerPresent() && GetWindowThreadProcessId( wnd2hwnd( tbw ), nullptr ) != GetCurrentThreadId() )
        __debugbreak();

    HWND par = tbw && GetWindowThreadProcessId( wnd2hwnd( tbw ), nullptr ) == GetCurrentThreadId() ? wnd2hwnd( tbw ) : nullptr;
    UINT f = 0;

    switch ( options )
    {
    case SMB_OK_ERROR:
        f |= MB_ICONERROR;
        // no break here
    case SMB_OK:
        f |= MB_OK;
        break;
    case SMB_OKCANCEL:
        f |= MB_OKCANCEL;
        break;
    case SMB_YESNOCANCEL:
        f |= MB_YESNOCANCEL;
        break;
    case SMB_YESNO_ERROR:
        f |= MB_ICONERROR;
        // no break here
    case SMB_YESNO:
        f |= MB_YESNO;
        break;
    default:
        break;
    }

    int rslt = MessageBoxW( par, text, caption, f | MB_TASKMODAL | MB_TOPMOST );
    switch ( rslt )
    {
    case IDOK:
        return SMBR_OK;
    case IDYES:
        return SMBR_YES;
    case IDNO:
        return SMBR_NO;
    case IDCANCEL:
        return SMBR_CANCEL;
    }
    return SMBR_UNKNOWN;
#endif
#ifdef _NIX
    return master().sys_mb(caption, text, options);
#endif // _NIX

}


static smbr_e LoggedMessageBox(const str_c &text, const char *notLoggedText, const char *caption, smb_e f )
{
    LogMessage(caption, text);
    return sys_mb( to_wstr(caption), to_wstr(notLoggedText[ 0 ] ? str_c( text ).append( notLoggedText ) : text), f );
}

static static_setup< hashmap_t<str_c, bool>, 0 > messages;

void Log(const char *s, ...)
{
    char str[65535];

    va_list args;
    va_start(args, s);
    vsnprintf( str, ARRAY_SIZE( str ), s, args );
    va_end(args);

    LogMessage(nullptr, str);
}

bool Warning(const char *s, ...)
{
	if (g_warning_inprogress) return false;
	g_warning_inprogress = true;

	ts::str_c str(4000, false);

	va_list args;
	va_start(args, s);
    str.set_length( vsnprintf(str.str(), str.get_capacity(), s, args) );
	va_end(args);

	bool result = false;
	if (messages().get(str) == nullptr)
		switch (LoggedMessageBox(str, "\n\nShow the same messages?", "Warning", SMB_YESNOCANCEL))
	    {
		    case SMBR_CANCEL:
			    result = true;
			    break;
            case SMBR_NO:
			    messages().add(str.make_clone());
			    break;
	    }
	g_warning_inprogress = false;
	return result;
}

void Error(const char *s, ...)
{
    ts::str_c str(4000, false);

	va_list args;
	va_start(args, s);
    str.set_length(vsnprintf(str.str(), str.get_capacity(), s, args));
	va_end(args);

	if ( LoggedMessageBox(str, "", "Error", sys_is_debugger_present() ? SMB_OKCANCEL : SMB_OK) == SMBR_CANCEL)
		DEBUG_BREAK();
}

bool AssertFailed(const char *file, int line, const char *s, ...)
{
	char str[4000];

	va_list args;
	va_start(args, s);
    vsnprintf(str, ARRAY_SIZE(str), s, args);
	va_end(args);

	return Warning("Assert failed at %s (%i)\n%s", file, line, str);
}

#ifndef _FINAL
tmcalc_c::~tmcalc_c()
{
#ifdef _WIN32
	LARGE_INTEGER   timestamp2;
	LARGE_INTEGER   frq;
	QueryPerformanceCounter( &timestamp2 );
	QueryPerformanceFrequency( &frq );
	__int64 takts = timestamp2.QuadPart - m_timestamp;
	ts::str_c text( "Takts: " );
	text.append_as_uint( as_dword(takts) );
	text.append(", Time: ").append( roundstr<str_c>( 1000.0 * double(takts) / double(frq.QuadPart), 3 ) ).append(" ms");
	//LogMessage(m_tag, text);
    DMSG("tm: " << m_tag << text);
#endif
}
#endif //_FINAL

//#############################################################################################################
//     DMSG            //////////////////////////////////////////////////////////////////////////////////////
//#############################################################################################################
#ifdef __DMSG__

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace
{
    struct dmsg_internals
    {
        spinlock::long3264   sync = 0;
        WSADATA              wsa;
        SOCKET               sock;
        SOCKADDR_IN          saUdpServ;

        dmsg_internals()
        {
            int debug_port = 21212;
            sock = INVALID_SOCKET;

            SOCKADDR_IN saUdpCli;

            int iRetVal = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (0 != iRetVal)
            {
                return;
            }

            sock = socket(AF_INET, SOCK_DGRAM, 0);
            int err;

            // bind to a local socket and an interface.

            saUdpCli.sin_family = AF_INET;
            saUdpCli.sin_addr.s_addr = htonl(INADDR_ANY);
            saUdpCli.sin_port = htons(0);

            err = bind(sock, (SOCKADDR *)&saUdpCli, sizeof(SOCKADDR_IN));

            if (err == SOCKET_ERROR)
            {
                WARNING("DoUdpClient, bind %i", WSAGetLastError());
                return;
            }

            // Fill an IP address structure, to send an IP broadcast. The
            // packet will be broadcasted to the specified port.
            saUdpServ.sin_family = AF_INET;
            //InetPton(AF_INET, "127.0.0.1", &saUdpServ.sin_addr.s_addr);
            saUdpServ.sin_addr.s_addr = inet_addr("127.0.0.1");
            saUdpServ.sin_port = htons((word)debug_port);

        }
        ~dmsg_internals()
        {
            closesocket(sock);
            WSACleanup();
        }
        void    sendstring_udp_datagram(const asptr &s)
        {
            sendto(sock, s.s, (int)s.l, 0, (SOCKADDR *)&saUdpServ, sizeof(SOCKADDR_IN));
        }
    };
    static_setup<dmsg_internals, 0> __dmsg__;
}




void dmsg(const char *str)
{
    SIMPLELOCK(__dmsg__().sync);

    int i = 0;
    int d = 0;
    for (i = 0; str[i]; ++i)
        if (str[i] == '\n')
        {
            __dmsg__().sendstring_udp_datagram(asptr(str + d, i-d));
            d = i + 1;
        }
    __dmsg__().sendstring_udp_datagram(asptr(str + d));

}
#endif

#ifdef _DEBUG
delta_time_profiler_s::delta_time_profiler_s(int n) :n(n)
{
    entries = (entry_s *)MM_ALLOC( n*sizeof(entry_s) );
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    notfreq = 1000.0 / (double)freq.QuadPart;
    QueryPerformanceCounter( ( LARGE_INTEGER * ) &prev);
}
delta_time_profiler_s::~delta_time_profiler_s()
{
    MM_FREE(entries);
}
void delta_time_profiler_s::operator()(int id)
{
    entries[index].id = id;

    LARGE_INTEGER cur;
    QueryPerformanceCounter(&cur);
    entries[index].deltams = (float)((double)(cur.QuadPart - prev) * notfreq);
    prev = (uint64 &)cur;
    ++index;
    if (index >= n)
        index = 0;

}
#endif

} // namespace ts
