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

**The table is empty until the first translation unit under test lands.**
`--list` prints the header and no rows, and a run reports `0/0 targets passed`.

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
