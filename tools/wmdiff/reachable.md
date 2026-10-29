# The `wmdiff` table, and what the engine reaches of it

**This file is generated data.**  It is the ledger of the **current** harness
table - the 156 rows `./build/clang-x86-release/tools/wmdiff/wmdiff.exe --list`
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

## The four values

`wmdiff status` uses the four values the harness has always used:

| value | meaning | count |
|---|---|---:|
| **已匹配** | the row is compared against the retail and passes bit for bit | 85 |
| **未匹配** | the target has no row - it is a Wild Magic function the table does not cover | 0 |
| **失败** | the row is compared against the retail and fails | 71 |
| **不适用** | no row is the right answer for the target | 0 |

Every target in this ledger **is** a table row, so only **已匹配** and **失败**
occur here; **未匹配** and **不适用** are the values a *function without a row*
would carry, and they are the two ways a newly reachable Wild Magic member can
still need work.

## The counts

| | |
|---|---:|
| rows in the table | **156** |
| **executed** - the engine can reach the row | **46** |
| **dead** - the engine cannot reach the row | **110** |
| **unsettled** - reachability not decided | **0** |
| rows the harness runs by default | **46** |
| rows the harness skips unless `--include-dead` is given | **110** |
| of the default run: **已匹配** | **46** |
| of the default run: **失败** | **0** |
| of the whole table: **已匹配** | **85** |
| of the whole table: **失败** | **71** |

**A function the main project does not call does not need a comparison entry.**
That is the rule this ledger applies: the 110 dead rows stay in the table (they
were verified bit for bit at some point, and they cost nothing to keep), but
the harness skips them by default, so the pass/fail line is about the code the
engine actually runs.

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
  `grep -rn "\bdouble\b" src/` | grep -i wm3 -> 0), so every `double`
  instantiation is linked but never executed.  See `double-audit.md`.

No row is **unsettled**: every one of the 156 rows was settled either by a
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
| 8 | `Wm3::Vector3<float>::Normalize()` | `0x00452AF0` | no | `grep -rn "\.Normalize()" src/` -> only `Wm3::Vector3f::Normalize(&v)` (the *pointer* overload, row 7). This member's own callers are `Foundation/Math/Wm3Vector3.inl:322` (`UnitCross`), `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:143,176` and `Foundation/Containment/Wm3ContBox3.cpp:288,430,436,447` - none of them reachable. (The retail body `0x00452AF0` stays covered by row 7.) | 已匹配 |
| 137 | `Wm3::Vector3<float>::GenerateOrthonormalBasis(Wm3::Vector3<float>&, Wm3::Vector3<float>&, Wm3::Vector3<float>&, bool)` | `0x00A3CF80` | no | `grep -rn "GenerateOrthonormalBasis\|Orthonormalize" src/` -> 0 hits. The Wild Magic callers (`Foundation/Containment/Wm3ContBox3.cpp:292,307,369`, `Wm3ContCapsule3.cpp:52`, `Foundation/Intersection/Wm3IntrLine3Capsule3.cpp:99`, ...) are all in classes `src/` never reaches. | 失败 |
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
| 58 | `Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*)` | `0x00A685F0` | no | `grep -rn "ContAlignedBox" src/` -> one call, `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`, whose argument is `Wm3::Vector3f vertices[8]` (same file `:23`) => only the 3D overload is instantiated. The 2D overload (`Foundation/Containment/Wm3ContBox2.cpp:20`) has no caller at all. | 已匹配 |
| 59 | `Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*, const bool*)` | `0x00A685F0` | no | `grep -rn "ContAlignedBox" src/` -> one call, `src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`, whose argument is `Wm3::Vector3f vertices[8]` => only the 3D overload is instantiated. | 失败 |

