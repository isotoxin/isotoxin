/**********************************************************************
 *
 * StackWalker.cpp
 *
 *
 * History:
 *  2005-07-27   v1    - First public release on http://www.codeproject.com/
 *                       http://www.codeproject.com/threads/StackWalker.asp
 *  2005-07-28   v2    - Changed the params of the constructor and ShowCallstack
 *                       (to simplify the usage)
 *  2005-08-01   v3    - Changed to use 'CONTEXT_FULL' instead of CONTEXT_ALL
 *                       (should also be enough)
 *                     - Changed to compile correctly with the PSDK of VC7.0
 *                       (GetFileVersionInfoSizeA and GetFileVersionInfoA is wrongly defined:
 *                        it uses LPSTR instead of LPCSTR as first paremeter)
 *                     - Added declarations to support VC5/6 without using 'dbghelp.h'
 *                     - Added a 'pUserData' member to the ShowCallstack function and the
 *                       PReadProcessMemoryRoutine declaration (to pass some user-defined data,
 *                       which can be used in the readMemoryFunction-callback)
 *  2005-08-02   v4    - OnSymInit now also outputs the OS-Version by default
 *                     - Added example for doing an exception-callstack-walking in main.cpp
 *                       (thanks to owillebo: http://www.codeproject.com/script/profile/whos_who.asp?id=536268)
 *  2005-08-05   v5    - Removed most Lint (http://www.gimpel.com/) errors... thanks to Okko Willeboordse!
 *
 **********************************************************************/

#include "toolset.h"
#include "platform.h"

#if (!defined _FINAL || defined _CRASH_HANDLER) && defined _WIN32

#ifdef _WIN32
#include "stkwlk.h"

// TODO - move to /src/win

#include <tchar.h>
#include <stdio.h>


#pragma comment(lib, "version.lib")  // for "VerQueryValue"

// Normally it should be enough to use 'CONTEXT_FULL' (better would be 'CONTEXT_ALL')
#define USED_CONTEXT_FLAGS CONTEXT_FULL

CHAR outBuffer[StackWalker::STACKWALK_MAX_NAMELEN];

static CHAR tmpData[4096];

class StackWalkerInternal
{
public:
  StackWalkerInternal(StackWalker *parent, HANDLE hProcess)
  {
    m_parent = parent;
    m_hDbhHelp = nullptr;
    pSC = nullptr;
    m_hProcess = hProcess;
    m_szSymPath = nullptr;
    pSFTA = nullptr;
    pSGLFA = nullptr;
    pSGMB = nullptr;
    pSGMI = nullptr;
    pSGO = nullptr;
    pSGSFA = nullptr;
    pSI = nullptr;
    pSLM = nullptr;
    pSSO = nullptr;
    pSW = nullptr;
    pUDSN = nullptr;
    pSGSP = nullptr;
  }
  ~StackWalkerInternal()
  {
    if (pSC != nullptr)
      pSC(m_hProcess);  // SymCleanup
    if (m_hDbhHelp != nullptr)
      FreeLibrary(m_hDbhHelp);
    m_hDbhHelp = nullptr;
    m_parent = nullptr;
    if(m_szSymPath != nullptr)
      free(m_szSymPath);
    m_szSymPath = nullptr;
  }
  BOOL Init(LPCSTR szSymPath)
  {
    if (m_parent == nullptr)
      return FALSE;
    // Dynamically load the Entry-Points for dbghelp.dll:
    // First try to load the newsest one from
    TCHAR szTemp[4096];
    // But before wqe do this, we first check if the ".local" file exists
    if (GetModuleFileName(nullptr, szTemp, 4096) > 0)
    {
      _tcscat_s(szTemp, _T(".local"));
      if (GetFileAttributes(szTemp) == INVALID_FILE_ATTRIBUTES)
      {
        // ".local" file does not exist, so we can try to load the dbghelp.dll from the "Debugging Tools for Windows"
        if (GetEnvironmentVariable(_T("ProgramFiles"), szTemp, 4096) > 0)
        {
          _tcscat_s(szTemp, _T("\\Debugging Tools for Windows\\dbghelp.dll"));
          // now check if the file exists:
          if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
          {
            m_hDbhHelp = LoadLibrary(szTemp);
          }
        }
          // Still not found? Then try to load the 64-Bit version:
        if ( (m_hDbhHelp == nullptr) && (GetEnvironmentVariable(_T("ProgramFiles"), szTemp, 4096) > 0) )
        {
          _tcscat_s(szTemp, _T("\\Debugging Tools for Windows 64-Bit\\dbghelp.dll"));
          if (GetFileAttributes(szTemp) != INVALID_FILE_ATTRIBUTES)
          {
            m_hDbhHelp = LoadLibrary(szTemp);
          }
        }
      }
    }
    if (m_hDbhHelp == nullptr)  // if not already loaded, try to load a default-one
      m_hDbhHelp = LoadLibrary( _T("dbghelp.dll") );
    if (m_hDbhHelp == nullptr)
      return FALSE;
    pSI = (tSI) GetProcAddress(m_hDbhHelp, "SymInitialize" );
    pSC = (tSC) GetProcAddress(m_hDbhHelp, "SymCleanup" );

    pSW = (tSW) GetProcAddress(m_hDbhHelp, "StackWalk64" );
    pSGO = (tSGO) GetProcAddress(m_hDbhHelp, "SymGetOptions" );
    pSSO = (tSSO) GetProcAddress(m_hDbhHelp, "SymSetOptions" );

    pSFTA = (tSFTA) GetProcAddress(m_hDbhHelp, "SymFunctionTableAccess64" );
    pSGLFA = (tSGLFA) GetProcAddress(m_hDbhHelp, "SymGetLineFromAddr64" );
    pSGMB = (tSGMB) GetProcAddress(m_hDbhHelp, "SymGetModuleBase64" );
    pSGMI = (tSGMI) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64" );
    //pSGMI_V3 = (tSGMI_V3) GetProcAddress(m_hDbhHelp, "SymGetModuleInfo64" );
    pSGSFA = (tSGSFA) GetProcAddress(m_hDbhHelp, "SymGetSymFromAddr64" );
    pUDSN = (tUDSN) GetProcAddress(m_hDbhHelp, "UnDecorateSymbolName" );
    pSLM = (tSLM) GetProcAddress(m_hDbhHelp, "SymLoadModule64" );
    pSGSP =(tSGSP) GetProcAddress(m_hDbhHelp, "SymGetSearchPath" );

    if ( pSC == nullptr || pSFTA == nullptr || pSGMB == nullptr || pSGMI == nullptr ||
      pSGO == nullptr || pSGSFA == nullptr || pSI == nullptr || pSSO == nullptr ||
      pSW == nullptr || pUDSN == nullptr || pSLM == nullptr )
    {
      FreeLibrary(m_hDbhHelp);
      m_hDbhHelp = nullptr;
      pSC = nullptr;
      return FALSE;
    }

    // SymInitialize
    if (szSymPath != nullptr)
      m_szSymPath = _strdup(szSymPath);
    if (this->pSI(m_hProcess, m_szSymPath, FALSE) == FALSE)
      this->m_parent->OnDbgHelpErr("SymInitialize", GetLastError(), 0);

    DWORD symOptions = this->pSGO();  // SymGetOptions
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
    //symOptions |= SYMOPT_NO_PROMPTS;
    // SymSetOptions
    symOptions = this->pSSO(symOptions);

    char buf[StackWalker::STACKWALK_MAX_NAMELEN] = {};
    if (this->pSGSP != nullptr)
    {
      if (this->pSGSP(m_hProcess, buf, StackWalker::STACKWALK_MAX_NAMELEN) == FALSE)
        this->m_parent->OnDbgHelpErr("SymGetSearchPath", GetLastError(), 0);
    }
    char szUserName[1024] = {};
    DWORD dwSize = 1024;
    GetUserNameA(szUserName, &dwSize);
    this->m_parent->OnSymInit(buf, symOptions, szUserName);

    return TRUE;
  }

