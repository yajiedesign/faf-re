// Target table for Wm3::Math<float>'s leaf functions (Foundation/Math/Wm3Math.inl).
//
// A row is here only once its retail address is known. Most of Math has no
// rebuild call site, so most of it is out of scope: the README's "What is in
// scope" section and wmdiff/reachable.md list it.

#include "wm3_math_targets.h"

#include "fuzz.h"

#include <cstdint>

#include "wm3_math_harness.h"
#include "wm3_math_thunks.h"

using wmdiff::FromBits;
using wmdiff::GenAngle;
using wmdiff::GenEdge;
using wmdiff::GenFloat;
using wmdiff::GenPlain;
using wmdiff::Rng;

using u32 = std::uint32_t;

namespace wm3math
{
  namespace
  {
    // ---- input generators ---------------------------------------------------

    // One arbitrary float: raw bit patterns (NaN payloads, infinities,
    // denormals, FLT_MAX), edge values, +-k ulp neighbours and a log-uniform
    // sweep of the exponent range.
    void GenX1(Rng& rng, u32* in)
    {
      in[0] = wmdiff::Bits(GenFloat(rng));
    }

// ---- wrappers ----------------------------------------------------------
    //
    // The harness and the thunks already return raw u32 bit patterns, so the
    // retail and ours wrappers are the same shape and only the callee differs.

#define WM3MATH_WRAP_UNARY(NAME)                     \
    void Retail##NAME(const u32* in, u32* out)       \
    {                                                \
      out[0] = wm3mathretail::NAME(FromBits(in[0])); \
    }                                                \
    void Ours##NAME(const u32* in, u32* out)         \
    {                                                \
      out[0] = wm3mathharness::NAME(FromBits(in[0])); \
    }

    WM3MATH_WRAP_UNARY(FAbs)
    WM3MATH_WRAP_UNARY(Sqrt)

#undef WM3MATH_WRAP_UNARY

  } // namespace

  const wmdiff::Target kMathTargets[] = {
    //  name                 va          our source                                  inW outW inLayout  outLayout  gen          retail              ours
    { "Wm3::Math<float>::FAbs(float)", 0x0040DAB0, "Foundation/Math/Wm3Math.inl:82",           1, 1, "x",       "ret",      GenX1,        RetailFAbs,          OursFAbs },
    { "Wm3::Math<float>::Sqrt(float)", 0x00452FC0, "Foundation/Math/Wm3Math.inl:142",          1, 1, "x",       "ret",      GenX1,        RetailSqrt,          OursSqrt },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kMathTargets) / sizeof(kMathTargets[0]));
    return kMathTargets;
  }
} // namespace wm3math
