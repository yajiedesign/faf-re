# The `wmdiff` table, and what the engine reaches of it

**This file is generated data.**  It is the ledger of the **current** harness
table - the 46 rows `./build/clang-x86-release/tools/wmdiff/wmdiff.exe --list`
prints - and it is generated from two things:

* the **reachability audit** of every row (the `executed?` and `evidence`
  columns), and
* the **harness result** for every row (the `wmdiff status` column).

A **new row must be audited before it is added.**  A row that is not in this
ledger has no reachability verdict, so the harness cannot tell whether the
engine runs it; adding it to the table without an audit would silently widen
what a default run claims to cover.

**Names are the C++ names**, with the parameter list the declaration has, so
overloads are told apart by their parameters instead of by a hand-written tag:
`Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&)` and
`Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)`.
The harness rows carry the same names, so the two documents cross-reference
exactly.  **The source is the authority for a parameter list** - where the
decompiled signature and the declaration disagreed, the declaration won.

## The deletion of 2026-10-29

The table used to carry **156 rows**: 46 the engine reaches and **110 the
engine never reaches** (every `double` instantiation, plus the hull / eigen /
query families and a number of `float` members whose only callers live in
classes `src/` never enters).  Those 110 rows were kept in the table and
**skipped by default**, so a default run said nothing about them.

On **2026-10-29** they were **deleted**, along with the code that existed only
for them - the generators, the retail thunks, the "ours" wrappers and the
file-local helpers that no remaining row named.  The harness has no
`--include-dead` option any more: with the dead rows gone there is nothing to
skip, and `--list` simply lists the 46 rows that are left.

**They are recoverable from git history.**  The deletion is commit
`ef0a2856`; its parent holds the full 156-row table and all the code that went
with it (the parent is commit `03b738dfe8a83374d09a8940bb6c68a9d7a95d94`):

```bash
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/wm3_coll_targets.cpp   # the 108 dead coll rows
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/wm3_coll_thunks.cpp    # their retail thunks
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/wm3_coll_harness.cpp   # their "ours" wrappers
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/wm3_vec_targets.cpp    # the dead Vector3 rows
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/wm3_math_targets.cpp   # the dead Math rows
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/src/engine_scope.cpp       # the dead-name list itself
git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/reachable.md               # the full 156-row ledger
```

The evidence column of the rows that remain is unchanged - the audit that
produced it is the same audit; only the rows it was recorded against were
removed.

## The four values

`wmdiff status` uses the four values the harness has always used:

| value | meaning | count |
|---|---|---:|
| **已匹配** | the row is compared against the retail and passes bit for bit | 46 |
| **未匹配** | the target has no row - it is a Wild Magic function the table does not cover | 0 |
| **失败** | the row is compared against the retail and fails | 0 |
| **不适用** | no row is the right answer for the target | 0 |

Every target in this ledger **is** a table row, so only **已匹配** occurs here;
**未匹配** and **不适用** are the values a *function without a row* would carry,
and they are the two ways a newly reachable Wild Magic member can still need
work.

## The counts

| | |
|---|---:|
| rows in the table | **46** |
| **executed** - the engine can reach the row | **46** |
| **dead** - the engine cannot reach the row | **0** |
| **unsettled** - reachability not decided | **0** |
| rows the harness runs by default | **46** |
| rows the harness skips | **0** |
| of the default run: **已匹配** | **46** |
| of the default run: **失败** | **0** |

The 143 functions that have **no row and should not have one** are listed
further down ("Functions with no row, and why"): they are the carried-forward
verdicts from the 2026-10-28 ledger, so the question "is anything reachable
but unmatched?" is answered by reading this file, not by a new census.

**A function the main project does not call does not need a comparison entry.**
That is the rule this ledger applies, and since 2026-10-29 it is enforced by
deletion rather than by a skip flag: the 110 dead rows were removed, so the
pass/fail line is about the code the engine actually runs and nothing else.

## How this was decided

Three rules, in the order they were applied.

1. **A `src/` call site that names the member** decides it - the member is
   `executed`.  The call site is quoted, with the file and the line.
2. **Otherwise, a Wild Magic call from a reached member** decides it - the
   member is `executed` if Wild Magic itself calls it from a member that
   rule 1 (or rule 2) already reached.  The chain is quoted: the `src/` line,
   the Wild Magic file and line, and the member it calls.
3. **Otherwise the row is `dead`.**  The evidence for a `dead` row is the
   **search that found nothing** - the `grep` over `src/`, and, where the
   member is reachable only through Wild Magic, the `grep` over
   `dependencies/WildMagic3p8/Foundation/` that shows the caller is itself
   unreachable.  "Not found" without the search is not evidence.

