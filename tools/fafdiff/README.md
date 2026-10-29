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
* **a reachability ledger** — a recorded answer to "does the engine actually
  execute this?", so work is not spent on functions the engine never runs.

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

**The table has one row.** The first translation unit under test is
`src/moho/math/Vector3f.cpp`, compiled into the harness with the engine's own
switches (`src/legacy/math/X87Math.cpp` comes with it: it is the dependency
that holds the `msvc8::` x87 helpers `Vector3f.cpp` calls).

```bash
./build/clang-x86-release/tools/fafdiff/fafdiff.exe --list
# name               retail VA  our source                     typed layout
# NormalizeVectorInto 0x0044F7E0 src/moho/math/Vector3f.cpp:25  in[12B] out[16B]
```

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
