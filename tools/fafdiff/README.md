# fafdiff

`fafdiff` is a **differential harness**: it compares the main engine rebuild's
source (`src/...`) against the retail
`C:\ProgramData\FAForever\bin\ForgedAlliance_6c6c92f.exe`, bit for bit.

It is modelled on `dependencies/WildMagic3p8/wmdiff/`, which does the same
thing for the Wild Magic 3.8 library rebuild. The shared core
(`src/fuzz.h`, `src/gen.cpp`, `src/pe_image.*`) follows wmdiff's.

## Scope

The design point: the translation units under test are **compiled into the
harness straight from `src/`**, with the engine's own switches (the engine's
CMake target links `faf_codegen`, the same code-generation contract the engine
uses). Nothing is mirrored by hand, so what is measured is the engine's own
code.

Beyond that, fafdiff adds dimensions the earlier leaf-function harness does
not have:

* **systematic per-TU coverage** — one translation unit at a time, in a ledger,
  rather than "whatever was recovered so far";
* **object state** — not only return values and out-parameters but the bytes of
  the objects the functions leave behind;
* **call sequences** — the same inputs driven through a sequence of retail
  calls, not a single call per row;
* **a retail-counterpart ledger** — a recorded answer to "does the retail
  image have this function at all?", so rebuild-only helpers are recorded as
  out of scope instead of being paired against nothing.

## Build

The harness is a target of the engine's own CMake project, so it is built with
the engine's compiler (clang-cl) and the engine's code-generation contract:

```bash
# from the repository root
cmake --preset clang-x86-release
cmake --build --preset clang-x86-release --target fafdiff
# -> build/clang-x86-release/tools/fafdiff/fafdiff.exe
```

The harness maps a 32-bit retail image, so it exists for x86 configures only.
It is `EXCLUDE_FROM_ALL`: the engine build does not depend on it.

## Run

```bash
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --list
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --iters 1000000 --seed 0x5EED2007
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --iters 1000000 --filter "SomeName"
```

Options: `--iters N`, `--seed S`, `--filter NAME` (comma-separated substrings),
`--max-mismatch N`, `--image PATH`, `--cw 0xNNNN`, `--snan`, `--strict-nan`,
`--skip-extreme`, `--list`.

The default retail image is
`C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe`.

## The comparison row

A row is one paired function. The ABI is **typed from the start**: the row's
own file owns the input and output struct types, and the harness only ever sees
opaque byte buffers behind them.

| field | meaning |
|---|---|
| `name` | short name, used by `--filter` |
| `va` | the retail entry point (0 until it is filled in) |
| `source` | where our side lives (`file:line`) |
| `inBytes` / `outBytes` | the size of the typed input and output structs |
| `genT` | fills the typed input struct from the row's RNG |
| `retailT` | calls the retail function through the mapped image |
| `oursT` | calls the engine's own function |

`genT` takes a `DrawKind`: `WithExtremes` (the default) draws the full
mixture, `Ordinary` (what `--skip-extreme` selects) draws only finite, in-range,
normal lanes.

The table lives in `src/targets.cpp`; the aggregator is `fafdiff::Targets(int&)`
declared in `src/targets.h`.

## State

**The table has ten rows.** Two translation units are under test:
`src/moho/math/Vector3f.cpp` (the row for `moho::NormalizeVectorInto`, retail
0x0044F7E0) and `src/moho/math/QuaternionMath.cpp` (the nine quaternion rows
below), both compiled into the harness with the engine's own switches. The
dependencies they bring with them are `src/legacy/math/X87Math.cpp` (the
`msvc8::` x87 helpers both of them call) and Wild Magic's
`Foundation/System/Wm3System.cpp` (`Wm3::Quaternion`'s copy assignment calls
`Wm3::System::Memcpy`).

