#pragma once

#include "tsstreamstr.h"

namespace ts
{
extern HWND g_main_window;
#ifdef _CRASH_HANDLER
void Log(const char *s, ...);
#endif
}

#ifdef _FINAL
#define WARNING(...) do ; while ((1, false))
#define ASSERT(...) (1, true)//чтобы не было предупреждения C4127
#define ASSERTO(...) (1, true)//чтобы не было предупреждения C4127
#define CHECK(expr,...) (expr)//в релизе проверка тоже должна отработать
#define TOOOFTEN(ms) (1, false)
#define FORBIDDEN() do ; while ((1, false))
#undef ERROR
#define ERROR(...) do ; while ((1, false))
#define DMSG(expr, ...) (1,true)
#else

#ifdef _DEBUG_OPTIMIZED
#define SMART_DEBUG_BREAK (IsDebuggerPresent() ? __debugbreak(), false : false) //чтобы в _DEBUG_OPTIMIZED версии брейк был только из под отладчика
#else
#define SMART_DEBUG_BREAK __debugbreak() //в этом случае брейк срабатывает всегда, т.к. простой дебаг используют только программисты
#endif

namespace ts 
{
	bool AssertFailed(const char *file, int line, const char *s, ...);
	INLINE bool AssertFailed(const char *file, int line) {return AssertFailed(file, line, "");}

	extern int (*MessageBoxOverride)(const char *text, const char *notLoggedText, const char *caption, UINT type);
	void LogMessage(const char *caption, const char *msg);
	INLINE int LoggedMessageBox(const char *text, const char *caption, UINT type) { LogMessage(caption, text); return MessageBoxOverride(text, "", caption, type); }

	bool Warning(const char *s, ...);
	void Error(const char *s, ...);
    void Log(const char *s, ...);

#define TM(x) ts::tmcalc_c UNIQID(TM)(#x);

	class tmcalc_c
	{
		LARGE_INTEGER   m_timestamp;
		const char     *m_tag;
	public:
		tmcalc_c( const char *tag ):m_tag(tag)
		{
			QueryPerformanceCounter( &m_timestamp );
		};
		~tmcalc_c();
	};

    template<int dummynum> INLINE bool __tmbreak( int ms )
    {
        static LARGE_INTEGER   prevtimestamp = {dummynum};
        static bool initialized = false;
        bool r = false;
        LARGE_INTEGER   timestamp = {dummynum};
        QueryPerformanceCounter(initialized ? &timestamp : &prevtimestamp);
        if (initialized)
        {
            LARGE_INTEGER   frq;
            QueryPerformanceFrequency(&frq);
            __int64 takts = timestamp.QuadPart - prevtimestamp.QuadPart;
            int curms = lround(1000.0 * double(takts) / double(frq.QuadPart));
            if (curms < ms)
            {
                r = true;
            }
            prevtimestamp = timestamp;
        } 
        initialized = true;
        return r;
    };

}

#define TOOOFTEN(ms) ts::__tmbreak<__COUNTER__>(ms)

#define ASSERTO(expr,...) NOWARNING(4800, ((expr) || (ts::AssertFailed(__FILE__, __LINE__, __VA_ARGS__) ? SMART_DEBUG_BREAK, false : false))) // (...) нужно чтобы можно было писать ASSERT(expr, "Message")
#define CHECK ASSERT//в дебаг-версии CHECK работает также как и ASSERT
#define WARNING(...) (ts::Warning(__VA_ARGS__) ? SMART_DEBUG_BREAK, false : false)

#undef ERROR
#define ERROR(...) ts::Error(__VA_ARGS__)

#define ASSERT(expr, ...) ASSERTO(expr, (ts::streamstr< ts::sstr_t<1024> >() << ""__VA_ARGS__).buffer().cstr())
#define FORBIDDEN() ERROR("bad execution")

namespace ts
{
#ifdef __DMSG__
void _cdecl dmsg(const char *str);
#define DMSG(...) ts::dmsg( (ts::streamstr< ts::sstr_t<1024> >() << ""__VA_ARGS__).buffer().cstr() )
#else
INLINE void _cdecl dmsg(const char *str) {}
#define DMSG(expr, ...) (1,true)
#endif
}

#endif