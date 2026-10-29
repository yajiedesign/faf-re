#pragma once

// Leaf-function target table for Wm3::Vector3<float>.
//
// Split out of targets_wm3.cpp so the vector leaf set can grow without growing
// the aggregate table. targets_wm3.cpp chains to this via wm3vec::Targets().

namespace wmdiff
{
  struct Target;
}

namespace wm3vec
{
  const wmdiff::Target* Targets(int& count);
} // namespace wm3vec