```bash
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --list
# name               retail VA  our source                     typed layout
# NormalizeVectorInto 0x0044F7E0 src/moho/math/Vector3f.cpp:25  in[12B] out[16B]
# MatrixToQuat       0x004EB3F0 src/moho/math/QuaternionMath.cpp:256   in[36B] out[20B]
# RotateQuatByAngle  0x004EB740 src/moho/math/QuaternionMath.cpp:557   in[20B] out[20B]
# QuatLERP           0x004EBA80 src/moho/math/QuaternionMath.cpp:408   in[36B] out[20B]
# SLERP              0x004EBC00 src/moho/math/QuaternionMath.cpp:466   in[36B] out[20B]
# NormalizeQuatInPlace 0x004EDAA0 src/moho/math/QuaternionMath.cpp:220 in[16B] out[16B]
# QuatsNearEqual     0x004EDED0 src/moho/math/QuaternionMath.cpp:201   in[32B] out[4B]
# QuatFromVecRot     0x0069AA50 src/moho/math/QuaternionMath.cpp:606   in[32B] out[16B]
# QuatToAxisAndAngle 0x006C1070 src/moho/math/QuaternionMath.cpp:673   in[16B] out[16B]
# QuatCrossAdd       0x0044F880 src/moho/sim/Sim.cpp:6286             in[36B] out[20B]
```

### One extracted function, and why

`moho::QuatCrossAdd` (retail 0x0044F880) is defined in
`src/moho/sim/Sim.cpp`, next to its `CDebugCanvas` callers, and
`QuatFromVecRot` (0x0069AA50) calls it. The harness cannot link `Sim.cpp` as
a translation unit: the file has a non-COMDAT `.CRT$XCU` static initializer
(`__GLOBAL__sub_I_Sim.cpp`) that is a link root, references every static
object in the file, and drags in most of the engine -- 591 of the engine's
941 translation units by the object-level symbol closure, and adding the
handful of TUs that satisfy the first round of undefined symbols only moves
the gap deeper into the sim core.

So the harness does not link the file; it compiles **just the function**.
`extract.json` lists, by hand, the line ranges to copy and the sha256 of the
source; `scripts/extract_sources.py` slices them into
`<build>/tools/fafdiff/generated/`; the CMake target compiles that copy with
the engine's own switches. Two hashes close the loop: a source edit fails the
build until the ranges and `sourceSha256` are re-checked, and a changed
extraction fails the build until somebody reads it and records
`outputSha256`. Details in `AGENTS.md`.

The function therefore stays out of the coverage ledger, whose predicate is
"the file lives under `src/moho/math/` or `src/legacy/math/`" -- the ledger
holds 38 functions, and the row below is the tenth of the paired table.

## The paired rows

### `NormalizeVectorInto` -- retail `0x0044F7E0`

`src/moho/math/Vector3f.cpp:31` (the doc comment starts at line 25).

**The retail convention, measured from the image**
(`.tmp/disasm.py 0x0044F7E0`):

| | |
|---|---|
| destination | `ESI` |
| source | `EDI` |
| return value | `EAX` = the destination pointer (`mov eax, esi` at 0x0044F82A, on both paths) |
| epilogue | `add esp, 0x0C` / `ret` -- the stack only; the `push ecx` at 0x0044F810 is the four-byte slot the float argument to the sqrt call lives in, not a saved register (the body never writes `ECX`) |

The body: the squared length in SSE float (`(x*x + y*y) + z*z`), the retail's
own sqrt at 0x00452FC0 (`fld; fsqrt`, i.e. an x87 fsqrt under the harness's
`_PC_24` control word), the length rounded to float by `fst dword [esp+8]`,
then `fcompi` against zero and `jbe` to the zero branch. `jbe` is taken when
the length is **not positive or unordered**, so a NaN length also produces the
zero vector. The reciprocal is `divss` (`1.0f / length`), multiplied into the
three lanes.

**The typed shapes**

```cpp
struct NormIn  { Wm3::Vector3f source; };              // 12 bytes
struct NormOut { Wm3::Vector3f dest;                   // 12 bytes
                 std::uint32_t returnedDest; };        //  4 bytes
```

`returnedDest` is the answer to the question the return value answers -- "is it
the destination pointer?" (1 = yes). The pointers themselves cannot be
compared: the two sides write into different output buffers.

**The generator**

* `Ordinary` (`--skip-extreme`): every lane finite, normal (never subnormal),
  non-zero, and the vector therefore has a strictly positive length. The
  degenerate branch is **not** reached here.
