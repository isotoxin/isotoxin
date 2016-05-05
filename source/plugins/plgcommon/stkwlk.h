/**********************************************************************
 * 
 * StackWalker.h
 *
 *
 * History:
 *  2005-07-27   v1    - First public release on http://www.codeproject.com/
 *  (for additional changes see History in 'StackWalker.cpp'!
 *
 **********************************************************************/
// #pragma once is supported starting with _MCS_VER 1000, 
// so we need not to check the version (because we only support _MSC_VER >= 1100)!
#pragma once

#ifndef _STACKWALKER_
#define _STACKWALKER_

// special defines for VC5/6 (if no actual PSDK is installed):
#if _MSC_VER < 1300
typedef unsigned __int64 DWORD64, *PDWORD64;
#if defined(_WIN64)
typedef unsigned __int64 SIZE_T, *PSIZE_T;
#else
typedef unsigned long SIZE_T, *PSIZE_T;
#endif
#endif  // _MSC_VER < 1300

// If VC7 and later, then use the shipped 'dbghelp.h'-file
#if _MSC_VER >= 1300
#include <dbghelp.h>
#else
// inline the important dbghelp.h-declarations...
typedef enum {
	SymNone = 0,
	SymCoff,
	SymCv,
	SymPdb,
	SymExport,
	SymDeferred,
	SymSym,
	SymDia,
	SymVirtual,
	NumSymTypes
} SYM_TYPE;
typedef struct _IMAGEHLP_LINE64 {
	DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_LINE64)
	PVOID                       Key;                    // internal
	DWORD                       LineNumber;             // line number in file
	PCHAR                       FileName;               // full filename
	DWORD64                     Address;                // first instruction of line
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;
typedef struct _IMAGEHLP_MODULE64 {
	DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
	DWORD64                     BaseOfImage;            // base load address of module
	DWORD                       ImageSize;              // virtual size of the loaded module
	DWORD                       TimeDateStamp;          // date/time stamp from pe header
	DWORD                       CheckSum;               // checksum from the pe header
	DWORD                       NumSyms;                // number of symbols in the symbol table
	SYM_TYPE                    SymType;                // type of symbols loaded
	CHAR                        ModuleName[32];         // module name
	CHAR                        ImageName[256];         // image name
	CHAR                        LoadedImageName[256];   // symbol file name
} IMAGEHLP_MODULE64, *PIMAGEHLP_MODULE64;
typedef struct _IMAGEHLP_SYMBOL64 {
	DWORD                       SizeOfStruct;           // set to sizeof(IMAGEHLP_SYMBOL64)
	DWORD64                     Address;                // virtual address including dll base address
	DWORD                       Size;                   // estimated size of symbol, can be zero
	DWORD                       Flags;                  // info about the symbols, see the SYMF defines
	DWORD                       MaxNameLength;          // maximum size of symbol name in 'Name'
	CHAR                        Name[1];                // symbol name (null terminated string)
} IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;
typedef enum {
	AddrMode1616,
	AddrMode1632,
	AddrModeReal,
	AddrModeFlat
} ADDRESS_MODE;
typedef struct _tagADDRESS64 {
	DWORD64       Offset;
	WORD          Segment;
	ADDRESS_MODE  Mode;
} ADDRESS64, *LPADDRESS64;
typedef struct _KDHELP64 {
	DWORD64   Thread;
	DWORD   ThCallbackStack;
	DWORD   ThCallbackBStore;
	DWORD   NextCallback;
	DWORD   FramePointer;
	DWORD64   KiCallUserMode;
	DWORD64   KeUserCallbackDispatcher;
	DWORD64   SystemRangeStart;
	DWORD64  Reserved[8];
} KDHELP64, *PKDHELP64;
typedef struct _tagSTACKFRAME64 {
	ADDRESS64   AddrPC;               // program counter
	ADDRESS64   AddrReturn;           // return address
	ADDRESS64   AddrFrame;            // frame pointer
	ADDRESS64   AddrStack;            // stack pointer
	ADDRESS64   AddrBStore;           // backing store pointer
	PVOID       FuncTableEntry;       // pointer to pdata/fpo or NULL
	DWORD64     Params[4];            // possible arguments to the function
	BOOL        Far;                  // WOW far call
	BOOL        Virtual;              // is this a virtual frame?
	DWORD64     Reserved[3];
	KDHELP64    KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;
typedef
BOOL
(__stdcall *PREAD_PROCESS_MEMORY_ROUTINE64)(
	HANDLE      hProcess,
	DWORD64     qwBaseAddress,
	PVOID       lpBuffer,
	DWORD       nSize,
	LPDWORD     lpNumberOfBytesRead
	);
typedef
PVOID
(__stdcall *PFUNCTION_TABLE_ACCESS_ROUTINE64)(
	HANDLE  hProcess,
	DWORD64 AddrBase
	);
typedef
DWORD64
(__stdcall *PGET_MODULE_BASE_ROUTINE64)(
										HANDLE  hProcess,
										DWORD64 Address
										);
typedef
DWORD64
(__stdcall *PTRANSLATE_ADDRESS_ROUTINE64)(
	HANDLE    hProcess,
	HANDLE    hThread,
	LPADDRESS64 lpaddr
	);
#define SYMOPT_CASE_INSENSITIVE         0x00000001
#define SYMOPT_UNDNAME                  0x00000002
#define SYMOPT_DEFERRED_LOADS           0x00000004
#define SYMOPT_NO_CPP                   0x00000008
#define SYMOPT_LOAD_LINES               0x00000010
#define SYMOPT_OMAP_FIND_NEAREST        0x00000020
#define SYMOPT_LOAD_ANYTHING            0x00000040
#define SYMOPT_IGNORE_CVREC             0x00000080
#define SYMOPT_NO_UNQUALIFIED_LOADS     0x00000100
#define SYMOPT_FAIL_CRITICAL_ERRORS     0x00000200
#define SYMOPT_EXACT_SYMBOLS            0x00000400
#define SYMOPT_ALLOW_ABSOLUTE_SYMBOLS   0x00000800
#define SYMOPT_IGNORE_NT_SYMPATH        0x00001000
#define SYMOPT_INCLUDE_32BIT_MODULES    0x00002000
#define SYMOPT_PUBLICS_ONLY             0x00004000
#define SYMOPT_NO_PUBLICS               0x00008000
#define SYMOPT_AUTO_PUBLICS             0x00010000
#define SYMOPT_NO_IMAGE_SEARCH          0x00020000
#define SYMOPT_SECURE                   0x00040000
#define SYMOPT_DEBUG                    0x80000000
#define UNDNAME_COMPLETE                 (0x0000)  // Enable full undecoration
#define UNDNAME_NAME_ONLY                (0x1000)  // Crack only the name for primary declaration;
#endif  // _MSC_VER < 1300

// Some missing defines (for VC5/6):
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif  


// secure-CRT_functions are only available starting with VC8
#if _MSC_VER < 1400
#define strncpy_s strcpy
#define strncat_s(dst, len, src) strcat(dst, src)
#define _snprintf_s _snprintf
#define _tcscat_s _tcscat
#endif

//////////////////////////////////////////////////////////////////////////////////

class StackWalkerInternal;  // forward
class StackWalker
{
public:
  typedef enum StackWalkOptions
  {
	// No addition info will be retrived 
	// (only the address is available)
	RetrieveNone = 0,
    
	// Try to get the symbol-name
	RetrieveSymbol = 1,
    
	// Try to get the line for this symbol
	RetrieveLine = 2,
    
	// Try to retrieve the module-infos
	RetrieveModuleInfo = 4,
    
	// Also retrieve the version for the DLL/EXE
	RetrieveFileVersion = 8,
    
	// Contains all the abouve
	RetrieveVerbose = 0xF,
    
	// Generate a "good" symbol-search-path
	SymBuildPath = 0x10,
    
	// Also use the public Microsoft-Symbol-Server
	SymUseSymSrv = 0x20,
    
	// Contains all the abouve "Sym"-options
	SymAll = 0x30,
    
	// Contains all options (default)
	OptionsAll = 0x3F
  } StackWalkOptions;

  StackWalker(
	int options = OptionsAll, // 'int' is by design, to combine the enum-flags
	LPCSTR szSymPath = NULL, 
	DWORD dwProcessId = GetCurrentProcessId(), 
	HANDLE hProcess = GetCurrentProcess()
	);
  StackWalker(DWORD dwProcessId, HANDLE hProcess);
  virtual ~StackWalker();

  typedef BOOL (__stdcall *PReadProcessMemoryRoutine)(
	HANDLE      hProcess,
	DWORD64     qwBaseAddress,
	PVOID       lpBuffer,
	DWORD       nSize,
	LPDWORD     lpNumberOfBytesRead,
	LPVOID      pUserData  // optional data, which was passed in "ShowCallstack"
	);

  BOOL LoadModules();

  void TraceRegisters(const HANDLE thread, const CONTEXT* pCxt)const;

  BOOL ShowCallstack(
	HANDLE hThread = GetCurrentThread(), 
	const CONTEXT *context = NULL
	)const;

   enum { STACKWALK_MAX_NAMELEN = 2048 }; // max name length for found symbols

  // Entry for each Callstack-Entry
  typedef struct CallstackEntry
  {
    DWORD64 offset;  // if 0, we have no valid entry
	WORD    segment;
    CHAR name[STACKWALK_MAX_NAMELEN];
    CHAR undName[STACKWALK_MAX_NAMELEN];
    CHAR undFullName[STACKWALK_MAX_NAMELEN];
    DWORD64 offsetFromSmybol;
    DWORD offsetFromLine;
    DWORD lineNumber;
    CHAR lineFileName[STACKWALK_MAX_NAMELEN];
    DWORD symType;
    LPCSTR symTypeString;
    CHAR moduleName[STACKWALK_MAX_NAMELEN];
    DWORD64 baseOfImage;
    CHAR loadedImageName[STACKWALK_MAX_NAMELEN];
	CHAR* loadedImageNamep;
  } CallstackEntry;

  enum CallstackEntryType {firstEntry, nextEntry, lastEntry};

  virtual void OnSymInit(LPCSTR szSearchPath, DWORD symOptions, LPCSTR szUserName);
  virtual void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion);
  virtual void OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr);
  
  virtual void OnOSVersion(DWORD dwMajorVersion, DWORD dwMinorVersion, DWORD dwBuildNumber, LPCSTR szCSDVersion, WORD wServicePackMajor, WORD wServicePackMinor, WORD wSuiteMask, BYTE  wProductType)const;
  virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry, STACKFRAME64 &stackInfo)const;  
  virtual void OnOutput(LPCSTR szText, size_t len)const;
  void OnOutput1(LPCSTR szText)const { OnOutput(szText, strlen(szText)); }

  StackWalkerInternal *m_sw;
  HANDLE m_hProcess;
  DWORD m_dwProcessId;
  BOOL m_modulesLoaded;
  LPSTR m_szSymPath;

  int m_options;

  static BOOL __stdcall myReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);

  friend StackWalkerInternal;

  HMODULE DBGDLL()const;
};

extern CHAR outBuffer[StackWalker::STACKWALK_MAX_NAMELEN];

#endif //#ifndef _STACKWALKER_
