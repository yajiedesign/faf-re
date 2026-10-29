// wmdiff - differential test of the rebuilt Wild Magic 3.8 against the retail
// engine's copy.
//
// Same shape as tools/wmdiff: map the retail PE at its preferred base, call
// the retail function and our rebuilt one with identical random inputs, and
// compare every output word bit for bit. The difference is scope - wmdiff
// covers recovered engine sources that its build cannot include directly, so it
// mirrors them by hand; wmdiff lives inside WildMagic3p8 and links the real
// headers, so wm3_harness.cpp only forwards and nothing is copied.
//
// Retail side is stubbed: see wm3_thunks.h.

#include "fuzz.h"
#include "targets_wm3.h"

#include "pe_image.h"

#include <windows.h>

#include <cfloat>
#include <xmmintrin.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "wm3_coll_targets.h"

using wmdiff::FromBits;
using wmdiff::kCanary;
using wmdiff::kMaxWords;
using wmdiff::Rng;

using u32 = std::uint32_t;
using u64 = std::uint64_t;

/// Float lane i of an input word block.
[[nodiscard]] inline float F(const u32* in, const int i) noexcept
{
  return FromBits(in[i]);
}

namespace
{
  bool gList = false;
  int gIters = 1000000;
  u64 gSeed = 0x5EED2007ull;
  int gMaxMismatch = 5;
  const char* gFilter = nullptr;
  const char* gImage = "C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe";
  bool gStrictNan = false;
  bool gSkipExtreme = false;
  wmdiff::Rng* gGenRng = nullptr;

  // The retail CRT's Windows-version cache. `__get_winmajor` (0x00A83756)
  // checks the flag at 0x00FB7D50 and, when it is clear, works the version out
  // again - and working it out goes through the CRT's per-thread data and back
  // into `__get_winmajor`, so with the flag never set the path recurses until
  // the stack is gone. The CRT's own start-up sets the flag and the version;
  // this is the same thing, done by hand.
  //
  // 5 is the major version of the Windows the retail engine shipped against
  // (Windows XP / Server 2003).
  struct RetailCrtState
  {
    u32 va;
    u32 value;
  };
  constexpr RetailCrtState kRetailCrtState[] = {
    { 0x00FB7D50u, 1u },   // the version is cached
    { 0x00FB7D5Cu, 5u },   // major version
  };

  // The retail CRT's fiber-local-storage API pointers. The CRT's start-up
  // resolves them from KERNEL32.DLL and leaves them at 0x00FB7E18..0x00FB7E24;
  // the file holds a descending junk pattern there, so a call through one of
  // them lands outside the image.
  //
  // The order is not a guess. __mtinit (0x00A95DC4) calls GetProcAddress with
  // these four names and stores each result in this order, and the name strings
  // are in the image:
  //
  //   0x00D72F40 "FlsAlloc"    -> 0x00FB7E18
  //   0x00D72F34 "FlsGetValue" -> 0x00FB7E1C
  //   0x00D72F28 "FlsSetValue" -> 0x00FB7E20
  //   0x00D72F20 "FlsFree"     -> 0x00FB7E24
  //
  // They were wired as GetValue/SetValue/Free/Alloc, i.e. rotated by one, which
  // put FlsAlloc's address where FlsGetValue is called and vice versa.
  //
  // If any of the four fails to resolve, __mtinit replaces all four with its own
  // TLS-based fallbacks (0x00A95A34 into 0x00FB7E18 and the IAT's TlsGetValue /
  // TlsSetValue / TlsFree into the rest); the FLS path is what runs on anything
  // from Vista on.
  struct RetailCrtFls
  {
    u32 slot;
    const char* name;
  };
  constexpr RetailCrtFls kRetailCrtFls[] = {
    { 0x00FB7E18u, "FlsAlloc" },
    { 0x00FB7E1Cu, "FlsGetValue" },
    { 0x00FB7E20u, "FlsSetValue" },
    { 0x00FB7E24u, "FlsFree" },
  };

