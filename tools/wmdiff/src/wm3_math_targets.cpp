// Target table for Wm3::Math<float>'s leaf functions (Foundation/Math/Wm3Math.inl).
//
// A row is here only once its retail address is known. Most of Math has no
// rebuild call site, so most of it is out of scope: the README's "What is in
// scope" section and wmdiff/reachable.md list it.
//
// Both rows are on the typed path: the two sides are handed the same typed
// input struct and answer with a typed output struct, and the harness only
// ever sees the byte buffers behind them.

#include "wm3_math_targets.h"

#include "fuzz.h"

#include <cmath>
#include <cstdint>

#include "wm3_math_harness.h"
#include "wm3_math_thunks.h"

using wmdiff::DrawKind;
using wmdiff::FromBits;
using wmdiff::GenFloat;
using wmdiff::GenPlain;
using wmdiff::Rng;

using u32 = std::uint32_t;

namespace wm3math
{
  namespace
  {
    // ---- typed row shapes --------------------------------------------------
    //
    // A plain float lane, not a class: the retail thunk takes the value by
    // value and the harness forward - which is what calls the real
    // Math<float> member - takes a float too.

    struct ScalarIn
    {
      float x;
    };

    struct ScalarOut
    {
      // The answer is carried as the float it is, so the bytes compared are
      // the float's own bit pattern - exactly what the word path compared.
      float ret;
    };

    // ---- typed generator ---------------------------------------------------
    //
    // WithExtremes draws exactly what the word path drew: one GenFloat, in
    // the same place in the sequence, so the same seed still produces the
    // same input bytes.
    //
    // Ordinary is the --skip-extreme draw: a finite, in-range, normal lane -
    // no NaN, no infinity, no subnormal. The square root's lane is
    // non-negative, so the answer is not a NaN either.

    void GenAbsT(Rng& rng, void* p, DrawKind kind)
    {
      ScalarIn& in = *static_cast<ScalarIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        in.x = GenFloat(rng);
        return;
      }
      in.x = GenPlain(rng, 1000.0f);
    }

    void GenSqrtT(Rng& rng, void* p, DrawKind kind)
    {
      ScalarIn& in = *static_cast<ScalarIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        in.x = GenFloat(rng);
        return;
      }
      in.x = std::fabs(GenPlain(rng, 1000.0f));
    }

    // ---- both sides --------------------------------------------------------
    //
    // The harness and the thunks already answer with raw u32 bit patterns,
    // so the retail and ours wrappers are the same shape and only the callee
    // differs; FromBits puts the pattern back into the float lane the
    // comparison reads.

    void FAbsRetailT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      ScalarOut& out = *static_cast<ScalarOut*>(q);
      out.ret = FromBits(wm3mathretail::FAbs(in.x));
    }

    void FAbsOursT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      ScalarOut& out = *static_cast<ScalarOut*>(q);
      out.ret = FromBits(wm3mathharness::FAbs(in.x));
    }

    void SqrtRetailT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      ScalarOut& out = *static_cast<ScalarOut*>(q);
      out.ret = FromBits(wm3mathretail::Sqrt(in.x));
    }

    void SqrtOursT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      ScalarOut& out = *static_cast<ScalarOut*>(q);
      out.ret = FromBits(wm3mathharness::Sqrt(in.x));
    }
  } // namespace

  const wmdiff::Target kMathTargets[] = {
    //  name                 va          our source                                  typed in/out
    { "Wm3::Math<float>::FAbs(float)", 0x0040DAB0, "Foundation/Math/Wm3Math.inl:82",
      static_cast<std::uint32_t>(sizeof(ScalarIn)), static_cast<std::uint32_t>(sizeof(ScalarOut)),
      GenAbsT, FAbsRetailT, FAbsOursT },
    { "Wm3::Math<float>::Sqrt(float)", 0x00452FC0, "Foundation/Math/Wm3Math.inl:142",
      static_cast<std::uint32_t>(sizeof(ScalarIn)), static_cast<std::uint32_t>(sizeof(ScalarOut)),
      GenSqrtT, SqrtRetailT, SqrtOursT },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kMathTargets) / sizeof(kMathTargets[0]));
    return kMathTargets;
  }
} // namespace wm3math
