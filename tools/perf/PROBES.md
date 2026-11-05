# UIGC instrumentation for the performance runs

The engine-side probes were removed before the engine fixes were committed.
This directory is kept so the probes can be restored for one measurement run
and then removed again.

## What was removed

- `src/lua/LuaIncrementalGc.h` / `.cpp`: `TraceEnabled()`, `TimingEnabled()`,
  the `SState` cycle-accounting fields, the `[UIGC-STARTUP]` log.
- `src/lua/LuaIncrementalGcCore.inc`: the `IncrDiagCycle*` helpers and their
  calls from `IncrStep`, `IncrSlice`, `IncrFullGC`, plus `[UIGC-ROLE]`.
- `src/lua/LuaObject.cpp`: the `luaC_collectgarbage` timing wrapper and the
  `[UIGC-T] mode=full` / `mode=step` lines.
- `src/moho/ui/CUIManager.cpp`: the `[UIGC-P]` frame sample and the
  `[UIGC-T] mode=slice` / `mode=full path=idle|direct` timing lines.

## Restoring a probe build

`git show <commit>:<path>` for the commit just before the cleanup, or re-apply
the patch kept beside this file. The probes are gated by:

- `FAF_UIGC_TRACE=1` — VM role and start-up value
- `FAF_UIGC_TIMING=1` — frame, full, step, slice and cycle timing

`tools/perf/run_perf.py` sets both variables itself.

## Rules

- Never measure a build that has both the probes and a changed collector path.
- Report warm-up and steady state separately; the analyzer does this for you.
- Remove the probes again before the engine change is proposed for merge.