Two rules of thumb that came out of the audit:

* **"The class is executed" is not "the member is executed."**  The engine
  calls `IntrBox2Circle2f::Test()` and never the swept `Find`; it calls
  `IntrSegment3Sphere3f::Find()` and never `Test`; it calls
  `IntrSegment3Box3f::Find()` and never `Test`.  Those three families of
  members are `dead` even though the class is very much alive.
* **A `double` instantiation is `dead` by default.**  No `src/` line passes a
  `double` argument to a Wild Magic template
  (`grep -rn "Vector3d\|Vector2d" src/` -> 0;
  `grep -rn "\bdouble\b" src/" | grep -i wm3 -> 0), so every `double`
  instantiation is linked but never executed.  See `double-audit.md`.

No row is **unsettled**: every one of the 46 rows was settled either by a
quoted `src/` call site or by a quoted search that found nothing.


## The table


### `Wm3::Vector3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 1 | `Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00452AD0` | yes | `src/moho/render/camera/VTransform.h:284` `Wm3::Vec3f::Dot(rotatedNormal, transform.pos_)`; `src/moho/render/CWldTerrainDecal.cpp:65` | 已匹配 |
| 2 | `Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&) const` | `0x00452AD0` | yes | one Wild Magic call from row 5: `src/moho/collision/CColPrimitiveBase.cpp:430` -> `DistVector3Box3f::GetSquared()` -> `Foundation/Distance/Wm3DistVector3Box3.cpp:56` `kDiff.Dot(m_rkBox.Axis[i])` | 已匹配 |
| 3 | `Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00452BC0` | yes | `src/moho/audio/AudioEngine.cpp:1197,1199`; `src/moho/collision/CColPrimitiveBase.cpp:140,221` | 已匹配 |
| 4 | `Wm3::Vector3<float>::Cross(const Wm3::Vector3<float>&) const` | `0x00452BC0` | yes | one Wild Magic call from row 21: `src/moho/ui/UiRuntimeTypes.cpp:21008` -> `IntrLine3Box3f::Test()` -> `Foundation/Intersection/Wm3IntrLine3Box3.cpp:44` `m_rkLine.Direction.Cross(kDiff)` | 已匹配 |
| 5 | `Wm3::Vector3<float>::LengthSq(const Wm3::Vector3<float>&)` | `0x004757E0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:165,394,587,620`; `src/moho/audio/AudioEngine.cpp:1134`; `src/moho/ai/CAiSteeringImpl.cpp:603,637` | 已匹配 |
| 6 | `Wm3::Vector3<float>::SquaredLength() const` | `0x004757E0` | yes | one Wild Magic call from row 17: `src/moho/projectile/Projectile.cpp:1553` -> `DistVector3Segment3f::GetSquared()` -> `Foundation/Distance/Wm3DistVector3Segment3.cpp:72` `kDiff.SquaredLength()` | 已匹配 |
| 7 | `Wm3::Vector3<float>::Normalize(Wm3::Vector3<float>&, float)` | `0x00452AF0` | yes | `src/moho/entity/Entity.cpp:7664,7862,7867,7979` `Wm3::Vector3f::Normalize(&pushedUp)` -> `Foundation/Math/Wm3Vector3.h:177` -> this `Vector3&` overload | 已匹配 |
| 151 | `Wm3::Vector3<float>::Length() const` | `0x00452A90` | yes | `src/moho/ai/CAiSteeringImpl.cpp:198` `velocity.Length()` and `:1458` `mOwnerUnit->GetVelocity().Length()` (the argument is `const Wm3::Vector3f&`, `CAiSteeringImpl.cpp:191`) | 已匹配 |
| 152 | `Wm3::Vector3<float>::CompareArrays(const Wm3::Vector3<float>&) const` | `0x004F0A50` | yes | `src/moho/render/camera/VTransform.cpp:174` `pos_ != rhs.pos_` and `src/moho/entity/CTextureScroller.cpp:553` `pos_ == ...pos_`; `operator!=` / `operator==` both call `CompareArrays` (`Foundation/Math/Wm3Vector3.inl:109,115`) | 已匹配 |
| 153 | `Wm3::Vector3<float>::operator==(const Wm3::Vector3<float>&) const` | `0x004F08B0` | yes | `src/moho/entity/CTextureScroller.cpp:553` `entityData.mCurTransform.pos_ == entityData.mLastTransform.pos_` (both `Wm3::Vector3f`) | 已匹配 |

### `Wm3`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 9 | `Wm3::SqrtfBinary(float)` | `0x00452FC0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:171,394,442,587,620,645` | 已匹配 |
| 47 | `Wm3::InBox<float>(const Wm3::Vector3<float>&, const Wm3::Box3<float>&)` | `0x00A3BC10` | yes | `src/moho/collision/CColPrimitiveBase.cpp:457` | 已匹配 |
| 50 | `Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*)` | `0x00A3AE40` | yes | `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26` | 已匹配 |
| 57 | `Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*, const bool*)` | `0x00A3AE40` | yes | the same C++ function as row 50 (`Foundation/Containment/Wm3ContBox3.h:27`; the mask argument defaults to null) - reached through `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`. | 已匹配 |

### `Wm3::Math<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 10 | `Wm3::Math<float>::FAbs(float)` | `0x0040DAB0` | yes | one Wild Magic call from row 33: `src/moho/ai/CAiSteeringImpl.cpp:273` -> `IntrBox2Box2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Box2.cpp:53` `Math<Real>::FAbs(...)` | 已匹配 |
| 12 | `Wm3::Math<float>::Sqrt(float)` | `0x00452FC0` | yes | one Wild Magic call from row 20: `src/moho/vision/VisionDB.cpp:89` -> `IntrBox2Circle2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` -> `Foundation/Distance/Wm3DistVector2Box2.cpp:40` `Math<Real>::Sqrt(GetSquared())` | 已匹配 |

### `Wm3::AxisAlignedBox3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 13 | `Wm3::AxisAlignedBox3<float>::TestIntersection(const Wm3::AxisAlignedBox3<float>&) const` | `0x00506010` | yes | `src/moho/mesh/Mesh.cpp:508,519,1591` | 已匹配 |

### `Wm3::Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 14 | `Wm3::Box3<float>::ComputeVertices(Wm3::Vector3<float>*) const` | `0x004FFF00` | yes | `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:25`; `src/moho/render/SelectionBracketRenderer.cpp:364` | 已匹配 |

### `Wm3::DistVector3Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 15 | `Wm3::DistVector3Box3<float>::GetSquared()` | `0x00A45C00` | yes | `src/moho/collision/CColPrimitiveBase.cpp:430` | 已匹配 |

### `Wm3::DistVector3Segment3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 17 | `Wm3::DistVector3Segment3<float>::GetSquared()` | `0x00A484F0` | yes | `src/moho/projectile/Projectile.cpp:1553` | 已匹配 |
| 18 | `Wm3::DistVector3Segment3<float>::Get()` | `0x00A480E0` | yes | `src/moho/ui/CUIWorldView.cpp:277` | 已匹配 |

### `Wm3::IntrBox2Circle2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 20 | `Wm3::IntrBox2Circle2<float>::Test()` | `0x00A4E620` | yes | `src/moho/vision/VisionDB.cpp:89`; `src/moho/render/VisionRenderer.cpp:313` | 已匹配 |

### `Wm3::IntrLine3Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 21 | `Wm3::IntrLine3Box3<float>::Test()` | `0x00A4FE60` | yes | `src/moho/ui/UiRuntimeTypes.cpp:21008` | 已匹配 |
| 68 | `Wm3::IntrLine3Box3<float>::DoClipping(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Box3<float>&, bool, int&, Wm3::Vector3<float>*, int&)` | `0x00A50220` | yes | one Wild Magic call from row 22: `src/moho/collision/CColPrimitiveBase.cpp:385` -> `IntrSegment3Box3f::Find()` -> `Foundation/Intersection/Wm3IntrSegment3Box3.cpp:100` `IntrLine3Box3<Real>::DoClipping(...)` | 已匹配 |
| 69 | `Wm3::IntrLine3Box3<float>::Clip(float, float, float&, float&)` | `0x00A4FCC0` | yes | two Wild Magic calls from row 22: -> `Foundation/Intersection/Wm3IntrLine3Box3.cpp:150` (called by `DoClipping`) | 已匹配 |

### `Wm3::IntrSegment3Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 22 | `Wm3::IntrSegment3Box3<float>::Find()` | `0x00A462A0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:385` | 已匹配 |
| 27 | `Wm3::IntrSegment3Box3<float>::GetPoint(int) const` | `0x00A46300` | yes | `src/moho/collision/CColPrimitiveBase.cpp:391,394,395` | 已匹配 |

### `Wm3::Distance<float, Wm3::Vector3<float>>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 72 | `Wm3::Distance<float, Wm3::Vector3<float>>::SetDifferenceStep(float)` | `0x00A391D0` | yes | `Foundation/Distance/Wm3Distance.cpp:22` - called by the `Distance3f` constructor, which runs for the `DistVector3Box3f` object built at `src/moho/collision/CColPrimitiveBase.cpp:429`. | 已匹配 |
| 73 | `Wm3::Distance<float, Wm3::Vector3<float>>::Distance()` | `0x00A39F20` | yes | the base constructor of every `Distance3f`; reached through `src/moho/collision/CColPrimitiveBase.cpp:429` (`Wm3::DistVector3Box3f distance(sphere->Center, mShape)`) and `src/moho/projectile/Projectile.cpp:1552`. | 已匹配 |

### `Wm3::IntrBox3Sphere3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 24 | `Wm3::IntrBox3Sphere3<float>::Test()` | `0x00A41560` | yes | `src/moho/collision/CColPrimitiveBase.cpp:608` | 已匹配 |
| 25 | `Wm3::IntrBox3Sphere3<float>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A43420` | yes | `src/moho/collision/CColPrimitiveBase.cpp:618` | 已匹配 |
| 26 | `Wm3::IntrBox3Sphere3<float>::GetContactPoint() const` | `0x00A414C0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:619` | 已匹配 |
| 51 | `Wm3::IntrBox3Sphere3<float>::GetVertexIntersection(float, float, float, float, float, float, float)` | `0x00A41740` | yes | two Wild Magic calls from row 25: `src/moho/collision/CColPrimitiveBase.cpp:618` -> `IntrBox3Sphere3f::Find(fTMax,v0,v1)` -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:384` (and `:480,:657`) | 已匹配 |
| 52 | `Wm3::IntrBox3Sphere3<float>::GetEdgeIntersection(float, float, float, float, float, float)` | `0x00A41800` | yes | two Wild Magic calls from row 25: -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:392` (and `:411,:489`) | 已匹配 |
| 53 | `Wm3::IntrBox3Sphere3<float>::FindFaceRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)` | `0x00A41880` | yes | two Wild Magic calls from row 25: -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:181,191,211` | 已匹配 |
| 54 | `Wm3::IntrBox3Sphere3<float>::FindJustEdgeIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)` | `0x00A41CC0` | yes | two Wild Magic calls from row 25: -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:553,575` | 已匹配 |
| 55 | `Wm3::IntrBox3Sphere3<float>::FindEdgeRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&, bool)` | `0x00A41F50` | yes | two Wild Magic calls from row 25: -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:198,218,228` | 已匹配 |
| 56 | `Wm3::IntrBox3Sphere3<float>::FindVertexRegionIntersection(float, float, float, float, float, float, float, float, float, float&, float&, float&)` | `0x00A421E0` | yes | two Wild Magic calls from row 25: -> `Foundation/Intersection/Wm3IntrBox3Sphere3.cpp:235` | 已匹配 |

### `Wm3::IntrSegment3Sphere3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 28 | `Wm3::IntrSegment3Sphere3<float>::Find()` | `0x00A471C0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:578` | 已匹配 |
| 29 | `Wm3::IntrSegment3Sphere3<float>::GetPoint(int) const` | `0x00A46940` | yes | `src/moho/collision/CColPrimitiveBase.cpp:584,587,588` | 已匹配 |