* `WithExtremes` (the default): 40% `GenVec3` (unit vectors, plain vectors,
  mixed magnitudes, axis-aligned lanes), 15% the zero-length vector (with
  signed zeros -- the degenerate branch), 15% NaN- and infinity-laden vectors,
  15% denormal lanes (the squared length underflows) and 15% a unit direction
  at a random magnitude (the squared length overflows to infinity).

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep (`0x5EED2007`, `0xDEADBEEF`, `0x12345678`, `0xA5A5A5A5`, `0xC0FFEE01`), 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation 1: `1.0f / length` -> `1.0f / (length + 1e-7f)` | `[FAIL] 248312 mismatches in 1000000 iterations` |
| restore mutation 1 | `[PASS] 1000000/1000000` |
| mutation 2: `length > 0.0f` -> `length >= 0.0f` (the degenerate branch) | `[FAIL] 333413 mismatches in 1000000 iterations` |
| restore mutation 2 | `[PASS] 1000000/1000000` |

The harness's build of `Vector3f.cpp` and the engine's own build of it were
compared instruction by instruction (`llvm-objdump` on both `.obj` files):
identical. The engine's x86 `/FIplatform/X87SqrtCall.h` does not change this
translation unit -- `faf_codegen` already keeps `sqrt` a call on x86.

## The quaternion rows

All nine rows share one convention: the quaternion is stored **scalar-first**,
`w` in lane 0 and `x`, `y`, `z` in lanes 1..3 -- the layout
`Wm3::Quaternionf` has (`m_afTuple[0]` is `w`). The retail reads lane 0 as the
scalar in every one of the nine, and the engine's quaternion code does the
same.

Every retail convention below was **measured** from the image with
`.tmp/disasm.py <va>` (and cross-read against the engine's own doc comments,
which quote the same addresses), never read off a header.

### `MatrixToQuat` -- retail `0x004EB3F0`

`src/moho/math/QuaternionMath.cpp:256`.

**The retail convention**

| | |
|---|---|
| matrix | `EAX` -- three packed rows of three floats (row 1 at +0x0C, row 2 at +0x18), i.e. `Wm3::Vector3f rows[3]` |
| destination | `ESI` |
| return value | `EAX` = the destination pointer (`mov eax, esi` at 0x004EB491) |
| arguments | none on the stack |

The trace is the diagonal (`[edi+0]`, `[edi+0x10]`, `[edi+0x20]` -- the three
lanes 16 bytes apart, which in a packed 3x3 are exactly the diagonal), summed
in float SSE; `comiss` against `0.0f` picks the trace branch. The other branch
picks the dominant diagonal and reads the cross-shuffle table at `0x00F61F1C`
(`{1, 2, 0}`). The quaternion is written as `[w, x, y, z]`.

**The typed shapes**

```cpp
struct MatQuatIn  { Wm3::Vector3f rows[3]; };            // 36 bytes
struct MatQuatOut { Wm3::Quaternionf quat;                // 16 bytes
                    std::uint32_t returnedDest; };       //  4 bytes
```

**The generator**

* `Ordinary`: a rotation matrix built from a random axis and a random angle,
  so the trace is `1 + 2*cos(angle)` -- the trace branch (angle < pi/2) and the
  dominant-axis branch (angle near pi) are both reached.
* `WithExtremes`: 45% the same rotation matrices, 20% plain matrices (the
  trace is then an arbitrary sum) and 35% rows whose lanes are drawn from the
  full float mixture (NaN, infinity, denormal, zero), which drive the trace
  test and the sqrt into their unordered branches.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[FAIL] 1614 mismatches` -- **NaN-payload only** (see below) |
| mutation: `trace > 0.0f` -> `trace > 1.0f` (the branch decision) | `[FAIL] 158569 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `RotateQuatByAngle` -- retail `0x004EB740`

`src/moho/math/QuaternionMath.cpp:557`.

**The retail convention**

| | |
|---|---|
| quaternion | `ESI` (modified in place) |
| angle | the first stack argument (`fld dword ptr [ebp + 8]` at 0x004EB749) |
| return value | `AL` (0 or 1) |
| epilogue | `mov esp, ebp / pop ebp / ret` -- the argument is **not** popped, the caller does it |

