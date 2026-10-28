#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <crtdbg.h>
#include <csignal>
#include <dbghelp.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include "gpg/core/diagnostics/CrashHooks.h"
#include "gpg/core/diagnostics/CrashReport.h"

namespace gpg::diag
{
  namespace
  {

    // ---------------------------------------------------------------------
    // CRT / SEH diagnostic hooks.
    //
    // The debug CRT aborts inside ucrtbased with a stack the debugger cannot
    // walk (every frame resolves to data or to a non-return address), so the
    // one thing that actually names the fault -- the CRT's own report text,
    // e.g. "HEAP CORRUPTION DETECTED: after Normal block (#NNNN)" or an
    // invalid-parameter expression -- is discarded. These two hooks copy it
    // into faf_diag.log beside the other probes.
    //
    // `/heapcheck` additionally turns on _CRTDBG_CHECK_ALWAYS_DF, which
    // validates the whole CRT heap on every allocation and free.
    //
    // Note this only covers the *CRT* heap. `gpg/core/utils/Global.cpp` replaces
    // `malloc`/`free` with the engine's own small-block allocator, so most engine
    // allocations never reach the CRT heap at all. The switch for that one is the
    // environment variable FAF_HEAPWATCH=1, which makes the allocator's existing
    // stamp/validate probe watch every size class instead of the single stale
    // class it was pinned to. For an engine-side corruption that is the switch
    // that matters; this one is the backstop for whatever does reach the CRT.
    // ---------------------------------------------------------------------

  } // namespace

  // The log sits beside the executable, not in whatever the process happened to
  // make its working directory, so it can actually be found and handed over.
  const char* DiagLogPath()
  {
    static char sPath[MAX_PATH * 2] = {};
    static bool sResolved = false;
    if (!sResolved) {
      sResolved = true;
      char exePath[MAX_PATH] = {};
      const DWORD written = ::GetModuleFileNameA(nullptr, exePath, static_cast<DWORD>(sizeof(exePath)));
      if (written == 0u || written >= sizeof(exePath)) {
        (void)::strcpy_s(sPath, sizeof(sPath), "faf_diag.log");
      } else {
        char* const lastSlash = std::strrchr(exePath, '\\');
        if (lastSlash != nullptr) {
          *(lastSlash + 1) = '\0';
        } else {
          exePath[0] = '\0';
        }
        (void)::sprintf_s(sPath, sizeof(sPath), "%sfaf_diag.log", exePath);
      }
    }
    return sPath;
  }

  namespace
  {
    // One append handle for the process, opened lazily.
    //
    // DiagLine used to fopen/fclose per line, and that malloc inside a crash
    // handler is exactly what a fault cannot survive. When the handler itself
    // faults, the OS terminates the process with the *original* exception code
    // and no remaining handler runs - so a Release|Win32 access violation left a
    // bare 0xC0000005 exit code and not one [FAULT] line anywhere. WriteFile on
    // a cached handle keeps the write path off the CRT heap entirely.
    HANDLE DiagSink()
    {
      static HANDLE sSink = INVALID_HANDLE_VALUE;
      if (sSink == INVALID_HANDLE_VALUE) {
        sSink = ::CreateFileA(
          DiagLogPath(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr
        );
      }
      return sSink;
    }
  } // namespace

