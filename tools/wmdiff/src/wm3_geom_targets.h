#pragma once

// Leaf-function target table for the Wild Magic 3.8 geometry classes that the
// engine actually calls into (Wm3::Box3, Wm3::AxisAlignedBox3).
//
// Split out of targets_wm3.cpp so a class family can grow on its own.
// targets_wm3.cpp chains to this via wm3geom::Targets().

namespace difffuzz
{
  struct Target;
}

namespace wm3geom
{
  const difffuzz::Target* Targets(int& count);
} // namespace wm3geom
