// Leaf-function targets for Wm3::Vector3<float>.
//
// Same contract as targets_wm3.cpp: a row is here only once its retail address
// is known. The Vector2 block and the Vector3 leaves that are inlined or have
// no rebuild call site are not carried as rows with a zero address - they are
// listed in the README's "What is in scope" section and in
// wmdiff/reachable.md.

#include "wm3_vec_targets.h"

#include "fuzz.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "wm3_harness.h"
#include "wm3_thunks.h"

using difffuzz::FromBits;
using difffuzz::GenEdge;
using difffuzz::GenFloat;
using difffuzz::GenPlain;
using difffuzz::GenVec3;
using difffuzz::Rng;

using u32 = std::uint32_t;

namespace wm3vec
{
  namespace
  {
    // ---- input generators ---------------------------------------------------

    void GenVec3Lanes(Rng& rng, u32* in)
    {
      GenVec3(rng, reinterpret_cast<float*>(in));
    }

    void GenVec3Pair(Rng& rng, u32* in)
    {
      difffuzz::GenVecPair(rng, reinterpret_cast<float*>(in),
        reinterpret_cast<float*>(in) + 3);
    }

    // (The eps generator that used to live here is gone: retail bakes its
    // normalize threshold in as a .rdata constant, so a random eps on our side
    // would only ever report a signature difference.)

    void GenScalar(Rng& rng, u32* in)
    {
      reinterpret_cast<float*>(in)[0] =
        rng.Chance(50) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
    }

    // ---- Vector2 generators (fuzz.h only ships the Vec3 ones) ---------------
    //
    // Same mode split as GenVec3 in gen.cpp, with the lane count dropped to 2.

    void GenVec2(Rng& rng, float* v)
    {
      const u32 mode = rng.Below(100);
      if (mode < 35) { // unit direction
        const float theta = static_cast<float>(rng.Range(0.0, 6.283185307179586));
        v[0] = std::cos(theta);
        v[1] = std::sin(theta);
      } else if (mode < 60) {
        for (int i = 0; i < 2; ++i) {
          v[i] = GenFloat(rng);
        }
      } else if (mode < 75) {
        for (int i = 0; i < 2; ++i) {
          v[i] = GenPlain(rng, 100.0f);
        }
      } else if (mode < 88) { // axis-aligned / signed zeros
        static constexpr float kLane[] = {0.0f, -0.0f, 1.0f, -1.0f};
        for (int i = 0; i < 2; ++i) {
          v[i] = rng.Chance(20) ? GenPlain(rng, 10.0f) : kLane[rng.Below(4)];
        }
      } else { // unit direction at a random magnitude
        const float theta = static_cast<float>(rng.Range(0.0, 6.283185307179586));
        const double mag = std::ldexp(rng.Range(1.0, 2.0), static_cast<int>(rng.Below(200)) - 100);
        v[0] = static_cast<float>(std::cos(theta) * mag);
        v[1] = static_cast<float>(std::sin(theta) * mag);
      }
    }

    // ---- retail side (stubs for now) --------------------------------------

    void DotRetail(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      out[0] = difffuzz::Bits(wm3retail::Dot(a, a + 3));
    }

    void CrossRetail(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      wm3retail::Cross(a, a + 3, reinterpret_cast<float*>(out));
    }

    void LengthSqRetail(const u32* in, u32* out)
    {
      out[0] = difffuzz::Bits(wm3retail::LengthSquared(reinterpret_cast<const float*>(in)));
    }

    void NormalizeRetail(const u32* in, u32* out)
    {
      // The retail thunk normalises in place, so it needs a writable copy of
      // the input lanes; the copy lands in the output block the canary already
      // pre-filled.
      float* lanes = reinterpret_cast<float*>(out);
      std::memcpy(lanes, in, 3 * sizeof(float));
      wm3retail::Normalize(lanes);
    }

    void NormalizeRetRetail(const u32* in, u32* out)
    {
      // The same body, but keeping what it leaves in ST(0): the length.
      float* lanes = reinterpret_cast<float*>(out);
      std::memcpy(lanes, in, 3 * sizeof(float));
      const float fLength = wm3retail::NormalizeRet(lanes);
      out[3] = difffuzz::Bits(fLength);
    }

    void SqrtfBinaryRetail(const u32* in, u32* out)
    {
      out[0] = difffuzz::Bits(wm3retail::SqrtfBinary(FromBits(in[0])));
    }

    // ---- our side (forwards to the real Wild Magic 3.8) --------------------

    void DotOurs(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      out[0] = wm3harness::Dot(a, a + 3);
    }

    void CrossOurs(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      wm3harness::Cross(a, a + 3, reinterpret_cast<float*>(out));
    }