The half-angle gate: `fld [ebp+8]; fmul 0.5f; fabs` and then `fcomi` against
`pi/2` (`0x00E4F7A0`); `|rads|/2 >= pi/2` rejects. The second gate is
`comiss` of the squared axis length against `sin^2(half angle)` -- and it
rejects the unordered case too. The sign of the sine follows the hemisphere
(`comiss` of `0.0f` against the `w` lane). The axis is normalised by
`0x00452AF0`, and `sin`/`cos` are the x87 `fsin`/`fcos` of the half angle.

**The typed shapes**

```cpp
struct RotQuatIn  { Wm3::Quaternionf quat; float rads; };   // 20 bytes
struct RotQuatOut { Wm3::Quaternionf quat;                 // 16 bytes
                    std::uint32_t accepted; };             //  4 bytes
```

The retail works in place, so the row copies the input quaternion into the
output buffer first and hands the retail that buffer; `accepted` is the
retail's `AL`.

**The generator**

* `Ordinary`: a unit quaternion and an angle in `[-3, 3]`.
* `WithExtremes`: the full quaternion mixture (unit, unnormalized, the zero
  quaternion, denormal and NaN lanes) and an angle drawn from the edge table:
  `0`, `pi`, `pi` +- 1 ulp, `pi/2`, `-pi/2`, NaN, `+inf`, and both the wide
  and the narrow band around zero -- the values that sit on the half-angle
  gate.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the axis is used unnormalised | `[FAIL] 374166 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `QuatLERP` -- retail `0x004EBA80`

`src/moho/math/QuaternionMath.cpp:408`.

**The retail convention**

| | |
|---|---|
| q1 | `EAX` |
| q2 | `ECX` |
| destination | `EBX` |
| amount | the first stack argument (`movss xmm7, [esp + 0x24]` at 0x004EBAB8) |
| return value | `EAX` = the destination pointer (`mov eax, ebx` at 0x004EBBF1) |
| epilogue | `pop edi / pop esi / add esp, 0x18 / ret` -- the amount is **not** popped, the caller does it |

The retail calls `QuatsNearEqual(q2, q1)` first (the operands are the other
way round from `SLERP`'s call) and copies `q2` into the destination when the
two are equal. The amount is clamped to `[0,1]` with `comiss` (a NaN amount
clamps to 1). The dot product is summed in storage order (`w` first), and the
endpoint that is negated when the dot is negative is **q1**.

**The typed shapes**

```cpp
struct LerpIn  { Wm3::Quaternionf q1; Wm3::Quaternionf q2; float amount; }; // 36 bytes
struct LerpOut { Wm3::Quaternionf quat; std::uint32_t returnedDest; };     // 20 bytes
```

**The generator**

* `Ordinary`: two unit quaternions -- independent, nearly equal, or nearly
  antipodal (the perturbation is `1e-5`, so the sign of the dot product is
  the interesting part) -- and an amount in `[0,1)`.
* `WithExtremes`: the same pair structure over the full quaternion mixture
  (which includes **exactly antipodal** pairs: the short-arc decision turns on
  the sign of the dot product), and an amount from the edge table: `0`, `1`,
  `1 + 1 ulp`, just below `1`, the smallest denormal, `-0.0f`, `-0.0f` as a
  bit pattern, NaN, and the band around the ends.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the short-arc test `dot < 0.0f` -> `dot > 0.0f` | `[FAIL] 470445 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `SLERP` -- retail `0x004EBC00`

`src/moho/math/QuaternionMath.cpp:466`.

**The retail convention**

| | |
|---|---|
| q2 | `EAX` |
| q1 | `ECX` |
| destination | `EBX` |
| t | the first stack argument (`movss xmm1, [ebp + 8]` at 0x004EBC3E) |
| return value | `EAX` = the destination pointer (`mov eax, ebx` at 0x004EBEB0) |
| epilogue | `mov esp, ebp / pop ebp / ret` -- the argument is **not** popped, the caller does it |