  StackWalker *m_parent;

  HMODULE m_hDbhHelp;
  HANDLE m_hProcess;
  LPSTR m_szSymPath;

/*typedef struct IMAGEHLP_MODULE64_V3 {
    DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64  BaseOfImage;            // base load address of module
    DWORD    ImageSize;              // virtual size of the loaded module
    DWORD    TimeDateStamp;          // date/time stamp from pe header
    DWORD    CheckSum;               // checksum from the pe header
    DWORD    NumSyms;                // number of symbols in the symbol table
    SYM_TYPE SymType;                // type of symbols loaded
    CHAR     ModuleName[32];         // module name
    CHAR     ImageName[256];         // image name
    // new elements: 07-Jun-2002
    CHAR     LoadedImageName[256];   // symbol file name
    CHAR     LoadedPdbName[256];     // pdb file name
    DWORD    CVSig;                  // Signature of the CV record in the debug directories
    CHAR         CVData[MAX_PATH * 3];   // Contents of the CV record
    DWORD    PdbSig;                 // Signature of PDB
    GUID     PdbSig70;               // Signature of PDB (VC 7 and up)
    DWORD    PdbAge;                 // DBI age of pdb
    BOOL     PdbUnmatched;           // loaded an unmatched pdb
    BOOL     DbgUnmatched;           // loaded an unmatched dbg
    BOOL     LineNumbers;            // we have line number information
    BOOL     GlobalSymbols;          // we have internal symbol information
    BOOL     TypeInfo;               // we have type information
    // new elements: 17-Dec-2003
    BOOL     SourceIndexed;          // pdb supports source server
    BOOL     Publics;                // contains public symbols
};
*/
struct IMAGEHLP_MODULE64_V2 {
    DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64  BaseOfImage;            // base load address of module
    DWORD    ImageSize;              // virtual size of the loaded module
    DWORD    TimeDateStamp;          // date/time stamp from pe header
    DWORD    CheckSum;               // checksum from the pe header
    DWORD    NumSyms;                // number of symbols in the symbol table
    SYM_TYPE SymType;                // type of symbols loaded
    CHAR     ModuleName[32];         // module name
    CHAR     ImageName[256];         // image name
    CHAR     LoadedImageName[256];   // symbol file name
};


  // SymCleanup()
  typedef BOOL (__stdcall *tSC)( IN HANDLE hProcess );
  tSC pSC;

  // SymFunctionTableAccess64()
  typedef PVOID (__stdcall *tSFTA)( HANDLE hProcess, DWORD64 AddrBase );
  tSFTA pSFTA;

  // SymGetLineFromAddr64()
  typedef BOOL (__stdcall *tSGLFA)( IN HANDLE hProcess, IN DWORD64 dwAddr,
    OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE64 Line );
  tSGLFA pSGLFA;

  // SymGetModuleBase64()
  typedef DWORD64 (__stdcall *tSGMB)( IN HANDLE hProcess, IN DWORD64 dwAddr );
  tSGMB pSGMB;

  // SymGetModuleInfo64()
  typedef BOOL (__stdcall *tSGMI)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT IMAGEHLP_MODULE64_V2 *ModuleInfo );
  tSGMI pSGMI;

