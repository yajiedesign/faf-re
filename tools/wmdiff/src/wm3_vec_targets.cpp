// Leaf-function targets for Wm3::Vector3<float>.
//
// Same contract as targets_wm3.cpp: a row is here only once its retail address
// is known. The Vector2 block and the Vector3 leaves that are inlined or have
// no rebuild call site are not carried as rows with a zero address - they are
// listed in the README's "What is in scope" section and in
// wmdiff/reachable.md.
//
// Every row here is on the typed path: the two sides are handed the same
// typed input struct and answer with a typed output struct, and the harness
// only ever sees the byte buffers behind them.

#include "wm3_vec_targets.h"

#include "fuzz.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "wm3_harness.h"
#include "wm3_thunks.h"

using wmdiff::DrawKind;
using wmdiff::FromBits;
using wmdiff::GenFloat;
using wmdiff::GenPlain;
using wmdiff::Rng;

using u32 = std::uint32_t;

namespace wm3vec
{
  namespace
  {
    // ---- typed row shapes --------------------------------------------------
    //
    // The lanes are plain floats rather than Wm3::Vector3<float>: the retail
    // thunk takes the *address* of the lanes (`const float*`), and so does our
    // harness forward - which is where the Wm3::Vector3<float> is built and
    // the real member called. Wrapping the lanes in the class here would only
    // add a copy on the way in and out.

    struct Vec3In
    {
      float v[3];      // one vector
    };

    struct PairIn
    {
      float a[3];      // two vectors, the two arguments
      float b[3];
    };

    struct ScalarIn
    {
      float x;
    };

    struct FloatOut
    {
      float ret;       // a scalar answer
    };

    struct Vec3Out
    {
      float v[3];      // a vector answer
    };

    // ---- typed generators --------------------------------------------------
    //
    // WithExtremes draws exactly what the word path drew, in exactly the same
    // order: the same helper, the same arguments, the same sequence of RNG
    // draws. The same seed therefore still produces the same input bytes, so
    // the comparison stays bit for bit - that is what makes the conversion
    // provably harmless rather than merely plausible.
    //
    // Ordinary is the --skip-extreme draw: every lane is finite, in range and
    // normal - no NaN, no infinity, no subnormal - so a typed row always has
    // a comparable input and can never be skipped. It consumes a different
    // number of draws; the two kinds are never mixed in one run.

    void GenPairT(Rng& rng, void* p, DrawKind kind)
    {
      PairIn& in = *static_cast<PairIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVecPair(rng, in.a, in.b);
        return;
      }
      // GenPlain draws uniformly from [-lim, lim] with binary64 resolution,
      // so the smallest non-zero magnitude it can produce is far above the
      // subnormal range and its ends are far below the overflow one.
      for (int i = 0; i < 3; ++i) {
        in.a[i] = GenPlain(rng, 100.0f);
      }
      for (int i = 0; i < 3; ++i) {
        in.b[i] = GenPlain(rng, 100.0f);
      }
    }