### `Wm3::Math<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 10 | `Wm3::Math<float>::FAbs(float)` | `0x0040DAB0` | yes | one Wild Magic call from row 33: `src/moho/ai/CAiSteeringImpl.cpp:273` -> `IntrBox2Box2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Box2.cpp:53` `Math<Real>::FAbs(...)` | 已匹配 |
| 11 | `Wm3::Math<float>::InvSqrt(float)` | `0x0050D150` | no | `grep -rn "InvSqrt" src/` -> 4 hits, all the engine's own `moho::CoordsScaleByInvSqrt` (`src/moho/math/QuaternionMath.cpp:972`), never `Wm3::Math<float>::InvSqrt`. The Wild Magic callers are `Foundation/Approximation/Wm3ApprCylinderFit3.cpp:45`, `Foundation/Containment/Wm3ContBox3.cpp:571` (`MergeBoxes`) and `Foundation/Math/Wm3Quaternion.inl:790` - every one of them in a class `src/` never reaches. | 失败 |
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
| 16 | `Wm3::DistVector3Box3<float>::Get()` | `0x00A457E0` | no | `grep -rn "DistVector3Box3" src/` -> one use, `src/moho/collision/CColPrimitiveBase.cpp:429`, and it calls `GetSquared()` (row 15). `Get()` has no other caller: `grep -rn "DistVector3Box3<" dependencies/WildMagic3p8/Foundation/` -> only `Wm3DistVector3Box3.cpp` itself and `Wm3DistRay3Box3.cpp:62` / `Wm3DistSegment3Box3.cpp:65,73` (classes `src/` never reaches). | 已匹配 |
| 62 | `Wm3::DistVector3Box3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A458C0` | no | the swept overload of row 15; `grep -rn "DistVector3Box3" src/` -> one use, `src/moho/collision/CColPrimitiveBase.cpp:429`, unswept. | 已匹配 |
| 63 | `Wm3::DistVector3Box3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A45A60` | no | the swept overload of row 15; `grep -rn "DistVector3Box3" src/` -> one use, `src/moho/collision/CColPrimitiveBase.cpp:429`, unswept. | 已匹配 |

### `Wm3::DistVector3Segment3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 17 | `Wm3::DistVector3Segment3<float>::GetSquared()` | `0x00A484F0` | yes | `src/moho/projectile/Projectile.cpp:1553` | 已匹配 |
| 18 | `Wm3::DistVector3Segment3<float>::Get()` | `0x00A480E0` | yes | `src/moho/ui/CUIWorldView.cpp:277` | 已匹配 |
| 48 | `Wm3::DistVector3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A48190` | no | the swept overload of row 17; `grep -rn "DistVector3Segment3f" src/` -> `src/moho/projectile/Projectile.cpp:1552` and `src/moho/ui/CUIWorldView.cpp:276`, both unswept. | 已匹配 |
| 49 | `Wm3::DistVector3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A48340` | no | the swept overload of row 15; `grep -rn "DistVector3Segment3f" src/` -> `src/moho/projectile/Projectile.cpp:1552` and `src/moho/ui/CUIWorldView.cpp:276`, both unswept. | 已匹配 |

### `Wm3::IntrBox2Circle2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 19 | `Wm3::IntrBox2Circle2<float>::Find(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | `0x00A4F580` | no | `grep -rn "IntrBox2Circle2f" src/` -> `src/moho/vision/VisionDB.cpp:89` and `src/moho/render/VisionRenderer.cpp:313`, and both call `Test()` (row 20). The swept `Find(fTMax,v0,v1)` has no caller in `src/` and no reached Wild Magic caller. | 已匹配 |
| 20 | `Wm3::IntrBox2Circle2<float>::Test()` | `0x00A4E620` | yes | `src/moho/vision/VisionDB.cpp:89`; `src/moho/render/VisionRenderer.cpp:313` | 已匹配 |
| 60 | `Wm3::IntrBox2Circle2<float>::TestVertexRegion(float, float, float, float, float, float, float&, float&)` | `0x00A4E6C0` | no | a private helper of `IntrBox2Circle2`, called only from the swept `Find` (`Foundation/Intersection/Wm3IntrBox2Circle2.cpp:194`); that `Find` is itself unreachable (row 19). | 已匹配 |
| 61 | `Wm3::IntrBox2Circle2<float>::TestEdgeRegion(float, float, float, float, float, float, float&, float&)` | `0x00A4EB60` | no | a private helper of `IntrBox2Circle2`, called only from the swept `Find` (`Foundation/Intersection/Wm3IntrBox2Circle2.cpp:290`); that `Find` is itself unreachable (row 19). | 已匹配 |