  // Retail `global_mode_sse2` (0x01107030). SetSse2Mode (0x00A8ECD3) writes it
  // from `global_compat_flag` during CRT startup, which never runs here. The
  // CRT math dispatch bodies read it directly: at 0 the retail ceil/floor (and
  // friends) take the x87 fallback, whose NaN/errno path calls imports the
  // mapped image cannot service. wmdiff seeds the same word to 1 before any
  // target runs.
  constexpr u32 kRetailGlobalModeSse2 = 0x01107030u;

  // Retail static C-init entries whose BSS results the targets read. They run
  // in the CRT's static-init pass, before any thread installs _PC_24, so they
  // execute under the Windows default control word 0x027F.
  constexpr u32 kRetailStaticInits[] = {0x00BEDC30u, 0x00BEDC60u};

  // The retail CRT's own start-up, or the part of it the harness needs.
  //
  // 0x00A95DC4 is the CRT's dynamic API resolution: it loads KERNEL32.DLL,
  // resolves FlsAlloc / FlsGetValue / FlsSetValue / FlsFree into the slots at
  // 0x00FB7E18..0x00FB7E24, allocates the per-thread slot, publishes it at
  // 0x00F3E49C and initialises the per-thread data. The MSVC 2008+ CRT keeps
  // its errno and its thread state in fiber-local storage, and without this
  // the FLS pointers are the 0xFEFEFEFE the file happens to hold.
  //
  // It has to run before any target does. Without it, the CRT's per-thread
  // accessor asks the OS for the Windows version, which goes back through the
  // same accessor, and the pair recurses until the stack is gone.
  // (The retail CRT's start-up itself is 0x00A95DC4: it loads KERNEL32.DLL and
  // resolves FlsAlloc / FlsGetValue / FlsSetValue / FlsFree into the slots at
  // 0x00FB7E18..0x00FB7E24, then allocates the thread slot and publishes it at
  // 0x00F3E49C. Calling it from the harness recurses, because the start-up
  // itself reaches the version path that recurses - so what is published here
  // is the one piece of it that matters and that does not recurse.)

  // The simulation's FP environment. The x87 control word is applied with
  // fldcw, so this is a raw 16-bit control word. 0x007F decodes as precision
  // control 00 (24-bit significand, the engine's `_PC_24`), rounding control 00
  // (nearest), and the low six exception masks set. It is not a guess: the
  // engine installs exactly this word (`_controlfp(_PC_24, _MCW_PC)` in
  // WinApp.cpp and SimDriver.cpp) and MathReflection.cpp recognises it as
  // `(controlWord & 0x7F) == 0x7F`. The value moves the numbers a lot (see the
  // README section on codegen), so do not change it here without re-measuring.
  // The TLS index the retail CRT's per-thread accessor reads from 0x00F3E49C.
  DWORD gRetailTlsIndex = TLS_OUT_OF_INDEXES;

  u32 gControlWord = 0x007Fu;
  // MXCSR control bits: all exceptions masked, round-to-nearest, DAZ and FTZ
  // off. This is what the engine's IsDefaultFloatingPointEnvironment() gates on
  // (`_mm_getcsr() & 0x1F80 == 0x1F80`). The banner can print 0x1FA0 because
  // bits 0..5 are sticky exception *flags*, not control: 0x1FA0 is this value
  // with the precision flag (0x20) set by startup code. DAZ is bit 6 and is
  // clear; it was never the cause of the denormal residuals.
  constexpr u32 kMxcsr = 0x1F80u;

  void SetFpEnv() noexcept
  {
    unsigned short cw = static_cast<unsigned short>(gControlWord);
    __asm {
      fninit
      fldcw cw
    }
    _mm_setcsr(kMxcsr);
  }

  [[nodiscard]] unsigned short ReadControlWord() noexcept
  {
    unsigned short cw;
    __asm fnstcw cw
    return cw;
  }

  /// x87 stack depth (8 - number of empty tag slots), to catch an unbalanced
  /// call on either side.
  [[nodiscard]] int X87Depth() noexcept
  {
    unsigned short sw;
    __asm fnstsw sw
    const int top = (sw >> 11) & 7;
    return top == 0 ? 0 : 8 - top;
  }

  // Where the last caught exception was raised, and its code. A stack overflow
  // in the retail's own code and one in a handler that recurses look the same
  // in the code alone, and the address is what tells them apart.
  void* gLastFault = nullptr;
  DWORD gLastCode = 0;
  void* gLastFaultStack = nullptr;
  const void* gStackLimit = nullptr;