//  // SymGetModuleInfo64()
//  typedef BOOL (__stdcall *tSGMI_V3)( IN HANDLE hProcess, IN DWORD64 dwAddr, OUT IMAGEHLP_MODULE64_V3 *ModuleInfo );
//  tSGMI_V3 pSGMI_V3;

  // SymGetOptions()
  typedef DWORD (__stdcall *tSGO)( VOID );
  tSGO pSGO;

  // SymGetSymFromAddr64()
  typedef BOOL (__stdcall *tSGSFA)( IN HANDLE hProcess, IN DWORD64 dwAddr,
    OUT PDWORD64 pdwDisplacement, OUT PIMAGEHLP_SYMBOL64 Symbol );
  tSGSFA pSGSFA;

  // SymInitialize()
  typedef BOOL (__stdcall *tSI)( IN HANDLE hProcess, IN PSTR UserSearchPath, IN BOOL fInvadeProcess );
  tSI pSI;

  // SymLoadModule64()
  typedef DWORD64 (__stdcall *tSLM)( IN HANDLE hProcess, IN HANDLE hFile,
    IN PSTR ImageName, IN PSTR ModuleName, IN DWORD64 BaseOfDll, IN DWORD SizeOfDll );
  tSLM pSLM;

  // SymSetOptions()
  typedef DWORD (__stdcall *tSSO)( IN DWORD SymOptions );
  tSSO pSSO;

  // StackWalk64()
  typedef BOOL (__stdcall *tSW)(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
  tSW pSW;

  // UnDecorateSymbolName()
  typedef DWORD (WINAPI *tUDSN)( PCSTR DecoratedName, PSTR UnDecoratedName,
    DWORD UndecoratedLength, DWORD Flags );
  tUDSN pUDSN;

  typedef BOOL (WINAPI *tSGSP)(HANDLE hProcess, PSTR SearchPath, DWORD SearchPathLength);
  tSGSP pSGSP;


private:
  // **************************************** ToolHelp32 ************************
  #define MAX_MODULE_NAME32 255
  #define TH32CS_SNAPMODULE   0x00000008
  #pragma pack( push, 8 )
  typedef struct tagMODULEENTRY32
  {
      DWORD   dwSize;
      DWORD   th32ModuleID;       // This module
      DWORD   th32ProcessID;      // owning process
      DWORD   GlblcntUsage;       // Global usage count on the module
      DWORD   ProccntUsage;       // Module usage count in th32ProcessID's context
      BYTE  * modBaseAddr;        // Base address of module in th32ProcessID's context
      DWORD   modBaseSize;        // Size in bytes of module starting at modBaseAddr
      HMODULE hModule;            // The hModule of this module in th32ProcessID's context
      char    szModule[MAX_MODULE_NAME32 + 1];
      char    szExePath[MAX_PATH];
  } MODULEENTRY32;
  typedef MODULEENTRY32 *  PMODULEENTRY32;
  typedef MODULEENTRY32 *  LPMODULEENTRY32;
  #pragma pack( pop )

  BOOL GetModuleListTH32(HANDLE hProcess, DWORD pid)
  {
    // CreateToolhelp32Snapshot()
    typedef HANDLE (__stdcall *tCT32S)(DWORD dwFlags, DWORD th32ProcessID);
    // Module32First()
    typedef BOOL (__stdcall *tM32F)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
    // Module32Next()
    typedef BOOL (__stdcall *tM32N)(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

    // try both dlls...
    const TCHAR *dllname[] = { _T("kernel32.dll"), _T("tlhelp32.dll") };
    HINSTANCE hToolhelp = nullptr;
    tCT32S pCT32S = nullptr;
    tM32F pM32F = nullptr;
    tM32N pM32N = nullptr;

    HANDLE hSnap;
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    BOOL keepGoing;
    size_t i;

    for (i = 0; i<(sizeof(dllname) / sizeof(dllname[0])); i++ )
    {
      hToolhelp = LoadLibrary( dllname[i] );
      if (hToolhelp == nullptr)
        continue;
      pCT32S = (tCT32S) GetProcAddress(hToolhelp, "CreateToolhelp32Snapshot");
      pM32F = (tM32F) GetProcAddress(hToolhelp, "Module32First");
      pM32N = (tM32N) GetProcAddress(hToolhelp, "Module32Next");
      if ( (pCT32S != nullptr) && (pM32F != nullptr) && (pM32N != nullptr) )
        break; // found the functions!
      FreeLibrary(hToolhelp);
      hToolhelp = nullptr;
    }

    if (hToolhelp == nullptr)
      return FALSE;

    hSnap = pCT32S( TH32CS_SNAPMODULE, pid );
    if (hSnap == (HANDLE) -1)
      return FALSE;

    keepGoing = !!pM32F( hSnap, &me );
    int cnt = 0;
    while (keepGoing)
    {
      this->LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64) me.modBaseAddr, me.modBaseSize);
      cnt++;
      keepGoing = !!pM32N( hSnap, &me );
    }
    CloseHandle(hSnap);
    FreeLibrary(hToolhelp);
    if (cnt <= 0)
      return FALSE;
    return TRUE;
  }  // GetModuleListTH32

  // **************************************** PSAPI ************************
  typedef struct _MODULEINFO {
      LPVOID lpBaseOfDll;
      DWORD SizeOfImage;
      LPVOID EntryPoint;
  } MODULEINFO, *LPMODULEINFO;

  BOOL GetModuleListPSAPI(HANDLE hProcess)
  {
    // EnumProcessModules()
    typedef BOOL (__stdcall *tEPM)(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded );
    // GetModuleFileNameEx()
    typedef DWORD (__stdcall *tGMFNE)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize );
    // GetModuleBaseName()
    typedef DWORD (__stdcall *tGMBN)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, DWORD nSize );
    // GetModuleInformation()
    typedef BOOL (__stdcall *tGMI)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO pmi, DWORD nSize );

    HINSTANCE hPsapi;
    tEPM pEPM;
    tGMFNE pGMFNE;
    tGMBN pGMBN;
    tGMI pGMI;

    DWORD i;
    //ModuleEntry e;
    DWORD cbNeeded;
    MODULEINFO mi;
    HMODULE *hMods = 0;
    char *tt = nullptr;
    char *tt2 = nullptr;
    const SIZE_T TTBUFLEN = 8096;
    int cnt = 0;

    hPsapi = LoadLibrary( _T("psapi.dll") );
    if (hPsapi == nullptr)
      return FALSE;

    pEPM = (tEPM) GetProcAddress( hPsapi, "EnumProcessModules" );
    pGMFNE = (tGMFNE) GetProcAddress( hPsapi, "GetModuleFileNameExA" );
    pGMBN = (tGMFNE) GetProcAddress( hPsapi, "GetModuleBaseNameA" );
    pGMI = (tGMI) GetProcAddress( hPsapi, "GetModuleInformation" );
    if ( (pEPM == nullptr) || (pGMFNE == nullptr) || (pGMBN == nullptr) || (pGMI == nullptr) )
    {
      // we couldn't find all functions
      FreeLibrary(hPsapi);
      return FALSE;
    }

    hMods = (HMODULE*) malloc(sizeof(HMODULE) * (TTBUFLEN / sizeof HMODULE));
    tt = (char*) malloc(sizeof(char) * TTBUFLEN);
    tt2 = (char*) malloc(sizeof(char) * TTBUFLEN);
    if ( (hMods == nullptr) || (tt == nullptr) || (tt2 == nullptr) )
      goto cleanup;

    if ( ! pEPM( hProcess, hMods, TTBUFLEN, &cbNeeded ) )
    {
      //_ftprintf(fLogFile, _T("%lu: EPM failed, GetLastError = %lu\n"), g_dwShowCount, gle );
      goto cleanup;
    }

    if ( cbNeeded > TTBUFLEN )
    {
      //_ftprintf(fLogFile, _T("%lu: More than %lu module handles. Huh?\n"), g_dwShowCount, lenof( hMods ) );
      goto cleanup;
    }

    for ( i = 0; i < cbNeeded / sizeof hMods[0]; i++ )
    {
      // base address, size
      pGMI(hProcess, hMods[i], &mi, sizeof mi );
      // image file name
      tt[0] = 0;
      pGMFNE(hProcess, hMods[i], tt, TTBUFLEN );
      // module name
      tt2[0] = 0;
      pGMBN(hProcess, hMods[i], tt2, TTBUFLEN );

      DWORD dwRes = this->LoadModule(hProcess, tt, tt2, (DWORD64) mi.lpBaseOfDll, mi.SizeOfImage);
      if (dwRes != ERROR_SUCCESS)
        this->m_parent->OnDbgHelpErr("LoadModule", dwRes, 0);
      cnt++;
    }

  cleanup:
    if (hPsapi != nullptr) FreeLibrary(hPsapi);
    if (tt2 != nullptr) free(tt2);
    if (tt != nullptr) free(tt);
    if (hMods != nullptr) free(hMods);

    return cnt != 0;
  }  // GetModuleListPSAPI

  DWORD LoadModule(HANDLE hProcess, LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size)
  {
    CHAR *szImg = _strdup(img);
    CHAR *szMod = _strdup(mod);
    DWORD result = ERROR_SUCCESS;
    if ( (szImg == nullptr) || (szMod == nullptr) )
      result = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
      if (pSLM(hProcess, 0, szImg, szMod, baseAddr, size) == 0)
        result = GetLastError();
    }
    ULONGLONG fileVersion = 0;
    if ( (m_parent != nullptr) && (szImg != nullptr) )
    {
      // try to retrive the file-version:
//       if ( (this->m_parent->m_options & StackWalker::RetrieveFileVersion) != 0)
//       {
//         VS_FIXEDFILEINFO *fInfo = nullptr;
//         DWORD dwHandle;
//         DWORD dwSize = GetFileVersionInfoSizeA(szImg, &dwHandle);
//         if (dwSize > 0)
//         {
//           LPVOID vData = malloc(dwSize);
//           if (vData != nullptr)
//           {
//             if (GetFileVersionInfoA(szImg, dwHandle, dwSize, vData) != 0)
//             {
//               UINT len;
//               TCHAR szSubBlock[] = _T("\\");
//               if (VerQueryValue(vData, szSubBlock, (LPVOID*) &fInfo, &len) == 0)
//                 fInfo = nullptr;
//               else
//               {
//                 fileVersion = ((ULONGLONG)fInfo->dwFileVersionLS) + ((ULONGLONG)fInfo->dwFileVersionMS << 32);
//               }
//             }
//             free(vData);
//           }
//         }
//       }

      // Retrive some additional-infos about the module
      IMAGEHLP_MODULE64_V2 Module;
      const char *szSymType = "-unknown-";
      if (this->GetModuleInfo(hProcess, baseAddr, &Module) != FALSE)
      {
        switch(Module.SymType)
        {
          case SymNone:
            szSymType = "-nosymbols-";
            break;
          case SymCoff:
            szSymType = "COFF";
            break;
          case SymCv:
            szSymType = "CV";
            break;
          case SymPdb:
            szSymType = "PDB";
            break;
          case SymExport:
            szSymType = "-exported-";
            break;
          case SymDeferred:
            szSymType = "-deferred-";
            break;
          case SymSym:
            szSymType = "SYM";
            break;
          case 8: //SymVirtual:
            szSymType = "Virtual";
            break;
          case 9: // SymDia:
            szSymType = "DIA";
            break;
        }
      }
      this->m_parent->OnLoadModule(img, mod, baseAddr, size, result, szSymType, Module.LoadedImageName, fileVersion);
    }
    if (szImg != nullptr) free(szImg);
    if (szMod != nullptr) free(szMod);
    return result;
  }