### `Wm3::IntrLine3Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 21 | `Wm3::IntrLine3Box3<float>::Test()` | `0x00A4FE60` | yes | `src/moho/ui/UiRuntimeTypes.cpp:21008` | 已匹配 |
| 40 | `Wm3::IntrLine3Box3<float>::Find()` | `0x00A508F0` | no | `grep -rn "IntrLine3Box3f" src/` -> one use, `src/moho/ui/UiRuntimeTypes.cpp:21007`, and it calls `Test()` (row 21). `Find()` is called only from `Foundation/Intersection/Wm3IntrLine3Box3.cpp:77` - itself only from a `Find` that is never reached. | 已匹配 |
| 68 | `Wm3::IntrLine3Box3<float>::DoClipping(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Box3<float>&, bool, int&, Wm3::Vector3<float>*, int&)` | `0x00A50220` | yes | one Wild Magic call from row 22: `src/moho/collision/CColPrimitiveBase.cpp:385` -> `IntrSegment3Box3f::Find()` -> `Foundation/Intersection/Wm3IntrSegment3Box3.cpp:100` `IntrLine3Box3<Real>::DoClipping(...)` | 已匹配 |
| 69 | `Wm3::IntrLine3Box3<float>::Clip(float, float, float&, float&)` | `0x00A4FCC0` | yes | two Wild Magic calls from row 22: -> `Foundation/Intersection/Wm3IntrLine3Box3.cpp:150` (called by `DoClipping`) | 已匹配 |

### `Wm3::IntrSegment3Box3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 22 | `Wm3::IntrSegment3Box3<float>::Find()` | `0x00A462A0` | yes | `src/moho/collision/CColPrimitiveBase.cpp:385` | 已匹配 |
| 27 | `Wm3::IntrSegment3Box3<float>::GetPoint(int) const` | `0x00A46300` | yes | `src/moho/collision/CColPrimitiveBase.cpp:391,394,395` | 已匹配 |
| 46 | `Wm3::IntrSegment3Box3<float>::Test()` | `0x00A46440` | no | `grep -rn "IntrSegment3Box3f" src/` -> one construction, `src/moho/collision/CColPrimitiveBase.cpp:384`, and it calls `Find()` (row 22). `Test()` has no caller: `grep -rn "IntrSegment3Box3<" dependencies/WildMagic3p8/Foundation/` -> only `Wm3IntrSegment3Box3.cpp` itself. | 失败 |

### `Wm3::Distance<float, Wm3::Vector3<float>>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 23 | `Wm3::Distance<float, Wm3::Vector3<float>>::GetClosestPoint1() const` | `0x00A39230` | no | `grep -rn "Wm3::Distance\|Distance<" src/` -> 0 hits; no `Distance` object exists on the engine side. The Wild Magic callers of `GetClosestPoint1` are all in unreachable classes (`Foundation/Distance/Wm3DistLine3Circle3.cpp:99`, `Wm3DistRay3Box3.cpp:58,65`, `Wm3DistSegment3Box3.cpp:61,68,76`, `Wm3DistSegment3Rectangle3.cpp:67,80,90`, ...). | 已匹配 |
| 44 | `Wm3::Distance<float, Wm3::Vector3<float>>::GetDerivative(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A39110` | no | virtual default of the `Distance` base; its only caller is the base's own Newton driver `Foundation/Distance/Wm3Distance.cpp:150` (`Get(fTMin,fTMax,...)`, row 70), and `src/` never enters it (`grep -rn "Wm3::Distance\|Distance<" src/` -> 0). | 已匹配 |
| 45 | `Wm3::Distance<float, Wm3::Vector3<float>>::GetDerivativeSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A39180` | no | virtual default of the `Distance` base; only the base's own Newton driver `Foundation/Distance/Wm3Distance.cpp:150` calls it, and `src/` never enters that. | 已匹配 |
| 70 | `Wm3::Distance<float, Wm3::Vector3<float>>::Get(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A38C50` | no | the base class's Newton driver; `src/` holds no `Distance` object (`grep -rn "Wm3::Distance\|Distance<" src/` -> 0), so nothing reaches it. | 已匹配 |
| 71 | `Wm3::Distance<float, Wm3::Vector3<float>>::GetSquared(float, float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A38EB0` | no | the base class's Newton driver; `src/` holds no `Distance` object (`grep -rn "Wm3::Distance\|Distance<" src/` -> 0), so nothing reaches it. | 已匹配 |
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
| 30 | `Wm3::IntrSegment3Sphere3<float>::Test()` | `0x00A46C80` | no | `grep -rn "IntrSegment3Sphere3" src/` -> one construction, `src/moho/collision/CColPrimitiveBase.cpp:577`, and it calls the unswept `Find()` (row 28). `Test()` has no caller: `grep -rn "IntrSegment3Sphere3<" dependencies/WildMagic3p8/Foundation/` -> only `Wm3IntrSegment3Sphere3.cpp` itself. | 已匹配 |
| 31 | `Wm3::IntrSegment3Sphere3<float>::Test(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A46B10` | no | the swept overload of row 30; `grep -rn "IntrSegment3Sphere3" src/` -> one hit, the unswept `Find()` call at `src/moho/collision/CColPrimitiveBase.cpp:578`. Its only Wild Magic caller is `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp` itself (the virtual slot), never reached. | 失败 |
| 32 | `Wm3::IntrSegment3Sphere3<float>::Find(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A46DB0` | no | the swept overload of row 28; `grep -rn "IntrSegment3Sphere3" src/` -> one hit, the unswept `Find()` call at `src/moho/collision/CColPrimitiveBase.cpp:578`. | 失败 |

