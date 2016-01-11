#include "toolset.h"
#include <intrin.h>
#include <stdio.h>
#include <time.h>
#include <Ws2tcpip.h>

namespace ts {

bool g_warning_inprogress = false, force_all_warnings_show_mb = true;//чтобы во всех тулзах показывались warnings всегда
int ignoredWarnings;
HWND g_main_window = nullptr;

void LogMessage(const char *caption, const char *msg)
{
#if defined _DEBUG || defined _DEBUG_OPTIMIZED || defined _CRASH_HANDLER
	FILE *f = nullptr;
	fopen_s(&f, "messages.log", "ab");
	if (f)
	{
		char module[MAX_PATH];
		GetModuleFileNameA(NULL, module, LENGTH(module));
		tm t;
		time_t curtime;
		time(&curtime);
		localtime_s(&t, &curtime);
        if (caption)
		    fprintf(f, "-------------------------\r\nMODULE: %s\r\nDATETIME: %i-%02i-%02i %02i:%02i\r\nCAPTION: %s\r\nMESSAGE: %s\r\n\r\n", strrchr(module, '\\')+1, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, caption, msg);
        else
            fprintf(f, "%s (%i-%02i-%02i %02i:%02i) : %s\r\n", strrchr(module, '\\') + 1, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, msg);
		fclose(f);
	}
#endif
}

int MessageBoxDef(const char *text, const char *notLoggedText, const char *caption, UINT type)
{
    
    if (IsDebuggerPresent() && GetWindowThreadProcessId(g_main_window, nullptr) != GetCurrentThreadId())
        __debugbreak();

	return MessageBoxA(g_main_window && GetWindowThreadProcessId(g_main_window, nullptr) == GetCurrentThreadId() ? g_main_window : nullptr, notLoggedText[0] ? tmp_str_c(text).append(notLoggedText).cstr() : text, caption, type|MB_TASKMODAL|MB_TOPMOST);
}
int (*MessageBoxOverride)(const char *text, const char *notLoggedText, const char *caption, UINT type) = &MessageBoxDef;
inline int LoggedMessageBox(const ts::str_c &text, const char *notLoggedText, const char *caption, UINT type) { LogMessage(caption, text); return MessageBoxOverride(text, notLoggedText, caption, type); }

static static_setup< hashmap_t<str_c, bool>, 0 > messages;

void Log(const char *s, ...)
{
    char str[65535];

    va_list args;
    va_start(args, s);
    vsprintf_s(str, LENGTH(str), s, args);
    va_end(args);

    LogMessage(nullptr, str);
}

bool Warning(const char *s, ...)
{
	if (g_warning_inprogress) return false;
	g_warning_inprogress = true;

	char str[4000];

	va_list args;
	va_start(args, s);
	vsprintf_s(str, LENGTH(str), s, args);
	va_end(args);

	ts::str_c msg = str;

//#ifdef _DEBUG
//	if (!warningContextStack.empty())
//	{
//		msg += "\nCONTEXT: ";
//		for (int i=0; i<warningContextStack.size(); i++)
//			(msg += warningContextStack[i]) += '\n';
//	}
//#endif

	bool result = false;
	if (messages().get(msg) == nullptr)
		switch (LoggedMessageBox(msg, "\n\nShow the same messages?", "Warning", MB_YESNOCANCEL))
	{
		case IDCANCEL:
			result = true;
			break;
		case IDNO:
			messages().add(msg);
			break;
	}
	g_warning_inprogress = false;
	return result;
}

void Error(const char *s, ...)
{
	char str[4000];

	va_list args;
	va_start(args, s);
	vsprintf_s(str, LENGTH(str), s, args);
	va_end(args);

	LogMessage("Error", str);
	if (MessageBoxDef(str, "", "Error", IsDebuggerPresent() ? MB_OKCANCEL : MB_OK) == IDCANCEL)
		__debugbreak();
}

bool AssertFailed(const char *file, int line, const char *s, ...)
{
	char str[4000];

	va_list args;
	va_start(args, s);
	vsprintf_s(str, LENGTH(str), s, args);
	va_end(args);

	return Warning("Assert failed at %s (%i)\n%s", file, line, str);
}

#ifndef _FINAL
tmcalc_c::~tmcalc_c()
{
	LARGE_INTEGER   timestamp2;
	LARGE_INTEGER   frq;
	QueryPerformanceCounter( &timestamp2 );
	QueryPerformanceFrequency( &frq );
	__int64 takts = timestamp2.QuadPart - m_timestamp.QuadPart;
	ts::str_c text( "Takts: " );
	text.append_as_uint( as_dword(takts) );
	text.append(", Time: ").append( roundstr<str_c>( 1000.0 * double(takts) / double(frq.QuadPart), 3 ) ).append(" ms");
	//LogMessage(m_tag, text);
    DMSG("tm: " << m_tag << text);
}
#endif //_FINAL

//#############################################################################################################
//     DMSG            //////////////////////////////////////////////////////////////////////////////////////
//#############################################################################################################
#ifdef __DMSG__

#pragma comment(lib, "ws2_32.lib")

namespace
{
    struct dmsg_internals
    {
        CRITICAL_SECTION     csect; //critical section for dmsg funtion
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

            InitializeCriticalSection(&csect);
        }
        ~dmsg_internals()
        {
            closesocket(sock);
            WSACleanup();
            DeleteCriticalSection(&csect);
        }
        void    sendstring_udp_datagram(const char *s, int len)
        {
            sendto(sock, s, len, 0, (SOCKADDR *)&saUdpServ, sizeof(SOCKADDR_IN));
        }
    };
    static_setup<dmsg_internals, 0> __dmsg__;
}




void _cdecl dmsg(const char *str)
{
    EnterCriticalSection(&__dmsg__().csect);

    int i = 0;
    int d = 0;
    for (i = 0; str[i]; ++i)
        if (str[i] == '\n')
        {
            __dmsg__().sendstring_udp_datagram(str + d, i-d);
            d = i + 1;
        }
    __dmsg__().sendstring_udp_datagram(str + d, CHARz_len(str+d));

    LeaveCriticalSection(&__dmsg__().csect);
}
#endif

#ifdef _DEBUG
delta_time_profiler_s::delta_time_profiler_s(int n) :n(n)
{
    entries = (entry_s *)MM_ALLOC( n*sizeof(entry_s) );
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    notfreq = 1000.0 / (double)freq.QuadPart;
    QueryPerformanceCounter(&prev);
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
    entries[index].deltams = (float)((double)(cur.QuadPart - prev.QuadPart) * notfreq);
    prev = cur;
    ++index;
    if (index >= n)
        index = 0;

}
#endif

} // namespace ts
