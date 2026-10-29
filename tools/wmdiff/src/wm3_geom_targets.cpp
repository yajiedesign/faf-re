// Leaf-function targets for the Wild Magic 3.8 geometry classes the engine
// calls: Wm3::AxisAlignedBox3<float> and Wm3::Box3<float>.
//
// Every row here is justified by a real rebuild call site, per the README's
// "no rebuild call site means no emitted body" rule:
//
//   AxisAlignedBox3::TestIntersection  src/sdk/moho/mesh/Mesh.cpp:502,513,1370
//   Box3::ComputeVertices             src/sdk/moho/math/
//                                      Wm3AxisAlignedBox3FafExtras.cpp:25 and
//                                      src/sdk/moho/render/
//                                      SelectionBracketRenderer.cpp:299.
//                                      Box3::GetCorners
//                                      (src/sdk/moho/render/d3d/
//                                      CD3DPrimBatcher.cpp:1286) is a one-line
//                                      forward to the same body
//                                      (Wm3Box3.h:65), so it is a second
//                                      caller of this row, not a second row.
//
// Both rows are on the typed path: the two sides are handed the same typed
// input struct and answer with a typed output struct, and the harness only
// ever sees the byte buffers behind them.

#include "wm3_geom_targets.h"

#include "fuzz.h"

#include <cstring>

#include "wm3_geom_harness.h"
#include "wm3_geom_thunks.h"

using wmdiff::DrawKind;
using wmdiff::GenEdge;
using wmdiff::GenFloat;
using wmdiff::GenPlain;
using wmdiff::Rng;

namespace wm3geom
{
  namespace
  {
    // ---- typed row shapes --------------------------------------------------
    //
    // The lanes are plain floats, not Wm3::Vector3<float> / Wm3::Box3<float>:
    // the retail thunk takes the *address* of the lanes (`const float*`), and
    // so does our harness forward - which is where the Wm3 classes are built
    // and the real members called.

    struct Aabb3PairIn
    {
      float a[6];   // Min[3] then Max[3]
      float b[6];   // Min[3] then Max[3]
    };

    struct BoolOut
    {
      std::uint32_t ret;   // 1 or 0
    };

    struct Box3In
    {
      // The order Wm3Box3 stores them in (Wm3Box3.h: Center, Axis, Extent).
      float center[3];
      float axis[3][3];
      float extent[3];
    };

    struct VerticesOut
    {
      float vertex[8][3];
    };

    // ---- typed generators --------------------------------------------------
    //
    // WithExtremes draws exactly what the word path drew, in exactly the
    // same order: the same helpers, the same arguments, the same sequence of
    // RNG draws. The same seed therefore still produces the same input bytes,
    // so the comparison stays bit for bit - that is what makes the conversion
    // provably harmless rather than merely plausible.
    //
    // Ordinary is the --skip-extreme draw: every lane is finite, in range and
    // normal - no NaN, no infinity, no subnormal - so a typed row always has
    // a comparable input and can never be skipped.

    void GenAabb3PairT(Rng& rng, void* p, DrawKind kind)
    {
      Aabb3PairIn& in = *static_cast<Aabb3PairIn*>(p);
      // Half the time the pair is generated well-formed (Min <= Max on every
      // axis, which is what a real bounding box looks like) and half the time
      // the two boxes are independent random draws. Ordered input alone would
      // leave every comparison `Max[i] < rkBox.Min[i]` either clearly true or
      // clearly false; the interesting cases are the near-touching and
      // inverted ones, where the answer flips on the last bit of a lane.
      if (kind == DrawKind::WithExtremes) {
        if (rng.Chance(50)) {
          for (int box = 0; box < 2; ++box) {
            float* const f = (box == 0) ? in.a : in.b;
            float centre[3];
            wmdiff::GenVec3(rng, centre);
            for (int axis = 0; axis < 3; ++axis) {
              const float half = GenPlain(rng, 100.0f);
              f[axis] = centre[axis] - half;
              f[3 + axis] = centre[axis] + half;
            }
          }
        }
        else {
          for (int i = 0; i < 12; ++i) {
            float* const f = (i < 6) ? in.a : in.b;
            f[i % 6] = rng.Chance(70) ? GenPlain(rng, 1000.0f) : GenFloat(rng);
          }
        }
        return;
      }
      // Ordinary: the same two shapes, but every lane is plain.
      if (rng.Chance(50)) {
        for (int box = 0; box < 2; ++box) {
          float* const f = (box == 0) ? in.a : in.b;
          float centre[3];
          for (int axis = 0; axis < 3; ++axis) {
            centre[axis] = GenPlain(rng, 100.0f);
          }
          for (int axis = 0; axis < 3; ++axis) {
            const float half = GenPlain(rng, 100.0f);
            f[axis] = centre[axis] - half;
            f[3 + axis] = centre[axis] + half;
          }
        }
      }
      else {
        for (int i = 0; i < 12; ++i) {
          float* const f = (i < 6) ? in.a : in.b;
          f[i % 6] = GenPlain(rng, 1000.0f);
        }
      }
    }

