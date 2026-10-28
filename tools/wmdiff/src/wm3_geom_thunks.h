#pragma once

// Retail side of the geometry-class comparison.
//
// Stubs until a retail address is filled in: an untouched output buffer leaves
// main.cpp's 0xCDCDCDCD canary in place, which is how a row reports "not wired
// up yet". See wm3_geom_targets.cpp and the README's address procedure.

#include <cstdint>

namespace wm3geomretail
{
  using u32 = std::uint32_t;

  /// Retail 0x00506010 (address written down in src/sdk/moho/mesh/Mesh.cpp:489).
  /// `this` arrives in EDX and the argument in EDI - not ECX as for the other
  /// thiscall rows in this harness - and the bool result comes back in AL.
  /// No stack arguments; the body pushes and pops ESI only.
  u32 Aabb3TestIntersection(const float* self, const float* other);

  // Retail 0x004FFF00. ECX = `this`, the Vector3[8] output array as the single
  // stack argument (`RET 4`), void return.
  void Box3ComputeVertices(const float* self, float* out24);
} // namespace wm3geomretail
