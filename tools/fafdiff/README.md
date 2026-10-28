# fafdiff

`fafdiff` is a **second differential harness**: it compares the main engine
rebuild's source (`src/sdk/...`) against the retail
`C:\ProgramData\FAForever\bin\ForgedAlliance_6c6c92f.exe`, bit for bit.

It is modelled on `dependencies/WildMagic3p8/wmdiff/`, which does the same
thing for the Wild Magic 3.8 library rebuild. The shared core
(`src/fuzz.h`, `src/gen.cpp`, `src/pe_image.*`) is a verbatim copy of wmdiff's.

## How it differs from `tools/difffuzz`

`tools/difffuzz` is the earlier harness and is **upstream** (read-only for us).
Its design differs in one decisive respect: the engine sources it could not
include directly were **mirrored by hand** into `src/sdk_mirror.cpp`, so it
measured a copy rather than the engine.

`fafdiff` takes the other route: the translation units under test are
**compiled into the harness from `src/sdk`**, with the engine's own switches
(the arrangement `tools/difffuzz/build.bat` already uses for three
translation units). Nothing is mirrored by hand, so what is measured is the
engine's own code.

Beyond that, fafdiff is planned to add dimensions difffuzz does not have:

* **systematic per-TU coverage** — one translation unit at a time, in a ledger,
  rather than "whatever was recovered so far";
* **object state** — not only return values and out-parameters but the bytes of
  the objects the functions leave behind;
* **call sequences** — the same inputs driven through a sequence of retail
  calls, not a single call per row;
* **a reachability ledger** — a recorded answer to "does the engine actually
  execute this?", so work is not spent on functions the engine never runs.

## Build

```bash
# from the repository root
MSYS_NO_PATHCONV=1 ./tools/fafdiff/build.bat all      # release + debug
MSYS_NO_PATHCONV=1 ./tools/fafdiff/build.bat release
MSYS_NO_PATHCONV=1 ./tools/fafdiff/build.bat debug
```

Output: `tools/fafdiff/build/fafdiff_release.exe` and
`tools/fafdiff/build/fafdiff_debug.exe`.

The build never touches `src/sdk/main.vcxproj` or any `.props`; see
`AGENTS.md` for the compile-parity rule and the flag audit.

## Run

```bash
./tools/fafdiff/build/fafdiff_release.exe --list                 # the table
./tools/fafdiff/build/fafdiff_release.exe --iters 1000000 --seed 0x5EED2007
./tools/fafdiff/build/fafdiff_release.exe --iters 1000000 --filter "SomeName"
```

Options: `--iters N`, `--seed S`, `--filter NAME` (comma-separated substrings),
`--max-mismatch N`, `--image PATH`, `--cw 0xNNNN`, `--snan`, `--strict-nan`,
`--skip-extreme`, `--list`.

The default retail image is
`C:/ProgramData/FAForever/bin/ForgedAlliance_6c6c92f.exe`.

## The target row

A row is one paired function:

| field | meaning |
|---|---|
| `name` | short name, used by `--filter` |
| `va` | the retail entry point (0 until it is filled in) |
| `source` | where our side lives (`file:line`) |
| `inWords` / `outWords` | how many 32-bit words the generator writes and the comparison reads |
| `inLayout` / `outLayout` | human-readable description of those words |
| `gen` | fills the input words from the row's RNG |
| `retail` | calls the retail function through the mapped image |
| `ours` | calls the engine's own function |

The table lives in `src/targets.cpp`; the aggregator is `fafdiff::Targets(int&)`
declared in `src/targets.h`.

## State

**The table is empty until P1.** There are no targets yet: `--list` prints the
header and no rows, and a run reports `0/0 targets passed`. P1 adds the first
translation unit under test and the first rows.

## Coverage ledger

`COVERAGE.md` is the per-function coverage ledger for the x87-tier math
surface: the **195** functions of `src/sdk/moho/math/**` and
`src/sdk/legacy/math/**` that the retail image still executes with x87
arithmetic. Every row carries the function's source-level name, its retail
address, its source location and one of four statuses (`未匹配`, `已匹配`,
`失败`, `不适用`). Start there: it says, per function, whether the pair exists
yet, and -- for the rows marked `不适用` -- why it will never exist
(difffuzz already covers that function, so fafdiff would add no dimension).

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

The generator fails loudly (and writes nothing) if the set no longer holds
195 functions, if a status is missing, invalid or lacks its `why` sentence, or
if a status entry points at an address outside the set -- the denominator of
the ledger must not drift silently. `scripts/gen_coverage.py --init-status`
writes a fresh status file (everything `未匹配`, difffuzz-covered rows
`不适用`).