public:
  BOOL LoadModules(HANDLE hProcess, DWORD dwProcessId)
  {
    // first try toolhelp32
    if (GetModuleListTH32(hProcess, dwProcessId))
      return true;
    // then try psapi
    return GetModuleListPSAPI(hProcess);
  }


  BOOL GetModuleInfo(HANDLE hProcess, DWORD64 baseAddr, IMAGEHLP_MODULE64_V2 *pModuleInfo)
  {
    if(this->pSGMI == nullptr)
    {
      SetLastError(ERROR_DLL_INIT_FAILED);
      return FALSE;
    }
    // First try to use the larger ModuleInfo-Structure
//    memset(pModuleInfo, 0, sizeof(IMAGEHLP_MODULE64_V3));
//    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V3);
//    if (this->pSGMI_V3 != nullptr)
//    {
//      if (this->pSGMI_V3(hProcess, baseAddr, pModuleInfo) != FALSE)
//        return TRUE;
//      // check if the parameter was wrong (size is bad...)
//      if (GetLastError() != ERROR_INVALID_PARAMETER)
//        return FALSE;
//    }
    // could not retrive the bigger structure, try with the smaller one (as defined in VC7.1)...
    pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
    void *pData = tmpData; // reserve enough memory, so the bug in v6.3.5.1 does not lead to memory-overwrites...
    memcpy(pData, pModuleInfo, sizeof(IMAGEHLP_MODULE64_V2));
    if (this->pSGMI(hProcess, baseAddr, (IMAGEHLP_MODULE64_V2*) pData) != FALSE)
    {
      // only copy as much memory as is reserved...
      memcpy(pModuleInfo, pData, sizeof(IMAGEHLP_MODULE64_V2));
      pModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64_V2);
      return TRUE;
    }
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }
};

