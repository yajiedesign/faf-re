#pragma once

#include "fuzz.h"

namespace fafdiff
{
  /// The comparison table. `count` receives the number of rows; the returned
  /// pointer is the first row. Empty until P1 adds the first translation unit
  /// under test.
  const difffuzz::Target* Targets(int& count);
} // namespace fafdiff