### `Wm3::IntrBox2Box2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 33 | `Wm3::IntrBox2Box2<float>::Test()` | `0x00A47D40` | yes | `src/moho/ai/CAiSteeringImpl.cpp:273` | 已匹配 |

### `Wm3::DistVector2Box2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 36 | `Wm3::DistVector2Box2<float>::GetSquared()` | `0x00A6C9D0` | yes | two Wild Magic calls from row 20: `src/moho/vision/VisionDB.cpp:89` -> `IntrBox2Circle2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` -> `Foundation/Distance/Wm3DistVector2Box2.cpp:40` `Get()` -> `GetSquared()` | 已匹配 |
| 41 | `Wm3::DistVector2Box2<float>::Get()` | `0x00A6C680` | yes | one Wild Magic call from row 20: `src/moho/vision/VisionDB.cpp:89` -> `IntrBox2Circle2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` `DistVector2Box2<Real>(...).Get()` | 已匹配 |

### `Wm3::Quaternion<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 154 | `Wm3::Quaternion<float>::CompareArrays(const Wm3::Quaternion<float>&) const` | `0x004F0B40` | yes | one Wild Magic call from row 155: `src/moho/ai/CAiFormationInstance.cpp:3590` `mOrientation == kZeroQuaternion` -> `Foundation/Math/Wm3Quaternion.inl:109` `return CompareArrays(rkV) == 0;` | 已匹配 |
| 155 | `Wm3::Quaternion<float>::operator==(const Wm3::Quaternion<float>&) const` | `0x004F08E0` | yes | `src/moho/ai/CAiFormationInstance.cpp:3590` `mOrientation == kZeroQuaternion` | 已匹配 |