// #############################################################
StackWalker::StackWalker(DWORD dwProcessId, HANDLE hProcess)
{
  this->m_options = OptionsAll;
  this->m_modulesLoaded = FALSE;
  this->m_hProcess = hProcess;
  this->m_sw = TSNEW_T( MEMT_STACKWLK, StackWalkerInternal, this, this->m_hProcess);
  this->m_dwProcessId = dwProcessId;
  this->m_szSymPath = nullptr;
}
StackWalker::StackWalker(int options, LPCSTR szSymPath, DWORD dwProcessId, HANDLE hProcess)
{
  this->m_options = options;
  this->m_modulesLoaded = FALSE;
  this->m_hProcess = hProcess;
  this->m_sw = TSNEW_T(MEMT_STACKWLK, StackWalkerInternal, this, this->m_hProcess);
  this->m_dwProcessId = dwProcessId;
  if (szSymPath != nullptr)
  {
    this->m_szSymPath = _strdup(szSymPath);
    this->m_options |= SymBuildPath;
  }
  else
    this->m_szSymPath = nullptr;
}

StackWalker::~StackWalker()
{
  if (m_szSymPath != nullptr)
    free(m_szSymPath);
  m_szSymPath = nullptr;
  if (this->m_sw != nullptr)
    TSDEL( this->m_sw );
  this->m_sw = nullptr;
}