  /// The module a fault address belongs to. A crash inside the retail image and
  /// a crash inside a system DLL the retail called out to look identical in the
  /// code alone, and the module name is what tells them apart.
  const char* ModuleOf(void* addr)
  {
    MEMORY_BASIC_INFORMATION mbi{};
    if (addr == nullptr || VirtualQuery(addr, &mbi, sizeof mbi) == 0) {
      return "?";
    }
    static char path[MAX_PATH] = {0};
    path[0] = '\0';
    if (GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase), path, MAX_PATH) == 0) {
      return "?";
    }
    path[MAX_PATH - 1] = '\0';
    return path;
  }

  // The return addresses on the stack when the exception was raised, with the
  // repeats counted. A recursion shows up as one address appearing over and
  // over, and that is the only way to see one from the outside.
  void DumpStackFrames()
  {
    if (gLastFaultStack == nullptr) {
      return;
    }
    const auto* const words = static_cast<const std::uint32_t*>(gLastFaultStack);
    constexpr int kWords = 1024;
    // The upper bound used to read 0x004E96E9C, which is 0x400000 + 0x4A96E9C -
    // one hex digit too many. The image is 0x400000 + 0xE96E9C, so the bound is
    // 0x01296E9C, and the old value let every harness-heap address through: the
    // "stack frames" it printed were the harness's own allocations, which is why
    // they looked like nothing at all.
    constexpr std::uint32_t kImageLo = 0x00401000u;
    constexpr std::uint32_t kImageHi = 0x01296E9Cu;
    std::uint32_t counts[64] = {0};
    std::uint32_t values[64] = {0};
    int distinct = 0;
    for (int i = 0; i < kWords; ++i) {
      const std::uint32_t w = words[i];
      if (w < kImageLo || w > kImageHi) {
        continue; // not a code address in the retail image
      }
      int found = -1;
      for (int k = 0; k < distinct; ++k) {
        if (values[k] == w) {
          found = k;
          break;
        }
      }
      if (found < 0) {
        if (distinct == 64) {
          continue;
        }
        found = distinct++;
        values[found] = w;
      }
      counts[found] += 1;
    }
    std::printf("  stack frames (retail image addresses, innermost first):\n");
    for (int k = 0; k < distinct && k < 24; ++k) {
      std::printf("    0x%08X x%u\n", values[k], counts[k]);
    }
  }

  // The exception information is only available in the filter expression, so
  // the filter is where it has to be picked up.
  //
  // A minidump is written here too. The printf-based diagnosis above cannot get
  // to the bottom of a fault whose stack has been walked over - the values it
  // printed were UTF-16 text, i.e. not a stack at all - and a dump taken in the
  // filter, before the handler unwinds, is the only view of the faulting
  // context that is not already stale. Set WMDIFF_DUMP=1 to write it.
  void WriteFaultDump(EXCEPTION_POINTERS* info)
  {
    const char* const want = std::getenv("WMDIFF_DUMP");
    if (want == nullptr || want[0] == '\0' || want[0] == '0' || info == nullptr) {
      return;
    }
    const char* const path = std::getenv("WMDIFF_DUMP_PATH");
    HMODULE const dbg = LoadLibraryA("dbghelp.dll");
    if (dbg == nullptr) {
      return;
    }
    using WriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, int, void*, void*, void*);
    const auto write = reinterpret_cast<WriteDumpFn>(
      reinterpret_cast<void*>(GetProcAddress(dbg, "MiniDumpWriteDump")));
    if (write == nullptr) {
      FreeLibrary(dbg);
      return;
    }
    HANDLE const file = CreateFileA(path != nullptr ? path : "wmdiff-fault.dmp",
      GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      // MINIDUMP_EXCEPTION_INFORMATION, spelled out to avoid a windows headers
      // dependency ordering problem.
      struct DumpExceptionInfo {
        DWORD ThreadId;
        EXCEPTION_POINTERS* ExceptionPointers;
        BOOL ClientPointers;
      } dumpInfo{ GetCurrentThreadId(), info, FALSE };
      // 0x2 full memory | 0x800 full memory info | 0x1000 thread info
      write(GetCurrentProcess(), GetCurrentProcessId(), file, 0x2 | 0x800 | 0x1000,
        &dumpInfo, nullptr, nullptr);
      CloseHandle(file);
    }
    FreeLibrary(dbg);
  }

  unsigned long __stdcall GuardFilter(EXCEPTION_POINTERS* info)
  {
    if (info != nullptr) {
      gLastFault = info->ExceptionRecord->ExceptionAddress;
      gLastCode = info->ExceptionRecord->ExceptionCode;
      gLastFaultStack = info->ContextRecord != nullptr
        ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(info->ContextRecord->Esp))
        : nullptr;
      WriteFaultDump(info);
    }
    return EXCEPTION_EXECUTE_HANDLER;
  }

  // SEH has to live in a function without C++ unwinding objects.
  DWORD CallGuarded(void (*fn)(const u32*, u32*), const u32* in, u32* out) noexcept
  {
    __try {
      fn(in, out);
    } __except (GuardFilter(GetExceptionInformation())) {
      return gLastCode;
    }
    return 0;
  }

  // A retail void(void) C-init entry. wmdiff runs the two entries whose BSS
  // results the targets read (0x00BEDC30 computes float pi at 0x00FB78FC,
  // 0x00BEDC60 computes pi/2 at 0x00FB7914); the retail CRT would have run
  // them before main. Without them 0x006C1070's w <= -1 arm returns 0 instead
  // of pi.
  __declspec(naked) void RunRetailInit(const unsigned int va) noexcept
  {
    __asm {
      push ebx
      push esi
      push edi
      push ebp
      mov  eax, [esp + 20]
      call eax
      pop  ebp
      pop  edi
      pop  esi
      pop  ebx
      ret
    }
  }

  DWORD CallGuardedInit(const unsigned int va) noexcept
  {
    __try {
      RunRetailInit(va);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return GetExceptionCode();
    }
    return 0;
  }

  [[nodiscard]] bool IsNan(const u32 w) noexcept
  {
    return (w & 0x7F800000u) == 0x7F800000u && (w & 0x007FFFFFu) != 0;
  }

  void PrintWords(const char* label, const u32* w, const int n, const char* layout)
  {
    std::printf("    %s [%s]\n      ", label, layout);
    for (int k = 0; k < n; ++k) {
      if (k && (k % 4) == 0) {
        std::printf("\n      ");
      }
      std::printf("%08X(%g) ", w[k], static_cast<double>(FromBits(w[k])));
    }
    std::printf("\n");
  }

  void PrintMismatch(const wmdiff::Target& t, int iter, bool extreme, const u32* in,
    const u32* outR, const u32* outO)
  {
    std::printf(
      "[MISMATCH] %s (retail 0x%08X vs %s) iter %d  inputs: %s\n", t.name, t.va, t.source,
      iter, extreme ? "EXTREME (denormal/inf/NaN or |x| outside 2^-63..2^63)" : "ordinary");
    std::printf("      input   [%s]\n", t.inLayout);
    for (int k = 0; k < t.inWords; ++k) {
      std::printf("        %08X(%g)\n", in[k], static_cast<double>(FromBits(in[k])));
    }
    PrintWords("retail", outR, t.outWords, t.outLayout);
    PrintWords("ours  ", outO, t.outWords, t.outLayout);
  }

  void Usage()
  {
    std::printf(
      "wmdiff [--iters N] [--seed S] [--filter NAME] [--max-mismatch N]\n"
      "       [--image PATH] [--cw 0xNNNN] [--snan] [--strict-nan] [--skip-extreme] [--list]\n"
      "  --filter   run only targets whose name contains NAME (comma-separated list ok)\n"
      "  --cw       x87 control word (default 0x007F: _PC_24, round-nearest, all masked)\n"
      "  --snan     allow signaling NaNs in generated inputs\n"
      "  --strict-nan  count NaN-payload-only differences as mismatches\n"
      "  --skip-extreme  drop inputs that are denormal/inf/NaN or outside 2^-63..2^63\n"
      "  --list     print the target table and exit\n");
  }

  bool ParseArgs(int argc, char** argv)
  {
    for (int i = 1; i < argc; ++i) {
      const char* const a = argv[i];
      if (std::strcmp(a, "--iters") == 0) {
        gIters = std::atoi(argv[++i]);
      } else if (std::strcmp(a, "--seed") == 0) {
        gSeed = std::strtoull(argv[++i], nullptr, 0);
      } else if (std::strcmp(a, "--filter") == 0) {
        gFilter = argv[++i];
      } else if (std::strcmp(a, "--max-mismatch") == 0) {
        gMaxMismatch = std::atoi(argv[++i]);
      } else if (std::strcmp(a, "--image") == 0) {
        gImage = argv[++i];
      } else if (std::strcmp(a, "--cw") == 0) {
        gControlWord = static_cast<u32>(std::strtoul(argv[++i], nullptr, 0));
      } else if (std::strcmp(a, "--skip-extreme") == 0) {
        gSkipExtreme = true;
      } else if (std::strcmp(a, "--strict-nan") == 0) {
        gStrictNan = true;
      } else if (std::strcmp(a, "--snan") == 0) {
        wmdiff::gAllowSnan = true;
      } else if (std::strcmp(a, "--list") == 0) {
        gList = true;
      } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
        Usage();
        std::exit(0);
      } else {
        std::printf("unknown option: %s\n", a);
        Usage();
        return false;
      }
    }
    return true;
  }

  bool Selected(const char* name)
  {
    if (!gFilter) {
      return true;
    }
    std::string f(gFilter);
    size_t start = 0;
    while (start <= f.size()) {
      const size_t comma = f.find(',', start);
      const std::string part = f.substr(start, comma == std::string::npos ? std::string::npos
                                                                         : comma - start);
      if (!part.empty() && std::string(name).find(part) != std::string::npos) {
        return true;
      }
      if (comma == std::string::npos) {
        break;
      }
      start = comma + 1;
    }
    return false;
  }
} // namespace

