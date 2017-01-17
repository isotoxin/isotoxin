#pragma once
#include "stkwlk.h"

typedef BOOL (WINAPI * MINIDUMPWRITEDUMP)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

typedef void(*GET_CALLSTACK_PROC)(std::string&);


class exception_operator_c : public StackWalker
{
private:
    spinlock::long3264 lock = 0;
    MINIDUMP_TYPE dump_type;
	mutable std::sstr_t<32768> output;

	static exception_operator_c self;
	exception_operator_c();
	void trace_info(EXCEPTION_POINTERS* pExp);
    static LONG WINAPI DUMP(HANDLE hFile, MINIDUMPWRITEDUMP pDump, EXCEPTION_POINTERS* pExp);

protected:
	virtual LONG TraceFinal(EXCEPTION_POINTERS* pExp);
	void OnOutput(LPCSTR szText, int len)const;

public:
	static LONG WINAPI exception_filter(EXCEPTION_POINTERS* pExp);
    static void WINAPI show_callstack(HANDLE hThread, const char* name);

    static void create_dump(EXCEPTION_POINTERS* pExp=nullptr, bool needExcept=true);
    static void set_dump_type(MINIDUMP_TYPE minidumpType)
    {
        self.dump_type = minidumpType;
    }

    static void set_unhandled_exception_filter()
    {
        ::SetUnhandledExceptionFilter(&exception_filter);
    }
    static std::swstr_t<MAX_PATH> dump_filename;
};

#define EXCEPTIONFILTER() exception_operator_c::exception_filter(GetExceptionInformation())
#define SHOW_CALL_STACK() exception_operator_c::show_callstack(GetCurrentThread(), "CallStack")