    void GenBox3T(Rng& rng, void* p, DrawKind kind)
    {
      Box3In& in = *static_cast<Box3In*>(p);
      if (kind == DrawKind::WithExtremes) {
        wmdiff::GenVec3(rng, in.center);

        if (rng.Chance(66)) {
          // A signed permutation matrix: orthonormal, and cheap to keep exact.
          const int axis = static_cast<int>(rng.Below(3));
          for (int i = 0; i < 3; ++i) {
            float* row = in.axis[i];
            row[0] = row[1] = row[2] = 0.0f;
            row[axis] = rng.Chance(50) ? 1.0f : -1.0f;
          }
        }
        else {
          for (int i = 0; i < 9; ++i) {
            in.axis[i / 3][i % 3] = rng.Chance(70) ? GenPlain(rng, 10.0f) : GenFloat(rng);
          }
        }

        for (int i = 0; i < 3; ++i) {
          in.extent[i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
        }
        return;
      }
      // Ordinary: the same shapes, but no extreme lane anywhere. The axis
      // stays a signed permutation most of the time, because that is what an
      // oriented bounding box actually carries.
      for (int i = 0; i < 3; ++i) {
        in.center[i] = GenPlain(rng, 100.0f);
      }
      if (rng.Chance(66)) {
        const int axis = static_cast<int>(rng.Below(3));
        for (int i = 0; i < 3; ++i) {
          float* row = in.axis[i];
          row[0] = row[1] = row[2] = 0.0f;
          row[axis] = rng.Chance(50) ? 1.0f : -1.0f;
        }
      }
      else {
        for (int i = 0; i < 9; ++i) {
          in.axis[i / 3][i % 3] = GenPlain(rng, 10.0f);
        }
      }
      for (int i = 0; i < 3; ++i) {
        in.extent[i] = GenPlain(rng, 1000.0f);
      }
    }

    // ---- both sides --------------------------------------------------------

    void Aabb3TestIntersectionRetailT(const void* p, void* q)
    {
      const Aabb3PairIn& in = *static_cast<const Aabb3PairIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3geomretail::Aabb3TestIntersection(in.a, in.b);
    }

    void Aabb3TestIntersectionOursT(const void* p, void* q)
    {
      const Aabb3PairIn& in = *static_cast<const Aabb3PairIn*>(p);
      BoolOut& out = *static_cast<BoolOut*>(q);
      out.ret = wm3geomharness::Aabb3TestIntersection(in.a, in.b);
    }

    void Box3ComputeVerticesRetailT(const void* p, void* q)
    {
      const Box3In& in = *static_cast<const Box3In*>(p);
      VerticesOut& out = *static_cast<VerticesOut*>(q);
      wm3geomretail::Box3ComputeVertices(&in.center[0], &out.vertex[0][0]);
    }

    void Box3ComputeVerticesOursT(const void* p, void* q)
    {
      const Box3In& in = *static_cast<const Box3In*>(p);
      VerticesOut& out = *static_cast<VerticesOut*>(q);
      wm3geomharness::Box3ComputeVertices(&in.center[0], &out.vertex[0][0]);
    }
  } // namespace

  const wmdiff::Target kTargets[] = {
    { "Wm3::AxisAlignedBox3<float>::TestIntersection(const Wm3::AxisAlignedBox3<float>&) const", 0x00506010,
      "Foundation/Math/Wm3AxisAlignedBox3.inl:49 (retail: this in EDX, arg in EDI, bool in AL)",
      0, 0, "a[6] b[6]", "ret", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(Aabb3PairIn)), static_cast<std::uint32_t>(sizeof(BoolOut)),
      GenAabb3PairT, Aabb3TestIntersectionRetailT, Aabb3TestIntersectionOursT },
    { "Wm3::Box3<float>::ComputeVertices(Wm3::Vector3<float>*) const", 0x004FFF00,
      "Foundation/Math/Wm3Box3.inl:47 (retail thiscall, void, out array on the stack)",
      0, 0, "center[3] axis[9] extent[3]", "vertex[8][3]", nullptr, nullptr, nullptr,
      static_cast<std::uint32_t>(sizeof(Box3In)), static_cast<std::uint32_t>(sizeof(VerticesOut)),
      GenBox3T, Box3ComputeVerticesRetailT, Box3ComputeVerticesOursT },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }
} // namespace wm3geom