    void GenVec3T(Rng& rng, void* p, DrawKind kind)
    {
      Vec3In& in = *static_cast<Vec3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.v);
        return;
      }
      for (int i = 0; i < 3; ++i) {
        in.v[i] = GenPlain(rng, 100.0f);
      }
    }

    void GenScalarT(Rng& rng, void* p, DrawKind kind)
    {
      ScalarIn& in = *static_cast<ScalarIn*>(p);
      if (kind == DrawKind::WithExtremes) {
        in.x = rng.Chance(50) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
        return;
      }
      // The square root of a negative is a NaN on both sides, and an ordinary
      // draw should not produce a NaN answer, so the lane is non-negative.
      in.x = std::fabs(GenPlain(rng, 1000.0f));
    }

    // ---- retail side (the existing thunks) ---------------------------------

    void DotRetailT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3retail::Dot(in.a, in.b);
    }

    void CrossRetailT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      Vec3Out& out = *static_cast<Vec3Out*>(q);
      wm3retail::Cross(in.a, in.b, out.v);
    }

    void LengthSqRetailT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3retail::LengthSquared(in.v);
    }

    void NormalizeRetailT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      Vec3Out& out = *static_cast<Vec3Out*>(q);
      // The retail thunk normalises in place, so it needs a writable copy of
      // the input lanes; the copy lands in the output block the canary
      // already pre-filled.
      std::memcpy(out.v, in.v, sizeof out.v);
      wm3retail::Normalize(out.v);
    }

    void SqrtfBinaryRetailT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = wm3retail::SqrtfBinary(in.x);
    }

    // ---- our side (the harness forwards to the real Wild Magic 3.8) --------

    void DotOursT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3harness::Dot(in.a, in.b));
    }

    void InstanceDotOursT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3harness::InstanceDot(in.a, in.b));
    }

    void CrossOursT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      Vec3Out& out = *static_cast<Vec3Out*>(q);
      wm3harness::Cross(in.a, in.b, out.v);
    }

    void InstanceCrossOursT(const void* p, void* q)
    {
      const PairIn& in = *static_cast<const PairIn*>(p);
      Vec3Out& out = *static_cast<Vec3Out*>(q);
      wm3harness::InstanceCross(in.a, in.b, out.v);
    }

    void LengthSqOursT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3harness::LengthSquared(in.v));
    }

    void InstanceSqLenOursT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3harness::InstanceSquaredLength(in.v));
    }

    void NormalizeOursT(const void* p, void* q)
    {
      const Vec3In& in = *static_cast<const Vec3In*>(p);
      Vec3Out& out = *static_cast<Vec3Out*>(q);
      std::memcpy(out.v, in.v, sizeof out.v);
      wm3harness::Normalize(out.v);
    }

    void SqrtfBinaryOursT(const void* p, void* q)
    {
      const ScalarIn& in = *static_cast<const ScalarIn*>(p);
      FloatOut& out = *static_cast<FloatOut*>(q);
      out.ret = FromBits(wm3harness::SqrtfBinary(in.x));
    }
  } // namespace

  const wmdiff::Target kTargets[] = {
    //  name                va          our source                                       inW outW inLayout outLayout gen retail ours
    { "Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00452AD0, "Foundation/Math/Wm3Vector3.h:166 (retail thiscall, ST0 extended)", 0, 0, "a[3] b[3]", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(PairIn)), static_cast<std::uint32_t>(sizeof(FloatOut)), GenPairT, DotRetailT, DotOursT },
    { "Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&) const", 0x00452AD0, "Foundation/Math/Wm3Vector3.inl:277 (same retail body)", 0, 0, "a[3] b[3]", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(PairIn)), static_cast<std::uint32_t>(sizeof(FloatOut)), GenPairT, DotRetailT, InstanceDotOursT },
    { "Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00452BC0, "Foundation/Math/Wm3Vector3.h:170 (retail hidden return pointer)", 0, 0, "a[3] b[3]", "out[3]", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(PairIn)), static_cast<std::uint32_t>(sizeof(Vec3Out)), GenPairT, CrossRetailT, CrossOursT },
    { "Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&) const", 0x00452BC0, "Foundation/Math/Wm3Vector3.inl:316 (same retail body)", 0, 0, "a[3] b[3]", "out[3]", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(PairIn)), static_cast<std::uint32_t>(sizeof(Vec3Out)), GenPairT, CrossRetailT, InstanceCrossOursT },
    { "Wm3::Vector3<float>::LengthSq(const Wm3::Vector3<float>&)", 0x004757E0, "Foundation/Math/Wm3Vector3.h:176 (retail thiscall, ST0 extended)", 0, 0, "v[3]", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(Vec3In)), static_cast<std::uint32_t>(sizeof(FloatOut)), GenVec3T, LengthSqRetailT, LengthSqOursT },
    { "Wm3::Vector3<float>::SquaredLength() const", 0x004757E0, "Foundation/Math/Wm3Vector3.inl:268 (same retail body)", 0, 0, "v[3]", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(Vec3In)), static_cast<std::uint32_t>(sizeof(FloatOut)), GenVec3T, LengthSqRetailT, InstanceSqLenOursT },
    // Indirectly verified through Vec3::Normalize (0x00452AF0): that target
    // passes 1M/1M on seed 0x5EED2007 and its retail body expands Length
    // inline (lengthSq at 0x00452AF6..0x00452B2A, SqrtfBinary call at
    // 0x00452B39). Coverage: every vector from GenVec3; Length is evaluated
    // before the epsilon branch.
    // Also carries the indirect verification of Vec3::Length (see the
    // Vec3::Length row above).
    { "Wm3::Vector3<float>::Normalize(Wm3::Vector3<float>&, float)", 0x00452AF0, "Foundation/Math/Wm3Vector3.h:184 (retail void, eps baked in)", 0, 0, "v[3]", "v[3]", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(Vec3In)), static_cast<std::uint32_t>(sizeof(Vec3Out)), GenVec3T, NormalizeRetailT, NormalizeOursT },
    { "Wm3::SqrtfBinary(float)", 0x00452FC0, "Foundation/Math/Wm3Vector3.h:303 (free function, FUN_00452FC0)", 0, 0, "x", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(ScalarIn)), static_cast<std::uint32_t>(sizeof(FloatOut)), GenScalarT, SqrtfBinaryRetailT, SqrtfBinaryOursT },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }
} // namespace wm3vec
