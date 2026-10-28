# Float comparison: the ordinary-input failures

Scope: the `wmdiff` rows that fail **with** `--skip-extreme`, i.e. the rows whose
difference shows up on ordinary (non-denormal, non-infinite) inputs. These are
the 17 float rows that survive the harness's extreme filter:

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme --max-mismatch 0
```

```
[FAIL] Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&, const Wm3::Vector3<float>&) 0x00452AD0  27812
[FAIL] Wm3::Vector3<float>::Dot(const Wm3::Vector3<float>&) const                          0x00452AD0  27837
[FAIL] Wm3::Vector3<float>::LengthSq(const Wm3::Vector3<float>&)                           0x004757E0     514
[FAIL] Wm3::Vector3<float>::SquaredLength() const                                         0x004757E0     518
[FAIL] Wm3::Math<float>::InvSqrt(float)                                                   0x0050D150   20585
[FAIL] Wm3::DistVector3Segment3<float>::GetSquared()                                      0x00A484F0    3379
[FAIL] Wm3::DistVector3Segment3<float>::Get()                                              0x00A480E0    4797
[FAIL] Wm3::DistVector3Segment3<float>::Get(float, const Vector3<float>&, const Vector3<float>&)        0x00A48190  3323
[FAIL] Wm3::DistVector3Segment3<float>::GetSquared(float, const Vector3<float>&, const Vector3<float>&) 0x00A48340  3401
[FAIL] Wm3::DistVector2Box2<float>::Get(float, const Vector2<float>&, const Vector2<float>&)            0x00A6C730  3349
[FAIL] Wm3::DistVector2Box2<float>::GetSquared(float, const Vector2<float>&, const Vector2<float>&)     0x00A6C880  3318
[FAIL] Wm3::DistSegment3Segment3<float>::Get(float, const Vector3<float>&, const Vector3<float>&)       0x00A7E2A0  3403
[FAIL] Wm3::DistSegment3Segment3<float>::GetSquared(float, const Vector3<float>&, const Vector3<float>&) 0x00A7E480  3354
[FAIL] Wm3::DistLine3Segment3<float>::GetSquared()                                         0x00A80DE0    9639
[FAIL] Wm3::DistLine3Segment3<float>::Get()                                                0x00A809A0    8199
[FAIL] Wm3::DistLine3Segment3<float>::GetSquared(float, const Vector3<float>&, const Vector3<float>&)   0x00A80C10  9512
[FAIL] Wm3::DistLine3Segment3<float>::Get(float, const Vector3<float>&, const Vector3<float>&)          0x00A80A40  8557
```

**How the counts reconcile.** The full run (no `--skip-extreme`) fails **34** float
rows; `--skip-extreme` fails **17**. The other 17 are the extreme-input rows and
are already attributed in `float-analysis-extreme.md` (whose §1 carries the full
34-row list). The 17 rows here are the *distinct* ordinary-input failures, and
they are the entire remainder of the float failure set: `92/109` targets pass
with `--skip-extreme`, and every one of the 17 failures below is a
one-or-few-ulp difference.

The `double` instantiations of the same classes also differ, but they are out of
scope for the same reason as in the extreme document: the engine never executes a
`double` instantiation (`tools/wmdiff/double-audit.md`).

Nothing in the rebuild, in the harness or in the compiler options was changed.
The only artefacts built for this analysis are read-only disassembly dumps and
small Python models under `.tmp/floatord/`; they are a measuring instrument, not
a fix. Every claim below is backed by a disassembly excerpt plus a numeric
reproduction that matches the retail bits *exactly*.

---

## 1. Summary table

| # | target | retail addr | verdict | mechanism | evidence |
|---|---|---|---|---|---|
| 1 | `Vector3<float>::Dot(const Vector3<float>&, const Vector3<float>&)` | `0x00452AD0` | **C-order** | the retail's x87 body sums the dot product **right-to-left**, `((z·z' + y·y') + x·x')`; the source tree's `a.x*b.x + a.y*b.y + a.z*b.z` associates **left-to-right** | §2.1 |
| 2 | `Vector3<float>::Dot(const Vector3<float>&) const` | `0x00452AD0` | **C-order** | same retail body as row 1 (the two rows differ only in which WM3 overload the rebuild calls) | §2.1 |
| 3 | `Vector3<float>::LengthSq(const Vector3<float>&)` | `0x004757E0` | **C-precision** | the retail keeps `x², y², z²` and their sum in the **x87 register file** (24-bit significand, 15-bit exponent) and rounds to binary32 only at the end; the rebuild's `/arch:SSE2` codegen rounds **every** intermediate to binary32, so a sum that lands in binary32's subnormal band loses bits | §2.2 |
| 4 | `Vector3<float>::SquaredLength() const` | `0x004757E0` | **C-precision** | same retail body as row 3 | §2.2 |
| 5 | `Math<float>::InvSqrt(float)` | `0x0050D150` | **C-root** | the retail computes `1/√x` **in x87**: `FSQRT` under `_PC_24` (24-bit root) then `FDIVRP` (24-bit quotient); the rebuild follows `Wm3Math.inl:95` `(Real)(1.0/sqrt((double)fValue))`, i.e. a **binary64** root and a binary64 division | §2.3 |
| 6 | `DistVector3Segment3<float>::GetSquared()` | `0x00A484F0` | **C-precision** | the retail's `kDiff` and squared-length arithmetic stays in the x87 register file; the rebuild's SSE2 path rounds each product to binary32, and this row's generator makes the squared distance **subnormal** | §2.4 |
| 7 | `DistVector3Segment3<float>::Get()` | `0x00A480E0` | **C-precision + C-root** | calls the retail's `GetSquared()` (row 6) through the vtable, then the retail's CRT square root at `0x00A91000` | §2.4 |
| 8 | `DistVector3Segment3<float>::GetSquared(fT, v0, v1)` | `0x00A48340` | **C-precision** | the swept body computes the moved geometry (`origin + fT·velocity`) and the core in the x87 register file | §2.4 |
| 9 | `DistVector3Segment3<float>::Get(fT, v0, v1)` | `0x00A48190` | **C-precision** | inherits row 8's movement and core arithmetic | §2.4, §4 |
| 10 | `DistVector2Box2<float>::GetSquared(fT, v0, v1)` | `0x00A6C880` | **C-precision** | the retail's movement prologue keeps `fT·velocity` in the x87 register (24-bit significand) and rounds it to binary32 **afterwards**; the rebuild's `mulss` rounds the exact product straight to binary32 - a double rounding that differs once the product is subnormal | §2.5 |
| 11 | `DistVector2Box2<float>::Get(fT, v0, v1)` | `0x00A6C730` | **C-precision** | inherits row 10's movement prologue and the (passing) unswept core | §2.5, §4 |
| 12 | `DistSegment3Segment3<float>::GetSquared(fT, v0, v1)` | `0x00A7E480` | **C-precision** | the swept body builds the two moved segments in the x87 register file; the unswept core of the same class **passes** bit-identically, so the difference is introduced by the movement prologue | §2.6, §4 |
| 13 | `DistSegment3Segment3<float>::Get(fT, v0, v1)` | `0x00A7E2A0` | **C-precision** | inherits row 12 | §2.6, §4 |
| 14 | `DistLine3Segment3<float>::GetSquared()` | `0x00A80DE0` | **C-precision** (bit-level model not completed) | the retail implements the **same** expression tree as `Wm3DistLine3Segment3.cpp:46` (it even compares against the same `ZERO_TOLERANCE` constant `0x00D71BFC`), but evaluates it with the x87 register file and its own association of the long `fSqrDist` sum | §2.7, §4 |
| 15 | `DistLine3Segment3<float>::Get()` | `0x00A809A0` | **C-precision** (inherits row 14) | calls the retail's `GetSquared()` through the vtable, then the CRT square root at `0x00A91000` | §2.7, §4 |
| 16 | `DistLine3Segment3<float>::GetSquared(fT, v0, v1)` | `0x00A80C10` | **C-precision** (inherits row 14) | the swept body moves the line and the segment in the x87 register file, then calls row 14's core | §2.7, §4 |
| 17 | `DistLine3Segment3<float>::Get(fT, v0, v1)` | `0x00A80A40` | **C-precision** (inherits rows 14 and 16) | as row 16, plus the CRT square root | §2.7, §4 |

Counts: **S 0**, **C 17**, **H 0** - of which 9 rows have a completed
bit-level reproduction and 8 are attributed by inheritance (§5 lists them
explicitly).

---

## 2. The two mechanisms, and where they bite

### 2.0 The shared cause: the x87 register file vs binary32 SSE

The rebuild's Wild Magic is compiled for x86 with `/arch:SSE2`
(`cmake/FafCodegen.cmake:64-67`), so every `float` operation is a binary32 SSE
operation: each product and each sum is **rounded to binary32**, and an
intermediate that leaves binary32's range becomes ±inf or a subnormal with
fewer significand bits.

The retail's Wild Magic bodies are x87: the products and the sums stay in the
80-bit register file and are rounded to binary32 only where the body stores them
(`FSTP dword ptr ...`). Under the harness's control word (`0x007F` = `_PC_24`,
set by `SetFpEnv()` at `tools/wmdiff/src/main.cpp:154,163-170` before *each*
call) the x87 significand is 24 bits - the same as binary32 - but the
**exponent range is 15 bits, not 8**. So:

* an intermediate that is a perfectly ordinary binary32 **subnormal** in the
  rebuild (below `2^-126`) is a **normal** 24-bit number in the retail, and the
  rebuild's rounding to the subnormal grid loses bits the retail keeps;
* where the retail rounds a 24-bit x87 intermediate to binary32 and the rebuild
  rounds the exact product to binary32, the two **double-round** and can differ
  by one ulp in the subnormal band.

This is a codegen property, not a source difference: the *same* source
expression is being evaluated in two different numeric domains.

**It is the only thing that separates the rows from passing.** With the
harness's own control word the gap is one or a few ulp; give the retail its
default 53-bit control word instead and the same corpus breaks much wider:

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme --max-mismatch 0 --cw 0x027F
```

⇒ **75 rows fail** (vs 17 with the default `--cw 0x007F`), including the
`Distance<>`, `Intr*`, `Eigen` and `Normalize` families that pass at `_PC_24`.
The `Dot` row alone goes from 27 812 to 39 641 mismatches.

### 2.1 `Vector3<float>::Dot` - evaluation order (`0x00452AD0`)

Retail body (`0x00452AD0`, `b` = the stack argument, `a` = `this`):

```
0x00452ad1  mov  eax, dword ptr [esp + 4]    ; b
0x00452ad4  fld  dword ptr [eax + 8]          ; b.z
0x00452ad7  fmul dword ptr [ecx + 8]         ; * a.z
0x00452ada  fld  dword ptr [eax + 4]          ; b.y
0x00452add  fmul dword ptr [ecx + 4]         ; * a.y
0x00452ae0  faddp st(1)                      ; (b.z*a.z) + (b.y*a.y)
0x00452ae2  fld  dword ptr [eax]              ; b.x
0x00452ae5  fmul dword ptr [ecx]             ; * a.x
0x00452ae8  faddp st(1)                      ; + (b.x*a.x)
```

The rebuild's source (`dependencies/WildMagic3p8/Foundation/Math/Wm3Vector3.h:145-148`):

```cpp
static constexpr Real Dot (const Vector3& a, const Vector3& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;      // ((a.x*b.x + a.y*b.y) + a.z*b.z)
}
```

Same three products, same tree of operations, **different association**: the
retail sums `((z·z' + y·y') + x·x')`, the source tree sums
`((x·x' + y·y') + z·z')`. Addition is commutative but not associative, so the two
round differently. Nothing else differs: the products themselves are exact
float multiplies and the intermediates stay in range.

Worked example (the one from the task, reproduced exactly by a model of the
retail's association):

```
a = (3EF04ABF, BF35D5B2, 3F064D59)      b = (BF2B475D, BF3A3379, BE1C658D)
source tree (left-to-right, binary32)  3DFAE21F
retail      (right-to-left, x87)       3DFAE220
```

Model check (400 sampled mismatches, `--filter "Vector3<float>::Dot"`):
* x87 model with the retail's association reproduces the **retail bits in
  400/400** cases;
* binary32 model with the source association reproduces the **rebuild bits in
  400/400** cases.

⇒ **C, evaluation order.** The retail is not computing a different function;
MSVC's x87 codegen simply emitted the sum in the opposite order. The
`Vector3<float>::Dot` *instance* row (row 2) shares the retail body and behaves
identically.

### 2.2 `Vector3<float>::LengthSq` / `SquaredLength` - the subnormal band (`0x004757E0`)

Retail body (`0x004757E0`), the whole sum in the register file:

```
0x004757e0  fld  dword ptr [ecx + 8]      ; z
0x004757e3  fld  dword ptr [ecx + 4]      ; y
0x004757e6  fld  dword ptr [ecx]          ; x
0x004757e8  fmul st(0), st(0)             ; x*x     <- stays in the x87 register
0x004757ea  fld  st(1)
0x004757ec  fmulp st(2)
0x004757ee  faddp st(1)
0x004757f0  fld  st(1)
0x004757f2  fmulp st(2)
0x004757f4  faddp st(1)
0x004757f6  ret                            ; the result is still in the register
```

The rebuild's source (`Wm3Vector3.h:155-158`):

```cpp
static constexpr Real LengthSq (const Vector3& v) noexcept
{
    return v.x * v.x + v.y * v.y + v.z * v.z;      // each product and each sum -> binary32
}
```

The association is the same in both (modulo commutativity of the first add), so
this row is **not** an order difference - it is the exponent range. The retail
computes `x²`, `y²`, `z²` and the running sum in the x87 register file, where a
value such as `1e-40` is a normal 24-bit number; the rebuild's SSE2 path rounds
each of them to binary32, where `1e-40` is subnormal and carries only a handful
of significand bits.

Model check (290 sampled mismatches):
* replaying the **retail's own bytes** on an x87 model (24-bit significand, wide
  exponent; binary32 only where the body stores) reproduces the **retail bits in
  290/290** cases;
* evaluating the WM3.8 source tree with binary32 rounding after every operation
  reproduces the **rebuild bits in 290/290** cases.

⇒ **C, precision / exponent range.** 514 (resp. 518) mismatches in 200 000
draws - rare, because it needs a vector whose squared length falls into the
subnormal band.

### 2.3 `Math<float>::InvSqrt` - the root path (`0x0050D150`)

Retail body (`0x0050D150`):

```
0x0050d156  fld  dword ptr [ebp + 8]      ; x, a float
0x0050d159  fsqrt                          ; root, 24-bit significand under _PC_24
0x0050d15b  fld1
0x0050d15d  fdivrp st(1)                   ; 1/root, 24-bit, still in the register
```

The rebuild's source (`dependencies/WildMagic3p8/Foundation/Math/Wm3Math.inl:93-96`):

```cpp
template <class Real>
Real Math<Real>::InvSqrt (Real fValue)
{
    return (Real)(1.0/sqrt((double)fValue));       // binary64 root, binary64 divide
}
```

Same operation sequence (square root, then reciprocal), different numeric
domain: the retail's root is a **24-bit** x87 `FSQRT` and its reciprocal a
24-bit x87 division; the rebuild's root is the correctly-rounded **binary64**
square root of the widened float, and the division is binary64 as well. The
result is then rounded to binary32 once, in both cases.

Model check (400 sampled mismatches, `--filter "InvSqrt"`):

| model | matches retail | matches rebuild |
|---|---|---|
| 24-bit root, 24-bit divide (x87) | **400/400** | 0/400 |
| 24-bit root, 53-bit divide | 400/400 | 0/400 |
| 53-bit root, 24-bit divide | 0/400 | 0/400 |
| 53-bit root, 53-bit divide | 0/400 | **400/400** |

The discriminating knob is the **root width** (24 vs 53 bits); the quotient
width is irrelevant once the result is rounded to binary32.

⇒ **C, root path / precision.** 20 585 mismatches in 200 000 draws - the
highest rate of the precision family, because `1/√x` is a *reciprocal*: the
relative error of the root is amplified by the reciprocal, so a 1-ulp difference
in the root survives into the final binary32 result far more often than for a
plain product.

### 2.4 `DistVector3Segment3<float>` - the core, in the register file (`0x00A484F0`)

Retail body (`0x00A484F0`), the `kDiff` and the dot product:

```
0x00A484f9  fld  dword ptr [edx]           ; kDiff.x = vector.x - segment.x
0x00A484fb  fsub dword ptr [eax]
0x00A484fd  fstp dword ptr [esp + 4]        ; <- the only binary32 rounding of kDiff
...
0x00A48515  fld  dword ptr [eax + 0x10]    ; segment.direction.y
0x00A48518  fmul dword ptr [esp + 8]       ; * kDiff.y    <- stays in the register
0x00A4851c  fld  dword ptr [eax + 0xc]
0x00A4851f  fmul dword ptr [esp + 4]
0x00A48523  faddp st(1)
0x00A48525  fld  dword ptr [eax + 0x14]
0x00A48528  fmul dword ptr [esp + 0xc]
0x00A4852c  faddp st(1)
0x00A4852e  fstp dword ptr [esp]            ; the dot product, rounded to binary32 here
```

and the squared length at the tail (`0x00A4867c-0x00A48698`) is the same
register-resident `fld`/`fmul`/`faddp` chain, rounded to binary32 only at the
final `fstp`.

So the retail computes `kDiff` in binary32 (it stores it) but computes the
**products and the sum of squares in the x87 register file**, where a product
like `(7.8e-20)² = 6.0e-39` is a normal 24-bit number. The rebuild's SSE2
path rounds that product to binary32, where it is subnormal.

Worked example (`--filter "DistVector3Segment3<float>::GetSquared()"`, one draw):

```
vec     = (0, 7.7734983e-20, 0)
segment = origin (0,0,0), direction (1,0,0), extent 10
kDiff   = (0, 7.7734983e-20, 0)
retail  0041CCAA (6.0427276e-39)      ours 0041CCAB (6.0427276e-39)
```

Model check (400 sampled mismatches):
* the **x87-register** model of the source tree (binary32 at the `fstp` points
  only) reproduces the **retail bits in 400/400** cases;
* the **binary32** model reproduces the **rebuild bits in 400/400** cases.

⇒ **C, precision / exponent range**, inherited by the `Get()` row and by the two
swept rows (the swept body repeats the movement arithmetic in the register
file before calling the core).

### 2.5 `DistVector2Box2<float>` swept rows - double rounding in the movement prologue (`0x00A6C880`)

The unswept `DistVector2Box2<float>::GetSquared()` (`0x00A6C9D0`) and
`Get()` (`0x00A6C680`) **pass 200 000/200 000 bit-identically**. The *swept*
rows fail. The only new arithmetic in the swept rows is the movement prologue:

```
0x00A6C8b7  fld  dword ptr [esp + 0x78]     ; fT
0x00A6C8bb  fld  dword ptr [esp + 0x7c]     ; velocity.x
0x00A6C8bf  fmul st(1)                     ; fT * velocity.x   (24-bit, wide exponent)
0x00A6C8c1  fstp dword ptr [esp + 0xc]     ; rounded to binary32 only here
```

The rebuild's source (`Wm3DistVector2Box2.cpp:90-96`):

```cpp
Real DistVector2Box2<Real>::GetSquared (Real fT,
    const Vector2<Real>& rkVelocity0, const Vector2<Real>& rkVelocity1)
{
    Vector2<Real> kMVector = m_rkVector + fT*rkVelocity0;
    Vector2<Real> kMCenter = m_rkBox.Center + fT*rkVelocity1;
    Box2<Real> kMBox(kMCenter, m_rkBox.Axis, m_rkBox.Extent);
    return DistVector2Box2<Real>(kMVector,kMBox).GetSquared();
}
```

The retail computes `fT·velocity` in the register and rounds it to binary32
**afterwards**; the rebuild's `mulss` rounds the exact product straight to
binary32. That is a double rounding, and it differs as soon as the product is
subnormal - which it is for these draws:

```
fT = 8.3517197e-20, velocity.x = 8.3517197e-20
exact product = 6.9751234e-39   (binary32: subnormal)
retail  (24-bit register, then binary32)  004BF3CC
ours   (mulss, straight to binary32)       004BF3CD
```

Model check (300 sampled mismatches, `--filter "DistVector2Box2<float>::GetSquared(float"`):
the whole swept computation (movement + core) evaluated with the **x87** backend
reproduces the **retail bits in 300/300** cases; with the **binary32** backend
it reproduces the **rebuild bits in 300/300** cases.

⇒ **C, precision.** The failure is introduced by the movement prologue, not by
the distance core - which is why the unswept twins pass.

### 2.6 `DistSegment3Segment3<float>` swept rows - the same prologue (`0x00A7E480`)

Same shape as §2.5: the unswept `DistSegment3Segment3<float>::GetSquared()`
(`0x00A7E660`) and `Get()` (`0x00A7E230`) **pass 200 000/200 000
bit-identically**, while the swept rows `0x00A7E480` / `0x00A7E2A0` fail. The
retail's swept body (`0x00A7E480`) builds the two moved segments with the
register-resident `fld`/`fmul`/`faddp` chain and calls the core through the
vtable; the rebuild does the same movement in binary32. The difference is
therefore the same double-rounding-in-the-subnormal-band mechanism, but I did
**not** complete a bit-level model of this body - see §5.

### 2.7 `DistLine3Segment3<float>` - same tree, different association (`0x00A80DE0`)

The retail body is a long x87 expression. It implements the **same algorithm**
as `Wm3DistLine3Segment3.cpp:46-105` - the giveaway is that it compares its
determinant against the very same `ZERO_TOLERANCE` constant the source uses
(`0x00D71BFC` = `9.99999997e-07` = float `1e-6`):

```
0x00a80e6f  fld  dword ptr [esp]           ; the determinant
0x00a80e72  fld  st(0)
0x00a80e74  fmul st(0)                    ; det*det
0x00a80e76  fld1
0x00a80e78  fld  st(0)
0x00a80e7a  fsubrp st(2)                  ; 1 - det*det
0x00a80e7c  fxch
0x00a80e7e  fstp dword ptr [esp]
0x00a80e81  fld  dword ptr [esp]
0x00a80e84  fabs                           ; |1 - det*det|
0x00a80e86  fstp dword ptr [esp]
0x00a80e89  fld  dword ptr [esp]
0x00a80e8c  fld  float ptr [0x00d71bfc]    ; Math<float>::ZERO_TOLERANCE (1e-6f)
0x00a80e92  fcomp
```

An exact-rational evaluation of the WM3.8 source tree agrees with the retail's
output to within `1e-5` relative on **379/400** sampled mismatches (the
remaining 21 are all in the subnormal band, where a relative comparison is
meaningless), and both sides sit within `~1e-6` relative of that exact value.
So the *algorithm* is the same - **S is ruled out** - and the difference is the
evaluation domain plus the association the x87 codegen chose for the long
`fSqrDist` sum:

* 284/400 sampled mismatches have a **subnormal** retail result (exponent field
  0) - the §2.0 mechanism;
* the remaining 116 are in the normal range and differ by 1..8 ulp - the
  association of the sum, i.e. the §2.1 mechanism, amplified by the length of
  the expression.

The retail body also carries two **double** constants
(`FLD double ptr [0x00D71CD8]` = `2.0`, and `FADD qword ptr [0x00E4F7E0]` =
`0.0`) where the WM3.8 source uses float constants. Both are exactly
representable, so they are numerically inert; they are a fingerprint of the
retail's own source text, not a cause of the mismatch. I did not complete a
bit-level model of this body - see §5.

---

## 3. Why no row is **S** and no row is **H**

* **S (source/algorithm difference)** would mean the retail body performs
  different operations - a different expression, a different constant, an extra
  or a missing step. For every one of the 17 rows the retail body's operation
  sequence is the WM3.8 tree's:
  * `Dot` computes the same three products and the same two adds, only in the
    opposite order (§2.1);
  * `LengthSq`/`SquaredLength` compute the same three squares and the same two
    adds (§2.2);
  * `InvSqrt` computes a square root and a reciprocal (§2.3);
  * the four `Dist*` families compute the same distance expressions, down to
    the shared `ZERO_TOLERANCE` comparison in `DistLine3Segment3` (§2.7).
  The only differences are *when* the results are rounded and *in which order*
  the sums are associated - both codegen properties.
* **H (harness wiring)** would mean a row compares the wrong things. It cannot
  be the explanation here: for 9 of the 17 rows a model of the *source tree*
  evaluated in one numeric domain reproduces the retail bits **exactly**
  (400/400, 300/300, 290/290), which is only possible if the harness feeds
  the right arguments in the right order and extracts the right result. The
  remaining 8 rows are the same code paths as their siblings, which do
  reproduce bit-exactly.

---

## 4. Source and disassembly side by side

### 4.1 `Vector3<float>::Dot` - `0x00452AD0`

```
retail 0x00452AD0                          rebuild: Foundation/Math/Wm3Vector3.h:145-148
  mov  eax,[esp+4]      ; b                 static constexpr Real Dot (const Vector3& a,
  fld  [eax+8]          ; b.z                              const Vector3& b) noexcept
  fmul [ecx+8]          ; * a.z              {
  fld  [eax+4]          ; b.y                  return a.x * b.x + a.y * b.y + a.z * b.z;
  fmul [ecx+4]          ; * a.y              }
  faddp st(1)           ; z·z' + y·y'
  fld  [eax]            ; b.x              rebuild (instance): Foundation/Math/Wm3Vector3.inl:275-281
  fmul [ecx]            ; * a.x              Real Vector3<Real>::Dot (const Vector3& rkV) const
  faddp st(1)           ; + x·x'            {
                                          return
                                              m_afTuple[0]*rkV.m_afTuple[0] +
                                              m_afTuple[1]*rkV.m_afTuple[1] +
                                              m_afTuple[2]*rkV.m_afTuple[2];
                                          }
```

### 4.2 `Vector3<float>::LengthSq` / `SquaredLength` - `0x004757E0`

```
retail 0x004757E0                          rebuild: Foundation/Math/Wm3Vector3.h:155-158
  fld  [ecx+8]          ; z               static constexpr Real LengthSq (const Vector3& v) noexcept
  fld  [ecx+4]          ; y               {
  fld  [ecx]            ; x                   return v.x * v.x + v.y * v.y + v.z * v.z;
  fmul st(0),st(0)      ; x*x            }
  fld  st(1)
  fmulp st(2)                             rebuild (instance): Foundation/Math/Wm3Vector3.inl:268
  faddp st(1)                             Real Vector3<Real>::SquaredLength () const
  fld  st(1)                             {
  fmulp st(2)                                 return
  faddp st(1)                                  m_afTuple[0]*m_afTuple[0] +
  ret                    ; still in ST(0)   m_afTuple[1]*m_afTuple[1] +
                                          m_afTuple[2]*m_afTuple[2];
                                      }
```

### 4.3 `Math<float>::InvSqrt` - `0x0050D150`

```
retail 0x0050D150                          rebuild: Foundation/Math/Wm3Math.inl:93-96
  fld  dword ptr [ebp+8] ; x               template <class Real>
  fsqrt                   ; 24-bit root   Real Math<Real>::InvSqrt (Real fValue)
  fld1                                    {
  fdivrp st(1)            ; 1/root             return (Real)(1.0/sqrt((double)fValue));
  ...                                     }
```

### 4.4 `DistVector3Segment3<float>` - `0x00A484F0` / `0x00A480E0` / `0x00A48340` / `0x00A48190`

```
retail 0x00A484F0 (core)                   rebuild: Foundation/Distance/Wm3DistVector3Segment3.cpp:46-73
  fld  [edx] ; fsub [eax]                 Real DistVector3Segment3<Real>::GetSquared ()
  fstp dword ptr [esp+4]  ; kDiff             {
  fld  [eax+0x10] ; fmul [esp+8]               Vector3<Real> kDiff = m_rkVector - m_rkSegment.Origin;
  fld  [eax+0xc]  ; fmul [esp+4]               Real fParam = m_rkSegment.Direction.Dot(kDiff);
  faddp st(1)                               ...
  fld  [eax+0x14] ; fmul [esp+0xc]           Vector3<Real> kClosestPoint1 = ...;
  faddp st(1)                               kClosestPoint1 -= m_rkVector;
  fstp dword ptr [esp]    ; the dot product   Vector3<Real> kDiff = kClosestPoint1 - m_rkVector;
  ... (squared length in the registers)       return kDiff.SquaredLength();
                                           }
retail 0x00A480E0 (Get)
  mov  edx,[eax+0x18] ; call edx            rebuild: Wm3DistVector3Segment3.cpp:39-44
  call 0x00a91000        ; CRT sqrt          Real DistVector3Segment3<Real>::Get ()
  fstp dword ptr [esp]                      {
  fld  dword ptr [esp]                          Real fSqrDist = GetSquared();
  ret                                         return Math<Real>::Sqrt(fSqrDist);
                                           }
```

### 4.5 `DistVector2Box2<float>` swept - `0x00A6C880` / `0x00A6C730`

```
retail 0x00A6C880 (swept GetSquared)       rebuild: Foundation/Distance/Wm3DistVector2Box2.cpp:90-97
  fld  [esp+0x78] ; fld [esp+0x7c]         Real DistVector2Box2<Real>::GetSquared (Real fT,
  fmul st(1)              ; fT*velocity.x       const Vector2<Real>& rkVelocity0,
  fstp dword ptr [esp+0xc]   ; -> binary32       const Vector2<Real>& rkVelocity1)
  ... (the same for y, then the added      {
  ...  origin + fT*velocity, then the          Vector2<Real> kMVector = m_rkVector + fT*rkVelocity0;
  ...  core through the vtable)                Vector2<Real> kMCenter = m_rkBox.Center + fT*rkVelocity1;
                                               Box2<Real> kMBox(kMCenter, m_rkBox.Axis, m_rkBox.Extent);
                                               return DistVector2Box2<Real>(kMVector,kMBox).GetSquared();
                                           }
```

### 4.6 `DistSegment3Segment3<float>` swept - `0x00A7E480` / `0x00A7E2A0`

```
retail 0x00A7E480 (swept GetSquared)       rebuild: Foundation/Distance/Wm3DistSegment3Segment3.cpp:380-390
  fld  [eax+8] / fld [eax+4] / fld [eax]    Real DistSegment3Segment3<Real>::GetSquared (Real fS1,
  fstp dword ptr [esp+0xc] ... ; the            const Vector3<Real>& rkVelocity0,
  ...  register-resident fld/fmul/faddp        const Vector3<Real>& rkVelocity1)
  ...  chain builds the moved origins          {
  ...  then the core through the vtable           Vector3<Real> kMOrigin0 = m_rkSegment0.Origin + fS1*rkVelocity0;
                                                   Vector3<Real> kMOrigin1 = m_rkSegment1.Origin + fS1*rkVelocity1;
                                                   Segment3<Real> kMSegment0(kMOrigin0, m_rkSegment0.Direction, m_rkSegment0.Extent);
                                                   Segment3<Real> kMSegment1(kMOrigin1, m_rkSegment1.Direction, m_rkSegment1.Extent);
                                                   return DistSegment3Segment3<Real>(kMSegment0,kMSegment1).GetSquared();
                                               }
```

### 4.7 `DistLine3Segment3<float>` - `0x00A80DE0` / `0x00A809A0` / `0x00A80C10` / `0x00A80A40`

```
retail 0x00A80DE0 (core)                   rebuild: Foundation/Distance/Wm3DistLine3Segment3.cpp:46-105
  fld  [eax] ; fsub [edx] ; fstp [esp+0xc]  Real DistLine3Segment3<Real>::GetSquared ()
  ...  the three direction/diff dot products    {
  ...  fld float ptr [0x00d71bfc] ; fcomp      Vector3<Real> kDiff = m_rkLine.Origin - m_rkSegment.Origin;
  ...  (the ZERO_TOLERANCE test)              Real fA01 = -m_rkLine.Direction.Dot(m_rkSegment.Direction);
  ...  the long fSqrDist sum in the registers Real fB0 = kDiff.Dot(m_rkLine.Direction);
  ...  fld qword ptr [0x00d71cd8]  ; double 2.0  Real fC = kDiff.SquaredLength();
  ...  fmul st(6),st(0)                       Real fDet = Math<Real>::FAbs((Real)1.0 - fA01*fA01);
  ...                                          if (fDet >= Math<Real>::ZERO_TOLERANCE) { ... }
                                               }
retail 0x00A809A0 (Get)
  mov  edx,[eax+0x18] ; call edx            rebuild: Wm3DistLine3Segment3.cpp:39-44
  call 0x00a91000        ; CRT sqrt          Real DistLine3Segment3<Real>::Get ()
  fstp dword ptr [esp]                      {
  fld  dword ptr [esp]                          Real fSqrDist = GetSquared();
  ret                                         return Math<Real>::Sqrt(fSqrDist);
                                           }
```

---

## 5. Rows not analysed to the bit level

Eight rows are attributed by inheritance from a sibling whose mechanism was
reproduced bit-exactly; their own bodies were not modelled instruction by
instruction. No new work was done for them beyond the disassembly excerpts
quoted above.

| # | row | retail addr | verdict | why not analysed |
|---|---|---|---|---|
| 9 | `DistVector3Segment3<float>::Get(fT, v0, v1)` | `0x00A48190` | C-precision (inherited) | the swept `GetSquared` twin (row 8) was modelled bit-exactly; the `Get` twin only adds the CRT square root on top of it |
| 12 | `DistSegment3Segment3<float>::GetSquared(fT, v0, v1)` | `0x00A7E480` | C-precision (inherited) | the swept body itself was not modelled: its **unswept** twin `0x00A7E660` passes 200 000/200 000 bit-identically, which localises the difference to the movement prologue (§2.6), but the prologue was not reproduced bit-for-bit |
| 11 | `DistVector2Box2<float>::Get(fT, v0, v1)` | `0x00A6C730` | C-precision (inherited) | the swept `GetSquared` twin (row 10) was modelled bit-exactly; the `Get` twin adds the CRT square root |
| 13 | `DistSegment3Segment3<float>::Get(fT, v0, v1)` | `0x00A7E2A0` | C-precision (inherited) | as row 12 |
| 14 | `DistLine3Segment3<float>::GetSquared()` | `0x00A80DE0` | C-precision (algorithm verified, bit-level model incomplete) | the algorithm is verified identical to the WM3.8 source tree (exact-rational agreement 379/400, same `ZERO_TOLERANCE` constant), but the retail's own association of the long `fSqrDist` sum was not pinned down, so the row is not reproduced bit-for-bit |
| 15 | `DistLine3Segment3<float>::Get()` | `0x00A809A0` | C-precision (inherited) | as row 14, plus the CRT square root |
| 16 | `DistLine3Segment3<float>::GetSquared(fT, v0, v1)` | `0x00A80C10` | C-precision (inherited) | as row 14, with the movement prologue |
| 17 | `DistLine3Segment3<float>::Get(fT, v0, v1)` | `0x00A80A40` | C-precision (inherited) | as rows 16 and 15 |

---

## 6. Closing summary

**Verdict counts over the 17 ordinary-input float rows**

| verdict | rows | which |
|---|---|---|
| **S** (source/algorithm difference) | **0** | - |
| **C** (codegen difference) | **17** | all of them: 2 evaluation order, 1 root path, 14 precision/exponent range (4 of those also carry an association difference) |
| **H** (harness wiring) | **0** | - |

No row looks like a genuine source difference. Every failing row's operation
sequence is the WM3.8 tree's; the divergences are all *evaluation* properties
of the two numeric domains. The closest candidate to an **S** is the pair of
double constants in the retail's `DistLine3Segment3` body
(`FLD double ptr [0x00D71CD8]` = `2.0`, `FADD qword ptr [0x00E4F7E0]` =
`0.0`) where the source tree uses float constants - but both are exactly
representable and numerically inert, so they cannot be the cause of the
mismatch.

**C mechanism histogram**

| mechanism | rows | retail addr | symptom |
|---|---|---|---|
| evaluation order (right-to-left vs left-to-right sum) | 2 | `0x00452AD0` | 1 ulp, at **ordinary** magnitudes (the two rows' 27 812 + 27 837 mismatches are mostly in the normal range) |
| root path / precision (`FSQRT` at `_PC_24` vs a binary64 square root) | 1 | `0x0050D150` | 1 ulp, at ordinary magnitudes, amplified by the reciprocal |
| precision / exponent range (x87 register file vs binary32 SSE) | 14 | `0x004757E0`, `0x00A484F0`, `0x00A480E0`, `0x00A48190`, `0x00A48340`, `0x00A6C730`, `0x00A6C880`, `0x00A7E2A0`, `0x00A7E480`, `0x00A80DE0`, `0x00A809A0`, `0x00A80C10`, `0x00A80A40` | 1..8 ulp; the sampled mismatches land in binary32's **subnormal** band. Four of these rows (`0x00A80DE0` group) additionally carry an association difference, visible in the normal range |

(The two `0x004757E0` rows and the four `0x00A80DE0`-group rows share one retail
body each, so the address list has 13 entries for 14 rows.)

**Shared core.** There is no single shared core in the "one bug" sense: the
Distances do not call `Vector3<float>::Dot`/`SquaredLength` (the retail bodies
inline their own copies of those expressions), so the Dist rows fail for their
own reasons. What they do share with `LengthSq`/`SquaredLength` is the
*numeric domain*: the retail's x87 register file versus the rebuild's binary32
SSE. That single difference accounts for 14 of the 17 rows; the `Dot` rows fail
for a different reason (association order), and `InvSqrt` for a third (the width
of the square root).

**Engine impact.** None of these rows is an engine-visible defect: all 17 are
1-to-8-ulp differences on results that are subnormal in binary32 or differ in
the last bit of an ordinary-magnitude result. `DistVector3Segment3`,
`DistSegment3Segment3`, `DistVector2Box2` and `DistLine3Segment3` are the
distance queries the engine actually uses, and the difference is below the
precision any game logic can observe.

---

## 7. Reproduction

The measuring instrument lives in `.tmp/floatord/` (scratch only; nothing in
the rebuild, the harness or the compiler options was touched):

```
.tmp/floatord/f32lib.py     binary32 and x87 (24-bit, wide exponent) numeric backends
.tmp/floatord/x87emu.py     x87 emulator (the FLD ST(i) / FADDP / FMULP semantics)
.tmp/floatord/verify*.py    the per-row reproductions quoted above
.tmp/disasm.py              retail disassembler:  .tmp/venv/Scripts/python.exe .tmp/disasm.py <va> [count]
```

Row reproductions (each prints `x87 == retail N/N` and `binary32 == ours N/N`):

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme \
    --max-mismatch 400 --filter "<row name>"
```

The control-word experiment of §2.0:

```
./build/clang-x86-release/tools/wmdiff/wmdiff.exe --iters 200000 --seed 0x5EED2007 --skip-extreme \
    --max-mismatch 0 --cw 0x027F
```

The extreme-input rows that complete the 34-row failure set are attributed in
`float-analysis-extreme.md`.
