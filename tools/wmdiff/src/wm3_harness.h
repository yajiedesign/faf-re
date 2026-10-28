#pragma once

// Our side of the comparison harness.
//
// Unlike tools/difffuzz - which has to mirror engine sources verbatim because
// its build compiles only a hand-picked whitelist of translation units, from
// arbitrary include paths - this harness lives inside the WildMagic3p8 tree
// and links the real headers. So nothing here is a copy: every function below
// forwards to the actual Wm3 implementation, and the code under test is the
// same source the engine compiles.

#include <cstdint>

namespace wm3harness
{
  using u32 = std::uint32_t;

  // Leaf predicates and metrics.
  u32 Dot(const float* a, const float* b);
  void Cross(const float* a, const float* b, float* out);
  u32 LengthSquared(const float* v);
  void Normalize(float* v);

  // Upstream instance forms, which take the same retail bodies as the static
  // helpers above (0x00452AD0 and 0x004757E0) but exercise the .inl code.
  u32 InstanceDot(const float* a, const float* b);
  u32 InstanceSquaredLength(const float* v);
  void InstanceNormalize(const float* v, float* out);
  // The same call, keeping the length the member returns.
  float InstanceNormalizeRet(const float* v, float* out);
  void InstanceCross(const float* a, const float* b, float* out);

  // Free function, not a member (Wm3Vector3.h:303).
  u32 SqrtfBinary(float x);
} // namespace wm3harness
