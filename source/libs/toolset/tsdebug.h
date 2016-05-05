#pragma once

#include "tsstreamstr.h"

namespace ts
{
#ifdef _CRASH_HANDLER
void Log(const char *s, ...);
#endif

enum smb_e
{
    SMB_OK,
    SMB_OKCANCEL,
    SMB_YESNOCANCEL,
    SMB_YESNO,
    SMB_YESNO_ERROR,
    SMB_OK_ERROR,
};

enum smbr_e
{
    SMBR_UNKNOWN,

    SMBR_OK,
    SMBR_YES,
    SMBR_NO,
    SMBR_CANCEL,
};
smbr_e TSCALL sys_mb( const wchar *caption, const wchar *text, smb_e options );

}

#ifdef _FINAL
#define WARNING(...) do ; while ((1, false))
#define ASSERT(...) (1, true)
#define ASSERTO(...) (1, true)
#define CHECK(expr,...) (expr)
#define TOOOFTEN(ms) (1, false)
#define FORBIDDEN() do ; while ((1, false))
#undef ERROR
#define ERROR(...) do ; while ((1, false))
#define DMSG(expr, ...) (1,true)
#define RECURSIVE_ALERT()
#define LOG(...) (1, false)
#define BOK(...) (1, false)
#define CBOK(...) (1, false)
#else

#ifdef _DEBUG_OPTIMIZED
#define SMART_DEBUG_BREAK (ts::sys_is_debugger_present() ? DEBUG_BREAK(), false : false)
#else
#define SMART_DEBUG_BREAK DEBUG_BREAK() // always break in debug
#endif

#define BOK( sk ) if ( ts::master().is_key_pressed(sk) ) SMART_DEBUG_BREAK;
#define CBOK( sk, ... ) if ( ts::master().is_key_pressed(sk) && (__VA_ARGS__) ) SMART_DEBUG_BREAK;

namespace ts 
{

    bool sys_is_debugger_present();


	bool AssertFailed(const char *file, int line, const char *s, ...);
	INLINE bool AssertFailed(const char *file, int line) {return AssertFailed(file, line, "");}

	void LogMessage(const char *caption, const char *msg);

	bool Warning(const char *s, ...);
	void Error(const char *s, ...);
    void Log(const char *s, ...);

#define TM(x) ts::tmcalc_c UNIQID(TM)(#x);

	class tmcalc_c
	{
		uint64 m_timestamp;
		const char *m_tag;
	public:
        tmcalc_c( const char *tag );
		~tmcalc_c();
	};

}

//-V:ASSERT:807
//-V:CHECK:807
//-V:LOG:807

#define TOOOFTEN(ms) ts::__tmbreak<__COUNTER__>(ms)

#define ASSERTO(expr,...) NOWARNING(4800, ((expr) || (ts::AssertFailed(__FILE__, __LINE__, ##__VA_ARGS__) ? (SMART_DEBUG_BREAK, false) : false))) // (...) need to make possible syntax: ASSERT(expr, "Message")
#define CHECK ASSERT // in debug CHECK same as ASSERT
#define WARNING(...) (ts::Warning(__VA_ARGS__) ? SMART_DEBUG_BREAK, false : false)

#undef ERROR
#define ERROR(...) ts::Error(__VA_ARGS__)

#define ASSERT(expr, ...) ASSERTO(expr, (ts::streamstr< ts::sstr_t<1024> >(true) << ""  __VA_ARGS__).buffer().cstr())
#define FORBIDDEN() ERROR("bad execution")
#define RECURSIVE_ALERT() static int __r = 0; struct rcheck { int *r; rcheck(int *r):r(r) {++ (*r); ASSERT(*r == 1);} ~rcheck() {-- (*r);} } __rcheck(&__r)
#define LOG(...) ts::LogMessage(nullptr, (ts::streamstr< ts::sstr_t<1024> >(true) << "" __VA_ARGS__).buffer().cstr())

namespace ts
{
#ifdef __DMSG__
void TSCALL dmsg(const char *str);
#define DMSG(...) ts::dmsg( (ts::streamstr< ts::sstr_t<1024> >(true) << "" __VA_ARGS__).buffer().cstr() )
#else
INLINE void TSCALL dmsg(const char *str) {}
#define DMSG(expr, ...) (1,true)
#endif

#ifdef _DEBUG
struct delta_time_profiler_s
{
    double notfreq;
    uint64 prev;
    struct entry_s
    {
        int id;
        float deltams;
    };
    entry_s *entries = nullptr;
    int index = 0;
    int n = 0;
    delta_time_profiler_s(int n);
    ~delta_time_profiler_s();
    void operator()(int id);
};

#define DELTA_TIME_PROFILER(name, n) ts::delta_time_profiler_s name(n)
#define DELTA_TIME_CHECKPOINT(name) name(__LINE__)
#else
#define DELTA_TIME_PROFILER(n, nn) 
#define DELTA_TIME_CHECKPOINT(n) 
#endif

}

#endif