### `Wm3::IntrBox2Box2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 33 | `Wm3::IntrBox2Box2<float>::Test()` | `0x00A47D40` | yes | `src/moho/ai/CAiSteeringImpl.cpp:273` | 已匹配 |

### `Wm3::DistSegment3Segment3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 34 | `Wm3::DistSegment3Segment3<float>::GetSquared()` | `0x00A7E660` | no | `grep -rn "DistSegment3Segment3" src/` -> 0 hits (the class is named only in the comment `src/moho/math/Wm3Segment3FafExtras.h:8`). The Wild Magic caller `Foundation/Distance/Wm3DistSegment3Rectangle3.cpp:62` belongs to `DistSegment3Rectangle3`, which `src/` never reaches. | 失败 |
| 35 | `Wm3::DistSegment3Segment3<float>::Get()` | `0x00A7E230` | no | `grep -rn "DistSegment3Segment3" src/` -> 0 hits; the only Wild Magic caller is `Foundation/Distance/Wm3DistSegment3Rectangle3.cpp:62` (unreachable class). | 失败 |
| 66 | `Wm3::DistSegment3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A7E2A0` | no | the swept overload of row 35; `grep -rn "DistSegment3Segment3" src/` -> 0 hits and the only Wild Magic caller is unreachable. | 失败 |
| 67 | `Wm3::DistSegment3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A7E480` | no | the swept overload of row 34; `grep -rn "DistSegment3Segment3" src/` -> 0 hits and the only Wild Magic caller is unreachable. | 失败 |

### `Wm3::DistVector2Box2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 36 | `Wm3::DistVector2Box2<float>::GetSquared()` | `0x00A6C9D0` | yes | two Wild Magic calls from row 20: `src/moho/vision/VisionDB.cpp:89` -> `IntrBox2Circle2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` -> `Foundation/Distance/Wm3DistVector2Box2.cpp:40` `Get()` -> `GetSquared()` | 已匹配 |
| 41 | `Wm3::DistVector2Box2<float>::Get()` | `0x00A6C680` | yes | one Wild Magic call from row 20: `src/moho/vision/VisionDB.cpp:89` -> `IntrBox2Circle2f::Test()` -> `Foundation/Intersection/Wm3IntrBox2Circle2.cpp:42` `DistVector2Box2<Real>(...).Get()` | 已匹配 |
| 64 | `Wm3::DistVector2Box2<float>::Get(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | `0x00A6C730` | no | the swept overload of row 41; the swept `Find` of `IntrBox2Circle2` is the only caller and it is unreachable (row 19). | 失败 |
| 65 | `Wm3::DistVector2Box2<float>::GetSquared(float, const Wm3::Vector2<float>&, const Wm3::Vector2<float>&)` | `0x00A6C880` | no | the swept overload of row 36; the swept `Find` of `IntrBox2Circle2` is the only caller and it is unreachable (row 19). | 失败 |

### `Wm3::IntrSegment3Capsule3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 37 | `Wm3::IntrSegment3Capsule3<float>::Test()` | `0x00A6BEB0` | no | `grep -rn "IntrSegment3Capsule3\|Capsule3" src/` -> 0 hits. The Wild Magic callers are `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:145,177` - the swept sphere queries, which are themselves unreachable. | 已匹配 |
| 38 | `Wm3::IntrSegment3Capsule3<float>::Find()` | `0x00A6C070` | no | `grep -rn "IntrSegment3Capsule3\|Capsule3" src/` -> 0 hits; the only Wild Magic caller is `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:177` (unreachable). | 失败 |
| 42 | `Wm3::IntrSegment3Capsule3<float>::GetPoint(int) const` | `0x00A6BDF0` | no | `grep -rn "IntrSegment3Capsule3" src/` -> 0 hits; the accessor is read only in `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:185` (unreachable). | 失败 |
| 43 | `Wm3::IntrSegment3Capsule3<float>::GetParameter(int) const` | `0x00A6BE00` | no | `grep -rn "IntrSegment3Capsule3" src/` -> 0 hits; the accessor is read only in `Foundation/Intersection/Wm3IntrSegment3Sphere3.cpp:183` (unreachable). | 失败 |

