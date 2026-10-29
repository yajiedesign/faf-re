#pragma once

#include <cstdint>
#include <string>

namespace wmdiff
{
  /**
   * What a hand-mapped image needs from the loader, and what it now gets.
   *
   * The image is mapped at its preferred base (0x00400000) without running
   * any of it: no entry point and no CRT startup. Two things the loader does
   * for a module are done by hand instead, because the retail's own runtime
   * needs them:
   *
   * - **Imports are resolved for real**, against the same DLLs the engine
   *   would have used. The CRT keeps its errno in thread-local storage and
   *   reaches it through TlsGetValue; with the import trapped, the CRT's
   *   error path - the one the sqrt takes for a non-positive argument - dies
   *   instead of setting errno and returning NaN. An import that cannot be
   *   resolved still gets a trap stub, which prints "unresolved import
   *   DLL!name" and raises an SEH exception the harness catches, so a target
   *   that reaches it fails loudly instead of jumping into garbage.
   * - **The TLS block is allocated and its index published**, the way the
   *   loader does it, and the TLS callbacks are called with
   *   DLL_PROCESS_ATTACH. Without it the retail CRT reads a garbage
   *   __tls_index and dereferences a null errno pointer.
   */
  struct ImageEnvironment
  {
    unsigned resolved_imports = 0;
    unsigned trapped_imports = 0;
    unsigned tls_index = 0;
    unsigned tls_bytes = 0;
  };

  /// The environment the mapped image ended up with.
  ImageEnvironment DescribeEnvironment();
  class RetailImage
  {
  public:
    bool Load(const std::string& path, std::string& error);

    [[nodiscard]] std::uint32_t Base() const noexcept { return mBase; }
    [[nodiscard]] std::uint32_t SizeOfImage() const noexcept { return mSizeOfImage; }
    [[nodiscard]] unsigned ImportCount() const noexcept { return mImportCount; }

    /// True when `va` lies inside a mapped executable section.
    [[nodiscard]] bool IsExecutable(std::uint32_t va) const noexcept;

    /// Writes one dword into the mapped image (used to seed CRT globals such
    /// as `global_mode_sse2`, which the never-run CRT init would have set).
    bool PokeDword(std::uint32_t va, std::uint32_t value) noexcept;

  private:
    std::uint32_t mBase = 0;
    std::uint32_t mSizeOfImage = 0;
    unsigned mImportCount = 0;
  };

  /// Reads ImageBase/SizeOfImage from the PE header without mapping anything.
  bool ReadImageExtent(const std::string& path, std::uint32_t& base, std::uint32_t& size);

  /// Re-runs this exe with the same command line as a suspended child,
  /// commits [base, base+size) in it before it initialises (process init
  /// otherwise puts NLS views and heaps at 0x00400000), resumes it and
  /// returns its exit code.
  int RelaunchWithPreclaimedImage(std::uint32_t base, std::uint32_t size);
} // namespace wmdiff