### `Wm3::Sphere3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 156 | `Wm3::Sphere3<float>::Sphere3(const Wm3::Vector3<float>&, float)` | `0x004740C0` | yes | `src/moho/ai/CAiSteeringImpl.cpp:537` `const Wm3::Sphere3f querySphere{owner->GetPosition(), queryRadius}`; `src/moho/collision/CColPrimitiveBase.cpp:468` `mShape(localCenter, radius)` | 已匹配 |

## Notes on the audit

### The 110 deleted rows came in two islands

Before the 2026-10-29 deletion the table carried 156 rows.  The 110 rows that
were deleted - the ones no `src/` call site reaches - came in two islands:

* **the `double` island** (51 rows): every `double` instantiation in the table.
  They are linked into the binary because the library instantiates them
  explicitly, and never executed.  `double-audit.md` has the per-target
  evidence; the audit above re-verified the rule against the current `src/`.
* **the `float` island** (59 rows): the float rows whose classes and members
  the engine never calls - the convex hulls and `Eigen`, `Query2`/`Query3`,
  the capsule intersectors, the swept distance queries, the `Distance` base
  class's Newton driver, and the private helpers of intersectors the engine
  only enters through their unswept entry points.

Both islands are gone from the table now.  The per-row evidence for the
deleted rows is in the deletion commit's parent (see
`git show 03b738dfe8a83374d09a8940bb6c68a9d7a95d94:tools/wmdiff/reachable.md`); `double-audit.md` and
`double-audit` in the harness's own notes still carry the `double` island's
reasoning.

