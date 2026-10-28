# Where the suite stands

A one-page state of the `wmdiff` harness.  The per-entry ledger is `README.md`;
the reachability data and its queue are `reachable.md`; this file answers "what
is done, what is open, and what would have to be true to resume".

## What it measures

Every entry in `kTargets[]` takes one rebuilt Wild Magic 3.8 function and one
address in the retail `ForgedAlliance_6c6c92f.exe`, calls both with the same
generated inputs, and compares the results **bit for bit**.  The rebuilt side is
the project's own C++ (`dependencies/WildMagic3p8/Foundation/`); the retail side
is a naked-assembly thunk in `wmdiff/src/wm3_coll_thunks.cpp` that sets up the
retail's calling convention and jumps into the mapped image.

Both sides run under **`_PC_24`** (x87 control word `0x007F`), because that is
what the engine sets.  Under `_PC_24` a `double` is a 24-bit significand with the
extended exponent - not binary64 - which is why the harness models x87
arithmetic with hardware instructions rather than with `double`.

## The numbers

| | |
|---|---|
| entries in `kTargets[]` | **156** |
| entries passing 1,000,000 draws on each of five seeds | **156 / 156** |
| duplicate entries | **0** |
| failing entries | **0** |
| symbol-table records from the Ghidra run | **228** |
| of which not Wild Magic functions | **9** - eight compiler-generated closures and one mislabelled duplicate of `Vector2<float>::X() const` |
| **distinct Wild Magic functions** | **219** |
| of those, paired (`已匹配`) | **75** |
| of those, with no entry (`未匹配`) | **0** |
| of those, where no entry is the right answer (`不适用`) | **143** |
| entries with no counterpart among the 219 | **80** |

The seeds are `0x5EED2007`, `0xDEADBEEF`, `0x12345678`, `0xC0FFEE01`,
`0xA5A5A5A5`.  The last full sweep was 156/156 on all five.  A later, text-only
edit to one entry's description forced a rebuild; the entry and the `0x5EED2007`
sweep were re-run after it (1,000,000/1,000,000 and 156/156).

Two counting traps worth knowing, because both were real:

* **228 - 9 = 219, not 220.**  The table used to print "228 entries, 9 not WM3,
  therefore 220 Wild Magic functions".  The 220th was the mislabelled
  `ViewOrientFromLook` record, which is a second copy of `Vector2<float>::X()
  const` at the same source line; counting its canonical name made it look like
  a WM3 function.
* **152 entries and 219 functions are different numbers on purpose.**  The 219
  are what the Ghidra run over the rebuilt **Debug** image found; the 152 entries
  include every `double` entry the pairing work built plus a few `float`
  overloads the census records once.  About 80 entries are therefore
  `linked-but-unreachable` work, kept because they are verified and cost nothing.
  Mechanically checked: all 72 `wmdiff_row` names resolve to an entry in
  `--list`, zero missing.

## Nothing is left unpaired

Every Wild Magic function the engine executes now has a settled answer: **76
carry a passing entry**, and **143 have a documented reason why no entry can
exist**:

| why no entry can exist | count |
|---|---:|
| not executed - the engine has no call site | 57 |
| executed, but the retail inlines it everywhere (no body) | 52 |
| exists only in the rebuild (no retail counterpart) | 21 |
| executed with a body, but a row could not discriminate | 13 |

The queue was declared empty once before, on a census verdict that turned out to
be wrong.  It is empty **now** on evidence: the last eleven - four base
destructors, `~Distance<float,Vector2>` (never executed), and seven
`Matrix3`/`Plane3`/`Sphere3` members the retail inlines - were settled by
reading the retail's `Distance` and `Intersector` blocks and the `CGeomSolid3`
plane-array helpers, not by a probe.

## The census correction, and why it matters

The reachability census decides "does the engine run this" with three rounds,
and round 0 is a **withdrawal probe**: remove the library's explicit
instantiations and see whether `main.vcxproj` still links.  For a **header-inline
template member**, that probe answers the wrong question.  The engine's own
translation units instantiate the members they use, so the link succeeds whether
the class is used or not - "nobody references it" and "everybody references it,
in a header" are indistinguishable.

`Wm3::Quaternion<float>` is the proof.  The probe said *not applicable*; the
engine calls the class in its live paths - `Identity()`
(`src/sdk/moho/ai/CAimManipulator.cpp:1076`), the lane accessors
(`src/sdk/moho/unit/CUnitMotion.cpp:813-816`), `operator==`
(`src/sdk/moho/ai/CAiFormationInstance.cpp:3585`) and `Rotate`
(`src/sdk/moho/entity/CollisionBeamEntity.cpp:71-73`).  **Round 0 may cut
candidates; it can never close one.**

The same round also produced the opposite error: entries were later marked
*pairable* because their **class** was executed, with notes citing `C6`/`C20` -
identifiers that exist nowhere in the repository.  A member is pairable only
when both halves are established: a call site that names **that** member, and a
retail body with an address.  The 14 `not yet established` entries are exactly
the members where the second half is missing, and they are honest about it.