    void LengthSqOurs(const u32* in, u32* out)
    {
      out[0] = wm3harness::LengthSquared(reinterpret_cast<const float*>(in));
    }

    void InstanceDotOurs(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      out[0] = wm3harness::InstanceDot(a, a + 3);
    }

    void InstanceSqLenOurs(const u32* in, u32* out)
    {
      out[0] = wm3harness::InstanceSquaredLength(reinterpret_cast<const float*>(in));
    }

    void InstanceNormalizeOurs(const u32* in, u32* out)
    {
      wm3harness::InstanceNormalize(reinterpret_cast<const float*>(in),
        reinterpret_cast<float*>(out));
    }

    void InstanceNormalizeRetOurs(const u32* in, u32* out)
    {
      const float fLength = wm3harness::InstanceNormalizeRet(
        reinterpret_cast<const float*>(in), reinterpret_cast<float*>(out));
      out[3] = difffuzz::Bits(fLength);
    }

    void InstanceCrossOurs(const u32* in, u32* out)
    {
      const float* a = reinterpret_cast<const float*>(in);
      wm3harness::InstanceCross(a, a + 3, reinterpret_cast<float*>(out));
    }

    void NormalizeOurs(const u32* in, u32* out)
    {
      float* lanes = reinterpret_cast<float*>(out);
      std::memcpy(lanes, in, 3 * sizeof(float));
      wm3harness::Normalize(lanes);
    }



    void SqrtfBinaryOurs(const u32* in, u32* out)
    {
      out[0] = wm3harness::SqrtfBinary(FromBits(in[0]));
    }

  } // namespace

  const difffuzz::Target kTargets[] = {
    //  name                va          our source                                       inW outW inLayout outLayout gen           retail             ours
    { "Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00452AD0, "Foundation/Math/Wm3Vector3.h:166 (retail thiscall, ST0 extended)", 6, 1, "a[3] b[3]", "ret", GenVec3Pair, DotRetail, DotOurs },
    { "Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&) const", 0x00452AD0, "Foundation/Math/Wm3Vector3.inl:277 (same retail body)", 6, 1, "a[3] b[3]", "ret", GenVec3Pair, DotRetail, InstanceDotOurs },
    { "Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)", 0x00452BC0, "Foundation/Math/Wm3Vector3.h:170 (retail hidden return pointer)", 6, 3, "a[3] b[3]", "out[3]", GenVec3Pair, CrossRetail, CrossOurs },
    { "Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&) const", 0x00452BC0, "Foundation/Math/Wm3Vector3.inl:316 (same retail body)", 6, 3, "a[3] b[3]", "out[3]", GenVec3Pair, CrossRetail, InstanceCrossOurs },
    { "Wm3::Vector3<float>::LengthSq(const Wm3::Vector3<float>&)", 0x004757E0, "Foundation/Math/Wm3Vector3.h:176 (retail thiscall, ST0 extended)", 3, 1, "v[3]", "ret", GenVec3Lanes, LengthSqRetail, LengthSqOurs },
    { "Wm3::Vector3<float>::SquaredLength() const", 0x004757E0, "Foundation/Math/Wm3Vector3.inl:268 (same retail body)", 3, 1, "v[3]", "ret", GenVec3Lanes, LengthSqRetail, InstanceSqLenOurs },
    // Indirectly verified through Vec3::Normalize (0x00452AF0): that target
    // passes 1M/1M on seed 0x5EED2007 and its retail body expands Length
    // inline (lengthSq at 0x00452AF6..0x00452B2A, SqrtfBinary call at
    // 0x00452B39). Coverage: every vector from GenVec3; Length is evaluated
    // before the epsilon branch.
    // Also carries the indirect verification of Vec3::Length (see the
    // Vec3::Length row above).
    { "Wm3::Vector3<float>::Normalize(Wm3::Vector3<float>&, float)", 0x00452AF0, "Foundation/Math/Wm3Vector3.h:184 (retail void, eps baked in)", 3, 3, "v[3]", "v[3]", GenVec3Lanes, NormalizeRetail, NormalizeOurs },
    { "Wm3::Vector3<float>::Normalize()", 0x00452AF0, "Foundation/Math/Wm3Vector3.inl:309 (same retail body; the row also compares the length the body leaves in ST(0) at the RET - FSTP ST(1) writes before it pops, so the value FCOMI compared is what survives)", 3, 4, "v[3]", "v[3] len", GenVec3Lanes, NormalizeRetRetail, InstanceNormalizeRetOurs },
    { "Wm3::SqrtfBinary(float)", 0x00452FC0, "Foundation/Math/Wm3Vector3.h:303 (free function, FUN_00452FC0)", 1, 1, "x", "ret", GenScalar, SqrtfBinaryRetail, SqrtfBinaryOurs },
  };

  const difffuzz::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }
} // namespace wm3vec
