# wmdiff: consolidated conclusions

Scope: the whole `wmdiff` comparison table for the Wild Magic 3.8 tree, as of
2026-10-29. This document states what the table says, why, and where the
remaining differences belong. The analysis (§3–§5) is a summary, not new
evidence: every claim below carries the pointer to the document, file:line,
address or bit pattern that supports it. The counts are given twice — the
state **before** the x87 fixes (§2.1) and the state **after** them (§2.2) —
because the analysis is the evidence for why the fixes look the way they do.

The four source documents:

* `tools/wmdiff/double-audit.md` — the 47 `double` targets, all **dead**.
* `tools/wmdiff/float-analysis-ordinary.md` — the 17 ordinary-input float
  failures, and their per-row resolution.
* `tools/wmdiff/float-analysis-extreme.md` — the 17 extreme-input failures.
* `tools/wmdiff/x87math-usage.md` — what `src/legacy/math/X87Math` does and does not cover.

---

## 1. The question and the answer

**Is the Wild Magic 3.8 tree in `dependencies/WildMagic3p8` the right tree, and
if not, where do the retail-versus-rebuild differences come from?** The tree is
genuine WM3.8: for every failing float row the retail body's operation sequence
is the WM3.8 source tree's, down to shared constants
(`Math<float>::ZERO_TOLERANCE` at `0x00D71BFC` in both the retail's
`DistLine3Segment3` body and `Foundation/Distance/Wm3DistLine3Segment3.cpp:46-105`).
The differences are overwhelmingly **codegen** — the retail's Wild Magic bodies
are x87 code evaluated under `_PC_24` (24-bit significand, extended exponent),
while the rebuild is compiled `/arch:SSE2` (`cmake/FafCodegen.cmake:64-67`) and
rounds every intermediate to binary32 — so the three mechanisms that recur are
**evaluation order**, **precision / exponent range**, and the **root path**.
Exactly **one** genuine source divergence was found: the retail's
`Query2::ToTriangle` / `Query3::ToTetrahedron` combine the simplex signs with
**OR** where the rebuild (and the WM4 text it was translated from) uses **AND**
(`float-analysis-extreme.md` §3.3). Exactly **one** harness-semantics issue was
found: the retail's `ContAlignedBox` tests the mask byte against **0**, while
the rebuild's `bool` test compiles to `== 1`, and the harness's mode-4 generator
feeds `0x80` bytes (`float-analysis-extreme.md` §3.2). Nothing else in the table
points at a wrong algorithm.

**The codegen differences have since been closed at the source.**  Two commits
on the submodule's `my0` branch (`59f1614`, `9af34dc`) reproduce the retail's
x87 chains; §6.1 lists what changed, row by row.  The live suite — the 46 rows
the engine executes — is now 46/46 on all five seeds at 1 000 000 draws.  The
sections below are kept as they were written, because they are the evidence for
*why* the fixes look the way they do; the counts in §2 are given both before
and after.

## 2. The numbers

### 2.1 Before the fixes (historical)

This is the state the analysis below was written against; it is kept because
it is the evidence for *why* the fixes look the way they do.  The harness then
ran 109 rows (it skipped only the 47 `double` rows).

| quantity | count | evidence |
|---|---:|---|
| rows in the comparison table | **156** | `double-audit.md` §1 ("156 项中 75 过 81 败") |
| of those, `double` rows | **47** | `double-audit.md` §1; **all 47 are `dead`** — no `src/` call site passes a `double` to a WM3 template |
| rows the harness actually ran (default, then) | **109** | 156 − 47; the harness skips the dead ones unless `--include-dead` is given |
| of the 109: **pass** | **75** | bit-identical over the corpus |
| of the 109: **fail** | **34** | the entire remainder of the failure set |
| — ordinary-input failures | **17** | `float-analysis-ordinary.md` §1 — **all C (codegen)**; 9 of 17 reproduced bit-exactly, 8 attributed by inheritance |
| — extreme-input failures | **17** | `float-analysis-extreme.md` §1–§2 |