## What the suite actually fixed

**Two** changes have altered what the engine computes.

**`ContAlignedBox<float>` did not write the box's axis.**  The retail's WM3.8
body writes the identity into the axis before computing the centre and extents;
the rebuild's came from the Wild Magic 4 body, which leaves it uninitialised - as
does `Box3`'s default constructor in both the rebuild and the WM3.4 copy.  The
engine's `moho::AxisAlignedBoundsFromBox` (`0x004FE2D0`) reads `aligned.Axis[0][axis]`
and its siblings, so before the fix the engine computed bounds from an
uninitialised axis.  The same entry exposed a harness fault: the retail
`ContAlignedBox<float>` (`0x00A3AE40`) takes a **fourth** argument, the mask
pointer, and the engine's call site pushes `0`; the thunk pushed only three, so
garbage in that slot sent the retail down its mask path and the entry had been
measuring a call the engine never makes.

**`Vector3<Real>::Length()`** - the retail has two length bodies that disagree.
`Length` (`0x00452A90`) computes the three squares with **SSE**;
`SquaredLength` (`0x004757E0`) is **x87**.  They agree in the float normal range
and part company where the squares are denormal.  The rebuild had folded the
second into the first (`Sqrt(SquaredLength())`), wrong for **529 of 200000**
draws.  The fix is in the shared template, so it reaches the engine at
`src/sdk/moho/ai/CAiSteeringImpl.cpp:198` and `:1458`; the lockstep game
(`2p-main-x86-vs-x86`) still ends in **PASS** in 5m19s.

Two more changes were verified and cannot move the engine:

* **`Eigen::Tridiagonal3`** compared against `ZERO_TOLERANCE` instead of zero
  (**23640 of 200000** draws).  Real, fixed, and never executed.
* **`Vector3<float>::CompareArrays` was made public**, because the retail's
  engine calls it by name while the WM3.4 copy has it private.  Code-neutral, and
  proved so: with the specifier reverted, the Release engine rebuilt, then
  restored and rebuilt again, both `.text` sections hash to
  `ed96fca73b7b5be547c2342fe5b02ccc`.  Note that *our* engine never calls the
  member - only the harness does.

## How an entry is trusted

1. **Two-direction mutation.**  Put the rebuild's body back on the wrong
   spelling: the entry must fail, and the failure count is recorded.  Restore it:
   the entry must pass.  An entry that cannot be made to fail is not shipped.
2. **A `bool` entry is a weak witness.**  It flips only when the two sides land
   within one ulp of a threshold, about one draw in a million.  Those entries are
   closed only on **1,000,000 draws across five seeds**.
3. **Check the build, not the shell.**  A failed build leaves the previous exe in
   place, so an old binary can report yesterday's PASS.
4. **No duplicates**, because the seed derives from the entry's index.

## Deliberate limits

* **`double` is not in scope.**  No `src/` line passes a double argument to a
  Wild Magic template, so every double instantiation is `not executed` however
  many entries it has.
* **"Linked" is not "executed".**  The withdrawal probe is a filter only.
* **Only what has a retail body can be paired.**  34 functions are executed and
  the retail inlines them everywhere (`Quaternion<float>::Rotate` is the clearest
  case); 17 exist only in the rebuild.
* **The harness cannot see the engine's own code.**  Differences inside `src/`
  are outside its reach.

## Resuming

* **The queue is empty**, and the four rounds that emptied it are in
  `reachable.md`.  The three `0x004F08xx` members were the ones that proved the
  **register-passing thunk**: the quaternion bodies take their argument in
  `EAX`, so the thunk sets `ECX`/`EAX`, pushes nothing, and calls through `EDX`.
  `Sphere3<float>(const Vector3&, float)` is the one constructor row this library
  can have.  The last eleven were closed as `executed-no-body` or
  `executed-not-pairable` - four base destructors whose rows would compare only
  the vtable pointer, `~Distance<float,Vector2>` which the engine never runs, and
  seven `Matrix3`/`Plane3`/`Sphere3` members the retail inlines.
* **Constructor rows: one is in the table, the rest cannot be built.**  The
  others fail the rule in the README: the `Dist*`/`Intr*` objects hold
  **references** (the compared bytes are the caller's addresses) and their bases
  leave fields uninitialised; the two `Intersector` constructors, once the vtable
  pointer is skipped, leave only `m_fContactTime = 0.0` and `IT_EMPTY`, constants
  that can never disagree.  Do not spend a round re-deriving this.
* **What would reopen the queue**: a Wild Magic function that becomes reachable,
  or a member whose "no body" verdict is overturned - which has already happened
  once (`Sphere3`, and before that `Quaternion`'s class verdict).  Re-verifying a
  negative is cheap; treating one as final is not.
* Regenerating `reachable.md`: it is **generated** - edit
  `.tmp/wmdiff/regen_reachable.py` and re-run it, never the file.