### What the audit changed

Before this ledger the harness skipped only the 47 `double` rows, so a default
run exercised 109 rows.  It then exercised **46** (the 63 rows the engine
cannot reach were being compared on every run - the 59 `float` rows of the
float island, plus the 4 `double` rows the old skip list had missed - and
their results said nothing about the engine).  On 2026-10-29 those 63 plus the
47 were deleted outright, so the table is now **46 rows, all of them
executed, all of them run by default**, and there is no skip list.

The 46 rows that run by default all pass.  They used to be 34 passing
and 12 failing; the 12 failures were exactly the ones the float analysis
documents (`float-analysis-ordinary.md`, `float-analysis-extreme.md`)
attributed: the two `Dot` rows, the two `LengthSq`/`SquaredLength` rows,
`DistVector3Box3f::GetSquared`, `DistVector3Segment3f::Get`/`GetSquared`,
`IntrLine3Box3f::Test`, `IntrSegment3Box3f::Find`/`GetPoint`,
`IntrBox3Sphere3f::Test` and the masked `ContAlignedBox`.  All of them were
**codegen** differences (the retail's x87 register file against the rebuild's
binary32 SSE), not source differences - and the three that reach the game
(`Dot`, `LengthSq`/`SquaredLength`, `DistVector3Segment3`) are the ones
`x87math-usage.md` names.  The two submodule commits `59f1614` and `9af34dc`
reproduce the retail's x87 chains for those rows; the mechanism per row is in
`float-analysis-ordinary.md` under `## Resolution`.

The 110 deleted rows were 85 passing and 71 failing in total across the two
islands; every one of the 71 failures was a **dead** row - the engine never
executes it - so those failures said nothing about the engine either.

### The rows that were hardest to classify

* **`Wm3::Vector3<float>::Normalize()`** - the row's retail address
  (`0x00452AF0`) is the same one the *static* `Normalize(Vector3&, float)`
  uses, so the two rows look like one function.  They are two C++ functions:
  the engine calls the static one (`Wm3::Vector3f::Normalize(&v)`), and the
  member has no caller at all - its own callers are `UnitCross` and the
  swept sphere queries.  `dead`, with no loss of coverage: the shared retail
  body is still compared by row 7.
* **`Wm3::DistVector2Box2<float>::Get()`** - `src/` never names it
  (`VisionShader.h:46` is a comment about a hand-written HLSL copy), but
  `IntrBox2Circle2f::Test()` calls it, and the engine calls that `Test`.  So
  it is `executed`, and with it `GetSquared()` and `Math<float>::Sqrt`.  The
  old census said "not executed" because it only looked at `src/`.
* **`Wm3::IntrLine3Capsule3<float>::Find(...)` and the whole
  `IntrSegment3Capsule3<float>` family** - these look engine-facing because
  the *swept* `IntrSegment3Sphere3f` queries use them.  But the engine calls
  the **unswept** `IntrSegment3Sphere3f::Find()`, so the swept path is never
  entered and the capsule intersector is never built.  `dead`.
