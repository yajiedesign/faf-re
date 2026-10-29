// The targets the engine never executes.
//
// Scope: `tools/wmdiff/reachable.md` is the ledger; every row of the harness
// table has a reachability verdict there, and this list is the set of rows
// whose verdict is `dead` - the engine code in `src/` cannot reach them.
//
// Two kinds of dead row:
//
//   * every `double` instantiation - `src/` passes no `double` argument to a
//     Wild Magic template anywhere (`double-audit.md`);
//   * the float rows of classes and members the engine never calls - the
//     convex hulls, `Eigen`, `Query2`/`Query3`, the capsule intersectors, the
//     swept distance queries, and the private helpers of intersectors the
//     engine only uses through their unswept entry points.
//
// The rule is the same for both: a `src/` call site that names the member, or
// a member reached through Wild Magic's own calls from such a site, with the
// call site quoted.  The evidence for every row is in `reachable.md`.
//
// They stay in the table: every one of them was verified bit-for-bit against
// the retail at some point, and deleting the rows would throw that away.  The
// harness skips them unless --include-dead is given, so the pass/fail line
// reflects the code the engine actually runs.
#pragma once

namespace wm3scope
{
  bool IsDeadInEngine(const char* name);
}