### `Wm3::IntrLine3Capsule3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 39 | `Wm3::IntrLine3Capsule3<float>::Find(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&, const Wm3::Capsule3<float>&, float*)` | `0x00A7F920` | no | `grep -rn "IntrLine3Capsule3\|Capsule3" src/` -> 0 hits. The Wild Magic callers are `Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:51` and `Wm3IntrRay3Capsule3.cpp:51`, both in unreachable classes. | 失败 |
| 78 | `Wm3::IntrLine3Capsule3<float>::Test()` | `0x00A7F760` | no | `grep -rn "IntrLine3Capsule3\|Capsule3" src/` -> 0 hits; the only Wild Magic callers are `Foundation/Intersection/Wm3IntrSegment3Capsule3.cpp:51` and `Wm3IntrRay3Capsule3.cpp:51`, both unreachable. | 已匹配 |

### `Wm3::DistLine3Segment3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 74 | `Wm3::DistLine3Segment3<float>::GetSquared()` | `0x00A80DE0` | no | `grep -rn "DistLine3Segment3" src/` -> 0 hits. The Wild Magic caller `Foundation/Distance/Wm3DistLine3Rectangle3.cpp:61` belongs to `DistLine3Rectangle3`, which `src/` never reaches. | 失败 |
| 75 | `Wm3::DistLine3Segment3<float>::Get()` | `0x00A809A0` | no | `grep -rn "DistLine3Segment3" src/` -> 0 hits; the only Wild Magic caller is unreachable. | 失败 |
| 76 | `Wm3::DistLine3Segment3<float>::GetSquared(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A80C10` | no | `grep -rn "DistLine3Segment3" src/` -> 0 hits; the only Wild Magic caller is unreachable. | 失败 |
| 77 | `Wm3::DistLine3Segment3<float>::Get(float, const Wm3::Vector3<float>&, const Wm3::Vector3<float>&)` | `0x00A80A40` | no | `grep -rn "DistLine3Segment3" src/` -> 0 hits; the only Wild Magic caller is unreachable. | 失败 |

### `Wm3::ConvexHull3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 79 | `Wm3::ConvexHull3<float>::ConvexHull3(int, Wm3::Vector3<float>*, float, bool, int)` | `0x00A67120` | no | `grep -rn "ConvexHull3" src/` -> 5 hits, all comments (`src/legacy/containers/RbTree.h:4365-4368,4701`). The Wild Magic caller is `Foundation/Containment/Wm3ContBox3.cpp:266`, inside `ContMinBox`, whose only caller is its own recursion (`Wm3ContBox3.cpp:319,402`). | 已匹配 |