* **`Wm3::Distance<float, Wm3::Vector3<float>>::SetDifferenceStep(float)`** -
  no `src/` line names it, and it is not virtual.  It is `executed` because
  the `Distance` base constructor calls it (`Wm3Distance.cpp:22`), and the
  base constructor runs for every `DistVector3Box3f` / `DistVector3Segment3f`
  the engine builds.  The same chain makes the base constructor `executed`.
* **`Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*)` and
  `Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*, const bool*)`** -
  in WM3.8 there is only **one** function here; the mask argument has a
  default.  The two rows are the harness's two call shapes, so both are
  `executed`.  The 2D overloads are a different function and are `dead`:
  `src/`'s only `ContAlignedBox` call passes `Wm3::Vector3f vertices[8]`.

## Functions with no row, and why (carried forward)

These are the entries of the 2026-10-28 ledger whose verdict was `不适用` -
no row is the right answer.  They are carried forward **verbatim** from
`bd18d01^:wmdiff/reachable.md` (the ledger as it stood before wmdiff moved
out of the dependency tree), because the deletion of 2026-10-29 removed them
from view: the file then listed only the rows that still exist.

They were not re-derived.  The 46 rows above were re-audited on 2026-10-29;
these were not touched by that round.  If a question turns on one of them,
check that one entry and record the result here - do not start a new census.

