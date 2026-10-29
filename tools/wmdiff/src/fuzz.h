#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace wmdiff
{
  // The row model counts words, and a double is two of them, so the double
  // rows need more room than the float ones: the widest is the swept
  // DistVector3Box3<double> row, whose input is the vector (three doubles),
  // the box (fifteen), the time (one) and the two velocities (six) - fifty
  // words.
  constexpr int kMaxWords = 56;
  constexpr std::uint32_t kCanary = 0xCDCDCDCDu; // pre-fill of every output word

  struct Rng
  {
    std::uint64_t s;

    explicit Rng(const std::uint64_t seed) noexcept : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    std::uint64_t Next() noexcept // splitmix64
    {
      std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
      return z ^ (z >> 31);
    }
    std::uint32_t U32() noexcept { return static_cast<std::uint32_t>(Next() >> 32); }
    std::uint32_t Below(const std::uint32_t n) noexcept { return static_cast<std::uint32_t>((Next() >> 32) % n); }
    bool Chance(const std::uint32_t percent) noexcept { return Below(100) < percent; }
    double Unit() noexcept { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); } // [0,1)
    double Range(const double lo, const double hi) noexcept { return lo + (hi - lo) * Unit(); }
  };

  /// Global switch: allow signaling NaNs in generated inputs (off by default -
  /// engine arithmetic never produces one, and x87 `fld` quiets them, which
  /// is a known, intentional-free divergence class for pure copies).
  extern bool gAllowSnan;

  [[nodiscard]] inline std::uint32_t Bits(const float f) noexcept
  {
    std::uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
  }
  [[nodiscard]] inline float FromBits(const std::uint32_t u) noexcept
  {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
  }

  // A double travels as two 32-bit words: the low half first, because x86 is
  // little-endian.  A row's layout therefore counts two words per double,
  // and the words are never read as floats - the generator writes them with
  // StoreDouble and the row reads them back with LoadDouble.
  inline void StoreDouble(std::uint32_t* p, const double d) noexcept
  {
    std::uint64_t u = 0;
    std::memcpy(&u, &d, sizeof u);
    p[0] = static_cast<std::uint32_t>(u & 0xFFFFFFFFu);
    p[1] = static_cast<std::uint32_t>(u >> 32);
  }

  [[nodiscard]] inline double LoadDouble(const std::uint32_t* p) noexcept
  {
    const std::uint64_t u = static_cast<std::uint64_t>(p[0]) |
        (static_cast<std::uint64_t>(p[1]) << 32);
    double d = 0.0;
    std::memcpy(&d, &u, sizeof d);
    return d;
  }

  /// One float drawn from a mix of classes: small, moderate, unit-range,
  /// log-uniform across every exponent, raw bit patterns (NaN payloads, inf,
  /// denormals), and a table of edge values plus their +-k ulp neighbours.
  float GenFloat(Rng& rng);
  /// A "well-behaved" float: uniform in [-lim, lim].
  float GenPlain(Rng& rng, float lim);
  /// An edge value (zeros, +-inf, NaN, FLT_MIN, denormals, FLT_MAX, +-1, ...).
  float GenEdge(Rng& rng);
  /// The same mixture as GenFloat, but over binary64.  The double's exponent
  /// range is much wider than the float's, so the degenerate cases here are
  /// exact powers of two and exact halves - values a double holds without any
  /// rounding at all, which is what a degenerate branch needs.
  double GenDouble(Rng& rng);

  void GenVec3(Rng& rng, float* v);
  void GenUnitVec3(Rng& rng, float* v);
  /// The same, over binary64.
  void GenVec3d(Rng& rng, double* v);
  void GenUnitVec3d(Rng& rng, double* v);
  /// v2 = +-v1 (+ a tiny perturbation): near-parallel / antiparallel pairs.
  void GenVecPair(Rng& rng, float* v1, float* v2);
  /// Scalar-first quaternion lanes (w, x, y, z): unit, near-identity, scaled,
  /// raw, or edge.
  void GenQuat(Rng& rng, float* q);
  /// q2 = +-q1 (+ perturbation) or independent.
  void GenQuatPair(Rng& rng, float* q1, float* q2);
  float GenAngle(Rng& rng);
  float GenT(Rng& rng);

  /// Which draw a typed row's generator should make.
  ///
  /// WithExtremes is the word path's mixture, extremes included: the typed
  /// generator makes *the same draws in the same order*, so the same seed
  /// produces the same input bytes as the word path did - that is what makes
  /// the conversion provably harmless.
  ///
  /// Ordinary is the --skip-extreme draw: every lane is finite, in range and
  /// normal (no NaN, no infinity, no subnormal), so a typed row can never be
  /// skipped for want of a comparable input. It may consume a different number
  /// of draws - the two kinds are never mixed in one run.
  enum class DrawKind
  {
    Ordinary,
    WithExtremes
  };

  struct Target
  {
    const char* name;
    std::uint32_t va;     // retail entry point (for the report)
    const char* source;   // our definition, file:line
    int inWords;
    int outWords;
    const char* inLayout;  // e.g. "v[3] q[4]"
    const char* outLayout; // e.g. "dest[3] ret"
    void (*gen)(Rng&, std::uint32_t* in);
    void (*retail)(const std::uint32_t* in, std::uint32_t* out);
    void (*ours)(const std::uint32_t* in, std::uint32_t* out);

    // --- typed path ----------------------------------------------------------
    //
    // A typed row hands the two sides the *same* typed input struct and gets
    // a typed output struct back; the row's own file owns the struct
    // definitions, so the harness only ever sees opaque byte buffers
    // (inBytes / outBytes long). The typed members are used when genT is not
    // null; the word members above are used otherwise. Both are kept for now:
    // the word path still carries the rows that have not been converted yet.
    //
    // The typed members come last so that a row which has not been converted
    // keeps its initializer exactly as it was - the default initializers
    // below leave the typed members null and zero.
    std::uint32_t inBytes = 0;
    std::uint32_t outBytes = 0;
    void (*genT)(Rng&, void* in, DrawKind) = nullptr;
    void (*retailT)(const void* in, void* out) = nullptr;
    void (*oursT)(const void* in, void* out) = nullptr;
  };

  const Target* MathTargets(int& count);
} // namespace wmdiff