  void DiagLine(const char* const format, ...)
  {
    // Isolated on purpose, see DiagSink: nothing below may escape, or the fault
    // being reported gets replaced by this one and everything is lost.
    __try {
      char line[512] = {};
      std::va_list args;
      va_start(args, format);
      // _TRUNCATE still null-terminates, so a long line is logged in part
      // rather than dropped - the return value is deliberately not used as a
      // gate. sizeof - 1 leaves room for the newline appended below.
      (void)_vsnprintf_s(line, sizeof(line) - 1, _TRUNCATE, format, args);
      va_end(args);
      std::size_t length = std::strlen(line);
      line[length] = '\n';
      ++length;

      const HANDLE sink = DiagSink();
      if (sink != INVALID_HANDLE_VALUE) {
        // Both game instances append to this file, so loop on short writes.
        const char* payload = line;
        DWORD remaining = static_cast<DWORD>(length);
        while (remaining != 0u) {
          DWORD chunk = 0u;
          if (::WriteFile(sink, payload, remaining, &chunk, nullptr) == FALSE || chunk == 0u) {
            break;
          }
          payload += chunk;
          remaining -= chunk;
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      ::OutputDebugStringA("faf_diag: DiagLine failed\n");
    }
  }

  // ---------------------------------------------------------------------
  // Self-symbolising backtrace.
  //
  // The reported fault is an abort raised on the sim thread from inside
  // ucrtbased, and the debugger cannot walk out of it: ucrtbased ships no
  // symbols here, so every frame above the break resolves to data or to a
  // non-return address and the one useful thing -- which engine call asked the
  // CRT to do something illegal -- is lost.
  //
  // The process does not need the debugger for that. main.exe is built with a
  // PDB and dbghelp.lib is already linked, so it can resolve its own return
  // addresses to function + file:line. Capturing the stack *inside* the handler
  // also beats reading it afterwards: the handler runs on the faulting thread
  // before any unwinding, so the frames above it are the real ones.
  // ---------------------------------------------------------------------

  bool DiagSymbolsReady()
  {
    static bool sReady = false;
    static bool sTried = false;
    if (!sTried) {
      sTried = true;
      (void)::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
      // WinApp.cpp already initialises dbghelp for the crash reporter. A second
      // call fails with ERROR_INVALID_PARAMETER and that is fine - the handle is
      // already usable, so treat "already initialised" as success.
      sReady = ::SymInitialize(::GetCurrentProcess(), nullptr, TRUE) != FALSE
        || ::GetLastError() == ERROR_INVALID_PARAMETER;
    }
    return sReady;
  }

  namespace
  {

  } // namespace

  void DiagDescribeAddress(const void* const address, char* const out, const std::size_t outBytes)
  {
    const auto raw = reinterpret_cast<DWORD64>(address);

    // dbghelp is a large amount of code to run at a crash site, and a garbage
    // return address can reach it in any state. One undescribable frame must
    // cost one line, not the rest of the report.
    __try {
    // Module + RVA always works, even with no symbols, and is what makes an
    // unresolved frame decodable offline against the map file.
    char moduleName[MAX_PATH] = "?";
    DWORD64 moduleBase = 0;
    HMODULE module = nullptr;
    if (::GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          static_cast<LPCSTR>(address),
          &module
        ) != FALSE
        && module != nullptr) {
      moduleBase = reinterpret_cast<DWORD64>(module);
      char fullPath[MAX_PATH] = {};
      if (::GetModuleFileNameA(module, fullPath, static_cast<DWORD>(sizeof(fullPath))) != 0u) {
        const char* const slash = std::strrchr(fullPath, '\\');
        (void)::strcpy_s(moduleName, sizeof(moduleName), (slash != nullptr) ? (slash + 1) : fullPath);
      }
    }

    char symbolText[300] = {};
    if (DiagSymbolsReady()) {
      alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO) + 256] = {};
      auto* const symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = 255;

      DWORD64 symbolOffset = 0;
      if (::SymFromAddr(::GetCurrentProcess(), raw, &symbolOffset, symbol) != FALSE) {
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineOffset = 0;
        // `_TRUNCATE`, never `sprintf_s`: a decorated template name runs to
        // `MaxNameLen` (255) and `line.FileName` is a full source path, so the
        // two of them overflow `symbolText` routinely. `sprintf_s` treats that
        // as an invalid parameter and aborts the process -- which is how a
        // perfectly loggable access violation turned into a CRT assertion box
        // with the real fault's stack already discarded.
        if (::SymGetLineFromAddr64(::GetCurrentProcess(), raw, &lineOffset, &line) != FALSE) {
          (void)::_snprintf_s(
            symbolText, sizeof(symbolText), _TRUNCATE, " %s+0x%llX (%s:%lu)",
            symbol->Name, symbolOffset, (line.FileName != nullptr) ? line.FileName : "?", line.LineNumber
          );
        } else {
          (void)::_snprintf_s(symbolText, sizeof(symbolText), _TRUNCATE, " %s+0x%llX", symbol->Name, symbolOffset);
        }
      }
    }

    (void)::_snprintf_s(
      out, outBytes, _TRUNCATE, "%08llX %s+0x%llX%s",
      raw, moduleName, (moduleBase != 0) ? (raw - moduleBase) : 0ull, symbolText
    );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      (void)::_snprintf_s(out, outBytes, _TRUNCATE, "%08llX <undescribable>", raw);
    }
  }

