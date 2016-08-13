#include "stdafx.h"

unsigned int g_logging_flags = 0;
unsigned int g_telemetry_flags = 0;
HINSTANCE g_module = nullptr;

#define LENGTH(a) (sizeof(a)/sizeof(a[0]))

void LogMessage(const char *fn, const char *caption, const char *msg)
{
#if defined _DEBUG || defined _DEBUG_OPTIMIZED
    FILE *f = nullptr;
    fopen_s(&f, fn ? fn : "plghost.log", "ab");
    if (f)
    {
        char module[MAX_PATH];
        GetModuleFileNameA(g_module, module, LENGTH(module));
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

void MaskLog(unsigned mask, const char *s, ...)
{
    unsigned effmask = (g_logging_flags & mask);
    if (0 == effmask) return;

    FILE *f = nullptr;
    fopen_s(&f, "plghost.log", "ab");
    if (f)
    {
        sstr_t<4050> str;

        str.set_length( GetModuleFileNameA(g_module, str.str(), MAX_PATH) );
        str.cut( 0, 1 + str.find_last_pos_of(CONSTASTR("\\/")) );
        str.append_char(' ');
        str.append_as_uint( timeGetTime() );
        str.append_char(' ');

        if (0 != (effmask & LFLS_CLOSE))
            str.append( CONSTASTR("CLOSE: ") );

        if (0 != (effmask & LFLS_ESTBLSH))
            str.append(CONSTASTR("ESTBLSH: "));

        va_list args;
        va_start(args, s);
        int l = vsprintf_s(str.str() + str.get_length(), str.get_capacity() - str.get_length(), s, args);
        va_end(args);
        str.set_length( str.get_length() + l );
        str.append( CONSTASTR("\r\n") );

        fwrite(str.cstr(), str.get_length(), 1, f);
        fclose(f);
    }
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

void fifo_stream_c::get_data(aint offset, byte *dest, aint size)
{
    aint ost1 = buf[readbuf].size() - readpos;

    if (offset < ost1)
    {
        aint have2copy = min(ost1 - offset, size);
        memcpy(dest, buf[readbuf].data() + readpos + offset, have2copy);
        size -= have2copy;
        dest += have2copy;
        offset += have2copy;
    }
    if (size)
    {
        aint offs2 = offset - ost1;
        if (newdata != readbuf && size <= (aint)buf[newdata].size() - offs2)
            memcpy(dest, buf[newdata].data() + offs2, size);
    }
}

aint fifo_stream_c::read_data(byte *dest, aint size)
{
    if (size < ((aint)buf[readbuf].size() - readpos))
    {
        if (dest) memcpy(dest, buf[readbuf].data() + readpos, size);
        readpos += (int)size;
        if (newdata == readbuf) newdata ^= 1;
        return size;
    }
    else
    {
        aint szleft = buf[readbuf].size() - readpos;
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

// cpu detector
namespace cpu_detect
{
#if _M_X64
#if defined(_MSC_VER) && _MSC_VER > 1500
#pragma intrinsic(__cpuidex)
#define cpuid(func, func2, a, b, c, d) do {\
    int regs[4];\
    __cpuidex(regs, func, func2); \
    a = regs[0];  b = regs[1];  c = regs[2];  d = regs[3];\
      } while(0)
#else
    void __cpuid(int CPUInfo[4], int info_type);
#pragma intrinsic(__cpuid)
#define cpuid(func, func2, a, b, c, d) do {\
    int regs[4];\
    __cpuid(regs, func); \
    a = regs[0];  b = regs[1];  c = regs[2];  d = regs[3];\
          } while (0)
#endif
#else
#define cpuid(func, func2, a, b, c, d)\
  __asm mov eax, func\
  __asm mov ecx, func2\
  __asm cpuid\
  __asm mov a, eax\
  __asm mov b, ebx\
  __asm mov c, ecx\
  __asm mov d, edx
#endif

#if !defined(__native_client__) && (defined(__i386__) || defined(__x86_64__))
    static INLINE uint64_t xgetbv(void) {
        const uint32_t ecx = 0;
        uint32_t eax, edx;
        // Use the raw opcode for xgetbv for compatibility with older toolchains.
        __asm__ volatile (
            ".byte 0x0f, 0x01, 0xd0\n"
            : "=a"(eax), "=d"(edx) : "c" (ecx));
        return ((uint64_t)edx << 32) | eax;
    }
#elif (defined(_M_X64) || defined(_M_IX86)) && \
      defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 160040219  // >= VS2010 SP1
#include <immintrin.h>
#define xgetbv() _xgetbv(0)
#elif defined(_MSC_VER) && defined(_M_IX86)
    static INLINE uint64_t xgetbv(void) {
        uint32_t eax_, edx_;
        __asm {
            xor ecx, ecx  // ecx = 0
            // Use the raw opcode for xgetbv for compatibility with older toolchains.
            __asm _emit 0x0f __asm _emit 0x01 __asm _emit 0xd0
            mov eax_, eax
            mov edx_, edx
        }
        return ((uint64_t)edx_ << 32) | eax_;
    }
#else
#define xgetbv() 0U  // no AVX for older x64 or unrecognized toolchains.
#endif

#define SETBIT(x) ((1U)<<(x))

u32 detect_cpu_caps()
{
    unsigned int max_cpuid_val, reg_eax, reg_ebx, reg_ecx, reg_edx;

    /* Ensure that the CPUID instruction supports extended features */
    cpuid(0, 0, max_cpuid_val, reg_ebx, reg_ecx, reg_edx);

    if (max_cpuid_val < 1)
        return 0;

    u32 flags = 0;

    /* Get the standard feature flags */
    cpuid(1, 0, reg_eax, reg_ebx, reg_ecx, reg_edx);

    if (reg_edx & SETBIT(23)) flags |= CPU_MMX;
    if (reg_edx & SETBIT(25)) flags |= CPU_SSE; /* aka xmm */
    if (reg_edx & SETBIT(26)) flags |= CPU_SSE2; /* aka wmt */
    if (reg_ecx & SETBIT(0)) flags |= CPU_SSE3;
    if (reg_ecx & SETBIT(9)) flags |= CPU_SSSE3;
    if (reg_ecx & SETBIT(19)) flags |= CPU_SSE41;

    // bits 27 (OSXSAVE) & 28 (256-bit AVX)
    if ((reg_ecx & (SETBIT(27) | SETBIT(28))) == (SETBIT(27) | SETBIT(28))) {
        if ((xgetbv() & 0x6) == 0x6)
        {
            flags |= CPU_AVX;

            if (max_cpuid_val >= 7)
            {
                /* Get the leaf 7 feature flags. Needed to check for AVX2 support */
                cpuid(7, 0, reg_eax, reg_ebx, reg_ecx, reg_edx);

                if (reg_ebx & SETBIT(5)) flags |= CPU_AVX2;
            }
        }
    }


    return flags;
}
}



extern "C"
{
    u32 g_cpu_caps = cpu_detect::detect_cpu_caps();
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

static spinlock::long3264 dlmalloc_spinlock = 0;

#define PREACTION(M)  (spinlock::simple_lock(dlmalloc_spinlock), 0)
#define POSTACTION(M) spinlock::simple_unlock(dlmalloc_spinlock)

extern "C"
{
#include "dlmalloc/dlmalloc.c"
}

#ifdef _WIN32
// disable VS2015 telemetry (Th you, kidding? Fuck spies.)
extern "C"
{
    void _cdecl __vcrt_initialize_telemetry_provider() {}
    void _cdecl __telemetry_main_invoke_trigger() {}
    void _cdecl __telemetry_main_return_trigger() {}
    void _cdecl __vcrt_uninitialize_telemetry_provider() {}
};
#endif
