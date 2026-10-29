# wmdiff

Differential test of this rebuilt Wild Magic 3.8 against the copy inside the
retail `ForgedAlliance.exe`. Same shape as `tools/difffuzz`: map the retail PE at
its preferred base, call the retail function and ours with identical random
inputs, and compare every output word bit for bit.

The difference from difffuzz is scope. difffuzz covers recovered engine sources
that its build cannot include directly, so it mirrors them by hand. wmdiff lives
inside this tree and links the real headers, so `*_harness.cpp` only ever
**forwards** — the code under test is the same source the engine compiles.

## Rules

**Never change compiler options to make a comparison pass.** Not in
`tools/wmdiff/CMakeLists.txt`, not in `faf_codegen`, not for one experiment.

The rebuild's job is to be built the way it is built. If retail's bits and ours
differ because of a compiler option, that is a *result* — the harness's job is to
report it, not to hide it by turning on a flag that happens to line the two up.
A special build would turn wmdiff into a machine for demonstrating what we could
prove if we had chosen different flags, which is worth nothing.

This is easy to be tempted by, because the temptation is measurable: rebuilding
with `/arch:IA32` takes `Vec3::LengthSq` from 575 mismatches to a clean
200000/200000. That is exactly why it is off limits. The flag does not make the
rebuild more correct; it makes the rebuild a different program.

If a row fails, the fix belongs in the rebuilt source or in the retail side of
the comparison — never in how either is compiled.

**Every wired row must pass the provenance audit.** A name, a similar operation,
or an entry in `tools/difffuzz`'s retail-helper catalog is not evidence that the
address is the WildMagic template's body. See "Provenance audit" under
"Locating a retail address" for the four checks (reachability, anchor, ABI/body,
xrefs); a row that fails one is removed, not kept as a "cross-check".

**Cross-check the WM3 source before concluding "codegen".** This tree is a WM4
translation, not a WM3 copy, and WM4 changed algorithms as well as spelling. A
failing row's logic is compared against `.tmp/wm3/Source` before the mismatch is
classified. See "Cross-check the WM3 source" below.

**There is one conformance mode in this reconstruction, and it is the
engine's.** `src/sdk/main.vcxproj` compiles with `/permissive-`, and the
whole tree now matches it: `dependencies/WildMagic3p8/Foundation/
Foundation.vcxproj` was switched to `/permissive-` as well, and the harness
compiles everything with it.

That switch cost twelve lines, all behaviour-neutral. The Wild Magic 3.8
sources came from Wild Magic 4, and two subtrees call members of a dependent
base class unqualified, which the strict mode rejects (error C3861):
`Foundation/RationalArithmetic`'s `Wm3Query3Filtered.inl` (`Det3`, `Det4`)
and `Wm3Query2Filtered.inl` (`Det2`, `Det3`), and `Foundation/Numerics`' four
Ode solvers (`m_oFunction`, eight calls). The calls now name their base -
`Query3<Real>::Det3`, `this->m_oFunction` - so the same function is called
with the same arguments and no behaviour changes. The Wild Magic 4 text under
`.tmp/wm4ref/` is the only thing that differs, and the engine executes
neither of those subtrees.

**Every row was re-measured after the switch and none of them moved**: the
56 rows pass 1,000,000/1,000,000 on every seed tried, with the Wild Magic
sources compiled the same way the engine compiles them. The rule above is
about hiding a *difference* between the retail and the rebuild; this was
about the tree and the instrument being built one way rather than two.

The engine's own binary **does** move with the switch - the Wild Magic
library's code generation changes with the mode - so the check that belongs
to it is the lockstep suite, and it passed: `tools/mpemu selftest` is clean
and `2p-main-x86-vs-x86` (the rebuild against the retail, five minutes of
real play) ends in **PASS**. See "`InBox`, and the harness's own build
flags" at the end of this file.

## Cross-check the WM3 source

`dependencies/WildMagic3p8` is not a copy of Wild Magic 3.8. Its API and a large
part of its implementation were translated from Wild Magic 4 and then corrected
only where wmdiff proved a difference. WM4 is not a renamed WM3: it changed
algorithms, branch structure, signatures and constants. A WM4-derived body is
therefore a suspect whenever a row disagrees with retail, and a disagreement
that changes a branch or a returned value is more likely a translated algorithm
than a rounding difference.

A second Wild Magic source sits at `.tmp/wm3/Source` (gitignored, local only).
It is the older WM3 Foundation tree:

| property | value |
|---|---|
| files | 914: 430 `.h`, 345 `.cpp`, 132 `.inl`, 5 `.mcr`, 1 `.vcproj`, 1 `.txt` |
| project | `Source.vcproj`, Visual Studio 2003 format 7.10 |
| source date | every file has mtime 2004-10-09; headers say copyright 2004, "The Wild Magic Library (WM3)" |
| scope | Foundation only; no `Renderers/`, no `Applications/` |
| not present | `ConvexPolyhedron3`, `Mapper2/3`, `IVector2/3`, `Quadratic2/3`, all of which the rebuild has |

The exact patch level is not established. The missing classes show it is not the
same source set as the rebuild, and the Pitfalls section below already calls the
`Vector3` reference "the Wild Magic 3.4 original"; treat it as a
**WM3.4-generation** tree, not as retail's exact source. It is still much closer
to retail's lineage than the WM4 translation is, and for `Foundation/Math`,
`Foundation/Intersection` and `Foundation/Distance` it is the first thing to
read when a rebuilt body's logic is in question.

If `.tmp/wm3/Source` is missing, say the comparison was not made. Do not
substitute a downloaded WM3 of a different version, and do not describe a
WM4-vs-rebuild diff as a WM3 check.

### The rule

**A failing row is not classified as a codegen difference until its WM3 body
has been compared** — or its absence from the WM3 tree has been established and
recorded as a WM4-only function. Apply this order:

1. **Harness first.** Confirm the thunk's stack slots, the retail ABI and the
   generator before reading either source. The largest errors in this file's
   history were harness bugs wearing the shape of code differences.
2. **WM3 logic second.** Diff the rebuilt function against `.tmp/wm3/Source`.
   Read control flow, comparisons, signs, gates and constants first; expression
   order and width second. A changed branch or comparison is a first-class
   hypothesis.
3. **Retail disassembly third.** Dump the retail body and check the hypothesis
   operation by operation.
4. **Only then name it codegen** — with the retail instruction sequence that the
   rebuilt source fails to reproduce.

A difference from WM3 is a hypothesis, not a verdict. The WM3 copy is older than
retail's build, so the WM3 text can itself be the wrong side of a later fix.
Retail's disassembly and wmdiff's measurement outrank both sources. What the
WM3 copy buys is the ability to tell **a translation error** from **a compiler
difference** before spending a day on the compiler.

### Comparing without drowning in noise

The path mapping is one-to-one:

    .tmp/wm3/Source/<Dir>/<File>  <->  dependencies/WildMagic3p8/Foundation/<Dir>/<File>

These differences are expected and are not logic:

- the copyright header (`Magic Software, Inc.` 2004 vs `Geometric Tools, Inc.`
  1998-2006);
- `#include "Wm3FoundationPCH.h"` in the rebuild;
- brace style and line wrapping (the rebuild braces single-statement bodies);
- `memcpy` replaced by element loops in copy constructors and `operator=`;
- the `// float` / `// double` comments above explicit instantiations;
- `m_afTuple[i]` vs `x`/`y`/`z`: the rebuild's public union overlays the same
  storage (see the `FAF MOD` comment in `Wm3Vector3.h`), so the two spellings
  are not a difference;
- `if constexpr (std::is_same_v<Real, float>)` branches: FAF additions that
  carry retail-exact float arithmetic. Strip them to see the upstream body.

Use `diff -w` and read top-down. The `else` branch of a float branch is the WM3
lineage; changes that exist only in the float branch are wmdiff findings and
should already be documented.

### Differences already established; do not "correct" them back

These disagree with the WM3 copy on purpose and match retail. Restoring the WM3
spelling reopens the row.

| function | WM3 copy | rebuild | evidence |
|---|---|---|---|
| `Math::InvSqrt` | `1.0/sqrt((double)x)` | `1/Sqrt(x)` | 0x0050D150 rounds twice at `_PC_24` |
| `IntrBox3Sphere3::GetVertexIntersection` / `GetEdgeIntersection` | `Math<Real>::InvSqrt(Math<Real>::FAbs(...))` | `1.0f/std::sqrt(Math<Real>::FAbs(...))` - a **float** square root | retail's CRT sqrt is a 24-bit one under `_PC_24` (`0x00A91028 CMP word [ESP], 0x027F`); 0x00A417CE |
| `IntrBox3Sphere3::GetVertexIntersection` / `GetEdgeIntersection` | `Math::InvSqrt` | `1.0/std::sqrt(double)` | retail inlines the reciprocal and rounds once, 0x00A417CE |
| `DistVector3Box3::GetSquared` | `kDiff.Dot(axis)` | explicit x-first `Mul24`/`Add24` | retail's inlined dot is x-first; 20947 -> 48 -> 0 |
| `DistVector3Segment3::GetSquared` | the same `Dot` | explicit x-first, product stored to float | the dot gates the branch; 7281 -> 0 |
| `Vector3::Dot` / `LengthSq` | source order | z-first with `RoundTo24` | x87 order and extended exponent, 0x00452AD0 / 0x004757E0 |
| `Math::FMod` | plain `fmod` | zero-dividend fix for float | 2007 CRT vs modern CRT on `fmod(-0, y)` |

### Differences that are still open

These are WM4 imports with no WM3 counterpart. They are not automatically
wrong, but they are where a logic divergence starts:

- **`bSolid` in the box intersectors.** `IntrLine3Box3::DoClipping` takes
  `bool bSolid`, and `IntrSegment3Box3`'s constructor takes it; WM3.4 has
  neither. The flag is not cosmetic: with `bSolid == true`, `DoClipping` reports
  the segment even when nothing was clipped. For a line entirely inside the box
  that means `IT_SEGMENT` with its two points computed from the unclipped
  `fT0 = -MAX_REAL` / `fT1 = +MAX_REAL`, where the WM3.4 body returns
  `IT_EMPTY`. The rebuild's two call sites treat it differently —
  `CColPrimitiveBase.cpp:385` passes `false`, while `IntrLine3Box3::Find`
  hard-codes `true`.

  **This one is closed.** The retail's own bodies answer it, and the answer
  is that the rebuild is right on both sides: `IntrLine3Box3::Find`
  (0x00A508F0) pushes the literal `1` for the flag, and
  `IntrSegment3Box3::Find` (0x00A462A0) reads it out of the object as a byte
  (`MOVZX EDX, byte ptr [ECX+0x14]`). Both `Find` rows are now wired, and
  the `bSolid` path is measured rather than assumed - see "Four more rows"
  below.
- **Static `Vector3::Zero/Add/Sub/Scale/Dot/Cross/LengthSq`.** FAF additions,
  not upstream symbols; see Caveats.

### Record the comparison

When the WM3 diff produces the fix, record it on the row: the WM3 file and
function compared, what differed, and whether retail confirmed the WM3 form,
the rebuild form, or neither. "Matched WM3" is not evidence that a row is
correct — only wmdiff is — but an unrecorded diff is how a translation error
gets re-introduced by the next reader.

## What counts as correct

**Bit-identical behaviour with retail is the standard.** A row is correct when
our side and the retail side produce the same bits for the same inputs, full
stop. That is the strongest evidence available that the rebuilt source is right,
it is the only evidence this harness produces, and it is the one we hold
ourselves to.

Understanding *why* a row fails is preparation, not absolution. Knowing that
`Vec3::Dot` fails because of summation order tells us what to try; it does not
move the row one step closer to passing. **A failing row is an open item until it
passes.** Do not sort rows into "known" and "unknown" buckets and treat the
first bucket as done — a rebuild bug and a codegen difference look identical from
the outside, and the only way to tell them apart is to close the gap.

The compiler gap is a real constraint, not an excuse. Retail was emitted by a
2007-era MSVC, this rebuild by 14.38/14.44, and we are not going back. That means
the only place a divergence can be closed is the **rebuilt source**: write the
expression such that this compiler emits what retail's did. If some divergence
turns out to be genuinely unreachable that way, say so plainly and name it as a
permanent limitation with its mechanism attached. Do not quietly promote it into
"accepted".

There is one other thing fair to change when a row fails, and it is not the
rebuild: the harness's fidelity to the retail runtime. Retail executes under a
particular x87 control word and MXCSR; if our side runs under different FP flags
the comparison is not measuring what it claims to. Making the environment
faithful is part of getting a correct answer. Changing how the rebuild is
compiled is not — see `## Rules`.

### The three former failing rows, all closed in source

- **`Vec3::Dot` — 0 / 200000 (was 134).** The sum order was fixed first (retail
  sums `(z*z' + y*y') + x*x'`, because MSVC's x87 pattern for a left-associative
  chain adds the leftmost term last; SSE evaluates strictly left to right, so the
  rebuilt sum is written z-first). That took the row from 14.3% to 0.067%. The
  residual was **exponent range**, not rounding: the 134 cases split into 91
  overflow cases (a binary32 product saturates to inf and `inf + (-inf)` gives
  NaN, where x87's 15-bit exponent defers the overflow to the final store and
  yields an infinity), 23 subnormal results, 6 normal results whose intermediate
  products were subnormal, 13 signed-zero differences and 1 normal/inf case.
  Widening the intermediates to plain `double` is much worse (41060) because 53
  significand bits change the ordinary cases. What matches retail is `double`
  storage with the significand forced back to 24 bits after every operation; see
  `Wm3Detail::RoundTo24` in `Wm3Vector3.h`.
- **`Math::InvSqrt` — 0 / 200000 (was 25047).** Retail `0x0050D150` is
  `FLD float [EBP+8]; FSQRT; FLD1; FDIVRP; RET` under `_PC_24`: the square root
  rounds to 24 bits, the reciprocal rounds to 24 bits, and the final `FSTP` is
  then exact. Ours computed `1.0/sqrt((double)x)`, which keeps 53 bits across
  both steps and rounds once at the end - a different result on 12.5% of inputs,
  always by 1 ULP. Rewriting it as `1/Sqrt(x)` makes each step round where the
  retail instruction does.
- **`Vec3::LengthSq` — 0 / 200000 (was 575).** The summation order already
  matched retail (`z*z + (y*y + x*x)`; `FMULP ST2` is `ST(2) = ST(2) * ST(0)`
  then pop). The whole 575 was explained by the x87 `_PC_24` extended-exponent
  model: 520 subnormal/subnormal, 28 subnormal/zero and 27 normal/normal (the
  last group differs because an *intermediate* product is subnormal while the
  stored result is normal). The same emulation as `Dot` closed it.

All three now pass **1,000,000/1,000,000** iterations on seeds `0x5EED2007`,
`0x12345678` and `0xDEADBEEF`, and 200,000/200,000 on the default run; the other
six wired rows still pass 200,000/200,000. NaN-payload-only differences (76 on
`Dot`, 57 on `LengthSq` in 200000 iterations) are still tallied rather than
failed, as they always were; `--strict-nan` remains the flag that counts them.

The MXCSR question that used to be the "highest-value thing left to check" is
settled, and it was a red herring. The banner's `MXCSR=0x1FA0` is the configured
`0x1F80` with the **sticky precision-exception flag** (bit 5, `0x20`) set by
startup code. DAZ is bit 6 (`0x40`) and is clear. `SetFpEnv` writes `0x1F80`
before every call, which is exactly the control value the engine's
`IsDefaultFloatingPointEnvironment()` gates on (`_mm_getcsr() & 0x1F80 ==
0x1F80`). The denormal residuals were the x87 extended exponent, not the SSE
denormal mode.

The x87 control word itself is no longer a guess either: `0x007F` decodes as
precision control `00` (24-bit significand, `_PC_24`), rounding control `00`
(nearest), and the six low exception masks set. The engine installs that word
with `_controlfp(_PC_24, _MCW_PC)` (`WinApp.cpp:2476`, `SimDriver.cpp:1121`) and
`MathReflection.cpp` recognises it as `(controlWord & 0x7F) == 0x7F`.

## Layout

| file | role |
|---|---|
| `src/main.cpp` | driver: FP environment, canary comparison, reporting |
| `src/fuzz.h`, `src/gen.cpp` | input generators and the `Target` record |
| `src/pe_image.cpp` | maps the retail PE, traps its imports |
| `src/targets_wm3.cpp` | chains the per-class slices into one table |
| `src/wm3_vec_targets.cpp` | Wm3Vector3 rows (9) |
| `src/wm3_math_targets.cpp` | Wm3Math rows (3) |
| `src/wm3_geom_targets.cpp` | Wm3Box3 / Wm3AxisAlignedBox3 rows (2) |
| `src/wm3_coll_targets.cpp` | Wm3 Intersection / Distance rows (35) |
| `src/wm3*_harness.{h,cpp}` | our side: forwards to the real headers |
| `src/wm3*_thunks.{h,cpp}` | retail side: a naked call through a known VA |

A row is in the table only once its retail address is known, and a thunk exists
only for a row. The functions that are reachable but have no retail address yet
- the Vector2 block, most of Math, the Quaternion block - are deliberately
**not** carried as rows with a zero address: they would report a mismatch
against the 0xCDCDCDCD canary on every draw and drown the rows that mean
something. They are listed in "What is in scope" below and in `reachable.md`,
which is generated from the reachability analysis and is the work list.

`reachable.md` is one generation behind on the counters - it says
"wmdiff-wired 20" where the table now holds thirty rows - because
regenerating it needs a fresh Ghidra analysis of the Debug binary with its
PDB, which is what `reachability/README.md` describes. Its *candidate* list,
the reachable functions with no wmdiff row, is the part that matters here and
it is unaffected.

## Building

The harness is a target of the engine's own CMake project, compiled by
**clang-cl x86** with `faf_codegen` and `faf_wm3_build_flags` - the same
compiler and the same code-generation contract as `src/`, so the Wild Magic
code under test is built exactly the way the engine builds it.  It is
`EXCLUDE_FROM_ALL`; the engine build does not depend on it.

    cmake --preset clang-x86-release
    cmake --build --preset clang-x86-release --target wmdiff

The binary lands at `build/clang-x86-release/tools/wmdiff/wmdiff.exe`.
The presets need `LLVM_ROOT` pointing at an LLVM install and Ninja on `PATH`
(both ship with Visual Studio 2022).

`Foundation/Math/Wm3Math.cpp` is the one library `.cpp` compiled rather than
header-reached: it carries the explicit specialisations of the `Math<float>`
constants (`PI`, `HALF_PI`, `LN_2`, ...), which `ACos`/`ASin` return. **Do not**
resolve that link error by redefining the constants in this tree - that would
take them out of the comparison.

The harness needs `Vector3<float>::CompareArrays` and
`Quaternion<float>::CompareArrays` to be reachable.  The authoritative tree
keeps them private, so `dependencies/patches/wildmagic3p8_wmdiff_access.patch`
makes them public - an access specifier only, with no effect on generated
code.  Apply it after the FAF patch, from inside the dependency tree.

### The harness compiles the code under test as the engine does

The harness's own flags are not free: the code under test has to be compiled
the way the engine compiles it, or the rows measure a different program. Two
things were wrong and are now right:

* **One conformance mode.** The engine's `main.vcxproj` and, now, the Wild
  Magic `Foundation.vcxproj` both compile with `/permissive-`, and so does
  the harness - one mode for the tree, the engine's own code and the
  instrument alike. See the Rules section above for what switching the Wild
  Magic library to it cost.
* **The same Foundation surface the engine builds.** The `InBox` row needs
  `Foundation/Containment/Wm3ContBox3.cpp`, and that file's other functions
  pull in the convex hull, the Gauss-points fit and the eigen-decomposition.
  The harness's source list now carries them, with the include directories
  they need:

  | added source | why |
  |---|---|
  | `Math/Wm3Vector2.cpp`, `Math/Wm3Vector3.cpp`, `Math/Wm3Quaternion.cpp` | the `ZERO`/`UNIT_*` statics and `Quaternion::ms_iNext` are FAF-MOD additions defined in the `.cpp`, not the header |
  | `System/Wm3System.cpp` | `System::Read4le`/`Write4le`/`Read8le`/`Write8le` |
  | `Containment/Wm3ContBox3.cpp`, `Wm3ContBox2.cpp` | `InBox` lives in the first |
  | `Containment/Wm3ConvexHull.cpp`, `Wm3ConvexHull1-3.cpp`, `Wm3HullEdge2.cpp`, `Wm3HullTriangle3.cpp` | `ContConvexHull3` and `ContMinBox` |
  | `Approximation/Wm3ApprGaussPointsFit2.cpp`, `Wm3ApprGaussPointsFit3.cpp`, `Numerics/Wm3Eigen.cpp` | `ContMinBox` |

  and the include directories `Foundation/Approximation`,
  `Foundation/Numerics`, `Foundation/Meshes` and
  `Foundation/RationalArithmetic`.

## Running

    build\wmdiff_release.exe --list
    build\wmdiff_release.exe --iters 1000000 --filter Math::
    build\wmdiff_release.exe --skip-extreme --strict-nan

Useful flags: `--list`, `--filter NAME` (comma-separated), `--iters`, `--seed`,
`--cw` (x87 control word, default `0x007F`), `--snan`, `--strict-nan`,
`--skip-extreme`, `--include-dead`, `--max-mismatch`, `--image PATH`.

`--skip-extreme` drops draws whose input words look like NaN, denormals or
values beyond 2^63 **when read as floats**, so it measures ordinary-value
behaviour only.  A row whose input block also carries counts, indices, bools
or mask bytes never produces an ordinary draw: it is reported as
`[SKIP] ... 0/N run: every draw had an extreme word (not a pass)` and counted
separately, never as a pass.  A partly skipped row says how many draws went
(`(+N extreme inputs skipped)`).

**The suite runs the targets the engine executes.**  47 of the 156 rows are
`double` instantiations that no `src/` call site reaches - `double-audit.md`
verifies each one - and they are skipped by default, so a run reports
`75/109` rather than `75/156`.  They stay in the table and `--include-dead`
runs them; `--list` marks them `[dead: not executed by the engine]`.

The default image is `C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe` —
the patch-level build this rebuild actually targets, not the retail 2007 binary
and not the FAF client build.

## The x87 simulator: `tools/x87sim`

When a row disagrees over a rounding difference, the retail's own bytes can be
run outside the retail process. `tools/x87sim` - in the parent repository, at
`../../../tools/x87sim` from this directory - is a Python interpreter for the
x87, built for exactly that: it models the engine's `_PC_24` environment, so a
transcription of retail's code can be checked bit for bit against the hardware
with the retail out of the loop.

| piece | role |
|---|---|
| `x87sim/` | the simulator: decode, the register file, memory, exact-rational arithmetic |
| `hwprobe/x87ref32.c` | the hardware reference: runs one encoding or a byte blob on a real x87 and dumps the registers |
| `hwprobe/build.py` | builds it as x86 through the VS 2022 environment on `G:` |
| `tests/test_instructions.py` | the simulator's own tests: every encoding, against the hardware reference |

There is no install step. Import it with a path relative to your own file:

    import os, sys
    sys.path.insert(0, os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tools', 'x87sim')))
    from x87sim import run_function

(`../../../tools/x87sim` from this directory, `../../../../tools/x87sim` from
`src/`.)

Two things to carry over from "The swept rows, closed" at the end of this file:

- **the rows run under `_PC_24`** (`fninit; fldcw 0x007F`; see `--cw` above),
  so the simulator's `SIMPREC=24` is the model that matches them, not `53`.
  The 53-bit reading this file once recorded came from a probe that measured
  the retail in a different control word than the row used.
- **the tool holds no retail bytes and no corpus.** Comparing the simulator
  against retail's own `GetSquared` is a *use* of the simulator, and it belongs
  to the caller - here, in a scratch directory outside the tool.

An instruction the simulator does not implement raises
`Stop('unhandled x87 %02X %02X')`. Measure that one encoding with
`hwprobe/build/x87ref32.exe` (build it with `hwprobe/build.py` first): six
known values in, execute, dump the top four registers - a form that pops one
shifts the window by one, two shifts it by two - then write the measured
semantics into `x87sim/decode.py`, and add a matching instruction-level test.

## Reading the retail image with Ghidra

Retail addresses come from disassembly, so the image gets imported into a Ghidra
project once and then queried repeatedly. Ghidra 12.1.4 is at
`G:\soft\ghidra_12.1.4_PUBLIC`.

**The project cannot live under `.tmp/`.** Ghidra rejects any path element that
starts with `.`:

    ERROR Abort due to Headless analyzer error: Path element starting with '.'
    is not permitted (HeadlessAnalyzer) java.lang.IllegalArgumentException

so the project sits in the gitignored `ghwork/` at the repository root instead.
The directory also has to exist before `analyzeHeadless` is called — it will not
create it (`Directory not found: ...\ghwork\proj`).

One-time import, `.tmp/gh/import_base.bat`:

    call "G:\soft\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" ^
      "D:\D\BC\2026\faf-re\ghwork\proj" "base" ^
      -import "C:\ProgramData\FAForever\bin\ForgedAlliance_6c6c92f.exe" ^
      -overwrite -scriptPath "D:\D\BC\2026\faf-re\.tmp\gh"

Afterwards, dump a function with `-noanalysis` against the existing project:

    call "G:\soft\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" ^
      "D:\D\BC\2026\faf-re\ghwork\proj" "base" ^
      -process ForgedAlliance_6c6c92f.exe -noanalysis -readOnly ^
      -scriptPath "D:\D\BC\2026\faf-re\.tmp\gh" ^
      -postScript DumpFunc.java 0x00452AF0 0x0044F880

`-readOnly` lets several lookups share one project; if the Ghidra build does not
accept it, copy `ghwork/proj` aside per user instead — Ghidra locks a project
against concurrent writers.

The three `bin/*.exe` files are not interchangeable:

| file | what it is | usable here |
|---|---|---|
| `ForgedAlliance_6c6c92f.exe` | retail + FAF patch set `6c6c92f` | **yes** — the baseline |
| `ForgedAlliance_base.exe` | unpatched retail | only to prove a patch did not touch bytes |
| `ForgedAlliance.exe` | FAF client build `8da86fe` #143 | **no** — not what this rebuild targets |

## What is in scope

**A function is in scope when it can be located in the retail binary and
disassembly analysis establishes that it is the corresponding function serving
the same role.** Those two conditions are the whole test.

How the address was found is not part of it. A direct `CALL` operand read out
of an anchored retail function is one way in; a vtable slot is another.
`IntrLine3Box3<float>::Test` (`0x00A4FE60`) was reached through vtable slot
+0x08 rather than a `CALL`, and it is in scope: the address exists in the retail
image, and the body — a three-axis slab test against the box's extent and centre
and the line's origin and direction — is that function. What disqualifies a
candidate is not the route to the address but a disassembly reading that says it
is something else, which is how `Moho::MultQuadVec` and friends were caught and
removed.

What keeps a function *out* is failing the first condition: it cannot be located
in the retail binary at all. `Quaternion<float>::Rotate` has rebuild call sites,
so it is reachable, but no retail address could be found to anchor a walk from —
its rebuild callers are a debug helper and a `VTransform::ApplyTransform` with
no callers. Reusing `0x00452D40` for it would fail the second condition outright:
that address is `Moho::MultQuadVec`, as the engine-helper audit established.

A rebuild that never calls a function does not emit it, so such a function has no
out-of-line retail counterpart either and there is nothing to compare. That is a
statement about both binaries rather than about annotation coverage.

**The table carries only the functions that are in scope and located.** A row
with a zero address is not a row: it can only ever report a mismatch against
the canary, so it says nothing and hides the rows that do. The candidates that
are reachable but not yet located are therefore not in the table at all - they
are in `reachable.md` (generated from the reachability analysis, and the work
list) and in the per-class notes below. The groups that are known to be out of
scope today:

| group | why |
|---|---|
| the Vector2 block (`Vec2::Dot`, `LengthSq`, `Length`, `Perp`, `DotPerp`) | no rebuild call site: the engine uses its own 2D code |
| most of `Math<float>` | the same, and several of them are engine-side CRT wrappers (`Math::Ceil`/`Floor`/`FMod` are `0x00453A00` / `0x0044F720` / `0x00422920`) |
| the Quaternion block (`Quat::LengthSq`, `SquaredLength`, `Dot`, `Conjugate`, `Multiply`, `FromAxisAngle`) | no rebuild call site; and `Quat::LengthSq`/`Dot` are FAF-MOD additions, not upstream WM 3.8 symbols |
| `Vec3::Zero`, `NaN`, `Add`, `Sub`, `Scale`, `Length`, `DistanceSq3D`, `DistanceSqXZ` | inlined at their call sites in the retail, so there is no out-of-line body to compare |
| the `Intersector` base's default `Test`/`Find` | the base's bodies are stubs in both binaries: the rebuild's are `assert(false); return false;`, which under `NDEBUG` is a constant `false`, and the retail's base slots point at the same two-byte stubs (`xor al,al; ret 0xC` at `0x00A45510` / `0x00A45530`). There is nothing in the comparison to distinguish them |

Two claims that were on the list are **not** closed by this, and are left
open rather than asserted:

* The trivial value classes' constructors and `operator=` are *not* out of scope
  on the strength of a guess. The retail does emit out-of-line constructors
  (`IntrBox2Circle2`'s at `0x00A4E5F0`, for one), so "inlined" has to be shown
  per class before the group can be closed - and it does not hold in general:
  `Vector3<float>::operator=` has a body at `0x00428D30`, `Box3f`'s
  copy-assignment at `0x004FFDF0`, and the FAF `Segment3f` constructor at
  `0x004FE130`. Those three are *engine-side* bodies (the engine's own
  compilation of header code), so they are not the Wild Magic library's to
  compare - the rebuild's `Wm3Box3` has no `operator=` at all, so the retail's
  copy-assignment has no counterpart either. `Sphere3f`'s constructor, by
  contrast, is inlined at `0x0067AD41`.
* `Distance::SetDifferenceStep` and `GetDifferenceStep` were listed here on the
  ground that they return no geometric result. **That reason does not hold the
  group any more:** the retail emits out-of-line bodies for both
  (`0x00A391D0` and `0x00A39200` for `<float,Vector3f>`, plus
  `0x00A38BB0`/`0x00A38BE0`, `0x00A397C0`/`0x00A397F0` and
  `0x00A39DB0`/`0x00A39DE0` for the other three instantiations), so they
  satisfy the scope test and are now **rows** - see the section on the
  difference step below, where they are measured to agree with the rebuild.

## Locating a retail address

The anchor-and-walk procedure, normalised. Pick the target function (call it
**A**) and walk:

1. **Is A called from the rebuild?** Grep `src/sdk` for a call to A. The engine
   sources are heavily annotated — thousands of doc comments of the form
   `Address: 0x007672E0 (FUN_007672E0)` — so the *caller* R's retail entry point
   is usually already written down in R's own comment block. `WildMagic3p8/`
   itself carries almost none, which is why the walk starts in `src/sdk`.

2. **Anchor on R.** Open R's retail address in Ghidra
   (`analyzeHeadless` + `.tmp/gh/DumpFunc.java <address>`) and read R's
   disassembly. Put R's rebuilt source next to it.

3. **Match R, then find A inside it.** Line up R's rebuilt source against R's
   retail disassembly until they correspond instruction for instruction. A is
   called from R, so it shows up as a `CALL` at the point where the source makes
   the call. That `CALL`'s operand is **A's retail address**.

4. **Confirm A's signature before writing the thunk.** Dump A
   (`DumpFunc.java <A>`) and read off how it takes arguments and where it
   returns. Step 5 depends on this, and getting it wrong produces a target that
   mismatches on every iteration for reasons that have nothing to do with the
   code.

5. **Write the thunk.** In the class's `*_thunks.cpp`, give A a naked function
   that **keeps a plain C signature and marshals into A's real ABI inside the
   asm** — the same shape as `tools/difffuzz/src/retail_thunks.cpp`. The retail
   PE is already mapped at `0x00400000` with its imports trapped, so the thunk
   calls into the mapping; it does **not** inline A's machine code.

       __declspec(naked) float __cdecl Sqrtf(float)   // -> 0x00452FC0
       {
         __asm {
           push ebx / push esi / push edi / push ebp
           push dword ptr [esp + 20]        // stdcall stack argument
           mov eax, 0x00452FC0
           call eax
           add esp, 4
           pop ebp / pop edi / pop esi / pop ebx
           ret
         }
       }

   The three marshalling forms that cover this table:

   | how retail takes it | thunk asm |
   |---|---|
   | `this` in ECX | `mov ecx, [esp + 20]` |
   | x87 stack operand | `fld qword ptr [esp + 20]` |
   | stdcall stack argument | `push dword ptr [esp + 20]` |

   Save/restore the callee-saved registers around the call: the retail code was
   compiled expecting a normal frame and we are jumping into it from ours.

6. **Record the address** in that row's `va` in the slice's `*_targets.cpp`.

7. **Verify the landing.** Run with a small `--iters`. A live thunk replaces the
   canary with a real value, so a row still reporting `CDCDCDCD` means the call
   did not land where it was supposed to.

### If A is not called from the rebuild

Go one level up **inside WildMagic3p8**: find a Wild Magic function **B** that
calls A, then apply steps 1–7 to B. If B is not called from the rebuild either,
repeat upwards. This is why the leaf-function-only target table is useful: a
leaf has no Wild Magic callers left, so the walk is guaranteed to terminate at
an engine function — one that is annotated and therefore anchorable.

### If the rebuild never calls A, directly or indirectly

Stop; do not look for an address. A function with no rebuild call site — direct
or transitive — is not emitted by the rebuild and cannot have an out-of-line
counterpart in retail. There is nothing to compare and no reason to wire it.
This is stronger than "the walk has no anchor": it is a statement about both
binaries, not just about the annotation coverage.

The transitive part matters. `Quaternion<float>::FromRotationMatrix` is called
only from `MergeBoxes` in `Wm3ContBox3.cpp`, and `MergeBoxes` has no callers in
the rebuild, so `FromRotationMatrix` is unreachable too. `Quaternion::Slerp` has
no call site at all. Both are correctly outside the table.

Do not substitute a same-named engine helper such as `Moho::MatrixToQuat`
(0x004EB3F0) or `Moho::SLERP` (0x004EBC00); those are different functions with
different signatures and algorithms, not the retail bodies of the Wild Magic
templates.

### Provenance audit (required for every wired row)

An address is not a match because the function has the same name or performs the
same operation. Before a row is wired, it must pass all four checks:

1. **Reachability.** The rebuild calls the WildMagic function, directly or
   transitively. If not, stop: no call site means no emitted body (see the
   previous section).
2. **Anchor.** The address is a direct `CALL` target from a retail function that
   can be tied to a rebuild call site, established by the procedure above. A
   candidate from `tools/difffuzz`'s `retail_thunks` catalog, a same-named engine
   helper, or a shape scan is not an anchor by itself.
3. **ABI and body.** The retail function's register/stack convention and
   instruction shape match the WildMagic function (thiscall/ECX, hidden return
   pointer, ST(0) result, ...). If the same address is an engine helper with a
   different signature or convention (`Moho::MultQuadVec`,
   `Moho::QuatToMatrix`, `NormalizeQuatInPlace`, `QuatToAxisAndAngle`,
   `VecLimitLengthTo`, ...), it is not the template body.
4. **Xrefs.** `getReferencesTo(address)` has at least one code reference, and the
   caller is consistent with the call site that justified step 2. Zero xrefs
   means the function is dead/unreferenced, not a retail leaf.

Record the evidence (caller, call instruction, ABI, xrefs) in the row comment or
the commit. A row that fails any check is removed, not kept as a "cross-check".
The 2026-10 audit used this checklist to remove `Quat::Rotate`,
`Quat::ToRotationMatrix`, `Quat::Normalize`, `Quat::ToAxisAngle`,
`Vec3::LimitLengthTo`, `Math::Ceil`, `Math::Floor` and `Math::FMod`.

### When the retail ABI and the Wild Magic API disagree

They routinely do, and no thunk can bridge it — the retail function is a
different function, not a differently-called one. Two cases to watch for:

- **Return values retail does not produce.** Retail
  `Vector3<float>::Normalize` (`0x00452AF0`) is `void` — its `FST` writes the
  *length* to `[esp+0x8]`, and the `DIVSS XMM0,[esp+0x4]` that follows computes
  `1.0f/length` purely as a scaling factor for three `MULSS`; nothing is left in
  `EAX` or `ST(0)`. So the target must **not** compare a return value. Comparing
  one anyway is what made `difffuzz`'s `Wm3Normalize` row fail: measured over
  20000 iterations, 400/400 reported mismatches were solely in the `len` word,
  and the normalised vector itself never differed once.
- **Parameters retail hard-codes.** The same function's `1e-6f` threshold is a
  constant loaded from `.rdata` (`0x00d71bfc`), not an argument. Feeding a
  random `eps` on our side would report a divergence that is really just a
  signature difference — fix `eps` to the constant the retail build baked in.

## Current state

Thirty-nine rows are wired up and passing bit-for-bit at 1,000,000/1,000,000
on every seed tried (`0x5EED2007`, `0xDEADBEEF`, `0x12345678`, `0xC0FFEE01`,
`0xA5A5A5A5`). Four more were added afterwards - `IntrLine3Box3::Find`,
`DistVector2Box2::Get`, `IntrSegment3Capsule3::GetPoint` and
`IntrSegment3Capsule3::GetParameter` - then three more (the two `Distance`
derivative rows and `IntrSegment3Box3::Test`), then `InBox`, then the two
swept distance rows, then `ContAlignedBox`, then the two
`IntrBox3Sphere3` intersection helpers, then the four `IntrBox3Sphere3`
region helpers, and then the masked form of `ContAlignedBox`, and then the
2D `ContAlignedBox` with and without its mask and the two `IntrBox2Circle2`
region helpers, then the six swept `Distance` rows and the two
`IntrLine3Box3` helpers and the base's two window queries, and then the
difference-step pair, and then the four `DistLine3Segment3` rows, and then the
line-capsule `Test`, the hull construction and the `Query3` predicate, and
then the rest of the `Query` family and the two hull siblings. The
table now holds **152** rows, all of them passing 1,000,000/1,000,000 on every
seed tried. The hundred-and-twelve are written up under
"Four more rows: the box-line Find, the box2 distance and the capsule
accessors", "Three more rows: the distance derivatives and the segment-box
Test", "`InBox`, and the harness's own build flags", "The swept distance
rows", "`ContAlignedBox`: the axis, and the fourth argument",
"`IntrBox3Sphere3::GetVertexIntersection` and `GetEdgeIntersection`",
"`IntrBox3Sphere3`'s four region helpers", "`ContAlignedBox` takes a fourth
argument, and has a masked path", "The 2D `ContAlignedBox` and the
`IntrBox2Circle2` region helpers", "The ten rows of the swept `Distance`
queries, the two `IntrLine3Box3` helpers, and the base's window queries", "The
denormal range in the 2D box and the segment distance", "The difference
step: the retail agrees with both upstream texts", "The denormal range in
the Vector3 segment distance, and the line-segment distance" and "Three more
rows: the line-capsule Test, the hull construction and the Query3 predicate"
and "Thirteen more rows: the rest of the Query family and the two hull
siblings", "The double instantiation: the x87, not binary64" and "The double
instantiation, part two: the segment class, and a swept body that says zero"
at the end of this file.
    [PASS] Vec3::Dot          0x00452AD0
    [PASS] Vec3::Dot(instance) 0x00452AD0
    [PASS] Vec3::Cross        0x00452BC0
    [PASS] Vec3::Cross(instance) 0x00452BC0
    [PASS] Vec3::LengthSq     0x004757E0
    [PASS] Vec3::SquaredLength 0x004757E0
    [PASS] Vec3::Normalize    0x00452AF0
    [PASS] Vec3::Normalize(instance) 0x00452AF0
    [PASS] Vec3::SqrtfBinary  0x00452FC0
    [PASS] Math::FAbs         0x0040DAB0
    [PASS] Math::InvSqrt      0x0050D150
    [PASS] Math::Sqrt         0x00452FC0
    [PASS] Aabb3::TestIntersection 0x00506010
    [PASS] Box3::ComputeVertices 0x004FFF00
    [PASS] DistVector3Box3::GetSquared 0x00A45C00
    [PASS] DistVector3Box3::Get 0x00A457E0
    [PASS] DistVector3Segment3::GetSquared 0x00A484F0
    [PASS] DistVector3Segment3::Get 0x00A480E0
    [PASS] DistVector3Segment3::GetClosestPoint1 0x00A39230
    [PASS] IntrBox2Circle2::Test   0x00A4E620
    [PASS] IntrBox2Circle2::Find   0x00A4F580
    [PASS] IntrLine3Box3::Test     0x00A4FE60
    [PASS] IntrLine3Box3::Find     0x00A508F0
    [PASS] IntrSegment3Box3::Find  0x00A462A0
    [PASS] IntrSegment3Box3::GetPoint 0x00A46300
    [PASS] IntrBox3Sphere3::Test   0x00A41560
    [PASS] IntrBox3Sphere3::Find   0x00A43420
    [PASS] IntrBox3Sphere3::GetContactPoint 0x00A414C0
    [PASS] IntrSegment3Sphere3::Find 0x00A471C0
    [PASS] IntrSegment3Sphere3::GetPoint 0x00A46940
    [PASS] IntrSegment3Sphere3::Test 0x00A46C80
    [PASS] IntrSegment3Sphere3::Test(swept) 0x00A46B10
    [PASS] IntrSegment3Sphere3::Find(swept) 0x00A46DB0
    [PASS] IntrBox2Box2::Test     0x00A47D40
    [PASS] DistSegment3Segment3::GetSquared 0x00A7E660
    [PASS] DistSegment3Segment3::Get 0x00A7E230
    [PASS] DistVector2Box2::GetSquared 0x00A6C9D0
    [PASS] DistVector2Box2::Get   0x00A6C680
    [PASS] IntrSegment3Capsule3::Test 0x00A6BEB0
    [PASS] IntrSegment3Capsule3::Find 0x00A6C070
    [PASS] IntrSegment3Capsule3::GetPoint 0x00A6BDF0
    [PASS] IntrSegment3Capsule3::GetParameter 0x00A6BE00
    [PASS] IntrLine3Capsule3::Find 0x00A7F920
    [PASS] DistVector3Segment3::GetDerivative 0x00A39110
    [PASS] DistVector3Segment3::GetDerivativeSquared 0x00A39180
    [PASS] IntrSegment3Box3::Test 0x00A46440
    [PASS] InBox 0x00A3BC10
    [PASS] DistVector3Segment3::Get(swept) 0x00A48190
    [PASS] DistVector3Segment3::GetSquared(swept) 0x00A48340
    [PASS] ContAlignedBox 0x00A3AE40
    [PASS] IntrBox3Sphere3::GetVertexIntersection 0x00A41740
    [PASS] IntrBox3Sphere3::GetEdgeIntersection 0x00A41800
    [PASS] IntrBox3Sphere3::FindFaceRegionIntersection 0x00A41880
    [PASS] IntrBox3Sphere3::FindJustEdgeIntersection 0x00A41CC0
    [PASS] IntrBox3Sphere3::FindEdgeRegionIntersection 0x00A41F50
    [PASS] IntrBox3Sphere3::FindVertexRegionIntersection 0x00A421E0
    [PASS] ContAlignedBox(mask) 0x00A3AE40
    [PASS] ContAlignedBox2 0x00A685F0
    [PASS] ContAlignedBox2(mask) 0x00A685F0
    [PASS] IntrBox2Circle2::TestVertexRegion 0x00A4E6C0
    [PASS] IntrBox2Circle2::TestEdgeRegion 0x00A4EB60
    [PASS] DistVector3Box3::Get(swept) 0x00A458C0
    [PASS] DistVector3Box3::GetSquared(swept) 0x00A45A60
    [PASS] DistVector2Box2::Get(swept) 0x00A6C730
    [PASS] DistVector2Box2::GetSquared(swept) 0x00A6C880
    [PASS] DistSegment3Segment3::Get(swept) 0x00A7E2A0
    [PASS] DistSegment3Segment3::GetSquared(swept) 0x00A7E480
    [PASS] IntrLine3Box3::DoClipping 0x00A50220
    [PASS] IntrLine3Box3::Clip 0x00A4FCC0
    [PASS] Distance::Get(fTMin,fTMax,v0,v1) 0x00A38C50
    [PASS] Distance::GetSquared(fTMin,fTMax,v0,v1) 0x00A38EB0
    [PASS] Distance::SetDifferenceStep 0x00A391D0
    [PASS] Distance::DifferenceStep (default) 0x00A39F20
    [PASS] DistLine3Segment3::GetSquared 0x00A80DE0
    [PASS] DistLine3Segment3::Get 0x00A809A0
    [PASS] DistLine3Segment3::GetSquared(swept) 0x00A80C10
    [PASS] DistLine3Segment3::Get(swept) 0x00A80A40
    [PASS] IntrLine3Capsule3::Test 0x00A7F760
    [PASS] ConvexHull3<float>::ConvexHull3 0x00A67120
    [PASS] Query3<float>::ToPlane 0x00A55CE0
    [PASS] Query3<float>::ToPlane(index) 0x00A530A0
    [PASS] Query3<float>::ToTetrahedron 0x00A530D0
    [PASS] Query3<float>::ToTetrahedron(index) 0x00A53180
    [PASS] Query3<float>::ToCircumsphere 0x00A55DF0
    [PASS] Query3<float>::ToCircumsphere(index) 0x00A531B0
    [PASS] Query2<float>::ToLine 0x00A75C00
    [PASS] Query2<float>::ToLine(index) 0x00A74E70
    [PASS] Query2<float>::ToTriangle 0x00A74EA0
    [PASS] Query2<float>::ToTriangle(index) 0x00A74F30
    [PASS] Query2<float>::ToCircumcircle 0x00A75C80
    [PASS] Query2<float>::ToCircumcircle(index) 0x00A74F60
    [PASS] ConvexHull1<float>::ConvexHull1 0x00A74560
    [PASS] ConvexHull2<float>::ConvexHull2 0x00A78E30
    [PASS] Distance<double>::SetDifferenceStep 0x00A39DB0
    [PASS] Distance<double>::DifferenceStep (default) 0x00A39FA0
    [PASS] Distance<double,Vector2>::SetDifferenceStep 0x00A397C0
    [PASS] Distance<double,Vector2>::DifferenceStep (default) 0x00A39F60
    [PASS] DistVector3Box3<double>::GetSquared 0x00A460D0
    [PASS] DistVector3Box3<double>::Get 0x00A45800
    [PASS] DistVector3Segment3<double>::GetSquared 0x00A48910
    [PASS] DistVector3Segment3<double>::Get 0x00A48100
    [PASS] DistVector3Segment3<double>::GetSquared(swept) 0x00A487E0
    [PASS] DistVector3Segment3<double>::Get(swept) 0x00A486B0
    [PASS] DistVector3Box3<double>::GetSquared(swept) 0x00A45FB0
    [PASS] DistVector3Box3<double>::Get(swept) 0x00A45E90
    [PASS] DistVector2Box2<double>::GetSquared 0x00A6CD70
    [PASS] DistVector2Box2<double>::Get 0x00A6C6A0
    [PASS] DistVector2Box2<double>::GetSquared(swept) 0x00A6CC60
    [PASS] DistVector2Box2<double>::Get(swept) 0x00A6CB50
    [PASS] DistLine3Segment3<double>::GetSquared 0x00A81330
    [PASS] DistLine3Segment3<double>::Get 0x00A809C0
    [PASS] DistLine3Segment3<double>::GetSquared(swept) 0x00A811F0
    [PASS] DistLine3Segment3<double>::Get(swept) 0x00A810B0
    [PASS] IntrBox2Circle2<double>::Test 0x00A4EE50
    [PASS] IntrSegment3Capsule3<double>::Test 0x00A6BF80
    [PASS] IntrBox2Box2<double>::Test 0x00A47F00
    [PASS] IntrBox3Sphere3<double>::Test 0x00A425C0
    [PASS] IntrLine3Box3<double>::Test 0x00A50090
    [PASS] IntrSegment3Box3<double>::Test 0x00A46720
    [PASS] IntrSegment3Sphere3<double>::Test 0x00A475E0
    [PASS] Distance<double,Vector3d>::Get(fTMin,fTMax,v0,v1) 0x00A39860
    [PASS] Distance<double,Vector3d>::GetSquared(fTMin,fTMax,v0,v1) 0x00A39AB0
    [PASS] Distance<double,Vector3d>::GetDerivative 0x00A39D00
    [PASS] Distance<double,Vector3d>::GetDerivativeSquared 0x00A39D60
    [PASS] Distance<double,Vector2d>::Get(fTMin,fTMax,v0,v1) 0x00A39270
    [PASS] Distance<double,Vector2d>::GetSquared(fTMin,fTMax,v0,v1) 0x00A394C0
    [PASS] Distance<double,Vector2d>::GetDerivative 0x00A39710
    [PASS] Distance<double,Vector2d>::GetDerivativeSquared 0x00A39770
    [PASS] DistSegment3Segment3<double>::GetSquared 0x00A7F100
    [PASS] DistSegment3Segment3<double>::Get 0x00A7E250
    [PASS] DistSegment3Segment3<double>::GetSquared(swept) 0x00A7EFB0
    [PASS] DistSegment3Segment3<double>::Get(swept) 0x00A7EE60
    [PASS] IntrLine3Box3<double>::Find 0x00A50940
    [PASS] IntrSegment3Box3<double>::Find 0x00A46330
    [PASS] IntrSegment3Sphere3<double>::Find 0x00A479B0
    [PASS] Vector3<double>::GenerateOrthonormalBasis 0x00A3D150
    [PASS] Vector3<float>::GenerateOrthonormalBasis 0x00A3CF80
    [PASS] Vector3<double>::Normalize 0x00A3D0A0
    [PASS] IntrSegment3Capsule3<double>::Find 0x00A6C3C0
    [PASS] IntrSegment3Sphere3<double>::Find(swept) 0x00A47690
    [PASS] Eigen<float>::IncrSortEigenStuff3 0x00A70DB0
    [PASS] Eigen<float>::IncrSortEigenStuff2 0x00A70D70
    [PASS] Eigen<float>::GetEigenvalue 0x00A6CE70
    [PASS] Eigen<float>::GetEigenvector 0x00A6D350
    [PASS] Eigen<float>::GetEigenvector(Vector2) 0x00A6D3A0
    [PASS] Eigen<double>::IncrSortEigenStuff3 0x00A71460
    [PASS] Eigen<double>::IncrSortEigenStuff2 0x00A71420
    [PASS] Eigen<double>::GetEigenvalue 0x00A6CEA0
    [PASS] Eigen<double>::GetEigenvector 0x00A6EE40
    [PASS] Eigen<double>::GetEigenvector(Vector2) 0x00A6EE90
    [PASS] Vector3<float>::Length 0x00452A90

The table has no failing row left. The two swept `IntrSegment3Sphere3` rows
pass too; their own section below says how they were closed.

The `IntrBox3Sphere3::GetContactPoint` row was the last one that failed for
a reason of its own, and it turned out to be two separate things: a row that
compared uninitialised memory, and a real arithmetic difference in the
rebuilt source (the square root in the two region helpers). Both are written
up in "The last row: `IntrBox3Sphere3::GetContactPoint`" below.

The three rows after it are written up in "Two more classes: the segment-sphere
and the box-box" below.

The fixes are in the rebuilt source (`Wm3Vector3.h`, `Wm3Vector3.inl`,
`Wm3Math.inl`, `Wm3Quaternion.inl`, `Wm3IntrLine3Box3.cpp`,
`Wm3IntrSegment3Sphere3.cpp`, `Wm3IntrLine3Capsule3.cpp`,
`Wm3DistLine3Segment3.cpp`, `Wm3DistVector3Box3.cpp` and
`Wm3DistVector3Segment3.cpp`), not in how either side is compiled.

The wired set is exactly the WildMagic leaves that are both reachable from the
rebuild and emitted out-of-line in retail. Everything else falls into one of
three groups:

- **Inlined at a reachable call site.** Verified in the retail disassembly:
  0x00491760 inlines `Vec3::Add`/`Sub`/`Scale`/`Length`;
  0x00596930 inlines `Vec3::DistanceSq3D`; 0x005D2480 inlines
  `Vec3::DistanceSqXZ`; 0x006B92E0 inlines `Quat::LengthSq`.
- **No rebuild call site, directly or transitively.** The no-caller rule puts
  these outside the comparison: the Vec2 block, `Quat::SquaredLength`/`Dot`/
  `Conjugate`/`Multiply`/`FromAxisAngle`, most of `Math`, and
  `Quat::Slerp`/`FromRotationMatrix`. A function the rebuild never calls is not
  emitted on either side.
- **The address looked like the function but is an engine helper.** The
  2026-10 provenance audit (see its four checks above) removed `Quat::Rotate` (0x00452D40 is
  `Moho::MultQuadVec`, 60+ engine callers and a custom EBX/ESI/ECX convention),
  `Quat::ToRotationMatrix` (0x00452FD0 is `Moho::QuatToMatrix`),
  `Quat::Normalize` (0x004EDAA0 is `NormalizeQuatInPlace`), `Quat::ToAxisAngle`
  (0x006C1070 is `QuatToAxisAndAngle`), `Vec3::LimitLengthTo` (0x005D1E70 is
  `VecLimitLengthTo`; the WM static helper has no caller), and `Math::Ceil`/
  `Floor`/`FMod` (0x00453A00 / 0x0044F720 / 0x00422920 are engine-side CRT
  wrappers; the WM functions have no caller). Their xrefs and register
  conventions are in commit `591aca7`'s message.

The nine Vec3 rows above are the retail instance bodies (`ECX = this`, ST(0) or
a hidden return pointer); the instance rows compare the `.inl` implementation
against the same bodies. `Math::FAbs`/`InvSqrt`/`Sqrt` are the out-of-line
`Math<float>` leaves reached from Wild Magic code (`InBox` uses FAbs,
`Normalize` uses Sqrt, `ToAxisAngle`/Normalize use InvSqrt).

### The geometry slice: Wm3Box3 and Wm3AxisAlignedBox3

The Vector/Math/Quaternion families turned out to be exhausted — every remaining
row is inlined, uncalled, or points at an engine helper. The way to find more
work is to look at the **other** 49 headers under `Foundation/Math`, and apply
the no-rebuild-caller rule to them rather than assuming they are out of scope.

Most are. `Box3f` appears 173 times in `src/sdk` and `Sphere3f` 125, but the
engine uses them as *types* — constructors, member access, template arguments —
and computes with its own collision code, so their Wild Magic methods have no
call site. Two do not:

| row | retail | provenance |
|---|---|---|
| `AxisAlignedBox3::TestIntersection` | `0x00506010` | address already written down in the engine source: `src/sdk/moho/mesh/Mesh.cpp:489` (`Address: 0x00506010 (FUN_00506010, Wm3::AxisAlignedBox3<float>::TestIntersection)`), called from `Mesh.cpp:502,513,1370` |
| `Box3::ComputeVertices` | `0x004FFF00` | `CALL` read out of `moho::AxisAlignedBoundsFromBox` (`0x004FE2D0`), the caller annotated at `Wm3AxisAlignedBox3FafExtras.cpp:22`; independently corroborated by a second `CALL` in `func_DrawSelectionBrackets` (`0x007FC820`), whose rebuild side is `SelectionBracketRenderer.cpp:299` |

`Box3::GetCorners` (`CD3DPrimBatcher.cpp:1286`) is not a separate row: it is a
one-line forward to `ComputeVertices` (`Wm3Box3.h:65`), so it is a second caller
of the same body, not a second function.

Both ABIs are worth recording because neither is the usual thiscall:

- `TestIntersection` takes **`this` in EDX and the argument in EDI**, not ECX,
  and returns the bool in AL with no stack arguments.
- `ComputeVertices` is a normal thiscall — `ECX = this`, the `Vector3[8]` output
  array as the single stack argument, `RET 4` — but **void**: EAX is read as the
  output pointer on entry and is a leftover on exit, so no return value may be
  compared.

`ComputeVertices` passing 200000/200000 matters more than its size suggests. It
is 24 chained `Vector3` adds and subtracts (`Center ± e0 ± e1 ± e2` in eight
combinations), so it is the most direct test yet of chained vector arithmetic:
the eight sign combinations, their order and their rounding all agree.

### The collision slice: Foundation/Intersection and Foundation/Distance

`Foundation/Math` was not the whole of WildMagic. `Foundation` has 27
subdirectories and about a thousand files, and the engine calls into
`Intersection/` and `Distance/` as well — from `CColPrimitiveBase.cpp`, the same
file that anchored the first wired rows. Finding them needed a **type-associated**
scan, not a name count: pair each `recv.Method(` with the `Wm3::` type its
receiver was declared as in the same file. Counting method names across
`src/sdk` returns the same number for `Normalize` in half a dozen classes,
because it is counting the same handful of unrelated engine calls repeatedly.

| row | retail | provenance |
|---|---|---|
| `DistVector3Box3<float>::GetSquared` | `0x00A45C00` | `CALL` in `Moho::CColPrimitive_Box::CollideSphere` (`0x004FF150`, annotated at `CColPrimitiveBase.cpp:420`); a `double` instantiation at `0x00A460D0` corroborates |

These objects are built by construction and then queried, and they hold
references to caller-owned data, so the retail **constructor** (`0x00A45730`)
is driven too rather than the object's layout being fabricated by hand.

#### Retail's inlined dot products do not share one summation order

This is the finding worth keeping. `Vector3::Dot` had to be written z-first to
match retail's standalone `Dot` at `0x00452AD0`. Reusing that `Dot` inside the
two WildMagic functions that contain their own copies disagreed with retail in
**both**:

| consumer | with our shared `Dot` | dot spelled out x-first |
|---|---|---|
| `DistVector3Box3<float>::GetSquared` | 20947 / 200000 | 48 |
| `DistVector3Segment3<float>::GetSquared` | 7281 / 200000 | **0 / 200000** |

Both use the same `_PC_24` emulation, only x-first. In the Segment3 case the
difference is much worse than a last-bit wobble, because the dot product's
result gates which of the three closest-point branches is taken — a 1 ULP
difference in the parameter selects a different branch and moves the answer.

So one summation order cannot be right for retail: the compiler picks one per
call site, and the standalone body and the inlined uses genuinely differ. That is
an argument against tuning the one shared `Dot` to satisfy more call sites — each
new consumer may want the other order, and the right move is to write that
consumer's arithmetic in the order its own retail body uses.

The 48 remaining on `DistVector3Box3` were not retail's exponent width: the
emulation itself stored each product into a `Real` (float) slot, so a product
above `FLT_MAX` became an infinity before the x87-style sum. Keeping px/py/pz in
`double` and storing only the summed 24-bit value to `Real` took the row from 48
to 0. `DistVector3Segment3::GetSquared` had the same bug, visible on one 1M
input where retail returned `-nan(ind)` and ours `+inf`.

### What the collision slice added

Five rows, all passing 1M/1M: `DistVector3Box3::GetSquared`,
`DistVector3Segment3::GetSquared`, `IntrBox2Circle2::Test`,
`IntrLine3Box3::Test` and `IntrSegment3Box3::Find`.

Three of them exposed real emulation defects rather than retail surprises:

- `DistVector3Box3::GetSquared` and `DistVector3Segment3::GetSquared` kept the
  per-lane dot products in `Real` slots. A product above `FLT_MAX` therefore
  became an infinity before the 24-bit sum, where retail's x87 keeps the
  extended exponent until the store. Keeping `px/py/pz` (and `dpx/dpy/dpz`) in
  `double` closed 48/200000 and the one 1M input respectively.
- `IntrLine3Box3::Test` at `0x00A4FE60` is x87. The full per-operation
  emulation is needed: `Sub24` for `kDiff`, `Cross24` for the cross product,
  `Dot24x` for the six x-first dots, and `Mul24`/`Add24` for the RHS, with
  float stores at the points the source assigns to `Real`. That took the row
  from 490 to 0. The earlier experiments were insufficient because they changed
  only the dot order (490 unchanged) or only widened storage (3292/7057):
  retail is x87, but the *store points and the non-dot arithmetic* were what
  the model was missing.
- `IntrSegment3Box3::Find` at `0x00A462A0` tail-calls `DoClipping` at
  `0x00A50220`. The same x87 model applies to `DoClipping`'s six dots and to
  `Clip`'s products/divisions/comparisons; emulating them took 43 to 0.

The dot-order finding from the first two rows still stands: the standalone
`Vector3::Dot` is z-first, while the dots inlined in these functions are
x-first. But order alone does not close an x87 body; the per-operation width
and the float store points have to match too.

`Quaternion<float>::Rotate` stays off the table: it has rebuild call sites, so
it is in scope, but its rebuild callers are a debug helper and a
`VTransform::ApplyTransform` with no callers, and no annotated retail address
could be found to start the walk from. Reporting that as no anchor, rather than
reusing `0x00452D40` — which the engine-helper audit already established is
`Moho::MultQuadVec` — is the only honest answer.

### `Get()` rows: retail takes the CRT sqrt, not the game's

`DistVector3Segment3<float>::Get()` = `0x00A480E0`, identified off the object's
own vtable `0x00D71E6C`:

| slot | address | what it is |
|---|---|---|
| +0x08 | `0x00A48190` | `Get(fT, rkVelocity0, rkVelocity1)` - reads three stack arguments |
| +0x0C | `0x00A480E0` | `Get()` - seven instructions: dispatch, sqrt, narrow, return |
| +0x18 | `0x00A484F0` | `GetSquared()` - the row already passing on every seed |

The vtable was read out of `.rdata` using the file offset the PE section table
gives, not inferred from call sites. Slot +0x0C is the only entry that takes no
arguments, dispatches another slot, and squares its result into a square root -
which is `Get()` as `Wm3DistVector3Segment3.cpp:39` writes it, and which the
slot below it (`GetSquared`) corroborates, since that one is already proven.

Wired, it raised the harness's own import-trap code (`0xE0F1A001`,
`pe_image.cpp:20`) on a handful of draws and stopped the row. The chain is
retail's. `Get()` does not call the game's `Math<Real>::Sqrt`; it calls the
CRT's `0x00A91000`, and that routine branches on the x87 control word:

```
0x00a91028  66 81 3c 24 7f 02      CMP word [ESP], 0x027F   ; the x87 control word
0x00a91030  e8 a0 6b 01 00         CALL 0x00F97BD5          ; error path
0x00a9103e  83 3d ac 7d fb 00 00 00 CMP dword [0x00FB7DAC], 0
0x00a91046  0f 85 13 6c 01 00      JNZ 0x00A47C5F
```

`0x027F` is the Windows default control word - not the engine's `_PC_24` - and
seeing it is what lets the CRT skip that error path. Every crashing draw carries
a NaN or an infinity (`7FC00000`, `7FC00002`, `7F800000`, `80000000`); every
accepted one does not.

The control word was the obvious suspect and the obvious suspect was wrong.
Re-running with `--cw 0x027F` left the crash on the same input at the same
iteration while breaking the two rows that had been passing. So it is not the
control word: it is the non-finite argument reaching an import the harness does
not resolve. `Get()` is simply the first row to go through the CRT sqrt rather
than the game's own.

That bounds the row to finite results. The filter lives in this row's generator,
not behind the global `--skip-extreme`, so the other rows keep full coverage.
Worth carrying to the rows still unwired: **any retail function that
calls the CRT sqrt instead of `Math<Real>::Sqrt` has the same ceiling**, and the
ceiling is on inputs, not on the rebuild's implementation.

### The retail's three-argument `Get`: an earlier account here was wrong twice

This section used to argue that the three-argument `Get` rows could not be run
because the retail aborted, and that the abort was not a pure-virtual call.
Both halves were wrong, and in the same way: the thunks were handing retail
the wrong stack slots, so retail was never running the function under test.

See "The three-argument thunks were pushing the wrong stack slots" below for
the root cause. The addresses and the ABI stated here are right; the thunks
were not.

### difffuzz has no answer to this - and that is the useful part

`tools/difffuzz` is where this harness came from: the same `pe_image.cpp`,
`main.cpp`, `gen.cpp` and `fuzz.h`, and the same idea - call a function in the
retail image and in the recovered C++ with the same inputs and compare the
output words. It was built for the Math leaves; this harness grew the
collision and distance slices.

Its README says, about the loader:

> It never runs the entry point, TLS callbacks or CRT init, and it never
> resolves imports. Every IAT slot points at a stub that prints "unresolved
> import DLL!name (called from retail 0x...)" and raises an SEH exception.

So **difffuzz has the same gap, and no solution to it.** What it does have is
the method, stated in the same file:

> Retail state the CRT would have set. ... `kRetailStaticInits` in `main.cpp`
> lists pure C-init entries that are run once under the default CW 0x027F.
> ... **When a target reads a BSS global, find its writer and add it there.**

That is the rule this section follows, applied to a whole CRT instead of one
global. The difference is that the retail's CRT reads a *lot* of globals, and
they are read on the error path of every function that takes a square root of
something negative.

### What each step bought, measured

| step | what the retail side did |
|---|---|
| every import trapped | died with the trap code `0xE0F1A001` |
| imports resolved for real, no TLS | died with the trap code again, now on `TlsGetValue` |
| imports resolved, the module's TLS block published | died with `0xC00000FD` - a stack overflow, 500KB of stack, faulting in ntdll |
| the CRT's version cache poked (0x00FB7D50, 0x00FB7D5C) | died with `0xC0000005` - an access violation, no stack involved |
| the CRT's thread index published (0x00F3E49C) and its FLS pointers resolved (0x00FB7E18..24) | the per-thread data initialises; the error path reaches a callback that is still the file's garbage |

The recursion is worth spelling out, because it is the part that looks like a
difference between the rebuild and the retail and is not:

```
0x00A958E3 -> 0x00A83756 (__get_winmajor) -> 0x00A833AC
            -> 0x00A95B9F (__getptd) -> 0x00A95A58 -> 0x00A959C6
            -> 0x00A958E3 -> ...
```

`__getwinmajor` checks the flag at 0x00FB7D50 and, when it is clear, works
the Windows version out again; working it out goes through the CRT's
per-thread data, which asks for the version, which goes back into
`__getwinmajor`. The CRT's own start-up sets that flag, and the harness
pokes it - the version is cached, major version 5, which is what the engine
shipped against.

### Two negative results, and what they rule out

**The five slots are not it.** Pointing all five at identifying stubs - one
stub per slot, each printing its own address - and running the row showed that
**none of the five is called**. The faulting call goes through a pointer that
none of them holds, so the search has to widen past the `.data` slots the CRT
calls indirectly.

**The CRT's own start-up cannot simply be called.** `0x00A95DC4` is the CRT's
dynamic API resolution: it loads KERNEL32.DLL, resolves
`FlsAlloc` / `FlsGetValue` / `FlsSetValue` / `FlsFree` into the slots at
`0x00FB7E18..0x00FB7E24`, allocates the per-thread slot, publishes it at
`0x00F3E49C` and initialises the per-thread data. Calling it from the harness
**segfaults**, and the reason is that the start-up itself reaches the version
path that recurses - the version cache being poked afterwards does not help,
because the start-up asks for the version before the cache is consulted.

So the harness does what the start-up would do for the parts that can be done
from outside, and does not call the start-up:

| what the harness does | what it replaces |
|---|---|
| resolves the imports for real (642 of 646) | the loader's import resolution |
| allocates the module's TLS block and publishes its index | the loader's TLS setup |
| resolves the FLS APIs into `0x00FB7E18..24` | the start-up's API resolution |
| allocates the CRT's per-thread slot and publishes it at `0x00F3E49C` | the start-up's slot allocation |
| pokes the version cache (`0x00FB7D50`, `0x00FB7D5C`) | the start-up's version detection |

What is left is the part of the start-up that cannot be done from outside: the
value of one more callback pointer that the CRT's error path calls.

### The next step, precisely

Find the **writer** of that pointer - the difffuzz rule, applied to one
slot - and give it the value the start-up would. The start-up is the writer,
and it cannot be called, so the writer has to be found *inside* it: the
candidates are the stores into `.data` in the 0x00A95DC4..0x00A96200 range,
and the one that matters is whichever the square root's error path reads.

The practical way to that is to run the start-up under a debugger and watch
where it recurses; the harness cannot do it because the recursion is the
thing being debugged. That is the one piece of this that wants a debugger
rather than a code change.

### What is still missing, and the next step

One callback on the CRT's error path is still the file's garbage. The
candidates are the five `.data` slots the CRT calls indirectly:

| slot | value in the file | called from |
|---|---|---|
| 0x00FB67A4 | 0xFFE0A244 | 0x009F5297, 0x009F52B7 (engine code) |
| 0x00FB896C | 0xFFBFBFBF | 0x00AB54C2 |
| 0x00FB89A0 | 0xFFFDFDFD | 0x00AB2CA8, 0x00AB2F98, 0x00AB383C |
| 0x00FB8C10 | 0xFFE4CFB4 | 0x00AC1A05, 0x00AC1C09 |
| 0x00FB8C2C | 0xFFEFD2B0 | 0x00AC5105, 0x00AC539C |

All five are called with **one stdcall argument** - the call sites push the
argument and there is no `add esp, 4` after, so the callee pops it. They are
**not interchangeable**: pointing all five at a "return zero" stub leaves the
crash in the harness's own image, because at least one of them is not a
handler that returns a value.

So the next step is the difffuzz rule applied to one slot: find which of the
five the square root's error path uses, and give it the value the CRT's
start-up would - the CRT's own default handler, which is in the image.

### The retail's runtime environment: imports and TLS

Two things the Windows loader does for a module with a `.tls` section, and
which a hand-mapped image does not get, turned out to matter.

**Imports are now resolved for real.** The harness used to point every IAT slot
at a trap stub, and 646 of them trapped. The retail's CRT keeps its errno in
thread-local storage and reaches it through `TlsGetValue`; with that import
trapped, the CRT's error path - the one the sqrt takes for a non-positive
argument - ended in the trap code instead of setting errno and returning NaN.
The retail engine has the real kernel32 underneath it, so the harness does too:
642 imports are resolved against the real DLLs now, and 4 that cannot be
resolved keep their trap, which is still how a genuinely missing dependency
gets reported.

**The TLS block is now allocated and its index published**, the way the loader
does it. The image's TLS directory holds absolute addresses rather than the
RVAs the PE specification calls for, so the fields are used as they are: the
raw template is the 8276-byte zero block at RVA `0x00E3C000`, the index
variable is at RVA `0x00BB7DB4`, and the two callbacks are at RVA
`0x009410F8`. The harness allocates the block, `TlsAlloc`s an index (14 here),
publishes it where the module's code reads it, and calls the callbacks with
`DLL_PROCESS_ATTACH` - **after** the sections are made executable, because
calling into a section that is still `PAGE_READWRITE` faults under DEP. (That
was the first symptom: a segmentation fault in the middle of the loader work,
which is what a wrong order of operations looks like.)

With the index left at whatever the file held, the retail CRT read a garbage
index, `TlsGetValue` returned zero, and the CRT's errno pointer was null.

The banner now says so:

```
wmdiff: mapped .../ForgedAlliance_6c6c92f.exe at 0x00400000 (0xE96E9C bytes,
         642 imports resolved, 4 trapped), TLS index 14 with a 8276 byte block
```

What is still missing is the retail CRT's **process** startup: the harness runs
two of the retail's static initialisers because a target reads their results,
but it does not run the CRT's own init, so the CRT's math-error handler and
its security cookie are whatever the file holds. That is the next thing to
fix, and it is the reason the rows below still die rather than return.

### The retail's CRT math-error path recurses until the stack is gone

The recursion was real, and it is worth keeping the mechanism: the CRT's
per-thread accessor asks the OS for the Windows version, which goes back
through the same accessor, and the pair recurses until the stack is gone.

What it was triggered by, though, was the broken thunk rather than anything
about the geometry - a garbage `fTMax` and two garbage velocity vectors put NaN
bit patterns into the region helpers, and a comparison against a NaN is false,
so the helper fell through into its intersection arithmetic. With the thunk
fixed the retail CRT runs and this section is history; see "The retail CRT runs
now, and `IntrBox2Circle2::Find` can be measured" below.

### The harness now says where a fault happened

Every caught exception is reported with more than its code:

```
[CRASH] <row> retail side raised 0x%08X at <address> esp=<esp> (limit <limit>)
         [<module>], iter <n>; inputs: ...
```

The address, the module, the stack pointer and the stack limit are what turn
"the retail side crashed" into a statement about the retail's own code, its
CRT, or the harness. A stack overflow inside the retail and one inside a
handler that recurses look identical in the code alone.

### `IntrBox2Circle2::Find` at 0x00A4F580 - runs now

The address and the ABI here are right; "not runnable" is not. The row is
wired and passes 200,000/200,000 - see "The retail CRT runs now, and
`IntrBox2Circle2::Find` can be measured" below for what had to be fixed first
(the retail CRT's per-thread state, and the thunk's stack slots).

### Reading a vtable straight out of `.rdata`

Locating a retail address by walking call sites is slow and, as
`IntrBox3Sphere3::Find` shows, a call site can be right while the *slot* is
wrong. The vtable is better: it is the object's own dispatch table, and the
slot order is the class's declaration order.

The table lives in `.rdata` with a Complete Object Locator one dword ahead of
it, so the base is `COL + 4`. The PE section table gives the file offset, and
the slots are plain dwords:

```python
off  = section.PointerToRawData + (rva - section.VirtualAddress)
base = col_va + 4
for k in range(0, 0x30, 4):
    slot = struct.unpack_from('<I', data, off + (base - 0x400000) + k)[0]
```

The two slot layouts that came out of this, each confirmed on more than one
class:

| family | slot | member |
|---|---|---|
| `Distance` | +0x00 | the deleting destructor |
| | +0x04 | `Get(fTMin, fTMax, rkVelocity0, rkVelocity1)` - the base's window search |
| | +0x08 | `Get(fT, rkVelocity0, rkVelocity1)` - the derived class's |
| | +0x0C | `Get()` - the derived class's |
| | +0x10 | `GetSquared(fTMin, fTMax, rkVelocity0, rkVelocity1)` - the base's window search |
| | +0x14 | `GetSquared(fT, rkVelocity0, rkVelocity1)` - the derived class's |
| | +0x18 | `GetSquared()` - the derived class's |
| | +0x1C | `GetDerivative(fT, rkVelocity0, rkVelocity1)` - the base's generic |
| | +0x20 | `GetDerivativeSquared(fT, rkVelocity0, rkVelocity1)` - the base's generic |
| `Intersector` | +0x00 | the deleting destructor |
| | +0x04 | `Test(fTMax, rkVelocity0, rkVelocity1)` |
| | +0x08 | `Test()` |
| | +0x0C | `Find(fTMax, rkVelocity0, rkVelocity1)` |
| | +0x10 | `Find()` |

**The last two `Distance` slots were mislabelled, and it is worth recording
how.** The two slots that no passing row pins are +0x1C and +0x20; they were
read as `GetClosestPoint0()` and `GetClosestPoint1()`, which are not even
virtual in the rebuilt `Distance` base. Their bodies say otherwise:

* **0x00A39110** (the +0x1C slot) reads the step at `[this+0x2C]`, calls the
  vtable's **+0x08** slot - the swept `Get` - twice with `fT+step` and
  `fT-step`, and returns the difference times `[this+0x30]`. That is
  `GetDerivative`, and `[this+0x30]` is `m_fInvTwoDifferenceStep`: the
  rebuilt `Distance::GetDerivative` computes
  `m_fInvTwoDifferenceStep*(f(t+h)-f(t-h))` from the same two members.
* **0x00A39180** (the +0x20 slot) calls the +0x08 slot and then the +0x1C
  slot and doubles the result: `GetDerivativeSquared` is `2*d*d'`, which is
  what the rebuilt `Distance::GetDerivativeSquared` computes.

The Vector2 instantiation has the same two bodies at the same slots
(0x00A38AF0 and 0x00A38B60), which is what makes the reading a property of
the base class rather than of one class.

**The closest-point accessors are not in the vtable at all.** They are plain
methods, and the out-of-line body the existing `DistVector3Segment3::
GetClosestPoint1` row uses is 0x00A39230 - `LEA EAX,[ECX+0x1C]; RET` - a
direct-CALL target that the retail's `Moho::Projectile::CheckCollision`
reaches, not a slot.

**The retail's `Distance` base and the rebuilt one declare the same eight
virtuals, but in a different order**, so a slot index does not index the
rebuild's vtable. The rebuilt base is a WM4 translation, and the Wild Magic
3.4 copy has the same set: `Get()`, `GetSquared()`, `Get(fT,v0,v1)`,
`GetSquared(fT,v0,v1)`, `GetDerivative`, `GetDerivativeSquared`, and the two
window searches `Get(fTMin,fTMax,v0,v1)` and `GetSquared(fTMin,fTMax,v0,v1)`
- the Newton/bisection search for the minimum distance over a time window.
The retail has those eight too, but it declares the window searches at +0x04
and +0x10, while the rebuild declares them last.

**An earlier reading of this table was wrong on two points, and both were
caught by reading the base's own vtable.** `+0x00` is the deleting destructor
and `+0x04` is *not* a second destructor - it is the base's window `Get`.
`+0x10` is *not* a generic three-argument `GetSquared(fT,v0,v1)` - the retail
has no such member at all, and the slot holds the base's window
`GetSquared(fTMin,fTMax,v0,v1)`. The misreading produced the claim that the
retail had seven virtuals and the rebuild eight; they are eight and eight.

The base's four vftables are `0x00D71C2C` (`Distance<float,Vector2f>`),
`0x00D71C54` (`Distance<float,Vector3f>`), `0x00D71C7C`
(`Distance<double,Vector2d>`) and `0x00D71CA4` (`Distance<double,Vector3d>`),
each read out of the vptr the corresponding base constructor stores. The
window bodies they hold:

| instantiation | `Get(window)` | `GetSquared(window)` |
|---|---|---|
| `<float,Vector3f>` | `0x00A38C50` | `0x00A38EB0` |
| `<float,Vector2f>` | `0x00A38630` | `0x00A38890` |
| `<double,Vector2d>` | `0x00A39270` | `0x00A394C0` |
| `<double,Vector3d>` | `0x00A39860` | `0x00A39AB0` |

Their bodies are the Wild Magic 3.4 algorithm - the Newton loop bounded by
`MaximumIterations` (at `this+0x04`), then the bisection on the derivative
with the product compared against `±ZeroThreshold` (at `this+0x08`) - and they
dispatch to the derived class's swept `Get`/`GetSquared` at +0x08/+0x14 and to
the base's derivative slots at +0x1C/+0x20. The ABI is thiscall with four
stack arguments (`fTMin`, `fTMax`, `&rkVelocity0`, `&rkVelocity1`), the callee
pops them, and the result comes back in `ST(0)`.

No call anywhere in `src/sdk` reaches the window search, and no call inside
Foundation does either, so the rebuild's copy of that body sits on no engine
path; the slot order difference is therefore invisible to the game, and a
slot number remains a property of the retail's table alone.

Each was pinned by a row that already passes: `Test` at +0x08 by
`IntrBox3Sphere3::Test`, `IntrBox2Circle2::Test` and `IntrLine3Box3::Test`;
`Find` at +0x0C by `IntrSegment3Box3::Find`; `GetSquared()` at +0x18 by
`DistVector3Segment3::GetSquared` and `DistVector3Box3::GetSquared`; `Get()` at
+0x0C by `DistVector3Segment3::Get`. The vtables that gave them:
`0x00D71CFC` (IntrBox3Sphere3), `0x00D71D8C` (DistVector3Box3), `0x00D71E6C`
(DistVector3Segment3), `0x00D71EC4` (IntrBox2Circle2), `0x00D71EF4`
(IntrLine3Box3), `0x00D71DDC` (IntrSegment3Box3).

The `Intersector` row was corrected while wiring `IntrSegment3Capsule3`. The
earlier table listed two destructors at +0x00/+0x04 and put `Find()` at +0x0C;
that is wrong. Read against five classes, the retail layout is
`[dtor][Test(tmax)][Test()][Find(tmax)][Find()]` - the dynamic `Test` really is
at +0x04 and the static `Find` at +0x10. `IntrSegment3Capsule3`'s ctor
(`0x00A6BE80`, vtable `0x00D72094`) shows it in one row:
`0xA6C030, 0xA45510, 0xA6BEB0, 0xA45530, 0xA6C070`, where `0xA45510` and
`0xA45530` are the base's `xor al,al; ret 0xC` stubs, `0xA6BEB0` is `Test()` and
`0xA6C070` is `Find()` at +0x10. It also explains why the earlier rows still
passed: the addresses they called directly were right, only the slot labels
were off (and the two swept `IntrSegment3Sphere3` rows had +0x04 and +0x0C
swapped in their source strings - corrected).


Reading the table is also a check on an existing row: `DistVector3Box3::Get`
at +0x0C turned out to be the same seven instructions as
`DistVector3Segment3::Get` at +0x0C, dispatching +0x18 and calling the CRT's
sqrt - which is how the CRT-sqrt ceiling below was confirmed for a second
class rather than assumed.

### `IntrBox3Sphere3::Find` at 0x00A43420 - the rejection was wrong, and so was the correction

The rejection of 2026-10 was wrong: the address is the `Find` in the
`Intersector` vtable, and the row is wired and passes 200,000/200,000.

The correction that came with it was wrong too, and in the same way - it was
made from a measurement taken with a broken thunk. See the next section.

### `IntrBox3Sphere3::GetContactPoint`: the bool agrees and the point does not

`IntrBox3Sphere3::Find` passes 200,000/200,000. `GetContactPoint` - the same
object, read after the same `Find` - fails on 94% of draws, and the two numbers
are not close: the retail's point and ours are different points on the box.

The row now also reports the **contact time**, because that is what tells the
two possibilities apart:

- the time agrees and the point does not, so the region helpers agree on
  *whether* they hit and disagree on *where*;
- the time disagrees, so the time computation itself differs.

The time agrees (zero on both sides in the samples), so it is the first.

The point is `Centre + (iSignX*fIx)*Axis[0] + (iSignY*fIy)*Axis[1] +
(iSignZ*fIz)*Axis[2]`, and the retail's own tail (0x00A44102..0x00A4416C) is
that sum, term by term, in the same order: four adds per component, chained
`((centre + t0) + t1) + t2`. So the **form** of the sum is not the difference.
The difference is in the terms - `fIx`, `fIy`, `fIz` - which the region helpers
produce, and which are computed inside code the retail has inlined into `Find`.

**One hand-made case per region branch says where it is not.** A box centred at
the origin with the identity axis and extents 10, and a sphere of radius 2
placed so that it really overlaps: on the +x face, the +y face, the +z face,
each of the three edges, and the vertex - plus the same two with no motion.
**All ten agree exactly**, so the divergence needs geometry that is not
axis-aligned.

> **Both of those readings were wrong, and the row is closed now.** The ten
> hand-made cases agreed for a reason that had nothing to do with geometry:
> a sphere that really overlaps takes a *hit* path, and this algorithm turns a
> hit into `IT_EMPTY` and `false` - so both sides reported zero for the point
> and the cases agreed trivially. The generator's axis is the identity on every
> draw, so the geometry was never oblique either. The real causes are in
> "The last row: `IntrBox3Sphere3::GetContactPoint`" below: the row was
> comparing uninitialised memory, and the contact time was off by an ulp
> because the retail's square root is a 24-bit one.

That is the same class of divergence the collision-slice work found in
`IntrBox2Circle2::Find`: the helpers agree on the hit and disagree on the
coordinates, and only for oblique geometry. Wiring the helpers themselves is
not possible - retail has them inlined - so the next step is the same as it was
there: find the comparison inside them that differs. The two rows very likely
share that comparison.

**One thing worth stating about the row's contract:** on a *miss* the contact
point is not written by either side - retail's region helpers leave whatever
they had computed in it, and the rebuild's leave nothing at all - so comparing
it would compare two pieces of uninitialised memory. The row reports zero for
a miss on both sides, and the contact time, which *is* written on both paths,
is still compared. That was already the behaviour; what the work established
is *why* it has to be, and that the failing draws are not misses at all (they
all have a contact).

> Corrected: the draws that failed were neither misses nor contacts. `Find`
> returns **true** when the region helper said "no intersection", and on that
> path both sides write the contact point from undefined locals. The gate is
> the intersection type, not `Find`'s return value - see below.

### The retail inlines a reciprocal in two of these helpers

`GetVertexIntersection` (retail 0x00A41740) and `GetEdgeIntersection`
(0x00A41800) are real retail functions, and the rebuild has both. Retail does
**not** call its `InvSqrt` in them:

```
0x00a417c4  FABS
0x00a417c6  FSTP float ptr [ESP + 0x18]
0x00a417ca  FLD float ptr [ESP + 0x18]
0x00a417ce  CALL 0x00A91000        ; the CRT's sqrt, on a double
0x00a417d3  FLD1
0x00a417d5  FLD ST0
0x00a417d7  FDIVRP ST2,ST0          ; 1.0 / that
0x00a417db  FSTP float ptr [ESP + 0x18]   ; and only now narrowed to a float
```

`Math<Real>::InvSqrt` cannot be used for that, because it models retail's
*standalone* `InvSqrt` (0x0050D150), which takes the reciprocal in the x87
register and so rounds twice - 24 bits for the square root and 24 bits again
for the reciprocal.

**This section's earlier reading of the division was wrong**: it said the
division "divides in double and rounds once". It does not - the x87 divides at
the control word's precision, so under `_PC_24` the reciprocal is a 24-bit
one as well. What the earlier account missed is the *square root*: the retail's
CRT branches on the control word (`0x00A91028 CMP word [ESP], 0x027F`) and
under `_PC_24` it returns a 24-bit square root, not a correctly rounded
double. Both halves of the reciprocal are therefore 24-bit, and the rebuilt
source has to take the square root in float precision to match. That is
measured in "The retail's square root is a 24-bit one" below.

The other class gets this right with `Math<Real>::InvSqrt`, because retail
really does call its own `InvSqrt` there. The two classes are not the same on
this, and reading the disassembly of each is the only way to know.

This change moved no measurement on its own - the draws that fail do not take
the time-of-impact path - so it stands on the instruction sequence above
rather than on a number. (The measurement arrived later, with a row that does
compare the time of impact: see below.)

### Two defects in this row's own generator

The generator drew the sphere's **radius** and the **horizon** with `GenPlain`,
which signs them. A negative radius is not a shape: the region tests compare
against `Radius*fVx*iXSign`, so the sign of the radius flips those
comparisons. A negative horizon is equally meaningless. Both are now positive.

### Retracted: `IntrBox3Sphere3::Find`'s gate was not inverted

This section claimed a real rebuild bug: that `Wm3IntrBox3Sphere3.cpp`'s gate
read `iRetVal != 0` where it should read `iRetVal == 0`, and it changed the
source accordingly.

**There was no bug.** The measurement behind it was taken with a broken thunk
- the three-argument thunk was handing retail the wrong stack slots, so retail
was running on garbage and the row was not measuring anything. With the thunk
fixed, the original `iRetVal != 0` is right:

| the gate | `IntrBox3Sphere3::Find` |
|---|---|
| `iRetVal != 0` (the Wild Magic source) | **PASS 200,000/200,000** |
| `iRetVal == 0` (what this section changed it to) | 154359 mismatches in 200000 |
| `iRetVal != 1` | 151260 mismatches in 200000 |

The source is back to the Wild Magic reconstruction, byte for byte, except for
the two helpers described in the next section. The lesson is the one this file
already keeps: **a row that looks close can be two wrong answers cancelling**,
and a measurement taken while the harness itself is broken is not a
measurement.

### One harness property that matters when reading per-seed numbers

`main.cpp` drives every target from a single global RNG, so **adding a row
shifts the random stream for the rows after it**. A per-seed mismatch count is
therefore only comparable against the same table. `IntrSegment3Box3::GetPoint`
measured 0 on `0xC0FFEE01` before `IntrBox3Sphere3::Test` was added and 1 after,
with no change to that row at all. Re-measure when the table changes rather than
treating a shifted count as a regression.

### `IntrSegment3Box3::GetPoint` — closed

Wired at `0x00A46300`. The retail method takes the index as a dword stack
argument with `RET 4` and returns `&m_akPoint[index]` in EAX — a pointer into
the object at `this + 0x1C + 12*i`, not a Vector3 by value. The thunk hands the
pointer back untouched and the target dereferences it on both sides.

Two things about the row are narrowing, on purpose:

- It is **gated on `Find()` succeeding**. `DoClipping` only writes `m_akPoint`
  on intersection and the source's own `GetPoint` asserts `0 <= i < m_iQuantity`.
  The first version read the array unconditionally and reported 5311
  mismatches — it was comparing uninitialised memory.
- **Index 0 only.** Index 1 needs `m_iQuantity`, which the generator cannot know
  before running `Find`.

It was open for two seeds at 1–2 mismatches in 200000, always 1 ULP on a
subnormal. Closing it needed the instruction sequence at `0x00A504ac`, not just
the knowledge that the body is x87:

```
FLD  float ptr [EDI]          ; Direction.x
FMUL ST1                      ; x fT0
FSTP float ptr [ESP + 0x18]   ; <- product rounded to FLOAT here
FLD  float ptr [EBX]          ; Origin.x
FADD float ptr [ESP + 0x18]   ; + that float
FSTP float ptr [ESP + 0xc]
FSTP float ptr [EAX]          ; akPoint[0].x
```

The product is **stored back to float before the origin is added**, so this is
not one extended-precision chain. The first attempt modelled it as
`Add24(Mul24(t, d), o)`, which keeps the product in binary64 storage, and it made
the row worse — 0 -> 3 on the default seed, 2 -> 5 on `0x12345678`. Narrowing
the product to float first is the entire difference. All twenty-one rows now pass
every seed tried, 200000 and 1000000 iterations.

### Indirect verification of inlined leaves

A WildMagic leaf A with no out-of-line retail body can still be verified through
a WildMagic function B that calls it, provided B itself has a verified retail
address. B is matched like any other row; when B passes, A is recorded as
indirectly verified through B. This never substitutes an engine helper with a
similar name: B must be a `dependencies/WildMagic3p8` function, and its address
must come from anchor-and-walk.

Rules:

- B must live in `dependencies/WildMagic3p8`. Engine functions are not used:
  wmdiff cannot link the rebuilt engine code, and a mirror would test a copy.
- If B is itself inlined, walk up until an ancestor with a retail address is
  found, and record the chain.
- Up to three distinct B per A, chosen for input-domain diversity where
  possible. Fewer is acceptable; zero leaves A unverified.
- B must pass 200000/200000; at least one B per A also gets a 1M run. A's
  coverage is the input domain B actually exercises, and that domain is part of
  the record.
- The retail disassembly is inspected to see whether A's work is inline or a
  call; the method does not require inlining.
- The relation is recorded as a comment on A's target row, with a pointer on
  B's row. There is no separate table or harness field.

First application: `Vec3::Length` through `Vec3::Normalize` (0x00452AF0). The
B row passes 1M/1M; its retail body expands Length inline (lengthSq at
0x00452AF6..0x00452B2A, `SqrtfBinary` call at 0x00452B39), and every `GenVec3`
input evaluates Length before the epsilon branch. The exact record is in the
`Vec3::Length` row comment.

Addresses are established against `ForgedAlliance_6c6c92f.exe` — the same image
the engine rebuild is compared against — not the unpatched 2007 binary.

Addresses must come out of the procedure above. A `FUN_` address that merely
turns up in an unrelated reverse-engineering note is not evidence; see the
`SqrtfBinary` caveat below.

## Floating-point codegen is per function, not per build

The rows that used to fail were not harness bugs and not translation mistakes. The
retail image compiled **different leaf functions of the same header with
different floating-point models**, and no single build switch reproduces all of
them. Rebuilding this tree with `/arch:IA32` (forcing x87) and re-running the
same four rows gave (measurements taken before the source fix below, so the
default column is the old source):

| function | retail codegen | default build (SSE2), pre-fix | `/arch:IA32` (x87) |
|---|---|---|---|
| `Vec3::Normalize` | SSE (`MOVSS`/`MULSS`/`ADDSS`) | **PASS 200000** | FAIL 99 |
| `Vec3::Cross` | SSE (`XMM0`..`XMM6`) | **PASS 200000** | FAIL 7313 |
| `Vec3::LengthSq` | x87 (`FLD`/`FMUL`/`FADDP`) | FAIL 575 | **PASS 200000** |
| `Vec3::Dot` | x87, summed z,y,x | FAIL 28529 | FAIL 28280 |

Read that as four different answers to "which model was this built with", in
one 300-byte region of the image. A build-flag fix cannot reconcile them, and
per the rule above it is not the kind of fix that would be allowed anyway.
The harness stays on the engine's settings (`/fp:precise`, SSE2 via `faf_codegen`) because it is
measuring the engine as the engine is actually compiled. The two x87 leaves are
matched in source instead: their expressions are evaluated in `double` with the
significand rounded back to 24 bits after every operation, which reproduces
`_PC_24`'s 24-bit significand *and* x87's 15-bit exponent while still compiling
to SSE.

### `Math::InvSqrt`: two 24-bit roundings, not one 53-bit one

Retail `0x0050D150` is `FLD float [EBP+8]; FSQRT; FLD1; FDIVRP; RET` under
`_PC_24`. The square root rounds its significand to 24 bits, the reciprocal
rounds to 24 bits, and the final `FSTP dword` is then exact. Ours computed
`1.0/sqrt((double)x)`: 53 bits carried across both steps and one rounding at the
end. That is a *different* sequence, not a more precise version of the same
one - it differs on 12.5% of inputs, always by 1 ULP. Worth 769.434 in,
0.0360508 out, retail `0x3D13A9F5` against the old ours `0x3D13A9F6`. The fix is
`return ((Real)1)/Math<Real>::Sqrt(fValue);`, so each step rounds where the
retail instruction does.

### `Vec3::Dot`: order first, then exponent range

Retail's body is `FLD [EAX+8]; FMUL [ECX+8]; FLD [EAX+4]; FMUL [ECX+4]; FADDP;
FLD [EAX]; FMUL [ECX]; FADDP` — i.e. `(z*z' + y*y') + x*x'`. Ours was the
source order `(x*x' + y*y') + z*z'`. This is MSVC's x87 pattern for a
left-associative chain: the leftmost term is added **last**, because the
accumulator is built from the tail of the expression.

Simulating both orders exactly (products are exact in binary64, so the only
freedom is where each sum rounds) reproduces both sides' bits on every sampled
mismatch of that first class:

| model | iter2 retail | iter5 retail | iter11 retail |
|---|---|---|---|
| retail order, round each step to float32 | **matches retail** | **matches retail** | **matches retail** |
| exact (no intermediate rounding), either order | matches ours | matches neither | matches ours |

That order change reproduces on 14.3% of random inputs, all 1 ULP, and took the
row to 134 / 200000. The residual was **exponent range**, which the float32
model above cannot represent: 91 overflow cases (binary32 product saturates to
inf and `inf + (-inf)` gives NaN, while x87's larger exponent defers overflow to
the final store and yields an infinity), 23 subnormal results, 6 normal results
whose intermediate products were subnormal, 13 signed-zero differences, and 1
normal/inf case. Plain `double` intermediates do not fix this (41060
mismatches): 53 significand bits change the ordinary cases. Rounding the double
back to 24 bits after each multiply/add does, and that is what
`Wm3Detail::RoundTo24` implements.

### `Vec3::LengthSq`: order already matched, exponent range did not

The same stack-trace discipline gives retail `z*z + (y*y + x*x)` for
`0x004757E0` — the same order as ours, since `FMULP ST2` is
`ST(2) = ST(2) * ST(0)` followed by a pop, not `ST(0) = ST(0) * ST(2)`.

Its old 0.29% was therefore not order. Modelling the retail body exactly (each
operation rounded to a 24-bit significand, x87's extended exponent, one final
store to float) reproduces all 575 sampled mismatches; the binary32 model
reproduces none of them. The cases break down as 520 subnormal/subnormal, 28
subnormal/zero and 27 normal/normal. The last group is the important one: the
stored result is a normal float, but an *intermediate* product was subnormal in
binary32 and lost bits that x87 kept. This is why the harness's MXCSR was never
the explanation - `0x1FA0` in the banner is `0x1F80` plus the sticky precision
flag (bit 5), and DAZ (bit 6) is clear. The gap is the exponent width itself,
and it is closed by the same `_PC_24` emulation as `Dot`.

### The x87 control word moves the numbers a lot

`--cw 0x0073` and `--cw 0x0000` both **crash** the run rather than reporting
mismatches. The cause has not been worked out — note that `0x0073` would decode
to precision control `00` (single precision) with round-to-nearest and all
exceptions masked, which ought to be a perfectly ordinary control word, so the
obvious "reserved PC encoding" story does not fit. The two values that do run
disagree sharply:

| `--cw` | `LengthSq`, pre-fix source | `LengthSq`, current source |
|---|---|---|
| `0x007F` (default, `_PC_24`) | 575 | **PASS 200000** |
| `0x027F` (`_PC_53`) | 31846 | FAIL 31341 |

`0x007F` decodes as precision control `00` (24-bit significand), rounding
control `00` (nearest), and the low six exception masks set. That decode is no
longer an assumption: `_controlfp(_PC_24, _MCW_PC)` is what the engine installs
(`WinApp.cpp:2476`, `SimDriver.cpp:1121`), and `MathReflection.cpp` recognises
the result as `(controlWord & 0x7F) == 0x7F`. The `_PC_24` emulation in the
rebuilt source is therefore calibrated to `0x007F`; under `0x027F` retail is no
longer running the mode the rebuilt expressions reproduce, so those rows fail.
That is a property of the test environment, not a regression in the rebuild.

## Pitfalls

Every item here was hit on this rebuild, and cost a wrong conclusion or a broken
build. They are written as what happened and what to do instead.

### Reading the retail disassembly

- **A scratch slot is not the return value.** Retail `Normalize` ends in
  `DIVSS XMM0,[ESP+0x4]`, which reads like "returns 1/length". It is not: the
  `FST float [ESP+0x8]` just above wrote the *fsqrt result* — the length — to
  `[esp+0x4]`, and the `DIVSS` turns that into a scaling factor the three
  following `MULSS` reuse. `EAX` is never written and `ST(0)` is never laid out,
  so the function is `void`. Chasing that `DIVSS` is what produced a completely
  fictional "WM 3.8 changed the return-value semantics" conclusion, which then
  had to be withdrawn. **Push every store to the function first and re-derive
  which slot holds what, rather than reading the arithmetic in isolation.**
- **A `JBE` on `FCOMI` is just `<=`, not evidence of special handling.** MSVC
  lowers `if (len > eps)` on x87 that way; for ordered inputs it is the same
  test. It only differs on NaN, where `FCOMI` sets all three flags.

### Wiring a thunk

- **`__declspec(naked)` belongs on the definition only.** MSVC rejects it on a
  declaration with no body (C2488, "'naked' can only be applied to member
  functions"). That is why `wm3_thunks.h` carries just `__cdecl` and
  `wm3_thunks.cpp` carries the `naked`.
- **`std::vector<const T*>::data()` cannot be returned as `const T*`.** It hands
  back `const T**`, and `/permissive-` refuses the implicit drop of a level of
  indirection. `targets_wm3.cpp` therefore stores `Target` **by value** — it is
  a plain aggregate, and this keeps the function cast-free.
- **Do not put a by-reference `int` in the same call that sets it.**
  `Append(merged, wm3vec::Targets(n), n)` has unspecified argument evaluation
  order; MSVC reads the trailing `n` first, so it holds the *previous* slice's
  count and the last append reads far past the end of its array. It presented as
  a segfault on `--list` plus a spurious `[PASS]` for a target that had never
  run. Fetch into its own statement.
- **`printf("%s", some_std_string)`** is C4477 and passes a temporary's internals
  as a `char*`. Use `.c_str()`.

### Designing a target

- **Never compare a value retail does not produce.** `difffuzz` declared
  `float Wm3Normalize(void*)` for a void retail function and compared whatever
  was left in `EAX`. Over 20000 iterations, 400/400 of its reported mismatches
  were in that one word and the normalised vector never differed once — a pure
  artefact that had been sitting in the results looking like a code divergence.
- **Never feed a parameter retail hard-codes.** Retail bakes `Normalize`'s
  threshold in as a `.rdata` constant at `0x00d71bfc`. A random `eps` on our side
  would report a "divergence" that is only a signature difference. When the
  retail signature differs from the Wild Magic API, the *target row* changes
  (lanes, word counts, layouts) — a thunk cannot bridge it.

### Drawing conclusions

- **Matching a reference source is not matching the binary.** `Wm3Vector3.h`'s
  `Normalize` is a faithful translation of the Wild Magic 3.4 original, and a
  3.4-vs-rebuild diff called it identical — while the retail build's ABI differs
  in return type, calling convention and one hard-coded constant. A source diff
  validates the translation; it cannot answer whether the rebuild matches the
  retail binary. Only wmdiff can.
- **Source diffs are blind to constants and codegen.** They cannot see a
  mistyped magic number, x87 excess precision, FMA contraction, or intermediate
  rounding. Upstream commit `da3926cf` was exactly that last class and changed no
  source text at all.
- **An address from an unrelated note is not evidence.** `FUN_00452FC0` in the
  `SqrtfBinary` comment looks like a gift because it happens to be the call
  target inside retail `Normalize` — but it came out of a different
  reverse-engineering pass and nothing about it has been verified. Addresses come
  from the procedure above or they do not come.
- **A same-named engine helper is not the template's retail body.** difffuzz's
  `retail_thunks` catalog names engine mirrors (`Moho::SLERP`,
  `Moho::MatrixToQuat`), not the WildMagic templates. `Quaternion::Slerp` has no
  rebuild call site at all and `FromRotationMatrix` only appears inside
  `Wm3ContBox3.cpp`; neither has an anchored out-of-line body. Pairing
  0x004EBC00 / 0x004EB3F0 with them because the names match produced
  173k and 179k mismatches and proved nothing about WildMagic.
- **Regex-extracted structure has to be spot-checked by eye.** A
  "nearest preceding definition" scan confidently named
  `AssignWeakAudioEnginePairFromShared` as the caller of `Dot` and `Cross`. The
  same scan missed `FastInvSqrt`'s body entirely. Never report a count or an
  attribution from a regex alone.
- **Two failed experiments do not make a conclusion.** Trying the width lever on
  `IntrLine3Box3::Test` gave 490 (dot order only, unchanged), 3292 (double
  storage throughout) and 7057 (`Real` slots, double expressions) against an
  untouched-source 490. Every emulation lost to the original, so the natural
  reading was "retail is not running this on a wide-exponent stack" — and that
  reading was wrong. Retail *is* x87 there; the model had to match both the
  per-operation width *and* the float store points, and the two half-models
  tested were each missing one half. The binary was already dumped and was not
  read operation by operation. Read the retail body first, work out where its
  intermediates land, and only then choose an emulation. Recording a negative
  result is right; treating it as an explanation is not.
- **Runs are not deterministic unless you made them so.** `.memory`'s recorded
  runs were assumed reproducible and a beat-400 "stable" result did not
  reproduce. Say how many iterations a claim rests on.
- **A normal-looking result can still hide an underflow.** In the old
  `LengthSq` row, 27 of 575 mismatches had normal floats on *both* sides; the
  difference came from an intermediate product that was subnormal in binary32
  and therefore lost bits x87 kept. Classifying only the stored result would
  have dismissed them as a different bug.
- **MXCSR bits 0..5 are sticky flags, not control settings.** The banner's
  `0x1FA0` against the configured `0x1F80` is the precision-exception flag
  (bit 5) left behind by startup code. DAZ is bit 6 (`0x40`) and was clear all
  along. Compare environments with `& 0x1F80` (the engine's
  `IsDefaultFloatingPointEnvironment` does exactly that), not on the raw word.
- **`_PC_24` is not "compute in double".** A plain `double` evaluation carries
  53 significand bits and changes the ordinary cases (`Dot`: 41060 mismatches).
  The faithful model is `double` storage with the significand rounded back to
  24 bits after *every* multiply and add, keeping the binary64 exponent; one
  final conversion to float then does the same single store rounding retail's
  `FSTP dword` does.
- **39 of the 58 targets have no engine call site at all** (all of `Wm3Math`, most
  of `Wm3Quaternion`). For those the anchor-and-walk procedure cannot start from
  the rebuild side, and small template leaves may not exist out of line in the
  retail image either — they get inlined at /O2. Do not read "no anchor" as
  "not present".

## Caveats worth knowing before reading results

- `Math<float>::UnitRandom` / `SymmetricRandom` / `IntervalRandom` are `srand`
  / `rand` over global CRT state. Their generators force a non-zero seed, since
  `seed == 0` skips the reseed and the two sides would then advance the shared
  sequence at different points and differ for reasons that are not the code's
  fault.
- `Quat::LengthSq/Dot/Conjugate/Multiply` and the `Vec2::Dot/LengthSq/Length`
  static forms are **FAF-MOD additions**, not upstream WM 3.8 symbols. A retail
  VA lookup may legitimately come up empty for those; the upstream spellings
  are the instance methods (`SquaredLength`, instance `Dot`, ...), which are
  also in the table so the two forms can be cross-checked.
- The `Vec3::Dot` and `Vec3::LengthSq` static rows and the
  `Vec3::Dot(instance)` / `Vec3::SquaredLength` rows point at the same retail
  bodies (`0x00452AD0`, `0x004757E0`), because the retail image only has the
  instance forms. Both rebuilt forms now use the `_PC_24` emulation, and the
  instance `Length` is routed through `SquaredLength`, so the static helper and
  the .inl implementation agree bit for bit.
- `Wm3::SqrtfBinary` carries a comment saying it was recovered from the retail
  binary at `FUN_00452FC0`. That address came out of an unrelated
  reverse-engineering pass, not out of a systematic identification, so treat it
  as unverified: it is the retail call target inside `Vector3<float>::Normalize`
  (`0x00452AF0`), which is consistent with it being that function, but nothing
  about it has been confirmed. Do not use it as the first row to wire up — the
  addresses have to come out of a systematic pass.

### The `IntrBox2Circle2::Find` wall, measured again - the fault is now known

Took the row as far as it goes and got further than the last attempt, by
throwing away the printf-based diagnosis.

**The diagnosis itself had a bug.** `DumpStackFrames` filtered the stack with
`w > 0x004E96E9C` as the image's upper bound. The image is `0x400000 +
0xE96E9C`, so the bound is `0x01296E9C` - one hex digit too many. Everything
from `0x01296E9C` down, which is the harness's entire heap, passed the filter,
so the "stack frames" it printed were the harness's own allocations. Every
reading taken from that output was meaningless. Fixed.

**A fault dump is now written in the exception filter**, before the handler
unwinds, with `WMDIFF_DUMP=1` and `WMDIFF_DUMP_PATH=...`. That is the only view
of the faulting context that is not already stale, and it is what produced
everything below.

**The real stack**, from the dump:

```
0xcf8830a3        <- the call target: not in any module
0x00a95bb6        <- inside the CRT's per-thread accessor
0x00aa7ba0        <- the CRT's error path
0x00a910b8        <- inside the CRT sqrt, 0x00A91000
wmdiff_release!main+0x720
```

and the instruction it dies on:

```
00a95b9f  push esi
00a95ba1  call dword ptr ds:[0C0F548h]     ; -> edi, and it returns 0
00a95ba7  push dword ptr ds:[0F3E498h]     ; -1, which becomes the callee's argument
00a95baf  call 00a95a58                    ; -> eax, the per-thread block
00a95bb4  call eax                          ; *** dies here
```

The block itself is fine: `[esp+8]` holds `0x002F7000`, a freshly allocated
block, and `ebx` holds the same. What is garbage is the value taken out of it.

**It is not input-dependent.** Same seed, two runs, two different fault
addresses (`0x4EDFC576`, then `0xBACABA7D`). Three seeds, three more. So the
pointer is read from memory nobody initialised, not from the function's inputs -
which rules out the row's own arithmetic and points squarely at the CRT's
per-thread state.

**The five `.data` slots are confirmed a dead end**, by the previous attempt's
own experiment: each pointed at a stub that printed its own address, and none
was called. This run adds why: read out of the loaded image all five are
`0x00000000`, and they belong to three different subsystems - two
(`0x00FB896C`, `0x00FB89A0`) are the C++ name undecorator's own function
pointers, written by `___unDName` and `UnDecorator`; one (`0x00FB67A4`) is
written by `FUN_009F5140`; two (`0x00FB8C10`, `0x00FB8C2C`) sit inside the x87
emulation / SEH translation block around `__Dscale`/`__FDnorm`/`Catch_All`.
They were never five of the same thing, so "point them all at a return-zero
stub" could not have worked.

What is left is one question and it is now narrow: **the per-thread block is
allocated and then something reads a function pointer out of it before the CRT
has filled the field in.** The next step is to break at `0x00A95BB4` and read
`eax`, then trace where the field it came from is supposed to be written. That
wants a debugger on a live process, not a dump - the dump gives the state after
the fact and the block's contents are the thing being asked about.

Everything else for this row is ready and stays ready: the thunk
(`wm3_coll_thunks.cpp`, `0x00A4F580`, `RET 0xC`, bool in AL), the harness
function, and the generator. Only the table row is missing, and it stays missing
until the retail side can run.

### The CRT's FLS slots were wired in the wrong order - proven, and not the cause

`__mtinit` (`0x00A95DC4`) resolves four KERNEL32 exports and stores them at
`0x00FB7E18..0x00FB7E24`. The order was read out of the image: it calls
`GetProcAddress` with the name strings at `0x00D72F40` ("FlsAlloc"), `0x00D72F34`
("FlsGetValue"), `0x00D72F28` ("FlsSetValue") and `0x00D72F20` ("FlsFree"), and
stores each result in turn:

| slot | should hold | was wired as |
|---|---|---|
| `0x00FB7E18` | `FlsAlloc` | `FlsGetValue` |
| `0x00FB7E1C` | `FlsGetValue` | `FlsSetValue` |
| `0x00FB7E20` | `FlsSetValue` | `FlsFree` |
| `0x00FB7E24` | `FlsFree` | `FlsAlloc` |

Rotated by one. Fixed. It did **not** change the fault, and the reason is worth
writing down: the failing path (`__getptd` -> `0x00A95A58`) calls the IAT's
`TlsGetValue`/`TlsSetValue` at `0x00C0F400`/`0x00C0F408` directly and never
touches those four slots. So the rotation was a real bug in the harness wiring
but not the one blocking this row.

The same disassembly also confirms the one thing the harness had right: `__mtinit`
stores `TlsAlloc()`'s result into `0x00F3E49C`, which is the index the harness
publishes.

`__mtinit` still cannot be called. With the thread index and the version cache
both published beforehand it kills the process outright - a bare segfault before
any output, not a caught exception - so the earlier note stands and the slot
values have to be reconstructed from the disassembly rather than by running the
initialiser.

What is left is one datum: the value `0x00A95A58` returns. Its `call eax` is the
instruction that dies, `eax` is garbage that changes between runs of the same
seed, and `0x00A95A58` is the only thing between the caller and that value: it is
`TlsGetValue(index)`, then on the null branch a helper at `0x00A959C6`, then
`TlsSetValue`. Reading `eax` inside it needs a software breakpoint at
`0x00A95A58` - the hardware route is not available because the image's `.text` is
mapped read-only, and `ba` cannot be armed at the initial process breakpoint
("the system resets thread contexts").

### `0x00A959C6` is a pointer decoder, and encoding the slots makes it worse

The CRT's per-thread accessor is a pair. `0x00A959C6(arg)` is
`DecodePointer(arg)` - it resolves `KERNEL32!DecodePointer` through
`GetProcAddress` and calls it on the argument, returning the argument unchanged
only when the resolution fails. `0x00A95A58` uses it on the null branch of
`TlsGetValue`:

```
00a95a58  push [0x00F3E49C]        ; the thread index
00a95a5f  call [0x00C0F400]        ; TlsGetValue(index)
00a95a69  jne  0x00a95a86          ; present -> return it
00a95a6b  push [0x00FB7E1C]        ; FlsGetValue, through the decoder
00a95a71  call 0x00A959C6          ; DecodePointer(FlsGetValue)
00a95a79  push esi
00a95a7a  push [0x00F3E49C]
00a95a80  call [0x00C0F408]        ; TlsSetValue(index, decoded)
```

So raw addresses in `0x00FB7E18..24` get `DecodePointer` applied to a pointer
that was never encoded. That looks like the bug: store `EncodePointer(proc)`
instead. **It was tried, and it is worse** - with encoded values the failure
changes from a caught `0xC0000005` to an uncatchable stack overflow, `__except`
cannot see it, and the process dies before printing anything. Reverted.

So the decode path is real but is not the whole story, and the encoded form is
measurably the wrong answer. Do not re-try it.

Note also that `__mtinit` itself stores the raw `GetProcAddress` results into
those slots (0x00A95DF4 onward, no encode in between), which is why the raw form
is the one that stays.

### What is left, precisely, and why it is not a quick step

Two facts now bracket the fault and neither is enough on its own:

- the value that dies is the return of `0x00A95A58`, and that return is either
  `TlsGetValue(index)` or `DecodePointer(FlsGetValue)` stored back through
  `TlsSetValue`. Both forms were measured and both end in a garbage `call eax`;
  the first as a caught AV, the second as a stack overflow.
- the index is right: `0x00F3E49C` holds 15 and the harness got 15 from
  `TlsAlloc`, and `__mtinit` writes `TlsAlloc`'s result to that same slot.

What is not known is which of the two branches is taken and what value it
carries. Reaching that wants a breakpoint at `0x00A95A58`, and neither route
available here works:

- `bp 0x00A95A58` from the debugger fails with Win32 error 299,
  `ERROR_PARTIAL_COPY`, because the image is mapped by the harness process and
  does not exist at the debugger's initial break.
- planting an `int3` from inside the harness (`WMDIFF_INT3_AT`) does write the
  byte, but the WOW64 target raises `0xC000001D` at the following address instead
  of a breakpoint exception, so it cannot be used as a stop.

The remaining route is to write the debugger's own `int3` after the image is
mapped - the harness would have to yield to the debugger at a known point after
`image.Load`, which is a `DebugBreak()` behind an environment variable and one
more session.

### The per-thread slot is empty, so the null branch runs - measured

With `WMDIFF_BREAK_AFTER_LOAD=1` a breakpoint at `0x00A95A58` can finally be set
after the image is mapped, and stepping it gives the one datum this was stuck on:

```
00a95a5f  call dword ptr ds:[0C0F400h]   KERNEL32!TlsGetValue (75493660)
00a95a65  mov  esi,eax
eax=00000000                             ; NULL
```

`TlsGetValue(index)` returns NULL, so the null branch runs:
`DecodePointer([0x00FB7E1C])`, store it back through `TlsSetValue`, return it,
and `__getptd` calls that value.

That closes the question of which branch, and it also finishes off the encoding
idea. Both forms of the slot fail, each in its own way:

| `0x00FB7E1C` holds | result |
|---|---|
| raw `FlsGetValue` | `DecodePointer(raw)` is a per-process random value, and `call eax` faults on it |
| `EncodePointer(FlsGetValue)` | decodes back to a valid `FlsGetValue`, which is then **called** as if it were the per-thread block - an uncatchable stack overflow |

Neither is the answer, which means the slot is not meant to hold an FLS entry
point at all. `call eax` on the result says the value in that TLS slot is a
**function**, so `0x00FB7E1C` should hold an encoded pointer to some CRT
function - and which function is what is still unknown.

`WMDIFF_BREAK_AFTER_LOAD` is kept: it is off by default and it is the only way
found to get a breakpoint into the retail image, since `bp` from the debugger
fails with Win32 error 299 and an int3 planted by the harness raises
`0xC000001D` past it on the WOW64 target.

### What a real retail process actually has in those slots

Debugging `ForgedAlliance_6c6c92f.exe` itself under cdb answers the question the
reverse engineering could not. At `0x00A95A58`, in a process whose CRT started
normally:

```
0x00FB7E18    b5bfceba b5b8553a b5be493a b5b4b53a     ; four encoded pointers
0x00F3E498    00000007                                ; an FLS index
0x00F3E49C    00000014                                ; a TLS index (20)
```

and one `p` past the `TlsGetValue` call, `eax = 754935f0` - the FlsGetValue
pointer itself. So the accessor's **non-null** branch is the one a started CRT
takes, and the TLS slot holds the FlsGetValue pointer.

Three things follow, and the harness had each of them wrong or missing:

- `0x00F3E498` must hold an **FLS index**, not the file's `-1`. `__getptd` pushes
  it and then calls the FlsGetValue pointer, so `-1` means "ask FlsGetValue for
  slot -1". The harness now allocates one with `FlsAlloc(nullptr)` and publishes
  it: `published the retail CRT FLS index 7`, same value the real process has.
  **Kept** - it is a measured match, not a guess.
- the four pointers at `0x00FB7E18..24` are **encoded** in a started process.
  Encoding them in the harness was tried and made the failure worse (an
  uncatchable stack overflow instead of a caught access violation), so that is
  not the whole story and is recorded as a negative result.
- seeding the TLS slot with the FlsGetValue pointer directly, so the accessor
  takes its non-null branch, was also tried. `TlsSetValue` is confirmed to take -
  a readback immediately after returns the pointer - but by the time the target
  runs, `TlsGetValue` on that index returns 0 again. **Reverted**, because it
  turned a caught fault into an uncaught one. Where the value goes between the
  two points is the next question, and it needs another interactive session.

So `IntrBox2Circle2::Find` still crashes on the retail side, now with the FLS
index matching what a real process has. The remaining gap is one value that
disappears from a TLS slot between being set and being read.

### The next layer down is the CRT's heap, and that is where this stops

Feeding the TLS slot correctly does not help, and re-seeding it immediately
before every call does not help either - tried, and it turns a caught fault into
an uncaught one, so it was reverted. That rules out "the slot gets cleared" as
the explanation and points past it.

What the accessor's null branch actually does:

```
00a95bb4  call eax                 ; FlsGetValue(7) - null, since nothing is set
00a95bb8  test esi,esi
00a95bba  jne  0x00a95c0a          ; present -> done
00a95bbc  push 0x214
00a95bc1  push 1
00a95bc3  call 0x00a95fb9          ; the CRT's own calloc for the per-thread block
```

So a started CRT would already have the block in FLS slot 7. The harness does
not, so the CRT allocates it - through **its own allocator**, and the CRT's heap
is initialised by the same start-up that has never been run. A block from an
uninitialised heap is the next candidate, and it explains why every fix at the
TLS/FLS layer changes the shape of the failure without removing it.

Running the CRT's start-up is not available (see above: calling `__mtinit` kills
the process outright), so the remaining options are to initialise the CRT's heap
separately, or to pre-create the per-thread block and put it in FLS slot 7 so the
allocation never happens. The second is the smaller of the two and is what a
started CRT would have anyway.

This is where the current effort stops. The row stays in the table marked
UNFINISHED, and everything learned is above: the FLS slot order, the real
per-thread state from a running retail process, the pointer decoder, the two
negative results on encoding, and now the layer where the fault actually lives.

### The retail CRT runs now, and `IntrBox2Circle2::Find` can be measured

Three layers of the CRT's per-thread state had to be right, and the last one was
the one that mattered:

1. `0x00F3E49C` - a **TlsAlloc** index. Already published.
2. `0x00F3E498` - an **FlsAlloc** index. Was the file's `-1`, so `__getptd` was
   asking `FlsGetValue` for slot -1. Now allocated with `FlsAlloc(nullptr)`.
3. the per-thread block itself, which a started CRT already has in that FLS slot.
   Without it `__getptd` allocates one through **the CRT's own calloc**
   (`0x00A95FB9`), and that heap is initialised by the same start-up that never
   ran. Pre-creating a zeroed `0x214`-byte block - which is what the CRT's
   `calloc(1, 0x214)` would have made - and putting it in the slot means the
   allocation never happens.

With all three in place the retail side stops dying in the CRT and returns real
answers. That is the thing that had been blocking every row whose implementation
reaches the CRT's math-error path, not just this one.

**The four `0x00FB7E18..24` slots must also be `EncodePointer`d** - and the
earlier note that encoding made things worse was right for the wrong reason. It
made things worse because the FLS index and the per-thread block did not exist
yet: the decoded getter had nothing to return. With all four layers in place the
chain closes, and encoding is what makes it close.

### `IntrBox2Circle2::Find`: the retail runs, and the retail's own gate is odd

`0x00A4F580` is confirmed as slot `+0x0C` of vtable `0x00D71EC4`, and its ABI is
confirmed from its own prologue:

```
SUB ESP,0x1c
MOV EDX,[ESP+0x28]        ; entry [E+0x0C] = arg3 = velocity1
MOV EDI,[ESP+0x2c]        ; entry [E+0x08] = arg2 = velocity0
FLD [EDX] / FSUB [EDI]    ; kVel = velocity1 - velocity0
```

so `ECX = this`, arg1 = `fTMax`, arg2 = `v0`, arg3 = `v1`, `RET 0xC`, bool in AL -
which is what the thunk already does.

Its tail is the interesting part:

```
00a4f8c8  CMP EAX,0x1
00a4f8cb  JZ  0x00a4f8de        ; iType == 1 -> MOV AL,1; RET
00a4f8db  RET 0xc               ; otherwise return with AL as it stands
```

The fall-through returns **the raw `iType` in AL**, so `iType == -1` (the circle is
already intersecting the box - a contact at time zero) comes back as `0xFF` and
the caller reads it as true. The rebuilt `Find` had `if (iType != 1) return false`,
which threw exactly that case away. Changed to `if (iType == 0)`, which is the
same thing retail does, and kept.

That change did not move the number, and the reason is now the whole of the
remaining work: our region helpers compute `iType == 0` where retail computes
something else, on roughly 70% of the generator's draws. A hand-checked sample -
circle 3.72 from the box corner with radius 3.22, moving away, `fTMax == 0` -
says our "no contact" is the right answer and retail's "contact" is not, which
means the divergence is inside `TestVertexRegion`/`TestEdgeRegion`, most likely
in how the comparison flags come out when a discriminant is negative and the CRT
returns a NaN.

The generator was also fixed: it now draws an orthonormal axis, positive extents,
a circle within a couple of box widths and velocities on the box's scale, instead
of `GenIntrBox2Circle2`'s signed extents and arbitrary axis. That alone took the
mismatch count from 19511 to 14096 of 20000.

### The three-argument thunks were pushing the wrong stack slots

`IntrBox2Circle2::Find` answered "contact" on every input, including a circle 200
units away that was not moving. The callee explained it: at the entry to
`TestEdgeRegion` the stack held

```
fCx = -200.0   fCy = 0.0   fVx = 0x7fffffff   fVy = 0xffffffff   fEx, fEy correct
```

`fVx` and `fVy` were NaN bit patterns. Every ordered comparison against a NaN is
false, so `if (fVx <= 0) return 0` fell through and the helper ran its
intersection arithmetic on NaN and returned 1.

The cause was the thunk, not the callee. Both `IntrBox2Circle2Find` and
`IntrBox3Sphere3Find` are declared `(void* self, float fTMax, const float* vel0,
const float* vel1)` and push the three arguments with

```
push dword ptr [esp + 16]
push dword ptr [esp + 16]
push dword ptr [esp + 16]
```

which is the offset you would use with **no** register pushes. With the four
pushes the thunk opens with, ESP sits at `T - 16`, so `self` is `[esp + 20]`
(which was right) and the three arguments are at `[esp + 24]`, `[esp + 28]` and
`[esp + 32]`. `[esp + 16]` is `T + 0` - the **return address**. So retail was
handed a code pointer as `fTMax` and two stack words as the velocity vectors, and
the three reads then walked *down* the stack together, which is why the mistake
survived: it looked self-consistent.

Fixed to `[esp + 32]`. `IntrBox2Circle2::Find` goes from 14096 mismatches in
20000 to **20000/20000 passing**, and 1000000/1000000 across seeds. The `far`
control - circle one box away, static, `fTMax = 0`, which must answer false -
now passes too, where before both sides had "agreed" only because the retail side
was being fed rubbish.

Two consequences worth recording:

- **The previous `IntrBox3Sphere3::Find` number was not a measurement.** Its 4060
  mismatches in 200000 were taken with the same broken thunk, so the retail side
  was running on garbage arguments; with the arguments correct the row reads
  154359, which is what the rebuild actually disagrees by. A row that looks close
  may be two wrong answers cancelling.
- **`Wm3IntrBox2Circle2.cpp` rejects `iType != 1`, and that is right.** An earlier
  change here to `iType == 0` argued that `-1` (circle already intersecting) is a
  contact. Retail's own code says otherwise: `0x00A4F8C8` is `CMP EAX,1 / JZ`, and
  the fall-through path runs `XOR AL,AL` before its `RET 0xc`. The earlier reading
  had missed the `XOR AL,AL`. Reverted, and the row only passes with the original
  form.

Also removes a temporary trace that was added to `Wm3IntrBox2Circle2.cpp` to find
this; it is what produced the `fVx`/`fVy` evidence.

## The last row: `IntrBox3Sphere3::GetContactPoint`

The row was failing on 94% of the draws with "the retail's point and ours are
different points on the box", and the earlier note in this file blamed the
region helpers' arithmetic. Two things were wrong with that account, and both
had to be settled against the retail's own code before the row could close.

### The retail's class, function by function

`0x00A41400..0x00A43600` holds the whole retail class. The functions, with the
call graph the disassembly shows (`ListRange.java` over the range, plus the
`CALL`s inside each body):

| retail | what it is | evidence |
|---|---|---|
| `0x00A414C0` | `GetContactPoint` | `LEA EAX,[ECX+0x14]; RET` - four bytes, it hands back `&m_kContactPoint` and computes nothing |
| `0x00A41530` | constructor | stores the box at `[this+0x0C]`, the sphere at `[this+0x10]`, vtable `0x00D71CFC`, `RET 8` |
| `0x00A41560` | `Test` | vtable slot `+0x08` (`0x00D71D04`) |
| `0x00A41740` | `GetVertexIntersection` | 7 arguments, calls the CRT's sqrt at `0x00A417CE` |
| `0x00A41800` | `GetEdgeIntersection` | 6 arguments, same shape |
| `0x00A41880` | `FindFaceRegionIntersection` | 13 arguments, the last one the `bAboveFace` flag |
| `0x00A41CC0` | `FindJustEdgeIntersection` | 12 arguments, three of them output pointers |
| `0x00A41F50` | `FindEdgeRegionIntersection` | 13 arguments, the last one the `bAboveEdge` flag |
| `0x00A421E0` | `FindVertexRegionIntersection` | 12 arguments, three of them output pointers |
| `0x00A43420` | `Find` | vtable slot `+0x0C`, `RET 0xC`, the bool in `AL` |

The retail keeps the box and the sphere as *pointers* (`this+0x0C`,
`this+0x10`), the rebuild as references - the same layout either way, and the
harness builds the retail object with the retail's own constructor.

The retail's `Find` does not inline the region functions: it calls
`0x00A41740` and `0x00A41800` directly (the vertex and edge intersection
helpers), and `0x00A41F50` and `0x00A421E0` for the edge and vertex regions.

### `Find` reports the inverse of what its name says

This is the reason the row was comparing garbage, and it is in the retail, in
the WM3 copy, and in the rebuild - the rebuild reproduces it, which is why the
`Find` row passes 1,000,000/1,000,000 on the bool.

The region helpers return **0 for a miss, 1 for a hit and -1 for "already
intersecting"** (retail `0x00A421E0` ends its vertex-hit path with `return 1`
and its "not moving towards the box" path with `return 0`). `Find` then does:

```
00a4401b  TEST EAX,EAX
00a4401d  JNZ  0x00a438eb      ; region returned non-zero -> FAILURE
00a44023  JMP  0x00a44059
...
00a438eb  MOV dword ptr [ESI + 0x8],0x0    ; IT_EMPTY
00a438f4  XOR AL,AL                      ; return false
```

So a non-zero region result - which is a *hit* - is turned into
`IT_EMPTY` and `false`. `Find` returns **true** when the region said "no
intersection". The success path (`0x00A44070`) is the one that computes the
contact point and sets `IT_POINT` (`0x00A44116 MOV AL,BL`, with `EBX = 1`).

The rebuild says the same thing in the source it inherited:

```cpp
    if (iRetVal != 0 || m_fContactTime > fTMax)
    {
        m_iIntersectionType = IT_EMPTY;
        return false;
    }
```

The two sides agree bit for bit on that, and the `Find` row proves it.

The consequence for the contact point: on the success path the region helper
returned 0, and a helper that returns 0 never wrote its `rfIx`/`rfIy`/`rfIz`
outputs. `Find` then builds the contact point out of them:

```
m_kContactPoint = m_rkBox.Center + (iSignX*fIx)*m_rkBox.Axis[0] + ...
```

with three uninitialised values. Retail builds the same sum from the same
uninitialised values, out of its own stack - so the row was comparing two
different pieces of garbage on every draw, which is what the "94% of the draws
are different points" measurement was.

There is exactly one path on which the contact point is defined: the sphere
centre inside the box, which the retail takes at `0x00A435F2` with

```
00a435e6  FLD float ptr [EDX + 0x4]      ; the sphere centre
00a435f2  MOV dword ptr [ESI + 0x8],0x8  ; IT_OTHER
```

and which writes the sphere centre into the member. The rebuild writes the
same thing, and the harness now compares the point only on that path - the
intersection type is the gate, not `Find`'s return value, because on the
`IT_POINT` path `Find` returns true *because there was no contact*. The
contact time is written on every path that has a defined value for it, so it
is still compared everywhere.

With that, the row's mismatch count went from 943,423 in 1,000,000 to **756**,
and the 756 were all one ulp in the contact time.

### The retail's square root is a 24-bit one

The remaining 756 draws were the contact time differing by one ulp, in the
`GetEdgeIntersection` / `GetVertexIntersection` helpers. The formula is the
same on both sides - retail `0x00A41800` computes
`fInv*fDiff/(1-fDot*fInv)` with the same groupings - so the difference had to
be in `fInv`.

`fInv` is `1/sqrt(|fDot*fDot - fVSqr*fDiff|)`, and the retail takes the square
root through the CRT's `sqrt` (`0x00A91000`), which **branches on the x87
control word**:

```
0x00a91028  66 81 3c 24 7f 02      CMP word [ESP], 0x027F
0x00a91030  e8 a0 6b 01 00         CALL 0x00F97BD5
```

`0x027F` is the Windows default control word, not the engine's `_PC_24`
(`0x007F`). Under `_PC_24` the CRT takes the other path, and the square root
it returns is the x87's `FSQRT` at 24 bits - not a correctly rounded double.
The rebuild's `std::sqrt` on a double *is* correctly rounded, so the two square
roots differ, the reciprocals differ by an ulp, and the time of impact moves
by an ulp on about 0.4% of the inputs.

Checked numerically on the first failing draw (`iter 818` of the default run):
with the argument `10780614656.0`, the correctly rounded double square root
gives a reciprocal of `0x3721957C`, the 24-bit one gives `0x3721957B`, and the
retail's contact time (`0x3EDCA495`) is the one that follows from
`0x3721957B`. The rebuild's (`0x3EDCA494`) follows from `0x3721957C`.

The fix is in the rebuilt source, in both helpers
(`Wm3IntrBox3Sphere3.cpp`): the square root is taken in float precision and
the division is a float division, so the whole reciprocal is the x87's
24-bit `round24(1/round24(sqrt(x)))`:

```cpp
    const Real fInv = ((Real)1.0)/((Real)std::sqrt(
      Math<Real>::FAbs(fDot*fDot-fVSqr*fDiff)));
```

`Math<Real>::InvSqrt` still cannot be used for it: that one models retail's
*standalone* `InvSqrt` (`0x0050D150`), a different function with a different
rounding path.

The row passes 200,000/200,000 after that, and 1,000,000/1,000,000 on the
default seed.

### What the WM3 copy says

`dependencies/WildMagic3p8/Foundation/Intersection/Wm3IntrBox3Sphere3.cpp` is
a faithful copy of `.tmp/wm3/Source/Intersection/Wm3IntrBox3Sphere3.cpp`: the
same branches, the same comparison operators, the same constants, the same
term orders (including `fVSqrX = fVz*fVz + fVx*fVx`, `(-fDz + Radius)/fVz` and
the two different `fVy*m_fContactTime` orderings), and the same member write
order. The only semantic differences are the two `fInv` statements discussed
above. So this row was never a translation error - it was the retail's
arithmetic, and the square root's precision was in it.

## Two more classes: the segment-sphere and the box-box

`Wm3::IntrSegment3Sphere3f` and `Wm3::IntrBox2Box2f` are the next two
classes the engine calls and the rebuild emits out of line. Both are anchored
by a direct CALL from an annotated retail function, which is what the scope
rule asks for.

### `IntrSegment3Sphere3<float>`

| retail | what it is | evidence |
|---|---|---|
| `0x00A46A10` | constructor | direct CALL from `Moho::CColPrimitive_Sphere::CollideLine` (0x004FE9D0) at 0x004FEA13; annotated at `src/sdk/moho/collision/CColPrimitiveBase.cpp:569` |
| `0x00A471C0` | `Find()` | direct CALL from the same function at 0x004FEA27, and the class vtable's +0x10 slot |
| `0x00A46940` | `GetPoint(int)` | three CALLs from the same function (0x004FEA3A, 0x004FEA98, 0x004FEAED) |

The retail's constructor writes the vtable `0x00D71E0C`, the two references at
`[this+0x10]` and `[this+0x14]`, the 1e-6f `ZeroThreshold` at `[this+0x0C]`
and `m_iQuantity = 0` at `[this+0x18]`. The rebuilt class has the same
layout - `ZeroThreshold` is a public member declared before the private
section, so it lands at `0x0C` there too.

`Find()` is a better row than the box-sphere one because everything it writes
is defined: `m_iQuantity` at `[this+0x18]` (0, 1 or 2) and the intersection
points at `[this+0x1C]` and `[this+0x28]`, and the bool it returns is
`m_iQuantity > 0` (`SETG AL`).

Two things had to be fixed in the rebuilt source, and both are the same two
effects the box-sphere row turned up:

- **The two dot products are inlined in the retail, and they add their three
  terms x, y, z** - not the z, y, x the shared `Vector3::Dot` uses. The
  grouping is what the rounding rides on, and one ulp in the discriminant is
  the difference between "hit" and "miss" when the discriminant sits at zero.
  With the shared `Dot` the row failed on 0.285% of the draws, in both
  directions; with the sums spelled out it passes.
  This is the same per-call-site effect the collision slice found: the
  standalone `Dot` at 0x00452AD0 sums z, y, x, while the dots inlined in
  other functions sum x, y, z. One shared `Dot` cannot be right for both, and
  the rebuild's `Find()` now spells its two sums out.
- **The square root is a 24-bit one**, for the same reason as in
  `IntrBox3Sphere3`: retail's CRT sqrt branches on the x87 control word and
  under `_PC_24` returns the `FSQRT` result, not a correctly rounded double.
  `Math<Real>::Sqrt` is a double square root, so the root - and with it the
  intersection point - differed by an ulp on 6.5% of the draws.

The generator places the segment relative to the sphere's centre rather than
drawing it blindly, so the three quantities the quadratic depends on are
controlled: the perpendicular distance `m` from the centre to the segment's
line (`m > R` misses, `m = R` is tangent, `m < R` cuts), the signed offset `t`
of the origin along the direction, and the extent `e`, drawn independently of
`t` so "the segment stops short of the sphere" and "the segment spans it" are
both common. `m = R` is forced 10% of the time, because that is the
`ZeroThreshold` branch and it is the one random geometry would never reach.

### `IntrBox2Box2<float>`

| retail | what it is | evidence |
|---|---|---|
| `0x00A47BF0` | constructor | direct CALL from the AI steering test (0x00596930) at 0x00596DB2; annotated at `src/sdk/moho/ai/CAiSteeringImpl.cpp:268` |
| `0x00A47D40` | `Test()` | direct CALL from the same function at 0x00596DC3, and the class vtable's +0x0C slot |

`Test()` is the separating-axis test on two oriented boxes, and it needed no
source change at all: the rebuilt `Test()` agrees with the retail on every
draw, including the `FCOMPP`/`JP` NaN handling - the retail jumps to the
next axis when the comparison is unordered, which is what the rebuilt
`if (fAbsAdD > fRSum) return false;` does with a NaN.

### The segment-sphere class, the rest of it

The class's vtable is `0x00D71E0C`, and its four virtuals are:

| slot | retail | what it is |
|---|---|---|
| +0x04 | `0x00A46B10` | the swept `Find(fTMax, v0, v1)` - `RET 0xC` |
| +0x08 | `0x00A46C80` | the static `Test()` |
| +0x0C | `0x00A46DB0` | the swept `Test(fTMax, v0, v1)` - `RET 0xC` |
| +0x10 | `0x00A471C0` | the static `Find()` - the one the engine calls |

The static `Test()` is a new row, and it needed the same fix as `Find()`: the
rebuilt `Test()` summed its two dot products with the shared `Vector3::Dot`
(z, y, x) where the retail inlines them and sums them x, y, z. With the
shared `Dot` the row fails on the draws where the discriminant sits at zero,
in both directions; spelled out, it passes.

### The swept rows: the capsule chain

The two swept rows are wired and they fail on 1.2% of the draws, in both
directions. What is known:

- The retail's swept `Test` (0x00A46B10, the vtable's +0x04 slot) calls the
  static `Test` through the vtable first (`MOV EDX,[EAX+8]; CALL EDX`) and
  then reduces the swept sphere to a segment-versus-capsule test: it calls
  `Vector3<float>::Normalize` out of line (0x00452AF0, a row that passes),
  the `IntrSegment3Capsule3` constructor (0x00A6BE80) and then the capsule's
  own `Test` **virtually** (the vtable slot +0x08, `MOV EAX,[EDX+8]; CALL
  EAX`). The rebuilt code does the same three things in the same order, so the
  shapes agree.
- The retail's capsule `Test` (0x00A6BEB0) builds a
  `DistSegment3Segment3` (constructor 0x00A7E190), calls its virtual `Get()`
  (the vtable slot +0x0C, 0x00A7E230) and compares the result against the
  capsule's radius at `[capsule+0x1C]`. The rebuilt code does the same.
- **The difference is in `DistSegment3Segment3::GetSquared` - retail
  0x00A7E660.** The rebuilt file carries the **WM3.4 body**: it is
  byte-for-byte the copy in `.tmp/wm3/Source/Distance/Wm3DistSegment3Segment3.cpp`
  (the only difference in the whole file is the four dot products, below), and
  the git history has one commit for it, so the file has been the WM3.4 body
  since the tree was imported. The retail's body is a different formulation
  of the same quantity, and it is not a rounding difference: on the first
  failing draw the rebuilt distance is 0.067 against a radius of 1.922 - the
  rebuild says "hit" by a margin of 1.855, and the retail says "miss". No
  ordering of the sums can explain that.

What was ruled out, by measurement:

| experiment | result |
|---|---|
| the four dot products in x, y, z instead of the shared `Vector3::Dot`'s z, y, x | 2,333 -> 2,332 mismatches: not the cause |
| the two negated dots summed z, x, y (what retail's decompilation suggests) and the other two x, y, z | 2,333 -> 2,332: not the cause |
| the square root in float precision instead of `Math<Real>::Sqrt`'s double one, in `Wm3IntrLine3Capsule3.cpp` and `Wm3DistLine3Segment3.cpp` | 2,333 -> 2,333: not the cause - and the retail's `Get()` takes the CRT's *double* sqrt (0x00A91000) and narrows it, which is what `Math<Real>::Sqrt` does, so the change was reverted |
| the two branches of the rebuilt `GetSquared` swapped (the parallel formula and the region analysis) | 2,332 -> 2,319: not the cause - the two bodies agree with each other, and neither is the retail's |
| the swept velocity forced parallel to the segment, so both sides take the "parallel" branch | 2,579 -> the same rate: the difference is not in the branch selection either |

So the remaining work is a **port of retail's WM3.8 `GetSquared` body** into
the rebuilt tree - the same kind of job as the WM3 corrections the tree
already carries, but a whole function rather than a line. The retail's body
is at 0x00A7E660, its decompilation is in the Ghidra project, and the
rebuilt file to change is
`Foundation/Distance/Wm3DistSegment3Segment3.cpp`. The rows stay in the table
meanwhile, because they are located and the disagreement is a measurement,
not a canary.

**The two rows were also the wrong way round.** Reading the bodies settles it:
0x00A46DB0 computes the contact time and the contact point (it takes the
segment parameter from the capsule with `GetParameter(0)`, 0x00A6BE00, and
writes the sphere centre plus that parameter times the swept direction into
`m_kContactPoint` at `[this+0x14]`), so it is the swept **Find**; 0x00A46B10
answers with a bool and nothing else, so it is the swept **Test**. The table
had them the other way round.

### What the retail's disassembly says about the identification

The chain was re-checked from the retail's own code, and it holds:

- `0x00A7E230` is `DistSegment3Segment3<float>::Get()`. Its whole body is
  seven instructions: load the vtable, dispatch slot **+0x18**, call the CRT's
  sqrt (0x00A91000) and narrow the result to a float. So the vtable's +0x18
  is `GetSquared()` and +0x0C is `Get()`, which is the layout the README
  already records for the Distance classes - and the retail's `Get()` takes the
  CRT's **double** square root and narrows it, which is exactly what
  `Math<Real>::Sqrt` does. The float-precision square root that was tried in
  the capsule chain was therefore the wrong model, and it is reverted.
- `0x00A7E660` is `DistSegment3Segment3<float>::GetSquared()`: a
  self-contained body with no calls, reading the two segments at `[this+0x34]`
  and `[this+0x38]`, and computing the same four quantities the WM3 body
  computes - the negated direction dot, `kDiff . dir0`, the negated
  `kDiff . dir1` and `|kDiff|^2`.
- **The dots are not in one order.** The first one, the negated direction dot,
  adds its three terms **z, x, y** (the z term's product is the first one
  computed, at 0x00A7E686, and the y term's is the last, at 0x00A7E694),
  while the rebuild's shared `Vector3::Dot` adds z, y, x. The per-call-site
  order is the same phenomenon as in the segment-sphere row, and it is a
  sub-ulp effect: it cannot explain a distance that is wrong by 1.855.

So the function was identified correctly; what differs is its **body**, and the
port has to be made against the assembly rather than against Ghidra's
expression tree, which re-associates the x87 sums (it got the segment-sphere
row's order wrong the same way).

### The algorithm difference, region by region

The retail's body and the rebuilt one compute the same four quantities and
then the same distance, but they do not compute them the same way. What the
assembly and the decompilation between them show:

**The inputs are the same.** `kDiff = seg0.Origin - seg1.Origin`, the negated
direction dot, `kDiff . dir0`, the negated `kDiff . dir1` and `|kDiff|^2` -
the same four, with the same signs.

**The dots are not summed in one order.** The negated direction dot adds its
three terms **z, x, y** (the z term's product is the first one computed, at
0x00A7E686, the y term's is the last, at 0x00A7E694), and `kDiff . dir0`
adds them **y, x, z** (the y term's product is first, at 0x00A7E6B3). The
shared `Vector3::Dot` sums z, y, x. The retail's four dots are therefore all
*different sums* from the ones the rebuilt body uses.

**The region formulas are algebraically different.** The region where both
parameter ranges contain the closest point - the rebuilt body's "region 0
(interior)" - is

    rebuilt (WM3.4):  fS0*(fS0 + fA01*fS1 + 2*fB0)
                     + fS1*(fA01*fS0 + fS1 + 2*fB1) + fC
    retail:          ((2*fB0 + fS0)*fS0 - fS1*fS1) + fC

The retail's has no `fA01` term at all: it is a different algebraic form of the
same distance, and the two round differently. The same is true of the other
regions - the rebuilt body has eight of them, the retail's body has the same
branch conditions (`-e0*fDet < fS0`, `-e1*fDet < fS1`, the clamps against
`e0*fDet` and `e1:fDet`) but different formulas inside them.

**The parallel case is the same formula.** The rebuilt body's "segments are
parallel" branch and the retail's parallel branch compute the same thing:
`fE0pE1 = e0+e1`, `fSign` from the sign of the negated direction dot,
`fLambda = -fB0` clamped to `+-fE0pE1`, `fS1 = fSign*fB0*e1/fE0pE1`,
`fS0 = fLambda + fSign*fS1`, and
`fSqrDist = fLambda*(fLambda + 2*fB0) + fC`. The retail reaches it through
the same `fDet < 1e-6` test.

**The square root is the double one.** The retail's `Get()` takes the CRT's
sqrt (0x00A91000) of the squared distance and narrows the result to a float,
which is what `Math<Real>::Sqrt` does - so the rebuilt body was right about
that and the float-precision change that was tried in the capsule chain was
the wrong model.

What that means for the port: the rebuilt `GetSquared` has to keep the four
quantities and the branch conditions and take the retail's formulas and its
per-call-site dot orders. The branch conditions agree, so the port is a
formula-level rewrite of the eight regions plus the four dot sums, not a
restructuring.

The measurements that pinned this down, none of which moved the count:

| what was tried | result |
|---|---|
| the region-0 formula replaced by the retail's | 2,333 -> 2,334: the region-0 formula is not where the draws differ |
| the negated direction dot summed z, x, y and `kDiff . dir0` summed y, x, z | 2,333 -> 2,331: the dot orders are not where the draws differ either |
| the two branches of `GetSquared` swapped, so the parallel formula runs when the segments are not parallel | 2,333 -> 10,362: the retail does **not** do that, and this is what rules the arrangement out |
| the square root in float precision in the capsule chain | 2,333 -> 2,333, and the retail's `Get()` shows the double square root is the right model, so it was reverted |

So the remaining port is the other seven regions' formulas and the last two
dots' orders, and it is a formula-level change to one function.



### Two classes that are located but not wired

`Wm3::InBox<float>` is retail `0x00A3BC10` - the engine's own comment says
so: "Calls Wild Magic's `InBox<float>` (0x00A3BC10, Wm3ContBox3.cpp) with the
point and `mShape`" at `src/sdk/moho/collision/CColPrimitiveBase.cpp:447`. It
is not wired, and the reason is the harness's build rather than the
comparison: the rebuilt `InBox` lives in `Wm3ContBox3.cpp`, whose other
functions (`ContConvexHull3`, `ContMinBox`) pull the convex hull, the
eigen-decomposition and the approximation machinery into the harness - the
link then wants a dozen more library sources, and the harness deliberately
compiles only the handful it needs.

`Wm3::ContAlignedBox<float>` has no address yet. Its anchor is the engine's
`AxisAlignedBoundsFromBox` at 0x004FE2D0, and walking from there is the next
step.

The engine's own Wild Magic surface, for the record: every `Wm3::` function
the engine calls is either a row in the table or one of the two groups the
README lists as out of scope. `DistVector2Box2`, `DistSegment3Segment3`,
`IntrSegment3Capsule3` and `IntrLine3Capsule3` are reached only from inside
Wild Magic, so their retail addresses come from walking the already-anchored
bodies rather than from an engine call site; `IntrLine3Capsule3` is not
called at all on either side and is dead.

### The two-argument constructor thunks were reading the wrong slot

While wiring the new constructors it became clear that the five existing
two-argument constructor thunks read their *second* argument from
`[esp + 12]`. With the four register pushes ESP sits at T - 16, so the
second argument is at `[esp + 28]` (T + 12); after the first push has moved
ESP to T - 20, the first argument is at `[esp + 28]` as well (T + 8).
`[esp + 12]` is T - 8 - the saved ESI, not an argument at all.

Those thunks worked because the caller happened to leave the first argument
in ESI at the call site, which is a register-allocation accident and not a
stack-slot fact. They now read `[esp + 28]` twice, like the three-argument
constructor already did, and every row still passes 1,000,000/1,000,000.

The rule is the one the three-argument thunk already documented: each push
moves ESP down by 4 while the argument being read stays where it is, so every
read lands on the same offset.

### The two classes' Wild Magic 3.8 ancestry

`.tmp/wm3/Source/Intersection/` has both classes' WM3 copies
(`Wm3IntrSegment3Sphere3.cpp`, `Wm3IntrBox2Box2.cpp`), and the rebuilt
versions follow them. The `IntrBox2Box2` test is the same separating-axis
test in the same order, and the segment-sphere `Find()` is the same
quadratic; the differences that mattered were the two above, and both were in
the arithmetic rather than in the algorithm.

### The segment order, and why the port has to come from the assembly

Two things were established since, and one of them changes how the port has
to be done.

**The two segments go in the other order.** The retail's
`IntrSegment3Capsule3::Test` (0x00A6BEB0) pushes the **capsule** first and
the segment second (0x00A6BEDC), so in the retail's `DistSegment3Segment3` the
capsule is segment0 and the box's segment is segment1. The rebuilt body
passes them the other way round. With the WM3.4 body that makes no
difference - that body is symmetric in the two segments, and the count is
2,333 either way - but the WM3.8 body is **not** symmetric (its interior
formula has no `fA01` term, and its "vertex" formula is the mirror of the
interior one, not the same one), so with the WM3.8 body the order decides
the answer. The rebuilt `IntrSegment3Capsule3::Test` now passes the capsule
first, which is the retail's order and a no-op against the WM3.4 body.

**The port cannot be made from Ghidra's expression tree.** Transcribing the
decompiled region analysis wholesale made the row *worse*, 2,333 -> 42,581,
and 25,339 with the argument order corrected - so the transcription was
faithful to the tree, and the tree is not faithful to the machine. Ghidra
re-associates the x87 sums, which is the same thing that made its rendering
of the segment-sphere row's dot order wrong. The port has to be read out of
the assembly instruction by instruction, the way the segment-sphere row's dot
order was.

The structure to port is known and is in the decompilation: the same branch
conditions as the WM3.4 body (`-e0*fDet < fS0`, `-e1*fDet < fS1`, the clamps
against `e0:fDet` and `e1:fDet`) and, inside them, the retail's formulas -
the interior one `((2*fB0 + fS0)*fS0 - fS1*fS1 + fC` with the `1/fDet`
scaling, its mirror `(2*fB1 + fS1)*fS1 - fS0*fS0 + fC` for the vertex case,
and the side case `fS0*(fS0 - 2*fTmp) + fS1*(fS1 + 2*fB1) + fC`, which is
the same shape as the WM3.4 side formula. The two dots whose order is known
are the negated direction dot (z, x, y) and `kDiff . dir0` (y, x, z); the
other two still have to be read out of the assembly.

So the port is: the WM3.8 body, from the assembly, in the retail's argument
order.

### The Wild Magic 4 SDK is on this machine, and it is not the retail's body

`.tmp/wm4ref/` is the Wild Magic 4 SDK. Its
`LibFoundation/Distance/Wm4DistSegment3Segment3.cpp` carries a `GetSquared()`
that is **byte for byte the body the rebuilt tree has** - so the WM4 source is
not the retail's either, and the retail's is the Wild Magic 3.8 formulation,
which exists nowhere but in the binary.

Two things came out of reading the two against the machine code with capstone
(capstone 5.0.9 is installed in `.tmp/wmdiff/venv`; `.tmp/wmdiff/disasm.py`
disassembles a range of the retail image):

- The four three-term sums are inlined in the retail and add their terms
  **y, x, z** - the z, y, x the shared `Vector3::Dot` uses is the *standalone*
  `Dot` at 0x00452AD0, not the inlined one. The rebuilt body now spells the
  four sums out that way. It moves the count by one draw on its own: the
  difference is not in the sums.
- The retail's interior region contains an `fsubrp` - a subtraction - which
  the WM4 region 0 does not have. That is the difference: the retail's
  interior formula is a different algebraic form of the distance, and it is
  only in the machine code.

So the last piece of the port is a reading of the retail's own instructions,
and the reliable way to do that is to execute them rather than to transcribe
them: an x87 interpreter over 0x00A7E660 gives the retail's fSqrDist and its
intermediates for any input, which is an oracle the formulas can be fitted
against without a rebuild per experiment.

## `IntrSegment3Sphere3::Test(swept)` / `Find(swept)` — how the two rows were fixed

These two rows spent several rounds failing at ~1.2% and ~0.06%. The cause was
**in the harness, not in the rebuilt source**, and finding it invalidated a whole
line of investigation that had looked very productive.

### Two bugs in the harness thunks

`wmdiff/src/wm3_coll_thunks.cpp`, `IntrSegment3Sphere3TestDynamic`:

1. **Wrong function.** It called `0x00A46DB0`, the swept **Find**, not
   `0x00A46B10`, the swept Test that the row declares and that the class
   vtable's `+0x04` slot holds. So the row was comparing retail's swept Find
   against the rebuild's swept Test. The Find applies the contact-time test
   against `fTMax` on top of the geometry, and the generator sets `fTMax` to 0
   in half the draws and to `4*radius/speed` otherwise, so retail's answer came
   back FALSE nearly always.

2. **Wrong argument order.** It pushed `vel1, vel0, fTMax`. The callee's arg1 is
   the value pushed **last**, so retail saw `arg1 = fTMax, arg2 = vel0,
   arg3 = vel1`. Retail's swept path computes `Delta = arg2 - arg1`
   (`0x00A46B68`: `fld [eax]` / `fsub [ecx]`, with `eax = [ESP+0x90]`,
   `ecx = [ESP+0x8C]`), so the relative velocity came out as `vel0 - fTMax` —
   garbage. The push order must be `fTMax, vel1, vel0`, which leaves
   `arg1 = vel0, arg2 = vel1, arg3 = fTMax`.

Both thunks now push `fTMax, vel1, vel0`; `TestDynamic` calls `0x00A46B10` and
`FindDynamic` keeps `0x00A46DB0`.

**A row's declared retail VA is documentation only — the thunk is what actually
runs.** They disagreed here for several rounds, and nothing about the row's
output looked like "wrong function": it produced plausible-looking numbers, and
the harness reported them faithfully.

### Swept signature difference (WM3.8 vs WM3.4) — engine-side API mirroring is OUTSTANDING

Retail 3.8's swept methods take `(vel0, vel1, tMax)`; the rebuilt WM3.4 API
takes `Test(fTMax, velocity0, velocity1)`. **This difference is currently
bridged inside the wmdiff thunk only.** The engine's own
`Wm3IntrSegment3Sphere3.h` and its callers still use the WM3.4 argument order and
have **not** been changed to mirror retail. If the engine is ever to reproduce
retail's swept behaviour through its own API rather than through the harness,
that mirroring is required and is not done.

### `_PC_24`: every retail x87 computation rounds to float, per operation

The rows run under the harness's own FP environment (`wmdiff/src/main.cpp:160`,
`SetFpEnv`): x87 control word `0x007F`, i.e. `_PC_24`. The banner's `CW=0x027F`
is the *live* control word after the retail CRT's own initialisation, not the one
the rows run under. Consequently **every** x87 operation in the retail rounds to
a 24-bit significand, so retail's arithmetic chains are per-operation float32
chains and **not** 64-bit extended. An earlier revision modelled them as
`long double`; that modelled the wrong precision control and was wrong.

### `DistSegment3Segment3<float>::GetSquared` (retail `0x00A7E660`)

- The four three-term sums are **inlined** (one body, not four calls to the
  standalone `Vector3::Dot` at `0x00452AD0`) and they add their terms in the
  order **y, x, z** — not the z, y, x the shared `Dot` uses. The disassembly is
  unambiguous: `0x00A7E686` loads `[segment1+0x10]` (y) and multiplies by
  `[segment0+0x10]`, `0x00A7E68C` does `+0x0C` (x), `0x00A7E694` does `+0x14`
  (z), and the accumulated sum is negated only at `0x00A7E6A4`. The other three
  have the same shape: `fB0` at `0x00A7E6AA`, `fB1` at `0x00A7E6D3`, `fC` at
  `0x00A7E6F4`. All four round per operation.
- Spill-slot map: `[ESP+0x08]` `fA01` → `fDet` → `fS0`, `[ESP+0x04]` `fB1` →
  `fS1`, `[ESP+0x10]` `fB0` → `fExtDet1`, `[ESP+0x14]` `fExtDet0`,
  `[ESP+0x0C]` `fC`.
- The region blocks consume **register-held** values, not the spilled ones. The
  region-4 block at `0x00A7E86A` takes no memory operand except the extent, its
  own spill of that extent, and `fC` — everything else comes off the x87 stack.
  So the `fB1` question resolves cleanly: under `_PC_24` a register value *is*
  the per-operation float value, the header needs no special case, and the
  extended `0xC01420EF` seen in a model was the model's own value, not something
  the blocks consume.
- `Mul24` / `Add24` / `Div24` (`Foundation/Math/Wm3Vector3.h:76-95`) spell each
  multiply and add as its own rounded step in the order the machine code performs
  it, so reassociation is impossible regardless of what the optimiser does with
  the source expression. Measured: region 0 and region 4's middle branch are now
  spelled this way, and neither move `sqDiff` — `/fp:precise` was already
  preserving the grouping, so the remaining difference is not a reassociation.
- `Get()` (`0x00A7E230`) is `fl(crt_double_sqrt((double)fSqrDist))`: it calls
  `GetSquared` via the vtable slot `+0x18`, then `0x00A91000`, which stores its
  argument as a double (`0x00A91003`) and calls the CRT double sqrt `0x00AA7C48`,
  and the result is narrowed to float. This is **not** `Math<float>::Sqrt`
  (`0x00452FC0`, a raw `fsqrt`), and the two differ by a few ulp. `Math<Real>::Sqrt`
  is deliberately unchanged — it is calibrated for its own row, which passes.

### Method lesson

The single most expensive result in this work was `radius0 113 of 114` — a
grid-scan candidate that appeared to say the capsule's radius was being read as
zero. It was scoring candidates against an answer that was almost always FALSE,
because the row was calling a Find. **A candidate that makes the comparison
false matches nearly always when the reference answer is nearly always false.**
The scan was not measuring the capsule; it was measuring the harness bug.

What made the bug findable was refusing to publish a grouping that could not be
justified from the assembly, and keeping the narrow counters alive throughout:
`earlyDiff 0`, `lenDiff 0`, `capDiff 0`, `sqrtDiff 0`, `ours inconsistent 0`,
`retail inconsistent 0`. Those zeroes were the honest signal that the geometry
was fine and the disagreement lived elsewhere — and once the thunks were fixed,
`sqDiff` went from 19 of 245 disagreements to **19 of 19**, i.e. every remaining
disagreement is in `GetSquared` and none of it is in the swept geometry.

## The x87 register-form semantics, measured on the hardware

The simulator for retail's `DistSegment3Segment3<float>::GetSquared`
(`0x00A7E660`) was written from the SDM's opcode table and was wrong in four
ways, each of which cost a factor of ten in the number of draws it reproduced.
All four are now settled by *measuring the hardware*, not by reading the
manual: `.tmp/wmdiff/x87hw32.c` builds a two-byte buffer in executable
memory, loads a known register file with six distinct doubles, executes the
instruction under test, and dumps the top four registers. The encoding is
therefore exactly what is being asked about, and the answer is the hardware's.

What the hardware says (rm = 1, so ST(0) = 600, ST(1) = 500, ST(2) = 400):

| opcode | reg | meaning | destination | pops |
|---|---|---|---|---|
| D8 | 0 | `FADD ST(0),ST(i)` | ST(0) | 0 |
| D8 | 1 | `FMUL ST(0),ST(i)` | ST(0) | 0 |
| D8 | 2 | `FCOM ST(i)` | - | 0 |
| D8 | 3 | `FCOMP ST(i)` | - | 1 |
| D8 | 4/5 | `FSUB` / `FSUBR`, `ST(0) := ST(0) - ST(i)` / `ST(i) - ST(0)` | ST(0) | 0 |
| D8 | 6/7 | `FDIV` / `FDIVR` | ST(0) | 0 |
| DC | 0/1 | `FADD` / `FMUL ST(i),ST(0)` | **ST(i)** | 0 |
| DC | 2/3 | `FCOMP ST(i)` | - | 0 / 1 |
| DC | 4..7 | sub/div family | **ST(i)** | 0 |
| DE | 0/1 | `FADDP` / `FMULP ST(i),ST(0)` | ST(i) | 1 |
| DE | 2/3 | `FCOMP ST(i)` / `FCOMPP` | - | 1 / 2 |
| DE | 4/5 | `ST(i) := ST(0) - ST(i)` / `ST(i) - ST(0)` | ST(i) | 1 |
| DE | 6/7 | `ST(i) := ST(0)/ST(i)` / `ST(i)/ST(0)` | ST(i) | 1 |

The three that the SDM's summary tables do not make obvious, and that a
disassembler's mnemonic does not settle either:

* **D8's arithmetic forms do not pop**, and D8's `E`/`F` forms write **ST(0)**
  (not ST(i)). A disassembler prints one operand for them (`fmul st(4)`), which
  reads like "ST(4) is the destination" and is wrong.
* **DC writes ST(i) and does not pop.** DC is the reversed-operand, non-popping
  twin of D8; DE is the popping one. So `DC C8+i` is `ST(i) := ST(i)*ST(0)`
  with the stack unchanged, and `DE C8+i` is the same with a pop.
* **`FSTP ST(i)` stores ST(0) into ST(i) and *then* pops** - the store happens
  first, so after the pop the value that was ST(i) is on top. Storing ST(i)
  into ST(i) and popping (what a "store and pop" reading suggests) is a
  different instruction, and the simulator had it, which moved every value on
  the stack by one slot.

Two more simulator bugs, both of which the hardware settles the moment the
trace is compared with the disassembly:

* **`TEST` sets the integer flags.** The retail's idiom is
  `fnstsw ax; test ah, 0x41; je/jp`. `je` and `jne` must therefore test
  ZF from the `TEST`'s AND result, and `jp` must test PF from its parity -
  not the FPU's C0 bit. With `test ah, 0x41`, PF is set exactly when C0 and
  C3 agree, which is what makes the retail's "unordered or out of range"
  branch work.
* **Rounding twice is not rounding once.** Rounding a product to 25 bits and
  then to 24 lands one ulp away from rounding it to 24 directly, and it was
  worth 1 ulp in `fB1` - which is one of the values a clamp comparison is made
  against.

### The precision: the retail's chain is 53-bit, not 24-bit

> **Retracted.** This section's conclusion is wrong: the probe that produced it
> had the rebuilt `GetSquared` running between its `fldcw` and the retail call,
> which reset the control word, so the retail was measured at `0x027F` while
> the rows compare it at `0x007F`. The rows' environment is `_PC_24` and the
> retail's chain is per-operation float32. See "The swept rows, closed" at the
> end of this file for the measurement that settles it.

With the decode correct, the simulator still disagreed with the retail on
two thirds of the draws, and only when the probe was made to run under the
*row's* control word did the two agree.

The rows run under `main.cpp`'s `SetFpEnv`, which sets the x87 control word to
`0x007F` (`_PC_24`). The probe (`RunSegmentSegmentProbe`) did **not** do that:
left alone it inherits the ambient CRT control word (`0x027F`/`0x037F`,
`_PC_53`), so it was measuring the retail in a different floating-point
environment than the rows compare it in. 80 of 400 sampled draws changed when
the probe was given the same control word as the rows.

That is not a cosmetic difference. `fB0` and `fB1` land one ulp apart between
the two chains, and the region code compares `fTmpS0` against the extents, so
a one-ulp difference in the header is enough to flip a clamp comparison and
change the squared distance. With the probe running under `0x007F`, the
simulator, with the register file at 53 bits, reproduces the retail's
`fSqrDist` **bit for bit on 400 of 400 sampled draws** - which is the
validation the whole simulator was built for. Its trace of a region block is
now evidence.

### The rebuild's own floating-point codegen is layout-dependent

Getting the probe to run under `0x007F` is not as simple as setting the
control word before the probe call, and the reason is worth recording.

* Setting the control word *inside* the probe function works: the draws come
  out as the 53-bit chain, and the simulator matches them 400 of 400.
* Setting it in `main.cpp` before the probe call does **not**: the probe's
  draws come out on the ambient chain again (321 of 400), because something
  in the probe's own path resets the control word. The rebuild's
  `GetSquared` is the obvious candidate - MSVC's x87 code for `float`/`double`
  expressions can `fldcw` to select the precision it wants and restore it
  afterwards.
* And the first of those two - adding a few instructions to
  `wm3_coll_targets.cpp` - moves `IntrSegment3Sphere3::Test(swept)` from
  **606** mismatches in 1 000 000 iterations to **1705**, with
  `Find(swept)` unchanged at 545. The probe changes no row and compares
  nothing itself; the only way it can move a row is by changing the
  *rebuild's* code generation, because the rebuild's `GetSquared` is
  instantiated and inlined in that translation unit.

So the rebuild's squared distance is not a stable function of its source: the
same source gives two different answers depending on what else is in the
translation unit, because the compiler picks between x87 (53-bit
intermediates) and SSE (24-bit intermediates) by inlining and layout, and
because an `fldcw` in the middle of the probe's draws changes the chain the
retail is measured on. That is a bigger problem than the 19 residual
disagreements: the numbers the rows report are partly a property of the
build, not only of the code. The rebuild's region chains have to be written so
that their precision is fixed by the source (an explicit `double`, with the
spilled values staying `Real`), and that is the change to make next.

The lesson is the same one the `radius0 113 of 114` grid scan taught, in a
new costume: **an oracle measured in the wrong environment measures a
different function.** The header values, the region branches and the final
distance are one function; they are only comparable when both sides run under
the same control word.

### The symbolic tracer: the simulator's trace *as an expression*

The simulator answers "what number does the retail produce", but a number is
not a transcription. To get the *block's arithmetic* - the expressions, the
operand order, the rounding - the simulator's own trace is walked by a
symbolic post-processor, `wmdiff/tools/symtrace.py`, driven by
`wmdiff/tools/symrun.py`. Every register carries two things at once: its
numeric value (computed with the simulator's rules) and its expression text.
A load of a slot the tracer has written symbolically yields that slot's
expression again, so a spill and reload does not erase the expression; a spill
also rounds the *value* to float32, because the retail spills to 32-bit slots.

The tracer is not a second, weaker oracle - it is checked against the
simulator at every step:

* before every instruction the tracer's live register values are compared
  with the simulator's own snapshot of the register file, and the first
  disagreement is reported with its address. On the sampled draws there is
  none, so the tracer's decode and the simulator's decode agree step for step;
* the expression it ends with evaluates to the retail's `fSqrDist` bit for
  bit, on every sampled draw (400 of 400 with the register file at 53 bits).

That is what licenses the transcription: the expression is not a reading of
the disassembly, it is the simulator's own arithmetic written down.

Two bugs in the tracer are worth recording, because both produced plausible
output rather than an error:

* **A missing 64-bit load.** The retail's interior loads the double `2.0`
  from `0x00D71CD8`. The tracer read it as zero, which was one ulp away in
  every second term of the interior - and the tracer still produced a
  *plausible* expression.
* **`self.n = 0` orphaned into a method.** A patch inserted the new
  `compare()` method above the constructor's last line, so the live-depth
  counter was never initialised. Every push and pop raised, was swallowed by
  the tracer's own `except`, and the register file was still updated - so the
  expressions and the values were still right, while the tracer reported
  "unhandled instruction" on the first load of every draw. The symptom was
  visible only because the errors were printed.

### The names the expression is given

The trace is in addresses; the transcription has to be in the retail's terms.
Two naming rules, both taken from the trace itself:

* the **segment fields** are named by address - origin at `+0x00`, direction
  at `+0x0C`, extent at `+0x18` of each `Segment3` - so they read `s0.d.y`,
  `s1.ext`;
* the **header values** are named by *the expression that was in the register
  when the retail spilled it*. `dA01` is the 53-bit value the retail kept in
  the register; `fA01` is the float32 copy it spilled and reloaded. The two
  are different numbers, and which one a region uses is a fact, not a choice.

### The corner block's transcription

These are the traced expressions, verbatim, for one draw in each region. `r32`
is a spill to a 32-bit slot; `c64[00d71cd8]` is the double `2.0`.

**Interior** (draw 2):

```
r32(abs(r32((((((fA01 * r32((fS1 * r32(1.0/fDet)))))
              + r32((fS0 * r32(1.0/fDet))))
              + (c64[00d71cd8] * fB0))
             * r32((fS0 * r32(1.0/fDet))))
           + ((fB1 * c64[00d71cd8]) + ((r32((fS0 * r32(1.0/fDet)))) * fA01)
              + r32((fS1 * r32(1.0/fDet)))))
           * r32((fS1 * r32(1.0/fDet))))
         + fC))))
```

read as

```
invDet = r32(1.0/fDet)          s0 = r32(fS0*invDet)      s1 = r32(fS1*invDet)
u0 = ((fA01*s1) + s0) + (2.0*fB0)
u1 = ((2.0*fB1) + (fA01*s0)) + s1
sq  = ((s0*u0) + (s1*u1)) + fC
```

**Side region, middle branch** (draw 1) - `fTmp0` is spilled:

```
r32(abs(r32(((((fB1 + fB1) + r32(s1.ext)) * r32(s1.ext))
            - (r32(-((fB0 + (fA01 * r32(s1.ext)))))
               * r32(-((fB0 + (fA01 * r32(s1.ext))))))) + fC))))
```

**Corner region, first inner branch** (draw 223) - `fTmp1` is spilled:

```
r32(abs(r32(((((fB0 * c64[00d71cd8]) + r32(s0.ext)) * r32(s0.ext))
            + ((r32(r32(-s1.ext))
                - (r32(-((fB1 + (fA01 * r32(s0.ext))))) * c64[00d71cd8]))
               * r32(r32(-s1.ext)))) + fC))))
```

**Corner region, outer block** (draw 226) - `fTmp0` is spilled:

```
r32(abs(r32(((((fB1 * c64[00d71cd8]) + r32(-s1.ext)) * r32(-s1.ext))
            + ((r32(s0.ext) - (r32(-((fB0 + (fA01 * r32(-s1.ext)))))
                              * c64[00d71cd8])) * r32(s0.ext))) + fC))))
```

What the transcription says, and it is not what the WM4 source says:

* **The formulas are the WM4 ones.** Every region's expression is the WM4
  region formula, term for term. The corner block is not a different
  algorithm; it is the same one.
* **One grouping is different.** The interior's `u1` is
  `((2.0*fB1) + (fA01*s0)) + s1`. The WM4 source writes
  `fA01*fS0 + fS1 + 2*fB1`, which C++ groups as
  `((fA01*s0) + s1) + (2.0*fB1)`. The two are not the same number: the
  intermediate sums differ, and the difference survives the final rounding.
  The rebuild now writes the retail's grouping.
* **The spills are where the trace says.** `kDiff`'s three components, then
  `fA01`, `fB0`, `fB1`, `fC`, `fDet`, `fS0`, `fS1`, `fExtDet0`, `fExtDet1`;
  inside a region, `fTmpS0` or `fTmpS1`; the region's own arithmetic stays in
  the registers until the result is returned.

### The precision, fixed in the source

The retail's chain is 53-bit with float32 spills (measured, see above), and
the rebuild's `Real` chains round per operation. The fix is in the source, not
in the build: every chain in `Foundation/Distance/Wm3DistSegment3Segment3.cpp`
is written in `double` and rounded to `Real` at exactly the points the trace
shows the retail spilling, and the region's arithmetic is computed at 53 bits
until the result is returned.

The compiler's output shows the change took (`mulsd`/`addsd` for the chains,
`cvtsd2ps`/`cvtsd2ss` at the spills, the `subss` for `kDiff` that the retail
also does in float):

```
movss  xmm0, dword ptr [edx]      ; kDiff.x, in float, as the retail does
subss  xmm0, dword ptr [eax]
cvtps2pd xmm6, xmm0
mulsd  xmm1, xmm7                 ; the chain, at 53 bits
addsd  xmm1, xmm0
cvtpd2ps xmm3, xmm6               ; the spill, back to float32
```

**After the change the rebuild's header matches the retail's on 400 of 400
sampled draws, bit for bit** - `fA01`, `fB0`, `fB1`, `fC` and `fDet` all. Before
it, `fB0` and `fDet` were one ulp off on some draws, which is enough to flip
a clamp comparison, because the generator forces exact tangency in a tenth of
the draws.

### What the numbers are now

Measured on this build, and re-measured after a *clean* rebuild of the harness
(objects deleted, everything recompiled):

| measurement | before | after |
|---|---|---|
| `IntrSegment3Sphere3::Test(swept)`, 200 000 | 114 | 114 |
| `IntrSegment3Sphere3::Find(swept)`, 200 000 | 138 | 138 |
| `IntrSegment3Sphere3::Test(swept)`, 1 000 000 | 606 | 606 |
| `IntrSegment3Sphere3::Find(swept)`, 1 000 000 | 545 | 545 |
| targets passing | 31 / 33 | 31 / 33 |

`build_engine.bat` (the real `main.vcxproj`, Win32 Release) exits 0 and
produces `main.exe`.

**The rows did not move.** The precision change fixed the header - it now
matches the retail exactly on every sampled draw - but the residual
disagreements are not in the header. They are in the region blocks' own
arithmetic, and the four traced expressions above are the tool to fix them:
what is left is to compare each region's traced expression with what the
rebuild computes for it, branch by branch, and write the difference back.

So the honest state is: the instrument is built and validated (400/400), the
corner block's transcription is done, the precision is pinned by the source
and the header is now bit-exact, and the two swept rows still fail with the
same counts they had before. The simulator's validated status is not a fix;
it is what makes the remaining fix possible.

### Still outstanding

* **The region blocks' exact arithmetic.** The traced expressions are in hand
  and validated; they have to be compared with the rebuild's region code
  branch by branch, and the differences written back into
  `Foundation/Distance/Wm3DistSegment3Segment3.cpp`. The interior's `u1`
  grouping is already done; the rest is not.
* **The engine-side API mirroring** for the swept rows (the WM3.8-vs-WM3.4
  signature difference is bridged in the harness only) - still outstanding,
  and stated as such.
* **The build-dependence of the rebuild's other floating-point code.** The
  chains in this one function are pinned by the source now; whether the same
  holds elsewhere in the collision slice is not measured.

## The swept rows, closed: `_PC_24` after all, and the two functions under it

The two swept `IntrSegment3Sphere3` rows now pass **1,000,000/1,000,000 on
seeds `0x5EED2007`, `0x12345678` and `0xDEADBEEF`**, and the whole table passes
33/33 at 200,000. Three separate errors had kept them open; all three were in
the rebuilt source, and the earlier "53-bit chain" conclusion was a measurement
artifact that has to be retracted.

### The control word: `0x007F` really is `_PC_24`

The section above ("The precision: the retail's chain is 53-bit, not 24-bit")
is wrong, and the way it went wrong is worth recording because it produced a
whole round of source changes that could not possibly have worked.

The rows call `SetFpEnv()` before each side, which is `fninit; fldcw 0x007F`.
`0x007F` is precision control `00` - a 24-bit significand, the engine's
`_PC_24` - and the retail's x87 therefore rounds **every** arithmetic result to
float32. That is what the `Vec3::Dot` and `Vec3::LengthSq` rows already proved:
they only pass with their per-operation `RoundTo24` emulation.

The 53-bit reading came from a probe whose own measurement path called the
*rebuilt* `GetSquared` between setting the control word and calling the
retail's (the SIMD dump in `RunSegmentSegmentProbe` did exactly that), and the
rebuild's `double` chains make MSVC emit `fldcw`, which resets the word. So the
retail was measured at the ambient `0x027F` while the rows compare it at
`0x007F`. `simd.txt` was generated that way, which is why the 53-bit simulator
matched it 400/400: it was reproducing the probe, not the row.

The correct check is direct. On iter 304 of the default seed the row's retail
`GetSquared` is `0x41936534` (its `Get()` is `0x40895B12`, and
`sqrt(0x41936534) = 0x40895B12` under the CRT's narrowing). The simulator at
`SIMPREC=24` gives `0x41936534`; at `SIMPREC=53` it gives `0x4193653D`. The
row's environment is the 24-bit one.

### `DistSegment3Segment3::GetSquared` is the WM4 body in plain float

With the control word settled, the body is simple: every intermediate is a
float32, so the source only has to reproduce the **operation order and
grouping**, and plain `Real` arithmetic on this build (SSE `mulss`/`addss`)
rounds exactly where the retail's x87 does. The three remaining differences
from the WM4 text were found by comparing a Python transcription of the region
code against the simulator, draw by draw:

1. **The four dot sums.** They are inlined in the retail, and all four add
   their terms **y, x, z** - spelled out in the source now. (The tracer renders
   B0's and C's expressions with the terms re-associated; the numeric value is
   what the simulator agrees with, and a z-first sum moves the row. This is the
   same "the string is not the arithmetic" trap the 53-bit trace set.)
2. **Region 0's `u1`** is `((fA01*fS0) + fS1) + 2*fB1` - WM4's grouping. The
   traced `((2*fB1) + (fA01*fS0)) + fS1` is the same expression re-rendered and
   is *not* the value the row agrees with.
3. **Region 7's third branch** (`fTmpS0 > extent0`) does **not** do WM4's
   nested `fTmpS1` clamp. Retail 0x00A7E86A jumps to a shared block at
   0x00A7E871 that uses `fS1 = -extent1` and the `fTmpS0` computed for it:
   `fS0*(fS0 - 2*fTmpS0) + fS1*(fS1 + 2*fB1) + fC`. Regions 2/4/6 do have the
   nested clamp; that asymmetry is in the retail.

The validation used for the transcription is the simulator, not the rebuild:

| check | result |
|---|---|
| Python model vs `sim.py` (`SIMPREC=24`) on 3000 random segment pairs, all regions | 3000/3000 |
| rebuilt `GetSquared` (dumped from inside the function) vs the model, 400-row SIMD sample | 400/400 |
| rebuilt `IntrSegment3Sphere3::Test(swept)` | 1,000,000/1,000,000, three seeds |

`Get()` keeps the CRT's double square root and narrowing; that was already
right and is unchanged.

### `IntrLine3Capsule3::Find`: the swept Find's last two ulps

`IntrSegment3Sphere3::Find(swept)` still failed 138 in 200,000 after
`GetSquared` was fixed. A probe that replayed the row's draws and compared the
capsule Find both sides run (`IntrSegment3Capsule3::Find`, retail 0x00A6C070)
on the same path found that **all** the residual disagreements had retail
returning one intersection and the rebuild none, with the path and capsule
bit-identical. So the difference was inside `IntrLine3Capsule3::Find`
(retail 0x00A7F920), and it was the two effects this file keeps turning up:

- **The six dot products are inlined and add their terms y, x, z.** The shared
  `Vector3::Dot` sums z, y, x. One ulp in the quadratic's discriminant is the
  difference between "tangent, one root" and "miss" on a swept path that the
  generator puts exactly on the capsule's surface. The dots are spelled out.
- **The square roots are `Math<Real>::Sqrt`**, not `std::sqrt`. Retail's
  0x00A91000 ends in an x87 `FSQRT` (0x00A9103C) when the control word is not
  `0x027F`, so under `_PC_24` it is a 24-bit root - a correctly rounded double
  root narrowed is a different number. (The same call site had been changed to
  `std::sqrt` in the 53-bit round; this reverts it, with the reason recorded.)

### What was wrong with the probe, for the next reader

The temporary probe accumulated two defects that made its own numbers
untrustworthy and are worth recognising:

- it did not set the control word around the retail call in every path (see
  above);
- a missing closing brace left the summary `printf`s inside the per-draw loop,
  so it printed one "PROBE done" per disagreement. The block compiled only
  because the extra brace at the end of the file happened to balance it. The
  probe has been removed now that the rows pass; the three engine-side probe
  globals (`Wm3LastSweptCapsule`, `Wm3LastSweptPath`, `Wm3Dss3LastHeader`) went
  with it.

The engine-side API mirroring of the swept signature (WM3.8 `(vel0, vel1,
tMax)` vs the rebuild's `(fTMax, vel0, vel1)`) is still bridged in the harness
only, and remains outstanding as stated above.

## The distance and capsule batch: six more rows

After the two swept rows closed, the chain they reduce to still had no row of
its own, and the same address walk reached three more leaves. Six rows were
added, and **all six pass 1,000,000/1,000,000 on seeds `0x5EED2007`,
`0xDEADBEEF`, `0x12345678`, `0xC0FFEE01` and `0xA5A5A5A5`**; the full table is
39/39 at 1,000,000.

| row | retail | provenance | ABI |
|---|---|---|---|
| `DistSegment3Segment3::GetSquared` | `0x00A7E660` | vtable `0x00D72254` +0x18; ctor `0x00A7E190` | thiscall, no args, result in ST(0) |
| `DistSegment3Segment3::Get` | `0x00A7E230` | same vtable +0x0C; dispatches +0x18, then the CRT sqrt at `0x00A91000` | thiscall, result in ST(0) |
| `DistVector2Box2::GetSquared` | `0x00A6C9D0` | ctor `0x00A6C5E0` writes vtable `0x00D720C4`; +0x18 | thiscall, result in ST(0) |
| `IntrSegment3Capsule3::Test` | `0x00A6BEB0` | ctor `0x00A6BE80`, vtable `0x00D72094` +0x08 | thiscall, bool in EAX |
| `IntrSegment3Capsule3::Find` | `0x00A6C070` | same vtable +0x10 (the static Find; +0x0C is the base's swept stub) | thiscall, bool in AL |
| `IntrLine3Capsule3::Find` | `0x00A7F920` | direct `CALL` at `0x00A6C089` inside `IntrSegment3Capsule3::Find` | `__cdecl`, four pointers, count in EAX |

`DistVector2Box2` is the one that needed the vtable route: it has no engine
call site of its own. `IntrBox2Circle2::Test` (`0x00A4E620`) constructs it at
`0x00A6C5E0` and dispatches `Get()` through slot +0x0C; the constructor's
vtable immediate is `0x00D720C4`, and slot +0x18 there is `0x00A6C9D0`. That
also makes this row a second, independent check on the already-passing
`IntrBox2Circle2::Test`, which only compares the boolean `distance <= radius`
and would not notice a one-ulp difference in the distance itself.

### What the rows compare

- `DistSegment3Segment3::GetSquared` and `Get`: two `Segment3`s (14 words) and
  the result. The body is the one the swept round reconstructed (dots summed
  y, x, z; plain float; region 0's grouping; the asymmetric region-8 clamp), so
  this row is the first *direct* comparison of that reconstruction. `Get` has
  the same finite-and-non-negative scope as the other `Get` rows: retail's
  `Get` takes the CRT sqrt, whose non-positive slow path is a fatal math error
  in the retail as well as in the harness.
- `DistVector2Box2::GetSquared`: `vec[2] box[8]` (10 words) and the result.
- `IntrSegment3Capsule3::Test`: `segment[7] capsule[8]`, one boolean.
- `IntrSegment3Capsule3::Find`: the same 15 words, six outputs - the bool, the
  quantity, `GetParameter(0)` and `GetPoint(0)`. The last three are only
  compared after a successful Find: retail leaves the point array untouched on
  the miss path, and comparing it there would compare uninitialised memory.
  The retail's members are read out of its own object storage (`m_iQuantity`
  at +0x14, the first point at +0x18, the first parameter at +0x30), the same
  way the `IntrBox3Sphere3` rows read the contact point.
- `IntrLine3Capsule3::Find`: `line[6] capsule[8]`, the root count and the two
  roots. Each root is written only when the returned count covers it.

### Genuinely tangent inputs, not near-tangent ones

The capsule Find has a `ZERO_TOLERANCE` branch on the quadratic's
discriminant, and a random perpendicular offset around the radius never
reaches it: at 20,000 draws the first generator produced 11,582 misses and
8,418 two-root answers, and **zero one-root answers**. Forcing the capsule's
spine parallel to the query with the offset exactly equal to the radius does
not help either - that is the `|dz| >= 1 - eps` branch, which returns 0 or 2 and
never 1.

The construction that does reach it is exact in the capsule's own frame:
capsule spine at the origin along +z, query at `(r,0,0)` along
`(0, 1/2, sqrt(3)/2)`. Then `kP = (r,0,0)`, `kD = (0,1/2,0)` in that frame,
`fA1 = 0`, `fA0 = r*r - r*r` is exactly zero, and so is the discriminant.
One draw in ten is built that way. On 20,000 draws of the default seed that
moved the static Find's answer distribution to **10,398 misses, 2,015
one-root and 7,587 two-root**; the capsule row's `Find` has 5,342 hits, 1,611
of them with two points; and `Test` hits 5,642 times. Both rows still agree
bit for bit on all of them.

The rest of the generator places the second shape relative to the first -
along its direction and across it, on the scale of the extents - so the
interior/corner/vertex regions are common rather than only the far-away case.
About 12% of the segment pairs have parallel or antiparallel directions, which
is the other `fDet` branch; the capsule spine is explicitly parallel to the
query one draw in four, and the exact tangent draw is parallel as well.

### What this batch says

No rebuilt source changed for it. The six rows pass against the bodies as they
stand, which is the first direct evidence that the distance reconstruction and
the capsule chain are right outside the swept generator's narrow placements.
Two harness defects were found and fixed while wiring it, both of the shape
this file already warns about: the `vec[2] box[8]` generator wrote the box at
word 0 and left the last two extent words uninitialised, and the retail
`Find` wrapper's return word was written only on one sub-branch, which shows up
as the canary `0xCDCDCDCD` in the retail lane. A vtable slot label was wrong
too - see "Reading a vtable straight out of `.rdata`" above; the addresses the
old rows called were right, only the slot numbers and two source strings were
not.

## Four more rows: the box-line Find, the box2 distance and the capsule accessors

Four rows were added on top of the thirty-nine: the `IntrLine3Box3::Find` the
open `bSolid` question was waiting on, the `Get()` of the `DistVector2Box2`
distance, and the two accessors of the capsule intersector. All four pass
bit-for-bit, and **no rebuilt source changed** - which is the first thing to
say about them, because the rebuild already carried the right behaviour and
nothing here is a translation fix.

| row | retail | provenance | ABI |
|---|---|---|---|
| `IntrLine3Box3<float>::Find` | `0x00A508F0` | vtable `0x00D71EF4` +0x10 | thiscall, no stack arguments, the bool in AL |
| `DistVector2Box2<float>::Get` | `0x00A6C680` | vtable `0x00D720C4` +0x0C, dispatched by `IntrBox2Circle2::Test` (0x00A4E620) | thiscall, no stack arguments, the float in ST(0) |
| `IntrSegment3Capsule3<float>::GetPoint` | `0x00A6BDF0` | the class's own accessor body - see the note on the zero xrefs | thiscall, the index as a dword stack argument, `RET 4`, a **pointer** in EAX |
| `IntrSegment3Capsule3<float>::GetParameter` | `0x00A6BE00` | the same | thiscall, the index as a dword stack argument, `RET 4`, the float in ST(0) |

The four rows are **appended at the end** of `wm3coll::kTargets[]`, so no
existing row's per-row seed moved - the harness derives each row's seed from
its index in the merged table, and the README's "One harness property that
matters when reading per-seed numbers" is the reason the order is deliberate.

### `IntrLine3Box3::Find`: the `bSolid` question, answered by the retail's own body

The address comes from the vtable, and the slot number is pinned by two rows
that already pass: the `Intersector` layout is
`[dtor][Test(fTMax)][Test()][Find(fTMax)][Find()]`, `IntrSegment3Capsule3`'s
vtable `0x00D72094` reads `[0x00A6C030, 0x00A45510, 0x00A6BEB0, 0x00A45530,
0x00A6C070]`, and `IntrLine3Box3`'s vtable `0x00D71EF4` reads
`[0x00A501E0, 0x00A45510, 0x00A4FE60, 0x00A45530, 0x00A508F0]` - whose +0x08
slot is the `Test` that is already a row.

The body is 0x4A bytes and it does one thing: it calls the shared
`DoClipping` at **0x00A50220** with nine stack arguments.

```
00a508f3  FLD dword ptr [0x00D71C00]     ; the float 0x7F7FFFFF = FLT_MAX
...
00a5091b  PUSH 1                          ; <- bSolid
00a5091d  PUSH ECX                        ; &rkLine.Direction
00a50921  PUSH EDX                        ; &rkBox.Axis[0]
00a50922  PUSH EAX                        ; &rkLine
00a50931  CALL 0x00A50220                 ; DoClipping
00a50936  ADD ESP,0x2C
00a50939  RET
```

Three things fall out of that, and the first one is the answer to the open
question in "Differences that are still open":

* **The flag is the literal 1.** `IntrLine3Box3::Find` pushes `1` for
  `bSolid`, so the rebuild's hard-coded `true` is what the retail does. The
  sibling `IntrSegment3Box3::Find` (0x00A462A0) reads the same flag out of
  its object as a byte - `MOVZX EDX, byte ptr [ECX+0x14]` - which is the
  `bSolid` member the rebuild's `IntrSegment3Box3` constructor takes. Both
  call sites in the rebuild are right, and neither needed changing.
* **The two `Find` rows share one `DoClipping`.** `IntrSegment3Box3::Find`
  calls the same 0x00A50220, which is why the `Clip`/`DoClipping` x87 model
  the earlier rows needed applies here unchanged.
* **The parameters are the float `MAX_REAL`, not the double one.** The
  constant is loaded as a *float* (`FLD dword ptr [0x00D71C00]`, 0x7F7FFFFF)
  and negated with `FCHS`, so `fT0 = -FLT_MAX` and `fT1 = +FLT_MAX` - which
  is what the rebuild's `-Math<Real>::MAX_REAL, Math<Real>::MAX_REAL` gives
  (`Math<float>::MAX_REAL` is `FLT_MAX`). The sibling function at 0x00A50940
  loads the *double* `DBL_MAX` at 0x00D71C18 instead, so the two are not
  interchangeable and the row is pinned to the float one.

The retail's object layout was read off the retail's own constructors rather
than assumed:

| offset | member | written by |
|---|---|---|
| +0x00 | vtable | 0x00A4FE30 (`MOV [ESI], 0x00D71EF4`) |
| +0x04 | `m_fContactTime` | the base ctor 0x00A45570 (`FLDZ; FSTP [EAX+4]`) |
| +0x08 | `m_iIntersectionType` | the base ctor 0x00A45570 (`MOV [EAX+8], 0`) |
| +0x0C | `m_rkLine` | 0x00A4FE30 |
| +0x10 | `m_rkBox` | 0x00A4FE30 |
| +0x14 | `m_iQuantity` | `DoClipping`, through its seventh argument |
| +0x18 / +0x24 | `m_akPoint[0]` / `m_akPoint[1]` | `DoClipping`, through its eighth |

The rebuild's layout is the same, and for the same reason: the rebuild's
`Intersector` declares `Real m_fContactTime` before `int
m_iIntersectionType`, so the base is 12 bytes and the derived members start
at +0x0C. The row reads the type, the quantity and the two points out of the
retail's object storage, the way the `IntrBox3Sphere3` rows read the contact
point.

**The generator has to reach all three branches, or the row is half a row.**
A random line and a random box produce `IT_EMPTY` and `IT_SEGMENT` and
almost never `IT_POINT`, and the `bSolid` path needs a line that cannot be
clipped at all. The generator therefore builds three shapes:

* **the exact tangency**, one draw in ten, built in the box's own frame so
  the arithmetic is exact: the box is the unit cube at the origin and the
  line starts at (2,0,0) along (1,1,0). In the box's coordinates that is
  `o = (2,0,0)` and `d = (1,1,0)`, so the x slab gives [-3,-1] and the y slab
  gives [-1,1] - they touch at `t = -1` exactly, and `fT0 == fT1` is what
  makes the retail take the `IT_POINT` branch. A near-tangent draw would be a
  few ulp away and would take the `IT_SEGMENT` branch instead.
* **the unclippable line**, one draw in twenty, with a zero direction and
  the origin *inside* the box. That is the only geometry that leaves the
  line unclipped, and therefore the only one that reaches the `bSolid`
  branch: the retail reports `IT_SEGMENT` with the two points computed from
  the unclipped `-MAX_REAL` / `+MAX_REAL`.
* **the general case**, an orthonormal box with positive extents and a line
  through its neighbourhood.

The answer distribution over 200,000 draws on the default seed, measured with
a temporary counter in the row's wrapper and then removed:

```
type:     empty=150890  point=19984  segment=29126
quantity: 0=150890     1=19984      2=29126
zero-direction hits (the bSolid branch) = 7505
```

All three branches are therefore exercised, and the row passes 200,000/200,000
bit-identically with them.

### `DistVector2Box2::Get`: the same six instructions as the other `Get` rows

`0x00A6C680` is the vtable's +0x0C slot, and the anchor is a direct code
reference rather than the vtable alone: `IntrBox2Circle2::Test` (0x00A4E620)
builds a `DistVector2Box2` on its own stack with the constructor at
0x00A6C5E0 and then dispatches it -

```
00a4e657  MOV EDX,[EAX]          ; the vtable
00a4e65b  MOV EAX,[EDX+0x0C]     ; the +0x0C slot = Get()
00a4e666  CALL EAX
```

The body is six instructions: dispatch the vtable's +0x18 slot
(`GetSquared`, 0x00A6C9D0 - the row that already passes), take the CRT's
square root at 0x00A91000, narrow the result to a float through a stack
slot, reload it and return. That is the same shape as
`DistSegment3Segment3::Get` (0x00A7E230) and `DistVector3Segment3::Get`
(0x00A480E0), and the rebuild's model for it - `Math<Real>::Sqrt`, which is
`(Real)sqrt((double)fValue)`, a correctly rounded double root narrowed to
float - is the one those two rows already validate. **No source change was
needed**, which makes this row a second independent check on the CRT-sqrt
ceiling the README records: the distance is never negative, so the CRT's
error path is not reached.

### The two capsule accessors, and a deliberate deviation from the audit

`GetPoint` and `GetParameter` are 0x00A6BDF0 and 0x00A6BE00:

```
00a6bdf0  MOV EAX,[ESP+4] / LEA EAX,[EAX+EAX*2+6] / LEA EAX,[ECX+EAX*4] / RET 4
00a6be00  MOV EAX,[ESP+4] / FLD dword ptr [ECX+EAX*4+0x30] / RET 4
```

`GetPoint` returns `&m_akPoint[index]` - a pointer into the object at
`this + 0x18 + 12*index` - and `GetParameter` returns
`m_afParameter[index]` in ST(0). The pointer is handed back untouched and the
target dereferences it on both sides; the float is stored through the
caller's pointer.

**Neither body has a code reference in the retail image.** There is no CALL
to either address, no `.rdata` slot holding either address, and no
instruction with either address as an immediate: the compiler inlined both
accessors at their call sites, so the out-of-line copies are dead code in the
retail. That fails the "Xrefs" check of the provenance audit above, and it
is recorded here rather than glossed over. The reason the rows are still
worth having is that the check the audit is really after - *is this address
the Wild Magic template's body* - is answered by the body itself: the
pointer arithmetic matches the class's member layout exactly
(`+0x18 + 12*i` for the points, `+0x30 + 4*i` for the parameters), and the
third accessor, `GetQuantity` at 0x00A6BDE0 (`MOV EAX,[ECX+0x14]; RET`),
sits between them, so the three are a contiguous and self-consistent group
rather than three unrelated functions that happen to be nearby.

What the rows buy is that the rebuild's `GetPoint` and `GetParameter` are
**otherwise never called** - no engine call site reaches them - so their
bodies were unverified until now. And they call the retail's accessors
rather than reading the members out of the object storage the way the
`IntrSegment3Capsule3::Find` row does, so the accessor bodies themselves are
what is compared.

The intersection set is only defined after a successful Find - the retail's
`Find` writes the point array on the hit path only, and the rebuild's own
`GetPoint` asserts `0 <= i && i < m_iQuantity` - so both sides write zero on
the miss path. The existing `IntrSegment3Box3::GetPoint` row is gated the
same way, for the same reason: the first version read the array
unconditionally and reported 5311 mismatches, which was it comparing
uninitialised memory.

### What the four rows compare

* `IntrLine3Box3::Find`: `line[6] box[15]` in, nine words out - the bool, the
  intersection type, the quantity and the two points. A point is only
  compared when the quantity covers it.
* `DistVector2Box2::Get`: `vec[2] box[8]` in, one word out - the raw float
  bits of the distance. No gating at all: the value is defined on every draw.
* `IntrSegment3Capsule3::GetPoint`: `segment[7] capsule[8]` in, three words
  out - the point's three floats, zero on the miss path.
* `IntrSegment3Capsule3::GetParameter`: the same inputs, one word out - the
  parameter's raw float bits, zero on the miss path.

All four reuse the generators the classes' other rows already use
(`GenDistVector2Box2`, `GenIntrSegment3Capsule3`), so the new rows measure
the same input domains the passing rows measure.

## Three more rows: the distance derivatives and the segment-box Test

Three rows, all passing 1,000,000/1,000,000 on every seed tried. Two of them
are the base class's generic bodies, and the third found a real difference in
the rebuilt source.

| row | retail | provenance | ABI |
|---|---|---|---|
| `DistVector3Segment3<float>::GetDerivative` | `0x00A39110` | the `Distance<float,Vector3<float>>` base's generic body, the class vtable's +0x1C slot (the Vector2 instantiation is 0x00A38AF0) | thiscall, three stack arguments - fT, rkVelocity0, rkVelocity1 - `RET 0xC`, the float in ST(0) |
| `DistVector3Segment3<float>::GetDerivativeSquared` | `0x00A39180` | the same base's generic body, the vtable's +0x20 slot (the Vector2 instantiation is 0x00A38B60) | the same |
| `IntrSegment3Box3<float>::Test` | `0x00A46440` | the class vtable's +0x08 slot - the same slot the already-wired `IntrLine3Box3::Test` occupies | thiscall, no stack arguments, the bool in AL |

### The two `Distance` slots the table had mislabelled

The `Distance` vtable table under "Reading a vtable straight out of `.rdata`"
was wrong about its last two slots, and the correction is written up there.
The short version: **+0x1C and +0x20 are the base's generic `GetDerivative`
and `GetDerivativeSquared`, not the closest-point accessors** - which are not
virtual at all.

So these two rows are the first to pin those slots, and they pin them by
measurement rather than by reading. The retail's `GetDerivative` reads the
step at `[this+0x2C]`, calls the vtable's +0x08 slot - the swept `Get` -
twice with `fT+step` and `fT-step`, and returns the difference times
`[this+0x30]`, which is `m_fInvTwoDifferenceStep`. That is the rebuilt
`Distance::GetDerivative`'s formula exactly, and the row agrees bit for bit.

**The rows also exercise the retail's swept `Get` under the hood**, because
the derivative's inner calls go through the vtable's +0x08 slot - the
retail's `DistVector3Segment3::Get(fT,v0,v1)` at 0x00A48190. That body has
no row of its own yet; this is the first time it runs under the harness.

### `IntrSegment3Box3::Test` found a real difference

The row failed on 165 of 200,000 draws - 0.08% - always with the retail
answering "no intersection" and the rebuild answering "yes". The cause is
the one the collision slice has already met twice, in a place that had never
been checked because the row did not exist.

Retail 0x00A46440 is x87 and inlines its arithmetic. The rebuilt
`IntrSegment3Box3::Test` used the shared `Vector3::Dot` and the shared
`Vector3::Cross`, and neither models that body:

* `Vector3::Dot` models the retail's **standalone** Dot at 0x00452AD0, whose
  grouping is `(z+y)+x`. The dot inlined in the retail's Test sums its three
  products as `(y+x)+z`. The two groupings differ in which pairing rounds,
  and on the extreme inputs that is enough to flip a comparison.
* `Vector3::Cross` models the retail's standalone Cross at 0x00452BC0. The
  cross product inlined in the retail's Test is its own.

That is why `Wm3IntrLine3Box3.cpp` has its own `Dot24x`, `Cross24` and
`Sub24Vector` helpers for `IntrLine3Box3::Test` - the same algorithm against
the same kind of box, and the same problem. The fix is the same one, applied
to `Wm3IntrSegment3Box3.cpp`: a float branch that spells the six dots and the
cross product out with the per-operation `_PC_24` rounding and the retail's
grouping, and a double branch that keeps the original body.

The helpers are **local to each file on purpose**. Moving them into
`Wm3Vector3.h`, next to `Wm3Detail::RoundTo24`, would be the tidier place -
and it would change the code layout of a translation unit whose rows are
measured, and this file already records that the rebuild's own
floating-point codegen is layout-dependent. The duplication is the smaller
risk, and it is what the tree already does for `Wm3IntrLine3Box3.cpp`.

With the fix the row passes 200,000/200,000, and the engine's `main.exe`
still builds: `scripts/build_sdk_with_timeout.ps1` on `src\sdk\main.vcxproj`
(`Release`, `Win32`) exits 0, `output/main/Win32/Release/main.exe` and its
`.pdb` are rewritten, and the log holds no `: error` line. The rebuild of
`Wm3IntrSegment3Box3.cpp` is the only one the change causes.

## `InBox`, and the harness's own build flags

One row, and the two things it took to wire it.

| row | retail | provenance | ABI |
|---|---|---|---|
| `InBox<float>` | `0x00A3BC10` | a direct `CALL` at 0x004FF459 in `Moho::CColPrimitive_Box::PointInShape` (0x004FF450), which the engine's own comment names: "Calls Wild Magic's `InBox<float>` (0x00A3BC10, Wm3ContBox3.cpp) with the point and `mShape`" | a free function, cdecl: the point and the box are two stack pointers, the caller removes them, the bool in AL |

### The call site confirms the calling convention

The caller is the retail's own:

```
004ff450  mov eax,[esp+4]      ; the point
004ff454  add ecx,4            ; the shape's centre
004ff457  push ecx
004ff458  push eax
004ff459  call 0x00a3bc10      ; InBox
004ff45e  add esp,8            ; cdecl: the caller removes the arguments
```

which is the same shape as the rebuilt
`Moho::CColPrimitive_Box::PointInShape` calls `Wm3::InBox(*point, mShape)` -
and it fixes the argument order (the point first, the box second) and the
calling convention (cdecl) without any guesswork.

### A third dot grouping

The retail's `InBox` is x87 and inlines its dot:

```
00a3bc4c  fld dword ptr [edx - 8]    ; the box's axis x
00a3bc4f  fmul st(1)                 ; x the difference's x
00a3bc51  fld dword ptr [edx - 4]    ; the box's axis y
00a3bc54  fmul st(3)
00a3bc56  faddp st(1)                ; (x*x) + (y*y)
00a3bc58  fld dword ptr [edx]        ; the box's axis z
00a3bc5a  fmul st(4)
00a3bc5c  faddp st(1)                ; + z*z
```

so the sum is grouped **((x*x)+(y*y))+z*z** - a third grouping, and neither of
the two the rebuild already has: the shared `Vector3::Dot` models the retail's
standalone Dot at 0x00452AD0 and groups `((z*z)+(y*y))+x*x`, while
`Vector3::LengthSq` groups `(z*z)+(y*y+x*x)`.

The rebuilt `InBox` used the shared `Vector3::Dot`, and the row failed on
**2824 draws in 200,000**, in both directions - a sub-ulp difference in the
dot flipping the `|coeff| > extent` comparison. The fix is the one the other
two collision rows already use: a float branch in
`Foundation/Containment/Wm3ContBox3.cpp` that spells the dot out with the
per-operation `_PC_24` rounding and the retail's grouping, with the double
branch keeping the original body. The helper is local to that file, for the
same reason the other two are.

### The harness's own build flags

Wiring this row needed the harness to compile
`Foundation/Containment/Wm3ContBox3.cpp`, and that file's other functions pull
in the convex hull, the Gauss-points fit and the eigen-decomposition - about
fifteen more library sources and four more include directories, all of which
the engine's own `Foundation.vcxproj` already carries.

The first attempt to compile them failed, and the failure was worth more than
the row: `Foundation/RationalArithmetic`'s `Wm3Query3Filtered.inl` calls
`Det3` and `Det4` unqualified, and those are members of a *dependent* base
class, so MSVC's strict mode rejects them (error C3861). The Wild Magic 4
text under `.tmp/wm4ref/` is the same, so it is the translation's text and
not a typo.

**The tree is now built one way**, with the engine's `/permissive-`, and
those calls name their base class so the strict mode accepts them - which is
what the Rules section above records. The harness follows the same mode, so
the Wild Magic sources it compiles are compiled the way the engine compiles
them.

With the fix the row passes 200,000/200,000, and the engine's `main.exe`
still builds: `main.vcxproj` (Release, Win32) exits 0, `main.exe` and its
`.pdb` are rewritten, and the log holds no `: error` line.

## The swept distance rows

Two more rows, and **no rebuilt source changed** - the first time in a while
that a new row did not find a difference.

| row | retail | provenance | ABI |
|---|---|---|---|
| `DistVector3Segment3<float>::Get(fT,v0,v1)` | `0x00A48190` | the class vtable's +0x08 slot | thiscall, three stack arguments - fT, rkVelocity0, rkVelocity1 - `RET 0xC`, the float in ST(0) |
| `DistVector3Segment3<float>::GetSquared(fT,v0,v1)` | `0x00A48340` | the class vtable's +0x14 slot | the same |

The rebuilt `DistVector3Segment3::Get(fT,v0,v1)` builds the moving shapes
and calls the no-argument `Get`, and the retail's body does the same by
dispatching the vtable's **+0x0C** slot - the no-argument `Get` - through
`CALL dword ptr [0x00D71E78]`, which is that vtable's +0x0C entry. The swept
`GetSquared` dispatches the vtable's +0x18 slot the same way. So the
structure matches, and so does the arithmetic: both sides pass 1,000,000
draws on every seed.

These two bodies were already running under the harness - the derivative's
inner calls go through the vtable's +0x08 slot, which is the swept `Get` - so
this is the first time they were compared on their own.

### `ContAlignedBox<float>` is at 0x00A3AE40

The README listed this class as "located but not wired", with the anchor
`AxisAlignedBoundsFromBox` at 0x004FE2D0. Walking from there gives the
address in one step, and it is a direct `CALL`:

```
004fe2db  call 0x004fff00        ; Box3::ComputeVertices
004fe2e0  push 0
004fe2e6  push ecx
004fe2eb  push 8
004fe2ed  push edx
004fe2ee  call 0x00a3ae40       ; ContAlignedBox<float>
```

The caller is the engine's own `moho::AxisAlignedBoundsFromBox`
(`src/sdk/moho/math/Wm3AxisAlignedBox3FafExtras.cpp:26` calls
`Wm3::ContAlignedBox(8, vertices)`), so this is an anchor by the README's own
rule rather than a name match.

The calling convention is worth writing down, because it is not the one a
member function would use. It is a **free function that returns a `Box3` by
value**, so the hidden return pointer is the first stack argument and the
callee leaves all three arguments on the stack - it ends in a plain `RET`.
The stack is `(Box3* out, int iQuantity, const Vector3* akPoint)`, and the
returned box has the identity axis written into it before the centre and the
extents are computed. The box's layout is the rebuild's - the centre, then
the three axes, then the three extents - so the fifteen floats compare
directly.

## `ContAlignedBox`: the axis, and the fourth argument

The last class the README listed as "located but not wired" is wired, and it
found two differences - one closed, one still open.

| row | retail | provenance | ABI |
|---|---|---|---|
| `ContAlignedBox<float>` | `0x00A3AE40` | a direct `CALL` at 0x004FE2EE in the engine's `Moho::AxisAlignedBoundsFromBox` (0x004FE2D0) | a free function returning a `Box3` **by value**, so the hidden return pointer is the first stack argument; cdecl, the callee leaves its three arguments |

### The axis: a real difference, and an engine-visible one

The first draw the row reported had the retail's box and the rebuild's box
differing in exactly one place: **the axis**. The retail writes the identity
into it; the rebuild left it uninitialized.

That is not a rounding difference. `Box3`'s default constructor is
"uninitialized" in the rebuilt tree **and** in the Wild Magic 3.4 copy
(`.tmp/wm3/Source/Math/Wm3Box3.h:29`), and the Wild Magic 4 body the
rebuild's `ContAlignedBox` was translated from -
`.tmp/wm4ref/LibFoundation/Containment/Wm4ContBox3.cpp` - does not set the
axis either, byte for byte the same as the rebuilt one. The retail's WM 3.8
body does: it writes the identity into the box's axis before it computes the
centre and the extents.

So this is a **WM4-vs-WM3.8 behavioural difference**, which is exactly the
class of thing the "Cross-check the WM3 source" rule is about. The fix sets
the axis in `Foundation/Containment/Wm3ContBox3.cpp`:

```cpp
    kBox.Axis[0] = Vector3<Real>::UNIT_X;
    kBox.Axis[1] = Vector3<Real>::UNIT_Y;
    kBox.Axis[2] = Vector3<Real>::UNIT_Z;
```

**It is not cosmetic.** The engine's `moho::AxisAlignedBoundsFromBox` reads
that axis - `aligned.Axis[0][axis]`, `aligned.Axis[1][axis]`,
`aligned.Axis[2][axis]` - to build the axis-aligned bounds, so before the fix
the engine's bounds were computed from an uninitialized axis.

### The second difference was the harness's, and the retail has a fourth argument

The retail's `Wm3::ContAlignedBox<float>` at 0x00A3AE40 takes **four**
arguments - `(Box3* out, int iQuantity, const Vector3* akPoint, const
unsigned char* akMask)` - and the engine's own call site pushes `0` for the
mask at 0x004FE2E0, right before the other three. The thunk pushed only
three, so the fourth slot held whatever the caller's stack carried; nonzero,
it sent the retail into its **mask path** at 0x00A3B304, where a point counts
only if the byte at `akMask[i]` is nonzero. That is why the retail's box
covered fewer points than the count it was given - it was including the
points whose garbage mask byte happened to be nonzero.

The retail's no-mask path was never wrong: the unrolled group of four at
0x00A3AF25 advances the point pointer by 0x30 and runs `((count-5)>>2)+1`
times, and the tail loop at 0x00A3B1AB walks the remainder twelve bytes at a
time - `iQuantity` points in total, which is what the rebuilt
`ComputeExtremes` does. The row was measuring a call the engine never
makes.

The thunk now pushes the fourth argument as `0`, exactly as the engine's call
site does, and the row passes 1,000,000/1,000,000 on every seed tried.

**The library API difference is recorded, not closed.** The retail's Wild
Magic build carries that mask parameter; the rebuild keeps the
three-argument upstream signature, because the Wild Magic 4 body it was
translated from has three arguments as well and the engine passes `0` - so
the behaviour is identical. Adding the parameter would mean porting the
retail's mask path into `Wm3ContBox3.h`/`.cpp` and changing the engine's
call site, for no behavioural gain; that is a separate piece of work, and the
wmdiff row would have to exercise the mask path to verify it.

## `IntrBox3Sphere3::GetVertexIntersection` and `GetEdgeIntersection` - closed

The two private statics of the class are wired, and both pass.

    IntrBox3Sphere3::GetVertexIntersection  0x00A41740  CALL at 0x00A43831
    IntrBox3Sphere3::GetEdgeIntersection     0x00A41800  CALL at 0x00A43868

Neither is thiscall. They are **cdecl** with float arguments on the stack -
the retail's own caller pushes 0x1C bytes for the seven-argument one and
0x18 for the six-argument one, and removes them itself - and the result
comes back in ST(0); the bodies end in `FLD float ptr [ESP+0x18]; RET` and
never write EAX.

The arithmetic, read off the x87 sequence, is the rebuilt source's formula
with the same groupings:

    fDot   = a1*a4 + a2*a5 + a3*a6
    fVSqr  = a4^2 + a5^2 + a6^2
    fDiff  = (a1^2 + a2^2 + a3^2) - a7
    disc   = fDot^2 - fVSqr*fDiff
    fInv   = 1/sqrt(|disc|)
    result = (fInv*fDiff)/(1 - fDot*fInv)

so the rebuilt source needed no change - the float square root that "The
retail's square root is a 24-bit one" above already put into
`Wm3IntrBox3Sphere3.cpp` is what makes these two match. The rows are
200,000/200,000 and 1,000,000/1,000,000 bit-identical on the default seed
0x5EED2007, and they pass `--strict-nan` as well.

The generator draws a line that really meets the sphere - the point of
closest approach is at a distance h <= R - and reaches the two degenerate
cases random geometry never reaches: the **tangent** line, whose
discriminant is exactly zero, so the reciprocal is an infinity and the
answer a NaN (0xFFC00000 on both sides), and the line that **starts exactly
on the sphere**, whose fDiff is exactly zero and whose answer is exactly
zero. Both use R = 1024, a power of two, so `R*R` and `|D|^2` are the same
number and the subtraction is exact.

Two harness facts came out of it.

* The helpers are private, and **MSVC mangles the access into the symbol
  name**, so a reference from a translation unit that sees them as public
  does not bind to the instantiation the class's own `.cpp` emits. The
  harness reaches them through the "Rob" idiom instead - an explicit
  instantiation, whose template argument is not access-checked - which
  takes the *private* address and so names the private symbol: the engine's
  own code, not a copy. Opening the class up with `#define private public`
  does not work here: it changes the mangled name and the link fails with
  LNK2019.
* A cdecl thunk has to remove the arguments it pushed for the retail. The
  first version of these two did not, and the thunk's own `RET` then
  returned into the middle of the pushed arguments: the row crashed at an
  address that was one of the argument values, and the fault looked like a
  jump into the CRT's math-error path. It was neither - the stack was simply
  unbalanced by 0x1C bytes.

The other 50 rows are unaffected and still pass 200,000/200,000. The engine
was not touched: the rebuilt Foundation sources are unchanged, so there was
nothing to rebuild.

## `IntrBox3Sphere3`'s four region helpers - closed

The four private members the retail's `Find` calls to split the box-sphere
intersection into regions are wired:

    IntrBox3Sphere3::FindFaceRegionIntersection    0x00A41880  CALL at 0x00A420EE
    IntrBox3Sphere3::FindJustEdgeIntersection      0x00A41CC0  CALL at 0x00A4208A
    IntrBox3Sphere3::FindEdgeRegionIntersection    0x00A41F50  CALL at 0x00A43C27
    IntrBox3Sphere3::FindVertexRegionIntersection  0x00A421E0  CALL at 0x00A44016

All four are thiscall members with the callee removing the arguments: `ECX`
is `this`, and the bodies end in `RET 0x34` (thirteen dwords) for the face
and edge regions and `RET 0x30` (twelve) for the just-edge and vertex
regions. The face and edge regions take a flag as their last argument, read
as a byte by the retail (`CMP byte ptr [ESP+0x80],0` at 0x00A4189F and
`CMP byte ptr [ESP+0x3C],0` at 0x00A41F53).

The argument order is the rebuilt header's, and the retail's body confirms
it: the face region compares the centre's z with the face's z plus the
sphere's radius and then reads the flag, which is what the rebuilt source
does at `Wm3IntrBox3Sphere3.cpp:338`; the edge region forms the offsets
`fCx - fEx` and `fCz - fEz` and reads its flag at the top, as the rebuilt
source does at line 543. The rows pass 200,000/200,000 bit-identically on
every seed tried, and they pass `--strict-nan` as well.

Two harness facts came out of it.

* The helpers are private *members*, so the "Rob" idiom needs a pointer to
  member rather than a plain function pointer. The explicit instantiation
  still bypasses the access check ([temp.spec]/6), so the same trick reaches
  the private member; the pointer names the member the class's own `.cpp`
  instantiates, so the engine's code runs, not a copy.
  `FindFaceRegionIntersection` and `FindEdgeRegionIntersection` have the same
  signature, so their pointers have the same type - the tags still differ,
  because the members differ.
* The intersection coordinates are only written on the hit path. Both sides
  therefore start them from the same sentinel (`0x7FC00001`) and compare the
  raw bits, so a path that writes nothing agrees and a path that writes a
  different coordinate is a mismatch rather than a coincidence of
  uninitialised memory. The contact time is the member at `[this + 0x04]` on
  the retail side and `GetContactTime()` on the rebuilt side.

The generators build the geometry each helper is documented to work in - the
sphere above the +Z face for the face region, above the +X and +Z faces for
the edge region, above the corner for the vertex region, and `fDx, fDz` away
from an edge running along y for the just-edge region - and reach the
degenerate cases the algorithm has an answer for: the "already
intersecting" tests, the "not moving towards" tests, and the exact boundaries
of the face and the edge. The coverage is not partial: over 50,000 draws the
face region takes its face, x-edge and vertex paths 11,705 / 616 / 49 times;
the edge region delegates to the face region 26,741 times and to the
just-edge region 18,142 times; the just-edge region takes its vertex path
11,833 times and its edge path 38,167; and the vertex region hits the vertex
10,625 times and the edge region 39,375.

The rows are sensitive: swapping the first two arguments on either side
alone turns the 2,000-iteration face row from 2,000/2,000 into 264
mismatches, and perturbing the intersection coordinate or the contact time on
the rebuilt side alone turns it into 501 and 2,000 mismatches respectively.

The other 52 rows are unaffected and still pass 200,000/200,000. The engine
was not touched: the rebuilt Foundation sources are unchanged, so there was
nothing to rebuild.

## `ContAlignedBox` takes a fourth argument, and has a masked path

The retail's `Wm3::ContAlignedBox<float>` at `0x00A3AE40` takes **four** stack
arguments - the hidden return pointer, the quantity, the points and an
`abValid` mask - and the rebuild's took three. The engine's own call site
pushes a zero for the mask, so the retail takes its unmasked path there; but
the function has a masked path the rebuild did not have at all. The
disassembly establishes what it is:

* the box is constructed with the identity axis and unit extents (the twelve
  constants at `0x00F3D21C` are `ZERO, UNIT_X, UNIT_Y, UNIT_Z`, and the
  three extents after them are `1.0` - the **Wild Magic 3.4** constructor, not
  the Wild Magic 4 one);
* the extremes are seeded from the **first point whose mask byte is
  nonzero**, and the mask is read **a byte at a time** (`0x00A3B30E: cmp byte
  ptr [ecx+esi],0`), so any nonzero byte means "keep this point";
* when the mask admits no point at all the three extents become `-1.0`
  (`0x00A3B366` loads the constant `-1.0f` from `0x00E4F6E8`) and the
  function returns immediately, leaving the centre at the origin and the axis
  the identity;
* the comparison is the 3.4 **`else if`** form - a value below the minimum is
  not also tested against the maximum - and it is reproduced as written, not
  "improved" into two independent tests.

`ContAlignedBox` in `Wm3ContBox3.h`/`.cpp` is therefore back to the Wild
Magic 3.4 form: the third parameter `const bool* abValid = 0`, the 3.4 body,
and the explicit instantiations widened to match. The 2D `ContAlignedBox` in
`Wm3ContBox2.h`/`.cpp` gets the same treatment - its 3.4 copy carries the
same third parameter. **The default argument is what keeps the engine's
source unchanged**: `Wm3::ContAlignedBox(8, vertices)` still compiles to a
call that passes a zero mask, which is what the retail's call site does.

This is the second time the Wild Magic 4 copy sent the rebuild down the wrong
road, and the lesson is the one the "Cross-check the WM3 source" rule keeps
insisting on: **the reference is the Wild Magic 3.4 copy, not the Wild Magic
4 SDK the tree was translated from.** The 3.4 copy is retail's lineage; WM4
is only where the text came from. Comparing a WM4-derived body against WM4
proves the translation was faithful - it cannot show that the translation
dropped a feature. Here it had dropped three: the mask, the box's
initialisation, and the "no valid point" convention.

The row that covers it is **`ContAlignedBox(mask)`**, at the end of the table
so that no other row's seed shifts. It is the same retail entry point,
called with the mask as the fourth argument, with the layout
`count[1] mode[1] points[24] mask[8]` in and `center[3] axis[9] extent[3]`
out. The eight mask bytes are packed into the two words after the points, one
byte per point, and the same bytes are handed to both sides: the retail gets
them as the mask pointer, ours gets them as the `const bool*` the 3.4
signature takes (a bool array is a byte array in MSVC). The generator
produces five shapes - no mask at all, every point valid, a random mix of
valid and invalid points, no point valid (the `-1` extent case), and a mix
whose valid bytes are `0x80` rather than `1`, which pins that the retail's
test is "nonzero" and not "one". The points are duplicated and zeroed now and
then, so the comparisons hit the equal case, which is where the `else if` form
differs from two independent tests.

The rows pass 200,000/200,000 and 1,000,000/1,000,000 bit-identically on the
default seed `0x5EED2007`, and the whole table still passes 200,000/200,000 -
57/57 rows. The engine builds: `main.vcxproj` Release|Win32, exit code 0, no
`: error` line in the log, and `main.exe` and `main.pdb` are rewritten. The
Wild Magic library's code generation moves with that restoration, so the
lockstep suite is the check that belongs to it, and it passed:
`2p-main-x86-vs-x86` (the rebuild against the retail, five and a half minutes
of real play) ends in **PASS**.

## The 2D `ContAlignedBox` and the `IntrBox2Circle2` region helpers

Four more rows, and the 2D `ContAlignedBox` turned out to have a retail body
after all - which settles the one caveat the 3D restoration carried.

`ContAlignedBox2` (0x00A685F0) is the 2D `Wm3::ContAlignedBox<float>`. The
engine never calls it, so it has no code xref in the retail image; it was
found by its **shape** - a scan of the whole `.text` for a run of
constant-stores into one output buffer at increasing offsets yields exactly
two hits, the 3D one (0x00A3AE40) and this one. The body writes the 2D
identity (ZERO, UNIT_X, UNIT_Y from 0x00F3DBE0, and the two unit extents with
`FLD1`) before it computes the extremes, the same shape as the 3D one. It
returns a `Box2` by value (hidden return pointer first), is cdecl, and takes
four stack arguments - the fourth is the `abValid` mask, tested a byte at a
time (0x00A688B4), with the `-1.0f` extents (0x00E4F6E8) when the mask
admits no point. `ContAlignedBox2(mask)` is the same entry point with the
mask passed through.

**So the 2D restoration is now proved against the retail, not only against the
Wild Magic 3.4 copy** - which is what the 3D section above could not say
when it was written.

`IntrBox2Circle2::TestVertexRegion` (0x00A4E6C0) and `::TestEdgeRegion`
(0x00A4EB60) are **not** inlined by the retail's `Find` (0x00A4F580) - it
calls them out of line (the vertex helper at 0x00A4F67D, 0x00A4F815 and
0x00A4F8AF, the edge helper at 0x00A4F75E, 0x00A4F7D2 and 0x00A4F866) - so
each has a body of its own and each gets its own row. Which body is which is
settled by the arithmetic rather than by the call order: the vertex body
computes `fCx+fEx`, `fCy+fEy` first and then their squares minus R squared,
the edge body computes `fCx+fEx`, adds the radius and answers -1 when that is
not negative. Both are thiscall private members with eight stack arguments
`(fCx,fCy,fVx,fVy,fEx,fEy,&rfIx,&rfIy)` and `RET 0x20`, so the harness
reaches them through the "Rob" idiom, and the retail object is built by the
retail's own constructor 0x00A4E5F0.

The branch coverage is measured, not assumed: the vertex row returns -1 / 0 /
1 in 4330 / 7359 / 8311 of 20,000 draws, and its hit draws split into the
corner answer 6006, the left edge 2221 and the bottom edge 84; the edge row
returns -1 / 0 / 1 in 5260 / 7272 / 7468, with the hits split into the edge
interior 5477, the upper corner 1962 and the lower corner 29. The
circle-overlapping-a-vertex and circle-overlapping-an-edge cases are both
exercised, and swapping the two retail addresses makes both rows fail (8857
mismatches in 20,000), so the rows really compare the right bodies.

One coverage note worth keeping: the vertex helper's *"first contact at the
upper left corner"* branch is nearly dead. It needs the recomputed cross
product to be both greater than R*fVy and at most R*|V|, which the helper's
own earlier tests make very hard - it is taken **twice** in 20,000 draws,
while every other branch is taken thousands of times.

**No rebuilt source changed** for these four rows: the rebuilt 2D
`ContAlignedBox` (the 3.4 body) matches the retail bit for bit, and the two
region helpers in `Wm3IntrBox2Circle2.cpp` are correct as they stand. The
engine builds - `main.vcxproj` Release|Win32, exit code 0, no `: error` line -
and the Wild Magic library's contents did not change, so the lockstep
baseline is untouched.

## The ten rows of the swept Distance queries, the two `IntrLine3Box3` helpers, and the base's window queries

Ten rows, all passing 1,000,000/1,000,000 on every seed tried.

The six swept rows are the *derived* class's own `Get(fT,v0,v1)` and
`GetSquared(fT,v0,v1)` - the vtable's +0x08 and +0x14 slots - at
`0x00A458C0`/`0x00A45A60` (`DistVector3Box3`, vtable `0x00D71D8C`),
`0x00A6C730`/`0x00A6C880` (`DistVector2Box2`, vtable `0x00D720C4`) and
`0x00A7E2A0`/`0x00A7E480` (`DistSegment3Segment3`, vtable `0x00D72254`). The
last two vtables were found by the `.rdata` recipe and confirmed twice over:
by the RTTI type descriptor the Complete Object Locator points at
(`.?AV?$DistVector2Box2@M@Wm3@@`, `.?AV?$DistSegment3Segment3@M@Wm3@@`) and
by the retail's own constructors, which write those vtable pointers
(`DistSegment3Segment3Ctor` at `0x00A7E190` writes `0x00D72254`).

**The slot map has to be read from the base class's own vftable.** The base
constructor at `0x00A38C40` installs `0x00D71C54`, whose RTTI names
`Distance<float,Vector3f>`; read from there, +0x04 is the base's window
`Get(fTMin,fTMax,v0,v1)` (`0x00A38C50`) and +0x10 the base's window
`GetSquared(fTMin,fTMax,v0,v1)` (`0x00A38EB0`) - not a second destructor and
not a generic three-argument `GetSquared`, which the retail does not have at
all (it is pure virtual, `0x00A82547`). The two window rows run on a
`DistVector3Segment3f`, so the body's vtable dispatch reaches the derived
swept `Get`.

`IntrLine3Box3::DoClipping` (`0x00A50220`) and `::Clip` (`0x00A4FCC0`) are
static members: no `this`, nine and four stack arguments respectively, both
cdecl, the bool in `AL`. `Clip` is private and is reached through the "Rob"
idiom. The argument orders are established from the bodies and from the
retail's own call sites, not from the header.

The coverage is measured, not assumed: `Clip`'s denominator exactly zero
(including `-0`) 39,891 draws, an exactly equal numerator 60,058, an entirely
clipped segment 39,793, an exact quotient 29,974; `DoClipping` returns
IT_EMPTY 141,953, IT_POINT 39,450, IT_SEGMENT 18,597, with `bSolid` true
99,964 and false 100,036, and the exact tangency case 49,796. The mutation
checks hold: one ulp on the `Clip` row's first parameter fails 200,000 of
200,000, and swapping `DoClipping`'s two time parameters fails 58,047 of
200,000.

## The denormal range in the 2D box and the segment distance

**A row only passes if its generator reaches the case that matters.** The
`DistVector3Box3` defect - the retail computes that squared distance on the
x87 stack (extended exponent, 24-bit significand) and stores back to float
only at the end, so a float denormal result can differ by one denormal ulp -
showed up once in a million draws, by accident. The `DistVector2Box2` and
`DistSegment3Segment3` swept generators now draw the denormal case on
purpose: an extent of about `1e-19` with the vector just outside the face,
and two parallel segments about `1e-19` apart, in 15% of the draws. **Both
classes then failed by one denormal ulp**, on every seed - the same defect,
unnoticed until the generator was made to look for it:

| row | mismatches per million | retail | rebuild |
|---|---|---|---|
| `DistVector2Box2::GetSquared(swept)` | 16,976 | `0x004BF3CC` | `0x004BF3CD` |
| `DistVector2Box2::Get(swept)` | 16,849 | | |
| `DistSegment3Segment3::GetSquared(swept)` | 16,907 | `0x005EDA14` | `0x005EDA13` |
| `DistSegment3Segment3::Get(swept)` | 17,122 | | |

`Wm3DistVector2Box2.cpp` now models the difference, the dot product, the
delta, the square and the accumulation with the `Wm3Detail` helpers, as
`Wm3DistVector3Box3.cpp` already did for its own dot product.
`Wm3DistSegment3Segment3.cpp` does the same, carrying its intermediates in
`Wm3Detail::X87` - a binary64 with a 24-bit significand and the x87's
operators, added to `Wm3Vector3.h` - because that body interleaves the
source's own `Real` members; the double instantiation keeps the plain
arithmetic. All four rows of both classes pass 1,000,000/1,000,000 on every
seed afterwards, and the pre-existing static rows of both classes still pass
at 1,000,000: the modelling is behaviour-neutral in the normal range, as it
must be.

One difference is left open on purpose: the closest points and the two
segment parameters are still stored as plain `Real`s, while the retail
computes them on the x87 too. No row compares them, so that difference is
unmeasured and unchanged.

## The difference step: the retail agrees with both upstream texts

`Distance<float,Vector3f>::SetDifferenceStep` is the retail body at
`0x00A391D0` and `GetDifferenceStep` at `0x00A39200` (`fld dword ptr
[ecx+0x2c]; ret`). Both are wired as rows - `Distance::SetDifferenceStep` and
`Distance::DifferenceStep (default)`, the latter being the base constructor
`0x00A39F20`, which *inlines* the defaults instead of calling the setter.
What they say is that **the retail and the rebuild agree**: the retail stores
the argument when it is positive and `1e-3` when it is not, exactly as the
Wild Magic 3.4 copy and the Wild Magic 4 SDK say. **No source changed.**

The interesting part is how nearly the opposite conclusion is reached by
reading the disassembly:

```
00a391d0  fldz                            ; ST0 = 0
00a391d2  fld  dword ptr [esp+4]          ; ST0 = the argument, ST1 = 0
00a391d6  fcom st(1)                      ; compare the argument with 0
00a391d8  fnstsw ax
00a391da  fstp st(1)                      ; drop the zero; ST0 = the argument
00a391dc  test ah, 0x41                   ; C0 (status bit 8) and C3 (bit 14)
00a391df  je   0xa391e9                   ; -> store the argument
00a391e1  fstp st(0)                      ; drop the argument
00a391e3  fld  dword ptr [0xd45db0]       ; 1e-3
00a391e9  fstp dword ptr [ecx+0x2c]       ; m_fDifferenceStep
00a391ec  fld  dword ptr [ecx+0x2c]
00a391ef  fdivr qword ptr [0xd4ddf8]      ; 0.5 / step
00a391f5  fstp dword ptr [ecx+0x30]       ; m_fInvTwoDifferenceStep
00a391f8  ret  4                          ; the callee pops the float
```

The `je` looks like the "not greater" branch, which would make the retail
store `1e-3` for a positive argument - the inverse of both upstream texts.
**That reading is wrong**, and the flags are what settle it: the x87
comparison's flags follow **ST(0) ? ST(i)**, so C0 is set when ST(0) is
*less* than ST(i), not when it is greater. This repository's own simulator
says so - `tools/x87sim/x87sim/machine.py` has `_compare(a, b)` set
`c0 = 1 if a < b else 0`, and `fcom st(i)` is decoded as the comparison of
`ST(0)` with `ST(i)` - and the hardware reference confirms it:
`tools/x87sim/hwprobe` runs the retail's own four instructions (`fldz ;
fld [arg] ; fcom st(1) ; fnstsw ax`) and reports status words `0x0000`,
`0x0100`, `0x4000` for `+1`, `-1`, `+0` - C0 and C3 both clear exactly for
the positive argument, so the `je` is the *positive* branch and the argument
is stored. The SDM's phrasing "FCOMP compares ST(i) with ST(0)" is where the
wrong reading comes from: it says which operand is subtracted, not how the
flags are encoded.

**The row is the measurement.** Writing the rebuilt setter the other way
round - a positive argument stores `1e-3` - makes the row fail with 160,086
of 200,000 mismatches, and that 80% is itself the coverage: it is exactly the
share of draws whose argument is not `1e-3`, so both member words are
compared and the argument's value matters in both polarities. The generator
draws `1e-3` (20%), other positive magnitudes (25%: `1e-30` ... `1e30`,
including `FLT_MIN`), negative magnitudes (17%), both zeros (13%),
`±1e38` (15%, where `0.5/step` is itself a float denormal) and plain random
values (10%).

A scan of the whole `.text` finds no direct caller of `0x00A391D0`, so the
setter is reached only through the vtable, and the difference step is not on
any engine path that could change it.

The four instantiations of the pair are `0x00A391D0`/`0x00A39200`
(`<float,Vector3f>`), `0x00A38BB0`/`0x00A38BE0` (`<float,Vector2f>`),
`0x00A397C0`/`0x00A397F0` (`<double,Vector2d>`) and `0x00A39DB0`/`0x00A39DE0`
(`<double,Vector3d>`).

## The denormal range in the Vector3 segment distance, and the line-segment distance

Three results, and the first one is a *negative* one.

**`DistVector3Segment3` - the class the engine calls - is already right.** The
engine builds a `DistVector3Segment3f` and calls its **static** `GetSquared`
and `Get` in `src/sdk/moho/projectile/Projectile.cpp:1539` and
`src/sdk/moho/ui/CUIWorldView.cpp:266`. Its four distance rows - the static
`GetSquared` (0x00A484F0), the static `Get` (0x00A480E0), the swept `Get`
(0x00A48190, the class vtable 0x00D71E6C's +0x08 slot) and the swept
`GetSquared` (0x00A48340, the +0x14 slot) - had generators that never
reached the float denormal range, so the exponent-range defect of the
previous section could not have shown up in them.

The generators now draw that case on purpose: a point a few times `1e-19` off
a segment's line, so the squared distance is around `1e-38` and lives on the
denormal grid; the swept generator draws it with the time and the velocities
at zero, so the moved pair is exactly the pair below. **The rows pass** -
200,000/200,000 and 1,000,000/1,000,000 on seed `0x5EED2007`, on all seven
rows of the class - and **no rebuilt source changed**. That is a result, not
an omission: the squared length the retail computes at the end of its
`GetSquared` (0x00A4867C..0x00A48698 - the three products and the two sums
stay on the x87 stack, and only the result is stored to a float) is exactly
what `Vector3::SquaredLength` already does for the float instantiation, the
products with `Wm3Detail::Mul24`, the sums with `Wm3Detail::Add24` and the
final store to a float.

The mutation check is what says the generator reaches the case that matters:
writing the square as a plain float expression (`fx*fx + fy*fy + fz*fz`)
makes the four distance rows fail - 3379, 4797, 3323 and 3401 mismatches in
200,000, one denormal ulp apart (`0x000939C6` = 8.47245e-40 against
`0x000939C5` = 8.47243e-40) - and leaves `GetClosestPoint1` and the two
derivative rows passing.

**`DistVector3Line3`, `DistVector3Ray3` and `DistVector3Triangle3` are not in
the retail.** There is no vtable to read and no code to compare against, and
the evidence is exhaustive rather than a failed search:

* Every RTTI name in the image was collected (4214 of them). The Distance
  family's names are exactly: `Distance<float,Vector2f>`,
  `Distance<float,Vector3f>`, `Distance<double,Vector2d>`,
  `Distance<double,Vector3d>`, `DistVector3Box3`, `DistVector3Segment3`,
  `DistVector2Box2`, `DistSegment3Segment3` and `DistLine3Segment3` - each of
  the five derived ones in a float and a double instantiation.
* Every Complete Object Locator in `.rdata` was read (signature, offset,
  cdIndex and the type descriptor it points at). The five derived classes are
  the only Distance-derived vtables in the image; their COLs are 0x00E5B0C8
  (`DistVector3Box3`), 0x00E5B328 (`DistVector3Segment3`), 0x00E5B970
  (`DistVector2Box2`), 0x00E5BDB0 (`DistSegment3Segment3`) and 0x00E5BEE0
  (`DistLine3Segment3`).
* The raw byte count of the three names in the image is **zero**, and two for
  each of the five classes that are present (the float and the double
  instantiation's type descriptor).

MSVC emits a type descriptor for every polymorphic class whose vtable it
emits, and the retail does emit RTTI - for these very classes. A class that
is not in the image has no type descriptor, no vtable and no code. Note that
the *rebuilt* tree does have the three classes: they are Wild Magic 4 files
that the retail's trimmed Wild Magic 3.4 library never needed.

**`DistLine3Segment3` - the one retail distance class the sweep had not
covered - had two defects.** The five-class list above leaves exactly one
derived Distance class untouched, and it is the retail's own: the vtable is
0x00D722D4, the COL at 0x00E5BEE0 carries the type descriptor
`.?AV?$DistLine3Segment3@M@Wm3@@`, and the constructor at 0x00A80900 writes
the vtable pointer. Nothing in `src/sdk` names it (only Foundation's
`IntrLine3Capsule3::Find` does, and the retail's own
`IntrLine3Capsule3::Find` at 0x00A7F920 does not call it either), so a
difference here cannot reach the game - but it is a difference, and the rows
found it:

| row | retail | provenance |
|---|---|---|
| `DistLine3Segment3::GetSquared` | 0x00A80DE0 | vtable 0x00D722D4's +0x18 slot |
| `DistLine3Segment3::Get` | 0x00A809A0 | the +0x0C slot; it dispatches +0x18 and jumps to the CRT's square root at 0x00A91000 |
| `DistLine3Segment3::GetSquared(swept)` | 0x00A80C10 | the +0x14 slot; thiscall, three stack arguments, `RET 0xC` |
| `DistLine3Segment3::Get(swept)` | 0x00A80A40 | the +0x08 slot |

The four rows are appended at the end of `kTargets[]`, so no existing row's
seed moved, and the harness drives the retail's own constructor - the same
shape as the other distance constructors: `ECX` = `this`, the line and the
segment on the stack, stored at `[this+0x34]` and `[this+0x38]`, `RET 8`.

* **The three dot products are inlined in the retail, and their order is not
  ours.** The retail sums `(y*y' + x*x') + z*z'` (0x00A80E05..0x00A80E19 for
  `fA01`, 0x00A80E28..0x00A80E51 for `fB0`); our `Vector3::Dot` sums
  `(z*z' + y*y') + x*x'`. The two groupings differ by an ulp often enough to
  matter: with the shared `Dot`, the static `GetSquared` row failed with
  **39,699** mismatches in 200,000, always a few ulps apart (`0x43036620` =
  131.399 against `0x43036630`). `Wm3DistLine3Segment3.cpp` now spells the dot
  out in the retail's order, with the products and the sums on the x87 stack
  under `_PC_24`.
* **The squared distance is one x87 chain in the retail.** The products and
  the sums of the final expression stay on the stack (0x00A80F0F..0x00A80F5F)
  and only the result is stored back to a float (0x00A80F63); the reciprocal
  of the determinant is kept on the stack too (0x00A80F07, `FDIVRP`). The
  rebuild computed the expression in float. The row's generator scales the
  whole configuration - the two origins and the extent, but not the
  directions, which decide the branch - down by a factor around `1e-21`, so
  that the squared distance is around `1e-40` and every product the
  algorithm forms on the way there lives on the denormal grid. The row then
  failed with **4,502** mismatches in 200,000, one denormal ulp apart
  (`0x00754208` = 1.07684e-38 against `0x00754207`). The float instantiation
  now models the same rounding with the `Wm3Detail` helpers; the double
  instantiation keeps the plain arithmetic.

The generator also draws the extent **positive**. A negative extent puts the
algorithm's own branch tests outside the range the two agree on - the retail
returns an infinity and the rebuild a NaN - and that is an overflow of the
inputs, not a difference in the arithmetic.

After the two fixes the four rows pass 200,000/200,000 and 1,000,000/1,000,000
bit-identical on seed `0x5EED2007`, and the whole table is **77/77** at
200,000. The mutation checks: reverting the dot to the shared
`Vector3::Dot` gives 39,699 mismatches in 200,000, and restoring the plain
float final expression while keeping the dot fixed gives 4,502 - each half of
the fix is necessary on its own. Perturbing one side alone (adding `1e-30`
to our result) makes the static `GetSquared` row fail with 316 mismatches in
2,000, so the comparison is bit for bit.

One difference is left open on purpose, as in the previous section: the
swept queries still move the two shapes with a plain float `fT*velocity`,
while the retail computes the product on the x87 stack, stores it to a float
and adds it there. That difference is only observable when the product or the
sum leaves the float normal range, and there the movement is absorbed by the
geometry - the squared distance is then far below the denormal grid - so no
row can see it.

**The lockstep game, after the three x87 fixes.** The rebuilt Wild Magic
library's contents changed - three distance classes now model the retail's
x87 intermediates - so the engine binary moved and the lockstep suite had to
be run again: `2p-main-x86-vs-x86` (the rebuild against the retail, five and
a half minutes of real play) ends in **PASS** after a fresh
`main.vcxproj` Release|Win32 build (exit code 0, no `: error` line) and a
deployment whose `main.exe` and `main.pdb` match the build's outputs. That is
what the fixes predict: the differences they remove live in the float
denormal range, which the game does not reach.

**The lockstep game again, after the `DistLine3Segment3` fixes.** The rebuilt
library's contents changed a second time - the dot product's summation order
and the squared distance's x87 chain - so the engine was rebuilt, deployed
and the lockstep suite run once more: `2p-main-x86-vs-x86` ends in **PASS**
in 5m16s. Nothing in `src/sdk` names `DistLine3Segment3` at all, so these two
defects could not have reached the game even before they were fixed; the
suite is here to prove that fixing them did not change anything that does.

## The retail's Wild Magic class inventory, and two more corrections

The addresses for new rows used to be found one class at a time, by following
call sites and hoping. A survey of the retail's own **RTTI** says better, and
it found two more mistakes in this file while doing it.

**The method.** The image holds 4234 RTTI type descriptors. Each one is
reachable from a Complete Object Locator, and each COL is pointed at by a
dword in `.rdata` - the **RTTI slot**, which is what precedes the vtable (the
COL struct itself lives elsewhere in `.rdata`; for `Distance<float,Vector3f>`
the slot is `0x00D71C50`, the COL is `0x00E5AE38` and the vtable base is
`0x00D71C54`). Two traps: the x86 COL signature is **0**, not 1, so a scan
that filters on the signature finds almost nothing; and two COL records are
compiler-internal base-subobject records with a garbage `cdOffset` and zero
references in the image (`0x00E5AF50`/`0x00E5AFD0` and `0x00E5B7F0`/`0x00E5B870`),
which must not be reported as classes. Filtering on
`cdOffset < 0x10000 && offset < 0x10000 && pCHD != 0` leaves 4062
well-formed COLs, of which 28 belong to `Wm3` classes.

**67 `Wm3` RTTI names resolve to 45 distinct retail classes** - 28 with a
vtable and a COL, 7 with a type descriptor only (the value types, which have
no vtable: `Vector3`, `Vector2`, `Sphere3`, `Box3`, `AxisAlignedBox3`,
`Quaternion`, `IVector2`, `IVector3`). Every retail class has a counterpart in
the rebuild, and the rebuild declares 163 `WM3_ITEM` classes plus 90 Math
value types, 145 of them with a vtable.

### The two corrections

* **`Distance<double,Vector2d>` and `Distance<double,Vector3d>` were
  swapped in this file.** The RTTI slot `0x00D71C78` holds the COL
  `0x00E5AE80`, whose type descriptor names
  `.?AV?$Distance@NV?$Vector2@N@Wm3@@@Wm3@@`, so `0x00D71C7C` is the
  **Vector2d** base and `0x00D71CA4` the **Vector3d** one. The Class Hierarchy
  Descriptor confirms it independently: `DistVector2Box2@N` derives from
  `Distance@NV?$Vector2@N@Wm3@@@Wm3@@`, and its own vtable's +0x04 is
  `0x00A39270`, which is exactly `0x00D71C7C`+0x04. The window bodies and the
  difference-step pairs follow the same swap. **No wired row uses a double
  instantiation, so no passing row was affected** - but a future double row
  would have been wired to the wrong body.
* **`IntrSegment3Box3`'s vtable base is `0x00D71DDC`, not `0x00D71DE0`.**
  The RTTI slot `0x00D71DD8` holds the COL `0x00E5B160`, and the constructor
  at `0x00A463A0` writes `mov dword ptr [esi], 0xd71ddc`. `0x00D71DE0` is
  that table's **+0x04** slot, so the slot labels for this class were
  computed off the wrong base: from `0x00D71DDC`, `+0x08` is `0x00A46440`
  (`Test`) and `+0x0C` is `0x00A45530` (a base stub), while `Find` sits at
  **+0x10** = `0x00A462A0`. The addresses recorded for the rows are right.

### What the inventory says about the next rows

| class | retail vtable | first virtual | note |
|---|---|---|---|
| `IntrLine3Capsule3` | `0x00D722A4` | `Test` @+0x08 = `0x00A7F760` | the class's only unwired virtual; `Find` is wired |
| `ConvexHull3<float>` | `0x00D72058` | dtor @+0x00 = `0x00A665D0` | the real work (`MakeHull`, `Update`, `Merge`) is **not** virtual, so it must be reached by address from the constructor at `0x00A66440` |
| `Intersector` (4 inst) | `0x00D71D2C`/`0x00D71D44`/`0x00D71D5C`/`0x00D71D74` | `0x00A456B0`... | the base's four slots are `xor al,al; ret` stubs |
| `Query`, `Query2*`, `Query3*` (17 names) | `0x00D71F2C`, `0x00D72134`... | 8 slots | real geometry, but the engine never names them |

**117 rebuild classes with a vtable have no retail RTTI name** - all 39
non-retail `Dist*` variants (including `DistVector3Line3`,
`DistVector3Ray3`, `DistVector3Triangle3`, whose name bytes occur zero times
in the image), all 57 non-retail `Intr*` variants, plus `Polyhedron3`,
`ConvexPolyhedron3`, `LinComp`/`2`/`3`, `BSplineGeodesic`, `IntpAkima*`,
`Intersector1`, `Edge`/`Vertex` and `Query2Filtered`/`Query3Filtered`. No
retail address exists for any of them, so no row is possible; they are Wild
Magic 4 files the retail's trimmed library never needed, and the rebuild
carries them as dead code.

**One caveat about the WM3.4 copy.** The retail has 17 `Query` RTTI names
with real eight-slot vtables, while the Wild Magic 3.4 copy in
`.tmp/wm3/Source/` has no Query files at all - `grep -rl "Query"` over the
whole 3.4 tree returns nothing. So the 3.4 copy is a **reference for
behaviour**, not the retail's exact ancestor: for the Query family the
retail is the only reference there is, and the rebuild's WM4-derived Query
classes are right to be there.

## Three more rows: the line-capsule Test, the hull construction and the Query3 predicate

Three classes the class inventory listed as reachable but unwired are now
rows. All three pass 1,000,000/1,000,000 on the default seed `0x5EED2007`,
and the whole table is 80/80 at 200,000 and at 1,000,000.

| row | retail | provenance | ABI |
|---|---|---|---|
| `IntrLine3Capsule3::Test` | `0x00A7F760` | class vtable `0x00D722A4` +0x08; the ctor at `0x00A7F730` installs that vtable | thiscall, no stack arguments, the bool in EAX |
| `ConvexHull3<float>::ConvexHull3` | `0x00A67120` | the class has one virtual slot (the dtor at `0x00A66440`), so the construction is reached by address; the argument order comes from the base ctor `0x00A6BBC0` | thiscall, five stack arguments, `RET 0x14` |
| `Query3<float>::ToPlane` | `0x00A55CE0` | class vtable `0x00D71F38` +0x08; the COL `0x00E5B538` names `.?AV?$Query3@M@Wm3@@` | thiscall, four stack arguments, `RET 0x10`, the sign in EAX |

**No rebuilt source changed** - the rebuild agreed with the retail on all
three.

### What the rows compare

* `IntrLine3Capsule3::Test`: `line[6] capsule[8]`, one boolean. The retail's
  body builds a `DistLine3Segment3<float>` (ctor `0x00A80900`) and dispatches
  its **+0x0C** slot (`0x00A809A0` - the distance, itself a row) and compares
  the result with the capsule's radius at `[capsule+0x1C]`; the rebuild's
  `Test` does the same through its own `DistLine3Segment3`. The Find row's
  generator is reused, so the exact tangent construction (distance == radius)
  is in the draws. Coverage over 200,000 draws: 95,175 hits, 104,825 misses.
  Mutation (the rebuild's side alone uses `<` instead of `<=`): 19,933
  mismatches. Scaling the rebuild's radius by `1+1e-6` does **not** flip the
  boolean - a boolean row only has teeth at the equality boundary, which is
  why the `<=`/`<` mutation is the meaningful one.
* `ConvexHull3`: `count points[18] eps type` in, and the dimension, the
  simplex quantity, the fifteen line/plane floats and the triangles out. The
  hull's own index order comes from a `std::set` of triangle **pointers**, so
  it follows the heap addresses and cannot be compared between the two sides;
  each triple is rotated so that its smallest index comes first - which keeps
  the winding - and the triples are sorted. The generator builds hulls with a
  known shape: a triangle, a square, a pentagon and a hexagon on the unit
  circle, a tetrahedron, six corners of a cube, four collinear points, a
  duplicated point, and a generic set in a cube. The epsilon is the Mapper3's
  relative tolerance, `1e-6` - with zero a single point would be taken for a
  zero-length line and the direction's normalisation would divide by zero.
  Coverage over 200,000 draws: dimension 0 in 20,093 draws, 1 in 39,990, 2 in
  79,911, 3 in 60,006; simplex quantities 0, 4, 6 and 8. Mutation (the
  rebuild's hull built from the points in the opposite order): 97,250
  mismatches.
* `Query3<float>::ToPlane`: `verts[12] point[3] iV0 iV1 iV2`, one word. The
  retail builds this class inline (`0x00A67433`: the vptr, the count at
  `+0x04`, the vertex array at `+0x08`), so there is no separate constructor
  and the row builds the three fields the same way. The body computes
  `Det3(P-V[iV0], V[iV1]-V[iV0], V[iV2]-V[iV0])` through the shared helper
  `0x00A54050` and returns +1/-1/0. The generator puts the point exactly on
  the plane (the determinant is exactly zero), one denormal ulp off it, on a
  vertex, on a degenerate triangle and off the plane in general. Coverage
  over 200,000 draws: the sign is -1 in 30,844 draws, 0 in 105,298 and +1 in
  63,858. Mutation (the rebuild's sign negated): 94,702 mismatches, which is
  exactly the number of non-zero draws.

The double instantiations are recorded but not wired: the retail's are
`0x00A7F830` (`Test`), `0x00A67870` (the hull constructor) and a
`Query3<double>` of its own; their objects hold `double`s, so a double row
needs its own generator, its own double-sized layouts and its own harness
entry. Every row in this harness is float-only by construction.

### The retail's static TLS, and why the harness had to change

The hull's first triangle is the first thing the retail allocates, and that
allocation faulted at `0x00958A4A`. The retail's allocator reads the thread's
**static** TLS array - `mov eax,[0x00FB7DB4]` / `mov ecx,fs:[0x2C]` / `mov
ebp,[ecx+eax*4]` at `0x00958A35` - and `0x00FB7DB4` is the `AddressOfIndex` of
the retail's TLS directory, so the index published there has to be a **static**
slot, the kind the loader hands a module with a TLS directory.

The harness published a `TlsAlloc` index there. On Windows 8 and later the
dynamically allocated slots live outside the array at `fs:[0x2C]`: the retail
read a null block (the fault showed `eax = 14`, `fs:[0x2C][14] = 0`, while
`TlsGetValue(14)` returned the block the harness had set), and the process
died on the first `new`.

The harness now publishes its own static TLS slot - the loader gives it one
for its own `__declspec(thread)` block - and the retail's block is that
zeroed block. That is what the retail's allocator wants: it creates its heap
on the first use while the state at `[block+0x1CC]` is still zero. The
retail's own TLS template is not in the image (the directory's raw-data
address, `0x0123C000`, lies outside it), so the zeroed block is the closest
the harness can get to the loader's copy of it. The CRT's own thread-local
index at `0x00F3E49C` is dynamic and stays as it was.

This is a **fidelity** change and not a comparison change: no compiler
option, no comparison rule and no Wild Magic source was touched, and every
row still passes - so no row moved.

## Thirteen more rows: the rest of the Query family and the two hull siblings

The `Query3<float>` and `Query2<float>` classes are wired in full now, and so
are the `ConvexHull1<float>` and `ConvexHull2<float>` constructions. The
table holds **93** rows, all passing 1,000,000/1,000,000 on every seed tried.

Two things were worth the round.

**The retail's table is not the rebuild's table.** `Query3<float>`'s class
vtable is `0x00D71F38` and the RTTI slot that precedes it - the dword at
`0x00D71F34`, holding the Complete Object Locator `0x00E5B538` - names
`.?AV?$Query3@M@Wm3@@`. Read off that vtable, the retail puts the
*point-taking* overload of every predicate **before** the index-taking one,
which is the opposite of the WM4 declaration order the rebuild inherits. So
both sides have to be called by address and never by slot, and the slot
order has to be read off the class's *own* vtable. The same is true of
`Query2<float>` (`0x00D72134`) and of the two hull classes.

**The retail's compound predicates are disjunctions, not conjunctions.**
`Query3<float>::ToTetrahedron` (`0x00A530D0`) and `Query2<float>::ToTriangle`
(`0x00A74EA0`) return -1 as soon as *any* of the face signs is non-zero and
0 only when all of them are zero; the WM4 text the rebuild inherited spells
the conjunction, which returns 0 as soon as any sign is zero. The two differ
exactly on a point that lies on a face or an edge of the simplex - and there
the retail calls it "inside" (-1) where the conjunction calls it "on the
simplex" (0). `Foundation/RationalArithmetic/Wm3Query3.inl` and
`Wm3Query2.inl` now spell the disjunction. The retail's
`Query3Int64<float>::ToTetrahedron` (`0x00A534C0`) has the same final block,
so this is the retail's WM3.8 spelling of the predicate; the rebuild's
`Query3TRational` still spells the conjunction and is left alone, because
the retail's `Query3TRational<float>` body (`0x00A64610`) has not been read
and the mechanism is not established for it.

Both changes were established from the disassembly rather than guessed: the
bodies' arguments were recovered by stack-simulating them, which is how the
four values the final block compares - stored in the dead argument slots
`a5`, `a1`, `a3` and `EAX` - were identified as the four face signs.

The hull rows compare the dimension, the simplex quantity and the index
array, and the index array has to be canonicalised: the hull's own order
follows the heap addresses (`ConvexHull3` keeps its triangles in a `std::set`
of pointers) or the traversal order of its edge list (`ConvexHull2`), so
each pair is normalised to put the smaller index first and the list is
sorted. For `ConvexHull1` the two indices are sorted for a second reason:
the hull's order there is by value, and the sort that produces it is not
stable, so two hulls that agree on the segment can name a different one of
two equal-valued vertices.

The degenerate configurations the generators reach **exactly** are the point
of these rows: a point on a face plane or on an edge (the determinant is
exactly zero), one ulp off it (a denormal determinant), a point at a vertex,
a degenerate face (two equal vertices), three collinear vertices, four
coplanar vertices (the 4x4 determinant has a zero row), four cocircular
points (all the `z` terms are differences of equal products), a circumcentre,
cube corners, an interior point, a point exactly on a hull edge, duplicate
and identical vertices, and a range below the epsilon (where the dimension
test must reject the set).

### What widening the harness to the other instantiations would take

The retail also has `Query2Int64`, `Query2TInteger`, `Query2TRational`,
`Query3Int64`, `Query3TInteger` and `Query3TRational`, each with a float and
a double instantiation, while the rebuild's `Query2Filtered`/`Query3Filtered`
have **no** retail counterpart at all. Every row in this harness is
float-only by construction, so this is recorded rather than built:

* **Layouts.** The integer and rational classes have the same three-field
  object (vptr, count, vertex array) but their vertex arrays are *scaled* by
  the hull (`ConvexHull2`/`3` scale to `[0,1]^n`, then to `[0,2^20]` for
  `Int64` and `[0,2^24]` for `TInteger`), so a row has to choose the scaling
  itself rather than take the point set as given. `TRational` carries
  `TRational<Real>` pairs (numerator and denominator) rather than a bare
  `Real`, so its input words double and the `Det2`/`Det3`/`Det4` predicates
  are exact rational ones, not float ones. `Filtered` takes an extra epsilon
  and keeps a filtered-exact accumulator, so its "sign" is a filtered value,
  not an `int`.
* **Generators.** The float generators do not transfer: the point sets have
  to be *scaled* exactly as the hull does (otherwise the integer predicates'
  magnitude assumptions break), and the degenerate cases have to be
  constructed in the scaled frame - an exactly-coplanar quadruple must still
  be exactly coplanar after the scaling, which needs the scale factor to be
  a power of two (it is: `2^20`/`2^24`) and the point set to be a dyadic
  rational.
* **Object layout differences.** `Query3<double>`/`Query2<double>` have a
  different vertex stride (24/16 bytes instead of 12/8) and a different
  vertex array type, so the retail object fabrication has to change with the
  instantiation; the `Int64`/`TInteger`/`TRational` classes have **different
  member sets** - in the rebuild `Query3Int64`/`Query3TInteger` have only
  `ToPlane` and `ToCircumsphere` (no `ToTetrahedron`, no index-taking
  overloads at all), while the retail's `Query3Int64<float>` has all eight
  slots (`0x00D71F80`: `0x00A56300`, `0x00A53490`, `0x00A534C0`,
  `0x00A53570`, `0x00A56430`, `0x00A535A0`). So the **API surface** differs
  between the rebuild and the retail for those classes, independently of any
  row.
* **How many rows.** Each instantiation times each of the six predicates is 12
  extra classes times 6 = 72 rows for the integer and rational families, plus
  another 72 if the doubles are wanted. Each needs its own retail thunk and
  its own harness wrapper, since none of them can share the float one.

### The engine's build deploys on its own

`main.vcxproj` has a `PostBuildEvent` that stages the binary to
`%ProgramData%\FAForever\bin\` (the `FafStageToRunDir` property), so **an
engine build deploys by itself** - the file in the run directory is
byte-identical to the build's output without anyone copying it. This round's
two source changes moved the binary by 163 bytes, so the lockstep suite was
run again even though the engine never calls `ToTetrahedron` or `ToTriangle`
(`grep` over `src/` finds no call to either, and the only Foundation user of
the Query classes is the convex hull machinery, which uses `ToPlane` and
`ToLine`).

**The lockstep game, after the two disjunction fixes.** The rebuilt library's
contents changed again - the two compound predicates - so the engine binary
moved and the lockstep suite was run once more: `2p-main-x86-vs-x86` ends
in **PASS** in 5m17s. The engine never calls `ToTetrahedron` or `ToTriangle`
at all, so the two fixes could not have reached the game; the suite is here
to prove that fixing them did not change anything that does.

## The rational compound predicates are disjunctions too

The float instantiation's fix did not settle the rational variant: the
rebuild's `Wm3Query3TRational.inl` and `Wm3Query2TRational.inl` still spelled
the **conjunction**, because nobody had read the retail's rational bodies end
to end. Now they have.

`Query3TRational<float>::ToTetrahedron` is the class vtable `0x0D72010`'s
+0x10 slot = `0x0A64610`. That wrapper converts the query point to an exact
`TRational<float>` and calls the private
`ToTetrahedron(const RVector&, iV0,iV1,iV2,iV3)` at `0x0A61AF0`, which calls
the rational `ToPlane` analogue `0x0A61810` four times with the vertex
triples (iV1,iV2,iV3), (iV0,iV2,iV3), (iV0,iV1,iV3) and (iV0,iV1,iV2).

The four signs are stored in the dead argument slots (arg5, arg1 and arg3) and
in `EAX`, and the final block at `0x0A61B64`-`0x0A61B7B` is

```
cmp  dword ptr [esp+0x24], 0   ; iSign0
jne  -> -1
cmp  dword ptr [esp+0x14], 0   ; iSign1
jne  -> -1
cmp  dword ptr [esp+0x1c], 0   ; iSign2
jne  -> -1
test eax, eax                  ; iSign3
jne  -> -1
ret  0x14                      ; eax = 0
```

with the `-1` produced by `or eax,0xffffffff` at `0x0A61B87`. **The retail
returns -1 when *any* of the four face signs is non-zero and 0 only when all
four are zero - a disjunction.** The four early exits (`iSign0>0`,
`iSign1<0`, `iSign2>0`, `iSign3<0` -> `+1`) are exactly the Wild Magic 4
ones, so the whole body is WM4's `ToTetrahedron` with only the last line
changed.

`Query2TRational<float>::ToTriangle` is the same one dimension down: vtable
`0x0D7220C`, slot +0x10 = `0x0A7C9E0`, the private helper at `0x0A7BB30`
calls the rational `ToLine` analogue `0x0A7B940` three times, and the final
block at `0x0A7BB8F`-`0x0A7BB9C` is `cmp`, `test`, `test` with three `jne`
to the `or eax,0xffffffff` at `0x0A7BBA8` - again a disjunction. The
`double` instantiations have the identical shape, so it is not a float-only
artifact.

**The signs are exact.** `0x0A61810` contains no x87 instruction at all (the
whole 0x5D0-byte body greps clean for `fld|fstp|fadd|fmul|fcmp`): the
determinant is a `TRational<TInteger>` value and the sign comes from the two
`TRational` comparison operators `0x0A5AD90` (-> `+1`) and `0x0A5AC40`
(`neg al; sbb eax,eax` -> -1 iff non-zero, else 0). So **zero means the
determinant is exactly zero** - no tolerance, no filtered accumulator - and
the float input is converted to its exact rational value by
`TRational<float>(float)`, which introduces no rounding at the boundary.

**Two honest caveats.** The *polarity* of the retail's rational `ToPlane`
(which branch is `>0`) was not pinned down by reading the two comparison
operators down to their big-integer comparisons; it is inferred from the fact
that the retail's rational combinator's four early-exit conditions are
character-for-character the WM4 ones, which requires the same sign convention
as the WM4 text the rebuild inherits - the same convention the verified float
instantiation uses. And **no row covers the rational classes**, because the
harness is float-only by construction: this change rests on the
disassembly, not on a comparison. The fix belongs in the rebuilt source
either way - the two files now spell `||` - and the engine's binary does not
move, because the rational Query classes are not instantiated in it, so the
lockstep baseline stands.

## The double instantiation: the x87, not binary64

The retail emits a *double* instantiation of the Wild Magic 3.8 distance
classes, and the harness has never compared one. The first six rows are in:

| row | retail | class vtable | slot |
|---|---|---|---|
| `Distance<double>::SetDifferenceStep` | `0x00A39DB0` | `0x00D71CA4` | the base's own |
| `Distance<double>::DifferenceStep (default)` | `0x00A39FA0` | `0x00D71CA4` | the base's constructor |
| `Distance<double,Vector2>::SetDifferenceStep` | `0x00A397C0` | `0x00D71C7C` | the base's own |
| `Distance<double,Vector2>::DifferenceStep (default)` | `0x00A39F60` | `0x00D71C7C` | the base's constructor |
| `DistVector3Box3<double>::GetSquared` | `0x00A460D0` | `0x00D71DB4` | +0x18 |
| `DistVector3Box3<double>::Get` | `0x00A45800` | `0x00D71DB4` | +0x0C |

The vtables are RTTI-checked: `0x00D71DB4`'s Complete Object Locator names
`.?AV?$DistVector3Box3@N@Wm3@@` and `0x00D71CA4`'s names
`.?AV?$Distance@N?$Vector3@N@Wm3@@@Wm3@@` (`N` is MSVC's mangling for
double). The retail's double bodies are x87 code, and the engine runs under
`_PC_24`, so every arithmetic result is a **24-bit significand with the
extended exponent** - not a binary64 number. A hardware probe settles the
model: under PC 24 the x87 `fadd` of `1.0` and `2^-30` is `1`, under PC 53
it is `1.0000000009313225746`, and `fadd qword` under PC 24 is `1.0` as
well.

So a binary64 expression cannot reproduce the retail's double path at all:
`0.5/1e-3` is `500.00000000000006` in binary64 and exactly `500.0` on the
x87, and `500.0` is the value the retail's own constructor stores.
`Foundation/Math/Wm3Vector3.h` therefore grows `Wm3Detail::X87Add`,
`X87Sub`, `X87Mul`, `X87Div`, `X87Neg`, `X87Abs` and `X87Sqrt` - each one
an inline x87 instruction under the ambient control word - and the double
branches of `Foundation/Distance/Wm3Distance.cpp` and
`Foundation/Distance/Wm3DistVector3Box3.cpp` use them. The float branches
are untouched (`if constexpr` keeps the float instantiation's expression
tree exactly as it was), and the float rows still pass 200000/200000.

**The CRT square root keeps the precision control.** The section above
reads the branch at `0x00A91028` as the CRT's error path; it is not. The
helper it calls is:

```
0x00aa7bd5  8b542404     MOV EDX,[ESP+4]      ; the saved control word
0x00aa7bd9  81e200030000 AND EDX,0x300       ; keep the precision bits
0x00aa7bdf  83ca7f       OR  EDX,0x7F        ; the default masks
0x00aa7be7  d96c2406     FLDCW word ptr [ESP+6]
```

The routine normalises the control word *except* for the three precision
bits, so under the engine's `_PC_24` the `FSQRT` rounds to 24 bits, and it
restores the saved word afterwards (`0x00AA7C90`). A binary64 square root
therefore differs from the retail's on 85% of the inputs - which is exactly
what the `Get` row said before `X87Sqrt` went in.

**Two limits, recorded.** The emulation models the x87's 24-bit significand
but not its extended exponent, so a value that leaves the binary64 range in
the middle of a computation comes back as an infinity where the retail keeps
a finite number on its stack; the generator keeps the magnitudes inside
`[1e-100, 1e100]`, where every intermediate is a binary64 number and the two
agree bit for bit. And the engine's binary moves: the double instantiation
lives in the same object as the float one, the linker keeps it, and the code
it emits changed size, so the `.text` section shifts by 512 bytes. The float
behaviour does not move - the float rows pass - but the lockstep baseline is
invalidated by the move and has to be re-run.

**Plumbing.** A double is two 32-bit words, the low half first
(`difffuzz::StoreDouble` / `LoadDouble` in `fuzz.h`), so a layout counts
words: `vec[3] box[15]` is thirty-six words for the double row and eighteen
for the float one. `GenDouble`, `GenVec3d` and `GenUnitVec3d` in `gen.cpp`
draw binary64 values, and the new retail thunks (`Distance3dCtor`,
`Distance3dSetDifferenceStep`, `Distance3dGetDifferenceStep`, the Vector2
trio, `DistVector3Box3dCtor`, `DistVector3Box3dGetSquared`,
`DistVector3Box3dGet`) call the retail's bodies with the same thiscall
shapes. The new rows sit at the **end** of `kTargets[]`, so no existing
row's seed moves.

**Still missing.** The remaining four `Dist*<double>` classes
(`DistVector3Segment3`, `DistVector2Box2`, `DistSegment3Segment3`,
`DistLine3Segment3`), the base's window queries in the double
instantiation (`0x00A39860` / `0x00A39AB0` for the Vector3 base,
`0x00A39270` / `0x00A394C0` for the Vector2 one, plus the base's
`GetDerivative` at `0x00A39D00` / `0x00A39710`), `Query2<double>` and
`Query3<double>`, `ConvexHull1/2/3<double>` and the two `TRational<double>`
classes. The `Query2`/`Query3` instantiations are header-only in the
rebuild - `Foundation/RationalArithmetic/Wm3Query2.inl` and
`Wm3Query3.inl` - so the harness's own translation unit instantiates them,
and the rebuild's `Query3Int64`/`Query3TInteger` have no `ToTetrahedron` or
index overloads at all, so those rows are impossible as the rebuild stands.

**The lockstep game, after the double instantiation's x87 modelling.** The
rebuilt library's contents changed again - the double branches of
`Wm3Distance.cpp` and `Wm3DistVector3Box3.cpp` now model the retail's x87
arithmetic - so the engine binary moved (its `.text` shifts by 512 bytes,
because the double instantiation shares the object with the float one and the
linker keeps it) and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m29s. The engine is float-only, so
no double code is on any engine path; the suite is here to prove that
fixing it did not change anything that does.

## The double instantiation, part two: the segment class, and a swept body that says zero

Two more rows are in, `DistVector3Segment3<double>::GetSquared` and
`::Get`, both 1000000/1000000 bit-identical; the whole table is 101/101.

The addresses come from the class vtable `0x00D71E94`, which the survey's
RTTI rows name `.?AV?$DistVector3Segment3@N@Wm3@@` and whose constructor at
`0x00A48080` writes (`C7 06 94 1E D7 00`).  The vtable was read out of the
image itself, slot by slot:

| slot | address | what it is |
|---|---|---|
| +0x08 | `0x00A486B0` | `Get(fT, rkVelocity0, rkVelocity1)` |
| +0x0C | `0x00A48100` | `Get()` - dispatches +0x18 and jumps to the CRT sqrt |
| +0x14 | `0x00A487E0` | `GetSquared(fT, rkVelocity0, rkVelocity1)` |
| +0x18 | `0x00A48910` | `GetSquared()` |

The base's own three bodies sit at +0x04, +0x10 and +0x1C and are the
Vector3d base's `0x00A39860`, `0x00A39AB0` and `0x00A39D00`.  The
constructor stores the two references at `[this+0x68]` and `[this+0x6C]`,
which is where the static `GetSquared` reads them.

The defect was the same x87-versus-binary64 shape as the difference step's,
in the same places.  The rebuild's `double` branch of
`Wm3DistVector3Segment3.cpp` was plain binary64; the difference, the
parameter, the closest point and the squared distance are all 24-bit
significands on the retail side, because the engine runs under `_PC_24`.
Two details the retail's instruction sequence settles and the source does
not:

* the parameter sums **x-first** - `FADDP ST(2)` at `0x00A48932` adds the y
  term to the x term, `0x00A48937` adds the z term;
* the squared distance is **(x\*x + y\*y) + z\*z** - the x and the y squares
  are formed at `0x00A489C9` and `0x00A489CD` and the z square is added last
  at `0x00A489D5` - and not the standalone `LengthSq`'s
  `z\*z + (y\*y + x\*x)`.

`Get()` takes the CRT's square root, which is a 24-bit significand too, so
the double branch uses `X87Sqrt`.

**The degenerate branches are reached exactly, and in double that takes no
denormal.**  The segment parameter gates the three closest-point branches,
and its boundaries are `t == -extent`, `t == 0` and `t == +extent`.  The
generator puts the direction on the x axis, the origin at the origin and
the point on the axis, so the parameter is the point's x - exactly, with a
multiply by `1.0` and a subtraction of `0.0` - and the extent is set to
that very number, so `t` lands exactly on the boundary.  The magnitudes
stay inside `[1e-100, 1e100]`: that is the range inside which the rebuild's
double path is exact, because the emulation models the x87's 24-bit
significand but not its extended exponent, so a value that leaves the
binary64 range mid-computation would come back as an infinity where the
retail keeps a finite number.  The branches the limit keeps out of reach
are the ones that need a value outside the binary64 range in the middle of
the computation - an intermediate that overflows or lands on the subnormal
grid.

**The swept double bodies are not broken - the harness's thunk was.**  The
two earlier rounds recorded that the retail's swept (time dependent) double
bodies - `DistVector3Segment3<double>::GetSquared` at `0x00A487E0` and `Get`
at `0x00A486B0`, `DistVector3Box3<double>::GetSquared` at `0x00A45FB0` and
`Get` at `0x00A45E90` - "return exactly zero for every input", and left the
rows out of the table because a retail bug should not be copied into the
rebuild.  That reading was wrong: the zero was manufactured by the harness.

What the retail's bodies do, read out of the image:

  * `0x00A487E0` moves the point by `fT*rkVelocity0` and the segment's
    origin by `fT*rkVelocity1` on the x87 (`0x00A48812..0x00A48898`),
    assembles the moved `Segment3` on the stack, runs the base class's
    constructor on the temporary (`0x00A4889C`), installs the temporary's own
    vtable pointer (`0x00D71E94`) and its two references - the moved point at
    `[this+0x68]` and the moved segment at `[this+0x6C]` -
    (`0x00A488A9..0x00A488B8`), and calls the class's own static `GetSquared`
    (the +0x18 slot, `0x00A48910`) on it (`0x00A488CE`); the result comes
    back in ST(0).
  * `0x00A45FB0` does the same for the box and ends in the class's own
    static `GetSquared` (`0x00A460D0`).

The retail's answers are right.  With the point `(0,1,0)` against the
segment from the origin along the x axis with extent 1 and no motion, the
retail's swept `GetSquared` returns 1 - the same as its static `GetSquared`
on the same pair - and the temporary the swept body builds was read back out
of the retail's own stack and found to hold the moved point and the moved
segment.

The zero was the harness's.  The naked thunk that calls these four bodies
ended in

    fstp qword ptr [esp + 40]      // the distance -> our `out'

which stores the returned double into the CALLER's out-pointer slot - the
slot that holds the pointer - and not into the double the pointer refers
to.  The row therefore compared the rebuild's answer against the `0.0` the
row's own local still held.  The float thunks in the same file get this
right (`mov edi, [esp + 36]` and then `fstp dword ptr [edi]`), and so do the
other double thunks; these four were the only ones written with the wrong
store.

With the thunks corrected, the four rows are in the table and agree with the
rebuild bit for bit:

  DistVector3Segment3<double>::GetSquared(swept)  0x00A487E0  1000000/1000000
  DistVector3Segment3<double>::Get(swept)          0x00A486B0  1000000/1000000
  DistVector3Box3<double>::GetSquared(swept)       0x00A45FB0  1000000/1000000
  DistVector3Box3<double>::Get(swept)              0x00A45E90  1000000/1000000

and the table is 105/105 at 1000000 on the default seed `0x5EED2007` and at
200000 on all five seeds.  Note the box's two addresses: the +0x08 slot is
the swept `Get` (`0x00A45E90`) and the +0x14 slot the swept `GetSquared`
(`0x00A45FB0`) - the earlier note had them the other way round.

No rebuild change was needed for these four rows: the rebuild's swept double
path already moved the two shapes on the x87 and asked the class's own static
`GetSquared` for the distance, which is what the retail does.  The comments
in `Foundation/Distance/Wm3DistVector3Segment3.cpp` and
`Foundation/Distance/Wm3DistVector3Box3.cpp` now record the mechanism, so
that nobody reads the old claim and "fixes" the rebuild to return zero.

The lesson for the harness: a row that disagrees with the retail is evidence
about the retail only after the harness's own plumbing has been checked, and
the cheapest check is to look at what the row actually received - a row that
"always says zero" is far more likely to be a row that never received an
answer than a retail that always says zero.  This is the same reasoning that
recovered the `ContAlignedBox` mask argument and the `bSolid` parameter:
read the retail's function body before believing what the comparison says.

`kMaxWords` goes from 48 to 56: a double is two words, and the swept
`DistVector3Box3<double>` row - the vector, the box, the time and the two
velocities - is fifty words.

## The `DistVector2Box2<double>` rows are in

The static and the swept rows of `DistVector2Box2<double>` are in the table
and agree with the retail bit for bit.

The addresses come out of the class vtable `0x00D720EC` (the RTTI slot in
front of it names `.?AV?$DistVector2Box2@M@Wm3@@`, and the constructor at
`0x00A6C630` writes the pointer):

| slot | address | body |
|---|---|---|
| +0x08 | `0x00A6CB50` | the swept `Get(fT,v0,v1)` |
| +0x0C | `0x00A6C6A0` | the static `Get` |
| +0x14 | `0x00A6CC60` | the swept `GetSquared(fT,v0,v1)` |
| +0x18 | `0x00A6CD70` | the static `GetSquared` |

The two swept bodies are thiscall with three stack arguments - the double
`fT` and the two velocity pointers - and `RET 0x10`; the double comes back
in ST(0).  The constructor takes the vector and the box pointers on the
stack and stores them at `[this+0x58]` and `[this+0x5C]`; the
`Box2<double>` layout the retail uses is the centre at +0x00, the two axes
at +0x10 and +0x20 and the two extents at +0x30 and +0x38.

What the retail's instruction sequence fixes about the orderings, and what
the rebuild's double branch therefore does now:

* the difference is the point minus the centre, taken on the x87
  (`fsub qword ptr [edx]` at `0x00A6CD7C` and `0x00A6CD81`);
* the two projections are formed on the x87 - the first is
  `(Axis0.y*dy) + (Axis0.x*dx)` (`faddp st(1)` at `0x00A6CD92`) and the
  second is `(dx*Axis1.x) + (dy*Axis1.y)` (`faddp st(3)` at `0x00A6CDDE`).
  Which operand is on the left does not change the value - a sum is
  commutative and the x87's rounding is symmetric - so what matters is only
  that every product and every sum is a 24-bit significand, which is what
  the plain double arithmetic of the source does not do;
* the delta is `P + extent` in the "less than -extent" branch (`fadd qword
  ptr [eax+0x30]` at `0x00A6CDA6`) and `P - extent` in the "greater than
  extent" branch (`fsub qword ptr [eax+0x30]` at `0x00A6CDC2`), the square
  is taken on the x87 (`fmul st(0),st(0)`) and added to the accumulator
  there (`faddp st(1)`), so the accumulator stays a 24-bit significand;
* the closest point is accumulated on the x87 and each component is stored
  back to memory between the two axes (`0x00A6CE3F` and `0x00A6CE49`);
* the swept bodies move the point and the box's centre on the x87: the
  product `fT*velocity` is formed first (`fmulp st(1)` at `0x00A6CB8F` for
  the point and `fmulp st(2)` at `0x00A6CBC0` for the centre) and the
  position is added to it there.

The rebuild's double branch was plain binary64; it now spells all of that
out in the `Wm3Detail::X87` helpers.  The float branch is untouched and the
float rows still pass.

The generators: the box's axes are an orthonormal pair, and in the aligned
family they are the x and the y axis with the centre at the origin, so the
projection of the point onto an axis is the point's own coordinate minus
zero - exactly - and a point placed at ±extent lands exactly on the
boundary between the clamped and the unclamped branch.  The magnitudes stay
inside `[1e-100,1e100]`, which is what keeps the emulation - a 24-bit
significand with an extended exponent - exact; what that leaves out of
reach is the x87's extended exponent range itself, i.e. the overflow and the
underflow of the products.  The swept rows take the time zero in a third of
the draws, with the velocities zero as well, so the moved pair is exactly
the pair below and the exact-boundary family is carried into the swept body.

The numbers: the four rows are 1000000/1000000 bit-identical on the default
seed `0x5EED2007` and 200000/200000 on the whole table, which is 109/109.
They are mutation-checked: flipping the sign of the delta in the "less than
-extent" branch makes all four fail (567, 524, 600 and 548 mismatches of
2000 draws).

## The `DistLine3Segment3<double>` rows are in

The static and the swept rows of `DistLine3Segment3<double>` are in the table
and agree with the retail bit for bit; the table is 113/113.

The addresses come out of the class vtable `0x00D722FC` (the RTTI slot in
front of it names `.?AV?$DistLine3Segment3@N@Wm3@@`, and the constructor at
`0x00A80950` writes the pointer):

| slot | address | body |
|---|---|---|
| +0x08 | `0x00A810B0` | the swept `Get(fT,v0,v1)` |
| +0x0C | `0x00A809C0` | the static `Get` |
| +0x14 | `0x00A811F0` | the swept `GetSquared(fT,v0,v1)` |
| +0x18 | `0x00A81330` | the static `GetSquared` |

The constructor takes the line and the segment pointers on the stack and
stores them at `[this+0x68]` and `[this+0x6C]`; the retail's layouts are
`Line3<double>` = origin +0x00, direction +0x18, and `Segment3<double>` =
origin +0x00, direction +0x18, extent +0x30.  The two swept bodies are
thiscall with three stack arguments - the double `fT` and the two velocity
pointers - and `RET 0x10`; the double comes back in ST(0).

The rebuild's double branch was plain binary64; the float branch was already
modelled in the retail's orderings, and the two now differ only in which
helper they call, because the retail's double body (`0x00A81330`) is the same
algorithm with the same groupings as its float body (`0x00A80DE0`):

* the difference is the line's origin minus the segment's origin, taken on
  the x87 (`fsub qword ptr [edx]` at `0x00A81338` and `0x00A8133D`);
* the two inline dots keep the retail's grouping `(y*y' + x*x') + z*z'`
  (`0x00A81352`/`0x00A8135A` for fA01, `0x00A81368`/`0x00A8136F` for fB0);
* fC is `z*z + (y*y + x*x)` (`0x00A81379`/`0x00A8137F`), the grouping the
  shared `Vector3::SquaredLength` gives for the float instantiation;
* the swept bodies move the two origins on the x87: the product
  `fT*velocity` is formed first and the position is added to it there.

The numbers: the four rows are 1000000/1000000 bit-identical on the default
seed `0x5EED2007` and 200000/200000 on the whole table, which is 113/113.

The rows are mutation-checked.  Changing fC's grouping - `(z*z + (y*y + x*x))`
into `((z*z + y*y) + x*x)` - makes all four fail (124, 104, 142 and 111
mismatches of 2000 draws); that is the z term's position in the sum, which
the x87's rounding does see.  Swapping the first two terms of the inline dot
changes nothing, as it must not: a sum of two products is commutative and
the x87's rounding is symmetric.  What matters is where the z term sits.

## `DistSegment3Segment3<double>`: the addresses, and what is left

Its addresses are confirmed - the class vtable `0x00D7227C` gives +0x08
`0x00A7EE60`, +0x0C `0x00A7E250`, +0x14 `0x00A7EFB0`, +0x18 `0x00A7F100`,
and the constructor is `0x00A7E1E0` - and the plumbing (the retail
wrappers, the forwards, the generators) is in place, but the double branch
does not reproduce the retail yet, so the rows are not in the table and
nothing is shipped for them.  Carrying the double instantiation in the
modelled x87 value, the way the float branch already is, takes the rows
from 100% wrong to 1% wrong.

**A correction to the earlier note in this file.**  The claim that the
retail's float body (`0x00A7E660`) and its double body (`0x00A7F100`)
disagree on 110 of 300 draws came from running the two bodies in the
simulator - and **the simulator cannot be used as the cross-check for that
body**: it returns a NaN for a trivial input (two identical segments along
the x axis, where the answer is zero).  Its verdict on that body is not
evidence, and the figure is withdrawn.  What the harness itself measures is
that the float formulation of the double body disagrees with the retail on
about **1%** of the draws.  Note that the same class of claim turned out to
hold for the line class, where the double body *is* the float body with
another type - so the question has to be settled per class, by the harness.

What the body's own instruction bytes do establish (the decoder in
`tools/x87sim` is the authority; capstone's branch targets in this body are
not always right):

* the four sums are spelled out with the retail's grouping and they are the
  same as the source's: `fA01 = -((d1y*d0y + d1x*d0x) + d1z*d0z)`,
  `fB0 = ((d0y*dy + dx*d0x) + d0z*dz)`,
  `fB1 = -((d1y*dy + dx*d1x) + d1z*dz)`,
  `fC = (dy*dy + dx*dx) + dz*dz` - none of them is the difference;
* `fDet = |1 - fA01*fA01|`, `fS0 = fA01*fB1 - fB0`, `fS1 = fA01*fB0 - fB1`
  (the reversed subtraction was tried and makes the FLOAT rows fail, so the
  source's order is the retail's);
* the interior region's formula is grouped as the source groups it.

So the remaining 1% is in one of the other regions' formulas - the side and
corner branches - and the next step is to decode those blocks (`0x00A7F230`
onwards) and compare their groupings with the source's.

### Notes on the tooling

* **The retail's branch idiom is `fnstsw ax; test ah, imm; jcc`, and the
  branch is on the X87 codes in AH.**  The TEST only turns them into an
  integer flag; it does not change the sense of the condition.  So
  `test ah, 1; jne` means "C0 is set" and `test ah, 0x41; jae` means the
  x87's "above or equal".  Reading them as integer branches flips them, and
  flipping one inverts a whole region of the body.
* **The simulator's decoder is reliable and is the way to read a retail
  body; its execution is not, for a body that uses the parity branch idiom
  together with the x87-code branches.**  Combining the simulator's decoder
  with capstone for the instruction boundaries reads a body reliably - and
  it is what settled the line class's groupings above.  Its *arithmetic
  model* (the rounding rules) is a different matter and is usable; what is
  not usable is its verdict on a body it cannot execute correctly.  Any
  earlier claim in this file that rests on a simulator run of such a body
  has to be re-checked - the 110-of-300 figure above was exactly that.
* **Capstone's branch targets are not always right.**  The `jne` at
  `0x00A6CDBC` is `75 10`, whose target is `0x00A6CDCE`, not the `0x00A6CE00`
  capstone printed - the two bytes before it are `F6 C4 41` (`test ah, 0x41`)
  and capstone's instruction stream had drifted.  The instruction bytes are
  the authority.
* **The parity flag is the x86 one**: PF is set when the number of set bits
  is *even*, so `test ah, 5; jp` is taken when C0 and C2 agree.  Reading
  `jp` as "odd parity" flips the branch and sends the reader into the wrong
  region.
* **The simulator cannot load a pointer from memory** - its `mov reg, [mem]`
  reads a `float32` - so a retail body that loads its two shape pointers has
  to have those two `mov`s replaced by NOPs and the registers pre-seeded.
  With that, the retail's static bodies (which have no prologue) run in the
  simulator directly.
* **The simulator does not implement `FLDZ` (`D9 EE`) or `FCOM`/`FCOMP` with a
  memory operand** (`DC /2`, `DC /3`), both of which the retail's bodies use;
  they can be added locally in a scratch script.

## The base class's own eight double bodies are in

The two `Distance` bases have their own vtables, and the four bodies below are
the base's own members - the ones the derived classes do not override:

| base | vtable | +0x04 | +0x10 | +0x1C | +0x20 |
|---|---|---|---|---|---|
| `Distance<double,Vector3d>` | `0x00D71CA4` | `0x00A39860` | `0x00A39AB0` | `0x00A39D00` | `0x00A39D60` |
| `Distance<double,Vector2d>` | `0x00D71C7C` | `0x00A39270` | `0x00A394C0` | `0x00A39710` | `0x00A39770` |

= the window `Get(fTMin,fTMax,&v0,&v1)` and `GetSquared(...)` at +0x04 / +0x10,
and `GetDerivative(fT,&v0,&v1)` and `GetDerivativeSquared(...)` at +0x1C /
+0x20.  The eight addresses come from the two base classes' own vtables, read
out of the image; the base constructors at `0x00A39FA0` (Vector3) and
`0x00A39F60` (Vector2) install them.  The four slots in between are
`0x00A82547`, the pure-virtual stub that **aborts** - a row that landed there
would be a crash and not a mismatch.  The two window bodies are thiscall with
four stack arguments - fTMin, fTMax, `&rkVelocity0`, `&rkVelocity1`, the two
doubles first - and `RET 0x18`; the two derivative bodies take three stack
arguments - the double fT and the two velocity pointers - and `RET 0x10`.
Every one of them returns the double in ST(0).  The bodies dispatch through
the object's vtable, so the rows run on a `DistVector3Segment3<double>` and a
`DistVector2Box2<double>`.

All eight rows compare a **value**, so they see the arithmetic: with the
rebuild's double branch still on binary64 they failed 14597, 21799, 54091,
54363, 5373, 29607, 29066 and 27157 times in 200,000 draws; with the
transcription they are 200000/200000 and 1000000/1000000 on each of the five
seeds.

What the retail's instruction sequence fixes (read off `0x00A39860` and its
siblings):

* the Newton iterate is `fT0 - (fF0/fDF0)`: the quotient is rounded to 24 bits
  **before** the subtraction (`fdivr st(2)`, `fsubr st(1)`);
* the bisection's midpoint is `0.5*(fT0 + fT1)`: the sum first, then the
  halving (`fadd qword ptr`, `fmul qword ptr [0.5]`);
* the derivative product is `fDFm*fDF0` (`fmul st(1)`);
* `GetDerivative` forms the two sample times on the x87 - `fld
  m_fDifferenceStep; fadd fT` and `fld fT; fsub m_fDifferenceStep`;
* `GetDerivativeSquared` doubles the distance with `fadd st(0), st(0)` and
  multiplies by the derivative with `fmulp st(1)`.

So `Foundation/Distance/Wm3Distance.cpp`'s double branch now spells its
arithmetic out in the `Wm3Detail::X87` helpers, through four small helpers
(`DistanceAdd/Sub/Mul/Div`) overloaded on the `Real` so that the **float**
branch is literally `a+b` and the double branch is the x87.

## The x87's comparison direction does not change with the encoding (a retraction)

An earlier round recorded that `FCOMPP ST(1)` (`DE D9`) compares ST(1) with
ST(0) while `FCOM ST(i)` (`D8 D0+i`) compares ST(0) with ST(i), and that
reading a `fcompp` as a `fcomp st(1)` inverts the branch.  **That is wrong
and is withdrawn.**  All three - `FCOM`, `FCOMP`, `FCOMPP` - compare **ST(0)
with the other operand** (`FCOMPP`'s fixed operand is ST(1)), and **C0 = 1
means ST(0) < the other operand**.  A 32-bit real-hardware probe settles it:
with `ST0 = 1.0` and `ST1 = 2.0`, `FCOM st(1)`, `FCOMP st(1)` and `FCOMPP`
all give C0 = 1; the Intel SDM's pseudocode is the same.  The evidence the
earlier note leaned on - the retail's own `Distance::Get`, `fldz; fcompp`
with ST(0)=0 and ST(1)=fDF0, continuing exactly when fDF0 < 0 - is consistent
with **this** direction, not with the reversed one, so it never supported the
retraction's claim.

**Two traps when reading these bodies.**  Capstone spells `D8 D0+i` - which
is `FCOM`, and does **not** pop - as `fcomp`; the bytes are the authority
(reg field 2 = FCOM, 3 = FCOMP), and tracing the stack as if it popped gives
a wrong trace.  And after `test ah,0x41` the `jp` is the x87's "above or
unordered" (PF is the *even* parity of the AND result, so the jump is taken
when C0 and C3 agree), while after `test ah,5` it is "C0 == C2".

## The `IntrBox2Box2` body is transcribed too, and the bool row is now a million clean

`IntrBox2Box2<double>::Test` (`0x00A47F00`) - the row that seed `0xA5A5A5A5`
found one draw in a million on - has its double branch transcribed now.  The
retail's own orderings, read off the body:

* the centre difference is `C1 - C0`;
* the radius sum of an axis is `(otherBox.Ext0*projection) +
  (thisBox.Extent) + (otherBox.Ext1*projection2)`;
* the two terms of a projection are formed **y-first** for the projections
  onto the centre difference - an addition is commutative so that does not
  change the bits, but the *grouping* does;
* the test is "the projection is greater than the radius sum", which is what
  the retail's `fcompp` decides - with its operands the other way round, per
  the section above.

The row is 1000000/1000000 on seed `0xA5A5A5A5` now, and the seven
`Intr*<double>::Test` rows together were run to **10,000,000 draws on each of
three seeds** (210M draws) without a mismatch.  That is the shape of evidence
a `bool` row needs: not one seed at a million.

**The six rows that are still open say so in their own descriptions.**  They
previously claimed an `X87` modelling that did not exist; that is corrected.
`IntrBox2Circle2` (`0x00A4EE50`) is not one of them - it delegates to
`DistVector2Box2<double>::Get()` and inherits the modelling - and
`IntrSegment3Capsule3` (`0x00A6BF80`) is blocked on
`DistSegment3Segment3<double>`.  `IntrBox3Sphere3` (`0x00A425C0`),
`IntrLine3Box3` (`0x00A50090`), `IntrSegment3Box3` (`0x00A46720`) and
`IntrSegment3Sphere3` (`0x00A475E0`) still have a binary64 double branch.  The
method is the `IntrBox2Box2` one: read the body, spell the products, the sums
**and the grouping** out in the `Wm3Detail::X87` helpers, check the `fcompp`
polarity, and keep the float branch exactly as it is.  Note that
`IntrSegment3Sphere3` and `IntrBox3Sphere3` also need their *comparison*
corners checked (e.g. the retail's `fDiscr` test is `fDiscr <= 0` -> false,
where the source says `fDiscr < 0`).

## The four open `Intr*<double>::Test` bodies are transcribed - and the rows are still weak witnesses

`IntrBox3Sphere3` (`0x00A425C0`), `IntrLine3Box3` (`0x00A50090`),
`IntrSegment3Box3` (`0x00A46720`) and `IntrSegment3Sphere3` (`0x00A475E0`)
now have their double branches spelled out in the `Wm3Detail::X87` helpers.
Each is the class vtable's **+0x08** slot, thiscall with ECX = `this` and no
stack arguments, answering a `bool` in EAX; the retail's classes hold their
operands by pointer (at `+0x18` / `+0x1C`) while the rebuild's hold them by
value, and the harness builds each side to its own layout.  The float
branches are untouched.

What the retail's instruction sequences fix:

* every dot is `(y*y' + x*x') + z*z'` - the **y term first**, unlike the
  shared `Vector3::Dot`, which starts with x.  The bits are the same
  (addition is commutative) but the *grouping* is not, and the 24-bit
  rounding rides on the grouping;
* the cross product is the standard one, built in a rotated order (second
  component first); each component is the same subtraction of the same two
  products;
* the centre differences are sphere − box, line origin − box centre, segment
  origin − box centre;
* `IntrBox3Sphere3`'s region tree is the source's, region by region, and each
  region's test is the source's - face regions `fDz <= Radius`, edge and
  corner regions `sum of squares <= Radius*Radius`, all inclusive, the
  corner's three-term sum as `(fDx*fDx + fDy*fDy) + fDz*fDz`;
* `IntrLine3Box3` / `IntrSegment3Box3` use the source's tests and the
  source's two-term sums (`Extent[1]*fAWdU[2] + Extent[2]*fAWdU[1]`, ...),
  with the retail's operands the other way round inside a sum (same bits);
* `IntrSegment3Sphere3` tests `fA1*fA1 - fA0` with the x87's "at or above
  zero", forms `fTmp1 = (2*fA1)*Extent` (doubling first), and its **last**
  test compares `|fA1|` with the extent with "at or below" where the source
  is strictly below - a corner that is **unreachable** (`|fA1| == Extent`
  forces `fQM*fQP <= 0`, so the earlier test has already answered true), so
  it makes no observable difference; the rebuild spells the retail's "at or
  below" and says why in the comment.

**These four rows are weak witnesses, and the mutation check says so.**  With
the double branch put back on the plain binary64 source they still pass
1,000,000/1,000,000 on all five seeds and 10,000,000/10,000,000 on seed
`0x5EED2007`.  A `bool` flips only when the two sides of a comparison are
within an ulp of each other, and with these generators (operands of the order
of a thousand) that happens about **once in 10^10** draws.  The transcription
is therefore established by **reading the bodies** (a symbolic x87 tracer
over the retail bytes), not by the rows.  To turn these four into real
witnesses a generator has to put the two sides *exactly* together - e.g. a
box axis that is exact in binary64 but not in 24 bits (`1 + 2^-30`) - which
is the same kind of construction the `IntrBox2Box2` row needed.  That is
work for the next round, and it is the reason the four rows are recorded as
**transcribed but not yet witnessing**.

## The bool rows of the double family are witnesses now

A `bool` row only witnesses a comparison when the two sides of it land within
an ulp of each other.  With the random geometry the `Intr*<double>`
generators drew (operands of the order of a thousand) that happens about once
in 10^10 draws, so the four `Intr*<double>::Test` rows and
`IntrBox2Circle2<double>::Test` could not witness the precision of the
transcription at all: with the double branch put back on plain binary64 they
still passed 1000000/1000000 on five seeds and 10000000/10000000 on one.

**They build the collision exactly now.**  Half of each generator's draws are
a geometry in which the two sides' results differ by a whole ulp of the
deciding threshold.  The three numbers it is built from are

| value | bits | what it is for |
|---|---|---|
| `1 + 2^-30` | `0x3FF0000000400000` | the coordinate that carries the collision - exact in binary64, not a 24-bit significand, so the x87 rounds it to `1.0` |
| `1 - 2^-32` | `0x3FEFFFFFFFE00000` | the threshold, which the x87 loads without rounding |
| `3 * 2^-32` | `0x3E08000000000000` | a radius between the two sides' results |

so the retail's distance to the face is `2^-32` and the binary64 one is
`5 * 2^-32`, and the retail answers true where binary64 answers false.  The
other two coordinates of the colliding shape stay random inside the shape, so
the draws are not all the same case.

**The numbers are built from their bit patterns, and that is not fussiness.**
The generator runs inside the harness's loop, where the x87 control word is
still the engine's `_PC_24` from the previous side - `SetFpEnv()` sets it and
leaves it - so `1.0 + std::ldexp(1.0,-30)` is evaluated **at 24 bits** and
comes out as `1.0`, exactly the value the construction must not have.  The
first collision family was degenerate for that reason and the rows still
passed when mutated.  **A generator's own arithmetic is subject to the
ambient floating-point environment, the same way the library's is**; anything
that needs a specific bit pattern must be built from bits (a `memcpy` of a
`uint64_t`), or at least checked under the harness's control word.

The mutation check, with the `Wm3Detail::X87*` helpers in `Wm3Vector3.h` put
back on plain binary64 (the float branches use the separate `Mul24`/`Add24`
helpers and are untouched):

| row | mutation (binary64) | restored (x87) |
|---|---|---|
| `IntrBox3Sphere3<double>::Test` | **100317 / 200000** | 200000/200000, 1e6/1e6 on five seeds |
| `IntrLine3Box3<double>::Test` | **100001 / 200000** | same |
| `IntrSegment3Box3<double>::Test` | **99619 / 200000** | same |
| `IntrSegment3Sphere3<double>::Test` | **99956 / 200000** | same |
| `IntrBox2Circle2<double>::Test` | **99960 / 200000** | same |

Of the seven `bool` rows of the double family, **five are real witnesses now**.
`IntrBox2Box2<double>::Test` was already one - it caught a draw in a million -
and `IntrSegment3Capsule3<double>::Test` **cannot be one yet**: it delegates to
`DistSegment3Segment3<double>::Get()`, whose double branch is still not in the
tree, so making it discriminating would make it fail.

What the collision families rest on, per class (all read off the retail with
the symbolic x87 tracer):

* `IntrBox3Sphere3` - the inline dot is `(ay*dY + dx*ax) + az*dZ`; the region
  dispatch compares `E0` with `|dot0|` and so on, and the face cases compare
  `(|dot_k| - E_k) < Radius`.  The collision puts `|dot_k| = 1.0` (x87)
  against `1+2^-30` (binary64) with `E_k = 1-2^-32` and `R = 3*2^-32`, so the
  retail's `2^-32 < R` is true and the binary64 `5*2^-32 < R` is false.
* `IntrLine3Box3` - the cross product `W = dir x diff` and the three tests
  `|dot(Axis_i, W)| > E_j*|dot(Axis_j,dir)| + E_k*|dot(Axis_k,dir)|`; the
  collision makes `W`'s component `1.0` against `1+2^-30` with a right-hand
  side of exactly `1.0`.
* `IntrSegment3Box3` - the first three tests compare `|dot(Axis_i, diff)|`
  with `E_i + Extent*|dot(Axis_i,dir)|`; the collision makes
  `|dot(Axis_j, diff)|` `1.0` against `1+2^-30` with a right-hand side of
  exactly `1.0`.
* `IntrSegment3Sphere3` - the discriminant `fA1*fA1 - fA0` with
  `fA0 = (dY*dY + dX*dX) + dZ*dZ - R*R`; the collision makes the x87's
  `fA0 = 0` (it rounds `(1+2^-30)^2` to `1.0`) against the binary64 `2^-29`,
  so the retail's discriminant is `0` and it answers true, while binary64's is
  negative and answers false.
* `IntrBox2Circle2` - `DistVector2Box2<double>::GetSquared` takes the delta on
  the x87 and squares it there; the collision makes the distance to the face
  `2^-32` against `5*2^-32` with a radius of `3*2^-32`.

**One more retail fact the collision work settled:** the face test is
**strict** - `|dot_k| - E_k < Radius`, from `test ah,0x41; jp` being "above or
unordered" - not `<=` as the source spells it.  The two differ exactly on a
tie, and the collision family is built so that the tie never lands on the
radius; the rows confirm the strict form.

The Foundation is unchanged by this round - the commit is the harness's
generators only - so the engine binary does not move and the lockstep baseline
stands.

## The distance class's double branch, and the software model's double rounding

`DistSegment3Segment3<double>` is transcribed (`Foundation/Distance/
Wm3DistSegment3Segment3.cpp`) and its four value slots are in the table:

| row | retail | slot | in words | out |
|---|---|---|---|---|
| `DistSegment3Segment3<double>::GetSquared` | `0x00A7F100` | class vtable `0x00D7227C` +0x18 | 28 (`seg0[7] seg1[7]`) | 2 |
| `DistSegment3Segment3<double>::Get` | `0x00A7E250` | +0x0C (dispatches +0x18, `jmp 0x00A91000`) | 28 | 2 |
| `DistSegment3Segment3<double>::GetSquared(swept)` | `0x00A7EFB0` | +0x14, 3 stack args, `RET 0x10` | 42 | 2 |
| `DistSegment3Segment3<double>::Get(swept)` | `0x00A7EE60` | +0x08 | 42 | 2 |

**This corrects two earlier readings of this class, and the correction is the
interesting part.**  An earlier round recorded that the retail's float body
(`0x00A7E660`) and its double body (`0x00A7F100`) compute the same geometry
with a *different* formulation, needing a region-by-region transcription of
473 FPU instructions, and that the float formulation of the double body was
"about 1% wrong" (20 draws in 2000, spread over every region).  Both halves
were wrong about the cause:

* run under the x87 simulator, the retail's double body and its **float** body
  agree **bit for bit on every input whose segments are float-representable**
  (300 random draws, no disagreement) - so the double branch is the **same
  source** with the x87's rounding.  There is no separate grouping to chase,
  which is why the twelve-variant search over the sums' associations had left
  the count at exactly 20 every time: it was searching a degree of freedom
  that did not exist.
* the "1%" was the **software model's own double rounding**.  The
  `Wm3Detail::X87` value type keeps a `double` and rounds each operation once
  - but a `double` product of two 53-bit operands needs 106 bits, so rounding
  the product to binary64 first and to the 24-bit significand second rounds
  **twice**, and that is a different number.

So the fix is a new value type: **`Wm3Detail::X87Asm`** (`Foundation/Math/
Wm3Vector3.h`), whose operators run the operation on the **hardware's x87**
(`X87Add/Sub/Mul/Div/Neg`) with an explicit conversion back.  The double
instantiation of this class now carries its intermediates in it, and

* `Get()` takes the CRT's square root at `0x00A91000`, which keeps the ambient
  precision control - under the engine's `_PC_24` that is a **24-bit square
  root**, not the correctly rounded binary64 one, so it goes through
  `X87Sqrt`;
* the two swept forms move the shapes on the x87 (`fT*velocity` in a register,
  then added to the position);
* the parallel branch's extent sum is an x87 addition for the double.

**The float branch is textually and behaviourally unchanged** - same `F` alias,
same `Get()`, same swept code, same float extent sum.

**The four rows are real witnesses** (they return values): with the double
branch reverted to plain binary64 they fail **199845**, **199839**, **199839**
and **199837** times in 200,000 draws; restored, they pass 200000/200000 and
1,000,000/1,000,000 on all five seeds.

**And the blocker is gone**: `IntrSegment3Capsule3<double>::Test`
(`0x00A6BF80`) delegates to this class's `Get()`, so it could not be a witness
before.  Its generator now builds an exact collision in half its draws: both
segments degenerate to points one `1 + 5*2^-26` apart along y, with a radius of
`1 + 6*2^-26`; the x87 rounds the difference of the origins to `1 + 2^-23`, so
the distance is **above** the radius and the test is false, while the binary64
chain keeps the difference exact, its distance is the gap itself
`1 + 5*2^-26`, **below** the radius and the test is true.  With the
transcription reverted it fails **100088/200000**.  Both colliding values are
built from their bit patterns (`0x3FF0000014000000`,
`0x3FF0000018000000`) because the generator runs under `_PC_24`.

**One more tool fix came out of this:** the x87 simulator's `DC` memory table
had no `/2` or `/3` - `FCOM` and `FCOMP` on a 64-bit double - so it raised
`Stop('unhandled x87 DC 52')` on the retail's double distance body.  Both are
in `tools/x87sim/x87sim/decode.py` now.

## The `Intr*` double slots that are left, and why some should stay out

The seven double classes' slot maps, each read out of that class's own vtable:

| class | vtable | +0x04 | +0x08 `Test` | +0x0C `Find` (dynamic) | +0x10 `Find` |
|---|---|---|---|---|---|
| `IntrBox2Box2` | `0x00D71E54` | base `0x00A455A0` | `0x00A47F00` | base `0x00A455C0` | base `0x00A455D0` |
| `IntrBox2Circle2` | `0x00D71EDC` | base `0x00A455A0` | `0x00A4EE50` | **`0x00A4F960`** | base `0x00A455D0` |
| `IntrBox3Sphere3` | `0x00D71D14` | base `0x00A45630` | `0x00A425C0` | **`0x00A44190`** | base `0x00A45660` |
| `IntrLine3Box3` | `0x00D71F0C` | base `0x00A45630` | `0x00A50090` | base `0x00A45650` | **`0x00A50940`** |
| `IntrSegment3Box3` | `0x00D71DF4` | base `0x00A45630` | `0x00A46720` | base `0x00A45650` | **`0x00A46330`** |
| `IntrSegment3Capsule3` | `0x00D720AC` | base `0x00A45630` | `0x00A6BF80` | base `0x00A45650` | **`0x00A6C3C0`** |
| `IntrSegment3Sphere3` | `0x00D71E24` | `0x00A474C0` | `0x00A475E0` | **`0x00A47690`** | **`0x00A479B0`** |

* **The four shared `Intersector` base bodies are stubs with no arithmetic**:
  `0x00A455A0` = `xor al,al; ret 0x10`, `0x00A455D0` = `xor al,al; ret`,
  `0x00A45630` = `xor al,al; ret 0x10`, `0x00A45650` = `xor al,al; ret 0x10`.
  They always return false, so no row over them can witness anything (and the
  rebuild's counterparts `assert(false)` before returning false, so such a row
  would also need `NDEBUG`).  **They are deliberately left out**, and the
  earlier work list's "the four shared base bodies, each to be done once" is
  withdrawn: there is nothing there to do.
* The `Find` slots with a derived override are the seven bold entries above.
  The rebuild has the methods, but **their double bodies are not transcribed**
  - only the `Test` functions have a double branch - so a row over them would
  fail, not pass.  Transcribing the five large x87 `Find` bodies (and their
  retail thunks) is a separate, larger piece of work.
* The "region / contact-point helpers at +0x10 onwards" of `IntrBox3Sphere3`
  and `IntrBox2Circle2` are, in the double instantiation, the base's `Find()`
  stub; the helpers themselves (`GetVertexIntersection`,
  `GetEdgeIntersection`, `FindFaceRegionIntersection`, ...) are **private,
  non-virtual** bodies with their own addresses - the float ones are already
  rows, the double ones are separate transcriptions.

## Three of the six `Intr*<double>::Find` bodies are transcribed, and their rows are in

The `Find` slots of the seven `Intr*<double>` classes were the largest piece of
the double family still open (see "The `Intr*` double slots that are left" above):
the retail's bodies are x87 code, the engine runs under `_PC_24`, and the rebuild
had no double branch for them at all, so a row over them could only fail.  Three
of the six bodies are transcribed now, and the rows are in the table:

| row | retail body | what the body is |
|---|---|---|
| `IntrLine3Box3<double>::Find` | `0x00A50940` | a thin wrapper around the shared double `DoClipping` (`0x00A505F0`) |
| `IntrSegment3Box3<double>::Find` | `0x00A46330` | the same, with the parameter range `[-Extent, +Extent]` |
| `IntrSegment3Sphere3<double>::Find` | `0x00A479B0` | the *static* `Find` (the dynamic one is `0x00A47690`) |

All three rows compare values, not a bool: the hit flag, the quantity, the
intersection type and the two points, and for the sphere the two segment
parameters as well.  The retail's own members are read out of its object storage,
and the double instantiation's layout is not the float one's:

| class | the type | the quantity | the points | the parameters |
|---|---|---|---|---|
| `IntrLine3Box3<double>` | `[this+0x10]` | `[this+0x20]` | `[this+0x28]` | - |
| `IntrSegment3Box3<double>` | `[this+0x10]` | `[this+0x24]` | `[this+0x28]` | - |
| `IntrSegment3Sphere3<double>` | `[this+0x10]` | `[this+0x28]` | `[this+0x30]`, `[this+0x48]` | `[this+0x60]`, `[this+0x68]` |

The first two offsets come from the two `Find` bodies themselves (the `lea` of
the three out-parameters) and from the retail's own accessors - `0x00A46370` is
the double `GetQuantity` (`mov eax,[ecx+0x24]`) and `0x00A46380` the double
`GetPoint` (`lea eax,[ecx+eax*8+0x28]`).

### What the two shared bodies say

`DoClipping` (0x00A505F0) and `Clip` (0x00A4FD90) are read off the image, not
assumed from the source:

* every dot is formed as `(y*y' + x*x') + z*z'`, the **y term first** - not the
  shared `Vector3::Dot`'s z-first order:
  `Bo = (A[i][1]*d1 + A[i][0]*d0) + A[i][2]*d2`,
  `Bd = (A[i][1]*g1 + g0*A[i][0]) + A[i][2]*g2`;
* the six clipping calls are `Clip(+Bd0, (-Bo0)-E0)`, `Clip(-Bd0, Bo0-E0)`,
  `Clip(+Bd1, (-Bo1)-E1)`, `Clip(-Bd1, Bo1-E1)`, `Clip(+Bd2, (-Bo2)-E2)`,
  `Clip(-Bd2, Bo2-E2)`, and each of them short-circuits on the retail's
  `test al,al`;
* the two numerators are **not** the same number: the positive call takes the
  negated dot minus the extent (the retail negates on the x87 and then
  subtracts), the negative one takes the dot minus the extent;
* the intersection point is `(d*t) + o` - the product first, the origin added
  afterwards - and the two points are written x, y, z;
* `Clip`'s positive branch compares the numerator against `rfT1*denom` and then
  updates `rfT0`; the negative branch compares it against `rfT0*denom` and then
  updates `rfT1`.  The two are not interchangeable;
* the products and the quotient are the x87's, and the update happens only when
  the deciding comparison was an ordered one;
* a zero denominator answers `numer <= 0`, and a NaN denominator takes the
  negative branch, whose comparisons are all unordered, so it answers true
  without touching either t value.

### What the sphere's body says

`IntrSegment3Sphere3<double>::Find` (0x00A479B0):

* `fA0` is `((dy*dy + dx*dx) + dz*dz) - R*R` - the squared distance with the y
  term first, and the radius squared subtracted at the end;
* `fA1` is `((dx*D0) + (dy*D1)) + (dz*D2)`, the products with the difference
  first;
* the discriminant is `fA1*fA1 - fA0`, and a negative one is the empty answer;
* `fTmp0` is `fA0 + E*E` and `fTmp1` is `(fA1+fA1)*E` - the retail adds the two
  `fA1`'s on the x87 rather than multiplying by two, which is the same number;
* the square root is the CRT's (0x00A91000), which under `_PC_24` is a **24-bit**
  square root and not the correctly rounded binary64 one, so the double branch
  takes it with `Wm3Detail::X87Sqrt` and not with `std::sqrt`;
* the parameter of the first point is `-fA1 - root` and the second one is
  `root - fA1`, and the negative sign is a `fchs` on the x87 - which is **not**
  the same number as a subtraction from zero when `fA1` is zero.  The first
  attempt wrote it as a subtraction from zero and the row failed 100034/200000
  with the retail's `-0.0` against the rebuild's `+0.0`;
* the point is `Origin + (t*Direction)`, the product first - the retail's
  `0x00A3AC80` and `0x00A3AB20` do exactly that.

### The mutation check

Both directions, on the default seed with 200,000 draws.  "Reverted" is the
double branch put back on its plain binary64 spelling, "restored" is the
transcription:

| row | reverted | restored |
|---|---|---|
| `IntrLine3Box3<double>::Find` | 101721/200000 mismatches | 0/200000 |
| `IntrSegment3Box3<double>::Find` | 50594/200000 mismatches | 0/200000 |
| `IntrSegment3Sphere3<double>::Find` | 113107/200000 mismatches | 0/200000 |

### The three bodies that are still open

> **Not work any more.**  These are `double` bodies, and the reachability audit
> put every double instantiation in `linked-but-unreachable`: no `src/` line
> passes a double argument to a Wild Magic template, so transcribing them
> cannot move the engine.  The findings below are kept because they are the
> only record of what the retail's double bodies look like and what they cost -
> if a double path ever becomes reachable, the round starts here instead of at
> the disassembler.  See `reachable.md`'s `不适用` section.

`IntrBox2Circle2` (0x00A4F960), `IntrBox3Sphere3` (0x00A44190) and
`IntrSegment3Capsule3` (0x00A6C3C0) are **not** transcribed, and they are not a
mechanical x87-ification of the rebuild's source either.  The reason is the one
the coverage map already names: the retail's double bodies are not the float
bodies the rebuild's source follows.  The clearest example is the double
`IntrLine3Capsule3::Find` (0x00A80080), which the capsule's `Find` calls: it
forms its dots as `(y + z) + x`, where the source's spelling is `(y + x) + z`.

What each one needs, so the next round does not have to find out again:

* `IntrBox2Circle2` (0x00A4F960): the dynamic `Find` - the difference, the
  velocity and the four dots (`fCx`, `fCy`, `fVx`, `fVy`, each grouped as
  `(y*y' + x*x')`), the region dispatch, and the two region bodies
  `0x00A4EEF0` (`TestVertexRegion`) and `0x00A4F2C0` (`TestEdgeRegion`) with
  their own `sqrt` and `InvSqrt`;
* `IntrBox3Sphere3` (0x00A44190): the face/edge/vertex dispatch and the five
  helpers it calls (`0x00A3AB80`, `0x00A42710`, `0x00A42790`, `0x00A427F0`,
  `0x00A42BC0`);
* `IntrSegment3Capsule3` (0x00A6C3C0): the outer body, which calls the double
  `IntrLine3Capsule3::Find` (0x00A80080), which in turn calls the double
  `Vector3<double>::GenerateOrthonormalBasis` (0x00A3D150) - the orthonormal
  basis is where the degenerate branches live, and the rebuild's own
  `GenerateOrthonormalBasis` is not the retail's double body either.

## `Vector3::GenerateOrthonormalBasis`, and two retractions

`Vector3<Real>::GenerateOrthonormalBasis` is a **cdecl static** member with
four stack arguments - `&rkU`, `&rkV`, `&rkW` and the `bUnitLengthW` flag - and
its body ends in a plain `RET`, so the caller pops them.  The order is settled
by the retail's own call sites: `0x00A800B4` (the double
`IntrLine3Capsule3::Find`) pushes the flag first and the three pointers after
it, and pops all four with `ADD ESP,0x10`; `0x00A7F94E` is the float one.

| body | address | answer |
|---|---|---|
| `Vector3<double>::GenerateOrthonormalBasis` | `0x00A3D150` | **value** (U and V) |
| `Vector3<float>::GenerateOrthonormalBasis` | `0x00A3CF80` | **value** |

**Two retractions.**  An earlier round recorded that the retail's double
`GenerateOrthonormalBasis` is the "WM3.4 four-branch algorithm" while the
rebuild has the "WM4 two-branch" one, and that the double
`IntrLine3Capsule3::Find` groups its dots `(y+z)+x` where the source spells
`(y+x)+z`.  Neither survives the machine code:

* the WM3.4 copy (`.tmp/wm3/Source/Math/Wm3Vector3.inl:604`, a read-only 2004
  file) is **the two-branch algorithm** - branch on `|W.x| >= |W.y|`, inverse
  length `1/sqrt(z*z + x*x)`, `V = rkW.Cross(rkU)` - the rebuild is the same,
  and both retail bodies are the same: a `fabs`/`fcompp`/`fnstsw`/`test
  ah,0x41`/`jp` decision, one of two sums, the CRT square root at `0x00A91000`
  and `fld1; fdivrp st(1)` for the inverse length;
* the double `IntrLine3Capsule3::Find` forms all three dots as
  **(y term + x term) + z term** (two `faddp`: the first joins the `dy` and
  `dx` products, the second adds the `dz` product) - the same grouping the
  source uses.

**What the rebuild was missing is the rounding, not the algorithm.**  The
retail's bodies are x87 code and the engine runs under `_PC_24`, so every
product, every sum and the square root carry a 24-bit significand with the
extended exponent; the rebuild's double instantiation was a plain binary64
expression.  `Foundation/Math/Wm3Vector3.inl` now spells the double branch in
x87:

* the sum is `z*z + x*x` (or `z*z + y*y`) with **the two products rounded
  before the sum** - an addition is commutative, so it is the order of the
  *products* that matters;
* the inverse length is `1/sqrt(sum)` **with the square root rounded first**,
  not a binary64 `1.0/sqrt(sum)`, which keeps 53 bits across both steps;
* `U.x` is the **negated product** (`fchs` after the multiply), not a
  subtraction from zero, which would give `+0.0` where the retail gives
  `-0.0`;
* `U.z` is `invlen*x` and `V.x` is that **stored** `U.z` times `y`, so the
  product is `(invlen*x)*y` and is not re-associated.

The float branch is the `else` half of the `if constexpr`, text unchanged.

Mutation counts, both directions:

| row | mutation | count |
|---|---|---|
| `Vector3<double>::GenerateOrthonormalBasis` | the double branch back on the binary64 spelling | **198173 / 200000** |
| | restored | 0 / 200000 |
| `Vector3<float>::GenerateOrthonormalBasis` | the branch's `>=` turned into `>` | **16764 / 200000** |
| | restored | 0 / 200000 |

**What the float row does not cover.**  The rebuild's *float* path computes the
sum in a binary32 while the retail keeps it on the x87 with the extended
exponent; the two part company once the sum leaves the binary32 normal range
(`|W|` beyond about `1e19`).  Widening the float generator to the binary64
range makes the row fail **1783/200000**, which is how it was measured.  The
engine's callers all pass unit-length directions, so the difference is not
reachable in the game, and the float path is left as it is.

**Two things this round did not do, now recorded as work:** the double
`Vector3::Normalize` is still binary64, and - more than a rounding question -
**the retail's `Normalize` returns `1/length` while the source returns
`length`**; a row over it has to settle which the rebuild should return.  That
is what the `bUnitLengthW == false` path of the basis function needs; the two
rows above use `bUnitLengthW = 1`, as the retail's own call site does.

## The double instantiation, part three: `Vector3::Normalize` returns the length

The note that "the retail's `Normalize` returns `1/length` while the source
returns `length`" is **wrong**, and it is retracted here.  The WM3.4 copy
(`.tmp/wm3/Source/Math/Wm3Vector3.inl:299`) returns the length, the rebuild
returns the length, and so does the retail.

**Why the note was wrong - and it is a stack-reading mistake, not a reading
of the source.**  The float instantiation (`0x00452AF0`) ends with `FLD dword
ptr [0x00d71bfc]` (the 1e-6 threshold), `FXCH ST(1)`, `FCOMI ST(1)` and `FSTP
ST(1)`.  `FSTP ST(i)` writes before it pops, so the value `FCOMI` compared -
the length - is what survives in `ST(0)` at the `RET`; the degenerate branch
(length <= the threshold) pops the length and leaves `0.0f`.  The double
instantiation (`0x00A3D0A0`, the WM library's own body, called from the double
`GenerateOrthonormalBasis` at `0x00A3D15E`) computes `1.0/length` with `FLD1;
FDIV ST(1)`, and the **D8 family writes `ST(0)` and does not pop** - so the
reciprocal lands in `ST(0)` and the length stays in `ST(1)`; the three
multiplies consume the reciprocal's copies and the epilogue leaves the
**length** in `ST(0)`.  The degenerate branch leaves `0.0`.

**The retail's own call site settles it independently.**  At `0x0050B829` the
engine calls `0x00452AF0`, pushes `0.0` and compares the returned value
against it (`fcompi`, `jbe`) - the value is read as the **length**, and the
rebuild's `Entity.cpp:7850` spells the same test as
`if (!(Wm3::Vector3f::Normalize(&surfaceNormal) > 0.0f))`.  (That also refutes
an older note that the retail's body "returns void": it returns in `ST(0)`.)

**What the retail does differ in is the rounding.**  The double body is x87
code under `_PC_24`: the three products `x*x`, `y*y`, `z*z` are each rounded on
their own, the sum is `(y*y + x*x) + z*z`, the square root is the CRT's
`0x00A91000` (24-bit under `_PC_24`), the reciprocal is an x87 division, and
the three components are multiplied by it.  The rebuild's double branch was
plain binary64 - **that** was the real defect, and it is an x87 transcription
now (`Foundation/Math/Wm3Vector3.inl`).  The float branch is untouched.

Two rows witness it:

| row | retail | answer | mutation (reverted) |
|---|---|---|---|
| `Vector3<double>::Normalize` | `0x00A3D0A0` | **value** (`v[3] len` = 8 words) | double branch back on binary64: **185934 / 200000** |
| `Vec3::Normalize(instance)` | `0x00452AF0` | **value** now (`v[3] len` = 4 words; it used to compare the vector only) | the float branch returning `1/fLength`: **136482 / 200000** (retail `100.637` against rebuild `0.00993672`) |

Restored, both are 200000/200000 and 1,000,000/1,000,000.  The table is 138
rows, 138/138 at 1,000,000 on all five seeds.

## `IntrSegment3Capsule3<double>::Find`, and the capsule body behind it

`IntrSegment3Capsule3<double>::Find` (`0x00A6C3C0`, the class vtable
`0x00D720AC`'s `+0x0C` slot) is **not** a thin wrapper: it calls the *static*
`IntrLine3Capsule3<double>::Find` (`0x00A80080`), filters the roots by the
**query segment's** extent and computes the points on the x87.  Both bodies are
transcribed now, and the row is in the table.

**The ABI.**  thiscall, `ECX = this`, no stack arguments, the hit flag in
**AL** - the retail writes only AL, so the thunk masks with `movzx eax,al`.
The roots go into `[this+0x58]` (`m_afParameter`); for each root, `|t|` is
compared with the query segment's extent (`[this+0x18]->+0x30`) using "at or
below" to decide whether it is kept, and the point is `(t*direction) + origin`,
the product first.  The outputs are the type at `[this+0x10]` (2 = segment,
1 = point, 0 = empty), the quantity at `[this+0x20]`, and the two points at
`[this+0x28]` and `[this+0x40]`.

**The static capsule body's x87 orderings**, read out of `0x00A80080` with the
repo's own x87 decoder and a symbolic tracer (not by path enumeration - the
body has hundreds of symbolic paths and enumerating them does not converge):

* the capsule-frame dots are `(y*dy + x*dx) + z*dz`, and the axis coordinate
  is `(dz*wz + (dx*wx + dy*wy))`;
* the parallel branch is taken when `|W.dir| >= 1 - ZERO_TOLERANCE`, with the
  radial term `(r*r - px*px) - py*py`;
* `fA0 = (py*py + px*px) - r*r`, `fA1 = px*dx + py*dy`,
  `fA2 = dx*dx + dy*dy`, `discr = fA1*fA1 - fA2*fA0`;
* the cylinder roots are `((-fA1) - root) * (1/fA2)` and
  `(1/fA2) * (root - fA1)`, the tangent root `-(fA1/fA2)`; **the cylinder
  branch has no `|discr| <= tol` test of its own**;
* the bottom hemisphere adds `(pz + e)` to `fA1` and `(pz + e)^2` to `fA0` and
  accepts at `z <= -e`; the top hemisphere subtracts `(e + e)*dz` from `fA1`
  and `(e*4.0)*pz` from `fA0` and accepts at `z >= e`; the two roots are
  sorted ascending.

**The row** takes `segment[7] capsule[8]` (30 words) and compares
`hit type quantity point0[3] point1[3]` (15 words).  The two *parameters* are
deliberately **not** compared: the retail's array holds every root the static
body found, while the rebuild's holds only the ones the extent filter kept, so
the parameter arrays legitimately differ in length and content - the points,
which are what a caller uses, do not.

**Mutation, both directions:** reverting the two double branches to the plain
binary64 source gives **111743 / 200000** mismatches; restoring gives 0.  The
table is 139 rows and passes 1,000,000 on all five seeds.

## `IntrSegment3Sphere3<double>::Find(swept)` - the dynamic Find

The *dynamic* `Find(fTMax, v0, v1)` (`0x00A47690`, the class vtable
`0x00D71E24`'s `+0x0C` slot) is transcribed as well now; the *static* one
(`0x00A479B0`) was already a row.  The rebuild's Foundation diff for this is
purely additive - the `if constexpr` double branch is new and the float branch
is byte-identical.

The body is a thin wrapper, but what it wraps is not: it builds a swept
capsule and asks the capsule's own first parameter.  Read off the machine code:

* an early hit answers contact time `0.0` and type `IT_OTHER` (8);
* otherwise the direction is `v1 - v0` and the origin is
  `C + 0.5*(v1 - v0)` - **the half is multiplied first** - the direction is
  normalised in place and the extent is `0.5*length`;
* the contact time is the capsule's first parameter, and the hit test is the
  x87's "at or above", so the time must be **strictly below** `fTMax` - the
  source's `>` would let equality through;
* `kMOrigin = (v0*t) + Origin` and `kMCenter = (v1*t) + Center`;
* the two projections add the **y term first**:
  `(D.y*MO.y + D.x*MO.x) + D.z*MO.z` and `(Mcent.y*D.y + Mcent.x*D.x) + D.z*Mcent.z`;
* the point is `MO - E*D` at the negative end, `MO + E*D` at the positive end,
  and `MO + ((fCenter - fNegEnd)/(fPosEnd - fNegEnd))*D` in the interior.

**The row** takes `segment[7] sphere[4] tmax vel0[3] vel1[3]` (36 words) and
compares `hit type time point0[3]` (10 words).  **Mutation, both directions:**
the double branch reverted to plain binary64 gives **12065 / 200000**
mismatches; restored, 0.

**Branch coverage, measured with temporary instrumentation that was then
removed:** over 200,000 draws the row hits 66,900 times - 54,835 early hits and
12,065 swept (3,990 at the negative end, 2,923 at the positive end, 5,152 in
the interior) - and **all 12,065 swept hits differ under the mutation**, so the
row witnesses the arithmetic and not merely the branch.  That measurement is
worth repeating for any row whose body has an early-out: a row that only ever
takes the early-out witnesses nothing about the rest.

## The Eigen rows: five of them, and one branch that was wrong

The link probe found `Eigen` referenced in **both** precisions with **no row in
either** - it is reached from the retail's own `GaussPointsFit2` and
`GaussPointsFit3` (`0x00A509A0` / `0x00A514D0`), which are the engine's
`ContBox2` / `ContBox3` path, and their fingerprint is nine calls to
`Eigen::operator()` to fill the covariance matrix.

The retail does have the class, in the **WM3.8 shape** - `Tridiagonal2`,
`Tridiagonal3`, `Tridiagonal4`, `TridiagonalN`, `QLAlgorithm`,
`IncreasingSort` and `DecreasingSort` - not the WM4 shape the rebuild was
translated from.  Every body is x87 in **both** precisions (float operands are
`dword`, double `qword`).

Five rows go in, all float:

| row | retail | in -> out (words) |
|---|---|---|
| `Eigen<float>::IncrSortEigenStuff3` | `0x00A70DB0` | `m[9]` -> `diag[3] mat[9]` |
| `Eigen<float>::IncrSortEigenStuff2` | `0x00A70D70` | `m[4]` -> `diag[2] mat[4]` |
| `Eigen<float>::GetEigenvalue` | `0x00A6CE70` | `m[9]` -> `ret[3]` |
| `Eigen<float>::GetEigenvector` | `0x00A6D350` | `m[9]` -> `vec[9]` |
| `Eigen<float>::GetEigenvector(Vector2)` | `0x00A6D3A0` | `m[4]` -> `vec[4]` |

Each row builds the object through the **retail's own constructor**, fills the
matrix through the **retail's own `operator()`**, runs the **retail's own
solver** and reads the results back through the **retail's own accessors**; the
retail's destructor runs each time, because it allocates on the retail heap.

### The one real defect - and it is on an engine path

`Tridiagonal3` tested the `(0,2)` element against `Math<Real>::ZERO_TOLERANCE`,
which is the **WM4 form**, while the retail tests it against **zero**: a
`FUCOM` of `fM02` with `0.0` whose *equal* case is the one that leaves (float
`0x00A6D480`, double `0x00A6EF70`; the WM3.4 copy has `fM02 != 0` as well).
The two paths are not close - one rotates the matrix, the other leaves it
alone - and a small non-zero element took the wrong one.

**The fix is in the shared template code, so it changes the float branch too**,
and the float branch is what the engine runs.  That makes this the first
change in this whole effort that can move engine behaviour: it is on the
`ContBox2` / `ContBox3` path, and the five float rows are the proof that it now
matches the retail.

Mutation, both directions:

| row | mutation | mismatches / 200000 |
|---|---|---|
| `IncrSortEigenStuff3` | `Tridiagonal3` back on `ZERO_TOLERANCE` | **23640** |
| `GetEigenvalue` | same | **12117** |
| `GetEigenvector` | same | **23744** |
| `IncrSortEigenStuff2` | `IncreasingSort` -> `DecreasingSort` | **142830** |
| `GetEigenvector(Vector2)` | same | **143236** |

restored, all five are 0/200000 and 1000000/1000000 on all five seeds.  The
two-by-two rows are untouched by the `Tridiagonal3` change (`Tridiagonal2` has
no such branch), so their mutation is the sort order - and it is isolated: the
other three rows still passed under it, and vice versa.

### Everything else in that body matched, and was left alone

The convergence test `|subd[i2]| + (|diag[i2]|+|diag[i2+1]|) ==
(|diag[i2]|+|diag[i2+1]|)`; the `fG` branch polarity (`fG < 0 -> fG-fR`,
else `fG+fR`); the `|fF| >= |fG|` test; the sort's strict `<` with the
`m_bIsRotation` toggle; `GuaranteeRotation`'s real `fchs` on the first column;
and the CRT sqrt at `0x00A91000`.

### What is left of Eigen

* **The double instantiation is not paired.**  The retail's double path is x87
  under `_PC_24`, so every intermediate is a 24-bit significand with the
  extended exponent, while the rebuild's double branch is still binary64 -
  that needs the `Wm3Detail::X87Asm` transcription of `Tridiagonal3`,
  `QLAlgorithm` and the sorts.  **Every address is known**: constructor
  `0x00A71DC0`, destructor `0x00A70ED0`, `operator()` `0x00A6EDC0`,
  `GetEigenvalue` `0x00A6CEA0`, `Tridiagonal3` `0x00A6EF70`, `QLAlgorithm`
  `0x00A6FF60`, `IncrSortEigenStuff3` `0x00A71460`.  The float side of the
  same class is the same algorithm, so what is left is the **precision model**,
  not the algorithm.
* `Tridiagonal4` / `TridiagonalN` / `DecrSort*` / `EigenStuff*` (float) are not
  needed: the engine's `Eigen` is only ever size 2 or 3, and the rebuild has no
  `Tridiagonal4` at all.
* `GetEigenvalues()` / `GetEigenvectors()` / `operator()` /
  `GetEigenvector(Vector4)`: `operator()` is exercised by every row; a
  dedicated row over the accessors would only re-read what the harness wrote.

## The `Eigen<double>` rows: the precision model

The float side of `Eigen` went in first; the double side is the half that
needs a **model** rather than a fix.  The retail's double body is x87 code and
the engine runs under `_PC_24`, so every product, every sum, every division and
the square root carry a **24-bit significand with the extended exponent**.  A
binary64 expression cannot reproduce that - it rounds to 53 bits and has no
extended exponent - so the rebuild's double branch carries its intermediates in
**`Wm3Detail::X87Asm`**, which runs the operations on the hardware's x87 the
way the retail does.

**The software model is not enough here.**  `Wm3Detail::X87` keeps a `double`
and rounds once per operation, but its operands are 53-bit doubles whose
products need 106 bits - so it rounds those twice (the binary64 product first,
then the 24-bit significand) and lands an ulp of the 24-bit significand away
from the retail.  That is the same distinction that made
`DistSegment3Segment3<double>` work, and it is now the rule for every double
body with a multiply in it.

**The square root is the retail's own.**  The CRT's routine at `0x00A91000`
re-sets the control word to `(CW & 0x300) | 0x7F` - it masks the *precision*
bits and keeps them - so under `_PC_24` (`0x007F`) the root is a **24-bit** one.
`Wm3Detail::X87Sqrt`, which takes the ambient control word, is that
computation; `Math<double>::Sqrt`, a correctly rounded binary64 root, is not.

Five rows go in:

| row | retail | in -> out (words) | mutation (revert to binary64) |
|---|---|---|---|
| `Eigen<double>::IncrSortEigenStuff3` | `0x00A71460` | `m[9]`(18) -> `diag[3] mat[9]`(24) | **114495 / 200000** |
| `Eigen<double>::IncrSortEigenStuff2` | `0x00A71420` | `m[4]`(8) -> `diag[2] mat[4]`(12) | **114434 / 200000** |
| `Eigen<double>::GetEigenvalue` | `0x00A6CEA0` | `m[9]` -> `ret[3]` | **114544 / 200000** |
| `Eigen<double>::GetEigenvector` | `0x00A6EE40` | `m[9]` -> `vec[9]` | **114290 / 200000** |
| `Eigen<double>::GetEigenvector(Vector2)` | `0x00A6EE90` | `m[4]` -> `vec[4]` | **114177 / 200000** |

Each row builds the object through the **retail's own constructor**
(`0x00A71DC0`), fills the matrix through the retail's own `operator()`
(`0x00A6EDC0`), runs the retail's own solver and reads the results back through
the retail's accessors; the destructor (`0x00A70ED0`) runs each time because the
object allocates on the retail heap.  Restored, all five are 0/200000 and
1,000,000/1,000,000 on all five seeds.

**The algorithm did not change.**  The double body is the float body's
algorithm - the WM3.4 copy's - with the same expression order and groupings, and
its `Tridiagonal3` branch is `fM02 != 0` too (its `FUCOMP` compares `0.0` with
`fM02` - the opposite operand order from the float body, the same meaning).
The float branch is untouched: the diff adds lines and removes none, and the
five float rows still pass 1,000,000/1,000,000.  **The change cannot reach the
engine** - it is inside `if constexpr (std::is_same_v<Real,double>)`.

Skipped, and why: `Tridiagonal4` (`0x00A6F120`) and `TridiagonalN`
(`0x00A6F7B0`) - the engine never builds an `Eigen` of size > 3
(`GaussPointsFit2/3` only) and the rebuild has no `Tridiagonal4` at all, so the
double `TridiagonalN` is still binary64 and the source says so.  The accessors
`GetEigenvalues()` / `GetEigenvectors()` / `GetEigenvector(int)`->`GVector` only
re-read what the solver wrote, and `operator()` is exercised by every row.

### A correction to an older note about the CRT square root

The `DistVector3Box3<double>` prose used to say the CRT's square root
"normalises the control word to `0x027F` before the `FSQRT` - so the square root
itself is a 53-bit significand".  The machine code at `0x00AA7BD5` computes
`(CW & 0x300) | 0x7F`, which under `_PC_24` (`0x007F`) is `0x007F`: **the root
is a 24-bit one**.  The rebuild's code and the rows were right; only the prose
was loose, and it is corrected here.

## The double hull: the addresses are known, and the retail leaks

The link probe found `ConvexHull<double>`, `ConvexHull2<double>` and
`ConvexHull3<double>` referenced by the engine, and the float hull rows were in
while the double side was empty.  **This round produced the addresses, the
conventions and the stride - and found a retail-side blocker, so no row went
into the table.**

### The addresses

| member | address | convention |
|---|---|---|
| `ConvexHull<double>::ConvexHull(int,Real,bool,Query::Type)` | `0x00A6BC90` | thiscall, 4 args, `RET 0x14`; installs vtable `0x00D7208C` |
| `ConvexHull<double>::~ConvexHull()` | `0x00A6BCD0` | thiscall, no args |
| `GetDimension()` | `0x00A6BD30` | thiscall, int in EAX |
| `GetSimplexQuantity()` | `0x00A6BD40` | thiscall, int in EAX |
| `GetIndices()` | `0x00A6BD50` | thiscall, pointer in EAX |
| `ConvexHull2<double>::ConvexHull2` | `0x00A79220` | thiscall, **5** args, `RET 0x18`; vtable `0x00D7212C` |
| `~ConvexHull2<double>()` | `0x00A74BA0` | thiscall |
| `ConvexHull3<double>::ConvexHull3` | `0x00A67840` | thiscall, same 5 args, `RET 0x18`; vtable `0x00D72060` |
| `~ConvexHull3<double>()` | `0x00A66500` | thiscall (the deleting dtor is `0x00A66630`) |
| `GetPlaneOrigin()` | `0x00A51F60` | thiscall, pointer in EAX (`lea eax,[ecx+0x70]`) |
| `GetPlaneDirection(int)` | `0x00A51F70` | thiscall, one int, `RET 4`, pointer in EAX (`[ecx + i*24 + 0x88]`) |

The five arguments are the vertex quantity, the vertex array, the **epsilon (a
double, so eight bytes)**, `bOwner` and the query type - the order is the
engine's own call at `0x00A400A8`, which pushes them, calls `0x00A67840` and
then reads `GetDimension` / `GetSimplexQuantity` / `GetIndices` at
`0x00A400C8` / `0x00A400D6` / `0x00A400E8`.  The three accessors are the
**base's own non-virtual bodies**, so one row witnesses the engine's
`ConvexHull<double>` and `ConvexHull2<double>` symbols alike.

### The vertex-array stride trap was real

The retail's `Mapper2<double>` (`0x00A773F0`) reads the vertices **sixteen**
bytes apart and its `Mapper3<double>` (`0x00A58EB0`) **twenty-four**;
`ConvexHull3<double>` sizes the array as the count times `0x18` and
`ConvexHull2<double>` as the count times `0x10`, and the engine's own loop uses
the same twenty-four (`lea eax,[eax+eax*2]; fld qword [esi+eax*8]`).  The
harness hands the constructors an array of doubles, two or three per vertex and
contiguous - not the float layout.

### Why no row: the retail leaks

The `ConvexHull2<double>` row built from the above **agreed with the retail bit
for bit - 200000/200000 at seed `0x5EED2007`, with no source change** - but the
retail's `ConvexHull2<double>` allocates roughly **three kilobytes per hull that
nothing gives back**, and the run dies of a `bad_alloc` (a C++ exception,
`0xE06D7363`) after some **350000 constructions, on every seed**.  The leak is
not the hull's destructor: `ConvexHull2<double>`'s (`0x00A74BA0`) and the
float's (`0x00A74970`) are structurally identical and neither destroys a hull
container, so it is inside one of the helpers the double instantiation
allocates (the `Mapper2` / `Query2Int64` double path).  **A row cannot be run at
the closing gate on a body that leaks.**

`ConvexHull3<double>` has a second problem: its **plane data does not agree bit
for bit**, because the retail's `Mapper3` computes it on the x87 (24-bit
significands) and the rebuild's in binary64 - the same finding as
`Eigen<double>`, and it would need the `Wm3Detail::X87Asm` transcription of the
`Mapper3` double branch.

### The mutation check, for what it is worth

The `ConvexHull2<double>` row needed no source change, so its mutation is a
deliberate perturbation of the rebuild - `m_iDimension = 2;` to `= 1;` in
`Foundation/Containment/Wm3ConvexHull2.cpp`: **109203/200000** mismatches, and
the same mutation on the float row gives **127560/200000**, so the row does have
discriminating power.  The mutation is in the shared template so it reaches both
precisions; that is recorded in the row's note.

## The `Vector3<float>::Length` row: the retail has two length bodies

The reachability audit left `Vector3` / `Vector2` as the last gap: sixteen float
classes are `executed` and the harness covered six of `Vector3`'s members.  This
round pairs the seventh.

| row | retail | in -> out (words) | answer |
|---|---|---|---|
| `Vector3<float>::Length` | `0x00452A90` | `v[3]` -> `ret` | a **value** |

The address sits between `operator-` (`0x00452A50`) and `Dot` (`0x00452AD0`) in
the retail's WM3.8 `Vector3.cpp` block, and its body is three squares plus a call
to `SqrtfBinary` (`0x00452FC0`).  It is thiscall with **no stack arguments** and
a plain `RET`, the length coming back in **ST(0)**.

### The defect: two retail bodies, and the rebuild folded one into the other

`0x00452A90` (**`Length`**) computes the three squares with **SSE**
(`MOVSS`/`MULSS`) and calls the CRT root.  `0x004757E0` (**`SquaredLength`**, and
the static `LengthSq`, already a row) is **x87**.  The two agree in the float's
normal range and disagree where the squares are **denormal**, because the SSE
squares behave as binary32 while the x87 keeps the extended exponent.

The rebuild's `Vector3<Real>::Length()` was `Math<Real>::Sqrt(SquaredLength())` -
the x87 body - so it was wrong for exactly the vectors the generator produces:
**529 of 200000** draws at seed `0x5EED2007`.  The first mismatch reported:
`v = (1.33045e-22, 1.05414e-22, 2.79115e-23)`, squares ~1.8e-44 (a float
denormal): retail `1.75581e-22`, rebuild `1.71544e-22`.

**The fix is in the shared template, so it reaches the engine** -
`src/sdk/moho/ai/CAiSteeringImpl.cpp:198` (`velocity.Length() * 10.0f`) and
`:1458`.  The float instantiation now computes the squares as float multiplies,
sums them `((x*x + y*y) + z*z)` - the retail's SSE grouping - and takes the
**24-bit** root through `Wm3Detail::X87Sqrt`.  The double instantiation is
untouched.  The static `Vector3f::Length(v)`, a rebuild addition with no retail
counterpart, was given the same value, because the retail's engine code calls the
*instance* method and the rebuild's static one stands in for it.

Mutation: the fix reverted, the row fails **529/200000**; restored, it passes
200000/200000 and 1,000,000/1,000,000 on all five seeds.

### What of the priority list could not be paired, and why

**Most of it has no retail counterpart at all.**

| member | why there is no row |
|---|---|
| `Zero()`, `NaN()`, `IsntNaN()`, `IsInvalid()`, `Compare()`, `DistanceSq3D()`, `DistanceSq()`, `Sub()`, `PerpDot()` | **rebuild-side additions** - `Foundation/Math/Wm3Vector3.h:462` says so, and the WM3.4 copy has neither the names nor the bodies.  A row would compare the rebuild against itself. |
| `ZERO` | a **static data object** in the retail (`0x00F3D21C`), not a body; a row would compare `(0,0,0)` with `(0,0,0)` and could not fail under any mutation. |
| `Vector2<float>::Normalize()` (14 engine calls) and `Vector2<float>::Length()` (2) | **no out-of-line retail body**: the retail's WM3.8 `Vector2.cpp` has only `operator-` (`0x00452C50`) and the *tail* of `Normalize` (`0x00452C80`, which takes the length and the vector as arguments - the shape an inliner leaves behind), and the engine's code inlines the rest.  `0x00452C80` has no caller at all. |

So the "90 symbols" of `Vector3` are mostly **one inlined body used in ninety
places**, not ninety bodies - which is worth knowing before anyone reads that
number as a work estimate.

## Which instantiations the engine actually uses - measured, not grepped

The harness had been treating the retail's `double` instantiations as a work
list.  **Most of them are dead weight for this project**: the engine
(`src/`) is float-only in practice, and a `grep` for a class name does not
settle it - the name appears in headers, in comments, and in the library's own
explicit instantiations.

The measurement that does settle it: **withdraw every explicit instantiation
of the instantiation under test and see whether `main.vcxproj` still links.**
If it links, nothing in `src/` references it; if it fails, the unresolved
symbols name exactly what is referenced and from which object.

### Double: 4 of 140 are used

With all **140** `template WM3_ITEM class X<double>;` pairs commented out, the
link failed with **9 unresolved symbols**, all of them from four classes:

| class | the symbols the engine references |
|---|---|
| `Eigen<double>` | `GetDimension`, `GetEigenvalue`, `GetEigenvector`, `IncrSortEigenStuff2`, `IncrSortEigenStuff3` |
| `ConvexHull<double>` | `GetDimension`, `GetSimplexQuantity`, `GetIndices` |
| `ConvexHull2<double>` | `GetDimension`, `GetSimplexQuantity`, `GetIndices` |
| `ConvexHull3<double>` | `GetPlaneOrigin`, `GetPlaneDirection` |

**The other 136 double instantiations are not referenced by the engine at
all.** That includes every `Intr*<double>` and every `Dist*<double>` the
harness spent this whole effort on - `IntrBox3Sphere3<double>` was the first
one checked by hand, and it is representative.

### Float: 20 of 140 are used

The same probe on the `float` instantiations fails the link with **152
unresolved symbols** spread over **20 classes**:

`Vector3` (90 references), `Box3` (34), **`Eigen` (32)**, `Sphere3` (26),
`DistVector3Segment3` (16), `Segment3` (16), `Box2` (16),
`IntrBox3Sphere3` (16), `ConvexHull3` (16), `IntrSegment3Box3` (12),
`IntrSegment3Sphere3` (12), `ConvexHull` (12), `Vector2` (12),
`DistVector3Box3` (8), `IntrBox2Circle2` (8), `Circle2` (8),
`IntrLine3Box3` (8), `ConvexHull2` (8), `IntrBox2Box2` (4), `Line3` (4).

So the float side is a **fifth of the library**, not the whole of it - and it
is the side that matters, because it is what the engine runs.

### What this changes about the work

* **`Eigen` is used in BOTH instantiations and the harness has no row for
  either.**  It is 32 float references and five double symbols - the largest
  single gap the measurement found, and it is an *iterative* numerical
  algorithm, exactly the kind whose operation order matters.
* **`ConvexHull` / `ConvexHull2` / `ConvexHull3` are used in both
  instantiations**; the double side has no rows (`ConvexHull3<double>` also
  needs the 24-byte vertex-array stride that the retail's inline `Query3`
  object uses).
* The classes the engine does **not** use are marked **out of scope for
  pairing**, not "not yet done".  Rows already in the table for them stay -
  they are correct and cost nothing - but no further work goes into them, and
  they are listed below rather than in the work list.

### The out-of-scope list (engine does not reference them)

* **Double, 136 of 140**: every `Intr*<double>`, every `Dist*<double>`
  (including `DistSegment3Segment3<double>`, `DistVector3Segment3<double>`,
  `DistLine3Segment3<double>`, `DistVector2Box2<double>`,
  `DistVector3Box3<double>`), `Vector2<double>`/`Vector3<double>` on their
  own, `Box2`/`Box3`/`Circle2`/`Line3`/`Segment3`/`Sphere3<double>` as
  classes, all of `Containment` except the three hulls, all of
  `Approximation`, all of `RationalArithmetic`.
* **Float, 120 of 140**: the same families minus the 20 listed above.

**Already-paired rows for out-of-scope classes are kept**: they are verified
bit-for-bit and they cost nothing to run, and several of them are what
established the x87 method in the first place.  They are simply no longer a
reason to do more.

## The double instantiation's coverage map

How much of the retail's `double` side is actually compared, measured against
the survey's inventory (`.tmp/survey/SURVEY_TABLE.md`, the RTTI rows and the
vtable slot addresses read out of the image):

* **25** retail classes have a `double` instantiation;
* they have **153** `double` virtual slots;
* **50** of those slots are covered by a row today - the four `Get` /
  `GetSquared` slots of `DistVector2Box2<double>`, `DistVector3Box3<double>`,
  `DistVector3Segment3<double>` and `DistLine3Segment3<double>`, the `Test`
  slot of all seven `Intr*` classes, and the four base bodies (window `Get`,
  window `GetSquared`, `GetDerivative`, `GetDerivativeSquared`) which appear
  in every derived class's slot list;
* so **103** slots are not covered, which is **94 distinct bodies** (the
  base classes' slots are shared by several derived classes).

That is the honest size of the remaining work, and it is worth writing down
because the list below is the work list:

| family | classes | distinct bodies | state |
|---|---|---|---|
| `Dist*<double>` | 5 | 5 classes' own slots plus the 8 shared base bodies | 4 classes done, 1 at the last 1% |
| the base's window queries and derivatives | - | 8 | **in** |
| `Intr*<double>` | 8 | 32 | all seven `Test` slots in; the rest compare values |
| `Query2/3<double>`, `Query2/3TRational<double>` | 4 | 32 | not started |
| `ConvexHull<double>`, `ConvexHull1/2/3<double>` | 4 | 4 | not started |
| `Query2/3Int64<double>`, `Query2/3TInteger<double>` | 4 | 32 | **blocked** - the rebuild has no `ToTetrahedron` and no index overloads |

The `Intr*<double>` family is the part that had gone missing: the eight
intersection classes (`IntrBox2Box2`, `IntrBox2Circle2`, `IntrBox3Sphere3`,
`IntrLine3Box3`, `IntrLine3Capsule3`, `IntrSegment3Box3`,
`IntrSegment3Capsule3`, `IntrSegment3Sphere3`) have a `double` instantiation
in the retail and **not one `double` row**.  Their float instantiations are
already compared, so their layouts and generators exist - but each double
body has to be checked the way `DistSegment3Segment3<double>` was: the
retail's float body and its double body are not the same formulation, and
only the double body is the reference.

The shared base bodies, listed once because several classes use them:

| body | used by |
|---|---|
| `0x00A39860` (+0x04) | the four `Dist*<double>` classes' window `Get` (Vector3d) |
| `0x00A39AB0` (+0x10) | the same, window `GetSquared` (Vector3d) |
| `0x00A39D00` (+0x1C) | the same, `GetDerivative` (Vector3d) |
| `0x00A39D60` (+0x20) | the same, `GetDerivativeSquared` (Vector3d) |
| `0x00A39270` / `0x00A394C0` / `0x00A39710` / `0x00A39770` | the same four slots, Vector2d |
| `0x00A455A0` / `0x00A455D0` | the `Intersector` base, shared by `IntrBox2Box2` and `IntrBox2Circle2` |
| `0x00A45630` / `0x00A45650` | the `Intersector` base, shared by five `Intr*` classes |

## The first `Intr*<double>` row is in, and it needs no source change

`IntrBox2Circle2<double>::Test` is in the table and agrees with the retail
bit for bit.  It is the first of the eight `Intr*<double>` classes, and it
needed **no change in the rebuild at all** - but the reason is narrower than
this section first said, and the correction is recorded below: the `Intr*`
double bodies **are** x87 code, and this row is free because the class
delegates its distance to a `Distance` class that is already modelled.

The address comes out of the class vtable `0x00D71EDC`, read out of the image
(the constructor at `0x00A4EE20` writes the pointer):

| slot | address | body |
|---|---|---|
| +0x00 | `0x00A4F520` | the destructor |
| +0x04 | `0x00A455A0` | the `Intersector` base's body |
| **+0x08** | **`0x00A4EE50`** | **the Test - the row** |
| +0x0C | `0x00A4F960` | the Find |
| +0x10 | `0x00A455D0` | the `Intersector` base's body |

The Test is thiscall, ECX = `this`, no stack arguments, the bool in EAX.
The constructor takes the box and the circle pointers on the stack and stores
them at `[this+0x18]` and `[this+0x1C]`; `RET 8`.  The row's layout is
`box[8] circle[3]` - eight doubles and three, twenty-two words - and the
output is one word, the bool.

The rebuild's double branch already agrees: the class's `Test` is
`DistVector2Box2<double>(m_rkCircle.Center,m_rkBox).Get() <=
m_rkCircle.Radius`, and the modelled-x87 `DistVector2Box2<double>` branch
carries it.  The float branch is untouched and nothing in the engine changed,
so the lockstep baseline stands.

The numbers: the row is 1000000/1000000 bit-identical on the default seed
`0x5EED2007` and 200000/200000 on the whole table, which is 114/114.  It is
mutation-checked: inverting the rebuild's answer in the harness's double
forward makes it fail on 2000 of 2000 draws.

## The `Intr*<double>` bodies are x87 code, but a `Test` row cannot see it

`IntrSegment3Capsule3<double>::Test` is in the table and agrees with the
retail bit for bit - 1000000/1000000 on the default seed `0x5EED2007` and
200000/200000 on the whole table, which is 115/115.  The table holds 115
rows and no duplicates (checked against the harness's own `--list`).

The address comes out of the class vtable `0x00D720AC`, read out of the image
(the constructor at `0x00A6BF50` writes the pointer):

| slot | address | body |
|---|---|---|
| +0x00 | `0x00A6C050` | the destructor |
| +0x04 | `0x00A45630` | the `Intersector` base's body |
| **+0x08** | **`0x00A6BF80`** | **the Test - the row** |
| +0x0C | `0x00A45650` | the `Intersector` base's body |
| +0x10 | `0x00A6C3C0` | |
| +0x14 | `0x00E5B970` | |
| +0x18 | `0x00A6C6B0` | |
| +0x1C | `0x00A38630` | |
| +0x20 | `0x00A6C730` | |
| +0x24 | `0x00A6C680` | |

The Test is thiscall, ECX = `this`, no stack arguments, the bool in EAX.
The constructor takes the segment and the capsule pointers on the stack and
stores them at `[this+0x18]` and `[this+0x1C]`; `RET 8`.  The row's layout
is `segment[7] capsule[8]` - seven doubles and eight, thirty words - and the
output is one word, the bool.  It is mutation-checked: inverting the
rebuild's answer in the harness's double forward makes it fail on 2000 of
2000 draws.

**A correction to the last round's conclusion, which was wrong.**  It said
that the intersection bodies are plain binary64 in both instantiations and
so the `Intr*<double>` classes would need no source change.  That is not
true in general.  The bodies are **x87 code** in the double instantiation -
the retail's `IntrBox2Box2<double>::Test` at `0x00A47F00` is a chain of
`fld`/`fmul`/`faddp`/`fabs` on 64-bit memory operands, and so is
`IntrBox3Sphere3<double>::Test` at `0x00A425C0`.  So the double
instantiations need the modelled x87 arithmetic, exactly like the `Distance`
ones.

**What decides whether a class's row passes.**  Two things, and the first
of them was established the hard way in the next section:

* whether the arithmetic lives in the class or is delegated to a `Distance`
  class (the delegated ones ride on the modelled x87 branch there), and
* **what the row compares.**  A `Test` row compares a **`bool`**, and a
  `bool` cannot see the difference between the x87 under `_PC_24` and a
  plain binary64 expression: the two answers differ only when the value
  lands within an ulp of the threshold, which is rare enough that a million
  draws do not find it.  A `Get`/`GetSquared` row compares a **value**, and
  such a row does see it - which is why the `Distance<double>` rows need the
  modelled x87 arithmetic and the `Test` rows do not.

So the five classes whose `Test` does its own arithmetic were failing for a
reason of the harness's own, described below - and once that was fixed they
passed at every seed that had been run.  They have since been shown to need
the transcription anyway, one draw in a million; see the note above.

The addresses of the constructors and of the Test bodies, read out of the
images (the constructor is the function that writes the class's vtable
pointer; the Test is the class's own body that loads its two shapes and
computes the distance):

| class | vtable | ctor | Test (+0x08) |
|---|---|---|---|
| `IntrBox2Box2` | `0x00D71E54` | `0x00A47C40` | `0x00A47F00` |
| `IntrBox3Sphere3` | `0x00D71D14` | `0x00A42590` | `0x00A425C0` |
| `IntrLine3Box3` | `0x00D71F0C` | `0x00A50060` | `0x00A50090` |
| `IntrSegment3Box3` | `0x00D71DF4` | `0x00A463D0` | `0x00A46720` |
| `IntrSegment3Capsule3` | `0x00D720AC` | `0x00A6BF50` | `0x00A6BF80` |
| `IntrSegment3Sphere3` | `0x00D71E24` | `0x00A46A50` | `0x00A475E0` |

Two notes on how to read a vtable, both of which cost time here.  The float
and the double vtables of a class are **not** slot-aligned - for
`IntrBox2Circle2` the float's Test is at its +0x0C and the double's at its
+0x08 - so a slot number means a different slot in the other instantiation.
And the `Intersector` base bodies are shared by several classes:
`0x00A455A0` and `0x00A455D0` by the two 2D classes, `0x00A45630` and
`0x00A45650` by the 3D ones, so they are to be done once and not per class.

## The seven `Intr*<double>::Test` rows are in, and the cause was a dirty EAX

`IntrBox2Box2`, `IntrBox3Sphere3`, `IntrLine3Box3`, `IntrSegment3Box3` and
`IntrSegment3Sphere3` now have their `double::Test` rows.  With the two rows
from the earlier rounds, all seven `Intr*<double>::Test` rows agree with the
retail bit for bit - 1000000/1000000 each on the default seed `0x5EED2007`,
and 200000/200000 for the whole table, which holds **120 rows** and no
duplicate row (checked against the harness's own `--list`).

**But not all of them hold at a million on every seed.**  On seed
`0xA5A5A5A5` at 1,000,000, `IntrBox2Box2<double>::Test` (`0x00A47F00`) fails
**once**: the retail answers true and the rebuild answers false, on a draw
whose words are extreme in magnitude (`E0000000`, `078E86F3`, ...).  That is
the row seeing the precision after all - the 24-bit result and the binary64
result land on opposite sides of the threshold about once in a million - so
**the `Intr*<double>::Test` bodies do need the x87 transcription**, and the
seven Test rows are open items, not finished ones.  The first sentence of
the next paragraph therefore needs its "no source change at all" read as "no
source change was needed *to pass at the seeds that were run*"; the honest
statement is that a `bool` row is a **weak witness, not a blind one**.

**The cause of the mismatches was the harness, not the rebuild.**  The
retail's `Test` returns its `bool` in **AL only** - `mov al,1` at the end of
`IntrBox3Sphere3<double>::Test` - so the upper 24 bits of EAX are whatever
they happened to be.  The thunks returned EAX as it was, and the row compared
the whole word: **the answer was the same and the word was not.**  That is why
the counts looked so different from one class to the next - 2000/2000 for
`IntrBox2Box2`, `IntrLine3Box3` and `IntrSegment3Box3`, 432/2000 for
`IntrSegment3Sphere3`, 3/2000 for `IntrBox3Sphere3` - all of them the same
single defect, and the spread only reflecting how often the junk in the upper
bits happened to be zero.

The fix is the float thunk's own: **`movzx eax, al`** after the call, which
`IntrBox3Sphere3Test` and the other float thunks have always done.  It is now
applied to the double thunks as well.  The out-store is verified: with the
retail side of one thunk forced to return 42, the row's retail word reads
`0x0000002A`.

**A second correction, to this file's own earlier reading.**  An earlier
round took the retail's bodies to be plain binary64 and concluded that the
`Intr*<double>` classes would need no source change; the round after that
found the bodies are x87 code and concluded they *would*.  Both readings were
half right.  The bodies **are** x87 code - `IntrBox2Box2<double>::Test` at
`0x00A47F00` is a chain of `fld`/`fmul`/`faddp`/`fabs` on 64-bit memory
operands - but **a `Test` row cannot tell**: it compares a `bool`, and at the
threshold the 24-bit and 53-bit answers agree unless the value is within an
ulp of it.  What settles the question is the *other* family: the
`Distance<double>` rows compare **values**, and they pass **only** with the
24-bit modelling.  So the harness does run the retail under `_PC_24`
(`gControlWord = 0x007F` in `main.cpp`, applied by `SetFpEnv()` before each
side; the `0x027F` that appears in `main.cpp` belongs to the retail's *static
initialisers*, which run before any target).  A modelled x87 branch was tried
for `IntrBox3Sphere3` and dropped again once the `movzx` defect was found and
the row passed without it - **the Foundation is untouched by this round**.

The addresses come out of each class's own vtable, read out of the image;
the constructor is the function that writes the vtable pointer, and the
`Test` is the class's own body that loads its two shapes:

| class | vtable | ctor | Test (+0x08) |
|---|---|---|---|
| `IntrBox2Box2` | `0x00D71E54` | `0x00A47C40` | `0x00A47F00` |
| `IntrBox3Sphere3` | `0x00D71D14` | `0x00A42590` | `0x00A425C0` |
| `IntrLine3Box3` | `0x00D71F0C` | `0x00A50060` | `0x00A50090` |
| `IntrSegment3Box3` | `0x00D71DF4` | `0x00A463D0` | `0x00A46720` |
| `IntrSegment3Capsule3` | `0x00D720AC` | `0x00A6BF50` | `0x00A6BF80` |
| `IntrSegment3Sphere3` | `0x00D71E24` | `0x00A46A50` | `0x00A475E0` |
| `IntrBox2Circle2` | `0x00D71EDC` | `0x00A4EE20` | `0x00A4EE50` |

Every one of them is the class vtable's **+0x08** slot.  The `Test` is
thiscall, ECX = `this`, no stack arguments, the `bool` in AL.  The
constructors take the shapes on the stack - `IntrSegment3Box3`'s takes a third
one, the `solid` flag, and `RET 12` - and store them at `[this+0x18]` and
`[this+0x1C]`.

The rows' layouts, in words, a `double` counted as two:

| row | layout | words |
|---|---|---|
| `IntrBox2Box2::Test` | `box0[8] box1[8]` | 32 |
| `IntrBox3Sphere3::Test` | `box[15] sphere[4]` | 38 |
| `IntrLine3Box3::Test` | `line[6] box[15]` | 42 |
| `IntrSegment3Box3::Test` | `segment[7] box[15] solid` | 45 |
| `IntrSegment3Capsule3::Test` | `segment[7] capsule[8]` | 30 |
| `IntrSegment3Sphere3::Test` | `segment[7] sphere[4]` | 22 |

The output is one word, the `bool`, in every case; `kMaxWords` stays 56.

The generators draw the same shapes the float rows' generators do, with the
magnitudes inside `[1e-100,1e100]`; what that leaves out of reach is the x87's
extended exponent range itself.  Every row is mutation-checked: inverting the
rebuild's answer in each of the seven double forwards makes all seven rows
fail on 200 of 200 draws.

**The lessons, and there are two of them.**  A mismatch in a `bool` row is
evidence about the **word** the function returns before it is evidence about
the arithmetic - which is why `movzx eax, al` was the whole fix for the
mismatches that were 100% and 20% of the draws.  But a `bool` row is only a
**weak** witness of the arithmetic, not a blind one: the 24-bit and binary64
answers differ by about an ulp, so the bool flips only when the value sits
within that ulp of the threshold - once in a million, which is exactly what
seed `0xA5A5A5A5` found.  **A million draws on the default seed is not a
gate; a million draws on five seeds is.**  The five-seed sweep is what turned
this row from "in" to "open", and it is now the closing gate for the whole
table.

## Two findings for the rounds to come

**The float and the double vtables of a class are not slot-aligned.**  For
`IntrBox2Circle2` the float class's vtable is `0x00D71EC0` and the double
class's is `0x00D71EDC` - and the float's Test is at its +0x0C while the
double's Test at its **+0x08**.  The double class's base has four fewer
entries, so a slot number from one instantiation means a different slot in
the other.  Every address has to be read out of the class's own vtable.  The
list in `.tmp/wmdiff/double_slots_remaining.txt` is in slot order per class
(+0x00, +0x04, +0x08, +0x0C, +0x10, ...), and the third address of the
`IntrBox2Circle2` line is the Test - but that is a fact about that class, not
a rule.

**The harness can be run with a stale binary when the build fails.**  The
build script's error goes to its own log and its exit code, and the row then
measures whatever binary is in `wmdiff/build/`.  That happened here: a
missing cast in the segment class's double branch left the build failing
while the rows kept reporting the old binary's numbers, and a search over
twelve candidate formulations came back "no change" for every one of them
because nothing was ever rebuilt.  **Check the build's exit code and the
executable's timestamp before believing a row's numbers.**  The same blind
spot left twelve duplicate rows in the table - the search appended the same
four `DistLine3Segment3<double>` rows three more times, and the duplicates
shift the seeds of every row after them, because a row's seed is derived
from its index.  They are removed; the table is 114 rows.

## `DistSegment3Segment3<double>`: where the remaining 1% is

With the modelled x87 value in place the rows are 20 mismatches in 2000
draws - about one per cent - and the failing draws are spread over every
region (0, 1, 2, 3, 4, 5, 6, 7, 8 and the parallel branch), so the
difference is in a part common to all of them.  The common parts are the four
sums, fC, the determinant and fS0/fS1 - and a search over twelve variants of
their associations and term orders (fA01, fB0, fB1, fC, fS0, fS1, each with
its alternative association, and the z term moved to the front of each sum)
left the count at 20 in every case.  What is left is the inner sums of the
region formulas - `fS0 + fA01*fS1 + 2*fB0` and its siblings - so the next
step is to change the association of the inner sums in all nine regions at
once and see the count move.  Its rows are not in the table and nothing
ships for them: the double branch of that class is not in the tree.

### Still to do

1. The four `Intr*<double>` `Find` bodies, each with the gap localised by
   reading the retail's own body:
   * `IntrBox2Circle2` (`0x00A4F960`) - the retail's body computes `kDiff`,
     `kVel` and the four dots on the x87 and then runs the **nine-region
     dispatch itself**, calling `0x00A4EEF0` (`TestVertexRegion`) four times
     and `0x00A4F2C0` (`TestEdgeRegion`) four times, with the region arguments
     passed on the x87 stack (`SUB ESP,0x30` of doubles).  The rebuild's file
     has **one generic body for both instantiations** and no double branch at
     all; its region helpers are the plain C++ ones, which is what the retail's
     two region bodies are - so the gap is the **dispatch's four dots and the
     x87 argument marshalling**, not the helpers;
   * `IntrBox3Sphere3` (`0x00A44190`) - the face/edge/vertex dispatch is
     inline and calls `0x00A3AB80`, `0x00A42710`, `0x00A42790` (twice),
     `0x00A427F0` and `0x00A42BC0`; two of those use the CRT sqrt.  The four
     *region* helpers are already transcribed (they have rows), so the gap is
     the dispatch and the two sqrt-using helpers;
   * `IntrSegment3Capsule3` (`0x00A6C3C0`) - **done**, together with the static
     `IntrLine3Capsule3::Find` (`0x00A80080`) it calls; see the section above.
     (It is not a thin wrapper, as an earlier note said.)
   * `IntrSegment3Sphere3` dynamic (`0x00A47690`) - **done**; see the section
     above.  (It was not blocked after all: it needs `Normalize` and the
     capsule's static `Find`, both of which are in.)  Each needs its own reading and its
   callees: the circle's two region bodies `0x00A4EEF0` / `0x00A4F2C0` (with
   their own `sqrt` and `InvSqrt`); the box-sphere's five helpers
   `0x00A3AB80`, `0x00A42710`, `0x00A42790`, `0x00A427F0`, `0x00A42BC0`; and
   the capsule's double `IntrLine3Capsule3::Find` (`0x00A80080`) plus the
   double `Vector3<double>::GenerateOrthonormalBasis` (`0x00A3D150`).  The
   four shared `Intersector` base bodies are stubs and are **not** work.
2. The six `Intr*<double>::Test` bodies that still have a binary64 double
   branch - `IntrBox3Sphere3` (`0x00A425C0`), `IntrLine3Box3` (`0x00A50090`),
   `IntrSegment3Box3` (`0x00A46720`), `IntrSegment3Sphere3`
   (`0x00A475E0`), and `IntrSegment3Capsule3` (`0x00A6BF80`, blocked on
   `DistSegment3Segment3<double>`).  `IntrBox2Box2` and `IntrBox2Circle2` are
   done.  The method is the `IntrBox2Box2` one.
3. The rest of the `Intr*<double>` slots - all seven `Test` slots are in,
   so what is left is the other members: the `Find` slots (`+0x0C`), the
   region / contact-point helpers of `IntrBox3Sphere3` and `IntrBox2Circle2`
   (`+0x10` onwards), and the four shared `Intersector` base bodies
   (`0x00A455A0`, `0x00A455D0`, `0x00A45630`, `0x00A45650`).  Those compare
   values where the `Test` rows compare a `bool`, so they *will* see the
   precision - and the rebuild's double branches for them are still plain
   binary64.
4. `Query2<double>` (`0x00D72158`) and `Query3<double>` (`0x00D71F5C`) - the
   retail builds the `Query3` object inline, and the double instantiation's
   vertex array has a stride of 24 bytes instead of 12 - and
   `Query2TRational<double>` (`0x00D72230`) and `Query3TRational<double>`
   (`0x00D72034`).
5. `ConvexHull<double>` (`0x00D7208C`, body `0x00A6BD90`),
   `ConvexHull1<double>` (`0x00D7211C`, `0x00A71F60`),
   `ConvexHull2<double>` (`0x00D7212C`, `0x00A74DF0`) and
   `ConvexHull3<double>` (`0x00D72060`, `0x00A66630`) - each class has one
   virtual slot, so the construction is reached by address.
6. `Query2Int64<double>` (`0x00D721A0`), `Query2TInteger<double>`
   (`0x00D721E8`), `Query3Int64<double>` (`0x00D71FA4`) and
   `Query3TInteger<double>` (`0x00D71FEC`) - **blocked**: the rebuild has no
   `ToTetrahedron` and no index overloads, so there is nothing to call.

The base's own +0x08 / +0x0C / +0x14 / +0x18 defaults are **not** a zero
return: they are a pure-call stub at `0x00A82547` that aborts, so a sweep
that lands on the base's implementation is a crash, not a zero.

**The lockstep game, after the `DistVector2Box2<double>` double branch.**
The rebuilt library's contents changed again - the double branch of
`Wm3DistVector2Box2.cpp` now models the retail's x87 arithmetic - so the
engine binary moved and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m15s.  The engine is float-only, so
no double code is on any engine path; the suite is here to prove that
fixing it did not change anything that does.

**The lockstep game, after the `DistLine3Segment3<double>` double branch.**
The rebuilt library's contents changed again - the double branch of
`Wm3DistLine3Segment3.cpp` now models the retail's x87 arithmetic - so the
engine binary moved and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m16s.  The engine is float-only, so
no double code is on any engine path; the suite is here to prove that
fixing it did not change anything that does.

**The lockstep game, after the base's eight bodies and the `IntrBox2Box2`
transcription.**  The rebuilt library changed again - the double branches of
`Wm3Distance.cpp` (through the `DistanceAdd/Sub/Mul/Div` helpers) and of
`Wm3IntrBox2Box2.cpp` now model the retail's x87 arithmetic - so the engine
binary moved and the lockstep suite was run again: `2p-main-x86-vs-x86` ends
in **PASS** in 5m17s.  The engine is float-only, so no double code is on any
engine path; the suite is here to prove that fixing it did not change
anything that does.

**The lockstep game, after the four `Intr*<double>::Test` transcriptions.**
The rebuilt library changed again - the double branches of
`Wm3IntrBox3Sphere3.cpp`, `Wm3IntrLine3Box3.cpp`, `Wm3IntrSegment3Box3.cpp`
and `Wm3IntrSegment3Sphere3.cpp` now model the retail's x87 arithmetic - so
the engine binary moved and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m17s.  The engine is float-only and
every changed line sits inside a `if constexpr (std::is_same_v<Real,double>)`,
so no engine path is touched; the suite is here to prove it.

**The lockstep game, after the distance class's double branch and `X87Asm`.**
The rebuilt library changed again - `Wm3DistSegment3Segment3.cpp`'s double
instantiation and the new `Wm3Detail::X87Asm` in `Wm3Vector3.h` - so the
engine binary moved and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m25s.  The engine is float-only and
every changed line is inside the double instantiation, so no engine path is
touched; the suite is here to prove it.

**The lockstep game, after the three `Intr*<double>::Find` bodies.**  The
rebuilt library changed again - the double branches of `Wm3IntrLine3Box3.cpp`
(`DoClipping`, `Clip`) and `Wm3IntrSegment3Sphere3.cpp` (`Find`) - so the
engine binary moved and the lockstep suite was run again:
`2p-main-x86-vs-x86` ends in **PASS** in 5m15s.  The engine is float-only and
every changed line sits inside the double branch, so no engine path is
touched; the suite is here to prove it.

**The lockstep game, after the orthonormal basis rows.**  The rebuilt library
changed again - the double branch of `GenerateOrthonormalBasis` in
`Foundation/Math/Wm3Vector3.inl` - so the engine binary moved and the lockstep
suite was run again: `2p-main-x86-vs-x86` ends in **PASS** in 5m16s.  The
engine is float-only and the change is the `if constexpr` double half, so no
engine path is touched; the suite is here to prove it.

**The lockstep game, after `Vector3::Normalize`.**  The rebuilt library
changed again - the double branch of `Vector3<Real>::Normalize` in
`Foundation/Math/Wm3Vector3.inl` - so the engine binary moved and the lockstep
suite was run again: `2p-main-x86-vs-x86` ends in **PASS** in 5m16s.  The
engine is float-only, it never calls the double instantiation, and the float
body is byte-for-byte the old one; the suite is here to prove it.

**The lockstep game, after the capsule bodies.**  The rebuilt library changed
again - the new double branches of `Wm3IntrLine3Capsule3.cpp` (the static
`Find`) and `Wm3IntrSegment3Capsule3.cpp` (`Find`) - so the engine binary moved
and the lockstep suite was run again: `2p-main-x86-vs-x86` ends in **PASS** in
5m29s.  The engine is float-only and the changes are the `if constexpr` double
halves, so no engine path is touched; the suite is here to prove it.

**The lockstep game, after the `Tridiagonal3` branch fix.**  This one is not
like the earlier lockstep runs in this file: the fix is in the shared template
code of `Eigen`, so it changes the **float** branch, and the float branch is
what the engine runs - it sits on the `ContBox2` / `ContBox3` path.  The suite
was run against the rebuilt engine and `2p-main-x86-vs-x86` ends in **PASS** in
5m21s: the two games still agree frame for frame after the change, which is
what a branch that decides "rotate the matrix or leave it" has to be checked
for.

The earlier lockstep entries in this file exist to show that a double-only
change touched nothing the engine runs.  This entry is the opposite case, and
it is the reason the suite is worth running even when the harness is green.

**The lockstep game, after the `Length` fix.**  Like the `Tridiagonal3` entry
above, this change is **engine-visible**: `Vector3<Real>::Length()` is shared
template code and the engine calls it (`CAiSteeringImpl.cpp:198` and `:1458`),
so the float instantiation the games run has changed.  The suite was run against
the rebuilt engine and `2p-main-x86-vs-x86` ends in **PASS** in 5m19s - the two
games still agree frame for frame with the SSE-shaped `Length` in place.

This is the second entry of its kind.  The many earlier ones exist to show that
a double-only change touched nothing the engine runs; these two exist to show
that the changes which *do* reach the engine keep the lockstep intact, which is
the only end-to-end check this project has.

## `Wm3::Vector3<float>::CompareArrays` - the retail's `memcmp`, inlined

| | |
|---|---|
| retail address | **`0x004F0A50`** |
| how it was found | the engine's own annotations name it: `src/sdk/moho/entity/intel/CIntel.cpp:62`, `src/sdk/moho/render/camera/VTransform.cpp:178`, `src/sdk/moho/ai/CAiBuilderImpl.cpp:585`, `src/sdk/moho/entity/UserEntity.cpp:356` |
| convention | thiscall, `ECX = this`, one stack argument (the other vector's address), `RET 4`, the answer an **int in EAX** |
| layout | in `a[3] b[3]` (6 words), out `ret` (1 word) |
| body | **integer only** - no x87, no float arithmetic |

**What the body is.**  The compiler's **inlined `memcmp`** over the twelve bytes
of the two vectors: three dword compares and a byte-wise tail, answering
**-1, 0 or +1**.  So the comparison is **bitwise**, over the lanes in the
object's little-endian order (x, then y, then z), and the sign says which lane
differed first.  A component-wise `==` would disagree twice over: on `+0.0f`
against `-0.0f`, and on a NaN against the same NaN.

**A correction, because the first version of this entry got it backwards.**
The entry originally said the retail's inlined memcmp answers -1/0/+1 "and not
the byte difference the CRT's memcmp returns".  That last clause is **wrong**,
and it was checked the only way it can be: `memcmp` on `{0x00}` against `{0x40}`
returns **-1** under both `msvcrt.dll` and `ucrtbase.dll`, and a probe compiled
with this project's own toolchain at `/O2 /Oi /MD` returns -1 / +1 / -1 for
`00|40`, `40|00` and `02|00`.  MSVC's `memcmp` is sign-normalised, so the retail
and the rebuild agree on the return value by construction.

What the row therefore settles is **not the normalisation but what precedes
it**: the bitwise comparison and the lane order.  That is still the thing worth
settling - a component-wise comparison gets `+0.0f` against `-0.0f` and a NaN
against itself wrong - and the mutation shows the row can tell: it fails 72% of
the draws.  But a reader who took the old sentence at face value would have
believed the row witnessed something it does not.

**The row compares the `int`, not a `bool`.**  A `bool` row could not tell the
lane order from the sign; this one witnesses both, and that is why it is not
the weak witness most comparison rows are.

**The generator is built for the cases a float comparison gets wrong**, and it
builds them from **bit patterns** (the generator's own arithmetic runs under
`_PC_24`): the two vectors bit-identical with a NaN in a lane, `+0.0f` against
`-0.0f` in one lane, one lane one ulp apart, two NaNs with different payloads
(last byte differing by `0xFF`), a lane whose first byte differs by `0x40`, and
a random bit flip.

**Mutation, both directions:** the member's body put back on a component-wise
comparison fails **145229 / 200000**; restored, it passes 200000/200000 and
1,000,000/1,000,000, and the whole table passes 1,000,000/1,000,000 on all five
seeds.  It fails on 73% of the draws and on families no float comparison could
agree with - a real witness.

### The one source change is an access specifier

In the WM3.8 retail the member is **public** - the retail's own engine calls it
by name - while the WM3.4 copy has it **private**
(`.tmp/wm3/Source/Math/Wm3Vector3.h:103-105`), and the rebuild had copied that.
`Foundation/Math/Wm3Vector3.h` now has it public, with the body untouched.

**Note what this does *not* buy.**  The rebuild's own engine never calls the
member: `src/` names it only in comments (`VTransform.h:111`).  So the public
specifier is not needed to compile the engine - it is needed by the **harness**,
which links against the same header and calls the member directly.  Reconstruct
the retail's access because the retail has it, not because the engine needs it.

**The proof that this cannot move the engine is the `.text` hash, not the
absence of a diff**: `main.exe`'s `.text` section hashes to
`ed96fca73b7b5be547c2342fe5b02ccc` before and after the rebuild.  The binary was
relinked - its md5 and its PE timestamp moved - but not one byte of code
changed.

## `Wm3::IVector2<int>::operator<` has no retail counterpart

The retail **has the class**: its RTTI carries `.?AV?$IVector2@H@Wm3@@` and
`std::less<IVector2<int>>`.  What it does not have is an **out-of-line body** -
the retail's engine **inlines the comparison away**.  The build-structure map's
lower-bound walk at `0x005808D0` *is* the comparison:

```
005808F0  mov esi, [ebx + ecx]   ; the key's component
005808F3  mov edi, [ecx]         ; the node's component
005808F5  cmp esi, edi
005808F7  jl  0x58091d           ; less    -> the left subtree
005808F9  jg  0x580906           ; greater -> the right subtree
005808FB  add edx, 1 / add ecx, 4 / cmp edx, 2 / jl loop   ; on a tie, the next lane
```

x first, then y, signed - the same order as the rebuild's member, but there is
no body to point a row at.  A search of the whole retail image for an
out-of-line two-component integer comparison finds only the CRT's `memcmp` at
`0x00A9185E`; nothing of that shape exists anywhere in the library region.  And
the WM3.4 copy has no `IVector2` at all - the class arrived after 3.4.

So the census entry is **`不适用` / no retail counterpart**, with that evidence
in its note.  A row over the rebuild's own copy would compare the rebuild with
itself, which is the one thing this harness must never do.

## The retail's Wild Magic 3.8 block at `0x004F0800`, and the link probe's blind spot

The round that paired `Vector3<float>::CompareArrays` walked this block for one
address.  Walking **all** of it answers a larger question, so here it is:

| address | what it is | convention | callers |
|---|---|---|---|
| `0x004F08B0` | `Vector3<float>::operator==` - calls `0x004F0A50` and folds the answer into a bool (`neg eax; sbb eax,eax; add eax,1`) | thiscall: `mov eax,[esp+4]`, `RET 4` | 0 |
| `0x004F08E0` | `Quaternion<float>::operator==` - the same shape around `0x004F0B40` | register: `ECX` = `this`, `EAX` = the other; plain `ret` | 0 |
| `0x004F0A50` | `Vector3<float>::CompareArrays` - the inlined memcmp over **12** bytes | thiscall: `mov eax,[esp+4]`, `RET 4` | 95 |
| `0x004F0AE0` | a `Vector3` copy helper (three `fld`/`fstp` in a 0x28-byte frame) | - | - |
| `0x004F0B40` | `Quaternion<float>::CompareArrays` - the inlined memcmp over **16** bytes | register: `ECX` = `this`, **`EAX`** = the other; plain `ret` | 24 |

**The two conventions are not the same, and that is not a detail.**  A row over
`0x004F08B0` or `0x004F0A50` uses the ordinary thiscall thunk - the argument is
pushed and the body pops it.  A row over either quaternion body must set **ECX
and EAX** and push nothing, because the body never reads the stack.  `0` callers
means the body is a compiler-emitted copy that every use site inlined past; it is
still the member's code, which is the same situation as the `Vector3::Length`
row.

**There is no `Quaternion<float>::Rotate` here, and none anywhere else.**  The
member is header-inline in the retail exactly as in the WM3.4 copy, and the
Release build inlines it at every call site.  Two independent checks: the block
above is all there is between the `operator==` wrappers, and **no body in the
whole Wild Magic region `0x00A00000`-`0x00A90000` has as many as six
multiplies** - a rotation matrix plus a 3x3 product needs eighteen.  The retail's
engine rotates with **its own** `QuatToMatrix` at `0x00452FD0` (SSE - it opens
`83 ec 0c / f3 0f 10 41 04 / f3 0f 10 61 08 / f3 0f 10 71 0c`), which is what
`src/sdk/moho/entity/CollisionBeamEntity.cpp:61` names.

**The blind spot.**  The reachability census had marked `Quaternion<float>` *not
applicable* because withdrawing the library's explicit instantiation still let
`main.vcxproj` link.  That test cannot see a header-inline member: the engine's
own translation units instantiate what they use, so the link succeeds whether
the class is used or not.  The class is in fact used in the engine's live paths -
`Identity()` (`CAimManipulator.cpp:1076`), the lane accessors
(`CUnitMotion.cpp:813-816`), `operator==` (`CAiFormationInstance.cpp:3585`) and
`Rotate` (`CollisionBeamEntity.cpp:71-73`).  So the probe may only cut
candidates; it can never close one.

Three members of this block are now **rows**, and they are what proved the
register-passing thunk:

| member | retail | why it is in scope | mutation |
|---|---|---|---|
| `Wm3::Vector3<float>::operator==` | `0x004F08B0` | `if (!(aimTarget == Wm3::Vector3f::ZERO))` at `CBuilderArmManipulator.cpp:596` | **49845 / 200000** |
| `Wm3::Quaternion<float>::CompareArrays` | `0x004F0B40` | reached through `operator==` at `CAiFormationInstance.cpp:3585` | **140499 / 200000** |
| `Wm3::Quaternion<float>::operator==` | `0x004F08E0` | `!(orientation == prevOrientation)` at `CAnimationManipulator.cpp:1330` | **49752 / 200000** |

**The quaternion thunks push nothing.**  The bodies read their argument from
**EAX**, so the thunk sets `ECX` = `this`, `EAX` = the argument, and calls
through **`EDX`** - `call eax` would destroy the argument.  A pushed argument
would change the stack the body returns to.  The `Vector3` thunk is the ordinary
shape: push, and the body's `RET 4` pops it.

Two things the mutations taught, both now in `AGENTS.md`.  A component-wise
`Quaternion::CompareArrays` fails 70% of the draws because the retail answers
`-1/0/+1` and the mutation answers `1` - the *magnitude* is visible, which is
what makes an `int` row stronger than a `bool` one.  And a `bool` row only
discriminates if the generator aims at the comparison's semantics: the `±0`
family needs the other lanes **identical**, because with random lanes on both
sides a component-wise `==` says "not equal" anyway.  That one change took the
`Vector3::operator==` mutation count from 20109 to 49845.

Making `Quaternion<float>::CompareArrays` public was the one source change, and
it is code-neutral the same way the `Vector3` one was: the Release engine's
`.text` hashes to `ed96fca73b7b5be547c2342fe5b02ccc` before and after.

## Constructor rows: one works, the rest cannot

A constructor has no return value - it writes the object through a hidden return
pointer - so a constructor row compares the object's **bytes**.  Usually that
means comparing addresses and bytes nobody initialised.  Three pilots, one per
failure mode - and one of them was rejected on a wrong premise, which is why the
section ends with the rule rather than with the verdict:

| member | retail | why an entry cannot work |
|---|---|---|
| `DistVector3Box3<float>::DistVector3Box3` | `0x00A45730` | the object holds **references** (`m_rkVector`, `m_rkBox`), stored at `+0x34`/`+0x38` - the bytes are the caller's addresses, different on the two sides by construction.  The base `Distance` ctor at `0x00A39F20` also leaves `m_fContactTime`, `m_kClosestPoint0/1` and the `m_bHasMultipleClosestPoints` flags **unwritten** |
| `IntrBox2Box2<float>::IntrBox2Box2` | none | same two references, and **no out-of-line body**: the retail's `func_UnitsWillCollide` (`0x00596930`) contains no constructor call - it is inlined |
| `Sphere3<float>::Sphere3(const Vector3&, float)` | **`0x004740C0`** | the layout is clean and **a body does exist** - `ECX` = the object, the `Vector3*` at `[esp+4]`, the radius at `[esp+8]`, `RET 8`, writing x/y/z through x87 `fld`/`fstp` and the radius with `movss` to `+0xC`.  The pilot said "no body" because the engine's own construction at `0x005D38CA`-`0x005D3921` is inlined - but whether a body exists somewhere else is a different question, and the pilot answered the wrong one |

**The rule a constructor row has to satisfy here**: the object must hold no
references or pointers, the constructor must write every byte that is compared,
and the retail must have an out-of-line body.  `Sphere3` is the only member of
the queue that satisfies all three - so it gets a row, and the other seven
reference-holding constructors are `executed-not-pairable`.

The `ContAlignedBox` entry, which caught a field the rebuild never wrote (the
box's axis), is a **function** row over a box returned by value - a different
shape.  That is why the lesson is not "constructors are useless" but "check the
object's bytes before building one".

**The convention is not the source's, either.**  `DistVector3Box3<float>`'s
constructor takes **ECX as the hidden return pointer** - the object itself - its
two arguments on the stack, and returns with `RET 8`.  The same file's other
members take ECX as `this` and one stack argument.  Read the epilogue and the
first load before writing a thunk.

## `Wm3::Sphere3<float>::Sphere3(const Wm3::Vector3<float>&, float)` - the one constructor row

Retail **`0x004740C0`**.  The convention is the hidden-return-pointer one:
**ECX is the object the constructor writes**, the centre's address is at
`[esp+4]`, the radius at `[esp+8]`, and the body ends in `RET 8`.  It writes all
sixteen bytes - the three centre lanes through the x87 (`fld`/`fstp`), the radius
with a plain `movss` to `+0xC` - so every compared byte is a value: no padding,
no references, nothing left unwritten.  That is the whole reason this one works
and the other constructors do not.

Two things the generator has to do, both learned the hard way:

* **Quiet every signalling NaN it draws.**  The retail copies the centre through
  x87, which quiets a signalling NaN; the rebuild's plain copy does not.  That is
  the harness's known divergence class for pure copies, not a property of the
  rebuild, and leaving it in produced a `[nan-payload]` note on the first
  sixty-five draws.  With the sNaNs quieted the row passes even under
  `--strict-nan`.
* **Push the centre last.**  cdecl pushes right to left, so the last argument
  pushed lands at the *lowest* address; getting it backwards crashes inside the
  retail at its `fld [ecx]`, and the harness reports a crash rather than a
  mismatch.

The mutation swaps the centre's x and y lanes in the rebuild's constructor - the
same instruction sequence with different bytes, which is exactly what a
constructor row needs - and fails **199875 / 200000**.

### Why the two `Intersector` constructors still get no row

The bodies exist: `Intersector<float, Vector2>::Intersector()` at `0x00A454E0`
and the Vector3 one at `0x00A45570`, both thiscall, `this` in ECX, no stack
arguments, plain `ret`.  Each writes three words: the **vtable pointer** at `+0`
(a link-time constant, different in the retail and the rebuild), `m_fContactTime
= 0.0` at `+4` and `m_iIntersectionType = IT_EMPTY` at `+8`.

The harness compares exactly `outWords` words, and an entry narrows that by
writing only the words it wants - so the vtable pointer **is** skippable.  But
what is left is `0.0f` and `0` on both sides of every draw.  **Skipping the noise
is not the same as having something left to compare**, and an entry that can
never fail is not an entry.  Both are `executed-not-pairable`, with that
reasoning in the census note.
