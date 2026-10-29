#pragma once

// Leaf-function target table for the Wild Magic 3.8 collision and distance
// classes the engine calls (Foundation/Intersection, Foundation/Distance).
//
// Split out of targets_wm3.cpp so a class family can grow on its own;
// targets_wm3.cpp chains to this via wm3coll::Targets().

namespace wmdiff
{
  struct Target;
}

namespace wm3coll
{
  const wmdiff::Target* Targets(int& count);
} // namespace wm3coll
