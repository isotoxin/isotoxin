#include "stdafx.h"

#define LENGTH(a) (sizeof(a)/sizeof(a[0]))

void LogMessage(const char *fn, const char *caption, const char *msg)
{
#if defined _DEBUG || defined _DEBUG_OPTIMIZED
    FILE *f = nullptr;
    fopen_s(&f, fn ? fn : "plghost.log", "ab");
    if (f)
    {
        char module[MAX_PATH];
        GetModuleFileNameA(NULL, module, LENGTH(module));
        tm t;
        time_t curtime;
        time(&curtime);
        localtime_s(&t, &curtime);
        if (caption)
            fprintf(f, "-------------------------\r\nMODULE: %s\r\nDATETIME: %i-%02i-%02i %02i:%02i\r\nCAPTION: %s\r\nMESSAGE: %s\r\n\r\n", strrchr(module, '\\') + 1, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, caption, msg);
        else
            fprintf(f, "%s (%i-%02i-%02i %02i:%02i) : %s\r\n", strrchr(module, '\\') + 1, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, msg);
        fclose(f);
    }
#endif
}

int MessageBoxDef(const char *text, const char *notLoggedText, const char *caption, UINT type)
{
    return MessageBoxA(nullptr, notLoggedText[0] ? tmp_str_c(text).append(notLoggedText).cstr() : text, caption, type | MB_TASKMODAL | MB_TOPMOST);
}

inline int LoggedMessageBox(const str_c &text, const char *notLoggedText, const char *caption, UINT type)
{
    LogMessage(nullptr, caption, text);
    return MessageBoxDef(text, notLoggedText, caption, type);
}


bool Warning(const char *s, ...)
{
    char str[4000];

    static std::map<str_c, bool> messages;

    va_list args;
    va_start(args, s);
    vsprintf_s(str, LENGTH(str), s, args);
    va_end(args);

    str_c msg = str;
    bool result = false;
    if (messages.find(msg) == messages.end())
        switch (LoggedMessageBox(msg, "\n\nShow the same messages?", "Warning", MB_YESNOCANCEL))
    {
        case IDCANCEL:
            result = true;
            break;
        case IDNO:
            messages[msg] = true;
            break;
    }
    return result;
}

void Error(const char *s, ...)
{
    char str[4000];

    va_list args;
    va_start(args, s);
    vsprintf_s(str, LENGTH(str), s, args);
    va_end(args);

    LogMessage(nullptr, "Error", str);
    if (MessageBoxDef(str, "", "Error", IsDebuggerPresent() ? MB_OKCANCEL : MB_OK) == IDCANCEL)
        __debugbreak();
}

void Log(const char *s, ...)
{
    char str[4000];

    va_list args;
    va_start(args, s);
    vsprintf_s(str, LENGTH(str), s, args);
    va_end(args);

    LogMessage(nullptr ,nullptr, str);
}

void LogToFile(const char *fn, const char *s, ...)
{
    char str[4000];

    va_list args;
    va_start(args, s);
    vsprintf_s(str, LENGTH(str), s, args);
    va_end(args);

    LogMessage(fn, nullptr, str);
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

int skip_utf8_char( const asptr &utf8, int i )
{
    byte x = (byte)utf8.s[i];
    if (x == 0) return i;
    if (x <= 0x7f) return i+1;
    if ((x & (~0x1f)) == 0xc0) return i+2;
    if ((x & (~0xf)) == 0xe0) return i+3;
    if ((x & (~0x7)) == 0xf0) return i+4;
    if ((x & (~0x3)) == 0xf8) return i+5;
    return i+6;
}

asptr utf8clamp( const asptr &utf8, int maxbytesize )
{
    if( utf8.l > maxbytesize )
    {
        int i0 = 0;
        int i1 = skip_utf8_char( utf8, i0 );
        for(;i1 <= maxbytesize && i1 < utf8.l;)
        {
            i0 = i1;
            i1 = skip_utf8_char( utf8, i0 );
        }
        return utf8.part(i0);
    }
    return utf8;
}

void fifo_stream_c::get_data(int offset, byte *dest, int size)
{
    int ost1 = buf[readbuf].size() - readpos;

    if (offset < ost1)
    {
        int have2copy = min(ost1 - offset, size);
        memcpy(dest, buf[readbuf].data() + readpos + offset, have2copy);
        size -= have2copy;
        dest += have2copy;
        offset += have2copy;
    }
    if (size)
    {
        int offs2 = offset - ost1;
        if (newdata != readbuf && size <= (int)buf[newdata].size() - offs2)
            memcpy(dest, buf[newdata].data() + offs2, size);
    }
}

int fifo_stream_c::read_data(byte *dest, int size)
{
    if (size < ((int)buf[readbuf].size() - readpos))
    {
        if (dest) memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        int szleft = buf[readbuf].size() - readpos;
        if (dest) memcpy(dest, buf[readbuf].data() + readpos, szleft);
        size -= szleft;
        buf[readbuf].clear(); // this buf fully extracted
        newdata = readbuf; // now it is for new data
        readbuf ^= 1; readpos = 0; // and read will be from other one
        if (size)
        {
            if (buf[readbuf].size()) // something in buf - extract it
                return szleft + read_data(dest ? (dest + szleft) : nullptr, size);
            newdata = readbuf = 0; // both bufs are empty, but data still required
        }
        return szleft;
    }
}

wstr_c get_exe_full_name()
{
    wstr_c wd;
    wd.set_length(2048 - 8);
    int len = GetModuleFileNameW(nullptr, wd.str(), 2048 - 8);
    wd.set_length(len);

    if (wd.get_char(0) == '\"')
    {
        int s = wd.find_pos(1, '\"');
        wd.set_length(s);
    }

    wd.trim_right();
    wd.trim_right('\"');
    wd.trim_left('\"');

    return wd;
}

#ifdef _DEBUG
delta_time_profiler_s::delta_time_profiler_s(int n):n(n)
{
    entries = new entry_s[ n ];
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    notfreq = 1000.0 / (double)freq.QuadPart;
    QueryPerformanceCounter(&prev);
}
delta_time_profiler_s::~delta_time_profiler_s()
{
    delete[] entries;
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
