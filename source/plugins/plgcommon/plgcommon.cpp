#include "stdafx.h"

#define LENGTH(a) (sizeof(a)/sizeof(a[0]))

void LogMessage(const char *caption, const char *msg)
{
#ifdef _DEBUG
    FILE *f = nullptr;
    fopen_s(&f, "plghost.log", "ab");
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
    LogMessage(caption, text);
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

    LogMessage("Error", str);
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

    LogMessage(nullptr, str);
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



int fifo_stream_c::read_data(byte *dest, int size)
{
    if (size < ((int)buf[readbuf].size() - readpos))
    {
        memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        int szleft = buf[readbuf].size() - readpos;
        memcpy(dest, buf[readbuf].data() + readpos, szleft);
        size -= szleft;
        buf[readbuf].clear(); // this buf fully extracted
        newdata = readbuf; // now it is for new data
        readbuf ^= 1; readpos = 0; // and read will be from other one
        if (size)
        {
            if (buf[readbuf].size()) // something in buf - extract it
                return szleft + read_data(dest + szleft, size);
            newdata = readbuf = 0; // both bufs are empty, but data still required
        }
        return szleft;
    }
}