**Note the argument order**: the retail's first register argument is `q2` and
the second is `q1` -- the other way round from `QuatLERP`. The retail calls
`QuatsNearEqual(q1, q2)` and copies `q1` into the destination when the two
are equal. `t` is clamped to `[0,1]` (a NaN `t` clamps to 1). The spherical
path is gated by `1 - dot > 0.001`, by the unrounded `acos(dot) >= 0.001`, by
`pi - angle >= 0.001` and by the finiteness of the angle; the weights, the
sines and the products stay on the x87 stack until the one float store per
lane. The fallback is a linear blend plus `NormalizeQuatInPlace`.

**The typed shapes**

```cpp
struct SlerpIn  { Wm3::Quaternionf q2; Wm3::Quaternionf q1; float t; };  // 36 bytes
struct SlerpOut { Wm3::Quaternionf quat; std::uint32_t returnedDest; };   // 20 bytes
```

**The generator**

The same pair and parameter structure as `QuatLERP`: unit quaternions in the
`Ordinary` draw (independent, nearly equal, nearly antipodal), the full
mixture -- including exactly antipodal pairs -- in the `WithExtremes` draw,
and the same edge table for `t`.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the short-arc test `dot < 0.0f` -> `dot > 0.0f` | `[FAIL] 510974 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `NormalizeQuatInPlace` -- retail `0x004EDAA0`

`src/moho/math/QuaternionMath.cpp:220`.

**The retail convention**

| | |
|---|---|
| quaternion | `ESI` (modified in place) |
| arguments / return value | none |

The squares are summed in storage order -- `((w^2 + x^2) + y^2) + z^2` in
float SSE -- and the length is the retail's own sqrt (`0x00452FC0`, an x87
`fsqrt` under the harness's `_PC_24` control word), rounded to float by
`fst`. `fcompi` against `1e-6f` (`0x00D71BFC`) and `jbe` take the zero branch
when the length is **not** greater than the epsilon, so a NaN length zeroes
the quaternion too. Otherwise the reciprocal is `divss` and is multiplied into
the four lanes.

**The typed shapes**

```cpp
struct NormQuatIn  { Wm3::Quaternionf quat; };   // 16 bytes
struct NormQuatOut { Wm3::Quaternionf quat; };   // 16 bytes
```

**The generator**

* `Ordinary`: a unit quaternion.
* `WithExtremes`: the full mixture -- unit quaternions, unnormalized ones,
  the zero quaternion (with signed zeros and denormal lanes), arbitrary finite
  lanes and NaN/infinity lanes. The degenerate branch (the length is not
  greater than 1e-6, and the NaN length) is reached here.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the scaling is dropped (`1.0f / length` -> `1.0f`) | `[FAIL] 574098 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `QuatsNearEqual` -- retail `0x004EDED0`

`src/moho/math/QuaternionMath.cpp:201`.

**The retail convention**

| | |
|---|---|
| first operand | `EDI` |
| second operand | `ESI` |
| return value | `EAX` = 0 or 1 (`mov eax, 1` at 0x004EDF5F, `xor eax, eax` at 0x004EDF66) |

Each lane is compared with `subss` and the retail's `fabs` (`0x0040DAB0`),
then `fcompi` against `1e-6f` (`0x00DFFBE8`): the lane is *equal* when
`|a - b| <= 1e-6`. The lanes are compared in storage order (`w` first), and
the unordered case (a NaN lane) rejects.

**The typed shapes**

```cpp
struct NearIn  { Wm3::Quaternionf a; Wm3::Quaternionf b; };  // 32 bytes
struct NearOut { std::uint32_t equal; };                      //  4 bytes
```

**The generator**

