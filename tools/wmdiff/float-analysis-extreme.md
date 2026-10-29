# Float comparison: the extreme-input failures

Scope: the `wmdiff` rows that **pass** with `--skip-extreme` and **fail** without
it, i.e. the rows whose difference only shows up when the generator feeds
denormals, infinities, NaNs or values outside 2^-63..2^63.

The ordinary-input float failures (`Dot`, `LengthSq`, `InvSqrt`, the
`DistVector3Segment3` / `DistLine3Segment3` / `DistVector2Box2` /
`DistSegment3Segment3` families, ...) are **not** in this document; they belong
to `float-analysis-ordinary.md`.

Nothing in the rebuild, in the harness or in the compiler options was changed.
The only artefact built for this analysis is a copy of the harness under
`.tmp/probe/` (the harness sources plus three extra options: `--probe-q2`,
which calls the retail's `Query2::ToLine` / `Query2::ToTriangle` with a
hand-made input and prints the individual signs; `--probe-l3b`, which does the
same for `IntrLine3Box3<float>::Test`; and a counter that prints how many
inputs a row actually executed under `--skip-extreme`). It links the
*unmodified* retail image and the *unmodified* rebuild objects; it is a
measuring instrument, not a fix.

---

## 1. The authoritative list

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --max-mismatch 0
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme --max-mismatch 0
```

Full run: **75/109 passed**, 34 rows fail.
With `--skip-extreme`: **92/109 passed**, 17 rows fail.
The difference (17 rows) is the subject of this document.

| # | row | retail VA | mismatches (full run) |
|---|---|---|---|
| 1 | `DistVector3Box3<float>::GetSquared()` | `0x00A45C00` | 64 |
| 2 | `DistVector3Box3<float>::Get()` | `0x00A457E0` | 130 |
| 3 | `DistVector3Box3<float>::Get(float, const Vector3<float>&, const Vector3<float>&)` | `0x00A458C0` | 117 |
| 4 | `DistVector3Box3<float>::GetSquared(float, const Vector3<float>&, const Vector3<float>&)` | `0x00A45A60` | 65 |
| 5 | `IntrLine3Box3<float>::Test()` | `0x00A4FE60` | 1257 |
| 6 | `IntrSegment3Box3<float>::Find()` | `0x00A462A0` | 108 |
| 7 | `IntrSegment3Box3<float>::GetPoint(int) const` | `0x00A46300` | 138 (+11 NaN-payload-only) |
| 8 | `IntrSegment3Box3<float>::Test()` | `0x00A46440` | 341 |
| 9 | `Distance<float,Vector3<float>>::GetClosestPoint1() const` | `0x00A39230` | 84 (+36 NaN-payload-only) |
| 10 | `IntrBox3Sphere3<float>::Test()` | `0x00A41560` | 58 |
| 11 | `ContAlignedBox<float>(int, const Vector3<float>*, const bool*)` | `0x00A3AE40` | 34947 |
| 12 | `ContAlignedBox<float>(int, const Vector2<float>*, const bool*)` | `0x00A685F0` | 35429 |
| 13 | `Query3<float>::ToTetrahedron(const Vector3<float>&, int, int, int, int) const` | `0x00A530D0` | 16542 |
| 14 | `Query3<float>::ToTetrahedron(int, int, int, int, int) const` | `0x00A53180` | 16648 |
| 15 | `Query2<float>::ToTriangle(const Vector2<float>&, int, int, int) const` | `0x00A74EA0` | 47010 |
| 16 | `Query2<float>::ToTriangle(int, int, int, int) const` | `0x00A74F30` | 46651 |
| 17 | `Vector3<float>::GenerateOrthonormalBasis(Vector3<float>&, Vector3<float>&, Vector3<float>&, bool)` | `0x00A3CF80` | 50071 |

The same difference on the `double` instantiation (7 rows: `IntrBox2Circle2<double>::Test`
`0x00A4EE50`, `IntrBox3Sphere3<double>::Test` `0x00A425C0`, `IntrLine3Box3<double>::Test`
`0x00A50090`, `IntrSegment3Box3<double>::Find` `0x00A46330`, `IntrSegment3Box3<double>::Test`
`0x00A46720`, `IntrSegment3Sphere3<double>::Test` `0x00A475E0`,
`Vector3<double>::GenerateOrthonormalBasis` `0x00A3D150`) is out of scope: the
engine never executes a `double` instantiation (`tools/wmdiff/double-audit.md`).
17 float rows + 7 double rows = the 24 rows the task lists. (The task's list
quotes `0x00A44190` for `IntrBox3Sphere3<double>::Test`; the row that answers to
that name in this harness is at `0x00A425C0`, and the *float* instantiation is
at `0x00A41560` - the addresses here are the ones the harness prints.)

### 1.1 Cheap reproduction

Every row of the table above reproduces at a small iteration count
(`--iters 5000 --seed 0x5EED2007 --filter "<row>"`, no `--skip-extreme`); the
row seeds are derived from the row's index, so the first 5000 draws are the
same draws the 200000-iteration run starts with. The one row that does not
show up in 5000 draws was pushed to 20000 and then stopped:

| row | 5000 iterations | 20000 iterations |
|---|---|---|
| `DistVector3Box3<float>::GetSquared()` | **pass** (not reproduced) | 4 mismatches ⇒ reproduced |
| `DistVector3Box3<float>::Get()` | 7 mismatches | - |
| `DistVector3Box3<float>::Get(fT,..)` | 3 | - |
| `DistVector3Box3<float>::GetSquared(fT,..)` | 4 | - |
| `IntrLine3Box3<float>::Test()` | 21 | - |
| `IntrSegment3Box3<float>::Find()` | 3 | - |
| `IntrSegment3Box3<float>::GetPoint(int)` | 9 | - |
| `IntrSegment3Box3<float>::Test()` | 4 | - |
| `Distance<float,Vector3<float>>::GetClosestPoint1()` | 4 | - |
| `IntrBox3Sphere3<float>::Test()` | 3 | - |
| `ContAlignedBox<float>(int, Vector3<float>*, const bool*)` | 857 | - |
| `ContAlignedBox<float>(int, Vector2<float>*, const bool*)` | 860 | - |
| `Query3<float>::ToTetrahedron(const Vector3<float>&, ...)` | 422 | - |
| `Query3<float>::ToTetrahedron(int, ...)` | 408 | - |
| `Query2<float>::ToTriangle(const Vector2<float>&, ...)` | 1146 | - |
| `Query2<float>::ToTriangle(int, ...)` | 1195 | - |
| `Vector3<float>::GenerateOrthonormalBasis(...)` | 1255 | - |

(The two unmasked `ContAlignedBox` rows pass at 5000 and at 200000.)

---

## 2. Summary table

| # | target | retail addr | failing input class | verdict | mechanism | evidence |
|---|---|---|---|---|---|---|
| 1 | `DistVector3Box3<float>::GetSquared()` | `0x00A45C00` | large magnitudes (products overflow binary32) | **C-denormal** (x87 range family, *overflow* end) | the retail's dot product stays in the 80-bit register (3.32e44) and overflows to +inf only at the final `FSTP`; the rebuild's `mulss` overflows each product to ±inf, so the sum is NaN | §4.1 |
| 2 | `DistVector3Box3<float>::Get()` | `0x00A457E0` | same | **C-denormal** | same body, `sqrt` of the differing squared distance | §4.1 |
| 3 | `DistVector3Box3<float>::Get(fT,v0,v1)` | `0x00A458C0` | same | **C-denormal** | swept variant, same dot product | §4.1 |
| 4 | `DistVector3Box3<float>::GetSquared(fT,v0,v1)` | `0x00A45A60` | same | **C-denormal** | swept variant, same dot product | §4.1 |
| 5 | `IntrLine3Box3<float>::Test()` | `0x00A4FE60` | large magnitudes (products overflow binary32) | **C-denormal** (x87 range family) | `Extent[0]·afAWdU[1]` = 3.69e39 stays finite in the retail's x87 (so `fRhs` = -inf) but overflows to +inf in binary32 (so `fRhs` = NaN) ⇒ `afAWxDdU[2] > fRhs` is true in x87 and unordered in SSE | §4.2 |
| 6 | `IntrSegment3Box3<float>::Find()` | `0x00A462A0` | inf / very large | **C-denormal** | `DoClipping`: the box-local direction components overflow binary32 | §4.3 |
| 7 | `IntrSegment3Box3<float>::GetPoint(int) const` | `0x00A46300` | inf / very large | **C-denormal** | the point is interpolated from the clipped parameter, which differs for the same reason | §4.3 |
| 8 | `IntrSegment3Box3<float>::Test()` | `0x00A46440` | inf / very large | **C-denormal** | the three `|dot|` terms are computed in x87 and rounded once; in binary32 they overflow and the `inf - inf` makes a NaN | §4.3 |
| 9 | `Distance<float,Vector3<float>>::GetClosestPoint1() const` | `0x00A39230` | inf / FLT_MAX | **C-denormal** | `Direction.Dot(kDiff)` overflows binary32 ⇒ the clamp branch (`-Extent < t < Extent`) is taken differently, so a different segment endpoint is produced | §4.4 |
| 10 | `IntrBox3Sphere3<float>::Test()` | `0x00A41560` | very large / denormal extents | **C-denormal** | `kCDiff·axis1` overflows to `-inf`/`+inf` (⇒ NaN) in binary32 while the x87 keeps -6.88e54 (⇒ -inf); the NaN then decides `fDy*fDy + fDz*fDz <= r*r` | §4.5 |
| 11 | `ContAlignedBox<float>(int, const Vector3<float>*, const bool*)` | `0x00A3AE40` | **none** (the row's mask bytes are the "extreme" input) | **H** (row wiring) | the retail tests the mask byte against **0**, the rebuild's `bool` test compiles to `cmp byte ptr [..], 1`; the harness's mode-4 generator feeds `0x80` bytes | §3.2 |
| 12 | `ContAlignedBox<float>(int, const Vector2<float>*, const bool*)` | `0x00A685F0` | **none** | **H** | same | §3.2 |
| 13 | `Query3<float>::ToTetrahedron(const Vector3<float>&, ...)` | `0x00A530D0` | **none** (the index words are the "extreme" input) | **S** | the retail combines the four `ToPlane` signs with **OR** (`if any sign != 0 → -1`); the rebuild uses AND (`all four != 0 → -1`) | §3.3 |
| 14 | `Query3<float>::ToTetrahedron(int, ...)` | `0x00A53180` | **none** | **S** | same body, index form | §3.3 |
| 15 | `Query2<float>::ToTriangle(const Vector2<float>&, ...)` | `0x00A74EA0` | **none** | **S** | the retail combines the three `ToLine` signs with **OR**; the rebuild uses AND | §3.3 |
| 16 | `Query2<float>::ToTriangle(int, ...)` | `0x00A74F30` | **none** | **S** | same body, index form | §3.3 |
| 17 | `Vector3<float>::GenerateOrthonormalBasis(...)` | `0x00A3CF80` | **none** (the `bool` word is the "extreme" input) | **C-order / ordinary family** (not an extreme difference) | the retail keeps `w.x²+w.y²` in the x87 register, the rebuild rounds to binary32 at each step: a 1-2 ulp difference in the *ordinary* range | §3.4 |

Counts: **C-denormal (x87 range family) 10**, **S 4**, **H 2**, **C-order/ordinary 1**,
**C-nan 0**.

---

## 3. The decisive finding first: half of these rows are not extreme failures at all

`--skip-extreme` is implemented in `tools/wmdiff/src/main.cpp:704-712` as a test on
**every word of the input block**, interpreted as a float:

```cpp
if (IsNan(in[k]) || std::fabs(static_cast<double>(FromBits(in[k]))) > 9.2e18
  || (FromBits(in[k]) != 0.0f && std::fabs(static_cast<double>(FromBits(in[k]))) < 1.2e-38)) {
  extreme = true;
}
```

Several rows put **non-float words** into the input block: the point count, the
vertex indices, the `bSolid` flag, the `bUnitLengthW` flag and the mask bytes.
A word `0x00000001` read as a float is `1.4e-45`, a denormal - so *every* input
of those rows is classified "extreme" and dropped. The rows then report
`200000/200000 bit-identical` while **not a single input was executed**.

Measured with the instrument in `.tmp/probe/` (same harness, one extra counter
printed per row):

```
[RAN] Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*)                      ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::ContAlignedBox<float>(int, const Wm3::Vector3<float>*, const bool*)          ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*)                      ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::ContAlignedBox<float>(int, const Wm3::Vector2<float>*, const bool*)          ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::Query2<float>::ToTriangle(const Wm3::Vector2<float>&, int, int, int) const    ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::Query2<float>::ToTriangle(int, int, int, int) const                          ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::Query3<float>::ToTetrahedron(const Wm3::Vector3<float>&, ...) const          ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::Query3<float>::ToTetrahedron(int, int, int, int, int) const                  ran 0 inputs, skipped 200000 extreme
[RAN] Wm3::Vector3<float>::GenerateOrthonormalBasis(...)                                ran 0 inputs, skipped 200000 extreme
```

against, for the rows whose difference really is an extreme-value difference:

```
[RAN] Wm3::DistVector3Box3<float>::GetSquared()                     ran 68656 inputs, skipped 131344 extreme
[RAN] Wm3::DistVector3Box3<float>::Get()                             ran 117116 inputs, skipped  82884 extreme
[RAN] Wm3::DistVector3Box3<float>::Get(fT, v0, v1)                   ran 118136 inputs, skipped  81864 extreme
[RAN] Wm3::DistVector3Box3<float>::GetSquared(fT, v0, v1)            ran  68565 inputs, skipped 131435 extreme
[RAN] Wm3::IntrLine3Box3<float>::Test()                              ran  57483 inputs, skipped 142517 extreme
[RAN] Wm3::IntrSegment3Box3<float>::Find()                           ran  21925 inputs, skipped 178075 extreme
[RAN] Wm3::IntrSegment3Box3<float>::GetPoint(int) const              ran  22101 inputs, skipped 177899 extreme
[RAN] Wm3::IntrSegment3Box3<float>::Test()                            ran  22134 inputs, skipped 177866 extreme
[RAN] Wm3::Distance<float, Wm3::Vector3<float>>::GetClosestPoint1()  ran 121909 inputs, skipped  78091 extreme
[RAN] Wm3::IntrBox3Sphere3<float>::Test()                            ran  81401 inputs, skipped 118599 extreme
```

Consequences:

* The whole `ContAlignedBox` family (4 rows), both `Query2::ToTriangle` rows, both
  `Query3::ToTetrahedron` rows and `GenerateOrthonormalBasis` are **untested**
  under `--skip-extreme`. Their "pass" there is vacuous.
* The ten remaining rows do run a subset of the inputs, and the failures come
  from the dropped subset - the extreme subset.
* The `IntrSegment3Box3` rows skip 89% of their inputs because the `bSolid`
  word is `0x00000001` (a denormal as a float) half of the time.

### 3.2 `ContAlignedBox<float>` (2 rows) - verdict **H**

Retail body (`0x00A3AE40`, the masked path):

```
0x00A3B30A  test ebx, ebx
0x00A3B30C  jle 0xa3b356
0x00A3B30E  cmp byte ptr [ecx + esi], 0     ; the mask byte against ZERO
0x00A3B312  jne 0xa3b31d                   ; != 0  =>  the point is valid
```

The rebuild compiles the same source (`Foundation/Containment/Wm3ContBox3.cpp:87`,
`if (abValid[i])`) to a test against **1** (clang-cl, `/O2`, `Wm3ContBox3.cpp.obj`):

```
00000090 <Wm3::ContAlignedBox<float>(int, Vector3<float> const*, bool const*)>:
       90:  cmpb   $0x1, (%edx,%ebx)        ; the mask byte against ONE
       94:  je    0x139                     ; == 1  =>  the point is valid