  void DiagLogStack(const char* const tag)
  {
    // Cheap and complete first, expensive and pretty second.
    //
    // STATUS_STACK_OVERFLOW arrives here with no stack left, and dbghelp needs
    // several KB plus heap for one symbol. Symbolising first therefore lost the
    // entire walk: the header printed and all 40 frames were missing. The raw
    // return addresses are written as one cheap loop before any of that, which
    // is enough to symbolise offline against main.pdb.
    __try {
      void* frames[24] = {};
      const USHORT captured = ::RtlCaptureStackBackTrace(1, 24, frames, nullptr);

      DiagLine("[STACK] %s tid=%lu frames=%u", tag, ::GetCurrentThreadId(), static_cast<unsigned>(captured));
      for (USHORT i = 0; i < captured; ++i) {
        DiagLine("[STACKRAW] %02u %p", static_cast<unsigned>(i), frames[i]);
      }

      for (USHORT i = 0; i < captured; ++i) {
        char described[256] = {};
        DiagDescribeAddress(frames[i], described, sizeof(described));
        DiagLine("[STACK]   #%02u %s", static_cast<unsigned>(i), described);
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      DiagLine("[STACK] %s <symbol walk stopped>", tag);
    }
  }

  namespace
  {

    int CrtReportToDiagLog(const int reportType, char* const message, int* const returnValue)
    {
      static const char* const kKind[] = {"WARN", "ERROR", "ASSERT"};
      const char* const kind =
        (reportType >= 0 && reportType < 3) ? kKind[reportType] : "?";
      DiagLine("[CRTDIAG] %s: %s", kind, (message != nullptr) ? message : "(null)");
      DiagLogStack("crt-report");

      if (returnValue != nullptr) {
        *returnValue = 0; // do not force a breakpoint; let the CRT proceed
      }
      return 0; // fall through to the CRT's own reporting too
    }

    void DiagAbortSignalHandler(int)
    {
      DiagLine("[CRTDIAG] abort() raised");
      DiagLogStack("abort");
      // abort() is a process-killing path too (the CRT raises it for the
      // debug-heap reports and for failed runtime checks - verified by
      // /crashtest intdiv, where the divide never becomes SEH in a Debug
      // CRT). The full fatal path: report, dump, dialog.
      CrashReportContext reportContext{};
      reportContext.reason = "abort() raised (CRT fatal or runtime check)";
      HandleFatalPath(reportContext);
    }

    void DiagTerminateHandler()
    {
      DiagLine("[CRTDIAG] std::terminate (uncaught exception or noexcept violation)");
      DiagLogStack("terminate");
      // The full fatal path: report, dump, dialog. After the dialog closes,
      // let the CRT's own terminate run so behaviour is unchanged.
      CrashReportContext reportContext{};
      reportContext.reason = "std::terminate (uncaught exception or noexcept violation)";
      HandleFatalPath(reportContext);
      (void)std::signal(SIGABRT, SIG_DFL);
      std::abort();
    }

    void DiagPureCallHandler()
    {
      DiagLine("[CRTDIAG] pure virtual call");
      DiagLogStack("purecall");
      CrashReportContext reportContext{};
      reportContext.reason = "pure virtual call";
      HandleFatalPath(reportContext);
    }

    // First-chance, log-only. Always continues the search, so the debugger still
    // receives every exception exactly as before.
    LONG CALLBACK DiagVectoredHandler(EXCEPTION_POINTERS* const info)
    {
      if (info == nullptr || info->ExceptionRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
      }

      const DWORD code = info->ExceptionRecord->ExceptionCode;
      constexpr DWORD kCppException = 0xE06D7363u;

      // A C++ throw is an ordinary control-flow event in this engine, so it is
      // opt-in (FAF_LOGTHROW=1) - useful for pinning the bad_alloc, far too noisy
      // by default.
      static int sThrowBudget = 0;
      static const bool sLogThrows = ::GetEnvironmentVariableA("FAF_LOGTHROW", nullptr, 0) != 0u;

      static int sFaultBudget = 0;

      // This handler is the only thing that records an unhandled fault - it does
      // not take the fatal path, so no report and no minidump are written - so
      // it must not be able to fault itself. A handler that faults is skipped by
      // the OS, which then terminates the process with the original exception
      // code and no diagnostics whatsoever. Anything raised inside is therefore
      // swallowed here on purpose.
      __try {
      if (code == kCppException) {
        if (!sLogThrows || sThrowBudget >= 40) {
          return EXCEPTION_CONTINUE_SEARCH;
        }
        ++sThrowBudget;
        DiagLine("[FAULT] C++ throw");
        DiagLogStack("throw");
        return EXCEPTION_CONTINUE_SEARCH;
      }

      if (code != EXCEPTION_BREAKPOINT && code != EXCEPTION_ACCESS_VIOLATION
          && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO
          && code != EXCEPTION_STACK_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
      }

      if (sFaultBudget >= 12) {
        return EXCEPTION_CONTINUE_SEARCH;
      }
      ++sFaultBudget;

      char detail[160] = {};
      if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR kind = info->ExceptionRecord->ExceptionInformation[0];
        const ULONG_PTR target = info->ExceptionRecord->ExceptionInformation[1];
        (void)::sprintf_s(
          detail, sizeof(detail), " %s address %08IX",
          (kind == 0) ? "reading" : ((kind == 1) ? "writing" : "executing"), target
        );
      }

      // No module lookup here: on a stack-overflow fault the thread has room
      // for roughly one DiagLine call, and RtlPcToFileHeader is what pushed the
      // walk over the edge twice. The image base is logged once at startup
      // instead, where there is stack to spare.
      DiagLine(
        "[FAULT] code=0x%08lX at %08IX%s",
        code, reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress), detail
      );
      DiagLogStack("fault");
      } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Deliberately empty. This handler exists to observe, never to alter:
        // falling through to CONTINUE_SEARCH leaves the default handling - and
        // therefore the exit code the operator sees - completely unchanged.
      }
      return EXCEPTION_CONTINUE_SEARCH;
    }

    void CrtInvalidParameterToDiagLog(
      const wchar_t* const expression,
      const wchar_t* const function,
      const wchar_t* const file,
      const unsigned int line,
      const std::uintptr_t
    )
    {
      DiagLine(
        "[CRTDIAG] invalid parameter: expr=%ls function=%ls file=%ls line=%u",
        (expression != nullptr) ? expression : L"(none)",
        (function != nullptr) ? function : L"(none)",
        (file != nullptr) ? file : L"(none)",
        line
      );
    }

  } // namespace

  void InstallCrtDiagnosticHooks(const CrtDiagOptions& options)
  {
    (void)_CrtSetReportHook(&CrtReportToDiagLog);
    (void)_set_invalid_parameter_handler(&CrtInvalidParameterToDiagLog);

    // Every route the CRT can take to kill the process, each one logging the
    // stack of whoever asked for it. The vectored handler is first-chance and
    // always continues the search, so a debugger still sees everything.
    (void)::AddVectoredExceptionHandler(1UL, &DiagVectoredHandler);
    (void)std::signal(SIGABRT, &DiagAbortSignalHandler);
    (void)std::set_terminate(&DiagTerminateHandler);
    (void)_set_purecall_handler(&DiagPureCallHandler);
    (void)_set_abort_behavior(0U, _WRITE_ABORT_MSG); // no modal box on abort

    // The image base belongs here, not at the fault site. On a stack-overflow
    // fault the thread has room for about one DiagLine call, and a loader-lock
    // lookup there is what cost us the whole stack walk twice; logging it once
    // here is what makes the raw [STACKRAW] addresses resolvable after ASLR.
    DiagLine(
      "[CRTDIAG] ==== run start, pid=%lu base=%p ====", ::GetCurrentProcessId(), ::GetModuleHandleA(nullptr)
    );

    if (options.enableHeapCheck) {
      // _CRTDBG_CHECK_ALWAYS_DF walks every live block on every alloc and free.
      // On this engine that never gets past mounting the archives, so the
      // interval is selectable: FAF_HEAPCHECK_EVERY=1/16/128/1024 (default
      // 1024) still narrows an overrun to the allocations either side of it,
      // at a cost the run survives.
      char interval[16]{};
      const DWORD intervalLen = ::GetEnvironmentVariableA("FAF_HEAPCHECK_EVERY", interval, sizeof(interval));
      int checkFlag = _CRTDBG_CHECK_EVERY_1024_DF;
      const char* checkName = "every 1024 allocations";
      if (intervalLen != 0U && intervalLen < sizeof(interval)) {
        if (std::strcmp(interval, "1") == 0) {
          checkFlag = _CRTDBG_CHECK_ALWAYS_DF;
          checkName = "on every alloc/free";
        } else if (std::strcmp(interval, "16") == 0) {
          checkFlag = _CRTDBG_CHECK_EVERY_16_DF;
          checkName = "every 16 allocations";
        } else if (std::strcmp(interval, "128") == 0) {
          checkFlag = _CRTDBG_CHECK_EVERY_128_DF;
          checkName = "every 128 allocations";
        }
      }

      int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
      flags |= _CRTDBG_ALLOC_MEM_DF | checkFlag;
      (void)_CrtSetDbgFlag(flags);
      DiagLine("[CRTDIAG] /heapcheck on: validating the heap %s", checkName);
    }
  }

} // namespace gpg::diag