The pair structure of `QuatLERP` (independent, nearly equal, nearly or
exactly antipodal) -- the near-equal pairs are what put the epsilon under
pressure, since the difference between the two operands is then of the order
of the perturbation.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the epsilon `1e-6f` -> `1e-5f` | `[FAIL] 16262 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `QuatFromVecRot` -- retail `0x0069AA50`

`src/moho/math/QuaternionMath.cpp:606`.

**The retail convention**

| | |
|---|---|
| reference axis | `ECX` |
| quaternion | the first stack argument (`mov ebp, [esp + 0x30]` at 0x0069AA62) |
| angle | the second stack argument (`fld dword ptr [esp + 0x54]` at 0x0069AB09) |
| return value | none; the quaternion is written in place |

The retail computes the third column of the rotation matrix (the forward
axis) from the quaternion, calls `QuatCrossAdd` (`0x0044F880`) with the
forward axis and the reference vector and the fallback axis `(0, 1, 0)`
(`lea edi, [esp + 0x2C]` at 0x0069AAF3), calls `RotateQuatByAngle` (`0x004EB740`)
on the delta -- and **ignores the returned flag**, so a rejected step
multiplies in the unclamped delta -- and finally pre-multiplies the delta
into the quaternion. The product's `x` and `y` lanes start from the cross
term, not from the scalar product; the engine keeps that grouping.

**The typed shapes**

```cpp
struct VecRotIn  { Wm3::Quaternionf quat; Wm3::Vector3f refAxis; float rads; }; // 32 bytes
struct VecRotOut { Wm3::Quaternionf quat; };                                     // 16 bytes
```

**The generator**

* `Ordinary`: a unit quaternion, a unit reference axis, an angle in `[-3, 3]`.
* `WithExtremes`: the full quaternion mixture, a reference axis from the full
  float mixture (so the axis can be zero, and the `QuatCrossAdd` degenerate
  branch -- the half turn about the fallback axis -- is reached) and the edge
  table of angles.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[FAIL] 3747 mismatches` -- **NaN-payload only** (see below) |
| mutation: the forward axis' `y` lane becomes a sum instead of a difference | `[FAIL] 562716 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `QuatToAxisAndAngle` -- retail `0x006C1070`

`src/moho/math/QuaternionMath.cpp:673`.

**The retail convention**

| | |
|---|---|
| quaternion | `ESI` |
| axis out | `EDI` |
| angle out | `EBX` |
| arguments / return value | none |

The squared axis length is `(x^2 + y^2) + z^2` in float SSE and is tested
against `1e-6f` (`0x00D71BFC`) with `comiss` and `jbe` -- so the NaN case
takes the degenerate branch too, which writes the axis `(1, 0, 0)` and the
angle `0`. The half angle is `pi` when `w <= -1` (the unordered case lands
here as well), `0` when `w >= 1`, and `acos(w)` (`0x00A8DCD0`, the retail's
double `acos`) otherwise; the angle is the half angle times `2.0f`. The axis
is scaled by `1/|axis|`, and the reciprocal stays on the x87 stack
(`fsqrt; fld1; fdivrp`) across the three multiplies.

**The typed shapes**

```cpp
struct AxisAngleIn  { Wm3::Quaternionf quat; };               // 16 bytes
struct AxisAngleOut { Wm3::Vector3f axis; float angle; };     // 16 bytes
```

**The generator**