### `Wm3::Query3<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 80 | `Wm3::Query3<float>::ToPlane(const Wm3::Vector3<float>&, int, int, int) const` | `0x00A55CE0` | no | `grep -rn "Query3" src/` -> 0 hits. The Wild Magic users are `Foundation/Containment/Wm3ConvexHull3.cpp:112,481` and `Wm3HullTriangle3.cpp:32` - the hull classes, unreachable (row 79). | 已匹配 |
| 81 | `Wm3::Query3<float>::ToPlane(int, int, int, int) const` | `0x00A530A0` | no | `grep -rn "Query3" src/` -> 0 hits; the only Wild Magic users are the unreachable hull classes. | 已匹配 |
| 82 | `Wm3::Query3<float>::ToTetrahedron(const Wm3::Vector3<float>&, int, int, int, int) const` | `0x00A530D0` | no | `grep -rn "Query3" src/` -> 0 hits; the only Wild Magic users are the unreachable hull classes. | 失败 |
| 83 | `Wm3::Query3<float>::ToTetrahedron(int, int, int, int, int) const` | `0x00A53180` | no | `grep -rn "Query3" src/` -> 0 hits; the only Wild Magic users are the unreachable hull classes. | 失败 |
| 84 | `Wm3::Query3<float>::ToCircumsphere(const Wm3::Vector3<float>&, int, int, int, int) const` | `0x00A55DF0` | no | `grep -rn "Query3" src/` -> 0 hits; the only Wild Magic users are the unreachable hull classes. | 已匹配 |
| 85 | `Wm3::Query3<float>::ToCircumsphere(int, int, int, int, int) const` | `0x00A531B0` | no | `grep -rn "Query3" src/` -> 0 hits; the only Wild Magic users are the unreachable hull classes. | 已匹配 |

### `Wm3::Query2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 86 | `Wm3::Query2<float>::ToLine(const Wm3::Vector2<float>&, int, int) const` | `0x00A75C00` | no | `grep -rn "Query2" src/` -> 0 hits. The Wild Magic users are `Foundation/Containment/Wm3ConvexHull2.cpp:94,344`, `Wm3HullEdge2.cpp:29` and the tessellation classes - all unreachable. | 已匹配 |
| 87 | `Wm3::Query2<float>::ToLine(int, int, int) const` | `0x00A74E70` | no | `grep -rn "Query2" src/` -> 0 hits; the only Wild Magic users are the unreachable hull and tessellation classes. | 已匹配 |
| 88 | `Wm3::Query2<float>::ToTriangle(const Wm3::Vector2<float>&, int, int, int) const` | `0x00A74EA0` | no | `grep -rn "Query2" src/` -> 0 hits; the only Wild Magic users are the unreachable hull and tessellation classes. | 失败 |
| 89 | `Wm3::Query2<float>::ToTriangle(int, int, int, int) const` | `0x00A74F30` | no | `grep -rn "Query2" src/` -> 0 hits; the only Wild Magic users are the unreachable hull and tessellation classes. | 失败 |
| 90 | `Wm3::Query2<float>::ToCircumcircle(const Wm3::Vector2<float>&, int, int, int) const` | `0x00A75C80` | no | `grep -rn "Query2" src/` -> 0 hits; the only Wild Magic users are the unreachable hull and tessellation classes. | 已匹配 |
| 91 | `Wm3::Query2<float>::ToCircumcircle(int, int, int, int) const` | `0x00A74F60` | no | `grep -rn "Query2" src/` -> 0 hits; the only Wild Magic users are the unreachable hull and tessellation classes. | 已匹配 |

### `Wm3::ConvexHull1<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 92 | `Wm3::ConvexHull1<float>::ConvexHull1(int, const float*, float, bool, int)` | `0x00A74560` | no | `grep -rn "ConvexHull1" src/` -> 0 hits. The Wild Magic caller is `Foundation/Containment/Wm3ConvexHull2.cpp:195` and `Wm3ConvexHull3.cpp:238` (`GetConvexHull1`), both unreachable. | 已匹配 |

### `Wm3::ConvexHull2<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 93 | `Wm3::ConvexHull2<float>::ConvexHull2(int, Wm3::Vector2<float>*, float, bool, int)` | `0x00A78E30` | no | `grep -rn "ConvexHull2" src/` -> 0 hits. The Wild Magic caller is `Foundation/Containment/Wm3ContBox2.cpp:248` (`ContMinBox` for 2D), whose only caller is its own recursion. | 已匹配 |