| function | source | verdict |
|---|---|---|
| `Wm3::DistSegment3Segment3<float>::DistSegment3Segment3(const Wm3::Segment3<float>&, const Wm3::Segment3<float>&)` | Wm3DistSegment3Segment3.cpp | `不适用` |
| `Wm3::DistVector2Box2<float>::DistVector2Box2(const Wm3::Vector2<float>&, const Wm3::Box2<float>&)` | Wm3DistVector2Box2.cpp | `不适用` |
| `Wm3::DistVector3Box3<float>::DistVector3Box3(const Wm3::Vector3<float>&, const Wm3::Box3<float>&)` | Wm3DistVector3Box3.cpp | `不适用` |
| `Wm3::DistVector3Segment3<float>::DistVector3Segment3(const Wm3::Vector3<float>&, const Wm3::Segment3<float>&)` | Wm3DistVector3Segment3.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::Distance()` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::~Distance()` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector3<float>>::~Distance()` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::SetDifferenceStep(float)` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::GetDerivative(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::GetDerivativeSquared(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::Get(float, float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Distance.cpp | `不适用` |
| `Wm3::Distance<float, Wm3::Vector2<float>>::GetSquared(float, float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Distance.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::Intersector()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::Intersector()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::~Intersector()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::~Intersector()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::Test()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::Test()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::Find()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::Find()` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::Test(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::Test(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector2<float>>::Find(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | Wm3Intersector.cpp | `不适用` |
| `Wm3::Intersector<float, Wm3::Vector3<float>>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Intersector.cpp | `不适用` |
| `Wm3::IntrBox2Box2<float>::IntrBox2Box2(const Wm3::Box2<float>&, const Wm3::Box2<float>&)` | Wm3IntrBox2Box2.cpp | `不适用` |
| `Wm3::IntrBox2Circle2<float>::IntrBox2Circle2(const Wm3::Box2<float>&, const Wm3::Circle2<float>&)` | Wm3IntrBox2Circle2.cpp | `不适用` |
| `Wm3::IntrBox3Sphere3<float>::IntrBox3Sphere3(const Wm3::Box3<float>&, const Wm3::Sphere3<float>&)` | Wm3IntrBox3Sphere3.cpp | `不适用` |
| `Wm3::Dot24x(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3IntrLine3Box3.cpp | `不适用` |
| `Wm3::Cross24(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3IntrLine3Box3.cpp | `不适用` |
| `Wm3::Sub24Vector(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3IntrLine3Box3.cpp | `不适用` |
| `Wm3::IntrLine3Box3<float>::IntrLine3Box3(const Wm3::Line3<float>&, const Wm3::Box3<float>&)` | Wm3IntrLine3Box3.cpp | `不适用` |
| `Wm3::IntrSegment3Box3<float>::IntrSegment3Box3(const Wm3::Segment3<float>&, const Wm3::Box3<float>&, bool)` | Wm3IntrSegment3Box3.cpp | `不适用` |
| `Wm3::IntrSegment3Capsule3<float>::IntrSegment3Capsule3(const Wm3::Segment3<float>&, const Wm3::Capsule3<float>&)` | Wm3IntrSegment3Capsule3.cpp | `不适用` |
| `Wm3::IntrSegment3Sphere3<float>::IntrSegment3Sphere3(const Wm3::Segment3<float>&, const Wm3::Sphere3<float>&)` | Wm3IntrSegment3Sphere3.cpp | `不适用` |
| `Wm3::AxisAlignedBox3<float>::AxisAlignedBox3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3AxisAlignedBox3.h | `不适用` |
| `Wm3::AxisAlignedBox3<float>::AxisAlignedBox3()` | Wm3AxisAlignedBox3.inl | `不适用` |
| `Wm3::Box2<float>::Box2()` | Wm3Box2.inl | `不适用` |
| `Wm3::Box2<float>::Box2(const Wm3::Vector2<float>&, const Wm3::Vector2<float>*, const float*)` | Wm3Box2.inl | `不适用` |
| `Wm3::Box2<float>::Box2(const Wm3::Vector2<float>&, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&, float, float)` | Wm3Box2.inl | `不适用` |
| `Wm3::Box3<float>::Box3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Box3.h | `不适用` |
| `Wm3::Box3<float>::Box3()` | Wm3Box3.inl | `不适用` |
| `Wm3::Box3<float>::Box3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>*, const float*)` | Wm3Box3.inl | `不适用` |
| `Wm3::Box3<float>::Box3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, float, float, float)` | Wm3Box3.inl | `不适用` |
| `Wm3::Capsule3<float>::Capsule3()` | Wm3Capsule3.inl | `不适用` |
| `Wm3::Circle2<float>::Circle2(const Wm3::Vector2<float>&, float)` | Wm3Circle2.inl | `不适用` |
| `Wm3::IVector2<int>::IVector2()` | Wm3IVector2.h | `不适用` |
| `Wm3::IVector2<int>::IVector2(int, int)` | Wm3IVector2.h | `不适用` |
| `Wm3::IVector2<int>::X() const` | Wm3IVector2.h | `不适用` |
| `Wm3::IVector2<int>::Y() const` | Wm3IVector2.h | `不适用` |
| `Wm3::IVector2<int>::operator<(const Wm3::IVector2<int>&) const` | Wm3IVector2.h | `不适用` |
| `Wm3::Line3<float>::Line3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Line3.inl | `不适用` |
| `Wm3::Math<float>::Cos(float)` | Wm3Math.inl | `不适用` |
| `Wm3::Math<float>::Sin(float)` | Wm3Math.inl | `不适用` |
| `Wm3::Matrix3<float>::Matrix3(bool)` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Matrix3<float>::operator()(int, int)` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Matrix3<float>::I(int, int)` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Matrix3<float>::MakeZero()` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Matrix3<float>::MakeIdentity()` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Matrix3<float>::operator*(const Wm3::Vector3<float>&) const` | Wm3Matrix3.inl | `不适用` |
| `Wm3::Plane3<float>::Plane3()` | Wm3Plane3.inl | `不适用` |
| `Wm3::Plane3<float>::Plane3(const Wm3::Plane3<float>&)` | Wm3Plane3.inl | `不适用` |
| `Wm3::Plane3<float>::Plane3(const Wm3::Vector3<float>&, float)` | Wm3Plane3.inl | `不适用` |
| `Wm3::Plane3<float>::operator=(const Wm3::Plane3<float>&)` | Wm3Plane3.inl | `不适用` |
| `Wm3::Quaternion<float>::Quaternion(const Wm3::Vector3<float>&, float)` | Wm3Quaternion.h | `不适用` |
| `Wm3::Quaternion<float>::Quaternion()` | Wm3Quaternion.h | `不适用` |
| `Wm3::Quaternion<float>::Quaternion(float, float, float, float)` | Wm3Quaternion.h | `不适用` |
| `Wm3::Quaternion<float>::Identity()` | Wm3Quaternion.h | `不适用` |
| `Wm3::Quaternion<float>::LengthSq(const Wm3::Quaternion<float>&)` | Wm3Quaternion.h | `不适用` |
| `Wm3::Quaternion<float>::Quaternion(const Wm3::Quaternion<float>&)` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::operator[](int)` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::W() const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::X() const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::Y() const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::Z() const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::operator=(const Wm3::Quaternion<float>&)` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::operator!=(const Wm3::Quaternion<float>&) const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::operator*(float) const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::ToRotationMatrix(Wm3::Matrix3<float>&) const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::FromAxisAngle(const Wm3::Vector3<float>&, float)` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::Length() const` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::Normalize()` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Quaternion<float>::Rotate(const Wm3::Vector3<float>&)` | Wm3Quaternion.inl | `不适用` |
| `Wm3::Segment3<float>::Segment3()` | Wm3Segment3.inl | `不适用` |
| `Wm3::Segment3<float>::Segment3(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, float)` | Wm3Segment3.inl | `不适用` |
| `Wm3::Sphere3<float>::Sphere3()` | Wm3Sphere3.inl | `不适用` |
| `Wm3::Sphere3<float>::operator=(const Wm3::Sphere3<float>&)` | Wm3Sphere3.inl | `不适用` |
| `Wm3::Vector2<float>::Vector2()` | Wm3Vector2.h | `不适用` |
| `Wm3::Vector2<float>::Vector2(float, float)` | Wm3Vector2.h | `不适用` |
| `Wm3::Vector2<float>::Vector2(const Wm3::Vector2<float>&)` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator[](int) const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator[](int)` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::X() const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::X()` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::Y() const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::Y()` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator=(const Wm3::Vector2<float>&)` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator+(const Wm3::Vector2<float>&) const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator-(const Wm3::Vector2<float>&) const` | Wm3Vector2.inl | `不适用` |
| `Wm3::operator*(float, const Wm3::Vector2<float>&)` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::operator+=(const Wm3::Vector2<float>&)` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::Length() const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::Dot(const Wm3::Vector2<float>&) const` | Wm3Vector2.inl | `不适用` |
| `Wm3::Vector2<float>::Normalize()` | Wm3Vector2.inl | `不适用` |
| `Wm3::Wm3Detail::RoundTo24(double)` | Wm3Vector3.h | `不适用` |
| `Wm3::Wm3Detail::Mul24(double, double)` | Wm3Vector3.h | `不适用` |
| `Wm3::Wm3Detail::Add24(double, double)` | Wm3Vector3.h | `不适用` |
| `Wm3::Wm3Detail::Sub24(double, double)` | Wm3Vector3.h | `不适用` |
| `Wm3::Wm3Detail::Div24(double, double)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Vector3()` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Vector3(float, float, float)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Zero()` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::NaN()` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Add(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Sub(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Scale(const Wm3::Vector3<float>&, float)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Length(const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Normalize(Wm3::Vector3<float>*, float)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::DistanceSq3D(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::DistanceSqXZ(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::IsntNaN(const Wm3::Vector3<float>*)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::IsntNaN(const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::IsInvalid(const Wm3::Vector3<float>&)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3fIsntNaN(const Wm3::Vector3<float>*)` | Wm3Vector3.h | `不适用` |
| `Wm3::Vector3<float>::Vector3(const Wm3::Vector3<float>&)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator const float*() const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator[](int) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator[](int)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::X() const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::X()` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::Y() const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::Y()` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::Z() const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::Z()` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator=(const Wm3::Vector3<float>&)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator!=(const Wm3::Vector3<float>&) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator+(const Wm3::Vector3<float>&) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator-(const Wm3::Vector3<float>&) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator*(float) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator/(float) const` | Wm3Vector3.inl | `不适用` |
| `Wm3::operator*(float, const Wm3::Vector3<float>&)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator+=(const Wm3::Vector3<float>&)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::operator/=(float)` | Wm3Vector3.inl | `不适用` |
| `Wm3::Vector3<float>::ComputeExtremes(int, const Wm3::Vector3<float>*, Wm3::Vector3<float>&, Wm3::Vector3<float>&)` | Wm3Vector3.inl | `不适用` |

### The three reasons behind `不适用`

* **not executed** (57): the engine has no call site for the member.  Every
  `double` instantiation is here - no `src/` line anywhere passes a double
  argument to a Wild Magic template - and so are the three hull classes in both
  precisions and `Eigen` in both.  Rows already in the table for those classes
  are kept: verified, bit-exact, and they cost nothing, but they are not
  engine-facing and no further work goes into them.
* **executed, but no retail body** (52): the retail inlines the member at every
  call site, so there is no address to pair.  This is a finding, not a gap - the
  evidence is the retail block that was read, and it is recorded in the note.
  `Quaternion<float>::Rotate` is the clearest case: the engine calls it, and the
  WM3.8 `Quaternion.cpp` block holds only `CompareArrays` and the two
  `operator==` wrappers.
* **executed, but a row could not discriminate** (13): eight constructors
  whose objects hold **references** - the compared bytes are the caller's
  addresses, and their bases leave fields uninitialised - and the two
  `Intersector` constructors, where skipping the vtable pointer leaves only
  `m_fContactTime = 0.0` and `IT_EMPTY`, constants that could never disagree.
  The pilot and the reasoning are in the README (`Constructor rows`).
* **only in the rebuild** (21): the helpers the rebuild adds - `Zero`, `NaN`,
  `Add`, `Sub`, `Scale`, `DistanceSq3D`, `DistanceSqXZ`, `IsInvalid`, the static
  `Length`, and the `Wm3Detail::*24` x87 model.  There is nothing on the retail
  side to compare them with; their correctness is judged by the members that
  use them.