BOOL StackWalker::LoadModules()
{
    MEMT( MEMT_STACKWLK );

  if (this->m_sw == nullptr)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }
  if (m_modulesLoaded != FALSE)
    return TRUE;

  // Build the sym-path:
  char *szSymPath = nullptr;
  if ( (this->m_options & SymBuildPath) != 0)
  {
    const size_t nSymPathLen = 4096;
    szSymPath = (char*) MM_ALLOC(nSymPathLen);
    if (szSymPath == nullptr)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
    }
    szSymPath[0] = 0;
    // Now first add the (optional) provided sympath:
    if (this->m_szSymPath != nullptr)
    {
      strncat_s(szSymPath, nSymPathLen, this->m_szSymPath, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
    }

    strncat_s(szSymPath, nSymPathLen, ".;", nSymPathLen);

    const size_t nTempLen = 1024;
    char szTemp[nTempLen];
    // Now add the current directory:
    if (GetCurrentDirectoryA(nTempLen, szTemp) > 0)
    {
      szTemp[nTempLen-1] = 0;
      strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
    }

    // Now add the path for the main-module:
    if (GetModuleFileNameA(nullptr, szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      for (char *p = (szTemp+strlen(szTemp)-1); p >= szTemp; --p)
      {
        // locate the rightmost path separator
        if ( (*p == '\\') || (*p == '/') || (*p == ':') )
        {
          *p = 0;
          break;
        }
      }  // for (search for path separator...)
      if (szTemp[0])
      {
        strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
        strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
      }
    }
    if (GetEnvironmentVariableA("_NT_SYMBOL_PATH", szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
    }
    if (GetEnvironmentVariableA("_NT_ALTERNATE_SYMBOL_PATH", szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
    }
    if (GetEnvironmentVariableA("SYSTEMROOT", szTemp, nTempLen) > 0)
    {
      szTemp[nTempLen-1] = 0;
      strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
      // also add the "system32"-directory:
      strncat_s(szTemp, nTempLen, "\\system32", nTempLen);
      strncat_s(szSymPath, nSymPathLen, szTemp, nSymPathLen);
      strncat_s(szSymPath, nSymPathLen, ";", nSymPathLen);
    }

//     if ( (this->m_options & SymBuildPath) != 0)
//     {
//       if (GetEnvironmentVariableA("SYSTEMDRIVE", szTemp, nTempLen) > 0)
//       {
//         szTemp[nTempLen-1] = 0;
//         strncat_s(szSymPath, nSymPathLen, "SRV*");
//         strncat_s(szSymPath, nSymPathLen, szTemp);
//         strncat_s(szSymPath, nSymPathLen, "\\websymbols");
//         strncat_s(szSymPath, nSymPathLen, "*http://msdl.microsoft.com/download/symbols;");
//       }
//       else
//         strncat_s(szSymPath, nSymPathLen, "SRV*c:\\websymbols*http://msdl.microsoft.com/download/symbols;");
//     }
  }

  // First Init the whole stuff...
  BOOL bRet = this->m_sw->Init(szSymPath);
  if (szSymPath != nullptr) MM_FREE(szSymPath); szSymPath = nullptr;
  if (bRet == FALSE)
  {
    this->OnDbgHelpErr("Error while initializing dbghelp.dll", 0, 0);
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }

  bRet = this->m_sw->LoadModules(this->m_hProcess, this->m_dwProcessId);
  if (bRet != FALSE)
    m_modulesLoaded = TRUE;
  return bRet;
}

static CONTEXT __c;
static StackWalker::CallstackEntry csEntry;
static IMAGEHLP_SYMBOL64 *pSym;
static StackWalkerInternal::IMAGEHLP_MODULE64_V2 Module;
static IMAGEHLP_LINE64 Line;
static int frameNum;
static STACKFRAME64 __s; // in/out stackframe

static char sym[sizeof(IMAGEHLP_SYMBOL64) + StackWalker::STACKWALK_MAX_NAMELEN];

BOOL StackWalker::ShowCallstack(HANDLE hThread, const CONTEXT *context)const
{
  pSym = nullptr;

  if (this->m_sw->m_hDbhHelp == nullptr)
  {
    SetLastError(ERROR_DLL_INIT_FAILED);
    return FALSE;
  }

  if (context == nullptr)
  {
    if (hThread!=GetCurrentThread()) SuspendThread(hThread); //-V720
    memset(&__c, 0, sizeof(CONTEXT));
    __c.ContextFlags = USED_CONTEXT_FLAGS;
    if (GetThreadContext(hThread, &__c) == FALSE)
    {
        ResumeThread(hThread);
        return FALSE;
    }
    TraceRegisters(hThread, &__c);
  }else __c = *context;

  int len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "Call stack (%08p):\r\n", hThread);
  OnOutput(outBuffer, len);

  // init STACKFRAME for first call
  memset(&__s, 0, sizeof(__s));
  DWORD imageType;
#ifdef _M_IX86
  // normally, call ImageNtHeader() and use machine info from PE header
  imageType = IMAGE_FILE_MACHINE_I386;
  __s.AddrPC.Offset = __c.Eip;
  __s.AddrPC.Mode = AddrModeFlat;
  __s.AddrFrame.Offset = __c.Ebp;
  __s.AddrFrame.Mode = AddrModeFlat;
  __s.AddrStack.Offset = __c.Esp;
  __s.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
  imageType = IMAGE_FILE_MACHINE_AMD64;
  __s.AddrPC.Offset = __c.Rip;
  __s.AddrPC.Mode = AddrModeFlat;
  __s.AddrFrame.Offset = __c.Rsp;
  __s.AddrFrame.Mode = AddrModeFlat;
  __s.AddrStack.Offset = __c.Rsp;
  __s.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

  pSym = (IMAGEHLP_SYMBOL64 *)sym;
  memset(pSym, 0, sizeof(IMAGEHLP_SYMBOL64) + STACKWALK_MAX_NAMELEN);
  pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
  pSym->MaxNameLength = STACKWALK_MAX_NAMELEN;

  memset(&Line, 0, sizeof(Line));
  Line.SizeOfStruct = sizeof(Line);

  memset(&Module, 0, sizeof(Module));
  Module.SizeOfStruct = sizeof(Module);

  for (frameNum = 0; ; ++frameNum )
  {
    // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
    // if this returns ERROR_INVALID_ADDRESS (487) or ERROR_NOACCESS (998), you can
    // assume that either you are done, or that the stack is so hosed that the next
    // deeper frame could not be found.
    // CONTEXT need not to be suplied if imageTyp is IMAGE_FILE_MACHINE_I386!
    if ( ! this->m_sw->pSW(imageType, this->m_hProcess, hThread, &__s, &__c, myReadProcMem, this->m_sw->pSFTA, this->m_sw->pSGMB, nullptr) )
    {
      //this->OnDbgHelpErr("StackWalk64", GetLastError(), s.AddrPC.Offset);
      break;
    }

    csEntry.offset = __s.AddrPC.Offset;
	csEntry.segment = (WORD)__c.SegCs;
    csEntry.name[0] = 0;
    csEntry.undName[0] = 0;
    csEntry.undFullName[0] = 0;
    csEntry.offsetFromSmybol = 0;
    csEntry.offsetFromLine = 0;
    csEntry.lineFileName[0] = 0;
    csEntry.lineNumber = 0;
    csEntry.loadedImageName[0] = 0;
    csEntry.moduleName[0] = 0;
    if (__s.AddrPC.Offset == __s.AddrReturn.Offset)
    {
      //this->OnDbgHelpErr("StackWalk64-Endless-Callstack!", 0, s.AddrPC.Offset);
      break;
    }
    if (__s.AddrPC.Offset != 0)
    {
      // we seem to have a valid PC
      // show procedure info (SymGetSymFromAddr64())
      if (this->m_sw->pSGSFA(this->m_hProcess, __s.AddrPC.Offset, &(csEntry.offsetFromSmybol), pSym) != FALSE)
      {
        strncpy_s(csEntry.name, sizeof(csEntry.name), pSym->Name, sizeof(csEntry.name));
        // UnDecorateSymbolName()
        this->m_sw->pUDSN( pSym->Name, csEntry.undName, STACKWALK_MAX_NAMELEN, UNDNAME_NAME_ONLY );
        this->m_sw->pUDSN( pSym->Name, csEntry.undFullName, STACKWALK_MAX_NAMELEN, UNDNAME_COMPLETE );
      }
      else
      {
        //this->OnDbgHelpErr("SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset);
      }

      // show line number info, NT5.0-method (SymGetLineFromAddr64())
      if (this->m_sw->pSGLFA != nullptr )
      { // yes, we have SymGetLineFromAddr64()
        if (this->m_sw->pSGLFA(this->m_hProcess, __s.AddrPC.Offset, &(csEntry.offsetFromLine), &Line) != FALSE)
        {
          csEntry.lineNumber = Line.LineNumber;

		  strncpy_s(csEntry.lineFileName, sizeof(csEntry.lineFileName), Line.FileName, sizeof(csEntry.lineFileName));
        }
        else
        {
          //this->OnDbgHelpErr("SymGetLineFromAddr64", GetLastError(), s.AddrPC.Offset);
        }
      } // yes, we have SymGetLineFromAddr64()

      // show module info (SymGetModuleInfo64())
      if (this->m_sw->GetModuleInfo(this->m_hProcess, __s.AddrPC.Offset, &Module ) != FALSE)
      { // got module info OK
        switch ( Module.SymType )
        {
        case SymNone:
          csEntry.symTypeString = "-nosymbols-";
          break;
        case SymCoff:
          csEntry.symTypeString = "COFF";
          break;
        case SymCv:
          csEntry.symTypeString = "CV";
          break;
        case SymPdb:
          csEntry.symTypeString = "PDB";
          break;
        case SymExport:
          csEntry.symTypeString = "-exported-";
          break;
        case SymDeferred:
          csEntry.symTypeString = "-deferred-";
          break;
        case SymSym:
          csEntry.symTypeString = "SYM";
          break;
#if API_VERSION_NUMBER >= 9
        case SymDia:
          csEntry.symTypeString = "DIA";
          break;
#endif
        case 8: //SymVirtual:
          csEntry.symTypeString = "Virtual";
          break;
        default:
          //_snprintf( ty, sizeof ty, "symtype=%ld", (long) Module.SymType );
          csEntry.symTypeString = nullptr;
          break;
        }

        strncpy_s(csEntry.moduleName, sizeof(csEntry.moduleName), Module.ModuleName, sizeof(csEntry.moduleName));
        csEntry.baseOfImage = Module.BaseOfImage;
        strncpy_s(csEntry.loadedImageName, sizeof(csEntry.loadedImageName), Module.LoadedImageName, sizeof(csEntry.loadedImageName));
		csEntry.loadedImageNamep=strrchr(csEntry.loadedImageName,'\\');
		if (csEntry.loadedImageNamep) csEntry.loadedImageNamep++;
      } // got module info OK
      else
      {
        //this->OnDbgHelpErr("SymGetModuleInfo64", GetLastError(), s.AddrPC.Offset);
      }
    } // we seem to have a valid PC

    CallstackEntryType et = nextEntry;
    if (frameNum == 0)
      et = firstEntry;
    this->OnCallstackEntry(et, csEntry, __s);

    if (__s.AddrReturn.Offset == 0)
    {
      this->OnCallstackEntry(lastEntry, csEntry, __s);
      SetLastError(ERROR_SUCCESS);
      break;
    }
  } // for ( frameNum )

  if (context == nullptr)
    ResumeThread(hThread);

  return TRUE;
}

BOOL __stdcall StackWalker::myReadProcMem(
    HANDLE      hProcess,
    DWORD64     qwBaseAddress,
    PVOID       lpBuffer,
    DWORD       nSize,
    LPDWORD     lpNumberOfBytesRead
    )
{
    SIZE_T st;
    BOOL bRet = ReadProcessMemory(hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, &st);
    *lpNumberOfBytesRead = (DWORD) st;
    //printf("ReadMemory: hProcess: %p, baseAddr: %p, buffer: %p, size: %d, read: %d, result: %d\n", hProcess, (LPVOID) qwBaseAddress, lpBuffer, nSize, (DWORD) st, (DWORD) bRet);
    return bRet;
}

void StackWalker::OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion)
{
	UNREFERENCED_PARAMETER(img);
	UNREFERENCED_PARAMETER(mod);
	UNREFERENCED_PARAMETER(baseAddr);
	UNREFERENCED_PARAMETER(size);
	UNREFERENCED_PARAMETER(result);
	UNREFERENCED_PARAMETER(symType);
	UNREFERENCED_PARAMETER(pdbName);
	UNREFERENCED_PARAMETER(fileVersion);
}

