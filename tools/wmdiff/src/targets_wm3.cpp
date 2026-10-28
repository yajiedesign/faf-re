// Aggregate target table for the Wild Magic 3.8 comparison harness.
//
// The rows themselves live in four per-class slices, one per header family:
//
//   wm3vec::Targets()             Wm3Vector3 / Wm3Vector2        (9 rows)
//   wm3math::Targets()            Wm3Math                        (3 rows)
//   wm3geom::Targets()            Wm3Box3 / Wm3AxisAlignedBox3    (2 rows)
//   wm3coll::Targets()            Wm3 Intersection / Distance    (16 rows)
//
// Splitting them keeps each class's harness, retail stubs and generators
// together and lets them be extended independently. This file only chains the
// slices into one contiguous array, because main.cpp walks a single table.
//
// A row is in the table only once its retail address is known. The functions
// that are reachable but have no retail address yet - the Vector2 block, most
// of Math, the Quaternion block - are not carried here as rows with a zero
// address: they would report a mismatch against the canary on every draw and
// drown the rows that mean something. They are listed instead in the
// README's "What is in scope" section and in wmdiff/reachable.md, which is
// generated from the reachability analysis.
//
// Retail addresses are established with Ghidra (the IDA export under decomp/ is
// not present on this machine). When one is known, put it in that row's `va` and
// give the matching thunk in the class's *_thunks.cpp a naked call through that
// entry point, mirroring tools/difffuzz/src/retail_thunks.cpp.

#include "targets_wm3.h"

#include "fuzz.h"

#include "wm3_coll_targets.h"
#include "wm3_geom_targets.h"
#include "wm3_math_targets.h"
#include "wm3_vec_targets.h"

#include <vector>

namespace wm3targets
{
  namespace
  {
    // Held by value, not by pointer: difffuzz::Target is a plain aggregate of
    // scalars and function pointers, and this keeps Targets() free of casts -
    // a vector of pointers would hand back `const Target**`, which does not
    // implicitly decay to the `const Target*` this function returns.
    using Slice = std::vector<difffuzz::Target>;

    void Append(Slice& merged, const difffuzz::Target* rows, const int n)
    {
      for (int i = 0; i < n; ++i) {
        merged.push_back(rows[i]);
      }
    }
  } // namespace

  const difffuzz::Target* Targets(int& count)
  {
    // Assembled once, then handed out as one contiguous block. main.cpp treats
    // the table as read-only for the whole run.
    static Slice merged;

    if (merged.empty()) {
      int n = 0;
      // Each slice is fetched into its own statement on purpose. Writing
      // Append(merged, Slice::Targets(n), n) would leave the count's read
      // order unspecified, and MSVC evaluates the trailing `n` first - it then
      // holds the *previous* slice's count, so the last append reads far past
      // the end of its array.
      const difffuzz::Target* rows = wm3vec::Targets(n); // Wm3Vector3 / Wm3Vector2
      Append(merged, rows, n);

      rows = wm3math::Targets(n); // Wm3Math
      Append(merged, rows, n);

      rows = wm3geom::Targets(n); // Wm3Box3 / Wm3AxisAlignedBox3
      Append(merged, rows, n);

      rows = wm3coll::Targets(n); // Wm3 Intersection / Distance classes
      Append(merged, rows, n);
    }

    count = static_cast<int>(merged.size());
    return merged.data();
  }
} // namespace wm3targets