int main(int argc, char** argv)
{
  if (!ParseArgs(argc, argv)) {
    return 3;
  }

  int count = 0;
  const wmdiff::Target* const targets = wm3targets::Targets(count);

  if (gList) {
    std::printf("%-18s %-10s %-52s %s\n", "name", "retail VA", "our source", "layouts");
    for (int i = 0; i < count; ++i) {
      const wmdiff::Target& t = targets[i];
      std::printf("%-18s 0x%08X %-52s in[%s] out[%s]%s\n", t.name, t.va,
        t.source[0] ? t.source : "(none)", t.inLayout, t.outLayout,
        t.va == 0 ? "   [TODO: retail VA not filled in]" : "");
    }
    return 0;
  }

  // Process initialisation puts NLS views and heaps at 0x00400000, so the retail
  // range has to be committed before our own CRT runs: re-exec this exe
  // suspended, commit [base, base+size) in the child, then let it initialise.
  if (GetEnvironmentVariableA("WMDIFF_CHILD", nullptr, 0) == 0) {
    u32 base = 0;
    u32 size = 0;
    if (!wmdiff::ReadImageExtent(gImage, base, size)) {
      std::fprintf(stderr, "wmdiff: cannot read PE header of %s\n", gImage);
      return 3;
    }
    std::fflush(stdout);
    return wmdiff::RelaunchWithPreclaimedImage(base, size);
  }

  wmdiff::RetailImage image;
  std::string error;
  if (!image.Load(gImage, error)) {
    std::printf("wmdiff: cannot map %s: %s\n", gImage, error.c_str());
    return 3;
  }
  if (!image.PokeDword(kRetailGlobalModeSse2, 1u)) {
    std::printf("wmdiff: cannot seed global_mode_sse2 at 0x%08X\n", kRetailGlobalModeSse2);
    return 3;
  }
  for (const RetailCrtState& state : kRetailCrtState) {
    if (!image.PokeDword(state.va, state.value)) {
      std::printf("wmdiff: cannot seed the CRT state at 0x%08X\n", state.va);
      return 3;
    }
  }
  {
    HMODULE const kernel32 = GetModuleHandleA("KERNEL32.DLL");
    for (const RetailCrtFls& fls : kRetailCrtFls) {
      FARPROC const proc = kernel32 != nullptr ? GetProcAddress(kernel32, fls.name) : nullptr;
      if (proc != nullptr) {
        // Encoded. The accessor reads these back through 0x00A959C6, which is
        // DecodePointer, so a raw address is decoded into a per-process random
        // value. This was tried once before and made things worse - but that was
        // before the FLS index and the per-thread block existed, so there was
        // nothing for the decoded getter to return. With both in place the chain
        // closes: decode gives FlsGetValue, FlsGetValue(7) gives the block.
        const auto value = static_cast<u32>(reinterpret_cast<std::uintptr_t>(
          EncodePointer(reinterpret_cast<void*>(proc))));
        if (!image.PokeDword(fls.slot, value)) {
          std::printf("wmdiff: cannot seed the CRT FLS slot at 0x%08X\n", fls.slot);
          return 3;
        }
      }
    }
  }
  {
    // The two thread indices the CRT keeps. Measured out of a real retail
    // process at 0x00A95A58: 0x00F3E49C holds 20 (a TlsAlloc index, stored by
    // __mtinit at 0x00A95E6C) and 0x00F3E498 holds 7 (an FlsAlloc index).
    //
    // __getptd pushes [0x00F3E498] and then calls the FlsGetValue pointer, so
    // that slot is the index handed to FlsGetValue. Left at the file's -1 it
    // asks FlsGetValue for slot -1 and gets whatever that returns.
    const DWORD tlsIndex = TlsAlloc();
    if (tlsIndex != TLS_OUT_OF_INDEXES) {
      if (!image.PokeDword(0x00F3E49Cu, tlsIndex)) {
        std::printf("wmdiff: cannot publish the retail CRT thread index at 0x%08X\n",
          0x00F3E49Cu);
        return 3;
      }
      gRetailTlsIndex = tlsIndex;
      std::printf("wmdiff: published the retail CRT TLS index %u\n", tlsIndex);
    }
    if (HMODULE const kernel32 = GetModuleHandleA("KERNEL32.DLL")) {
      using FlsAllocFn = DWORD(WINAPI*)(void*);
      const auto flsAlloc = reinterpret_cast<FlsAllocFn>(
        reinterpret_cast<void*>(GetProcAddress(kernel32, "FlsAlloc")));
      if (flsAlloc != nullptr) {
        const DWORD flsIndex = flsAlloc(nullptr);
        if (flsIndex != FLS_OUT_OF_INDEXES) {
          if (!image.PokeDword(0x00F3E498u, flsIndex)) {
            std::printf("wmdiff: cannot publish the retail CRT FLS index at 0x%08X\n",
              0x00F3E498u);
            return 3;
          }
          std::printf("wmdiff: published the retail CRT FLS index %u\n", flsIndex);

          // Pre-create the per-thread block. A started CRT already has one in
          // this slot; without it, __getptd allocates it through the CRT's own
          // calloc (0x00A95FB9) and that heap is initialised by the same start-up
          // that has never run. The CRT's own allocation is calloc(1, 0x214), so
          // a zeroed 0x214-byte block is the same thing the CRT would have made,
          // and the accessor then takes its non-null branch instead of allocating.
          using FlsSetValueFn = BOOL(WINAPI*)(DWORD, void*);
          const auto flsSetValue = reinterpret_cast<FlsSetValueFn>(
            reinterpret_cast<void*>(GetProcAddress(kernel32, "FlsSetValue")));
          if (flsSetValue != nullptr) {
            void* const ptd = std::calloc(1, 0x214);
            if (ptd != nullptr) {
              flsSetValue(flsIndex, ptd);
              using FlsGetValueFn = void*(WINAPI*)(DWORD);
              const auto flsGetValue = reinterpret_cast<FlsGetValueFn>(
                reinterpret_cast<void*>(GetProcAddress(kernel32, "FlsGetValue")));
              const void* const back = flsGetValue != nullptr ? flsGetValue(flsIndex) : nullptr;
              std::printf("wmdiff: pre-created the CRT per-thread block %p for FLS %u (read back %p)\n",
                ptd, flsIndex, back);
            }
          }
        }
      }
    }
  }
  for (const u32 init : kRetailStaticInits) {
    unsigned short defaultCw = 0x027F;
    __asm {
      fninit
      fldcw defaultCw
    }
    const DWORD code = CallGuardedInit(init);
    if (code != 0) {
      std::printf("wmdiff: retail static init 0x%08X raised 0x%08X\n", init, code);
      return 3;
    }
  }
  {
    const wmdiff::ImageEnvironment env = wmdiff::DescribeEnvironment();
    std::printf("wmdiff: mapped %s at 0x%08X (0x%X bytes, %u imports resolved, %u trapped)",
      gImage, image.Base(), image.SizeOfImage(), env.resolved_imports, env.trapped_imports);
    if (env.tls_bytes != 0) {
      std::printf(", TLS index %u with a %u byte block", env.tls_index, env.tls_bytes);
    }
    std::printf("\n");
  }

  std::printf("wmdiff: x87 CW=0x%04X MXCSR=0x%04X seed=0x%llX iters=%d (%s build)\n",
    ReadControlWord(), _mm_getcsr(), gSeed, gIters,
#ifdef _DEBUG
    "debug"
#else
    "release"
#endif
  );

  // WMDIFF_BREAK_AFTER_LOAD yields to an attached debugger once the retail image
  // is mapped. Both other ways of getting a breakpoint at a retail address are
  // closed: `bp` from the debugger fails with Win32 error 299 because the image
  // does not exist at the debugger's initial break, and an int3 planted from
  // inside the harness (WMDIFF_INT3_AT) is written but the WOW64 target raises
  // 0xC000001D past it instead of a breakpoint exception. Here the image is
  // mapped and the debugger can install its own breakpoint before any target
  // runs. Combine with WMDIFF_RW_CODE so the .text write is permitted.
  if (std::getenv("WMDIFF_BREAK_AFTER_LOAD") != nullptr) {
    std::printf("wmdiff: WMDIFF_BREAK_AFTER_LOAD - yielding to the debugger\n");
    std::fflush(stdout);
    DebugBreak();
  }

  int passed = 0, failed = 0, ran = 0, vacuous = 0;
  // WMDIFF_INT3_AT=<va>[,<va>...] plants int3s in the retail image once it is
  // mapped, so a debugger attached to this process stops there with the
  // registers live. A breakpoint set from the debugger side cannot do this: the
  // image is mapped by this process, so at the debugger's initial break the
  // address does not exist yet and `bp` fails with Win32 error 299
  // (ERROR_PARTIAL_COPY). A list, because the useful questions come in pairs -
  // which branch was taken, and what it returned.
  if (const char* const at = std::getenv("WMDIFF_INT3_AT")) {
    char buf[256] = {0};
    std::snprintf(buf, sizeof buf, "%s", at);
    for (char* p = buf; p != nullptr && *p != '\0';) {
      char* const comma = std::strpbrk(p, ",;");
      if (comma != nullptr) {
        *comma = '\0';
      }
      const u32 va = static_cast<u32>(std::strtoul(p, nullptr, 0));
      if (va != 0) {
        const std::uint32_t cur =
          *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(va));
        if (image.PokeDword(va, (cur & 0xFFFFFF00u) | 0xCCu)) {
          std::printf("wmdiff: planted int3 at 0x%08X (was %08X)\n", va, cur);
        }
        else {
          std::printf("wmdiff: cannot plant int3 at 0x%08X\n", va);
        }
      }
      p = comma == nullptr ? nullptr : comma + 1;
    }
  }

  for (int i = 0; i < count; ++i) {
    const wmdiff::Target& t = targets[i];
    if (!Selected(t.name)) {
      continue;
    }
    ++ran;

    Rng gen(gSeed ^ (0x9E3779B97F4A7C15ull * static_cast<u64>(i + 1)));

    int mismatches = 0, nanPayloadOnly = 0, extremeSkipped = 0;
    bool crashed = false;

    for (int iter = 0; iter < gIters; ++iter) {
      u32 in[wmdiff::kMaxWords];
      u32 outR[wmdiff::kMaxWords];
      u32 outO[wmdiff::kMaxWords];

      for (int k = 0; k < t.inWords; ++k) {
        in[k] = wmdiff::kCanary;
      }
      for (int k = 0; k < t.outWords; ++k) {
        outR[k] = wmdiff::kCanary;
        outO[k] = wmdiff::kCanary;
      }

      t.gen(gen, in);

      bool extreme = false;
      for (int k = 0; k < t.inWords; ++k) {
        if (IsNan(in[k]) || std::fabs(static_cast<double>(FromBits(in[k]))) > 9.2e18
          || (FromBits(in[k]) != 0.0f && std::fabs(static_cast<double>(FromBits(in[k]))) < 1.2e-38)) {
          extreme = true;
        }
      }
      if (gSkipExtreme && extreme) {
        ++extremeSkipped;
        continue;
      }

      SetFpEnv();
      if (gStackLimit == nullptr) {
        MEMORY_BASIC_INFORMATION stackInfo{};
        ULONG_PTR low = 0, high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        gStackLimit = reinterpret_cast<const void*>(low);
      }
      {
        const DWORD code = CallGuarded(t.retail, in, outR);
        if (code != 0) {
          std::printf("[CRASH] %s retail side raised 0x%08X at %p esp=%p (limit %p) [%s], iter %d; inputs:",
            t.name, code, gLastFault, gLastFaultStack, gStackLimit, ModuleOf(gLastFault), iter);
          DumpStackFrames();
          for (int k = 0; k < t.inWords; ++k) {
            std::printf(" %08X", in[k]);
          }
          std::printf("\n");
          crashed = true;
          break;
        }
      }

      SetFpEnv();
      {
        const DWORD code = CallGuarded(t.ours, in, outO);
        if (code != 0) {
          std::printf("[CRASH] %s our side raised 0x%08X at %p esp=%p (limit %p) [%s], iter %d; inputs:",
            t.name, code, gLastFault, gLastFaultStack, gStackLimit, ModuleOf(gLastFault), iter);
          for (int k = 0; k < t.inWords; ++k) {
            std::printf(" %08X", in[k]);
          }
          std::printf("\n");
          crashed = true;
          break;
        }
      }

      bool bad = false, onlyNanPayload = true;
      for (int k = 0; k < t.outWords; ++k) {
        if (outR[k] != outO[k]) {
          if (!(IsNan(outR[k]) && IsNan(outO[k]))) {
            onlyNanPayload = false;
          }
          bad = true;
        }
      }

      if (bad) {
        if (onlyNanPayload && !gStrictNan) {
          if (nanPayloadOnly == 0) {
            std::printf("[nan-payload] %s iter %d (first example; tallied, not a failure)\n",
              t.name, iter);
          }
          ++nanPayloadOnly;
          continue;
        }
        if (mismatches < gMaxMismatch) {
          PrintMismatch(t, iter, extreme, in, outR, outO);
        }
        ++mismatches;
      }
    }

    if (gSkipExtreme && extremeSkipped == gIters) {
      // Nothing was compared: every draw had an extreme word.  Printing this as
      // a pass is how rows with integer/bool/mask words in their input block
      // reported 200000/200000 without running a single input.
      ++vacuous;
      std::printf("[SKIP] %-18s 0x%08X  0/%d run: every draw had an extreme word"
                  " (not a pass)\n", t.name, t.va, gIters);
    } else if (crashed) {
      ++failed;
      std::printf("[FAIL] %s crashed\n", t.name);
    } else if (mismatches > 0) {
      ++failed;
      const std::string nanNote =
        nanPayloadOnly ? (" (+" + std::to_string(nanPayloadOnly) + " NaN-payload-only)") : "";
      std::printf("[FAIL] %-18s 0x%08X  %d mismatches in %d iterations%s\n", t.name, t.va,
        mismatches, gIters, nanNote.c_str());
    } else {
      ++passed;
      std::printf("[PASS] %-18s 0x%08X  %d/%d bit-identical%s\n", t.name, t.va, gIters, gIters,
        extremeSkipped ? (" (+" + std::to_string(extremeSkipped) + " extreme inputs skipped)").c_str()
                       : "");
    }
  }

  if (vacuous != 0) {
    std::printf("wmdiff: %d targets skipped with no non-extreme input; a --skip-extreme "
                "run says nothing about them\n", vacuous);
  }
  std::printf("wmdiff: %d/%d targets passed\n", passed, ran);
  return failed == 0 ? 0 : 1;
}