void StackWalker::OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry, STACKFRAME64 &stackInfo)const
{
	if ( (eType != lastEntry) && (entry.offset != 0) )
	{
        int len = 0;
		if (/*entry.lineFileName!=nullptr && */entry.lineFileName[0] != 0)
		{
			if (entry.offsetFromLine!=0)
			{
				len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "%04X:%p (0x%p 0x%p 0x%p 0x%p) %s, %s()+%llu byte(s), %s, line %u+%u byte(s)\r\n", entry.segment, (LPVOID) entry.offset, (LPVOID) stackInfo.Params[0], (LPVOID) stackInfo.Params[1], (LPVOID) stackInfo.Params[2], (LPVOID) stackInfo.Params[3], entry.loadedImageNamep, entry.name, entry.offsetFromSmybol, entry.lineFileName, entry.lineNumber, entry.offsetFromLine);
			}
			else
			{
				len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "%04X:%p (0x%p 0x%p 0x%p 0x%p) %s, %s()+%llu byte(s), %s, line %u\r\n", entry.segment, (LPVOID) entry.offset, (LPVOID) stackInfo.Params[0], (LPVOID) stackInfo.Params[1], (LPVOID) stackInfo.Params[2], (LPVOID) stackInfo.Params[3], entry.loadedImageNamep, entry.name, entry.offsetFromSmybol, entry.lineFileName, entry.lineNumber);
			}
		}
		else
		{
			len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "%04X:%p (0x%p 0x%p 0x%p 0x%p) %s, %s()+%llu byte(s), %s\r\n", entry.segment, (LPVOID) entry.offset, (LPVOID) stackInfo.Params[0], (LPVOID) stackInfo.Params[1], (LPVOID) stackInfo.Params[2], (LPVOID) stackInfo.Params[3], entry.loadedImageNamep, entry.name, entry.offsetFromSmybol, entry.lineFileName);
		}
		OnOutput(outBuffer, len);
	}
}

