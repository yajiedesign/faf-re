#pragma once

#include "fuzz.h" // difffuzz::Target

namespace wm3math
{
  /// Target table for Wm3::Math<float>'s leaf functions. Same contract as
  /// wm3targets::Targets() in targets_wm3.h: a row is here only once its retail
  /// address is known.
  const difffuzz::Target* Targets(int& count);
} // namespace wm3math
