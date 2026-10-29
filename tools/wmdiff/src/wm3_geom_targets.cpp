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

#include "wm3_geom_targets.h"

#include "fuzz.h"

#include <cstring>

#include "wm3_geom_harness.h"
#include "wm3_geom_thunks.h"

using wmdiff::GenEdge;
using wmdiff::GenPlain;

namespace wm3geom
{
  namespace
  {
    // Two AABBs as Min[3] then Max[3], twelve floats.
    //
    // Half the time the pair is generated well-formed (Min <= Max on every axis,
    // which is what a real bounding box looks like) and half the time the two
    // boxes are independent random draws. Ordered input alone would leave every
    // comparison `Max[i] < rkBox.Min[i]` either clearly true or clearly false;
    // the interesting cases are the near-touching and inverted ones, where the
    // answer flips on the last bit of a lane.
    void GenAabb3Pair(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      if (rng.Chance(50)) {
        for (int box = 0; box < 2; ++box) {
          float centre[3];
          wmdiff::GenVec3(rng, centre);
          for (int axis = 0; axis < 3; ++axis) {
            const float half = GenPlain(rng, 100.0f);
            f[box * 6 + axis] = centre[axis] - half;
            f[box * 6 + 3 + axis] = centre[axis] + half;
          }
        }
      } else {
        for (int i = 0; i < 12; ++i) {
          f[i] = rng.Chance(70) ? GenPlain(rng, 1000.0f) : wmdiff::GenFloat(rng);
        }
      }
    }

    // A Box3 as Center[3], Axis[3][3], Extent[3] - fifteen floats, in the order
    // Wm3Box3 stores them (Wm3Box3.h: Center, Axis, Extent).
    //
    // Two thirds orthonormal axes, because that is what an oriented bounding box
    // actually carries and it keeps the vertex arithmetic in a sane range; one
    // third arbitrary axes and extents, including degenerate and edge values,
    // to reach the overflow and subnormal paths the chained adds can hit.
    void GenBox3(wmdiff::Rng& rng, std::uint32_t* in)
    {
      float* f = reinterpret_cast<float*>(in);
      wmdiff::GenVec3(rng, f);

      if (rng.Chance(66)) {
        // A signed permutation matrix: orthonormal, and cheap to keep exact.
        const int axis = static_cast<int>(rng.Below(3));
        for (int i = 0; i < 3; ++i) {
          float* row = f + 3 + 3 * i;
          row[0] = row[1] = row[2] = 0.0f;
          row[axis] = rng.Chance(50) ? 1.0f : -1.0f;
        }
      } else {
        for (int i = 0; i < 9; ++i) {
          f[3 + i] = rng.Chance(70) ? GenPlain(rng, 10.0f) : wmdiff::GenFloat(rng);
        }
      }

      for (int i = 0; i < 3; ++i) {
        f[12 + i] = rng.Chance(75) ? GenPlain(rng, 1000.0f) : GenEdge(rng);
      }
    }

    void Aabb3TestIntersectionRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3geomretail::Aabb3TestIntersection(reinterpret_cast<const float*>(in),
                                                     reinterpret_cast<const float*>(in) + 6);
    }

    void Aabb3TestIntersectionOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      out[0] = wm3geomharness::Aabb3TestIntersection(reinterpret_cast<const float*>(in),
                                                     reinterpret_cast<const float*>(in) + 6);
    }

    void Box3ComputeVerticesRetail(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3geomretail::Box3ComputeVertices(reinterpret_cast<const float*>(in),
                                         reinterpret_cast<float*>(out));
    }

    void Box3ComputeVerticesOurs(const std::uint32_t* in, std::uint32_t* out)
    {
      wm3geomharness::Box3ComputeVertices(reinterpret_cast<const float*>(in),
                                           reinterpret_cast<float*>(out));
    }
  } // namespace

  const wmdiff::Target kTargets[] = {
    { "Wm3::AxisAlignedBox3<float>::TestIntersection(const Wm3::AxisAlignedBox3<float>&) const", 0x00506010,
      "Foundation/Math/Wm3AxisAlignedBox3.inl:49 (retail: this in EDX, arg in EDI, bool in AL)",
      12, 1, "a[6] b[6]", "ret", GenAabb3Pair,
      Aabb3TestIntersectionRetail, Aabb3TestIntersectionOurs },
    { "Wm3::Box3<float>::ComputeVertices(Wm3::Vector3<float>*) const", 0x004FFF00,
      "Foundation/Math/Wm3Box3.inl:47 (retail thiscall, void, out array on the stack)",
      15, 24, "center[3] axis[9] extent[3]", "vertex[8][3]", GenBox3,
      Box3ComputeVerticesRetail, Box3ComputeVerticesOurs },
  };

  const wmdiff::Target* Targets(int& count)
  {
    count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
    return kTargets;
  }
} // namespace wm3geom
