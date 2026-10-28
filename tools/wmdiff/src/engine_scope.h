// The targets whose double instantiation the engine never executes.
//
// tools/wmdiff/double-audit.md established this per target: the rule is the
// argument type at the call site, followed up to src/.  None of these has a
// src/ call site with a double argument, none is reached through another Wild
// Magic double function, and the rebuilt engine carries no double RTTI for the
// Wild Magic polymorphic classes.  They are linked into the binary because the
// library instantiates them explicitly - linked, never executed.
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