The 17 extreme rows split as: **10 C-denormal** (the x87 range family — the
retail's 80-bit intermediates do not overflow where the rebuild's binary32 ones
do), **4 S** (`Query2::ToTriangle` ×2, `Query3::ToTetrahedron` ×2 — OR vs AND),
**2 H** (`ContAlignedBox` ×2 — the mask-byte test), **1 C-order**
(`GenerateOrthonormalBasis`, an ordinary 1-2 ulp x87-vs-binary32 difference that
is in the list only because the harness's extreme filter hides it).
`C-nan`: **0** — no row's difference is decided by NaN quieting or payload
propagation (`float-analysis-extreme.md` §5.1).

The 17 ordinary rows split as: **2** evaluation order (`Dot`), **1** root path
(`InvSqrt`), **14** precision / exponent range (four of which also carry an
association difference) — **S 0, H 0** (`float-analysis-ordinary.md` §6).

Command lines that produce those numbers:

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --max-mismatch 0
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme --max-mismatch 0
```

Full run: 75/109 pass, 34 fail. With `--skip-extreme`: 92/109 pass, 17 fail
(`float-analysis-extreme.md` §1).

### 2.2 After the fixes (current)

Measured on 2026-10-29 with the two submodule commits of §6 landed:

| quantity | count | evidence |
|---|---:|---|
| rows in the comparison table | **156** | `wmdiff.exe --list` |
| rows the engine executes | **46** | `reachable.md` — one row per target, with the quoted `src/` call site |
| rows the engine never executes (`dead`) | **110** | `reachable.md`; the harness skips them unless `--include-dead` is given |
| default run: **pass / fail** | **46 / 0** | `--iters 1000000` on each of the five seeds |
| whole table (`--include-dead`, 20 000 draws): **pass / fail** | **85 / 71** | `--include-dead` |
| of the 71 whole-table failures: rows the engine executes | **0** | every one is a `dead` row |
| of the 71: `double` rows | **47** | `double-audit.md` |
| of the 71: `float` rows | **24** | the `float` dead rows listed in `reachable.md` |

Command lines that produce those numbers:

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 1000000 --seed 0x5EED2007
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 20000 --include-dead
```

**The engine-facing surface is clean.**  The default run is 46/46 on all five
seeds (`0x5EED2007`, `0xDEADBEEF`, `0x12345678`, `0xC0FFEE01`, `0xA5A5A5A5`) at
1 000 000 draws.  The 71 remaining failures are all rows the engine never
executes.

## 3. The mechanisms

*Each mechanism below was identified while the rows were still failing.  The
ones that reach the engine have been fixed at the source (§6.1); the ones that
do not are listed in §7.*

### 3.1 Evaluation order — `Vector3<float>::Dot` (`0x00452AD0`)

The retail's x87 body sums the dot product **right-to-left**:
`((z·z' + y·y') + x·x')` — `faddp st(1)` at `0x00452AE0` merges the z and y
products, the second `faddp st(1)` at `0x00452AE8` adds the x product. The
source tree (`dependencies/WildMagic3p8/Foundation/Math/Wm3Vector3.h:145-148`)
writes `a.x*b.x + a.y*b.y + a.z*b.z`, which associates **left-to-right**. Same
three products, same two adds, different association; addition is commutative
but not associative.

Worked example (`float-analysis-ordinary.md` §2.1):

```
a = (3EF04ABF, BF35D5B2, 3F064D59)      b = (BF2B475D, BF3A3379, BE1C658D)
source tree (left-to-right, binary32)  3DFAE21F
retail      (right-to-left, x87)       3DFAE220
```

The model check is 400/400 in both directions: the x87 model with the retail's
association reproduces the retail bits, the binary32 model with the source
association reproduces the rebuild's bits.

The four `DistLine3Segment3` rows carry an association difference of their own
on the long `fSqrDist` sum (`0x00A80DE0`): 284/400 sampled mismatches are
subnormal, the remaining 116 are in the normal range and differ by 1..8 ulp.

### 3.2 Root path — `Math<float>::InvSqrt` (`0x0050D150`)

Retail: `fld dword [ebp+8]; fsqrt; fld1; fdivrp st(1)` — the root is a 24-bit
x87 `FSQRT` under `_PC_24` and the quotient a 24-bit division, both computed in
the register file. The rebuild (`Foundation/Math/Wm3Math.inl:93-96`) writes
`(Real)(1.0/sqrt((double)fValue))` — a binary64 root and a binary64 division.

The discriminating knob is the **root width**, not the quotient width:

| model | matches retail | matches rebuild |
|---|---|---|
| 24-bit root, 24-bit divide | **400/400** | 0/400 |
| 24-bit root, 53-bit divide | 400/400 | 0/400 |
| 53-bit root, 24-bit divide | 0/400 | 0/400 |
| 53-bit root, 53-bit divide | 0/400 | **400/400** |

20 585 mismatches in 200 000 draws — the highest rate of the precision family,
because `1/√x` is a reciprocal and amplifies the root's relative error.

### 3.3 Precision and exponent range — the x87 register file versus binary32 SSE

The retail's Wild Magic bodies are x87: products and sums stay in the 80-bit
register file (24-bit significand under `_PC_24`, **15-bit exponent**) and are
rounded to binary32 only where the body stores them (`FSTP dword ptr ...`). The
rebuild is `/arch:SSE2`: every product and every sum is rounded to binary32, and
an intermediate that leaves binary32's range becomes ±inf **at that step**.

Two ends of the same mechanism:

* **Overflow end.** `DistVector3Box3<float>` (`0x00A45C00`): the retail's dot
  product stays extended (exact sum 3.3196e44) and overflows to +inf only at the
  final `FSTP`; the rebuild's `mulss` overflows each product, so the sum is NaN
  (`ours 5CC63F8A` = 4.46415e+17 against `retail 7F800000` = inf).
* **Subnormal end.** `Vector3<float>::LengthSq` / `SquaredLength`
  (`0x004757E0`): the retail keeps `x², y², z²` and the running sum in the
  register, where `1e-40` is a normal 24-bit number; the rebuild rounds each to
  binary32, where it is subnormal with a handful of significand bits. 514/518
  mismatches in 200 000 draws — rare, because it needs a vector whose squared
  length falls into the subnormal band.

This accounts for **14 of the 17** ordinary rows and **10 of the 17** extreme
rows. It is a property of the numeric domain, not of the source: the same
source expression is correct in both.

### 3.4 NaN and denormal

The overflow end turns into a NaN, and the NaN — not the value — is what flips
the comparison. `IntrBox3Sphere3<float>::Test` (`0x00A41560`): `fAy` is the
stored -inf in the retail (the exact sum is -6.88e54) but NaN in the rebuild
(the products overflow to -inf and +inf); `fDy*fDy + fDz*fDz` is then +inf
against NaN, and `lhs <= r*r` is true against false. `IntrLine3Box3<float>::Test`
(`0x00A4FE60`) is the same shape: `Extent[0]·afAWdU[1]` = 3.691319428298697e+39
stays finite in the retail (so `fRhs` = -inf) and becomes +inf in binary32, so
the sum is `+inf + (-inf)` = NaN and the test does not reject.

Two secondary differences ride along: the retail compares with `fcomp` (x87,
where *unordered* is a separate case) while the rebuild uses `ucomiss` (where
*unordered* is "not above"), and `inf - inf` in the rebuild's `IntrSegment3Box3`
path. `C-nan` is **0**: the NaN-payload-only tallies (+36 for
`GetClosestPoint1`, +11 for `IntrSegment3Box3::GetPoint`) are by-products of the
overflow differences, not a separate class.

### 3.5 The harness's `--skip-extreme` defect

`--skip-extreme` is implemented in `tools/wmdiff/src/main.cpp:704-712` as a test
on **every word** of the input block, interpreted as a float:

```cpp
if (IsNan(in[k]) || std::fabs(static_cast<double>(FromBits(in[k]))) > 9.2e18
  || (FromBits(in[k]) != 0.0f && std::fabs(static_cast<double>(FromBits(in[k]))) < 1.2e-38)) {
  extreme = true;
}
```

Rows that put **non-float words** into the input block — the point count, the
vertex indices, the `bSolid` flag, the `bUnitLengthW` flag, the mask bytes —
are therefore entirely dropped: the word `0x00000001` read as a float is
1.4e-45, a denormal. Measured with the counter in `.tmp/probe/`, nine rows
**ran 0 inputs** under `--skip-extreme` and reported `200000/200000
bit-identical`:

* the four `ContAlignedBox` rows,
* both `Query2::ToTriangle` rows,
* both `Query3::ToTetrahedron` rows,
* `GenerateOrthonormalBasis`.

Their "pass" under `--skip-extreme` is **vacuous**. The filter should only look
at the words that are floats; that is a hole in the instrument, not in the
rebuild (`float-analysis-extreme.md` §3, §5.2). The ten genuinely extreme rows
do run a subset of the inputs (21 925 to 121 909 of 200 000) and their failures
come from the dropped subset.

## 4. Reachability

Which of the mismatching bodies the engine actually executes
(`x87math-usage.md` §3–§4):

| mismatching body | engine reaches it? | decisive evidence |
|---|---|---|
| `Vector3<float>::Dot` (`0x00452AD0`) | **YES** | `src/moho/render/camera/VTransform.h:284` `Wm3::Vec3f::Dot(rotatedNormal, transform.pos_)`; `src/moho/render/CWldTerrainDecal.cpp:65` `Wm3::Vec3f::Dot(normal, anchor)` |
| `Vector3<float>::LengthSq` / `SquaredLength` (`0x004757E0`) | **YES** | `src/moho/collision/CColPrimitiveBase.cpp:165` `Wm3::Vector3f::LengthSq(axis)`; also `:394,587,620`; `src/moho/audio/AudioEngine.cpp:1134`; `src/moho/unit/CUnitMotion.cpp:4175,4340,4419`; `src/moho/ai/CAiSteeringImpl.cpp:603,637` and `:198,1458` via `velocity.Length()` |
| `DistVector3Segment3<float>` (`0x00A480E0`/`0x00A484F0`) | **YES** | `src/moho/ui/CUIWorldView.cpp:276`; `src/moho/projectile/Projectile.cpp:1552` |
| `Math<float>::InvSqrt` (`0x0050D150`) | no | `grep -rn "InvSqrt" src/` → only `moho::CoordsScaleByInvSqrt` (`src/moho/math/QuaternionMath.cpp:972`), never `Wm3::Math<float>::InvSqrt` |
| `DistLine3Segment3<float>` (`0x00A809A0`/`0x00A80DE0`) | no | `grep -rn "DistLine3Segment3" src/` → 0 hits |
| `DistSegment3Segment3<float>` (`0x00A7E2A0`/`0x00A7E480`) | no | 0 hits (only a comment in `src/moho/math/Wm3Segment3FafExtras.h:8`) |
| `DistVector2Box2<float>` (`0x00A6C730`/`0x00A6C880`) | **yes** - corrected | reached one level down: `Wm3IntrBox2Circle2.cpp:42` calls it from `IntrBox2Circle2f::Test()`, which the engine calls at `src/moho/vision/VisionDB.cpp:89` |
| `GenerateOrthonormalBasis` (`0x00A3CF80`) | no | 0 hits; called only from `ContMinBox`, whose only caller is its own recursion |

**`X87Math` is not a bypass.** `src/legacy/math/X87Math.*` reproduces the
arithmetic the **retail engine performed inline in its own object code** (the
retail's `FUN_005D1E70` clamp, `FUN_005B1C90` set-length, the quaternion/SLERP
chains, the Lua `math.*` library, the `/Oi` transcendentals) — operations that
have **no WM3.8 counterpart at all**. There is no `X87Math` stand-in for a
three-lane dot product or squared length, and the engine compiles and links the
WM3.8 library itself with the same `faf_codegen` flags the harness uses
(`cmake/deps/wildmagic/CMakeLists.txt:22`, `src/CMakeLists.txt:65`);
`moho::Vector3f` **is** `Wm3::Vector3f` (`src/moho/math/Vector3f.h:8`). So the
WM3.8 bodies the harness measures are literally the bodies the engine runs. The
mix is visible in one function: `src/moho/collision/CColPrimitiveBase.cpp:165`
computes `Wm3::Vector3f::LengthSq(axis)` (mismatching WM3.8 body) and then
scales the axis with `msvc8::ScaleVectorX87` (`:177`) — an x87-faithful scale
built on top of a length that is not x87-faithful.

The **S** rows are unreachable as well: `src/` contains no call to `Wm3::Query2`
or `Wm3::Query3` at all (they are reachable only through `ConvexHull`, recorded
as *linked but unreachable*), and the engine uses the **unmasked** `ContAlignedBox`
overload (`src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`), which passes
200 000/200 000 in the full run — so the **H** rows are also engine-invisible.

The ten x87-range rows **are** called by the engine (`DistVector3Box3` at
`src/moho/collision/CColPrimitiveBase.cpp:429`, `IntrSegment3Box3` at `:384`,
`IntrBox3Sphere3` at `:607`, `IntrLine3Box3` at
`src/moho/ui/UiRuntimeTypes.cpp:21007`, `GetClosestPoint1` at
`src/moho/projectile/Projectile.cpp:1566`), but their trigger needs an
intermediate that leaves the binary32 range — a product of two engine
quantities above ~1e19, or a coordinate difference that lands on a binary32
denormal. Engine geometry is of order 1..1e4 metres with unit box axes and
normalised segment directions, so that trigger is not reachable with game data
— though a mod-supplied 1e30 coordinate would reach it, the WM3 library has no
range check of its own (`float-analysis-extreme.md` §5.2).

## 5. What is not a fix

The temptation is to move the rebuild onto x87 so the two sides agree. That does
not work, and the reason is precise.

* **Per-TU x87.** The real switch is `/clang:-mno-sse`, applied per
  translation unit with
  `set_source_files_properties(... COMPILE_OPTIONS "/clang:-mno-sse")`.
  `-mfpmath=387` is only a *validity check* — it does not turn SSE off. The
  per-TU switch is ABI-safe (float returns come back in ST(0) in both modes)
  but it is incompatible with SSE intrinsics in that TU. Under `_PC_24`, clang's
  x87 and MSVC's `/arch:IA32` agree **bit for bit** — but clang's x87 keeps the
  **source** association order, while the retail's MSVC x87 sums `Dot` in the
  **reverse** order. So a per-TU x87 build does not close the gap by itself: it
  changes the numeric domain but not the association, and the association is one
  of the two things that differ.
* **`-mfpmath=387`.** A validity check only; see above.
* **`-ffloat-store`.** Unsupported by clang.
* **`-fexcess-precision=fast`.** A no-op for this code.
* **The control word.** The retail drives its own control word to `_PC_24`
  itself — `and 0x300; or 0x7f; fldcw` at `0x00aa7bd5`, with four callers —
  so the rebuild's side is not what makes the two differ. clang emits no
  `FLDCW` and inherits the runtime control word, which the engine sets to
  `_PC_24` anyway (`platform::SetX87PrecisionControl(_PC_24)`,
  `src/platform/X87Precision.h:14`).

The general reason: **under `_PC_24` the choice of FPU does not change float
results for the same association order.** The significand is 24 bits either
way. What the FPU choice changes is the *exponent range* (80-bit extended versus
binary32) — and that difference is exactly what the per-TU switch is forbidden
from being used for, because it would be "modifying the compiler options to make
the comparison pass", the one thing the project rule forbids
(`tools/wmdiff/AGENTS.md`, "不要为了让比对通过而改编译选项").

Note also that LLVM's x87 codegen is supported but legacy: open excess-precision
miscompiles remain (LLVM #44218, #89885), so a long-term x87 rebuild is not a
safe foundation either.

## 6. Where the fix belongs — and what was done

Differences converge in **the rebuilt source** or **in the retail-side wiring of
the comparison** — never in compiler flags. That leaves the WM3.8 source:

* **Association order** for the rows whose only difference is the order of the
  sum — `Dot` in `Foundation/Math/Wm3Vector3.h:145-148` (and the instance
  overload in `Wm3Vector3.inl:275-281`), and the long `fSqrDist` sum in
  `Foundation/Distance/Wm3DistLine3Segment3.cpp:46-105`. Write the expression
  so the current compiler emits the retail's order.
* **An explicit 24-bit helper — only where the exponent range matters.** For the
  rows whose difference is that a binary32 intermediate left binary32's range
  while the retail's 80-bit intermediate did not, the fix is not "use x87" but a
  source-level helper that keeps the intermediate in the extended domain (or
  otherwise reproduces the retail's rounding points). The rows that need this are
  the ones where the *range*, not the *rounding*, is the cause:
  `LengthSq`/`SquaredLength`, `DistVector3Segment3`, `DistVector2Box2`,
  `DistSegment3Segment3`, `DistLine3Segment3`, `DistVector3Box3`,
  `IntrLine3Box3`, `IntrSegment3Box3`, `IntrBox3Sphere3`,
  `GenerateOrthonormalBasis`.
* **The root path** for `InvSqrt` (`Foundation/Math/Wm3Math.inl:93-96`): the
  rebuild's binary64 `sqrt` must become a 24-bit root.
* **The `S` row**: `Query2::ToTriangle` / `Query3::ToTetrahedron` must combine
  the signs with OR, not AND (`Foundation/RationalArithmetic/Wm3Query2.inl:92`).

This is why `dependencies/WildMagic3p8` sits on branch **`my0`** (`3a00c5b`,
"the authoritative WM 3.8 tree with the FAF patches applied") rather than on
upstream `main` (`9baf054`): the branch exists precisely to carry these source
changes. The patches under `dependencies/patches/` are applied on top of the
tree and are not committed in the submodule.

The **H** row is not a source problem at all: the retail tests the mask byte
against 0, the rebuild's `bool` test is `== 1`, and the harness's generator
feeds `0x80`. The engine cannot produce a `bool` array containing `0x80`, so the
row is engine-invisible; it is a harness-input question, not a rebuild
question.

### 6.1 What was changed

**The work was done.**  Two commits on the submodule's `my0` branch carry it;
neither touches the compiler options, the harness, or the engine's own `src/`.

| function | retail address | mechanism | commit |
|---|---|---|---|
| `Vector3<float>::Dot` (static and instance) | `0x00452AD0` | x87 sum order `(z·z' + y·y') + x·x'`, every step at 24 bits | `59f1614` |
| `Vector3<float>::LengthSq` / `SquaredLength` | `0x004757E0` | x87 squares and sum kept in the extended domain | `59f1614` |
| `DistVector3Segment3<float>::GetSquared` / `Get` | `0x00A484F0` / `0x00A480E0` | the inlined dot chain, stored back to binary32 at the retail's `fstp` points | `59f1614` |
| `DistVector3Segment3<float>::GetSquared(fT,v0,v1)` / `Get(fT,v0,v1)` | `0x00A48340` / `0x00A48190` | the swept movement arithmetic on the x87 | `59f1614` |
| the callers the retail inlines with an x-first dot (`InBox`, `IntrLine3Box3`, `IntrSegment3Sphere3`, `DistVector3Box3`) | `0x00A3BC10`, `0x00A4FE60`, `0x00A471C0`, `0x00A45C00` | the dot products spelled out instead of going through the shared reverse-order `Vector3::Dot` | `59f1614` |
| `IntrLine3Box3<float>::Test` | `0x00A4FE60` | slab right-hand sides and the direction cross product in the extended domain | `9af34dc` |
| `IntrLine3Box3<float>::Clip` | `0x00A4FCC0` | clip product kept in the extended domain, quotient via `Div24`, point product rounded to 24 bits before the origin is added | `9af34dc` |
| `IntrSegment3Box3<float>::Find` / `GetPoint` | `0x00A462A0` / `0x00A46300` | inherit `Clip`'s chain | `9af34dc` |
| `IntrBox3Sphere3<float>::Test` | `0x00A41560` | inherits the inlined-dot chain of `59f1614` | `59f1614` |
| `ContAlignedBox<float>(int, const Vector3<float>*, const bool*)` | `0x00A3AE40` | the mask byte is tested against **0** on the raw byte, not as a `bool` | `9af34dc` |

The helper header is **`Foundation/Math/Wm3X87.h`** in the submodule:
`RoundTo24` / `Mul24` / `Add24` / `Sub24` / `Div24` — one x87 arithmetic
instruction emulated under `_PC_24`.  The value is carried in a binary64 whose
significand is forced back to 24 bits after every operation; the wide exponent
of the carrier is what keeps an intermediate product that leaves binary32's
range alive until the next `FADD` cancels it again.

The gate for both commits was the live suite: **46/46 at 1 000 000 draws** on
each of the five seeds, and the engine still builds
(`cmake --build --preset clang-x86-release`, exit 0).

## 7. Open items

**On the engine-facing surface there is nothing left.**  The 46 rows the engine
executes are 46/46 at 1 000 000 draws on all five seeds.  What remains open is
below the live surface.

1. **The 71 `--include-dead` failures.**  Every one of them is a row the engine
   never executes, so none of them can move the game:

   * **47 `double` rows** — `double-audit.md`; `src/` passes no `double`
     argument to a Wild Magic template anywhere.
   * **24 `float` rows**, in four groups:
     * the **`DistLine3Segment3` family** (4 rows) and the **`DistSegment3Segment3`
       family** (4 rows) — the retail's association of the long `fSqrDist` sum
       was never pinned down (`float-analysis-ordinary.md` §5), so the two
       commits did not touch them;
     * the **`DistVector2Box2` swept rows** (2) — the same double rounding in
       the movement prologue, not fixed;
     * the **capsule and swept-sphere intersectors** (`IntrSegment3Capsule3`
       `Find`/`GetPoint`/`GetParameter`, `IntrLine3Capsule3::Find`,
       `IntrSegment3Sphere3` swept `Test`/`Find`, `IntrSegment3Box3::Test`) —
       8 rows;
     * **`Math<float>::InvSqrt`** (root path), **`GenerateOrthonormalBasis`**,
       the masked **2D `ContAlignedBox`**, and the four **`Query2`/`Query3`**
       rows — see item 3.

   These are recorded, not scheduled: fixing them changes code the game never
   runs, and the rule of the harness is that a row the engine cannot reach does
   not need a comparison entry.
2. **The `--skip-extreme` measurement defect** (`main.cpp:704-712`), **still
   open**.  The filter tests *every* word of the input block as a float, so the
   rows that put non-float words there (the point count, the vertex indices,
   the `bSolid`/`bUnitLengthW` flags, the mask bytes) run **zero** inputs and
   report a vacuous `200000/200000 bit-identical`.  Nine rows are affected:
   the four `ContAlignedBox` rows, both `Query2::ToTriangle` rows, both
   `Query3::ToTetrahedron` rows and `GenerateOrthonormalBasis`.  The filter
   must be restricted to the words that are actually floats before any
   `--skip-extreme` number is quoted as evidence.
3. **The `Query2`/`Query3` OR-vs-AND source divergence** is real and **still
   unfixed**: the retail combines the simplex signs with **OR** where the
   rebuild (and the WM4 text it was translated from) uses **AND**
   (`Foundation/RationalArithmetic/Wm3Query2.inl:92`).  It is unreachable —
   `src/` never names `Wm3::Query2` or `Wm3::Query3`, they are reachable only
   through `ConvexHull` — so it is recorded rather than scheduled.  The fix is
   known and cheap.
4. **`GenerateOrthonormalBasis`** is a 1-2 ulp x87-vs-binary32 difference
   (C-order) that no commit has touched; it is dead for the same reason.
5. **`InvSqrt`**'s root path is still the rebuild's binary64 `sqrt`; the row is
   dead.

## 8. Where to read more

| document | covers |
|---|---|
| `tools/wmdiff/double-audit.md` | the 47 dead `double` rows, item by item |
| `tools/wmdiff/float-analysis-ordinary.md` | the 17 ordinary-input rows, with the bit-level models |
| `tools/wmdiff/float-analysis-extreme.md` | the 17 extreme-input rows, the `--skip-extreme` defect, engine impact |
| `tools/wmdiff/x87math-usage.md` | what `X87Math` is, and why it is not a bypass |
| `tools/wmdiff/AGENTS.md` | the operating rules, the lessons, the judgment criteria |
| `tools/wmdiff/README.md` | the row-by-row ledger (addresses, calling conventions, withdrawn conclusions) |
