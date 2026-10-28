#pragma once

// Our side of the geometry-class comparison: forwards to the real Wild Magic
// 3.8 headers, so the code exercised is the code the engine compiles. No copies.

#include <cstdint>

namespace wm3geomharness
{
  using u32 = std::uint32_t;

  /// Wm3::AxisAlignedBox3<float>::TestIntersection, returns 1/0.
  /// `a` and `b` are six floats each: Min[3] then Max[3].
  u32 Aabb3TestIntersection(const float* a, const float* b);

  /// Wm3::Box3<float>::ComputeVertices.
  /// `self` is fifteen floats: Center[3], Axis[3][3], Extent[3].
  /// Writes eight vertices (24 floats) to `out24`.
  void Box3ComputeVertices(const float* self, float* out24);
} // namespace wm3geomharness
