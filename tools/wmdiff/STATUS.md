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
| entries the harness runs by default | **46** |
| entries the engine never executes - skipped unless `--include-dead` | **110** |
| default run: pass / fail | **46 / 0** |
| whole table (`--include-dead`): pass / fail | **85 / 71** |
| duplicate entries | **0** |
| of the 71 whole-table failures: `double` rows | **47** |
| of the 71 whole-table failures: `float` rows | **24** |
| of the 71 whole-table failures: rows the engine executes | **0** |

**The live surface is clean.**  A default run is **46/46**, and it stays 46/46
at 1,000,000 draws on each of the five seeds `0x5EED2007`, `0xDEADBEEF`,
`0x12345678`, `0xC0FFEE01`, `0xA5A5A5A5`.  Before the two x87 fixes the same
default run was **34 / 46**: the 12 failures were the two `Dot` rows, the two
`LengthSq`/`SquaredLength` rows, `DistVector3Box3f::GetSquared`,
`DistVector3Segment3f::Get`/`GetSquared`, `IntrLine3Box3f::Test`,
`IntrSegment3Box3f::Find`/`GetPoint`, `IntrBox3Sphere3f::Test` and the masked
`ContAlignedBox` - all of them codegen differences, closed by the submodule
commits `59f1614` and `9af34dc`.

**Before the reachability work the split was 75 / 109.**  The harness skipped
only the 47 `double` rows, so a default run exercised 109 rows - 75 passing and
34 failing.  It now exercises **46** rows: 63 rows the engine cannot reach -
59 `float` rows and the 4 `double` rows the old skip list had missed - were
being compared on every run, and their verdicts said nothing about the engine.
The dead rows stay in the table - they were verified bit for bit at some point
and they cost nothing to keep - but they are skipped by default, so the
pass/fail line is about the code the engine actually runs.

The whole table (`--include-dead`, 20,000 draws) is **85 / 156**.  Every one of
the 71 failures is a **dead** row: the engine never executes it.  They are
listed with the `失败` status in `reachable.md` because that is what the
`--include-dead` run says, not because anything the game runs is wrong.

**The 156 entries and the "how many Wild Magic functions are there" question
are different questions now.**  The old figure - 219 functions, from the Ghidra
run over the *old* rebuild in `.tmp/WildMagic3p8-re` - described a tree that is
no longer here: the tree is now the upstream Wild Magic 3.8 submodule and the
engine is the merged `src/`.  Reachability is now settled **per table row**, and
`reachable.md` is the ledger: one row per target, with the reachability
verdict, the evidence, and the harness verdict.

## What the default run covers

**A function the main project does not call does not need a comparison
entry.**  The audit behind `reachable.md` decided all 156 rows:

| verdict | rows | meaning |
|---|---:|---|
| `executed` | **46** | a `src/` call site names the member, or Wild Magic calls it from a member that a `src/` call site reaches - the call site and the chain are quoted |
| `dead` | **110** | no `src/` call site and no reached Wild Magic caller - the evidence is the `grep` that found nothing |
| `unsettled` | **0** | - |

The 110 dead rows are two islands: **51** `double` rows (`src/` passes no
`double` argument to a Wild Magic template anywhere - `double-audit.md`) and
**59** `float` rows (the convex hulls, `Eigen`, `Query2`/`Query3`, the capsule
intersectors, the swept distance queries, the `Distance` base class's Newton
driver, and the private helpers of intersectors the engine only enters through
their unswept entry points).

All 46 rows that run by default pass.  They used to be 34 passing and 12
failing; the 12 failures were the ones the float analysis documents already
attributed: the two `Dot` rows, the two `LengthSq`/`SquaredLength` rows,
`DistVector3Box3f::GetSquared`, `DistVector3Segment3f::Get`/`GetSquared`,
`IntrLine3Box3f::Test`, `IntrSegment3Box3f::Find`/`GetPoint`,
`IntrBox3Sphere3f::Test` and the masked `ContAlignedBox`.  All of them were
**codegen** differences (the retail's x87 register file against the rebuild's
binary32 SSE), not source differences; the three that reach the game are the
ones `x87math-usage.md` names.  The two submodule commits `59f1614` and
`9af34dc` reproduce the retail's x87 chains for them; the per-row mechanism is
in `float-analysis-ordinary.md` under `## Resolution`.

**A new row must be audited before it is added.**  A row that is not in
`reachable.md` has no reachability verdict, so the harness cannot tell whether
the engine runs it.

## The census correction, and why it matters

The reachability census decides "does the engine run this" with three rounds,
and round 0 is a **withdrawal probe**: remove the library's explicit
instantiations and see whether the engine still links.  For a **header-inline
template member**, that probe answers the wrong question.  The engine's own
translation units instantiate the members they use, so the link succeeds whether
the class is used or not - "nobody references it" and "everybody references it,
in a header" are indistinguishable.

`Wm3::Quaternion<float>` is the proof.  The probe said *not applicable*; the
engine calls the class in its live paths - `Identity()`
(`src/moho/ai/CAimManipulator.cpp`), the lane accessors
(`src/moho/unit/CUnitMotion.cpp`), `operator==`
(`src/moho/ai/CAiFormationInstance.cpp`) and `Rotate`
(`src/moho/entity/CollisionBeamEntity.cpp`).  **Round 0 may cut candidates; it
can never close one.**