* `Ordinary`: a unit quaternion.
* `WithExtremes`: the full mixture -- unit, unnormalized, zero, denormal and
  NaN lanes. The `w` lane is what drives the half-angle branch, and the
  vector lanes drive the degenerate branch.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the epsilon `1e-6f` -> `1e-5f` | `[FAIL] 836 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

### `QuatCrossAdd` -- retail `0x0044F880`

`src/moho/sim/Sim.cpp:6286`, extracted into the harness by `extract.json`
(see above).

**The retail convention**

| | |
|---|---|
| destination | `EBX` |
| fallback axis | `EDI` (a pointer) |
| v1 | the first stack argument, **by value** (three floats) |
| v2 | the second stack argument, **by value** (three floats) |
| return value | `EAX` = the destination pointer (`mov eax, ebx` at 0x0044F99D) |
| epilogue | `pop esi / add esp, 0x10 / ret` -- the arguments are **not** popped, the caller does it (24 bytes) |

Both vectors are normalised (`0x00452AF0`), the half vector is their sum,
and the quaternion is the dot product of the half vector with `v1` plus
`v1 x half`. The degenerate branch (`fcompi` + `jbe`: the half vector does
not normalise, or is unordered) writes a half turn about the **caller's**
fallback axis: the scalar lane is zero and the imaginary lanes carry the
normalised fallback (`NormalizeVectorInto`, `0x0044F7E0`).

**The typed shapes**

```cpp
struct CrossAddIn  { Wm3::Vector3f v1; Wm3::Vector3f v2; Wm3::Vector3f fallback; }; // 36 bytes
struct CrossAddOut { Wm3::Quaternionf quat; std::uint32_t returnedDest; };        // 20 bytes
```

**The generator**

* `Ordinary`: unit vectors; `v2` is either independent, nearly parallel or
  nearly antipodal to `v1`, and the fallback axis is a unit vector.
* `WithExtremes`: `GenVecPair` for the two vectors (which draws the
  antipodal and the degenerate cases) and `GenVec3` for the fallback axis, so
  the fallback can be zero as well.

**The evidence**

| check | result |
|---|---|
| 1,000,000 iterations, seed `0x5EED2007` | `[PASS] 1000000/1000000 bit-identical` |
| 1,000,000 iterations, seed `0xDEADBEEF` | `[PASS] 1000000/1000000 bit-identical` |
| the five-seed sweep, 1,000,000 each | `[PASS]` on all five |
| `--skip-extreme`, the same five seeds, 1,000,000 each | `[PASS]` on all five |
| `--strict-nan`, 1,000,000, `0x5EED2007` | `[PASS]` (no NaN-payload differences) |
| mutation: the degenerate test is inverted | `[FAIL] 992817 mismatches in 1000000 iterations` |
| restore | `[PASS] 1000000/1000000` |

## The NaN-payload differences

Two rows (`MatrixToQuat` and `QuatFromVecRot`) report differences under
`--strict-nan` that are **NaN payloads only**: both sides produce a NaN in
the same lane, and only the payload bits differ (for example the retail
writes `7FC00000` where the rebuild writes `7FC12345`, the payload of the NaN
that came in). The count is 1614 and 3747 lanes per million iterations
respectively, i.e. 0.16% and 0.37% of the iterations, and both come from the
same mechanism: the retail's x87 path quiets a NaN into the default one
while the rebuild propagates the payload (or the other way round).

The harness's default is to **tally** such differences rather than fail on
them (`--strict-nan` turns them into failures); the numbers are listed above
so the class is on the record rather than hidden.

## The engine's own build of the translation units

The harness compiles the translation units under test from `src/` with the
engine's switches; the engine compiles them itself. The two builds were
compared instruction by instruction (`llvm-objdump -d` on both `.obj` files,
the symbol names and the file header lines removed):

| translation unit | result |
|---|---|
| `src/moho/math/Vector3f.cpp` | identical (386 instruction lines) |
| `src/moho/math/QuaternionMath.cpp` | identical (2908 instruction lines) |
| `src/legacy/math/X87Math.cpp` | identical (1811 instruction lines) |
| the extracted `QuatCrossAdd` (`Sim.cpp` vs `generated/sim_quat_cross_add.cpp`) | identical (172 instruction lines) |

The engine's x86 `/FIplatform/X87SqrtCall.h` changes none of them:
`faf_codegen` already keeps `sqrt` a call on x86
(`/clang:-fno-builtin-sqrt`).

## Coverage ledger

`COVERAGE.md` is the per-function coverage ledger for the x87-tier math
surface: the functions of `src/moho/math/**` and `src/legacy/math/**` that the
retail image still executes with x87 arithmetic. Every row carries the
function's source-level name, its retail address, its source location and one of
four statuses (`未匹配`, `已匹配`, `失败`, `不适用`). Start there: it says, per
function, whether the pair exists yet, and -- for the rows marked `不适用` --
why it will never exist.

The table is **generated**, not hand-written: `scripts/gen_coverage.py` builds
it from the screen data (`scripts/screen_math.py` ->
`.tmp/fafdiff/screen.json`) and the hand-maintained status file
(`data/coverage_status.json`, one entry per address). To change a status,
edit `data/coverage_status.json` and re-run:

```bash
PATH=.tmp/venv/Scripts:$PATH .tmp/venv/Scripts/python.exe \
    tools/fafdiff/scripts/screen_math.py --out .tmp/fafdiff
PYTHONUTF8=1 .tmp/venv/Scripts/python.exe \
    tools/fafdiff/scripts/gen_coverage.py
```

The generator fails loudly (and writes nothing) if a status is missing, invalid
or lacks its `why` sentence, or if a status entry points at an address outside
the set -- the denominator of the ledger must not drift silently.
`scripts/gen_coverage.py --init-status` writes a fresh status file (everything
`未匹配`).
