#include "targets.h"

#include <vector>

namespace fafdiff
{
namespace
{
  // The table of comparison rows. One row is one paired function: the retail
  // entry point, the engine source that implements it, and the generator that
  // feeds both sides the same typed input.
  //
  // The table is empty until P1 - there are no rows yet, so `--list` prints
  // the header and nothing else, and a run has nothing to compare.
  //
  // The rows live in a vector rather than a plain array because an empty
  // initializer list is not a valid array in C++ (MSVC: C2079), and the table
  // starts out empty. Add rows below, one brace group per row.
  const std::vector<Target> kTargets = {
    // rows go here
  };
}  // namespace

const Target* Targets(int& count)
{
  // The aggregator idiom wmdiff uses: each slice's count is fetched in its
  // own statement, because MSVC evaluates the trailing `n` of an argument
  // list before the earlier ones. With one slice it does not matter, but keep
  // the shape so the second slice cannot break it.
  count = static_cast<int>(kTargets.size());
  return kTargets.data();
}
}  // namespace fafdiff