The same round also produced the opposite error: entries were later marked
*pairable* because their **class** was executed, with notes citing `C6`/`C20` -
identifiers that exist nowhere in the repository.  A member is pairable only
when both halves are established: a call site that names **that** member, and a
retail body with an address.

The current audit in `reachable.md` replaces the census with a per-row
evidence-based verdict, and it keeps the lesson: **"the class is executed" is
not "the member is executed."**  The engine calls `IntrBox2Circle2f::Test()`
and never the swept `Find`; it calls `IntrSegment3Sphere3f::Find()` and never
`Test`; it calls `IntrSegment3Box3f::Find()` and never `Test`.

## What the suite actually fixed

**Four** changes have altered what the engine computes.

**The x87 float chains (submodule `59f1614`, `9af34dc`).**  The retail's
`float` bodies are x87 code, not SSE: the intermediate sums and products live
on the x87 register file, where the exponent range is extended and the
significand is 24 bits.  The rebuild compiled them as binary32 SSE, so the
results parted company outside the float normal range.  The two commits
reproduce the retail's chains:

| function | retail address | mechanism | commit |
|---|---|---|---|
| `Vector3<float>::Dot` | `0x00452AD0` | x87 sum order and 24-bit rounding | `59f1614` |
| `Vector3<float>::LengthSq` / `SquaredLength` | `0x004757E0` | x87 squares, extended exponent | `59f1614` |
| `DistVector3Segment3<float>::Get` / `GetSquared` | `0x00A480E0` / `0x00A484F0` | the inlined dot chain | `59f1614` |
| `IntrLine3Box3<float>::Test` | `0x00A4FE60` | x87 clip arithmetic | `9af34dc` |
| `IntrSegment3Box3<float>::Find` / `GetPoint` | `0x00A462A0` / `0x00A46300` | the x87 line-box chain | `9af34dc` |
| `IntrBox3Sphere3<float>::Test` | `0x00A41560` | x87 radius comparison | `9af34dc` |
| `ContAlignedBox<float>` (masked call shape) | `0x00A3AE40` | the mask byte test | `9af34dc` |

The helper header is `Foundation/Math/Wm3X87.h` (`RoundTo24`, `Mul24`,
`Add24`, `Sub24`, `Div24`).  With those in place the default run is **46/46**
at 1,000,000 draws on all five seeds.  The per-row account of what was wrong
and what changed is in `float-analysis-ordinary.md` under `## Resolution`.

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
`src/moho/ai/CAiSteeringImpl.cpp:198` and `:1458`; the lockstep game
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

* **A function the main project does not call does not need a comparison
  entry.**  The 110 rows the engine cannot reach are skipped by default; they
  stay in the table, but they do not get work.
* **`double` is not in scope.**  No `src/` line passes a double argument to a
  Wild Magic template, so every double instantiation is `not executed` however
  many entries it has.
* **"Linked" is not "executed".**  The withdrawal probe is a filter only.
* **Only what has a retail body can be paired.**  34 functions are executed and
  the retail inlines them everywhere (`Quaternion<float>::Rotate` is the clearest
  case); 17 exist only in the rebuild.
* **The harness cannot see the engine's own code.**  Differences inside `src/`
  are outside its reach.

## What is left

**Nothing on the engine-facing surface.**  All 46 rows the engine executes are
bit-identical to the retail, at 1,000,000 draws on all five seeds.

What remains are the **whole-table failures under `--include-dead`** - 71 rows,
every one of them `dead`:

* **47 `double` rows.**  `src/` passes no `double` argument to a Wild Magic
  template anywhere (`double-audit.md`).
* **24 `float` rows.**  The `DistLine3Segment3` and `DistSegment3Segment3`
  families (the retail's association of the long `fSqrDist` sum was never
  pinned down), the swept `DistVector2Box2` rows, the capsule and swept-sphere
  intersectors, `Math<float>::InvSqrt`, `GenerateOrthonormalBasis`, the masked
  2D `ContAlignedBox`, and the four `Query2`/`Query3` rows with the known
  **OR-vs-AND** source divergence - which is real but unreachable.

Two other items are open, and neither is a rebuild defect:

* **The `--skip-extreme` measurement defect.**  The filter tests every word of
  the input block as a float, so nine rows run **zero** inputs under it and
  report a vacuous pass (`CONCLUSIONS.md` §3.5, §7).
* **The `Query2`/`Query3` OR-vs-AND source divergence.**  The retail combines
  the simplex signs with OR where the rebuild uses AND.  `src/` never names
  `Wm3::Query2` or `Wm3::Query3`, so it is recorded rather than scheduled.

## Resuming

* **The reachability queue is empty**, and the audit that emptied it is
  `reachable.md` - one row per table row, with the verdict and the evidence.
  The three `0x004F08xx` members were the ones that proved the
  **register-passing thunk**: the quaternion bodies take their argument in
  `EAX`, so the thunk sets `ECX`/`EAX`, pushes nothing, and calls through `EDX`.
  `Sphere3<float>(const Vector3&, float)` is the one constructor row this library
  can have.
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
* Regenerating `reachable.md`: it is **generated** - the ledger is written from
  the audit and from `--list`, and a **new row must be audited before it is
  added**.  The dead set the harness skips is `src/engine_scope.{h,cpp}`; it
  must be regenerated from the same audit, not edited by hand.