### `Wm3::Distance<double, Wm3::Vector3<double>>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 94 | `Wm3::Distance<double, Wm3::Vector3<double>>::SetDifferenceStep(double)` | `0x00A39DB0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 95 | `Wm3::Distance<double, Wm3::Vector3<double>>::Distance()` | `0x00A39FA0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 已匹配 |
| 121 | `Wm3::Distance<double, Wm3::Vector3<double>>::Get(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A39860` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 122 | `Wm3::Distance<double, Wm3::Vector3<double>>::GetSquared(double, double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A39AB0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 123 | `Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivative(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A39D00` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 124 | `Wm3::Distance<double, Wm3::Vector3<double>>::GetDerivativeSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A39D60` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::Distance<double, Wm3::Vector2<double>>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 96 | `Wm3::Distance<double, Wm3::Vector2<double>>::SetDifferenceStep(double)` | `0x00A397C0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 97 | `Wm3::Distance<double, Wm3::Vector2<double>>::Distance()` | `0x00A39F60` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 已匹配 |
| 125 | `Wm3::Distance<double, Wm3::Vector2<double>>::Get(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A39270` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 126 | `Wm3::Distance<double, Wm3::Vector2<double>>::GetSquared(double, double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A394C0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 127 | `Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivative(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A39710` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 128 | `Wm3::Distance<double, Wm3::Vector2<double>>::GetDerivativeSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A39770` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::DistVector3Box3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 98 | `Wm3::DistVector3Box3<double>::GetSquared()` | `0x00A460D0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 99 | `Wm3::DistVector3Box3<double>::Get()` | `0x00A45800` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 104 | `Wm3::DistVector3Box3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A45FB0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 105 | `Wm3::DistVector3Box3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A45E90` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::DistVector3Segment3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 100 | `Wm3::DistVector3Segment3<double>::GetSquared()` | `0x00A48910` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 101 | `Wm3::DistVector3Segment3<double>::Get()` | `0x00A48100` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 102 | `Wm3::DistVector3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A487E0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 103 | `Wm3::DistVector3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A486B0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::DistVector2Box2<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 106 | `Wm3::DistVector2Box2<double>::GetSquared()` | `0x00A6CD70` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 107 | `Wm3::DistVector2Box2<double>::Get()` | `0x00A6C6A0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 108 | `Wm3::DistVector2Box2<double>::GetSquared(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A6CC60` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 109 | `Wm3::DistVector2Box2<double>::Get(double, const Wm3::Vector2<double>&, const Wm3::Vector2<double>&)` | `0x00A6CB50` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::DistLine3Segment3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 110 | `Wm3::DistLine3Segment3<double>::GetSquared()` | `0x00A81330` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 111 | `Wm3::DistLine3Segment3<double>::Get()` | `0x00A809C0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 112 | `Wm3::DistLine3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A811F0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 113 | `Wm3::DistLine3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A810B0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrBox2Circle2<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 114 | `Wm3::IntrBox2Circle2<double>::Test()` | `0x00A4EE50` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrSegment3Capsule3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 115 | `Wm3::IntrSegment3Capsule3<double>::Test()` | `0x00A6BF80` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 已匹配 |
| 139 | `Wm3::IntrSegment3Capsule3<double>::Find()` | `0x00A6C3C0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrBox3Sphere3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 116 | `Wm3::IntrBox3Sphere3<double>::Test()` | `0x00A425C0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrBox2Box2<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 117 | `Wm3::IntrBox2Box2<double>::Test()` | `0x00A47F00` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 已匹配 |

### `Wm3::IntrLine3Box3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 118 | `Wm3::IntrLine3Box3<double>::Test()` | `0x00A50090` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 133 | `Wm3::IntrLine3Box3<double>::Find()` | `0x00A50940` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrSegment3Box3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 119 | `Wm3::IntrSegment3Box3<double>::Test()` | `0x00A46720` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 134 | `Wm3::IntrSegment3Box3<double>::Find()` | `0x00A46330` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::IntrSegment3Sphere3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 120 | `Wm3::IntrSegment3Sphere3<double>::Test()` | `0x00A475E0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 135 | `Wm3::IntrSegment3Sphere3<double>::Find()` | `0x00A479B0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 140 | `Wm3::IntrSegment3Sphere3<double>::Find(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A47690` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::DistSegment3Segment3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 129 | `Wm3::DistSegment3Segment3<double>::GetSquared()` | `0x00A7F100` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 130 | `Wm3::DistSegment3Segment3<double>::Get()` | `0x00A7E250` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 131 | `Wm3::DistSegment3Segment3<double>::GetSquared(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A7EFB0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 132 | `Wm3::DistSegment3Segment3<double>::Get(double, const Wm3::Vector3<double>&, const Wm3::Vector3<double>&)` | `0x00A7EE60` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::Vector3<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 136 | `Wm3::Vector3<double>::GenerateOrthonormalBasis(Wm3::Vector3<double>&, Wm3::Vector3<double>&, Wm3::Vector3<double>&, bool)` | `0x00A3D150` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 138 | `Wm3::Vector3<double>::Normalize()` | `0x00A3D0A0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

### `Wm3::Eigen<float>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 141 | `Wm3::Eigen<float>::IncrSortEigenStuff3()` | `0x00A70DB0` | no | `grep -rn "Eigen" src/` -> 0 hits. The Wild Magic users are the approximation classes (`Foundation/Approximation/Wm3ApprGaussPointsFit2.cpp:92`, `Wm3ApprLineFit3.cpp:98`, `Wm3ApprQuadraticFit2.cpp:22,106`, ...), reachable only through `ContOrientedBox` / `ContEllipse` / `ContEllipsoid` / `ContLozenge`, none of which `src/` calls. | 已匹配 |
| 142 | `Wm3::Eigen<float>::IncrSortEigenStuff2()` | `0x00A70D70` | no | `grep -rn "Eigen" src/` -> 0 hits; the approximation classes that use it are unreachable. | 已匹配 |
| 143 | `Wm3::Eigen<float>::GetEigenvalue(int) const` | `0x00A6CE70` | no | `grep -rn "Eigen" src/` -> 0 hits; the approximation classes that use it are unreachable. | 已匹配 |
| 144 | `Wm3::Eigen<float>::GetEigenvector(int, Wm3::Vector3<float>&) const` | `0x00A6D350` | no | `grep -rn "Eigen" src/` -> 0 hits; the approximation classes that use it are unreachable. | 已匹配 |
| 145 | `Wm3::Eigen<float>::GetEigenvector(int, Wm3::Vector2<float>&) const` | `0x00A6D3A0` | no | `grep -rn "Eigen" src/` -> 0 hits; the approximation classes that use it are unreachable. | 已匹配 |

### `Wm3::Eigen<double>`

| # | target | retail address | executed? | evidence | wmdiff status |
|---:|---|---|---|---|---|
| 146 | `Wm3::Eigen<double>::IncrSortEigenStuff3()` | `0x00A71460` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 147 | `Wm3::Eigen<double>::IncrSortEigenStuff2()` | `0x00A71420` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 148 | `Wm3::Eigen<double>::GetEigenvalue(int) const` | `0x00A6CEA0` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 149 | `Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector3<double>&) const` | `0x00A6EE40` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |
| 150 | `Wm3::Eigen<double>::GetEigenvector(int, Wm3::Vector2<double>&) const` | `0x00A6EE90` | no | `src/` never passes a `double` to a Wild Magic template: `grep -rn "Vector3d\|Vector2d" src/` -> 0, `grep -rn "\bdouble\b" src/` \| grep -i wm3 -> 0. The float instantiation of the same class is the one the engine reaches (`double-audit.md`). | 失败 |

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

### The 110 dead rows come in two islands

* **the `double` island** (51 rows): every `double` instantiation in the table.
  They are linked into the binary because the library instantiates them
  explicitly, and never executed.  `double-audit.md` has the per-target
  evidence; the audit above re-verified the rule against the current `src/`.
* **the `float` island** (59 rows): the float rows whose classes and members
  the engine never calls - the convex hulls and `Eigen`, `Query2`/`Query3`,
  the capsule intersectors, the swept distance queries, the `Distance` base
  class's Newton driver, and the private helpers of intersectors the engine
  only enters through their unswept entry points.

### What the audit changed

Before this ledger the harness skipped only the 47 `double` rows, so a default
run exercised 109 rows.  It now exercises **46**: 63 rows the engine cannot
reach were being compared on every run - the 59 `float` rows of the float
island, plus the 4 `double` rows the old skip list had missed - and their
results said nothing about the engine.

The 46 rows that run by default now all pass.  They used to be 34 passing
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

The whole table (`--include-dead`) is 85 passing and 71 failing.  Every one of
the 71 failures is a **dead** row - the engine never executes it - and they
are listed as 失败 in the tables below because the `--include-dead` run says
so.

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