```

The harness's mode-4 generator deliberately produces "nonzero-but-not-one"
bytes (`tools/wmdiff/src/wm3_coll_targets.cpp:3158-3171`):

```cpp
else if (iMode == 4) {
  // Nonzero-but-not-one bytes. The retail tests the mask byte against
  // zero, so these count as valid; ...
  for (int i = 0; i < 8; ++i) { mask[i] = 0x80; }
```

so the two sides disagree on every mode-4 draw (1/5 of the draws ≈ the observed
17-18% mismatch rate; the exact-1 modes agree).

Worked example (harness output, iter 10): mask bytes `80 00 00 80 80 00 00 00`,
points 0, 3, 4 valid. The retail returns the box of points {0,3,4}
(centre `(-1.10464,-2.24789,4.18198)`, extents `(1.10464,2.24789,4.18198)`);
the rebuild returns the "no valid point" box (centre `(0,0,0)`, extents
`(-1,-1,-1)`) because it does not accept `0x80` as a `true`.

**Engine impact: none.** The engine calls the unmasked overload
(`src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`,
`Wm3::ContAlignedBox(8, vertices)` - `abValid` defaults to `0`), and the
unmasked rows pass 200000/200000 in the full run. A `bool` array with the
value `0x80` is not something the engine can produce.

### 3.3 `Query2::ToTriangle` / `Query3::ToTetrahedron` (4 rows) - verdict **S**

The retail's `Query2<float>::ToTriangle` (`0x00A74EA0`) calls the `ToLine` slot
(`+0x08` = `0x00A75C00`) three times and combines the signs:

```
00a74ebb  call *%edx          ; sign0 = ToLine(P, iV1, iV2)
00a74ebd  test %eax, %eax
00a74ec3  jle  0xa74ed0       ; sign0 > 0  =>  return +1
00a74edf  call *%edx          ; sign1 = ToLine(P, iV0, iV2)
00a74ee5  jl   0xa74efb       ; sign1 < 0  =>  return +1
00a74ef5  call *%edx          ; sign2 = ToLine(P, iV0, iV1)
00a74ef9  jle  0xa74f07       ; sign2 > 0  =>  return +1
00a74f07  cmp  dword ptr [esp + 0x20], 0x0   ; sign0 != 0 ?
00a74f0c  jnz  0xa74f1d
00a74f0e  test %ebp, %ebp                    ; sign1 != 0 ?
00a74f10  jnz  0xa74f1d
00a74f12  test %eax, %eax                    ; sign2 != 0 ?
00a74f14  jnz  0xa74f1d
00a74f16  ...  ret 0x10                      ; return 0
00a74f1d  or   %eax, 0xffffffff              ; return -1
```

That is an **OR**: *any* non-zero sign yields `-1`. The rebuild
(`Foundation/RationalArithmetic/Wm3Query2.inl:92`, and the WM4 text it was
translated from) uses an **AND**:

```cpp
return ((iSign0 && iSign1 && iSign2) ? -1 : 0);
```

`Query3<float>::ToTetrahedron` (`0x00A530D0`) has the same shape with four
`ToPlane` calls and the same OR at `0x00A5314e-0x00A53165`.

The retail's `ToLine` itself agrees with the rebuild bit for bit (the
`ToLine` rows pass 200000/200000, including the degenerate `iV0 == iV1` draws
that make the determinant exactly zero). The difference is only in the
*combination*, and it shows up exactly when one or two of the three/four signs
are zero - a point on an edge of the simplex.

Measured with the probe (the retail called directly with the harness's own
input words):

```
input: v0=(0,0) v1=(1,0) v2=(0.5,0.5)  P=(0.5,0.5)  iV0=1 iV1=2 iV2=0
PROBE retail signs 0 1 0 -> triangle -1
PROBE ours  signs 0 1 0 -> triangle 0

input: v0=(0,0) v1=(1,0) v2=(0,1)  P=v0  iV0=0 iV1=1 iV2=2
PROBE retail signs -1 0 0 -> triangle -1
PROBE ours  signs -1 0 0 -> triangle 0
```

The three signs are identical on both sides; only the final combination differs.

**Engine impact: none.** `src/` contains no call to `Wm3::Query2` or
`Wm3::Query3` at all (they are reachable only through `ConvexHull`, which
`tools/wmdiff/reachable.md` records as *linked but unreachable*). The four rows
measure WM4-derived code the engine never runs.

### 3.4 `Vector3<float>::GenerateOrthonormalBasis` - not an extreme difference

The row's input block is `w[3] unit`; `unit` is the `bool` flag and the
generator always writes `in[3] = 1u`
(`tools/wmdiff/src/wm3_coll_targets.cpp:3079`), so every input is classified
"extreme" and dropped.

The actual difference is an ordinary one - a 1-2 ulp discrepancy in ordinary
magnitudes:

```
input: w = (59.1466, 1.23298, 895.837)
retail u = BF7F719F(-0.997827) 00000000 3D86EC4E(0.0658804)
ours  u = BF7F71A1(-0.997828) 00000000 3D86EC4F(0.0658804)
```

Retail body (`0x00A3CF80`) keeps the sum on the x87:

```
0x00A3CFBA  fld  dword ptr [esi]           ; w.x
0x00A3CFBC  fmul st(0), st(0)              ; w.x * w.x
0x00A3CFBE  fld  st(1)                     ; |w.y|
0x00A3CFC0  fmulp st(2)                    ; w.y * w.y
0x00A3CFC2  faddp st(1)                    ; the sum, still in the 80-bit register
0x00A3CFC4  fstp dword ptr [esp + 0x14]    ; ... rounded to binary32 here
0x00A3CFCC  call 0xa91000                  ; the CRT sqrt
0x00A3CFD1  fld1
0x00A3CFD3  fdivrp st(1)                   ; 1/sqrt, in the register
```

The rebuild rounds to binary32 after every operation, so the sum and the
quotient differ in the last bits. This belongs to the ordinary-input family
(the same x87-vs-binary32 argument as the `Dot` row), not to the extreme
family. It is listed here only because the harness's extreme filter hides it.

**Engine impact: none.** `GenerateOrthonormalBasis` is called only from
`ContMinBox`, which is *linked but unreachable* (the only caller of
`ContMinBox` is its own recursion), and `src/` has no call to it.

---

## 4. The genuinely extreme rows: the x87's range (the "C-denormal" family)

The rebuild's Wild Magic is compiled for x86 with `/arch:SSE2`
(`cmake/FafCodegen.cmake:64-67`), so every float operation is a binary32 SSE
operation: each product and each sum is rounded to binary32, and an
intermediate that leaves the binary32 range becomes ±inf **at that step**.

The retail's Wild Magic bodies are x87: the products and the sums stay in the
80-bit register (exponent range 2^-16382..2^16383, 64-bit significand) and are
rounded to binary32 only where the body stores them (`FSTP dword ptr ...`).
A binary32 intermediate that overflows is therefore finite in the retail.

That is the whole mechanism of the ten rows below. It is a codegen property,
not a bug in the source: the same source is correct in both.

### 4.1 `DistVector3Box3<float>` (4 rows) - `0x00A45C00`, `0x00A457E0`, `0x00A458C0`, `0x00A45A60`

Retail (`0x00A45C2F-0x00A45C4E`), the dot product of `kDiff` with `Axis[0]`:

```
0x00A45C2F  fld  dword ptr [esp + 0x20]     ; diff.y
0x00A45C33  fld  dword ptr [esp + 0x1c]     ; diff.x
0x00A45C37  fld  dword ptr [esp + 0x24]     ; diff.z
0x00A45C3B  fld  dword ptr [esi + 0x10]     ; axis0.y
0x00A45C3E  fmul st(3)                     ; axis0.y * diff.y     (extended)
0x00A45C40  fld  dword ptr [esi + 0xc]      ; axis0.x
0x00A45C43  fmul st(3)                     ; axis0.x * diff.x
0x00A45C45  faddp                           ; sum, still extended
0x00A45C47  fld  dword ptr [esi + 0x14]     ; axis0.z
0x00A45C4A  fmul st(2)                     ; axis0.z * diff.z
0x00A45C4C  faddp                           ; sum, still extended
0x00A45C4E  fstp dword ptr [esp + 0x1c]     ; kClosest[0] - the FIRST rounding
```

The rebuild's compiled body (`Wm3DistVector3Box3.cpp.obj`) does the same
arithmetic in binary32:

```
      28:  mulss  %xmm5, %xmm0        ; axis0.x * diff.x
      2c:  mulps  %xmm3, %xmm4        ; axis0.y*diff.y , axis0.z*diff.z
      2f:  addss  %xmm4, %xmm0        ; + axis0.y*diff.y
      33:  shufps $0x55, %xmm4, %xmm4
      37:  addss  %xmm0, %xmm4        ; + axis0.z*diff.z
```

Worked example (harness output, `GetSquared`, iter 9877):

```
vec  = (-168.205, 976551, 0.99999994)
box  = centre (-700.519, 967.832, 38.8371)
       axis0 (1, 3.4028235e38, 3.4028235e38)   extent (-215.359, 3.03823e-09, 689.407)
diff = (532.314, 975583.17, -37.8371)
```

| | axis 0 | axis 1 | axis 2 | result |
|---|---|---|---|---|
| retail (x87) | exact sum 3.3196e44 → rounded to float → **+inf** | 5.37025e8 | -3.97517e8 | **+inf** (inf² added) |
| rebuild (SSE) | products `532.314`, **+inf**, **-inf** → sum **NaN** | 5.37025e8 | -3.97517e8 | 2.88396e17 + 1.58023e17 = **4.46415e17** |

which is exactly what the harness prints (`retail 7F800000(inf)` vs
`ours 5CC63F8A(4.46415e+17)`). The rebuild's NaN makes every comparison in
the `if (kClosest[i] < -Extent[i]) ... else if (kClosest[i] > Extent[i])` chain
false, so axis 0 contributes nothing; the retail's `+inf` makes the first test
true, so it contributes `inf`.

`Get()` (`0x00A457E0`) and the two swept forms (`0x00A458C0`, `0x00A45A60`)
call the same body and inherit the same difference.

### 4.2 `IntrLine3Box3<float>::Test()` - `0x00A4FE60`

Retail (`0x00A4FEB8-0x00A4FF47`): the three `|dot|` terms and the two `fRhs`
sums are x87 sums - the products and the additions stay in the 80-bit
register and are rounded to binary32 only where the body stores them
(`fstp dword ptr [esp + 0x4]`, then `fabs`, then `fcomp`).

Worked example (harness output, iter 46; reproduced with the probe in
`.tmp/probe/`, which prints `retail=0 ours=1` for exactly these words):

```
line  = origin (0, -9.95662, 2.39467), direction (2.36037e24, 3.0496e23, -1.55464e24)
box   = centre (-127.93, 143.175, -712.242)
        axis0 (271.57, 1.9057e31, 9.75177e-19)   axis1 (-880.745, -1.28252e13, -216.398)
        axis2 (936.976, 779.94, -123.902)        extent (943.786, -793.89, 251.023)
```

The quantities the source computes
(`Foundation/Intersection/Wm3IntrLine3Box3.cpp:39-77`):

| quantity | retail (x87) | rebuild (binary32) |
|---|---|---|
| `afAWdU[0] = \|dir·axis0\|` | +inf | +inf |
| `afAWdU[1] = \|dir·axis1\|` | 3.911182858194224e+36 | 3.911182858194224e+36 |
| `afAWxDdU[2] = \|WxD·axis2\|` | 1.4399702820931888e+30 | 1.4399702820931888e+30 |
| `fRhs = Extent[0]·afAWdU[1] + Extent[1]·afAWdU[0]` | **-inf** | **NaN** |
| `afAWxDdU[2] > fRhs` | 1.44e30 > -inf → **true** → *no intersection* | 1.44e30 > NaN → **false** (unordered) |
| result | `false` (0) | `true` (1) |

The decisive line is the third `fRhs`:

* `Extent[0]·afAWdU[1]` = `943.786 · 3.911182858194224e36` = `3.691319428298697e+39`.
  In the retail this product is computed on the x87 and **stays finite in the
  register** (the 80-bit range reaches 2^16383); the other term,
  `Extent[1]·afAWdU[0]` = `-793.89 · inf` = `-inf`; the sum is `-inf`, stored as
  `-inf`.
* In the rebuild the same product is a binary32 `mulss`: `3.6913e39` leaves the
  binary32 range and becomes `+inf` **at that step**. The sum is then
  `+inf + (-inf)` = **NaN**, and the NaN poisons the comparison
  (`ucomiss` treats unordered as "not above", so the test does not reject).

The bisect confirms the story: zeroing `dir.y`, `axis0.y`, `axis1.y`,
`extent[0]` or `extent[2]` makes the two sides agree again, while zeroing
`dir.x`, `dir.z` or `axis0.x` does not - i.e. the trigger is exactly the pair
of terms that overflow in binary32.

The rebuild's code for the same source (`Wm3IntrLine3Box3.cpp.obj`) is
`mulss`/`mulps`/`addss`/`ucomiss`; note also that the retail compares with
`fcomp` (x87, where *unordered* is a separate case) while the rebuild uses
`ucomiss` (where *unordered* is "not above"). That second difference only
matters for NaN operands, which is why the NaN-payload counts are small.

### 4.3 `IntrSegment3Box3<float>` (3 rows) - `0x00A462A0`, `0x00A46300`, `0x00A46440`

Retail `Test()` (`0x00A46440`) computes the three `|dot|` terms in x87:

```
0x00A46469  fld  dword ptr [eax + 0x10]      ; direction.x
0x00A4646C  fmul dword ptr [edx + 0x10]      ; * diff.x     (extended)
0x00A4646F  fld  dword ptr [eax + 0xc]
0x00A46472  fmul dword ptr [edx + 0xc]       ; * diff.y
0x00A46475  faddp                              ; extended sum
0x00A46477  fld  dword ptr [eax + 0x14]
0x00A4647A  fmul dword ptr [edx + 0x14]       ; * diff.z
0x00A4647F  faddp                              ; extended sum
0x00A46481  fstp dword ptr [esp + 4]          ; the FIRST rounding
0x00A46489  fabs
```

`Find()` (`0x00A462A0`) and `GetPoint()` (`0x00A46300`) go through
`IntrLine3Box3::DoClipping`, whose box-local direction components
`kDiff.Dot(rkBox.Axis[i])` are x87 sums in the retail and binary32 sums in the
rebuild. With the sampled inputs the trigger is the same as in §4.2: the
failing draws carry `inf` in a box axis (`7F800000`), a huge axis
(`9.578e+31`), a `-9.80505e+36` segment direction and a denormal segment
origin (`-2.8026e-45`); the binary32 dot products leave the binary32 range
where the x87 keeps them, the sums become `inf` or NaN, and the clip
parameters - and therefore the returned boolean and the interpolated point -
differ. The rebuild's `DoClipping` is `mulss`/`addss`/`divss` throughout
(`Wm3IntrLine3Box3.cpp.obj`), the retail's is `fmul`/`fadd`/`fdiv` with the
values kept in the 80-bit register between the operations.

### 4.4 `Distance<float,Vector3<float>>::GetClosestPoint1()` - `0x00A39230`

`0x00A39230` is only the getter (`lea eax, [ecx + 0x1c]; ret`); the row drives
`DistVector3Segment3::GetSquared` (`0x00A484F0`) and then reads the member.
The retail's parameter:

```
0x00A48515  fld  dword ptr [eax + 0x10]      ; direction.z
0x00A48518  fmul dword ptr [esp + 0x8]       ; * diff.y    (extended)
0x00A4851C  fld  dword ptr [eax + 0xc]       ; direction.y
0x00A4851F  fmul dword ptr [esp + 0x4]       ; * diff.x
0x00A48523  faddp
0x00A48525  fld  dword ptr [eax + 0x14]      ; direction.x
0x00A48528  fmul dword ptr [esp + 0xc]       ; * diff.z
0x00A4852C  faddp
0x00A4852E  fstp dword ptr [esp]             ; fParam - the FIRST rounding
```

With an extreme input the parameter comes out `+inf`/`-inf`/`NaN` in binary32
and finite in x87, so the clamp

```cpp
if (-m_rkSegment.Extent < fParam) { if (fParam < m_rkSegment.Extent) {...} else {...} }
else { m_kClosestPoint1 = Origin - Extent*Direction; }
```

takes a different branch and the two sides land on different segment
endpoints. The harness output shows exactly that - the two results are often
the same point with a flipped sign, e.g. `7886C73C(2.1869e+34)` (retail) vs
`F886C73C(-2.1869e+34)` (ours).

### 4.5 `IntrBox3Sphere3<float>::Test()` - `0x00A41560`

Retail (`0x00A41588-0x00A415AD`) computes the three `|dot|` terms in x87 and
rounds once per term; the rebuild's compiled body is `mulss`/`mulps`/`addss`
with a binary32 rounding per operation.

Worked example (harness output, iter 1012):

```
box    = centre (7.02925e23, 142.604, -1.17549e-38)
         axis0 (600.632, -907.544, 289.288)
         axis1 (9.78681e30, 497.284, -3.40282e38)
         axis2 (6.80117, 610.279, 512.554)
         extent (498.454, -426.051, 1.17549e-38)
sphere = centre (5.59448, -984.713, -734.214), radius 7.99944e+36
```

`kCDiff = sphere.Center - box.Center` = `(-7.029247842013426e23, -1127.31689453125, -734.2142944335938)`.

| quantity | retail (x87) | rebuild (binary32) |
|---|---|---|
| `fAx = \|kCDiff·axis0\|` | 4.2219937537168145e26 | 4.2219937537168145e26 |
| `fAy = \|kCDiff·axis1\|` | **+inf** (the exact sum -6.88e54, stored as -inf) | **NaN** (the products overflow to -inf and +inf) |
| `fAz = \|kCDiff·axis2\|` | 4.780708634458394e24 | 4.780708634458394e24 |
| `fDy = fAy - Extent[1]` | +inf | **NaN** |
| `fDy*fDy + fDz*fDz` | +inf | **NaN** |
| `r*r` (`7.99944e36²` = 6.4e73) | +inf (stored from the x87) | +inf (overflows) |
| `lhs <= r*r` | inf <= inf → **true** | NaN <= inf → **false** |
| result | `true` (1) | `false` (0) |

exactly the harness's `retail 00000001` vs `ours 00000000`.

The mechanism is the same as in §4.2: one binary32 product leaves the
binary32 range where the x87 keeps it, the resulting `inf - inf` makes a NaN,
and the NaN - not the value - is what flips the comparison.

---

## 5. Counts and engine impact

### 5.1 Counts

| verdict | rows | which |
|---|---:|---|
| **C-denormal** (x87 range family: the retail's 80-bit intermediates do not overflow where the rebuild's binary32 ones do) | **10** | `DistVector3Box3` ×4, `IntrLine3Box3::Test`, `IntrSegment3Box3` ×3, `GetClosestPoint1`, `IntrBox3Sphere3::Test` |
| **S** (the retail's body really does something different) | **4** | `Query2::ToTriangle` ×2, `Query3::ToTetrahedron` ×2 - the retail combines the signs with OR, the rebuild with AND |
| **H** (the row's wiring/input is wrong) | **2** | `ContAlignedBox` ×2 - the harness feeds mask bytes that are not canonical `bool`s; the retail tests `!= 0`, the rebuild's `bool` test is `== 1` |
| **C-order / ordinary family** | **1** | `GenerateOrthonormalBasis` - a 1-2 ulp x87-vs-binary32 difference in ordinary magnitudes; it is in this list only because the `bool` argument word trips the extreme filter |
| **C-nan** | **0** | no row's difference is decided by NaN quieting or payload propagation; the NaN-payload-only tallies (`+36` for `GetClosestPoint1`, `+11` for `IntrSegment3Box3::GetPoint`) are a by-product of the overflow differences, not a separate class |

### 5.2 Can any of this reach the engine?

* **The four `Query2`/`Query3` rows and `GenerateOrthonormalBasis`: no.**
  `src/` contains no call to `Wm3::Query2`, `Wm3::Query3` or
  `Vector3::GenerateOrthonormalBasis`. `Query2`/`Query3` are reachable only
  through `ConvexHull`, which `tools/wmdiff/reachable.md` records as *linked
  but unreachable*; `GenerateOrthonormalBasis` is called only from
  `ContMinBox`, whose only caller is its own recursion. So the one **S**
  verdict in this document sits in code the engine never runs.
* **`ContAlignedBox`: no.** The engine uses the unmasked overload
  (`src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`), which passes
  200000/200000 in the full run. The mask path is dead for the engine.
* **The ten x87-range rows: not with game data.** The engine *does* call these
  bodies - `DistVector3Box3` (`src/moho/collision/CColPrimitiveBase.cpp:429`),
  `IntrSegment3Box3` (`CColPrimitiveBase.cpp:384`), `IntrBox3Sphere3`
  (`CColPrimitiveBase.cpp:607`), `IntrLine3Box3`
  (`src/moho/ui/UiRuntimeTypes.cpp:21007`), `GetClosestPoint1`
  (`src/moho/projectile/Projectile.cpp:1566`). But the trigger needs an
  intermediate that leaves the binary32 range, i.e. a product of two engine
  quantities above ~1e19 (or a difference that lands on a binary32 denormal).
  The engine's inputs are game geometry: positions, radii and unit-length
  box/segment axes, all of order 1..1e4 metres. The box axes are unit vectors
  by construction (`Wm3ContBox3.cpp`, `Axis[3]` "must be an orthonormal set of
  vectors"), the sphere radii are game radii, the segment directions are
  normalised. A product of two such quantities cannot reach 3.4e38. The
  denormal end would need a coordinate difference of ~1e-38 m, which no
  quantised game coordinate produces.
  **What I can support:** the difference is unreachable for the engine's data
  ranges. **What I cannot support:** that no game object can ever hold a
  denormal or a huge coordinate - the harness's own generator is the only
  evidence here, and it deliberately searches the whole binary32 range. A
  mod/plugin-supplied asset with a 1e30 coordinate would reach it; the WM3
  library has no range check of its own.
* **The `Query2`/`Query3`/`GenerateOrthonormalBasis`/`ContAlignedBox` rows
  should not have been hidden from `--skip-extreme` at all.** The filter
  classifies *integer* and *boolean* input words as "extreme", so the nine rows
  that carry index/flag words report `200000/200000` without executing a single
  input. That is a hole in the instrument, not in the rebuild, and it is worth
  fixing before the next full sweep (the filter should only look at the words
  that are floats).

---

## 6. Method notes

* Retail bodies were read with `.tmp/disasm.py` (capstone + pefile) and with
  Ghidra's `tools/wmdiff/ghidra/dump_func.bat` (read-only) on the mapped
  retail image; the rebuild's bodies were read from the *built* objects in
  `build/clang-x86-release/tools/wmdiff/CMakeFiles/wmdiff_wm3.dir/`, i.e. from
  the code the engine actually runs.
* The x87 flag conventions used when reading the retail's comparisons
  (`FNSTSW AX` + `TEST AH,0x41` / `TEST AH,0x5` + `JP`) were **measured**, not
  assumed: a small program in `.tmp/ext/t/retail_sign.cpp` executes the
  retail's exact instruction sequence and shows that
  `det > 0 -> +1`, `det < 0 -> -1`, `det == 0 -> 0`, `NaN -> 0`, i.e. the
  retail's `ToLine` agrees with the source.
* The retail's `Query2::ToTriangle` / `ToLine` behaviour was **measured**, not
  inferred from the disassembly, with the probe binary in `.tmp/probe/`
  (a copy of the harness with one extra option). This is what turned the
  `ToTriangle` verdict from "the difference is in `ToLine`" (wrong: the signs
  are identical) into "the difference is in the final combination" (an OR in
  the retail against an AND in the rebuild).

---

## 7. The second batch: the four remaining rows are closed

The first batch landed the x87 chains of `Dot`, `LengthSq` and the segment
distance (submodule commit `59f1614`) and took the live suite from 34/46 to
42/46.  The four rows that were left are closed here (submodule commit
`9af34dc`); the live suite is now **46/46**, at 1000000 iterations on the
seeds `0x5EED2007` and `0xDEADBEEF`.

| row | retail VA | verdict | what decided it |
|---|---|---|---|
| `IntrLine3Box3<float>::Test()` | `0x00A4FE60` | range (extended domain) | the three slab right-hand sides and the cross product are x87 chains |
| `IntrSegment3Box3<float>::Find()` | `0x00A462A0` | range (extended domain) | `Clip` multiplies and divides on the x87 |
| `IntrSegment3Box3<float>::GetPoint(int) const` | `0x00A46300` | range + a second rounding | `Clip` and the computation of the point |
| `ContAlignedBox<float>(int, const Vector3<float>*, const bool*)` | `0x00A3AE40` | **call shape** (a bool byte) | the retail tests the mask byte against zero |

### 7.1 `IntrLine3Box3<float>::Test` - the slab right-hand sides and the cross product

Section 4.2 named the right quantity (`Extent[0]*afAWdU[1]` leaves the
binary32 range) but not the place where the retail keeps it.  The retail's
`Test` (0x00A4FE60) computes the three right-hand sides as x87 chains:

```
0x00A4FF5C  fmulp st(2)        ; Extent[1]*|d.Axis[2]|      (extended)
0x00A4FF61  fmul  dword [..]   ; Extent[2]*|d.Axis[1]|      (extended)
0x00A4FF65  faddp st(2)        ; the sum                    (extended)
0x00A4FF69  fstp dword [esp+4] ; the sum -> binary32
0x00A4FF71  fcomp st(2)        ; |WxD.Axis[0]| > the sum
```

(the second and the third test are the same shape at
0x00A4FFDF/0x00A4FFE4/0x00A4FFEC/0x00A4FFF4 and
0x00A5002D/0x00A50032/0x00A50018/0x00A50042).
Only the sum is rounded to binary32; the two products keep the extended
exponent, so a product that leaves the binary32 range does not become an
infinity and the sum does not become a NaN.

The cross product of the direction with the difference is an x87 chain as
well (0x00A4FE88..0x00A4FECA): when both products of a component leave the
binary32 range, their difference is still finite on the x87, while in
binary32 the two infinities cancel into a NaN.

Both are now computed in the extended domain (`SlabRhs`, `CrossInlined` in
`Foundation/Intersection/Wm3IntrLine3Box3.cpp`).

### 7.2 `IntrSegment3Box3<float>::Find` and `GetPoint` - `Clip`, not the box-local direction

Section 4.3 attributed the two segment rows to "the box-local direction
components overflow binary32".  That is not where the difference is: the
box-local components are stored to binary32 by the retail as well (FSTP at
0x00A50277, 0x00A5028E, 0x00A502A5 and 0x00A502BE, 0x00A502D7,
0x00A502F0), so both sides see the same infinity.  The difference is in
`Clip` (0x00A4FCC0), which the segment rows reach through `DoClipping`
(0x00A50220):

```
0x00A4FCD9  fld  dword [eax]   ; rfT1
0x00A4FCDB  fmul st(2)         ; fDenom*rfT1     (extended, never rounded)
0x00A4FCDD  fcomp st(1)        ; the product against the numerator
...
0x00A4FCFE  fdivrp st(1)       ; fNumer/fDenom   (extended)
0x00A4FD02  fstp dword [ecx]   ; the parameter -> binary32
```

The product is compared with the numerator while it still has the extended
exponent, and the quotient is computed on the x87 and rounded to binary32
once.  In binary32 the product can overflow to an infinity and the
comparison then goes the wrong way.  `Clip` now does the same
(`ClipProduct`, `ClipQuotient`; the quotient needed a new operation in the
carrier, `Wm3Detail::Div24`).

The intersection point is computed on the x87 as well: the product of the
clip parameter and the direction is rounded to a 24-bit significand and
stored back to binary32 (FLD/FMUL at 0x00A504FC..0x00A50500) *before* the
origin is added (FADD dword at 0x00A50518).  The second rounding of the
product is visible when the product lands in the binary32 denormal range,
where the binary32 format has fewer bits to work with - that was the last
two mismatches of the `GetPoint` row at 1000000 iterations.

### 7.3 `ContAlignedBox<float>` - the mask byte is tested against zero

Section 3.2 was right about the mechanism (the retail tests the mask byte
against zero, the rebuild's `bool` test compiles to a comparison against
one) and the row is now closed by spelling the test on the raw byte
(`Foundation/Containment/Wm3ContBox3.cpp`):

```
0x00A3B30E  cmp byte ptr [ecx + esi], 0   ; the first valid point
0x00A3B395  cmp byte ptr [esi + ecx], 0   ; the rest
```

**This is a call-shape difference, not an engine behaviour.**  For the
canonical mask bytes (zero and one) the byte test and the `bool` test are
the same, and the engine only ever calls the unmasked overload
(`src/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26`, `abValid` defaults to
`0`), so the change is semantically neutral for the engine; it only
reproduces the retail for a mask byte that is not a canonical `bool`, which
the engine cannot produce.  The same holds for the `Vector2` overload
(0x00A685F0), which is a dead row and was left alone.