void StackWalker::OnDbgHelpErr(LPCSTR szFuncName, DWORD gle, DWORD64 addr)
{
	int len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, " ERROR: %s, GetLastError: %lu (Address: %p)\r\n", szFuncName, gle, (LPVOID) addr);
	OnOutput(outBuffer, len);
}

void StackWalker::OnSymInit(LPCSTR, DWORD, LPCSTR)
{
#if _MSC_VER <= 1200
	OSVERSIONINFOA ver;
	ZeroMemory(&ver, sizeof(OSVERSIONINFOA));
	ver.dwOSVersionInfoSize = sizeof(ver);
	if (GetVersionExA(&ver) != FALSE)
	{
		OnOSVersion(ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
					ver.szCSDVersion, 0, 0, 0, 0);
	}
#else
#pragma warning(push)
#pragma warning(disable:4996)
	OSVERSIONINFOEXA ver;
	ZeroMemory(&ver, sizeof(OSVERSIONINFOEXA));
	ver.dwOSVersionInfoSize = sizeof(ver);
	if (GetVersionExA( (OSVERSIONINFOA*) &ver) != FALSE)
	{
		OnOSVersion(ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber,
					ver.szCSDVersion, ver.wServicePackMajor, ver.wServicePackMinor,
					ver.wSuiteMask, ver.wProductType);
	}
#pragma warning(pop)
#endif
}

void StackWalker::OnOutput(LPCSTR szText, size_t /*len*/)const
{
	printf("%08X %s", (unsigned int)GetCurrentThreadId(), szText);

	fflush(stdout);
	fflush(stderr);
}

void StackWalker::OnOSVersion(DWORD dwMajorVersion, DWORD dwMinorVersion, DWORD dwBuildNumber, LPCSTR szCSDVersion, WORD wServicePackMajor, WORD wServicePackMinor, WORD wSuiteMask, BYTE wProductType) const
{
	_snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "OS-Version: %lu.%lu Build:%lu (%s) SP: %d.%d (0x%x-0x%x)\r\n",
				dwMajorVersion, dwMinorVersion, dwBuildNumber,
				szCSDVersion, wServicePackMajor, wServicePackMinor, wSuiteMask, wProductType);
// 	OnOutput(outBuffer);
}

void StackWalker::TraceRegisters( const HANDLE thread, const CONTEXT* pCxt )const
{
    // registers
#ifdef _M_IX86
    int len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "Registers (%08p):\r\n\
EAX=%p  EBX=%p  ECX=%p  EDX=%p  ESI=%p\r\n\
EDI=%p  EBP=%p  ESP=%p  EIP=%p  FLG=%p\r\n\
CS=%04X   DS=%04X  SS=%04X  ES=%04X   FS=%04X  GS=%04X\r\n\r\n", thread,
        (LPVOID)pCxt->Eax, (LPVOID)pCxt->Ebx, (LPVOID)pCxt->Ecx, (LPVOID)pCxt->Edx, (LPVOID)pCxt->Esi,
        (LPVOID)pCxt->Edi, (LPVOID)pCxt->Ebp, (LPVOID)pCxt->Esp, (LPVOID)pCxt->Eip, (LPVOID)pCxt->EFlags,
        pCxt->SegCs,pCxt->SegDs,pCxt->SegSs,
        pCxt->SegEs,pCxt->SegFs,pCxt->SegGs);
#elif _M_X64
    int len = _snprintf_s(outBuffer, STACKWALK_MAX_NAMELEN, "Registers (%08p):\r\n\
RAX=%p  RBX=%p  RCX=%p  RDX=%p  RSI=%p\r\n\
RDI=%p  R8 =%p  R9 =%p  R10=%p  R11=%p\r\n\
R12=%p  R13=%p  R14=%p  R15=%p  RIP=%p\r\n\
RSP=%p  EBP=%p  FLG=%p\r\n\
CS=%04X  DS=%04X  SS=%04X  ES=%04X   FS=%04X  GS=%04X\r\n\r\n", thread,
        (LPVOID)pCxt->Rax, (LPVOID)pCxt->Rbx, (LPVOID)pCxt->Rcx, (LPVOID)pCxt->Rdx, (LPVOID)pCxt->Rsi,
        (LPVOID)pCxt->Rdi, (LPVOID)pCxt->R8 , (LPVOID)pCxt->R9 , (LPVOID)pCxt->R10, (LPVOID)pCxt->R11,
        (LPVOID)pCxt->R12, (LPVOID)pCxt->R13, (LPVOID)pCxt->R14, (LPVOID)pCxt->R15, (LPVOID)pCxt->Rip,
        (LPVOID)pCxt->Rsp, (LPVOID)pCxt->Rbp, (LPVOID)(size_t)pCxt->EFlags,
        pCxt->SegCs,pCxt->SegDs,pCxt->SegSs,
        pCxt->SegEs,pCxt->SegFs,pCxt->SegGs);
#else
#error "Platform not supported!"
#endif
    OnOutput(outBuffer, len);
}

HMODULE StackWalker::DBGDLL() const
{
    return(m_sw->m_hDbhHelp);
}

#endif // _WIN32


#endif
